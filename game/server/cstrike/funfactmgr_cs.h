//========= Copyright Valve Corporation, All rights reserved. ============//
#ifndef FUNFACTMGR_H
#define FUNFACTMGR_H
#ifdef _WIN32
#pragma once
#endif

#include "GameEventListener.h"
#include "funfact_cs.h"
#include "utlmap.h"

class FunFactEvaluator;

class CCSFunFactMgr : public CAutoGameSystemPerFrame, public CGameEventListener
{
public:
	CCSFunFactMgr();
	~CCSFunFactMgr();

	virtual bool Init();
	virtual void Shutdown();
	virtual void Update( float frametime );

	bool GetRoundEndFunFact( int iWinningTeam, int iRoundResult, FunFact& funfact );

protected:
	float ScoreFunFact( const FunFact& funfact );
	void FireGameEvent( IGameEvent *event );

private:

	float m_playerCooldown[MAX_PLAYERS];	// Weights for all players.  Updated every round

	struct FunFactDatabaseEntry
	{
		const FunFactEvaluator* pEvaluator;
		int iOccurrences;
		float fCooldown;
	};
	CUtlMap<int, FunFactDatabaseEntry> m_funFactDatabase;
	int m_numRounds;
};

#endif

