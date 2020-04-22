//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: particle system code
//
//===========================================================================//

#include "tier0/platform.h"
#include "particles/particles.h"
#include "filesystem.h"
#include "tier2/tier2.h"
#include "tier2/fileutils.h"
#include "tier1/UtlStringMap.h"
#include "tier1/strtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern int g_nParticle_Multiplier;

//-----------------------------------------------------------------------------
// Emits particles immediately
//-----------------------------------------------------------------------------
struct InstantaneousEmitterContext_t
{
	int m_nRemainingParticles;
	int m_ActualParticlesToEmit;
	float m_flTimeOffset;
	bool m_bReadScaleFactor;
};

class C_OP_InstantaneousEmitter : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_InstantaneousEmitter );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	virtual uint64 GetReadControlPointMask() const 
	{
		if ( m_nScaleControlPoint >= 0 )
			return ( 1ULL << m_nScaleControlPoint );
		return 0; 
	}

	virtual uint32 Emit( CParticleCollection *pParticles, float flCurStrength, 
						 void *pContext ) const;

	// unpack structure will be applied by creator. add extra initialization needed here
	virtual void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		if ( m_nMinParticlesToEmit >= 0 )
		{
			if ( m_nMinParticlesToEmit > m_nParticlesToEmit )
			{
				V_swap( m_nParticlesToEmit, m_nMinParticlesToEmit );
			}
		}

		if ( m_nPerFrameNum < 0 )
		{
			m_nPerFrameNum = INT_MAX;
		}
		m_nScaleControlPointField = clamp( m_nScaleControlPointField, 0, 2 );
	}

	virtual void StopEmission( CParticleCollection *pParticles, void *pContext, bool bInfiniteOnly ) const
	{
		InstantaneousEmitterContext_t *pCtx = reinterpret_cast<InstantaneousEmitterContext_t *>( pContext );
		if ( !bInfiniteOnly )
		{
			pCtx->m_nRemainingParticles = 0;
		}
	}
	virtual void StartEmission( CParticleCollection *pParticles, void *pContext, bool bInfiniteOnly ) const
	{
		InstantaneousEmitterContext_t *pCtx = reinterpret_cast<InstantaneousEmitterContext_t *>( pContext );
		if ( !bInfiniteOnly )
		{
			pCtx->m_nRemainingParticles = pCtx->m_ActualParticlesToEmit;
			SkipToTime( pParticles->m_flCurTime, pParticles, pCtx );
		}
	}

	// Called when the SFM wants to skip forward in time
	virtual void SkipToTime( float flTime, CParticleCollection *pParticles, void *pContext ) const
	{
		// NOTE: This is a bit of a hack. We're saying that if we're skipping more than two seconds, that we're
		//		 probably not going to bother emitting at all.  Really, this would have to know the maximum 
		//		 lifetime of the child particles and only skip if past that.

		InstantaneousEmitterContext_t *pCtx = reinterpret_cast<InstantaneousEmitterContext_t *>( pContext );
		float flStartTime = m_flStartTime + pCtx->m_flTimeOffset;
		if ( flTime > ( flStartTime + 2.0f ) )
		{
			pCtx->m_nRemainingParticles = 0;
		}
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		InstantaneousEmitterContext_t *pCtx = reinterpret_cast<InstantaneousEmitterContext_t *>( pContext );
		if ( m_nMinParticlesToEmit >= 0 )
		{
			pCtx->m_ActualParticlesToEmit = pParticles->RandomInt( m_nMinParticlesToEmit, m_nParticlesToEmit );
		}
		else
		{
			pCtx->m_ActualParticlesToEmit = m_nParticlesToEmit;
		}
		pCtx->m_nRemainingParticles = pCtx->m_ActualParticlesToEmit;
		pCtx->m_flTimeOffset = 0.0f;
		pCtx->m_bReadScaleFactor = false;
	}

	virtual void Restart( CParticleCollection *pParticles, void *pContext )
	{
		InstantaneousEmitterContext_t *pCtx = reinterpret_cast<InstantaneousEmitterContext_t *>( pContext );
		pCtx->m_nRemainingParticles = pCtx->m_ActualParticlesToEmit;
		pCtx->m_flTimeOffset = pParticles->m_flCurTime;
		pCtx->m_bReadScaleFactor = false;
	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( InstantaneousEmitterContext_t );
	}

	virtual bool MayCreateMoreParticles( CParticleCollection *pParticles, void *pContext ) const
	{
		InstantaneousEmitterContext_t *pCtx = reinterpret_cast<InstantaneousEmitterContext_t *>( pContext );
		return !(pCtx->m_nRemainingParticles <= 0);
	}

	int m_nParticlesToEmit;
	int m_nMinParticlesToEmit;
	float m_flStartTime;
	int m_nPerFrameNum;
	int m_nScaleControlPoint;
	int m_nScaleControlPointField;
};

DEFINE_PARTICLE_OPERATOR( C_OP_InstantaneousEmitter, "emit_instantaneously", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_InstantaneousEmitter ) 
	DMXELEMENT_UNPACK_FIELD( "emission_start_time", "0", float, m_flStartTime )
	DMXELEMENT_UNPACK_FIELD( "num_to_emit_minimum", "-1", int, m_nMinParticlesToEmit )
	DMXELEMENT_UNPACK_FIELD( "num_to_emit", "100", int, m_nParticlesToEmit )
	DMXELEMENT_UNPACK_FIELD( "maximum emission per frame", "-1", int, m_nPerFrameNum )
	DMXELEMENT_UNPACK_FIELD( "emission count scale control point", "-1", int, m_nScaleControlPoint )
	DMXELEMENT_UNPACK_FIELD( "emission count scale control point field", "0", int, m_nScaleControlPointField )
END_PARTICLE_OPERATOR_UNPACK( C_OP_InstantaneousEmitter )


uint32 C_OP_InstantaneousEmitter::Emit( CParticleCollection *pParticles, float flCurStrength, 
										void *pContext ) const
{
	// Don't emit any more if the particle system has emitted all it's supposed to.
	InstantaneousEmitterContext_t *pCtx = reinterpret_cast<InstantaneousEmitterContext_t *>( pContext );
	if ( pCtx->m_nRemainingParticles <= 0 )
		return 0;

	// Wait until we're told to start emitting
	float flStartTime = m_flStartTime + pCtx->m_flTimeOffset;
	if ( pParticles->m_flCurTime < flStartTime )
		return 0;

	if ( pCtx->m_ActualParticlesToEmit == 0 )
		return 0;

	if ( ( m_nScaleControlPoint >= 0 ) && !pCtx->m_bReadScaleFactor )
	{
		Vector vecScale;
		if ( flStartTime <= pParticles->m_flCurTime && flStartTime >= pParticles->m_flCurTime - pParticles->m_flPreviousDt )
		{
			pParticles->GetControlPointAtTime( m_nScaleControlPoint, flStartTime, &vecScale );
		}
		else
		{
			pParticles->GetControlPointAtPrevTime( m_nScaleControlPoint, &vecScale );
		}

		pCtx->m_ActualParticlesToEmit *= vecScale[m_nScaleControlPointField];
		pCtx->m_nRemainingParticles *= vecScale[m_nScaleControlPointField];
		pCtx->m_bReadScaleFactor = true;
	}

	pCtx->m_nRemainingParticles = max( pCtx->m_nRemainingParticles, 0 );

	// NOTE: Applying the scale here because I don't believe we can sample the control point
	// values inside 
	// We're only allowed to emit so many particles, though..
	// If we run out of room, only emit the last N particles
	int nAllowedParticlesToEmit = pParticles->m_nMaxAllowedParticles - pParticles->m_nActiveParticles;
	// Cap to the maximum emission per frame
	int nParticlesThisFrame = min( m_nPerFrameNum, pCtx->m_nRemainingParticles );
	nAllowedParticlesToEmit = min( nAllowedParticlesToEmit, nParticlesThisFrame );
	int nActualParticlesToEmit = min( nAllowedParticlesToEmit, pCtx->m_ActualParticlesToEmit * g_nParticle_Multiplier );
	pCtx->m_nRemainingParticles -= nParticlesThisFrame;
	Assert( pCtx->m_nRemainingParticles >= 0 );

	if ( nActualParticlesToEmit == 0 )
		return 0;
	
	int nStartParticle = pParticles->m_nActiveParticles;
	pParticles->SetNActiveParticles( nActualParticlesToEmit + pParticles->m_nActiveParticles );

	// !! speed!! do sse init here
	for( int i = nStartParticle; i < nStartParticle + nActualParticlesToEmit; i++ )
	{
		float *pTimeStamp = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_CREATION_TIME, i );
		*pTimeStamp = flStartTime;
	}

	return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
}


//-----------------------------------------------------------------------------
// Emits particles over time
//-----------------------------------------------------------------------------
struct ContinuousEmitterContext_t
{
	float	m_flTotalActualParticlesSoFar;
	int		m_nTotalEmittedSoFar;
	float	m_flNextEmitTime;
	float	m_flTimeOffset;
	bool	m_bStoppedEmission;
};
	  
bool g_bDontMakeSkipToTimeTakeForever = false;


class C_OP_ContinuousEmitter : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_ContinuousEmitter );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		if ( m_flEmitRate < 0.0f )
		{
			m_flEmitRate = 0.0f;
		}
		if ( m_flEmissionDuration < 0.0f )
		{
			m_flEmissionDuration = 0.0f;
		}
		m_flEmitRate *= g_nParticle_Multiplier;
	}

	virtual uint32 Emit( CParticleCollection *pParticles, float flCurStrength,
						 void *pContext ) const ;

	inline bool IsInfinitelyEmitting() const
	{
		return ( m_flEmissionDuration == 0.0f );
	}

	virtual void StopEmission( CParticleCollection *pParticles, void *pContext, bool bInfiniteOnly ) const
	{
		ContinuousEmitterContext_t *pCtx = reinterpret_cast<ContinuousEmitterContext_t *>( pContext );
		if ( !bInfiniteOnly || IsInfinitelyEmitting() )
		{
			pCtx->m_bStoppedEmission = true;
		}
	}
	virtual void StartEmission( CParticleCollection *pParticles, void *pContext, bool bInfiniteOnly ) const
	{
		ContinuousEmitterContext_t *pCtx = reinterpret_cast<ContinuousEmitterContext_t *>( pContext );
		if ( !bInfiniteOnly || IsInfinitelyEmitting() )
		{
			pCtx->m_bStoppedEmission = false;
			SkipToTime( pParticles->m_flCurTime, pParticles, pCtx );
		}
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		ContinuousEmitterContext_t *pCtx = reinterpret_cast<ContinuousEmitterContext_t *>( pContext );
		pCtx->m_flNextEmitTime = m_flStartTime;
		pCtx->m_flTotalActualParticlesSoFar = 0.0f;
		pCtx->m_nTotalEmittedSoFar = 0;
		pCtx->m_flTimeOffset = 0.0f;
		pCtx->m_bStoppedEmission = false;
	}

	virtual void Restart( CParticleCollection *pParticles, void *pContext )
	{
		if ( !IsInfinitelyEmitting() )
		{
			ContinuousEmitterContext_t *pCtx = reinterpret_cast<ContinuousEmitterContext_t *>( pContext );
			pCtx->m_flNextEmitTime = pParticles->m_flCurTime + m_flStartTime;
			pCtx->m_flTotalActualParticlesSoFar = 0.0f;
			pCtx->m_nTotalEmittedSoFar = 0;
			pCtx->m_flTimeOffset = pParticles->m_flCurTime;
		}
	}

	// Called when the SFM wants to skip forward in time
	// Currently hacked for save/load pre-sim - correct solution is to serialize rather 
	// than skip-to-time and simulate
	virtual void SkipToTime( float flTime, CParticleCollection *pParticles, void *pContext ) const
	{
		ContinuousEmitterContext_t *pCtx = reinterpret_cast<ContinuousEmitterContext_t *>( pContext );
		float flStartTime = m_flStartTime + pCtx->m_flTimeOffset;
		if ( flTime <= flStartTime )
			return;
	
		float flControlPointScale = pParticles->GetHighestControlPoint();
		flControlPointScale *= m_flEmissionScale;
		float flEmissionRate = m_flEmitRate;
	
		float flEmitStrength;
		if ( pParticles->CheckIfOperatorShouldRun( this, &flEmitStrength ) )
		{
			flEmissionRate *= flEmitStrength;
		}

		if ( flControlPointScale != 0.0f )
		{
			flEmissionRate *= flControlPointScale;
		}

		float flPrevDrawTime = pParticles->m_flCurTime - flTime;
		float flCurrDrawTime = pParticles->m_flCurTime;

		if ( !IsInfinitelyEmitting() )
		{
			if ( flPrevDrawTime < flStartTime )
			{
				flPrevDrawTime = flStartTime;
			}
			//if ( flCurrDrawTime > flStartTime + m_flEmissionDuration )
			//{
			//	flCurrDrawTime = flStartTime + m_flEmissionDuration;
			//}
		}
		float flDeltaTime = flCurrDrawTime - flPrevDrawTime;
		flDeltaTime = min( flDeltaTime, 4.f );
		flPrevDrawTime = flCurrDrawTime - flDeltaTime;
		//disabled for now
		pCtx->m_flTotalActualParticlesSoFar = flDeltaTime * flEmissionRate;
		

		//if ( !IsInfinitelyEmitting() )
		//	pCtx->m_flTotalActualParticlesSoFar = min( pCtx->m_ActualParticlesToEmit, pCtx->m_flTotalActualParticlesSoFar );
		pCtx->m_nTotalEmittedSoFar = 0;
		//simulate a bunch
		int nActualParticlesToEmit = floor (pCtx->m_flTotalActualParticlesSoFar);
		int nStartParticle = pParticles->m_nActiveParticles;

		if ( pParticles->m_nMaxAllowedParticles < nStartParticle + nActualParticlesToEmit )
		{
			nActualParticlesToEmit = pParticles->m_nMaxAllowedParticles - nStartParticle;
		}

		pParticles->SetNActiveParticles( nActualParticlesToEmit + pParticles->m_nActiveParticles );
		
		float flTimeStampStep = ( flDeltaTime ) / ( nActualParticlesToEmit );
		float flTimeStep = flPrevDrawTime + flTimeStampStep;

		// Set the particle creation time to the exact sub-frame particle emission time
		// !! speed!! do sse init here
		for( int i = nStartParticle; i < nStartParticle + nActualParticlesToEmit; i++ )
		{
			float *pTimeStamp = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_CREATION_TIME, i );
			flTimeStep = min( flTimeStep, flCurrDrawTime );
			*pTimeStamp = flTimeStep;
			flTimeStep += flTimeStampStep;
		}

		if ( !g_bDontMakeSkipToTimeTakeForever )
		{
			flPrevDrawTime = max( flPrevDrawTime, flCurrDrawTime - pParticles->m_pDef->m_flNoDrawTimeToGoToSleep );
			pParticles->m_flCurTime = flPrevDrawTime;
			pParticles->m_fl4CurTime = ReplicateX4( flPrevDrawTime );
			for( float i = flPrevDrawTime; i < flCurrDrawTime; i += 0.1 )
			{
				pParticles->Simulate( .1, false );
			}
		}
	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( ContinuousEmitterContext_t );
	}

	virtual bool MayCreateMoreParticles( CParticleCollection *pParticles, void *pContext ) const
	{
		ContinuousEmitterContext_t *pCtx = reinterpret_cast<ContinuousEmitterContext_t *>( pContext );
		if ( pCtx->m_bStoppedEmission )
			return false;

		if ( m_flEmitRate <= 0.0f )
			return false;

		float flStartTime = m_flStartTime + pCtx->m_flTimeOffset;
		if ( m_flEmissionDuration != 0.0f && ( pParticles->m_flCurTime - pParticles->m_flDt ) > ( flStartTime + m_flEmissionDuration ) )
			return false;

		return true;
	}

	float m_flEmissionDuration;
	float m_flStartTime;
	float m_flEmitRate;
	float m_flTimePerEmission;
	float m_flEmissionScale;
	bool  m_bScalePerParticle;
};

DEFINE_PARTICLE_OPERATOR( C_OP_ContinuousEmitter, "emit_continuously", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_ContinuousEmitter ) 
	DMXELEMENT_UNPACK_FIELD( "emission_start_time", "0", float, m_flStartTime )
	DMXELEMENT_UNPACK_FIELD( "emission_rate", "100", float, m_flEmitRate )
	DMXELEMENT_UNPACK_FIELD( "emission_duration", "0", float, m_flEmissionDuration )
	DMXELEMENT_UNPACK_FIELD( "scale emission to used control points", "0.0", float, m_flEmissionScale )
	DMXELEMENT_UNPACK_FIELD( "use parent particles for emission scaling", "0", bool, m_bScalePerParticle )
END_PARTICLE_OPERATOR_UNPACK( C_OP_ContinuousEmitter )

uint32 C_OP_ContinuousEmitter::Emit( CParticleCollection *pParticles, float flCurStrength,
									 void *pContext ) const
{
	// Have we emitted all the particles we're going to emit?
	// NOTE: Using C_OP_ContinuousEmitter:: avoids a virtual function call
	ContinuousEmitterContext_t *pCtx = reinterpret_cast<ContinuousEmitterContext_t *>( pContext );

	//Allows for dynamic scaling via changes in number of control points.
	float flControlPointScale = pParticles->GetHighestControlPoint();
	//The emission scale here allows for a scalar value per controlpoint, like 2 or .25...
	flControlPointScale *= m_flEmissionScale;
	//Global strength scale brought in by operator fade in/fade out/oscillate 
	float flEmissionRate = m_flEmitRate * flCurStrength;
	if ( flControlPointScale != 0.0f || m_bScalePerParticle )
	{
		if ( m_bScalePerParticle )
		{ 
			if ( pParticles->m_pParent )
			{
				flControlPointScale = pParticles->m_pParent->m_nActiveParticles * m_flEmissionScale;
			}
			else
			{
				flControlPointScale = m_flEmissionScale;
			}

		}
		flEmissionRate *= flControlPointScale;
	}

	if ( flEmissionRate == 0.0f )
		return 0;

	if ( !C_OP_ContinuousEmitter::MayCreateMoreParticles( pParticles, pContext ) )
		return 0;

	float flStartTime = m_flStartTime + pCtx->m_flTimeOffset;
	if ( pParticles->m_flCurTime < flStartTime )
		return 0;

	Assert( flEmissionRate != 0.0f );

	// determine our previous and current draw times and clamp them to start time and emission duration
	float flPrevDrawTime = pParticles->m_flCurTime - pParticles->m_flDt;
	float flCurrDrawTime = pParticles->m_flCurTime;

	if ( !IsInfinitelyEmitting() )
	{
		if ( flPrevDrawTime < flStartTime )
		{
			flPrevDrawTime = flStartTime;
		}
		if ( flCurrDrawTime > flStartTime + m_flEmissionDuration )
		{
			flCurrDrawTime = flStartTime + m_flEmissionDuration;
		}
	}
	
	float flDeltaTime = flCurrDrawTime - flPrevDrawTime;

	//Calculate emission rate by delta time from last frame to determine number of particles to emit this frame as a fractional float
	float flActualParticlesToEmit = flEmissionRate  * flDeltaTime;

	//Add emitted particle to float counter to allow for fractional emission
	pCtx->m_flTotalActualParticlesSoFar += flActualParticlesToEmit;

	//Floor float accumulated value and subtract whole int emitted so far from the result to determine total whole particles to emit this frame
	int nParticlesToEmit = 	floor ( pCtx->m_flTotalActualParticlesSoFar ) - pCtx->m_nTotalEmittedSoFar;

	//Add emitted particles to running int total.
	pCtx->m_nTotalEmittedSoFar += nParticlesToEmit;

 
	if ( nParticlesToEmit == 0 )
		return 0;

	// We're only allowed to emit so many particles, though..
	// If we run out of room, only emit the last N particles
	int nActualParticlesToEmit = nParticlesToEmit;
	int nAllowedParticlesToEmit = pParticles->m_nMaxAllowedParticles - pParticles->m_nActiveParticles;
	if ( nAllowedParticlesToEmit < nParticlesToEmit )
	{
		nActualParticlesToEmit = nAllowedParticlesToEmit;
		//flStartEmissionTime = pCtx->m_flNextEmitTime - flTimePerEmission * nActualParticlesToEmit;
	}
	if ( nActualParticlesToEmit == 0 )
		return 0;

	int nStartParticle = pParticles->m_nActiveParticles;
	pParticles->SetNActiveParticles( nActualParticlesToEmit + pParticles->m_nActiveParticles );


	float flTimeStampStep = ( flDeltaTime ) / ( nActualParticlesToEmit );
	float flTimeStep = flPrevDrawTime + flTimeStampStep;
	
	// Set the particle creation time to the exact sub-frame particle emission time
	// !! speed!! do sse init here
	for( int i = nStartParticle; i < nStartParticle + nActualParticlesToEmit; i++ )
	{
		float *pTimeStamp = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_CREATION_TIME, i );
		flTimeStep = min( flTimeStep, flCurrDrawTime );
		*pTimeStamp = flTimeStep;
		flTimeStep += flTimeStampStep;
	}

	return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
}


//-----------------------------------------------------------------------------
// Noise Emitter
//-----------------------------------------------------------------------------
struct NoiseEmitterContext_t
{
	float	m_flTotalActualParticlesSoFar;
	int		m_nTotalEmittedSoFar;
	float	m_flNextEmitTime;
	float	m_flTimeOffset;
	bool	m_bStoppedEmission;
};

class C_OP_NoiseEmitter : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_NoiseEmitter );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		if ( m_flEmitRate < 0.0f )
		{
			m_flEmitRate = 0.0f;
		}
		if ( m_flEmissionDuration < 0.0f )
		{
			m_flEmissionDuration = 0.0f;
		}
		m_flEmitRate *= g_nParticle_Multiplier;
	}

	virtual uint32 Emit( CParticleCollection *pParticles, float flCurStrength,
		void *pContext ) const ;

	inline bool IsInfinitelyEmitting() const
	{
		return ( m_flEmissionDuration == 0.0f );
	}

	virtual void StopEmission( CParticleCollection *pParticles, void *pContext, bool bInfiniteOnly ) const
	{
		NoiseEmitterContext_t *pCtx = reinterpret_cast<NoiseEmitterContext_t *>( pContext );
		if ( !bInfiniteOnly || IsInfinitelyEmitting() )
		{
			pCtx->m_bStoppedEmission = true;
		}
	}
	virtual void StartEmission( CParticleCollection *pParticles, void *pContext, bool bInfiniteOnly ) const
	{
		NoiseEmitterContext_t *pCtx = reinterpret_cast<NoiseEmitterContext_t *>( pContext );
		if ( !bInfiniteOnly || IsInfinitelyEmitting() )
		{
			pCtx->m_bStoppedEmission = false;
			SkipToTime( pParticles->m_flCurTime, pParticles, pCtx );
		}
	}
	 
	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		NoiseEmitterContext_t *pCtx = reinterpret_cast<NoiseEmitterContext_t *>( pContext );
		pCtx->m_flNextEmitTime = m_flStartTime;
		pCtx->m_flTotalActualParticlesSoFar = 1.0f;
		pCtx->m_nTotalEmittedSoFar = 0;
		pCtx->m_flTimeOffset = 0.0f;
		pCtx->m_bStoppedEmission = false;
	}

	virtual void Restart( CParticleCollection *pParticles, void *pContext )
	{
		if ( !IsInfinitelyEmitting() )
		{
			NoiseEmitterContext_t *pCtx = reinterpret_cast<NoiseEmitterContext_t *>( pContext );
			pCtx->m_flNextEmitTime = m_flStartTime + pParticles->m_flCurTime;
			pCtx->m_flTotalActualParticlesSoFar = 1.0f;
			pCtx->m_nTotalEmittedSoFar = 0;
			pCtx->m_flTimeOffset = pParticles->m_flCurTime;
		}
	}

	// Called when the SFM wants to skip forward in time
	virtual void SkipToTime( float flTime, CParticleCollection *pParticles, void *pContext ) const
	{
		NoiseEmitterContext_t *pCtx = reinterpret_cast<NoiseEmitterContext_t *>( pContext );
		float flStartTime = m_flStartTime + pCtx->m_flTimeOffset;
		if ( flTime <= flStartTime )
			return;

		float flControlPointScale = pParticles->GetHighestControlPoint();
		flControlPointScale *= m_flEmissionScale;
		float flEmissionRate = m_flEmitRate;

		float flEmitStrength;
		if ( pParticles->CheckIfOperatorShouldRun( this, &flEmitStrength ) )
		{
			flEmissionRate *= flEmitStrength;
		}

		if ( flControlPointScale != 0.0f )
		{
			flEmissionRate *= flControlPointScale;
		}
		pCtx->m_flTotalActualParticlesSoFar = ( pParticles->m_flCurTime - flStartTime ) * flEmissionRate + 1;

		//if ( !IsInfinitelyEmitting() )
		//	pCtx->m_flTotalActualParticlesSoFar = min( pCtx->m_ActualParticlesToEmit, pCtx->m_flTotalActualParticlesSoFar );
		pCtx->m_nTotalEmittedSoFar = 0;

	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( NoiseEmitterContext_t );
	}

	virtual bool MayCreateMoreParticles( CParticleCollection *pParticles, void *pContext ) const
	{
		NoiseEmitterContext_t *pCtx = reinterpret_cast<NoiseEmitterContext_t *>( pContext );
		if ( pCtx->m_bStoppedEmission )
			return false;

		if ( m_flEmitRate <= 0.0f )
			return false;

		float flStartTime = m_flStartTime + pCtx->m_flTimeOffset;
		if ( m_flEmissionDuration != 0.0f && ( pParticles->m_flCurTime - pParticles->m_flDt ) > ( flStartTime + m_flEmissionDuration ) )
			return false;

		return true;
	}

	float	m_flEmissionDuration;
	float	m_flStartTime;
	float	m_flEmitRate;
	float	m_flTimePerEmission;
	float	m_flEmissionScale;
	bool	m_bAbsVal, m_bAbsValInv;
	float	m_flOffset;
	float	m_flOutputMin;
	float	m_flOutputMax;
	float	m_flNoiseScale, m_flNoiseScaleLoc;
	Vector  m_vecOffsetLoc;
	float   m_flWorldTimeScale;
};


DEFINE_PARTICLE_OPERATOR( C_OP_NoiseEmitter, "emit noise", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_NoiseEmitter ) 
	DMXELEMENT_UNPACK_FIELD( "emission_start_time", "0", float, m_flStartTime )
	DMXELEMENT_UNPACK_FIELD( "emission_duration", "0", float, m_flEmissionDuration )
	DMXELEMENT_UNPACK_FIELD( "scale emission to used control points", "0.0", float, m_flEmissionScale )
	DMXELEMENT_UNPACK_FIELD( "time noise coordinate scale","0.1",float,m_flNoiseScale)
	//DMXELEMENT_UNPACK_FIELD( "spatial noise coordinate scale","0.001",float,m_flNoiseScaleLoc)
	DMXELEMENT_UNPACK_FIELD( "time coordinate offset","0", float, m_flOffset )
	//DMXELEMENT_UNPACK_FIELD( "spatial coordinate offset","0 0 0", Vector, m_vecOffsetLoc )
	DMXELEMENT_UNPACK_FIELD( "absolute value","0", bool, m_bAbsVal )
	DMXELEMENT_UNPACK_FIELD( "invert absolute value","0", bool, m_bAbsValInv )
	DMXELEMENT_UNPACK_FIELD( "emission minimum","0", float, m_flOutputMin )
	DMXELEMENT_UNPACK_FIELD( "emission maximum","100", float, m_flOutputMax )
	DMXELEMENT_UNPACK_FIELD( "world time noise coordinate scale","0", float, m_flWorldTimeScale )
END_PARTICLE_OPERATOR_UNPACK( C_OP_NoiseEmitter )

uint32 C_OP_NoiseEmitter::Emit( CParticleCollection *pParticles, float flCurStrength,
									void *pContext ) const
{
	// Have we emitted all the particles we're going to emit?
	// NOTE: Using C_OP_ContinuousEmitter:: avoids a virtual function call
	NoiseEmitterContext_t *pCtx = reinterpret_cast<NoiseEmitterContext_t *>( pContext );

	//Allows for dynamic scaling via changes in number of control points.
	float flControlPointScale = pParticles->GetHighestControlPoint();
	//The emission scale here allows for a scalar value per controlpoint, like 2 or .25...
	flControlPointScale *= m_flEmissionScale;

	float	flAbsScale;
	int		nAbsVal;
	nAbsVal = 0xffffffff; 
	flAbsScale = 0.5;
	if ( m_bAbsVal )
	{
		nAbsVal = 0x7fffffff;
		flAbsScale = 1.0;
	}

	float fMin = m_flOutputMin;
	float fMax = m_flOutputMax;


	float CoordScale = m_flNoiseScale;
	//float CoordScaleLoc = m_flNoiseScaleLoc;


	float ValueScale, ValueBase;

	Vector Coord, CoordLoc, CoordWorldTime;
	//CoordLoc.x = pxyz[0]; 
	//CoordLoc.y = pxyz[4];
	//CoordLoc.z = pxyz[8];
	//CoordLoc += m_vecOffsetLoc;

	float Offset = m_flOffset;
	Coord = Vector ( (pParticles->m_flCurTime + Offset), (pParticles->m_flCurTime + Offset), (pParticles->m_flCurTime + Offset) );
	Coord *= CoordScale;
	//CoordLoc *= CoordScaleLoc;
	//Coord += CoordLoc;

	CoordWorldTime = Vector( (Plat_MSTime() * m_flWorldTimeScale), (Plat_MSTime() * m_flWorldTimeScale), (Plat_MSTime() * m_flWorldTimeScale) );
	Coord += CoordWorldTime;

	fltx4 flNoise128;
	FourVectors fvNoise;

	fvNoise.DuplicateVector( Coord );
	flNoise128 = NoiseSIMD( fvNoise );
	float flNoise = SubFloat( flNoise128, 0 );

	*( (int *) &flNoise)  &= nAbsVal;

	ValueScale = ( flAbsScale *( fMax - fMin ) );
	ValueBase = ( fMin+ ( ( 1.0 - flAbsScale ) *( fMax - fMin ) ) );

	if ( m_bAbsValInv )
	{
		flNoise = 1.0 - flNoise;
	}

	float flInitialNoise = ( ValueBase + ( ValueScale * flNoise ) );
	flInitialNoise = clamp(flInitialNoise, 0.0f, (float) INT_MAX );

	//Global strength scale brought in by operator fade in/fade out/oscillate 
	float flEmissionRate = flInitialNoise * flCurStrength;
	if ( flControlPointScale != 0.0f )
	{
		flEmissionRate *= flControlPointScale;
	}

	if ( flEmissionRate == 0.0f )
		return 0;

	if ( !C_OP_NoiseEmitter::MayCreateMoreParticles( pParticles, pContext ) )
		return 0;

	float flStartTime = m_flStartTime + pCtx->m_flTimeOffset;
	if ( pParticles->m_flCurTime < flStartTime )
		return 0;

	Assert( flEmissionRate != 0.0f );

	// determine our previous and current draw times and clamp them to start time and emission duration
	float flPrevDrawTime = pParticles->m_flCurTime - pParticles->m_flDt;
	float flCurrDrawTime = pParticles->m_flCurTime;

	if ( !IsInfinitelyEmitting() )
	{
		if ( flPrevDrawTime < flStartTime )
		{
			flPrevDrawTime = flStartTime;
		}
		if ( flCurrDrawTime > flStartTime + m_flEmissionDuration )
		{
			flCurrDrawTime = flStartTime + m_flEmissionDuration;
		}
	}

	float flDeltaTime = flCurrDrawTime - flPrevDrawTime;

	//Calculate emission rate by delta time from last frame to determine number of particles to emit this frame as a fractional float
	float flActualParticlesToEmit = flEmissionRate  * flDeltaTime;

	//Add emitted particle to float counter to allow for fractional emission
	pCtx->m_flTotalActualParticlesSoFar += flActualParticlesToEmit;

	//Floor float accumulated value and subtract whole int emitted so far from the result to determine total whole particles to emit this frame
	int nParticlesToEmit = 	floor ( pCtx->m_flTotalActualParticlesSoFar ) - pCtx->m_nTotalEmittedSoFar;

	//Add emitted particles to running int total.
	pCtx->m_nTotalEmittedSoFar += nParticlesToEmit;


	if ( nParticlesToEmit == 0 )
		return 0;

	// We're only allowed to emit so many particles, though..
	// If we run out of room, only emit the last N particles
	int nActualParticlesToEmit = nParticlesToEmit;
	int nAllowedParticlesToEmit = pParticles->m_nMaxAllowedParticles - pParticles->m_nActiveParticles;
	if ( nAllowedParticlesToEmit < nParticlesToEmit )
	{
		nActualParticlesToEmit = nAllowedParticlesToEmit;
		//flStartEmissionTime = pCtx->m_flNextEmitTime - flTimePerEmission * nActualParticlesToEmit;
	}
	if ( nActualParticlesToEmit == 0 )
		return 0;

	int nStartParticle = pParticles->m_nActiveParticles;
	pParticles->SetNActiveParticles( nActualParticlesToEmit + pParticles->m_nActiveParticles );

	float flTimeStampStep = ( flCurrDrawTime - flPrevDrawTime ) / ( nActualParticlesToEmit );
	float flTimeStep = flPrevDrawTime + flTimeStampStep;

	// Set the particle creation time to the exact sub-frame particle emission time
	// !! speed!! do sse init here
	for( int i = nStartParticle; i < nStartParticle + nActualParticlesToEmit; i++ )
	{
		float *pTimeStamp = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_CREATION_TIME, i );
		flTimeStep = min( flTimeStep, flCurrDrawTime );
		*pTimeStamp = flTimeStep;
		flTimeStep += flTimeStampStep;
	}

	return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
}


void AddBuiltInParticleEmitters( void )
{
	REGISTER_PARTICLE_OPERATOR( FUNCTION_EMITTER, C_OP_ContinuousEmitter );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_EMITTER, C_OP_InstantaneousEmitter );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_EMITTER, C_OP_NoiseEmitter );
}

