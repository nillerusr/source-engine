#ifndef _INCLUDED_ASW_OBJECTIVE_TRIGGERED_H
#define _INCLUDED_ASW_OBJECTIVE_TRIGGERED_H
#pragma once

#include "asw_objective.h"

// An objective controlled by mapper inputs

class CASW_Objective_Triggered : public CASW_Objective
{
public:
	DECLARE_CLASS( CASW_Objective_Triggered, CASW_Objective );
	DECLARE_DATADESC();

	CASW_Objective_Triggered();
	virtual ~CASW_Objective_Triggered();
	 
	// inputs
	void InputSetComplete( inputdata_t &inputdata );
	void InputSetIncomplete( inputdata_t &inputdata );
	void InputSetFailed( inputdata_t &inputdata );
};

#endif /* _INCLUDED_ASW_OBJECTIVE_TRIGGERED_H */