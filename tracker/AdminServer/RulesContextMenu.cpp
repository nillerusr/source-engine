//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "RulesContextMenu.h"

#include <VGUI_Controls.h>
#include <VGUI_IInput.h>
#include <VGUI_IPanel.h>
#include <VGUI_ISurface.h>
#include <VGUI_KeyValues.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CRulesContextMenu::CRulesContextMenu(Panel *parent) : Menu(parent, "RulesContextMenu")
{
	CRulesContextMenu::parent=parent;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CRulesContextMenu::~CRulesContextMenu()
{
}

//-----------------------------------------------------------------------------
// Purpose: Activates the menu
//-----------------------------------------------------------------------------
void CRulesContextMenu::ShowMenu(Panel *target, unsigned int cvarID)
{
	ClearMenu();
		
	AddMenuItem("cvar", "&Change Value", new KeyValues("cvar", "cvarID", cvarID), CRulesContextMenu::parent);


	MakePopup();
	int x, y, gx, gy;
	input()->GetCursorPos(x, y);
	ipanel()->GetPos(surface()->GetEmbeddedPanel(), gx, gy);
	SetPos(x - gx, y - gy);
	MoveToFront();
	RequestFocus();
	SetVisible(true);
}
