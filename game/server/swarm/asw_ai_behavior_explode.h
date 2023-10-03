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


#ifndef ASW_AI_BEHAVIOR_EXPLODE_H
#define ASW_AI_BEHAVIOR_EXPLODE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

class CAI_ASW_ExplodeBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_ExplodeBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_ExplodeBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_EXPLODE; }
	static	const char			*GetClassName() { return "behavior_explode"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() {	return "Explode"; }

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

	enum EExplodeType
	{
		EXPLODE_TYPE_PUFF_PROJECTILES,
		EXPLODE_TYPE_SUICIDE,

		EXPLODE_TYPE_EXPLODED,

		EXPLODE_TYPE_MAX
	};

	enum
	{
		SCHED_EXPLODE_PREPARE = BaseClass::NEXT_SCHEDULE,		
		SCHED_EXPLODE_SUICIDE,
		NEXT_SCHEDULE,

		TASK_EXPLODE_KILL_SELF = BaseClass::NEXT_TASK,
		TASK_EXPLODE_PREPARE_BUILDUP,
		TASK_EXPLODE_BEGIN_BUILDUP,
		TASK_EXPLODE_SUICIDE,
		NEXT_TASK,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );

			void	MoveTowardsPlayer( );
			void	DoExplosion( );
			void	DoSuicideExplosion( );

private:
	EExplodeType	m_ExplodeType;
	float			m_flRange;
	int				m_nMaxProjectiles;
	float			m_flMinVelocity;
	float			m_flMaxVelocity;
	CUtlSymbol		m_AttachName;
	float			m_flMaxBuildupTime;
	float			m_flBuildupApproachDist;
	int				m_nAttachCount;
	float			m_flDamageAmount;
	float			m_flDamageRadius;
	float			m_flStartExplodeTime;
	float			m_flStartBuildup;
	float			m_flEndBuildup;
	int				m_iSecondaryLayer;

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_EXPLODE_H
