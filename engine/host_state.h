//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HOST_STATE_H
#define HOST_STATE_H
#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"

void	HostState_Init();
void	HostState_RunGameInit();
void	HostState_Frame( float time );
void	HostState_NewGame( char const *pMapName, bool remember_location, bool background );
void	HostState_LoadGame( char const *pSaveFileName, bool remember_location );
void	HostState_ChangeLevelSP( char const *pNewLevel, char const *pLandmarkName );
void	HostState_ChangeLevelMP( char const *pNewLevel, char const *pLandmarkName );
void	HostState_GameShutdown();
void	HostState_Shutdown();
void	HostState_Restart();
bool	HostState_IsGameShuttingDown();
bool	HostState_IsShuttingDown();
void	HostState_OnClientConnected();
void	HostState_OnClientDisconnected();
void	HostState_SetSpawnPoint(Vector &position, QAngle &angle);

#endif // HOST_STATE_H
