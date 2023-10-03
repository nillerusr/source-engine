#ifndef _INCLUDED_ASW_PATHFINDER_H
#define _INCLUDED_ASW_PATHFINDER_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_basenpc.h"

class CASW_Path_Utils_NPC;

class CASW_Path_Utils
{
public:
	AI_Waypoint_t *BuildRoute( const Vector &vStart, const Vector &vEnd, CBaseEntity *pTarget, float goalTolerance, Navigation_t curNavType = NAV_NONE, int nBuildFlags = 0 );
	void DeleteRoute( AI_Waypoint_t *pWaypointList );
	AI_Waypoint_t *GetLastRoute() { return m_pLastRoute; }
	void DebugDrawRoute( const Vector &vecStartPos, AI_Waypoint_t *pWaypoint );

private:
	CASW_Path_Utils_NPC* GetPathfinderNPC();

	CHandle<CASW_Path_Utils_NPC> m_hPathfinderNPC;
	AI_Waypoint_t *m_pLastRoute;
};

CASW_Path_Utils* ASWPathUtils();

#endif // _INCLUDED_ASW_PATHFINDER_H