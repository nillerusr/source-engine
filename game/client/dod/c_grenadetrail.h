//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// This defines the client-side SmokeTrail entity. It can also be used without
// an entity, in which case you must pass calls to it and set its position each frame.

#ifndef PARTICLE_SMOKETRAIL_H
#define PARTICLE_SMOKETRAIL_H

#include "particlemgr.h"
#include "particle_prototype.h"
#include "particle_util.h"
#include "particles_simple.h"
#include "c_baseentity.h"
#include "baseparticleentity.h"

#include "fx_trail.h"

//
// Smoke Trail
//

class C_GrenadeTrail : public C_BaseParticleEntity, public IPrototypeAppEffect
{
public:
	DECLARE_CLASS( C_GrenadeTrail, C_BaseParticleEntity );
	DECLARE_CLIENTCLASS();
	
					C_GrenadeTrail();
	virtual			~C_GrenadeTrail();

public:

	//For attachments
	void			GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pAbsOrigin, QAngle *pAbsAngles );

	// Enable/disable emission.
	void			SetEmit(bool bEmit);

	// Change the spawn rate.
	void			SetSpawnRate(float rate);

// C_BaseEntity.
public:
	virtual	void	OnDataChanged(DataUpdateType_t updateType);

// IPrototypeAppEffect.
public:
	virtual void	Start(CParticleMgr *pParticleMgr, IPrototypeArgAccess *pArgs);

// IParticleEffect.
public:
	virtual void	Update(float fTimeDelta);
	virtual void RenderParticles( CParticleRenderIterator *pIterator );
	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );


public:
	// Effect parameters. These will assume default values but you can change them.
	float			m_SpawnRate;			// How many particles per second.

	Vector			m_StartColor;			// Fade between these colors.
	Vector			m_EndColor;
	float			m_Opacity;

	float			m_ParticleLifetime;		// How long do the particles live?
	float			m_StopEmitTime;			// When do I stop emitting particles? (-1 = never)
	
	float			m_MinSpeed;				// Speed range.
	float			m_MaxSpeed;
	
	float			m_MinDirectedSpeed;		// Directed speed range.
	float			m_MaxDirectedSpeed;

	float			m_StartSize;			// Size ramp.
	float			m_EndSize;

	float			m_SpawnRadius;

	Vector			m_VelocityOffset;		// Emit the particles in a certain direction.

	bool			m_bEmit;				// Keep emitting particles?

	int				m_nAttachment;

private:
	PMaterialHandle	m_MaterialHandle[2];
	TimedEvent		m_ParticleSpawn;

	CParticleMgr	*m_pParticleMgr;
	CSmartPtr<CSimpleEmitter> m_pSmokeEmitter;

	C_GrenadeTrail( const C_GrenadeTrail & );
};

#endif //PARTICLE_SMOKETRAIL_H