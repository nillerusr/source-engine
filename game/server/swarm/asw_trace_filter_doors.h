#ifndef _INCLUDED_ASW_TRACE_FILTER_DOORS
#define _INCLUDED_ASW_TRACE_FILTER_DOORS
#pragma once

#include "basecombatcharacter.h"

class CASW_Door;
struct AI_Waypoint_t;

class CASW_Trace_Filter_Doors : public CTraceFilterEntitiesOnly
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS_NOBASE( CASW_Trace_Filter_Doors );

	CASW_Trace_Filter_Doors( const IHandleEntity *passentity, int collisionGroup, bool bRequireLockedOrSealed )
		: m_pPassEnt(passentity), m_collisionGroup(collisionGroup), m_pHit(NULL), m_bRequireLockedOrSealed(bRequireLockedOrSealed)
	{		
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );

public:
	const IHandleEntity *m_pPassEnt;
	int					m_collisionGroup;
	CBaseEntity			*m_pHit;
	bool				m_bRequireLockedOrSealed;
};

CASW_Door* UTIL_ASW_DoorBlockingRoute( AI_Waypoint_t *pRoute, bool bRequireLockedOrSealed );
bool UTIL_ASW_BrushBlockingRoute( AI_Waypoint_t *pRoute, const int nCollisionMask, const int nCollisionGroup );

#endif // _INCLUDED_ASW_TRACE_FILTER_DOORS