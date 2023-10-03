#ifndef _INCLUDED_ASW_OBJECTIVE_COUNTDOWN_H
#define _INCLUDED_ASW_OBJECTIVE_COUNTDOWN_H
#pragma once

#include "asw_objective.h"

// An objective that counts down to zero, blowing up the level and failing the mission when it hits zero
//   can be triggered to cancel the countdown

class CBaseTrigger;

class CASW_Objective_Countdown : public CASW_Objective
{
public:
	DECLARE_CLASS( CASW_Objective_Countdown, CASW_Objective );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Objective_Countdown();
	virtual ~CASW_Objective_Countdown();
	 
	virtual void Spawn();
	virtual void Precache();
	void ExplodeLevel();
	void FailMission();

	void InputStartCountdown( inputdata_t &inputdata );
	void InputCancelCountdown( inputdata_t &inputdata );

	CNetworkVar(float, m_fCountdownFinishTime);
	float m_fCountdownLength;
	bool m_bCountdownStarted;

	COutputEvent m_OnCountdownFailed;
};

#endif /* _INCLUDED_ASW_OBJECTIVE_COUNTDOWN_H */