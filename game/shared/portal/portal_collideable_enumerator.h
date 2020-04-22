//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PORTAL_COLLIDEABLE_ENUMERATOR_H
#define PORTAL_COLLIDEABLE_ENUMERATOR_H

#ifdef _WIN32
#pragma once
#endif

#include "ispatialpartition.h"

#ifdef CLIENT_DLL
class C_Prop_Portal;
typedef C_Prop_Portal CProp_Portal;
#else
class CProp_Portal;
#endif

//only enumerates entities in front of the associated portal and are solid (as in a player would get stuck in them)
class CPortalCollideableEnumerator : public IPartitionEnumerator
{
private:
	EHANDLE m_hTestPortal; //the associated portal that we only want objects in front of
	Vector m_vPlaneNormal; //portal plane normal
	float m_fPlaneDist; //plane equation distance
	Vector m_ptForward1000; //a point exactly 1000 units from the portal center along its forward vector
public:
	IHandleEntity *m_pHandles[1024];
	int m_iHandleCount;
	CPortalCollideableEnumerator( const CProp_Portal *pAssociatedPortal );
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity );
};



#endif //#ifndef PORTAL_COLLIDEABLE_ENUMERATOR_H