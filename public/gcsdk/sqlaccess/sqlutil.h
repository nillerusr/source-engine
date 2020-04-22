//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef SQLUTIL_H
#define SQLUTIL_H

#ifdef _WIN32
#pragma once
#endif


namespace GCSDK
{
typedef CFmtStrN< 4096 >		TSQLCmdStr;

// Returns a long (1024 char) string of "?,?,?,?,?"... for use in IN clauses or INSERT statements
const char *GetInsertArgString();
// Returns the number of characters that should be used for the specified number of parameters
uint32 GetInsertArgStringChars( uint32 nNumParams );
// Returns the maximum number of parameters that are supported by the GetInsertArgString string
uint32 GetInsertArgStringMaxParams();


void ConvertFieldToText( EGCSQLType eFieldType, uint8 *pubRecord, int cubRecord, char *rgchField, int cchField, bool bQuoteString = true );
void ConvertFieldArrayToInText( const CColumnInfo &columnInfo, byte *pubData, int cubData, char *rgchResult, bool bForPreparedStatement );
char *SQLTypeFromField( const CColumnInfo &colInfo, char *pchBuf, int cchBuf );
void EscapeStringValue( char *rgchField, int cchField );
void AppendConstraints( const CRecordInfo *pRecordInfo, const CColumnInfo *pColumnInfo, bool bForAdd, CFmtStrMax & sCmd );
void AppendTableConstraints( CRecordInfo *pRecordInfo, CFmtStrMax & sCmd );
void AppendConstraint( const char *pchTableName, const char *pchColumnName, int nColFlagConstraint, bool bForAdd, bool bClustered,
					   CFmtStrMax & sCmd, int nFillFactor );
void BuildTablePKConstraintText( TSQLCmdStr *psStatement, CRecordInfo *pRecordInfo );
//void BuildSelectStatementText( CUtlVector<CQuery> *pVecQuery, bool bForPreparedStatement, char *pchStatement, int cchStatement );

void BuildInsertStatementText( TSQLCmdStr *psStatement, const CRecordInfo *pRecordInfo );
void BuildInsertAndReadStatementText( TSQLCmdStr *psStatement, CUtlVector<int> *pvecOutputFields, const CRecordInfo *pRecordInfo ) ;
void BuildMergeStatementTextOnPKWhenMatchedUpdateWhenNotMatchedInsert( TSQLCmdStr *psStatement, const CRecordInfo *pRecordInfo );
void BuildMergeStatementTextOnPKWhenNotMatchedInsert( TSQLCmdStr *psStatement, const CRecordInfo *pRecordInfo );
void BuildSelectStatementText( TSQLCmdStr *psStatement, const CColumnSet & selectSet, const char *pchTopClause = NULL );		  
void BuildUpdateStatementText( TSQLCmdStr *psStatement, const CColumnSet & columnSet );
void BuildDeleteStatementText( TSQLCmdStr *psStatement, const CRecordInfo* pRecordInfo );
void AppendWhereClauseText( TSQLCmdStr *psClause, const CColumnSet & columnSet );
void BuildOutputClauseText( TSQLCmdStr *psClause, const CColumnSet & columnSet );

template< typename T >
bool CopyResultToSchVector( IGCSQLResultSet *pResultSet, const CColumnSet & readSet, CUtlVector< T > *pvecRecords )
{
	if ( pResultSet->GetRowCount() == 0 )
		return true;

	FOR_EACH_COLUMN_IN_SET( readSet, nColumnIndex )
	{
		EGCSQLType eRecordType = readSet.GetColumnInfo( nColumnIndex ).GetType();
		EGCSQLType eResultType = pResultSet->GetColumnType( nColumnIndex );

		Assert( eResultType == eRecordType );
		if( eRecordType != eResultType )
			return false;
	}


	for( CSQLRecord sqlRecord( 0, pResultSet ); sqlRecord.IsValid(); sqlRecord.NextRow() )
	{
		int nRecord = pvecRecords->AddToTail();

		FOR_EACH_COLUMN_IN_SET( readSet, nColumnIndex )
		{
			uint8 *pubData;
			uint32 cubData;

			DbgVerify( sqlRecord.BGetColumnData( nColumnIndex, &pubData, (int*)&cubData ) );
			DbgVerify( pvecRecords->Element( nRecord ).BSetField( readSet.GetColumn( nColumnIndex), pubData, cubData ) );
		}
	}
	return true;
}

//EResult UpdateOrInsertUnique( CSQLAccess &sqlAccess, int iTable, int iField, CRecordBase *pRecordBase, int iIndexID );
//bool BIsDuplicateInsertAttempt( const CSQLErrorInfo *pErr );


#define EXIT_WITH_SQL_FAILURE( ret ) { nRet = ret; goto Exit; }

#define EXIT_ON_BOOL_FAILURE( bRet, msg ) \
	{ \
		if ( false == bRet ) \
		{ \
			SetSQLError( msg ); \
			nRet = SQL_ERROR; \
			goto Exit; \
		} \
	}

#define SAFE_CLOSE_STMT( x ) \
	if ( NULL != (x) ) \
	{ \
		SQLFreeHandle( SQL_HANDLE_STMT, (x) ); \
		(x) = NULL; \
	} 

#define SAFE_FREE_HANDLE( x, y ) \
	if ( NULL != (x) ) \
{ \
	SQLFreeHandle( y, (x) ); \
	(x) = NULL; \
} 

} // namespace GCSDK
#endif // SQLUTIL_H
