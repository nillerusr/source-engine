//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: An application framework 
//
//=============================================================================//

#ifdef POSIX
#error
#else
#if defined( _WIN32 ) && !defined( _X360 )
#include <windows.h>
#endif
#include "appframework/appframework.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "interface.h"
#include "filesystem.h"
#include "appframework/iappsystemgroup.h"
#include "filesystem_init.h"
#include "vstdlib/cvar.h"
#include "xbox/xbox_console.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Globals...
//-----------------------------------------------------------------------------
HINSTANCE s_HInstance;

//static CSimpleWindowsLoggingListener s_SimpleWindowsLoggingListener;
//static CSimpleLoggingListener s_SimpleLoggingListener;
//ILoggingListener *g_pDefaultLoggingListener = &s_SimpleLoggingListener;

//-----------------------------------------------------------------------------
// HACK: Since I don't want to refit vgui yet...
//-----------------------------------------------------------------------------
void *GetAppInstance()
{
	return s_HInstance;
}


//-----------------------------------------------------------------------------
// Sets the application instance, should only be used if you're not calling AppMain.
//-----------------------------------------------------------------------------
void SetAppInstance( void* hInstance )
{
	s_HInstance = (HINSTANCE)hInstance;
}

//-----------------------------------------------------------------------------
// Specific 360 environment setup.
//-----------------------------------------------------------------------------
#if defined( _X360 )
bool SetupEnvironment360()
{
	CommandLine()->CreateCmdLine( GetCommandLine() );

	if ( !CommandLine()->FindParm( "-game" ) && !CommandLine()->FindParm( "-vproject" ) )
	{
		// add the default game name due to lack of vproject environment
		CommandLine()->AppendParm( "-game", "hl2" );
	}

	// success
	return true;
}
#endif

//-----------------------------------------------------------------------------
// Version of AppMain used by windows applications
//-----------------------------------------------------------------------------
int AppMain( void* hInstance, void* hPrevInstance, const char* lpCmdLine, int nCmdShow, CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );

//	g_pDefaultLoggingListener = &s_SimpleWindowsLoggingListener;
	s_HInstance = (HINSTANCE)hInstance;
#if !defined( _X360 )
	CommandLine()->CreateCmdLine( ::GetCommandLine() );
#else
	SetupEnvironment360();
#endif

	return pAppSystemGroup->Run();
}

//-----------------------------------------------------------------------------
// Version of AppMain used by console applications
//-----------------------------------------------------------------------------
int AppMain( int argc, char **argv, CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );

//	g_pDefaultLoggingListener = &s_SimpleLoggingListener;
	s_HInstance = NULL;
#if !defined( _X360 )
	CommandLine()->CreateCmdLine( argc, argv );
#else
	SetupEnvironment360();
#endif

	return pAppSystemGroup->Run();
}

//-----------------------------------------------------------------------------
// Used to startup/shutdown the application
//-----------------------------------------------------------------------------
int AppStartup( void* hInstance, void* hPrevInstance, const char* lpCmdLine, int nCmdShow, CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );

//	g_pDefaultLoggingListener = &s_SimpleWindowsLoggingListener;
	s_HInstance = (HINSTANCE)hInstance;
#if !defined( _X360 )
	CommandLine()->CreateCmdLine( ::GetCommandLine() );
#else
	SetupEnvironment360();
#endif

	return pAppSystemGroup->Startup();
}

int AppStartup( int argc, char **argv, CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );

//	g_pDefaultLoggingListener = &s_SimpleLoggingListener;
	s_HInstance = NULL;
#if !defined( _X360 )
	CommandLine()->CreateCmdLine( argc, argv );
#else
	SetupEnvironment360();
#endif

	return pAppSystemGroup->Startup();
}

void AppShutdown( CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );
	pAppSystemGroup->Shutdown();
}


//-----------------------------------------------------------------------------
//
// Default implementation of an application meant to be run using Steam
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSteamApplication::CSteamApplication( CSteamAppSystemGroup *pAppSystemGroup )
{
	m_pChildAppSystemGroup = pAppSystemGroup;
	m_pFileSystem = NULL;
	m_bSteam = false;
}

//-----------------------------------------------------------------------------
// Create necessary interfaces
//-----------------------------------------------------------------------------
bool CSteamApplication::Create()
{
	FileSystem_SetErrorMode( FS_ERRORMODE_AUTO );

	char pFileSystemDLL[MAX_PATH];
	if ( FileSystem_GetFileSystemDLLName( pFileSystemDLL, MAX_PATH, m_bSteam ) != FS_OK )
		return false;
	
	// Add in the cvar factory
	AppModule_t cvarModule = LoadModule( VStdLib_GetICVarFactory() );
	AddSystem( cvarModule, CVAR_INTERFACE_VERSION );

	AppModule_t fileSystemModule = LoadModule( pFileSystemDLL );
	m_pFileSystem = (IFileSystem*)AddSystem( fileSystemModule, FILESYSTEM_INTERFACE_VERSION );
	if ( !m_pFileSystem )
	{
		Error( "Unable to load %s", pFileSystemDLL );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// The file system pointer is invalid at this point
//-----------------------------------------------------------------------------
void CSteamApplication::Destroy()
{
	m_pFileSystem = NULL;
}

//-----------------------------------------------------------------------------
// Pre-init, shutdown
//-----------------------------------------------------------------------------
bool CSteamApplication::PreInit()
{
	return true;
}

void CSteamApplication::PostShutdown()
{
}

//-----------------------------------------------------------------------------
// Run steam main loop
//-----------------------------------------------------------------------------
int CSteamApplication::Main()
{
	// Now that Steam is loaded, we can load up main libraries through steam
	if ( FileSystem_SetBasePaths( m_pFileSystem ) != FS_OK )
		return 0;

	m_pChildAppSystemGroup->Setup( m_pFileSystem, this );
	return m_pChildAppSystemGroup->Run();
}

//-----------------------------------------------------------------------------
// Use this version in cases where you can't control the main loop and
// expect to be ticked
//-----------------------------------------------------------------------------
int CSteamApplication::Startup()
{
	int nRetVal = BaseClass::Startup();
	if ( GetErrorStage() != NONE )
		return nRetVal;

	if ( FileSystem_SetBasePaths( m_pFileSystem ) != FS_OK )
		return 0;

	// Now that Steam is loaded, we can load up main libraries through steam
	m_pChildAppSystemGroup->Setup( m_pFileSystem, this );
	return m_pChildAppSystemGroup->Startup();
}

void CSteamApplication::Shutdown()
{
	m_pChildAppSystemGroup->Shutdown();
	BaseClass::Shutdown();
}

#endif
