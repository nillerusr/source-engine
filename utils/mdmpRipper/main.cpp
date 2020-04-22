//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Configuration utility
//
//===========================================================================//

#include <windows.h>
#include <io.h>
#include <stdio.h>

#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Panel.h>
#include "appframework/tier3app.h"
#include "inputsystem/iinputsystem.h"
#include "tier0/icommandline.h"
#include "filesystem_init.h"
#include "CMDRipperMain.h"
#include "isqlwrapper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#define MDRIPPER_MAIN_PATH_ID	"MAIN"
#define MDRIPPER_WRITE_PATH     "DEFAULT_WRITE_PATH"

CMDRipperMain *g_pMainFrame = 0;
ISQLWrapper *g_pSqlWrapper;

// Dummy window
static WNDCLASS staticWndclass = { NULL };
static ATOM staticWndclassAtom = 0;
static HWND staticHwnd = 0;

// List of our game configs, as read from the gameconfig.txt file
//CGameConfigManager			g_ConfigManager;
//CUtlVector<CGameConfig *>	g_Configs;
HANDLE g_dwChangeHandle = NULL;


//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *GetBaseDirectory( void )
{
	static char path[MAX_PATH] = {0};
	if ( path[0] == 0 )
	{
		GetModuleFileName( (HMODULE)GetAppInstance(), path, sizeof( path ) );
		Q_StripLastDir( path, sizeof( path ) );	// Get rid of the filename.
		Q_StripTrailingSlash( path );
	}
	return path;
}


//-----------------------------------------------------------------------------
// Purpose: Message handler for dummy app
//-----------------------------------------------------------------------------
static LRESULT CALLBACK messageProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
	// See if we've gotten a VPROJECT change
	if ( msg == WM_SETTINGCHANGE )
	{
		if ( g_pMainFrame != NULL )
		{
			// Reset the list and pop an error if they've chosen something we don't understand
			//g_pMainFrame->PopulateConfigList();
		}
	}
 
	return ::DefWindowProc(hwnd,msg,wparam,lparam);
}

//-----------------------------------------------------------------------------
// Purpose: Creates a dummy window that handles windows messages
//-----------------------------------------------------------------------------
void CreateMessageWindow( void )
{
	// Make and register a very simple window class
	memset(&staticWndclass, 0, sizeof(staticWndclass));
	staticWndclass.style = 0;
	staticWndclass.lpfnWndProc = messageProc;
	staticWndclass.hInstance = GetModuleHandle(NULL);
	staticWndclass.lpszClassName = "minidumpRipper_Window";
	staticWndclassAtom = ::RegisterClass( &staticWndclass );

	// Create an empty window just for message handling
	staticHwnd = CreateWindowEx(0, "minidumpRipper_Window", "Hidden Window", 0, 0, 0, 1, 1, NULL, NULL, GetModuleHandle(NULL), NULL);
}
 
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ShutdownMessageWindow( void )
{
	// Kill our windows instance
	::DestroyWindow( staticHwnd );
	::UnregisterClass("minidumpRipper_Window", ::GetModuleHandle(NULL));
}


//-----------------------------------------------------------------------------
// Purpose: Setup all our VGUI info
//-----------------------------------------------------------------------------
bool InitializeVGUI( void )
{
	vgui::ivgui()->SetSleep(false);

	// Init the surface
	vgui::Panel *pPanel = new vgui::Panel( NULL, "TopPanel" );
	pPanel->SetVisible(true);

	vgui::surface()->SetEmbeddedPanel(pPanel->GetVPanel());

	// load the scheme
	vgui::scheme()->LoadSchemeFromFile( "resource/sourcescheme.res", NULL );

	// localization
	g_pVGuiLocalize->AddFile( "resource/platform_%language%.txt" );
	g_pVGuiLocalize->AddFile( "resource/vgui_%language%.txt" );
	g_pVGuiLocalize->AddFile( "mdmpRipper_english.txt" );

	// Start vgui
	vgui::ivgui()->Start();

	// add our main window
	g_pMainFrame = new CMDRipperMain( pPanel, "CMDRipperMain" );

	// show main window
	g_pMainFrame->MoveToCenterOfScreen();
	g_pMainFrame->Activate();
	g_pMainFrame->SetSizeable( true );
	g_pMainFrame->SetMenuButtonVisible( true );

	g_pSqlWrapper = g_pMainFrame->GetSqlWrapper();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Stop VGUI
//-----------------------------------------------------------------------------
void ShutdownVGUI( void )
{
	delete g_pMainFrame;
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CMDRipperApp : public CVguiSteamApp
{
	typedef CVguiSteamApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual void PostShutdown();
	virtual void Destroy() {}
};

DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CMDRipperApp );


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CMDRipperApp::Create()
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
bool CMDRipperApp::PreInit()
{
	if ( !BaseClass::PreInit() )
		return false;

	// Create a window to capture messages
	CreateMessageWindow();

	FileSystem_SetErrorMode( FS_ERRORMODE_AUTO );

	// We only want to use the gameinfo.txt that is in the bin\vconfig directory.
	char dirName[MAX_PATH];
	Q_strncpy( dirName, GetBaseDirectory(), sizeof( dirName ) );
	Q_AppendSlash( dirName, sizeof( dirName ) );
	Q_strncat( dirName, "minidumpRipper", sizeof( dirName ), COPY_ALL_CHARACTERS );

	if ( !BaseClass::SetupSearchPaths( dirName, true, true ) )
	{
		::MessageBox( NULL, "Error", "Unable to initialize file system\n", MB_OK );
		return false;
	}

	// the "base dir" so we can scan mod name
	g_pFullFileSystem->AddSearchPath(GetBaseDirectory(), MDRIPPER_MAIN_PATH_ID);	

	// the main platform dir
	g_pFullFileSystem->AddSearchPath("platform","PLATFORM", PATH_ADD_TO_HEAD);
	g_pFullFileSystem->AddSearchPath(".\\minidumpRipper\\",MDRIPPER_WRITE_PATH, PATH_ADD_TO_HEAD);

	return true;
}


void CMDRipperApp::PostShutdown()
{
	// Stop our message window
	ShutdownMessageWindow();

	BaseClass::PostShutdown();
}


//-----------------------------------------------------------------------------
// Purpose: Entry point
//-----------------------------------------------------------------------------
int CMDRipperApp::Main()
{
	// Run app frame loop
	if ( !InitializeVGUI() )
		return 0;

	// Run the app
	while (vgui::ivgui()->IsRunning())
	{
		Sleep( 10 );
		vgui::ivgui()->RunFrame();
	}

	ShutdownVGUI();

	return 1;
}
