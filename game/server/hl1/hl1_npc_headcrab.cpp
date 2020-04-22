//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the headcrab, a tiny, jumpy alien parasite.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "game.h"
#include "ai_default.h"
#include "ai_schedule.h"
#include "ai_hull.h"
#include "ai_route.h"
#include "ai_motor.h"
#include "npcevent.h"
#include "hl1_npc_headcrab.h"
#include "gib.h"
//#include "AI_Interactions.h"
#include "ndebugoverlay.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "movevars_shared.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

extern void ClearMultiDamage(void);
extern void ApplyMultiDamage( void );

ConVar	sk_headcrab_health( "sk_headcrab_health","20");
ConVar	sk_headcrab_dmg_bite( "sk_headcrab_dmg_bite","10");

#define CRAB_ATTN_IDLE				(float)1.5
#define HEADCRAB_GUTS_GIB_COUNT		1
#define HEADCRAB_LEGS_GIB_COUNT		3
#define HEADCRAB_ALL_GIB_COUNT		5

#define HEADCRAB_MAX_JUMP_DIST		256

#define HEADCRAB_RUNMODE_ACCELERATE		1
#define HEADCRAB_RUNMODE_IDLE			2
#define HEADCRAB_RUNMODE_DECELERATE		3
#define HEADCRAB_RUNMODE_FULLSPEED		4
#define HEADCRAB_RUNMODE_PAUSE			5

#define HEADCRAB_RUN_MINSPEED	0.5
#define HEADCRAB_RUN_MAXSPEED	1.0

#define	HC_AE_JUMPATTACK		( 2 )

BEGIN_DATADESC( CNPC_Headcrab )
	// m_nGibCount - don't save

	// Function Pointers
	DEFINE_ENTITYFUNC( LeapTouch ),
	DEFINE_FIELD( m_vecJumpVel, FIELD_VECTOR ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( monster_headcrab, CNPC_Headcrab );


enum
{
	SCHED_HEADCRAB_RANGE_ATTACK1 = LAST_SHARED_SCHEDULE,
	SCHED_FAST_HEADCRAB_RANGE_ATTACK1,
};


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output : 
//-----------------------------------------------------------------------------
void CNPC_Headcrab::Spawn( void )
{
	Precache();

	SetRenderColor( 255, 255, 255, 255 );

	SetModel( "models/headcrab.mdl" );
	m_iHealth = sk_headcrab_health.GetFloat();

	SetHullType(HULL_TINY);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	SetViewOffset( Vector(6, 0, 11) );		// Position of the eyes relative to NPC's origin.

	m_bloodColor		= BLOOD_COLOR_GREEN;
	m_flFieldOfView		= 0.5;
	m_NPCState			= NPC_STATE_NONE;
	m_nGibCount			= HEADCRAB_ALL_GIB_COUNT;
	
	CapabilitiesClear();
	CapabilitiesAdd( bits_CAP_MOVE_GROUND | bits_CAP_INNATE_RANGE_ATTACK1 );

	NPCInit();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output : 
//-----------------------------------------------------------------------------
void CNPC_Headcrab::Precache( void )
{
	PrecacheModel( "models/headcrab.mdl" );
//	PrecacheModel( "models/hc_squashed01.mdl" );
//	PrecacheModel( "models/gibs/hc_gibs.mdl" );

	PrecacheScriptSound( "Headcrab.Bite" );
	PrecacheScriptSound( "Headcrab.Attack" );
	PrecacheScriptSound( "Headcrab.Idle" );
	PrecacheScriptSound( "Headcrab.Die" );
	PrecacheScriptSound( "Headcrab.Alert" );
	PrecacheScriptSound( "Headcrab.Pain" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_Headcrab::IdleSound()
{
	HeadCrabSound( "Headcrab.Idle" );
}


void CNPC_Headcrab::AlertSound()
{
	HeadCrabSound(  "Headcrab.Alert" );
}


void CNPC_Headcrab::PainSound( const CTakeDamageInfo &info )
{
	HeadCrabSound( "Headcrab.Pain" );
}


void CNPC_Headcrab::DeathSound( const CTakeDamageInfo &info )
{
	HeadCrabSound( "Headcrab.Die" );
}

void CNPC_Headcrab::HeadCrabSound( const char *pchSound )
{
	CPASAttenuationFilter filter( this, ATTN_IDLE );

	CSoundParameters params;
	if ( GetParametersForSound( pchSound, params, NULL ) )
	{
		EmitSound_t ep( params );

		ep.m_flVolume = GetSoundVolume();
		ep.m_nPitch = GetVoicePitch();

		EmitSound( filter, entindex(), ep );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTask - 
//-----------------------------------------------------------------------------
void CNPC_Headcrab::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_RANGE_ATTACK1:
		{
			SetIdealActivity( ACT_RANGE_ATTACK1 );
			SetTouch( &CNPC_Headcrab::LeapTouch );
			break;
		}

		default:
		{
			BaseClass::StartTask( pTask );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTask - 
//-----------------------------------------------------------------------------
void CNPC_Headcrab::RunTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
		case TASK_RANGE_ATTACK1:
		case TASK_RANGE_ATTACK2:
		{
			if ( IsSequenceFinished() )
			{
				TaskComplete();
				SetTouch( NULL );
				SetIdealActivity( ACT_IDLE );
			}
			break;
		}

		default:
		{
			CAI_BaseNPC::RunTask( pTask );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : 
//-----------------------------------------------------------------------------
int CNPC_Headcrab::SelectSchedule( void )
{
	switch ( m_NPCState )
	{
		case NPC_STATE_ALERT:
		{
			if (HasCondition( COND_LIGHT_DAMAGE ) || HasCondition( COND_HEAVY_DAMAGE ))
			{
				if ( fabs( GetMotor()->DeltaIdealYaw() ) < ( 1.0 - m_flFieldOfView) * 60 ) // roughly in the correct direction
				{
					return SCHED_TAKE_COVER_FROM_ORIGIN;
				}
				else if ( SelectWeightedSequence( ACT_SMALL_FLINCH ) != -1 )
				{
					return SCHED_SMALL_FLINCH;
				}
			}
			else if (HasCondition( COND_HEAR_DANGER ) ||
					 HasCondition( COND_HEAR_PLAYER ) ||
					 HasCondition( COND_HEAR_WORLD  ) ||
					 HasCondition( COND_HEAR_COMBAT ))
			{
				return SCHED_ALERT_FACE_BESTSOUND;
			}
			else
			{
				return SCHED_PATROL_WALK;
			}
			break;
		}
	}

	// no special cases here, call the base class
	return BaseClass::SelectSchedule();
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CNPC_Headcrab::Touch( CBaseEntity *pOther )
{ 
	// If someone has smacked me into a wall then gib!
/*	if (m_NPCState == NPC_STATE_DEAD) 
	{
		if (GetAbsVelocity().Length() > 250)
		{
			trace_t tr;
			Vector vecDir = GetAbsVelocity();
			VectorNormalize(vecDir);
			UTIL_TraceLine(GetAbsOrigin(), GetAbsOrigin() + vecDir * 100, 
				MASK_SOLID_BRUSHONLY, pev, COLLISION_GROUP_NONE, &tr); 
			float dotPr = DotProduct(vecDir,tr.plane.normal);
			if ((tr.fraction						!= 1.0) && 
				(dotPr <  -0.8) )
			{
				Event_Gibbed();
				// Throw headcrab guts 
				CGib::SpawnSpecificGibs( this, HEADCRAB_GUTS_GIB_COUNT, 300, 400, "models/gibs/hc_gibs.mdl");
			}
		
		}
	}*/
	BaseClass::Touch(pOther);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pevInflictor - 
//			pevAttacker - 
//			flDamage - 
//			bitsDamageType - 
// Output : 
//-----------------------------------------------------------------------------
int CNPC_Headcrab::OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;

	//
	// Don't take any acid damage.
	//
	if ( info.GetDamageType() & DMG_ACID )
	{
		return 0;
	}

	return BaseClass::OnTakeDamage_Alive( info );
}

float CNPC_Headcrab::GetDamageAmount( void )
{
	return sk_headcrab_dmg_bite.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Type - 
// Output : CAI_Schedule *
//-----------------------------------------------------------------------------
int CNPC_Headcrab::TranslateSchedule( int scheduleType )
{
	switch( scheduleType )
	{
		case SCHED_RANGE_ATTACK1:
			return SCHED_HEADCRAB_RANGE_ATTACK1;

		case SCHED_FAIL_TAKE_COVER:
			return SCHED_ALERT_FACE;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Headcrab::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();
	
	//
	// Make the crab coo a little bit in combat state.
	//
	if (( m_NPCState == NPC_STATE_COMBAT ) && ( random->RandomFloat( 0, 5 ) < 0.1 ))
	{
		IdleSound();
	}
}

//-----------------------------------------------------------------------------
// Purpose: For innate melee attack
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Headcrab::RangeAttack1Conditions ( float flDot, float flDist )
{
	if ( gpGlobals->curtime < m_flNextAttack )
	{
		return( 0 );
	}

	if ( !(GetFlags() & FL_ONGROUND) )
	{
		return( 0 );
	}

	if ( flDist > 256 )
	{
		return( COND_TOO_FAR_TO_ATTACK );
	}
	else if ( flDot < 0.65 )
	{
		return( COND_NOT_FACING_ATTACK );
	}

	return( COND_CAN_RANGE_ATTACK1 );
}


//-----------------------------------------------------------------------------
// Purpose: Indicates this monster's place in the relationship table.
// Output : 
//-----------------------------------------------------------------------------
Class_T	CNPC_Headcrab::Classify( void )
{
	return CLASS_ALIEN_PREY; 
}


//-----------------------------------------------------------------------------
// Purpose: Returns the real center of the monster. The bounding box is much larger
//			than the actual creature so this is needed for targetting.
// Output : Vector
//-----------------------------------------------------------------------------
Vector CNPC_Headcrab::Center( void )
{
	return Vector( GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z + 6 );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &posSrc - 
// Output : Vector
//-----------------------------------------------------------------------------
Vector CNPC_Headcrab::BodyTarget( const Vector &posSrc, bool bNoisy ) 
{ 
	return( Center() );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output : 
//-----------------------------------------------------------------------------
float CNPC_Headcrab::MaxYawSpeed ( void )
{
	switch ( GetActivity() )
	{
	case ACT_IDLE:			
		return 30;
		break;

	case ACT_RUN:			
	case ACT_WALK:			
		return 20;
		break;

	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		return 15;
		break;

	case ACT_RANGE_ATTACK1:	
		return 30;
		break;

	default:
		return 30;
		break;
	}

	return BaseClass::MaxYawSpeed();
}


//-----------------------------------------------------------------------------
// Purpose: LeapTouch - this is the headcrab's touch function when it is in the air.
// Input  : *pOther - 
//-----------------------------------------------------------------------------
void CNPC_Headcrab::LeapTouch( CBaseEntity *pOther )
{
	if ( pOther->Classify() == Classify() )
	{
		return;
	}

	// Don't hit if back on ground
	if ( !(GetFlags() & FL_ONGROUND) && ( pOther->IsNPC() || pOther->IsPlayer() ) )
	{
		BiteSound();
		TouchDamage( pOther );
	}

	SetTouch( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Make the sound of this headcrab chomping a target.
// Input  : 
//-----------------------------------------------------------------------------
void CNPC_Headcrab::BiteSound( void )
{
	HeadCrabSound( "Headcrab.Bite" );
}

//-----------------------------------------------------------------------------
// Purpose: Deal the damage from the headcrab's touch attack.
//-----------------------------------------------------------------------------
void CNPC_Headcrab::TouchDamage( CBaseEntity *pOther )
{
	CTakeDamageInfo info( this, this, GetDamageAmount(), DMG_SLASH );
	CalculateMeleeDamageForce( &info, GetAbsVelocity(), GetAbsOrigin() );
	pOther->TakeDamage( info );
}


//-----------------------------------------------------------------------------
// Purpose: Catches the monster-specific messages that occur when tagged
//			animation frames are played.
// Input  : *pEvent - 
//-----------------------------------------------------------------------------
void CNPC_Headcrab::HandleAnimEvent( animevent_t *pEvent )
{
	switch ( pEvent->event )
	{
		case HC_AE_JUMPATTACK:
		{
			SetGroundEntity( NULL );

			//
			// Take him off ground so engine doesn't instantly reset FL_ONGROUND.
			//
			UTIL_SetOrigin( this, GetAbsOrigin() + Vector( 0 , 0 , 1 ));

			Vector vecJumpDir;
			CBaseEntity *pEnemy = GetEnemy();
			if ( pEnemy )
			{
				Vector vecEnemyEyePos = pEnemy->EyePosition();

				float gravity = GetCurrentGravity();
				if ( gravity <= 1 )
				{
					gravity = 1;
				}

				//
				// How fast does the headcrab need to travel to reach my enemy's eyes given gravity?
				//
				float height = ( vecEnemyEyePos.z - GetAbsOrigin().z );
				if ( height < 16 )
				{
					height = 16;
				}
				else if ( height > 120 )
				{
					height = 120;
				}
				float speed = sqrt( 2 * gravity * height );
				float time = speed / gravity;

				//
				// Scale the sideways velocity to get there at the right time
				//
				vecJumpDir = vecEnemyEyePos - GetAbsOrigin();
				vecJumpDir = vecJumpDir / time;

				//
				// Speed to offset gravity at the desired height.
				//
				vecJumpDir.z = speed;

				//
				// Don't jump too far/fast.
				//
				float distance = vecJumpDir.Length();
				if ( distance > 650 )
				{
					vecJumpDir = vecJumpDir * ( 650.0 / distance );
				}
			}
			else
			{
				//
				// Jump hop, don't care where.
				//
				Vector forward, up;
				AngleVectors( GetAbsAngles(), &forward, NULL, &up );
				vecJumpDir = Vector( forward.x, forward.y, up.z ) * 350;
			}

			int iSound = random->RandomInt( 0 , 2 );
			if ( iSound != 0 )
			{
				AttackSound();
			}

			SetAbsVelocity( vecJumpDir );
			m_flNextAttack = gpGlobals->curtime + 2;
			break;
		}

		default:
		{
			CAI_BaseNPC::HandleAnimEvent( pEvent );
			break;
		}
	}
}

void CNPC_Headcrab::AttackSound( void )
{
	HeadCrabSound( "Headcrab.Attack" );
}


//------------------------------------------------------------------------------
//
// Schedules
//
//------------------------------------------------------------------------------

AI_BEGIN_CUSTOM_NPC( monster_headcrab, CNPC_Headcrab )

	//=========================================================
	// > SCHED_HEADCRAB_RANGE_ATTACK1
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_HEADCRAB_RANGE_ATTACK1,
	
		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_IDEAL				0"
		"		TASK_RANGE_ATTACK1			0"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
		"		TASK_FACE_IDEAL				0"
		"		TASK_WAIT_RANDOM			0.5"
		"	"
		"	Interrupts"
		"		COND_ENEMY_OCCLUDED"
		"		COND_NO_PRIMARY_AMMO"
	)
	
	//=========================================================
	// > SCHED_FAST_HEADCRAB_RANGE_ATTACK1
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_FAST_HEADCRAB_RANGE_ATTACK1,
	
		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_IDEAL				0"
		"		TASK_RANGE_ATTACK1			0"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
		"	"
		"	Interrupts"
		"		COND_ENEMY_OCCLUDED"
		"		COND_NO_PRIMARY_AMMO"
	)

AI_END_CUSTOM_NPC()


class CNPC_BabyCrab : public CNPC_Headcrab
{
	DECLARE_CLASS( CNPC_BabyCrab, CNPC_Headcrab );
public:
	void Spawn( void );
	void Precache( void );
	
	unsigned int PhysicsSolidMaskForEntity( void ) const;

	int RangeAttack1Conditions ( float flDot, float flDist );
	float MaxYawSpeed( void ){ return 120.0f; }
	float GetDamageAmount( void );

	virtual int GetVoicePitch( void ) { return PITCH_NORM + random->RandomInt( 40,50 ); }
	virtual float GetSoundVolume( void ) { return 0.8; }
};
LINK_ENTITY_TO_CLASS( monster_babycrab, CNPC_BabyCrab );

unsigned int CNPC_BabyCrab::PhysicsSolidMaskForEntity( void ) const
{
	unsigned int iMask = BaseClass::PhysicsSolidMaskForEntity();

	iMask &= ~CONTENTS_MONSTERCLIP;

	return iMask;
}

void CNPC_BabyCrab::Spawn( void )
{
	CNPC_Headcrab::Spawn();
	SetModel( "models/baby_headcrab.mdl" );
	m_nRenderMode = kRenderTransTexture;

	SetRenderColor( 255, 255, 255, 192 );

	UTIL_SetSize(this, Vector(-12, -12, 0), Vector(12, 12, 24));
	
	m_iHealth	  = sk_headcrab_health.GetFloat() * 0.25;	// less health than full grown
}

void CNPC_BabyCrab::Precache( void )
{
	PrecacheModel( "models/baby_headcrab.mdl" );
	CNPC_Headcrab::Precache();
}

int CNPC_BabyCrab::RangeAttack1Conditions( float flDot, float flDist )
{
	if ( GetFlags() & FL_ONGROUND )
	{
		if ( GetGroundEntity() && ( GetGroundEntity()->GetFlags() & ( FL_CLIENT | FL_NPC ) ) )
			return COND_CAN_RANGE_ATTACK1;

		// A little less accurate, but jump from closer
		if ( flDist <= 180 && flDot >= 0.55 )
			return COND_CAN_RANGE_ATTACK1;
	}

	return COND_NONE;
}

float CNPC_BabyCrab::GetDamageAmount( void )
{
	return sk_headcrab_dmg_bite.GetFloat() * 0.3;
}
