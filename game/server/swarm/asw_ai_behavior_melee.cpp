//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: NPC does a melee attack against his enemy when close enough.  Uses MeleeAttack1Conditions.
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_melee.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"
#include "asw_marine.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_MeleeBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_MeleeBehavior );


//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_MeleeBehavior::CAI_ASW_MeleeBehavior( )
{
	m_flMinRange = 0.0f;
	m_flMaxRange = 100.0f;
	m_flAttackDotAngle = 0.7f;
	m_flMinDamage = 1.0f;
	m_flMaxDamage = 1.0f;
	m_flForce = 1.0f;
	m_flPunchVelocity = 200.0f;
	m_flPunchAngle = 45.0f;
	m_AttackHitSound = UTL_INVAL_SYMBOL;
	m_MissHitSound = UTL_INVAL_SYMBOL;
	m_bNoTurnDuringAttack = false;
	m_bKnockdown = false;
	m_flKnockdownLift = 200.0f;
	m_flKnockdownSpeed = 400.0f;
	m_bSecondaryMelee = false;
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_MeleeBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( V_stricmp( szKeyName, "min_range" ) == 0 )
	{
		m_flMinRange = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "max_range" ) == 0 )
	{
		m_flMaxRange = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "attack_dot_angle" ) == 0 )
	{
		m_flAttackDotAngle = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "min_damage" ) == 0 )
	{
		m_flMinDamage = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "max_damage" ) == 0 )
	{
		m_flMaxDamage = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "force" ) == 0 )
	{
		m_flForce = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "punch_velocity" ) == 0 )
	{
		m_flPunchVelocity = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "punch_angle" ) == 0 )
	{
		m_flPunchAngle = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "attack_hit_sound" ) == 0 )
	{
		m_AttackHitSound = GetSymbol( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "attack_miss_sound" ) == 0 )
	{
		m_MissHitSound = GetSymbol( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "allow_turning" ) == 0 )
	{
		m_bNoTurnDuringAttack = ( atoi( szValue ) == 0 );
		return true;
	}
	else if ( V_stricmp( szKeyName, "knockdown" ) == 0 )
	{
		m_bKnockdown = ( atoi( szValue ) == 1 );
		return true;
	}
	else if ( V_stricmp( szKeyName, "knockdown_speed" ) == 0 )
	{
		m_flKnockdownSpeed = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "knockdown_lift" ) == 0 )
	{
		m_flKnockdownLift = atof( szValue );
		return true;
	}	
	else if ( V_stricmp( szKeyName, "secondary_melee" ) == 0 )
	{
		m_bSecondaryMelee = ( atoi( szValue ) == 1 );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_MeleeBehavior::Precache( void )
{
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CAI_ASW_MeleeBehavior::Init( )
{
	CASW_Alien *pNPC = static_cast< CASW_Alien * >( GetOuter() );
	if ( !pNPC )
	{
		return;
	}

	pNPC->meleeAttack1.Init( m_flMinRange, m_flMaxRange, m_flAttackDotAngle, true );
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_MeleeBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

	if ( !m_bSecondaryMelee && !HasCondition( COND_CAN_MELEE_ATTACK1 ) )
	{
		return false;
	}

	if ( m_bSecondaryMelee && !HasCondition( COND_CAN_MELEE_ATTACK2 ) )
	{
		return false;
	}

	return BaseClass::CanSelectSchedule();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_MeleeBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_MeleeBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_MeleeBehavior::GatherCommonConditions( )
{
	BaseClass::GatherCommonConditions();
}


void CAI_ASW_MeleeBehavior::BeginScheduleSelection( )
{
	SetBehaviorParam( m_StatusParm, -1 );
}


//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_MeleeBehavior::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_MELEE_FLIP_AROUND:
			{
				Assert( 0 ); // NOT DONE.  See example in shield behavior: TASK_SHIELD_MAINTAIN_FLIP
#if 0
				bool	bNeedFlip = false;

				if ( HaveSequenceForActivity( ACT_SPINAROUND ) == true )
				{
					CBaseEntity *pTarget = GetEnemy();
					if ( pTarget )
					{
						float flFacing = UTIL_VecToYaw( pTarget->GetAbsOrigin() - GetAbsOrigin() );
						float flDiff = UTIL_AngleDiff( GetAbsAngles()[ YAW ], flFacing );
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
#endif
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
void CAI_ASW_MeleeBehavior::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_MELEE_FLIP_AROUND:
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
int CAI_ASW_MeleeBehavior::SelectSchedule()
{
	if ( m_bNoTurnDuringAttack == true ) 
	{
		if ( m_bSecondaryMelee )
			return SCHED_MELEE2_DO_ATTACK_NO_TURNING;
		else 
			return SCHED_MELEE1_DO_ATTACK_NO_TURNING;
	}
	else
	{
		if ( m_bSecondaryMelee )
			return SCHED_MELEE2_DO_ATTACK;
		else
			return SCHED_MELEE1_DO_ATTACK;
	}
}


//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
bool CAI_ASW_MeleeBehavior::BehaviorHandleAnimEvent( animevent_t *pEvent )
{
	int nEvent = pEvent->Event();

	if ( nEvent == AE_ALIEN_MELEE_HIT )
	{
		float flMinDamage = ASWGameRules()->ModifyAlienDamageBySkillLevel( m_flMinDamage );
		float flMaxDamage = ASWGameRules()->ModifyAlienDamageBySkillLevel( m_flMaxDamage );

		flMinDamage = ( flMinDamage < 1.0f ? 1.0f : flMinDamage );
		flMaxDamage = ( flMaxDamage < 1.0f ? 1.0f : flMaxDamage );

		HullAttack( m_flMaxRange, RandomFloat( flMinDamage, flMaxDamage ), m_flForce, m_AttackHitSound, m_MissHitSound );
		return true;
	}

/*
		case BEHAVIOR_EVENT_MELEE1_SPHERE_ATTACK:
			{
				float flMinDamage = ASWGameRules()->ModifyAlienDamageBySkillLevel( m_flMinDamage );
				float flMaxDamage = ASWGameRules()->ModifyAlienDamageBySkillLevel( m_flMaxDamage );

				flMinDamage = ( flMinDamage < 1.0f ? 1.0f : flMinDamage );
				flMaxDamage = ( flMaxDamage < 1.0f ? 1.0f : flMaxDamage );

				SphereAttack( m_flMaxRange, RandomFloat( flMinDamage, flMaxDamage ), m_flForce, m_AttackHitSound, m_MissHitSound );
				return true;
			}
			break;
			*/
	
	return false;
}


//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
void CAI_ASW_MeleeBehavior::HullAttack( float flDistance, float flDamage, float flForce, CUtlSymbol &AttackHitSound, CUtlSymbol &AttackMissSound )
{
	Vector vecForceDir;

	CBaseEntity *pHurt = GetOuter()->CheckTraceHullAttack( flDistance, -Vector( 16.0f, 16.0f, 32.0f ), Vector( 16.0f, 16.0f, 32.0f ), flDamage, DMG_SLASH, flForce );
	if ( pHurt )
	{
		SetBehaviorParam( m_StatusParm, 1 );
		// Play a random attack hit sound
		if ( AttackHitSound != UTL_INVAL_SYMBOL )
		{
			GetOuter()->EmitSound( GetSymbolText( AttackHitSound ) );
		}

		// change our sequence to one with the hit in it
		if ( !m_bSecondaryMelee && ( GetActivity() == ACT_MELEE_ATTACK1 ) )
		{
			if ( GetOuter()->HaveSequenceForActivity( (Activity) ACT_MELEE_ATTACK1_HIT ) )
			{
				SetActivity( (Activity) ACT_MELEE_ATTACK1_HIT );
			}
		}
		else if ( m_bSecondaryMelee && ( GetActivity() == ACT_MELEE_ATTACK2 ) )
		{
			if ( GetOuter()->HaveSequenceForActivity( (Activity) ACT_MELEE_ATTACK2_HIT ) )
			{
				SetActivity( (Activity) ACT_MELEE_ATTACK2_HIT );
			}
		}
		if ( m_bKnockdown && pHurt->Classify() == CLASS_ASW_MARINE )
		{
			CASW_Marine *pMarine = static_cast<CASW_Marine*>( pHurt );
			Vector vecImpulse = ( GetOuter()->BodyDirection2D() * m_flKnockdownSpeed ) + Vector( 0, 0, m_flKnockdownLift );
			pMarine->Knockdown( GetOuter(), vecImpulse );
		}
	}
	else
	{
		// Play a miss sound.
		GetOuter()->EmitSound( GetSymbolText( AttackMissSound ) );
		SetBehaviorParam( m_StatusParm, 0 );
	}
}


//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
void CAI_ASW_MeleeBehavior::SphereAttack( float flDistance, float flDamage, float flForce, CUtlSymbol &AttackHitSound, CUtlSymbol &AttackMissSound )
{
	// TODO: Fixme
	/*
	CUtlVector<CBaseEntity*> aHitList;

	CASW_Alien	*pAlien = static_cast< CASW_Alien * >( GetOuter() );
	bool bHit = pAlien->CheckSphereAttack( flDistance, flDamage, DMG_SLASH, 1.0f, aHitList );
	if ( bHit )
	{
		SetBehaviorParam( m_StatusParm, 1 );
		// Play a random attack hit sound
		GetOuter()->EmitSound( GetSymbolText( AttackHitSound ) );

		Vector vecVelocityPunch;
		AngleVectors( GetLocalAngles(), &vecVelocityPunch );
		vecVelocityPunch = vecVelocityPunch * m_flPunchVelocity;

		QAngle angViewPunch( m_flPunchAngle, random->RandomFloat( -5.0f, 5.0f ), random->RandomFloat( -5.0f, 5.0f ) );

		// Set punch angles on all players hit.
		int nHitCount = aHitList.Count();
		for ( int iHit = 0; iHit < nHitCount; ++iHit )
		{
			if ( m_bKnockdown && aHitList[iHit]->Classify() == CLASS_ASW_MARINE )
			{
				CASW_Marine *pMarine = static_cast<CASW_Marine*>( aHitList[iHit] );
				Vector vecImpulse = ( GetOuter()->BodyDirection2D() * m_flKnockdownSpeed ) + Vector( 0, 0, m_flKnockdownLift );
				pMarine->Knockdown( GetOuter(), vecImpulse );
			}
		}
	}
	else
	{
		// Play a miss sound.
		GetOuter()->EmitSound( GetSymbolText( AttackMissSound ) );
		SetBehaviorParam( m_StatusParm, 0 );
	}
	*/
}


AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_MeleeBehavior )

	DECLARE_TASK( TASK_MELEE_FLIP_AROUND )

	DEFINE_SCHEDULE
	(
		SCHED_MELEE1_DO_ATTACK,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
//		"		TASK_MELEE_FLIP_AROUND	0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_MELEE_ATTACK1		0"
		""
		"	Interrupts"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
	)

	DEFINE_SCHEDULE
	(
		SCHED_MELEE1_DO_ATTACK_NO_TURNING,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
//		"		TASK_MELEE_FLIP_AROUND	0"
		"		TASK_MELEE_ATTACK1		0"
		""
		"	Interrupts"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
	)

	DEFINE_SCHEDULE
	(
		SCHED_MELEE2_DO_ATTACK,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
//		"		TASK_MELEE_FLIP_AROUND	0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_MELEE_ATTACK2		0"
		""
		"	Interrupts"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
	)

	DEFINE_SCHEDULE
	(
		SCHED_MELEE2_DO_ATTACK_NO_TURNING,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
//		"		TASK_MELEE_FLIP_AROUND	0"
		"		TASK_MELEE_ATTACK2		0"
		""
		"	Interrupts"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
	)


AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
