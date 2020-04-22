//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CRecordBase::~CRecordBase()
{
	Cleanup();
}


//-----------------------------------------------------------------------------
// Purpose: Copy constructor
// Input:	that -			CRecord to copy from
//-----------------------------------------------------------------------------
CRecordBase::CRecordBase( const CRecordBase &that )
{
	*this = that;
}


//-----------------------------------------------------------------------------
// Purpose: Assignment operator - COPIES the record data
// Input:	that -			CRecord to copy from
//-----------------------------------------------------------------------------
CRecordBase& CRecordBase::operator = ( const CRecordBase & that )
{
	Assert( GetITable() == that.GetITable() );

	// COPY that record
	Copy( that );

	return *this;
}


//-----------------------------------------------------------------------------
// Purpose: Copies the data in the record. This is overridden by CRecordVar and
//			CRecordExternal
// Input:	that -			CRecord to copy from
//-----------------------------------------------------------------------------
void CRecordBase::Copy( const CRecordBase & that )
{
	Cleanup();
	Q_memcpy( PubRecordFixed(), that.PubRecordFixed(), GetPSchema()->CubRecordFixed() );
}


//-----------------------------------------------------------------------------
// Purpose: Return the record info for this record's schema
//-----------------------------------------------------------------------------
const CRecordInfo *CRecordBase::GetPRecordInfo() const
{
	return GetPSchema()->GetRecordInfo();
}


//-----------------------------------------------------------------------------
// Purpose: Copies the data in the var record.
// Input:	that -			CRecord to copy from
//-----------------------------------------------------------------------------
void CRecordVar::Copy( const CRecordBase & baseThat )
{
	const CRecordVar & that = (const CRecordVar &)baseThat;

	// COPY that record
	Cleanup();
	m_pSchema = that.m_pSchema;
	Q_memcpy( PubRecordFixed(), that.PubRecordFixed(), GetPSchema()->CubRecordFixed() );

	SetFlag( k_EAllocatedVarBlock, false );
	if ( VarFieldBlockInfo_t *pVarBlockInfo = GetPSchema()->PVarFieldBlockInfoFromRecord(	PubRecordFixed() ) )
	{
		if ( pVarBlockInfo->m_cubBlock )
		{
			void *pvNewBlock = malloc( pVarBlockInfo->m_cubBlock );
			Q_memcpy( pvNewBlock, pVarBlockInfo->m_pubBlock, pVarBlockInfo->m_cubBlock );
			pVarBlockInfo->m_pubBlock = ( uint8 * )pvNewBlock;
			SetFlag( k_EAllocatedVarBlock, true );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Copies the data in the var record.
// Input:	that -			CRecord to copy from
//-----------------------------------------------------------------------------
void CRecordExternal::Copy( const CRecordBase & baseThat )
{
	const CRecordExternal & that = (const CRecordExternal &)baseThat;

	Cleanup();
	m_pSchema = that.m_pSchema;

	m_pubRecordFixedExternal = ( uint8 * )malloc( m_pSchema->CubRecordFixed() );
	Q_memcpy( m_pubRecordFixedExternal, that.PubRecordFixed(), m_pSchema->CubRecordFixed() );
	SetFlag( k_EAllocatedFixed, true );

	SetFlag( k_EAllocatedVarBlock, false );
	if ( VarFieldBlockInfo_t *pVarBlockInfo = m_pSchema->PVarFieldBlockInfoFromRecord(	PubRecordFixed() ) )
	{
		if ( pVarBlockInfo->m_cubBlock )
		{
			void *pvNewBlock = malloc( pVarBlockInfo->m_cubBlock );
			Q_memcpy( pvNewBlock, pVarBlockInfo->m_pubBlock, pVarBlockInfo->m_cubBlock );
			pVarBlockInfo->m_pubBlock = ( uint8 * )pvNewBlock;
			SetFlag( k_EAllocatedVarBlock, true );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Initialize to an empty record
// Input:	pSchema -			Schema for the record this will hold
//-----------------------------------------------------------------------------
void CRecordExternal::Init( CSchema *pSchema )
{
	Cleanup();
	m_pSchema = pSchema;
	m_pubRecordFixedExternal = ( uint8 * )malloc( m_pSchema->CubRecordFixed() );
	Q_memset( m_pubRecordFixedExternal, 0, m_pSchema->CubRecordFixed() );
	SetFlag( k_EAllocatedFixed, true );
}


//-----------------------------------------------------------------------------
// Purpose: Initialize pointing to a record expanded in memory
// Input:	pSchema -			Schema for the record this will hold
//			pubRecord -			Pointer to fixed record data
//			bTakeOwnership -	Should we delete the record when destroyed
// Output:	Size of the record's data
//-----------------------------------------------------------------------------
int CRecordBase::InitFromBytes( uint8 *pubRecord )
{
	Cleanup();
	
	Q_memcpy( PubRecordFixed(), pubRecord, GetPSchema()->CubRecordFixed() );
	int cubRead = GetPSchema()->CubRecordFixed();
	return cubRead;
}

int CRecordVar::InitFromBytes( uint8 *pubRecord )
{
	Cleanup();

	Q_memcpy( PubRecordFixed(), pubRecord, GetPSchema()->CubRecordFixed() );
	int cubRead = GetPSchema()->CubRecordFixed();
	if ( VarFieldBlockInfo_t *pVarBlockInfo = GetPSchema()->PVarFieldBlockInfoFromRecord(	PubRecordFixed() ) )
	{
		if ( pVarBlockInfo->m_cubBlock )
		{
			void *pvNewBlock = malloc( pVarBlockInfo->m_cubBlock );
			Q_memcpy( pvNewBlock, pVarBlockInfo->m_pubBlock, pVarBlockInfo->m_cubBlock );
			pVarBlockInfo->m_pubBlock = ( uint8 * )pvNewBlock;
			SetFlag( k_EAllocatedVarBlock, true );
			cubRead += pVarBlockInfo->m_cubBlock;
		}
	}

	return cubRead;
}


int CRecordExternal::Init( CSchema *pSchema, uint8 *pubRecord, bool bTakeOwnership )
{
	m_pSchema = pSchema;
	m_pubRecordFixedExternal = pubRecord;
	SetFlag( k_EAllocatedFixed, bTakeOwnership );
	SetFlag( k_EAllocatedVarBlock, bTakeOwnership );
	int cubRead = m_pSchema->CubRecordFixed() + CubRecordVarBlock();

	return cubRead;
}

CSchema *CRecordBase::GetPSchema()
{
	return GetPSchemaImpl();
}

CSchema *CRecordBase::GetPSchemaImpl()
{
	CSchema *pSchema = NULL;
	int i = GetITable();
	if ( i != -1 )
		pSchema = &GSchemaFull().GetSchema( i );
	return pSchema;
}

//-----------------------------------------------------------------------------
// Purpose: Render a field to a buffer
// Input:	unColumn - field to render
//			cchBuffer - size of render buffer
//			pchBuffer - buffer to render into
//-----------------------------------------------------------------------------
void CRecordBase::RenderField( uint32 unColumn, int cchBuffer, char *pchBuffer ) const
{
	Q_strncpy( pchBuffer, "", cchBuffer );

	uint8 *pubData;
	uint32 cubData;
	if ( !BGetField( unColumn, &pubData, &cubData ) )
		return;

	// Get the column info and figure out how to interpret the data
	ConvertFieldToText( GetPRecordInfo()->GetColumnInfo( unColumn ).GetType(), pubData, cubData, pchBuffer, cchBuffer, false );
}


//-----------------------------------------------------------------------------
// Purpose: Reset to base state, freeing any memory we are responsible for
//-----------------------------------------------------------------------------
void CRecordBase::Cleanup()
{
}

void CRecordVar::Cleanup()
{
	// Must do this before freeing memory that encloses it
	// (eg releasing the net packet)
	if ( BFlagSet( k_EAllocatedVarBlock ) )
	{
		void *pvVarBlock = m_pSchema->PVarFieldBlockInfoFromRecord( PubRecordFixed() )->m_pubBlock;
		free( pvVarBlock );
		m_pSchema->PVarFieldBlockInfoFromRecord( PubRecordFixed() )->m_pubBlock = NULL;
		SetFlag( k_EAllocatedVarBlock, false );
	}
}

void CRecordExternal::Cleanup()
{
	// clean up the variable-length memory we might have allocated
	if ( BFlagSet( k_EAllocatedVarBlock ) )
	{
		void *pvVarBlock = m_pSchema->PVarFieldBlockInfoFromRecord( PubRecordFixed() )->m_pubBlock;
		free( pvVarBlock );
		m_pSchema->PVarFieldBlockInfoFromRecord( PubRecordFixed() )->m_pubBlock = NULL;
		SetFlag( k_EAllocatedVarBlock, false );
	}

	// clean up the external memory we might have allocated
	if ( BFlagSet( k_EAllocatedFixed ) )
		free( m_pubRecordFixedExternal );
	SetFlag( k_EAllocatedFixed, false );
	m_pubRecordFixedExternal = NULL;

	// clean up the lowest layer, not calling CRecordVar
	CRecordBase::Cleanup();
}

//-----------------------------------------------------------------------------
// Purpose:	Deserializes a block of memory into this record
// Input:	pubData -		Memory block to deserialize from
//-----------------------------------------------------------------------------
void CRecordExternal::DeSerialize( uint8 *pubData )
{
	InitFromBytes( pubData );
}


//-----------------------------------------------------------------------------
// Purpose:	Calculates the size of this record when serialized
// Output:	Size of serialized message
//-----------------------------------------------------------------------------
uint32 CRecordBase::CubSerialized()
{
	return CubRecordFixed() + CubRecordVarBlock();
}


//-----------------------------------------------------------------------------
// Purpose: Get pointer to fixed part of record
// Output:	pubRecordFixed
//-----------------------------------------------------------------------------
uint8* CRecordBase::PubRecordFixed()
{
	return ( uint8 * )( this + 1 );
}

uint8* CRecordExternal::PubRecordFixed()
{
	Assert( m_pubRecordFixedExternal );
	return m_pubRecordFixedExternal;
}

uint8* CRecordVar::PubRecordFixed()
{
	return ( uint8 * )( this + 1 );
}


//-----------------------------------------------------------------------------
// Purpose: Get pointer to fixed part of record
// Output:	pubRecordFixed
//-----------------------------------------------------------------------------
const uint8* CRecordBase::PubRecordFixed() const
{
	return const_cast<CRecordBase *>( this )->PubRecordFixed();
}

const uint8* CRecordVar::PubRecordFixed() const
{
	return const_cast<CRecordVar *>( this )->PubRecordFixed();
}

const uint8* CRecordExternal::PubRecordFixed() const
{
	return const_cast<CRecordExternal *>( this )->PubRecordFixed();
}


//-----------------------------------------------------------------------------
// Purpose: Get size of fixed part of record
// Output:	size in bytes of fixed part
//-----------------------------------------------------------------------------
uint32 CRecordBase::CubRecordFixed() const
{
	return GetPSchema()->CubRecordFixed();
}


//-----------------------------------------------------------------------------
// Purpose: Get pointer to variable part of record
// Output:	Pointer to variable-length block -- may be NULL if this record
//			has no var-length fields or they are all empty
//-----------------------------------------------------------------------------
uint8* CRecordBase::PubRecordVarBlock()
{
	VarFieldBlockInfo_t *pVarFieldBlockInfo = GetPSchema()->PVarFieldBlockInfoFromRecord( PubRecordFixed() );
	if ( pVarFieldBlockInfo )
	{
		return pVarFieldBlockInfo->m_pubBlock;
	}
	else
	{
		return NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get pointer to variable part of record
// Output:	Pointer to variable-length block -- may be NULL if this record
//			has no var-length fields or they are all empty
//-----------------------------------------------------------------------------
const uint8* CRecordBase::PubRecordVarBlock() const
{
	return const_cast<CRecordBase *>( this )->PubRecordVarBlock();
}


//-----------------------------------------------------------------------------
// Purpose: Get size of variable part of record
// Output:	Size in bytes of var-length block - may be zero if this record
//			has no var-length fields or they are all empty
//-----------------------------------------------------------------------------
uint32 CRecordBase::CubRecordVarBlock()  const
{
	VarFieldBlockInfo_t *pVarFieldBlockInfo = GetPSchema()->PVarFieldBlockInfoFromRecord( PubRecordFixed() );
	if ( pVarFieldBlockInfo )
	{
		return pVarFieldBlockInfo->m_cubBlock;
	}
	else
	{
		return 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get size of variable part of record
// Output:	Size in bytes of var-length block - may be zero if this record
//			has no var-length fields or they are all empty
//-----------------------------------------------------------------------------
bool CRecordBase::BAssureRecordVarStorage( uint32 cVariableBytes )
{
	// get the variable field block
	VarFieldBlockInfo_t *pVarFieldBlockInfo = GetPSchema()->PVarFieldBlockInfoFromRecord( PubRecordFixed() );
	if ( pVarFieldBlockInfo )
	{
		// if we have it, see if it's got enough storage
		if ( pVarFieldBlockInfo->m_cubBlock >= cVariableBytes )
		{
			// already there
			return true;
		}

		// allocate it
		uint8* pubData = (uint8*) malloc( cVariableBytes );
		if ( pubData == NULL )
			return false;

		// do we have something right now?
		if ( pVarFieldBlockInfo->m_cubBlock != 0 )
		{
			// sure do. copy it over.
			Q_memcpy( pubData, pVarFieldBlockInfo->m_pubBlock, pVarFieldBlockInfo->m_cubBlock );

			// free what was there
			free( pVarFieldBlockInfo->m_pubBlock );
		}

		// hook up our buffer
		pVarFieldBlockInfo->m_cubBlockFree = cVariableBytes - pVarFieldBlockInfo->m_cubBlock;
		pVarFieldBlockInfo->m_cubBlock = cVariableBytes;
		pVarFieldBlockInfo->m_pubBlock = pubData;

		return true;
	}
	else
	{
		// we don't have one;
		// we've got no variable length fields, and so can't preallocate for them!
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Initialize this whole record to random data
// Input:	unPrimaryIndex -		Primary index to set
//-----------------------------------------------------------------------------
void CRecordExternal::InitRecordRandom( uint32 unPrimaryIndex )
{
	bool bRealloced = false;
	GetPSchema()->InitRecordRandom( PubRecordFixed(), unPrimaryIndex, &bRealloced, BFlagSet( k_EAllocatedVarBlock ) );

	if ( bRealloced )
		SetFlag( k_EAllocatedVarBlock, true );
}


//-----------------------------------------------------------------------------
// Purpose: Set a field in this record to random bits
// Input:	iField -			Field to set
//-----------------------------------------------------------------------------
void CRecordExternal::SetFieldRandom( int iField )
{
	bool bRealloced = false;
	GetPSchema()->SetFieldRandom( PubRecordFixed(), iField, &bRealloced, BFlagSet( k_EAllocatedVarBlock ) );

	if ( bRealloced )
		SetFlag( k_EAllocatedVarBlock, true );
}


//-----------------------------------------------------------------------------
// Purpose: Get a field (var or fixed) from this record
// Input:	iField -			Field to set
//			ppubData -			Receives pointer to fields data
//			pcubField -			Receives count of bytes of data (will count the null for strings)
// Output:	true if succeeds
//-----------------------------------------------------------------------------
bool CRecordBase::BGetField( int iField, uint8 **ppubData, uint32 *pcubField ) const
{
	return GetPSchema()->BGetFieldData( PubRecordFixed(), iField, ppubData, pcubField );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the data for a field, whether fixed or variable length
// Input:	iField -			index of field to set
//			pubData -			pointer to field data to copy from
//			cubData -			size in bytes of that data
// Output:	true if successful
//-----------------------------------------------------------------------------
bool CRecordBase::BSetField( int iField, void *pvData, uint32 cubData )
{
	bool bRealloced = false;
	bool bResult = BSetField( iField, pvData, cubData, &bRealloced );
	Assert( !bRealloced );

	return bResult;
}

bool CRecordBase::BSetField( int iField, void *pvData, uint32 cubData, bool *pbRealloced )
{
	uint8 *pubData = reinterpret_cast<uint8 *>( pvData );
	
	if ( !GetPSchema()->BSetFieldData( PubRecordFixed(), iField, pubData, cubData, pbRealloced ) )
		return false;

	return true;
}

bool CRecordVar::BSetField( int iField, void *pvData, uint32 cubData )
{
	bool bRealloced = false;
	bool bResult = CRecordBase::BSetField( iField, pvData, cubData, &bRealloced );

	if ( bRealloced )
		SetFlag( k_EAllocatedVarBlock, true );
	return bResult;
}


//-----------------------------------------------------------------------------
// Purpose: Erases a field, setting it to 0 length (if possible) and filling with nulls
// Input:	iField -			index of field to wipe
// NOTE:	This relies on CSchema::BSetFieldData nulling out the rest of a field when it is set to 0 length!
//-----------------------------------------------------------------------------
void CRecordBase::WipeField( int iField )
{
	bool bRealloced = false;

	// Empty Data
	uint32 un = 0;

	// Length should be 0, except for non-variable length strings where length should be 1 (for an empty string "")
	int cub = 0;
	Field_t &field = GetPSchema()->GetField( iField );
	Assert( !field.BIsVariableLength() );
	if ( field.BIsStringType() )
		cub = 1;

	GetPSchema()->BSetFieldData( PubRecordFixed(), iField, ( uint8 * ) &un, cub, &bRealloced );

	Assert( !bRealloced );
}

void CRecordVar::WipeField( int iField )
{
	bool bRealloced = false;

	// Empty Data
	uint32 un = 0;

	// Length should be 0, except for non-variable length strings where length should be 1 (for an empty string "")
	int cub = 0;
	Field_t &field = GetPSchema()->GetField( iField );
	if ( field.BIsStringType() && !field.BIsVariableLength() )
		cub = 1;

	GetPSchema()->BSetFieldData( PubRecordFixed(), iField, ( uint8 * ) &un, cub, &bRealloced );

	if ( bRealloced )
		SetFlag( k_EAllocatedVarBlock, true );
}


//-----------------------------------------------------------------------------
// Purpose: Get a string field - will return empty string instead of NULL if field has no datas
// Input:	iField -			Field to get
//			pcubField -			Receives count of bytes of data (will count the null for strings)
// Output:	const pointer to string data (to an empty string if no data)
//-----------------------------------------------------------------------------
const char * CRecordBase::GetStringField( int iField, uint32 *pcubField )
{
	uint8 * pubData = NULL;
	*pcubField = 0;

	if ( BGetField( iField, &pubData, pcubField ) && *pcubField > 0 )
		return ( const char * ) pubData;
	else
		return "";
}


//-----------------------------------------------------------------------------
// Purpose: Get an int field
// Input:	iField -			Field to get
// Output:	Int (0 if no data)
//-----------------------------------------------------------------------------
int CRecordBase::GetInt( int iField )
{
	return ( int ) GetUint32( iField );
}


//-----------------------------------------------------------------------------
// Purpose: Get a uint16 field
// Input:	iField -			Field to get
// Output:	uint16 (0 if no data)
//-----------------------------------------------------------------------------
uint16 CRecordBase::GetUint16( int iField )
{
	uint8 * pubData = NULL;
	uint32 cubField = 0;

	DbgVerify( BGetField( iField, &pubData, &cubField ) );
	Assert( 0 < cubField );

	if ( NULL != pubData )
		return *( uint16 * ) pubData;
	else
		return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Get a uint32 field
// Input:	iField -			Field to get
// Output:	uint32 (0 if no data)
//-----------------------------------------------------------------------------
uint32 CRecordBase::GetUint32( int iField )
{
	uint8 * pubData = NULL;
	uint32 cubField = 0;

	DbgVerify( BGetField( iField, &pubData, &cubField ) );
	Assert( 0 < cubField );

	if ( NULL != pubData )
		return *( uint32 * ) pubData;
	else
		return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Get a uint64 field
// Input:	iField -			Field to get
// Output:	uint64 (0 if no data)
//-----------------------------------------------------------------------------
uint64 CRecordBase::GetUint64( int iField )
{
	uint8 * pubData = NULL;
	uint32 cubField = 0;

	DbgVerify( BGetField( iField, &pubData, &cubField ) );
	Assert( 0 < cubField );

	if ( NULL != pubData )
		return *( uint64 * ) pubData;
	else
		return 0;
}



const char * CRecordBase::ReadVarCharField( const CVarCharField &field ) const
{
	Assert( false );
	return NULL;
}

const uint8 * CRecordBase::ReadVarDataField( const CVarField &field, uint32 *pcubField ) const
{
	Assert( false );
	return NULL;
}

// These may cause a realloc
bool CRecordBase::SetVarCharField( CVarCharField &field, const char *pchString, bool bTruncate, int32 iField )
{
	Assert( false );
	return false ;
}

void CRecordBase::SetVarDataField( CVarField &field, const void *pvData, uint32 cubData )
{
	Assert( false );
}


//-----------------------------------------------------------------------------
// Purpose: Read data from a varchar field
// Input:	field -				opaque field object to read from
// Output:	pointer to data - may be NULL if that field is empty.
//-----------------------------------------------------------------------------
const char * CRecordVar::ReadVarCharField( const CVarCharField &field ) const
{
	Assert ( GetPSchema()->BHasVariableFields() );

	uint8 *pubData;
	uint32 cubData;
	if ( GetPSchema()->BGetVarField( PubRecordFixed(), &field, &pubData, &cubData ) )
		return (const char *)pubData;
	else
		return "";
}


//-----------------------------------------------------------------------------
// Purpose: Read data from a vardata field
// Input:	field -				opaque field object to read from
// Output:	pointer to data - may be NULL if that field is empty.
//-----------------------------------------------------------------------------
const uint8 *CRecordVar::ReadVarDataField( const CVarField &field, uint32 *pcubField ) const
{
	Assert ( GetPSchema()->BHasVariableFields() );

	uint8 *pubData;
	*pcubField = 0;
	if ( GetPSchema()->BGetVarField( PubRecordFixed(), &field, &pubData, pcubField ) )
		return pubData;
	else
		return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Update (in memory) a varchar field
// Input:	field -				opaque field object to update
//			pchString -			string data to set
//-----------------------------------------------------------------------------
bool CRecordVar::SetVarCharField( CVarCharField &field, const char *pchString, bool bTruncate, int32 iField )
{
	Assert ( GetPSchema()->BHasVariableFields() );
	if( iField < 0 )
	{
		AssertMsg1( false, "Encountered a bad call to SetVarCharField with an invalid field specified: %d", iField );
		return false;
	}

	bool bTruncated = false;
	int cchLen = Q_strlen( pchString ) + 1;
	
	// since we're a VARCHAR field, cbMaxLength is the length in characters
	const int cchMaxLength = m_pSchema->GetField( iField ).m_cchMaxLength;
	if ( ( cchMaxLength > 0 )  && ( cchLen > cchMaxLength ) )
	{
		if( bTruncate )
		{
			bTruncated = true;
			cchLen = cchMaxLength;
		}
		else
		{
			// caller should check his data and not pass stuff that wont fit
			AssertMsg4( false, "Overflow in SetVarCharField (%u > %u) for column %s in table %s", cchLen, cchMaxLength, m_pSchema->GetField( iField ).m_rgchName, m_pSchema->GetPchName() );
			return false;
		}
	}

	bool bRealloced = false;
	bool fSuccess = GetPSchema()->BSetVarField( PubRecordFixed(), &field, pchString, cchLen, &bRealloced, BFlagSet( k_EAllocatedVarBlock ) );
	if( fSuccess && bTruncated )
	{
		//make sure the last character is NULL if we truncated
		VarFieldBlockInfo_t *pBlock = GetPSchema()->PVarFieldBlockInfoFromRecord( PubRecordFixed() );
		uint8 *pubVarBlock = pBlock->m_pubBlock;
		char* pField = ( char* )( pubVarBlock + field.m_dubOffset );
		pField[ cchLen - 1 ] = '\0';
	}

	if ( bRealloced )
		SetFlag( k_EAllocatedVarBlock, true );
	return fSuccess;
}


//-----------------------------------------------------------------------------
// Purpose: Update (in memory) a vardata field
// Input:	field -				opaque field object to update
//			pvData -			pointer to data to put there
//			cubData -			size in bytes of the data
//-----------------------------------------------------------------------------
void CRecordVar::SetVarDataField( CVarField &field, const void *pvData, uint32 cubData )
{
	Assert ( GetPSchema()->BHasVariableFields() );

	bool bRealloced = false;
	GetPSchema()->BSetVarField( PubRecordFixed(), &field, pvData, cubData, &bRealloced, BFlagSet( k_EAllocatedVarBlock ) );

	if ( bRealloced )
		SetFlag( k_EAllocatedVarBlock, true );
}


//-----------------------------------------------------------------------------
// Purpose: Set or clear the specified flag in m_nFlags
// Input:	eFlag -				flag (single bit) to change
//			bSet -				Set it, else clear it
//-----------------------------------------------------------------------------
void CRecordVar::SetFlag( int eFlag, bool bSet )
{
	if ( bSet )
		m_nFlags |= eFlag;
	else
		m_nFlags &= ~eFlag;
}


//-----------------------------------------------------------------------------
// Purpose: Get the state of the specified flag
// Input:	eFlag -				flag (single bit) to check
//-----------------------------------------------------------------------------
bool CRecordVar::BFlagSet( int eFlag ) const
{
	return 0 != ( m_nFlags & eFlag );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Run a global validation pass on all of our data structures and memory
//			allocations.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CRecordBase::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();
}

void CRecordVar::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	if ( BFlagSet( k_EAllocatedVarBlock ) )
	{
		validator.ClaimMemory( GetPSchema()->PVarFieldBlockInfoFromRecord( PubRecordFixed() )->m_pubBlock );
	}

}

void CRecordExternal::Validate( CValidator &validator, const char *pchName )
{
	if ( BFlagSet( k_EAllocatedFixed ) )
	{
		validator.ClaimMemory( m_pubRecordFixedExternal );
	}

	CRecordBase::Validate( validator, pchName );
}

void CRecordBase::ValidateStatics( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE_STATIC( "CRecordBase class statics" );
}

#endif // DBGFLAG_VALIDATE


//-----------------------------------------------------------------------------
// Purpose: Return the schema for this record type
//-----------------------------------------------------------------------------
CSchema *CRecordType::GetSchema() const
{
	return &GSchemaFull().GetSchema( GetITable() );
}


//-----------------------------------------------------------------------------
// Purpose: Return the CRecordInfo for this record type
//-----------------------------------------------------------------------------
CRecordInfo *CRecordType::GetRecordInfo() const
{
	return GetSchema()->GetRecordInfo();
}

} // namespace GCSDK
