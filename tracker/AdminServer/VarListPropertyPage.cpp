//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "VarListPropertyPage.h"
#include "RemoteServer.h"
#include "VarEditDialog.h"

#include <KeyValues.h>
#include <vgui/KeyCode.h>

#include <vgui_controls/ListPanel.h>
#include <vgui_controls/Button.h>

#include "filesystem.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVarListPropertyPage::CVarListPropertyPage(vgui::Panel *parent, const char *name) : vgui::PropertyPage(parent, name)
{
	m_pRulesList = new ListPanel(this, "RulesList");
	m_pRulesList->AddColumnHeader(0, "name", "Variable", 256);
	m_pRulesList->AddColumnHeader(1, "value", "Value", 256);

	m_pEditButton = new Button(this, "EditButton", "Edit...");
	m_pEditButton->SetCommand(new KeyValues("EditVariable"));
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CVarListPropertyPage::~CVarListPropertyPage()
{
	// remove from callback list
	RemoteServer().RemoveServerDataResponseTarget(this);
}

//-----------------------------------------------------------------------------
// Purpose: loads a list of variables from the file
//-----------------------------------------------------------------------------
bool CVarListPropertyPage::LoadVarList(const char *varfile)
{
	bool bSuccess = false;
	// clear the current list
	m_pRulesList->DeleteAllItems();

	// load list from file
	KeyValues *dat = new KeyValues("VarList");
	if (dat->LoadFromFile( g_pFullFileSystem, varfile, NULL))
	{
		// enter into list
		for (KeyValues *rule = dat->GetFirstSubKey(); rule != NULL; rule = rule->GetNextKey())
		{
			m_pRulesList->AddItem(rule, 0, false, false);
		}
		bSuccess = true;
	}

	dat->deleteThis();
	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: configures layout
//-----------------------------------------------------------------------------
void CVarListPropertyPage::PerformLayout()
{
	BaseClass::PerformLayout();
	OnItemSelected();
}

//-----------------------------------------------------------------------------
// Purpose: Handles edit button press
//-----------------------------------------------------------------------------
void CVarListPropertyPage::EditVariable()
{
	// get rule from the list
	int itemID = m_pRulesList->GetSelectedItem(0);
	KeyValues *rule = m_pRulesList->GetItem(itemID);
	if (!rule)
		return;

	OnEditVariable(rule);
}

//-----------------------------------------------------------------------------
// Purpose: Edits var
//-----------------------------------------------------------------------------
void CVarListPropertyPage::OnEditVariable(KeyValues *rule)
{
	// open an edit box appropriate
	CVarEditDialog *box = new CVarEditDialog(this, "VarEditDialog");
	box->Activate(this, rule);
}

//-----------------------------------------------------------------------------
// Purpose: refreshes all the values of cvars in the list
//-----------------------------------------------------------------------------
void CVarListPropertyPage::RefreshVarList()
{
	// iterate the vars
	for (int i = 0; i < m_pRulesList->GetItemCount(); i++)
	{
		KeyValues *row = m_pRulesList->GetItem(i);
		if (!row)
			continue;

		// request the info from the server on each one of them
		RemoteServer().RequestValue(this, row->GetName());
	}

	RemoteServer().ProcessServerResponse();
}

//-----------------------------------------------------------------------------
// Purpose: sets the value of a variable in the list
//-----------------------------------------------------------------------------
void CVarListPropertyPage::SetVarString(const char *varName, const char *value)
{
	// find the item by name
	int itemID = m_pRulesList->GetItem(varName);
	KeyValues *rule = m_pRulesList->GetItem(itemID);
	if (!rule)
		return;

	// parse the rule
	const char *type = rule->GetString("type");
	if (!stricmp(type, "enumeration"))
	{
		// look up the value in the enumeration
		int iValue = atoi(value);
		const char *result = rule->FindKey("list", true)->GetString(value, "");
		rule->SetString("value", result);
		rule->SetInt("enum", iValue);
	}
	else 
	{
		// no special type, treat it as a string
		rule->SetString("value", value);
	}
		
	m_pRulesList->ApplyItemChanges(itemID);
}

//-----------------------------------------------------------------------------
// Purpose: Sets a custom string list
//-----------------------------------------------------------------------------
void CVarListPropertyPage::SetCustomStringList(const char *varName, const char *stringList)
{
	// find the item by name
	int itemID = m_pRulesList->GetItem(varName);
	KeyValues *rule = m_pRulesList->GetItem(itemID);
	if (!rule)
		return;

	rule->SetString("stringlist", stringList);
}

//-----------------------------------------------------------------------------
// Purpose: gets back the value of a variable
//-----------------------------------------------------------------------------
const char *CVarListPropertyPage::GetVarString(const char *varName)
{
	int itemID = m_pRulesList->GetItem(varName);
	KeyValues *rule = m_pRulesList->GetItem(itemID);
	if (!rule)
		return "";

	return rule->GetString("value");
}

//-----------------------------------------------------------------------------
// Purpose: Causes values to refresh on the user hitting F5
//-----------------------------------------------------------------------------
void CVarListPropertyPage::OnKeyCodeTyped(KeyCode code)
{
	if (code == KEY_F5)
	{
		OnResetData();
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Enables the edit button if any rows in the list are selected
//-----------------------------------------------------------------------------
void CVarListPropertyPage::OnItemSelected()
{
	if (m_pRulesList->GetSelectedItemsCount() > 0)
	{
		m_pEditButton->SetEnabled(true);
	}
	else
	{
		m_pEditButton->SetEnabled(false);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when a var gets edited
//-----------------------------------------------------------------------------
void CVarListPropertyPage::OnVarChanged(const char *var)
{
	// request new value from server
	RemoteServer().RequestValue(this, var);
	// process the queue immediately, since if we're running a local server there will already be a reply
	RemoteServer().ProcessServerResponse();
}

//-----------------------------------------------------------------------------
// Purpose: handles responses from the server
//-----------------------------------------------------------------------------
void CVarListPropertyPage::OnServerDataResponse(const char *value, const char *response)
{
	SetVarString(value, response);
}
