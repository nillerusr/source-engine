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


#ifndef ASW_AI_BEHAVIOR_COMBATSTUN_H
#define ASW_AI_BEHAVIOR_COMBATSTUN_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

class CAI_ASW_CombatStunBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_CombatStunBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_CombatStunBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_COMBAT_STUN; }
	static	const char			*GetClassName() { return "behavior_combat_stun"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() { return "Combat Stun"; }

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
		SCHED_COMBAT_STUN = BaseClass::NEXT_SCHEDULE,
		NEXT_SCHEDULE,
	};

	enum
	{
		TASK_COMBAT_STUN = BaseClass::NEXT_TASK,
		NEXT_TASK
	};

	enum
	{
		COND_BEGIN_COMBAT_STUN = BaseClass::NEXT_CONDITION,
		NEXT_CONDITION
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );

private:
	float	m_flStunEndTime;
	float	m_flStunDuration;

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_COMBATSTUN_H


