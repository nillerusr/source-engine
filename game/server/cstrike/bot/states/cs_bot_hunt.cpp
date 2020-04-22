//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"
#include "cs_simple_hostage.h"
#include "cs_bot.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------------
/**
 * Begin the hunt
 */
void HuntState::OnEnter( CCSBot *me )
{
	// lurking death
	if (me->IsUsingKnife() && me->IsWellPastSafe() && !me->IsHurrying())
		me->Walk();
	else
		me->Run();


	me->StandUp();
	me->SetDisposition( CCSBot::ENGAGE_AND_INVESTIGATE );
	me->SetTask( CCSBot::SEEK_AND_DESTROY );

	me->DestroyPath();
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Hunt down our enemies
 */
void HuntState::OnUpdate( CCSBot *me )
{
	// if we've been hunting for a long time, drop into Idle for a moment to
	// select something else to do
	const float huntingTooLongTime = 30.0f;
	if (gpGlobals->curtime - me->GetStateTimestamp() > huntingTooLongTime)
	{
		// stop being a rogue and do the scenario, since there must not be many enemies left to hunt
		me->PrintIfWatched( "Giving up hunting.\n" );
		me->SetRogue( false );
		me->Idle();
		return;
	}

	// scenario logic
	if (TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_DEFUSE_BOMB)
	{
		if (me->GetTeamNumber() == TEAM_TERRORIST)
		{
			// if we have the bomb and it's time to plant, or we happen to be in a bombsite and it seems safe, do it
			if (me->HasC4())
			{
				const float safeTime = 3.0f;

				if (TheCSBots()->IsTimeToPlantBomb() || 
						(me->IsAtBombsite() && gpGlobals->curtime - me->GetLastSawEnemyTimestamp() > safeTime))
				{
					me->Idle();
					return;
				}
			}

			// if we notice the bomb lying on the ground, go get it
			if (me->NoticeLooseBomb())
			{
				me->FetchBomb();
				return;
			}

			// if bomb has been planted, and we hear it, move to a hiding spot near the bomb and watch it
			const Vector *bombPos = me->GetGameState()->GetBombPosition();
			if (!me->IsRogue() && me->GetGameState()->IsBombPlanted() && bombPos)
			{
				me->SetTask( CCSBot::GUARD_TICKING_BOMB );
				me->Hide( TheNavMesh->GetNavArea( *bombPos ) );
				return;
			}
		}
		else		// CT
		{
			if (!me->IsRogue() && me->CanSeeLooseBomb())
			{
				// if we are near the loose bomb and can see it, hide nearby and guard it
				me->SetTask( CCSBot::GUARD_LOOSE_BOMB );
				me->Hide( TheCSBots()->GetLooseBombArea() );
				me->GetChatter()->GuardingLooseBomb( TheCSBots()->GetLooseBomb() );
				return;
			}
			else if (TheCSBots()->IsBombPlanted())
			{
				// rogues will defuse a bomb, but not guard the defuser
				if (!me->IsRogue() || !TheCSBots()->GetBombDefuser())
				{
					// search for the planted bomb to defuse
					me->Idle();
					return;
				}
			}
		}
	}
	else if (TheCSBots()->GetScenario() == CCSBotManager::SCENARIO_RESCUE_HOSTAGES)
	{
		if (me->GetTeamNumber() == TEAM_TERRORIST)
		{
			if (me->GetGameState()->AreAllHostagesBeingRescued())
			{
				// all hostages are being rescued, head them off at the escape zones
				if (me->GuardRandomZone())
				{
					me->SetTask( CCSBot::GUARD_HOSTAGE_RESCUE_ZONE );
					me->PrintIfWatched( "Trying to beat them to an escape zone!\n" );
					me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
					me->GetChatter()->GuardingHostageEscapeZone( IS_PLAN );
					return;
				}
			}

			// if safe time is up, and we stumble across a hostage, guard it
			if (!me->IsRogue() && !me->IsSafe())
			{
				CHostage *hostage = me->GetGameState()->GetNearestVisibleFreeHostage();
				if (hostage)
				{
					CNavArea *area = TheNavMesh->GetNearestNavArea( GetCentroid( hostage ) );
					if (area)
					{
						// we see a free hostage, guard it
						me->SetTask( CCSBot::GUARD_HOSTAGES );
						me->Hide( area );
						me->PrintIfWatched( "I'm guarding hostages\n" );
						me->GetChatter()->GuardingHostages( area->GetPlace(), IS_PLAN );
						return;
					}
				}
			}
		}
	}

	// listen for enemy noises
	if (me->HeardInterestingNoise())
	{
		me->InvestigateNoise();
		return;
	}
		
	// look around
	me->UpdateLookAround();

	// if we have reached our destination area, pick a new one
	// if our path fails, pick a new one
	if (me->GetLastKnownArea() == m_huntArea || me->UpdatePathMovement() != CCSBot::PROGRESSING)
	{
		// pick a new hunt area
		const float earlyGameTime = 45.0f;
		if (TheCSBots()->GetElapsedRoundTime() < earlyGameTime && !me->HasVisitedEnemySpawn())
		{
			// in the early game, rush the enemy spawn
			CBaseEntity *enemySpawn = TheCSBots()->GetRandomSpawn( OtherTeam( me->GetTeamNumber() ) );

			//ADRIAN: REVISIT
			if ( enemySpawn )
			{
				m_huntArea = TheNavMesh->GetNavArea( enemySpawn->WorldSpaceCenter() );
			}
		}
		else
		{
			m_huntArea = NULL;
			float oldest = 0.0f;

			int areaCount = 0;
			const float minSize = 150.0f;

			FOR_EACH_VEC( TheNavAreas, it )
			{
				CNavArea *area = TheNavAreas[ it ];

				++areaCount;

				// skip the small areas
				Extent extent;
				area->GetExtent(&extent);
				if (extent.hi.x - extent.lo.x < minSize || extent.hi.y - extent.lo.y < minSize)
					continue;

				// keep track of the least recently cleared area
				float age = gpGlobals->curtime - area->GetClearedTimestamp( me->GetTeamNumber()-1 );
				if (age > oldest)
				{
					oldest = age;
					m_huntArea = area;
				}
			}

			// if all the areas were too small, pick one at random
			int which = RandomInt( 0, areaCount-1 );

			areaCount = 0;
			FOR_EACH_VEC( TheNavAreas, hit )
			{
				m_huntArea = TheNavAreas[ hit ];

				if (which == areaCount)
					break;

				--which;
			}
		}

		if (m_huntArea)
		{
			// create a new path to a far away area of the map
			me->ComputePath( m_huntArea->GetCenter() );
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Done hunting
 */
void HuntState::OnExit( CCSBot *me )
{
}
