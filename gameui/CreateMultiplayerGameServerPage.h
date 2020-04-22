//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CREATEMULTIPLAYERGAMESERVERPAGE_H
#define CREATEMULTIPLAYERGAMESERVERPAGE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>
#include "CvarToggleCheckButton.h"

//-----------------------------------------------------------------------------
// Purpose: server options page of the create game server dialog
//-----------------------------------------------------------------------------
class CCreateMultiplayerGameServerPage : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( CCreateMultiplayerGameServerPage, vgui::PropertyPage );

public:
	CCreateMultiplayerGameServerPage(vgui::Panel *parent, const char *name);
	~CCreateMultiplayerGameServerPage();

	virtual void OnKeyCodePressed( vgui::KeyCode code );

	// returns currently entered information about the server
	void SetMap(const char *name);
	bool IsRandomMapSelected();
	const char *GetMapName();

	vgui::ComboBox *GetMapList( void ) { return m_pMapList; }

	// CS Bots
	void EnableBots( KeyValues *data );
	int GetBotQuota( void );
	bool GetBotsEnabled( void );

protected:
	virtual void OnApplyChanges();
	MESSAGE_FUNC( OnCheckButtonChecked, "CheckButtonChecked" );

private:
	void LoadMapList();
	void LoadMaps( const char *pszPathID );

	vgui::ComboBox *m_pMapList;
	vgui::CheckButton *m_pEnableBotsCheck;
	CCvarToggleCheckButton *m_pEnableTutorCheck;
	KeyValues *m_pSavedData;

	enum { DATA_STR_LENGTH = 64 };
	char m_szMapName[DATA_STR_LENGTH];
};


#endif // CREATEMULTIPLAYERGAMESERVERPAGE_H
