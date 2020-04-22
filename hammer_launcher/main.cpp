//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Launcher for hammer, which is sitting in its own DLL
//
//===========================================================================//

#include <windows.h>
#include <eh.h>
#include "appframework/AppFramework.h"
#include "ihammer.h"
#include "tier0/dbg.h"
#include "vstdlib/cvar.h"
#include "filesystem.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"
#include "filesystem_init.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "vgui/ivgui.h"
#include "vgui/ISurface.h"
#include "inputsystem/iinputsystem.h"
#include "tier0/icommandline.h"
#include "p4lib/ip4.h"

//-----------------------------------------------------------------------------
// Global systems
//-----------------------------------------------------------------------------
IHammer *g_pHammer;
IMaterialSystem *g_pMaterialSystem;
IFileSystem *g_pFileSystem;
IDataCache *g_pDataCache;
IInputSystem *g_pInputSystem;

//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CHammerApp : public CAppSystemGroup
{
public:
	// Methods of IApplication
	virtual bool Create( );
	virtual bool PreInit( );
	virtual int Main( );
	virtual void PostShutdown();
	virtual void Destroy();

private:
	int	MainLoop();
};


//-----------------------------------------------------------------------------
// Define the application object
//-----------------------------------------------------------------------------
CHammerApp	g_ApplicationObject;
DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR( g_ApplicationObject );


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CHammerApp::Create( )
{
	// Save some memory so engine/hammer isn't so painful
	CommandLine()->AppendParm( "-disallowhwmorph", NULL );

	IAppSystem *pSystem;

	// Add in the cvar factory
	AppModule_t cvarModule = LoadModule( VStdLib_GetICVarFactory() );
	pSystem = AddSystem( cvarModule, CVAR_INTERFACE_VERSION );
	if ( !pSystem )
		return false;
	
	bool bSteam;
	char pFileSystemDLL[MAX_PATH];
	if ( FileSystem_GetFileSystemDLLName( pFileSystemDLL, MAX_PATH, bSteam ) != FS_OK )
		return false;

	AppModule_t fileSystemModule = LoadModule( pFileSystemDLL );
	g_pFileSystem = (IFileSystem*)AddSystem( fileSystemModule, FILESYSTEM_INTERFACE_VERSION );

	FileSystem_SetBasePaths( g_pFileSystem );

	AppSystemInfo_t appSystems[] = 
	{
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ "inputsystem.dll",		INPUTSYSTEM_INTERFACE_VERSION },
		{ "studiorender.dll",		STUDIO_RENDER_INTERFACE_VERSION },
		{ "vphysics.dll",			VPHYSICS_INTERFACE_VERSION },
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },
		{ "datacache.dll",			MDLCACHE_INTERFACE_VERSION },
		{ "datacache.dll",			STUDIO_DATA_CACHE_INTERFACE_VERSION },
		{ "vguimatsurface.dll",		VGUI_SURFACE_INTERFACE_VERSION },
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },
		{ "hammer_dll.dll",			INTERFACEVERSION_HAMMER },
		{ "", "" }	// Required to terminate the list
	};

	if ( !AddSystems( appSystems ) ) 
		return false;

	// Add Perforce separately since it's possible it isn't there. (SDK)
	if ( !CommandLine()->CheckParm( "-nop4" ) )
	{
		AppModule_t p4Module = LoadModule( "p4lib.dll" );
		AddSystem( p4Module, P4_INTERFACE_VERSION );
	}
	// Connect to interfaces loaded in AddSystems that we need locally
	g_pMaterialSystem = (IMaterialSystem*)FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );
	g_pHammer = (IHammer*)FindSystem( INTERFACEVERSION_HAMMER );
	g_pDataCache = (IDataCache*)FindSystem( DATACACHE_INTERFACE_VERSION );
	g_pInputSystem = (IInputSystem*)FindSystem( INPUTSYSTEM_INTERFACE_VERSION );

	// This has to be done before connection.
	g_pMaterialSystem->SetShaderAPI( "shaderapidx9.dll" );

	return true;
}

void CHammerApp::Destroy()
{
	g_pFileSystem = NULL;
	g_pMaterialSystem = NULL;
	g_pDataCache = NULL;
	g_pHammer = NULL;
	g_pInputSystem = NULL;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
SpewRetval_t HammerSpewFunc( SpewType_t type, tchar const *pMsg )
{
	if ( type == SPEW_ASSERT )
	{
		return SPEW_DEBUGGER;
	}
	else if( type == SPEW_ERROR )
	{
		MessageBox( NULL, pMsg, "Hammer Error", MB_OK | MB_ICONSTOP );
		return SPEW_ABORT;
	}
	else
	{
		return SPEW_CONTINUE;
	}
}

//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CHammerApp::PreInit( )
{
	SpewOutputFunc( HammerSpewFunc );
	if ( !g_pHammer->InitSessionGameConfig( GetVProjectCmdLineValue() ) )
		return false;

	//
	// Init the game and mod dirs in the file system.
	// This needs to happen before calling Init on the material system.
	//
	CFSSearchPathsInit initInfo;
	initInfo.m_pFileSystem = g_pFileSystem;
	initInfo.m_pDirectoryName = g_pHammer->GetDefaultModFullPath();

	if ( FileSystem_LoadSearchPaths( initInfo ) != FS_OK )
	{
		Error( "Unable to load search paths!\n" );
	}

	// Required to run through the editor
	g_pMaterialSystem->EnableEditorMaterials();

	// needed for VGUI model rendering
	g_pMaterialSystem->SetAdapter( 0, MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE );

	return true; 
}

void CHammerApp::PostShutdown()
{
}


//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CHammerApp::Main( )
{
	return g_pHammer->MainLoop();
}





