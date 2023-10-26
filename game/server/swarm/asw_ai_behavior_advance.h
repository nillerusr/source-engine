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

class CAI_ASW_AdvanceBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_AdvanceBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_AdvanceBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_ADVANCE; }
	static	const char			*GetClassName() { return "behavior_advance"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() { return "Advance"; }

	virtual bool	KeyValue( const char *szKeyName, const char *szValue );
	virtual void	Precache( void );

	virtual bool 	CanSelectSchedule( );
	virtual void	GatherConditions( );
	virtual void	GatherConditionsNotActive( );

	virtual void	StartTask( const Task_t *pTask );
	virtual void	RunTask( const Task_t *pTask );

	virtual int		SelectSchedule( );

	enum
	{
		SCHED_ADVANCE_TOWARDS_ENEMY = BaseClass::NEXT_SCHEDULE,		
		NEXT_SCHEDULE,

		TASK_ADVANCE_TOWARDS_ENEMY = BaseClass::NEXT_TASK,
		NEXT_TASK,

		COND_ADVANCE_COMPLETED = BaseClass::NEXT_CONDITION,	
		NEXT_CONDITION,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );

private:
	bool	m_bInitialPathSet;

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_ADVANCE_H


