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


#ifndef ASW_AI_BEHAVIOR_HEAL_OTHER_H
#define ASW_AI_BEHAVIOR_HEAL_OTHER_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

class CAI_ASW_HealOtherBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_HealOtherBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_HealOtherBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_HEAL_OTHER; }
	static	const char			*GetClassName() { return "behavior_heal_other"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() { return "Heal Other"; }

	virtual bool	KeyValue( const char *szKeyName, const char *szValue );
	virtual void	Precache( void );

	virtual bool 	CanSelectSchedule( );
	virtual void	GatherConditions( );
	virtual void	GatherConditionsNotActive( );

	virtual void	StartTask( const Task_t *pTask );
	virtual void	RunTask( const Task_t *pTask );

	virtual int		SelectSchedule( );

	CBaseEntity *GetCurrentHealTarget();

	enum
	{
		SCHED_HEAL_OTHER_MOVE_TO_CANDIDATE = BaseClass::NEXT_SCHEDULE,		
		SCHED_HEAL_WAIT,
		NEXT_SCHEDULE,

		TASK_HEAL_OTHER_FIND_TARGET = BaseClass::NEXT_TASK,
		TASK_HEAL_OTHER_MOVE_TO_TARGET,
		TASK_HEAL_OTHER_DO_HEAL,
		NEXT_TASK,

		COND_HEAL_OTHER_NOT_HAS_TARGET = BaseClass::NEXT_CONDITION,	
//		COND_HEAL_OTHER_HAS_TARGET,
		COND_HEAL_OTHER_HAS_FULL_HEALTH,
		COND_HEAL_OTHER_TARGET_CHANGED,
		COND_HEAL_OTHER_TARGET_IN_RANGE,
		COND_HEAL_OTHER_NOT_TARGET_IN_RANGE,
		NEXT_CONDITION,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

	float			m_flHealAmount;

protected:
	virtual void	GatherCommonConditions( );

private:

	EHANDLE			m_hHealTargetEnt;	// the entity that the npc is trying to heal
	float			m_flHealDistance;
	float			m_flApproachDistance;
	float			m_flHealConsiderationDistance;
	float			m_flHealConsiderationDistanceSquared;	
	float			m_flDeferUntil;
	bool			m_bIsHealing;

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_HEAL_OTHER_H


