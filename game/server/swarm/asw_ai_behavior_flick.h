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


#ifndef ASW_AI_BEHAVIOR_FLICK_H
#define ASW_AI_BEHAVIOR_FLICK_H
#ifdef _WIN32
#pragma once
#endif


#include "asw_ai_behavior.h"


typedef struct SFlickInfo TFlickInfo;


class CAI_ASW_FlickBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_FlickBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_FlickBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_FLICK; }
	static	const char			*GetClassName() { return "behavior_flick"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() {	return "Flick"; }

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

	enum
	{
		SCHED_SHIELD_FLICK = BaseClass::NEXT_SCHEDULE,		
		NEXT_SCHEDULE,

		TASK_SHIELD_FLICK = BaseClass::NEXT_TASK,
		NEXT_TASK,

		COND_SHIELD_CAN_FLICK = BaseClass::NEXT_CONDITION,	
		NEXT_CONDITION,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );

private:

			TFlickInfo	*GetFlickActivity( );
			void		TryFlicking( CBaseEntity *pEntity );
			void		Flick( );

	float				m_flDistance;
	float				m_flDistanceSq;
	float				m_flPropelDistance;
	float				m_flPropelHeight;
	float				m_flMinDamage;
	float				m_flMaxDamage;
	float				m_flNextFlickCheck;
	TFlickInfo			*m_pPickedFlick;

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_FLICK_H
