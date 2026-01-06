//=============================================================================//
//
// Purpose: Flying enemy with similar behaviors to the bullsquid.
//			Fires spiky projectiles at enemies.
// Author: 1upD
//
//=============================================================================//

#include "cbase.h"
#include "ai_squad.h"
#include "npcevent.h"
#include "npc_flyingpredator.h"
#include "particle_parse.h"
#include "ai_interactions.h"
#include "ai_route.h"
#include "ai_moveprobe.h"
#include "ai_hint.h"
#include "ai_link.h"
#include "ai_network.h"
#include "hl2_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sk_flyingpredator_health( "sk_flyingpredator_health", "100" );
ConVar sk_flyingpredator_dmg_dive( "sk_flyingpredator_dmg_dive", "33" );
ConVar sk_flyingpredator_dmg_explode( "sk_flyingpredator_dmg_explode", "75" );
ConVar sk_flyingpredator_spit_min_wait( "sk_flyingpredator_spit_min_wait", "0.5");
ConVar sk_flyingpredator_spit_max_wait( "sk_flyingpredator_spit_max_wait", "1");
ConVar sk_flyingpredator_radius_explode( "sk_flyingpredator_radius_explode", "128" );
ConVar sk_flyingpredator_gestation( "sk_flyingpredator_gestation", "15.0" );
ConVar sk_flyingpredator_spawn_time( "sk_flyingpredator_spawn_time", "5.0" );
ConVar sk_flyingpredator_eatincombat_percent( "sk_flyingpredator_eatincombat_percent", "1.0", FCVAR_NONE, "Below what percentage of health should stukabats eat during combat?" );
ConVar sk_flyingpredator_flyspeed( "sk_flyingpredator_flyspeed", "300.0" );

ConVar npc_flyingpredator_force_allow_fly( "npc_flyingpredator_force_allow_fly", "0", FCVAR_NONE, "Allows all flying predators to use air navigation regardless of what their keyvalue is set to. Useful for testing with stukabats that haven't been given the keyvalue yet" );

LINK_ENTITY_TO_CLASS( npc_flyingpredator, CNPC_FlyingPredator );
LINK_ENTITY_TO_CLASS( npc_stukabat, CNPC_FlyingPredator );

//=========================================================
// monster-specific tasks
//=========================================================
enum
{
	TASK_TAKEOFF = LAST_SHARED_PREDATOR_TASK + 1,
	TASK_FLY_DIVE_BOMB,
	TASK_LAND,
	TASK_START_FLYING,
	TASK_START_FALLING
};

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_TAKEOFF = LAST_SHARED_SCHEDULE_PREDATOR + 1,
	SCHED_TAKEOFF_CEILING,
	SCHED_FLY_DIVE_BOMB,
	SCHED_LAND,
	SCHED_FALL,
	SCHED_START_FLYING,
	SCHED_FLY,
	SCHED_TAKEOFF_IMMEDIATE,
	SCHED_APPROACH_ENEMY,
	SCHED_CIRCLE_ENEMY,
	SCHED_FACE_AND_DIVE_BOMB,
	SCHED_FLY_TO_EAT,
	SCHED_TAKEOFF_AND_FLY_TO_EAT,
};

//-----------------------------------------------------------------------------
// monster-specific Conditions
//-----------------------------------------------------------------------------
enum
{
	COND_FORCED_FLY	= NEXT_CONDITION + 1,
	COND_CAN_LAND,
	COND_FORCED_FALL,
	COND_CEILING_NEAR,
	COND_CAN_FLY,
	COND_FLY_STUCK,
};

//---------------------------------------------------------
// Activities
//---------------------------------------------------------
int ACT_FALL;
int ACT_IDLE_CEILING;
int ACT_LAND_CEILING;
int ACT_DIVEBOMB_START;

//---------------------------------------------------------
// Animation Events
//---------------------------------------------------------
int AE_FLYING_PREDATOR_FLAP_WINGS;
int AE_FLYING_PREDATOR_BEGIN_DIVEBOMB;

ConVar ai_debug_stukabat( "ai_debug_stukabat", "0" );

//---------------------------------------------------------
// Node filter to check for available air nodes
//---------------------------------------------------------
class CFlyingPredatorAirNodeFilter : public INearestNodeFilter
{
public:
	CFlyingPredatorAirNodeFilter( CNPC_FlyingPredator *pPredator ) { m_pPredator = pPredator; }

	bool IsValid( CAI_Node *pNode )
	{
		// Must be an air node
		if ( pNode->GetType() != NODE_AIR )
			return false;

		if (ai_debug_stukabat.GetBool())
		{
			float flDist = (pNode->GetOrigin() - m_pPredator->GetAbsOrigin()).LengthSqr();
			if (flDist >= Square( MAX_AIR_NODE_LINK_DIST ))
				NDebugOverlay::Line( pNode->GetOrigin(), m_pPredator->GetAbsOrigin(), 255, 0, 0, true, 3.0f );
			else
				NDebugOverlay::Line( pNode->GetOrigin(), m_pPredator->GetAbsOrigin(), 0, 255, 0, true, 3.0f );
		}

		// Must be close enough
		if ( (pNode->GetOrigin() - m_pPredator->GetAbsOrigin()).LengthSqr() >= Square(MAX_AIR_NODE_LINK_DIST) )
			return false;

		int nStaleLinks = 0;
		int hull = m_pPredator->GetHullType();
		for ( int i = 0; i < pNode->NumLinks(); i++ )
		{
			CAI_Link *pLink = pNode->GetLinkByIndex( i );
			if ( pLink->m_LinkInfo & ( bits_LINK_STALE_SUGGESTED | bits_LINK_OFF ) )
			{
				nStaleLinks++;
			}
			else if ( ( pLink->m_iAcceptedMoveTypes[hull] & bits_CAP_MOVE_FLY ) == 0 )
			{
				nStaleLinks++;
			}
		}

		if ( nStaleLinks && nStaleLinks == pNode->NumLinks() )
			return false;

		m_bFoundValidNode = true;
		return true;
	}

	bool ShouldContinue()
	{
		return !m_bFoundValidNode;
	}

private:
	CNPC_FlyingPredator *m_pPredator;
	bool m_bFoundValidNode;
};

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_FlyingPredator )

	DEFINE_KEYFIELD( m_tFlyState, FIELD_INTEGER, "FlyState"),

	DEFINE_FIELD( m_vecDiveBombDirection, FIELD_VECTOR ),
	DEFINE_FIELD( m_flDiveBombRollForce, FIELD_FLOAT ),

	DEFINE_KEYFIELD( m_bCanUseFlyNav, FIELD_BOOLEAN, "CanUseFlyNav" ),

	DEFINE_FIELD( m_bReachedMoveGoal, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vLastStoredOrigin, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flLastStuckCheck, FIELD_TIME ),
	DEFINE_FIELD( m_vDesiredTarget, FIELD_VECTOR ),
	DEFINE_FIELD( m_vCurrentTarget, FIELD_VECTOR ),

	DEFINE_KEYFIELD( m_AdultModelName, FIELD_MODELNAME, "adultmodel" ),
	DEFINE_KEYFIELD( m_BabyModelName, FIELD_MODELNAME, "babymodel" ),

	DEFINE_INPUTFUNC( FIELD_STRING, "ForceFlying", InputForceFlying ),
	DEFINE_INPUTFUNC( FIELD_STRING, "Fly", InputFly ),

END_DATADESC()

//=========================================================
// Spawn
//=========================================================
void CNPC_FlyingPredator::Spawn()
{
	Precache( );

	if (m_bIsBaby)
	{
		SetModel( STRING( m_BabyModelName ) );
		SetHullType( HULL_TINY );
	}
	else
	{
		SetModel( STRING( m_AdultModelName ) );
		SetHullType( HULL_WIDE_SHORT );
	}


	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );

	if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		SetBloodColor( BLOOD_COLOR_BLUE );
	}
	else
	{
		SetBloodColor( BLOOD_COLOR_GREEN );
	}
	
	SetRenderColor( 255, 255, 255, 255 );

	m_iMaxHealth		= m_bIsBaby ? sk_flyingpredator_health.GetFloat() / 4 : sk_flyingpredator_health.GetFloat();
	m_iHealth			= m_iMaxHealth;
	m_flFieldOfView		= -0.2f;	// indicates the width of this monster's forward view cone ( as a dotproduct result )
								// Stukabats have a wider FOV because in testing they tended to have too much tunnel vision.
								// It makes sense since they have six eyes, three on either side - they have a very wide view
	m_NPCState			= NPC_STATE_NONE;

	CapabilitiesClear();
	// UNDONE: Previously, stukabats used melee attack schedules to translate to the range attack.
	// With the stukabat rewrite, this is no longer necessary, and instead causes stukabats to get stuck in a divebomb loop when too close to the player.
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_RANGE_ATTACK1 /*| bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK2*/ | bits_CAP_SQUAD | bits_CAP_MOVE_JUMP );

	// Baby stukas are docile
	if (m_bIsBaby)
	{
		CapabilitiesRemove( bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_RANGE_ATTACK1 );
	}

	m_fCanThreatDisplay	= TRUE;
	m_flNextSpitTime = gpGlobals->curtime;

	BaseClass::Spawn();

	NPCInit();

	m_flDistTooFar		= 1024;

	// Reset flying state
	SetFlyingState( m_tFlyState );

	m_vLastStoredOrigin = vec3_origin;
	m_flLastStuckCheck = gpGlobals->curtime;
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_FlyingPredator::Precache()
{

	if ( GetModelName() == NULL_STRING )
	{
		SetModelName( AllocPooledString( "models/stukabat.mdl" ) );
	}

	if ( m_AdultModelName == NULL_STRING )
	{
		m_AdultModelName = GetModelName();
	}
	else
		PrecacheModel( STRING( m_AdultModelName ) );

	if ( m_BabyModelName == NULL_STRING )
	{
		m_BabyModelName = AllocPooledString( "models/stukapup.mdl" );
	}

	PrecacheModel( STRING( GetModelName() ) );
	PrecacheModel( STRING( m_BabyModelName ) );

	if (m_tEzVariant == EZ_VARIANT_RAD)
	{
		PrecacheParticleSystem( "blood_impact_blue_01" );
	}
	else
	{
		PrecacheParticleSystem( "blood_impact_yellow_01" );
	}

	// Use this gib particle system to show baby squids 'molting'
	PrecacheParticleSystem( "bullsquid_explode" );

	PrecacheScriptSound( "NPC_FlyingPredator.Idle" );
	PrecacheScriptSound( "NPC_FlyingPredator.Pain" );
	PrecacheScriptSound( "NPC_FlyingPredator.Alert" );
	PrecacheScriptSound( "NPC_FlyingPredator.Death" );
	PrecacheScriptSound( "NPC_FlyingPredator.Attack1" );
	PrecacheScriptSound( "NPC_FlyingPredator.FoundEnemy" );
	PrecacheScriptSound( "NPC_FlyingPredator.Growl" );
	PrecacheScriptSound( "NPC_FlyingPredator.TailWhip");
	PrecacheScriptSound( "NPC_FlyingPredator.Bite" );
	PrecacheScriptSound( "NPC_FlyingPredator.Eat" );
	PrecacheScriptSound( "NPC_FlyingPredator.Explode" );
	PrecacheScriptSound( "NPC_FlyingPredator.Flap" );
	BaseClass::Precache();
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
int CNPC_FlyingPredator::RangeAttack1Conditions( float flDot, float flDist )
{
	if ( flDist <= InnateRange1MaxRange() && gpGlobals->curtime >= m_flNextSpitTime)
	{
		if ( CapableOfFlyNav() )
		{
			if (flDist <= InnateRange1MinRange())
				return COND_TOO_CLOSE_TO_ATTACK;

			// Trace hull to enemy
			if (GetEnemy())
			{
				trace_t tr;
				AI_TraceHull( GetAbsOrigin(), GetEnemyLKP(), GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

				if (tr.fraction != 1.0f && tr.m_pEnt != GetEnemy())
					return COND_WEAPON_SIGHT_OCCLUDED;
			}
		}
		else
		{
			if (flDot < 0.5)
				return COND_NOT_FACING_ATTACK;
		}

		return( COND_CAN_RANGE_ATTACK1 );
	}

	return( COND_NONE );
}

//=========================================================
// Delays for next bullsquid spit time
//=========================================================
float CNPC_FlyingPredator::GetMaxSpitWaitTime( void )
{
	return sk_flyingpredator_spit_max_wait.GetFloat();
}

float CNPC_FlyingPredator::GetMinSpitWaitTime( void )
{
	return sk_flyingpredator_spit_max_wait.GetFloat();
}

//=========================================================
// At what percentage health should this NPC seek food?
//=========================================================
float CNPC_FlyingPredator::GetEatInCombatPercentHealth( void )
{
	return sk_flyingpredator_eatincombat_percent.GetFloat();
}

//=========================================================
// Overridden to make Stuka pups fearful
//=========================================================
Disposition_t CNPC_FlyingPredator::IRelationType( CBaseEntity *pTarget )
{
	Disposition_t disposition = BaseClass::IRelationType( pTarget );;

	// If this stuka hates the enemy and is a pup, run away!
	if ( disposition == D_HT && m_bIsBaby )
	{
		return D_FR;
	}

	return disposition;
}

//=========================================================
// Gather custom conditions
//=========================================================
void CNPC_FlyingPredator::GatherConditions( void )
{
	BaseClass::GatherConditions();

	ClearCondition( COND_CAN_LAND );
	ClearCondition( COND_CEILING_NEAR );

	switch (m_tFlyState)
	{
	case FlyState_Falling:
		if( CanLand() )
		{
			SetCondition( COND_CAN_LAND );
		}
		break;
	case FlyState_Walking:
		if ( CanUseFlyNav() )
		{
			SetCondition( COND_CAN_FLY );
		}
		break;
	}

	if ( m_NPCState != NPC_STATE_COMBAT && CeilingNear() )
	{
		DevMsg( "%s - Ceiling is near, setting COND_CEILING_NEAR\n", GetDebugName() );
		SetCondition( COND_CEILING_NEAR );
	}
}

bool CNPC_FlyingPredator::CanLand() {
	trace_t tr;
	Vector checkPos = WorldSpaceCenter() - Vector( 0, 0, 32 );
	AI_TraceLine( WorldSpaceCenter(), checkPos, MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );
	return tr.fraction < 1.0f || GetGroundEntity() != NULL;
}

bool CNPC_FlyingPredator::CeilingNear() {
	trace_t tr;
	Vector checkPos = WorldSpaceCenter() + Vector( 0, 0, 64 );
	AI_TraceLine( WorldSpaceCenter(), checkPos, MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );
	return tr.fraction < 1.0f;
}

// Shared activities from base predator
extern int ACT_EXCITED;
extern int ACT_DETECT_SCENT;
extern int ACT_INSPECT_FLOOR;

//=========================================================
// Translate missing activities to custom ones
//=========================================================
Activity CNPC_FlyingPredator::NPC_TranslateActivity( Activity eNewActivity )
{
	switch (m_tFlyState)
	{
	case FlyState_Dive:
		if (eNewActivity != (Activity)ACT_DIVEBOMB_START)
			return ACT_RANGE_ATTACK2;
	case FlyState_Flying:
		if ( eNewActivity == ACT_WALK || eNewActivity == ACT_RUN )
			return ACT_FLY;
		else if ( eNewActivity == ACT_IDLE )
			return ACT_HOVER;
		else if ( eNewActivity == ACT_HOP )
			return ACT_HOVER;
		else if ( eNewActivity == ACT_INSPECT_FLOOR )
			return ACT_HOVER;
		else if ( eNewActivity == ACT_EXCITED )
			return ACT_HOVER;
		break;
	case FlyState_Falling:
		return (Activity) ACT_FALL;
	case FlyState_Ceiling:
		if ( eNewActivity == ACT_LEAP )
			return (Activity) ACT_FALL;
		if ( eNewActivity == ACT_IDLE )
			return ( Activity )ACT_IDLE_CEILING;
		if (eNewActivity == ACT_LAND)
			return (Activity) ACT_LAND_CEILING;
		else if (eNewActivity == ACT_HOP)
			return (Activity) ACT_IDLE_CEILING;
		else if (eNewActivity == ACT_INSPECT_FLOOR)
			return (Activity) ACT_IDLE_CEILING;
		else if (eNewActivity == ACT_EXCITED)
			return (Activity) ACT_IDLE_CEILING;
		break;
	default:
		if ( eNewActivity == ACT_HOP )
		{
			return (Activity) ACT_DETECT_SCENT;
		}
		else if ( eNewActivity == ACT_INSPECT_FLOOR )
		{
			return (Activity) ACT_DETECT_SCENT;
		}
		else if ( eNewActivity == ACT_EXCITED )
		{
			return (Activity) ACT_DETECT_SCENT;
		}
		else if ( eNewActivity == ACT_JUMP )
		{
			return ACT_LEAP;
		}
		else if ( eNewActivity == ACT_GLIDE )
		{
			return ACT_HOVER;
		}
	}



	return BaseClass::NPC_TranslateActivity( eNewActivity );
}

//=========================================================
// Schedule translation
//=========================================================
int CNPC_FlyingPredator::TranslateSchedule( int scheduleType )
{
	switch (scheduleType)
	{
	// npc_flyingpredator treats ranged attacks as melee attacks so that the melee attack conditions can be used as intterupts
	case SCHED_MELEE_ATTACK1:
	case SCHED_MELEE_ATTACK2:
	case SCHED_RANGE_ATTACK1:
		if ( m_tFlyState == FlyState_Flying )
		{
			return SCHED_FLY_DIVE_BOMB;
		} 
		else
		{
			return SCHED_TAKEOFF;
		}
		break;
	case SCHED_TAKEOFF:
		if ( m_tFlyState == FlyState_Ceiling )
		{
			return SCHED_TAKEOFF_CEILING;
		}
		else if ( !HasCondition( COND_CAN_RANGE_ATTACK1 ) && CanUseFlyNav() )
		{
			return SCHED_TAKEOFF_IMMEDIATE;
		}
		break;
	case SCHED_PREDATOR_RUN_EAT:
	case SCHED_PRED_SNIFF_AND_EAT:
	case SCHED_PREDATOR_EAT:
		{
			if (CanUseFlyNav())
			{
				// Prefer to fly to eat, it's faster and safer
				if (m_tFlyState == FlyState_Flying)
					return SCHED_FLY_TO_EAT;
				else if (m_tFlyState == FlyState_Walking)
				{
					CSound *pScent = GetBestScent();
					if (pScent && (pScent->GetSoundOrigin()).LengthSqr() > 128.0f)
						return SCHED_TAKEOFF_AND_FLY_TO_EAT;
				}
			}
		} // Fall through
	case SCHED_PREDATOR_WALLOW:
		if ( m_tFlyState == FlyState_Ceiling || m_tFlyState == FlyState_Flying )
		{
			return SCHED_FALL;
		}
		break;
	case SCHED_PREDATOR_WANDER:
		if ( m_tFlyState == FlyState_Flying && !CanUseFlyNav() )
		{
			return SCHED_FALL;
		}
	case SCHED_PREDATOR_HURTHOP:
		{
			// Immediately take off or fly away
			if (m_tFlyState != FlyState_Flying)
				return SCHED_TAKEOFF;
			else
				return SCHED_RUN_RANDOM;
		}
		break;
	case SCHED_CHASE_ENEMY:
	case SCHED_CHASE_ENEMY_FAILED:
	case SCHED_STANDOFF:
		if ( m_tFlyState == FlyState_Ceiling )
		{
			return SCHED_IDLE_STAND;
		}
		else if ( m_tFlyState == FlyState_Flying )
		{
			if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) && HasStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
			{
				return SCHED_FACE_AND_DIVE_BOMB;
			}
			else
			{
				// Didn't find a valid attack position
				//if (IsCurSchedule( SCHED_APPROACH_ENEMY, false ) || IsCurSchedule( SCHED_CIRCLE_ENEMY, false ))
				//	return SCHED_TAKE_COVER_FROM_ENEMY;

				// Find a direct node if we can attack
				if (HasStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) && !IsCurSchedule( SCHED_APPROACH_ENEMY, false ))
					return SCHED_APPROACH_ENEMY;

				// Circle the enemy and find a spot to divebomb
				return SCHED_CIRCLE_ENEMY;
			}
		}
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

//=========================================================
// GetSchedule 
//=========================================================
int CNPC_FlyingPredator::SelectSchedule( void )
{
	// Force clear the falling condition to make sure it doesn't interrupt again
	ClearCondition( COND_FORCED_FALL );

	if (HasCondition( COND_FORCED_FLY ))
	{
		return SCHED_TAKEOFF;
	}

	if (HasCondition( COND_CAN_LAND ) || HasCondition( COND_FLY_STUCK ))
	{
		return SCHED_LAND;
	}

	// Make sure the stukabat is in midair before spawning a baby bat
	if ( m_tFlyState != FlyState_Flying && m_bReadyToSpawn )
	{
		return SCHED_TAKEOFF;
	}

	switch (m_tFlyState)
	{
	case FlyState_Dive:
		if (HasCondition( COND_HEAVY_DAMAGE ))
		{
			return SCHED_FALL;
		}

		return SCHED_FLY_DIVE_BOMB;
	case FlyState_Falling:
		return SCHED_FALL;
	case FlyState_Ceiling:
		// If anything 'disturbs' the stukabat, fall from this perch
		if ( ( GetEnemy() != NULL && EnemyDistance( GetEnemy() ) <= 512.0f ) || HasCondition( COND_HEAR_BULLET_IMPACT ) || HasCondition( COND_HEAR_DANGER ) || HasCondition( COND_HEAR_COMBAT ) || HasCondition( COND_PREDATOR_SMELL_FOOD ) || HasCondition( COND_SMELL ) || HasCondition( COND_PREDATOR_CAN_GROW ) )
		{
			return SCHED_FALL;
		}

		return SCHED_IDLE_STAND;
	case FlyState_Walking:
		if ( HasCondition( COND_PREDATOR_CAN_GROW ) && !HasCondition( COND_PREDATOR_GROWTH_INVALID ) )
		{
			if ( GetState() == NPC_STATE_COMBAT && GetEnemy() && !HasMemory( bits_MEMORY_INCOVER ) )
			{
				PredMsg( "Baby stuka '%s' wants to grow, but is in combat. Take cover! \n" );
				return SCHED_TAKE_COVER_FROM_ENEMY;
			}

			return SCHED_PREDATOR_GROW;
		}
		
		if ( HasCondition( COND_CAN_FLY ) )
		{
			// Always try to fly when in combat
			if ( GetEnemy() )
				return SCHED_TAKEOFF;
		}

		break;

	case FlyState_Flying:
		if ( HasCondition( COND_CEILING_NEAR ) && GetAbsVelocity().z >= 32.0f )
		{
			DevMsg( "%s - Ceiling is near, Stukabat is going to start flying immediately!\n", GetDebugName() );
			return SCHED_START_FLYING;
		}

		if ( CanUseFlyNav() && HasCondition( COND_HEAVY_DAMAGE ) )
		{
			m_flPlaybackRate = 1.25f;
			return SCHED_TAKE_COVER_FROM_ENEMY;
		}

		break;

	}

	return BaseClass::SelectSchedule();
}

//=========================================================
// GetSchedule 
//=========================================================
int CNPC_FlyingPredator::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	if (m_tFlyState == FlyState_Flying)
	{
		// Abort flight if we failed
		SetFlyingState( FlyState_Falling );
	}
	else if (taskFailCode == FAIL_NO_ROUTE && m_tFlyState == FlyState_Walking && CanUseFlyNav())
	{
		// Try flying, that's a good trick
		SetFlyingState( FlyState_Flying );
	}

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//=========================================================
// GetSchedule 
//=========================================================
void CNPC_FlyingPredator::BuildScheduleTestBits()
{
	BaseClass::BuildScheduleTestBits();

	if ( IsFlying() )
	{
		SetCustomInterruptCondition( COND_FLY_STUCK );

		if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) && HasStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ) )
		{
			if (IsCurSchedule( SCHED_APPROACH_ENEMY, false ) || IsCurSchedule( SCHED_CIRCLE_ENEMY, false ))
			{
				SetCustomInterruptCondition( COND_CAN_RANGE_ATTACK1 );
			}
		}
	}
	else if ( CapableOfFlyNav() && m_tFlyState == FlyState_Walking )
	{
		// Interrupt when we can fly
		if ( IsCurSchedule( SCHED_CHASE_ENEMY, false ) )
			SetCustomInterruptCondition( COND_CAN_FLY );
	}
}

//=========================================================
// GetSchedule 
//=========================================================
void CNPC_FlyingPredator::OnStartScene()
{
	BaseClass::OnStartScene();

	if ( IsFlying() )
	{
		// HACKHACK: Slam velocity and pitch/roll to 0
		SetAbsVelocity( vec3_origin );

		QAngle angles = GetLocalAngles();
		angles.x = 0.0f;
		angles.z = 0.0f;
		SetLocalAngles( angles );
	}
}

//=========================================================
// Start task - selects the correct activity and performs
// any necessary calculations to start the next task on the
// schedule. 
//
// Overridden for bullsquids to play specific activities
//=========================================================
void CNPC_FlyingPredator::StartTask( const Task_t *pTask )
{
	Vector vSpitPos;

	switch (pTask->iTask)
	{
	case TASK_TAKEOFF:
		SetFlyingState( FlyState_Flying );
		SetAbsOrigin( GetAbsOrigin() + Vector( 0, 0, 8 ) );
		SetAbsVelocity( Vector( 0, 0, 64.0f ) );
		TaskComplete();
		break;
	case TASK_PREDATOR_SPAWN:
		m_flNextSpawnTime = gpGlobals->curtime + sk_flyingpredator_spawn_time.GetFloat();

		GetAttachment( "Mouth", vSpitPos );
		vSpitPos -= Vector( 0, 0, 32 );
		if ( SpawnNPC( vSpitPos, true ) )
		{
			TaskComplete();
		}
		else
		{
			TaskFail( FAIL_BAD_POSITION );
		}
		break;
	case TASK_FLY_DIVE_BOMB:
	{
		if ( !HaveSequenceForActivity( (Activity)ACT_DIVEBOMB_START ) )
		{
			DiveBombAttack();
			SetIdealActivity( ACT_RANGE_ATTACK2 );
		}
		else
		{
			SetIdealActivity( (Activity)ACT_DIVEBOMB_START );
		}
		break;
	}
	case TASK_LAND:
		SetIdealActivity( ACT_LAND );
		SetFlyingState( FlyState_Landing );
		break;
	case TASK_START_FLYING:
		// Just in case we took off, set velocity to 0
		SetAbsVelocity( vec3_origin );
		SetFlyingState( FlyState_Flying );
		TaskComplete();
		break;
	case TASK_START_FALLING:
		// If already falling, do nothing
		if ( m_tFlyState != FlyState_Falling )
		{
			// Just in case we took off, set velocity to 0
			SetAbsVelocity( vec3_origin );
			SetFlyingState( FlyState_Falling );
		}
		TaskComplete();
		break;
	case TASK_PREDATOR_GROW:
		// Temporarily become non-solid so the new NPC can spawn
		SetSolid( SOLID_NONE );

		// Spawn an adult squid
		if ( !SpawnNPC( GetAbsOrigin() + Vector( 0, 0, 32 ), false ) )
		{
			TaskFail( FAIL_BAD_POSITION );
			break;
		}

		SetSolid( SOLID_BBOX );

		// Delete baby squid
		SUB_Remove();

		// Task should end after scale is applied
		TaskComplete();
		break;
	default:
	{
		BaseClass::StartTask( pTask );
		break;
	}
	}
}

//=========================================================
// RunTask
//=========================================================
void CNPC_FlyingPredator::RunTask ( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_FLY_DIVE_BOMB:
		if ( GetIdealActivity() == (Activity)ACT_DIVEBOMB_START )
		{
			if (GetEnemy())
			{
				// Face where we'll be dive bombing
				Vector vecEnemyLKP = GetEnemyLKP();
				if (!FInAimCone( vecEnemyLKP ))
				{
					GetMotor()->SetIdealYawToTarget( vecEnemyLKP );
					GetMotor()->SetIdealYaw( CalcReasonableFacing( true ) ); // CalcReasonableFacing() is based on previously set ideal yaw
				}
				else
				{
					float flReasonableFacing = CalcReasonableFacing( true );
					if ( fabsf( flReasonableFacing - GetMotor()->GetIdealYaw() ) >= 1 )
					{
						GetMotor()->SetIdealYaw( flReasonableFacing );
					}
				}

				GetMotor()->UpdateYaw();
			}

			if ( IsActivityFinished() )
				SetIdealActivity( ACT_RANGE_ATTACK2 );
		}
		else if ( m_tFlyState != FlyState_Dive )
		{
			TaskComplete();
		}

		break;
	case TASK_PREDATOR_SPAWN:
	{
		// If we fall in this case, end the task when the activity ends
		if ( IsActivityFinished() )
		{
			TaskComplete();
		}
		break;
	}
	case TASK_LAND:
		if ( IsActivityFinished() )
		{
			SetFlyingState( FlyState_Walking );
			TaskComplete();
		}
		break;
	default:
	{
		BaseClass::RunTask( pTask );
		break;
	}
	}
}

// Override OnFed() to set this stukabat ready to spawn after eating
void CNPC_FlyingPredator::OnFed() 
{
	BaseClass::OnFed(); 
	if ( !m_bIsBaby && m_bSpawningEnabled )
	{
		m_bReadyToSpawn = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_FlyingPredator::ShouldGib( const CTakeDamageInfo &info )
{
	// If the damage type is "always gib", we better gib!
	if ( info.GetDamageType() & DMG_ALWAYSGIB )
		return true;

	// Stukapups always gib
	if ( m_bIsBaby )
		return true;

	// Stukabats gib for any non-crush, non-blast damage greater than 15 (TODO: cvar?)
	if ( info.GetDamage() > 15.0f || (info.GetDamageType() & (DMG_CRUSH | DMG_BLAST) ) )
		return true;

	return IsBoss() || BaseClass::ShouldGib( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_FlyingPredator::CorpseGib( const CTakeDamageInfo &info )
{
	ExplosionEffect();

	// TODO flying predators might need unique gibs
	return BaseClass::CorpseGib( info );
}

void CNPC_FlyingPredator::ExplosionEffect( void )
{
	// Babies don't explode
	if ( m_bIsBaby )
	{
		// TODO - Replace this with a non-explosive particle
		DispatchParticleEffect( "bullsquid_explode", WorldSpaceCenter(), GetAbsAngles() );
	}
	else
	{
		// TODO - Replace this with a unique particle
		DispatchParticleEffect( "bullsquid_explode", WorldSpaceCenter(), GetAbsAngles() );

		CTakeDamageInfo info( this, this, sk_flyingpredator_dmg_explode.GetFloat(), DMG_BLAST_SURFACE | DMG_ACID | DMG_ALWAYSGIB );

		RadiusDamage( info, GetAbsOrigin(), sk_flyingpredator_radius_explode.GetFloat(), CLASS_NONE, this );
		EmitSound( "NPC_FlyingPredator.Explode" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create a new flying predator / stukabat
// Output : True if the new offspring is created
//-----------------------------------------------------------------------------
bool CNPC_FlyingPredator::SpawnNPC( const Vector position, bool isBaby )
{
	// Try to create entity
	CNPC_FlyingPredator *pChild = dynamic_cast< CNPC_FlyingPredator * >(CreateEntityByName(GetClassname()));
	if ( pChild )
	{
		pChild->m_bIsBaby = isBaby;
		pChild->m_tEzVariant = this->m_tEzVariant;
		pChild->m_tWanderState = this->m_tWanderState;
		pChild->m_bSpawningEnabled = this->m_bSpawningEnabled;
		pChild->m_bCanUseFlyNav = this->m_bCanUseFlyNav;
		pChild->m_BabyModelName = this->m_BabyModelName;
		pChild->m_AdultModelName = this->m_AdultModelName;
		pChild->m_nSkin = this->m_nSkin;
		pChild->Precache();

		DispatchSpawn( pChild );

		// Now attempt to drop into the world
		pChild->Teleport( &position, NULL, NULL );
		UTIL_DropToFloor( pChild, MASK_NPCSOLID );

		// Now check that this is a valid location for the new npc to be
		Vector	vUpBit = pChild->GetAbsOrigin();
		vUpBit.z += 1;

		trace_t tr;
		AI_TraceHull( pChild->GetAbsOrigin(), vUpBit, pChild->GetHullMins(), pChild->GetHullMaxs(),
			MASK_NPCSOLID, pChild, COLLISION_GROUP_NONE, &tr );
		if (tr.startsolid || (tr.fraction < 1.0))
		{
			pChild->SUB_Remove();
			DevMsg( "Can't create baby stukabat. Bad Position!\n" );
			return false;
		}

		pChild->SetSquad( this->GetSquad() );
		pChild->Activate();

		// Stukabat babies start falling
		if (isBaby)
		{
			pChild->SetFlyingState( FlyState_Falling );
		}

		// Gib!
		// TODO - Should have unique effects for growing!
		DispatchParticleEffect( "bullsquid_explode", pChild->WorldSpaceCenter(), pChild->GetAbsAngles() );
		EmitSound( "NPC_FlyingPredator.Explode" );

		// Decrement feeding counter
		m_iTimesFed--;
		if ( m_iTimesFed <= 0 )
		{
			m_bReadyToSpawn = false;
		}

		// Fire output
		variant_t value;
		value.SetEntity( pChild );
		m_OnSpawnNPC.CBaseEntityOutput::FireOutput( value, this, this );

		return true;
	}

	// Couldn't instantiate NPC
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this NPC can hear the specified sound
//-----------------------------------------------------------------------------
bool CNPC_FlyingPredator::QueryHearSound( CSound *pSound )
{
	// Don't smell dead stukabats
	if ( pSound->FIsScent() && IsSameSpecies( pSound->m_hTarget ) )
		return false;

	return BaseClass::QueryHearSound( pSound );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_FlyingPredator::ModifyOrAppendCriteria( AI_CriteriaSet& set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	set.AppendCriteria( "flystate", CNumStr( m_tFlyState ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_FlyingPredator::ModifyOrAppendCriteriaForPlayer( CBasePlayer *pPlayer, AI_CriteriaSet &set )
{
	BaseClass::ModifyOrAppendCriteriaForPlayer( pPlayer, set );

	set.AppendCriteria( "flystate", CNumStr( m_tFlyState ) );
}

//-----------------------------------------------------------------------------
// Purpose:  This is a generic function (to be implemented by sub-classes) to
//			 handle specific interactions between different types of characters
//			 (For example the barnacle grabbing an NPC)
// Input  :  Constant for the type of interaction
// Output :	 true  - if sub-class has a response for the interaction
//			 false - if sub-class has no response
//-----------------------------------------------------------------------------
bool CNPC_FlyingPredator::HandleInteraction( int interactionType, void *data, CBaseCombatCharacter* sourceEnt )
{
	if ( interactionType == g_interactionXenGrenadePull )
	{
		// Set flying state to "falling"
		if (m_tFlyState != FlyState_Landing && m_tFlyState != FlyState_Walking)
		{
			SetFlyingState( FlyState_Falling );
			SetCondition( COND_FORCED_FALL );
		}

		CTakeDamageInfo* damageInfo = (CTakeDamageInfo *)data;

		if ( damageInfo != NULL )
		{
			ApplyAbsVelocityImpulse( damageInfo->GetDamageForce() );
		}

		// Already handled pulling the stukabat
		return true;
	}

	// YEET - Similar to how base predator handles baby predator kicks, but for all stukabats
	if ( interactionType == g_interactionBadCopKick )
	{
		// Set the stukabat into falling mode
		// MAKE SURE the stukabat enters falling mode before any forces are applied, as falling mode resets velocity
		SetFlyingState( FlyState_Falling );
		SetCondition( COND_FORCED_FALL );

		KickInfo_t * pInfo = static_cast< KickInfo_t *>(data);
		CTakeDamageInfo * pDamageInfo = pInfo->dmgInfo;
		pDamageInfo->ScaleDamageForce( 500 );
		DispatchTraceAttack( *pDamageInfo, pDamageInfo->GetDamageForce(), pInfo->tr );
	 
		ApplyAbsVelocityImpulse( (pInfo->tr->endpos - pInfo->tr->startpos) * 500 );

		// Already handled the kick
		return true;
	}

	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_FlyingPredator::HandleAnimEvent( animevent_t *pEvent )
{
	if (pEvent->type & AE_TYPE_NEWEVENTSYSTEM)
	{
		if (pEvent->event == AE_FLYING_PREDATOR_FLAP_WINGS)
		{
			EmitSound( "NPC_FlyingPredator.Flap" );
		}
		else if (pEvent->event == AE_FLYING_PREDATOR_BEGIN_DIVEBOMB)
		{
			DiveBombAttack();
		}
		else
			BaseClass::HandleAnimEvent( pEvent );
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

//=========================================================
// Dive bomb function
//=========================================================
void CNPC_FlyingPredator::DiveBombAttack()
{
	// Pick a direction to divebomb.
	if (GetEnemy() != NULL)
	{
		// Fly towards my enemy
		Vector vEnemyPos = GetEnemyLKP();
		m_vecDiveBombDirection = vEnemyPos - GetLocalOrigin();
	}
	else
	{
		// Pick a random forward and down direction.
		Vector forward;
		GetVectors( &forward, NULL, NULL );
		m_vecDiveBombDirection = forward + Vector( random->RandomFloat( -10, 10 ), random->RandomFloat( -10, 10 ), random->RandomFloat( -20, -10 ) );
	}
	VectorNormalize( m_vecDiveBombDirection );

	// Calculate a roll force.
	m_flDiveBombRollForce = random->RandomFloat( 20.0, 420.0 );
	if (random->RandomInt( 0, 1 ))
	{
		m_flDiveBombRollForce *= -1;
	}

	m_flNextSpitTime = gpGlobals->curtime + GetMinSpitWaitTime();
	SetFlyingState( FlyState_Dive );
	AttackSound();
}

//-----------------------------------------------------------------------------
// Purpose: Switches between flying mode and ground mode.
//-----------------------------------------------------------------------------
void CNPC_FlyingPredator::SetFlyingState( FlyState_t eState )
{
	// Reset playback rate when changing state
	m_flPlaybackRate = 1.0f;

	if (eState == FlyState_Flying || eState == FlyState_Dive )
	{
		// Flying
		SetGroundEntity( NULL );
		AddFlag( FL_FLY );
		SetNavType( NAV_FLY );
		CapabilitiesRemove( bits_CAP_MOVE_GROUND | bits_CAP_MOVE_JUMP );
		CapabilitiesAdd( bits_CAP_MOVE_FLY );
		SetMoveType( MOVETYPE_STEP );

		if (eState == FlyState_Dive)
		{
			SetCollisionGroup( COLLISION_GROUP_NPC );
		}
		else
		{
			SetCollisionGroup( HL2COLLISION_GROUP_CROW );
		}
	}
	else if ( eState == FlyState_Ceiling )
	{
		// Ceiling
		SetGroundEntity( NULL );
		AddFlag( FL_FLY );
		SetNavType( NAV_FLY );
		CapabilitiesRemove( bits_CAP_MOVE_GROUND | bits_CAP_MOVE_FLY | bits_CAP_MOVE_JUMP );
		SetMoveType( MOVETYPE_STEP );
		SetCollisionGroup( COLLISION_GROUP_NPC );
	}
	else if (eState == FlyState_Walking)
	{
		// Walking
		QAngle angles = GetAbsAngles();
		angles[PITCH] = 0.0f;
		angles[ROLL] = 0.0f;
		SetAbsAngles( angles );

		RemoveFlag( FL_FLY );
		SetNavType( NAV_GROUND );
		CapabilitiesRemove( bits_CAP_MOVE_FLY );
		CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_MOVE_JUMP );
		SetMoveType( MOVETYPE_STEP );
		SetCollisionGroup( COLLISION_GROUP_NPC );
	}
	else
	{
		// Falling
		RemoveFlag( FL_FLY );
		SetNavType( NAV_GROUND );
		CapabilitiesRemove( bits_CAP_MOVE_FLY );
		CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_MOVE_JUMP );
		SetMoveType( MOVETYPE_STEP );
		SetCollisionGroup( COLLISION_GROUP_NPC );
	}

	m_tFlyState = eState;
}

//-----------------------------------------------------------------------------
// Purpose: Checks whether stukabat is capable of fly navigation
//-----------------------------------------------------------------------------
bool CNPC_FlyingPredator::CapableOfFlyNav( void )
{
	if (!m_bCanUseFlyNav && !npc_flyingpredator_force_allow_fly.GetBool())
		return false;

	if (IsBaby())
		return false;
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Checks whether stukabat can currently use fly navigation
//-----------------------------------------------------------------------------
bool CNPC_FlyingPredator::CanUseFlyNav( void )
{
	if (!CapableOfFlyNav())
		return false;

	// Needed for navigator to look for air nodes
	bool bAlreadyFlying = (CapabilitiesGet() & bits_CAP_MOVE_FLY);
	if (!bAlreadyFlying)
		CapabilitiesAdd( bits_CAP_MOVE_FLY );
	
	// Any air nodes I can use?
	CFlyingPredatorAirNodeFilter filter( this );
	int nNode = GetNavigator()->GetNetwork()->NearestNodeToPoint( this, GetAbsOrigin(), true, &filter );

	if (!bAlreadyFlying)
		CapabilitiesRemove( bits_CAP_MOVE_FLY );

	if (nNode == NO_NODE)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flInterval - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_FlyingPredator::OverrideMove( float flInterval )
{
	// ----------------------------------------------
	//	If dive bombing
	// ----------------------------------------------
	if ( m_tFlyState == FlyState_Dive )
	{
		MoveToDivebomb( flInterval );
		return true;
	}

	// ----------------------------------------------
	//	If flying
	// ----------------------------------------------
	if ( IsFlying() && CapableOfFlyNav() )
	{
		if ( IsCurSchedule( SCHED_FACE_AND_DIVE_BOMB, false ) || IsCurSchedule( SCHED_FLY_DIVE_BOMB, false ) )
		{
			// Slow down and align pitch and roll with target
			SetAbsVelocity( GetAbsVelocity() * 0.5f );

			Vector vecDir = ( GetEnemyLKP() - GetAbsOrigin() );
			QAngle angDir;
			VectorAngles( vecDir, angDir );

			QAngle angles = GetLocalAngles();
			angles.x += AngleDiff( angDir.x, angles.x ) * 0.5f;
			angles.z *= 0.5f;
			SetLocalAngles( angles );
		}
		else if ( GetNavigator()->GetPath()->GetCurWaypoint() && !m_bReachedMoveGoal )
		{
			if ( m_flLastStuckCheck <= gpGlobals->curtime )
			{
				if ( m_vLastStoredOrigin == GetAbsOrigin() )
				{
					if ( GetAbsVelocity() == vec3_origin )
					{
						SetCondition( COND_FLY_STUCK );
						return false;
					}
					else
					{
						m_vLastStoredOrigin = GetAbsOrigin();
					}
				}
				else
				{
					m_vLastStoredOrigin = GetAbsOrigin();
				}
				
				m_flLastStuckCheck = gpGlobals->curtime + 1.0f;
			}

			/*if (m_bReachedMoveGoal )
			{
				SetIdealActivity( ACT_LAND );
				SetFlyingState( FlyState_Landing );
				TaskMovementComplete();
			}
			else*/
			{
				if (GetIdealActivity() != ACT_FLY)
				{
					// Initial playback rate based on whether we're moving to attack
					if (HasStrategySlotRange( SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2 ))
						m_flPlaybackRate = 1.5f;
					else
						m_flPlaybackRate = 0.75f;
				}

				SetIdealActivity ( ACT_FLY );
				MoveFly( flInterval );
			}
		}
		else
		{
			SetIdealActivity( ACT_HOVER );

			if ( m_bReachedMoveGoal )
			{
				TaskMovementComplete();
				m_bReachedMoveGoal = false;
				SetAbsVelocity( vec3_origin );
			}
		}
		return true;
	}

	return BaseClass::OverrideMove( flInterval );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Copied from Scanner
// Input  : flInterval - 
//-----------------------------------------------------------------------------
void CNPC_FlyingPredator::MoveToDivebomb( float flInterval )
{
	float myAccel = 3200;
	float myDecay = 0.05f; // decay current velocity to 10% in 1 second

	// Fly towards my enemy
	Vector vEnemyPos = GetEnemyLKP();
	Vector vFlyDirection  = vEnemyPos - GetLocalOrigin();
	VectorNormalize( vFlyDirection );

	// Set net velocity 
	MoveInDirection( flInterval, m_vecDiveBombDirection, myAccel, myAccel, myDecay );

	// Spin out of control.
	/*Vector forward;
	VPhysicsGetObject()->LocalToWorldVector( &forward, Vector( 1.0, 0.0, 0.0 ) );
	AngularImpulse torque = forward * m_flDiveBombRollForce;
	VPhysicsGetObject()->ApplyTorqueCenter( torque );

	// BUGBUG: why Y axis and not Z?
	Vector up;
	VPhysicsGetObject()->LocalToWorldVector( &up, Vector( 0.0, 1.0, 0.0 ) );
	VPhysicsGetObject()->ApplyForceCenter( up * 2000 );*/

	// Slowly correct roll
	QAngle angles = GetLocalAngles();
	angles.z *= 0.5f;
	SetLocalAngles( angles );
}

void CNPC_FlyingPredator::MoveInDirection( float flInterval, const Vector &targetDir,
	float accelXY, float accelZ, float decay )
{
	decay = ExponentialDecay( decay, 1.0, flInterval );
	accelXY *= flInterval;
	accelZ  *= flInterval;

	Vector oldVelocity = GetAbsVelocity();
	Vector newVelocity;

	newVelocity.x = (decay * oldVelocity.x + accelXY * targetDir.x);
	newVelocity.y = (decay * oldVelocity.y + accelXY * targetDir.y);
	newVelocity.z = (decay * oldVelocity.z + accelZ  * targetDir.z);

	SetAbsVelocity( newVelocity );
}

//-----------------------------------------------------------------------------
// Purpose: Handles all flight movement.
// Input  : flInterval - Seconds to simulate.
//-----------------------------------------------------------------------------
void CNPC_FlyingPredator::MoveFly( float flInterval )
{
	//
	// Bound interval so we don't get ludicrous motion when debugging
	// or when framerate drops catastrophically.  
	//
	if (flInterval > 1.0)
	{
		flInterval = 1.0;
	}

	//
	// Determine the goal of our movement.
	//
	Vector vecMoveGoal = GetAbsOrigin();

	if ( GetNavigator()->IsGoalActive() )
	{
		vecMoveGoal = GetNavigator()->GetCurWaypointPos();

		if ( GetNavigator()->CurWaypointIsGoal() == false  )
		{
  			AI_ProgressFlyPathParams_t params( MASK_NPCSOLID );
			params.bTrySimplify = false;

			GetNavigator()->ProgressFlyPath( params ); // ignore result, crow handles completion directly

			// Fly towards the hint.
			if ( GetNavigator()->GetPath()->GetCurWaypoint() )
			{
				vecMoveGoal = GetNavigator()->GetCurWaypointPos();
			}
		}
	}
	else
	{
		// No movement goal.
		vecMoveGoal = GetAbsOrigin();
		SetAbsVelocity( vec3_origin );
		return;
	}

	Vector vecMoveDir = ( vecMoveGoal - GetAbsOrigin() );
	Vector vForward;
	AngleVectors( GetAbsAngles(), &vForward );
	
	//
	// Fly towards the movement goal.
	//
	float flDistance = ( vecMoveGoal - GetAbsOrigin() ).Length();

	if ( vecMoveGoal != m_vDesiredTarget )
	{
		m_vDesiredTarget = vecMoveGoal;
	}
	else
	{
		m_vCurrentTarget = ( m_vDesiredTarget - GetAbsOrigin() );
		VectorNormalize( m_vCurrentTarget );
	}

	float flLerpMod = 0.25f;

	if ( flDistance <= 256.0f )
	{
		flLerpMod = 1.0f - ( flDistance / 256.0f );
	}

	VectorLerp( vForward, m_vCurrentTarget, flLerpMod, vForward );

	float flFlightSpeed = sk_flyingpredator_flyspeed.GetFloat();
	if ( flDistance < flFlightSpeed * flInterval )
	{
		if ( GetNavigator()->IsGoalActive() )
		{
			if ( GetNavigator()->CurWaypointIsGoal() )
			{
				m_bReachedMoveGoal = true;
			}
			else
			{
				GetNavigator()->AdvancePath();
			}
		}
		else
			m_bReachedMoveGoal = true;
	}

	// Slow down on approach and reflect speed in pose

	flFlightSpeed *= m_flPlaybackRate;

	if ( GetHintNode() )
	{
		AIMoveTrace_t moveTrace;
		GetMoveProbe()->MoveLimit( NAV_FLY, GetAbsOrigin(), GetNavigator()->GetCurWaypointPos(), MASK_NPCSOLID, GetNavTargetEntity(), &moveTrace );

		//See if it succeeded
		if ( IsMoveBlocked( moveTrace.fStatus ) )
		{
			Vector vNodePos = vecMoveGoal;
			GetHintNode()->GetPosition(this, &vNodePos);
			
			GetNavigator()->SetGoal( vNodePos );
		}
	}

	//
	// Look to see if we are going to hit anything.
	//
	VectorNormalize( vForward );
	Vector vecDeflect;
	if ( Probe( vForward, flFlightSpeed * flInterval, vecDeflect ) )
	{
		vForward = vecDeflect;
		VectorNormalize( vForward );
	}

	SetAbsVelocity( vForward * flFlightSpeed );


	//Bank and set angles.
	Vector vRight;
	QAngle vRollAngle;
	
	VectorAngles( vForward, vRollAngle );
	vRollAngle.z = 0;

	AngleVectors( vRollAngle, NULL, &vRight, NULL );

	float flRoll = DotProduct( vRight, vecMoveDir ) * 45;
	flRoll = clamp( flRoll, -45, 45 );

	vRollAngle[ROLL] = flRoll;
	SetAbsAngles( vRollAngle );
}

//-----------------------------------------------------------------------------
// Purpose: Looks ahead to see if we are going to hit something. If we are, a
//			recommended avoidance path is returned.
// Input  : vecMoveDir - 
//			flSpeed - 
//			vecDeflect - 
// Output : Returns true if we hit something and need to deflect our course,
//			false if all is well.
//-----------------------------------------------------------------------------
bool CNPC_FlyingPredator::Probe( const Vector &vecMoveDir, float flSpeed, Vector &vecDeflect )
{
	//
	// Look 1/2 second ahead.
	//
	trace_t tr;
	AI_TraceHull( GetAbsOrigin(), GetAbsOrigin() + vecMoveDir * flSpeed, GetHullMins(), GetHullMaxs(), MASK_NPCSOLID, this, HL2COLLISION_GROUP_CROW, &tr );
	if ( tr.fraction < 1.0f )
	{
		//
		// If we hit something, deflect flight path parallel to surface hit.
		//
		Vector vecUp;
		CrossProduct( vecMoveDir, tr.plane.normal, vecUp );
		CrossProduct( tr.plane.normal, vecUp, vecDeflect );
		VectorNormalize( vecDeflect );
		return true;
	}

	vecDeflect = vec3_origin;
	return false;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CNPC_FlyingPredator::StartTouch( CBaseEntity *pOther )
{
	BaseClass::StartTouch( pOther );

	switch ( m_tFlyState )
	{
	case FlyState_Dive:
	case FlyState_Falling:
		// If traveling faster than 100 units / second, crash into things!
		// TODO - Really, falling / dive bombing stukabats should be using vPhysics instead
		if ( m_tFlyState == FlyState_Falling && GetAbsVelocity().IsLengthLessThan(200) )
		{
			break;
		}
		
		// If I fear or hate this entity, do damage!
		if ( IRelationType( pOther ) != D_LI )
		{
			CTakeDamageInfo info( this, this, m_bIsBaby ? sk_flyingpredator_dmg_dive.GetFloat() / 3.0f : sk_flyingpredator_dmg_dive.GetFloat(), DMG_CLUB | DMG_ALWAYSGIB );
			pOther->TakeDamage( info );

			if ( m_tFlyState == FlyState_Falling )
			{
				// If being flung, take self damage as well!
				TakeDamage( info );
			}
			else
			{
				EmitSound( "NPC_FlyingPredator.Bite" );
			}
		}

		if (m_tFlyState != FlyState_Falling)
		{
			if ( CanUseFlyNav() )
				SetFlyingState( CanLand() ? FlyState_Walking : FlyState_Flying );
			else
				SetFlyingState( CanLand() ? FlyState_Walking : FlyState_Falling );
		}
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Input handler that makes the crow fly away.
//-----------------------------------------------------------------------------
void CNPC_FlyingPredator::InputForceFlying( inputdata_t &inputdata )
{
	SetFlyingState( FlyState_Flying );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler that makes the crow fly away.
//-----------------------------------------------------------------------------
void CNPC_FlyingPredator::InputFly( inputdata_t &inputdata )
{
	SetCondition( COND_FORCED_FLY );
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Input  :
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CNPC_FlyingPredator::DrawDebugTextOverlays( void )
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		static const char *g_pszFlyStates[] =
		{
			"Walking",
			"Landing",
			"Falling",
			"Dive",
			"Flying",
			"Ceiling",
		};

		const char *pszFlyState = g_pszFlyStates[(int)m_tFlyState];

		char tempstr[64];
		Q_snprintf( tempstr, sizeof( tempstr ), "Flying State: %s (%i)", pszFlyState, (int)m_tFlyState );
		EntityText( text_offset, tempstr, 0 );
		text_offset++;
	}

	return text_offset;
}

//------------------------------------------------------------------------------
//
// Schedules
//
//------------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( NPC_FlyingPredator, CNPC_FlyingPredator )

	DECLARE_ACTIVITY( ACT_FALL )
	DECLARE_ACTIVITY( ACT_IDLE_CEILING )
	DECLARE_ACTIVITY( ACT_LAND_CEILING )
	DECLARE_ACTIVITY( ACT_DIVEBOMB_START )

	DECLARE_ANIMEVENT( AE_FLYING_PREDATOR_FLAP_WINGS )
	DECLARE_ANIMEVENT( AE_FLYING_PREDATOR_BEGIN_DIVEBOMB )

	DECLARE_CONDITION( COND_FORCED_FLY );
	DECLARE_CONDITION( COND_CAN_LAND );
	DECLARE_CONDITION( COND_FORCED_FALL );
	DECLARE_CONDITION( COND_CEILING_NEAR );
	DECLARE_CONDITION( COND_CAN_FLY );
	DECLARE_CONDITION( COND_FLY_STUCK );

	DECLARE_TASK( TASK_TAKEOFF )
	DECLARE_TASK( TASK_FLY_DIVE_BOMB )
	DECLARE_TASK( TASK_LAND )
	DECLARE_TASK( TASK_START_FLYING )
	DECLARE_TASK( TASK_START_FALLING )

	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_TAKEOFF,

		"	Tasks"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_LEAP"
		"		TASK_WAIT				0.5"
		"		TASK_TAKEOFF			0"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_HOVER"
		"		TASK_WAIT				1"
		"		TASK_START_FLYING		0"
		"		"
		"	Interrupts"
		"		COND_FORCED_FALL"
		"		COND_CEILING_NEAR"

	)

	DEFINE_SCHEDULE
	(
		SCHED_TAKEOFF_CEILING,

		"	Tasks"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_FALL"
		"		TASK_START_FALLING		0"
		"		TASK_WAIT				0.1"
		"		TASK_START_FLYING		0"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_HOVER"
		"		"
		"	Interrupts"
		"		COND_FORCED_FALL"
	)

	DEFINE_SCHEDULE
	(
		SCHED_FLY_DIVE_BOMB,

		"	Tasks"
		//"		TASK_FACE_ENEMY			0"
		"		TASK_WAIT				0.2"
		"		TASK_FLY_DIVE_BOMB		0"
		"		"
		"	Interrupts"
		"		COND_HEAVY_DAMAGE"
		"		COND_CAN_LAND"
		"		COND_FORCED_FALL"
	)

	DEFINE_SCHEDULE
	(
		SCHED_FALL,

		"	Tasks"
		"		TASK_START_FALLING		0"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_FALL"
		"		TASK_WAIT				5"
		"	Interrupts"
		"		COND_CAN_LAND"
	)

	DEFINE_SCHEDULE
	(
		SCHED_LAND,

		"	Tasks"
		"		TASK_LAND				0"
		"		"
		"	Interrupts"
		"		"
	)

	DEFINE_SCHEDULE
	(
		SCHED_START_FLYING,

		"	Tasks"
		"		TASK_START_FLYING				0"
		"		"
		"	Interrupts"
		"		COND_FORCED_FALL"
	)

	DEFINE_SCHEDULE
	(
		SCHED_TAKEOFF_IMMEDIATE,

		"	Tasks"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_LEAP"
		"		TASK_START_FLYING		0"
		"		"
		"	Interrupts"
		"		COND_FORCED_FALL"
		"		COND_CEILING_NEAR"
	)

	DEFINE_SCHEDULE
	(
		SCHED_APPROACH_ENEMY,

		"	Tasks"
		"		TASK_SET_TOLERANCE_DISTANCE				64"
		"		TASK_SET_ROUTE_SEARCH_TIME				1"	// Spend 1 second trying to build a path if stuck
		"		TASK_GET_FLANK_RADIUS_PATH_TO_ENEMY_LOS	512"
		"		TASK_WAIT_FOR_MOVEMENT					0"
		"		"
		"	Interrupts"
		"		COND_HEAVY_DAMAGE"
		"		COND_FORCED_FALL"
	)

	DEFINE_SCHEDULE
	(
		SCHED_CIRCLE_ENEMY,

		"	Tasks"
		"		TASK_SET_TOLERANCE_DISTANCE				64"
		"		TASK_SET_ROUTE_SEARCH_TIME				1"	// Spend 1 second trying to build a path if stuck
		"		TASK_GET_FLANK_ARC_PATH_TO_ENEMY_LOS	90"
		"		TASK_WAIT_FOR_MOVEMENT					0"
		"		"
		"	Interrupts"
		"		COND_HEAVY_DAMAGE"
		"		COND_FORCED_FALL"
	)

	DEFINE_SCHEDULE
	(
		SCHED_FACE_AND_DIVE_BOMB,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_IDLE"
		"		TASK_WAIT				0.25"
		"		TASK_FACE_ENEMY			0"
		"		TASK_WAIT				0.25"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_FLY_DIVE_BOMB"
		"		"
		"	Interrupts"
		"		COND_HEAVY_DAMAGE"
		"		COND_CAN_LAND"
		"		COND_FORCED_FALL"
	)

	DEFINE_SCHEDULE
	(
		SCHED_FLY_TO_EAT,

		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_PREDATOR_EAT					10"
		"		TASK_GET_PATH_TO_BESTSCENT			0"
		"		TASK_RUN_PATH						0"
		"		TASK_WAIT_FOR_MOVEMENT				0"
		"		TASK_LAND							0"
		"		TASK_PREDATOR_PLAY_EAT_ACT			0"
		"		TASK_PREDATOR_EAT					30"
		"		TASK_PREDATOR_REGEN					0"
		"	"
		"	Interrupts"
	)

	DEFINE_SCHEDULE
	(
		SCHED_TAKEOFF_AND_FLY_TO_EAT,

		"	Tasks"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_LEAP"
		"		TASK_START_FLYING		0"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_FLY_TO_EAT"
		"	"
		"	Interrupts"
	)

		
AI_END_CUSTOM_NPC()
