//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: particle system code
//
//===========================================================================//

#include "tier0/platform.h"
#include "particles/particles.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
void AddBuiltInParticleOperators( void );
void AddBuiltInParticleRenderers( void );
void AddBuiltInParticleInitializers( void );
void AddBuiltInParticleEmitters( void );
void AddBuiltInParticleForceGenerators( void );
void AddBuiltInParticleConstraints( void );

void CParticleSystemMgr::AddBuiltinSimulationOperators( void )
{
	static bool s_DidAddSim = false;
	if ( ! s_DidAddSim )
	{
		s_DidAddSim = true;
		AddBuiltInParticleOperators();
		AddBuiltInParticleInitializers();
		AddBuiltInParticleEmitters();
		AddBuiltInParticleForceGenerators();
		AddBuiltInParticleConstraints();
	}
}

void CParticleSystemMgr::AddBuiltinRenderingOperators( void )
{
	static bool s_DidAddRenderers = false;
	if ( ! s_DidAddRenderers )
	{
		s_DidAddRenderers = true;
		AddBuiltInParticleRenderers();
	}
}
