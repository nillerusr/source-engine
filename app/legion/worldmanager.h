//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains all world state--the main game database
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#ifndef WORLDMANAGER_H
#define WORLDMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include "gamemanager.h"
#include "mathlib/mathlib.h"
#include "tier1/convar.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CHeightField;
class CCameraProperty;


//-----------------------------------------------------------------------------
// Entity types
//-----------------------------------------------------------------------------
class CPlayerEntity
{
public:
	CCameraProperty *m_pCameraProperty;
};


//-----------------------------------------------------------------------------
// World state
//-----------------------------------------------------------------------------
class CWorldManager : public CGameManager<>
{
public:
	CWorldManager();
	virtual ~CWorldManager();

	// Inherited from IGameManager
	virtual LevelRetVal_t LevelInit( bool bFirstCall );
//	virtual void Update( );
	virtual LevelRetVal_t LevelShutdown( bool bFirstCall );

	// Draws the world
	void DrawWorld();

	// Gets the local player
	CPlayerEntity *GetLocalPlayer();

private:
	CON_COMMAND_MEMBER_F( CWorldManager, "+forward", ForwardStart, "Start forward movement", 0 );
	CON_COMMAND_MEMBER_F( CWorldManager, "-forward", ForwardStop, "Stop forward movement", 0 );
	CON_COMMAND_MEMBER_F( CWorldManager, "+back", BackwardStart, "Start backward movement", 0 );
	CON_COMMAND_MEMBER_F( CWorldManager, "-back", BackwardStop, "Stop backward movement", 0 );

	// Creates, destroys entities
	void CreateEntities();
	void DestroyEntities();

	// Sets the initial camera position
	void SetInitialLocalPlayerPosition();

	CHeightField *m_pHeightField;

	CPlayerEntity m_PlayerEntity;
};


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
extern CWorldManager *g_pWorldManager;


#endif // WORLDMANAGER_H

