//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_advance.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"
#include "asw_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_AdvanceBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_AdvanceBehavior );


//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_AdvanceBehavior::CAI_ASW_AdvanceBehavior( )
{
	m_bInitialPathSet = false;
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_AdvanceBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_AdvanceBehavior::Precache( void )
{
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_AdvanceBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

	if ( HasCondition( COND_ADVANCE_COMPLETED ) )
	{
		return false;
	}

	if ( m_bInitialPathSet )
	{
		return false;
	}

	if ( GetEnemy() == NULL )
	{
		return false;
	}

	return BaseClass::CanSelectSchedule();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_AdvanceBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_AdvanceBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_AdvanceBehavior::GatherCommonConditions( )
{
	BaseClass::GatherCommonConditions();
}


//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_AdvanceBehavior::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_ADVANCE_TOWARDS_ENEMY:
			{
				CBaseEntity *pEnemy = GetEnemy();

				if ( pEnemy == NULL )
				{
					TaskFail( FAIL_NO_ENEMY );
					return;
				}

				AI_NavGoal_t goal( GOALTYPE_ENEMY, ACT_RUN, 100.0f, AIN_YAW_TO_DEST | AIN_LOCAL_SUCCEEED_ON_WITHIN_TOLERANCE | AIN_UNLIMITED_DISTANCE, AIN_DEF_TARGET );

				if ( GetNavigator()->SetGoal( goal ) )
				{
					m_bInitialPathSet = true;
					TaskComplete();
				}
				else
				{
					TaskFail( FAIL_NO_ROUTE );
				}
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
void CAI_ASW_AdvanceBehavior::RunTask( const Task_t *pTask )
{
#if 0
	switch( pTask->iTask )
	{
		default:
			BaseClass::RunTask( pTask );
			break;
	}
#endif
	BaseClass::RunTask( pTask );
}


//------------------------------------------------------------------------------
// Purpose: routine called to select what schedule we want to run
// Output : returns the schedule id of the schedule we want to run
//------------------------------------------------------------------------------
int CAI_ASW_AdvanceBehavior::SelectSchedule()
{
	SetCondition( COND_ADVANCE_COMPLETED );

	return SCHED_ADVANCE_TOWARDS_ENEMY;
}


AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_AdvanceBehavior )

	DECLARE_TASK( TASK_ADVANCE_TOWARDS_ENEMY )

	DECLARE_CONDITION( COND_ADVANCE_COMPLETED )

	DEFINE_SCHEDULE
	(
		SCHED_ADVANCE_TOWARDS_ENEMY,
		"	Tasks"
		"		TASK_ADVANCE_TOWARDS_ENEMY	0"
		"		TASK_RUN_PATH				0"
		"		TASK_WAIT_FOR_MOVEMENT		0"
		""
		"	Interrupts"
	);

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
