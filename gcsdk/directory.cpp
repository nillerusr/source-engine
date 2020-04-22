//====== Copyright ©, Valve Corporation, All rights reserved. =================
//
// Purpose: Defines a directory for all the GC processes for an app
//
//=============================================================================


#include "stdafx.h"
#include "gcsdk/directory.h"


namespace GCSDK
{

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGCDirTypeInstance::CGCDirTypeInstance( const char* pszTypeName, uint32 nType, uint32 nInstance, const KeyValues *pKVConfig, const CGCDirProcess* pProcess )
	: m_nType( nType )
	, m_sTypeName( pszTypeName )
	, m_nInstance( nInstance )
	, m_pProcess( pProcess )
{
	m_pConfig = pKVConfig->MakeCopy();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGCDirTypeInstance::~CGCDirTypeInstance()
{
	m_pConfig->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the type for this GC
//-----------------------------------------------------------------------------
uint32 CGCDirTypeInstance::GetType() const
{
	return m_nType;
}

//-----------------------------------------------------------------------------
const char* CGCDirTypeInstance::GetTypeName() const
{
	return m_sTypeName;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the instance index for GCs of this type
//-----------------------------------------------------------------------------
uint32 CGCDirTypeInstance::GetInstance() const
{
	return m_nInstance;
}


//-----------------------------------------------------------------------------
// Purpose: Gets any additional configuration data for this GC
//-----------------------------------------------------------------------------
KeyValues * CGCDirTypeInstance::GetConfig() const
{
	return m_pConfig;
}

//-----------------------------------------------------------------------------
// Purpose: Gets the box that this GC was associated with
//-----------------------------------------------------------------------------
const CGCDirProcess* CGCDirTypeInstance::GetProcess( ) const
{
	return m_pProcess;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGCDirProcess::CGCDirProcess( uint32 nGCDirIndex, const char *pchName, const char* pchProcessType, const KeyValues *pKVConfig ) :
	m_iDirGC( nGCDirIndex ),
	m_sName( pchName ),
	m_sType( pchProcessType ),
	m_nTypeMask( 0 )
{
	m_pConfig = pKVConfig->MakeCopy();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGCDirProcess::~CGCDirProcess()
{
	// Don't need to delete our pointers. They were allocated by the directory
	// and will be cleaned up by the directory.
}

//-----------------------------------------------------------------------------
// Purpose: Gets any additional configuration data for this GC
//-----------------------------------------------------------------------------
KeyValues * CGCDirProcess::GetConfig() const
{
	return m_pConfig;
}

//-----------------------------------------------------------------------------
// Purpose: Gets the name for this box
//-----------------------------------------------------------------------------
const char *CGCDirProcess::GetName() const
{
	return m_sName.Get();
}

const char *CGCDirProcess::GetProcessType() const
{
	return m_sType.Get();
}

//-----------------------------------------------------------------------------
// Purpose: Gets the number of GCs assigned to this box
//-----------------------------------------------------------------------------
uint32 CGCDirProcess::GetTypeInstanceCount() const
{
	return m_vecGCs.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Gets the specified GC definition
//-----------------------------------------------------------------------------
const CGCDirTypeInstance *CGCDirProcess::GetTypeInstance( uint32 nGCIndex ) const
{
	bool bValidIndex = m_vecGCs.IsValidIndex( nGCIndex );
	Assert( bValidIndex );
	if ( !bValidIndex )
		return NULL;

	return m_vecGCs[ nGCIndex ];
}


//-----------------------------------------------------------------------------
// Purpose: Adds a GC to this box
//-----------------------------------------------------------------------------
void CGCDirProcess::AddGC( CGCDirTypeInstance *pGC )
{
	if ( !pGC )
	{
		Assert( false );
		return;
	}

	//set this within our type mask
	Assert( pGC->GetType() < 64 );
	m_nTypeMask |= ( uint64 )1 << pGC->GetType();

	m_vecGCs.AddToTail( pGC );
	if( !m_vecUniqueTypeList.HasElement( pGC->GetType() ) )
	{
		m_vecUniqueTypeList.Insert( pGC->GetType() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
uint32 CGCDirProcess::GetGCDirIndex() const
{
	return m_iDirGC;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CGCDirProcess::HasInstanceOfType( uint32 nType ) const
{
	return ( m_nTypeMask & ( ( uint64 )1 << nType ) ) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const CUtlSortVector< uint32 >& CGCDirProcess::GetUniqueTypeList() const
{
	return m_vecUniqueTypeList;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDirectory::CDirectory()
	:	m_bInitialized( false )
	,	m_mapGCsByType( DefLessFunc( int32 ) )
	,	m_mapRegisteredGCTypes( DefLessFunc( int32 ) )
{
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDirectory::~CDirectory()
{
	m_vecProcesses.PurgeAndDeleteElements();
	m_vecTypeInstances.PurgeAndDeleteElements();
}


//-----------------------------------------------------------------------------
// Purpose: Initializes the directory
//-----------------------------------------------------------------------------
bool CDirectory::BInit( KeyValues *pKVDirectory )
{
	Assert( !m_bInitialized );
	if ( m_bInitialized )
		return false;

	if ( NULL == pKVDirectory )
	{
		AssertMsg( false, "Null KV passed to CDirectory::BInit()" );
		return false;
	}

	CUtlSymbolTable processNamesTable( 0, 16, true );
	FOR_EACH_TRUE_SUBKEY( pKVDirectory, pkvProcess )
	{
		if( 0 != Q_stricmp( pkvProcess->GetName( ), "process" ) )
			continue;

		//get the name of this process and ensure that it is unique
		const char *pchProcessName = pkvProcess->GetString( "name" );
		if( !pchProcessName || !pchProcessName[ 0 ] )
		{
			AssertMsg( false, "Process defined in the config with no name" );
			return false;
		}
		if( processNamesTable.Find( pchProcessName ).IsValid() )
		{
			AssertMsg( false, "Duplicate box \"%s\" encountered while parsing the config", pchProcessName );
			return false;
		}
		processNamesTable.AddString( pchProcessName );

		//get the type associated with this process
		const char *pchProcessType = pkvProcess->GetString( "type" );
		if( !pchProcessType || !pchProcessType[ 0 ] )
		{
			AssertMsg( false, "Process %s defined in the config with no process type associated with it", pchProcessName );
			return false;
		}

		//create our new process info
		CGCDirProcess *pProcess = new CGCDirProcess( m_vecProcesses.Count( ), pchProcessName, pchProcessType, pkvProcess );
		m_vecProcesses.AddToTail( pProcess );

		FOR_EACH_SUBKEY( pkvProcess, pkvType )
		{
			if( 0 != Q_stricmp( pkvType->GetName(), "gc" ) )
				continue;

			const char *pchGCType = NULL;
			if( pkvType->GetFirstSubKey() )
			{
				pchGCType = pkvType->GetString( "gc" );
			}
			else
			{
				pchGCType = pkvType->GetString( );
			}

			if ( !pchGCType || !pchGCType[0] )
			{
				AssertMsg( false, "GC defined on box \"%s\" in the config with no type", pchProcessName );
				return false;
			}

			int32 nGCType = GetGCTypeForName( pchGCType );
			if ( s_nInvalidGCType == nGCType )
			{
				AssertMsg( false, "GC defined on box \"%s\" with unknown type \"%s\" encountered while parsing the config", pchProcessName, pchGCType );
				return false;
			}

			//note that to get the count we can't call GetGCCountForType until after we register since otherwise we don't have a type entry
			int nGCTypeIndex = m_mapGCsByType.Find( nGCType );
			if ( !m_mapGCsByType.IsValidIndex( nGCType ) )
			{
				nGCTypeIndex = m_mapGCsByType.Insert( nGCType );
			}

			CGCDirTypeInstance *pGC = new CGCDirTypeInstance( pchGCType, nGCType, m_mapGCsByType[ nGCTypeIndex ].Count(), pkvType, pProcess );
			pProcess->AddGC( pGC );
			m_vecTypeInstances.AddToTail( pGC );
			m_mapGCsByType[ nGCTypeIndex ].AddToTail( pGC );
		}
	}

	// There must be exactly one master GC defined. Make sure it exists
	int32 nMasterType = GetGCTypeForName( "master" );
	if ( s_nInvalidGCType == nMasterType )
	{
		AssertMsg( false, "Master GC type is not registered" );
		return false;
	}

	if ( 1 != GetGCCountForType( nMasterType ) )
	{
		AssertMsg( false, "Incorrect number of master GCs in the config. Expected: 1 Actual: %d", GetGCCountForType( nMasterType ) );
		return false;
	}

	const CGCDirTypeInstance *pGC = GetGCInstanceForType( nMasterType, 0 );
	if ( 0 != pGC->GetProcess()->GetGCDirIndex() )
	{
		AssertMsg( false, "The master GC must be contained within the first GC defined." );
		return false;
	}

	m_bInitialized = true;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Registers a type of GC
//-----------------------------------------------------------------------------
void CDirectory::RegisterGCType( int32 nTypeID, const char *pchName )
{
	Assert( s_nInvalidGCType != nTypeID );
	if ( s_nInvalidGCType == nTypeID )
		return;

	bool bHasElement = m_mapRegisteredGCTypes.IsValidIndex( m_mapRegisteredGCTypes.Find( nTypeID ) );
	AssertMsg( !bHasElement, "A GC of type %d has already been registered", nTypeID );
	if ( bHasElement )
		return;

	bHasElement = m_dictRegisteredGCNameToType.HasElement( pchName );
	AssertMsg( !bHasElement, "A GC type with name \"%s\" has already been registered", pchName );
	if ( bHasElement )
		return;

	RegisteredGCType_t &gcReg = m_mapRegisteredGCTypes[ m_mapRegisteredGCTypes.Insert( nTypeID ) ];
	gcReg.m_strName = pchName;

	m_dictRegisteredGCNameToType.Insert( pchName, nTypeID );
}


//-----------------------------------------------------------------------------
// Purpose: Gets the number of boxes hosting GCs in the universe
//-----------------------------------------------------------------------------
uint32 CDirectory::GetProcessCount() const
{
	return m_vecProcesses.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Gets the definition for a particular box
//-----------------------------------------------------------------------------
const CGCDirProcess *CDirectory::GetProcess( int32 nIndex ) const
{
	if( !m_vecProcesses.IsValidIndex( nIndex ) )
	{
		Assert( false );
		return NULL;
	}

	return m_vecProcesses[ nIndex ];
}


//-----------------------------------------------------------------------------
// Purpose: Gets the number of GCs in the universe
//-----------------------------------------------------------------------------
uint32 CDirectory::GetGCTypeInstanceCount() const
{
	return m_vecTypeInstances.Count();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
const CGCDirTypeInstance *CDirectory::GetGCTypeInstance( uint32 iDirGC ) const
{
	if( iDirGC >= ( uint32 )m_vecTypeInstances.Count() )
	{
		Assert( false );
		return NULL;
	}

	return m_vecTypeInstances[ iDirGC ];
}


//-----------------------------------------------------------------------------
// Purpose: Gets the number of GCs in the universe of the given type
//-----------------------------------------------------------------------------
int32 CDirectory::GetGCCountForType( int32 nTypeID ) const
{
	int nIndex = m_mapGCsByType.Find( nTypeID );
	if ( !m_mapGCsByType.IsValidIndex( nIndex ) )
	{
		EmitWarning( SPEW_GC, 2, "CDirectory::GetGCCountForType() called with unregistered type %d (%s)\n", nTypeID, GetNameForGCType( nTypeID ) );
		return 0;
	}

	return m_mapGCsByType[nIndex].Count();
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
const CGCDirTypeInstance *CDirectory::GetGCInstanceForType( int32 nTypeID, int32 nInstance ) const
{
	int nIndex = m_mapGCsByType.Find( nTypeID );
	if ( !m_mapGCsByType.IsValidIndex( nIndex ) )
	{
		EmitWarning( SPEW_GC, 2, "CDirectory::GetGCInstanceForType() called with unregistered type %d (%s)\n", nTypeID, GetNameForGCType( nTypeID ) );
		return NULL;
	}

	const CCopyableUtlVector<CGCDirTypeInstance *> &vecGCs = m_mapGCsByType[ nIndex ];
	bool bValidIndex = vecGCs.IsValidIndex( nInstance );
	Assert( bValidIndex );
	if ( !bValidIndex )
		return NULL;

	return vecGCs[ nInstance ];
}


//-----------------------------------------------------------------------------
// Purpose: Given a type and index, this will hash it to be a valid in-range index for that type and return the directory index
//-----------------------------------------------------------------------------
int32 CDirectory::GetGCDirIndexForInstance( int32 nTypeID, uint32 nInstanceIndex ) const
{
	int nIndex = m_mapGCsByType.Find( nTypeID );
	if ( !m_mapGCsByType.IsValidIndex( nIndex ) )
	{
		EmitWarning( SPEW_GC, 2, "CDirectory::GetGCDirIndexForInstance() called with unregistered type %d (%s)\n", nTypeID, GetNameForGCType( nTypeID ) );
		return -1;
	}

	const CCopyableUtlVector<CGCDirTypeInstance *> &vecGCs = m_mapGCsByType[ nIndex ];
	uint32 nWrappedIndex = nInstanceIndex % vecGCs.Count();
	return vecGCs[ nWrappedIndex ]->GetProcess()->GetGCDirIndex();
}

//-----------------------------------------------------------------------------
// Purpose: given a GC type, this will add all of the GC indices associated with that type onto the provided vector
//-----------------------------------------------------------------------------
void CDirectory::GetGCDirIndicesForType( int32 nTypeID, CUtlVector< uint32 >& vecIndices ) const
{
	int nIndex = m_mapGCsByType.Find( nTypeID );
	if ( m_mapGCsByType.IsValidIndex( nIndex ) )
	{
		const CCopyableUtlVector<CGCDirTypeInstance *> &vecGCs = m_mapGCsByType[ nIndex ];
		FOR_EACH_VEC( vecGCs, nGC )
		{
			vecIndices.AddToTail( vecGCs[ nGC ]->GetProcess()->GetGCDirIndex() );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Gets a name for a registered GC type
//-----------------------------------------------------------------------------
const char *CDirectory::GetNameForGCType( int32 nTypeID ) const
{
	int nIndex = m_mapRegisteredGCTypes.Find( nTypeID );
	if ( !m_mapRegisteredGCTypes.IsValidIndex( nIndex ) )
		return "unknown";

	return m_mapRegisteredGCTypes[nIndex].m_strName.Get();
}


//-----------------------------------------------------------------------------
// Purpose: Gets the creation factory for a registered GC types
//-----------------------------------------------------------------------------
CDirectory::GCFactory_t CDirectory::GetFactoryForProcessType( const char* pszProcessType ) const
{
	int nIndex = m_dictProcessTypeToFactory.Find( pszProcessType );
	if( nIndex == m_dictProcessTypeToFactory.InvalidIndex() )
		return NULL;
	return m_dictProcessTypeToFactory[ nIndex ];
}

//-----------------------------------------------------------------------------
// Purpose: Registers a factory for a specific process type
//-----------------------------------------------------------------------------
void CDirectory::RegisterProcessTypeFactory( const char* pszProcessType, GCFactory_t pfnFactory )
{
	m_dictProcessTypeToFactory.Insert( pszProcessType, pfnFactory );
}


//-----------------------------------------------------------------------------
// Purpose: Gets the type for a GC given a name
//-----------------------------------------------------------------------------
int32 CDirectory::GetGCTypeForName( const char *pchName ) const
{
	int nIndex = m_dictRegisteredGCNameToType.Find( pchName );
	if ( !m_dictRegisteredGCNameToType.IsValidIndex( nIndex ) )
		return s_nInvalidGCType;

	return m_dictRegisteredGCNameToType[nIndex];
}


//-----------------------------------------------------------------------------
// Purpose: Gets the global directory singleton
//-----------------------------------------------------------------------------
CDirectory *GDirectory()
{
	static CDirectory g_directory;
	return &g_directory;
}


} // namespace GCSDK
