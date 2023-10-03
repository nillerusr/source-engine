//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_flick.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"
#include "movevars_shared.h"
#include "asw_marine.h"
#include "asw_player.h"
#include "asw_director.h"
#include "asw_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_FlickBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_FlickBehavior );


//#define DRAW_DEBUG	1


typedef struct SFlickInfo
{
	Activity	m_Activity;
	float		m_flMinDot;
	float		m_flMaxDot;
} TFlickInfo;

#define MAX_FLICKS	4

static TFlickInfo FlickInfo[ MAX_FLICKS ] =
{
	{ ACT_FLICK_LEFT,			0.45f,	0.8f },
	{ ACT_FLICK_LEFT_MIDDLE,	0.0f,	0.45f },
	{ ACT_FLICK_RIGHT_MIDDLE,	-0.45f,	0.0f },
	{ ACT_FLICK_RIGHT,			-0.8f,	-0.45f }
};


//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_FlickBehavior::CAI_ASW_FlickBehavior( )
{
	m_flDistance = -1;
	m_flDistanceSq = -1;
	m_flMinDamage = 1.0f;
	m_flMaxDamage = 1.0f;
	m_pPickedFlick = NULL;
	m_flNextFlickCheck = 0.0f;
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_FlickBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( V_stricmp( szKeyName, "distance" ) == 0 )
	{
		m_flDistance = atof( szValue );
		m_flDistanceSq = m_flDistance * m_flDistance;
		return true;
	}
	else if ( V_stricmp( szKeyName, "propel_distance" ) == 0 )
	{
		m_flPropelDistance = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "propel_height" ) == 0 )
	{
		m_flPropelHeight = atof( szValue );
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

	return BaseClass::KeyValue( szKeyName, szValue );
}


//------------------------------------------------------------------------------
// Purpose: precaches any additional assets this behavior needs
//------------------------------------------------------------------------------
void CAI_ASW_FlickBehavior::Precache( void )
{
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CAI_ASW_FlickBehavior::Init( )
{
}


//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_FlickBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
	{
		return false;
	}

	if ( HasCondition( COND_SHIELD_CAN_FLICK ) == false )
	{
		return false;
	}

	return BaseClass::CanSelectSchedule();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is active.  this is
//			generally a larger set of conditions to interrupt any tasks.
//------------------------------------------------------------------------------
void CAI_ASW_FlickBehavior::GatherConditions( )
{
	BaseClass::GatherConditions();
}


//------------------------------------------------------------------------------
// Purpose: sets / clears conditions for when the behavior is not active.  this is 
//			mainly to have a smaller set of conditions to wake up the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_FlickBehavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditionsNotActive();

	if ( m_flNextFlickCheck < gpGlobals->curtime )
	{
		if ( GetFlickActivity() != NULL )
		{
			SetCondition( COND_SHIELD_CAN_FLICK );
		}

		m_flNextFlickCheck = gpGlobals->curtime + 1.0f;
	}
}


void CAI_ASW_FlickBehavior::BeginScheduleSelection( )
{
	m_pPickedFlick = NULL;
}


void CAI_ASW_FlickBehavior::EndScheduleSelection( )
{
}


//------------------------------------------------------------------------------
// Purpose: general purpose routine to collect conditions used both during active
//			and non-active states of the behavior.
//------------------------------------------------------------------------------
void CAI_ASW_FlickBehavior::GatherCommonConditions( )
{
	BaseClass::GatherCommonConditions();
}


//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_FlickBehavior::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_SHIELD_FLICK:
			{
				ClearCondition( COND_SHIELD_CAN_FLICK );
				m_pPickedFlick = GetFlickActivity();
				if ( m_pPickedFlick != NULL )
				{
					GetOuter()->RestartGesture( m_pPickedFlick->m_Activity );
				}
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
void CAI_ASW_FlickBehavior::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_SHIELD_FLICK:
			if ( !m_pPickedFlick || GetOuter()->IsPlayingGesture( m_pPickedFlick->m_Activity ) == false )
			{
				TaskComplete();
				m_pPickedFlick = NULL;
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
int CAI_ASW_FlickBehavior::SelectSchedule()
{
	return SCHED_SHIELD_FLICK;
}


TFlickInfo *CAI_ASW_FlickBehavior::GetFlickActivity( )
{
	Vector		vRightNPC, vForwardNPC, vForwardEnemy;
	int			nCount = 0;
	int			nFlickTotal[ MAX_FLICKS ];

	for( int i = 0; i < MAX_FLICKS; i++ )
	{
		nFlickTotal[ i ] = 0;
	}

	AngleVectors( GetAbsAngles(), &vForwardNPC, &vRightNPC, NULL );
#ifdef DRAW_DEBUG
	UTIL_AddDebugLine( GetAbsOrigin(), GetAbsOrigin() + vForwardNPC * 300.0f, true, false );
#endif	// #ifdef DRAW_DEBUG

	AIEnemiesIter_t iter;
	for( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst( &iter ); pEMemory != NULL; pEMemory = GetEnemies()->GetNext( &iter ) )
	{
		CBaseEntity	*pEntity = pEMemory->hEnemy;

		Vector	vDelta = GetAbsOrigin() - pEntity->GetAbsOrigin();
		float	flLenSq = vDelta.LengthSqr();

		if ( flLenSq > m_flDistanceSq )
		{
			continue;
		}

#ifdef DRAW_DEBUG
		UTIL_AddDebugLine( GetAbsOrigin(), pEntity->GetAbsOrigin(), true, false );
#endif	// #ifdef DRAW_DEBUG

		vForwardEnemy = vDelta;
		vForwardEnemy.NormalizeInPlace();

		float flResult = vForwardNPC.Dot( vForwardEnemy );
		if ( flResult > 0.0f )
		{	// we are behind
			continue;
		}

		flResult = vRightNPC.Dot( vForwardEnemy );
		for( int j = 0; j < MAX_FLICKS; j++ )
		{
			if ( flResult >= FlickInfo[ j ].m_flMinDot && flResult <= FlickInfo[ j ].m_flMaxDot )
			{	// we are within the flick angle of this arm
				nFlickTotal[ j ]++;
				nCount++;
				break;
			}
		}
	}

	if ( nCount > 0 )
	{
		int nTotal = 0;

		nCount = RandomInt( 0, nCount - 1 );
		for( int j = 0; j < MAX_FLICKS; j++ )
		{
			nTotal += nFlickTotal[ j ];
			if ( nCount < nTotal )
			{
				return &FlickInfo[ j ];
			}
		}
	}

	return NULL;
}


//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
void CAI_ASW_FlickBehavior::TryFlicking( CBaseEntity *pEntity )
{
	Vector		vRightNPC, vForwardNPC, vForwardEnemy;
	Vector		vDelta = GetAbsOrigin() - pEntity->GetAbsOrigin();
	float		flLenSq = vDelta.LengthSqr();

	if ( flLenSq > m_flDistanceSq )
	{
		return;
	}

	vForwardEnemy = vDelta;
	vForwardEnemy.NormalizeInPlace();
	AngleVectors( GetAbsAngles(), &vForwardNPC, &vRightNPC, NULL );

	float		flResult = vForwardNPC.Dot( vForwardEnemy );
	if ( flResult > 0.0f )
	{	// we are behind
		return;
	}

	flResult = vRightNPC.Dot( vForwardEnemy );
	if ( flResult < m_pPickedFlick->m_flMinDot || flResult > m_pPickedFlick->m_flMaxDot )
	{	// we are not within the flick angle of the flicking arm
		return;
	}

	CASW_Alien	*pOwner = dynamic_cast< CASW_Alien * >( GetOuter() );
	float flMinDamage = ASWGameRules()->ModifyAlienDamageBySkillLevel( m_flMinDamage );
	float flMaxDamage = ASWGameRules()->ModifyAlienDamageBySkillLevel( m_flMaxDamage );

	flMinDamage = ( flMinDamage < 1.0f ? 1.0f : flMinDamage );
	flMaxDamage = ( flMaxDamage < 1.0f ? 1.0f : flMaxDamage );

	CTakeDamageInfo info( pOwner, pOwner, RandomFloat( flMinDamage, flMaxDamage ), DMG_GENERIC );
	Vector	killDir = pEntity->GetAbsOrigin() - GetAbsOrigin();
	VectorNormalize( killDir );
	info.SetDamageForce( killDir );
	info.SetDamagePosition( GetAbsOrigin() );

	float flTime = sqrt( ( 2.0f * m_flPropelHeight / sv_gravity.GetFloat() ) );

	vDelta = -vDelta;
	vDelta.z = 0.0f;
	vDelta.NormalizeInPlace();
	vDelta *= m_flPropelDistance / ( flTime * 2.0f );
	vDelta.z = sv_gravity.GetFloat() * flTime;

	CASW_Player	*pPlayer = dynamic_cast< CASW_Player * >( pEntity );
	if ( pPlayer )
	{
		CASW_Marine	*pBaseMarine = pPlayer->GetMarine();
	//	CASW_Marine	*pBaseMarine = CASW_Marine::AsMarine( pEntity );
		if ( pBaseMarine )
		{
			pBaseMarine->TakeDamage( info );
			pBaseMarine->m_bNoAirControl = true;
			pBaseMarine->SetAbsVelocity( vDelta );
			pBaseMarine->SetGroundEntity( NULL );	
			return;
		}
	}
	else
	{
		pEntity->TakeDamage( info );
	}

	pEntity->SetAbsVelocity( vDelta );
	pEntity->SetGroundEntity( NULL );	
}


//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
void CAI_ASW_FlickBehavior::Flick( )
{
#if 0
	for( int i = 0; i < ASWDirector()->GetNPCListCount(); i++ )
	{
		CASW_Alien	*pAlien = ASWDirector()->GetNPCFromList( i );

		if ( !pAlien || !pAlien->IsAlive() )
		{
			continue;
		}

		if ( GetOuter() != pAlien )
		{
			TryFlicking( pAlien );
		}
	}

	for ( int i = 0; i < ASW_MAX_READY_PLAYERS; i++ )
	{
		// found a connected player?
		CASW_Player *pOtherPlayer = dynamic_cast< CASW_Player * >( UTIL_PlayerByIndex( i + 1 ) );
		// if they're not connected, skip them
		if ( !pOtherPlayer || !pOtherPlayer->IsConnected() )
		{
			continue;
		}

		TryFlicking( pOtherPlayer );
	}
#else
	AIEnemiesIter_t iter;
	for( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst( &iter ); pEMemory != NULL; pEMemory = GetEnemies()->GetNext( &iter ) )
	{
		CBaseEntity	*pEntity = pEMemory->hEnemy;

		if ( pEntity->IsAlive() && GetOuter() != pEntity )
		{
			TryFlicking( pEntity );
		}
	}
#endif
	// turn this into an area of affect
	// reverse his velocity - if he is coming towards the shield bug, add that back into his outgoing to increase it
}


//------------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
void CAI_ASW_FlickBehavior::HandleBehaviorEvent( CBaseEntity *pInflictor, BehaviorEvent_t eEvent, int nParm )
{
	switch( eEvent )
	{
		case BEHAVIOR_EVENT_FLICK:
			Flick();
			break;
	}
}


AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_FlickBehavior )

	DECLARE_TASK( TASK_SHIELD_FLICK )

	DECLARE_CONDITION( COND_SHIELD_CAN_FLICK )

	DEFINE_SCHEDULE
	(
		SCHED_SHIELD_FLICK,
		"	Tasks"
		"		TASK_SHIELD_FLICK				0"
		""
		"	Interrupts"
		"		"
	);

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
