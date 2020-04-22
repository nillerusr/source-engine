//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "PlayerContextMenu.h"

#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <KeyValues.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CPlayerContextMenu::CPlayerContextMenu(Panel *parent) : Menu(parent, "ServerContextMenu")
{
	CPlayerContextMenu::parent=parent;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CPlayerContextMenu::~CPlayerContextMenu()
{
}

//-----------------------------------------------------------------------------
// Purpose: Activates the menu
//-----------------------------------------------------------------------------
void CPlayerContextMenu::ShowMenu(Panel *target, unsigned int playerID)
{
	DeleteAllItems();
		
	AddMenuItem("Kick", "#Player_Menu_Kick", new KeyValues("Kick", "playerID", playerID), CPlayerContextMenu::parent);
	AddMenuItem("Ban", "#Player_Menu_Ban", new KeyValues("Ban", "playerID", playerID), CPlayerContextMenu::parent);
	//addMenuItem("Status", "&Player Status", new KeyValues("Status", "playerID", playerID), CPlayerContextMenu::parent);

	MakePopup();
	int x, y, gx, gy;
	input()->GetCursorPos(x, y);
	ipanel()->GetPos(surface()->GetEmbeddedPanel(), gx, gy);
	SetPos(x - gx, y - gy);
	MoveToFront();
	RequestFocus();
	SetVisible(true);
}
