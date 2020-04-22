//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "BaseGamesPage.h"
#include "ServerListCompare.h"
#include "util.h"
#include "serverpage.h"

#include <VGUI_Controls.h>
#include <VGUI_CheckButton.h>
#include <VGUI_ComboBox.h>
#include <VGUI_ImagePanel.h>
#include <VGUI_IScheme.h>
#include <VGUI_IVGui.h>
#include <VGUI_ListPanel.h>
#include <VGUI_MenuButton.h>
#include <VGUI_Menu.h>
#include <VGUI_KeyValues.h>
#include <VGUI_MouseCode.h>

#include <stdio.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBaseGamesPage::CBaseGamesPage(vgui::Panel *parent, const char *name) : Frame(parent, name), m_Servers(this)
{
	ivgui()->AddTickSignal(GetVPanel());

	//SetSize(500, 500);
	
	//SetParent(parent); // doesn't have any effect....
	m_pParent=parent;

	// load the password icon
	m_pPasswordIcon = new ImagePanel(NULL, NULL);
	m_pPasswordIcon->SetImage(scheme()->GetImage(scheme()->GetDefaultScheme(), "server/icon_password"));

	// Init UI
//	m_pConnect = new Button(this, "ConnectButton", "Connect");
	m_pRefresh = new Button(this, "RefreshButton", "Refresh");
	m_pRefresh->SetCommand("refresh");
	//m_pRefresh->AddActionSignalTarget(this);

	m_pAddIP = new Button(this, "AddIPButton", "Add IP");

	m_pManage = new Button(this, "ManageButton", "Manage");
	m_pManage->SetCommand(new KeyValues("Manage"));
	m_pManage->AddActionSignalTarget(this);


//	m_pRefreshMenu = new Menu(this, "RefreshMenu");
//	m_pRefreshMenu->MakePopup();
	//m_pRefresh->SetMenu(m_pRefreshMenu);
	//m_pRefresh->SetOpenDirection(MenuButton::UP);
	//m_pRefreshMenu->AddMenuItem("Refresh", "Get new info for servers in current list  ", "refresh", this);
	//m_pRefreshMenu->AddMenuItem("GetNewList", "Get new server list  ", "getnewlist", this);
	//m_pRefreshMenu->AddMenuItem("StopRefresh", "Stop refreshing server list  ", "stoprefresh", this);
	m_pGameList = new OurListPanel(this, "gamelist");

	// Add the column headers
	m_pGameList->AddColumnHeader(0, "Password", util->GetString(""), 20, false, NOT_RESIZABLE, NOT_RESIZABLE );
	m_pGameList->AddColumnHeader(1, "Name", util->GetString(" Servers"), 50, true,  RESIZABLE, RESIZABLE);
	m_pGameList->AddColumnHeader(2, "GameDesc", util->GetString(" Game"), 80, true, RESIZABLE, NOT_RESIZABLE);
	m_pGameList->AddColumnHeader(3, "Players", util->GetString(" Players"), 55, true, RESIZABLE, NOT_RESIZABLE);
	m_pGameList->AddColumnHeader(4, "Map", util->GetString(" Map" ), 90, true, RESIZABLE, NOT_RESIZABLE);
	m_pGameList->AddColumnHeader(5, "Ping", util->GetString(" Latency" ), 55, true, RESIZABLE, NOT_RESIZABLE);

	// setup fast sort functions
	m_pGameList->SetSortFunc(0, PasswordCompare);
	m_pGameList->SetSortFunc(1, ServerNameCompare);
	m_pGameList->SetSortFunc(2, GameCompare);
	m_pGameList->SetSortFunc(3, PlayersCompare);
	m_pGameList->SetSortFunc(4, MapCompare);
	m_pGameList->SetSortFunc(5, PingCompare);

	// Sort by ping time by default
	m_pGameList->SetSortColumn(5);
	
	m_pGameList->AddActionSignalTarget(this);

//	LoadControlSettings("Admin\\DialogAdminServerPage.res");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBaseGamesPage::~CBaseGamesPage()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseGamesPage::PerformLayout()
{
	BaseClass::PerformLayout();

/*	// game list in middle
	int x = 0, y = 0, wide, tall;
	GetSize(wide, tall);
	m_pGameList->SetBounds(10, 30, wide - 20, tall - 200);

	Repaint();
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnTick()
{
	m_Servers.RunFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseGamesPage::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	m_pGameList->SetFont(scheme()->GetFont(scheme()->GetDefaultScheme(), "DefaultSmall"));
}

//-----------------------------------------------------------------------------
// Purpose: gets information about specified server
//-----------------------------------------------------------------------------
serveritem_t &CBaseGamesPage::GetServer(unsigned int serverID)
{
	return m_Servers.GetServer(serverID);
}


//-----------------------------------------------------------------------------
// Purpose: call to let the UI now whether the game list is currently refreshing
//-----------------------------------------------------------------------------
void CBaseGamesPage::SetRefreshing(bool state)
{
	if(!CServerPage::GetInstance())
	{
		return;
	}

	if (state)
	{
		CServerPage::GetInstance()->UpdateStatusText("Refreshing server list...");
	}
	else
	{
		CServerPage::GetInstance()->UpdateStatusText("");
	}

//	m_pRefreshMenu->FindChildByName("Refresh")->SetVisible(!state);
	//m_pRefreshMenu->FindChildByName("GetNewList")->SetVisible(!state);
//	m_pRefreshMenu->FindChildByName("StopRefresh")->SetVisible(state);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnCommand(const char *command)
{
	if (!stricmp(command, "Connect"))
	{
		OnBeginConnect();
	}
	else if (!stricmp(command, "stoprefresh"))
	{
		// cancel the existing refresh
		StopRefresh();
	}
	else if (!stricmp(command, "refresh"))
	{
		// start a new refresh
		StartRefresh();
	}
	else if (!stricmp(command, "GetNewList"))
	{
		GetNewServerList();
	}
	else if (!stricmp(command, "addip"))
	{
		PostMessage(this,new KeyValues("AddServerByName")); // CFavorites handles this message
	}
	else if (!stricmp(command, "config"))
	{
		CServerPage::GetInstance()->ConfigPanel();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when the game dir combo box is changed
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnTextChanged(Panel *panel, const char *text)
{
}

//-----------------------------------------------------------------------------
// Purpose: Handles filter dropdown being toggled
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnButtonToggled(Panel *panel, int state)
{

		// treat changing these buttons like any other filter has changed
		OnTextChanged(panel, "");

}

void CBaseGamesPage::OnManage()
{
	if (m_pGameList->GetNumSelectedRows())
	{
		// get the server
		unsigned int serverID = m_pGameList->GetDataItem(m_pGameList->GetSelectedRow(0))->userData;


		PostMessage(m_pParent->GetVPanel(),  new KeyValues("Manage", "serverID", serverID));
	}
}


void CBaseGamesPage::OurListPanel::OnMouseDoublePressed( vgui::MouseCode code )
{
	PostMessage(m_pParent->GetVPanel(),  new KeyValues("Manage"));	
}

//-----------------------------------------------------------------------------
// Purpose: Message map
//-----------------------------------------------------------------------------
MessageMapItem_t CBaseGamesPage::m_MessageMap[] =
{
	MAP_MESSAGE_PTR_INT( CBaseGamesPage, "ButtonToggled", OnButtonToggled, "panel", "state" ),
	MAP_MESSAGE_PTR_CONSTCHARPTR( CBaseGamesPage, "TextChanged", OnTextChanged, "panel", "text" ),
	MAP_MESSAGE( CBaseGamesPage , "Manage",OnManage ),
};
IMPLEMENT_PANELMAP(CBaseGamesPage, BaseClass);
