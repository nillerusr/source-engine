//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SERVERCONTEXTMENU_H
#define SERVERCONTEXTMENU_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Basic right-click context menu for servers
//-----------------------------------------------------------------------------
class CServerContextMenu : public vgui::Menu
{
public:
	CServerContextMenu(vgui::Panel *parent);
	~CServerContextMenu();

	// call this to Activate the menu
	void ShowMenu(
		vgui::Panel *target, 
		unsigned int serverID, 
		bool showConnect, 
		bool showViewGameInfo,
		bool showRefresh, 
		bool showAddToFavorites);
};


#endif // SERVERCONTEXTMENU_H
