//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "BanContextMenu.h"

#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <KeyValues.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBanContextMenu::CBanContextMenu(Panel *parent) : Menu(parent, "BanContextMenu")
{
	CBanContextMenu::parent=parent;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBanContextMenu::~CBanContextMenu()
{
}

//-----------------------------------------------------------------------------
// Purpose: Activates the menu
//-----------------------------------------------------------------------------
void CBanContextMenu::ShowMenu(Panel *target, unsigned int banID)
{
	DeleteAllItems();

	if(banID==-1) 
	{
		AddMenuItem("ban", "#Ban_Menu_Add", new KeyValues("addban", "banID", banID), CBanContextMenu::parent);
	} 
	else
	{
		AddMenuItem("ban", "#Ban_Menu_Remove", new KeyValues("removeban", "banID", banID), CBanContextMenu::parent);
		AddMenuItem("ban", "#Ban_Menu_Change", new KeyValues("changeban", "banID", banID), CBanContextMenu::parent);
	}

	MakePopup();
	int x, y, gx, gy;
	input()->GetCursorPos(x, y);
	ipanel()->GetPos(surface()->GetEmbeddedPanel(), gx, gy);
	SetPos(x - gx, y - gy);
	MoveToFront();
	RequestFocus();
	SetVisible(true);
}
