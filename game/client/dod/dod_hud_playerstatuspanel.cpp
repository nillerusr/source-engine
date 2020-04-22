//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui/ISurface.h>
#include "c_dod_player.h"
#include "clientmode_dod.h"

#include "dodcornercutpanel.h"

#include "c_dod_objective_resource.h"
#include "c_dod_playerresource.h"

#include "dod_hud_playerstatus_ammo.h"
#include "dod_hud_playerstatus_health.h"
#include "dod_hud_playerstatus_weapon.h"
#include "dod_hud_playerstatus_stamina.h"
#include "dod_hud_playerstatus_mgheat.h"
#include "dod_hud_playerstatus_fireselect.h"
#include "dod_hud_playerstatus_tnt.h"

class CDoDHudPlayerStatusPanel : public CHudElement, public vgui::EditablePanel
{
public:
	DECLARE_CLASS_SIMPLE( CDoDHudPlayerStatusPanel, vgui::EditablePanel );

	CDoDHudPlayerStatusPanel( const char *pElementName );

	virtual void Init();
	virtual void OnThink();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( IScheme *pScheme );

private:

	CDoDCutEditablePanel	*m_pMainBar;
	ImagePanel				*m_pAlliesIcon;
	ImagePanel				*m_pAxisIcon;
	CDoDHudAmmo				*m_pAmmoStatus;
	CDoDHudHealth			*m_pHealthStatus;	
	CDoDHudCurrentWeapon	*m_pCurrentWeapon;
	CDoDHudStamina			*m_pStamina;
	CDoDHudMGHeat			*m_pMGHeat;
	CDoDHudFireSelect		*m_pFireSelect;
	CDoDHudTNT				*m_pTNT;
};

DECLARE_HUDELEMENT( CDoDHudPlayerStatusPanel );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDoDHudPlayerStatusPanel::CDoDHudPlayerStatusPanel( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudPlayerStatusPanel" ) 
{
	SetParent( g_pClientMode->GetViewport() );

	m_pMainBar = new CDoDCutEditablePanel( this, "PlayerStatusMainBackground" );
	m_pAlliesIcon = new ImagePanel( this, "PlayerStatusAlliesIcon" );
	m_pAxisIcon = new ImagePanel( this, "PlayerStatusAxisIcon" );
	m_pAmmoStatus = new CDoDHudAmmo( this, "PlayerStatusAmmo" );
	m_pCurrentWeapon = new CDoDHudCurrentWeapon( this, "PlayerStatusCurrentWeapon" );
	m_pHealthStatus = new CDoDHudHealth( this, "PlayerStatusHealth" );
	m_pStamina = new CDoDHudStamina( this, "PlayerStatusStamina" );
	m_pMGHeat = new CDoDHudMGHeat( this, "PlayerStatusMGHeat" );
	m_pFireSelect = new CDoDHudFireSelect( this, "PlayerStatusFireSelect" );
	m_pTNT = new CDoDHudTNT( this, "PlayerStatusTNT" );
}

void CDoDHudPlayerStatusPanel::ApplySchemeSettings( IScheme *pScheme )
{
	// load control settings...
	LoadControlSettings( "resource/UI/HudPlayerStatusPanel.res" );

	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudPlayerStatusPanel::Init()
{
	if ( m_pMainBar )
	{
		m_pMainBar->SetVisible( true );
	}

	if ( m_pStamina )
	{
		m_pStamina->SetVisible( true );	
	}

	if ( m_pAlliesIcon )
	{
		m_pAlliesIcon->SetVisible( false );
	}

	if ( m_pAxisIcon )
	{
		m_pAxisIcon->SetVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudPlayerStatusPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, w, t;
	GetBounds( x, y, w, t );

	if ( w != ScreenWidth() )
	{
		// doing this because of the 16:9 and 16:10 resolutions (VGUI doesn't scale properly for them)
		SetBounds( x, y, ScreenWidth(), t ); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudPlayerStatusPanel::OnThink()
{
	BaseClass::OnThink();

	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if ( pPlayer )
	{
		// turn off the panel and children if the player is dead
		if ( !pPlayer->IsAlive() )
		{
			if ( IsVisible() )
			{
				SetVisible( false );
			}
			return;
		}
		else
		{
			if ( !IsVisible() )
			{
				SetVisible( true );
			}
		}

		// set our team icon
		int nTeam = pPlayer->GetTeamNumber();

		switch( nTeam )
		{
		case TEAM_AXIS:
			if ( m_pAlliesIcon && m_pAlliesIcon->IsVisible() )
			{
				m_pAlliesIcon->SetVisible( false );
			}
			if ( m_pAxisIcon && !m_pAxisIcon->IsVisible() )
			{
				m_pAxisIcon->SetVisible( true );
			}
			break;
		case TEAM_ALLIES:
		default:
			if ( m_pAlliesIcon && !m_pAlliesIcon->IsVisible() )
			{
				m_pAlliesIcon->SetVisible( true );
			}
			if ( m_pAxisIcon && m_pAxisIcon->IsVisible() )
			{
				m_pAxisIcon->SetVisible( false );
			}
			break;
		}
	}
}


