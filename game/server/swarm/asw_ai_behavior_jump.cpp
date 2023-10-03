//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: NPC does a jump attack against his enemy when close enough.
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "ai_moveprobe.h"
#include "asw_ai_behavior_jump.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"
#include "asw_marine.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//Debug visualization
ConVar	asw_debug_jump_behavior( "asw_debug_jump_behavior", "0" );

BEGIN_DATADESC( CAI_ASW_JumpBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_JumpBehavior );

Activity ACT_ALIEN_JUMP;
Activity ACT_ALIEN_LAND;

extern ConVar sv_gravity;

//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_JumpBehavior::CAI_ASW_JumpBehavior( )
{
	m_bForcedStuckJump = false;
	m_bHasDoneAirAttack = false;
	m_flJumpTime = 0.0f;
	m_vecSavedJump.Init();
	m_vecLastJumpAttempt.Init();
	m_flMinRange = 512.0f;
	m_flJumpTimeMin = 2.0f;
	m_flJumpTimeMax = 6.0f;
	m_flJumpDamage = 4.0f;
	m_flDamageDistance = 100.0f;
}

//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_JumpBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( V_stricmp( szKeyName, "min_range" ) == 0 )
	{
		m_flMinRange = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "jump_delay_min" ) == 0 )
	{
		m_flJumpTimeMin = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "jump_delay_max" ) == 0 )
	{
		m_flJumpTimeMax = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "damage_distance" ) == 0 )
	{
		m_flDamageDistance = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "jump_damage" ) == 0 )
	{
		m_flJumpDamage = atof( szValue );
		return true;
	}
	
	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_JumpBehavior::Precache( void )
{
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CAI_ASW_JumpBehavior::Init( )
{
	CASW_Alien *pNPC = static_cast< CASW_Alien * >( GetOuter() );
	if ( !pNPC )
	{
		return;
	}
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_JumpBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

	if ( !ShouldJump() )
	{
		return false;
	}

	return BaseClass::CanSelectSchedule();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_JumpBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_JumpBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_JumpBehavior::GatherCommonConditions( )
{
	//New Enemy? Try to jump at him.
	if ( HasCondition( COND_NEW_ENEMY ) )
	{
		m_flJumpTime = 0.0f;
	}

	BaseClass::GatherCommonConditions();
}


void CAI_ASW_JumpBehavior::BeginScheduleSelection( )
{
	SetBehaviorParam( m_StatusParm, -1 );
}

//------------------------------------------------------------------------------
// Purpose: routine called to select what schedule we want to run
// Output : returns the schedule id of the schedule we want to run
//------------------------------------------------------------------------------
int CAI_ASW_JumpBehavior::SelectSchedule()
{
	return SCHED_ALIEN_JUMP;
}


//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
void CAI_ASW_JumpBehavior::HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm )
{
	switch( eEvent )
	{
		case BEHAVIOR_EVENT_JUMP:
		{
			if ( m_bForcedStuckJump == false )
			{
				//Don't jump if we're not on the ground
				if ( ( GetOuter()->GetFlags() & FL_ONGROUND ) == false )
					return;
			}

			//Take us off the ground
			SetGroundEntity( NULL );
			GetOuter()->SetAbsVelocity( m_vecSavedJump );

			m_bForcedStuckJump = false;
			m_bHasDoneAirAttack = false;

			//Setup our jump time so that we don't try it again too soon
			m_flJumpTime = gpGlobals->curtime + random->RandomInt( m_flJumpTimeMin, m_flJumpTimeMax );

			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:  
//-----------------------------------------------------------------------------
void CAI_ASW_JumpBehavior::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask ) 
	{
	case TASK_FACE_JUMP:
		break;

	case TASK_JUMP:
		if ( CheckLanding() )
		{
			TaskComplete();
		}

		break;

	default:
		BaseClass::StartTask( pTask );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CAI_ASW_JumpBehavior::RunTask( const Task_t *pTask )
{
	// some state that needs be set each frame
	if ( GetOuter()->GetFlags() & FL_ONGROUND )
	{
		m_bHasDoneAirAttack = false;
	}

	switch ( pTask->iTask )
	{
	case TASK_FACE_JUMP:
		{
			Vector	jumpDir = m_vecSavedJump;
			VectorNormalize( jumpDir );

			QAngle	jumpAngles;
			VectorAngles( jumpDir, jumpAngles );

			GetMotor()->SetIdealYawAndUpdate( jumpAngles[YAW], AI_KEEP_YAW_SPEED );
			GetOuter()->SetTurnActivity();

			if ( GetMotor()->DeltaIdealYaw() < 2 )
			{
				TaskComplete();
			}
		}

		break;

	case TASK_JUMP:
		if ( CheckLanding() )
		{
			TaskComplete();
		}

		break;

	default:
		BaseClass::RunTask( pTask );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAI_ASW_JumpBehavior::ShouldJump( void )
{
	if ( GetOuter()->GetEnemy() == NULL )
		return false;

	//Too soon to try to jump
	if ( m_flJumpTime > gpGlobals->curtime )
		return false;

	// only jump if you're on the ground
	if (!(GetOuter()->GetFlags() & FL_ONGROUND) || GetOuter()->GetNavType() == NAV_JUMP )
		return false;

	// Don't jump if I'm not allowed
	if ( ( CapabilitiesGet() & bits_CAP_MOVE_JUMP ) == false )
		return false;

	Vector vEnemyForward, vForward;

	GetEnemy()->GetVectors( &vEnemyForward, NULL, NULL );
	GetOuter()->GetVectors( &vForward, NULL, NULL );

	float flDot = DotProduct( vForward, vEnemyForward );

	if ( flDot < 0.5f )
		flDot = 0.5f;

	Vector vecPredictedPos;

	//Get our likely position in two seconds
	UTIL_PredictedPosition( GetEnemy(), flDot * 2.5f, &vecPredictedPos );

	// Don't jump if we're already near the target
	if ( ( GetAbsOrigin() - vecPredictedPos ).LengthSqr() < (m_flMinRange*m_flMinRange) )
		return false;

	//Don't retest if the target hasn't moved enough
	//FIXME: Check your own distance from last attempt as well
	if ( ( ( m_vecLastJumpAttempt - vecPredictedPos ).LengthSqr() ) < (128*128) )
	{
		m_flJumpTime = gpGlobals->curtime + random->RandomFloat( 1.0f, 2.0f );		
		return false;
	}

	Vector	targetDir = ( vecPredictedPos - GetAbsOrigin() );
	VectorNormalize( targetDir );

	Vector	targetPos = vecPredictedPos + ( targetDir * (GetHullWidth()*4.0f) );

	//if ( CDroneAntlionRepellant::IsPositionRepellantFree( targetPos ) == false )
	//	return false;

	// Try the jump
	AIMoveTrace_t moveTrace;
	GetOuter()->GetMoveProbe()->MoveLimit( NAV_JUMP, GetAbsOrigin(), targetPos, GetOuter()->GetAITraceMask(), GetOuter()->GetNavTargetEntity(), &moveTrace );

	//See if it succeeded
	if ( IsMoveBlocked( moveTrace.fStatus ) )
	{
		if ( asw_debug_jump_behavior.GetInt() == 2 )
		{
			NDebugOverlay::Box( targetPos, GetHullMins(), GetHullMaxs(), 255, 0, 0, 0, 5 );
			NDebugOverlay::Line( GetAbsOrigin(), targetPos, 255, 0, 0, 0, 5 );
		}

		m_flJumpTime = gpGlobals->curtime + random->RandomFloat( 1.0f, 2.0f );
		return false;
	}

	if ( asw_debug_jump_behavior.GetInt() == 2 )
	{
		NDebugOverlay::Box( targetPos, GetHullMins(), GetHullMaxs(), 0, 255, 0, 0, 5 );
		NDebugOverlay::Line( GetAbsOrigin(), targetPos, 0, 255, 0, 0, 5 );
	}

	//Save this jump in case the next time fails
	m_vecSavedJump = moveTrace.vJumpVelocity;
	m_vecLastJumpAttempt = targetPos;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Monitor the alien's jump to play the proper landing sequence
//-----------------------------------------------------------------------------
bool CAI_ASW_JumpBehavior::CheckLanding( void )
{
	trace_t	tr;
	Vector	testPos;

	//Amount of time to predict forward
	const float	timeStep = 0.1f;

	//Roughly looks one second into the future
	testPos = GetAbsOrigin() + ( GetOuter()->GetAbsVelocity() * timeStep );
	testPos[2] -= ( 0.5 * sv_gravity.GetFloat() * GetGravity() * timeStep * timeStep);

	if ( asw_debug_jump_behavior.GetInt() == 2 )
	{
		NDebugOverlay::Line( GetAbsOrigin(), testPos, 255, 0, 0, 0, 0.5f );
		NDebugOverlay::Cross3D( m_vecSavedJump, -Vector(2,2,2), Vector(2,2,2), 0, 255, 0, true, 0.5f );
	} 

	// Look below
	AI_TraceHull( GetAbsOrigin(), testPos, NAI_Hull::Mins( GetHullType() ), NAI_Hull::Maxs( GetHullType() ), GetOuter()->GetAITraceMask(), GetOuter(), COLLISION_GROUP_NONE, &tr );

	//See if we're about to contact, or have already contacted the ground
	if ( ( tr.fraction != 1.0f ) || ( GetOuter()->GetFlags() & FL_ONGROUND ) )
	{
		int	sequence = GetOuter()->SelectWeightedSequence( (Activity)ACT_ALIEN_LAND );
		if ( GetSequence() != sequence )
		{
			CASW_Alien *pNPC = static_cast< CASW_Alien * >( GetOuter() );

			pNPC->VacateStrategySlot();
			pNPC->SetIdealActivity( (Activity) ACT_ALIEN_LAND );
			//pNPC->Land();

			if ( GetEnemy() && GetEnemy()->IsPlayer()  )
			{
				CBasePlayer *pPlayer = ToBasePlayer( GetEnemy() );

				if ( pPlayer && pPlayer->IsInAVehicle() == false )
				{
					//pNPC->MeleeAttack( m_flDamageDistance, m_flJumpDamage, QAngle( 4.0f, 0.0f, 0.0f ), Vector( -250.0f, 1.0f, 1.0f ) );
				}
			}

			GetOuter()->SetAbsVelocity( GetOuter()->GetAbsVelocity() * 0.33f );
			return false;
		}

		return GetOuter()->IsActivityFinished();
	}

	return false;
}

AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_JumpBehavior )

	DECLARE_ACTIVITY( ACT_ALIEN_JUMP )
	DECLARE_ACTIVITY( ACT_ALIEN_LAND )

	DECLARE_TASK( TASK_FACE_JUMP )
	DECLARE_TASK( TASK_JUMP )

	// forward jump attack
	DEFINE_SCHEDULE
	(
		SCHED_ALIEN_JUMP,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_FACE_JUMP					0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_ALIEN_JUMP"
		"		TASK_JUMP						0"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
	)

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
