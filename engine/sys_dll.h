//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Expose functions from sys_dll.cpp.
//
// $NoKeywords: $
//=============================================================================//

#ifndef SYS_DLL_H
#define SYS_DLL_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IHammer;
class IDataCache;
class IPhysics;
class IMDLCache;
class IMatSystemSurface;
class IVideoServices;
class IVideoRecorder;
class IInputSystem;
class IDedicatedExports;
class ISoundEmitterSystemBase;

typedef unsigned short AVIHandle_t;


//-----------------------------------------------------------------------------
// Class factories
//-----------------------------------------------------------------------------
// This factory gets to many of the major app-single systems,
// including the material system, vgui, vgui surface, the file system.
extern CreateInterfaceFn g_AppSystemFactory;

// this factory connect the AppSystemFactory + client.dll + gameui.dll
extern CreateInterfaceFn g_GameSystemFactory;


//-----------------------------------------------------------------------------
// Singleton interfaces
//-----------------------------------------------------------------------------
extern IHammer *g_pHammer;
extern IDataCache *g_pDataCache;
extern IPhysics *g_pPhysics;
extern IMDLCache *g_pMDLCache;
extern IMatSystemSurface *g_pMatSystemSurface;
extern IInputSystem *g_pInputSystem;
extern IVideoServices *g_pVideo;
extern IDedicatedExports *dedicated;

//-----------------------------------------------------------------------------
// Other singletons
//-----------------------------------------------------------------------------
extern IVideoRecorder *g_pVideoRecorder;

inline bool InEditMode()
{
	return g_pHammer != NULL;
}

struct modinfo_t
{
	char szInfo[ 256 ];
	char szDL  [ 256 ];
	char szHLVersion[ 32 ];
	int version;
	int size;
	bool svonly;
	bool cldll;
};

extern modinfo_t gmodinfo;

void LoadEntityDLLs( const char *szBaseDir, bool bIsServerOnly );
void UnloadEntityDLLs( void );

// This returns true if someone called Error() or Sys_Error() and we're exiting.
// Since we call exit() from inside those, some destructors need to be safe and not crash.
bool IsInErrorExit();

// error message
bool Sys_MessageBox(const char *title, const char *info, bool bShowOkAndCancel);

bool ServerDLL_Load( bool bIsServerOnly );
void ServerDLL_Unload();

typedef uint32 AppId_t;

// steam.inf information.
struct SteamInfVersionInfo_t
{
	int32 ClientVersion; // PatchVersion
	int32 ServerVersion; // ServerVersion
	char szVersionString[ 32 ]; // PatchVersion string
	char szProductString[ 32 ]; // ProductName string

	AppId_t AppID; // Steam AppID. Read from steam.inf(AppID) or gameinfo.txt(SteamAppId)
	AppId_t ServerAppID; // ServerAppID. Used for dedicated server crash reporting.
};
const SteamInfVersionInfo_t& GetSteamInfIDVersionInfo();

extern CreateInterfaceFn g_ServerFactory;

#endif

