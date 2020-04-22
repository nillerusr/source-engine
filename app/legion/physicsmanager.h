//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The main manager of the physics 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#ifndef PHYSICSMANAGER_H
#define PHYSICSMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include "gamemanager.h"
#include "tier2/keybindings.h"
#include "tier1/commandbuffer.h"
#include "bitvec.h"


//-----------------------------------------------------------------------------
// Input management
//-----------------------------------------------------------------------------
class CPhysicsManager : public CGameManager<>
{
public:
	// Inherited from IGameManager
	virtual void Update( );
	virtual bool PerformsSimulation() { return true; }

private:
};


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
extern CPhysicsManager *g_pPhysicsManager;


#endif // PHYSICSMANAGER_H

