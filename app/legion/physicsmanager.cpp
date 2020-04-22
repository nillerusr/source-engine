//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The main manager of the physics 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "physicsmanager.h"
#include "legion.h"
#include "tier2/tier2.h"
#include "tier1/convar.h"



//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
static CPhysicsManager s_PhysicsManager;
extern CPhysicsManager *g_pPhysicsManager = &s_PhysicsManager;

	
//-----------------------------------------------------------------------------
// Per-frame update
//-----------------------------------------------------------------------------
void CPhysicsManager::Update( )
{
}

