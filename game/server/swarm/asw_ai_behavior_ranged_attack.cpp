//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_ranged_attack.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"
#include "asw_player.h"
#include "asw_missile_round_shared.h"
#include "movevars_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_RangedAttackBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_RangedAttackBehavior );

float CAI_ASW_RangedAttackBehavior::s_flGlobalShotDeferUntil = 0;

//#define DRAW_DEBUG	1


//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_RangedAttackBehavior::CAI_ASW_RangedAttackBehavior( )
{
	m_flMinFiringDistance = 200.0f;
	m_flMaxFiringDistance = 800.0f;
	m_flMaxFiringDistanceSq = m_flMaxFiringDistance * m_flMaxFiringDistance;;

	m_flFireRate = 2.0;

	m_flGlobalShotDelay = 0.0f;

	m_flDeferUntil = gpGlobals->curtime;

	m_bRadiusAttack = false;
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_RangedAttackBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( V_stricmp( szKeyName, "minRange" ) == 0 )
	{
		m_flMinFiringDistance = atof( szValue );
		return true;
	}

	if ( V_stricmp( szKeyName, "maxRange" ) == 0 )
	{
		m_flMaxFiringDistance = atof( szValue );
		m_flMaxFiringDistanceSq = m_flMaxFiringDistance * m_flMaxFiringDistance;
		return true;
	}

	if ( V_stricmp( szKeyName, "rate" ) == 0 )
	{
		m_flFireRate = atof( szValue );
		return true;
	}

	if ( V_stricmp( szKeyName, "global_shot_delay" ) == 0 )
	{
		m_flGlobalShotDelay = atof( szValue );
		return true;
	}
	
	if ( V_stricmp( szKeyName, "volley_type" ) == 0 )
	{
		m_strVolleyType = szValue;
		return true;
	}
	
	if ( V_stricmp( szKeyName, "radius_attack") )
	{
		m_bRadiusAttack = ( atoi( szValue ) != 0 );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

//------------------------------------------------------------------------------
// Purpose: initializes static vars
//------------------------------------------------------------------------------
void CAI_ASW_RangedAttackBehavior::LevelInit( void )
{
	s_flGlobalShotDeferUntil = gpGlobals->curtime;
}

//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_RangedAttackBehavior::Precache( void )
{
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CAI_ASW_RangedAttackBehavior::Init()
{
	CASW_Alien *pNPC = static_cast<CASW_Alien*>( GetOuter() );
	if ( !pNPC )
		return;

	pNPC->rangeAttack1.Init( m_flMinFiringDistance, m_flMaxFiringDistance, COMBAT_COND_NO_FACING_CHECK, true );
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_RangedAttackBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

	if ( !HasCondition( COND_CAN_RANGE_ATTACK1 ) )
	{
		return false;
	}

  	if ( s_flGlobalShotDeferUntil > gpGlobals->curtime )
  	{
  		return false;
  	}
  
  	if ( m_flDeferUntil > gpGlobals->curtime )
  	{
  		return false;
  	}

	return BaseClass::CanSelectSchedule();
}


void CAI_ASW_RangedAttackBehavior::HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm )
{
	switch( eEvent )
	{
		case BEHAVIOR_EVENT_MORTAR_FIRE:
			CASW_Alien *pNPC = static_cast<CASW_Alien*>( GetOuter() );
			pNPC->LaunchMissile( m_strVolleyType, m_vMissileLocation );
			break;
	}
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_RangedAttackBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_RangedAttackBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_RangedAttackBehavior::GatherCommonConditions( )
{
	BaseClass::GatherCommonConditions();
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
bool CAI_ASW_RangedAttackBehavior::FindFiringLocation( )
{
	CBaseEntity *pBestEnt = NULL;
	float flBestDistSq = FLT_MAX;
	AIEnemiesIter_t iter;
	CBaseEntity *pEnemy;

	pEnemy = GetEnemy();

	if ( pEnemy && pEnemy->IsAlive() && GetOuter()->FVisible( pEnemy ) )
	{
		m_hTarget = pEnemy;
		UpdateTargetLocation();
		return true;
	}

	for( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst( &iter ); pEMemory != NULL; pEMemory = GetEnemies()->GetNext( &iter ) )
	{
		pEnemy = pEMemory->hEnemy;
		if ( !pEnemy || !pEnemy->IsAlive() || !GetOuter()->FVisible( pEnemy ) )
		{
			continue;
		}

		Vector vDelta = GetAbsOrigin() - pEnemy->GetAbsOrigin();

		float flLenSq = vDelta.LengthSqr();
		if ( flLenSq <= flBestDistSq )
		{
			pBestEnt = pEnemy;
			flBestDistSq = flLenSq;
		}
	}

	if ( !pBestEnt )
	{
		// just try shooting at our enemy for now
		if ( pEnemy && pEnemy->IsAlive() )
		{
			UpdateTargetLocation();
			return true;
		}
		return false;
	}

	m_hTarget = pBestEnt;
	GetOuter()->SetEnemy( pBestEnt );
	UpdateTargetLocation();
	return true;
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CAI_ASW_RangedAttackBehavior::UpdateTargetLocation( )
{
	if ( m_hTarget )
	{
		m_vMissileLocation = GetOuter()->GetEnemies()->LastSeenPosition( m_hTarget );
	}
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
bool CAI_ASW_RangedAttackBehavior::ValidateMissileLocation( )
{
	// Don't bother checking the for a radial attack.
	if ( !m_bRadiusAttack )
	{
		trace_t		tr;
		Vector		vStart = GetAbsOrigin() + Vector( 0.0f, 0.0f, 15.0f );
		Vector		vFinal = m_vMissileLocation + Vector( 0.0f, 0.0f, 15.0f );

#ifdef DRAW_DEBUG
		UTIL_AddDebugLine( vStart, vFinal, true, true );
#endif	// #ifdef DRAW_DEBUG

		UTIL_TraceHull( vStart, vFinal, -Vector(2,2,2), Vector(2,2,2), MASK_SOLID, GetOuter(), ASW_COLLISION_GROUP_IGNORE_NPCS, &tr );
		if ( tr.fraction != 1.0f )
		{
			return false;
		}
	}

	s_flGlobalShotDeferUntil = gpGlobals->curtime + m_flGlobalShotDelay;
	return true;
}


//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_RangedAttackBehavior::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_RANGED_FIND_MISSILE_LOCATION:
			{
				if ( !FindFiringLocation() )
				{
					m_flDeferUntil = gpGlobals->curtime + 1.0f;
					TaskFail( "Failed to FindFiringLocation" );
				}
				else
				{
					// TODO: Fix this!
					//  It's too late to fail here, as we've already picked this behavior and schedule.
					//  If we're in a bad spot for firing, we need to detect it earlier and pick a behavior that will move us.
					//  That behavior needs to be aware of bad firing spots too and factor that into its spot finding.

// 					if ( !ValidateMissileLocation() )
// 					{
// 						m_flDeferUntil = gpGlobals->curtime + 1.0f;
// 						TaskFail( "Failed to ValidateMissileLocation" );
// 					}
				}
				break;
			}

		case TASK_RANGED_PREPARE_TO_FIRE:
			{
				GetOuter()->SetIdealActivity( ACT_PREP_TO_FIRE );
				break;
			}

		case TASK_RANGED_FIRE:
			{
				GetOuter()->SetIdealActivity( ACT_FIRE );
				break;
			}

		case TASK_RANGED_FIRE_RECOVER:
			{
				GetOuter()->SetIdealActivity( ACT_FIRE_RECOVER );
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
void CAI_ASW_RangedAttackBehavior::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_RANGED_FIND_MISSILE_LOCATION:
			{
				TaskComplete();
				m_flDeferUntil = gpGlobals->curtime + m_flFireRate;
				break;
			}

		case TASK_RANGED_PREPARE_TO_FIRE:
			{
				if ( !GetOuter()->FInAimCone( m_vMissileLocation ) )
				{
					GetMotor()->SetIdealYawToTargetAndUpdate( m_vMissileLocation, AI_KEEP_YAW_SPEED );
				}

				if ( GetOuter()->IsActivityFinished() )
				{
					TaskComplete();
				}
				break;
			}

		case TASK_RANGED_FIRE:
			{
				if ( GetOuter()->IsActivityFinished() )
				{
					TaskComplete();
					GetOuter()->OnRangeAttack1();
				}
				break;
			}

		case TASK_RANGED_FIRE_RECOVER:
			{
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
int CAI_ASW_RangedAttackBehavior::SelectSchedule()
{
	return SCHED_RANGED_MOVE_TO_FIRE_SPOT;
}


AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_RangedAttackBehavior )

	DECLARE_TASK( TASK_RANGED_FIND_MISSILE_LOCATION )
	DECLARE_TASK( TASK_RANGED_PREPARE_TO_FIRE )
	DECLARE_TASK( TASK_RANGED_FIRE )
	DECLARE_TASK( TASK_RANGED_FIRE_RECOVER )

	DEFINE_SCHEDULE
	(
		SCHED_RANGED_MOVE_TO_FIRE_SPOT,
		"	Tasks"
		"		TASK_RANGED_FIND_MISSILE_LOCATION		0"
		"		TASK_RANGED_PREPARE_TO_FIRE				0"
		"		TASK_RANGED_FIRE						0"
		"		TASK_RANGED_FIRE_RECOVER				0"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
		"		COND_ENEMY_DEAD"
		"		COND_ENEMY_UNREACHABLE"
		"		COND_LOST_ENEMY"
	);

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
