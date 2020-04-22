//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include "interface.h"

#include <windows.h>
//#include "..\..\tracker\common\winlite.h"
#include "vgui_controls/Controls.h"
#include "vgui/vgui.h"
#include "VGUI\IPanel.h"
#include "VGUI\IScheme.h"
#include "VGUI\ISurface.h"
#include "VGUI\ILocalize.h"
#include "VGUI\IVGui.h"
#include "vgui_controls/Panel.h"
#include "filesystem.h"
#include "tier0/icommandline.h"
#include "appframework/tier3app.h"
#include "inputsystem/iinputsystem.h"
#include "CControlCatalog.h"

#include <stdio.h>

//-----------------------------------------------------------------------------
// Purpose: Warning/Msg call back through this API
// Input  : type - 
//			*pMsg - 
// Output : SpewRetval_t
//-----------------------------------------------------------------------------
SpewRetval_t SpewFunc( SpewType_t type, char const *pMsg )
{	
	switch ( type )
	{

	default:
	case SPEW_MESSAGE:
	case SPEW_ASSERT:
	case SPEW_LOG:
		OutputDebugString( pMsg );
		break;
	case SPEW_WARNING:
		OutputDebugString( pMsg );
		break;
	case SPEW_ERROR:
		OutputDebugString( pMsg );
		exit( -1 );
		break;
	}
	
	return SPEW_CONTINUE;
}

//-----------------------------------------------------------------------------
// Purpose: Entry point
//			loads interfaces and initializes dialog
//-----------------------------------------------------------------------------
static CreateInterfaceFn s_pFactoryList[2];

void *VGuiFactory( const char *pName, int *pReturnCode )
{
	for ( int i = 0; i < ARRAYSIZE( s_pFactoryList ); ++i )
	{
		void *pInterface = s_pFactoryList[i]( pName, pReturnCode );
		if ( pInterface )
			return pInterface;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CPanelZooApp : public CVguiSteamApp
{
	typedef CVguiSteamApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual void Destroy() {}
};

DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CPanelZooApp );


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CPanelZooApp::Create()
{
	SpewOutputFunc( SpewFunc );
	SpewActivate( "panelzoo", 2 );

	AppSystemInfo_t appSystems[] = 
	{
		{ "inputsystem.dll",		INPUTSYSTEM_INTERFACE_VERSION },
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	return AddSystems( appSystems );
}


//-----------------------------------------------------------------------------
// Setup
//-----------------------------------------------------------------------------
bool CPanelZooApp::PreInit()
{
	if ( !BaseClass::PreInit() )
		return false;

	if ( !BaseClass::SetupSearchPaths( NULL, false, true ) )
	{
		::MessageBox( NULL, "Error", "Unable to initialize file system\n", MB_OK );
		return false;
	}

	g_pFullFileSystem->AddSearchPath( "platform", "PLATFORM" );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Entry point
//-----------------------------------------------------------------------------
int CPanelZooApp::Main()
{
	// In order to load resource files the file must be in your vgui filesystem path.
//	g_pFullFileSystem->AddSearchPath("../", "resources");

	// Init the surface
//	vgui::surface()->Init();

	// Make a embedded panel
	vgui::Panel *panel = new vgui::Panel(NULL, "TopPanel");
	vgui::surface()->SetEmbeddedPanel( panel->GetVPanel() );

	// Load the scheme
	if (!vgui::scheme()->LoadSchemeFromFile( "//platform/Resource/SourceScheme.res", "PANELZOO" ))
		return 1;

	// localization
	g_pVGuiLocalize->AddFile( "Resource/platform_english.txt" );
	g_pVGuiLocalize->AddFile( "Resource/valve_%language%.txt" );
	g_pVGuiLocalize->AddFile( "Resource/vgui_%language%.txt" );

	// Start vgui
	vgui::ivgui()->Start();

	// Add our main window
	CControlCatalog *panelZoo = new CControlCatalog();
	panelZoo->Activate();

	// Run app frame loop
	while (vgui::ivgui()->IsRunning())
	{
		vgui::ivgui()->RunFrame();
	}

	delete panelZoo;
//	delete panel;

	return 1;
}






