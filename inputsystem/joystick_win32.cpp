//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: PC Joystick implementation for inputsystem.dll
//
//===========================================================================//

/* For force feedback testing. */
#include "inputsystem.h"
#include "tier1/convar.h"
#include "tier0/icommandline.h"

//-----------------------------------------------------------------------------
// Joystick helpers
//-----------------------------------------------------------------------------
#define JOY_POVFWDRIGHT		( ( JOY_POVFORWARD + JOY_POVRIGHT ) >> 1 )  // 4500
#define JOY_POVRIGHTBACK	( ( JOY_POVRIGHT + JOY_POVBACKWARD ) >> 1 ) // 13500
#define JOY_POVFBACKLEFT	( ( JOY_POVBACKWARD + JOY_POVLEFT ) >> 1 ) // 22500
#define JOY_POVLEFTFWD		( ( JOY_POVLEFT + JOY_POVFORWARD ) >> 1 ) // 31500

ConVar joy_wwhack1( "joy_wingmanwarrior_centerhack", "0", FCVAR_ARCHIVE, "Wingman warrior centering hack." );
ConVar joy_axisbutton_threshold( "joy_axisbutton_threshold", "0.3", FCVAR_ARCHIVE, "Analog axis range before a button press is registered." );

//-----------------------------------------------------------------------------
// Initialize all joysticks 
//-----------------------------------------------------------------------------
void CInputSystem::InitializeJoysticks( void ) 
{  
 	// assume no joystick
	m_nJoystickCount = 0; 

	// abort startup if user requests no joystick
	if ( CommandLine()->FindParm("-nojoy" ) ) 
		return; 
 
	// verify joystick driver is present
	int nMaxJoysticks = joyGetNumDevs();
	if ( nMaxJoysticks > MAX_JOYSTICKS )
	{
		nMaxJoysticks = MAX_JOYSTICKS; 
	}
	else if ( nMaxJoysticks <= 0 )
	{
		DevMsg( 1, "joystick not found -- driver not present\n");
		return;
	}

	// cycle through the joysticks looking for valid ones
	MMRESULT	mmr;
	for ( int i=0; i < nMaxJoysticks; i++ )
	{
		JOYINFOEX ji;
		Q_memset( &ji, 0, sizeof( ji ) );
		ji.dwSize = sizeof(ji);
		ji.dwFlags = JOY_RETURNCENTERED;
		mmr = joyGetPosEx( i, &ji );
		if ( mmr != JOYERR_NOERROR )
			continue;

		// get the capabilities of the selected joystick
		// abort startup if command fails
		JOYCAPS		jc;
		Q_memset( &jc, 0, sizeof( jc ) );
		mmr = joyGetDevCaps( i, &jc, sizeof( jc ) );
		if ( mmr != JOYERR_NOERROR )
			continue;

		JoystickInfo_t &info = m_pJoystickInfo[m_nJoystickCount];
		info.m_nDeviceId = i;
		info.m_JoyInfoEx = ji;
		info.m_nButtonCount = (int)jc.wNumButtons;
		info.m_bHasPOVControl = ( jc.wCaps & JOYCAPS_HASPOV ) ? true : false;
		info.m_bDiagonalPOVControlEnabled = false;
		info.m_nFlags = JOY_RETURNCENTERED | JOY_RETURNBUTTONS | JOY_RETURNX | JOY_RETURNY;
		info.m_nAxisFlags = 0;
		if ( jc.wNumAxes >= 2 )
		{
			info.m_nAxisFlags |= 0x3;
		}
		if ( info.m_bHasPOVControl )
		{
			info.m_nFlags |= JOY_RETURNPOV;
		}
		if ( jc.wCaps & JOYCAPS_HASZ )
		{
			info.m_nFlags |= JOY_RETURNZ;
			info.m_nAxisFlags |= 0x4;
		}
		if ( jc.wCaps & JOYCAPS_HASR )
		{
			info.m_nFlags |= JOY_RETURNR;
			info.m_nAxisFlags |= 0x8;
		}
		if ( jc.wCaps & JOYCAPS_HASU )
		{
			info.m_nFlags |= JOY_RETURNU;
			info.m_nAxisFlags |= 0x10;
		}
		if ( jc.wCaps & JOYCAPS_HASV )
		{
			info.m_nFlags |= JOY_RETURNV;
			info.m_nAxisFlags |= 0x20;
		}
		info.m_nLastPolledButtons = 0;
		info.m_nLastPolledAxisButtons = 0;
		info.m_nLastPolledPOVState = 0;
		memset( info.m_pLastPolledAxes, 0, sizeof(info.m_pLastPolledAxes) );
		++m_nJoystickCount;

		EnableJoystickInput( i, true );
	} 
}

void CInputSystem::ShutdownJoysticks()
{
}

//-----------------------------------------------------------------------------
//	Process the event
//-----------------------------------------------------------------------------
void CInputSystem::JoystickButtonEvent( ButtonCode_t button, int sample )
{
	// package the key
	if ( sample )
	{
		PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, button, button );
	}
	else
	{
		PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, button, button );
	}
}


//-----------------------------------------------------------------------------
// Update the joystick button state
//-----------------------------------------------------------------------------
void CInputSystem::UpdateJoystickButtonState( int nJoystick )
{
	JoystickInfo_t &info = m_pJoystickInfo[nJoystick];
	JOYINFOEX& ji = info.m_JoyInfoEx;

	// Standard joystick buttons
	unsigned int buttons = ji.dwButtons ^ info.m_nLastPolledButtons;
	if ( buttons )
	{
		for ( int j = 0 ; j < info.m_nButtonCount ; ++j )
		{
			int mask = buttons & ( 1 << j );
			if ( !mask )
				continue;

			ButtonCode_t code = (ButtonCode_t)JOYSTICK_BUTTON( nJoystick, j );
			if ( mask & ji.dwButtons )
			{
				// down event
				JoystickButtonEvent( code, MAX_BUTTONSAMPLE );
			}
			else
			{
				// up event
				JoystickButtonEvent( code, 0 );
			}
		}

		info.m_nLastPolledButtons = (unsigned int)ji.dwButtons;
	}

	// Analog axis buttons
	const float minValue = joy_axisbutton_threshold.GetFloat() * MAX_BUTTONSAMPLE;
	for ( int j = 0 ; j < MAX_JOYSTICK_AXES; ++j )
	{
		if ( ( info.m_nAxisFlags & (1 << j) ) == 0 )
			continue;

		// Positive side of the axis
		int mask = ( 1 << (j << 1) );
		ButtonCode_t code = JOYSTICK_AXIS_BUTTON( nJoystick, (j << 1) );
		float value = GetAnalogValue( JOYSTICK_AXIS( nJoystick, j ) );

		if ( value > minValue && !(info.m_nLastPolledAxisButtons & mask) )
		{
			info.m_nLastPolledAxisButtons |= mask;
			JoystickButtonEvent( code, MAX_BUTTONSAMPLE );
		}
		if ( value <= minValue && (info.m_nLastPolledAxisButtons & mask) )
		{
			info.m_nLastPolledAxisButtons &= ~mask;
			JoystickButtonEvent( code, 0 );
		}

		// Negative side of the axis
		mask <<= 1;
		code = (ButtonCode_t)( code + 1 );
		if ( value < -minValue && !(info.m_nLastPolledAxisButtons & mask) )
		{
			info.m_nLastPolledAxisButtons |= mask;
			JoystickButtonEvent( code, MAX_BUTTONSAMPLE );
		}
		if ( value >= -minValue && (info.m_nLastPolledAxisButtons & mask) )
		{
			info.m_nLastPolledAxisButtons &= ~mask;
			JoystickButtonEvent( code, 0 );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Get raw joystick sample along axis
//-----------------------------------------------------------------------------
unsigned int CInputSystem::AxisValue( JoystickAxis_t axis, JOYINFOEX& ji )
{
	switch (axis)
	{
	case JOY_AXIS_X:
		return (unsigned int)ji.dwXpos;
	case JOY_AXIS_Y:
		return (unsigned int)ji.dwYpos;
	case JOY_AXIS_Z:
		return (unsigned int)ji.dwZpos;
	case JOY_AXIS_R:
		return (unsigned int)ji.dwRpos;
	case JOY_AXIS_U:
		return (unsigned int)ji.dwUpos;
	case JOY_AXIS_V:
		return (unsigned int)ji.dwVpos;
	}
	// FIX: need to do some kind of error
	return (unsigned int)ji.dwXpos;
}


//-----------------------------------------------------------------------------
// Update the joystick POV control
//-----------------------------------------------------------------------------
void CInputSystem::UpdateJoystickPOVControl( int nJoystick )
{
	JoystickInfo_t &info = m_pJoystickInfo[nJoystick];
	JOYINFOEX& ji = info.m_JoyInfoEx;

	if ( !info.m_bHasPOVControl )
		return;

	// convert POV information into 4 bits of state information
	// this avoids any potential problems related to moving from one
	// direction to another without going through the center position
	unsigned int povstate = 0;

	if ( ji.dwPOV != JOY_POVCENTERED )
	{
		if (ji.dwPOV == JOY_POVFORWARD)  // 0
		{
			povstate |= 0x01;
		}
		if (ji.dwPOV == JOY_POVRIGHT)  // 9000
		{
			povstate |= 0x02;
		}
		if (ji.dwPOV == JOY_POVBACKWARD) // 18000
		{
			povstate |= 0x04;
		}
		if (ji.dwPOV == JOY_POVLEFT)  // 27000
		{
			povstate |= 0x08;
		}

		// Deal with diagonals if user wants them
		if ( info.m_bDiagonalPOVControlEnabled )
		{
			if (ji.dwPOV == JOY_POVFWDRIGHT)  // 4500
			{
				povstate |= ( 0x01 | 0x02 );
			}
			if (ji.dwPOV == JOY_POVRIGHTBACK)  // 13500
			{
				povstate |= ( 0x02 | 0x04 );
			}
			if (ji.dwPOV == JOY_POVFBACKLEFT) // 22500
			{
				povstate |= ( 0x04 | 0x08 );
			}
			if (ji.dwPOV == JOY_POVLEFTFWD)  // 31500
			{
				povstate |= ( 0x08 | 0x01 );
			}
		}
	}

	// determine which bits have changed and key an auxillary event for each change
	unsigned int buttons = povstate ^ info.m_nLastPolledPOVState;
	if ( buttons )
	{
		for ( int i = 0; i < JOYSTICK_POV_BUTTON_COUNT; ++i )
		{
			unsigned int mask = buttons & ( 1 << i );
			if ( !mask )
				continue;

			ButtonCode_t code = (ButtonCode_t)JOYSTICK_POV_BUTTON( nJoystick, i );

			if ( mask & povstate )
			{
				// Keydown on POV buttons
				JoystickButtonEvent( code, MAX_BUTTONSAMPLE );
			}
			else
			{
				// KeyUp on POV buttons
				JoystickButtonEvent( code, 0 );
			}
		}

		// Latch old values
		info.m_nLastPolledPOVState = povstate;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sample the joystick
//-----------------------------------------------------------------------------
void CInputSystem::PollJoystick( void )
{
	if ( !m_JoysticksEnabled.IsAnyFlagSet() )
		return;

	InputState_t &state = m_InputState[ m_bIsPolling ];
	for ( int i = 0; i < m_nJoystickCount; ++i )
	{
		if ( !m_JoysticksEnabled.IsFlagSet( 1 << i ) )
			continue;

		JoystickInfo_t &info = m_pJoystickInfo[i];
		JOYINFOEX& ji = info.m_JoyInfoEx;
		Q_memset( &ji, 0, sizeof( ji ) );
		ji.dwSize = sizeof( ji );
		ji.dwFlags = (DWORD)info.m_nFlags;

		if ( joyGetPosEx( info.m_nDeviceId, &ji ) != JOYERR_NOERROR )
			continue;

		// This hack fixes a bug in the Logitech WingMan Warrior DirectInput Driver
		// rather than having 32768 be the zero point, they have the zero point at 32668
		// go figure -- anyway, now we get the full resolution out of the device
		if ( joy_wwhack1.GetBool() )
		{
			ji.dwUpos += 100;
		}

		// Poll joystick axes
		for ( int j = 0; j < MAX_JOYSTICK_AXES; ++j )
		{
			if ( ( info.m_nAxisFlags & ( 1 << j ) ) == 0 )
				continue;

			AnalogCode_t code = JOYSTICK_AXIS( i, j );
			int nValue = AxisValue( (JoystickAxis_t)j, ji ) - MAX_BUTTONSAMPLE;
			state.m_pAnalogDelta[ code ] = nValue - state.m_pAnalogValue[ code ];
			state.m_pAnalogValue[ code ] = nValue;
			if ( state.m_pAnalogDelta[ code ] != 0 )
			{
				PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, code, state.m_pAnalogValue[ code ], state.m_pAnalogDelta[ code ] );
			}
		}

		UpdateJoystickButtonState( i );
		UpdateJoystickPOVControl( i );
	}
}
