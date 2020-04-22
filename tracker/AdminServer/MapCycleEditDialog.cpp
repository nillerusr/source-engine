//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "MapCycleEditDialog.h"

#include <vgui/KeyCode.h>
#include <KeyValues.h>

#include <vgui_controls/Button.h>
#include <vgui_controls/ListPanel.h>

#include "RemoteServer.h"
#include "tier1/utlbuffer.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMapCycleEditDialog::CMapCycleEditDialog(vgui::Panel *parent, const char *name) : BaseClass(parent, name)
{
	SetSize(480, 320);
	SetSizeable(false);

	m_pAvailableMapList = new ListPanel(this, "AvailableMapList");
	m_pAvailableMapList->AddColumnHeader(0, "Map", "#Available_Maps", 128);
	m_pAvailableMapList->SetColumnSortable(0, false);

	m_pMapCycleList = new ListPanel(this, "MapCycleList");
	m_pMapCycleList->AddColumnHeader(0, "Map", "#Map_Cycle", 128);
	m_pMapCycleList->SetColumnSortable(0, false);

	m_RightArrow = new Button(this, "RightButton", "");
	m_LeftArrow = new Button(this, "LeftButton", "");
	m_UpArrow = new Button(this, "UpButton", "");
	m_DownArrow = new Button(this, "DownButton", "");

	LoadControlSettings("Admin/MapCycleEditDialog.res", "PLATFORM");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMapCycleEditDialog::~CMapCycleEditDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Shows the dialog, building the lists from the params
//-----------------------------------------------------------------------------
void CMapCycleEditDialog::Activate(vgui::Panel *updateTarget, CUtlVector<CUtlSymbol> &availableMaps, CUtlVector<CUtlSymbol> &mapCycle)
{
	// set the action signal target
	AddActionSignalTarget(updateTarget);

	// clear lists
	m_pAvailableMapList->DeleteAllItems();
	m_pMapCycleList->DeleteAllItems();

	// build lists
	for (int i = 0; i < availableMaps.Count(); i++)
	{
		// only add to the available maps list if it's not in mapCycle
		bool inMapCycle = false;
		for (int j = 0; j < mapCycle.Count(); j++)
		{
			if (!stricmp(mapCycle[j].String(), availableMaps[i].String()))
			{
				inMapCycle = true;
				break;
			}
		}

		if (!inMapCycle)
		{
			m_pAvailableMapList->AddItem(new KeyValues("MapItem", "Map", availableMaps[i].String()), 0, false, false);
		}
	}
	for (int i = 0; i < mapCycle.Count(); i++)
	{
		m_pMapCycleList->AddItem(new KeyValues("MapItem", "Map", mapCycle[i].String()), 0, false, false);
	}

	// show window
	SetTitle("Change Map Cycle", false);
	MoveToCenterOfScreen();
	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Sets up button state
//-----------------------------------------------------------------------------
void CMapCycleEditDialog::PerformLayout()
{
	m_LeftArrow->SetEnabled(false);
	m_RightArrow->SetEnabled(false);
	m_UpArrow->SetEnabled(false);
	m_DownArrow->SetEnabled(false);

	if (m_pMapCycleList->GetSelectedItemsCount() > 0)
	{
		m_LeftArrow->SetEnabled(true);
		m_LeftArrow->SetAsDefaultButton(true);

		if (m_pMapCycleList->GetSelectedItemsCount() == 1)
		{
			int row = m_pMapCycleList->GetSelectedItem(0);
			if (row > 0)
			{
				m_UpArrow->SetEnabled(true);
			}
			if (row + 1 < m_pMapCycleList->GetItemCount())
			{
				m_DownArrow->SetEnabled(true);
			}
		}
	}
	else if (m_pAvailableMapList->GetSelectedItemsCount() > 0)
	{
		m_RightArrow->SetEnabled(true);
		m_RightArrow->SetAsDefaultButton(true);
	}

	BaseClass::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Updates UI based on which listpanel got selection
//-----------------------------------------------------------------------------
void CMapCycleEditDialog::OnItemSelected(vgui::Panel *panel)
{
	if (panel == m_pAvailableMapList && m_pAvailableMapList->GetSelectedItemsCount() > 0)
	{
		m_pMapCycleList->ClearSelectedItems();
	}
	else if (panel == m_pMapCycleList && m_pMapCycleList->GetSelectedItemsCount() > 0)
	{
		m_pAvailableMapList->ClearSelectedItems();
	}
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Button command handler
//-----------------------------------------------------------------------------
void CMapCycleEditDialog::OnCommand(const char *command)
{
	if (!stricmp(command, "ArrowLeft"))
	{
		// move map from mapcycle to available list
		while (m_pMapCycleList->GetSelectedItemsCount() > 0)
		{
			int itemID = m_pMapCycleList->GetSelectedItem(0);
			KeyValues *data = m_pMapCycleList->GetItem(itemID);
			if (!data)
				return;

			const char *map = data->GetString("Map");
			m_pAvailableMapList->AddItem(new KeyValues("MapItem", "Map", map), 0, true, false);
			m_pMapCycleList->RemoveItem(itemID);
		}
	}
	else if (!stricmp(command, "ArrowRight"))
	{
		// move map from available list to mapcycle 
		while (m_pAvailableMapList->GetSelectedItemsCount() > 0)
		{
			int itemID = m_pAvailableMapList->GetSelectedItem(0);
			KeyValues *data = m_pAvailableMapList->GetItem(itemID);
			if (!data)
				return;

			const char *map = data->GetString("Map");
			m_pMapCycleList->AddItem(new KeyValues("MapItem", "Map", map), 0, true, false);
			m_pAvailableMapList->RemoveItem(itemID);
		}
	}
	else if (!stricmp(command, "ArrowUp"))
	{
		int itemID = m_pMapCycleList->GetSelectedItem(0);
		int row = m_pMapCycleList->GetItemCurrentRow(itemID);
		int prevRow = row - 1;
		if (prevRow < 0)
			return;

		int prevItemID = m_pMapCycleList->GetItemIDFromRow(prevRow);

		// get the data
		KeyValues *d1 = m_pMapCycleList->GetItem(itemID);
		KeyValues *d2 = m_pMapCycleList->GetItem(prevItemID);

		// swap the strings
		CUtlSymbol tempString = d1->GetString("Map");
		d1->SetString("Map", d2->GetString("Map"));
		d2->SetString("Map", tempString.String());

		// update the list
		m_pMapCycleList->ApplyItemChanges(itemID);
		m_pMapCycleList->ApplyItemChanges(prevItemID);
		PostMessage(m_pMapCycleList, new KeyValues("KeyCodePressed", "code", KEY_UP));
	}
	else if (!stricmp(command, "ArrowDown"))
	{
		int itemID = m_pMapCycleList->GetSelectedItem(0);
		int row = m_pMapCycleList->GetItemCurrentRow(itemID);
		int nextRow = row + 1;
		if (nextRow + 1 > m_pMapCycleList->GetItemCount())
			return;

		int nextItemID = m_pMapCycleList->GetItemIDFromRow(nextRow);

		// get the data
		KeyValues *d1 = m_pMapCycleList->GetItem(itemID);
		KeyValues *d2 = m_pMapCycleList->GetItem(nextItemID);

		// swap the strings
		CUtlSymbol tempString = d1->GetString("Map");
		d1->SetString("Map", d2->GetString("Map"));
		d2->SetString("Map", tempString.String());

		// update the list
		m_pMapCycleList->ApplyItemChanges(itemID);
		m_pMapCycleList->ApplyItemChanges(nextItemID);
		PostMessage(m_pMapCycleList, new KeyValues("KeyCodePressed", "code", KEY_DOWN));
	}
	else if (!stricmp(command, "Cancel"))
	{
		Close();
	}
	else if (!stricmp(command, "OK"))
	{
		// write out the data
		CUtlBuffer msg(0, 1024, CUtlBuffer::TEXT_BUFFER);

		for (int i = 0; i < m_pMapCycleList->GetItemCount(); i++)
		{
			int itemID = m_pMapCycleList->GetItemIDFromRow(i);
			KeyValues *kv = m_pMapCycleList->GetItem(itemID);
			if ( kv )
			{
				msg.PutString(kv->GetString("Map"));
				msg.PutChar('\n');
			}
		}

		msg.PutChar(0);
		RemoteServer().SetValue("mapcycle", (const char *)msg.Base());

		// post message to tell varlist update
		PostActionSignal(new KeyValues("VarChanged", "var", "mapcycle"));

		// close
		Close();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}
