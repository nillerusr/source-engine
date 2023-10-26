#ifndef _INCLUDED_ASW_TRACE_FILTER_DOOR_CRUSH_H
#define _INCLUDED_ASW_TRACE_FILTER_DOOR_CRUSH_H
#pragma once

#include "basecombatcharacter.h"

class CASW_Trace_Filter_Door_Crush : public CTraceFilterEntitiesOnly
{
public:
	// It does have a base, but we'll never network anything below here..
	DECLARE_CLASS_NOBASE( CASW_Trace_Filter_Door_Crush );
	
	CASW_Trace_Filter_Door_Crush( const IHandleEntity *passentity, int collisionGroup, CTakeDamageInfo *dmgInfo, float flForceScale, bool bDamageAnyNPC )
		: m_pPassEnt(passentity), m_collisionGroup(collisionGroup), m_dmgInfo(dmgInfo), m_pHit(NULL), m_flForceScale(flForceScale), m_bDamageAnyNPC(bDamageAnyNPC)
	{
	}
	
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask );

public:
	const IHandleEntity *m_pPassEnt;
	int					m_collisionGroup;
	CTakeDamageInfo		*m_dmgInfo;
	CBaseEntity			*m_pHit;
	float				m_flForceScale;
	bool				m_bDamageAnyNPC;
};

#endif // _INCLUDED_ASW_TRACE_FILTER_DOOR_CRUSH_H