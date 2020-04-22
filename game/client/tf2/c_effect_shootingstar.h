//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's Meteor
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_EFFECT_SHOOTINGSTAR_H
#define C_EFFECT_SHOOTINGSTAR_H
#pragma once

#include "c_baseanimating.h"
#include "particles_simple.h"

class C_ShootingStar;

//=============================================================================
//
// Client-side shooting star spawner.
//
class C_ShootingStarSpawner : public C_BaseEntity
{
	DECLARE_CLASS( C_ShootingStarSpawner, C_BaseEntity );

public:

	DECLARE_CLIENTCLASS();

	C_ShootingStarSpawner();

	void ClientThink( void );
	void SpawnShootingStars( void );

public:

	float						m_flSpawnInterval;		// How often do I spawn meteors?
};


//=============================================================================
//
// Shooting Star Effect
//
class C_ShootingStar : public C_BaseAnimating, CSimpleEmitter
{
	DECLARE_CLASS( C_ShootingStar, C_BaseAnimating );

public:

	C_ShootingStar( );
	~C_ShootingStar( void );

	void	Init( const Vector vecOrigin, const Vector vecVelocity, int nSize, float flLifeTime );
	void	Destroy( void );

	void	Update( float timeDelta );

public:

	float	m_flScale;

private:

	void SetSortOrigin( const Vector &vSortOrigin );

private:
	C_ShootingStar( const C_ShootingStar & );

	CUtlVector<SimpleParticle*>		m_aParticles;
};

#endif // C_EFFECT_SHOOTINGSTAR_H