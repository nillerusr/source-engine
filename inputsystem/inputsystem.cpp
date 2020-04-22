//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "inputsystem.h"
#include "key_translation.h"
#include "inputsystem/ButtonCode.h"
#include "inputsystem/AnalogCode.h"
#include "tier0/etwprof.h"
#include "tier1/convar.h"
#include "tier0/icommandline.h"

#if defined( USE_SDL )
#undef M_PI
#include "SDL.h"
static void initKeymap(void);
#endif

#ifdef _X360
#include "xbox/xbox_win32stubs.h"
#endif
ConVar joy_xcontroller_found( "joy_xcontroller_found", "1", FCVAR_HIDDEN, "Automatically set to 1 if an xcontroller has been detected." );

//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CInputSystem g_InputSystem;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CInputSystem, IInputSystem, 
						INPUTSYSTEM_INTERFACE_VERSION, g_InputSystem );



#if defined( WIN32 ) && !defined( _X360 )
typedef BOOL (WINAPI *RegisterRawInputDevices_t)
(
	PCRAWINPUTDEVICE pRawInputDevices,
	UINT uiNumDevices,
	UINT cbSize
);

typedef UINT (WINAPI *GetRawInputData_t)
(
	HRAWINPUT hRawInput,
	UINT uiCommand,
	LPVOID pData,
	PUINT pcbSize,
	UINT cbSizeHeader
);

RegisterRawInputDevices_t pfnRegisterRawInputDevices;
GetRawInputData_t pfnGetRawInputData;
#endif



//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CInputSystem::CInputSystem()
{
	m_nLastPollTick = m_nLastSampleTick = m_StartupTimeTick = 0;
	m_ChainedWndProc = 0;
	m_hAttachedHWnd = 0;
	m_hEvent = NULL;
	m_bEnabled = true;
	m_bPumpEnabled = true;
	m_bIsPolling = false;
	m_JoysticksEnabled.ClearAllFlags();
	m_nJoystickCount = 0;
	m_bJoystickInitialized = false;
	m_nPollCount = 0;
	m_PrimaryUserId = INVALID_USER_ID;
	m_uiMouseWheel = 0;
	m_bXController = false;
	m_bRawInputSupported = false;
	m_bSteamController = false;
	m_bSteamControllerActionsInitialized = false;
	m_bSteamControllerActive = false;

	Assert( (MAX_JOYSTICKS + 7) >> 3 << sizeof(unsigned short) ); 

	m_pXInputDLL = NULL;
	m_pRawInputDLL = NULL;

#if defined ( _WIN32 ) && !defined ( _X360 )
	// NVNT DLL
	m_pNovintDLL = NULL;
#endif

	m_bConsoleTextMode = false;
	m_bSkipControllerInitialization = false;

	if ( CommandLine()->CheckParm( "-nosteamcontroller" ) )
	{
		m_bSkipControllerInitialization = true;
	}
}

CInputSystem::~CInputSystem()
{
	if ( m_pXInputDLL )
	{
		Sys_UnloadModule( m_pXInputDLL );
		m_pXInputDLL = NULL;
	}

	if ( m_pRawInputDLL )
	{
		Sys_UnloadModule( m_pRawInputDLL );
		m_pRawInputDLL = NULL;
	}

#if defined ( _WIN32 ) && !defined ( _X360 )
	// NVNT DLL unload
	if ( m_pNovintDLL )
	{
		Sys_UnloadModule( m_pNovintDLL );
		m_pNovintDLL = NULL;
	}
#endif
}


//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
InitReturnVal_t CInputSystem::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	m_StartupTimeTick = Plat_MSTime();


#if !defined( POSIX )
	if ( IsPC() )
	{
		m_uiMouseWheel = RegisterWindowMessage( "MSWHEEL_ROLLMSG" );
	}

	m_hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	if ( !m_hEvent )
		return INIT_FAILED;
#endif

	// Initialize the input system copy of the steam API context, for use by controller stuff (don't do this if we're a dedicated server).
	if ( !m_bSkipControllerInitialization && SteamAPI_InitSafe() )
	{
		m_SteamAPIContext.Init();
		if ( m_SteamAPIContext.SteamController() )
		{
			m_SteamAPIContext.SteamController()->Init();
			m_bSteamController = InitializeSteamControllers();
			m_bSteamControllerActionsInitialized = m_bSteamController && InitializeSteamControllerActionSets();
			if ( m_bSteamControllerActionsInitialized )
			{
				ActivateSteamControllerActionSet( GAME_ACTION_SET_MENUCONTROLS );
			}
		}
	}

	ButtonCode_InitKeyTranslationTable();
	ButtonCode_UpdateScanCodeLayout();

	joy_xcontroller_found.SetValue( 0 );
	if ( IsPC() && !m_bConsoleTextMode )
	{
		InitializeJoysticks();
		if ( m_bXController )
			joy_xcontroller_found.SetValue( 1 );


#if defined( PLATFORM_WINDOWS_PC )
		// NVNT try and load and initialize through the haptic dll, but only if the drivers are installed
		HMODULE hdl = LoadLibraryEx( "hdl.dll", NULL, LOAD_LIBRARY_AS_DATAFILE );

		if ( hdl )
		{
			m_pNovintDLL = Sys_LoadModule( "haptics.dll" );
			if ( m_pNovintDLL )
			{
				InitializeNovintDevices();
			}
			FreeLibrary( hdl );
		}
#endif
	}

#if defined( _X360 )
		SetPrimaryUserId( XBX_GetPrimaryUserId() );
		InitializeXDevices();
		m_bXController = true;
#endif

#if defined( USE_SDL )

	m_bRawInputSupported = true;
	initKeymap();

#elif defined( WIN32 ) && !defined( _X360 )

	// Check if this version of windows supports raw mouse input (later than win2k)
	m_bRawInputSupported = false;

	CSysModule *m_pRawInputDLL = Sys_LoadModule( "USER32.dll" );
	if ( m_pRawInputDLL )
	{
		pfnRegisterRawInputDevices = (RegisterRawInputDevices_t)GetProcAddress( (HMODULE)m_pRawInputDLL, "RegisterRawInputDevices" );
		pfnGetRawInputData = (GetRawInputData_t)GetProcAddress( (HMODULE)m_pRawInputDLL, "GetRawInputData" );
		if ( pfnRegisterRawInputDevices && pfnGetRawInputData )
			m_bRawInputSupported = true;
	}

#endif

	return INIT_OK; 
}

bool CInputSystem::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

#if defined( USE_SDL )
	m_pLauncherMgr = (ILauncherMgr *)factory( SDLMGR_INTERFACE_VERSION, NULL );
#endif

	return true;
}


//-----------------------------------------------------------------------------
// Shutdown
//-----------------------------------------------------------------------------
void CInputSystem::Shutdown()
{
#if !defined( POSIX )
	if ( m_hEvent != NULL )
	{
		CloseHandle( m_hEvent );
		m_hEvent = NULL;
	}
#endif
	
	if ( IsPC() )
	{
		ShutdownJoysticks();
	}

	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Sleep until input
//-----------------------------------------------------------------------------
void CInputSystem::SleepUntilInput( int nMaxSleepTimeMS )
{
#if defined( _WIN32 ) && !defined( USE_SDL )
	if ( nMaxSleepTimeMS < 0 )
	{
		nMaxSleepTimeMS = INFINITE;
	}

	MsgWaitForMultipleObjects( 1, &m_hEvent, FALSE, nMaxSleepTimeMS, QS_ALLEVENTS );
#elif defined( USE_SDL )
	m_pLauncherMgr->WaitUntilUserInput( nMaxSleepTimeMS );
#else
#warning "need a SleepUntilInput impl"
#endif
}



//-----------------------------------------------------------------------------
// Callback to call into our class
//-----------------------------------------------------------------------------
#if defined( PLATFORM_WINDOWS )
static LRESULT CALLBACK InputSystemWindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return g_InputSystem.WindowProc( hwnd, uMsg, wParam, lParam );
}
#endif


//-----------------------------------------------------------------------------
// Hooks input listening up to a window
//-----------------------------------------------------------------------------
void CInputSystem::AttachToWindow( void* hWnd )
{
	Assert( m_hAttachedHWnd == 0 );
	if ( m_hAttachedHWnd )
	{
		Warning( "CInputSystem::AttachToWindow: Cannot attach to two windows at once!\n" );
		return;
	}

#if defined( PLATFORM_WINDOWS )
	m_ChainedWndProc = (WNDPROC)GetWindowLongPtrW( (HWND)hWnd, GWLP_WNDPROC );
	SetWindowLongPtrW( (HWND)hWnd, GWLP_WNDPROC, (LONG_PTR)InputSystemWindowProc );
#endif

	m_hAttachedHWnd = (HWND)hWnd;

#if defined( PLATFORM_WINDOWS_PC ) && !defined( USE_SDL )
	// NVNT inform novint devices of window
	AttachWindowToNovintDevices( hWnd );

	// register to read raw mouse input

#if !defined(HID_USAGE_PAGE_GENERIC)
#define HID_USAGE_PAGE_GENERIC         ((USHORT) 0x01)
#endif
#if !defined(HID_USAGE_GENERIC_MOUSE)
#define HID_USAGE_GENERIC_MOUSE        ((USHORT) 0x02)
#endif

	if ( m_bRawInputSupported )
	{
		RAWINPUTDEVICE Rid[1];
		Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC; 
		Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE; 
		Rid[0].dwFlags = RIDEV_INPUTSINK;   
		Rid[0].hwndTarget = g_InputSystem.m_hAttachedHWnd; // GetHhWnd;
		pfnRegisterRawInputDevices(Rid, ARRAYSIZE(Rid), sizeof(Rid[0]));
	}
#endif

	// New window, clear input state
	ClearInputState();
}


//-----------------------------------------------------------------------------
// Unhooks input listening from a window
//-----------------------------------------------------------------------------
void CInputSystem::DetachFromWindow( )
{
	if ( !m_hAttachedHWnd )
		return;

	ResetInputState();

#if defined( PLATFORM_WINDOWS )
	if ( m_ChainedWndProc )
	{
		SetWindowLongPtrW( m_hAttachedHWnd, GWLP_WNDPROC, (LONG_PTR)m_ChainedWndProc );
		m_ChainedWndProc = 0;
	}
#endif

#if defined( PLATFORM_WINDOWS_PC )
	// NVNT inform novint devices loss of window
	DetachWindowFromNovintDevices( );
#endif
	m_hAttachedHWnd = 0;
}


//-----------------------------------------------------------------------------
// Enables/disables input
//-----------------------------------------------------------------------------
void CInputSystem::EnableInput( bool bEnable )
{
	m_bEnabled = bEnable;
}


//-----------------------------------------------------------------------------
// Enables/disables the inputsystem windows message pump
//-----------------------------------------------------------------------------
void CInputSystem::EnableMessagePump( bool bEnable )
{
	m_bPumpEnabled = bEnable;
}
	

//-----------------------------------------------------------------------------
// Clears the input state, doesn't generate key-up messages
//-----------------------------------------------------------------------------
void CInputSystem::ClearInputState()
{
	for ( int i = 0; i < INPUT_STATE_COUNT; ++i )
	{
		InputState_t& state = m_InputState[i];
		state.m_ButtonState.ClearAll();
		memset( state.m_pAnalogDelta, 0, ANALOG_CODE_LAST * sizeof(int) );
		memset( state.m_pAnalogValue, 0, ANALOG_CODE_LAST * sizeof(int) );
		memset( state.m_ButtonPressedTick, 0, BUTTON_CODE_LAST * sizeof(int) );
		memset( state.m_ButtonReleasedTick, 0, BUTTON_CODE_LAST * sizeof(int) );
		state.m_Events.Purge();
		state.m_bDirty = false;
	}
	memset( m_appXKeys, 0, XUSER_MAX_COUNT * XK_MAX_KEYS * sizeof(appKey_t) );
}

//-----------------------------------------------------------------------------
// Resets the input state
//-----------------------------------------------------------------------------
void CInputSystem::ResetInputState()
{
	ReleaseAllButtons();
	ZeroAnalogState( 0, ANALOG_CODE_LAST - 1 );
	memset( m_appXKeys, 0, XUSER_MAX_COUNT * XK_MAX_KEYS * sizeof(appKey_t) );

	m_mouseRawAccumX = m_mouseRawAccumY = 0;
}


//-----------------------------------------------------------------------------
// Convert back + forth between ButtonCode/AnalogCode + strings
//-----------------------------------------------------------------------------
const char *CInputSystem::ButtonCodeToString( ButtonCode_t code ) const
{
	return ButtonCode_ButtonCodeToString( code, m_bXController );
}

const char *CInputSystem::AnalogCodeToString( AnalogCode_t code ) const
{
	return AnalogCode_AnalogCodeToString( code );
}

ButtonCode_t CInputSystem::StringToButtonCode( const char *pString ) const
{
	return ButtonCode_StringToButtonCode( pString, m_bXController );
}

AnalogCode_t CInputSystem::StringToAnalogCode( const char *pString ) const
{
	return AnalogCode_StringToAnalogCode( pString );
}


//-----------------------------------------------------------------------------
// Convert back + forth between virtual codes + button codes
// FIXME: This is a temporary piece of code
//-----------------------------------------------------------------------------
ButtonCode_t CInputSystem::VirtualKeyToButtonCode( int nVirtualKey ) const
{
	return ButtonCode_VirtualKeyToButtonCode( nVirtualKey );
}

int CInputSystem::ButtonCodeToVirtualKey( ButtonCode_t code ) const
{
	return ButtonCode_ButtonCodeToVirtualKey( code );
}

ButtonCode_t CInputSystem::XKeyToButtonCode( int nPort, int nXKey ) const
{
	if ( m_bXController )
		return ButtonCode_XKeyToButtonCode( nPort, nXKey );
	return KEY_NONE;
}

ButtonCode_t CInputSystem::ScanCodeToButtonCode( int lParam ) const
{
	return ButtonCode_ScanCodeToButtonCode( lParam );
}

ButtonCode_t CInputSystem::SKeyToButtonCode( int nPort, int nXKey ) const
{
	return ButtonCode_SKeyToButtonCode( nPort, nXKey );
}

//-----------------------------------------------------------------------------
// Post an event to the queue
//-----------------------------------------------------------------------------
void CInputSystem::PostEvent( int nType, int nTick, int nData, int nData2, int nData3 )
{
  InputEvent_t event;

  memset( &event, 0, sizeof(event) );
  event.m_nType = nType;
  event.m_nTick = nTick;
  event.m_nData = nData;
  event.m_nData2 = nData2;
  event.m_nData3 = nData3;

  PostUserEvent( event );
}


//-----------------------------------------------------------------------------
// Post an button press event to the queue
//-----------------------------------------------------------------------------
void CInputSystem::PostButtonPressedEvent( InputEventType_t nType, int nTick, ButtonCode_t scanCode, ButtonCode_t virtualCode )
{
	InputState_t &state = m_InputState[ m_bIsPolling ];
	if ( !state.m_ButtonState.IsBitSet( scanCode ) )
	{
		// Update button state
		state.m_ButtonState.Set( scanCode ); 
		state.m_ButtonPressedTick[ scanCode ] = nTick;

		// Add this event to the app-visible event queue
		PostEvent( nType, nTick, scanCode, virtualCode );

#if defined( _X360 )
		// FIXME: Remove! Fake a windows message for vguimatsurface's input handler
		if ( IsJoystickCode( scanCode ) )
		{
			ProcessEvent( WM_XCONTROLLER_KEY, scanCode, 1 );
		}
#endif
	}
}


//-----------------------------------------------------------------------------
// Post an button release event to the queue
//-----------------------------------------------------------------------------
void CInputSystem::PostButtonReleasedEvent( InputEventType_t nType, int nTick, ButtonCode_t scanCode, ButtonCode_t virtualCode )
{
	InputState_t &state = m_InputState[ m_bIsPolling ];
	if ( state.m_ButtonState.IsBitSet( scanCode ) )
	{
		// Update button state
		state.m_ButtonState.Clear( scanCode ); 
		state.m_ButtonReleasedTick[ scanCode ] = nTick;

		// Add this event to the app-visible event queue
		PostEvent( nType, nTick, scanCode, virtualCode );

#if defined( _X360 )
		// FIXME: Remove! Fake a windows message for vguimatsurface's input handler
		if ( IsJoystickCode( scanCode ) )
		{
			ProcessEvent( WM_XCONTROLLER_KEY, scanCode, 0 );
		}
#endif
	}
}


//-----------------------------------------------------------------------------
//	Purpose: Pass Joystick button events through the engine's window procs
//-----------------------------------------------------------------------------
void CInputSystem::ProcessEvent( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
#if !defined( POSIX )
	// To prevent subtle input timing bugs, all button events must be fed 
	// through the window proc once per frame, same as the keyboard and mouse.
	HWND hWnd = GetFocus();
	WNDPROC windowProc = (WNDPROC)GetWindowLongPtrW(hWnd, GWLP_WNDPROC );
	if ( windowProc )
	{
		windowProc( hWnd, uMsg, wParam, lParam );
	}
#endif
}


//-----------------------------------------------------------------------------
// Copies the input state record over
//-----------------------------------------------------------------------------
void CInputSystem::CopyInputState( InputState_t *pDest, const InputState_t &src, bool bCopyEvents )
{
	pDest->m_Events.RemoveAll();
	pDest->m_bDirty = false;
	if ( src.m_bDirty )
	{
		pDest->m_ButtonState = src.m_ButtonState;
		memcpy( &pDest->m_ButtonPressedTick, &src.m_ButtonPressedTick, sizeof( pDest->m_ButtonPressedTick ) );
		memcpy( &pDest->m_ButtonReleasedTick, &src.m_ButtonReleasedTick, sizeof( pDest->m_ButtonReleasedTick ) );
		memcpy( &pDest->m_pAnalogDelta, &src.m_pAnalogDelta, sizeof( pDest->m_pAnalogDelta ) );
		memcpy( &pDest->m_pAnalogValue, &src.m_pAnalogValue, sizeof( pDest->m_pAnalogValue ) );
		if ( bCopyEvents )
		{
			if ( src.m_Events.Count() > 0 )
			{
				pDest->m_Events.EnsureCount( src.m_Events.Count() );
				memcpy( pDest->m_Events.Base(), src.m_Events.Base(), src.m_Events.Count() * sizeof(InputEvent_t) );
			}
		}
	}
}


#if defined( PLATFORM_WINDOWS_PC )
void CInputSystem::PollInputState_Windows()
{
	if ( m_bPumpEnabled )
	{
		// Poll mouse + keyboard
		MSG msg;
		while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
		{
			if ( msg.message == WM_QUIT )
			{
				PostEvent( IE_Quit, m_nLastSampleTick );
				break;
			}

			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}

		// NOTE: Under some implementations of Win9x, 
		// dispatching messages can cause the FPU control word to change
		SetupFPUControlWord();
	}
}
#endif

#if defined( USE_SDL )

static BYTE        scantokey[SDL_NUM_SCANCODES];

static void initKeymap(void)
{
	memset(scantokey, '\0', sizeof (scantokey));

	for (int i = SDL_SCANCODE_A; i <= SDL_SCANCODE_Z; i++)
		scantokey[i] = KEY_A + (i - SDL_SCANCODE_A);
	for (int i = SDL_SCANCODE_1; i <= SDL_SCANCODE_9; i++)
		scantokey[i] = KEY_1 + (i - SDL_SCANCODE_1);
	for (int i = SDL_SCANCODE_F1; i <= SDL_SCANCODE_F12; i++)
		scantokey[i] = KEY_F1 + (i - SDL_SCANCODE_F1);
	for (int i = SDL_SCANCODE_KP_1; i <= SDL_SCANCODE_KP_9; i++)
		scantokey[i] = KEY_PAD_1 + (i - SDL_SCANCODE_KP_1);

	scantokey[SDL_SCANCODE_0] = KEY_0;
    scantokey[SDL_SCANCODE_KP_0] = KEY_PAD_0;
    scantokey[SDL_SCANCODE_RETURN] = KEY_ENTER;
    scantokey[SDL_SCANCODE_ESCAPE] = KEY_ESCAPE;
    scantokey[SDL_SCANCODE_BACKSPACE] = KEY_BACKSPACE;
    scantokey[SDL_SCANCODE_TAB] = KEY_TAB;
    scantokey[SDL_SCANCODE_SPACE] = KEY_SPACE;
    scantokey[SDL_SCANCODE_MINUS] = KEY_MINUS;
    scantokey[SDL_SCANCODE_EQUALS] = KEY_EQUAL;
    scantokey[SDL_SCANCODE_LEFTBRACKET] = KEY_LBRACKET;
    scantokey[SDL_SCANCODE_RIGHTBRACKET] = KEY_RBRACKET;
    scantokey[SDL_SCANCODE_BACKSLASH] = KEY_BACKSLASH;
    scantokey[SDL_SCANCODE_SEMICOLON] = KEY_SEMICOLON;
    scantokey[SDL_SCANCODE_APOSTROPHE] = KEY_APOSTROPHE;
    scantokey[SDL_SCANCODE_GRAVE] = KEY_BACKQUOTE;
    scantokey[SDL_SCANCODE_COMMA] = KEY_COMMA;
    scantokey[SDL_SCANCODE_PERIOD] = KEY_PERIOD;
    scantokey[SDL_SCANCODE_SLASH] = KEY_SLASH;
    scantokey[SDL_SCANCODE_CAPSLOCK] = KEY_CAPSLOCK;
    scantokey[SDL_SCANCODE_SCROLLLOCK] = KEY_SCROLLLOCK;
    scantokey[SDL_SCANCODE_INSERT] = KEY_INSERT;
    scantokey[SDL_SCANCODE_HOME] = KEY_HOME;
    scantokey[SDL_SCANCODE_PAGEUP] = KEY_PAGEUP;
    scantokey[SDL_SCANCODE_DELETE] = KEY_DELETE;
    scantokey[SDL_SCANCODE_END] = KEY_END;
    scantokey[SDL_SCANCODE_PAGEDOWN] = KEY_PAGEDOWN;
    scantokey[SDL_SCANCODE_RIGHT] = KEY_RIGHT;
    scantokey[SDL_SCANCODE_LEFT] = KEY_LEFT;
    scantokey[SDL_SCANCODE_DOWN] = KEY_DOWN;
    scantokey[SDL_SCANCODE_UP] = KEY_UP;
    scantokey[SDL_SCANCODE_NUMLOCKCLEAR] = KEY_NUMLOCK;
    scantokey[SDL_SCANCODE_KP_DIVIDE] = KEY_PAD_DIVIDE;
    scantokey[SDL_SCANCODE_KP_MULTIPLY] = KEY_PAD_MULTIPLY;
    scantokey[SDL_SCANCODE_KP_MINUS] = KEY_PAD_MINUS;
    scantokey[SDL_SCANCODE_KP_PLUS] = KEY_PAD_PLUS;
	// Map keybad enter to enter for vgui. This means vgui dialog won't ever see KEY_PAD_ENTER
    scantokey[SDL_SCANCODE_KP_ENTER] = KEY_ENTER;
    scantokey[SDL_SCANCODE_KP_PERIOD] = KEY_PAD_DECIMAL;
    scantokey[SDL_SCANCODE_APPLICATION] = KEY_APP;
    scantokey[SDL_SCANCODE_LCTRL] = KEY_LCONTROL;
    scantokey[SDL_SCANCODE_LSHIFT] = KEY_LSHIFT;
    scantokey[SDL_SCANCODE_LALT] = KEY_LALT;
    scantokey[SDL_SCANCODE_LGUI] = KEY_LWIN;
    scantokey[SDL_SCANCODE_RCTRL] = KEY_RCONTROL;
    scantokey[SDL_SCANCODE_RSHIFT] = KEY_RSHIFT;
    scantokey[SDL_SCANCODE_RALT] = KEY_RALT;
    scantokey[SDL_SCANCODE_RGUI] = KEY_RWIN;
}

bool MapCocoaVirtualKeyToButtonCode( int nCocoaVirtualKeyCode, ButtonCode_t *pOut )
{
	if ( nCocoaVirtualKeyCode < 0 )
		*pOut = (ButtonCode_t)(-1 * nCocoaVirtualKeyCode);
	else 
	{
		nCocoaVirtualKeyCode &= 0x000000ff;
	
		*pOut = (ButtonCode_t)scantokey[nCocoaVirtualKeyCode];
	}

	return true;
}

void CInputSystem::PollInputState_Platform()
{
	InputState_t &state = m_InputState[ m_bIsPolling ];

	if (  m_bPumpEnabled )
		m_pLauncherMgr->PumpWindowsMessageLoop();
	// These are Carbon virtual key codes. AFAIK they don't have a header that defines these, but they are supposed to map
	// to the same letters across international keyboards, so our mapping here should work.
	CCocoaEvent events[32];
	while ( 1 )
	{
		int nEvents = m_pLauncherMgr->GetEvents( events, ARRAYSIZE( events ) );
		if ( nEvents == 0 )
			break;

		for ( int iEvent=0; iEvent < nEvents; iEvent++ )
		{
			CCocoaEvent *pEvent = &events[iEvent];

			switch( pEvent->m_EventType )
			{
				case CocoaEvent_Deleted:
					break;

				case CocoaEvent_KeyDown:
				{
					ButtonCode_t virtualCode;
					if ( MapCocoaVirtualKeyToButtonCode( pEvent->m_VirtualKeyCode, &virtualCode ) )
					{
						ButtonCode_t scanCode = virtualCode;

						if( ( scanCode != BUTTON_CODE_NONE ) )
						{
							// For SDL, hitting spacebar causes a SDL_KEYDOWN event, then SDL_TEXTINPUT with
							//	event.text.text[0] = ' ', and then we get here and wind up sending two events
							//	to PostButtonPressedEvent. The first is virtualCode = ' ', the 2nd has virtualCode = 0.
							// This will confuse Button::OnKeyCodePressed(), which is checking for space keydown
							//	followed by space keyup. So we ignore all BUTTON_CODE_NONE events here.
							PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, scanCode, virtualCode );
						}
						
						InputEvent_t event;
						memset( &event, 0, sizeof(event) );
						event.m_nTick = GetPollTick();
						// IE_KeyCodeTyped
						event.m_nType = IE_FirstVguiEvent + 4;
						event.m_nData = scanCode;
						g_pInputSystem->PostUserEvent( event );
						
					}

					if ( !(pEvent->m_ModifierKeyMask & (1<<eCommandKey) ) && pEvent->m_VirtualKeyCode >= 0 && pEvent->m_UnicodeKey > 0 )
					{
						InputEvent_t event;
						memset( &event, 0, sizeof(event) );
						event.m_nTick = GetPollTick();
						// IE_KeyTyped
						event.m_nType = IE_FirstVguiEvent + 3;
						event.m_nData = (int)pEvent->m_UnicodeKey;
						g_pInputSystem->PostUserEvent( event );
					}
					
				}
				break;

				case CocoaEvent_KeyUp:
				{
					ButtonCode_t virtualCode;
					if ( MapCocoaVirtualKeyToButtonCode( pEvent->m_VirtualKeyCode, &virtualCode ) )
					{
						if( virtualCode != BUTTON_CODE_NONE )
						{
							ButtonCode_t scanCode = virtualCode;
							PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, scanCode, virtualCode );
						}
					}
				}
				break;

				case CocoaEvent_MouseButtonDown:
				{
					int nButtonMask = pEvent->m_MouseButtonFlags;
					ButtonCode_t dblClickCode = BUTTON_CODE_INVALID;
					if ( pEvent->m_nMouseClickCount > 1 )
					{
						switch( pEvent->m_MouseButton )
						{
							default:
							case COCOABUTTON_LEFT:
								dblClickCode = MOUSE_LEFT;
								break;
							case COCOABUTTON_RIGHT:
								dblClickCode = MOUSE_RIGHT;
								break;
							case COCOABUTTON_MIDDLE:
								dblClickCode = MOUSE_MIDDLE;
								break;
							case COCOABUTTON_4:
								dblClickCode = MOUSE_4;
								break;
							case COCOABUTTON_5:
								dblClickCode = MOUSE_5;
								break;
						}
					}
					UpdateMouseButtonState( nButtonMask, dblClickCode );
				}
				break;

				case CocoaEvent_MouseButtonUp:
				{
					int nButtonMask = pEvent->m_MouseButtonFlags;
					UpdateMouseButtonState( nButtonMask );
				}
				break;

				case CocoaEvent_MouseMove:
				{
					UpdateMousePositionState( state, (short)pEvent->m_MousePos[0], (short)pEvent->m_MousePos[1] );

					InputEvent_t event;
					memset( &event, 0, sizeof(event) );
					event.m_nTick = GetPollTick();
					// IE_LocateMouseClick
					event.m_nType = IE_FirstVguiEvent + 1;
					event.m_nData = (short)pEvent->m_MousePos[0];
					event.m_nData2 = (short)pEvent->m_MousePos[1];
					g_pInputSystem->PostUserEvent( event );
				}
				break;
					
				case CocoaEvent_MouseScroll:
				{
					ButtonCode_t code = (short)pEvent->m_MousePos[1] > 0 ? MOUSE_WHEEL_UP : MOUSE_WHEEL_DOWN;
					state.m_ButtonPressedTick[ code ] = state.m_ButtonReleasedTick[ code ] = m_nLastSampleTick;
					PostEvent( IE_ButtonPressed, m_nLastSampleTick, code, code );
					PostEvent( IE_ButtonReleased, m_nLastSampleTick, code, code );
					
					state.m_pAnalogDelta[ MOUSE_WHEEL ] = pEvent->m_MousePos[1];
					state.m_pAnalogValue[ MOUSE_WHEEL ] += state.m_pAnalogDelta[ MOUSE_WHEEL ];
					PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_WHEEL, state.m_pAnalogValue[ MOUSE_WHEEL ], state.m_pAnalogDelta[ MOUSE_WHEEL ] );
				}
				break;
					
				case CocoaEvent_AppActivate:
				{		
					InputEvent_t event;
					memset( &event, 0, sizeof(event) );
					event.m_nType = IE_FirstAppEvent + 2; // IE_AppActivated (defined in sys_mainwind.cpp).
					event.m_nData = (bool)(pEvent->m_ModifierKeyMask != 0);

					g_pInputSystem->PostUserEvent( event );

					if( pEvent->m_ModifierKeyMask == 0 )
					{
						// App just lost focus. Handle like WM_ACTIVATEAPP in CInputSystem::WindowProc().
						// Otherwise alt+tab will bring focus away from our app, vgui will still think that
						//	the alt key is down, and when we regain focus, fun ensues.
						g_pInputSystem->ResetInputState();
					}
				}
				break;
				case CocoaEvent_AppQuit:
				{
					PostEvent( IE_Quit, m_nLastSampleTick );

				}
				break;
				break;
			}
		}
	}
}
#endif // USE_SDL


//-----------------------------------------------------------------------------
// Polls the current input state
//-----------------------------------------------------------------------------
void CInputSystem::PollInputState()
{
	m_bIsPolling = true;
	++m_nPollCount;

	// Deals with polled input events
	InputState_t &queuedState = m_InputState[ INPUT_STATE_QUEUED ];
	CopyInputState( &m_InputState[ INPUT_STATE_CURRENT ], queuedState, true );

	// Sample the joystick
	SampleDevices();

	// NOTE: This happens after SampleDevices since that updates LastSampleTick
	// Also, I believe it's correct to post the joystick events with
	// the LastPollTick not updated (not 100% sure though)
	m_nLastPollTick = m_nLastSampleTick;

#if defined( PLATFORM_WINDOWS_PC )
	PollInputState_Windows();
#endif

#if defined( USE_SDL )
	PollInputState_Platform();
#endif

	// Leave the queued state up-to-date with the current
	CopyInputState( &queuedState, m_InputState[ INPUT_STATE_CURRENT ], false );

	m_bIsPolling = false;
}


//-----------------------------------------------------------------------------
// Computes the sample tick
//-----------------------------------------------------------------------------
int CInputSystem::ComputeSampleTick()
{
	// This logic will only fail if the app has been running for 49.7 days
	int nSampleTick;

	DWORD nCurrentTick = Plat_MSTime();
	if ( nCurrentTick >= m_StartupTimeTick )
	{
		nSampleTick = (int)( nCurrentTick - m_StartupTimeTick );
	}
	else
	{
		DWORD nDelta = (DWORD)0xFFFFFFFF - m_StartupTimeTick;
		nSampleTick = (int)( nCurrentTick + nDelta ) + 1;
	}
	return nSampleTick;
}


//-----------------------------------------------------------------------------
// How many times has poll been called?
//-----------------------------------------------------------------------------
int CInputSystem::GetPollCount() const
{
	return m_nPollCount;
}


//-----------------------------------------------------------------------------
// Samples attached devices and appends events to the input queue
//-----------------------------------------------------------------------------
void CInputSystem::SampleDevices( void )
{
	m_nLastSampleTick = ComputeSampleTick();

	PollJoystick();

#if defined( PLATFORM_WINDOWS_PC )
	// NVNT if we have device/s poll them.
	if ( m_bNovintDevices )
	{
		PollNovintDevices();
	}
#endif

	PollSteamControllers();
}

//-----------------------------------------------------------------------------
//	Purpose: Sets a player as the primary user - all other controllers will be ignored.
//-----------------------------------------------------------------------------
void CInputSystem::SetPrimaryUserId( int userId )
{
	if ( userId >= XUSER_MAX_COUNT || userId < 0 )
	{
		m_PrimaryUserId = INVALID_USER_ID;
	}
	else
	{
		m_PrimaryUserId = userId;
	}
#if !defined(POSIX)
	XBX_SetPrimaryUserId( m_PrimaryUserId );
#endif
	ConMsg("PrimaryUserId is %d\n", m_PrimaryUserId );
}

//-----------------------------------------------------------------------------
//	Purpose: Forwards rumble info to attached devices
//-----------------------------------------------------------------------------
void CInputSystem::SetRumble( float fLeftMotor, float fRightMotor, int userId )
{
	SetXDeviceRumble( fLeftMotor, fRightMotor, userId );
}


//-----------------------------------------------------------------------------
//	Purpose: Force an immediate stop, transmits immediately to all devices
//-----------------------------------------------------------------------------
void CInputSystem::StopRumble( void )
{
#ifdef _X360
	xdevice_t* pXDevice = &m_XDevices[0];

	for ( int i = 0; i < XUSER_MAX_COUNT; ++i, ++pXDevice )
	{
		if ( pXDevice->active )
		{
			pXDevice->vibration.wLeftMotorSpeed = 0;
			pXDevice->vibration.wRightMotorSpeed = 0;
			pXDevice->pendingRumbleUpdate = true;
			WriteToXDevice( pXDevice );
		}
	}
#else
	for ( int i = 0; i < XUSER_MAX_COUNT; ++i )
	{
		SetRumble(0.0, 0.0, i);
	}
#endif
}


//-----------------------------------------------------------------------------
// Joystick interface
//-----------------------------------------------------------------------------
int CInputSystem::GetJoystickCount() const
{
	return m_nJoystickCount;
}

void CInputSystem::EnableJoystickInput( int nJoystick, bool bEnable )
{
	m_JoysticksEnabled.SetFlag( 1 << nJoystick, bEnable ); 
}

void CInputSystem::EnableJoystickDiagonalPOV( int nJoystick, bool bEnable )
{
	m_pJoystickInfo[ nJoystick ].m_bDiagonalPOVControlEnabled = bEnable;
}

//-----------------------------------------------------------------------------
// Poll current state
//-----------------------------------------------------------------------------
int CInputSystem::GetPollTick() const
{
	return m_nLastPollTick;
}
	
bool CInputSystem::IsButtonDown( ButtonCode_t code ) const
{
	return m_InputState[INPUT_STATE_CURRENT].m_ButtonState.IsBitSet( code );
}

int CInputSystem::GetAnalogValue( AnalogCode_t code ) const
{
	return m_InputState[INPUT_STATE_CURRENT].m_pAnalogValue[code];
}

int CInputSystem::GetAnalogDelta( AnalogCode_t code ) const
{
	return m_InputState[INPUT_STATE_CURRENT].m_pAnalogDelta[code];
}

int CInputSystem::GetButtonPressedTick( ButtonCode_t code ) const
{
	return m_InputState[INPUT_STATE_CURRENT].m_ButtonPressedTick[code];
}

int CInputSystem::GetButtonReleasedTick( ButtonCode_t code ) const
{
	return m_InputState[INPUT_STATE_CURRENT].m_ButtonReleasedTick[code];
}


//-----------------------------------------------------------------------------
// Returns the input events since the last poll
//-----------------------------------------------------------------------------
int CInputSystem::GetEventCount() const
{
	return m_InputState[INPUT_STATE_CURRENT].m_Events.Count();
}

const InputEvent_t* CInputSystem::GetEventData( ) const
{
	return m_InputState[INPUT_STATE_CURRENT].m_Events.Base();
}


//-----------------------------------------------------------------------------
// Posts a user-defined event into the event queue; this is expected
// to be called in overridden wndprocs connected to the root panel.
//-----------------------------------------------------------------------------
void CInputSystem::PostUserEvent( const InputEvent_t &event )
{
	InputState_t &state = m_InputState[ m_bIsPolling ];
	state.m_Events.AddToTail( event );
	state.m_bDirty = true;
}

	
//-----------------------------------------------------------------------------
// Chains the window message to the previous wndproc
//-----------------------------------------------------------------------------
inline LRESULT CInputSystem::ChainWindowMessage( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
#if !defined( POSIX )
	if ( m_ChainedWndProc )
		return CallWindowProc( m_ChainedWndProc, hwnd, uMsg, wParam, lParam );
#endif
	// FIXME: This comment is lifted from vguimatsurface; 
	// may not apply in future when the system is completed.

	// This means the application is driving the messages (calling our window procedure manually)
	// rather than us hooking their window procedure. The engine needs to do this in order for VCR 
	// mode to play back properly.
	return 0;	
}

	
//-----------------------------------------------------------------------------
// Release all buttons
//-----------------------------------------------------------------------------
void CInputSystem::ReleaseAllButtons( int nFirstButton, int nLastButton )
{
	// Force button up messages for all down buttons
	for ( int i = nFirstButton; i <= nLastButton; ++i )
	{
		PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, (ButtonCode_t)i, (ButtonCode_t)i );
	}
}


//-----------------------------------------------------------------------------
// Zero analog state
//-----------------------------------------------------------------------------
void CInputSystem::ZeroAnalogState( int nFirstState, int nLastState )
{
	InputState_t &state = m_InputState[ m_bIsPolling ];
	memset( &state.m_pAnalogDelta[nFirstState], 0, ( nLastState - nFirstState + 1 ) * sizeof(int) );
	memset( &state.m_pAnalogValue[nFirstState], 0, ( nLastState - nFirstState + 1 ) * sizeof(int) );
}


//-----------------------------------------------------------------------------
// Determines all mouse button presses
//-----------------------------------------------------------------------------
int CInputSystem::ButtonMaskFromMouseWParam( WPARAM wParam, ButtonCode_t code, bool bDown ) const
{
	int nButtonMask = 0;

#if defined( PLATFORM_WINDOWS )
	if ( wParam & MK_LBUTTON )
	{
		nButtonMask |= 1;
	}

	if ( wParam & MK_RBUTTON )
	{
		nButtonMask |= 2;
	}

	if ( wParam & MK_MBUTTON )
	{
		nButtonMask |= 4;
	}

	if ( wParam & MS_MK_BUTTON4 )
	{
		nButtonMask |= 8;
	}

	if ( wParam & MS_MK_BUTTON5 )
	{
		nButtonMask |= 16;
	}
#endif

#ifdef _DEBUG
	if ( code != BUTTON_CODE_INVALID )
	{
		int nMsgMask = 1 << ( code - MOUSE_FIRST );
		int nTestMask = bDown ? nMsgMask : 0;
		Assert( ( nButtonMask & nMsgMask ) == nTestMask );
	}
#endif

	return nButtonMask;
}


//-----------------------------------------------------------------------------
// Updates the state of all mouse buttons
//-----------------------------------------------------------------------------
void CInputSystem::UpdateMouseButtonState( int nButtonMask, ButtonCode_t dblClickCode )
{
	for ( int i = 0; i < 5; ++i )
	{
		ButtonCode_t code = (ButtonCode_t)( MOUSE_FIRST + i );
		bool bDown = ( nButtonMask & ( 1 << i ) ) != 0;
		if ( bDown )
		{
			InputEventType_t type = ( code != dblClickCode ) ? IE_ButtonPressed : IE_ButtonDoubleClicked; 
			PostButtonPressedEvent( type, m_nLastSampleTick, code, code );
		}
		else
		{
			PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, code, code );
		}
	}
}


//-----------------------------------------------------------------------------
// Handles input messages
//-----------------------------------------------------------------------------
void CInputSystem::SetCursorPosition( int x, int y )
{
	if ( !m_hAttachedHWnd )
		return;

#if defined( PLATFORM_WINDOWS )
	POINT pt;
	pt.x = x; pt.y = y;
	ClientToScreen( (HWND)m_hAttachedHWnd, &pt );
	SetCursorPos( pt.x, pt.y );
#elif defined( USE_SDL )
	m_pLauncherMgr->SetCursorPosition( x, y );
#endif

	InputState_t &state = m_InputState[ m_bIsPolling ];
	bool bXChanged = ( state.m_pAnalogValue[ MOUSE_X ] != x );
	bool bYChanged = ( state.m_pAnalogValue[ MOUSE_Y ] != y );

	state.m_pAnalogValue[ MOUSE_X ] = x;
	state.m_pAnalogValue[ MOUSE_Y ] = y;
	state.m_pAnalogDelta[ MOUSE_X ] = 0;
	state.m_pAnalogDelta[ MOUSE_Y ] = 0;

	if ( bXChanged )
	{
		PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_X, state.m_pAnalogValue[ MOUSE_X ], state.m_pAnalogDelta[ MOUSE_X ] );
	}
	if ( bYChanged )
	{
		PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_Y, state.m_pAnalogValue[ MOUSE_Y ], state.m_pAnalogDelta[ MOUSE_Y ] );
	}
	if ( bXChanged || bYChanged )
	{
		PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_XY, state.m_pAnalogValue[ MOUSE_X ], state.m_pAnalogValue[ MOUSE_Y ] );
	}
}


void CInputSystem::UpdateMousePositionState( InputState_t &state, short x, short y )
{
	int nOldX = state.m_pAnalogValue[ MOUSE_X ];
	int nOldY = state.m_pAnalogValue[ MOUSE_Y ];

	state.m_pAnalogValue[ MOUSE_X ] = x;
	state.m_pAnalogValue[ MOUSE_Y ] = y;
	state.m_pAnalogDelta[ MOUSE_X ] = state.m_pAnalogValue[ MOUSE_X ] - nOldX;
	state.m_pAnalogDelta[ MOUSE_Y ] = state.m_pAnalogValue[ MOUSE_Y ] - nOldY;

	if ( state.m_pAnalogDelta[ MOUSE_X ] != 0 )
	{
		PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_X, state.m_pAnalogValue[ MOUSE_X ], state.m_pAnalogDelta[ MOUSE_X ] );
	}
	if ( state.m_pAnalogDelta[ MOUSE_Y ] != 0 )
	{
		PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_Y, state.m_pAnalogValue[ MOUSE_Y ], state.m_pAnalogDelta[ MOUSE_Y ] );
	}
	if ( state.m_pAnalogDelta[ MOUSE_X ] != 0 || state.m_pAnalogDelta[ MOUSE_Y ] != 0 )
	{
		PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_XY, state.m_pAnalogValue[ MOUSE_X ], state.m_pAnalogValue[ MOUSE_Y ] );
	}
}


//-----------------------------------------------------------------------------
// Handles input messages
//-----------------------------------------------------------------------------
LRESULT CInputSystem::WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
#if defined( PLATFORM_WINDOWS ) // We use this even for SDL to handle mouse move.
	if ( !m_bEnabled )
		return ChainWindowMessage( hwnd, uMsg, wParam, lParam );

	if ( hwnd != m_hAttachedHWnd )
		return ChainWindowMessage( hwnd, uMsg, wParam, lParam );

	InputState_t &state = m_InputState[ m_bIsPolling ];
	switch( uMsg )
	{
	
#if !defined( USE_SDL )
	case WM_ACTIVATEAPP:
		if ( hwnd == m_hAttachedHWnd )
		{
			bool bActivated = ( wParam == 1 );
			if ( !bActivated )
			{
				ResetInputState();
			}
		}
		break;

	case WM_LBUTTONDOWN:
		{
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_LEFT, true );
			ETWMouseDown( 0, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask );
		}
		break;

	case WM_LBUTTONUP:
		{
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_LEFT, false );
			ETWMouseUp( 0, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask );
		}
		break;

	case WM_RBUTTONDOWN:
		{
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_RIGHT, true );
			ETWMouseDown( 2, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask );
		}
		break;

	case WM_RBUTTONUP:
		{
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_RIGHT, false );
			ETWMouseUp( 2, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask );
		}
		break;

	case WM_MBUTTONDOWN:
		{
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_MIDDLE, true );
			ETWMouseDown( 1, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask );
		}
		break;

	case WM_MBUTTONUP:
		{
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_MIDDLE, false );
			ETWMouseUp( 1, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask );
		}
		break;

	case MS_WM_XBUTTONDOWN:
		{
			ButtonCode_t code = ( HIWORD( wParam ) == 1 ) ? MOUSE_4 : MOUSE_5;
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, code, true );
			UpdateMouseButtonState( nButtonMask );

			// Windows docs say the XBUTTON messages we should return true from
			return TRUE;
		}
		break;

	case MS_WM_XBUTTONUP:
		{
			ButtonCode_t code = ( HIWORD( wParam ) == 1 ) ? MOUSE_4 : MOUSE_5;
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, code, false );
			UpdateMouseButtonState( nButtonMask );

			// Windows docs say the XBUTTON messages we should return true from
			return TRUE;
		}
		break;

	case WM_LBUTTONDBLCLK:
		{
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_LEFT, true );
			ETWMouseDown( 0, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask, MOUSE_LEFT );
		}
		break;

	case WM_RBUTTONDBLCLK:
		{
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_RIGHT, true );
			ETWMouseDown( 2, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask, MOUSE_RIGHT );
		}
		break;

	case WM_MBUTTONDBLCLK:
		{
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_MIDDLE, true );
			ETWMouseDown( 1, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask, MOUSE_MIDDLE );
		}
		break;

	case MS_WM_XBUTTONDBLCLK:
		{
			ButtonCode_t code = ( HIWORD( wParam ) == 1 ) ? MOUSE_4 : MOUSE_5;
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, code, true );
			UpdateMouseButtonState( nButtonMask, code );

			// Windows docs say the XBUTTON messages we should return true from
			return TRUE;
		}
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		{
			// Suppress key repeats
			if ( !( lParam & ( 1<<30 ) ) )
			{
				// NOTE: These two can be unequal! For example, keypad enter
				// which returns KEY_ENTER from virtual keys, and KEY_PAD_ENTER from scan codes
				// Since things like vgui care about virtual keys; we're going to
				// put both scan codes in the input message
				ButtonCode_t virtualCode = ButtonCode_VirtualKeyToButtonCode( wParam );
				ButtonCode_t scanCode = ButtonCode_ScanCodeToButtonCode( lParam );
				PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, scanCode, virtualCode );

				// Post ETW events describing key presses to help correlate input events to performance
				// problems in the game.
				ETWKeyDown( scanCode, virtualCode, ButtonCodeToString( virtualCode ) );

				// Deal with toggles
				if ( scanCode == KEY_CAPSLOCK || scanCode == KEY_SCROLLLOCK || scanCode == KEY_NUMLOCK )
				{
					int nVirtualKey;
					ButtonCode_t toggleCode;
					switch( scanCode )
					{
					default: case KEY_CAPSLOCK: nVirtualKey = VK_CAPITAL; toggleCode = KEY_CAPSLOCKTOGGLE; break;
					case KEY_SCROLLLOCK: nVirtualKey = VK_SCROLL; toggleCode = KEY_SCROLLLOCKTOGGLE; break;
					case KEY_NUMLOCK: nVirtualKey = VK_NUMLOCK; toggleCode = KEY_NUMLOCKTOGGLE; break;
					};

					SHORT wState = GetKeyState( nVirtualKey );
					bool bToggleState = ( wState & 0x1 ) != 0;
					PostButtonPressedEvent( bToggleState ? IE_ButtonPressed : IE_ButtonReleased, m_nLastSampleTick, toggleCode, toggleCode );
				}
			}
		}
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		{
			// Don't handle key ups if the key's already up. This can happen when we alt-tab back to the engine.
			ButtonCode_t virtualCode = ButtonCode_VirtualKeyToButtonCode( wParam );
			ButtonCode_t scanCode = ButtonCode_ScanCodeToButtonCode( lParam );
			PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, scanCode, virtualCode );
		}
		break;

	case WM_MOUSEWHEEL:
		{
			ButtonCode_t code = (short)HIWORD( wParam ) > 0 ? MOUSE_WHEEL_UP : MOUSE_WHEEL_DOWN;
			state.m_ButtonPressedTick[ code ] = state.m_ButtonReleasedTick[ code ] = m_nLastSampleTick;
			PostEvent( IE_ButtonPressed, m_nLastSampleTick, code, code );
			PostEvent( IE_ButtonReleased, m_nLastSampleTick, code, code );

			state.m_pAnalogDelta[ MOUSE_WHEEL ] = ( (short)HIWORD(wParam) ) / WHEEL_DELTA;
			state.m_pAnalogValue[ MOUSE_WHEEL ] += state.m_pAnalogDelta[ MOUSE_WHEEL ];
			PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_WHEEL, state.m_pAnalogValue[ MOUSE_WHEEL ], state.m_pAnalogDelta[ MOUSE_WHEEL ] );
		}
		break;

#if defined( PLATFORM_WINDOWS_PC )
	case WM_INPUT:
		{
			if ( m_bRawInputSupported )
			{
				UINT dwSize = 40;
				static BYTE lpb[40];

				pfnGetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));

				RAWINPUT* raw = (RAWINPUT*)lpb;
				if (raw->header.dwType == RIM_TYPEMOUSE) 
				{
					m_mouseRawAccumX += raw->data.mouse.lLastX;
					m_mouseRawAccumY += raw->data.mouse.lLastY;
				} 
			}
		}
		break;
#endif

#endif // !USE_SDL

	case WM_MOUSEMOVE:
		{
			UpdateMousePositionState( state, (short)LOWORD(lParam), (short)HIWORD(lParam) );

			int nButtonMask = ButtonMaskFromMouseWParam( wParam );
			UpdateMouseButtonState( nButtonMask );
		}
 		break;

	}
	
#if defined( PLATFORM_WINDOWS_PC ) && !defined( USE_SDL )
	// Can't put this in the case statement, it's not constant
	if ( uMsg == m_uiMouseWheel )
	{
		ButtonCode_t code = ( ( int )wParam ) > 0 ? MOUSE_WHEEL_UP : MOUSE_WHEEL_DOWN;
		state.m_ButtonPressedTick[ code ] = state.m_ButtonReleasedTick[ code ] = m_nLastSampleTick;
		PostEvent( IE_ButtonPressed, m_nLastSampleTick, code, code );
		PostEvent( IE_ButtonReleased, m_nLastSampleTick, code, code );

		state.m_pAnalogDelta[ MOUSE_WHEEL ] = ( ( int )wParam ) / WHEEL_DELTA;
		state.m_pAnalogValue[ MOUSE_WHEEL ] += state.m_pAnalogDelta[ MOUSE_WHEEL ];
		PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_WHEEL, state.m_pAnalogValue[ MOUSE_WHEEL ], state.m_pAnalogDelta[ MOUSE_WHEEL ] );
	}
#endif

#endif // PLATFORM_WINDOWS
	return ChainWindowMessage( hwnd, uMsg, wParam, lParam );
}

bool CInputSystem::GetRawMouseAccumulators( int& accumX, int& accumY )
{
#if defined( USE_SDL )

	if ( m_pLauncherMgr )
	{
		m_pLauncherMgr->GetMouseDelta( accumX, accumY, false );
		return true;
	}
	return false;

#else

	accumX = m_mouseRawAccumX;
	accumY = m_mouseRawAccumY;
	m_mouseRawAccumX = m_mouseRawAccumY = 0;
	return m_bRawInputSupported;

#endif
}

void CInputSystem::SetConsoleTextMode( bool bConsoleTextMode )
{
	/* If someone calls this after init, shut it down. */
	if ( bConsoleTextMode && m_bJoystickInitialized )
	{
		ShutdownJoysticks();
	}

	m_bConsoleTextMode = bConsoleTextMode;
}

ISteamController* CInputSystem::SteamControllerInterface()
{
	if ( m_bSkipControllerInitialization )
	{
		return nullptr;
	}
	else
	{
		return m_SteamAPIContext.SteamController();
	}
}
