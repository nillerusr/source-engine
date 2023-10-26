//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_explode.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"
#include "asw_boomer_blob.h"
#include "particle_parse.h"
#include "asw_gamerules.h"
#include "asw_boomer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_ExplodeBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_ExplodeBehavior );


//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_ExplodeBehavior::CAI_ASW_ExplodeBehavior( )
{
	m_ExplodeType = EXPLODE_TYPE_PUFF_PROJECTILES;
	m_flRange = 100.0f;
	m_nMaxProjectiles = 3;
	m_flMinVelocity = 150.0f;
	m_flMaxVelocity = 300.0f;
	m_AttachName = UTL_INVAL_SYMBOL;
	m_nAttachCount = 0;
	m_flStartBuildup = -1.0f;
	m_flStartExplodeTime = -1.0f;
	m_flMaxBuildupTime = 3.0f;
	m_flBuildupApproachDist = 200.0f;
	m_flDamageAmount = 10.0f;
	m_flDamageRadius = 200.0f;  // NOTE: this gets overridden
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_ExplodeBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( V_stricmp( szKeyName, "type" ) == 0 )
	{
		if ( V_stricmp( szValue, "suicide" ) == 0 )
		{
			m_ExplodeType = EXPLODE_TYPE_SUICIDE;
		}

		return true;
	}
	else if ( V_stricmp( szKeyName, "range" ) == 0 )
	{
		m_flRange = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "min_velocity" ) == 0 )
	{
		m_flMinVelocity = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "max_velocity" ) == 0 )
	{
		m_flMaxVelocity = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "attach_name" ) == 0 )
	{
		m_AttachName = GetSymbol( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "attach_count" ) == 0 )
	{
		m_nAttachCount = atoi( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "max_projectiles" ) == 0 )
	{
		m_nMaxProjectiles = atoi( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "max_buildup_time" ) == 0 )
	{
		m_flMaxBuildupTime = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "damage" ) == 0 )
	{
		m_flDamageAmount = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "radius" ) == 0 )
	{
		m_flDamageRadius = atof( szValue );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_ExplodeBehavior::Precache( void )
{
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CAI_ASW_ExplodeBehavior::Init( )
{
	CASW_Alien *pNPC = static_cast< CASW_Alien * >( GetOuter() );
	if ( !pNPC )
	{
		return;
	}

	if ( pNPC->Classify() != CLASS_ASW_BOOMER )
	{
		Error( "Explode behavior added to non-boomer alien class\n" );
	}

	pNPC->meleeAttack2.Init( 0.0f, m_flRange, COMBAT_COND_NO_FACING_CHECK, true );
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_ExplodeBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

	if ( !HasCondition( COND_CAN_MELEE_ATTACK2 ) )
	{
		return false;
	}

	return BaseClass::CanSelectSchedule();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_ExplodeBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_ExplodeBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_ExplodeBehavior::GatherCommonConditions( )
{
	BaseClass::GatherCommonConditions();
}


void CAI_ASW_ExplodeBehavior::MoveTowardsPlayer( )
{
	CBaseEntity *pEnemy = GetEnemy();
	Activity	IdealActivity = ACT_IDLE;

	if ( pEnemy )
	{
		AI_NavGoal_t goal( GOALTYPE_ENEMY, ACT_RUN, m_flBuildupApproachDist, AIN_LOCAL_SUCCEEED_ON_WITHIN_TOLERANCE, AIN_DEF_TARGET );
		float	flDistSq = ( GetAbsOrigin() - pEnemy->GetAbsOrigin() ).LengthSqr();

		if ( flDistSq > ( m_flBuildupApproachDist * m_flBuildupApproachDist ) && GetNavigator()->SetGoal( goal, AIN_NO_PATH_TASK_FAIL ) )
		{
			IdealActivity = ACT_RUN;
			GetNavigator()->SetMovementActivity( IdealActivity );
		}
	}
	GetOuter()->SetIdealActivity( IdealActivity );
}


//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_ExplodeBehavior::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_EXPLODE_PREPARE_BUILDUP:
			{
				GetOuter()->SetIdealActivity( ACT_PREP_EXPLODE );
				CASW_Boomer *pBoomer = assert_cast<CASW_Boomer*>( GetOuter() );
				pBoomer->SetInflating( true );
				m_flStartExplodeTime = gpGlobals->curtime;
			}
			break;

		case TASK_EXPLODE_BEGIN_BUILDUP:
			{
				MoveTowardsPlayer();
				CASW_Boomer *pBoomer = assert_cast<CASW_Boomer*>( GetOuter() );
				pBoomer->SetInflating( true );
//				GetOuter()->RestartGesture( ACT_EXPLODE, true, false );
				//GetOuter()->AddGesture( ACT_EXPLODE, false );

//				m_iSecondaryLayer = GetOuter()->AddLayeredSequence( GetOuter()->SelectWeightedSequence( ACT_EXPLODE ), 0 );
//				GetOuter()->SetLayerWeight( m_iSecondaryLayer, 0.0 );
//				GetOuter()->SetLayerPlaybackRate( m_iSecondaryLayer, 1.0 );

				m_flStartBuildup = gpGlobals->curtime;
				m_flEndBuildup = m_flStartBuildup + m_flMaxBuildupTime;
			}
			break;
			
		case TASK_EXPLODE_KILL_SELF:
			{
				CTakeDamageInfo		info( GetOuter(), GetOuter(), GetOuter()->GetMaxHealth() * 2, DMG_GENERIC );

				GetOuter()->m_takedamage = DAMAGE_YES;
				GetOuter()->OnTakeDamage( info );

				DoExplosion();
			}
			break;

		case TASK_EXPLODE_SUICIDE:
			{
				CTakeDamageInfo		info( GetOuter(), GetOuter(), GetOuter()->GetMaxHealth() * 2, DMG_GENERIC );

				GetOuter()->m_takedamage = DAMAGE_YES;
				GetOuter()->OnTakeDamage( info );
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
void CAI_ASW_ExplodeBehavior::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_EXPLODE_PREPARE_BUILDUP:
			if ( GetOuter()->IsActivityFinished() )
			{
				TaskComplete();
			}
			break;

		case TASK_EXPLODE_BEGIN_BUILDUP:
			if ( m_flEndBuildup <= gpGlobals->curtime )
			{
				TaskComplete();
			}
			else if ( GetNavigator()->GetGoalType() == GOALTYPE_NONE || !GetNavigator()->IsGoalActive() )
			{
				MoveTowardsPlayer();
			}
			break;

		case TASK_EXPLODE_KILL_SELF:
			TaskComplete();
			break;	

		case TASK_EXPLODE_SUICIDE:
			TaskComplete();
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
int CAI_ASW_ExplodeBehavior::SelectSchedule()
{
	if ( m_ExplodeType == EXPLODE_TYPE_SUICIDE )
	{
		return SCHED_EXPLODE_SUICIDE;
	}
	else
	{
		return SCHED_EXPLODE_PREPARE;
	}
}



//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
void CAI_ASW_ExplodeBehavior::DoExplosion( )
{
	m_ExplodeType = EXPLODE_TYPE_EXPLODED;		// prevent recursion

	Vector	vPosition = GetAbsOrigin();
	int		*pnAvailList = ( int * )stackalloc( sizeof( int ) * m_nAttachCount );
	int		nNumAvail = -1;

	if ( m_flStartExplodeTime < 0.0f )
	{
		return;
	}

	for( int i = 0; i < m_nAttachCount; i++ )
	{
		pnAvailList[ i ] = i + 1;
	}

	int		nPrepSequence = GetOuter()->SelectWeightedSequence( ACT_PREP_EXPLODE );
	float	flPrepTime = GetOuter()->SequenceDuration( nPrepSequence );
	float	flPercent = ( gpGlobals->curtime - m_flStartExplodeTime ) / ( m_flMaxBuildupTime + flPrepTime );
	int		nCount = ( m_nMaxProjectiles + 1 ) * flPercent;

	nCount = clamp( nCount, 1, m_nMaxProjectiles );
	m_flStartExplodeTime = m_flStartBuildup = -1.0f;	// to prevent a double call

	for( int i = 0; i < nCount; i++ )
	{
		Vector	vSpawn;
		QAngle	vAngles;
		Vector	vVelocity;
		//char	temp[ 64 ];	
		int		nPosition;

		if ( nNumAvail < 0 )
		{
			nNumAvail = m_nAttachCount - 1;
		}
		nPosition = RandomInt( 0, nNumAvail );
		int nSelected = pnAvailList[ nPosition ];
		pnAvailList[ nPosition ] = pnAvailList[ nNumAvail ];
		pnAvailList[ nNumAvail ] = nSelected;
		nNumAvail--;

		//V_snprintf( temp, sizeof( temp ), "%s%02d", GetSymbolText( m_AttachName ), nSelected );
		GetOuter()->GetAttachment( "attach_explosion", vSpawn, vAngles );

		Vector vCenter = vSpawn;
		Vector vOffset = RandomVector( -10, 10 );		
		if ( vOffset == vec3_origin )
		{
			vOffset = Vector( 1, 0, 1 );
		}
		vSpawn += vOffset;

		vVelocity = vSpawn - vCenter;
		vVelocity.NormalizeInPlace();
		vVelocity.z = fabs( vVelocity.z );
		//Msg( "Velocity base = %f %f %f\n", VectorExpand( vVelocity ) );
		vVelocity *= RandomFloat( m_flMinVelocity, m_flMaxVelocity );

		vAngles = vec3_angle;
		CASW_Boomer_Blob::Boomer_Blob_Create( m_flDamageAmount, m_flDamageRadius, 0, vSpawn, vAngles, vVelocity, AngularImpulse( 0.0f, 0.0f, 0.0f ), GetOuter() );
	}
}


//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
void CAI_ASW_ExplodeBehavior::DoSuicideExplosion( )
{
	m_ExplodeType = EXPLODE_TYPE_EXPLODED;		// prevent recursion

	// explosion effects
	DispatchParticleEffect( "mortar_explosion", GetAbsOrigin(), Vector( m_flDamageRadius, 0.0f, 0.0f ), QAngle( 0.0f, 0.0f, 0.0f ) );
	GetOuter()->EmitSound( "ASWGrenade.Explode" );

	ASWGameRules()->RadiusDamage( CTakeDamageInfo( GetOuter(), GetOuter(), m_flDamageAmount, DMG_BLAST ), GetAbsOrigin(), m_flDamageRadius, CLASS_NONE, NULL );
}


//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
void CAI_ASW_ExplodeBehavior::HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm )
{
	switch( eEvent )
	{
		case BEHAVIOR_EVENT_EXPLODE:
			switch( m_ExplodeType )
			{
				case EXPLODE_TYPE_SUICIDE:
					DoSuicideExplosion();
					break;

				case EXPLODE_TYPE_PUFF_PROJECTILES:
					DoExplosion();
					break;
			}
			break;
	}
}


AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_ExplodeBehavior )

	DECLARE_TASK( TASK_EXPLODE_KILL_SELF )
	DECLARE_TASK( TASK_EXPLODE_PREPARE_BUILDUP )
	DECLARE_TASK( TASK_EXPLODE_BEGIN_BUILDUP )
	DECLARE_TASK( TASK_EXPLODE_SUICIDE )

	DEFINE_SCHEDULE
	(
		SCHED_EXPLODE_PREPARE,
		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_EXPLODE_PREPARE_BUILDUP	0"
		"		TASK_EXPLODE_BEGIN_BUILDUP		0"
		"		TASK_EXPLODE_KILL_SELF			0"
		""
		"	Interrupts"
	);

	DEFINE_SCHEDULE
	(
		SCHED_EXPLODE_SUICIDE,
		"	Tasks"
		"		TASK_EXPLODE_SUICIDE			0"
		""
		"	Interrupts"
	);

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
