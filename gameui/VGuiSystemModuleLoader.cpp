//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <stdio.h>

#include "VGuiSystemModuleLoader.h"
#include "Sys_Utils.h"
#include "IVguiModule.h"
#include "ServerBrowser/IServerBrowser.h"

#include <vgui/IPanel.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <KeyValues.h>

#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>

#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// instance of class
CVGuiSystemModuleLoader g_VModuleLoader;

#ifdef GAMEUI_EXPORTS
extern vgui::VPANEL GetGameUIBasePanel();
#else
#include "../SteamUI/PlatformMainPanel.h"
extern CPlatformMainPanel *g_pMainPanel;
#endif

bool bSteamCommunityFriendsVersion = false;

#include <tier0/dbg.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CVGuiSystemModuleLoader, IVGuiModuleLoader, VGUIMODULELOADER_INTERFACE_VERSION, g_VModuleLoader);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVGuiSystemModuleLoader::CVGuiSystemModuleLoader()
{
	m_bModulesInitialized = false;
	m_bPlatformShouldRestartAfterExit = false;
	m_pPlatformModuleData = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CVGuiSystemModuleLoader::~CVGuiSystemModuleLoader()
{
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the module loader has acquired the platform mutex and loaded the modules
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::IsPlatformReady()
{
	return m_bModulesInitialized;
}

//-----------------------------------------------------------------------------
// Purpose: sets up all the modules for use
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::InitializeAllModules(CreateInterfaceFn *factorylist, int factorycount)
{
	if ( IsX360() )
	{
		// not valid for 360
		return false;
	}

	bool bSuccess = true;

	// Init vgui in the modules
	int i;
	for ( i = 0; i < m_Modules.Count(); i++ )
	{
		if (!m_Modules[i].moduleInterface->Initialize(factorylist, factorycount))
		{
			bSuccess = false;
			Error("Platform Error: module failed to initialize\n");
		}
	}

	// create a table of all the loaded modules
	CreateInterfaceFn *moduleFactories = (CreateInterfaceFn *)_alloca(sizeof(CreateInterfaceFn) * m_Modules.Count());
	for ( i = 0; i < m_Modules.Count(); i++ )
	{
		moduleFactories[i] = Sys_GetFactory(m_Modules[i].module);
	}

	// give the modules a chance to link themselves together
	for (i = 0; i < m_Modules.Count(); i++)
	{
		if (!m_Modules[i].moduleInterface->PostInitialize(moduleFactories, m_Modules.Count()))
		{
			bSuccess = false;
			Error("Platform Error: module failed to initialize\n");
		}
		
#ifdef GAMEUI_EXPORTS
		m_Modules[i].moduleInterface->SetParent(GetGameUIBasePanel());
#else
		m_Modules[i].moduleInterface->SetParent(g_pMainPanel->GetVPanel());		
#endif
	}

	m_bModulesInitialized = true;
	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: Loads and initializes all the modules specified in the platform file
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::LoadPlatformModules(CreateInterfaceFn *factorylist, int factorycount, bool useSteamModules)
{
	if ( IsX360() )
	{
		// not valid for 360
		return false;
	}

	bool bSuccess = true;

	// load platform menu
	KeyValues *kv = new KeyValues("Platform");
	if (!kv->LoadFromFile(g_pFullFileSystem, "steam/games/PlatformMenu.vdf", "PLATFORM"))
	{
		kv->deleteThis();
		return false;
	}

	// walk the platform menu loading all the interfaces
	KeyValues *menuKeys = kv->FindKey("Menu", true);
	for (KeyValues *it = menuKeys->GetFirstSubKey(); it != NULL; it = it->GetNextKey())
	{
		// see if we should skip steam modules
		if (!useSteamModules && it->GetInt("SteamApp"))
			continue;

		const char *pchInterface = it->GetString("interface");

		// don't load friends if we are using Steam Community
		if ( !Q_stricmp( pchInterface, "VGuiModuleTracker001" ) && bSteamCommunityFriendsVersion )
			continue;

		// get copy out of steam cache
		const char *dllPath = NULL;
		if ( IsOSX() )
		{
			dllPath = it->GetString("dll_osx");
		}
		else if ( IsLinux() )
		{
			dllPath = it->GetString("dll_linux");
		}
		else 
		{
			dllPath = it->GetString("dll");
		}


		// load the module (LoadModule calls GetLocalCopy() under steam)
		CSysModule *mod = g_pFullFileSystem->LoadModule(dllPath, "EXECUTABLE_PATH");
		if (!mod)
		{
			Error("Platform Error: bad module '%s', not loading\n", it->GetString("dll"));
			bSuccess = false;
			continue;
		}

		// make sure we get the right version
		IVGuiModule *moduleInterface = (IVGuiModule *)Sys_GetFactory(mod)(pchInterface, NULL);
		if (!moduleInterface)
		{
			Warning("Platform Error: module version ('%s, %s) invalid, not loading\n", it->GetString("dll"), it->GetString("interface"));
			bSuccess = false;
			continue;
		}

		// store off the module
		int newIndex = m_Modules.AddToTail();
		m_Modules[newIndex].module = mod;
		m_Modules[newIndex].moduleInterface = moduleInterface;
		m_Modules[newIndex].data = it;
	}

	m_pPlatformModuleData = kv;
	return InitializeAllModules(factorylist, factorycount) && bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: gives all platform modules a chance to Shutdown gracefully
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::ShutdownPlatformModules()
{
	if ( IsX360() )
	{
		// not valid for 360
		return;
	}

	// static include guard to prevent recursive calls
	static bool runningFunction = false;
	if (runningFunction)
		return;

	runningFunction = true;

	// deactivate all the modules first
	DeactivatePlatformModules();

	// give all the modules notice of quit
	int i;
	for ( i = 0; i < m_Modules.Count(); i++ )
	{
		vgui::ivgui()->PostMessage(m_Modules[i].moduleInterface->GetPanel(), new KeyValues("Command", "command", "Quit"), NULL);
	}

	for ( i = 0; i < m_Modules.Count(); i++ )
	{
		m_Modules[i].moduleInterface->Shutdown();
	}

	runningFunction = false;
}

//-----------------------------------------------------------------------------
// Purpose: Deactivates all the modules (puts them into in inactive but recoverable state)
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::DeactivatePlatformModules()
{
	for (int i = 0; i < m_Modules.Count(); i++)
	{
		m_Modules[i].moduleInterface->Deactivate();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reenables all the deactivated platform modules
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::ReactivatePlatformModules()
{
	for (int i = 0; i < m_Modules.Count(); i++)
	{
		m_Modules[i].moduleInterface->Reactivate();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Disables and unloads platform
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::UnloadPlatformModules()
{
	for (int i = 0; i < m_Modules.Count(); i++)
	{
		g_pFullFileSystem->UnloadModule(m_Modules[i].module);
	}

	m_Modules.RemoveAll();

	if (m_pPlatformModuleData)
	{
		m_pPlatformModuleData->deleteThis();
		m_pPlatformModuleData = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::RunFrame()
{
}

//-----------------------------------------------------------------------------
// Purpose: returns number of modules loaded
//-----------------------------------------------------------------------------
int CVGuiSystemModuleLoader::GetModuleCount()
{
	return m_Modules.Count();
}

//-----------------------------------------------------------------------------
// Purpose: returns the string menu name (unlocalized) of a module
//			moduleIndex is of the range [0, GetModuleCount())
//-----------------------------------------------------------------------------
const char *CVGuiSystemModuleLoader::GetModuleLabel(int moduleIndex)
{
	return m_Modules[moduleIndex].data->GetString("MenuName", "< unknown >");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::IsModuleVisible(int moduleIndex)
{
	return vgui::ipanel()->IsVisible( m_Modules[moduleIndex].moduleInterface->GetPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: brings the specified module to the foreground
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::IsModuleHidden(int moduleIndex)
{
	return m_Modules[moduleIndex].data->GetInt("Hidden", 0);
}

//-----------------------------------------------------------------------------
// Purpose: brings the specified module to the foreground
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::ActivateModule(int moduleIndex)
{
	if (!m_Modules.IsValidIndex(moduleIndex))
		return false;

	m_Modules[moduleIndex].moduleInterface->Activate();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: activates a module by name
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::ActivateModule(const char *moduleName)
{
	for (int i = 0; i < GetModuleCount(); i++)
	{
		if (!stricmp(GetModuleLabel(i), moduleName) || !stricmp(m_Modules[i].data->GetName(), moduleName))
		{
			ActivateModule(i);
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns a modules interface factory
//-----------------------------------------------------------------------------
CreateInterfaceFn CVGuiSystemModuleLoader::GetModuleFactory(int moduleIndex)
{
	return Sys_GetFactory(m_Modules[moduleIndex].module);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::PostMessageToAllModules(KeyValues *message)
{
	for (int i = 0; i < m_Modules.Count(); i++)
	{
		vgui::ivgui()->PostMessage(m_Modules[i].moduleInterface->GetPanel(), message->MakeCopy(), NULL);
	}
	message->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: sets the the platform should update and restart when it quits
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::SetPlatformToRestart()
{
	m_bPlatformShouldRestartAfterExit = true;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::ShouldPlatformRestart()
{
	return m_bPlatformShouldRestartAfterExit;
}
