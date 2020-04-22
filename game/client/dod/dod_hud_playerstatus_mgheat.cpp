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

#include "weapon_mg42.h"

#include "dodcornercutpanel.h"
#include "dod_hud_playerstatus_mgheat.h"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudMGHeatProgressBar::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_clrActive = pScheme->GetColor( "HudMGHeatBar.Active", GetFgColor() );
	m_clrActiveLow = pScheme->GetColor( "HudMGHeatBar.ActiveLow", GetFgColor() );
	m_clrInactive = pScheme->GetColor( "HudMGHeatBar.InActive", GetFgColor() );
	m_clrInactiveLow = pScheme->GetColor( "HudMGHeatBar.InActiveLow", GetFgColor() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudMGHeatProgressBar::Paint()
{
	BaseClass::Paint();

	int xpos = 0, ypos = 0;
	int x, y, w, t;
	GetBounds( x, y, w, t );

	if ( m_flPercentage > m_flWarningLevel )
	{
		vgui::surface()->DrawSetColor( m_clrInactiveLow );
	}
	else
	{
		vgui::surface()->DrawSetColor( m_clrInactive );
	}

	int nActiveLimit = w * ( 1.0f - m_flPercentage );

	while ( xpos + m_flSliceWidth < w )
	{
		if ( xpos + m_flSliceWidth > nActiveLimit )
		{
			if ( m_flPercentage > m_flWarningLevel )
			{
				vgui::surface()->DrawSetColor( m_clrActiveLow );
			}
			else
			{
				vgui::surface()->DrawSetColor( m_clrActive );
			}
		}

		vgui::surface()->DrawFilledRect( xpos, ypos, xpos + m_flSliceWidth, ypos + t );
		xpos += m_flSliceWidth + m_flSliceSpacer;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudMGHeatIcon::SetType( int nType )
{
	m_nType = nType;

	switch( nType )
	{
//	case WEAPON_30CAL:
//		m_icon = gHUD.GetIcon( m_szIcon30cal );
//		break;
	case WEAPON_MG42:
	default:
		m_icon = gHUD.GetIcon( m_szIconMg42 );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudMGHeatIcon::ApplySettings( KeyValues *inResourceData )
{
//	Q_strncpy( m_szIcon30cal, inResourceData->GetString( "icon_30cal", "mgheat_30cal" ), sizeof( m_szIcon30cal ) );
	Q_strncpy( m_szIconMg42, inResourceData->GetString( "icon_mg42", "mgheat_mg42" ), sizeof( m_szIconMg42 ) );
	
	BaseClass::ApplySettings( inResourceData );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudMGHeatIcon::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_clrActive = pScheme->GetColor( "HudMGHeatIcon.Active", GetFgColor() );
	m_clrActiveLow = pScheme->GetColor( "HudMGHeatIcon.ActiveLow", GetFgColor() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudMGHeatIcon::Paint()
{
	BaseClass::Paint();

	int x, y, w, t;
	GetBounds( x, y, w, t );

	if ( m_icon )
	{
		m_icon->DrawSelf( 0, 0, w, t, ( m_flPercentage > m_flWarningLevel ) ? m_clrActiveLow : m_clrActive );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDoDHudMGHeat::CDoDHudMGHeat( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{
	m_pBackground = new CDoDCutEditablePanel( this, "MGHeatBackground" );
	m_pIcon = new CDoDHudMGHeatIcon( this, "MGHeatIcon" );
	m_pProgressBar = new CDoDHudMGHeatProgressBar( this, "MGHeatProgressBar" );
}

void CDoDHudMGHeat::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	// load control settings...
	LoadControlSettings( "resource/UI/HudPlayerStatusMGHeat.res" );

	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudMGHeat::SetVisible( bool state )
{
	if ( m_pBackground && m_pBackground->IsVisible() != state )
	{
		m_pBackground->SetVisible( state );
	}

	if ( m_pIcon && m_pIcon->IsVisible() != state )
	{
		m_pIcon->SetVisible( state );
	}

	if ( m_pProgressBar && m_pProgressBar->IsVisible() != state  )
	{
		m_pProgressBar->SetVisible( state );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudMGHeat::OnThink()
{
	BaseClass::OnThink();

	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();
	if ( pPlayer )
	{
		CWeaponDODBase *pWeapon = pPlayer->GetActiveDODWeapon();
		if ( pWeapon )
		{
/*			if ( pWeapon->IsA( WEAPON_30CAL ) )
			{
				CWeapon30cal *p30Cal = (CWeapon30cal *)pWeapon;

				if( p30Cal )
				{
					float flPercentage = (float)p30Cal->GetWeaponHeat() / 100.0;

					if ( m_pProgressBar )
					{
						m_pProgressBar->SetPercentage( flPercentage );
					}

					if ( m_pIcon )
					{
						m_pIcon->SetPercentage( flPercentage );
					}
				}

				if ( m_pIcon )
				{
					m_pIcon->SetType( WEAPON_30CAL );
				}

				SetVisible( true );
			}
*/
			if ( pWeapon->IsA( WEAPON_MG42 ) )
			{
				CWeaponMG42 *pMG42 = (CWeaponMG42 *)pWeapon;
									
				if( pMG42 )
				{
					float flPercentage = (float)pMG42->GetWeaponHeat() / 100.0;

					if ( m_pProgressBar )
					{
						m_pProgressBar->SetPercentage( flPercentage );
					}

					if ( m_pIcon )
					{
						m_pIcon->SetPercentage( flPercentage );
					}
				}

				if ( m_pIcon )
				{
					m_pIcon->SetType( WEAPON_MG42 );
				}

				SetVisible( true );
			}
			else
			{
				SetVisible( false );
			}
		}
	}
}
