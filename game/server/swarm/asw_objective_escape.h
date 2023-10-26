#ifndef _INCLUDED_ASW_OBJECTIVE_ESCAPE_H
#define _INCLUDED_ASW_OBJECTIVE_ESCAPE_H
#pragma once

#include "asw_objective.h"

// An objective that completes when all other non-optional objectives
//   are completed and all live marines are in the exit area

class CBaseTrigger;

class CASW_Objective_Escape : public CASW_Objective
{
public:
	DECLARE_CLASS( CASW_Objective_Escape, CASW_Objective );
	DECLARE_DATADESC();

	CASW_Objective_Escape();
	virtual ~CASW_Objective_Escape();
	 
	void CheckEscapeStatus();
	bool OtherObjectivesComplete();
	bool AllLiveMarinesInExit();
	void InputMarineInEscapeArea( inputdata_t &inputdata );

	CBaseTrigger* GetTrigger();
	EHANDLE m_hTrigger;
};

extern CUtlVector<CASW_Objective_Escape*> g_aEscapeObjectives;

#endif /* _INCLUDED_ASW_OBJECTIVE_ESCAPE_H */