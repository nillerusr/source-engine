//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef ADMINSERVER_H
#define ADMINSERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "IAdminServer.h"
#include "IVGuiModule.h"

#include <utlvector.h>

class CServerPage;

//-----------------------------------------------------------------------------
// Purpose: Handles the UI and pinging of a half-life game server list
//-----------------------------------------------------------------------------
class CAdminServer : public IAdminServer, public IVGuiModule
{
public:
	CAdminServer();
	~CAdminServer();

	// IVGui module implementation
	virtual bool Initialize(CreateInterfaceFn *factorylist, int numFactories);
	virtual bool PostInitialize(CreateInterfaceFn *modules, int factoryCount);
	virtual vgui::VPANEL GetPanel();
	virtual bool Activate();
	virtual bool IsValid();
	virtual void Shutdown();
	virtual void Deactivate();
	virtual void Reactivate();
	virtual void SetParent(vgui::VPANEL parent);

	// IAdminServer implementation
	// opens a manage server dialog for a local server
	virtual ManageServerUIHandle_t OpenManageServerDialog(const char *serverName, const char *gameDir);

	// opens a manage server dialog to a remote server
	virtual ManageServerUIHandle_t OpenManageServerDialog(unsigned int gameIP, unsigned int gamePort, const char *password);

	// forces the game info dialog closed
	virtual void CloseManageServerDialog(ManageServerUIHandle_t gameDialog);

	// Gets a handle to the interface
	virtual IManageServer *GetManageServerInterface(ManageServerUIHandle_t handle);

private:
	struct OpenedManageDialog_t
	{
		unsigned long handle;
		IManageServer *manageInterface;
	};
	CUtlVector<OpenedManageDialog_t> m_OpenedManageDialog;
	vgui::VPANEL m_hParent;
};


class IVProfExport;
extern IVProfExport *g_pVProfExport;


#endif // AdminServer_H
