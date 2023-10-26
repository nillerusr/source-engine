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


#ifndef ASW_AI_BEHAVIOR_SCUTTLE_H
#define ASW_AI_BEHAVIOR_SCUTTLE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

class CAI_ASW_ScuttleBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_ScuttleBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_ScuttleBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_SCUTTLE; }
	static const char			*GetClassName() { return "behavior_scuttle"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() { return "Scuttle"; }

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
		SCHED_SCUTTLE_MOVE_NEAR_TARGET = BaseClass::NEXT_SCHEDULE,		
		NEXT_SCHEDULE,

		TASK_SCUTTLE_SET_PATH = BaseClass::NEXT_TASK,
		TASK_SCUTTLE_WAIT,
		NEXT_TASK,

		COND_SCUTTLE_HAS_DESTINATION = BaseClass::NEXT_CONDITION,	
		COND_SCUTTLE_TARGET_CHANGED,
		NEXT_CONDITION,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );

private:
	EHANDLE			m_hTargetEnt;	// the entity that the npc is trying to go near
	float			m_flDeferUntil;
	float			m_flPackRange;
	float			m_flPackRangeSquared;
	float			m_flMinBackOff;
	float			m_flMaxBackOff;
	float			m_flMinYawVariation;
	float			m_flMaxYawVariation;
	float			m_flMinTimeWait;
	float			m_flMaxTimeWait;
	Vector			m_vDestination;

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_SCUTTLE_H


