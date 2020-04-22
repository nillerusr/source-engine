//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// bot_hide.cpp
// Mechanisms for using Hiding Spots in the Navigation Mesh
// Author: Michael Booth, 2003-2004

#include "cbase.h"
#include "bot.h"
#include "cs_nav_pathfind.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------------
/**
 * If a player is at the given spot, return true
 */
bool IsSpotOccupied( CBaseEntity *me, const Vector &pos )
{
	const float closeRange = 75.0f;		// 50

	// is there a player in this spot
	float range;
	CBasePlayer *player = UTIL_GetClosestPlayer( pos, &range );

	if (player != me)
	{
		if (player && range < closeRange)
			return true;
	}

	// is there is a hostage in this spot
	// BOTPORT: Implement hostage manager
	/*
	if (g_pHostages)
	{
		CHostage *hostage = g_pHostages->GetClosestHostage( *pos, &range );
		if (hostage && hostage != me && range < closeRange)
			return true;
	}
	*/

	return false;
}

//--------------------------------------------------------------------------------------------------------------
class CollectHidingSpotsFunctor
{
public:
	CollectHidingSpotsFunctor( CBaseEntity *me, const Vector &origin, float range, int flags, Place place = UNDEFINED_PLACE ) : m_origin( origin )
	{
		m_me = me;
		m_count = 0;
		m_range = range;
		m_flags = (unsigned char)flags;
		m_place = place;
		m_totalWeight = 0;
	}

	enum { MAX_SPOTS = 256 };

	bool operator() ( CNavArea *area )
	{
		// if a place is specified, only consider hiding spots from areas in that place
		if (m_place != UNDEFINED_PLACE && area->GetPlace() != m_place)
			return true;

		// collect all the hiding spots in this area
		const HidingSpotVector *pSpots = area->GetHidingSpots();
		
		FOR_EACH_VEC( (*pSpots), it )
		{
			const HidingSpot *spot = (*pSpots)[ it ];

			// if we've filled up, stop searching
			if (m_count == MAX_SPOTS)
			{
				return false;
			}

			// make sure hiding spot is in range
			if (m_range > 0.0f)
			{
				if ((spot->GetPosition() - m_origin).IsLengthGreaterThan( m_range ))
				{
					continue;
				}
			}

			// if a Player is using this hiding spot, don't consider it
			if (IsSpotOccupied( m_me, spot->GetPosition() ))
			{
				// player is in hiding spot
				/// @todo Check if player is moving or sitting still
				continue;
			}

			if (spot->GetArea() && (spot->GetArea()->GetAttributes() & NAV_MESH_DONT_HIDE))
			{
				// the area has been marked as DONT_HIDE since the last analysis, so let's ignore it
				continue;
			}

			// only collect hiding spots with matching flags
			if (m_flags & spot->GetFlags())
			{
				m_hidingSpot[ m_count ] = &spot->GetPosition();
				m_hidingSpotWeight[ m_count ] = m_totalWeight;

				// if it's an 'avoid' area, give it a low weight
				if ( spot->GetArea() && ( spot->GetArea()->GetAttributes() & NAV_MESH_AVOID ) )
				{
					m_totalWeight += 1;
				}
				else
				{
					m_totalWeight += 2;
				}

				++m_count;
			}
		}

		return (m_count < MAX_SPOTS);
	}

	/**
	 * Remove the spot at index "i"
	 */
	void RemoveSpot( int i )
	{
		if (m_count == 0)
			return;

		for( int j=i+1; j<m_count; ++j )
			m_hidingSpot[j-1] = m_hidingSpot[j];

		--m_count;
	}


	int GetRandomHidingSpot( void )
	{
		int weight = RandomInt( 0, m_totalWeight-1 );
		for ( int i=0; i<m_count-1; ++i )
		{
			// if the next spot's starting weight is over the target weight, this spot is the one
			if ( m_hidingSpotWeight[i+1] >= weight )
			{
				return i;
			}
		}

		// if we didn't find any, it's the last one
		return m_count - 1;
	}

	CBaseEntity *m_me;
	const Vector &m_origin;
	float m_range;

	const Vector *m_hidingSpot[ MAX_SPOTS ];
	int m_hidingSpotWeight[ MAX_SPOTS ];
	int m_totalWeight;
	int m_count;

	unsigned char m_flags;

	Place m_place;
};

/**
 * Do a breadth-first search to find a nearby hiding spot and return it.
 * Don't pick a hiding spot that a Player is currently occupying.
 * @todo Clean up this mess
 */
const Vector *FindNearbyHidingSpot( CBaseEntity *me, const Vector &pos, float maxRange, bool isSniper, bool useNearest )
{
	CNavArea *startArea = TheNavMesh->GetNearestNavArea( pos );
	if (startArea == NULL)
		return NULL;

	// collect set of nearby hiding spots
	if (isSniper)
	{
		CollectHidingSpotsFunctor collector( me, pos, maxRange, HidingSpot::IDEAL_SNIPER_SPOT );
		SearchSurroundingAreas( startArea, pos, collector, maxRange );

		if (collector.m_count)
		{
			int which = collector.GetRandomHidingSpot();
			return collector.m_hidingSpot[ which ];
		}
		else
		{
			// no ideal sniping spots, look for "good" sniping spots
			CollectHidingSpotsFunctor collector( me, pos, maxRange, HidingSpot::GOOD_SNIPER_SPOT );
			SearchSurroundingAreas( startArea, pos, collector, maxRange );

			if (collector.m_count)
			{
				int which = collector.GetRandomHidingSpot();
				return collector.m_hidingSpot[ which ];
			}

			// no sniping spots at all.. fall through and pick a normal hiding spot
		}
	}

	// collect hiding spots with decent "cover"
	CollectHidingSpotsFunctor collector( me, pos, maxRange, HidingSpot::IN_COVER );
	SearchSurroundingAreas( startArea, pos, collector, maxRange );

	if (collector.m_count == 0)
	{
		// no hiding spots at all - if we're not a sniper, try to find a sniper spot to use instead
		if (!isSniper)
		{
			return FindNearbyHidingSpot( me, pos, maxRange, true, useNearest );
		}

		return NULL;
	}

	if (useNearest)
	{
		// return closest hiding spot
		const Vector *closest = NULL;
		float closeRangeSq = 9999999999.9f;
		for( int i=0; i<collector.m_count; ++i )
		{
			float rangeSq = (*collector.m_hidingSpot[i] - pos).LengthSqr();
			if (rangeSq < closeRangeSq)
			{
				closeRangeSq = rangeSq;
				closest = collector.m_hidingSpot[i];
			}
		}

		return closest;
	}

	// select a hiding spot at random
	int which = collector.GetRandomHidingSpot();
	return collector.m_hidingSpot[ which ];
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Select a random hiding spot among the nav areas that are tagged with the given place
 */
const Vector *FindRandomHidingSpot( CBaseEntity *me, Place place, bool isSniper )
{
	// collect set of nearby hiding spots
	if (isSniper)
	{
		CollectHidingSpotsFunctor collector( me, me->GetAbsOrigin(), -1.0f, HidingSpot::IDEAL_SNIPER_SPOT, place );
		TheNavMesh->ForAllAreas( collector );

		if (collector.m_count)
		{
			int which = RandomInt( 0, collector.m_count-1 );
			return collector.m_hidingSpot[ which ];
		}
		else
		{
			// no ideal sniping spots, look for "good" sniping spots
			CollectHidingSpotsFunctor collector( me, me->GetAbsOrigin(), -1.0f, HidingSpot::GOOD_SNIPER_SPOT, place );
			TheNavMesh->ForAllAreas( collector );

			if (collector.m_count)
			{
				int which = RandomInt( 0, collector.m_count-1 );
				return collector.m_hidingSpot[ which ];
			}

			// no sniping spots at all.. fall through and pick a normal hiding spot
		}
	}

	// collect hiding spots with decent "cover"
	CollectHidingSpotsFunctor collector( me, me->GetAbsOrigin(), -1.0f, HidingSpot::IN_COVER, place );
	TheNavMesh->ForAllAreas( collector );

	if (collector.m_count == 0)
		return NULL;

	// select a hiding spot at random
	int which = RandomInt( 0, collector.m_count-1 );
	return collector.m_hidingSpot[ which ];
}


//--------------------------------------------------------------------------------------------------------------------
/**
 * Select a nearby retreat spot.
 * Don't pick a hiding spot that a Player is currently occupying.
 * If "avoidTeam" is nonzero, avoid getting close to members of that team.
 */
const Vector *FindNearbyRetreatSpot( CBaseEntity *me, const Vector &start, float maxRange, int avoidTeam )
{
	CNavArea *startArea = TheNavMesh->GetNearestNavArea( start );
	if (startArea == NULL)
		return NULL;

	// collect hiding spots with decent "cover"
	CollectHidingSpotsFunctor collector( me, start, maxRange, HidingSpot::IN_COVER );
	SearchSurroundingAreas( startArea, start, collector, maxRange );

	if (collector.m_count == 0)
		return NULL;

	// find the closest unoccupied hiding spot that crosses the least lines of fire and has the best cover
	for( int i=0; i<collector.m_count; ++i )
	{
		// check if we would have to cross a line of fire to reach this hiding spot
		if (IsCrossingLineOfFire( start, *collector.m_hidingSpot[i], me ))
		{
			collector.RemoveSpot( i );

			// back up a step, so iteration won't skip a spot
			--i;

			continue;
		}

		// check if there is someone on the avoidTeam near this hiding spot
		if (avoidTeam)
		{
			float range;
			if (UTIL_GetClosestPlayer( *collector.m_hidingSpot[i], avoidTeam, &range ))
			{
				const float dangerRange = 150.0f;
				if (range < dangerRange)
				{
					// there is an avoidable player too near this spot - remove it
					collector.RemoveSpot( i );

					// back up a step, so iteration won't skip a spot
					--i;

					continue;
				}
			}
		}
	}

	if (collector.m_count <= 0)
		return NULL;

	// all remaining spots are ok - pick one at random
	int which = RandomInt( 0, collector.m_count-1 );
	return collector.m_hidingSpot[ which ];
}


//--------------------------------------------------------------------------------------------------------------------
/**
 * Functor to collect all hiding spots in range that we can reach before the enemy arrives.
 * NOTE: This only works for the initial rush.
 */
class CollectArriveFirstSpotsFunctor
{
public:
	CollectArriveFirstSpotsFunctor( CBaseEntity *me, const Vector &searchOrigin, float enemyArriveTime, float range, int flags ) : m_searchOrigin( searchOrigin )
	{
		m_me = me;
		m_count = 0;
		m_range = range;
		m_flags = (unsigned char)flags;
		m_enemyArriveTime = enemyArriveTime;
	}

	enum { MAX_SPOTS = 256 };

	bool operator() ( CNavArea *area )
	{
		// collect all the hiding spots in this area
		const HidingSpotVector *pSpots = area->GetHidingSpots();
		
		FOR_EACH_VEC( (*pSpots), it )
		{
			const HidingSpot *spot = (*pSpots)[ it ];

			// make sure hiding spot is in range
			if (m_range > 0.0f)
			{
				if ((spot->GetPosition() - m_searchOrigin).IsLengthGreaterThan( m_range ))
				{
					continue;
				}
			}

			// if a Player is using this hiding spot, don't consider it
			if (IsSpotOccupied( m_me, spot->GetPosition() ))
			{
				// player is in hiding spot
				/// @todo Check if player is moving or sitting still
				continue;
			}

			// only collect hiding spots with matching flags
			if (!(m_flags & spot->GetFlags()))
			{
				continue;
			}

			// only collect this hiding spot if we can reach it before the enemy arrives
			// NOTE: This assumes the area is fairly small and the difference of moving to the corner vs the center is small
			const float settleTime = 1.0f;
			if (spot->GetArea()->GetEarliestOccupyTime( m_me->GetTeamNumber() ) + settleTime < m_enemyArriveTime)
			{
				m_hidingSpot[ m_count++ ] = spot;
			}
		}

		// if we've filled up, stop searching
		if (m_count == MAX_SPOTS)
			return false;

		return true;
	}

	CBaseEntity *m_me;
	const Vector &m_searchOrigin;

	float m_range;
	float m_enemyArriveTime;
	unsigned char m_flags;

	const HidingSpot *m_hidingSpot[ MAX_SPOTS ];
	int m_count;
};


/**
 * Select a hiding spot that we can reach before the enemy arrives.
 * NOTE: This only works for the initial rush.
 */
const HidingSpot *FindInitialEncounterSpot( CBaseEntity *me, const Vector &searchOrigin, float enemyArriveTime, float maxRange, bool isSniper )
{
	CNavArea *startArea = TheNavMesh->GetNearestNavArea( searchOrigin );
	if (startArea == NULL)
		return NULL;

	// collect set of nearby hiding spots
	if (isSniper)
	{
		CollectArriveFirstSpotsFunctor collector( me, searchOrigin, enemyArriveTime, maxRange, HidingSpot::IDEAL_SNIPER_SPOT );
		SearchSurroundingAreas( startArea, searchOrigin, collector, maxRange );

		if (collector.m_count)
		{
			int which = RandomInt( 0, collector.m_count-1 );
			return collector.m_hidingSpot[ which ];
		}
		else
		{
			// no ideal sniping spots, look for "good" sniping spots
			CollectArriveFirstSpotsFunctor collector( me, searchOrigin, enemyArriveTime, maxRange, HidingSpot::GOOD_SNIPER_SPOT );
			SearchSurroundingAreas( startArea, searchOrigin, collector, maxRange );

			if (collector.m_count)
			{
				int which = RandomInt( 0, collector.m_count-1 );
				return collector.m_hidingSpot[ which ];
			}

			// no sniping spots at all.. fall through and pick a normal hiding spot
		}
	}

	// collect hiding spots with decent "cover"
	CollectArriveFirstSpotsFunctor collector( me, searchOrigin, enemyArriveTime, maxRange, HidingSpot::IN_COVER | HidingSpot::EXPOSED );
	SearchSurroundingAreas( startArea, searchOrigin, collector, maxRange );

	if (collector.m_count == 0)
		return NULL;

	// select a hiding spot at random
	int which = RandomInt( 0, collector.m_count-1 );
	return collector.m_hidingSpot[ which ];
}

