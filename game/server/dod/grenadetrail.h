//========= Copyright Valve Corporation, All rights reserved. ============//
#ifndef GRENADE_TRAIL_H
#define GRENADE_TRAIL_H

#include "baseparticleentity.h"

//==================================================
// SmokeTrail
//==================================================

class CGrenadeTrail : public CBaseParticleEntity
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CGrenadeTrail, CBaseParticleEntity );
	DECLARE_SERVERCLASS();

	CGrenadeTrail();
	void					SetEmit(bool bVal);
	void					FollowEntity( CBaseEntity *pEntity, const char *pAttachmentName = NULL);
	static	CGrenadeTrail*	CreateGrenadeTrail();

public:
	CNetworkVar( float, m_SpawnRate );			// How many particles per second.
	CNetworkVar( float, m_ParticleLifetime );		// How long do the particles live?
	CNetworkVar( bool,  m_bEmit );
	CNetworkVar( float, m_StopEmitTime );			// When do I stop emitting particles?
	CNetworkVar( int,   m_nAttachment );
};

#endif //GRENADE_TRAIL_H
