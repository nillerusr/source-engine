//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "pch_serverbrowser.h"

using namespace vgui;

ConVar sb_showblacklists( "sb_showblacklists", "0", FCVAR_NONE, "If set to 1, blacklist rules will be printed to the console as they're applied." );

//-----------------------------------------------------------------------------
// Purpose: Server name comparison function
//-----------------------------------------------------------------------------
int __cdecl BlacklistedServerNameCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
	blacklisted_server_t *pSvr1 = ServerBrowserDialog().GetBlacklistPage()->GetBlacklistedServer( p1.userData );
	blacklisted_server_t *pSvr2 = ServerBrowserDialog().GetBlacklistPage()->GetBlacklistedServer( p2.userData );

	if ( !pSvr1 && pSvr2 ) 
		return -1;
	if ( !pSvr2 && pSvr1 )
		return 1;
	if ( !pSvr1 && !pSvr2 )
		return 0;

	return Q_stricmp( pSvr1->m_szServerName, pSvr2->m_szServerName );
}

//-----------------------------------------------------------------------------
// Purpose: list column sort function
//-----------------------------------------------------------------------------
int __cdecl BlacklistedIPAddressCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
	blacklisted_server_t *pSvr1 = ServerBrowserDialog().GetBlacklistPage()->GetBlacklistedServer( p1.userData );
	blacklisted_server_t *pSvr2 = ServerBrowserDialog().GetBlacklistPage()->GetBlacklistedServer( p2.userData );

	if ( !pSvr1 && pSvr2 ) 
		return -1;
	if ( !pSvr2 && pSvr1 )
		return 1;
	if ( !pSvr1 && !pSvr2 )
		return 0;

	if ( pSvr1->m_NetAdr < pSvr2->m_NetAdr )
		return -1;
	else if ( pSvr2->m_NetAdr < pSvr1->m_NetAdr )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Player number comparison function
//-----------------------------------------------------------------------------
int __cdecl BlacklistedAtCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
	blacklisted_server_t *pSvr1 = ServerBrowserDialog().GetBlacklistPage()->GetBlacklistedServer( p1.userData );
	blacklisted_server_t *pSvr2 = ServerBrowserDialog().GetBlacklistPage()->GetBlacklistedServer( p2.userData );

	if ( !pSvr1 && pSvr2 ) 
		return -1;
	if ( !pSvr2 && pSvr1 )
		return 1;
	if ( !pSvr1 && !pSvr2 )
		return 0;

	if ( pSvr1->m_ulTimeBlacklistedAt > pSvr2->m_ulTimeBlacklistedAt )
		return -1;
	if ( pSvr1->m_ulTimeBlacklistedAt < pSvr2->m_ulTimeBlacklistedAt )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBlacklistedServers::CBlacklistedServers( vgui::Panel *parent ) : 
	vgui::PropertyPage( parent, "BlacklistedGames" )
{
	SetSize( 624, 278 );

	m_pAddServer = new Button(this, "AddServerButton", "#ServerBrowser_AddServer");
	m_pAddCurrentServer = new Button(this, "AddCurrentServerButton", "#ServerBrowser_AddCurrentServer");
	m_pGameList = vgui::SETUP_PANEL( new vgui::ListPanel(this, "gamelist") );
	m_pGameList->SetAllowUserModificationOfColumns(true);

	// Add the column headers
	m_pGameList->AddColumnHeader(0, "Name", "#ServerBrowser_BlacklistedServers", 50, ListPanel::COLUMN_RESIZEWITHWINDOW | ListPanel::COLUMN_UNHIDABLE);
	m_pGameList->AddColumnHeader(1, "IPAddr", "#ServerBrowser_IPAddress", 64, ListPanel::COLUMN_HIDDEN);
	m_pGameList->AddColumnHeader(2, "BlacklistedAt", "#ServerBrowser_BlacklistedDate", 100);

	//m_pGameList->SetColumnHeaderTooltip(0, "#ServerBrowser_PasswordColumn_Tooltip");

	// setup fast sort functions
	m_pGameList->SetSortFunc(0, BlacklistedServerNameCompare);
	m_pGameList->SetSortFunc(1, BlacklistedIPAddressCompare);
	m_pGameList->SetSortFunc(2, BlacklistedAtCompare);

	// Sort by name by default
	m_pGameList->SetSortColumn(0);

	m_blackList.Reset();
	m_blackListTimestamp = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBlacklistedServers::~CBlacklistedServers()
{
	ClearServerList();
}

//-----------------------------------------------------------------------------
// Purpose: loads the initial blacklist from disk
//-----------------------------------------------------------------------------
void CBlacklistedServers::LoadBlacklistedList()
{
	m_pGameList->SetEmptyListText("#ServerBrowser_NoBlacklistedServers");

	ClearServerList();
	m_blackList.Reset();

	int count = m_blackList.LoadServersFromFile( BLACKLIST_DEFAULT_SAVE_FILE, false );

	m_blackListTimestamp = g_pFullFileSystem->GetFileTime( BLACKLIST_DEFAULT_SAVE_FILE );

	for( int i=0; i<count; ++i )
	{
		UpdateBlacklistUI( m_blackList.GetServer(i) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds all the servers inside the specified file to the blacklist
//-----------------------------------------------------------------------------
bool CBlacklistedServers::AddServersFromFile( const char *pszFilename, bool bResetTimes )
{
	KeyValues *pKV = new KeyValues( "serverblacklist" );
	if ( !pKV->LoadFromFile( g_pFullFileSystem, pszFilename, "GAME" ) )
		return false;

	for ( KeyValues *pData = pKV->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey() )
	{
		const char *pszName = pData->GetString( "name" );

		uint32 ulDate = pData->GetInt( "date" );
		if ( bResetTimes )
		{
			time_t today;
			time( &today );
			ulDate = today;
		}

		const char *pszNetAddr = pData->GetString( "addr" );

		if ( pszNetAddr && pszNetAddr[0] && pszName && pszName[0] )
		{
			blacklisted_server_t *blackServer = m_blackList.AddServer( pszName, pszNetAddr, ulDate );

			UpdateBlacklistUI( blackServer );
		}
	}

	// write out blacklist to preserve changes
	SaveBlacklistedList();

	pKV->deleteThis();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: save blacklist to disk
//-----------------------------------------------------------------------------
void CBlacklistedServers::SaveBlacklistedList()
{
	m_blackList.SaveToFile( BLACKLIST_DEFAULT_SAVE_FILE );
	m_blackListTimestamp = g_pFullFileSystem->GetFileTime( BLACKLIST_DEFAULT_SAVE_FILE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBlacklistedServers::AddServer( gameserveritem_t &server )
{
	blacklisted_server_t *blackServer = m_blackList.AddServer( server );

	if ( !blackServer )
	{
		return;
	}

	SaveBlacklistedList();

	UpdateBlacklistUI( blackServer );

	if ( GameSupportsReplay() )
	{
		// send command to propagate to the client so the client can send it on to the GC
		char command[ 256 ];
		Q_snprintf( command, Q_ARRAYSIZE( command ), "rbgc %s\n", blackServer->m_NetAdr.ToString() );
		g_pRunGameEngine->AddTextCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
blacklisted_server_t *CBlacklistedServers::GetBlacklistedServer( int iServerID )
{
	return m_blackList.GetServer( iServerID );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBlacklistedServers::IsServerBlacklisted( gameserveritem_t &server )
{
	// if something has changed the blacklist file, reload it
	if ( g_pFullFileSystem->GetFileTime( BLACKLIST_DEFAULT_SAVE_FILE ) != m_blackListTimestamp )
	{
		LoadBlacklistedList();
	}

	return m_blackList.IsServerBlacklisted( server );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBlacklistedServers::UpdateBlacklistUI( blacklisted_server_t *blackServer )
{
	if ( !blackServer )
		return;

	KeyValues *kv;
	int iItemId = m_pGameList->GetItemIDFromUserData( blackServer->m_nServerID );
	if ( m_pGameList->IsValidItemID( iItemId ) )
	{
		// we're updating an existing entry
		kv = m_pGameList->GetItem( iItemId );
		m_pGameList->SetUserData( iItemId, blackServer->m_nServerID );
	}
	else
	{
		// new entry
		kv = new KeyValues("Server");
	}

	kv->SetString( "name", blackServer->m_szServerName );

	// construct a time string for blacklisted time
	struct tm *now;
	now = localtime( (time_t*)&blackServer->m_ulTimeBlacklistedAt );
	if ( now ) 
	{
		char buf[64];
		strftime(buf, sizeof(buf), "%a %d %b %I:%M%p", now);
		Q_strlower(buf + strlen(buf) - 4);
		kv->SetString("BlacklistedAt", buf);
	}

	kv->SetString( "IPAddr", blackServer->m_NetAdr.ToString() );

	if ( !m_pGameList->IsValidItemID( iItemId ) )
	{
		// new server, add to list
		iItemId = m_pGameList->AddItem(kv, blackServer->m_nServerID, false, false);
		kv->deleteThis();
	}
	else
	{
		// tell the list that we've changed the data
		m_pGameList->ApplyItemChanges( iItemId );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBlacklistedServers::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	const char *pPathID = "PLATFORM";
	const char *pszFileName = "servers/BlacklistedServersPage.res";
	if ( g_pFullFileSystem->FileExists( pszFileName, "MOD" ) )
	{
		pPathID = "MOD";
	}	
	LoadControlSettings( pszFileName, pPathID );

	vgui::HFont hFont = pScheme->GetFont( "ListSmall", IsProportional() );
	if ( !hFont )
	{
		hFont = pScheme->GetFont( "DefaultSmall", IsProportional() );
	}
	m_pGameList->SetFont( hFont );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBlacklistedServers::OnPageShow( void )
{
	// reload list since client may have changed it
	LoadBlacklistedList();

	m_pGameList->SetEmptyListText("#ServerBrowser_NoBlacklistedServers");
	m_pGameList->SortList();

	BaseClass::OnPageShow();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBlacklistedServers::GetSelectedServerID( void )
{
	int serverID = -1;
	if ( m_pGameList->GetSelectedItemsCount() )
	{
		serverID = m_pGameList->GetItemUserData( m_pGameList->GetSelectedItem(0) );
	}

	return serverID;
}

//-----------------------------------------------------------------------------
// Purpose: opens context menu (user right clicked on a server)
//-----------------------------------------------------------------------------
void CBlacklistedServers::OnOpenContextMenu(int itemID)
{
	CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu( m_pGameList );

	// get the server
	int serverID = GetSelectedServerID();

	menu->ShowMenu( this,(uint32)-1, false, false, false, false );
	if ( serverID != -1 )
	{
		menu->AddMenuItem("RemoveServer", "#ServerBrowser_RemoveServerFromBlacklist", new KeyValues("RemoveFromBlacklist"), this);
	}

	menu->AddMenuItem("AddServerByName", "#ServerBrowser_AddServerByIP", new KeyValues("AddServerByName"), this);
}


//-----------------------------------------------------------------------------
// Purpose: Adds a server by IP address
//-----------------------------------------------------------------------------
void CBlacklistedServers::OnAddServerByName()
{
	// open the add server dialog
	CDialogAddBlacklistedServer *dlg = new CDialogAddBlacklistedServer( &ServerBrowserDialog(), NULL );
	dlg->MoveToCenterOfScreen();
	dlg->DoModal();
}

//-----------------------------------------------------------------------------
// Purpose: removes a server from the blacklist
//-----------------------------------------------------------------------------
void CBlacklistedServers::OnRemoveFromBlacklist()
{
	// iterate the selection
	for ( int iGame = (m_pGameList->GetSelectedItemsCount() - 1); iGame >= 0; iGame-- )
	{
		int itemID = m_pGameList->GetSelectedItem( iGame );
		int serverID = m_pGameList->GetItemData( itemID )->userData;

		m_pGameList->RemoveItem( itemID );
		m_blackList.RemoveServer( serverID );
	}

	InvalidateLayout();
	Repaint();

	ServerBrowserDialog().BlacklistsChanged();

	SaveBlacklistedList();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBlacklistedServers::ClearServerList( void )
{
	m_pGameList->RemoveAll();
	m_blackList.Reset();
	m_blackListTimestamp = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Adds the currently connected server to the list
//-----------------------------------------------------------------------------
void CBlacklistedServers::OnAddCurrentServer()
{
	gameserveritem_t *pConnected = ServerBrowserDialog().GetCurrentConnectedServer();

	if ( pConnected )
	{
		blacklisted_server_t *blackServer = m_blackList.AddServer( *pConnected );

		if ( !blackServer )
		{
			return;
		}

		UpdateBlacklistUI( blackServer );

		ServerBrowserDialog().BlacklistsChanged();

		SaveBlacklistedList();

		if ( GameSupportsReplay() )
		{
			// send command to propagate to the client so the client can send it on to the GC
			char command[ 256 ];
			Q_snprintf( command, Q_ARRAYSIZE( command ), "rbgc %s\n", pConnected->m_NetAdr.GetConnectionAddressString() );
			g_pRunGameEngine->AddTextCommand( command );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBlacklistedServers::OnImportBlacklist()
{
	if ( m_hImportDialog.Get() )
	{
		m_hImportDialog.Get()->MarkForDeletion();
	}

	m_hImportDialog = new FileOpenDialog( this, "#ServerBrowser_ImportBlacklistTitle", true );
	if ( m_hImportDialog.Get() )
	{
		m_hImportDialog->SetStartDirectory( "cfg/" );
		m_hImportDialog->AddFilter( "*.txt", "#ServerBrowser_BlacklistFiles", true );
		m_hImportDialog->DoModal( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBlacklistedServers::OnFileSelected( char const *fullpath )
{
	AddServersFromFile( fullpath, true );

	if ( m_hImportDialog.Get() )
	{
		m_hImportDialog.Get()->MarkForDeletion();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Parse posted messages
//			 
//-----------------------------------------------------------------------------
void CBlacklistedServers::OnCommand(const char *command)
{
	if (!Q_stricmp(command, "AddServerByName"))
	{
		OnAddServerByName();
	}
	else if (!Q_stricmp(command, "AddCurrentServer" ))
	{
		OnAddCurrentServer();
	}
	else if (!Q_stricmp(command, "ImportBlacklist" ))
	{
		OnImportBlacklist();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

//-----------------------------------------------------------------------------
// Purpose: enables adding server
//-----------------------------------------------------------------------------
void CBlacklistedServers::OnConnectToGame()
{
	m_pAddCurrentServer->SetEnabled( true );
}

//-----------------------------------------------------------------------------
// Purpose: disables adding current server
//-----------------------------------------------------------------------------
void CBlacklistedServers::OnDisconnectFromGame( void )
{
	m_pAddCurrentServer->SetEnabled( false );
}
