//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef BLACKLISTEDSERVERS_H
#define BLACKLISTEDSERVERS_H
#ifdef _WIN32
#pragma once
#endif

#include "ServerBrowser/blacklisted_server_manager.h"

//-----------------------------------------------------------------------------
// Purpose: Blacklisted servers list
//-----------------------------------------------------------------------------
class CBlacklistedServers : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( CBlacklistedServers, vgui::PropertyPage );

public:
	CBlacklistedServers(vgui::Panel *parent);
	~CBlacklistedServers();

	// blacklist list, loads/saves from file
	void LoadBlacklistedList();
	void SaveBlacklistedList();
	void AddServer(gameserveritem_t &server);

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	
	// passed from main server browser window instead of messages
	void OnConnectToGame();
	void OnDisconnectFromGame( void );

	blacklisted_server_t *GetBlacklistedServer( int iServerID );
	bool IsServerBlacklisted(gameserveritem_t &server); 

private:
	// context menu message handlers
	MESSAGE_FUNC( OnPageShow, "PageShow" );
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );
	MESSAGE_FUNC( OnAddServerByName, "AddServerByName" );
	MESSAGE_FUNC( OnRemoveFromBlacklist, "RemoveFromBlacklist" );
	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );

	void ClearServerList( void );
	void OnAddCurrentServer( void );
	void OnImportBlacklist( void );
	void OnCommand(const char *command);
	void UpdateBlacklistUI( blacklisted_server_t *blackServer );
	int  GetSelectedServerID( void );
	bool AddServersFromFile( const char *pszFilename, bool bResetTimes );

private:
	vgui::Button *m_pAddServer;
	vgui::Button *m_pAddCurrentServer;
	vgui::ListPanel *m_pGameList;
	vgui::DHANDLE< vgui::FileOpenDialog >	m_hImportDialog;

	CBlacklistedServerManager m_blackList;
	long m_blackListTimestamp;
};


#endif // BLACKLISTEDSERVERS_H
