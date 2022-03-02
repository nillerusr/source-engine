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
#include "c_cs_player.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialvar.h"

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <KeyValues.h>
#include <vgui_controls/AnimationController.h>

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
	void	LevelInit( void );

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *scheme);
	virtual void Paint( void );

private:
	CMaterialReference m_ScopeMaterial;	
	CMaterialReference m_DustOverlayMaterial;

	int m_iScopeArcTexture;
	int m_iScopeDustTexture;
};

DECLARE_HUDELEMENT_DEPTH( CHudScope, 70 );

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
	m_iScopeArcTexture = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(m_iScopeArcTexture, "sprites/scope_arc", true, false);

	m_iScopeDustTexture = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile(m_iScopeDustTexture, "overlays/scope_lens", true, false);
}

//-----------------------------------------------------------------------------
// Purpose: standard hud element init function
//-----------------------------------------------------------------------------
void CHudScope::LevelInit( void )
{
	Init();
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
	C_CSPlayer *pPlayer = dynamic_cast<C_CSPlayer *>(C_BasePlayer::GetLocalPlayer());
	
	if ( pPlayer == NULL )
		return;

	CWeaponCSBase *pWeapon = pPlayer->GetActiveCSWeapon();
		
	if( !pWeapon )
		return;

	Assert( m_iScopeArcTexture );
	Assert( m_iScopeDustTexture );

	// see if we're zoomed with a sniper rifle
	if( pPlayer->GetFOV() != pPlayer->GetDefaultFOV() &&
		pWeapon->GetCSWpnData().m_WeaponType == WEAPONTYPE_SNIPER_RIFLE )
	{
		int screenWide, screenTall;
		GetHudSize(screenWide, screenTall);

		// calculate the bounds in which we should draw the scope
		int inset = screenTall / 16;
		int y1 = inset;
		int x1 = (screenWide - screenTall) / 2 + inset; 
		int y2 = screenTall - inset;
		int x2 = screenWide - x1;

		int x = screenWide / 2;
		int y = screenTall / 2;

		float uv1 = 0.5f / 256.0f, uv2 = 1.0f - uv1;

		vgui::Vertex_t vert[4];	
		
		Vector2D uv11( uv1, uv1 );
		Vector2D uv12( uv1, uv2 );
		Vector2D uv21( uv2, uv1 );
		Vector2D uv22( uv2, uv2 );

		int xMod = ( screenWide / 2 );
		int yMod = ( screenTall / 2 );

		int iMiddleX = (screenWide / 2 );
		int iMiddleY = (screenTall / 2 );

		vgui::surface()->DrawSetTexture( m_iScopeDustTexture );
		vgui::surface()->DrawSetColor( 255, 255, 255, 255 );

		vert[0].Init( Vector2D( iMiddleX + xMod, iMiddleY + yMod ), uv21 );
		vert[1].Init( Vector2D( iMiddleX - xMod, iMiddleY + yMod ), uv11 );
		vert[2].Init( Vector2D( iMiddleX - xMod, iMiddleY - yMod ), uv12 );
		vert[3].Init( Vector2D( iMiddleX + xMod, iMiddleY - yMod ), uv22 );
		vgui::surface()->DrawTexturedPolygon( 4, vert );
		
		vgui::surface()->DrawSetColor(0,0,0,255);

		//Draw the reticle with primitives
		vgui::surface()->DrawLine( 0, y, screenWide, y );
		vgui::surface()->DrawLine( x, 0, x, screenTall );

		//Draw the outline
		vgui::surface()->DrawSetTexture(m_iScopeArcTexture);

		// bottom right
		vert[0].Init( Vector2D( x, y ), uv11 );
		vert[1].Init( Vector2D( x2, y ), uv21 );
		vert[2].Init( Vector2D( x2, y2 ), uv22 );
		vert[3].Init( Vector2D( x, y2 ), uv12 );
		vgui::surface()->DrawTexturedPolygon( 4, vert );

		// top right
		vert[0].Init( Vector2D( x - 1, y1 ), uv12 );
		vert[1].Init( Vector2D ( x2, y1 ), uv22 );
		vert[2].Init( Vector2D( x2, y + 1 ), uv21 );
		vert[3].Init( Vector2D( x - 1, y + 1 ), uv11 );
		vgui::surface()->DrawTexturedPolygon(4, vert);

		// bottom left
		vert[0].Init( Vector2D( x1, y ), uv21 );
		vert[1].Init( Vector2D( x, y ), uv11 );
		vert[2].Init( Vector2D( x, y2 ), uv12 );
		vert[3].Init( Vector2D( x1, y2), uv22 );
		vgui::surface()->DrawTexturedPolygon(4, vert);

		// top left
		vert[0].Init( Vector2D( x1, y1 ), uv22 );
		vert[1].Init( Vector2D( x, y1 ), uv12 );
		vert[2].Init( Vector2D( x, y ), uv11 );
		vert[3].Init( Vector2D( x1, y ), uv21 );
		vgui::surface()->DrawTexturedPolygon(4, vert);
	
		vgui::surface()->DrawFilledRect(0, 0, screenWide, y1);				// top
		vgui::surface()->DrawFilledRect(0, y2, screenWide, screenTall);		// bottom
		vgui::surface()->DrawFilledRect(0, y1, x1, screenTall);				// left
		vgui::surface()->DrawFilledRect(x2, y1, screenWide, screenTall);	// right
	}
}
