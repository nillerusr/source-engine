#ifndef _INCLUDED_ASW_OBJECTIVE_DUMMY_H
#define _INCLUDED_ASW_OBJECTIVE_DUMMY_H
#pragma once

#include "asw_objective.h"

// A dummy objective used to show some text on the objectives screen

class CASW_Objective_Dummy : public CASW_Objective
{
public:
	DECLARE_CLASS( CASW_Objective_Dummy, CASW_Objective );
	DECLARE_DATADESC();

	CASW_Objective_Dummy();
	virtual ~CASW_Objective_Dummy();	 
};

#endif /* _INCLUDED_ASW_OBJECTIVE_DUMMY_H */