//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Pieces of the application framework, shared between POSIX systems (Mac OS X, Linux, etc)
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//
#include "appframework/AppFramework.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "interface.h"
#include "filesystem.h"
#include "appframework/IAppSystemGroup.h"
#include "filesystem_init.h"
#include "tier1/convar.h"
#include "vstdlib/cvar.h"
#include "togl/rendermechanism.h"

// NOTE: This has to be the last file included! (turned off below, since this is included like a header)
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Globals...
//-----------------------------------------------------------------------------
HINSTANCE s_HInstance;

//#if !defined(LINUX)
//static CSimpleLoggingListener s_SimpleLoggingListener;
//ILoggingListener *g_pDefaultLoggingListener = &s_SimpleLoggingListener;
//#endif

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
// Version of AppMain used by windows applications
//-----------------------------------------------------------------------------

int AppMain( void* hInstance, void* hPrevInstance, const char* lpCmdLine, int nCmdShow, CAppSystemGroup *pAppSystemGroup )
{
	Assert( 0 );
	return -1;
}

//#if !defined(LINUX)
//static CNonFatalLoggingResponsePolicy s_NonFatalLoggingResponsePolicy;
//#endif

//-----------------------------------------------------------------------------
// Version of AppMain used by console applications
//-----------------------------------------------------------------------------
int AppMain( int argc, char **argv, CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );

	//#if !defined(LINUX)
	//	LoggingSystem_SetLoggingResponsePolicy( &s_NonFatalLoggingResponsePolicy );
	//#endif
	s_HInstance = NULL;
	CommandLine()->CreateCmdLine( argc, argv );

	return pAppSystemGroup->Run( );
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
}


//-----------------------------------------------------------------------------
// Create necessary interfaces
//-----------------------------------------------------------------------------
bool CSteamApplication::Create( )
{
	FileSystem_SetErrorMode( FS_ERRORMODE_NONE );

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
bool CSteamApplication::PreInit( )
{
	return true;
}

void CSteamApplication::PostShutdown( )
{
}


//-----------------------------------------------------------------------------
// Run steam main loop
//-----------------------------------------------------------------------------
int CSteamApplication::Main( )
{
	// Now that Steam is loaded, we can load up main libraries through steam
	m_pChildAppSystemGroup->Setup( m_pFileSystem, this );
	return m_pChildAppSystemGroup->Run( );
}


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

// Turn off memdbg macros (turned on up top) since this is included like a header
#include "tier0/memdbgoff.h"

