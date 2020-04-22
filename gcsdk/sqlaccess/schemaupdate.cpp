//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains the job that's responsible for updating the database schema
//
//=============================================================================

#include "stdafx.h"
#include "gcsdk/sqlaccess/schemaupdate.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{
#ifndef SQL_SUCCESS
#define SQL_SUCCESS                0
#define SQL_SUCCESS_WITH_INFO      1
#define SQL_NO_DATA              100
#define SQL_ERROR                 (-1)
#endif // SQLSUCCESS

// this comes from sql.h. The GC really shouldn't depend on the MSSQL headers
#define SQL_INDEX_CLUSTERED 1

inline bool SQL_OK( SQLRETURN nRet )
{
	return ( ( SQL_SUCCESS == nRet ) || (SQL_SUCCESS_WITH_INFO == nRet ) );
}

#define SQL_FAILED( ret )	( !SQL_OK( ret ) )

#define EXIT_ON_SQL_FAILURE( ret ) \
{ \
	if ( !SQL_OK( ret ) ) \
{ \
	goto Exit;	\
}  \
}

#define EXIT_ON_BOOLSQL_FAILURE( ret ) \
{ \
	if ( !(ret) ) \
{ \
	nRet = SQL_ERROR; \
	goto Exit;	\
}  \
}

#define RETURN_SQL_ERROR_ON_FALSE( ret ) \
	if( !(ret) ) \
	{\
		return SQL_ERROR;\
	}


//-----------------------------------------------------------------------------
// Purpose: Emits a message and appends it to a string
//-----------------------------------------------------------------------------
void EmitAndAppend( CFmtStr1024 & sTarget, const CGCEmitGroup& Group, int iLevel, int iLevelLog, PRINTF_FORMAT_STRING const char *pchMessage, ... )
{
	va_list args;
	va_start( args, pchMessage );

	if( sTarget.Length() < 1024 )
		sTarget.AppendFormatV( pchMessage, args );

	EmitInfoV( Group, iLevel, iLevelLog, pchMessage, args );

	va_end( args );
}

//-----------------------------------------------------------------------------
// builds a command of the form:
//		CREATE [CLUSTERED] [UNIQUE] INDEX <index_name> ON <table_name>
//		(col1, col2, ...)
//		[INCLUDE (icol1, icol2, ...)]
//		[WITH (FILLFACTOR = n)]
//-----------------------------------------------------------------------------
CUtlString GetAddIndexSQL( CRecordInfo *pRecordInfo, const FieldSet_t &refFields )
{
	CFmtStrMax sCmd;
	sCmd.sprintf( "CREATE %s%sINDEX %s ON App%u.%s (",
		refFields.IsClustered() ? "CLUSTERED " : "",
		refFields.IsUnique() ? "UNIQUE " : "",
		refFields.GetIndexName(),
		GGCBase()->GetAppID(),
		pRecordInfo->GetName() );

	// add real columns
	for ( int n = 0; n < refFields.GetCount(); n++ )
	{
		int nField = refFields.GetField( n );
		const CColumnInfo &refInfo = pRecordInfo->GetColumnInfo( nField );

		sCmd.AppendFormat( "%s%s", 
			(n > 0) ? "," : "",
			refInfo.GetName() );
	}
	sCmd += ")";

	// do we have any included columns?
	if ( refFields.GetIncludedCount() > 0 )
	{
		// yes, add those
		sCmd += "\nINCLUDE (";

		for ( int n = 0; n < refFields.GetIncludedCount(); n++ )
		{
			int nField = refFields.GetIncludedField( n );
			const CColumnInfo &refInfo = pRecordInfo->GetColumnInfo( nField );

			sCmd.AppendFormat( "%s%s", 
				(n > 0) ? "," : "",
				refInfo.GetName() );
		}
		sCmd += ")";
	}

	// do we need a fill factor?
	if ( refFields.GetFillFactor() != 0)
	{
		sCmd.AppendFormat("\nWITH (FILLFACTOR = %d)",
			refFields.GetFillFactor() );
	}

	return CUtlString( sCmd.String() );
}

CUtlString GetAlterColumnText( CRecordInfo *pRecordInfo, const CColumnInfo *pColumnInfoDesired )
{
	Assert( pRecordInfo );
	Assert( pColumnInfoDesired );

	char rgchTmp[128];
	CUtlString sCmd;
	sCmd.Format( "ALTER TABLE App%u.%s ALTER COLUMN %s %s %s", GGCBase()->GetAppID(), pRecordInfo->GetName(), pColumnInfoDesired->GetName(),
		SQLTypeFromField( *pColumnInfoDesired, rgchTmp, Q_ARRAYSIZE( rgchTmp ) ),
		pColumnInfoDesired->BIsPrimaryKey() ? "NOT NULL" : ""
	);
	return sCmd;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSchemaUpdate::CSchemaUpdate()
{
	m_mapPRecordInfoDesired.SetLessFunc( CaselessStringLessThan );
	m_eConversionMode = k_EConversionModeInspectOnly;

	m_bConversionNeeded = false;
	m_cTablesDesiredMissing = 0;
	m_cTablesActualDifferent = 0;
	m_cTablesActualUnknown = 0;
	m_cTablesNeedingChange = 0;
	m_cColumnsDesiredMissing = 0;
	m_cColumnsActualDifferent = 0;
	m_cColumnsActualUnknown = 0;
	m_bSkippedAChange = false;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSchemaUpdate::~CSchemaUpdate()
{
	// release all the record info's we're holding onto
	FOR_EACH_MAP_FAST( m_mapPRecordInfoDesired, iRecordInfo )
	{
		SAFE_RELEASE( m_mapPRecordInfoDesired[iRecordInfo] );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds a record info that describes a desired table that should
//			exist in the database
// Input:	pRecordInfo - pointer to record info
//-----------------------------------------------------------------------------
void CSchemaUpdate::AddRecordInfoDesired( CRecordInfo *pRecordInfo )
{
	Assert( pRecordInfo );
	const char *pchName = pRecordInfo->GetName();
	Assert( pchName && pchName[0] );
	Assert( m_mapPRecordInfoDesired.InvalidIndex() == m_mapPRecordInfoDesired.Find( pchName ) );
	// addref the record info since we're hanging onto it
	pRecordInfo->AddRef();
	// insert it in our map, indexed by name
	m_mapPRecordInfoDesired.Insert( pchName, pRecordInfo );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a CFTSCatalogInfo that tells us about a FTS catalog that
//			should exist in the database
//-----------------------------------------------------------------------------
void CSchemaUpdate::AddFTSInfo( const CFTSCatalogInfo &refFTSInfo )
{
	m_listFTSCatalogInfo.AddToTail( refFTSInfo );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a CFTSCatalogInfo that tells us about a FTS catalog that
//			should exist in the database
//-----------------------------------------------------------------------------
void CSchemaUpdate::AddTriggerInfos( const CUtlVector< CTriggerInfo > &refTriggerInfo )
{
	m_vecTriggerInfo = refTriggerInfo;
}


//-----------------------------------------------------------------------------
// Purpose: Validates and updates the database schema
//-----------------------------------------------------------------------------
bool CJobUpdateSchema::BYieldingRunJob( void * )
{
	// update the main schema
	EmitInfo( SPEW_GC, 2, 2, "Updating main schema...\n" );
	if ( !BYieldingUpdateSchema( k_ESchemaCatalogMain ) )
	{
		m_pGC->SetStartupComplete( false );
		return false;
	}

	// Could fail, but we shouldn't stop from starting up if it does
	BYieldingUpdateSchema( k_ESchemaCatalogOGS );

	bool bSuccess = m_pGC->BYieldingFinishStartup();
	m_pGC->SetStartupComplete( bSuccess );
	if ( bSuccess )
	{
		bSuccess = m_pGC->BYieldingPostStartup();
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CJobUpdateSchema::BYieldingUpdateSchema( ESchemaCatalog eSchemaCatalog )
{
	if( !YieldingBuildTypeMap( eSchemaCatalog ) )
		return false;

	// make an object to communicate desired database schema & results
	CSchemaUpdate *pSchemaUpdate = new CSchemaUpdate();

	// do safe conversions only
	// TODO - do one round of inspection only first so we can send watchdog alert about what's about
	// to happen, then do conversions.  Also force conversions in dev system.
	pSchemaUpdate->m_eConversionMode = k_EConversionModeConvertSafe;

	// Add all the tables to desired schema
	for ( int iTable = 0; iTable < m_iTableCount; iTable++ )
	{
		// MERGE COMMENT: "schema" cannot be used as a variable name because it is an empty #define resulting in error C2059
		CSchema &gc_schema = GSchemaFull().GetSchema( iTable );

		// is it in the schema we want?
		if ( gc_schema.GetESchemaCatalog() == eSchemaCatalog )
			pSchemaUpdate->AddRecordInfoDesired( gc_schema.GetRecordInfo() );
	}

	// add all the FTS catalogs to the desired schema
	for ( int n = 0; n < GSchemaFull().GetCFTSCatalogs(); n++ )
	{
		const CFTSCatalogInfo &refInfo = GSchemaFull().GetFTSCatalogInfo( n );
		pSchemaUpdate->AddFTSInfo( refInfo );
	}

	pSchemaUpdate->AddTriggerInfos( GSchemaFull().GetTriggerInfos() );

	SQLRETURN nRet = YieldingEnsureDatabaseSchemaCorrect( eSchemaCatalog, pSchemaUpdate );
	if( !SQL_OK( nRet ) )
	{
		AssertMsg( false, "SQL Schema Update failed" );
		return false;
	}

	SAFE_RELEASE( pSchemaUpdate );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Examines actual running schema of database, compares to specified desired
//			schema, and changes the actual schema to correspond to desired schema
// Input:	pSQLThread - SQL thread to execute on
//			pSchemaUpdate - pointer to object with desired schema 
// Output:  SQL return code.
//-----------------------------------------------------------------------------
SQLRETURN CJobUpdateSchema::YieldingEnsureDatabaseSchemaCorrect( ESchemaCatalog eSchemaCatalog, CSchemaUpdate *pSchemaUpdate )
{
	Assert( pSchemaUpdate );

	CMapPRecordInfo &mapPRecordInfoDesired = pSchemaUpdate->m_mapPRecordInfoDesired;
	const CUtlVector< CTriggerInfo > &vecTriggerInfoDesired = pSchemaUpdate->m_vecTriggerInfo;
	m_eConversionMode = pSchemaUpdate->m_eConversionMode;

	bool bDoConversion = true;
//	bool bDoConversion = ( ( k_EConversionModeConvertSafe == eConversionMode ) || 
//		( k_EConversionModeConvertIrreversible == eConversionMode ) );	

	CMapPRecordInfo mapPRecordInfoActual;
	mapPRecordInfoActual.SetLessFunc( CaselessStringLessThan );
	CUtlVector<CRecordInfo *> vecPRecordInfoDesiredMissing;
	CUtlVector<CRecordInfo *> vecPRecordInfoActualDifferent;
	CUtlVector<CRecordInfo *> vecPRecordInfoActualUnknown;
	CUtlVector< CTriggerInfo > vecTriggerInfoActual;
	CUtlVector< CTriggerInfo > vecTriggerInfoMissing;
	CUtlVector< CTriggerInfo > vecTriggerInfoDifferent;

	pSchemaUpdate->m_cTablesDesiredMissing = 0;
	pSchemaUpdate->m_cTablesActualDifferent = 0;
	pSchemaUpdate->m_cTablesActualUnknown = 0;
	pSchemaUpdate->m_cTablesNeedingChange = 0;
	pSchemaUpdate->m_cColumnsDesiredMissing = 0;
	pSchemaUpdate->m_cColumnsActualDifferent = 0;
	pSchemaUpdate->m_cColumnsActualUnknown = 0;
	pSchemaUpdate->m_sDetail.Clear();

	CFmtStr1024 &sDetail = pSchemaUpdate->m_sDetail;

	CFastTimer tickCounterOverall;
	tickCounterOverall.Start();

	//
	// Do some up-front bookkeeping to see how many tables need to be created and/or altered
	//

	int nSchemaID;
	SQLRETURN nRet = YieldingGetSchemaID( eSchemaCatalog, &nSchemaID );
	EXIT_ON_SQL_FAILURE( nRet );

	// Determine the actual running schema
	nRet = YieldingGetRecordInfoForAllTables( eSchemaCatalog, nSchemaID, mapPRecordInfoActual );
	EXIT_ON_SQL_FAILURE( nRet );

	nRet = YieldingGetTriggers( eSchemaCatalog, nSchemaID, vecTriggerInfoActual );
	EXIT_ON_SQL_FAILURE( nRet );

	// Look through the list of desired tables, find any that are missing or different from the actual schema
	FOR_EACH_MAP_FAST( mapPRecordInfoDesired, iRecordInfoDesired )
	{
		// is this desired table in the currently connected catalog?
		CRecordInfo *pRecordInfoDesired = mapPRecordInfoDesired[iRecordInfoDesired];
		if ( pRecordInfoDesired->GetESchemaCatalog() == eSchemaCatalog )
		{
			// yes. do something about it
			int iRecordInfoActual = mapPRecordInfoActual.Find( pRecordInfoDesired->GetName() );
			if ( mapPRecordInfoDesired.InvalidIndex() == iRecordInfoActual )
			{
				// This table is desired but does not exist
				vecPRecordInfoDesiredMissing.AddToTail( pRecordInfoDesired );
			}
			else
			{
				// Table with same name exists in desired & actual schemas; is it exactly the same?
				CRecordInfo *pRecordInfoActual = mapPRecordInfoActual[iRecordInfoActual];
				if ( !pRecordInfoDesired->EqualTo( pRecordInfoActual ) )
				{
					// This desired table exists but the actual table is different than desired
					vecPRecordInfoActualDifferent.AddToTail( pRecordInfoActual );
				}
			}
		}
	}

	// Now, look through the list of actual tables and find any that do not exist in the list of desired tables
	FOR_EACH_MAP_FAST( mapPRecordInfoActual, iRecordInfoActual )
	{
		CRecordInfo *pRecordInfoActual = mapPRecordInfoActual[iRecordInfoActual];
		int iRecordInfoDesired = mapPRecordInfoDesired.Find( pRecordInfoActual->GetName() );
		if ( !mapPRecordInfoDesired.IsValidIndex( iRecordInfoDesired ) )
		{
			// This table exists but is not in the list of desired tables
			// maybe it's an old table.

			vecPRecordInfoActualUnknown.AddToTail( pRecordInfoActual );
		}
	}

	// find a list of missing triggers
	FOR_EACH_VEC( vecTriggerInfoDesired, iDesired )
	{
		// not of this catalog? skip it
		if ( vecTriggerInfoDesired[ iDesired ].m_eSchemaCatalog != eSchemaCatalog )
			continue;

		// it is our catalog, so try and match
		bool bMatched = false;
		FOR_EACH_VEC( vecTriggerInfoActual, iActual )
		{
			// is it the same table and trigger name?
			if ( vecTriggerInfoActual[ iActual ] == vecTriggerInfoDesired[ iDesired ] )
			{
				// yes! test the text for differences
				if ( vecTriggerInfoActual[ iActual ].IsDifferent( vecTriggerInfoDesired[ iDesired ] ) )
				{
					vecTriggerInfoDifferent.AddToTail( vecTriggerInfoDesired[ iDesired ] );
				}
				else
				{
					// we have a match!
					vecTriggerInfoActual[ iActual ].m_bMatched = true;
					bMatched = true;
				}
				break;
			}
		}

		if ( !bMatched )
		{
			vecTriggerInfoMissing.AddToTail( vecTriggerInfoDesired[ iDesired ] );
		}
	}

	//
	// Now do the actual conversion
	//
	EmitAndAppend( sDetail, SPEW_GC, 2, 2, "Database conversion: %s\n",
		bDoConversion ? "beginning" : "inspection only" );


	// find tables which need to be created
	EmitAndAppend( sDetail, SPEW_GC, 2, 2, "# of specified tables that do not currently exist in database: %d\n", 
		vecPRecordInfoDesiredMissing.Count() );
	if ( vecPRecordInfoDesiredMissing.Count() > 0 )
		pSchemaUpdate->m_bConversionNeeded = true;

	// Create any tables which need to be created
	for ( int iTable = 0; iTable < vecPRecordInfoDesiredMissing.Count(); iTable++ )
	{
		CRecordInfo *pRecordInfoDesired = vecPRecordInfoDesiredMissing[iTable];
		if ( bDoConversion )
		{
			CFastTimer tickCounter;
			tickCounter.Start();

			// Create the table
			nRet = YieldingCreateTable( eSchemaCatalog, pRecordInfoDesired );
			EXIT_ON_SQL_FAILURE( nRet );
			tickCounter.End();
			int nElapsedMilliseconds = tickCounter.GetDuration().GetMilliseconds();
			EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\tCreated table: %s: %d millisec\n", pRecordInfoDesired->GetName(), nElapsedMilliseconds );
		}
		else
			EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t%s\n", pRecordInfoDesired->GetName() );
	}

	// find tables which are different
	EmitAndAppend( sDetail, SPEW_GC, 2, 2, "[GC]: # of specified tables that differ from schema in database: %d\n",
		vecPRecordInfoActualDifferent.Count() );

#ifdef _DEBUG
	// are some different? if so, list their names only for now.
	// This is in _debug only because it's useful for debugging the below loop,
	// but spewey for everyday life (as long as the below loop is working).
	if ( vecPRecordInfoActualDifferent.Count() > 0 ) 
	{
		FOR_EACH_VEC( vecPRecordInfoActualDifferent, i )
		{
			CRecordInfo *pRecordInfoActual = vecPRecordInfoActualDifferent[i];
			EmitAndAppend( sDetail, SPEW_GC, 2, 2, "[GC]:   table %s is different\n",
				pRecordInfoActual->GetName() );
		}
	}
#endif

	pSchemaUpdate->m_bSkippedAChange = false;

	// Alter any table which needs to be altered
	for ( int iTable = 0; iTable < vecPRecordInfoActualDifferent.Count(); iTable++ )
	{
		CRecordInfo *pRecordInfoActual = vecPRecordInfoActualDifferent[iTable];
		int iRecordInfoDesired = mapPRecordInfoDesired.Find( pRecordInfoActual->GetName() );
		Assert( mapPRecordInfoDesired.InvalidIndex() != iRecordInfoDesired );
		CRecordInfo *pRecordInfoDesired = mapPRecordInfoDesired[iRecordInfoDesired];

		CUtlVector<const CColumnInfo *> vecPColumnInfoDesiredMissing;
		CUtlVector<const CColumnInfo *> vecPColumnInfoActualDifferent;
		CUtlVector<const CColumnInfo *> vecPColumnInfoActualUnknown;

		// We know something is different between the actual & desired schema for this table, but don't yet know what

		// Compare each column
		for ( int iColumnDesired = 0; iColumnDesired < pRecordInfoDesired->GetNumColumns(); iColumnDesired ++ )
		{
			const CColumnInfo &columnInfoDesired = pRecordInfoDesired->GetColumnInfo( iColumnDesired );
			int iColumnActual = -1;
			bool bRet = pRecordInfoActual->BFindColumnByName( columnInfoDesired.GetName(), &iColumnActual );
			if ( bRet )
			{
				const CColumnInfo &columnInfoActual = pRecordInfoActual->GetColumnInfo( iColumnActual );
				if ( columnInfoActual.GetChecksum() != columnInfoDesired.GetChecksum() )
				{
					// The actual column is different than the desired column
					vecPColumnInfoActualDifferent.AddToTail( &columnInfoActual );
				}
			}
			else
			{
				// The desired column is missing from the actual table
				vecPColumnInfoDesiredMissing.AddToTail( &columnInfoDesired );
			}
		}
		for ( int iColumnActual = 0; iColumnActual < pRecordInfoActual->GetNumColumns(); iColumnActual ++ )
		{
			const CColumnInfo &columnInfoActual = pRecordInfoActual->GetColumnInfo( iColumnActual );
			int iColumnDesired = -1;
			bool bRet = pRecordInfoDesired->BFindColumnByName( columnInfoActual.GetName(), &iColumnDesired );
			if ( !bRet )
			{
				// this column exists in the running schema, but not in the desired schema (e.g. old column)
				vecPColumnInfoActualUnknown.AddToTail( &columnInfoActual );
			}
		}

		if ( ( vecPColumnInfoDesiredMissing.Count() > 0 )  || ( vecPColumnInfoActualDifferent.Count() > 0 ) )
		{
			pSchemaUpdate->m_bConversionNeeded = true;
			pSchemaUpdate->m_cTablesNeedingChange++;
		}

		// Add any desired columns which are missing from the actual schema
		if ( vecPColumnInfoDesiredMissing.Count() > 0 )
		{
			EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t\tDesired columns missing in table %s:\n", pRecordInfoActual->GetName() );

			for ( int iColumn = 0; iColumn < vecPColumnInfoDesiredMissing.Count(); iColumn++ )
			{
				const CColumnInfo *pColumnInfoDesired = vecPColumnInfoDesiredMissing[iColumn];
				EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t\t\t%s\n", pColumnInfoDesired->GetName() );
				if ( bDoConversion )
				{
					CFastTimer tickCounter;
					tickCounter.Start();
					// Add the column
					nRet = YieldingAlterTableAddColumn( eSchemaCatalog, pRecordInfoActual, pColumnInfoDesired );
					EXIT_ON_SQL_FAILURE( nRet );
					tickCounter.End();
					int nElapsedMilliseconds = tickCounter.GetDuration().GetMilliseconds();
					EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t\t\t\tCreated column %s: %d millisec\n", pColumnInfoDesired->GetName(), nElapsedMilliseconds );
				}
			}
		}

		// Check for any stray indices that aren't found in the specification?
		bool bIndexMismatch = false;
		for ( int idx = 0 ; idx < pRecordInfoActual->GetIndexFieldCount() ; ++idx )
		{
			const FieldSet_t &fs = pRecordInfoActual->GetIndexFields()[idx];
			if ( pRecordInfoDesired->FindIndex( pRecordInfoActual, fs ) >= 0 )
				continue;
			if ( pRecordInfoDesired->FindIndexByName( fs.GetIndexName() ) >= 0 )
				continue; // we already handled this above
			bIndexMismatch = true;

			if ( idx == pRecordInfoActual->GetPKIndex() )
			{
				EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\tTable %s primary key in database differs from specification.\n",
					pRecordInfoDesired->GetName() );
				nRet = YieldingProcessUnsafeConversion( eSchemaCatalog,
					CFmtStr("ALTER TABLE App%u.%s DROP CONSTRAINT %s", GGCBase()->GetAppID(), pRecordInfoActual->GetName(), fs.GetIndexName() ).String(),
					CFmtStr("%s: remove old primary key.", pRecordInfoDesired->GetName() ).String() );
				EXIT_ON_SQL_FAILURE( nRet );
			}
			else
			{
				EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\tTable %s index %s exists in database but is not in specification.  (Possible performance problem?).\n",
					pRecordInfoDesired->GetName(), fs.GetIndexName() );
				nRet = YieldingProcessUnsafeConversion( eSchemaCatalog,
					CFmtStr("DROP INDEX %s ON App%u.%s", fs.GetIndexName(), GGCBase()->GetAppID(), pRecordInfoActual->GetName() ).String(),
					CFmtStr("%s: cleanup stray index %s.", pRecordInfoDesired->GetName(), fs.GetIndexName() ).String() );
				EXIT_ON_SQL_FAILURE( nRet );
			}
		}

		// Change any columns which are different between desired and actual schema
		if ( vecPColumnInfoActualDifferent.Count() > 0 )
		{
			EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\tColumns that differ between specification and database in table %s:\n",
				pRecordInfoActual->GetName() );
			for ( int iColumn = 0; iColumn < vecPColumnInfoActualDifferent.Count(); iColumn++ )
			{
				const CColumnInfo *pColumnInfoActual = vecPColumnInfoActualDifferent[iColumn];
				EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t\t%s", pColumnInfoActual->GetName() );
				int iColumnDesired = -1;
				DbgVerify( pRecordInfoDesired->BFindColumnByName( pColumnInfoActual->GetName(), &iColumnDesired ) );
				const CColumnInfo *pColumnInfoDesired = &pRecordInfoDesired->GetColumnInfo( iColumnDesired );

				// if type or size changed, alter the column
				if ( ( pColumnInfoDesired->GetType() != pColumnInfoActual->GetType() )  ||
					// fixed length field, and the sizes differ
					( ! pColumnInfoDesired->BIsVariableLength() && 
					( pColumnInfoDesired->GetFixedSize() != pColumnInfoActual->GetFixedSize() ) ) ||
					// variable length field, and the sizes differ
					// fixed length field, and the sizes differ
					( pColumnInfoDesired->BIsVariableLength() && 
					( pColumnInfoDesired->GetMaxSize() != pColumnInfoActual->GetMaxSize() ) ) )
				{
					if ( k_EConversionModeConvertIrreversible != m_eConversionMode )
					{
						pSchemaUpdate->m_bSkippedAChange = true;
						if ( pColumnInfoDesired->GetType() != pColumnInfoActual->GetType() )
							EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t(data types differ: desired=%s, actual=%s)", PchNameFromEGCSQLType( pColumnInfoDesired->GetType() ), PchNameFromEGCSQLType( pColumnInfoActual->GetType() ) ); 
						if ( pColumnInfoDesired->GetFixedSize() != pColumnInfoActual->GetFixedSize() )
							EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t(column sizes differ: desired=%d, actual=%d)", pColumnInfoDesired->GetFixedSize(), pColumnInfoActual->GetFixedSize()  ); 
						if ( pColumnInfoDesired->GetMaxSize() != pColumnInfoActual->GetMaxSize() )
							EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t(maximum column sizes differ: desired=%d, actual=%d)", pColumnInfoDesired->GetMaxSize(), pColumnInfoActual->GetMaxSize()  ); 
					}
					nRet = YieldingChangeColumnTypeOrLength( eSchemaCatalog, pRecordInfoActual, pColumnInfoDesired );
					EXIT_ON_SQL_FAILURE( nRet );
				}

				// If column constraints/indexes are different, make appropriate adjustments
				// do this second so it has a chance to succeed - otherwise, we may create an index here that
				// prevents us from performing an alter table
				if ( pColumnInfoDesired->GetColFlags() != pColumnInfoActual->GetColFlags() )
				{
					if ( k_EConversionModeConvertIrreversible != m_eConversionMode )
					{
						char szDesiredFlags[k_nKiloByte];
						char szActualFlags[k_nKiloByte];
						pColumnInfoDesired->GetColFlagDescription( szDesiredFlags, Q_ARRAYSIZE( szDesiredFlags ) );
						pColumnInfoActual->GetColFlagDescription( szActualFlags, Q_ARRAYSIZE( szActualFlags ) );

						pSchemaUpdate->m_bSkippedAChange = true;
						EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t(column flags differ: desired=\"%s\", actual=\"%s\")",
							szDesiredFlags, szActualFlags );
					}
					nRet = YieldingChangeColumnProperties( eSchemaCatalog, pRecordInfoActual, pColumnInfoActual, pColumnInfoDesired );
					EXIT_ON_SQL_FAILURE( nRet );
				}

				if ( pSchemaUpdate->m_bSkippedAChange )
					EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t(Not attempting unsafe change).\n" );
			}
		}

		// Scan for any new / changed indices
		for ( int idx = 0 ; idx < pRecordInfoDesired->GetIndexFieldCount() ; ++idx )
		{
			const FieldSet_t &fs = pRecordInfoDesired->GetIndexFields()[idx];
			int iActualIdx = pRecordInfoActual->FindIndex( pRecordInfoDesired, fs );
			if ( iActualIdx >= 0 )
				continue;
			bIndexMismatch = true;

			// The exact index we want doesn't exist.  Is it the primary key?
			CUtlString sCommand;
			CUtlString sComment;
			if ( idx == pRecordInfoDesired->GetPKIndex() )
			{
					EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\tTable %s primary key in specification differs from database.\n",
						pRecordInfoDesired->GetName() );
					sComment.Format( "%s: create new primary key constraint", pRecordInfoDesired->GetName() );
					TSQLCmdStr cmd;
					BuildTablePKConstraintText( &cmd, pRecordInfoDesired );
					for ( int i = 0 ; i < fs.GetCount() ; ++i ) // make sure they are non-NULL
					{
						int idxField = fs.GetField(i);
						const CColumnInfo *pColInfo = &pRecordInfoDesired->GetColumnInfo( idxField );
						Assert( pColInfo->BIsPrimaryKey() );
						sCommand += GetAlterColumnText( pRecordInfoDesired, pColInfo );
						sCommand += ";\n";
					}

					CUtlString sCreatePK;
					sCreatePK.Format( "GO\nALTER TABLE App%u.%s ADD %s\n", GGCBase()->GetAppID(), pRecordInfoDesired->GetName(), cmd.String() );
					sCommand += sCreatePK;
			}
			else
			{

				// Another common thing that could happen is that an index is changed.
				// Look for an existing index with the same name as a way to try to
				// detect this common case and provide a more specific message.  (Otherwise,
				// we will report it as a missing index, and an extra unwanted index --- which
				// is correct but a bit more confusing.)

				iActualIdx = pRecordInfoActual->FindIndexByName( fs.GetIndexName() );
				if ( iActualIdx < 0 )
				{
					sComment.Format("%s: add index %s", pRecordInfoDesired->GetName(), fs.GetIndexName() );
					EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\tTable %s index %s is specified but not present in database.\n",
						pRecordInfoDesired->GetName(), fs.GetIndexName() );
				}
				else
				{
					EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\tTable %s index %s differs between specification and database.\n",
						pRecordInfoDesired->GetName(), fs.GetIndexName() );
					sComment.Format( "%s: fix index %s", pRecordInfoDesired->GetName(), fs.GetIndexName() );
					sCommand.Format( "DROP INDEX %s ON App%u.%s;\n", fs.GetIndexName(), GGCBase()->GetAppID(), pRecordInfoDesired->GetName() );
				}
				sCommand += GetAddIndexSQL( pRecordInfoDesired, fs );
			}
			nRet = YieldingProcessUnsafeConversion( eSchemaCatalog, sCommand, sComment );
			EXIT_ON_SQL_FAILURE( nRet );
		}

		// Just to be safe, let's run the old code, too.
		if ( !bIndexMismatch && !pRecordInfoActual->CompareIndexLists( pRecordInfoDesired ) )
		{
			EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\tIndex sets in table %s differ between specification and database [GC]:\n",
				pRecordInfoDesired->GetName() );
			CFmtStr1024 sTemp;

			pRecordInfoDesired->GetIndexFieldList( &sTemp, 3 );
			EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t\tDesired %s", sTemp.Access() );

			pRecordInfoActual->GetIndexFieldList( &sTemp, 3 );
			EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t\tActual %s", sTemp.Access() );
		}

		// what about foreign key constraints?
		if ( ! pRecordInfoActual->CompareFKs( pRecordInfoDesired ) )
		{
			EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\tForeign Key constraints in table %s differs between specification and database [GC]:\n",
				pRecordInfoDesired->GetName() );

			CFmtStr1024 sTemp;
			pRecordInfoDesired->GetFKListString( &sTemp, 3 );
			EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t\tDesired %s", sTemp.Access() );

			pRecordInfoActual->GetFKListString( &sTemp, 3 );
			EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t\tActual %s", sTemp.Access() );
		}



		if ( vecPColumnInfoActualUnknown.Count() > 0 )
		{
			EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\tColumns in database [GC] table %s that are not in specification (ignored):\n",
				pRecordInfoActual->GetName() );

			// Since this can actually destroy data, let's not ever, ever run it automatically.
			CUtlString sCommand;
			sCommand.Format( "ALTER TABLE App%u.%s DROP COLUMN ", GGCBase()->GetAppID(), pRecordInfoActual->GetName() );

			for ( int iColumn = 0; iColumn < vecPColumnInfoActualUnknown.Count(); iColumn++ )
			{
				const CColumnInfo *pColumnInfo = vecPColumnInfoActualUnknown[iColumn];
				EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t\t%s\n", pColumnInfo->GetName() );
				if ( iColumn != 0 )
					sCommand += ", ";
				sCommand += pColumnInfo->GetName();
			}
			AddDataDestroyingConversion( sCommand,
				CFmtStr( "-- Drop extra column(s) in %s\n", pRecordInfoActual->GetName() ) );
		}

		pSchemaUpdate->m_cColumnsDesiredMissing += vecPColumnInfoDesiredMissing.Count();
		pSchemaUpdate->m_cColumnsActualDifferent += vecPColumnInfoActualDifferent.Count();
		pSchemaUpdate->m_cColumnsActualUnknown += vecPColumnInfoActualUnknown.Count();
	}

	EmitAndAppend( sDetail, SPEW_GC, 2, 2, "[GC]: # of tables that currently exist in database but were unspecified: %d\n", 
		vecPRecordInfoActualUnknown.Count() );
	for ( int iTable = 0; iTable < vecPRecordInfoActualUnknown.Count(); iTable++ )
	{
		CRecordInfo *pRecordInfo = vecPRecordInfoActualUnknown[iTable];
		EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t%s\n", pRecordInfo->GetName() );
		AddDataDestroyingConversion(
			CFmtStr( "DROP TABLE App%u.%s\n", GGCBase()->GetAppID(), pRecordInfo->GetName() ),
			CFmtStr( "-- Drop extra table %s\n", pRecordInfo->GetName() ) );
	}

	// then, the triggers
	if ( vecTriggerInfoMissing.Count() > 0 )
	{
		EmitAndAppend( sDetail, SPEW_GC, 2, 2, "[GC]: # of specified triggers that do not currently exist: %d\n",
			vecTriggerInfoMissing.Count() );

		FOR_EACH_VEC( vecTriggerInfoMissing, iMissing )
		{
			CFastTimer tickCounter;
			tickCounter.Start();

			// Create the trigger
			nRet = YieldingCreateTrigger( eSchemaCatalog, vecTriggerInfoMissing[ iMissing] );
			EXIT_ON_SQL_FAILURE( nRet );
			tickCounter.End();
			int nElapsedMilliseconds = tickCounter.GetDuration().GetMilliseconds();
			EmitAndAppend( sDetail, SPEW_GC, 2, 2, "[GC]: Created trigger %s on table %s: %d millisec\n",
				vecTriggerInfoMissing[ iMissing ].m_szTriggerName,
				vecTriggerInfoMissing[ iMissing ].m_szTriggerTableName, nElapsedMilliseconds );
		}
	}

	// different triggers
	FOR_EACH_VEC( vecTriggerInfoDifferent, iDifferent )
	{
		EmitAndAppend( sDetail, SPEW_GC, 2, 2, "[GC]: Trigger %s on table %s differs from the desired trigger\n", vecTriggerInfoMissing[ iDifferent ].m_szTriggerName,
			vecTriggerInfoMissing[ iDifferent ].m_szTriggerTableName);

		// a different trigger text is a forced failure.
		nRet = SQL_ERROR;
	}

	// extra triggers
	FOR_EACH_VEC( vecTriggerInfoActual, iActual )
	{
		// if it was never matched, it isn't in the schema anywhere
		if ( ! vecTriggerInfoActual[ iActual ].m_bMatched )
		{
			SQLRETURN nSQLReturn = YieldingDropTrigger( eSchemaCatalog, vecTriggerInfoActual[ iActual ] );

			EmitAndAppend( sDetail, SPEW_GC, 2, 2, "[GC]: Trigger %s on table %s is not in the declared schema ... Drop %s",
				vecTriggerInfoActual[ iActual ].m_szTriggerName,
				vecTriggerInfoActual[ iActual ].m_szTriggerTableName,
				SQL_OK( nSQLReturn ) ? "OK" : "FAILED!" ) ;

			if ( !SQL_OK ( nSQLReturn ) )
			{
				// it broke; latch the failure
				nRet = nSQLReturn;
			}
		}
	}

	tickCounterOverall.End();
	EmitAndAppend( sDetail, SPEW_GC, 2, 2, "[GC]: Total time: %d milliseconds\n",
		tickCounterOverall.GetDuration().GetMilliseconds() );

Exit:

	// Spew suggested SQL to clean up stuff
	if ( !m_sRecommendedSQL.IsEmpty() || !m_sDataDestroyingCleanupSQL.IsEmpty() )
	{
		EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\tThe following SQL might work to fixup schema differences.\n" );
		EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t** \n" );
		EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t** DISCLAIMER ** This conversion code is not well tested.  Review the SQL and use at your own risk.\n" );
		EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t** \n" );
		{
			CUtlVectorAutoPurge<char*> vecLines;
			V_SplitString( m_sRecommendedSQL, "\n", vecLines );
			FOR_EACH_VEC( vecLines, i )
			{
				EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t\t%s\n", vecLines[i] );
			}
			m_sRecommendedSQL.Clear();
		}
		if ( !m_sDataDestroyingCleanupSQL.IsEmpty() )
		{
			EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t\t-- WARNING: The following operations will *destroy* data that is in the database but not in the specification.\n" );
			EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t\t--          If you have manually created extra tables or columns in your database, it will appear here!\n" );
			CUtlVectorAutoPurge<char*> vecLines;
			V_SplitString( m_sDataDestroyingCleanupSQL, "\n", vecLines );
			FOR_EACH_VEC( vecLines, i )
			{
				EmitAndAppend( sDetail, SPEW_GC, 2, 2, "\t\t%s\n", vecLines[i] );
			}
			m_sDataDestroyingCleanupSQL.Clear();
		}
	}

	pSchemaUpdate->m_cTablesDesiredMissing = vecPRecordInfoDesiredMissing.Count();
	pSchemaUpdate->m_cTablesActualDifferent = vecPRecordInfoActualDifferent.Count();
	pSchemaUpdate->m_cTablesActualUnknown = vecPRecordInfoActualUnknown.Count();

	FOR_EACH_MAP_FAST( mapPRecordInfoActual, iRecordInfoActual )
		SAFE_RELEASE( mapPRecordInfoActual[iRecordInfoActual] );

	return nRet;
}


//-----------------------------------------------------------------------------
// Purpose: Builds a record info for each table in database that describes the
//			columns in that table
// Input:	pSQLConnection - SQL connection to execute on
//			mapPRecordInfo - map to add the table record infos to
// Output:  SQL return code.
//-----------------------------------------------------------------------------
SQLRETURN CJobUpdateSchema::YieldingGetSchemaID( ESchemaCatalog eSchemaCatalog, int *pSchemaID )
{
	CSQLAccess sqlAccess( eSchemaCatalog );
	CFmtStr1024 sDefaultSchema;
	if( !sqlAccess.BYieldingExecuteString( FILE_AND_LINE, "SELECT default_schema_name FROM sys.database_principals WHERE name=CURRENT_USER",
		&sDefaultSchema ) )
		return SQL_ERROR;

	CFmtStr sExpectedDefaultSchema( "App%u", GGCBase()->GetAppID() );
	if ( 0 != Q_stricmp( sDefaultSchema, sExpectedDefaultSchema ) )
	{
		AssertMsg2( false, "SQL connection has the wrong default schema. Expected: %s. Actual %s", sExpectedDefaultSchema.Get(), sDefaultSchema.Get() );
		return SQL_ERROR;
	}

	sqlAccess.AddBindParam( sDefaultSchema );
	if( !sqlAccess.BYieldingExecuteScalarInt( FILE_AND_LINE, "SELECT SCHEMA_ID(?)", pSchemaID ) )
		return SQL_ERROR;

	return SQL_SUCCESS;
}


//-----------------------------------------------------------------------------
// Purpose: Builds a record info for each table in database that describes the
//			columns in that table
// Input:	pSQLConnection - SQL connection to execute on
//			mapPRecordInfo - map to add the table record infos to
// Output:  SQL return code.
//-----------------------------------------------------------------------------
SQLRETURN CJobUpdateSchema::YieldingGetRecordInfoForAllTables( ESchemaCatalog eSchemaCatalog, int nSchemaID, CMapPRecordInfo &mapPRecordInfo )
{
	CSQLAccess sqlAccess( eSchemaCatalog );

	// create a query that returns all tables in the database
	sqlAccess.AddBindParam( nSchemaID );
	RETURN_SQL_ERROR_ON_FALSE( sqlAccess.BYieldingExecute( FILE_AND_LINE, "SELECT object_id, name FROM sys.objects WHERE type_desc = 'USER_TABLE' AND is_ms_shipped = 0 AND schema_id = ?" ) );

	FOR_EACH_SQL_RESULT( sqlAccess, 0, tableIDRecord )
	{
		int nTableID;
		RETURN_SQL_ERROR_ON_FALSE( tableIDRecord.BGetIntValue( 0, &nTableID ) );

		CFmtStr1024 sTableName;
		RETURN_SQL_ERROR_ON_FALSE( tableIDRecord.BGetStringValue( 1, &sTableName) );

		YieldingGetColumnInfoForTable( eSchemaCatalog, mapPRecordInfo, nTableID, sTableName );
	}


	// now, get the column indexes and constraints for each table
	FOR_EACH_MAP_FAST( mapPRecordInfo, iRecordInfo )
	{
		CRecordInfo *pRecordInfo = mapPRecordInfo[iRecordInfo];
		pRecordInfo->SetAllColumnsAdded();

		// determine indexes and constraints
		YieldingGetColumnIndexes( eSchemaCatalog, pRecordInfo );

		// determine FK constraints
		YieldingGetTableFKConstraints( eSchemaCatalog, pRecordInfo );

		// do final calculations then ready to use
		pRecordInfo->PrepareForUse();
	}

	return SQL_SUCCESS;
}

SQLRETURN CJobUpdateSchema::YieldingGetColumnInfoForTable( ESchemaCatalog eSchemaCatalog, CMapPRecordInfo &mapPRecordInfo, int nTableID, const char *pchTableName )
{
	CSQLAccess sqlAccess( eSchemaCatalog );

	sqlAccess.AddBindParam( nTableID );
	RETURN_SQL_ERROR_ON_FALSE( sqlAccess.BYieldingExecute( FILE_AND_LINE, "SELECT name, column_id, user_type_id, max_length, is_identity FROM sys.columns WHERE object_id = ?") );

	CRecordInfo *pRecordInfo = CRecordInfo::Alloc();
	pRecordInfo->SetName( pchTableName );
	pRecordInfo->SetTableID( nTableID );
	FOR_EACH_SQL_RESULT( sqlAccess, 0, columnInfo )
	{
		CFmtStr1024 sColumnName;
		int nType;
		int nColumnID;
		int nMaxLength;
		bool bIdentity;
		RETURN_SQL_ERROR_ON_FALSE( columnInfo.BGetStringValue( 0, &sColumnName) );
		RETURN_SQL_ERROR_ON_FALSE( columnInfo.BGetIntValue( 1, &nColumnID ) );
		RETURN_SQL_ERROR_ON_FALSE( columnInfo.BGetIntValue( 2, &nType ) );
		RETURN_SQL_ERROR_ON_FALSE( columnInfo.BGetIntValue( 3, &nMaxLength ) );
		RETURN_SQL_ERROR_ON_FALSE( columnInfo.BGetBoolValue( 4, &bIdentity) );

		int nColFlags = 0;
		if( bIdentity )
			nColFlags |= k_nColFlagAutoIncrement;

		pRecordInfo->AddColumn( sColumnName, nColumnID, GetEGCSQLTypeForMSSQLType( nType ), nMaxLength, nColFlags, nMaxLength );
	}
	mapPRecordInfo.Insert( pRecordInfo->GetName(), pRecordInfo );

	return SQL_SUCCESS;
}


//-----------------------------------------------------------------------------
// purpose: get a list of triggers from the database.
//-----------------------------------------------------------------------------
SQLRETURN CJobUpdateSchema::YieldingGetTriggers( ESchemaCatalog eSchemaCatalog, int nSchemaID, CUtlVector< CTriggerInfo > &vecTriggerInfo )
{
	CSQLAccess sqlAccess( eSchemaCatalog );

	// get some description and the text of the triggers on this database.
	// Find the name of the trigger, the name of the table it servers, the type of
	// trigger, and its text. Doesn't bring back any disabled or MS-shipped triggers,
	// and gets only DML triggers and not DDL triggers.
	const char *pchStatement = "SELECT ST.name AS TriggerName, SOT.name AS TableName, ST.is_instead_of_trigger, SC.Text, SC.ColID"
		"  FROM sys.triggers AS ST"
		"  JOIN sys.syscomments AS SC ON SC.id = ST.object_id"
		"  JOIN sys.objects AS SO ON SO.object_id = ST.object_id"
		"  JOIN sys.objects AS SOT on SOT.object_id = ST.parent_id"
		" WHERE ST.type_desc = 'SQL_TRIGGER'"
		"   AND ST.is_ms_shipped = 0 AND ST.is_disabled = 0"
		"   AND ST.parent_class = 1"
		"   AND SO.schema_id = ?"
		" ORDER BY TableName, TriggerName, SC.ColID";
	sqlAccess.AddBindParam( nSchemaID );
	RETURN_SQL_ERROR_ON_FALSE( sqlAccess.BYieldingExecute( FILE_AND_LINE, pchStatement ) );

	// should be one results set
	Assert( 1 == sqlAccess.GetResultSetCount() );

	FOR_EACH_SQL_RESULT( sqlAccess, 0, sqlRecord )
	{
		// get the text of the procedure
		const char *pchText;
		DbgVerify( sqlRecord.BGetStringValue( 3, &pchText ) );

		// is this instead of?
		bool bIsInsteadOf;
		DbgVerify( sqlRecord.BGetBoolValue( 2, &bIsInsteadOf ) );

		// get the table name
		const char *pchTableName;
		DbgVerify( sqlRecord.BGetStringValue( 1, &pchTableName ) );

		// and the trigger name
		const char *pchTriggerName;
		DbgVerify( sqlRecord.BGetStringValue( 0, &pchTriggerName ) );

		// finally, grab the collation id
		int16 nColID;
		DbgVerify( sqlRecord.BGetInt16Value( 4, &nColID ) );

		if ( nColID == 1 )
		{
			// the collation ID is one, so we know this is a new one
			CTriggerInfo info;
			info.m_strText = pchText;
			Q_strncpy( info.m_szTriggerName, pchTriggerName, Q_ARRAYSIZE( info.m_szTriggerName ) );
			Q_strncpy( info.m_szTriggerTableName, pchTableName, Q_ARRAYSIZE( info.m_szTriggerTableName ) );
			info.m_eSchemaCatalog = eSchemaCatalog;
			vecTriggerInfo.AddToTail( info );
		}
		else
		{
			// the collation ID is not one, so we're concatenating.
			Assert( vecTriggerInfo.Count() - 1 >= 0 );

			// the name could not have changed
			Assert( 0 == Q_strcmp( vecTriggerInfo[vecTriggerInfo.Count() - 1].m_szTriggerName, pchTriggerName ) );
		}
	}

	return SQL_SUCCESS;
}


//-----------------------------------------------------------------------------
// Purpose: retrieves the index information for the specified table, then adds this
//			information to the record info. This is a SQL Server-specific implementation
//			which gets data describing index features not available through plain ODBC
//			queries.
// Input:	pSQLConnection - SQL connection to execute on
//			pRecordInfo - CRecordInfo to add the index info into
// Output:  SQL return code.
//-----------------------------------------------------------------------------
SQLRETURN CJobUpdateSchema::YieldingGetColumnIndexes( ESchemaCatalog eSchemaCatalog, CRecordInfo *pRecordInfo )
{
	// query the system management views for all of the indexes on this table
	static const char *pstrStatement = 
		"SELECT SI.Name AS INDEX_NAME, SC.Name AS COLUMN_NAME, SI.Type AS [TYPE], IS_INCLUDED_COLUMN, SIC.KEY_Ordinal AS [ORDINAL_POSITION], IS_UNIQUE, IS_PRIMARY_KEY, SI.INDEX_ID"
		"  FROM sys.indexes SI "
		"  JOIN sys.index_columns SIC"
		"		ON SIC.Object_id = SI.Object_Id"
		"			AND SIC.Index_ID = SI.Index_id"
		"  JOIN sys.objects SO"
		"		ON SIC.Object_ID = SO.Object_ID"
		"  JOIN sys.columns SC"
		"		ON SC.Object_ID = SO.Object_ID"
		"			AND SC.column_id = SIC.column_id"
		" WHERE SO.Object_ID = ? "
		"ORDER BY SIC.Index_id, SIC.Key_Ordinal ";
	CSQLAccess sqlAccess( eSchemaCatalog );
	sqlAccess.AddBindParam( pRecordInfo->GetTableID() );
	RETURN_SQL_ERROR_ON_FALSE( sqlAccess.BYieldingExecute( FILE_AND_LINE, pstrStatement ) );

	// builds a list of columns in a particular index.
	CUtlVector<int> vecColumns;
	CUtlVector<int> vecIncluded;
	int nLastIndexID = -1;
	bool bIsClustered = false;
	bool bIsIndexUnique = false;
	bool bIsPrimaryKey = false;
	CFmtStr1024 sIndexName, sColumnName;

	FOR_EACH_SQL_RESULT( sqlAccess, 0, typeRecord )
	{
		// Starting a new index?
		int nIndexID;
		DbgVerify( typeRecord.BGetIntValue( 7, &nIndexID ) );
		if ( nIndexID != nLastIndexID )
		{
			// first column! is it our first time through?
			if ( vecColumns.Count() > 0 )
			{
				// yes. let's add what we had from the previous index
				// to the fieldset
				FieldSet_t fs( bIsIndexUnique, bIsClustered, vecColumns, sIndexName );
				fs.AddIncludedColumns( vecIncluded );
				int idx = pRecordInfo->AddIndex( fs );
				if ( bIsPrimaryKey )
				{
					Assert( bIsIndexUnique );
					Assert( pRecordInfo->GetPKIndex() < 0 );
					pRecordInfo->SetPKIndex( idx );
				}

				// reset the vector
				vecColumns.RemoveAll();
				vecIncluded.RemoveAll();
				bIsIndexUnique = false;
				bIsClustered = false;
				bIsPrimaryKey = false;
			}
			nLastIndexID = nIndexID;
		}	

		int nTypeID, nOrdinalPosition;
		bool bIsIncluded, bIsColumnUnique;

		DbgVerify( typeRecord.BGetStringValue( 0, &sIndexName ) );
		DbgVerify( typeRecord.BGetStringValue( 1, &sColumnName ) );
		DbgVerify( typeRecord.BGetIntValue( 2, &nTypeID ) );
		DbgVerify( typeRecord.BGetBoolValue( 3, &bIsIncluded ) );
		DbgVerify( typeRecord.BGetIntValue( 4, &nOrdinalPosition ) );
		DbgVerify( typeRecord.BGetBoolValue( 5, &bIsColumnUnique ) );
		DbgVerify( typeRecord.BGetBoolValue( 6, &bIsPrimaryKey ) );

		RETURN_SQL_ERROR_ON_FALSE( sColumnName.Length() > 0 );

		int nColumnIndexed = -1;
		RETURN_SQL_ERROR_ON_FALSE( pRecordInfo->BFindColumnByName( sColumnName, &nColumnIndexed ) );

		CColumnInfo & columnInfo = pRecordInfo->GetColumnInfo( nColumnIndexed );
		int nColFlags = 0;

		if ( bIsIncluded )
		{
			Assert( nOrdinalPosition == 0 );
			// it's included; no flags
			vecIncluded.AddToTail( nColumnIndexed );
		}
		else
		{
			Assert( nOrdinalPosition != 0 );
			if ( bIsPrimaryKey )
			{
				// if we're working a primary key, mark those flags
				nColFlags = k_nColFlagPrimaryKey | k_nColFlagIndexed | k_nColFlagUnique;
				// PKs are always unique
				bIsIndexUnique = true;
			}
			else
			{
				// if we're working a "regular" index, we need to know the uniqueness of the index ...

				nColFlags = k_nColFlagIndexed;
				if ( bIsColumnUnique )
				{
					nColFlags |= k_nColFlagUnique;
					bIsIndexUnique = true;
				}
			}

			// clustering type
			if ( nTypeID == SQL_INDEX_CLUSTERED )
			{
				nColFlags |= k_nColFlagClustered;
				bIsClustered = true;
			}
			columnInfo.SetColFlagBits( nColFlags );

			// add this column to our list for the index set
			vecColumns.AddToTail( nColumnIndexed );
		}
	}

	// anything left over?
	if ( vecColumns.Count() > 0 )
	{
		// yep, add that, too
		FieldSet_t fs( bIsIndexUnique, bIsClustered, vecColumns, sIndexName );
		fs.AddIncludedColumns( vecIncluded );
		int idx = pRecordInfo->AddIndex( fs );
		if ( bIsPrimaryKey )
		{
			Assert( bIsIndexUnique );
			Assert( pRecordInfo->GetPKIndex() < 0 );
			pRecordInfo->SetPKIndex( idx );
		}
	}

	return SQL_SUCCESS;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the schema info on any FK constraints defined for the table
// Input:	pRecordInfo - CRecordInfo to add the index info into (and lookup table name out of)
// Output:  SQL return code.
//-----------------------------------------------------------------------------
SQLRETURN CJobUpdateSchema::YieldingGetTableFKConstraints( ESchemaCatalog eSchemaCatalog, CRecordInfo *pRecordInfo )
{
	// Used below, declared up here because of goto
	FKData_t fkData;

	CSQLAccess sqlAccess( eSchemaCatalog );
	const char *pchStatement =  "SELECT fk.name AS FKName, current_table.name AS TableName, parent_table.name AS ParentTableName, "
		"table_column.name AS ColName, parent_table_column.name AS ParentColName, "
		"fk.delete_referential_action_desc AS OnDelete, "
		"fk.update_referential_action_desc AS OnUpdate "
		"FROM sys.objects AS current_table "
		"JOIN sys.foreign_keys AS fk ON fk.parent_object_id=current_table.object_id AND fk.is_ms_shipped=0 "
		"JOIN sys.foreign_key_columns AS fk_col ON fk_col.constraint_object_id=fk.object_id "
		"JOIN sys.objects AS parent_table ON parent_table.object_id=fk_col.referenced_object_id "
		"JOIN sys.columns AS table_column ON table_column.object_id=fk_col.parent_object_id AND table_column.column_id=fk_col.parent_column_id "
		"JOIN sys.columns AS parent_table_column ON parent_table_column.object_id=fk_col.referenced_object_id AND parent_table_column.column_id=fk_col.referenced_column_id "
		"WHERE current_table.object_id = ? ORDER BY fk.name";
	sqlAccess.AddBindParam( pRecordInfo->GetTableID() );
	RETURN_SQL_ERROR_ON_FALSE( sqlAccess.BYieldingExecute( FILE_AND_LINE, pchStatement ) );

	FOR_EACH_SQL_RESULT( sqlAccess, 0, sqlRecord )
	{
		// get all the data for the FK
		const char *pchFKName;
		DbgVerify( sqlRecord.BGetStringValue( 0, &pchFKName ) );

		const char *pchTableName;
		DbgVerify( sqlRecord.BGetStringValue( 1, &pchTableName ) );

		AssertMsg( Q_stricmp( pchTableName, pRecordInfo->GetName() ) == 0, "FOREIGN KEY schema conversion found FK for table not matching search!\n" );

		const char *pchParentTable;
		DbgVerify( sqlRecord.BGetStringValue( 2, &pchParentTable ) );

		const char *pchColName;
		DbgVerify( sqlRecord.BGetStringValue( 3, &pchColName ) );

		const char *pchParentColName;
		DbgVerify( sqlRecord.BGetStringValue( 4, &pchParentColName ) );

		const char *pchOnDelete;
		DbgVerify( sqlRecord.BGetStringValue( 5, &pchOnDelete ) );

		const char *pchOnUpdate;
		DbgVerify( sqlRecord.BGetStringValue( 6, &pchOnUpdate ) );

		// Is this more data for the FK we are already tracking?  If so just append the column data,
		// otherwise, assuming some data exists, add the key to the record info
		if ( Q_strcmp( fkData.m_rgchName, pchFKName ) == 0 )
		{
			int iColRelation = fkData.m_VecColumnRelations.AddToTail();
			FKColumnRelation_t &colRelation = fkData.m_VecColumnRelations[iColRelation];
			Q_strncpy( colRelation.m_rgchCol, pchColName, Q_ARRAYSIZE( colRelation.m_rgchCol ) );
			Q_strncpy( colRelation.m_rgchParentCol, pchParentColName, Q_ARRAYSIZE( colRelation.m_rgchParentCol ) );
		}
		else 
		{
			if ( Q_strlen( fkData.m_rgchName ) )
			{	
				pRecordInfo->AddFK( fkData  );
			}

			// Do initial setup of the new key
			Q_strncpy( fkData.m_rgchName, pchFKName, Q_ARRAYSIZE( fkData.m_rgchName ) );
			Q_strncpy( fkData.m_rgchParentTableName, pchParentTable, Q_ARRAYSIZE( fkData.m_rgchParentTableName ) );
			fkData.m_eOnDeleteAction = EForeignKeyActionFromName( pchOnDelete ); 
			fkData.m_eOnUpdateAction = EForeignKeyActionFromName( pchOnUpdate );
			int iColRelation = fkData.m_VecColumnRelations.AddToTail();
			FKColumnRelation_t &colRelation = fkData.m_VecColumnRelations[iColRelation];
			Q_strncpy( colRelation.m_rgchCol, pchColName, Q_ARRAYSIZE( colRelation.m_rgchCol ) );
			Q_strncpy( colRelation.m_rgchParentCol, pchParentColName, Q_ARRAYSIZE( colRelation.m_rgchParentCol ) );
		}
	}


	// Add the last key we were building data for
	if ( Q_strlen( fkData.m_rgchName ) )
	{
		pRecordInfo->AddFK( fkData  );
	}

	return SQL_SUCCESS;
}


//-----------------------------------------------------------------------------
// Purpose: Creates a table in the DB to match the recordinfo
// Input:	pRecordInfo - CRecordInfo to create a table for
// Output:  SQL return code.
//-----------------------------------------------------------------------------
SQLRETURN CJobUpdateSchema::YieldingCreateTable( ESchemaCatalog eSchemaCatalog, CRecordInfo *pRecordInfo )
{
	CFmtStrMax sCmd;
	SQLRETURN nRet = 0;
	const char *pchName = pRecordInfo->GetName();
	Assert( pchName && pchName[0] );
	CSQLAccess sqlAccess( eSchemaCatalog );

	// build the create table command
	sCmd.sprintf( "CREATE TABLE %s (", pchName );

	// add all the columns
	for ( int iColumn = 0; iColumn < pRecordInfo->GetNumColumns(); iColumn++ )
	{
		char rgchType[k_cSmallBuff];
		const CColumnInfo &columnInfo = pRecordInfo->GetColumnInfo( iColumn );
		char *pchType = SQLTypeFromField( columnInfo, rgchType, Q_ARRAYSIZE( rgchType ) );
		Assert( pchType[0] );

		// add the name and type of this column
		sCmd.AppendFormat( "%s %s", columnInfo.GetName(), pchType );

		// add any constraints
		AppendConstraints( pRecordInfo, &columnInfo, true, sCmd );

		if ( iColumn < ( pRecordInfo->GetNumColumns() - 1 ) )
			sCmd += ", ";
	}

	AppendTableConstraints( pRecordInfo, sCmd );
	sCmd += ")";


	// create the table
	// metadata operations aren't transactional, so we'll set bAutoTransaction = true
	EXIT_ON_BOOLSQL_FAILURE( sqlAccess.BYieldingExecute( FILE_AND_LINE, sCmd ) );

	// add any indexes
	for ( int n = 0; n < pRecordInfo->GetIndexFields( ).Count(); n++ )
	{
		const FieldSet_t& refSet = pRecordInfo->GetIndexFields( )[ n ];

		// already added the PK index, so skip it
		if ( n == pRecordInfo->GetPKIndex() )
			continue;

		// call YieldingAddIndex to do the work
		nRet = YieldingAddIndex( eSchemaCatalog, pRecordInfo, refSet );
		EXIT_ON_SQL_FAILURE( nRet );
	}

Exit:
	return nRet;
}

//-----------------------------------------------------------------------------
// Purpose: Adds an index of multiple columns to a table.
// Input:	pSQLConnection - connection to use for command
//			pRecordInfo - record info describing table
//			refFields - description of index to add
// Output:  SQL return code.
//-----------------------------------------------------------------------------
SQLRETURN CJobUpdateSchema::YieldingAddIndex( ESchemaCatalog eSchemaCatalog, CRecordInfo *pRecordInfo, const FieldSet_t &refFields )
{
	CSQLAccess sqlAccess( eSchemaCatalog );
	CUtlString sCmd = GetAddIndexSQL( pRecordInfo, refFields );
	RETURN_SQL_ERROR_ON_FALSE( sqlAccess.BYieldingExecute( FILE_AND_LINE, sCmd ) );
	return SQL_SUCCESS;
}


//-----------------------------------------------------------------------------
// Purpose: Depending on the conversion mode, either really perfom the
//          conversion, or just log the SQL text so a human being can review
//          it and do it later.
//-----------------------------------------------------------------------------

SQLRETURN CJobUpdateSchema::YieldingProcessUnsafeConversion( ESchemaCatalog eSchemaCatalog, const char *pszSQL, const char *pszComment )
{
	if ( k_EConversionModeConvertIrreversible != m_eConversionMode )
	{
		if ( pszComment )
		{
			m_sRecommendedSQL.Append( "-- " );
			m_sRecommendedSQL.Append( pszComment );
			m_sRecommendedSQL.Append( "\n" );
		}
		m_sRecommendedSQL.Append( pszSQL );
		m_sRecommendedSQL.Append("\nGO\n \n"); // that space is a kludge, because we are using V_splitlines, which will ignore consecutive seperators
		return SQL_SUCCESS;
	}
	CSQLAccess sqlAccess( eSchemaCatalog );
	RETURN_SQL_ERROR_ON_FALSE( sqlAccess.BYieldingExecute( FILE_AND_LINE, pszSQL ) );
	return SQL_SUCCESS;
}

//-----------------------------------------------------------------------------
// Purpose: Add a SQL command to cleanup the schema that will actually destroy (supposedly unused) data.
//-----------------------------------------------------------------------------

void CJobUpdateSchema::AddDataDestroyingConversion( const char *pszSQL, const char *pszComment )
{
	if ( pszComment )
	{
		m_sDataDestroyingCleanupSQL.Append( "-- " );
		m_sDataDestroyingCleanupSQL.Append( pszComment );
		m_sDataDestroyingCleanupSQL.Append( "\n" );
	}
	m_sDataDestroyingCleanupSQL.Append( pszSQL );
	m_sDataDestroyingCleanupSQL.Append("\nGO\n \n"); // that space is a kludge, because we are using V_splitlines, which will ignore consecutive seperators
}

//-----------------------------------------------------------------------------
// Purpose: Adds a column to a table
// Input:	pRecordInfo - record info describing table to add a column to
//			pColumnInfo - column info describing column to add
// Output:  SQL return code.
//-----------------------------------------------------------------------------
SQLRETURN CJobUpdateSchema::YieldingAlterTableAddColumn( ESchemaCatalog eSchemaCatalog, CRecordInfo *pRecordInfo, const CColumnInfo *pColumnInfo )
{
	CSQLAccess sqlAccess( eSchemaCatalog );
	CFmtStrMax sCmd;
	char rgchType[k_cSmallBuff];
	const char *pchTableName = pRecordInfo->GetName();
	Assert( pchTableName );
	const char *pchColumnName = pColumnInfo->GetName();
	Assert( pchColumnName );
	DbgVerify( SQLTypeFromField( *pColumnInfo, rgchType, Q_ARRAYSIZE( rgchType ) )[0] );

	// build the alter table command
	sCmd.sprintf( "ALTER TABLE %s ADD %s %s", pchTableName, pchColumnName, rgchType );

	// add any constraints
	AppendConstraints( pRecordInfo, pColumnInfo, true, sCmd );

	// !KLUDGE! This is guaranteed to fail if it has "not null" in it.
	if ( V_strstr( sCmd, "NOT NULL" ) )
	{
		EmitError( SPEW_SQL, "Cannot add column %s to table %s with NOT NULL constraint\n", pchColumnName, pchTableName );
		EmitInfo( SPEW_SQL, SPEW_ALWAYS, LOG_ALWAYS, "Here is the SQL that should be run to add the column:\n" );
		EmitInfo( SPEW_SQL, SPEW_ALWAYS, LOG_ALWAYS, "  %s\n", sCmd.String() );
		return SQL_ERROR;
	}

	// execute the command.
	RETURN_SQL_ERROR_ON_FALSE( sqlAccess.BYieldingExecute( FILE_AND_LINE, sCmd ) );
	return SQL_SUCCESS;
}


//-----------------------------------------------------------------------------
// Purpose: Adds a constraint to an existing column
// Input:	hDBC - SQL connection to execute on
//			pRecordInfo - record info describing table
//			pColumnInfo - column info describing column to add contraint for
//			nColFlagConstraint - constraint to add
// Output:  SQL return code.
//-----------------------------------------------------------------------------
SQLRETURN CJobUpdateSchema::YieldingAddConstraint( ESchemaCatalog eSchemaCatalog, CRecordInfo *pRecordInfo, const CColumnInfo *pColumnInfo, int nColFlagConstraint )
{
	Assert( pRecordInfo );
	Assert( pColumnInfo );

	CFmtStrMax sCmd;
	sCmd.sprintf( "ALTER TABLE App%u.%s ADD", GGCBase()->GetAppID(), pRecordInfo->GetName() );

	Assert( !pColumnInfo->BIsClustered() );
	AppendConstraint( pRecordInfo->GetName(), pColumnInfo->GetName(), nColFlagConstraint, true, pColumnInfo->BIsClustered(), sCmd, 0 );

	sCmd.AppendFormat( " (%s)", pColumnInfo->GetName() );

	return YieldingProcessUnsafeConversion( eSchemaCatalog, sCmd );
}


//-----------------------------------------------------------------------------
// Purpose: Changes type or length of existing column
// Input:	hDBC - SQL connection to execute on
//			pRecordInfo - record info describing table
//			pColumnInfoDesired - column info describing new column type or length
// Output:  SQL return code.
//-----------------------------------------------------------------------------
SQLRETURN CJobUpdateSchema::YieldingChangeColumnTypeOrLength( ESchemaCatalog eSchemaCatalog, CRecordInfo *pRecordInfo, const CColumnInfo *pColumnInfoDesired )
{
	CUtlString sCmd = GetAlterColumnText( pRecordInfo, pColumnInfoDesired );
	return YieldingProcessUnsafeConversion( eSchemaCatalog, sCmd );
}


//-----------------------------------------------------------------------------
// Purpose: Changes constraints/indexes on a column
// Input:	hDBC - SQL connection to execute on
//			pRecordInfo - record info describing table
//			pColumnInfoActual - column info describing existing column properties
//			pColumnInfoActual - column info describing desired new column properties
// Output:  SQL return code.
//-----------------------------------------------------------------------------
SQLRETURN CJobUpdateSchema::YieldingChangeColumnProperties( ESchemaCatalog eSchemaCatalog, CRecordInfo *pRecordInfo, const CColumnInfo *pColumnInfoActual, 
											 const CColumnInfo *pColumnInfoDesired )
{
	CSQLAccess sqlAccess( eSchemaCatalog );
	Assert( pRecordInfo );
	Assert( pColumnInfoActual );
	Assert( pColumnInfoDesired );

	SQLRETURN nRet = SQL_SUCCESS;

	pColumnInfoActual->ValidateColFlags();
	pColumnInfoDesired->ValidateColFlags();

	Assert( pColumnInfoActual->GetColFlags() != pColumnInfoDesired->GetColFlags() );

	// all the operations we might have to perform; note we have have to drop one
	// thing and add another
	bool bAddExplicitIndex = false, bRemoveExplicitIndex = false;
	bool bAddUniqueConstraint = false, bRemoveUniqueConstraint = false;
	bool bAddPrimaryKeyConstraint = false, bRemovePrimaryKeyConstraint = false;

	// determine which operations need to be performed
	if ( !pColumnInfoActual->BIsPrimaryKey() && pColumnInfoDesired->BIsPrimaryKey() )
	{
		bAddPrimaryKeyConstraint = true;
	}
	else if ( pColumnInfoActual->BIsPrimaryKey() && !pColumnInfoDesired->BIsPrimaryKey() )
	{
		bRemovePrimaryKeyConstraint = true;
	}

	if ( !pColumnInfoActual->BIsExplicitlyUnique() && pColumnInfoDesired->BIsExplicitlyUnique() )
	{
		bAddUniqueConstraint = true;
	}
	else if ( pColumnInfoActual->BIsExplicitlyUnique() && !pColumnInfoDesired->BIsExplicitlyUnique() )
	{
		bRemoveUniqueConstraint = true;
	}

	if ( !pColumnInfoActual->BIsExplicitlyIndexed() && pColumnInfoDesired->BIsExplicitlyIndexed() )
	{
		bAddExplicitIndex = true;
	}
	else if ( pColumnInfoActual->BIsExplicitlyIndexed() && !pColumnInfoDesired->BIsExplicitlyIndexed() )
	{
		bRemoveExplicitIndex = true;
	}

	// sanity check 
	Assert( !( bAddUniqueConstraint && bAddPrimaryKeyConstraint ) );	// primary key constraint adds implicit uniqueness constraint; it's a bug if we decide to do both
	Assert( !( bAddUniqueConstraint && bAddExplicitIndex ) );	// uniqueness constraint adds implicit index; it's a bug if we decide to do both
	Assert( !( bAddPrimaryKeyConstraint && bAddExplicitIndex ) );	// primary key constraint adds implicit index; it's a bug if we decide to do both

	// The index conversion stuff already handles dropping of unexpected indices
	// and primary key constraints.  I'm not even sure that this works or is used anymore.
	if ( bAddUniqueConstraint )
	{
		nRet = YieldingAddConstraint( eSchemaCatalog, pRecordInfo, pColumnInfoActual, k_nColFlagUnique );
		EXIT_ON_SQL_FAILURE( nRet );
	}

Exit:
	return nRet;
}




//-----------------------------------------------------------------------------
// Purpose: Creates specified trigger
// Input:	pSQLConnection - SQL connection to execute on
//			refTriggerInfo - trigger to be created
// Output:  SQL return code.
//-----------------------------------------------------------------------------
SQLRETURN CJobUpdateSchema::YieldingCreateTrigger( ESchemaCatalog eSchemaCatalog, CTriggerInfo &refTriggerInfo )
{
	char rgchCmd[ k_cchSQLStatementTextMax];
	CSQLAccess sqlAccess( eSchemaCatalog );

	// build the create command
	Q_snprintf( rgchCmd, Q_ARRAYSIZE( rgchCmd ),
		"CREATE TRIGGER [%s] ON [%s] "
		"%s "
		"AS BEGIN"
		"%s\n"
		"END",
		refTriggerInfo.m_szTriggerName,
		refTriggerInfo.m_szTriggerTableName,
		refTriggerInfo.GetTriggerTypeString(), 
		refTriggerInfo.m_strText.Get() );

	RETURN_SQL_ERROR_ON_FALSE( sqlAccess.BYieldingExecute( FILE_AND_LINE, rgchCmd ) );
	return SQL_SUCCESS;
}


//-----------------------------------------------------------------------------
// Purpose: drops specified trigger
// Input:	pSQLConnection - SQL connection to execute on
//			refTriggerInfo - trigger to be dropped
// Output:  SQL return code.
//-----------------------------------------------------------------------------
SQLRETURN CJobUpdateSchema::YieldingDropTrigger( ESchemaCatalog eSchemaCatalog, CTriggerInfo &refTriggerInfo )
{
	char rgchCmd[ k_cchSQLStatementTextMax];
	CSQLAccess sqlAccess( eSchemaCatalog );

	// build the create command
	Q_snprintf( rgchCmd, Q_ARRAYSIZE( rgchCmd ),
		"DROP TRIGGER [%s];",
		refTriggerInfo.m_szTriggerName );

	RETURN_SQL_ERROR_ON_FALSE( sqlAccess.BYieldingExecute( FILE_AND_LINE, rgchCmd ) );
	return SQL_SUCCESS;
}



//-----------------------------------------------------------------------------
// Purpose: Used for SQL/EGCSQLType conversion
//-----------------------------------------------------------------------------
struct SQLTypeMapping_t
{
	const char *m_pchTypeName;
	EGCSQLType m_eType;
};

static SQLTypeMapping_t g_rSQLTypeMapping[] =
{
	{ "tinyint", k_EGCSQLType_int8 },
	{ "smallint", k_EGCSQLType_int16 },
	{ "int", k_EGCSQLType_int32 },
	{ "bigint", k_EGCSQLType_int64 },
	{ "real", k_EGCSQLType_float },
	{ "float", k_EGCSQLType_double },
	{ "text", k_EGCSQLType_String },
	{ "ntext", k_EGCSQLType_String },
	{ "char", k_EGCSQLType_String },
	{ "varchar", k_EGCSQLType_String },
	{ "nchar", k_EGCSQLType_String },
	{ "nvarchar", k_EGCSQLType_String },
	{ "sysname", k_EGCSQLType_String },
	{ "varbinary", k_EGCSQLType_Blob },
	{ "image", k_EGCSQLType_Image },
};
static uint32 g_cSQLTypeMapping = Q_ARRAYSIZE( g_rSQLTypeMapping );


//-----------------------------------------------------------------------------
// Purpose: Returns the EGCSQLType for the specified SQL type name (or Invalid 
//			for unsupported types.)
//-----------------------------------------------------------------------------
EGCSQLType ETypeFromMSSQLDataType( const char *pchType )
{
	for( uint32 unMapping = 0; unMapping < g_cSQLTypeMapping; unMapping++ )
	{
		if( !Q_stricmp( pchType, g_rSQLTypeMapping[ unMapping ].m_pchTypeName ) )
			return g_rSQLTypeMapping[ unMapping ].m_eType;
	}
	return k_EGCSQLTypeInvalid;
}


//-----------------------------------------------------------------------------
// Purpose: Prepares the type map for use in schema upgrades
//-----------------------------------------------------------------------------
bool CJobUpdateSchema::YieldingBuildTypeMap( ESchemaCatalog eSchemaCatalog )
{
	CSQLAccess sqlAccess( eSchemaCatalog );
	if( !sqlAccess.BYieldingExecute( FILE_AND_LINE, "select name, system_type_id from sys.types" ) )
		return false;

	FOR_EACH_SQL_RESULT( sqlAccess, 0, typeRecord )
	{
		CFmtStr1024 sTypeName;
		byte nTypeID;
		DbgVerify( typeRecord.BGetStringValue( 0, &sTypeName ) );
		DbgVerify( typeRecord.BGetByteValue( 1, &nTypeID ) );

		EGCSQLType eType = ETypeFromMSSQLDataType( sTypeName );
		if( eType != k_EGCSQLTypeInvalid )
			m_mapSQLTypeToEType.Insert( nTypeID, eType );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Returns the EGCSQLType for the specified type ID out of the database
//-----------------------------------------------------------------------------
EGCSQLType CJobUpdateSchema::GetEGCSQLTypeForMSSQLType( int nType )
{
	int nIndex = m_mapSQLTypeToEType.Find( nType );
	if( m_mapSQLTypeToEType.IsValidIndex( nIndex ) )
		return m_mapSQLTypeToEType[ nIndex ];
	else
		return k_EGCSQLTypeInvalid;
}

} // namespace GCSDK
