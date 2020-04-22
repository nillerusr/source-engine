//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Sets of columns in SQL queries
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{

//-----------------------------------------------------------------------------
// Purpose: Constructs a column set with no columns in it
//-----------------------------------------------------------------------------
CColumnSet::CColumnSet( const CRecordInfo *pRecordInfo )
: m_pRecordInfo( pRecordInfo )
{
}


//-----------------------------------------------------------------------------
// Purpose: Constructs a column set with a single column in it
// Inputs:	nColumn - the column to add
//-----------------------------------------------------------------------------

CColumnSet::CColumnSet( const CRecordInfo *pRecordInfo, int col1 )
	: m_pRecordInfo( pRecordInfo )
{
	m_vecColumns.AddToTail( col1 );
}

CColumnSet::CColumnSet( const CRecordInfo *pRecordInfo, int col1, int col2 )
	: m_pRecordInfo( pRecordInfo )
{
	m_vecColumns.EnsureCapacity( 2 );
	m_vecColumns.AddToTail( col1 );
	m_vecColumns.AddToTail( col2 );
}

CColumnSet::CColumnSet( const CRecordInfo *pRecordInfo, int col1, int col2, int col3 )
	: m_pRecordInfo( pRecordInfo )
{
	m_vecColumns.EnsureCapacity( 3 );
	m_vecColumns.AddToTail( col1 );
	m_vecColumns.AddToTail( col2 );
	m_vecColumns.AddToTail( col3 );
}

CColumnSet::CColumnSet( const CRecordInfo *pRecordInfo, int col1, int col2, int col3, int col4 )
	: m_pRecordInfo( pRecordInfo )
{
	m_vecColumns.EnsureCapacity( 4 );
	m_vecColumns.AddToTail( col1 );
	m_vecColumns.AddToTail( col2 );
	m_vecColumns.AddToTail( col3 );
	m_vecColumns.AddToTail( col4 );
}

CColumnSet::CColumnSet( const CRecordInfo *pRecordInfo, int col1, int col2, int col3, int col4, int col5 )
	: m_pRecordInfo( pRecordInfo )
{
	m_vecColumns.EnsureCapacity( 5 );
	m_vecColumns.AddToTail( col1 );
	m_vecColumns.AddToTail( col2 );
	m_vecColumns.AddToTail( col3 );
	m_vecColumns.AddToTail( col4 );
	m_vecColumns.AddToTail( col5 );
}

CColumnSet::CColumnSet( const CRecordInfo *pRecordInfo, int col1, int col2, int col3, int col4, int col5, int col6 )
	: m_pRecordInfo( pRecordInfo )
{
	m_vecColumns.EnsureCapacity( 6 );
	m_vecColumns.AddToTail( col1 );
	m_vecColumns.AddToTail( col2 );
	m_vecColumns.AddToTail( col3 );
	m_vecColumns.AddToTail( col4 );
	m_vecColumns.AddToTail( col5 );
	m_vecColumns.AddToTail( col6 );
}

CColumnSet::CColumnSet( const CRecordInfo *pRecordInfo, int col1, int col2, int col3, int col4, int col5, int col6, int col7 )
	: m_pRecordInfo( pRecordInfo )
{
	m_vecColumns.EnsureCapacity( 7 );
	m_vecColumns.AddToTail( col1 );
	m_vecColumns.AddToTail( col2 );
	m_vecColumns.AddToTail( col3 );
	m_vecColumns.AddToTail( col4 );
	m_vecColumns.AddToTail( col5 );
	m_vecColumns.AddToTail( col6 );
	m_vecColumns.AddToTail( col7 );
}

CColumnSet::CColumnSet( const CRecordInfo *pRecordInfo, int col1, int col2, int col3, int col4, int col5, int col6, int col7, int col8 )
: m_pRecordInfo( pRecordInfo )
{
	m_vecColumns.EnsureCapacity( 8 );
	m_vecColumns.AddToTail( col1 );
	m_vecColumns.AddToTail( col2 );
	m_vecColumns.AddToTail( col3 );
	m_vecColumns.AddToTail( col4 );
	m_vecColumns.AddToTail( col5 );
	m_vecColumns.AddToTail( col6 );
	m_vecColumns.AddToTail( col7 );
	m_vecColumns.AddToTail( col8 );
}


//-----------------------------------------------------------------------------
// Purpose: Copy constructor
//-----------------------------------------------------------------------------
CColumnSet::CColumnSet( const CColumnSet & rhs )
{
	MEM_ALLOC_CREDIT_("CColumnSet");
	m_vecColumns.CopyArray( rhs.m_vecColumns.Base(), rhs.m_vecColumns.Count() );
	m_pRecordInfo = rhs.m_pRecordInfo;
}


//-----------------------------------------------------------------------------
// Purpose: Assignment operator
//-----------------------------------------------------------------------------
CColumnSet & CColumnSet::operator=( const CColumnSet & rhs )
{
	MEM_ALLOC_CREDIT_("CColumnSet");
	m_vecColumns.CopyArray( rhs.m_vecColumns.Base(), rhs.m_vecColumns.Count() );
	m_pRecordInfo = rhs.m_pRecordInfo;
	return *this;
}


//-----------------------------------------------------------------------------
// Purpose: Addition operator. lhs ColumnSet will be a union of the two
//	ColumnSets
//-----------------------------------------------------------------------------
CColumnSet & CColumnSet::operator+=( const CColumnSet & rhs )
{
	Assert( this->GetRecordInfo() == rhs.GetRecordInfo() );
	FOR_EACH_COLUMN_IN_SET( rhs, i )
	{
		BAddColumn( rhs.GetColumn( i ) );
	}

	return *this;
}

//-----------------------------------------------------------------------------
// Purpose: Addition operator. Returns a union of lhs and rhs
//-----------------------------------------------------------------------------
const CColumnSet CColumnSet::operator+( const CColumnSet & rhs ) const
{
	return CColumnSet( *this ) += rhs;
}


//-----------------------------------------------------------------------------
// Purpose: Adds a column to the set if it is 
// Inputs:	nColumn - THe column to add
//-----------------------------------------------------------------------------
void CColumnSet::BAddColumn( int nColumn )
{
	if( nColumn >= 0 && nColumn < m_pRecordInfo->GetNumColumns() )
	{
		//not sure best way to handle the 'is already set case'
		if( !IsSet( nColumn ) )
			m_vecColumns.AddToTail( nColumn );
	}
	else
	{
		AssertMsg3( false, "Attempting to set an out of range column on schema type %s, %d (of %d)", GetRecordInfo()->GetName(), nColumn, m_pRecordInfo->GetNumColumns() );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes a column from the set
// Inputs:	nColumn - THe column to remove
//-----------------------------------------------------------------------------
void CColumnSet::BRemoveColumn( int nColumn )
{
	m_vecColumns.FindAndRemove( nColumn );
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if a column is in the set
// Inputs:	nColumn - THe column to test
//-----------------------------------------------------------------------------
bool CColumnSet::IsSet( int nColumn ) const
{
	int nIndex = m_vecColumns.Find( nColumn );
	return m_vecColumns.IsValidIndex( nIndex );
}


//-----------------------------------------------------------------------------
// Purpose: Returns the number of columns in the set
//-----------------------------------------------------------------------------
uint32 CColumnSet::GetColumnCount() const
{
	return m_vecColumns.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Returns the column index of the Nth column in the set
// Inputs:	nIndex - the position in the set to return a column index for.
//-----------------------------------------------------------------------------
int CColumnSet::GetColumn( int nIndex ) const
{
	return m_vecColumns[nIndex];
}


//-----------------------------------------------------------------------------
// Purpose: Returns a CColumnInfo object for the nth column in the set
// Inputs:	nIndex - the position in the set to return a column info for.
//-----------------------------------------------------------------------------
const CColumnInfo & CColumnSet::GetColumnInfo( int nIndex ) const
{
	return m_pRecordInfo->GetColumnInfo( GetColumn( nIndex ) );
}


//-----------------------------------------------------------------------------
// Purpose: Empties the column set
//-----------------------------------------------------------------------------
void CColumnSet::MakeEmpty()
{
	m_vecColumns.RemoveAll();
}


//-----------------------------------------------------------------------------
// Purpose: Makes the column set be the full set of all columns in the record info
//-----------------------------------------------------------------------------
void CColumnSet::MakeFull()
{
	MakeEmpty();
	const int nNumColumns = m_pRecordInfo->GetNumColumns();
	m_vecColumns.EnsureCapacity( nNumColumns );
	for( int nColumn = 0; nColumn < m_pRecordInfo->GetNumColumns(); nColumn++ )
	{
		//do a direct add to avoid the exponential cost since we know we won't have conflicts
		m_vecColumns.AddToTail( nColumn );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Makes the column set be the full set of all insertable columns in 
//			the record info
//-----------------------------------------------------------------------------
void CColumnSet::MakeInsertable()
{
	MakeEmpty();
	for( int nColumn = 0; nColumn < m_pRecordInfo->GetNumColumns(); nColumn++ )
	{
		const CColumnInfo & columnInfo = m_pRecordInfo->GetColumnInfo( nColumn );
		if( columnInfo.BIsInsertable() )
		{
			//do a direct add to avoid the exponential cost since we know we won't have conflicts
			m_vecColumns.AddToTail( nColumn );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Makes the column set be the full set of all noninsertable columns in 
//			the record info
//-----------------------------------------------------------------------------
void CColumnSet::MakeNoninsertable()
{
	MakeEmpty();
	for( int nColumn = 0; nColumn < m_pRecordInfo->GetNumColumns(); nColumn++ )
	{
		const CColumnInfo & columnInfo = m_pRecordInfo->GetColumnInfo( nColumn );
		if( !columnInfo.BIsInsertable() )
		{
			//do a direct add to avoid the exponential cost since we know we won't have conflicts
			m_vecColumns.AddToTail( nColumn );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Makes the column set be the full set of all primary key columns in 
//			the record info
//-----------------------------------------------------------------------------
void CColumnSet::MakePrimaryKey()
{
	MakeEmpty();
	for( int nColumn = 0; nColumn < m_pRecordInfo->GetNumColumns(); nColumn++ )
	{
		const CColumnInfo & columnInfo = m_pRecordInfo->GetColumnInfo( nColumn );
		if( columnInfo.BIsPrimaryKey() )
		{
			//do a direct add to avoid the exponential cost since we know we won't have conflicts
			m_vecColumns.AddToTail( nColumn );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Makes the column set be the full set of all primary key columns in 
//			the record info
//-----------------------------------------------------------------------------
void CColumnSet::MakeInverse( const CColumnSet & columnSet )
{
	MakeEmpty();
	for( int nColumn = 0; nColumn < m_pRecordInfo->GetNumColumns(); nColumn++ )
	{
		if( !columnSet.IsSet( nColumn ) )
		{
			//do a direct add to avoid the exponential cost since we know we won't have conflicts
			m_vecColumns.AddToTail( nColumn );
		}
	}
}

//-----------------------------------------------------------------------------
// determines if the current column set has all fields set. Useful for detection of new columns being added to the schema
//-----------------------------------------------------------------------------
bool CColumnSet::BAreAllFieldsSet() const
{
	for( int nColumn = 0; nColumn < m_pRecordInfo->GetNumColumns(); nColumn++ )
	{
		if( !IsSet( nColumn ) )
			return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a Column Set which is the inverse of the given column set
// STATIC - Difference from MakeInverse is that it has a return value
//-----------------------------------------------------------------------------
CColumnSet CColumnSet::Inverse( const CColumnSet & columnSet )
{
	CColumnSet set( columnSet.GetRecordInfo() );
	set.MakeInverse( columnSet );
	return set;
}

//-----------------------------------------------------------------------------
// Purpose: Claims the memory for CColumnSet
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CColumnSet::Validate( CValidator &validator, const char *pchName )
{
	// these are INSIDE the function instead of outside so the interface 
	// doesn't change
	VALIDATE_SCOPE();

	ValidateObj( m_vecColumns );
}
#endif


} // namespace GCSDK
