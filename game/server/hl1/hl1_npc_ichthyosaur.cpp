//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Ichthyosaur - buh bum...  buh bum...
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "beam_shared.h"
#include	"ai_default.h"
#include	"ai_task.h"
#include	"ai_schedule.h"
#include	"ai_node.h"
#include	"ai_hull.h"
#include	"ai_hint.h"
#include	"ai_memory.h"
#include	"ai_route.h"
#include	"ai_motor.h"
#include "activitylist.h"
#include "game.h"
#include "npcevent.h"
#include "player.h"
#include "entitylist.h"
#include "soundenvelope.h"
#include "shake.h"
#include "ndebugoverlay.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "hl1_npc_ichthyosaur.h"

ConVar sk_ichthyosaur_health ( "sk_ichthyosaur_health", "0" );
ConVar sk_ichthyosaur_shake ( "sk_ichthyosaur_shake", "0" );

#define	ICH_SWIM_SPEED_WALK		150
#define	ICH_SWIM_SPEED_RUN		400
#define PROBE_LENGTH			150

enum IchthyosaurMoveType_t
{
	ICH_MOVETYPE_SEEK = 0,	// Fly through the target without stopping.
	ICH_MOVETYPE_ARRIVE		// Slow down and stop at target.
};

enum
{
	SCHED_SWIM_AROUND = LAST_SHARED_SCHEDULE + 1,
	SCHED_SWIM_AGITATED,
	SCHED_CIRCLE_ENEMY,
	SCHED_TWITCH_DIE,
};


//=========================================================
// monster-specific tasks and states
//=========================================================
enum 
{
	TASK_ICHTHYOSAUR_CIRCLE_ENEMY = LAST_SHARED_TASK + 1,
	TASK_ICHTHYOSAUR_SWIM,
	TASK_ICHTHYOSAUR_FLOAT,
};


LINK_ENTITY_TO_CLASS( monster_ichthyosaur, CNPC_Ichthyosaur );

BEGIN_DATADESC( CNPC_Ichthyosaur )

	// Function Pointers
	DEFINE_ENTITYFUNC( BiteTouch ),

	DEFINE_FIELD( m_SaveVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_idealDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_flBlink, FIELD_TIME ),
	DEFINE_FIELD( m_flEnemyTouched, FIELD_TIME ),
	DEFINE_FIELD( m_bOnAttack, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flMaxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_flMinSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_flMaxDist, FIELD_FLOAT ),
	DEFINE_FIELD( m_flNextAlert, FIELD_TIME ),
	DEFINE_FIELD( m_flMaxSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecLastMoveTarget, FIELD_VECTOR ),
	DEFINE_FIELD( m_bHasMoveTarget, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flFlyingSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( m_flLastAttackSound, FIELD_TIME ),

	DEFINE_INPUTFUNC( FIELD_VOID, "StartCombat", InputStartCombat ),
	DEFINE_INPUTFUNC( FIELD_VOID, "EndCombat", InputEndCombat ),

END_DATADESC()

//=========================================================
// AI Schedules Specific to this monster
//=========================================================


AI_BEGIN_CUSTOM_NPC( monster_ichthyosaur, CNPC_Ichthyosaur )

DECLARE_TASK ( TASK_ICHTHYOSAUR_SWIM )
DECLARE_TASK ( TASK_ICHTHYOSAUR_CIRCLE_ENEMY )
DECLARE_TASK ( TASK_ICHTHYOSAUR_FLOAT )

	//=========================================================
	// > SCHED_SWIM_AROUND
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_SWIM_AROUND,

		"	Tasks"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_GLIDE"
		"		TASK_ICHTHYOSAUR_SWIM		0.0"
		" "
		"	Interrupts"
		"   COND_LIGHT_DAMAGE"
		"   COND_HEAVY_DAMAGE"
		"   COND_SEE_ENEMY"
		"   COND_NEW_ENEMY"
		"   COND_HEAR_PLAYER"
		"   COND_HEAR_COMBAT"
	)
	//=========================================================
	// > SCHED_SWIM_AGITATED
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_SWIM_AGITATED,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_SWIM"
		"		TASK_WAIT					2.0"
		" "
	)
	//=========================================================
	// > SCHED_CIRCLE_ENEMY
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CIRCLE_ENEMY,

		"	Tasks"
		"		TASK_SET_ACTIVITY					ACTIVITY:ACT_GLIDE"
		"		TASK_ICHTHYOSAUR_CIRCLE_ENEMY		0.0"
		" "
		"	Interrupts"
		"   COND_LIGHT_DAMAGE"
		"   COND_HEAVY_DAMAGE"
		"   COND_NEW_ENEMY"
		"   COND_CAN_MELEE_ATTACK1"
		"   COND_CAN_RANGE_ATTACK1"
	)
	//=========================================================
	// > SCHED_TWITCH_DIE
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_TWITCH_DIE,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_SOUND_DIE				0"
		"		TASK_DIE					0"
		"		TASK_ICHTHYOSAUR_FLOAT		0"
		" "
	)

AI_END_CUSTOM_NPC()


//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_Ichthyosaur::Precache()
{
	PrecacheModel("models/icky.mdl");

	PrecacheModel("sprites/lgtning.vmt");

	PrecacheScriptSound( "Ichthyosaur.Bite" );
	PrecacheScriptSound( "Ichthyosaur.Alert" );
	PrecacheScriptSound( "Ichthyosaur.Pain" );
	PrecacheScriptSound( "Ichthyosaur.Die" );
	PrecacheScriptSound( "Ichthyosaur.Idle" );
	PrecacheScriptSound( "Ichthyosaur.Attack" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Ichthyosaur::Spawn( void )
{
	Precache( );

	SetModel( "models/icky.mdl");
	UTIL_SetSize( this, Vector( -32, -32, -32 ), Vector( 32, 32, 32 ) );
	
	SetHullType(HULL_LARGE_CENTERED);
	SetHullSizeNormal();
	SetDefaultEyeOffset();

	// Use our hitboxes to determine our render bounds
	CollisionProp()->SetSurroundingBoundsType( USE_HITBOXES );

	SetNavType( NAV_FLY );
	m_NPCState			= NPC_STATE_NONE;
	
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	AddFlag( FL_FLY | FL_STEPMOVEMENT );

	m_flGroundSpeed			= ICH_SWIM_SPEED_RUN;

	m_bloodColor		= BLOOD_COLOR_YELLOW;
	m_iHealth			= sk_ichthyosaur_health.GetFloat();
	m_iMaxHealth		= m_iHealth;
	m_flFieldOfView		= -0.707;	// 270 degrees
	
	AddFlag( FL_SWIM );

	m_flFlyingSpeed		= ICHTHYOSAUR_SPEED;

	SetDistLook( 1024 );

	
	SetTouch( &CNPC_Ichthyosaur::BiteTouch );

	m_idealDist = 384;
	m_flMinSpeed = 80;
	m_flMaxSpeed = 400;
	m_flMaxDist = 384;
	m_flLastAttackSound = gpGlobals->curtime;

	Vector vforward;
	AngleVectors(GetAbsAngles(), &vforward );
	VectorNormalize ( vforward );
	SetAbsVelocity( m_flFlyingSpeed * vforward );
	m_SaveVelocity = GetAbsVelocity();

	CapabilitiesClear();
	CapabilitiesAdd( bits_CAP_MOVE_FLY | bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_RANGE_ATTACK1 );

	m_bOnAttack = false;

	NPCInit();
}


//=========================================================
//=========================================================
int CNPC_Ichthyosaur::TranslateSchedule( int scheduleType )
{
	switch	( scheduleType )
	{
	case SCHED_IDLE_WALK:
		return SCHED_SWIM_AROUND;
	case SCHED_STANDOFF:
		return SCHED_CIRCLE_ENEMY;
	case SCHED_FAIL:
		return SCHED_SWIM_AGITATED;
	case SCHED_DIE:
		return SCHED_TWITCH_DIE;
	case SCHED_CHASE_ENEMY:
		
		if ( m_flLastAttackSound < gpGlobals->curtime )
		{
			AttackSound();
			m_flLastAttackSound = gpGlobals->curtime + random->RandomFloat( 2.0f, 4.0f );
		}

		break;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

int CNPC_Ichthyosaur::SelectSchedule()
{
	switch(m_NPCState)
	{
	case NPC_STATE_IDLE:
		m_flFlyingSpeed = 80;
		m_flMaxSpeed = 80;
		return TranslateSchedule( SCHED_IDLE_WALK );

	case NPC_STATE_ALERT:
		m_flFlyingSpeed = 150;
		m_flMaxSpeed = 150;
		return TranslateSchedule( SCHED_IDLE_WALK );

	case NPC_STATE_COMBAT:
		m_flMaxSpeed = 400;

		// eat them
		if ( HasCondition( COND_CAN_MELEE_ATTACK1 ) )
		{
			return TranslateSchedule( SCHED_MELEE_ATTACK1 );
		}

		// chase them down and eat them
		if ( HasCondition( COND_CAN_RANGE_ATTACK1 ) )
		{
			return TranslateSchedule( SCHED_CHASE_ENEMY );
		}

		if ( HasCondition( COND_HEAVY_DAMAGE ) )
		{
			m_bOnAttack = true;
		}

		if ( GetHealth() < GetMaxHealth() - 20 )
		{
			m_bOnAttack = true;
		}

		return TranslateSchedule( SCHED_STANDOFF );
	}

	return BaseClass::SelectSchedule();
}


bool CNPC_Ichthyosaur::OverrideMove( float flInterval )
{
	if ( m_lifeState == LIFE_ALIVE )
	{
		if ( m_NPCState != NPC_STATE_SCRIPT)
		{
			MoveExecute_Alive( flInterval );
		}
	}	
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CNPC_Ichthyosaur::MoveExecute_Alive(float flInterval)
{
	Vector vStart = GetAbsOrigin();
	Vector vForward, vRight, vUp;

	if (GetNavigator()->IsGoalActive())
	{
		Vector vecDir =  ( GetNavigator()->GetPath()->CurWaypointPos() - GetAbsOrigin());	
		VectorNormalize( vecDir );

		m_SaveVelocity = vecDir * m_flFlyingSpeed;
	}

	// If we're attacking, accelerate to max speed
	if (m_bOnAttack && m_flFlyingSpeed < m_flMaxSpeed)
	{
		m_flFlyingSpeed = MIN( m_flMaxSpeed, m_flFlyingSpeed+40 );
	}

	if (m_flFlyingSpeed < 180)
	{
		if (GetIdealActivity() == ACT_SWIM)
			SetActivity( ACT_GLIDE );
		if (GetIdealActivity() == ACT_GLIDE)
			m_flPlaybackRate = m_flFlyingSpeed / 150.0;
	}
	else
	{
		if (GetIdealActivity() == ACT_GLIDE)
			SetActivity( ACT_SWIM );
		if (GetIdealActivity() == ACT_SWIM)
			m_flPlaybackRate = m_flFlyingSpeed / 300.0;
	}

	// Steering
	QAngle angSaveAngles;
	VectorAngles( m_SaveVelocity, angSaveAngles );
	AngleVectors(angSaveAngles, &vForward, &vRight, &vUp);

	Vector z;
	float frac;

	Vector f, u, l, r, d;
	f = DoProbe(vStart + (PROBE_LENGTH * vForward) );
	r = DoProbe(vStart + ((PROBE_LENGTH/3) * (vForward + vRight)) );
	l = DoProbe(vStart + ((PROBE_LENGTH/3) * (vForward - vRight)) );	
	u = DoProbe(vStart + ((PROBE_LENGTH/3) * (vForward + vUp)) );
	d = DoProbe(vStart + ((PROBE_LENGTH/3) * (vForward - vUp)) );

	Vector SteeringVector = f+r+l+u+d;
	
	if( ProbeZ( vStart + vForward*50, vUp*50, &frac ) )
	{
		// reflect off the water surface
		m_SaveVelocity.z = -10;
	}

	m_SaveVelocity += SteeringVector/32;
	VectorNormalize( m_SaveVelocity );

	angSaveAngles = GetAbsAngles();

	AngleVectors( angSaveAngles, &vForward, &vRight, &vUp );

	float flDot = DotProduct( vForward, m_SaveVelocity );
	if (flDot > 0.5)
		m_SaveVelocity = m_SaveVelocity * m_flFlyingSpeed;
	else if (flDot > 0)
		m_SaveVelocity = m_SaveVelocity * m_flFlyingSpeed * (flDot + 0.5);
	else
		m_SaveVelocity = m_SaveVelocity * 80;

	SetAbsVelocity( m_SaveVelocity );
	
	VectorAngles( m_SaveVelocity, angSaveAngles );
	
	//
	// Smooth Pitch
	//
	if (angSaveAngles.x > 180)
		angSaveAngles.x = angSaveAngles.x - 360;
	
	QAngle angAbsAngles = GetAbsAngles();

	angAbsAngles.x = clamp( UTIL_Approach(angSaveAngles.x, angAbsAngles.x, 10 ), -60, 60 );

	//
	// Smooth Yaw and generate Roll
	//
	float turn = 360;

	if (fabs(angSaveAngles.y - angAbsAngles.y) < fabs(turn))
	{
		turn = angSaveAngles.y - angAbsAngles.y;
	}
	if (fabs(angSaveAngles.y - angAbsAngles.y + 360) < fabs(turn))
	{
		turn = angSaveAngles.y - angAbsAngles.y + 360;
	}
	if (fabs(angSaveAngles.y - angAbsAngles.y - 360) < fabs(turn))
	{
		turn = angSaveAngles.y - angAbsAngles.y - 360;
	}	

	float speed = m_flFlyingSpeed * 0.4;

	if (fabs(turn) > speed)
	{
		if (turn < 0.0)
		{
			turn = -speed;
		}
		else
		{
			turn = speed;
		}
	}
	angAbsAngles.y += turn;
	angAbsAngles.z -= turn;
	angAbsAngles.y = fmod((angAbsAngles.y + 360.0), 360.0);
	
	// don't touch bone controller, makes swim animation look funky with all these hard turns.
//	static float yaw_adj;	
//	yaw_adj = yaw_adj * 0.8 + turn;
//	SetBoneController( 0, -yaw_adj / 4.0 );

	//
	// Roll Smoothing
	//
	turn = 360;
	float flTempRoll = angAbsAngles.z;

	if (fabs(angSaveAngles.z - angAbsAngles.z) < fabs(turn))
	{
		turn = angSaveAngles.z - angAbsAngles.z;
	}
	if (fabs(angSaveAngles.z - angAbsAngles.z + 360) < fabs(turn))
	{
		turn = angSaveAngles.z - angAbsAngles.z + 360;
	}
	if (fabs(angSaveAngles.z - angAbsAngles.z - 360) < fabs(turn))
	{
		turn = angSaveAngles.z - angAbsAngles.z - 360;
	}
	speed = m_flFlyingSpeed/2 * 0.1;
	if (fabs(turn) < speed)
	{
		flTempRoll += turn;
	}
	else
	{
		if (turn < 0.0)
		{
			flTempRoll -= speed;
		}
		else
		{
			flTempRoll += speed;
		}
	}

	angAbsAngles.z = clamp( UTIL_Approach(flTempRoll, angAbsAngles.z, 5 ), -20, 20 );

	SetAbsAngles( angAbsAngles );

	//Move along the current velocity vector
	if ( WalkMove( m_SaveVelocity * flInterval, MASK_NPCSOLID ) == false )
	{
		//Attempt a half-step
		if ( WalkMove( (m_SaveVelocity*0.5f) * flInterval,  MASK_NPCSOLID) == false )
		{
			//Restart the velocity
			//VectorNormalize( m_vecVelocity );
			m_SaveVelocity *= 0.25f;
		}
		else
		{
			//Cut our velocity in half
			m_SaveVelocity *= 0.5f;
		}
	}

	SetAbsVelocity( m_SaveVelocity );
}

void CNPC_Ichthyosaur::InputStartCombat( inputdata_t &input )
{
	m_bOnAttack = true;
}

void CNPC_Ichthyosaur::InputEndCombat( inputdata_t &input )
{
	m_bOnAttack = false;
}

//=========================================================
// Start task - selects the correct activity and performs
// any necessary calculations to start the next task on the
// schedule.
//=========================================================
void CNPC_Ichthyosaur::StartTask(const Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_ICHTHYOSAUR_CIRCLE_ENEMY:
		break;
	case TASK_ICHTHYOSAUR_SWIM:
		break;
	case TASK_SMALL_FLINCH:
		if (m_idealDist > 128)
		{
			m_flMaxDist = 512;
			m_idealDist = 512;
		}
		else
		{
			m_bOnAttack = true;
		}
		BaseClass::StartTask(pTask);
		break;

	case TASK_ICHTHYOSAUR_FLOAT:
		m_nSkin = EYE_BASE;
		SetSequenceByName( "bellyup" );
		break;

	default:
		BaseClass::StartTask(pTask);
		break;
	}
}

void CNPC_Ichthyosaur::RunTask(const Task_t *pTask )
{
	QAngle angles = GetAbsAngles();

	switch ( pTask->iTask )
	{
	case TASK_ICHTHYOSAUR_CIRCLE_ENEMY:

		if (GetEnemy() == NULL )
		{
			TaskComplete( );
		}
		else if (FVisible( GetEnemy() ))
		{
			Vector vecFrom = GetEnemy()->EyePosition( );

			Vector vecDelta = GetAbsOrigin() - vecFrom;
			VectorNormalize( vecDelta );
			Vector vecSwim = CrossProduct( vecDelta, Vector( 0, 0, 1 ) );
			VectorNormalize( vecSwim );
			
			if (DotProduct( vecSwim, m_SaveVelocity ) < 0)
			{
				vecSwim = vecSwim * -1.0;
			}

			Vector vecPos = vecFrom + vecDelta * m_idealDist + vecSwim * 32;

			trace_t tr;
		
//			UTIL_TraceHull( vecFrom, vecPos, ignore_monsters, large_hull, m_hEnemy->edict(), &tr );
			UTIL_TraceEntity( this, vecFrom, vecPos, MASK_NPCSOLID, &tr );

			if (tr.fraction > 0.5)
			{
				vecPos = tr.endpos;
			}

			Vector vecNorm = vecPos - GetAbsOrigin();
			VectorNormalize( vecNorm );
			m_SaveVelocity = m_SaveVelocity * 0.8 + 0.2 * vecNorm * m_flFlyingSpeed;

			if (HasCondition( COND_ENEMY_FACING_ME ) && GetEnemy()->FVisible( this ))
			{
				m_flNextAlert -= 0.1;

				if (m_idealDist < m_flMaxDist)
				{
					m_idealDist += 4;
				}
				if (m_flFlyingSpeed > m_flMinSpeed)
				{
					m_flFlyingSpeed -= 2;
				}
				else if (m_flFlyingSpeed < m_flMinSpeed)
				{
					m_flFlyingSpeed += 2;
				}
				if (m_flMinSpeed < m_flMaxSpeed)
				{
					m_flMinSpeed += 0.5;
				}
			}
			else 
			{
				m_flNextAlert += 0.1;

				if (m_idealDist > 128)
				{
					m_idealDist -= 4;
				}
				if (m_flFlyingSpeed < m_flMaxSpeed)
				{
					m_flFlyingSpeed += 4;
				}
			}
		}
		else
		{
			m_flNextAlert = gpGlobals->curtime + 0.2;
		}

		if (m_flNextAlert < gpGlobals->curtime)
		{
			// ALERT( at_console, "AlertSound()\n");
			AlertSound( );
			m_flNextAlert = gpGlobals->curtime + RandomFloat( 3, 5 );
		}

		break;
	case TASK_ICHTHYOSAUR_SWIM:
		if ( IsSequenceFinished() )
		{
			TaskComplete( );
		}
		break;
	case TASK_DIE:
		if ( IsSequenceFinished() )
		{
//			pev->deadflag = DEAD_DEAD;

			TaskComplete( );
		}
		break;

	case TASK_ICHTHYOSAUR_FLOAT:
		angles.x = UTIL_ApproachAngle( 0, angles.x, 20 );
		SetAbsAngles( angles );

//		SetAbsVelocity( GetAbsVelocity() * 0.8 );
//		if (pev->waterlevel > 1 && GetAbsVelocity().z < 64)
//		{
//			pev->velocity.z += 8;
//		}
//		else 
//		{
//			pev->velocity.z -= 8;
//		}
		// ALERT( at_console, "%f\n", m_vecAbsVelocity.z );
		break;

	default: 
		BaseClass::RunTask( pTask );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get our conditions for a melee attack
// Input  : flDot - 
//			flDist - 
// Output : int
//-----------------------------------------------------------------------------
int CNPC_Ichthyosaur::MeleeAttack1Conditions( float flDot, float flDist )
{
	// Enemy must be submerged with us
	if ( GetEnemy() && GetEnemy()->GetWaterLevel() != GetWaterLevel() )
		return COND_NONE;

	Vector	predictedDir	= ( (GetEnemy()->GetAbsOrigin()+(GetEnemy()->GetSmoothedVelocity())) - GetAbsOrigin() );	
	float	flPredictedDist = VectorNormalize( predictedDir );
	
	Vector	vBodyDir;
	GetVectors( &vBodyDir, NULL, NULL );

	float	flPredictedDot	= DotProduct( predictedDir, vBodyDir );

	if ( flPredictedDot < 0.8f )
		return COND_NOT_FACING_ATTACK;

	if ( ( flPredictedDist > ( GetAbsVelocity().Length() * 0.5f) ) && ( flDist > 128.0f ) )
		return COND_TOO_FAR_TO_ATTACK;

	return COND_CAN_MELEE_ATTACK1;
}

//=========================================================
// RangeAttack1Conditions
//=========================================================
int CNPC_Ichthyosaur::RangeAttack1Conditions( float flDot, float flDist )
{
	CBaseEntity *pEnemy = GetEnemy();
	
	if( pEnemy && pEnemy->GetWaterLevel() != GetWaterLevel() )
	{
		return COND_NONE;
	}

	if ( flDot > -0.7 && (m_bOnAttack || ( flDist <= 192 && m_idealDist <= 192)))
	{
		return COND_CAN_RANGE_ATTACK1;
	}

	return COND_NONE;
}

void CNPC_Ichthyosaur::BiteTouch( CBaseEntity *pOther )
{
	// bite if we hit who we want to eat
	if ( pOther == GetEnemy() ) 
	{
		m_flEnemyTouched = gpGlobals->curtime + 0.2f;
		m_bOnAttack = true;
	}
}

#define ICHTHYOSAUR_AE_SHAKE_RIGHT 1
#define ICHTHYOSAUR_AE_SHAKE_LEFT  2

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_Ichthyosaur::HandleAnimEvent( animevent_t *pEvent )
{
	int bDidAttack = FALSE;
	Vector vForward, vRight;
	QAngle angles = GetAbsAngles();
	AngleVectors( angles, &vForward, &vRight, NULL );

	switch( pEvent->event )
	{
	case ICHTHYOSAUR_AE_SHAKE_RIGHT:
	case ICHTHYOSAUR_AE_SHAKE_LEFT:
		{
			CBaseEntity* hEnemy = GetEnemy();

			if (hEnemy != NULL && FVisible( hEnemy ))
			{
				CBaseEntity *pHurt = GetEnemy();

				if ( m_flEnemyTouched > gpGlobals->curtime && (pHurt->BodyTarget( GetAbsOrigin() ) - GetAbsOrigin()).Length() > (32+16+32) )
					break;

				Vector vecShootOrigin = Weapon_ShootPosition();
				Vector vecShootDir = GetShootEnemyDir( vecShootOrigin );

				if (DotProduct( vecShootDir, vForward ) > 0.707)
				{
					angles = pHurt->GetAbsAngles();
					m_bOnAttack = true;
					pHurt->SetAbsVelocity( pHurt->GetAbsVelocity() - vRight * 300 );
					if (pHurt->IsPlayer())
					{
						angles.x += RandomFloat( -35, 35 );
						angles.y += RandomFloat( -90, 90 );
						angles.z = 0;
						((CBasePlayer*) pHurt)->SetPunchAngle( angles );
					}

					CTakeDamageInfo info( this, this, sk_ichthyosaur_shake.GetInt(), DMG_SLASH );
					CalculateMeleeDamageForce( &info, vForward, pHurt->GetAbsOrigin() );
					pHurt->TakeDamage( info );
				}
			}

			// Do our bite sound
			BiteSound();

			bDidAttack = TRUE;
		}
		break;
	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
	// make bubbles when he attacks
	if (bDidAttack)
	{
		Vector vecSrc = GetAbsOrigin() + vForward * 32;
		UTIL_Bubbles( vecSrc - Vector( 8, 8, 8 ), vecSrc + Vector( 8, 8, 8 ), 16 );
	}
}

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
Class_T CNPC_Ichthyosaur::Classify ( void )
{
	return	CLASS_ALIEN_MONSTER;
}

void CNPC_Ichthyosaur::NPCThink ( void )
{
	// blink the eye
	if (m_flBlink < gpGlobals->curtime)
	{
		m_nSkin = EYE_CLOSED;
		if (m_flBlink + 0.2 < gpGlobals->curtime)
		{
			m_flBlink = gpGlobals->curtime + random->RandomFloat( 3, 4 );
			if (m_bOnAttack)
				m_nSkin = EYE_MAD;
			else
				m_nSkin = EYE_BASE;
		}
	}

	BaseClass::NPCThink( );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : speed to move at
//-----------------------------------------------------------------------------
float CNPC_Ichthyosaur::GetGroundSpeed( void )
{
	if ( GetIdealActivity() == ACT_GLIDE )
		return ICH_SWIM_SPEED_WALK;

	return ICH_SWIM_SPEED_RUN;
}

Vector CNPC_Ichthyosaur::DoProbe( const Vector &Probe )
{
	Vector WallNormal = Vector(0,0,-1); // WATER normal is Straight Down for fish.
	float frac = 1.0;
	bool bBumpedSomething = false;	// = ProbeZ(GetAbsOrigin(), Probe, &frac);

	trace_t tr;
	UTIL_TraceEntity( this, GetAbsOrigin(), Probe, MASK_NPCSOLID, &tr );
	if ( tr.allsolid || tr.fraction < 0.99 )
	{
		if (tr.fraction < 0.0) tr.fraction = 0.0;
		if (tr.fraction > 1.0) tr.fraction = 1.0;
		if (tr.fraction < frac)
		{
			frac = tr.fraction;
			bBumpedSomething = true;
			WallNormal = tr.plane.normal;
		}
	}
	//NOTENOTE: Debug start
	//NDebugOverlay::Line( tr.startpos, tr.endpos, 255.0f*(1.0f-tr.fraction), 255.0f * tr.fraction, 0.0f, true, 0.05f );
	//NOTENOTE: Debug end

	if (bBumpedSomething && (GetEnemy() == NULL || !tr.m_pEnt || tr.m_pEnt->entindex() != GetEnemy()->entindex()))
	{
		Vector ProbeDir = Probe - GetAbsOrigin();

		Vector NormalToProbeAndWallNormal = CrossProduct(ProbeDir, WallNormal);
		Vector SteeringVector = CrossProduct( NormalToProbeAndWallNormal, ProbeDir);

		VectorNormalize( WallNormal );
		VectorNormalize( m_SaveVelocity );

		float SteeringForce = m_flFlyingSpeed * (1-frac) * ( DotProduct( WallNormal, m_SaveVelocity ) );
		if (SteeringForce < 0.0)
		{
			SteeringForce = -SteeringForce;
		}

		Vector vSteering = SteeringVector;

		VectorNormalize( vSteering );
		SteeringVector = SteeringForce * vSteering;
	
		//NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + (SteeringVector*4.0f), 0, 255, 255, true, 0.1f );
		
		return SteeringVector;
	}
	return Vector(0, 0, 0);
}

bool CNPC_Ichthyosaur::ProbeZ( const Vector &position, const Vector &probe, float *pFraction)
{
	int iPositionContents = UTIL_PointContents( position );
	int iProbeContents = UTIL_PointContents( position );

	if( !(iPositionContents & MASK_WATER) )
	{
		// we're not in the water anymore
		*pFraction = 0.0;
		return true; // We hit a water boundary because we are where we don't belong.
	}
	if( iProbeContents == iPositionContents )
	{
		// The probe is entirely inside the water
		*pFraction = 1.0;
		return false;
	}

	Vector ProbeUnit = (probe-position);
	VectorNormalize( ProbeUnit );
	float ProbeLength = (probe-position).Length();
	float maxProbeLength = ProbeLength;
	float minProbeLength = 0;

	float diff = maxProbeLength - minProbeLength;
	while (diff > 1.0)
	{
		float midProbeLength = minProbeLength + diff/2.0;
		Vector midProbeVec = midProbeLength * ProbeUnit;
		if (UTIL_PointContents(position+midProbeVec) == iPositionContents)
		{
			minProbeLength = midProbeLength;
		}
		else
		{
			maxProbeLength = midProbeLength;
		}
		diff = maxProbeLength - minProbeLength;
	}
	*pFraction = minProbeLength/ProbeLength;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Ichthyosaur::FVisible( CBaseEntity *pEntity, int traceMask, CBaseEntity **ppBlocker )
{
	// Can't see entities that aren't in water
	if ( pEntity->GetWaterLevel() < 1 )
		return false;

	return BaseClass::FVisible( pEntity, traceMask, ppBlocker );
}

void CNPC_Ichthyosaur::IdleSound( void )	
{ 
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Ichthyosaur.Idle" );
}

void CNPC_Ichthyosaur::AlertSound( void ) 
{ 
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Ichthyosaur.Alert" );
}

void CNPC_Ichthyosaur::AttackSound( void ) 
{ 
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Ichthyosaur.Attack" );
}

void CNPC_Ichthyosaur::BiteSound( void ) 
{ 
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Ichthyosaur.Bite" );
}

void CNPC_Ichthyosaur::DeathSound( const CTakeDamageInfo &info ) 
{ 
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Ichthyosaur.Die" );
}

void CNPC_Ichthyosaur::PainSound( const CTakeDamageInfo &info )	
{ 
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Ichthyosaur.Pain" );
}

//-----------------------------------------------------------------------------
void CNPC_Ichthyosaur::GatherEnemyConditions( CBaseEntity *pEnemy )
{
	// Do the base class
	BaseClass::GatherEnemyConditions( pEnemy );

	if ( HasCondition( COND_ENEMY_UNREACHABLE ) == false )
	{
		if( pEnemy == NULL || pEnemy->GetWaterLevel() != GetWaterLevel() )
		{
			SetCondition( COND_ENEMY_UNREACHABLE );
		}
	}
}
