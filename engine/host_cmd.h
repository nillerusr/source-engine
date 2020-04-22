//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#if !defined( HOST_CMD_H )
#define HOST_CMD_H
#ifdef _WIN32
#pragma once
#endif

#include "savegame_version.h"
#include "host_saverestore.h"
#include "convar.h"

// The launcher includes this file, too
#ifndef LAUNCHERONLY

void Host_Init( bool bIsDedicated );
void Host_Shutdown(void);
int  Host_Frame (float time, int iState );
void Host_ShutdownServer(void);
bool Host_NewGame( char *mapName, bool loadGame, bool bBackgroundLevel, const char *pszOldMap = NULL, const char *pszLandmark = NULL, bool bOldSave = false );
bool Host_Changelevel( bool loadfromsavedgame, const char *mapname, const char *start );
void Disconnect();

extern ConVar host_name;

extern int  gHostSpawnCount;

#endif // LAUNCHERONLY
#endif // HOST_CMD_H
