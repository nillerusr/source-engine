//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef RULESCONTEXTMENU_H
#define RULESCONTEXTMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <VGUI_Menu.h>

//-----------------------------------------------------------------------------
// Purpose: Basic right-click context menu for servers
//-----------------------------------------------------------------------------
class CRulesContextMenu : public vgui::Menu
{
public:
	CRulesContextMenu(vgui::Panel *parent);
	~CRulesContextMenu();

	// call this to activate the menu
	void ShowMenu(vgui::Panel *target, unsigned int cvarID);
private:
	vgui::Panel *parent; // so we can send it messages
};


#endif // RULESCONTEXTMENU_H
