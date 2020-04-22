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
 * Move towards currently heard noise
 */
void InvestigateNoiseState::AttendCurrentNoise( CCSBot *me )
{
	if (!me->IsNoiseHeard() && me->GetNoisePosition())
		return;

	// remember where the noise we heard was
	m_checkNoisePosition = *me->GetNoisePosition();

	// tell our teammates (unless the noise is obvious, like gunfire)
	if (me->IsWellPastSafe() && me->HasNotSeenEnemyForLongTime() && me->GetNoisePriority() != PRIORITY_HIGH)
		me->GetChatter()->HeardNoise( *me->GetNoisePosition() );

	// figure out how to get to the noise		
	me->PrintIfWatched( "Attending to noise...\n" );
	me->ComputePath( m_checkNoisePosition, FASTEST_ROUTE );

	const float minAttendTime = 3.0f;
	const float maxAttendTime = 10.0f;
	m_minTimer.Start( RandomFloat( minAttendTime, maxAttendTime ) );

	// consume the noise
	me->ForgetNoise();
}

//--------------------------------------------------------------------------------------------------------------
void InvestigateNoiseState::OnEnter( CCSBot *me )
{
	AttendCurrentNoise( me );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * @todo Use TravelDistance instead of distance...
 */
void InvestigateNoiseState::OnUpdate( CCSBot *me )
{
	Vector myOrigin = GetCentroid( me );

	// keep an ear out for closer noises...
	if (m_minTimer.IsElapsed())
	{
		const float nearbyRange = 500.0f;
		if (me->HeardInterestingNoise() && me->GetNoiseRange() < nearbyRange)
		{
			// new sound is closer
			AttendCurrentNoise( me );
		}
	}


	// if the pathfind fails, give up
	if (!me->HasPath())
	{
		me->Idle();
		return;
	}

	// look around
	me->UpdateLookAround();

	// get distance remaining on our path until we reach the source of the noise
	float range = me->GetPathDistanceRemaining();

	if (me->IsUsingKnife())
	{
		if (me->IsHurrying())
			me->Run();
		else
			me->Walk();
	}
	else
	{
		const float closeToNoiseRange = 1500.0f;
		if (range < closeToNoiseRange)
		{
			// if we dont have many friends left, or we are alone, and we are near noise source, sneak quietly
			if ((me->GetNearbyFriendCount() == 0 || me->GetFriendsRemaining() <= 2) && !me->IsHurrying())
			{
				me->Walk();
			}
			else
			{
				me->Run();
			}
		}
		else
		{
			me->Run();
		}
	}


	// if we can see the noise position and we're close enough to it and looking at it, 
	// we don't need to actually move there (it's checked enough)
	const float closeRange = 500.0f;
	if (range < closeRange)
	{
		if (me->IsVisible( m_checkNoisePosition, CHECK_FOV ))
		{
			// can see noise position
			me->PrintIfWatched( "Noise location is clear.\n" );
			me->ForgetNoise();
			me->Idle();
			return;
		}
	}

	// move towards noise
	if (me->UpdatePathMovement() != CCSBot::PROGRESSING)
	{
		me->Idle();
	}
}

//--------------------------------------------------------------------------------------------------------------
void InvestigateNoiseState::OnExit( CCSBot *me )
{
	// reset to run mode in case we were sneaking about
	me->Run();
}
