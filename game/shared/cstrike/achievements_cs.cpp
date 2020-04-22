//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include <time.h>

#ifdef CLIENT_DLL

#include "achievementmgr.h"
#include "baseachievement.h"
#include "cs_achievement_constants.h"
#include "c_cs_team.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "cs_gamerules.h"
#include "achievements_cs.h"
#include "cs_gamestats_shared.h"
#include "cs_client_gamestats.h"


// [dwenger] Necessary for stats display
#include "cs_achievements_and_stats_interface.h"

// [dwenger] Necessary for sorting achievements by award time
#include <vgui/ISystem.h>
#include "../../src/public/vgui_controls/Controls.h"


ConVar achievements_easymode( "achievement_easymode", "0", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY,
						 "Enables all stat-based achievements to be earned at 10% of goals" );

// global achievement mgr for CS
CAchievementMgr g_AchievementMgrCS;		

// [dwenger] Necessary for achievement / stats panel
CSAchievementsAndStatsInterface g_AchievementsAndStatsInterfaceCS;


CCSBaseAchievement::CCSBaseAchievement()
{
}

//-----------------------------------------------------------------------------
// Purpose: Determines the timestamp when the achievement is awarded
//-----------------------------------------------------------------------------
void CCSBaseAchievement::OnAchieved()
{
// 	__time32_t unlockTime;
// 	_time32(&unlockTime);
// 	SetUnlockTime(unlockTime);
}


//-----------------------------------------------------------------------------
// Purpose: Returns the time values when the achievement was awarded.
//-----------------------------------------------------------------------------
bool CCSBaseAchievement::GetAwardTime( int& year, int& month, int& day, int& hour, int& minute, int& second )
{
	time_t t = GetUnlockTime();

	if ( t != 0 )
	{
		struct tm structuredTime;

		Plat_localtime(&t, &structuredTime);

		year = structuredTime.tm_year + 1900;
		month = structuredTime.tm_mon + 1;	// 0..11
		day = structuredTime.tm_mday;
		hour = structuredTime.tm_hour;
		minute = structuredTime.tm_min;
		second = structuredTime.tm_sec;

		return true;
	} 

	return false;
}

void CCSBaseAchievement::GetSettings( KeyValues* pNodeOut )
{
	BaseClass::GetSettings(pNodeOut);

	pNodeOut->SetInt("unlockTime", GetUnlockTime());
}

void CCSBaseAchievement::ApplySettings( /* const */ KeyValues* pNodeIn )
{
	BaseClass::ApplySettings(pNodeIn);

	SetUnlockTime(pNodeIn->GetInt("unlockTime"));
}


/**
* Meta Achievement base class methods
*/
CAchievement_Meta::CAchievement_Meta() :
	m_CallbackUserAchievement( this, &CAchievement_Meta::Steam_OnUserAchievementStored )
{
}

void CAchievement_Meta::Init()
{
	SetFlags( ACH_SAVE_GLOBAL );
	SetGoal( 1 );
}

void CAchievement_Meta::Steam_OnUserAchievementStored( UserAchievementStored_t *pUserAchievementStored )
{
	if ( IsAchieved() )
		return;

	int iAchieved = 0;

	FOR_EACH_VEC(m_requirements, i)
	{
		IAchievement* pAchievement = (IAchievement*)m_pAchievementMgr->GetAchievementByID(m_requirements[i]);
		Assert ( pAchievement );

		if ( pAchievement->IsAchieved() )
			iAchieved++;
		else
			break;
	}

	if ( iAchieved == m_requirements.Count() )
	{
		AwardAchievement();
	}
}

void CAchievement_Meta::AddRequirement( int nAchievementId )
{
	m_requirements.AddToTail(nAchievementId);
}

#if 0
bool CheckWinNoEnemyCaps( IGameEvent *event, int iRole );

// Grace period that we allow a player to start after level init and still consider them to be participating for the full round.  This is fairly generous
// because it can in some cases take a client several minutes to connect with respect to when the server considers the game underway
#define CS_FULL_ROUND_GRACE_PERIOD	( 4 * 60.0f )

bool IsLocalCSPlayerClass( int iClass );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSBaseAchievementFullRound::Init() 
{
	m_iFlags |= ACH_FILTER_FULL_ROUND_ONLY;		
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSBaseAchievementFullRound::ListenForEvents()
{
	ListenForGameEvent( "teamplay_round_win" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSBaseAchievementFullRound::FireGameEvent_Internal( IGameEvent *event )
{
	if ( 0 == Q_strcmp( event->GetName(), "teamplay_round_win" ) )
	{
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pLocalPlayer )
		{
			// is the player currently on a game team?
			int iTeam = pLocalPlayer->GetTeamNumber();
			if ( iTeam >= FIRST_GAME_TEAM ) 
			{
				float flRoundTime = event->GetFloat( "round_time", 0 );
				if ( flRoundTime > 0 )
				{
					Event_OnRoundComplete( flRoundTime, event );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSBaseAchievementFullRound::PlayerWasInEntireRound( float flRoundTime )
{
	float flTeamplayStartTime = m_pAchievementMgr->GetTeamplayStartTime();
	if ( flTeamplayStartTime > 0 ) 
	{	
		// has the player been present and on a game team since the start of this round (minus a grace period)?
		if ( flTeamplayStartTime < ( gpGlobals->curtime - flRoundTime ) + CS_FULL_ROUND_GRACE_PERIOD )
			return true;
	}
	return false;
}
#endif

//Base class for all achievements to kill x players with a given weapon
class CAchievement_StatGoal: public CCSBaseAchievement
{
public:
	void SetStatId(CSStatType_t stat)
	{
		m_StatId = stat;
	}
private:
	virtual void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );		
	}	

	void OnPlayerStatsUpdate()
	{
		// when stats are updated by server, use most recent stat value
		const StatsCollection_t& roundStats = g_CSClientGameStats.GetLifetimeStats();

		int iOldCount = GetCount();
		SetCount(roundStats[m_StatId]);
		if ( GetCount() != iOldCount )
		{
			m_pAchievementMgr->SetDirty(true);
		}

		int iGoal = GetGoal();
		if (!IsAchieved() && iGoal > 0 )
		{
			if (achievements_easymode.GetBool())
			{
				iGoal /= 10;
				if ( iGoal == 0 )
					iGoal = 1;
			}

			if ( GetCount() >= iGoal )
			{
				AwardAchievement();
			}
		}
	}
	CSStatType_t m_StatId;
};

#define DECLARE_ACHIEVEMENT_STATGOAL( achievementID, achievementName, iPointValue, iStatId, iGoal ) \
	static CBaseAchievement *Create_##achievementID( void )			\
{																		\
	CAchievement_StatGoal *pAchievement = new CAchievement_StatGoal();	\
	pAchievement->SetAchievementID( achievementID );					\
	pAchievement->SetName( achievementName );							\
	pAchievement->SetPointValue( iPointValue );							\
	pAchievement->SetHideUntilAchieved( false );						\
	pAchievement->SetStatId(iStatId);									\
	pAchievement->SetGoal( iGoal );										\
	return pAchievement;												\
};																		\
static CBaseAchievementHelper g_##achievementID##_Helper( Create_##achievementID );			

DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsLow,		    "KILL_ENEMY_LOW",			10,	CSSTAT_KILLS,					25); //25
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsMed,		    "KILL_ENEMY_MED",			10,	CSSTAT_KILLS,					500); //500
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsHigh,		    "KILL_ENEMY_HIGH",			10,	CSSTAT_KILLS,					10000); //10000

DECLARE_ACHIEVEMENT_STATGOAL(CSWinRoundsLow,			"WIN_ROUNDS_LOW",			10,	CSSTAT_ROUNDS_WON,				10); //10
DECLARE_ACHIEVEMENT_STATGOAL(CSWinRoundsMed,            "WIN_ROUNDS_MED",			10,	CSSTAT_ROUNDS_WON,				200); //200
DECLARE_ACHIEVEMENT_STATGOAL(CSWinRoundsHigh,		    "WIN_ROUNDS_HIGH",			10,	CSSTAT_ROUNDS_WON,				5000); //5000
DECLARE_ACHIEVEMENT_STATGOAL(CSWinPistolRoundsLow,	    "WIN_PISTOLROUNDS_LOW",		10,	CSSTAT_PISTOLROUNDS_WON,		5); //5
DECLARE_ACHIEVEMENT_STATGOAL(CSWinPistolRoundsMed,	    "WIN_PISTOLROUNDS_MED",		10,	CSSTAT_PISTOLROUNDS_WON,		25); //25
DECLARE_ACHIEVEMENT_STATGOAL(CSWinPistolRoundsHigh,	    "WIN_PISTOLROUNDS_HIGH",	10,	CSSTAT_PISTOLROUNDS_WON,		250); //250

DECLARE_ACHIEVEMENT_STATGOAL(CSMoneyEarnedLow,		    "EARN_MONEY_LOW",			10,	CSSTAT_MONEY_EARNED,			125000);    //125000
DECLARE_ACHIEVEMENT_STATGOAL(CSMoneyEarnedMed,		    "EARN_MONEY_MED",			10,	CSSTAT_MONEY_EARNED,			2500000);   //2500000
DECLARE_ACHIEVEMENT_STATGOAL(CSMoneyEarnedHigh,		    "EARN_MONEY_HIGH",			10,	CSSTAT_MONEY_EARNED,			50000000);  //50000000

DECLARE_ACHIEVEMENT_STATGOAL(CSGiveDamageLow,		    "GIVE_DAMAGE_LOW",			10,	CSSTAT_DAMAGE,					2500); //2500
DECLARE_ACHIEVEMENT_STATGOAL(CSGiveDamageMed,		    "GIVE_DAMAGE_MED",			10,	CSSTAT_DAMAGE,					50000); //50000
DECLARE_ACHIEVEMENT_STATGOAL(CSGiveDamageHigh,		    "GIVE_DAMAGE_HIGH",			10,	CSSTAT_DAMAGE,					1000000); //1000000

DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsDeagle,		"KILL_ENEMY_DEAGLE",		5,	CSSTAT_KILLS_DEAGLE,			200); //200
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsUSP,		    "KILL_ENEMY_USP",			5,	CSSTAT_KILLS_USP,				200); //200
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsGlock,		    "KILL_ENEMY_GLOCK",			5,	CSSTAT_KILLS_GLOCK,				200); //200
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsP228,		    "KILL_ENEMY_P228",			5,	CSSTAT_KILLS_P228,				200); //200
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsElite,		    "KILL_ENEMY_ELITE",			5,	CSSTAT_KILLS_ELITE,				100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsFiveSeven,	    "KILL_ENEMY_FIVESEVEN",		5,	CSSTAT_KILLS_FIVESEVEN,			100); //100

DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsAWP,		    "KILL_ENEMY_AWP",			5,	CSSTAT_KILLS_AWP,				1000); //1000
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsAK47,		    "KILL_ENEMY_AK47",			5,	CSSTAT_KILLS_AK47,				1000); //1000
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsM4A1,		    "KILL_ENEMY_M4A1",			5,	CSSTAT_KILLS_M4A1,				1000); //1000
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsAUG,		    "KILL_ENEMY_AUG",			5,	CSSTAT_KILLS_AUG,				500); //500
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsSG552,		    "KILL_ENEMY_SG552",         5,	CSSTAT_KILLS_SG552,				500); //500
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsSG550,		    "KILL_ENEMY_SG550",			5,	CSSTAT_KILLS_SG550,				500); //500
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsGALIL,		    "KILL_ENEMY_GALIL",			5,	CSSTAT_KILLS_GALIL,				500); //500
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsFAMAS,		    "KILL_ENEMY_FAMAS",			5,	CSSTAT_KILLS_FAMAS,				500); //500
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsScout,		    "KILL_ENEMY_SCOUT",			5,	CSSTAT_KILLS_SCOUT,				1000); //1000
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsG3SG1,		    "KILL_ENEMY_G3SG1",			5,	CSSTAT_KILLS_G3SG1,				500); //500

DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsP90,		    "KILL_ENEMY_P90",			5,	CSSTAT_KILLS_P90,				1000); //1000
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsMP5NAVY,	    "KILL_ENEMY_MP5NAVY",		5,	CSSTAT_KILLS_MP5NAVY,			1000); //1000
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsTMP,		    "KILL_ENEMY_TMP",			5,	CSSTAT_KILLS_TMP,				500); //500
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsMAC10,		    "KILL_ENEMY_MAC10",			5,	CSSTAT_KILLS_MAC10,				500); //500
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsUMP45,		    "KILL_ENEMY_UMP45",			5,	CSSTAT_KILLS_UMP45,				1000); //1000

DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsM3,			"KILL_ENEMY_M3",			5,	CSSTAT_KILLS_M3,				200); //200
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsXM1014,		"KILL_ENEMY_XM1014",		5,	CSSTAT_KILLS_XM1014,			200); //200

DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsM249,		    "KILL_ENEMY_M249",			5,	CSSTAT_KILLS_M249,				500); //500
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsKnife,		    "KILL_ENEMY_KNIFE",			5,	CSSTAT_KILLS_KNIFE,				100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSEnemyKillsHEGrenade,	    "KILL_ENEMY_HEGRENADE",		5,	CSSTAT_KILLS_HEGRENADE,			500); //500

DECLARE_ACHIEVEMENT_STATGOAL(CSHeadshots,				"HEADSHOTS",				5,	CSSTAT_KILLS_HEADSHOT,			250); //250
DECLARE_ACHIEVEMENT_STATGOAL(CSKillsEnemyWeapon,		"KILLS_ENEMY_WEAPON",		5,	CSSTAT_KILLS_ENEMY_WEAPON,		100); //100

DECLARE_ACHIEVEMENT_STATGOAL(CSKillEnemyBlinded,		"KILL_ENEMY_BLINDED",		5,	CSSTAT_KILLS_ENEMY_BLINDED,		25); //25

DECLARE_ACHIEVEMENT_STATGOAL(CSDefuseBombsLow,          "BOMB_DEFUSE_LOW",			5,	CSSTAT_NUM_BOMBS_DEFUSED,		100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSPlantBombsLow,		    "BOMB_PLANT_LOW",			5,	CSSTAT_NUM_BOMBS_PLANTED,		100); //100

DECLARE_ACHIEVEMENT_STATGOAL(CSRescueHostagesLow,	    "RESCUE_HOSTAGES_LOW",		5,	CSSTAT_NUM_HOSTAGES_RESCUED,	100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSRescueHostagesMid,	    "RESCUE_HOSTAGES_MED",		5,	CSSTAT_NUM_HOSTAGES_RESCUED,	500); //500

DECLARE_ACHIEVEMENT_STATGOAL(CSWinKnifeFightsLow,       "WIN_KNIFE_FIGHTS_LOW",		5,  CSSTAT_KILLS_KNIFE_FIGHT,       1); //1
DECLARE_ACHIEVEMENT_STATGOAL(CSWinKnifeFightsHigh,      "WIN_KNIFE_FIGHTS_HIGH",	5,  CSSTAT_KILLS_KNIFE_FIGHT,       100); //100

DECLARE_ACHIEVEMENT_STATGOAL(CSDecalSprays,             "DECAL_SPRAYS",				5,	CSSTAT_DECAL_SPRAYS,            100); //100

DECLARE_ACHIEVEMENT_STATGOAL(CSNightvisionDamage,       "NIGHTVISION_DAMAGE",       5,	CSSTAT_NIGHTVISION_DAMAGE,      5000); //5000

DECLARE_ACHIEVEMENT_STATGOAL(CSKillSnipers,             "KILL_SNIPERS",				5,	CSSTAT_KILLS_AGAINST_ZOOMED_SNIPER, 100); //100

DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapCS_ASSAULT,        "WIN_MAP_CS_ASSAULT",       5,  CSSTAT_MAP_WINS_CS_ASSAULT,     100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapCS_COMPOUND,       "WIN_MAP_CS_COMPOUND",      5,  CSSTAT_MAP_WINS_CS_COMPOUND,    100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapCS_HAVANA,         "WIN_MAP_CS_HAVANA",        5,  CSSTAT_MAP_WINS_CS_HAVANA,      100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapCS_ITALY,          "WIN_MAP_CS_ITALY",         5,  CSSTAT_MAP_WINS_CS_ITALY,       100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapCS_MILITIA,        "WIN_MAP_CS_MILITIA",       5,  CSSTAT_MAP_WINS_CS_MILITIA,     100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapCS_OFFICE,         "WIN_MAP_CS_OFFICE",        5,  CSSTAT_MAP_WINS_CS_OFFICE,      100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_AZTEC,          "WIN_MAP_DE_AZTEC",         5,  CSSTAT_MAP_WINS_DE_AZTEC,       100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_CBBLE,          "WIN_MAP_DE_CBBLE",         5,  CSSTAT_MAP_WINS_DE_CBBLE,       100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_CHATEAU,        "WIN_MAP_DE_CHATEAU",       5,  CSSTAT_MAP_WINS_DE_CHATEAU,     100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_DUST2,          "WIN_MAP_DE_DUST2",         5,  CSSTAT_MAP_WINS_DE_DUST2,       100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_DUST,           "WIN_MAP_DE_DUST",          5,  CSSTAT_MAP_WINS_DE_DUST,        100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_INFERNO,        "WIN_MAP_DE_INFERNO",       5,  CSSTAT_MAP_WINS_DE_INFERNO,     100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_NUKE,           "WIN_MAP_DE_NUKE",          5,  CSSTAT_MAP_WINS_DE_NUKE,        100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_PIRANESI,       "WIN_MAP_DE_PIRANESI",      5,  CSSTAT_MAP_WINS_DE_PIRANESI,    100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_PORT,           "WIN_MAP_DE_PORT",          5,  CSSTAT_MAP_WINS_DE_PORT,        100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_PRODIGY,        "WIN_MAP_DE_PRODIGY",       5,  CSSTAT_MAP_WINS_DE_PRODIGY,     100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_TIDES,          "WIN_MAP_DE_TIDES",         5,  CSSTAT_MAP_WINS_DE_TIDES,       100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSWinMapDE_TRAIN,          "WIN_MAP_DE_TRAIN",         5,  CSSTAT_MAP_WINS_DE_TRAIN,       100); //100

DECLARE_ACHIEVEMENT_STATGOAL(CSDonateWeapons,           "DONATE_WEAPONS",           5,  CSSTAT_WEAPONS_DONATED,         100); //100

DECLARE_ACHIEVEMENT_STATGOAL(CSDominationsLow,          "DOMINATIONS_LOW",          5,  CSSTAT_DOMINATIONS,             1); //1
DECLARE_ACHIEVEMENT_STATGOAL(CSDominationsHigh,         "DOMINATIONS_HIGH",         5,  CSSTAT_DOMINATIONS,             10); //10
DECLARE_ACHIEVEMENT_STATGOAL(CSDominationOverkillsLow,  "DOMINATION_OVERKILLS_LOW", 5,  CSSTAT_DOMINATION_OVERKILLS,    1); //1
DECLARE_ACHIEVEMENT_STATGOAL(CSDominationOverkillsHigh, "DOMINATION_OVERKILLS_HIGH",5,  CSSTAT_DOMINATION_OVERKILLS,    100); //100
DECLARE_ACHIEVEMENT_STATGOAL(CSRevengesLow,             "REVENGES_LOW",             5,  CSSTAT_REVENGES,                1); //1
DECLARE_ACHIEVEMENT_STATGOAL(CSRevengesHigh,            "REVENGES_HIGH",            5,  CSSTAT_REVENGES,                20); //20


//-----------------------------------------------------------------------------
// Purpose: Generic server awarded achievement
//-----------------------------------------------------------------------------
class CAchievementCS_ServerAwarded : public CCSBaseAchievement
{
	virtual void Init() 
	{
		SetGoal(1);
		SetFlags( ACH_SAVE_GLOBAL );
	}

	// server fires an event for this achievement, no other code within achievement necessary
};

#define DECLARE_ACHIEVEMENT_SERVERAWARDED(achievementID, achievementName, iPointValue) \
	static CBaseAchievement *Create_##achievementID( void )			\
{																		\
	CAchievementCS_ServerAwarded *pAchievement = new CAchievementCS_ServerAwarded( );					\
	pAchievement->SetAchievementID( achievementID );					\
	pAchievement->SetName( achievementName );							\
	pAchievement->SetPointValue( iPointValue );							\
	return pAchievement;												\
};																		\
static CBaseAchievementHelper g_##achievementID##_Helper( Create_##achievementID );



// server triggered achievements
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSBombDefuseCloseCall, "BOMB_DEFUSE_CLOSE_CALL", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSDefuseAndNeededKit, "BOMB_DEFUSE_NEEDED_KIT", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKilledDefuser, "KILL_BOMB_DEFUSER", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSWinBombPlant, "WIN_BOMB_PLANT", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSWinBombDefuse, "WIN_BOMB_DEFUSE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSPlantBombWithin25Seconds, "BOMB_PLANT_IN_25_SECONDS", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSRescueAllHostagesInARound, "RESCUE_ALL_HOSTAGES", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillEnemyWithFormerGun, "KILL_WITH_OWN_GUN", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillingSpree, "KILLING_SPREE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillTwoWithOneShot, "KILL_TWO_WITH_ONE_SHOT", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillEnemyReloading, "KILL_ENEMY_RELOADING", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillsWithMultipleGuns, "KILLS_WITH_MULTIPLE_GUNS", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSPosthumousGrenadeKill, "DEAD_GRENADE_KILL", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillEnemyTeam, "KILL_ENEMY_TEAM", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSLastPlayerAlive, "LAST_PLAYER_ALIVE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillEnemyLastBullet, "KILL_ENEMY_LAST_BULLET", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillingSpreeEnder, "KILLING_SPREE_ENDER", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillEnemiesWhileBlind, "KILL_ENEMIES_WHILE_BLIND", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillEnemiesWhileBlindHard, "KILL_ENEMIES_WHILE_BLIND_HARD", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSDamageNoKill, "DAMAGE_NO_KILL", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillLowDamage, "KILL_LOW_DAMAGE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKilledRescuer, "KILL_HOSTAGE_RESCUER", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSSurviveGrenade, "SURVIVE_GRENADE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKilledDefuserWithGrenade, "KILLED_DEFUSER_WITH_GRENADE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSSurvivedHeadshotDueToHelmet, "SURVIVED_HEADSHOT_DUE_TO_HELMET", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillSniperWithSniper, "KILL_SNIPER_WITH_SNIPER", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillSniperWithKnife, "KILL_SNIPER_WITH_KNIFE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSHipShot, "HIP_SHOT", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillWhenAtLowHealth, "KILL_WHEN_AT_LOW_HEALTH", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSGrenadeMultikill, "GRENADE_MULTIKILL", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSBombMultikill, "BOMB_MULTIKILL", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSPistolRoundKnifeKill, "PISTOL_ROUND_KNIFE_KILL", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSFastRoundWin, "FAST_ROUND_WIN", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSSurviveManyAttacks, "SURVIVE_MANY_ATTACKS", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSGooseChase, "GOOSE_CHASE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSWinBombPlantAfterRecovery, "WIN_BOMB_PLANT_AFTER_RECOVERY", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSLosslessExtermination, "LOSSLESS_EXTERMINATION", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSFlawlessVictory, "FLAWLESS_VICTORY", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSWinDualDuel, "WIN_DUAL_DUEL", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSFastHostageRescue, "FAST_HOSTAGE_RESCUE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSBreakWindows, "BREAK_WINDOWS", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSBreakProps, "BREAK_PROPS", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSUnstoppableForce, "UNSTOPPABLE_FORCE", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSImmovableObject, "IMMOVABLE_OBJECT", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSHeadshotsInRound, "HEADSHOTS_IN_ROUND", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillWhileInAir, "KILL_WHILE_IN_AIR", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillEnemyInAir, "KILL_ENEMY_IN_AIR", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillerAndEnemyInAir, "KILLER_AND_ENEMY_IN_AIR", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSSilentWin, "SILENT_WIN", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSBloodlessVictory, "BLOODLESS_VICTORY", 5);
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSWinRoundsWithoutBuying, "WIN_ROUNDS_WITHOUT_BUYING", 5)
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSDefuseDefense, "DEFUSE_DEFENSE", 5)
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSKillBombPickup, "KILL_BOMB_PICKUP", 5)
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSSameUniform, "SAME_UNIFORM", 5)
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSConcurrentDominations, "CONCURRENT_DOMINATIONS", 5)
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSDominationOverkillsMatch, "DOMINATION_OVERKILLS_MATCH", 5)
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSExtendedDomination, "EXTENDED_DOMINATION", 5)
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSCauseFriendlyFireWithFlashbang, "CAUSE_FRIENDLY_FIRE_WITH_FLASHBANG", 5)
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSWinClanMatch, "WIN_CLAN_MATCH", 5)
DECLARE_ACHIEVEMENT_SERVERAWARDED(CSSnipeTwoFromSameSpot, "SNIPE_TWO_FROM_SAME_SPOT", 5 )




//-----------------------------------------------------------------------------
// Meta Achievements
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Get all the pistol achievements
//-----------------------------------------------------------------------------
class CAchievementCS_PistolMaster : public CAchievement_Meta
{
	DECLARE_CLASS( CAchievementCS_PistolMaster, CAchievement_Meta );
	virtual void Init() 
	{
		BaseClass::Init();
		AddRequirement(CSEnemyKillsDeagle);
		AddRequirement(CSEnemyKillsUSP);
		AddRequirement(CSEnemyKillsGlock);
		AddRequirement(CSEnemyKillsP228);
		AddRequirement(CSEnemyKillsElite);
		AddRequirement(CSEnemyKillsFiveSeven);
	}
};
DECLARE_ACHIEVEMENT(CAchievementCS_PistolMaster, CSMetaPistol, "META_PISTOL", 10);

//-----------------------------------------------------------------------------
// Purpose: Get all the rifle achievements
//-----------------------------------------------------------------------------
class CAchievementCS_RifleMaster : public CAchievement_Meta
{
	DECLARE_CLASS( CAchievementCS_RifleMaster, CAchievement_Meta );
	virtual void Init() 
	{
		BaseClass::Init();
		AddRequirement(CSEnemyKillsAWP);
		AddRequirement(CSEnemyKillsAK47);
		AddRequirement(CSEnemyKillsM4A1);
		AddRequirement(CSEnemyKillsAUG);
		AddRequirement(CSEnemyKillsSG552);
		AddRequirement(CSEnemyKillsSG550);
		AddRequirement(CSEnemyKillsGALIL);
		AddRequirement(CSEnemyKillsFAMAS);
		AddRequirement(CSEnemyKillsScout);
		AddRequirement(CSEnemyKillsG3SG1);
	}
};
DECLARE_ACHIEVEMENT(CAchievementCS_RifleMaster, CSMetaRifle, "META_RIFLE", 10);

//-----------------------------------------------------------------------------
// Purpose: Get all the SMG achievements
//-----------------------------------------------------------------------------
class CAchievementCS_SubMachineGunMaster : public CAchievement_Meta
{
	DECLARE_CLASS( CAchievementCS_SubMachineGunMaster, CAchievement_Meta );
	virtual void Init() 
	{
		BaseClass::Init();
		AddRequirement(CSEnemyKillsP90);
		AddRequirement(CSEnemyKillsMP5NAVY);
		AddRequirement(CSEnemyKillsTMP);
		AddRequirement(CSEnemyKillsMAC10);
		AddRequirement(CSEnemyKillsUMP45);
	}
};
DECLARE_ACHIEVEMENT(CAchievementCS_SubMachineGunMaster, CSMetaSMG, "META_SMG", 10);

//-----------------------------------------------------------------------------
// Purpose: Get all the Shotgun achievements
//-----------------------------------------------------------------------------
class CAchievementCS_ShotgunMaster : public CAchievement_Meta
{
	DECLARE_CLASS( CAchievementCS_ShotgunMaster, CAchievement_Meta );
	virtual void Init() 
	{
		BaseClass::Init();
		AddRequirement(CSEnemyKillsM3);
		AddRequirement(CSEnemyKillsXM1014);
	}
};
DECLARE_ACHIEVEMENT(CAchievementCS_ShotgunMaster, CSMetaShotgun, "META_SHOTGUN", 10);

//-----------------------------------------------------------------------------
// Purpose: Get every weapon achievement
//-----------------------------------------------------------------------------
class CAchievementCS_WeaponMaster : public CAchievement_Meta
{
	DECLARE_CLASS( CAchievementCS_WeaponMaster, CAchievement_Meta );
	virtual void Init() 
	{
		BaseClass::Init();
		AddRequirement(CSMetaPistol);	
		AddRequirement(CSMetaRifle);
		AddRequirement(CSMetaSMG);
		AddRequirement(CSMetaShotgun);
		AddRequirement(CSEnemyKillsM249);
		AddRequirement(CSEnemyKillsKnife);
		AddRequirement(CSEnemyKillsHEGrenade);
	}
};
DECLARE_ACHIEVEMENT(CAchievementCS_WeaponMaster, CSMetaWeaponMaster, "META_WEAPONMASTER", 50);



class CAchievementCS_KillWithAllWeapons : public CCSBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );
	}

	void OnPlayerStatsUpdate()
	{
		const StatsCollection_t& roundStats = g_CSClientGameStats.GetLifetimeStats();

		//Loop through all weapons we care about and make sure we got a kill with each.
		for (int i = 0; WeaponName_StatId_Table[i].killStatId != CSSTAT_UNDEFINED; ++i)
		{
			CSStatType_t statId = WeaponName_StatId_Table[i].killStatId;

			if ( roundStats[statId] == 0)
			{
				return;
			}
		}

		//If we haven't bailed yet, award the achievement.		
		IncrementCount();
	}
};
DECLARE_ACHIEVEMENT( CAchievementCS_KillWithAllWeapons, CSKillWithEveryWeapon, "KILL_WITH_EVERY_WEAPON", 5 );

class CAchievementCS_FriendsSameUniform : public CCSBaseAchievement
{
    void Init()
    {
        SetFlags(ACH_SAVE_GLOBAL);
        SetGoal(1);
    }

    void ListenForEvents()
    {
        ListenForGameEvent( "round_start" );
    }

    void FireGameEvent_Internal( IGameEvent *event )
    {
        if ( Q_strcmp( event->GetName(), "round_start" ) == 0 )
        {
            int localPlayerIndex = GetLocalPlayerIndex();
            C_CSPlayer* pLocalPlayer = ToCSPlayer(UTIL_PlayerByIndex(localPlayerIndex));

            // Initialize all to 1, since the local player doesn't get counted as we loop.
            int numPlayersOnTeam = 1;
            int numFriendsOnTeam = 1;
            int numMatchingFriendsOnTeam = 1;

            if (pLocalPlayer)
            {    
                int localPlayerClass = pLocalPlayer->PlayerClass();
                int localPlayerTeam = pLocalPlayer->GetTeamNumber();
                for ( int i = 1; i <= gpGlobals->maxClients; i++ )
                {
                    if ( i != localPlayerIndex)
                    {
                        CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( i );

                        if (pPlayer)
                        {
                            if (pPlayer->GetTeamNumber() == localPlayerTeam)
                            {
                                ++numPlayersOnTeam;
                                if (pLocalPlayer->HasPlayerAsFriend(pPlayer) )
                                {
                                    ++numFriendsOnTeam;
                                    if ( pPlayer->PlayerClass() == localPlayerClass )
                                        ++numMatchingFriendsOnTeam;
                                }
                            }
                        }
                    }
                }

                if (numMatchingFriendsOnTeam >= AchievementConsts::FriendsSameUniform_MinPlayers )
                {
                    AwardAchievement();
                }
            }
        }
    }
};
DECLARE_ACHIEVEMENT( CAchievementCS_FriendsSameUniform, CSFriendsSameUniform, "FRIENDS_SAME_UNIFORM", 5 );



class CAchievementCS_AvengeFriend : public CCSBaseAchievement
{
    void Init()
    {
        SetFlags(ACH_SAVE_GLOBAL);
        SetGoal(1);
    }

    void ListenForEvents()
    {
        ListenForGameEvent( "player_avenged_teammate" );
    }

    void FireGameEvent_Internal( IGameEvent *event )
    {
        if ( Q_strcmp( event->GetName(), "player_avenged_teammate" ) == 0 )
        {
            int localPlayerIndex = GetLocalPlayerIndex();
            C_CSPlayer* pLocalPlayer = ToCSPlayer(UTIL_PlayerByIndex(localPlayerIndex));

            //for debugging
            //int eventId = event->GetInt( "avenger_id" );
            //int localUserId = pLocalPlayer->GetUserID();

            if (pLocalPlayer && pLocalPlayer->GetUserID() == event->GetInt( "avenger_id" ))
            {
                int avengedPlayerIndex = engine->GetPlayerForUserID( event->GetInt( "avenged_player_id" ) );

                if ( avengedPlayerIndex > 0 )
                {
                    C_CSPlayer* pAvengedPlayer = ToCSPlayer(UTIL_PlayerByIndex(avengedPlayerIndex));                        
                    if (pAvengedPlayer && pLocalPlayer->HasPlayerAsFriend(pAvengedPlayer))
                    {
                        AwardAchievement();
                    }
                }             
            }
        }
    }
};
DECLARE_ACHIEVEMENT( CAchievementCS_AvengeFriend, CSAvengeFriend, "AVENGE_FRIEND", 5 );



class CAchievementCS_CollectHolidayGifts : public CCSBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 3 );
		SetStoreProgressInSteam( true );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "christmas_gift_grab" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( !UTIL_IsHolidayActive( 3 /*kHoliday_Christmas*/ ) )
			return;

		if ( Q_strcmp( event->GetName(), "christmas_gift_grab" ) == 0 )
		{
			int iPlayer = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
			CBaseEntity *pPlayer = UTIL_PlayerByIndex( iPlayer );

			if ( pPlayer && pPlayer == C_CSPlayer::GetLocalCSPlayer() )
			{
				IncrementCount();
			}
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementCS_CollectHolidayGifts, CSCollectHolidayGifts, "COLLECT_GIFTS", 5 );

#endif // CLIENT_DLL

