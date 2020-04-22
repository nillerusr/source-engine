//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VARLISTPROPERTYPAGE_H
#define VARLISTPROPERTYPAGE_H
#ifdef _WIN32
#pragma once
#endif

#include <KeyValues.h>
#include <vgui_controls/PropertyPage.h>

#include "utlvector.h"
#include "RemoteServer.h"

//-----------------------------------------------------------------------------
// Purpose: Property page for use in the server admin tool
//			holds a list of variables and manages the editing of them
//-----------------------------------------------------------------------------
class CVarListPropertyPage : public vgui::PropertyPage, public IServerDataResponse
{
	DECLARE_CLASS_SIMPLE( CVarListPropertyPage, vgui::PropertyPage );
public:
	CVarListPropertyPage(vgui::Panel *parent, const char *name);
	~CVarListPropertyPage();

	// loads the var list description from the specified file
	bool LoadVarList(const char *varfile);

	// sets the value of a variable
	void SetVarString(const char *varName, const char *value);

	// gets back the value of a variable
	const char *GetVarString(const char *varName);

	// refreshes all the values of cvars in the list
	virtual void RefreshVarList();

	// sets a custom string list for a combobox edit
	// stringList is a newline-delimited set of strings
	void SetCustomStringList(const char *varName, const char *stringList);

protected:
	// override to change how a variable is edited
	virtual void OnEditVariable(KeyValues *rule);
	// handles responses from the server
	virtual void OnServerDataResponse(const char *value, const char *response);

	// vgui overrides
	virtual void PerformLayout();
	virtual void OnKeyCodeTyped(vgui::KeyCode code);
	MESSAGE_FUNC_CHARPTR( OnVarChanged, "VarChanged", var );

private:
	MESSAGE_FUNC( EditVariable, "EditVariable" );
	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );

	vgui::ListPanel *m_pRulesList;
	vgui::Button *m_pEditButton;
};


#endif // VARLISTPROPERTYPAGE_H
