//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef PLAYERPANEL_H
#define PLAYERPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <KeyValues.h>

#include <vgui_controls/Frame.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/PropertyPage.h>

#include "PlayerContextMenu.h"
#include "RemoteServer.h"

//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying information about a game server
//-----------------------------------------------------------------------------
class CPlayerPanel : public vgui::PropertyPage, public IServerDataResponse
{
	DECLARE_CLASS_SIMPLE( CPlayerPanel, vgui::PropertyPage );
public:
	CPlayerPanel(vgui::Panel *parent, const char *name);
	~CPlayerPanel();

	// returns the keyvalues for the currently selected row
	KeyValues *GetSelected(); 

protected:
	// property page handlers
	virtual void OnResetData();
	virtual void OnThink();
	virtual void OnKeyCodeTyped(vgui::KeyCode code);

	// called when the server has returned a requested value
	virtual void OnServerDataResponse(const char *value, const char *response);

private:
	// vgui overrides
	virtual void OnCommand(const char *command);

	// msg handlers
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );
	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );

	MESSAGE_FUNC( OnKickButtonPressed, "KickPlayer" );
	MESSAGE_FUNC( OnBanButtonPressed, "BanPlayer" );
	MESSAGE_FUNC( KickSelectedPlayers, "KickSelectedPlayers" );
	MESSAGE_FUNC_CHARPTR_CHARPTR( AddBanByID, "AddBanValue", id, time );

	vgui::ListPanel *m_pPlayerListPanel;
	vgui::Button *m_pKickButton;
	vgui::Button *m_pBanButton;

	CPlayerContextMenu *m_pPlayerContextMenu;

	float m_flUpdateTime;
};

#endif // PLAYERPANEL_H