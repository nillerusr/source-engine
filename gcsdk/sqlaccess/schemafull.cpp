//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================


#include "stdafx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{
CSchemaFull g_SchemaFull;
CSchemaFull & GSchemaFull()
{
	return g_SchemaFull;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSchemaFull::CSchemaFull()
{
	m_pubScratchBuffer = NULL;
	m_cubScratchBuffer = 0;
	m_unCheckSum = 0;

	m_mapFTSEnabled.SetLessFunc( DefLessFunc( enum ESchemaCatalog ) );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSchemaFull::~CSchemaFull()
{
	Uninit();
}



//-----------------------------------------------------------------------------
// Purpose: Call this after you've finished setting up the SchemaFull (either
//			by loading it or by in GenerateIntrinsic).  It calculates our checksum
//			and allocates our scratch buffer.
//-----------------------------------------------------------------------------
void CSchemaFull::FinishInit()
{
	// Calculate our checksum
	m_unCheckSum = 0;

	for ( int iSchema = 0; iSchema < m_VecSchema.Count(); iSchema++ )
		m_unCheckSum += m_VecSchema[iSchema].CalcChecksum();

	// Allocate our scratch buffer
	Assert( NULL == m_pubScratchBuffer );
	// Include some slop for field IDs and sizes in a sparse record
	// 2k is way overkill but still no big deal
	m_cubScratchBuffer = k_cubRecordMax + 2048;
	m_pubScratchBuffer = ( uint8 * ) malloc( m_cubScratchBuffer );
}

//-----------------------------------------------------------------------------
// Purpose: Call this after you've finished setting up the SchemaFull (either
//			by loading it or by in GenerateIntrinsic).  It calculates our checksum
//			and allocates our scratch buffer.
//-----------------------------------------------------------------------------

void CSchemaFull::SetITable( CSchema* pSchema, int iTable )
{
	// make sure we don't have this schema anywhere already
	for ( int iSchema = 0; iSchema < m_VecSchema.Count(); iSchema++ )
	{
		if ( pSchema != &m_VecSchema[iSchema] )
			AssertFatalMsg( m_VecSchema[iSchema].GetITable() != iTable, "Duplicate iTable in schema definition.\n" );
	}

	// set the pSchema object
	pSchema->SetITable( iTable );
}



//-----------------------------------------------------------------------------
// Purpose: Uninits the schema.  Need to call this explicitly before app shutdown
//			on static instances of this object, as the CSchema objects
//			point to memory in static memory pools which may destruct
//			before static instances of this object.
//-----------------------------------------------------------------------------
void CSchemaFull::Uninit()
{
	m_VecSchema.RemoveAll();
	if ( NULL != m_pubScratchBuffer )
	{
		free( m_pubScratchBuffer );
		m_pubScratchBuffer = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get the scratch buffer. It is large enough to handle any 
//			record, sparse or otherwise
//			
//-----------------------------------------------------------------------------
uint8* CSchemaFull::GetPubScratchBuffer( )
{
	return m_pubScratchBuffer;
}


//-----------------------------------------------------------------------------
// Purpose: This is used during the generation of our intrinsic schema.  We've
//			added a new schema to ourselves, and we need to make sure that it
//			matches the corresponding C class.
// Input:	pSchema -			Schema to check
//			cField -			Number of fields the schema should contain.
//			cubRecord -			Size of a record in the schema
//-----------------------------------------------------------------------------
void CSchemaFull::CheckSchema( CSchema *pSchema, int cField, uint32 cubRecord )
{
	// We generate our structures and our schema using macros that operate on the
	// same source.  We check a couple of things to make sure that they're properly in sync.

	// This will fail if the schema's definition specifies the wrong iTable
	if ( pSchema != &m_VecSchema[pSchema->GetITable()] )
	{
		EmitError( SPEW_SQL, "Table %s has a bad iTable\n", pSchema->GetPchName() );
	}

	// This will fail if there are missing lines in the schema definition
	if ( pSchema->GetCField() != cField )
	{
		EmitError( SPEW_SQL, "Badly formed table %s (blank line in schema def?)\n", pSchema->GetPchName() );
		AssertFatal( false );
	}

	// This is unlikely to fail.  It indicates some kind of size mismatch (maybe a packing problem?)
	if ( pSchema->CubRecordFixed() != cubRecord )
	{
		// You may hit this if END_FIELDDATA_HAS_VAR_FIELDS is not used properly
		EmitError( SPEW_SQL, "Table %s has an inconsistent size (class = %d, schema = %d)\n",
			pSchema->GetPchName(), cubRecord, pSchema->CubRecordFixed() );
		AssertFatal( false );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Finds the table with a given name.
// Input:	pchName -			Name of the table to search for
// Output:	Index of the matching table ( k_iTableNil if there isn't one)
//-----------------------------------------------------------------------------
int CSchemaFull::FindITable( const char *pchName )
{
	for ( int iSchema = 0; iSchema < m_VecSchema.Count(); iSchema++ )
	{
		if ( 0 == Q_strcmp( pchName, m_VecSchema[iSchema].GetPchName() ) )
			return iSchema;
	}

	return k_iTableNil;
}


//-----------------------------------------------------------------------------
// Purpose: Finds the table with a given iTable (iSchema)
// Input:	iTable -
// Output:	NULL or a const char * to the name (for temporary use only)
//-----------------------------------------------------------------------------
const char * CSchemaFull::PchTableFromITable( int iTable )
{
	if ( iTable < 0 || iTable >= m_VecSchema.Count() )
		return NULL;
	else
		return m_VecSchema[ iTable ].GetPchName();
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSchemaFull::AddFullTextCatalog( enum ESchemaCatalog eCatalog, const char *pstrCatalogName, int nFileGroup )
{
	CFTSCatalogInfo info;
	info.m_eCatalog = eCatalog;
	info.m_nFileGroup = nFileGroup;
	info.m_pstrName = strdup(pstrCatalogName);

	m_vecFTSCatalogs.AddToTail( info );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CSchemaFull::GetFTSCatalogByName( enum ESchemaCatalog eCatalog, const char *pstrCatalogName )
{
	int nIndex = -1;
	FOR_EACH_VEC( m_vecFTSCatalogs, i )
	{
		CFTSCatalogInfo &refInfo = m_vecFTSCatalogs[ i ];
		if ( 0 == Q_stricmp( pstrCatalogName, refInfo.m_pstrName ) )
		{
			nIndex = i;
			break;
		}
	}

	return nIndex;
}


//-----------------------------------------------------------------------------
// Purpose: turn on FTS for the named schema catalog. Called by the
//			InitIntrinsic() function.
//-----------------------------------------------------------------------------
void CSchemaFull::EnableFTS( enum ESchemaCatalog eCatalog )
{
	// mark it enabled in the map
	m_mapFTSEnabled.Insert( eCatalog, true );
}


//-----------------------------------------------------------------------------
// Purpose: is FTS enabled for the supplied schema catalog?
//-----------------------------------------------------------------------------
bool CSchemaFull::GetFTSEnabled( enum ESchemaCatalog eCatalog )
{
	int iEntry = m_mapFTSEnabled.Find( eCatalog );
	if ( iEntry == m_mapFTSEnabled.InvalidIndex() )
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Adds a schema conversion instruction (for use in converting from
//			a different SchemaFull to this one).
//-----------------------------------------------------------------------------
void CSchemaFull::AddDeleteTable( const char *pchTableName )
{
	DeleteTable_t &deleteTable = m_VecDeleteTable[m_VecDeleteTable.AddToTail()];
	Q_strncpy( deleteTable.m_rgchTableName, pchTableName, sizeof( deleteTable.m_rgchTableName ) );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a schema conversion instruction (for use in converting from
//			a different SchemaFull to this one).
//-----------------------------------------------------------------------------
void CSchemaFull::AddRenameTable( const char *pchTableNameOld, const char *pchTableNameNew )
{
	RenameTable_t &renameTable = m_VecRenameTable[m_VecRenameTable.AddToTail()];
	Q_strncpy( renameTable.m_rgchTableNameOld, pchTableNameOld, sizeof( renameTable.m_rgchTableNameOld ) );
	renameTable.m_iTableDst = FindITable( pchTableNameNew );
	Assert( k_iTableNil != renameTable.m_iTableDst );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a schema conversion instruction (for use in converting from
//			a different SchemaFull to this one).
//-----------------------------------------------------------------------------
void CSchemaFull::AddDeleteField( const char *pchTableName, const char *pchFieldName )
{
	int iSchema = FindITable( pchTableName );
	AssertFatal( k_iTableNil != iSchema );

	m_VecSchema[iSchema].AddDeleteField( pchFieldName );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a schema conversion instruction (for use in converting from
//			a different SchemaFull to this one).
//-----------------------------------------------------------------------------
void CSchemaFull::AddRenameField( const char *pchTableName, const char *pchFieldNameOld, const char *pchFieldNameNew )
{
	int iSchema = FindITable( pchTableName );
	AssertFatal( k_iTableNil != iSchema );

	m_VecSchema[iSchema].AddRenameField( pchFieldNameOld, pchFieldNameNew );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a schema conversion instruction (for use in converting from
//			a different SchemaFull to this one).
//-----------------------------------------------------------------------------
void CSchemaFull::AddAlterField( const char *pchTableName, const char *pchFieldNameOld, const char *pchFieldnameNew, PfnAlterField_t pfnAlterField )
{
	int iSchema = FindITable( pchTableName );
	AssertFatal( k_iTableNil != iSchema );

	m_VecSchema[iSchema].AddAlterField( pchFieldNameOld, pchFieldnameNew, pfnAlterField );
}


//-----------------------------------------------------------------------------
// Purpose: Add a trigger to the desired schema
//-----------------------------------------------------------------------------
void CSchemaFull::AddTrigger( ESchemaCatalog eCatalog, const char *pchTableName, const char *pchTriggerName, ETriggerType eTriggerType, const char *pchTriggerText )
{
	CTriggerInfo trigger;
	trigger.m_eTriggerType = eTriggerType;
	trigger.m_eSchemaCatalog = eCatalog;
	Q_strncpy( trigger.m_szTriggerName, pchTriggerName, Q_ARRAYSIZE( trigger.m_szTriggerName ) );
	Q_strncpy( trigger.m_szTriggerTableName, pchTableName, Q_ARRAYSIZE( trigger.m_szTriggerTableName ) );
	trigger.m_strText = pchTriggerText;

	// add it to our list
	m_VecTriggers.AddToTail( trigger );
}

		
//-----------------------------------------------------------------------------
// Purpose:	Figures out how to map a table from another SchemaFull into us.
//			First we check our conversion instructions to see if any apply,
//			and then we look for a straightforward match.
// Input:	pchTableName -		Name of the table we're trying to map
//			piTableDst -		[Return] Index of the table to map it to
// Output:	true if we know what to do with this table (if false, the conversion
//			is undefined and dangerous).
//-----------------------------------------------------------------------------
bool CSchemaFull::BCanConvertTable( const char *pchTableName, int *piTableDst )
{
	// Should this table be deleted?
	for ( int iDeleteTable = 0; iDeleteTable < m_VecDeleteTable.Count(); iDeleteTable++ )
	{
		if ( 0 == Q_strcmp( pchTableName, m_VecDeleteTable[iDeleteTable].m_rgchTableName ) )
		{
			*piTableDst = k_iTableNil;
			return true;
		}
	}

	// Should this table be renamed?
	for ( int iRenameTable = 0; iRenameTable < m_VecRenameTable.Count(); iRenameTable++ )
	{
		if ( 0 == Q_strcmp( pchTableName, m_VecRenameTable[iRenameTable].m_rgchTableNameOld ) )
		{
			*piTableDst = m_VecRenameTable[iRenameTable].m_iTableDst;
			return true;
		}
	}

	// Find out which of our tables this table maps to (if it doesn't map
	// to any of them, we don't know what to do with it).
	*piTableDst = FindITable( pchTableName );
	return ( k_iTableNil != *piTableDst );
}


//-----------------------------------------------------------------------------
// Purpose:	Gets the default SQL schema name for a catalog
//-----------------------------------------------------------------------------
const char *CSchemaFull::GetDefaultSchemaNameForCatalog( ESchemaCatalog eCatalog )
{
	// For all catalogs it's actually the same
	if ( m_strDefaultSchemaName.IsEmpty() )
	{
		m_strDefaultSchemaName.Set( CFmtStr( "App%u", GGCBase()->GetAppID() ) );
	}

	return m_strDefaultSchemaName.Get();
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Run a global validation pass on all of our data structures and memory
//			allocations.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CSchemaFull::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	ValidateObj( m_VecSchema );
	for ( int iSchema = 0; iSchema < m_VecSchema.Count(); iSchema++ )
	{
		ValidateObj( m_VecSchema[iSchema] );
	}

	ValidateObj( m_VecDeleteTable );
	ValidateObj( m_VecRenameTable );

	ValidateObj( m_mapFTSEnabled );

	ValidateObj( m_vecFTSCatalogs );
	FOR_EACH_VEC( m_vecFTSCatalogs, i )
	{
		ValidateObj( m_vecFTSCatalogs[i] );
	}

	ValidateObj( m_VecTriggers );
	FOR_EACH_VEC( m_VecTriggers, i )
	{
		ValidateObj( m_VecTriggers[i] );
	}

	validator.ClaimMemory( m_pubScratchBuffer );
}
#endif // DBGFLAG_VALIDATE

} // namespace GCSDK
