//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Handles loading/unloading of different vgui modules into a shared context
//
//=============================================================================//

#ifndef VGUISYSTEMMODULELOADER_H
#define VGUISYSTEMMODULELOADER_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/PHandle.h"
#include "utlvector.h"
#include "IVGuiModuleLoader.h"

class IVGuiModule;

class KeyValues;

//-----------------------------------------------------------------------------
// Purpose: Handles loading/unloading of different vgui modules into a shared context
//-----------------------------------------------------------------------------
class CVGuiSystemModuleLoader : public IVGuiModuleLoader
{
public:
	CVGuiSystemModuleLoader();
	~CVGuiSystemModuleLoader();

	// loads all the modules in the platform
	bool LoadPlatformModules(CreateInterfaceFn *factorylist, int factorycount, bool useSteamModules);

	// returns true if the module loader has loaded the modules
	bool IsPlatformReady();

	// needs to be called every frame - updates all the modules states
	void RunFrame();

	// returns number of modules loaded
	int GetModuleCount();

	// returns the string menu name (unlocalized) of a module
	// moduleIndex is of the range [0, GetModuleCount())
	const char *GetModuleLabel(int moduleIndex);

	bool IsModuleHidden(int moduleIndex);
	bool IsModuleVisible(int moduleIndex);

	// returns a modules interface factory
	CreateInterfaceFn GetModuleFactory(int moduleIndex);

	// brings the specified module to the foreground
	bool ActivateModule(int moduleIndex);
	bool ActivateModule(const char *moduleName);

	// Deactivates all the modules (puts them into in inactive but recoverable state)
	void DeactivatePlatformModules();

	// Reenables all the deactivated platform modules
	void ReactivatePlatformModules();

	// shuts down all the platform modules
	void ShutdownPlatformModules();

	// unload all active platform modules/dlls from memory
	void UnloadPlatformModules();

	// posts a message to all active modules
	void PostMessageToAllModules(KeyValues *message);

	// sets the the platform should update and restart when it quits
	void SetPlatformToRestart();

	// returns true if the platform should restart after exit
	bool ShouldPlatformRestart();

private:
	// sets up all the modules for use
	bool InitializeAllModules(CreateInterfaceFn *factorylist, int factorycount);

	bool m_bModulesInitialized;
	bool m_bPlatformShouldRestartAfterExit;

	struct module_t
	{
		CSysModule *module;
		IVGuiModule *moduleInterface;
		KeyValues *data;
	};

	CUtlVector<module_t> m_Modules;
	KeyValues *m_pPlatformModuleData;
};

extern CVGuiSystemModuleLoader g_VModuleLoader;

#endif // VGUISYSTEMMODULELOADER_H
