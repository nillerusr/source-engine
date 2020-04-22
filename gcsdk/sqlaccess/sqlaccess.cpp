//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Provides access to SQL at a high level
//
//=============================================================================

#include "stdafx.h"
#include "gcsdk/sqlaccess/sqlaccess.h"
#include "gcsdk/gcsqlquery.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

template< typename LISTENER_FUNC >
static void RunAndClearListenerList( std::vector< LISTENER_FUNC > &vecListeners )
{
	// Let us not underestimate the ability of random listeners to re-enter everything.
	std::vector< LISTENER_FUNC > listenerCopy;
	listenerCopy.swap( vecListeners );
	vecListeners.clear();

	// Why would you consider such a thing
	DO_NOT_YIELD_THIS_SCOPE();

	for ( const auto &listener : listenerCopy )
	{
		listener();
	}
}


namespace GCSDK
{
//------------------------------------------------------------------------------------
// Purpose: Constructor
//------------------------------------------------------------------------------------
CSQLAccess::CSQLAccess( ESchemaCatalog eSchemaCatalog )
	: m_eSchemaCatalog( eSchemaCatalog)
	, m_pCurrentQuery( NULL )
	, m_bInTransaction( false )
{
	m_pQueryGroup = CGCSQLQueryGroup::Alloc();
}


//------------------------------------------------------------------------------------
// Purpose: Destructor
//------------------------------------------------------------------------------------
CSQLAccess::~CSQLAccess( )
{
	SAFE_RELEASE( m_pQueryGroup );
	Assert( !m_pCurrentQuery );
	SAFE_DELETE( m_pCurrentQuery );
	AssertMsg( !m_bInTransaction, "GCSDK::CSQLAccess object being destroyed with a transaction pending. Use BCommitTransaction or RollbackTransaction to match your BBeginTransaction call." );
}


//------------------------------------------------------------------------------------
// Purpose: Perform a query 
//------------------------------------------------------------------------------------
bool CSQLAccess::BYieldingExecute( const char *pchName, const char *pchSQLCommand, uint32 *pcRowsAffected, bool bSpewOnError )
{
	if ( NULL == pchName )
	{
		pchName = pchSQLCommand;
	}

	bool bStandalone = !BInTransaction();
	if( bStandalone )
	{
		BBeginTransaction( pchName );
	}

	CurrentQuery()->SetCommand( pchSQLCommand );
	m_pQueryGroup->AddQuery( m_pCurrentQuery );
	m_pCurrentQuery = NULL;

	bool bSuccess = true;
	if( bStandalone )
	{
		bSuccess = BCommitTransaction();
		if( bSuccess && pcRowsAffected )
		{
			*pcRowsAffected = m_pQueryGroup->GetResults()->GetRowsAffected( 0 );
		}
	}
	return bSuccess;
}


//------------------------------------------------------------------------------------
// Purpose: Starts a transaction 
//------------------------------------------------------------------------------------
bool CSQLAccess::BBeginTransaction( const char *pchName )
{
	Assert( !m_bInTransaction );
	if( m_bInTransaction )
		return false;
	m_pQueryGroup->Clear();
	m_pQueryGroup->SetName( pchName );
	m_bInTransaction = true;
	return true;
}

//------------------------------------------------------------------------------------
// Purpose: Returns the string last passed to BBeginTransaction
//------------------------------------------------------------------------------------
const char *CSQLAccess::PchTransactionName( ) const
{
	return m_pQueryGroup->PchName();
}


//------------------------------------------------------------------------------------
// Purpose: Commits a transaction to the database
//------------------------------------------------------------------------------------
bool CSQLAccess::BCommitTransaction( bool bAllowEmpty )
{
	Assert( BInTransaction() );
	if( !BInTransaction() )
		return false;

	if( !m_pCurrentQuery && !m_pQueryGroup->GetStatementCount() )
	{
		if( bAllowEmpty )
		{
			// No-op success
			m_bInTransaction = false;
			RunListeners_Commit();
			return true;
		}
		else
		{
			AssertMsg1( false, "BCommitTransaction with empty transaction at %s", m_pQueryGroup->PchName() );
			return false;
		}
	}

	AssertMsg1( !m_pCurrentQuery, "Unexecuted query present in BCommitTransaction: %s", m_pCurrentQuery->PchCommand() );
	if( m_pCurrentQuery )
		return false;

	m_bInTransaction = false;

	if( !GJobCur().BYieldingRunQuery( m_pQueryGroup, m_eSchemaCatalog ) )
	{
		// Notify listeners that the transaction did not succeed
		RunListeners_Rollback();
		return false;
	}

	// The transaction presumably did make the database, so we do not notify rollback listeners beyond here.
	RunListeners_Commit();

	if( !m_pQueryGroup->GetResults() )
		return false;

	return true;
}


//------------------------------------------------------------------------------------
// Purpose: Rolls back a transaction and clears any queries
//------------------------------------------------------------------------------------
void CSQLAccess::RollbackTransaction()
{
	bool bWasTransaction = BInTransaction();

	Assert( bWasTransaction );
	SAFE_DELETE( m_pCurrentQuery );
	m_bInTransaction = false;

	if ( bWasTransaction )
	{
		RunListeners_Rollback();
	}
	else
	{
		m_vecCommitListeners.clear();
		m_vecRollbackListeners.clear();
	}
}

//------------------------------------------------------------------------------------
// Purpose: Adds a listener to be called synchronously should the transaction successfully commit
//------------------------------------------------------------------------------------
void CSQLAccess::AddCommitListener( std::function<void (void)> &&listener )
{
	if ( !BInTransaction() )
	{
		AssertMsg( BInTransaction(), "Adding a listener to a non-transaction access, will never fire" );
		return;
	}

	m_vecCommitListeners.push_back( std::move( listener ) );
}

//------------------------------------------------------------------------------------
// Purpose: Adds a listener to be called synchronously should the transaction fail or explicitly rollback
//------------------------------------------------------------------------------------
void CSQLAccess::AddRollbackListener( std::function<void (void)> &&listener )
{
	if ( !BInTransaction() )
	{
		AssertMsg( BInTransaction(), "Adding a listener to a non-transaction access, will never fire" );
		return;
	}

	m_vecRollbackListeners.push_back( std::move( listener ) );
}

//------------------------------------------------------------------------------------
// Purpose: Notifies listeners of successful commit.
//------------------------------------------------------------------------------------
void CSQLAccess::RunListeners_Commit()
{
	RunAndClearListenerList( m_vecCommitListeners );
	// Clear the unused set
	m_vecRollbackListeners.clear();
}

//------------------------------------------------------------------------------------
// Purpose: Notifies listeners of a implicitly or explicitly rolled back transactions and clears the listener list.
//------------------------------------------------------------------------------------
void CSQLAccess::RunListeners_Rollback()
{
	RunAndClearListenerList( m_vecRollbackListeners );
	// Clear the unused set
	m_vecCommitListeners.clear();
}

//------------------------------------------------------------------------------------
// Purpose: Perform a query that returns a single string
//------------------------------------------------------------------------------------
CSQLAccess::EReadSingleResultResult CSQLAccess::BYieldingExecuteSingleResultDataInternal( const char *pchName, const char *pchSQLCommand, EGCSQLType eType, uint8 **ppubData, uint32 *punSize, uint32 *pcRowsAffected, bool bHasDefaultValue )
{
	AssertMsg( !BInTransaction(),  "BYieldingExecuteSingleResultData is not supported in a transaction" );
	if( BInTransaction() )
		return eReadSingle_Error;

	bool bRet = BYieldingExecute( pchName, pchSQLCommand, pcRowsAffected );
	if ( !bRet )
		return eReadSingle_Error;

	if( m_pQueryGroup->GetResults()->GetResultSetCount() != 1 )
	{
		AssertMsg1( false, "Expected single result set, found %d", m_pQueryGroup->GetResults()->GetResultSetCount() );
		return eReadSingle_Error;
	}

	IGCSQLResultSet *pResultSet = m_pQueryGroup->GetResults()->GetResultSet( 0 );

	// If we have a default value, getting back zero rows is acceptable.
	if( pResultSet->GetRowCount() == 0 && bHasDefaultValue )
	{
		return eReadSingle_UseDefault;
	}

	// If we either have more than one row or no default value specified, that's an error.
	if( pResultSet->GetRowCount() != 1 )
	{
		AssertMsg1( false, "Expected single result, found %d", pResultSet->GetRowCount() );
		return eReadSingle_Error;
	}

	if( pResultSet->GetColumnCount() != 1 )
	{
		AssertMsg1( false, "Expected single column, found %d", pResultSet->GetColumnCount() );
		return eReadSingle_Error;
	}
	if( pResultSet->GetColumnType( 0 ) != eType )
	{
		AssertMsg2( false, "Expected column of type %s, found %s", PchNameFromEGCSQLType( eType ), PchNameFromEGCSQLType( pResultSet->GetColumnType( 0 ) ) );
		return eReadSingle_Error;
	}

	return pResultSet->GetData( 0, 0, ppubData, punSize )
		 ? eReadSingle_ResultFound
		 : eReadSingle_Error;
}




//------------------------------------------------------------------------------------
// Purpose: Perform a query that returns a single string
//------------------------------------------------------------------------------------
bool CSQLAccess::BYieldingExecuteString( const char *pchName, const char *pchSQLCommand, CFmtStr1024 *psResult, uint32 *pcRowsAffected )
{
	uint8 *pubData;
	uint32 cubData;
	if( CSQLAccess::BYieldingExecuteSingleResultDataInternal( pchName, pchSQLCommand, k_EGCSQLType_String, &pubData, &cubData, pcRowsAffected, false ) != eReadSingle_ResultFound )
		return false;

	*psResult = (char *)pubData;

	return true;
}

//------------------------------------------------------------------------------------
// Purpose: Perform a query that returns a single int
//------------------------------------------------------------------------------------
bool CSQLAccess::BYieldingExecuteScalarInt( const char *pchName, const char *pchSQLCommand, int *pnResult, uint32 *pcRowsAffected )
{
	return BYieldingExecuteSingleResult<int32, uint32>( pchName, pchSQLCommand, k_EGCSQLType_int32, pnResult, pcRowsAffected );
}

bool CSQLAccess::BYieldingExecuteScalarIntWithDefault( const char *pchName, const char *pchSQLCommand, int *pnResult, int iDefaultValue, uint32 *pcRowsAffected )
{
	return BYieldingExecuteSingleResultWithDefault<int32, uint32>( pchName, pchSQLCommand, k_EGCSQLType_int32, pnResult, iDefaultValue, pcRowsAffected );
}

//------------------------------------------------------------------------------------
// Purpose: Perform a query that returns a single uint32
//------------------------------------------------------------------------------------
bool CSQLAccess::BYieldingExecuteScalarUint32( const char *pchName, const char *pchSQLCommand, uint32 *punResult, uint32 *pcRowsAffected )
{
	return BYieldingExecuteSingleResult<uint32, uint32>( pchName, pchSQLCommand, k_EGCSQLType_int32, punResult, pcRowsAffected );
}

bool CSQLAccess::BYieldingExecuteScalarUint32WithDefault( const char *pchName, const char *pchSQLCommand, uint32 *punResult, uint32 unDefaultValue, uint32 *pcRowsAffected )
{
	return BYieldingExecuteSingleResultWithDefault<uint32, uint32>( pchName, pchSQLCommand, k_EGCSQLType_int32, punResult, unDefaultValue, pcRowsAffected );
}

//------------------------------------------------------------------------------------
// Purpose: A bunch of pass throughs to the query itself
//------------------------------------------------------------------------------------
void CSQLAccess::AddBindParam( const char *pchValue )
{
	CurrentQuery()->AddBindParam( pchValue );
}

void CSQLAccess::AddBindParam( const int16 nValue )
{
	CurrentQuery()->AddBindParam( nValue );
}

void CSQLAccess::AddBindParam( const uint16 uValue )
{
	CurrentQuery()->AddBindParam( uValue );
}

void CSQLAccess::AddBindParam( const int32 nValue )
{
	CurrentQuery()->AddBindParam( nValue );
}

void CSQLAccess::AddBindParam( const uint32 uValue )
{
	CurrentQuery()->AddBindParam( uValue );
}

void CSQLAccess::AddBindParam( const uint64 ulValue )
{
	CurrentQuery()->AddBindParam( ulValue );
}

void CSQLAccess::AddBindParam( const uint8 *ubValue, const int cubValue )
{
	CurrentQuery()->AddBindParam( ubValue, cubValue );
}

void CSQLAccess::AddBindParam( const float fValue )
{
	CurrentQuery()->AddBindParam( fValue );
}

void CSQLAccess::AddBindParam( const double dValue )
{
	CurrentQuery()->AddBindParam( dValue );
}

void CSQLAccess::AddBindParamRaw( EGCSQLType eType, const byte *pubData, uint32 cubData )
{
	CurrentQuery()->AddBindParamRaw( eType, pubData, cubData );
}

void CSQLAccess::ClearParams()
{
	if( m_pCurrentQuery )
	{
		delete m_pCurrentQuery;
		m_pCurrentQuery = NULL;
	}
}


IGCSQLResultSetList *CSQLAccess::GetResults()
{
	return m_pQueryGroup->GetResults();
}


//------------------------------------------------------------------------------------
// Purpose: Returns the number of result sets
//------------------------------------------------------------------------------------
uint32 CSQLAccess::GetResultSetCount()
{
	if( m_pQueryGroup->GetResults() )
		return m_pQueryGroup->GetResults()->GetResultSetCount();
	else
		return 0;
}


//------------------------------------------------------------------------------------
// Purpose: Returns the number of rows in a result set
//------------------------------------------------------------------------------------
uint32 CSQLAccess::GetResultSetRowCount( uint32 unResultSet )
{
	if( m_pQueryGroup->GetResults() && unResultSet < m_pQueryGroup->GetResults()->GetResultSetCount() )
		return m_pQueryGroup->GetResults()->GetResultSet( unResultSet )->GetRowCount();
	else
		return 0;
}


//------------------------------------------------------------------------------------
// Purpose: Returns a CSQLRecord object that represents a row in a result set
//------------------------------------------------------------------------------------
CSQLRecord CSQLAccess::GetResultRecord( uint32 unResultSet, uint32 unRow )
{
	if( m_pQueryGroup->GetResults() && unResultSet < m_pQueryGroup->GetResults()->GetResultSetCount() )
	{
		IGCSQLResultSet *pResultSet = m_pQueryGroup->GetResults()->GetResultSet( unResultSet );
		if( unRow < pResultSet->GetRowCount() )
			return CSQLRecord( unRow, pResultSet );
	}
	return CSQLRecord(); // if there was a problem return an empty record
}

//-----------------------------------------------------------------------------
// Purpose: Inserts a new record into the DS
// Input:	pRecordBase - record to insert
// Output:	true if successful, false otherwise
//-----------------------------------------------------------------------------
bool CSQLAccess::BYieldingInsertRecord( const CRecordBase *pRecordBase )
{
	ClearParams();

	const CRecordInfo *pRecordInfo = pRecordBase->GetPSchema()->GetRecordInfo();
	int cColumns = pRecordInfo->GetNumColumns();
	for ( int nColumn = 0; nColumn < cColumns; nColumn++ )
	{
		const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( nColumn );
		if ( !columnInfo.BIsInsertable() )
			continue;

		uint8 *pubData;
		uint32 cubData;
		DbgVerify( pRecordBase->BGetField( nColumn, &pubData, &cubData ) );
		
		CurrentQuery()->AddBindParamRaw( columnInfo.GetType(), pubData, cubData );
	}

	uint32 nRows;
	const char *pchStatement = pRecordBase->GetPSchema()->GetInsertStatementText();

	bool bRet = BYieldingExecute( pchStatement, pchStatement, &nRows );
	return ( nRows == 1 || BInTransaction() ) && bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Inserts a new record into the DS if such row doesn't exist
// Input:	pRecordBase - record to insert
// Output:	true if successful, false otherwise
//-----------------------------------------------------------------------------
bool CSQLAccess::BYieldingInsertWhenNotMatchedOnPK( CRecordBase *pRecordBase )
{
	ClearParams();

	const CRecordInfo *pRecordInfo = pRecordBase->GetPSchema()->GetRecordInfo();
	int cColumns = pRecordInfo->GetNumColumns();
	for ( int nColumn = 0; nColumn < cColumns; nColumn++ )
	{
		const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( nColumn );
		if ( !columnInfo.BIsInsertable() )
		{
			Assert( columnInfo.BIsInsertable() );
			return false;
		}

		uint8 *pubData;
		uint32 cubData;
		DbgVerify( pRecordBase->BGetField( nColumn, &pubData, &cubData ) );

		CurrentQuery()->AddBindParamRaw( columnInfo.GetType(), pubData, cubData );
	}

	uint32 nRows;
	const char *pchStatement = pRecordBase->GetPSchema()->GetMergeStatementTextOnPKWhenNotMatchedInsert();

	bool bRet = BYieldingExecute( pchStatement, pchStatement, &nRows );
	return ( nRows == 1 || nRows == 0 || BInTransaction() ) && bRet;
}

//-----------------------------------------------------------------------------
// Purpose: Inserts a new record into the DS if such row doesn't exist
//			updates an existing row if such row is matched by PK
// Input:	pRecordBase - record to insert
// Output:	true if successful, false otherwise
//-----------------------------------------------------------------------------
bool CSQLAccess::BYieldingInsertOrUpdateOnPK( CRecordBase *pRecordBase )
{
	ClearParams();

	const CRecordInfo *pRecordInfo = pRecordBase->GetPSchema()->GetRecordInfo();
	int cColumns = pRecordInfo->GetNumColumns();
	for ( int nColumn = 0; nColumn < cColumns; nColumn++ )
	{
		const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( nColumn );
		if ( !columnInfo.BIsInsertable() )
		{
			Assert( columnInfo.BIsInsertable() );
			return false;
		}

		uint8 *pubData;
		uint32 cubData;
		DbgVerify( pRecordBase->BGetField( nColumn, &pubData, &cubData ) );

		CurrentQuery()->AddBindParamRaw( columnInfo.GetType(), pubData, cubData );
	}

	uint32 nRows;
	const char *pchStatement = pRecordBase->GetPSchema()->GetMergeStatementTextOnPKWhenMatchedUpdateWhenNotMatchedInsert();

	bool bRet = BYieldingExecute( pchStatement, pchStatement, &nRows );
	return ( nRows == 1 || BInTransaction() ) && bRet;
}

//-----------------------------------------------------------------------------
// Purpose: Inserts a new record into the DB and reads non-insertable fields back
//			into the record.
// Input:	pRecordBase - record to insert
// Output:	true if successful, false otherwise
//-----------------------------------------------------------------------------
bool CSQLAccess::BYieldingInsertWithIdentity( CRecordBase* pRecordBase ) 
{
	AssertMsg( !BInTransaction(),  "BYieldingInsertWithIdentity is not supported in a transaction" );
	if( BInTransaction() )
		return false;
	ClearParams();

	TSQLCmdStr sStatement;
	CUtlVector<int> vecOutputFields;
	CRecordInfo *pRecordInfo = pRecordBase->GetPSchema()->GetRecordInfo();
	BuildInsertAndReadStatementText( &sStatement, &vecOutputFields, pRecordInfo );

	AssertMsg( vecOutputFields.Count() > 0, "BYieldingInsertAndReadRecord called for a record type with no non-insertable columns" );
	if ( vecOutputFields.Count() == 0 )
		return false;

	int cColumns = pRecordInfo->GetNumColumns();
	for ( int nColumn = 0; nColumn < cColumns; nColumn++ )
	{
		const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( nColumn );
		if ( !columnInfo.BIsInsertable() )
		{
			continue;
		}

		uint8 *pubData;
		uint32 cubData;
		DbgVerify( pRecordBase->BGetField( nColumn, &pubData, &cubData ) );

		CurrentQuery()->AddBindParamRaw( columnInfo.GetType(), pubData, cubData );
	}

	bool bRet = BYieldingExecute( sStatement, sStatement );
	if( !bRet )
		return false;

	Assert( 1 == GetResultSetCount() );
	if ( 1 != GetResultSetCount() )
		return false;

	IGCSQLResultSet *pResultSet = m_pQueryGroup->GetResults()->GetResultSet( 0 );
	Assert( 1 == pResultSet->GetRowCount() );
	if ( 1 != pResultSet->GetRowCount() )
		return false;

	Assert( (uint32)vecOutputFields.Count() == pResultSet->GetColumnCount() );
	if ( (uint32)vecOutputFields.Count() != pResultSet->GetColumnCount() )
		return false;

	for( uint32 nColumn = 0; nColumn < pResultSet->GetColumnCount(); nColumn++ )
	{
		uint8 *pubData;
		uint32 cubData;
		DbgVerify( pResultSet->GetData( 0, nColumn, &pubData, &cubData ) );

		int nSchColumn = vecOutputFields[nColumn];
		Assert( pResultSet->GetColumnType( nColumn ) == pRecordInfo->GetColumnInfo( nSchColumn ).GetType() );
		DbgVerify( pRecordBase->BSetField( nSchColumn, pubData, cubData ) );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Reads a list of records from the DB according to the specified where
//			clause
// Input:	pRecordBase - record to read
//			readSet - The set of columns to read
//			whereSet - The set of columns to query on
// Output:	true if successful, false otherwise
//-----------------------------------------------------------------------------
EResult CSQLAccess::YieldingReadRecordWithWhereColumns( CRecordBase *pRecord, const CColumnSet & readSet, const CColumnSet & whereSet, const char* pchOrderClause )
{
	AssertMsg( !BInTransaction(), "BYieldingReadRecordWithWhereColumns is not supported in a transaction" );
	if( BInTransaction() )
		return k_EResultInvalidState;

	//if there is an order by clause, only take the top one, if there isn't, then validate that we have a single instance
	const char* pszTopClause = ( pchOrderClause ) ? "TOP (1)" : "TOP (2)";

	TSQLCmdStr sStatement;
	BuildSelectStatementText( &sStatement, readSet, pszTopClause );

	// if we actually have some columns for the where clause,
	// append a where clause.
	if( whereSet.GetColumnCount() )
	{
		sStatement.Append( " WHERE " );
		AppendWhereClauseText( &sStatement, whereSet );
		AddRecordParameters( *pRecord, whereSet );
	}
	//append the order by if they added one
	if( pchOrderClause )
	{
		sStatement.Append( " ORDER BY " );
		sStatement.Append( pchOrderClause );
	}

	Assert(!readSet.IsEmpty() );
	if( !BYieldingExecute( sStatement, sStatement ) )
		return k_EResultFail;

	if ( GetResultSetCount() != 1 )
	{
		AssertMsg( GetResultSetCount() == 1, "Unexpected number of result sets returned from select statement" );
		return k_EResultFail;
	}

	// make sure the types are the same
	IGCSQLResultSet *pResultSet = m_pQueryGroup->GetResults()->GetResultSet( 0 );
	if ( pResultSet->GetRowCount() == 0 )
		return k_EResultNoMatch;

	//note that since we only take the top one when there is an order by clause, we don't need to handle that case down here, only if top 2 is selected
	if( pResultSet->GetRowCount() != 1 )
	{
		// Make sure we aren't failing because there are multiple matching records.
		// That is probably a misuse of the API or some unexpected condition.
		AssertMsg1( false, "BYieldingReadRecordWithWhereColumns from %s failing because multiple records match WHERE clause", readSet.GetRecordInfo()->GetName() );
		return k_EResultLimitExceeded;
	}
	FOR_EACH_COLUMN_IN_SET( readSet, nColumnIndex )
	{
		EGCSQLType eRecordType = readSet.GetColumnInfo( nColumnIndex ).GetType();
		EGCSQLType eResultType = pResultSet->GetColumnType( nColumnIndex );

		AssertMsg2( eResultType == eRecordType, "Column %d type mismatch in %s", nColumnIndex, readSet.GetRecordInfo()->GetName() );
		if( eRecordType != eResultType )
			return k_EResultInvalidParam;
	}

	CSQLRecord sqlRecord = GetResultRecord( 0, 0 );

	FOR_EACH_COLUMN_IN_SET( readSet, nColumnIndex )
	{
		uint8 *pubData;
		uint32 cubData;

		DbgVerify( sqlRecord.BGetColumnData( nColumnIndex, &pubData, (int*)&cubData ) );
		DbgVerify( pRecord->BSetField( readSet.GetColumn( nColumnIndex), pubData, cubData ) );
	}

	return k_EResultOK;
}

//-----------------------------------------------------------------------------
// Purpose: Updates a record in the DB
// Input:	record - data source for columns to match against (whereColumns) and
//					 columns to assign (updateColumns)
//			whereColumns - columns to match against
//			updateColumns - columns to update
// Output:	true if successful, false otherwise
//-----------------------------------------------------------------------------
bool CSQLAccess::BYieldingUpdateRecord( const CRecordBase & record, const CColumnSet & whereColumns, const CColumnSet & updateColumns, const CSQLOutputParams *pOptionalOutputParams /* = NULL */ )
{
	return BYieldingUpdateRecords( record, whereColumns, record, updateColumns, pOptionalOutputParams );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CSQLAccess::BYieldingUpdateRecords( const CRecordBase & whereRecord, const CColumnSet & whereColumns, const CRecordBase & updateRecord, const CColumnSet & updateColumns, const CSQLOutputParams *pOptionalOutputParams /* = NULL */ )
{
	ClearParams();

	Assert( whereColumns.GetRecordInfo() == updateColumns.GetRecordInfo() );
	if ( whereColumns.GetRecordInfo() != updateColumns.GetRecordInfo() )
		return false;
	Assert( whereColumns.GetRecordInfo() == whereRecord.GetPSchema()->GetRecordInfo() );
	if ( whereColumns.GetRecordInfo() != whereRecord.GetPSchema()->GetRecordInfo() )
		return false;
	Assert( whereColumns.GetRecordInfo() == updateRecord.GetPSchema()->GetRecordInfo() );
	if ( whereColumns.GetRecordInfo() != updateRecord.GetPSchema()->GetRecordInfo() )
		return false;

	AssertMsg( !updateColumns.IsEmpty(), "Someone is calling BYieldingUpdateRecord with no columns to update." );
	if ( updateColumns.IsEmpty() )
		return false;

	// add the columns we're updating as bound params
	TSQLCmdStr sStatement;
	BuildUpdateStatementText( &sStatement, updateColumns );

	AddRecordParameters( updateRecord, updateColumns );

	// did the users specify an OUTPUT block?
	if ( pOptionalOutputParams )
	{
		TSQLCmdStr sOutput;
		BuildOutputClauseText( &sOutput, pOptionalOutputParams->GetColumnSet() );
		sStatement.Append( sOutput );

		AddRecordParameters( pOptionalOutputParams->GetRecord(), pOptionalOutputParams->GetColumnSet() );
	}

	if ( !whereColumns.IsEmpty() )
	{
		sStatement.Append( " WHERE " );
		AppendWhereClauseText( &sStatement, whereColumns );
	
		// add the columns we're querying on as bound params
		AddRecordParameters( whereRecord, whereColumns );
	}

	return BYieldingExecute( sStatement, sStatement );
}

//-----------------------------------------------------------------------------
// Purpose: Deletes this record's row in the table
// Input:	record - record to delete
//			whereColumns - columns to use when searching for this record
//-----------------------------------------------------------------------------
bool CSQLAccess::BYieldingDeleteRecords( const CRecordBase & record, const CColumnSet & whereColumns )
{
	Assert( whereColumns.GetRecordInfo() == record.GetPSchema()->GetRecordInfo() );
	if ( whereColumns.GetRecordInfo() != record.GetPSchema()->GetRecordInfo() )
		return false;

	ClearParams();
	AddRecordParameters( record, whereColumns );

	TSQLCmdStr sStatement;
	BuildDeleteStatementText( &sStatement, record.GetPRecordInfo() );
	sStatement.Append( " WHERE " );
	AppendWhereClauseText( &sStatement, whereColumns );

	uint32 unRowsAffected;
	if( !BYieldingExecute( sStatement, sStatement, &unRowsAffected ) )
		return false;

	return unRowsAffected > 0 || BInTransaction();
}

//--------------------------------------------------------------------------------------------------------------------------------
// CSQLUpdateOrInsert
//--------------------------------------------------------------------------------------------------------------------------------

CSQLUpdateOrInsert::CSQLUpdateOrInsert( const char* pszName, int nTable, const CColumnSet & whereColumns, const CColumnSet & updateColumns, const char* pszWhereClause, const char* pszUpdateClause )
{
	const CRecordInfo* pRecordInfo = GSchemaFull().GetSchema( nTable ).GetRecordInfo();

	//how many columns do we have 
	const int nNumColumns = pRecordInfo->GetNumColumns();

	TSQLCmdStr sStatement;
	sStatement = "MERGE INTO ";
	sStatement.Append( GSchemaFull().GetDefaultSchemaNameForCatalog( pRecordInfo->GetESchemaCatalog() ) );
	sStatement.Append( '.' );
	sStatement.Append( pRecordInfo->GetName() );
	sStatement.Append( " WITH(HOLDLOCK) AS D USING(VALUES(" );
	sStatement.AppendFormat( "%.*s", GetInsertArgStringChars( nNumColumns ), GetInsertArgString() );
	sStatement.Append( "))AS S(" );

	//add each column that we are adding the values for, along with the parameter from the structure
	for( int nCurrColumn = 0; nCurrColumn < nNumColumns; nCurrColumn++ )
	{
		const CColumnInfo& colInfo = pRecordInfo->GetColumnInfo( nCurrColumn );
		if( nCurrColumn != 0 )
			sStatement.Append( ',' );
		sStatement.Append( colInfo.GetName() );
	}

	//our where clause
	sStatement.Append( ")ON " );

	if( pszWhereClause )
	{
		sStatement.Append( pszWhereClause );
	}
	else
	{
		FOR_EACH_COLUMN_IN_SET( whereColumns, nCurrColumn )
		{
			const char* pszColName = pRecordInfo->GetColumnInfo( whereColumns.GetColumn( nCurrColumn ) ).GetName();
			if( nCurrColumn > 0 )
				sStatement.Append( " AND " );
			sStatement.AppendFormat( "D.%s=S.%s", pszColName, pszColName );
		}
	}

	//our update clause (if they have provided fields that they want to update)
	if( pszUpdateClause || !updateColumns.IsEmpty() )
	{
		sStatement.Append( " WHEN MATCHED THEN UPDATE SET " );
		if( pszUpdateClause )
		{
			sStatement.Append( pszUpdateClause );
		}
		else
		{
			FOR_EACH_COLUMN_IN_SET( updateColumns, nCurrColumn )
			{
				const char* pszColName = pRecordInfo->GetColumnInfo( updateColumns.GetColumn( nCurrColumn ) ).GetName();
				if( nCurrColumn > 0 )
					sStatement.Append( ',' );
				sStatement.AppendFormat( "%s=S.%s", pszColName, pszColName );
			}
		}
	}

	//our insert clause
	sStatement.Append( " WHEN NOT MATCHED THEN INSERT(" );
	bool bFirstColumn = true;
	for( int nCurrColumn = 0; nCurrColumn < nNumColumns; nCurrColumn++ )
	{
		const CColumnInfo& colInfo = pRecordInfo->GetColumnInfo( nCurrColumn );
		if( !colInfo.BIsInsertable() )
			continue;

		if( !bFirstColumn )
			sStatement.Append( ',' );
		bFirstColumn = false;
		sStatement.Append( colInfo.GetName() );
	}

	sStatement.Append( ")VALUES(" );
	bFirstColumn = true;
	for( int nCurrColumn = 0; nCurrColumn < nNumColumns; nCurrColumn++ )
	{
		const CColumnInfo& colInfo = pRecordInfo->GetColumnInfo( nCurrColumn );
		if( !colInfo.BIsInsertable() )
			continue;

		if( !bFirstColumn )
			sStatement.Append( ',' );
		bFirstColumn = false;
		sStatement.AppendFormat( "S.%s", colInfo.GetName() );
	}
	sStatement.Append( ");" );

	//save our results so we can execute it in the future
	m_nTable = nTable;
	m_sName = pszName;
	m_sQuery = sStatement;
}

bool CSQLUpdateOrInsert::BYieldingExecute( CSQLAccess& sqlAccess, const CRecordBase& record, uint32 *out_punRowsAffected /* = NULL */ ) const
{
	AssertMsg2( record.GetITable() == m_nTable, "Error: Merge was compiled for table %s, but was attempted to be executed against %s", GSchemaFull().GetSchema( m_nTable ).GetRecordInfo()->GetName(), record.GetPRecordInfo()->GetName() );

	const CRecordInfo* pRecordInfo = record.GetPRecordInfo();
	//how many columns do we have 
	const int nNumColumns = pRecordInfo->GetNumColumns();

	sqlAccess.ClearParams();
	for( int nCurrColumn = 0; nCurrColumn < nNumColumns; nCurrColumn++ )
	{
		const CColumnInfo& colInfo = pRecordInfo->GetColumnInfo( nCurrColumn );
		uint8 *pubData;
		uint32 cubData;
		DbgVerify( record.BGetField( nCurrColumn, &pubData, &cubData ) );
		sqlAccess.AddBindParamRaw( colInfo.GetType(), pubData, cubData );
	}

	return sqlAccess.BYieldingExecute( m_sName, m_sQuery, out_punRowsAffected );
}


//-----------------------------------------------------------------------------
// Purpose: Adds bind parameters to the list based on a set of fields in a record
// Input:	record - record to insert
//			columnSet - The set of columns to add as params
//-----------------------------------------------------------------------------
void CSQLAccess::AddRecordParameters( const CRecordBase &record, const CColumnSet & columnSet )
{
	Assert( record.GetPSchema()->GetRecordInfo() == columnSet.GetRecordInfo() );
	if ( record.GetPSchema()->GetRecordInfo() != columnSet.GetRecordInfo() )
		return;

	FOR_EACH_COLUMN_IN_SET( columnSet, nColumnIndex )
	{
		const CColumnInfo &columnInfo = columnSet.GetColumnInfo( nColumnIndex );
		uint8 *pubData;
		uint32 cubData;
		DbgVerify( record.BGetField( columnSet.GetColumn( nColumnIndex ), &pubData, &cubData ) );
		EGCSQLType eType = columnInfo.GetType();
		CurrentQuery()->AddBindParamRaw( eType, pubData, cubData );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Deletes all records from a table
// Input:	iTable - table to wipe
// Output:	true if the operation was successful
// Note:	PERFORMANCE WARNING: this is slow on big tables, not intended for use
//				in production
//-----------------------------------------------------------------------------
bool CSQLAccess::BYieldingWipeTable( int iTable )
{
	// make a wipe operation
	CRecordInfo *pRecordInfo = GSchemaFull().GetSchema( iTable ).GetRecordInfo();

	CUtlString buf;
	buf.Format( "DELETE FROM %s", pRecordInfo->GetName() );
	return BYieldingExecute( buf.String(), buf.String() );
}


//-----------------------------------------------------------------------------
// Purpose: Returns the current query to add stuff to, creating it if there isn't
//			already a current query
//-----------------------------------------------------------------------------
CGCSQLQuery *CSQLAccess::CurrentQuery()
{
	if( m_pCurrentQuery )
		return m_pCurrentQuery;

	m_pCurrentQuery = new CGCSQLQuery();
	return m_pCurrentQuery;
}


} // namespace GCSDK
