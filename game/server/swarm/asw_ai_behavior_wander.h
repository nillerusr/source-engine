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


#ifndef ASW_AI_BEHAVIOR_WANDER_H
#define ASW_AI_BEHAVIOR_WANDER_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

enum EWanderType
{
	WANDER_TYPE_NORMAL,
	WANDER_TYPE_DIRECTION,

	WANDER_TYPE_MAX
};

class CAI_ASW_WanderBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_WanderBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_WanderBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_WANDER; }
	static const char			*GetClassName() { return "behavior_wander"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() { return "Wander"; }

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
		SCHED_WANDER_MEDIUM = BaseClass::NEXT_SCHEDULE,	
		SCHED_WANDER_STANDOFF,
		SCHED_WANDER_DIRECTIONAL,
		SCHED_WANDER_FAIL,
		NEXT_SCHEDULE,

		TASK_WANDER_DIRECTION = BaseClass::NEXT_TASK,
		NEXT_TASK,

		COND_WANDER_ENEMY_TOO_CLOSE = BaseClass::NEXT_CONDITION,	
		NEXT_CONDITION,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );

private:
	EWanderType		m_WanderType;
	float			m_flMinDistance;
	float			m_flMaxDistance;
	float			m_flStrafeFactor;
	float			m_flDistanceTooCloseSq;
	float			m_flDivisions;
	bool			m_bCanStrafeLeft;
	bool			m_bCanStrafeRight;

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_WANDER_H


