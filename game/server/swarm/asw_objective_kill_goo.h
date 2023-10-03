#ifndef _INCLUDED_ASW_OBJECTIVE_KILL_GOO_H
#define _INCLUDED_ASW_OBJECTIVE_KILL_GOO_H
#pragma once

#include "asw_objective.h"

// An objective to kill a certain number of alien goos

class CASW_Objective_Kill_Goo : public CASW_Objective
{
public:
	DECLARE_CLASS( CASW_Objective_Kill_Goo, CASW_Objective );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Objective_Kill_Goo();
	virtual ~CASW_Objective_Kill_Goo();

	virtual void GooKilled(CASW_Alien_Goo* pGoo);
	virtual float GetObjectiveProgress();
	 
	CNetworkVar(int, m_iTargetKills);
	CNetworkVar(int, m_iCurrentKills);
};

#endif /* _INCLUDED_ASW_OBJECTIVE_KILL_GOO_H */