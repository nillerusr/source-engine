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

#include "weapon_dodfireselect.h"
#include "dodcornercutpanel.h"
#include "dod_hud_playerstatus_fireselect.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDoDHudFireSelect::CDoDHudFireSelect( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{
	m_pBackground = new CDoDCutEditablePanel( this, "FireSelectBackground" );

	m_pIconMP44 = new CIconPanel( this, "Icon_MP44" );
	m_pIconBAR = new CIconPanel( this, "Icon_BAR" );

	m_pBulletLeft = new CIconPanel( this, "Bullet_Left" );
	m_pBulletCenter = new CIconPanel( this, "Bullet_Center" );
	m_pBulletRight = new CIconPanel( this, "Bullet_Right" );

	m_pBulletRight->SetVisible( true );
}

void CDoDHudFireSelect::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	LoadControlSettings( "resource/UI/HudPlayerStatusFireSelect.res" );

	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudFireSelect::SetVisible( bool state )
{
	if ( m_pBackground && m_pBackground->IsVisible() != state )
	{
		m_pBackground->SetVisible( state );
	}

	m_pBulletLeft->SetVisible( state );
	m_pBulletCenter->SetVisible( state );
	m_pBulletRight->SetVisible( state );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudFireSelect::OnThink()
{
	BaseClass::OnThink();

	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();
	if ( pPlayer )
	{
		CWeaponDODBase *pWeapon = pPlayer->GetActiveDODWeapon();
		if ( pWeapon )
		{
			bool bSemiAutoWpn = false;

			if ( pWeapon->IsA( WEAPON_MP44 ) )
			{
				m_pIconBAR->SetVisible( false );
				m_pIconMP44->SetVisible( true );

				bSemiAutoWpn = true;
			}
			else if ( pWeapon->IsA( WEAPON_BAR ) )
			{
				m_pIconBAR->SetVisible( true );
				m_pIconMP44->SetVisible( false );

				bSemiAutoWpn = true;
			}
			else
			{
				m_pIconBAR->SetVisible( false );
				m_pIconMP44->SetVisible( false );
			}

			if ( bSemiAutoWpn )
			{
				SetVisible( true );

				CDODFireSelectWeapon *pFireSelect = dynamic_cast<CDODFireSelectWeapon *>(pWeapon);

				if ( pFireSelect && pFireSelect->IsSemiAuto() )
				{
					m_pBulletLeft->SetVisible( false );
					m_pBulletCenter->SetVisible( false );
				}
				else
				{
					m_pBulletLeft->SetVisible( true );
					m_pBulletCenter->SetVisible( true );
				}
			}
			else
			{
				SetVisible( false );
			}
		}
	}
}
