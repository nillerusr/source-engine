//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_heal_other.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "asw_alien.h"
#include "particle_parse.h"
#include "particles/particles.h"
#include "asw_director.h"
#include "asw_shaman.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define GROUNDTURRET_BEAM_SPRITE "materials/swarm/effects/greenlaser1.vmt"
static int		s_pHealEffectIndex;

BEGIN_DATADESC( CAI_ASW_HealOtherBehavior )
	DEFINE_FIELD( m_flHealDistance, FIELD_FLOAT ),
	DEFINE_FIELD( m_flHealAmount, FIELD_FLOAT ),
	DEFINE_FIELD( m_hHealTargetEnt, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flDeferUntil, FIELD_TIME ),
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_HealOtherBehavior );

ConVar asw_shaman_aim_ahead_time( "asw_shaman_aim_ahead_time", "1.0", FCVAR_CHEAT, "Look ahead time for shaman's heal target" );

//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_HealOtherBehavior::CAI_ASW_HealOtherBehavior( )
{
	m_flHealDistance = 300.0f;
	m_flApproachDistance = 100.0f;
	m_flHealAmount = 0.10f;
	m_flHealConsiderationDistance = 600.0f;
	m_flHealConsiderationDistanceSquared = m_flHealConsiderationDistance * m_flHealConsiderationDistance;
	m_flDeferUntil = gpGlobals->curtime;
	m_bIsHealing = false;
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_HealOtherBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( V_stricmp( szKeyName, "heal_distance" ) == 0 )
	{
		m_flHealDistance = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "approach_distance" ) == 0 )
	{
		m_flApproachDistance = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "heal_amount" ) == 0 )
	{
		m_flHealAmount = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "consideration_distance" ) == 0 )
	{
		m_flHealConsiderationDistance = atof( szValue );
		m_flHealConsiderationDistanceSquared = m_flHealConsiderationDistance * m_flHealConsiderationDistance;
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_HealOtherBehavior::Precache( void )
{
	BaseClass::Precache();

	CBaseEntity::PrecacheModel( GROUNDTURRET_BEAM_SPRITE );
//	PRECACHE_INDEX( PARTICLE_SYSTEM, "impact_puddle_pring", s_pHealEffectIndex );
	PrecacheParticleSystem( "heal_giver" );
	PrecacheParticleSystem( "heal_receiver" );
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_HealOtherBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

	if ( HasCondition( COND_HEAL_OTHER_NOT_HAS_TARGET ) )
	{
		return false;
	}

	if ( m_flDeferUntil > gpGlobals->curtime )
	{
		return false;
	}

	return BaseClass::CanSelectSchedule();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_HealOtherBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();

	ClearCondition( COND_HEAL_OTHER_HAS_FULL_HEALTH );		// needed?

	if ( GetTarget() != NULL )
	{
		if ( GetTarget()->m_iHealth == GetTarget()->m_iMaxHealth )
		{
			SetCondition( COND_HEAL_OTHER_HAS_FULL_HEALTH );
		}
	}
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_HealOtherBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_HealOtherBehavior::GatherCommonConditions( )
{
	BaseClass::GatherCommonConditions();

//	ClearCondition( COND_HEAL_OTHER_HAS_TARGET );
	SetCondition( COND_HEAL_OTHER_NOT_HAS_TARGET );
	ClearCondition( COND_HEAL_OTHER_TARGET_CHANGED );
	ClearCondition( COND_HEAL_OTHER_TARGET_IN_RANGE );
	ClearCondition( COND_HEAL_OTHER_NOT_TARGET_IN_RANGE );

	CBaseEntity					*pBestObject = NULL;
	int							nPotentialHealthBenefit = 0;

	for( int i = 0; i < GetOuter()->GetNumFactions(); i++ )
	{
		if ( GetOuter()->GetFactionRelationshipDisposition( i ) == D_LIKE )
		{
			CUtlVector< EHANDLE >	*pEntityList = GetOuter()->GetEntitiesInFaction( i );
			for( int j = 0; j < pEntityList->Count(); j++ )
			{
				CBaseEntity	*pEntity = pEntityList->Element( j );
				if ( pEntity->GetHealth() <= 0 )
					continue;
				
				Class_T entClass = pEntity->Classify();
 				if ( entClass == CLASS_ASW_SHAMAN || entClass == CLASS_ASW_BOOMER || entClass == CLASS_ASW_BOOMERMINI
						|| entClass == CLASS_ASW_PARASITE || entClass == CLASS_ASW_BLOB || entClass == CLASS_ASW_BUZZER
						|| entClass == CLASS_ASW_RUNNER )
 				{
 					continue;
 				}

				if ( GetOuter() != pEntity )
				{
					Vector	vDiff = pEntity->GetAbsOrigin() - GetAbsOrigin();
					float	flLengthSquared = vDiff.LengthSqr();

					if ( flLengthSquared <= m_flHealConsiderationDistanceSquared )
						//&& ( !asw_shaman_only_heal_hurt_pEntity->m_iHealth < pEntity->m_iMaxHealth )
					{
						if ( nPotentialHealthBenefit == 0 || ( pEntity->m_iMaxHealth - pEntity->m_iHealth ) > nPotentialHealthBenefit )
						{
							nPotentialHealthBenefit = ( pEntity->m_iMaxHealth - pEntity->m_iHealth );
							pBestObject = pEntity;
						}	
					}
				}
			}
		}
	}

	if ( pBestObject )
	{
		if ( m_hHealTargetEnt != NULL && m_hHealTargetEnt != pBestObject )
		{
			SetCondition( COND_HEAL_OTHER_TARGET_CHANGED );
		}
//		SetCondition( COND_HEAL_OTHER_HAS_TARGET );
		ClearCondition( COND_HEAL_OTHER_NOT_HAS_TARGET );

		m_hHealTargetEnt = pBestObject;
		
		if ( m_hHealTargetEnt.Get() )
		{
			Vector vPredictedPos = m_hHealTargetEnt->GetAbsOrigin() + m_hHealTargetEnt->GetAbsVelocity() * asw_shaman_aim_ahead_time.GetFloat();
			Vector	vDiff = vPredictedPos - GetAbsOrigin();
			if ( vDiff.Length() < m_flHealDistance )
			{
				SetCondition( COND_HEAL_OTHER_TARGET_IN_RANGE );
			}
			else
			{
				SetCondition( COND_HEAL_OTHER_NOT_TARGET_IN_RANGE );
			}
		}
	}
	else
	{
		if ( m_hHealTargetEnt != NULL && m_hHealTargetEnt != pBestObject )
		{
			SetCondition( COND_HEAL_OTHER_TARGET_CHANGED );
		}

		m_hHealTargetEnt = NULL;
	}
}


//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_HealOtherBehavior::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_HEAL_OTHER_FIND_TARGET:
			{
				if ( m_hHealTargetEnt )
				{
					AI_NavGoal_t goal( GOALTYPE_TARGETENT, ACT_RUN, m_flApproachDistance, AIN_YAW_TO_DEST | AIN_UPDATE_TARGET_POS, m_hHealTargetEnt );
					SetTarget( m_hHealTargetEnt );
					GetNavigator()->SetArrivalDistance( m_flApproachDistance );
					GetNavigator()->SetGoal( goal );
				}
				else
				{
					TaskFail( FAIL_NO_TARGET );
					m_flDeferUntil = gpGlobals->curtime + 3.0f;
				}
				break;
			}

		case TASK_HEAL_OTHER_MOVE_TO_TARGET:
			{
				break;
			}

		default:
			BaseClass::StartTask( pTask );
			break;
	}
}


void CAI_ASW_HealOtherBehavior::RunTask( const Task_t *pTask )
{	
	switch( pTask->iTask )
	{
		case TASK_HEAL_OTHER_MOVE_TO_TARGET:
			{
				if ( GetNavigator()->GetGoalType() == GOALTYPE_NONE )
				{
					TaskComplete();
				}

				if ( m_hHealTargetEnt.Get() )
				{
					Vector	vDiff = m_hHealTargetEnt->GetAbsOrigin() - GetAbsOrigin();
					if ( vDiff.LengthSqr() <=  ( m_flApproachDistance * m_flApproachDistance ) )
					{
						GetNavigator()->StopMoving( true );
						GetNavigator()->ClearGoal();
						TaskComplete();
					}
				}
				break;
			}

		default:
			BaseClass::RunTask( pTask );
			break;
	}
}

CBaseEntity* CAI_ASW_HealOtherBehavior::GetCurrentHealTarget()
{
	if ( m_hHealTargetEnt.Get() )
	{
		Vector	vDiff = m_hHealTargetEnt->GetAbsOrigin() - GetAbsOrigin();
		if ( vDiff.Length() <= m_flHealDistance )
				//&& GetOuter()->FInAimCone( m_hHealTargetEnt->BodyTarget( GetOuter()->WorldSpaceCenter() ) ) 
		{
			return m_hHealTargetEnt.Get();
		}
	}
	return NULL;
}


//------------------------------------------------------------------------------
// Purpose: routine called to select what schedule we want to run
// Output : returns the schedule id of the schedule we want to run
//------------------------------------------------------------------------------
int CAI_ASW_HealOtherBehavior::SelectSchedule()
{
	if ( HasCondition( COND_HEAL_OTHER_NOT_HAS_TARGET ) || HasCondition( COND_HEAL_OTHER_HAS_FULL_HEALTH ) || HasCondition( COND_HEAL_OTHER_NOT_TARGET_IN_RANGE ) || HasCondition( COND_HEAL_OTHER_TARGET_CHANGED ) )
	{
		return SCHED_HEAL_OTHER_MOVE_TO_CANDIDATE;
	}
	else
	{
		return SCHED_HEAL_WAIT;
	}

//	return BaseClass::SelectSchedule();
}




AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_HealOtherBehavior )

	DECLARE_TASK( TASK_HEAL_OTHER_FIND_TARGET )
	DECLARE_TASK( TASK_HEAL_OTHER_MOVE_TO_TARGET )

//	DECLARE_CONDITION( COND_HEAL_OTHER_HAS_TARGET )
	DECLARE_CONDITION( COND_HEAL_OTHER_NOT_HAS_TARGET )
	DECLARE_CONDITION( COND_HEAL_OTHER_HAS_FULL_HEALTH )
	DECLARE_CONDITION( COND_HEAL_OTHER_TARGET_CHANGED )
	DECLARE_CONDITION( COND_HEAL_OTHER_TARGET_IN_RANGE )
	DECLARE_CONDITION( COND_HEAL_OTHER_NOT_TARGET_IN_RANGE )

	//===============================================
	//===============================================
	DEFINE_SCHEDULE
	(
		 SCHED_HEAL_OTHER_MOVE_TO_CANDIDATE,
		 "	Tasks"
		 "		TASK_HEAL_OTHER_FIND_TARGET			0"
		 "		TASK_HEAL_OTHER_MOVE_TO_TARGET		0"
//		 "		TASK_RUN_PATH						0"
//		 "		TASK_WAIT_FOR_MOVEMENT				0"
//		 "		TASK_SET_SCHEDULE					SCHEDULE:SCHED_HEAL_OTHER_DO_HEAL"
		 ""
		 "	Interrupts"
		 ""
		 "		COND_ENEMY_DEAD"
		 "		COND_HEAL_OTHER_TARGET_CHANGED"
//		 "		COND_HEAL_OTHER_TARGET_IN_RANGE"
	);

	DEFINE_SCHEDULE
	(
		SCHED_HEAL_WAIT,
		"	Tasks"
		//"		TASK_HEAL_OTHER_DO_HEAL				0"
		//"		TASK_SET_ACTIVITY					ACTIVITY:ACT_IDLE"
		"		TASK_WAIT							0.1"
		""
		"	Interrupts"
		""
//		"		COND_HEAL_OTHER_HAS_FULL_HEALTH"
//		"		COND_HEAL_OTHER_TARGET_CHANGED"
//		"		COND_HEAL_OTHER_NOT_HAS_TARGET"
//		"		COND_HEAL_OTHER_NOT_TARGET_IN_RANGE"
	);

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
