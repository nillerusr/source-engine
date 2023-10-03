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


#ifndef ASW_AI_BEHAVIOR_SLEEP_H
#define ASW_AI_BEHAVIOR_SLEEP_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

class CAI_ASW_SleepBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_SleepBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_SleepBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_SLEEP; }
	static	const char			*GetClassName() { return "behavior_sleep"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() {	return "Sleep"; }

	virtual bool	KeyValue( const char *szKeyName, const char *szValue );
	virtual void	Precache( void );
	virtual void	Init( );

	virtual bool 	CanSelectSchedule( );
	virtual void	GatherConditions( );
	virtual void	GatherConditionsNotActive( );

	virtual void	HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm );

	virtual void	StartTask( const Task_t *pTask );
	virtual void	RunTask( const Task_t *pTask );

	virtual int		SelectSchedule( );

	void			ClearBurrowPoint( const Vector &origin );
	void			Unburrow( void );

	enum
	{
		SCHED_SLEEP_SNORING = BaseClass::NEXT_SCHEDULE,		
		SCHED_SLEEP_UNBURROW,
		NEXT_SCHEDULE,

		TASK_SLEEP_WAIT_UNTIL_AWAKENED = BaseClass::NEXT_TASK,
		TASK_SLEEP_UNBURROW,
		NEXT_TASK,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );

private:
	bool	m_bAwakened;

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_SLEEP_H
