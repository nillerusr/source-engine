//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//

#include "cbase.h"
#include "view.h"

#include "iclientmode.h"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>

#include "convar.h"
#include "c_dod_team.h"
#include "c_dod_playerresource.h"
#include "c_dod_player.h"

#include "dod_hud_playerstatus_health.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDoDHudHealthBar::CDoDHudHealthBar( vgui::Panel *parent, const char *name ) : vgui::ImagePanel( parent, name )
{
	m_flPercentage = 1.0f;

	m_iMaterialIndex = vgui::surface()->DrawGetTextureId( "vgui/white" );
	if ( m_iMaterialIndex == -1 ) // we didn't find it, so create a new one
	{
		m_iMaterialIndex = vgui::surface()->CreateNewTextureID();	
	}

	vgui::surface()->DrawSetTextureFile( m_iMaterialIndex, "vgui/white", true, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudHealthBar::OnThink()
{
	BaseClass::OnThink();

	C_DODPlayer *pPlayer = GetHealthDelegatePlayer();
	if ( pPlayer )
	{
		// m_nHealth >= 0 
		int nHealth = MAX( pPlayer->GetHealth(), 0 );
		m_flPercentage = nHealth / 100.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudHealthBar::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_clrHealthHigh = pScheme->GetColor( "HudHealthGreen", GetFgColor() );
	m_clrHealthMed = pScheme->GetColor( "HudHealthYellow", GetFgColor() );
	m_clrHealthLow = pScheme->GetColor( "HudHealthRed", GetFgColor() );
	m_clrBackground = pScheme->GetColor( "HudHealthBG", GetBgColor() );
	m_clrBorder = pScheme->GetColor( "HudHealthBorder", GetBgColor() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudHealthBar::Paint( void )
{
	BaseClass::Paint();

	int x, y, w, h;
	GetBounds( x, y, w, h );

	int xpos = 0, ypos = 0;
	float flDamageY = h * ( 1.0f - m_flPercentage );

	Color *pclrHealth;

	if ( m_flPercentage > m_flFirstWarningLevel )
	{
		pclrHealth = &m_clrHealthHigh; 
	}
	else if ( m_flPercentage > m_flSecondWarningLevel )
	{
		pclrHealth = &m_clrHealthMed; 
	}
	else
	{
		pclrHealth = &m_clrHealthLow;
	}

	// blend in the red "damage" part
	float uv1 = 0.0f;
	float uv2 = 1.0f;

	vgui::surface()->DrawSetTexture( m_iMaterialIndex );

	Vector2D uv11( uv1, uv1 );
	Vector2D uv21( uv2, uv1 );
	Vector2D uv22( uv2, uv2 );
	Vector2D uv12( uv1, uv2 );

	vgui::Vertex_t vert[4];	

	// background
	vert[0].Init( Vector2D( xpos, ypos ), uv11 );
	vert[1].Init( Vector2D( xpos + w, ypos ), uv21 );
	vert[2].Init( Vector2D( xpos + w, ypos + h ), uv22 );				
	vert[3].Init( Vector2D( xpos, ypos + h ), uv12 );

	if ( m_flPercentage <= 0.01 )
	{
		vgui::surface()->DrawSetColor( m_clrHealthLow );
	}
	else
	{
		vgui::surface()->DrawSetColor( m_clrBackground );
	}
	vgui::surface()->DrawTexturedPolygon( 4, vert );

	// damage part
	vert[0].Init( Vector2D( xpos, flDamageY ), uv11 );
	vert[1].Init( Vector2D( xpos + w, flDamageY ), uv21 );
	vert[2].Init( Vector2D( xpos + w, ypos + h ), uv22 );				
	vert[3].Init( Vector2D( xpos, ypos + h ), uv12 );

	vgui::surface()->DrawSetColor( *pclrHealth );
	vgui::surface()->DrawTexturedPolygon( 4, vert );

	// outline
	vert[0].Init( Vector2D( xpos, ypos ), uv11 );
	vert[1].Init( Vector2D( xpos + w - 1, ypos ), uv21 );
	vert[2].Init( Vector2D( xpos + w - 1, ypos + h - 1 ), uv22 );				
	vert[3].Init( Vector2D( xpos, ypos + h - 1 ), uv12 );

	vgui::surface()->DrawSetColor( m_clrBorder );
	vgui::surface()->DrawTexturedPolyLine( vert, 4 );
}

// Show the health / class for a player other than the local player
void CDoDHudHealthBar::SetHealthDelegatePlayer( C_DODPlayer *pPlayer )
{
	m_hHealthDelegatePlayer = pPlayer;
}

C_DODPlayer *CDoDHudHealthBar::GetHealthDelegatePlayer( void )
{
	if ( m_hHealthDelegatePlayer.Get() )
	{
		C_DODPlayer *pPlayer = dynamic_cast<C_DODPlayer *>( m_hHealthDelegatePlayer.Get() );
		if ( pPlayer )
			return pPlayer;
	}

	return C_DODPlayer::GetLocalDODPlayer();
}

DECLARE_BUILD_FACTORY( CDoDHudHealth );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDoDHudHealth::CDoDHudHealth( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{
	SetProportional( true );

	m_pHealthBar = new CDoDHudHealthBar( this, "HealthBar" );
	m_pClassImageBG = new vgui::ImagePanel( this, "HealthClassImageBG" );
	m_pClassImage = new vgui::ImagePanel( this, "HealthClassImage" );
	
	m_nPrevClass = PLAYERCLASS_UNDEFINED;
	m_nPrevTeam = TEAM_INVALID;

	// load control settings...
	LoadControlSettings( "resource/UI/HudPlayerStatusHealth.res" );

	m_hHealthDelegatePlayer = NULL;
}

void CDoDHudHealth::OnScreenSizeChanged(int iOldWide, int iOldTall)
{
	LoadControlSettings( "resource/UI/HudPlayerStatusHealth.res" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDoDHudHealth::OnThink()
{
	BaseClass::OnThink();

	C_DODPlayer *pPlayer = GetHealthDelegatePlayer();
	if ( pPlayer )
	{
		int nTeam = pPlayer->GetTeamNumber();

		if ( nTeam == TEAM_ALLIES || nTeam == TEAM_AXIS )
		{
			C_DODTeam *pTeam = dynamic_cast<C_DODTeam *>( GetGlobalTeam( nTeam ) );
			C_DOD_PlayerResource *dod_PR = dynamic_cast<C_DOD_PlayerResource *>( g_PR );
			int nClass = dod_PR->GetPlayerClass( pPlayer->entindex() );

			if ( nClass != PLAYERCLASS_UNDEFINED )
			{
				if ( ( nClass != m_nPrevClass ) ||
					( nTeam != TEAM_INVALID && ( nTeam == TEAM_AXIS || nTeam == TEAM_ALLIES ) && nTeam != m_nPrevTeam ) )
				{
					m_nPrevClass = nClass;
					m_nPrevTeam = nTeam;

					if ( m_pClassImage )
					{
						m_pClassImage->SetImage( ( pTeam->GetPlayerClassInfo( nClass ) ).m_szClassHealthImage );
					}

					if ( m_pClassImageBG )
					{
						m_pClassImageBG->SetImage( ( pTeam->GetPlayerClassInfo( nClass ) ).m_szClassHealthImageBG );
					}
				}
			}
		}
	}
}

// Show the health / class for a player other than the local player
void CDoDHudHealth::SetHealthDelegatePlayer( C_DODPlayer *pPlayer )
{
	m_hHealthDelegatePlayer = pPlayer;

	m_pHealthBar->SetHealthDelegatePlayer( pPlayer );
}

C_DODPlayer *CDoDHudHealth::GetHealthDelegatePlayer( void )
{
	if ( m_hHealthDelegatePlayer.Get() )
	{
		C_DODPlayer *pPlayer = dynamic_cast<C_DODPlayer *>( m_hHealthDelegatePlayer.Get() );
		if ( pPlayer )
			return pPlayer;
	}

	return C_DODPlayer::GetLocalDODPlayer();
}