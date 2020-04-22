//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IADMINSERVER_H
#define IADMINSERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

// handle to a game window
typedef unsigned int ManageServerUIHandle_t;
class IManageServer;

//-----------------------------------------------------------------------------
// Purpose: Interface to server administration functions
//-----------------------------------------------------------------------------
abstract_class IAdminServer : public IBaseInterface
{
public:
	// opens a manage server dialog for a local server
	virtual ManageServerUIHandle_t OpenManageServerDialog(const char *serverName, const char *gameDir) = 0;

	// opens a manage server dialog to a remote server
	virtual ManageServerUIHandle_t OpenManageServerDialog(unsigned int gameIP, unsigned int gamePort, const char *password) = 0;

	// forces the game info dialog closed
	virtual void CloseManageServerDialog(ManageServerUIHandle_t gameDialog) = 0;

	// Gets a handle to the interface
	virtual IManageServer *GetManageServerInterface(ManageServerUIHandle_t handle) = 0;
};

#define ADMINSERVER_INTERFACE_VERSION "AdminServer002"



#endif // IAdminServer_H
