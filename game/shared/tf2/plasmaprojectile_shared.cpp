//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "plasmaprojectile_shared.h"

#define PLASMA_LIFETIME				2.0

ConVar	plasma_gravity( "plasma_gravity","1000", FCVAR_REPLICATED, "Plasma gravity" );
ConVar	plasma_drag( "plasma_drag","2", FCVAR_REPLICATED, "Plasma drag" );


//-----------------------------------------------------------------------------
// Setup state needed to perform the physics computation
//-----------------------------------------------------------------------------
void CPlasmaProjectileShared::Init( const Vector &vecStart, const Vector &vecDir, float flSpawnSpeed )
{
	m_vecSpawnPosition = vecStart;
	m_vTracerDir = vecDir;
	m_flSpawnSpeed = flSpawnSpeed;
}

void CPlasmaProjectileShared::SetSpawnTime( float flSpawnTime )
{
	m_flSpawnTime = flSpawnTime;
}

void CPlasmaProjectileShared::SetDeathTime( float flDeathTime )
{
	m_flDeathTime = flDeathTime;
}


//-----------------------------------------------------------------------------
// Perform custom physics on this dude (when we're in ballistic mode)
//-----------------------------------------------------------------------------
void CPlasmaProjectileShared::ComputePosition( float flTime, Vector *pNewPosition, Vector *pNewVelocity, QAngle *pNewAngles, QAngle *pNewAngVelocity )
{
	float flLifeTime = flTime - m_flSpawnTime;
	if (flLifeTime < 0)
		return;

	// Travel ballistically until we run out of juice..
	if (flTime <= m_flDeathTime)
	{
		VectorMultiply( m_vTracerDir, m_flSpawnSpeed, *pNewVelocity );
		VectorMA( m_vecSpawnPosition, flLifeTime, *pNewVelocity, *pNewPosition );
	}
	else
	{
		VectorMultiply( m_vTracerDir, m_flSpawnSpeed, *pNewVelocity );
		VectorMA( m_vecSpawnPosition, m_flDeathTime - m_flSpawnTime, *pNewVelocity, *pNewPosition );

		// Ran out of juice... fall!
		float flFallTime = flTime - m_flDeathTime;

		float flDragFactor = exp( -plasma_drag.GetFloat() * flFallTime );
		*pNewVelocity *= flDragFactor;

		float flDist = (m_flSpawnSpeed / plasma_drag.GetFloat()) * ( 1.0f - flDragFactor );
		VectorMA( *pNewPosition, flDist, m_vTracerDir, *pNewPosition );

		// Add in the effects of gravity!
		pNewVelocity->z -= flFallTime * plasma_gravity.GetFloat();
		pNewPosition->z -= 0.5f * plasma_gravity.GetFloat() * flFallTime * flFallTime; 
	}
}


