//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef GCSCHEMA_H
#define GCSCHEMA_H
#ifdef _WIN32
#pragma once
#endif

namespace GCSDK
{
const int k_nColFlagIndexed =	 0x0001;		// this column is indexed.  
const int k_nColFlagUnique =	 0x0002;		// this column has a uniqueness constraint - creates implicit index
const int k_nColFlagPrimaryKey = 0x0004;		// this column has a primary key constraint - creates implicit uniqueness constraint & implicit index
const int k_nColFlagAutoIncrement = 0x0008;		// this column can have it's values created implicitly by the sql counter
const int k_nColFlagClustered = 0x0010;			// this column is clustered

const int k_nColFlagAllConstraints = k_nColFlagUnique | k_nColFlagPrimaryKey;

class CRecordInfo;
struct VarFieldBlockInfo_t;
struct VarFieldBlockInfo_t;
struct Field_t;
struct VarField_t;

// Function type for altering field types when converting schemas
typedef void (* PfnAlterField_t )( void *pvDest, const void *pvSrc );

// EPrimaryKeyType
// This shows if a table has a primary key and, if so, if it has multiple columns
// or not.
enum EPrimaryKeyType
{
	k_EPrimaryKeyTypeNone = 0,	// none at all
	k_EPrimaryKeyTypeSingle,	// single-column primary key
	k_EPrimaryKeyTypeMulti,		// multi-column primary key
};


// EWipePolicy
// This tells us if a table is supposed to be wiped before all tests,
// before all tests except stress tests, or not to be wiped before tests.
enum EWipePolicy
{
	k_EWipePolicyPreserveAlways = 0,		// don't wipe table
	k_EWipePolicyPreserveForStress = 1,		// preserve for stress tests, wipe before regular tests
	k_EWipePolicyWipeForAllTests = 2,		// wipe table prior to all tests
};

//-----------------------------------------------------------------------------
// Field
// This defines the schema of a single field in one of our tables.
//-----------------------------------------------------------------------------
struct Field_t
{
	bool BGetIntData( uint8 *pubRecord, uint64 *pulRet ) const;
	bool BGetFloatData( uint8 *pubRecord, float *fRet ) const;
	bool SetIntData( uint8 *pubRecord, uint64 ulValue ) const;
	// Not all fields are updated wholly at a time
	int CubFieldUpdateSize() const;
	// Handy helpers - complex fields like "rolling unique" are
	// still binary
	bool BIsStringType() const;

	bool BIsVariableLength() const;

	// Members that get serialized
	EGCSQLType m_EType;						// Field type
	uint32 m_cubLength;					// Length of the field in bytes
	uint32 m_cchMaxLength;					// maximum length of the field in characters
	char m_rgchName[k_cSQLObjectNameMax];	// Human-readable name of this field
	char m_rgchSQLName[k_cSQLObjectNameMax];	// SQL name of this field

	// Members that don't get serialized
	uint32 m_nColFlags;					// k_nColFlag* bits for this field
	uint32 m_dubOffset;					// Offset of this field from beginning of record
};


// --------------------------------------------------------------------------
// Information about a foreign key relationship defined on a table
struct FKColumnRelation_t
{
	char m_rgchCol[k_cSQLObjectNameMax+1];
	char m_rgchParentCol[k_cSQLObjectNameMax+1];

	FKColumnRelation_t()
	{
		Q_memset( m_rgchCol, 0, Q_ARRAYSIZE( m_rgchCol ) );
		Q_memset( m_rgchParentCol, 0, Q_ARRAYSIZE( m_rgchParentCol ) );
	}

	bool operator==( const FKColumnRelation_t &other ) const
	{
		if ( Q_stricmp( m_rgchCol, other.m_rgchCol ) )
			return false;
		if ( Q_stricmp( m_rgchParentCol, other.m_rgchParentCol ) )
			return false;
		return true;
	}
};

struct FKData_t
{
	char m_rgchName[k_cSQLObjectNameMax+1];
	char m_rgchParentTableName[k_cSQLObjectNameMax+1]; 
	CCopyableUtlVector<FKColumnRelation_t> m_VecColumnRelations;
	EForeignKeyAction m_eOnDeleteAction;
	EForeignKeyAction m_eOnUpdateAction;

	FKData_t()
	{
		Q_memset( m_rgchName, 0, Q_ARRAYSIZE( m_rgchName ) );
		Q_memset( m_rgchParentTableName, 0, Q_ARRAYSIZE( m_rgchParentTableName ) );
		m_eOnDeleteAction = k_EForeignKeyActionNoAction;
		m_eOnUpdateAction = k_EForeignKeyActionNoAction;
	}

	bool operator==( const FKData_t &other ) const
	{
		if ( Q_stricmp( m_rgchName, other.m_rgchName ) )
			return false;

		if ( Q_stricmp( m_rgchParentTableName, other.m_rgchParentTableName ) )
			return false;

		if ( m_eOnDeleteAction != other.m_eOnDeleteAction || m_eOnUpdateAction != other.m_eOnUpdateAction )
			return false;

		FOR_EACH_VEC( m_VecColumnRelations, i )
		{
			bool bFoundInOther = false;
			const FKColumnRelation_t &cols = m_VecColumnRelations[i];

			FOR_EACH_VEC( other.m_VecColumnRelations, j )
			{
				const FKColumnRelation_t &colsOther = other.m_VecColumnRelations[j];
				if ( cols == colsOther )
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

#ifdef DBGFLAG_VALIDATE
	// Validate our internal structures
	void Validate( CValidator &validator, const char *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_VecColumnRelations );
	}
#endif

};


#pragma pack( push, 1 )


//-----------------------------------------------------------------------------
// SerSchema
// Defines the binary serialization format for a schema.
//-----------------------------------------------------------------------------
struct SerSchema_t
{
	uint32 m_cub;						// Size of the whole schema (including header and fields)
	int32 m_iTable;						// Our table's iTable
	char m_rgchName[k_cSQLObjectNameMax];	// Human-readable name of this table
	int16 m_cField;						// # of fields in the schema (int16 for backward-compatibility reasons)
	int16 m_ETableGroup;				// Our table's TableGroup (int16 for backward-compatibility reasons) - OBSOLETE
};


//-----------------------------------------------------------------------------
// SerField
// Defines the binary serialization format for a field in a schema
// Note that certain fields are missing from this because we only use serialized
// schemas for schema mapping.  Fields that only affect runtime behavior (like
// indexing) are always defined by the intrinsic schema.
//-----------------------------------------------------------------------------
struct SerField_t 
{
	int32 m_EType;					// Field type
	uint32 m_cubLength;				// Length of field data in bytes
									// For rolling fields, high 16 bits are the
									// size of each element

	char m_rgchName[k_cSQLObjectNameMax];// Human-readable name of this field
	char m_rgchSQLName[k_cSQLObjectNameMax]; // SQL name of this field
};

#pragma pack( pop )


//-----------------------------------------------------------------------------
// Schema conversion instructions
// These specify various operations that can be performed when converting
// from one Schema to another.
//-----------------------------------------------------------------------------
struct DeleteField_t
{
	char m_rgchFieldName[k_cSQLObjectNameMax];		// Name of the field to delete
};

struct RenameField_t
{
	char m_rgchFieldNameOld[k_cSQLObjectNameMax];	// Rename a field with this name
	int m_iFieldDst;							// to this field
};

struct AlterField_t
{
	char m_rgchFieldNameOld[k_cSQLObjectNameMax];	// Name of field in the old schema
	int m_iFieldDst;							// iField of it in the new
	PfnAlterField_t m_pfnAlterFunc;				// Function to translate the data
};

//-----------------------------------------------------------------------------
// FieldSet_t describes a collection of fields in an index, as well as
// attributes of the index itself
//-----------------------------------------------------------------------------

class FieldSet_t
{
public:
	FieldSet_t( bool bUnique, bool bClustered, const CUtlVector<int>& vecFields, const char* pstrIndexName )
		: m_bClustered( bClustered ), m_bUnique( bUnique )
	{
		m_VecFields = vecFields;

		// zero means to use the server default
		m_nFillFactor = 0;

		// null name is allowed for primary keys
		if ( pstrIndexName != NULL )
			Q_strncpy( m_szIndexName, pstrIndexName, Q_ARRAYSIZE( m_szIndexName ) );
		else
			m_szIndexName[0] = 0;
	}

	FieldSet_t( )
	{
	}

	~FieldSet_t( )
	{
	}

	FieldSet_t( const FieldSet_t &refOther )
	{
		DoAssignment( refOther );
	}

	FieldSet_t& operator=( const FieldSet_t &refOther )
	{
		DoAssignment( refOther );
		return *this;
	}

	void DoAssignment( const FieldSet_t &refOther )
	{
		m_VecFields = refOther.m_VecFields;
		m_VecIncluded = refOther.m_VecIncluded;
		m_bClustered = refOther.m_bClustered;
		m_bUnique = refOther.m_bUnique;
		m_nFillFactor = refOther.m_nFillFactor;
		Q_strncpy( m_szIndexName, refOther.m_szIndexName, Q_ARRAYSIZE( m_szIndexName ) );

	}

	// get count of fields in this index
	int GetCount() const
	{
		return m_VecFields.Count();
	}

	// get count of included fields in this index
	int GetIncludedCount() const
	{
		return m_VecIncluded.Count();
	}

	void AddIncludedColumn( int nIndex )
	{
		m_VecIncluded.AddToTail( nIndex );
	}

	void AddIncludedColumns( const CUtlVector<int> &refVec )
	{
		m_VecIncluded.AddVectorToTail( refVec );
	}

	// get a particular field ID
	// the returned index is into the VecFields of the associated schema
	int GetField( int nIndex ) const
	{
		return m_VecFields[ nIndex ];
	}

	int GetIncludedField( int nIndex ) const
	{
		return m_VecIncluded[ nIndex ];
	}

	// is this index clustered?
	bool IsClustered() const
	{
		return m_bClustered;
	}

	// is this index unique?
	bool IsUnique() const
	{
		return m_bUnique;
	}

	void SetClustered( bool bIsClustered )
	{
		m_bClustered = bIsClustered;
	}

	void SetFillFactor( int nFactor )
	{
		Assert( nFactor >= 0 && nFactor <= 100 );
		m_nFillFactor = nFactor;
	}

	int GetFillFactor( ) const
	{
		return m_nFillFactor; 
	}

	const char* GetIndexName() const
	{
		return m_szIndexName;
	}

	// determine if this fieldset is equal to the other one
	static bool CompareFieldSets( const FieldSet_t& refThis, CRecordInfo* pRecordInfoThis,
		const FieldSet_t& refOther, CRecordInfo* pRecordInfoOther );

#ifdef DBGFLAG_VALIDATE
	// Validate our internal structures
	void Validate( CValidator &validator, const char *pchName )
	{
		VALIDATE_SCOPE();
		m_VecFields.Validate( validator, "m_VecFields" );
		m_VecIncluded.Validate( validator, "m_VecIncluded" );
	}
#endif

private:
	CUtlVector<int> m_VecFields;	// ids of fields; indexes into m_VecFields of CSchema for a table
	CUtlVector<int> m_VecIncluded;	// ids of included fields
	int m_nFillFactor;				// fill factor for the index; zero means to use the server's default
	char m_szIndexName[k_cSQLObjectNameMax]; // name of this index
	bool m_bClustered:1;			// is this index clustered?
	bool m_bUnique:1;				// is this index unique?
};

//-----------------------------------------------------------------------------

enum ESchemaCatalog
{
	k_ESchemaCatalogInvalid = -1,
	k_ESchemaCatalogMain = 0,		// main GC catalog
	k_ESchemaCatalogOGS = 4,		// operational game stats
};

extern const char* PchNameFromESchemaCatalog( ESchemaCatalog e );


//-----------------------------------------------------------------------------
// CSchema
// This defines the schema for a single table.  The schema essentially defines
// what's in the table (ie, field 0 is a 32 char string called "Name", etc.)
// The schema is in charge of manipulating individual records within the table.
//-----------------------------------------------------------------------------

class CSchemaFull;

class CSchema
{
public:

	// Constructors & destructors
	CSchema();
	~CSchema();

	// Recalculates field offsets and maximum record size.
	// Must be called after changing schema.
	void CalcOffsets();

	// called to make final calculations when all fields/indexes/etc have been added and the schema is ready to be used
	void PrepareForUse();

	// Sizing information
	uint32 CubSerialSchema() const { return( sizeof( SerSchema_t ) + m_VecField.Count() * sizeof( SerField_t ) ); }

	// Size of total fixed-length portion of record
	uint32 CubRecordFixed() const { return( m_cubRecord ); }

	// Size of the total variable-length portion of record (zero if no var-length fields)
	uint32 CubRecordVariable( const void *pvRecord ) const;

	// Does this record have variable-length fields?
	bool BHasVariableFields() const { return m_bHasVarFields; }

	VarFieldBlockInfo_t* PVarFieldBlockInfoFromRecord( const void *pvRecord ) const;

	// Access field data - fixed or variable (may return NULL/0 for empty var field)
	bool BGetFieldData( const void *pvRecord, int iField, uint8 **ppubField, uint32 *pcubField ) const;
	bool BSetFieldData( void *pvRecord, int iField, uint8 *pubField, uint32 cubField, bool *pbVarBlockRealloced );

	bool BGetVarField( const void *pvRecord, const VarField_t *pVarField, uint8 **ppubField, uint32 *pcubField ) const;
	bool BSetVarField( void *pvRecord, VarField_t *pVarField, const void *pvData, uint32 cubData, bool *pbRealloced, bool bFreeOnRealloc );

	// Adjust var-block pointer, if present, to point just after the fixed part of the record
	void FixupDeserializedRecord( void *pvRecord );

	// Render a record in text format
	void RenderRecord( uint8 *pubRecord );
	void RenderField( uint8 *pubRecord, int iField, int cchBuffer, char *pchBuffer );

	// Accessors
	void SetITable( int iTable ) { m_iTable = iTable; }
	int GetITable() const { return m_iTable; }
	int GetCField() const { return m_VecField.Count(); }
	void SetReportingInterval( int nInterval ) { m_nReportingInterval = nInterval; }
	int GetReportingInterval( ) const { return m_nReportingInterval; }
	Field_t &GetField( int iField ) { return m_VecField[iField]; }
	const Field_t &GetField( int iField ) const { return m_VecField[iField]; }
	VarField_t *GetPVarField( void *pvRecord, int iField ) { return ( VarField_t * )( ( uint8 * ) pvRecord + m_VecField[iField].m_dubOffset ); }
	void SetName( const char *pchName ) { Q_strncpy( m_rgchName, pchName, sizeof( m_rgchName ) ); }
	const char *GetPchName() const { return m_rgchName; }
	const FieldSet_t& GetPKFields() { Assert( m_iPKIndex != -1 ); return GetIndexes()[m_iPKIndex]; }
	int GetPKIndex() const { return m_iPKIndex; }
	const CUtlVector<FieldSet_t>& GetIndexes() { return m_VecIndexes; }
	const CUtlVector<int>& GetFTSColumns() { return m_VecFullTextIndexes; }
	int GetFTSIndexCatalog() const { return m_nFullTextIndexCatalog; }

	ESchemaCatalog GetESchemaCatalog() const { return m_eSchemaCatalog; }
	void SetESchemaCatalog( ESchemaCatalog eSchemaCatalog ) { m_eSchemaCatalog = eSchemaCatalog; }

	// If cRecordMax is non-zero, this is a rolling table that only
	// holds on to cRecordMax records at most.
	void SetCRecordMax( int cRecordMax ) { m_cRecordMax = cRecordMax; }
	int GetCRecordMax() const { return m_cRecordMax; }

	// Is this table for TESTs only?
	void SetBTestTable( bool bTestTable ) { m_bTestTable = bTestTable; }
	bool GetBTestTable() const { return m_bTestTable; }

	// Randomly init a record or field to random values
	void InitRecordRandom( uint8 *pubRecord, uint32 unPrimaryIndex, bool *pbVarBlockRealloced, bool bFreeVarBlockOnRealloc );
	void SetFieldRandom( uint8 *pubRecord, int iField, bool *pbVarBlockRealloced, bool bFreeVarBlockOnRealloc );

	// Checksum the schema
	uint32 CalcChecksum();

	// pre-allocate space in the field array
	void EnsureFieldCount( int cFields )
	{
		m_VecField.EnsureCapacity( cFields );
	}
	
	// This adds a field from our intrinsic schema to us
	void AddField( char *pchName, char *pchSQLName, EGCSQLType eType, uint32 cubSize, int cchMaxLength );
	void AddIntField( char *pchName, char *pchSQLName, EGCSQLType eType, int cubSize );

	// We want to make a particular field the primary key
	int PrimaryKey( bool bClustered, int nFillFactor, const char *pchName );

	// we want to make a particular list of fields the primary key
	int PrimaryKeys( bool bClustered, int nFillFactor, const char *pchNames );

	// We want to index a particular field by name
	int IndexField( const char *pchName, const char *pchIndexName );

	// We want to index a particular list of fields in a group
	int IndexFields( const char *pchIndexName, const char *pchNames );

	// We want a certain index to additionally include a list of fields
	void AddIncludedFields( const char *pchIndexName, const char *pchNames );

	// We want to unique index a particular list of fields in a group
	int UniqueFields( const char *pchIndexName, const char *pchNames );

	// add a full-text index to the given column
	void AddFullTextIndex( CSchemaFull *pSchemaFull, const char *pchCatalogName, const char *pchColumnName );

	// We want to index a particular field by field number
	// (field number is an index into the m_VecField array)
	int AddIndexToFieldNumber( int iField, const char *pchIndexName, bool bClustered );

	// We want to index a particular set of fields
	//	pchNames includes the names, separated by commas, of each field
	int AddIndexToFieldList( const char *pchNames, const char *pchIndexName, int nFlags, int nFillFactor );

	// We want a unique index on a particular field
	int UniqueField( const char *pchName, const char *pchIndexName );

	// We want to have a clustered index on a particular field by name
	int ClusteredIndexField( int nFillFactor, const char *pchName, const char *pchIndexName );

	// We want to index a particular list of fields in a group
	int ClusteredIndexFields( int nFillFactor, const char *pchIndexName, const char *pchNames );

	// We want an autoinc on a particular field
	void AutoIncrementField( char *pchName );
	
	// catalog on which we'll enable FTS
	void EnableFTS( ESchemaCatalog eCatalog );

	// adds a full text catalog with the given name on the identified fileset
	void AddFullTextCatalog( ESchemaCatalog eCatalog, const char *pstrCatalogName, const char *pstrFileGroupName );

	// Adds a FK on the table
	void AddFK( const char* pchName, const char* pchColumn, const char* pchParentTable, const char* pchParentColumn, EForeignKeyAction eOnDeleteAction, EForeignKeyAction eOnUpdateAction );

	// Access FK data
	int GetFKCount();
	FKData_t &GetFKData( int iIndex );

	void SetTestWipePolicy( EWipePolicy policy ) { m_wipePolicy = policy; }
	EWipePolicy GetTestWipePolicy() const { return m_wipePolicy; }

	void SetBAllowWipeTableInProd( bool bVal ) { m_bAllowWipeInProd = bVal; }
	bool BAllowWipeTableInProd() const { return m_bAllowWipeInProd; }

	void SetPrepopulatedTable( ) { m_bPrepopulatedTable = true; }
	bool BPrepopulatedTable( ) const { return m_bPrepopulatedTable; }

	// Find the field with a given name (returns k_iFieldNil if not found)
	int FindIField( const char *pchName );
	int FindIFieldSQL( const char *pchName );

	// Helper functions for recording schema conversion operations
	void AddDeleteField( const char *pchFieldName );
	void AddRenameField( const char *pchFieldNameOld, const char *pchFieldNameNew );
	void AddAlterField( const char *pchFieldNameOld, const char *pchFieldNameNew, PfnAlterField_t pfnAlterField );

	// Schema conversion helper: figure out what field to map a field from a different schema to
	bool BCanConvertField( const char *pchFieldSrc, int *piFieldDst, PfnAlterField_t *ppfnAlterField );

	CRecordInfo *GetRecordInfo() { return m_pRecordInfo; }
	const CRecordInfo *GetRecordInfo() const { return m_pRecordInfo; }

	void Validate( CValidator &validator, const char *pchName );		// Validate our internal structures
	void ValidateRecord( uint8 *pubRecord );					// Validate a record that uses our schema

	// cached queries
	const char *GetInsertStatementText() const;
	const char *GetMergeStatementTextOnPKWhenMatchedUpdateWhenNotMatchedInsert();
	const char *GetMergeStatementTextOnPKWhenNotMatchedInsert();

private:
	int m_iTable;									// The index of our table
	int m_iPKIndex;									// index into of m_VecIndexes of our PK index; k_iFieldNil if no PK
	char m_rgchName[k_cSQLObjectNameMax];			// Name of this table
	CUtlVector<Field_t> m_VecField;					// All the fields that make up the schema
	CUtlVector<FieldSet_t> m_VecIndexes;			// vector of all fields in all indexes
	int m_cRecordMax;								// Max # records in the table (for rolling tables)
	bool m_bTestTable;								// Table exists only for tests
	bool m_bAllowWipeInProd;						// should we allow WipeTable operations on this table in the beta/public universe?
	EWipePolicy m_wipePolicy;						// should this table be wiped between all tests, no tests, or non-stress tests?
	bool m_bHasVarFields;							// True if this table has variable-length fields
	bool m_bPrepopulatedTable;						// true if this table is pre-populated
	EPrimaryKeyType m_nHasPrimaryKey;				// what kind of PK do we have, if any?
	CRecordInfo *m_pRecordInfo;		// The record description corresponding to this schema.  (Similar info, record description is new form, have both for a while during DS->SQL switch)
	CUtlVector<int> m_VecFullTextIndexes;			// vector of indexes into m_VecField of fields covered by this table's full-text index.
	int m_nFullTextIndexCatalog;					// index of catalog to use for creating full-text indexes
	CUtlVector<FKData_t> m_VecFKData;				// data on foreign keys for this schema object

	uint32 m_cubRecord;								// Binary record length
	int m_nReportingInterval;						// reporting interval of this table if stats; 0 if not stats
	ESchemaCatalog m_eSchemaCatalog;				// what catalog does this table live in?

	// Schema conversion instructions
	CUtlVector<DeleteField_t> m_VecDeleteField;
	CUtlVector<RenameField_t> m_VecRenameField;
	CUtlVector<AlterField_t> m_VecAlterField;

	// Cached queries
	mutable CUtlString m_sInsertStatementText;				// Cached insert statement for the table
	CUtlString m_sMergeStatementTextOnPKWhenMatchedUpdateWhenNotMatchedInsert;	// Cached insert or update via MERGE statement for the table
	CUtlString m_sMergeStatementTextOnPKWhenNotMatchedInsert;	// Cached insert via MERGE statement for the table
};

} // namespace GCSDK
#endif // GCSCHEMA_H
