//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "hud_numericdisplay.h"
#include "iclientmode.h"
#include "c_tf_player.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialvar.h"

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <KeyValues.h>
#include <vgui_controls/AnimationController.h>

//for screenfade
#include "ivieweffects.h"
#include "shake.h"
#include "view_scene.h"

#include "tf_weapon_sniperrifle.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Draws the sniper chargeup meter
//-----------------------------------------------------------------------------
class CHudScopeCharge : public vgui::Panel, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CHudScopeCharge, vgui::Panel );

public:
	CHudScopeCharge( const char *pElementName );

	void	Init( void );

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *scheme);
	virtual void Paint( void );

private:
	int m_iChargeupTexture;
	int m_iChargeupTextureWidth;
	CPanelAnimationVarAliasType( float, m_iChargeup_xpos, "chargeup_xpos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_iChargeup_ypos, "chargeup_ypos", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_iChargeup_wide, "chargeup_wide", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_iChargeup_tall, "chargeup_tall", "0", "proportional_float" );
};

DECLARE_HUDELEMENT_DEPTH( CHudScopeCharge, 100 );

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudScopeCharge::CHudScopeCharge( const char *pElementName ) : CHudElement(pElementName), BaseClass(NULL, "HudScopeCharge")
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_PLAYERDEAD );
}

//-----------------------------------------------------------------------------
// Purpose: standard hud element init function
//-----------------------------------------------------------------------------
void CHudScopeCharge::Init( void )
{
	m_iChargeupTexture = vgui::surface()->CreateNewTextureID();

	vgui::surface()->DrawSetTextureFile(m_iChargeupTexture, "HUD/sniperscope_numbers", true, false);

	// Get the texture size
	int ignored;
	surface()->DrawGetTextureSize( m_iChargeupTexture, m_iChargeupTextureWidth, ignored );
}

//-----------------------------------------------------------------------------
// Purpose: sets scheme colors
//-----------------------------------------------------------------------------
void CHudScopeCharge::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings(scheme);

	SetPaintBackgroundEnabled(false);
	SetPaintBorderEnabled(false);
}

//-----------------------------------------------------------------------------
// Purpose: draws the zoom effect
//-----------------------------------------------------------------------------
void CHudScopeCharge::Paint( void )
{
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( GetSpectatorTarget() != 0 && GetSpectatorMode() == OBS_MODE_IN_EYE )
	{
		pPlayer = (C_TFPlayer *)UTIL_PlayerByIndex( GetSpectatorTarget() );
	}

	if ( !pPlayer )
		return;

	if ( !pPlayer->m_Shared.InCond( TF_COND_ZOOMED ) )
		return;

	// Make sure the current weapon is a sniper rifle
	CTFSniperRifle *pWeapon = assert_cast<CTFSniperRifle*>(pPlayer->GetActiveTFWeapon());
	if ( !pWeapon )
		return;

	// Actual charge value is set through a material proxy in the sniper rifle class

	int wide, tall;
	GetSize( wide, tall );

	vgui::surface()->DrawSetColor(255,255,255,255);
	vgui::surface()->DrawSetTexture(m_iChargeupTexture);
	vgui::surface()->DrawTexturedRect( 0, 0, wide, tall );
}

//-----------------------------------------------------------------------------
// Purpose: Draws the zoom screen
//-----------------------------------------------------------------------------
class CHudScope : public vgui::Panel, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CHudScope, vgui::Panel );

public:
			CHudScope( const char *pElementName );
	
	void	Init( void );

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *scheme);
	virtual void Paint( void );
	virtual bool ShouldDraw( void );

private:
	int m_iScopeTexture[4];
};

DECLARE_HUDELEMENT_DEPTH( CHudScope, 100 );

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudScope::CHudScope( const char *pElementName ) : CHudElement(pElementName), BaseClass(NULL, "HudScope")
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	
	SetHiddenBits( HIDEHUD_PLAYERDEAD );
}

//-----------------------------------------------------------------------------
// Purpose: standard hud element init function
//-----------------------------------------------------------------------------
void CHudScope::Init( void )
{
	int i;
	for( i=0;i<4;i++ )
	{
		m_iScopeTexture[i] = vgui::surface()->CreateNewTextureID();
	}

	vgui::surface()->DrawSetTextureFile(m_iScopeTexture[0], "HUD/scope_sniper_ul", true, false);
	vgui::surface()->DrawSetTextureFile(m_iScopeTexture[1], "HUD/scope_sniper_ur", true, false);
	vgui::surface()->DrawSetTextureFile(m_iScopeTexture[2], "HUD/scope_sniper_lr", true, false);
	vgui::surface()->DrawSetTextureFile(m_iScopeTexture[3], "HUD/scope_sniper_ll", true, false);

	// remove ourselves from the global group so the scoreboard doesn't hide us
	UnregisterForRenderGroup( "global" );
}

//-----------------------------------------------------------------------------
// Purpose: sets scheme colors
//-----------------------------------------------------------------------------
void CHudScope::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings(scheme);

	SetPaintBackgroundEnabled(false);
	SetPaintBorderEnabled(false);

	int screenWide, screenTall;
	GetHudSize(screenWide, screenTall);
	SetBounds(0, 0, screenWide, screenTall);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudScope::ShouldDraw( void )
{
	// Because our spectator gui is drawn before this in the viewport hierarchy 
	// don't draw the scope ring and refraction when in spectator
	if ( GetSpectatorTarget() != 0 && GetSpectatorMode() == OBS_MODE_IN_EYE )
	{
		return false;
	}

	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pPlayer || !pPlayer->m_Shared.InCond( TF_COND_ZOOMED ) )
		return false;

	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: draws the zoom effect
//-----------------------------------------------------------------------------
void CHudScope::Paint( void )
{
	// We need to update the refraction texture so the scope can refract it
	UpdateRefractTexture();

	int screenWide, screenTall;
	GetHudSize(screenWide, screenTall);

	// calculate the bounds in which we should draw the scope
	int xMid = screenWide / 2;
	int yMid = screenTall / 2;

	// width of the drawn scope. in widescreen, we draw the sides with primitives
	int wide = ( screenTall / 3 ) * 4;

	int xLeft = xMid - wide/2;
	int xRight = xMid + wide/2;
	int yTop = 0;
	int yBottom = screenTall;

	float uv1 = 0.5f / 256.0f, uv2 = 1.0f - uv1;

	vgui::Vertex_t vert[4];	
	
	Vector2D uv11( uv1, uv1 );
	Vector2D uv12( uv1, uv2 );
	Vector2D uv21( uv2, uv1 );
	Vector2D uv22( uv2, uv2 );

	vgui::surface()->DrawSetColor(0,0,0,255);

	//upper left
	vgui::surface()->DrawSetTexture(m_iScopeTexture[0]);
	vert[0].Init( Vector2D( xLeft, yTop ), uv11 );
	vert[1].Init( Vector2D( xMid,  yTop ), uv21 );
	vert[2].Init( Vector2D( xMid,  yMid ), uv22 );
	vert[3].Init( Vector2D( xLeft, yMid ), uv12 );
	vgui::surface()->DrawTexturedPolygon(4, vert);

	// top right
	vgui::surface()->DrawSetTexture(m_iScopeTexture[1]);
	vert[0].Init( Vector2D( xMid - 1, yTop ), uv11 );
	vert[1].Init( Vector2D( xRight,   yTop ), uv21 );
	vert[2].Init( Vector2D( xRight,   yMid + 1 ), uv22 );
	vert[3].Init( Vector2D( xMid - 1, yMid + 1 ), uv12 );
	vgui::surface()->DrawTexturedPolygon(4, vert);

	// bottom right
	vgui::surface()->DrawSetTexture(m_iScopeTexture[2]);
	vert[0].Init( Vector2D( xMid,   yMid ), uv11 );
	vert[1].Init( Vector2D( xRight, yMid ), uv21 );
	vert[2].Init( Vector2D( xRight, yBottom ), uv22 );
	vert[3].Init( Vector2D( xMid,   yBottom ), uv12 );
	vgui::surface()->DrawTexturedPolygon( 4, vert );

	// bottom left
	vgui::surface()->DrawSetTexture(m_iScopeTexture[3]);
	vert[0].Init( Vector2D( xLeft, yMid ), uv11 );
	vert[1].Init( Vector2D( xMid,  yMid ), uv21 );
	vert[2].Init( Vector2D( xMid,  yBottom ), uv22 );
	vert[3].Init( Vector2D( xLeft, yBottom), uv12 );
	vgui::surface()->DrawTexturedPolygon(4, vert);

	if ( wide < screenWide )
	{
		// Left block
		vgui::surface()->DrawFilledRect( 0, 0, xLeft, screenTall );
		
		// Right block
		vgui::surface()->DrawFilledRect( xRight, 0, screenWide, screenTall );
	}
}