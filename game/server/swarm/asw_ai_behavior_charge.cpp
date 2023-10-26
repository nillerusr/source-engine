//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_charge.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"
#include "asw_marine.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_ChargeBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_ChargeBehavior );

Activity ACT_ALIEN_CHARGE_START;
Activity ACT_ALIEN_CHARGE_STOP;
Activity ACT_ALIEN_CHARGE_CRASH;
Activity ACT_ALIEN_CHARGE_RUN;
Activity ACT_ALIEN_CHARGE_ANTICIPATION;
Activity ACT_ALIEN_CHARGE_HIT;

ConVar asw_debug_charging( "asw_debug_charging", "0", FCVAR_CHEAT, "Visualize charging" );

//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_ChargeBehavior::CAI_ASW_ChargeBehavior( )
{
	m_flMinRange = 200.0f;
	m_flMaxRange = 600.0f;
	m_flAngle = cos( DEG2RAD( 90.0f - 15.0f ) );
	m_IsCrouched = UTL_INVAL_SYMBOL;
	m_iChargeMisses = 0;
	m_flMinDamage = 1.0f;
	m_flMaxDamage = 1.0f;
	m_bDecidedNotToStop = false;
	m_flRetryChargeChance = 0.0f;
	m_FinishedChargeParm = UTL_INVAL_SYMBOL;
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_ChargeBehavior::KeyValue( const char *szKeyName, const char *szValue )
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
	else if ( V_stricmp( szKeyName, "angle" ) == 0 )
	{
		m_flAngle = cos( DEG2RAD( 90.0f - atof( szValue ) ) );
		return true;
	}
	else if ( V_stricmp( szKeyName, "is_crouched" ) == 0 )
	{
		m_IsCrouched = GetSymbol( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "min_damage" ) == 0 )
	{
		m_flMinDamage = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "max_damage" ) == 0 )
	{
		m_flMaxDamage = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "finished_charge_param" ) == 0 )
	{
		m_FinishedChargeParm = GetSymbol( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "retry_charge_chance" ) == 0 )
	{
		m_flRetryChargeChance = atof( szValue );
		return true;
	}
		

	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_ChargeBehavior::Precache( void )
{
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CAI_ASW_ChargeBehavior::Init( )
{
#if 0
	CASW_Alien *pNPC = static_cast< CASW_Alien * >( GetOuter() );
	if ( !pNPC )
	{
		return;
	}
#endif
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_ChargeBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

	if ( HasCondition( COND_CHARGE_CAN_CHARGE ) == false )
	{
		return false;
	}

	return BaseClass::CanSelectSchedule();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_ChargeBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_ChargeBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();

	ClearCondition( COND_CHARGE_CAN_CHARGE );

	Vector		vRightNPC, vForwardNPC, vForwardEnemy;
	AngleVectors( GetAbsAngles(), &vForwardNPC, &vRightNPC, NULL );

	int nTotalPotential = GetEnemies()->NumEnemies();

	CBaseEntity		**pEligiblePlayers = ( CBaseEntity ** )stackalloc( sizeof( CBaseEntity * ) * nTotalPotential );
	int				nEligibleCount = 0;

	AIEnemiesIter_t iter;
	for( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst( &iter ); pEMemory != NULL; pEMemory = GetEnemies()->GetNext( &iter ) )
	{
		CBaseEntity	*pEntity = pEMemory->hEnemy;

		Vector vDelta = GetAbsOrigin() - pEntity->GetAbsOrigin();
		vForwardEnemy = vDelta;
		float flLen = vForwardEnemy.NormalizeInPlace();
		if ( flLen < m_flMinRange || flLen > m_flMaxRange )
		{
			continue;
		}

		float		flResult = vForwardNPC.Dot( vForwardEnemy );
		if ( flResult > 0.0f )
		{	// we are behind
			continue;
		}

		flResult = vRightNPC.Dot( vForwardEnemy );

		if ( flResult >= -m_flAngle && flResult <= m_flAngle )
		{
			pEligiblePlayers[ nEligibleCount ] = pEntity;
			nEligibleCount++;
		}
	}

	int nBestEnemy = -1;
	int nBestThreat = -999;
	for ( int i = 0; i < nEligibleCount; i++ )
	{
		int nThreat = GetOuter()->IRelationPriority( pEligiblePlayers[ i ] );
		if ( nThreat > nBestThreat )
		{
			nBestEnemy = i;
			nBestThreat = nThreat;
		}
	}

	if ( nBestEnemy >= 0 )
	{
		SetCondition( COND_CHARGE_CAN_CHARGE );
		m_ChargeDestination = pEligiblePlayers[ nBestEnemy ]->GetAbsOrigin();
	}
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_ChargeBehavior::GatherCommonConditions( )
{
	BaseClass::GatherCommonConditions();
}


//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_ChargeBehavior::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_CHARGE_MOVE:
			{
				// HACK: Because the alien stops running his normal blended movement 
				//		 here, he also needs to remove his blended movement layers!
				GetMotor()->MoveStop();

				Activity	MovementActivity = HaveSequenceForActivity( ACT_ALIEN_CHARGE_START ) ? ACT_ALIEN_CHARGE_START : ACT_ALIEN_CHARGE_RUN;
				if ( !HaveSequenceForActivity( MovementActivity ) && HaveSequenceForActivity( ACT_RUN_AGITATED ) )
				{
					MovementActivity = ACT_RUN_AGITATED;
				}

				if ( m_IsCrouched != UTL_INVAL_SYMBOL )
				{
					if ( GetBehaviorParam( m_IsCrouched ) != 0 )
					{
						MovementActivity = ACT_RUN_CROUCH;
					}
				}
				SetActivity( MovementActivity );
				m_bDecidedNotToStop = false;
			}
			break;

		default:
			BaseClass::StartTask( pTask );
			break;
	}
}


//------------------------------------------------------------------------------
// Purpose: routine called every frame when a task is running
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_ChargeBehavior::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_CHARGE_MOVE:
			{
				Activity eActivity = GetOuter()->GetActivity();

				// TODO: If it's frozen, crash instantly?

				// See if we're trying to stop after hitting/missing our target
				if ( eActivity == ACT_ALIEN_CHARGE_STOP || eActivity == ACT_ALIEN_CHARGE_CRASH ) 
				{
					if ( GetOuter()->IsActivityFinished() )
					{
						SetBehaviorParam( m_FinishedChargeParm, 1 );
						TaskComplete();
						return;
					}

					// Still in the process of slowing down. Run movement until it's done.
					GetOuter()->AutoMovement();
					return;
				}

				// Check for manual transition
				if ( ( eActivity == ACT_ALIEN_CHARGE_START ) && ( GetOuter()->IsActivityFinished() ) )
				{
					SetActivity( ACT_ALIEN_CHARGE_RUN );
				}

				// See if we're still running
				if ( eActivity == ACT_ALIEN_CHARGE_RUN || eActivity == ACT_ALIEN_CHARGE_START ) 
				{
					if ( HasCondition( COND_NEW_ENEMY ) || HasCondition( COND_LOST_ENEMY ) || HasCondition( COND_ENEMY_DEAD ) )
					{
						if ( HaveSequenceForActivity( ACT_ALIEN_CHARGE_STOP ) )
						{
							SetActivity( ACT_ALIEN_CHARGE_STOP );
						}
						return;
					}
					else 
					{
						if ( GetEnemy() != NULL )
						{
							Vector	goalDir = ( GetEnemy()->GetAbsOrigin() - GetAbsOrigin() );
							VectorNormalize( goalDir );

							if ( DotProduct( GetOuter()->BodyDirection2D(), goalDir ) < 0.25f )
							{
								if ( !m_bDecidedNotToStop )
								{
									// We've missed the target. Randomly decide not to stop, which will cause
									// the guard to just try and swing around for another pass.
									if ( RandomFloat() >= m_flRetryChargeChance )
									{
										m_iChargeMisses++;
										if ( HaveSequenceForActivity( ACT_ALIEN_CHARGE_STOP ) )
										{
											SetActivity( ACT_ALIEN_CHARGE_STOP );
										}
										m_bDecidedNotToStop = false;
									}
									else
									{
										m_bDecidedNotToStop = true;
									}
								}
							}
							else
							{
								m_bDecidedNotToStop = false;
							}
						}
					}
				}

				// Steer towards our target
				float idealYaw;
				if ( GetEnemy() == NULL )
				{
					idealYaw = GetMotor()->GetIdealYaw();
				}
				else
				{
					idealYaw = GetOuter()->CalcIdealYaw( GetEnemy()->GetAbsOrigin() );
				}

				// Add in our steering offset
				idealYaw += ChargeSteer();

				// Turn to face
				GetMotor()->SetIdealYawAndUpdate( idealYaw );

				// See if we're going to run into anything soon
				ChargeLookAhead();

				// Let our animations simply move us forward. Keep the result
				// of the movement so we know whether we've hit our target.
				AIMoveTrace_t moveTrace;
				if ( GetOuter()->AutoMovement( GetEnemy(), &moveTrace ) == false )
				{
					// Only stop if we hit the world
					if ( HandleChargeImpact( moveTrace.vEndPosition, moveTrace.pObstruction ) )
					{
						// If we're starting up, this is an error
						if ( eActivity == ACT_ALIEN_CHARGE_START )
						{
							TaskFail( "Unable to make initial movement of charge\n" );
							return;
						}

						// Crash unless we're trying to stop already
						if ( eActivity != ACT_ALIEN_CHARGE_STOP && HaveSequenceForActivity( ACT_ALIEN_CHARGE_STOP ) )
						{
							if ( ( moveTrace.fStatus == AIMR_BLOCKED_WORLD && moveTrace.vHitNormal == vec3_origin )
									|| !HaveSequenceForActivity( ACT_ALIEN_CHARGE_CRASH ) )
							{
								SetActivity( ACT_ALIEN_CHARGE_STOP );
							}
							else
							{
								SetActivity( ACT_ALIEN_CHARGE_CRASH );
							}
						}
					}
					else if ( moveTrace.pObstruction )
					{
						// If we hit an antlion, don't stop, but kill it
						if ( moveTrace.pObstruction->Classify() == CLASS_ANTLION )
						{
							if ( FClassnameIs( moveTrace.pObstruction, "npc_antlionguard" ) )
							{
								// Crash unless we're trying to stop already
								if ( eActivity != ACT_ALIEN_CHARGE_STOP && HaveSequenceForActivity( ACT_ALIEN_CHARGE_STOP ) )
								{
									SetActivity( ACT_ALIEN_CHARGE_STOP );
								}
							}
							else
							{
								ApplyChargeDamage( moveTrace.pObstruction, moveTrace.pObstruction->GetHealth() );
							}
						}
					}
				}
			}
			break;

		default:
			BaseClass::RunTask( pTask );
			break;
	}
}


//------------------------------------------------------------------------------
// Purpose: routine called to select what schedule we want to run
// Output : returns the schedule id of the schedule we want to run
//------------------------------------------------------------------------------
int CAI_ASW_ChargeBehavior::SelectSchedule()
{
	return SCHED_CHARGE_DO;
}


//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
void CAI_ASW_ChargeBehavior::HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm )
{
#if 0
	switch( eEvent )
	{
	}
#endif
}

void CAI_ASW_ChargeBehavior::ChargeDamage( CBaseEntity *pTarget )
{
	float flMinDamage = ASWGameRules()->ModifyAlienDamageBySkillLevel( m_flMinDamage );
	float flMaxDamage = ASWGameRules()->ModifyAlienDamageBySkillLevel( m_flMaxDamage );

	flMinDamage = ( flMinDamage < 1.0f ? 1.0f : flMinDamage );
	flMaxDamage = ( flMaxDamage < 1.0f ? 1.0f : flMaxDamage );

	ApplyChargeDamage( pTarget, RandomFloat( flMinDamage, flMaxDamage ) );
}

//-----------------------------------------------------------------------------
// Purpose: Calculate & apply damage & force for a charge to a target.
//			Done outside of the guard because we need to do this inside a trace filter.
//-----------------------------------------------------------------------------
void CAI_ASW_ChargeBehavior::ApplyChargeDamage( CBaseEntity *pTarget, float flDamage )
{
	Vector attackDir = ( pTarget->WorldSpaceCenter() - WorldSpaceCenter() );
	VectorNormalize( attackDir );
	Vector offset = RandomVector( -32, 32 ) + pTarget->WorldSpaceCenter();

	// Generate enough force to make a 75kg guy move away at 700 in/sec
	Vector vecForce = attackDir * ImpulseScale( 75, 700 );

	// Deal the damage
	CTakeDamageInfo	info( GetOuter(), GetOuter(), vecForce, offset, flDamage, DMG_CLUB );

	pTarget->TakeDamage( info );
}

//-----------------------------------------------------------------------------
// Purpose: A simple trace filter class to skip small moveable physics objects
//-----------------------------------------------------------------------------
class CTraceFilterCharge : public CTraceFilter
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS_NOBASE( CTraceFilterCharge );

	CTraceFilterCharge( const IHandleEntity *passentity, int collisionGroup, float minMass )
		: m_pPassEnt(passentity), m_collisionGroup(collisionGroup), m_minMass(minMass)
	{
	}
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		if ( !StandardFilterRules( pHandleEntity, contentsMask ) )
			return false;

		if ( !PassServerEntityFilter( pHandleEntity, m_pPassEnt ) )
			return false;

		// Don't test if the game code tells us we should ignore this collision...
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( pEntity )
		{
			if ( !pEntity->ShouldCollide( m_collisionGroup, contentsMask ) )
				return false;

			if ( !g_pGameRules->ShouldCollide( m_collisionGroup, pEntity->GetCollisionGroup() ) )
				return false;

			// don't test small moveable physics objects (unless it's an NPC)
			if ( !pEntity->IsNPC() && pEntity->GetMoveType() == MOVETYPE_VPHYSICS )
			{
				IPhysicsObject *pPhysics = pEntity->VPhysicsGetObject();
				Assert(pPhysics);
				if ( pPhysics->IsMoveable() && pPhysics->GetMass() < m_minMass )
					return false;
			}

			// TODO: If we hit certain alien types, don't stop, but kill them?
// 			if ( pEntity->Classify() == CLASS_ANTLION )
// 			{
// 				CBaseEntity *pGuard = (CBaseEntity*)EntityFromEntityHandle( m_pPassEnt );
// 				ApplyChargeDamage( pGuard, pEntity, pEntity->GetHealth() );
// 				return false;
// 			}
		}

		return true;
	}



private:
	const IHandleEntity *m_pPassEnt;
	int m_collisionGroup;
	float m_minMass;
};

//-----------------------------------------------------------------------------
inline void TraceHull_Charge( const Vector &vecAbsStart, const Vector &vecAbsEnd, const Vector &hullMin, 
								  const Vector &hullMax,	unsigned int mask, const CBaseEntity *ignore, 
								  int collisionGroup, trace_t *ptr, float minMass )
{
	Ray_t ray;
	ray.Init( vecAbsStart, vecAbsEnd, hullMin, hullMax );
	CTraceFilterCharge traceFilter( ignore, collisionGroup, minMass );
	enginetrace->TraceRay( ray, mask, &traceFilter, ptr );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if our charge target is right in front of the guard
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAI_ASW_ChargeBehavior::EnemyIsRightInFrontOfMe( CBaseEntity **pEntity )
{
	if ( !GetEnemy() )
		return false;

	if ( (GetEnemy()->WorldSpaceCenter() - WorldSpaceCenter()).LengthSqr() < (156*156) )
	{
		Vector vecLOS = ( GetEnemy()->GetAbsOrigin() - GetAbsOrigin() );
		vecLOS.z = 0;
		VectorNormalize( vecLOS );
		Vector vBodyDir = GetOuter()->BodyDirection2D();
		if ( DotProduct( vecLOS, vBodyDir ) > 0.8 )
		{
			// He's in front of me, and close. Make sure he's not behind a wall.
			trace_t tr;
			UTIL_TraceLine( WorldSpaceCenter(), GetEnemy()->EyePosition(), MASK_SOLID, GetOuter(), COLLISION_GROUP_NONE, &tr );
			if ( tr.m_pEnt == GetEnemy() )
			{
				*pEntity = tr.m_pEnt;
				return true;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: While charging, look ahead and see if we're going to run into anything.
//			If we are, start the gesture so it looks like we're anticipating the hit.
//-----------------------------------------------------------------------------
void CAI_ASW_ChargeBehavior::ChargeLookAhead( void )
{
	trace_t	tr;
	Vector vecForward;
	GetOuter()->GetVectors( &vecForward, NULL, NULL );
	Vector vecTestPos = GetAbsOrigin() + ( vecForward * GetOuter()->m_flGroundSpeed * 0.75 );
	Vector testHullMins = GetHullMins();
	testHullMins.z += (GetOuter()->StepHeight() * 2);
	TraceHull_Charge( GetAbsOrigin(), vecTestPos, testHullMins, GetHullMaxs(), MASK_SHOT_HULL, GetOuter(), COLLISION_GROUP_NONE, &tr, GetMass() * 0.5 );

	//NDebugOverlay::Box( tr.startpos, testHullMins, GetHullMaxs(), 0, 255, 0, true, 0.1f );
	//NDebugOverlay::Box( vecTestPos, testHullMins, GetHullMaxs(), 255, 0, 0, true, 0.1f );

	if ( tr.fraction != 1.0 && HaveSequenceForActivity( ACT_ALIEN_CHARGE_ANTICIPATION ) )
	{
		// Start playing the hit animation
		GetOuter()->AddGesture( ACT_ALIEN_CHARGE_ANTICIPATION );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles the guard charging into something. Returns true if it hit the world.
//-----------------------------------------------------------------------------
bool CAI_ASW_ChargeBehavior::HandleChargeImpact( Vector vecImpact, CBaseEntity *pEntity )
{
	// Cause a shock wave from this point which will disrupt nearby physics objects
	//ImpactShock( vecImpact, 128, 350 );

	// Did we hit anything interesting?
	if ( !pEntity || pEntity->IsWorld() )
	{
		// Robin: Due to some of the finicky details in the motor, the guard will hit
		//		  the world when it is blocked by our enemy when trying to step up 
		//		  during a moveprobe. To get around this, we see if the enemy's within
		//		  a volume in front of the guard when we hit the world, and if he is,
		//		  we hit him anyway.
		EnemyIsRightInFrontOfMe( &pEntity );

		// Did we manage to find him? If not, increment our charge miss count and abort.
		if ( pEntity->IsWorld() )
		{
			m_iChargeMisses++;
			return true;
		}
	}

	// Hit anything we don't like
	if ( GetOuter()->IRelationType( pEntity ) == D_HT && ( GetOuter()->GetNextAttack() < gpGlobals->curtime ) )
	{
		GetOuter()->EmitSound( "NPC_AntlionGuard.Shove" );

		if ( !GetOuter()->IsPlayingGesture( ACT_ALIEN_CHARGE_HIT ) && HaveSequenceForActivity( ACT_ALIEN_CHARGE_HIT ) )
		{
			GetOuter()->RestartGesture( ACT_ALIEN_CHARGE_HIT );
		}

		ChargeDamage( pEntity );

		Vector vecImpulse = ( GetOuter()->BodyDirection2D() * 400 ) + Vector( 0, 0, 200 );
		CASW_Marine *pMarine = CASW_Marine::AsMarine( pEntity );
		if ( pMarine )
		{
			pMarine->Knockdown( GetOuter(), vecImpulse );
		}
		else
		{
			pEntity->ApplyAbsVelocityImpulse( vecImpulse );
		}

		if ( !pEntity->IsAlive() && GetEnemy() == pEntity )
		{
			GetOuter()->SetEnemy( NULL );
		}

		GetOuter()->SetNextAttack( gpGlobals->curtime + 2.0f );
		if ( HaveSequenceForActivity( ACT_ALIEN_CHARGE_STOP ) )
		{
			SetActivity( ACT_ALIEN_CHARGE_STOP );
		}

		// We've hit something, so clear our miss count
		m_iChargeMisses = 0;
		return false;
	}

	// Hit something we don't hate. If it's not moveable, crash into it.
	if ( pEntity->GetMoveType() == MOVETYPE_NONE || pEntity->GetMoveType() == MOVETYPE_PUSH )
		return true;

	// If it's a vphysics object that's too heavy, crash into it too.
	if ( pEntity->GetMoveType() == MOVETYPE_VPHYSICS )
	{
		IPhysicsObject *pPhysics = pEntity->VPhysicsGetObject();
		if ( pPhysics )
		{
			if ( (!pPhysics->IsMoveable() || pPhysics->GetMass() > GetMass() * 0.5f ) )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CAI_ASW_ChargeBehavior::ChargeSteer( void )
{
	trace_t	tr;
	Vector	testPos, steer, forward, right;
	QAngle	angles;
	const float	testLength = GetOuter()->m_flGroundSpeed * 0.15f;

	//Get our facing
	GetOuter()->GetVectors( &forward, &right, NULL );

	steer = forward;

	const float faceYaw	= UTIL_VecToYaw( forward );

	//Offset right
	VectorAngles( forward, angles );
	angles[YAW] += 45.0f;
	AngleVectors( angles, &forward );

	//Probe out
	testPos = GetAbsOrigin() + ( forward * testLength );

	//Offset by step height
	Vector testHullMins = GetHullMins();
	testHullMins.z += (GetOuter()->StepHeight() * 2);

	//Probe
	TraceHull_Charge( GetAbsOrigin(), testPos, testHullMins, GetHullMaxs(), MASK_SOLID_BRUSHONLY, GetOuter(), COLLISION_GROUP_NONE, &tr, GetMass() * 0.5f );

	//Debug info
	if ( asw_debug_charging.GetInt() == 1 )
	{
		if ( tr.fraction == 1.0f )
		{
			NDebugOverlay::BoxDirection( GetAbsOrigin(), testHullMins, GetHullMaxs() + Vector(testLength,0,0), forward, 0, 255, 0, 8, 0.1f );
		}
		else
		{
			NDebugOverlay::BoxDirection( GetAbsOrigin(), testHullMins, GetHullMaxs() + Vector(testLength,0,0), forward, 255, 0, 0, 8, 0.1f );
		}
	}

	//Add in this component
	steer += ( right * 0.5f ) * ( 1.0f - tr.fraction );

	//Offset left
	angles[YAW] -= 90.0f;
	AngleVectors( angles, &forward );

	//Probe out
	testPos = GetAbsOrigin() + ( forward * testLength );

	// Probe
	TraceHull_Charge( GetAbsOrigin(), testPos, testHullMins, GetHullMaxs(), MASK_SOLID_BRUSHONLY, GetOuter(), COLLISION_GROUP_NONE, &tr, GetMass() * 0.5f );

	//Debug
	if ( asw_debug_charging.GetInt() == 1 )
	{
		if ( tr.fraction == 1.0f )
		{
			NDebugOverlay::BoxDirection( GetAbsOrigin(), testHullMins, GetHullMaxs() + Vector(testLength,0,0), forward, 0, 255, 0, 8, 0.1f );
		}
		else
		{
			NDebugOverlay::BoxDirection( GetAbsOrigin(), testHullMins, GetHullMaxs() + Vector(testLength,0,0), forward, 255, 0, 0, 8, 0.1f );
		}
	}

	//Add in this component
	steer -= ( right * 0.5f ) * ( 1.0f - tr.fraction );

	//Debug
	if ( asw_debug_charging.GetInt() == 1 )
	{
		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + ( steer * 512.0f ), 255, 255, 0, true, 0.1f );
		NDebugOverlay::Cross3D( GetAbsOrigin() + ( steer * 512.0f ), Vector(2,2,2), -Vector(2,2,2), 255, 255, 0, true, 0.1f );

		NDebugOverlay::Line( GetAbsOrigin(), GetAbsOrigin() + ( GetOuter()->BodyDirection3D() * 256.0f ), 255, 0, 255, true, 0.1f );
		NDebugOverlay::Cross3D( GetAbsOrigin() + ( GetOuter()->BodyDirection3D() * 256.0f ), Vector(2,2,2), -Vector(2,2,2), 255, 0, 255, true, 0.1f );
	}

	return UTIL_AngleDiff( UTIL_VecToYaw( steer ), faceYaw );
}

float CAI_ASW_ChargeBehavior::GetMass()
{
	if ( GetOuter()->VPhysicsGetObject() )
	{
		return GetOuter()->VPhysicsGetObject()->GetMass();
	}
	return 100.0f;
}

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CAI_ASW_ChargeBehavior::OnStartSchedule( int scheduleType )
{
	// Only go into this schedule/state once and make.
	if ( scheduleType == SCHED_CHARGE_DO )
	{
		SetBehaviorParam( m_FinishedChargeParm, 0 );
	}
	BaseClass::OnStartSchedule( scheduleType );
}


AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_ChargeBehavior )

	DECLARE_TASK( TASK_CHARGE_MOVE )

	DECLARE_ACTIVITY( ACT_ALIEN_CHARGE_START )
	DECLARE_ACTIVITY( ACT_ALIEN_CHARGE_STOP )
	DECLARE_ACTIVITY( ACT_ALIEN_CHARGE_CRASH )
	DECLARE_ACTIVITY( ACT_ALIEN_CHARGE_RUN )
	DECLARE_ACTIVITY( ACT_ALIEN_CHARGE_ANTICIPATION )
	DECLARE_ACTIVITY( ACT_ALIEN_CHARGE_HIT )

	DEFINE_SCHEDULE
	(
		SCHED_CHARGE_DO,
		"	Tasks"
		"		TASK_STOP_MOVING					0"
		"		TASK_FACE_ENEMY						0"
		"		TASK_CHARGE_MOVE			0"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
	);

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
