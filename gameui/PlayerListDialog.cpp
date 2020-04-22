//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "PlayerListDialog.h"

#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui_controls/ListPanel.h>
#include <KeyValues.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/ComboBox.h>

#include "EngineInterface.h"
#include "game/client/IGameClientExports.h"
#include "GameUI_Interface.h"
#include "steam/steam_api.h"
#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CPlayerListDialog::CPlayerListDialog(vgui::Panel *parent) : BaseClass(parent, "PlayerListDialog")
{
	SetSize(320, 240);
	SetTitle("#GameUI_CurrentPlayers", true);

	m_pMuteButton = new Button(this, "MuteButton", "");

	m_pPlayerList = new ListPanel(this, "PlayerList");
	m_pPlayerList->AddColumnHeader(0, "Name", "#GameUI_PlayerName", 180);
	m_pPlayerList->AddColumnHeader(1, "Properties", "#GameUI_Properties", 80);

	m_pPlayerList->SetEmptyListText("#GameUI_NoOtherPlayersInGame");

	LoadControlSettings("Resource/PlayerListDialog.res");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CPlayerListDialog::~CPlayerListDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerListDialog::Activate()
{
	BaseClass::Activate();

	// refresh player list
	m_pPlayerList->DeleteAllItems();
	int maxClients = engine->GetMaxClients();
	for (int i = 1; i <= maxClients; i++)
	{
		// get the player info from the engine	
		player_info_t pi;

		if ( !engine->GetPlayerInfo(i, &pi) )
			continue;

		char szPlayerIndex[32];
		Q_snprintf(szPlayerIndex, sizeof( szPlayerIndex ), "%d", i);

		// collate user data then add it to the table
		KeyValues *data = new KeyValues(szPlayerIndex);
		
		data->SetString("Name", pi.name );
		data->SetInt("index", i);

		// add to the list
		m_pPlayerList->AddItem(data, 0, false, false);
	}

	// refresh player properties info
	RefreshPlayerProperties();

	// select the first item by default
	m_pPlayerList->SetSingleSelectedItem( m_pPlayerList->GetItemIDFromRow(0) );

	// toggle button states
	OnItemSelected();
}

//-----------------------------------------------------------------------------
// Purpose: walks the players and sets their info display in the list
//-----------------------------------------------------------------------------
void CPlayerListDialog::RefreshPlayerProperties()
{
	for (int i = 0; i <= m_pPlayerList->GetItemCount(); i++)
	{
		KeyValues *data = m_pPlayerList->GetItem(i);
		if (!data)
			continue;

		// assemble properties
		int playerIndex = data->GetInt("index");
		player_info_t pi;

		if ( !engine->GetPlayerInfo( playerIndex, &pi) )
		{
			// disconnected
			data->SetString("properties", "Disconnected");
			continue;
		}

		data->SetString( "name", pi.name );

		bool muted = false, friends = false, bot = false;
		
		if ( GameClientExports() && GameClientExports()->IsPlayerGameVoiceMuted(playerIndex) )
		{
			muted = true;
		}
		if ( pi.fakeplayer )
		{
			bot = true;
		}

		if (bot)
		{
			data->SetString("properties", "CPU Player");
		}
		else if (muted && friends)
		{
			data->SetString("properties", "Friend; Muted");
		}
		else if (muted)
		{
			data->SetString("properties", "Muted");
		}
		else if (friends)
		{
			data->SetString("properties", "Friend");
		}
		else
		{
			data->SetString("properties", "");
		}
	}
	m_pPlayerList->RereadAllItems();
}

//-----------------------------------------------------------------------------
// Purpose: Handles the AddFriend command
//-----------------------------------------------------------------------------
void CPlayerListDialog::OnCommand(const char *command)
{
	if (!stricmp(command, "Mute"))
	{
		ToggleMuteStateOfSelectedUser();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

//-----------------------------------------------------------------------------
// Purpose: toggles whether a user is muted or not
//-----------------------------------------------------------------------------
void CPlayerListDialog::ToggleMuteStateOfSelectedUser()
{
	if (!GameClientExports())
		return;

	for ( int iSelectedItem = 0; iSelectedItem < m_pPlayerList->GetSelectedItemsCount(); iSelectedItem++ )
	{
		KeyValues *data = m_pPlayerList->GetItem( m_pPlayerList->GetSelectedItem( iSelectedItem ) );
		if (!data)
			return;
		int playerIndex = data->GetInt("index");
		Assert(playerIndex);

		if (GameClientExports()->IsPlayerGameVoiceMuted(playerIndex))
		{
			GameClientExports()->UnmutePlayerGameVoice(playerIndex);
		}
		else
		{
			GameClientExports()->MutePlayerGameVoice(playerIndex);
		}
	}

	RefreshPlayerProperties();
	OnItemSelected();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerListDialog::OnItemSelected()
{
	// make sure the data is up-to-date
	RefreshPlayerProperties();

	// set the button state based on the selected item
	bool bMuteButtonEnabled = false;
	if (m_pPlayerList->GetSelectedItemsCount() > 0)
	{
		KeyValues *data = m_pPlayerList->GetItem(m_pPlayerList->GetSelectedItem(0));

		player_info_t pi;

		int iLocalPlayer = engine->GetLocalPlayer();

		int iPlayerIndex = data->GetInt("index");		
		bool isValidPlayer = engine->GetPlayerInfo( iPlayerIndex, &pi );

		// make sure the player is not a bot, or the user 
		// Matt - changed this check to see if player indeces match, instead of using friends ID
		if ( pi.fakeplayer || iPlayerIndex == iLocalPlayer ) // || pi.friendsID == g_pFriendsUser->GetFriendsID() )
		{
			// invalid player, 
			isValidPlayer = false;
		}

		if (data && isValidPlayer && GameClientExports() && GameClientExports()->IsPlayerGameVoiceMuted(data->GetInt("index")))
		{
			m_pMuteButton->SetText("#GameUI_UnmuteIngameVoice");
		}
		else
		{
			m_pMuteButton->SetText("#GameUI_MuteIngameVoice");
		}

		if (GameClientExports() && isValidPlayer)
		{
			bMuteButtonEnabled = true;
		}
	}
	else
	{
		m_pMuteButton->SetText("#GameUI_MuteIngameVoice");
	}

	m_pMuteButton->SetEnabled( bMuteButtonEnabled );
}
