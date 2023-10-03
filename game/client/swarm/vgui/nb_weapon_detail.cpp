#include "cbase.h"
#include "nb_weapon_detail.h"
#include "vgui_controls/Label.h"
#include "asw_weapon_parse.h"
#include "asw_equipment_list.h"
#include <vgui/ILocalize.h>
#include "StatsBar.h"
#include "asw_briefing.h"
#include "c_asw_player.h"
#include "ammodef.h"
#include "asw_marine_skills.h"
#include "c_asw_game_resource.h"
#include "c_asw_marine_resource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar asw_skill_reloading_base;
extern ConVar asw_skill_melee_dmg_base;

CNB_Weapon_Detail::CNB_Weapon_Detail( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// == MANAGED_MEMBER_CREATION_START: Do not edit by hand ==
	m_pTitleLabel = new vgui::Label( this, "TitleLabel", "" );
	m_pValueLabel = new vgui::Label( this, "ValueLabel", "" );
	m_pStatsBar = new StatsBar( this, "StatsBar" );
	// == MANAGED_MEMBER_CREATION_END ==

	m_bHidden = false;
	m_pStatsBar->SetShowMaxOnCounter( true );
	m_pStatsBar->SetColors( Color( 255, 255, 255, 0 ), Color( 93,148,192,255 ), Color( 255, 255, 255, 255 ), Color( 17,37,57,255 ), Color( 35, 77, 111, 255 ) );
	m_pStatsBar->m_flBorder = 1.5f;
	m_pStatsBar->AddMinMax( 0, 1.0f );
	m_nEquipIndex = -1;
	m_nProfileIndex = -1;
}

CNB_Weapon_Detail::~CNB_Weapon_Detail()
{

}

void CNB_Weapon_Detail::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	
	LoadControlSettings( "resource/ui/nb_weapon_detail.res" );
}

void CNB_Weapon_Detail::PerformLayout()
{
	BaseClass::PerformLayout();
}

void CNB_Weapon_Detail::OnThink()
{
	BaseClass::OnThink();

	if ( m_bHidden )
	{
		m_pTitleLabel->SetVisible( false );
		m_pValueLabel->SetVisible( false );
		m_pStatsBar->SetVisible( false );
		return;
	}
	
	if ( m_nEquipIndex != -1 && ASWEquipmentList() )
	{
		CASW_EquipItem *pItem = ASWEquipmentList()->GetItemForSlot( m_nInventorySlot, m_nEquipIndex );
		if ( pItem )
		{
			CASW_WeaponInfo* pWeaponInfo = ASWEquipmentList()->GetWeaponDataFor( STRING( pItem->m_EquipClass ) );
			if ( pWeaponInfo )
			{
				UpdateLabels( pWeaponInfo );
			}
		}
	}
}

void CNB_Weapon_Detail::SetWeaponDetails( int nEquipIndex, int nInventorySlot, int nProfileIndex, int nDetailIndex )
{
	m_nEquipIndex = nEquipIndex;
	m_nInventorySlot = nInventorySlot;
	m_nProfileIndex = nProfileIndex;
	m_nDetailIndex = nDetailIndex;
}

void CNB_Weapon_Detail::UpdateLabels( CASW_WeaponInfo *pWeaponData )
{
	C_ASW_Player *pPlayer = C_ASW_Player::GetLocalASWPlayer();
	if ( !pPlayer )
		return;

	CASW_Marine_Profile *pProfile = Briefing()->GetMarineProfileByProfileIndex( m_nProfileIndex );

	switch( m_nDetailIndex )
	{
		case 0:
		{
			// for the chainsaw (which uses melee damage as a skill modifier), it applies a base value to the specified number in the script file
			// even if the skill is at 0 - because of this, we have to make a special case for the chainsaw and shift those values from the 
			// "bonus" side to the "base" side because the base bonus always exists - confused yet?
			float flBaseSkillDmgShift = 0;
			int nBonusDmg = 0;
			int nPellets = pWeaponData->m_iNumPellets;
			if ( pProfile )
			{
				if ( FStrEq("asw_weapon_prifle", pWeaponData->szClassName) )
					nBonusDmg = MarineSkills()->GetSkillBasedValue(pProfile, ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_PRIFLE_DMG);
				else if ( FStrEq("asw_weapon_shotgun", pWeaponData->szClassName) )
					nBonusDmg = MarineSkills()->GetSkillBasedValue(pProfile, ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_SHOTGUN_DMG);
				else if ( FStrEq("asw_weapon_railgun", pWeaponData->szClassName) )
					nBonusDmg = MarineSkills()->GetSkillBasedValue(pProfile, ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_RAILGUN_DMG);
				else if ( FStrEq("asw_weapon_flamer", pWeaponData->szClassName) || FStrEq("asw_weapon_sentry_flamer", pWeaponData->szClassName) )
					nBonusDmg = MarineSkills()->GetSkillBasedValue(pProfile, ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_FLAMER_DMG);
				else if ( FStrEq("asw_weapon_pistol", pWeaponData->szClassName) )
					nBonusDmg = MarineSkills()->GetSkillBasedValue(pProfile, ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_PISTOL_DMG);
				else if ( FStrEq("asw_weapon_pdw", pWeaponData->szClassName) )
					nBonusDmg = MarineSkills()->GetSkillBasedValue(pProfile, ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_PDW_DMG);
				else if ( FStrEq("asw_weapon_sniper_rifle", pWeaponData->szClassName) )
					nBonusDmg = MarineSkills()->GetSkillBasedValue(pProfile, ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_SNIPER_RIFLE_DMG);
				else if ( FStrEq("asw_weapon_tesla_gun", pWeaponData->szClassName) )
					nBonusDmg = 0.5f + MarineSkills()->GetSkillBasedValue(pProfile, ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_TESLA_CANNON_DMG);
				else if ( FStrEq("asw_weapon_chainsaw", pWeaponData->szClassName) )
				{
					flBaseSkillDmgShift = asw_skill_melee_dmg_base.GetFloat();
					nBonusDmg = MarineSkills()->GetSkillBasedValue(pProfile, ASW_MARINE_SKILL_MELEE, ASW_MARINE_SUBSKILL_MELEE_DMG);
				}
				else if ( FStrEq("asw_weapon_vindicator", pWeaponData->szClassName) )
				{
					nBonusDmg = MarineSkills()->GetSkillBasedValue(pProfile, ASW_MARINE_SKILL_VINDICATOR, ASW_MARINE_SUBSKILL_VINDICATOR_DAMAGE);
					nPellets = MarineSkills()->GetSkillBasedValue(pProfile, ASW_MARINE_SKILL_VINDICATOR, ASW_MARINE_SUBSKILL_VINDICATOR_PELLETS);
				}
				else if ( FStrEq("asw_weapon_autogun", pWeaponData->szClassName) || FStrEq("asw_weapon_minigun", pWeaponData->szClassName) )
					nBonusDmg = MarineSkills()->GetSkillBasedValue(pProfile, ASW_MARINE_SKILL_AUTOGUN, ASW_MARINE_SUBSKILL_AUTOGUN_DMG);
				else if ( FStrEq("asw_weapon_grenade_launcher", pWeaponData->szClassName) || FStrEq("asw_weapon_sentry_cannon", pWeaponData->szClassName) )
					nBonusDmg = MarineSkills()->GetSkillBasedValue(pProfile, ASW_MARINE_SKILL_GRENADES, ASW_MARINE_SUBSKILL_GRENADE_CLUSTER_DMG);
				else
					nBonusDmg = MarineSkills()->GetSkillBasedValue(pProfile, ASW_MARINE_SKILL_ACCURACY, ASW_MARINE_SUBSKILL_ACCURACY_RIFLE_DMG);

			}

			// fire power
			static wchar_t wszPowerLine[32];
			int iDamValue = (pWeaponData->m_flBaseDamage*pWeaponData->m_iNumPellets)+flBaseSkillDmgShift;
			int nTotalBonusDmg = (nBonusDmg*nPellets)-flBaseSkillDmgShift;
			Q_snwprintf( wszPowerLine, sizeof( wszPowerLine ), g_pVGuiLocalize->Find( "#asw_weapon_details_firepower" ) );
			wchar_t wzDamValue[10];
			if ( iDamValue <= 0 )
			{
				Q_snwprintf( wzDamValue, sizeof( wzDamValue ), g_pVGuiLocalize->Find( "#asw_weapon_altfire_NA" ) );
			}
			else if ( nTotalBonusDmg > 0 )
			{
				Q_snwprintf( wzDamValue, sizeof( wzDamValue ), L"%d (+%d)", iDamValue, nTotalBonusDmg );
			}
			else
			{
				Q_snwprintf( wzDamValue, sizeof( wzDamValue ), L"%d", iDamValue );
			}

			m_pTitleLabel->SetText( wszPowerLine );
			m_pTitleLabel->SetVisible( true );
			m_pValueLabel->SetText( wzDamValue );
			m_pValueLabel->SetVisible( true );
			
			float flDamage = iDamValue + nTotalBonusDmg;
			float flCurrent = 0.0f;
			if ( flDamage > 60 )
				flCurrent = 1.0f;
			else if ( iDamValue > 0 )
				flCurrent = MAX( (flDamage-5.0f) / 55.0f, 0.05f );

			if ( flDamage <= 0.0f )
			{
				m_pValueLabel->SetText( L"N/A" );
				m_pStatsBar->SetVisible( false );
			}
			else
			{
				m_pStatsBar->Init( flCurrent, flCurrent, 0.1f, false, false );
				m_pStatsBar->SetVisible( true );
			}

			break;
		}

		case 1:
		{
			// fire rate
			static wchar_t wszRateLine[32];
			Q_snwprintf( wszRateLine, sizeof( wszRateLine ), g_pVGuiLocalize->Find( "#asw_weapon_details_firerate" ) );
			m_pTitleLabel->SetText( wszRateLine );
			m_pTitleLabel->SetVisible( true );

			float flRate = pWeaponData->m_flFireRate;
			wchar_t wzFireValue[32];

			float flCurrent = 0.0f;
			if ( flRate <= 0 )
			{
				Q_snwprintf( wzFireValue, sizeof( wzFireValue ), g_pVGuiLocalize->Find( "#asw_weapon_altfire_NA" ) );
			}
			else
			{
				Q_snwprintf( wzFireValue, sizeof( wzFireValue ), L"%.1f / %s", (1.0f/flRate), g_pVGuiLocalize->Find( "#asw_weapon_details_seconds" ) );
				if ( flRate <= 0.125f )
					flCurrent = 1.0f - MIN( ((pWeaponData->m_flFireRate-0.03f)/0.125f)*0.5f, 0.48f );
				else
					flCurrent = 1.0f - MIN( ((pWeaponData->m_flFireRate-0.5f)/0.65f)+0.5f, 0.98f );
			}

			m_pValueLabel->SetText( wzFireValue );
			m_pValueLabel->SetVisible( true );
			m_pStatsBar->Init( flCurrent, flCurrent, 0.1f, false, false );
			m_pStatsBar->SetVisible( true );

			break;
		}

		case 2:
		{
			// the skill modifies the base reload time by this amount so all of the numbers in the script files are incorrect for displaying base amount
			float flBaseSkillModifier = asw_skill_reloading_base.GetFloat();
			// reload time
			static wchar_t wszReloadLine[32];
			Q_snwprintf( wszReloadLine, sizeof( wszReloadLine ), g_pVGuiLocalize->Find( "#asw_weapon_details_reload" ) );
			float flBaseReload = pWeaponData->flReloadTime;
			if ( pWeaponData->m_flDisplayReloadTime >= 0 )
				flBaseReload = pWeaponData->m_flDisplayReloadTime;

			float flCurrent = flBaseReload;
			float flRealReload = flBaseReload;
			wchar_t wzReloadValue[32];
			if ( flBaseReload <= 0 )
			{
				Q_snwprintf( wzReloadValue, sizeof( wzReloadValue ), g_pVGuiLocalize->Find( "#asw_weapon_altfire_NA" ) );
			}
			else
			{
				float fSpeedScale = 1;
				float flReloadDiff = 0;
				if ( pProfile )
				{
					fSpeedScale = MarineSkills()->GetSkillBasedValue(pProfile, ASW_MARINE_SKILL_RELOADING, ASW_MARINE_SUBSKILL_RELOADING_SPEED_SCALE);
					if ( fSpeedScale != 1 )
					{
						flRealReload = flBaseReload * fSpeedScale;
					
						// to get an accurate difference, we need to multiply the script defined base reload amount by the base modifier that the Skill does
						flReloadDiff = flRealReload - (flBaseReload * flBaseSkillModifier);
					}
				}

				if ( flReloadDiff != 0 )
					Q_snwprintf( wzReloadValue, sizeof( wzReloadValue ), L"%.1f %s (%s%.1f)", (flBaseReload*flBaseSkillModifier), g_pVGuiLocalize->Find( "#asw_weapon_details_seconds" ), (flReloadDiff>0)?L"+":L"", flReloadDiff );
				else
					Q_snwprintf( wzReloadValue, sizeof( wzReloadValue ), L"%.1f %s", (flBaseReload*flBaseSkillModifier), g_pVGuiLocalize->Find( "#asw_weapon_details_seconds" ) );
				flCurrent = 1.0f - MIN( (flRealReload-0.75f)/2.5f, 0.99f );
			}

			m_pTitleLabel->SetText( wszReloadLine );
			m_pTitleLabel->SetVisible( true );
			m_pValueLabel->SetText( wzReloadValue );
			m_pValueLabel->SetVisible( true );
			m_pStatsBar->Init( flCurrent, flCurrent, 0.1f, false, false );
			m_pStatsBar->SetVisible( true );
			break;
		}

		case 3:
		{
			// clip capacity
			static wchar_t wszClipLine[32];
			Q_snwprintf( wszClipLine, sizeof( wszClipLine ), g_pVGuiLocalize->Find( "#asw_weapon_details_clipsize" ) );
			int iClipValue = pWeaponData->iMaxClip1;
			int nMaxBulletsPerGun = GetAmmoDef()->MaxCarry( pWeaponData->iAmmoType, NULL );
			int iNumClips = ( nMaxBulletsPerGun / iClipValue ) + 1;
			if ( pWeaponData->m_bShowClipsDoubled )
			{
				iNumClips *= 2;
			}

			if ( pWeaponData->m_iDisplayClipSize >= 0 )
				iClipValue = pWeaponData->m_iDisplayClipSize;

			wchar_t wzClipValue[10];
			if ( iClipValue == 111 ) // magic number for infinite ammo
			{
				// this displays an "infinity" symbol in the neosans font
				wzClipValue[0] = 0x221E;
				wzClipValue[1] = L'\0';
			}
			else if ( pWeaponData->m_bShowClipsInWeaponDetail )
			{
				Q_snwprintf( wzClipValue, sizeof( wzClipValue ), L"%d x %d", iClipValue, iNumClips );
			}
			else
			{
				Q_snwprintf( wzClipValue, sizeof( wzClipValue ), L"%d", iClipValue );
			}
			float flCurrent = MIN( (float)iClipValue/200.0f, 1.0f );

			m_pTitleLabel->SetText( wszClipLine );
			m_pTitleLabel->SetVisible( true );
			m_pValueLabel->SetText( wzClipValue );
			m_pValueLabel->SetVisible( true );
			m_pStatsBar->Init( flCurrent, flCurrent, 0.1f, false, false );
			m_pStatsBar->SetVisible( true );
			break;
		}

		case 4:
		{
			// alt fire
			static wchar_t wszAltLine[32];
			Q_snwprintf( wszAltLine, sizeof( wszAltLine ), g_pVGuiLocalize->Find( "#asw_weapon_details_altfire" ) );
			wchar_t wzAltValue[64];
			bool bHighlightText = false;
			if ( !Q_stricmp( pWeaponData->szAltFireText, "" ) )
				Q_snwprintf( wzAltValue, sizeof( wzAltValue ), g_pVGuiLocalize->Find( "#asw_weapon_altfire_none" ) );
			else
			{
				int iAltFire = pWeaponData->iDefaultClip2;
				if ( iAltFire > 0 )
					Q_snwprintf( wzAltValue, sizeof( wzAltValue ), L"%d %s", iAltFire, g_pVGuiLocalize->Find( pWeaponData->szAltFireText ) );
				else
					Q_snwprintf( wzAltValue, sizeof( wzAltValue ), g_pVGuiLocalize->Find( pWeaponData->szAltFireText ) );
				bHighlightText = true;
			}

			m_pTitleLabel->SetText( wszAltLine );
			m_pTitleLabel->SetVisible( true );
			m_pValueLabel->SetText( wzAltValue );
			m_pValueLabel->SetVisible( true );
			m_pStatsBar->SetVisible( false );
			break;
		}
		
		case 5:
		{
			// attributes
			static wchar_t wszAttributesLine[32];
			Q_snwprintf( wszAttributesLine, sizeof( wszAttributesLine ), g_pVGuiLocalize->Find( "#asw_weapon_details_notes" ) );
			//wchar_t wszAttributesTempNull[4];
			//wchar_t wszAttributesTempText[64];
			wchar_t wszAttributesValue[64];
			bool bHighlightText = false;
			//Q_snwprintf( wszAttributesTempText, sizeof( wszAttributesTempText ), g_pVGuiLocalize->Find( pWeaponData->szAttributesText ) );
			//Q_snwprintf( wszAttributesTempNull, sizeof( wszAttributesTempNull ), "" );
			if ( !Q_wcscmp( g_pVGuiLocalize->Find( pWeaponData->szAttributesText ), g_pVGuiLocalize->Find( "#asw_weapon_null_line" ) ) )
				//asw_weapon_null_line
			{
			//	Q_snwprintf( wszAttributesValue, sizeof( wszAttributesValue ), g_pVGuiLocalize->Find( "#asw_weapon_altfire_none" ) );
				m_pTitleLabel->SetVisible( false );
				m_pValueLabel->SetVisible( false );
			}
			else
			{
				Q_snwprintf( wszAttributesValue, sizeof( wszAttributesValue ), g_pVGuiLocalize->Find( pWeaponData->szAttributesText ) );
				bHighlightText = true;
				m_pTitleLabel->SetText( wszAttributesLine );
				m_pTitleLabel->SetVisible( true );
				m_pValueLabel->SetText( wszAttributesValue );
				m_pValueLabel->SetVisible( true );
			}

			m_pStatsBar->SetVisible( false );

			break;
		}

		case 6:
		{
			int nRequiredLevel = pPlayer->GetWeaponLevelRequirement( pWeaponData->szClassName );
			if ( nRequiredLevel == -1 )
			{
				m_pTitleLabel->SetVisible( false );
				m_pValueLabel->SetVisible( false );
				m_pStatsBar->SetVisible( false );
				return;
			}

			// required level
			static wchar_t wszAltLine[32];
			Q_snwprintf( wszAltLine, sizeof( wszAltLine ), g_pVGuiLocalize->Find( "#asw_weapon_details_required_level" ) );

			nRequiredLevel++;	// for display it's actually 1 higher (levels start from 0 in code)
			wchar_t wzAltValue[64];
			Q_snwprintf( wzAltValue, sizeof( wzAltValue ), L"%d", nRequiredLevel );

			m_pTitleLabel->SetText( wszAltLine );
			m_pTitleLabel->SetVisible( true );
			m_pValueLabel->SetText( wzAltValue );
			m_pValueLabel->SetVisible( true );
			m_pStatsBar->SetVisible( false );

			break;
		}
	}	
}