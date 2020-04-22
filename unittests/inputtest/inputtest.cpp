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
#include "inputsystem/iinputsystem.h"
#include "filesystem.h"
#include "filesystem_init.h"
#include "tier0/icommandline.h"


//-----------------------------------------------------------------------------
// Purpose: Warning/Msg call back through this API
// Input  : type - 
//			*pMsg - 
// Output : SpewRetval_t
//-----------------------------------------------------------------------------
SpewRetval_t SpewFunc( SpewType_t type, char const *pMsg )
{	
	OutputDebugString( pMsg );
	return SPEW_CONTINUE;
}


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CInputTestApp : public CTier2SteamApp
{
	typedef CTier2SteamApp BaseClass;

public:
	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit( );
	virtual int Main();
	virtual void PostShutdown( );
	virtual void Destroy();
	virtual const char *GetAppName() { return "InputTest"; }
	virtual bool AppUsesReadPixels() { return false; }

private:
	// Window management
	bool CreateAppWindow( char const *pTitle, bool bWindowed, int w, int h );

	// Sets up the game path
	bool SetupSearchPaths();

	HWND m_HWnd;
};

DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( CInputTestApp );


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CInputTestApp::Create()
{
	SpewOutputFunc( SpewFunc );

	AppSystemInfo_t appSystems[] = 
	{
		{ "inputsystem.dll",		INPUTSYSTEM_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	if ( !AddSystems( appSystems ) ) 
		return false;

	return true;
}

void CInputTestApp::Destroy()
{
}


//-----------------------------------------------------------------------------
// Window management
//-----------------------------------------------------------------------------
bool CInputTestApp::CreateAppWindow( char const *pTitle, bool bWindowed, int w, int h )
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
bool CInputTestApp::SetupSearchPaths()
{
	if ( !BaseClass::SetupSearchPaths( NULL, false, true ) )
		return false;

	g_pFullFileSystem->AddSearchPath( GetGameInfoPath(), "SKIN", PATH_ADD_TO_HEAD );
	return true;
}


//-----------------------------------------------------------------------------
// PreInit, PostShutdown
//-----------------------------------------------------------------------------
bool CInputTestApp::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	if (!g_pFullFileSystem || !g_pInputSystem )
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

	if (!CreateAppWindow( "InputTest", bWindowed, iWidth, iHeight ))
		return false;

	g_pInputSystem->AttachToWindow( m_HWnd );
	return true;
}

void CInputTestApp::PostShutdown( )
{
	g_pInputSystem->DetachFromWindow( );
	BaseClass::PostShutdown();
}


	
//-----------------------------------------------------------------------------
// main application
//-----------------------------------------------------------------------------
int CInputTestApp::Main()
{
	while( true )
	{
		g_pInputSystem->PollInputState();

		int nEventCount = g_pInputSystem->GetEventCount();
		const InputEvent_t* pEvents = g_pInputSystem->GetEventData( );
		for ( int i = 0; i < nEventCount; ++i )
		{
			const InputEvent_t* pEvent = &pEvents[i];
			switch( pEvent->m_nType )
			{
			case IE_ButtonPressed:
				Msg("Button Pressed Event %d : Start tick %d\n", pEvent->m_nData, pEvent->m_nTick );
				break;

			case IE_ButtonReleased:
				Msg("Button Released Event %d : End tick %d Start tick %d\n", pEvent->m_nData, pEvent->m_nTick, g_pInputSystem->GetButtonPressedTick( (ButtonCode_t)pEvent->m_nData ) );
				break;

			case IE_ButtonDoubleClicked:
				Msg("Button Double clicked Event %d : Start tick %d\n", pEvent->m_nData, pEvent->m_nTick );
				break;

			case IE_AnalogValueChanged:
				Msg("Analog Value Changed %d : Start tick %d Value %d\n", pEvent->m_nData, pEvent->m_nTick, pEvent->m_nData2 );
				break;

			case IE_Quit:
				Msg("Quit");
				return 1;
			}
		}
	}

	return 1;
}



