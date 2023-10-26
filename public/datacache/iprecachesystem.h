//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#ifndef IPRECACHESYSTEM_H
#define IPRECACHESYSTEM_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/dbg.h"
#include "tier2/tier2.h"
#include "tier2/resourceprecacher.h"
#include "appframework/IAppSystem.h"

//-----------------------------------------------------------------------------
// Resource access control API
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// IResourceAccessControl
// Purpose: Maintains lists of resources to use them as filters to prevent access
// to ensure proper precache behavior in game code
//-----------------------------------------------------------------------------
abstract_class IPrecacheSystem : public IAppSystem
{
public:
	// Precaches/uncaches all resources used by a particular system
	virtual void Cache( IPrecacheHandler *pPrecacheHandler, PrecacheSystem_t nSystem, 
		const char *pName, bool bPrecache, ResourceList_t hResourceList, bool bBuildResourceList ) = 0;

	virtual void UncacheAll( IPrecacheHandler *pPrecacheHandler ) = 0 ;

	virtual void Register( IResourcePrecacher *pResourcePrecacherFirst, PrecacheSystem_t nSystem ) = 0;

	// Limits resource access to only resources used by this particular system
	// Use GLOBAL system, and NULL name to disable limited resource access
	virtual void LimitResourceAccess( PrecacheSystem_t nSystem, const char *pName ) = 0;
	virtual void EndLimitedResourceAccess() = 0;
};

DECLARE_TIER2_INTERFACE( IPrecacheSystem, g_pPrecacheSystem );


#endif // IPRECACHESYSTEM_H
