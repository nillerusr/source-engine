//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include	"cbase.h"
#include	"ai_default.h"
#include	"ai_task.h"
#include	"ai_schedule.h"
#include	"ai_node.h"
#include	"ai_hull.h"
#include	"ai_hint.h"
#include	"ai_memory.h"
#include	"ai_route.h"
#include	"ai_motor.h"
#include	"soundent.h"
#include	"game.h"
#include	"npcevent.h"
#include	"entitylist.h"
#include	"activitylist.h"
#include	"animation.h"
#include	"basecombatweapon.h"
#include	"IEffects.h"
#include	"vstdlib/random.h"
#include	"engine/IEngineSound.h"
#include	"ammodef.h"
#include    "util.h"
#include	"hl1_ai_basenpc.h"
#include	"hl1_basegrenade.h"
#include	"movevars_shared.h"
#include	"ai_basenpc.h"


ConVar sk_hassassin_health( "sk_hassassin_health", "50" );

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_ASSASSIN_EXPOSED = LAST_SHARED_SCHEDULE,// cover was blown.
	SCHED_ASSASSIN_JUMP,	// fly through the air
	SCHED_ASSASSIN_JUMP_ATTACK,	// fly through the air and shoot
	SCHED_ASSASSIN_JUMP_LAND, // hit and run away
	SCHED_ASSASSIN_FAIL, 
	SCHED_ASSASSIN_TAKE_COVER_FROM_ENEMY1,
	SCHED_ASSASSIN_TAKE_COVER_FROM_ENEMY2,
	SCHED_ASSASSIN_TAKE_COVER_FROM_BEST_SOUND,
	SCHED_ASSASSIN_HIDE,
	SCHED_ASSASSIN_HUNT,
};

Activity ACT_ASSASSIN_FLY_UP;
Activity ACT_ASSASSIN_FLY_ATTACK;
Activity ACT_ASSASSIN_FLY_DOWN;

//=========================================================
// monster-specific tasks
//=========================================================

enum
{
	TASK_ASSASSIN_FALL_TO_GROUND = LAST_SHARED_TASK + 1, // falling and waiting to hit ground
};


//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		ASSASSIN_AE_SHOOT1	1
#define		ASSASSIN_AE_TOSS1	2
#define		ASSASSIN_AE_JUMP	3


#define MEMORY_BADJUMP	bits_MEMORY_CUSTOM1

class CNPC_HAssassin : public CHL1BaseNPC
{
	DECLARE_CLASS( CNPC_HAssassin, CHL1BaseNPC );

public:
	void Spawn( void );
	void Precache( void );

	int  TranslateSchedule( int scheduleType );

	void HandleAnimEvent( animevent_t *pEvent );
	float MaxYawSpeed() { return 360.0f; }

	void Shoot ( void );

	int  MeleeAttack1Conditions ( float flDot, float flDist );
	int	 RangeAttack1Conditions ( float flDot, float flDist );
	int	 RangeAttack2Conditions ( float flDot, float flDist );
	
	int	 SelectSchedule ( void );

	void RunTask ( const Task_t *pTask );
	void StartTask ( const Task_t *pTask );

	Class_T	Classify ( void );

	int GetSoundInterests( void );

	void RunAI( void );

	float m_flLastShot;
	float m_flDiviation;

	float m_flNextJump;
	Vector m_vecJumpVelocity;

	float m_flNextGrenadeCheck;
	Vector	m_vecTossVelocity;
	bool	m_fThrowGrenade;

	int		m_iTargetRanderamt;

	int		m_iFrustration;

	int		m_iAmmoType;

public:
	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;

};

LINK_ENTITY_TO_CLASS( monster_human_assassin, CNPC_HAssassin );

BEGIN_DATADESC( CNPC_HAssassin )
	DEFINE_FIELD( m_flLastShot, FIELD_TIME ),
	DEFINE_FIELD( m_flDiviation, FIELD_FLOAT ),

	DEFINE_FIELD( m_flNextJump, FIELD_TIME ),
	DEFINE_FIELD( m_vecJumpVelocity, FIELD_VECTOR ),

	DEFINE_FIELD( m_flNextGrenadeCheck, FIELD_TIME ),
	DEFINE_FIELD( m_vecTossVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_fThrowGrenade, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_iTargetRanderamt, FIELD_INTEGER ),
	DEFINE_FIELD( m_iFrustration, FIELD_INTEGER ),

	//DEFINE_FIELD( m_iAmmoType, FIELD_INTEGER ),

END_DATADESC()

//=========================================================
// Spawn
//=========================================================
void CNPC_HAssassin::Spawn()
{
	Precache( );

	SetModel( "models/hassassin.mdl");
	
	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();


	SetNavType ( NAV_GROUND );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	m_bloodColor		= BLOOD_COLOR_RED;
    ClearEffects();
	m_iHealth			= sk_hassassin_health.GetFloat();
	m_flFieldOfView		= VIEW_FIELD_WIDE; // indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_NPCState			= NPC_STATE_NONE;
	
	m_HackedGunPos		= Vector( 0, 24, 48 );

	m_iTargetRanderamt	= 20;
	SetRenderColor( 255, 255, 255, 20 );
	m_nRenderMode		= kRenderTransTexture;

	CapabilitiesClear();
	CapabilitiesAdd( bits_CAP_MOVE_GROUND );
	CapabilitiesAdd( bits_CAP_INNATE_RANGE_ATTACK1 | bits_CAP_INNATE_RANGE_ATTACK2 | bits_CAP_INNATE_MELEE_ATTACK1 );

	NPCInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_HAssassin::Precache()
{
	m_iAmmoType = GetAmmoDef()->Index("9mmRound");

	PrecacheModel("models/hassassin.mdl");

	UTIL_PrecacheOther( "npc_handgrenade" );

	PrecacheScriptSound( "HAssassin.Shot" );
	PrecacheScriptSound( "HAssassin.Beamsound" );
	PrecacheScriptSound( "HAssassin.Footstep" );
}

int CNPC_HAssassin::GetSoundInterests( void )
{
	return	SOUND_WORLD	|
			SOUND_COMBAT	|
			SOUND_PLAYER	|
			SOUND_DANGER;
}	

Class_T	CNPC_HAssassin::Classify ( void )
{
	return CLASS_HUMAN_MILITARY;
}

//=========================================================
// CheckMeleeAttack1 - jump like crazy if the enemy gets too close. 
//=========================================================
int CNPC_HAssassin::MeleeAttack1Conditions ( float flDot, float flDist )
{
	if ( m_flNextJump < gpGlobals->curtime && ( flDist <= 128 || HasMemory( MEMORY_BADJUMP )) && GetEnemy() != NULL )
	{
		trace_t	tr;

		Vector vecMin = Vector( random->RandomFloat( 0, -64), random->RandomFloat( 0, -64 ), 0 );
		Vector vecMax = Vector( random->RandomFloat( 0, 64), random->RandomFloat( 0, 64 ), 160 );

		Vector vecDest = GetAbsOrigin() + Vector( random->RandomFloat( -64, 64), random->RandomFloat( -64, 64 ), 160 );

		UTIL_TraceHull( GetAbsOrigin() + Vector( 0, 0, 36 ), GetAbsOrigin() + Vector( 0, 0, 36 ), vecMin, vecMax, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

		//NDebugOverlay::Box( GetAbsOrigin() + Vector( 0, 0, 36 ), vecMin, vecMax, 0,0, 255, 0, 2.0 );

		if ( tr.startsolid || tr.fraction < 1.0)
		{
			return COND_TOO_CLOSE_TO_ATTACK;
		}

		float flGravity = GetCurrentGravity();

		float time = sqrt( 160 / (0.5 * flGravity));
		float speed = flGravity * time / 160;
		m_vecJumpVelocity = ( vecDest - GetAbsOrigin() ) * speed;

		return COND_CAN_MELEE_ATTACK1;
	}

	if ( flDist > 128 )
		 return COND_TOO_FAR_TO_ATTACK;
	
	return COND_NONE;
}

//=========================================================
// CheckRangeAttack1  - drop a cap in their ass
//
//=========================================================
int CNPC_HAssassin::RangeAttack1Conditions ( float flDot, float flDist )
{
	if ( !HasCondition( COND_ENEMY_OCCLUDED ) && flDist > 64 && flDist <= 2048 )
	{
		trace_t	tr;

		Vector vecSrc = GetAbsOrigin() + m_HackedGunPos;

		// verify that a bullet fired from the gun will hit the enemy before the world.
		UTIL_TraceLine( vecSrc, GetEnemy()->BodyTarget(vecSrc), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);

		if ( tr.fraction == 1.0 || tr.m_pEnt == GetEnemy() )
		{
			return COND_CAN_RANGE_ATTACK1;
		}
	}

	return COND_NONE;
}

//=========================================================
// CheckRangeAttack2 - toss grenade is enemy gets in the way and is too close. 
//=========================================================
int CNPC_HAssassin::RangeAttack2Conditions ( float flDot, float flDist )
{
	m_fThrowGrenade = false;
	if ( !FBitSet ( GetEnemy()->GetFlags(), FL_ONGROUND ) )
	{
		// don't throw grenades at anything that isn't on the ground!
		return COND_NONE;
	}

	// don't get grenade happy unless the player starts to piss you off
	if ( m_iFrustration <= 2)
		return COND_NONE;

	if ( m_flNextGrenadeCheck < gpGlobals->curtime && !HasCondition( COND_ENEMY_OCCLUDED ) && flDist <= 512 )
	{
		Vector vTossPos;
		QAngle vAngles;

		GetAttachment( "grenadehand", vTossPos, vAngles );

		Vector vecToss = VecCheckThrow( this, vTossPos, GetEnemy()->WorldSpaceCenter(), flDist, 0.5 ); // use dist as speed to get there in 1 second

		if ( vecToss != vec3_origin )
		{
			m_vecTossVelocity = vecToss;

			// throw a hand grenade
			m_fThrowGrenade = TRUE;

			return COND_CAN_RANGE_ATTACK2;
		}
	}

	return COND_NONE;
}

//=========================================================
// StartTask
//=========================================================
void CNPC_HAssassin::StartTask ( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_RANGE_ATTACK2:
		if (!m_fThrowGrenade)
		{
			TaskComplete( );
		}
		else
		{
			BaseClass::StartTask ( pTask );
		}
		break;
	case TASK_ASSASSIN_FALL_TO_GROUND:
		m_flWaitFinished = gpGlobals->curtime + 2.0f;
		break;
	default:
		BaseClass::StartTask ( pTask );
		break;
	}
}


//=========================================================
// RunTask 
//=========================================================
void CNPC_HAssassin::RunTask ( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_ASSASSIN_FALL_TO_GROUND:
		GetMotor()->SetIdealYawAndUpdate( GetEnemyLKP() );

		if ( IsSequenceFinished() )
		{
			if ( GetAbsVelocity().z > 0)
			{
				SetActivity( ACT_ASSASSIN_FLY_UP );
			}
			else if ( HasCondition ( COND_SEE_ENEMY ))
			{
				SetActivity( ACT_ASSASSIN_FLY_ATTACK );
				SetCycle( 0 );
			}
			else
			{
				SetActivity( ACT_ASSASSIN_FLY_DOWN );
				SetCycle( 0 );
			}
						
			ResetSequenceInfo( );
		}

		if ( GetFlags() & FL_ONGROUND)
		{
			TaskComplete( );
		}
		else if( gpGlobals->curtime > m_flWaitFinished || GetAbsVelocity().z == 0.0 )
		{
			// I've waited two seconds and haven't hit the ground. Try to force it.
			trace_t trace;
			UTIL_TraceEntity( this, GetAbsOrigin(), GetAbsOrigin() - Vector( 0, 0, 1 ), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &trace );

			if( trace.DidHitWorld() )
			{
				SetGroundEntity( trace.m_pEnt );
			}
			else
			{
				// Try again in a couple of seconds.
				m_flWaitFinished = gpGlobals->curtime + 2.0f;
			}
		}
		break;
	default: 
		BaseClass::RunTask ( pTask );
		break;
	}
}

//=========================================================
// GetSchedule - Decides which type of schedule best suits
// the monster's current state and conditions. Then calls
// monster's member function to get a pointer to a schedule
// of the proper type.
//=========================================================
int CNPC_HAssassin::SelectSchedule ( void )
{
	switch	( m_NPCState )
	{
	case NPC_STATE_IDLE:
	case NPC_STATE_ALERT:
		{
			if ( HasCondition ( COND_HEAR_DANGER ) || HasCondition ( COND_HEAR_COMBAT ) )
			{
				if ( HasCondition ( COND_HEAR_DANGER ) )
					 return SCHED_TAKE_COVER_FROM_BEST_SOUND;
				
				else
				 	 return SCHED_INVESTIGATE_SOUND;
			}
		}
		break;

	case NPC_STATE_COMBAT:
		{
			// dead enemy
			if ( HasCondition( COND_ENEMY_DEAD ) )
			{
				// call base class, all code to handle dead enemies is centralized there.
				return BaseClass::SelectSchedule();
			}

			// flying?
			if ( GetMoveType() == MOVETYPE_FLYGRAVITY )
			{
				if ( GetFlags() & FL_ONGROUND )
				{
					//Msg( "landed\n" );
					// just landed
					SetMoveType( MOVETYPE_STEP );
					return SCHED_ASSASSIN_JUMP_LAND;
				}
				else
				{
					//Msg("jump\n");
					// jump or jump/shoot
					if ( m_NPCState == NPC_STATE_COMBAT )
						return SCHED_ASSASSIN_JUMP;
					else
						return SCHED_ASSASSIN_JUMP_ATTACK;
				}
			}

			if ( HasCondition ( COND_HEAR_DANGER ) )
			{
				return SCHED_TAKE_COVER_FROM_BEST_SOUND;
			}

			if ( HasCondition ( COND_LIGHT_DAMAGE ) )
			{
				m_iFrustration++;
			}
			if ( HasCondition ( COND_HEAVY_DAMAGE ) )
			{
				m_iFrustration++;
			}

			// jump player!
			if ( HasCondition ( COND_CAN_MELEE_ATTACK1 ) )
			{
				//Msg( "melee attack 1\n");
				return SCHED_MELEE_ATTACK1;
			}

			// throw grenade
			if ( HasCondition ( COND_CAN_RANGE_ATTACK2 ) )
			{
				//Msg( "range attack 2\n");
				return SCHED_RANGE_ATTACK2;
			}

			// spotted
			if ( HasCondition ( COND_SEE_ENEMY ) && HasCondition ( COND_ENEMY_FACING_ME ) )
			{
				//Msg("exposed\n");
				m_iFrustration++;
				return SCHED_ASSASSIN_EXPOSED;
			}

			// can attack
			if ( HasCondition ( COND_CAN_RANGE_ATTACK1 ) )
			{
				//Msg( "range attack 1\n" );
				m_iFrustration = 0;
				return SCHED_RANGE_ATTACK1;
			}

			if ( HasCondition ( COND_SEE_ENEMY ) )
			{
				//Msg( "face\n");
				return SCHED_COMBAT_FACE;
			}

			// new enemy
			if ( HasCondition ( COND_NEW_ENEMY ) )
			{
				//Msg( "take cover\n");
				return SCHED_TAKE_COVER_FROM_ENEMY;
			}

			// ALERT( at_console, "stand\n");
			return SCHED_ALERT_STAND;
		}
		break;
	}

	return BaseClass::SelectSchedule();
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//
// Returns number of events handled, 0 if none.
//=========================================================
void CNPC_HAssassin::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
	case ASSASSIN_AE_SHOOT1:
		Shoot( );
		break;
	case ASSASSIN_AE_TOSS1:
		{
			Vector vTossPos;
			QAngle vAngles;

			GetAttachment( "grenadehand", vTossPos, vAngles );

			CHandGrenade *pGrenade = (CHandGrenade*)Create( "grenade_hand", vTossPos, vec3_angle );
			if ( pGrenade )
			{
				pGrenade->ShootTimed( this, m_vecTossVelocity, 2.0 );
			}

			m_flNextGrenadeCheck = gpGlobals->curtime + 6;// wait six seconds before even looking again to see if a grenade can be thrown.
			m_fThrowGrenade = FALSE;
			// !!!LATER - when in a group, only try to throw grenade if ordered.
		}
		break;
	case ASSASSIN_AE_JUMP:
		{
			SetMoveType( MOVETYPE_FLYGRAVITY );
			SetGroundEntity( NULL );
			SetAbsVelocity( m_vecJumpVelocity );
			m_flNextJump = gpGlobals->curtime + 3.0;
		}
		return;
	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
}



//=========================================================
// Shoot
//=========================================================
void CNPC_HAssassin::Shoot ( void )
{
	Vector vForward, vRight, vUp;
	Vector vecShootOrigin;
	QAngle vAngles;

	if ( GetEnemy() == NULL)
	{
		return;
	}

	GetAttachment( "guntip", vecShootOrigin, vAngles );
	
	Vector vecShootDir = GetShootEnemyDir( vecShootOrigin );

	if (m_flLastShot + 2 < gpGlobals->curtime)
	{
		m_flDiviation = 0.10;
	}
	else
	{
		m_flDiviation -= 0.01;
		if (m_flDiviation < 0.02)
			m_flDiviation = 0.02;
	}
	m_flLastShot = gpGlobals->curtime;

	AngleVectors( GetAbsAngles(), &vForward, &vRight, &vUp );

	Vector	vecShellVelocity = vRight * random->RandomFloat(40,90) + vUp * random->RandomFloat(75,200) + vForward * random->RandomFloat(-40, 40);
	EjectShell( GetAbsOrigin() + vUp * 32 + vForward * 12, vecShellVelocity, GetAbsAngles().y, 0 ); 
	FireBullets( 1, vecShootOrigin, vecShootDir, Vector( m_flDiviation, m_flDiviation, m_flDiviation ), 2048, m_iAmmoType ); // shoot +-8 degrees

	//NDebugOverlay::Line( vecShootOrigin, vecShootOrigin + vecShootDir * 2048, 255, 0, 0, true, 2.0 );

	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "HAssassin.Shot" );

	DoMuzzleFlash();

	VectorAngles( vecShootDir, vAngles );
	SetPoseParameter( "shoot", vecShootDir.x );

	m_cAmmoLoaded--;
}

//=========================================================
//=========================================================
int CNPC_HAssassin::TranslateSchedule ( int scheduleType ) 
{
//	Msg( "%d\n", m_iFrustration );
	switch	( scheduleType )
	{
	case SCHED_TAKE_COVER_FROM_ENEMY:

		if ( m_iHealth > 30 )
			return SCHED_ASSASSIN_TAKE_COVER_FROM_ENEMY1;
		else
			return SCHED_ASSASSIN_TAKE_COVER_FROM_ENEMY2;

	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		return SCHED_ASSASSIN_TAKE_COVER_FROM_BEST_SOUND;
	case SCHED_FAIL:

		if ( m_NPCState == NPC_STATE_COMBAT )
			return SCHED_ASSASSIN_FAIL;

		break;
	case SCHED_ALERT_STAND:

		if ( m_NPCState == NPC_STATE_COMBAT )
			return SCHED_ASSASSIN_HIDE;

		break;
	//case SCHED_CHASE_ENEMY:
	//	return SCHED_ASSASSIN_HUNT;

	case SCHED_MELEE_ATTACK1:

		if ( GetFlags() & FL_ONGROUND)
		{
			if (m_flNextJump > gpGlobals->curtime)
			{
				// can't jump yet, go ahead and fail
				return SCHED_ASSASSIN_FAIL;
			}
			else
			{
				return SCHED_ASSASSIN_JUMP;
			}
		}
		else
		{
			return SCHED_ASSASSIN_JUMP_ATTACK;
		}
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

//=========================================================
// RunAI
//=========================================================
void CNPC_HAssassin::RunAI( void )
{
	BaseClass::RunAI();

	// always visible if moving
	// always visible is not on hard
	if (g_iSkillLevel != SKILL_HARD || GetEnemy() == NULL || m_lifeState == LIFE_DEAD || GetActivity() == ACT_RUN || GetActivity() == ACT_WALK || !(GetFlags() & FL_ONGROUND))
		m_iTargetRanderamt = 255;
	else
		m_iTargetRanderamt = 20;

	CPASAttenuationFilter filter( this );

	if ( GetRenderColor().a > m_iTargetRanderamt)
	{
		if ( GetRenderColor().a == 255)
		{
			EmitSound( filter, entindex(), "HAssassin.Beamsound" );
		}

		SetRenderColorA( MAX( GetRenderColor().a - 50, m_iTargetRanderamt ) );
		m_nRenderMode = kRenderTransTexture;
	}
	else if ( GetRenderColor().a < m_iTargetRanderamt)
	{
		SetRenderColorA ( MIN( GetRenderColor().a + 50, m_iTargetRanderamt ) );
		if (GetRenderColor().a == 255)
			m_nRenderMode = kRenderNormal;
	}

	if ( GetActivity() == ACT_RUN || GetActivity() == ACT_WALK)
	{
		static int iStep = 0;
		iStep = ! iStep;
		if (iStep)
		{
			EmitSound( filter, entindex(), "HAssassin.Footstep" );	
		}
	}
}

AI_BEGIN_CUSTOM_NPC( monster_human_assassin, CNPC_HAssassin )

	DECLARE_TASK( TASK_ASSASSIN_FALL_TO_GROUND )

	DECLARE_ACTIVITY( ACT_ASSASSIN_FLY_UP )
	DECLARE_ACTIVITY( ACT_ASSASSIN_FLY_ATTACK )
	DECLARE_ACTIVITY( ACT_ASSASSIN_FLY_DOWN )

	//=========================================================
	// AI Schedules Specific to this monster
	//=========================================================

	//=========================================================
	// Enemy exposed assasin's cover
	//=========================================================

	//=========================================================
	// > SCHED_ASSASSIN_EXPOSED
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_EXPOSED,
	
		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_RANGE_ATTACK1		0"
		"		TASK_SET_FAIL_SCHEDULE	SCHEDULE:SCHED_ASSASSIN_JUMP"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_TAKE_COVER_FROM_ENEMY"
		"	"
		"	Interrupts"
		"	COND_CAN_MELEE_ATTACK1"
		
	)

	//=========================================================
	// > SCHED_ASSASSIN_JUMP
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_JUMP,
	
		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_PLAY_SEQUENCE		ACTIVITY:ACT_HOP"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_ASSASSIN_JUMP_ATTACK"
		"	"
		"	Interrupts"
	)

	//=========================================================
	// > SCHED_ASSASSIN_JUMP_ATTACK
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_JUMP_ATTACK,
	
		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_ASSASSIN_JUMP_LAND"
		"		TASK_ASSASSIN_FALL_TO_GROUND	0"
		"	"
		"	Interrupts"
	)

	//=========================================================
	// > SCHED_ASSASSIN_JUMP_LAND
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_JUMP_LAND,
	
		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_ASSASSIN_EXPOSED"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
		"		TASK_REMEMBER				MEMORY:CUSTOM1"
		"		TASK_FIND_NODE_COVER_FROM_ENEMY		0"
		"		TASK_RUN_PATH						0"
		"		TASK_FORGET					MEMORY:CUSTOM1"
		"		TASK_WAIT_FOR_MOVEMENT				0"
		"		TASK_REMEMBER				MEMORY:INCOVER"
		"		TASK_FACE_ENEMY				0"
		"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_RANGE_ATTACK1"

		"	"
		"	Interrupts"
	)

	//=========================================================
	// Fail Schedule
	//=========================================================

	//=========================================================
	// > SCHED_ASSASSIN_FAIL
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_FAIL,
	
		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_IDLE"
		"		TASK_WAIT_FACE_ENEMY	2"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_CHASE_ENEMY"
		"	"
		"	Interrupts"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_HEAR_DANGER"
		"		COND_HEAR_PLAYER"
	)


	

	//=========================================================
	// > SCHED_ASSASSIN_TAKE_COVER_FROM_ENEMY1
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_TAKE_COVER_FROM_ENEMY1,
	
		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_WAIT				0.2"
		"		TASK_SET_FAIL_SCHEDULE	SCHEDULE:SCHED_RANGE_ATTACK1"
		"		TASK_FIND_COVER_FROM_ENEMY 0"
		"		TASK_RUN_PATH 0"
		"		TASK_WAIT_FOR_MOVEMENT 0"
		"		TASK_REMEMBER			MEMORY:INCOVER"
		"		TASK_FACE_ENEMY			0"
		"	"
		"	Interrupts"
		"	COND_CAN_MELEE_ATTACK1"
		"	COND_NEW_ENEMY"
		"	COND_HEAR_DANGER"
		
	)

	//=========================================================
	// > SCHED_ASSASSIN_TAKE_COVER_FROM_ENEMY2
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_TAKE_COVER_FROM_ENEMY2,
	
		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_WAIT				0.2"
		"		TASK_FACE_ENEMY			0"
		"		TASK_RANGE_ATTACK1		0"
		"		TASK_SET_FAIL_SCHEDULE	SCHEDULE:SCHED_RANGE_ATTACK1"
		"		TASK_FIND_COVER_FROM_ENEMY 0"
		"		TASK_RUN_PATH 0"
		"		TASK_WAIT_FOR_MOVEMENT 0"
		"		TASK_REMEMBER			MEMORY:INCOVER"
		"		TASK_FACE_ENEMY			0"
		"	"
		"	Interrupts"
		"	COND_CAN_MELEE_ATTACK1"
		"	COND_NEW_ENEMY"
		"	COND_HEAR_DANGER"
		
	)



	//=========================================================
	// hide from the loudest sound source
	//=========================================================

	//=========================================================
	// > SCHED_ASSASSIN_TAKE_COVER_FROM_BEST_SOUND
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_TAKE_COVER_FROM_BEST_SOUND,
	
		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE	SCHEDULE:SCHED_MELEE_ATTACK1"
		"		TASK_STOP_MOVING		0"
		"		TASK_FIND_COVER_FROM_BEST_SOUND	0"
		"		TASK_RUN_PATH			0"
		"		TASK_WAIT_FOR_MOVEMENT	0"
		"		TASK_REMEMBER			MEMORY:INCOVER"
		"		TASK_TURN_LEFT			179"
		"	"
		"	Interrupts"
		"	COND_NEW_ENEMY"
		
	)

	//=========================================================
	// > SCHED_ASSASSIN_HIDE
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_HIDE,
	
		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_IDLE"
		"		TASK_WAIT				2.0"
		"		TASK_SET_SCHEDULE		SCHEDULE:SCHED_CHASE_ENEMY"
	
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_SEE_FEAR"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_PROVOKED"
		"		COND_HEAR_DANGER"
	)

	//=========================================================
	// > SCHED_ASSASSIN_HUNT
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_ASSASSIN_HUNT,
	
		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_ASSASSIN_TAKE_COVER_FROM_ENEMY2"
		"		TASK_GET_PATH_TO_ENEMY		0"
		"		TASK_RUN_PATH				0"
		"		TASK_WAIT_FOR_MOVEMENT		0"
		
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_HEAR_DANGER"
	)

AI_END_CUSTOM_NPC()
