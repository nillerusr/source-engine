//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include "clientmode_csnormal.h"
#include "c_cs_player.h"
#include "cs_gamerules.h"

#include "c_cs_hostage.h"
#include "c_plantedc4.h"

class CHudScenarioIcon : public CHudElement, public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CHudScenarioIcon, vgui::Panel );

	CHudScenarioIcon( const char *name );

	virtual bool ShouldDraw();	
	virtual void Paint();

private:
	CPanelAnimationVar( Color, m_clrIcon, "IconColor", "IconColor" );	

	CHudTexture *m_pC4Icon;
	CHudTexture *m_pHostageIcon;
};


DECLARE_HUDELEMENT( CHudScenarioIcon );


CHudScenarioIcon::CHudScenarioIcon( const char *pName ) :
	vgui::Panel( NULL, "HudScenarioIcon" ), CHudElement( pName )
{
	SetParent( g_pClientMode->GetViewport() );
	m_pC4Icon = NULL;
	m_pHostageIcon = NULL;

	SetHiddenBits( HIDEHUD_PLAYERDEAD );
}

bool CHudScenarioIcon::ShouldDraw()
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	return pPlayer && pPlayer->IsAlive();
}

void CHudScenarioIcon::Paint()
{
	// If there is a bomb planted, draw that
	if( g_PlantedC4s.Count() > 0 )
	{
		if ( !m_pC4Icon )
		{
			m_pC4Icon = gHUD.GetIcon( "scenario_c4" );
		}

		if ( m_pC4Icon )
		{
			int x, y, w, h;
			GetBounds( x, y, w, h );

			C_PlantedC4 *pC4 = g_PlantedC4s[0];

			Color c = m_clrIcon;

			c[3] = 80;

			if( pC4->m_flNextGlow - gpGlobals->curtime < 0.1 )
			{
				c[3] = 255;
			}

			if( pC4->IsBombActive() )
				m_pC4Icon->DrawSelf( 0, 0, h, h, c );	//draw it square!
		}
	}

	CCSGameRules *pRules = CSGameRules();

	// If there are hostages, draw how many there are
	if( pRules && pRules->GetNumHostagesRemaining() )
	{
		if ( !m_pHostageIcon )
		{
			m_pHostageIcon = gHUD.GetIcon( "scenario_hostage" );
		}

		if( m_pHostageIcon )
		{
			int xpos = 0;
			int iconWidth = m_pHostageIcon->Width();

			for(int i=0;i<pRules->GetNumHostagesRemaining();i++)
			{
				m_pHostageIcon->DrawSelf( xpos, 0, m_clrIcon );
				xpos += iconWidth;
			}
		}
	}
}

