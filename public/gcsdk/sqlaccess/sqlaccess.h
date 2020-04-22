//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Provides access to SQL at a high level
//
//=============================================================================

#ifndef SQLACCESS_H
#define SQLACCESS_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/gcsqlquery.h"
#include <functional>

#include "tier0/memdbgon.h"

namespace GCSDK
{
class CGCSQLQuery;
class CGCSQLQueryGroup;
class CColumnSet;
class CRecordType;

//-----------------------------------------------------------------------------
// Purpose: Provides access to SQL at a high level
//-----------------------------------------------------------------------------
class CSQLAccess
{
public:
	CSQLAccess( ESchemaCatalog eSchemaCatalog = k_ESchemaCatalogMain );
	~CSQLAccess( );

	bool BBeginTransaction( const char *pchName );
	MUST_CHECK_RETURN bool BCommitTransaction( bool bAllowEmpty = false );
	void RollbackTransaction();
	bool BInTransaction( ) const { return m_bInTransaction; }
	const char *PchTransactionName( ) const;

	// Add a listener to be executed should this transaction successfully complete.  Invalid to call if
	// !BInTransaction()
	//
	// NOTE: Listeners must assume they are within a transaction commit scope with relevant locks, and must not yield or
	//       perform re-entrant actions.
	void AddCommitListener( std::function<void (void)> &&listener );

	// Add a listener to be executed should this transaction be rolled back, explicitly or automatically.  Invalid to
	// call if !BInTransaction()
	//
	// NOTE: Listeners must assume they are within a transaction commit scope with relevant locks, and must not yield or
	//       perform re-entrant actions.
	void AddRollbackListener( std::function<void (void)> &&listener );

	bool BYieldingExecute( const char *pchName, const char *pchSQLCommand, uint32 *pcRowsAffected = NULL, bool bSpewOnError = true );
	bool BYieldingExecuteString( const char *pchName, const char *pchSQLCommand, CFmtStr1024 *psResult, uint32 *pcRowsAffected = NULL );
	bool BYieldingExecuteScalarInt( const char *pchName, const char *pchSQLCommand, int *pnResult, uint32 *pcRowsAffected = NULL );
	bool BYieldingExecuteScalarIntWithDefault( const char *pchName, const char *pchSQLCommand, int *pnResult, int iDefaultValue, uint32 *pcRowsAffected = NULL );
	bool BYieldingExecuteScalarUint32( const char *pchName, const char *pchSQLCommand, uint32 *punResult, uint32 *pcRowsAffected = NULL );
	bool BYieldingExecuteScalarUint32WithDefault( const char *pchName, const char *pchSQLCommand, uint32 *punResult, uint32 unDefaultValue, uint32 *pcRowsAffected = NULL );
	bool BYieldingWipeTable( int iTable );

	template <typename TReturn, typename TCast>
	bool BYieldingExecuteSingleResult( const char *pchName, const char *pchSQLCommand, EGCSQLType eType, TReturn *pResult, uint32 *pcRowsAffected );
	template <typename TReturn, typename TCast>
	bool BYieldingExecuteSingleResultWithDefault( const char *pchName, const char *pchSQLCommand, EGCSQLType eType, TReturn *pResult, TReturn defaultValue, uint32 *pcRowsAffected );

	// manipulating CRecordBase (i.e. CSch...) objects in the database
	bool BYieldingInsertRecord( const CRecordBase *pRecordBase );
	bool BYieldingInsertWhenNotMatchedOnPK( CRecordBase *pRecordBase );
	bool BYieldingInsertOrUpdateOnPK( CRecordBase *pRecordBase );
	bool BYieldingInsertWithIdentity( CRecordBase* pRecordBase );

	/// Locate a *single* record from the table which matches the values of the specified columns.
	/// Returns:
	/// - k_EResultOK - A single record was found and returned
	/// - k_EResultNoMatch - no records found
	/// - k_EResultLimitExceeded - More than one record found (false is returned in this case --- use a different function to do the query)
	/// - (Other K_EResult codes might be returned for invalid usage, SQL problem, etc)
	/// If no order by clause is provided, this assumes a singleton will be returned, and will fail if more than one record meets the criteria, if an order by
	///   clause is provided though, this will just return the first element
	EResult YieldingReadRecordWithWhereColumns( CRecordBase *pRecord, const CColumnSet & readSet, const CColumnSet & whereSet, const char* pchOrderClause = NULL );

	template< typename SchClass_t>
	bool BYieldingReadRecordsWithWhereClause( CUtlVector< SchClass_t > *pvecRecords, const char *pchWhereClause, const CColumnSet & readSet, const char *pchTopClause = NULL, const char* pchOrderClause = NULL );
	template< typename SchClass_t>
	bool BYieldingReadRecordsWithQuery( CUtlVector< SchClass_t > *pvecRecords, const char *sQuery, const CColumnSet & readSet );
	bool BYieldingUpdateRecord( const CRecordBase & record, const CColumnSet & whereColumns, const CColumnSet & updateColumns, const class CSQLOutputParams *pOptionalOutputParams = NULL );
	bool BYieldingUpdateRecords( const CRecordBase & whereRecord, const CColumnSet & whereColumns, const CRecordBase & updateRecord, const CColumnSet & updateColumns, const class CSQLOutputParams *pOptionalOutputParams = NULL );
	bool BYieldingDeleteRecords( const CRecordBase & record, const CColumnSet & whereColumns );

	//called to handle an update or insert request (an upsert), where if the record exists, it will update the masked fields, otherwise it will insert the full record. All fields should
	//be initialized within this record in case of an insert. Note that all of the values will be loaded into a structure named 'Src', so you can reference in the where/update as Src.Column1, etc
	bool BYieldingUpdateOrInsert( const CRecordBase& record, const CColumnSet & whereColumns, const CColumnSet & updateColumns, const char* pszWhereClause = NULL, const char* pszUpdateClause = NULL );

	void AddRecordParameters( const CRecordBase &record, const CColumnSet & columnSet );

	void AddBindParam( const char *pchValue );
	void AddBindParam( const int16 nValue );
	void AddBindParam( const uint16 uValue );
	void AddBindParam( const int32 nValue );
	void AddBindParam( const uint32 uValue );
	void AddBindParam( const uint64 ulValue );
	void AddBindParam( const uint8 *ubValue, const int cubValue );
	void AddBindParam( const float fValue );
	void AddBindParam( const double dValue );
	void AddBindParamRaw( EGCSQLType eType, const byte *pubData, uint32 cubData );
	void ClearParams();
	IGCSQLResultSetList *GetResults();

	uint32 GetResultSetCount();
	uint32 GetResultSetRowCount( uint32 unResultSet );
	CSQLRecord GetResultRecord( uint32 unResultSet, uint32 unRow );

private:
	enum EReadSingleResultResult
	{
		eReadSingle_Error,			// something went wrong in the DB or the data was in a format we didn't expect
		eReadSingle_ResultFound,	// we found a single result and copied the value -- all is well!
		eReadSingle_UseDefault,		// we didn't find any results but we specified a value in advance for this case
	};

	EReadSingleResultResult BYieldingExecuteSingleResultDataInternal( const char *pchName, const char *pchSQLCommand, EGCSQLType eType, uint8 **pubData, uint32 *punSize, uint32 *pcRowsAffected, bool bHasDefaultValue );

private:
	void RunListeners_Commit();
	void RunListeners_Rollback();
	CGCSQLQuery *CurrentQuery();
	ESchemaCatalog m_eSchemaCatalog;
	CGCSQLQuery *m_pCurrentQuery;
	CGCSQLQueryGroup *m_pQueryGroup;
	bool m_bInTransaction;
	std::vector< std::function<void (void)> > m_vecCommitListeners;
	std::vector< std::function<void (void)> > m_vecRollbackListeners;
};

//this class will build up and cache an update or insert query. These can be pretty long and expensive to generate, so the intent of this object is to make a static version which can then
//be submitted to various SQL operations
class CSQLUpdateOrInsert
{
public:
	//builds up a query that will insert or merge. The optional clauses can be provided, and if need to reference existing tables, S=Source (new one being added) D=Destination (table being updated)
	//NOTE: You cannot use ?'s in the clauses since the parameters are reset and filled in with the provided object
	//NOTE: update columns can be empty along with a NULL update clause to simply do an 'insert if it doesn't exist' command
	CSQLUpdateOrInsert( const char* pszName, int nTable, const CColumnSet & whereColumns, const CColumnSet & updateColumns, const char* pszWhereClause = NULL, const char* pszUpdateClause = NULL );

	//called to execute the query on the provided record. Note that this will clear any previously set parameters for the query so it can fill in the provided record
	bool BYieldingExecute( CSQLAccess& sqlAccess, const CRecordBase& record, uint32 *out_punRowsAffected = NULL ) const;

private:
	//the display name for our query
	CUtlString	m_sName;
	//the formatted query to run
	CUtlString	m_sQuery;
	//which table we were compiled against to ensure we aren't used on the wrong objects
	int			m_nTable;
};

#define FOR_EACH_SQL_RESULT( sqlAccess, resultSet, record ) \
	for( CSQLRecord record = (sqlAccess).GetResultRecord( resultSet, 0 ); record.IsValid(); record.NextRow() )

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSQLOutputParams
{
public:
	CSQLOutputParams( const CRecordBase *pRecord, const CColumnSet& ColumnSet )
		: m_pRecord( pRecord )
		, m_ColumnSet( ColumnSet )
	{
		Assert( pRecord );
		Assert( pRecord->GetPRecordInfo() == ColumnSet.GetRecordInfo() );
	}
	
	const CRecordBase& GetRecord() const { return *m_pRecord; }
	const CColumnSet& GetColumnSet() const { return m_ColumnSet; }

private:
	const CRecordBase *m_pRecord;
	const CColumnSet& m_ColumnSet;
};

//-----------------------------------------------------------------------------
// Purpose: templatized version of querying for a single value
//-----------------------------------------------------------------------------
template <typename TReturn, typename TCast>
bool CSQLAccess::BYieldingExecuteSingleResult( const char *pchName, const char *pchSQLCommand, EGCSQLType eType, TReturn *pResult, uint32 *pcRowsAffected )
{
	uint8 *pubData;
	uint32 cubData;
	if( CSQLAccess::BYieldingExecuteSingleResultDataInternal( pchName, pchSQLCommand, eType, &pubData, &cubData, pcRowsAffected, false ) != eReadSingle_ResultFound )
		return false;

	*pResult = *( (TCast *)pubData );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: templatized version of querying for a single value
//-----------------------------------------------------------------------------
template <typename TReturn, typename TCast>
bool CSQLAccess::BYieldingExecuteSingleResultWithDefault( const char *pchName, const char *pchSQLCommand, EGCSQLType eType, TReturn *pResult, TReturn defaultValue, uint32 *pcRowsAffected )
{
	uint8 *pubData;
	uint32 cubData;
	EReadSingleResultResult eResult = CSQLAccess::BYieldingExecuteSingleResultDataInternal( pchName, pchSQLCommand, eType, &pubData, &cubData, pcRowsAffected, true );

	if ( eResult == eReadSingle_Error )
		return false;
	
	if ( eResult == eReadSingle_ResultFound )
	{
		*pResult = *( (TCast *)pubData );
	}
	else
	{
		Assert( eResult == eReadSingle_UseDefault );
		*pResult = defaultValue;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Reads a list of records from the DB according to the specified where
//			clause
// Input:	pRecordBase - record to insert
// Output:	true if successful, false otherwise
//-----------------------------------------------------------------------------
template< typename SchClass_t>
bool CSQLAccess::BYieldingReadRecordsWithWhereClause( CUtlVector< SchClass_t > *pvecRecords, const char *pchWhereClause, const CColumnSet & readSet, const char *pchTopClause, const char* pchOrderClause )
{
	AssertMsg( !BInTransaction(),  "BYieldingReadRecordsWithWhereClause is not supported in a transaction" );
	if( BInTransaction() )
		return false;

	Assert( !readSet.IsEmpty() );
	TSQLCmdStr sStatement;
	BuildSelectStatementText( &sStatement, readSet, pchTopClause );

	//finished our select, so add our where filter if provided
	if( pchWhereClause )
	{
		sStatement.Append( " WHERE " );
		sStatement.Append( pchWhereClause );
	}
	//finished our where, so add our order by clause if provided
	if( pchOrderClause )
	{
		sStatement.Append( " ORDER BY " );
		sStatement.Append( pchOrderClause );
	}

	return BYieldingReadRecordsWithQuery< SchClass_t >( pvecRecords, sStatement, readSet );
}


//-----------------------------------------------------------------------------
// Purpose: Inserts a new record into the DB and reads non-insertable fields back
//			into the record.
// Input:	pRecordBase - record to insert
// Output:	true if successful, false otherwise
//-----------------------------------------------------------------------------
template< typename SchClass_t>
bool CSQLAccess::BYieldingReadRecordsWithQuery( CUtlVector< SchClass_t > *pvecRecords, const char *sQuery, const CColumnSet & readSet )
{
	AssertMsg( !BInTransaction(),  "BYieldingReadRecordsWithQuery is not supported in a transaction" );
	if( BInTransaction() )
		return false;

	Assert(!readSet.IsEmpty() );
	if( !BYieldingExecute( NULL, sQuery ) )
		return false;

	Assert( GetResultSetCount() == 1 );
	if ( GetResultSetCount() != 1 )
		return false;

	// make sure the types are the same
	IGCSQLResultSet *pResultSet = m_pQueryGroup->GetResults()->GetResultSet( 0 );
	return CopyResultToSchVector( pResultSet, readSet, pvecRecords );
}

} // namespace GCSDK

#include "tier0/memdbgoff.h"

#endif // SQLACCESS_H
