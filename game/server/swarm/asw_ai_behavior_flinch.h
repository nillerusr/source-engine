//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Alien will flinch on damage
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//


#ifndef ASW_AI_BEHAVIOR_FLINCH_H
#define ASW_AI_BEHAVIOR_FLINCH_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

class CAI_ASW_FlinchBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_FlinchBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_FlinchBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_FLINCH; }
	static	const char			*GetClassName() { return "behavior_flinch"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() {	return "Flinch"; }

	virtual bool	KeyValue( const char *szKeyName, const char *szValue );
	virtual void	Precache( void );
	virtual void	Init( );

	virtual bool 	CanSelectSchedule( );
	virtual void	GatherConditions( );
	virtual void	GatherConditionsNotActive( );

	virtual void	BeginScheduleSelection( );

	virtual void	StartTask( const Task_t *pTask );
	virtual void	RunTask( const Task_t *pTask );

	virtual int		SelectSchedule( );

	virtual void	HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm );

	virtual void	OnOuterTakeDamage( const CTakeDamageInfo &info );
	virtual bool	ShouldStumble( const CTakeDamageInfo &info );

	enum
	{
		SCHED_ASW_FLINCH = BaseClass::NEXT_SCHEDULE,		
		NEXT_SCHEDULE,

		TASK_ASW_FLINCH = BaseClass::NEXT_TASK,
		NEXT_TASK,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );

	Activity m_FlinchActivity;
	float m_flNextFlinchTime;

private:
	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_FLINCH_H
