//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_sleep.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_SleepBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_SleepBehavior );


//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_SleepBehavior::CAI_ASW_SleepBehavior( )
{
	m_bAwakened = true;
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_SleepBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_SleepBehavior::Precache( void )
{
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CAI_ASW_SleepBehavior::Init( )
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
bool CAI_ASW_SleepBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

	if ( m_bAwakened == true )
	{
		return false;
	}

	return BaseClass::CanSelectSchedule();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_SleepBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_SleepBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_SleepBehavior::GatherCommonConditions( )
{
	BaseClass::GatherCommonConditions();
}


//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_SleepBehavior::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_SLEEP_WAIT_UNTIL_AWAKENED:
			break;

		case TASK_SLEEP_UNBURROW:
			m_bAwakened = true;
			Unburrow();
			break;

		default:
			BaseClass::StartTask( pTask );
			break;
	}
}


//------------------------------------------------------------------------------
// Purpose: routine called every frame when a task is running
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_SleepBehavior::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_SLEEP_WAIT_UNTIL_AWAKENED:
			if ( m_bAwakened == true )
			{
				TaskComplete();
			}
			break;

		case TASK_SLEEP_UNBURROW:
			GetOuter()->AutoMovement();
			if ( GetOuter()->IsActivityFinished() && ( GetOuter()->GetCycle() >= 1.0f ) )
			{
				//Msg(" Burrow out complete seq = %d cycle = %f last vis cycle = %f\n", GetOuter()->GetSequence(), GetOuter()->GetCycle(), GetOuter()->GetLastVisibleCycle( GetOuter()->GetModelPtr(), GetOuter()->GetSequence() ) );
				TaskComplete();
			}
			break;

		default:
			BaseClass::RunTask( pTask );
			break;
	}
}


//------------------------------------------------------------------------------
// Purpose: routine called to select what schedule we want to run
// Output : returns the schedule id of the schedule we want to run
//------------------------------------------------------------------------------
int CAI_ASW_SleepBehavior::SelectSchedule()
{
	// If we're supposed to be burrowed, stay there
	//CASW_Alien *pNPC = static_cast< CASW_Alien * >( GetOuter() );
	//if ( pNPC->m_iStartBurrowed > 0 )
		//return SCHED_SLEEP_UNBURROW;

	return SCHED_SLEEP_SNORING;
}

//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
void CAI_ASW_SleepBehavior::HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm )
{
	switch( eEvent )
	{
		case BEHAVIOR_EVENT_AWAKEN:
			m_bAwakened = true;
			break;

		case BEHAVIOR_EVENT_SLEEP:
			m_bAwakened = false;
			break;
	}
}

//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
void CAI_ASW_SleepBehavior::ClearBurrowPoint( const Vector &origin )
{
	CBaseEntity *pEntity = NULL;
	float		flDist;
	Vector		vecSpot, vecCenter, vecForce;

	// Cause a ruckus
	//UTIL_ScreenShake( origin, 1.0f, 80.0f, 1.0f, 256.0f, SHAKE_START );

	// Iterate on all entities in the vicinity.
	for ( CEntitySphereQuery sphere( origin, 128 ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		if ( pEntity->Classify() != CLASS_PLAYER && pEntity->VPhysicsGetObject() )
		{
			vecSpot	 = pEntity->BodyTarget( origin );
			vecForce = ( vecSpot - origin ) + Vector( 0, 0, 16 );

			// decrease damage for an ent that's farther from the bomb.
			flDist = VectorNormalize( vecForce );
			if ( flDist <= 128.0f )
			{
				GetOuter()->CollisionProp()->RandomPointInBounds( vec3_origin, Vector( 1.0f, 1.0f, 1.0f ), &vecCenter );
				pEntity->VPhysicsGetObject()->Wake();
				pEntity->VPhysicsGetObject()->ApplyForceOffset( vecForce * 250.0f, vecCenter );
			}
		}
	}
}

//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
void CAI_ASW_SleepBehavior::Unburrow( void )
{
	CASW_Alien *pNPC = static_cast< CASW_Alien * >( GetOuter() );
	pNPC->SetIdealActivity( ACT_ALIEN_BURROW_OUT );
	//pNPC->m_iStartBurrowed = 0;	
	ClearBurrowPoint( pNPC->GetAbsOrigin() );	// physics blast anything in the way out of the way

	// Become solid again and visible
	pNPC->RemoveEffects( EF_NODRAW );
	pNPC->RemoveFlag( FL_NOTARGET );
	pNPC->RemoveSpawnFlags( SF_NPC_GAG );
	pNPC->RemoveSolidFlags( FSOLID_NOT_SOLID );
	pNPC->m_takedamage = DAMAGE_YES;

	pNPC->SetGroundEntity( NULL );
}

AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_SleepBehavior )

	DECLARE_TASK( TASK_SLEEP_WAIT_UNTIL_AWAKENED )
	DECLARE_TASK( TASK_SLEEP_UNBURROW )

	DEFINE_SCHEDULE
	(
		SCHED_SLEEP_SNORING,
		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_SET_ACTIVITY				ACTIVITY:ACT_SLEEP"
		"		TASK_SLEEP_WAIT_UNTIL_AWAKENED	0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_WAKE"
		""
		"	Interrupts"
	);

	DEFINE_SCHEDULE
	(
		SCHED_SLEEP_UNBURROW,

		"	Tasks"
		"		TASK_SLEEP_UNBURROW				0"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
	);

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
