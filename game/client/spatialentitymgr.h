//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose : Singleton manager for spatial entities on the client
//
// $NoKeywords: $
//===========================================================================//

#ifndef SPATIALENTITYMGR_H
#define SPATIALENTITYMGR_H

#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h"


class C_SpatialEntity;


//------------------------------------------------------------------------------
// Purpose : Singleton manager for spatial entities on the client
//------------------------------------------------------------------------------
class CSpatialEntityMgr : public CAutoGameSystemPerFrame
{
	// Inherited from IGameSystemPerFrame
public:
	virtual char const *Name() { return "Spatial Entity Mgr"; }

	// Gets called each frame
	virtual void Update( float frametime );

	// Other public methods
public:

	// Create, destroy spatial entity
	void AddSpatialEntity( C_SpatialEntity *pSpatialEntity );
	void RemoveSpatialEntity( C_SpatialEntity *pSpatialEntity );

	CUtlVector<C_SpatialEntity*> m_SpatialEntities;
};


#endif // SPATIALENTITYMGR_H
