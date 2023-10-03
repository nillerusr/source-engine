//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Alien leaps at his enemy, doing damage on touch
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//


#ifndef ASW_AI_BEHAVIOR_JUMP_H
#define ASW_AI_BEHAVIOR_JUMP_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

extern Activity ACT_ALIEN_JUMP;
extern Activity ACT_ALIEN_LAND;

class CAI_ASW_JumpBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_JumpBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_JumpBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_JUMP; }
	static	const char			*GetClassName() { return "behavior_jump"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() { return "Jump"; }

	virtual bool	KeyValue( const char *szKeyName, const char *szValue );
	virtual void	Precache( void );
	virtual void	Init( );

	virtual bool 	CanSelectSchedule( );
	virtual void	GatherConditions( );
	virtual void	GatherConditionsNotActive( );

	virtual void	BeginScheduleSelection( );

	virtual int		SelectSchedule( );

	virtual void	HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm );

	virtual void	StartTask( const Task_t *pTask );
	virtual void	RunTask( const Task_t *pTask );

	virtual bool	CanYieldTargetSlot() { return false; }			// this behavior won't yield target slot if it's primary

	enum
	{
		SCHED_ALIEN_JUMP = BaseClass::NEXT_SCHEDULE,	
		NEXT_SCHEDULE,

		TASK_FACE_JUMP = BaseClass::NEXT_TASK,
		TASK_JUMP,
		NEXT_TASK,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );
	
	bool ShouldJump( void );
	bool CheckLanding( void );

	// params
	float		m_flMinRange;
	float		m_flJumpTimeMin;
	float		m_flJumpTimeMax;
	float		m_flJumpDamage;
	float		m_flDamageDistance;

	// state
	bool		m_bForcedStuckJump;
	bool		m_bHasDoneAirAttack;
	float		m_flJumpTime;
	Vector		m_vecSavedJump;
	Vector		m_vecLastJumpAttempt;

private:

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_JUMP_H


