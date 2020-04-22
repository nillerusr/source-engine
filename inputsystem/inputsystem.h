//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#ifndef INPUTSYSTEM_H
#define INPUTSYSTEM_H
#ifdef _WIN32
#pragma once
#endif


#ifdef WIN32
#if !defined( _X360 )
#define _WIN32_WINNT 0x502
#include <windows.h>
#include <zmouse.h>
#include "xbox/xboxstubs.h"
#include "../../dx9sdk/include/XInput.h"
#endif
#endif

#ifdef POSIX
#include "posix_stubs.h"
#endif // POSIX

#include "appframework/ilaunchermgr.h"

#include "inputsystem/iinputsystem.h"
#include "tier2/tier2.h"

#include "inputsystem/ButtonCode.h"
#include "inputsystem/AnalogCode.h"
#include "bitvec.h"
#include "tier1/utlvector.h"
#include "tier1/utlflags.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#include "xbox/xbox_console.h"
#endif

#include "steam/steam_api.h"

//-----------------------------------------------------------------------------
// Implementation of the input system
//-----------------------------------------------------------------------------
class CInputSystem : public CTier2AppSystem< IInputSystem >
{
	typedef CTier2AppSystem< IInputSystem > BaseClass;

public:
	// Constructor, destructor
	CInputSystem();
	virtual ~CInputSystem();

	// Inherited from IAppSystem
	virtual	InitReturnVal_t Init();
	virtual bool Connect( CreateInterfaceFn factory );

	virtual void Shutdown();

	// Inherited from IInputSystem
	virtual void AttachToWindow( void* hWnd );
	virtual void DetachFromWindow();
	virtual void EnableInput( bool bEnable );
	virtual void EnableMessagePump( bool bEnable );
	virtual int GetPollTick() const;
	virtual void PollInputState();
	virtual bool IsButtonDown( ButtonCode_t code ) const;
	virtual int GetButtonPressedTick( ButtonCode_t code ) const;
	virtual int GetButtonReleasedTick( ButtonCode_t code ) const;
	virtual int GetAnalogValue( AnalogCode_t code ) const;
	virtual int GetAnalogDelta( AnalogCode_t code ) const;
	virtual int GetEventCount() const;
	virtual const InputEvent_t* GetEventData() const;
	virtual void PostUserEvent( const InputEvent_t &event );
	virtual int GetJoystickCount() const;
	virtual void EnableJoystickInput( int nJoystick, bool bEnable );
	virtual void EnableJoystickDiagonalPOV( int nJoystick, bool bEnable );
	virtual void SampleDevices( void );
	virtual void SetRumble( float fLeftMotor, float fRightMotor, int userId );
	virtual void StopRumble( void );
	virtual void ResetInputState( void );
	virtual void SetPrimaryUserId( int userId );
	virtual const char *ButtonCodeToString( ButtonCode_t code ) const;
	virtual const char *AnalogCodeToString( AnalogCode_t code ) const;
	virtual ButtonCode_t StringToButtonCode( const char *pString ) const;
	virtual AnalogCode_t StringToAnalogCode( const char *pString ) const;
	virtual ButtonCode_t VirtualKeyToButtonCode( int nVirtualKey ) const;
	virtual int ButtonCodeToVirtualKey( ButtonCode_t code ) const;
	virtual ButtonCode_t ScanCodeToButtonCode( int lParam ) const;
	virtual void SleepUntilInput( int nMaxSleepTimeMS );
	virtual int GetPollCount() const;
	virtual void SetCursorPosition( int x, int y );
#if defined( WIN32 ) && !defined ( _X360 )
	virtual void *GetHapticsInterfaceAddress() const;
#else
	virtual void *GetHapticsInterfaceAddress() const { return NULL;}	
#endif
	bool GetRawMouseAccumulators( int& accumX, int& accumY );
	virtual void SetConsoleTextMode( bool bConsoleTextMode );

	// Windows proc
	LRESULT WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );


private:
	enum
	{
		STICK1_AXIS_LEFT,
		STICK1_AXIS_RIGHT,
		STICK1_AXIS_DOWN,
		STICK1_AXIS_UP,
		STICK2_AXIS_LEFT,
		STICK2_AXIS_RIGHT,
		STICK2_AXIS_DOWN,
		STICK2_AXIS_UP,
		MAX_STICKAXIS
	};

	// track Xbox stick keys from previous frame
	enum
	{
		LASTKEY_STICK1_X,
		LASTKEY_STICK1_Y,
		LASTKEY_STICK2_X,
		LASTKEY_STICK2_Y,
		MAX_LASTKEY,
	};

	enum
	{
		INPUT_STATE_QUEUED = 0,
		INPUT_STATE_CURRENT,

		INPUT_STATE_COUNT,
	};

public:

	struct JoystickInfo_t
	{
		void *m_pDevice;  // Really an SDL_GameController*, NULL if not present.
		void *m_pHaptic;  // Really an SDL_Haptic*
		float m_fCurrentRumble;
		bool m_bRumbleEnabled;
		int m_nButtonCount;
		int m_nAxisFlags;
		int m_nDeviceId;
		bool m_bHasPOVControl;
		bool m_bDiagonalPOVControlEnabled;
		unsigned int m_nFlags;
		unsigned int m_nLastPolledButtons;
		unsigned int m_nLastPolledAxisButtons;
		unsigned int m_nLastPolledPOVState;
		unsigned long m_pLastPolledAxes[MAX_JOYSTICK_AXES];
	};

	struct xdevice_t
	{
		int					userId;
		byte				type;
		byte				subtype;
		word				flags;
		bool				active;
		XINPUT_STATE		states[2];
		int					newState;
		xKey_t				lastStickKeys[MAX_LASTKEY];
		int					stickThreshold[MAX_STICKAXIS];
		float				stickScale[MAX_STICKAXIS];
		int					quitTimeout;
		int					dpadLock;
		// rumble
		XINPUT_VIBRATION	vibration;
		bool				pendingRumbleUpdate;
	};

	struct appKey_t
	{
		int repeats;
		int	sample;
	};

	struct InputState_t
	{
		// Analog states
		CBitVec<BUTTON_CODE_LAST> m_ButtonState;
		int m_ButtonPressedTick[BUTTON_CODE_LAST];
		int m_ButtonReleasedTick[BUTTON_CODE_LAST];
		int m_pAnalogDelta[ANALOG_CODE_LAST];
		int m_pAnalogValue[ANALOG_CODE_LAST];
		CUtlVector< InputEvent_t > m_Events;
		bool m_bDirty;
	};

	// Initializes all Xbox controllers
	void InitializeXDevices( void );

	// Opens an Xbox controller
	void OpenXDevice( xdevice_t* pXDevice, int userId );

	// Closes an Xbox controller
	void CloseXDevice( xdevice_t* pXDevice );

	// Samples the Xbox controllers
	void PollXDevices( void );

	// Samples an Xbox controller and queues input events
	void ReadXDevice( xdevice_t* pXDevice );

	// Submits force feedback data to an Xbox controller
	void WriteToXDevice( xdevice_t* pXDevice );

	// Sets rumble values for an Xbox controller
	void SetXDeviceRumble( float fLeftMotor, float fRightMotor, int userId );

	// Posts an Xbox key event, ignoring key repeats 
	void PostXKeyEvent( int nUserId, xKey_t xKey, int nSample );

	// Dispatches all joystick button events through the game's window procs
	void ProcessEvent( UINT uMsg, WPARAM wParam, LPARAM lParam );

	// Initializes joysticks
	void InitializeJoysticks( void );

	// Shut down joysticks
	void ShutdownJoysticks( void );

	// Samples the joystick
	void PollJoystick( void );

	// Update the joystick button state
	void UpdateJoystickButtonState( int nJoystick );

	// Update the joystick POV control
	void UpdateJoystickPOVControl( int nJoystick );

	// Record button state and post the event
	void JoystickButtonEvent( ButtonCode_t button, int sample );

#if defined( WIN32 ) && !defined ( _X360 )
	// NVNT attaches window to novint devices
	void AttachWindowToNovintDevices( void * hWnd );

	// NVNT detaches window from novint input
	void DetachWindowFromNovintDevices( void );

	// NVNT Initializes novint devices
	void InitializeNovintDevices( void );

	// NVNT Samples a novint device
	void PollNovintDevices( void );

	// NVNT Update the novint device button state
	void UpdateNovintDeviceButtonState( int nDevice );

	// NVNT Record button state and post the event
	void NovintDeviceButtonEvent( ButtonCode_t button, int sample );

	//Added called and set to true when binding input and set to false once bound
	void SetNovintPure( bool bPure );
#else
	void SetNovintPure( bool bPure ) {} // to satify the IInput virtual interface	
#endif
	// Chains the window message to the previous wndproc
	LRESULT ChainWindowMessage( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

	// Post an event to the queue
	void PostEvent( int nType, int nTick, int nData = 0, int nData2 = 0, int nData3 = 0 );

	// Deals with app deactivation (sends a bunch of button released messages)
	void ActivateInputSystem( bool bActivated );

	// Determines all mouse button presses
	int ButtonMaskFromMouseWParam( WPARAM wParam, ButtonCode_t code = BUTTON_CODE_INVALID, bool bDown = false ) const;

	// Updates the state of all mouse buttons
	void UpdateMouseButtonState( int nButtonMask, ButtonCode_t dblClickCode = BUTTON_CODE_INVALID );

	// Copies the input state record over
	void CopyInputState( InputState_t *pDest, const InputState_t &src, bool bCopyEvents );

	// Post an button press/release event to the queue
	void PostButtonPressedEvent( InputEventType_t nType, int nTick, ButtonCode_t scanCode, ButtonCode_t virtualCode );
	void PostButtonReleasedEvent( InputEventType_t nType, int nTick, ButtonCode_t scanCode, ButtonCode_t virtualCode );

	// Release all buttons
	void ReleaseAllButtons( int nFirstButton = 0, int nLastButton = BUTTON_CODE_LAST - 1 );

	// Zero analog state
	void ZeroAnalogState( int nFirstState, int nLastState );

	// Converts xbox keys to button codes
	ButtonCode_t XKeyToButtonCode( int nUserId, int nXKey ) const;

	// Computes the sample tick
	int ComputeSampleTick();

	// Clears the input state, doesn't generate key-up messages
	void ClearInputState();

	// Called for mouse move events. Sets the current x and y and posts events for the mouse moving.
	void UpdateMousePositionState( InputState_t &state, short x, short y );

	// Initializes SteamControllers - Returns true if steam is running and finds controllers, otherwise false
	bool InitializeSteamControllers( void );

	// Returns number of connected Steam Controllers
	uint32 GetNumSteamControllersConnected();

	// Update and sample steam controllers
	void PollSteamControllers( void );

#if defined( PLATFORM_WINDOWS_PC )
	void PollInputState_Windows();
#endif

	void JoystickHotplugAdded( int joystickIndex );
	void JoystickHotplugRemoved( int joystickId );
	void JoystickButtonPress( int joystickId, int button ); // button is a SDL_CONTROLLER_BUTTON;
	void JoystickButtonRelease( int joystickId, int button ); // same as above.
	void JoystickAxisMotion( int joystickId, int axis, int value );

	// Steam Controller
	void ReadSteamController( int iIndex );
	void PostKeyEvent( int iIndex, sKey_t sKey, int nSample );
	const int GetSteamPadDeadZone( ESteamPadAxis axis );
	bool IsSteamControllerConnected( void ) { return m_bSteamController; }
	bool IsSteamControllerActive( void );
	void ActivateSteamControllerActionSetForSlot( uint64 nSlot, GameActionSet_t eActionSet );
	ControllerActionSetHandle_t GetActionSetHandle( GameActionSet_t eActionSet );
	ControllerActionSetHandle_t GetActionSetHandle( const char* szActionSet );
	bool GetControllerStateForSlot( int nSlot );
	int GetSteamControllerIndexForSlot( int nSlot );
	bool GetRadialMenuStickValues( int nSlot, float &fX, float &fY );
	virtual ISteamController* SteamControllerInterface();
	bool InitializeSteamControllerActionSets();

	// Gets the action origin (i.e. which physical input) maps to the given action name
	virtual EControllerActionOrigin GetSteamControllerActionOrigin( const char* action, GameActionSet_t action_set );
	virtual EControllerActionOrigin GetSteamControllerActionOrigin( const char* action, ControllerActionSetHandle_t action_set_handle );

	// Maps a Steam Controller action origin to a string (consisting of a single character) in our SC icon font
	virtual const wchar_t*	GetSteamControllerFontCharacterForActionOrigin( EControllerActionOrigin origin );

	// Maps a Steam Controller action origin to a short text string (e.g. "X", "LB", "LDOWN") describing the control.
	// Prefer to actually use the icon font wherever possible.
	virtual const wchar_t* GetSteamControllerDescriptionForActionOrigin( EControllerActionOrigin origin );

	// Converts SteamController keys to button codes
	ButtonCode_t SKeyToButtonCode( int nUserId, int nXKey ) const;

	// This is called with "true" by dedicated server initialization (before calling Init) in order to
	// force us to skip initialization of Steam (which messes up dedicated servers).
	virtual void SetSkipControllerInitialization( bool bSkip )
	{
		m_bSkipControllerInitialization = bSkip;
	}

#if defined( USE_SDL )
	void PollInputState_Platform();

	ILauncherMgr *m_pLauncherMgr;
#endif

	WNDPROC m_ChainedWndProc;
	HWND m_hAttachedHWnd;
	bool m_bEnabled;
	bool m_bPumpEnabled;
	bool m_bIsPolling;

	// Current button state
	InputState_t m_InputState[INPUT_STATE_COUNT];

	// Current action set
	GameActionSet_t m_currentActionSet[STEAM_CONTROLLER_MAX_COUNT];

	DWORD m_StartupTimeTick;
	int m_nLastPollTick;
	int m_nLastSampleTick;
	int m_nPollCount;

	// Mouse wheel hack
	UINT m_uiMouseWheel;

	// Joystick info
	CUtlFlags<unsigned short> m_JoysticksEnabled;
	int m_nJoystickCount;
	bool m_bJoystickInitialized;
	bool m_bXController;
	JoystickInfo_t m_pJoystickInfo[ MAX_JOYSTICKS ];

	// Steam Controller
	struct steampad_t
	{
		steampad_t()
		{
			m_nHardwareIndex = 0;
			m_nJoystickIndex = INVALID_USER_ID;
			m_nLastPacketIndex = 0;
			active = false;
			memset( lastAnalogKeys, 0, sizeof( lastAnalogKeys ) );
		}
		bool				active;

		sKey_t				lastAnalogKeys[MAX_STEAMPADAXIS];
		appKey_t			m_appSKeys[SK_MAX_KEYS];
		// Hardware index and joystick index don't necessarily match
		// Joystick index will depend on the order of multiple initialized devices
		// Which could include other controller types
		// Hardware index should line up 1:1 with the order they're polled
		// and could change based on removing devices, unlike Joystick Index
		uint32				m_nHardwareIndex;
		int					m_nJoystickIndex;
		uint32				m_nLastPacketIndex;
	};

	float m_pRadialMenuStickVal[STEAM_CONTROLLER_MAX_COUNT][2];
	steampad_t m_Device[STEAM_CONTROLLER_MAX_COUNT];
	uint32 m_unNumConnected;
	float m_flLastSteamControllerInput;
	int m_nJoystickBaseline;
	int m_nControllerType[MAX_JOYSTICKS+STEAM_CONTROLLER_MAX_COUNT];

	bool m_bSteamController;						// true if the Steam Controller system has been initialized successfully (this doesn't mean one is actually connected necessarily)
	bool m_bSteamControllerActionsInitialized;		// true if our action sets and handles were successfully initialized (this doesn't mean a controller is necessarily connected, or that in-game client actions were initialized)
	bool m_bSteamControllerActive;					// true if our action sets and handles were successfully initialized *and* that at least one controller is actually connected and switched on.

#if defined( WIN32 ) && !defined ( _X360 )
	// NVNT Novint device info
	int m_nNovintDeviceCount;
	bool m_bNovintDevices;
#endif

	// Xbox controller info
	appKey_t	m_appXKeys[ XUSER_MAX_COUNT ][ XK_MAX_KEYS ];
	xdevice_t	m_XDevices[ XUSER_MAX_COUNT ];
	int			m_PrimaryUserId;

	// raw mouse input
	bool m_bRawInputSupported;
	int	 m_mouseRawAccumX, m_mouseRawAccumY;

	// For the 'SleepUntilInput' feature
	HANDLE m_hEvent;

	CSysModule   *m_pXInputDLL;
	CSysModule   *m_pRawInputDLL;
	
#if defined( WIN32 ) && !defined ( _X360 )
	// NVNT falcon module
	CSysModule	 *m_pNovintDLL;
#endif

	bool m_bConsoleTextMode;

public:
	// Steam API context for use by input system for access to steam controllers.
	CSteamAPIContext& SteamAPIContext() { return m_SteamAPIContext; }

private:
	CSteamAPIContext m_SteamAPIContext;
	bool m_bSkipControllerInitialization;
};

#endif // INPUTSYSTEM_H
