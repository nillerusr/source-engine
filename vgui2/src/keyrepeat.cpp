//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "inputsystem/InputEnums.h"

#include "vgui/KeyCode.h"
#include "vgui/keyrepeat.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file
#include "tier0/memdbgon.h"

//#define DEBUG_REPEATS

#ifdef DEBUG_REPEATS
#define DbgRepeat(...) ConMsg( __VA_ARGS__ )
#else
#define DbgRepeat(...) 
#endif

using namespace vgui;

vgui::KeyCode g_iCodesForAliases[FM_NUM_KEYREPEAT_ALIASES] = 
{
	KEY_XBUTTON_UP,
	KEY_XBUTTON_DOWN,
	KEY_XBUTTON_LEFT,
	KEY_XBUTTON_RIGHT
};

//-----------------------------------------------------------------------------
// Purpose: Map joystick codes to our internal ones
//-----------------------------------------------------------------------------
static int GetIndexForCode( vgui::KeyCode code )
{ 
	KeyCode localCode = GetBaseButtonCode( code );

	switch ( localCode )
	{
	case KEY_XBUTTON_DOWN: 
	case KEY_XSTICK1_DOWN:
	case KEY_XSTICK2_DOWN:
		return KR_ALIAS_DOWN; break;
	case KEY_XBUTTON_UP: 
	case KEY_XSTICK1_UP:
	case KEY_XSTICK2_UP:
		return KR_ALIAS_UP; break;
	case KEY_XBUTTON_LEFT: 
	case KEY_XSTICK1_LEFT:
	case KEY_XSTICK2_LEFT:
		return KR_ALIAS_LEFT; break;
	case KEY_XBUTTON_RIGHT: 
	case KEY_XSTICK1_RIGHT:
	case KEY_XSTICK2_RIGHT:
		return KR_ALIAS_RIGHT; break;
	default:
		break;
	}
	return -1;
}

//-----------------------------------------------------------------------------
CKeyRepeatHandler::CKeyRepeatHandler()
{
	Reset();
	for ( int i = 0; i < FM_NUM_KEYREPEAT_ALIASES; i++ )
	{
		m_flRepeatTimes[i] = 0.16;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clear all state
//-----------------------------------------------------------------------------
void CKeyRepeatHandler::Reset() 
{ 
	DbgRepeat( "KeyRepeat: Reset\n" );

	memset( m_bAliasDown, 0, sizeof( m_bAliasDown ) ); 
	m_bHaveKeyDown = false; 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKeyRepeatHandler::KeyDown( vgui::KeyCode code )
{
	int joyStick = GetJoystickForCode( code );
	int iIndex = GetIndexForCode(code);
	if ( iIndex == -1 )
		return;

	if ( m_bAliasDown[ joyStick ][ iIndex ] )
		return;

	DbgRepeat( "KeyRepeat: KeyDown %d(%d)\n", joyStick, iIndex );

	Reset();
	m_bAliasDown[ joyStick ][ iIndex ] = true;
	m_flNextKeyRepeat[ joyStick ] = Plat_FloatTime() + 0.4;
	m_bHaveKeyDown = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKeyRepeatHandler::KeyUp( vgui::KeyCode code )
{
	int joyStick = GetJoystickForCode( code );
	int iIndex = GetIndexForCode(code);
	if ( iIndex == -1 )
		return;

	DbgRepeat( "KeyRepeat: KeyUp %d(%d)\n", joyStick, iIndex );

	m_bAliasDown[ joyStick ][ iIndex ] = false;

	m_bHaveKeyDown = false;
	for ( int i = 0; i < FM_NUM_KEYREPEAT_ALIASES; i++ )
	{
		for ( int j = 0; j < MAX_JOYSTICKS; j++ )
		{
			if ( m_bAliasDown[ j ][ i ] )
			{
				m_bHaveKeyDown = true;
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
vgui::KeyCode CKeyRepeatHandler::KeyRepeated( void )
{
	if ( IsPC() )
		return BUTTON_CODE_NONE;

	if ( !m_bHaveKeyDown )
		return BUTTON_CODE_NONE;

	float currentTime = Plat_FloatTime();

	for ( int j = 0; j < MAX_JOYSTICKS; j++ )
	{
		if ( m_flNextKeyRepeat[ j ] < currentTime )
		{
			for ( int i = 0; i < FM_NUM_KEYREPEAT_ALIASES; i++ )
			{
				if ( m_bAliasDown[ j ][ i ] )
				{
					m_flNextKeyRepeat[ j ] = currentTime + m_flRepeatTimes[i];
					DbgRepeat( "KeyRepeat: Repeat %d(%d)\n", j, i );

					return ButtonCodeToJoystickButtonCode( g_iCodesForAliases[i], j );
				}
			}
		}
	}

	return BUTTON_CODE_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKeyRepeatHandler::SetKeyRepeatTime( vgui::KeyCode code, float flRepeat )
{
	int iIndex = GetIndexForCode(code);
	Assert( iIndex != -1 );
	m_flRepeatTimes[ iIndex ] = flRepeat;
}
