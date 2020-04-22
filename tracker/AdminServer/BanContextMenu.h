//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef BANCONTEXTMENU_H
#define BANCONTEXTMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Menu.h>

//-----------------------------------------------------------------------------
// Purpose: Basic right-click context menu for servers
//-----------------------------------------------------------------------------
class CBanContextMenu : public vgui::Menu
{
public:
	CBanContextMenu(vgui::Panel *parent);
	~CBanContextMenu();

	// call this to activate the menu
	void ShowMenu(vgui::Panel *target, unsigned int banID);
private:
	vgui::Panel *parent; // so we can send it messages
};


#endif // BANCONTEXTMENU_H
