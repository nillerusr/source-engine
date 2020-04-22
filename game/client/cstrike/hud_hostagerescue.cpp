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

class CHudHostageRescueZone : public CHudElement, public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CHudHostageRescueZone, vgui::Panel );

	CHudHostageRescueZone( const char *name );

	virtual bool ShouldDraw();	
	virtual void Paint();

private:
	CPanelAnimationVar( Color, m_clrIcon, "IconColor", "IconColor" );

	CHudTexture *m_pIcon;
};


DECLARE_HUDELEMENT( CHudHostageRescueZone );


CHudHostageRescueZone::CHudHostageRescueZone( const char *pName ) :
	vgui::Panel( NULL, "HudHostageRescueZone" ), CHudElement( pName )
{
	SetParent( g_pClientMode->GetViewport() );
	m_pIcon = NULL;

	SetHiddenBits( HIDEHUD_PLAYERDEAD );

	//=============================================================================
	// HPE_BEGIN:
	// [tj] Add this to the render group that disappears when the scoreboard is up
	//=============================================================================
	RegisterForRenderGroup( "hide_for_scoreboard" );
	//=============================================================================
	// HPE_END
	//=============================================================================
}

bool CHudHostageRescueZone::ShouldDraw()
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	//=============================================================================
	// HPE_BEGIN:
	// [tj] Added base class call
	//=============================================================================
	return ( pPlayer &&	pPlayer->IsInHostageRescueZone() && CHudElement::ShouldDraw());
	//=============================================================================
	// HPE_END
	//=============================================================================
}

void CHudHostageRescueZone::Paint()
{
	if ( !m_pIcon )
	{
		m_pIcon = gHUD.GetIcon( "hostage_rescue" );
	}
	
	if ( m_pIcon )
	{
		int x, y, w, h;
		GetBounds( x, y, w, h );

		m_pIcon->DrawSelf( 0, 0, w, h, m_clrIcon );
	}
}

