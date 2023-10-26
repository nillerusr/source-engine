//========= Copyright c 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef GAME_SERVER_PROP_PHYSICS2_H
#define GAME_SERVER_PROP_PHYSICS2_H

#include "props_shared.h"
#include "networkvar.h"
#include "props.h"


class CPhysics2Prop : public CBaseProp
{
	DECLARE_CLASS( CPhysics2Prop, CBaseEntity );
	DECLARE_DATADESC();
	//DECLARE_SERVERCLASS();
public:
	~CPhysics2Prop ();

	void Spawn( void );
	void Precache( void );
	//CNetworkVar( bool, m_bAwake );
	void VPhysicsUpdate( IPhysicsObject *pPhysics );

public:
	class CPhysics2Shape *m_pShape;
	class CPhysics2Actor *m_pActor;
	//class CPhysics2Inertia *m_pInertia;
	class IPhysics2CookedInertia *m_pCookedInertia;
	class CPhysics2Inertia *m_pInertia;

	//class CPhysics2Actor *m_pBoxActor;
	//class CPhysics2Box *m_pBoxShape;
};

#endif