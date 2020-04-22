//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of the VGUI ISurface interface using the 
// material system to implement it
//
//===========================================================================//

#if defined( WIN32 ) && !defined( _X360 )
#include <windows.h>
#include <zmouse.h>
#endif
#include "inputsystem/iinputsystem.h"
#include "tier2/tier2.h"
#include "Input.h"
#include "vguimatsurface.h"
#include "../vgui2/src/VPanel.h"
#include <vgui/KeyCode.h>
#include <vgui/MouseCode.h>
#include <vgui/IVGui.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui/IClientPanel.h>
#include "inputsystem/ButtonCode.h"
#include "Cursor.h"
#include "tier0/dbg.h"
#include "../vgui2/src/vgui_key_translation.h"
#include <vgui/IInputInternal.h>
#include "tier0/icommandline.h"
#ifdef _X360
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Vgui input events
//-----------------------------------------------------------------------------
enum VguiInputEventType_t
{
	IE_Close = IE_FirstVguiEvent,
	IE_LocateMouseClick,
	IE_SetCursor,
	IE_KeyTyped,
	IE_KeyCodeTyped,
	IE_InputLanguageChanged,
	IE_IMESetWindow,
	IE_IMEStartComposition,
	IE_IMEComposition,
	IE_IMEEndComposition,
	IE_IMEShowCandidates,
	IE_IMEChangeCandidates,
	IE_IMECloseCandidates,
	IE_IMERecomputeModes,
};

void InitInput()
{
	EnableInput( true );
}


static bool s_bInputEnabled = true;
#ifdef WIN32
//-----------------------------------------------------------------------------
// Translates actual keys into VGUI ids
//-----------------------------------------------------------------------------
static WNDPROC s_ChainedWindowProc = NULL;
extern HWND thisWindow;


//-----------------------------------------------------------------------------
// Initializes the input system
//-----------------------------------------------------------------------------

static bool s_bIMEComposing = false;
static HWND s_hLastHWnd = 0;


//-----------------------------------------------------------------------------
// Handles input messages
//-----------------------------------------------------------------------------
static LRESULT CALLBACK MatSurfaceWindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if ( !s_bInputEnabled )
		goto chainWndProc;

	InputEvent_t event;
	memset( &event, 0, sizeof(event) );
	event.m_nTick = g_pInputSystem->GetPollTick();

	if ( hwnd != s_hLastHWnd )
	{
		s_hLastHWnd = hwnd;
		event.m_nType = IE_IMESetWindow;
		event.m_nData = (int)s_hLastHWnd;
		g_pInputSystem->PostUserEvent( event );
	}

	switch(uMsg)
	{
	case WM_QUIT:
		// According to windows docs, WM_QUIT should never be passed to wndprocs 
		Assert( 0 );
		break;

	case WM_CLOSE:
		// Handle close messages
		{
			LONG_PTR wndProc = GetWindowLongPtrW( hwnd, GWLP_WNDPROC );
			if ( wndProc == (LONG_PTR)MatSurfaceWindowProc )
			{
				event.m_nType = IE_Close;
				g_pInputSystem->PostUserEvent( event );
			}
		}
		return 0;

	// All mouse messages need to mark where the click occurred before chaining down
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case MS_WM_XBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case MS_WM_XBUTTONUP:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDBLCLK:
	case MS_WM_XBUTTONDBLCLK:
		event.m_nType = IE_LocateMouseClick;
		event.m_nData = (short)LOWORD(lParam);
		event.m_nData2 = (short)HIWORD(lParam);
		g_pInputSystem->PostUserEvent( event );
		break;

	case WM_SETCURSOR:
		event.m_nType = IE_SetCursor;
		g_pInputSystem->PostUserEvent( event );
		break;

	case WM_XCONTROLLER_KEY:
		if ( IsX360() )
		{	
			// First have to insert the edge case event
			int nRetVal = 0;
			if ( s_ChainedWindowProc )
			{
				nRetVal = CallWindowProcW( s_ChainedWindowProc, hwnd, uMsg, wParam, lParam );
			}

			// xboxissue - as yet HL2 input hasn't been made aware of analog inputs or ports
			// so just digital step on the sample range
			int sample = LOWORD( lParam );
			if ( sample )
			{
				event.m_nType = IE_KeyCodeTyped;
				event.m_nData = (vgui::KeyCode)wParam;
				g_pInputSystem->PostUserEvent( event );
			}
		}
		break;

	// Need to deal with key repeat for keydown since inputsystem doesn't
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		{
			// First have to insert the edge case event
			int nRetVal = 0;
			if ( s_ChainedWindowProc )
			{
				nRetVal = CallWindowProcW( s_ChainedWindowProc, hwnd, uMsg, wParam, lParam );
			}

			int nKeyRepeat = LOWORD( lParam );
			for ( int i = 0; i < nKeyRepeat; ++i )
			{
				event.m_nType = IE_KeyCodeTyped;
				event.m_nData = KeyCode_VirtualKeyToVGUI( wParam );
				g_pInputSystem->PostUserEvent( event );
			}

			return nRetVal;
		}

	case WM_SYSCHAR:
	case WM_CHAR:
		if ( !s_bIMEComposing )
		{
			event.m_nType = IE_KeyTyped;
			event.m_nData = (int)wParam;
			g_pInputSystem->PostUserEvent( event );
		}
		break;

	case WM_INPUTLANGCHANGE:
		event.m_nType = IE_InputLanguageChanged;
		g_pInputSystem->PostUserEvent( event );
		break;

	case WM_IME_STARTCOMPOSITION:
		s_bIMEComposing = true;
		event.m_nType = IE_IMEStartComposition;
		g_pInputSystem->PostUserEvent( event );
		return TRUE;

	case WM_IME_COMPOSITION:
		event.m_nType = IE_IMEComposition;
		event.m_nData = (int)lParam;
		g_pInputSystem->PostUserEvent( event );
		return TRUE;

	case WM_IME_ENDCOMPOSITION:
		s_bIMEComposing = false;
		event.m_nType = IE_IMEEndComposition;
		g_pInputSystem->PostUserEvent( event );
		return TRUE;

	case WM_IME_NOTIFY:
		{
			switch (wParam)
			{
			default:
				break;

			case 14:  // Chinese Traditional IMN_PRIVATE...
				break; 

			case IMN_OPENCANDIDATE:
				event.m_nType = IE_IMEShowCandidates;
				g_pInputSystem->PostUserEvent( event );
				return 1;

			case IMN_CHANGECANDIDATE:
				event.m_nType = IE_IMEChangeCandidates;
				g_pInputSystem->PostUserEvent( event );
				return 0;

			case IMN_CLOSECANDIDATE:
				event.m_nType = IE_IMECloseCandidates;
				g_pInputSystem->PostUserEvent( event );
				break;

			// To detect the change of IME mode, or the toggling of Japanese IME 
			case IMN_SETCONVERSIONMODE:
			case IMN_SETSENTENCEMODE:
			case IMN_SETOPENSTATUS:   
				event.m_nType = IE_IMERecomputeModes;
				g_pInputSystem->PostUserEvent( event );
				if ( wParam == IMN_SETOPENSTATUS )
					return 0;
				break;

			case IMN_CLOSESTATUSWINDOW:   
			case IMN_GUIDELINE:   
			case IMN_OPENSTATUSWINDOW:   
			case IMN_SETCANDIDATEPOS:   
			case IMN_SETCOMPOSITIONFONT:   
			case IMN_SETCOMPOSITIONWINDOW:   
			case IMN_SETSTATUSWINDOWPOS:   
				break;
			}
		}

	case WM_IME_SETCONTEXT:
		// We draw all IME windows ourselves
		lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
		lParam &= ~ISC_SHOWUIGUIDELINE;
		lParam &= ~ISC_SHOWUIALLCANDIDATEWINDOW;
		break;

	case WM_IME_CHAR:
		// We need to process this message so that the IME doesn't double convert the unicode IME characters into garbage characters and post
		//  them to our window... (get ? marks after text entry ).
		return 0;
	}

chainWndProc:
	if ( s_ChainedWindowProc )
		return CallWindowProcW( s_ChainedWindowProc, hwnd, uMsg, wParam, lParam );

	// This means the application is driving the messages (calling our window procedure manually)
	// rather than us hooking their window procedure. The engine needs to do this in order for VCR 
	// mode to play back properly.
	return 0;	
}

#endif


//-----------------------------------------------------------------------------
// Enables/disables input (enabled by default)
//-----------------------------------------------------------------------------
void EnableInput( bool bEnable )
{
#if 0 // #ifdef BENCHMARK
	s_bInputEnabled = false;
#else
	s_bInputEnabled = bEnable;
#endif
}


#ifdef WIN32
//-----------------------------------------------------------------------------
// Hooks input listening up to a window
//-----------------------------------------------------------------------------
void InputAttachToWindow(void *hwnd)
{
#if !defined( USE_SDL )
	s_ChainedWindowProc = (WNDPROC)GetWindowLongPtrW( (HWND)hwnd, GWLP_WNDPROC );
	SetWindowLongPtrW( (HWND)hwnd, GWLP_WNDPROC, (LONG_PTR)MatSurfaceWindowProc );
#endif
}

void InputDetachFromWindow(void *hwnd)
{
	if (!hwnd)
		return;
	if ( s_ChainedWindowProc )
	{
		SetWindowLongPtrW( (HWND)hwnd, GWLP_WNDPROC, (LONG_PTR) s_ChainedWindowProc );
		s_ChainedWindowProc = NULL;
	}
}
#else
void InputAttachToWindow(void *hwnd)
{
#if !defined( OSX ) && !defined( LINUX )
	if ( hwnd && !HushAsserts() )
	{
		// under OSX we use the Cocoa mgr to route events rather than hooking winprocs
		// and under Linux we use SDL
		Assert( !"Implement me" );
	}
#endif
}

void InputDetachFromWindow(void *hwnd)
{
#if !defined( OSX ) && !defined( LINUX )
	if ( hwnd && !HushAsserts() )
	{
		// under OSX we use the Cocoa mgr to route events rather than hooking winprocs
		// and under Linux we use SDL
		Assert( !"Implement me" );
	}
#endif
}
#endif


//-----------------------------------------------------------------------------
// Converts an input system button code to a vgui key code
// FIXME: Remove notion of vgui::KeyCode + vgui::MouseCode altogether
//-----------------------------------------------------------------------------
static vgui::KeyCode ButtonCodeToKeyCode( ButtonCode_t buttonCode )
{
	return ( vgui::KeyCode )buttonCode;
}

static vgui::MouseCode ButtonCodeToMouseCode( ButtonCode_t buttonCode )
{
	return ( vgui::MouseCode )buttonCode;
}


//-----------------------------------------------------------------------------
// Handles an input event, returns true if the event should be filtered
// from the rest of the game
//-----------------------------------------------------------------------------
bool InputHandleInputEvent( const InputEvent_t &event )
{
	switch( event.m_nType )
	{
	case IE_ButtonPressed:
		{
			// NOTE: data2 is the virtual key code (data1 contains the scan-code one)
			ButtonCode_t code = (ButtonCode_t)event.m_nData2;
			if ( IsKeyCode( code ) || IsJoystickCode( code ) )
			{
				vgui::KeyCode keyCode = ButtonCodeToKeyCode( code );
				return g_pIInput->InternalKeyCodePressed( keyCode );
			}

			if ( IsJoystickCode( code ) )
			{
				vgui::KeyCode keyCode = ButtonCodeToKeyCode( code );
				return g_pIInput->InternalKeyCodePressed( keyCode );
			}

			if ( IsMouseCode( code ) )
			{
				vgui::MouseCode mouseCode = ButtonCodeToMouseCode( code );
				return g_pIInput->InternalMousePressed( mouseCode );
			}
		}
		break;

	case IE_ButtonReleased:
		{
			// NOTE: data2 is the virtual key code (data1 contains the scan-code one)
			ButtonCode_t code = (ButtonCode_t)event.m_nData2;
			if ( IsKeyCode( code ) || IsJoystickCode( code ) )
			{
				vgui::KeyCode keyCode = ButtonCodeToKeyCode( code );
				return g_pIInput->InternalKeyCodeReleased( keyCode );
			}

			if ( IsJoystickCode( code ) )
			{
				vgui::KeyCode keyCode = ButtonCodeToKeyCode( code );
				return g_pIInput->InternalKeyCodeReleased( keyCode );
			}

			if ( IsMouseCode( code ) )
			{
				vgui::MouseCode mouseCode = ButtonCodeToMouseCode( code );
				return g_pIInput->InternalMouseReleased( mouseCode );
			}
		}
		break;

	case IE_ButtonDoubleClicked:
		{
			// NOTE: data2 is the virtual key code (data1 contains the scan-code one)
			ButtonCode_t code = (ButtonCode_t)event.m_nData2;
			if ( IsMouseCode( code ) )
			{
				vgui::MouseCode mouseCode = ButtonCodeToMouseCode( code );
				return g_pIInput->InternalMouseDoublePressed( mouseCode );
			}
		}
		break;

	case IE_AnalogValueChanged:
		{
			if ( event.m_nData == MOUSE_WHEEL )
				return g_pIInput->InternalMouseWheeled( event.m_nData3 );
			if ( event.m_nData == MOUSE_XY )
				return g_pIInput->InternalCursorMoved( event.m_nData2, event.m_nData3 );
		}
		break;

	case IE_KeyCodeTyped:
		{
			vgui::KeyCode code = (vgui::KeyCode)event.m_nData;
			g_pIInput->InternalKeyCodeTyped( code );
		}
		return true;

	case IE_KeyTyped:
		{
			vgui::KeyCode code = (vgui::KeyCode)event.m_nData;
			g_pIInput->InternalKeyTyped( code );
		}
		return true;

	case IE_Quit:
		g_pVGui->Stop();
#if defined( USE_SDL )
		return false; // also let higher layers consume it
#else
		return true;
#endif

	case IE_Close:
		// FIXME: Change this so we don't stop until 'save' occurs, etc.
		g_pVGui->Stop();
		return true;

	case IE_SetCursor:
		ActivateCurrentCursor();
		return true;

	case IE_IMESetWindow:
		g_pIInput->SetIMEWindow( (void *)event.m_nData );
		return true;

	case IE_LocateMouseClick:
		g_pIInput->InternalCursorMoved( event.m_nData, event.m_nData2 );
		return true;

	case IE_InputLanguageChanged:
		g_pIInput->OnInputLanguageChanged();
		return true;

	case IE_IMEStartComposition:
		g_pIInput->OnIMEStartComposition();
		return true;

	case IE_IMEComposition:
		g_pIInput->OnIMEComposition( event.m_nData );
		return true;

	case IE_IMEEndComposition:
		g_pIInput->OnIMEEndComposition();
		return true;

	case IE_IMEShowCandidates:
		g_pIInput->OnIMEShowCandidates();
		return true;

	case IE_IMEChangeCandidates:
		g_pIInput->OnIMEChangeCandidates();
		return true;

	case IE_IMECloseCandidates:
		g_pIInput->OnIMECloseCandidates();
		return true;

	case IE_IMERecomputeModes:
		g_pIInput->OnIMERecomputeModes();
		return true;
	}

	return false;
}



