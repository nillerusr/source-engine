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


#ifndef ASW_AI_BEHAVIOR_CHASE_ENEMY_H
#define ASW_AI_BEHAVIOR_CHASE_ENEMY_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

class CAI_ASW_ChaseEnemyBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_ChaseEnemyBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_ChaseEnemyBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_CHASE_ENEMY; }
	static	const char			*GetClassName() { return "behavior_chase_enemy"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() { return "Chase Enemy"; }

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
		// Schedules.
		SCHED_CHASE_ENEMY_RUN_PRIMARY = BaseClass::NEXT_SCHEDULE,	
		SCHED_CHASE_ENEMY_WALK_PRIMARY,
		SCHED_CHASE_ENEMY_PRIMARY_FAILED,
		SCHED_CHASE_ENEMY_PRIMARY_STANDOFF,
		SCHED_CHASE_ENEMY_LURCH_FORWARD_PRIMARY,
		SCHED_CHASE_ENEMY_LURCH_STRAFE_PRIMARY,
		NEXT_SCHEDULE,

		// Tasks.
		TASK_LURCH_STRAFE = BaseClass::NEXT_TASK,
		TASK_LURCH_FORWARD,
		NEXT_TASK,

		// Conditions.
		COND_CHASE_ENEMY_LURCH_FORWARD_PRIMARY = BaseClass::NEXT_CONDITION,
		COND_CHASE_ENEMY_LURCH_STRAFE_PRIMARY,
		COND_CHASE_ENEMY_LURCH_FORWARD_OR_STRAFE_PRIMARY,
		NEXT_CONDITION,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );

private:

	int HandleLurch();
	void OnStartTaskLurchForward();
	void OnStartTaskLurchStrafe();

	bool m_bWalk;				// Are we walking or running!
	float m_flChaseDistance;
	float m_flLurchForwardDistance;
	float m_flLurchStrafeDistance;

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_CHASE_ENEMY_H


