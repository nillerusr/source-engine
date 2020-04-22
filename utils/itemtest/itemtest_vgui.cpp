//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=============================================================================

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>			// HINSTANCE


// Valve includes
#include "inputsystem/iinputsystem.h"
#include "itemtest/itemtest.h"
#include "itemtest/itemtest_controls.h"
#include "p4lib/ip4.h"
#include "tier0/icommandline.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui_controls/consoledialog.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/Panel.h"


// Local includes
#include "itemtestapp.h"


// Last include
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CConsoleDialogNew;
SpewRetval_t ConsoleDialogSpewFunc( SpewType_t spewType, const tchar *pMsg );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static vgui::Panel *g_pTopPanel = NULL;
static vgui::DHANDLE< CConsoleDialogNew > g_hConsoleDialog;
static vgui::DHANDLE< vgui::Frame > g_hMainFrame;


//=============================================================================
//
//=============================================================================
class CItemTestVGUIApp : public CItemTestApp
{
	typedef CItemTestApp BaseClass;

public:
	virtual bool Create();
	virtual int Main();

	// Methods of IApplication
	virtual bool PreInit();

protected:
	static vgui::Panel *InitializeVGUI();

	static void ShutdownVGUI();

};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CItemTestVGUIApp );


//=============================================================================
//
//=============================================================================
class CConsoleDialogNew : public vgui::CConsoleDialog
{
	DECLARE_CLASS_SIMPLE( CConsoleDialogNew, vgui::CConsoleDialog );

public:
	CConsoleDialogNew( vgui::Panel *pPanel, const char *pszName )
	: CConsoleDialog( pPanel, pszName, false )
	{
	}

	virtual void OnCommandSubmitted( const char *pszCommand )
	{
		if ( !V_stricmp( pszCommand, "quit" ) )
		{
			if ( g_hMainFrame )
				g_hMainFrame->Close();

			Close();
		}
		else if ( !V_stricmp( pszCommand, "help" ) )
		{
			__s_ApplicationObject.PrintHelp();
		}
		else
		{
			Warning( "Error! Unknown command \"%s\"\n", pszCommand );
		}
	}

};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemTestVGUIApp::Create()
{
	AppSystemInfo_t appSystems[] = 
	{
		{ "inputsystem.dll",		INPUTSYSTEM_INTERFACE_VERSION },
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	if ( FindParam( kDev ) && !FindParam( kNoP4 ) )
	{
		AppModule_t p4Module = LoadModule( "p4lib.dll" );
		AddSystem( p4Module, P4_INTERFACE_VERSION );
	}

	return AddSystems( appSystems );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CItemTestVGUIApp::PreInit()
{
	if ( !BaseClass::PreInit() )
		return false;

	CreateInterfaceFn factory = GetFactory();
	return vgui::VGui_InitInterfacesList( "CVguiSteamApp", &factory, 1 );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CItemTestVGUIApp::Main()
{
	g_pTopPanel = InitializeVGUI();
	if ( !g_pTopPanel )
		return 1;

	SpewOutputFunc( ConsoleDialogSpewFunc );

	if ( !g_hMainFrame )
	{
		// add our main window
		CItemUploadWizard *pItemUploadWizard = new CItemUploadWizard( g_pTopPanel, "Item Upload Wizard" );
		if ( pItemUploadWizard )
		{
			g_hMainFrame = pItemUploadWizard;

			CAssetTF &assetTF = pItemUploadWizard->Asset();

			ProcessCommandLine( &assetTF, false );
			pItemUploadWizard->UpdateGUI();
		}
	}

	if ( !g_hConsoleDialog )
	{
		if ( g_pTopPanel )
		{
			CConsoleDialogNew *pConsoleDialog = g_hMainFrame ?
				new CConsoleDialogNew( g_hMainFrame, "console" ) :
				new CConsoleDialogNew( g_pTopPanel, "console" );

			if ( pConsoleDialog )
			{
				g_hConsoleDialog = pConsoleDialog;
				if ( !g_hMainFrame )
				{
					g_hMainFrame = g_hConsoleDialog;

					g_hConsoleDialog->SetSize( 640, 480 );
					g_hConsoleDialog->MoveToCenterOfScreen();
				}
				else
				{
					int nWide = 0;
					int nTall = 0;
					int nX = 0;
					int nY = 0;

					g_hMainFrame->GetBounds( nX, nY, nWide, nTall );

					g_hConsoleDialog->SetSize( nWide, MAX( nTall / 3, 120 ) );
					g_hConsoleDialog->SetPos( nX, nY + nTall + 3 );
				}

				g_hConsoleDialog->Activate();
				g_hConsoleDialog->SetVisible( true );
				g_hConsoleDialog->SetDeleteSelfOnClose( true );
				g_hConsoleDialog->ColorPrint( Color( 255, 192, 0, 255 ), "\nNOTE" );
				g_hConsoleDialog->Print( ": The only commands available are 'quit' and 'help'\n" );
			}
		}
	}

	if ( g_hMainFrame )
	{
		// show main window
		g_hMainFrame->SetSizeable( true );
		g_hMainFrame->SetMenuButtonVisible( true );
		g_hMainFrame->MoveToCenterOfScreen();
		g_hMainFrame->Activate();

		// Run the app
		while ( vgui::ivgui()->IsRunning() && g_hMainFrame )
		{
			vgui::ivgui()->RunFrame();
		}
	}

	ShutdownVGUI();

	return 0;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
vgui::Panel *CItemTestVGUIApp::InitializeVGUI()
{
	// Init the surface
	vgui::Panel *pTopPanel = new vgui::Panel( NULL, "TopPanel" );
	if ( !pTopPanel )
		return NULL;

	pTopPanel->SetVisible( true );

	vgui::surface()->SetEmbeddedPanel( pTopPanel->GetVPanel() );

	// load the scheme
	vgui::scheme()->LoadSchemeFromFile( "resource/itemtest_scheme.res", NULL );

	// localization
	g_pVGuiLocalize->AddFile( "resource/platform_%language%.txt");
	g_pVGuiLocalize->AddFile( "resource/vgui_%language%.txt" );

	g_pVGuiLocalize->AddFile( "resource/itemtest_%language%.txt");
	g_pVGuiLocalize->AddFile( "resource/itemtest_english.txt");

	// Start vgui
	vgui::ivgui()->Start();


	return pTopPanel;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CItemTestVGUIApp::ShutdownVGUI()
{
	if ( g_hConsoleDialog )
		g_hConsoleDialog->Close();

	if ( g_hMainFrame )
		g_hMainFrame->Close();

	if ( !g_pTopPanel )
		return;

	delete g_pTopPanel;
	g_pTopPanel = NULL;
}


//-----------------------------------------------------------------------------
// Spew func
//-----------------------------------------------------------------------------
SpewRetval_t ConsoleDialogSpewFunc( SpewType_t spewType, const tchar *pMsg )
{
	vgui::CConsoleDialog *pConsole = g_hConsoleDialog;

	if ( !pConsole )
		return SPEW_CONTINUE;

	OutputDebugString( pMsg );

	switch( spewType )
	{
	case SPEW_ASSERT:
		pConsole->ColorPrint( Color( 255, 192, 0, 255 ), pMsg );
#ifdef _DEBUG
		return SPEW_DEBUGGER;
#else
		return SPEW_CONTINUE;
#endif

	case SPEW_ERROR:
		pConsole->ColorPrint( Color( 255, 0, 0, 255 ), pMsg );
		break;

	case SPEW_WARNING:
		pConsole->ColorPrint( Color( 192, 192, 0, 255 ), pMsg );
		break;

	case SPEW_MESSAGE:
		{
			Color c = *GetSpewOutputColor();
			if ( !V_stricmp( GetSpewOutputGroup(), "developer" ) )
			{
				pConsole->Print( pMsg );
			}
			else
			{
				pConsole->ColorPrint( c, pMsg );
			}
		}
		break;
	}

	if ( vgui::ivgui()->IsRunning() && g_hMainFrame )
	{
		vgui::ivgui()->RunFrame();
	}

	return SPEW_CONTINUE;
}