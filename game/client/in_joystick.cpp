//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Joystick handling function
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "basehandle.h"
#include "utlvector.h"
#include "cdll_client_int.h"
#include "cdll_util.h"
#include "kbutton.h"
#include "usercmd.h"
#include "iclientvehicle.h"
#include "input.h"
#include "iviewrender.h"
#include "iclientmode.h"
#include "convar.h"
#include "hud.h"
#include "vgui/isurface.h"
#include "vgui_controls/controls.h"
#include "vgui/cursor.h"
#include "tier0/icommandline.h"
#include "inputsystem/iinputsystem.h"
#include "inputsystem/ButtonCode.h"
#include "math.h"
#include "tier1/convar_serverbounded.h"
#include "c_baseplayer.h"
#include "inputsystem/iinputstacksystem.h"
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#else
#include "../common/xbox/xboxstubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Control like a joystick
#define JOY_ABSOLUTE_AXIS	0x00000000		
// Control like a mouse, spinner, trackball
#define JOY_RELATIVE_AXIS	0x00000010		

static ConVar joy_variable_frametime( "joy_variable_frametime", IsX360() ? "0" : "1", 0 );

// Axis mapping
static ConVar joy_name( "joy_name", "joystick", FCVAR_ARCHIVE );
static ConVar joy_advanced( "joy_advanced", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisx( "joy_advaxisx", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisy( "joy_advaxisy", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisz( "joy_advaxisz", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisr( "joy_advaxisr", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisu( "joy_advaxisu", "0", FCVAR_ARCHIVE );
static ConVar joy_advaxisv( "joy_advaxisv", "0", FCVAR_ARCHIVE );

// Basic "dead zone" and sensitivity
static ConVar joy_forwardthreshold( "joy_forwardthreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_sidethreshold( "joy_sidethreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_pitchthreshold( "joy_pitchthreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_yawthreshold( "joy_yawthreshold", "0.15", FCVAR_ARCHIVE );
static ConVar joy_forwardsensitivity( "joy_forwardsensitivity", "-1", FCVAR_ARCHIVE );
static ConVar joy_sidesensitivity( "joy_sidesensitivity", "1", FCVAR_ARCHIVE );
static ConVar joy_pitchsensitivity( "joy_pitchsensitivity", "1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX | FCVAR_SS );
static ConVar joy_yawsensitivity( "joy_yawsensitivity", "-1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX | FCVAR_SS );

// Advanced sensitivity and response
#ifdef _X360 //tmuaer
static ConVar joy_response_move( "joy_response_move", "9", FCVAR_ARCHIVE, "'Movement' stick response mode: 0=Linear, 1=quadratic, 2=cubic, 3=quadratic extreme, 4=power function(i.e., pow(x,1/sensitivity)), 5=two-stage" );
#else
static ConVar joy_response_move( "joy_response_move", "1", FCVAR_ARCHIVE, "'Movement' stick response mode: 0=Linear, 1=quadratic, 2=cubic, 3=quadratic extreme, 4=power function(i.e., pow(x,1/sensitivity)), 5=two-stage" );
#endif

ConVar joy_response_move_vehicle("joy_response_move_vehicle", "6");
static ConVar joy_response_look( "joy_response_look", "0", FCVAR_ARCHIVE, "'Look' stick response mode: 0=Default, 1=Acceleration Promotion" );
static ConVar joy_lowend( "joy_lowend", "1", FCVAR_ARCHIVE );
static ConVar joy_lowmap( "joy_lowmap", "1", FCVAR_ARCHIVE );
static ConVar joy_accelscale( "joy_accelscale", "0.6", FCVAR_ARCHIVE);
static ConVar joy_accelmax( "joy_accelmax", "1.0", FCVAR_ARCHIVE);
static ConVar joy_autoaimdampenrange( "joy_autoaimdampenrange", "0", FCVAR_ARCHIVE, "The stick range where autoaim dampening is applied. 0 = off" );
ConVar joy_autoaimdampen( "joy_autoaimdampen", "0", FCVAR_ARCHIVE | FCVAR_USERINFO, "How much to scale user stick input when the gun is pointing at a valid target." );

static ConVar joy_vehicle_turn_lowend("joy_vehicle_turn_lowend", "0.7");
static ConVar joy_vehicle_turn_lowmap("joy_vehicle_turn_lowmap", "0.4");

static ConVar joy_sensitive_step0( "joy_sensitive_step0", "0.1", FCVAR_ARCHIVE);
static ConVar joy_sensitive_step1( "joy_sensitive_step1", "0.4", FCVAR_ARCHIVE);
static ConVar joy_sensitive_step2( "joy_sensitive_step2", "0.90", FCVAR_ARCHIVE);
static ConVar joy_circle_correct( "joy_circle_correct", "1", FCVAR_ARCHIVE);

// Misc
static ConVar joy_diagonalpov( "joy_diagonalpov", "0", FCVAR_ARCHIVE, "POV manipulator operates on diagonal axes, too." );
static ConVar joy_display_input("joy_display_input", "0", FCVAR_ARCHIVE);
static ConVar joy_wwhack2( "joy_wingmanwarrior_turnhack", "0", FCVAR_ARCHIVE, "Wingman warrior hack related to turn axes." );
ConVar joy_autosprint("joy_autosprint", "0", 0, "Automatically sprint when moving with an analog joystick" );

static ConVar joy_inverty("joy_inverty", "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX | FCVAR_SS, "Whether to invert the Y axis of the joystick for looking." );

// XBox Defaults
static ConVar joy_yawsensitivity_default( "joy_yawsensitivity_default", "-1.25", FCVAR_NONE );
static ConVar joy_pitchsensitivity_default( "joy_pitchsensitivity_default", "-1.0", FCVAR_NONE );
static ConVar sv_stickysprint_default( "sv_stickysprint_default", "0", FCVAR_NONE );
static ConVar joy_lookspin_default( "joy_lookspin_default", "0.35", FCVAR_NONE );

static ConVar joy_cfg_preset( "joy_cfg_preset", "0", FCVAR_ARCHIVE_XBOX | FCVAR_SS );

void joy_movement_stick_Callback( IConVar *var, const char *pOldString, float flOldValue )
{
	engine->ClientCmd( "joyadvancedupdate silent\n" );
}
static ConVar joy_movement_stick("joy_movement_stick", "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX | FCVAR_SS, "Which stick controls movement (0 is left stick)", joy_movement_stick_Callback );

static ConVar joy_xcontroller_cfg_loaded( "joy_xcontroller_cfg_loaded", "0", 0, "If 0, the 360controller.cfg file will be executed on startup & option changes." );

extern ConVar lookspring;
extern ConVar cl_forwardspeed;
extern ConVar lookstrafe;
extern ConVar in_joystick;
extern ConVar_ServerBounded *m_pitch;
extern ConVar l_pitchspeed;
extern ConVar cl_sidespeed;
extern ConVar cl_yawspeed;
extern ConVar cl_pitchdown;
extern ConVar cl_pitchup;
extern ConVar cl_pitchspeed;
#ifdef INFESTED_DLL
extern ConVar asw_cam_marine_yaw;
#endif
extern ConVar cam_idealpitch;
extern ConVar cam_idealyaw;
extern ConVar thirdperson_platformer;
extern ConVar thirdperson_screenspace;

//-----------------------------------------------
// Response curve function for the move axes
//-----------------------------------------------
static float ResponseCurve( int curve, float x, int axis, float sensitivity )
{
	switch ( curve )
	{
	case 1:
		// quadratic
		if ( x < 0 )
			return -(x*x) * sensitivity;
		return x*x * sensitivity;

	case 2:
		// cubic
		return x*x*x*sensitivity;

	case 3:
		{
		// quadratic extreme
		float extreme = 1.0f;
		if ( fabs( x ) >= 0.95f )
		{
			extreme = 1.5f;
		}
		if ( x < 0 )
			return -extreme * x*x*sensitivity;
		return extreme * x*x*sensitivity;
		}
	case 4:
		{
			float flScale = sensitivity < 0.0f ? -1.0f : 1.0f;

			sensitivity = clamp( fabs( sensitivity ), 1.0e-8f, 1000.0f );

			float oneOverSens = 1.0f / sensitivity;
		
			if ( x < 0.0f )
			{
				flScale = -flScale;
			}

			float retval = clamp( powf( fabs( x ), oneOverSens ), 0.0f, 1.0f );
			return retval * flScale;
		}
		break;
	case 5:
		{
			float out = x;

			if( fabs(out) <= 0.6f )
			{
				out *= 0.5f;
			}

			out = out * sensitivity;
			return out;
		}
		break;
	case 6: // Custom for driving a vehicle!
		{
			if( axis == YAW )
			{
				// This code only wants to affect YAW axis (the left and right axis), which 
				// is used for turning in the car. We fall-through and use a linear curve on 
				// the PITCH axis, which is the vehicle's throttle. REALLY, these are the 'forward'
				// and 'side' axes, but we don't have constants for those, so we re-use the same
				// axis convention as the look stick. (sjb)
				float sign = 1;

				if( x  < 0.0 )
					sign = -1;

				x = fabs(x);

				if( x <= joy_vehicle_turn_lowend.GetFloat() )
					x = RemapVal( x, 0.0f, joy_vehicle_turn_lowend.GetFloat(), 0.0f, joy_vehicle_turn_lowmap.GetFloat() );
				else
					x = RemapVal( x, joy_vehicle_turn_lowend.GetFloat(), 1.0f, joy_vehicle_turn_lowmap.GetFloat(), 1.0f );

				return x * sensitivity * sign;
			}
			//else
			//	fall through and just return x*sensitivity below (as if using default curve)
		}
		//The idea is to create a max large walk zone surrounded by a max run zone.
	case 7:
		{
			float xAbs = fabs(x);
			if(xAbs < joy_sensitive_step0.GetFloat())
			{
				return 0;
			}
			else if (xAbs < joy_sensitive_step2.GetFloat())
			{
				return (85.0f/cl_forwardspeed.GetFloat()) * ((x < 0)? -1.0f : 1.0f);
			}
			else
			{
				return ((x < 0)? -1.0f : 1.0f);
			}
		}
		break;
	case 8: //same concept as above but with smooth speeds
		{
			float xAbs = fabs(x);
			if(xAbs < joy_sensitive_step0.GetFloat())
			{
				return 0;
			}
			else if (xAbs < joy_sensitive_step2.GetFloat())
			{
				float maxSpeed = (85.0f/cl_forwardspeed.GetFloat());
				float t = (xAbs-joy_sensitive_step0.GetFloat())
					/ (joy_sensitive_step2.GetFloat()-joy_sensitive_step0.GetFloat());
				float speed = t*maxSpeed;
				return speed * ((x < 0)? -1.0f : 1.0f);
			}
			else
			{
				float maxSpeed = 1.0f;
				float minSpeed = (85.0f/cl_forwardspeed.GetFloat());
				float t = (xAbs-joy_sensitive_step2.GetFloat())
					/ (1.0f-joy_sensitive_step2.GetFloat());
				float speed = t*(maxSpeed-minSpeed) + minSpeed;
				return speed * ((x < 0)? -1.0f : 1.0f);
			}
		}
		break;
	case 9: //same concept as above but with smooth speeds for walking and a hard speed for running
		{
			float xAbs = fabs(x);
			if(xAbs < joy_sensitive_step0.GetFloat())
			{
				return 0;
			}
			else if (xAbs < joy_sensitive_step1.GetFloat())
			{
				float maxSpeed = (85.0f/cl_forwardspeed.GetFloat());
				float t = (xAbs-joy_sensitive_step0.GetFloat())
					/ (joy_sensitive_step1.GetFloat()-joy_sensitive_step0.GetFloat());
				float speed = t*maxSpeed;
				return speed * ((x < 0)? -1.0f : 1.0f);
			}
			else if (xAbs < joy_sensitive_step2.GetFloat())
			{
				return (85.0f/cl_forwardspeed.GetFloat()) * ((x < 0)? -1.0f : 1.0f);
			}
			else
			{
				return ((x < 0)? -1.0f : 1.0f);
			}
		}
		break;
	}

	// linear
	return x*sensitivity;
}


//-----------------------------------------------
// If we have a valid autoaim target, dampen the 
// player's stick input if it is moving away from
// the target.
//
// This assists the player staying on target.
//-----------------------------------------------
float AutoAimDampening( float x, int axis, float dist )
{
	// Help the user stay on target if the feature is enabled and the user
	// is not making a gross stick movement.
	if ( joy_autoaimdampen.GetFloat() > 0.0f && fabs(x) < joy_autoaimdampenrange.GetFloat() )
	{
		// Get the player
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pLocalPlayer )
		{
			// Get the autoaim target
			if ( pLocalPlayer->m_Local.m_bAutoAimTarget )
			{
				return joy_autoaimdampen.GetFloat();
			}
		}
	}

	return 1.0f;// No dampening.
}


//-----------------------------------------------
// This structure holds persistent information used
// to make decisions about how to modulate analog
// stick input.
//-----------------------------------------------
typedef struct 
{
	float	envelopeScale[2];
	bool	peggedAxis[2];
	bool	axisPeggedDir[2];
} envelope_t;

envelope_t	controlEnvelope[ MAX_SPLITSCREEN_PLAYERS ];

//-----------------------------------------------
// Response curve function specifically for the 
// 'look' analog stick.
//
// when AXIS == YAW, otherAxisValue contains the 
// value for the pitch of the control stick, and
// vice-versa.
//-----------------------------------------------
ConVar joy_pegged("joy_pegged", "0.75");// Once the stick is pushed this far, it's assumed pegged.
ConVar joy_virtual_peg("joy_virtual_peg", "0");
static float ResponseCurveLookDefault( int nSlot, float x, int axis, float otherAxis, float dist, float frametime )
{
	envelope_t &envelope = controlEnvelope[ MAX( nSlot, 0 ) ];
	float input = x;

	bool bStickIsPhysicallyPegged = ( dist >= joy_pegged.GetFloat() );

	// Make X positive to make things easier, just remember whether we have to flip it back!
	bool negative = false;
	if( x < 0.0f )
	{
		negative = true;
		x *= -1;
	}

	if( axis == YAW && joy_virtual_peg.GetBool() )
	{
		if( x >= 0.95f )
		{
			// User has pegged the stick
			envelope.peggedAxis[axis] = true;
			envelope.axisPeggedDir[axis] = negative;
		}
		
		if( envelope.peggedAxis[axis] == true )
		{
			// User doesn't have the stick pegged on this axis, but they used to. 
			// If the stick is physically pegged, pretend this axis is still pegged.
			if( bStickIsPhysicallyPegged && negative == envelope.axisPeggedDir[axis] )
			{
				// If the user still has the stick physically pegged and hasn't changed direction on
				// this axis, keep pretending they have the stick pegged on this axis.
				x = 1.0f;
			}
			else
			{
				envelope.peggedAxis[axis] = false;
			}
		}
	}

	// Perform the two-stage mapping.
	if( x > joy_lowend.GetFloat() )
	{
		float highmap = 1.0f - joy_lowmap.GetFloat();
		float xNormal = x - joy_lowend.GetFloat();

		float factor = xNormal / ( 1.0f - joy_lowend.GetFloat() );
		x = joy_lowmap.GetFloat() + (highmap * factor);

		// Accelerate.
		if( envelope.envelopeScale[axis] < 1.0f )
		{
			envelope.envelopeScale[axis] += ( frametime * joy_accelscale.GetFloat() );
			if( envelope.envelopeScale[axis] > 1.0f )
			{
				envelope.envelopeScale[axis] = 1.0f;
			}
		}

		float delta = x - joy_lowmap.GetFloat();
		x = joy_lowmap.GetFloat() + (delta * envelope.envelopeScale[axis]);
	}
	else
	{
		// Shut off acceleration
		envelope.envelopeScale[axis] = 0.0f;
		float factor = x / joy_lowend.GetFloat();
		x = joy_lowmap.GetFloat() * factor;
	}

	x *= AutoAimDampening( input, axis, dist );

	if( axis == YAW && x > 0.0f && joy_display_input.GetBool() )
	{
		Msg("In:%f Out:%f Frametime:%f\n", input, x, frametime );
	}

	if( negative )
	{
		x *= -1;
	}

	return x;
}

ConVar joy_accel_filter("joy_accel_filter", "0.2");// If the non-accelerated axis is pushed farther than this, then accelerate it, too.
static float ResponseCurveLookAccelerated( int nSlot, float x, int axis, float otherAxis, float dist, float frametime )
{
	envelope_t &envelope = controlEnvelope[ MAX( nSlot, 0 ) ];

	float input = x;

	float flJoyDist = ( sqrt(x*x + otherAxis * otherAxis) );
	bool bIsPegged = ( flJoyDist>= joy_pegged.GetFloat() );

	// Make X positive to make arithmetic easier for the rest of this function, and
	// remember whether we have to flip it back!
	bool negative = false;
	if( x < 0.0f )
	{
		negative = true;
		x *= -1;
	}

	// Perform the two-stage mapping.
	bool bDoAcceleration = false;// Assume we won't accelerate the input

	if( bIsPegged && x > joy_accel_filter.GetFloat() )
	{
		// Accelerate this axis, since the stick is pegged and 
		// this axis is pressed farther than the acceleration filter
		// Take the lowmap value, or the input, whichever is higher, since 
		// we don't necesarily know whether this is the axis which is pegged
		x = MAX( joy_lowmap.GetFloat(), x );
		bDoAcceleration = true;
	}
	else
	{
		// Joystick is languishing in the low-end, turn off acceleration.
		envelope.envelopeScale[axis] = 0.0f;
		float factor = x / joy_lowend.GetFloat();
		x = joy_lowmap.GetFloat() * factor;
	}

	if( bDoAcceleration )
	{
		float flMax = joy_accelmax.GetFloat();
		if( envelope.envelopeScale[axis] < flMax )
		{
			envelope.envelopeScale[axis] += ( frametime * joy_accelscale.GetFloat() );
			if( envelope.envelopeScale[axis] > flMax )
			{
				envelope.envelopeScale[axis] = flMax;
			}
		}
		float delta = x - joy_lowmap.GetFloat();
		x = joy_lowmap.GetFloat() + (delta * envelope.envelopeScale[axis]);
	}

	x *= AutoAimDampening( input, axis, dist );

	if( axis == YAW && input != 0.0f && joy_display_input.GetBool() )
	{
		Msg("In:%f Out:%f Frametime:%f\n", input, x, frametime );
	}

	if( negative )
	{
		x *= -1;
	}

	return x;
}

//-----------------------------------------------
//-----------------------------------------------
static float ResponseCurveLook( int nSlot, int curve, float x, int axis, float otherAxis, float dist, float frametime )
{
	switch( curve )
	{
	case 1://Promotion of acceleration
		return ResponseCurveLookAccelerated( nSlot, x, axis, otherAxis, dist, frametime );
		break;

	default:
		return ResponseCurveLookDefault( nSlot, x, axis, otherAxis, dist, frametime );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Advanced joystick setup
//-----------------------------------------------------------------------------
void CInput::Joystick_Advanced( bool bSilent )
{
	m_fJoystickAdvancedInit = true;

	// called whenever an update is needed
	int	i;
	DWORD dwTemp;

	if ( IsX360() )
	{
		// Xbox always uses a joystick
		in_joystick.SetValue( 1 );
	}

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );

		PerUserInput_t &user = GetPerUser();

		// Initialize all the maps
		for ( i = 0; i < MAX_JOYSTICK_AXES; i++ )
		{
			user.m_rgAxes[i].AxisMap = GAME_AXIS_NONE;
			user.m_rgAxes[i].ControlMap = JOY_ABSOLUTE_AXIS;
		}

		if ( !joy_advanced.GetBool() )
		{
			// default joystick initialization
			// 2 axes only with joystick control
			user.m_rgAxes[JOY_AXIS_X].AxisMap = GAME_AXIS_YAW;
			user.m_rgAxes[JOY_AXIS_Y].AxisMap = GAME_AXIS_FORWARD;
		}
		else
		{
			if ( !bSilent && 
				hh == 0 && Q_stricmp( joy_name.GetString(), "joystick") )
			{
				// notify user of advanced controller
				Msg( "Using joystick '%s' configuration\n", joy_name.GetString() );
			}

			static SplitScreenConVarRef s_joy_movement_stick( "joy_movement_stick" );

			bool bJoyMovementStick = s_joy_movement_stick.GetBool( hh );

			// advanced initialization here
			// data supplied by user via joy_axisn cvars
			dwTemp = ( bJoyMovementStick ) ? (DWORD)joy_advaxisu.GetInt() : (DWORD)joy_advaxisx.GetInt();
			user.m_rgAxes[JOY_AXIS_X].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_X].ControlMap = dwTemp & JOY_RELATIVE_AXIS;
	
			dwTemp = ( bJoyMovementStick ) ? (DWORD)joy_advaxisr.GetInt() : (DWORD)joy_advaxisy.GetInt();
			user.m_rgAxes[JOY_AXIS_Y].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_Y].ControlMap = dwTemp & JOY_RELATIVE_AXIS;

			dwTemp = (DWORD)joy_advaxisz.GetInt();
			user.m_rgAxes[JOY_AXIS_Z].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_Z].ControlMap = dwTemp & JOY_RELATIVE_AXIS;

			dwTemp = ( bJoyMovementStick ) ? (DWORD)joy_advaxisy.GetInt() : (DWORD)joy_advaxisr.GetInt();
			user.m_rgAxes[JOY_AXIS_R].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_R].ControlMap = dwTemp & JOY_RELATIVE_AXIS;

			dwTemp = ( bJoyMovementStick ) ? (DWORD)joy_advaxisx.GetInt() : (DWORD)joy_advaxisu.GetInt();
			user.m_rgAxes[JOY_AXIS_U].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_U].ControlMap = dwTemp & JOY_RELATIVE_AXIS;
	
			dwTemp = (DWORD)joy_advaxisv.GetInt();
			user.m_rgAxes[JOY_AXIS_V].AxisMap = dwTemp & 0x0000000f;
			user.m_rgAxes[JOY_AXIS_V].ControlMap = dwTemp & JOY_RELATIVE_AXIS;

			if ( !bSilent )
			{
				Msg( "Advanced joystick settings initialized for joystick %d\n------------\n", hh + 1 );
				DescribeJoystickAxis( hh, "x axis", &user.m_rgAxes[JOY_AXIS_X] );
				DescribeJoystickAxis( hh, "y axis", &user.m_rgAxes[JOY_AXIS_Y] );
				DescribeJoystickAxis( hh, "z axis", &user.m_rgAxes[JOY_AXIS_Z] );
				DescribeJoystickAxis( hh, "r axis", &user.m_rgAxes[JOY_AXIS_R] );
				DescribeJoystickAxis( hh, "u axis", &user.m_rgAxes[JOY_AXIS_U] );
				DescribeJoystickAxis( hh, "v axis", &user.m_rgAxes[JOY_AXIS_V] );
			}
		}
	}

#if defined( SWARM_DLL )
	// If we have an xbox controller, load the cfg file if it hasn't been loaded.
	ConVarRef var( "joy_xcontroller_found" );
	if ( var.IsValid() && var.GetBool() && in_joystick.GetBool() )
	{
		if ( joy_xcontroller_cfg_loaded.GetBool() == false )
		{
			if ( IsPC() )
			{
				engine->ClientCmd( "exec 360controller_pc.cfg" );
			}
			else if ( IsX360() )
			{
				engine->ClientCmd( "exec 360controller_xbox.cfg" );
			}

			joy_xcontroller_cfg_loaded.SetValue( 1 );
		}
	}
	else if ( joy_xcontroller_cfg_loaded.GetBool() )
	{
		engine->ClientCmd( "exec undo360controller.cfg" );
		joy_xcontroller_cfg_loaded.SetValue( 0 );
	}
#else // SWARM_DLL 
	if ( IsPC() )
	{
		// If we have an xcontroller on the PC, load the cfg file if it hasn't been loaded.
		ConVarRef var( "joy_xcontroller_found" );
		if ( var.IsValid() && var.GetBool() && in_joystick.GetBool() )
		{
			if ( joy_xcontroller_cfg_loaded.GetBool() == false )
			{
				engine->ClientCmd( "exec 360controller.cfg" );
				joy_xcontroller_cfg_loaded.SetValue( 1 );
			}
		}
		else if ( joy_xcontroller_cfg_loaded.GetBool() )
		{
			engine->ClientCmd( "exec undo360controller.cfg" );
			joy_xcontroller_cfg_loaded.SetValue( 0 );
		}
	}
#endif // SWARM_DLL
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CInput::DescribeAxis( int index )
{
	switch ( index )
	{
	case GAME_AXIS_FORWARD:
		return "forward";
	case GAME_AXIS_PITCH:
		return "pitch";
	case GAME_AXIS_SIDE:
		return "strafe";
	case GAME_AXIS_YAW:
		return "yaw";
	case GAME_AXIS_NONE:
	default:
		return "n/a";
	}

	return "n/a";
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *axis - 
//			*mapping - 
//-----------------------------------------------------------------------------
void CInput::DescribeJoystickAxis( int nJoystick, char const *axis, joy_axis_t *mapping )
{
	if ( !mapping->AxisMap )
	{
		Msg( "joy%d %s:  unmapped\n", nJoystick + 1, axis );
	}
	else
	{
		Msg( "joy%d %s:  %s (%s)\n",
			nJoystick + 1,
			axis, 
			DescribeAxis( mapping->AxisMap ),
			mapping->ControlMap != 0 ? "relative" : "absolute" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Allow joystick to issue key events
// Not currently used - controller button events are pumped through the windprocs. KWD
//-----------------------------------------------------------------------------
void CInput::ControllerCommands( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: Scales the raw analog value to lie withing the axis range (full range - deadzone )
//-----------------------------------------------------------------------------
float CInput::ScaleAxisValue( const float axisValue, const float axisThreshold )
{
	// Xbox scales the range of all axes in the inputsystem. PC can't do that because each axis mapping
	// has a (potentially) unique threshold value.  If all axes were restricted to a single threshold
	// as they are on the Xbox, this function could move to inputsystem and be slightly more optimal.
	float result = 0.f;
	if ( IsPC() )
	{
		if ( axisValue < -axisThreshold )
		{
			result = ( axisValue + axisThreshold ) / ( MAX_BUTTONSAMPLE - axisThreshold );
		}
		else if ( axisValue > axisThreshold )
		{
			result = ( axisValue - axisThreshold ) / ( MAX_BUTTONSAMPLE - axisThreshold );
		}
	}
	else
	{
		// IsXbox
		result =  axisValue * ( 1.f / MAX_BUTTONSAMPLE );
	}

	return result;
}


void CInput::Joystick_SetSampleTime(float frametime)
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		m_PerUser[ i ].m_flRemainingJoystickSampleTime = frametime;
	}
}

extern void IN_ForceSpeedUp( );
extern void IN_ForceSpeedDown( );


bool CInput::ControllerModeActive( void )
{
	return ( in_joystick.GetInt() != 0 );
}

//--------------------------------------------------------------------
// See if we want to use the joystick
//--------------------------------------------------------------------
bool CInput::JoyStickActive()
{
	// verify joystick is available and that the user wants to use it
	if ( !in_joystick.GetInt() || 0 == inputsystem->GetJoystickCount() )
		return false; 

	// Skip out if vgui or gameui is active
	if ( !g_pInputStackSystem->IsTopmostEnabledContext( m_hInputContext ) )
		return false;

	return true;
}

//--------------------------------------------------------------------
// Reads joystick values
//--------------------------------------------------------------------
void CInput::JoyStickSampleAxes( float &forward, float &side, float &pitch, float &yaw, bool &bAbsoluteYaw, bool &bAbsolutePitch )
{
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	PerUserInput_t &user = GetPerUser( nSlot );

	struct axis_t
	{
		float	value;
		int		controlType;
	};
	axis_t gameAxes[ MAX_GAME_AXES ];
	memset( &gameAxes, 0, sizeof(gameAxes) );

	// Get each joystick axis value, and normalize the range
	for ( int i = 0; i < MAX_JOYSTICK_AXES; ++i )
	{
		if ( GAME_AXIS_NONE == user.m_rgAxes[i].AxisMap )
			continue;

		float fAxisValue = inputsystem->GetAnalogValue( (AnalogCode_t)JOYSTICK_AXIS( GET_ACTIVE_SPLITSCREEN_SLOT(), i ) );

		if ( joy_wwhack2.GetInt() != 0 )
		{
			// this is a special formula for the Logitech WingMan Warrior
			// y=ax^b; where a = 300 and b = 1.3
			// also x values are in increments of 800 (so this is factored out)
			// then bounds check result to level out excessively high spin rates
			float fTemp = 300.0 * pow(abs(fAxisValue) / 800.0, 1.3);
			if (fTemp > 14000.0)
				fTemp = 14000.0;
			// restore direction information
			fAxisValue = (fAxisValue > 0.0) ? fTemp : -fTemp;
		}

		unsigned int idx = user.m_rgAxes[i].AxisMap;
		gameAxes[idx].value = fAxisValue;
		gameAxes[idx].controlType = user.m_rgAxes[i].ControlMap;
	}

	// Re-map the axis values if necessary, based on the joystick configuration
	if ( (joy_advanced.GetInt() == 0) && (in_jlook.GetPerUser( nSlot ).state & 1) )
	{
		// user wants forward control to become pitch control
		gameAxes[GAME_AXIS_PITCH] = gameAxes[GAME_AXIS_FORWARD];
		gameAxes[GAME_AXIS_FORWARD].value = 0;

		// if mouse invert is on, invert the joystick pitch value
		// Note: only absolute control support here - joy_advanced = 0
		if ( m_pitch->GetFloat() < 0.0 )
		{
			gameAxes[GAME_AXIS_PITCH].value *= -1;
		}
	}

	if ( (in_strafe.GetPerUser( nSlot ).state & 1) || lookstrafe.GetFloat() && (in_jlook.GetPerUser( nSlot ).state & 1) )
	{
		// user wants yaw control to become side control
		gameAxes[GAME_AXIS_SIDE] = gameAxes[GAME_AXIS_YAW];
		gameAxes[GAME_AXIS_YAW].value = 0;
	}

	forward	= ScaleAxisValue( gameAxes[GAME_AXIS_FORWARD].value, MAX_BUTTONSAMPLE * joy_forwardthreshold.GetFloat() );
	side	= ScaleAxisValue( gameAxes[GAME_AXIS_SIDE].value, MAX_BUTTONSAMPLE * joy_sidethreshold.GetFloat()  );
	pitch	= ScaleAxisValue( gameAxes[GAME_AXIS_PITCH].value, MAX_BUTTONSAMPLE * joy_pitchthreshold.GetFloat()  );
	yaw		= ScaleAxisValue( gameAxes[GAME_AXIS_YAW].value, MAX_BUTTONSAMPLE * joy_yawthreshold.GetFloat()  );

	bAbsoluteYaw = ( JOY_ABSOLUTE_AXIS == gameAxes[GAME_AXIS_YAW].controlType );
	bAbsolutePitch = ( JOY_ABSOLUTE_AXIS == gameAxes[GAME_AXIS_PITCH].controlType );

	// If we're inverting our joystick, do so
	static SplitScreenConVarRef s_joy_inverty( "joy_inverty" );
	if ( s_joy_inverty.IsValid() && s_joy_inverty.GetBool( nSlot ) )
	{
		pitch *= -1.0f;
	}
}


//--------------------------------------------------------------------
// drive yaw, pitch and move like a screen relative platformer game
//--------------------------------------------------------------------
void CInput::JoyStickThirdPersonPlatformer( CUserCmd *cmd, float &forward, float &side, float &pitch, float &yaw )
{
	// Get starting angles
	QAngle viewangles;
	engine->GetViewAngles( viewangles );

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	PerUserInput_t &user = GetPerUser( nSlot );

	if ( forward || side )
	{
		// apply turn control [ YAW ]
		// factor in the camera offset, so that the move direction is relative to the thirdperson camera
		viewangles[ YAW ] = RAD2DEG(atan2(-side, -forward)) + user.m_vecCameraOffset[ YAW ];
		engine->SetViewAngles( viewangles );

		// apply movement
		Vector2D moveDir( forward, side );
		cmd->forwardmove += moveDir.Length() * cl_forwardspeed.GetFloat();
	}

	if ( pitch || yaw )
	{
		static SplitScreenConVarRef s_joy_yawsensitivity( "joy_yawsensitivity" );
		static SplitScreenConVarRef s_joy_pitchsensitivity( "joy_pitchsensitivity" );

		// look around with the camera
		user.m_vecCameraOffset[ PITCH ] += pitch * s_joy_pitchsensitivity.GetFloat( nSlot );
		user.m_vecCameraOffset[ YAW ]   += yaw * s_joy_yawsensitivity.GetFloat( nSlot );
	}

	if ( forward || side || pitch || yaw )
	{
		// update the ideal pitch and yaw
		cam_idealpitch.SetValue( user.m_vecCameraOffset[ PITCH ] - viewangles[ PITCH ] );
		cam_idealyaw.SetValue( user.m_vecCameraOffset[ YAW ] - viewangles[ YAW ] );
	}
}

//-----------------------------------------------
// Turns viewangles based on sampled joystick
//-----------------------------------------------
void CInput::JoyStickTurn( CUserCmd *cmd, float &yaw, float &pitch, float frametime, bool bAbsoluteYaw, bool bAbsolutePitch )
{
	// Get starting angles
	QAngle viewangles;
	engine->GetViewAngles( viewangles );

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	PerUserInput_t &user = GetPerUser( nSlot );

	static SplitScreenConVarRef s_joy_yawsensitivity( "joy_yawsensitivity" );
	static SplitScreenConVarRef s_joy_pitchsensitivity( "joy_pitchsensitivity" );

	Vector2D move( yaw, pitch );
	float dist = move.Length();
	bool	bVariableFrametime = joy_variable_frametime.GetBool();
	float	lookFrametime = bVariableFrametime ? frametime : gpGlobals->frametime;
	float   aspeed = lookFrametime * GetHud().GetFOVSensitivityAdjust();

	// apply turn control
	float angle = 0.f;

	if ( user.m_flSpinFrameTime )
	{
		// apply specified yaw velocity until duration expires
		float delta = lookFrametime;
		if ( user.m_flSpinFrameTime - delta <= 0 )
		{
			// effect expired, avoid floating point creep
			delta = user.m_flSpinFrameTime;
			user.m_flSpinFrameTime = 0;
		}
		else
		{
			user.m_flSpinFrameTime -= delta;
		}
		angle = user.m_flSpinRate * delta;
	}
	else if ( bVariableFrametime || frametime != gpGlobals->frametime )
	{
		if ( bAbsoluteYaw )
		{
			float fAxisValue = ResponseCurveLook( nSlot, joy_response_look.GetInt(), yaw, YAW, pitch, dist, lookFrametime );
			angle = fAxisValue * s_joy_yawsensitivity.GetFloat( nSlot ) * aspeed * cl_yawspeed.GetFloat();
		}
		else
		{
			angle = yaw * s_joy_yawsensitivity.GetFloat( nSlot ) * aspeed * 180.0;
		}
	}

	if ( angle )
	{
		// track angular direction
		user.m_flLastYawAngle = angle;
	}

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer( nSlot );
	if ( ( in_lookspin.GetPerUser( nSlot ).state & 2 ) && !user.m_flSpinFrameTime && pLocalPlayer && !pLocalPlayer->IsObserver() )
	{
		// user has actuated a new spin boost
		float spinFrameTime = joy_lookspin_default.GetFloat();
		user.m_flSpinFrameTime = spinFrameTime;
		// yaw velocity is in last known direction
		if ( user.m_flLastYawAngle >= 0 )
		{ 
			user.m_flSpinRate = 180.0f/spinFrameTime;
		}
		else
		{
			user.m_flSpinRate = -180.0f/spinFrameTime;
		}
	}

	viewangles[YAW] += angle;
	cmd->mousedx = angle;

	// apply look control
	if ( in_jlook.GetPerUser( nSlot ).state & 1 )
	{
		float angle = 0;
		if ( bVariableFrametime || frametime != gpGlobals->frametime )
		{
			if ( bAbsolutePitch )
			{
				float fAxisValue = ResponseCurveLook( nSlot, joy_response_look.GetInt(), pitch, PITCH, yaw, dist, lookFrametime );
				angle = fAxisValue * s_joy_pitchsensitivity.GetFloat( nSlot ) * aspeed * cl_pitchspeed.GetFloat();
			}
			else
			{
				angle = pitch * s_joy_pitchsensitivity.GetFloat( nSlot ) * aspeed * 180.0;
			}
		}
		viewangles[PITCH] += angle;
		cmd->mousedy = angle;
		view->StopPitchDrift();
		if ( pitch == 0.f && lookspring.GetFloat() == 0.f )
		{
			// no pitch movement
			// disable pitch return-to-center unless requested by user
			// *** this code can be removed when the lookspring bug is fixed
			// *** the bug always has the lookspring feature on
			view->StopPitchDrift();
		}
	}

	viewangles[PITCH] = clamp( viewangles[ PITCH ], -cl_pitchup.GetFloat(), cl_pitchdown.GetFloat() );
	engine->SetViewAngles( viewangles );
}

//---------------------------------------------------------------------
// Calculates strafe and forward/back motion based on sampled joystick
//---------------------------------------------------------------------
void CInput::JoyStickForwardSideControl( float forward, float side, float &joyForwardMove, float &joySideMove )
{
	joyForwardMove = joySideMove = 0.0f;

	// apply forward and side control
	if ( joy_response_move.GetInt() > 6 && joy_circle_correct.GetBool() )
	{
		// ok the 360 controller is scaled to a circular area.  our movement is scaled to the square two axis, 
		// so diagonal needs to be scaled properly to full speed.

		bool bInWalk = true;
		float scale = MIN(1.0f,sqrt(forward*forward+side*side));
		if ( scale > 0.01f )
		{
			float val;
			if ( scale > joy_sensitive_step2.GetFloat() )
			{
				bInWalk = false;
			}
			float scaledVal = ResponseCurve( joy_response_move.GetInt(), scale, PITCH, joy_forwardsensitivity.GetFloat() );
			val = scaledVal*(forward/scale);
			joyForwardMove += val * cl_forwardspeed.GetFloat();
			val = scaledVal*(side/scale);
			joySideMove += val * cl_sidespeed.GetFloat();
		}

		// big hack here, if we are not moving past the joy_sensitive_step2 thresh hold then walk.
		if ( bInWalk )
		{
			IN_ForceSpeedDown();
		}
		else
		{
			IN_ForceSpeedUp();
		}
	}
	else
	{
		// apply forward and side control
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

		int iResponseCurve = 0;
		if ( pLocalPlayer && pLocalPlayer->IsInAVehicle() )
		{
			iResponseCurve = pLocalPlayer->GetVehicle() ? pLocalPlayer->GetVehicle()->GetJoystickResponseCurve() : joy_response_move_vehicle.GetInt();
		}
		else
		{
			iResponseCurve = joy_response_move.GetInt();
		}	

		float val = ResponseCurve( iResponseCurve, forward, PITCH, joy_forwardsensitivity.GetFloat() );
		joyForwardMove	+= val * cl_forwardspeed.GetFloat();
		val = ResponseCurve( iResponseCurve, side, YAW, joy_sidesensitivity.GetFloat() );
		joySideMove		+= val * cl_sidespeed.GetFloat();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Apply joystick to CUserCmd creation
// Input  : frametime - 
//			*cmd - 
//-----------------------------------------------------------------------------
void CInput::JoyStickMove( float frametime, CUserCmd *cmd )
{
	// complete initialization if first time in ( needed as cvars are not available at initialization time )
	if ( !m_fJoystickAdvancedInit )
	{
		Joystick_Advanced( false );
	}

	if ( !JoyStickActive() )
		return;

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	PerUserInput_t &user = GetPerUser( nSlot );

	if ( user.m_flRemainingJoystickSampleTime <= 0 )
		return;
	frametime = MIN(user.m_flRemainingJoystickSampleTime, frametime);
	user.m_flRemainingJoystickSampleTime -= frametime;	

	float forward, side, pitch, yaw;
	bool bAbsoluteYaw, bAbsolutePitch;
	JoyStickSampleAxes( forward, side, pitch, yaw, bAbsoluteYaw, bAbsolutePitch );
		
	if ( CAM_IsThirdPerson() && thirdperson_platformer.GetInt() )
	{
		JoyStickThirdPersonPlatformer( cmd, forward, side, pitch, yaw );
		return;
	}

	float	joyForwardMove, joySideMove;
	JoyStickForwardSideControl( forward, side, joyForwardMove, joySideMove );

	JoyStickTurn( cmd, yaw, pitch, frametime, bAbsoluteYaw, bAbsolutePitch );	

	JoyStickApplyMovement( cmd, joyForwardMove, joySideMove );
}

//--------------------------------------------------------------
// Applies the calculated forward/side movement to the UserCmd
//--------------------------------------------------------------
void CInput::JoyStickApplyMovement( CUserCmd *cmd, float joyForwardMove, float joySideMove )
{
	// apply player motion relative to screen space
	if ( CAM_IsThirdPerson() && thirdperson_screenspace.GetInt() )
	{
#ifdef INFESTED_DLL
		float ideal_yaw = asw_cam_marine_yaw.GetFloat();
#else
		float ideal_yaw = cam_idealyaw.GetFloat();
#endif
		float ideal_sin = sin(DEG2RAD(ideal_yaw));
		float ideal_cos = cos(DEG2RAD(ideal_yaw));
		float side_movement = ideal_cos*joySideMove - ideal_sin*joyForwardMove;
		float forward_movement = ideal_cos*joyForwardMove + ideal_sin*joySideMove;
		cmd->forwardmove += forward_movement;
		cmd->sidemove += side_movement;
	}
	else
	{
		cmd->forwardmove += joyForwardMove;
		cmd->sidemove += joySideMove;
	}

	if ( IsPC() )
	{
		CCommand tmp;
		if ( FloatMakePositive(joyForwardMove) >= joy_autosprint.GetFloat() || FloatMakePositive(joySideMove) >= joy_autosprint.GetFloat() )
		{
			KeyDown( &in_joyspeed, NULL );
		}
		else
		{
			KeyUp( &in_joyspeed, NULL );
		}
	}
}
