//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Alien will flinch when hurt.
//				Flinch will either be a small gesture, or a full body activity
//

//	TODO: Gesture flinches

//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_flinch.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"
#include "asw_marine.h"
#include "asw_marine_profile.h"
#include "asw_weapon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_FlinchBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_FlinchBehavior );

Activity ACT_GESTURE_FLINCH;
Activity ACT_GESTURE_FLINCH_LEFT;
Activity ACT_GESTURE_FLINCH_RIGHT;
Activity ACT_GESTURE_FLINCH_BACK;
Activity ACT_GESTURE_FLINCH_FORWARD;
Activity ACT_STUMBLE;
Activity ACT_STUMBLE_LEFT;
Activity ACT_STUMBLE_RIGHT;
Activity ACT_STUMBLE_BACK;
Activity ACT_STUMBLE_FORWARD;

extern ConVar asw_debug_alien_damage;
extern ConVar asw_drone_weak_from_behind;

//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_FlinchBehavior::CAI_ASW_FlinchBehavior( )
{
	m_flNextFlinchTime = 0.0f;
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_FlinchBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_FlinchBehavior::Precache( void )
{
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CAI_ASW_FlinchBehavior::Init( )
{
#if 0
	CASW_Alien *pNPC = static_cast< CASW_Alien * >( GetOuter() );
	if ( !pNPC )
	{
		return;
	}
#endif
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_FlinchBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
		return false;
	
	//if ( gpGlobals->curtime < m_flNextFlinchTime )
		//return false;

	if ( !HasCondition( CASW_Alien::COND_ASW_FLINCH ) )	
		return false;

	return BaseClass::CanSelectSchedule();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_FlinchBehavior::GatherConditions()
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_FlinchBehavior::GatherConditionsNotActive()
{
	BaseClass::GatherConditionsNotActive();
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_FlinchBehavior::GatherCommonConditions()
{
	BaseClass::GatherCommonConditions();
}


void CAI_ASW_FlinchBehavior::BeginScheduleSelection()
{
	BaseClass::BeginScheduleSelection();
}


//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_FlinchBehavior::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_ASW_FLINCH:
		{
			if ( m_FlinchActivity == ACT_INVALID )
			{
				m_FlinchActivity = ACT_STUMBLE;
			}
			GetOuter()->SetIdealActivity( m_FlinchActivity );
			break;
		}
		default:
			BaseClass::StartTask( pTask );
			break;
	}
}


//------------------------------------------------------------------------------
// Purpose: routine called every frame when a task is running
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_FlinchBehavior::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_ASW_FLINCH:
		{
			GetOuter()->AutoMovement();
			if ( GetOuter()->IsActivityFinished() )
			{
				TaskComplete();
			}
			break;
		}
		default:
			BaseClass::RunTask( pTask );
			break;
	}
}


//------------------------------------------------------------------------------
// Purpose: routine called to select what schedule we want to run
// Output : returns the schedule id of the schedule we want to run
//------------------------------------------------------------------------------
int CAI_ASW_FlinchBehavior::SelectSchedule()
{
	return SCHED_ASW_FLINCH;
}


//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
void CAI_ASW_FlinchBehavior::HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm )
{
#if 0
	switch( eEvent )
	{
	}
#endif
}

bool CAI_ASW_FlinchBehavior::ShouldStumble( const CTakeDamageInfo &info )
{
	// shock damage never causes flinching
	if ( ( info.GetDamageType() & DMG_SHOCK ) != 0 )
		return false;

	if ( ( info.GetDamageType() & DMG_BLAST ) != 0 )
		return true;

	// dots never cause flinching
	if ( ( info.GetDamageType() & DMG_DIRECT ) != 0 )
		return false;

	if ( info.GetAttacker() && info.GetAttacker()->Classify() == CLASS_ASW_MARINE )
	{
		CASW_Marine *pMarine = assert_cast<CASW_Marine*>( info.GetAttacker() );
		
		CASW_Weapon *pWeapon = pMarine->GetActiveASWWeapon();
		if ( pWeapon && pWeapon->ShouldAlienFlinch( GetOuter(), info ) )
			return true;

		// non-melee marines always cause a flinch when they melee an alien
		ASW_Marine_Class MarineClass = pMarine->GetMarineProfile()->GetMarineClass();
		if ( (info.GetDamageType() & DMG_CLUB) != 0 && ( MarineClass != MARINE_CLASS_NCO ) )
			return true;
	}

	return false;
}

void CAI_ASW_FlinchBehavior::OnOuterTakeDamage( const CTakeDamageInfo &info )
{	
	m_FlinchActivity = ACT_INVALID;
	if ( !info.GetAttacker() )
		return;

	if ( gpGlobals->curtime < m_flNextFlinchTime )
		return;

	Vector vecSrcDiff = info.GetAttacker()->GetAbsOrigin() - GetAbsOrigin();
	VectorNormalize( vecSrcDiff );
	Vector forward;
	QAngle angFacing = GetAbsAngles();
	AngleVectors( angFacing, &forward );
	float flForwardDot = forward.Dot( vecSrcDiff );

	angFacing[YAW] += 90;
	AngleVectors( angFacing, &forward );
	float flSideDot = forward.Dot( vecSrcDiff );

	if ( ShouldStumble( info ) )
	{
		// always reflinch on melee attack
		m_flNextFlinchTime = gpGlobals->curtime + 1.0f;
				
		if ( flForwardDot > 0.5f )
		{
			m_FlinchActivity = (Activity) ACT_STUMBLE_BACK;
		}
		else if ( flForwardDot < 0.5f )
		{
			m_FlinchActivity = (Activity) ACT_STUMBLE_FORWARD;
		}
		else if ( flSideDot > 0.5f )
		{
			m_FlinchActivity = (Activity) ACT_STUMBLE_LEFT;
		}
		else
		{
			m_FlinchActivity = (Activity) ACT_STUMBLE_RIGHT;
		}

		if ( !GetOuter()->HaveSequenceForActivity( m_FlinchActivity ) )
		{
			m_FlinchActivity = ACT_STUMBLE;		// fall back to the standard stumble if we don't have the specific directionals
		}

		GetOuter()->SetCondition( CASW_Alien::COND_ASW_FLINCH );
	}
	else
	{
		Activity gestureActivity = ACT_INVALID;
		if ( flForwardDot > 0.5f )
		{
			gestureActivity = (Activity) ACT_GESTURE_FLINCH_BACK;
		}
		else if ( flForwardDot < 0.5f )
		{
			gestureActivity = (Activity) ACT_GESTURE_FLINCH_FORWARD;
		}
		else if ( flSideDot > 0.5f )
		{
			gestureActivity = (Activity) ACT_GESTURE_FLINCH_LEFT;
		}
		else
		{
			gestureActivity = (Activity) ACT_GESTURE_FLINCH_RIGHT;
		}

		if ( !GetOuter()->HaveSequenceForActivity( m_FlinchActivity ) )
		{
			gestureActivity = ACT_GESTURE_FLINCH;		// fall back to the standard stumble if we don't have the specific directionals
		}

		if ( HaveSequenceForActivity( gestureActivity ) )
		{
			GetOuter()->RestartGesture( gestureActivity );
		}

		if ( gestureActivity != ACT_INVALID )
		{
			//Get the duration of the flinch and delay the next one by that (plus a bit more)
			int iSequence = GetOuter()->GetLayerSequence( GetOuter()->FindGestureLayer( gestureActivity ) );

			if ( iSequence != ACT_INVALID )
			{
				//GetOuter()->
				m_flNextFlinchTime = gpGlobals->curtime + GetOuter()->SequenceDuration( iSequence );
			}
		}
	}
}


AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_FlinchBehavior )

	DECLARE_TASK( TASK_ASW_FLINCH )

	DECLARE_ACTIVITY( ACT_GESTURE_FLINCH )
	DECLARE_ACTIVITY( ACT_GESTURE_FLINCH_LEFT )
	DECLARE_ACTIVITY( ACT_GESTURE_FLINCH_RIGHT )
	DECLARE_ACTIVITY( ACT_GESTURE_FLINCH_BACK )
	DECLARE_ACTIVITY( ACT_GESTURE_FLINCH_FORWARD )
	DECLARE_ACTIVITY( ACT_STUMBLE )
	DECLARE_ACTIVITY( ACT_STUMBLE_LEFT )
	DECLARE_ACTIVITY( ACT_STUMBLE_RIGHT )
	DECLARE_ACTIVITY( ACT_STUMBLE_BACK )
	DECLARE_ACTIVITY( ACT_STUMBLE_FORWARD )
	
	DEFINE_SCHEDULE
	(
		SCHED_ASW_FLINCH,

		"	Tasks"
		"		 TASK_REMEMBER				MEMORY:FLINCHED  "
		//"		 TASK_STOP_MOVING			0"
		"		 TASK_ASW_FLINCH			0"
		""
		"	Interrupts"
		"		COND_ASW_FLINCH"
	)

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
