//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		blah blah blah
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//


#ifndef ASW_AI_BEHAVIOR_ADVANCE_H
#define ASW_AI_BEHAVIOR_ADVANCE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

class CAI_ASW_PrepareToEngageBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_PrepareToEngageBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_PrepareToEngageBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_PREPARE_TO_ENGAGE; }
	static	const char			*GetClassName() { return "behavior_prepare_to_engage"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() { return "PrepareToEngage"; }

	virtual bool	KeyValue( const char *szKeyName, const char *szValue );

	virtual bool 	CanSelectSchedule( );

	virtual void	StartTask( const Task_t *pTask );

	virtual int		SelectSchedule( );

	enum
	{
		SCHED_PREPARE_TO_ENGAGE = BaseClass::NEXT_SCHEDULE,		
		NEXT_SCHEDULE,

		TASK_PREPARE_TO_ENGAGE = BaseClass::NEXT_TASK,
		NEXT_TASK,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	bool GetPrepareToAttackPath(const Vector &vecThreat );

private:
	float	m_flPrepareRadiusMin;
	float	m_flPrepareRadiusMax;

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_ADVANCE_H


