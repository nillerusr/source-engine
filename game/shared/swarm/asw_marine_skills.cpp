#include "cbase.h"

#ifdef CLIENT_DLL
	#include "c_asw_marine.h"
	#include "c_asw_marine_resource.h"
	#include "c_asw_game_resource.h"
	#define CASW_Marine C_ASW_Marine
	#define CASW_Marine_Resource C_ASW_Marine_Resource
	#define CASW_Game_Resource C_ASW_Game_Resource
#else
	#include "asw_marine.h"
	#include "asw_marine_resource.h"
	#include "asw_game_resource.h"
#endif
#include "asw_marine_skills.h"
#include "asw_marine_profile.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// base convars
ConVar asw_skill_leadership_accuracy_chance_base("asw_skill_leadership_accuracy_chance_base", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_leadership_damage_resist_base("asw_skill_leadership_damage_resist_base", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_vindicator_dmg_base("asw_skill_vindicator_dmg_base", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_vindicator_pellets_base("asw_skill_vindicator_pellets_base", "7", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar asw_skill_autogun_base("asw_skill_autogun_base", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_piercing_base("asw_skill_piercing_base", "0", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar asw_skill_healing_charges_base("asw_skill_healing_charges_base", "4", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_self_healing_charges_base("asw_skill_self_healing_charges_base", "2", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_healing_medkit_hps_base("asw_skill_healing_medkit_hps_base", "50", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_healing_hps_base("asw_skill_healing_hps_base", "25", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_healing_grenade_base("asw_skill_healing_grenade_base", "50", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_healing_gun_charges_base("asw_skill_healing_gun_charges_base", "40", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_healing_gun_base("asw_skill_healing_gun_base", "5", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_xenowounds_base("asw_skill_xenowounds_base", "100", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_drugs_base("asw_skill_drugs_base", "5", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar asw_skill_hacking_speed_base("asw_skill_hacking_speed_base", "2.0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_scanner_base("asw_skill_scanner_base", "600", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_engineering_welding_base("asw_skill_engineering_welding_base", "0.8", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_engineering_sentry_base("asw_skill_engineering_sentry_base", "1.0", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar asw_skill_grenades_radius_base("asw_skill_grenades_radius_base", "280", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_dmg_base("asw_skill_grenades_dmg_base", "80", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_incendiary_dmg_base("asw_skill_grenades_incendiary_dmg_base", "80", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_cluster_dmg_base("asw_skill_grenades_cluster_dmg_base", "80", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_clusters_base("asw_skill_grenades_clusters_base", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_flechette_dmg_base("asw_skill_grenades_flechette_dmg_base", "10", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_hornet_dmg_base("asw_skill_grenades_hornet_dmg_base", "50", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_hornet_count_base("asw_skill_grenades_hornet_count_base", "8", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_hornet_interval_base("asw_skill_grenades_hornet_interval_base", "0.09", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_freeze_radius_base("asw_skill_grenades_freeze_radius_base", "210", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_freeze_duration_base("asw_skill_grenades_freeze_duration_base", "3.0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_smart_count_base("asw_skill_grenades_smart_count_base", "32", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_smart_interval_base("asw_skill_grenades_smart_interval_base", "0.09", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar asw_skill_health_base("asw_skill_health_base", "80", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_melee_dmg_base("asw_skill_melee_dmg_base", "30", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_melee_force_base("asw_skill_melee_force_base", "10", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_melee_speed_base("asw_skill_melee_speed_base", "1.0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_reloading_base("asw_skill_reloading_base", "1.4", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_reloading_fast_base( "asw_skill_reloading_fast_base", "1.0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_agility_movespeed_base("asw_skill_agility_movespeed_base", "290", FCVAR_REPLICATED | FCVAR_CHEAT );

// step convars
ConVar asw_skill_leadership_accuracy_chance_step("asw_skill_leadership_accuracy_chance_step", "0.03", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_leadership_damage_resist_step("asw_skill_leadership_damage_resist_step", "0.06", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_vindicator_dmg_step("asw_skill_vindicator_dmg_step", "2.0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_vindicator_pellets_step("asw_skill_vindicator_pellets_step", "0", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar asw_skill_autogun_step("asw_skill_autogun_step", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_piercing_step("asw_skill_piercing_step", "0.20", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar asw_skill_healing_charges_step("asw_skill_healing_charges_step", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_self_healing_charges_step("asw_skill_self_healing_charges_step", "0.5", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_healing_hps_step("asw_skill_healing_hps_step", "8", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_healing_grenade_step("asw_skill_healing_grenade_step", "10", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_healing_gun_charges_step("asw_skill_healing_gun_charges_step", "10", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_healing_gun_step("asw_skill_healing_gun_step", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_healing_medkit_hps_step("asw_skill_healing_medkit_hps_step", "5", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_xenowounds_step("asw_skill_xenowounds_step", "-25", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_drugs_step("asw_skill_drugs_step", "0.8", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar asw_skill_hacking_speed_step("asw_skill_hacking_speed_step", "0.1", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_scanner_step("asw_skill_scanner_step", "150", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_engineering_welding_step("asw_skill_engineering_welding_step", "0.5", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_engineering_sentry_step("asw_skill_engineering_sentry_step", "0.25", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar asw_skill_grenades_radius_step("asw_skill_grenades_radius_step", "20", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_dmg_step("asw_skill_grenades_dmg_step", "10", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_incendiary_dmg_step("asw_skill_grenades_incendiary_dmg_step", "10", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_cluster_dmg_step("asw_skill_grenades_cluster_dmg_step", "10", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_clusters_step("asw_skill_grenades_clusters_step", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_flechette_dmg_step("asw_skill_grenades_flechette_dmg_step", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_hornet_dmg_step("asw_skill_grenades_hornet_dmg_step", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_hornet_count_step("asw_skill_grenades_hornet_count_step", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_hornet_interval_step("asw_skill_grenades_hornet_interval_step", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_freeze_radius_step("asw_skill_grenades_freeze_radius_step", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_freeze_duration_step("asw_skill_grenades_freeze_duration_step", "0.3", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_smart_count_step("asw_skill_grenades_smart_count_step", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_grenades_smart_interval_step("asw_skill_grenades_smart_interval_step", "0", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar asw_skill_health_step("asw_skill_health_step", "15", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_melee_dmg_step("asw_skill_melee_dmg_step", "6", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_melee_force_step("asw_skill_melee_force_step", "1.0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_melee_speed_step("asw_skill_melee_speed_step", "0.1", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_reloading_step("asw_skill_reloading_step", "-0.14", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_reloading_fast_step( "asw_skill_reloading_fast_step", "0.05", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_agility_movespeed_step("asw_skill_agility_movespeed_step", "10", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_agility_reload_step("asw_skill_agility_reload_step", "0", FCVAR_REPLICATED | FCVAR_CHEAT );

ConVar asw_skill_mines_fires_base("asw_skill_mines_fires_base", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_mines_fires_step("asw_skill_mines_fires_step", "0.5", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_mines_duration_base("asw_skill_mines_duration_base", "10.0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_mines_duration_step("asw_skill_mines_duration_step", "5.0", FCVAR_REPLICATED | FCVAR_CHEAT );

// accuracy convars
ConVar asw_skill_accuracy_rifle_dmg_base("asw_skill_accuracy_rifle_dmg_base", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_rifle_dmg_step("asw_skill_accuracy_rifle_dmg_step", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_prifle_dmg_base("asw_skill_accuracy_prifle_dmg_base", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_prifle_dmg_step("asw_skill_accuracy_prifle_dmg_step", "1", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_shotgun_dmg_base("asw_skill_accuracy_shotgun_dmg_base", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_shotgun_dmg_step("asw_skill_accuracy_shotgun_dmg_step", "2", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_railgun_dmg_base("asw_skill_accuracy_railgun_dmg_base", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_railgun_dmg_step("asw_skill_accuracy_railgun_dmg_step", "10", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_flamer_dmg_base("asw_skill_accuracy_flamer_dmg_base", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_flamer_dmg_step("asw_skill_accuracy_flamer_dmg_step", "0.5", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_pistol_dmg_base("asw_skill_accuracy_pistol_dmg_base", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_pistol_dmg_step("asw_skill_accuracy_pistol_dmg_step", "2", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_pdw_dmg_base("asw_skill_accuracy_pdw_dmg_base", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_pdw_dmg_step("asw_skill_accuracy_pdw_dmg_step", "1.0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_muzzle_flash_base("asw_skill_muzzle_flash_base", "1.0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_muzzle_flash_step("asw_skill_muzzle_flash_step", "0.2", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_sniper_rifle_dmg_base("asw_skill_accuracy_sniper_rifle_dmg_base", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_sniper_rifle_dmg_step("asw_skill_accuracy_sniper_rifle_dmg_step", "10", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_tesla_cannon_dmg_base("asw_skill_accuracy_tesla_cannon_dmg_base", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar asw_skill_accuracy_tesla_cannon_dmg_step("asw_skill_accuracy_tesla_cannon_dmg_step", "0.25", FCVAR_REPLICATED | FCVAR_CHEAT );

namespace
{
	float MuzzleFlashScale(int iSkillPoints)
	{
		return asw_skill_muzzle_flash_base.GetFloat() + (asw_skill_muzzle_flash_step.GetFloat() * (float)iSkillPoints);
	}
};

CASW_Marine_Skills::CASW_Marine_Skills()
{
#ifndef CLIENT_DLL
	m_hLastSkillMarine = NULL;
#endif
}

// accessor functions to get at any game variables that are based on a skill
float CASW_Marine_Skills::GetSkillBasedValueByMarine(CASW_Marine *pMarine, ASW_Skill iSkillIndex, int iSubSkill)
{
	if (!pMarine || !pMarine->GetMarineProfile())
		return 0;
	return GetSkillBasedValue(pMarine->GetMarineProfile(), iSkillIndex, iSubSkill);
}

float CASW_Marine_Skills::GetSkillBasedValueByMarineResource(CASW_Marine_Resource *pMarineResource, ASW_Skill iSkillIndex, int iSubSkill)
{
	if (!pMarineResource)
		return 0;

	return GetSkillBasedValue(pMarineResource->GetProfile(), iSkillIndex, iSubSkill);
}

float CASW_Marine_Skills::GetSkillBasedValue( CASW_Marine_Profile *pProfile, ASW_Skill iSkillIndex, int iSubSkill, int iSkillPoints )
{
	if ( !ASWGameResource() || !MarineProfileList() || !pProfile )
		return 0;

	int iProfileIndex = pProfile->m_ProfileIndex;
	if (iProfileIndex < 0 || iProfileIndex >=MarineProfileList()->m_NumProfiles)
		return 0;

	int nSkillSlot = ASWGameResource()->GetSlotForSkill( iProfileIndex, iSkillIndex );
	if ( nSkillSlot == -1 )
	{
		iSkillPoints = 0;		// assume zero skill points if the marine doesn't have this skill
	}
	else if (iSkillPoints == -1)
	{
		// get the skill points from the ASWGameResource
		iSkillPoints = ASWGameResource()->GetMarineSkill(iProfileIndex, nSkillSlot);
	}

	switch (iSkillIndex)
	{
		case ASW_MARINE_SKILL_LEADERSHIP:
			if ( iSkillPoints <= 0 )
				return 0.0f;
			if (iSubSkill == ASW_MARINE_SUBSKILL_LEADERSHIP_ACCURACY_CHANCE)			
				return asw_skill_leadership_accuracy_chance_base.GetFloat() + asw_skill_leadership_accuracy_chance_step.GetFloat() * iSkillPoints;
			else	// ASW_MARINE_SUBSKILL_LEADERSHIP_DAMAGE_RESIST
				return asw_skill_leadership_damage_resist_base.GetFloat() + asw_skill_leadership_damage_resist_step.GetFloat() * iSkillPoints;
			break;								
		case ASW_MARINE_SKILL_VINDICATOR:
			if (iSubSkill == ASW_MARINE_SUBSKILL_VINDICATOR_DAMAGE)
				return asw_skill_vindicator_dmg_base.GetFloat() + asw_skill_vindicator_dmg_step.GetFloat() * iSkillPoints;
			else if (iSubSkill == ASW_MARINE_SUBSKILL_VINDICATOR_PELLETS)
				return asw_skill_vindicator_pellets_base.GetFloat() + asw_skill_vindicator_pellets_step.GetFloat() * iSkillPoints;
			else // ASW_MARINE_SUBSKILL_VINDICATOR_MUZZLE
				return MuzzleFlashScale(iSkillPoints);
			break;
		case ASW_MARINE_SKILL_AUTOGUN:
			if (iSubSkill == ASW_MARINE_SUBSKILL_AUTOGUN_DMG)
				return asw_skill_autogun_base.GetFloat() + asw_skill_autogun_step.GetFloat() * iSkillPoints;
			else // ASW_MARINE_SUBSKILL_AUTOGUN_MUZZLE
				return MuzzleFlashScale(iSkillPoints);
			break;
		case ASW_MARINE_SKILL_STOPPING_POWER:
			return iSkillPoints;
			break;
		case ASW_MARINE_SKILL_PIERCING:
			return asw_skill_piercing_base.GetFloat() + asw_skill_piercing_step.GetFloat() * iSkillPoints;
			break;
		case ASW_MARINE_SKILL_HEALING:
			switch( iSubSkill )
			{
				case ASW_MARINE_SUBSKILL_HEALING_CHARGES: return asw_skill_healing_charges_base.GetFloat() + asw_skill_healing_charges_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_SELF_HEALING_CHARGES: return asw_skill_self_healing_charges_base.GetFloat() + asw_skill_self_healing_charges_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_HEALING_HPS: return asw_skill_healing_hps_base.GetFloat() + asw_skill_healing_hps_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_HEAL_GRENADE_HEAL_AMOUNT: return asw_skill_healing_grenade_base.GetFloat() + asw_skill_healing_grenade_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_HEAL_GUN_CHARGES: return asw_skill_healing_gun_charges_base.GetFloat() + asw_skill_healing_gun_charges_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_HEAL_GUN_HEAL_AMOUNT: return asw_skill_healing_gun_base.GetFloat() + asw_skill_healing_gun_step.GetFloat() * iSkillPoints; break;					
				default: return asw_skill_healing_medkit_hps_base.GetFloat() + asw_skill_healing_medkit_hps_step.GetFloat() * iSkillPoints; break;
			}
			break;
		case ASW_MARINE_SKILL_XENOWOUNDS:
			return asw_skill_xenowounds_base.GetFloat() + asw_skill_xenowounds_step.GetFloat() * iSkillPoints;
			break;
		case ASW_MARINE_SKILL_DRUGS:
			return asw_skill_drugs_base.GetFloat() + asw_skill_drugs_step.GetFloat() * iSkillPoints;
			break;
		case ASW_MARINE_SKILL_HACKING:
			if (iSubSkill == ASW_MARINE_SUBSKILL_HACKING_TUMBLER_COUNT_REDUCTION)
			{
				if (iSkillPoints >= 3)
					return 1;
				return 0;
			}
			else if (iSubSkill == ASW_MARINE_SUBSKILL_HACKING_ENTRIES_PER_TUMBLER_REDUCTION)
			{
				if (iSkillPoints >= 2)
					return 2;
				else if (iSkillPoints >= 4)
					return 4;
				return 0;
			}				
			//else if (iSubSkill == ASW_MARINE_SUBSKILL_HACKING_SPEED_SCALE)
			return asw_skill_hacking_speed_base.GetFloat() + asw_skill_hacking_speed_step.GetFloat() * iSkillPoints;
			break;
		case ASW_MARINE_SKILL_SCANNER:
			return asw_skill_scanner_base.GetFloat() + asw_skill_scanner_step.GetFloat() * iSkillPoints;
			break;
		case ASW_MARINE_SKILL_ENGINEERING:
			if ( iSkillPoints <= 0 )
				return 0.0f;
			if (iSubSkill == ASW_MARINE_SUBSKILL_ENGINEERING_WELDING)			
				return asw_skill_engineering_welding_base.GetFloat() + asw_skill_engineering_welding_step.GetFloat() * iSkillPoints;
			else
				return asw_skill_engineering_sentry_base.GetFloat() + asw_skill_engineering_sentry_step.GetFloat() * iSkillPoints;
			break;
		case ASW_MARINE_SKILL_ACCURACY:
			switch (iSubSkill)
			{
				case ASW_MARINE_SUBSKILL_ACCURACY_MUZZLE: return MuzzleFlashScale(iSkillPoints); break;
				case ASW_MARINE_SUBSKILL_ACCURACY_PRIFLE_DMG: return asw_skill_accuracy_prifle_dmg_base.GetFloat() + asw_skill_accuracy_prifle_dmg_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_ACCURACY_SHOTGUN_DMG: return asw_skill_accuracy_shotgun_dmg_base.GetFloat() + asw_skill_accuracy_shotgun_dmg_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_ACCURACY_RAILGUN_DMG: return asw_skill_accuracy_railgun_dmg_base.GetFloat() + asw_skill_accuracy_railgun_dmg_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_ACCURACY_FLAMER_DMG: return asw_skill_accuracy_flamer_dmg_base.GetFloat() + asw_skill_accuracy_flamer_dmg_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_ACCURACY_PISTOL_DMG: return asw_skill_accuracy_pistol_dmg_base.GetFloat() + asw_skill_accuracy_pistol_dmg_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_ACCURACY_PDW_DMG: return asw_skill_accuracy_pdw_dmg_base.GetFloat() + asw_skill_accuracy_pdw_dmg_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_ACCURACY_SNIPER_RIFLE_DMG:  return asw_skill_accuracy_sniper_rifle_dmg_base.GetFloat() + asw_skill_accuracy_sniper_rifle_dmg_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_ACCURACY_TESLA_CANNON_DMG:	 return asw_skill_accuracy_tesla_cannon_dmg_base.GetFloat() + asw_skill_accuracy_tesla_cannon_dmg_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_ACCURACY_RIFLE_DMG:
				default: return asw_skill_accuracy_rifle_dmg_base.GetFloat() + asw_skill_accuracy_rifle_dmg_step.GetFloat() * iSkillPoints; break;
			}			
			break;
		case ASW_MARINE_SKILL_GRENADES:
			switch ( iSubSkill )
			{
				case ASW_MARINE_SUBSKILL_GRENADE_RADIUS: return asw_skill_grenades_radius_base.GetFloat() + asw_skill_grenades_radius_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_GRENADE_CLUSTERS: return asw_skill_grenades_clusters_base.GetFloat() + asw_skill_grenades_clusters_step.GetFloat() * iSkillPoints;	break;
				case ASW_MARINE_SUBSKILL_GRENADE_INCENDIARY_DMG: return asw_skill_grenades_incendiary_dmg_base.GetFloat() + asw_skill_grenades_incendiary_dmg_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_GRENADE_CLUSTER_DMG: return asw_skill_grenades_cluster_dmg_base.GetFloat() + asw_skill_grenades_cluster_dmg_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_GRENADE_FLECHETTE_DMG: return asw_skill_grenades_flechette_dmg_base.GetFloat() + asw_skill_grenades_flechette_dmg_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_GRENADE_HORNET_DMG: return asw_skill_grenades_hornet_dmg_base.GetFloat() + asw_skill_grenades_hornet_dmg_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_GRENADE_HORNET_COUNT: return asw_skill_grenades_hornet_count_base.GetFloat() + asw_skill_grenades_hornet_count_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_GRENADE_HORNET_INTERVAL: return asw_skill_grenades_hornet_interval_base.GetFloat() + asw_skill_grenades_hornet_interval_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_GRENADE_FREEZE_RADIUS: return asw_skill_grenades_freeze_radius_base.GetFloat() + asw_skill_grenades_freeze_radius_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_GRENADE_FREEZE_DURATION: return asw_skill_grenades_freeze_duration_base.GetFloat() + asw_skill_grenades_freeze_duration_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_GRENADE_SMART_BOMB_COUNT: return asw_skill_grenades_smart_count_base.GetFloat() + asw_skill_grenades_smart_count_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_GRENADE_SMART_BOMB_INTERVAL: return asw_skill_grenades_smart_interval_base.GetFloat() + asw_skill_grenades_smart_interval_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_GRENADE_LASER_MINES: return ( iSkillPoints > 3 ? 3 : ( iSkillPoints > 0 ? 2 : 1 ) ); break;
				case ASW_MARINE_SUBSKILL_GRENADE_MINES_FIRES: return asw_skill_mines_fires_base.GetFloat() + asw_skill_mines_fires_step.GetFloat() * iSkillPoints; break;
				case ASW_MARINE_SUBSKILL_GRENADE_MINES_DURATION: return asw_skill_mines_duration_base.GetFloat() + asw_skill_mines_duration_step.GetFloat() * iSkillPoints; break;
				default: return asw_skill_grenades_dmg_base.GetFloat() + asw_skill_grenades_dmg_step.GetFloat() * iSkillPoints; break;
			}
			break;
		case ASW_MARINE_SKILL_HEALTH:
			return asw_skill_health_base.GetFloat() + asw_skill_health_step.GetFloat() * iSkillPoints;
			break;
		case ASW_MARINE_SKILL_MELEE:
			if ( iSubSkill == ASW_MARINE_SUBSKILL_MELEE_DMG )
			{
				return asw_skill_melee_dmg_base.GetFloat() + asw_skill_melee_dmg_step.GetFloat() * iSkillPoints;
			}
			else if (iSubSkill == ASW_MARINE_SUBSKILL_MELEE_FLINCH)
			{
				// return a different length flinch (tiny, small, large) based on our skill points
				if (iSkillPoints <= 1)
					return 0;
				else if (iSkillPoints <= 3)
					return 1;
				else
					return 2;
			}
			else if (iSubSkill == ASW_MARINE_SUBSKILL_MELEE_SPEED)
			{
				return asw_skill_melee_speed_base.GetFloat() + asw_skill_melee_speed_step.GetFloat() * iSkillPoints;
			}
			else
			{
				return asw_skill_melee_force_base.GetFloat() + asw_skill_melee_force_step.GetFloat() * iSkillPoints;
			}
			break;
		case ASW_MARINE_SKILL_RELOADING:
			if ( iSubSkill == ASW_MARINE_SUBSKILL_RELOADING_SPEED_SCALE )
				return asw_skill_reloading_base.GetFloat() + asw_skill_reloading_step.GetFloat() * iSkillPoints;
			else if ( iSubSkill == ASW_MARINE_SUBSKILL_RELOADING_FAST_WIDTH_SCALE )
				return asw_skill_reloading_fast_base.GetFloat() + asw_skill_reloading_fast_step.GetFloat() * iSkillPoints;
			else	// ASW_MARINE_SUBSKILL_RELOADING_SOUND
				return iSkillPoints;
			break;
		case ASW_MARINE_SKILL_AGILITY:
			return asw_skill_agility_movespeed_base.GetFloat() + asw_skill_agility_movespeed_step.GetFloat() * iSkillPoints;
			break;
		default:
			Assert( "Unknown marine skill or subskill" );
			return 0;
			break;
	}
	Assert( "Unknown marine skill or subskill" );
	return 0;
}

float CASW_Marine_Skills::GetBestSkillValue(ASW_Skill iSkillIndex, int iSubSkill)
{
	if ( !ASWGameResource() || !MarineProfileList() )
		return 0;

	float fBestSkill = -1;
	CASW_Game_Resource *pGameResource = ASWGameResource();

	// find the live marine with the highest value for this skill
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if (pMR && pMR->GetHealthPercent() > 0 && pMR->IsAlive() && pMR->GetProfile())
		{
			float skill = GetSkillBasedValueByMarineResource(pMR, iSkillIndex, iSubSkill);
			if (skill > fBestSkill)
				fBestSkill = skill;
		}
	}
	return fBestSkill;
}

float CASW_Marine_Skills::GetHighestSkillValueNearby(const Vector &pos, float MaxDistance, ASW_Skill iSkillIndex, int iSubSkill)
{
#ifndef CLIENT_DLL
	m_hLastSkillMarine = NULL;
#endif
	if ( !ASWGameResource() || !MarineProfileList() )
		return 0;

	float fBestSkill = -1;
	CASW_Game_Resource *pGameResource = ASWGameResource();

	// find the live marine with the highest value for this skill
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if (pMR && pMR->GetHealthPercent() > 0 && pMR->IsAlive() && pMR->GetProfile() && pMR->GetMarineEntity())
		{
			// check he's near enough
			float dist = pMR->GetMarineEntity()->GetAbsOrigin().DistTo(pos);
			if (dist > MaxDistance)
				continue;

			float skill = GetSkillBasedValueByMarineResource(pMR, iSkillIndex, iSubSkill);
			if (skill > fBestSkill)
			{
				fBestSkill = skill;
#ifndef CLIENT_DLL
				m_hLastSkillMarine = pMR->GetMarineEntity();
#endif
			}
		}
	}
	return fBestSkill;
}

float CASW_Marine_Skills::GetLowestSkillValueNearby(const Vector &pos, float MaxDistance, ASW_Skill iSkillIndex, int iSubSkill)
{
#ifndef CLIENT_DLL
	m_hLastSkillMarine = NULL;
#endif
	if ( !ASWGameResource() || !MarineProfileList() )
		return 0;

	float fBestSkill = -1;
	CASW_Game_Resource *pGameResource = ASWGameResource();

	// find the live marine with the highest value for this skill
	for (int i=0;i<pGameResource->GetMaxMarineResources();i++)
	{
		CASW_Marine_Resource *pMR = pGameResource->GetMarineResource(i);
		if (pMR && pMR->GetHealthPercent() > 0 && pMR->IsAlive() && pMR->GetProfile() && pMR->GetMarineEntity())
		{
			// check he's near enough
			float dist = pMR->GetMarineEntity()->GetAbsOrigin().DistTo(pos);
			if (dist > MaxDistance)
				continue;

			float skill = GetSkillBasedValueByMarineResource(pMR, iSkillIndex, iSubSkill);
			if (fBestSkill == -1 || skill < fBestSkill)
			{
#ifndef CLIENT_DLL
				m_hLastSkillMarine = pMR->GetMarineEntity();
#endif
				fBestSkill = skill;
			}
		}
	}
	return fBestSkill;
}

const char* s_szSkillImageName[ ASW_NUM_MARINE_SKILLS ] =
{
	"swarm/SkillButtons/Leadership", // ASW_MARINE_SKILL_LEADERSHIP = 0,
	"swarm/SkillButtons/Vindicator", // ASW_MARINE_SKILL_VINDICATOR,

	// special weapons
	"swarm/SkillButtons/Autogun", // ASW_MARINE_SKILL_AUTOGUN,
	"swarm/SkillButtons/Secondary", // ASW_MARINE_SKILL_STOPPING_POWER,
	"swarm/SkillButtons/Piercing", // ASW_MARINE_SKILL_PIERCING,

	// medic
	"swarm/SkillButtons/Healing", // ASW_MARINE_SKILL_HEALING,
	"swarm/SkillButtons/Xenowound", // ASW_MARINE_SKILL_XENOWOUNDS,
	"swarm/SkillButtons/Drugs", // ASW_MARINE_SKILL_DRUGS,

	// tech
	"swarm/SkillButtons/Hacking", //ASW_MARINE_SKILL_HACKING,
	"swarm/SkillButtons/Scanner", // 3ASW_MARINE_SKILL_SCANNER,
	"swarm/SkillButtons/Engineering", // 3ASW_MARINE_SKILL_ENGINEERING,

	"swarm/SkillButtons/Accuracy", // ASW_MARINE_SKILL_ACCURACY,
	"swarm/SkillButtons/Grenade", // ASW_MARINE_SKILL_GRENADES,
	"swarm/SkillButtons/Health", // ASW_MARINE_SKILL_HEALTH,
	"swarm/SkillButtons/Melee", // ASW_MARINE_SKILL_MELEE,
	"swarm/SkillButtons/SkillReload", // ASW_MARINE_SKILL_RELOADING,
	"swarm/SkillButtons/Agility", // ASW_MARINE_SKILL_AGILITY

	"swarm/SkillButtons/Spare", // ASW_MARINE_SKILL_SPARE
};

const char* s_szSkillName[ ASW_NUM_MARINE_SKILLS ] =
{
	"#asw_leadership", // ASW_MARINE_SKILL_LEADERSHIP = 0,
	"#asw_vindicator", // ASW_MARINE_SKILL_VINDICATOR,

	// special weapons
	"#asw_autogunsk", // ASW_MARINE_SKILL_AUTOGUN,
	"#asw_stopping", // ASW_MARINE_SKILL_STOPPING_POWER,
	"#asw_piercingbullets", // ASW_MARINE_SKILL_PIERCING,

	// medic
	"#asw_healing", // ASW_MARINE_SKILL_HEALING,
	"#asw_xenowound", // ASW_MARINE_SKILL_XENOWOUNDS,
	"#asw_combatdrugs", // ASW_MARINE_SKILL_DRUGS,

	// tech
	"#asw_hacking", //ASW_MARINE_SKILL_HACKING,
	"#asw_scanner", // 3ASW_MARINE_SKILL_SCANNER,
	"#asw_engineering", // 3ASW_MARINE_SKILL_ENGINEERING,

	"#asw_accuracy_skill", // ASW_MARINE_SKILL_ACCURACY,
	"#asw_grenades", // ASW_MARINE_SKILL_GRENADES,
	"#asw_health", // ASW_MARINE_SKILL_HEALTH,
	"#asw_melee", // ASW_MARINE_SKILL_MELEE,
	"#asw_reloading", // ASW_MARINE_SKILL_RELOADING,
	"#asw_agility", // ASW_MARINE_SKILL_AGILITY

	"#asw_points", // ASW_MARINE_SKILL_SPARE
};

const char* s_szSkillDescription[ ASW_NUM_MARINE_SKILLS ] =
{
	"#asw_leadership_desc", // ASW_MARINE_SKILL_LEADERSHIP = 0,
	"#asw_vindicator_desc", // ASW_MARINE_SKILL_VINDICATOR,

	// special weapons
	"#asw_autogunsk_desc", // ASW_MARINE_SKILL_AUTOGUN,
	"#asw_stopping_desc", // ASW_MARINE_SKILL_STOPPING_POWER,
	"#asw_piercingbullets_desc", // ASW_MARINE_SKILL_PIERCING,

	// medic
	"#asw_healing_desc", // ASW_MARINE_SKILL_HEALING,
	"#asw_xenowound_desc", // ASW_MARINE_SKILL_XENOWOUNDS,
	"#asw_combatdrugs_desc", // ASW_MARINE_SKILL_DRUGS,

	// tech
	"#asw_hacking_desc", //ASW_MARINE_SKILL_HACKING,
	"#asw_scanner_desc", // 3ASW_MARINE_SKILL_SCANNER,
	"#asw_engineering_desc", // 3ASW_MARINE_SKILL_ENGINEERING,

	"#asw_accuracy_skill_desc", // ASW_MARINE_SKILL_ACCURACY,
	"#asw_grenades_desc", // ASW_MARINE_SKILL_GRENADES,
	"#asw_health_desc", // ASW_MARINE_SKILL_HEALTH,
	"#asw_melee_desc", // ASW_MARINE_SKILL_MELEE,
	"#asw_reloading_desc", // ASW_MARINE_SKILL_RELOADING,
	"#asw_agility_desc", // ASW_MARINE_SKILL_AGILITY

	"#asw_points_desc", // ASW_MARINE_SKILL_SPARE
};

int s_nMaxSkillPoints[ ASW_NUM_MARINE_SKILLS ] =
{
	5, // ASW_MARINE_SKILL_LEADERSHIP = 0,
	5, // ASW_MARINE_SKILL_VINDICATOR,

	// special weapons
	5, // ASW_MARINE_SKILL_AUTOGUN,
	5, // ASW_MARINE_SKILL_STOPPING_POWER,
	5, // ASW_MARINE_SKILL_PIERCING,

	// medic
	5, // ASW_MARINE_SKILL_HEALING,
	3, // ASW_MARINE_SKILL_XENOWOUNDS,
	5, // ASW_MARINE_SKILL_DRUGS,

	// tech
	5, //ASW_MARINE_SKILL_HACKING,
	3, // 3ASW_MARINE_SKILL_SCANNER,
	3, // 3ASW_MARINE_SKILL_ENGINEERING,

	5, // ASW_MARINE_SKILL_ACCURACY,
	5, // ASW_MARINE_SKILL_GRENADES,
	5, // ASW_MARINE_SKILL_HEALTH,
	5, // ASW_MARINE_SKILL_MELEE,
	5, // ASW_MARINE_SKILL_RELOADING,
	5, // ASW_MARINE_SKILL_AGILITY

	99, // ASW_MARINE_SKILL_SPARE
};

#ifdef CLIENT_DLL
const char* CASW_Marine_Skills::GetSkillImage( ASW_Skill nSkillIndex )
{
	if ( nSkillIndex < 0 || nSkillIndex >= ASW_NUM_MARINE_SKILLS )
		return "";

	return s_szSkillImageName[ nSkillIndex ];
}

const char* CASW_Marine_Skills::GetSkillName( ASW_Skill nSkillIndex )
{
	if ( nSkillIndex < 0 || nSkillIndex >= ASW_NUM_MARINE_SKILLS )
		return "";

	return s_szSkillName[ nSkillIndex ];
}

const char* CASW_Marine_Skills::GetSkillDescription( ASW_Skill nSkillIndex )
{
	if ( nSkillIndex < 0 || nSkillIndex >= ASW_NUM_MARINE_SKILLS )
		return "";

	return s_szSkillDescription[ nSkillIndex ];
}
#endif

int CASW_Marine_Skills::GetMaxSkillPoints( ASW_Skill nSkillIndex )
{
	if ( nSkillIndex < 0 || nSkillIndex >= ASW_NUM_MARINE_SKILLS )
		return 0;

	return s_nMaxSkillPoints[ nSkillIndex ];
}

const char* g_szSkillNames[ ASW_NUM_MARINE_SKILLS ] =
{
	"ASW_MARINE_SKILL_LEADERSHIP",
	"ASW_MARINE_SKILL_VINDICATOR",

	// special weapons
	"ASW_MARINE_SKILL_AUTOGUN",
	"ASW_MARINE_SKILL_STOPPING_POWER",
	"ASW_MARINE_SKILL_PIERCING",

	// medic
	"ASW_MARINE_SKILL_HEALING",
	"ASW_MARINE_SKILL_XENOWOUNDS",
	"ASW_MARINE_SKILL_DRUGS",

	// tech
	"ASW_MARINE_SKILL_HACKING",
	"ASW_MARINE_SKILL_SCANNER",
	"ASW_MARINE_SKILL_ENGINEERING",

	"ASW_MARINE_SKILL_ACCURACY",
	"ASW_MARINE_SKILL_GRENADES",
	"ASW_MARINE_SKILL_HEALTH",
	"ASW_MARINE_SKILL_MELEE",
	"ASW_MARINE_SKILL_RELOADING",
	"ASW_MARINE_SKILL_AGILITY",

	"ASW_MARINE_SKILL_SPARE",
};

ASW_Skill SkillFromString( const char *szSkill )
{
	int nSkills = NELEMS( g_szSkillNames );
	for ( int i = 0; i < nSkills; i++ )
	{
		if ( !Q_stricmp( szSkill, g_szSkillNames[i] ) )
		{
			return (ASW_Skill) i;
		}
	}

	return ASW_MARINE_SKILL_INVALID;
}

const char * SkillToString( ASW_Skill nSkill )
{
	if ( nSkill < 0 || nSkill >= NELEMS( g_szSkillNames ) )
	{
		return "ASW_MARINE_SKILL_INVALID";
	}
	return g_szSkillNames[ nSkill ];
}


// global instance
CASW_Marine_Skills g_MarineSkills;
CASW_Marine_Skills* MarineSkills()
{
	return &g_MarineSkills;
}