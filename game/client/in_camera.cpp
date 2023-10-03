//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "cbase.h"
#include "hud.h"
#include "kbutton.h"
#include "input.h"

#include <vgui/IInput.h>
#include "vgui_controls/controls.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-------------------------------------------------- Constants

#define CAM_MIN_DIST 30.0
#define CAM_ANGLE_MOVE .5
#define MAX_ANGLE_DIFF 10.0
#define PITCH_MAX 90.0
#define PITCH_MIN 0
#define YAW_MAX  135.0
#define YAW_MIN	 -135.0
#define	DIST	 2
#define CAM_HULL_OFFSET		9.0    // the size of the bounding hull used for collision checking
static Vector CAM_HULL_MIN(-CAM_HULL_OFFSET,-CAM_HULL_OFFSET,-CAM_HULL_OFFSET);
static Vector CAM_HULL_MAX( CAM_HULL_OFFSET, CAM_HULL_OFFSET, CAM_HULL_OFFSET);

//-------------------------------------------------- Global Variables

static ConVar cam_snapto( "cam_snapto", "0", FCVAR_ARCHIVE );	 // snap to thirdperson view
static ConVar cam_ideallag( "cam_ideallag", "4.0", FCVAR_ARCHIVE, "Amount of lag used when matching offset to ideal angles in thirdperson view" );
static ConVar cam_idealdelta( "cam_idealdelta", "4.0", FCVAR_ARCHIVE, "Controls the speed when matching offset to ideal angles in thirdperson view" );
ConVar cam_idealyaw( "cam_idealyaw", "0", FCVAR_ARCHIVE );	 // thirdperson yaw
ConVar cam_idealpitch( "cam_idealpitch", "0", FCVAR_ARCHIVE );	 // thirperson pitch
ConVar cam_idealdist( "cam_idealdist", "150", FCVAR_ARCHIVE );	 // thirdperson distance
static ConVar cam_collision( "cam_collision", "1", FCVAR_ARCHIVE, "When in thirdperson and cam_collision is set to 1, an attempt is made to keep the camera from passing though walls." );
static ConVar cam_showangles( "cam_showangles", "0", FCVAR_CHEAT, "When in thirdperson, print viewangles/idealangles/cameraoffsets to the console." );
static ConVar c_maxpitch( "c_maxpitch", "90", FCVAR_ARCHIVE );
static ConVar c_minpitch( "c_minpitch", "0", FCVAR_ARCHIVE );
static ConVar c_maxyaw( "c_maxyaw",   "135", FCVAR_ARCHIVE );
static ConVar c_minyaw( "c_minyaw",   "-135", FCVAR_ARCHIVE );
static ConVar c_maxdistance( "c_maxdistance",   "200", FCVAR_ARCHIVE );
static ConVar c_mindistance( "c_mindistance",   "30", FCVAR_ARCHIVE );
static ConVar c_orthowidth( "c_orthowidth",   "100", FCVAR_ARCHIVE );
static ConVar c_orthoheight( "c_orthoheight",   "100", FCVAR_ARCHIVE );
static ConVar c_thirdpersonshoulder( "c_thirdpersonshoulder", "false", FCVAR_ARCHIVE ); // flag to indicate when we are using thirdperson-shoulder
static ConVar c_thirdpersonshoulderoffset( "c_thirdpersonshoulderoffset", "20.0", FCVAR_ARCHIVE ); // camera right offset for thirdperson-shoulder
static ConVar c_thirdpersonshoulderdist( "c_thirdpersonshoulderdist", "40.0", FCVAR_ARCHIVE ); // camera distance from the player when in thirdperson-shoulder
static ConVar c_thirdpersonshoulderheight( "c_thirdpersonshoulderheight", "5.0", FCVAR_ARCHIVE ); // camera height above the player
static ConVar c_thirdpersonshoulderaimdist( "c_thirdpersonshoulderaimdist", "120.0", FCVAR_ARCHIVE ); // the distance in front of the player to focus the camera

static kbutton_t cam_pitchup, cam_pitchdown, cam_yawleft, cam_yawright;
static kbutton_t cam_in, cam_out; // -- "cam_move" is unused

extern const ConVar *sv_cheats;
extern ConVar in_forceuser;

CON_COMMAND_F( cam_command, "Tells camera to change modes", FCVAR_CHEAT )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "cam_command <0, 1, or 2>\n" );
		return;
	}
	input->CAM_Command( Q_atoi( args.Arg( 1 ) ) );
}

// ==============================
// CAM_ToThirdPerson
// ==============================
void Cmd_CAM_ToThirdPerson(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	input->CAM_ToThirdPerson();

	// Let the local player know
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	if ( localPlayer )
	{
		localPlayer->ThirdPersonSwitch( true );
	}
}

static bool & Is_CAM_ThirdPerson_MayaMode(void)
{
	static bool s_b_CAM_ThirdPerson_MayaMode = false;
	return s_b_CAM_ThirdPerson_MayaMode;
}
void CAM_ToThirdPerson_MayaMode(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	bool &rb = Is_CAM_ThirdPerson_MayaMode();
	rb = !rb;
}

// ==============================
// Cmd_CAM_ToFirstPerson
// ==============================
void Cmd_CAM_ToFirstPerson(void) 
{ 
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	input->CAM_ToFirstPerson();

	// Let the local player know
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	if ( localPlayer )
	{
		localPlayer->ThirdPersonSwitch( false );
	}
	c_thirdpersonshoulder.SetValue( false );
	cam_idealdist.SetValue( cam_idealdist.GetDefault() );
}

/*
==============================
CAM_ToThirdPersonShoulder

==============================
*/
void Cmd_CAM_ToThirdPersonShoulder(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	input->CAM_ToThirdPersonShoulder();

}

void CInput::CAM_ToThirdPersonShoulder()
{
	PerUserInput_t &user = GetPerUser();

	if( c_thirdpersonshoulder.GetBool() )
	{
		user.m_nCamCommand = 2; // CAM_COMMAND_TOFIRSTPERSON
		c_thirdpersonshoulder.SetValue( false );
		cam_idealdist.SetValue( cam_idealdist.GetDefault() );
	}
	else
	{
		user.m_nCamCommand = 1; // CAM_COMMAND_TOTHIRDPERSON
		c_thirdpersonshoulder.SetValue( true );
		cam_idealdist.SetValue( c_thirdpersonshoulderdist.GetFloat() );
	}
}

//==============================
// Cmd_CAM_ToOrthographic
// ==============================
void Cmd_CAM_ToOrthographic(void) 
{ 
	input->CAM_ToOrthographic();
}

void CAM_StartMouseMove( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	input->CAM_StartMouseMove();
}

void CAM_EndMouseMove( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	input->CAM_EndMouseMove();
}

void CAM_StartDistance( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	input->CAM_StartDistance();
}

void CAM_EndDistance( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	input->CAM_EndDistance();
}

void CAM_ToggleSnapto( void )
{ 
	cam_snapto.SetValue( !cam_snapto.GetInt() );
}

float MoveToward( float cur, float goal, float lag )
{
	if( cur != goal )
	{
		if( abs( cur - goal ) > 180.0 )
		{
			if( cur < goal )
				cur += 360.0;
			else
				cur -= 360.0;
		}

		if( cur < goal )
		{
			if( cur < goal - 1.0 )
				cur += ( goal - cur ) / lag;
			else
				cur = goal;
		}
		else
		{
			if( cur > goal + 1.0 )
				cur -= ( cur - goal ) / lag;
			else
				cur = goal;
		}
	}


	// bring cur back into range
	if( cur < 0 )
		cur += 360.0;
	else if( cur >= 360 )
		cur -= 360;

	return cur;
}

void CInput::CAM_Command( int command )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	user.m_nCamCommand = command;
}

void CInput::CAM_Think( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();

	VPROF("CAM_Think");
	if ( user.m_pCameraThirdData )
	{
		return CAM_CameraThirdThink();
	}

	Vector idealAngles;
	Vector camOffset;
	float flSensitivity;
	QAngle viewangles;
	
	switch( user.m_nCamCommand )
	{
	case CAM_COMMAND_TOTHIRDPERSON:
		CAM_ToThirdPerson();
		break;
		
	case CAM_COMMAND_TOFIRSTPERSON:
		CAM_ToFirstPerson();
		break;
		
	case CAM_COMMAND_NONE:
	default:
		break;
	}
	
	if( !user.m_fCameraInThirdPerson )
		return;

	if ( !sv_cheats )
	{
		sv_cheats = cvar->FindVar( "sv_cheats" );
	}

	// If cheats have been disabled, pull us back out of third-person view.
	if ( sv_cheats && !sv_cheats->GetBool() && !c_thirdpersonshoulder.GetBool() )
	{
		CAM_ToFirstPerson();
		return;
	}

	C_BasePlayer* localPlayer = C_BasePlayer::GetLocalPlayer();
	// In Maya-mode
	if ( Is_CAM_ThirdPerson_MayaMode() )
	{
		// Unless explicitly moving the camera, don't move it
		user.m_fCameraInterceptingMouse = user.m_fCameraMovingWithMouse =
			vgui::input()->IsKeyDown( KEY_LALT ) || vgui::input()->IsKeyDown( KEY_RALT );
		if ( !user.m_fCameraMovingWithMouse )
			return;

		// Zero-out camera-control kbutton_t structures
		memset( &cam_pitchup, 0, sizeof( cam_pitchup ) );
		memset( &cam_pitchdown, 0, sizeof( cam_pitchdown ) );
		memset( &cam_yawleft, 0, sizeof( cam_yawleft ) );
		memset( &cam_yawright, 0, sizeof( cam_yawright ) );
		memset( &cam_in, 0, sizeof( cam_in ) );
		memset( &cam_out, 0, sizeof( cam_out ) );

		// Unless left or right mouse button is down, don't do anything
#ifndef _XBOX
		if ( /* Left+Middle Button Down */ vgui::input()->IsMouseDown( MOUSE_LEFT ) && vgui::input()->IsMouseDown( MOUSE_MIDDLE ) )
		{
			// Do only zoom in/out camera adjustment
			user.m_fCameraDistanceMove = true;
		}
		else if ( /* Left Button Down */ vgui::input()->IsMouseDown( MOUSE_LEFT ) )
		{
			// Do only rotational camera movement
			user.m_fCameraDistanceMove = false;
		}
		else if ( /* Right Button Down */ vgui::input()->IsMouseDown( MOUSE_RIGHT ) )
		{
			// Do only zoom in/out camera adjustment
			user.m_fCameraDistanceMove = true;
		}
		else
		{
			// Neither left or right buttons down, don't do anything
			ResetMouse();
			return;
		}
#endif
	}
	
	idealAngles[ PITCH ] = cam_idealpitch.GetFloat();
	idealAngles[ YAW ]   = cam_idealyaw.GetFloat();
	idealAngles[ DIST ]  = cam_idealdist.GetFloat();

	//
	//movement of the camera with the mouse
	//
	if ( user.m_fCameraMovingWithMouse )
	{
		int cpx, cpy;
#ifndef _XBOX		
		//get windows cursor position
		GetMousePos (cpx, cpy);
#else
		//xboxfixme
		cpx = cpy = 0;
#endif
		
		user.m_nCameraX = cpx;
		user.m_nCameraY = cpy;
		
		//check for X delta values and adjust accordingly
		//eventually adjust YAW based on amount of movement
		//don't do any movement of the cam using YAW/PITCH if we are zooming in/out the camera	
		if (!user.m_fCameraDistanceMove)
		{
			int x, y;
			GetWindowCenter( x,  y );
			
			//keep the camera within certain limits around the player (ie avoid certain bad viewing angles)  
			if (user.m_nCameraX>x)
			{
				//if ((idealAngles[YAW]>=225.0)||(idealAngles[YAW]<135.0))
				if (idealAngles[YAW]<c_maxyaw.GetFloat())
				{
					idealAngles[ YAW ] += (CAM_ANGLE_MOVE)*((user.m_nCameraX-x)/2);
				}
				if (idealAngles[YAW]>c_maxyaw.GetFloat())
				{
					
					idealAngles[YAW]=c_maxyaw.GetFloat();
				}
			}
			else if (user.m_nCameraX<x)
			{
				//if ((idealAngles[YAW]<=135.0)||(idealAngles[YAW]>225.0))
				if (idealAngles[YAW]>c_minyaw.GetFloat())
				{
					idealAngles[ YAW ] -= (CAM_ANGLE_MOVE)* ((x-user.m_nCameraX)/2);
					
				}
				if (idealAngles[YAW]<c_minyaw.GetFloat())
				{
					idealAngles[YAW]=c_minyaw.GetFloat();
					
				}
			}
			
			//check for y delta values and adjust accordingly
			//eventually adjust PITCH based on amount of movement
			//also make sure camera is within bounds
			if (user.m_nCameraY > y)
			{
				if(idealAngles[PITCH]<c_maxpitch.GetFloat())
				{
					idealAngles[PITCH] +=(CAM_ANGLE_MOVE)* ((user.m_nCameraY-y)/2);
				}
				if (idealAngles[PITCH]>c_maxpitch.GetFloat())
				{
					idealAngles[PITCH]=c_maxpitch.GetFloat();
				}
			}
			else if  (user.m_nCameraY<y)
			{
				if (idealAngles[PITCH]>c_minpitch.GetFloat())
				{
					idealAngles[PITCH] -= (CAM_ANGLE_MOVE)*((y-user.m_nCameraY)/2);
				}
				if (idealAngles[PITCH]<c_minpitch.GetFloat())
				{
					idealAngles[PITCH]=c_minpitch.GetFloat();
				}
			}
			
			//set old mouse coordinates to current mouse coordinates
			//since we are done with the mouse
			
			if ( ( flSensitivity = GetHud().GetSensitivity() ) != 0 )
			{
				user.m_nCameraOldX=user.m_nCameraX*flSensitivity;
				user.m_nCameraOldY=user.m_nCameraY*flSensitivity;
			}
			else
			{
				user.m_nCameraOldX=user.m_nCameraX;
				user.m_nCameraOldY=user.m_nCameraY;
			}
#ifndef _XBOX
			ResetMouse();
#endif
		}
	}
	
	//Nathan code here
	if( input->KeyState( &cam_pitchup ) )
		idealAngles[ PITCH ] += cam_idealdelta.GetFloat();
	else if( input->KeyState( &cam_pitchdown ) )
		idealAngles[ PITCH ] -= cam_idealdelta.GetFloat();
	
	if( input->KeyState( &cam_yawleft ) )
		idealAngles[ YAW ] -= cam_idealdelta.GetFloat();
	else if( input->KeyState( &cam_yawright ) )
		idealAngles[ YAW ] += cam_idealdelta.GetFloat();
	
	if( input->KeyState( &cam_in ) )
	{
		idealAngles[ DIST ] -= 2*cam_idealdelta.GetFloat();
		if( idealAngles[ DIST ] < CAM_MIN_DIST )
		{
			// If we go back into first person, reset the angle
			idealAngles[ PITCH ] = 0;
			idealAngles[ YAW ] = 0;
			idealAngles[ DIST ] = CAM_MIN_DIST;
		}
		
	}
	else if( input->KeyState( &cam_out ) )
		idealAngles[ DIST ] += 2*cam_idealdelta.GetFloat();
	
	if (user.m_fCameraDistanceMove)
	{
		int x, y;
		GetWindowCenter( x, y );

		if (user.m_nCameraY>y)
		{
			if(idealAngles[ DIST ]<c_maxdistance.GetFloat())
			{
				idealAngles[ DIST ] +=cam_idealdelta.GetFloat() * ((user.m_nCameraY-y)/2);
			}
			if (idealAngles[ DIST ]>c_maxdistance.GetFloat())
			{
				idealAngles[ DIST ]=c_maxdistance.GetFloat();
			}
		}
		else if (user.m_nCameraY<y)
		{
			if (idealAngles[ DIST ]>c_mindistance.GetFloat())
			{
				idealAngles[ DIST ] -= (cam_idealdelta.GetFloat())*((y-user.m_nCameraY)/2);
			}
			if (idealAngles[ DIST ]<c_mindistance.GetFloat())
			{
				idealAngles[ DIST ]=c_mindistance.GetFloat();
			}
		}
		//set old mouse coordinates to current mouse coordinates
		//since we are done with the mouse
		user.m_nCameraOldX=user.m_nCameraX*GetHud().GetSensitivity();
		user.m_nCameraOldY=user.m_nCameraY*GetHud().GetSensitivity();
#ifndef _XBOX
		ResetMouse();
#endif
	}

	// Obtain engine view angles and if they popped while the camera was static,
	// fix the camera angles as well
	engine->GetViewAngles( viewangles );
	static QAngle s_oldAngles = viewangles;
	if ( Is_CAM_ThirdPerson_MayaMode() && ( s_oldAngles != viewangles ) )
	{
		idealAngles[ PITCH ] += s_oldAngles[ PITCH ] - viewangles[ PITCH ];
		idealAngles[  YAW  ] += s_oldAngles[  YAW  ] - viewangles[  YAW  ];
		s_oldAngles = viewangles;
	}

	// bring the pitch values back into a range that MoveToward can handle
	if ( idealAngles[ PITCH ] > 180 )
		idealAngles[ PITCH ] -= 360;
	else if ( idealAngles[ PITCH ] < -180 )
		idealAngles[ PITCH ] += 360;

	// bring the yaw values back into a range that MoveToward can handle
	// --
	// Vitaliy: going with >= 180 and <= -180.
	// This introduces a potential discontinuity when looking directly at model face
	// as camera yaw will be jumping from +180 to -180 and back, but when working with
	// the camera allows smooth rotational transitions from left to right and back.
	// Otherwise one of the transitions that has ">"-comparison will be locked.
	// --
	if ( idealAngles[ YAW ] >= 180 )
		idealAngles[ YAW ] -= 360;
	else if ( idealAngles[ YAW ] <= -180 )
		idealAngles[ YAW ] += 360;

	// clamp pitch, yaw and dist...
	idealAngles[ PITCH ] = clamp( idealAngles[ PITCH ], c_minpitch.GetFloat(), c_maxpitch.GetFloat() );
	idealAngles[ YAW ]   = clamp( idealAngles[ YAW ], c_minyaw.GetFloat(), c_maxyaw.GetFloat() );
	idealAngles[ DIST ]  = clamp( idealAngles[ DIST ], c_mindistance.GetFloat(), c_maxdistance.GetFloat() );

	// update ideal angles
	cam_idealpitch.SetValue( idealAngles[ PITCH ] );
	cam_idealyaw.SetValue( idealAngles[ YAW ] );
	cam_idealdist.SetValue( idealAngles[ DIST ] );
	
	// Move the CameraOffset "towards" the idealAngles
	// Note: CameraOffset = viewangle + idealAngle
	VectorCopy( user.m_vecCameraOffset, camOffset );
	
	if( cam_snapto.GetInt() )
	{
		camOffset[ YAW ] = cam_idealyaw.GetFloat() + viewangles[ YAW ];
		camOffset[ PITCH ] = cam_idealpitch.GetFloat() + viewangles[ PITCH ];
		camOffset[ DIST ] = cam_idealdist.GetFloat();
	}
	else
	{
		float lag = MAX( 1, 1 + cam_ideallag.GetFloat() );

		if( camOffset[ YAW ] - viewangles[ YAW ] != cam_idealyaw.GetFloat() )
			camOffset[ YAW ] = MoveToward( AngleNormalizePositive( camOffset[ YAW ] ), AngleNormalizePositive( cam_idealyaw.GetFloat() + viewangles[ YAW ] ), lag );
		
		if( camOffset[ PITCH ] - viewangles[ PITCH ] != cam_idealpitch.GetFloat() )
			camOffset[ PITCH ] = MoveToward( camOffset[ PITCH ], cam_idealpitch.GetFloat() + viewangles[ PITCH ], lag );
		
		if( abs( camOffset[ DIST ] - cam_idealdist.GetFloat() ) < 2.0 )
			camOffset[ DIST ] = cam_idealdist.GetFloat();
		else
			camOffset[ DIST ] += ( cam_idealdist.GetFloat() - camOffset[ DIST ] ) / lag;
	}

	// move the camera closer to the player if it hit something
	if ( cam_collision.GetInt() && localPlayer )
	{
		Vector camForward;

		// find our player's origin, and from there, the eye position
		Vector origin = localPlayer->GetLocalOrigin();
		origin += localPlayer->GetViewOffset();

		// get the forward vector
		AngleVectors( QAngle(camOffset[ PITCH ], camOffset[ YAW ], 0), &camForward, NULL, NULL );

		// use our previously #defined hull to collision trace
		trace_t trace;
		CTraceFilterSimple traceFilter( localPlayer, COLLISION_GROUP_NONE );
		UTIL_TraceHull( origin, origin - (camForward * camOffset[ DIST ]),
			CAM_HULL_MIN, CAM_HULL_MAX,
			MASK_SOLID, &traceFilter, &trace );

		// move the camera closer if it hit something
		if( trace.fraction < 1.0 )
		{
			camOffset[ DIST ] *= trace.fraction;
		}

		// For now, I'd rather see the insade of a player model than punch the camera through a wall
		// might try the fade out trick at some point
		//if( camOffset[ DIST ] < CAM_MIN_DIST )
		//    camOffset[ DIST ] = CAM_MIN_DIST; // clamp up to minimum
	}

	if ( cam_showangles.GetInt() )
	{
		engine->Con_NPrintf( 4, "Pitch: %6.1f   Yaw: %6.1f %38s", viewangles[ PITCH ], viewangles[ YAW ], "view angles" );
		engine->Con_NPrintf( 6, "Pitch: %6.1f   Yaw: %6.1f   Dist: %6.1f %19s", cam_idealpitch.GetFloat(), cam_idealyaw.GetFloat(), cam_idealdist.GetFloat(), "ideal angles" );
		engine->Con_NPrintf( 8, "Pitch: %6.1f   Yaw: %6.1f   Dist: %6.1f %16s", user.m_vecCameraOffset[ PITCH ], user.m_vecCameraOffset[ YAW ],user. m_vecCameraOffset[ DIST ], "camera offset" );
	}

	user.m_vecCameraOffset[ PITCH ] = camOffset[ PITCH ];
	user.m_vecCameraOffset[ YAW ]   = camOffset[ YAW ];
	user.m_vecCameraOffset[ DIST ]  = camOffset[ DIST ];
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void ClampRange180( float &value )
{
	if ( value >= 180.0f )
	{
		value -= 360.0f;
	}
	else if ( value <= -180.0f )
	{
		value += 360.0f;
	}
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CInput::CAM_SetCameraThirdData( CameraThirdData_t *pCameraData, const QAngle &vecCameraOffset )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	user.m_pCameraThirdData = pCameraData;

	user.m_vecCameraOffset[PITCH] = vecCameraOffset[PITCH];
	user.m_vecCameraOffset[YAW] = vecCameraOffset[YAW];
	user.m_vecCameraOffset[DIST] = vecCameraOffset[DIST];
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CInput::CAM_CameraThirdThink( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	// Verify data.
	if ( !user.m_pCameraThirdData )
		return;

	// Verify that we are in third person mode.
	if( !user.m_fCameraInThirdPerson )
		return;

	// Opting out of camera movement
	if ( user.m_pCameraThirdData->m_flLag == -1.0f )
		return;

	// Obtain engine view angles and if they popped while the camera was static, fix the camera angles as well.
	QAngle angView;
	engine->GetViewAngles( angView );

	// Move the CameraOffset "towards" the idealAngles, Note: CameraOffset = viewangle + idealAngle
	Vector vecCamOffset;
	VectorCopy( user.m_vecCameraOffset, vecCamOffset );

	// Move the camera.
	float flLag = MAX( 1, 1 + user.m_pCameraThirdData->m_flLag );
	if( vecCamOffset[PITCH] - angView[PITCH] != user.m_pCameraThirdData->m_flPitch )
	{
		vecCamOffset[PITCH] = MoveToward( vecCamOffset[PITCH], ( user.m_pCameraThirdData->m_flPitch + angView[PITCH] ), flLag );
	}
	if( vecCamOffset[YAW] - angView[YAW] != user.m_pCameraThirdData->m_flYaw )
	{
		vecCamOffset[YAW] = MoveToward( vecCamOffset[YAW], ( user.m_pCameraThirdData->m_flYaw + angView[YAW] ), flLag );
	}
	if( abs( vecCamOffset[DIST] - user.m_pCameraThirdData->m_flDist ) < 2.0 )
	{
		vecCamOffset[DIST] = user.m_pCameraThirdData->m_flDist;
	}
	else
	{
		vecCamOffset[DIST] += ( user.m_pCameraThirdData->m_flDist - vecCamOffset[DIST] ) / flLag;
	}

	C_BasePlayer* pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer )
	{
		Vector vecForward;

		// Find our player's origin, and from there, the eye position.
		Vector vecOrigin = pLocalPlayer->GetLocalOrigin();
		vecOrigin += pLocalPlayer->GetViewOffset();

		// Get the forward vector
		AngleVectors( QAngle( vecCamOffset[PITCH], vecCamOffset[YAW], 0 ), &vecForward, NULL, NULL );

		// Collision trace and move the camera closer if we hit something.
		trace_t trace;
		UTIL_TraceHull( vecOrigin, vecOrigin - ( vecForward * vecCamOffset[DIST] ), user.m_pCameraThirdData->m_vecHullMin, 
			user.m_pCameraThirdData->m_vecHullMax,	MASK_SOLID, pLocalPlayer, 
			COLLISION_GROUP_NONE, &trace );

		if( trace.fraction < 1.0 )
		{
			vecCamOffset[DIST] *= trace.fraction;
		}
	}

	ClampRange180( vecCamOffset[PITCH] );
	ClampRange180( vecCamOffset[YAW] );

	user.m_vecCameraOffset[PITCH] = vecCamOffset[PITCH];
	user.m_vecCameraOffset[YAW] = vecCamOffset[YAW];
	user.m_vecCameraOffset[DIST] = vecCamOffset[DIST];
}

void CAM_PitchUpDown( const CCommand &args ) { KeyDown( &cam_pitchup, args[1] ); }
void CAM_PitchUpUp( const CCommand &args ) { KeyUp( &cam_pitchup, args[1] ); }
void CAM_PitchDownDown( const CCommand &args ) { KeyDown( &cam_pitchdown, args[1] ); }
void CAM_PitchDownUp( const CCommand &args ) { KeyUp( &cam_pitchdown, args[1] ); }
void CAM_YawLeftDown( const CCommand &args ) { KeyDown( &cam_yawleft, args[1] ); }
void CAM_YawLeftUp( const CCommand &args ) { KeyUp( &cam_yawleft, args[1] ); }
void CAM_YawRightDown( const CCommand &args ) { KeyDown( &cam_yawright, args[1] ); }
void CAM_YawRightUp( const CCommand &args ) { KeyUp( &cam_yawright, args[1] ); }
void CAM_InDown( const CCommand &args ) { KeyDown( &cam_in, args[1] ); }
void CAM_InUp( const CCommand &args ) { KeyUp( &cam_in, args[1] ); }
void CAM_OutDown( const CCommand &args ) { KeyDown( &cam_out, args[1] ); }
void CAM_OutUp( const CCommand &args ) { KeyUp( &cam_out, args[1] ); }

void CInput::CAM_ToThirdPerson(void)
{ 
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	QAngle viewangles;

	engine->GetViewAngles( viewangles );

	if( !user.m_fCameraInThirdPerson )
	{
		user.m_fCameraInThirdPerson = true; 
		
		user.m_vecCameraOffset[ YAW ] = viewangles[ YAW ]; 
		user.m_vecCameraOffset[ PITCH ] = viewangles[ PITCH ]; 
		user.m_vecCameraOffset[ DIST ] = CAM_MIN_DIST; 
	}

	user.m_nCamCommand = 0;
}

void CInput::CAM_ToFirstPerson(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	user.m_fCameraInThirdPerson = false;
	user.m_nCamCommand = 0;
}

bool CInput::CAM_IsOrthographic(void) const
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	const PerUserInput_t &user = GetPerUser();
	return user.m_CameraIsOrthographic;
}

void CInput::CAM_OrthographicSize(float& w, float& h) const
{
	w = c_orthowidth.GetFloat(); h = c_orthoheight.GetFloat();
}

void CInput::CAM_ToOrthographic(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	user.m_fCameraInThirdPerson = false;
	user.m_CameraIsOrthographic = true;
	user.m_nCamCommand = 0;
}

void CInput::CAM_StartMouseMove(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	float flSensitivity;
		
	//only move the cam with mouse if we are in third person.
	if ( user.m_fCameraInThirdPerson )
	{
		//set appropriate flags and initialize the old mouse position
		//variables for mouse camera movement
		if (!user.m_fCameraMovingWithMouse)
		{
			int cpx, cpy;

			user.m_fCameraMovingWithMouse=true;
			user.m_fCameraInterceptingMouse=true;
#ifndef _XBOX			
			GetMousePos(cpx, cpy);
#else
			// xboxfixme
			cpx = cpy = 0;
#endif
			user.m_nCameraX = cpx;
			user.m_nCameraY = cpy;

			if ( ( flSensitivity = GetHud().GetSensitivity() ) != 0 )
			{
				user.m_nCameraOldX=user.m_nCameraX*flSensitivity;
				user.m_nCameraOldY=user.m_nCameraY*flSensitivity;
			}
			else
			{
				user.m_nCameraOldX=user.m_nCameraX;
				user.m_nCameraOldY=user.m_nCameraY;
			}
		}
	}
	//we are not in 3rd person view..therefore do not allow camera movement
	else
	{   
		user.m_fCameraMovingWithMouse=false;
		user.m_fCameraInterceptingMouse=false;
	}
}

/*
==============================
CAM_EndMouseMove

the key has been released for camera movement
tell the engine that mouse camera movement is off
==============================
*/
void CInput::CAM_EndMouseMove(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	user.m_fCameraMovingWithMouse=false;
	user.m_fCameraInterceptingMouse=false;
}

/*
==============================
CAM_StartDistance

routines to start the process of moving the cam in or out 
using the mouse
==============================
*/
void CInput::CAM_StartDistance(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	//only move the cam with mouse if we are in third person.
	if ( user.m_fCameraInThirdPerson )
	{
	  //set appropriate flags and initialize the old mouse position
	  //variables for mouse camera movement
	  if (!user.m_fCameraDistanceMove)
	  {
		  int cpx, cpy;

		  user.m_fCameraDistanceMove=true;
		  user.m_fCameraMovingWithMouse=true;
		  user.m_fCameraInterceptingMouse=true;
#ifndef _XBOX
		  GetMousePos(cpx, cpy);
#else
		  // xboxfixme
		  cpx = cpy = 0;
#endif

		  user.m_nCameraX = cpx;
		  user.m_nCameraY = cpy;

		  user.m_nCameraOldX=user.m_nCameraX*GetHud().GetSensitivity();
		  user.m_nCameraOldY=user.m_nCameraY*GetHud().GetSensitivity();
	  }
	}
	//we are not in 3rd person view..therefore do not allow camera movement
	else
	{   
		user.m_fCameraDistanceMove=false;
		user.m_fCameraMovingWithMouse=false;
		user.m_fCameraInterceptingMouse=false;
	}
}

/*
==============================
CAM_EndDistance

the key has been released for camera movement
tell the engine that mouse camera movement is off
==============================
*/
void CInput::CAM_EndDistance(void)
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	user.m_fCameraDistanceMove=false;
	user.m_fCameraMovingWithMouse=false;
	user.m_fCameraInterceptingMouse=false;
}

/*
==============================
CAM_IsThirdPerson

==============================
*/
int CInput::CAM_IsThirdPerson( int nSlot /*=-1*/ )
{
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		PerUserInput_t &user = GetPerUser();
		return user.m_fCameraInThirdPerson;
	}
	return m_PerUser[ nSlot ].m_fCameraInThirdPerson;
}

/*
==============================
CAM_GetCameraOffset

==============================
*/
void CInput::CAM_GetCameraOffset( Vector& ofs )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	VectorCopy( user.m_vecCameraOffset, ofs );
}

/*
==============================
CAM_InterceptingMouse

==============================
*/
int CInput::CAM_InterceptingMouse( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	PerUserInput_t &user = GetPerUser();
	return user.m_fCameraInterceptingMouse;
}

static ConCommand startpitchup( "+campitchup", CAM_PitchUpDown );
static ConCommand endpitcup( "-campitchup", CAM_PitchUpUp );
static ConCommand startpitchdown( "+campitchdown", CAM_PitchDownDown );
static ConCommand endpitchdown( "-campitchdown", CAM_PitchDownUp );
static ConCommand startcamyawleft( "+camyawleft", CAM_YawLeftDown );
static ConCommand endcamyawleft( "-camyawleft", CAM_YawLeftUp );
static ConCommand startcamyawright( "+camyawright", CAM_YawRightDown );
static ConCommand endcamyawright( "-camyawright", CAM_YawRightUp );
static ConCommand startcamin( "+camin", CAM_InDown );
static ConCommand endcamin( "-camin", CAM_InUp );
static ConCommand startcamout( "+camout", CAM_OutDown );
static ConCommand camout( "-camout", CAM_OutUp );
#ifdef INFESTED_DLL
static ConCommand thirdperson( "thirdperson", Cmd_CAM_ToThirdPerson, "Switch to thirdperson camera." );
static ConCommand firstperson( "firstperson", Cmd_CAM_ToFirstPerson, "Switch to firstperson camera.", FCVAR_CHEAT );
#else
static ConCommand thirdperson( "thirdperson", Cmd_CAM_ToThirdPerson, "Switch to thirdperson camera.", FCVAR_CHEAT );
static ConCommand firstperson( "firstperson", Cmd_CAM_ToFirstPerson, "Switch to firstperson camera." );
#endif
static ConCommand thirdperson_mayamode( "thirdperson_mayamode", ::CAM_ToThirdPerson_MayaMode, "Switch to thirdperson Maya-like camera controls.", FCVAR_CHEAT );
static ConCommand thirdpersonshoulder( "thirdpersonshoulder", Cmd_CAM_ToThirdPersonShoulder, "Switch to thirdperson-shoulder camera." );
static ConCommand camortho( "camortho", Cmd_CAM_ToOrthographic, "Switch to orthographic camera.", FCVAR_CHEAT );
static ConCommand startcammousemove( "+cammousemove",::CAM_StartMouseMove);
static ConCommand endcammousemove( "-cammousemove",::CAM_EndMouseMove);
static ConCommand startcamdistance( "+camdistance", ::CAM_StartDistance );
static ConCommand endcamdistance( "-camdistance", ::CAM_EndDistance );
static ConCommand snapto( "snapto", CAM_ToggleSnapto );
/*
==============================
Init_Camera

==============================
*/
void CInput::Init_Camera( void )
{	
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		m_PerUser[ i ].m_CameraIsOrthographic = false;
	}
}
