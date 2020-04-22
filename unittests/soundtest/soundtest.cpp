//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Sound unit test application
//
//=============================================================================

#include <windows.h>
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "filesystem.h"
#include "datacache/idatacache.h"
#include "appframework/appframework.h"
#include "soundsystem/isoundsystem.h"
#include "vstdlib/cvar.h"
#include "filesystem_init.h"


//-----------------------------------------------------------------------------
// Main system interfaces
//-----------------------------------------------------------------------------
IFileSystem *g_pFileSystem;
ISoundSystem *g_pSoundSystem;


//-----------------------------------------------------------------------------
// Standard spew functions
//-----------------------------------------------------------------------------
static SpewRetval_t SoundTestOutputFunc( SpewType_t spewType, char const *pMsg )
{
	printf( pMsg );
	fflush( stdout );

	if (spewType == SPEW_ERROR)
		return SPEW_ABORT;
	return (spewType == SPEW_ASSERT) ? SPEW_DEBUGGER : SPEW_CONTINUE; 
}




//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CSoundTestApp : public CDefaultAppSystemGroup<CSteamAppSystemGroup>
{
public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual int Main();
	virtual void PostShutdown();
	virtual void Destroy();

private:
	bool CreateAppWindow( char const *pTitle, bool bWindowed, int w, int h );
	bool SetupSearchPaths();
	void AppPumpMessages();

	// Windproc
	static LONG WINAPI WinAppWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LONG WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	HWND m_hWnd;
	bool m_bExitMainLoop;
};

static CSoundTestApp s_SoundTestApp;
DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT_GLOBALVAR( CSoundTestApp, s_SoundTestApp );


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
bool CSoundTestApp::Create()
{
	SpewOutputFunc( SoundTestOutputFunc );

	// Add in the cvar factory
	AppModule_t cvarModule = LoadModule( VStdLib_GetICVarFactory() );
	AddSystem( cvarModule, CVAR_INTERFACE_VERSION );

	AppSystemInfo_t appSystems[] = 
	{
		{ "datacache.dll",			DATACACHE_INTERFACE_VERSION },
		{ "soundsystem.dll",		SOUNDSYSTEM_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	if ( !AddSystems( appSystems ) ) 
		return false;

	g_pFileSystem = (IFileSystem*)FindSystem( FILESYSTEM_INTERFACE_VERSION );
	g_pSoundSystem = (ISoundSystem*)FindSystem( SOUNDSYSTEM_INTERFACE_VERSION );

	return ( g_pFileSystem && g_pSoundSystem );
}

void CSoundTestApp::Destroy()
{
	g_pFileSystem = NULL;
	g_pSoundSystem = NULL;
}


//-----------------------------------------------------------------------------
// Window management
//-----------------------------------------------------------------------------
bool CSoundTestApp::CreateAppWindow( char const *pTitle, bool bWindowed, int w, int h )
{
	WNDCLASSEX		wc;
	memset( &wc, 0, sizeof( wc ) );
	wc.cbSize		 = sizeof( wc );
    wc.style         = CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc   = WinAppWindowProc;
    wc.hInstance     = (HINSTANCE)GetAppInstance();
    wc.lpszClassName = "Valve001";
	wc.hIcon		 = NULL; //LoadIcon( s_HInstance, MAKEINTRESOURCE( IDI_LAUNCHER ) );
	wc.hIconSm		 = wc.hIcon;

    RegisterClassEx( &wc );

	// Note, it's hidden
	DWORD style = WS_POPUP | WS_CLIPSIBLINGS;
	
	if ( bWindowed )
	{
		// Give it a frame
		style |= WS_OVERLAPPEDWINDOW;
		style &= ~WS_THICKFRAME;
	}

	// Never a max box
	style &= ~WS_MAXIMIZEBOX;

	RECT windowRect;
	windowRect.top		= 0;
	windowRect.left		= 0;
	windowRect.right	= w;
	windowRect.bottom	= h;

	// Compute rect needed for that size client area based on window style
	AdjustWindowRectEx(&windowRect, style, FALSE, 0);

	// Create the window
	m_hWnd = CreateWindow( wc.lpszClassName, pTitle, style, 0, 0, 
		windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, 
		NULL, NULL, (HINSTANCE)GetAppInstance(), NULL );

	if ( m_hWnd == INVALID_HANDLE_VALUE )
		return false;

    int     CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;

	// In VCR modes, keep it in the upper left so mouse coordinates are always relative to the window.
	SetWindowPos (m_hWnd, NULL, CenterX, CenterY, 0, 0,
				  SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);

	return true;
}


//-----------------------------------------------------------------------------
// Message handler
//-----------------------------------------------------------------------------
LONG CSoundTestApp::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if ( uMsg == WM_CLOSE )
	{
		m_bExitMainLoop = true;
	}
	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

LONG WINAPI CSoundTestApp::WinAppWindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return s_SoundTestApp.WindowProc( hWnd, uMsg, wParam, lParam );
}

	
//-----------------------------------------------------------------------------
// Sets up the game path
//-----------------------------------------------------------------------------
bool CSoundTestApp::SetupSearchPaths()
{
	CFSSteamSetupInfo steamInfo;
	steamInfo.m_pDirectoryName = NULL;
	steamInfo.m_bOnlyUseDirectoryName = false;
	steamInfo.m_bToolsMode = true;
	steamInfo.m_bSetSteamDLLPath = true;
	steamInfo.m_bSteam = g_pFileSystem->IsSteam();
	if ( FileSystem_SetupSteamEnvironment( steamInfo ) != FS_OK )
		return false;

	CFSMountContentInfo fsInfo;
	fsInfo.m_pFileSystem = g_pFileSystem;
	fsInfo.m_bToolsMode = true;
	fsInfo.m_pDirectoryName = steamInfo.m_GameInfoPath;

	if ( FileSystem_MountContent( fsInfo ) != FS_OK )
		return false;

	// Finally, load the search paths for the "GAME" path.
	CFSSearchPathsInit searchPathsInit;
	searchPathsInit.m_pDirectoryName = steamInfo.m_GameInfoPath;
	searchPathsInit.m_pFileSystem = g_pFileSystem;
	if ( FileSystem_LoadSearchPaths( searchPathsInit ) != FS_OK )
		return false;

	g_pFileSystem->AddSearchPath( steamInfo.m_GameInfoPath, "SKIN", PATH_ADD_TO_HEAD );

	FileSystem_AddSearchPath_Platform( g_pFileSystem, steamInfo.m_GameInfoPath );

	// and now add episodic to the GAME searchpath
	char shorts[MAX_PATH];
	Q_strncpy( shorts, steamInfo.m_GameInfoPath, MAX_PATH );
	Q_StripTrailingSlash( shorts );
	Q_strncat( shorts, "/../episodic", MAX_PATH, MAX_PATH );

	g_pFileSystem->AddSearchPath( shorts, "GAME", PATH_ADD_TO_HEAD );

	return true;
}


//-----------------------------------------------------------------------------
// PreInit, PostShutdown
//-----------------------------------------------------------------------------
bool CSoundTestApp::PreInit( )
{
	// Add paths...
	if ( !SetupSearchPaths() )
		return false;

	const char *pArg;
	int iWidth = 1024;
	int iHeight = 768;
	bool bWindowed = (CommandLine()->CheckParm( "-fullscreen" ) == NULL);
	if (CommandLine()->CheckParm( "-width", &pArg ))
	{
		iWidth = atoi( pArg );
	}
	if (CommandLine()->CheckParm( "-height", &pArg ))
	{
		iHeight = atoi( pArg );
	}

	if ( !CreateAppWindow( "SoundTest", bWindowed, iWidth, iHeight ) )
		return false;

	return true; 
}

void CSoundTestApp::PostShutdown()
{
}


//-----------------------------------------------------------------------------
// Pump messages
//-----------------------------------------------------------------------------
void CSoundTestApp::AppPumpMessages()
{
	MSG msg;
	while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
	{
		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
int CSoundTestApp::Main()
{
	CAudioSource *pSource = g_pSoundSystem->LoadSound( "sound/ambient/alarms/alarm1.wav" );

	CAudioMixer *pMixer;
	g_pSoundSystem->PlaySound( pSource, 1.0f, &pMixer );

	m_bExitMainLoop = false;
	while ( !m_bExitMainLoop )
	{
		AppPumpMessages();
		g_pSoundSystem->Update( Plat_FloatTime() );
	}

	return 1;
}



