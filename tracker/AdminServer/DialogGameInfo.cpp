//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "DialogGameInfo.h"
#include "Info.h"
#include "IRunGameEngine.h"
#include "IGameList.h"
#include "TrackerProtocol.h"
#include "serverpage.h"
#include "ServerList.h"
#include "DialogServerPassword.h"

#include <VGUI_Controls.h>
#include <VGUI_ISystem.h>
#include <VGUI_ISurface.h>
#include <VGUI_IVGui.h>
#include <VGUI_KeyValues.h>
#include <VGUI_Label.h>
#include <VGUI_TextEntry.h>
#include <VGUI_Button.h>
#include <VGUI_ToggleButton.h>
#include <VGUI_RadioButton.h>

#include <stdio.h>

using namespace vgui;

static const long RETRY_TIME = 10000;		// refresh server every 10 seconds

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDialogGameInfo::CDialogGameInfo(IGameList *gameList, unsigned int serverID, int serverIP, int serverPort) : Frame(NULL, "DialogGameInfo"), m_Servers(this)
{
	MakePopup();
	SetBounds(0, 0, 512, 384);
	m_bConnecting = false;
	m_bServerFull = false;
	m_bShowAutoRetryToggle = false;
	m_bServerNotResponding = false;
	m_bShowingExtendedOptions = false;

	m_szPassword[0] = 0;

	m_iServerID = serverID;

	m_pConnectButton = new Button(this, "Connect", "&Join Game");
	m_pCloseButton = new Button(this, "Close", "&Close");
	m_pRefreshButton = new Button(this, "Refresh", "&Refresh");
	m_pInfoLabel = new Label(this, "InfoLabel", "");
	m_pAutoRetry = new ToggleButton(this, "AutoRetry", "&Auto-Retry");
	m_pAutoRetry->AddActionSignalTarget(this);

	m_pAutoRetryAlert = new RadioButton(this, "AutoRetryAlert", "A&lert me when a player slot is available on server.");
	m_pAutoRetryJoin = new RadioButton(this, "AutoRetryJoin", "J&oin the server as soon as a player slot is available.");

	m_pAutoRetryAlert->SetSelected(true);

	m_pConnectButton->SetCommand(new KeyValues("Connect"));
	m_pCloseButton->SetCommand(new KeyValues("Close"));
	m_pRefreshButton->SetCommand(new KeyValues("Refresh"));

	m_iRequestRetry = 0;

	SetSizeable(false);

	if (gameList)
	{
		// we already have the game info, fill it in
		serveritem_t &server = gameList->GetServer(serverID);
		m_iServerID = m_Servers.AddNewServer(server);
	}
	else
	{
		// create a new server to watch
		serveritem_t server;
		memset(&server, 0, sizeof(server));
		*((int *)server.ip) = serverIP;
		server.port = serverPort;
		m_iServerID = m_Servers.AddNewServer(server);
	}

	// refresh immediately
	RequestInfo();

	// let us be ticked every frame
	ivgui()->AddTickSignal(this->GetVPanel());

	LoadControlSettings("Admin\\DialogGameInfo.res");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDialogGameInfo::~CDialogGameInfo()
{
}

//-----------------------------------------------------------------------------
// Purpose: Activates the dialog
//-----------------------------------------------------------------------------
void CDialogGameInfo::Run(const char *titleName)
{
	char buf[512];
	if (titleName)
	{
		sprintf(buf, "Game Info - %s", titleName);
	}
	else
	{
		strcpy(buf, "Game Info");
	}
	SetTitle(buf, true);

	// get the info from the user
	RequestInfo();

	RequestFocus();
}

//-----------------------------------------------------------------------------
// Purpose: Changes which server to watch
//-----------------------------------------------------------------------------
void CDialogGameInfo::ChangeGame(int serverIP, int serverPort)
{
	// check to see if it's the same game
	serveritem_t &server = m_Servers.GetServer(m_iServerID);

	if (*(int *)server.ip == serverIP && server.port == serverPort)
	{
		return;
	}

	// change the server
	m_Servers.Clear();

	// create a new server to watch
	serveritem_t newServer;
	memset(&newServer, 0, sizeof(newServer));
	*((int *)newServer.ip) = serverIP;
	newServer.port = serverPort;
	m_iServerID = m_Servers.AddNewServer(newServer);

	// start refresh immediately
	RequestInfo();
}


//-----------------------------------------------------------------------------
// Purpose: Relayouts the data
//-----------------------------------------------------------------------------
void CDialogGameInfo::PerformLayout()
{
	BaseClass::PerformLayout();

	// get the server we're watching
	serveritem_t &server = m_Servers.GetServer(m_iServerID);

	SetControlText("ServerText", server.name);
	SetControlText("GameText", server.gameDescription);
	SetControlText("MapText", server.map);

	char buf[128];
	if (server.maxPlayers > 0)
	{
		sprintf(buf, "%d / %d", server.players, server.maxPlayers);
	}
	else
	{
		buf[0] = 0;
	}
	SetControlText("PlayersText", buf);

	if (server.ip[0] && server.port)
	{
		char buf[64];
		sprintf(buf, "%d.%d.%d.%d:%d", server.ip[0], server.ip[1], server.ip[2], server.ip[3], server.port);
		SetControlText("ServerIPText", buf);
		m_pConnectButton->SetEnabled(true);
	}
	else
	{
		SetControlText("ServerIPText", "");
		m_pConnectButton->SetEnabled(false);
	}

	sprintf(buf, "%d", server.ping);
	SetControlText("PingText", buf);

	// set the info text
	if (m_pAutoRetry->IsSelected())
	{
		if (server.players < server.maxPlayers)
		{
			m_pInfoLabel->SetText("Press 'Join Game' to connect to the server.");
		}
		else if (m_pAutoRetryJoin->IsSelected())
		{
			m_pInfoLabel->SetText("You will join the server as soon as a player slot is free.");
		}
		else
		{
			m_pInfoLabel->SetText("You will be alerted as soon player slot is free on the server.");
		}
	}
	else if (m_bServerFull)
	{
		m_pInfoLabel->SetText("Could not connect - server is full.");
	}
	else if (m_bServerNotResponding)
	{
		char text[100];
		_snprintf(text,100,"Server is not responding.%d.%d.%d.%d:%d", server.ip[0], server.ip[1], server.ip[2], server.ip[3], server.port);

		m_pInfoLabel->SetText(text);
	}
	else
	{
		// clear the status
		m_pInfoLabel->SetText("");
	}

	// auto-retry layout
	m_pAutoRetry->SetVisible(m_bShowAutoRetryToggle);

	if (m_pAutoRetry->IsSelected())
	{
		m_pAutoRetryAlert->SetVisible(true);
		m_pAutoRetryJoin->SetVisible(true);
	}
	else
	{
		m_pAutoRetryAlert->SetVisible(false);
		m_pAutoRetryJoin->SetVisible(false);
	}

	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Sets up the current scheme colors
//-----------------------------------------------------------------------------
void CDialogGameInfo::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	// force the label to get it's scheme settings
	m_pInfoLabel->InvalidateLayout(true);
	// override them
	m_pInfoLabel->SetFgColor(GetSchemeColor("BrightControlText"));
}

//-----------------------------------------------------------------------------
// Purpose: Forces the game info dialog to try and connect
//-----------------------------------------------------------------------------
void CDialogGameInfo::Connect()
{
	OnConnect();
}

//-----------------------------------------------------------------------------
// Purpose: Connects the user to this game
//-----------------------------------------------------------------------------
void CDialogGameInfo::OnConnect()
{
	// flag that we are attempting connection
	m_bConnecting = true;

	// reset state
	m_bServerFull = false;
	m_bServerNotResponding = false;

	InvalidateLayout();

	// need to refresh server before attempting to connect, to make sure there is enough room on the server
	RequestInfo();
}

//-----------------------------------------------------------------------------
// Purpose: Handles Refresh button press, starts a re-ping of the server
//-----------------------------------------------------------------------------
void CDialogGameInfo::OnRefresh()
{
	// re-ask the server for the game info
	RequestInfo();
}

//-----------------------------------------------------------------------------
// Purpose: Deletes the dialog when it's closed
//-----------------------------------------------------------------------------
void CDialogGameInfo::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: Forces the whole dialog to redraw when the auto-retry button is toggled
//-----------------------------------------------------------------------------
void CDialogGameInfo::OnButtonToggled(Panel *panel)
{
	if (panel == m_pAutoRetry)
	{
		ShowAutoRetryOptions(m_pAutoRetry->IsSelected());
	}

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether the extended auto-retry options are visible or not
// Input  : state - 
//-----------------------------------------------------------------------------
void CDialogGameInfo::ShowAutoRetryOptions(bool state)
{
	// we need to extend the dialog
	int growSize = 60;
	if (!state)
	{
		growSize = -growSize;
	}

	// alter the dialog size accordingly
	int wide, tall;
	GetSize(wide, tall);
	tall += growSize;
	SetSize(wide, tall);
	
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the text of a control by name
//-----------------------------------------------------------------------------
void CDialogGameInfo::SetControlText(const char *textEntryName, const char *text)
{
	TextEntry *entry = dynamic_cast<TextEntry *>(FindChildByName(textEntryName));
	if (entry)
	{
		entry->SetText(text);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Requests the right info from the server
//-----------------------------------------------------------------------------
void CDialogGameInfo::RequestInfo()
{
	// reset the time at which we auto-refresh
	m_iRequestRetry = system()->GetTimeMillis() + RETRY_TIME;

	if (!m_Servers.IsRefreshing())
	{
		m_Servers.AddServerToRefreshList(m_iServerID);
		m_Servers.StartRefresh();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame, handles resending network messages
//-----------------------------------------------------------------------------
void CDialogGameInfo::OnTick()
{
	// check to see if we should perform an auto-refresh
	if (m_iRequestRetry && m_iRequestRetry < system()->GetTimeMillis())
	{
		// reask
		RequestInfo();
	}

	m_Servers.RunFrame();
}

//-----------------------------------------------------------------------------
// Purpose: called when the server has successfully responded
//-----------------------------------------------------------------------------
void CDialogGameInfo::ServerResponded(serveritem_t &server)
{
	if (m_bConnecting)
	{
		ConnectToServer();
	}
	else if (m_pAutoRetry->IsSelected())
	{
		// auto-retry is enabled, see if we can join
		if (server.players < server.maxPlayers)
		{
			// there is a slot free, we can join

			// make the sound
			surface()->PlaySound("Servers\\game_ready.wav");

			// flash this window
			FlashWindow();

			// if it's set, connect right away
			if (m_pAutoRetryJoin->IsSelected())
			{
				ConnectToServer();
			}
		}
	}

	m_bServerNotResponding = false;

	InvalidateLayout();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: called when a server response has timed out
//-----------------------------------------------------------------------------
void CDialogGameInfo::ServerFailedToRespond(serveritem_t &server)
{
	// the server didn't respond, mark that in the UI
	// only mark if we haven't ever received a response
	if (!server.hadSuccessfulResponse)
	{
		m_bServerNotResponding = true;
	}

	InvalidateLayout();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Connects to the server
//-----------------------------------------------------------------------------
void CDialogGameInfo::ConnectToServer()
{
	m_bConnecting = false;
	serveritem_t &server = m_Servers.GetServer(m_iServerID);

	// check to see if we need a password
	if (server.password && !m_szPassword[0])
	{
		CDialogServerPassword *box = new CDialogServerPassword();
		box->AddActionSignalTarget(this);
		box->Activate(server.name, server.serverID);
		return;
	}

	// check the player count
	if (server.players >= server.maxPlayers)
	{
		// mark why we cannot connect
		m_bServerFull = true;
		// give them access to auto-retry options
		m_bShowAutoRetryToggle = true;
		InvalidateLayout();
		return;
	}

	// tell the engine to connect
	char buf[64];
	sprintf(buf, "%d.%d.%d.%d:%d", server.ip[0], server.ip[1], server.ip[2], server.ip[3], server.port);

	const char *gameDir = server.gameDir;
	if (g_pRunGameEngine->IsRunning())
	{
		char command[256];

		// set the server password, if any
		if (m_szPassword[0])
		{
			sprintf(command, "password \"%s\"\n", m_szPassword);
			g_pRunGameEngine->AddTextCommand(command);
		}

		// send engine command to change servers
		sprintf(command, "connect %s\n", buf);
		g_pRunGameEngine->AddTextCommand(command);
	}
	else
	{
		char command[256];
//		sprintf(command, " -game %s +connect %s", gameDir, buf);
		sprintf(command, " +connect %s", buf);
		if (m_szPassword[0])
		{
			strcat(command, " +password \"");
			strcat(command, m_szPassword);
			strcat(command, "\"");\
		}
		
		g_pRunGameEngine->RunEngine(gameDir, command);
	}

	// close this dialog
	PostMessage(this, new KeyValues("Close"));
}


//-----------------------------------------------------------------------------
// Purpose: called when the current refresh list is complete
//-----------------------------------------------------------------------------
void CDialogGameInfo::RefreshComplete()
{
}

//-----------------------------------------------------------------------------
// Purpose: handles response from the get password dialog
//-----------------------------------------------------------------------------
void CDialogGameInfo::OnJoinServerWithPassword(const char *password)
{
	// copy out the password
	v_strncpy(m_szPassword, password, sizeof(m_szPassword));

	// retry connecting to the server again
	OnConnect();
}


//-----------------------------------------------------------------------------
// Purpose: Message map
//-----------------------------------------------------------------------------
MessageMapItem_t CDialogGameInfo::m_MessageMap[] =
{
	MAP_MESSAGE( CDialogGameInfo, "Refresh", OnRefresh ),
	MAP_MESSAGE( CDialogGameInfo, "Connect", OnConnect ),

	MAP_MESSAGE_PTR( CDialogGameInfo, "ButtonToggled", OnButtonToggled, "panel" ),
	MAP_MESSAGE_PTR( CDialogGameInfo, "RadioButtonChecked", OnButtonToggled, "panel" ),

	MAP_MESSAGE_CONSTCHARPTR( CDialogGameInfo, "JoinServerWithPassword", OnJoinServerWithPassword, "password" ),
};

IMPLEMENT_PANELMAP( CDialogGameInfo, Frame );
