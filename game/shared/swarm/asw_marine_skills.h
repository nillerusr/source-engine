#ifndef _INCLUDED_ASW_MARINE_SKILLS_H
#define _INCLUDED_ASW_MARINE_SKILLS_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_shareddefs.h"

// marine skill enumerations

// skills
enum ASW_Skill
{
	ASW_MARINE_SKILL_INVALID = -1,

	// NCO
	ASW_MARINE_SKILL_LEADERSHIP = 0,
	ASW_MARINE_SKILL_VINDICATOR,

	// special weapons
	ASW_MARINE_SKILL_AUTOGUN,
	ASW_MARINE_SKILL_STOPPING_POWER,		// note: skill code doesn't calculate the actual % chance, because that is an attribute of the individual guns.  See their weapon info for the stopping power % increase per skill point
	ASW_MARINE_SKILL_PIERCING,

	// medic
	ASW_MARINE_SKILL_HEALING,
	ASW_MARINE_SKILL_XENOWOUNDS,
	ASW_MARINE_SKILL_DRUGS,

	// tech
	ASW_MARINE_SKILL_HACKING,
	ASW_MARINE_SKILL_SCANNER,
	ASW_MARINE_SKILL_ENGINEERING,

	ASW_MARINE_SKILL_ACCURACY,
	ASW_MARINE_SKILL_GRENADES,
	ASW_MARINE_SKILL_HEALTH,
	ASW_MARINE_SKILL_MELEE,
	ASW_MARINE_SKILL_RELOADING,
	ASW_MARINE_SKILL_AGILITY,

	ASW_MARINE_SKILL_SPARE,

	// NOTE: If you change this, update: SkillFromString, s_nMaxSkillPoints, s_szSkillName

	ASW_NUM_MARINE_SKILLS
};

ASW_Skill SkillFromString( const char *szSkill );
const char * SkillToString( ASW_Skill nSkill );

// subskills - used to select different elements of the game that are affected by the same skill
enum
{
	ASW_MARINE_SUBSKILL_LEADERSHIP_ACCURACY_CHANCE = 0,
	ASW_MARINE_SUBSKILL_LEADERSHIP_DAMAGE_RESIST,
	ASW_MARINE_SUBSKILL_VINDICATOR_DAMAGE = 0,
	ASW_MARINE_SUBSKILL_VINDICATOR_PELLETS,
	ASW_MARINE_SUBSKILL_VINDICATOR_MUZZLE,
	ASW_MARINE_SUBSKILL_GRENADE_RADIUS = 0,
	ASW_MARINE_SUBSKILL_GRENADE_DMG,
	ASW_MARINE_SUBSKILL_GRENADE_INCENDIARY_DMG,
	ASW_MARINE_SUBSKILL_GRENADE_CLUSTER_DMG,
	ASW_MARINE_SUBSKILL_GRENADE_CLUSTERS,
	ASW_MARINE_SUBSKILL_GRENADE_FLECHETTE_DMG,
	ASW_MARINE_SUBSKILL_GRENADE_HORNET_DMG,
	ASW_MARINE_SUBSKILL_GRENADE_HORNET_COUNT,
	ASW_MARINE_SUBSKILL_GRENADE_HORNET_INTERVAL,
	ASW_MARINE_SUBSKILL_GRENADE_FREEZE_RADIUS,
	ASW_MARINE_SUBSKILL_GRENADE_FREEZE_DURATION,
	ASW_MARINE_SUBSKILL_GRENADE_SMART_BOMB_COUNT,
	ASW_MARINE_SUBSKILL_GRENADE_SMART_BOMB_INTERVAL,
	ASW_MARINE_SUBSKILL_GRENADE_LASER_MINES,
	ASW_MARINE_SUBSKILL_GRENADE_MINES_FIRES,
	ASW_MARINE_SUBSKILL_GRENADE_MINES_DURATION,
	ASW_MARINE_SUBSKILL_MELEE_DMG = 0,
	ASW_MARINE_SUBSKILL_MELEE_FORCE,
	ASW_MARINE_SUBSKILL_MELEE_FLINCH,
	ASW_MARINE_SUBSKILL_MELEE_SPEED,
	ASW_MARINE_SUBSKILL_ENGINEERING_WELDING = 0,
	ASW_MARINE_SUBSKILL_ENGINEERING_SENTRY,
	ASW_MARINE_SUBSKILL_HEALING_CHARGES = 0,
	ASW_MARINE_SUBSKILL_SELF_HEALING_CHARGES,
	ASW_MARINE_SUBSKILL_HEALING_HPS,
	ASW_MARINE_SUBSKILL_HEALING_MEDKIT_HPS,
	ASW_MARINE_SUBSKILL_HEAL_GRENADE_HEAL_AMOUNT,
	ASW_MARINE_SUBSKILL_HEAL_GUN_CHARGES,
	ASW_MARINE_SUBSKILL_HEAL_GUN_HEAL_AMOUNT,
	ASW_MARINE_SUBSKILL_AUTOGUN_DMG = 0,
	ASW_MARINE_SUBSKILL_AUTOGUN_MUZZLE,
	ASW_MARINE_SUBSKILL_ACCURACY_RIFLE_DMG = 0,
	ASW_MARINE_SUBSKILL_ACCURACY_PRIFLE_DMG,
	ASW_MARINE_SUBSKILL_ACCURACY_SHOTGUN_DMG,
	ASW_MARINE_SUBSKILL_ACCURACY_RAILGUN_DMG,
	ASW_MARINE_SUBSKILL_ACCURACY_FLAMER_DMG,
	ASW_MARINE_SUBSKILL_ACCURACY_PISTOL_DMG,
	ASW_MARINE_SUBSKILL_ACCURACY_PDW_DMG,
	ASW_MARINE_SUBSKILL_ACCURACY_SNIPER_RIFLE_DMG,
	ASW_MARINE_SUBSKILL_ACCURACY_TESLA_CANNON_DMG,
	ASW_MARINE_SUBSKILL_ACCURACY_MUZZLE,
	ASW_MARINE_SUBSKILL_HACKING_SPEED_SCALE = 0,
	ASW_MARINE_SUBSKILL_HACKING_TUMBLER_COUNT_REDUCTION,
	ASW_MARINE_SUBSKILL_HACKING_ENTRIES_PER_TUMBLER_REDUCTION,
	ASW_MARINE_SUBSKILL_RELOADING_SPEED_SCALE = 0,
	ASW_MARINE_SUBSKILL_RELOADING_SOUND,	
	ASW_MARINE_SUBSKILL_RELOADING_FAST_WIDTH_SCALE,
};

#ifdef CLIENT_DLL
class C_ASW_Marine;
class C_ASW_Marine_Resource;
#else
class CASW_Marine;
class CASW_Marine_Resource;
#endif
class CASW_Marine_Profile;

// this class holds accessor functions for getting at the variables that are affected by skills
class CASW_Marine_Skills
{
public:
	CASW_Marine_Skills();
#ifdef CLIENT_DLL
	// get the value based on the current skills of that marine
	float GetSkillBasedValueByMarine(C_ASW_Marine *pMarine, ASW_Skill iSkillIndex, int iSubSkill=0);
	float GetSkillBasedValueByMarineResource(C_ASW_Marine_Resource *pMarineResource, ASW_Skill iSkillIndex, int iSubSkill=0);
#else
	float GetSkillBasedValueByMarine(CASW_Marine *pMarine, ASW_Skill iSkillIndex, int iSubSkill=0);
	float GetSkillBasedValueByMarineResource(CASW_Marine_Resource *pMarineResource, ASW_Skill iSkillIndex, int iSubSkill=0);
#endif
	// returns the live marine with the best skill of this type
	float GetBestSkillValue( ASW_Skill iSkillIndex, int iSubSkill=0);
	float GetHighestSkillValueNearby(const Vector &pos, float MaxDistance, ASW_Skill iSkillIndex, int iSubSkill=0);
	float GetLowestSkillValueNearby(const Vector &pos, float MaxDistance, ASW_Skill iSkillIndex, int iSubSkill=0);
	// get the value based on the current skills of that marine, or based on some specified number of points in that skill
	float GetSkillBasedValue( CASW_Marine_Profile *pProfile, ASW_Skill iSkillIndex, int iSubSkill=0, int iSkillPoints=-1 );
#ifndef CLIENT_DLL
	// returns the marine last used by the GetHighest/Lowest functions
	CASW_Marine* GetLastSkillMarine() { return m_hLastSkillMarine.Get(); }		
	CHandle<CASW_Marine> m_hLastSkillMarine;
#else
	const char* GetSkillImage( ASW_Skill nSkillIndex );
	const char* GetSkillName( ASW_Skill nSkillIndex );
	const char* GetSkillDescription( ASW_Skill nSkillIndex );
#endif
	int GetMaxSkillPoints( ASW_Skill nSkillIndex );
};

// global accessor
CASW_Marine_Skills* MarineSkills();

#endif // _INCLUDED_ASW_MARINE_SKILLS_H

