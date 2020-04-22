//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// nav_generate.cpp
// Auto-generate a Navigation Mesh by sampling the current map
// Author: Michael S. Booth (mike@turtlerockstudios.com), 2003

#include "cbase.h"
#include "util_shared.h"
#include "nav_mesh.h"
#include "cs_nav_area.h"
#include "cs_nav_node.h"
#include "cs_nav_pathfind.h"
#include "viewport_panel_names.h"

enum { MAX_BLOCKED_AREAS = 256 };
static unsigned int blockedID[ MAX_BLOCKED_AREAS ];
static int blockedIDCount = 0;
static float lastMsgTime = 0.0f;


//ConVar nav_slope_limit( "nav_slope_limit", "0.7", FCVAR_GAMEDLL, "The ground unit normal's Z component must be greater than this for nav areas to be generated." );
ConVar nav_restart_after_analysis( "nav_restart_after_analysis", "1", FCVAR_GAMEDLL, "When nav nav_restart_after_analysis finishes, restart the server.  Turning this off can cause crashes, but is useful for incremental generation." );


//--------------------------------------------------------------------------------------------------------------
/**
 * Shortest path cost, paying attention to "blocked" areas
 */
class ApproachAreaCost
{
public:
	// HPE_TODO[pmf]: check that these new parameters are okay to be ignored
	float operator() ( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder, const CFuncElevator *elevator, float length )
	{
		// check if this area is "blocked"
		for( int i=0; i<blockedIDCount; ++i )
		{
			if (area->GetID() == blockedID[i])
			{
				return -1.0f;
			}
		}

		if (fromArea == NULL)
		{
			// first area in path, no cost
			return 0.0f;
		}
		else
		{
			// compute distance traveled along path so far
			float dist;

			if (ladder)
			{
				dist = ladder->m_length;
			}
			else
			{
				dist = (area->GetCenter() - fromArea->GetCenter()).Length();
			}

			float cost = dist + fromArea->GetCostSoFar();

			return cost;
		}
	}
};

/*
 * Determine the set of "approach areas".
 * An approach area is an area representing a place where players 
 * move into/out of our local neighborhood of areas.
 * @todo Optimize by search from eye outward and modifying pathfinder to treat all links as bi-directional
 */
void CCSNavArea::ComputeApproachAreas( void )
{
	m_approachCount = 0;

	if (nav_quicksave.GetBool())
		return;

	// use the center of the nav area as the "view" point
	Vector eye = m_center;
	if (TheNavMesh->GetGroundHeight( eye, &eye.z ) == false)
		return;

	// approximate eye position
	if (GetAttributes() & NAV_MESH_CROUCH)
		eye.z += 0.9f * HalfHumanHeight;
	else
		eye.z += 0.9f * HumanHeight;

	enum { MAX_PATH_LENGTH = 256 };
	CNavArea *path[ MAX_PATH_LENGTH ];
	ApproachAreaCost cost;

	enum SearchType
	{
		FROM_EYE,		///< start search from our eyepoint outward to farArea
		TO_EYE,			///< start search from farArea beack towards our eye
		SEARCH_FINISHED
	};

	//
	// In order to *completely* enumerate all of the approach areas, we
	// need to search from our eyepoint outward, as well as from outwards
	// towards our eyepoint
	//
	for( int searchType = FROM_EYE; searchType != SEARCH_FINISHED; ++searchType )
	{
		//
		// In order to enumerate all of the approach areas, we need to
		// run the algorithm many times, once for each "far away" area
		// and keep the union of the approach area sets
		//
		int it;
		for( it = 0; it < TheNavAreas.Count(); ++it )
		{
			CNavArea *farArea = TheNavAreas[ it ];

			blockedIDCount = 0;

			// skip the small areas
			const float minSize = 200.0f;		// 150
			Extent extent;
			farArea->GetExtent(&extent);
			if (extent.SizeX() < minSize || extent.SizeY() < minSize)
			{
				continue;
			}

			// if we can see 'farArea', try again - the whole point is to go "around the bend", so to speak
			if (farArea->IsVisible( eye ))
			{
				continue;
			}

			//
			// Keep building paths to farArea and blocking them off until we
			// cant path there any more.
			// As areas are blocked off, all exits will be enumerated.
			//
			while( m_approachCount < MAX_APPROACH_AREAS )
			{
				CNavArea *from, *to;

				if (searchType == FROM_EYE)
				{
					// find another path *to* 'farArea'
					// we must pathfind from us in order to pick up one-way paths OUT OF our area
					from = this;
					to = farArea;
				}
				else // TO_EYE
				{
					// find another path *from* 'farArea'
					// we must pathfind to us in order to pick up one-way paths INTO our area
					from = farArea;
					to = this;
				}

				// build the actual path
				if (NavAreaBuildPath( from, to, NULL, cost ) == false)
				{
					break;
				}

				// find number of areas on path
				int count = 0;
				CNavArea *area;
				for( area = to; area; area = area->GetParent() )
				{
					++count;
				}

				if (count > MAX_PATH_LENGTH)
				{
					count = MAX_PATH_LENGTH;
				}

				// if the path is only two areas long, there can be no approach points
				if (count <= 2)
				{
					break;
				}

				// build path starting from eye
				int i = 0;

				if (searchType == FROM_EYE)
				{
					for( area = to; i < count && area; area = area->GetParent() )
					{
						path[ count-i-1 ] = area;
						++i;
					}
				}
				else // TO_EYE
				{
					for( area = to; i < count && area; area = area->GetParent() )
					{
						path[ i++ ] = area;
					}
				}

				// traverse path to find first area we cannot see (skip the first area)
				for( i=1; i<count; ++i )
				{
					// if we see this area, continue on
					if (path[i]->IsVisible( eye ))
					{
						continue;
					}

					// we can't see this area - mark this area as "blocked" and unusable by subsequent approach paths
					if (blockedIDCount == MAX_BLOCKED_AREAS)
					{
						Msg( "Overflow computing approach areas for area #%d.\n", GetID());
						return;
					}

					// if the area to be blocked is actually farArea, block the one just prior
					// (blocking farArea will cause all subsequent pathfinds to fail)
					int block = (path[i] == farArea) ? i-1 : i;

					// dont block the start area, or all subsequence pathfinds will fail
					if (block == 0)
					{
						continue;
					}

					blockedID[ blockedIDCount++ ] = path[ block ]->GetID();

					// store new approach area if not already in set
					int a;
					for( a=0; a<m_approachCount; ++a )
					{
						if (m_approach[a].here.area == path[block-1])
						{
							break;
						}
					}

					if (a == m_approachCount)
					{
						m_approach[ m_approachCount ].prev.area = (block >= 2) ? path[block-2] : NULL;

						m_approach[ m_approachCount ].here.area = path[block-1];
						m_approach[ m_approachCount ].prevToHereHow = path[block-1]->GetParentHow();

						m_approach[ m_approachCount ].next.area = path[block];
						m_approach[ m_approachCount ].hereToNextHow = path[block]->GetParentHow();

						++m_approachCount;
					}

					// we are done with this path
					break;
				}
			}
		}
	}
}
