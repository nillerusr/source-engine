//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include <vgui/isystem.h>

#include <vgui_controls/MessageBox.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>

#include "SDKLauncherDialog.h"
#include "appframework/tier3app.h"
#include "tier0/icommandline.h"
#include "filesystem_tools.h"
#include "sdklauncher_main.h"
#include "configs.h"
#include "min_footprint_files.h"
#include "CreateModWizard.h"
#include "inputsystem/iinputsystem.h"

#include <io.h>
#include <stdio.h>

// Since windows redefines MessageBox.
typedef vgui::MessageBox vguiMessageBox;

#include <winsock2.h>
#include "steam/steam_api.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

HANDLE g_dwChangeHandle = NULL;

#define DEFAULTGAMEDIR_KEYNAME	"DefaultGameDir"

// Dummy window
static WNDCLASS staticWndclass = { NULL };
static ATOM staticWndclassAtom = 0;
static HWND staticHwnd = 0;
CSteamAPIContext g_SteamAPIContext;
CSteamAPIContext *steamapicontext = &g_SteamAPIContext;

// This is the base engine + mod-specific game dir (e.g. "c:\tf2\mytfmod\")
char	gamedir[1024];	
extern	char g_engineDir[50];
CSDKLauncherDialog *g_pMainFrame = 0;


bool g_bAutoHL2Mod = false;
bool g_bModWizard_CmdLineFields = false;
char g_ModWizard_CmdLine_ModDir[MAX_PATH];
char g_ModWizard_CmdLine_ModName[256];
bool g_bAppQuit = false;


//-----------------------------------------------------------------------------
// Purpose: Message handler for dummy app
//-----------------------------------------------------------------------------
static LRESULT CALLBACK messageProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
	// See if we've gotten a VPROJECT change
	if ( msg == WM_SETTINGCHANGE )
	{
		if ( g_pMainFrame != NULL  )
		{
			char szCurrentGame[MAX_PATH];

			// Get VCONFIG from the registry
			GetVConfigRegistrySetting( GAMEDIR_TOKEN, szCurrentGame, sizeof( szCurrentGame ) );

			g_pMainFrame->SetCurrentGame( szCurrentGame );
		}
	} 
 
	return ::DefWindowProc(hwnd,msg,wparam,lparam);
}

const char* GetLastWindowsErrorString()
{
	static char err[2048];
	
	LPVOID lpMsgBuf;
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
	);

	strncpy( err, (char*)lpMsgBuf, sizeof( err ) );
	LocalFree( lpMsgBuf );

	err[ sizeof( err ) - 1 ] = 0;

	return err;
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
	staticWndclass.lpszClassName = "SDKLauncher_Window";
	staticWndclassAtom = ::RegisterClass( &staticWndclass );

	// Create an empty window just for message handling
	staticHwnd = CreateWindowEx(0, "SDKLauncher_Window", "Hidden Window", 0, 0, 0, 1, 1, NULL, NULL, GetModuleHandle(NULL), NULL);
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

SpewRetval_t SDKLauncherSpewOutputFunc( SpewType_t spewType, char const *pMsg )
{
#ifdef _WIN32
	OutputDebugString( pMsg );
#endif

	if (spewType == SPEW_ERROR)
	{
		// In Windows vgui mode, make a message box or they won't ever see the error.
#ifdef _WIN32
		MessageBox( NULL, pMsg, "Error", MB_OK | MB_TASKMODAL );
		TerminateProcess( GetCurrentProcess(), 1 );
#elif _LINUX
		_exit(1);
#else
#error "Implement me"
#endif
		
		return SPEW_ABORT;
	}
	if (spewType == SPEW_ASSERT)
	{
		if ( CommandLine()->FindParm( "-noassert" ) == 0 )
			return SPEW_DEBUGGER;
		else
			return SPEW_CONTINUE;
	}
	return SPEW_CONTINUE;
}


const char* GetSDKLauncherBinDirectory()
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


const char* GetSDKToolsBinDirectory( )
{
	static char path[MAX_PATH] = {0};
	if ( path[0] == 0 )
	{
		GetModuleFileName( (HMODULE)GetAppInstance(), path, sizeof( path ) );
		Q_StripLastDir( path, sizeof( path ) );	// Get rid of the filename.
		V_strncat( path, g_engineDir, sizeof( path ) );
		V_strncat( path, "\\bin", sizeof( path ) );
	}
	return path;
}


const char* GetSDKLauncherBaseDirectory()
{
	static char basedir[512] = {0};
	if ( basedir[0] == 0 )
	{
		Q_strncpy( basedir, GetSDKLauncherBinDirectory(), sizeof( basedir ) );
		Q_StripLastDir( basedir, sizeof( basedir ) );	// Get rid of the bin directory.
		Q_StripTrailingSlash( basedir );
	}
	return basedir;
}


void SubstituteBaseDir( const char *pIn, char *pOut, int outLen )
{
	Q_StrSubst( pIn, "%basedir%", GetSDKLauncherBaseDirectory(), pOut, outLen );
}


CUtlVector<char> g_FileData;
CUtlVector<char> g_ReplacementData[2];

CUtlVector<char>* GetFileStringWithReplacements( 
	const char *pInputFilename, 
	const char **ppReplacements, int nReplacements,
	int &dataWriteLen )
{
	Assert( nReplacements % 2 == 0 );
 
	// Read in the file data.
	FileHandle_t hFile = g_pFullFileSystem->Open( pInputFilename, "rb" );
	if ( !hFile )
	{
		return false;
	}
	g_FileData.SetSize( g_pFullFileSystem->Size( hFile ) );
	g_pFullFileSystem->Read( g_FileData.Base(), g_FileData.Count(), hFile );
	g_pFullFileSystem->Close( hFile );
	
	CUtlVector<char> *pCurData = &g_FileData;
	dataWriteLen = g_FileData.Count();
	if ( nReplacements )
	{
		// Null-terminate it.
		g_FileData.AddToTail( 0 );

		// Apply all the string substitutions.
		int iCurCount = g_FileData.Count() * 2;
		g_ReplacementData[0].EnsureCount( iCurCount );
		g_ReplacementData[1].EnsureCount( iCurCount );
		for ( int i=0; i < nReplacements/2; i++ )
		{
			for ( int iTestCount=0; iTestCount < 64; iTestCount++ )
			{
				if ( Q_StrSubst( pCurData->Base(), ppReplacements[i*2], ppReplacements[i*2+1], g_ReplacementData[i&1].Base(), g_ReplacementData[i&1].Count() ) )
					break;

				// Ok, we would overflow the string.. add more space to do the string substitution into.
				iCurCount += 2048;
				g_ReplacementData[0].EnsureCount( iCurCount );
				g_ReplacementData[1].EnsureCount( iCurCount );
			}
			pCurData = &g_ReplacementData[i&1];
			dataWriteLen = strlen( pCurData->Base() );
		}
	}
	
	return pCurData;
}


bool CopyWithReplacements( 
	const char *pInputFilename, 
	const char **ppReplacements, int nReplacements,
	const char *pOutputFilenameFormat, ... )
{
	int dataWriteLen;
	CUtlVector<char> *pCurData = GetFileStringWithReplacements( pInputFilename, ppReplacements, nReplacements, dataWriteLen );
	if ( !pCurData )
	{
		char msg[512];
		Q_snprintf( msg, sizeof( msg ), "Can't open %s for reading.", pInputFilename );
		::MessageBox( NULL, msg, "Error", MB_OK );
		return false;
	} 

	// Get the output filename.
	char outFilename[MAX_PATH];
	va_list marker;
	va_start( marker, pOutputFilenameFormat );
	Q_vsnprintf( outFilename, sizeof( outFilename ), pOutputFilenameFormat, marker );
	va_end( marker );

	// Write it out. I'd like to use IFileSystem, but Steam lowercases all filenames, which screws case-sensitive linux
	// (since the linux makefiles are tuned to the casing in Perforce).
	FILE *hFile = fopen( outFilename, "wb" );
	if ( !hFile )
	{
		char msg[512];
		Q_snprintf( msg, sizeof( msg ), "Can't open %s for writing.", outFilename );
		::MessageBox( NULL, msg, "Error", MB_OK );
		return false;
	}

	fwrite( pCurData->Base(), 1, dataWriteLen, hFile );
	fclose( hFile );
	return true;
}

int InitializeVGui()
{
	vgui::ivgui()->SetSleep(false);

	// find our configuration directory
	char szConfigDir[512];
	const char *steamPath = getenv("SteamInstallPath");
	if (steamPath)
	{
		// put the config dir directly under steam
		Q_snprintf(szConfigDir, sizeof(szConfigDir), "%s/config", steamPath);
	}
	else
	{
		// we're not running steam, so just put the config dir under the platform
		Q_strncpy( szConfigDir, "platform/config", sizeof(szConfigDir));
	}
	g_pFullFileSystem->CreateDirHierarchy("config", "PLATFORM");
	g_pFullFileSystem->AddSearchPath(szConfigDir, "CONFIG", PATH_ADD_TO_HEAD);

	// initialize the user configuration file
	vgui::system()->SetUserConfigFile("DedicatedServerDialogConfig.vdf", "CONFIG");

	// Init the surface
	vgui::Panel *pPanel = new vgui::Panel(NULL, "TopPanel");
	pPanel->SetVisible(true);

	vgui::surface()->SetEmbeddedPanel(pPanel->GetVPanel());

	// load the scheme
	vgui::scheme()->LoadSchemeFromFile("Resource/sdklauncher_scheme.res", NULL);
	
	// localization
	g_pVGuiLocalize->AddFile( "resource/platform_english.txt" );
	g_pVGuiLocalize->AddFile( "vgui/resource/vgui_english.txt" );
	g_pVGuiLocalize->AddFile( "sdklauncher_english.txt" );

	// Start vgui
	vgui::ivgui()->Start();

	// add our main window
	g_pMainFrame = new CSDKLauncherDialog(pPanel, "SDKLauncherDialog");

	// show main window
	g_pMainFrame->MoveToCenterOfScreen();
	g_pMainFrame->Activate();

	return 0;
}


void ShutdownVGui()
{
	delete g_pMainFrame;
}


KeyValues* LoadGameDirsFile()
{
	char filename[MAX_PATH];
	Q_snprintf( filename, sizeof( filename ), "%ssdklauncher_gamedirs.txt", gamedir );

	KeyValues *dataFile = new KeyValues("gamedirs");
	dataFile->UsesEscapeSequences( true );
	dataFile->LoadFromFile( g_pFullFileSystem, filename, NULL );
	return dataFile;
}


bool SaveGameDirsFile( KeyValues *pFile )
{
	char filename[MAX_PATH];
	Q_snprintf( filename, sizeof( filename ), "%ssdklauncher_gamedirs.txt", gamedir );
	return pFile->SaveToFile( g_pFullFileSystem, filename );
}



class CModalPreserveMessageBox : public vguiMessageBox
{
public:
	CModalPreserveMessageBox(const char *title, const char *text, vgui::Panel *parent)
		: vguiMessageBox( title, text, parent )
	{
		m_PrevAppFocusPanel = vgui::input()->GetAppModalSurface();
	}

	~CModalPreserveMessageBox()
	{
		vgui::input()->SetAppModalSurface( m_PrevAppFocusPanel );
	}


public:
	vgui::VPANEL m_PrevAppFocusPanel;
};
		


void VGUIMessageBox( vgui::Panel *pParent, const char *pTitle, const char *pMsg, ... )
{
	char msg[4096];
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( msg, sizeof( msg ), pMsg, marker );
	va_end( marker );

	vguiMessageBox *dlg = new CModalPreserveMessageBox( pTitle, msg, pParent );
	dlg->DoModal();
	dlg->RequestFocus();
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
		Q_strncpy( szConfigDir, GetSDKLauncherBinDirectory(), sizeof( szConfigDir ) );
		Q_strncat ( szConfigDir, "\\", MAX_PATH );
		Q_strncat ( szConfigDir, g_engineDir, MAX_PATH );
		Q_strncat ( szConfigDir, "\\bin", MAX_PATH );

		g_dwChangeHandle = FindFirstChangeNotification( 
			szConfigDir,													// directory to watch 
			false,															// watch the subtree 
			(FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_ATTRIBUTES));	// watch file and dir name changes 
 
		if ( g_dwChangeHandle == INVALID_HANDLE_VALUE )
		{
			// FIXME: Unable to watch the file
		}
	}
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
			g_pMainFrame->RefreshConfigs();
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

void QuickLaunchCommandLine( char *pCommandLine )
{
	STARTUPINFO si;
	memset( &si, 0, sizeof( si ) );
	si.cb = sizeof( si );

	PROCESS_INFORMATION pi;
	memset( &pi, 0, sizeof( pi ) );

	DWORD dwFlags = 0;
	
	if ( !CreateProcess( 
		0,
		pCommandLine, 
		NULL,							// security
		NULL,
		TRUE,
		dwFlags,						// flags
		NULL,							// environment
		GetSDKLauncherBaseDirectory(),	// current directory
		&si,
		&pi ) )
	{
		::MessageBoxA( NULL, GetLastWindowsErrorString(), "Error", MB_OK | MB_ICONINFORMATION | MB_APPLMODAL );
	}
}

bool RunQuickLaunch()
{
	char cmdLine[512];

	if ( CommandLine()->FindParm( "-runhammer" ) )
	{
		Q_snprintf( cmdLine, sizeof( cmdLine ), "\"%s\\%s\\bin\\hammer.exe\"", GetSDKLauncherBinDirectory(), g_engineDir );
		QuickLaunchCommandLine( cmdLine );
		return true;
	}
	else if ( CommandLine()->FindParm( "-runmodelviewer" ) )
	{
		Q_snprintf( cmdLine, sizeof( cmdLine ), "\"%s\\%s\\bin\\hlmv.exe\"", GetSDKLauncherBinDirectory(), g_engineDir );
		QuickLaunchCommandLine( cmdLine );
		return true;
	}
	else if ( CommandLine()->FindParm( "-runfaceposer" ) )
	{
		Q_snprintf( cmdLine, sizeof( cmdLine ), "\"%s\\%s\\bin\\hlfaceposer.exe\"", GetSDKLauncherBinDirectory(), g_engineDir );
		QuickLaunchCommandLine( cmdLine );
		return true;
	}
	
	return false;
}


void CheckCreateModParameters()
{
	if ( CommandLine()->FindParm( "-AutoHL2Mod" ) )
		g_bAutoHL2Mod = true;

	int iParm = CommandLine()->FindParm( "-CreateMod" );
	if ( iParm == 0 )
		return;

	if ( (iParm + 2) < CommandLine()->ParmCount() )
	{
		// Set it up so the mod wizard can skip the mod dir/mod name panel.
		g_bModWizard_CmdLineFields = true;
		Q_strncpy( g_ModWizard_CmdLine_ModDir, CommandLine()->GetParm( iParm + 1 ), sizeof( g_ModWizard_CmdLine_ModDir ) );
		Q_strncpy( g_ModWizard_CmdLine_ModName, CommandLine()->GetParm( iParm + 2 ), sizeof( g_ModWizard_CmdLine_ModName ) );

		RunCreateModWizard( true );
	}
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CSDKLauncherApp : public CVguiSteamApp
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

DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CSDKLauncherApp );


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CSDKLauncherApp::Create()
{
	SpewOutputFunc( SDKLauncherSpewOutputFunc );

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
bool CSDKLauncherApp::PreInit()
{
	if ( !BaseClass::PreInit() )
		return false;

	// Make sure we're using the proper environment variable
	ConvertObsoleteVConfigRegistrySetting( GAMEDIR_TOKEN );

	if ( !CommandLine()->ParmValue( "-game" ) )
	{
		Error( "SDKLauncher requires -game on the command line." );
		return false;
	}

	// winsock aware
	WSAData wsaData;
	WSAStartup( MAKEWORD(2,0), &wsaData );

	// Create a window to capture messages
	CreateMessageWindow();

	FileSystem_SetErrorMode( FS_ERRORMODE_AUTO );

	if ( !BaseClass::SetupSearchPaths( NULL, false, true ) )
	{
		::MessageBox( NULL, "Error", "Unable to initialize file system\n", MB_OK );
		return false;
	}

	// Set gamedir.
	Q_MakeAbsolutePath( gamedir, sizeof( gamedir ), GetGameInfoPath() );
	Q_AppendSlash( gamedir, sizeof( gamedir ) );

	// the "base dir" so we can scan mod name
	g_pFullFileSystem->AddSearchPath(GetSDKLauncherBaseDirectory(), SDKLAUNCHER_MAIN_PATH_ID);	
	// the main platform dir
	g_pFullFileSystem->AddSearchPath("platform","PLATFORM", PATH_ADD_TO_HEAD);

	return true;
}

void CSDKLauncherApp::PostShutdown()
{
	// Stop our message window
	ShutdownMessageWindow();
	::WSACleanup();

	BaseClass::PostShutdown();
}


//-----------------------------------------------------------------------------
// Purpose: Entry point
//-----------------------------------------------------------------------------
int CSDKLauncherApp::Main()
{
	SetVConfigRegistrySetting( "sourcesdk", GetSDKLauncherBaseDirectory() );

	// If they just want to run Hammer or hlmv, just do that and exit.
	if ( RunQuickLaunch() )
		return 1;
	
	// Run app frame loop
	int ret = InitializeVGui();
	if ( ret != 0 )
		return ret;

	DumpMinFootprintFiles( false );

	SteamAPI_InitSafe();
	SteamAPI_SetTryCatchCallbacks( false ); // We don't use exceptions, so tell steam not to use try/catch in callback handlers
	g_SteamAPIContext.Init();
	
	// Start looking for file updates
//	UpdateConfigsStatus_Init();

	// Check if they want to run the Create Mod wizard right off the bat.
	CheckCreateModParameters();

	while ( vgui::ivgui()->IsRunning() && !g_bAppQuit )
	{
		Sleep( 10 );
//		UpdateConfigsStatus();
		vgui::ivgui()->RunFrame();
	}
	
	ShutdownVGui();

//	UpdateConfigsStatus_Shutdown();

	return 1;
}

