//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PLASMAPROJECTILE_SHARED_H
#define PLASMAPROJECTILE_SHARED_H

#ifndef _WIN32
#pragma once
#endif

#include "predictable_entity.h"
#include "server_class.h"
#include "client_class.h"
#include "mathlib/vector.h"

class CPlasmaProjectileShared
{
public:
	DECLARE_PREDICTABLE();
	DECLARE_NETWORKCLASS_NOBASE();
	DECLARE_CLASS_NOBASE( CPlasmaProjectileShared );
	DECLARE_EMBEDDED_NETWORKVAR();

public:
	void Init( const Vector &vecStart, const Vector &vecDir, float flSpawnSpeed );
	float GetSpawnTime() const { return m_flSpawnTime; }
	void SetSpawnTime( float flSpawnTime );
	void SetDeathTime( float flDeathTime );
	float GetDeathTime() const { return m_flDeathTime; }

	void ComputePosition( float flTime, Vector *pNewPosition, Vector *pNewVelocity, QAngle *pNewAngles, QAngle *pNewAngVelocity );

	const Vector &TracerDir() { return m_vTracerDir.Get(); }

private:
	CNetworkVector( m_vTracerDir );
	CNetworkVector( m_vecSpawnPosition );
	CNetworkVar( float, m_flSpawnTime );
	CNetworkVar( float, m_flSpawnSpeed );
	CNetworkVar( float, m_flDeathTime );
};


#endif // PLASMAPROJECTILE_SHARED_H
