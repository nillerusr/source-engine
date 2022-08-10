//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "c_tf_player.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ProgressBar.h>
#include "tf_weapon_medigun.h"
#include <vgui_controls/AnimationController.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudMedicChargeMeter : public CHudElement, public EditablePanel
{
	DECLARE_CLASS_SIMPLE( CHudMedicChargeMeter, EditablePanel );

public:
	CHudMedicChargeMeter( const char *pElementName );

	virtual void	ApplySchemeSettings( IScheme *scheme );
	virtual bool	ShouldDraw( void );
	virtual void	OnTick( void );

private:
	vgui::ContinuousProgressBar *m_pChargeMeter;

	bool m_bCharged;
	float m_flLastChargeValue;
};

DECLARE_HUDELEMENT( CHudMedicChargeMeter );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudMedicChargeMeter::CHudMedicChargeMeter( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudMedicCharge" )
{
	Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_pChargeMeter = new ContinuousProgressBar( this, "ChargeMeter" );

	SetHiddenBits( HIDEHUD_MISCSTATUS );

	vgui::ivgui()->AddTickSignal( GetVPanel() );

	m_bCharged = false;
	m_flLastChargeValue = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMedicChargeMeter::ApplySchemeSettings( IScheme *pScheme )
{
	// load control settings...
	LoadControlSettings( "resource/UI/HudMedicCharge.res" );

	BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudMedicChargeMeter::ShouldDraw( void )
{
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pPlayer || !pPlayer->IsPlayerClass( TF_CLASS_MEDIC ) || !pPlayer->IsAlive() )
	{
		return false;
	}

	CTFWeaponBase *pWpn = pPlayer->GetActiveTFWeapon();

	if ( !pWpn )
	{
		return false;
	}

	if ( pWpn->GetWeaponID() != TF_WEAPON_MEDIGUN )
	{
		return false;
	}

	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMedicChargeMeter::OnTick( void )
{
	C_TFPlayer *pPlayer = C_TFPlayer::GetLocalTFPlayer();

	if ( !pPlayer )
		return;

	CTFWeaponBase *pWpn = pPlayer->GetActiveTFWeapon();

	if ( !pWpn || ( pWpn->GetWeaponID() != TF_WEAPON_MEDIGUN ) )
		return;

	CWeaponMedigun *pMedigun = static_cast< CWeaponMedigun *>( pWpn );

	if ( !pMedigun )
		return;

	float flCharge = pMedigun->GetChargeLevel();

	if ( flCharge != m_flLastChargeValue )
	{
		if ( m_pChargeMeter )
		{
			m_pChargeMeter->SetProgress( flCharge );
		}

		if ( !m_bCharged )
		{
			if ( flCharge >= 1.0 )
			{
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( this, "HudMedicCharged" );
				m_bCharged = true;
			}
		}
		else
		{
			// we've got invuln charge or we're using our invuln
			if ( !pMedigun->IsReleasingCharge() )
			{
				g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( this, "HudMedicChargedStop" );
				m_bCharged = false;
			}
		}
	}	

	m_flLastChargeValue = flCharge;
}