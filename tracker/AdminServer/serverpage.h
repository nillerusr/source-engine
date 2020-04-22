//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================
#if !defined( VINTERNETDLG_H )
#define VINTERNETDLG_H
#ifdef _WIN32
#pragma once
#endif

#include <VGUI_Frame.h>
#include <VGUI_ListPanel.h>
#include <VGUI_PHandle.h>

#include "utlvector.h"
#include "netadr.h"
#include "Server.h"
//#include "serversession.h"
//#include "trackerdoc.h"

//#include "ITrackerUser.h"
#include "../TrackerNET/TrackerNET_Interface.h"
#include "../TrackerNET/NetAddress.h" // for CNetAddress
#include "../TrackerNET/BinaryBuffer.h" // for IBinaryBuffer

#include "IGameList.h"

//#include "ClickableTabbedPanel.h"

class CServerContextMenu;
namespace vgui
{
class Label;
class Font;
class ListPanel;
class Button;
class ComboBox;
class QueryBox;
class ToggleButton;
class TextEntry;
class CheckButton;
class PropertySheet;
}

extern class IRunGameEngine *g_pRunGameEngine;
extern void v_strncpy(char *dest, const char *src, int bufsize);

class CFavoriteGames;
class CGamePanelInfo;
class CDialogGameInfo;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class VInternetDlg : public vgui::Frame
{

public:
	// Construction/destruction
						VInternetDlg( unsigned int userid );
	virtual				~VInternetDlg( void );

	virtual void		Initialize( void );

	// displays the dialog, moves it into focus, updates if it has to
	virtual void		Open( void );

	// gets server info
	serveritem_t &VInternetDlg::GetServer(unsigned int serverID);

	// setup
	virtual void		PerformLayout();

	// updates status text at bottom of window
	virtual void		UpdateStatusText(PRINTF_FORMAT_STRING const char *format, ...);

	// context menu access
	virtual CServerContextMenu *GetContextMenu();		

	// returns a pointer to a static instance of this dialog
	// valid for use only in sort functions
	static VInternetDlg *GetInstance();

	// begins the process of joining a server from a game list
	// the game info dialog it opens will also update the game list
	virtual CDialogGameInfo *JoinGame(IGameList *gameList, unsigned int serverIndex);

	// joins a game by a specified IP, not attached to any game list
	virtual CDialogGameInfo *JoinGame(int serverIP, int serverPort, const char *titleName);

	// opens a game info dialog from a game list
	virtual CDialogGameInfo *OpenGameInfoDialog(IGameList *gameList, unsigned int serverIndex);

	// opens a game info dialog by a specified IP, not attached to any game list
	virtual CDialogGameInfo *OpenGameInfoDialog(int serverIP, int serverPort, const char *titleName);

	virtual vgui::PropertySheet *GetTabPanel(); 

	// called by the config panel to setup some global config values
	virtual void SetConfig(bool autorefresh,bool savercon,int refreshtime,bool graphs,int graphrefreshtime,bool getlogs);

	// passes the server info to the favorites panel to update the rconPassword
	virtual void UpdateServer(serveritem_t &server);

	// opens up the config dialog with the current config settings
	virtual void ConfigPanel();

	virtual void OnTick();

	void SearchForFriend(unsigned int uid, const char *email, const char *username, const char *firstname, const char *lastname);
	ISendMessage *VInternetDlg::CreateServerMessage(int msgID);
	CNetAddress GetServerAddress();
	void SendInitialLogin();
	bool CheckMessageValidity(IReceiveMessage *dataBlock);



private:

	// menu handler for tabs
	void OnOpenContextMenu();
	// current game list change
	//virtual void		OnGameListChanged();
	

	// password entry dialog for new servers
	void OnPlayerDialog(vgui::KeyValues *data);
	void OnDeleteServer(int chosenPanel);

	// load/saves filter settings from disk
	virtual void		LoadFilters();
	virtual void		SaveFilters();

	// Load/saves position and window size of a dialog from disk
	virtual void		LoadDialogState(vgui::Panel *dialog, const char *dialogName);
	virtual void		SaveDialogState(vgui::Panel *dialog, const char *dialogName);

	// called when dialog is shut down
	virtual void		OnClose();

	// catches the "manage server" menu option
	void OnManageServer(int serverID);

	// actually creates the new tab
	void ManageServer(int serverID,const char *pass);



	// pointer to current game list
	IGameList *m_pGameList;

	// Status text
	vgui::Label	*m_pStatusLabel;

	// property sheet
	vgui::PropertySheet *m_pTabPanel;
	CFavoriteGames *m_pFavoriteGames;
	CGamePanelInfo *m_pGamePanelInfo;

	vgui::KeyValues *m_pSavedData;

	bool m_bAutoRefresh;
	bool m_bSaveRcon;
	int m_iRefreshTime;
	bool m_bGraphs;
	int m_iGraphsRefreshTime;
	bool m_bDoLogging; // whether to get server log messages

	ITrackerNET *m_pNet;
	unsigned int m_iSessionID; // the session ID from tracker
	unsigned int m_iUserID;
	int m_iRemoteUID;
	bool m_bLoggedIn;
	CNetAddress m_iServerAddr;

	// context menu
	CServerContextMenu *m_pContextMenu;

	typedef vgui::Frame BaseClass;

	DECLARE_PANELMAP();
};

#endif // VINTERNETDLG_H