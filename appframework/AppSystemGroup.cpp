//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines a group of app systems that all have the same lifetime
// that need to be connected/initialized, etc. in a well-defined order
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//

#include "appframework/IAppSystemGroup.h"
#include "appframework/IAppSystem.h"
#include "interface.h"
#include "filesystem.h"
#include "filesystem_init.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
//extern ILoggingListener *g_pDefaultLoggingListener;


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CAppSystemGroup::CAppSystemGroup( CAppSystemGroup *pAppSystemParent ) : m_SystemDict(false, 0, 16)
{
	m_pParentAppSystem = pAppSystemParent;
}


//-----------------------------------------------------------------------------
// Actually loads a DLL
//-----------------------------------------------------------------------------
CSysModule *CAppSystemGroup::LoadModuleDLL( const char *pDLLName )
{
	return Sys_LoadModule( pDLLName );
}


//-----------------------------------------------------------------------------
// Methods to load + unload DLLs
//-----------------------------------------------------------------------------
AppModule_t CAppSystemGroup::LoadModule( const char *pDLLName )
{
	// Remove the extension when creating the name.
	int nLen = Q_strlen( pDLLName ) + 1;
	char *pModuleName = (char*)stackalloc( nLen );
	Q_StripExtension( pDLLName, pModuleName, nLen );

	// See if we already loaded it...
	for ( int i = m_Modules.Count(); --i >= 0; ) 
	{
		if ( m_Modules[i].m_pModuleName )
		{
			if ( !Q_stricmp( pModuleName, m_Modules[i].m_pModuleName ) )
				return i;
		}
	}

	CSysModule *pSysModule = LoadModuleDLL( pDLLName );
	if (!pSysModule)
	{
		Warning("AppFramework : Unable to load module %s!\n", pDLLName );
		return APP_MODULE_INVALID;
	}

	int nIndex = m_Modules.AddToTail();
	m_Modules[nIndex].m_pModule = pSysModule;
	m_Modules[nIndex].m_Factory = 0;
	m_Modules[nIndex].m_pModuleName = (char*)malloc( nLen );
	Q_strncpy( m_Modules[nIndex].m_pModuleName, pModuleName, nLen );

	return nIndex;
}

AppModule_t CAppSystemGroup::LoadModule( CreateInterfaceFn factory )
{
	if (!factory)
	{
		Warning("AppFramework : Unable to load module %p!\n", factory );
		return APP_MODULE_INVALID;
	}

	// See if we already loaded it...
	for ( int i = m_Modules.Count(); --i >= 0; ) 
	{
		if ( m_Modules[i].m_Factory )
		{
			if ( m_Modules[i].m_Factory == factory )
				return i;
		}
	}

	int nIndex = m_Modules.AddToTail();
	m_Modules[nIndex].m_pModule = NULL;
	m_Modules[nIndex].m_Factory = factory;
	m_Modules[nIndex].m_pModuleName = NULL; 
	return nIndex;
}

void CAppSystemGroup::UnloadAllModules()
{
	// NOTE: Iterate in reverse order so they are unloaded in opposite order
	// from loading
	for (int i = m_Modules.Count(); --i >= 0; )
	{
		if ( m_Modules[i].m_pModule )
		{
			Sys_UnloadModule( m_Modules[i].m_pModule );
		}
		if ( m_Modules[i].m_pModuleName )
		{
			free( m_Modules[i].m_pModuleName );
		}
	}
	m_Modules.RemoveAll();
}


//-----------------------------------------------------------------------------
// Methods to add/remove various global singleton systems 
//-----------------------------------------------------------------------------
IAppSystem *CAppSystemGroup::AddSystem( AppModule_t module, const char *pInterfaceName )
{
	if (module == APP_MODULE_INVALID)
		return NULL;

	Assert( (module >= 0) && (module < m_Modules.Count()) );
	CreateInterfaceFn pFactory = m_Modules[module].m_pModule ? Sys_GetFactory( m_Modules[module].m_pModule ) : m_Modules[module].m_Factory;

	int retval;
	void *pSystem = pFactory( pInterfaceName, &retval );
	if ((retval != IFACE_OK) || (!pSystem))
	{
		Warning("AppFramework : Unable to create system %s!\n", pInterfaceName );
		return NULL;
	}

	IAppSystem *pAppSystem = static_cast<IAppSystem*>(pSystem);
	
	int sysIndex = m_Systems.AddToTail( pAppSystem );

	// Inserting into the dict will help us do named lookup later
	MEM_ALLOC_CREDIT();
	m_SystemDict.Insert( pInterfaceName, sysIndex );
	return pAppSystem;
}

static char const *g_StageLookup[] = 
{
	"CREATION",
	"CONNECTION",
	"PREINITIALIZATION",
	"INITIALIZATION",
	"SHUTDOWN",
	"POSTSHUTDOWN",
	"DISCONNECTION",
	"DESTRUCTION",
	"NONE",
};

void CAppSystemGroup::ReportStartupFailure( int nErrorStage, int nSysIndex )
{
	char const *pszStageDesc = "Unknown";
	if ( nErrorStage >= 0 && nErrorStage < ARRAYSIZE( g_StageLookup ) )
	{
		pszStageDesc = g_StageLookup[ nErrorStage ];
	}

	char const *pszSystemName = "(Unknown)";
	for ( int i = m_SystemDict.First(); i != m_SystemDict.InvalidIndex(); i = m_SystemDict.Next( i ) )
	{
		if ( m_SystemDict[ i ] != nSysIndex )
			continue;

		pszSystemName = m_SystemDict.GetElementName( i );
		break;
	}
		 
	// Walk the dictionary
	Warning( "System (%s) failed during stage %s\n", pszSystemName, pszStageDesc );
}

void CAppSystemGroup::AddSystem( IAppSystem *pAppSystem, const char *pInterfaceName )
{
	if ( !pAppSystem )
		return;

	int sysIndex = m_Systems.AddToTail( pAppSystem );

	// Inserting into the dict will help us do named lookup later
	MEM_ALLOC_CREDIT();
	m_SystemDict.Insert( pInterfaceName, sysIndex );
}

void CAppSystemGroup::RemoveAllSystems()
{
	// NOTE: There's no deallcation here since we don't really know
	// how the allocation has happened. We could add a deallocation method
	// to the code in interface.h; although when the modules are unloaded
	// the deallocation will happen anyways
	m_Systems.RemoveAll();
	m_SystemDict.RemoveAll();
}


//-----------------------------------------------------------------------------
// Simpler method of doing the LoadModule/AddSystem thing.
//-----------------------------------------------------------------------------
bool CAppSystemGroup::AddSystems( AppSystemInfo_t *pSystemList )
{
	while ( pSystemList->m_pModuleName[0] )
	{
		AppModule_t module = LoadModule( pSystemList->m_pModuleName );
		IAppSystem *pSystem = AddSystem( module, pSystemList->m_pInterfaceName );
		if ( !pSystem )
		{
			Warning( "Unable to load interface %s from %s\n", pSystemList->m_pInterfaceName, pSystemList->m_pModuleName );
			return false;
		}
		++pSystemList;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Methods to find various global singleton systems 
//-----------------------------------------------------------------------------
void *CAppSystemGroup::FindSystem( const char *pSystemName )
{
	unsigned short i = m_SystemDict.Find( pSystemName );
	if (i != m_SystemDict.InvalidIndex())
		return m_Systems[m_SystemDict[i]];

	// If it's not an interface we know about, it could be an older
	// version of an interface, or maybe something implemented by
	// one of the instantiated interfaces...

	// QUESTION: What order should we iterate this in?
	// It controls who wins if multiple ones implement the same interface
 	for ( i = 0; i < m_Systems.Count(); ++i )
	{
		void *pInterface = m_Systems[i]->QueryInterface( pSystemName );
		if (pInterface)
			return pInterface;
	}

	if ( m_pParentAppSystem )
	{
		void* pInterface = m_pParentAppSystem->FindSystem( pSystemName );
		if ( pInterface )
			return pInterface;
	}

	// No dice..
	return NULL;
}


//-----------------------------------------------------------------------------
// Gets at the parent appsystem group
//-----------------------------------------------------------------------------
CAppSystemGroup *CAppSystemGroup::GetParent()
{
	return m_pParentAppSystem;
}

	
//-----------------------------------------------------------------------------
// Method to connect/disconnect all systems
//-----------------------------------------------------------------------------
bool CAppSystemGroup::ConnectSystems()
{
	for (int i = 0; i < m_Systems.Count(); ++i )
	{
		IAppSystem *sys = m_Systems[i];

		if (!sys->Connect( GetFactory() ))
		{
			ReportStartupFailure( CONNECTION, i );
			return false;
		}
	}
	return true;
}

void CAppSystemGroup::DisconnectSystems()
{
	// Disconnect in reverse order of connection
	for (int i = m_Systems.Count(); --i >= 0; )
	{
		m_Systems[i]->Disconnect();
	}
}


//-----------------------------------------------------------------------------
// Method to initialize/shutdown all systems
//-----------------------------------------------------------------------------
InitReturnVal_t CAppSystemGroup::InitSystems()
{
	for (int i = 0; i < m_Systems.Count(); ++i )
	{
		InitReturnVal_t nRetVal = m_Systems[i]->Init();
		if ( nRetVal != INIT_OK )
		{
			ReportStartupFailure( INITIALIZATION, i );
			return nRetVal;
		}
	}
	return INIT_OK;
}

void CAppSystemGroup::ShutdownSystems()
{
	// Shutdown in reverse order of initialization
	for (int i = m_Systems.Count(); --i >= 0; )
	{
		m_Systems[i]->Shutdown();
	}
}


//-----------------------------------------------------------------------------
// Returns the stage at which the app system group ran into an error
//-----------------------------------------------------------------------------
CAppSystemGroup::AppSystemGroupStage_t CAppSystemGroup::GetErrorStage() const
{
	return m_nErrorStage;
}


//-----------------------------------------------------------------------------
// Gets at a factory that works just like FindSystem
//-----------------------------------------------------------------------------
// This function is used to make this system appear to the outside world to
// function exactly like the currently existing factory system
CAppSystemGroup *s_pCurrentAppSystem;
void *AppSystemCreateInterfaceFn(const char *pName, int *pReturnCode)
{
	void *pInterface = s_pCurrentAppSystem->FindSystem( pName );
	if ( pReturnCode )
	{
		*pReturnCode = pInterface ? IFACE_OK : IFACE_FAILED;
	}
	return pInterface;
}


//-----------------------------------------------------------------------------
// Gets at a class factory for the topmost appsystem group in an appsystem stack
//-----------------------------------------------------------------------------
CreateInterfaceFn CAppSystemGroup::GetFactory()
{
	return AppSystemCreateInterfaceFn;
}

	
//-----------------------------------------------------------------------------
// Main application loop
//-----------------------------------------------------------------------------
int CAppSystemGroup::Run()
{	
	// The factory now uses this app system group
	s_pCurrentAppSystem	= this;

	// Load, connect, init
	int nRetVal = OnStartup();
 	if ( m_nErrorStage != NONE )
		return nRetVal;

	// Main loop implemented by the application
	// FIXME: HACK workaround to avoid vgui porting
	nRetVal = Main();

	// Shutdown, disconnect, unload
	OnShutdown();

	// The factory now uses the parent's app system group
	s_pCurrentAppSystem	= GetParent();

	return nRetVal;
}


//-----------------------------------------------------------------------------
// Virtual methods for override
//-----------------------------------------------------------------------------
int CAppSystemGroup::Startup()
{
	return OnStartup();
}

void CAppSystemGroup::Shutdown()
{
	return OnShutdown();
}


//-----------------------------------------------------------------------------
// Use this version in cases where you can't control the main loop and
// expect to be ticked
//-----------------------------------------------------------------------------
int CAppSystemGroup::OnStartup()
{
	// The factory now uses this app system group
	s_pCurrentAppSystem	= this;

 	m_nErrorStage = NONE;

	// Call an installed application creation function
	if ( !Create() )
	{
		m_nErrorStage = CREATION;
		return -1;
	}

	// Let all systems know about each other
	if ( !ConnectSystems() )
	{
		m_nErrorStage = CONNECTION;
		return -1;
	}

	// Allow the application to do some work before init
	if ( !PreInit() )
	{
		m_nErrorStage = PREINITIALIZATION;
		return -1;
	}

	// Call Init on all App Systems
	int nRetVal = InitSystems();
	if ( nRetVal != INIT_OK )
	{
		m_nErrorStage = INITIALIZATION;
		return -1;
	}

	return nRetVal;
}

void CAppSystemGroup::OnShutdown()
{
	// The factory now uses this app system group
	s_pCurrentAppSystem	= this;

	switch( m_nErrorStage )
	{
	case NONE:
		break;

	case PREINITIALIZATION:
	case INITIALIZATION:
		goto disconnect;
	
	case CREATION:
	case CONNECTION:
		goto destroy;
	}

	// Cal Shutdown on all App Systems
	ShutdownSystems();

	// Allow the application to do some work after shutdown
	PostShutdown();

disconnect:
	// Systems should disconnect from each other
	DisconnectSystems();

destroy:
	// Unload all DLLs loaded in the AppCreate block
	RemoveAllSystems();

	// Have to do this because the logging listeners & response policies may live in modules which are being unloaded
	// @TODO: this seems like a bad legacy practice... app systems should unload their spew handlers gracefully.
//	LoggingSystem_ResetCurrentLoggingState();
//	Assert( g_pDefaultLoggingListener != NULL );
//	LoggingSystem_RegisterLoggingListener( g_pDefaultLoggingListener );

	UnloadAllModules();

	// Call an installed application destroy function
	Destroy();
}


	
//-----------------------------------------------------------------------------
//
// This class represents a group of app systems that are loaded through steam
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSteamAppSystemGroup::CSteamAppSystemGroup( IFileSystem *pFileSystem, CAppSystemGroup *pAppSystemParent )
{
	m_pFileSystem = pFileSystem;
	m_pGameInfoPath[0] = 0;
}


//-----------------------------------------------------------------------------
// Used by CSteamApplication to set up necessary pointers if we can't do it in the constructor
//-----------------------------------------------------------------------------
void CSteamAppSystemGroup::Setup( IFileSystem *pFileSystem, CAppSystemGroup *pParentAppSystem )
{
	m_pFileSystem = pFileSystem;
	m_pParentAppSystem = pParentAppSystem;
}


//-----------------------------------------------------------------------------
// Loads the module from Steam
//-----------------------------------------------------------------------------
CSysModule *CSteamAppSystemGroup::LoadModuleDLL( const char *pDLLName )
{
	return m_pFileSystem->LoadModule( pDLLName );
}


//-----------------------------------------------------------------------------
// Returns the game info path
//-----------------------------------------------------------------------------
const char *CSteamAppSystemGroup::GetGameInfoPath()	const
{
	return m_pGameInfoPath;
}


//-----------------------------------------------------------------------------
// Sets up the search paths
//-----------------------------------------------------------------------------
bool CSteamAppSystemGroup::SetupSearchPaths( const char *pStartingDir, bool bOnlyUseStartingDir, bool bIsTool )
{
	CFSSteamSetupInfo steamInfo;
	steamInfo.m_pDirectoryName = pStartingDir;
	steamInfo.m_bOnlyUseDirectoryName = bOnlyUseStartingDir;
	steamInfo.m_bToolsMode = bIsTool;
	steamInfo.m_bSetSteamDLLPath = true;
	steamInfo.m_bSteam = m_pFileSystem->IsSteam();
	if ( FileSystem_SetupSteamEnvironment( steamInfo ) != FS_OK )
		return false;

	CFSMountContentInfo fsInfo;
	fsInfo.m_pFileSystem = m_pFileSystem;
	fsInfo.m_bToolsMode = bIsTool;
	fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;

	if ( FileSystem_MountContent( fsInfo ) != FS_OK )
		return false;

	// Finally, load the search paths for the "GAME" path.
	CFSSearchPathsInit searchPathsInit;
	searchPathsInit.m_pDirectoryName = steamInfo.m_GameInfoPath;
	searchPathsInit.m_pFileSystem = fsInfo.m_pFileSystem;
	if ( FileSystem_LoadSearchPaths( searchPathsInit ) != FS_OK )
		return false;

	FileSystem_AddSearchPath_Platform( fsInfo.m_pFileSystem, steamInfo.m_GameInfoPath );
	Q_strncpy( m_pGameInfoPath, steamInfo.m_GameInfoPath, sizeof(m_pGameInfoPath) );
	return true;
}
