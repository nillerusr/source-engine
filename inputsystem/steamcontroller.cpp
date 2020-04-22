//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Native Steam Controller Interface
//=============================================================================//
#include "inputsystem.h"
#include "key_translation.h"
#include "filesystem.h"
#include "steam/isteamcontroller.h"
#include "math.h"

#ifndef UINT64_MAX
const uint64 UINT64_MAX = 0xffffffffffffffff;
#endif

#if !defined( NO_STEAM )
#include "steam/steam_api.h"
#endif //NO_STEAM

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ConVar sc_joystick_map( "sc_joystick_map", "1", FCVAR_ARCHIVE, "How to map the analog joystick deadzone and extents 0 = Scaled Cross, 1 = Concentric Mapping to Square." );

#define STEAMPAD_MAX_ANALOGSAMPLE_GYRO		32768
#define STEAMPAD_MAX_ANALOGSAMPLE_TRIGGER	32768
#define STEAMPAD_MAX_ANALOGSAMPLE_LEFT		32768
#define STEAMPAD_MAX_ANALOGSAMPLE_RIGHT		32767
#define STEAMPAD_MAX_ANALOGSAMPLE_DOWN		32768
#define STEAMPAD_MAX_ANALOGSAMPLE_UP		32767
#define STEAMPAD_MAX_ANALOGSAMPLE_MAX		32768
#define STEAMPAD_ANALOG_SCALE_LEFT(x) 		( ( float )STEAMPAD_MAX_ANALOGSAMPLE_LEFT/( float )( STEAMPAD_MAX_ANALOGSAMPLE_LEFT-(x) ) )
#define STEAMPAD_ANALOG_SCALE_RIGHT(x) 		( ( float )STEAMPAD_MAX_ANALOGSAMPLE_RIGHT/( float )( STEAMPAD_MAX_ANALOGSAMPLE_RIGHT-(x) ) )
#define STEAMPAD_ANALOG_SCALE_DOWN(x) 		( ( float )STEAMPAD_MAX_ANALOGSAMPLE_DOWN/( float )( STEAMPAD_MAX_ANALOGSAMPLE_DOWN-(x) ) )
#define STEAMPAD_ANALOG_SCALE_UP(x)	 		( ( float )STEAMPAD_MAX_ANALOGSAMPLE_UP/( float )( STEAMPAD_MAX_ANALOGSAMPLE_UP-(x) ) )


#define STEAMPAD_ANALOG_TRIGGER_THRESHOLD	( ( int )( STEAMPAD_MAX_ANALOGSAMPLE_TRIGGER * 0.5f ) )
#define STEAMPAD_ANALOG_PAD_THRESHOLD		( ( int )( STEAMPAD_MAX_ANALOGSAMPLE_MAX * 0.25f ) )
#define STEAMPAD_ANALOG_GYRO_THRESHOLD		( ( int ) ( STEAMPAD_MAX_ANALOGSAMPLE_GYRO * 0.3f ) )
#define STEAMPAD_DIGITAL_PAD_THRESHOLD		( ( int )( STEAMPAD_MAX_ANALOGSAMPLE_MAX * 0.52f ) )

#define STEAMPAD_AXIS_REPEAT_INTERVAL_START	0.6f
#define STEAMPAD_AXIS_REPEAT_INTERVAL_END	0.05f
#define STEAMPAD_AXIS_REPEAT_CURVE_TIME		1.75f

#define PAD_ANALOG_BUTTON_THRESHOLD			( ( int )( STEAMPAD_MAX_ANALOGSAMPLE_MAX * 0.285f ) )
#define PAD_ANALOG_BUTTON_THRESHOLD_STRONG	( ( int )( STEAMPAD_MAX_ANALOGSAMPLE_MAX * 0.68f ) )

#define SQRT2 1.414213562

// key translation
typedef struct
{
	uint64			steampadinput;
	ButtonCode_t	steampadkey;
} steampadInputTosteampadKey_t;

static const int s_nSteamPadDeadZoneTable[] =
{
	STEAMPAD_ANALOG_PAD_THRESHOLD,		// LEFTPAD_AXIS_X
	STEAMPAD_ANALOG_PAD_THRESHOLD,		// LEFTPAD_AXIS_Y
	STEAMPAD_ANALOG_PAD_THRESHOLD,		// RIGHTPAD_AXIS_X
	STEAMPAD_ANALOG_PAD_THRESHOLD,		// RIGHTPAD_AXIS_Y
	STEAMPAD_ANALOG_TRIGGER_THRESHOLD,  //LEFT_TRIGGER_AXIS
	STEAMPAD_ANALOG_TRIGGER_THRESHOLD,  //RIGHT_TRIGGER_AXIS
	STEAMPAD_ANALOG_GYRO_THRESHOLD,		//GYRO_AXIS_PITCH
	STEAMPAD_ANALOG_GYRO_THRESHOLD,		//GYRO_AXIS_ROLL
	STEAMPAD_ANALOG_GYRO_THRESHOLD,		//GYRO_AXIS_YAW
};

//-----------------------------------------------------------------------------
// Purpose: Counts the number of active gamepads connected
//-----------------------------------------------------------------------------
uint32 CInputSystem::GetNumSteamControllersConnected()
{
	return m_unNumConnected;
}

struct SDigitalMenuAction
{
	const char *strName;
	ButtonCode_t buttonCode;
	ControllerDigitalActionHandle_t handle;
	bool bState[STEAM_CONTROLLER_MAX_COUNT];
	bool bAwaitingDebounce[STEAM_CONTROLLER_MAX_COUNT];
};

static SDigitalMenuAction g_DigitalMenuActions[] = {
	{ "menu_left", STEAMCONTROLLER_DPAD_LEFT, 0, { false }, { false } },
	{ "menu_right", STEAMCONTROLLER_DPAD_RIGHT, 0, { false }, { false } },
	{ "menu_up", STEAMCONTROLLER_DPAD_UP, 0, { false }, { false } },
	{ "menu_down", STEAMCONTROLLER_DPAD_DOWN, 0, { false }, { false } },
	{ "menu_cancel", STEAMCONTROLLER_B, 0, { false }, { false } },
	{ "menu_select", STEAMCONTROLLER_A, 0, { false }, { false } },
	{ "resume_esc", STEAMCONTROLLER_START, 0, { false }, { false } },
	{ "cl_trigger_first_notification", STEAMCONTROLLER_F1, 0, { false }, { false } },		// Command is in the in-game action set (use for hitting accept/ok on dialogs)
	{ "cl_decline_first_notification", STEAMCONTROLLER_F2, 0, { false }, { false } },		// Command is in the in-game action set (use for hitting cancel/decline on dialogs)
	{ "menu_toggle_function", STEAMCONTROLLER_Y, 0, { false }, { false }, },				// Command is in the in-game HUD action set
	{ "menu_alt_function", STEAMCONTROLLER_X, 0, { false }, { false } },					// Command is in the in-game HUD action set
	{ "cancelselect", STEAMCONTROLLER_START, 0, { false }, { false } },						// Command is in the in-game action set
	{ "toggleready", STEAMCONTROLLER_F4, 0, { false }, { false } },							// Command is in the in-game action set
};

struct SAnalogAction
{
	const char *strName;
	JoystickAxis_t joystickAxisX, joystickAxisY;
	ControllerAnalogActionHandle_t handle;
};

struct SGameActionSet
{
	const char *strName;
	GameActionSet_t eGameActionSet;
	ControllerActionSetHandle_t handle;
};
static SGameActionSet g_GameActionSets[] = {
	{ "MenuControls", GAME_ACTION_SET_MENUCONTROLS, 0 },
	{ "FPSControls", GAME_ACTION_SET_FPSCONTROLS, 0 },
	{ "InGameHUDControls", GAME_ACTION_SET_IN_GAME_HUD, 0 },
	{ "SpectatorControls", GAME_ACTION_SET_SPECTATOR, 0 },
};


// Table that maps a physical steam controller origin to the corresponding character in our icon font.
// If the EControllerActionOrigin enum or the font changes, this table must be changed to match.
static const wchar_t* g_MapSteamControllerOriginToIconFont[] = {
	L"",					// k_EControllerActionOrigin_None 
	L"A",					// k_EControllerActionOrigin_A
	L"B",					// k_EControllerActionOrigin_B
	L"X",					// k_EControllerActionOrigin_X
	L"Y",					// k_EControllerActionOrigin_Y
	L"2",					// k_EControllerActionOrigin_LeftBumper
	L"3",					// k_EControllerActionOrigin_RightBumper
	L"(",					// k_EControllerActionOrigin_LeftGrip
	L")",					// k_EControllerActionOrigin_RightGrip
	L"5",					// k_EControllerActionOrigin_Start
	L"4",					// k_EControllerActionOrigin_Back
	L"q",					// k_EControllerActionOrigin_LeftPad_Touch
	L"w",					// k_EControllerActionOrigin_LeftPad_Swipe
	L"e",					// k_EControllerActionOrigin_LeftPad_Click
	L"a",					// k_EControllerActionOrigin_LeftPad_DPadNorth
	L"s",					// k_EControllerActionOrigin_LeftPad_DPadSouth
	L"d",					// k_EControllerActionOrigin_LeftPad_DPadWest
	L"f",					// k_EControllerActionOrigin_LeftPad_DPadEast					
	L"y",					// k_EControllerActionOrigin_RightPad_Touch
	L"u",					// k_EControllerActionOrigin_RightPad_Swipe
	L"i",					// k_EControllerActionOrigin_RightPad_Click
	L"h",					// k_EControllerActionOrigin_RightPad_DPadNorth
	L"j",					// k_EControllerActionOrigin_RightPad_DPadSouth
	L"k",					// k_EControllerActionOrigin_RightPad_DPadWest
	L"l",					// k_EControllerActionOrigin_RightPad_DEast
	L"z",					// k_EControllerActionOrigin_LeftTrigger_Pull
	L"x",					// k_EControllerActionOrigin_LeftTrigger_Click
	L"n",					// k_EControllerActionOrigin_RightTrigger_Pull
	L"m",					// k_EControllerActionOrigin_RightTrigger_Click
	L"C",					// k_EControllerActionOrigin_LeftStick_Move
	L"V",					// k_EControllerActionOrigin_LeftStick_Click
	L"7",					// k_EControllerActionOrigin_LeftStick_DPadNorth
	L"8",					// k_EControllerActionOrigin_LeftStick_DPadSouth
	L"9",					// k_EControllerActionOrigin_LeftStick_DPadWest
	L"0",					// k_EControllerActionOrigin_LeftStick_DPadEast
	L"6",					// k_EControllerActionOrigin_Gyro_Move
	L"6",					// k_EControllerActionOrigin_Gyro_Pitch
	L"6",					// k_EControllerActionOrigin_Gyro_Yaw
	L"6",					// k_EControllerActionOrigin_Gyro_Roll
};

// Table that maps a physical steam controller origin to a short description string for user display (only use this
// if it's impractical to use the icon font).
// If the EControllerActionOrigin enum changes, this table must be changed to match.
static const wchar_t* g_MapSteamControllerOriginToDescription[] = {
	L"",					// k_EControllerActionOrigin_None 
	L"A",					// k_EControllerActionOrigin_A
	L"B",					// k_EControllerActionOrigin_B
	L"X",					// k_EControllerActionOrigin_X
	L"Y",					// k_EControllerActionOrigin_Y
	L"LB",					// k_EControllerActionOrigin_LeftBumper
	L"RB",					// k_EControllerActionOrigin_RightBumper
	L"LG",					// k_EControllerActionOrigin_LeftGrip
	L"RG",					// k_EControllerActionOrigin_RightGrip
	L"START",				// k_EControllerActionOrigin_Start
	L"BACK",				// k_EControllerActionOrigin_Back
	L"LPTOUCH",				// k_EControllerActionOrigin_LeftPad_Touch
	L"LPSWIPE",				// k_EControllerActionOrigin_LeftPad_Swipe
	L"LPCLICK",				// k_EControllerActionOrigin_LeftPad_Click
	L"LPUP",				// k_EControllerActionOrigin_LeftPad_DPadNorth
	L"LPDOWN",				// k_EControllerActionOrigin_LeftPad_DPadSouth
	L"LPLEFT",				// k_EControllerActionOrigin_LeftPad_DPadWest
	L"LPRIGHT",				// k_EControllerActionOrigin_LeftPad_DPadEast					
	L"RPTOUCH",				// k_EControllerActionOrigin_RightPad_Touch
	L"RPSWIPE",				// k_EControllerActionOrigin_RightPad_Swipe
	L"RPCLICK",				// k_EControllerActionOrigin_RightPad_Click
	L"RPUP",				// k_EControllerActionOrigin_RightPad_DPadNorth
	L"RPDOWN",				// k_EControllerActionOrigin_RightPad_DPadSouth
	L"RPLEFT",				// k_EControllerActionOrigin_RightPad_DPadWest
	L"RPRIGHT",				// k_EControllerActionOrigin_RightPad_DEast
	L"LT",					// k_EControllerActionOrigin_LeftTrigger_Pull
	L"LT",					// k_EControllerActionOrigin_LeftTrigger_Click
	L"LT",					// k_EControllerActionOrigin_RightTrigger_Pull
	L"LT",					// k_EControllerActionOrigin_RightTrigger_Click
	L"LS",					// k_EControllerActionOrigin_LeftStick_Move
	L"LSCLICK",				// k_EControllerActionOrigin_LeftStick_Click
	L"LSUP",				// k_EControllerActionOrigin_LeftStick_DPadNorth
	L"LSDOWN",				// k_EControllerActionOrigin_LeftStick_DPadSouth
	L"LSLEFT",				// k_EControllerActionOrigin_LeftStick_DPadWest
	L"LSRIGHT",				// k_EControllerActionOrigin_LeftStick_DPadEast
	L"GYRO",				// k_EControllerActionOrigin_Gyro_Move
	L"GYRO",				// k_EControllerActionOrigin_Gyro_Pitch
	L"GYRO",				// k_EControllerActionOrigin_Gyro_Yaw
	L"GYRO",				// k_EControllerActionOrigin_Gyro_Roll
};


bool CInputSystem::InitializeSteamControllerActionSets()
{
	auto psteamcontroller = g_pInputSystem->SteamControllerInterface();
	if ( !psteamcontroller )
	{
		return false;
	}

	bool bGotHandles = true;

	for ( int i = 0; i != ARRAYSIZE( g_DigitalMenuActions ); ++i )
	{
		g_DigitalMenuActions[i].handle = psteamcontroller->GetDigitalActionHandle( g_DigitalMenuActions[i].strName );
		bGotHandles = bGotHandles && ( g_DigitalMenuActions[i].handle != 0 );

		// Init the button state array 
		for( int j = 0; j < STEAM_CONTROLLER_MAX_COUNT; j++ )
		{
			g_DigitalMenuActions[i].bState[j] = false;
		}
	}

	for ( int i = 0; i != ARRAYSIZE( g_GameActionSets ); ++i )
	{
		g_GameActionSets[i].handle = psteamcontroller->GetActionSetHandle( g_GameActionSets[i].strName );
		bGotHandles = bGotHandles &&  g_GameActionSets[i].handle;
	}

	return bGotHandles;
}

ConVar sc_debug_sets( "sc_debug_sets", "0", FCVAR_ARCHIVE, "Debugging" );

void CInputSystem::PollSteamControllers( void )
{
	// Make sure we've got the appropriate connections to Steam
	auto steamcontroller = SteamControllerInterface();
	if ( !steamcontroller )
	{
		return;
	}

	steamcontroller->RunFrame();

	uint64 nControllerHandles[STEAM_CONTROLLER_MAX_COUNT];
	m_unNumConnected = steamcontroller->GetConnectedControllers( nControllerHandles );

	if ( m_unNumConnected > 0 )
	{
		if ( !m_bSteamControllerActionsInitialized )
		{
			// Retry initialization of controller actions if we didn't acquire them all before for some reason.
			m_bSteamControllerActionsInitialized = m_bSteamController && InitializeSteamControllerActionSets();
			g_pInputSystem->ActivateSteamControllerActionSet( GAME_ACTION_SET_MENUCONTROLS );
		}

		if ( m_bSteamControllerActionsInitialized )
		{  
			// If we successfully initialized all the actions, and we have a connected controller, then we're considered "active".
			m_bSteamControllerActive = true;

			// For each digital action
			for ( int i = 0; i != ARRAYSIZE( g_DigitalMenuActions ); ++i )
			{
				SDigitalMenuAction& action = g_DigitalMenuActions[i];

				// and for each controller
				for ( uint64 j = 0; j < m_unNumConnected; ++j )
				{

					// Get the action's current state
					ControllerDigitalActionData_t data = steamcontroller->GetDigitalActionData( nControllerHandles[j], action.handle );

					// We only care if the action is active
					if ( data.bActive )
					{
						action.bAwaitingDebounce[j] = action.bAwaitingDebounce[j] && data.bState;

						// If the action's state has changed
						if ( data.bState != action.bState[j] )
						{
							action.bState[j] = data.bState;

							// Press the key for the correct controller
							ButtonCode_t buttonCode = ButtonCodeToJoystickButtonCode( action.buttonCode, j );

							if ( action.bState[j] )
							{
								if ( !action.bAwaitingDebounce[j] )
								{
									PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, buttonCode, buttonCode );
								}
							}
							else
							{
								PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, buttonCode, buttonCode );
							}
						}
					}
				}
			}
		}
	}
	else
	{
		// If no controllers are connected, unflag the active state.
		m_bSteamControllerActive = false;
	}
}

bool CInputSystem::GetRadialMenuStickValues( int nSlot, float &fX, float &fY )
{
	fX = m_pRadialMenuStickVal[nSlot][0];
	fY = m_pRadialMenuStickVal[nSlot][1];

	return true;
}

bool CInputSystem::IsSteamControllerActive( void )
{
	return m_bSteamControllerActive;
}

bool CInputSystem::InitializeSteamControllers()
{
	m_flLastSteamControllerInput = -FLT_MAX;
	auto steamcontroller = SteamControllerInterface();
	if ( steamcontroller )
	{
		if ( !steamcontroller->Init() )
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	for( int i=0; i<STEAM_CONTROLLER_MAX_COUNT; i++ )
	{
		m_pRadialMenuStickVal[i][0] = 0.0f;
		m_pRadialMenuStickVal[i][1] = 0.0f;
	}
	
	// We have to account for other joysticks prior to adding steam controllers
	// So we get the baseline number here when first initializing
	m_nJoystickBaseline = m_nJoystickCount;
	if ( steamcontroller )
	{
		uint64 nControllerHandles[STEAM_CONTROLLER_MAX_COUNT];
		m_unNumConnected = steamcontroller->GetConnectedControllers( nControllerHandles );
		steamcontroller->RunFrame();

		if ( m_unNumConnected > 0 )
		{
			for ( uint32 i = 0; i < m_unNumConnected; i++ )
			{
				if ( m_Device[i].m_nJoystickIndex == INVALID_USER_ID )
				{
					int nJoystickIndex = i;
					m_Device[i].m_nJoystickIndex = nJoystickIndex;
					m_Device[i].m_nHardwareIndex = i;
				}
				m_nControllerType[m_Device[i].m_nJoystickIndex] = INPUT_TYPE_STEAMCONTROLLER;
			}
		}

		return true;
	}

	return false;	
}

ControllerActionSetHandle_t CInputSystem::GetActionSetHandle( GameActionSet_t eActionSet )
{
	return g_GameActionSets[eActionSet].handle;
}

ControllerActionSetHandle_t CInputSystem::GetActionSetHandle( const char* szActionSet )
{
	for ( int i = 0; i != ARRAYSIZE( g_GameActionSets ); ++i )
	{
		if ( !Q_strcmp( szActionSet, g_GameActionSets[i].strName ) )
		{
			return g_GameActionSets[i].handle;
		}
	}

	return 0;
}


void CInputSystem::ActivateSteamControllerActionSetForSlot( uint64 nSlot, GameActionSet_t eActionSet )
{
	auto steamcontroller = SteamControllerInterface();
	bool bChangedActionSet = false;

	if ( steamcontroller )
	{
		if ( nSlot == STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS )
		{
			for ( int i = 0; i < STEAM_CONTROLLER_MAX_COUNT; i++ )
			{
				if ( m_currentActionSet[i] != eActionSet )
				{
					bChangedActionSet = true;
					m_currentActionSet[i] = eActionSet;
				}
			}

			steamcontroller->ActivateActionSet( STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS, g_GameActionSets[eActionSet].handle );
		}
		else
		{
			uint64 nControllerHandles[STEAM_CONTROLLER_MAX_COUNT];
			int unNumConnected = steamcontroller->GetConnectedControllers( nControllerHandles );

			if( nSlot < unNumConnected )
			{
				if ( m_currentActionSet[nSlot] != eActionSet )
				{
					bChangedActionSet = true;
					m_currentActionSet[nSlot] = eActionSet;
				}

				steamcontroller->ActivateActionSet( nControllerHandles[nSlot], g_GameActionSets[eActionSet].handle );
			}
		}
	}

	if ( bChangedActionSet )
	{
		// If we changed action set, then flag everything for a debounce (meaning we demand to see an unpressed state before we'll register a pressed one)
		for ( int i = 0; i < STEAM_CONTROLLER_MAX_COUNT; i++ )
		{
			for ( int j = 0; j != ARRAYSIZE( g_DigitalMenuActions ); ++j )
			{
				g_DigitalMenuActions[j].bAwaitingDebounce[i] = true;
			}
		}
	}
}

const int CInputSystem::GetSteamPadDeadZone( ESteamPadAxis axis )
{
  int nDeadzone = s_nSteamPadDeadZoneTable[ axis ];

  // Do modifications if required here?

  return nDeadzone;
}

//-----------------------------------------------------------------------------
// Purpose: Processes data for controller
//-----------------------------------------------------------------------------
void CInputSystem::ReadSteamController( int iIndex )
{
}

//-----------------------------------------------------------------------------
//	Purpose: Pulse haptic feedback
//-----------------------------------------------------------------------------
/* void CInputSystem::PulseHapticOnSteamController( uint32 nControllerIndex, ESteamControllerPad ePad, unsigned short durationMicroSec )
{
	auto steamcontroller = SteamControllerInterface();
	if ( steamcontroller )
	{
			steamcontroller->TriggerHapticPulse( nControllerIndex, ePad, durationMicroSec );
	}
}*/

//-----------------------------------------------------------------------------
//	Purpose: Returns the controller State for a particular joystick slot
//-----------------------------------------------------------------------------
bool CInputSystem::GetControllerStateForSlot( int nSlot )
{
	return false;
}

int CInputSystem::GetSteamControllerIndexForSlot( int nSlot )
{
	for ( int i = 0; i < Q_ARRAYSIZE( m_Device ); i++ )
	{
		if ( m_Device[i].active && (int)m_Device[i].m_nJoystickIndex == nSlot )
		{
			return m_Device[i].m_nHardwareIndex;
		}
	}
	return -1;
}

//-----------------------------------------------------------------------------
//	Purpose: Post events, ignoring key repeats
//-----------------------------------------------------------------------------
void CInputSystem::PostKeyEvent( int iIndex, sKey_t sKey, int nSample )
{
	// Rework of xbox code here :

	AnalogCode_t	code	= ANALOG_CODE_LAST;
	float			value	= 0.f;
	//int nMsgSlot = iIndex;
	int nMsgSlot = m_Device[iIndex].m_nJoystickIndex;
	int nSampleThreshold = 1; 

	// Look for changes on the analog axes
	switch( sKey )
	{
	case SK_BUTTON_LPAD_LEFT:
	case SK_BUTTON_LPAD_RIGHT:
		{
			code = (AnalogCode_t)JOYSTICK_AXIS( nMsgSlot, JOY_AXIS_X );
			value = ( sKey == SK_BUTTON_LPAD_LEFT ) ? -nSample : nSample;
			// Kind of a hack to horizontal values for menu selection items in Portal 2
			// The additional 5k helps accidental horizontals in menus.
			nSampleThreshold = ( int )( STEAMPAD_DIGITAL_PAD_THRESHOLD ) + 5000;  
		}
		break;

	case SK_BUTTON_LPAD_UP:
	case SK_BUTTON_LPAD_DOWN:
		{
			code = (AnalogCode_t)JOYSTICK_AXIS( nMsgSlot, JOY_AXIS_Y );
			value = ( sKey == SK_BUTTON_LPAD_UP ) ? -nSample : nSample;
			nSampleThreshold = ( int )( STEAMPAD_DIGITAL_PAD_THRESHOLD );
		}
		break;

	case SK_BUTTON_RPAD_LEFT:
	case SK_BUTTON_RPAD_RIGHT:
		{
			code = (AnalogCode_t)JOYSTICK_AXIS( nMsgSlot, JOY_AXIS_U );
			value = ( sKey == SK_BUTTON_RPAD_LEFT ) ? -nSample : nSample;
			// Kind of a hack to horizontal values for menu selection items in Portal 2
			// The additional 5k helps accidental horizontals in menus.
			nSampleThreshold = ( int )( STEAMPAD_DIGITAL_PAD_THRESHOLD ) + 5000;;
		}
		break;

	case SK_BUTTON_RPAD_UP:
	case SK_BUTTON_RPAD_DOWN:
		{
			code = (AnalogCode_t)JOYSTICK_AXIS( nMsgSlot, JOY_AXIS_R );
			value = ( sKey == SK_BUTTON_RPAD_UP ) ? -nSample : nSample;
			nSampleThreshold = ( int )( STEAMPAD_DIGITAL_PAD_THRESHOLD );
		}
		break;
	}

	// Store the analog event
	if ( ANALOG_CODE_LAST != code )
	{
		InputState_t &state = m_InputState[ m_bIsPolling ];
		state.m_pAnalogDelta[ code ] = ( int )( value - state.m_pAnalogValue[ code ] );
		state.m_pAnalogValue[ code ] = ( int )value;
		if ( state.m_pAnalogDelta[ code ] != 0 )
		{
			PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, code, ( int )value, state.m_pAnalogDelta[ code ] );
		}
	}

	// store the key
	m_Device[iIndex].m_appSKeys[sKey].sample = nSample;
	if ( nSample > nSampleThreshold )
	{
		m_Device[iIndex].m_appSKeys[sKey].repeats++;
	}
	else
	{
		m_Device[iIndex].m_appSKeys[sKey].repeats = 0;
		nSample = 0;
	}

	if ( m_Device[iIndex].m_appSKeys[sKey].repeats > 1 )
	{
		// application cannot handle streaming keys
		// first keypress is the only edge trigger
		return;
	}

	// package the key
	ButtonCode_t buttonCode = SKeyToButtonCode( nMsgSlot, sKey );
	if ( nSample )
	{
		PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, buttonCode, buttonCode );
	}
	else
	{
		PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, buttonCode, buttonCode );
	}
}

// Gets the action origin (i.e. which physical input) maps to the given virtual button for the given action set
EControllerActionOrigin CInputSystem::GetSteamControllerActionOrigin( const char* action, GameActionSet_t action_set )
{
	auto pSC = SteamControllerInterface();
	if ( pSC )
	{
		ControllerHandle_t hConnected[STEAM_CONTROLLER_MAX_COUNT];
		auto nConnected = pSC->GetConnectedControllers( hConnected );
		if ( nConnected == 0 )
		{
			return k_EControllerActionOrigin_None;
		}

		SGameActionSet* pActionSet = nullptr;
		for ( int i = 0; i < ARRAYSIZE( g_GameActionSets ); i++ )
		{
			if ( g_GameActionSets[i].eGameActionSet == action_set )
			{
				pActionSet = &g_GameActionSets[i];
				break;
			}
		}

		if ( pActionSet )
		{
			auto actionHandle = pSC->GetDigitalActionHandle( action );
			EControllerActionOrigin origins[STEAM_CONTROLLER_MAX_ORIGINS];
			int nOrigins = pSC->GetDigitalActionOrigins( hConnected[0], pActionSet->handle, actionHandle, origins );
			if ( nOrigins > 0 )
			{
				return origins[0];
			}
		}
	}

	return k_EControllerActionOrigin_None;
}

// Gets the action origin (i.e. which physical input) maps to the given virtual button for the given action set
EControllerActionOrigin CInputSystem::GetSteamControllerActionOrigin( const char* action, ControllerActionSetHandle_t action_set_handle )
{
	auto pSC = SteamControllerInterface();
	if ( pSC && action_set_handle )
	{
		ControllerHandle_t hConnected[STEAM_CONTROLLER_MAX_COUNT];
		auto nConnected = pSC->GetConnectedControllers( hConnected );
		if ( nConnected == 0 )
		{
			return k_EControllerActionOrigin_None;
		}

		auto actionHandle = pSC->GetDigitalActionHandle( action );
		EControllerActionOrigin origins[STEAM_CONTROLLER_MAX_ORIGINS];
		int nOrigins = pSC->GetDigitalActionOrigins( hConnected[0], action_set_handle, actionHandle, origins );
		if ( nOrigins > 0 )
		{
			return origins[0];
		}
	}

	return k_EControllerActionOrigin_None;
}

// Maps a Steam Controller action origin to a string (consisting of a single character) in our SC icon font
const wchar_t*	CInputSystem::GetSteamControllerFontCharacterForActionOrigin( EControllerActionOrigin origin )
{
	if ( origin >= 0 && origin < ARRAYSIZE( g_MapSteamControllerOriginToIconFont ) )
	{
		return g_MapSteamControllerOriginToIconFont[origin];
	}
	else
	{
		return L"";
	}
}

// Maps a Steam Controller action origin to a short text string (e.g. "X", "LB", "LDOWN") describing the control.
// Prefer to actually use the icon font wherever possible.
const wchar_t* CInputSystem::GetSteamControllerDescriptionForActionOrigin( EControllerActionOrigin origin )
{
	if ( origin >= 0 && origin < ARRAYSIZE( g_MapSteamControllerOriginToDescription ) )
	{
		return g_MapSteamControllerOriginToDescription[origin];
	}
	else
	{
		return L"";
	}
}
