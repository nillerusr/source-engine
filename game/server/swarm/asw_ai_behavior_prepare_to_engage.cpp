//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: NPC moves towards his enemy, but doesn't actually attack until a target slot frees up
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior_prepare_to_engage.h"
#include "ai_hint.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "asw_alien.h"
#include "asw_player.h"
#include "asw_marine.h"
#include "ai_network.h"
#include "ai_networkmanager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_PrepareToEngageBehavior )
END_DATADESC();

LINK_BEHAVIOR_TO_CLASSNAME( CAI_ASW_PrepareToEngageBehavior );


//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_PrepareToEngageBehavior::CAI_ASW_PrepareToEngageBehavior( )
{
	m_flPrepareRadiusMin = 100.0f;
	m_flPrepareRadiusMax = 400.0f;
}


//------------------------------------------------------------------------------
// Purpose: function to set up parameters
// Input  : szKeyName - the name of the key
//			szValue - the value to be set
// Output : returns true of we handled this key
//------------------------------------------------------------------------------
bool CAI_ASW_PrepareToEngageBehavior::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( V_stricmp( szKeyName, "prepare_radius_min" ) == 0 )
	{
		m_flPrepareRadiusMin = atof( szValue );
		return true;
	}

	if ( V_stricmp( szKeyName, "prepare_radius_max" ) == 0 )
	{
		m_flPrepareRadiusMax = atof( szValue );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

//------------------------------------------------------------------------------
// Purpose: determines if we can use this behavior currently
// Output : returns true if this behavior is able to run
//------------------------------------------------------------------------------
bool CAI_ASW_PrepareToEngageBehavior::CanSelectSchedule()
{
	if ( !GetOuter()->IsInterruptable() )
		return false;

	if ( GetEnemy() == NULL )
		return false;

	if ( GetEnemy()->Classify() != CLASS_ASW_MARINE )
		return false;

	// don't select this schedule if we're in a target slot and can actually attack
	//CASW_Marine *pMarine = assert_cast<CASW_Marine*>( GetEnemy() );
	//CASW_Alien *pAlien = assert_cast<CASW_Alien*>( GetOuter() );
	//if ( pMarine->IsInTargetSlot( pAlien, pAlien->GetTargetSlotType() ) )
		//return false;

	// TODO: Do we want to grab a target slot here?  This assumes our attack/chase schedules are higher priority than anything else below us...
	//if ( pMarine->OccupyTargetSlot( pAlien, pAlien->GetTargetSlotType() ) )
		//return false;

	return BaseClass::CanSelectSchedule();
}

//------------------------------------------------------------------------------
// Purpose: routine called to start when a task initially starts
// Input  : pTask - the task structure
//------------------------------------------------------------------------------
void CAI_ASW_PrepareToEngageBehavior::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
		case TASK_PREPARE_TO_ENGAGE:
			{
				CBaseEntity *pEnemy = GetEnemy();

				if ( pEnemy == NULL )
				{
					TaskFail( FAIL_NO_ENEMY );
					return;
				}

				if ( GetPrepareToAttackPath( GetEnemy()->GetAbsOrigin() ) )
				{
					TaskComplete();
				}
				else
				{
					// TODO: wander to a random nearby spot
					TaskFail( FAIL_NO_ROUTE );
				}
				return;
			}

		default:
			BaseClass::StartTask( pTask );
			break;
	}
}

//------------------------------------------------------------------------------
// Purpose: routine called to select what schedule we want to run
// Output : returns the schedule id of the schedule we want to run
//------------------------------------------------------------------------------
int CAI_ASW_PrepareToEngageBehavior::SelectSchedule()
{
	return SCHED_PREPARE_TO_ENGAGE;
}


//-------------------------------------
// Purpose: Find a node near the enemy
//-------------------------------------

bool CAI_ASW_PrepareToEngageBehavior::GetPrepareToAttackPath(const Vector &vecThreat )
{
	if ( !CAI_NetworkManager::NetworksLoaded() )
	{
		DevWarning( 2, "Graph not ready for GetPrepareToAttackPath!\n" );
		return false;
	}

	Vector vecToThreat = vecThreat - GetAbsOrigin();
	float flDistToThreat = vecToThreat.NormalizeInPlace();
/*
	int iMyNode			= GetOuter()->GetPathfinder()->NearestNodeToNPC();
	int iThreatNode		= GetOuter()->GetPathfinder()->NearestNodeToPoint( vecThreat );

	if ( iMyNode == NO_NODE )
	{
		DevWarning( 2, "FindPrepareToAttackNode() - %s has no nearest node!\n", GetEntClassname());
		return false;
	}
	if ( iThreatNode == NO_NODE )
	{
		// DevWarning( 2, "FindBackAwayNode() - Threat has no nearest node!\n" );
		iThreatNode = iMyNode;
		// return false;
	}

	// A vector pointing to the threat.
	Vector vecToThreat;
	vecToThreat = vecThreat - GetLocalOrigin();

	// Get my current distance from the threat
	float flCurDist = VectorNormalize( vecToThreat );

	// find all nodes within the radius of our target
	m_flPrepareRadius;
*/
	int iNumNodes = g_pBigAINet->NumNodes();
	CUtlVector<int> candidateNodes;
	for ( int i = 0; i < iNumNodes; i++ )
	{
		CAI_Node *pNode = g_pBigAINet->GetNode( i );
		if ( !pNode || pNode->GetType() != NODE_GROUND )
			continue;

		Vector vecPos = pNode->GetPosition( GetOuter()->GetHullType() );
		Vector vecDir = vecPos - vecThreat;
		float flDist = vecDir.NormalizeInPlace();
		if ( flDist > m_flPrepareRadiusMax || flDist < m_flPrepareRadiusMin )
			continue;
	
		// Make sure this node doesn't take me past the enemy's position.
		Vector vecToNode = vecPos - GetAbsOrigin();
		float flDistToNode = vecToNode.NormalizeInPlace();

		if( DotProduct( vecToNode, vecToThreat ) > 0.0 && flDistToNode > flDistToThreat )
			continue;

		candidateNodes.AddToTail( i );
	}

	if ( candidateNodes.Count() <= 0 )
		return false;

	int iOffset = RandomInt( 0, candidateNodes.Count() - 1 );
	int iNumCandidateNodes = candidateNodes.Count();
	int iMaxTries = 4;
	for ( int i = 0; i < iNumCandidateNodes && iMaxTries > 0; i++ )
	{
		CAI_Node *pNode = g_pBigAINet->GetNode( candidateNodes[ i + iOffset ] );
		if ( !pNode || pNode->GetType() != NODE_GROUND )
			continue;

		// see if we can reach it
		Vector vecPos = pNode->GetPosition( GetOuter()->GetHullType() );
		AI_NavGoal_t goal( vecPos );

		if ( GetNavigator()->SetGoal( goal, AIN_CLEAR_TARGET ) )
		{
			return true;
		}
		iMaxTries--;
	}
	return false;
}


AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_PrepareToEngageBehavior )

	DECLARE_TASK( TASK_PREPARE_TO_ENGAGE )

	DEFINE_SCHEDULE
	(
		SCHED_PREPARE_TO_ENGAGE,
		"	Tasks"
		"		TASK_PREPARE_TO_ENGAGE		0"
		"		TASK_RUN_PATH				0"
		"		TASK_WAIT_FOR_MOVEMENT		0"
		""
		"	Interrupts"
	);

AI_END_CUSTOM_SCHEDULE_PROVIDER()

#include "tier0/memdbgoff.h"
