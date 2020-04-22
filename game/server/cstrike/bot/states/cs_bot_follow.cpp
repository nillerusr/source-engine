//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"
#include "cs_bot.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------------
/**
 * Follow our leader
 */
void FollowState::OnEnter( CCSBot *me )
{
	me->StandUp();
	me->Run();
	me->DestroyPath();

	m_isStopped = false;
	m_stoppedTimestamp = 0.0f;

	// to force immediate repath
	m_lastLeaderPos.x = -99999999.9f;
	m_lastLeaderPos.y = -99999999.9f;
	m_lastLeaderPos.z = -99999999.9f;

	m_lastSawLeaderTime = 0;

	// set re-pathing frequency
	m_repathInterval.Invalidate();

	m_isSneaking = false;

	m_walkTime.Invalidate();
	m_isAtWalkSpeed = false;

	m_leaderMotionState = INVALID;
	m_idleTimer.Start( RandomFloat( 2.0f, 5.0f ) );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Determine the leader's motion state by tracking his speed
 */
void FollowState::ComputeLeaderMotionState( float leaderSpeed )
{
	// walk = 130, run = 250
	const float runWalkThreshold = 140.0f;
	const float walkStopThreshold = 10.0f; // 120.0f;
	LeaderMotionStateType prevState = m_leaderMotionState;
	if (leaderSpeed > runWalkThreshold)
	{
		m_leaderMotionState = RUNNING;
		m_isAtWalkSpeed = false;
	}
	else if (leaderSpeed > walkStopThreshold)
	{
		// track when began to walk
		if (!m_isAtWalkSpeed)
		{
			m_walkTime.Start();
			m_isAtWalkSpeed = true;
		}

		const float minWalkTime = 0.25f;
		if (m_walkTime.GetElapsedTime() > minWalkTime)
		{
			m_leaderMotionState = WALKING;
		}
	}
	else
	{
		m_leaderMotionState = STOPPED;
		m_isAtWalkSpeed = false;
	}

	// track time spent in this motion state
	if (prevState != m_leaderMotionState)
	{
		m_leaderMotionStateTime.Start();
		m_waitTime = RandomFloat( 1.0f, 3.0f );
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Functor to collect all areas in the forward direction of the given player within a radius
 */
class FollowTargetCollector
{
public:
	FollowTargetCollector( CBasePlayer *player )
	{
		m_player = player;

		Vector playerVel = player->GetAbsVelocity();
		m_forward.x = playerVel.x;
		m_forward.y = playerVel.y;
		float speed = m_forward.NormalizeInPlace();

		Vector playerOrigin = GetCentroid( player );

		const float walkSpeed = 100.0f;
		if (speed < walkSpeed)
		{
			m_cutoff.x = playerOrigin.x;
			m_cutoff.y = playerOrigin.y;
			m_forward.x = 0.0f;
			m_forward.y = 0.0f;
		}
		else
		{
			const float k = 1.5f; // 2.0f;
			float trimSpeed = MIN( speed, 200.0f );
			m_cutoff.x = playerOrigin.x + k * trimSpeed * m_forward.x;
			m_cutoff.y = playerOrigin.y + k * trimSpeed * m_forward.y;
		}

		m_targetAreaCount = 0;
	}

	enum { MAX_TARGET_AREAS = 128 };

	bool operator() ( CNavArea *area )
	{
		if (m_targetAreaCount >= MAX_TARGET_AREAS)
			return false;
		
		// only use two-way connections
		if (!area->GetParent() || area->IsConnected( area->GetParent(), NUM_DIRECTIONS ))
		{
			if (m_forward.IsZero())
			{
				m_targetArea[ m_targetAreaCount++ ] = area;
			}
			else
			{
				// collect areas in the direction of the player's forward motion
				Vector2D to( area->GetCenter().x - m_cutoff.x, area->GetCenter().y - m_cutoff.y );
				to.NormalizeInPlace();

				//if (DotProduct( to, m_forward ) > 0.7071f)
				if ((to.x * m_forward.x + to.y * m_forward.y) > 0.7071f)
					m_targetArea[ m_targetAreaCount++ ] = area;
			}
		}

		return (m_targetAreaCount < MAX_TARGET_AREAS);
	}


	CBasePlayer *m_player;
	Vector2D m_forward;
	Vector2D m_cutoff;

	CNavArea *m_targetArea[ MAX_TARGET_AREAS ];
	int m_targetAreaCount;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Follow our leader
 * @todo Clean up this nasty mess
 */
void FollowState::OnUpdate( CCSBot *me )
{
	// if we lost our leader, give up
	if (m_leader == NULL || !m_leader->IsAlive())
	{
		me->Idle();
		return;
	}

	// if we are carrying the bomb and at a bombsite, plant
	if (me->HasC4() && me->IsAtBombsite())
	{
		// plant it
		me->SetTask( CCSBot::PLANT_BOMB );
		me->PlantBomb();

		// radio to the team
		me->GetChatter()->PlantingTheBomb( me->GetPlace() );

		return;
	}

	// look around
	me->UpdateLookAround();

	// if we are moving, we are not idle
	if (me->IsNotMoving() == false)
		m_idleTimer.Start( RandomFloat( 2.0f, 5.0f ) );

	// compute the leader's speed
	Vector leaderVel = m_leader->GetAbsVelocity();
	float leaderSpeed = Vector2D( leaderVel.x, leaderVel.y ).Length();

	// determine our leader's movement state
	ComputeLeaderMotionState( leaderSpeed );

	// track whether we can see the leader
	bool isLeaderVisible;
	Vector leaderOrigin = GetCentroid( m_leader );
	if (me->IsVisible( leaderOrigin ))
	{
		m_lastSawLeaderTime = gpGlobals->curtime;
		isLeaderVisible = true;
	}
	else
	{
		isLeaderVisible = false;
	}


	// determine whether we should sneak or not
	const float farAwayRange = 750.0f;
	Vector myOrigin = GetCentroid( me );
	if ((leaderOrigin - myOrigin).IsLengthGreaterThan( farAwayRange ))
	{
		// far away from leader - run to catch up
		m_isSneaking = false;
	}
	else if (isLeaderVisible)
	{
		// if we see leader walking and we are nearby, walk
		if (m_leaderMotionState == WALKING)
			m_isSneaking = true;

		// if we are sneaking and our leader starts running, stop sneaking
		if (m_isSneaking && m_leaderMotionState == RUNNING)
			m_isSneaking = false;
	}

	// if we haven't seen the leader for a long time, run
	const float longTime = 20.0f;
	if (gpGlobals->curtime - m_lastSawLeaderTime > longTime)
		m_isSneaking = false;

	if (m_isSneaking)
		me->Walk();
	else
		me->Run();


	bool repath = false;

	// if the leader has stopped, hide nearby
	const float nearLeaderRange = 250.0f;
	if (!me->HasPath() && m_leaderMotionState == STOPPED && m_leaderMotionStateTime.GetElapsedTime() > m_waitTime)
	{
		// throttle how often this check occurs
		m_waitTime += RandomFloat( 1.0f, 3.0f );

		// the leader has stopped - if we are close to him, take up a hiding spot
		if ((leaderOrigin - myOrigin).IsLengthLessThan( nearLeaderRange ))
		{
			const float hideRange = 250.0f;
			if (me->TryToHide( NULL, -1.0f, hideRange, false, USE_NEAREST ))
			{
				me->ResetStuckMonitor();
				return;
			}
		}
	}

	// if we have been idle for awhile, move
	if (m_idleTimer.IsElapsed())
	{
		repath = true;

		// always walk when we move such a short distance
		m_isSneaking = true;
	}

	// if our leader has moved, repath (don't repath if leading is stopping)
	if (leaderSpeed > 100.0f && m_leaderMotionState != STOPPED)
	{
		repath = true;
	}

	// move along our path
	if (me->UpdatePathMovement( NO_SPEED_CHANGE ) != CCSBot::PROGRESSING)
	{
		me->DestroyPath();
	}

	// recompute our path if necessary	
	if (repath && m_repathInterval.IsElapsed() && !me->IsOnLadder())
	{
		// recompute our path to keep us near our leader
		m_lastLeaderPos = leaderOrigin;

		me->ResetStuckMonitor();

		const float runSpeed = 200.0f;

		const float collectRange = (leaderSpeed > runSpeed) ? 600.0f : 400.0f;		// 400, 200
		FollowTargetCollector collector( m_leader );
		SearchSurroundingAreas( TheNavMesh->GetNearestNavArea( m_lastLeaderPos ), m_lastLeaderPos, collector, collectRange );

		if (cv_bot_debug.GetBool())
		{
			for( int i=0; i<collector.m_targetAreaCount; ++i )
				collector.m_targetArea[i]->Draw( /*255, 0, 0, 2*/ );		
		}

		// move to one of the collected areas
		if (collector.m_targetAreaCount)
		{
			CNavArea *target = NULL;
			Vector targetPos;

			// if we are idle, pick a random area
			if (m_idleTimer.IsElapsed())
			{
				target = collector.m_targetArea[ RandomInt( 0, collector.m_targetAreaCount-1 ) ];				
				targetPos = target->GetCenter();
				me->PrintIfWatched( "%4.1f: Bored. Repathing to a new nearby area\n", gpGlobals->curtime );
			}
			else
			{
				me->PrintIfWatched( "%4.1f: Repathing to stay with leader.\n", gpGlobals->curtime );

				// find closest area to where we are
				CNavArea *area;
				float closeRangeSq = 9999999999.9f;
				Vector close;

				for( int a=0; a<collector.m_targetAreaCount; ++a )
				{
					area = collector.m_targetArea[a];

					area->GetClosestPointOnArea( myOrigin, &close );

					float rangeSq = (myOrigin - close).LengthSqr();
					if (rangeSq < closeRangeSq)
					{
						target = area;
						targetPos = close;
						closeRangeSq = rangeSq;
					}
				}
			}
						
			if (target == NULL || me->ComputePath( target->GetCenter(), FASTEST_ROUTE ) == false)
				me->PrintIfWatched( "Pathfind to leader failed.\n" );

			// throttle how often we repath
			m_repathInterval.Start( 0.5f );

			m_idleTimer.Reset();
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
void FollowState::OnExit( CCSBot *me )
{
}
