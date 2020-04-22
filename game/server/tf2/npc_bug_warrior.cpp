//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		The warrior bug
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "AI_Task.h"
#include "AI_Default.h"
#include "AI_Schedule.h"
#include "AI_Hull.h"
#include "AI_Hint.h"
#include "AI_Navigator.h"
#include "AI_Memory.h"
#include "AI_Squad.h"
#include "activitylist.h"
#include "soundent.h"
#include "game.h"
#include "NPCEvent.h"
#include "tf_player.h"
#include "EntityList.h"
#include "ndebugoverlay.h"
#include "shake.h"
#include "monstermaker.h"
#include "decals.h"
#include "vstdlib/random.h"
#include "tf_obj.h"
#include "engine/IEngineSound.h"
#include "IEffects.h"
#include "npc_bug_warrior.h"
#include "npc_bug_hole.h"

ConVar	npc_bug_warrior_health( "npc_bug_warrior_health", "150" );
ConVar	npc_bug_warrior_swipe_damage( "npc_bug_warrior_swipe_damage", "20" );
 
BEGIN_DATADESC( CNPC_Bug_Warrior )

	DEFINE_FIELD( m_flIdleDelay,		FIELD_FLOAT ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_bug_warrior, CNPC_Bug_Warrior );
IMPLEMENT_CUSTOM_AI( npc_bug_warrior, CNPC_Bug_Warrior );

// Bug interactions
int g_interactionBugSquadAttacking = 0;

//==================================================
// Bug Conditions
//==================================================
enum BugConditions
{
	COND_WBUG_STOP_FLEEING = LAST_SHARED_CONDITION,
	COND_WBUG_RETURN_TO_BUGHOLE,
	COND_WBUG_ASSIST_FELLOW_BUG,
};

//==================================================
// Bug Schedules
//==================================================

enum BugSchedules
{
	SCHED_WBUG_CHASE_ENEMY = LAST_SHARED_SCHEDULE,
	SCHED_WBUG_FLEE_ENEMY,
	SCHED_WBUG_CHASE_ENEMY_FAILED,
	SCHED_WBUG_PATROL,
	SCHED_WBUG_RETURN_TO_BUGHOLE,
	SCHED_WBUG_RETURN_TO_BUGHOLE_AND_REMOVE,
	SCHED_WBUG_ASSIST_FELLOW_BUG,
};

//==================================================
// Bug Tasks
//==================================================

enum BugTasks
{
	TASK_WBUG_GET_PATH_TO_FLEE = LAST_SHARED_TASK,
	TASK_WBUG_GET_PATH_TO_PATROL,
	TASK_WBUG_GET_PATH_TO_BUGHOLE,
	TASK_WBUG_HOLE_REMOVE,
	TASK_WBUG_GET_PATH_TO_ASSIST,
};

//==================================================
// Bug Activities
//==================================================

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNPC_Bug_Warrior::CNPC_Bug_Warrior( void )
{
	m_flFieldOfView	= 0.5f;
	m_flIdleDelay	= 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Setup our schedules and tasks, etc.
//-----------------------------------------------------------------------------
void CNPC_Bug_Warrior::InitCustomSchedules( void ) 
{
	INIT_CUSTOM_AI( CNPC_Bug_Warrior );

	// Register our interactions
	ADD_CUSTOM_INTERACTION( g_interactionBugSquadAttacking );

	// Schedules
	ADD_CUSTOM_SCHEDULE( CNPC_Bug_Warrior, SCHED_WBUG_CHASE_ENEMY );
	ADD_CUSTOM_SCHEDULE( CNPC_Bug_Warrior, SCHED_WBUG_FLEE_ENEMY );
	ADD_CUSTOM_SCHEDULE( CNPC_Bug_Warrior, SCHED_WBUG_CHASE_ENEMY_FAILED );
	ADD_CUSTOM_SCHEDULE( CNPC_Bug_Warrior, SCHED_WBUG_PATROL );
	ADD_CUSTOM_SCHEDULE( CNPC_Bug_Warrior, SCHED_WBUG_RETURN_TO_BUGHOLE );
	ADD_CUSTOM_SCHEDULE( CNPC_Bug_Warrior, SCHED_WBUG_RETURN_TO_BUGHOLE_AND_REMOVE );
	ADD_CUSTOM_SCHEDULE( CNPC_Bug_Warrior, SCHED_WBUG_ASSIST_FELLOW_BUG );

	// Conditions
	ADD_CUSTOM_CONDITION( CNPC_Bug_Warrior, COND_WBUG_STOP_FLEEING );
	ADD_CUSTOM_CONDITION( CNPC_Bug_Warrior, COND_WBUG_RETURN_TO_BUGHOLE );
	ADD_CUSTOM_CONDITION( CNPC_Bug_Warrior, COND_WBUG_ASSIST_FELLOW_BUG );
		
	// Tasks
	ADD_CUSTOM_TASK( CNPC_Bug_Warrior,	TASK_WBUG_GET_PATH_TO_FLEE );
	ADD_CUSTOM_TASK( CNPC_Bug_Warrior,	TASK_WBUG_GET_PATH_TO_PATROL );
	ADD_CUSTOM_TASK( CNPC_Bug_Warrior,	TASK_WBUG_GET_PATH_TO_BUGHOLE );
	ADD_CUSTOM_TASK( CNPC_Bug_Warrior,	TASK_WBUG_HOLE_REMOVE );
	ADD_CUSTOM_TASK( CNPC_Bug_Warrior,	TASK_WBUG_GET_PATH_TO_ASSIST );

	// Activities
	//ADD_CUSTOM_ACTIVITY( CNPC_Bug_Warrior,	ACT_BUG_WARRIOR_DISTRACT );

	AI_LOAD_SCHEDULE( CNPC_Bug_Warrior, SCHED_WBUG_CHASE_ENEMY );
	AI_LOAD_SCHEDULE( CNPC_Bug_Warrior, SCHED_WBUG_FLEE_ENEMY );
	AI_LOAD_SCHEDULE( CNPC_Bug_Warrior, SCHED_WBUG_CHASE_ENEMY_FAILED );
	AI_LOAD_SCHEDULE( CNPC_Bug_Warrior, SCHED_WBUG_PATROL );
	AI_LOAD_SCHEDULE( CNPC_Bug_Warrior, SCHED_WBUG_RETURN_TO_BUGHOLE );
	AI_LOAD_SCHEDULE( CNPC_Bug_Warrior, SCHED_WBUG_RETURN_TO_BUGHOLE_AND_REMOVE );
	AI_LOAD_SCHEDULE( CNPC_Bug_Warrior, SCHED_WBUG_ASSIST_FELLOW_BUG );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Bug_Warrior::Spawn( void )
{
	Precache();

	SetModel( BUG_WARRIOR_MODEL );

	SetHullType(HULL_WIDE_HUMAN);//HULL_WIDE_SHORT;
	SetHullSizeNormal();
	SetDefaultEyeOffset();
	SetViewOffset( (WorldAlignMins() + WorldAlignMaxs()) * 0.5 );	// See from my center
	SetDistLook( 1024.0 );
	
	SetNavType(NAV_GROUND);
	m_NPCState		= NPC_STATE_NONE;
	SetBloodColor( BLOOD_COLOR_YELLOW );
	m_iHealth		= npc_bug_warrior_health.GetFloat();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );

	m_iszPatrolPathName = NULL_STRING;

	// Only do this if a squadname appears in the entity
	if ( m_SquadName != NULL_STRING )
	{
		CapabilitiesAdd( bits_CAP_SQUAD );
	}

	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_MELEE_ATTACK1 );

	NPCInit();

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Bug_Warrior::Precache( void )
{
	PrecacheModel( BUG_WARRIOR_MODEL );

	PrecacheScriptSound( "NPC_Bug_Warrior.AttackHit" );
	PrecacheScriptSound( "NPC_Bug_Warrior.AttackSingle" );
	PrecacheScriptSound( "NPC_Bug_Warrior.Idle" );
	PrecacheScriptSound( "NPC_Bug_Warrior.Pain" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CNPC_Bug_Warrior::SelectSchedule( void )
{
	ClearCondition( COND_WBUG_STOP_FLEEING );

	// Turn towards sounds
	if ( HasCondition(COND_HEAR_DANGER) || HasCondition(COND_HEAR_COMBAT) )
	{
		CSound *pSound = GetBestSound();
		if ( pSound)
		{
			if ( !HasCondition( COND_SEE_ENEMY ) && ( pSound->m_iType & (SOUND_PLAYER | SOUND_COMBAT) ) )
			{
				GetMotor()->SetIdealYawToTarget( pSound->GetSoundReactOrigin() );
			}
		}
	}

	switch ( m_NPCState )
	{
	case NPC_STATE_IDLE:
		{
			// If I have an enemy, but I don't have any nearby friends, flee
			if ( HasCondition( COND_SEE_ENEMY ) && ShouldFlee() )
			{
				SetState( NPC_STATE_ALERT );
				return SCHED_WBUG_FLEE_ENEMY;
			}

			// Fellow bug might be requesting assistance
			if ( HasCondition( COND_WBUG_ASSIST_FELLOW_BUG ) )
				return SCHED_WBUG_ASSIST_FELLOW_BUG;

			// BugHole might be requesting help
			if ( HasCondition( COND_WBUG_RETURN_TO_BUGHOLE ) )
				return SCHED_WBUG_RETURN_TO_BUGHOLE;

			// Return to my bughole
			return SCHED_WBUG_RETURN_TO_BUGHOLE_AND_REMOVE;
			break;
		}
	case NPC_STATE_ALERT:
		{
			// If I have an enemy, but I don't have any nearby friends, flee
			if ( HasCondition( COND_SEE_ENEMY ) && ShouldFlee() )
			{
				SetState( NPC_STATE_ALERT );
				return SCHED_WBUG_FLEE_ENEMY;
			}

			// Fellow bug might be requesting assistance
			if ( HasCondition( COND_WBUG_ASSIST_FELLOW_BUG ) )
				return SCHED_WBUG_ASSIST_FELLOW_BUG;

			// BugHole might be requesting help
			if ( HasCondition( COND_WBUG_RETURN_TO_BUGHOLE ) )
				return SCHED_WBUG_RETURN_TO_BUGHOLE;

			// If I have a patrol path, walk it
			if ( m_iszPatrolPathName != NULL_STRING )
				return SCHED_WBUG_PATROL;

			break;
		}
	case NPC_STATE_COMBAT:
		{
			// Did I lose my enemy?
			if ( HasCondition ( COND_LOST_ENEMY ) || HasCondition ( COND_ENEMY_UNREACHABLE ) )
			{
				SetEnemy( NULL );
				m_bFightingToDeath = false;
				SetState(NPC_STATE_IDLE);
				return BaseClass::SelectSchedule();
			}

			// If I have an enemy, but I don't have any nearby friends, flee
			if ( HasCondition( COND_SEE_ENEMY ) && ShouldFlee() )
				return SCHED_WBUG_FLEE_ENEMY;

			// If we're able to melee, do so
			if ( HasCondition( COND_CAN_MELEE_ATTACK1 ) )
				return BaseClass::SelectSchedule();
		}
		break;
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: override/translate a schedule by type
// Input  : Type - schedule type
// Output : int - translated type
//-----------------------------------------------------------------------------
int CNPC_Bug_Warrior::TranslateSchedule( int scheduleType ) 
{
	if ( scheduleType == SCHED_CHASE_ENEMY ) 
	{
		// Tell my squad that I'm attacking this guy
		if ( m_pSquad )
		{
			m_pSquad->BroadcastInteraction( g_interactionBugSquadAttacking, (void *)GetEnemy(), this );
		}
		return SCHED_WBUG_CHASE_ENEMY;
	}

	return BaseClass::TranslateSchedule(scheduleType);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CNPC_Bug_Warrior::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_WBUG_GET_PATH_TO_FLEE:
		{
			// Always tell our bughole that we're under attack
			if ( m_hMyBugHole )
			{
				m_hMyBugHole->IncomingFleeingBug( this );
			}

			// We're fleeing from an enemy.
			// If I have a squadmate, run to him.
			CAI_BaseNPC *pSquadMate;
			if ( m_pSquad && (pSquadMate = m_pSquad->NearestSquadMember(this)) != false )
			{
				SetTarget( pSquadMate );
				AI_NavGoal_t goal( GOALTYPE_TARGETENT, vec3_origin, ACT_RUN );
				if ( GetNavigator()->SetGoal( goal ) )
				{
					TaskComplete();
					return;
				}
			}

			// If we have no squad, or we couldn't get a path to our squadmate, look for a bughole
			if ( m_hMyBugHole )
			{
				SetTarget( m_hMyBugHole );
				AI_NavGoal_t goal( GOALTYPE_TARGETENT, vec3_origin, ACT_RUN );
				if ( GetNavigator()->SetGoal( goal ) )
				{
					TaskComplete();
					return;
				}
			}

			// Give up, and fight to the death
			m_bFightingToDeath = true;
			TaskComplete();
		}
		break;

	case TASK_WBUG_GET_PATH_TO_PATROL:
		{
			// Get a path to the next point in the patrol.
			// Abort if we have no patrol path
			if ( !m_iszPatrolPathName )
			{
				TaskFail( "Attempting to patrol without patrol path." );
				return;
			}

			SetHintNode( CAI_HintManager::FindHint( this, HINT_BUG_PATROL_POINT, bits_HINT_NODE_RANDOM, 4096 ) ); 
			if( !GetHintNode() )
			{
				TaskFail("Couldn't find patrol node");
				return;
			}

			Vector vHintPos;
			GetHintNode()->GetPosition( this, &vHintPos );
			AI_NavGoal_t goal( vHintPos, ACT_RUN );
			GetNavigator()->SetGoal( goal );
			TaskComplete();
		}
		break;

	case TASK_WBUG_GET_PATH_TO_BUGHOLE:
		{
			// Get a path back to my bughole
			// If we have no squad, or we couldn't get a path to our squadmate, look for a bughole
			if ( m_hMyBugHole )
			{
				SetTarget( m_hMyBugHole );
				AI_NavGoal_t goal( GOALTYPE_TARGETENT, vec3_origin, ACT_RUN );
				if ( GetNavigator()->SetGoal( goal ) )
				{
					TaskComplete();
					return;
				}
			}

			TaskFail( "Couldn't get to bughole." );
		}
		break;

	case TASK_WBUG_HOLE_REMOVE:
		{
			// Crawl inside the bughole and remove myself
			AddEffects( EF_NODRAW );
			AddSolidFlags( FSOLID_NOT_SOLID );
			Event_Killed( CTakeDamageInfo( this, this, 200, DMG_CRUSH ) );

			// Tell the bughole
			if ( m_hMyBugHole )
			{
				m_hMyBugHole->BugReturned();
			}

			TaskComplete();
		}
		break;

	case TASK_WBUG_GET_PATH_TO_ASSIST:
		{
			if ( m_hAssistTarget )
			{
				SetTarget( m_hAssistTarget );
				AI_NavGoal_t goal( GOALTYPE_TARGETENT, vec3_origin, ACT_RUN );
				if ( GetNavigator()->SetGoal( goal ) )
				{
					TaskComplete();
					return;
				}
			}

			TaskFail( "Couldn't get to assist target." );
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
void CNPC_Bug_Warrior::RunTask( const Task_t *pTask )
{
	BaseClass::RunTask( pTask );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Bug_Warrior::FValidateHintType(CAI_Hint *pHint)
{
	if ( pHint->HintType() == HINT_BUG_PATROL_POINT )
	{
		// Make sure the name matches the patrol path I'm on
		if ( pHint->GetEntityName() == m_iszPatrolPathName )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Handle specific interactions with other NPCs
//-----------------------------------------------------------------------------
bool CNPC_Bug_Warrior::HandleInteraction( int interactionType, void *data, CBaseCombatCharacter *sender )
{
	// Check for a target found while burrowed
	if ( interactionType == g_interactionBugSquadAttacking )
	{
		// Ignore ones sent by me
		if ( sender == this )
			return false;

		// Interrupt me if I'm fleeing
		if ( IsCurSchedule(SCHED_WBUG_FLEE_ENEMY) || 
			 IsCurSchedule(SCHED_WBUG_CHASE_ENEMY_FAILED) )
		{
			SetCondition( COND_WBUG_STOP_FLEEING );
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pVictim - 
//-----------------------------------------------------------------------------
void CNPC_Bug_Warrior::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed( info );

	// Remove myself in a minute
	if ( !ShouldFadeOnDeath() )
	{
		SetThink( SUB_Remove );
		SetNextThink( gpGlobals->curtime + 20 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Bug_Warrior::MeleeAttack( float distance, float damage, QAngle& viewPunch, Vector& shove )
{
	if ( GetEnemy() == NULL )
		return;

	// Trace directly at my target
	Vector vStart = GetAbsOrigin();
	vStart.z += WorldAlignSize().z * 0.5;
	Vector vecForward = (GetEnemy()->EyePosition() - vStart);
	VectorNormalize( vecForward );
	Vector vEnd = vStart + (vecForward * distance );

	// Use half the size of my target for the box 
	Vector vecHalfTraceBox = (GetEnemy()->WorldAlignMaxs() - GetEnemy()->WorldAlignMins()) * 0.25;
	//NDebugOverlay::Box( vStart, -Vector(10,10,10), Vector(10,10,10), 0,255,0,20,1.0);
	//NDebugOverlay::Box( GetEnemy()->EyePosition(), -Vector(10,10,10), Vector(10,10,10), 255,255,255,20,1.0);
	CBaseEntity *pHurt = CheckTraceHullAttack( vStart, vEnd, -vecHalfTraceBox, vecHalfTraceBox, damage, DMG_SLASH );
	
	if ( pHurt )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pHurt );

		if ( pPlayer )
		{
			//Kick the player angles
			pPlayer->ViewPunch( viewPunch );	

			Vector	dir = pHurt->GetAbsOrigin() - GetAbsOrigin();
			VectorNormalize(dir);

			QAngle angles;
			VectorAngles( dir, angles );
			Vector forward, right;
			AngleVectors( angles, &forward, &right, NULL );

			// Push the target back
			Vector vecImpulse;
			VectorMultiply( right, -shove[1], vecImpulse );
			VectorMA( vecImpulse, -shove[0], forward, vecImpulse );
			pHurt->ApplyAbsVelocityImpulse( vecImpulse );
		}

		// Play a random attack hit sound
		EmitSound( "NPC_Bug_Warrior.AttackHit" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEvent - 
//-----------------------------------------------------------------------------
void CNPC_Bug_Warrior::HandleAnimEvent( animevent_t *pEvent )
{
	switch ( pEvent->event )
	{
	case BUG_WARRIOR_AE_MELEE_HIT1:
		MeleeAttack( BUG_WARRIOR_MELEE1_RANGE, npc_bug_warrior_swipe_damage.GetFloat(), QAngle( 20.0f, 0.0f, 0.0f ), Vector( -350.0f, 1.0f, 1.0f ) );
		return;
		break;

	case BUG_WARRIOR_AE_MELEE_SOUND1:
		{
			EmitSound( "NPC_Bug_Warrior.AttackSingle" );
			return;
		}
		break;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pInflictor - 
//			*pAttacker - 
//			flDamage - 
//			bitsDamageType - 
// Output : int
//-----------------------------------------------------------------------------
int CNPC_Bug_Warrior::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	Vector	faceDir;

	return BaseClass::OnTakeDamage_Alive( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Bug_Warrior::IdleSound( void )
{
	EmitSound( "NPC_Bug_Warrior.Idle" );
	m_flIdleDelay = gpGlobals->curtime + 4.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Bug_Warrior::PainSound( const CTakeDamageInfo &info )
{
	EmitSound( "NPC_Bug_Warrior.Pain" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecTarget - 
// Output : float
//-----------------------------------------------------------------------------
float CNPC_Bug_Warrior::CalcIdealYaw( const Vector &vecTarget )
{
	//If we can see our enemy but not reach them, face them always
	if ( ( GetEnemy() != NULL ) && ( HasCondition( COND_SEE_ENEMY ) && HasCondition( COND_ENEMY_UNREACHABLE ) ) )
	{
		return UTIL_VecToYaw ( GetEnemy()->GetAbsOrigin() - GetAbsOrigin() );
	}

	return BaseClass::CalcIdealYaw( vecTarget );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CNPC_Bug_Warrior::MaxYawSpeed( void )
{
	switch ( GetActivity() )
	{
	case ACT_IDLE:		
		return 15.0f;
		break;
	
	case ACT_WALK:
		return 15.0f;
		break;
	
	default:
	case ACT_RUN:
		return 30.0f;
		break;
	}

	return 30.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Bug_Warrior::ShouldPlayIdleSound( void )
{
	//Only do idles in the right states
	if ( ( m_NPCState != NPC_STATE_IDLE && m_NPCState != NPC_STATE_ALERT ) )
		return false;

	//Gagged monsters don't talk
	if ( HasSpawnFlags( SF_NPC_GAG ) )
		return false;

	//Don't cut off another sound or play again too soon
	if ( m_flIdleDelay > gpGlobals->curtime )
		return false;

	//Randomize it a bit
	if ( random->RandomInt( 0, 20 ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flDot - 
//			flDist - 
// Output : int
//-----------------------------------------------------------------------------
int CNPC_Bug_Warrior::MeleeAttack1Conditions( float flDot, float flDist )
{
	if ( ( gpGlobals->curtime - m_flLastAttackTime ) < 1.0f )
		return 0;

#if 0

	float		flPrDist, flPrDot;
	Vector		vecPrPos;
	Vector2D	vec2DPrDir;

	//Get our likely position in one second
	UTIL_GetPredictedPosition( GetEnemy(), 0.5f, &vecPrPos );

	flPrDist = ( vecPrPos - GetAbsOrigin() ).Length();

	vec2DPrDir	= ( vecPrPos - GetAbsOrigin() ).AsVector2D();

	Vector vecBodyDir;
	
	BodyDirection2D( &vecBodyDir );

	Vector2D	vec2DBodyDir = vecBodyDir.AsVector2D();
	
	flPrDot	= DotProduct2D (vec2DPrDir, vec2DBodyDir );

	if ( flPrDist > BUG_WARRIOR_MELEE1_RANGE )
		return COND_TOO_FAR_TO_ATTACK;

	if ( flPrDot < 0.5f )
		return COND_NOT_FACING_ATTACK;

#else

	if ( flDist > BUG_WARRIOR_MELEE1_RANGE )
		return COND_TOO_FAR_TO_ATTACK;

	if ( flDot < 0.5f )
		return COND_NOT_FACING_ATTACK;

#endif

	return COND_CAN_MELEE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this bug should flee
//-----------------------------------------------------------------------------
bool CNPC_Bug_Warrior::ShouldFlee( void )
{
	// I don't flee if I'm fighting to the death
	if ( m_bFightingToDeath )
		return false;

	// I don't flee if I'm not alone
	if ( !IsAlone() )
		return false;

	// I don't flee if I'm within sight of a bughole
	if ( m_hMyBugHole.Get() && FVisible( m_hMyBugHole ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Bug_Warrior::IsAlone( void )
{
	// If I'm not in a squad, make me just fight
	if ( !m_pSquad )
		return false;

	// If there aren't any visible squad members, I'm alone
	if ( m_pSquad->GetVisibleSquadMembers( this ) == 0 )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Bug_Warrior::SetBugHole( CMaker_BugHole *pBugHole )
{
	m_hMyBugHole = pBugHole;
}

//-----------------------------------------------------------------------------
// Purpose: BugHole is calling me home to defend it
//-----------------------------------------------------------------------------
void CNPC_Bug_Warrior::ReturnToBugHole( void )
{
	SetCondition( COND_WBUG_RETURN_TO_BUGHOLE );
}

//-----------------------------------------------------------------------------
// Purpose: Assist a bug that's under attack and making it's way back to the bughole
//-----------------------------------------------------------------------------
void CNPC_Bug_Warrior::Assist( CAI_BaseNPC *pBug )
{
	// If I'm not idle, I can't assist
	if ( m_NPCState != NPC_STATE_IDLE && m_NPCState != NPC_STATE_ALERT && m_NPCState != NPC_STATE_NONE )
		return;

	m_hAssistTarget = pBug;
	SetCondition( COND_WBUG_ASSIST_FELLOW_BUG );
	SetCondition( COND_PROVOKED );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Bug_Warrior::StartPatrolling( string_t iszPatrolPathName )
{
	// If I'm not idle, I can't patrol
	if ( m_NPCState != NPC_STATE_IDLE && m_NPCState != NPC_STATE_ALERT && m_NPCState != NPC_STATE_NONE )
		return false;

	// If I'm patrolling already, I can't patrol
	if ( IsPatrolling() )
		return false;

	SetState( NPC_STATE_ALERT );
	SetCondition( COND_PROVOKED );

	// Store off my patrol name
	m_iszPatrolPathName = iszPatrolPathName;
	m_iPatrolPoint = 0;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this bug is currently patrolling
//-----------------------------------------------------------------------------
bool CNPC_Bug_Warrior::IsPatrolling( void )
{
	return ( IsCurSchedule(SCHED_WBUG_PATROL) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Bug_Warrior::AlertSound( void )
{
	if ( GetEnemy() )
	{
		//FIXME: We need a better solution for inner-squad alerts!!
		//SOUND_DANGER is designed to frighten NPC's away. Need a different SOUND_ type.
		CSoundEnt::InsertSound( SOUND_DANGER, GetEnemy()->GetAbsOrigin(), 1024, 0.5f, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Overridden for team handling
//-----------------------------------------------------------------------------
Disposition_t CNPC_Bug_Warrior::IRelationType( CBaseEntity *pTarget )
{
	if ( pTarget->GetFlags() & FL_NOTARGET )
		return D_NU;

	if ( pTarget->Classify() == CLASS_PLAYER )
	{
		// Ignore stealthed enemies
		CBaseTFPlayer *pPlayer = (CBaseTFPlayer *)pTarget;
		if ( pPlayer->IsCamouflaged() )
			return D_NU;

		return D_HT;
	}

	// Attack objects
	if ( pTarget->Classify() == CLASS_MILITARY )
		return D_HT;

	return D_NU;
}

//------------------------------------------------------------------------------
// Purpose : Hate sentryguns more than other types of objects
//------------------------------------------------------------------------------
int	CNPC_Bug_Warrior::IRelationPriority( CBaseEntity *pTarget )
{
	if ( pTarget->Classify() == CLASS_MILITARY )
	{
		CBaseObject* pBaseObject = dynamic_cast<CBaseObject*>(pTarget);
		if ( pBaseObject && pBaseObject->IsSentrygun() )
			return ( BaseClass::IRelationPriority ( pTarget ) + 1 );
	}

	return BaseClass::IRelationPriority ( pTarget );
}


//------------------------------------------------------------------------------
//
// Schedules
//
//------------------------------------------------------------------------------

//=========================================================
// Chase Enemy
//=========================================================
AI_DEFINE_SCHEDULE
(
	SCHED_WBUG_CHASE_ENEMY,

	"	Tasks"
	//"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_WBUG_CHASE_ENEMY_FAILED"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY_FAILED"
	"		TASK_SET_TOLERANCE_DISTANCE		24"
	"		TASK_GET_PATH_TO_ENEMY			0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_ENEMY_UNREACHABLE"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK2"
	"		COND_GIVE_WAY"
	"		COND_LOST_ENEMY"
);

//=========================================================
// Failed to chase my enemy
//=========================================================
AI_DEFINE_SCHEDULE
(
	SCHED_WBUG_CHASE_ENEMY_FAILED,

	"	Tasks"
	"		TASK_STOP_MOVING			0"
	"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
	"		TASK_WAIT_FACE_ENEMY		0.5"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
	"		COND_HEAR_COMBAT"
	"		COND_HEAR_DANGER"
	"		COND_CAN_RANGE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_CAN_RANGE_ATTACK2"
	"		COND_CAN_MELEE_ATTACK2"
	"		COND_WBUG_STOP_FLEEING"
);

//=========================================================
// Flee from our enemy
//=========================================================
AI_DEFINE_SCHEDULE 
(
	SCHED_WBUG_FLEE_ENEMY,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_WBUG_CHASE_ENEMY"
	"		TASK_SET_TOLERANCE_DISTANCE		128"
	"		TASK_WBUG_GET_PATH_TO_FLEE		0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_TURN_RIGHT					180"
	""
	"	Interrupts"
	"		COND_ENEMY_DEAD"
	"		COND_WBUG_STOP_FLEEING"
	"		COND_LOST_ENEMY"
);

//=========================================================
// Walk a set of patrol points
//=========================================================
AI_DEFINE_SCHEDULE 
(
	SCHED_WBUG_PATROL,

	"	Tasks"
	//"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_WBUG_CHASE_ENEMY"
	"		TASK_SET_TOLERANCE_DISTANCE		256"
	"		TASK_WBUG_GET_PATH_TO_PATROL		0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"		// Use look around
	"		TASK_WAIT						2"
	"		TASK_WAIT_RANDOM				4"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_HEAR_COMBAT"
	"		COND_HEAR_DANGER"
	"		COND_WBUG_RETURN_TO_BUGHOLE"
);

//=========================================================
// Return to defend a bughole
//=========================================================
AI_DEFINE_SCHEDULE 
(
	SCHED_WBUG_RETURN_TO_BUGHOLE,

	"	Tasks"
	"		TASK_SET_TOLERANCE_DISTANCE		128"
	"		TASK_WBUG_GET_PATH_TO_BUGHOLE	0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_HEAR_COMBAT"
	"		COND_HEAR_DANGER"
);

//=========================================================
// Return to a bughole and remove myself
//=========================================================
AI_DEFINE_SCHEDULE 
(
	SCHED_WBUG_RETURN_TO_BUGHOLE_AND_REMOVE,

	"	Tasks"
	"		TASK_WAIT						5"		// Wait for 5-10 seconds to see if anything happens
	"		TASK_WAIT_RANDOM				5"
	"		TASK_SET_TOLERANCE_DISTANCE		128"
	"		TASK_WBUG_GET_PATH_TO_BUGHOLE	0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_WBUG_HOLE_REMOVE			0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_HEAR_COMBAT"
	"		COND_HEAR_DANGER"
);

//=========================================================
// Move to assist a fellow bug
//=========================================================
AI_DEFINE_SCHEDULE 
(
	SCHED_WBUG_ASSIST_FELLOW_BUG,

	"	Tasks"
	"		TASK_SET_TOLERANCE_DISTANCE		128"
	"		TASK_WBUG_GET_PATH_TO_ASSIST	0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	""
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_CAN_MELEE_ATTACK1"
	"		COND_CAN_MELEE_ATTACK2"
	"		COND_GIVE_WAY"
);