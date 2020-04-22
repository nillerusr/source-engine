//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "CvarTextEntry.h"
#include "EngineInterface.h"
#include <vgui/IVGui.h>
#include "IGameUIFuncs.h"
#include "tier1/KeyValues.h"
#include "tier1/convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static const int MAX_CVAR_TEXT = 64;

CCvarTextEntry::CCvarTextEntry( Panel *parent, const char *panelName, char const *cvarname )
 : TextEntry( parent, panelName)
{
	m_pszCvarName = cvarname ? strdup( cvarname ) : NULL;
	m_pszStartValue[0] = 0;

	if ( m_pszCvarName )
	{
		Reset();
	}

	AddActionSignalTarget( this );
}

CCvarTextEntry::~CCvarTextEntry()
{
	if ( m_pszCvarName )
	{
		free( m_pszCvarName );
	}
}

void CCvarTextEntry::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	if (GetMaximumCharCount() < 0 || GetMaximumCharCount() > MAX_CVAR_TEXT)
	{
		SetMaximumCharCount(MAX_CVAR_TEXT - 1);
	}
}

void CCvarTextEntry::ApplyChanges( bool immediate )
{
	if ( !m_pszCvarName )
		return;

	char szText[ MAX_CVAR_TEXT ];
	GetText( szText, MAX_CVAR_TEXT );

	if ( !szText[ 0 ] )
		return;

	if ( immediate )
	{
		// set immediately - don't wait for the next frame
		ConVarRef newCvar( m_pszCvarName );
		newCvar.SetValue( szText );
	}
	else
	{
		char szCommand[ 256 ];
		sprintf( szCommand, "%s \"%s\"\n", m_pszCvarName, szText );
		engine->ClientCmd_Unrestricted( szCommand );
	}

	Q_strncpy( m_pszStartValue, szText, sizeof( m_pszStartValue ) );
}

void CCvarTextEntry::Reset()
{
//	char *value = engine->pfnGetCvarString( m_pszCvarName );
	ConVarRef var( m_pszCvarName );
	if ( !var.IsValid() )
		return;
	const char *value = var.GetString();
	if ( value && value[ 0 ] )
	{
		SetText( value );
		Q_strncpy( m_pszStartValue, value, sizeof( m_pszStartValue ) );
	}
}

bool CCvarTextEntry::HasBeenModified()
{
	char szText[ MAX_CVAR_TEXT ];
	GetText( szText, MAX_CVAR_TEXT );

	return stricmp( szText, m_pszStartValue );
}


void CCvarTextEntry::OnTextChanged()
{
	if ( !m_pszCvarName )
		return;
	
	if (HasBeenModified())
	{
		PostActionSignal(new KeyValues("ControlModified"));
	}
}
