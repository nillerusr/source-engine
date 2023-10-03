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


#ifndef ASW_AI_BEHAVIOR_MELEE_H
#define ASW_AI_BEHAVIOR_MELEE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

class CAI_ASW_MeleeBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_MeleeBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_MeleeBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_MELEE; }
	static	const char			*GetClassName() { return "behavior_melee"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() { return "Melee"; }

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

	virtual bool	BehaviorHandleAnimEvent( animevent_t *pEvent );

	virtual bool	CanYieldTargetSlot() { return false; }			// this behavior won't yield target slot if it's primary

	enum
	{
		SCHED_MELEE1_DO_ATTACK = BaseClass::NEXT_SCHEDULE,		
		SCHED_MELEE1_DO_ATTACK_NO_TURNING,		
		SCHED_MELEE2_DO_ATTACK,
		SCHED_MELEE2_DO_ATTACK_NO_TURNING,		
		NEXT_SCHEDULE,

		TASK_MELEE_FLIP_AROUND = BaseClass::NEXT_TASK,
		NEXT_TASK,

	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );

private:
			void	HullAttack( float flDistance, float flDamage, float flForce, CUtlSymbol &AttackHitSound, CUtlSymbol &AttackMissSound );
			void	SphereAttack( float flDistance, float flDamage, float flForce, CUtlSymbol &AttackHitSound, CUtlSymbol &AttackMissSound );

	float		m_flMinRange;
	float		m_flMaxRange;
	float		m_flAttackDotAngle;
	float		m_flMinDamage;
	float		m_flMaxDamage;
	float		m_flForce;
	float		m_flPunchVelocity;
	float		m_flPunchAngle;
	bool		m_bNoTurnDuringAttack;
	CUtlSymbol	m_AttackHitSound;
	CUtlSymbol	m_MissHitSound;
	bool		m_bKnockdown;
	float		m_flKnockdownLift;
	float		m_flKnockdownSpeed;
	bool		m_bSecondaryMelee;

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_MELEE_H


