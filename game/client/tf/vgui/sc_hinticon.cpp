//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "sc_hinticon.h"
#include <vgui/IVGui.h>
#include "inputsystem/iinputsystem.h"

using namespace vgui;

DECLARE_BUILD_FACTORY( CSCHintIcon );

//-----------------------------------------------------------------------------
CSCHintIcon::CSCHintIcon( vgui::Panel *parent, const char* panelName ) :
	vgui::Label( parent, panelName, L"" )
	, m_bIsActionMapped( false )
	, m_actionSetHandle( 0 )
{
	m_szActionName[0] = '\0';
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSCHintIcon::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	auto szActionName = inResourceData->GetString( "actionName", "" );
	Q_strncpy( m_szActionName, szActionName, nMaxActionNameLength );

	auto szActionSet = inResourceData->GetString( "actionSet", nullptr );
	if ( szActionSet )
	{
		m_actionSetHandle = g_pInputSystem->GetActionSetHandle( szActionSet );
	}
	else
	{
		m_actionSetHandle = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSCHintIcon::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	const wchar_t* iconText = L"";
	m_bIsActionMapped = false;
	if ( m_actionSetHandle )
	{
		auto origin = g_pInputSystem->GetSteamControllerActionOrigin( m_szActionName, m_actionSetHandle );
		if ( origin != k_EControllerActionOrigin_None )
		{
			iconText = g_pInputSystem->GetSteamControllerFontCharacterForActionOrigin( origin );
			if ( iconText && iconText[0] )
			{
				m_bIsActionMapped = true;
			}
		}
	}

	SetText( iconText );
}