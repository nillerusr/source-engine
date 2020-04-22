//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		The builder bug
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
#include "npc_bug_builder.h"
#include "npc_bug_hole.h"

ConVar	npc_bug_builder_health( "npc_bug_builder_health", "100" );
 
BEGIN_DATADESC( CNPC_Bug_Builder )

	DEFINE_FIELD( m_flIdleDelay,		FIELD_FLOAT ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( npc_bug_builder, CNPC_Bug_Builder );
IMPLEMENT_CUSTOM_AI( npc_bug_builder, CNPC_Bug_Builder );

// Dawdling details
// Max & Min distances for dawdle forward movement
#define DAWDLE_MIN_DIST			64
#define DAWDLE_MAX_DIST			1024

//==================================================
// Bug Conditions
//==================================================
enum BugConditions
{
	COND_BBUG_RETURN_TO_BUGHOLE = LAST_SHARED_CONDITION,
};

//==================================================
// Bug Schedules
//==================================================

enum BugSchedules
{
	SCHED_BBUG_FLEE_ENEMY = LAST_SHARED_SCHEDULE,
	SCHED_BBUG_RETURN_TO_BUGHOLE,
	SCHED_BBUG_RETURN_TO_BUGHOLE_AND_REMOVE,
	SCHED_BBUG_DAWDLE,
};

//==================================================
// Bug Tasks
//==================================================

enum BugTasks
{
	TASK_BBUG_GET_PATH_TO_FLEE = LAST_SHARED_TASK,
	TASK_BBUG_GET_PATH_TO_BUGHOLE,
	TASK_BBUG_HOLE_REMOVE,
	TASK_BBUG_GET_PATH_TO_DAWDLE,
	TASK_BBUG_FACE_DAWDLE,
};

//==================================================
// Bug Activities
//==================================================

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CNPC_Bug_Builder::CNPC_Bug_Builder( void )
{
	m_flFieldOfView	= 0.5f;
	m_flIdleDelay	= 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Setup our schedules and tasks, etc.
//-----------------------------------------------------------------------------
void CNPC_Bug_Builder::InitCustomSchedules( void ) 
{
	INIT_CUSTOM_AI( CNPC_Bug_Builder );

	// Schedules
	ADD_CUSTOM_SCHEDULE( CNPC_Bug_Builder, SCHED_BBUG_FLEE_ENEMY );
	ADD_CUSTOM_SCHEDULE( CNPC_Bug_Builder, SCHED_BBUG_RETURN_TO_BUGHOLE );
	ADD_CUSTOM_SCHEDULE( CNPC_Bug_Builder, SCHED_BBUG_RETURN_TO_BUGHOLE_AND_REMOVE );
	ADD_CUSTOM_SCHEDULE( CNPC_Bug_Builder, SCHED_BBUG_DAWDLE );

	// Conditions
	ADD_CUSTOM_CONDITION( CNPC_Bug_Builder, COND_BBUG_RETURN_TO_BUGHOLE );
		
	// Tasks
	ADD_CUSTOM_TASK( CNPC_Bug_Builder,	TASK_BBUG_GET_PATH_TO_FLEE );
	ADD_CUSTOM_TASK( CNPC_Bug_Builder,	TASK_BBUG_GET_PATH_TO_BUGHOLE );
	ADD_CUSTOM_TASK( CNPC_Bug_Builder,	TASK_BBUG_HOLE_REMOVE );
	ADD_CUSTOM_TASK( CNPC_Bug_Builder,	TASK_BBUG_GET_PATH_TO_DAWDLE );
	ADD_CUSTOM_TASK( CNPC_Bug_Builder,	TASK_BBUG_FACE_DAWDLE );

	// Activities
	//ADD_CUSTOM_ACTIVITY( CNPC_Bug_Builder,	ACT_BUG_WARRIOR_DISTRACT );

	AI_LOAD_SCHEDULE( CNPC_Bug_Builder, SCHED_BBUG_FLEE_ENEMY );
	AI_LOAD_SCHEDULE( CNPC_Bug_Builder, SCHED_BBUG_RETURN_TO_BUGHOLE );
	AI_LOAD_SCHEDULE( CNPC_Bug_Builder, SCHED_BBUG_RETURN_TO_BUGHOLE_AND_REMOVE );
	AI_LOAD_SCHEDULE( CNPC_Bug_Builder, SCHED_BBUG_DAWDLE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Bug_Builder::Spawn( void )
{
	Precache();

	SetModel( BUG_BUILDER_MODEL );

	SetHullType(HULL_TINY);
	SetHullSizeNormal();
	SetDefaultEyeOffset();
	SetViewOffset( (WorldAlignMins() + WorldAlignMaxs()) * 0.5 );	// See from my center
	SetDistLook( 1024.0 );
	m_flNextDawdle = 0;
	
	SetNavType(NAV_GROUND);
	m_NPCState		= NPC_STATE_NONE;
	SetBloodColor( BLOOD_COLOR_YELLOW );
	m_iHealth		= npc_bug_builder_health.GetFloat();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );

	CapabilitiesAdd( bits_CAP_MOVE_GROUND );

	NPCInit();

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Bug_Builder::Precache( void )
{
	PrecacheModel( BUG_BUILDER_MODEL );

	PrecacheScriptSound( "NPC_Bug_Builder.Idle" );
	PrecacheScriptSound( "NPC_Bug_Builder.Pain" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CNPC_Bug_Builder::SelectSchedule( void )
{
	// If I'm not in idle anymore, don't idle
	if ( m_NPCState != NPC_STATE_IDLE )
	{
		m_flNextDawdle = 0;
	}

	switch ( m_NPCState )
	{
	case NPC_STATE_IDLE:
		{
			// BugHole might be requesting help
			if ( HasCondition( COND_BBUG_RETURN_TO_BUGHOLE ) )
				return SCHED_BBUG_RETURN_TO_BUGHOLE;

			// Setup to dawdle a bit from now
			if ( !m_flNextDawdle )
			{
				m_flNextDawdle = gpGlobals->curtime + random->RandomFloat( 3.0, 5.0 );
			}
			else if ( m_flNextDawdle < gpGlobals->curtime )
			{
				m_flNextDawdle = 0;
				return SCHED_BBUG_DAWDLE;
			}

			// When I take damage, I flee
			if ( HasCondition( COND_LIGHT_DAMAGE | COND_HEAVY_DAMAGE ) )
				return SCHED_BBUG_FLEE_ENEMY;

			// Return to my bughole
			//return SCHED_BBUG_RETURN_TO_BUGHOLE_AND_REMOVE;
			break;
		}
	case NPC_STATE_ALERT:
		{
			// BugHole might be requesting help
			if ( HasCondition( COND_BBUG_RETURN_TO_BUGHOLE ) )
				return SCHED_BBUG_RETURN_TO_BUGHOLE;

			// When I take damage, I flee
			if ( HasCondition( COND_LIGHT_DAMAGE ) || HasCondition( COND_HEAVY_DAMAGE ) )
				return SCHED_BBUG_FLEE_ENEMY;

			break;
		}
	case NPC_STATE_COMBAT:
		{
			// Did I lose my enemy?
			if ( HasCondition ( COND_LOST_ENEMY ) || HasCondition ( COND_ENEMY_UNREACHABLE ) )
			{
				SetEnemy( NULL );
				SetState(NPC_STATE_IDLE);
				return BaseClass::SelectSchedule();
			}

			// When I take damage, I flee
			if ( HasCondition( COND_LIGHT_DAMAGE ) || HasCondition( COND_HEAVY_DAMAGE ) )
				return SCHED_BBUG_FLEE_ENEMY;
		}
		break;
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CNPC_Bug_Builder::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_BBUG_GET_PATH_TO_FLEE:
		{
			// Always tell our bughole that we're under attack
			if ( m_hMyBugHole )
			{
				m_hMyBugHole->IncomingFleeingBug( this );
			}

			// If we have no squad, or we couldn't get a path to our squadmate, move to our bughole
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

			TaskComplete();
		}
		break;

	case TASK_BBUG_GET_PATH_TO_BUGHOLE:
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

	case TASK_BBUG_HOLE_REMOVE:
		{
			TaskComplete();

			// Crawl inside the bughole and remove myself
			AddEffects( EF_NODRAW );
			AddSolidFlags( FSOLID_NOT_SOLID );
			Event_Killed( CTakeDamageInfo( this, this, 200, DMG_CRUSH ) );

			// Tell the bughole
			if ( m_hMyBugHole )
			{
				m_hMyBugHole->BugReturned();
			}
		}
		break;

	case TASK_BBUG_GET_PATH_TO_DAWDLE:
		{
			// Get a dawdle point ahead of us
			Vector vecForward, vecTarget;
			AngleVectors( GetAbsAngles(), &vecForward );
			VectorMA( GetAbsOrigin(), random->RandomFloat( DAWDLE_MIN_DIST, DAWDLE_MAX_DIST ), vecForward, vecTarget );

			// See how far we could move ahead
			trace_t tr;
			UTIL_TraceEntity( this, GetAbsOrigin(), vecTarget, MASK_SOLID, &tr);
			float flDistance = tr.fraction * (vecTarget - GetAbsOrigin()).Length();
			if ( flDistance >= DAWDLE_MIN_DIST )
			{
				AI_NavGoal_t goal( tr.endpos );
				GetNavigator()->SetGoal( goal );
			}

			TaskComplete();
		}
		break;

	case TASK_BBUG_FACE_DAWDLE:
		{
			// Turn a random amount to the right
			float flYaw = GetMotor()->GetIdealYaw();
			flYaw = flYaw + random->RandomFloat( 45, 135 );
			GetMotor()->SetIdealYaw( UTIL_AngleMod(flYaw) );
			SetTurnActivity();
			break;
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
void CNPC_Bug_Builder::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_BBUG_FACE_DAWDLE:
		{
			GetMotor()->UpdateYaw();
			if ( FacingIdeal() )
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

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CNPC_Bug_Builder::FValidateHintType(CAI_Hint *pHint)
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pVictim - 
//-----------------------------------------------------------------------------
void CNPC_Bug_Builder::Event_Killed( const CTakeDamageInfo &info )
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
// Input  : *pEvent - 
//-----------------------------------------------------------------------------
void CNPC_Bug_Builder::HandleAnimEvent( animevent_t *pEvent )
{
	BaseClass::HandleAnimEvent( pEvent );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CNPC_Bug_Builder::MaxYawSpeed( void )
{
	return 2.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Bug_Builder::IdleSound( void )
{
	EmitSound( "NPC_Bug_Builder.Idle" );
	m_flIdleDelay = gpGlobals->curtime + 4.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Bug_Builder::PainSound( const CTakeDamageInfo &info )
{
	EmitSound( "NPC_Bug_Builder.Pain" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Bug_Builder::ShouldPlayIdleSound( void )
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
//-----------------------------------------------------------------------------
void CNPC_Bug_Builder::SetBugHole( CMaker_BugHole *pBugHole )
{
	m_hMyBugHole = pBugHole;
}

//-----------------------------------------------------------------------------
// Purpose: BugHole is calling me home to defend it
//-----------------------------------------------------------------------------
void CNPC_Bug_Builder::ReturnToBugHole( void )
{
	SetCondition( COND_BBUG_RETURN_TO_BUGHOLE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Bug_Builder::AlertSound( void )
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
Disposition_t CNPC_Bug_Builder::IRelationType( CBaseEntity *pTarget )
{
	// Builders ignore everything
	return D_NU;
}


//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------

//=========================================================
// Dawdle around
//=========================================================
AI_DEFINE_SCHEDULE 
(
	SCHED_BBUG_DAWDLE,

	"	Tasks"
	"		TASK_SET_TOLERANCE_DISTANCE		32"
	"		TASK_BBUG_GET_PATH_TO_DAWDLE	0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_BBUG_FACE_DAWDLE			0"
	"	"
	"	Interrupts"
	"		COND_LIGHT_DAMAGE"
	"		COND_HEAVY_DAMAGE"
);

//=========================================================
// Flee from our enemy
//=========================================================
AI_DEFINE_SCHEDULE 
(
	SCHED_BBUG_FLEE_ENEMY,

	"	Tasks"
	"		TASK_SET_TOLERANCE_DISTANCE		128"
	"		TASK_BBUG_GET_PATH_TO_FLEE		0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_TURN_RIGHT					180"
	"	"
	"	Interrupts"
	"		COND_ENEMY_DEAD"
	"		COND_LOST_ENEMY"
);

//=========================================================
// Retreat to a bughole
//=========================================================
AI_DEFINE_SCHEDULE 
(
	SCHED_BBUG_RETURN_TO_BUGHOLE,

	"	Tasks"
	"		TASK_SET_TOLERANCE_DISTANCE		128"
	"		TASK_BBUG_GET_PATH_TO_BUGHOLE	0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"	"
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
	SCHED_BBUG_RETURN_TO_BUGHOLE_AND_REMOVE,

	"	Tasks"
	"		TASK_WAIT						5"		// Wait for 5-10 seconds to see if anything happens
	"		TASK_WAIT_RANDOM				5"
	"		TASK_SET_TOLERANCE_DISTANCE		128"
	"		TASK_BBUG_GET_PATH_TO_BUGHOLE	0"
	"		TASK_RUN_PATH					0"
	"		TASK_WAIT_FOR_MOVEMENT			0"
	"		TASK_BBUG_HOLE_REMOVE			0"
	"	"
	"	Interrupts"
	"		COND_NEW_ENEMY"
	"		COND_HEAR_COMBAT"
	"		COND_HEAR_DANGER"
);

