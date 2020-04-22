//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "VarEditDialog.h"
#include "RemoteServer.h"

#include <stdio.h>

#include <vgui/IInput.h>

#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/TextEntry.h>
#include <KeyValues.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVarEditDialog::CVarEditDialog(vgui::Panel *parent, const char *name) : Frame(parent, name)
{
	SetSize(280, 180);
	SetSizeable(false);
	m_pOKButton = new Button(this, "OKButton", "OK");
	m_pCancelButton = new Button(this, "CancelButton", "Cancel");
	m_pStringEdit = new TextEntry(this, "StringEdit");
	m_pComboEdit = new ComboBox(this, "ComboEdit", 12, false);
	m_pRules = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CVarEditDialog::~CVarEditDialog()
{
//	input()->ReleaseAppModalSurface();
	if (m_pRules)
	{
		m_pRules->deleteThis();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Configures and shows the var edit dialog
//-----------------------------------------------------------------------------
void CVarEditDialog::Activate(vgui::Panel *actionSignalTarget, KeyValues *rules)
{
	// configure
	AddActionSignalTarget(actionSignalTarget);
	m_pRules = rules->MakeCopy();

	const char *type = m_pRules->GetString("type");
	if (!stricmp(type, "enumeration"))
	{
		LoadControlSettings("Admin/VarEditDialog_ComboBox.res", "PLATFORM");
		m_pStringEdit->SetVisible(false);

		// fill in the combo box
		for (KeyValues *kv = m_pRules->FindKey("list", true)->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
		{
			Assert( 0 );
			// FIXME: This Assert doesn't compile
//			Assert(index++ == atoi(kv->GetName()));
			m_pComboEdit->AddItem(kv->GetString(), NULL);
		}

		// activate the current item
		m_pComboEdit->ActivateItemByRow(m_pRules->GetInt("enum"));
	}
	else if (!stricmp(type, "customlist"))
	{
		LoadControlSettings("Admin/VarEditDialog_ComboBox.res", "PLATFORM");
		m_pStringEdit->SetVisible(false);

		// fill in the combo box
		int index = 0;
		const char *currentValue = m_pRules->GetString("value");
		const char *parse = m_pRules->GetString("stringlist");
		while (*parse)
		{
			// newline-seperated map list
			if (*parse == '\n')
			{
				parse++;
				continue;
			}

			// pull out the map name
			const char *end = strstr(parse, "\n");
			if (!end)
				break;

			char customString[64];
			int nameSize = end - parse;
			if (nameSize >= sizeof(customString))
			{
				nameSize = sizeof(customString) - 1;
			}

			// copy in the name
			strncpy(customString, parse, nameSize);
			customString[nameSize] = 0;
			parse = end;

			// add to dropdown
			int itemID = m_pComboEdit->AddItem(customString, NULL);
			index++;

			// activate the current item
			if (!stricmp(customString, currentValue))
			{
				m_pComboEdit->ActivateItem(itemID);
			}
		}
	}
	else
	{
		// normal string edit
		LoadControlSettings("Admin/VarEditDialog_String.res", "PLATFORM");
		m_pComboEdit->SetVisible(false);
		m_pStringEdit->SelectAllOnFirstFocus(true);
		m_pStringEdit->SetText(m_pRules->GetString("value"));
	}

	// set value
	char title[256];
	_snprintf(title, sizeof(title) - 1, "Change %s", m_pRules->GetString("name"));
	SetTitle(title, false);

	// bring to front	
//	input()->SetAppModalSurface(GetVPanel());
	MoveToCenterOfScreen();
	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: button command handler
//-----------------------------------------------------------------------------
void CVarEditDialog::OnCommand(const char *command)
{
	if (!stricmp(command, "OK"))
	{
		// change the value
		ApplyChanges();
		Close();
	}
	else if (!stricmp(command, "Cancel"))
	{
		Close();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Applies changes
//-----------------------------------------------------------------------------
void CVarEditDialog::ApplyChanges()
{
	const char *type = m_pRules->GetString("type");
	if (!stricmp(type, "enumeration"))
	{
		// get the enumeration position from the combo box
		int iVal = m_pComboEdit->GetActiveItem();
		char value[32];
		_snprintf(value, sizeof(value) - 1, "%d", iVal);
		RemoteServer().SetValue(m_pRules->GetName(), value);
	
	}
	else if (!stricmp(type, "customlist"))
	{
		char value[512];
		m_pComboEdit->GetText(value, sizeof(value));
		RemoteServer().SetValue(m_pRules->GetName(), value);
	}
	else
	{
		// normal string
		char value[512];
		m_pStringEdit->GetText(value, sizeof(value));
		RemoteServer().SetValue(m_pRules->GetName(), value);
	}

	// tell the caller the var changed
	PostActionSignal(new KeyValues("VarChanged", "var", m_pRules->GetName()));
}

//-----------------------------------------------------------------------------
// Purpose: Deletes on close
//-----------------------------------------------------------------------------
void CVarEditDialog::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}

