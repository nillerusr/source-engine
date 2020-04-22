//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "pch_serverbrowser.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDialogServerPassword::CDialogServerPassword(vgui::Panel *parent) : Frame(parent, "DialogServerPassword")
{
	m_iServerID = -1;
	SetSize(320, 240);
	SetDeleteSelfOnClose(true);
	SetSizeable(false);

	m_pInfoLabel = new Label(this, "InfoLabel", "#ServerBrowser_ServerRequiresPassword");
	m_pGameLabel = new Label(this, "GameLabel", "<game label>");
	m_pPasswordEntry = new TextEntry(this, "PasswordEntry");
	m_pConnectButton = new Button(this, "ConnectButton", "#ServerBrowser_Connect");
	m_pPasswordEntry->SetTextHidden(true);

	LoadControlSettings("Servers/DialogServerPassword.res");

	SetTitle("#ServerBrowser_ServerRequiresPasswordTitle", true);

	// set our initial position in the middle of the workspace
	MoveToCenterOfScreen();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDialogServerPassword::~CDialogServerPassword()
{
}

//-----------------------------------------------------------------------------
// Purpose: initializes the dialog and brings it to the foreground
//-----------------------------------------------------------------------------
void CDialogServerPassword::Activate(const char *serverName, unsigned int serverID)
{
	m_pGameLabel->SetText(serverName);
	m_iServerID = serverID;

	m_pConnectButton->SetAsDefaultButton(true);
	m_pPasswordEntry->RequestFocus();
	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CDialogServerPassword::OnCommand(const char *command)
{
	bool bClose = false;

	if (!Q_stricmp(command, "Connect"))
	{
		KeyValues *msg = new KeyValues("JoinServerWithPassword");
		char buf[64];
		m_pPasswordEntry->GetText(buf, sizeof(buf)-1);
		msg->SetString("password", buf);
		msg->SetInt("serverID", m_iServerID);
		PostActionSignal(msg);

		bClose = true;
	}
	else if (!Q_stricmp(command, "Close"))
	{
		bClose = true;
	}
	else
	{
		BaseClass::OnCommand(command);
	}

	if (bClose)
	{
		PostMessage(this, new KeyValues("Close"));
	}
}

