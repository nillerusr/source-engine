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

#include <VGUI_Menu.h>
#include "serverpage.h"

//-----------------------------------------------------------------------------
// Purpose: Basic right-click context menu for servers
//-----------------------------------------------------------------------------
class CServerContextMenu : public vgui::Menu
{
public:
	CServerContextMenu(CServerPage /*vgui::Panel*/ *parent);
	~CServerContextMenu();

	// call this to activate the menu
	void ShowMenu(vgui::Panel *target, unsigned int serverID, bool showConnect, bool showRefresh, bool showAddToFavorites,bool manage);
private:
	CServerPage /*vgui::Panel*/ *parent; // so we can send it messages
};


#endif // SERVERCONTEXTMENU_H
