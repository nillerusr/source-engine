 //========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Mouse input routines
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//
#if defined( WIN32 ) && !defined( _X360 )
#define _WIN32_WINNT 0x0502
#include <windows.h>
#endif
#include "cbase.h"
#include "hud.h"
#include "cdll_int.h"
#include "kbutton.h"
#include "basehandle.h"
#include "usercmd.h"
#include "input.h"
#include "iviewrender.h"
#include "iclientmode.h"
#include "tier0/icommandline.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"
#include "vgui/Cursor.h"
#include "cdll_client_int.h"
#include "cdll_util.h"
#include "tier1/convar_serverbounded.h"
#include "cam_thirdperson.h"
#include "inputsystem/iinputsystem.h"
#include "touch.h"

// up / down
#define	PITCH	0
// left / right
#define	YAW		1

extern ConVar cl_sidespeed;
extern ConVar cl_forwardspeed;
extern ConVar sensitivity;
extern ConVar lookstrafe;
extern ConVar thirdperson_platformer;
extern ConVar touch_pitch;
extern ConVar touch_yaw;
extern ConVar default_fov;

#ifdef PORTAL
extern bool g_bUpsideDown;
#endif

ConVar touch_enable_accel( "touch_enable_accel", "0", FCVAR_ARCHIVE );
ConVar touch_accel( "touch_accel", "1.f", FCVAR_ARCHIVE );
ConVar touch_reverse( "touch_reverse", "0", FCVAR_ARCHIVE );
ConVar touch_sensitivity( "touch_sensitivity", "3.0", FCVAR_ARCHIVE, "touch look sensitivity" );

void CInput::TouchScale( float &dx, float &dy )
{
	dx *= touch_yaw.GetFloat();
	dy *= touch_pitch.GetFloat();

	float sensitivity = touch_sensitivity.GetFloat() * gHUD.GetFOVSensitivityAdjust() * ( touch_reverse.GetBool() ? -1.f : 1.f );

	if( touch_enable_accel.GetBool() )
	{
		float raw_touch_movement_distance_squared = dx * dx + dy * dy;
		float fExp = MAX(0.0f, (touch_accel.GetFloat() - 1.0f) / 2.0f);
		float accelerated_sensitivity = powf( raw_touch_movement_distance_squared, fExp ) * sensitivity;

		dx *= accelerated_sensitivity;
		dy *= accelerated_sensitivity;
	}
	else
	{
		dx *= sensitivity;
		dy *= sensitivity;
	}
}

void CInput::ApplyTouch( QAngle &viewangles, CUserCmd *cmd, float dx, float dy )
{
	viewangles[YAW] -= dx;
	viewangles[PITCH] += dy;
	cmd->mousedx = dx;
	cmd->mousedy = dy;
}

void CInput::TouchMove( CUserCmd *cmd )
{
	QAngle viewangles;
	float dx,dy,side,forward,pitch,yaw;

	engine->GetViewAngles( viewangles );

	view->StopPitchDrift();

	gTouch.GetTouchAccumulators( &side, &forward, &yaw, &pitch );

	cmd->sidemove -= cl_sidespeed.GetFloat() * side;
	cmd->forwardmove += cl_forwardspeed.GetFloat() * forward;

	gTouch.GetTouchDelta( yaw, pitch, &dx, &dy );

	TouchScale( dx, dy );

	// Let the client mode at the mouse input before it's used
	g_pClientMode->OverrideMouseInput( &dx, &dy );

	// Add mouse X/Y movement to cmd
	ApplyTouch( viewangles, cmd, dx, dy );

	// Store out the new viewangles.
	engine->SetViewAngles( viewangles );
}
