//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>

#include <vgui/ISurface.h>

#include "c_dod_team.h"
#include "c_dod_playerresource.h"
#include "c_dod_player.h"

#include "dodcornercutpanel.h"
#include "dod_hud_playerstatus_stamina.h"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudStaminaProgressBar::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_clrActive = pScheme->GetColor( "HudStaminaBar.Active", GetFgColor() );
	m_clrActiveLow = pScheme->GetColor( "HudStaminaBar.ActiveLow", GetFgColor() );
	m_clrInactive = pScheme->GetColor( "HudStaminaBar.InActive", GetFgColor() );
	m_clrInactiveLow = pScheme->GetColor( "HudStaminaBar.InActiveLow", GetFgColor() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudStaminaProgressBar::Paint()
{
	BaseClass::Paint();

	int xpos = 0, ypos = 0;
	int x, y, w, t;
	GetBounds( x, y, w, t );

	if ( m_flPercentage < m_flWarningLevel )
	{
		vgui::surface()->DrawSetColor( m_clrActiveLow );
	}
	else
	{
		vgui::surface()->DrawSetColor( m_clrActive );
	}

	int nActiveLimit = w * m_flPercentage;

	while ( xpos + m_flSliceWidth < w )
	{
		if ( xpos + m_flSliceWidth > nActiveLimit )
		{
			if ( m_flPercentage < m_flWarningLevel )
			{
				vgui::surface()->DrawSetColor( m_clrInactiveLow );
			}
			else
			{
				vgui::surface()->DrawSetColor( m_clrInactive );
			}
		}

		vgui::surface()->DrawFilledRect( xpos, ypos, xpos + m_flSliceWidth, ypos + t );
		xpos += m_flSliceWidth + m_flSliceSpacer;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDoDHudStaminaIcon::CDoDHudStaminaIcon( vgui::Panel *parent, const char *name ) : vgui::ImagePanel( parent, name )
{
	m_icon = NULL;
	memset( m_szIcon, 0, sizeof( m_szIcon ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudStaminaIcon::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	Q_strncpy( m_szIcon, inResourceData->GetString( "stamina_icon", "stamina_icon" ), sizeof( m_szIcon ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudStaminaIcon::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_clrActive = pScheme->GetColor( "HudStaminaIcon.Active", GetFgColor() );
	m_clrActiveLow = pScheme->GetColor( "HudStaminaIcon.ActiveLow", GetFgColor() );

	if ( !m_icon )
	{
		m_icon = gHUD.GetIcon( m_szIcon );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudStaminaIcon::Paint()
{
	int x, y, w, t;
	GetBounds( x, y, w, t );

	if ( m_icon )
	{
		m_icon->DrawSelf( 0, -3, w, t, ( m_flPercentage < m_flWarningLevel ) ? m_clrActiveLow : m_clrActive );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDoDHudStamina::CDoDHudStamina( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{
	m_pBackground = new CDoDCutEditablePanel( this, "StaminaBackground" );
	m_pIcon = new CDoDHudStaminaIcon( this, "StaminaIcon" );
	m_pProgressBar = new CDoDHudStaminaProgressBar( this, "StaminaProgressBar" );

	// load control settings...
	LoadControlSettings( "resource/UI/HudPlayerStatusStamina.res" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudStamina::OnScreenSizeChanged( int iOldWide, int iOldTall )
{
	LoadControlSettings( "resource/UI/HudPlayerStatusStamina.res" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudStamina::OnThink()
{
	BaseClass::OnThink();

	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();
	if ( pPlayer )
	{
		float flPercentage = (float)( pPlayer->m_Shared.GetStamina() ) / 100.0f;

		if ( m_pProgressBar )
		{
			m_pProgressBar->SetPercentage( flPercentage );
		}

		if ( m_pIcon )
		{
			m_pIcon->SetPercentage( flPercentage );
		}
	}
}
