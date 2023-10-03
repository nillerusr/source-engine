#include "cbase.h"
#include "npcevent.h"
#include "asw_simple_alien.h"
#include "asw_shareddefs.h"
#include "asw_util_shared.h"
#include "asw_fx_shared.h"
#include "ai_debug_shared.h"
#include "asw_spawner.h"
#include "asw_marine.h"
#include "asw_trace_filter_melee.h"
#include "te_effect_dispatch.h"
#include "basecombatcharacter.h"
#include "asw_gamerules.h"
#include "EntityFlame.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// TODO:
/*
[ ] add in acceleration?
[ ] make the alien walk up steps
[ ] and have gravity
[ ] orders - normal movement, pathing, etc?
*/

//LINK_ENTITY_TO_CLASS( asw_simple_alien, CASW_Simple_Alien );

BEGIN_DATADESC( CASW_Simple_Alien )
	DEFINE_THINKFUNC( AlienThink ),
	DEFINE_THINKFUNC( SleepThink ),

	DEFINE_FIELD(m_fLastThinkTime, FIELD_TIME),
	DEFINE_FIELD(m_iState, FIELD_INTEGER),
	DEFINE_FIELD(m_AlienOrders, FIELD_INTEGER),
	DEFINE_FIELD(m_vecAlienOrderSpot, FIELD_VECTOR),	
	DEFINE_FIELD(m_AlienOrderObject, FIELD_EHANDLE),
	DEFINE_FIELD(m_hEnemy, FIELD_EHANDLE),
	DEFINE_FIELD(m_bSleeping, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bIgnoreMarines, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bFailedMoveTo, FIELD_BOOLEAN),
	DEFINE_FIELD(m_bMoving, FIELD_BOOLEAN),
	DEFINE_FIELD(m_vecMoveTarget, FIELD_VECTOR),
	DEFINE_FIELD(m_hMoveTarget, FIELD_EHANDLE),
	DEFINE_FIELD(m_fArrivedTolerance, FIELD_FLOAT),
	DEFINE_FIELD(m_bAttacking, FIELD_BOOLEAN),
	DEFINE_FIELD( m_bHoldoutAlien, FIELD_BOOLEAN ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CASW_Simple_Alien, DT_ASW_Simple_Alien)
	
END_SEND_TABLE()

ConVar asw_debug_alien_damage("asw_debug_alien_damage", "0", 0, "Print damage aliens receive");
extern ConVar asw_drone_health;
extern ConVar sk_asw_drone_damage;
extern ConVar asw_drone_gib_chance;
extern ConVar ai_show_hull_attacks;
extern ConVar sv_gravity;
ConVar asw_debug_simple_alien("asw_debug_simple_alien", "0", FCVAR_CHEAT, "Print debug text for simple aliens");
extern int AE_DRONE_WALK_FOOTSTEP;
extern int AE_DRONE_FOOTSTEP_SOFT;
extern int AE_DRONE_FOOTSTEP_HEAVY;
extern int AE_DRONE_MELEE_HIT1;
extern int AE_DRONE_MELEE_HIT2;
extern int AE_DRONE_MELEE1_SOUND;
extern int AE_DRONE_MELEE2_SOUND;
extern int AE_DRONE_MOUTH_BLEED;
extern int AE_DRONE_ALERT_SOUND;
extern int AE_DRONE_SHADOW_ON;

#define ASW_DRONE_MELEE1_RANGE 100.0f

// =========================================
// Creation
// =========================================

CASW_Simple_Alien::CASW_Simple_Alien()
{
	m_fNextPainSound = 0;
	m_bOnGround = false;
	m_bHoldoutAlien = false;
}

CASW_Simple_Alien::~CASW_Simple_Alien()
{
}

void CASW_Simple_Alien::Spawn(void)
{
	BaseClass::Spawn();
	Precache();

	// appearance
	SetModel( SWARM_NEW_DRONE_MODEL );
	m_flAnimTime = gpGlobals->curtime;	
	SetCycle( 0 );
	PlayRunningAnimation();

	// collision
	SetHullType( HULL_MEDIUMBIG ); 
	UTIL_SetSize(this, Vector(-19,-19,0), Vector(19,19,69));
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetCollisionGroup( ASW_COLLISION_GROUP_ALIEN );	

	// movement
	SetMoveType( MOVETYPE_STEP );
	m_fArrivedTolerance = 30.0f;

	// health
	m_iHealth = ASWGameRules()->ModifyAlienHealthBySkillLevel(asw_drone_health.GetInt());

	SetMaxHealth(m_iHealth);
	m_takedamage		= DAMAGE_YES;
		
	// state
	SetState(ASW_SIMPLE_ALIEN_IDLING);
	SetSleeping(true);
	m_fLastThinkTime = gpGlobals->curtime;
	SetThink( &CASW_Simple_Alien::SleepThink );
	SetNextThink( gpGlobals->curtime + random->RandomFloat(0.01f, 0.2f) );

	UpdateSleeping();
}

void CASW_Simple_Alien::Precache()
{
	BaseClass::Precache();

	PrecacheModel( SWARM_NEW_DRONE_MODEL );
}

// =========================================
// Enemy
// =========================================

void CASW_Simple_Alien::SetEnemy(CBaseEntity *pEntity)
{
	if (!pEntity)
	{
		m_hEnemy = NULL;
		return;
	}

	m_hEnemy = pEntity;
	SetState(ASW_SIMPLE_ALIEN_ATTACKING);	// attack the taffer
}

float CASW_Simple_Alien::GetZigZagChaseDistance() const
{
	return 192.0f;
}

Vector& CASW_Simple_Alien::GetChaseDestination(CBaseEntity *pEnt)
{
	static Vector vecDest = vec3_origin;
	vecDest = pEnt->GetAbsOrigin();

	if (!pEnt)
		return vecDest;

	Vector vecDiff = vecDest - GetAbsOrigin();
	vecDiff.z = 0;
	float dist = vecDiff.Length2D();
    if (dist > GetZigZagChaseDistance())	// do we need to zig zag?
	{
		QAngle angSideways(0, UTIL_VecToYaw(vecDiff), 0);
		Vector vecForward, vecRight, vecUp;
		AngleVectors(angSideways, &vecForward, &vecRight, &vecUp);
		
        vecDest = GetAbsOrigin() + vecForward * 92.0f + vecRight * (random->RandomFloat() * 144 - 72);
	}
	return vecDest;
}

void CASW_Simple_Alien::FindNewEnemy()
{
	float dist;
	CBaseEntity *pNearest = UTIL_ASW_NearestMarine(GetAbsOrigin(), dist);
	if (CanSee(pNearest))
	{
		SetEnemy(pNearest);
	}
}

bool CASW_Simple_Alien::CanSee(CBaseEntity *pEntity)
{
	if (!pEntity)
		return false;

	Vector diff = pEntity->GetAbsOrigin() - GetAbsOrigin();
	if (diff.Length2D() < 768.0f)
		return true;

	return false;
}

// =========================================
// Thinking & Decision making
// =========================================

void CASW_Simple_Alien::AlienThink()
{
	float delta = gpGlobals->curtime - m_fLastThinkTime;

	StudioFrameAdvance();
	DispatchAnimEvents(this);

	UpdateYaw(delta);

	if (m_bMoving)
	{
		PerformMovement(delta);

		SetNextThink( gpGlobals->curtime + 0.1f );
	}
	else
	{
		WhatToDoNext(delta);	// this will set the next think interval based on what the alien decides to do next
	}

	m_fLastThinkTime = gpGlobals->curtime;
}

void CASW_Simple_Alien::WhatToDoNext(float delta)
{
	if (m_iState == ASW_SIMPLE_ALIEN_ATTACKING)
	{
		if (!GetEnemy() || !CanSee(GetEnemy()) || GetEnemy()->GetHealth() <= 0)
		{
			LostEnemy();	// clears our enemy and sets us back to idling
		}
		else
		{
			// head towards enemy
			SetMoveTarget(GetChaseDestination(GetEnemy()));

			PerformMovement(delta);				
		}

		SetNextThink( gpGlobals->curtime + 0.1f );
	}
	else if (m_iState == ASW_SIMPLE_ALIEN_IDLING)
	{
		// look for a new enemy?
		FindNewEnemy();
		
		if (GetEnemy())
			SetNextThink( gpGlobals->curtime + 0.1f );
		else
			SetNextThink( gpGlobals->curtime + 0.5f );
	}
	else if (m_iState == ASW_SIMPLE_ALIEN_DEAD)
	{
		// don't need to think again
	}

	// todo: if in moving to dest state, then move towards that dest, etc.
}

void CASW_Simple_Alien::LostEnemy()
{
	SetEnemy(NULL);
	SetState(ASW_SIMPLE_ALIEN_IDLING);
}

void CASW_Simple_Alien::SetState(int iNewState)
{
	if (iNewState == m_iState)
		return;

	m_iState = iNewState;	
}

// =========================================
// Movement
// =========================================

void CASW_Simple_Alien::SetMoveTarget(Vector &vecTarget)
{
	m_vecMoveTarget = vecTarget;
	m_hMoveTarget = NULL;
	m_bMoving = true;
}

float CASW_Simple_Alien::GetIdealSpeed() const
{
	return 300.0f;
}

float CASW_Simple_Alien::GetYawSpeed() const
{
	return 360.0f;
}

void CASW_Simple_Alien::UpdateYaw(float delta)
{
	// don't allow turning in the air
	if (!m_bOnGround)
		return;
	float current = UTIL_AngleMod( GetLocalAngles().y );
	float ideal = UTIL_AngleMod( GetIdealYaw() );
	
	float dt = MIN( 0.2, delta );	
	float newYaw = ASW_ClampYaw( GetYawSpeed(), current, ideal, dt );
		
	if (newYaw != current)
	{
		QAngle angles = GetLocalAngles();
		angles.y = newYaw;
		SetLocalAngles( angles );
	}
}

float CASW_Simple_Alien::GetFaceEnemyDistance() const
{
	return 140.0f;
}

float CASW_Simple_Alien::GetIdealYaw()
{
	if (m_bMoving)
	{
		if (m_iState == ASW_SIMPLE_ALIEN_ATTACKING && GetEnemy()
			&& GetEnemy()->GetAbsOrigin().DistTo(GetAbsOrigin()) < GetFaceEnemyDistance())
		{
			return UTIL_VecToYaw(GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
		}
		else
		{
			if (m_hMoveTarget.Get())
			{
				m_vecMoveTarget = m_hMoveTarget->GetAbsOrigin();
			}
			return UTIL_VecToYaw(m_vecMoveTarget - GetAbsOrigin());
		}
	}

	return GetAbsAngles()[YAW];
}

bool CASW_Simple_Alien::PerformMovement(float deltatime)
{
	if (!m_bMoving)
		return false;

	if (m_hMoveTarget.Get())	// if we're moving to a specific object, update our target vector with its current location
	{
		m_vecMoveTarget = m_hMoveTarget->GetAbsOrigin();
	}
	Vector vecStartDiff = m_vecMoveTarget - GetAbsOrigin();
	vecStartDiff.z = 0;

	// check we're not there already
	if (vecStartDiff.Length2D() <= m_fArrivedTolerance)
	{
		FinishedMovement();
		return true;
	}
	
	// work out the new position we want to be in
	Vector vecDir = vecStartDiff;
	vecDir.NormalizeInPlace();
	//Msg("moving with delta %f\n", delta);
	Vector vecNewPos = GetAbsOrigin() + vecDir * GetIdealSpeed() * deltatime;
	if (TryMove(GetAbsOrigin(), vecNewPos, deltatime))
	{
		// apply gravity to our new position
		ApplyGravity(vecNewPos, deltatime);

		// we moved (at least some portion, maybe all)
		UTIL_SetOrigin(this, vecNewPos);

		// check if we're close enough, or have gone past, the move target
		Vector vecEndDiff = m_vecMoveTarget - GetAbsOrigin();
		vecEndDiff.z = 0;
		if (vecStartDiff.Dot(vecEndDiff) < 0 || vecEndDiff.Length2D() <= m_fArrivedTolerance)
		{
			// we've arrived
			FinishedMovement();
		}
		return true;
	}
	else
	{
		vecNewPos = GetAbsOrigin();
		if (ApplyGravity(vecNewPos, deltatime))	// if we failed to move forward, make sure gravity is still applied
		{
			// we moved (at least some portion, maybe all)
			UTIL_SetOrigin(this, vecNewPos);
		}
	}
	
	FailedMove();
	return false;
}

bool CASW_Simple_Alien::TryMove(const Vector &vecSrc, Vector &vecTarget, float deltatime, bool bStepMove)
{
	// do a trace to the dest
	Ray_t ray;
	trace_t trace;
	CTraceFilterSimple traceFilter(this, GetCollisionGroup() );
	ray.Init( vecSrc, vecTarget, GetHullMins(), GetHullMaxs() );
	enginetrace->TraceRay( ray, MASK_NPCSOLID, &traceFilter, &trace );
	if (trace.startsolid)
	{
		// doh, we're stuck in something!
		//  todo: move us to a safe spot? wait for push out phys props?
		if (asw_debug_simple_alien.GetBool())
			Msg("CASW_Simple_Alien stuck!\n");
		m_MoveFailure.trace = trace;
		m_MoveFailure.vecStartPos = vecSrc;
		m_MoveFailure.vecTargetPos = vecTarget;
		return false;
	}
	if (trace.fraction < 0.1f)	 // barely/didn't move
	{
		// try and do a 'stepped up' move to the target
		if (!bStepMove)
		{
			Vector vecStepSrc = vecSrc;
			vecStepSrc.z += 24;
			Vector vecStepTarget = vecTarget;
			vecTarget.z += 24;
			if (TryMove(vecStepSrc, vecStepTarget, deltatime, true))
			{
				vecTarget = vecStepTarget;
				return true;
			}
		}

		m_MoveFailure.trace = trace;
		m_MoveFailure.vecStartPos = vecSrc;
		m_MoveFailure.vecTargetPos = vecTarget;

		return false;
	}
	else if (trace.fraction < 1)  // we hit something early, but we did move
	{
		// we hit something early
		m_MoveFailure.trace = trace;
		m_MoveFailure.vecStartPos = vecSrc;
		m_MoveFailure.vecTargetPos = vecTarget;

		vecTarget = trace.endpos;		
	}

	return true;
}

// drops us down from the specified position, to the floor
bool CASW_Simple_Alien::ApplyGravity(Vector &vecSrc, float deltatime)
{
	// decide if we're on the ground or not	
	Ray_t ray;
	trace_t trace;
	CTraceFilterSimple traceFilter(this, GetCollisionGroup() );
	ray.Init( vecSrc, vecSrc - Vector(0,0,2), GetHullMins(), GetHullMaxs() );
	enginetrace->TraceRay( ray, MASK_NPCSOLID, &traceFilter, &trace );	
	
	m_bOnGround = (trace.fraction < 1.0f);

	// if we're on the ground, just drop us down as much as we can
	if (m_bOnGround)
	{
		Vector vecGravityTarget = vecSrc;
		vecGravityTarget.z -= sv_gravity.GetFloat() * deltatime;
		// do a trace to the floor
		Ray_t ray;
		trace_t trace;
		CTraceFilterSimple traceFilter(this, GetCollisionGroup() );
		ray.Init( vecSrc, vecGravityTarget, GetHullMins(), GetHullMaxs() );
		enginetrace->TraceRay( ray, MASK_NPCSOLID, &traceFilter, &trace );

		if (trace.fraction > 0 && fabs(trace.endpos.z - vecSrc.z) > 1) // if we moved up/down
		{
			vecSrc = trace.endpos;
			
			return true;
		}
		m_fFallSpeed = 0;  // clear fall speed if we can't fall any further

		return false;
	}

	// we're falling, so apply the fall speed and increase the fall speed over time
	m_fFallSpeed -= sv_gravity.GetFloat() * 1.5f * deltatime;

	Vector vecGravityTarget = vecSrc;
	vecGravityTarget.z += m_fFallSpeed * deltatime;

	// do a trace to the floor
	Ray_t ray2;
	trace_t trace2;
	CTraceFilterSimple traceFilter2(this, GetCollisionGroup() );
	ray2.Init( vecSrc, vecGravityTarget, GetHullMins(), GetHullMaxs() );
	enginetrace->TraceRay( ray2, MASK_NPCSOLID, &traceFilter2, &trace2 );

	if (trace2.fraction > 0 && fabs(trace2.endpos.z - vecSrc.z) > 1) // if we moved up/down
	{
		vecSrc = trace2.endpos;
		return true;
	}
	m_fFallSpeed = 0;  // clear fall speed if we can't fall any further
	return false;
}

// we failed to move this interval
//   returns true if we lay in a new move target
bool CASW_Simple_Alien::FailedMove()
{
	m_bMoving = false;
	m_vecMoveTarget = GetAbsOrigin();

	// we've hit something
	if (m_MoveFailure.trace.m_pEnt)
	{
		Vector vecNormal = m_MoveFailure.trace.plane.normal;
		// todo: if we've hit a marine, hurt him?
		CASW_Marine *pMarine = dynamic_cast<CASW_Marine*>(m_MoveFailure.trace.m_pEnt);
		if (pMarine)
		{
			// it's okay to be stuck on a marine, since that means we're probably clawing him to bits!
			return false;
		}

		// if we've hit another alien, set our normal as though it were a cylinder collision, to get a more accurate avoidance
		//   note: won't work unless the actual movement of the alien is done with a cylinder too
		//CASW_Simple_Alien *pAlien = dynamic_cast<CASW_Simple_Alien*>(m_MoveFailure.trace.m_pEnt);
		//if (pAlien)
		//{
			//vecNormal = pAlien->GetAbsOrigin() - GetAbsOrigin();
			//vecNormal.NormalizeInPlace();
		//}

		// otherwise, assume we've hit some prop or geometry
		
		// if we have no enemy, just randomly move away from the wall
        if (!GetEnemy())
        {
        	SetMoveTarget(PickRandomDestination(32.0f, 16.0f * vecNormal));
			return true;
        }

		// pick a random distance to move away from the wall
		float fDist = 56.25f;
		float fDistPick = random->RandomFloat();
		if (fDistPick > 0.8f)
			fDist = 187.5f;
		else if (fDistPick > 0.4f)
			fDist = 112.5f;

		// find vectors for the two possible directions
		float wall_yaw = UTIL_VecToYaw(vecNormal);        
		float wall_yaw_left = wall_yaw - 90.0f;
		float wall_yaw_right = wall_yaw + 90.0f;
        
        // which one takes us closer to the enemy?
		float enemy_yaw = UTIL_VecToYaw(GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
		if (fabs(UTIL_AngleDiff(enemy_yaw, wall_yaw_left)) < fabs(UTIL_AngleDiff(enemy_yaw, wall_yaw_right)))
		{
			// go left
			Vector vecLeft;
			AngleVectors(QAngle(0, wall_yaw_left, 0), &vecLeft);
			SetMoveTarget(GetAbsOrigin() + vecLeft * fDist);
		}
		else
		{
			// go right
			Vector vecRight;
			AngleVectors(QAngle(0, wall_yaw_right, 0), &vecRight);
			SetMoveTarget(GetAbsOrigin() + vecRight * fDist);			
		}
		return true;
	}

	return false;
}

void CASW_Simple_Alien::FinishedMovement()
{
	m_bMoving = false;
	m_vecMoveTarget = GetAbsOrigin();
}

Vector CASW_Simple_Alien::PickRandomDestination(float dist, Vector bias)
{
	Vector vecRandom = RandomVector(-1, 1);
	vecRandom.z = 0;
	return GetAbsOrigin() + vecRandom * dist + bias;
}

// =========================================
// Sleeping
// =========================================

void CASW_Simple_Alien::SleepThink()
{
	UpdateSleeping();	// see if we should wake up

	// todo: increase this interval if the nearest marine is a long way away?
	if (m_bSleeping)
	{
		float interval = random->RandomFloat() + 1.0f;
		SetNextThink( gpGlobals->curtime + interval );
	}

	m_fLastThinkTime = gpGlobals->curtime;
}

void CASW_Simple_Alien::UpdateSleeping()
{
	if (m_bSleeping)	// asleep:  wake up if we can be seen
	{
		bool bCorpseCanSee = false;
		bool bCanBeSeen = (UTIL_ASW_AnyMarineCanSee(GetAbsOrigin(), 384.0f, bCorpseCanSee) != NULL) || bCorpseCanSee;
		SetSleeping(!bCanBeSeen);
	}
	else	// awake: if have no orders, no enemy and can't be seen, sleep
	{
		if (GetEnemy() == NULL && GetAlienOrders() == AOT_None)
		{
			bool bCorpseCanSee = false;
			bool bCanBeSeen = (UTIL_ASW_AnyMarineCanSee(GetAbsOrigin(), 384.0f, bCorpseCanSee) != NULL) || bCorpseCanSee;
			SetSleeping(!bCanBeSeen);
		}
	}
}

// todo: turn off collision when asleep?
void CASW_Simple_Alien::SetSleeping(bool bAsleep)
{
	if (bAsleep == m_bSleeping)
		return;

	m_bSleeping = bAsleep;
	if (bAsleep)
	{
		// go asleep
		AddEffects( EF_NODRAW );
		SetThink( &CASW_Simple_Alien::SleepThink );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
	else
	{
		// wake up
		RemoveEffects( EF_NODRAW );
		SetThink( &CASW_Simple_Alien::AlienThink );
		SetNextThink( gpGlobals->curtime + 0.1f );

		// set our enemy to the nearest marine
		FindNewEnemy();

		// if we couldn't see any marines, wake up, but just idle
		if (!GetEnemy())
		{
			SetState(ASW_SIMPLE_ALIEN_IDLING);
		}
		else
		{
			AlertSound();
		}
	}
}

// =========================================
// Attacking
// =========================================

void CASW_Simple_Alien::ReachedEndOfSequence()
{
	bool bShouldAttack = ShouldAttack();
	if (bShouldAttack != m_bAttacking)	// make sure we're playing the right sequence for running/attacking
	{
		m_bAttacking = bShouldAttack;
		if (!m_bOnGround)
			PlayFallingAnimation();
		else if (m_bAttacking)
			PlayAttackingAnimation();
		else
			PlayRunningAnimation();

		m_flPlaybackRate = 1.25f;
	}
	else
	{
		if (!m_bOnGround)
			PlayFallingAnimation();
		else
			PlayRunningAnimation();
	}
	
}

bool CASW_Simple_Alien::ShouldAttack()
{
	if (!GetEnemy() || GetEnemy()->GetHealth() <= 0)
		return false;

	float dist = GetEnemy()->GetAbsOrigin().DistTo(GetAbsOrigin());
	return (dist < 100.0f);
}

void CASW_Simple_Alien::MeleeAttack( float distance, float damage, QAngle &viewPunch, Vector &shove )
{
	Vector vecForceDir;

	// Always hurt bullseyes for now
	if ( ( GetEnemy() != NULL ) && ( GetEnemy()->Classify() == CLASS_BULLSEYE ) )
	{
		vecForceDir = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin());
		CTakeDamageInfo info( this, this, damage, DMG_SLASH );
		CalculateMeleeDamageForce( &info, vecForceDir, GetEnemy()->GetAbsOrigin() );
		GetEnemy()->TakeDamage( info );
		return;
	}

	CBaseEntity *pHurt = CheckTraceHullAttack( distance, -Vector(16,16,32), Vector(16,16,32), damage, DMG_SLASH, 5.0f );

	if ( pHurt )
	{
		vecForceDir = ( pHurt->WorldSpaceCenter() - WorldSpaceCenter() );

		CBasePlayer *pPlayer = ToBasePlayer( pHurt );

		if ( pPlayer != NULL )
		{
			//Kick the player angles
			pPlayer->ViewPunch( viewPunch );

			Vector	dir = pHurt->GetAbsOrigin() - GetAbsOrigin();
			VectorNormalize(dir);

			QAngle angles;
			VectorAngles( dir, angles );
			Vector forward, right;
			AngleVectors( angles, &forward, &right, NULL );

			//Push the target back
			pHurt->ApplyAbsVelocityImpulse( - right * shove[1] - forward * shove[0] );
		}

		// Play a random attack hit sound
		AttackSound();

		// bleed em
		if ( UTIL_ShouldShowBlood(pHurt->BloodColor()) )
		{
			// Hit an NPC. Bleed them!
			Vector vecBloodPos;

			Vector forward, right, up;
			AngleVectors( GetAbsAngles(), &forward, &right, &up );

			//if( GetAttachment( "leftclaw", vecBloodPos ) )
			{
				//Vector diff = vecBloodPos - GetAbsOrigin();
				//if (diff.z < 0)
					//vecBloodPos.z = GetAbsOrigin().z - (diff.z * 2);
				vecBloodPos = GetAbsOrigin() + forward * 60 - right * 14 + up * 50;
				SpawnBlood( vecBloodPos, g_vecAttackDir, pHurt->BloodColor(), MIN( damage, 30 ) );
			}

			//if( GetAttachment( "rightclaw", vecBloodPos ) )
			{				
				vecBloodPos = GetAbsOrigin() + forward * 60 + right * 14 + up * 50;
				SpawnBlood( vecBloodPos, g_vecAttackDir, pHurt->BloodColor(), MIN( damage, 30 ) );
			}
						
		}
	}
}

CBaseEntity *CASW_Simple_Alien::CheckTraceHullAttack( float flDist, const Vector &mins, const Vector &maxs, int iDamage, int iDmgType, float forceScale, bool bDamageAnyNPC )
{
	// If only a length is given assume we want to trace in our facing direction
	Vector forward;
	AngleVectors( GetAbsAngles(), &forward );
	Vector vStart = GetAbsOrigin();

	// The ideal place to start the trace is in the center of the attacker's bounding box.
	// however, we need to make sure there's enough clearance. Some of the smaller monsters aren't 
	// as big as the hull we try to trace with. (SJB)
	float flVerticalOffset = WorldAlignSize().z * 0.5;

	if( flVerticalOffset < maxs.z )
	{
		// There isn't enough room to trace this hull, it's going to drag the ground.
		// so make the vertical offset just enough to clear the ground.
		flVerticalOffset = maxs.z + 1.0;
	}

	vStart.z += flVerticalOffset;
	Vector vEnd = vStart + (forward * flDist );
	return CheckTraceHullAttack( vStart, vEnd, mins, maxs, iDamage, iDmgType, forceScale, bDamageAnyNPC );
}

CBaseEntity *CASW_Simple_Alien::CheckTraceHullAttack( const Vector &vStart, const Vector &vEnd, const Vector &mins, const Vector &maxs, int iDamage, int iDmgType, float flForceScale, bool bDamageAnyNPC )
{
	// Handy debuging tool to visualize HullAttack trace
	if ( ai_show_hull_attacks.GetBool() )
	{
		float length	 = (vEnd - vStart ).Length();
		Vector direction = (vEnd - vStart );
		VectorNormalize( direction );
		Vector hullMaxs = maxs;
		hullMaxs.x = length + hullMaxs.x;
		NDebugOverlay::BoxDirection(vStart, mins, hullMaxs, direction, 100,255,255,20,1.0);
		NDebugOverlay::BoxDirection(vStart, mins, maxs, direction, 255,0,0,20,1.0);
	}
	
	CASW_Trace_Filter_Melee traceFilter( this, COLLISION_GROUP_NONE, this, bDamageAnyNPC );

	Ray_t ray;
	ray.Init( vStart, vEnd, mins, maxs );

	trace_t tr;
	enginetrace->TraceRay( ray, MASK_SHOT_HULL, &traceFilter, &tr );

	CBaseEntity *pEntity = traceFilter.m_pHit;
	
	if ( pEntity == NULL )
	{
		// See if perhaps I'm trying to claw/bash someone who is standing on my head.
		Vector vecTopCenter;
		Vector vecEnd;
		Vector vecMins, vecMaxs;

		// Do a tracehull from the top center of my bounding box.
		vecTopCenter = GetAbsOrigin();
		CollisionProp()->WorldSpaceAABB( &vecMins, &vecMaxs );
		vecTopCenter.z = vecMaxs.z + 1.0f;
		vecEnd = vecTopCenter;
		vecEnd.z += 2.0f;
		
		ray.Init( vecTopCenter, vEnd, mins, maxs );
		enginetrace->TraceRay( ray, MASK_SHOT_HULL, &traceFilter, &tr );

		pEntity = traceFilter.m_pHit;
	}

	return pEntity;
}

// =========================================
// Alien Orders
// =========================================

void CASW_Simple_Alien::SetAlienOrders(AlienOrder_t Orders, Vector vecOrderSpot, CBaseEntity* pOrderObject)
{
	m_AlienOrders = Orders;
	m_vecAlienOrderSpot = vecOrderSpot;	// unused currently
	m_AlienOrderObject = pOrderObject;

	if (Orders == AOT_None)
	{
		ClearAlienOrders();
		return;
	}

	// todo: set our state etc based on these?	
}

AlienOrder_t CASW_Simple_Alien::GetAlienOrders()
{
	return m_AlienOrders;
}

void CASW_Simple_Alien::ClearAlienOrders()
{
	m_AlienOrders = AOT_None;
	m_vecAlienOrderSpot = vec3_origin;
	m_AlienOrderObject = NULL;
	m_bIgnoreMarines = false;
	m_bFailedMoveTo = false;
}

void CASW_Simple_Alien::ASW_Ignite( float flFlameLifetime, float flSize, CBaseEntity *pAttacker, CBaseEntity *pDamagingWeapon )
{
	if (AllowedToIgnite())
	{
		if( IsOnFire() )
		return;

		CEntityFlame *pFlame = CEntityFlame::Create( this, flFlameLifetime, flSize );
		if (pFlame)
		{
			if (pAttacker)
				pFlame->SetOwnerEntity(pAttacker);
			pFlame->SetLifetime( flFlameLifetime );
			AddFlag( FL_ONFIRE );

			SetEffectEntity( pFlame );
		}

		m_OnIgnite.FireOutput( this, this );
	}
}

// =========================================
// Health
// =========================================

int CASW_Simple_Alien::OnTakeDamage( const CTakeDamageInfo &info )
{
	// don't get hurt if our attacker is our friend
	if (info.GetAttacker() && info.GetAttacker()->Classify() != CLASS_NONE)
	{
		// Proper way to check, but BCC default relationship array is private :/
		//Disposition_t disp = CBaseCombatCharacter::m_DefaultRelationship[Classify()][info.GetAttacker()->Classify()].disposition;
		//if (disp == D_LI)
			//return 0;

		// Hacky way to stop simple aliens getting hurt by other Infested aliens
		Class_T c = info.GetAttacker()->Classify();
		if ( IsAlienClass( c ) )
			return 0;
	}

	if (asw_debug_alien_damage.GetBool())
	{
		Msg("%d %s hurt by %f dmg\n", entindex(), GetClassname(), info.GetDamage());
	}

	int iDamage = BaseClass::OnTakeDamage(info);

	if (iDamage > 0 && GetHealth() > 0)
	{
		PainSound(info);
	}

	return iDamage;
}

void CASW_Simple_Alien::ASWTraceBleed( float flDamage, const Vector &vecDir, trace_t *ptr, int bitsDamageType )
{
	if ((BloodColor() == DONT_BLEED) || (BloodColor() == BLOOD_COLOR_MECH))
	{
		return;
	}

	if (flDamage == 0)
		return;

	if (! (bitsDamageType & (DMG_CRUSH | DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB | DMG_AIRBOAT)))
		return;

	// make blood decal on the wall!
	trace_t Bloodtr;
	Vector vecTraceDir;
	float flNoise;
	int cCount;
	int i;

#ifdef GAME_DLL
	if ( !IsAlive() )
	{
		// dealing with a dead npc.
		if ( GetMaxHealth() <= 0 )
		{
			// no blood decal for a npc that has already decalled its limit.
			return;
		}
		else
		{
			m_iMaxHealth -= 1;
		}
	}
#endif

	if (flDamage < 10)
	{
		flNoise = 0.1;
		cCount = 1;
	}
	else if (flDamage < 25)
	{
		flNoise = 0.2;
		cCount = 2;
	}
	else
	{
		flNoise = 0.3;
		cCount = 4;
	}

	float flTraceDist = (bitsDamageType & DMG_AIRBOAT) ? 384 : 172;
	for ( i = 0 ; i < cCount ; i++ )
	{
		vecTraceDir = vecDir * -1;// trace in the opposite direction the shot came from (the direction the shot is going)

		vecTraceDir.x += random->RandomFloat( -flNoise, flNoise );
		vecTraceDir.y += random->RandomFloat( -flNoise, flNoise );
		vecTraceDir.z += random->RandomFloat( -flNoise, flNoise );

		// Don't bleed on grates.
		Vector vecEndPos = ptr->endpos; // + m_LagCompensation.GetLagCompensationOffset();
		AI_TraceLine( vecEndPos, vecEndPos + vecTraceDir * -flTraceDist, MASK_SOLID_BRUSHONLY & ~CONTENTS_GRATE, this, COLLISION_GROUP_NONE, &Bloodtr);

		if ( Bloodtr.fraction != 1.0 )
		{
			UTIL_BloodDecalTrace( &Bloodtr, BloodColor() );
		}
	}
}

void CASW_Simple_Alien::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	if ( m_takedamage == DAMAGE_NO )
		return;

	CTakeDamageInfo subInfo = info;	
	m_nForceBone = ptr->physicsbone;		// save this bone for physics forces
	Assert( m_nForceBone > -255 && m_nForceBone < 256 );

	if ( subInfo.GetDamage() >= 1.0 && !(subInfo.GetDamageType() & DMG_SHOCK )
		 && !(subInfo.GetDamageType() & DMG_BURN ))
	{		
		// NPC's always bleed. Players only bleed in multiplayer.
		//SpawnBlood( ptr->endpos, vecDir, BloodColor(), subInfo.GetDamage() );// a little surface blood.		
		UTIL_ASW_DroneBleed( ptr->endpos, vecDir, 4 );	//  + m_LagCompensation.GetLagCompensationOffset()
		ASWTraceBleed( subInfo.GetDamage(), vecDir, ptr, subInfo.GetDamageType() );
	}

	if( info.GetInflictor() )
	{
		subInfo.SetInflictor( info.GetInflictor() );
	}
	else
	{
		subInfo.SetInflictor( info.GetAttacker() );
	}

	AddMultiDamage( subInfo, this );
}

bool CASW_Simple_Alien::ShouldGib( const CTakeDamageInfo &info )
{
	// don't gib if we burnt to death
	if (info.GetDamageType() & DMG_BURN)
		return false;

	if (info.GetDamageType() & DMG_ALWAYSGIB)
		return true;

	return RandomFloat() < asw_drone_gib_chance.GetFloat();
}

bool CASW_Simple_Alien::CorpseGib( const CTakeDamageInfo &info )
{
	CEffectData	data;

	data.m_vOrigin = WorldSpaceCenter();
	data.m_vNormal = data.m_vOrigin - info.GetDamagePosition();
	VectorNormalize( data.m_vNormal );
	
	data.m_flScale = RemapVal( m_iHealth, 0, -500, 1, 3 );
	data.m_flScale = clamp( data.m_flScale, 1, 3 );
	data.m_nColor = m_nSkin;

	//DispatchEffect( "DroneGib", data );

	return true;
}

bool  CASW_Simple_Alien::Event_Gibbed( const CTakeDamageInfo &info )
{
	bool fade = false;
	ConVar const *agibs = cvar->FindVar( "violence_agibs" );
	if ( agibs && agibs->GetInt() == 0 )
	{
		fade = true;
	}

	m_takedamage	= DAMAGE_NO;
	AddSolidFlags( FSOLID_NOT_SOLID );
	m_lifeState		= LIFE_DEAD;

	if ( fade )
	{
		CorpseFade();
		return false;
	}
	
	AddEffects( EF_NODRAW ); // make the model invisible.
	CorpseGib( info );

	UTIL_Remove( this );
	SetThink( NULL );
	return true;
}

void CASW_Simple_Alien::CorpseFade()
{
	StopAnimation();
	SetAbsVelocity( vec3_origin );
	SetMoveType( MOVETYPE_NONE );
	SetLocalAngularVelocity( vec3_angle );
	m_flAnimTime = gpGlobals->curtime;
	AddEffects( EF_NOINTERP );
	SUB_StartFadeOut();
}

Vector CASW_Simple_Alien::CalcDeathForceVector( const CTakeDamageInfo &info )
{
	// Already have a damage force in the data, use that.
	if ( info.GetDamageForce() != vec3_origin || (g_pGameRules->Damage_NoPhysicsForce(info.GetDamageType())))
	{
		if( info.GetDamageType() & DMG_BLAST )
		{
			float scale = random->RandomFloat( 0.85, 1.15 );
			Vector force = info.GetDamageForce();
			force.x *= scale;
			force.y *= scale;
			// Try to always exaggerate the upward force because we've got pretty harsh gravity
			force.z *= (force.z > 0) ? 1.15 : scale;
			return force;
		}

		return info.GetDamageForce();
	}

	CBaseEntity *pForce = info.GetInflictor();
	if ( !pForce )
	{
		pForce = info.GetAttacker();
	}

	if ( pForce )
	{
		// Calculate an impulse large enough to push a 75kg man 4 in/sec per point of damage
		float forceScale = info.GetDamage() * 75 * 4;

		Vector forceVector;
		// If the damage is a blast, point the force vector higher than usual, this gives 
		// the ragdolls a bodacious "really got blowed up" look.
		if( info.GetDamageType() & DMG_BLAST )
		{
			// exaggerate the force from explosions a little (37.5%)
			forceVector = (GetLocalOrigin() + Vector(0, 0, WorldAlignSize().z) ) - pForce->GetLocalOrigin();
			VectorNormalize(forceVector);
			forceVector *= 1.375f;
		}
		else
		{
			// taking damage from self?  Take a little random force, but still try to collapse on the spot.
			if ( this == pForce )
			{
				forceVector.x = random->RandomFloat( -1.0f, 1.0f );
				forceVector.y = random->RandomFloat( -1.0f, 1.0f );
				forceVector.z = 0.0;
				forceScale = random->RandomFloat( 1000.0f, 2000.0f );
			}
			else
			{
				// UNDONE: Collision forces are baked in to CTakeDamageInfo now
				// UNDONE: Is this MOVETYPE_VPHYSICS code still necessary?
				if ( pForce->GetMoveType() == MOVETYPE_VPHYSICS )
				{
					// killed by a physics object
					IPhysicsObject *pPhysics = VPhysicsGetObject();
					if ( !pPhysics )
					{
						pPhysics = pForce->VPhysicsGetObject();
					}
					pPhysics->GetVelocity( &forceVector, NULL );
					forceScale = pPhysics->GetMass();
				}
				else
				{
					forceVector = GetLocalOrigin() - pForce->GetLocalOrigin();
					VectorNormalize(forceVector);
				}
			}
		}
		return forceVector * forceScale;
	}
	return vec3_origin;
}

void CASW_Simple_Alien::Event_Killed( const CTakeDamageInfo &info )
{
	if (ASWGameRules())
	{
		ASWGameRules()->AlienKilled(this, info);
	}	

	if (m_hSpawner.Get())
		m_hSpawner->AlienKilled(this);

	// Calculate death force
	Vector forceVector = CalcDeathForceVector( info );

	StopLoopingSounds();
	DeathSound(info);
	// todo: remove touch function?

	SetState(ASW_SIMPLE_ALIEN_DEAD);

	// todo: Select a death pose to extrapolate the ragdoll's velocity?
	//SelectDeathPose( info );

	if (!ShouldGib(info))
	{
		SetCollisionGroup(ASW_COLLISION_GROUP_PASSABLE);	// don't block marines by dead bodies

		if ( BecomeRagdollOnClient( forceVector ) )
		{

		}
	}
	else
	{
		Event_Gibbed( info );
	}
}

// =========================================
// Debugging
// =========================================

int CASW_Simple_Alien::DrawDebugTextOverlays()
{
	int text_offset = 0;

	text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		char tempstr[512];
		// health
		Q_snprintf(tempstr,sizeof(tempstr),"Health: %i", m_iHealth );
		EntityText(text_offset,tempstr,0);
		text_offset++;
		// state
		static const char *pStateNames[] = { "Idling", "Attacking" };
		if ( m_iState < ARRAYSIZE(pStateNames) )
		{
			Q_snprintf(tempstr,sizeof(tempstr),"State: %s, ", pStateNames[m_iState] );
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}
		// enemy
		if (GetEnemy())
		{
			Q_snprintf(tempstr,sizeof(tempstr),"Enemy: %d %s", GetEnemy()->entindex(), GetEnemy()->GetClassname() );
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}
		else
		{
			Q_snprintf(tempstr,sizeof(tempstr),"Enemy: NONE" );
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}
		// alien orders
		static const char *pOrderNames[] = { "SpreadThenHibernate", "MoveTo", "MoveToIgnoring", "MoveToNearestM", "None" };		
		if ( (int)m_AlienOrders < ARRAYSIZE(pOrderNames) )
		{
			Q_snprintf(tempstr,sizeof(tempstr),"Orders: %s, ", pOrderNames[m_iState] );
			EntityText(text_offset,tempstr,0);
			text_offset++;
		}
		// moving
		Q_snprintf(tempstr,sizeof(tempstr),"Moving: %s, ", m_bMoving ? "Yes" : "No" );
		EntityText(text_offset,tempstr,0);
		text_offset++;
		
		// sleeping
		Q_snprintf(tempstr,sizeof(tempstr),"Sleeping: %s, ", m_bSleeping ? "Yes" : "No" );
		EntityText(text_offset,tempstr,0);
		text_offset++;

		// collision
		Q_snprintf(tempstr,sizeof(tempstr),"Col Group: %d, ", GetCollisionGroup());
		EntityText(text_offset,tempstr,0);
		text_offset++;
	}

	return text_offset;
}

void CASW_Simple_Alien::DrawDebugGeometryOverlays()
{
	BaseClass::DrawDebugGeometryOverlays();
	// draw route
	if ((m_debugOverlays & OVERLAY_NPC_ROUTE_BIT))
	{		
		if ( m_bMoving )
		{
			Vector vecDiff = m_vecMoveTarget - GetAbsOrigin();
			NDebugOverlay::Line(WorldSpaceCenter(), WorldSpaceCenter() + vecDiff,0,0,255,true,0.0);
		}
		if (GetEnemy())
		{
			NDebugOverlay::Line(WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(),255,0,0,true,0.0);
		}
	}
}

void CASW_Simple_Alien::HandleAnimEvent( animevent_t *pEvent )
{
	int nEvent = pEvent->Event();

	if ( nEvent == AE_DRONE_WALK_FOOTSTEP )
	{
		return;
	}

	if ( nEvent == AE_DRONE_FOOTSTEP_SOFT )
	{
		return;
	}

	if ( nEvent == AE_DRONE_FOOTSTEP_HEAVY )
	{
		return;
	}

	if ( nEvent == AE_DRONE_MELEE_HIT1 )
	{
		MeleeAttack( ASW_DRONE_MELEE1_RANGE, ASWGameRules()->ModifyAlienDamageBySkillLevel(sk_asw_drone_damage.GetFloat()), QAngle( 20.0f, 0.0f, -12.0f ), Vector( -250.0f, 1.0f, 1.0f ) );
		return;
	}

	if ( nEvent == AE_DRONE_MELEE_HIT2 )
	{
		MeleeAttack( ASW_DRONE_MELEE1_RANGE, ASWGameRules()->ModifyAlienDamageBySkillLevel(sk_asw_drone_damage.GetFloat()), QAngle( 20.0f, 0.0f, 0.0f ), Vector( -350.0f, 1.0f, 1.0f ) );
		return;
	}	
	
	if ( nEvent == AE_DRONE_MELEE1_SOUND )
	{
		AttackSound();
		return;
	}
	
	if ( nEvent == AE_DRONE_MELEE2_SOUND )
	{
		AttackSound();
		return;
	}

	if ( nEvent == AE_DRONE_MOUTH_BLEED )
	{
		Vector vecOrigin, vecDir;
		if (GetAttachment( LookupAttachment("mouth") , vecOrigin, &vecDir ))
			UTIL_ASW_BloodDrips( vecOrigin+vecDir*3, vecDir, BLOOD_COLOR_RED, 6 );
		return;
	}

	if ( nEvent == AE_DRONE_ALERT_SOUND )
	{
		EmitSound( "ASW_Drone.Alert" );
		return;
	}

	if ( nEvent == AE_DRONE_SHADOW_ON)
	{
		RemoveEffects( EF_NOSHADOW );
		return;
	}

	BaseClass::HandleAnimEvent( pEvent );
}

void CASW_Simple_Alien::SetHealthByDifficultyLevel()
{
	SetHealth(ASWGameRules()->ModifyAlienHealthBySkillLevel(asw_drone_health.GetInt()));
}