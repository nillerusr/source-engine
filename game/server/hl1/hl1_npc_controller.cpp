//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Bullseyes act as targets for other NPC's to attack and to trigger
//			events 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
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
//#include	"hl1_npc_controller.h"
#include	"ai_basenpc_flyer.h"
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
#include	"Sprite.h"
#include	"ai_moveprobe.h"


//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define	CONTROLLER_AE_HEAD_OPEN		1
#define	CONTROLLER_AE_BALL_SHOOT	2
#define	CONTROLLER_AE_SMALL_SHOOT	3
#define CONTROLLER_AE_POWERUP_FULL	4
#define CONTROLLER_AE_POWERUP_HALF	5

#define CONTROLLER_FLINCH_DELAY			2		// at most one flinch every n secs

#define DIST_TO_CHECK	200

ConVar sk_controller_health ( "sk_controller_health", "60" );
ConVar sk_controller_dmgzap ( "sk_controller_dmgzap", "15" );
ConVar sk_controller_speedball ( "sk_controller_speedball", "650" );
ConVar sk_controller_dmgball ( "sk_controller_dmgball", "3" );

int ACT_CONTROLLER_UP;
int ACT_CONTROLLER_DOWN;
int ACT_CONTROLLER_LEFT;
int ACT_CONTROLLER_RIGHT;
int ACT_CONTROLLER_FORWARD;
int ACT_CONTROLLER_BACKWARD;

class CSprite;
class CNPC_Controller;

enum
{
	TASK_CONTROLLER_CHASE_ENEMY = LAST_SHARED_TASK,
	TASK_CONTROLLER_STRAFE,
	TASK_CONTROLLER_TAKECOVER,
	TASK_CONTROLLER_FAIL,
};

enum
{
	SCHED_CONTROLLER_CHASE_ENEMY = LAST_SHARED_SCHEDULE,
	SCHED_CONTROLLER_STRAFE,
	SCHED_CONTROLLER_TAKECOVER,
	SCHED_CONTROLLER_FAIL,
};

class CControllerNavigator : public CAI_ComponentWithOuter<CNPC_Controller, CAI_Navigator>
{
	typedef CAI_ComponentWithOuter<CNPC_Controller, CAI_Navigator> BaseClass;
public:
	CControllerNavigator( CNPC_Controller *pOuter )
	 :	BaseClass( pOuter )
	{
	}

	bool ActivityIsLocomotive( Activity activity ) { return true; }
};

class CNPC_Controller : public CAI_BaseFlyingBot
{
public:

	DECLARE_CLASS( CNPC_Controller, CAI_BaseFlyingBot );
	DEFINE_CUSTOM_AI;
	DECLARE_DATADESC();

	void Spawn( void );
	void Precache( void );

	float MaxYawSpeed( void ) { return 120.0f; }
	Class_T	Classify ( void ) { return	CLASS_ALIEN_MILITARY; }

	void HandleAnimEvent( animevent_t *pEvent );

	void RunAI( void );

	int RangeAttack1Conditions ( float flDot, float flDist );	// balls
	int RangeAttack2Conditions ( float flDot, float flDist );	// head
	int MeleeAttack1Conditions ( float flDot, float flDist ) { return COND_NONE; }	
	int MeleeAttack2Conditions ( float flDot, float flDist ) { return COND_NONE; }

	int TranslateSchedule( int scheduleType );
	void StartTask ( const Task_t *pTask );
	void RunTask ( const Task_t *pTask );

	void Stop( void );
	bool OverridePathMove( float flInterval );
	bool OverrideMove( float flInterval );

	void MoveToTarget( float flInterval, const Vector &vecMoveTarget );

	Activity NPC_TranslateActivity( Activity eNewActivity );
	void SetActivity ( Activity NewActivity );
	bool ShouldAdvanceRoute( float flWaypointDist );
	int LookupFloat( );

	friend class CControllerNavigator;
	CAI_Navigator *CreateNavigator()
	{
		return new CControllerNavigator( this );
	}

	bool ShouldGib( const CTakeDamageInfo &info );
	bool HasAlienGibs( void ) { return true; }
	bool HasHumanGibs( void ) { return false; }

	float m_flShootTime;
	float m_flShootEnd;

	void PainSound( const CTakeDamageInfo &info );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );
	void DeathSound( const CTakeDamageInfo &info );

	int OnTakeDamage_Alive( const CTakeDamageInfo &info );
	void Event_Killed( const CTakeDamageInfo &info );

	CSprite *m_pBall[2];	// hand balls
	int m_iBall[2];			// how bright it should be
	float m_iBallTime[2];	// when it should be that color
	int m_iBallCurrent[2];	// current brightness

	Vector m_vecEstVelocity;

	Vector m_velocity;
	bool m_fInCombat;

	void SetSequence( int nSequence );

	int IRelationPriority( CBaseEntity *pTarget );
};

class CNPC_ControllerHeadBall : public CAI_BaseNPC
{
public:
	DECLARE_CLASS( CNPC_ControllerHeadBall, CAI_BaseNPC );

	DECLARE_DATADESC();

	void Spawn( void );
	void Precache( void );

	void EXPORT HuntThink( void );
	void EXPORT KillThink( void );
	void EXPORT BounceTouch( CBaseEntity *pOther );
	void MovetoTarget( Vector vecTarget );

	float m_flSpawnTime;
	Vector m_vecIdeal;
	EHANDLE m_hOwner;

	CSprite *m_pSprite;
};

class CNPC_ControllerZapBall : public CAI_BaseNPC
{	
public:	
	DECLARE_CLASS( CNPC_ControllerHeadBall, CAI_BaseNPC );

	DECLARE_DATADESC();

	void Spawn( void );
	void Precache( void );

	void EXPORT AnimateThink( void );
	void EXPORT ExplodeTouch( CBaseEntity *pOther );

	void Kill( void );

	EHANDLE m_hOwner;
	float m_flSpawnTime;

	CSprite *m_pSprite;
};

LINK_ENTITY_TO_CLASS( monster_alien_controller, CNPC_Controller );

BEGIN_DATADESC( CNPC_Controller )

	DEFINE_ARRAY( m_pBall, FIELD_CLASSPTR, 2 ),
	DEFINE_ARRAY( m_iBall, FIELD_INTEGER, 2 ),
	DEFINE_ARRAY( m_iBallTime, FIELD_TIME, 2 ),
	DEFINE_ARRAY( m_iBallCurrent, FIELD_INTEGER, 2 ),
	DEFINE_FIELD( m_vecEstVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_velocity, FIELD_VECTOR ),
	DEFINE_FIELD( m_fInCombat, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_flShootTime, FIELD_TIME ),
	DEFINE_FIELD( m_flShootEnd, FIELD_TIME ),

END_DATADESC()


void CNPC_Controller::Spawn()
{
	Precache( );

	SetModel( "models/controller.mdl" );
	UTIL_SetSize( this, Vector( -32, -32, 0 ), Vector( 32, 32, 64 ));

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );

	SetMoveType( MOVETYPE_STEP );
	SetGravity(0.001);


	m_bloodColor		= BLOOD_COLOR_GREEN;
	m_iHealth =			sk_controller_health.GetFloat();

	m_flFieldOfView		= VIEW_FIELD_FULL;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_NPCState			= NPC_STATE_NONE;

	SetRenderColor( 255, 255, 255, 255 );

	CapabilitiesClear();

	AddFlag( FL_FLY );
	SetNavType( NAV_FLY );

	CapabilitiesAdd( bits_CAP_MOVE_FLY | bits_CAP_INNATE_RANGE_ATTACK1 | bits_CAP_INNATE_RANGE_ATTACK2 | bits_CAP_MOVE_SHOOT);

	NPCInit();

	
	SetDefaultEyeOffset();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CNPC_Controller::Precache()
{
	PrecacheModel("models/controller.mdl");

	PrecacheModel( "sprites/xspark4.vmt");

	UTIL_PrecacheOther( "controller_energy_ball" );	
	UTIL_PrecacheOther( "controller_head_ball" );

	PrecacheScriptSound( "Controller.Pain" );
	PrecacheScriptSound( "Controller.Alert" );
	PrecacheScriptSound( "Controller.Die" );
	PrecacheScriptSound( "Controller.Idle" );
	PrecacheScriptSound( "Controller.Attack" );

}	

//=========================================================
// TakeDamage - 
//=========================================================
int CNPC_Controller::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	PainSound( info );
	return BaseClass::OnTakeDamage_Alive( info );
}

bool CNPC_Controller::ShouldGib( const CTakeDamageInfo &info )
{
	if ( info.GetDamageType() & DMG_NEVERGIB )
		 return false;

	if ( ( g_pGameRules->Damage_ShouldGibCorpse( info.GetDamageType() ) && m_iHealth < GIB_HEALTH_VALUE ) || ( info.GetDamageType() & DMG_ALWAYSGIB ) )
		 return true;
	
	return false;
	
}

int CNPC_Controller::IRelationPriority( CBaseEntity *pTarget )
{
	if ( pTarget->Classify() == CLASS_PLAYER )
	{
		 return BaseClass::IRelationPriority ( pTarget ) + 1;
	}

	return BaseClass::IRelationPriority( pTarget );
}

void CNPC_Controller::Event_Killed( const CTakeDamageInfo &info )
{
	if( ShouldGib(info) )
	{
		//remove the balls 
		if (m_pBall[0])
		{
			UTIL_Remove( m_pBall[0] );
			m_pBall[0] = NULL;
		}
		if (m_pBall[1])
		{
			UTIL_Remove( m_pBall[1] );
			m_pBall[1] = NULL;
		}
	}
	else
	{
		// fade balls
		if (m_pBall[0])
		{
			m_pBall[0]->FadeAndDie( 2 );
			m_pBall[0] = NULL;
		}
		if (m_pBall[1])
		{
			m_pBall[1]->FadeAndDie( 2 );
			m_pBall[1] = NULL;
		}
	}

	BaseClass::Event_Killed( info );
}

void CNPC_Controller::PainSound( const CTakeDamageInfo &info )
{
	if (random->RandomInt(0,5) < 2)
	{
		CPASAttenuationFilter filter( this );
		EmitSound( filter, entindex(), "Controller.Pain" );
	}
}

void CNPC_Controller::AlertSound( void )
{
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Controller.Alert" ); 
}

void CNPC_Controller::IdleSound( void )
{
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Controller.Idle" );
}

void CNPC_Controller::AttackSound( void )
{
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Controller.Attack" );
}

void CNPC_Controller::DeathSound( const CTakeDamageInfo &info )
{
	CPASAttenuationFilter filter( this );
	EmitSound( filter, entindex(), "Controller.Die" );
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CNPC_Controller::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
		case CONTROLLER_AE_HEAD_OPEN:
		{
			Vector vecStart;
			QAngle angleGun;
			
			GetAttachment( 0, vecStart, angleGun );

			// BUGBUG - attach to attachment point!

			CBroadcastRecipientFilter filter;
			te->DynamicLight( filter, 0.0, &vecStart, 255, 192, 64, 0, 1 /*radius*/, 0.2, -32 );
		
			m_iBall[0] = 192;
			m_iBallTime[0] = gpGlobals->curtime + atoi( pEvent->options ) / 15.0;
			m_iBall[1] = 255;
			m_iBallTime[1] = gpGlobals->curtime + atoi( pEvent->options ) / 15.0;

		}
		break;

		case CONTROLLER_AE_BALL_SHOOT:
		{
			Vector vecStart;
			QAngle angleGun;
			
			GetAttachment( 1, vecStart, angleGun );

			CBroadcastRecipientFilter filter;
			te->DynamicLight( filter, 0.0, &vecStart, 255, 192, 64, 0, 1 /*radius*/, 0.1, 32 );

			CAI_BaseNPC *pBall = (CAI_BaseNPC*)Create( "controller_head_ball", vecStart, angleGun );

			pBall->SetAbsVelocity( Vector(0,0,32) );
			pBall->SetEnemy( GetEnemy() );

//			DevMsg( 1, "controller shooting head ball\n" );

			m_iBall[0] = 0;
			m_iBall[1] = 0;
		}
		break;

		case CONTROLLER_AE_SMALL_SHOOT:
		{
			AttackSound( );
			m_flShootTime = gpGlobals->curtime;
			m_flShootEnd = m_flShootTime + atoi( pEvent->options ) / 15.0;
		}
		break;
		case CONTROLLER_AE_POWERUP_FULL:
		{
			m_iBall[0] = 255;
			m_iBallTime[0] = gpGlobals->curtime + atoi( pEvent->options ) / 15.0;
			m_iBall[1] = 255;
			m_iBallTime[1] = gpGlobals->curtime + atoi( pEvent->options ) / 15.0;
		}
		break;
		case CONTROLLER_AE_POWERUP_HALF:
		{
			m_iBall[0] = 192;
			m_iBallTime[0] = gpGlobals->curtime + atoi( pEvent->options ) / 15.0;
			m_iBall[1] = 192;
			m_iBallTime[1] = gpGlobals->curtime + atoi( pEvent->options ) / 15.0;
		}
		break;
		default:
			BaseClass::HandleAnimEvent( pEvent );
			break;
	}
}


//=========================================================
// AI Schedules Specific to this monster
//=========================================================

AI_BEGIN_CUSTOM_NPC( monster_alien_controller, CNPC_Controller )

	//declare our tasks
	DECLARE_TASK( TASK_CONTROLLER_CHASE_ENEMY )
	DECLARE_TASK( TASK_CONTROLLER_STRAFE )
	DECLARE_TASK( TASK_CONTROLLER_TAKECOVER )
	DECLARE_TASK( TASK_CONTROLLER_FAIL )

	DECLARE_ACTIVITY( ACT_CONTROLLER_UP )
	DECLARE_ACTIVITY( ACT_CONTROLLER_DOWN )
	DECLARE_ACTIVITY( ACT_CONTROLLER_LEFT )
	DECLARE_ACTIVITY( ACT_CONTROLLER_RIGHT )
	DECLARE_ACTIVITY( ACT_CONTROLLER_FORWARD )
	DECLARE_ACTIVITY( ACT_CONTROLLER_BACKWARD )

	//=========================================================
	// > SCHED_CONTROLLER_CHASE_ENEMY
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CONTROLLER_CHASE_ENEMY,

		"	Tasks"
		"		TASK_GET_PATH_TO_ENEMY			128"
		"		TASK_WAIT_FOR_MOVEMENT				0"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_TASK_FAILED"


	)

	//=========================================================
	// > SCHED_CONTROLLER_STRAFE
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CONTROLLER_STRAFE,

		"	Tasks"
		"		TASK_WAIT						0.2"
		"		TASK_GET_PATH_TO_ENEMY			128"
		"		TASK_WAIT_FOR_MOVEMENT			  0"
		"		TASK_WAIT						  1"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
	)

	//=========================================================
	// > SCHED_CONTROLLER_TAKECOVER
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CONTROLLER_TAKECOVER,

		"	Tasks"
		"		TASK_WAIT						0.2"
		"		TASK_FIND_COVER_FROM_ENEMY		  0"
		"		TASK_WAIT_FOR_MOVEMENT			  0"
		"		TASK_WAIT						  1"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
	)

	//=========================================================
	// > SCHED_CONTROLLER_FAIL
	//=========================================================
	DEFINE_SCHEDULE
	(
		SCHED_CONTROLLER_FAIL,

		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_SET_ACTIVITY					ACTIVITY:ACT_IDLE"
		"		TASK_WAIT							2"
		"		TASK_WAIT_PVS						0"
	)

AI_END_CUSTOM_NPC()

//=========================================================
// StartTask
//=========================================================
void CNPC_Controller::StartTask( const Task_t *pTask )
{
	BaseClass::StartTask( pTask );
}

Vector Intersect( Vector vecSrc, Vector vecDst, Vector vecMove, float flSpeed )
{
	Vector vecTo = vecDst - vecSrc;

	float a = DotProduct( vecMove, vecMove ) - flSpeed * flSpeed;
	float b = 0 * DotProduct(vecTo, vecMove); // why does this work?
	float c = DotProduct( vecTo, vecTo );

	float t;
	if (a == 0)
	{
		t = c / (flSpeed * flSpeed);
	}
	else
	{
		t = b * b - 4 * a * c;
		t = sqrt( t ) / (2.0 * a);
		float t1 = -b +t;
		float t2 = -b -t;

		if (t1 < 0 || t2 < t1)
			t = t2;
		else
			t = t1;
	}

	if (t < 0.1)
		t = 0.1;
	if (t > 10.0)
		t = 10.0;

	Vector vecHit = vecTo + vecMove * t;
	VectorNormalize( vecHit );
	return vecHit * flSpeed;
}


int CNPC_Controller::LookupFloat( )
{
	if (m_velocity.Length( ) < 32.0)
	{
		return ACT_CONTROLLER_UP;
	}

	Vector vecForward, vecRight, vecUp;
	AngleVectors( GetAbsAngles(), &vecForward, &vecRight, &vecUp );

	float x = DotProduct( vecForward, m_velocity );
	float y = DotProduct( vecRight, m_velocity );
	float z = DotProduct( vecUp, m_velocity );

	if (fabs(x) > fabs(y) && fabs(x) > fabs(z))
	{
		if (x > 0)
			return ACT_CONTROLLER_FORWARD;
		else
			return ACT_CONTROLLER_BACKWARD;
	}
	else if (fabs(y) > fabs(z))
	{
		if (y > 0)
			return ACT_CONTROLLER_RIGHT;
		else
			return ACT_CONTROLLER_LEFT;
	}
	else
	{
		if (z > 0)
			return ACT_CONTROLLER_UP;
		else
			return ACT_CONTROLLER_DOWN;
	}
}


//=========================================================
// RunTask 
//=========================================================
void CNPC_Controller::RunTask ( const Task_t *pTask )
{
	if (m_flShootEnd > gpGlobals->curtime)
	{
		Vector vecHand;
		QAngle vecAngle;
		
		GetAttachment( 2, vecHand, vecAngle );
	
		while (m_flShootTime < m_flShootEnd && m_flShootTime < gpGlobals->curtime)
		{
			Vector vecSrc = vecHand + GetAbsVelocity() * (m_flShootTime - gpGlobals->curtime);
			Vector vecDir;
			
			if (GetEnemy() != NULL)
			{
				if (HasCondition( COND_SEE_ENEMY ))
				{
					m_vecEstVelocity = m_vecEstVelocity * 0.5 + GetEnemy()->GetAbsVelocity() * 0.5;
				}
				else
				{
					m_vecEstVelocity = m_vecEstVelocity * 0.8;
				}
				vecDir = Intersect( vecSrc, GetEnemy()->BodyTarget( GetAbsOrigin() ), m_vecEstVelocity, sk_controller_speedball.GetFloat() );
			
				float delta = 0.03490; // +-2 degree
				vecDir = vecDir + Vector( random->RandomFloat( -delta, delta ), random->RandomFloat( -delta, delta ), random->RandomFloat( -delta, delta ) ) * sk_controller_speedball.GetFloat();

				vecSrc = vecSrc + vecDir * (gpGlobals->curtime - m_flShootTime);
				CAI_BaseNPC *pBall = (CAI_BaseNPC*)Create( "controller_energy_ball", vecSrc, GetAbsAngles(), this );
				pBall->SetAbsVelocity( vecDir );

//				DevMsg( 2, "controller shooting energy ball\n" );
			}
			
			m_flShootTime += 0.2;
		}

		if (m_flShootTime > m_flShootEnd)
		{
			m_iBall[0] = 64;
			m_iBallTime[0] = m_flShootEnd;
			m_iBall[1] = 64;
			m_iBallTime[1] = m_flShootEnd;
			m_fInCombat = FALSE;
		}
	}

	switch ( pTask->iTask )
	{
	case TASK_WAIT_FOR_MOVEMENT:
	case TASK_WAIT:
	case TASK_WAIT_FACE_ENEMY:
	case TASK_WAIT_PVS:
		{
			if( GetEnemy() )
			{
				float idealYaw = UTIL_VecToYaw( GetEnemy()->GetAbsOrigin() - GetAbsOrigin() );
				GetMotor()->SetIdealYawAndUpdate( idealYaw );
			}

			if ( IsSequenceFinished() || GetActivity() == ACT_IDLE)
			{
				m_fInCombat = false;
			}

			BaseClass::RunTask ( pTask );

			if (!m_fInCombat)
			{
				if( HasCondition( COND_CAN_RANGE_ATTACK1 ))
				{
					SetActivity( ACT_RANGE_ATTACK1 );
					SetCycle( 0 ); 
					ResetSequenceInfo( );
					m_fInCombat = true;
				}
				else if( HasCondition( COND_CAN_RANGE_ATTACK2 ) )
				{
					SetActivity( ACT_RANGE_ATTACK2 );
					SetCycle( 0 );
					ResetSequenceInfo( );
					m_fInCombat = true;
				}
				else
				{
					int iFloatActivity = LookupFloat();
					if( IsSequenceFinished() || iFloatActivity != GetActivity() )
					{
						SetActivity( (Activity)iFloatActivity );
					}
				}
			}
		}
		break;
	default: 
		BaseClass::RunTask ( pTask );
		break;
	}
}

void CNPC_Controller::SetSequence( int nSequence )
{
	BaseClass::SetSequence( nSequence );
}


//=========================================================
//=========================================================
int CNPC_Controller::TranslateSchedule( int scheduleType )
{
	switch	( scheduleType )
	{
	case SCHED_CHASE_ENEMY:
		return SCHED_CONTROLLER_CHASE_ENEMY;
	case SCHED_RANGE_ATTACK1:
		return SCHED_CONTROLLER_STRAFE;
	case SCHED_RANGE_ATTACK2:
	case SCHED_MELEE_ATTACK1:
	case SCHED_MELEE_ATTACK2:
	case SCHED_TAKE_COVER_FROM_ENEMY:
		return SCHED_CONTROLLER_TAKECOVER;
	case SCHED_FAIL:
		return SCHED_CONTROLLER_FAIL;

	default:
		break;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}


//=========================================================
// CheckRangeAttack1  - shoot a bigass energy ball out of their head
//=========================================================
int CNPC_Controller::RangeAttack1Conditions ( float flDot, float flDist )
{
	if( flDist > 2048 )
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	if( flDist <= 256 )
	{
		return COND_TOO_CLOSE_TO_ATTACK;
	}

//	if( flDot <= 0.5 )
//	{
//		return COND_NOT_FACING_ATTACK;
//	}

	return COND_CAN_RANGE_ATTACK1;
}

//=========================================================
// CheckRangeAttack1  - head
//=========================================================
int CNPC_Controller::RangeAttack2Conditions ( float flDot, float flDist )
{
		if( flDist > 2048 )
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	if( flDist <= 64 )
	{
		return COND_TOO_CLOSE_TO_ATTACK;
	}

//	if( flDot <= 0.5 )
//	{
//		return COND_NOT_FACING_ATTACK;
//	}

	return COND_CAN_RANGE_ATTACK2;
}

//=========================================================
//=========================================================
Activity CNPC_Controller::NPC_TranslateActivity( Activity eNewActivity )
{
	switch ( eNewActivity)
	{
	case ACT_IDLE:
		return (Activity)LookupFloat();
		break;

	default:
		return BaseClass::NPC_TranslateActivity( eNewActivity );
	}
}

//=========================================================
// SetActivity  - 
//=========================================================
void CNPC_Controller::SetActivity ( Activity NewActivity )
{
	BaseClass::SetActivity( NewActivity );
	m_flGroundSpeed = 100;
}

//=========================================================
// RunAI
//=========================================================
void CNPC_Controller::RunAI( void )
{
	BaseClass::RunAI();

	Vector vecStart;
	QAngle angleGun;

	//some kind of hack in hl1 ?
//	if ( HasMemory( bits_MEMORY_KILLED ) )
	//use this instead
	if( !IsAlive() )
		return;

	for (int i = 0; i < 2; i++)
	{
		if (m_pBall[i] == NULL)
		{
			m_pBall[i] = CSprite::SpriteCreate( "sprites/xspark4.vmt", GetAbsOrigin(), TRUE );
			m_pBall[i]->SetTransparency( kRenderGlow, 255, 255, 255, 255, kRenderFxNoDissipation );
			m_pBall[i]->SetAttachment( this, (i + 3) );
			m_pBall[i]->SetScale( 1.0 );
		}

		float t = m_iBallTime[i] - gpGlobals->curtime;
		if (t > 0.1)
			t = 0.1 / t;
		else
			t = 1.0;

		m_iBallCurrent[i] += (m_iBall[i] - m_iBallCurrent[i]) * t;

		m_pBall[i]->SetBrightness( m_iBallCurrent[i] );

		GetAttachment( i + 2, vecStart, angleGun );
		m_pBall[i]->SetAbsOrigin( vecStart );

		CBroadcastRecipientFilter filter;
		GetAttachment( i + 3, vecStart, angleGun );
		te->DynamicLight( filter, 0.0, &vecStart, 255, 192, 64, 0/*exponent*/, m_iBallCurrent[i] / 8 /*radius*/, 0.5, 0 );
	}
}

//=========================================================
// Stop  - 
//=========================================================
void CNPC_Controller::Stop( void ) 
{ 
	SetIdealActivity( GetStoppedActivity() );
}

//-----------------------------------------------------------------------------
// Purpose: Handles movement towards the last move target.
// Input  : flInterval - 
//-----------------------------------------------------------------------------
bool CNPC_Controller::OverridePathMove( float flInterval )
{
	CBaseEntity *pMoveTarget = (GetTarget()) ? GetTarget() : GetEnemy();
	Vector waypointDir = GetNavigator()->GetCurWaypointPos() - GetLocalOrigin();

	float flWaypointDist = waypointDir.Length2D();
	VectorNormalize(waypointDir);

	// cut corner?
	if (flWaypointDist < 128)
	{
		if (m_flGroundSpeed > 100)
			m_flGroundSpeed -= 40;
	}
	else
	{
		if (m_flGroundSpeed < 400)
			m_flGroundSpeed += 10;
	}

	m_velocity = m_velocity * 0.8 + m_flGroundSpeed * waypointDir * 0.5;
	SetAbsVelocity( m_velocity );

	// -----------------------------------------------------------------
	// Check route is blocked
	// ------------------------------------------------------------------
	Vector checkPos = GetLocalOrigin() + (waypointDir * (m_flGroundSpeed * flInterval));

	AIMoveTrace_t moveTrace;
	GetMoveProbe()->MoveLimit( NAV_FLY, GetLocalOrigin(), checkPos, MASK_NPCSOLID|CONTENTS_WATER,
		pMoveTarget, &moveTrace);
	if (IsMoveBlocked( moveTrace ))
	{
		TaskFail(FAIL_NO_ROUTE);
		GetNavigator()->ClearGoal();
		return true;
	}

	// ----------------------------------------------
	
	Vector lastPatrolDir = GetNavigator()->GetCurWaypointPos() - GetLocalOrigin();
	
	if ( ProgressFlyPath( flInterval, pMoveTarget, MASK_NPCSOLID, false, 64 ) == AINPP_COMPLETE )
	{
		{
			m_vLastPatrolDir = lastPatrolDir;
			VectorNormalize(m_vLastPatrolDir);
		}
		return true;
	}
	return false;
}

bool CNPC_Controller::OverrideMove( float flInterval )
{
	if (m_flGroundSpeed == 0)
	{
		m_flGroundSpeed = 100;
	}

	// ----------------------------------------------
	//	Select move target 
	// ----------------------------------------------
	CBaseEntity *pMoveTarget = NULL;
	if (GetTarget() != NULL )
	{
		pMoveTarget = GetTarget();
	}
	else if (GetEnemy() != NULL )
	{
		pMoveTarget		= GetEnemy();
	}

	// ----------------------------------------------
	//	Select move target position 
	// ----------------------------------------------
	Vector vMoveTargetPos(0,0,0);
	if (GetTarget())
	{
		vMoveTargetPos = GetTarget()->GetAbsOrigin();
	}
	else if (GetEnemy() != NULL)
	{
		vMoveTargetPos = GetEnemy()->GetAbsOrigin();
	}

	// -----------------------------------------
	//  See if we can fly there directly
	// -----------------------------------------
	if (pMoveTarget /*|| HaveInspectTarget()*/)
	{
		trace_t tr;

		if (pMoveTarget)
		{
			UTIL_TraceEntity( this, GetAbsOrigin(), vMoveTargetPos, 
				MASK_NPCSOLID_BRUSHONLY, pMoveTarget, GetCollisionGroup(), &tr);
		}
		else
		{
			UTIL_TraceEntity( this, GetAbsOrigin(), vMoveTargetPos, MASK_NPCSOLID_BRUSHONLY, &tr);
		}
/*
		float fTargetDist = (1-tr.fraction)*(GetAbsOrigin() - vMoveTargetPos).Length();
		if (fTargetDist > 50)
		{
			//SetCondition( COND_SCANNER_FLY_BLOCKED );
		}
		else
		{
			//SetCondition( COND_SCANNER_FLY_CLEAR );
		}
*/
	}

	// -----------------------------------------------------------------
	// If I have a route, keep it updated and move toward target
	// ------------------------------------------------------------------
	if (GetNavigator()->IsGoalActive())
	{
		if ( OverridePathMove( flInterval ) )
			return true;
	}
	else
	{
		//do nothing
		Stop();
		TaskComplete();
	}

	return true;
}

void CNPC_Controller::MoveToTarget( float flInterval, const Vector &vecMoveTarget )
{
	const float	myAccel	 = 300.0;
	const float	myDecay	 = 9.0;
	
	//TurnHeadToTarget( flInterval, MoveTarget );
	MoveToLocation( flInterval, vecMoveTarget, myAccel, (2 * myAccel), myDecay );
}

//=========================================================
// Controller bouncy ball attack
//=========================================================

LINK_ENTITY_TO_CLASS( controller_head_ball, CNPC_ControllerHeadBall );

BEGIN_DATADESC( CNPC_ControllerHeadBall )

	DEFINE_THINKFUNC( HuntThink ),
	DEFINE_THINKFUNC( KillThink ),
	DEFINE_ENTITYFUNC( BounceTouch ),

	DEFINE_FIELD( m_pSprite, FIELD_CLASSPTR ),

	DEFINE_FIELD( m_flSpawnTime, FIELD_TIME ),
	DEFINE_FIELD( m_vecIdeal, FIELD_VECTOR ),
	DEFINE_FIELD( m_hOwner, FIELD_EHANDLE ),

END_DATADESC()


void CNPC_ControllerHeadBall::Spawn( void )
{
	Precache( );
	// motor
	SetMoveType( MOVETYPE_FLY );
	SetSolid( SOLID_BBOX );
	SetSize( vec3_origin, vec3_origin );

	m_pSprite = CSprite::SpriteCreate( "sprites/xspark4.vmt", GetAbsOrigin(), FALSE );
	m_pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
	m_pSprite->SetAttachment( this, 0 );
	m_pSprite->SetScale( 2.0 );

	UTIL_SetSize( this, Vector( 0, 0, 0), Vector(0, 0, 0) );
	UTIL_SetOrigin( this, GetAbsOrigin() );

	SetThink( &CNPC_ControllerHeadBall::HuntThink );
	SetTouch( &CNPC_ControllerHeadBall::BounceTouch );

//	m_vecIdeal = vec3_origin;	//(0,0,0)

	SetNextThink( gpGlobals->curtime + 0.1 );

	m_hOwner = GetOwnerEntity();

	m_flSpawnTime = gpGlobals->curtime;
}


void CNPC_ControllerHeadBall::Precache( void )
{
	PrecacheModel( "sprites/xspark4.vmt");
}

extern short		g_sModelIndexLaser;	

void CNPC_ControllerHeadBall::HuntThink( void  )
{
	SetNextThink( gpGlobals->curtime + 0.1 );

	if( !m_pSprite )
	{
		Assert(0);
		return;
	}

	m_pSprite->SetBrightness( m_pSprite->GetBrightness() - 5, 0.1f );

	CBroadcastRecipientFilter filter;
	te->DynamicLight( filter, 0.0, &GetAbsOrigin(), 255, 255, 255, 0, m_pSprite->GetBrightness() / 16, 0.2, 0 );

	// check world boundaries
	if (gpGlobals->curtime - m_flSpawnTime > 5 || m_pSprite->GetBrightness() < 64 /*|| GetEnemy() == NULL || m_hOwner == NULL*/ || !IsInWorld() )
	{
		SetTouch( NULL );
		SetThink( &CNPC_ControllerHeadBall::KillThink );
		SetNextThink( gpGlobals->curtime );
		return;
	}

	if( !GetEnemy() )
		return;

	MovetoTarget( GetEnemy()->GetAbsOrigin() );

	if ((GetEnemy()->WorldSpaceCenter() - GetAbsOrigin()).Length() < 64)
	{
		trace_t tr;

		UTIL_TraceLine( GetAbsOrigin(), GetEnemy()->WorldSpaceCenter(), MASK_ALL, this, COLLISION_GROUP_NONE, &tr );

		CBaseEntity *pEntity = tr.m_pEnt;
		if (pEntity != NULL && pEntity->m_takedamage == DAMAGE_YES)
		{
			ClearMultiDamage( );
			Vector dir = GetAbsVelocity();
			VectorNormalize( dir );
			CTakeDamageInfo info( this, this, sk_controller_dmgball.GetFloat(), DMG_SHOCK );
			CalculateMeleeDamageForce( &info, dir, tr.endpos );
			pEntity->DispatchTraceAttack( info, dir, &tr );
			ApplyMultiDamage();

			int haloindex = 0;
			int fadelength = 0;
			int amplitude = 0;
			const Vector vecEnd = tr.endpos;
			te->BeamEntPoint( filter, 0.0, entindex(), NULL, 0, &(tr.m_pEnt->GetAbsOrigin()), 
				g_sModelIndexLaser, haloindex /* no halo */, 0, 10, 3, 20, 20, fadelength, 
				amplitude, 255, 255, 255, 255, 10 );

		}

		UTIL_EmitAmbientSound( GetSoundSourceIndex(), GetAbsOrigin(), "Controller.ElectroSound", 0.5, SNDLVL_NORM, 0, 100 );

		SetNextAttack( gpGlobals->curtime + 3.0 );

		SetThink( &CNPC_ControllerHeadBall::KillThink );
		SetNextThink( gpGlobals->curtime + 0.3 );
	}
}

void CNPC_ControllerHeadBall::MovetoTarget( Vector vecTarget )
{
	// accelerate
	float flSpeed = m_vecIdeal.Length();
	if (flSpeed == 0)
	{
		m_vecIdeal = GetAbsVelocity();
		flSpeed = m_vecIdeal.Length();
	}

	if (flSpeed > 400)
	{
		VectorNormalize( m_vecIdeal );
		m_vecIdeal = m_vecIdeal * 400;
	}

	Vector t = vecTarget - GetAbsOrigin();
	VectorNormalize(t);
	m_vecIdeal = m_vecIdeal + t * 100;
	SetAbsVelocity(m_vecIdeal);
}

void CNPC_ControllerHeadBall::BounceTouch( CBaseEntity *pOther )
{
	Vector vecDir = m_vecIdeal;
	VectorNormalize( vecDir );

	trace_t tr;
	tr = CBaseEntity::GetTouchTrace( );

	float n = -DotProduct(tr.plane.normal, vecDir);

	vecDir = 2.0 * tr.plane.normal * n + vecDir;

	m_vecIdeal = vecDir * m_vecIdeal.Length();
}

void CNPC_ControllerHeadBall::KillThink( void )
{
	UTIL_Remove( m_pSprite );
	UTIL_Remove( this );
}


//=========================================================
// Controller Zap attack
//=========================================================

LINK_ENTITY_TO_CLASS( controller_energy_ball, CNPC_ControllerZapBall );

BEGIN_DATADESC( CNPC_ControllerZapBall )

	DEFINE_THINKFUNC( AnimateThink ),
	DEFINE_ENTITYFUNC( ExplodeTouch ),

	DEFINE_FIELD( m_hOwner, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flSpawnTime, FIELD_TIME ),
	DEFINE_FIELD( m_pSprite, FIELD_CLASSPTR ),

END_DATADESC()


void CNPC_ControllerZapBall::Spawn( void )
{
	Precache( );
	// motor
	SetMoveType( MOVETYPE_FLY );
//	SetSolid( SOLID_CUSTOM );
	SetSolid( SOLID_BBOX );
	SetSize( vec3_origin, vec3_origin );

	m_pSprite = CSprite::SpriteCreate( "sprites/xspark4.vmt", GetAbsOrigin(), FALSE );
	m_pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 255, kRenderFxNoDissipation );
	m_pSprite->SetAttachment( this, 0 );
	m_pSprite->SetScale( 0.5 );

	UTIL_SetSize( this, Vector( 0, 0, 0), Vector(0, 0, 0) );
	UTIL_SetOrigin( this, GetAbsOrigin() );

	SetThink( &CNPC_ControllerZapBall::AnimateThink );
	SetTouch( &CNPC_ControllerZapBall::ExplodeTouch );

	m_hOwner = GetOwnerEntity();

	m_flSpawnTime = gpGlobals->curtime; // keep track of when ball spawned
	SetNextThink( gpGlobals->curtime + 0.1 );
}


void CNPC_ControllerZapBall::Precache( void )
{
	PrecacheModel( "sprites/xspark4.vmt");
}


void CNPC_ControllerZapBall::AnimateThink( void  )
{
	SetNextThink( gpGlobals->curtime + 0.1 );
	
	SetCycle( ((int)GetCycle() + 1) % 11 );

	if (gpGlobals->curtime - m_flSpawnTime > 5 || GetAbsVelocity().Length() < 10)
	{
		SetTouch( NULL );
		Kill();
	}
}


void CNPC_ControllerZapBall::ExplodeTouch( CBaseEntity *pOther )
{
	if (m_takedamage = DAMAGE_YES )
	{
		trace_t tr;
		tr = GetTouchTrace( );

		ClearMultiDamage( );

		Vector vecAttackDir = GetAbsVelocity();
		VectorNormalize( vecAttackDir );

		if (m_hOwner != NULL)
		{
			CTakeDamageInfo info( this, m_hOwner, sk_controller_dmgball.GetFloat(), DMG_ENERGYBEAM );
			CalculateMeleeDamageForce( &info, vecAttackDir, tr.endpos );
			pOther->DispatchTraceAttack( info, vecAttackDir, &tr );
		}
		else
		{
			CTakeDamageInfo info( this, this, sk_controller_dmgball.GetFloat(), DMG_ENERGYBEAM );
			CalculateMeleeDamageForce( &info, vecAttackDir, tr.endpos );
			pOther->DispatchTraceAttack( info, vecAttackDir, &tr );
		}

		ApplyMultiDamage();

	//	void UTIL_EmitAmbientSound( CBaseEntity *entity, const Vector &vecOrigin, const char *samp, float vol, soundlevel_t soundlevel, int fFlags, int pitch, float soundtime /*= 0.0f*/ )

		UTIL_EmitAmbientSound( GetSoundSourceIndex(), tr.endpos, "Controller.ElectroSound", 0.3, SNDLVL_NORM, 0, random->RandomInt( 90, 99 ) );
	}

	Kill();
}

void CNPC_ControllerZapBall::Kill( void )
{
	UTIL_Remove( m_pSprite );
	UTIL_Remove( this );
}
