//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
// Health.cpp
//
// implementation of CHudHealth class
//
#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"

#include "iclientmode.h"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/Panel.h>

using namespace vgui;

#include "hudelement.h"
#include "convar.h"
#include "c_dod_player.h"

//-----------------------------------------------------------------------------
// Purpose: Health panel
//-----------------------------------------------------------------------------
class CHudHealth : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudHealth, vgui::Panel );

public:
	CHudHealth( const char *pElementName );
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void OnThink();

	virtual bool ShouldDraw( void );
	virtual void Paint( void );
	virtual void ApplySchemeSettings( IScheme *scheme );

private:
	int		m_iHealth;
	int		m_iStamina;

	CHudTexture *m_pIconHealthBar;
	CHudTexture *m_pIconHealthOverlay;
	CHudTexture *m_pIconBG;
	CHudTexture *m_pIconStaminaBar;
};	

//DECLARE_HUDELEMENT( CHudHealth );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudHealth::CHudHealth( const char *pElementName ) : CHudElement( pElementName ), BaseClass(NULL, "HudHealth") 
{
	SetParent( g_pClientMode->GetViewport() );
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHealth::Init()
{
	m_iHealth = 100;
}

void CHudHealth::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	if( !m_pIconHealthBar )
	{
		m_pIconHealthBar = gHUD.GetIcon( "hud_healthbar" );
	}

	if( !m_pIconHealthOverlay )
	{
		m_pIconHealthOverlay = gHUD.GetIcon( "hud_health_overlay" );
	}

	if( !m_pIconBG )
	{
		m_pIconBG = gHUD.GetIcon( "hud_main" );
	}

	if( !m_pIconStaminaBar )
	{
		m_pIconStaminaBar = gHUD.GetIcon( "hud_staminabar" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHealth::Reset()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHealth::VidInit()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHealth::OnThink()
{
	int realHealth = 0;
	C_DODPlayer *local = C_DODPlayer::GetLocalDODPlayer();
	if ( local )
	{
		// Never below zero
		realHealth = MAX( local->GetHealth(), 0 );

		m_iStamina = local->m_Shared.GetStamina();
	}

	// Only update the fade if we've changed health
	if ( realHealth == m_iHealth )
	{
		return;
	}

#ifdef _DEBUG
	g_pClientMode->GetViewportAnimationController()->SetAutoReloadScript(true);
#endif

	m_iHealth = realHealth;
}

bool CHudHealth::ShouldDraw( void )
{
	// No local player yet?
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	if( pPlayer->GetTeamNumber() != TEAM_ALLIES && 
		pPlayer->GetTeamNumber() != TEAM_AXIS )
		return false;

	return CHudElement::ShouldDraw();
}

void CHudHealth::Paint( void )
{
	int x, y, w, h;
	GetBounds( x, y, w, h );

	Color clrIcon(255,255,255,255);

	int xpos = 0;
	int ypos = h - m_pIconBG->Height();
	
	m_pIconBG->DrawSelf( xpos, ypos, clrIcon );

	int nOffset = m_pIconHealthOverlay->Height() * ( 1.0 - ( (float)m_iHealth / 100.0 ) );
	if ( nOffset < m_pIconHealthOverlay->Height() )
	{
		m_pIconHealthOverlay->DrawSelfCropped( xpos + 55, ypos + 27, 0, 0, m_pIconHealthOverlay->Width(), nOffset, clrIcon );
	}

	nOffset = m_pIconHealthBar->Height() * ( 1.0 - ( (float)m_iHealth / 100.0 ) );
	if ( nOffset < m_pIconHealthBar->Height() )
	{
		m_pIconHealthBar->DrawSelfCropped( xpos + 99, ypos + 27 + nOffset, 0, nOffset, m_pIconHealthBar->Width(), m_pIconHealthBar->Height() - nOffset, clrIcon );
	}

	nOffset = m_pIconStaminaBar->Height() * ( 1.0 - ( (float)m_iStamina / 100.0 ) );
	if ( nOffset < m_pIconStaminaBar->Height() )
	{
		m_pIconStaminaBar->DrawSelfCropped( xpos + 8, ypos + 12 + nOffset, 0, nOffset, m_pIconStaminaBar->Width(), m_pIconStaminaBar->Height() - nOffset, clrIcon );
	}

	BaseClass::Paint();
}
