//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include "winsock.h" // this BUGGER defines PropertySheet to PropertySheetA ....
#undef PropertySheet
#include "tokenline.h"

#include "GamePanelInfo.h"
//#include "Info.h"
#include "IRunGameEngine.h"

#include "DialogCvarChange.h"
#include "DialogAddBan.h"
#include "BudgetPanelContainer.h"

#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <KeyValues.h>
#include <vgui/Cursor.h>
#include <vgui/ILocalize.h>

#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/QueryBox.h>
#include <vgui_controls/Image.h>
#include <vgui_controls/ImagePanel.h>

#include "tier0/icommandline.h"

#include <proto_oob.h>
#include <netadr.h>

using namespace vgui;

static const long RETRY_TIME = 10000;		// refresh server every 10 seconds
static const long MAP_CHANGE_TIME = 20000;		// refresh 20 seconds after a map change 
static const long RESTART_TIME = 60000;		// refresh 60 seconds after a "_restart"

#include "IManageServer.h"


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGamePanelInfo::CGamePanelInfo(vgui::Panel *parent, const char *name, const char *mod) :  Frame(parent, name)
{
	SetSize(560, 420);
	SetMinimumSize(560, 420);
	m_bRemoteServer = false;
	m_bShuttingDown = false;

	// Create an Animating Image Panel
	m_pAnimImagePanel = new AnimatingImagePanel(this, "AnAnimatingImagePanel");

	// Each image file is named ss1, ss2, ss3... ss20, one image for each frame of the animation.
	// This loads the 20 images in to the Animation class.
	m_pAnimImagePanel->LoadAnimation("resource\\steam\\g", 12);
	
	//!! animation temporarily disabled until UI pass done
	//m_pAnimImagePanel->StartAnimation();
	m_pAnimImagePanel->SetVisible(false);

	// the main container for the various sub panels
	m_pDetailsSheet = new PropertySheet(this, "Panels");

	// the sub panels
	m_pPlayerListPanel = new CPlayerPanel(this, "Player List");
	m_pBanListPanel = new CBanPanel(this, "Ban List");
	m_pServerLogPanel = new CRawLogPanel(this, "ServerLog");
// chat panel disabled until we get the parsing done
//	m_pServerChatPanel = new CChatPanel(this, "ChatPanel");
	m_pServerConfigPanel = new CServerConfigPanel(this, "ServerConfigPanel", mod);
	m_pGraphsPanel = new CGraphPanel(this,"GraphsPanel");
	m_pServerInfoPanel = new CServerInfoPanel(this, "ServerInfo");
	
	if ( CommandLine()->CheckParm( "-BudgetPanel" ) )
		m_pBudgetPanel = new CBudgetPanelContainer( this, "BudgetPanel" );
	else
		m_pBudgetPanel = NULL;

	m_pServerInfoPanel->AddActionSignalTarget(this);

	m_pDetailsSheet->AddPage(m_pServerInfoPanel,"#Game_Main_Settings");
	m_pDetailsSheet->AddPage(m_pServerConfigPanel,"#Game_Configure");
	m_pDetailsSheet->AddPage(m_pGraphsPanel,"#Game_Server_Statistics");
	m_pDetailsSheet->AddPage(m_pPlayerListPanel,"#Game_Current_Players");
	m_pDetailsSheet->AddPage(m_pBanListPanel,"#Game_Bans");
	
	if ( m_pBudgetPanel )
		m_pDetailsSheet->AddPage(m_pBudgetPanel,"#Game_Budgets");

// chat panel disabled until we get the parsing done
//	m_pDetailsSheet->AddPage(m_pServerChatPanel,"#Game_Chat");
	m_pDetailsSheet->AddPage(m_pServerLogPanel,"#Game_Console");

	// let us be ticked every frame
	ivgui()->AddTickSignal(this->GetVPanel());

	LoadControlSettingsAndUserConfig("Admin\\DialogGamePanelInfo.res");

	SetNewTitle(false, name);
	SetVisible(true);	
	
	MoveToCenterOfScreen();
	RequestFocus();
	MoveToFront();

	//!! hack, force the server info panel to refresh fast 
	// because the info it receives while loading the server is wrong
	PostMessage(m_pServerInfoPanel, new KeyValues("ResetData"), 0.1f);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGamePanelInfo::~CGamePanelInfo()
{
}

//-----------------------------------------------------------------------------
// Purpose: Sets the title of the dialog
//-----------------------------------------------------------------------------
void CGamePanelInfo::SetNewTitle(bool connectionFailed, const char *additional_text)
{
	const char *localized_title = "#Game_RemoteTitle";
	if (!m_bRemoteServer)
{
		localized_title = "Game_LocalTitle";
	}
	else if (connectionFailed)
	{
		localized_title = "#Game_RemoteTitle_Failed";
	}

	wchar_t serverName[256];
	g_pVGuiLocalize->ConvertANSIToUnicode(additional_text, serverName, sizeof(serverName));
	wchar_t title[256];
	g_pVGuiLocalize->ConstructString(title, sizeof(title), g_pVGuiLocalize->Find(localized_title), 1, serverName);
	
	SetTitle(title, true);
}

//-----------------------------------------------------------------------------
// Purpose: Updates the title
//-----------------------------------------------------------------------------
void CGamePanelInfo::OnUpdateTitle()
{
	SetNewTitle(false, m_pServerInfoPanel->GetHostname());
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CGamePanelInfo::SetAsRemoteServer(bool remote)
{
	m_bRemoteServer = remote;
}

//-----------------------------------------------------------------------------
// Purpose: Relayouts the data
//-----------------------------------------------------------------------------
void CGamePanelInfo::PerformLayout()
{
	BaseClass::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame, handles resending network messages
//-----------------------------------------------------------------------------
void CGamePanelInfo::OnTick()
{
	// service the server I/O queue
	RemoteServer().ProcessServerResponse();
}

//-----------------------------------------------------------------------------
// Purpose: handles button responses
//-----------------------------------------------------------------------------
void CGamePanelInfo::OnCommand(const char *command)
{
	if (!stricmp(command, "stop2")) 
	{
		RemoteServer().SendCommand("quit");
		m_bShuttingDown = true;
		Close();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

//-----------------------------------------------------------------------------
// Purpose: produces a dialog asking a player to enter a new ban
//-----------------------------------------------------------------------------
void CGamePanelInfo::OnStop()
{	
	QueryBox *box = new QueryBox("#Game_Stop_Server_Title", "#Game_Restart_Server");
	box->AddActionSignalTarget(this);
	box->SetOKButtonText("#Game_Stop_Server");
	box->SetOKCommand(new KeyValues("Command", "command", "stop2"));
	box->ShowWindow();
}

//-----------------------------------------------------------------------------
// Purpose: displays the help page
//-----------------------------------------------------------------------------
void CGamePanelInfo::OnHelp()
{
	system()->ShellExecute("open", "Admin\\Admin.html");
}

//-----------------------------------------------------------------------------
// Purpose: Does any processing needed before closing the dialog
//-----------------------------------------------------------------------------
void CGamePanelInfo::OnClose()
{
	if (m_bRemoteServer || m_bShuttingDown)
	{
		BaseClass::OnClose();
		return;
	}

	// closing the window will kill the local server, notify user
	OnStop();
}

//-----------------------------------------------------------------------------
// Purpose: allow the build mode editor to edit the current sub panel
//-----------------------------------------------------------------------------
void CGamePanelInfo::ActivateBuildMode()
{
//	BaseClass::ActivateBuildMode();
//	return;
	// no subpanel, no build mode
	EditablePanel *pg = dynamic_cast<EditablePanel *>(m_pDetailsSheet->GetActivePage());
	if (pg)
	{
		pg->ActivateBuildMode();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Writes text to the console
//-----------------------------------------------------------------------------
void CGamePanelInfo::AddToConsole(const char *msg)
{
	if (m_pServerLogPanel)
	{
		// hack, look for restart message
		if (*msg == 3 && !strncmp(msg + 1, "MasterRequestRestart", strlen("MasterRequestRestart")))
		{
			OnMasterRequestRestart();
		}
		else if (*msg == 3 && !strncmp(msg + 1, "MasterOutOfDate", strlen("MasterOutOfDate")))
		{
			const char *details = strstr( msg, "MasterOutOfDate" );
			if ( details )
			{
				OnMasterOutOfDate(details + strlen("MasterOutOfDate"));
			}
		}
		else
		{
			// nothing special, just print
			m_pServerLogPanel->DoInsertString(msg);
		}
	}
}

void CGamePanelInfo::OnMasterOutOfDate( const char *msg)
{
#if !defined(_DEBUG)

	// open a dialog informing user that they need to restart the server
	if (!m_hOutOfDateQueryBox.Get())
	{
		char *fullmsg = (char *) _alloca( strlen(msg) + strlen( "\n\nDo you wish to shutdown now?\n") + 1 );

		_snprintf( fullmsg, strlen(msg) + strlen( "\n\nDo you wish to shutdown now?\n") + 1 , "%s\n\nDo you wish to shutdown now?\n", msg );
		m_hOutOfDateQueryBox = new QueryBox("Server restart pending", fullmsg);
		m_hOutOfDateQueryBox->AddActionSignalTarget(this);
		m_hOutOfDateQueryBox->SetOKCommand(new KeyValues("RestartServer"));
		m_hOutOfDateQueryBox->ShowWindow();
	}
	else
	{
		// reshow the existing window
		m_hOutOfDateQueryBox->Activate();
	}

#endif // !defined(_DEBUG)
}

//-----------------------------------------------------------------------------
// Purpose: Called when master server has requested the dedicated server restart
//-----------------------------------------------------------------------------
void CGamePanelInfo::OnMasterRequestRestart()
{
#if !defined(_DEBUG)

	// open a dialog informing user that they need to restart the server
	if (!m_hRestartQueryBox.Get())
	{
		m_hRestartQueryBox = new QueryBox("Server restart needed", "Your server is out of date, and will not be listed\non the master server until you restart.\n\nDo you wish to shutdown now?\n");
		m_hRestartQueryBox->AddActionSignalTarget(this);
		m_hRestartQueryBox->SetOKCommand(new KeyValues("RestartServer"));
		m_hRestartQueryBox->ShowWindow();
	}
	else
	{
		// reshow the existing window
		m_hRestartQueryBox->Activate();
	}

#endif // !defined(_DEBUG)
}

//-----------------------------------------------------------------------------
// Purpose: Restarts the server
//-----------------------------------------------------------------------------
void CGamePanelInfo::OnRestartServer()
{
	//!! mark us as needing restart
	//!! this doesn't work yet, just shut us down

	// shut us down
	RemoteServer().SendCommand("quit");
	m_bShuttingDown = true;
	Close();
}
