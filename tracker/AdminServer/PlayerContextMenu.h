//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef PLAYERCONTEXTMENU_H
#define PLAYERCONTEXTMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Menu.h>

//-----------------------------------------------------------------------------
// Purpose: Basic right-click context menu for servers
//-----------------------------------------------------------------------------
class CPlayerContextMenu : public vgui::Menu
{
public:
	CPlayerContextMenu(vgui::Panel *parent);
	~CPlayerContextMenu();

	// call this to activate the menu
	void ShowMenu(vgui::Panel *target, unsigned int serverID);
private:
	vgui::Panel *parent; // so we can send it messages
};


#endif // PLAYERCONTEXTMENU_H
