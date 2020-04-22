//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include "c_dod_player.h"
#include "iclientmode.h"
#include "c_dod_playerresource.h"
#include "weapon_mg42.h"


class CHudMGHeatIcon : public CHudElement, public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CHudMGHeatIcon, vgui::Panel );

	CHudMGHeatIcon( const char *name );
	
	virtual void Paint();
	virtual void Init();
	virtual bool ShouldDraw();

private:
	CHudTexture *m_pBarrel;
	CHudTexture *m_pHotBarrel;

	Color m_clrIcon;
};


DECLARE_HUDELEMENT( CHudMGHeatIcon );


CHudMGHeatIcon::CHudMGHeatIcon( const char *pName ) :
	vgui::Panel( NULL, "HudMGHeatIcon" ), CHudElement( pName )
{
	SetParent( g_pClientMode->GetViewport() );

	m_clrIcon = Color(255,255,255,255);
	
	SetHiddenBits( HIDEHUD_PLAYERDEAD );
}

void CHudMGHeatIcon::Init()
{
	if( !m_pBarrel )
	{
		m_pBarrel = gHUD.GetIcon( "hud_barrel" );
	}

	if( !m_pHotBarrel )
	{
		m_pHotBarrel = gHUD.GetIcon( "hud_barrelo" );
	}
}

bool CHudMGHeatIcon::ShouldDraw()
{
	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if( !pPlayer )
		return false;

	//is their active weapon an mg42 ?
	CWeaponDODBase *pWeapon = pPlayer->GetActiveDODWeapon();
	if( pWeapon && pWeapon->IsA( WEAPON_MG42 ) )
		return true;

	return false;
}

void CHudMGHeatIcon::Paint()
{
	int x,y,w,h;
	GetBounds( x,y,w,h );

	if( !m_pBarrel )
	{
		m_pBarrel = gHUD.GetIcon( "hud_barrel" );
	}

	if( !m_pHotBarrel )
	{
		m_pHotBarrel = gHUD.GetIcon( "hud_barrelo" );
	}

	//draw the base
	m_pBarrel->DrawSelf( 0, 0, m_clrIcon );

	float flPercentHotness = 0.0f;

	C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

	if( !pPlayer )
		return;

	CWeaponDODBase *pWeapon = pPlayer->GetActiveDODWeapon();
	if( pWeapon && pWeapon->IsA( WEAPON_MG42 ) )
	{	
		CWeaponMG42 *pMG42 = (CWeaponMG42 *)pWeapon;

		if( pMG42 )
		{
			flPercentHotness = (float)pMG42->GetWeaponHeat() / 100.0;
		}
	}
	
	int nOffset = m_pHotBarrel->Height() * ( 1.0 - flPercentHotness );
	if ( nOffset < m_pHotBarrel->Height() )
	{
		m_pHotBarrel->DrawSelfCropped( 0, nOffset, 0, nOffset, m_pHotBarrel->Width(), m_pHotBarrel->Height() - nOffset, m_clrIcon );
	}


}

