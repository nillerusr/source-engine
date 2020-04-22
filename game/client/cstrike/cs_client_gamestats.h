//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_STEAMSTATS_H
#define CS_STEAMSTATS_H
#ifdef _WIN32
#pragma once
#endif

#include "steam/steam_api.h"
#include "GameEventListener.h"
#include "../game/shared/cstrike/cs_gamestats_shared.h"

struct SRoundData : public BaseStatData 
{
	SRoundData( const StatsCollection_t *pRoundData ) 
	{
		nRoundTime		= (*pRoundData)[CSSTAT_PLAYTIME];
		nWasKilled		= (*pRoundData)[CSSTAT_DEATHS];
		nIsMVP			= (*pRoundData)[CSSTAT_MVPS];
		nMoneySpent		= (*pRoundData)[CSSTAT_MONEY_SPENT];
		nStartingMoney	= -1;// We'll get this data separately 
		nRoundEndReason	= Invalid_Round_End_Reason;// We'll get this data separately 
		nRevenges		= (*pRoundData)[CSSTAT_REVENGES];
		nDamageDealt	= (*pRoundData)[CSSTAT_DAMAGE];

		if ( (*pRoundData)[CSSTAT_T_ROUNDS_WON] )
		{
			nWinningTeamID = TEAM_TERRORIST;
			if ( (*pRoundData)[CSSTAT_ROUNDS_WON] )
			{
				nTeamID = TEAM_TERRORIST;
			}
			else
			{
				nTeamID = TEAM_CT;
			}
		}
		else
		{
			nWinningTeamID = TEAM_CT;
			if ( (*pRoundData)[CSSTAT_ROUNDS_WON] )
			{
				nTeamID = TEAM_CT;
			}
			else
			{
				nTeamID = TEAM_TERRORIST;
			}
		}
	}

	uint32	nRoundTime;	
	int		nTeamID;
	int		nWinningTeamID;
	int		nWasKilled;
	int		nIsMVP;
	int		nDamageDealt;
	int		nMoneySpent;
	int		nStartingMoney;
	int		nRevenges;
	int		nRoundEndReason;

	BEGIN_STAT_TABLE( "CSSRoundData" )
		REGISTER_STAT_NAMED( nRoundTime, "RoundTime" )
		REGISTER_STAT_NAMED( nTeamID, "TeamID" )
		REGISTER_STAT_NAMED( nWinningTeamID, "WinningTeamID" )
		REGISTER_STAT_NAMED( nWasKilled, "WasKilled" )
		REGISTER_STAT_NAMED( nIsMVP, "IsMvp" )
		REGISTER_STAT_NAMED( nDamageDealt, "DamageDealt" )
		REGISTER_STAT_NAMED( nMoneySpent, "MoneySpent" )
		REGISTER_STAT_NAMED( nStartingMoney, "StartingMoney" )
		REGISTER_STAT_NAMED( nRevenges, "Revenges" )
		REGISTER_STAT_NAMED( nRoundEndReason, "RoundEndReason" )
	END_STAT_TABLE()
};
typedef CUtlVector< SRoundData* > VectorRoundData;

struct PlayerStatData_t
{
	wchar_t*    pStatDisplayName;			// Localized display name of the stat
	int         iStatId;					// CSStatType_t enum value of stat
	int32       iStatValue;                 // Value of the stat
};

class CCSClientGameStats : public CAutoGameSystem, public CGameEventListener, public IGameStatTracker
{
public:
	CCSClientGameStats();
	virtual void PostInit();
	virtual void LevelShutdownPreEntity();
	virtual void Shutdown();
	virtual void LevelShutdownPreClearSteamAPIContext();

	int GetStatCount();
	PlayerStatData_t GetStatByIndex(int index);
	PlayerStatData_t GetStatById(int id);

	void		ResetAllStats( void );
	void		ResetMatchStats();
	void		UploadRoundData( void );

	const StatsCollection_t&	GetLifetimeStats() { return m_lifetimeStats; }
	const StatsCollection_t&	GetMatchStats() { return m_matchStats; }

	RoundStatsDirectAverage_t* GetDirectCTStatsAverages() { return &m_directCTStatAverages; }
	RoundStatsDirectAverage_t* GetDirectTStatsAverages() { return &m_directTStatAverages; }
	RoundStatsDirectAverage_t* GetDirectPlayerStatsAverages() { return &m_directPlayerStatAverages; }

	void		MsgFunc_MatchStatsUpdate( bf_read &msg );
	void		MsgFunc_PlayerStatsUpdate( bf_read &msg );

	// Steamworks Gamestats 
	virtual void SubmitGameStats( KeyValues *pKV )
	{
		int listCount = s_StatLists->Count();
		for( int i=0; i < listCount; ++i )
		{
			// Create a master key value that has stats everybody should share (map name, session ID, etc)
			(*s_StatLists)[i]->SendData(pKV);
			(*s_StatLists)[i]->Clear();
		}
	}

	virtual StatContainerList_t* GetStatContainerList( void )
	{
		return s_StatLists;
	}

protected:
	void FireGameEvent( IGameEvent *event );

	void UpdateSteamStats();
	void RetrieveSteamStats();
	void UpdateStats( const StatsCollection_t &stats );
	void CalculateMatchFavoriteWeapons();
	void UpdateLastMatchStats();

private:
	StatsCollection_t				m_lifetimeStats;
	StatsCollection_t				m_matchStats;
	StatsCollection_t				m_roundStats;

	RoundStatsDirectAverage_t	m_directCTStatAverages;
	RoundStatsDirectAverage_t	m_directTStatAverages;
	RoundStatsDirectAverage_t	m_directPlayerStatAverages;
	int							m_matchMaxPlayerCount;
	bool						m_bSteamStatsDownload;
	VectorRoundData				m_RoundStatData;

	int							m_RoundEndReason;

	// A static list of all the stat containers, one for each data structure being tracked
	static StatContainerList_t * s_StatLists;
};

extern CCSClientGameStats g_CSClientGameStats;

#endif //CS_STEAMSTATS_H
