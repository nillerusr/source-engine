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
// Material editor
//=============================================================================

#include <windows.h>
#include "appframework/tier2app.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "vstdlib/random.h"
#include "filesystem.h"
#include "filesystem_init.h"
#include "tier0/icommandline.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "materialsystem/imesh.h"


//-----------------------------------------------------------------------------
// Purpose: Warning/Msg call back through this API
// Input  : type - 
//			*pMsg - 
// Output : SpewRetval_t
//-----------------------------------------------------------------------------
SpewRetval_t SpewFunc( SpewType_t type, const char *pMsg )
{
	if ( Plat_IsInDebugSession() )
	{
		OutputDebugString( pMsg );
		if ( type == SPEW_ASSERT )
			return SPEW_DEBUGGER;
	}
	return SPEW_CONTINUE;
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CMaterialSystemTestApp : public CTier2SteamApp
{
	typedef CTier2SteamApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void PostShutdown( );
	virtual void Destroy();
	virtual const char *GetAppName() { return "MaterialSystemTest"; }
	virtual bool AppUsesReadPixels() { return false; }

private:
	// Window management
	bool CreateAppWindow( const char *pTitle, bool bWindowed, int w, int h );

	// Sets up the game path
	bool SetupSearchPaths();

	// Waits for a keypress
	bool WaitForKeypress();

	// Sets the video mode
	bool SetMode();

	// Tests dynamic buffers
	void TestDynamicBuffers( IMatRenderContext *pRenderContext, bool bBuffered );

	// Creates, destroys a test material
	void CreateWireframeMaterial();
	void DestroyMaterial();

	CMaterialReference m_pMaterial;

	HWND m_HWnd;
};

DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CMaterialSystemTestApp );


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CMaterialSystemTestApp::Create()
{
	SpewOutputFunc( SpewFunc );

	AppSystemInfo_t appSystems[] = 
	{
		{ "materialsystem.dll",		MATERIAL_SYSTEM_INTERFACE_VERSION },

		// Required to terminate the list
		{ "", "" }
	};

	if ( !AddSystems( appSystems ) ) 
		return false;

	IMaterialSystem *pMaterialSystem = (IMaterialSystem*)FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );
	if ( !pMaterialSystem )
	{
		Warning( "CMaterialSystemTestApp::Create: Unable to connect to necessary interface!\n" );
		return false;
	}

	bool bIsVistaOrHigher = false;

	OSVERSIONINFO info;
	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if ( GetVersionEx( &info ) )
	{
		bIsVistaOrHigher = info.dwMajorVersion >= 6;
	}

	const char *pShaderDLL = CommandLine()->ParmValue( "-shaderdll" );
	if ( !pShaderDLL )
	{
		pShaderDLL = "shaderapidx10.dll";
	}

	if ( !bIsVistaOrHigher && !Q_stricmp( pShaderDLL, "shaderapidx10.dll" ) )
	{
		pShaderDLL = "shaderapidx9.dll";
	}

	pMaterialSystem->SetShaderAPI( pShaderDLL );	
	return true;
}

void CMaterialSystemTestApp::Destroy()
{
}


//-----------------------------------------------------------------------------
// Window callback
//-----------------------------------------------------------------------------
static LRESULT CALLBACK MaterialSystemTestWndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch( message )
	{
	case WM_DESTROY:
		PostQuitMessage( 0 );
		break;

	default:
		return DefWindowProc( hWnd, message, wParam, lParam );
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Window management
//-----------------------------------------------------------------------------
bool CMaterialSystemTestApp::CreateAppWindow( const char *pTitle, bool bWindowed, int w, int h )
{
	WNDCLASSEX		wc;
	memset( &wc, 0, sizeof( wc ) );
	wc.cbSize		 = sizeof( wc );
    wc.style         = CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc   = MaterialSystemTestWndProc;
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
	m_HWnd = CreateWindow( wc.lpszClassName, pTitle, style, 0, 0, 
		windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, 
		NULL, NULL, (HINSTANCE)GetAppInstance(), NULL );

	if (!m_HWnd)
		return false;

    int     CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;

	// In VCR modes, keep it in the upper left so mouse coordinates are always relative to the window.
	SetWindowPos (m_HWnd, NULL, CenterX, CenterY, 0, 0,
				  SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);

	return true;
}


//-----------------------------------------------------------------------------
// Sets up the game path
//-----------------------------------------------------------------------------
bool CMaterialSystemTestApp::SetupSearchPaths()
{
	if ( !BaseClass::SetupSearchPaths( NULL, false, true ) )
		return false;

	g_pFullFileSystem->AddSearchPath( GetGameInfoPath(), "SKIN", PATH_ADD_TO_HEAD );
	return true;
}


//-----------------------------------------------------------------------------
// PreInit, PostShutdown
//-----------------------------------------------------------------------------
bool CMaterialSystemTestApp::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	if ( !g_pFullFileSystem || !g_pMaterialSystem )
		return false;

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

	if (!CreateAppWindow( "Press a Key To Continue", bWindowed, iWidth, iHeight ))
		return false;

	// Get the adapter from the command line....
	const char *pAdapterString;
	int nAdapter = 0;
	if ( CommandLine()->CheckParm( "-adapter", &pAdapterString ) )
	{
		nAdapter = atoi( pAdapterString );
	}

	int nAdapterFlags = 0;
	if ( AppUsesReadPixels() )
	{
		nAdapterFlags |= MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE;
	}

	g_pMaterialSystem->SetAdapter( nAdapter, nAdapterFlags );

	return true;
}

void CMaterialSystemTestApp::PostShutdown( )
{
	BaseClass::PostShutdown();
}


//-----------------------------------------------------------------------------
// Waits for a keypress
//-----------------------------------------------------------------------------
bool CMaterialSystemTestApp::WaitForKeypress()
{
	MSG msg = {0};
	while( WM_QUIT != msg.message )
	{
		if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}

		if ( msg.message == WM_KEYDOWN )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Sets the video mode
//-----------------------------------------------------------------------------
bool CMaterialSystemTestApp::SetMode()
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

	bool modeSet = g_pMaterialSystem->SetMode( m_HWnd, config );
	if (!modeSet)
	{
		Error( "Unable to set mode\n" );
		return false;
	}

	g_pMaterialSystem->OverrideConfig( config, false );
	return true;
}


//-----------------------------------------------------------------------------
// Creates, destroys a test material
//-----------------------------------------------------------------------------
void CMaterialSystemTestApp::CreateWireframeMaterial()
{
	KeyValues *pVMTKeyValues = new KeyValues( "Wireframe" );
	pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	m_pMaterial.Init( "__test", pVMTKeyValues );
}

void CMaterialSystemTestApp::DestroyMaterial()
{
	m_pMaterial.Shutdown();
}


//-----------------------------------------------------------------------------
// Tests dynamic buffers
//-----------------------------------------------------------------------------
void CMaterialSystemTestApp::TestDynamicBuffers( IMatRenderContext *pMatRenderContext, bool bBuffered )
{
	CreateWireframeMaterial();

	g_pMaterialSystem->BeginFrame( 0 );

	pMatRenderContext->Bind( m_pMaterial );
	IMesh *pMesh = pMatRenderContext->GetDynamicMesh( bBuffered );

	// clear (so that we can make sure that we aren't getting results from the previous quad)
	pMatRenderContext->ClearColor3ub( RandomInt( 0, 100 ), RandomInt( 0, 100 ), RandomInt( 190, 255 ) );
	pMatRenderContext->ClearBuffers( true, true );

	static unsigned char s_pColors[4][4] = 
	{
		{ 255,   0,   0, 255 },
		{   0, 255,   0, 255 },
		{   0,   0, 255, 255 },
		{ 255, 255, 255, 255 },
	};

	static int nCount = 0;

	const int nLoopCount = 8;
	float flWidth = 2.0f / nLoopCount;
	for ( int i = 0; i < nLoopCount; ++i )
	{
		CMeshBuilder mb;
		mb.Begin( pMesh, MATERIAL_TRIANGLES, 4, 6 );

		mb.Position3f( -1.0f + i * flWidth, -1.0f, 0.5f );
		mb.Normal3f( 0.0f, 0.0f, 1.0f );
		mb.Color4ubv( s_pColors[nCount++ % 4] );
		mb.AdvanceVertex();

		mb.Position3f( -1.0f + i * flWidth + flWidth, -1.0f, 0.5f );
		mb.Normal3f( 0.0f, 0.0f, 1.0f );
		mb.Color4ubv( s_pColors[nCount++ % 4] );
		mb.AdvanceVertex();

		mb.Position3f( -1.0f + i * flWidth + flWidth,  1.0f, 0.5f );
		mb.Normal3f( 0.0f, 0.0f, 1.0f );
		mb.Color4ubv( s_pColors[nCount++ % 4] );
		mb.AdvanceVertex();

		mb.Position3f( -1.0f + i * flWidth,  1.0f, 0.5f );
		mb.Normal3f( 0.0f, 0.0f, 1.0f );
		mb.Color4ubv( s_pColors[nCount++ % 4] );
		mb.AdvanceVertex();

		++nCount;

		mb.FastIndex( 0 );
		mb.FastIndex( 2 );
		mb.FastIndex( 1 );
		mb.FastIndex( 0 );
		mb.FastIndex( 3 );
		mb.FastIndex( 2 );

		mb.End( true );

		pMesh->Draw( );
	}

	++nCount;

	g_pMaterialSystem->EndFrame();
	g_pMaterialSystem->SwapBuffers();

	DestroyMaterial();
}


//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CMaterialSystemTestApp::Main()
{
	if ( !SetMode() )
		return 0;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	// Sets up a full-screen viewport
	int w, h;
	pRenderContext->GetWindowSize( w, h );
	pRenderContext->Viewport( 0, 0, w, h );
	pRenderContext->DepthRange( 0.0f, 1.0f );

	// Clears the screen
	g_pMaterialSystem->BeginFrame( 0 );
	pRenderContext->ClearColor4ub( 76, 88, 68, 255 ); 
	pRenderContext->ClearBuffers( true, true );
	g_pMaterialSystem->EndFrame();
	g_pMaterialSystem->SwapBuffers();

	SetWindowText( m_HWnd, "Buffer clearing . . hit a key" );
	if ( !WaitForKeypress() )
		return 1;

	SetWindowText( m_HWnd, "Dynamic buffer test.. hit a key" );
	TestDynamicBuffers( pRenderContext, false );
	if ( !WaitForKeypress() )
		return 1;

	SetWindowText( m_HWnd, "Buffered dynamic buffer test.. hit a key" );
	TestDynamicBuffers( pRenderContext, true );
	if ( !WaitForKeypress() )
		return 1;

	return 1;
}