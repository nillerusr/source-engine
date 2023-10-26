//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_wander.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_WanderBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_WanderBehavior );


#define STRAFE_ANGLE_RANGE	45.0f
//#define DRAW_DEBUG	1

//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_WanderBehavior::CAI_ASW_WanderBehavior( )
{
	m_WanderType = WANDER_TYPE_NORMAL;
	m_flMinDistance = 100.0f;
	m_flMaxDistance = 150.0f;
	m_flStrafeFactor = 1.3f;
	m_flDivisions = 5;
	m_flDistanceTooCloseSq = 200.0f;
	m_flDistanceTooCloseSq *= m_flDistanceTooCloseSq;
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_WanderBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( V_stricmp( szKeyName, "type" ) == 0 )
	{
		if ( V_stricmp( szValue, "direction" ) == 0 )
		{
			m_WanderType = WANDER_TYPE_DIRECTION;
		}

		return true;
	}

	if ( V_stricmp( szKeyName, "min_distance" ) == 0 )
	{
		m_flMinDistance = atof( szValue );
		return true;
	}

	if ( V_stricmp( szKeyName, "max_distance" ) == 0 )
	{
		m_flMaxDistance = atof( szValue );
		return true;
	}

	if ( V_stricmp( szKeyName, "max_distance" ) == 0 )
	{
		m_flMaxDistance = atof( szValue );
		return true;
	}

	if ( V_stricmp( szKeyName, "strafe_factor" ) == 0 )
	{
		m_flStrafeFactor = atof( szValue );
		return true;
	}

	if ( V_stricmp( szKeyName, "tooclose" ) == 0 )
	{
		m_flDistanceTooCloseSq = atof( szValue );
		m_flDistanceTooCloseSq *= m_flDistanceTooCloseSq;
		return true;
	}
	

	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_WanderBehavior::Precache( void )
{
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_WanderBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

#if 0
	if ( GetOuter()->GetState() != NPC_STATE_COMBAT )
	{
		return false;
	}
#endif

	if ( m_WanderType == WANDER_TYPE_NORMAL && !HasCondition( COND_LOST_ENEMY ) && !HasCondition( COND_ENEMY_UNREACHABLE ) )
	{
		return false;
	}

	if ( m_WanderType == WANDER_TYPE_DIRECTION && HasCondition( COND_WANDER_ENEMY_TOO_CLOSE ) )
	{
		return false;
	}

	return BaseClass::CanSelectSchedule();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_WanderBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_WanderBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_WanderBehavior::GatherCommonConditions( )
{
	BaseClass::GatherCommonConditions();

	ClearCondition( COND_WANDER_ENEMY_TOO_CLOSE );
	
	if ( GetEnemy() )
	{
		float DistanceSq = ( GetEnemyLKP() - GetAbsOrigin() ).LengthSqr();

		if ( DistanceSq <= m_flDistanceTooCloseSq )
		{
			SetCondition( COND_WANDER_ENEMY_TOO_CLOSE );
		}
	}
}


//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_WanderBehavior::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_WANDER_DIRECTION:
			{
				trace_t		tr;
				Vector		*vGoodResults = ( Vector * )stackalloc( sizeof( Vector ) * ( int )m_flDivisions );
				int			nNumResults = 0;
				const float flSpinAmount = 360.0f / m_flDivisions;
				float		flCurrentAngle = RandomFloat( 0.0f, flSpinAmount );
				Vector		vCurrent = GetAbsOrigin();
#ifdef DRAW_DEBUG
				int			r, g, b;
#endif // #ifdef DRAW_DEBUG

				m_bCanStrafeLeft = HaveSequenceForActivity( ACT_STRAFE_LEFT );
				m_bCanStrafeRight = HaveSequenceForActivity( ACT_STRAFE_RIGHT );

				for( int i = 0; i < m_flDivisions; i++ )
				{
					Vector	vDest = vCurrent;

					float	flDistance = RandomFloat( m_flMinDistance, m_flMaxDistance );
					float	flYawDelta = UTIL_AngleMod( GetLocalAngles().y - flCurrentAngle );
					if ( m_bCanStrafeLeft && fabs( flYawDelta - 270.0f ) <= STRAFE_ANGLE_RANGE )
					{
						flDistance *= m_flStrafeFactor;
#ifdef DRAW_DEBUG
						r = 255;
						g = 255;
						b = 0;
#endif // #ifdef DRAW_DEBUG
					}
					else if ( m_bCanStrafeRight && fabs( flYawDelta - 90.0f ) <= STRAFE_ANGLE_RANGE )
					{
						flDistance *= m_flStrafeFactor;
#ifdef DRAW_DEBUG
						r = 0;
						g = 255;
						b = 255;
#endif // #ifdef DRAW_DEBUG
					}
#ifdef DRAW_DEBUG
					else
					{
						r = 255;
						g = 255;
						b = 255;
					}
#endif // #ifdef DRAW_DEBUG

					vDest.x += cos( DEG2RAD( flCurrentAngle ) ) * flDistance;
					vDest.y += sin( DEG2RAD( flCurrentAngle ) ) * flDistance;
					vDest.z += 15.0f;

#ifdef DRAW_DEBUG
					NDebugOverlay::Line( vCurrent, vDest, r, g, b, true, 4.0f );
#endif // #ifdef DRAW_DEBUG
					UTIL_TraceLine( vCurrent, vDest, MASK_SOLID, NULL, ASW_COLLISION_GROUP_IGNORE_NPCS, &tr );

					if ( tr.fraction > 0.6f )
					{
						vGoodResults[ nNumResults ] = vDest;
						nNumResults++;
					}

					flCurrentAngle = UTIL_AngleMod( flCurrentAngle + RandomFloat( flSpinAmount / 2.0f, flSpinAmount * 1.5f ) );
				}

				if ( nNumResults > 0 )
				{
					nNumResults = RandomInt( 0, nNumResults - 1 );

					float	flYaw = UTIL_VecToYaw ( vGoodResults[ nNumResults ] - GetAbsOrigin() );
					float	flYawDelta = UTIL_AngleMod( GetLocalAngles().y - flYaw );

					if ( m_bCanStrafeLeft && fabs( flYawDelta - 270.0f ) <= STRAFE_ANGLE_RANGE )
					{
						AI_NavGoal_t goal( GOALTYPE_LOCATION, vGoodResults[ nNumResults ], ACT_STRAFE_LEFT, 10.0f, AIN_LOCAL_SUCCEEED_ON_WITHIN_TOLERANCE, AIN_DEF_TARGET );
						GetNavigator()->SetGoal( goal );
					}
					else if ( m_bCanStrafeRight && fabs( flYawDelta - 90.0f ) <= STRAFE_ANGLE_RANGE )
					{
						AI_NavGoal_t goal( GOALTYPE_LOCATION, vGoodResults[ nNumResults ], ACT_STRAFE_RIGHT, 10.0f, AIN_LOCAL_SUCCEEED_ON_WITHIN_TOLERANCE, AIN_DEF_TARGET );
						GetNavigator()->SetGoal( goal );
					}
					else
					{
						AI_NavGoal_t goal( GOALTYPE_LOCATION, vGoodResults[ nNumResults ], ACT_WALK, 10.0f, AIN_YAW_TO_DEST | AIN_LOCAL_SUCCEEED_ON_WITHIN_TOLERANCE, AIN_DEF_TARGET );
						GetNavigator()->SetGoal( goal );
					}
				}

				TaskComplete();
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
void CAI_ASW_WanderBehavior::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_WANDER_DIRECTION:
			{
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
int CAI_ASW_WanderBehavior::SelectSchedule()
{
	switch( m_WanderType )
	{
		case WANDER_TYPE_NORMAL:
			{
				if ( HasCondition( COND_ENEMY_OCCLUDED ) )
				{
					return SCHED_WANDER_STANDOFF;
				}
				
				return SCHED_WANDER_MEDIUM;
			}

		case WANDER_TYPE_DIRECTION:
			{
				return SCHED_WANDER_DIRECTIONAL;
			}
	}

	return SCHED_NONE;
}


AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_WanderBehavior )

	DECLARE_TASK( TASK_WANDER_DIRECTION )

	DECLARE_CONDITION( COND_WANDER_ENEMY_TOO_CLOSE )

	//=========================================================
	// Wander around for a while so we don't look stupid. 
	// this is done if we ever lose track of our enemy.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_WANDER_MEDIUM,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_WANDER						480384" // 4 feet to 32 feet
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_STOP_MOVING				0"
		"		TASK_WAIT_PVS					0" // if the player left my PVS, just wait.
		"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_WANDER_MEDIUM" // keep doing it
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
	)

	DEFINE_SCHEDULE
	(
		SCHED_WANDER_STANDOFF,

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
	)

	DEFINE_SCHEDULE
	(
		SCHED_WANDER_DIRECTIONAL,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_WANDER_DIRECTION			0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_WAIT_PVS					0" // if the player left my PVS, just wait.
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_WANDER_ENEMY_TOO_CLOSE"
	)


	//=========================================================
	// If you fail to wander, wait just a bit and try again.
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_WANDER_FAIL,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_WAIT				1"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_WANDER_MEDIUM"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_ENEMY_DEAD"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
	)

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
