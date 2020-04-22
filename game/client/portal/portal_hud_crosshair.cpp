//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "portal_hud_crosshair.h"
#include "iclientmode.h"
#include "c_portal_player.h"
#include "view.h"
#include "weapon_portalbase.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "ivrenderview.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar crosshair;
extern ConVar cl_observercrosshair;

using namespace vgui;

int ScreenTransform( const Vector& point, Vector& screen );

DECLARE_HUDELEMENT( CHudPortalCrosshair );

CHudPortalCrosshair::CHudPortalCrosshair( const char *pElementName ) :
CHudElement( pElementName ), BaseClass( NULL, "HudPortalCrosshair" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_pCrosshair = 0;

	m_clrCrosshair = Color( 0, 0, 0, 0 );

	m_vecCrossHairOffsetAngle.Init();

	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_CROSSHAIR );
}

void CHudPortalCrosshair::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_pDefaultCrosshair = gHUD.GetIcon("crosshair_default");
	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and 
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHudPortalCrosshair::ShouldDraw()
{
	// NOTE: Portal crosshair should no longer be in use, but I'm leaving the code here until X360 lock down... we don't want to draw this ever. -Jeep
	return false;
	C_Portal_Player *pPlayer = C_Portal_Player::GetLocalPortalPlayer();

	if ( !pPlayer )
		return false;

	CWeaponPortalBase *pWeapon = dynamic_cast<CWeaponPortalBase*>( pPlayer->GetActiveWeapon() );

	if ( !pWeapon )
		return false;

	bool bNeedsDraw = m_pCrosshair && 
			crosshair.GetInt() &&
			!engine->IsDrawingLoadingImage() &&
			!engine->IsPaused() && 
			g_pClientMode->ShouldDrawCrosshair() &&
			!( pPlayer->GetFlags() & FL_FROZEN ) &&
			( pPlayer->entindex() == render->GetViewEntity() ) &&
			!pPlayer->IsInVGuiInputMode() &&
			( pPlayer->IsAlive() ||	( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE ) || ( cl_observercrosshair.GetBool() && pPlayer->GetObserverMode() == OBS_MODE_ROAMING ) );

	return ( bNeedsDraw && CHudElement::ShouldDraw() );
}

void CHudPortalCrosshair::Paint( void )
{
	if ( !m_pCrosshair )
		return;

	if ( !IsCurrentViewAccessAllowed() )
		return;

	m_curViewAngles = CurrentViewAngles();
	m_curViewOrigin = CurrentViewOrigin();

	float x, y;
	x = ScreenWidth()/2;
	y = ScreenHeight()/2;

	// MattB - m_vecCrossHairOffsetAngle is the autoaim angle.
	// if we're not using autoaim, just draw in the middle of the 
	// screen
	if ( m_vecCrossHairOffsetAngle != vec3_angle )
	{
		QAngle angles;
		Vector forward;
		Vector point, screen;

		// this code is wrong
		angles = m_curViewAngles + m_vecCrossHairOffsetAngle;
		AngleVectors( angles, &forward );
		VectorAdd( m_curViewOrigin, forward, point );
		ScreenTransform( point, screen );

		x += 0.5f * screen[0] * ScreenWidth() + 0.5f;
		y += 0.5f * screen[1] * ScreenHeight() + 0.5f;
	}

	m_pCrosshair->DrawSelf( 
		x - 0.5f * m_pCrosshair->Width(), 
		y - 0.5f * m_pCrosshair->Height(),
		m_clrCrosshair );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudPortalCrosshair::SetCrosshairAngle( const QAngle& angle )
{
	VectorCopy( angle, m_vecCrossHairOffsetAngle );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudPortalCrosshair::SetCrosshair( CHudTexture *texture, Color& clr )
{
	m_pCrosshair = texture;
	m_clrCrosshair = clr;
}

//-----------------------------------------------------------------------------
// Purpose: Resets the crosshair back to the default
//-----------------------------------------------------------------------------
void CHudPortalCrosshair::ResetCrosshair()
{
	Color white(255, 255, 255, 255);
	SetCrosshair( m_pDefaultCrosshair, white );
}
