#include "cbase.h"
#include "c_asw_sentry_base.h"
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/Panel.h>
#include "asw_util_shared.h"
#include "c_asw_marine.h"
#include "c_asw_player.h"
#include "c_asw_pickup_weapon.h"
#include "asw_equipment_list.h"
#include "asw_weapon_parse.h"
#include "asw_hud_use_icon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_ASW_Sentry_Base, DT_ASW_Sentry_Base, CASW_Sentry_Base)
	RecvPropBool(RECVINFO(m_bAssembled)),
	RecvPropBool(RECVINFO(m_bIsInUse)),
	RecvPropFloat(RECVINFO(m_fAssembleProgress)),
	RecvPropFloat(RECVINFO(m_fAssembleCompleteTime)),
	RecvPropInt(RECVINFO(m_iAmmo)),	
	RecvPropInt(RECVINFO(m_iMaxAmmo)),	
	RecvPropBool(RECVINFO(m_bSkillMarineHelping)),
	RecvPropInt(RECVINFO(m_nGunType)),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_ASW_Sentry_Base )

END_PREDICTION_DATA()

CUtlVector<C_ASW_Sentry_Base*>	g_SentryGuns;

vgui::HFont C_ASW_Sentry_Base::s_hAmmoFont = vgui::INVALID_FONT;

C_ASW_Sentry_Base::C_ASW_Sentry_Base()
{
	m_bAssembled = false;
	m_bIsInUse = false;
	m_fAssembleProgress = 0;
	m_bSkillMarineHelping = false;
	g_SentryGuns.AddToTail( this );
	m_nUseIconTextureID = -1;
}


C_ASW_Sentry_Base::~C_ASW_Sentry_Base()
{
	g_SentryGuns.FindAndRemove( this );
}

bool C_ASW_Sentry_Base::ShouldDraw()
{
	return true;
}

int C_ASW_Sentry_Base::DrawModel( int flags, const RenderableInstance_t &instance )
{
	int d = BaseClass::DrawModel(flags, instance);

	return d;
}

int C_ASW_Sentry_Base::GetSentryIconTextureID()
{
	int nIndex = g_WeaponUseIcons.Find( GetWeaponClass() );
	if ( nIndex == g_WeaponUseIcons.InvalidIndex() && ASWEquipmentList() )
	{
		CASW_WeaponInfo *pInfo = ASWEquipmentList()->GetWeaponDataFor( GetWeaponClass() );
		if ( pInfo )
		{
			m_nUseIconTextureID = vgui::surface()->CreateNewTextureID();
			char buffer[ 256 ];
			Q_snprintf( buffer, sizeof( buffer ), "vgui/%s", pInfo->szEquipIcon );
			vgui::surface()->DrawSetTextureFile( m_nUseIconTextureID, buffer, true, false);

			nIndex = g_WeaponUseIcons.Insert( GetWeaponClass(), m_nUseIconTextureID );
		}
	}
	else
	{
		m_nUseIconTextureID = g_WeaponUseIcons.Element( nIndex );
	}

	return m_nUseIconTextureID;
}

bool C_ASW_Sentry_Base::WantsDismantle( void ) const
{
	if ( IsInUse() || !IsAssembled() || GetAmmo() <= 0 || gpGlobals->curtime < m_fAssembleCompleteTime + 5.0f )
		return false;

	return true;
}

bool C_ASW_Sentry_Base::IsUsable(C_BaseEntity *pUser)
{
	return (pUser && pUser->GetAbsOrigin().DistTo(GetAbsOrigin()) < ASW_MARINE_USE_RADIUS);	// near enough?
}

bool C_ASW_Sentry_Base::GetUseAction(ASWUseAction &action, C_ASW_Marine *pUser)
{
	if ( IsAssembled() )
	{
		C_ASW_Player *pPlayer = pUser->GetCommander();
		if ( pPlayer && pUser->IsInhabited() && pPlayer->m_flUseKeyDownTime != 0.0f && ( gpGlobals->curtime - pPlayer->m_flUseKeyDownTime ) > 0.2f )		// if player has started holding down the USE key
		{
			TryLocalize( "#asw_disassembling_sentry", action.wszText, sizeof( action.wszText ) );
			action.fProgress = ( ( gpGlobals->curtime - pPlayer->m_flUseKeyDownTime ) - 0.2f ) / ( ASW_USE_KEY_HOLD_SENTRY_TIME - 0.2f );
			action.fProgress = clamp<float>( action.fProgress, 0.0f, 1.0f );
			action.bShowHoldButtonUseKey = false;
			action.bShowUseKey = false;
			action.bNoFadeIfSameUseTarget = true;
		}
		else
		{
			TryLocalize( "#asw_turn_sentry", action.wszText, sizeof( action.wszText ) );
			action.fProgress = -1;
			TryLocalize( "#asw_disassemble_sentry", action.wszHoldButtonText, sizeof( action.wszHoldButtonText ) );
			action.bShowHoldButtonUseKey = true;
			action.bShowUseKey = true;
		}
		action.iUseIconTexture = GetSentryIconTextureID();		
		action.UseTarget = this;
	}
	else
	{
		if ( m_bIsInUse )
		{
			TryLocalize( "#asw_assembling_sentry", action.wszText, sizeof( action.wszText ) );
			action.bShowUseKey = false;
			action.bNoFadeIfSameUseTarget = true;
		}
		else
		{
			TryLocalize( "#asw_assemble_sentry", action.wszText, sizeof( action.wszText ) );
			action.bShowUseKey = true;
		}
		action.iUseIconTexture = GetSentryIconTextureID();		
		action.UseTarget = this;
		action.fProgress = GetAssembleProgress();
	}
	action.UseIconRed = 66;
	action.UseIconGreen = 142;
	action.UseIconBlue = 192;
	action.iInventorySlot = -1;
	action.bWideIcon = true;
	return true;
}

void C_ASW_Sentry_Base::CustomPaint( int ix, int iy, int alpha, vgui::Panel *pUseIcon )
{	
	if ( !m_bAssembled )
		return;

	if (s_hAmmoFont == vgui::INVALID_FONT)
	{
		vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile("resource/SwarmSchemeNew.res", "SwarmSchemeNew");
		vgui::IScheme *pScheme = vgui::scheme()->GetIScheme(scheme);
		if (pScheme)
			s_hAmmoFont = vgui::scheme()->GetIScheme(scheme)->GetFont("DefaultSmall", true);
	}

	if (s_hAmmoFont == vgui::INVALID_FONT || alpha <= 0)
		return;
	
	int nAmmo = MAX( 0, GetAmmo() );
	Color textColor( 255, 255, 255, 255 );
	if ( nAmmo < 50 )
	{
		textColor = Color( 255, 255, 0, 255 );
	}

	if ( pUseIcon )
	{
		CASW_HUD_Use_Icon *pUseIconPanel = static_cast<CASW_HUD_Use_Icon*>(pUseIcon);
		float flProgress = (float) nAmmo / (float) GetMaxAmmo();
		char szCountText[64];
		Q_snprintf( szCountText, sizeof( szCountText ), "%d", MAX( nAmmo, 0 ) );
		pUseIconPanel->CustomPaintProgressBar( ix, iy, alpha / 255.0f, flProgress, szCountText, s_hAmmoFont, textColor, "#asw_ammo_label" );
	}
}

const char* C_ASW_Sentry_Base::GetWeaponClass()
{
	switch( m_nGunType.Get() )
	{
		case 0: return "asw_weapon_sentry";
		case 1: return "asw_weapon_sentry_cannon";
		case 2: return "asw_weapon_sentry_flamer";
		case 3: return "asw_weapon_sentry_freeze";
	}
	return "asw_weapon_sentry";
}