//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef GCSCHEMAFULL_H
#define GCSCHEMAFULL_H
#ifdef _WIN32
#pragma once
#endif


namespace GCSDK
{


//-----------------------------------------------------------------------------
// SerSchemaFull
// This defines the binary serialization format for a CSchemaFull
//-----------------------------------------------------------------------------
struct SerSchemaFull_t
{
	enum EVersion
	{
		k_ECurrentVersion = 1,
	};

	int32 m_nVersion;		// version of serialization format 
	int32 m_cSchema;		// # of schema we contain
};


//-----------------------------------------------------------------------------
// CFTSCatalogInfo
// information about a full text search catalog object in our schema
//-----------------------------------------------------------------------------
class CFTSCatalogInfo 
{
public:
	enum ESchemaCatalog m_eCatalog;
	const char *m_pstrName;
	int m_nFileGroup;

	CFTSCatalogInfo()
		: m_pstrName( NULL ),
		m_eCatalog( k_ESchemaCatalogInvalid )
	{
	}

	~CFTSCatalogInfo()
	{
		free( (void*) m_pstrName);
	}

	CFTSCatalogInfo( const CFTSCatalogInfo &refOther )
	{
		m_eCatalog = refOther.m_eCatalog;
		m_nFileGroup = refOther.m_nFileGroup;
		if ( refOther.m_pstrName != NULL )
			m_pstrName = strdup( refOther.m_pstrName );
		else
			m_pstrName = NULL;
	}

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName )		// Validate our internal structures
	{
		validator.ClaimMemory( (void *) m_pstrName );
	}
#endif
};


//-----------------------------------------------------------------------------
// SchemaFull conversion instructions
// These specify various operations that can be performed when converting
// from one SchemaFull to another.
//-----------------------------------------------------------------------------
struct DeleteTable_t
{
	char m_rgchTableName[k_cSQLObjectNameMax];		// Name of the table to delete
};

struct RenameTable_t
{
	char m_rgchTableNameOld[k_cSQLObjectNameMax];	// Rename a table with this name
	int m_iTableDst;							// to this table
};


enum ETriggerType
{
	k_ETriggerType_Invalid,
	k_ETriggerType_After_Insert,
	k_ETriggerType_InsteadOf_Insert,
	k_ETriggerType_After_Delete,
	k_ETriggerType_InsteadOf_Delete,
	k_ETriggerType_After_Update,
	k_ETriggerType_InsteadOf_Update,
};

class CTriggerInfo
{
public:
	CTriggerInfo()
		: m_eTriggerType( k_ETriggerType_Invalid ),
		m_bMatched( false )
	{
	}

	// are these equal for identity?
	bool operator==( const CTriggerInfo& refOther ) const
	{
		if ( 0 != Q_stricmp( m_szTriggerTableName, refOther.m_szTriggerTableName ) )
			return false;
		if ( 0 != Q_stricmp( m_szTriggerName, refOther.m_szTriggerName ) )
			return false;

		// they're equal!
		return true;
	}

	// if the identity is the same, this will tell if text or type differs
	bool IsDifferent( const CTriggerInfo& refOther ) const
	{
		if ( m_eTriggerType != refOther.m_eTriggerType )
			return false;
		if ( m_strText != refOther.m_strText )
			return false;

		// they're equal!
		return true;
	}

	const char* GetTriggerTypeString() const
	{
		const char *pstrSQL = "~~ unknown trigger type syntax error ~~";

		switch ( m_eTriggerType )
		{
		case k_ETriggerType_After_Insert:
			pstrSQL = "AFTER INSERT";
			break;
		case k_ETriggerType_InsteadOf_Insert:
			pstrSQL = "INSTEAD OF INSERT";
			break;
		case k_ETriggerType_After_Delete:
			pstrSQL = "AFTER DELETE";
			break;
		case k_ETriggerType_InsteadOf_Delete:
			pstrSQL = "INSTEAD OF DELETE";
			break;
		case k_ETriggerType_After_Update:
			pstrSQL = "AFTER UPDATE";
			break;
		case k_ETriggerType_InsteadOf_Update:
			pstrSQL = "INSTEAD OF UPDATE";
			break;

		default:
		case k_ETriggerType_Invalid:
			/* initialize is fine, thanks */
			break;
		}

		return pstrSQL;
	}

	bool m_bMatched;								// got matched during schema convert
	ETriggerType m_eTriggerType;					// what kinda trigger is this?
	ESchemaCatalog m_eSchemaCatalog;				// catalog where this trigger lives
	char m_szTriggerName[k_cSQLObjectNameMax];		// name of the trigger object
	char m_szTriggerTableName[k_cSQLObjectNameMax];	// name of the table hosting this trigger
	CUtlString m_strText;							// text of the trigger

	// Validate our internal structures
#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName )
	{
		m_strText.Validate( validator, pchName );
	}
#endif
};


//-----------------------------------------------------------------------------
// CSchemaFull
// This defines the schema for the entire data store.  It's essentially just
// a collection of CSchema, which define the schema for individual tables.
//-----------------------------------------------------------------------------
class CSchemaFull
{
public:
	// Constructors & destructors
	CSchemaFull();
	~CSchemaFull();

	void Uninit();

	// add a new schema and return its pointer.
	CSchema *AddNewSchema( int iTable, ESchemaCatalog eCatalog, const char *pstrName )
	{
		CSchema &refNewSchema = m_VecSchema[m_VecSchema.AddToTail()];
		refNewSchema.SetName( pstrName );
		refNewSchema.SetESchemaCatalog( eCatalog );
		SetITable( &refNewSchema, iTable );
		return &refNewSchema;
	}


	// Accessors
	int GetCSchema() const { return m_VecSchema.Count(); }
	CSchema &GetSchema( int iSchema ) { return m_VecSchema[iSchema]; }
	uint32 GetCheckSum() const { return m_unCheckSum; }
	const char *GetDefaultSchemaNameForCatalog( ESchemaCatalog eCatalog );

	uint8 *GetPubScratchBuffer( );
	uint32 GetCubScratchBuffer() const { return m_cubScratchBuffer; }

	// Makes sure that a generated intrinsic schema is consistent
	void CheckSchema( CSchema *pSchema, int cField, uint32 cubRecord );

	// Find the table with a given name (returns -1 if not found)
	int FindITable( const char *pchName );
	const char *PchTableFromITable( int iTable );

	// Helper functions for recording schema conversion operations
	void AddDeleteTable( const char *pchTableName );
	void AddRenameTable( const char *pchTableNameOld, const char *pchTableNameNew );
	void AddDeleteField( const char *pchTableName, const char *pchFieldName );
	void AddRenameField( const char *pchTableName, const char *pchFieldNameOld, const char *pchFieldNameNew );
	void AddAlterField( const char *pchTableName, const char *pchFieldNameOld, const char *pchFieldNameNew, PfnAlterField_t pfnAlterField );

	// declare that a trigger is on a table
	void AddTrigger( ESchemaCatalog eCatalog, const char *pchTableName, const char *pchTriggerName, ETriggerType eTriggerType, const char *pchTriggerText );

	// Schema conversion helper: figure out what table to map a table from a different schema to
	bool BCanConvertTable( const char *pchTableSrc, int *piTableDst );

	// full text catalogs
	void AddFullTextCatalog( enum ESchemaCatalog eCatalog, const char *pstrCatalogName, int nFileGroup );
	int GetFTSCatalogByName( enum ESchemaCatalog eCatalog, const char *pstrCatalogName );
	void EnableFTS( enum ESchemaCatalog eCatalog );
	int GetCFTSCatalogs() const { return m_vecFTSCatalogs.Count(); }
	const CFTSCatalogInfo &  GetFTSCatalogInfo( int nIndex ) const { return m_vecFTSCatalogs[nIndex]; }

	const CUtlVector< CTriggerInfo> & GetTriggerInfos( ) const { return m_VecTriggers; }

	// is the given schema catalog FTS enabled?
	bool GetFTSEnabled( enum ESchemaCatalog eCatalog );

	void Validate( CValidator &validator, const char *pchName );		// Validate our internal structures

	// sets tableID on CSchema, checking that it is not a duplicate
	void SetITable( CSchema* pSchema, int iTable );
	void FinishInit();						// Recalculates some internal fields
private:
	CUtlConstString m_strDefaultSchemaName;

	CUtlVector< CSchema > m_VecSchema;		// Schema for tables in all catalogs
	CUtlVector< CTriggerInfo > m_VecTriggers;		// list of triggers in all catalogs

	// which schema catalogs have FTS enabled?
	CUtlMap< ESchemaCatalog, bool > m_mapFTSEnabled;

	// list of catalogs; each is marked with the schema where it lives.
	CUtlVector< CFTSCatalogInfo > m_vecFTSCatalogs;

	uint32 m_unCheckSum;					// A simple checksum of our contents

	// SchemaFull conversion instructions
	CUtlVector<DeleteTable_t> m_VecDeleteTable;
	CUtlVector<RenameTable_t> m_VecRenameTable;

	uint8 *m_pubScratchBuffer;				// Big enough to hold any record or sparse record in this schemafull
	uint32 m_cubScratchBuffer;					// Size of the scratch buffer
};

extern CSchemaFull & GSchemaFull();

} // namespace GCSDK
#endif // GCSCHEMAFULL_H
