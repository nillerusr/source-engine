//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PORTAL_PHYSICS_COLLISIONEVENT_H
#define PORTAL_PHYSICS_COLLISIONEVENT_H

#ifdef _WIN32
#pragma once
#endif

#include "physics_collisionevent.h"

class CPortal_CollisionEvent : public CCollisionEvent
{
public:
	DECLARE_CLASS_GAMEROOT( CPortal_CollisionEvent, CCollisionEvent );

	virtual int ShouldCollide( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1 );
	virtual void PreCollision( vcollisionevent_t *pEvent );
	virtual void PostCollision( vcollisionevent_t *pEvent );
	virtual int ShouldSolvePenetration( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1, float dt );

	virtual void PostSimulationFrame( void );
	void PortalPostSimulationFrame( void );
	void AddDamageEvent( CBaseEntity *pEntity, const CTakeDamageInfo &info, IPhysicsObject *pInflictorPhysics, bool bRestoreVelocity, const Vector &savedVel, const AngularImpulse &savedAngVel );
};

#endif //#ifndef PORTAL_PHYSICS_COLLISIONEVENT_H
