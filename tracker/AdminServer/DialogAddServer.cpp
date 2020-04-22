//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "DialogAddServer.h"
#include "INetAPI.h"
#include "IGameList.h"
#include "Server.h"

#include <VGUI_MessageBox.h>
#include <VGUI_KeyValues.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Input  : *gameList - game list to add specified server to
//-----------------------------------------------------------------------------
CDialogAddServer::CDialogAddServer(IGameList *gameList) : Frame(NULL, "DialogAddServer")
{
	MakePopup();

	m_pGameList = gameList;

	SetTitle("Add Server - Servers", true);

	LoadControlSettings("Admin\\DialogAddServer.res");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDialogAddServer::~CDialogAddServer()
{
}

//-----------------------------------------------------------------------------
// Purpose: Activates this dialog
//-----------------------------------------------------------------------------
void CDialogAddServer::Open()
{
	MoveToFront();
	RequestFocus();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CDialogAddServer::OnCommand(const char *command)
{
	if (!stricmp(command, "OK"))
	{
		OnOK();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles the OK button being pressed; adds the server to the game list
//-----------------------------------------------------------------------------
void CDialogAddServer::OnOK()
{
	// try and parse out IP address
	const char *address = GetControlString("ServerNameText", "");
	netadr_t netaddr;
	if (net->StringToAdr(address, &netaddr))
	{
		// net address successfully parsed, add the server to the game list
		serveritem_t server;
		memset(&server, 0, sizeof(server));
		for (int i = 0; i < 4; i++)
		{
			server.ip[i] = netaddr.ip[i];
		}
		server.port = (netaddr.port & 0xff) << 8 | (netaddr.port & 0xff00) >> 8;;
		if (!server.port)
		{
			// use the default port since it was not entered
			server.port = 27015;
		}
		m_pGameList->AddNewServer(server);
		m_pGameList->StartRefresh();
	}
	else
	{
		// could not parse the ip address, popup an error
		MessageBox *dlg = new MessageBox("Add Server - Error", "The server IP address you entered is invalid.");
		dlg->DoModal();
	}

	// mark ourselves to be closed
	PostMessage(this, new KeyValues("Close"));
}

//-----------------------------------------------------------------------------
// Purpose: Deletes dialog on close
//-----------------------------------------------------------------------------
void CDialogAddServer::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}
