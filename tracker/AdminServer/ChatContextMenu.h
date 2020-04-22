//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef CHATCONTEXTMENU_H
#define CHATCONTEXTMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <VGUI_Menu.h>

//-----------------------------------------------------------------------------
// Purpose: Basic right-click context menu for servers
//-----------------------------------------------------------------------------
class CChatContextMenu : public vgui::Menu
{
public:
	CChatContextMenu(vgui::Panel *parent);
	~CChatContextMenu();

	// call this to activate the menu
	void ShowMenu(vgui::Panel *target);
private:
	vgui::Panel *parent; // so we can send it messages
};


#endif // CHATCONTEXTMENU_H
