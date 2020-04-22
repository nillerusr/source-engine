//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PROP_PORTAL_SHARED_H
#define PROP_PORTAL_SHARED_H

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"

#ifdef CLIENT_DLL
#include "c_prop_portal.h"
#else
#include "prop_portal.h"
#endif

// CProp_Portal enum for the portal corners (if a user wants a specific corner)
enum PortalCorners_t { PORTAL_DOWN_RIGHT = 0, PORTAL_DOWN_LEFT, PORTAL_UP_RIGHT, PORTAL_UP_LEFT };

class CProp_Portal_Shared  //defined as a class to make intellisense more intelligent
{
public:
	static void UpdatePortalTransformationMatrix( const matrix3x4_t &localToWorld, const matrix3x4_t &remoteToWorld, VMatrix *pMatrix );

	static bool IsEntityTeleportable( CBaseEntity *pEntity );
	//static CProp_Portal *GetPortal1( bool bCreateIfNotFound = false );
	//static CProp_Portal *GetPortal2( bool bCreateIfNotFound = false );

	static const Vector		vLocalMins;
	static const Vector		vLocalMaxs;

#ifdef CLIENT_DLL
	static CUtlVector<C_Prop_Portal *> AllPortals; //an array of existing portal entities	
#else
	static CUtlVector<CProp_Portal *> AllPortals; //an array of existing portal entities
#endif //#ifdef CLIENT_DLL
};





#endif //#ifndef PROP_PORTAL_SHARED_H