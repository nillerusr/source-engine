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


#ifndef ASW_AI_BEHAVIOR_SHIELD_H
#define ASW_AI_BEHAVIOR_SHIELD_H
#ifdef _WIN32
#pragma once
#endif


#include "asw_ai_behavior.h"


class CAI_ASW_ShieldBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_ShieldBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_ShieldBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_SHIELD; }
	static	const char			*GetClassName() { return "behavior_shield"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() {	return "Shield"; }

	virtual bool	KeyValue( const char *szKeyName, const char *szValue );
	virtual void	Precache( void );
	virtual void	Init( );

	virtual bool 	CanSelectSchedule( );
	virtual void	GatherConditions( );
	virtual void	GatherConditionsNotActive( );

	virtual void	BeginScheduleSelection( );
	virtual void	EndScheduleSelection( );

	virtual void	StartTask( const Task_t *pTask );
	virtual void	RunTask( const Task_t *pTask );

	virtual int		SelectSchedule( );

	virtual void	HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm );
//	virtual bool	GetBehaviorParam( BehaviorParam_t eParam, bool &bResult );

	enum
	{
		SCHED_SHIELD_PREPARE = BaseClass::NEXT_SCHEDULE,		
		SCHED_SHIELD_MAINTAIN,		
		SCHED_SHIELD_LOWER,
		NEXT_SCHEDULE,

		TASK_SHIELD_RAISE = BaseClass::NEXT_TASK,
		TASK_SHIELD_MAINTAIN,
		TASK_SHIELD_MAINTAIN_FLIP,
		TASK_SHIELD_LOWER,
		NEXT_TASK,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );

private:
	float				m_flDistance;
	float				m_flMaxFrozenTime;
	float				m_flSFrozenDownTime;
	CUtlSymbol			m_FrozenParm;
	float				m_flFrozenTimeReductionMultiplier;
	float				m_flFrozenTimeExpansionMultiplier;
	bool				m_bShieldLowering;
	bool				m_bBeganFrozen;
	float				m_flStartFrozenTime;
	float				m_flEndFrozenTime;
	float				m_flStartDownTime;
	bool				m_bReachedFrozenLimit;
	Activity			m_TurningGesture;
	EHANDLE				m_Freezer;

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_SHIELD_H
