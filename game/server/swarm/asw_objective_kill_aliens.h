#ifndef _INCLUDED_ASW_OBJECTIVE_KILL_ALIENS_H
#define _INCLUDED_ASW_OBJECTIVE_KILL_ALIENS_H
#pragma once

#include "asw_objective.h"

// An objective to kill a certain number of eggs

class CASW_Objective_Kill_Aliens : public CASW_Objective
{
public:
	DECLARE_CLASS( CASW_Objective_Kill_Aliens, CASW_Objective );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CASW_Objective_Kill_Aliens();
	virtual ~CASW_Objective_Kill_Aliens();

	virtual void AlienKilled(CBaseEntity* pAlien);
	virtual float GetObjectiveProgress();
	 
	CNetworkVar(int, m_iTargetKills);
	CNetworkVar(int, m_iCurrentKills);
	CNetworkVar(int, m_AlienClassNum);		
};

#endif /* _INCLUDED_ASW_OBJECTIVE_KILL_ALIENS_H */