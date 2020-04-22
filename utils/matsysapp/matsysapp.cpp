//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

  glapp.c - Simple OpenGL shell
  
	There are several options allowed on the command line.  They are:
	-height : what window/screen height do you want to use?
	-width  : what window/screen width do you want to use?
	-bpp    : what color depth do you want to use?
	-window : create a rendering window rather than full-screen
	-fov    : use a field of view other than 90 degrees
*/

#include "stdafx.h"

#pragma warning(disable:4305)
#pragma warning(disable:4244)
#include <windows.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <mmsystem.h>
#include "matsysapp.h"
#include "cmdlib.h"
#include "mathlib/mathlib.h"
#include "materialsystem/imaterialproxyfactory.h"
#include "filesystem.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "tier0/icommandline.h"
#include "filesystem_tools.h"
#include "materialsystem/imesh.h"
#include "vstdlib/cvar.h"


static int g_nCapture = 0;


#define OSR2_BUILD_NUMBER 1111
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h


SpewRetval_t MatSysAppSpewFunc( SpewType_t type, char const *pMsg )
{
	printf( "%s", pMsg );
	OutputDebugString( pMsg );

	if( type == SPEW_ASSERT )
		return SPEW_DEBUGGER;
	else if( type == SPEW_ERROR )
		return SPEW_ABORT;
	else
		return SPEW_CONTINUE;
}


class MatSysAppMaterialProxyFactory : public IMaterialProxyFactory
{
public:
	virtual IMaterialProxy *CreateProxy( const char *proxyName )
	{
		CreateInterfaceFn clientFactory = Sys_GetFactoryThis();
		if( !clientFactory )
		{
			return NULL;
		}
		// allocate exactly enough memory for the versioned name on the stack.
		char proxyVersionedName[1024];
		strcpy( proxyVersionedName, proxyName );
		strcat( proxyVersionedName, IMATERIAL_PROXY_INTERFACE_VERSION );

		IMaterialProxy *materialProxy;
		materialProxy = ( IMaterialProxy * )clientFactory( proxyVersionedName, NULL );
		if( !materialProxy )
		{
			return NULL;
		}
		
		return materialProxy;
	}

	virtual void DeleteProxy( IMaterialProxy *pProxy )
	{
		if( pProxy )
		{
			pProxy->Release();
		}
	}
};
MatSysAppMaterialProxyFactory	g_MatSysAppMaterialProxyFactory;


MaterialSystemApp	g_MaterialSystemApp;

float       fAngle = 0.0f;

char       *szFSDesc[] = { "Windowed", "Full Screen" };

extern "C" unsigned int g_Time;
unsigned int g_Time = 0;



int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow)
{
	return g_MaterialSystemApp.WinMain(hInstance, hPrevInstance, szCmdLine, iCmdShow);
}

  

BOOL isdigits( char *s )
{
   int i;
   
   for (i = 0; s[i]; i++)
   {
	   if ((s[i] > '9') || (s[i] < '0'))
       {
		   return FALSE;
       }
   }
   return TRUE;
}


LRESULT CALLBACK WndProc (HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	return g_MaterialSystemApp.WndProc(hwnd, iMsg, wParam, lParam);
}

// This function builds a list the screen resolutions supported by the display driver
static void BuildModeList(screen_res_t* &pResolutions, int &iResCount)
{
   DEVMODE  dm;
   int      mode;
   
   mode = 0;
   while(EnumDisplaySettings(NULL, mode, &dm))
   {
	   mode++;
   }
   
   pResolutions = (screen_res_t *)malloc(sizeof(screen_res_t)*mode);
   mode = 0;
   while(EnumDisplaySettings(NULL, mode, &dm))
   {
	   pResolutions[mode].width = dm.dmPelsWidth;
	   pResolutions[mode].height = dm.dmPelsHeight;
	   pResolutions[mode].bpp = dm.dmBitsPerPel;
	   pResolutions[mode].flags = dm.dmDisplayFlags;
	   pResolutions[mode].frequency = dm.dmDisplayFrequency;
	   mode++;
   }
   iResCount = mode;
}


bool Sys_Error(const char *pMsg, ...)
{
	va_list marker;
	char msg[4096];
	
	va_start(marker, pMsg);
	vsprintf(msg, pMsg, marker);
	va_end(marker);

	MessageBox(NULL, msg, "FATAL ERROR", MB_OK);

	g_MaterialSystemApp.Term();
	exit(1);
	return false;
}


void con_Printf(const char *pMsg, ...)
{
	char msg[2048];
	va_list marker;

	va_start(marker, pMsg);
	vsprintf(msg, pMsg, marker);
	va_end(marker);

	OutputDebugString(msg);
}


bool MSA_IsKeyDown(char key)
{
	return !!(GetAsyncKeyState(key) & 0x8000);
}


bool MSA_IsMouseButtonDown( int button )
{
	if( button == MSA_BUTTON_LEFT )
		return !!(GetAsyncKeyState(VK_LBUTTON) & 0x8000);
	else
		return !!(GetAsyncKeyState(VK_RBUTTON) & 0x8000);
}


void MSA_Sleep(unsigned long count)
{
	if(count > 0)
		Sleep(count);
}


static void MaterialSystem_Error( char *fmt, ... )
{
	char str[4096];
	va_list marker;
	
	va_start(marker, fmt);
	vsprintf(str, fmt, marker);
	va_end(marker);
	
	Sys_Error(str);
}


static void MaterialSystem_Warning( char *fmt, ... )
{
}


void InitMaterialSystemConfig(MaterialSystem_Config_t *pConfig)
{
	memset( pConfig, 0, sizeof(*pConfig) );
//	pConfig->screenGamma = 2.2f;
//	pConfig->texGamma = 2.2;
//	pConfig->overbright = 2;
	pConfig->bAllowCheats = false;
//	pConfig->bLinearFrameBuffer = false;
	pConfig->skipMipLevels = 0;
//	pConfig->lightScale = 1.0f;
	pConfig->bFilterLightmaps = true;
	pConfig->bFilterTextures = true;
	pConfig->bMipMapTextures = true;
	pConfig->nShowMipLevels = 0;
	pConfig->bReverseDepth = false;
	pConfig->bCompressedTextures = true;
//	pConfig->bBumpmap = true;
	pConfig->bShowSpecular = true;
	pConfig->bShowDiffuse = true;
//	pConfig->maxFrameLatency = 1;
	pConfig->bDrawFlat = false;
//	pConfig->bLightingOnly = false;
	pConfig->bSoftwareLighting = false;
	pConfig->bEditMode = false;	// No, we're not in WorldCraft.
//	pConfig->m_bForceTrilinear = false;
	pConfig->m_nForceAnisotropicLevel = 0;
//	pConfig->m_bForceBilinear = false;
}


/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
	float	a;
	float	x;

	if (fov_x < 1 || fov_x > 179)
		fov_x = 90;	// error, set to 90

	x = width/tan(fov_x/360*M_PI);

	a = atan (height/x);

	a = a*360/M_PI;

	return a;
}


MaterialSystemApp::MaterialSystemApp()
{
	Clear();
}

MaterialSystemApp::~MaterialSystemApp()
{
	Term();
}

void MaterialSystemApp::Term()
{
   int i;

   // Free the command line holder memory
   if (m_argc > 0)
   {
	   // Free in reverse order of allocation
	   for (i = (m_argc-1); i >= 0; i--)
       {
		   free(m_argv[i]);
       }
	   // Free the parameter "pockets"
	   free(m_argv);
   }
	
  
   // Free the memory that holds the video resolution list
   if (m_pResolutions)
	   free(m_pResolutions);

   if (m_hDC)
   {
	   if (!ReleaseDC((HWND)m_hWnd, (HDC)m_hDC))
       {
		   MessageBox(NULL, "ShutdownOpenGL - ReleaseDC failed\n", "ERROR", MB_OK);
       }
	   m_hDC   = NULL;
   }
   if (m_bFullScreen)
   {
	   ChangeDisplaySettings( 0, 0 );
   }

	Clear();
}

void MaterialSystemApp::Clear()
{
	m_pMaterialSystem = NULL;
	m_hMaterialSystemInst = 0;
	m_hInstance = 0;
	m_iCmdShow = 0;
	m_hWnd = 0;
	m_hDC = 0;
	m_bActive = false;
	m_bFullScreen = false;
	m_width = m_height = 0;
	m_centerx = m_centery = 0;
	m_bpp = 0;
	m_bChangeBPP = false;
	m_bAllowSoft = 0;
	g_nCapture = 0;
	m_szCmdLine = 0;
	m_argc = 0;
	m_argv = 0;
	m_glnWidth = 0;
	m_glnHeight = 0;
	m_gldAspect = 0;
	m_NearClip = m_FarClip = 0;
	m_fov = 90;
	m_pResolutions = 0;
	m_iResCount = 0;
	m_iVidMode = 0;
}

int MaterialSystemApp::WinMain(void *hInstance, void *hPrevInstance, char *szCmdLine, int iCmdShow)
{
	MSG         msg;
    HDC         hdc;
	memset(&msg,0,sizeof(msg));
	CommandLine()->CreateCmdLine( Plat_GetCommandLine() );

    // Not changable by user
    m_hInstance    = hInstance;
    m_iCmdShow     = iCmdShow;
    m_pResolutions = 0;
    m_NearClip     = 8.0f;
    m_FarClip      = 28400.0f;
	
    // User definable
    m_fov          = 90.0f;
    m_bAllowSoft   = FALSE;
    m_bFullScreen  = TRUE;
	
    // Get the current display device info
    hdc = GetDC( NULL );
    m_DevInfo.bpp    = GetDeviceCaps(hdc, BITSPIXEL);
    m_DevInfo.width  = GetSystemMetrics(SM_CXSCREEN);
    m_DevInfo.height = GetSystemMetrics(SM_CYSCREEN);
    ReleaseDC(NULL, hdc);
	
    // Parse the command line if there is one
    m_argc = 0;
    if (strlen(szCmdLine) > 0)
	{
        m_szCmdLine = szCmdLine;
        GetParameters();
	}
	
    // Default to 640 pixels wide
    m_width  = FindNumParameter("-width", 640);
    m_height = FindNumParameter("-height", 480);
    m_bpp    = FindNumParameter("-bpp", 32);
    m_fov    = FindNumParameter("-fov", 90);
	
    // Check for windowed rendering
    m_bFullScreen = FALSE;
    if (FindParameter("-fullscreen"))
	{
        m_bFullScreen = TRUE;
	}
	
    // Build up the video mode list
    BuildModeList(m_pResolutions, m_iResCount);
	
    // Create the main program window, start up OpenGL and create our viewport
    if (CreateMainWindow( m_width, m_height, m_bpp, m_bFullScreen) != TRUE)
	{
        ChangeDisplaySettings(0, 0);
        MessageBox(NULL, "Unable to create main window.\nProgram will now end.", "FATAL ERROR", MB_OK);
        Term();
        return 0;
	}

    // Turn the cursor off for full-screen mode
    if (m_bFullScreen == TRUE)
	{ 
		// Probably want to do this all the time anyway
        ShowCursor(FALSE);
	}

    // We're live now
    m_bActive = TRUE;
	
	// Define this funciton to init your app
	AppInit();


	RECT rect;
	GetWindowRect( (HWND)m_hWnd, &rect );

	m_centerx = ( rect.left + rect.right ) / 2;
	m_centery = ( rect.top + rect.bottom ) / 2;


    // Begin the main program loop
    while (m_bActive == TRUE)
	{
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
            TranslateMessage (&msg);
            DispatchMessage (&msg);
		}
        
		if (m_pMaterialSystem)
		{
            RenderScene();
		}
	}
    if (m_bFullScreen == TRUE)
	{
        ShowCursor(TRUE);
	}
    
	// Release the parameter and video resolution lists
	Term();
	
	// Tell the app to cleanup.
	AppExit();
    return msg.wParam;
}


long MaterialSystemApp::WndProc(void *inhwnd, long iMsg, long wParam, long lParam)
{
	if(inhwnd != m_hWnd)
	{
		return DefWindowProc((HWND)inhwnd, iMsg, wParam, lParam);
	}

	HWND hwnd = (HWND)inhwnd;
	
	switch (iMsg)
	{
		case WM_CHAR:
			switch(wParam)
			{
			case VK_ESCAPE:
				SendMessage(hwnd, WM_CLOSE, 0, 0);
				break;
			}
			AppChar( wParam );
		break;

		case WM_KEYDOWN:
			AppKey( wParam, true );
			break;
		case WM_KEYUP:
			AppKey( wParam, false );
			break;

		
		case WM_ACTIVATE:
			if ((LOWORD(wParam) != WA_INACTIVE) && ((HWND)lParam == NULL))
			{
				ShowWindow(hwnd, SW_RESTORE);
				SetForegroundWindow(hwnd);
			}
			else
			{
				if (m_bFullScreen)
				{
					ShowWindow(hwnd, SW_MINIMIZE);
				}
			}
			return 0;
			
		case WM_SETFOCUS:
			if(g_bCaptureOnFocus)
			{
				MouseCapture();
			}
			break;

		case WM_KILLFOCUS:
			if(g_bCaptureOnFocus)
			{
				MouseRelease();
			}
			break;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		{
			if(!g_bCaptureOnFocus)
			{
				g_nCapture++;
				MouseCapture();
			}
		}
		break;

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		{
			if(!g_bCaptureOnFocus)
			{
				g_nCapture--;
				MouseRelease();
			}
		}
		break;

		case WM_CLOSE:
			Term();
			m_bActive = FALSE;
			break;
			
		case WM_DESTROY:
			PostQuitMessage (0);
			return 0;
	}
   
   return DefWindowProc (hwnd, iMsg, wParam, lParam);
}


bool MaterialSystemApp::InitMaterialSystem()
{
	RECT rect;

	// Init libraries.
	MathLib_Init( true, true, true, 2.2f, 2.2f, 0.0f, 2.0f );
	SpewOutputFunc( MatSysAppSpewFunc );
	
	if ((m_hDC = GetDC((HWND)m_hWnd)) == NULL)
	{
		ChangeDisplaySettings(0, 0);
		MessageBox(NULL, "GetDC on main window failed", "FATAL ERROR", MB_OK);
		return FALSE;
	}
	
	// Load the material system DLL and get its interface.
	char *pDLLName = "MaterialSystem.dll";
	m_hMaterialSystemInst = LoadLibrary( pDLLName );
	if( !m_hMaterialSystemInst )
	{
		return Sys_Error( "Can't load MaterialSystem.dll\n" );
	}

	CreateInterfaceFn clientFactory = Sys_GetFactory( pDLLName );
	if ( clientFactory )
	{
		m_pMaterialSystem = (IMaterialSystem *)clientFactory( MATERIAL_SYSTEM_INTERFACE_VERSION, NULL );
		if ( !m_pMaterialSystem )
		{
			return Sys_Error( "Could not get the material system interface from materialsystem.dll" );
		}
	}
	else
	{
		return Sys_Error( "Could not find factory interface in library MaterialSystem.dll" );
	}

	const char *pPath = CommandLine()->ParmValue("-game");
	if(!pPath)
	{
		// If they didn't specify -game on the command line, use VPROJECT.
		CmdLib_InitFileSystem( "." );
		pPath = ".";
	}
	else
	{
		CmdLib_InitFileSystem( pPath );
	}
	// BUGBUG: Can't figure out why this is broken.  EXECUTABLE_PATH should already be there
	// but it isn't so the shader system is computing texture memory on each run...
	g_pFullFileSystem->AddSearchPath( "U:\\main\\game\\bin", "EXECUTABLE_PATH", PATH_ADD_TO_TAIL );

	const char *pShaderDLL = FindParameterArg("-shaderdll");
	char defaultShaderDLL[256];
	if(!pShaderDLL)
	{
		strcpy(defaultShaderDLL, "shaderapidx9.dll");
		pShaderDLL = defaultShaderDLL;
	}

	if(!m_pMaterialSystem->Init(pShaderDLL, &g_MatSysAppMaterialProxyFactory, CmdLib_GetFileSystemFactory()))
		return Sys_Error("IMaterialSystem::Init failed");

	MaterialSystem_Config_t config;
	InitMaterialSystemConfig(&config);
	config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, true ); 
	config.SetFlag( MATSYS_VIDCFG_FLAGS_RESIZING, true ); 

	if(!m_pMaterialSystem->SetMode(m_hWnd, config))
		return Sys_Error("IMaterialSystem::SetMode failed");

	m_pMaterialSystem->OverrideConfig(config, false);
	GetClientRect((HWND)m_hWnd, &rect);
	m_glnWidth= rect.right;
	m_glnHeight = rect.bottom;
	m_gldAspect = (float)m_glnWidth / m_glnHeight;

	GetWindowRect( (HWND)m_hWnd, &rect );
	m_centerx = (rect.left + rect.right) / 2;
	m_centery = (rect.top + rect.bottom) / 2;
	
	return true;
}


bool MaterialSystemApp::CreateMainWindow(int width, int height, int bpp, bool fullscreen)
{
   HWND        hwnd;
   WNDCLASSEX  wndclass;
   DWORD       dwStyle, dwExStyle;
   int         x, y, sx, sy, ex, ey, ty;
   
   if ((hwnd = FindWindow(g_szAppName, g_szAppName)) != NULL)
   {
	   SetForegroundWindow(hwnd);
	   return 0;
   }
   
   wndclass.cbSize        = sizeof (wndclass);
   wndclass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
   wndclass.lpfnWndProc   = ::WndProc;
   wndclass.cbClsExtra    = 0;
   wndclass.cbWndExtra    = 0;
   wndclass.hInstance     = (HINSTANCE)m_hInstance;
   wndclass.hIcon         = 0;
   wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW);
   wndclass.hbrBackground = (HBRUSH)COLOR_GRAYTEXT;
   wndclass.lpszMenuName  = NULL;
   wndclass.lpszClassName = g_szAppName;
   wndclass.hIconSm       = 0;
   
   
   if (!RegisterClassEx (&wndclass))
   {
	   MessageBox(NULL, "Window class registration failed.", "FATAL ERROR", MB_OK);
	   return FALSE;
   }
   
   if (fullscreen)
   {
	   dwExStyle = WS_EX_TOPMOST;
	   dwStyle = WS_POPUP | WS_VISIBLE;
	   x = y = 0;
	   sx = m_width;
	   sy = m_height;
   }
   else
   {
	   dwExStyle = 0;
	   //dwStyle = WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;  // Use this if you want a "normal" window
	   dwStyle = WS_CAPTION;
	   ex = GetSystemMetrics(SM_CXEDGE);
	   ey = GetSystemMetrics(SM_CYEDGE);
	   ty = GetSystemMetrics(SM_CYSIZE);
	   // Center the window on the screen
	   x = (m_DevInfo.width / 2) - ((m_width+(2*ex)) / 2);
	   y = (m_DevInfo.height / 2) - ((m_height+(2*ey)+ty) / 2);
	   sx = m_width+(2*ex);
	   sy = m_height+(2*ey)+ty;
	   /*
       Check to be sure the requested window size fits on the screen and
       adjust each dimension to fit if the requested size does not fit.
	   */
	   if (sx >= m_DevInfo.width)
       {
		   x = 0;
		   sx = m_DevInfo.width-(2*ex);
       }
	   if (sy >= m_DevInfo.height)
       {
		   y = 0;
		   sy = m_DevInfo.height-((2*ey)+ty);
       }
   }
   
   if ((hwnd = CreateWindowEx (dwExStyle,
	   g_szAppName,               // window class name
	   g_szAppName,                // window caption
	   dwStyle | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, // window style
	   x,           // initial x position
	   y,           // initial y position
	   sx,           // initial x size
	   sy,           // initial y size
	   NULL,                    // parent window handle
	   NULL,                    // window menu handle
	   (HINSTANCE)m_hInstance,       // program instance handle
	   NULL))                   // creation parameters
	   == NULL)
   {
	   ChangeDisplaySettings(0, 0);
	   MessageBox(NULL, "Window creation failed.", "FATAL ERROR", MB_OK);
	   return FALSE;
   }
   
   m_hWnd = hwnd;
   
   if (!InitMaterialSystem())
   {
	   m_hWnd = NULL;
	   return FALSE;
   }
   
   ShowWindow((HWND)m_hWnd, m_iCmdShow);
   UpdateWindow((HWND)m_hWnd);
   
   SetForegroundWindow((HWND)m_hWnd);
   SetFocus((HWND)m_hWnd);
   
   return TRUE;
}


void MaterialSystemApp::RenderScene()
{
	if(!m_pMaterialSystem)
		return;

	static DWORD lastTime = 0;
	POINT cursorPoint;
	float deltax = 0, deltay = 0, frametime;

	DWORD newTime = GetTickCount();
	DWORD deltaTime = newTime - lastTime;

	if ( deltaTime > 1000 )
		deltaTime = 0;
	
	lastTime = newTime;
	frametime = (float) ((double)deltaTime * 0.001);
	g_Time = newTime;

	if ( g_nCapture )
	{
		GetCursorPos( &cursorPoint );
		SetCursorPos( m_centerx, m_centery );

		deltax = (cursorPoint.x - m_centerx) * 0.1f;
		deltay = (cursorPoint.y - m_centery) * -0.1f;
	}
	else
	{
		deltax = deltay = 0;
	}

	CMatRenderContextPtr pRenderContext(m_pMaterialSystem);
	m_pMaterialSystem->BeginFrame(deltaTime);

	pRenderContext->ClearBuffers(true, true);
	pRenderContext->Viewport( 0, 0, m_width, m_height );
	pRenderContext->MatrixMode(MATERIAL_PROJECTION);
	pRenderContext->LoadIdentity();
	pRenderContext->PerspectiveX(m_fov, m_gldAspect, m_NearClip, m_FarClip);

	pRenderContext->MatrixMode(MATERIAL_VIEW);
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode(MATERIAL_MODEL);
	pRenderContext->LoadIdentity();

	AppRender( frametime, deltax, deltay );

    m_pMaterialSystem->SwapBuffers();

	m_pMaterialSystem->EndFrame();
}


void MaterialSystemApp::MouseCapture()
{
	SetCapture( (HWND)m_hWnd );
    ShowCursor(FALSE);
	SetCursorPos( m_centerx, m_centery );
}


void MaterialSystemApp::MouseRelease()
{
    ShowCursor(TRUE);
	ReleaseCapture();
	
	SetCursorPos( m_centerx, m_centery );
}


void MaterialSystemApp::GetParameters()
{
   int   count;
   char *s, *tstring;
   
   // Make a copy of the command line to count the parameters - strtok is destructive
   tstring = (char *)malloc(sizeof(char)*(strlen(m_szCmdLine)+1));
   strcpy(tstring, m_szCmdLine);
   
   // Count the parameters
   s = strtok(tstring, " ");
   count = 1;
   while (strtok(NULL, " ") != NULL)
   {
	   count++;
   }
   free(tstring);
   
   // Allocate "pockets" for the parameters
   m_argv = (char **)malloc(sizeof(char*)*(count+1));
   
   // Copy first parameter into the "pockets"
   m_argc = 0;
   s = strtok(m_szCmdLine, " ");
   m_argv[m_argc] = (char *)malloc(sizeof(char)*(strlen(s)+1));
   strcpy(m_argv[m_argc], s);
   m_argc++;
   
   // Copy the rest of the parameters
   do
   {
	   // get the next token
	   s = strtok(NULL, " ");
	   if (s != NULL)
       { 
		   // add it to the list
		   m_argv[m_argc] = (char *)malloc(sizeof(char)*(strlen(s)+1));
		   strcpy(m_argv[m_argc], s);
		   m_argc++;
       }
   }
   while (s != NULL);
}


int MaterialSystemApp::FindNumParameter(const char *s, int defaultVal)
{
   int i;
   
   for (i = 0; i < (m_argc-1); i++)
   {
	   if (stricmp(m_argv[i], s) == 0)
       {
		   if (isdigits(m_argv[i+1]))
           {
			   return(atoi(m_argv[i+1]));
           }
		   else
           {
			   return defaultVal;
           }
       }
   }
   return defaultVal;
}


bool MaterialSystemApp::FindParameter(const char *s)
{
   int i;
   
   for (i = 0; i < m_argc; i++)
   {
	   if (stricmp(m_argv[i], s) == 0)
       {
		   return true;
       }
   }
   return false;
}


const char *MaterialSystemApp::FindParameterArg( const char *s )
{
   int i;
   
   for (i = 0; i < m_argc; i++)
   {
	   if (stricmp(m_argv[i], s) == 0)
       {
		   if( (i+1) < m_argc )
			   return m_argv[i+1];
			else
				return "";
       }
   }
   return NULL;
}


void MaterialSystemApp::SetTitleText(const char *fmt, ...)
{
	char str[4096];
	va_list marker;

	va_start(marker, fmt);
	vsprintf(str, fmt, marker);
	va_end(marker);

	::SetWindowText((HWND)m_hWnd, str);
}


void MaterialSystemApp::MakeWindowTopmost()
{
	::SetWindowPos((HWND)m_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
}


void MaterialSystemApp::AppShutdown()
{
	SendMessage( (HWND)m_hWnd, WM_CLOSE, 0, 0 );
}


void MaterialSystemApp::QuitNextFrame()
{
	PostMessage( (HWND)m_hWnd, WM_CLOSE, 0, 0 );
}


