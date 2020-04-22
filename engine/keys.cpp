//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "keys.h"
#include "cdll_engine_int.h"
#include "cmd.h"
#include "toolframework/itoolframework.h"
#include "toolframework/itoolsystem.h"
#include "tier1/utlbuffer.h"
#include "vgui_baseui_interface.h"
#include "tier2/tier2.h"
#include "inputsystem/iinputsystem.h"
#include "cheatcodes.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


enum KeyUpTarget_t
{
	KEY_UP_ANYTARGET = 0,
	KEY_UP_ENGINE,
	KEY_UP_VGUI,
	KEY_UP_TOOLS,
	KEY_UP_CLIENT,
};

struct KeyInfo_t
{
    char			*m_pKeyBinding;
	unsigned char	m_nKeyUpTarget : 3;	// see KeyUpTarget_t
	unsigned char	m_bKeyDown : 1;
};


//-----------------------------------------------------------------------------
// Current keypress state
//-----------------------------------------------------------------------------
static KeyInfo_t	s_pKeyInfo[BUTTON_CODE_LAST];


//-----------------------------------------------------------------------------
// Trap mode is used by the keybinding UI
//-----------------------------------------------------------------------------
static bool			s_bTrapMode = false;
static bool			s_bDoneTrapping = false;
static ButtonCode_t	s_nTrapKeyUp = BUTTON_CODE_INVALID;
static ButtonCode_t	s_nTrapKey = BUTTON_CODE_INVALID;


//-----------------------------------------------------------------------------
// Can keys be passed to various targets?
//-----------------------------------------------------------------------------
static inline bool ShouldPassKeyUpToTarget( ButtonCode_t code, KeyUpTarget_t target )
{
	return ( s_pKeyInfo[code].m_nKeyUpTarget == target ) || ( s_pKeyInfo[code].m_nKeyUpTarget == KEY_UP_ANYTARGET );
}

/*
===================
Key_SetBinding
===================
*/
void Key_SetBinding( ButtonCode_t keynum, const char *pBinding )
{
	char	*pNewBinding;
	int		l;
			
	if ( keynum == BUTTON_CODE_INVALID )
		return;

	// free old bindings
	if ( s_pKeyInfo[keynum].m_pKeyBinding )
	{
		// Exactly the same, don't re-bind and fragment memory
		if ( !Q_strcmp( s_pKeyInfo[keynum].m_pKeyBinding, pBinding ) )
			return;

		delete[] s_pKeyInfo[keynum].m_pKeyBinding;
		s_pKeyInfo[keynum].m_pKeyBinding = NULL;
	}
			
	// allocate memory for new binding
	l = Q_strlen( pBinding );	
	pNewBinding = (char *)new char[ l+1 ];
	Q_strncpy( pNewBinding, pBinding, l + 1 );
	pNewBinding[l] = 0;
	s_pKeyInfo[keynum].m_pKeyBinding = pNewBinding;	
}

/*
===================
Key_Unbind_f
===================
*/
CON_COMMAND_F( unbind, "Unbind a key.", FCVAR_DONTRECORD )
{
	ButtonCode_t b;

	if ( args.ArgC() != 2 )
	{
		ConMsg( "unbind <key> : remove commands from a key\n" );
		return;
	}
	
	b = g_pInputSystem->StringToButtonCode( args[1] );
	if ( b == BUTTON_CODE_INVALID )
	{
		ConMsg( "\"%s\" isn't a valid key\n", args[1] );
		return;
	}
	if ( b == KEY_ESCAPE )
	{
		ConMsg( "Can't unbind ESCAPE key\n" );
		return;
	}

	Key_SetBinding( b, "" );
}


CON_COMMAND_F( unbind_mac, "Unbind a key on the Mac only.", FCVAR_DONTRECORD )
{
	if ( IsOSX() )
	{
		ButtonCode_t b;
		
		if ( args.ArgC() != 2 )
		{
			ConMsg( "unbind <key> : remove commands from a key\n" );
			return;
		}
		
		b = g_pInputSystem->StringToButtonCode( args[1] );
		if ( b == BUTTON_CODE_INVALID )
		{
			ConMsg( "\"%s\" isn't a valid key\n", args[1] );
			return;
		}
		if ( b == KEY_ESCAPE )
		{
			ConMsg( "Can't unbind ESCAPE key\n" );
			return;
		}
		
		Key_SetBinding( b, "" );
	}
}



CON_COMMAND_F( unbindall, "Unbind all keys.", FCVAR_DONTRECORD )
{
	int		i;
	
	for ( i=0; i<BUTTON_CODE_LAST; i++ )
	{
		if ( !s_pKeyInfo[i].m_pKeyBinding )
			continue;
		
		// Don't ever unbind escape or console key
		if ( i == KEY_ESCAPE )	
			continue;

		if ( i == KEY_BACKQUOTE )
			continue;

		Key_SetBinding( (ButtonCode_t)i, "" );
	}
}

#ifndef SWDS
CON_COMMAND_F( escape, "Escape key pressed.", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	EngineVGui()->HideGameUI();
}
#endif

/*
===================
Key_Bind_f
===================
*/

void BindKey( const char *pchBind, bool bShow, const char *pchCmd )
{		
	if ( !g_pInputSystem )
		return;

	ButtonCode_t b = g_pInputSystem->StringToButtonCode( pchBind );
	if ( b == BUTTON_CODE_INVALID )
	{
		ConMsg( "\"%s\" isn't a valid key\n", pchBind );
		return;
	}
	
	if ( bShow )
	{
		if (s_pKeyInfo[b].m_pKeyBinding)
		{
			ConMsg( "\"%s\" = \"%s\"\n", pchBind, s_pKeyInfo[b].m_pKeyBinding );
		}
		else
		{
			ConMsg( "\"%s\" is not bound\n", pchBind );
		}
		return;
	}
	
	if ( b == KEY_ESCAPE )
	{
		pchCmd = "cancelselect";
	}
	
	Key_SetBinding( b, pchCmd );
}


CON_COMMAND_F( bind, "Bind a key.", FCVAR_DONTRECORD )
{
	int i, c;
	char cmd[1024];
	
	c = args.ArgC();

	if ( c != 2 && c != 3 )
	{
		ConMsg( "bind <key> [command] : attach a command to a key\n" );
		return;
	}
	
	// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	for ( i=2 ; i< c ; i++ )
	{
		if (i > 2)
		{
			Q_strncat( cmd, " ", sizeof( cmd ), COPY_ALL_CHARACTERS );
		}
		Q_strncat( cmd, args[i], sizeof( cmd ), COPY_ALL_CHARACTERS );
	}

	BindKey( args[1], c == 2, cmd );
}

CON_COMMAND_F( bind_mac, "Bind this key but only on Mac, not win32", FCVAR_DONTRECORD )
{
	if ( IsOSX() )
	{
		int i, c;
		char cmd[1024];
		
		c = args.ArgC();
		
		if ( c != 2 && c != 3 )
		{
			ConMsg( "bind <key> [command] : attach a command to a key\n" );
			return;
		}
		
		// copy the rest of the command line
		cmd[0] = 0;		// start out with a null string
		for ( i=2 ; i< c ; i++ )
		{
			if (i > 2)
			{
				Q_strncat( cmd, " ", sizeof( cmd ), COPY_ALL_CHARACTERS );
			}
			Q_strncat( cmd, args[i], sizeof( cmd ), COPY_ALL_CHARACTERS );
		}
		
		BindKey( args[1], c == 2, cmd );
	}
	
}
	


/*
============
Key_CountBindings

Count number of lines of bindings we'll be writing
============
*/
int  Key_CountBindings( void )
{
	int		i;
	int		c = 0;

	for ( i = 0; i < BUTTON_CODE_LAST; i++ )
	{
		if ( !s_pKeyInfo[i].m_pKeyBinding || !s_pKeyInfo[i].m_pKeyBinding[0] )
			continue;

		c++;
	}

	return c;
}

/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void Key_WriteBindings( CUtlBuffer &buf )
{
	int		i;

	for ( i = 0 ; i < BUTTON_CODE_LAST ; i++ )
	{
		if ( !s_pKeyInfo[i].m_pKeyBinding || !s_pKeyInfo[i].m_pKeyBinding[0] )
			continue;

		buf.Printf( "bind \"%s\" \"%s\"\n", g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i ), s_pKeyInfo[i].m_pKeyBinding );
	}
}


/*
============
Key_NameForBinding

Returns the keyname to which a binding string is bound.  E.g., if 
TAB is bound to +use then searching for +use will return "TAB"
============
*/
const char *Key_NameForBinding( const char *pBinding )
{
	int i;

	const char *pBind = pBinding;
	if ( pBinding[0] == '+' )
	{
		++pBind;
	}

	for (i=0 ; i<BUTTON_CODE_LAST ; i++)
	{
		if (s_pKeyInfo[i].m_pKeyBinding)
		{
			if (*s_pKeyInfo[i].m_pKeyBinding)
			{
				if ( s_pKeyInfo[i].m_pKeyBinding[0] == '+' )
				{
					if ( !Q_strcasecmp( s_pKeyInfo[i].m_pKeyBinding+1, (char *)pBind ) )
						return g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i );
				}
				else
				{
					if ( !Q_strcasecmp( s_pKeyInfo[i].m_pKeyBinding, (char *)pBind ) )
						return g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i );
				}

			}
		}
	}

	// Xbox 360 controller: Handle the dual bindings for duck and zoom
	if ( !Q_stricmp( "duck", pBind ) )
		return Key_NameForBinding( "toggle_duck" );

	if ( !Q_stricmp( "zoom", pBind ) )
		return Key_NameForBinding( "toggle_zoom" );

	return NULL;
}

/*
============
Key_NameForBinding

Returns the keyname to which a binding string is bound.  E.g., if 
TAB is bound to +use then searching for +use will return "TAB"

Does not perform "helpful" removal of '+' character from bindings.
============
*/
const char *Key_NameForBindingExact( const char *pBinding )
{
	int i;

	for (i=0 ; i<BUTTON_CODE_LAST ; i++)
	{
		if (s_pKeyInfo[i].m_pKeyBinding)
		{
			if (*s_pKeyInfo[i].m_pKeyBinding)
			{
				if ( !Q_strcasecmp( s_pKeyInfo[i].m_pKeyBinding, pBinding ) )
					return g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i );
			}
		}
	}

	return NULL;
}

const char *Key_BindingForKey( ButtonCode_t code )
{
	if ( code < 0 || code > BUTTON_CODE_LAST )
		return NULL;

	if ( !s_pKeyInfo[ code ].m_pKeyBinding )
		return NULL;

	return s_pKeyInfo[ code ].m_pKeyBinding;
}

CON_COMMAND( key_listboundkeys, "List bound keys with bindings." )
{
	int i;

	for (i=0 ; i<BUTTON_CODE_LAST ; i++)
	{
		const char *pBinding = Key_BindingForKey( (ButtonCode_t)i );
		if ( !pBinding || !pBinding[0] )
			continue;

		ConMsg( "\"%s\" = \"%s\"\n", g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i ), pBinding );
	}
}

CON_COMMAND( key_findbinding, "Find key bound to specified command string." )
{
	if ( args.ArgC() != 2 )
	{
		ConMsg( "usage:  key_findbinding substring\n" );
		return;
	}

	const char *substring = args[1];
	if ( !substring || !substring[ 0 ] )
	{
		ConMsg( "usage:  key_findbinding substring\n" );
		return;
	}

	int i;

	for (i=0 ; i<BUTTON_CODE_LAST ; i++)
	{
		const char *pBinding = Key_BindingForKey( (ButtonCode_t)i );
		if ( !pBinding || !pBinding[0] )
			continue;

		if ( Q_strstr( pBinding, substring ) )
		{
			ConMsg( "\"%s\" = \"%s\"\n",
				g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i ), pBinding );
		}
	}
}


//-----------------------------------------------------------------------------
// Initialization, shutdown 
//-----------------------------------------------------------------------------
void Key_Init (void)
{
	ReadCheatCommandsFromFile( "scripts/cheatcodes.txt" );
	ReadCheatCommandsFromFile( "scripts/mod_cheatcodes.txt" );
}

void Key_Shutdown( void )
{
	for ( int i = 0; i < ARRAYSIZE( s_pKeyInfo ); ++i )
	{
		delete[] s_pKeyInfo[ i ].m_pKeyBinding;
		s_pKeyInfo[ i ].m_pKeyBinding = NULL;
	}

	ClearCheatCommands();
}


//-----------------------------------------------------------------------------
// Purpose: Starts trap mode (used for keybinding UI)
//-----------------------------------------------------------------------------
void Key_StartTrapMode( void )
{
	if ( s_bTrapMode )
		return;

	Assert( !s_bDoneTrapping && s_nTrapKeyUp == BUTTON_CODE_INVALID );

	s_bDoneTrapping = false;
	s_bTrapMode = true;
	s_nTrapKeyUp = BUTTON_CODE_INVALID;
}


//-----------------------------------------------------------------------------
// We're done trapping once the first key is hit
//-----------------------------------------------------------------------------
bool Key_CheckDoneTrapping( ButtonCode_t& code )
{
	if ( s_bTrapMode )
		return false;

	if ( !s_bDoneTrapping )
		return false;

	code = s_nTrapKey;
	s_nTrapKey = BUTTON_CODE_INVALID;

	// Reset since we retrieved the results
	s_bDoneTrapping = false;
	return true;
}

#ifndef SWDS

//-----------------------------------------------------------------------------
// Filter out trapped keys
//-----------------------------------------------------------------------------
static bool FilterTrappedKey( ButtonCode_t code, bool bDown )
{
	// After we've trapped a key, we want to capture the button up message for that key
	if ( s_nTrapKeyUp == code && !bDown )
	{
		s_nTrapKeyUp = BUTTON_CODE_INVALID;
		return true;
	}

	// Only key down events are trapped
	if ( s_bTrapMode && bDown )
	{
		s_nTrapKey		= code;
		s_bTrapMode     = false;
		s_bDoneTrapping = true;
		s_nTrapKeyUp	= code;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Lets tools have a whack at key events
//-----------------------------------------------------------------------------
static bool HandleToolKey( const InputEvent_t &event )
{
	IToolSystem *toolsys = toolframework->GetTopmostTool();
	return toolsys && toolsys->TrapKey( (ButtonCode_t)event.m_nData, ( event.m_nType != IE_ButtonReleased ) );
}

#endif // !SWDS

//-----------------------------------------------------------------------------
// Lets vgui have a whack at key events
//-----------------------------------------------------------------------------
#ifndef SWDS
static bool HandleVGuiKey( const InputEvent_t &event )
{
	bool bDown = event.m_nType != IE_ButtonReleased;
	ButtonCode_t code = (ButtonCode_t)event.m_nData;

	if ( bDown && IsX360() )
	{
		LogKeyPress( code );
		CheckCheatCodes();
	}

	return EngineVGui()->Key_Event( event );
}
//-----------------------------------------------------------------------------
// Lets the client have a whack at key events
//-----------------------------------------------------------------------------

static bool HandleClientKey( const InputEvent_t &event )
{
	bool bDown = event.m_nType != IE_ButtonReleased;
	ButtonCode_t code = (ButtonCode_t)event.m_nData;

	if ( g_ClientDLL && g_ClientDLL->IN_KeyEvent( bDown ? 1 : 0, code, s_pKeyInfo[ code ].m_pKeyBinding ) == 0 )
		return true;

	return false;
}
#endif


//-----------------------------------------------------------------------------
// Lets the engine have a whack at key events
//-----------------------------------------------------------------------------
#ifndef SWDS
static bool HandleEngineKey( const InputEvent_t &event )
{
	bool bDown = event.m_nType != IE_ButtonReleased;
	ButtonCode_t code = (ButtonCode_t)event.m_nData;

	// Warn about unbound keys 
	if ( IsPC() && bDown )
	{
		if ( IsJoystickCode( code ) && !IsJoystickAxisCode( code ) && !IsSteamControllerCode( code ) && !s_pKeyInfo[code].m_pKeyBinding )
		{
			ConDMsg( "%s is unbound.\n", g_pInputSystem->ButtonCodeToString( code ) );
		}
	}

	// Allow the client to handle mouse wheel events while the game has focus, without having to bind keys.
#if !defined( SWDS )
	if ( ( code == MOUSE_WHEEL_UP || code == MOUSE_WHEEL_DOWN ) && g_pClientReplay )
	{
		g_ClientDLL->IN_OnMouseWheeled( code == MOUSE_WHEEL_UP ? 1 : -1 );
	}
#endif

	// key up events only generate commands if the game key binding is
	// a button command (leading + sign).  These will occur even in console mode,
	// to keep the character from continuing an action started before a console
	// switch.  Button commands include the kenum as a parameter, so multiple
	// downs can be matched with ups
	char *kb = s_pKeyInfo[ code ].m_pKeyBinding;
	if ( !kb || !kb[0] )
		return false;

	char cmd[1024];
	if ( !bDown )
	{
		if ( kb[0] == '+' )
		{
			Q_snprintf( cmd, sizeof( cmd ), "-%s %i\n", kb+1, code );
			Cbuf_AddText( cmd );
			return true;
		}
		return false;
	}


	// Send to the interpreter
	if (kb[0] == '+')
	{
		// button commands add keynum as a parm
		Q_snprintf( cmd, sizeof( cmd ), "%s %i\n", kb, code );
		Cbuf_AddText( cmd );
		return true;
	}

	// Swallow console toggle if any modifier keys are down if it's bound to toggleconsole (the default)
	if ( !Q_stricmp( kb, "toggleconsole" ) )
	{
		if ( s_pKeyInfo[KEY_LALT].m_bKeyDown || s_pKeyInfo[KEY_LSHIFT].m_bKeyDown || s_pKeyInfo[KEY_LCONTROL].m_bKeyDown ||
			s_pKeyInfo[KEY_RALT].m_bKeyDown || s_pKeyInfo[KEY_RSHIFT].m_bKeyDown || s_pKeyInfo[KEY_RCONTROL].m_bKeyDown )
			return false;
	}

	Cbuf_AddText( kb );
	Cbuf_AddText( "\n" );
	return true;
}
#endif // !SWDS


//-----------------------------------------------------------------------------
// Helper function to make sure key down/key up events go to the right places
//-----------------------------------------------------------------------------
#ifndef SWDS
typedef bool (*FilterKeyFunc_t)( const InputEvent_t &event );

static bool FilterKey( const InputEvent_t &event, KeyUpTarget_t target, FilterKeyFunc_t func )
{
	bool bDown = event.m_nType != IE_ButtonReleased;
	ButtonCode_t code = (ButtonCode_t)event.m_nData;

	// Don't pass the key up to tools if some other system wants it
	if ( !bDown && !ShouldPassKeyUpToTarget( code, target ) )
		return false;

	bool bFiltered = func( event );

	// If we filtered it, then we need to get the key up event
	if ( bDown )
	{
		if ( bFiltered )
		{
			Assert( s_pKeyInfo[code].m_nKeyUpTarget == KEY_UP_ANYTARGET );
			s_pKeyInfo[code].m_nKeyUpTarget = target;
		}
	}
	else // Up case
	{
		if ( s_pKeyInfo[code].m_nKeyUpTarget == target )
		{
			s_pKeyInfo[code].m_nKeyUpTarget = KEY_UP_ANYTARGET;
			bFiltered = true;
		}
		else
		{
			// NOTE: It is illegal to trap up key events. The system will do it for us
			Assert( !bFiltered );
		}
	}

	return bFiltered;
}
#endif // !SWDS


//-----------------------------------------------------------------------------
// Called by the system between frames for both key up and key down events
//-----------------------------------------------------------------------------
void Key_Event( const InputEvent_t &event )
{
#ifdef SWDS
	return;
#else
	ASSERT_NO_REENTRY();

	bool bDown = event.m_nType != IE_ButtonReleased;
	ButtonCode_t code = (ButtonCode_t)event.m_nData;

#ifdef LINUX
	// We're getting some crashes referencing s_pKeyInfo[ code ]. Let's try to
	//	hard error here and see if we can get code and type at http://minidump.
	if ( code < 0 || code >= ARRAYSIZE( s_pKeyInfo ) )
	{
		Error( "Key_Event: invalid code! type:%d code:%d\n", event.m_nType, code );
	}
#endif

	// Don't handle key ups if the key's already up. 
	// NOTE: This should already be taken care of by the input system
	Assert( s_pKeyInfo[code].m_bKeyDown != bDown );
	if ( s_pKeyInfo[code].m_bKeyDown == bDown )
		return;

	s_pKeyInfo[code].m_bKeyDown = bDown;

	// Deal with trapped keys
	if ( FilterTrappedKey( code, bDown ) )
		return;

	// Keep vgui's notion of which keys are down up-to-date regardless of filtering
	// Necessary because vgui has multiple input contexts, so vgui can't directly
	// ask the input system for this information.
	EngineVGui()->UpdateButtonState( event );

	// Let tools have a whack at keys
	if ( FilterKey( event, KEY_UP_TOOLS, HandleToolKey ) )
		return;
							 
	// Let vgui have a whack at keys
	if ( FilterKey( event, KEY_UP_VGUI, HandleVGuiKey ) )
		return;

	// Let the client have a whack at keys
	if ( FilterKey( event, KEY_UP_CLIENT, HandleClientKey ) )
		return;

	// Finally, let the engine deal. Here's where keybindings occur.
	FilterKey( event, KEY_UP_ENGINE, HandleEngineKey );
#endif
}


