//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Map entity that allows players to build objects on it
//
//=============================================================================//

#ifndef INFO_BUILDPOINT_H
#define INFO_BUILDPOINT_H
#ifdef _WIN32
#pragma once
#endif

#include "ihasbuildpoints.h"

//-----------------------------------------------------------------------------
// Purpose: Map entity that allows players to build objects on it
//-----------------------------------------------------------------------------
class CInfoBuildPoint : public CBaseEntity, public IHasBuildPoints
{
	DECLARE_CLASS( CInfoBuildPoint, CBaseEntity );
public:
	DECLARE_DATADESC();

	void Spawn( void );
	void UpdateOnRemove( void );

// IHasBuildPoints
public:
	// Tell me how many build points you have
	virtual int			GetNumBuildPoints( void ) const;

	// Give me the origin & angles of the specified build point
	virtual bool		GetBuildPoint( int iPoint, Vector &vecOrigin, QAngle &vecAngles );

	virtual int			GetBuildPointAttachmentIndex( int iPoint ) const;

	// Can I build the specified object on the specified build point?
	virtual bool		CanBuildObjectOnBuildPoint( int iPoint, int iObjectType );

	// I've finished building the specified object on the specified build point
	virtual void		SetObjectOnBuildPoint( int iPoint, CBaseObject *pObject );

	// Get the number of objects build on this entity
	virtual int			GetNumObjectsOnMe( void );

	// Get the first object that's built on me
	virtual CBaseEntity	*GetFirstObjectOnMe( void );

	// Get the first object of type, return NULL if no such type available
	virtual CBaseObject *GetObjectOfTypeOnMe( int iObjectType );

	// Remove all objects built on me
	virtual void		RemoveAllObjects( void );

	// Return the maximum distance that this entity's build points can be snapped to
	virtual float		GetMaxSnapDistance( int iPoint );

	// Return true if it's possible that build points on this entity may move in local space (i.e. due to animation)
	virtual bool		ShouldCheckForMovement( void );

	// I've finished building the specified object on the specified build point
	virtual int			FindObjectOnBuildPoint( CBaseObject *pObject );

	// Returns an exit point for a vehicle built on a build point...
	virtual void		GetExitPoint( CBaseEntity *pPlayer, int iPoint, Vector *pAbsOrigin, QAngle *pAbsAngles );


private:
	string_t				m_iszAllowedObject;
	int						m_iAllowedObjectType;
	CHandle<CBaseObject>	m_hObjectBuiltOnMe;
};

extern CUtlVector<CInfoBuildPoint*>		g_MapDefinedBuildPoints;

#endif // INFO_BUILDPOINT_H
