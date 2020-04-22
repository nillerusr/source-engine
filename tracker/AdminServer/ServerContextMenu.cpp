//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "ServerContextMenu.h"

#include <VGUI_Controls.h>
#include <VGUI_IInput.h>
#include <VGUI_IPanel.h>
#include <VGUI_ISurface.h>
#include <VGUI_KeyValues.h>
#include <VGUI_PropertySheet.h>
#include <stdio.h>


using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CServerContextMenu::CServerContextMenu(CServerPage *parent) : Menu(parent, "ServerContextMenu")
{
	CServerContextMenu::parent=parent;
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
void CServerContextMenu::ShowMenu(Panel *target, unsigned int serverID, bool showConnect, bool showRefresh, bool showAddToFavorites,bool manage)
{
	ClearMenu();


	// by default show the menu
	bool displayed=false;

	if(serverID==-1)
	{
		displayed=true; // no server selected, clicking on an empty area :)
	}

	if (showConnect)
	{
		displayed=true;
		AddMenuItem("ConnectToServer", "&Connect to server", new KeyValues("ConnectToServer", "serverID", serverID), target);
		AddMenuItem("ViewGameInfo", "&View server info", new KeyValues("ViewGameInfo", "serverID", serverID), target);
	}

	if (showRefresh)
	{
		displayed=true;
		AddMenuItem("RefreshServer", "&Refresh server", new KeyValues("RefreshServer", "serverID", serverID), target);
	}

	if (showAddToFavorites)
	{
		displayed=true;
		AddMenuItem("AddToFavorites", "&Add server to favorites", new KeyValues("AddToFavorites", "serverID", serverID), target);
	}
	
	if(manage)
	{
		displayed=true;
		AddMenuItem("Manage", "&Manage Server", new KeyValues("Manage", "serverID", serverID), CServerContextMenu::parent);
	}

/*	if (parent->IsCursorOver() ) // if the cursor is over the tabs
	{
		displayed=false;
		if((int) parent->GetTabPanel()->GetActivePageNum()!=0) // don't let the first tab be deleted, its our servers tab :) 
		{
			char name[100];
			displayed=true;

			strncpy(name,"&Delete ",100);
			parent->GetTabPanel()->GetActiveTabTitle(name+8,100-8);

			AddMenuItem("Delete", name,new KeyValues("DeleteServer", "panelid",(int) parent->GetTabPanel()->GetActivePageNum()), CServerContextMenu::parent);
		}
	} */


	if(displayed) 
	{
		MakePopup();
		int x, y, gx, gy;
		input()->GetCursorPos(x, y);
		ipanel()->GetPos(surface()->GetEmbeddedPanel(), gx, gy);
		SetPos(x - gx, y - gy);
		MoveToFront();
		RequestFocus();
		SetVisible(true);
	}
}
