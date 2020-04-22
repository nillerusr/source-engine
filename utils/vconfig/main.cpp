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

#include "tier0/icommandline.h"
#include "inputsystem/iinputsystem.h"
#include "appframework/tier3app.h"
#include "vconfig_main.h"
#include "VConfigDialog.h"
#include "ConfigManager.h"
#include "steam/steam_api.h"
#include <iregistry.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#define VCONFIG_MAIN_PATH_ID	"MAIN"

CVConfigDialog *g_pMainFrame = 0;
char g_engineDir[50];


// Dummy window
static WNDCLASS staticWndclass = { NULL };
static ATOM staticWndclassAtom = 0;
static HWND staticHwnd = 0;

// List of our game configs, as read from the gameconfig.txt file
CGameConfigManager			g_ConfigManager;
CUtlVector<CGameConfig *>	g_Configs;
HANDLE g_dwChangeHandle = NULL;
CSteamAPIContext g_SteamAPIContext;
CSteamAPIContext *steamapicontext = &g_SteamAPIContext;

//-----------------------------------------------------------------------------
// Purpose: Copy a string into a CUtlVector of characters
//-----------------------------------------------------------------------------
void UtlStrcpy( CUtlVector<char> &dest, const char *pSrc )
{
	dest.EnsureCount( (int) (strlen( pSrc ) + 1) );
	Q_strncpy( dest.Base(), pSrc, dest.Count() );
}

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

// Fetch the engine version for when running in steam.
void GetEngineVersion(char* pcEngineVer, int nSize)
{
	IRegistry *reg = InstanceRegistry( "Source SDK" );
	Assert( reg );
	V_strncpy( pcEngineVer, reg->ReadString( "EngineVer", "orangebox" ), nSize );
	ReleaseInstancedRegistry( reg );
}
//-----------------------------------------------------------------------------
// Purpose: Add a new configuration with proper defaults to a keyvalue block
//-----------------------------------------------------------------------------
bool AddConfig( int configID )
{
	// Find the games block of the keyvalues
	KeyValues *gameBlock = g_ConfigManager.GetGameBlock();
	
	if ( gameBlock == NULL )
	{
		Assert( 0 );
		return false;
	}

	// Set to defaults
	defaultConfigInfo_t newInfo;
	memset( &newInfo, 0, sizeof( newInfo ) );

	// Data for building the new configuration
	const char *pModName = g_Configs[configID]->m_Name.Base();
	const char *pModDirectory = g_Configs[configID]->m_ModDir.Base();
	
	// Mod name
	Q_strncpy( newInfo.gameName, pModName, sizeof( newInfo.gameName ) );
	
	// FGD
	Q_strncpy( newInfo.FGD, "base.fgd", sizeof( newInfo.FGD ) );

	// Get the base directory
	Q_FileBase( pModDirectory, newInfo.gameDir, sizeof( newInfo.gameDir ) );

	// Default executable
	Q_strncpy( newInfo.exeName, "hl2.exe", sizeof( newInfo.exeName ) );

	char szPath[MAX_PATH];
	Q_strncpy( szPath, pModDirectory, sizeof( szPath ) );
	Q_StripLastDir( szPath, sizeof( szPath ) );
	Q_StripTrailingSlash( szPath );

	char fullDir[MAX_PATH];
	g_ConfigManager.GetRootGameDirectory( fullDir, sizeof( fullDir ), g_ConfigManager.GetRootDirectory() );
	
	return g_ConfigManager.AddDefaultConfig( newInfo, gameBlock, szPath, fullDir );
}

//-----------------------------------------------------------------------------
// Purpose: Remove a configuration from the data block
//-----------------------------------------------------------------------------
bool RemoveConfig( int configID )
{
	if ( !g_ConfigManager.IsLoaded() )
		return false;

	// Find the games block of the keyvalues
	KeyValues *gameBlock = g_ConfigManager.GetGameBlock();
	
	if ( gameBlock == NULL )
	{
		Assert( 0 );
		return false;
	}

	int i = 0;

	// Iterate through all subkeys
	for ( KeyValues *pGame=gameBlock->GetFirstTrueSubKey(); pGame; pGame=pGame->GetNextTrueSubKey(), i++ )
	{
		if ( i == configID )
		{
			KeyValues *pOldGame = pGame;
			pGame = pGame->GetNextTrueSubKey();

			gameBlock->RemoveSubKey( pOldGame );
			pOldGame->deleteThis();

			if ( pGame == NULL )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Updates the internal data of the keyvalue buffer with the edited info
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool UpdateConfigs( void )
{
	if ( !g_ConfigManager.IsLoaded() )
		return false;

	// Find the games block of the keyvalues
	KeyValues *gameBlock = g_ConfigManager.GetGameBlock();
	
	if ( gameBlock == NULL )
	{
		Assert( 0 );
		return false;
	}

	int i = 0;

	// Stomp parsed data onto the contained keyvalues
	for ( KeyValues *pGame=gameBlock->GetFirstTrueSubKey(); pGame != NULL; pGame=pGame->GetNextTrueSubKey(), i++ )
	{
		pGame->SetName( g_Configs[i]->m_Name.Base() );
		pGame->SetString( TOKEN_GAME_DIRECTORY, g_Configs[i]->m_ModDir.Base() );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Saves out changes to the config file
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool SaveConfigs( void )
{
	// Move the internal changes up to the base data stored in the config manager
	if ( UpdateConfigs() == false )
		return false;

	// Save out the data
	if ( g_ConfigManager.SaveConfigs() == false )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Read the information we use out of the configs
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ParseConfigs( void )
{
	if ( !g_ConfigManager.IsLoaded() )
		return false;

	// Find the games block of the keyvalues
	KeyValues *gameBlock = g_ConfigManager.GetGameBlock();
	
	if ( gameBlock == NULL )
	{
		Assert( 0 );
		return false;
	}

	// Iterate through all subkeys
	for ( KeyValues *pGame=gameBlock->GetFirstTrueSubKey(); pGame; pGame=pGame->GetNextTrueSubKey() )
	{
		const char *pName = pGame->GetName();
		const char *pDir = pGame->GetString( TOKEN_GAME_DIRECTORY );

		CGameConfig	*newConfig = new CGameConfig( pName, pDir );
		g_Configs.AddToTail( newConfig );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Startup our file watch
//-----------------------------------------------------------------------------
void UpdateConfigsStatus_Init( void )
{
	// Watch our config file for changes
	if ( g_dwChangeHandle == NULL)
	{
		char szConfigDir[MAX_PATH];
		Q_strncpy( szConfigDir, GetBaseDirectory(), sizeof( szConfigDir ) );

		g_dwChangeHandle = FindFirstChangeNotification( 
			szConfigDir,													// directory to watch 
			false,															// watch the subtree 
			FILE_NOTIFY_CHANGE_LAST_WRITE );								// watch file and dir name changes 
 
		if ( g_dwChangeHandle == INVALID_HANDLE_VALUE )
		{
			// FIXME: Unable to watch the file
		}
	}
}	 

//-----------------------------------------------------------------------------
// Purpose: Reload and re-parse our configuration data
//-----------------------------------------------------------------------------
void ReloadConfigs( bool bNoWarning /*= false*/ )
{
	g_Configs.PurgeAndDeleteElements();
	ParseConfigs();
	g_pMainFrame->PopulateConfigList( bNoWarning );
}

//-----------------------------------------------------------------------------
// Purpose: Update our status
//-----------------------------------------------------------------------------
void UpdateConfigsStatus( void )
{
 	// Wait for notification.
 	DWORD dwWaitStatus = WaitForSingleObject( g_dwChangeHandle, 0 );

	if ( dwWaitStatus == WAIT_OBJECT_0 )
	{
		// Something in the watched folder changed!
		if ( g_pMainFrame != NULL )
		{
			// Reload the configs
			g_ConfigManager.LoadConfigs();
			
			// Reparse the configurations
			ReloadConfigs();
		}
		
		// Start the next update
		if ( FindNextChangeNotification( g_dwChangeHandle ) == FALSE )
		{
			// This means that something unknown happened to our search handle!
			Assert( 0 );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stop watching the file
//-----------------------------------------------------------------------------
void UpdateConfigsStatus_Shutdown( void )
{
	FindCloseChangeNotification( g_dwChangeHandle );
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
			g_pMainFrame->PopulateConfigList();
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
	staticWndclass.lpszClassName = "VConfig_Window";
	staticWndclassAtom = ::RegisterClass( &staticWndclass );

	// Create an empty window just for message handling
	staticHwnd = CreateWindowEx(0, "VConfig_Window", "Hidden Window", 0, 0, 0, 1, 1, NULL, NULL, GetModuleHandle(NULL), NULL);
}
 
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ShutdownMessageWindow( void )
{
	// Kill our windows instance
	::DestroyWindow( staticHwnd );
	::UnregisterClass("VConfig_Window", ::GetModuleHandle(NULL));
}


//-----------------------------------------------------------------------------
// Sets up, shuts down vgui
//-----------------------------------------------------------------------------
bool InitializeVGUI( void )
{
	vgui::ivgui()->SetSleep(false);

	// Init the surface
	vgui::Panel *pPanel = new vgui::Panel( NULL, "TopPanel" );
	pPanel->SetVisible(true);

	vgui::surface()->SetEmbeddedPanel(pPanel->GetVPanel());

	// load the scheme
	vgui::scheme()->LoadSchemeFromFile( "vconfig_scheme.res", NULL );

	// localization
	g_pVGuiLocalize->AddFile( "resource/platform_%language%.txt");
	g_pVGuiLocalize->AddFile( "vgui/resource/vgui_%language%.txt" );
	g_pVGuiLocalize->AddFile( "vconfig_english.txt");

	// Start vgui
	vgui::ivgui()->Start();

	// add our main window
	g_pMainFrame = new CVConfigDialog( pPanel, "VConfigDialog" );

	// show main window
	g_pMainFrame->MoveToCenterOfScreen();
	g_pMainFrame->Activate();
	g_pMainFrame->SetSizeable( false );
	g_pMainFrame->SetMenuButtonVisible( true );

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
// Points the maya script to the appropriate place
//-----------------------------------------------------------------------------
void SetMayaScriptSettings( )
{
	char pMayaScriptPath[ MAX_PATH ];
	Q_snprintf( pMayaScriptPath, sizeof(pMayaScriptPath), "%%VPROJECT%%\\..\\sdktools\\maya\\scripts" );
	SetVConfigRegistrySetting( "MAYA_SCRIPT_PATH", pMayaScriptPath, false );
}


//-----------------------------------------------------------------------------
// Points the XSI script to the appropriate place
//-----------------------------------------------------------------------------
void SetXSIScriptSettings( )
{
	// Determine the currently installed version of XSI
	char *pXSIVersion = "5.1";
	
	// FIXME: We need a way of knowing the current version of XSI being used 
	// so we can set up the appropriate search paths. There's no easy way of doing this currently
	// so I'm defining my own environment variable
	char pXSIVersionBuf[ MAX_PATH ];
	if ( GetVConfigRegistrySetting( "XSI_VERSION", pXSIVersionBuf, sizeof(pXSIVersionBuf) ) )
	{
		pXSIVersion = pXSIVersionBuf;
	}

	char pXSIPluginPath[ MAX_PATH ];
	Q_snprintf( pXSIPluginPath, sizeof(pXSIPluginPath), "%%VPROJECT%%\\..\\sdktools\\xsi\\%s\\valvesource", pXSIVersion );
	SetVConfigRegistrySetting( "XSI_PLUGINS", pXSIPluginPath, false );
	SetVConfigRegistrySetting( "XSI_VERSION", pXSIVersion, false );
}


//-----------------------------------------------------------------------------
// Points the XSI script to the appropriate place
//-----------------------------------------------------------------------------
#define VPROJECT_BIN_PATH "%vproject%\\..\\bin"

void SetPathSettings( )
{
	char pPathBuf[ MAX_PATH*32 ];
	if ( GetVConfigRegistrySetting( "PATH", pPathBuf, sizeof(pPathBuf) ) )
	{
		Q_FixSlashes( pPathBuf );
		const char *pPath = pPathBuf;
		const char *pFound = Q_stristr( pPath, VPROJECT_BIN_PATH );
		int nLen = Q_strlen( VPROJECT_BIN_PATH );
		while ( pFound )
		{
			if ( pFound[nLen] == '\\' )
			{
				++nLen;
			}
			if ( !pFound[nLen] || pFound[nLen] == ';' )
				return;

			pPath += nLen;
			pFound = Q_stristr( pPath, VPROJECT_BIN_PATH );
		}

		Q_strncat( pPathBuf, ";%VPROJECT%\\..\\bin", sizeof(pPathBuf) );
	}
	else
	{
		Q_strncpy( pPathBuf, "%VPROJECT%\\..\\bin", sizeof(pPathBuf) );
	}

	SetVConfigRegistrySetting( "PATH", pPathBuf, false );
}


//-----------------------------------------------------------------------------
// Spew func
//-----------------------------------------------------------------------------
SpewRetval_t VConfig_SpewOutputFunc( SpewType_t type, char const *pMsg )
{
#ifdef _DEBUG
	OutputDebugString( pMsg );
#endif

	switch( type )
	{
	case SPEW_ERROR:
		::MessageBox( NULL, pMsg, "VConfig Error", MB_OK );
		return SPEW_ABORT;
		
	case SPEW_ASSERT:
		return SPEW_DEBUGGER;
	}

	return SPEW_CONTINUE;
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CVConfigApp : public CVguiSteamApp
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

DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CVConfigApp );


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CVConfigApp::Create()
{
	SpewOutputFunc( VConfig_SpewOutputFunc );

	// If they pass in -game, just set the registry key to the value they asked for.
	const char *pSetGame = CommandLine()->ParmValue( "-game" );
	if ( pSetGame )
	{
		SetMayaScriptSettings( );
		SetXSIScriptSettings( );
		SetPathSettings( );
		SetVConfigRegistrySetting( GAMEDIR_TOKEN, pSetGame );
		return false;
	}

	AppSystemInfo_t appSystems[] = 
	{
		{ "inputsystem.dll",		INPUTSYSTEM_INTERFACE_VERSION },
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	return AddSystems( appSystems );
}


//-----------------------------------------------------------------------------
// Pre-init
//-----------------------------------------------------------------------------
bool CVConfigApp::PreInit()
{
	if ( !BaseClass::PreInit() )
		return false;

	// Create a window to capture messages
	CreateMessageWindow();

	// Make sure we're using the proper environment variable
	ConvertObsoleteVConfigRegistrySetting( GAMEDIR_TOKEN );

	FileSystem_SetErrorMode( FS_ERRORMODE_AUTO );

	// We only want to use the gameinfo.txt that is in the bin\vconfig directory.
	char dirName[MAX_PATH];
	Q_strncpy( dirName, GetBaseDirectory(), sizeof( dirName ) );
	Q_AppendSlash( dirName, sizeof( dirName ) );
	Q_strncat( dirName, "vconfig", sizeof( dirName ), COPY_ALL_CHARACTERS );

	if ( !SetupSearchPaths( dirName, true, true ) )
	{
		::MessageBox( NULL, "Error", "Unable to initialize file system\n", MB_OK );
		return false;
	}

	// Load our configs
	if ( g_ConfigManager.LoadConfigs() == false )
	{
		::MessageBox( NULL, "Error", "Unable to load configuration file\n", MB_OK );
		return false;
	}

	// Parse them for internal use
	if ( ParseConfigs() == false )
	{
		::MessageBox( NULL, "Error", "Unable to parse configuration file\n", MB_OK );
		return false;
	}

	// Start looking for file updates
	UpdateConfigsStatus_Init();

	// the "base dir" so we can scan mod name
	g_pFullFileSystem->AddSearchPath( GetBaseDirectory(), VCONFIG_MAIN_PATH_ID );	

	// the main platform dir
	g_pFullFileSystem->AddSearchPath( "platform","PLATFORM", PATH_ADD_TO_HEAD );

	return true;
}


//-----------------------------------------------------------------------------
// Pre-init
//-----------------------------------------------------------------------------
void CVConfigApp::PostShutdown()
{
	// Stop our message window
	ShutdownMessageWindow();

	// Clear our configs
	g_Configs.PurgeAndDeleteElements();

	// Stop file notifications
	UpdateConfigsStatus_Shutdown();

	BaseClass::PostShutdown();
}


//-----------------------------------------------------------------------------
// Purpose: Main function
//-----------------------------------------------------------------------------
int CVConfigApp::Main()
{
	if ( !InitializeVGUI() )
		return 0;

	SteamAPI_InitSafe();
	SteamAPI_SetTryCatchCallbacks( false ); // We don't use exceptions, so tell steam not to use try/catch in callback handlers
	g_SteamAPIContext.Init();

	GetEngineVersion( g_engineDir, sizeof( g_engineDir ) );

	// Run the app
	while ( vgui::ivgui()->IsRunning() )
	{
		Sleep( 10 );
		UpdateConfigsStatus();
		vgui::ivgui()->RunFrame();
	}

	ShutdownVGUI();

	return 1;
}

