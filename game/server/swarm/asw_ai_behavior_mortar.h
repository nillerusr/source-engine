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


#ifndef ASW_AI_BEHAVIOR_MORTAR_H
#define ASW_AI_BEHAVIOR_MORTAR_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

class CAI_ASW_MortarBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_MortarBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_MortarBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_MORTAR; }
	static	const char			*GetClassName() { return "behavior_mortar"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() { return "Mortar"; }

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

			float	GetBlockedAmount( ) { return m_flBlockedAmount; }
			Vector	&GetMortarLocation( ) { return m_vMortarLocation; }

	enum
	{
		SCHED_MORTAR_MOVE_TO_FIRE_SPOT = BaseClass::NEXT_SCHEDULE,
		NEXT_SCHEDULE,

		TASK_MORTAR_FIND_MORTAR_LOCATION = BaseClass::NEXT_TASK,
		TASK_MORTAR_PREPARE_TO_FIRE,
		TASK_MORTAR_FIRE,
		TASK_MORTAR_FIRE_RECOVER,

		NEXT_TASK,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );

private:
	void	BadLocation( );
	void	FindMortarLocation( );
	void	CalcVelocity( Vector &vVelocity );
	void	ValidateMortarLocation( );
	void	LaunchMortar( );

	float	m_flBlockedAmount;
	float	m_flMinFiringDistance;
	float	m_flMaxFiringDistance;
	float	m_flMaxFiringDistanceSq;
	float	m_flDamageRadiusSq;
	float	m_flProjectileVelocity;
	float	m_flDamageAmount;
	float	m_flDamageRadius;
	float	m_flFireRate;
	float	m_flAccuracy;
	Vector	m_vMortarLocation;
	float	m_flDeferUntil;

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_MORTAR_H


