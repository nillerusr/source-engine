//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		When NPC has COND_CAN_RANGE_ATTACK1, this will make him fire
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//


#ifndef ASW_AI_BEHAVIOR_RANGED_ATTACK_H
#define ASW_AI_BEHAVIOR_RANGED_ATTACK_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

class CAI_ASW_RangedAttackBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_RangedAttackBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_RangedAttackBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_RANGED_ATTACK; }
	static	const char			*GetClassName() { return "behavior_rangedattack"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() { return "RangedAttack"; }

	static void		LevelInit( void );

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

	virtual bool	CanYieldTargetSlot() { return false; }			// this behavior won't yield target slot if it's primary

			Vector	&GetMissileLocation( ) { return m_vMissileLocation; }

	enum
	{
		SCHED_RANGED_MOVE_TO_FIRE_SPOT = BaseClass::NEXT_SCHEDULE,
		NEXT_SCHEDULE,

		TASK_RANGED_FIND_MISSILE_LOCATION = BaseClass::NEXT_TASK,
		TASK_RANGED_PREPARE_TO_FIRE,
		TASK_RANGED_FIRE,
		TASK_RANGED_FIRE_RECOVER,

		NEXT_TASK,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions();

private:
	bool	FindFiringLocation();
	void	UpdateTargetLocation();
	bool	ValidateMissileLocation();

	float	m_flMinFiringDistance;
	float	m_flMaxFiringDistance;
	float	m_flMaxFiringDistanceSq;
	float	m_flProjectileVelocity;
	float	m_flFireRate;
	float	m_flGlobalShotDelay;
	CUtlString	m_strVolleyType;
	bool	m_bRadiusAttack;

	EHANDLE			m_hTarget;
	Vector			m_vMissileLocation;
	static float	s_flGlobalShotDeferUntil;
	float			m_flDeferUntil;

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_RANGED_ATTACK_H


