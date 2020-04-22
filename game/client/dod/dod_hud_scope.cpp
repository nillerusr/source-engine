//========= Copyright Valve Corporation, All rights reserved. ============//
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
#include "c_dod_player.h"
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

#include "weapon_dodbasegun.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

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

private:
	int m_iScopeSpringfield[4];
	int m_iScopeK98[4];
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
		m_iScopeSpringfield[i] = vgui::surface()->CreateNewTextureID();
		m_iScopeK98[i] = vgui::surface()->CreateNewTextureID();
	}

	vgui::surface()->DrawSetTextureFile(m_iScopeSpringfield[0],		"sprites/scopes/scope_spring_ul", true, false);
	vgui::surface()->DrawSetTextureFile(m_iScopeSpringfield[1],		"sprites/scopes/scope_spring_ur", true, false);
	vgui::surface()->DrawSetTextureFile(m_iScopeSpringfield[2],		"sprites/scopes/scope_spring_lr", true, false);
	vgui::surface()->DrawSetTextureFile(m_iScopeSpringfield[3],		"sprites/scopes/scope_spring_ll", true, false);

	vgui::surface()->DrawSetTextureFile(m_iScopeK98[0],		"sprites/scopes/scope_k43_ul", true, false);
	vgui::surface()->DrawSetTextureFile(m_iScopeK98[1],		"sprites/scopes/scope_k43_ur", true, false);
	vgui::surface()->DrawSetTextureFile(m_iScopeK98[2],		"sprites/scopes/scope_k43_lr", true, false);
	vgui::surface()->DrawSetTextureFile(m_iScopeK98[3],		"sprites/scopes/scope_k43_ll", true, false);
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
// Purpose: draws the zoom effect
//-----------------------------------------------------------------------------
void CHudScope::Paint( void )
{
	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();
	
	if ( pPlayer == NULL )
		return;

	if( pPlayer->GetFOV() >= 90.0f )
		return;

	CWeaponDODBase *pWeapon = pPlayer->GetActiveDODWeapon();
		
	if( !pWeapon )
		return;

	Assert( m_iScopeSpringfield );
	Assert( m_iScopeK98 );
	
	int *piScopeTex = NULL;

	switch( pWeapon->GetWeaponID() )
	{
	case WEAPON_SPRING:
		piScopeTex = m_iScopeSpringfield;
		break;
	case WEAPON_K98_SCOPED:
		piScopeTex = m_iScopeK98;
		break;
	default:
		return;
	}

	if( !piScopeTex )
		return;

	// see if we're zoomed with a sniper rifle
	if( pPlayer->GetFOV() < 90 &&
		pWeapon->GetDODWpnData().m_WeaponType == WPN_TYPE_SNIPER )
	{
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

		//Draw the outline

		//upper left
		vgui::surface()->DrawSetTexture(piScopeTex[0]);
		vert[0].Init( Vector2D( xLeft, yTop ), uv11 );
		vert[1].Init( Vector2D( xMid,  yTop ), uv21 );
		vert[2].Init( Vector2D( xMid,  yMid ), uv22 );
		vert[3].Init( Vector2D( xLeft, yMid ), uv12 );
		vgui::surface()->DrawTexturedPolygon(4, vert);

		// top right
		vgui::surface()->DrawSetTexture(piScopeTex[1]);
		vert[0].Init( Vector2D( xMid - 1, yTop ), uv11 );
		vert[1].Init( Vector2D( xRight,   yTop ), uv21 );
		vert[2].Init( Vector2D( xRight,   yMid + 1 ), uv22 );
		vert[3].Init( Vector2D( xMid - 1, yMid + 1 ), uv12 );
		vgui::surface()->DrawTexturedPolygon(4, vert);

		// bottom right
		vgui::surface()->DrawSetTexture(piScopeTex[2]);
		vert[0].Init( Vector2D( xMid,   yMid ), uv11 );
		vert[1].Init( Vector2D( xRight, yMid ), uv21 );
		vert[2].Init( Vector2D( xRight, yBottom ), uv22 );
		vert[3].Init( Vector2D( xMid,   yBottom ), uv12 );
		vgui::surface()->DrawTexturedPolygon( 4, vert );

		// bottom left
		vgui::surface()->DrawSetTexture(piScopeTex[3]);
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

		if ( pWeapon->GetWeaponID() == WEAPON_SPRING )
		{
			//Draw the reticle with primitives
			vgui::surface()->DrawLine( 0, yMid, screenWide, yMid );
			vgui::surface()->DrawLine( xMid, 0, xMid, screenTall );
		}
	}
}