//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		NPC will take a step back when hurt.
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//


#ifndef ASW_AI_BEHAVIOR_RETREAT_H
#define ASW_AI_BEHAVIOR_RETREAT_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

class CAI_ASW_RetreatBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_RetreatBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_RetreatBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_RETREAT; }
	static	const char			*GetClassName() { return "behavior_retreat"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() { return "Retreat"; }

	virtual bool	KeyValue( const char *szKeyName, const char *szValue );
	virtual void	Precache( void );
	virtual void	Init( );

	virtual bool 	CanSelectSchedule( );
	virtual void	GatherConditions( );
	virtual void	GatherConditionsNotActive( );

	virtual void	StartTask( const Task_t *pTask );
	virtual void	RunTask( const Task_t *pTask );

	virtual int		SelectSchedule( );

	enum
	{
		SCHED_RETREAT = BaseClass::NEXT_SCHEDULE,
		NEXT_SCHEDULE,

		TASK_RETREAT = BaseClass::NEXT_TASK,

		NEXT_TASK,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );

private:
	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_RETREAT_H


