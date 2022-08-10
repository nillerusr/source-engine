//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================


#include "cbase.h"

#ifdef CLIENT_DLL

#include "achievementmgr.h"
#include "baseachievement.h"
#include "tf_hud_statpanel.h"
#include "c_tf_team.h"
#include "c_tf_player.h"

CAchievementMgr g_AchievementMgrTF;	// global achievement mgr for TF

bool CheckWinNoEnemyCaps( IGameEvent *event, int iRole );

// Grace period that we allow a player to start after level init and still consider them to be participating for the full round.  This is fairly generous
// because it can in some cases take a client several minutes to connect with respect to when the server considers the game underway
#define TF_FULL_ROUND_GRACE_PERIOD	( 4 * 60.0f )

bool IsLocalTFPlayerClass( int iClass );

// helper class for achievements that check that the player was playing on a game team for the full round
class CTFAchievementFullRound : public CBaseAchievement
{
	DECLARE_CLASS( CTFAchievementFullRound, CBaseAchievement );
public:
	void Init() 
	{
		m_iFlags |= ACH_FILTER_FULL_ROUND_ONLY;		
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "teamplay_round_win" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
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

	virtual void Event_OnRoundComplete( float flRoundTime, IGameEvent *event ) = 0 ;

};

class CAchievementTFPlayGameEveryClass : public CTFAchievementFullRound
{
	DECLARE_CLASS( CAchievementTFPlayGameEveryClass, CTFAchievementFullRound );
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL | ACH_HAS_COMPONENTS | ACH_FILTER_FULL_ROUND_ONLY );
		SetGoal( TF_LAST_NORMAL_CLASS - TF_FIRST_NORMAL_CLASS + 1 );
		BaseClass::Init();
	}

	virtual void Event_OnRoundComplete( float flRoundTime, IGameEvent *event )
	{
		float flLastClassChangeTime = m_pAchievementMgr->GetLastClassChangeTime();
		if ( flLastClassChangeTime > 0 ) 
		{					
			// has the player been present and not changed class since the start of this round (minus a grace period)?
			if ( flLastClassChangeTime < ( gpGlobals->curtime - flRoundTime ) + TF_FULL_ROUND_GRACE_PERIOD )
			{
				C_TFPlayer *pTFPlayer = C_TFPlayer::GetLocalTFPlayer();
				if ( pTFPlayer )
				{
					int iClass = pTFPlayer->GetPlayerClass()->GetClassIndex();
					if ( iClass >= TF_FIRST_NORMAL_CLASS && iClass <= TF_LAST_NORMAL_CLASS )
					{
						// yes, the achievement is satisfied for this class, set the corresponding bit
						int iBitNumber =( iClass - TF_FIRST_NORMAL_CLASS );
						EnsureComponentBitSetAndEvaluate( iBitNumber );
					}							
				}
			}
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementTFPlayGameEveryClass, ACHIEVEMENT_TF_PLAY_GAME_EVERYCLASS, "TF_PLAY_GAME_EVERYCLASS", 5 );

class CAchievementTFPlayGameEveryMap : public CTFAchievementFullRound
{
	DECLARE_CLASS( CAchievementTFPlayGameEveryMap, CTFAchievementFullRound );
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL | ACH_HAS_COMPONENTS | ACH_FILTER_FULL_ROUND_ONLY );

		static const char *szComponents[] =
		{
			"cp_dustbowl", "cp_granary", "cp_gravelpit", "cp_well", "ctf_2fort", "tc_hydro"
		};		
		m_pszComponentNames = szComponents;
		m_iNumComponents = ARRAYSIZE( szComponents );
		SetGoal( m_iNumComponents );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "teamplay_round_win" );
	}

	virtual void Event_OnRoundComplete( float flRoundTime, IGameEvent *event )
	{
		float flTeamplayStartTime = m_pAchievementMgr->GetTeamplayStartTime();
		if ( flTeamplayStartTime > 0 ) 
		{	
			// has the player been present and on a game team since the start of this round (minus a grace period)?
			if ( flTeamplayStartTime < ( gpGlobals->curtime - flRoundTime ) + TF_FULL_ROUND_GRACE_PERIOD )
			{
				// yes, the achievement is satisfied for this map, set the corresponding bit
				OnComponentEvent( m_pAchievementMgr->GetMapName() );
			}
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementTFPlayGameEveryMap, ACHIEVEMENT_TF_PLAY_GAME_EVERYMAP, "TF_PLAY_GAME_EVERYMAP", 5 );

class CAchievementTFGetHealPoints : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 25000 );		
	}

	void OnPlayerStatsUpdate()
	{
		ClassStats_t &classStats = CTFStatPanel::GetClassStats( TF_CLASS_MEDIC );
		int iOldCount = m_iCount;
		m_iCount = classStats.accumulated.m_iStat[TFSTAT_HEALING];
		if ( m_iCount != iOldCount )
		{
			m_pAchievementMgr->SetDirty( true );
		}

		if ( IsLocalTFPlayerClass( TF_CLASS_MEDIC ) )
		{
			EvaluateNewAchievement();
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementTFGetHealPoints, ACHIEVEMENT_TF_GET_HEALPOINTS, "TF_GET_HEALPOINTS", 5 );

class CAchievementTFBurnPlayersInMinimumTime : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );
	}
	// server fires an event for this achievement, no other code within achievement necessary
};
DECLARE_ACHIEVEMENT( CAchievementTFBurnPlayersInMinimumTime, ACHIEVEMENT_TF_BURN_PLAYERSINMINIMIMTIME, "TF_BURN_PLAYERSINMINIMUMTIME", 5 );

class CAchievementTFGetTurretKills : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );		
	}
	// server fires an event for this achievement, no other code within achievement necessary
};
DECLARE_ACHIEVEMENT( CAchievementTFGetTurretKills, ACHIEVEMENT_TF_GET_TURRETKILLS, "TF_GET_TURRETKILLS", 5 );

class CAchievementTFGetHeadshots: public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_GLOBAL );
		SetGoal( 25 );		
	}

	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event ) 
	{
		// was this a headshot by this player?
		if ( ( pAttacker == C_TFPlayer::GetLocalTFPlayer() ) && ( event->GetInt( "customkill" ) == TF_DMG_CUSTOM_HEADSHOT ) )
		{
			// Increment count.  Count will also get slammed whenever we get a stats update from server.  They should generally agree,
			// but server is authoritative.
			IncrementCount();
		}
	}

	void OnPlayerStatsUpdate()
	{
		// when stats are updated by server, use most recent stat value
		ClassStats_t &classStats = CTFStatPanel::GetClassStats( TF_CLASS_SNIPER );
		int iOldCount = m_iCount;
		m_iCount = classStats.accumulated.m_iStat[TFSTAT_HEADSHOTS];
		if ( m_iCount != iOldCount )
		{
			m_pAchievementMgr->SetDirty( true );
		}

		if ( IsLocalTFPlayerClass( TF_CLASS_SNIPER ) )
		{
			EvaluateNewAchievement();
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementTFGetHeadshots, ACHIEVEMENT_TF_GET_HEADSHOTS, "TF_GET_HEADSHOTS", 5 );

class CAchievementTFKillNemesis : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_LISTEN_KILL_EVENTS | ACH_SAVE_GLOBAL );
		SetGoal( 5 );
	}

	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event ) 
	{		
		if ( ( ( event->GetInt( "revenge" ) > 0 ) ) && ( pAttacker == C_TFPlayer::GetLocalTFPlayer() ) )
		{
			// local player got revenge as primary killer
			IncrementCount();
		}
		else if ( event->GetInt( "assister_revenge" ) > 0 )
		{
			int iAssisterIndex = engine->GetPlayerForUserID( event->GetInt( "assister" ) );
			if ( iAssisterIndex > 0 )
			{
				CBaseEntity *pAssister = UTIL_PlayerByIndex( iAssisterIndex );
				if ( pAssister && ( pAssister == C_TFPlayer::GetLocalTFPlayer() ) )
				{
					// local player got revenge as assister
					IncrementCount();
				}
			}				
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementTFKillNemesis, ACHIEVEMENT_TF_KILL_NEMESIS, "TF_KILL_NEMESIS", 5 );

class CAchievementTFGetConsecutiveKillsNoDeaths : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_LISTEN_KILL_EVENTS | ACH_SAVE_GLOBAL );
		SetGoal( 1 );
		m_iConsecutiveKills = 0;
	}

	virtual void Event_EntityKilled( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event ) 
	{
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pLocalPlayer == pVictim )
		{
			m_iConsecutiveKills = 0;
		}
		else if ( pLocalPlayer == pAttacker )
		{
			m_iConsecutiveKills++;
			if ( 5 == m_iConsecutiveKills )
			{
				IncrementCount();
			}
		}	
	}
	int m_iConsecutiveKills;
};
DECLARE_ACHIEVEMENT( CAchievementTFGetConsecutiveKillsNoDeaths, ACHIEVEMENT_TF_GET_CONSECUTIVEKILLS_NODEATHS, "TF_GET_CONSECUTIVEKILLS_NODEATHS", 10 );

class CAchievementTFGetHealedByEnemy: public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );
	}
	// server fires an event for this achievement, no other code within achievement necessary
};
DECLARE_ACHIEVEMENT( CAchievementTFGetHealedByEnemy, ACHIEVEMENT_TF_GET_HEALED_BYENEMY, "TF_GET_HEALED_BYENEMY", 15 );

class CAchievementTFPlayGameFriendsOnly : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL | ACH_FILTER_FULL_ROUND_ONLY );
		SetGoal( 1 );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "teamplay_round_win" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( 0 == Q_strcmp( event->GetName(), "teamplay_round_win" ) )
		{
			// Are there at least 7 friends in the game?  (at least 8 players total)
			if ( CalcPlayersOnFriendsList( 7 ) )
			{
				IncrementCount();
			}
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementTFPlayGameFriendsOnly, ACHIEVEMENT_TF_PLAY_GAME_FRIENDSONLY, "TF_PLAY_GAME_FRIENDSONLY", 10 );

class CAchievementTFWinMultipleGames : public CTFAchievementFullRound
{
	DECLARE_CLASS( CAchievementTFWinMultipleGames, CTFAchievementFullRound );
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL | ACH_FILTER_FULL_ROUND_ONLY );
		SetGoal( 20 );
		BaseClass::Init();
	}

	virtual void Event_OnRoundComplete( float flRoundTime, IGameEvent *event )
	{
		C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
		if ( pLocalPlayer )
		{
			// was the player on the winning team?
			int iPlayerTeam = pLocalPlayer->GetTeamNumber();
			int iWinningTeam = event->GetInt( "team" );
			if ( ( iWinningTeam >= FIRST_GAME_TEAM ) && ( iPlayerTeam == iWinningTeam ) )
			{
				IncrementCount();
			}
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementTFWinMultipleGames, ACHIEVEMENT_TF_WIN_MULTIPLEGAMES, "TF_WIN_MULTIPLEGAMES", 10 );

class CAchievementTFGetMultipleKills : public CBaseAchievement
{
	void Init() 
	{
		// listen for player kill enemy events, base class will increment count each time that happens
		SetFlags( ACH_LISTEN_PLAYER_KILL_ENEMY_EVENTS | ACH_SAVE_GLOBAL );
		SetGoal( 1000 );
	}

	void OnPlayerStatsUpdate()
	{
		// when stats are updated by server, use most recent stat values

		int iKills = 0;
		// get sum of kills per class across all classes to get total kills
		for ( int iClass = TF_FIRST_NORMAL_CLASS; iClass <= TF_LAST_NORMAL_CLASS; iClass++ )
		{
			ClassStats_t &classStats = CTFStatPanel::GetClassStats( iClass );
			iKills += classStats.accumulated.m_iStat[TFSTAT_KILLS];
		}

		int iOldCount = m_iCount;
		m_iCount = iKills;
		if ( m_iCount != iOldCount )
		{
			m_pAchievementMgr->SetDirty( true );
		}

		EvaluateNewAchievement();
	}
};
DECLARE_ACHIEVEMENT( CAchievementTFGetMultipleKills, ACHIEVEMENT_TF_GET_MULTIPLEKILLS, "TF_GET_MULTIPLEKILLS", 15 );

class CAchievementTFWin2FortNoEnemyCaps : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );
		SetMapNameFilter( "ctf_2fort" );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "teamplay_round_win" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( 0 == Q_strcmp( event->GetName(), "teamplay_round_win" ) )
		{
			if ( event->GetBool( "was_sudden_death" ) == false )
			{
				if ( event->GetInt( "team" ) == GetLocalPlayerTeam() )
				{
					// did the enemy team get any flag captures?
					C_TFTeam *pEnemyTeam = GetGlobalTFTeam( TF_TEAM_BLUE + TF_TEAM_RED - GetLocalPlayerTeam() );
					if ( 0 == pEnemyTeam->GetFlagCaptures() )
					{										
						IncrementCount();
					}
				}
			}
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementTFWin2FortNoEnemyCaps, ACHIEVEMENT_TF_WIN_2FORT_NOENEMYCAPS, "TF_WIN_2FORT_NOENEMYCAPS", 5 );

class CAchievementTFWinWellMinimumTime : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );
		SetMapNameFilter( "cp_well" );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "teamplay_round_win" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		if ( 0 == Q_strcmp( event->GetName(), "teamplay_round_win" ) )
		{
			if ( event->GetInt( "team" ) == GetLocalPlayerTeam() )
			{
				float flRoundTime = event->GetFloat( "round_time", 0 );
				if ( flRoundTime > 0 && flRoundTime < 5 * 60 )
				{
					IncrementCount();
				}
			}
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementTFWinWellMinimumTime, ACHIEVEMENT_TF_WIN_WELL_MINIMUMTIME, "TF_WIN_WELL_MINIMUMTIME", 10 );

class CAchievementTFWinHydroNoEnemyCaps : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL | ACH_FILTER_FULL_ROUND_ONLY );
		SetGoal( 1 );
		SetMapNameFilter( "tc_hydro" );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "teamplay_round_win" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		// winning hydro with no enemy caps means there were 2 previous minirounds completed (3 total for a shutout)
		// and local player's team won the final round
		if ( ( 2 == m_pAchievementMgr->GetMiniroundsCompleted() ) && ( CheckWinNoEnemyCaps( event, TEAM_ROLE_NONE ) ) )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementTFWinHydroNoEnemyCaps, ACHIEVEMENT_TF_WIN_HYDRO_NOENEMYCAPS, "TF_WIN_HYDRO_NOENEMYCAPS", 20 );

class CAchievementTFWinDustbowlNoEnemyCaps : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL | ACH_FILTER_FULL_ROUND_ONLY );
		SetGoal( 1 );
		SetMapNameFilter( "cp_dustbowl" );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "teamplay_round_win" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		// defending dustbowl with no enemy caps means there were no previous minirounds completed (that would be an attacker capture),
		// the player was on the defending team and they won with no enemy caps
		if ( ( 0 == m_pAchievementMgr->GetMiniroundsCompleted() ) && CheckWinNoEnemyCaps( event, TEAM_ROLE_DEFENDERS ) )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementTFWinDustbowlNoEnemyCaps, ACHIEVEMENT_TF_WIN_DUSTBOWL_NOENEMYCAPS, "TF_WIN_DUSTBOWL_NOENEMYCAPS", 10 );

class CAchievementTFWinGravelPitNoEnemyCaps : public CBaseAchievement
{
	void Init() 
	{
		SetFlags( ACH_SAVE_GLOBAL );
		SetGoal( 1 );
		SetMapNameFilter( "cp_gravelpit" );
	}

	virtual void ListenForEvents()
	{
		ListenForGameEvent( "teamplay_round_win" );
	}

	void FireGameEvent_Internal( IGameEvent *event )
	{
		// Was player on defenders and won with no enemy caps?
		if ( CheckWinNoEnemyCaps( event, TEAM_ROLE_DEFENDERS ) )
		{
			IncrementCount();
		}
	}
};
DECLARE_ACHIEVEMENT( CAchievementTFWinGravelPitNoEnemyCaps, ACHIEVEMENT_TF_WIN_GRAVELPIT_NOENEMYCAPS, "TF_WIN_GRAVELPIT_NOENEMYCAPS", 30 );

//-----------------------------------------------------------------------------
// Purpose: see if a round win was a win for the local player with no enemy caps
//-----------------------------------------------------------------------------
bool CheckWinNoEnemyCaps( IGameEvent *event, int iRole )
{
	if ( 0 == Q_strcmp( event->GetName(), "teamplay_round_win" ) )
	{
		if ( event->GetInt( "team" ) == GetLocalPlayerTeam() )
		{
			int iLosingTeamCaps = event->GetInt( "losing_team_num_caps" );
			if ( 0 == iLosingTeamCaps )
			{
				C_TFTeam *pLocalTeam = GetGlobalTFTeam( GetLocalPlayerTeam() );
				if ( pLocalTeam )
				{
					int iRolePlayer = pLocalTeam->GetRole();
					if ( iRole > TEAM_ROLE_NONE && ( iRolePlayer != iRole ) )
						return false;
					return true;
				}
			}
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Helper function to determine if local player is specified class
//-----------------------------------------------------------------------------
bool IsLocalTFPlayerClass( int iClass )
{
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	return( pLocalPlayer && pLocalPlayer->IsPlayerClass( iClass ) );
}


#endif // CLIENT_DLL