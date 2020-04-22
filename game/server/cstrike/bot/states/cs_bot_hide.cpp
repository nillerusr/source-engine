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
 * Begin moving to a nearby hidey-hole.
 * NOTE: Do not forget this state may include a very long "move-to" time to get to our hidey spot!
 */
void HideState::OnEnter( CCSBot *me )
{
	m_isAtSpot = false;
	m_isLookingOutward = false;

	// if duration is "infinite", set it to a reasonably long time to prevent infinite camping
	if (m_duration < 0.0f)
	{
		m_duration = RandomFloat( 30.0f, 60.0f );
	}

	// decide whether to "ambush" or not - never set to false so as not to override external setting
	if (RandomFloat( 0.0f, 100.0f ) < 50.0f)
	{
		m_isHoldingPosition = true;
	}

	// if we are holding position, decide for how long
	if (m_isHoldingPosition)
	{
		m_holdPositionTime = RandomFloat( 3.0f, 10.0f );
	}
	else
	{
		m_holdPositionTime = 0.0f;
	}

	m_heardEnemy = false;
	m_firstHeardEnemyTime = 0.0f;
	m_retry = 0;

	if (me->IsFollowing())
	{
		m_leaderAnchorPos = GetCentroid( me->GetFollowLeader() );
	}

	// if we are a sniper, we need to periodically pause while we retreat to squeeze off a shot or two
	if (me->IsSniper())
	{
		// start off paused to allow a final shot before retreating
		m_isPaused = false;
		m_pauseTimer.Invalidate();
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Move to a nearby hidey-hole.
 * NOTE: Do not forget this state may include a very long "move-to" time to get to our hidey spot!
 */
void HideState::OnUpdate( CCSBot *me )
{
	Vector myOrigin = GetCentroid( me );

	// wait until finished reloading to leave hide state
	if (!me->IsReloading())
	{
		// if we are momentarily hiding while following someone, check to see if he has moved on
		if (me->IsFollowing())
		{
			CCSPlayer *leader = static_cast<CCSPlayer *>( static_cast<CBaseEntity *>( me->GetFollowLeader() ) );
			Vector leaderOrigin = GetCentroid( leader );

			// BOTPORT: Determine walk/run velocity thresholds
			float runThreshold = 200.0f;
			if (leader->GetAbsVelocity().IsLengthGreaterThan( runThreshold ))
			{
				// leader is running, stay with him
				me->Follow( leader );
				return;			
			}

			// if leader has moved, stay with him
			const float followRange = 250.0f;
			if ((m_leaderAnchorPos - leaderOrigin).IsLengthGreaterThan( followRange ))
			{
				me->Follow( leader );
				return;
			}
		}

		// if we see a nearby buddy in combat, join him
		/// @todo - Perhaps tie in to TakeDamage(), so it works for human players, too

		//
		// Scenario logic
		//
		switch( TheCSBots()->GetScenario() )
		{
			case CCSBotManager::SCENARIO_DEFUSE_BOMB:
			{
				if (me->GetTeamNumber() == TEAM_CT)
				{
					// if we are just holding position (due to a radio order) and the bomb has just planted, go defuse it
					if (me->GetTask() == CCSBot::HOLD_POSITION && 
						TheCSBots()->IsBombPlanted() &&
						TheCSBots()->GetBombPlantTimestamp() > me->GetStateTimestamp())
					{
						me->Idle();
						return;
					}

					// if we are guarding the defuser and he dies/gives up, stop hiding (to choose another defuser)
					if (me->GetTask() == CCSBot::GUARD_BOMB_DEFUSER && TheCSBots()->GetBombDefuser() == NULL)
					{
						me->Idle();
						return;
					}

					// if we are guarding the loose bomb and it is picked up, stop hiding
					if (me->GetTask() == CCSBot::GUARD_LOOSE_BOMB && TheCSBots()->GetLooseBomb() == NULL)
					{
						me->GetChatter()->TheyPickedUpTheBomb();
						me->Idle();
						return;
					}

					// if we are guarding a bombsite and the bomb is dropped and we hear about it, stop guarding
					if (me->GetTask() == CCSBot::GUARD_BOMB_ZONE && me->GetGameState()->IsLooseBombLocationKnown())
					{
						me->Idle();
						return;
					}

					// if we are guarding (bombsite, initial encounter, etc) and the bomb is planted, go defuse it
					if (me->IsDoingScenario() && me->GetTask() != CCSBot::GUARD_BOMB_DEFUSER && TheCSBots()->IsBombPlanted())
					{
						me->Idle();
						return;
					}

				}
				else	// TERRORIST
				{
					// if we are near the ticking bomb and someone starts defusing it, attack!
					if (TheCSBots()->GetBombDefuser())
					{
						Vector defuserOrigin = GetCentroid( TheCSBots()->GetBombDefuser() );
						Vector toDefuser = defuserOrigin - myOrigin;

						const float hearDefuseRange = 2000.0f;
						if (toDefuser.IsLengthLessThan( hearDefuseRange ))
						{
							// if we are nearby, attack, otherwise move to the bomb (which will cause us to attack when we see defuser)
							if (me->CanSeePlantedBomb())
							{
								me->Attack( TheCSBots()->GetBombDefuser() );
							}
							else
							{
								me->MoveTo( defuserOrigin, FASTEST_ROUTE );
								me->InhibitLookAround( 10.0f );
							}

							return;
						}
					}
				}
				break;
			}

			//--------------------------------------------------------------------------------------------------
			case CCSBotManager::SCENARIO_RESCUE_HOSTAGES:
			{
				// if we're guarding the hostages and they all die or are taken, do something else
				if (me->GetTask() == CCSBot::GUARD_HOSTAGES)
				{
					if (me->GetGameState()->AreAllHostagesBeingRescued() || me->GetGameState()->AreAllHostagesGone())
					{
						me->Idle();
						return;
					}
				}
				else if (me->GetTask() == CCSBot::GUARD_HOSTAGE_RESCUE_ZONE)
				{
					// if we stumble across a hostage, guard it
					CHostage *hostage = me->GetGameState()->GetNearestVisibleFreeHostage();
					if (hostage)
					{
						// we see a free hostage, guard it
						Vector hostageOrigin = GetCentroid( hostage );
						CNavArea *area = TheNavMesh->GetNearestNavArea( hostageOrigin );
						if (area)
						{
							me->SetTask( CCSBot::GUARD_HOSTAGES );
							me->Hide( area );
							me->PrintIfWatched( "I'm guarding hostages I found\n" );
							// don't chatter here - he'll tell us when he's in his hiding spot
							return;
						}
					}
				}
			}
		}


		bool isSettledInSniper = (me->IsSniper() && m_isAtSpot) ? true : false;

		// only investigate noises if we are initiating attacks, and we aren't a "settled in" sniper
		// dont investigate noises if we are reloading
		if (!me->IsReloading() && 
			!isSettledInSniper && 
			me->GetDisposition() == CCSBot::ENGAGE_AND_INVESTIGATE)
		{
			// if we are holding position, and have heard the enemy nearby, investigate after our hold time is up
			if (m_isHoldingPosition && m_heardEnemy && (gpGlobals->curtime - m_firstHeardEnemyTime > m_holdPositionTime))
			{
				/// @todo We might need to remember specific location of last enemy noise here
				me->InvestigateNoise();
				return;
			}

			// investigate nearby enemy noises
			if (me->HeardInterestingNoise())
			{
				// if we are holding position, check if enough time has elapsed since we first heard the enemy
				if (m_isAtSpot && m_isHoldingPosition)
				{
					if (!m_heardEnemy)
					{
						// first time we heard the enemy
						m_heardEnemy = true;
						m_firstHeardEnemyTime = gpGlobals->curtime;
						me->PrintIfWatched( "Heard enemy, holding position for %f2.1 seconds...\n", m_holdPositionTime );
					}
				}
				else
				{
					// not holding position - investigate enemy noise
					me->InvestigateNoise();
					return;
				}
			}
		}
	}	// end reloading check

	// look around
	me->UpdateLookAround();

	// if we are at our hiding spot, crouch and wait
	if (m_isAtSpot)
	{
		me->ResetStuckMonitor();

		CNavArea *area = TheNavMesh->GetNavArea( m_hidingSpot );
		if ( !area || !( area->GetAttributes() & NAV_MESH_STAND ) )
		{
			me->Crouch();
		}

		// check if duration has expired
		if (m_hideTimer.IsElapsed()) 
		{
			if (me->GetTask() == CCSBot::GUARD_LOOSE_BOMB)
			{
				// if we're guarding the loose bomb, continue to guard it but pick a new spot
				me->Hide( TheCSBots()->GetLooseBombArea() );
				return;
			}
			else if (me->GetTask() == CCSBot::GUARD_BOMB_ZONE)
			{
				// if we're guarding a bombsite, continue to guard it but pick a new spot
				const CCSBotManager::Zone *zone = TheCSBots()->GetClosestZone( myOrigin );
				if (zone)
				{
					CNavArea *area = TheCSBots()->GetRandomAreaInZone( zone );
					if (area)
					{
						me->Hide( area );
						return;
					}
				}
			}
			else if (me->GetTask() == CCSBot::GUARD_HOSTAGE_RESCUE_ZONE)
			{
				// if we're guarding a rescue zone, continue to guard this or another rescue zone
				if (me->GuardRandomZone())
				{
					me->SetTask( CCSBot::GUARD_HOSTAGE_RESCUE_ZONE );
					me->PrintIfWatched( "Continuing to guard hostage rescue zones\n" );
					me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );
					me->GetChatter()->GuardingHostageEscapeZone( IS_PLAN );
					return;
				}
			}

			me->Idle();
			return;
		}

/*
		// if we are watching for an approaching noisy enemy, anticipate and fire before they round the corner
		/// @todo Need to check if we are looking at an ENEMY_NOISE here
		const float veryCloseNoise = 250.0f;
		if (me->IsLookingAtSpot() && me->GetNoiseRange() < veryCloseNoise)
		{
			// fire!
			me->PrimaryAttack();
			me->PrintIfWatched( "Firing at anticipated enemy coming around the corner!\n" );
		}
*/

		// if we have a shield, hide behind it
		if (me->HasShield() && !me->IsProtectedByShield())
			me->SecondaryAttack();

		// while sitting at our hiding spot, if we are being attacked but can't see our attacker, move somewhere else
		const float hurtRecentlyTime = 1.0f;
		if (!me->IsEnemyVisible() && me->GetTimeSinceAttacked() < hurtRecentlyTime)
		{
			me->Idle();
			return;
		}

		// encourage the human player
		if (!me->IsDoingScenario())
		{
			if (me->GetTeamNumber() == TEAM_CT)
			{
				if (me->GetTask() == CCSBot::GUARD_BOMB_ZONE && 
					me->IsAtHidingSpot() && 
					TheCSBots()->IsBombPlanted())
				{
					if (me->GetNearbyEnemyCount() == 0)
					{
						const float someTime = 30.0f;
						const float littleTime = 11.0;

						if (TheCSBots()->GetBombTimeLeft() > someTime)
							me->GetChatter()->Encourage( "BombsiteSecure", RandomFloat( 10.0f, 15.0f ) );
						else if (TheCSBots()->GetBombTimeLeft() > littleTime)
							me->GetChatter()->Encourage( "WaitingForHumanToDefuseBomb", RandomFloat( 5.0f, 8.0f ) );
						else
							me->GetChatter()->Encourage( "WaitingForHumanToDefuseBombPanic", RandomFloat( 3.0f, 4.0f ) );
					}
				}

				if (me->GetTask() == CCSBot::GUARD_HOSTAGES && me->IsAtHidingSpot())
				{
					if (me->GetNearbyEnemyCount() == 0)
					{
						CHostage *hostage = me->GetGameState()->GetNearestVisibleFreeHostage();
						if (hostage)
						{
							me->GetChatter()->Encourage( "WaitingForHumanToRescueHostages", RandomFloat( 10.0f, 15.0f ) );
						}
					}
				}
			}
		}
	}
	else
	{
		// we are moving to our hiding spot

		// snipers periodically pause and fire while retreating
		if (me->IsSniper() && me->IsEnemyVisible())
		{
			if (m_isPaused)
			{
				if (m_pauseTimer.IsElapsed())
				{
					// get moving
					m_isPaused = false;
					m_pauseTimer.Start( RandomFloat( 1.0f, 3.0f ) );
				}
				else
				{
					me->Wait( 0.2f );
				}
			}
			else
			{
				if (m_pauseTimer.IsElapsed())
				{
					// pause for a moment
					m_isPaused = true;
					m_pauseTimer.Start( RandomFloat( 0.5f, 1.5f ) );
				}
			}
		}

		// if a Player is using this hiding spot, give up
		float range;
		CCSPlayer *camper = static_cast<CCSPlayer *>( UTIL_GetClosestPlayer( m_hidingSpot, &range ) );

		const float closeRange = 75.0f;
		if (camper && camper != me && range < closeRange && me->IsVisible( camper, CHECK_FOV ))
		{
			// player is in our hiding spot
			me->PrintIfWatched( "Someone's in my hiding spot - picking another...\n" );

			const int maxRetries = 3;
			if (m_retry++ >= maxRetries)
			{
				me->PrintIfWatched( "Can't find a free hiding spot, giving up.\n" );
				me->Idle();
				return;
			}

			// pick another hiding spot near where we were planning on hiding
			me->Hide( TheNavMesh->GetNavArea( m_hidingSpot ) );

			return;
		}

		Vector toSpot;
		toSpot.x = m_hidingSpot.x - myOrigin.x;
		toSpot.y = m_hidingSpot.y - myOrigin.y;
		toSpot.z = m_hidingSpot.z - me->GetFeetZ(); // use feet location
		range = toSpot.Length();

		// look outwards as we get close to our hiding spot
		if (!me->IsEnemyVisible() && !m_isLookingOutward)
		{
			const float lookOutwardRange = 200.0f;
			const float nearSpotRange = 10.0f;
			if (range < lookOutwardRange && range > nearSpotRange)
			{
				m_isLookingOutward = true;

				toSpot.x /= range;
				toSpot.y /= range;
				toSpot.z /= range;

				me->SetLookAt( "Face outward", me->EyePosition() - 1000.0f * toSpot, PRIORITY_HIGH, 3.0f );
			}
		}

		const float atDist = 20.0f;
		if (range < atDist)
		{
			//-------------------------------------
			// Just reached our hiding spot
			//
			m_isAtSpot = true;
			m_hideTimer.Start( m_duration );

			// make sure our approach points are valid, since we'll be watching them
			me->ComputeApproachPoints();
			me->ClearLookAt();

			// ready our weapon and prepare to attack
			me->EquipBestWeapon( me->IsUsingGrenade() );
			me->SetDisposition( CCSBot::OPPORTUNITY_FIRE );

			// if we are a sniper, update our task
			if (me->GetTask() == CCSBot::MOVE_TO_SNIPER_SPOT)
			{
				me->SetTask( CCSBot::SNIPING );
			}
			else if (me->GetTask() == CCSBot::GUARD_INITIAL_ENCOUNTER)
			{
				const float campChatterChance = 20.0f;
				if (RandomFloat( 0, 100 ) < campChatterChance)
				{
					me->GetChatter()->Say( "WaitingHere" );
				}
			}

			
			// determine which way to look
			trace_t result;
			float outAngle = 0.0f;
			float outAngleRange = 0.0f;
			for( float angle = 0.0f; angle < 360.0f; angle += 45.0f )
			{
				UTIL_TraceLine( me->EyePosition(), me->EyePosition() + 1000.0f * Vector( BotCOS(angle), BotSIN(angle), 0.0f ), MASK_PLAYERSOLID, me, COLLISION_GROUP_NONE, &result );

				if (result.fraction > outAngleRange)
				{
					outAngle = angle;
					outAngleRange = result.fraction;
				}
			}

			me->SetLookAheadAngle( outAngle );

		}

		// move to hiding spot
		if (me->UpdatePathMovement() != CCSBot::PROGRESSING && !m_isAtSpot)
		{
			// we couldn't get to our hiding spot - pick another
			me->PrintIfWatched( "Can't get to my hiding spot - finding another...\n" );

			// search from hiding spot, since we know it was valid
			const Vector *pos = FindNearbyHidingSpot( me, m_hidingSpot, m_range, me->IsSniper() );
			if (pos == NULL)
			{
				// no available hiding spots
				me->PrintIfWatched( "No available hiding spots - hiding where I'm at.\n" );

				// hide where we are
				m_hidingSpot.x = myOrigin.x;
				m_hidingSpot.x = myOrigin.y;
				m_hidingSpot.z = me->GetFeetZ();
			}
			else
			{
				m_hidingSpot = *pos;
			}

			// build a path to our new hiding spot
			if (me->ComputePath( m_hidingSpot, FASTEST_ROUTE ) == false)
			{
				me->PrintIfWatched( "Can't pathfind to hiding spot\n" );
				me->Idle();
				return;
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void HideState::OnExit( CCSBot *me )
{
	m_isHoldingPosition = false;

	me->StandUp();
	me->ResetStuckMonitor();
	//me->ClearLookAt();
	me->ClearApproachPoints();

	// if we have a shield, put it away
	if (me->HasShield() && me->IsProtectedByShield())
		me->SecondaryAttack();
}
