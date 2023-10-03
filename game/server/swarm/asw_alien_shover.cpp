// Our Swarm Drone - the basic angry fighting alien

#include "cbase.h"
#include "asw_alien_shover.h"

#include "ai_hint.h"
#include "ai_memory.h"
#include "ai_moveprobe.h"
#include "npcevent.h"
#include "IEffects.h"
#include "ndebugoverlay.h"
#include "soundent.h"
#include "soundenvelope.h"
#include "ai_squad.h"
#include "ai_network.h"
#include "ai_pathfinder.h"
#include "ai_navigator.h"
#include "ai_senses.h"
#include "ai_blended_movement.h"
#include "physics_prop_ragdoll.h"
#include "iservervehicle.h"
#include "player_pickup.h"
#include "props.h"
#include "antlion_dust.h"
#include "decals.h"
#include "prop_combine_ball.h"
#include "eventqueue.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ALIEN_SHOVER_MAX_OBJECTS				128
#define	ALIEN_SHOVER_MIN_OBJECT_MASS			8
#define ALIEN_SHOVER_OBJECTFINDING_FOV			0.7
// stats for shove attack
#define	ALIEN_SHOVER_MELEE1_RANGE		156.0f
#define	ALIEN_SHOVER_MELEE1_CONE		0.7f
#define ALIEN_SWAT_DELAY 5.0f
#define SHOVER_PHYSICS_SEARCH_DEPTH 100
#define SHOVER_PLAYER_MAX_SWAT_DIST 1000
#define SHOVER_FARTHEST_PHYSICS_OBJECT 40.0*12.0
#define SHOVER_PHYSOBJ_SWATDIST 100

ConVar asw_debug_aliens("asw_debug_aliens", "0", FCVAR_CHEAT);
ConVar asw_alien_shover_speed("asw_alien_shover_speed", "50", FCVAR_CHEAT, "Speed of a 75kg object thrown by an alien");

Activity ACT_ALIEN_SHOVER_SHOVE_PHYSOBJECT;
Activity ACT_ALIEN_SHOVER_ROAR;

// Anim events
int AE_ALIEN_SHOVER_SHOVE_PHYSOBJECT;
int AE_ALIEN_SHOVER_SHOVE;
int AE_ALIEN_SHOVER_ROAR;


CASW_Alien_Shover::CASW_Alien_Shover( void )// : CASW_Alien()
{
	
}

LINK_ENTITY_TO_CLASS( asw_alien_shover, CASW_Alien_Shover );

BEGIN_DATADESC( CASW_Alien_Shover )
	DEFINE_FIELD( m_hShoveTarget,				FIELD_EHANDLE ),
	DEFINE_FIELD( m_hOldTarget,					FIELD_EHANDLE ),
	DEFINE_FIELD( m_hLastFailedPhysicsTarget,	FIELD_EHANDLE ),
	DEFINE_FIELD( m_hPhysicsTarget,				FIELD_EHANDLE ),
	DEFINE_FIELD( m_vecPhysicsTargetStartPos,	FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecPhysicsHitPosition,		FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flPhysicsCheckTime,			FIELD_TIME ),
	DEFINE_FIELD( m_flNextSwat, FIELD_TIME ),	
	DEFINE_FIELD( m_hObstructor, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bCanRoar, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flNextRoarTime,			FIELD_TIME ),
	
	DEFINE_INPUTFUNC( FIELD_STRING,	"SetShoveTarget", InputSetShoveTarget ),
END_DATADESC()


//==============================================================================================
// ALIEN SHOVER PHYSICS DAMAGE TABLE
//==============================================================================================
static impactentry_t alienShoverLinearTable[] =
{
	{ 100*100,	10 },
	{ 250*250,	25 },
	{ 350*350,	50 },
	{ 500*500,	75 },
	{ 1000*1000,100 },
};

static impactentry_t alienShoverAngularTable[] =
{
	{  50* 50, 10 },
	{ 100*100, 25 },
	{ 150*150, 50 },
	{ 200*200, 75 },
};

impactdamagetable_t gAlienShoverImpactDamageTable =
{
	alienShoverLinearTable,
	alienShoverAngularTable,
	
	ARRAYSIZE(alienShoverLinearTable),
	ARRAYSIZE(alienShoverAngularTable),

	200*200,// minimum linear speed squared
	180*180,// minimum angular speed squared (360 deg/s to cause spin/slice damage)
	15,		// can't take damage from anything under 15kg

	10,		// anything less than 10kg is "small"
	5,		// never take more than 1 pt of damage from anything under 15kg
	128*128,// <15kg objects must go faster than 36 in/s to do damage

	45,		// large mass in kg 
	2,		// large mass scale (anything over 500kg does 4X as much energy to read from damage table)
	1,		// large mass falling scale
	0,		// my min velocity
};

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const impactdamagetable_t
//-----------------------------------------------------------------------------
const impactdamagetable_t &CASW_Alien_Shover::GetPhysicsImpactDamageTable( void )
{
	return gAlienShoverImpactDamageTable;
}

void CASW_Alien_Shover::Spawn()
{
	m_flPhysicsCheckTime = 0;
	m_flNextSwat = gpGlobals->curtime;
	m_hShoveTarget = NULL;
	m_hPhysicsTarget = NULL;
	m_hLastFailedPhysicsTarget = NULL;
	m_bCanRoar = true;
	m_flNextRoarTime = 0;

	PrecacheScriptSound("ASW_Drone.Shove");

	BaseClass::Spawn();
}

void CASW_Alien_Shover::Activate( void )
{
	BaseClass::Activate();

	// Find all nearby physics objects and add them to the list of objects we will sense
	CBaseEntity *pObject = NULL;
	while ( ( pObject = gEntList.FindEntityInSphere( pObject, WorldSpaceCenter(), 2500 ) ) != NULL )
	{
		// Can't throw around debris
		if ( pObject->GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
			continue;

		// We can only throw a few types of things
		if ( !FClassnameIs( pObject, "prop_physics" ) && !FClassnameIs( pObject, "func_physbox" ) )
			continue;

		// Ensure it's mass is within range
		IPhysicsObject *pPhysObj = pObject->VPhysicsGetObject();
		if( ( pPhysObj == NULL ) || ( pPhysObj->GetMass() > GetMaxShoverObjectMass() ) || ( pPhysObj->GetMass() < ALIEN_SHOVER_MIN_OBJECT_MASS ) )
			continue;

		// Tell the AI sensing list that we want to consider this
		g_AI_SensedObjectsManager.AddEntity( pObject );

		if ( asw_debug_aliens.GetInt() == 5 )
		{
			Msg("Alien Shover: Added prop with model '%s' to sense list.\n", STRING(pObject->GetModelName()) );
			pObject->m_debugOverlays |= OVERLAY_BBOX_BIT;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Our enemy is unreachable. Select a schedule.
//-----------------------------------------------------------------------------
int CASW_Alien_Shover::SelectUnreachableSchedule( void )
{
	// Look for a physics objects nearby
	m_hLastFailedPhysicsTarget = NULL;
	UpdatePhysicsTarget( true );
	if ( HasCondition( COND_ALIEN_SHOVER_PHYSICS_TARGET ) )
	{
		//Msg("Alien shoving phys object from selectunreachableschedule\n");
		return SCHED_ALIEN_SHOVER_PHYSICS_ATTACK;
	}

	// Otherwise, roar at the player
	if ( m_bCanRoar && HasCondition(COND_SEE_ENEMY) && m_flNextRoarTime < gpGlobals->curtime )
	{
		m_flNextRoarTime = gpGlobals->curtime + RandomFloat( 20,40 );
		return SCHED_ALIEN_SHOVER_ROAR;
	}

	return -1;
}

int CASW_Alien_Shover::SelectCombatSchedule( void )
{
	//Physics target
	if ( HasCondition( COND_ALIEN_SHOVER_PHYSICS_TARGET ) )
	{
		//Msg("Alien shoving physics object from selectcombatschedule\n");
		return SCHED_ALIEN_SHOVER_PHYSICS_ATTACK;
	}

	// Attack if we can
	if ( HasCondition(COND_CAN_MELEE_ATTACK1) )
		return SCHED_MELEE_ATTACK1;

	// See if we can bash what's in our way, or roar
	if ( HasCondition( COND_ENEMY_UNREACHABLE ) )
	{
		int iUnreach = SelectUnreachableSchedule();
		if (iUnreach != -1)
			return iUnreach;
	}	

	return BaseClass::SelectSchedule();
}

int CASW_Alien_Shover::SelectSchedule( void )
{
	// Debug physics object finding
	if ( 0 ) //asw_debug_aliens.GetInt() == 3 )
	{
		m_flPhysicsCheckTime = 0;
		UpdatePhysicsTarget( false );
		return SCHED_IDLE_STAND;
	}

	// See if we need to clear a path to our enemy
	if ( HasCondition( COND_ENEMY_OCCLUDED ) || HasCondition( COND_ENEMY_UNREACHABLE ) )
	{
		CBaseEntity *pBlocker = GetEnemyOccluder();

		if ( ( pBlocker != NULL ) && FClassnameIs( pBlocker, "prop_physics" ) )
		{
			m_hPhysicsTarget = pBlocker;
			//Msg("Alien shoving physics object from selectschedule as enemy is occluded\n");
			return SCHED_ALIEN_SHOVER_PHYSICS_ATTACK;
		}
	}

	//Only do these in combat states
	if ( m_NPCState == NPC_STATE_COMBAT && GetEnemy() )
		return SelectCombatSchedule();

	return BaseClass::SelectSchedule();
}

void CASW_Alien_Shover::Shove( void )
{
	if ( GetNextAttack() > gpGlobals->curtime )
		return;

	CBaseEntity *pHurt = NULL;
	CBaseEntity *pTarget;

	pTarget = ( m_hShoveTarget == NULL ) ? GetEnemy() : m_hShoveTarget.Get();

	if ( pTarget == NULL )
	{
		m_hShoveTarget = NULL;
		return;
	}

	//Always damage bullseyes if we're scripted to hit them
	if ( pTarget->Classify() == CLASS_BULLSEYE )
	{
		pTarget->TakeDamage( CTakeDamageInfo( this, this, 1.0f, DMG_CRUSH ) );
		m_hShoveTarget = NULL;
		return;
	}

	//float damage = ( pTarget->IsPlayer() ) ? sk_antlionguard_dmg_shove.GetFloat() : 250;
	float damage = 250.0f;

	// If the target's still inside the shove cone, ensure we hit him	
	Vector vecForward, vecEnd;
	AngleVectors( GetAbsAngles(), &vecForward );
  	float flDistSqr = ( pTarget->WorldSpaceCenter() - WorldSpaceCenter() ).LengthSqr();
  	Vector2D v2LOS = ( pTarget->WorldSpaceCenter() - WorldSpaceCenter() ).AsVector2D();
  	Vector2DNormalize(v2LOS);
  	float flDot	= DotProduct2D (v2LOS, vecForward.AsVector2D() );
	if ( flDistSqr < (ALIEN_SHOVER_MELEE1_RANGE*ALIEN_SHOVER_MELEE1_RANGE) && flDot >= ALIEN_SHOVER_MELEE1_CONE )
	{
		vecEnd = pTarget->WorldSpaceCenter();
	}
	else
	{
		vecEnd = WorldSpaceCenter() + ( BodyDirection3D() * ALIEN_SHOVER_MELEE1_RANGE );
	}

	// Use the melee trace to ensure we hit everything there
	trace_t tr;
	CTakeDamageInfo	dmgInfo( this, this, damage, DMG_SLASH );
	CTraceFilterMelee traceFilter( this, COLLISION_GROUP_NONE, &dmgInfo, 1.0, true );
	Ray_t ray;
	ray.Init( WorldSpaceCenter(), vecEnd, Vector(-16,-16,-16), Vector(16,16,16) );
	enginetrace->TraceRay( ray, MASK_SHOT_HULL, &traceFilter, &tr );
	pHurt = tr.m_pEnt;

	// Knock things around
	//ImpactShock( tr.endpos, 100.0f, 250.0f );

	if ( pHurt )
	{
		Vector	traceDir = ( tr.endpos - tr.startpos );
		VectorNormalize( traceDir );

		// Generate enough force to make a 75kg guy move away at 600 in/sec
		Vector vecForce = traceDir * ImpulseScale( 75, asw_alien_shover_speed.GetFloat() );
		CTakeDamageInfo info( this, this, vecForce, tr.endpos, damage, DMG_CLUB );
		pHurt->TakeDamage( info );

		m_hShoveTarget = NULL;

		EmitSound( "ASW_Drone.Shove" );

		// If the player, throw him around
		if ( pHurt->IsPlayer() )
		{
			//Punch the view
			pHurt->ViewPunch( QAngle(20,0,-20) );
			
			//Shake the screen
			UTIL_ScreenShake( pHurt->GetAbsOrigin(), 100.0, 1.5, 1.0, 2, SHAKE_START );

			//Red damage indicator
			color32 red = {128,0,0,128};
			UTIL_ScreenFade( pHurt, red, 1.0f, 0.1f, FFADE_IN );

			Vector forward, up;
			AngleVectors( GetLocalAngles(), &forward, NULL, &up );
			pHurt->ApplyAbsVelocityImpulse( forward * 400 + up * 150 );
		}	
		else
		{
			CBaseCombatCharacter *pVictim = ToBaseCombatCharacter( pHurt );
			
			if ( pVictim )
			{
				pVictim->ApplyAbsVelocityImpulse( BodyDirection2D() * 400 + Vector( 0, 0, 250 ) );
			}

			m_hShoveTarget = NULL;
		}
	}
}

void CASW_Alien_Shover::HandleAnimEvent( animevent_t *pEvent )
{
	int nEvent = pEvent->Event();

	if ( nEvent == AE_ALIEN_SHOVER_SHOVE_PHYSOBJECT )
	{
		if ( m_hPhysicsTarget == NULL )
		{
			// Disrupt other objects near us anyway
			ImpactShock( WorldSpaceCenter(), 150, 250.0f );
			return;
		}

		//Setup the throw velocity
		IPhysicsObject *physObj = m_hPhysicsTarget->VPhysicsGetObject();

		Vector	targetDir = ( GetEnemy()->GetAbsOrigin() - m_hPhysicsTarget->WorldSpaceCenter() );
		float	targetDist = VectorNormalize( targetDir );

		// Must still be close enough to our target
		if ( UTIL_DistApprox( WorldSpaceCenter(), m_hPhysicsTarget->WorldSpaceCenter() ) > 300 )
		{
			m_hPhysicsTarget = NULL;
			return;
		}

		if ( targetDist < 512 )
			targetDist = 512;

		if ( targetDist > 1024 )
			targetDist = 1024;

		targetDir *= targetDist * 3 * physObj->GetMass();	//FIXME: Scale by skill
		targetDir[2] += physObj->GetMass() * 350.0f;
		
		//Display dust
		Vector vecRandom = RandomVector( -1, 1);
		VectorNormalize( vecRandom );
		g_pEffects->Dust( m_hPhysicsTarget->WorldSpaceCenter(), vecRandom, 64.0f, 32 );

		// If it's being held by the player, break that bond
		Pickup_ForcePlayerToDropThisObject( m_hPhysicsTarget );

		EmitSound( "ASW_Drone.Shove" );

		//Send it flying
		AngularImpulse angVel( random->RandomFloat(-180, 180), 100, random->RandomFloat(-360, 360) );
		physObj->ApplyForceCenter( targetDir );
		physObj->AddVelocity( NULL, &angVel );
		
		//Clear the state information, we're done
		ClearCondition( COND_ALIEN_SHOVER_PHYSICS_TARGET );
		ClearCondition( COND_ALIEN_SHOVER_PHYSICS_TARGET_INVALID );

		// Disrupt other objects near it
		ImpactShock( m_hPhysicsTarget->WorldSpaceCenter(), 150, 250.0f, m_hPhysicsTarget );

		m_hPhysicsTarget = NULL;
		m_flPhysicsCheckTime = gpGlobals->curtime + ALIEN_SWAT_DELAY;
		return;
	}
	
	if ( nEvent == AE_ALIEN_SHOVER_SHOVE )
	{
		EmitSound("NPC_AntlionGuard.StepLight", pEvent->eventtime );
		Shove();
		return;
	}

	
	if ( nEvent == AE_ALIEN_SHOVER_ROAR )
	{
		EmitSound( "NPC_AntlionGuard.Roar" );
		return;
	}


	BaseClass::HandleAnimEvent( pEvent );
}

void CASW_Alien_Shover::StartTask( const Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_ALIEN_SHOVER_SHOVE_PHYSOBJECT:
		{
			if ( ( m_hPhysicsTarget == NULL ) || ( GetEnemy() == NULL ) )
			{
				TaskFail( "Tried to shove a NULL physics target!\n" );
				break;
			}
			
			//Get the direction and distance to our thrown object
			Vector	dirToTarget = ( m_hPhysicsTarget->WorldSpaceCenter() - WorldSpaceCenter() );
			float	distToTarget = VectorNormalize( dirToTarget );
			dirToTarget.z = 0;

			//Validate it's still close enough to shove
			//FIXME: Real numbers
			if ( distToTarget > 256.0f )
			{
				m_hLastFailedPhysicsTarget = NULL;
				m_hPhysicsTarget = NULL;

				TaskFail( "Shove target moved\n" );
				break;
			}

			//Validate its offset from our facing
			float targetYaw = UTIL_VecToYaw( dirToTarget );
			float offset = UTIL_AngleDiff( targetYaw, UTIL_AngleMod( GetLocalAngles().y ) );

			if ( fabs( offset ) > 55 )
			{
				m_hLastFailedPhysicsTarget = NULL;
				m_hPhysicsTarget = NULL;

				TaskFail( "Shove target off-center\n" );
				break;
			}

			//Blend properly
			//SetPoseParameter( "throw", offset );

			RemoveAllGestures();

			//Start playing the animation
			SetActivity( ACT_ALIEN_SHOVER_SHOVE_PHYSOBJECT );
		}
		break;

	case TASK_ALIEN_SHOVER_FIND_PHYSOBJECT:
		{
			// Force the antlion guard to find a physobject
			m_flPhysicsCheckTime = 0;
			UpdatePhysicsTarget( true, 1024 );
			if ( m_hPhysicsTarget )
			{
				TaskComplete();
			}
			else
			{
				TaskFail( "Failed to find a physobject.\n" );
			}
		}
		break;

	case TASK_ALIEN_SHOVER_GET_PATH_TO_PHYSOBJECT:
		{
			Vector vecGoalPos;
			Vector vecDir;

			if (!m_hPhysicsTarget.IsValid())
			{
				AI_NavGoal_t goal( GetAbsOrigin() );
				GetNavigator()->SetGoal(goal);
				TaskComplete();
				break;
			}

			vecDir = GetLocalOrigin() - m_hPhysicsTarget->GetLocalOrigin();
			VectorNormalize(vecDir);
			vecDir.z = 0;

			AI_NavGoal_t goal( m_hPhysicsTarget->WorldSpaceCenter() );
			goal.pTarget = m_hPhysicsTarget;
			GetNavigator()->SetGoal( goal );

			if ( asw_debug_aliens.GetInt() == 1 )
			{
				NDebugOverlay::Cross3D( vecGoalPos, Vector(8,8,8), -Vector(8,8,8), 0, 255, 0, true, 2.0f );
				NDebugOverlay::Line( vecGoalPos, m_hPhysicsTarget->WorldSpaceCenter(), 0, 255, 0, true, 2.0f );
				NDebugOverlay::Line( m_hPhysicsTarget->WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(), 0, 255, 0, true, 2.0f );
			}

			TaskComplete();

			/*
			if ( ( m_hPhysicsTarget == NULL ) || ( GetEnemy() == NULL ) )
			{
				TaskFail( "Tried to find a path to NULL physics target!\n" );
				break;
			}

			Vector vecGoalPos;
			Vector vecDir;

			vecDir = GetLocalOrigin() - m_hPhysicsTarget->GetLocalOrigin();
			VectorNormalize(vecDir);
			vecDir.z = 0;

			AI_NavGoal_t goal( m_hPhysicsTarget->WorldSpaceCenter() );
			goal.pTarget = m_hPhysicsTarget;
			//GetNavigator()->SetGoal( goal );

			//TaskComplete();
			
			//Vector vecGoalPos = m_vecPhysicsHitPosition; 
			//AI_NavGoal_t goal( GOALTYPE_LOCATION, vecGoalPos, ACT_RUN );

			if ( GetNavigator()->SetGoal( goal ) )
			{
				if ( asw_debug_aliens.GetInt() == 1 )
				{
					NDebugOverlay::Cross3D( vecGoalPos, Vector(8,8,8), -Vector(8,8,8), 0, 255, 0, true, 2.0f );
					NDebugOverlay::Line( vecGoalPos, m_hPhysicsTarget->WorldSpaceCenter(), 0, 255, 0, true, 2.0f );
					NDebugOverlay::Line( m_hPhysicsTarget->WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(), 0, 255, 0, true, 2.0f );
				}

				// Face the enemy
				GetNavigator()->SetArrivalDirection( GetEnemy() );
				TaskComplete();
			}
			else
			{
				if ( asw_debug_aliens.GetInt() == 1 )
				{
					NDebugOverlay::Cross3D( vecGoalPos, Vector(8,8,8), -Vector(8,8,8), 255, 0, 0, true, 2.0f );
					NDebugOverlay::Line( vecGoalPos, m_hPhysicsTarget->WorldSpaceCenter(), 255, 0, 0, true, 2.0f );
					NDebugOverlay::Line( m_hPhysicsTarget->WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(), 255, 0, 0, true, 2.0f );
				}

				m_hLastFailedPhysicsTarget = m_hPhysicsTarget;
				m_hPhysicsTarget = NULL;
				TaskFail( "Unable to navigate to physics attack target!\n" );
				break;
			}
			*/
		}
		break;

	case TASK_ALIEN_SHOVER_OPPORTUNITY_THROW:
		{
			// If we've got some live antlions, look for a physics object to throw
			if ( RandomFloat(0,1) > 0.3 )
			{
				m_hLastFailedPhysicsTarget = NULL;
				UpdatePhysicsTarget( true );
				if ( HasCondition( COND_ALIEN_SHOVER_PHYSICS_TARGET ) )
				{
					//Msg("Alien shoving from opportunity throw\n");
					SetSchedule( SCHED_ALIEN_SHOVER_PHYSICS_ATTACK );
				}
			}

			TaskComplete();
		}
		break;

	default:
		BaseClass::StartTask(pTask);
		break;
	}
}

void CASW_Alien_Shover::RunTask( const Task_t *pTask )
{
	switch (pTask->iTask)
	{
	case TASK_ALIEN_SHOVER_SHOVE_PHYSOBJECT:
	
		if ( IsActivityFinished() )
		{
			TaskComplete();
		}

		break;

	default:
		BaseClass::RunTask(pTask);
		break;
	}
}

void CASW_Alien_Shover::InputSetShoveTarget( inputdata_t &inputdata )
{
	if ( IsAlive() == false )
		return;

	CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, inputdata.value.String(), NULL, inputdata.pActivator, inputdata.pCaller );

	if ( pTarget == NULL )
	{
		Warning( "**Alien Shover %s cannot find shove target %s\n", GetClassname(), inputdata.value.String() );
		m_hShoveTarget = NULL;
		return;
	}

	m_hShoveTarget = pTarget;
}


void CASW_Alien_Shover::UpdatePhysicsTarget( bool bAllowFartherObjects, float flRadius )
{
	if ( GetEnemy() == NULL )
		return;

	/*if ( m_hPhysicsTarget != NULL )
	{
		//Check to see if it's moved too much since we first picked it up
		if ( UTIL_DistApprox( m_hPhysicsTarget->WorldSpaceCenter(), m_vecPhysicsTargetStartPos ) > 256.0f )
		{
			ClearCondition( COND_ALIEN_SHOVER_PHYSICS_TARGET );
			SetCondition( COND_ALIEN_SHOVER_PHYSICS_TARGET_INVALID );
		}
		else
		{
			SetCondition( COND_ALIEN_SHOVER_PHYSICS_TARGET );
			ClearCondition( COND_ALIEN_SHOVER_PHYSICS_TARGET_INVALID );
		}

		if ( asw_debug_aliens.GetInt() == 3 )
		{
			NDebugOverlay::Cross3D( m_hPhysicsTarget->WorldSpaceCenter(), -Vector(32,32,32), Vector(32,32,32), 255, 255, 255, true, 0.1f );
		}

		return;
	}*/

	if ( m_flPhysicsCheckTime <= gpGlobals->curtime )
		FindNearestPhysicsObject(GetMaxShoverObjectMass());	// max ma
	//m_hPhysicsTarget = FindPhysicsObjectTarget( GetEnemy(), flRadius, ALIEN_SHOVER_OBJECTFINDING_FOV, bAllowFartherObjects );
	
	return;

	if ( m_hPhysicsTarget != NULL )
	{
		SetCondition( COND_ALIEN_SHOVER_PHYSICS_TARGET );
		m_vecPhysicsTargetStartPos = m_hPhysicsTarget->WorldSpaceCenter();
		m_hLastFailedPhysicsTarget = NULL;

		// We must steer around this obstacle until we've thrown it, this stops us from
		// shoving it out of the way while we travel there
		m_hPhysicsTarget->SetNavIgnore();
	}

	m_flPhysicsCheckTime = gpGlobals->curtime + 2.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Push and sweep away small mass items
//-----------------------------------------------------------------------------
void CASW_Alien_Shover::SweepPhysicsDebris( void )
{
	CBaseEntity *pList[ALIEN_SHOVER_MAX_OBJECTS];
	CBaseEntity	*pObject;
	IPhysicsObject *pPhysObj;
	Vector vecDelta(128,128,8);
	int	i;

	if ( asw_debug_aliens.GetInt() == 1 )
	{
		NDebugOverlay::Box( GetAbsOrigin(), vecDelta, -vecDelta, 255, 0, 0, true, 0.1f );
	}

	int count = UTIL_EntitiesInBox( pList, ALIEN_SHOVER_MAX_OBJECTS, GetAbsOrigin() - vecDelta, GetAbsOrigin() + vecDelta, 0 );

	for( i = 0, pObject = pList[0]; i < count; i++, pObject = pList[i] )
	{
		if ( pObject == NULL )
			continue;

		// Don't ignore our shoving target
		if ( pObject == m_hPhysicsTarget )
			continue;

		pPhysObj = pObject->VPhysicsGetObject();

		if( pPhysObj == NULL || pPhysObj->GetMass() > GetMaxShoverObjectMass() )
			continue;
	
		if ( FClassnameIs( pObject, "prop_physics" ) == false )
			continue;

		pObject->SetNavIgnore();
	}
}

void CASW_Alien_Shover::PrescheduleThink( void )
{
	BaseClass::PrescheduleThink();

	// Don't do anything after death
	if ( m_NPCState == NPC_STATE_DEAD )
		return;


	// Check our current physics target
	if ( m_hPhysicsTarget != NULL )
	{
		if ( asw_debug_aliens.GetInt() == 1 )
		{
			NDebugOverlay::Cross3D( m_hPhysicsTarget->WorldSpaceCenter(), Vector(15,15,15), -Vector(15,15,15), 0, 255, 0, true, 0.1f );
		}
	}

	// Automatically update our physics target when chasing enemies
	if ( IsCurSchedule( SCHED_CHASE_ENEMY ) )
	{
		UpdatePhysicsTarget( false );
	}
	else if ( !IsCurSchedule( SCHED_ALIEN_SHOVER_PHYSICS_ATTACK ) )
	{
		ClearCondition( COND_ALIEN_SHOVER_PHYSICS_TARGET );
		m_hPhysicsTarget = NULL;
	}

	//SweepPhysicsDebris();		
}

//-----------------------------------------------------------------------------
// Purpose: Return the point at which the alien wants to stand on to knock the physics object at the enemy
//-----------------------------------------------------------------------------
Vector CASW_Alien_Shover::GetPhysicsHitPosition( CBaseEntity *pObject, Vector &vecTrajectory, float &flClearDistance )
{
	Assert( GetEnemy() );

	// Get the trajectory we want to knock the object along
	vecTrajectory = GetEnemy()->WorldSpaceCenter() - pObject->WorldSpaceCenter();
	VectorNormalize( vecTrajectory );
	vecTrajectory.z = 0;

	// Get the distance we want to be from the object when we hit it
	IPhysicsObject *pPhys = pObject->VPhysicsGetObject();
	Vector extent = physcollision->CollideGetExtent( pPhys->GetCollide(), pObject->GetAbsOrigin(), pObject->GetAbsAngles(), -vecTrajectory );
	flClearDistance = ( extent - pObject->WorldSpaceCenter() ).Length() + CollisionProp()->BoundingRadius(); // asw rem + 32.0f;
	return (pObject->WorldSpaceCenter() + ( vecTrajectory * -flClearDistance ));
}


// zombie style search for a phys object
bool CASW_Alien_Shover::FindNearestPhysicsObject( int iMaxMass )
{
	CBaseEntity		*pList[ SHOVER_PHYSICS_SEARCH_DEPTH ];
	CBaseEntity		*pNearest = NULL;
	float			flDist;
	IPhysicsObject	*pPhysObj;
	int				i;
	Vector			vecDirToEnemy;
	Vector			vecDirToObject;

	if ( !GetEnemy() )
	{
		// Can't swat, or no enemy, so no swat.
		m_hPhysicsTarget = NULL;
		return false;
	}

	vecDirToEnemy = GetEnemy()->GetAbsOrigin() - GetAbsOrigin();
	float dist = VectorNormalize(vecDirToEnemy);
	vecDirToEnemy.z = 0;

	if( dist > SHOVER_PLAYER_MAX_SWAT_DIST )
	{
		// Player is too far away. Don't bother 
		// trying to swat anything at them until
		// they are closer.
		return false;
	}

	float flNearestDist = MIN( dist, SHOVER_FARTHEST_PHYSICS_OBJECT * 0.5 );
	Vector vecDelta( flNearestDist, flNearestDist, GetHullHeight() * 2.0 );

	class CShoverSwatEntitiesEnum : public CFlaggedEntitiesEnum
	{
	public:
		CShoverSwatEntitiesEnum( CBaseEntity **pList, int listMax, int iMaxMass )
		 :	CFlaggedEntitiesEnum( pList, listMax, 0 ),
			m_iMaxMass( iMaxMass )
		{
		}

		virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
		{
			CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
			if ( pEntity && 
				 pEntity->VPhysicsGetObject() && 
				 pEntity->VPhysicsGetObject()->GetMass() <= m_iMaxMass && 
				 pEntity->VPhysicsGetObject()->IsAsleep() && 
				 pEntity->VPhysicsGetObject()->IsMoveable() )
			{
				return CFlaggedEntitiesEnum::EnumElement( pHandleEntity );
			}
			return ITERATION_CONTINUE;
		}

		int m_iMaxMass;
	};

	CShoverSwatEntitiesEnum swatEnum( pList, SHOVER_PHYSICS_SEARCH_DEPTH, iMaxMass );

	int count = UTIL_EntitiesInBox( GetAbsOrigin() - vecDelta, GetAbsOrigin() + vecDelta, &swatEnum );

	// magically know where they are
	Vector vecZombieKnees;
	CollisionProp()->NormalizedToWorldSpace( Vector( 0.5f, 0.5f, 0.25f ), &vecZombieKnees );

	for( i = 0 ; i < count ; i++ )
	{
		pPhysObj = pList[ i ]->VPhysicsGetObject();

		Assert( !( !pPhysObj || pPhysObj->GetMass() > iMaxMass || !pPhysObj->IsAsleep() ) );

		Vector center = pList[ i ]->WorldSpaceCenter();
		flDist = UTIL_DistApprox2D( GetAbsOrigin(), center );

		if( flDist >= flNearestDist )
			continue;

		// This object is closer... but is it between the player and the zombie?
		vecDirToObject = pList[ i ]->WorldSpaceCenter() - GetAbsOrigin();
		VectorNormalize(vecDirToObject);
		vecDirToObject.z = 0;

		if( DotProduct( vecDirToEnemy, vecDirToObject ) < 0.8 )
			continue;

		if( flDist >= UTIL_DistApprox2D( center, GetEnemy()->GetAbsOrigin() ) )
			continue;

		// don't swat things where the highest point is under my knees
		// NOTE: This is a rough test; a more exact test is going to occur below
		if ( (center.z + pList[i]->BoundingRadius()) < vecZombieKnees.z )
			continue;

		// don't swat things that are over my head.
		if( center.z > EyePosition().z )
			continue;

		vcollide_t *pCollide = modelinfo->GetVCollide( pList[i]->GetModelIndex() );
		if (!pCollide)
			continue;
		Vector objMins, objMaxs;
		physcollision->CollideGetAABB( &objMins, &objMaxs, pCollide->solids[0], pList[i]->GetAbsOrigin(), pList[i]->GetAbsAngles() );

		if ( objMaxs.z < vecZombieKnees.z )
			continue;

		if ( !FVisible( pList[i] ) )
			continue;

		// Make this the last check, since it makes a string.
		// Don't swat server ragdolls!
		if ( FClassnameIs( pList[ i ], "physics_prop_ragdoll" ) )
			continue;
			
		if ( FClassnameIs( pList[ i ], "prop_ragdoll" ) )
			continue;

		// The object must also be closer to the zombie than it is to the enemy
		pNearest = pList[ i ];
		flNearestDist = flDist;
	}

	m_hPhysicsTarget = pNearest;

	if( m_hPhysicsTarget == NULL )
	{
		return false;
	}
	else
	{
		return true;
	}
}

// antlion style search for a phys object
CBaseEntity *CASW_Alien_Shover::FindPhysicsObjectTarget( CBaseEntity *pTarget, float radius, float targetCone, bool allowFartherObjects )
{
	CBaseEntity	*pNearest = NULL;

	// If we're allowed to look for farther objects, find the nearest object to the guard.
	// Otherwise, find the nearest object to the vector to the target.
	float flNearestDist = -1.0;
	if ( allowFartherObjects )
	{
		flNearestDist = radius;
	}

	Vector vecDirToTarget = pTarget->GetAbsOrigin() - GetAbsOrigin();
	VectorNormalize( vecDirToTarget );
	vecDirToTarget.z = 0;

	bool bDebug = asw_debug_aliens.GetInt() == 3;
	if ( bDebug )
	{
		if ( m_hLastFailedPhysicsTarget )
		{
			NDebugOverlay::Cross3D( m_hLastFailedPhysicsTarget->WorldSpaceCenter(), -Vector(32,32,32), Vector(32,32,32) , 255, 255, 0, true, 1.0f );
		}
	}

	// Traipse through the sensed object list
	AISightIter_t iter;
	CBaseEntity *pObject;
	for ( pObject = GetSenses()->GetFirstSeenEntity( &iter, SEEN_MISC ); pObject; pObject = GetSenses()->GetNextSeenEntity( &iter ) )
	{
		// If we couldn't shove this object last time, don't try again
		if ( pObject == m_hLastFailedPhysicsTarget )
			continue;

		IPhysicsObject *pPhysObj = pObject->VPhysicsGetObject();
		if ( !pPhysObj )
			continue;

		// Ignore motion disabled props
		if ( !pPhysObj->IsMoveable() )
			continue;

		// Ignore physics objects that are too low to really be noticed by the player
		Vector vecAbsMins, vecAbsMaxs;
		pObject->CollisionProp()->WorldSpaceAABB( &vecAbsMins, &vecAbsMaxs );
		if ( fabs(vecAbsMaxs.z - vecAbsMins.z) < 28 )
			continue;

		// Ignore objects moving too fast
		Vector	velocity;
		pPhysObj->GetVelocity( &velocity, NULL );
		if ( velocity.LengthSqr() > (16*16) )
			continue;

		Vector center = pObject->WorldSpaceCenter();
		Vector vecDirToObject = pObject->WorldSpaceCenter() - GetAbsOrigin();
		VectorNormalize( vecDirToObject );
		vecDirToObject.z = 0;
		float flDist = 0;

		if ( !allowFartherObjects )
		{
			// Validate our cone of sight
			if ( DotProduct( vecDirToTarget, vecDirToObject ) < targetCone )
			{
				if ( bDebug )
				{
					NDebugOverlay::Cross3D( center, Vector(15,15,15), -Vector(15,15,15), 255, 0, 255, true, 1.0f );
				}

				continue;
			}

			// Object must be closer than our target
			if ( UTIL_DistApprox2D( GetAbsOrigin(), center ) > UTIL_DistApprox2D( GetAbsOrigin(), GetEnemy()->GetAbsOrigin() ) )
			{
				if ( bDebug )
				{
					NDebugOverlay::Cross3D( center, Vector(15,15,15), -Vector(15,15,15), 0, 255, 255, true, 1.0f );
				}

				continue;
			}		

			// Must be closer to the line towards the target than a previously valid object
			flDist = DotProduct( vecDirToTarget, vecDirToObject );
			if ( flDist < flNearestDist )
			{
				if ( bDebug )
				{
					NDebugOverlay::Cross3D( center, Vector(15,15,15), -Vector(15,15,15), 255, 0, 0, true, 1.0f );
				}

				continue;
			}
		}
		else
		{
			// Must be closer than the nearest phys object
			flDist = UTIL_DistApprox2D( GetAbsOrigin(), center );
			if ( flDist > flNearestDist )
			{
				if ( bDebug )
				{
					NDebugOverlay::Cross3D( center, Vector(15,15,15), -Vector(15,15,15), 255, 0, 0, true, 1.0f );
				}

				continue;
			}
		}

		// Check for a clear shove path (roughly)
		trace_t	tr;
		UTIL_TraceLine( pObject->WorldSpaceCenter(), GetEnemy()->WorldSpaceCenter(), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
		
		// See how close to our target we got
		if ( ( tr.endpos - GetEnemy()->WorldSpaceCenter() ).LengthSqr() > (256*256) )
			continue;

		// Get the position we want to be at to swing at the object
		float flClearDistance;
		Vector vecTrajectory;
		Vector vecHitPosition = GetPhysicsHitPosition( pObject, vecTrajectory, flClearDistance );
		Vector vecObjectPosition = pObject->WorldSpaceCenter();

		if ( bDebug )
		{
			NDebugOverlay::Box( vecObjectPosition, NAI_Hull::Mins( HULL_MEDIUM ), NAI_Hull::Maxs( HULL_MEDIUM ), 255, 0, 0, true, 1.0f );
			NDebugOverlay::Box( vecHitPosition, NAI_Hull::Mins( HULL_MEDIUM ), NAI_Hull::Maxs( HULL_MEDIUM ), 0, 255, 0, true, 1.0f );
		}

		// Can we move to the spot behind the prop?
		UTIL_TraceHull( vecHitPosition, vecHitPosition, WorldAlignMins(), WorldAlignMaxs(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );
		if ( tr.startsolid || tr.allsolid || (tr.m_pEnt && tr.m_pEnt != pObject) )
		{
			if ( bDebug )
			{
				NDebugOverlay::Box( vecHitPosition, WorldAlignMins(), WorldAlignMaxs(), 255, 0, 0, 8, 1.0f );
				NDebugOverlay::Line( vecObjectPosition, vecHitPosition, 255, 0, 0, true, 1.0f );
			}
		
			// Can we get at it at from an angle on the side of it we're on?
			Vector vecUp(0,0,1);
			Vector vecRight;
			CrossProduct( vecUp, vecTrajectory, vecRight );
			
			// Is the guard to the right? or the left?
			Vector vecToGuard = ( WorldSpaceCenter() - vecObjectPosition );
			VectorNormalize( vecToGuard );
			if ( DotProduct( vecRight, vecToGuard ) > 0 )
			{
				vecHitPosition = vecHitPosition + (vecRight * 64) + (vecTrajectory * 64);
			}
			else
			{
				vecHitPosition = vecHitPosition - (vecRight * 64) + (vecTrajectory * 64);
			}

			if ( asw_debug_aliens.GetInt() == 4 )
			{
				NDebugOverlay::Box( vecHitPosition, NAI_Hull::Mins( HULL_MEDIUM ), NAI_Hull::Maxs( HULL_MEDIUM ), 0, 255, 0, true, 1.0f );
				NDebugOverlay::Line( vecObjectPosition, vecHitPosition, 255, 0, 0, true, 1.0f );
			}

			// Now try and move from the side position
			UTIL_TraceHull( vecHitPosition, vecHitPosition, WorldAlignMins(), WorldAlignMaxs(), MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr );
			if ( tr.startsolid || tr.allsolid || (tr.m_pEnt && tr.m_pEnt != pObject) )
			{
				if ( asw_debug_aliens.GetInt() == 4 )
				{
					NDebugOverlay::Box( vecHitPosition, WorldAlignMins(), WorldAlignMaxs(), 255, 0, 0, 8, 1.0f );
					NDebugOverlay::Line( vecObjectPosition, vecHitPosition, 255, 0, 0, true, 1.0f );
				}
				continue;
			}
		}

		// Take this as the best object so far
		pNearest = pObject;
		flNearestDist = flDist;
		m_vecPhysicsHitPosition = vecHitPosition;
		
		if ( bDebug )
		{
			NDebugOverlay::Cross3D( center, Vector(15,15,15), -Vector(15,15,15), 255, 255, 0, true, 0.5f );
		}
	}

	if ( pNearest && bDebug )
	{
		NDebugOverlay::Cross3D( pNearest->WorldSpaceCenter(), Vector(30,30,30), -Vector(30,30,30), 255, 255, 255, true, 0.5f );
	}

	return pNearest;
}

void CASW_Alien_Shover::ImpactShock( const Vector &origin, float radius, float magnitude, CBaseEntity *pIgnored )
{
	// Also do a local physics explosion to push objects away
	float	adjustedDamage, flDist;
	Vector	vecSpot;
	float	falloff = 1.0f / 2.5f;

	CBaseEntity *pEntity = NULL;

	// Find anything within our radius
	while ( ( pEntity = gEntList.FindEntityInSphere( pEntity, origin, radius ) ) != NULL )
	{
		// Don't affect the ignored target
		if ( pEntity == pIgnored )
			continue;
		if ( pEntity == this )
			continue;

		// UNDONE: Ask the object if it should get force if it's not MOVETYPE_VPHYSICS?
		if ( pEntity->GetMoveType() == MOVETYPE_VPHYSICS || ( pEntity->VPhysicsGetObject() && pEntity->IsPlayer() == false ) )
		{
			vecSpot = pEntity->BodyTarget( GetAbsOrigin() );
			
			// decrease damage for an ent that's farther from the bomb.
			flDist = ( GetAbsOrigin() - vecSpot ).Length();

			if ( radius == 0 || flDist <= radius )
			{
				adjustedDamage = flDist * falloff;
				adjustedDamage = magnitude - adjustedDamage;
		
				if ( adjustedDamage < 1 )
				{
					adjustedDamage = 1;
				}

				CTakeDamageInfo info( this, this, adjustedDamage, DMG_BLAST );
				CalculateExplosiveDamageForce( &info, (vecSpot - GetAbsOrigin()), GetAbsOrigin() );

				pEntity->VPhysicsTakeDamage( info );
			}
		}
	}
}

int CASW_Alien_Shover::TranslateSchedule( int scheduleType )
{
	switch( scheduleType )
	{
	case SCHED_CHASE_ENEMY:
		{
			return SCHED_SHOVER_CHASE_ENEMY;
		}
		break;
	case SCHED_CHASE_ENEMY_FAILED:
		{
			int baseType = BaseClass::TranslateSchedule(scheduleType);
			if ( baseType != SCHED_CHASE_ENEMY_FAILED )
				return baseType;

			int iUnreach = SelectUnreachableSchedule();
			if (iUnreach != -1)
				return iUnreach;
		}
		break;
	case SCHED_ALIEN_SHOVER_PHYSICS_ATTACK:
		// If the object is far away, move and swat it. If it's close, just swat it.
		if (random->RandomFloat() < 0.5f)
		{
			if( DistToPhysicsEnt() > SHOVER_PHYSOBJ_SWATDIST )
			{
				return SCHED_ALIEN_SHOVER_PHYSICS_ATTACK_MOVE;
			}
			else
			{
				return SCHED_ALIEN_SHOVER_PHYSICS_ATTACK;
			}
		}
		else
		{
			if( DistToPhysicsEnt() > SHOVER_PHYSOBJ_SWATDIST )
			{
				return SCHED_ALIEN_SHOVER_PHYSICS_ATTACKITEM_MOVE;
			}
			else
			{
				return SCHED_ALIEN_ATTACKITEM;
			}
			
		}
		break;
	}

	return BaseClass::TranslateSchedule( scheduleType );
}

void CASW_Alien_Shover::GatherConditions( void )
{
	BaseClass::GatherConditions();

	if( m_NPCState == NPC_STATE_COMBAT )
	{
		UpdatePhysicsTarget( false );	
	}

	if( (m_hPhysicsTarget != NULL) && gpGlobals->curtime >= m_flNextSwat && HasCondition( COND_SEE_ENEMY ) 
		&& m_AlienOrders == AOT_None)	// don't try and throw physics objects if we have specific alien orders to follow
	{
		SetCondition( COND_ALIEN_SHOVER_PHYSICS_TARGET );
	}
	else
	{
		ClearCondition( COND_ALIEN_SHOVER_PHYSICS_TARGET );
	}
}

int	CASW_Alien_Shover::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{
	// If we can swat physics objects, see if we can swat our obstructor
	if ( IsPathTaskFailure( taskFailCode ) && 
			m_hObstructor != NULL && m_hObstructor->VPhysicsGetObject() && 
			m_hObstructor->VPhysicsGetObject()->GetMass() < 100 )
	{
		m_hPhysicsTarget = m_hObstructor;
		m_hObstructor = NULL;
		//Msg("Alien shoving phys object from fail schedule\n");
		return SCHED_ALIEN_ATTACKITEM;
	}

	m_hObstructor = NULL;

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

bool CASW_Alien_Shover::OnInsufficientStopDist( AILocalMoveGoal_t *pMoveGoal, float distClear, AIMoveResult_t *pResult )
{
	if ( pMoveGoal->directTrace.fStatus == AIMR_BLOCKED_ENTITY && gpGlobals->curtime >= m_flNextSwat )
	{
		//Msg("Alien setting obstructor\n");
		m_hObstructor = pMoveGoal->directTrace.pObstruction;
	}
	
	return false;
}

float CASW_Alien_Shover::DistToPhysicsEnt( void )
{
	//return ( GetLocalOrigin() - m_hPhysicsEnt->GetLocalOrigin() ).Length();
	if ( m_hPhysicsTarget != NULL )
		return UTIL_DistApprox2D( GetAbsOrigin(), m_hPhysicsTarget->WorldSpaceCenter() );
	return SHOVER_PHYSOBJ_SWATDIST + 1;
}

AI_BEGIN_CUSTOM_NPC( npc_alien_shover, CASW_Alien_Shover )

	//Tasks
	DECLARE_TASK( TASK_ALIEN_SHOVER_GET_PATH_TO_PHYSOBJECT )
	DECLARE_TASK( TASK_ALIEN_SHOVER_SHOVE_PHYSOBJECT )
	DECLARE_TASK( TASK_ALIEN_SHOVER_OPPORTUNITY_THROW )
	DECLARE_TASK( TASK_ALIEN_SHOVER_FIND_PHYSOBJECT )

	//Activities	
	DECLARE_ACTIVITY( ACT_ALIEN_SHOVER_SHOVE_PHYSOBJECT )	
	DECLARE_ACTIVITY( ACT_ALIEN_SHOVER_ROAR )		
	
	//Adrian: events go here	
	DECLARE_ANIMEVENT( AE_ALIEN_SHOVER_SHOVE_PHYSOBJECT )
	DECLARE_ANIMEVENT( AE_ALIEN_SHOVER_SHOVE )
	DECLARE_ANIMEVENT( AE_ALIEN_SHOVER_ROAR )

	DECLARE_CONDITION( COND_ALIEN_SHOVER_PHYSICS_TARGET )
	DECLARE_CONDITION( COND_ALIEN_SHOVER_PHYSICS_TARGET_INVALID )	

	DEFINE_SCHEDULE
	(
		SCHED_ALIEN_ATTACKITEM,

		"	Tasks"
		"		TASK_FACE_ENEMY					0"
		"		TASK_MELEE_ATTACK1				0"
		"	"
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
	)

	DEFINE_SCHEDULE
	( 
		SCHED_ALIEN_SHOVER_PHYSICS_ATTACKITEM_MOVE,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE						SCHEDULE:SCHED_CHASE_ENEMY"
		"		TASK_ALIEN_SHOVER_GET_PATH_TO_PHYSOBJECT	0"
		"		TASK_RUN_PATH								0"
		"		TASK_WAIT_FOR_MOVEMENT						0"
		"		TASK_FACE_ENEMY								0"
		"		TASK_MELEE_ATTACK1			0"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
		"		COND_ENEMY_DEAD"
		"		COND_LOST_ENEMY"
		"		COND_ALIEN_SHOVER_PHYSICS_TARGET_INVALID"
	)	
	
	//==================================================
	// SCHED_ALIEN_SHOVER_PHYSICS_ATTACK
	//==================================================

	DEFINE_SCHEDULE
	( 
		SCHED_ALIEN_SHOVER_PHYSICS_ATTACK_MOVE,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE						SCHEDULE:SCHED_CHASE_ENEMY"
		"		TASK_ALIEN_SHOVER_GET_PATH_TO_PHYSOBJECT	0"
		"		TASK_RUN_PATH								0"
		"		TASK_WAIT_FOR_MOVEMENT						0"
		"		TASK_FACE_ENEMY								0"
		"		TASK_ALIEN_SHOVER_SHOVE_PHYSOBJECT			0"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
		"		COND_ENEMY_DEAD"
		"		COND_LOST_ENEMY"
		"		COND_ALIEN_SHOVER_PHYSICS_TARGET_INVALID"
	)	

	DEFINE_SCHEDULE
	( 
		SCHED_ALIEN_SHOVER_PHYSICS_ATTACK,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE						SCHEDULE:SCHED_CHASE_ENEMY"
		"		TASK_FACE_ENEMY								0"
		"		TASK_ALIEN_SHOVER_SHOVE_PHYSOBJECT			0"
		""
		"	Interrupts"
		"		COND_ENEMY_DEAD"
		"		COND_LOST_ENEMY"
	)	

	//==================================================
	// SCHED_FORCE_ALIEN_SHOVER_PHYSICS_ATTACK
	//==================================================

	DEFINE_SCHEDULE
	( 
		SCHED_FORCE_ALIEN_SHOVER_PHYSICS_ATTACK,

		"	Tasks"
		"		TASK_SET_FAIL_SCHEDULE						SCHEDULE:SCHED_ALIEN_SHOVER_CANT_ATTACK"
		"		TASK_ALIEN_SHOVER_FIND_PHYSOBJECT			0"
		"		TASK_SET_SCHEDULE							SCHEDULE:SCHED_ALIEN_SHOVER_PHYSICS_ATTACK"
		""
		"	Interrupts"
	)		

	//==================================================
	// SCHED_ANTLIONGUARD_ROAR
	//==================================================

	DEFINE_SCHEDULE
	(
		SCHED_ALIEN_SHOVER_ROAR,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_ENEMY				0"
		"		TASK_PLAY_SEQUENCE			ACTIVITY:ACT_ALIEN_SHOVER_ROAR"
		"	"
		"	Interrupts"
	)

	DEFINE_SCHEDULE
	( 
		SCHED_ALIEN_SHOVER_CANT_ATTACK,

		"	Tasks"
		"		TASK_SET_ROUTE_SEARCH_TIME		5"	// Spend 5 seconds trying to build a path if stuck
		"		TASK_GET_PATH_TO_RANDOM_NODE	200"
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_WAIT_PVS					0"
		""
		"	Interrupts"
		"		COND_GIVE_WAY"
		"		COND_NEW_ENEMY"
	)

	DEFINE_SCHEDULE
	(
		SCHED_SHOVER_CHASE_ENEMY,

		"	Tasks"
		"		 TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY_FAILED"
		//"		 TASK_SET_TOLERANCE_DISTANCE	24"
		"		 TASK_GET_CHASE_PATH_TO_ENEMY	300"
		"		 TASK_RUN_PATH					0"
		"		 TASK_WAIT_FOR_MOVEMENT			0"
		//"		 TASK_FACE_ENEMY				0"
		"	"
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_ENEMY_DEAD"
		"		COND_ENEMY_UNREACHABLE"
		"		COND_CAN_RANGE_ATTACK1"
		"		COND_CAN_MELEE_ATTACK1"
		"		COND_CAN_RANGE_ATTACK2"
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_TOO_CLOSE_TO_ATTACK"
		"		COND_TASK_FAILED"
		"		COND_ALIEN_SHOVER_PHYSICS_TARGET"
	)

AI_END_CUSTOM_NPC()