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

#include "tier1/utldict.h"

class CBaseGamesPage;

//-----------------------------------------------------------------------------
// Purpose: Acts like a regular ListPanel but forwards enter key presses
// to its outer control.
//-----------------------------------------------------------------------------
class CGameListPanel : public vgui::ListPanel
{
public:
	DECLARE_CLASS_SIMPLE( CGameListPanel, vgui::ListPanel );
	
	CGameListPanel( CBaseGamesPage *pOuter, const char *pName );
	
	virtual void OnKeyCodePressed(vgui::KeyCode code);

private:
	CBaseGamesPage *m_pOuter;
};

class CQuickListMapServerList : public CUtlVector< int >
{
public:
	CQuickListMapServerList() : CUtlVector< int >( 1, 0 )
	{
	}

	CQuickListMapServerList( const CQuickListMapServerList& src )
	{
		CopyArray( src.Base(), src.Count() );
	}

	CQuickListMapServerList &operator=( const CQuickListMapServerList &src )
	{
		CopyArray( src.Base(), src.Count() );
		return *this;
	}
};


class CCheckBoxWithStatus : public vgui::CheckButton
{
public:
	DECLARE_CLASS_SIMPLE( CCheckBoxWithStatus, vgui::CheckButton );

	CCheckBoxWithStatus(Panel *parent, const char *panelName, const char *text) : vgui::CheckButton( parent, panelName, text )
	{
	}

	virtual void OnCursorEntered();
	virtual void OnCursorExited();
};

struct servermaps_t
{
	const char *pOriginalName;
	const char *pFriendlyName;
	int			iPanelIndex;
	bool		bOnDisk;
};

struct gametypes_t
{
	const char *pPrefix;
	const char *pGametypeName;
};

//-----------------------------------------------------------------------------
// Purpose: Base property page for all the games lists (internet/favorites/lan/etc.)
//-----------------------------------------------------------------------------
class CBaseGamesPage : public vgui::PropertyPage, public IGameList, public ISteamMatchmakingServerListResponse, public ISteamMatchmakingPingResponse
{
	DECLARE_CLASS_SIMPLE( CBaseGamesPage, vgui::PropertyPage );

public:
	enum EPageType
	{
		eInternetServer,
		eLANServer,
		eFriendsServer,
		eFavoritesServer,
		eHistoryServer,
		eSpectatorServer
	};

	// Column indices
	enum
	{
		k_nColumn_Password = 0,
		k_nColumn_Secure = 1,
		k_nColumn_Replay = 2,
		k_nColumn_Name = 3,
		k_nColumn_IPAddr = 4,
		k_nColumn_GameDesc = 5,
		k_nColumn_Players = 6,
		k_nColumn_Bots = 7,
		k_nColumn_Map = 8,
		k_nColumn_Ping = 9,
	};

	CBaseGamesPage( vgui::Panel *parent, const char *name, EPageType eType, const char *pCustomResFilename=NULL);
	~CBaseGamesPage();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	// gets information about specified server
	virtual gameserveritem_t *GetServer(unsigned int serverID);
	virtual const char *GetConnectCode();

	uint32 GetServerFilters( MatchMakingKeyValuePair_t **pFilters );

	virtual void SetRefreshing(bool state);

	// loads filter settings from disk
	virtual void LoadFilterSettings();

	// Called by CGameList when the enter key is pressed.
	// This is overridden in the add server dialog - since there is no Connect button, the message
	// never gets handled, but we want to add a server when they dbl-click or press enter.
	virtual bool OnGameListEnterPressed();
	
	int GetSelectedItemsCount();

	// adds a server to the favorites
	MESSAGE_FUNC( OnAddToFavorites, "AddToFavorites" );
	MESSAGE_FUNC( OnAddToBlacklist, "AddToBlacklist" );

	virtual void StartRefresh();

	virtual void UpdateDerivedLayouts( void );
	
	void		PrepareQuickListMap( const char *pMapName, int iListID );
	void		SelectQuickListServers( void );
	vgui::Panel *GetActiveList( void );
	virtual bool IsQuickListButtonChecked()
	{
		return m_pQuickListCheckButton ? m_pQuickListCheckButton->IsSelected() : false;
	}

	STEAM_CALLBACK( CBaseGamesPage, OnFavoritesMsg, FavoritesListChanged_t, m_CallbackFavoritesMsg );

	// applies games filters to current list
	void ApplyGameFilters();

	void OnLoadingStarted()
	{
		StopRefresh();
	}

protected:
	virtual void OnCommand(const char *command);
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual int GetRegionCodeToFilter() { return 255; }

	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );

	// updates server count UI
	void UpdateStatus();

	// ISteamMatchmakingServerListResponse callbacks
	virtual void ServerResponded( HServerListRequest hReq, int iServer );
	virtual void ServerResponded( int iServer, gameserveritem_t *pServerItem );
	virtual void ServerFailedToRespond( HServerListRequest hReq, int iServer );
	virtual void RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response ) = 0;

	// ISteamMatchmakingPingResponse callbacks
	virtual void ServerResponded( gameserveritem_t &server );
	virtual void ServerFailedToRespond() {}

	// Removes server from list
	void RemoveServer( serverdisplay_t &server );

	virtual bool BShowServer( serverdisplay_t &server ) { return server.m_bDoNotRefresh; } 
	void ClearServerList();

	// filtering methods
	// returns true if filters passed; false if failed
	virtual bool CheckPrimaryFilters( gameserveritem_t &server);
	virtual bool CheckSecondaryFilters( gameserveritem_t &server );
	virtual bool CheckTagFilter( gameserveritem_t &server ) { return true; }
	virtual bool CheckWorkshopFilter( gameserveritem_t &server ) { return true; }
	virtual int GetInvalidServerListID();

	virtual void OnSaveFilter(KeyValues *filter);
	virtual void OnLoadFilter(KeyValues *filter);
	virtual void UpdateFilterSettings();

	// whether filter settings limit which master server to query
	CGameID &GetFilterAppID() { return m_iLimitToAppID; }
	
	virtual void GetNewServerList();
	virtual void StopRefresh();
	virtual bool IsRefreshing();
	virtual void OnPageShow();
	virtual void OnPageHide();

	// called when Connect button is pressed
	MESSAGE_FUNC( OnBeginConnect, "ConnectToServer" );
	// called to look at game info
	MESSAGE_FUNC( OnViewGameInfo, "ViewGameInfo" );
	// refreshes a single server
	MESSAGE_FUNC_INT( OnRefreshServer, "RefreshServer", serverID );

	// If true, then we automatically select the first item that comes into the games list.
	bool m_bAutoSelectFirstItemInGameList;

	CGameListPanel *m_pGameList;
	vgui::PanelListPanel *m_pQuickList;

	vgui::ComboBox *m_pLocationFilter;

	// command buttons
	vgui::Button *m_pConnect;
	vgui::Button *m_pRefreshAll;
	vgui::Button *m_pRefreshQuick;
	vgui::Button *m_pAddServer;
	vgui::Button *m_pAddCurrentServer;
	vgui::Button *m_pAddToFavoritesButton;
	vgui::ToggleButton *m_pFilter;

	CUtlMap<uint64, int> m_mapGamesFilterItem;
	CUtlMap<int, serverdisplay_t> m_mapServers;
	CUtlMap<netadr_t, int> m_mapServerIP;
	CUtlVector<MatchMakingKeyValuePair_t> m_vecServerFilters;
	CUtlDict< CQuickListMapServerList, int > m_quicklistserverlist;
	int m_iServerRefreshCount;
	CUtlVector< servermaps_t > m_vecMapNamesFound;
	

	EPageType m_eMatchMakingType;
	HServerListRequest m_hRequest;

	int	GetSelectedServerID( KeyValues **pKV = NULL );

	void	ClearQuickList( void );

	bool	TagsExclude( void );

	enum eWorkshopMode {
		// These correspond to the dropdown indices
		eWorkshop_None           = 0,
		eWorkshop_WorkshopOnly   = 1,
		eWorkshop_SubscribedOnly = 2
	};
	eWorkshopMode WorkshopMode();

	void	HideReplayFilter( void );

protected:
	virtual void CreateFilters();
	virtual void UpdateGameFilter();

	MESSAGE_FUNC_PTR_CHARPTR( OnTextChanged, "TextChanged", panel, text );
	MESSAGE_FUNC_PTR_INT( OnButtonToggled, "ButtonToggled", panel, state );
	
	void UpdateFilterAndQuickListVisibility();
	bool BFiltersVisible() { return m_bFiltersVisible; }

private:
	void RequestServersResponse( int iServer, EMatchMakingServerResponse response, bool bLastServer ); // callback for matchmaking interface

	void RecalculateFilterString();

	void SetQuickListEnabled( bool bEnabled );
	void SetFiltersVisible( bool bVisible );

	// If set, it uses the specified resfile name instead of its default one.
	const char *m_pCustomResFilename;

	// filter controls
	vgui::ComboBox *m_pGameFilter;
	vgui::TextEntry *m_pMapFilter;
	vgui::TextEntry *m_pMaxPlayerFilter;
	vgui::ComboBox *m_pPingFilter;
	vgui::ComboBox *m_pSecureFilter;
	vgui::ComboBox *m_pTagsIncludeFilter;
	vgui::ComboBox *m_pWorkshopFilter;
	vgui::CheckButton *m_pNoFullServersFilterCheck;
	vgui::CheckButton *m_pNoEmptyServersFilterCheck;
	vgui::CheckButton *m_pNoPasswordFilterCheck;
	CCheckBoxWithStatus *m_pQuickListCheckButton;
	vgui::Label *m_pFilterString;
	char m_szComboAllText[64];
	vgui::CheckButton *m_pReplayFilterCheck;

	KeyValues *m_pFilters; // base filter data
	bool m_bFiltersVisible;	// true if filter section is currently visible
	vgui::HFont m_hFont;

	int m_nImageIndexPassword;
	int m_nImageIndexSecure;
	int m_nImageIndexSecureVacBanned;
	int m_nImageIndexReplay;

	// filter data
	char m_szGameFilter[32];
	char m_szMapFilter[32];
	int m_iMaxPlayerFilter;
	int	m_iPingFilter;
	bool m_bFilterNoFullServers;
	bool m_bFilterNoEmptyServers;
	bool m_bFilterNoPasswordedServers;
	int m_iSecureFilter;
	int m_iServersBlacklisted;
	bool m_bFilterReplayServers;

	CGameID m_iLimitToAppID;
};

#endif // BASEGAMESPAGE_H
