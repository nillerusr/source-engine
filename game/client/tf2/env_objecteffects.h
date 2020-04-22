//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ENV_OBJECTEFFECTS_H
#define ENV_OBJECTEFFECTS_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Object smoke particles. They float upward.
//-----------------------------------------------------------------------------
class ObjectSmokeParticle : public SimpleParticle
{
public:
	Vector		m_vecAcceleration;
};

//-----------------------------------------------------------------------------
// Purpose: Object smoke particle emitter.
//-----------------------------------------------------------------------------
class CObjectSmokeParticles : public CSimpleEmitter
{
public:

	CObjectSmokeParticles( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}
	static CSmartPtr<CObjectSmokeParticles> Create( const char *pDebugName )	{return new CObjectSmokeParticles( pDebugName );}

	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );
	virtual void RenderParticles( CParticleRenderIterator *pIterator );

	//Setup for point emission
	virtual void		Setup( const Vector &origin, const Vector *direction, float angularSpread, float minSpeed, float maxSpeed, float gravity, float dampen, int flags = 0 );

	CParticleCollision m_ParticleCollision;

private:
	CObjectSmokeParticles( const CObjectSmokeParticles & ); // not defined, not accessible
};

//-----------------------------------------------------------------------------
// Purpose: Object fire particles. They know how to attach themselves to heirarchy.
//-----------------------------------------------------------------------------
class ObjectFireParticle : public SimpleParticle
{
public:
	EHANDLE		m_hParent;
	int			m_iAttachmentPoint;
};

//-----------------------------------------------------------------------------
// Purpose: Object smoke particle emitter.
//-----------------------------------------------------------------------------
class CObjectFireParticles : public CSimpleEmitter
{
public:
	CObjectFireParticles( const char *pDebugName ) : CSimpleEmitter( pDebugName ) {}
	static CSmartPtr<CObjectFireParticles> Create( const char *pDebugName )	{return new CObjectFireParticles( pDebugName );}

	virtual void SimulateParticles( CParticleSimulateIterator *pIterator );
	virtual void RenderParticles( CParticleRenderIterator *pIterator );

	//Setup for point emission
	virtual void		Setup( const Vector &origin, const Vector *direction, float angularSpread, float minSpeed, float maxSpeed, float gravity, float dampen, int flags = 0 );

private:
	CObjectFireParticles( const CObjectFireParticles & ); // not defined, not accessible
};

#endif // ENV_OBJECTEFFECTS_H
