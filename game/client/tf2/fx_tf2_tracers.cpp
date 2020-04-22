//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game-specific impact effect hooks
//
//=============================================================================//
#include "cbase.h"
#include "fx.h"
#include "c_te_effect_dispatch.h"
#include "clienteffectprecachesystem.h"
#include "clientsideeffects.h"

// Precache the effects
CLIENTEFFECT_REGISTER_BEGIN( PrecacheTF2TracerEffects )
CLIENTEFFECT_MATERIAL( "effects/human_bullet" )
CLIENTEFFECT_MATERIAL( "effects/alien_laser" )
CLIENTEFFECT_REGISTER_END()

Vector GetTracerOrigin( const CEffectData &data );

//-----------------------------------------------------------------------------
// Purpose: Human's laser rifle Tracer
//-----------------------------------------------------------------------------
void HLaserTracerCallback( const CEffectData &data )
{
	float flVelocity = data.m_flScale;
	Vector vecStart = GetTracerOrigin( data );
	Vector vecEnd = data.m_vOrigin;
	Vector shotDir;

	// Get out shot direction and length
	VectorSubtract( vecEnd, vecStart, shotDir );
	float totalDist = VectorNormalize( shotDir );

	// Don't make small tracers
	if ( totalDist <= 32 )
		return;

	float length = MAX( 64, random->RandomFloat( 200.0f, 256.0f ) );
	flVelocity = random->RandomFloat( 5000, 7000 );
	float life = ( totalDist + length ) / flVelocity;
	float flWidth = random->RandomFloat( 2.0, 2.5 );
	
	// Add it
	FX_AddDiscreetLine( vecStart, shotDir, flVelocity, length, totalDist, flWidth, life, "effects/human_bullet" );
}

DECLARE_CLIENT_EFFECT( "HLaserTracer", HLaserTracerCallback );

//-----------------------------------------------------------------------------
// Purpose: Alien's laser rifle Tracer
//-----------------------------------------------------------------------------
void ALaserTracerCallback( const CEffectData &data )
{
	float flVelocity = data.m_flScale;
	Vector vecStart = GetTracerOrigin( data );
	Vector vecEnd = data.m_vOrigin;
	Vector shotDir;

	// Get out shot direction and length
	VectorSubtract( vecEnd, vecStart, shotDir );
	float totalDist = VectorNormalize( shotDir );

	// Don't make small tracers
	if ( totalDist <= 32 )
		return;

	float length = MAX( 64, random->RandomFloat( 512.0f, 768.0f ) );
	flVelocity = random->RandomFloat( 5000, 7000 );
	float life = ( totalDist + length ) / flVelocity;
	float flWidth = random->RandomFloat( 2.0, 3.0 );
	
	// Add it
	FX_AddDiscreetLine( vecStart, shotDir, flVelocity, length, totalDist, flWidth, life, "effects/alien_laser" );
}

DECLARE_CLIENT_EFFECT( "ALaserTracer", ALaserTracerCallback );

//-----------------------------------------------------------------------------
// Purpose: Alien's minigun tracer
//-----------------------------------------------------------------------------
void MinigunTracerCallback( const CEffectData &data )
{
	float flVelocity = data.m_flScale;
	Vector vecStart = GetTracerOrigin( data );
	Vector vecEnd = data.m_vOrigin;
	Vector shotDir;

	// Get out shot direction and length
	VectorSubtract( vecEnd, vecStart, shotDir );
	float totalDist = VectorNormalize( shotDir );

	// Don't make small tracers
	if ( totalDist <= 32 )
		return;

	float length = MAX( 64, random->RandomFloat( 200.0f, 256.0f ) );
	flVelocity = random->RandomFloat( 5000, 7000 );
	float life = ( totalDist + length ) / flVelocity;
	float flWidth = random->RandomFloat( 1.5, 2.0 );
	
	// Add it
	FX_AddDiscreetLine( vecStart, shotDir, flVelocity, length, totalDist, flWidth, life, "effects/alien_laser" );
}

DECLARE_CLIENT_EFFECT( "MinigunTracer", MinigunTracerCallback );
