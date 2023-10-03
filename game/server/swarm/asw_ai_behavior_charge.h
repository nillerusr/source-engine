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


#ifndef ASW_AI_BEHAVIOR_CHARGE_H
#define ASW_AI_BEHAVIOR_CHARGE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

class CAI_ASW_ChargeBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_ChargeBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_ChargeBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_CHARGE; }
	static	const char			*GetClassName() { return "behavior_charge"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() {	return "Charge"; }

	virtual bool	KeyValue( const char *szKeyName, const char *szValue );
	virtual void	Precache( void );
	virtual void	Init( );

	virtual bool 	CanSelectSchedule( );
	virtual void	OnStartSchedule( int scheduleType );
	virtual void	GatherConditions( );
	virtual void	GatherConditionsNotActive( );

	virtual void	StartTask( const Task_t *pTask );
	virtual void	RunTask( const Task_t *pTask );

	virtual int		SelectSchedule( );

	virtual void	HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm );

	void			ChargeDamage( CBaseEntity *pTarget );
	void			ApplyChargeDamage( CBaseEntity *pTarget, float flDamage );
	bool			EnemyIsRightInFrontOfMe( CBaseEntity **pEntity );
	void			ChargeLookAhead( void );
	bool			HandleChargeImpact( Vector vecImpact, CBaseEntity *pEntity );
	float			ChargeSteer( void );
	virtual float	GetMass();

	enum
	{
		SCHED_CHARGE_DO = BaseClass::NEXT_SCHEDULE,		
		NEXT_SCHEDULE,

		TASK_CHARGE_MOVE = BaseClass::NEXT_TASK,
		NEXT_TASK,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );

private:
	float		m_flMinRange;
	float		m_flMaxRange;
	float		m_flMinDamage;
	float		m_flMaxDamage;
	float		m_flAngle;
	CUtlSymbol	m_IsCrouched;
	Vector		m_ChargeDestination;
	int			m_iChargeMisses;
	float		m_flChargeDamage;
	bool		m_bDecidedNotToStop;
	CUtlSymbol	m_FinishedChargeParm;
	float		m_flRetryChargeChance;

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_CHARGE_H
