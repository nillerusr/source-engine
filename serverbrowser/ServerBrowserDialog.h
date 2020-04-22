//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef SERVERBROWSERDIALOG_H
#define SERVERBROWSERDIALOG_H
#ifdef _WIN32
#pragma once
#endif

extern class IRunGameEngine *g_pRunGameEngine;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CServerBrowserDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CServerBrowserDialog, vgui::Frame ); 

public:
	// Construction/destruction
	CServerBrowserDialog( vgui::Panel *parent );
	~CServerBrowserDialog( void );

	void		Initialize( void );

	// displays the dialog, moves it into focus, updates if it has to
	void		Open( void );

	// gets server info
	gameserveritem_t *GetServer(unsigned int serverID);
	// called every frame
	virtual void OnTick();

	// updates status text at bottom of window
	void UpdateStatusText(PRINTF_FORMAT_STRING const char *format, ...);
	
	// updates status text at bottom of window
	void UpdateStatusText(wchar_t *unicode);

	// context menu access
	CServerContextMenu *GetContextMenu(vgui::Panel *pParent);		

	// returns a pointer to a static instance of this dialog
	// valid for use only in sort functions
	static CServerBrowserDialog *GetInstance();

	// Adds a server to the list of favorites
	void AddServerToFavorites(gameserveritem_t &server);
	// Adds a server to our list of blacklisted servers
	void AddServerToBlacklist(gameserveritem_t &server);
	bool IsServerBlacklisted(gameserveritem_t &server); 

	// begins the process of joining a server from a game list
	// the game info dialog it opens will also update the game list
	CDialogGameInfo *JoinGame(IGameList *gameList, unsigned int serverIndex);

	// joins a game by a specified IP, not attached to any game list
	CDialogGameInfo *JoinGame(int serverIP, int serverPort, const char *pszConnectCode);

	// opens a game info dialog from a game list
	CDialogGameInfo *OpenGameInfoDialog(IGameList *gameList, unsigned int serverIndex);

	// opens a game info dialog by a specified IP, not attached to any game list
	CDialogGameInfo *OpenGameInfoDialog( int serverIP, uint16 connPort, uint16 queryPort, const char *pszConnectCode );

	// closes all the game info dialogs
	void CloseAllGameInfoDialogs();
	CDialogGameInfo *GetDialogGameInfoForFriend( uint64 ulSteamIDFriend );

	// accessor to the filter save data
	KeyValues *GetFilterSaveData(const char *filterSet);

	// gets the name of the mod directory we're restricted to accessing, NULL if none
	const char *GetActiveModName();
	CGameID &GetActiveAppID();
	const char *GetActiveGameName();

	// load/saves filter & favorites settings from disk
	void		LoadUserData();
	void		SaveUserData();

	// forces the currently active page to refresh
	void		RefreshCurrentPage();

	virtual gameserveritem_t *GetCurrentConnectedServer()
	{
		return &m_CurrentConnection;
	}

	void		BlacklistsChanged();
	CBlacklistedServers *GetBlacklistPage( void ) { return m_pBlacklist; }

private:

	// current game list change
	MESSAGE_FUNC( OnGameListChanged, "PageChanged" );
	void ReloadFilterSettings();

	// receives a specified game is active, so no other game types can be displayed in server list
	MESSAGE_FUNC_PARAMS( OnActiveGameName, "ActiveGameName", name );

	// notification that we connected / disconnected
	MESSAGE_FUNC_PARAMS( OnConnectToGame, "ConnectedToGame", kv );
	MESSAGE_FUNC( OnDisconnectFromGame, "DisconnectedFromGame" );
	MESSAGE_FUNC( OnLoadingStarted, "LoadingStarted" );

	virtual bool GetDefaultScreenPosition(int &x, int &y, int &wide, int &tall);
	virtual void ActivateBuildMode();

	void OnKeyCodePressed( vgui::KeyCode code );

private:
	// list of all open game info dialogs
	CUtlVector<vgui::DHANDLE<CDialogGameInfo> > m_GameInfoDialogs;

	// pointer to current game list
	IGameList *m_pGameList;

	// Status text
	vgui::Label	*m_pStatusLabel;

	// property sheet
	vgui::PropertySheet *m_pTabPanel;
	CFavoriteGames *m_pFavorites;
	CBlacklistedServers *m_pBlacklist;
	CHistoryGames *m_pHistory;
	CInternetGames *m_pInternetGames;
	CSpectateGames *m_pSpectateGames;
	CLanGames *m_pLanGames;
	CFriendsGames *m_pFriendsGames;

	KeyValues *m_pSavedData;
	KeyValues *m_pFilterData;

	// context menu
	CServerContextMenu *m_pContextMenu;

	// active game
	char m_szGameName[128];
	char m_szModDir[128];
	CGameID m_iLimitAppID;

	// currently connected game
	bool m_bCurrentlyConnected;
	gameserveritem_t m_CurrentConnection;
};

// singleton accessor
extern CServerBrowserDialog &ServerBrowserDialog();

// Used by the LAN tab and the add server dialog when trying to find servers without having
// been given any ports to look for servers on.
void GetMostCommonQueryPorts( CUtlVector<uint16> &ports );

#endif // SERVERBROWSERDIALOG_H
