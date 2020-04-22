//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "KeyToggleCheckButton.h"
#include "EngineInterface.h"
#include <vgui/IVGui.h>
#include "IGameUIFuncs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CKeyToggleCheckButton::CKeyToggleCheckButton( Panel *parent, const char *panelName, const char *text, 
	char const *key, char const *cmdname )
 : CheckButton( parent, panelName, text )
{
	m_pszKeyName = key ? strdup( key ) : NULL;
	m_pszCmdName = cmdname ? strdup( cmdname ) : NULL;

	if (m_pszKeyName)
	{
		Reset();
	}
	//m_bNoCommand = false;
}

CKeyToggleCheckButton::~CKeyToggleCheckButton()
{
	free( m_pszKeyName );
	free( m_pszCmdName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKeyToggleCheckButton::Paint()
{
	BaseClass::Paint();

	if ( !m_pszKeyName )
		return;

	// Fixme, look up key state
	bool isdown;
	if ( gameuifuncs->IsKeyDown( m_pszKeyName, isdown ) )
	{
		// if someone changed the value using the consoel
		if ( m_bStartValue != isdown )
		{
			SetSelected( isdown );
			m_bStartValue = isdown;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *panel - 
//-----------------------------------------------------------------------------
/*
void CKeyToggleCheckButton::SetSelected( bool state )
{
	BaseClass::SetSelected( state );

	if ( !m_pszCmdName || !m_pszCmdName[ 0 ] ) 
		return;

	if ( m_bNoCommand )
		return;

	char szCommand[ 256 ];

	Q_snprintf( szCommand, sizeof( szCommand ), "%c%s\n", IsSelected() ? '+' : '-',
		m_pszCmdName );

	engine->pfnClientCmd( szCommand );
}*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKeyToggleCheckButton::Reset()
{
	gameuifuncs->IsKeyDown( m_pszKeyName, m_bStartValue );
	if ( IsSelected() != m_bStartValue)
	{
		SetSelected( m_bStartValue );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKeyToggleCheckButton::ApplyChanges()
{
	if ( !m_pszCmdName || !m_pszCmdName[ 0 ] ) 
		return;

	char szCommand[ 256 ];

	Q_snprintf( szCommand, sizeof( szCommand ), "%c%s\n", IsSelected() ? '+' : '-',
		m_pszCmdName );

	engine->ClientCmd_Unrestricted( szCommand );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CKeyToggleCheckButton::HasBeenModified()
{
	return IsSelected() != m_bStartValue;
}