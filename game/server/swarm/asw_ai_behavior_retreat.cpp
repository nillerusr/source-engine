//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_retreat.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"
#include "asw_player.h"
#include "particle_parse.h"
#include "particles/particles.h"
#include "asw_missile_round_shared.h"
#include "movevars_shared.h"
#include "te_effect_dispatch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_RetreatBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_RetreatBehavior );


//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_RetreatBehavior::CAI_ASW_RetreatBehavior( )
{
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_RetreatBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_RetreatBehavior::Precache( void )
{
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CAI_ASW_RetreatBehavior::Init()
{
	CASW_Alien *pNPC = static_cast<CASW_Alien*>( GetOuter() );
	if ( !pNPC )
		return;
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_RetreatBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

	if ( !HasCondition( COND_LIGHT_DAMAGE ) && !HasCondition( COND_HEAVY_DAMAGE ) )
	{
		return false;
	}

	return BaseClass::CanSelectSchedule();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_RetreatBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_RetreatBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_RetreatBehavior::GatherCommonConditions( )
{
	BaseClass::GatherCommonConditions();
}

//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_RetreatBehavior::StartTask( const Task_t *pTask )
{

	switch( pTask->iTask )
	{
		case TASK_RETREAT:
			{
				GetOuter()->SetIdealActivity( ACT_STEP_BACK );
				return;
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
void CAI_ASW_RetreatBehavior::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_RETREAT:
			{
				if ( GetOuter()->IsActivityFinished() )
				{
					TaskComplete();
					return;
				}
				GetOuter()->AutoMovement();
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
int CAI_ASW_RetreatBehavior::SelectSchedule()
{
	return SCHED_RETREAT;
}


AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_RetreatBehavior )

	DECLARE_TASK( TASK_RETREAT )

	DEFINE_SCHEDULE
	(
		SCHED_RETREAT,
		"	Tasks"
		"		TASK_SET_ACTIVITY	ACT_STEP_BACK"
		"		TASK_RETREAT		0"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
	);

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
