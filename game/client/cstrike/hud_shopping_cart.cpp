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
#include "cs_gamerules.h"
#include "hud_numericdisplay.h"
#include <vgui_controls/AnimationController.h>


class CHudShoppingCart : public CHudElement, public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CHudShoppingCart, vgui::Panel );

	CHudShoppingCart( const char *name );

	virtual bool ShouldDraw();	
	virtual void Paint();
	virtual void Init();


private:
	CPanelAnimationVar( Color, m_clrIcon, "IconColor", "IconColor" );

	CHudTexture *m_pCartIcon;
	bool		m_bPrevState;
};


DECLARE_HUDELEMENT( CHudShoppingCart );


CHudShoppingCart::CHudShoppingCart( const char *pName ) :
	vgui::Panel( NULL, "HudShoppingCart" ), CHudElement( pName )
{
	SetParent( g_pClientMode->GetViewport() );
	m_pCartIcon = NULL;
	
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


void CHudShoppingCart::Init()
{
}


bool CHudShoppingCart::ShouldDraw()
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	//=============================================================================
	// HPE_BEGIN:
	// [tj] Added base class call
	//=============================================================================
	return ( pPlayer && pPlayer->IsInBuyZone() && !CSGameRules()->IsBuyTimeElapsed() && CHudElement::ShouldDraw() );
	//=============================================================================
	// HPE_END
	//=============================================================================
}


void CHudShoppingCart::Paint()
{
	if ( !m_pCartIcon )
	{
		m_pCartIcon = gHUD.GetIcon( "shopping_cart" );
	}
	
	if ( m_pCartIcon )
	{
		int x, y, w, h;
		GetBounds( x, y, w, h );

		m_pCartIcon->DrawSelf( 0, 0, w, h, m_clrIcon );
	}
}

