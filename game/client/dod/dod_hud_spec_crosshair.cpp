//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "dod_hud_spec_crosshair.h"
#include "iclientmode.h"
#include "view.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "ivrenderview.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//ConVar crosshair( "crosshair", "1", FCVAR_ARCHIVE );
//ConVar cl_observercrosshair( "cl_observercrosshair", "1", FCVAR_ARCHIVE );

using namespace vgui;

int ScreenTransform( const Vector& point, Vector& screen );

DECLARE_HUDELEMENT( CHudSpecCrosshair );

CHudSpecCrosshair::CHudSpecCrosshair( const char *pElementName ) :
  CHudElement( pElementName ), BaseClass( NULL, "HudSpecCrosshair" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_pCrosshair = 0;

	m_clrCrosshair = Color( 255, 255, 255, 255 );

	SetHiddenBits( HIDEHUD_CROSSHAIR );
}

void CHudSpecCrosshair::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_pCrosshair = gHUD.GetIcon("crosshair_default");

	SetPaintBackgroundEnabled( false );
}

void CHudSpecCrosshair::Paint( void )
{
	if ( !g_pClientMode->ShouldDrawCrosshair() )
		return;

	if ( !m_pCrosshair )
		return;

	if ( engine->IsDrawingLoadingImage() || engine->IsPaused() )
		return;
	
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();

	if ( !pPlayer )
		return;

	// draw a crosshair only if alive or spectating in eye
	bool shouldDraw = false;

	if ( pPlayer->GetObserverMode() == OBS_MODE_ROAMING /*&& cl_observercrosshair.GetBool()*/ )
		shouldDraw = true;

	if ( !shouldDraw )
		return;

	if ( pPlayer->entindex() != render->GetViewEntity() )
		return;

	float x, y;
	x = ScreenWidth()/2;
	y = ScreenHeight()/2;

	m_pCrosshair->DrawSelf( 
			x - 0.5f * m_pCrosshair->Width(), 
			y - 0.5f * m_pCrosshair->Height(),
			m_clrCrosshair );
}