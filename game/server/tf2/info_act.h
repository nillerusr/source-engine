//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef INFO_ACT_H
#define INFO_ACT_H
#ifdef _WIN32
#pragma once
#endif

class CBaseTFPlayer;

//-----------------------------------------------------------------------------
// Purpose: Map entity that defines an act
//-----------------------------------------------------------------------------
class CInfoAct : public CBaseEntity
{
	DECLARE_CLASS( CInfoAct, CBaseEntity );
public:
	CInfoAct();

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	int UpdateTransmitState();

	void Spawn( void );
	void StartAct( void );
	void UpdateClient( CBaseTFPlayer *pPlayer );
	void FinishAct( void );
	void ActThink( void );
	int ActNumber() const;

	void CleanupOnActStart( void );

	bool IsAWaitingAct( void );

	// Act player locking
	void StartActOverlayTime( CBaseTFPlayer *pPlayer );
	void EndActOverlayTime( CBaseTFPlayer *pPlayer );
	void ActThinkEndActOverlayTime( void );

	// Intermissions
	void StartIntermission( CBaseTFPlayer *pPlayer );
	void EndIntermission( CBaseTFPlayer *pPlayer );

	int GetMCVTimer( void ) { return m_nRespawn2Team2Time; }

private:
	enum
	{
		RESPAWN_TIMER_90_REMAINING = 0,
		RESPAWN_TIMER_60_REMAINING,
		RESPAWN_TIMER_45_REMAINING,
		RESPAWN_TIMER_30_REMAINING,
		RESPAWN_TIMER_10_REMAINING,
		RESPAWN_TIMER_0_REMAINING,

		RESPAWN_TIMER_EVENT_COUNT
	};

	void SetUpRespawnTimers();
	void ShutdownRespawnTimers();

	// Inputs
	void InputStart( inputdata_t &inputdata );
	void InputFinishWinNone( inputdata_t &inputdata );
	void InputFinishWin1( inputdata_t &inputdata );
	void InputFinishWin2( inputdata_t &inputdata );
	void InputAddTime( inputdata_t &inputdata );

	// Respawn timers
	void RespawnTimerThink();

	// Respawn delay
	void Team1RespawnDelayThink();
	void Team2RespawnDelayThink();

	// Computes the time remaining
	int ComputeTimeRemaining( int nPeriod, int nDelay );

	// Fires respawn events
	void FireRespawnEvents( int nTimeRemaining, COutputEvent *pRespawnEvents, COutputInt &respawnTime );

	// Outputs
	COutputEvent	m_OnStarted;
	COutputEvent	m_OnFinishedTeamNone;
	COutputEvent	m_OnFinishedTeam1;
	COutputEvent	m_OnFinishedTeam2;
	COutputEvent	m_OnTimerExpired;

	COutputEvent	m_Team1RespawnDelayDone;
	COutputEvent	m_Team2RespawnDelayDone;

	COutputInt		m_Respawn1Team1TimeRemaining;
	COutputInt		m_Respawn2Team1TimeRemaining;
	COutputInt		m_Respawn1Team2TimeRemaining;
	COutputInt		m_Respawn2Team2TimeRemaining;

	// A whole buncha respawn timer events
	COutputEvent	m_Respawn1Team1Events[RESPAWN_TIMER_EVENT_COUNT];
	COutputEvent	m_Respawn2Team1Events[RESPAWN_TIMER_EVENT_COUNT];
	COutputEvent	m_Respawn1Team2Events[RESPAWN_TIMER_EVENT_COUNT];
	COutputEvent	m_Respawn2Team2Events[RESPAWN_TIMER_EVENT_COUNT];
	
	// Respawn timer periods
	CNetworkVar( int, m_nRespawn1Team1Time );
	CNetworkVar( int, m_nRespawn1Team2Time );
	CNetworkVar( int, m_nRespawn2Team1Time );
	CNetworkVar( int, m_nRespawn2Team2Time );
	CNetworkVar( int, m_nRespawnTeam1Delay );
	CNetworkVar( int, m_nRespawnTeam2Delay );

	// Data
	CNetworkVar( int, m_iActNumber );
	CNetworkVar( float, m_flActTimeLimit );
	int				m_iWinners;

	// Acts
	float			m_flActStartedAt;

	// Intermissions
	string_t		m_iszIntermissionCamera;
};

inline int CInfoAct::ActNumber() const
{
	return m_iActNumber;
}

extern CHandle<CInfoAct>	g_hCurrentAct;

bool CurrentActIsAWaitingAct( void );

#endif // INFO_ACT_H
