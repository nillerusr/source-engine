//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_scuttle.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"
#include "asw_director.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_ScuttleBehavior )
	DEFINE_FIELD( m_hTargetEnt, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flDeferUntil, FIELD_TIME ),
	DEFINE_FIELD( m_flPackRange, FIELD_FLOAT ),
	DEFINE_FIELD( m_flPackRangeSquared, FIELD_FLOAT ),
	DEFINE_FIELD( m_flMinBackOff, FIELD_FLOAT ),
	DEFINE_FIELD( m_flMaxBackOff, FIELD_FLOAT ),
	DEFINE_FIELD( m_flMinYawVariation, FIELD_FLOAT ),
	DEFINE_FIELD( m_flMaxYawVariation, FIELD_FLOAT ),
	DEFINE_FIELD( m_flMinTimeWait, FIELD_FLOAT ),
	DEFINE_FIELD( m_flMaxTimeWait, FIELD_FLOAT ),
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_ScuttleBehavior );


//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_ScuttleBehavior::CAI_ASW_ScuttleBehavior( )
{
	m_flDeferUntil = gpGlobals->curtime;
	m_flPackRange = 800.0f;
	m_flPackRangeSquared = m_flPackRange * m_flPackRange;
	m_flMinBackOff = 100.0f;
	m_flMaxBackOff = 150.0f;
	m_flMinYawVariation = 10.0f;
	m_flMaxYawVariation = 20.0f;
	m_flMinTimeWait = 1.25f;
	m_flMaxTimeWait = 2.0f;
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_ScuttleBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( V_stricmp( szKeyName, "pack_range" ) == 0 )
	{
		m_flPackRange = atof( szValue );
		m_flPackRangeSquared = m_flPackRange * m_flPackRange;
		return true;
	}
	else if ( V_stricmp( szKeyName, "min_backoff" ) == 0 )
	{
		m_flMinBackOff = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "max_backoff" ) == 0 )
	{
		m_flMaxBackOff = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "min_yaw" ) == 0 )
	{
		m_flMinYawVariation = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "max_yaw" ) == 0 )
	{
		m_flMaxYawVariation = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "min_wait" ) == 0 )
	{
		m_flMinTimeWait = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "max_wait" ) == 0 )
	{
		m_flMaxTimeWait = atof( szValue );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_ScuttleBehavior::Precache( void )
{
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_ScuttleBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

	if ( !HasCondition( COND_SCUTTLE_HAS_DESTINATION ) )
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
void CAI_ASW_ScuttleBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_ScuttleBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_ScuttleBehavior::GatherCommonConditions( )
{
	BaseClass::GatherCommonConditions();

	ClearCondition( COND_SCUTTLE_HAS_DESTINATION );
	ClearCondition( COND_SCUTTLE_TARGET_CHANGED );

	CBaseEntity		*pBestObject = NULL;
	bool			bFound = false;
	float			flLength = 0.0f;
	AIEnemiesIter_t iter;
	Vector			vEnemyPos;

	for( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst( &iter ); pEMemory != NULL; pEMemory = GetEnemies()->GetNext( &iter ) )
	{
		CBaseEntity *pEnemy = pEMemory->hEnemy;

		if ( !pEnemy || !pEnemy->IsAlive() )
		{
			continue;
		}

		Vector	vDiff = GetAbsOrigin() - pEMemory->vLastKnownLocation;
		float	flTestLength = vDiff.LengthSqr();

		if ( !bFound || flTestLength < flLength )
		{
			bFound = true;
			flLength = flTestLength;
			vEnemyPos = pEMemory->vLastKnownLocation;
			pBestObject = pEnemy;
		}
	}

	int					nCount = 0;
	CAI_ASW_ScuttleBehavior	*pBehavior;

	int iAliens = IAlienAutoList::AutoList().Count();
	for ( int i = 0; i < iAliens; i++ )
	{
		CASW_Alien *pAlien = static_cast< CASW_Alien* >( IAlienAutoList::AutoList()[ i ] );
		if ( !pAlien || !pAlien->IsAlive() || pAlien->GetBehavior( &pBehavior ) != NULL )
		{
			continue;
		}

		if ( GetOuter() != pAlien )
		{
			Vector	vDiffEnemy = pAlien->GetAbsOrigin() - vEnemyPos;
			float	flEnemyLength = vDiffEnemy.LengthSqr();
			Vector	vDiffUs = pAlien->GetAbsOrigin() - GetAbsOrigin();
			float	flUsLength = vDiffUs.LengthSqr();

			if ( flEnemyLength < m_flPackRangeSquared && flUsLength < m_flPackRangeSquared )
			{
				nCount++;
			}
		}
	}

	if ( pBestObject && nCount > 0 )
	{
		m_vDestination = pBestObject->GetAbsOrigin();
		SetCondition( COND_SCUTTLE_HAS_DESTINATION );

		if ( m_hTargetEnt != NULL && m_hTargetEnt != pBestObject )
		{
			SetCondition( COND_SCUTTLE_TARGET_CHANGED );
		}

		m_hTargetEnt = pBestObject;
	}
	else
	{
		if ( m_hTargetEnt != NULL && m_hTargetEnt != pBestObject )
		{
			SetCondition( COND_SCUTTLE_TARGET_CHANGED );
		}

		m_hTargetEnt = NULL;
	}
}


//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_ScuttleBehavior::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_SCUTTLE_SET_PATH:
		{
			bool			bFound = false;
			float			flLength = 0.0f;
			AIEnemiesIter_t iter;
			Vector			vEnemyPos;

			for( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst( &iter ); pEMemory != NULL; pEMemory = GetEnemies()->GetNext( &iter ) )
			{
				CBaseEntity *pEnemy = pEMemory->hEnemy;

				if ( !pEnemy || !pEnemy->IsAlive() )
				{
					continue;
				}

				Vector	vDiff = GetAbsOrigin() - pEMemory->vLastKnownLocation;
				float	flTestLength = vDiff.LengthSqr();

				if ( !bFound || flTestLength < flLength )
				{
					bFound = true;
					flLength = flTestLength;
					vEnemyPos = pEMemory->vLastKnownLocation;
				}
			}

			if ( bFound == false || flLength > m_flPackRangeSquared )
			{
				TaskFail( FAIL_NO_TARGET );
				break;
			}

			Vector				vCenter;
			int					nCount = 0;
			CAI_ASW_ScuttleBehavior	*pBehavior;

			vCenter.Zero();

			int iAliens = IAlienAutoList::AutoList().Count();
			for ( int i = 0; i < iAliens; i++ )
			{
				CASW_Alien *pAlien = static_cast< CASW_Alien* >( IAlienAutoList::AutoList()[ i ] );
				if ( !pAlien || !pAlien->IsAlive() || pAlien->GetBehavior( &pBehavior ) != NULL )
				{
					continue;
				}

				if ( GetOuter() != pAlien )
				{
					Vector	vDiffEnemy = pAlien->GetAbsOrigin() - vEnemyPos;
					float	flEnemyLength = vDiffEnemy.LengthSqr();
					Vector	vDiffUs = pAlien->GetAbsOrigin() - GetAbsOrigin();
					float	flUsLength = vDiffUs.LengthSqr();

					if ( flEnemyLength < m_flPackRangeSquared && flUsLength < m_flPackRangeSquared )
					{
						nCount++;
						vCenter += pAlien->GetAbsOrigin();
					}
				}
			}

			if ( nCount == 0 )
			{
				TaskFail( FAIL_NO_TARGET );
				break;
			}

			vCenter /= nCount;
//			NDebugOverlay::Cross3D( vCenter, -Vector(32,32,32), Vector(32,32,32), 0, 255, 0, true, 0.1f );

			Vector vDiff = vCenter - vEnemyPos;
//			flLength = vDiff.NormalizeInPlace();
			flLength = RandomFloat( m_flMinBackOff, m_flMaxBackOff );

			float targetYaw = UTIL_VecToYaw( vDiff );

			vDiff.z = 0.0f;

			float flTestYaw = targetYaw - RandomFloat( m_flMinYawVariation, m_flMaxYawVariation );
			vDiff.x = cos( DEG2RAD( flTestYaw ) );
			vDiff.y = sin( DEG2RAD( flTestYaw ) );
			Vector vPos1 = vCenter + ( vDiff * flLength );

			flTestYaw = targetYaw + RandomFloat( m_flMinYawVariation, m_flMaxYawVariation );
			vDiff.x = cos( DEG2RAD( flTestYaw ) );
			vDiff.y = sin( DEG2RAD( flTestYaw ) );
			Vector vPos2 = vCenter + ( vDiff * flLength );

			Vector vDiff1 = vPos1 - GetAbsOrigin();
			Vector vDiff2 = vPos2 - GetAbsOrigin();

			Vector vDestination;

			if ( vDiff1.LengthSqr() > vDiff2.LengthSqr() )
			{
				vDestination = vPos1;
//				NDebugOverlay::Cross3D( vPos2, -Vector(32,32,32), Vector(32,32,32), 255, 255, 0, true, 0.1f );
			}
			else
			{
				vDestination = vPos2;
//				NDebugOverlay::Cross3D( vPos1, -Vector(32,32,32), Vector(32,32,32), 255, 255, 0, true, 0.1f );
			}

//			NDebugOverlay::Cross3D( vDestination, -Vector(32,32,32), Vector(32,32,32), 255, 255, 255, true, 0.1f );

			AI_NavGoal_t goal( GOALTYPE_LOCATION, vDestination, ACT_WALK, 50.0f, AIN_YAW_TO_DEST | AIN_LOCAL_SUCCEEED_ON_WITHIN_TOLERANCE, AIN_DEF_TARGET );
			bool bResult = GetNavigator()->SetGoal( goal, AIN_NO_PATH_TASK_FAIL );

			if ( bResult == false )
			{
				flTestYaw = RandomFloat( 0.0f, 360.0f );
				vDiff.x = cos( DEG2RAD( flTestYaw ) );
				vDiff.y = sin( DEG2RAD( flTestYaw ) );
				vDestination = vCenter + ( vDiff * flLength );

				AI_NavGoal_t RandomGoal( GOALTYPE_LOCATION, vDestination, ACT_WALK, 50.0f, AIN_YAW_TO_DEST | AIN_LOCAL_SUCCEEED_ON_WITHIN_TOLERANCE, AIN_DEF_TARGET );
				bResult = GetNavigator()->SetGoal( RandomGoal, AIN_NO_PATH_TASK_FAIL );
			}

			GetNavigator()->SetMovementActivity( ACT_WALK );
			GetOuter()->SetWait( RandomFloat( m_flMinTimeWait, m_flMaxTimeWait ) );
			break;
		}

		case TASK_SCUTTLE_WAIT:
		{
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
void CAI_ASW_ScuttleBehavior::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_SCUTTLE_SET_PATH:
		{
			if ( GetNavigator()->GetGoalType() == GOALTYPE_NONE )
			{
				TaskComplete();
			}
			break;
		}

		case TASK_SCUTTLE_WAIT:
		{
			if ( GetOuter()->IsWaitFinished() )
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
int CAI_ASW_ScuttleBehavior::SelectSchedule()
{
	if ( HasCondition( COND_SCUTTLE_HAS_DESTINATION ) )
	{
		return SCHED_SCUTTLE_MOVE_NEAR_TARGET;
	}

	return BaseClass::SelectSchedule();
}


AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_ScuttleBehavior )

	DECLARE_TASK( TASK_SCUTTLE_SET_PATH )
	DECLARE_TASK( TASK_SCUTTLE_WAIT )

	DECLARE_CONDITION( COND_SCUTTLE_HAS_DESTINATION )
	DECLARE_CONDITION( COND_SCUTTLE_TARGET_CHANGED )

	//===============================================
	//===============================================
	DEFINE_SCHEDULE
	(
		 SCHED_SCUTTLE_MOVE_NEAR_TARGET,
		 "	Tasks"
		 "		TASK_SCUTTLE_SET_PATH				0"
		 "		TASK_SET_ACTIVITY					ACTIVITY:ACT_IDLE"
		 "		TASK_SCUTTLE_WAIT					0"
//		 "		TASK_WALK_PATH_TIMED				1.5"
//		 "		TASK_WAIT_FOR_MOVEMENT				0"
		 ""
		 "	Interrupts"
		 ""
//		 "		COND_SCUTTLE_TARGET_CHANGED"
	);

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
