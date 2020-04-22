//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// TerrainBlend.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "resource.h"
#include "mathlib/mathlib.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "tier1/strtools.h"


#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];								// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];								// The title bar text
HWND				g_hWnd;

// Foward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

int					g_nCapture = 0;
bool				g_bFocus = false;

int					g_ScreenWidth, g_ScreenHeight;

D3DPRESENT_PARAMETERS d3dpp;


IDirect3D8			*g_pDirect3D;
IDirect3DDevice8	*g_pDevice;


POINT GetWindowCenter()
{
	RECT rc;
	GetWindowRect(g_hWnd, &rc);
	POINT ret;
	ret.x = (rc.left + rc.right) / 2;
	ret.y = (rc.top + rc.bottom) / 2;
	return ret;
}


void CallAppRender( bool bInvalidRect )
{
	static DWORD lastTime = 0;
	static POINT lastMousePos = {0xFFFF, 0xFFFF};

	// Sample time and mouse position and tell the app to render.
	DWORD curTime = GetTickCount();
	float frametime = (curTime - lastTime) / 1000.0f;
	if( frametime > 0.1f )
		frametime = 0.1f;

	lastTime = curTime;

	// Get the cursor delta.
	POINT curMousePos;
	GetCursorPos(&curMousePos);

	int deltaX, deltaY;
	
	if( lastMousePos.x == 0xFFFF )
	{
		deltaX = deltaY = 0;
	}
	else
	{
		deltaX = curMousePos.x - lastMousePos.x;
		deltaY = curMousePos.y - lastMousePos.y;
	}

	// Recenter the cursor.
	if( g_nCapture )
	{
		lastMousePos = GetWindowCenter();
		SetCursorPos( lastMousePos.x, lastMousePos.y );
	}
	else
	{
		lastMousePos = curMousePos;
	}
	
	AppRender( frametime, (float)deltaX, (float)deltaY, bInvalidRect );
}


SpewRetval_t D3DAppSpewFunc( SpewType_t spewType, char const *pMsg )
{
	switch (spewType)
	{
	case SPEW_ERROR:
		MessageBox(NULL, pMsg, "FATAL ERROR", MB_OK);
		return SPEW_ABORT;

	default:
		OutputDebugString(pMsg);
#ifdef _DEBUG
		return spewType == SPEW_ASSERT ? SPEW_DEBUGGER : SPEW_CONTINUE;
#else
		return SPEW_CONTINUE;
#endif
	}
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	CommandLine()->CreateCmdLine( Plat_GetCommandLine() );

	// TODO: Place code here.
	SpewOutputFunc( D3DAppSpewFunc );
	MathLib_Init( true, true, true, 2.2f, 2.2f, 0.0f, 2.0f );
	MSG msg;
	HACCEL hAccelTable;
	
	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	strcpy( szWindowClass, "d3dapp" );
	MyRegisterClass(hInstance);
	
	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) 
	{
		return FALSE;
	}
	
	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_TERRAINBLEND);


	InvalidateRect( g_hWnd, NULL, FALSE );

	// Main message loop:
	while(1)
	{
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
		{
			if(msg.message == WM_QUIT)
				break;

			if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		if(msg.message == WM_QUIT)
			break;

		CallAppRender( false );
	}

	AppExit();
	
	return msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage is only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;
	
	wcex.cbSize = sizeof(WNDCLASSEX); 
	
	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_TERRAINBLEND);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);
	
	return RegisterClassEx(&wcex);
}


void ShutdownD3D()
{
	if( g_pDevice )
	{
		g_pDevice->Release();
		g_pDevice = NULL;
	}

	if( g_pDirect3D )
	{
		g_pDirect3D->Release();
		g_pDirect3D = NULL;
	}
}


void InitD3D()
{
	ShutdownD3D();

	
	RECT rcClient;
	GetClientRect( g_hWnd, &rcClient );

	g_ScreenWidth  = rcClient.right - rcClient.left;
	g_ScreenHeight = rcClient.bottom - rcClient.top;
	
	// Initialize D3D.
	g_pDirect3D = Direct3DCreate8( D3D_SDK_VERSION );

    // Get the current desktop display mode, so we can set up a back
    // buffer of the same format
    D3DDISPLAYMODE d3ddm;
    g_pDirect3D->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &d3ddm );


    // Set up the structure used to create the D3DDevice
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = d3ddm.Format;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
	d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;

	HRESULT hr = g_pDirect3D->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		g_hWnd,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING,
		&d3dpp,
		&g_pDevice);
	if( FAILED(hr) )
	{
		Sys_Error( "CreateDevice failed (%s)", DXGetErrorString8(hr) );
	}

    g_pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
	g_pDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
}

void DoResize()
{
	AppPreResize();
	InitD3D();
	AppPostResize();
	CallAppRender( true );
}

//
//   FUNCTION: InitInstance(HANDLE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	int x = Sys_FindArgInt( "-x", 0 );
	int y = Sys_FindArgInt( "-y", 0 );
	int w = Sys_FindArgInt( "-width", CW_USEDEFAULT );
	int h = Sys_FindArgInt( "-height", CW_USEDEFAULT );

	DWORD dwFlags = WS_OVERLAPPEDWINDOW;
	
	// Get the 'work area' so we don't overlap the taskbar on the top or left.
	RECT rcWorkArea;
	SystemParametersInfo( SPI_GETWORKAREA, 0, &rcWorkArea, 0 );

	// If they don't specify anything, maximize the window.
	if( x == 0 && y == 0 && w == CW_USEDEFAULT && h == CW_USEDEFAULT )
	{
		x = rcWorkArea.left;
		y = rcWorkArea.top;
		w = rcWorkArea.right - rcWorkArea.left;
		h = rcWorkArea.bottom - rcWorkArea.top;
		dwFlags |= WS_MAXIMIZE;
	}
	else
	{
		x += rcWorkArea.left;
		y += rcWorkArea.top;
	}

	
	g_hWnd = CreateWindow(
		szWindowClass, 
		szTitle, 
		dwFlags,				// window style
		x,						// x
		y,						// y
		w,						// width
		h,						// height
		NULL, 
		NULL, 
		hInstance, 
		NULL);
	
	if (!g_hWnd)
	{
		return FALSE;
	}
	
	ShowWindow(g_hWnd, nCmdShow);
	UpdateWindow(g_hWnd);


	InitD3D();
	AppInit();

	// Reinitialize D3D. For some reason, D3D point primitives are way too large
	// unless we do this.
	DoResize();
	
	return TRUE;
}


//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) 
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		BeginPaint( hWnd, &ps );
		EndPaint( hWnd, &ps );
		
		if( g_pDevice )
			CallAppRender( true );
	}
	break;

	case WM_KEYDOWN:
	{
		AppKey( (int)wParam, lParam&0xFFFF );
	}
	break;

	case WM_KEYUP:
	{
		AppKey( (int)wParam, 0 );
	}
	break;
	
	case WM_CHAR:
	{
		AppChar( (int)wParam );
	}
	break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	
	case WM_LBUTTONDOWN:
	{
		ShowCursor( FALSE );
		SetCapture( g_hWnd );
		g_nCapture++;
	}
	break;

	case WM_LBUTTONUP:
	{
		ShowCursor( TRUE );
		ReleaseCapture( );
		g_nCapture--;
	}
	break;

	case WM_RBUTTONDOWN:
	{
		ShowCursor( FALSE );
		SetCapture( g_hWnd );
		g_nCapture++;
	}
	break;

	case WM_RBUTTONUP:
	{
		ShowCursor( TRUE );
		ReleaseCapture( );
		g_nCapture--;
	}
	break;

	case WM_SIZE:
	{
		if( g_pDevice )
			DoResize();
	}
	break;

	case WM_SETFOCUS:
	{
		g_bFocus = true;
	}
	break;
	
	case WM_KILLFOCUS:
	{
		g_bFocus = false;
	}
	break;
	
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}


bool Sys_Error(const char *pMsg, ...)
{
	char msg[4096];
	va_list marker;

	va_start( marker, pMsg );
	V_vsprintf_safe( msg, pMsg, marker );
	va_end( marker );

	MessageBox( NULL, msg, "Error!", MB_OK );
	exit(1);
	return true;
}


void Sys_Quit()
{
	PostQuitMessage( 0 );
}


void Sys_SetWindowText( char const *pMsg, ... )
{
	va_list marker;
	char msg[4096];
	
	va_start(marker, pMsg);
	V_vsprintf_safe(msg, pMsg, marker);
	va_end(marker);

	SetWindowText( g_hWnd, msg );
}


bool Sys_GetKeyState( int key )
{
	int keyTranslations[][2] =
	{
		{APPKEY_LBUTTON, VK_LBUTTON},
		{APPKEY_RBUTTON, VK_RBUTTON},
		{APPKEY_SPACE,   VK_SPACE}
	};
	int nKeyTranslations = sizeof(keyTranslations) / sizeof(keyTranslations[0]);

	for( int i=0; i < nKeyTranslations; i++ )
	{
		if( key == keyTranslations[i][0] )
		{
			key = keyTranslations[i][1];
			break;
		}
	}

	return !!( GetAsyncKeyState( key ) & 0x8000 );
}


void Sys_Sleep( int ms )
{
	Sleep( (DWORD)ms );
}


bool Sys_HasFocus()
{
	return g_bFocus;
}


char const* Sys_FindArg( char const *pArg, char const *pDefault )
{
	for( int i=0; i < __argc; i++ )
	{
		if( stricmp( __argv[i], pArg ) == 0 )
			return (i+1) < __argc ? __argv[i+1] : "";
	}

	return pDefault;
}


int Sys_FindArgInt( char const *pArg, int defaultVal )
{
	char const *pVal = Sys_FindArg( pArg, NULL );
	if( pVal )
		return atoi( pVal );
	else
		return defaultVal;
}


int Sys_ScreenWidth()
{
	return g_ScreenWidth;
}


int Sys_ScreenHeight()
{
	return g_ScreenHeight;
}


