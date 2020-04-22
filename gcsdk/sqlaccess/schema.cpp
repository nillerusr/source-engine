//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================


#include "stdafx.h"
//#include "sqlaccess/sqlaccess.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{
#ifndef STEAM
bool isspace( char ch )
{
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

int Q_strnlen( const char *str, int count )
{
	// can't make more meaningful checks, because this routine is used itself 
	// to check the NUL-terminatedness of strings
	if ( !str || count < 0 )
		return -1;

	for ( const char *pch = str; pch < str + count; pch++ )
	{
		if ( *pch == '\0' )
			return pch - str;
	}

	return -1;
}

#endif

//-----------------------------------------------------------------------------
// Purpose: convert an ESchemaCatalog into a string for diagnostics and logging.
//		this can't be in enum_names because of data type dependencies.
//-----------------------------------------------------------------------------
const char* PchNameFromESchemaCatalog( ESchemaCatalog e )
{
	switch (e)
	{
	case k_ESchemaCatalogInvalid:
		return "k_ESchemaCatalogInvalid";
		break;

	case k_ESchemaCatalogMain:
		return "k_ESchemaCatalogMain";
		break;
	}

	AssertMsg1( false, "unknown ESchemaCatalog (%d)", e );
	return "Unknown";
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSchema::CSchema()
{
	m_iTable = -1;
	m_rgchName[0] = 0;
	m_cubRecord = 0;
	m_bTestTable = false;
	m_cRecordMax = 0;
	m_bHasVarFields = false;
	m_nHasPrimaryKey = k_EPrimaryKeyTypeNone;
	m_iPKIndex = -1;
	m_pRecordInfo = NULL;
	m_wipePolicy = k_EWipePolicyPreserveAlways;
	m_bAllowWipeInProd = false;
	m_bPrepopulatedTable = false;
	m_nFullTextIndexCatalog = -1;
	m_eSchemaCatalog = k_ESchemaCatalogInvalid;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSchema::~CSchema()
{
	SAFE_RELEASE( m_pRecordInfo );
}


//-----------------------------------------------------------------------------
// Purpose: Calculates offset of each field within a record structure, and the 
//			maximum length of a record structure.
//-----------------------------------------------------------------------------
void CSchema::CalcOffsets()
{
	int dubOffsetCur = 0;

	for ( int iField = 0; iField < m_VecField.Count(); iField++ )
	{
		m_VecField[iField].m_dubOffset = dubOffsetCur;
		dubOffsetCur += m_VecField[iField].m_cubLength;

		if ( m_VecField[iField].BIsVariableLength() )
			m_bHasVarFields = true;
	}

	m_cubRecord = dubOffsetCur;

	if ( m_bHasVarFields )
		m_cubRecord += sizeof( VarFieldBlockInfo_t );
}


//-----------------------------------------------------------------------------
// Purpose: called to make final calculations when all fields/indexes/etc have 
//			been added and the schema is ready to be used
//-----------------------------------------------------------------------------
void CSchema::PrepareForUse()
{
	// Create a record description (new form of schema information, for SQL) that corresponds to this schema object.
	// This contains essentially the information as the CSchema object, we keep both for the moment to bridge the DS and SQL worlds.
	Assert( !m_pRecordInfo );
	SAFE_RELEASE( m_pRecordInfo );
	m_pRecordInfo = CRecordInfo::Alloc();
	m_pRecordInfo->InitFromDSSchema( this );	
}




//-----------------------------------------------------------------------------
// Purpose: For a record that has variable-length fields, gets the info block from
//			the tail end
// Input  : pvRecord -			Record data
//-----------------------------------------------------------------------------
VarFieldBlockInfo_t* CSchema::PVarFieldBlockInfoFromRecord( const void *pvRecord ) const
{
	if ( !m_bHasVarFields )
		return NULL;

	uint8 *pubRecord = ( uint8* )pvRecord;
	return ( VarFieldBlockInfo_t * )( pubRecord + m_cubRecord ) - 1;
}


//-----------------------------------------------------------------------------
// Purpose: Return the total size of the variable-length block for this record
//			For records that have no variable-length fields, it will return zero
// Input  : pvRecord -			Record data
//-----------------------------------------------------------------------------
uint32 CSchema::CubRecordVariable( const void *pvRecord ) const
{
	VarFieldBlockInfo_t *pVarFieldsBlockInfo = PVarFieldBlockInfoFromRecord( pvRecord );
	if ( pVarFieldsBlockInfo )
		return pVarFieldsBlockInfo->m_cubBlock;
	else
		return 0;
}



void CSchema::RenderField( uint8 *pubRecord, int iField, int cchBuffer, char *pchBuffer )
{
	Field_t *pField = &m_VecField[iField];

	uint8 *pubData;
	uint32 cubData;
	char chEmpty = 0;

    if ( pField->BIsVariableLength() )
	{
		if ( !BGetVarField( pubRecord, ( VarField_t * )( pubRecord + pField->m_dubOffset ), &pubData, &cubData ) )
		{
			// just render a single byte
			pubData = ( uint8* )&chEmpty;
			cubData = 1;
		}
	}
	else
	{
		pubData = pubRecord + pField->m_dubOffset;
		cubData = m_VecField[iField].m_cubLength;
	}

	ConvertFieldToText( pField->m_EType, pubData, cubData, pchBuffer, cchBuffer );
}

//-----------------------------------------------------------------------------
// Purpose: Renders a text version of a record to the console.
// Input  : pubRecord -		Location of the record data
//-----------------------------------------------------------------------------
void CSchema::RenderRecord( uint8 *pubRecord )
{
	char rgchT[k_cMedBuff];

	// First the header
	EmitInfo( SPEW_CONSOLE, 2, 2, "%d\t*** Record header: # of lines ***\n", 1 + m_VecField.Count() );

	// Render each field in turn
	for ( int iField = 0; iField < m_VecField.Count(); iField++ )
	{
		Field_t *pField = &m_VecField[iField];

		RenderField( pubRecord, iField, Q_ARRAYSIZE(rgchT), rgchT );

		EmitInfo( SPEW_CONSOLE, 2, 2, "\t%s\t\t// Field %d (%s)\n", rgchT, iField, pField->m_rgchName );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get data and size of a field, whether fixed or variable length
// Input:	pvRecord -			Fixed-length part of record
//			iField -			index of field to get
//			ppubField -			receives pointer to field data
//			pcubField -			receives size in bytes of that data
// Output:	true if successful
//-----------------------------------------------------------------------------
bool CSchema::BGetFieldData( const void *pvRecord, int iField, uint8 **ppubField, uint32 *pcubField ) const
{
	*ppubField = NULL;
	*pcubField = 0;
	const Field_t &field = m_VecField[iField];
	if ( field.BIsVariableLength() )
	{
		return BGetVarField( pvRecord, ( VarField_t * )( ( uint8 * ) pvRecord + field.m_dubOffset ), ppubField, pcubField );
	}
	else
	{
		*ppubField = ( ( uint8 * ) pvRecord + field.m_dubOffset );
		*pcubField = field.CubFieldUpdateSize();
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Set data and size of a field, whether fixed or variable length
// Input:	pvRecord -			Fixed-length part of record
//			iField -			index of field to set
//			pubField -			pointer to field data to copy from
//			cubField -			size in bytes of that data
// Output:	true if successful
//-----------------------------------------------------------------------------
bool CSchema::BSetFieldData( void *pvRecord, int iField, uint8 *pubField, uint32 cubField, bool *pbVarBlockRealloced )
{
	*pbVarBlockRealloced = false;

	Field_t &field = m_VecField[iField];

	if ( field.BIsVariableLength() )
	{
		return BSetVarField( pvRecord, ( VarField_t * )( ( uint8 * ) pvRecord + field.m_dubOffset ), pubField, cubField, pbVarBlockRealloced, /*bFreeOnRealloc*/ true );
	}
	else // fixed length
	{
		uint8 *pubFieldWrite = ( ( uint8 * ) pvRecord + field.m_dubOffset );

		// Must fit in field and last byte for strings MUST be NULL
		if ( cubField > field.m_cubLength ||
			( k_EGCSQLType_String == field.m_EType && Q_strnlen( reinterpret_cast< char* >( pubField ), cubField ) == -1 ) )
		{
			Assert( false );
			return false;
		}

		if ( k_EGCSQLType_Blob != field.m_EType && k_EGCSQLType_Image != field.m_EType )
		{
			// Copy the data (string or binary, doesn't matter)
			if ( cubField > 0 )
				Q_memcpy( pubFieldWrite, pubField, cubField );

			// Null termination and overwrite any old data
			if ( cubField < field.m_cubLength )
				Q_memset( pubFieldWrite + cubField, 0, field.m_cubLength - cubField );
		}
		else
		{
			// Only support fixed char or binary
			Assert( false );
			return false;
		}
	}

	return true;
}



//-----------------------------------------------------------------------------
// Purpose: Get data from a variable-length field
// Input:	pvRecord -			Fixed-length part of record
//			pVarField -			fixed part of field in that record
//			ppubField -			receives pointer to field data
//			pcubField -			receives size in bytes of that data
//-----------------------------------------------------------------------------
bool CSchema::BGetVarField( const void *pvRecord, const VarField_t *pVarField, uint8 **ppubField, uint32 *pcubField ) const
{
	Assert( m_bHasVarFields );

	*ppubField = 0;
	*pcubField = 0;

	VarFieldBlockInfo_t *pVarFieldBlockInfo = PVarFieldBlockInfoFromRecord( pvRecord );

	if ( !pVarField->m_cubField )
	{
		*pcubField = 0;
		*ppubField = NULL;
		return true;
	}

	if ( pVarFieldBlockInfo->m_cubBlock )
	{
		uint8 *pubVarBlock = pVarFieldBlockInfo->m_pubBlock;

		*ppubField = pubVarBlock + pVarField->m_dubOffset;
		*pcubField = pVarField->m_cubField;

		// Sanity check
		Assert( *pcubField <= k_cubVarFieldMax );

		return true;
	}
	else
	{
		// Should never happen
		Assert( false );
		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Update a variable-length field in a record (may realloc the var block)
// Input:	pvRecord -			Fixed-length part of record
//			pVarField -			fixed part of field in that record
//			pvData -			data to place in the field
//			cubData -			size of that data
//			pbRealloced -		set to indicate if the var block was realloced
//			bFreeOnRealloc -	If we have to grow or shrink the var block, should we free the old memory?
//								Usually true, unless it lives in a NetPacket or something like that
//-----------------------------------------------------------------------------
bool CSchema::BSetVarField( void *pvRecord, VarField_t *pVarField, const void *pvData, uint32 cubData, bool *pbRealloced, bool bFreeOnRealloc )
{
	VarFieldBlockInfo_t *pVarFieldBlockInfo = PVarFieldBlockInfoFromRecord( pvRecord );
	*pbRealloced = false;


	if ( cubData > k_cubVarFieldMax )
	{
		// field size is too big
		Assert( false );
		return false;
	}

	// if no block exists, allocate and copy into it
	if ( pVarFieldBlockInfo->m_cubBlock == 0 )
	{
		// Nothing to do?
		if ( !cubData )
			return true;

		// create it
		void *pvBlock = PvAlloc( cubData );
		*pbRealloced = true;
		
		// copy the data
		Q_memcpy( pvBlock, pvData, cubData );

		// set the record's structure to point at our new data
		pVarFieldBlockInfo->m_cubBlock = cubData;
		pVarFieldBlockInfo->m_pubBlock = ( uint8 * )pvBlock;

		// set the field to point at its landing place
		pVarField->m_cubField = cubData;
		pVarField->m_dubOffset = 0;
	}
	else
	{
		// there is some block available.

		// is this field changing size?
		if ( cubData != 0 && ( cubData == pVarField->m_cubField ) )
		{
			// no size change - no need to reallocate anything
			Q_memcpy( pVarFieldBlockInfo->m_pubBlock + pVarField->m_dubOffset, pvData, cubData );
		}
		else
		{
			// size change - realloc
			*pbRealloced = true;
			uint32 cubFieldOld = pVarField->m_cubField;
			uint32 dubOffsetOld = pVarField->m_dubOffset;
			uint32 cubBlockNew = pVarFieldBlockInfo->m_cubBlock - pVarField->m_cubField + cubData;

			if ( ( cubBlockNew + m_cubRecord ) > k_cubRecordMax )
			{
				// total record size is too big
				Assert( false );
				return false;
			}

			void *pvBlockNew = NULL;

			if ( cubBlockNew )
			{
				// if this field has never been placed in the block (that is, it's not changing an old value)
				// and we have enough space, fastest to put it at the end in the free space.
				if ( pVarField->m_cubField == 0 && pVarField->m_dubOffset == 0 &&  cubData <= pVarFieldBlockInfo->m_cubBlockFree )
				{
					uint8 *pubLastUsed = pVarFieldBlockInfo->m_pubBlock;
					for ( int iField = 0; iField < m_VecField.Count(); ++iField )
					{
						Field_t &field = m_VecField[iField];
						if ( field.BIsVariableLength() )
						{
							// Needs updating
							VarField_t *pVarFieldCur = ( VarField_t * )( ( uint8 * )pvRecord + field.m_dubOffset );
							pubLastUsed += pVarFieldCur->m_cubField;
						}
					}

					// copy it there
					Q_memcpy( pubLastUsed, pvData, cubData );

					// set up the field
					pVarField->m_cubField = cubData;
					pVarField->m_dubOffset = static_cast<int>( (pubLastUsed - pVarFieldBlockInfo->m_pubBlock) );

					// note that we used some up
					pVarFieldBlockInfo->m_cubBlockFree -= cubData;
				}
				else
				{
					// yes ... rellocate
					pvBlockNew = PvAlloc( cubBlockNew );

					uint8 *pubBlockOldCursor = pVarFieldBlockInfo->m_pubBlock;
					uint8 *pubBlockNewCursor = ( uint8* )pvBlockNew;

					// copy data, skipping over this field (will put at end)
					while ( pubBlockOldCursor < ( pVarFieldBlockInfo->m_pubBlock + pVarFieldBlockInfo->m_cubBlock ) )
					{
						if ( pVarField->m_cubField && ( (int)pVarField->m_dubOffset == ( pubBlockOldCursor - pVarFieldBlockInfo->m_pubBlock ) ) )
						{
							pubBlockOldCursor += pVarField->m_cubField;
						}
						else
						{
							*pubBlockNewCursor++ = *pubBlockOldCursor++;
						}
					}

					// put this field data at the end
					Q_memcpy( pubBlockNewCursor, pvData, cubData );

					// free the old block
					if ( bFreeOnRealloc )
						FreePv( pVarFieldBlockInfo->m_pubBlock );

					// Update the block info
					pVarFieldBlockInfo->m_cubBlock = cubBlockNew;
					pVarFieldBlockInfo->m_pubBlock = ( uint8 * )pvBlockNew;
					pVarFieldBlockInfo->m_cubBlockFree = 0;

					// update this field
					pVarField->m_cubField = cubData;
					if ( cubData > 0 )
						pVarField->m_dubOffset = static_cast<int>( pubBlockNewCursor - pVarFieldBlockInfo->m_pubBlock );
					else
						pVarField->m_dubOffset = 0;

					// update other fields
					for ( int iField = 0; iField < m_VecField.Count(); ++iField )
					{
						Field_t &field = m_VecField[iField];
						if ( field.BIsVariableLength() )
						{
							// Needs updating
							VarField_t *pVarFieldCur = ( VarField_t * )( ( uint8 * )pvRecord + field.m_dubOffset );

							// Except the one we just changed
							if ( pVarFieldCur == pVarField )
								continue;

							// And empty fields
							if ( !pVarFieldCur->m_cubField )
								continue;

							if ( pVarFieldCur->m_dubOffset > dubOffsetOld )
								pVarFieldCur->m_dubOffset -= cubFieldOld;
						}
					}
				}
			}
			else
			{
				// all of the variable data is gone, so
				// free the old block
				if ( bFreeOnRealloc )
					FreePv( pVarFieldBlockInfo->m_pubBlock );

				// ... update the block info
				pVarFieldBlockInfo->m_cubBlock = 0;
				pVarFieldBlockInfo->m_pubBlock = NULL;
				pVarFieldBlockInfo->m_cubBlockFree = 0;

				// ... and update this field
				pVarField->m_dubOffset = 0;
				pVarField->m_cubField = cubData;
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: If this is a variable-length record, and we just read it from
//			a stream, then the var block is after the fixed-length part of the
//			record. So, we need to update the record's pointer to reflect that
// Input:	pvRecord -			Beginning of the serialized record
//-----------------------------------------------------------------------------
void CSchema::FixupDeserializedRecord( void *pvRecord )
{
	// Nothing to do if not a variable record
	if ( !BHasVariableFields() )
		return;

	uint8 *pubRecord = ( uint8 * )pvRecord;
	VarFieldBlockInfo_t *pVarBlockInfo = PVarFieldBlockInfoFromRecord( pvRecord );
	pVarBlockInfo->m_pubBlock = pubRecord + m_cubRecord;
}


//-----------------------------------------------------------------------------
// Purpose: Initializes a new record with random data.
// Input:	pubRecord -			The record's in-memory data that we'll fill out
//			unPrimaryIndex -	Primary index of the record
//			pbRealloced -		Set to 'true' if the var-fields block for this record was reallocated
//								(or allocated for the first time)
//			bFreeOnRealloc -	If true, we should Free() the var block after reallocating a new block
//								(should be false if that block lives inside a NetPacket)
//-----------------------------------------------------------------------------
void CSchema::InitRecordRandom( uint8 *pubRecord, uint32 unPrimaryIndex, bool *pbVarBlockRealloced, bool bFreeVarBlockOnRealloc )
{
	// Fill out each field in turn
	for ( int iField = 0; iField < m_VecField.Count(); iField++ )
	{
		bool bRealloced = false;
		SetFieldRandom( pubRecord, iField, &bRealloced, bFreeVarBlockOnRealloc );
		if ( bRealloced )
		{
			*pbVarBlockRealloced = true;
			// if we just allocated it, we can free it next time we need to
			bFreeVarBlockOnRealloc = true;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets a single field of a record to a random value.
// Input:	pubRecord -		Where the record lives in memory
//			iField -		Which field to set
//			pbRealloced -		Set to 'true' if the var-fields block for this record was reallocated
//								(or allocated for the first time)
//			bFreeOnRealloc -	If true, we should Free() the var block after reallocating a new block
//								(should be false if that block lives inside a NetPacket)
//-----------------------------------------------------------------------------
void CSchema::SetFieldRandom( uint8 *pubRecord, int iField, bool *pbVarBlockRealloced, bool bFreeVarBlockOnRealloc )
{
	Field_t &field = m_VecField[iField];
	*pbVarBlockRealloced = false;

	// Strings get random text
	if ( k_EGCSQLType_String == field.m_EType )
	{
		// Generate up to cubLength - 1 chars
		uint32 cch = UNRandFast() % field.m_cubLength;
		uint32 ich = 0;
		for ( ; ich < cch; ich++ )
			*( ( char * ) pubRecord + field.m_dubOffset + ich ) = CHRandFast();

		// Null termination
		for ( ; ich < field.m_cubLength; ich++ )
			*( ( char * ) pubRecord + field.m_dubOffset + ich ) = 0;
	}
	else if ( field.BIsVariableLength() )
	{
		// Need temp buffer to randomize before setting
		// kept reasonably small to prevent spamming the console
		uint8 rgubBuff[512];
		uint32 cubData =  UNRandFast() % sizeof(rgubBuff);

		// For strings, put in (cubData-1) random characters (each randomly in the range [32,126])
		// then a trailing NULL
		if ( field.BIsStringType() )
		{
			uint32 ich = 0;
			for ( ; ich < (cubData-1); ich++ )
				rgubBuff[ich] = CHRandFast();

			rgubBuff[ich] = 0;
		}
		else
		{
			// binary - just fill in random bytes
			for ( uint32 iub = 0; iub < cubData; iub++ )
			{
				rgubBuff[iub] = ( uint8 )( UNRandFast() % 256 );
			}
		}

		// Set the variable field in the record
		BSetVarField( pubRecord, ( VarField_t * ) ( pubRecord + field.m_dubOffset ), rgubBuff, cubData, pbVarBlockRealloced, bFreeVarBlockOnRealloc );
	}
	// Binaries are filled with completely random bytes
	else
	{
		for ( uint32 iub = 0; iub < field.m_cubLength; iub++ )
		{
			*( pubRecord + field.m_dubOffset + iub ) = ( uint8 ) ( UNRandFast() % 256 );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns checksum of our contents
// Output : checksum
//-----------------------------------------------------------------------------
uint32 CSchema::CalcChecksum()
{
	CRC32_t crc32;
	CRC32_Init( &crc32 );

	FOR_EACH_VEC( m_VecField, nField )
	{
		CRC32_ProcessBuffer( &crc32, &m_VecField[nField], sizeof( m_VecField[nField] ) );
	}

	// keep checksum for entire record info
	CRC32_Final( &crc32 );

	return (uint32)crc32;
}


void CSchema::AddIntField( char *pchName, char *pchSQLName, EGCSQLType eType, int cubSize )
{
	int nExpectedSize = -1;
	switch ( eType )
	{
		case k_EGCSQLType_int8:
			nExpectedSize = 1;
			break;

		case k_EGCSQLType_int16:
			nExpectedSize = 2;
			break;

		case k_EGCSQLType_int32:
		case k_EGCSQLType_float:
			nExpectedSize = 4;
			break;

		case k_EGCSQLType_int64:
		case k_EGCSQLType_double:
			nExpectedSize = 8;
			break;
	}

	AssertMsg2( nExpectedSize != -1, "Unexpected EType in AddIntField: %d for column %s", eType, pchSQLName );
	AssertMsg3( nExpectedSize == cubSize, "Unexpected size for in AddIntField for column %s: %d doesn't match %d ", pchSQLName, nExpectedSize, cubSize );

	AddField( pchName, pchSQLName, eType, cubSize, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Adds a field from our intrinsic schema to this schema.
// Input:	pchName -			Name of the field
//			pchSQLName -		Name of the field in SQL database
//			eType -				Type of the field
//			cubSize -			Size of the field
//			pfnCompare -		Function used to compare fields (NULL if none specified)
//-----------------------------------------------------------------------------
void CSchema::AddField( char *pchName, char *pchSQLName, EGCSQLType eType, uint32 cubSize, int cchMaxLength )
{
	int iFieldNew = m_VecField.AddToTail();
	Field_t &field = m_VecField[iFieldNew];

	field.m_EType = eType;
	field.m_cubLength = cubSize;
	field.m_nColFlags = 0;
	Q_strncpy( field.m_rgchName, pchName, Q_ARRAYSIZE( field.m_rgchName ) );
	Q_strncpy( field.m_rgchSQLName, pchSQLName, Q_ARRAYSIZE( field.m_rgchSQLName ) );
	field.m_cchMaxLength = cchMaxLength;
}



//-----------------------------------------------------------------------------
// Purpose:	Marks a given field as the primary index
// Input:	
//		bClustered -		true to create a clustered index
//		pchName -			Name of the field to make primary index
//-----------------------------------------------------------------------------
int CSchema::PrimaryKey( bool bClustered, int nFillFactor, const char *pchName )
{
	int iField = FindIField( pchName );
	AssertFatalMsg2( k_iFieldNil != iField, "Could not find index column \"%s\" on table \"%s\"", pchName, m_rgchName );

	Assert( m_nHasPrimaryKey == k_EPrimaryKeyTypeNone );	// may not already have a primary key defined
	Assert( m_iPKIndex == k_iFieldNil );

	// must not have been indexed in any way before
	Assert( 0 == ( m_VecField[ iField ].m_nColFlags & ( k_nColFlagPrimaryKey | k_nColFlagIndexed | k_nColFlagUnique | k_nColFlagClustered ) ) );

	// note that we have a single-column PK
	m_nHasPrimaryKey = k_EPrimaryKeyTypeSingle;

	// set our flags
	m_VecField[ iField ].m_nColFlags |= ( k_nColFlagPrimaryKey | k_nColFlagIndexed | k_nColFlagUnique );
	if ( bClustered )
		m_VecField[ iField ].m_nColFlags |= k_nColFlagClustered;

	// add to list of primary keys and remember the indexID
	CUtlVector<int> vecColumns;
	vecColumns.AddToTail( iField );

	FieldSet_t vecIndex( true /* unique */ , bClustered, vecColumns, NULL );
	vecIndex.SetFillFactor( nFillFactor );
	m_iPKIndex =  m_VecIndexes.AddToTail( vecIndex );

	return m_iPKIndex;
}

//-----------------------------------------------------------------------------
// Purpose:	Marks a set of fields as the primary index
// Input:	
//			bClustered -		clustered index created if true
//			nFillFactor -		fill facto to use; 0 is database default
//			pchName -			Name of the field to make primary index
//-----------------------------------------------------------------------------
int CSchema::PrimaryKeys( bool bClustered, int nFillFactor, const char *pchNames )
{
	Assert( pchNames != NULL );								// no bogus parameters, please
	Assert( m_nHasPrimaryKey == k_EPrimaryKeyTypeNone );	// may not already have a primary key defined
	Assert( m_iPKIndex == k_iFieldNil );					// no primary key defined
	
	int nFlags = k_nColFlagPrimaryKey | k_nColFlagIndexed | k_nColFlagUnique;
	if ( bClustered )
		nFlags |= k_nColFlagClustered;

	// go add all those fields as Indexed
	int nNewIndex = AddIndexToFieldList( pchNames, NULL, nFlags, nFillFactor );
	if ( nNewIndex != k_iFieldNil )
	{
		m_nHasPrimaryKey = k_EPrimaryKeyTypeMulti;				// remember that we have multiple keys
		m_iPKIndex = nNewIndex;
	}
	
	return nNewIndex;
}

//-----------------------------------------------------------------------------
// Purpose:	Marks a set of fields as the primary index
// Input:	
//			pchName -			Names of the fields for the primary index; comma-separated if multiple
//			pchIndexName -		Name of the index object (not in SQL)
//			nFlags -			flags for CColumnInfo on this object
//			nFillFactor -		fill facto to use; 0 is database default
//-----------------------------------------------------------------------------
int CSchema::AddIndexToFieldList( const char *pchNames, const char *pchIndexName, int nFlags, int nFillFactor )
{
	// need a copy of string list since the argument is const and read-only
	int cNamesLen = Q_strlen( pchNames ) + 1;
	char* pchNamesCopy = (char*) PvAlloc( cNamesLen );
	Q_strncpy( pchNamesCopy, pchNames, cNamesLen );

	if (pchNames == NULL)
	{
		// not enough memory!
		AssertFatal( false );
		return k_iFieldNil;
	}

	CUtlVector<int> vecKeys;

	char* pchCurrent = pchNamesCopy;
	do 
	{
		// find next token
		char* pchEnd = strchr(pchCurrent, ',');
		if ( pchEnd != NULL )
		{
			*pchEnd++ = 0;
		}

		// skip whitespace
		while (isspace(*pchCurrent))
			pchCurrent++;

		// find the name; we expect a C++ name, not a SQL name
		int iField = FindIField( pchCurrent );
		AssertFatalMsg2( k_iFieldNil != iField, "Could not find index column \"%s\" on table \"%s\"", pchCurrent, m_rgchName );

		// mark as a primary key
		m_VecField[iField].m_nColFlags |= nFlags ;

		// add it to our collection
		vecKeys.AddToTail( iField );

		// move past the end of this token in the string
		pchCurrent = pchEnd;
	} while ( pchCurrent != NULL );

	// release our copy
	FreePv(pchNamesCopy);

	// create a fieldset with our list of indexes
	// and add it to our indexes collection
	bool bUnique = 0 != (nFlags & k_nColFlagUnique);
	bool bClustered = 0 != (nFlags & k_nColFlagClustered);
	FieldSet_t vecIndex( bUnique, bClustered, vecKeys, pchIndexName );
	vecIndex.SetFillFactor( nFillFactor );
	int iReturn = m_VecIndexes.AddToTail( vecIndex );
	return iReturn;
}


//-----------------------------------------------------------------------------
// Purpose:	Marks a given field as indexed.
// Input:	pchName -			Name of the field to index
//			pchIndexName -		Name of the index object (not in SQL)
//-----------------------------------------------------------------------------
int CSchema::IndexField( const char *pchIndexName, const char *pchName )
{
	Assert( pchName != NULL );
	Assert( pchIndexName != NULL );

	// find the field by name
	int iField = FindIField( pchName );
	AssertFatalMsg2( k_iFieldNil != iField, "Could not find index column \"%s\" on table \"%s\"", pchName, m_rgchName );

	// add an index to it
	return AddIndexToFieldNumber( iField, pchIndexName, false /* Not clustered */ );
}


//-----------------------------------------------------------------------------
// Purpose:	Marks a given field as indexed.
// Input:	pchIndexName -		Name of the index object (not in SQL)
//			pchName -			Name of the fields to index; comma separated if multiple
//-----------------------------------------------------------------------------
int CSchema::IndexFields( const char *pchIndexName, const char *pchNames )
{
	Assert( pchNames != NULL );
	Assert( pchIndexName != NULL );

	// go add all those fields as Indexed
	int nNewIndex = AddIndexToFieldList( pchNames, pchIndexName, k_nColFlagIndexed, 0 );
	return nNewIndex;
}


//-----------------------------------------------------------------------------
// Purpose:	Includes a column (or columns) in an index for the INCLUDE clause
// Input:	pchIndexName -		Name of the index object (not in SQL)
//			pchName -			Name of the fields to index; comma separated if multiple
//-----------------------------------------------------------------------------
void CSchema::AddIncludedFields( const char *pchIndexName, const char *pchNames )
{
	Assert( pchNames != NULL );
	Assert( pchIndexName != NULL );

	// find that index by name
	int nIndexIndex = -1;
	FOR_EACH_VEC( m_VecIndexes, i )
	{
		FieldSet_t &refSet = m_VecIndexes.Element(i);
		const char *pstrMatch = refSet.GetIndexName();
			if ( Q_stricmp( pstrMatch, pchIndexName ) == 0 )
		{
			nIndexIndex = i;
			break;
		}
	}

	// must have found it
	AssertFatalMsg1( nIndexIndex != -1, "Index \"%s\" not found", pchIndexName );
	FieldSet_t &refSet = m_VecIndexes.Element( nIndexIndex );

	// need a copy of string list since the argument is const and read-only
	int cNamesLen = Q_strlen( pchNames ) + 1;
	char* pchNamesCopy = (char*) PvAlloc( cNamesLen );
	Q_strncpy( pchNamesCopy, pchNames, cNamesLen );

	char* pchCurrent = pchNamesCopy;
	do 
	{
		// find next token
		char* pchEnd = strchr(pchCurrent, ',');
		if ( pchEnd != NULL )
		{
			*pchEnd++ = 0;
		}

		// skip whitespace
		while (isspace(*pchCurrent))
			pchCurrent++;

		// find the name; we expect a C++ name, not a SQL name
		int iField = FindIField( pchCurrent );
		AssertFatalMsg2( k_iFieldNil != iField, "Could not find index column \"%s\" on table \"%s\"", pchCurrent, m_rgchName );

		// add it to our collection
		refSet.AddIncludedColumn( iField );

		// move past the end of this token in the string
		pchCurrent = pchEnd;
	} while ( pchCurrent != NULL );

	// release our copy
	FreePv( pchNamesCopy );

	return;
}

//-----------------------------------------------------------------------------
// Purpose:	Marks a given field as indexed.
// Input:	pchIndexName -		Name of the index object (not in SQL)
//			pchName -			Name of the fields to index; comma separated if multiple
//-----------------------------------------------------------------------------
int CSchema::UniqueFields( const char *pchIndexName, const char *pchNames )
{
	Assert( pchNames != NULL );
	Assert( pchIndexName != NULL );

	// go add all those fields as Indexed
	int nNewIndex = AddIndexToFieldList( pchNames, pchIndexName, k_nColFlagIndexed | k_nColFlagUnique, 0 );
	return nNewIndex;
}


//-----------------------------------------------------------------------------
// Purpose:	Marks a given field a having a clustered index.
// Input:	pchName -			Name of the field to index
//			pchIndexName -		Name of the index object (not in SQL)
//-----------------------------------------------------------------------------
int CSchema::ClusteredIndexField( int nFillFactor, const char *pchIndexName, const char *pchName )
{
	Assert( pchName != NULL );
	Assert( pchIndexName != NULL );

	// can't previously have some other clustered index
	FOR_EACH_VEC( m_VecIndexes, iIndex )
	{
		if ( m_VecIndexes[iIndex].IsClustered() )
		{
			AssertFatalMsg3( false, "On table %s, can't make index %s clustered because %s is already the clustered index\n",
				m_rgchName, pchIndexName, m_VecIndexes[iIndex].GetIndexName() );
			return -1;
		}
	}

	// find the field by name
	int iField = FindIField( pchName );
	AssertFatalMsg2( k_iFieldNil != iField, "Could not find index column \"%s\" on table \"%s\"", pchName, m_rgchName );

	// add an index to it
	int iIndex = AddIndexToFieldNumber( iField, pchIndexName, true /* clustered */ );
	m_VecIndexes[iIndex].SetFillFactor( nFillFactor );
	return iIndex;
}


//-----------------------------------------------------------------------------
// Purpose:	Marks a  given set of fields as having a clustered index.
// Input:	pchIndexName -		Name of the index object (not in SQL)
//			pchName -			Name of the fields to index; comma separated if multiple
//-----------------------------------------------------------------------------
int CSchema::ClusteredIndexFields( int nFillFactor, const char *pchIndexName, const char *pchNames )
{
	Assert( pchNames != NULL );
	Assert( pchIndexName != NULL );

	// can't previously have some other clustered index
	FOR_EACH_VEC( m_VecIndexes, iIndex )
	{
		if ( m_VecIndexes[iIndex].IsClustered() )
		{
			AssertFatalMsg3( false, "On table %s, can't make index %s clustered because %s is already the clustered index\n",
				m_rgchName, pchIndexName, m_VecIndexes[iIndex].GetIndexName() );
			return -1;
		}
	}

	// go add all those fields as Indexed
	int nNewIndex = AddIndexToFieldList( pchNames, pchIndexName, k_nColFlagClustered | k_nColFlagIndexed, nFillFactor );
	return nNewIndex;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSchema::AddFullTextIndex( CSchemaFull *pSchemaFull, const char *pchCatalogName, const char *pchColumnName )
{
	Assert( pchCatalogName != NULL );
	Assert( pchColumnName != NULL );

	// need a copy of string list since the argument is const and read-only
	int cNamesLen = Q_strlen( pchColumnName ) + 1;
	char* pchNamesCopy = (char*) PvAlloc( cNamesLen );
	Q_strncpy( pchNamesCopy, pchColumnName, cNamesLen );

	if ( pchNamesCopy == NULL )
	{
		// not enough memory!
		AssertFatal( false );
		return;
	}

	char* pchCurrent = pchNamesCopy;
	do 
	{
		// find next token
		char* pchEnd = strchr(pchCurrent, ',');
		if ( pchEnd != NULL )
		{
			*pchEnd++ = 0;
		}

		// skip whitespace
		while (isspace(*pchCurrent))
			pchCurrent++;

		// find the name; we expect a C++ name, not a SQL name
		int iField = FindIField( pchCurrent );
		AssertFatalMsg2( k_iFieldNil != iField, "Could not find index column \"%s\" on table \"%s\"", pchCurrent, m_rgchName );

		// add it to our collection
		m_VecFullTextIndexes.AddToTail( iField );

		// move past the end of this token in the string
		pchCurrent = pchEnd;
	} while ( pchCurrent != NULL );

	// release our copy
	FreePv(pchNamesCopy);

	// make a note of the catalog we want
	int nCatalogID = pSchemaFull->GetFTSCatalogByName( m_eSchemaCatalog, pchCatalogName );
	AssertFatalMsg2( nCatalogID != -1, "Could not find fulltext catalog \"%s\" on table \"%s\"", pchCatalogName, m_rgchName );
	m_nFullTextIndexCatalog = nCatalogID;

	return;
}


//-----------------------------------------------------------------------------
// Purpose:	Marks a field as indexed given its field number
//-----------------------------------------------------------------------------
int CSchema::AddIndexToFieldNumber( int iField, const char *pchIndexName, bool bClustered )
{
	// mark the field as indexed
	m_VecField[iField].m_nColFlags |= k_nColFlagIndexed;

	// meant to be clustered?
	if ( bClustered )
		m_VecField[iField].m_nColFlags |= k_nColFlagClustered;

	// add to list of indexes, and remember the indexID
	CUtlVector<int> vecColumns;
	vecColumns.AddToTail( iField );

	// false: not unique
	// false: not clustered
	FieldSet_t vecIndex( false, bClustered, vecColumns, pchIndexName);
	int iIndex = m_VecIndexes.AddToTail( vecIndex );

	return iIndex;
}


//-----------------------------------------------------------------------------
// Purpose:	Marks a given field as unique.
// Input:	pchName -			Name of the field to index
//			pchIndexName -		Name of the index object
//-----------------------------------------------------------------------------
int CSchema::UniqueField( const char *pchIndexName, const char *pchName )
{
	Assert( pchName != NULL );
	Assert( pchIndexName != NULL );

	int iField = FindIField( pchName );
	AssertFatalMsg2( k_iFieldNil != iField, "Could not find index column \"%s\" on table \"%s\"", pchName, m_rgchName );

	m_VecField[iField].m_nColFlags |= ( k_nColFlagUnique | k_nColFlagIndexed );

	// add to list of indexes, and remember the indexID
	CUtlVector<int> vecColumns;
	vecColumns.AddToTail( iField );
	
	// true: unique
	// false: not clustered
	FieldSet_t vecIndex( true, false, vecColumns, pchIndexName );
	int iIndex = m_VecIndexes.AddToTail( vecIndex );

	return iIndex;
}


//-----------------------------------------------------------------------------
// Purpose:	Marks a given field as auto increment.
// Input:	pchName -			Name of the field
//-----------------------------------------------------------------------------
void CSchema::AutoIncrementField( char *pchName )
{
	int iField = FindIField( pchName );
	AssertFatalMsg2( k_iFieldNil != iField, "Could not find index column \"%s\" on table \"%s\"", pchName, m_rgchName );
	m_VecField[iField].m_nColFlags |= k_nColFlagAutoIncrement;
}


//-----------------------------------------------------------------------------
// Purpose: Finds the field with a given name.
// Input:	pchName -			Name of the field to search for
// Output:	Index of the matching field (k_iFieldNil if there isn't one)
//-----------------------------------------------------------------------------
int CSchema::FindIField( const char *pchName )
{
	for ( int iField = 0; iField < m_VecField.Count(); iField++ )
	{
		if ( 0 == Q_strncmp( pchName, m_VecField[iField].m_rgchName, k_cSQLObjectNameMax ) )
			return iField;
	}

	return k_iFieldNil;
}


//-----------------------------------------------------------------------------
// Purpose: Finds the field with a given SQL name.
// Input:	pchName -			Name of the field to search for
// Output:	Index of the matching field (k_iFieldNil if there isn't one)
//-----------------------------------------------------------------------------
int CSchema::FindIFieldSQL( const char *pchName )
{
	for ( int iField = 0; iField < m_VecField.Count(); iField++ )
	{
		if ( 0 == Q_strncmp( pchName, m_VecField[iField].m_rgchSQLName, k_cSQLObjectNameMax ) )
			return iField;
	}

	return k_iFieldNil;
}


//-----------------------------------------------------------------------------
// Purpose: Adds a schema conversion instruction (for use in converting from
//			a different Schema to this one).
//-----------------------------------------------------------------------------
void CSchema::AddDeleteField( const char *pchFieldName )
{
	DeleteField_t &deleteField = m_VecDeleteField[m_VecDeleteField.AddToTail()];
	Q_strncpy( deleteField.m_rgchFieldName, pchFieldName, sizeof( deleteField.m_rgchFieldName ) );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a schema conversion instruction (for use in converting from
//			a different Schema to this one).
//-----------------------------------------------------------------------------
void CSchema::AddRenameField( const char *pchFieldNameOld, const char *pchFieldNameNew )
{
	RenameField_t &renameField = m_VecRenameField[m_VecRenameField.AddToTail()];
	Q_strncpy( renameField.m_rgchFieldNameOld, pchFieldNameOld, sizeof( renameField.m_rgchFieldNameOld ) );
	renameField.m_iFieldDst = FindIField( pchFieldNameNew );
	Assert( k_iFieldNil != renameField.m_iFieldDst );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a schema conversion instruction (for use in converting from
//			a different Schema to this one).
// Input:	pchFieldNameOld -	Name of the field in the old schema
//			pchFieldNameMew -	Name of the field in the new schema
//			pfnAlterField -		Function to translate data from the old format to
//								the new
//-----------------------------------------------------------------------------
void CSchema::AddAlterField( const char *pchFieldNameOld, const char *pchFieldNameNew, PfnAlterField_t pfnAlterField )
{
	Assert( pfnAlterField );
	AlterField_t &alterField = m_VecAlterField[m_VecAlterField.AddToTail()];
	Q_strncpy( alterField.m_rgchFieldNameOld, pchFieldNameOld, sizeof( alterField.m_rgchFieldNameOld ) );
	alterField.m_iFieldDst = FindIField( pchFieldNameNew );
	Assert( k_iFieldNil != alterField.m_iFieldDst );
	alterField.m_pfnAlterFunc = pfnAlterField;
}


//-----------------------------------------------------------------------------
// Purpose:	Figures out how to map a field from another Schema into us.
//			First we check our conversion instructions to see if any apply,
//			and then we look for a straightforward match.
// Input:	pchFieldName -		Name of the field we're trying to map
//			piFieldDst -		[Return] Index of the field to map it to
//			ppfnAlterField -	[Return] Optional function to convert data
// Output:	true if we know what to do with this field (if false, the conversion
//			is undefined and dangerous).
//-----------------------------------------------------------------------------
bool CSchema::BCanConvertField( const char *pchFieldName, int *piFieldDst, PfnAlterField_t *ppfnAlterField )
{
	*ppfnAlterField = NULL;

	// Should this field be deleted?
	for ( int iDeleteField = 0; iDeleteField < m_VecDeleteField.Count(); iDeleteField++ )
	{
		if ( 0 == Q_strcmp( pchFieldName, m_VecDeleteField[iDeleteField].m_rgchFieldName ) )
		{
			*piFieldDst = k_iFieldNil;
			return true;
		}
	}

	// Should this field be renamed?
	for ( int iRenameField = 0; iRenameField < m_VecRenameField.Count(); iRenameField++ )
	{
		if ( 0 == Q_strcmp( pchFieldName, m_VecRenameField[iRenameField].m_rgchFieldNameOld ) )
		{
			*piFieldDst = m_VecRenameField[iRenameField].m_iFieldDst;
			return true;
		}
	}

	// Was this field altered?
	for ( int iAlterField = 0; iAlterField < m_VecAlterField.Count(); iAlterField++ )
	{
		if ( 0 == Q_strcmp( pchFieldName, m_VecAlterField[iAlterField].m_rgchFieldNameOld ) )
		{
			*piFieldDst = m_VecAlterField[iAlterField].m_iFieldDst;
			*ppfnAlterField = m_VecAlterField[iAlterField].m_pfnAlterFunc;
			return true;
		}
	}

	// Find out which of our fields this field maps to (if it doesn't map
	// to any of them, we don't know what to do with it).
	*piFieldDst = FindIField( pchFieldName );
	return ( k_iFieldNil != *piFieldDst );
}

//-----------------------------------------------------------------------------
// Purpose: Return the size to use when writing to a field.
//-----------------------------------------------------------------------------
int Field_t::CubFieldUpdateSize() const
{
	switch ( m_EType )
	{
	case k_EGCSQLType_String:
	case k_EGCSQLType_Blob:
	case k_EGCSQLType_Image:
		// Nobody should call this function for
		// var-length fields
		Assert( false );
		return m_cubLength;

	case k_EGCSQLType_int8:
	case k_EGCSQLType_int16:
	case k_EGCSQLType_int32:
	case k_EGCSQLType_int64:
	case k_EGCSQLType_float:
	case k_EGCSQLType_double:
		return m_cubLength;

	default:
		Assert(false);
		return 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Tell whether or not a field is of string type.
//-----------------------------------------------------------------------------
bool Field_t::BIsStringType() const
{
	return ( k_EGCSQLType_String == m_EType );
}	


//-----------------------------------------------------------------------------
// Purpose: Tell whether or not the field is of variable-length type.
//-----------------------------------------------------------------------------
bool Field_t::BIsVariableLength() const
{
	return ( k_EGCSQLType_String == m_EType ) || ( k_EGCSQLType_Blob == m_EType ) || ( k_EGCSQLType_Image == m_EType );
}


//-----------------------------------------------------------------------------
// Purpose: Add a foreign key
//-----------------------------------------------------------------------------
void CSchema::AddFK( const char* pchName, const char* pchColumn, const char* pchParentTable, const char* pchParentColumn, EForeignKeyAction eOnDeleteAction, EForeignKeyAction eOnUpdateAction )
{
	int iTail = m_VecFKData.AddToTail();
	FKData_t &fkData = m_VecFKData[iTail];

	Q_snprintf( fkData.m_rgchName, Q_ARRAYSIZE( fkData.m_rgchName ), "%s_%s", GetPchName(), pchName );
	Q_strncpy( fkData.m_rgchParentTableName, pchParentTable, Q_ARRAYSIZE( fkData.m_rgchParentTableName ) );
	fkData.m_eOnDeleteAction = eOnDeleteAction;
	fkData.m_eOnUpdateAction = eOnUpdateAction;

	// Now we need to split up the column name strings and add their data
	FKColumnRelation_t colRelation;
	Q_memset( &colRelation, 0, sizeof( FKColumnRelation_t ) );
	uint iMyColumn = 0;
	uint iParentColumn = 0;
	uint iParentString = 0;
	for( uint i=0; i<(uint)Q_strlen( pchColumn )+1; ++i )
	{
		if ( pchColumn[i] != ',' && pchColumn[i] != 0 )
		{
			colRelation.m_rgchCol[ iMyColumn++ ] = pchColumn[i];
		}
		else
		{
			Assert( Q_strlen( colRelation.m_rgchCol ) );
			// Should have a matching column name in the parent string
			while( iParentString < (uint)Q_strlen( pchParentColumn ) )
			{
				if ( pchParentColumn[iParentString] != ',' )
				{
					colRelation.m_rgchParentCol[ iParentColumn++ ] = pchParentColumn[iParentString];
					++iParentString;
				}
				else
				{
					++iParentString;
					break;
				}
			}

			AssertMsg( Q_strlen( colRelation.m_rgchParentCol ), "Column counts for FK do not match between child/parent\n" );
			fkData.m_VecColumnRelations.AddToTail( colRelation );
			Q_memset( &colRelation, 0, sizeof( FKColumnRelation_t ) );
			// Reset positions
			iMyColumn = 0;
			iParentColumn = 0;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Caches the insert statement for each table so we don't need to
//	generate it for each row we insert.
//-----------------------------------------------------------------------------
const char *CSchema::GetInsertStatementText() const
{
	if ( m_sInsertStatementText.IsEmpty() )
	{
		TSQLCmdStr sBuilder;
		BuildInsertStatementText( &sBuilder, GetRecordInfo() );
		m_sInsertStatementText.Set( sBuilder );
	}

	return m_sInsertStatementText;
}

//-----------------------------------------------------------------------------
// Purpose: Caches the insert via MERGE statement for each table so we don't need to
//	generate it for each row we insert.
//-----------------------------------------------------------------------------
const char *CSchema::GetMergeStatementTextOnPKWhenMatchedUpdateWhenNotMatchedInsert()
{
	if ( m_sMergeStatementTextOnPKWhenMatchedUpdateWhenNotMatchedInsert.IsEmpty() )
	{
		TSQLCmdStr sBuilder;
		BuildMergeStatementTextOnPKWhenMatchedUpdateWhenNotMatchedInsert( &sBuilder, GetRecordInfo() );
		m_sMergeStatementTextOnPKWhenMatchedUpdateWhenNotMatchedInsert.Set( sBuilder );
	}

	return m_sMergeStatementTextOnPKWhenMatchedUpdateWhenNotMatchedInsert;
}


//-----------------------------------------------------------------------------
// Purpose: Caches the insert via MERGE statement for each table so we don't need to
//	generate it for each row we insert.
//-----------------------------------------------------------------------------
const char *CSchema::GetMergeStatementTextOnPKWhenNotMatchedInsert()
{
	if ( m_sMergeStatementTextOnPKWhenNotMatchedInsert.IsEmpty() )
	{
		TSQLCmdStr sBuilder;
		BuildMergeStatementTextOnPKWhenNotMatchedInsert( &sBuilder, GetRecordInfo() );
		m_sMergeStatementTextOnPKWhenNotMatchedInsert.Set( sBuilder );
	}

	return m_sMergeStatementTextOnPKWhenNotMatchedInsert;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of foreign key constraints defined for the table
//-----------------------------------------------------------------------------
int CSchema::GetFKCount()
{
	return m_VecFKData.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Get data for a foreign key by index (valid for 0...GetFKCount()-1)
//-----------------------------------------------------------------------------
FKData_t &CSchema::GetFKData( int iIndex )
{
	return m_VecFKData[iIndex];
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Run a global validation pass on all of our data structures and memory
//			allocations.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CSchema::Validate( CValidator &validator, const char *pchName )
{
	// 1.
	// Claim our memory
	VALIDATE_SCOPE();

	m_VecField.Validate( validator, "m_VecField" );
	m_VecDeleteField.Validate( validator, "m_VecDeleteField" );
	m_VecRenameField.Validate( validator, "m_VecRenameField" );
	m_VecIndexes.Validate( validator, "m_VecIndexes" );
	m_VecFullTextIndexes.Validate( validator, "m_VecFullTextIndexes" );
	ValidateObj( m_VecFKData );
	FOR_EACH_VEC( m_VecFKData, i )
	{
		ValidateObj( m_VecFKData[i] );
	}

	for ( int iIndex = 0; iIndex < m_VecIndexes.Count(); iIndex++ )
	{
		ValidateObj( m_VecIndexes[iIndex] );
	}

	ValidateObj( m_sInsertStatementText );

	// 2.
	// Validate that our fields make sense
#if defined(_DEBUG)
	uint32 dubOffset = 0;
	for ( int iField = 0; iField < m_VecField.Count(); iField++ )
	{
		Assert( dubOffset == m_VecField[iField].m_dubOffset );
		dubOffset += m_VecField[iField].m_cubLength;
	}
#endif // defined(_DEBUG)
}


//-----------------------------------------------------------------------------
// Purpose:	Validates that a given record from our table is in a good state.
// Input:	pubRecord -		Record to validate
//-----------------------------------------------------------------------------
void CSchema::ValidateRecord( uint8 *pubRecord )
{
	// Make sure each record is in a consistent state
	for ( int iField = 0; iField < m_VecField.Count(); iField++ )
	{
		Field_t &field = m_VecField[iField];

		// Ensure that strings are null-terminated, and that everything after the terminator is 0
		if ( k_EGCSQLType_String == field.m_EType )
		{
			char *pchField = ( char * ) pubRecord + field.m_dubOffset;
			Assert( 0 == pchField[field.m_cubLength - 1] );
			for ( uint32 ich = 0; ich < field.m_cubLength; ich++ )
			{
				if ( 0 == pchField[ich] )
				{
					while ( ich < field.m_cubLength )
					{
						Assert( 0 == pchField[ich] );
						ich++;
					}
				}
			}
		}
	}
}
#endif // DBGFLAG_VALIDATE
} // namespace GCSDK
