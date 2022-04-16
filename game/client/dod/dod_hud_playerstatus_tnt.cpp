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
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>

#include <vgui/ISurface.h>

#include "c_dod_player.h"

#include "dodcornercutpanel.h"
#include "dod_hud_playerstatus_tnt.h"
#include "dod_gamerules.h"
#include "clientmode_dod.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDoDHudTNT::CDoDHudTNT( vgui::Panel *parent, const char *name ) : CDoDCutEditablePanel( parent, name )
{
	m_pIconTNT = new CIconPanel( this, "Icon_TNT" );
	m_pIconTNT_Missing = new CIconPanel( this, "Icon_TNT_Missing" );

	SetProportional( true );
	LoadControlSettings( "resource/UI/HudPlayerStatusTNT.res" );

	m_pIconTNT->SetVisible( false );
	m_pIconTNT_Missing->SetVisible( true );

	m_bHasTNT = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudTNT::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	LoadControlSettings( "resource/UI/HudPlayerStatusTNT.res" );

	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudTNT::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	// Get icon loc, dims

	m_iIconX = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), inResourceData->GetInt( "icon_x", 0 ) );
	m_iIconY = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), inResourceData->GetInt( "icon_y", 0 ) );
	m_iIconW = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), inResourceData->GetInt( "icon_w", 32 ) );
	m_iIconH = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), inResourceData->GetInt( "icon_h", 32 ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudTNT::SetVisible( bool state )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudTNT::OnThink()
{
	BaseClass::OnThink();

	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();
	if ( !pPlayer )
	{
		return;
	}

	if ( DODGameRules()->IsBombingTeam( pPlayer->GetTeamNumber() ) )
	{
		SetAlpha( 255 );

		C_BaseCombatWeapon *pBomb = NULL;

		for ( int i = 0; i < MAX_WEAPONS; i++ )
		{
			C_BaseCombatWeapon *pWeapon = pPlayer->GetWeapon( i );

			if ( pWeapon == NULL )
			{
				continue;
			}

			if ( pWeapon->GetSlot() == WPN_SLOT_BOMB )
			{
				pBomb = pWeapon;
				break;
			}
		}

		if ( pBomb )
		{
			if ( m_bHasTNT == false )
			{
				m_bHasTNT = true;

				m_pIconTNT->SetVisible( true );
				m_pIconTNT_Missing->SetVisible( false );
			}
		}
		else
		{
			if ( m_bHasTNT == true )
			{
				m_bHasTNT = false;

				m_pIconTNT->SetVisible( false );
				m_pIconTNT_Missing->SetVisible( true );
			}			
		}
	}
	else
	{
		SetAlpha( 0 );
	}	
}