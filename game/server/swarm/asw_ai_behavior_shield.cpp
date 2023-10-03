//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_shield.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"
#include "movevars_shared.h"
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_director.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_ShieldBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_ShieldBehavior );


//#define DRAW_DEBUG	1


//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_ShieldBehavior::CAI_ASW_ShieldBehavior( )
{
	m_flDistance = 200.0f;
	m_flMaxFrozenTime = 3.0f;
	m_flSFrozenDownTime = 3.0f;
	m_flFrozenTimeReductionMultiplier = 10.0f;
	m_flFrozenTimeExpansionMultiplier = 50.0f;
	m_FrozenParm = UTL_INVAL_SYMBOL;
	m_bShieldLowering = false;
	m_bBeganFrozen = false;
	m_flStartFrozenTime = -1.0f;
	m_flEndFrozenTime = -1.0f;
	m_flStartDownTime = -1.0f;
	m_bReachedFrozenLimit = false;
	m_TurningGesture = ACT_INVALID;
	m_Freezer = NULL;
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_ShieldBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( V_stricmp( szKeyName, "distance") == 0 )
	{
		m_flDistance = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "max_frozen_time") == 0 )
	{
		m_flMaxFrozenTime = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "frozen_down_time") == 0 )
	{
		m_flSFrozenDownTime = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "frozen_param" ) == 0 )
	{
		m_FrozenParm = GetSymbol( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "frozen_time_reduction_multiplier" ) == 0 )
	{
		m_flFrozenTimeReductionMultiplier = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "frozen_time_expansion_multiplier" ) == 0 )
	{
		m_flFrozenTimeExpansionMultiplier = atof( szValue );
		return true;
	}
	
	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_ShieldBehavior::Precache( void )
{
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CAI_ASW_ShieldBehavior::Init( )
{
	CASW_Alien *pNPC = static_cast< CASW_Alien * >( GetOuter() );
	if ( !pNPC )
	{
		return;
	}

	pNPC->meleeAttack2.Init( 0.0f, m_flDistance, COMBAT_COND_NO_FACING_CHECK, true );
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_ShieldBehavior::CanSelectSchedule()
{
	if ( m_bShieldLowering )
	{
		return true;
	}

	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

	if ( !HasCondition( COND_CAN_MELEE_ATTACK2 ) )
	{
		if ( GetBehaviorParam( m_StatusParm ) == 0 )
		{
			return false;
		}
	}

	return BaseClass::CanSelectSchedule();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_ShieldBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_ShieldBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();
}


void CAI_ASW_ShieldBehavior::BeginScheduleSelection( )
{
	m_TurningGesture = ACT_INVALID;
}


void CAI_ASW_ShieldBehavior::EndScheduleSelection( )
{
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_ShieldBehavior::GatherCommonConditions( )
{
	BaseClass::GatherCommonConditions();
}


//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_ShieldBehavior::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_SHIELD_RAISE:
			GetOuter()->SetIdealActivity( ACT_SHIELD_UP );
			break;

		case TASK_SHIELD_LOWER:
			GetOuter()->SetIdealActivity( ACT_SHIELD_DOWN );
			break;

		case TASK_SHIELD_MAINTAIN:
			m_TurningGesture = ACT_INVALID;

			if ( m_flEndFrozenTime < gpGlobals->curtime )
			{
				CBaseEntity *pTarget = GetEnemy();

				if ( pTarget )
				{
					float flIdealYaw = UTIL_VecToYaw( pTarget->GetAbsOrigin() - GetLocalOrigin() );
					float flYawDiff = UTIL_AngleMod( GetLocalAngles().y - flIdealYaw );
					if ( flYawDiff > 180.0f && flYawDiff < 330.0f )
					{
						m_TurningGesture = ACT_GESTURE_TURN_LEFT45;
						GetOuter()->RestartGesture( m_TurningGesture );
						GetOuter()->SetIdealActivity( ACT_SHIELD_UP_IDLE );
					}
					else if ( flYawDiff <= 180.0f && flYawDiff > 30.0f )
					{
						m_TurningGesture = ACT_GESTURE_TURN_RIGHT45;
						GetOuter()->RestartGesture( m_TurningGesture );
						GetOuter()->SetIdealActivity( ACT_SHIELD_UP_IDLE );
					}
				}
			}

			if ( m_TurningGesture == ACT_INVALID )
			{
				if ( m_flEndFrozenTime > gpGlobals->curtime )
				{
					if ( m_bBeganFrozen == false )
					{
						m_bBeganFrozen = true;
						GetOuter()->SetIdealActivity( ACT_CROUCHING_SHIELD_UP );
					}
					else
					{
						GetOuter()->SetIdealActivity( ACT_CROUCHING_SHIELD_UP_IDLE );
					}
				}
				else
				{
					GetOuter()->SetIdealActivity( ACT_SHIELD_UP_IDLE );
				}
			}
			break;

		case TASK_SHIELD_MAINTAIN_FLIP:
			{
				bool	bNeedFlip = false;

				if ( HaveSequenceForActivity( ACT_SPINAROUND ) == true )
				{
					CBaseEntity *pTarget = GetEnemy();
					if ( pTarget )
					{
						float flIdealYaw = UTIL_VecToYaw( pTarget->GetAbsOrigin() - GetLocalOrigin() );
						float flYawDiff = UTIL_AngleMod( GetLocalAngles().y - flIdealYaw );

						if ( fabs( flYawDiff - 180.0f ) <= 40.0f )
						{
							bNeedFlip = true;
						}
					}
				}

				if ( bNeedFlip == false )
				{
					TaskComplete();
				}
				else
				{
					GetOuter()->SetIdealActivity( ACT_SPINAROUND );
				}

			}
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
void CAI_ASW_ShieldBehavior::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_SHIELD_RAISE:
			{
				CBaseEntity *pTarget = GetEnemy();

				if ( pTarget )
				{
					GetMotor()->SetIdealYawAndUpdate( pTarget->GetAbsOrigin() - GetLocalOrigin(), AI_KEEP_YAW_SPEED );
				}

				if ( GetOuter()->IsActivityFinished() )
				{
					TaskComplete();
					SetBehaviorParam( m_StatusParm, 1 );
				}
			}		
			break;

		case TASK_SHIELD_LOWER:
			if ( GetOuter()->IsActivityFinished() )
			{
				TaskComplete();
				SetBehaviorParam( m_StatusParm, 0 );
				m_bShieldLowering = false;
			}
			break;

		case TASK_SHIELD_MAINTAIN:
			{
				bool			bNoReset = false;

				if ( m_TurningGesture != ACT_INVALID )
				{
					CBaseEntity		*pTarget = GetEnemy();

					if ( pTarget )
					{
						GetMotor()->SetIdealYawAndUpdate( pTarget->GetAbsOrigin() - GetLocalOrigin(), AI_KEEP_YAW_SPEED );
					}

					if ( GetOuter()->IsPlayingGesture( m_TurningGesture ) == false )
					{
						TaskComplete();
						m_TurningGesture = ACT_INVALID;

						if ( pTarget )
						{
							float flIdealYaw = UTIL_VecToYaw( pTarget->GetAbsOrigin() - GetLocalOrigin() );
							float flYawDiff = UTIL_AngleMod( GetLocalAngles().y - flIdealYaw );
							if ( flYawDiff > 25.0f && flYawDiff < 335.0f )
							{
								bNoReset = true;
							}
						}
					}
				}
				else if ( GetOuter()->IsActivityFinished() )
				{
					TaskComplete();
				}

				if ( m_flStartFrozenTime != -1.0f && m_flEndFrozenTime < gpGlobals->curtime && bNoReset == false )
				{
					m_flStartFrozenTime = -1.0f;
					m_flStartDownTime = gpGlobals->curtime + m_flSFrozenDownTime;
					SetBehaviorParam( m_FrozenParm, 0 );
				}
			}
			break;

		case TASK_SHIELD_MAINTAIN_FLIP:
			{
				CBaseEntity *pTarget = GetEnemy();
				if ( pTarget )
				{
					GetMotor()->SetIdealYawAndUpdate( pTarget->GetAbsOrigin() - GetLocalOrigin(), AI_KEEP_YAW_SPEED );
				}

				if ( GetOuter()->IsActivityFinished() )
				{
					TaskComplete();
				}
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
int CAI_ASW_ShieldBehavior::SelectSchedule()
{
	if ( !HasCondition( COND_CAN_MELEE_ATTACK2 ) )
	{
		return SCHED_SHIELD_LOWER;
	}

	if ( GetBehaviorParam( m_StatusParm ) == 0 )
	{
		return SCHED_SHIELD_PREPARE;
	}
	else
	{
		return SCHED_SHIELD_MAINTAIN;
	}

#if 0
	if ( m_bShieldLowering == true )
	{
		return SCHED_SHIELD_LOWER;
	}

	if ( GetBehaviorParam( m_StatusParm ) == 0 )
	{
		return SCHED_SHIELD_PREPARE;
	}

	if ( HasCondition( COND_SHIELD_CAN_FLICK ) )
	{
		return SCHED_SHIELD_FLICK;
	}

	return SCHED_SHIELD_MAINTAIN;
#endif
}


//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
void CAI_ASW_ShieldBehavior::HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm )
{
	switch( eEvent )
	{
		case BEHAVIOR_EVENT_TOOK_DAMAGE:
			{
				float flPercent = ( float )nParm / ( float )GetOuter()->GetMaxHealth();
				if ( m_Freezer != pInflictor )
				{
					flPercent *= 3.0f;
				}
				m_flEndFrozenTime -= flPercent * m_flFrozenTimeReductionMultiplier;
			}
			break;

		case BEHAVIOR_EVENT_DEFLECTED_DAMAGE:
			{
				if ( m_flStartDownTime > gpGlobals->curtime )
				{
					break;
				}

				if ( m_flStartFrozenTime == -1.0f )
				{
					m_flStartFrozenTime = gpGlobals->curtime;
					m_flEndFrozenTime = m_flStartFrozenTime;
					m_bReachedFrozenLimit = false;
					SetBehaviorParam( m_FrozenParm, 1 );
					m_Freezer = pInflictor;
					m_bBeganFrozen = false;
				}
				else
				{
					m_flEndFrozenTime += gpGlobals->curtime - m_flStartFrozenTime;
/*					if ( m_bReachedFrozenLimit == false )
					{
						m_flStartFrozenTime = gpGlobals->curtime;
					}
*/
				}

				float flPercent = ( float )nParm / ( float )GetOuter()->GetMaxHealth();
				m_flEndFrozenTime += flPercent * m_flFrozenTimeExpansionMultiplier;
				if ( ( m_flEndFrozenTime - m_flStartFrozenTime ) > m_flMaxFrozenTime )
				{
					m_flEndFrozenTime = m_flStartFrozenTime + m_flMaxFrozenTime;
					m_bReachedFrozenLimit = true;
				}
//				Msg( "Time to awake: %g\n", m_flEndFrozenTime - gpGlobals->curtime );
			}
			break;
	}
}


#if 0
bool CAI_ASW_ShieldBehavior::GetBehaviorParam( BehaviorParam_t eParam, bool &bResult )
{
	switch( eParam )
	{
		case BEHAVIOR_PARAM_IS_SHIELDED:
			bResult = ( GetBehaviorParam( m_StatusParm ) == 1 );
			return true;
	}

	return BaseClass::GetBehaviorParam( eParam, bResult );
}
#endif


AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_ShieldBehavior )

	DECLARE_TASK( TASK_SHIELD_RAISE )
	DECLARE_TASK( TASK_SHIELD_MAINTAIN )
	DECLARE_TASK( TASK_SHIELD_MAINTAIN_FLIP )
	DECLARE_TASK( TASK_SHIELD_LOWER )

	DEFINE_SCHEDULE
	(
		SCHED_SHIELD_PREPARE,
		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_SHIELD_RAISE				0"
		""
		"	Interrupts"
	);

	DEFINE_SCHEDULE
	(
		SCHED_SHIELD_MAINTAIN,
		"	Tasks"
//		"		TASK_PLAY_SEQUENCE_FACE_ENEMY	ACTIVITY:ACT_SHIELD_UP_IDLE"
		"		TASK_SHIELD_MAINTAIN_FLIP		0"
		"		TASK_SHIELD_MAINTAIN			0"
		""
		"	Interrupts"
	);

	DEFINE_SCHEDULE
	(
		SCHED_SHIELD_LOWER,
		"	Tasks"
		"		TASK_SHIELD_LOWER				0"
		""
		"	Interrupts"
	);

	

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
