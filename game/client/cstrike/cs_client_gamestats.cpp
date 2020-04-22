//========= Copyright Valve Corporation, All rights reserved. ============//
//-------------------------------------------------------------
// File:		cs_client_gamestats.cpp
// Desc: 		Manages client side stat storage, accumulation, and access
// Author: 		Peter Freese <peter@hiddenpath.com>
// Date: 		2009/09/11
// Copyright:	© 2009 Hidden Path Entertainment
//
// Keywords: 	
//-------------------------------------------------------------

#include "cbase.h"
#include "cs_client_gamestats.h"
#include "achievementmgr.h"
#include "engine/imatchmaking.h"
#include "ipresence.h"
#include "usermessages.h"
#include "c_cs_player.h"
#include "achievements_cs.h"
#include "vgui/ILocalize.h"
#include "c_team.h"
#include "../shared/steamworks_gamestats.h"

CCSClientGameStats g_CSClientGameStats;

void MsgFunc_PlayerStatsUpdate( bf_read &msg )
{
	g_CSClientGameStats.MsgFunc_PlayerStatsUpdate(msg);
}

void MsgFunc_MatchStatsUpdate( bf_read &msg )
{
	g_CSClientGameStats.MsgFunc_MatchStatsUpdate(msg);
}

CCSClientGameStats::StatContainerList_t* CCSClientGameStats::s_StatLists = new CCSClientGameStats::StatContainerList_t();
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCSClientGameStats::CCSClientGameStats() 
{
	m_bSteamStatsDownload = false;
}

//-----------------------------------------------------------------------------
// Purpose: called at init time after all systems are init'd.  We have to
//			do this in PostInit because the Steam app ID is not available earlier
//-----------------------------------------------------------------------------
void CCSClientGameStats::PostInit()
{
	// listen for events
	ListenForGameEvent( "player_stats_updated" );
	ListenForGameEvent( "user_data_downloaded" );
	ListenForGameEvent( "round_end" );
	ListenForGameEvent( "round_start" );


	usermessages->HookMessage( "PlayerStatsUpdate", ::MsgFunc_PlayerStatsUpdate );
	usermessages->HookMessage( "MatchStatsUpdate", ::MsgFunc_MatchStatsUpdate );

	GetSteamWorksSGameStatsUploader().StartSession();

	m_RoundEndReason = Invalid_Round_End_Reason;
}

//-----------------------------------------------------------------------------
// Purpose: called at level shutdown
//-----------------------------------------------------------------------------
void CCSClientGameStats::LevelShutdownPreEntity()
{
	// This is a good opportunity to update our last match stats
	UpdateLastMatchStats();

	// upload user stats to Steam on every map change
	UpdateSteamStats();
}

//-----------------------------------------------------------------------------
// Purpose: called at app shutdown
//-----------------------------------------------------------------------------
void CCSClientGameStats::Shutdown()
{
	GetSteamWorksSGameStatsUploader().EndSession();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSClientGameStats::LevelShutdownPreClearSteamAPIContext( void )
{
	UploadRoundData();
}

//-----------------------------------------------------------------------------
// Purpose: called when the stats have changed in-game
//-----------------------------------------------------------------------------
void CCSClientGameStats::FireGameEvent( IGameEvent *event )
{
	const char *pEventName = event->GetName();
	if ( 0 == Q_strcmp( pEventName, "player_stats_updated" ) )
	{
		UpdateSteamStats();
	}
	else if ( 0 == Q_strcmp( pEventName, "user_data_downloaded" ) )
	{
		RetrieveSteamStats();
	}
	else if ( Q_strcmp( pEventName, "round_end" ) == 0 )
	{
		// Store off the reason the round ended. Used with the OGS data.
		m_RoundEndReason = event->GetInt( "reason", Invalid_Round_End_Reason );

		// update player count for last match stats
		int iCurrentPlayerCount = 0;
		if ( GetGlobalTeam(TEAM_CT) != NULL )
			iCurrentPlayerCount += GetGlobalTeam(TEAM_CT)->Get_Number_Players();
		if ( GetGlobalTeam(TEAM_TERRORIST) != NULL )
			iCurrentPlayerCount += GetGlobalTeam(TEAM_TERRORIST)->Get_Number_Players();

		m_matchMaxPlayerCount = MAX(m_matchMaxPlayerCount, iCurrentPlayerCount);
	}

	// The user stats for a round get sent piece meal, so we'll wait until a new round starts
	// before sending the previous round stats.
	else if ( Q_strcmp( pEventName, "round_start" ) == 0 && m_roundStats[CSSTAT_PLAYTIME] > 0 )
	{
		SRoundData *pRoundStatData = new SRoundData( &m_roundStats);
		C_CSPlayer *pPlayer = ToCSPlayer( C_BasePlayer::GetLocalPlayer() );
		if ( pPlayer )
		{
			// Our current money + what we spent is what we started with at the beginning of round
			pRoundStatData->nStartingMoney = pPlayer->GetAccount() + m_roundStats[CSSTAT_MONEY_SPENT];
		}
		m_RoundStatData.AddToTail( pRoundStatData );
		UploadRoundData();
		m_roundStats.Reset();
	}	
}

void CCSClientGameStats::RetrieveSteamStats()
{
	Assert( steamapicontext->SteamUserStats() );
	if ( !steamapicontext->SteamUserStats() )
		return;

	// we shouldn't be downloading stats more than once
	Assert(m_bSteamStatsDownload == false);
	if (m_bSteamStatsDownload)
		return;

	int nStatFailCount = 0;
	for ( int i = 0; i < CSSTAT_MAX; ++i )
	{
		if ( CSStatProperty_Table[i].szSteamName == NULL )
			continue;

		int iData;
		if ( steamapicontext->SteamUserStats()->GetStat( CSStatProperty_Table[i].szSteamName, &iData ) )
		{	
			m_lifetimeStats[CSStatProperty_Table[i].statId] = iData;
		}
		else
		{
			++nStatFailCount;
		}
	}

	if ( nStatFailCount > 0 )
	{
		Msg("RetrieveSteamStats: failed to get %i stats\n", nStatFailCount);
		return;
	}

	IGameEvent * event = gameeventmanager->CreateEvent( "player_stats_updated" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}

	m_bSteamStatsDownload = true;
}

//-----------------------------------------------------------------------------
// Purpose: Uploads stats for current Steam user to Steam
//-----------------------------------------------------------------------------
void CCSClientGameStats::UpdateSteamStats()
{
	// only upload if Steam is running
	if ( !steamapicontext->SteamUserStats() )
		return; 

	CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
	Assert(pAchievementMgr != NULL);
	if (!pAchievementMgr)
		return;

	// don't upload any stats if we haven't successfully download stats yet
	if ( !m_bSteamStatsDownload )
	{
		// try to periodically download stats from Steam if we haven't gotten them yet
		static float fLastStatsRetrieveTime = 0.0f;
		const float kRetrieveInterval = 30.0f;
		if ( gpGlobals->curtime > fLastStatsRetrieveTime + kRetrieveInterval )
		{
			pAchievementMgr->DownloadUserData();
			fLastStatsRetrieveTime = gpGlobals->curtime;
		}

		return;
	}

	for ( int i = 0; i < CSSTAT_MAX; ++i )
	{
		if ( CSStatProperty_Table[i].szSteamName == NULL )
			continue;

		// set the stats locally in Steam client
		steamapicontext->SteamUserStats()->SetStat( CSStatProperty_Table[i].szSteamName, m_lifetimeStats[CSStatProperty_Table[i].statId]);
	}

	// let the achievement manager know the stats have changed
	pAchievementMgr->SetDirty(true);
}

CON_COMMAND_F( stats_reset, "Resets all player stats", FCVAR_CLIENTDLL )
{
	g_CSClientGameStats.ResetAllStats();
}

CON_COMMAND_F( stats_dump, "Dumps all player stats", FCVAR_DEVELOPMENTONLY )
{
	Msg( "Accumulated stats on Steam\n");

	const StatsCollection_t& accumulatedStats = g_CSClientGameStats.GetLifetimeStats();

	for ( int i = 0; i < CSSTAT_MAX; ++i )
	{
		if ( CSStatProperty_Table[i].szSteamName == NULL )
			continue;

		Msg( "%42s: %i\n", CSStatProperty_Table[i].szSteamName, accumulatedStats[CSStatProperty_Table[i].statId]);
	}
}

#if defined(_DEBUG)
CON_COMMAND_F( stats_preload, "Load stats with data ripe for getting achievmenets", FCVAR_DEVELOPMENTONLY )
{
	struct DataSet
	{
		CSStatType_t	statId;
		int				value;
	};

	DataSet statData[] = 
	{
		{ CSSTAT_KILLS, 9999},
		{ CSSTAT_ROUNDS_WON, 4999},
		{ CSSTAT_PISTOLROUNDS_WON, 249},
		{ CSSTAT_MONEY_EARNED, 49999999},
		{ CSSTAT_DAMAGE, 999999},
		{ CSSTAT_KILLS_DEAGLE, 199},
		{ CSSTAT_KILLS_USP, 199},
		{ CSSTAT_KILLS_GLOCK, 199},
		{ CSSTAT_KILLS_P228, 199},
		{ CSSTAT_KILLS_ELITE, 99},
		{ CSSTAT_KILLS_FIVESEVEN, 99},
		{ CSSTAT_KILLS_AWP, 999},
		{ CSSTAT_KILLS_AK47, 999},
		{ CSSTAT_KILLS_M4A1, 999},
		{ CSSTAT_KILLS_AUG, 499},
		{ CSSTAT_KILLS_SG552, 499},
		{ CSSTAT_KILLS_SG550, 499},
		{ CSSTAT_KILLS_GALIL, 499},
		{ CSSTAT_KILLS_FAMAS, 499},
		{ CSSTAT_KILLS_SCOUT, 999},
		{ CSSTAT_KILLS_G3SG1, 499},
		{ CSSTAT_KILLS_P90, 999},
		{ CSSTAT_KILLS_MP5NAVY, 999},
		{ CSSTAT_KILLS_TMP, 499},
		{ CSSTAT_KILLS_MAC10, 499},
		{ CSSTAT_KILLS_UMP45, 999},
		{ CSSTAT_KILLS_M3, 199},
		{ CSSTAT_KILLS_XM1014, 199},
		{ CSSTAT_KILLS_M249, 499},
		{ CSSTAT_KILLS_KNIFE, 99},
		{ CSSTAT_KILLS_HEGRENADE, 499},
		{ CSSTAT_KILLS_HEADSHOT, 249},
		{ CSSTAT_KILLS_ENEMY_WEAPON, 99},
		{ CSSTAT_KILLS_ENEMY_BLINDED, 24},
		{ CSSTAT_NUM_BOMBS_DEFUSED, 99},
		{ CSSTAT_NUM_BOMBS_PLANTED, 99},
		{ CSSTAT_NUM_HOSTAGES_RESCUED, 499},
		{ CSSTAT_KILLS_KNIFE_FIGHT, 99},
		{ CSSTAT_DECAL_SPRAYS, 99},
		{ CSSTAT_NIGHTVISION_DAMAGE, 4999},
		{ CSSTAT_KILLS_AGAINST_ZOOMED_SNIPER, 99},
		{ CSSTAT_MAP_WINS_CS_ASSAULT, 99},
		{ CSSTAT_MAP_WINS_CS_COMPOUND, 99},
		{ CSSTAT_MAP_WINS_CS_HAVANA, 99},
		{ CSSTAT_MAP_WINS_CS_ITALY, 99},
		{ CSSTAT_MAP_WINS_CS_MILITIA, 99},
		{ CSSTAT_MAP_WINS_CS_OFFICE, 99},
		{ CSSTAT_MAP_WINS_DE_AZTEC, 99},
		{ CSSTAT_MAP_WINS_DE_CBBLE, 99},
		{ CSSTAT_MAP_WINS_DE_CHATEAU, 99},
		{ CSSTAT_MAP_WINS_DE_DUST2, 99},
		{ CSSTAT_MAP_WINS_DE_DUST, 99},
		{ CSSTAT_MAP_WINS_DE_INFERNO, 99},
		{ CSSTAT_MAP_WINS_DE_NUKE, 99},
		{ CSSTAT_MAP_WINS_DE_PIRANESI, 99},
		{ CSSTAT_MAP_WINS_DE_PORT, 99},
		{ CSSTAT_MAP_WINS_DE_PRODIGY, 99},
		{ CSSTAT_MAP_WINS_DE_TIDES, 99},
		{ CSSTAT_MAP_WINS_DE_TRAIN, 99},
		{ CSSTAT_WEAPONS_DONATED, 99},
		{ CSSTAT_DOMINATIONS, 9},
		{ CSSTAT_DOMINATION_OVERKILLS, 99},
		{ CSSTAT_REVENGES, 19},
	};

	StatsCollection_t& lifetimeStats = const_cast<StatsCollection_t&>(g_CSClientGameStats.GetLifetimeStats());

	for ( int i = 0; i < ARRAYSIZE(statData); ++i )
	{
		CSStatType_t statId = statData[i].statId;
		lifetimeStats[statId] = statData[i].value;
	}

	IGameEvent * event = gameeventmanager->CreateEvent( "player_stats_updated" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}
}
#endif

#if defined(_DEBUG)
CON_COMMAND_F( stats_corrupt, "Load stats with corrupt values", FCVAR_DEVELOPMENTONLY )
{
	struct DataSet
	{
		CSStatType_t	statId;
		int				value;
	};

	DataSet badData[] = 
	{
		{ CSSTAT_SHOTS_HIT,						0x40000089	},
		{ CSSTAT_SHOTS_FIRED,					0x400002BE	},
		{ CSSTAT_KILLS,							0x40000021	},
		{ CSSTAT_DEATHS,						0x00000056	},
		{ CSSTAT_DAMAGE,						0x00000FE3	},
		{ CSSTAT_NUM_BOMBS_PLANTED, 			0x00000004	},
		{ CSSTAT_NUM_BOMBS_DEFUSED, 			0x00000000	},
		{ CSSTAT_PLAYTIME,						0x40000F46	},
		{ CSSTAT_ROUNDS_WON,					0x40000028	},
		{ CSSTAT_ROUNDS_PLAYED,					0x40001019	},
		{ CSSTAT_PISTOLROUNDS_WON,				0x00000001	},
		{ CSSTAT_MONEY_EARNED,					0x00021E94	},
		{ CSSTAT_KILLS_DEAGLE,					0x00000009	},
		{ CSSTAT_KILLS_USP,						0x00000000	},
		{ CSSTAT_KILLS_GLOCK,					0x00000002	},
		{ CSSTAT_KILLS_P228,					0x00000000	},
		{ CSSTAT_KILLS_ELITE,					0x00000000	},
		{ CSSTAT_KILLS_FIVESEVEN,				0x00000000	},
		{ CSSTAT_KILLS_AWP,						0x00000000	},
		{ CSSTAT_KILLS_AK47,					0x00000001	},
		{ CSSTAT_KILLS_M4A1,					0x00000000	},
		{ CSSTAT_KILLS_AUG,						0x00000000	},
		{ CSSTAT_KILLS_SG552, 					0x00000000	},
		{ CSSTAT_KILLS_SG550, 					0x00000000	},
		{ CSSTAT_KILLS_GALIL, 					0x00000000	},
		{ CSSTAT_KILLS_FAMAS, 					0x00000001	},
		{ CSSTAT_KILLS_SCOUT, 					0x00000000	},
		{ CSSTAT_KILLS_G3SG1, 					0x00000000	},
		{ CSSTAT_KILLS_P90,						0x00000001	},
		{ CSSTAT_KILLS_MP5NAVY,					0x00000000	},
		{ CSSTAT_KILLS_TMP,						0x00000002	},
		{ CSSTAT_KILLS_MAC10,					0x00000000	},
		{ CSSTAT_KILLS_UMP45,					0x00000001	},
		{ CSSTAT_KILLS_M3,						0x00000000	},
		{ CSSTAT_KILLS_XM1014,					0x0000000A	},
		{ CSSTAT_KILLS_M249,					0x00000000	},
		{ CSSTAT_KILLS_KNIFE,					0x00000000	},
		{ CSSTAT_KILLS_HEGRENADE,				0x00000000	},
		{ CSSTAT_SHOTS_DEAGLE,					0x0000004C	},
		{ CSSTAT_SHOTS_USP,						0x00000001	},
		{ CSSTAT_SHOTS_GLOCK,					0x00000017	},
		{ CSSTAT_SHOTS_P228,					0x00000000	},
		{ CSSTAT_SHOTS_ELITE,					0x00000000	},
		{ CSSTAT_SHOTS_FIVESEVEN,				0x00000000	},
		{ CSSTAT_SHOTS_AWP,						0x00000000	},
		{ CSSTAT_SHOTS_AK47,					0x0000000E	},
		{ CSSTAT_SHOTS_M4A1,					0x00000000	},
		{ CSSTAT_SHOTS_AUG,						0x00000000	},
		{ CSSTAT_SHOTS_SG552,					0x00000000	},
		{ CSSTAT_SHOTS_SG550,					0x00000008	},
		{ CSSTAT_SHOTS_GALIL,					0x00000000	},
		{ CSSTAT_SHOTS_FAMAS,					0x00000010	},
		{ CSSTAT_SHOTS_SCOUT,					0x00000000	},
		{ CSSTAT_SHOTS_G3SG1,					0x00000000	},
		{ CSSTAT_SHOTS_P90,						0x0000007F	},
		{ CSSTAT_SHOTS_MP5NAVY, 				0x00000000	},
		{ CSSTAT_SHOTS_TMP,						0x00000010	},
		{ CSSTAT_SHOTS_MAC10,					0x00000000	},
		{ CSSTAT_SHOTS_UMP45,					0x00000015	},
		{ CSSTAT_SHOTS_M3,						0x00000009	},
		{ CSSTAT_SHOTS_XM1014,					0x0000024C	},
		{ CSSTAT_SHOTS_M249,					0x00000000	},
		{ CSSTAT_HITS_DEAGLE,					0x00000019	},
		{ CSSTAT_HITS_USP,						0x00000000	},
		{ CSSTAT_HITS_GLOCK,					0x0000000A	},
		{ CSSTAT_HITS_P228,						0x00000000	},
		{ CSSTAT_HITS_ELITE,					0x00000000	},
		{ CSSTAT_HITS_FIVESEVEN,				0x00000000	},
		{ CSSTAT_HITS_AWP,						0x00000000	},
		{ CSSTAT_HITS_AK47,						0x00000003	},
		{ CSSTAT_HITS_M4A1,						0x00000000	},
		{ CSSTAT_HITS_AUG,						0x00000000	},
		{ CSSTAT_HITS_SG552,					0x00000000	},
		{ CSSTAT_HITS_SG550,					0x00000001	},
		{ CSSTAT_HITS_GALIL,					0x00000000	},
		{ CSSTAT_HITS_FAMAS,					0x00000007	},
		{ CSSTAT_HITS_SCOUT,					0x00000000	},
		{ CSSTAT_HITS_G3SG1,					0x00000000	},
		{ CSSTAT_HITS_P90,						0x0000000D	},
		{ CSSTAT_HITS_MP5NAVY,					0x00000000	},
		{ CSSTAT_HITS_TMP,						0x00000006	},
		{ CSSTAT_HITS_MAC10,					0x00000000	},
		{ CSSTAT_HITS_UMP45,					0x00000006	},
		{ CSSTAT_HITS_M3,						0x00000000	},
		{ CSSTAT_HITS_XM1014,					0x0000004C	},
		{ CSSTAT_HITS_M249,						0x00000000	},
		{ CSSTAT_KILLS_HEADSHOT,				0x00000013	},
		{ CSSTAT_KILLS_ENEMY_BLINDED,			0x00000002	},
		{ CSSTAT_KILLS_ENEMY_WEAPON,			0x00000002	},
		{ CSSTAT_KILLS_KNIFE_FIGHT,				0x00000000	},
		{ CSSTAT_DECAL_SPRAYS,					0x00000000	},
		{ CSSTAT_NIGHTVISION_DAMAGE,			0x00000000	},
		{ CSSTAT_NUM_HOSTAGES_RESCUED,			0x00000000	},
		{ CSSTAT_NUM_BROKEN_WINDOWS,			0x00000000	},
		{ CSSTAT_KILLS_AGAINST_ZOOMED_SNIPER,	0x00000000	},
		{ CSSTAT_WEAPONS_DONATED,				0x00000000	},
		{ CSSTAT_DOMINATIONS,					0x00000001	},
		{ CSSTAT_DOMINATION_OVERKILLS,			0x00000000	},
		{ CSSTAT_REVENGES,						0x00000000	},
		{ CSSTAT_MVPS,							0x00000005	},
		{ CSSTAT_MAP_WINS_CS_ASSAULT,			0x00000000	},
		{ CSSTAT_MAP_WINS_CS_COMPOUND,			0x00000000	},
		{ CSSTAT_MAP_WINS_CS_HAVANA,			0x00000000	},
		{ CSSTAT_MAP_WINS_CS_ITALY,				0x40000002	},
		{ CSSTAT_MAP_WINS_CS_MILITIA,			0x00000000	},
		{ CSSTAT_MAP_WINS_CS_OFFICE,			0x00000000	},
		{ CSSTAT_MAP_WINS_DE_AZTEC,				0x0000000A	},
		{ CSSTAT_MAP_WINS_DE_CBBLE,				0x40000000	},
		{ CSSTAT_MAP_WINS_DE_CHATEAU,			0x00000000	},
		{ CSSTAT_MAP_WINS_DE_DUST2,				0x0000000B	},
		{ CSSTAT_MAP_WINS_DE_DUST,				0x00000000	},
		{ CSSTAT_MAP_WINS_DE_INFERNO,			0x00000000	},
		{ CSSTAT_MAP_WINS_DE_NUKE,				0x00000000	},
		{ CSSTAT_MAP_WINS_DE_PIRANESI,			0x00000000	},
		{ CSSTAT_MAP_WINS_DE_PORT,				0x00000000	},
		{ CSSTAT_MAP_WINS_DE_PRODIGY,			0x00000000	},
		{ CSSTAT_MAP_WINS_DE_TIDES,				0x00000000	},
		{ CSSTAT_MAP_WINS_DE_TRAIN,				0x00000000	},
		{ CSSTAT_MAP_ROUNDS_CS_ASSAULT, 		0x00000000	},
		{ CSSTAT_MAP_ROUNDS_CS_COMPOUND,		0x00000000	},
		{ CSSTAT_MAP_ROUNDS_CS_HAVANA,			0x00000000	},
		{ CSSTAT_MAP_ROUNDS_CS_ITALY,			0x00000000	},
		{ CSSTAT_MAP_ROUNDS_CS_MILITIA, 		0x00000000	},
		{ CSSTAT_MAP_ROUNDS_CS_OFFICE,			0x00000003	},
		{ CSSTAT_MAP_ROUNDS_DE_AZTEC,			0x00000019	},
		{ CSSTAT_MAP_ROUNDS_DE_CBBLE,			0x00000000	},
		{ CSSTAT_MAP_ROUNDS_DE_CHATEAU, 		0x00000000	},
		{ CSSTAT_MAP_ROUNDS_DE_DUST2,			0x00000014	},
		{ CSSTAT_MAP_ROUNDS_DE_DUST,			0x00000000	},
		{ CSSTAT_MAP_ROUNDS_DE_INFERNO, 		0x00000000	},
		{ CSSTAT_MAP_ROUNDS_DE_NUKE,			0x00000000	},
		{ CSSTAT_MAP_ROUNDS_DE_PIRANESI,		0x00000000	},
		{ CSSTAT_MAP_ROUNDS_DE_PORT,			0x00000000	},
		{ CSSTAT_MAP_ROUNDS_DE_PRODIGY, 		0x00000000	},
		{ CSSTAT_MAP_ROUNDS_DE_TIDES,			0x00000000	},
		{ CSSTAT_MAP_ROUNDS_DE_TRAIN,			0x00000000	},
		{ CSSTAT_LASTMATCH_T_ROUNDS_WON,		0x00000000	},
		{ CSSTAT_LASTMATCH_CT_ROUNDS_WON,		0x00000000	},
		{ CSSTAT_LASTMATCH_ROUNDS_WON,			0x40000000	},
		{ CSSTAT_LASTMATCH_KILLS,				0x00000000	},
		{ CSSTAT_LASTMATCH_DEATHS,				0x00000000	},
		{ CSSTAT_LASTMATCH_MVPS,				0x00000000	},
		{ CSSTAT_LASTMATCH_DAMAGE,				0x00000000	},
		{ CSSTAT_LASTMATCH_MONEYSPENT,			0x00000000	},
		{ CSSTAT_LASTMATCH_DOMINATIONS,			0x00000000	},
		{ CSSTAT_LASTMATCH_REVENGES,			0x00000000	},
		{ CSSTAT_LASTMATCH_MAX_PLAYERS,			0x0000001B	},
		{ CSSTAT_LASTMATCH_FAVWEAPON_ID,		0x00000000	},
		{ CSSTAT_LASTMATCH_FAVWEAPON_SHOTS,		0x00000000	},
		{ CSSTAT_LASTMATCH_FAVWEAPON_HITS,		0x00000000	},
		{ CSSTAT_LASTMATCH_FAVWEAPON_KILLS,		0x00000000	},
	};

	StatsCollection_t& lifetimeStats = const_cast<StatsCollection_t&>(g_CSClientGameStats.GetLifetimeStats());
	for ( int i = 0; i < ARRAYSIZE(badData); ++i )
	{
		CSStatType_t statId = badData[i].statId;
		lifetimeStats[statId] = badData[i].value;
	}

	IGameEvent * event = gameeventmanager->CreateEvent( "player_stats_updated" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}
}
#endif

int CCSClientGameStats::GetStatCount()
{
	return CSSTAT_MAX;
}

PlayerStatData_t CCSClientGameStats::GetStatByIndex( int index )
{
	PlayerStatData_t statData;

	statData.iStatId = CSStatProperty_Table[index].statId;
	statData.iStatValue = m_lifetimeStats[statData.iStatId];

	// we can make this more efficient by caching the localized names
	statData.pStatDisplayName = g_pVGuiLocalize->Find( CSStatProperty_Table[index].szLocalizationToken );

	return statData;
}

PlayerStatData_t CCSClientGameStats::GetStatById( int id )
{
	Assert(id >= 0 && id < CSSTAT_MAX);
	if ( id >= 0 && id < CSSTAT_MAX)
	{
		return GetStatByIndex(id);
	}
	else
	{
		PlayerStatData_t dummy;
		dummy.pStatDisplayName = NULL;
		dummy.iStatId = CSSTAT_UNDEFINED;
		dummy.iStatValue = 0;
		return dummy;
	}
}

void CCSClientGameStats::UpdateStats( const StatsCollection_t &stats )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	// don't count stats if cheats on, commentary mode, etc
	if ( !g_AchievementMgrCS.CheckAchievementsEnabled() )
		return;

	// Update the accumulated stats
	m_lifetimeStats.Aggregate(stats);
	m_matchStats.Aggregate(stats);
	m_roundStats.Aggregate(stats);

	IGameEvent * event = gameeventmanager->CreateEvent( "player_stats_updated" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}
}

void CCSClientGameStats::ResetAllStats( void )
{
	m_lifetimeStats.Reset();
	m_matchStats.Reset();
	m_roundStats.Reset();

	// reset the stats on Steam
	if (steamapicontext && steamapicontext->SteamUserStats())
	{
		steamapicontext->SteamUserStats()->ResetAllStats(false);
	}

	IGameEvent * event = gameeventmanager->CreateEvent( "player_stats_updated" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}
}

void CCSClientGameStats::MsgFunc_MatchStatsUpdate( bf_read &msg )
{
	int firstStat = msg.ReadShort();

	for (int iStat = firstStat; iStat < CSSTAT_MAX && msg.GetNumBytesLeft() > 0; iStat++ )
	{
		m_directTStatAverages.m_fStat[iStat] = msg.ReadFloat();
		m_directCTStatAverages.m_fStat[iStat] = msg.ReadFloat();
		m_directPlayerStatAverages.m_fStat[iStat] = msg.ReadFloat();
	}

	// sanity check: the message should contain exactly the # of bytes we expect based on the bit field
	Assert( !msg.IsOverflowed() );
	Assert( 0 == msg.GetNumBytesLeft() );
}

void CCSClientGameStats::MsgFunc_PlayerStatsUpdate( bf_read &msg )
{
	// Note: if any check fails while decoding this message, bail out and disregard this data to avoid 
	// potentially polluting player stats 

	StatsCollection_t deltaStats;

	CRC32_t crc;
	CRC32_Init( &crc );

	const uint32 key = 0x82DA9F4C;	// this key should match the key in cs_gamestats.cpp
	CRC32_ProcessBuffer( &crc, &key, sizeof(key));

	const byte version = 0x01;
	CRC32_ProcessBuffer( &crc, &version, sizeof(version));

	if (msg.ReadByte() != version)
	{
		Warning("PlayerStatsUpdate message: ignoring unsupported version\n");
		return;
	}

	byte iStatsToRead = msg.ReadByte();
	CRC32_ProcessBuffer( &crc, &iStatsToRead, sizeof(iStatsToRead));

	for ( int i = 0; i < iStatsToRead; ++i)
	{
		byte iStat = msg.ReadByte();
		CRC32_ProcessBuffer( &crc, &iStat, sizeof(iStat));

		if (iStat >= CSSTAT_MAX)
		{
			Warning("PlayerStatsUpdate: invalid statId encountered; ignoring stats update\n");
			return;
		}
		short delta = msg.ReadShort();
		deltaStats[iStat] = delta;
		CRC32_ProcessBuffer( &crc, &delta, sizeof(delta));
	}

	CRC32_Final( &crc );
	CRC32_t readCRC = msg.ReadLong();

	if (readCRC != crc || msg.IsOverflowed() || ( 0 != msg.GetNumBytesLeft() ) )
	{
		Warning("PlayerStatsUpdate message from server is corrupt; ignoring\n");
		return;
	}

	// do one additional pass for out of band values
	for ( int iStat = CSSTAT_FIRST; iStat < CSSTAT_MAX; ++iStat )
	{
		if (deltaStats[iStat] < 0 || deltaStats[iStat] >= 0x4000)
		{
			Warning("PlayerStatsUpdate message from server has out of band values; ignoring\n");
			return;
		}
	}

	// everything looks okay at this point; add these stats for the player's round, match, and lifetime stats
	UpdateStats(deltaStats);
}

void CCSClientGameStats::UploadRoundData()
{
	// If there's nothing to send, don't bother
	if ( !m_RoundStatData.Count() )
		return;

	// Temporary ConVar to disable stats
	if ( sv_noroundstats.GetBool() )
	{
		m_RoundStatData.PurgeAndDeleteElements();
		return;
	}

	// Send off all OGS stats at level shutdown
	KeyValues *pKV = new KeyValues( "basedata" );
	if ( !pKV )
		return;

	pKV->SetString( "MapID", MapName() );

	// Add all the vector based stats
	for ( int k=0 ; k < m_RoundStatData.Count() ; ++k )
	{
		m_RoundStatData[ k ] ->nRoundEndReason = m_RoundEndReason;
		SubmitStat( m_RoundStatData[ k ] );
	}

	// Perform the actual submission
	SubmitGameStats( pKV );

	// Clear out the per map stats
	m_RoundStatData.Purge();
	pKV->deleteThis();

	// Reset the last round's ending status.
	m_RoundEndReason = Invalid_Round_End_Reason;
}

void CCSClientGameStats::ResetMatchStats()
{
	m_roundStats.Reset();
	m_matchStats.Reset();
	m_matchMaxPlayerCount = 0;
}

void CCSClientGameStats::UpdateLastMatchStats()
{
	// only update that last match if we actually have valid data
	if ( m_matchStats[CSSTAT_ROUNDS_PLAYED] == 0 )
		return;

	// check to see if the player materially participate; they could have been spectating or joined just in time for the ending.
	int s = 0;
	s += m_matchStats[CSSTAT_ROUNDS_WON];
	s += m_matchStats[CSSTAT_KILLS];
	s += m_matchStats[CSSTAT_DEATHS];
	s += m_matchStats[CSSTAT_MVPS];
	s += m_matchStats[CSSTAT_DAMAGE];
	s += m_matchStats[CSSTAT_MONEY_SPENT];

	if ( s == 0 )
		return;

	m_lifetimeStats[CSSTAT_LASTMATCH_T_ROUNDS_WON]	= m_matchStats[CSSTAT_T_ROUNDS_WON];
	m_lifetimeStats[CSSTAT_LASTMATCH_CT_ROUNDS_WON]	= m_matchStats[CSSTAT_CT_ROUNDS_WON];
	m_lifetimeStats[CSSTAT_LASTMATCH_ROUNDS_WON]	= m_matchStats[CSSTAT_ROUNDS_WON];
	m_lifetimeStats[CSSTAT_LASTMATCH_KILLS]			= m_matchStats[CSSTAT_KILLS];
	m_lifetimeStats[CSSTAT_LASTMATCH_DEATHS]		= m_matchStats[CSSTAT_DEATHS];
	m_lifetimeStats[CSSTAT_LASTMATCH_MVPS]			= m_matchStats[CSSTAT_MVPS];
	m_lifetimeStats[CSSTAT_LASTMATCH_DAMAGE]		= m_matchStats[CSSTAT_DAMAGE];
	m_lifetimeStats[CSSTAT_LASTMATCH_MONEYSPENT]	= m_matchStats[CSSTAT_MONEY_SPENT];
	m_lifetimeStats[CSSTAT_LASTMATCH_DOMINATIONS]	= m_matchStats[CSSTAT_DOMINATIONS];
	m_lifetimeStats[CSSTAT_LASTMATCH_REVENGES]		= m_matchStats[CSSTAT_REVENGES];
	m_lifetimeStats[CSSTAT_LASTMATCH_MAX_PLAYERS]	= m_matchMaxPlayerCount;

	CalculateMatchFavoriteWeapons();
}

//-----------------------------------------------------------------------------
// Purpose: Calculate and store the match favorite weapon for each player as only deltaStats for that weapon are stored on Steam
//-----------------------------------------------------------------------------
void CCSClientGameStats::CalculateMatchFavoriteWeapons()
{
	int maxKills = 0, maxKillId = -1;

	for( int j = CSSTAT_KILLS_DEAGLE; j <= CSSTAT_KILLS_M249; ++j )
	{
		if ( m_matchStats[j] > maxKills )
		{
			maxKills = m_matchStats[j];
			maxKillId = j;
		}
	}
	if ( maxKillId == -1 )
	{
		m_lifetimeStats[CSSTAT_LASTMATCH_FAVWEAPON_ID] = WEAPON_NONE;
		m_lifetimeStats[CSSTAT_LASTMATCH_FAVWEAPON_SHOTS] = 0;
		m_lifetimeStats[CSSTAT_LASTMATCH_FAVWEAPON_HITS] = 0;
		m_lifetimeStats[CSSTAT_LASTMATCH_FAVWEAPON_KILLS] = 0;
	}
	else
	{
		int statTableID = -1;
		for (int j = 0; WeaponName_StatId_Table[j].killStatId != CSSTAT_UNDEFINED; ++j)
		{
			if ( WeaponName_StatId_Table[j].killStatId == maxKillId )
			{
				statTableID = j;
				break;
			}
		}
		Assert( statTableID != -1 );

		m_lifetimeStats[CSSTAT_LASTMATCH_FAVWEAPON_ID] = WeaponName_StatId_Table[statTableID].weaponId;
		m_lifetimeStats[CSSTAT_LASTMATCH_FAVWEAPON_SHOTS] = m_matchStats[WeaponName_StatId_Table[statTableID].shotStatId];
		m_lifetimeStats[CSSTAT_LASTMATCH_FAVWEAPON_HITS] = m_matchStats[WeaponName_StatId_Table[statTableID].hitStatId];
		m_lifetimeStats[CSSTAT_LASTMATCH_FAVWEAPON_KILLS] = m_matchStats[WeaponName_StatId_Table[statTableID].killStatId];
	}
}
