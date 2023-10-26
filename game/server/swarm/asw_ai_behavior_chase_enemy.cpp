//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_chase_enemy.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"
#include "ai_moveprobe.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_ChaseEnemyBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_ChaseEnemyBehavior );


// #define DRAW_DEBUG	1


//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_ChaseEnemyBehavior::CAI_ASW_ChaseEnemyBehavior( )
{
	m_flChaseDistance = 600.0f;
	m_bWalk = false;
	m_flLurchForwardDistance = 0.0f;
	m_flLurchStrafeDistance = 0.0f;
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_ChaseEnemyBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( V_stricmp( szKeyName, "chase_distance" ) == 0 )
	{
		m_flChaseDistance = atof( szValue );
		return true;
	}

	// Default is running, are we walking?
	if ( V_stricmp( szKeyName, "chase_movement" ) == 0 )
	{
		if ( V_stricmp( szValue, "walk" ) == 0 )
		{
			m_bWalk = true;
		}
		return true;
	}

	if ( V_stricmp( szKeyName, "lurch_forward_distance" ) == 0 )
	{
		m_flLurchForwardDistance = atof( szValue );
		return true;
	}

	if ( V_stricmp( szKeyName, "lurch_strafe_distance" ) == 0 )
	{
		m_flLurchStrafeDistance = atof( szValue );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_ChaseEnemyBehavior::Precache( void )
{
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_ChaseEnemyBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

	if ( ( CapabilitiesGet() & ( bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK2 |
			bits_CAP_INNATE_RANGE_ATTACK1 | bits_CAP_INNATE_RANGE_ATTACK2 ) ) == 0 )
	{
		return false;
	}

	if ( !HasCondition( COND_SEE_ENEMY ) && !HasCondition( COND_ENEMY_OCCLUDED ) )
	{
		return false;
	}

	if ( HasCondition( COND_LOST_ENEMY ) )
	{
		return false;
	}

	// Currently only have schedules written for primary attacks.
	if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) || HasCondition( COND_CAN_MELEE_ATTACK1 ) )
	{
		return false;
	}

	return BaseClass::CanSelectSchedule();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_ChaseEnemyBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_ChaseEnemyBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_ChaseEnemyBehavior::GatherCommonConditions( )
{
	BaseClass::GatherCommonConditions();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_ASW_ChaseEnemyBehavior::OnStartTaskLurchStrafe()
{
	// Verify that we can strafe.
	bool bCanStrafe = ( HaveSequenceForActivity( ACT_STRAFE_LEFT ) && HaveSequenceForActivity( ACT_STRAFE_RIGHT ) );
	if ( !bCanStrafe )
		return;

	// Get the strafe left and right directions.
	Vector vecRight;
	AngleVectors( GetOuter()->GetLocalAngles(), NULL, &vecRight, NULL );

	// Calculate the test lurch positions.
	Vector vecStart = GetOuter()->GetAbsOrigin();
	Vector vecEndRight = vecStart + ( vecRight * m_flLurchStrafeDistance );
	Vector vecEndLeft = vecStart + ( vecRight * -m_flLurchStrafeDistance );

	// Can we move to test lurch positions.
	AIMoveTrace_t moveTraceRight;
	AIMoveTrace_t moveTraceLeft;
	GetOuter()->GetMoveProbe()->MoveLimit( NAV_GROUND, vecStart, vecEndRight, MASK_NPCSOLID, NULL, &moveTraceRight );
	GetOuter()->GetMoveProbe()->MoveLimit( NAV_GROUND, vecStart, vecEndLeft, MASK_NPCSOLID, NULL, &moveTraceLeft );

	bool bCanMoveRight = !IsMoveBlocked( moveTraceLeft.fStatus );
	bool bCanMoveLeft =	!IsMoveBlocked( moveTraceLeft.fStatus );

	// Nowhere to lurch to!
	if ( !bCanMoveRight && !bCanMoveLeft )
	{
		TaskFail( FAIL_NO_ROUTE );
		return;
	}

	if ( bCanMoveRight && !bCanMoveLeft )
	{
#ifdef DRAW_DEBUG
		UTIL_AddDebugLine( vecStart, vecEndRight, true, false );
#endif	// #ifdef DRAW_DEBUG
		AI_NavGoal_t goal( GOALTYPE_LOCATION, vecEndRight, ACT_STRAFE_RIGHT, 10.0f, AIN_LOCAL_SUCCEEED_ON_WITHIN_TOLERANCE, AIN_DEF_TARGET );
		GetNavigator()->SetGoal( goal );
	}
	else if ( !bCanMoveRight && bCanMoveLeft )
	{
#ifdef DRAW_DEBUG
		UTIL_AddDebugLine( vecStart, vecEndLeft, true, false );
#endif	// #ifdef DRAW_DEBUG
		AI_NavGoal_t goal( GOALTYPE_LOCATION, vecEndLeft, ACT_STRAFE_LEFT, 10.0f, AIN_LOCAL_SUCCEEED_ON_WITHIN_TOLERANCE, AIN_DEF_TARGET );
		GetNavigator()->SetGoal( goal );
	}
	else
	{
		if ( RandomFloat() < 0.5f )
		{
#ifdef DRAW_DEBUG
			UTIL_AddDebugLine( vecStart, vecEndRight, true, false );
#endif	// #ifdef DRAW_DEBUG
			AI_NavGoal_t goal( GOALTYPE_LOCATION, vecEndRight, ACT_STRAFE_RIGHT, 10.0f, AIN_LOCAL_SUCCEEED_ON_WITHIN_TOLERANCE, AIN_DEF_TARGET );
			GetNavigator()->SetGoal( goal );
		}
		else
		{
#ifdef DRAW_DEBUG
			UTIL_AddDebugLine( vecStart, vecEndLeft, true, false );
#endif	// #ifdef DRAW_DEBUG
			AI_NavGoal_t goal( GOALTYPE_LOCATION, vecEndLeft, ACT_STRAFE_LEFT, 10.0f, AIN_LOCAL_SUCCEEED_ON_WITHIN_TOLERANCE, AIN_DEF_TARGET );
			GetNavigator()->SetGoal( goal );
		}
	}

	// Done with task.
	TaskComplete();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_ASW_ChaseEnemyBehavior::OnStartTaskLurchForward()
{
	// Get the strafe left and right directions.
	Vector vecForward;
	AngleVectors( GetOuter()->GetLocalAngles(), &vecForward, NULL, NULL );

	// Calculate the test lurch positions.
	Vector vecStart = GetOuter()->GetAbsOrigin();
	Vector vecEnd = vecStart + ( vecForward * m_flLurchForwardDistance );

	// Test movement.
	AIMoveTrace_t moveTrace;
	GetOuter()->GetMoveProbe()->MoveLimit( NAV_GROUND, vecStart, vecEnd, MASK_NPCSOLID, NULL, &moveTrace );

	if ( !IsMoveBlocked( moveTrace.fStatus ) )
	{
		AI_NavGoal_t goal( GOALTYPE_LOCATION, vecEnd, ACT_RUN, 10.0f, AIN_LOCAL_SUCCEEED_ON_WITHIN_TOLERANCE, AIN_DEF_TARGET );
		GetNavigator()->SetGoal( goal );

		// Done with task.
		TaskComplete();
	}
	else
	{
		TaskFail( FAIL_NO_ROUTE );
	}
}


//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_ChaseEnemyBehavior::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_GET_CHASE_PATH_TO_ENEMY:
		{
			CBaseEntity *pEnemy = GetEnemy();
			if ( !pEnemy )
			{
				TaskFail(FAIL_NO_ROUTE);
				return;
			}

			if ( ( pEnemy->GetAbsOrigin() - GetEnemyLKP() ).LengthSqr() < Square( m_flChaseDistance ) )
			{
				ChainStartTask( TASK_GET_PATH_TO_ENEMY );
			}
			else
			{
				ChainStartTask( TASK_GET_PATH_TO_ENEMY_LKP );
			}

			if ( !TaskIsComplete() && !HasCondition(COND_TASK_FAILED) )
			{
				TaskFail(FAIL_NO_ROUTE);
			}
			break;
		}
	case TASK_LURCH_FORWARD:
		{
			OnStartTaskLurchForward();
			break;
		}
	case TASK_LURCH_STRAFE:
		{
			OnStartTaskLurchStrafe();
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
void CAI_ASW_ChaseEnemyBehavior::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_LURCH_FORWARD:
		{
			break;
		}
	case TASK_LURCH_STRAFE:
		{
			break;
		}
	default:
		{
			BaseClass::RunTask( pTask );
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : int
//-----------------------------------------------------------------------------
int CAI_ASW_ChaseEnemyBehavior::HandleLurch()
{
	// Lurch Forward?
	if ( HasCondition( COND_CHASE_ENEMY_LURCH_FORWARD_PRIMARY ) )
		return SCHED_CHASE_ENEMY_LURCH_FORWARD_PRIMARY;

	// Lurch Strafe?
	if ( HasCondition( COND_CHASE_ENEMY_LURCH_STRAFE_PRIMARY ) )
	{
		// Do we have the strafe capability?
		bool bCanStrafe = ( HaveSequenceForActivity( ACT_STRAFE_LEFT ) && HaveSequenceForActivity( ACT_STRAFE_RIGHT ) );
		if ( bCanStrafe )
			return SCHED_CHASE_ENEMY_LURCH_STRAFE_PRIMARY;
	}

	// Lurch Forward or Strafe?
	if ( HasCondition( COND_CHASE_ENEMY_LURCH_FORWARD_OR_STRAFE_PRIMARY ) )
	{
		// Do we have the strafe capability?
		bool bCanStrafe = ( HaveSequenceForActivity( ACT_STRAFE_LEFT ) && HaveSequenceForActivity( ACT_STRAFE_RIGHT ) );
		if ( bCanStrafe )
		{
			float flRandomStrafe = RandomFloat( 1.0f, 100.0f );
			if ( flRandomStrafe < 50.0f )
				return SCHED_CHASE_ENEMY_LURCH_STRAFE_PRIMARY;
		}

		return SCHED_CHASE_ENEMY_LURCH_FORWARD_PRIMARY;
	}

	// No Lurching!
	return SCHED_NONE;
}


//------------------------------------------------------------------------------
// Purpose: routine called to select what schedule we want to run
// Output : returns the schedule id of the schedule we want to run
//------------------------------------------------------------------------------
int CAI_ASW_ChaseEnemyBehavior::SelectSchedule()
{
	// Should we lurch - strafe/forward?
	int nLurch = HandleLurch();
	if ( nLurch != SCHED_NONE )
		return nLurch;

	if ( CapabilitiesGet() & bits_CAP_INNATE_RANGE_ATTACK1 )
	{
		return SCHED_ESTABLISH_LINE_OF_FIRE;
	}

	// Default - Walking/Running.
	if ( m_bWalk )
		return SCHED_CHASE_ENEMY_WALK_PRIMARY;
	else
		return SCHED_CHASE_ENEMY_RUN_PRIMARY;
}


AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_ChaseEnemyBehavior )

	DECLARE_TASK( TASK_LURCH_FORWARD )
	DECLARE_TASK( TASK_LURCH_STRAFE )

	DECLARE_CONDITION( COND_CHASE_ENEMY_LURCH_FORWARD_PRIMARY )
	DECLARE_CONDITION( COND_CHASE_ENEMY_LURCH_STRAFE_PRIMARY )
	DECLARE_CONDITION( COND_CHASE_ENEMY_LURCH_FORWARD_OR_STRAFE_PRIMARY )

	DEFINE_SCHEDULE
	(
		SCHED_CHASE_ENEMY_RUN_PRIMARY,

		"	Tasks"
		"		 TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY_PRIMARY_FAILED"
		"		 TASK_SET_TOLERANCE_DISTANCE	24"
		"		 TASK_GET_CHASE_PATH_TO_ENEMY	600"
		"		 TASK_RUN_PATH					0"
		"		 TASK_WAIT_FOR_MOVEMENT			0"
		"		 TASK_FACE_ENEMY				0"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_ENEMY_UNREACHABLE"
		"		COND_LOST_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_TOO_CLOSE_TO_ATTACK"
		"		COND_TASK_FAILED"
		"		COND_HEAVY_DAMAGE"
		"		COND_CHARGE_CAN_CHARGE"
	);

	DEFINE_SCHEDULE
	(
		SCHED_CHASE_ENEMY_WALK_PRIMARY,

		"	Tasks"
		"		 TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY_PRIMARY_FAILED"
		"		 TASK_SET_TOLERANCE_DISTANCE	24"
		"		 TASK_GET_CHASE_PATH_TO_ENEMY	600"
		"		 TASK_WALK_PATH					0"
		"		 TASK_WAIT_FOR_MOVEMENT			0"
		"		 TASK_FACE_ENEMY				0"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_ENEMY_UNREACHABLE"
		"		COND_LOST_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_TOO_CLOSE_TO_ATTACK"
		"		COND_TASK_FAILED"
		"		COND_HEAVY_DAMAGE"
		"		COND_CHASE_ENEMY_LURCH_FORWARD_PRIMARY"
		"		COND_CHASE_ENEMY_LURCH_STRAFE_PRIMARY"
		"		COND_CHASE_ENEMY_LURCH_FORWARD_OR_STRAFE_PRIMARY"
		"		COND_CHARGE_CAN_CHARGE"
	);

	DEFINE_SCHEDULE
	(
		SCHED_CHASE_ENEMY_LURCH_FORWARD_PRIMARY,

		"	Tasks"
		"		 TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY_WALK_PRIMARY"
		"		 TASK_SET_TOLERANCE_DISTANCE	24"
		"		 TASK_GET_CHASE_PATH_TO_ENEMY	600"
		"		 TASK_LURCH_FORWARD				0"
		"		 TASK_WAIT_FOR_MOVEMENT			0"
		"		 TASK_FACE_ENEMY				0"
		"	"
		"	Interrupts"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
	);

	DEFINE_SCHEDULE
	(
		SCHED_CHASE_ENEMY_LURCH_STRAFE_PRIMARY,

		"	Tasks"
		"		 TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY_WALK_PRIMARY"
		"		 TASK_SET_TOLERANCE_DISTANCE	24"
		"		 TASK_LURCH_STRAFE				0"
		"		 TASK_WAIT_FOR_MOVEMENT			0"
		"		 TASK_FACE_ENEMY				0"
		"	"
		"	Interrupts"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
	);

	DEFINE_SCHEDULE
	(
		SCHED_CHASE_ENEMY_PRIMARY_FAILED,

		"	Tasks"
		"		 TASK_STOP_MOVING					0"
		"		 TASK_WAIT							0.2"
		"		 TASK_SET_FAIL_SCHEDULE				SCHEDULE:SCHED_CHASE_ENEMY_PRIMARY_STANDOFF"
		//	"		 TASK_SET_TOLERANCE_DISTANCE		24"
		"		 TASK_FIND_COVER_FROM_ENEMY			0"
		"		 TASK_RUN_PATH						0"
		"		 TASK_WAIT_FOR_MOVEMENT				0"
		"		 TASK_REMEMBER						MEMORY:INCOVER"
		"		 TASK_FACE_ENEMY					0"
		"		 TASK_SET_ACTIVITY					ACTIVITY:ACT_IDLE"	// Translated to cover
		"		 TASK_WAIT							1"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_HEAR_DANGER"
		"		COND_BETTER_WEAPON_AVAILABLE"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_CHARGE_CAN_CHARGE"
	);

	DEFINE_SCHEDULE
	(
		SCHED_CHASE_ENEMY_PRIMARY_STANDOFF,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_WANDER						480384" // 4 feet to 32 feet
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_STOP_MOVING				0"
		"		TASK_WAIT_PVS					0" // if the player left my PVS, just wait.
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_DEAD"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_CHARGE_CAN_CHARGE"
	);

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
