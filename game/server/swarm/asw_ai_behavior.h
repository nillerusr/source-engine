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


#ifndef ASW_AI_BEHAVIOR_H
#define ASW_AI_BEHAVIOR_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlsymbol.h"
#include "ai_behavior.h"
#include "asw_alien.h"


// Be sure to expose these to vscript in 
//		void CASW_Alien::LinkEnumsToScope( CScriptScope &hScope )
enum BehaviorEvent_t
{
	BEHAVIOR_EVENT_START_HEAL = 0,
	BEHAVIOR_EVENT_FINISH_HEAL,
	BEHAVIOR_EVENT_MORTAR_FIRE,
	BEHAVIOR_EVENT_MELEE1_HULL_ATTACK,
	BEHAVIOR_EVENT_MELEE1_SPHERE_ATTACK,
	BEHAVIOR_EVENT_FLICK,
	BEHAVIOR_EVENT_TOOK_DAMAGE,
	BEHAVIOR_EVENT_DEFLECTED_DAMAGE,
	BEHAVIOR_EVENT_EXPLODE,
	BEHAVIOR_EVENT_AWAKEN,
	BEHAVIOR_EVENT_SLEEP,
	BEHAVIOR_EVENT_POUNCE,
	BEHAVIOR_EVENT_JUMP,

	BEHAVIOR_EVENT_MAX
};

#if 0
enum BehaviorParam_t
{
	BEHAVIOR_PARAM_SPEED_MODIFIER = 0,
	BEHAVIOR_PARAM_IS_SHIELDED,

	BEHAVIOR_PARAM_MAX
};
#endif

#define MAX_REQUIRED_PARMS	2


//=============================================================================


enum BehaviorClass_T
{
	BEHAVIOR_CLASS_UNKNOWN = 0,
	BEHAVIOR_CLASS_ADVANCE,
	BEHAVIOR_CLASS_BACKOFF,
	BEHAVIOR_CLASS_CALL_GROUP,
	BEHAVIOR_CLASS_CHARGE,
	BEHAVIOR_CLASS_CHASE_ENEMY,
	BEHAVIOR_CLASS_COMBAT_STUN,
	BEHAVIOR_CLASS_EXPLODE,
	BEHAVIOR_CLASS_FEAR,
	BEHAVIOR_CLASS_FLICK,
	BEHAVIOR_CLASS_HEAL_OTHER,
	BEHAVIOR_CLASS_IDLE,
	BEHAVIOR_CLASS_MELEE,
	BEHAVIOR_CLASS_MORTAR,
	BEHAVIOR_CLASS_SCUTTLE,
	BEHAVIOR_CLASS_SHIELD,
	BEHAVIOR_CLASS_SLEEP,
	BEHAVIOR_CLASS_RANGED_ATTACK,
	BEHAVIOR_CLASS_RETREAT,
	BEHAVIOR_CLASS_WANDER,
	BEHAVIOR_CLASS_PROTECT,
	BEHAVIOR_CLASS_POUNCE,
	BEHAVIOR_CLASS_PREPARE_TO_ENGAGE,
	BEHAVIOR_CLASS_FLINCH,
	BEHAVIOR_CLASS_JUMP,

	BEHAVIOR_CLASS_MAX
};


//=============================================================================


class CAI_ASW_Behavior : public CAI_SimpleBehavior
{
	DECLARE_CLASS( CAI_ASW_Behavior, CAI_SimpleBehavior );

public:
	static	CUtlSymbol	GetSymbol( const char *pszText );
	static	const char	*GetSymbolText( CUtlSymbol &Symbol );

public:
	CAI_ASW_Behavior( );

	virtual BehaviorClass_T		Classify( void ) { return ( BehaviorClass_T )BEHAVIOR_CLASS_UNKNOWN; }

	virtual bool	KeyValue( const char *szKeyName, const char *szValue );
	virtual void	Init() {}

	virtual bool 	CanSelectSchedule( );
	virtual void	GatherConditions( );
	virtual void	GatherConditionsNotActive( );

	virtual void	BehaviorThink( void ) { }

	virtual void	HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm ) {}
	virtual bool	BehaviorHandleAnimEvent( animevent_t *pEvent ) { return false; }
//	virtual bool	GetBehaviorParam( BehaviorParam_t eParam, float &flResult ) { return false; }
//	virtual bool	GetBehaviorParam( BehaviorParam_t eParam, bool &bResult ) { return false; }

	// called when this is the primary behavior and the NPC touches something
	virtual void	StartTouch( CBaseEntity *pOther ) { }

	virtual bool	CanYieldTargetSlot() { return true; }			// will this behavior allow target slot to be yielded when it's the primary behavior?

	enum
	{
		COND_CHARGE_CAN_CHARGE = BaseClass::NEXT_CONDITION,
		NEXT_CONDITION,
	};

	DEFINE_CUSTOM_SCHEDULE_PROVIDER;

protected:
	inline	int		GetBehaviorParam( CUtlSymbol ParmName );
	inline	void	SetBehaviorParam( CUtlSymbol ParmName, int nValue );

	virtual void	GatherCommonConditions( ) { }


	CUtlSymbol	m_RequiredParm[ MAX_REQUIRED_PARMS ];
	int			m_nRequiredParmValue[ MAX_REQUIRED_PARMS ];
	int			m_nNumRequiredParms;
	CUtlSymbol	m_StatusParm;
	float		m_flScheduleChance;
	float		m_flScheduleChanceRate;
	float		m_flScheduleChanceNextCheck;

private:
	DECLARE_DATADESC();
};


int CAI_ASW_Behavior::GetBehaviorParam( CUtlSymbol ParmName )
{
	return static_cast< CASW_Alien * >( GetOuter() )->GetBehaviorParam( ParmName );
}


void CAI_ASW_Behavior::SetBehaviorParam( CUtlSymbol ParmName, int nValue )
{
	static_cast< CASW_Alien * >( GetOuter() )->SetBehaviorParam( ParmName, nValue );
}

#endif // ASW_AI_BEHAVIOR_H


