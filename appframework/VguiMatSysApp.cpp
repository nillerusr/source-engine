//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
//=============================================================================
#ifdef _WIN32

#if defined( _WIN32 ) && !defined( _X360 )
#include <windows.h>
#endif
#include "appframework/vguimatsysapp.h"
#include "filesystem.h"
#include "materialsystem/imaterialsystem.h"
#include "vgui/ivgui.h"
#include "vgui/ISurface.h"
#include "vgui_controls/controls.h"
#include "vgui/ischeme.h"
#include "vgui/ilocalize.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "materialsystem/materialsystem_config.h"
#include "filesystem_init.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "inputsystem/iinputsystem.h"
#include "tier3/tier3.h"


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CVguiMatSysApp::CVguiMatSysApp()
{
}


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CVguiMatSysApp::Create()
{
	AppSystemInfo_t appSystems[] = 
	{
		{ "inputsystem.dll",		INPUTSYSTEM_INTERFACE_VERSION },
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },

		// NOTE: This has to occur before vgui2.dll so it replaces vgui2's surface implementation
		{ "vguimatsurface.dll",		VGUI_SURFACE_INTERFACE_VERSION },
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },

		// Required to terminate the list
		{ "", "" }
	};

	if ( !AddSystems( appSystems ) ) 
		return false;

	IMaterialSystem *pMaterialSystem = (IMaterialSystem*)FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );

	if ( !pMaterialSystem )
	{
		Warning( "CVguiMatSysApp::Create: Unable to connect to necessary interface!\n" );
		return false;
	}

	pMaterialSystem->SetShaderAPI( "shaderapidx9.dll" );
	return true;
}

void CVguiMatSysApp::Destroy()
{
}



//-----------------------------------------------------------------------------
// Window management
//-----------------------------------------------------------------------------
void*CVguiMatSysApp::CreateAppWindow( char const *pTitle, bool bWindowed, int w, int h )
{
	WNDCLASSEX		wc;
	memset( &wc, 0, sizeof( wc ) );
	wc.cbSize		 = sizeof( wc );
    wc.style         = CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc   = DefWindowProc;
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
	void *hWnd = CreateWindow( wc.lpszClassName, pTitle, style, 0, 0, 
		windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, 
		NULL, NULL, (HINSTANCE)GetAppInstance(), NULL );

	if (!hWnd)
		return NULL;

    int CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;

	// In VCR modes, keep it in the upper left so mouse coordinates are always relative to the window.
	SetWindowPos( (HWND)hWnd, NULL, CenterX, CenterY, 0, 0,
				  SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);

	return hWnd;
}


//-----------------------------------------------------------------------------
// Pump messages
//-----------------------------------------------------------------------------
void CVguiMatSysApp::AppPumpMessages()
{
	g_pInputSystem->PollInputState();
}


//-----------------------------------------------------------------------------
// Sets up the game path
//-----------------------------------------------------------------------------
bool CVguiMatSysApp::SetupSearchPaths( const char *pStartingDir, bool bOnlyUseStartingDir, bool bIsTool )
{
	if ( !BaseClass::SetupSearchPaths( pStartingDir, bOnlyUseStartingDir, bIsTool ) )
		return false;

	g_pFullFileSystem->AddSearchPath( GetGameInfoPath(), "SKIN", PATH_ADD_TO_HEAD );
	return true;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CVguiMatSysApp::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	if ( !g_pFullFileSystem || !g_pMaterialSystem || !g_pInputSystem || !g_pMatSystemSurface )
	{
		Warning( "CVguiMatSysApp::PreInit: Unable to connect to necessary interface!\n" );
		return false;
	}

	// Add paths...
	if ( !SetupSearchPaths( NULL, false, true ) )
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

	m_nWidth = iWidth;
	m_nHeight = iHeight;
	m_HWnd = CreateAppWindow( GetAppName(), bWindowed, iWidth, iHeight );
	if ( !m_HWnd )
		return false;

	g_pInputSystem->AttachToWindow( m_HWnd );
	g_pMatSystemSurface->AttachToWindow( m_HWnd );

	// NOTE: If we specifically wanted to use a particular shader DLL, we set it here...
	//m_pMaterialSystem->SetShaderAPI( "shaderapidx8" );

	// Get the adapter from the command line....
	const char *pAdapterString;
	int adapter = 0;
	if (CommandLine()->CheckParm( "-adapter", &pAdapterString ))
	{
		adapter = atoi( pAdapterString );
	}

	int adapterFlags = 0;
	if ( CommandLine()->CheckParm( "-ref" ) )
	{
		adapterFlags |= MATERIAL_INIT_REFERENCE_RASTERIZER;
	}
	if ( AppUsesReadPixels() )
	{
		adapterFlags |= MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE;
	}

	g_pMaterialSystem->SetAdapter( adapter, adapterFlags );

	return true; 
}

void CVguiMatSysApp::PostShutdown()
{
	if ( g_pMatSystemSurface && g_pInputSystem )
	{
		g_pMatSystemSurface->AttachToWindow( NULL );
		g_pInputSystem->DetachFromWindow( );
	}

	BaseClass::PostShutdown();
}


//-----------------------------------------------------------------------------
// Gets the window size
//-----------------------------------------------------------------------------
int CVguiMatSysApp::GetWindowWidth() const
{
	return m_nWidth;
}

int CVguiMatSysApp::GetWindowHeight() const
{
	return m_nHeight;
}


//-----------------------------------------------------------------------------
// Returns the window
//-----------------------------------------------------------------------------
void* CVguiMatSysApp::GetAppWindow()
{
	return m_HWnd;
}

	
//-----------------------------------------------------------------------------
// Sets the video mode
//-----------------------------------------------------------------------------
bool CVguiMatSysApp::SetVideoMode( )
{
	MaterialSystem_Config_t config;
	if ( CommandLine()->CheckParm( "-fullscreen" ) )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, false );
	}
	else
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, true );
	}

	if ( CommandLine()->CheckParm( "-resizing" ) )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_RESIZING, true );
	}

	if ( CommandLine()->CheckParm( "-mat_vsync" ) )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_NO_WAIT_FOR_VSYNC, false );
	}

	config.m_nAASamples = CommandLine()->ParmValue( "-mat_antialias", 1 );
	config.m_nAAQuality = CommandLine()->ParmValue( "-mat_aaquality", 0 );

	config.m_VideoMode.m_Width = config.m_VideoMode.m_Height = 0;
	config.m_VideoMode.m_Format = IMAGE_FORMAT_BGRX8888;
	config.m_VideoMode.m_RefreshRate = 0;
	config.SetFlag(	MATSYS_VIDCFG_FLAGS_STENCIL, true );

	bool modeSet = g_pMaterialSystem->SetMode( m_HWnd, config );
	if (!modeSet)
	{
		Error( "Unable to set mode\n" );
		return false;
	}

	g_pMaterialSystem->OverrideConfig( config, false );
	return true;
}

#endif // _WIN32

