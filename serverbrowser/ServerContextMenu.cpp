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
CServerContextMenu::CServerContextMenu(Panel *parent) : Menu(parent, "ServerContextMenu")
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CServerContextMenu::~CServerContextMenu()
{
}

//-----------------------------------------------------------------------------
// Purpose: Activates the menu
//-----------------------------------------------------------------------------
void CServerContextMenu::ShowMenu(
	Panel *target, 
	unsigned int serverID, 
	bool showConnect, 
	bool showViewGameInfo,
	bool showRefresh, 
	bool showAddToFavorites )
{
	if (showConnect)
	{
		AddMenuItem("ConnectToServer", "#ServerBrowser_ConnectToServer", new KeyValues("ConnectToServer", "serverID", serverID), target);
	}

	if (showViewGameInfo)
	{
		AddMenuItem("ViewGameInfo", "#ServerBrowser_ViewServerInfo", new KeyValues("ViewGameInfo", "serverID", serverID), target);
	}

	if (showRefresh)
	{
		AddMenuItem("RefreshServer", "#ServerBrowser_RefreshServer", new KeyValues("RefreshServer", "serverID", serverID), target);
	}

	if (showAddToFavorites)
	{
		AddMenuItem("AddToFavorites", "#ServerBrowser_AddServerToFavorites", new KeyValues("AddToFavorites", "serverID", serverID), target);
		AddMenuItem("AddToBlacklist", "#ServerBrowser_AddServerToBlacklist", new KeyValues("AddToBlacklist", "serverID", serverID), target);
	}

	int x, y, gx, gy;
	input()->GetCursorPos(x, y);
	ipanel()->GetPos(surface()->GetEmbeddedPanel(), gx, gy);
	SetPos(x - gx, y - gy);
	SetVisible(true);
}
