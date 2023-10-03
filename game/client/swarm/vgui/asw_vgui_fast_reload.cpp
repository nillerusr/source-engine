#include "cbase.h"
#include "asw_vgui_fast_reload.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_weapon.h"
#include <vgui/ISurface.h>
#include "cdll_bounded_cvars.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CASW_VGUI_Fast_Reload::CASW_VGUI_Fast_Reload( vgui::Panel *pParent, const char *pElementName ) 
:	vgui::Panel( pParent, pElementName )
{	
	m_pBar = new vgui::Panel(this, "Bar");
	m_pFastSection = new vgui::Panel(this, "FastSection");
	m_pProgress = new vgui::Panel(this, "Progress");

	m_flLastReloadProgress = 0;
	m_flLastNextAttack = 0;
	m_flLastFastReloadStart = 0;
	m_flLastFastReloadEnd = 0;
}

CASW_VGUI_Fast_Reload::~CASW_VGUI_Fast_Reload()
{
	
}

void CASW_VGUI_Fast_Reload::PerformLayout()
{
	int w, t;
	GetSize(w, t);
	m_pBar->SetBounds(1, 1, w-2, t-2);

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;

	C_ASW_Marine *pMarine = pPlayer->GetMarine();
	if (pMarine && pMarine->GetActiveASWWeapon())
	{
		C_ASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
		if (pWeapon)
			PositionBars(pWeapon);
	}	
}

void CASW_VGUI_Fast_Reload::PositionBars( C_ASW_Weapon *pWeapon )
{
	if ( !pWeapon )
		return;

	bool bFailure = pWeapon->m_bFastReloadFailure;
	bool bSuccess = pWeapon->m_bFastReloadSuccess;

	float flBGAlpha = 255.0f;
	Color colorBG =		Color( 32, 32, 32, flBGAlpha );
	Color colorWindow =	Color( 170, 170, 170, 255 );
	Color colorBar =	Color( 255, 255, 255, 255 );
	float flAlphaFade = 1.0f;

	int w, t;
	GetSize(w, t);

	int iXPos = 0, iYPos = 0;

	float fStart = pWeapon->m_fReloadStart;
	if ( !bFailure && !bSuccess )
	{
		m_flLastNextAttack = pWeapon->m_flNextPrimaryAttack;
	}

	float fTotalTime = m_flLastNextAttack - fStart;
	if (fTotalTime <= 0)
		fTotalTime = 0.1f;

	float flProgress = 0.0f;
	// if we're in single player, the progress code in the weapon doesn't run on the client because we aren't predicting
	if ( !cl_predict->GetInt() )
		flProgress = (gpGlobals->curtime - fStart) / fTotalTime;
	else
		flProgress = pWeapon->m_fReloadProgress;

	if ( !bFailure && !bSuccess )
	{
		m_flLastReloadProgress = flProgress;
		m_flLastFastReloadStart = ((pWeapon->m_fFastReloadStart - fStart) / fTotalTime)+0.015f;
		m_flLastFastReloadEnd = ((pWeapon->m_fFastReloadEnd - fStart) / fTotalTime)-0.015f;
	}
	//Msg( "C: %f - %f - %f Reload Progress = %f\n", gpGlobals->curtime, fFastStart, fFastEnd, flProgress );

	if ( bFailure )
	{
		flBGAlpha = 255.0f;
		if ( flProgress > 0.75f )
			flAlphaFade = (1.0f - flProgress) / 0.25f;

		colorBG = Color( 128, 32, 16, flBGAlpha * flAlphaFade );
		colorWindow = Color( 200, 128, 128, 250 * flAlphaFade );
		colorBar =	Color( 255, 255, 255, 255 * flAlphaFade  );
	}
	else if ( bSuccess )
	{
		flAlphaFade = 1.0f - flProgress;
		colorBG = Color( 170, 170, 170, flBGAlpha * flAlphaFade );
		colorWindow = Color( 190, 220, 190, 255 * flAlphaFade );
		colorBar =	Color( 255, 255, 255, 255 * flAlphaFade  );
	}

	SetAlpha( flBGAlpha * flAlphaFade );
	m_pBar->SetBgColor( colorBG );
	m_pBar->SetAlpha( flBGAlpha * flAlphaFade );
	m_pFastSection->SetBgColor( colorWindow );
	m_pFastSection->SetAlpha( 255 * flAlphaFade );
	m_pProgress->SetBgColor( colorBar );
	m_pProgress->SetAlpha( 255 * flAlphaFade );

	m_pFastSection->SetBounds(iXPos+w*m_flLastFastReloadStart, iYPos+3, w * (m_flLastFastReloadEnd - m_flLastFastReloadStart), t-6);
	m_pProgress->SetBounds(iXPos+w*(MAX(m_flLastReloadProgress-0.01f,0)), 0, w*0.01f, t);
}

void CASW_VGUI_Fast_Reload::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetBgColor( Color( 255, 255, 255, 200 ) );
	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(true);

	m_pBar->SetPaintBackgroundType(0);
	m_pBar->SetPaintBackgroundEnabled(true);
	m_pBar->SetBgColor( Color(16,16,16,240) );

	m_pFastSection->SetPaintBackgroundType(0);
	m_pFastSection->SetPaintBackgroundEnabled(true);
	m_pFastSection->SetBgColor( Color(170, 170, 170, 255) );

	m_pProgress->SetPaintBackgroundType(0);
	m_pProgress->SetPaintBackgroundEnabled(true);
	m_pProgress->SetBgColor( Color(255,255,255,255) );

	
	SetMouseInputEnabled(false);
}

void CASW_VGUI_Fast_Reload::OnThink()
{
	SetAlpha( 0 );

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;

	C_ASW_Marine *pMarine = pPlayer->GetMarine();
	if ( !pMarine )
		return;

	C_ASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
	if ( !pWeapon || !pWeapon->IsReloading() )
		return;

	PositionBars(pWeapon);	
}