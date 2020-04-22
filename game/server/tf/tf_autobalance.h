//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef TF_AUTOBALANCE_H
#define TF_AUTOBALANCE_H
#ifdef _WIN32
#pragma once
#endif

#include "GameEventListener.h"

enum autobalance_state_t
{
	AB_STATE_INACTIVE = 0,
	AB_STATE_MONITOR,
	AB_STATE_FIND_VOLUNTEERS
};

enum volunteer_state_t
{
	AB_VOLUNTEER_STATE_ASKED = 0,
	AB_VOLUNTEER_STATE_NO,
	AB_VOLUNTEER_STATE_YES,
};

typedef struct
{
	CHandle<CTFPlayer> hPlayer;
	volunteer_state_t  eState;
	float flQueryExpireTime;
} volunteer_info_s;

//-----------------------------------------------------------------------------
class CTFAutobalance : public CAutoGameSystemPerFrame
{
public:

	CTFAutobalance();
	~CTFAutobalance();

	virtual char const *Name() { return "CTFAutobalance"; }

	virtual void Shutdown();
	virtual void LevelShutdownPostEntity();

	// called after entities think
	virtual void FrameUpdatePostEntityThink();

	void ReplyReceived( CTFPlayer *pTFPlayer, bool bResponse );

private:
	void Reset();
	bool ShouldBeActive() const;
	bool AreTeamsUnbalanced();
	void MonitorTeams();
	bool HaveAlreadyAskedPlayer( CTFPlayer *pTFPlayer ) const;
	int GetTeamAutoBalanceScore( int nTeam ) const;
	int GetPlayerAutoBalanceScore( CTFPlayer *pTFPlayer ) const;
	CTFPlayer *FindPlayerToAsk();
	void FindVolunteers();
	void SwitchVolunteers();
	bool IsOkayToBalancePlayers();

private:
	int m_iCurrentState;
	int m_iLightestTeam;
	int m_iHeaviestTeam;
	int m_nNeeded;
	float m_flBalanceTeamsTime;

	CUtlVector< volunteer_info_s > m_vecPlayersAsked;
};

CTFAutobalance *TFAutoBalance();

#endif // TF_AUTOBALANCE_H
