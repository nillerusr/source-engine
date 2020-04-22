//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include "c_cs_player.h"
#include "clientmode_csnormal.h"
#include "weapon_c4.h"

ConVar cl_c4progressbar( "cl_c4progressbar", "1", 0, "Draw progress bar when defusing the C4" );

class CHudProgressBar : public CHudElement, public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CHudProgressBar, vgui::Panel );

	CHudProgressBar( const char *name );

	// vgui overrides
	virtual void Paint();
	virtual bool ShouldDraw();

	CPanelAnimationVar( Color, m_clrProgress, "ProgressBarFg", "ProgressBar.FgColor" );
};


DECLARE_HUDELEMENT( CHudProgressBar );


CHudProgressBar::CHudProgressBar( const char *name ) :
	vgui::Panel( NULL, "HudProgressBar" ), CHudElement( name )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	
	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_WEAPONSELECTION );
}

void CHudProgressBar::Paint()
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if( pPlayer && pPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		C_BaseEntity *pTarget = pPlayer->GetObserverTarget();

		if( pTarget && pTarget->IsPlayer() )
		{
			pPlayer = ToCSPlayer( pTarget );

			if( !pPlayer->IsAlive() )
				return;
		}
		else
			return;
	}

	if ( !pPlayer )
		return;

	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );
	
	tall = 10;

	int xOffset=0;
	int yOffset=0;
	
	Color clr = m_clrProgress;

	clr[3] = 160;
	vgui::surface()->DrawSetColor( clr );
	vgui::surface()->DrawOutlinedRect( xOffset, yOffset, xOffset+wide, yOffset+tall );

	if( pPlayer->m_iProgressBarDuration > 0 )
	{
		// ProgressBarStartTime is now with respect to m_flSimulationTime rather than local time
		float percent = (pPlayer->m_flSimulationTime - pPlayer->m_flProgressBarStartTime) / (float)pPlayer->m_iProgressBarDuration;
		percent = clamp( percent, 0, 1 );
		
		clr[3] = 240;
		vgui::surface()->DrawSetColor( clr );
		vgui::surface()->DrawFilledRect( xOffset+2, yOffset+2, xOffset+(int)(percent*wide)-2, yOffset+tall-2 );
	}
}


bool CHudProgressBar::ShouldDraw()
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if( pPlayer && pPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		C_BaseEntity *pTarget = pPlayer->GetObserverTarget();

		if( pTarget && pTarget->IsPlayer() )
		{
			pPlayer = ToCSPlayer( pTarget );

			if( !pPlayer->IsAlive() )
				return false;
		}
		else
			return false;
	}

	if( !pPlayer || pPlayer->m_iProgressBarDuration == 0 || pPlayer->m_lifeState == LIFE_DEAD )
	{
		return false;
	}

	return cl_c4progressbar.GetBool();
}



