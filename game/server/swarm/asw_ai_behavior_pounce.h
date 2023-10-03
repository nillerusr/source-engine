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


#ifndef ASW_AI_BEHAVIOR_POUNCE_H
#define ASW_AI_BEHAVIOR_POUNCE_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_ai_behavior.h"

class CAI_ASW_PounceBehavior : public CAI_ASW_Behavior
{
	DECLARE_CLASS( CAI_ASW_PounceBehavior, CAI_ASW_Behavior );

public:
	CAI_ASW_PounceBehavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_POUNCE; }
	static	const char			*GetClassName() { return "behavior_pounce"; }
	virtual const char			*GetClassNameV() { return GetClassName(); }
	virtual const char			*GetName() { return "Pounce"; }

	virtual bool	KeyValue( const char *szKeyName, const char *szValue );
	virtual void	Precache( void );
	virtual void	Init( );

	virtual bool 	CanSelectSchedule( );
	virtual void	GatherConditions( );
	virtual void	GatherConditionsNotActive( );

	virtual void	BeginScheduleSelection( );

	virtual int		SelectSchedule( );
	virtual void	BehaviorThink( void );

	virtual void	HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm );

	virtual void	StartTouch( CBaseEntity *pOther );

	virtual bool	CanYieldTargetSlot() { return false; }			// this behavior won't yield target slot if it's primary

	enum
	{
		SCHED_ALIEN_POUNCE = BaseClass::NEXT_SCHEDULE,	
		NEXT_SCHEDULE,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	virtual void	GatherCommonConditions( );

	void PounceAttack( bool bRandomJump, const Vector &vecPos = vec3_origin, bool bThrown = false, float flBaseHeight = 60.0f, float flAdditionalHeight = 8.0f );
	void Leap( const Vector &vecVel );
	bool HasHeadroom();

	// params
	float		m_flMinRange;
	float		m_flMaxRange;
	float		m_flAttackDotAngle;
	float		m_flPounceInterval;
	float		m_flPounceDistance;
	float		m_flPounceDamage;

	// state
	float		m_flNextPounceTime;
	float		m_flIgnoreWorldCollisionTime;
	bool		m_bMidPounce;

private:

	DECLARE_DATADESC();
};

#endif // ASW_AI_BEHAVIOR_POUNCE_H


