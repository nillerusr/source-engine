//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "tier2/keybindings.h"
#include "tier2/tier2.h"
#include "inputsystem/iinputsystem.h"
#include "tier1/utlbuffer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



//-----------------------------------------------------------------------------
// Set a key binding
//-----------------------------------------------------------------------------
void CKeyBindings::SetBinding( ButtonCode_t code, const char *pBinding )
{
	if ( code == BUTTON_CODE_INVALID || code == KEY_NONE )
		return;

	// free old bindings
	if ( !m_KeyInfo[code].IsEmpty() )
	{
		// Exactly the same, don't re-bind and fragment memory
		if ( !Q_stricmp( m_KeyInfo[code], pBinding ) )
			return;
	}
			
	// allocate memory for new binding
	m_KeyInfo[code] = pBinding;
}

void CKeyBindings::SetBinding( const char *pButtonName, const char *pBinding )
{
	ButtonCode_t code = g_pInputSystem->StringToButtonCode( pButtonName );
	SetBinding( code, pBinding );
}

void CKeyBindings::Unbind( ButtonCode_t code )
{
	if ( code != KEY_NONE && code != BUTTON_CODE_INVALID )
	{
		m_KeyInfo[code] = "";
	}
}

void CKeyBindings::Unbind( const char *pButtonName )
{
	ButtonCode_t code = g_pInputSystem->StringToButtonCode( pButtonName );
	Unbind( code );
}

void CKeyBindings::UnbindAll()
{
	for ( int i = 0; i < BUTTON_CODE_LAST; i++ )
	{
		m_KeyInfo[i] = "";
	}
}


//-----------------------------------------------------------------------------
// Count number of lines of bindings we'll be writing
//-----------------------------------------------------------------------------
int CKeyBindings::GetBindingCount( ) const
{
	int	nCount = 0;
	for ( int i = 0; i < BUTTON_CODE_LAST; i++ )
	{
		if ( !m_KeyInfo[i].IsEmpty() )
		{
			nCount++;
		}
	}

	return nCount;
}


//-----------------------------------------------------------------------------
// Writes lines containing "bind key value"
//-----------------------------------------------------------------------------
void CKeyBindings::WriteBindings( CUtlBuffer &buf )
{
	for ( int i = 0; i < BUTTON_CODE_LAST; i++ )
	{
		if ( !m_KeyInfo[i].IsEmpty() )
		{
			const char *pButtonCode = g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i );
			buf.Printf( "bind \"%s\" \"%s\"\n", pButtonCode, m_KeyInfo[i].Get() );
		}
	}
}


//-----------------------------------------------------------------------------
// Returns the keyname to which a binding string is bound.  E.g., if 
// TAB is bound to +use then searching for +use will return "TAB"
//-----------------------------------------------------------------------------
const char *CKeyBindings::ButtonNameForBinding( const char *pBinding )
{
	const char *pBind = pBinding;
	if ( pBinding[0] == '+' )
	{
		++pBind;
	}

	for ( int i = 0; i < BUTTON_CODE_LAST; i++ )
	{
		if ( m_KeyInfo[i].IsEmpty() )
			continue;

		if ( m_KeyInfo[i][0] == '+' )
		{
			if ( !Q_stricmp( &m_KeyInfo[i].Get()[1], pBind ) )
				return g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i );
		}
		else
		{
			if ( !Q_stricmp( m_KeyInfo[i], pBind ) )
				return g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i );
		}
	}

	return NULL;
}

const char *CKeyBindings::GetBindingForButton( ButtonCode_t code )
{
	if ( m_KeyInfo[code].IsEmpty() )
		return NULL;

	return m_KeyInfo[ code ];
}



