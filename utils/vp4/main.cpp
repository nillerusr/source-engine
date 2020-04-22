//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#define NOWINRES
#define NOSERVICE
#define NOMCX
#define NOIME
#include <windows.h>

#undef MessageBox
#undef PostMessage

#include "stdafx.h"
#include "appframework/tier3app.h"
#include "tier2/tier2.h"
#include "inputsystem/iinputsystem.h"
#include "vgui_controls/controls.h"

// root panel
vgui::Panel *g_pMainPanel = NULL;


//-----------------------------------------------------------------------------
// Purpose: Adds in any search paths
//-----------------------------------------------------------------------------
void AddFileSystemSearchPaths(const char *pszExeName)
{
	// search locally first
	char pExeName[MAX_PATH];
    if ( ::GetModuleFileName( ( HINSTANCE )GetModuleHandle( NULL ), pExeName, sizeof(pExeName) ) )
	{
		char pPlatform[MAX_PATH];
		Q_StripFilename( pExeName );
		Q_snprintf( pPlatform, sizeof(pPlatform), "%s\\..\\platform", pExeName );
		g_pFullFileSystem->AddSearchPath( pExeName, "EXECUTABLE_PATH");
		g_pFullFileSystem->AddSearchPath( pPlatform, "PLATFORM");
		g_pFullFileSystem->AddSearchPath( pPlatform, "SKIN");
	}
	else
	{
		g_pFullFileSystem->AddSearchPath(".", "EXECUTABLE_PATH");
		g_pFullFileSystem->AddSearchPath("../platform/", "PLATFORM");
		g_pFullFileSystem->AddSearchPath("../platform/", "SKIN");
	}

	// add self as a pack file
//	g_pFullFileSystem->AddPackFile(pszExeName, "PLATFORM");
}


//-----------------------------------------------------------------------------
// Purpose: Sets up the main vgui
//-----------------------------------------------------------------------------
bool InitializeVGui( )
{
	// add in the search paths
	AddFileSystemSearchPaths(NULL);

	// Init the surface
	g_pMainPanel = new vgui::Panel(NULL, NULL);
	vgui::surface()->SetEmbeddedPanel( g_pMainPanel->GetVPanel() );

	// load the scheme
	g_pMainPanel->SetScheme( vgui::scheme()->LoadSchemeFromFile( "//PLATFORM/Resource/sourcescheme.res", "PLATFORM" ) );

	// localization
	g_pVGuiLocalize->AddFile( "Resource/platform_%language%.txt");
	g_pVGuiLocalize->AddFile( "Resource/vgui_%language%.txt");

	// configuration settings
	vgui::system()->SetUserConfigFile( "vp4config.txt", "EXECUTABLE_PATH" );

	// Start vgui
	vgui::ivgui()->Start();

	// finish setting up main panel
	vgui::SETUP_PANEL( g_pMainPanel );

	return true;
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CVP4App : public CVguiSteamApp
{
public:
	// Methods of IApplication
	virtual bool Create();
	virtual int Main();
	virtual void Destroy() {}
};

DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CVP4App );


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CVP4App::Create()
{
	AppSystemInfo_t appSystems[] = 
	{
		{ "inputsystem.dll",		INPUTSYSTEM_INTERFACE_VERSION },
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },
		{ "p4lib.dll",				P4_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	return AddSystems( appSystems );
}


//-----------------------------------------------------------------------------
// Purpose: program entrypoint
//-----------------------------------------------------------------------------
int CVP4App::Main()
{
	if ( !InitializeVGui( ) )
	{
		::MessageBoxA( NULL, "Fatal Error: Could not initialize vgui.", "Steam - Fatal Error", MB_OK | MB_ICONERROR );
		return 0;
	}

	// open the wizard
	CVP4Dialog *dlg = SETUP_PANEL(new CVP4Dialog());
	dlg->SetParent(g_pMainPanel);
	dlg->Activate();

	// run vgui
	while (vgui::ivgui()->IsRunning())
	{
		vgui::ivgui()->RunFrame();
	}

	// save configuration
	vgui::system()->SaveUserConfigFile();

	// delete all the panels
	delete g_pMainPanel;

	return 0;
}
