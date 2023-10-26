//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_combat_stun.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"
#include "asw_marine.h"
#include "particle_parse.h"
#include "asw_marine_skills.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_CombatStunBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_CombatStunBehavior );

// todo: if we want more varied stuns, tie this to damage, etc
static const float COMBAT_STUN_DURATION = 2.5f;

// for now govern how often we check recent damage events
static const float COMBAT_STUN_CHECK_TIME = 0.5f;

//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_CombatStunBehavior::CAI_ASW_CombatStunBehavior( )
{
	m_flStunEndTime = 0.0f;
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_CombatStunBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_CombatStunBehavior::Precache( void )
{
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_CombatStunBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

	if ( !HasCondition( COND_BEGIN_COMBAT_STUN ) )
	{
		return false;
	}

	return BaseClass::CanSelectSchedule();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_CombatStunBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_CombatStunBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();

	ClearCondition( COND_BEGIN_COMBAT_STUN );
	
	if ( !GetOuter() )
	{
		return;
	}

	if ( !IsAlienClass( GetOuter()->Classify() ) )
	{
		// This behavior isn't currently designed for non-aliens, relies on CASW_Alien::m_RecentDamage
		Assert( false );
		return;
	}

	CASW_Alien *pAlien = static_cast<CASW_Alien*>( GetOuter() );

	for ( int i = 0; i < pAlien->m_RecentDamage.Count(); i++ )
	{
		int nDamageType = pAlien->m_RecentDamage[i].GetDamageType();

		if ( (nDamageType & DMG_CLUB) && (nDamageType & DMG_SONIC) )
		{
			if ( pAlien->m_RecentDamage[i].GetAttacker() && pAlien->m_RecentDamage[i].GetAttacker()->Classify() == CLASS_ASW_MARINE )
			{	
				m_flStunDuration = COMBAT_STUN_DURATION;
				SetCondition( COND_BEGIN_COMBAT_STUN );
				break;
			}
		}
	}
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_CombatStunBehavior::GatherCommonConditions( )
{
	BaseClass::GatherCommonConditions();
}


//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_CombatStunBehavior::StartTask( const Task_t *pTask )
{
	if ( !GetOuter() || !IsAlienClass( GetOuter()->Classify() ) )
	{
		TaskComplete();

		// This behavior isn't currently designed for non-aliens
		Assert( false );
		return;
	}

	CASW_Alien *pAlien = static_cast<CASW_Alien*>( GetOuter() );

	switch( pTask->iTask )
	{
		case TASK_COMBAT_STUN:
		{
			m_flStunEndTime = gpGlobals->curtime + m_flStunDuration;
			pAlien->m_RecentDamage.RemoveAll();
			DispatchParticleEffect( "melee_stun", PATTACH_POINT_FOLLOW, pAlien, "attach_top" );

			break;
		}
		default:
		{
			BaseClass::StartTask( pTask );
			break;
		}
	}
}


//------------------------------------------------------------------------------
// Purpose: routine called every frame when a task is running
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_CombatStunBehavior::RunTask( const Task_t *pTask )
{
	CASW_Alien *pAlien = dynamic_cast<CASW_Alien*>( GetOuter() );

	if ( !pAlien )
	{
		TaskComplete();

		Assert( false );
		return;
	}

	switch( pTask->iTask )
	{
		case TASK_COMBAT_STUN:
		{
			if ( gpGlobals->curtime > m_flStunEndTime )
			{
				TaskComplete();
			}
			break;
		}
		default:
		{
			BaseClass::RunTask( pTask );
			break;
		}
	}
}


//------------------------------------------------------------------------------
// Purpose: routine called to select what schedule we want to run
// Output : returns the schedule id of the schedule we want to run
//------------------------------------------------------------------------------
int CAI_ASW_CombatStunBehavior::SelectSchedule()
{
	if ( HasCondition( COND_BEGIN_COMBAT_STUN ) )
	{
		return SCHED_COMBAT_STUN;
	}

	// SelectSchedule() shouldn't be called unless we're ready to run this behavior...
	Assert( 0 );
	return SCHED_NONE;
}


AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_CombatStunBehavior )

	DECLARE_TASK( TASK_COMBAT_STUN );
	DECLARE_CONDITION( COND_BEGIN_COMBAT_STUN );

	DEFINE_SCHEDULE
	(
		 SCHED_COMBAT_STUN,
		 "	Tasks"
		 "		TASK_STOP_MOVING			0"
		 "		TASK_SET_ACTIVITY			ACTIVITY:ACT_FLINCH_PHYSICS"
		 "		TASK_COMBAT_STUN			0"
		 ""
		 "	Interrupts"
		 ""
	 );

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
