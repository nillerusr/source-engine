//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_mortar.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"
#include "asw_player.h"
#include "particle_parse.h"
#include "particles/particles.h"
#include "asw_mortar_round.h"
#include "movevars_shared.h"
#include "te_effect_dispatch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_MortarBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_MortarBehavior );


extern ConVar asw_mortar_round_gravity;


//#define DRAW_DEBUG	1


//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_MortarBehavior::CAI_ASW_MortarBehavior( )
{
	m_flMinFiringDistance = 200.0f;
	m_flMaxFiringDistance = 800.0f;
	m_flMaxFiringDistanceSq = m_flMaxFiringDistance * m_flMaxFiringDistance;;

	m_flDamageRadius = 100.0f;
	m_flDamageRadiusSq = m_flDamageRadius * m_flDamageRadius;

	m_flAccuracy = 150.0f;

	m_flProjectileVelocity = 300.0f;

	m_flDamageAmount = 10.0f;

	m_flFireRate = 2.0;

	m_flBlockedAmount = -1.0f;
	m_flDeferUntil = gpGlobals->curtime;
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_MortarBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( V_stricmp( szKeyName, "minRange" ) == 0 )
	{
		m_flMinFiringDistance = atof( szValue );
		return true;
	}

	if ( V_stricmp( szKeyName, "maxRange" ) == 0 )
	{
		m_flMaxFiringDistance = atof( szValue );
		m_flMaxFiringDistanceSq = m_flMaxFiringDistance * m_flMaxFiringDistance;
		return true;
	}

	if ( V_stricmp( szKeyName, "radius" ) == 0 )
	{
		m_flDamageRadius = atof( szValue );
		m_flDamageRadiusSq = m_flDamageRadius * m_flDamageRadius;
		return true;
	}

	if ( V_stricmp( szKeyName, "velocity" ) == 0 )
	{
		m_flProjectileVelocity = atof( szValue );
		return true;
	}

	if ( V_stricmp( szKeyName, "damage" ) == 0 )
	{
		m_flDamageAmount = atof( szValue );
		return true;
	}

	if ( V_stricmp( szKeyName, "rate" ) == 0 )
	{
		m_flFireRate = atof( szValue );
		return true;
	}

	if ( V_stricmp( szKeyName, "accuracy" ) == 0 )
	{
		m_flAccuracy = atof( szValue );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_MortarBehavior::Precache( void )
{
	BaseClass::Precache();

	PrecacheParticleSystem( "impact_mortar_preattack" );
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CAI_ASW_MortarBehavior::Init()
{
	CASW_Alien *pNPC = static_cast<CASW_Alien*>( GetOuter() );
	if ( !pNPC )
		return;

	pNPC->rangeAttack1.Init( m_flMinFiringDistance, m_flMaxFiringDistance, COMBAT_COND_NO_FACING_CHECK, true );
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_MortarBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

	if ( !HasCondition( COND_CAN_RANGE_ATTACK1 ) )
	{
		return false;
	}

	if ( m_flDeferUntil > gpGlobals->curtime )
	{
		return false;
	}

	return BaseClass::CanSelectSchedule();
}


void CAI_ASW_MortarBehavior::HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm )
{
	switch( eEvent )
	{
		case BEHAVIOR_EVENT_MORTAR_FIRE:
			LaunchMortar();
			break;
	}
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_MortarBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_MortarBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_MortarBehavior::GatherCommonConditions( )
{
	BaseClass::GatherCommonConditions();
}


void CAI_ASW_MortarBehavior::BadLocation( )
{
	m_flDeferUntil = gpGlobals->curtime + 1.0f;
	GetOuter()->ClearSchedule( "No target location" );
}


void CAI_ASW_MortarBehavior::FindMortarLocation( )
{
	int nTotalPotential = GetEnemies()->NumEnemies();

	CBaseEntity		**pEligibleEnemies = ( CBaseEntity ** )stackalloc( sizeof( CBaseEntity * ) * nTotalPotential );
	Vector			*vNearPlayerCenter = ( Vector * )stackalloc( sizeof( Vector ) * nTotalPotential );
	int				*nNearPlayerThreat = ( int * )stackalloc( sizeof( int ) * nTotalPotential );
	int				*nNearPlayerCount = ( int * )stackalloc( sizeof( int ) * nTotalPotential );
	int				nEligibleCount = 0;

	AIEnemiesIter_t iter;
	for( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst( &iter ); pEMemory != NULL; pEMemory = GetEnemies()->GetNext( &iter ) )
	{
		CBaseEntity	*pEntity = pEMemory->hEnemy;
		Vector vDelta = GetAbsOrigin() - pEntity->GetAbsOrigin();

		float flLenSq = vDelta.LengthSqr();
		if ( flLenSq <= m_flMaxFiringDistanceSq )
		{
			pEligibleEnemies[ nEligibleCount ] = pEntity;
			vNearPlayerCenter[ nEligibleCount ] = pEntity->GetAbsOrigin();
			nNearPlayerThreat[ nEligibleCount ] = GetOuter()->IRelationPriority( pEntity );
			nNearPlayerCount[ nEligibleCount ] = 1;

			nEligibleCount++;
		}
	}

	if ( nEligibleCount == 0 )
	{
		m_flBlockedAmount = 0.0f;
		BadLocation();
		return;
	}

	for ( int i = 0; i < nEligibleCount; i++ )
	{
		for ( int j = 0; j < nEligibleCount; j++ )
		{
			if ( i == j )
			{
				continue;
			}

			Vector vDelta = pEligibleEnemies[ i ]->GetAbsOrigin() - pEligibleEnemies[ j ]->GetAbsOrigin();

			float flLenSq = vDelta.LengthSqr();
			if ( flLenSq <= m_flDamageRadiusSq * 2.0f )
			{
				vNearPlayerCenter[ i ] += pEligibleEnemies[ j ]->GetAbsOrigin();
				nNearPlayerThreat[ i ] += GetOuter()->IRelationPriority( pEligibleEnemies[ j ] );
				nNearPlayerCount[ i ]++;
			}
		}
	}

	int nBestLocation = 0;
	int nBestThreat = -999;
	for ( int i = 0; i < nEligibleCount; i++ )
	{
		if ( nNearPlayerThreat[ i ] > nBestThreat )
		{
			nBestLocation = i;
			nBestThreat = nNearPlayerThreat[ i ];
		}
	}

	vNearPlayerCenter[ nBestLocation ] /= nNearPlayerCount[ nBestLocation ];
	m_vMortarLocation = vNearPlayerCenter[ nBestLocation ];
	m_vMortarLocation.x += RandomFloat( -m_flAccuracy, m_flAccuracy );
	m_vMortarLocation.y += RandomFloat( -m_flAccuracy, m_flAccuracy );
}


void CAI_ASW_MortarBehavior::CalcVelocity( Vector &vVelocity )
{
	vVelocity = ( m_vMortarLocation - GetAbsOrigin() );
	float	flLength = vVelocity.NormalizeInPlace();
	vVelocity *= m_flProjectileVelocity;

	float flGravity = sv_gravity.GetFloat() * asw_mortar_round_gravity.GetFloat();
	float flTime = flLength / m_flProjectileVelocity;
	flTime /= 2.0f;
	float flVelocity = flGravity * flTime;
	vVelocity.z = flVelocity;
}


void CAI_ASW_MortarBehavior::ValidateMortarLocation( )
{
	trace_t		tr;
	Vector		vStart = GetAbsOrigin() + Vector( 0.0f, 0.0f, 15.0f );

	m_flBlockedAmount = -1.0f;

#ifdef DRAW_DEBUG
	UTIL_AddDebugLine( vStart, m_vMortarLocation + Vector( 0.0f, 0.0f, 15.0f ), true, true );
#endif	// #ifdef DRAW_DEBUG
	UTIL_TraceLine( vStart, m_vMortarLocation + Vector( 0.0f, 0.0f, 15.0f ), MASK_SOLID, GetOuter(), ASW_COLLISION_GROUP_IGNORE_NPCS, &tr );
	if ( tr.fraction >= 0.90f )
	{
		return;
	}

	const int	nSubdivisions = 5;

	Vector vVelocity;
	CalcVelocity( vVelocity );

	float	flGravity = sv_gravity.GetFloat() * asw_mortar_round_gravity.GetFloat();
	Vector	vAcceleration( 0.0f, 0.0f, -flGravity );
	float	flTotalTime = ( m_vMortarLocation - GetAbsOrigin() ).Length() / m_flProjectileVelocity;

	float	flTime = 0.0f;
	float	flTimeStep = ( flTotalTime / ( ( float ) ( nSubdivisions - 1 ) ) );
	Vector	vLast;

	m_flBlockedAmount = 0.0f;

	for( int i = 0; i < nSubdivisions; i++ )
	{
		Vector vFinal = vStart + ( vVelocity * flTime ) + ( 0.5f * flTime * flTime * vAcceleration );
		flTime += flTimeStep;

		if ( i > 0 )
		{
			m_flBlockedAmount += flTimeStep;

#ifdef DRAW_DEBUG
			UTIL_AddDebugLine( vLast, vFinal, true, true );
#endif	// #ifdef DRAW_DEBUG
			UTIL_TraceLine( vLast, vFinal, MASK_SOLID, GetOuter(), ASW_COLLISION_GROUP_IGNORE_NPCS, &tr );
			if ( ( i == ( nSubdivisions - 1 ) && tr.fraction < 0.90 ) || ( i < ( nSubdivisions - 1 ) && tr.fraction != 1.0f ) )
			{
				BadLocation();
				return;
			}

		}
		vLast = vFinal;
	}

	m_flBlockedAmount = -1.0f;
}


void CAI_ASW_MortarBehavior::LaunchMortar( )
{
#if 0
	trace_t		tr;

	UTIL_TraceLine( m_vMortarLocation + Vector( 0.0f, 0.0f, 25.0f ), m_vMortarLocation - Vector( 0.0f, 0.0f, 100.0f ), MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );

	if ( tr.allsolid == false )
	{
		UTIL_DecalTrace( &tr, "PaintSplatPink" );
	}
#endif
	DispatchParticleEffect( "impact_mortar_preattack", m_vMortarLocation, QAngle( 0, 0, 0 ) );

	CEffectData	data;
	data.m_vOrigin = GetAbsOrigin();
	CPASFilter filter( data.m_vOrigin );
	filter.SetIgnorePredictionCull(true);
	DispatchParticleEffect( "mortar_launch", PATTACH_POINT_FOLLOW, GetOuter(), "mouth", false, -1, &filter );

	Vector vVelocity;
	CalcVelocity( vVelocity );

	int iAttachment = GetOuter()->LookupAttachment( "mouth" );		
	Vector vecSpawn = GetAbsOrigin() + Vector( 0.0f, 0.0f, 15.0f );
	QAngle vecAngles = QAngle( 0.0f, 0.0f, 0.0f );
	if ( iAttachment > 0 )
		GetOuter()->GetAttachment( iAttachment, vecSpawn, vecAngles );	
		

	//				CASW_Mortar_Round *pGrenade = 
	CASW_Mortar_Round::Mortar_Round_Create( m_flDamageAmount, m_flDamageRadius, 0, vecSpawn, vecAngles, 
											vVelocity, AngularImpulse( 0.0f, 0.0f, 0.0f ), GetOuter() );
}


//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_MortarBehavior::StartTask( const Task_t *pTask )
{

	switch( pTask->iTask )
	{
		case TASK_MORTAR_FIND_MORTAR_LOCATION:
			{
				FindMortarLocation();
				ValidateMortarLocation();
				break;
			}

		case TASK_MORTAR_PREPARE_TO_FIRE:
			{
				GetOuter()->SetIdealActivity( ACT_PREP_TO_FIRE );
				break;
			}

		case TASK_MORTAR_FIRE:
			{
				GetOuter()->SetIdealActivity( ACT_FIRE );
//				LaunchMortar();
				break;
			}

		case TASK_MORTAR_FIRE_RECOVER:
			{
				GetOuter()->SetIdealActivity( ACT_FIRE_RECOVER );
				break;
			}

		default:
			BaseClass::StartTask( pTask );
			break;
	}
}


//------------------------------------------------------------------------------
// Purpose: routine called every frame when a task is running
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_MortarBehavior::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_MORTAR_FIND_MORTAR_LOCATION:
			{
				TaskComplete();
				m_flDeferUntil = gpGlobals->curtime + m_flFireRate;
				break;
			}

		case TASK_MORTAR_PREPARE_TO_FIRE:
			{
				if ( !GetOuter()->FInAimCone( m_vMortarLocation ) )
				{
					GetMotor()->SetIdealYawToTargetAndUpdate( m_vMortarLocation, AI_KEEP_YAW_SPEED );
				}

				if ( GetOuter()->IsActivityFinished() )
				{
					TaskComplete();
				}
				break;
			}

		case TASK_MORTAR_FIRE:
			{
				if ( GetOuter()->IsActivityFinished() )
				{
					TaskComplete();
				}
				break;
			}

		case TASK_MORTAR_FIRE_RECOVER:
			{
				if ( GetOuter()->IsActivityFinished() )
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


//------------------------------------------------------------------------------
// Purpose: routine called to select what schedule we want to run
// Output : returns the schedule id of the schedule we want to run
//------------------------------------------------------------------------------
int CAI_ASW_MortarBehavior::SelectSchedule()
{
	return SCHED_MORTAR_MOVE_TO_FIRE_SPOT;
}


AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_MortarBehavior )

	DECLARE_TASK( TASK_MORTAR_FIND_MORTAR_LOCATION )
	DECLARE_TASK( TASK_MORTAR_PREPARE_TO_FIRE )
	DECLARE_TASK( TASK_MORTAR_FIRE )
	DECLARE_TASK( TASK_MORTAR_FIRE_RECOVER )

	DEFINE_SCHEDULE
	(
		SCHED_MORTAR_MOVE_TO_FIRE_SPOT,
		"	Tasks"
		"		TASK_MORTAR_FIND_MORTAR_LOCATION		0"
		"		TASK_MORTAR_PREPARE_TO_FIRE				0"
		"		TASK_MORTAR_FIRE						0"
		"		TASK_MORTAR_FIRE_RECOVER				0"
//		"		TASK_WAIT								0.5"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
	);

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
