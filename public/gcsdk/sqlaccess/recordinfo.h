//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef GCRECORDINFO_H
#define GCRECORDINFO_H

namespace GCSDK
{

typedef CUtlMap<const char *,int> CMapIColumnInfo;

// --------------------------------------------------------------------------
// Information about a column in a record (table or result set)
class CColumnInfo
{
public:
	CColumnInfo();
	~CColumnInfo() { }

	void Set( const char *pchName, int nSQLColumn, EGCSQLType eGCSQLType, int cubFixedSize, int nColFlags, int cubMaxSize );
	const char *GetName() const { return m_rgchName; }
	int GetSQLColumn() const { return m_nSQLColumn; }
	EGCSQLType GetType() const { return m_eType; }
	int GetFixedSize() const { return m_cubFixedSize; }
	int GetMaxSize() const { return m_cchMaxSize; }
	int	GetChecksum() const { Assert( m_bHaveChecksum ); return m_nChecksum; }
	bool BIsVariableLength() const;
	int GetColFlags() const { return m_nColFlags; }
	void GetColFlagDescription( char* pstrOut, int cubOutLength ) const;
	int GetConstraintColFlags() { return m_nColFlags & k_nColFlagAllConstraints; }
	void SetColFlagBits( int nColFlag );
	bool BIsIndexed() const { return 0 != ( m_nColFlags & k_nColFlagIndexed ); }
	bool BIsClustered() const { return 0 != ( m_nColFlags & k_nColFlagClustered ); }
	bool BIsUnique() const { return 0 != ( m_nColFlags & k_nColFlagUnique ); }
	bool BIsAutoIncrement()	const { return 0 != ( m_nColFlags & k_nColFlagAutoIncrement ); }
	bool BIsPrimaryKey() const { return 0 != ( m_nColFlags & k_nColFlagPrimaryKey ); }
	bool BIsExplicitlyIndexed() const { return BIsIndexed() && !( BIsPrimaryKey() || BIsUnique() ); }
	bool BIsExplicitlyUnique() const { return BIsUnique() && !BIsPrimaryKey(); }
	bool BIsInsertable() const { return !BIsAutoIncrement(); }
	void CalculateChecksum();
	void ValidateColFlags() const;
	bool operator==( const CColumnInfo& refOther ) const;
	bool operator!=( const CColumnInfo& refOther ) const
	{
		return ! operator==( refOther );
	}

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
#endif // DBGFLAG_VALIDATE
private:
	CColumnInfo( CColumnInfo& ); // no copy constructor, disable default copy constructor
	CColumnInfo& operator = ( CColumnInfo& );	// no assignment operator, disable default assignment operator
	char	m_rgchName[k_cSQLObjectNameMax+1];

	EGCSQLType	m_eType;		// GC-based enum data type of this column
	int		m_nColFlags;			// flags for this column
	int		m_nSQLColumn;			// column # in SQL database to bind to, starts at 1.  
	int		m_cubFixedSize;			// if fixed size, the fixed size in bytes; else 0
	int		m_cchMaxSize;			// if variable size, the maximum size; else 0
	int		m_nChecksum;			// checksum of this column info for quick comparisons
	bool	m_bHaveChecksum;		// have we calculated a checksum yet?
};

// --------------------------------------------------------------------------
// Information about a record (table or result set)
class CRecordInfo : public CRefCount
{
public:
	CRecordInfo();

	void InitFromDSSchema( CSchema *pSchema );

	void SetName( const char *pchName );
	const char *GetName() const { return m_rgchName; }
	void AddColumn( const char *pchName, int nSQLColumn, EGCSQLType eGCSQLType, int cubFixedSize, int nColFlags, int cubMaxSize );
	void SetAllColumnsAdded() { m_bAllColumnsAdded = true; }
	void PrepareForUse();
	int GetFixedSize() const { return m_cubFixedSize; }
	int GetNumColumns() const { return m_VecColumnInfo.Count(); }
	const CColumnInfo &GetColumnInfo( uint32 unColumn ) const { return m_VecColumnInfo[unColumn]; }
	CColumnInfo &GetColumnInfo( uint32 unColumn ) { return m_VecColumnInfo[unColumn]; }
	bool BFindColumnByName( const char *pchName, int *piColumn );
	bool BPreparedForUse() const { return m_bPreparedForUse; }
	void EnsureCapacity( int cColumns ) { m_VecColumnInfo.EnsureCapacity( cColumns ); }
	int	GetChecksum();
	ESchemaCatalog GetESchemaCatalog() const { return m_eSchemaCatalog; }
	void SetESchemaCatalog( ESchemaCatalog e ) { m_eSchemaCatalog = e; }
	bool EqualTo( CRecordInfo* pOther );
	bool CompareIndexLists( CRecordInfo *pOther );
	bool CompareFKs( CRecordInfo *pOther );
	bool CompareFTSIndexLists( CRecordInfo *pOther ) const;
	EPrimaryKeyType GetPrimaryKeyType() const { return m_nHasPrimaryKey; }
	bool BHasPrimaryKey() { return GetPrimaryKeyType() != k_EPrimaryKeyTypeNone; }
	const FieldSet_t& GetPKFields() { Assert( BHasPrimaryKey()); return GetIndexFields( )[ m_iPKIndex ]; }
	const CUtlVector<FieldSet_t>& GetIndexFields() const { return m_VecIndexes; }
	int GetIndexFieldCount() const { return m_VecIndexes.Count(); }
	int FindIndex( CRecordInfo *pRec, const FieldSet_t& fieldSet );
	int FindIndexByName( const char *pszName ) const;
	int GetPKIndex() const { return m_iPKIndex; }
	void SetPKIndex( int i ) { m_iPKIndex = i; }
	int AddIndex( const FieldSet_t& fieldSet );
	void GetIndexFieldList( CFmtStr1024 *pstr, int nIndents ) const;
	int GetTableID() const { return m_nTableID; }
	void SetTableID( int nTableID ) { m_nTableID = nTableID; }
	bool BHasIdentity() const;

	// full-text index
	CUtlVector<int> & GetFTSFields() { return m_vecFTSFields; }
	bool BHasFTSIndex() const { return m_vecFTSFields.Count() > 0; }
	void AddFTSFields( CUtlVector< int > &refVecFields );
	int GetFullTextCatalogIndex() { return m_nFullTextCatalogIndex; }

	// foreign keys
	void AddFK( const FKData_t &fkData );
	void GetFKListString( CFmtStr1024 *pstr, int nIndents );
	int GetFKCount();
	FKData_t &GetFKData( int iIndex );


	static CRecordInfo *Alloc();
#ifdef DBGFLAG_VALIDATE
	static void ValidateStatics( CValidator &validator, const char *pchName );
	void Validate( CValidator &validator, const char *pchName );
#endif //DBGFLAG_VALIDATE


	// note: destructor is private.  This is a ref-counted object, private destructor ensures callers can't accidentally delete
	// directly, or declare on stack
	virtual ~CRecordInfo() { }

private:
	virtual void DestroyThis();
	void CalculateChecksum();
	void BuildColumnNameIndex();

	char m_rgchName[k_cSQLObjectNameMax+1];
	int m_nTableID;										// Object_ID if this table in SQL Server
	CUtlVector<CColumnInfo> m_VecColumnInfo;			// Vector of columns in this record
	CMapIColumnInfo m_MapIColumnInfo;					// Map of name->column index for quick lookup by name
	EPrimaryKeyType m_nHasPrimaryKey;					// Does this table contain a column that is a primary key?
	int m_iPKIndex;										// index info m_VecIndexes of our PK index; -1 if no PK
	CUtlVector<FieldSet_t> m_VecIndexes;				// vector of all fields in all indexes
	int m_cubFixedSize;									// Sum of data sizes for all fixed size columns
	bool m_bAllColumnsAdded;							// Have all columns been added
	bool m_bPreparedForUse;								// Have we finished being initialized?
	bool m_bHaveColumnNameIndex;						// Have we created a column name index?  (Only generated if someone asks.)
	bool m_bHaveChecksum;								// Have we generated a checksum?  (Only generated if someone asks.)
	int m_nChecksum;									// checksum of this record info for quick comparisons - includes all columns
	ESchemaCatalog m_eSchemaCatalog;					// what catalog owns this object?
	CUtlVector< int > m_vecFTSFields;					// which fields have FTS indexing?
	int m_nFullTextCatalogIndex;						// index of catalog for FTS index, if we get one
	CUtlVector<FKData_t> m_VecFKData;					// vector of all FK relationships defined on this table

	CThreadMutex m_Mutex;
	static CThreadSafeClassMemoryPool<CRecordInfo>		sm_MemPoolRecordInfo;

#ifdef _DEBUG
	// validation tracking
	static CUtlRBTree<CRecordInfo *, int >	sm_mapPMemPoolRecordInfo;
	static CThreadMutex	sm_mutexMemPoolRecordInfo;
#endif
};


int __cdecl CompareColumnInfo( const CColumnInfo *pColumnInfoLeft, const CColumnInfo *pColumnInfoRight );


} // namespace GCSDK
#endif // GCRECORDINFO_H
