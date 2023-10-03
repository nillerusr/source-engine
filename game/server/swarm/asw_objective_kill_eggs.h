#ifndef _INCLUDED_ASW_OBJECTIVE_KILL_EGGS_H
#define _INCLUDED_ASW_OBJECTIVE_KILL_EGGS_H
#pragma once

#include "asw_objective.h"

// An objective to kill a certain number of eggs

class CASW_Objective_Kill_Eggs : public CASW_Objective
{
public:
	DECLARE_CLASS( CASW_Objective_Kill_Eggs, CASW_Objective );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CASW_Objective_Kill_Eggs();
	virtual ~CASW_Objective_Kill_Eggs();

	virtual void EggKilled(CASW_Egg* pEgg);
	virtual float GetObjectiveProgress();
	 	
	CNetworkVar(int, m_iTargetKills);
	CNetworkVar(int, m_iCurrentKills);	
};

#endif /* _INCLUDED_ASW_OBJECTIVE_KILL_EGGS_H */