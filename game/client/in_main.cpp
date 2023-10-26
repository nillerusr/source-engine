//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: builds an intended movement command to send to the server
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//


#include "cbase.h"
#include "kbutton.h"
#include "usercmd.h"
#include "in_buttons.h"
#include "input.h"
#include "iviewrender.h"
#include "iclientmode.h"
#include "prediction.h"
#include "bitbuf.h"
#include "checksum_md5.h"
#include "hltvcamera.h"
#if defined( REPLAY_ENABLED )
#include "replaycamera.h"
#endif
#include "ivieweffects.h"
#include <ctype.h> // isalnum()
#include <voice_status.h>

extern ConVar in_joystick;
extern ConVar cam_idealpitch;
extern ConVar cam_idealyaw;
#ifdef INFESTED_DLL
extern ConVar asw_cam_marine_yaw;
#endif
// For showing/hiding the scoreboard
#include <game/client/iviewport.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

int in_impulse[ MAX_SPLITSCREEN_PLAYERS ];
static int in_cancel[ MAX_SPLITSCREEN_PLAYERS ];

ConVar cl_anglespeedkey( "cl_anglespeedkey", "0.67", 0 );
ConVar cl_yawspeed( "cl_yawspeed", "210", 0 );
ConVar cl_pitchspeed( "cl_pitchspeed", "225", 0 );
ConVar cl_pitchdown( "cl_pitchdown", "89", FCVAR_CHEAT );
ConVar cl_pitchup( "cl_pitchup", "89", FCVAR_CHEAT );
ConVar cl_sidespeed( "cl_sidespeed", "450", FCVAR_CHEAT );
ConVar cl_upspeed( "cl_upspeed", "320", FCVAR_CHEAT );
ConVar cl_forwardspeed( "cl_forwardspeed", "450", FCVAR_CHEAT );
ConVar cl_backspeed( "cl_backspeed", "450", FCVAR_CHEAT );
ConVar lookspring( "lookspring", "0", FCVAR_ARCHIVE );
ConVar lookstrafe( "lookstrafe", "0", FCVAR_ARCHIVE );

void IN_JoystickChangedCallback_f( IConVar *pConVar, const char *pOldString, float flOldValue );
ConVar in_joystick( "joystick","0", FCVAR_ARCHIVE, "True if the joystick is enabled, false otherwise.", IN_JoystickChangedCallback_f );

ConVar thirdperson_platformer( "thirdperson_platformer", "0", 0, "Player will aim in the direction they are moving." );
ConVar thirdperson_screenspace( "thirdperson_screenspace", "0", 0, "Movement will be relative to the camera, eg: left means screen-left" );

ConVar sv_noclipduringpause( "sv_noclipduringpause", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "If cheats are enabled, then you can noclip with the game paused (for doing screenshots, etc.)." );
static ConVar cl_lagcomp_errorcheck( "cl_lagcomp_errorcheck", "0", 0, "Player index of other player to check for position errors." );

#if defined ( _X360 )
static ConVar option_duck_method( "option_duck_method", "1", FCVAR_ARCHIVE_XBOX | FCVAR_SS  );// 0 = HOLD to duck, 1 = Duck is a toggle
#endif

bool UsingMouselook( int nSlot ) 
{
	static SplitScreenConVarRef s_MouseLook( "cl_mouselook" );
	return s_MouseLook.GetBool( nSlot );
}

ConVar in_forceuser( "in_forceuser", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Force user input to this split screen player." );
static ConVar ss_mimic( "ss_mimic", "0", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "Split screen users mimic base player's CUserCmds" );

static void SplitScreenTeleport( int nSlot )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer( nSlot );
	if ( !pPlayer )
		return;

	Vector vecOrigin = pPlayer->GetAbsOrigin();
	QAngle angles = pPlayer->GetAbsAngles();

	int nOther = ( nSlot + 1 ) % MAX_SPLITSCREEN_PLAYERS;

	if ( C_BasePlayer::GetLocalPlayer( nOther ) )
	{
		char cmd[ 256 ];
		Q_snprintf( cmd, sizeof( cmd ), "cmd%d setpos %f %f %f;setang %f %f %f\n",
			nOther + 1,
			VectorExpand( vecOrigin ),
			VectorExpand( angles ) );
		engine->ClientCmd( cmd );
	}
}

CON_COMMAND_F( ss_teleport, "Teleport other splitscreen player to my location.", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY )
{
	SplitScreenTeleport( GET_ACTIVE_SPLITSCREEN_SLOT() );
}

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition

===============================================================================
*/

kbutton_t	in_speed;
kbutton_t	in_walk;
kbutton_t	in_jlook;
kbutton_t	in_strafe;
kbutton_t	in_commandermousemove;
kbutton_t	in_forward;
kbutton_t	in_back;
kbutton_t	in_moveleft;
kbutton_t	in_moveright;
// Display the netgraph
kbutton_t	in_graph;  
kbutton_t	in_joyspeed;		// auto-speed key from the joystick (only works for player movement, not vehicles)
kbutton_t	in_ducktoggle;
kbutton_t	in_lookspin;

kbutton_t	in_attack;
kbutton_t	in_attack2;
kbutton_t	in_zoom;

static	kbutton_t	in_klook;
static	kbutton_t	in_left;
static	kbutton_t	in_right;
static	kbutton_t	in_lookup;
static	kbutton_t	in_lookdown;
static	kbutton_t	in_use;
static	kbutton_t	in_jump;

static	kbutton_t	in_up;
static	kbutton_t	in_down;
static	kbutton_t	in_duck;
static	kbutton_t	in_reload;
static	kbutton_t	in_alt1;
static	kbutton_t	in_alt2;
static	kbutton_t	in_score;
static	kbutton_t	in_break;
static  kbutton_t   in_grenade1;
static  kbutton_t   in_grenade2;

#ifdef INFESTED_DLL
static  kbutton_t   in_currentability;
static  kbutton_t   in_prevability;
static  kbutton_t   in_nextability;
static  kbutton_t   in_ability1;
static  kbutton_t   in_ability2;
static  kbutton_t   in_ability3;
static  kbutton_t   in_ability4;
static  kbutton_t   in_ability5;
#endif

/*
===========
IN_CenterView_f
===========
*/
void IN_CenterView_f (void)
{
	QAngle viewangles;

	if ( UsingMouselook( GET_ACTIVE_SPLITSCREEN_SLOT() ) == false )
	{
		if ( !::input->CAM_InterceptingMouse() )
		{
			engine->GetViewAngles( viewangles );
			viewangles[PITCH] = 0;
			engine->SetViewAngles( viewangles );
		}
	}
}

/*
===========
IN_Joystick_Advanced_f
===========
*/
void IN_Joystick_Advanced_f (const CCommand& args)
{
	::input->Joystick_Advanced( args.ArgC() == 2 );
}


void IN_JoystickChangedCallback_f( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	::input->Joystick_Advanced( true );
}

/*
============
KB_ConvertString

Removes references to +use and replaces them with the keyname in the output string.  If
 a binding is unfound, then the original text is retained.
NOTE:  Only works for text with +word in it.
============
*/
int KB_ConvertString( char *in, char **ppout )
{
	char sz[ 4096 ];
	char binding[ 64 ];
	char *p;
	char *pOut;
	char *pEnd;
	const char *pBinding;

	if ( !ppout )
		return 0;

	*ppout = NULL;
	p = in;
	pOut = sz;
	while ( *p )
	{
		if ( *p == '+' )
		{
			pEnd = binding;
			while ( *p && ( isalnum( *p ) || ( pEnd == binding ) ) && ( ( pEnd - binding ) < 63 ) )
			{
				*pEnd++ = *p++;
			}

			*pEnd =  '\0';

			pBinding = NULL;
			if ( strlen( binding + 1 ) > 0 )
			{
				// See if there is a binding for binding?
				pBinding = engine->Key_LookupBinding( binding + 1 );
			}

			if ( pBinding )
			{
				*pOut++ = '[';
				pEnd = (char *)pBinding;
			}
			else
			{
				pEnd = binding;
			}

			while ( *pEnd )
			{
				*pOut++ = *pEnd++;
			}

			if ( pBinding )
			{
				*pOut++ = ']';
			}
		}
		else
		{
			*pOut++ = *p++;
		}
	}

	*pOut = '\0';

	int maxlen = strlen( sz ) + 1;
	pOut = ( char * )malloc( maxlen );
	Q_strncpy( pOut, sz, maxlen );
	*ppout = pOut;

	return 1;
}

/*
==============================
FindKey

Allows the engine to request a kbutton handler by name, if the key exists.
==============================
*/
kbutton_t *CInput::FindKey( const char *name )
{
	CKeyboardKey *p;
	p = m_pKeys;
	while ( p )
	{
		if ( !Q_stricmp( name, p->name ) )
		{
			return p->pkey;
		}

		p = p->next;
	}
	return NULL;
}

/*
============
AddKeyButton

Add a kbutton_t * to the list of pointers the engine can retrieve via KB_Find
============
*/
void CInput::AddKeyButton( const char *name, kbutton_t *pkb )
{
	CKeyboardKey *p;	
	kbutton_t *kb;

	kb = FindKey( name );
	
	if ( kb )
		return;

	p = new CKeyboardKey;

	Q_strncpy( p->name, name, sizeof( p->name ) );
	p->pkey = pkb;

	p->next = m_pKeys;
	m_pKeys = p;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CInput::CInput( void )
{
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		m_PerUser[ i ].m_pCommands = NULL;
		m_PerUser[ i ].m_pCameraThirdData = NULL;
		m_PerUser[ i ].m_pVerifiedCommands = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CInput::~CInput( void )
{
}

/*
============
Init_Keyboard

Add kbutton_t definitions that the engine can query if needed
============
*/
void CInput::Init_Keyboard( void )
{
	m_pKeys = NULL;

	AddKeyButton( "in_graph", &in_graph );
	AddKeyButton( "in_jlook", &in_jlook );
}

/*
============
Shutdown_Keyboard

Clear kblist
============
*/
void CInput::Shutdown_Keyboard( void )
{
	CKeyboardKey *p, *n;
	p = m_pKeys;
	while ( p )
	{
		n = p->next;
		delete p;
		p = n;
	}
	m_pKeys = NULL;
}

kbutton_t::Split_t &kbutton_t::GetPerUser( int nSlot /*=-1*/ )
{
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	}
	return m_PerUser[ nSlot ];
}

/*
============
KeyDown
============
*/
void KeyDown( kbutton_t *b, const char *c )
{
	kbutton_t::Split_t &data = b->GetPerUser();

	int		k = -1;
	if ( c && c[0] )
	{
		k = atoi(c);
	}

	if (k == data.down[0] || k == data.down[1])
		return;		// repeating key
	
	if (!data.down[0])
		data.down[0] = k;
	else if (!data.down[1])
		data.down[1] = k;
	else
	{
		if ( c[0] )
		{
			DevMsg( 1,"Three keys down for a button '%c' '%c' '%c'!\n", data.down[0], data.down[1], c);
		}
		return;
	}
	
	if (data.state & 1)
		return;		// still down
	data.state |= 1 + 2;	// down + impulse down
}

/*
============
KeyUp
============
*/
void KeyUp( kbutton_t *b, const char *c )
{	
	kbutton_t::Split_t &data = b->GetPerUser();
	if ( !c || !c[0] )
	{
		data.down[0] = data.down[1] = 0;
		data.state = 4;	// impulse up
		return;
	}

	int k = atoi(c);

	if (data.down[0] == k)
		data.down[0] = 0;
	else if (data.down[1] == k)
		data.down[1] = 0;
	else
		return;		// key up without coresponding down (menu pass through)

	if (data.down[0] || data.down[1])
	{
		//Msg ("Keys down for button: '%c' '%c' '%c' (%d,%d,%d)!\n", data.down[0], data.down[1], c, data.down[0], data.down[1], c);
		return;		// some other key is still holding it down
	}

	if (!(data.state & 1))
		return;		// still up (this should not happen)

	data.state &= ~1;		// now up
	data.state |= 4; 		// impulse up
}

void IN_ClearDuckToggle()
{
	if ( ::input->KeyState( &in_ducktoggle ) )
	{
		KeyUp( &in_ducktoggle, NULL ); 
	}
}
void IN_ForceSpeedDown( ) {KeyDown(&in_speed, NULL );}
void IN_ForceSpeedUp( ) {KeyUp(&in_speed, NULL );}
void IN_CommanderMouseMoveDown( const CCommand &args ) {KeyDown(&in_commandermousemove, args[1] );}
void IN_CommanderMouseMoveUp( const CCommand &args ) {KeyUp(&in_commandermousemove, args[1] );}
void IN_BreakDown( const CCommand &args ) { KeyDown( &in_break , args[1] );}
void IN_BreakUp( const CCommand &args )
{ 
	KeyUp( &in_break, args[1] ); 
#if defined( _DEBUG )
	DebuggerBreak();
#endif
};
void IN_KLookDown ( const CCommand &args ) {KeyDown(&in_klook, args[1] );}
void IN_KLookUp ( const CCommand &args ) {KeyUp(&in_klook, args[1] );}
void IN_JLookDown ( const CCommand &args ) {KeyDown(&in_jlook, args[1] );}
void IN_JLookUp ( const CCommand &args ) {KeyUp(&in_jlook, args[1] );}
void IN_UpDown( const CCommand &args ) {KeyDown(&in_up, args[1] );}
void IN_UpUp( const CCommand &args ) {KeyUp(&in_up, args[1] );}
void IN_DownDown( const CCommand &args ) {KeyDown(&in_down, args[1] );}
void IN_DownUp( const CCommand &args ) {KeyUp(&in_down, args[1] );}
void IN_LeftDown( const CCommand &args ) {KeyDown(&in_left, args[1] );}
void IN_LeftUp( const CCommand &args ) {KeyUp(&in_left, args[1] );}
void IN_RightDown( const CCommand &args ) {KeyDown(&in_right, args[1] );}
void IN_RightUp( const CCommand &args ) {KeyUp(&in_right, args[1] );}
void IN_ForwardDown( const CCommand &args ) {KeyDown(&in_forward, args[1] );}
void IN_ForwardUp( const CCommand &args ) {KeyUp(&in_forward, args[1] );}
void IN_BackDown( const CCommand &args ) {KeyDown(&in_back, args[1] );}
void IN_BackUp( const CCommand &args ) {KeyUp(&in_back, args[1] );}
void IN_LookupDown( const CCommand &args ) {KeyDown(&in_lookup, args[1] );}
void IN_LookupUp( const CCommand &args ) {KeyUp(&in_lookup, args[1] );}
void IN_LookdownDown( const CCommand &args ) {KeyDown(&in_lookdown, args[1] );}
void IN_LookdownUp( const CCommand &args ) {KeyUp(&in_lookdown, args[1] );}
void IN_MoveleftDown( const CCommand &args ) {KeyDown(&in_moveleft, args[1] );}
void IN_MoveleftUp( const CCommand &args ) {KeyUp(&in_moveleft, args[1] );}
void IN_MoverightDown( const CCommand &args ) {KeyDown(&in_moveright, args[1] );}
void IN_MoverightUp( const CCommand &args ) {KeyUp(&in_moveright, args[1] );}
void IN_WalkDown( const CCommand &args ) {KeyDown(&in_walk, args[1] );}
void IN_WalkUp( const CCommand &args ) {KeyUp(&in_walk, args[1] );}
void IN_SpeedDown( const CCommand &args ) {KeyDown(&in_speed, args[1] );}
void IN_SpeedUp( const CCommand &args ) {KeyUp(&in_speed, args[1] );}
void IN_StrafeDown( const CCommand &args ) {KeyDown(&in_strafe, args[1] );}
void IN_StrafeUp( const CCommand &args ) {KeyUp(&in_strafe, args[1] );}
void IN_Attack2Down( const CCommand &args ) { KeyDown(&in_attack2, args[1] );}
void IN_Attack2Up( const CCommand &args ) {KeyUp(&in_attack2, args[1] );}
void IN_UseDown ( const CCommand &args ) {KeyDown(&in_use, args[1] );}
void IN_UseUp ( const CCommand &args ) {KeyUp(&in_use, args[1] );}
void IN_JumpDown ( const CCommand &args ) {KeyDown(&in_jump, args[1] );}
void IN_JumpUp ( const CCommand &args ) 
{	
	KeyUp(&in_jump, args[1] );
}


void IN_DuckToggle( const CCommand &args ) 
{ 
	if ( ::input->KeyState(&in_ducktoggle) )
	{
		IN_ClearDuckToggle();
	}
	else
	{
		KeyDown( &in_ducktoggle, args[1] ); 
	}
}

void IN_DuckDown( const CCommand &args ) 
{
#if defined ( _X360 )
	SplitScreenConVarRef option_duck_method( "option_duck_method" );

	if ( option_duck_method.IsValid() && option_duck_method.GetBool( GET_ACTIVE_SPLITSCREEN_SLOT() ) )
	{
		IN_DuckToggle( args );
	}
	else
#endif
	{
		KeyDown(&in_duck, args[1] );
		IN_ClearDuckToggle();
	}
}
void IN_DuckUp( const CCommand &args ) 
{
#if defined ( _X360 )
	SplitScreenConVarRef option_duck_method( "option_duck_method" );

	if ( option_duck_method.IsValid() && option_duck_method.GetBool( GET_ACTIVE_SPLITSCREEN_SLOT() ) )
	{
		// intentionally blank
	}
	else
#endif
	{
		KeyUp(&in_duck, args[1] );
		IN_ClearDuckToggle();
	}
}

void IN_ReloadDown( const CCommand &args ) {KeyDown(&in_reload, args[1] );}
void IN_ReloadUp( const CCommand &args ) {KeyUp(&in_reload, args[1] );}
void IN_Alt1Down( const CCommand &args ) {KeyDown(&in_alt1, args[1] );}
void IN_Alt1Up( const CCommand &args ) {KeyUp(&in_alt1, args[1] );}
void IN_Alt2Down( const CCommand &args ) {KeyDown(&in_alt2, args[1] );}
void IN_Alt2Up( const CCommand &args ) {KeyUp(&in_alt2, args[1] );}
void IN_GraphDown( const CCommand &args ) {KeyDown(&in_graph, args[1] );}
void IN_GraphUp( const CCommand &args ) {KeyUp(&in_graph, args[1] );}
void IN_ZoomDown( const CCommand &args ) {KeyDown(&in_zoom, args[1] );}
void IN_ZoomUp( const CCommand &args ) {KeyUp(&in_zoom, args[1] );}
void IN_Grenade1Up( const CCommand &args ) { KeyUp( &in_grenade1, args[1] ); }
void IN_Grenade1Down( const CCommand &args ) { KeyDown( &in_grenade1, args[1] ); }
void IN_Grenade2Up( const CCommand &args ) { KeyUp( &in_grenade2, args[1] ); }
void IN_Grenade2Down( const CCommand &args ) { KeyDown( &in_grenade2, args[1] ); }
void IN_XboxStub( const CCommand &args ) { /*do nothing*/ }



#ifdef INFESTED_DLL
void IN_PrevAbilityUp( const CCommand &args ) { KeyUp( &in_prevability, args[1] ); }
void IN_PrevAbilityDown( const CCommand &args ) { KeyDown( &in_prevability, args[1] ); }
void IN_NextAbilityUp( const CCommand &args ) { KeyUp( &in_nextability, args[1] ); }
void IN_NextAbilityDown( const CCommand &args ) { KeyDown( &in_nextability, args[1] ); }
void IN_CurrentAbilityUp( const CCommand &args ) { KeyUp( &in_currentability, args[1] ); }
void IN_CurrentAbilityDown( const CCommand &args ) { KeyDown( &in_currentability, args[1] ); }

void IN_Ability1Up( const CCommand &args ) { KeyUp( &in_ability1, args[1] ); }
void IN_Ability1Down( const CCommand &args ) { KeyDown( &in_ability1, args[1] ); }
void IN_Ability2Up( const CCommand &args ) { KeyUp( &in_ability2, args[1] ); }
void IN_Ability2Down( const CCommand &args ) { KeyDown( &in_ability2, args[1] ); }
void IN_Ability3Up( const CCommand &args ) { KeyUp( &in_ability3, args[1] ); }
void IN_Ability3Down( const CCommand &args ) { KeyDown( &in_ability3, args[1] ); }
void IN_Ability4Up( const CCommand &args ) { KeyUp( &in_ability4, args[1] ); }
void IN_Ability4Down( const CCommand &args ) { KeyDown( &in_ability4, args[1] ); }
void IN_Ability5Up( const CCommand &args ) { KeyUp( &in_ability5, args[1] ); }
void IN_Ability5Down( const CCommand &args ) { KeyDown( &in_ability5, args[1] ); }
#endif

void IN_AttackDown( const CCommand &args )
{
	KeyDown( &in_attack, args[1] );
}

void IN_AttackUp( const CCommand &args )
{
	KeyUp( &in_attack, args[1] );
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	in_cancel[ GET_ACTIVE_SPLITSCREEN_SLOT() ] = 0;
}

// Special handling
void IN_Cancel( const CCommand &args )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	in_cancel[ GET_ACTIVE_SPLITSCREEN_SLOT() ] = 1;
}

void IN_Impulse( const CCommand &args )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	in_impulse[ GET_ACTIVE_SPLITSCREEN_SLOT() ] = atoi( args[1] );
}

void IN_ScoreDown( const CCommand &args )
{
	KeyDown( &in_score, args[1] );
	if ( GetViewPortInterface() )
	{
		GetViewPortInterface()->ShowPanel( PANEL_SCOREBOARD, true );
	}
}

void IN_ScoreUp( const CCommand &args )
{
	KeyUp( &in_score, args[1] );
	if ( GetViewPortInterface() )
	{
		GetViewPortInterface()->ShowPanel( PANEL_SCOREBOARD, false );
		GetClientVoiceMgr()->StopSquelchMode();
	}
}

void IN_LookSpinDown( const CCommand &args ) {KeyDown( &in_lookspin, args[1] );}
void IN_LookSpinUp( const CCommand &args ) {KeyUp( &in_lookspin, args[1] );}

/*
============
KeyEvent

Return 1 to allow engine to process the key, otherwise, act on it as needed
============
*/
int CInput::KeyEvent( int down, ButtonCode_t code, const char *pszCurrentBinding )
{
	// Deal with camera intercepting the mouse
	if ( ( code == MOUSE_LEFT ) || ( code == MOUSE_RIGHT ) )
	{
		if ( GetPerUser().m_fCameraInterceptingMouse )
			return 0;
	}

	if ( GetClientMode() )
		return GetClientMode()->KeyInput(down, code, pszCurrentBinding);

	return 1;
}



/*
===============
KeyState

Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
===============
*/
float CInput::KeyState ( kbutton_t *key )
{
	kbutton_t::Split_t &data = key->GetPerUser();

	float		val = 0.0;
	int			impulsedown, impulseup, down;
	
	impulsedown = data.state & 2;
	impulseup	= data.state & 4;
	down		= data.state & 1;
	
	if ( impulsedown && !impulseup )
	{
		// pressed and held this frame?
		val = down ? 0.5 : 0.0;
	}

	if ( impulseup && !impulsedown )
	{
		// released this frame?
		val = down ? 0.0 : 0.0;
	}

	if ( !impulsedown && !impulseup )
	{
		// held the entire frame?
		val = down ? 1.0 : 0.0;
	}

	if ( impulsedown && impulseup )
	{
		if ( down )
		{
			// released and re-pressed this frame
			val = 0.75;	
		}
		else
		{
			// pressed and released this frame
			val = 0.25;	
		}
	}

	// clear impulses
	data.state &= 1;		
	return val;
}

void CInput::IN_SetSampleTime( float frametime )
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		m_PerUser[ i ].m_flKeyboardSampleTime = frametime;
	}
}

/*
==============================
DetermineKeySpeed

==============================
*/
static ConVar in_usekeyboardsampletime( "in_usekeyboardsampletime", "1", 0, "Use keyboard sample time smoothing." );

float CInput::DetermineKeySpeed( int nSlot, float frametime )
{
	if ( in_usekeyboardsampletime.GetBool() )
	{
		PerUserInput_t &user = GetPerUser( nSlot );

		if ( user.m_flKeyboardSampleTime <= 0 )
			return 0.0f;
	
		frametime = MIN( user.m_flKeyboardSampleTime, frametime );
		user.m_flKeyboardSampleTime -= frametime;
	}
	
	float speed;

	speed = frametime;

	if ( in_speed.GetPerUser( nSlot ).state & 1 )
	{
		speed *= cl_anglespeedkey.GetFloat();
	}

	return speed;
}

/*
==============================
AdjustYaw

==============================
*/
void CInput::AdjustYaw( int nSlot, float speed, QAngle& viewangles )
{
	if ( !(in_strafe.GetPerUser( nSlot ).state & 1) )
	{
		viewangles[YAW] -= speed*cl_yawspeed.GetFloat() * KeyState (&in_right);
		viewangles[YAW] += speed*cl_yawspeed.GetFloat() * KeyState (&in_left);
	}

	const PerUserInput_t &user = GetPerUser( nSlot );

	// thirdperson platformer mode
	// use movement keys to aim the player relative to the thirdperson camera
	if ( CAM_IsThirdPerson() && thirdperson_platformer.GetInt() )
	{
		float side = KeyState(&in_moveleft) - KeyState(&in_moveright);
		float forward = KeyState(&in_forward) - KeyState(&in_back);

		if ( side || forward )
		{
			viewangles[YAW] = RAD2DEG(atan2(side, forward)) + user.m_vecCameraOffset[ YAW ];
		}
		if ( side || forward || KeyState (&in_right) || KeyState (&in_left) )
		{
			cam_idealyaw.SetValue( user.m_vecCameraOffset[ YAW ] - viewangles[ YAW ] );
		}
	}
}

/*
==============================
AdjustPitch

==============================
*/
void CInput::AdjustPitch( int nSlot, float speed, QAngle& viewangles )
{
	// only allow keyboard looking if mouse look is disabled
	if ( !UsingMouselook( nSlot ) )
	{
		float	up, down;

		if ( in_klook.GetPerUser( nSlot ).state & 1 )
		{
			view->StopPitchDrift ();
			viewangles[PITCH] -= speed*cl_pitchspeed.GetFloat() * KeyState (&in_forward);
			viewangles[PITCH] += speed*cl_pitchspeed.GetFloat() * KeyState (&in_back);
		}

		up		= KeyState ( &in_lookup );
		down	= KeyState ( &in_lookdown );
		
		viewangles[PITCH] -= speed*cl_pitchspeed.GetFloat() * up;
		viewangles[PITCH] += speed*cl_pitchspeed.GetFloat() * down;

		if ( up || down )
		{
			view->StopPitchDrift ();
		}
	}	
}

/*
==============================
ClampAngles

==============================
*/
void CInput::ClampAngles( QAngle& viewangles )
{
	if ( viewangles[PITCH] > cl_pitchdown.GetFloat() )
	{
		viewangles[PITCH] = cl_pitchdown.GetFloat();
	}
	if ( viewangles[PITCH] < -cl_pitchup.GetFloat() )
	{
		viewangles[PITCH] = -cl_pitchup.GetFloat();
	}


	if ( viewangles[ROLL] > 50 )
	{
		viewangles[ROLL] = 50;
	}
	if ( viewangles[ROLL] < -50 )
	{
		viewangles[ROLL] = -50;
	}

}

/*
================
AdjustAngles

Moves the local angle positions
================
*/
void CInput::AdjustAngles ( int nSlot, float frametime )
{
	float	speed;
	QAngle viewangles;
	
	// Determine control scaling factor ( multiplies time )
	speed = DetermineKeySpeed( nSlot, frametime );
	if ( speed <= 0.0f )
	{
		return;
	}

	// Retrieve latest view direction from engine
	engine->GetViewAngles( viewangles );

	// Undo tilting from previous frame
	viewangles -= GetPerUser().m_angPreviousViewAnglesTilt;

	// Apply tilting effects here (it affects aim)
	QAngle vecAnglesBeforeTilt = viewangles;
	GetViewEffects()->CalcTilt();
	GetViewEffects()->ApplyTilt( viewangles, 1.0f );

	// Remember the tilt delta so we can undo it before applying tilt next frame
	GetPerUser().m_angPreviousViewAnglesTilt = viewangles - vecAnglesBeforeTilt;

	// Adjust YAW
	AdjustYaw( nSlot, speed, viewangles );

	// Adjust PITCH if keyboard looking
	AdjustPitch( nSlot, speed, viewangles );
	
	// Make sure values are legitimate
	ClampAngles( viewangles );

	// Store new view angles into engine view direction
	engine->SetViewAngles( viewangles );
}

/*
==============================
ComputeSideMove

==============================
*/
void CInput::ComputeSideMove( int nSlot, CUserCmd *cmd )
{
	// thirdperson platformer movement
	if ( CAM_IsThirdPerson() && thirdperson_platformer.GetInt() )
	{
		// no sideways movement in this mode
		return;
	}

	// thirdperson screenspace movement
	if ( CAM_IsThirdPerson() && thirdperson_screenspace.GetInt() )
	{
#ifdef INFESTED_DLL
		float ideal_yaw = asw_cam_marine_yaw.GetFloat() - 90.0f;
#else
		float ideal_yaw = cam_idealyaw.GetFloat();
#endif
		float ideal_sin = sin(DEG2RAD(ideal_yaw));
		float ideal_cos = cos(DEG2RAD(ideal_yaw));
		
		float movement = ideal_cos*KeyState(&in_moveright)
			+  ideal_sin*KeyState(&in_back)
			+ -ideal_cos*KeyState(&in_moveleft)
			+ -ideal_sin*KeyState(&in_forward);

		cmd->sidemove += cl_sidespeed.GetFloat() * movement;

		return;
	}

	// If strafing, check left and right keys and act like moveleft and moveright keys
	if ( in_strafe.GetPerUser( nSlot ).state & 1 )
	{
		cmd->sidemove += cl_sidespeed.GetFloat() * KeyState (&in_right);
		cmd->sidemove -= cl_sidespeed.GetFloat() * KeyState (&in_left);
	}

	// Otherwise, check strafe keys
	cmd->sidemove += cl_sidespeed.GetFloat() * KeyState (&in_moveright);
	cmd->sidemove -= cl_sidespeed.GetFloat() * KeyState (&in_moveleft);
}

/*
==============================
ComputeUpwardMove

==============================
*/
void CInput::ComputeUpwardMove( int nSlot, CUserCmd *cmd )
{
	cmd->upmove += cl_upspeed.GetFloat() * KeyState (&in_up);
	cmd->upmove -= cl_upspeed.GetFloat() * KeyState (&in_down);
}

/*
==============================
ComputeForwardMove

==============================
*/
void CInput::ComputeForwardMove( int nSlot, CUserCmd *cmd )
{
	// thirdperson platformer movement
	if ( CAM_IsThirdPerson() && thirdperson_platformer.GetInt() )
	{
		// movement is always forward in this mode
		float movement = KeyState(&in_forward)
			|| KeyState(&in_moveright)
			|| KeyState(&in_back)
			|| KeyState(&in_moveleft);

		cmd->forwardmove += cl_forwardspeed.GetFloat() * movement;

		return;
	}

	// thirdperson screenspace movement
	if ( CAM_IsThirdPerson() && thirdperson_screenspace.GetInt() )
	{
#ifdef INFESTED_DLL
		float ideal_yaw = asw_cam_marine_yaw.GetFloat() - 90.0f;
#else
		float ideal_yaw = cam_idealyaw.GetFloat();
#endif
		float ideal_sin = sin(DEG2RAD(ideal_yaw));
		float ideal_cos = cos(DEG2RAD(ideal_yaw));
		
		float movement = ideal_cos*KeyState(&in_forward)
			+  ideal_sin*KeyState(&in_moveright)
			+ -ideal_cos*KeyState(&in_back)
			+ -ideal_sin*KeyState(&in_moveleft);

		cmd->forwardmove += cl_forwardspeed.GetFloat() * movement;

		return;
	}

	if ( !(in_klook.GetPerUser( nSlot ).state & 1 ) )
	{	
		cmd->forwardmove += cl_forwardspeed.GetFloat() * KeyState (&in_forward);
		cmd->forwardmove -= cl_backspeed.GetFloat() * KeyState (&in_back);
	}	
}

/*
==============================
ScaleMovements

==============================
*/
void CInput::ScaleMovements( CUserCmd *cmd )
{
	// float spd;

	// clip to maxspeed
	// FIXME FIXME:  This doesn't work
	return;

	/*
	spd = engine->GetClientMaxspeed();
	if ( spd == 0.0 )
		return;

	// Scale the speed so that the total velocity is not > spd
	float fmov = sqrt( (cmd->forwardmove*cmd->forwardmove) + (cmd->sidemove*cmd->sidemove) + (cmd->upmove*cmd->upmove) );

	if ( fmov > spd && fmov > 0.0 )
	{
		float fratio = spd / fmov;

		if ( !IsNoClipping() ) 
		{
			cmd->forwardmove	*= fratio;
			cmd->sidemove		*= fratio;
			cmd->upmove			*= fratio;
		}
	}
	*/
}


/*
===========
ControllerMove
===========
*/
void CInput::ControllerMove( int nSlot, float frametime, CUserCmd *cmd )
{
	if ( IsPC() && nSlot == in_forceuser.GetInt() )
	{
		if ( !GetPerUser( nSlot ).m_fCameraInterceptingMouse && m_fMouseActive )
		{
			MouseMove( nSlot, cmd );
		}
	}

	JoyStickMove( frametime, cmd);

	if ( IsPC() && nSlot == in_forceuser.GetInt() )
	{
		TrackIRMove(frametime, cmd);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *weapon - 
//-----------------------------------------------------------------------------
void CInput::MakeWeaponSelection( C_BaseCombatWeapon *weapon )
{
	GetPerUser().m_hSelectedWeapon = weapon;
}

CInput::PerUserInput_t &CInput::GetPerUser( int nSlot /*=-1*/ )
{
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		return m_PerUser[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
	}
	return m_PerUser[ nSlot ];
}

const CInput::PerUserInput_t &CInput::GetPerUser( int nSlot /*=-1*/ ) const
{
	if ( nSlot == -1 )
	{
		ASSERT_LOCAL_PLAYER_RESOLVABLE();
		return m_PerUser[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
	}
	return m_PerUser[ nSlot ];
}

void CInput::ExtraMouseSample( float frametime, bool active )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	static CUserCmd dummy[ MAX_SPLITSCREEN_PLAYERS ];
	CUserCmd *cmd = &dummy[ nSlot ];

	cmd->Reset();

	QAngle viewangles;

	if ( active )
	{

		// Determine view angles
		AdjustAngles ( nSlot, frametime );


		// Determine sideways movement
		ComputeSideMove( nSlot, cmd );

		// Determine vertical movement
		ComputeUpwardMove( nSlot, cmd );

		// Determine forward movement
		ComputeForwardMove( nSlot, cmd );

		// Scale based on holding speed key or having too fast of a velocity based on client maximum speed.
		ScaleMovements( cmd );

		// Allow mice and other controllers to add their inputs
		ControllerMove( nSlot, frametime, cmd );
	}

	// Retrieve view angles from engine ( could have been set in AdjustAngles above )
	engine->GetViewAngles( viewangles );

	// Set button and flag bits, don't blow away state
	cmd->buttons = GetButtonBits( false );

	// Use new view angles if alive, otherwise user last angles we stored off.
	VectorCopy( viewangles, cmd->viewangles );
	VectorCopy( viewangles, GetPerUser().m_angPreviousViewAngles );

	// Let the move manager override anything it wants to.
	if ( GetClientMode()->CreateMove( frametime, cmd ) )
	{
		// Get current view angles after the client mode tweaks with it
		engine->SetViewAngles( cmd->viewangles );
		prediction->SetLocalViewAngles( cmd->viewangles );
	}

	CheckPaused( cmd );

	CheckSplitScreenMimic( nSlot, cmd, &dummy[ 0 ] );
}

/*
================
CreateMove

Send the intended movement message to the server
if active == 1 then we are 1) not playing back demos ( where our commands are ignored ) and
2 ) we have finished signing on to server
================
*/

void CInput::CreateMove ( int sequence_number, float input_sample_frametime, bool active )
{	
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	CUserCmd *cmd = &GetPerUser( nSlot ).m_pCommands[ sequence_number % MULTIPLAYER_BACKUP];
	CVerifiedUserCmd *pVerified = &GetPerUser( nSlot ).m_pVerifiedCommands[ sequence_number % MULTIPLAYER_BACKUP];

	cmd->Reset();

	cmd->command_number = sequence_number;
	cmd->tick_count = gpGlobals->tickcount;

	QAngle viewangles;

	if ( active || sv_noclipduringpause.GetInt() )
	{
		if ( nSlot == in_forceuser.GetInt() )
		{

			// Determine view angles
			AdjustAngles ( nSlot, input_sample_frametime );


			// Determine sideways movement
			ComputeSideMove( nSlot, cmd );

			// Determine vertical movement
			ComputeUpwardMove( nSlot, cmd );

			// Determine forward movement
			ComputeForwardMove( nSlot, cmd );

			// Scale based on holding speed key or having too fast of a velocity based on client maximum
			//  speed.
			ScaleMovements( cmd );
		}

		// Allow mice and other controllers to add their inputs
		ControllerMove( nSlot, input_sample_frametime, cmd );
	}
	else
	{
		// need to run and reset mouse input so that there is no view pop when unpausing
		if ( !GetPerUser( nSlot ).m_fCameraInterceptingMouse && m_fMouseActive )
		{
			float mx, my;
			GetAccumulatedMouseDeltasAndResetAccumulators( nSlot, &mx, &my );
			ResetMouse();
		}
	}
	// Retreive view angles from engine ( could have been set in IN_AdjustAngles above )
	engine->GetViewAngles( viewangles );

	cmd->impulse = in_impulse[ nSlot ];
	in_impulse[ nSlot ] = 0;

	// Latch and clear weapon selection
	if ( GetPerUser( nSlot ).m_hSelectedWeapon != NULL )
	{
		C_BaseCombatWeapon *weapon = GetPerUser( nSlot ).m_hSelectedWeapon;

		cmd->weaponselect = weapon->entindex();
		cmd->weaponsubtype = weapon->GetSubType();

		// Always clear weapon selection
		GetPerUser( nSlot ).m_hSelectedWeapon = NULL;
	}

	// Set button and flag bits
	cmd->buttons = GetButtonBits( true );

	// Using joystick?
	if ( in_joystick.GetInt() )
	{
		if ( cmd->forwardmove > 0 )
		{
			cmd->buttons |= IN_FORWARD;
		}
		else if ( cmd->forwardmove < 0 )
		{
			cmd->buttons |= IN_BACK;
		}
	}

	// Use new view angles if alive, otherwise user last angles we stored off.
	VectorCopy( viewangles, cmd->viewangles );
	VectorCopy( viewangles, GetPerUser( nSlot ).m_angPreviousViewAngles );

	// Let the move manager override anything it wants to.
	if ( GetClientMode()->CreateMove( input_sample_frametime, cmd ) )
	{
		// Get current view angles after the client mode tweaks with it
		engine->SetViewAngles( cmd->viewangles );
	}

	CheckPaused( cmd );

	CUserCmd *pPlayer0Command = &m_PerUser[ 0 ].m_pCommands[ sequence_number % MULTIPLAYER_BACKUP ];
	CheckSplitScreenMimic( nSlot, cmd, pPlayer0Command );

	GetPerUser( nSlot ).m_flLastForwardMove = cmd->forwardmove;

	cmd->random_seed = MD5_PseudoRandom( sequence_number ) & 0x7fffffff;

	HLTVCamera()->CreateMove( cmd );
#if defined( REPLAY_ENABLED )
	ReplayCamera()->CreateMove( cmd );
#endif

#if defined( HL2_CLIENT_DLL )
	// copy backchannel data
	int i;
	for (i = 0; i < GetPerUser( nSlot ).m_EntityGroundContact.Count(); i++)
	{
		cmd->entitygroundcontact.AddToTail( GetPerUser().m_EntityGroundContact[i] );
	}
	GetPerUser( nSlot ).m_EntityGroundContact.RemoveAll();
#endif

	pVerified->m_cmd = *cmd;
	pVerified->m_crc = cmd->GetChecksum();
}

void CInput::CheckSplitScreenMimic( int nSlot, CUserCmd *cmd, CUserCmd *pPlayer0Command )
{
	// ss_mimic 2 is more of a "follow" mode
	int nMimicMode = ss_mimic.GetInt();
	if ( nMimicMode <= 0 || nSlot == 0 )
		return;

	*cmd = *pPlayer0Command;

	// We can't copy these over, since we send the weapon index it'll make the server attach the weapon to the other split screen player, even
	//  though he doesn't own it!!!
	cmd->weaponsubtype = 0;
	cmd->weaponselect = 0;

	// Get current view angles after the client mode tweaks with it
	engine->SetViewAngles( cmd->viewangles );

	int nLeader = ( nSlot + 1 ) % MAX_SPLITSCREEN_PLAYERS;

	C_BasePlayer *pLeader = C_BasePlayer::GetLocalPlayer( nLeader );
	C_BasePlayer *pFollower = C_BasePlayer::GetLocalPlayer( nSlot );

	if ( !pLeader || !pFollower )
		return;

	Vector leaderPos = pLeader->GetAbsOrigin();
	Vector followerPos = pFollower->GetAbsOrigin();

	float flFarDist = 256.0f * 256.0f;
	float flNearDist = 64.0f * 64.0f;

	Vector delta = leaderPos - followerPos;
	float flLength2DSqr = delta.Length2DSqr();
	if ( flLength2DSqr > flFarDist )
	{
		SplitScreenTeleport( nLeader );
	}
	else if ( flLength2DSqr > flNearDist )
	{
		// Run toward other guy
		cmd->buttons &= ~( IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT );

		Vector lookDir;
		Vector rightDir;
		AngleVectors( cmd->viewangles, &lookDir, &rightDir, NULL );
		lookDir.z = 0.0f;
		lookDir.NormalizeInPlace();
		Vector moveDir = delta;
		moveDir.z = 0.0f;
		moveDir.NormalizeInPlace();

		// This is the cos of the angle between them
		float fdot = lookDir.Dot( moveDir );
		float rdot = rightDir.Dot( moveDir );

		cmd->forwardmove = fdot * cl_forwardspeed.GetFloat();
		cmd->sidemove = rdot * cl_sidespeed.GetFloat();

		// We'll only be moving fwd or sideways
		cmd->upmove = 0.0f;

		if ( cmd->forwardmove > 0.0f )
		{
			cmd->buttons |= IN_FORWARD;
		}
		else if ( cmd->forwardmove < 0.0f )
		{
			cmd->buttons |= IN_BACK;
		}

		if ( cmd->sidemove > 0.0f )
		{
			cmd->buttons |= IN_MOVELEFT;
		}
		else if ( cmd->sidemove < 0.0f )
		{
			cmd->buttons |= IN_MOVERIGHT;
		}
	}
	else
	{
		// Stop movement buttons
		cmd->forwardmove = cmd->sidemove = cmd->upmove = 0.0f;
		cmd->buttons &= ~( IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT );
	}
}

void CInput::CheckPaused( CUserCmd *cmd )
{
	if ( !engine->IsPaused() )
		return;
	cmd->buttons = 0;
	cmd->forwardmove = 0;
	cmd->sidemove = 0;
	cmd->upmove = 0;

	// Don't allow changing weapons while paused
	cmd->weaponselect = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//			buffersize - 
//			slot - 
//-----------------------------------------------------------------------------
void CInput::EncodeUserCmdToBuffer( int nSlot, bf_write& buf, int sequence_number )
{
	CUserCmd nullcmd;
	CUserCmd *cmd = GetUserCmd( nSlot, sequence_number);

	WriteUsercmd( &buf, cmd, &nullcmd );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : buf - 
//			buffersize - 
//			slot - 
//-----------------------------------------------------------------------------
void CInput::DecodeUserCmdFromBuffer( int nSlot, bf_read& buf, int sequence_number )
{
	CUserCmd nullcmd;
	CUserCmd *cmd = &GetPerUser( nSlot ).m_pCommands[ sequence_number % MULTIPLAYER_BACKUP ];

	ReadUsercmd( &buf, cmd, &nullcmd );
}

void CInput::ValidateUserCmd( CUserCmd *usercmd, int sequence_number )
{
	// Validate that the usercmd hasn't been changed
	CRC32_t crc = usercmd->GetChecksum();
	if ( crc != GetPerUser().m_pVerifiedCommands[ sequence_number % MULTIPLAYER_BACKUP ].m_crc )
	{
		*usercmd = GetPerUser().m_pVerifiedCommands[ sequence_number % MULTIPLAYER_BACKUP ].m_cmd;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *buf - 
//			from - 
//			to - 
//-----------------------------------------------------------------------------
bool CInput::WriteUsercmdDeltaToBuffer( int nSlot, bf_write *buf, int from, int to, bool isnewcommand )
{
	Assert( GetPerUser( nSlot ).m_pCommands );

	CUserCmd nullcmd;

	CUserCmd *f, *t;

	int startbit = buf->GetNumBitsWritten();

	if ( from == -1 )
	{
		f = &nullcmd;
	}
	else
	{
		f = GetUserCmd( nSlot, from );

		if ( !f )
		{
			// DevMsg( "WARNING! User command delta too old (from %i, to %i)\n", from, to );
			f = &nullcmd;
		}
		else
		{
			ValidateUserCmd( f, from );
		}
	}

	t = GetUserCmd( nSlot, to );

	if ( !t )
	{
		// DevMsg( "WARNING! User command too old (from %i, to %i)\n", from, to );
		t = &nullcmd;
	}
	else
	{
		ValidateUserCmd( t, to );
	}

	// Write it into the buffer
	WriteUsercmd( buf, t, f );

	if ( buf->IsOverflowed() )
	{
		int endbit = buf->GetNumBitsWritten();

		Msg( "WARNING! User command buffer overflow(%i %i), last cmd was %i bits long\n",
			from, to,  endbit - startbit );

		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : slot - 
// Output : CUserCmd
//-----------------------------------------------------------------------------
CUserCmd *CInput::GetUserCmd( int nSlot, int sequence_number )
{
	Assert( GetPerUser( nSlot ).m_pCommands );

	CUserCmd *usercmd = &GetPerUser( nSlot ).m_pCommands[ sequence_number % MULTIPLAYER_BACKUP ];

	if ( usercmd->command_number != sequence_number )
	{
		return NULL;	// usercmd was overwritten by newer command
	}

	return usercmd;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bits - 
//			in_button - 
//			in_ignore - 
//			*button - 
//			reset - 
// Output : static void
//-----------------------------------------------------------------------------
static void CalcButtonBits( int nSlot, int& bits, int in_button, int in_ignore, kbutton_t *button, bool reset )
{
	kbutton_t::Split_t *pButtonState = &button->GetPerUser( nSlot );

	// Down or still down?
	if ( pButtonState->state & 3 )
	{
		bits |= in_button;
	}

	int clearmask = ~2;
	if ( in_ignore & in_button )
	{
		// This gets taken care of below in the GetButtonBits code
		//bits &= ~in_button;
		// Remove "still down" as well as "just down"
		clearmask = ~3;
	}

	if ( reset )
	{
		pButtonState->state &= clearmask;
	}
}

/*
============
GetButtonBits

Returns appropriate button info for keyboard and mouse state
Set bResetState to 1 to clear old state info
============
*/
int CInput::GetButtonBits( bool bResetState )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	int bits = 0;

	int ignore = GetPerUser( nSlot ).m_nClearInputState;
	CalcButtonBits( nSlot, bits, IN_SPEED, ignore, &in_speed, bResetState );
	CalcButtonBits( nSlot, bits, IN_WALK, ignore, &in_walk, bResetState );
	CalcButtonBits( nSlot, bits, IN_ATTACK, ignore, &in_attack, bResetState );
	CalcButtonBits( nSlot, bits, IN_DUCK, ignore, &in_duck, bResetState );
	CalcButtonBits( nSlot, bits, IN_JUMP, ignore, &in_jump, bResetState );
	CalcButtonBits( nSlot, bits, IN_FORWARD, ignore, &in_forward, bResetState );
	CalcButtonBits( nSlot, bits, IN_BACK, ignore, &in_back, bResetState );
	CalcButtonBits( nSlot, bits, IN_USE, ignore, &in_use, bResetState );
	CalcButtonBits( nSlot, bits, IN_LEFT, ignore, &in_left, bResetState );
	CalcButtonBits( nSlot, bits, IN_RIGHT, ignore, &in_right, bResetState );
	CalcButtonBits( nSlot, bits, IN_MOVELEFT, ignore, &in_moveleft, bResetState );
	CalcButtonBits( nSlot, bits, IN_MOVERIGHT, ignore, &in_moveright, bResetState );
	CalcButtonBits( nSlot, bits, IN_ATTACK2, ignore, &in_attack2, bResetState );
	CalcButtonBits( nSlot, bits, IN_RELOAD, ignore, &in_reload, bResetState );
	CalcButtonBits( nSlot, bits, IN_ALT1, ignore, &in_alt1, bResetState );
	CalcButtonBits( nSlot, bits, IN_ALT2, ignore, &in_alt2, bResetState );
	CalcButtonBits( nSlot, bits, IN_SCORE, ignore, &in_score, bResetState );
	CalcButtonBits( nSlot, bits, IN_ZOOM, ignore, &in_zoom, bResetState );
	CalcButtonBits( nSlot, bits, IN_GRENADE1, ignore, &in_grenade1, bResetState );
	CalcButtonBits( nSlot, bits, IN_GRENADE2, ignore, &in_grenade2, bResetState );
	CalcButtonBits( nSlot, bits, IN_LOOKSPIN, ignore, &in_lookspin, bResetState );



#ifdef INFESTED_DLL
	CalcButtonBits( nSlot, bits, IN_PREV_ABILITY, ignore, &in_prevability, bResetState );
	CalcButtonBits( nSlot, bits, IN_NEXT_ABILITY, ignore, &in_nextability, bResetState );
	CalcButtonBits( nSlot, bits, IN_CURRENT_ABILITY, ignore, &in_currentability, bResetState );
	CalcButtonBits( nSlot, bits, IN_ABILITY1, ignore, &in_ability1, bResetState );
	CalcButtonBits( nSlot, bits, IN_ABILITY2, ignore, &in_ability2, bResetState );
	CalcButtonBits( nSlot, bits, IN_ABILITY3, ignore, &in_ability3, bResetState );
	CalcButtonBits( nSlot, bits, IN_ABILITY4, ignore, &in_ability4, bResetState );
	CalcButtonBits( nSlot, bits, IN_ABILITY5, ignore, &in_ability5, bResetState );
#endif

	if ( KeyState(&in_ducktoggle) )
	{
		bits |= IN_DUCK;
	}

	if ( in_cancel[ nSlot ] )
	{
		bits |= IN_CANCEL;
	}

	if ( GetHud( nSlot ).m_iKeyBits & IN_WEAPON1 )
	{
		bits |= IN_WEAPON1;
	}

	if ( GetHud( nSlot ).m_iKeyBits & IN_WEAPON2 )
	{
		bits |= IN_WEAPON2;
	}

	// Clear out any residual
	bits &= ~ignore;

	if ( bResetState )
	{
		GetPerUser( nSlot ).m_nClearInputState = 0;
	}

	return bits;
}


//-----------------------------------------------------------------------------
// Causes an input to have to be re-pressed to become active
//-----------------------------------------------------------------------------
void CInput::ClearInputButton( int bits )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();

	if ( GET_ACTIVE_SPLITSCREEN_SLOT() != in_forceuser.GetInt() )
	{
		return;
	}

	GetPerUser().m_nClearInputState |= bits;
}


/*
==============================
GetLookSpring

==============================
*/
float CInput::GetLookSpring( void )
{
	return lookspring.GetInt();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CInput::GetLastForwardMove( void )
{
	return GetPerUser().m_flLastForwardMove;
}


#if defined( HL2_CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: back channel contact info for ground contact
// Output :
//-----------------------------------------------------------------------------

void CInput::AddIKGroundContactInfo( int entindex, float minheight, float maxheight )
{
	CEntityGroundContact data;
	data.entindex = entindex;
	data.minheight = minheight;
	data.maxheight = maxheight;

	AUTO_LOCK_FM( m_IKContactPointMutex );

	// These all route through the main player's slot!!!
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
	if ( m_PerUser[ 0 ].m_EntityGroundContact.Count() >= MAX_EDICTS )
	{
		// some overflow here, probably bogus anyway
		AssertOnce( "CInput::AddIKGroundContactInfo:  Overflow!!!" );
		m_PerUser[ 0 ].m_EntityGroundContact.RemoveAll();
		return;
	}
	m_PerUser[ 0 ].m_EntityGroundContact.AddToTail( data );
}
#endif


static ConCommand startcommandermousemove("+commandermousemove", IN_CommanderMouseMoveDown);
static ConCommand endcommandermousemove("-commandermousemove", IN_CommanderMouseMoveUp);
static ConCommand startmoveup("+moveup",IN_UpDown);
static ConCommand endmoveup("-moveup",IN_UpUp);
static ConCommand startmovedown("+movedown",IN_DownDown);
static ConCommand endmovedown("-movedown",IN_DownUp);
static ConCommand startleft("+left",IN_LeftDown);
static ConCommand endleft("-left",IN_LeftUp);
static ConCommand startright("+right",IN_RightDown);
static ConCommand endright("-right",IN_RightUp);
static ConCommand startforward("+forward",IN_ForwardDown);
static ConCommand endforward("-forward",IN_ForwardUp);
static ConCommand startback("+back",IN_BackDown);
static ConCommand endback("-back",IN_BackUp);
static ConCommand startlookup("+lookup", IN_LookupDown);
static ConCommand endlookup("-lookup", IN_LookupUp);
static ConCommand startlookdown("+lookdown", IN_LookdownDown);
static ConCommand lookdown("-lookdown", IN_LookdownUp);
static ConCommand startstrafe("+strafe", IN_StrafeDown);
static ConCommand endstrafe("-strafe", IN_StrafeUp);
static ConCommand startmoveleft("+moveleft", IN_MoveleftDown);
static ConCommand endmoveleft("-moveleft", IN_MoveleftUp);
static ConCommand startmoveright("+moveright", IN_MoverightDown);
static ConCommand endmoveright("-moveright", IN_MoverightUp);
static ConCommand startspeed("+speed", IN_SpeedDown);
static ConCommand endspeed("-speed", IN_SpeedUp);
static ConCommand startwalk("+walk", IN_WalkDown);
static ConCommand endwalk("-walk", IN_WalkUp);
static ConCommand startattack("+attack", IN_AttackDown);
static ConCommand endattack("-attack", IN_AttackUp);
static ConCommand startattack2("+attack2", IN_Attack2Down);
static ConCommand endattack2("-attack2", IN_Attack2Up);
static ConCommand startsecondary("+secondary", IN_Attack2Down);
static ConCommand endsecondary("-secondary", IN_Attack2Up);
static ConCommand startuse("+use", IN_UseDown);
static ConCommand enduse("-use", IN_UseUp);
static ConCommand startjump("+jump", IN_JumpDown);
static ConCommand endjump("-jump", IN_JumpUp);
static ConCommand impulse("impulse", IN_Impulse);
static ConCommand startklook("+klook", IN_KLookDown);
static ConCommand endklook("-klook", IN_KLookUp);
static ConCommand startjlook("+jlook", IN_JLookDown);
static ConCommand endjlook("-jlook", IN_JLookUp);
static ConCommand startduck("+duck", IN_DuckDown);
static ConCommand endduck("-duck", IN_DuckUp);
static ConCommand startreload("+reload", IN_ReloadDown);
static ConCommand endreload("-reload", IN_ReloadUp);
static ConCommand startalt1("+alt1", IN_Alt1Down);
static ConCommand endalt1("-alt1", IN_Alt1Up);
static ConCommand startalt2("+alt2", IN_Alt2Down);
static ConCommand endalt2("-alt2", IN_Alt2Up);
static ConCommand startscore("+score", IN_ScoreDown);
static ConCommand endscore("-score", IN_ScoreUp);
static ConCommand startshowscores("+showscores", IN_ScoreDown);
static ConCommand endshowscores("-showscores", IN_ScoreUp);
static ConCommand startgraph("+graph", IN_GraphDown);
static ConCommand endgraph("-graph", IN_GraphUp);
static ConCommand startbreak("+break",IN_BreakDown);
static ConCommand endbreak("-break",IN_BreakUp);
static ConCommand force_centerview("force_centerview", IN_CenterView_f);
static ConCommand joyadvancedupdate("joyadvancedupdate", IN_Joystick_Advanced_f, "", FCVAR_CLIENTCMD_CAN_EXECUTE);
static ConCommand startzoom("+zoom", IN_ZoomDown);
static ConCommand endzoom("-zoom", IN_ZoomUp);
static ConCommand endgrenade1( "-grenade1", IN_Grenade1Up );
static ConCommand startgrenade1( "+grenade1", IN_Grenade1Down );
static ConCommand endgrenade2( "-grenade2", IN_Grenade2Up );
static ConCommand startgrenade2( "+grenade2", IN_Grenade2Down );
static ConCommand startlookspin("+lookspin", IN_LookSpinDown);
static ConCommand endlookspin("-lookspin", IN_LookSpinUp);



#ifdef INFESTED_DLL
static ConCommand endprevability( "-prevability", IN_PrevAbilityUp );
static ConCommand startprevability( "+prevability", IN_PrevAbilityDown );
static ConCommand endnextability( "-nextability", IN_NextAbilityUp );
static ConCommand startnextability( "+nextability", IN_NextAbilityDown );
static ConCommand endcurrentability( "-currentability", IN_CurrentAbilityUp );
static ConCommand startcurrentability( "+currentability", IN_CurrentAbilityDown );
static ConCommand endability1( "-ability1", IN_Ability1Up );
static ConCommand startability1( "+ability1", IN_Ability1Down );
static ConCommand endability2( "-ability2", IN_Ability2Up );
static ConCommand startability2( "+ability2", IN_Ability2Down );
static ConCommand endability3( "-ability3", IN_Ability3Up );
static ConCommand startability3( "+ability3", IN_Ability3Down );
static ConCommand endability4( "-ability4", IN_Ability4Up );
static ConCommand startability4( "+ability4", IN_Ability4Down );
static ConCommand endability5( "-ability5", IN_Ability5Up );
static ConCommand startability5( "+ability5", IN_Ability5Down );
#endif

// Xbox 360 stub commands
static ConCommand xboxmove("xmove", IN_XboxStub);
static ConCommand xboxlook("xlook", IN_XboxStub);

/*
============
Init_All
============
*/
void CInput::Init_All (void)
{
	m_hInputContext = engine->GetInputContext( ENGINE_INPUT_CONTEXT_GAME );

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		Assert( !m_PerUser[ i ].m_pCommands );
		Assert( !m_PerUser[ i ].m_pVerifiedCommands );

		m_PerUser[ i ].m_pCommands = new CUserCmd[ MULTIPLAYER_BACKUP ];
		m_PerUser[ i ].m_pVerifiedCommands = new CVerifiedUserCmd[ MULTIPLAYER_BACKUP ];
	}

	m_fMouseInitialized	= false;
	m_fRestoreSPI		= false;
	m_fMouseActive		= false;
	Q_memset( m_rgOrigMouseParms, 0, sizeof( m_rgOrigMouseParms ) );
	Q_memset( m_rgNewMouseParms, 0, sizeof( m_rgNewMouseParms ) );
	Q_memset( m_rgCheckMouseParam, 0, sizeof( m_rgCheckMouseParam ) );

	m_rgNewMouseParms[ MOUSE_ACCEL_THRESHHOLD1 ] = 0; // no 2x
	m_rgNewMouseParms[ MOUSE_ACCEL_THRESHHOLD2 ] = 0; // no 4x
	m_rgNewMouseParms[ MOUSE_SPEED_FACTOR ] = 1; // slowest (10 default, 20 max)

	m_fMouseParmsValid	= false;
	m_fJoystickAdvancedInit = false;

	// Initialize inputs
	if ( IsPC() )
	{
		Init_Mouse ();
		Init_Keyboard();
	}
		
	// Initialize third person camera controls.
	Init_Camera();
	// TrackIR
	Init_TrackIR();
	// TrackIR
}

/*
============
Shutdown_All
============
*/
void CInput::Shutdown_All(void)
{
	DeactivateMouse();
	Shutdown_Keyboard();

	// TrackIR
	Shutdown_TrackIR();    
	// TrackIR

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		delete[] m_PerUser[ i ].m_pCommands;
		m_PerUser[ i ].m_pCommands = NULL;

		delete[] m_PerUser[ i ].m_pVerifiedCommands;
		m_PerUser[ i ].m_pVerifiedCommands = NULL;
	}
}

void CInput::LevelInit( void )
{
#if defined( HL2_CLIENT_DLL )
	// Remove any IK information
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetPerUser().m_EntityGroundContact.RemoveAll();
	}
#endif
}

