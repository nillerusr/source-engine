//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Sets of columns in queries
//
// $NoKeywords: $
//=============================================================================

#ifndef COLUMNSET_H
#define COLUMNSET_H

#ifdef _WIN32
#pragma once
#endif

namespace GCSDK
{

//-----------------------------------------------------------------------------
// Purpose: Sets of columns in queries
//-----------------------------------------------------------------------------
class CColumnSet
{
public:
	CColumnSet( const CRecordInfo *pRecordInfo );
	CColumnSet( const CColumnSet & rhs );
	CColumnSet & operator=( const CColumnSet & rhs );
	const CColumnSet operator+( const CColumnSet & rhs ) const;
	CColumnSet & operator+=( const CColumnSet & rhs );

	//NOTE: These do not ensure uniqueness, the CSET_ macros below should be used instead as they will compile time enforce uniqueness
	CColumnSet( const CRecordInfo *pRecordInfo, int col1 );
	CColumnSet( const CRecordInfo *pRecordInfo, int col1, int col2 );
	CColumnSet( const CRecordInfo *pRecordInfo, int col1, int col2, int col3 );
	CColumnSet( const CRecordInfo *pRecordInfo, int col1, int col2, int col3, int col4 );
	CColumnSet( const CRecordInfo *pRecordInfo, int col1, int col2, int col3, int col4, int col5 );
	CColumnSet( const CRecordInfo *pRecordInfo, int col1, int col2, int col3, int col4, int col5, int col6 );
	CColumnSet( const CRecordInfo *pRecordInfo, int col1, int col2, int col3, int col4, int col5, int col6, int col7 );
	CColumnSet( const CRecordInfo *pRecordInfo, int col1, int col2, int col3, int col4, int col5, int col6, int col7, int col8 );

	void BAddColumn( int nColumn );
	void BRemoveColumn( int nColumn );
	bool IsSet( int nColumn ) const;
	bool IsEmpty() const { return m_vecColumns.Count() ==  0;}

	uint32 GetColumnCount() const;
	int GetColumn( int nIndex ) const;
	const CColumnInfo & GetColumnInfo( int nIndex ) const;

	const CRecordInfo *GetRecordInfo() const { return m_pRecordInfo; }

	void MakeEmpty();
	void MakeFull();
	void MakeInsertable();
	void MakeNoninsertable();
	void MakePrimaryKey();
	void MakeInverse( const CColumnSet & columnSet );
	//determines if the current column set has all fields set. Useful for detection of new columns being added to the schema
	bool BAreAllFieldsSet() const;

	template< typename TSchClass >
	static CColumnSet Empty();
	template< typename TSchClass >
	static CColumnSet Full();
	template< typename TSchClass >
	static CColumnSet Insertable();
	template< typename TSchClass >
	static CColumnSet Noninsertable();
	template< typename TSchClass >
	static CColumnSet PrimaryKey();
	static CColumnSet Inverse( const CColumnSet & columnSet );

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
#endif

private:
	CUtlVector<int> m_vecColumns;
	const CRecordInfo *m_pRecordInfo;
};

//this is a utility class which can at compile time ensure that all of the provided column values are unique and will generate an error if that doesn't
//hold true. The default values just need to be unique and greater than what is expected for real columns
template < int n1 = 10001, int n2 = 10002, int n3 = 10003, int n4 = 10004, int n5 = 10005, int n6 = 10006, int n7 = 10007, int n8 = 10008 >
class CUniqueColChecker
{
public:

	//this is a simple pass through so that it can wrap the declaration of a column set
	static const CColumnSet& VerifyUnique( const CColumnSet& cs )
	{
		COMPILE_TIME_ASSERT( n1 != n2 && n1 != n3 && n1 != n4 && n1 != n5 && n1 != n6 && n1 != n7 && n1 != n8 );
		COMPILE_TIME_ASSERT( n2 != n3 && n2 != n4 && n2 != n5 && n2 != n6 && n2 != n7 && n2 != n8 );
		COMPILE_TIME_ASSERT( n3 != n4 && n3 != n5 && n3 != n6 && n3 != n7 && n3 != n8 );
		COMPILE_TIME_ASSERT( n4 != n5 && n4 != n6 && n4 != n7 && n4 != n8 );
		COMPILE_TIME_ASSERT( n5 != n6 && n5 != n7 && n5 != n8 );
		COMPILE_TIME_ASSERT( n6 != n7 && n6 != n8 );
		COMPILE_TIME_ASSERT( n7 != n8 );
		return cs;
	}
};

// Usage notes:
//		The fields in a column set are order-dependent, and must match the order of the fields in
//		the query used to generate the data. The code that reads values doesn't do any fancy
//		name-matching and will copy values to incorrect locations silently if there is a
//		disagreement between the fields in the query and the fields in the column set.
//
// Examples:
//		// This is broken.
//		query	  = "SELECT * FROM Items";
//		columnSet = CSET_12_COL( CSchItem, individual_field_names );
//
//		// This is fixed.
//		query	  = "SELECT * FROM Items";
//		columnSet = CSET_FULL( ... );

#define FOR_EACH_COLUMN_IN_SET( columnSet, iterName )		for( uint32 iterName = 0; iterName < (columnSet).GetColumnCount(); iterName++ )

#define CSET_EMPTY( schClass ) CColumnSet::Empty<schClass>()
#define CSET_FULL( schClass ) CColumnSet::Full<schClass>()
#define CSET_INSERTABLE( schClass ) CColumnSet::Insertable<schClass>()
#define CSET_NONINSERTABLE( schClass ) CColumnSet::Noninsertable<schClass>()
#define CSET_PK( schClass ) CColumnSet::PrimaryKey<schClass>()

#define CSET_1_COL( schClass, col1 ) \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1 )

#define CSET_2_COL( schClass, col1, col2 ) \
	CUniqueColChecker< schClass::col1, schClass::col2 >::VerifyUnique( \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1, schClass::col2 ) )

#define CSET_3_COL( schClass, col1, col2, col3 ) \
	CUniqueColChecker< schClass::col1, schClass::col2, schClass::col3 >::VerifyUnique( \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1, schClass::col2, schClass::col3 ) )

#define CSET_4_COL( schClass, col1, col2, col3, col4 ) \
	CUniqueColChecker< schClass::col1, schClass::col2, schClass::col3, schClass::col4 >::VerifyUnique( \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1, schClass::col2, schClass::col3, schClass::col4 ) )

#define CSET_5_COL( schClass, col1, col2, col3, col4, col5 ) \
	CUniqueColChecker< schClass::col1, schClass::col2, schClass::col3, schClass::col4, schClass::col5 >::VerifyUnique( \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1, schClass::col2, schClass::col3, schClass::col4, schClass::col5 ) )

#define CSET_6_COL( schClass, col1, col2, col3, col4, col5, col6 ) \
	CUniqueColChecker< schClass::col1, schClass::col2, schClass::col3, schClass::col4, schClass::col5, schClass::col6 >::VerifyUnique( \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1, schClass::col2, schClass::col3, schClass::col4, schClass::col5, schClass::col6 ) )

#define CSET_7_COL( schClass, col1, col2, col3, col4, col5, col6, col7 ) \
	CUniqueColChecker< schClass::col1, schClass::col2, schClass::col3, schClass::col4, schClass::col5, schClass::col6, schClass::col7 >::VerifyUnique( \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1, schClass::col2, schClass::col3, schClass::col4, schClass::col5, schClass::col6, schClass::col7 ) )

#define CSET_8_COL( schClass, col1, col2, col3, col4, col5, col6, col7, col8 ) \
	CUniqueColChecker< schClass::col1, schClass::col2, schClass::col3, schClass::col4, schClass::col5, schClass::col6, schClass::col7, schClass::col8 >::VerifyUnique( \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1, schClass::col2, schClass::col3, schClass::col4, schClass::col5, schClass::col6, schClass::col7, schClass::col8 ) )


//-----------------------------------------------------------------------------
// Purpose: Returns an empty Column Set for a schema class
//-----------------------------------------------------------------------------
template< typename TSchClass >
CColumnSet CColumnSet::Empty()
{
	CColumnSet set( GSchemaFull().GetSchema( TSchClass::k_iTable ).GetRecordInfo() );
	return set;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a Column Set for a schema class which contains every field
//-----------------------------------------------------------------------------
template< typename TSchClass >
CColumnSet CColumnSet::Full()
{
	CColumnSet set( GSchemaFull().GetSchema( TSchClass::k_iTable ).GetRecordInfo() );
	set.MakeFull();
	return set;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a Column Set for a schema class which contains every 
//			insertable field
//-----------------------------------------------------------------------------
template< typename TSchClass >
CColumnSet CColumnSet::Insertable()
{
	CColumnSet set( GSchemaFull().GetSchema( TSchClass::k_iTable ).GetRecordInfo() );
	set.MakeInsertable();
	return set;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a Column Set for a schema class which contains every 
//			noninsertable field
//-----------------------------------------------------------------------------
template< typename TSchClass >
CColumnSet CColumnSet::Noninsertable()
{
	CColumnSet set( GSchemaFull().GetSchema( TSchClass::k_iTable ).GetRecordInfo() );
	set.MakeNoninsertable();
	return set;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a Column Set for a schema class which contains every 
//			primary key field
//-----------------------------------------------------------------------------
template< typename TSchClass >
CColumnSet CColumnSet::PrimaryKey()
{
	CColumnSet set( GSchemaFull().GetSchema( TSchClass::k_iTable ).GetRecordInfo() );
	set.MakePrimaryKey();
	return set;
}

} // namespace GCSDK
#endif // COLUMNSET_H
