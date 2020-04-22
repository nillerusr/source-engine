//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "CvarToggleCheckButton.h"
#include "EngineInterface.h"
#include <vgui/IVGui.h>
#include "tier1/KeyValues.h"
#include "tier1/convar.h"
#include "IGameUIFuncs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

vgui::Panel *CvarToggleCheckButton_Factory()
{
	return new CCvarToggleCheckButton( NULL, NULL, "CvarToggleCheckButton", NULL );
}
DECLARE_BUILD_FACTORY_CUSTOM( CCvarToggleCheckButton, CvarToggleCheckButton_Factory );

CCvarToggleCheckButton::CCvarToggleCheckButton( Panel *parent, const char *panelName, const char *text, 
	char const *cvarname )
 : CheckButton( parent, panelName, text )
{
	m_pszCvarName = cvarname ? strdup( cvarname ) : NULL;

	if (m_pszCvarName)
	{
		Reset();
	}
	AddActionSignalTarget( this );
}

CCvarToggleCheckButton::~CCvarToggleCheckButton()
{
	if ( m_pszCvarName )
	{
		free( m_pszCvarName );
	}
}

void CCvarToggleCheckButton::Paint()
{
	if ( !m_pszCvarName || !m_pszCvarName[ 0 ] ) 
	{
		BaseClass::Paint();
		return;
	}

	// Look up current value
//	bool value = engine->pfnGetCvarFloat( m_pszCvarName ) > 0.0f ? true : false;
	ConVarRef var( m_pszCvarName, true );
	if ( !var.IsValid() )
		return;
	bool value = var.GetBool();
	
	if ( value != m_bStartValue )
	//if ( value != IsSelected() )
	{
		SetSelected( value );
		m_bStartValue = value;
	}
	BaseClass::Paint();
}

void CCvarToggleCheckButton::ApplyChanges()
{
	if ( !m_pszCvarName || !m_pszCvarName[ 0 ] ) 
		return;

	m_bStartValue = IsSelected();
//	engine->Cvar_SetValue( m_pszCvarName, m_bStartValue ? 1.0f : 0.0f );
	ConVarRef var( m_pszCvarName, true );
	if ( !var.IsValid() )
		return;
	var.SetValue( m_bStartValue );
}

void CCvarToggleCheckButton::Reset()
{
//	m_bStartValue = engine->pfnGetCvarFloat( m_pszCvarName ) > 0.0f ? true : false;

	if ( !m_pszCvarName || !m_pszCvarName[ 0 ] ) 
		return;

	ConVarRef var( m_pszCvarName, true );
	if ( !var.IsValid() )
		return;
	m_bStartValue = var.GetBool();
	SetSelected(m_bStartValue);
}

bool CCvarToggleCheckButton::HasBeenModified()
{
	return IsSelected() != m_bStartValue;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *panel - 
//-----------------------------------------------------------------------------
void CCvarToggleCheckButton::SetSelected( bool state )
{
	BaseClass::SetSelected( state );

	if ( !m_pszCvarName || !m_pszCvarName[ 0 ] ) 
		return;
/*
	// Look up current value
	bool value = state;

	engine->Cvar_SetValue( m_pszCvarName, value ? 1.0f : 0.0f );*/
}


//-----------------------------------------------------------------------------
void CCvarToggleCheckButton::OnButtonChecked()
{
	if (HasBeenModified())
	{
		PostActionSignal(new KeyValues("ControlModified"));
	}
}

//-----------------------------------------------------------------------------
void CCvarToggleCheckButton::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	const char *cvarName = inResourceData->GetString("cvar_name", "");
	const char *cvarValue = inResourceData->GetString("cvar_value", "");

	if( Q_stricmp( cvarName, "") == 0 )
		return;// Doesn't have cvar set up in res file, must have been constructed with it.

	if( m_pszCvarName )
		free( m_pszCvarName );// got a "", not a NULL from the create-control call

	m_pszCvarName = cvarName ? strdup( cvarName ) : NULL;

	if( Q_stricmp( cvarValue, "1") == 0 )
		m_bStartValue = true;
	else
		m_bStartValue = false;

	const ConVar *var = cvar->FindVar( m_pszCvarName );
	if ( var )
	{
		if( var->GetBool() )
			SetSelected( true );
		else
			SetSelected( false );
	}
}