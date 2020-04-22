//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"

#ifdef CLIENT_DLL

#include "achievementmgr.h"
#include "baseachievement.h"
#include "c_dod_player.h"
#include "dod_shareddefs.h"
#include "c_dod_objective_resource.h"
#include "dod_gamerules.h"

CAchievementMgr g_AchievementMgrDOD;	// global achievement mgr for DOD

//-----------------------------------------------------------------------------
// Purpose: Query if the gamerules allows achievement progress at this time
//-----------------------------------------------------------------------------
bool GameRulesAllowsAchievements( void )
{
	return ( DODGameRules()->State_Get() == STATE_RND_RUNNING );
}

class CAchievementDODThrowBackGren : public CBaseAchievement
{
	void Init() 
	{
		// listen for player kill enemy events, base class will increment count each time that happens
		SetFlags( ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_GLOBAL );
		SetGoal( 1 );
	}

	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event ) 
	{
		if ( !GameRulesAllowsAchievements() )
			return;

		C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();

		if ( pAttacker == pLocalPlayer && pVictim->GetTeamNumber() != pAttacker->GetTeamNumber() )
		{
			// if we are allies and killed with a german grenade
			// or if we are axis and killed with a us grenade

			const char *killedwith = event->GetString( "weapon" );
			int iLocalTeam = pLocalPlayer->GetTeamNumber();

			if ( ( iLocalTeam == TEAM_ALLIES &&	( FStrEq( killedwith, "frag_ger" ) || FStrEq( killedwith, "riflegren_ger" ) ) ) || 
				 ( iLocalTeam == TEAM_AXIS && ( FStrEq( killedwith, "frag_us" ) || FStrEq( killedwith, "riflegren_us" ) ) ) )
			{
				// This kill was made with an enemy grenade.
				IncrementCount();
			}
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODThrowBackGren, ACHIEVEMENT_DOD_THROW_BACK_GREN, "DOD_THROW_BACK_GREN", 1 );


class CAchievementDODConsecutiveHeadshots : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );

		// Handled on server
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODConsecutiveHeadshots, ACHIEVEMENT_DOD_CONSECUTIVE_HEADSHOTS, "DOD_CONSECUTIVE_HEADSHOTS", 1 );


class CAchievementDODMGPositionStreak : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );

		// Handled on server
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODMGPositionStreak, ACHIEVEMENT_DOD_MG_POSITION_STREAK, "DOD_MG_POSITION_STREAK", 1 );


class CAchievementDODWinKnifeFight : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_GLOBAL );
		SetGoal( 1 );
	}

	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event ) 
	{
		if ( !GameRulesAllowsAchievements() )
			return;

		C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();

		if ( pAttacker == pLocalPlayer && pVictim->GetTeamNumber() != pAttacker->GetTeamNumber() )
		{
			const char *killedwith = event->GetString( "weapon" );

			if ( FStrEq( killedwith, "amerknife" ) || FStrEq( killedwith, "spade" ) )
			{
				C_DODPlayer *pDodVictim = ToDODPlayer( pVictim );

				if ( pDodVictim )
				{
					CWeaponDODBase *pWpn = pDodVictim->GetActiveDODWeapon();

					if ( pWpn && pWpn->GetDODWpnData().m_WeaponType == WPN_TYPE_MELEE )
					{
						// Kill was made with a melee weapon, killer had melee weapon out
						IncrementCount();
					}
				}
			}			
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODWinKnifeFight, ACHIEVEMENT_DOD_WIN_KNIFE_FIGHT, "DOD_WIN_KNIFE_FIGHT", 1 );

const char *pszOfficialMaps[] =
{
	"dod_anzio",
	"dod_avalanche",
	"dod_argentan",
	"dod_colmar",
	"dod_donner",
	"dod_flash",
	"dod_jagd",
	"dod_kalt",
	"dod_palermo"
};


class CAchievementDODCustomMaps : public CBaseAchievement
{
	// Requires a player to kill at least one player on 5 different non-official maps

	void Init() 
	{
		SetFlags( ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_GLOBAL );
		SetGoal( 5 );
		SetStoreProgressInSteam( true );

		m_bCheckedCurrentMap = false;
	}

	virtual void ListenForEvents()
	{
		// hax, this is called from LevelInitPreEntity, init per-level here
		m_bCheckedCurrentMap = false;
	}

	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event ) 
	{
		if ( m_bCheckedCurrentMap ) 
			return;

		// don't store the map name if we're not going to give achievement progress
		if ( m_pAchievementMgr->WereCheatsEverOn() )
			return;

		C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();

		if ( pAttacker == pLocalPlayer && pVictim->GetTeamNumber() != pAttacker->GetTeamNumber() )
		{
			char szMap[MAX_PATH];
			Q_FileBase( engine->GetLevelName(), szMap, ARRAYSIZE( szMap ) );

			if ( !IsOfficialMap( szMap ) && !HasPlayedThisCustomMap() )
			{
				IncrementCount();

				UTIL_IncrementMapKey( "killed_a_player" );
			}

			// stop listening
			m_bCheckedCurrentMap = true;
		}
	}

	bool HasPlayedThisCustomMap( void )
	{
		return ( UTIL_GetMapKeyCount( "killed_a_player" ) > 0 );
	}

	bool IsOfficialMap( const char *pszMapName )
	{
		bool bFound = false;
	
		for ( int i=0;i<ARRAYSIZE(pszOfficialMaps);i++ )
		{
			if ( FStrEq( pszMapName, pszOfficialMaps[i] ) )
			{
				bFound = true;
				break;
			}
		}	

		return bFound;
	}

	bool m_bCheckedCurrentMap;

};
DECLARE_ACHIEVEMENT( CAchievementDODCustomMaps, ACHIEVEMENT_DOD_PLAY_CUSTOM_MAPS, "DOD_PLAY_CUSTOM_MAPS", 1 );


class CAchievementDODKillsWithGrenade : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_GLOBAL );
		SetGoal( 1 );
	}

	virtual void ListenForEvents()
	{
		// hax, this is called from LevelInitPreEntity, init per-level here
		m_flLastKillTime = 0;
		m_iKillCount = 0;
	}

	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event ) 
	{
		if ( !GameRulesAllowsAchievements() )
			return;

		C_DODPlayer *pLocalPlayer = C_DODPlayer::GetLocalDODPlayer();

		if ( pAttacker == pLocalPlayer )
		{
			// reject non-grenade/non-rocket inflictors
			const char *killedwith = event->GetString( "weapon" );

			if ( Q_strncmp( killedwith, "frag_", 5 ) &&
				 Q_strncmp( killedwith, "riflegren_", 10 ) &&
				 !FStrEq( killedwith, "pschreck" ) &&
				 !FStrEq( killedwith, "bazooka" ) )
			{
				m_flLastKillTime = 0;
				return;
			}

			if ( ( gpGlobals->curtime - m_flLastKillTime ) > 0.25 )
			{
				m_iKillCount = 0;
			}

			m_iKillCount++;
			m_flLastKillTime = gpGlobals->curtime;

			if ( m_iKillCount == 4 )
			{
				IncrementCount();
			}
		}
	}

private:
	float m_flLastKillTime;
	int m_iKillCount;
};
DECLARE_ACHIEVEMENT( CAchievementDODKillsWithGrenade, ACHIEVEMENT_DOD_KILLS_WITH_GRENADE, "DOD_KILLS_WITH_GRENADE", 1 );


class CAchievementDODLongRangeRocket : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );

		// Handled on server
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODLongRangeRocket, ACHIEVEMENT_DOD_LONG_RANGE_ROCKET, "DOD_LONG_RANGE_ROCKET", 1 );


class CAchievementDODEndRoundKills : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_GLOBAL );
		SetGoal( 1 );

		m_iKillCount = 0;
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "player_spawn" );
	}

	// Reset the count when we spawn
	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( m_iKillCount > 0 &&  0 == Q_strcmp( event->GetName(), "player_spawn" ) && C_BasePlayer::GetLocalPlayer() )
		{
			int iUserID = event->GetInt("userid");

			if ( iUserID == C_BasePlayer::GetLocalPlayer()->GetUserID() )
			{
				m_iKillCount = 0;
			}
		}
	}

	// count kills in endround. No requirement that your team must have won the round - grenades count
	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event ) 
	{
		DODRoundState state = DODGameRules()->State_Get();

		if ( state == STATE_ALLIES_WIN || state == STATE_AXIS_WIN )
		{
			Assert( pAttacker == C_BasePlayer::GetLocalPlayer() );

			if ( pVictim->GetTeamNumber() != pAttacker->GetTeamNumber() )
			{
				m_iKillCount++;

				if ( m_iKillCount > 3 )
				{
					IncrementCount();
				}
			}
		}
	}

	int m_iKillCount;
};
DECLARE_ACHIEVEMENT( CAchievementDODEndRoundKills, ACHIEVEMENT_DOD_END_ROUND_KILLS, "DOD_END_ROUND_KILLS", 1 );


class CAchievementDODCapLastFlag : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );

		// handled on server
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODCapLastFlag, ACHIEVEMENT_DOD_CAP_LAST_FLAG, "DOD_CAP_LAST_FLAG", 1 );


class CAchievementDODUseEnemyWeapons : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );
	}

	// Handled on server
};
DECLARE_ACHIEVEMENT( CAchievementDODUseEnemyWeapons, ACHIEVEMENT_DOD_USE_ENEMY_WEAPONS, "DOD_USE_ENEMY_WEAPONS", 1 );


class CAchievementDODKillDominatingMG : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );

		// Handled on server
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODKillDominatingMG, ACHIEVEMENT_DOD_KILL_DOMINATING_MG, "DOD_KILL_DOMINATING_MG", 1 );


class CAchievementDODColmarDefense : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );

		// Handled on server
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODColmarDefense, ACHIEVEMENT_DOD_COLMAR_DEFENSE, "DOD_COLMAR_DEFENSE", 1 );


class CAchievementDODJagdOvertimeCap : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );

		// Handled on server
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODJagdOvertimeCap, ACHIEVEMENT_DOD_JAGD_OVERTIME_CAP, "DOD_JAGD_OVERTIME_CAP", 1 );


class CAchievementDODWeaponMastery : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );

		// Handled on server
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODWeaponMastery, ACHIEVEMENT_DOD_WEAPON_MASTERY, "DOD_WEAPON_MASTERY", 1 );


class CAchievementDODBlockCaptures : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "dod_capture_blocked" );
	}

	// New achievement rule - block a capture that would have lost the game for your team

	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( !GameRulesAllowsAchievements() )
			return;

		Assert( FStrEq( event->GetName(), "dod_capture_blocked" ) );

		// was a blocked defuse or plant, don't count
		if ( event->GetBool("bomb") )
			return;

		if ( !g_pObjectiveResource )
			return;

		C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

		if ( pPlayer && pPlayer->entindex() == event->GetInt("blocker") )
		{
			int iCP = event->GetInt( "cp" );

			bool bIsLastOwnedPoint = true;

			int iPlayerTeam = pPlayer->GetTeamNumber();

			for( int i=0;i<g_pObjectiveResource->GetNumControlPoints();i++ )
			{
				// assume we own the one we blocked
				if ( i == iCP )
					continue;

				// if we find any other points owned by us that aren't hidden, this wasn't the last point

				if( !g_pObjectiveResource->IsCPVisible(i) )
					continue;

				if ( g_pObjectiveResource->GetOwningTeam(i) == iPlayerTeam )
				{
					bIsLastOwnedPoint = false;
					break;
				}
			}

			if ( bIsLastOwnedPoint )
			{
				IncrementCount();
			}
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODBlockCaptures, ACHIEVEMENT_DOD_BLOCK_CAPTURES, "DOD_BLOCK_CAPTURES", 1 );


// achievements part deux

// kills as allies
// kills as axis
class CBaseAchievementKillsOnTeam : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_GLOBAL );
		SetGoal( 5000 );
		SetStoreProgressInSteam( true );
	}

	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event ) 
	{
		Assert( pAttacker == C_BasePlayer::GetLocalPlayer() );

		if ( pVictim->GetTeamNumber() != pAttacker->GetTeamNumber() )
		{
			if ( pAttacker->GetTeamNumber() == GetTeam() )
			{
				IncrementCount();
			}
		}
	}

	virtual int GetTeam( void ) = 0;
};

class CAchievementKillsAsAllies : public CBaseAchievementKillsOnTeam
{
	virtual int GetTeam( void ) { return TEAM_ALLIES; }
};
DECLARE_ACHIEVEMENT( CAchievementKillsAsAllies, ACHIEVEMENT_DOD_KILLS_AS_ALLIES, "DOD_KILLS_AS_ALLIES", 1 );

class CAchievementKillsAsAxis : public CBaseAchievementKillsOnTeam
{
	virtual int GetTeam( void ) { return TEAM_AXIS; }
};
DECLARE_ACHIEVEMENT( CAchievementKillsAsAxis, ACHIEVEMENT_DOD_KILLS_AS_AXIS, "DOD_KILLS_AS_AXIS", 1 );

// rifleman
// assault
// support
// sniper
// mg
// bazooka
class CBaseAchievementKillsAsClass : public CBaseAchievement
{
	void Init()
	{
		SetFlags( ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_GLOBAL );
		SetGoal( 1000 );
		SetStoreProgressInSteam( true );
	}

	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event ) 
	{
		Assert( pAttacker == C_BasePlayer::GetLocalPlayer() );

		if ( pVictim->GetTeamNumber() != pAttacker->GetTeamNumber() )
		{
			C_DODPlayer *pDODAttacker = ToDODPlayer( pAttacker );

			if ( pDODAttacker->m_Shared.PlayerClass() == GetClass() )
			{
				IncrementCount();
			}
		}
	}

	virtual int GetClass( void ) = 0;
};

#define DECLARE_KILLS_AS_CLASS_ACHIEVEMENT( classIndex, achievementID ) \
class CAchievement_##achievementID : public CBaseAchievementKillsAsClass	\
{	\
	virtual int GetClass( void ) { return classIndex; }	\
};	\
DECLARE_ACHIEVEMENT( CAchievement_##achievementID, ACHIEVEMENT_##achievementID, #achievementID, 1 ) \

DECLARE_KILLS_AS_CLASS_ACHIEVEMENT( 0, DOD_KILLS_AS_RIFLEMAN );
DECLARE_KILLS_AS_CLASS_ACHIEVEMENT( 1, DOD_KILLS_AS_ASSAULT );
DECLARE_KILLS_AS_CLASS_ACHIEVEMENT( 2, DOD_KILLS_AS_SUPPORT );
DECLARE_KILLS_AS_CLASS_ACHIEVEMENT( 3, DOD_KILLS_AS_SNIPER );
DECLARE_KILLS_AS_CLASS_ACHIEVEMENT( 4, DOD_KILLS_AS_MG );
DECLARE_KILLS_AS_CLASS_ACHIEVEMENT( 5, DOD_KILLS_AS_BAZOOKAGUY );

// per weapon
class CBaseAchievementKillsWithWeapon : public CBaseAchievement
{
	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event )
	{
		Assert( pAttacker == C_BasePlayer::GetLocalPlayer() );

		if ( pVictim->GetTeamNumber() != pAttacker->GetTeamNumber() && event != NULL )
		{
			if ( FStrEq( event->GetString( "weapon", "" ), GetWeaponName() ) )
			{
				IncrementCount();
			}
		}
	}

	virtual const char *GetWeaponName( void ) = 0;
};

#define DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( weaponName, goalKills, achievementID ) \
class CAchievement_##achievementID : public CBaseAchievementKillsWithWeapon	\
{	\
	void Init()		\
	{	\
		SetFlags( ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_GLOBAL );	\
		SetGoal( goalKills );	\
		SetStoreProgressInSteam( true );	\
	}	\
	\
	virtual const char *GetWeaponName( void ) { return weaponName; } \
};	\
DECLARE_ACHIEVEMENT( CAchievement_##achievementID, ACHIEVEMENT_##achievementID, #achievementID, 1 ) \

DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "garand", 500, DOD_KILLS_WITH_GARAND );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "thompson", 500, DOD_KILLS_WITH_THOMPSON );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "bar", 500, DOD_KILLS_WITH_BAR );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "spring", 500, DOD_KILLS_WITH_SPRING );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "30cal", 500, DOD_KILLS_WITH_30CAL );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "bazooka", 500, DOD_KILLS_WITH_BAZOOKA );

DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "k98", 500, DOD_KILLS_WITH_K98 );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "mp40", 500, DOD_KILLS_WITH_MP40 );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "mp44", 500, DOD_KILLS_WITH_MP44 );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "k98_scoped", 500, DOD_KILLS_WITH_K98SCOPED );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "mg42", 500, DOD_KILLS_WITH_MG42 );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "pschreck", 500, DOD_KILLS_WITH_PSCHRECK );

DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "colt", 150, DOD_KILLS_WITH_COLT );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "p38", 150, DOD_KILLS_WITH_P38 );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "c96", 150, DOD_KILLS_WITH_C96 );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "m1carbine", 150, DOD_KILLS_WITH_M1CARBINE );

DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "amerknife", 150, DOD_KILLS_WITH_AMERKNIFE );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "spade", 150, DOD_KILLS_WITH_SPADE );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "punch", 150, DOD_KILLS_WITH_PUNCH );

DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "frag_us", 250, DOD_KILLS_WITH_FRAG_US );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "frag_ger", 250, DOD_KILLS_WITH_FRAG_GER );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "riflegren_us", 250, DOD_KILLS_WITH_RIFLEGREN_US );
DECLARE_KILLS_WITH_WEAPON_ACHIEVEMENT( "riflegren_ger", 250, DOD_KILLS_WITH_RIFLEGREN_GER );


// flag captures
class CAchievementDODFlagCaptureGrind : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 100 );
		SetStoreProgressInSteam( true );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "dod_point_captured" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		Assert( FStrEq( event->GetName(), "dod_point_captured" ) );

		if ( event->GetBool( "bomb" ) == true )
			return;

		if ( !GameRulesAllowsAchievements() )
			return;

		C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();
		if ( !pPlayer )
			return;

		int iLocalPlayerIndex = pPlayer->entindex();

		const char *cappers = event->GetString("cappers");

		int len = Q_strlen(cappers);
		for( int i=0;i<len;i++ )
		{
			if ( iLocalPlayerIndex == (int)cappers[i] )
			{
				IncrementCount();
				break;
			}
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODFlagCaptureGrind, ACHIEVEMENT_DOD_CAPTURE_GRIND, "DOD_CAPTURE_GRIND", 1 );


class CAchievementDODBlockCapturesGrind : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 100 );
		SetStoreProgressInSteam( true );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "dod_capture_blocked" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( !GameRulesAllowsAchievements() )
			return;

		Assert( FStrEq( event->GetName(), "dod_capture_blocked" ) );

		if ( event->GetBool( "bomb" ) )
			return;

		C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

		if ( pPlayer && pPlayer->entindex() == event->GetInt( "blocker" ) )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODBlockCapturesGrind, ACHIEVEMENT_DOD_BLOCK_CAPTURES_GRIND, "DOD_BLOCK_CAPTURES_GRIND", 1 );


class CAchievementDODRoundsWonGrind : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 100 );
		SetStoreProgressInSteam( true );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "dod_round_win" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( !GameRulesAllowsAchievements() )
			return;

		Assert( FStrEq( event->GetName(), "dod_round_win" ) );

		C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();
		if ( !pPlayer )
			return;

		if ( event->GetInt( "team" ) == pPlayer->GetTeamNumber() )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODRoundsWonGrind, ACHIEVEMENT_DOD_ROUNDS_WON_GRIND, "DOD_ROUNDS_WON_GRIND", 1 );


class CAchievementDODBombsPlantedGrind : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 100 );
		SetStoreProgressInSteam( true );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "dod_bomb_planted" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		Assert( FStrEq( event->GetName(), "dod_bomb_planted" ) );

		C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();

		if ( pPlayer && pPlayer->GetUserID() == event->GetInt("userid" ) )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODBombsPlantedGrind, ACHIEVEMENT_DOD_BOMBS_PLANTED_GRIND, "DOD_BOMBS_PLANTED_GRIND", 1 );


class CAchievementDODBombsDefusedGrind : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 100 );
		SetStoreProgressInSteam( true );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "dod_bomb_defused" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		Assert( FStrEq( event->GetName(), "dod_bomb_defused" ) );

		if ( event->GetInt("userid") == C_BasePlayer::GetLocalPlayer()->GetUserID() )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODBombsDefusedGrind, ACHIEVEMENT_DOD_BOMBS_DEFUSED_GRIND, "DOD_BOMBS_DEFUSED_GRIND", 1 );

//----------------------------------------------------------------------------------------------------------------
class CAchievementDOD_All_Pack_1 : public CAchievement_AchievedCount
{
public:
	DECLARE_CLASS( CAchievementDOD_All_Pack_1, CAchievement_AchievedCount );
	void Init() 
	{
		BaseClass::Init();
		SetAchievementsRequired( 51, 0, 51 );
	}

	// Complete all dod achievements
};
DECLARE_ACHIEVEMENT( CAchievementDOD_All_Pack_1, ACHIEVEMENT_DOD_ALL_PACK_1, "DOD_ALL_PACK_1", 5 );


// 2011 Summer sale achievement
// Win a round on each of the winter themed maps, dod_kalt and dod_colmar
// player is not required to have been present at the start of the round
class CAchievementDODBeatTheHeat : public CBaseAchievement
{
public:
	DECLARE_CLASS( CAchievementDODBeatTheHeat, CBaseAchievement );
	void Init()
	{
		SetFlags( ACH_SAVE_GLOBAL | ACH_HAS_COMPONENTS );	
		
		static const char *szComponents[] =
		{
			"dod_kalt", "dod_colmar"
		};	
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetGoal( m_iNumComponents ); 

		BaseClass::Init();
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "dod_round_win" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( !GameRulesAllowsAchievements() )
			return;

		Assert( FStrEq( event->GetName(), "dod_round_win" ) );

		C_DODPlayer *pPlayer = C_DODPlayer::GetLocalDODPlayer();
		if ( !pPlayer )
			return;

		if ( event->GetInt( "team" ) == pPlayer->GetTeamNumber() )
		{
			OnComponentEvent( m_pAchievementMgr->GetMapName() );
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODBeatTheHeat, ACHIEVEMENT_DOD_BEAT_THE_HEAT, "DOD_BEAT_THE_HEAT", 1 );

// Winter 2011
class CAchievementDODCollectHolidayGifts : public CBaseAchievement
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
	//	if ( !UTIL_IsHolidayActive( kHoliday_Christmas ) )
	//		return;

		if ( Q_strcmp( event->GetName(), "christmas_gift_grab" ) == 0 )
		{
			int iPlayer = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
			CBaseEntity *pPlayer = UTIL_PlayerByIndex( iPlayer );

			if ( pPlayer && pPlayer == C_DODPlayer::GetLocalDODPlayer() )
			{
				IncrementCount();
			}
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementDODCollectHolidayGifts, ACHIEVEMENT_DOD_COLLECT_HOLIDAY_GIFTS, "DOD_COLLECT_HOLIDAY_GIFTS", 5 );

#endif // CLIENT_DLL