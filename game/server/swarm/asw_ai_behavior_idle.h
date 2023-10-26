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


#ifndef ASW_AI_BEHAVIOR_IDLE_H
#define ASW_AI_BEHAVIOR_IDLE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

class CAI_ASW_IdleBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_IdleBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_IdleBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_IDLE; }
	static	const char			*GetClassName() { return "behavior_idle"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() {	return "Idle"; }

	virtual bool	KeyValue( const char *szKeyName, const char *szValue );
	virtual void	Precache( void );
	virtual void	Init( );

	virtual bool 	CanSelectSchedule( );
	virtual void	GatherConditions( );
	virtual void	GatherConditionsNotActive( );

	virtual void	StartTask( const Task_t *pTask );
	virtual void	RunTask( const Task_t *pTask );

	virtual int		SelectSchedule( );

	virtual void	HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm );

	enum
	{
		SCHED_IDLE_SIT_AROUND = BaseClass::NEXT_SCHEDULE,		
		NEXT_SCHEDULE,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );

private:
	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_IDLE_H
