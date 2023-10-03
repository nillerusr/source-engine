//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CREATEMULTIPLAYERGAMEGAMEPLAYPAGE_H
#define CREATEMULTIPLAYERGAMEGAMEPLAYPAGE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>

class CPanelListPanel;
class CDescription;
class mpcontrol_t;

//-----------------------------------------------------------------------------
// Purpose: server options page of the create game server dialog
//-----------------------------------------------------------------------------
class CCreateMultiplayerGameGameplayPage : public vgui::PropertyPage
{
public:
	CCreateMultiplayerGameGameplayPage(vgui::Panel *parent, const char *name);
	~CCreateMultiplayerGameGameplayPage();

	// returns currently entered information about the server
	int GetMaxPlayers();
	const char *GetPassword();
	const char *GetHostName();

protected:
	virtual void OnApplyChanges();

private:
	const char *GetValue(const char *cvarName, const char *defaultValue);
	void LoadGameOptionsList();
	void GatherCurrentValues();

	CDescription *m_pDescription;
	mpcontrol_t *m_pList;
	CPanelListPanel *m_pOptionsList;
};


#endif // CREATEMULTIPLAYERGAMEGAMEPLAYPAGE_H
