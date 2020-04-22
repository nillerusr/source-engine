//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Linux Joystick implementation for inputsystem.dll
//
//===========================================================================//

/* For force feedback testing. */
#include "inputsystem.h"
#include "tier1/convar.h"
#include "tier0/icommandline.h"

#include "SDL.h"
#include "SDL_gamecontroller.h"
#include "SDL_haptic.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

static ButtonCode_t ControllerButtonToButtonCode( SDL_GameControllerButton button );
static AnalogCode_t ControllerAxisToAnalogCode( SDL_GameControllerAxis axis );
static int JoystickSDLWatcher( void *userInfo, SDL_Event *event );

ConVar joy_axisbutton_threshold( "joy_axisbutton_threshold", "0.3", FCVAR_ARCHIVE, "Analog axis range before a button press is registered." );
ConVar joy_axis_deadzone( "joy_axis_deadzone", "0.2", FCVAR_ARCHIVE, "Dead zone near the zero point to not report movement." );

static void joy_active_changed_f( IConVar *var, const char *pOldValue, float flOldValue );
ConVar joy_active( "joy_active", "-1", FCVAR_NONE, "Which of the connected joysticks / gamepads to use (-1 means first found)", &joy_active_changed_f);

static void joy_gamecontroller_config_changed_f( IConVar *var, const char *pOldValue, float flOldValue );
ConVar joy_gamecontroller_config( "joy_gamecontroller_config", "", FCVAR_ARCHIVE, "Game controller mapping (passed to SDL with SDL_HINT_GAMECONTROLLERCONFIG), can also be configured in Steam Big Picture mode.", &joy_gamecontroller_config_changed_f );

void SearchForDevice()
{
	int newJoystickId = joy_active.GetInt();
	CInputSystem *pInputSystem = (CInputSystem *)g_pInputSystem;

	if ( !pInputSystem )
	{
		return;
	}
	// -1 means "first available."
	if ( newJoystickId < 0 )
	{
		pInputSystem->JoystickHotplugAdded(0);
		return;
	}

	for ( int device_index = 0; device_index < SDL_NumJoysticks(); ++device_index )
	{
		SDL_Joystick *joystick = SDL_JoystickOpen(device_index);
		if ( joystick == NULL )
		{
			continue;
		}

		int joystickId = SDL_JoystickInstanceID(joystick);
		SDL_JoystickClose(joystick);

		if ( joystickId == newJoystickId )
		{
			pInputSystem->JoystickHotplugAdded(device_index);
			break;
		}
	}
}

//---------------------------------------------------------------------------------------
// Switch our active joystick to another device
//---------------------------------------------------------------------------------------
void joy_active_changed_f( IConVar *var, const char *pOldValue, float flOldValue )
{
	SearchForDevice();
}

//---------------------------------------------------------------------------------------
// Reinitialize the game controller layer when the joy_gamecontroller_config is updated.
//---------------------------------------------------------------------------------------
void joy_gamecontroller_config_changed_f( IConVar *var, const char *pOldValue, float flOldValue )
{
	CInputSystem *pInputSystem = (CInputSystem *)g_pInputSystem;
	if ( pInputSystem && SDL_WasInit(SDL_INIT_GAMECONTROLLER) )
	{
		bool oldValuePresent = pOldValue && ( strlen( pOldValue ) > 0 );
		bool newValuePresent = ( strlen( joy_gamecontroller_config.GetString() ) > 0 );
		if ( !oldValuePresent && !newValuePresent )
		{
			return;
		}

		// We need to reinitialize the whole thing (i.e. undo CInputSystem::InitializeJoysticks and then call it again)
		// due to SDL_GameController only reading the SDL_HINT_GAMECONTROLLERCONFIG on init.
		pInputSystem->ShutdownJoysticks();
		pInputSystem->InitializeJoysticks();
	}
}

//-----------------------------------------------------------------------------
// Handle the events coming from the GameController SDL subsystem.
//-----------------------------------------------------------------------------
int JoystickSDLWatcher( void *userInfo, SDL_Event *event )
{
	CInputSystem *pInputSystem = (CInputSystem *)userInfo;
	Assert(pInputSystem != NULL);
	Assert(event != NULL);

	if ( event == NULL || pInputSystem == NULL )
	{
		Warning("No input system\n");
		return 1;
	}

	switch ( event->type )
	{
		case SDL_CONTROLLERAXISMOTION:
		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
		case SDL_CONTROLLERDEVICEADDED:
		case SDL_CONTROLLERDEVICEREMOVED:
			break;
		default:
			return 1;
	}

	// This is executed on the same thread as SDL_PollEvent, as PollEvent
	// updates the joystick subsystem, which then calls SDL_PushEvent for
	// the various events below.  PushEvent invokes this callback.
	// SDL_PollEvent is called in PumpWindowsMessageLoop which is coming
	// from PollInputState_Linux, so there's no worry about calling
	// PostEvent (which doesn't seem to be thread safe) from other threads.
	Assert(ThreadInMainThread());

	switch ( event->type )
	{
		case SDL_CONTROLLERAXISMOTION:
		{
			pInputSystem->JoystickAxisMotion(event->caxis.which, event->caxis.axis, event->caxis.value);
			break;
		}

		case SDL_CONTROLLERBUTTONDOWN:
			pInputSystem->JoystickButtonPress(event->cbutton.which, event->cbutton.button);
			break;
		case SDL_CONTROLLERBUTTONUP:
			pInputSystem->JoystickButtonRelease(event->cbutton.which, event->cbutton.button);
			break;

		case SDL_CONTROLLERDEVICEADDED:
			pInputSystem->JoystickHotplugAdded(event->cdevice.which);
			break;
		case SDL_CONTROLLERDEVICEREMOVED:
			pInputSystem->JoystickHotplugRemoved(event->cdevice.which);
			SearchForDevice();
			break;
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Initialize all joysticks
//-----------------------------------------------------------------------------
void CInputSystem::InitializeJoysticks( void )
{
	if ( m_bJoystickInitialized )
	{
		ShutdownJoysticks();
	}

	// assume no joystick
	m_nJoystickCount = 0;
	memset( m_pJoystickInfo, 0, sizeof( m_pJoystickInfo ) );
	for ( int i = 0; i < MAX_JOYSTICKS; ++i )
	{
		m_pJoystickInfo[ i ].m_nDeviceId = -1;
	}

	// abort startup if user requests no joystick
	if ( CommandLine()->FindParm("-nojoy") ) return;

	const char *controllerConfig = joy_gamecontroller_config.GetString();
	if ( strlen(controllerConfig) > 0 )
	{
		DevMsg("Passing joy_gamecontroller_config to SDL ('%s').\n", controllerConfig);
		// We need to pass this hint to SDL *before* we init the gamecontroller subsystem, otherwise it gets ignored.
		SDL_SetHint(SDL_HINT_GAMECONTROLLERCONFIG, controllerConfig);
	}

	if ( SDL_InitSubSystem( SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC ) == -1 )
	{
		Warning("Joystick init failed -- SDL_Init(SDL_INIT_GAMECONTROLLER|SDL_INIT_HAPTIC) failed: %s.\n", SDL_GetError());
		return;
	}

	m_bJoystickInitialized = true;

	SDL_AddEventWatch(JoystickSDLWatcher, this);

	const int totalSticks = SDL_NumJoysticks();
	for ( int i = 0; i < totalSticks; i++ )
	{
		if ( SDL_IsGameController(i) )
		{
			JoystickHotplugAdded(i);
		} 
		else
		{
			SDL_JoystickGUID joyGUID = SDL_JoystickGetDeviceGUID(i);
			char szGUID[sizeof(joyGUID.data)*2 + 1];
			SDL_JoystickGetGUIDString(joyGUID, szGUID, sizeof(szGUID));

			Msg("Found joystick '%s' (%s), but no recognized controller configuration for it.\n", SDL_JoystickNameForIndex(i), szGUID);
		}
	}

	if ( totalSticks < 1 )
	{
		Msg("Did not detect any valid joysticks.\n");
	}
}

void CInputSystem::ShutdownJoysticks()
{
	if ( !m_bJoystickInitialized )
	{
		return;
	}

	SDL_DelEventWatch( JoystickSDLWatcher, this );
	if ( m_pJoystickInfo[ 0 ].m_pDevice != NULL )
	{
		JoystickHotplugRemoved( m_pJoystickInfo[ 0 ].m_nDeviceId );
	}
	SDL_QuitSubSystem( SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC );

	m_bJoystickInitialized = false;
}

// Update the joy_xcontroller_found convar to force CInput::JoyStickMove to re-exec 360controller-linux.cfg
static void SetJoyXControllerFound( bool found )
{
	static ConVarRef xcontrollerVar( "joy_xcontroller_found" );
	static ConVarRef joystickVar( "joystick" );
	if ( xcontrollerVar.IsValid() )
	{
		xcontrollerVar.SetValue(found);
	}

	if ( found && joystickVar.IsValid() )
	{
		joystickVar.SetValue(true);
	}
}

void CInputSystem::JoystickHotplugAdded( int joystickIndex )
{
	// SDL_IsGameController doesn't bounds check its inputs.
	if ( joystickIndex < 0 || joystickIndex >= SDL_NumJoysticks() )
	{
		return;
	}

	if ( !SDL_IsGameController(joystickIndex) )
	{
		Warning("Joystick is not recognized by the game controller system. You can configure the controller in Steam Big Picture mode.\n");
		return;
	}

	SDL_Joystick *joystick = SDL_JoystickOpen(joystickIndex);
	if ( joystick == NULL )
	{
		Warning("Could not open joystick %i: %s", joystickIndex, SDL_GetError());
		return;
	}

	int joystickId = SDL_JoystickInstanceID(joystick);
	SDL_JoystickClose(joystick);

	int activeJoystick = joy_active.GetInt();
	JoystickInfo_t& info = m_pJoystickInfo[ 0 ];
	if ( activeJoystick < 0 )
	{
		// Only opportunistically open devices if we don't have one open already.
		if ( info.m_nDeviceId != -1 )
		{
			Msg("Detected supported joystick #%i '%s'. Currently active joystick is #%i.\n", joystickId, SDL_JoystickNameForIndex(joystickIndex), info.m_nDeviceId);
			return;
		}
	}
	else if ( activeJoystick != joystickId )
	{
		Msg("Detected supported joystick #%i '%s'. Currently active joystick is #%i.\n", joystickId, SDL_JoystickNameForIndex(joystickIndex), activeJoystick);
		return;
	}

	if ( info.m_nDeviceId != -1 )
	{
		// Don't try to open the device we already have open.
		if ( info.m_nDeviceId == joystickId )
		{
			return;
		}

		DevMsg("Joystick #%i already initialized, removing it first.\n", info.m_nDeviceId);
		JoystickHotplugRemoved(info.m_nDeviceId);
	}

	Msg("Initializing joystick #%i and making it active.\n", joystickId);

	SDL_GameController *controller = SDL_GameControllerOpen(joystickIndex);
	if ( controller == NULL )
	{
		Warning("Failed to open joystick %i: %s\n", joystickId, SDL_GetError());
		return;
	}

	// XXX: This will fail if this is a *real* hotplug event (and not coming from the initial InitializeJoysticks call).
	// That's because the SDL haptic subsystem currently doesn't do hotplugging. Everything but haptics will work fine.
	SDL_Haptic *haptic = SDL_HapticOpenFromJoystick(SDL_GameControllerGetJoystick(controller));
	if ( haptic == NULL || SDL_HapticRumbleInit(haptic) != 0 )
	{
		Warning("Unable to initialize rumble for joystick #%i: %s\n", joystickId, SDL_GetError());
		haptic = NULL;
	}

	info.m_pDevice = controller;
	info.m_pHaptic = haptic;
	info.m_nDeviceId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
	info.m_nButtonCount = SDL_CONTROLLER_BUTTON_MAX;
	info.m_bRumbleEnabled = false;

	SetJoyXControllerFound(true);
	EnableJoystickInput(0, true);
	m_nJoystickCount = 1;
	m_bXController =  true;

	// We reset joy_active to -1 because joystick ids are never reused - until you restart.
	// Setting it to -1 means that you get expected hotplugging behavior if you disconnect the current joystick.
	joy_active.SetValue(-1);
}

void CInputSystem::JoystickHotplugRemoved( int joystickId )
{
	JoystickInfo_t& info = m_pJoystickInfo[ 0 ];
	if ( info.m_nDeviceId != joystickId )
	{
		DevMsg("Ignoring hotplug remove for #%i, active joystick is #%i.\n", joystickId, info.m_nDeviceId);
		return;
	}

	if ( info.m_pDevice == NULL )
	{
		info.m_nDeviceId = -1;
		DevMsg("Got hotplug remove event for removed joystick #%i, ignoring.\n", joystickId);
		return;
	}

	m_nJoystickCount = 0;
	m_bXController =  false;
	EnableJoystickInput(0, false);
	SetJoyXControllerFound(false);

	SDL_HapticClose((SDL_Haptic *)info.m_pHaptic);
	SDL_GameControllerClose((SDL_GameController *)info.m_pDevice);

	info.m_pHaptic = NULL;
	info.m_pDevice = NULL;
	info.m_nButtonCount = 0;
	info.m_nDeviceId = -1;
	info.m_bRumbleEnabled = false;

	Msg("Joystick %i removed.\n", joystickId);
}

void CInputSystem::JoystickButtonPress( int joystickId, int button )
{
	JoystickInfo_t& info = m_pJoystickInfo[ 0 ];
	if ( info.m_nDeviceId != joystickId )
	{
		Warning("Not active device input system (%i x %i)\n", info.m_nDeviceId, joystickId);
		return;
	}

	ButtonCode_t buttonCode = ControllerButtonToButtonCode((SDL_GameControllerButton)button);
	PostButtonPressedEvent(IE_ButtonPressed, m_nLastSampleTick, buttonCode, buttonCode);
}

void CInputSystem::JoystickButtonRelease( int joystickId, int button )
{
	JoystickInfo_t& info = m_pJoystickInfo[ 0 ];
	if ( info.m_nDeviceId != joystickId )
	{
		return;
	}

	ButtonCode_t buttonCode = ControllerButtonToButtonCode((SDL_GameControllerButton)button);
	PostButtonReleasedEvent(IE_ButtonReleased, m_nLastSampleTick, buttonCode, buttonCode);
}


void CInputSystem::JoystickAxisMotion( int joystickId, int axis, int value )
{
	JoystickInfo_t& info = m_pJoystickInfo[ 0 ];
	if ( info.m_nDeviceId != joystickId )
	{
		return;
	}

	AnalogCode_t code = ControllerAxisToAnalogCode((SDL_GameControllerAxis)axis);
	if ( code == ANALOG_CODE_INVALID )
	{
		Warning("Invalid code for axis %i\n", axis);
		return;
	}

	ButtonCode_t buttonCode = BUTTON_CODE_NONE;
	switch ( axis )
	{
		case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
			buttonCode = KEY_XBUTTON_RTRIGGER;
			break;
		case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
			buttonCode = KEY_XBUTTON_LTRIGGER;
			break;
	}

	if ( buttonCode != BUTTON_CODE_NONE )
	{
		int pressThreshold = joy_axisbutton_threshold.GetFloat() * 32767;
		int keyIndex = buttonCode - KEY_XBUTTON_LTRIGGER;
		Assert( keyIndex < ARRAYSIZE( m_appXKeys[0] ) && keyIndex >= 0 );

		appKey_t &key = m_appXKeys[0][keyIndex];
		if ( value > pressThreshold )
		{
			if ( key.repeats < 1 )
			{
				PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, buttonCode, buttonCode );
			}
			key.repeats++;
		}
		else
		{
			PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, buttonCode, buttonCode );
			key.repeats = 0;
		}
	}

	int minValue = joy_axis_deadzone.GetFloat() * 32767;
	if ( abs(value) < minValue )
	{
		value = 0;
	}

	InputState_t& state = m_InputState[ m_bIsPolling ];
	state.m_pAnalogDelta[ code ] = value - state.m_pAnalogValue[ code ];
	state.m_pAnalogValue[ code ] = value;
	if ( state.m_pAnalogDelta[ code ] != 0 )
	{
		PostEvent(IE_AnalogValueChanged, m_nLastSampleTick, code, value, 0);
	}
}

//-----------------------------------------------------------------------------
//	Process the event
//-----------------------------------------------------------------------------
void CInputSystem::JoystickButtonEvent( ButtonCode_t button, int sample )
{
	// Not used - we post button events from JoystickButtonPress/Release.
}


//-----------------------------------------------------------------------------
// Update the joystick button state
//-----------------------------------------------------------------------------
void CInputSystem::UpdateJoystickButtonState( int nJoystick )
{
	// We don't sample - we get events posted by SDL_GameController in JoystickSDLWatcher
}


//-----------------------------------------------------------------------------
// Update the joystick POV control
//-----------------------------------------------------------------------------
void CInputSystem::UpdateJoystickPOVControl( int nJoystick )
{
	// SDL GameController does not support joystick POV. Should we poll?
}


//-----------------------------------------------------------------------------
// Purpose: Sample the joystick
//-----------------------------------------------------------------------------
void CInputSystem::PollJoystick( void )
{
	// We only pump the SDL event loop if we're not an SDL app, since otherwise PollInputState_Platform calls into CSDLMgr to pump it.
	// Our state updates happen in events posted by SDL_GameController in JoystickSDLWatcher, so the loop is empty.
#if !defined( USE_SDL )
	SDL_Event event;
	int nEventsProcessed = 0;

	SDL_PumpEvents();
	while ( SDL_PollEvent( &event ) && nEventsProcessed < 100 )
	{
		nEventsProcessed++;
	}
#endif
}

void CInputSystem::SetXDeviceRumble( float fLeftMotor, float fRightMotor, int userId )
{
	JoystickInfo_t& info = m_pJoystickInfo[ 0 ];
	if ( info.m_nDeviceId < 0  || info.m_pHaptic == NULL )
	{
		return;
	}

	float strength = (fLeftMotor + fRightMotor) / 2.f;
	static ConVarRef joystickVar( "joystick" );

	// 0f means "stop".
	bool shouldStop = ( strength < 0.01f );
	// If they've disabled the gamecontroller in settings, never rumble.
	if ( !joystickVar.IsValid() || !joystickVar.GetBool() )
	{
		shouldStop = true;
	}

	if ( shouldStop )
	{
		if ( info.m_bRumbleEnabled )
		{
			SDL_HapticRumbleStop( (SDL_Haptic *)info.m_pHaptic );
			info.m_bRumbleEnabled = false;
			info.m_fCurrentRumble = 0.0f;
		}

		return;
	}

	// If there's little change, then don't change the rumble strength.
	if ( info.m_bRumbleEnabled && abs(info.m_fCurrentRumble - strength) < 0.01f )
	{
		return;
	}

	info.m_bRumbleEnabled = true;
	info.m_fCurrentRumble = strength;

	if ( SDL_HapticRumblePlay((SDL_Haptic *)info.m_pHaptic, strength, SDL_HAPTIC_INFINITY) != 0 )
	{
		Warning("Couldn't play rumble (strength %.1f): %s\n", strength, SDL_GetError());
	}
}

ButtonCode_t ControllerButtonToButtonCode( SDL_GameControllerButton button )
{
	switch ( button )
	{
		case SDL_CONTROLLER_BUTTON_A: // KEY_XBUTTON_A
		case SDL_CONTROLLER_BUTTON_B: // KEY_XBUTTON_B
		case SDL_CONTROLLER_BUTTON_X: // KEY_XBUTTON_X
		case SDL_CONTROLLER_BUTTON_Y: // KEY_XBUTTON_Y
			return JOYSTICK_BUTTON(0, button);

		case SDL_CONTROLLER_BUTTON_BACK:
			return KEY_XBUTTON_BACK;
		case SDL_CONTROLLER_BUTTON_START:
			return KEY_XBUTTON_START;

		case SDL_CONTROLLER_BUTTON_GUIDE:
			return KEY_XBUTTON_BACK; // XXX: How are we supposed to handle this? Steam overlay etc.

		case SDL_CONTROLLER_BUTTON_LEFTSTICK:
			return KEY_XBUTTON_STICK1;
		case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
			return KEY_XBUTTON_STICK2;
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
			return KEY_XBUTTON_LEFT_SHOULDER;
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
			return KEY_XBUTTON_RIGHT_SHOULDER;

		case SDL_CONTROLLER_BUTTON_DPAD_UP:
			return KEY_XBUTTON_UP;
		case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
			return KEY_XBUTTON_DOWN;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
			return KEY_XBUTTON_LEFT;
		case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
			return KEY_XBUTTON_RIGHT;
	}

	return BUTTON_CODE_NONE;
}

AnalogCode_t ControllerAxisToAnalogCode( SDL_GameControllerAxis axis )
{
	switch ( axis )
	{
		case SDL_CONTROLLER_AXIS_LEFTX:
			return JOYSTICK_AXIS(0, JOY_AXIS_X);
		case SDL_CONTROLLER_AXIS_LEFTY:
			return JOYSTICK_AXIS(0, JOY_AXIS_Y);

		case SDL_CONTROLLER_AXIS_RIGHTX:
			return JOYSTICK_AXIS(0, JOY_AXIS_U);
		case SDL_CONTROLLER_AXIS_RIGHTY:
			return JOYSTICK_AXIS(0, JOY_AXIS_R);

		case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
		case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
			return JOYSTICK_AXIS(0, JOY_AXIS_Z);
	}

	return ANALOG_CODE_INVALID;
}
