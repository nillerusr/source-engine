//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================


#include "stdafx.h"

//#include "sqlaccess.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{
// Memory pool for CRecordInfo
CThreadSafeClassMemoryPool<CRecordInfo> CRecordInfo::sm_MemPoolRecordInfo( 10, UTLMEMORYPOOL_GROW_FAST );

#ifdef _DEBUG
// validation tracking
CUtlRBTree<CRecordInfo *, int > CRecordInfo::sm_mapPMemPoolRecordInfo( DefLessFunc( CRecordInfo *) );
CThreadMutex CRecordInfo::sm_mutexMemPoolRecordInfo;
#endif


//-----------------------------------------------------------------------------
// determine if this fieldset is equal to the other one
//-----------------------------------------------------------------------------
/* static */
bool FieldSet_t::CompareFieldSets( const FieldSet_t& refThis, CRecordInfo* pRecordInfoThis,
	const FieldSet_t& refOther, CRecordInfo* pRecordInfoOther )
{
	// same number of columns?
	int cColumns = refThis.GetCount();
	if ( refOther.GetCount() != cColumns )
		return false;

	int cIncludedColumns = refThis.GetIncludedCount();
	if ( refOther.GetIncludedCount() != cIncludedColumns )
		return false;

	// do the regular columns first; this is order-dependent
	for ( int m = 0; m < cColumns; m++ )
	{
		int nThisField = refThis.GetField( m );
		const CColumnInfo& refThisColumn = pRecordInfoThis->GetColumnInfo( nThisField );

		int nOtherField = refOther.GetField( m );
		const CColumnInfo& refOtherColumn = pRecordInfoOther->GetColumnInfo( nOtherField );

		if ( refOtherColumn != refThisColumn )
		{
			return false;
		}
	}

	// do the included columns now; order independent
	for ( int m = 0; m < cIncludedColumns; m++ )
	{
		int nThisField = refThis.GetIncludedField( m );
		const CColumnInfo& refThisColumn = pRecordInfoThis->GetColumnInfo( nThisField );
		bool bFoundMatch = false;

		for ( int n = 0; n < cIncludedColumns; n++ )
		{
			int nOtherField = refOther.GetIncludedField( n );
			const CColumnInfo& refOtherColumn = pRecordInfoOther->GetColumnInfo( nOtherField );

			if ( refOtherColumn == refThisColumn )
			{
				bFoundMatch = true;
				break;
			}
		}

		if ( !bFoundMatch )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CRecordInfo::CRecordInfo()
: m_MapIColumnInfo( 0, 0, CaselessStringLessThan )
{
	m_rgchName[0] = 0;
	m_bPreparedForUse = false;
	m_bAllColumnsAdded = false;
	m_bHaveChecksum = false;
	m_bHaveColumnNameIndex = false;
	m_nHasPrimaryKey = k_EPrimaryKeyTypeNone;
	m_iPKIndex = -1;
	m_cubFixedSize = 0;
	m_nChecksum = 0;
	m_eSchemaCatalog = k_ESchemaCatalogInvalid;
	m_nTableID = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Initializes this record info from DS equivalent information
//-----------------------------------------------------------------------------
void CRecordInfo::InitFromDSSchema( CSchema *pSchema )
{
	// copy the name over
	SetName( pSchema->GetPchName() );

	// copy each of the fields, preallocating capacity
	int cFields = pSchema->GetCField();
	m_VecColumnInfo.EnsureCapacity( cFields );
	for ( int iField = 0; iField < cFields; iField++ )
	{
		Field_t &field = pSchema->GetField( iField );
		AddColumn( field.m_rgchSQLName, iField+1, field.m_EType, field.m_cubLength, field.m_nColFlags, field.m_cchMaxLength );
	}

	m_nTableID = pSchema->GetITable();

	// copy the list of PK index fields
	m_iPKIndex = pSchema->GetPKIndex( );

	// copy the list of Indexes
	m_VecIndexes = pSchema->GetIndexes( );

	// which schema?
	m_eSchemaCatalog = pSchema->GetESchemaCatalog();

	// copy full-text column list
	// and the index of the catalog it will create on
	m_vecFTSFields = pSchema->GetFTSColumns();
	m_nFullTextCatalogIndex = pSchema->GetFTSIndexCatalog();

	// Copy over the FK data
	int cFKs = pSchema->GetFKCount();
	for ( int i = 0; i < cFKs; ++i )
	{
		FKData_t &fkData = pSchema->GetFKData( i );
		AddFK( fkData );
	}

	// prepare for use
	PrepareForUse( );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a new column to this record info
// Input:	pchName - column name
//			nSQLColumn - column index in SQL to bind to (1-based)
//			eType - data type of column
//			cubFixedSize - for fixed-size fields, the size
//			nColFlags - attributes
//-----------------------------------------------------------------------------
void CRecordInfo::AddColumn( const char *pchName, int nSQLColumn, EGCSQLType eType, int cubFixedSize, int nColFlags, int cchMaxSize )
{
	Assert( !m_bPreparedForUse );
	if ( m_bPreparedForUse )
		return;
	uint32 unColumn = m_VecColumnInfo.AddToTail();
	CColumnInfo &columnInfo = m_VecColumnInfo[unColumn];

	columnInfo.Set( pchName, nSQLColumn, eType, cubFixedSize, nColFlags, cchMaxSize );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a new FK to this record info
//-----------------------------------------------------------------------------
void CRecordInfo::AddFK( const FKData_t &fkData )
{
	m_VecFKData.AddToTail( fkData );
}


//-----------------------------------------------------------------------------
// Purpose: compare function to sort by column name
//-----------------------------------------------------------------------------
int __cdecl CompareColumnInfo( const CColumnInfo *pColumnInfoLeft, const CColumnInfo *pColumnInfoRight )
{
	const char *pchLeft = ( (CColumnInfo *) pColumnInfoLeft )->GetName();
	const char *pchRight = ( (CColumnInfo *) pColumnInfoRight )->GetName();
	Assert( pchLeft && pchLeft[0] );	
	Assert( pchRight && pchRight[0] );
	return Q_stricmp( pchLeft, pchRight );
}


//-----------------------------------------------------------------------------
// Purpose: compares this record info to another record info
//-----------------------------------------------------------------------------
bool CRecordInfo::EqualTo( CRecordInfo* pOther )
{
	int nOurs = GetChecksum();
	int nTheirs = pOther->GetChecksum();

	// if this much isn't equal, we're no good
	if (nOurs != nTheirs)
		return false;

	if ( !CompareIndexLists( pOther ) )
		return false;

	if ( !CompareFKs( pOther ) )
		return false;

	return CompareFTSIndexLists( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: format the index list into a string
//-----------------------------------------------------------------------------
void CRecordInfo::GetIndexFieldList( CFmtStr1024 *pstr, int nIndents ) const
{
	// table name at first
	pstr->sprintf( "Table %s:\n", this->GetName() );

	// for each of the indexes ...
	for ( int n = 0; n < m_VecIndexes.Count(); n++ )
	{
		const FieldSet_t& fs = m_VecIndexes[n];

		// indent enough
		for ( int x = 0; x < nIndents; x++ )
		{
			pstr->Append( "\t" );
		}

		// show if it is clustered or not
		pstr->AppendFormat( "Index %d (%s): %sclustered, %sunique {", n,
			fs.GetIndexName(),
			fs.IsClustered() ? "" : "non-",
			fs.IsUnique() ? "" : "non-" );

		// then show all the columns
		for (int m = 0; m < fs.GetCount(); m++ )
		{
			int x = fs.GetField( m );
			const char* pstrName = m_VecColumnInfo[x].GetName();
			pstr->AppendFormat( "%s %s", ( m == 0 ) ? "" : ",", pstrName );
		}

		// then the included columns, too
		for ( int m = 0; m < fs.GetIncludedCount(); m++ )
		{
			int x = fs.GetIncludedField( m );
			const char* pstrName = m_VecColumnInfo[x].GetName();
			pstr->AppendFormat( ", *%s", pstrName );
		}
		pstr->Append( " }\n" );
	}

	return;
}


//-----------------------------------------------------------------------------
// Purpose: Get the number of foreign key constraints defined for the table
//-----------------------------------------------------------------------------
int CRecordInfo::GetFKCount()
{
	return m_VecFKData.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Get data for a foreign key by index (valid for 0...GetFKCount()-1)
//-----------------------------------------------------------------------------
FKData_t &CRecordInfo::GetFKData( int iIndex )
{
	return m_VecFKData[iIndex];
}


//-----------------------------------------------------------------------------
// Purpose: format the FK list into a string
//-----------------------------------------------------------------------------
void CRecordInfo::GetFKListString( CFmtStr1024 *pstr, int nIndents )
{
	// table name at first
	pstr->sprintf( "Table %s Foreign Keys: \n", this->GetName() );



	if ( m_VecFKData.Count() == 0 )
	{
		// indent enough
		pstr->AppendIndent( nIndents );
		pstr->Append( "No foreign keys for table\n" );
	}
	else
	{
		for ( int n = 0; n < m_VecFKData.Count(); n++ )
		{
			// indent enough
			pstr->AppendIndent( nIndents );

			FKData_t &fkData = m_VecFKData[n];
			CFmtStr sColumns, sParentColumns;
			FOR_EACH_VEC( fkData.m_VecColumnRelations, i )
			{
				FKColumnRelation_t &colRelation = fkData.m_VecColumnRelations[i];
				if ( i > 0)
				{
					sColumns += ",";
					sParentColumns += ",";
				}
				sColumns += colRelation.m_rgchCol;
				sParentColumns += colRelation.m_rgchParentCol;
			}

			pstr->AppendFormat( "CONSTRAINT %s FOREIGN KEY (%s) REFERENCES %s(%s) ON DELETE %s ON UPDATE %s\n",
				fkData.m_rgchName, sColumns.Access(), fkData.m_rgchParentTableName, sParentColumns.Access(), 
				PchNameFromEForeignKeyAction( fkData.m_eOnDeleteAction ), PchNameFromEForeignKeyAction( fkData.m_eOnUpdateAction ) );
		}
	}

	return;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CRecordInfo::AddFTSFields( CUtlVector< int > &vecFields )
{
	AssertMsg( m_vecFTSFields.Count() == 0, "Only one FTS index per table" );
	FOR_EACH_VEC( vecFields, n )
	{
		int nField = vecFields[n];
		m_vecFTSFields.AddToTail( nField );
	}

	return;
}

//-----------------------------------------------------------------------------
// Purpose: compares FK lists in this record with those of another
//-----------------------------------------------------------------------------
bool CRecordInfo::CompareFKs( CRecordInfo *pOther )
{
	if ( pOther->m_VecFKData.Count() != m_VecFKData.Count() )
		return false;

	for( int i=0; i < m_VecFKData.Count(); ++i )
	{
		FKData_t &fkDataMine = m_VecFKData[i];

		bool bFoundInOther = false;
		for ( int j=0; j < pOther->m_VecFKData.Count(); ++j )
		{
			FKData_t &fkDataOther = pOther->m_VecFKData[j];

			if ( fkDataMine == fkDataOther )
			{
				bFoundInOther = true;
				break;
			}
		}

		if ( !bFoundInOther )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Locate an index by its properties (ignoring the name).
//          Returns position of index in the index array, or -1 if not found.
//-----------------------------------------------------------------------------
int CRecordInfo::FindIndex( CRecordInfo *pRec, const FieldSet_t& fieldSet )
{
	for ( int i = 0; i < m_VecIndexes.Count(); i++ )
	{
		if ( FieldSet_t::CompareFieldSets( m_VecIndexes[i], this, fieldSet, pRec ) )
			return i;
	}

	// Not found
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Locate an index with the given name.
//          Returns position of index in the index array, or -1 if not found.
//-----------------------------------------------------------------------------
int CRecordInfo::FindIndexByName( const char *pszName ) const
{
	for ( int i = 0; i < m_VecIndexes.Count(); i++ )
	{
		if ( V_stricmp( m_VecIndexes[i].GetIndexName(), pszName )== 0 )
			return i;
	}

	// Not found
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: compares index lists in this record with those of another
//-----------------------------------------------------------------------------
bool CRecordInfo::CompareIndexLists( CRecordInfo* pOther )
{
	// compare the index lists (but don't use CRCs)

	// different size? can't be the same
	if ( pOther->GetIndexFieldCount() != GetIndexFieldCount() )
	{
		return false;
	}

	// We have to loop through both lists of indexes and try to find a match.
	// We also must make sure the match is exact, and that no previous match
	// can alias another attempt at a match. Pretty messy, but with no available
	// identity over index objects, we're forced to a suboptimal solution.

	int nIndexes = GetIndexFieldCount();

	// get a copy of the other index vector, which we'll remove items from as
	// matches are found.

	CUtlVector<FieldSet_t> vecOtherIndexes;
	vecOtherIndexes.CopyArray( pOther->GetIndexFields().Base(), nIndexes );

	for ( int nOurs = 0; nOurs < nIndexes; nOurs++ )
	{
		int nOtherMatchIndex = -1;
		const FieldSet_t& refOurs = GetIndexFields()[nOurs];

		// rip through copy of other to find one that matches
		for ( int nOther = 0; nOther < vecOtherIndexes.Count(); nOther++ )
		{
			const FieldSet_t& refOther = vecOtherIndexes[nOther];
			if ( FieldSet_t::CompareFieldSets( refOurs, this, refOther, pOther ) )
			{
				nOtherMatchIndex = nOther;
				break;
			}
		}

		if ( nOtherMatchIndex >= 0 )
		{
			// this works! remove it from other copy
			vecOtherIndexes.Remove( nOtherMatchIndex );
		}
		else
		{
			// something didn't match, so bail out early
			return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: compares full-text indexes for this record with those of another
//			column order in an FTS is irrelevant, so this is a simple match
//-----------------------------------------------------------------------------
bool CRecordInfo::CompareFTSIndexLists( CRecordInfo* pOther ) const
{
	// compare full-text index columns
	if ( m_vecFTSFields.Count() != pOther->m_vecFTSFields.Count() )
	{
		// counts don't match, so obviously no good
		return false;
	}
	for ( int nColumnIndex = 0; nColumnIndex < m_vecFTSFields.Count(); nColumnIndex++ )
	{
		bool bFound = false;
		for ( int nInnerIndex = 0; nInnerIndex < pOther->m_vecFTSFields.Count(); nInnerIndex++ )
		{
			if ( m_vecFTSFields[nInnerIndex] == pOther->m_vecFTSFields[nColumnIndex] )
			{
				bFound = true;
				break;
			}
		}

		if ( !bFound )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the checksum for this record info
//-----------------------------------------------------------------------------
int	CRecordInfo::GetChecksum()
{
	Assert( m_bPreparedForUse );

	// calculate it now if we haven't already
	if ( !m_bHaveChecksum )
		CalculateChecksum();

	return m_nChecksum;
}


//-----------------------------------------------------------------------------
// Purpose: Prepares this object for use after all columns have been added
//-----------------------------------------------------------------------------
void CRecordInfo::PrepareForUse()
{
	Assert( !m_bPreparedForUse );
	Assert( 0 == m_cubFixedSize );
	Assert( 0 == m_nChecksum );

	SetAllColumnsAdded();

	FOR_EACH_VEC( m_VecColumnInfo, nColumn )
	{
		CColumnInfo &columnInfo = m_VecColumnInfo[nColumn];

		// keep track of total fixed size of all columns
		if ( !columnInfo.BIsVariableLength() )
			m_cubFixedSize += columnInfo.GetFixedSize();

		if ( columnInfo.BIsPrimaryKey() )
		{
			// a PK column! if we have seen one before,
			// know we have a-column PK; otherwise, a single column PK
			if (m_nHasPrimaryKey == k_EPrimaryKeyTypeNone)
				m_nHasPrimaryKey = k_EPrimaryKeyTypeSingle;
			else
				m_nHasPrimaryKey = k_EPrimaryKeyTypeMulti;
		}
	}

	// make sure count matches the enum
	/*
	Assert( ( m_nHasPrimaryKey == k_EPrimaryKeyTypeNone && m_VecPKFields.Count() == 0 ) ||
	( m_nHasPrimaryKey == k_EPrimaryKeyTypeMulti && m_VecPKFields.Count() > 1) ||
	( m_nHasPrimaryKey == k_EPrimaryKeyTypeSingle && m_VecPKFields.Count() == 1) );
	*/

	m_bPreparedForUse = true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns index of column with specified name
// Input:	pchName - column name
//			punColumn - pointer to fill in with index
// Output:	return true if found, false otherwise
//-----------------------------------------------------------------------------
bool CRecordInfo::BFindColumnByName( const char *pchName, int *punColumn )
{
	Assert( m_bAllColumnsAdded );
	Assert( pchName && *pchName );
	Assert( punColumn );

	*punColumn = -1;

	// if we haven't already built the name index, build it now
	if ( !m_bHaveColumnNameIndex )
		BuildColumnNameIndex();

	*punColumn = m_MapIColumnInfo.Find( pchName );
	return ( m_MapIColumnInfo.InvalidIndex() != *punColumn );
}


//-----------------------------------------------------------------------------
// Purpose: Sets the name of this record info
// Input:	pchName - name
// Notes:	record info that describes a table will have a name (the table name);
//			record info that describes a result set will not
//-----------------------------------------------------------------------------
void CRecordInfo::SetName( const char *pchName )
{
	Assert( pchName && *pchName );
	Assert( !m_bPreparedForUse );	// don't change this after prepared for use
	Q_strncpy( m_rgchName, pchName, Q_ARRAYSIZE( m_rgchName ) );
}


//-----------------------------------------------------------------------------
// Purpose: Builds the column name index for fast lookup by name
//-----------------------------------------------------------------------------
void CRecordInfo::BuildColumnNameIndex()
{
	AUTO_LOCK( m_Mutex );

	if ( m_bHaveColumnNameIndex )
		return;

	Assert( m_bAllColumnsAdded );

	Assert( 0 == m_MapIColumnInfo.Count() );

	FOR_EACH_VEC( m_VecColumnInfo, nColumn )
	{
		// build name->column index map
		CColumnInfo &columnInfo = m_VecColumnInfo[nColumn];
		m_MapIColumnInfo.Insert( columnInfo.GetName(), nColumn );
	}
	m_bHaveColumnNameIndex = true;
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the checksum for this record info
//-----------------------------------------------------------------------------
void CRecordInfo::CalculateChecksum()
{
	AUTO_LOCK( m_Mutex );

	if ( m_bHaveChecksum )
		return;

	// build the column name index if necessary
	if ( !m_bHaveColumnNameIndex )
		BuildColumnNameIndex();

	CRC32_t crc32;
	CRC32_Init( &crc32 );

	FOR_EACH_MAP( m_MapIColumnInfo, iMapItem )
	{
		uint32 unColumn = m_MapIColumnInfo[iMapItem];
		CColumnInfo &columnInfo = m_VecColumnInfo[unColumn];
		// calculate checksum of all of our columns
		columnInfo.CalculateChecksum();
		int nChecksum = columnInfo.GetChecksum();
		CRC32_ProcessBuffer( &crc32, (void*) &nChecksum, sizeof( nChecksum ) );	
	}

	// keep checksum for entire record info
	CRC32_Final( &crc32 );
	m_nChecksum = crc32;
	m_bHaveChecksum = true;
}

//-----------------------------------------------------------------------------
// Purpose: add another index disallowing duplicates. If a duplicate item is
//			found, we'll set the flags on the new item from the existing one.
//-----------------------------------------------------------------------------
int CRecordInfo::AddIndex( const FieldSet_t& fieldSet )
{
	for ( int n = 0; n < m_VecIndexes.Count(); n++ )
	{
		FieldSet_t& fs = m_VecIndexes[n];
		if ( FieldSet_t::CompareFieldSets( fieldSet, this, fs, this ) )
		{
			fs.SetClustered( fs.IsClustered() );
			return -1;
		}
	}

	int nRet = m_VecIndexes.AddToTail( fieldSet );
	return nRet;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if there is an IDENTITY column in the record info
//-----------------------------------------------------------------------------
bool CRecordInfo::BHasIdentity() const
{
	FOR_EACH_VEC( m_VecColumnInfo, nColumn)
	{
		if( m_VecColumnInfo[nColumn].BIsAutoIncrement() )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CColumnInfo::CColumnInfo() 
{
	m_rgchName[0] = 0;
	m_nSQLColumn = 0;
	m_eType = k_EGCSQLTypeInvalid;
	m_nColFlags = 0;
	m_cubFixedSize = 0;
	m_cchMaxSize = 0;
	m_nChecksum = 0;
	m_bHaveChecksum = false;
}

//-----------------------------------------------------------------------------
// Purpose: Sets column info for this column
// Input:	pchName - column name
//			nSQLColumn - column index in SQL to bind to (1-based)
//			eType - data type of column
//			cubFixedSize - for fixed-size fields, the size
//			nColFlags - attributes
//-----------------------------------------------------------------------------
void CColumnInfo::Set( const char *pchName, int nSQLColumn, EGCSQLType eType, int cubFixedSize, int nColFlags, int cchMaxSize )
{
	Assert( !m_rgchName[0] );
	Q_strncpy( m_rgchName, pchName, Q_ARRAYSIZE( m_rgchName ) );
	m_nSQLColumn = nSQLColumn;
	m_eType = eType;

	m_nColFlags = nColFlags;
	ValidateColFlags();

	if ( !BIsVariableLength() )
	{
		Assert( cubFixedSize > 0 );
		m_cubFixedSize = cubFixedSize;
		m_cchMaxSize = 0;
	}
	else
	{
		// it's variable length, so we need a max length
		m_cchMaxSize = cchMaxSize;
		m_cubFixedSize = 0;
	}
}



//-----------------------------------------------------------------------------
// Purpose: returns whether this column is variable length
//-----------------------------------------------------------------------------
bool CColumnInfo::BIsVariableLength() const
{
	return m_eType == k_EGCSQLType_Blob || m_eType == k_EGCSQLType_String || m_eType == k_EGCSQLType_Image;
}

//-----------------------------------------------------------------------------
// Purpose: convert column flags to a visible representation
//-----------------------------------------------------------------------------
void CColumnInfo::GetColFlagDescription( char* pstrOut, int cubOutLength ) const
{
	if ( m_nColFlags == 0 )
		Q_strncpy( pstrOut, "(none)", cubOutLength );
	else
	{
		pstrOut[0] = 0;
		if ( m_nColFlags & k_nColFlagIndexed )
			Q_strncat( pstrOut, "(Indexed)", cubOutLength );
		if ( m_nColFlags & k_nColFlagUnique )
			Q_strncat( pstrOut, "(Unique)", cubOutLength );
		if ( m_nColFlags & k_nColFlagPrimaryKey )
			Q_strncat( pstrOut, "(PrimaryKey)", cubOutLength );
		if ( m_nColFlags & k_nColFlagAutoIncrement )
			Q_strncat( pstrOut, "(AutoIncrement)", cubOutLength );
		if ( m_nColFlags & k_nColFlagClustered )
			Q_strncat( pstrOut, "(Clustered)", cubOutLength );
	}

	return;
}



//-----------------------------------------------------------------------------
// Purpose: sets column flag bits
// Input:	nColFlag - bits to set.  (Other bits are not cleared.)
//-----------------------------------------------------------------------------
void CColumnInfo::SetColFlagBits( int nColFlag )
{
	ValidateColFlags();
	m_nColFlags |= nColFlag;	// set these bits
	ValidateColFlags();
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the checksum for this column
//-----------------------------------------------------------------------------
void CColumnInfo::CalculateChecksum()
{
	if ( m_bHaveChecksum )
		return;

	// calculate checksum of this column for easy comparsion
	CRC32_t crc32;
	CRC32_Init( &crc32 );
	CRC32_ProcessBuffer( &crc32, (void*) m_rgchName, Q_strlen( m_rgchName ) );
	CRC32_ProcessBuffer( &crc32, (void*) &m_nColFlags, sizeof( m_nColFlags ) );
	CRC32_ProcessBuffer( &crc32, (void*) &m_eType, sizeof( m_eType ) );
	CRC32_ProcessBuffer( &crc32, (void*) &m_cubFixedSize, sizeof( m_cubFixedSize ) );
	CRC32_ProcessBuffer( &crc32, (void*) &m_cchMaxSize, sizeof( m_cchMaxSize ) );
	CRC32_Final( &crc32 );

	m_nChecksum = crc32;
	m_bHaveChecksum = true;
}

//-----------------------------------------------------------------------------
// determine if this CColumnInfo is the same as the referenced
//-----------------------------------------------------------------------------
bool CColumnInfo::operator==( const CColumnInfo& refOther ) const
{
	if ( m_eType != refOther.m_eType )
		return false;
	if ( m_cubFixedSize != refOther.m_cubFixedSize )
		return false;
	if ( m_cchMaxSize != refOther.m_cchMaxSize )
		return false;
	if ( m_nColFlags != refOther.m_nColFlags )
		return false;
	if ( 0 != Q_strcmp( m_rgchName, refOther.m_rgchName ) )
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Validates that column flags are set in valid combinations
//-----------------------------------------------------------------------------
void CColumnInfo::ValidateColFlags() const
{
	// Check that column flags follow rules about how columns get expressed in SQL

	if ( m_nColFlags & k_nColFlagPrimaryKey )
	{
		// a primary key must also be unique and indexed
		Assert( m_nColFlags & k_nColFlagUnique );
		Assert( m_nColFlags & k_nColFlagIndexed );
	}

	// a column with uniqueness constraint must also be indexed
	if ( m_nColFlags & k_nColFlagUnique )
		Assert( m_nColFlags & k_nColFlagIndexed );
}


CRecordInfo *CRecordInfo::Alloc() 
{ 
	CRecordInfo *pRecordInfo = sm_MemPoolRecordInfo.Alloc();

#ifdef _DEBUG
	AUTO_LOCK( sm_mutexMemPoolRecordInfo );
	sm_mapPMemPoolRecordInfo.Insert( pRecordInfo );
#endif

	return pRecordInfo;
}


void CRecordInfo::DestroyThis() 
{ 
#ifdef _DEBUG
	AUTO_LOCK( sm_mutexMemPoolRecordInfo );
	sm_mapPMemPoolRecordInfo.Remove( this );
#endif

	sm_MemPoolRecordInfo.Free( this ); 
}


#ifdef DBGFLAG_VALIDATE


void CRecordInfo::ValidateStatics( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE_STATIC( "CRecordInfo class statics" );

	ValidateObj( sm_MemPoolRecordInfo );

#ifdef _DEBUG
	AUTO_LOCK( sm_mutexMemPoolRecordInfo );
	ValidateObj( sm_mapPMemPoolRecordInfo );
	FOR_EACH_MAP_FAST( sm_mapPMemPoolRecordInfo, i )
	{
		sm_mapPMemPoolRecordInfo[i]->Validate( validator, "sm_mapPMemPoolRecordInfo[i]" );
	}
#endif
}


void CRecordInfo::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	m_VecIndexes.Validate( validator, "m_VecIndexes" );

	ValidateObj( m_VecFKData );
	FOR_EACH_VEC( m_VecFKData, i )
	{
		ValidateObj( m_VecFKData[i] );
	}

	for ( int iIndex = 0; iIndex < m_VecIndexes.Count(); iIndex++ )
	{
		ValidateObj( m_VecIndexes[iIndex] );
	}
	ValidateObj( m_vecFTSFields );

	ValidateObj( m_VecColumnInfo );
	FOR_EACH_VEC( m_VecColumnInfo, nColumn )
	{
		CColumnInfo &columnInfo = GetColumnInfo( nColumn );
		ValidateObj( columnInfo );
	}
	ValidateObj( m_MapIColumnInfo );
}

void CColumnInfo::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

}

#endif // DBGFLAG_VALIDATE

} // namespace GCSDK
