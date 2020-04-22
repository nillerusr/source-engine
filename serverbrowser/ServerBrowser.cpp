//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "pch_serverbrowser.h"

// expose the server browser interfaces
CServerBrowser g_ServerBrowserSingleton;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CServerBrowser, IServerBrowser, SERVERBROWSER_INTERFACE_VERSION, g_ServerBrowserSingleton);
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CServerBrowser, IVGuiModule, "VGuiModuleServerBrowser001", g_ServerBrowserSingleton); // the interface loaded by PlatformMenu.vdf

// singleton accessor
CServerBrowser &ServerBrowser()
{
	return g_ServerBrowserSingleton;
}

IRunGameEngine *g_pRunGameEngine = NULL;

static CSteamAPIContext g_SteamAPIContext;
CSteamAPIContext *steamapicontext = &g_SteamAPIContext;

IEngineReplay *g_pEngineReplay = NULL;

ConVar sb_firstopentime( "sb_firstopentime", "0", FCVAR_DEVELOPMENTONLY, "Indicates the time the server browser was first opened." );
ConVar sb_numtimesopened( "sb_numtimesopened", "0", FCVAR_DEVELOPMENTONLY, "Indicates the number of times the server browser was opened this session." );

// the original author of this code felt strdup was not acceptible.
inline char *CloneString( const char *str )
{
	char *cloneStr = new char [ strlen(str)+1 ];
	strcpy( cloneStr, str );
	return cloneStr;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CServerBrowser::CServerBrowser()
{
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CServerBrowser::~CServerBrowser()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CServerBrowser::CreateDialog()
{
	if (!m_hInternetDlg.Get())
	{
		m_hInternetDlg = new CServerBrowserDialog(NULL); // SetParent() call below fills this in
		m_hInternetDlg->Initialize();
	}
}


//-----------------------------------------------------------------------------
// Purpose: links to vgui and engine interfaces
//-----------------------------------------------------------------------------
bool CServerBrowser::Initialize(CreateInterfaceFn *factorylist, int factoryCount)
{
	ConnectTier1Libraries( factorylist, factoryCount );
	ConVar_Register();
	ConnectTier2Libraries( factorylist, factoryCount );
	ConnectTier3Libraries( factorylist, factoryCount );
	g_pRunGameEngine = NULL;

	for ( int i = 0; i < factoryCount; ++i )
	{
		if ( !g_pEngineReplay )
		{
			g_pEngineReplay = ( IEngineReplay * )factorylist[ i ]( ENGINE_REPLAY_INTERFACE_VERSION, NULL );
		}
	}
	
	SteamAPI_InitSafe();
	SteamAPI_SetTryCatchCallbacks( false ); // We don't use exceptions, so tell steam not to use try/catch in callback handlers
	steamapicontext->Init();

	for (int i = 0; i < factoryCount; i++)
	{
		if (!g_pRunGameEngine)
		{
			g_pRunGameEngine = (IRunGameEngine *)(factorylist[i])(RUNGAMEENGINE_INTERFACE_VERSION, NULL);
		}
	}

	// load the vgui interfaces
#if defined( STEAM ) || defined( HL1 )
	if ( !vgui::VGuiControls_Init("ServerBrowser", factorylist, factoryCount) )
#else
	if ( !vgui::VGui_InitInterfacesList("ServerBrowser", factorylist, factoryCount) )
#endif
		return false;

	// load localization file
	g_pVGuiLocalize->AddFile( "servers/serverbrowser_%language%.txt" );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: links to other modules interfaces (tracker)
//-----------------------------------------------------------------------------
bool CServerBrowser::PostInitialize(CreateInterfaceFn *modules, int factoryCount)
{
	// find the interfaces we need
	for (int i = 0; i < factoryCount; i++)
	{
		if (!g_pRunGameEngine)
		{
			g_pRunGameEngine = (IRunGameEngine *)(modules[i])(RUNGAMEENGINE_INTERFACE_VERSION, NULL);
		}
	}

	CreateDialog();
	m_hInternetDlg->SetVisible(false);

	return g_pRunGameEngine;
}


//-----------------------------------------------------------------------------
// Purpose: true if the user can't play a game due to VAC banning
//-----------------------------------------------------------------------------
bool CServerBrowser::IsVACBannedFromGame( int nAppID )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Marks that the tool/game loading us intends to feed us workshop information
//-----------------------------------------------------------------------------
void CServerBrowser::SetWorkshopEnabled( bool bManaged )
{
	m_bWorkshopEnabled = bManaged;
}

//-----------------------------------------------------------------------------
// Purpose: Add a mapname to our known user-subscribed workshop maps list
//-----------------------------------------------------------------------------
void CServerBrowser::AddWorkshopSubscribedMap( const char *pszMapName )
{
	CUtlString strMap( pszMapName );
	if ( !IsWorkshopSubscribedMap( strMap ) )
	{
		m_vecWorkshopSubscribedMaps.AddToTail( strMap );
	}
}

//-----------------------------------------------------------------------------
// Purpose: remove a mapname to our known user-subscribed workshop maps list
//-----------------------------------------------------------------------------
void CServerBrowser::RemoveWorkshopSubscribedMap( const char *pszMapName )
{
	m_vecWorkshopSubscribedMaps.FindAndFastRemove( CUtlString( pszMapName ) );
}

//-----------------------------------------------------------------------------
// Purpose: Well, is it?
//-----------------------------------------------------------------------------
bool CServerBrowser::IsWorkshopEnabled()
{
	return m_bWorkshopEnabled;
}

//-----------------------------------------------------------------------------
// Purpose: Check if this map is in our subscribed list
//-----------------------------------------------------------------------------
bool CServerBrowser::IsWorkshopSubscribedMap( const char *pszMapName )
{
	return m_vecWorkshopSubscribedMaps.HasElement( CUtlString( pszMapName ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CServerBrowser::IsValid()
{
	return ( g_pRunGameEngine );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CServerBrowser::Activate()
{
	static bool firstTimeOpening = true;
	if ( firstTimeOpening )
	{
		m_hInternetDlg->LoadUserData(); // reload the user data the first time the dialog is made visible, helps with the lag between module load and
										// steamui getting Deactivate() call
		firstTimeOpening = false;
	}

	int numTimesOpened = sb_numtimesopened.GetInt() + 1;
	sb_numtimesopened.SetValue( numTimesOpened );
	if ( numTimesOpened == 1 )
	{
		time_t aclock;
		time( &aclock );
		sb_firstopentime.SetValue( (int) aclock );
	}

	Open();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: called when the server browser gets used in the game
//-----------------------------------------------------------------------------
void CServerBrowser::Deactivate()
{
	if (m_hInternetDlg.Get())
	{
		m_hInternetDlg->SaveUserData();
	}
}


//-----------------------------------------------------------------------------
// Purpose: called when the server browser is no longer being used in the game
//-----------------------------------------------------------------------------
void CServerBrowser::Reactivate()
{
	if (m_hInternetDlg.Get())
	{
		m_hInternetDlg->LoadUserData();
		if (m_hInternetDlg->IsVisible())
		{
			m_hInternetDlg->RefreshCurrentPage();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CServerBrowser::Open()
{
	m_hInternetDlg->Open();
}


//-----------------------------------------------------------------------------
// Purpose: returns direct handle to main server browser dialog
//-----------------------------------------------------------------------------
vgui::VPANEL CServerBrowser::GetPanel()
{
	return m_hInternetDlg.Get() ? m_hInternetDlg->GetVPanel() : NULL;
}


//-----------------------------------------------------------------------------
// Purpose: sets the parent panel of the main module panel
//-----------------------------------------------------------------------------
void CServerBrowser::SetParent(vgui::VPANEL parent)
{
	if (m_hInternetDlg.Get())
	{
		m_hInternetDlg->SetParent(parent);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Closes down the server browser for good
//-----------------------------------------------------------------------------
void CServerBrowser::Shutdown()
{
	if (m_hInternetDlg.Get())
	{
		m_hInternetDlg->Close();
		m_hInternetDlg->MarkForDeletion();
	}

#if defined( STEAM )
	vgui::VGuiControls_Shutdown();
#endif

	DisconnectTier3Libraries();
	DisconnectTier2Libraries();
	ConVar_Unregister();
	DisconnectTier1Libraries();
}


//-----------------------------------------------------------------------------
// Purpose: opens a game info dialog to watch the specified server; associated with the friend 'userName'
//-----------------------------------------------------------------------------
bool CServerBrowser::OpenGameInfoDialog( uint64 ulSteamIDFriend, const char *pszConnectCode )
{
#if !defined( _X360 ) // X360TBD: SteamFriends()
	if ( m_hInternetDlg.Get() )
	{
		// activate an already-existing dialog
		CDialogGameInfo *pDialogGameInfo = m_hInternetDlg->GetDialogGameInfoForFriend( ulSteamIDFriend );
		if ( pDialogGameInfo )
		{
			pDialogGameInfo->Activate();
			return true;
		}

		// none yet, create a new dialog
		FriendGameInfo_t friendGameInfo;
		if ( steamapicontext->SteamFriends()->GetFriendGamePlayed( ulSteamIDFriend, &friendGameInfo ) )
		{
			uint16 usConnPort = friendGameInfo.m_usGamePort;
			if ( friendGameInfo.m_usQueryPort < QUERY_PORT_ERROR )
				usConnPort = friendGameInfo.m_usQueryPort;
			CDialogGameInfo *pDialogGameInfo = m_hInternetDlg->OpenGameInfoDialog( friendGameInfo.m_unGameIP, friendGameInfo.m_usGamePort, usConnPort, pszConnectCode );
			pDialogGameInfo->SetFriend( ulSteamIDFriend );
			return true;
		}
	}
#endif
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: joins a specified game - game info dialog will only be opened if the server is fully or passworded
//-----------------------------------------------------------------------------
bool CServerBrowser::JoinGame( uint64 ulSteamIDFriend, const char *pszConnectCode )
{
	if ( OpenGameInfoDialog( ulSteamIDFriend, pszConnectCode ) )
	{
		CDialogGameInfo *pDialogGameInfo = m_hInternetDlg->GetDialogGameInfoForFriend( ulSteamIDFriend );
		pDialogGameInfo->Connect();
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: joins a game by IP/Port
//-----------------------------------------------------------------------------
bool CServerBrowser::JoinGame( uint32 unGameIP, uint16 usGamePort, const char *pszConnectCode )
{
    m_hInternetDlg->JoinGame( unGameIP, usGamePort, pszConnectCode );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: forces the game info dialog closed
//-----------------------------------------------------------------------------
void CServerBrowser::CloseGameInfoDialog( uint64 ulSteamIDFriend )
{
	CDialogGameInfo *pDialogGameInfo = m_hInternetDlg->GetDialogGameInfoForFriend( ulSteamIDFriend );
	if ( pDialogGameInfo )
	{
		pDialogGameInfo->Close();
	}
}


//-----------------------------------------------------------------------------
// Purpose: closes all the game info dialogs
//-----------------------------------------------------------------------------
void CServerBrowser::CloseAllGameInfoDialogs()
{
	if ( m_hInternetDlg.Get() )
	{
		m_hInternetDlg->CloseAllGameInfoDialogs();
	}
}

CUtlVector< gametypes_t > g_GameTypes;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void LoadGameTypes( void )
{
	if ( g_GameTypes.Count() > 0 )
		return;

	#define GAMETYPES_FILE				"servers/ServerBrowserGameTypes.txt"

	KeyValues * kv = new KeyValues( GAMETYPES_FILE );

	if  ( !kv->LoadFromFile( g_pFullFileSystem, GAMETYPES_FILE, "MOD" ) )
	{
		kv->deleteThis();
		return;
	}

	g_GameTypes.RemoveAll();

	for ( KeyValues *pData = kv->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey() )
	{
		gametypes_t gametype;

		gametype.pPrefix = CloneString( pData->GetString( "prefix", "" ) );
		gametype.pGametypeName = CloneString( pData->GetString( "name", "" ) );
		g_GameTypes.AddToTail( gametype );
	}
	

	kv->deleteThis();
}

const char *GetGameTypeName( const char *pMapName )
{
	LoadGameTypes();
	for ( int i = 0; i < g_GameTypes.Count(); i++ )
	{
		int iLength = strlen( g_GameTypes[i].pPrefix );

		if ( !Q_strncmp( pMapName, g_GameTypes[i].pPrefix, iLength ) )
		{
			return g_GameTypes[i].pGametypeName;
		}
	}

	return "";
}

//-----------------------------------------------------------------------------
// Purpose of comments like these: none
//-----------------------------------------------------------------------------
const char *CServerBrowser::GetMapFriendlyNameAndGameType( const char *pszMapName, char *szFriendlyMapName, int cchFriendlyName )
{
	// Make sure game types are loaded
	LoadGameTypes();

	// Scan list
	const char *pszFriendlyGameTypeName = "";
	for ( int i = 0; i < g_GameTypes.Count(); i++ )
	{
		int iLength = strlen( g_GameTypes[i].pPrefix );

		if ( !Q_strnicmp( pszMapName, g_GameTypes[i].pPrefix, iLength ) )
		{
			pszMapName += iLength;
			pszFriendlyGameTypeName = g_GameTypes[i].pGametypeName;
			break;
		}
	}

	// See how many characters from the name to copy.
	// Start by assuming we'll copy the whole thing.
	// (After any prefix we just skipped)
	int l = V_strlen( pszMapName );
	const char *pszFinal = Q_stristr( pszMapName, "_final" );
	if ( pszFinal )
	{
		// truncate the _final (or _final1) part of the filename if it's at the end of the name
		const char *pszNextChar = pszFinal + Q_strlen( "_final" );
		if ( ( *pszNextChar == '\0' ) ||
			( ( *pszNextChar == '1' ) && ( *(pszNextChar+1) == '\0' ) ) )
		{
			l = pszFinal - pszMapName;
		}
	}

	// Safety check against buffer size
	if ( l >= cchFriendlyName )
	{
		Assert( !"Map name too long for buffer!" );
		l = cchFriendlyName-1;
	}

	// Copy friendly portion of name only
	V_memcpy( szFriendlyMapName, pszMapName, l );

	// It's like the Alamo.  We never forget.
	szFriendlyMapName[l] = '\0';

	// Result should be the friendly game type name
	return pszFriendlyGameTypeName;
}

