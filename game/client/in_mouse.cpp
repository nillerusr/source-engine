//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Mouse input routines
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//
#if defined( WIN32 ) && !defined( _X360 )
#include <windows.h>
#endif
#ifdef OSX
#include <Carbon/Carbon.h>
#endif
#include "hud.h"
#include "cdll_int.h"
#include "kbutton.h"
#include "basehandle.h"
#include "usercmd.h"
#include "input.h"
#include "iviewrender.h"
#include "iclientmode.h"
#include "tier0/icommandline.h"
#include "vgui/isurface.h"
#include "vgui_controls/controls.h"
#include "vgui/cursor.h"
#include "cdll_client_int.h"
#include "cdll_util.h"
#include "tier1/convar_serverbounded.h"
#include "inputsystem/iinputstacksystem.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// up / down
#define	PITCH	0
// left / right
#define	YAW		1

extern ConVar lookstrafe;
extern ConVar cl_pitchdown;
extern ConVar cl_pitchup;
extern const ConVar *sv_cheats;




class ConVar_m_pitch : public ConVar_ServerBounded
{
public:
	ConVar_m_pitch() : 
		ConVar_ServerBounded( "m_pitch","0.022", FCVAR_ARCHIVE|FCVAR_SS, "Mouse pitch factor." )
	{
	}
	
	virtual float GetFloat() const
	{
		if ( !sv_cheats )
			sv_cheats = cvar->FindVar( "sv_cheats" );

		float flBaseValue;
		// If sv_cheats is on then it can be anything.
		int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
		if ( nSlot != 0 )
		{
			static SplitScreenConVarRef s_m_pitch( "m_pitch" );
			flBaseValue = s_m_pitch.GetFloat( nSlot );
		}
		else
		{
			flBaseValue = GetBaseFloatValue();
		}
		if ( !sv_cheats || sv_cheats->GetBool() )
			return flBaseValue;

		// If sv_cheats is off than it can only be 0.022 or -0.022 (if they've reversed the mouse in the options).		
		if ( flBaseValue > 0 )
			return 0.022f;
		else
			return -0.022f;
	}
} cvar_m_pitch;
ConVar_ServerBounded *m_pitch = &cvar_m_pitch;

extern ConVar cam_idealyaw;
extern ConVar cam_idealpitch;
extern ConVar thirdperson_platformer;

static ConVar m_filter( "m_filter","0", FCVAR_ARCHIVE | FCVAR_SS, "Mouse filtering (set this to 1 to average the mouse over 2 frames)." );
ConVar sensitivity( "sensitivity","3", FCVAR_ARCHIVE, "Mouse sensitivity.", true, 0.0001f, true, 10000000 );

static ConVar m_side( "m_side","0.8", FCVAR_ARCHIVE, "Mouse side factor." );
static ConVar m_yaw( "m_yaw","0.022", FCVAR_ARCHIVE, "Mouse yaw factor." );
static ConVar m_forward( "m_forward","1", FCVAR_ARCHIVE, "Mouse forward factor." );

static ConVar m_customaccel( "m_customaccel", "0", FCVAR_ARCHIVE, "Custom mouse acceleration (0 disable, 1 to enable, 2 enable with separate yaw/pitch rescale)."\
	"\nFormula: mousesensitivity = ( rawmousedelta^m_customaccel_exponent ) * m_customaccel_scale + sensitivity"\
	"\nIf mode is 2, then x and y sensitivity are scaled by m_pitch and m_yaw respectively.", true, 0, false, 0.0f );
static ConVar m_customaccel_scale( "m_customaccel_scale", "0.04", FCVAR_ARCHIVE, "Custom mouse acceleration value.", true, 0, false, 0.0f );
static ConVar m_customaccel_max( "m_customaccel_max", "0", FCVAR_ARCHIVE, "Max mouse move scale factor, 0 for no limit" );
static ConVar m_customaccel_exponent( "m_customaccel_exponent", "1", FCVAR_ARCHIVE, "Mouse move is raised to this power before being scaled by scale factor.");

static ConVar m_mouseaccel1( "m_mouseaccel1", "0", FCVAR_ARCHIVE, "Windows mouse acceleration initial threshold (2x movement).", true, 0, false, 0.0f );
static ConVar m_mouseaccel2( "m_mouseaccel2", "0", FCVAR_ARCHIVE, "Windows mouse acceleration secondary threshold (4x movement).", true, 0, false, 0.0f );
static ConVar m_mousespeed( "m_mousespeed", "1", FCVAR_ARCHIVE, "Windows mouse speed factor (range 1 to 20).", true, 1, true, 20 );

ConVar cl_mouselook( "cl_mouselook", "1", FCVAR_ARCHIVE | FCVAR_NOT_CONNECTED | FCVAR_USERINFO | FCVAR_SS, "Set to 1 to use mouse for look, 0 for keyboard look. Cannot be set while connected to a server." );

ConVar cl_mouseenable( "cl_mouseenable", "1", FCVAR_RELEASE );


//-----------------------------------------------------------------------------
// Purpose: Hides cursor and starts accumulation/re-centering
//-----------------------------------------------------------------------------
void CInput::ActivateMouse (void)
{
	if ( m_fMouseActive )
		return;

	if ( m_fMouseInitialized )
	{
		if ( m_fMouseParmsValid )
		{
#ifdef WIN32
			m_fRestoreSPI = SystemParametersInfo (SPI_SETMOUSE, 0, m_rgNewMouseParms, 0) ? true : false;
#endif
		}
		m_fMouseActive = true;

		ResetMouse();
		g_pInputStackSystem->SetCursorIcon( m_hInputContext, INPUT_CURSOR_HANDLE_INVALID );

		// Clear accumulated error, too
		for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
		{
			GetPerUser( hh ).m_flAccumulatedMouseXMovement = 0;
			GetPerUser( hh ).m_flAccumulatedMouseYMovement = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gives back the cursor and stops centering of mouse
//-----------------------------------------------------------------------------
void CInput::DeactivateMouse (void)
{
	// This gets called whenever the mouse should be inactive. We only respond to it if we had 
	// previously activated the mouse. We'll show the cursor in here.
	if ( !m_fMouseActive )
		return;

	if ( m_fMouseInitialized )
	{
		if ( m_fRestoreSPI )
		{
#ifdef WIN32
			SystemParametersInfo( SPI_SETMOUSE, 0, m_rgOrigMouseParms, 0 );
#endif
		}
		m_fMouseActive = false;
		g_pInputStackSystem->SetCursorIcon( m_hInputContext, g_pInputSystem->GetStandardCursor( INPUT_CURSOR_ARROW ) );

		// Clear accumulated error, too
		for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
			GetPerUser().m_flAccumulatedMouseXMovement = 0;
			GetPerUser().m_flAccumulatedMouseYMovement = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInput::CheckMouseAcclerationVars()
{
	// Don't change them if the mouse is inactive, invalid, or not using parameters for restore
	if ( !m_fMouseActive ||
		 !m_fMouseInitialized || 
		 !m_fMouseParmsValid || 
		 !m_fRestoreSPI )
	{
		return;
	}

	int values[ NUM_MOUSE_PARAMS ];

	values[ MOUSE_SPEED_FACTOR ]		= m_mousespeed.GetInt();
	values[ MOUSE_ACCEL_THRESHHOLD1 ]	= m_mouseaccel1.GetInt();
	values[ MOUSE_ACCEL_THRESHHOLD2 ]	= m_mouseaccel2.GetInt();

	bool dirty = false;

	int i;
	for ( i = 0; i < NUM_MOUSE_PARAMS; i++ )
	{
		if ( !m_rgCheckMouseParam[ i ] )
			continue;

		if ( values[ i ] != m_rgNewMouseParms[ i ] )
		{
			dirty = true;
			m_rgNewMouseParms[ i ] = values[ i ];

			char const *name = "";
			switch ( i )
			{
			default:
			case MOUSE_SPEED_FACTOR:
				name = "m_mousespeed";
				break;
			case MOUSE_ACCEL_THRESHHOLD1:
				name = "m_mouseaccel1";
				break;
			case MOUSE_ACCEL_THRESHHOLD2:
				name = "m_mouseaccel2";
				break;
			}

			char sz[ 256 ];
			Q_snprintf( sz, sizeof( sz ), "Mouse parameter '%s' set to %i\n", name, values[ i ] );
			DevMsg( "%s", sz );
		}
	}

	if ( dirty )
	{
		// Update them
#ifdef WIN32
		m_fRestoreSPI = SystemParametersInfo( SPI_SETMOUSE, 0, m_rgNewMouseParms, 0 ) ? true : false;
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: One-time initialization
//-----------------------------------------------------------------------------
void CInput::Init_Mouse (void)
{
	if ( CommandLine()->FindParm("-nomouse" ) ) 
		return; 

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetPerUser().m_flPreviousMouseXPosition = 0.0f;
		GetPerUser().m_flPreviousMouseYPosition = 0.0f;
	}

	m_fMouseInitialized = true;

	m_fMouseParmsValid = false;

	if ( CommandLine()->FindParm ("-useforcedmparms" ) ) 
	{
#ifdef WIN32
		m_fMouseParmsValid = SystemParametersInfo( SPI_GETMOUSE, 0, m_rgOrigMouseParms, 0 ) ? true : false;
#else
		m_fMouseParmsValid = false;
#endif
		if ( m_fMouseParmsValid )
		{
			if ( CommandLine()->FindParm ("-noforcemspd" ) ) 
			{
				m_rgNewMouseParms[ MOUSE_SPEED_FACTOR ] = m_rgOrigMouseParms[ MOUSE_SPEED_FACTOR ];
			}
			else
			{
				m_rgCheckMouseParam[ MOUSE_SPEED_FACTOR ] = true;
			}

			if ( CommandLine()->FindParm ("-noforcemaccel" ) ) 
			{
				m_rgNewMouseParms[ MOUSE_ACCEL_THRESHHOLD1 ] = m_rgOrigMouseParms[ MOUSE_ACCEL_THRESHHOLD1 ];
				m_rgNewMouseParms[ MOUSE_ACCEL_THRESHHOLD2 ] = m_rgOrigMouseParms[ MOUSE_ACCEL_THRESHHOLD2 ];
			}
			else
			{
				m_rgCheckMouseParam[ MOUSE_ACCEL_THRESHHOLD1 ] = true;
				m_rgCheckMouseParam[ MOUSE_ACCEL_THRESHHOLD2 ] = true;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the center point of the engine window
// Input  : int&x - 
//			y - 
//-----------------------------------------------------------------------------
void CInput::GetWindowCenter( int&x, int& y )
{
	int w, h;
	engine->GetScreenSize( w, h );

	x = w >> 1;
	y = h >> 1;
}

//-----------------------------------------------------------------------------
// Purpose: Recenter the mouse
//-----------------------------------------------------------------------------
void CInput::ResetMouse( void )
{
	int x, y;
	GetWindowCenter( x,  y );
	SetMousePos( x, y );	
}


//-----------------------------------------------------------------------------
// Purpose: GetAccumulatedMouse -- the mouse can be sampled multiple times per frame and
//  these results are accumulated each time. This function gets the accumulated mouse changes and resets the accumulators
// Input  : *mx - 
//			*my - 
//-----------------------------------------------------------------------------
void CInput::GetAccumulatedMouseDeltasAndResetAccumulators( int nSlot, float *mx, float *my )
{
	Assert( mx );
	Assert( my );

	PerUserInput_t &user = GetPerUser( nSlot );

	*mx = user.m_flAccumulatedMouseXMovement;
	*my = user.m_flAccumulatedMouseYMovement;

	user.m_flAccumulatedMouseXMovement = 0;
	user.m_flAccumulatedMouseYMovement = 0;
}

//-----------------------------------------------------------------------------
// Purpose: GetMouseDelta -- Filters the mouse and stores results in old position
// Input  : mx - 
//			my - 
//			*oldx - 
//			*oldy - 
//			*x - 
//			*y - 
//-----------------------------------------------------------------------------
void CInput::GetMouseDelta( int nSlot, float inmousex, float inmousey, float *pOutMouseX, float *pOutMouseY )
{
	// Apply filtering?
	static SplitScreenConVarRef s_m_filter( "m_filter" );
	if ( s_m_filter.GetBool( nSlot ) )
	{
		// Average over last two samples
		*pOutMouseX = ( inmousex + GetPerUser().m_flPreviousMouseXPosition ) * 0.5f;
		*pOutMouseY = ( inmousey + GetPerUser().m_flPreviousMouseYPosition ) * 0.5f;
	}
	else
	{
		*pOutMouseX = inmousex;
		*pOutMouseY = inmousey;
	}

	// Latch previous
	GetPerUser().m_flPreviousMouseXPosition = inmousex;
	GetPerUser().m_flPreviousMouseYPosition = inmousey;

}

//-----------------------------------------------------------------------------
// Purpose: Multiplies mouse values by sensitivity.  Note that for windows mouse settings
//  the input x,y offsets are already scaled based on that.  The custom acceleration, therefore,
//  is totally engine-specific and applies as a post-process to allow more user tuning.
// Input  : *x - 
//			*y - 
//-----------------------------------------------------------------------------
void CInput::ScaleMouse( int nSlot, float *x, float *y )
{
	float mx = *x;
	float my = *y;

	float flHudSensitivity = GetHud( nSlot ).GetSensitivity();

	float mouse_senstivity = ( flHudSensitivity != 0 ) ? flHudSensitivity : sensitivity.GetFloat();

	if ( m_customaccel.GetBool() ) 
	{ 
		float raw_mouse_movement_distance = sqrt( mx * mx + my * my );
		float acceleration_scale = m_customaccel_scale.GetFloat();
		float accelerated_sensitivity_max = m_customaccel_max.GetFloat();
		float accelerated_sensitivity_exponent = m_customaccel_exponent.GetFloat();
		float accelerated_sensitivity = ( (float)pow( raw_mouse_movement_distance, accelerated_sensitivity_exponent ) * acceleration_scale + mouse_senstivity );

		if ( accelerated_sensitivity_max > 0.0001f && 
			accelerated_sensitivity > accelerated_sensitivity_max )
		{
			accelerated_sensitivity = accelerated_sensitivity_max;
		}

		*x *= accelerated_sensitivity; 
		*y *= accelerated_sensitivity; 

		// Further re-scale by yaw and pitch magnitude if user requests alternate mode 2
		// This means that they will need to up their value for m_customaccel_scale greatly (>40x) since m_pitch/yaw default
		//  to 0.022
		if ( m_customaccel.GetInt() == 2 )
		{ 
			*x *= m_yaw.GetFloat(); 
			*y *= m_pitch->GetFloat(); 
		} 
	}
	else
	{ 
		*x *= mouse_senstivity;
		*y *= mouse_senstivity;
	}
}

//-----------------------------------------------------------------------------
// Purpose: ApplyMouse -- applies mouse deltas to CUserCmd
// Input  : viewangles - 
//			*cmd - 
//			mouse_x - 
//			mouse_y - 
//-----------------------------------------------------------------------------
void CInput::ApplyMouse( int nSlot, QAngle& viewangles, CUserCmd *cmd, float mouse_x, float mouse_y )
{
	PerUserInput_t &user = GetPerUser( nSlot );

	//roll the view angles so roll is 0 (the HL2 assumed state) and mouse adjustments are relative to the screen.
	//Assuming roll is unchanging, we want mouse left to translate to screen left at all times (same for right, up, and down)
	


	if ( !((in_strafe.GetPerUser( nSlot ).state & 1) || lookstrafe.GetInt()) )
	{
		if ( CAM_IsThirdPerson() && thirdperson_platformer.GetInt() )
		{
			if ( mouse_x )
			{
				// use the mouse to orbit the camera around the player, and update the idealAngle
				user.m_vecCameraOffset[ YAW ] -= m_yaw.GetFloat() * mouse_x;
				cam_idealyaw.SetValue( user.m_vecCameraOffset[ YAW ] - viewangles[ YAW ] );

				// why doesn't this work??? CInput::AdjustYaw is why
				//cam_idealyaw.SetValue( cam_idealyaw.GetFloat() - m_yaw.GetFloat() * mouse_x );
			}
		}
		else
		{
			// Otherwize, use mouse to spin around vertical axis


			{
				viewangles[YAW] -= m_yaw.GetFloat() * mouse_x;
			}
		}
	}
	else
	{
		// If holding strafe key or mlooking and have lookstrafe set to true, then apply
		//  horizontal mouse movement to sidemove.
		cmd->sidemove += m_side.GetFloat() * mouse_x;
	}

	// If mouselooking and not holding strafe key, then use vertical mouse
	//  to adjust view pitch.
	if (!(in_strafe.GetPerUser( nSlot ).state & 1))
	{
		if ( CAM_IsThirdPerson() && thirdperson_platformer.GetInt() )
		{
			if ( mouse_y )
			{
				// use the mouse to orbit the camera around the player, and update the idealAngle
				user.m_vecCameraOffset[ PITCH ] += m_pitch->GetFloat() * mouse_y;
				cam_idealpitch.SetValue( user.m_vecCameraOffset[ PITCH ] - viewangles[ PITCH ] );

				// why doesn't this work??? CInput::AdjustYaw is why
				//cam_idealpitch.SetValue( cam_idealpitch.GetFloat() + m_pitch->GetFloat() * mouse_y );
			}
		}
		else
		{

			{
				viewangles[PITCH] += m_pitch->GetFloat() * mouse_y;
			}

			// Check pitch bounds
			if (viewangles[PITCH] > cl_pitchdown.GetFloat())
			{
				viewangles[PITCH] = cl_pitchdown.GetFloat();
			}
			if (viewangles[PITCH] < -cl_pitchup.GetFloat())
			{
				viewangles[PITCH] = -cl_pitchup.GetFloat();
			}
		}		
	}
	else
	{
		// Otherwise if holding strafe key and noclipping, then move upward
/*		if ((in_strafe.state & 1) && IsNoClipping() )
		{
			cmd->upmove -= m_forward.GetFloat() * mouse_y;
		} 
		else */
		{
			// Default is to apply vertical mouse movement as a forward key press.
			cmd->forwardmove -= m_forward.GetFloat() * mouse_y;
		}
	}

	// Finally, add mouse state to usercmd.
	// NOTE:  Does rounding to int cause any issues?  ywb 1/17/04
	cmd->mousedx = (int)mouse_x;
	cmd->mousedy = (int)mouse_y;
}

extern bool UsingMouselook( int nSlot );

//-----------------------------------------------------------------------------
// Purpose: AccumulateMouse
//-----------------------------------------------------------------------------
void CInput::AccumulateMouse( int nSlot )
{
	if( !cl_mouseenable.GetBool() )
	{
		return;
	}

	if( !UsingMouselook( nSlot ) )
	{
		return;
	}

	int w, h;
	engine->GetScreenSize( w, h );

	// x,y = screen center
	int x = w >> 1;
	int y = h >> 1;

	// Clamp
	if ( m_fMouseActive )
	{
		int ox, oy;
		GetMousePos( ox, oy );
		ox = clamp( ox, 0, w - 1 );
		oy = clamp( oy, 0, h - 1 );
		SetMousePos( ox, oy );
	}

	//only accumulate mouse if we are not moving the camera with the mouse
	PerUserInput_t &user = GetPerUser( nSlot );

	if ( !user.m_fCameraInterceptingMouse && vgui::surface()->IsCursorLocked() )
	{
		//Assert( !vgui::surface()->IsCursorVisible() );

#ifdef WIN32
		int current_posx, current_posy;

		GetMousePos(current_posx, current_posy);

		user.m_flAccumulatedMouseXMovement += current_posx - x;
		user.m_flAccumulatedMouseYMovement += current_posy - y;

		// force the mouse to the center, so there's room to move
		ResetMouse();
#elif defined(OSX)
		CGMouseDelta deltaX, deltaY;
		CGGetLastMouseDelta( &deltaX, &deltaY );
		user.m_flAccumulatedMouseXMovement += deltaX;
		user.m_flAccumulatedMouseYMovement += deltaY;
#else
#error
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get raw mouse position
// Input  : &ox - 
//			&oy - 
//-----------------------------------------------------------------------------
void CInput::GetMousePos(int &ox, int &oy)
{
	g_pInputSystem->GetCursorPosition( &ox, &oy );
}

//-----------------------------------------------------------------------------
// Purpose: Force raw mouse position
// Input  : x - 
//			y - 
//-----------------------------------------------------------------------------
void CInput::SetMousePos( int x, int y )
{
	g_pInputStackSystem->SetCursorPosition( m_hInputContext, x, y );
}

//-----------------------------------------------------------------------------
// Purpose: MouseMove -- main entry point for applying mouse
// Input  : *cmd - 
//-----------------------------------------------------------------------------
void CInput::MouseMove( int nSlot, CUserCmd *cmd )
{
	float	mouse_x, mouse_y;
	float	mx, my;
	QAngle	viewangles;

	// Get view angles from engine
	engine->GetViewAngles( viewangles );

	// Validate mouse speed/acceleration settings
	CheckMouseAcclerationVars();

	// Don't drift pitch at all while mouselooking.
	view->StopPitchDrift ();

	//jjb - this disables normal mouse control if the user is trying to 
	//      move the camera, or if the mouse cursor is visible 
	if ( !GetPerUser( nSlot ).m_fCameraInterceptingMouse && g_pInputStackSystem->IsTopmostEnabledContext( m_hInputContext ) )
	{
		// Sample mouse one more time
		AccumulateMouse( nSlot );

		// Latch accumulated mouse movements and reset accumulators
		GetAccumulatedMouseDeltasAndResetAccumulators( nSlot, &mx, &my );

		// Filter, etc. the delta values and place into mouse_x and mouse_y
		GetMouseDelta( nSlot, mx, my, &mouse_x, &mouse_y );

		// Apply scaling factor
		ScaleMouse( nSlot, &mouse_x, &mouse_y );

		// Let the client mode at the mouse input before it's used
		GetClientMode()->OverrideMouseInput( &mouse_x, &mouse_y );

		// Add mouse X/Y movement to cmd
		ApplyMouse( nSlot, viewangles, cmd, mouse_x, mouse_y );

		// Re-center the mouse.
		ResetMouse();
	}

	// Store out the new viewangles.
	engine->SetViewAngles( viewangles );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mx - 
//			*my - 
//			*unclampedx - 
//			*unclampedy - 
//-----------------------------------------------------------------------------
void CInput::GetFullscreenMousePos( int *mx, int *my, int *unclampedx /*=NULL*/, int *unclampedy /*=NULL*/ )
{
	Assert( mx );
	Assert( my );

#if !(INFESTED_DLL)
	if ( g_pInputStackSystem->IsTopmostEnabledContext( m_hInputContext ) )
		return;
#endif

	int x, y;
	GetWindowCenter( x,  y );

	int		current_posx, current_posy;

	GetMousePos(current_posx, current_posy);

	current_posx -= x;
	current_posy -= y;

	// Now need to add back in mid point of viewport
	//

	int w, h;
	vgui::surface()->GetScreenSize( w, h );
	current_posx += w  / 2;
	current_posy += h / 2;

	if ( unclampedx )
	{
		*unclampedx = current_posx;
	}

	if ( unclampedy )
	{
		*unclampedy = current_posy;
	}

	// Clamp
	current_posx = MAX( 0, current_posx );
	current_posx = MIN( ScreenWidth(), current_posx );

	current_posy = MAX( 0, current_posy );
	current_posy = MIN( ScreenHeight(), current_posy );

	*mx = current_posx;
	*my = current_posy;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mx - 
//			my - 
//-----------------------------------------------------------------------------
void CInput::SetFullscreenMousePos( int mx, int my )
{
	SetMousePos( mx, my );
}

//-----------------------------------------------------------------------------
// Purpose: ClearStates -- Resets mouse accumulators so you don't get a pop when returning to trapped mouse
//-----------------------------------------------------------------------------
void CInput::ClearStates (void)
{
	if ( !m_fMouseActive )
		return;

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetPerUser().m_flAccumulatedMouseXMovement = 0;
		GetPerUser().m_flAccumulatedMouseYMovement = 0;
	}
}