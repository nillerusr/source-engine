//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Deal intelligently with an enemy that we're afraid of
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//


#ifndef AI_ASW_BEHAVIOR_FEAR_H
#define AI_ASW_BEHAVIOR_FEAR_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

class CAI_ASW_FearBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_FearBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_FearBehavior();
	
	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_FEAR; }
	static	const char			*GetClassName() { return "behavior_fear"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() { return "ASW_Fear"; }

	virtual bool	KeyValue( const char *szKeyName, const char *szValue );
	virtual void	Precache( void );

	virtual bool 	CanSelectSchedule();
	virtual void	GatherConditions();
	
	virtual void	BeginScheduleSelection();
	virtual void	EndScheduleSelection();

	virtual void	StartTask( const Task_t *pTask );
	virtual void	RunTask( const Task_t *pTask );

	virtual void	BuildScheduleTestBits();
	virtual int		TranslateSchedule( int scheduleType );
	virtual int		SelectSchedule();

	enum
	{
		SCHED_FEAR_MOVE_TO_SAFE_PLACE = BaseClass::NEXT_SCHEDULE,		
		SCHED_FEAR_MOVE_TO_SAFE_PLACE_RETRY,
		SCHED_FEAR_STAY_IN_SAFE_PLACE,
		SCHED_FEAR_RUN_FROM_ENEMY,
		NEXT_SCHEDULE,

		TASK_FEAR_GET_PATH_TO_SAFETY_HINT = BaseClass::NEXT_TASK,
		TASK_FEAR_WAIT_FOR_SAFETY,
		TASK_FEAR_IN_SAFE_PLACE,
		TASK_FEAR_FIND_COVER_FROM_ENEMY,
		NEXT_TASK,

		COND_FEAR_ENEMY_CLOSE = BaseClass::NEXT_CONDITION,	// within 30 feet
		COND_FEAR_ENEMY_TOO_CLOSE,							// within 5 feet
		COND_FEAR_SEPARATED_FROM_PLAYER,
		NEXT_CONDITION,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

private:
	bool		IsInASafePlace( );
	void		SpoilSafePlace( );
	void		ReleaseAllHints( );
	CAI_Hint	*FindFearWithdrawalDest();

	bool			m_bForceFear;
	float			m_flDuration;

	float			m_flTimeToSafety;
	float			m_flTimePlayerLastVisible;
	float			m_flDeferUntil;
	
	EHANDLE			m_hTargetEnt;	// the entity that the npc is trying to run from

	CAI_MoveMonitor		m_SafePlaceMoveMonitor;
	CHandle<CAI_Hint>	m_hSafePlaceHint;
	CHandle<CAI_Hint>	m_hMovingToHint;

	DECLARE_DATADESC();
};

#endif // AI_ASW_BEHAVIOR_FEAR_H


