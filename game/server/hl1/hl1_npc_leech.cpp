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
#include	"hl1_ai_basenpc.h"
#include	"ai_senses.h"

// Animation events
#define LEECH_AE_ATTACK		1
#define LEECH_AE_FLOP		2

//#define DEBUG_BEAMS 0

ConVar sk_leech_health( "sk_leech_health", "2" );
ConVar sk_leech_dmg_bite( "sk_leech_dmg_bite", "2" );

// Movement constants

#define		LEECH_ACCELERATE		10
#define		LEECH_CHECK_DIST		45
#define		LEECH_SWIM_SPEED		50
#define		LEECH_SWIM_ACCEL		80
#define		LEECH_SWIM_DECEL		10
#define		LEECH_TURN_RATE			70
#define		LEECH_SIZEX				10
#define		LEECH_FRAMETIME			0.1

class CNPC_Leech : public CHL1BaseNPC
{
	DECLARE_CLASS( CNPC_Leech, CHL1BaseNPC );
public:

	DECLARE_DATADESC();
	
	void Spawn( void );
	void Precache( void );

	static const char *pAlertSounds[];

	void SwimThink( void );
	void DeadThink( void );

	void SwitchLeechState( void );
	float ObstacleDistance( CBaseEntity *pTarget );
	void UpdateMotion( void );

	void RecalculateWaterlevel( void );
	void Touch( CBaseEntity *pOther );

	Disposition_t IRelationType(CBaseEntity *pTarget);

	void HandleAnimEvent( animevent_t *pEvent );

	void AttackSound( void );
	void AlertSound( void );

	void Activate( void );

	Class_T	Classify( void ) { return CLASS_INSECT; };

	void Event_Killed( const CTakeDamageInfo &info );


	bool ShouldGib(	const CTakeDamageInfo &info );

	
/*	// Base entity functions
	void Killed( entvars_t *pevAttacker, int iGib );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
*/

private:
	// UNDONE: Remove unused boid vars, do group behavior
	float	m_flTurning;// is this boid turning?
	bool	m_fPathBlocked;// TRUE if there is an obstacle ahead
	float	m_flAccelerate;
	float	m_obstacle;
	float	m_top;
	float	m_bottom;
	float	m_height;
	float	m_waterTime;
	float	m_sideTime;		// Timer to randomly check clearance on sides
	float	m_zTime;
	float	m_stateTime;
	float	m_attackSoundTime;
	Vector  m_oldOrigin;
};

LINK_ENTITY_TO_CLASS( monster_leech, CNPC_Leech );

BEGIN_DATADESC( CNPC_Leech )
	DEFINE_FIELD( m_flTurning, FIELD_FLOAT ),
	DEFINE_FIELD( m_fPathBlocked, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flAccelerate, FIELD_FLOAT ),
	DEFINE_FIELD( m_obstacle, FIELD_FLOAT ),
	DEFINE_FIELD( m_top, FIELD_FLOAT ),
	DEFINE_FIELD( m_bottom, FIELD_FLOAT ),
	DEFINE_FIELD( m_height, FIELD_FLOAT ),
	DEFINE_FIELD( m_waterTime, FIELD_TIME ),
	DEFINE_FIELD( m_sideTime, FIELD_TIME ),
	DEFINE_FIELD( m_zTime, FIELD_TIME ),
	DEFINE_FIELD( m_stateTime, FIELD_TIME ),
	DEFINE_FIELD( m_attackSoundTime, FIELD_TIME ),
	DEFINE_FIELD( m_oldOrigin, FIELD_VECTOR ),

	DEFINE_THINKFUNC( SwimThink ),
	DEFINE_THINKFUNC( DeadThink ),
END_DATADESC()


bool CNPC_Leech::ShouldGib(	const CTakeDamageInfo &info )
{
	return false;
}

void CNPC_Leech::Spawn( void )
{
	Precache();
	SetModel( "models/leech.mdl" );

	SetHullType(HULL_TINY_CENTERED); 
	SetHullSizeNormal();

	UTIL_SetSize( this, Vector(-1,-1,0), Vector(1,1,2));

	Vector vecSurroundingMins(-8,-8,0);
	Vector vecSurroundingMaxs(8,8,2);
	CollisionProp()->SetSurroundingBoundsType( USE_SPECIFIED_BOUNDS, &vecSurroundingMins, &vecSurroundingMaxs );

	// Don't push the minz down too much or the water check will fail because this entity is really point-sized
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_FLY );
	AddFlag( FL_SWIM );
	m_iHealth	= sk_leech_health.GetInt();

	m_flFieldOfView		= -0.5;	// 180 degree FOV
	SetDistLook( 750 );
	NPCInit();
	SetThink( &CNPC_Leech::SwimThink );
	SetUse( NULL );
	SetTouch( NULL );
	SetViewOffset( vec3_origin );

	m_flTurning = 0;
	m_fPathBlocked = FALSE;
	SetActivity( ACT_SWIM );
	SetState( NPC_STATE_IDLE );
	m_stateTime = gpGlobals->curtime + random->RandomFloat( 1, 5 );

	SetRenderColor( 255, 255, 255, 255 );

	m_bloodColor		= DONT_BLEED;
	SetCollisionGroup( COLLISION_GROUP_DEBRIS );
}

void CNPC_Leech::Activate( void )
{
	RecalculateWaterlevel();

	BaseClass::Activate();
}

void CNPC_Leech::DeadThink( void )
{
	if ( IsSequenceFinished() )
	{
		if ( GetActivity() == ACT_DIEFORWARD )
		{
			SetThink( NULL );
			StopAnimation();
			return;
		}
		else if ( GetFlags() & FL_ONGROUND )
		{
			AddSolidFlags( FSOLID_NOT_SOLID );
			SetActivity( ACT_DIEFORWARD );
		}
	}
	StudioFrameAdvance();
	SetNextThink( gpGlobals->curtime + 0.1 );

	// Apply damage velocity, but keep out of the walls
	if ( GetAbsVelocity().x != 0 || GetAbsVelocity().y != 0 )
	{
		trace_t tr;

		// Look 0.5 seconds ahead
		UTIL_TraceLine( GetLocalOrigin(), GetLocalOrigin() + GetAbsVelocity() * 0.5, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
		if (tr.fraction != 1.0)
		{
			Vector vVelocity = GetAbsVelocity();

			vVelocity.x = 0;
			vVelocity.y = 0;

			SetAbsVelocity( vVelocity );
		}
	}
}


Disposition_t CNPC_Leech::IRelationType( CBaseEntity *pTarget )
{
	if ( pTarget->IsPlayer() )
		return D_HT;
	
	return BaseClass::IRelationType( pTarget );
}

void CNPC_Leech::Touch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
		 return;

	if ( pOther == GetTouchTrace().m_pEnt )
	{
		if ( pOther->GetAbsVelocity() == vec3_origin )
			 return;

		SetBaseVelocity( pOther->GetAbsVelocity() );
		AddFlag( FL_BASEVELOCITY );
	}
}

void CNPC_Leech::HandleAnimEvent( animevent_t *pEvent )
{
	CBaseEntity *pEnemy = GetEnemy();

	switch( pEvent->event )
	{
	case LEECH_AE_FLOP:
		// Play flop sound
		break;

	case LEECH_AE_ATTACK:
		AttackSound();
		
		if ( pEnemy != NULL )
		{
			Vector dir, face;
	
			AngleVectors( GetAbsAngles(), &face );
			
			face.z = 0;
			dir = (pEnemy->GetLocalOrigin() - GetLocalOrigin() );
			dir.z = 0;
			
			VectorNormalize( dir );
			VectorNormalize( face );
						
			if ( DotProduct(dir, face) > 0.9 )		// Only take damage if the leech is facing the prey
			{
				CTakeDamageInfo info( this, this, sk_leech_dmg_bite.GetInt(), DMG_SLASH );
				CalculateMeleeDamageForce( &info, dir, pEnemy->GetAbsOrigin() );
				pEnemy->TakeDamage( info );
			}
		}
		m_stateTime -= 2;
		break;
	

	
	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
}


void CNPC_Leech::Precache( void )
{
	PrecacheModel("models/leech.mdl");

	PrecacheScriptSound( "Leech.Attack" );
	PrecacheScriptSound( "Leech.Alert" );
}


void CNPC_Leech::AttackSound( void )
{
	if ( gpGlobals->curtime > m_attackSoundTime )
	{
		CPASAttenuationFilter filter( this );

		EmitSound(filter, entindex(), "Leech.Attack" );
		m_attackSoundTime = gpGlobals->curtime + 0.5;
	}
}


void CNPC_Leech::AlertSound( void )
{
	CPASAttenuationFilter filter( this );
	EmitSound(filter, entindex(), "Leech.Alert" );
}

void CNPC_Leech::SwitchLeechState( void )
{
	m_stateTime = gpGlobals->curtime + random->RandomFloat( 3, 6 );
	if ( m_NPCState == NPC_STATE_COMBAT )
	{
		SetEnemy ( NULL );
		SetState( NPC_STATE_IDLE );
		// We may be up against the player, so redo the side checks
		m_sideTime = 0;
	}
	else
	{
		GetSenses()->Look( GetSenses()->GetDistLook() );
		CBaseEntity *pEnemy = BestEnemy();
		if ( pEnemy && pEnemy->GetWaterLevel() != 0 )
		{
			SetEnemy ( pEnemy );
			SetState( NPC_STATE_COMBAT );
			m_stateTime = gpGlobals->curtime + random->RandomFloat( 18, 25 );
			AlertSound();
		}
	}
}

void CNPC_Leech::RecalculateWaterlevel( void )
{
	// Calculate boundaries
	Vector vecTest = GetLocalOrigin() - Vector(0,0,400);

	trace_t tr;

	UTIL_TraceLine( GetLocalOrigin(), vecTest, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
	
	if ( tr.fraction != 1.0 )
		m_bottom = tr.endpos.z + 1;
	else
		m_bottom = vecTest.z;

	m_top = UTIL_WaterLevel( GetLocalOrigin(), GetLocalOrigin().z, GetLocalOrigin().z + 400 ) - 1;

#if DEBUG_BEAMS
	NDebugOverlay::Line( GetLocalOrigin(), GetLocalOrigin() + Vector( 0, 0, m_bottom ), 0, 255, 0, false, 0.1f );
	NDebugOverlay::Line( GetLocalOrigin(), GetLocalOrigin() + Vector( 0, 0, m_top ), 0, 255, 255, false, 0.1f );
#endif

	// Chop off 20% of the outside range
	float newBottom = m_bottom * 0.8 + m_top * 0.2;
	m_top = m_bottom * 0.2 + m_top * 0.8;
	m_bottom = newBottom;
	m_height = random->RandomFloat( m_bottom, m_top );
	m_waterTime = gpGlobals->curtime + random->RandomFloat( 5, 7 );
}

void CNPC_Leech::SwimThink( void )
{
	trace_t			tr;
	float			flLeftSide;
	float			flRightSide;
	float			targetSpeed;
	float			targetYaw = 0;
	CBaseEntity		*pTarget;

	/*if ( !UTIL_FindClientInPVS( edict() ) )
	{
		m_flNextThink = gpGlobals->curtime + random->RandomFloat( 1.0f, 1.5f );
		SetAbsVelocity( vec3_origin );
		return;
	}
	else*/
		SetNextThink( gpGlobals->curtime + 0.1 );

	targetSpeed = LEECH_SWIM_SPEED;

	if ( m_waterTime < gpGlobals->curtime )
		RecalculateWaterlevel();

	if ( m_stateTime < gpGlobals->curtime )
		SwitchLeechState();

	ClearCondition( COND_CAN_MELEE_ATTACK1 );

	switch( m_NPCState )
	{
	case NPC_STATE_COMBAT:
		pTarget = GetEnemy();
		if ( !pTarget )
			SwitchLeechState();
		else
		{
			// Chase the enemy's eyes
			m_height = pTarget->GetLocalOrigin().z + pTarget->GetViewOffset().z - 5;
			// Clip to viable water area
			if ( m_height < m_bottom )
				m_height = m_bottom;
			else if ( m_height > m_top )
				m_height = m_top;
			Vector location = pTarget->GetLocalOrigin() - GetLocalOrigin();
			location.z += (pTarget->GetViewOffset().z);
			if ( location.Length() < 80 )
				SetCondition( COND_CAN_MELEE_ATTACK1 );
			// Turn towards target ent
			targetYaw = UTIL_VecToYaw( location );

			QAngle vTestAngle = GetAbsAngles();
			
			targetYaw = UTIL_AngleDiff( targetYaw, UTIL_AngleMod( GetAbsAngles().y ) );

			if ( targetYaw < (-LEECH_TURN_RATE) )
				targetYaw = (-LEECH_TURN_RATE);
			else if ( targetYaw > (LEECH_TURN_RATE) )
				targetYaw = (LEECH_TURN_RATE);
			else
				targetSpeed *= 2;
		}

		break;

	default:
		if ( m_zTime < gpGlobals->curtime )
		{
			float newHeight = random->RandomFloat( m_bottom, m_top );
			m_height = 0.5 * m_height + 0.5 * newHeight;
			m_zTime = gpGlobals->curtime + random->RandomFloat( 1, 4 );
		}
		if ( random->RandomInt( 0, 100 ) < 10 )
			targetYaw = random->RandomInt( -30, 30 );
		pTarget = NULL;
		// oldorigin test
		if ( ( GetLocalOrigin() - m_oldOrigin ).Length() < 1 )
		{
			// If leech didn't move, there must be something blocking it, so try to turn
			m_sideTime = 0;
		}

		break;
	}

	m_obstacle = ObstacleDistance( pTarget );
	m_oldOrigin = GetLocalOrigin();
	if ( m_obstacle < 0.1 )
		m_obstacle = 0.1;

	Vector vForward, vRight;

	AngleVectors( GetAbsAngles(), &vForward, &vRight, NULL );

	// is the way ahead clear?
	if ( m_obstacle == 1.0 )
	{
		// if the leech is turning, stop the trend.
		if ( m_flTurning != 0 )
		{
			m_flTurning = 0;
		}

		m_fPathBlocked = FALSE;
		m_flSpeed = UTIL_Approach( targetSpeed, m_flSpeed, LEECH_SWIM_ACCEL * LEECH_FRAMETIME );
		SetAbsVelocity( vForward * m_flSpeed );

	}
	else
	{
		m_obstacle = 1.0 / m_obstacle;
		// IF we get this far in the function, the leader's path is blocked!
		m_fPathBlocked = TRUE;

		if ( m_flTurning == 0 )// something in the way and leech is not already turning to avoid
		{
			Vector vecTest;
			// measure clearance on left and right to pick the best dir to turn
			vecTest = GetLocalOrigin() + ( vRight * LEECH_SIZEX) + ( vForward * LEECH_CHECK_DIST);
			UTIL_TraceLine( GetLocalOrigin(), vecTest, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
			flRightSide = tr.fraction;

			vecTest = GetLocalOrigin() + ( vRight * -LEECH_SIZEX) + ( vForward * LEECH_CHECK_DIST);
			UTIL_TraceLine( GetLocalOrigin(), vecTest, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
			
			flLeftSide = tr.fraction;

			// turn left, right or random depending on clearance ratio
			float delta = (flRightSide - flLeftSide);
			if ( delta > 0.1 || (delta > -0.1 && random->RandomInt( 0,100 ) < 50 ) )
				m_flTurning = -LEECH_TURN_RATE;
			else
				m_flTurning = LEECH_TURN_RATE;
		}

		m_flSpeed = UTIL_Approach( -(LEECH_SWIM_SPEED*0.5), m_flSpeed, LEECH_SWIM_DECEL * LEECH_FRAMETIME * m_obstacle );
		SetAbsVelocity( vForward * m_flSpeed );
	}
	
	GetMotor()->SetIdealYaw( m_flTurning + targetYaw );
	UpdateMotion();
}

//
// ObstacleDistance - returns normalized distance to obstacle
//
float CNPC_Leech::ObstacleDistance( CBaseEntity *pTarget )
{
	trace_t			tr;
	Vector			vecTest;
	Vector			vForward, vRight;

	// use VELOCITY, not angles, not all boids point the direction they are flying
	//Vector vecDir = UTIL_VecToAngles( pev->velocity );
	QAngle tmp = GetAbsAngles();
	tmp.x = -tmp.x;
	AngleVectors ( tmp, &vForward, &vRight, NULL );

	// check for obstacle ahead
	vecTest = GetLocalOrigin() + vForward * LEECH_CHECK_DIST;
	UTIL_TraceLine( GetLocalOrigin(), vecTest, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);

	if ( tr.startsolid )
	{
		m_flSpeed = -LEECH_SWIM_SPEED * 0.5;
	}

	if ( tr.fraction != 1.0 )
	{
		if ( (pTarget == NULL || tr.m_pEnt != pTarget ) )
		{
			return tr.fraction;
		}
		else
		{
			if ( fabs( m_height - GetLocalOrigin().z ) > 10 )
				return tr.fraction;
		}
	}

	if ( m_sideTime < gpGlobals->curtime )
	{
		// extra wide checks
		vecTest = GetLocalOrigin() + vRight * LEECH_SIZEX * 2 + vForward * LEECH_CHECK_DIST;
		UTIL_TraceLine( GetLocalOrigin(), vecTest, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);

		if (tr.fraction != 1.0)
			return tr.fraction;

		vecTest = GetLocalOrigin() - vRight * LEECH_SIZEX * 2 + vForward * LEECH_CHECK_DIST;
		UTIL_TraceLine( GetLocalOrigin(), vecTest, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
		if (tr.fraction != 1.0)
			return tr.fraction;

		// Didn't hit either side, so stop testing for another 0.5 - 1 seconds
		m_sideTime = gpGlobals->curtime + random->RandomFloat(0.5,1);
	}

	return 1.0;
}

void CNPC_Leech::UpdateMotion( void )
{
	float flapspeed = ( m_flSpeed - m_flAccelerate) / LEECH_ACCELERATE;
	m_flAccelerate = m_flAccelerate * 0.8 + m_flSpeed * 0.2;

	if (flapspeed < 0) 
		flapspeed = -flapspeed;
	flapspeed += 1.0;
	if (flapspeed < 0.5) 
		flapspeed = 0.5;
	if (flapspeed > 1.9) 
		flapspeed = 1.9;

	m_flPlaybackRate = flapspeed;

	QAngle vAngularVelocity = GetLocalAngularVelocity();
	QAngle vAngles = GetLocalAngles();

	if ( !m_fPathBlocked )
		vAngularVelocity.y = GetMotor()->GetIdealYaw();
	else
		vAngularVelocity.y = GetMotor()->GetIdealYaw() * m_obstacle;

	if ( vAngularVelocity.y > 150 )
		SetIdealActivity( ACT_TURN_LEFT );
	else if ( vAngularVelocity.y < -150 )
		SetIdealActivity( ACT_TURN_RIGHT );
	else
		SetIdealActivity( ACT_SWIM );

	// lean
	float targetPitch, delta;
	delta = m_height - GetLocalOrigin().z;

/*	if ( delta < -10 )
		targetPitch = -30;
	else if ( delta > 10 )
		targetPitch = 30;
	else*/
		targetPitch = 0;

	vAngles.x = UTIL_Approach( targetPitch, vAngles.x, 60 * LEECH_FRAMETIME );

	// bank
	vAngularVelocity.z = - ( vAngles.z + (vAngularVelocity.y * 0.25));

	if ( m_NPCState == NPC_STATE_COMBAT && HasCondition( COND_CAN_MELEE_ATTACK1 ) )
		SetIdealActivity( ACT_MELEE_ATTACK1 );

	// Out of water check
	if ( !GetWaterLevel() )
	{
		SetMoveType( MOVETYPE_FLYGRAVITY );
		SetIdealActivity( ACT_HOP );
		SetAbsVelocity( vec3_origin );

		// Animation will intersect the floor if either of these is non-zero
		vAngles.z = 0;
		vAngles.x = 0;

		m_flPlaybackRate = random->RandomFloat( 0.8, 1.2 );
	}
	else if ( GetMoveType() == MOVETYPE_FLYGRAVITY )
	{
		SetMoveType( MOVETYPE_FLY );
		SetGroundEntity( NULL );

	//  TODO
		RecalculateWaterlevel();
		m_waterTime = gpGlobals->curtime + 2;	// Recalc again soon, water may be rising
	}

	if ( GetActivity() != GetIdealActivity() )
	{
		SetActivity ( GetIdealActivity() );
	}
	StudioFrameAdvance();
	
	DispatchAnimEvents ( this );

	SetLocalAngles( vAngles );
	SetLocalAngularVelocity( vAngularVelocity );

	Vector vForward, vRight;

	AngleVectors( vAngles, &vForward, &vRight, NULL );

#if DEBUG_BEAMS
	if ( m_fPathBlocked )
	{
		float color = m_obstacle * 30;
		if ( m_obstacle == 1.0 )
			color = 0;
		if ( color > 255 )
			color = 255;
		 NDebugOverlay::Line( GetLocalOrigin(), GetLocalOrigin() + vForward * LEECH_CHECK_DIST, 255, color, color, false, 0.1f );
	}
	else
		 NDebugOverlay::Line( GetLocalOrigin(), GetLocalOrigin() + vForward * LEECH_CHECK_DIST, 255, 255, 0, false, 0.1f );
		 
	NDebugOverlay::Line( GetLocalOrigin(), GetLocalOrigin() + vRight * (vAngularVelocity.y*0.25), 0, 0, 255, false, 0.1f );
#endif

}

void CNPC_Leech::Event_Killed( const CTakeDamageInfo &info )
{
	Vector			vecSplatDir;
	trace_t			tr;

	//ALERT(at_aiconsole, "Leech: killed\n");
	// tell owner ( if any ) that we're dead.This is mostly for MonsterMaker functionality.
	CBaseEntity *pOwner = GetOwnerEntity();
	if (pOwner)
		pOwner->DeathNotice( this );

	// When we hit the ground, play the "death_end" activity
	if ( GetWaterLevel() )
	{
		QAngle qAngles = GetAbsAngles();
		QAngle qAngularVel = GetLocalAngularVelocity();
		Vector  vOrigin = GetLocalOrigin();

		qAngles.z = 0;
		qAngles.x = 0;

		vOrigin.z += 1;

		SetAbsVelocity( vec3_origin );

		if ( random->RandomInt( 0, 99 ) < 70 )
			 qAngularVel.y = random->RandomInt( -720, 720 );

		SetAbsAngles( qAngles );
		SetLocalAngularVelocity( qAngularVel );
		SetAbsOrigin( vOrigin );

		
		SetGravity ( 0.02 );
		SetGroundEntity( NULL );
		SetActivity( ACT_DIESIMPLE );
	}
	else
		SetActivity( ACT_DIEFORWARD );
	
	SetMoveType( MOVETYPE_FLYGRAVITY );
	m_takedamage = DAMAGE_NO;
	
	SetThink( &CNPC_Leech::DeadThink );
}
