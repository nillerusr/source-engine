//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "vgui_controls/Panel.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "filesystem.h"
#include "tier0/icommandline.h"
#include "inputsystem/iinputsystem.h"
#include "appframework/tier3app.h"
#include <windows.h>

//#include "..\..\tracker\common\winlite.h"

#include "LocalizationDialog.h"

#include <stdio.h>


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CVLocalizeApp : public CVguiSteamApp
{
	typedef CVguiSteamApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual void Destroy() {}
};

DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CVLocalizeApp );


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CVLocalizeApp::Create()
{
	AppSystemInfo_t appSystems[] = 
	{
		{ "inputsystem.dll",		INPUTSYSTEM_INTERFACE_VERSION },
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	return AddSystems( appSystems );
}


//-----------------------------------------------------------------------------
// Purpose: Entry point
//-----------------------------------------------------------------------------
bool CVLocalizeApp::PreInit()
{
	if ( !BaseClass::PreInit() )
		return false;

	if ( !BaseClass::SetupSearchPaths( NULL, false, true ) )
	{
		::MessageBox( NULL, "Error", "Unable to initialize file system\n", MB_OK );
		return false;
	}

	g_pFullFileSystem->AddSearchPath("../game/platform", "PLATFORM");
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Entry point
//-----------------------------------------------------------------------------
int CVLocalizeApp::Main()
{
	// load the scheme
	if (!vgui::scheme()->LoadSchemeFromFile("Resource/TrackerScheme.res", "Tracker" ))
		return 1;

	// Init the surface
	vgui::Panel *panel = new vgui::Panel(NULL, "TopPanel");
	vgui::surface()->SetEmbeddedPanel(panel->GetVPanel());

	// Start vgui
	vgui::ivgui()->Start();

	// add our main window
	if ( CommandLine()->ParmCount() < 2 )
	{
		Warning( "Must specify a localization file!\n" );
		return 0;
	}

	const char *pFileName = CommandLine()->GetParm( 1 );
	CLocalizationDialog *dlg = new CLocalizationDialog( pFileName );
	dlg->SetParent( panel->GetVPanel() );
	dlg->MakePopup();
//	dlg->SetBounds( 0, 0, 800, 600 );
	dlg->SetVisible( true );

	// Run app frame loop
	while (vgui::ivgui()->IsRunning())
	{
		vgui::ivgui()->RunFrame();
		vgui::surface()->PaintTraverseEx( panel->GetVPanel(), true );
	}

	return 1;
}
