//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TF_OBJ_DRAGONSTEETH_H
#define TF_OBJ_DRAGONSTEETH_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_obj.h"

// ------------------------------------------------------------------------ //
// Purpose: Object built to block vehicle movement
// ------------------------------------------------------------------------ //
class CObjectDragonsTeeth : public CBaseObject
{
DECLARE_CLASS( CObjectDragonsTeeth, CBaseObject );

public:
	DECLARE_SERVERCLASS();

	CObjectDragonsTeeth();

	virtual void	Spawn();
	virtual void	Precache();
	virtual float	GetNearbyObjectCheckRadius( void ) { return 10.0; }
	virtual bool	StartBuilding( CBaseEntity *pBuilder );
	virtual void	OnActivityChanged( Activity act );
	
public:
	CHandle< CBaseObject >	m_hOwningObject;		// Object I was created for
};

#endif // TF_OBJ_DRAGONSTEETH_H
