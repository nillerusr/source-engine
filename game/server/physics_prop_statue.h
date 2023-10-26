//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PHYSICS_PROP_STATUE_H
#define PHYSICS_PROP_STATUE_H
#ifdef _WIN32
#pragma once
#endif

#include "props.h"


struct outer_collision_obb_t
{
	bool	bDirty;
	Vector	vecPos;
	Vector	vecMins;
	Vector	vecMaxs;
	QAngle	angAngles;

	CUtlVector<short>	iSpheres;
};


//-----------------------------------------------------------------------------
// Purpose: entity class for simple ragdoll physics
//-----------------------------------------------------------------------------

// UNDONE: Move this to a private header
class CStatueProp : public CPhysicsProp
{
	DECLARE_CLASS( CStatueProp, CPhysicsProp );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

public:
	CStatueProp( void );

	virtual void	Spawn( void );
	virtual void	Precache();

	virtual bool	CreateVPhysics();
	virtual void	VPhysicsUpdate( IPhysicsObject *pPhysics );

	//virtual float	GetAutoAimRadius() { return 24.0f; }

	virtual void	ComputeWorldSpaceSurroundingBox( Vector *pMins, Vector *pMaxs );
	virtual bool	TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );
	virtual int		OnTakeDamage( const CTakeDamageInfo &info );
	virtual void	Event_Killed( const CTakeDamageInfo &info );

	virtual void	Freeze( float flFreezeAmount = -1.0f, CBaseEntity *pFreezer = NULL, Ray_t *pFreezeRay = NULL );

	void	CollisionPartnerThink( void );

private:

	bool	CreateVPhysicsFromHitBoxes( CBaseAnimating *pInitBaseAnimating );
	bool	CreateVPhysicsFromOBBs( CBaseAnimating *pInitBaseAnimating );

public:

	CNetworkHandle( CBaseAnimating,	m_hInitBaseAnimating );

	CNetworkVar( bool, m_bShatter );
	CNetworkVar( int, m_nShatterFlags );
	CNetworkVar( Vector, m_vShatterPosition );
	CNetworkVar( Vector, m_vShatterForce );

	const CUtlVector<outer_collision_obb_t>	*m_pInitOBBs;
};


CBaseEntity *CreateServerStatue( CBaseAnimating *pAnimating, int collisionGroup );
CBaseEntity *CreateServerStatueFromOBBs( const CUtlVector<outer_collision_obb_t> &vecSphereOrigins, CBaseAnimating *pChildEntity );


#endif // PHYSICS_PROP_STATUE_H
