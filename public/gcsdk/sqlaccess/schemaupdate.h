//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains the job that's responsible for updating the database schema
//
//=============================================================================
#ifndef UPDATESCHEMA_H
#define UPDATESCHEMA_H
#ifdef _WIN32
#pragma once
#endif

namespace GCSDK
{
typedef CUtlMap<const char *,CRecordInfo *> CMapPRecordInfo;

enum EConversionMode
{
	k_EConversionModeInspectOnly,
	k_EConversionModeConvertSafe,
	k_EConversionModeConvertIrreversible
};

class CSchemaUpdate : public CRefCount
{
public:
	CSchemaUpdate();

	void AddRecordInfoDesired( CRecordInfo *pRecordInfo );
	void AddFTSInfo( const CFTSCatalogInfo &refFTSInfo );
	void AddTriggerInfos( const CUtlVector< CTriggerInfo > &refTriggerInfo );

	// input parameters
	CMapPRecordInfo m_mapPRecordInfoDesired;
	EConversionMode m_eConversionMode;
	CUtlLinkedList< CFTSCatalogInfo > m_listFTSCatalogInfo;
	CUtlVector< CTriggerInfo > m_vecTriggerInfo;

	// output parameters
	bool m_bConversionNeeded;
	bool m_bSkippedAChange;
	int	 m_cTablesDesiredMissing;
	int	 m_cTablesActualDifferent;
	int	 m_cTablesActualUnknown;
	int	 m_cTablesNeedingChange;
	int	 m_cColumnsDesiredMissing;
	int	 m_cColumnsActualDifferent;
	int	 m_cColumnsActualUnknown;

	CFmtStr1024 m_sDetail;

private:
	virtual ~CSchemaUpdate();
};

// --------------------------------------------------------------------------

class CJobUpdateSchema : public CGCJob
{
public:
	CJobUpdateSchema( CGCBase *pGC, int iTableCount ) : CGCJob( pGC ), m_mapSQLTypeToEType( DefLessFunc(int) ), m_iTableCount( iTableCount ) { }
	bool BYieldingRunJob( void * );
private:
	bool BYieldingUpdateSchema( ESchemaCatalog eSchemaCatalog );
	SQLRETURN YieldingEnsureDatabaseSchemaCorrect( ESchemaCatalog eSchemaCatalog, CSchemaUpdate *pSchemaUpdate );
	EGCSQLType GetEGCSQLTypeForMSSQLType( int nType );
	bool YieldingBuildTypeMap( ESchemaCatalog eSchemaCatalog );
	SQLRETURN YieldingGetSchemaID( ESchemaCatalog eSchemaCatalog, int *pSchemaID );
	SQLRETURN YieldingGetRecordInfoForAllTables( ESchemaCatalog eSchemaCatalog, int nSchemaID, CMapPRecordInfo &mapPRecordInfo );
	SQLRETURN YieldingGetColumnInfoForTable( ESchemaCatalog eSchemaCatalog, CMapPRecordInfo &mapPRecordInfo, int nTableID, const char *pchTableName );
	SQLRETURN YieldingGetTableFKConstraints( ESchemaCatalog eSchemaCatalog, CRecordInfo *pRecordInfo );
	SQLRETURN YieldingGetColumnIndexes( ESchemaCatalog eSchemaCatalog, CRecordInfo *pRecordInfo );
	SQLRETURN YieldingGetTriggers( ESchemaCatalog eSchemaCatalog, int nSchemaID, CUtlVector< CTriggerInfo > &vecTriggerInfo );
	SQLRETURN YieldingCreateTable( ESchemaCatalog eSchemaCatalog, CRecordInfo *pRecordInfo );
	SQLRETURN YieldingAddIndex( ESchemaCatalog eSchemaCatalog, CRecordInfo *pRecordInfo, const FieldSet_t &refFields );
	SQLRETURN YieldingAlterTableAddColumn( ESchemaCatalog eSchemaCatalog, CRecordInfo *pRecordInfo, const CColumnInfo *pColumnInfo );
	SQLRETURN YieldingAddConstraint( ESchemaCatalog eSchemaCatalog, CRecordInfo *pRecordInfo, const CColumnInfo *pColumnInfo, int nColFlagConstraint );
	SQLRETURN YieldingChangeColumnTypeOrLength( ESchemaCatalog eSchemaCatalog, CRecordInfo *pRecordInfo, const CColumnInfo *pColumnInfoDesired );
	SQLRETURN YieldingChangeColumnProperties( ESchemaCatalog eSchemaCatalog, CRecordInfo *pRecordInfo, const CColumnInfo *pColumnInfoActual, const CColumnInfo *pColumnInfoDesired );
	SQLRETURN YieldingCreateTrigger( ESchemaCatalog eSchemaCatalog, CTriggerInfo &refTriggerInfo );
	SQLRETURN YieldingDropTrigger( ESchemaCatalog eSchemaCatalog, CTriggerInfo &refTriggerInfo );

	CUtlMap<int,EGCSQLType> m_mapSQLTypeToEType;
	int m_iTableCount;

	EConversionMode m_eConversionMode;

	CUtlString m_sRecommendedSQL;
	CUtlString m_sDataDestroyingCleanupSQL;
	void AddDataDestroyingConversion( const char *pszSQL, const char *pszComment );
	SQLRETURN YieldingProcessUnsafeConversion( ESchemaCatalog eSchemaCatalog, const char *pszSQL, const char *pszComment = NULL );
};


} // namespace GCSDK
#endif // UPDATESCHEMA_H
