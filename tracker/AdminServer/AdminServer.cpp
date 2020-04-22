//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "AdminServer.h"
#include "IRunGameEngine.h"
#include "IGameServerData.h"
#include "GamePanelInfo.h"
#include "ivprofexport.h"

#include <vgui/ISystem.h>
#include <vgui/IPanel.h>
#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <KeyValues.h>
#include "filesystem.h"

// expose the server browser interfaces
CAdminServer g_AdminServerSingleton;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CAdminServer, IAdminServer, ADMINSERVER_INTERFACE_VERSION, g_AdminServerSingleton);
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CAdminServer, IVGuiModule, "VGuiModuleAdminServer001", g_AdminServerSingleton);

IGameServerData *g_pGameServerData = NULL;
IVProfExport *g_pVProfExport = NULL;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CAdminServer::CAdminServer()
{
	// fill in the 0-based element of the manage servers list
	OpenedManageDialog_t empty = { 0, NULL };
	m_OpenedManageDialog.AddToTail(empty);
	m_hParent=0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CAdminServer::~CAdminServer()
{
}

//-----------------------------------------------------------------------------
// Purpose: links to vgui and engine interfaces
//-----------------------------------------------------------------------------
bool CAdminServer::Initialize(CreateInterfaceFn *factorylist, int factoryCount)
{
	ConnectTier1Libraries( factorylist, factoryCount );
	ConVar_Register();
	ConnectTier2Libraries( factorylist, factoryCount );
	ConnectTier3Libraries( factorylist, factoryCount );

	// find our interfaces
	for (int i = 0; i < factoryCount; i++)
	{
		// if we're running locally we can get this direct interface to the game engine
		if (!g_pGameServerData)
		{
			g_pGameServerData = (IGameServerData *)(factorylist[i])(GAMESERVERDATA_INTERFACE_VERSION, NULL);
		}
		if ( !g_pVProfExport )
		{
			g_pVProfExport = (IVProfExport*)(factorylist[i])( VPROF_EXPORT_INTERFACE_VERSION, NULL );
		}
	}

	RemoteServer().Initialize(); // now we have the game date interface, initialize the engine connection

	if ( vgui::VGui_InitInterfacesList("AdminServer", factorylist, factoryCount) )
	{
		// load localization file
		g_pVGuiLocalize->AddFile( "admin/admin_%language%.txt");
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: links to other modules interfaces (tracker)
//-----------------------------------------------------------------------------
bool CAdminServer::PostInitialize(CreateInterfaceFn *modules, int factoryCount)
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAdminServer::IsValid()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAdminServer::Activate()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: returns direct handle to main server browser dialog
//-----------------------------------------------------------------------------
vgui::VPANEL CAdminServer::GetPanel()
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Closes down the server browser for good
//-----------------------------------------------------------------------------
void CAdminServer::Shutdown()
{
	DisconnectTier3Libraries();
	DisconnectTier2Libraries();
	ConVar_Unregister();
	DisconnectTier1Libraries();
}

void CAdminServer::SetParent(vgui::VPANEL parent)
{
/*	if (m_hServerPage.Get())
	{
		m_hServerPage->SetParent(parent);
	}
	*/
	m_hParent = parent;
}

//-----------------------------------------------------------------------------
// Purpose: Called when the user enters the game
//-----------------------------------------------------------------------------
void CAdminServer::Deactivate()
{
}

//-----------------------------------------------------------------------------
// Purpose: Called when the user returns from the game to the outside UI
//-----------------------------------------------------------------------------
void CAdminServer::Reactivate()
{
}

//-----------------------------------------------------------------------------
// Purpose: opens a manage server dialog for a local server
//-----------------------------------------------------------------------------
ManageServerUIHandle_t CAdminServer::OpenManageServerDialog(const char *serverName, const char *gameDir)
{
	CGamePanelInfo *tmp = new CGamePanelInfo(NULL, serverName, gameDir);
	tmp->SetParent(m_hParent);

	// add a new item into the list
	int i = m_OpenedManageDialog.AddToTail();
	m_OpenedManageDialog[i].handle = vgui::ivgui()->PanelToHandle(tmp->GetVPanel());
	m_OpenedManageDialog[i].manageInterface = tmp;

	return (ManageServerUIHandle_t)i;
}

//-----------------------------------------------------------------------------
// Purpose: opens a manage server dialog to a remote server
//-----------------------------------------------------------------------------
ManageServerUIHandle_t CAdminServer::OpenManageServerDialog(unsigned int gameIP, unsigned int gamePort, const char *password)
{
	Assert(false);
	return (ManageServerUIHandle_t)0;
}

//-----------------------------------------------------------------------------
// Purpose: forces the game info dialog closed
//-----------------------------------------------------------------------------
void CAdminServer::CloseManageServerDialog(ManageServerUIHandle_t gameDialog)
{
	Assert(false);
}

//-----------------------------------------------------------------------------
// Purpose: Gets a handle to the management interface
//-----------------------------------------------------------------------------
IManageServer *CAdminServer::GetManageServerInterface(ManageServerUIHandle_t handle)
{
	// make sure it's safe
	if ((int)handle < 1 || (int)handle > m_OpenedManageDialog.Count())
		return NULL;

	vgui::VPANEL panel = vgui::ivgui()->HandleToPanel(m_OpenedManageDialog[handle].handle);
	if (!panel)
		return NULL;
	
	return m_OpenedManageDialog[handle].manageInterface;
}
