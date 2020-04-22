//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
// scenemanager.cpp : Defines the entry point for the console application.
//

#include "cbase.h"
#include "appframework/tier3app.h"
#include "workspacemanager.h"
#include "filesystem.h"
#include "FileSystem_Tools.h"
#include "cmdlib.h"
#include "vstdlib/random.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "iscenemanagersound.h"
#include <vgui/ILocalize.h>
#include <vgui/IVGui.h>
#include "tier0/icommandline.h"
#include "icvar.h"
#include "vstdlib/cvar.h"
#include "mathlib/mathlib.h"

char cmdline[1024] = "";

static CUniformRandomStream g_Random;
IUniformRandomStream *random = &g_Random;

IFileSystem *filesystem = NULL;

SpewRetval_t SceneManagerSpewFunc( SpewType_t spewType, char const *pMsg )
{
	switch (spewType)
	{
	case SPEW_ERROR:
		{
			MessageBox(NULL, pMsg, "FATAL ERROR", MB_OK);
		}
		return SPEW_ABORT;
	case SPEW_WARNING:
		{
			Con_ColorPrintf( 255, 0, 0, pMsg );
		}
		break;
	case SPEW_ASSERT:
		{
			Con_ColorPrintf( 255, 0, 0, pMsg );
		}
#ifdef _DEBUG
		return SPEW_DEBUGGER;
#else
		return SPEW_CONTINUE;
#endif
	default:
		{
			Con_Printf(pMsg);
		}
		break;
	}

	return SPEW_CONTINUE;
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CHLSceneManagerApp : public CTier3SteamApp
{
	typedef CTier3SteamApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual void PostShutdown();
	virtual void Destroy();

private:
	// Sets up the search paths
	bool SetupSearchPaths();
};


bool CHLSceneManagerApp::Create()
{
	SpewOutputFunc( SceneManagerSpewFunc );

	AppSystemInfo_t appSystems[] = 
	{
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },
		{ "soundemittersystem.dll",	SOUNDEMITTERSYSTEM_INTERFACE_VERSION },

		{ "", "" }	// Required to terminate the list
	};

	return AddSystems( appSystems );
}

void CHLSceneManagerApp::Destroy()
{
}

//-----------------------------------------------------------------------------
// Sets up the game path
//-----------------------------------------------------------------------------
bool CHLSceneManagerApp::SetupSearchPaths()
{
	if ( !BaseClass::SetupSearchPaths( NULL, false, true ) )
		return false;

	// Set gamedir.
	Q_MakeAbsolutePath( gamedir, sizeof( gamedir ), GetGameInfoPath() );
	Q_AppendSlash( gamedir, sizeof( gamedir ) );

	return true;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CHLSceneManagerApp::PreInit( )
{
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );

	if ( !BaseClass::PreInit() )
		return false;

	g_pFileSystem = filesystem = g_pFullFileSystem;
	if ( !g_pSoundEmitterSystem || !g_pVGuiLocalize || !g_pFileSystem  )
	{
		Error("Unable to load required library interface!\n");
		return false;
	}

	filesystem->SetWarningFunc( Warning );

	// Add paths...
	if ( !SetupSearchPaths() )
		return false;

	return true; 
}

void CHLSceneManagerApp::PostShutdown()
{
	g_pFileSystem = filesystem = NULL;
	BaseClass::PostShutdown();
}


//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CHLSceneManagerApp::Main()
{
	g_pSoundEmitterSystem->ModInit();

	sound->Init();

	CWorkspaceManager *sm = new CWorkspaceManager();

	bool workspace_loaded = false;
	for ( int i = 1; i < CommandLine()->ParmCount(); i++ )
	{
		char const *argv = CommandLine()->GetParm( i );

		if ( !workspace_loaded && strstr (argv, ".vsw") )
		{
			workspace_loaded = true;

			// Strip game directory and slash
			char workspace_name[ 512 ];
			filesystem->FullPathToRelativePath( argv, workspace_name, sizeof( workspace_name ) );

			sm->AutoLoad( workspace_name );
		}
	}

	if ( !workspace_loaded )
	{
		sm->AutoLoad( NULL );
	}
	
	int retval = mx::run ();
	
	sound->Shutdown();

	g_pSoundEmitterSystem->ModShutdown();

	return retval;
}

int main (int argc, char *argv[])
{
	CommandLine()->CreateCmdLine( argc, argv );
	CoInitialize(NULL);

	// make sure, we start in the right directory
	char szName[256];
	strcpy (szName, mx::getApplicationPath() );
	mx::init (argc, argv);

	char workingdir[ 256 ];
	workingdir[0] = 0;
	Q_getwd( workingdir, sizeof( workingdir ) );

 	CHLSceneManagerApp sceneManagerApp;
	CSteamApplication steamApplication( &sceneManagerApp );
	int nRetVal = steamApplication.Run();

	CoUninitialize();

	return nRetVal;
}
