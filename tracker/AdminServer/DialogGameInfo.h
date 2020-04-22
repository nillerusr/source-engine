//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef DIALOGGAMEINFO_H
#define DIALOGGAMEINFO_H
#ifdef _WIN32
#pragma once
#endif

#include <VGUI_Frame.h>
#include "ServerList.h"
#include "IServerRefreshResponse.h"

class KeyValues;

namespace vgui
{
class Button;
class ToggleButton;
class RadioButton;
class Label;
class TextEntry;
}

//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying information about a game server
//-----------------------------------------------------------------------------
class CDialogGameInfo : public vgui::Frame, public IServerRefreshResponse
{
public:
	CDialogGameInfo(class IGameList *gameList, unsigned int serverID, int serverIP = 0, int serverPort = 0);
	~CDialogGameInfo();

	void Run(const char *titleName);
	void ChangeGame(int serverIP, int serverPort);

	// forces the dialog to attempt to connect to the server
	void Connect();

	// implementation of IServerRefreshResponse interface
	// called when the server has successfully responded
	virtual void ServerResponded(serveritem_t &server);

	// called when a server response has timed out
	virtual void ServerFailedToRespond(serveritem_t &server);

	// called when the current refresh list is complete
	virtual void RefreshComplete();

protected:
	// message handlers
	void OnConnect();
	void OnRefresh();
	void OnButtonToggled(vgui::Panel *toggleButton);

	// response from the get password dialog
	void OnJoinServerWithPassword(const char *password);

	DECLARE_PANELMAP();

	// vgui overrides
	void OnTick();
	virtual void OnClose();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	long m_iRequestRetry;	// time at which to retry the request

	// methods
	void RequestInfo();
	void ConnectToServer();
	void ShowAutoRetryOptions(bool state);

	void SetControlText(const char *textEntryName, const char *text);

	vgui::Button *m_pConnectButton;
	vgui::Button *m_pCloseButton;
	vgui::Button *m_pRefreshButton;
	vgui::Label *m_pInfoLabel;
	vgui::ToggleButton *m_pAutoRetry;
	vgui::RadioButton *m_pAutoRetryAlert;
	vgui::RadioButton *m_pAutoRetryJoin;

	// the ID of the server we're watching
	unsigned int m_iServerID;

	// server refreshing object
	CServerList	m_Servers;

	// true if we should try connect to the server when it refreshes
	bool m_bConnecting;

	// password, if entered
	char m_szPassword[64];

	// state
	bool m_bServerNotResponding;
	bool m_bServerFull;
	bool m_bShowAutoRetryToggle;
	bool m_bShowingExtendedOptions;

	typedef vgui::Frame BaseClass;
};

#endif // DIALOGGAMEINFO_H
