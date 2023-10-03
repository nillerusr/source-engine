#include "cbase.h"
#include "asw_hud_powerups.h"
#include "c_asw_player.h"
#include "c_asw_marine.h"
#include "c_asw_weapon.h"
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include <vgui/ISurface.h>
#include "cdll_bounded_cvars.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CASW_Hud_Powerups::CASW_Hud_Powerups( vgui::Panel *pParent, const char *pElementName ) 
:	vgui::Panel( pParent, pElementName )
{	
	m_pIconImage = new vgui::ImagePanel( this, "PowerupIconImage" );
	m_pIconImage->SetShouldScaleImage( true );
	m_pIconImage->SetMouseInputEnabled( false );

	m_pStringLabel = new vgui::Label( this, "PowerupStringLabel", "" );
	m_pStringLabel->SetMouseInputEnabled( false );
}

CASW_Hud_Powerups::~CASW_Hud_Powerups()
{
	
}

void CASW_Hud_Powerups::PerformLayout()
{

	int w, t;
	GetSize(w, t);
	m_pIconImage->SetBounds(0, 0, t, t);

	m_pStringLabel->SetPos( t+2, 0 );
	m_pStringLabel->SizeToContents();
	m_pStringLabel->InvalidateLayout(true);

	/*
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

	SetTall( m_pStringLabel->GetTall() );
	*/
}

void CASW_Hud_Powerups::UpdatePowerupIcon( C_ASW_Marine *pMarine )
{
	m_pIconImage->SetVisible( false );
	m_pStringLabel->SetVisible( false );

	if ( !pMarine )
		return;

	if ( pMarine->HasAnyPowerups() )
	{
		// if it doesn't expire, then its on the weapon
		// check if the weapon that has it is currently wielded
		//if ( !pMarine->m_bPowerupExpires )
		//{
		//	CASW_Weapon* pWeapon = pMarine->GetActiveASWWeapon();
		//	if ( !pWeapon || !pWeapon->m_bPoweredUp )
		//		return;
		//}

		const char *m_szIconName;
		switch ( pMarine->m_iPowerupType )
		{
		case POWERUP_TYPE_FREEZE_BULLETS:
			m_szIconName = "hud/PowerupIcons/powerup_freeze_bullets"; break;
		case POWERUP_TYPE_FIRE_BULLETS:
			m_szIconName = "hud/PowerupIcons/powerup_fire_bullets"; break;
		case POWERUP_TYPE_ELECTRIC_BULLETS:
			m_szIconName = "hud/PowerupIcons/powerup_electric_bullets"; break;
		//case POWERUP_TYPE_CHEMICAL_BULLETS:
			//	m_szPowerupClassname = "hud/PowerupIcons/powerup_chemical_bullets"; break;
		//case POWERUP_TYPE_EXPLOSIVE_BULLETS:
			//m_szIconName = "hud/PowerupIcons/powerup_explosive_bullets"; break;
		case POWERUP_TYPE_INCREASED_SPEED:
			m_szIconName = "hud/PowerupIcons/powerup_increased_speed"; break;
		default:
			return; break;
		}

		m_pIconImage->SetImage( m_szIconName );
		m_pIconImage->SetDrawColor( Color( 255, 255, 255, 255 ) );
		m_pIconImage->SetVisible( true );

		if ( pMarine->m_bPowerupExpires )
		{
			m_pStringLabel->SetText( VarArgs( "%.0f", (pMarine->m_flPowerupExpireTime - gpGlobals->curtime) ) );
			m_pStringLabel->SetVisible( true );
		}

		InvalidateLayout( true );
	}
}

void CASW_Hud_Powerups::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetBgColor( Color( 255, 255, 255, 200 ) );
	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled( false );

	m_pStringLabel->SetFgColor(Color(200,200,200,255));
	m_pStringLabel->SetFont( pScheme->GetFont( "DefaultSmall", IsProportional() ) );//VerdanaSmall
	m_pStringLabel->SetContentAlignment( vgui::Label::a_west );

	SetMouseInputEnabled(false);
}

void CASW_Hud_Powerups::OnThink()
{
	//SetAlpha( 0 );

	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if (!pPlayer)
		return;

	C_ASW_Marine *pMarine = pPlayer->GetMarine();
	if ( !pMarine )
		return;

	UpdatePowerupIcon( pMarine );	
}