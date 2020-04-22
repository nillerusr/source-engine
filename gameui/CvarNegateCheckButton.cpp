//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "CvarNegateCheckButton.h"
#include "EngineInterface.h"
#include <vgui/IVGui.h>
#include "IGameUIFuncs.h"
#include "tier1/KeyValues.h"
#include "tier1/convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

CCvarNegateCheckButton::CCvarNegateCheckButton( Panel *parent, const char *panelName, const char *text, 
	const char *cvarname )
 : CheckButton( parent, panelName, text )
{
	m_pszCvarName = cvarname ? strdup( cvarname ) : NULL;
	Reset();
	AddActionSignalTarget( this );
}

CCvarNegateCheckButton::~CCvarNegateCheckButton()
{
	free( m_pszCvarName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCvarNegateCheckButton::Paint()
{
	if ( !m_pszCvarName )
	{
		BaseClass::Paint();
		return;
	}

	// Look up current value
//	float value = engine->pfnGetCvarFloat( m_pszCvarName );
	ConVarRef var( m_pszCvarName );
	if ( !var.IsValid() )
		return;

	float value = var.GetFloat();
		
	if ( value < 0 )
	{
		if ( !m_bStartState )
		{
			SetSelected( true );
			m_bStartState = true;
		}
	}
	else
	{
		if ( m_bStartState )
		{
			SetSelected( false );
			m_bStartState = false;
		}
	}
	BaseClass::Paint();
}

void CCvarNegateCheckButton::Reset()
{
	// Look up current value
//	float value = engine->pfnGetCvarFloat( m_pszCvarName );
	ConVarRef var( m_pszCvarName );
	if ( !var.IsValid() )
		return;

	float value = var.GetFloat();
		
	if ( value < 0 )
	{
		m_bStartState = true;
	}
	else
	{
		m_bStartState = false;
	}
	SetSelected(m_bStartState);
}

bool CCvarNegateCheckButton::HasBeenModified()
{
	return IsSelected() != m_bStartState;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *panel - 
//-----------------------------------------------------------------------------
void CCvarNegateCheckButton::SetSelected( bool state )
{
	BaseClass::SetSelected( state );
}

void CCvarNegateCheckButton::ApplyChanges()
{
	if ( !m_pszCvarName || !m_pszCvarName[ 0 ] ) 
		return;

	ConVarRef var( m_pszCvarName );
	float value = var.GetFloat();
	
	value = (float)fabs( value );
	if (value < 0.00001)
	{
		// correct the value if it's not set
		value = 0.022f;
	}

	m_bStartState = IsSelected();
	value = -value;

	float ans = m_bStartState ? value : -value;
	var.SetValue( ans );
}


void CCvarNegateCheckButton::OnButtonChecked()
{
	if (HasBeenModified())
	{
		PostActionSignal(new KeyValues("ControlModified"));
	}
}
