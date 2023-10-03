//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: NPC does a pounce attack against his enemy when close enough.  Uses MeleeAttack2Conditions.
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_pounce.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"
#include "asw_marine.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_PounceBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_PounceBehavior );

Activity ACT_ALIEN_POUNCE;
Activity ACT_ALIEN_POUNCE_HIT;
Activity ACT_ALIEN_POUNCE_MISS;

#define POUNCE_THINK_CONTEXT			"PounceThinkContext"

extern ConVar sv_gravity;

//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_PounceBehavior::CAI_ASW_PounceBehavior( )
{
	m_flMinRange = 0.0f;
	m_flMaxRange = 100.0f;
	m_flAttackDotAngle = 0.7f;
	m_flPounceInterval = 0.0f;
	m_flNextPounceTime = 0.0f;
	m_flPounceDistance = 300.0f;
	m_flPounceDamage = 15.0f;
	m_flIgnoreWorldCollisionTime = 0.0f;
	m_bMidPounce = false;
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_PounceBehavior::KeyValue( const char *szKeyName, const char *szValue )
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
	else if ( V_stricmp( szKeyName, "pounce_interval" ) == 0 )
	{
		m_flPounceInterval = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "pounce_distance" ) == 0 )
	{
		m_flPounceDistance = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "pounce_damage" ) == 0 )
	{
		m_flPounceDamage = atof( szValue );
		return true;
	}
	
	
	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_PounceBehavior::Precache( void )
{
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CAI_ASW_PounceBehavior::Init( )
{
	CASW_Alien *pNPC = static_cast< CASW_Alien * >( GetOuter() );
	if ( !pNPC )
	{
		return;
	}

	pNPC->meleeAttack2.Init( m_flMinRange, m_flMaxRange, m_flAttackDotAngle, true );
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_PounceBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

	if ( gpGlobals->curtime < m_flNextPounceTime )
	{
		return false;
	}

	if ( !HasCondition( COND_CAN_MELEE_ATTACK2 ) && !HasCondition( COND_TOO_CLOSE_TO_ATTACK ) )
	{
		return false;
	}

	return BaseClass::CanSelectSchedule();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_PounceBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_PounceBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_PounceBehavior::GatherCommonConditions( )
{
	BaseClass::GatherCommonConditions();
}


void CAI_ASW_PounceBehavior::BeginScheduleSelection( )
{
	SetBehaviorParam( m_StatusParm, -1 );
}

//------------------------------------------------------------------------------
// Purpose: routine called to select what schedule we want to run
// Output : returns the schedule id of the schedule we want to run
//------------------------------------------------------------------------------
int CAI_ASW_PounceBehavior::SelectSchedule()
{
	if ( HasCondition(COND_TOO_CLOSE_TO_ATTACK) ) 
		return SCHED_BACK_AWAY_FROM_ENEMY;

	m_flNextPounceTime = gpGlobals->curtime + m_flPounceInterval;

	return SCHED_ALIEN_POUNCE;
}


//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
void CAI_ASW_PounceBehavior::HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm )
{
	switch( eEvent )
	{
		case BEHAVIOR_EVENT_POUNCE:
		{
			CASW_Alien	*pOwner = static_cast< CASW_Alien * >( GetOuter() );

			if ( pOwner->GetEnemy() )
			{
				// jump straight ahead
				Vector vecForward;
				AngleVectors( pOwner->EyeAngles(), &vecForward );
				vecForward.z = 0;
				vecForward.NormalizeInPlace();
				PounceAttack( false, GetAbsOrigin() + vecForward * m_flPounceDistance );
			}
		}
		break;
	}
}

#define ALIEN_IGNORE_WORLD_COLLISION_TIME 0.5

//-----------------------------------------------------------------------------
// Purpose: Does a jump attack at the given position.
// Input  : bRandomJump - Just hop in a random direction.
//			vecPos - Position to jump at, ignored if bRandom is set to true.
//			bThrown - 
//-----------------------------------------------------------------------------
void CAI_ASW_PounceBehavior::PounceAttack( bool bRandomJump, const Vector &vecPos, bool bThrown, float flBaseHeight, float flAdditionalHeight )
{
	Vector vecJumpVel;
	if ( !bRandomJump )
	{
		float gravity = sv_gravity.GetFloat();
		if ( gravity <= 1 )
		{
			gravity = 1;
		}

		// How fast does the headcrab need to travel to reach the position given gravity?
		float flActualHeight = vecPos.z - GetAbsOrigin().z;
		float height = flActualHeight;
		if ( height < 16 )
		{
			height = flBaseHeight; //60; //16;
		}
		else
		{
			float flMaxHeight = bThrown ? 400 : 120;
			if ( height > flMaxHeight )
			{
				height = flMaxHeight;
			}
		}

		// overshoot the jump by an additional 8 inches
		// NOTE: This calculation jumps at a position INSIDE the box of the enemy (player)
		// so if you make the additional height too high, the crab can land on top of the
		// enemy's head.  If we want to jump high, we'll need to move vecPos to the surface/outside
		// of the enemy's box.

		float additionalHeight = 0;
		if ( height < 32 )
		{
			additionalHeight = flAdditionalHeight;
		}

		height += additionalHeight;

		// NOTE: This equation here is from vf^2 = vi^2 + 2*a*d
		float speed = sqrt( 2 * gravity * height );
		float time = speed / gravity;

		// add in the time it takes to fall the additional height
		// So the impact takes place on the downward slope at the original height
		time += sqrt( (2 * additionalHeight) / gravity );

		// Scale the sideways velocity to get there at the right time
		VectorSubtract( vecPos, GetAbsOrigin(), vecJumpVel );
		vecJumpVel /= time;

		// Speed to offset gravity at the desired height.
		vecJumpVel.z = speed;

		// Don't jump too far/fast.
		float flJumpSpeed = vecJumpVel.Length();
		float flMaxSpeed = bThrown ? 1000.0f : 650.0f;
		if ( flJumpSpeed > flMaxSpeed )
		{
			vecJumpVel *= flMaxSpeed / flJumpSpeed;
		}
	}
	else
	{
		//
		// Jump hop, don't care where.
		//
		Vector forward, up;
		AngleVectors( GetLocalAngles(), &forward, NULL, &up );
		vecJumpVel = Vector( forward.x, forward.y, up.z ) * 350;
	}

	//AttackSound();
	Leap( vecJumpVel );
}

void CAI_ASW_PounceBehavior::Leap( const Vector &vecVel )
{
	m_bMidPounce = true;

	SetCondition( COND_FLOATING_OFF_GROUND );
	SetGroundEntity( NULL );

	m_flIgnoreWorldCollisionTime = gpGlobals->curtime + ALIEN_IGNORE_WORLD_COLLISION_TIME;

	if( HasHeadroom() )
	{
		// Take him off ground so engine doesn't instantly reset FL_ONGROUND.
		UTIL_SetOrigin( GetOuter(), GetLocalOrigin() + Vector( 0, 0, 1 ) );
	}

	GetOuter()->SetAbsVelocity( vecVel );

	// think every frame so the player sees the alien where it is

	GetOuter()->SetContextThink( &CASW_Alien::CallBehaviorThink, gpGlobals->curtime, POUNCE_THINK_CONTEXT );
}

bool CAI_ASW_PounceBehavior::HasHeadroom()
{
	trace_t tr;
	UTIL_TraceEntity( GetOuter(), GetAbsOrigin(), GetAbsOrigin() + Vector( 0, 0, 1 ), MASK_NPCSOLID, GetOuter(), GetCollisionGroup(), &tr );

	return (tr.fraction == 1.0);
}

void CAI_ASW_PounceBehavior::BehaviorThink( void )
{
	if( GetOuter()->GetFlags() & FL_ONGROUND && gpGlobals->curtime >= m_flIgnoreWorldCollisionTime )
	{
		GetOuter()->SetAbsVelocity( vec3_origin );

		GetOuter()->SetContextThink( NULL, TICK_NEVER_THINK, POUNCE_THINK_CONTEXT );
		return;
	}

	GetOuter()->SetContextThink( &CASW_Alien::CallBehaviorThink, gpGlobals->curtime, POUNCE_THINK_CONTEXT );
}

//-----------------------------------------------------------------------------
// Purpose: LeapTouch - this is the headcrab's touch function when it is in the air.
// Input  : *pOther - 
//-----------------------------------------------------------------------------

void CAI_ASW_PounceBehavior::StartTouch( CBaseEntity *pOther )
{
	BaseClass::StartTouch( pOther );

	if ( !m_bMidPounce )
		return;

	if ( !pOther->ShouldCollide( GetCollisionGroup(), GetOuter()->PhysicsSolidMaskForEntity() ) )
		return;

	if ( !g_pGameRules->ShouldCollide( GetCollisionGroup(), pOther->GetCollisionGroup() ) )
		return;

	bool bHit = false;
	if ( GetOuter()->IRelationType( pOther ) == D_HT )
	{
		if ( pOther->m_takedamage != DAMAGE_NO && !pOther->IsWorld() )
		{
			Vector vecForceDir = GetOuter()->GetAbsVelocity();
			vecForceDir.NormalizeInPlace();
			// TODO: Check they're not behind me
			CASW_Marine *pMarine = CASW_Marine::AsMarine( pOther );
			if ( pMarine )
			{
				pMarine->Stumble( GetOuter(), vecForceDir, false );
			}

			float flDamage = ASWGameRules()->ModifyAlienDamageBySkillLevel( m_flPounceDamage );
			flDamage = MAX( flDamage, 1.0f );

			CTakeDamageInfo info( GetOuter(), GetOuter(), flDamage, DMG_SLASH );
			const trace_t &touchTrace = GetOuter()->GetTouchTrace();
			CalculateMeleeDamageForce( &info, vecForceDir, touchTrace.endpos );
			pOther->TakeDamage( info );

			bHit = true;
		}
	}
	else if( !(GetOuter()->GetFlags() & FL_ONGROUND) )
	{
		// Still in the air...
		if( gpGlobals->curtime < m_flIgnoreWorldCollisionTime )
		{
			// Headcrabs try to ignore the world, static props, and friends for a 
			// fraction of a second after they jump. This is because they often brush
			// doorframes or props as they leap, and touching those objects turns off
			// this touch function, which can cause them to hit the player and not bite.
			// A timer probably isn't the best way to fix this, but it's one of our 
			// safer options at this point (sjb).
			return;
		}

		if( !pOther->IsSolid() )
		{
			// Touching a trigger or something.
			return;
		}
	}

	GetOuter()->SetAbsVelocity( vec3_origin );

	if ( bHit )
	{
		if ( GetOuter()->HaveSequenceForActivity( (Activity) ACT_ALIEN_POUNCE_HIT ) )
		{
			GetOuter()->SetActivity( (Activity) ACT_ALIEN_POUNCE_HIT );
		}
	}
	else
	{
		if ( GetOuter()->HaveSequenceForActivity( (Activity) ACT_ALIEN_POUNCE_MISS ) )
		{
			GetOuter()->SetActivity( (Activity) ACT_ALIEN_POUNCE_MISS );
		}
	}

	// make sure we're solid
	GetOuter()->RemoveSolidFlags( FSOLID_NOT_SOLID );
	
	m_bMidPounce = false;
}

AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_PounceBehavior )

	DECLARE_ACTIVITY( ACT_ALIEN_POUNCE )
	DECLARE_ACTIVITY( ACT_ALIEN_POUNCE_HIT )
	DECLARE_ACTIVITY( ACT_ALIEN_POUNCE_MISS )

	// forward pounce attack
	DEFINE_SCHEDULE
	(
		SCHED_ALIEN_POUNCE,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_FACE_ENEMY			0"
		"		TASK_ANNOUNCE_ATTACK	1"
		"		TASK_RESET_ACTIVITY		0"
		"		TASK_PLAY_SEQUENCE		ACTIVITY:ACT_ALIEN_POUNCE"

		"	Interrupts"
		"		COND_TASK_FAILED"
	)

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
