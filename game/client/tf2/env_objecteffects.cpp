//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "particles_simple.h"
#include "particlemgr.h"
#include "particle_collision.h"
#include "env_objecteffects.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectSmokeParticles::Setup( const Vector &origin, const Vector *direction, float angularSpread, float minSpeed, float maxSpeed, float gravity, float dampen, int flags )
{
	// See if we've specified a direction
	m_ParticleCollision.Setup( origin, direction, angularSpread, minSpeed, maxSpeed, gravity, dampen );
}

void CObjectSmokeParticles::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	float timeDelta = pIterator->GetTimeDelta();

	ObjectSmokeParticle *pParticle = (ObjectSmokeParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		//Update velocity
		UpdateVelocity( pParticle, timeDelta );
		pParticle->m_Pos += (pParticle->m_vecVelocity * timeDelta);
		pParticle->m_vecVelocity += pParticle->m_vecAcceleration * 2 * timeDelta;

		//Should this particle die?
		pParticle->m_flLifetime += timeDelta;
		UpdateRoll( pParticle, timeDelta );

		if ( pParticle->m_flLifetime >= pParticle->m_flDieTime )
			pIterator->RemoveParticle( pParticle );

		pParticle = (ObjectSmokeParticle*)pIterator->GetNext();
	}
}


void CObjectSmokeParticles::RenderParticles( CParticleRenderIterator *pIterator )
{
	const ObjectSmokeParticle *pParticle = (const ObjectSmokeParticle *)pIterator->GetFirst();
	while ( pParticle )
	{
		//Render
		Vector	tPos;

		TransformParticle( ParticleMgr()->GetModelView(), pParticle->m_Pos, tPos );
		float sortKey = (int) tPos.z;

		//Render it
		RenderParticle_ColorSizeAngle(
			pIterator->GetParticleDraw(),
			tPos,
			UpdateColor( pParticle ),
			UpdateAlpha( pParticle ) * GetAlphaDistanceFade( tPos, m_flNearClipMin, m_flNearClipMax ),
			UpdateScale( pParticle ),
			pParticle->m_flRoll );

		pParticle = (const ObjectSmokeParticle *)pIterator->GetNext( sortKey );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CObjectFireParticles::Setup( const Vector &origin, const Vector *direction, float angularSpread, float minSpeed, float maxSpeed, float gravity, float dampen, int flags )
{
}

void CObjectFireParticles::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	float timeDelta = pIterator->GetTimeDelta();

	ObjectFireParticle *pParticle = (ObjectFireParticle*)pIterator->GetFirst();
	while ( pParticle )
	{
		// Lost our parent?
		if ( !pParticle->m_hParent )
		{
			pIterator->RemoveParticle( pParticle );
		}
		else
		{
			// Update position to match our parent
			Vector vecFire;
			QAngle angFire;
			if ( pParticle->m_hParent->GetAttachment( pParticle->m_iAttachmentPoint, vecFire, angFire ) )
			{
				pParticle->m_Pos = vecFire;
			}

			// Should this particle die?
			pParticle->m_flLifetime += timeDelta;

			UpdateRoll( pParticle, timeDelta );

			if ( pParticle->m_flLifetime >= pParticle->m_flDieTime )
				pIterator->RemoveParticle( pParticle );
		}

		pParticle = (ObjectFireParticle*)pIterator->GetNext();
	}
}


void CObjectFireParticles::RenderParticles( CParticleRenderIterator *pIterator )
{
	const ObjectFireParticle *pParticle = (const ObjectFireParticle *)pIterator->GetFirst();
	while ( pParticle )
	{
		// Render
		Vector	tPos;
		TransformParticle( ParticleMgr()->GetModelView(), pParticle->m_Pos, tPos );
		float sortKey = (int) tPos.z;

		// Render it
		RenderParticle_ColorSizeAngle(
			pIterator->GetParticleDraw(),
			tPos,
			UpdateColor( pParticle ),
			UpdateAlpha( pParticle ) * GetAlphaDistanceFade( tPos, m_flNearClipMin, m_flNearClipMax ),
			UpdateScale( pParticle ),
			pParticle->m_flRoll
			);

		pParticle = (const ObjectFireParticle *)pIterator->GetNext( sortKey );
	}
}


