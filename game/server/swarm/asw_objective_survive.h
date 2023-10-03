#ifndef _INCLUDED_ASW_OBJECTIVE_SURVIVE_H
#define _INCLUDED_ASW_OBJECTIVE_SURVIVE_H
#pragma once

#include "asw_objective.h"

// An objective that fails if any marines die

class CBaseTrigger;

class CASW_Objective_Survive : public CASW_Objective
{
public:
	DECLARE_CLASS( CASW_Objective_Survive, CASW_Objective );
	DECLARE_DATADESC();

	CASW_Objective_Survive();
	virtual ~CASW_Objective_Survive();
	virtual void Spawn();
	 
	virtual void MarineKilled(CASW_Marine* pMarine);
};

#endif /* _INCLUDED_ASW_OBJECTIVE_SURVIVE_H */