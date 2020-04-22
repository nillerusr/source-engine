//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>

#include "PlayerPanel.h"
#include "PlayerContextMenu.h"
#include "PlayerListCompare.h"
#include "DialogAddBan.h"

#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui/IVGui.h>
#include <vgui/KeyCode.h>
#include <KeyValues.h>

#include <vgui_controls/Button.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/QueryBox.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CPlayerPanel::CPlayerPanel(vgui::Panel *parent, const char *name) : vgui::PropertyPage(parent, name)
{
	m_pPlayerListPanel = new vgui::ListPanel(this, "Players list");

	m_pPlayerListPanel->AddColumnHeader(0, "name", "#Player_Panel_Name", 200, ListPanel::COLUMN_RESIZEWITHWINDOW );
	m_pPlayerListPanel->AddColumnHeader(1, "authid", "#Player_Panel_ID", 100);
	m_pPlayerListPanel->AddColumnHeader(2, "ping", "#Player_Panel_Ping", 50);
	m_pPlayerListPanel->AddColumnHeader(3, "loss", "#Player_Panel_Loss", 50);
	m_pPlayerListPanel->AddColumnHeader(4, "frags", "#Player_Panel_Frags", 50);
	m_pPlayerListPanel->AddColumnHeader(5, "time", "#Player_Panel_Time", 75);

/*
	// TODO: update me!!	
  m_pPlayerListPanel->SetSortFunc(0, PlayerNameCompare);
	m_pPlayerListPanel->SetSortFunc(1, PlayerAuthCompare);
	m_pPlayerListPanel->SetSortFunc(2, PlayerPingCompare);
	m_pPlayerListPanel->SetSortFunc(3, PlayerLossCompare);
	m_pPlayerListPanel->SetSortFunc(4, PlayerFragsCompare);
*/
	m_pPlayerListPanel->SetSortFunc(5, PlayerTimeCompare);
	m_pPlayerListPanel->SetEmptyListText("#Player_Panel_No_Players");
	// Sort by ping time by default
	m_pPlayerListPanel->SetSortColumn(0);

	m_pKickButton = new Button(this, "Kick", "#Player_Panel_Kick");
	m_pBanButton = new Button(this, "Ban", "#Player_Panel_Ban");

	m_pPlayerContextMenu = new CPlayerContextMenu(this);
	m_pPlayerContextMenu->SetVisible(false);

	LoadControlSettings("Admin/PlayerPanel.res", "PLATFORM");

	m_pKickButton->SetCommand(new KeyValues("KickPlayer"));
	m_pBanButton->SetCommand(new KeyValues("BanPlayer"));

	OnItemSelected(); // disable the buttons

	m_flUpdateTime = 0.0f;
	RemoteServer().AddServerMessageHandler(this, "UpdatePlayers");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CPlayerPanel::~CPlayerPanel()
{
	RemoteServer().RemoveServerDataResponseTarget(this);
}

//-----------------------------------------------------------------------------
// Purpose: Rebuilds player info
//-----------------------------------------------------------------------------
void CPlayerPanel::OnResetData()
{
	RemoteServer().RequestValue(this, "playerlist");

	// update once every minute
	m_flUpdateTime = (float)system()->GetFrameTime() + (60 * 1.0f);
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if we should update player info
//-----------------------------------------------------------------------------
void CPlayerPanel::OnThink()
{
	if (m_flUpdateTime < vgui::system()->GetFrameTime())
	{
		OnResetData();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Refreshes page if user hits F5
//-----------------------------------------------------------------------------
void CPlayerPanel::OnKeyCodeTyped(vgui::KeyCode code)
{
	if (code == KEY_F5)
	{
		OnResetData();
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns data on the player who is currently selected in the list
//-----------------------------------------------------------------------------
KeyValues *CPlayerPanel::GetSelected()
{
	return m_pPlayerListPanel->GetItem(m_pPlayerListPanel->GetSelectedItem(0));
}

static const char *FormatSeconds( int seconds )
{
	static char string[64];

	int hours = 0;
	int minutes = seconds / 60;

	if ( minutes > 0 )
	{
		seconds -= (minutes * 60);
		hours = minutes / 60;

		if ( hours > 0 )
		{
			minutes -= (hours * 60);
		}
	}
	
	if ( hours > 0 )
	{
		Q_snprintf( string, sizeof(string), "%2i:%02i:%02i", hours, minutes, seconds );
	}
	else
	{
		Q_snprintf( string, sizeof(string), "%02i:%02i", minutes, seconds );
	}

	return string;
}

//-----------------------------------------------------------------------------
// Purpose: called when the server has returned a requested value
//-----------------------------------------------------------------------------
void CPlayerPanel::OnServerDataResponse(const char *value, const char *response)
{
	if (!stricmp(value, "UpdatePlayers"))
	{
		// server has indicated a change, force an update
		m_flUpdateTime = 0.0f;
	}
	else if (!stricmp(value, "playerlist"))
	{
		// new list of players
		m_pPlayerListPanel->DeleteAllItems();
		
		// parse response
		const char *parse = response;
		while (parse && *parse)
		{
			// first should be the size of the player name
			char name[64];
			name[0] = '\0';
			char authID[64];
			authID[0] = '\0';
			char netAdr[32];
			netAdr[0] = '\0';
			int ping = 0, packetLoss = 0, frags = 0, connectTime = 0;

			ivgui()->DPrintf2("orig:  %s\n", parse);

			// parse out the name, which is quoted
			Assert(*parse == '\"');
			if (*parse != '\"')
				break;
			++parse;  // move past start quote
			int pos = 0;
			while (*parse && *parse != '\"')
			{
				name[pos++] = *parse;
				parse++;
			}
			name[pos] = 0;
			parse++;	// move past end quote

			if (6 != sscanf(parse, " %s %s %d %d %d %d\n", authID, netAdr, &ping, &packetLoss, &frags, &connectTime))
				break;

			const char *timeStr = FormatSeconds(connectTime);

			ivgui()->DPrintf2("pars:  \"%s\" %s %s %d %d %d %s\n", name, authID, netAdr, ping, packetLoss, frags, timeStr);

			// add to list
			KeyValues *player = new KeyValues("Player");
			player->SetString("name", name);
			player->SetString("authID", authID);
			player->SetString("netAdr", netAdr);
			player->SetInt("ping", ping);
			player->SetInt("loss", packetLoss);
			player->SetInt("frags", frags);
			player->SetString("time", timeStr);
			m_pPlayerListPanel->AddItem(player, 0, false, false);

			// move to next line
			parse = strchr(parse, '\n');
			if (parse)
			{
				parse++;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Buttom command handlers
//-----------------------------------------------------------------------------
void CPlayerPanel::OnCommand(const char *command)
{
	BaseClass::OnCommand(command);
}

//-----------------------------------------------------------------------------
// Purpose: Handles the UI for kicking a player from the server
//-----------------------------------------------------------------------------
void CPlayerPanel::OnKickButtonPressed()
{
	if (m_pPlayerListPanel->GetSelectedItemsCount() < 1)
		return;

	// open a message box to ask the user if they want to follow through on this
	QueryBox *box; 
	if (m_pPlayerListPanel->GetSelectedItemsCount() > 1)
	{
		box = new QueryBox("#Kick_Multiple_Players_Title", "#Kick_Multiple_Players_Question");
	}
	else
	{
		// show the player name in the message box if only one player is being booted
		KeyValues *kv = m_pPlayerListPanel->GetItem(m_pPlayerListPanel->GetSelectedItem(0));
		Assert(kv != NULL);
		if (!kv)
			return;

		wchar_t playerName[64];
		g_pVGuiLocalize->ConvertANSIToUnicode( kv->GetString("name"), playerName, sizeof(playerName) );

		wchar_t msg[512];
		g_pVGuiLocalize->ConstructString( msg, sizeof(msg), g_pVGuiLocalize->Find("Kick_Single_Player_Question"), 1, playerName );
		box = new QueryBox(g_pVGuiLocalize->Find("#Kick_Single_Player_Title"), msg);
	}
	box->AddActionSignalTarget(this);
	box->SetOKCommand(new KeyValues("KickSelectedPlayers"));
	box->ShowWindow();
}

//-----------------------------------------------------------------------------
// Purpose: Prompts a user to be banned
//-----------------------------------------------------------------------------
void CPlayerPanel::OnBanButtonPressed()
{
	if (m_pPlayerListPanel->GetSelectedItemsCount() != 1)
		return;

	// open a message box to ask the user if they want to follow through on this
	KeyValues *kv = m_pPlayerListPanel->GetItem(m_pPlayerListPanel->GetSelectedItem(0));
	Assert(kv != NULL);
	if (!kv)
		return;

	const char *player = kv->GetString("name");	
	const char *authid = kv->GetString("authid");
	const char *netAdr = kv->GetString("netAdr");

	char buf[64];
	if ( !strcmp( authid, "UNKNOWN" ) )
	{
		int s1, s2, s3, s4;
		if (4 == sscanf(netAdr, "%d.%d.%d.%d", &s1, &s2, &s3, &s4))
		{
			Q_snprintf( buf, sizeof(buf), "%d.%d.%d.%d", s1, s2, s3, s4 );
			authid = buf;
		}
	}

	CDialogAddBan *box = new CDialogAddBan(this);
	box->AddActionSignalTarget(this);
	box->Activate("addban", player, authid);
}

//-----------------------------------------------------------------------------
// Purpose: Kicks all the players currently selected
//-----------------------------------------------------------------------------
void CPlayerPanel::KickSelectedPlayers()
{
	for (int i = 0; i < m_pPlayerListPanel->GetSelectedItemsCount(); i++)
	{
		// get the player info
		int row = m_pPlayerListPanel->GetSelectedItem(i);
		KeyValues *pl = m_pPlayerListPanel->GetItem(row);
		if (!pl)
			continue;

		// kick 'em
		char cmd[512];
		_snprintf(cmd, sizeof(cmd), "kick \"%s\"", pl->GetString("name"));
		RemoteServer().SendCommand(cmd);
	}

	m_pPlayerListPanel->ClearSelectedItems();
	m_pKickButton->SetEnabled(false);
	m_pBanButton->SetEnabled(false);
	OnResetData();
}

//-----------------------------------------------------------------------------
// Purpose: Adds a new ban
//-----------------------------------------------------------------------------
void CPlayerPanel::AddBanByID(const char *id, const char *newtime)
{
	Assert(id && *id);
	if (!id || !*id)
		return;

	// if the newtime string is not valid, then set it to 0 (permanent ban)
	if (!newtime || atof(newtime) < 0.001)
	{
		newtime = "0";
	}

	const char *banCmd = "banid";
	const char *saveCmd = "writeip";
	int s1, s2, s3, s4;
	if (4 == sscanf(id, "%d.%d.%d.%d", &s1, &s2, &s3, &s4))
	{
		banCmd = "addip";
		saveCmd = "writeid";
	}

	// send down the ban command
	char cmd[512];
	_snprintf(cmd, sizeof(cmd) -1, "%s %s %s\n", banCmd, newtime, id);
	RemoteServer().SendCommand(cmd);

	// force the file to update
	RemoteServer().SendCommand(saveCmd);

	// refresh
	OnResetData();
}

//-----------------------------------------------------------------------------
// Purpose: opens context menu (user right clicked on a server)
//-----------------------------------------------------------------------------
void CPlayerPanel::OnOpenContextMenu(int itemID)
{
/* CODE DISABLED UNTIL VERIFIED AS WORKING
	// show the player menus only if the cursor is over it
	if (IsCursorOver() && m_pPlayerListPanel->GetNumSelectedRows()) 
	{
		// get the server
		unsigned int playerID = m_pPlayerListPanel->GetDataItem(m_pPlayerListPanel->GetSelectedRow(0))->userData;
		
		// activate context menu
		m_pPlayerContextMenu->ShowMenu(this, playerID);
	}
*/
}

//-----------------------------------------------------------------------------
// Purpose: called when an item on the list panel is selected.
//-----------------------------------------------------------------------------
void CPlayerPanel::OnItemSelected()
{
	bool state = true;

	if ( m_pPlayerListPanel->GetSelectedItemsCount() == 0 )
	{
		state = false;
	}

	m_pKickButton->SetEnabled(state);
	m_pBanButton->SetEnabled(state);

	if ( m_pPlayerListPanel->GetSelectedItemsCount() > 1 )
	{
		m_pBanButton->SetEnabled(false);
	}
}
