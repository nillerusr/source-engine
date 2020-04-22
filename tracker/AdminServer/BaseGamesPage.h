//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef BASEGAMESPAGE_H
#define BASEGAMESPAGE_H
#ifdef _WIN32
#pragma once
#endif

#include <VGUI_Frame.h>
#include "ServerList.h"
#include "IServerRefreshResponse.h"
#include "server.h"
#include "IGameList.h"

namespace vgui
{
class ListPanel;
class ImagePanel;
class ComboBox;
class ToggleButton;
class CheckButton;
class TextEntry;
class MenuButton;
class Menu;
};

//-----------------------------------------------------------------------------
// Purpose: Base property page for all the games lists (internet/favorites/lan/etc.)
//-----------------------------------------------------------------------------
class CBaseGamesPage : public vgui2::Frame, public IServerRefreshResponse, public IGameList
{
public:
	CBaseGamesPage(vgui::Panel *parent, const char *name);
	~CBaseGamesPage();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	// gets information about specified server
	virtual serveritem_t &GetServer(unsigned int serverID);

	virtual void SetRefreshing(bool state);

protected:
	virtual void OnTick();
	virtual void OnCommand(const char *command);
//	virtual void OnOnMouseDoublePressed(enum vgui::MouseCode code); 

	// an inner class
	class OurListPanel: public vgui::ListPanel
	{
	public:
		OurListPanel(vgui::Panel *parent, const char *panelName): vgui::ListPanel(parent,panelName) { m_pParent=parent;};
		
		virtual void	OnMouseDoublePressed( vgui::MouseCode code );
	private:
		vgui::Panel *m_pParent;

	};

	// a list panel we grab the double click event from :)
	OurListPanel *m_pGameList;
	CServerList	m_Servers;
	vgui::ImagePanel *m_pPasswordIcon; // password icon

private:
	void OnButtonToggled(vgui::Panel *panel, int state);
	void OnTextChanged(vgui::Panel *panel, const char *text);
	void OnManage();

	// command buttons
	vgui::Button *m_pConnect;
	vgui::Button *m_pRefresh;
	vgui::Button *m_pManage;
	vgui::Button *m_pAddIP;

	vgui::Panel *m_pParent;

//	vgui::Menu *m_pRefreshMenu;

	typedef vgui::Frame BaseClass;

	DECLARE_PANELMAP();
};


#endif // BASEGAMESPAGE_H
