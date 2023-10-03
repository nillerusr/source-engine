#include "cbase.h"
#include "asw_shareddefs.h"
#include "asw_medals_shared.h"
#include "asw_achievements.h"

bool IsAlienClass( Class_T npc_class )
{
	return ( npc_class == CLASS_ASW_DRONE ||
		npc_class == CLASS_ASW_PARASITE ||
		npc_class == CLASS_ASW_SHIELDBUG ||
		npc_class == CLASS_ASW_BUZZER ||
		npc_class == CLASS_ASW_HARVESTER ||
		npc_class == CLASS_ASW_GRUB ||
		npc_class == CLASS_ASW_QUEEN_DIVER ||
		npc_class == CLASS_ASW_QUEEN_GRABBER ||
		npc_class == CLASS_ASW_ALIEN_GOO ||
		npc_class == CLASS_ASW_SHAMAN ||
		npc_class == CLASS_ASW_BOOMER ||
		npc_class == CLASS_ASW_RANGER ||
		npc_class == CLASS_ASW_EGG
		);
}

// 		npc_class == CLASS_ASW_ZOMBIE ||
// 		npc_class == CLASS_ASW_HYDRA ||
// 		npc_class == CLASS_BLOB ||
// 		npc_class == CLASS_ASW_BOOMERMINI ||
// 		npc_class == CLASS_ASW_MEATBUG ||
// 		npc_class == CLASS_ASW_RUNNER ||
// 		npc_class == CLASS_ASW_FLOCK ||
// 		);
//}

bool IsDamagingWeaponClass( Class_T weapon_class )
{
	return ( weapon_class == CLASS_ASW_RIFLE ||
		weapon_class == CLASS_ASW_MINIGUN ||
		weapon_class == CLASS_ASW_PDW ||
		weapon_class == CLASS_ASW_FLECHETTE ||
		weapon_class == CLASS_ASW_TESLA_TRAP ||
		weapon_class == CLASS_ASW_SHOTGUN ||
		weapon_class == CLASS_ASW_SENTRY_FLAMER ||
		weapon_class == CLASS_ASW_SENTRY_CANNON ||
		weapon_class == CLASS_ASW_SENTRY_FREEZE ||
		weapon_class == CLASS_ASW_RICOCHET ||
		weapon_class == CLASS_ASW_RAILGUN ||
		weapon_class == CLASS_ASW_PRIFLE ||
		weapon_class == CLASS_ASW_PISTOL ||
		weapon_class == CLASS_ASW_MINING_LASER ||
		weapon_class == CLASS_ASW_MINES ||
		weapon_class == CLASS_ASW_LASER_MINES ||
		weapon_class == CLASS_ASW_HORNET_BARRAGE ||
		weapon_class == CLASS_ASW_HEALGRENADE ||
		weapon_class == CLASS_ASW_GRENADES ||
		weapon_class == CLASS_ASW_GRENADE_LAUNCHER ||
		weapon_class == CLASS_ASW_FREEZE_GRENADES ||
		weapon_class == CLASS_ASW_FLAMER ||
		weapon_class == CLASS_ASW_ELECTRIFIED_ARMOR ||
		weapon_class == CLASS_ASW_CHAINSAW ||
		weapon_class == CLASS_ASW_AUTOGUN ||
		weapon_class == CLASS_ASW_ASSAULT_SHOTGUN ||
		weapon_class == CLASS_ASW_SENTRY_GUN ||
		weapon_class == CLASS_ASW_T75 ||
		weapon_class == CLASS_ASW_RIFLE_GRENADE ||
		weapon_class == CLASS_ASW_GRENADE_VINDICATOR ||
		weapon_class == CLASS_ASW_GRENADE_CLUSER ||
		weapon_class == CLASS_MISSILE ||
		weapon_class == CLASS_ASW_FLAMER_PROJECTILE ||
		weapon_class == CLASS_ASW_GRENADE_PRIFLE);
}

bool IsWeaponClass( Class_T entity_class )
{
	return ( entity_class == CLASS_ASW_RIFLE ||
		entity_class == CLASS_ASW_SNIPER_RIFLE ||
		entity_class == CLASS_ASW_MINIGUN ||
		entity_class == CLASS_ASW_PDW ||
		entity_class == CLASS_ASW_FLECHETTE ||
		entity_class == CLASS_ASW_FIRE_EXTINGUISHER ||
		entity_class == CLASS_ASW_WELDER ||
		entity_class == CLASS_ASW_TESLA_TRAP ||
		entity_class == CLASS_ASW_STIM ||
		entity_class == CLASS_ASW_SHOTGUN ||
		entity_class == CLASS_ASW_SENTRY_FLAMER ||
		entity_class == CLASS_ASW_SENTRY_CANNON ||
		entity_class == CLASS_ASW_SENTRY_FREEZE ||
		entity_class == CLASS_ASW_RICOCHET ||
		entity_class == CLASS_ASW_RAILGUN ||
		entity_class == CLASS_ASW_PRIFLE ||
		entity_class == CLASS_ASW_PISTOL ||
		entity_class == CLASS_ASW_MINING_LASER ||
		entity_class == CLASS_ASW_MINES ||
		entity_class == CLASS_ASW_MEDKIT ||
		entity_class == CLASS_ASW_MEDICAL_SATCHEL ||
		entity_class == CLASS_ASW_HEAL_GUN ||
		entity_class == CLASS_ASW_LASER_MINES ||
		entity_class == CLASS_ASW_HORNET_BARRAGE ||
		entity_class == CLASS_ASW_HEALGRENADE ||
		entity_class == CLASS_ASW_GRENADES ||
		entity_class == CLASS_ASW_GRENADE_LAUNCHER ||
		entity_class == CLASS_ASW_FREEZE_GRENADES ||
		entity_class == CLASS_ASW_FLASHLIGHT ||
		entity_class == CLASS_ASW_FLARES ||
		entity_class == CLASS_ASW_FLAMER ||
		entity_class == CLASS_ASW_ELECTRIFIED_ARMOR ||
		entity_class == CLASS_ASW_NORMAL_ARMOR ||
		entity_class == CLASS_ASW_CHAINSAW ||
		entity_class == CLASS_ASW_BUFF_GRENADE ||
		entity_class == CLASS_ASW_AUTOGUN ||
		entity_class == CLASS_ASW_ASSAULT_SHOTGUN ||
		entity_class == CLASS_ASW_AMMO_BAG ||
		entity_class == CLASS_ASW_SENTRY_GUN ||
		entity_class == CLASS_ASW_T75 ||
		entity_class == CLASS_ASW_RIFLE_GRENADE ||
		entity_class == CLASS_ASW_GRENADE_VINDICATOR ||
		entity_class == CLASS_ASW_GRENADE_CLUSER ||
		entity_class == CLASS_MISSILE ||
		entity_class == CLASS_ASW_FLAMER_PROJECTILE ||
		entity_class == CLASS_ASW_TESLA_GUN ||
		entity_class == CLASS_ASW_SMART_BOMB );
}

// used by powerups
bool IsBulletBasedWeaponClass( Class_T weapon_class )
{
	return ( weapon_class == CLASS_ASW_RIFLE ||
		weapon_class == CLASS_ASW_MINIGUN ||
		weapon_class == CLASS_ASW_PDW ||
		weapon_class == CLASS_ASW_FLECHETTE ||
		weapon_class == CLASS_ASW_SHOTGUN ||
		weapon_class == CLASS_ASW_RICOCHET ||
		weapon_class == CLASS_ASW_RAILGUN ||
		weapon_class == CLASS_ASW_PRIFLE ||
		weapon_class == CLASS_ASW_PISTOL ||
		weapon_class == CLASS_ASW_AUTOGUN ||
		weapon_class == CLASS_ASW_ASSAULT_SHOTGUN);
}

bool IsSentryClass( Class_T entity_class )
{
	return ( entity_class == CLASS_ASW_SENTRY_GUN ||
			entity_class == CLASS_ASW_SENTRY_FLAMER ||
			entity_class == CLASS_ASW_SENTRY_FREEZE ||
			entity_class == CLASS_ASW_SENTRY_CANNON ||
			entity_class == CLASS_ASW_REMOTE_TURRET );
}

ConVar asw_visrange_generic( "asw_visrange_generic", "400", FCVAR_CHEAT | FCVAR_REPLICATED, "Vismon range" );

#ifdef CLIENT_DLL
IMPLEMENT_AUTO_LIST( IHealthTrackedAutoList );
#endif

// certain props are exempt from our auto stuck freeing code, since they break easily
bool CanMarineGetStuckOnProp( const char *szModelName )
{
	if ( !Q_stricmp( szModelName, "models/props/crates/supplycrate.mdl" ) )
		return false;

	if ( !Q_stricmp( szModelName, "models/swarmprops/barrelsandcrates/cardbox1breakable.mdl" ) )
		return false;

	return true;
}

class CASW_Medal_Achievement_Pair
{
public:
	CASW_Medal_Achievement_Pair( int nMedal, int nAchievement )
	{
		m_nMedal = nMedal;
		m_nAchievement = nAchievement;
	}
	int m_nMedal;
	int m_nAchievement;
};

CASW_Medal_Achievement_Pair s_MedalAchievements[]=
{
	CASW_Medal_Achievement_Pair( MEDAL_CLEAR_FIRING, ACHIEVEMENT_ASW_NO_FRIENDLY_FIRE ),
	CASW_Medal_Achievement_Pair( MEDAL_SHIELDBUG_ASSASSIN, ACHIEVEMENT_ASW_SHIELDBUG ),
	CASW_Medal_Achievement_Pair( MEDAL_EXPLOSIVES_MERIT, ACHIEVEMENT_ASW_GRENADE_MULTI_KILL ),
	CASW_Medal_Achievement_Pair( MEDAL_SHARPSHOOTER, ACHIEVEMENT_ASW_ACCURACY ),
	CASW_Medal_Achievement_Pair( MEDAL_PERFECT, ACHIEVEMENT_ASW_NO_DAMAGE_TAKEN ),
	CASW_Medal_Achievement_Pair( MEDAL_IRON_FIST, ACHIEVEMENT_ASW_MELEE_KILLS ),
	CASW_Medal_Achievement_Pair( MEDAL_COLLATERAL_DAMAGE, ACHIEVEMENT_ASW_BARREL_KILLS ),
	CASW_Medal_Achievement_Pair( MEDAL_GOLDEN_HALO, ACHIEVEMENT_ASW_INFESTATION_CURING ),
	CASW_Medal_Achievement_Pair( MEDAL_BLOOD_HALO, ACHIEVEMENT_ASW_ALL_HEALING ),
	CASW_Medal_Achievement_Pair( MEDAL_PEST_CONTROL, ACHIEVEMENT_ASW_EGGS_BEFORE_HATCH ),
	CASW_Medal_Achievement_Pair( MEDAL_EXTERMINATOR, ACHIEVEMENT_ASW_MELEE_PARASITE ),
	CASW_Medal_Achievement_Pair( MEDAL_ELECTRICAL_SYSTEMS_EXPERT, ACHIEVEMENT_ASW_PROTECT_TECH ),
	CASW_Medal_Achievement_Pair( MEDAL_SMALL_ARMS_SPECIALIST, ACHIEVEMENT_ASW_FAST_RELOADS_IN_A_ROW ),
	CASW_Medal_Achievement_Pair( MEDAL_GUNFIGHTER, ACHIEVEMENT_ASW_GROUP_DAMAGE_AMP ),
	CASW_Medal_Achievement_Pair( MEDAL_BUGSTOMPER, ACHIEVEMENT_ASW_BOOMER_KILL_EARLY ),
	CASW_Medal_Achievement_Pair( MEDAL_RECKLESS_EXPLOSIVES_MERIT, ACHIEVEMENT_ASW_STUN_GRENADE ),
	CASW_Medal_Achievement_Pair( MEDAL_LIFESAVER, ACHIEVEMENT_ASW_DODGE_RANGER_SHOT ),
	CASW_Medal_Achievement_Pair( MEDAL_HUNTER, ACHIEVEMENT_ASW_FREEZE_GRENADE ),
	CASW_Medal_Achievement_Pair( MEDAL_SPEED_RUN_LANDING_BAY, ACHIEVEMENT_ASW_SPEEDRUN_LANDING_BAY ),
	CASW_Medal_Achievement_Pair( MEDAL_SPEED_RUN_DESCENT, ACHIEVEMENT_ASW_SPEEDRUN_DESCENT ),
	CASW_Medal_Achievement_Pair( MEDAL_SPEED_RUN_OUTSIDE, ACHIEVEMENT_ASW_SPEEDRUN_DEIMA ),
	CASW_Medal_Achievement_Pair( MEDAL_SPEED_RUN_PLANT, ACHIEVEMENT_ASW_SPEEDRUN_RYDBERG ),
	CASW_Medal_Achievement_Pair( MEDAL_SPEED_RUN_OFFICE, ACHIEVEMENT_ASW_SPEEDRUN_RESIDENTIAL ),
	CASW_Medal_Achievement_Pair( MEDAL_SPEED_RUN_DESCENT, ACHIEVEMENT_ASW_SPEEDRUN_DESCENT ),
	CASW_Medal_Achievement_Pair( MEDAL_SPEED_RUN_SEWERS, ACHIEVEMENT_ASW_SPEEDRUN_SEWER ),
	CASW_Medal_Achievement_Pair( MEDAL_SPEED_RUN_MINE, ACHIEVEMENT_ASW_SPEEDRUN_TIMOR ),
	CASW_Medal_Achievement_Pair( MEDAL_ALL_SURVIVE_A_MISSION, ACHIEVEMENT_ASW_MISSION_NO_DEATHS )	
};

bool MedalMatchesAchievement( int nMedalIndex, int nAchievementIndex )
{
	int nPairs = NELEMS( s_MedalAchievements );
	for ( int i = 0; i < nPairs; i++ )
	{
		if ( nMedalIndex == s_MedalAchievements[i].m_nMedal && nAchievementIndex == s_MedalAchievements[i].m_nAchievement )
			return true;
	}
	return false;
}

int GetAchievementIndexForMedal( int nMedalIndex )
{
	int nPairs = NELEMS( s_MedalAchievements );
	for ( int i = 0; i < nPairs; i++ )
	{
		if ( nMedalIndex == s_MedalAchievements[i].m_nMedal )
			return s_MedalAchievements[i].m_nAchievement;
	}
	return -1;
}