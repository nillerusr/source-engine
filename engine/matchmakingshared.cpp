//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Handles joining clients together in a matchmaking session before a multiplayer
//			game, tracking new players and dropped players during the game, and reporting
//			game results and stats after the game is complete.
//
//=============================================================================//

#include "proto_oob.h"
#include "matchmaking.h"
#include "matchmakingqos.h"
#include "Session.h"
#include "vgui_baseui_interface.h"
#include "cdll_engine_int.h"
#include "convar.h"
#include "cmd.h"
#include "iclient.h"
#include "server.h"
#include "host.h"

#if defined( _X360 )   
#include "xbox/xbox_win32stubs.h"
#include "audio/private/snd_dev_xaudio.h"
#include "audio_pch.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CMatchmaking s_Matchmaking;
CMatchmaking *g_pMatchmaking = &s_Matchmaking;

extern IVEngineClient *engineClient;
extern IXboxSystem *g_pXboxSystem;

// Expose an interface for GameUI
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CMatchmaking, IMatchmaking, VENGINE_MATCHMAKING_VERSION, s_Matchmaking );

bool Channel_LessFunc( const uint &a, const uint &b )
{
	return a < b;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMatchmaking::CMatchmaking() : m_Channels( Channel_LessFunc )
{
	m_bPreventFullServerStartup = false;
	m_bCleanup = false;
	m_bEnteredLobby = false;
	m_nTotalTeams = 0;
	m_pSearchResults = NULL;
	m_pSessionKeys = new KeyValues( "SessionKeys" );

	m_Session.SetParent( this );

	m_CurrentState = MMSTATE_INITIAL;
	m_InviteState = INVITE_NONE;

	memset( &m_InviteWaitingInfo, 0, sizeof( m_InviteWaitingInfo ) );
}

CMatchmaking::~CMatchmaking()
{
	Cleanup();
	m_pSessionKeys->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Cleanup the matchmaking class to enable re-entry
//-----------------------------------------------------------------------------
void CMatchmaking::Cleanup()
{
	m_bInitialized = false;
	m_bCleanup = false;
	m_bEnteredLobby = false;

	m_Host.Clear();

#ifdef _X360
	if ( Audio_GetXVoice() )
	{
		CClientInfo *pLocal = NULL;

		if ( m_bCreatedLocalTalker )
		{
			pLocal = &m_Local;
		}

		Audio_GetXVoice()->RemoveAllTalkers( pLocal );
	}
#endif
	m_bCreatedLocalTalker = false;
	SetPreventFullServerStartup( false, "Cleanup\n" );

	m_Session.ResetSession();

	// TODO: Check on overlapped operations and cancel them
	//	g_pXboxSystem->CancelAsyncOperations();

	ClearSearchResults();
	m_pSessionKeys->Clear();

	m_pGameServer = NULL;

	Q_memset( m_Mutelist, 0, sizeof( m_Mutelist ) );
	for ( int i = 0; i < MAX_PLAYERS_PER_CLIENT; ++i )
	{
		m_MutedBy[i].Purge();
	}

	m_Channels.RemoveAll();

	m_SessionContexts.Purge();
	m_SessionProperties.Purge();
	m_PlayerStats.Purge();
	m_Remote.PurgeAndDeleteElements();

	m_nGameSize = 0;
	m_nPrivateSlots = 0;
	m_nSendCount = 0;

	m_fHeartbeatInterval = HEARTBEAT_INTERVAL_SHORT;
	m_fNextHeartbeatTime = GetTime();
}

int CMatchmaking::FindOrCreateContext( const uint id )
{
	int idx = m_SessionContexts.InvalidIndex();
	for ( int i = 0; i < m_SessionContexts.Count(); ++i )
	{
		if ( m_SessionContexts[i].dwContextId == id )
		{
			idx = i;
		}
	}
	if ( !m_SessionContexts.IsValidIndex( idx ) )
	{
		idx = m_SessionContexts.AddToTail();
	}
	return idx;
}

int CMatchmaking::FindOrCreateProperty( const uint id )
{
	int idx = m_SessionProperties.InvalidIndex();
	for ( int i = 0; i < m_SessionProperties.Count(); ++i )
	{
		if ( m_SessionProperties[i].dwPropertyId == id )
		{
			idx = i;
		}
	}
	if ( !m_SessionProperties.IsValidIndex( idx ) )
	{
		idx = m_SessionProperties.AddToTail();
	}
	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: Add an additional property to the current session
//-----------------------------------------------------------------------------
void CMatchmaking::AddSessionProperty( const uint nType, const char *pID, const char *pValue, const char *pValueType )
{
	KeyValues *pProperty = m_pSessionKeys->FindKey( pID, true );
	pProperty->SetName( pID );
	pProperty->SetInt( "type", nType );
	pProperty->SetString( "valuestring", pValue );
	pProperty->SetString( "valuetype", pValueType );

	AddSessionPropertyInternal( pProperty );
}

//-----------------------------------------------------------------------------
// Purpose: Set properties and contexts for the current session
//-----------------------------------------------------------------------------
void CMatchmaking::SetSessionProperties( KeyValues *pPropertyKeys )
{
	m_SessionContexts.RemoveAll();
	m_SessionProperties.RemoveAll();

	m_pSessionKeys->Clear();
	pPropertyKeys->CopySubkeys( m_pSessionKeys );

	for ( KeyValues *pProperty = m_pSessionKeys->GetFirstSubKey(); pProperty != NULL; pProperty = pProperty->GetNextKey() )
	{
		AddSessionPropertyInternal( pProperty );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatchmaking::AddSessionPropertyInternal( KeyValues *pProperty )
{
	const char *pID = pProperty->GetName();
	const char *pValue = pProperty->GetString( "valuestring" );

	switch( pProperty->GetInt( "type" ) )
	{
	case SESSION_CONTEXT:
		{
			Msg( "Adding Context: %s : %s\n", pID, pValue );

			int id = g_ClientDLL->GetPresenceID( pID );
			int val = g_ClientDLL->GetPresenceID( pValue );

			int idx = FindOrCreateContext( id );
			XUSER_CONTEXT &ctx = m_SessionContexts[idx];
			ctx.dwContextId = id;
			ctx.dwValue = val;

			// Set the display string for gameUI
			char szBuffer[MAX_PATH];
			g_ClientDLL->GetPropertyDisplayString( ctx.dwContextId, ctx.dwValue, szBuffer, sizeof( szBuffer ) );
			pProperty->SetString( "displaystring", szBuffer );

			// X360TBD: Such game specifics as this shouldn't be hard-coded
			if ( m_CurrentState != MMSTATE_INITIAL && !Q_stricmp( pID, "CONTEXT_SCENARIO" ) )
			{
				// Set the scenario in our host data structure
				Q_strncpy( m_HostData.scenario, szBuffer, sizeof( m_HostData.scenario ) );
				UpdateSessionReplyData( XNET_QOS_LISTEN_SET_DATA );
			}
		}
		break;

	case SESSION_PROPERTY:
		{
			Msg( "Adding Property: %s : %s\n", pID, pValue );

			if ( !Q_stricmp( pID, "PROPERTY_PRIVATE_SLOTS" ) )
			{
				// "Private Slots" is not a search criteria
				m_nPrivateSlots = atoi( pValue );
				break;
			}

			int id = g_ClientDLL->GetPresenceID( pID );

			int idx = FindOrCreateProperty( id );
			XUSER_PROPERTY &prop = m_SessionProperties[idx];
			prop.dwPropertyId = id;

			if ( !Q_stricmp( pProperty->GetString( "valuetype" ), "int" ) )
			{
				prop.value.nData = atoi( pValue );
				prop.value.type = XUSER_DATA_TYPE_INT32;
			}

			// Build out the property keyvalues for gameUI
			char szBuffer[MAX_PATH];
			g_ClientDLL->GetPropertyDisplayString( prop.dwPropertyId, prop.value.nData, szBuffer, sizeof( szBuffer ) );
			pProperty->SetString( "displaystring", szBuffer );

			// X360TBD: Such game specifics as these shouldn't be so hard-coded
			if ( !Q_stricmp( pID, "PROPERTY_GAME_SIZE" ) )
			{
				m_nGameSize = atoi( pValue );
			}
			if ( !Q_stricmp( pID, "PROPERTY_NUMBER_OF_TEAMS" ) )
			{
				m_nTotalTeams = atoi( pValue );
			}
			if ( m_CurrentState != MMSTATE_INITIAL && !Q_stricmp( pID, "PROPERTY_MAX_GAME_TIME" ) )
			{
				// Set the game time in our host data structure
				m_HostData.gameTime = prop.value.nData;
				UpdateSessionReplyData( XNET_QOS_LISTEN_SET_DATA );
			}
		}
		break;

	case SESSION_FLAG:
		m_Session.SetFlag( g_ClientDLL->GetPresenceID( pID ) );
		break;

	default:
		Warning( "Session option type %d not recognized/n", pProperty->GetInt( "type" ) );
		break;

	}
}

KeyValues *CMatchmaking::GetSessionProperties()
{
	return m_pSessionKeys;
}

double CMatchmaking::GetTime()
{
	return Plat_FloatTime();
}

//-----------------------------------------------------------------------------
// Purpose: At netchannel connection, register the messages
//-----------------------------------------------------------------------------
void CMatchmaking::ConnectionStart( INetChannel *chan )
{
	REGISTER_MM_MSG( JoinResponse );
	REGISTER_MM_MSG( ClientInfo );
	REGISTER_MM_MSG( RegisterResponse );
	REGISTER_MM_MSG( Migrate );
	REGISTER_MM_MSG( Mutelist );
	REGISTER_MM_MSG( Checkpoint );
	REGISTER_MM_MSG( Heartbeat );

	REGISTER_CLC_MSG( VoiceData );
}

//-----------------------------------------------------------------------------
// Purpose: Process a networked voice packet
//-----------------------------------------------------------------------------
bool CMatchmaking::ProcessVoiceData( CLC_VoiceData *pVoice )
{
	char chReceived[4096];
	DWORD dwLength = pVoice->m_nLength;
	pVoice->m_DataIn.ReadBits( chReceived, dwLength );

	if ( m_Session.IsHost() )
	{
		char chCopyBuffer[4096];

		// Forward this message on to everyone else
		pVoice->m_DataOut.StartWriting( chCopyBuffer, sizeof ( chCopyBuffer ) );
		Q_memcpy( chCopyBuffer, chReceived, sizeof( chCopyBuffer ) );
		pVoice->m_DataOut.SeekToBit( dwLength );

		SendToRemoteClients( pVoice, true, pVoice->m_xuid );
	}

	// Playback the voice data locally through xaudio
#if defined ( _X360 )
	if ( pVoice->m_xuid != m_Local.m_xuids[0] )
	{
		Audio_GetXVoice()->PlayIncomingVoiceData( pVoice->m_xuid, (byte*)chReceived, dwLength );
	}
#endif
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Delete channels that have been marked for deletion
//-----------------------------------------------------------------------------
void CMatchmaking::CleanupMarkedChannels()
{
	// Clean up net channels that need to be deleted
	for ( int i = 0; i < m_ChannelsToRemove.Count(); ++i )
	{
		INetChannel *pNetChannel = FindChannel( m_ChannelsToRemove[i] );
		if ( pNetChannel )
		{
			if ( !m_Channels.Remove( m_ChannelsToRemove[i] ) )
			{
				Warning( "CleanupMarkedChannels: Failed to remove a channel!\n" );
			}
		}
	}
	m_ChannelsToRemove.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Channels can be flagged for deletion during packet processing.
//			Now that processing is finished, delete them.
//-----------------------------------------------------------------------------
void CMatchmaking::PacketEnd()
{
	CleanupMarkedChannels();
}

//-----------------------------------------------------------------------------
// Purpose: Find a specific client from a netchannel
//-----------------------------------------------------------------------------
CClientInfo *CMatchmaking::FindClient( netadr_t *adr )
{
	CClientInfo *pClient = NULL;
	unsigned int ip = adr->GetIPNetworkByteOrder();

	if ( ip == m_Host.m_adr.GetIPNetworkByteOrder() )
	{
		pClient = &m_Host;
	}
	else
	{
		for ( int i = 0; i < m_Remote.Count(); ++i )
		{
			if ( ip == m_Remote[i]->m_adr.GetIPNetworkByteOrder() )
			{
				pClient = m_Remote[i];
				break;
			}
		}
	}
	return pClient;
}

//-----------------------------------------------------------------------------
// Purpose: Find a specific client by his XUID
//-----------------------------------------------------------------------------
CClientInfo *CMatchmaking::FindClientByXUID( XUID xuid )
{
	CClientInfo *pClient = NULL;

	if ( xuid == m_Host.m_xuids[0] )
	{
		pClient = &m_Host;
	}
	else
	{
		for ( int i = 0; i < m_Remote.Count(); ++i )
		{
			if ( xuid == m_Remote[i]->m_xuids[0] )
			{
				pClient = m_Remote[i];
				break;
			}
		}
	}

	return pClient;
}

//-----------------------------------------------------------------------------
// Purpose: Find a specific client's netchannel
//-----------------------------------------------------------------------------
INetChannel *CMatchmaking::FindChannel( const unsigned int ip )
{
	INetChannel *pChannel = NULL;

	int idx = m_Channels.Find( ip );
	if ( idx != m_Channels.InvalidIndex() )
	{
		pChannel = m_Channels.Element( idx );
	}

	return pChannel;
}

//-----------------------------------------------------------------------------
// Purpose: Create a new netchannel
//-----------------------------------------------------------------------------
INetChannel *CMatchmaking::CreateNetChannel( netadr_t *adr )
{
	INetChannel *pNewChannel = FindChannel( adr->GetIPNetworkByteOrder() );
	if ( !pNewChannel )
	{
		pNewChannel = NET_CreateNetChannel( NS_MATCHMAKING, adr, "MATCHMAKING", this );
	}

	if( pNewChannel )
	{
		// Set a rate limit and other relevant properties
		pNewChannel->SetTimeout( HEARTBEAT_TIMEOUT );
	}

	return pNewChannel;
}

//-----------------------------------------------------------------------------
// Purpose: Add a netchannel for a session client
//-----------------------------------------------------------------------------
INetChannel *CMatchmaking::AddRemoteChannel( netadr_t *adr )
{
	INetChannel *pNetChannel = CreateNetChannel( adr );
	if ( pNetChannel )
	{
		// Save this new channel
		m_Channels.Insert( adr->GetIPNetworkByteOrder(), pNetChannel );
	}
	return pNetChannel;
}

//-----------------------------------------------------------------------------
// Purpose: Remove a netchannel for a session client
//-----------------------------------------------------------------------------
void CMatchmaking::RemoveRemoteChannel( netadr_t *adr, const char *pReason )
{
	INetChannel *pNetChannel = FindChannel( adr->GetIPNetworkByteOrder() );
	if ( pNetChannel )
	{
		m_Channels.Remove( adr->GetIPNetworkByteOrder() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Mark a net channel to be removed
//-----------------------------------------------------------------------------
void CMatchmaking::MarkChannelForRemoval( netadr_t *adr )
{
	m_ChannelsToRemove.AddToTail( adr->GetIPNetworkByteOrder() );
}

//-----------------------------------------------------------------------------
// Purpose: Set the timeout for a net channel
//-----------------------------------------------------------------------------
void CMatchmaking::SetChannelTimeout( netadr_t *adr, int timeout )
{
	INetChannel *pChannel = FindChannel( adr->GetIPNetworkByteOrder() );
	if ( pChannel )
	{
		Msg( "Setting new timeout for ip %d: %d\n", adr->GetIPNetworkByteOrder(), timeout );
		pChannel->SetTimeout( timeout );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Send a net message to a specific address
//-----------------------------------------------------------------------------
void CMatchmaking::SendMessage( INetMessage *msg, netadr_t *adr, bool bVoice )
{
	// Find the matching net channel
	INetChannel *pChannel = FindChannel( adr->GetIPNetworkByteOrder() );
	if ( pChannel )
	{
		pChannel->SendNetMsg( *msg, false, bVoice );
		if ( !pChannel->Transmit() )
		{
			Msg( "Transmit failed\n" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Send a net message to a specific client
//-----------------------------------------------------------------------------
void CMatchmaking::SendMessage( INetMessage *msg, CClientInfo *pClient, bool bVoice )
{
	// Find the matching net channel
	INetChannel *pChannel = FindChannel( pClient->m_adr.GetIPNetworkByteOrder() );
	if ( pChannel )
	{
		pChannel->SendNetMsg( *msg, false, bVoice );
		if ( !pChannel->Transmit() )
		{
			Msg( "Transmit failed\n" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Send a net message to all remote clients
//-----------------------------------------------------------------------------
void CMatchmaking::SendToRemoteClients( INetMessage *msg, bool bVoice, XUID excludeXUID )
{
	for ( int i = 0; i < m_Remote.Count(); ++i )
	{
		CClientInfo *pInfo = m_Remote[i];

		if ( excludeXUID != -1 && pInfo->m_xuids[0] == excludeXUID )
			continue;

		SendMessage( msg, m_Remote[i], true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Send a heartbeat to a specific client
//-----------------------------------------------------------------------------
bool CMatchmaking::SendHeartbeat( CClientInfo *pClient )
{
	if ( pClient->m_adr.GetIPNetworkByteOrder() == 0 )
		return false;

	// Check for timeout
	INetChannel *pChannel = FindChannel( pClient->m_adr.GetIPNetworkByteOrder() );
	if ( pChannel )
	{
//		Msg( "Sending HB\n" );

		if ( pChannel->IsTimedOut() )
		{
			ClientDropped( pClient );
			return false;
		}

		// Send a heartbeat to the client
		MM_Heartbeat beat;
		pChannel->SendNetMsg( beat );	
		pChannel->Transmit();
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Transmit regular messages to keep the connection alive
//-----------------------------------------------------------------------------
void CMatchmaking::SendHeartbeat()
{
	double time = GetTime();
	if ( time < m_fNextHeartbeatTime )
		return;

	m_fNextHeartbeatTime = time + m_fHeartbeatInterval;

	if ( m_Session.IsHost() )
	{
		for ( int i = 0; i < m_Remote.Count(); ++i )
		{
			SendHeartbeat( m_Remote[i] );
		}
	}
	else
	{
		SendHeartbeat( &m_Host );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Look up a player by name
//-----------------------------------------------------------------------------
static uint64 FindPlayerByName( CClientInfo *pClient, const char *pName )
{
	for ( int i = 0; i < XUSER_MAX_COUNT; ++i )
	{
		if ( pClient->m_xuids[i] && !Q_stricmp( pClient->m_szGamertags[i], pName ) )
		{
			return pClient->m_xuids[i];
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Get an xuid from a CBasePlayer id
//-----------------------------------------------------------------------------
uint64 CMatchmaking::PlayerIdToXuid( int playerId )
{
	uint64 ret = 0;

	player_info_t info;
	if ( engineClient->GetPlayerInfo( playerId, &info ) )
	{
		// find the client with a matching name
		for ( int i = 0; i < m_Remote.Count(); ++i )
		{
			ret = FindPlayerByName( m_Remote[i], info.name );
			if ( ret )
			{
				break;
			}
		}
	}

	if ( !ret )
	{
		// Try ourselves
		ret = FindPlayerByName( &m_Local, info.name );
	}

	if ( !ret )
	{
		// Try the host
		ret = FindPlayerByName( &m_Host, info.name );
	}

	return ret;
}

bool CMatchmaking::GameIsActive()
{
	return m_CurrentState > MMSTATE_GAME_ACTIVE;
}

bool CMatchmaking::GameIsLocked()
{
	return ( m_Session.IsArbitrated() && m_CurrentState > MMSTATE_GAME_LOCKED );
}

bool CMatchmaking::ConnectedToServer()
{
	return engineClient->IsConnected();
}

bool CMatchmaking::IsInMigration()
{
	return ( m_CurrentState >= MMSTATE_HOSTMIGRATE_STARTINGMIGRATION && 
			 m_CurrentState <= MMSTATE_HOSTMIGRATE_WAITINGFORHOST );
}

bool CMatchmaking::IsAcceptingConnections()
{
	if ( !m_Session.IsHost() ||
		 m_CurrentState < MMSTATE_ACCEPTING_CONNECTIONS ||
		 m_CurrentState == MMSTATE_PREGAME ||
		 m_CurrentState == MMSTATE_LOADING ||
		 GameIsLocked() )
	{
		return false;
	}
	return true;
}

bool CMatchmaking::IsServer()
{
	// for now, the host is the server
	return m_Session.IsHost();
}

//-----------------------------------------------------------------------------
// Purpose: Helpers to convert between CClientInfo and MM_ClientInfo
//-----------------------------------------------------------------------------
void CMatchmaking::ClientInfoToNetMessage( MM_ClientInfo *pInfo, const CClientInfo *pClient )
{
	pInfo->m_id			= pClient->m_id;
	pInfo->m_xnaddr		= pClient->m_xnaddr;
	pInfo->m_cPlayers	= pClient->m_cPlayers;
	pInfo->m_bInvited	= pClient->m_bInvited;

	Q_memcpy( pInfo->m_xuids, pClient->m_xuids, sizeof( pInfo->m_xuids ) );
	Q_memcpy( pInfo->m_cVoiceState, pClient->m_cVoiceState, sizeof( pInfo->m_cVoiceState ) );
	Q_memcpy( pInfo->m_iTeam, pClient->m_iTeam, sizeof( pInfo->m_iTeam ) );
	Q_memcpy( pInfo->m_iControllers, pClient->m_iControllers, sizeof( pInfo->m_iControllers ) );
	Q_memcpy( pInfo->m_szGamertags, pClient->m_szGamertags, sizeof( pInfo->m_szGamertags ) );
}

void CMatchmaking::NetMessageToClientInfo( CClientInfo *pClient, const MM_ClientInfo *pInfo )
{
	pClient->m_id		= pInfo->m_id;
	pClient->m_xnaddr	= pInfo->m_xnaddr;
	pClient->m_cPlayers = pInfo->m_cPlayers;
	pClient->m_bInvited = pInfo->m_bInvited;

#if defined( _X360 )
	IN_ADDR winaddr;
	XNKID xid = m_Session.GetSessionId();
	if ( XNetXnAddrToInAddr( &pClient->m_xnaddr, &xid, &winaddr ) != 0 )
	{
		Warning( "Error resolving client IP\n" );
	}
 	pClient->m_adr.SetType( NA_IP );
 	pClient->m_adr.SetIPAndPort( winaddr.S_un.S_addr, PORT_MATCHMAKING );
#endif

	Q_memcpy( pClient->m_xuids, pInfo->m_xuids, sizeof( pClient->m_xuids ) );
	Q_memcpy( pClient->m_cVoiceState, pInfo->m_cVoiceState, sizeof( pClient->m_cVoiceState ) );
	Q_memcpy( pClient->m_iTeam, pInfo->m_iTeam, sizeof( pClient->m_iTeam ) );
	Q_memcpy( pClient->m_iControllers, pInfo->m_iControllers, sizeof( pClient->m_iControllers ) );
	Q_memcpy( pClient->m_szGamertags, pInfo->m_szGamertags, sizeof( pClient->m_szGamertags ) );
}

//----------------------------------------
//
//	Host/Client Shared
//
//----------------------------------------
//-----------------------------------------------------------------------------
// Purpose: Set up the properties of the local client machine
//-----------------------------------------------------------------------------
bool CMatchmaking::InitializeLocalClient( bool bIsHost )
{
	Q_memset( &m_Local, 0, sizeof( m_Local ) );

	m_Local.m_bInvited = bIsHost;

#if defined( _X360 )
	while( XNetGetTitleXnAddr( &m_Local.m_xnaddr ) == XNET_GET_XNADDR_PENDING )
		;

	// machine id
	if ( 0 != XNetXnAddrToMachineId( &m_Local.m_xnaddr, &m_Local.m_id ) )
	{
		// User isn't signed in to live, use their xuid instead
		XUserGetXUID( XBX_GetPrimaryUserId(), &m_Local.m_id );
	}

	m_Local.m_cPlayers = 0;

	// Set up the players
	for ( uint i = 0; i < MAX_PLAYERS_PER_CLIENT; ++i )
	{
		// We currently only allow one player per console
		if ( i != XBX_GetPrimaryUserId() )
		{
			continue;
		}

		// xuid
		uint ret = XUserGetXUID( i, &m_Local.m_xuids[m_Local.m_cPlayers] );
		if ( ret == ERROR_NO_SUCH_USER )
		{
			continue;
		}
		else if ( ret != ERROR_SUCCESS )
		{
			return false;
		}

		// gamertag
		ret = XUserGetName( XBX_GetPrimaryUserId(), m_Local.m_szGamertags[m_Local.m_cPlayers], MAX_PLAYER_NAME_LENGTH );
		if ( ret != ERROR_SUCCESS )
		{
			return false;
		}
		m_Local.m_szGamertags[m_Local.m_cPlayers][MAX_PLAYER_NAME_LENGTH - 1] = '\0';

		// Set the player's name in the game
		char szNameCmd[MAX_PLAYER_NAME_LENGTH + 16];
		Q_snprintf( szNameCmd, sizeof( szNameCmd ), "name %s", m_Local.m_szGamertags[m_Local.m_cPlayers] );
		engineClient->ClientCmd( szNameCmd );

		m_Local.m_iControllers[m_Local.m_cPlayers] = i;
		m_Local.m_iTeam[m_Local.m_cPlayers] = -1;


		m_Local.m_cVoiceState[m_Local.m_cPlayers] = 0;

		// number of players on this console
		++m_Local.m_cPlayers;
	}

	// Source can only support one player per console.
	if( m_Local.m_cPlayers > 1 )
	{
		Warning( "Too many players on this console\n" );
		return false;
	}

	// Set up the host data that gets sent back to searching clients
	// By default, the first player is considered the host
	Q_strncpy( m_HostData.hostName, m_Local.m_szGamertags[0], sizeof( m_HostData.hostName ) );
	m_HostData.gameState = GAMESTATE_INLOBBY;
	m_HostData.xuid = m_Local.m_xuids[ XBX_GetPrimaryUserId() ];

#endif

	m_bInitialized = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Connection to the game server has been established, so we can
//			add the local players to the teams that were setup in the lobby.
//-----------------------------------------------------------------------------
void CMatchmaking::AddLocalPlayersToTeams()
{
	if ( !m_bInitialized || XBX_GetPrimaryUserId() == INVALID_USER_ID )
		return;

	if ( m_Local.m_iTeam[0] == -1 )
		return;

	// Convert the team number into a team name
	char szTeamName[32];
	uint id = g_ClientDLL->GetPresenceID( "PROPERTY_TEAM" );
	g_ClientDLL->GetPropertyDisplayString( id, m_Local.m_iTeam[0], szTeamName, sizeof( szTeamName ) );

	Msg( "Joining team: %s\n", szTeamName );

	char cmd[32];
	Q_snprintf( cmd, sizeof( cmd ), "jointeam_nomenus %s", szTeamName );
	engineClient->ClientCmd( cmd );
}

//-----------------------------------------------------------------------------
// Purpose: Map loading is completed - restore full communication
//-----------------------------------------------------------------------------
void CMatchmaking::OnLevelLoadingFinished()
{
	// This functions gets called from some odd places
 	if ( m_CurrentState != MMSTATE_CONNECTED_TO_SERVER )
 		return;

	// Test code to force a disconnect at end of map load
// 	if ( !IsServer() )
// 	{
// 		char cmd[MAX_PATH];
// 		Q_snprintf( cmd, sizeof( cmd ), "connect 127.0.0.1\n" );		
// 		Cbuf_AddText( cmd );
// 		return;
// 	}

	SwitchToState( MMSTATE_INGAME );

	MM_Checkpoint msg;
	msg.m_Checkpoint = MM_Checkpoint::CHECKPOINT_LOADING_COMPLETE;

	if ( m_Session.IsHost() )
	{
		if ( !m_Session.IsArbitrated() )
		{
			// Re-enable response to probes
			m_HostData.gameState = GAMESTATE_INPROGRESS;
			UpdateSessionReplyData( XNET_QOS_LISTEN_ENABLE|XNET_QOS_LISTEN_SET_DATA );
		}

		// Reset netchannel timeouts for any clients that are also finished loading
		for ( int i = 0; i < m_Remote.Count(); ++i )
		{
			CClientInfo *pClient = m_Remote[i];
			if ( pClient->m_bLoaded )
			{
				// Send a reply and reset the netchannel timeout
				SendMessage( &msg, pClient );
				SetChannelTimeout( &pClient->m_adr, HEARTBEAT_TIMEOUT );
			}
		}
	}
	else
	{
		// Tell the host we're finished loading
		SendMessage( &msg, &m_Host );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Process packet from another client
//-----------------------------------------------------------------------------
bool CMatchmaking::ProcessConnectionlessPacket( netpacket_t *pPacket )
{
	Assert( pPacket );

	bf_read &msg = pPacket->message;
	int type = msg.ReadByte();
	switch( type )
	{
	case PTH_SYSTEMLINK_SEARCH:
		HandleSystemLinkSearch( pPacket );
		break;

	case HTP_SYSTEMLINK_REPLY:
		HandleSystemLinkReply( pPacket );
		break;

	case PTH_CONNECT:
		HandleJoinRequest( pPacket );
		break;

	default:
		break;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Process an info update about another client
//-----------------------------------------------------------------------------
bool CMatchmaking::ProcessClientInfo( MM_ClientInfo *pInfo )
{
	CClientInfo *pClient = NULL;
	bool bHost = false;

	if ( m_CurrentState == MMSTATE_INITIAL )
	{
		// Session has been reset, this is a stale message
		Msg( "Received MM_ClientInfo with MMSTATE_INITIAL\n" );
		return true;
	}

	if ( pInfo->m_id == m_Local.m_id )
	{
		if ( pInfo->m_cPlayers == 0 )
		{
			if ( m_CurrentState != MMSTATE_SESSION_DISCONNECTING )
			{
				// We've been kicked
				KickPlayerFromSession( 0 );
				SessionNotification( SESSION_NOTIFY_CLIENT_KICKED );
			}
			return true;
		}
		else
		{
			pClient = &m_Local;
		}
	}

	// Check against our host id
	if ( pInfo->m_id == m_Host.m_id )
	{
		pClient = &m_Host;
		bHost = true;
	}
	else
	{
		// Look for the client in our remote list
		for ( int i = 0; i < m_Remote.Count(); ++i )
		{
			if ( m_Remote[i]->m_id == pInfo->m_id )
			{
				pClient = m_Remote[i];
				break;
			}
		}
	}

	// If we didn't find it, this must be a new client
	if ( !pClient && pInfo->m_cPlayers != 0 )
	{
		Msg( "New client. %s\n", pInfo->ToString() );

		pClient = new CClientInfo();
		m_Remote.AddToTail( pClient );

		// Copy the new client info
		NetMessageToClientInfo( pClient, pInfo );

		AddPlayersToSession( pClient );
		SendPlayerInfoToLobby( pClient );
	}
	else
	{
		// We're updating an existing client
		if ( pInfo->m_cPlayers )
		{
			// Cache off the old client info, as pClient gets updated through this function
			CClientInfo tempClient = *pClient;

			// Check for player changes
			if ( Q_memcmp( &tempClient.m_xuids, pInfo->m_xuids, sizeof( tempClient.m_xuids ) ) )
			{
				// Remove the old players and add the new
				RemovePlayersFromSession( pClient );
				NetMessageToClientInfo( pClient, pInfo );
				AddPlayersToSession( pClient );
			}

			// Check for team changes
			for ( int i = 0; i < pInfo->m_cPlayers; ++i )
			{
				if ( pInfo->m_iTeam[i] != tempClient.m_iTeam[i] || pInfo->m_cVoiceState[i] != tempClient.m_cVoiceState[i] )
				{
					// X360TBD: send real "ready" setting, or remove entirely?
					EngineVGui()->UpdatePlayerInfo( pInfo->m_xuids[i], pInfo->m_szGamertags[i], pInfo->m_iTeam[i], pInfo->m_cVoiceState[i], GetPlayersNeeded(), bHost );
				}
			}

			// Store the new info
			NetMessageToClientInfo( pClient, pInfo );

			if ( m_Session.IsHost() )
			{
				SendToRemoteClients( pInfo );
			}
		}
		else
		{
			// A client has been dropped
			ClientDropped( pClient );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: A connection was lost - respond accordingly
//-----------------------------------------------------------------------------
void CMatchmaking::ClientDropped( CClientInfo *pClient )
{
	if ( !pClient )
	{
		Warning( "Null client pointer in ClientDropped!\n" );
		return;
	}

	if ( m_CurrentState == MMSTATE_SESSION_CONNECTING )
	{
		// Not really dropped, we just failed to connect to the host
		SessionNotification( SESSION_NOTIFY_CONNECT_NOTAVAILABLE );
		return;
	}

	Warning( "Dropped player: %llu!", pClient->m_id );

	// Do this first, before the team assignment gets cleared
	RemovePlayersFromSession( pClient );

	// Remove all players from the lobby
	for ( int i = 0; i < pClient->m_cPlayers; ++i )
	{
		pClient->m_iTeam[i] = -1;
	}
	SendPlayerInfoToLobby( pClient );

	MarkChannelForRemoval( &pClient->m_adr );

	if ( pClient == &m_Host )
	{
		// The host was lost
		if ( m_Session.IsSystemLink() )
		{
			// Can't migrate system link sessions
			SessionNotification( SESSION_NOTIFY_LOST_HOST );
		}
		else
		{
			// X360TBD: Migration still doesn't work correctly
			SessionNotification( SESSION_NOTIFY_LOST_HOST );
			/* 
			// Start migrating
			if ( !IsInMigration() )
			{
				m_PreMigrateState = m_CurrentState;
				StartHostMigration();
			}
			*/
		}
	}
	else
	{
		if ( m_Session.IsHost() )
		{
			// Send a disconnect ack back to the client
			MM_Checkpoint msg;
			msg.m_Checkpoint = MM_Checkpoint::CHECKPOINT_SESSION_DISCONNECT;
			SendMessage( &msg, &pClient->m_adr );

			// Tell everyone else this client is gone
			MM_ClientInfo droppedPlayer;
			ClientInfoToNetMessage( &droppedPlayer, pClient );
			droppedPlayer.m_cPlayers = 0;

			SendToRemoteClients( &droppedPlayer );

			for ( int i = 0; i < sv.GetClientCount(); i++ )
			{
				IClient *pIClient = sv.GetClient(i);
				bool bFound = false;
				
				if ( pIClient )
				{
					for ( int j = 0; j < pClient->m_cPlayers; ++j )
					{
						if ( pClient->m_xuids[j] == 0 )
							continue;

						if ( Q_stricmp( pIClient->GetClientName(), pClient->m_szGamertags[j] ) == 0 )
						{
							bFound = true;
							pIClient->Disconnect( "Timed Out" );
							break;
						}
					}
				}

				if ( bFound == true )
					break;
			}
		}
	}

	if ( m_Remote.FindAndRemove( pClient ) )
	{
		delete pClient;
	}
}

//-----------------------------------------------------------------------------
// Purpose: A player is leaving the session
//-----------------------------------------------------------------------------
void CMatchmaking::PerformDisconnect()
{
	if ( m_CurrentState == MMSTATE_SESSION_DISCONNECTING )
	{
		if ( ConnectedToServer() )
		{
			engineClient->ExecuteClientCmd( "disconnect" );
			EngineVGui()->ActivateGameUI();
		}

		if ( m_bEnteredLobby )
		{
			EngineVGui()->SessionNotification( SESSION_NOTIFY_WELCOME );
		}
		SwitchToState( MMSTATE_INITIAL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: A player is leaving the session
//-----------------------------------------------------------------------------
void CMatchmaking::KickPlayerFromSession( uint64 id )
{
	MM_ClientInfo droppedPlayer;

	if ( m_Local.m_xuids[0] == id || id == 0 )
	{
		// We've been kicked, or voluntarily left the session
		ClientInfoToNetMessage( &droppedPlayer, &m_Local );
		droppedPlayer.m_cPlayers = 0;

		if ( m_Session.IsHost() )
		{
			// Tell all the clients
			SendToRemoteClients( &droppedPlayer );
		}
		else
		{
			// tell the host to drop us
			SendMessage( &droppedPlayer, &m_Host );
		}

		// Prepare to close the session and reset
		SwitchToState( MMSTATE_SESSION_DISCONNECTING );
	}
	else if ( m_Session.IsHost() )
	{
		// Host wants to kick a client
		for ( int i = 0; i < m_Remote.Count(); ++i )
		{
			CClientInfo *pClient = m_Remote[i];
			for ( int j = 0; j < pClient->m_cPlayers; ++j )
			{
				if ( pClient->m_xuids[j] == id )
				{
					ClientInfoToNetMessage( &droppedPlayer, pClient );
					droppedPlayer.m_cPlayers = 0;
					SendMessage( &droppedPlayer, pClient );
					return;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add players to the session
//-----------------------------------------------------------------------------
void CMatchmaking::AddPlayersToSession( CClientInfo *pClient )
{
	bool bIsLocal = false;

	if ( &m_Local == pClient )
	{
		m_Session.JoinLocal( pClient );
		bIsLocal = true;
	}
	else
	{
		m_Session.JoinRemote( pClient );
	}

#if defined ( _X360 )
	if ( Audio_GetXVoice() )
	{
		Audio_GetXVoice()->AddPlayerToVoiceList( pClient, bIsLocal );
		if ( bIsLocal )
		{
			m_bCreatedLocalTalker = true;
		}
	}
#endif
	UpdateMuteList();
}

//-----------------------------------------------------------------------------
// Purpose: Remove players from the session
//-----------------------------------------------------------------------------
void CMatchmaking::RemovePlayersFromSession( CClientInfo *pClient )
{
	if ( !pClient->m_cPlayers )
		return;

	bool bIsLocal = false;

	if ( &m_Local == pClient )
	{
		m_Session.RemoveLocal( pClient );
		bIsLocal = true;
	}
	else
	{
		m_Session.RemoveRemote( pClient );
	}

#if defined ( _X360 )
	if ( Audio_GetXVoice() )
	{
		Audio_GetXVoice()->RemovePlayerFromVoiceList( pClient, bIsLocal );
	}
#endif
	UpdateMuteList();
}

//-----------------------------------------------------------------------------
// Purpose: Check if a client is muted
//-----------------------------------------------------------------------------
bool CMatchmaking::IsPlayerMuted( int iUserId, XUID playerId )
{
	for ( int i = 0; i < MAX_PLAYERS; ++i )
	{
		if ( m_Mutelist[iUserId][i] == playerId )
		{
			return true;
		}
	}
	if ( m_MutedBy[iUserId].HasElement( playerId ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Determine which clients should be muted for the local client
//-----------------------------------------------------------------------------
void CMatchmaking::UpdateMuteList()
{
	// Process our mute list
	MM_Mutelist msg;
	GenerateMutelist( &msg );

	// Send that to everyone else
	if ( !m_Session.IsHost() )
	{
		SendMessage( &msg, &m_Host );
	}
	else
	{
		ProcessMutelist( &msg );
	}
}

void Con_PrintTalkers( const CCommand &args )
{
	g_pMatchmaking->PrintVoiceStatus();
}

void CMatchmaking::PrintVoiceStatus( void )
{
#ifdef _X360
	int iRemoteClient = 0;
	int numRemoteTalkers;
	XUID remoteTalkers[MAX_PLAYERS];
	Audio_GetXVoice()->GetRemoteTalkers( &numRemoteTalkers, remoteTalkers );

	CClientInfo *pClient = &m_Local;

	if ( m_Session.IsHost() == false )
	{
		pClient = &m_Host;
	}


	Msg( "Num Remote Talkers: %d\n", numRemoteTalkers );

	bool bFound = false;

	while ( pClient )
	{
		if ( pClient != &m_Local )
		{
			for ( int iRemote = 0; iRemote < numRemoteTalkers; iRemote++ )
			{
				if ( pClient->m_xuids[0] == remoteTalkers[iRemote] )
				{
					bFound = true;
					break;
				}
			}

			if ( bFound == true )
			{
				Msg( "Found a Talker: %s\n", pClient->m_szGamertags[0] );
			}
			else
			{
				Msg( "ALERT!!! %s not in Talker list\n", pClient->m_szGamertags[0] );
			}
		}

		if ( iRemoteClient < m_Remote.Count() )
		{
			pClient = m_Remote[iRemoteClient];
			iRemoteClient++;
		}
		else
		{
			pClient = NULL;
		}
	}

#endif

}

static ConCommand voice_printtalkers( "voice_printtalkers", Con_PrintTalkers, "voice debug.", FCVAR_DONTRECORD );

//-----------------------------------------------------------------------------
// Purpose: Update a client's mute list
//-----------------------------------------------------------------------------
void CMatchmaking::GenerateMutelist( MM_Mutelist *pMsg )
{
#if defined( _X360 )
	// Get our remote talker list
	if ( !Audio_GetXVoice() )
		return;

	int numRemoteTalkers;
	XUID remoteTalkers[MAX_PLAYERS];
	Audio_GetXVoice()->GetRemoteTalkers( &numRemoteTalkers, remoteTalkers );

	pMsg->m_cPlayers = 0;

	// Loop through local players and update mutes
	for ( int iLocal = 0; iLocal < m_Local.m_cPlayers; ++iLocal )
	{	
		pMsg->m_cMuted[iLocal] = 0;

		pMsg->m_xuid[pMsg->m_cPlayers] = m_Local.m_xuids[iLocal];

		for ( int iRemote = 0; iRemote < numRemoteTalkers; ++iRemote )
		{
			BOOL bIsMuted = false;

			DWORD ret = XUserMuteListQuery( m_Local.m_iControllers[iLocal], remoteTalkers[iRemote], &bIsMuted );
			if( ERROR_SUCCESS != ret )
			{
				Warning( "Warning: XUserMuteListQuery() returned 0x%08x for user %d\n", ret, iLocal );
			}

			if( bIsMuted )
			{
				pMsg->m_Muted[pMsg->m_cPlayers].AddToTail( remoteTalkers[iRemote ] );
				++pMsg->m_cMuted[pMsg->m_cPlayers];
			}
			else
			{
				bIsMuted = m_MutedBy[iLocal].HasElement( remoteTalkers[iRemote] );
			}

			if( bIsMuted )
			{
				Audio_GetXVoice()->SetPlaybackPriority( remoteTalkers[iRemote], m_Local.m_iControllers[iLocal], XHV_PLAYBACK_PRIORITY_NEVER );
			}
			else
			{
				Audio_GetXVoice()->SetPlaybackPriority( remoteTalkers[iRemote], m_Local.m_iControllers[iLocal], XHV_PLAYBACK_PRIORITY_MAX );
			}
		}

		pMsg->m_cRemoteTalkers[pMsg->m_cPlayers] = m_MutedBy[pMsg->m_cPlayers].Count();
		++pMsg->m_cPlayers;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Handle the mutelist from another client
//-----------------------------------------------------------------------------
bool CMatchmaking::ProcessMutelist( MM_Mutelist *pMsg )
{
#if defined( _X360 )
	// local players
	for( int i = 0; i < m_Local.m_cPlayers; ++i )
	{
		// remote players
		for( int j = 0; j < pMsg->m_cPlayers; ++j )
		{
			m_MutedBy[i].FindAndRemove( pMsg->m_xuid[j] );

			// players muted by remote player
			for( int k = 0; k < pMsg->m_cMuted[j]; ++k )
			{
				if( m_Local.m_xuids[i] == pMsg->m_Muted[j][k] )
				{
					m_MutedBy[i].AddToTail( pMsg->m_xuid[j] );
				}
			}

			BOOL bIsMuted = m_MutedBy[i].HasElement( pMsg->m_xuid[j] );
			if ( !bIsMuted )
			{
				XUserMuteListQuery( m_Local.m_iControllers[i], pMsg->m_xuid[j], &bIsMuted );
			}
			if( bIsMuted )
			{
				Audio_GetXVoice()->SetPlaybackPriority( pMsg->m_xuid[j], m_Local.m_iControllers[i], XHV_PLAYBACK_PRIORITY_NEVER );
			}
			else
			{
				Audio_GetXVoice()->SetPlaybackPriority( pMsg->m_xuid[j], m_Local.m_iControllers[i], XHV_PLAYBACK_PRIORITY_MAX );
			}
		}
	}

	if ( m_Session.IsHost() )
	{
		// Pass this along to everyone else
		SendToRemoteClients( pMsg );
	}

#endif
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: A client is changing to another team
//-----------------------------------------------------------------------------
void CMatchmaking::ChangeTeam( const char *pTeamName )
{
	if ( !pTeamName )
	{
		// Automatic switch to next team
		if ( m_Session.IsHost() )
		{
			if ( m_CurrentState == MMSTATE_ACCEPTING_CONNECTIONS )
			{
				// Put ourselves on another team and
				// tell the other players
				SwitchToNextOpenTeam( &m_Local );
			}
		}
		else
		{
			// Send a request to the host
			MM_Checkpoint msg;
			msg.m_Checkpoint = MM_Checkpoint::CHECKPOINT_CHANGETEAM;
			SendMessage( &msg, &m_Host );
		}
	}
	else
	{
		// Find a team name that matches, and tell everyone our new team number
		char szTeamName[32];
		uint id = g_ClientDLL->GetPresenceID( "PROPERTY_TEAM" );

		for ( int iTeam = 0; iTeam < m_nTotalTeams; ++iTeam )
		{
			g_ClientDLL->GetPropertyDisplayString( id, iTeam, szTeamName, sizeof( szTeamName ) );
			if ( !Q_stricmp( szTeamName, pTeamName ) )
			{
				bool bChanged = false;
				MM_ClientInfo info;
				ClientInfoToNetMessage( &info, &m_Local );
				for ( int i = 0; i < m_Local.m_cPlayers; ++i )
				{
					if ( info.m_iTeam[i] != iTeam )
					{
						bChanged = true;
					}
					info.m_iTeam[i] = iTeam;
				}

				if ( !bChanged )
				{
					return;
				}

				if ( m_Session.IsHost() )
				{
					ProcessClientInfo( &info );
				}
				else
				{
					SendMessage( &info, &m_Host );
				}
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle various matchmaking checkpoint messages
//-----------------------------------------------------------------------------
bool CMatchmaking::ProcessCheckpoint( MM_Checkpoint *pMsg )
{
	switch( pMsg->m_Checkpoint )
	{
	case MM_Checkpoint::CHECKPOINT_CHANGETEAM:
		if ( m_Session.IsHost() )
		{
			// if the countdown has started, deny
			if ( m_CurrentState == MMSTATE_ACCEPTING_CONNECTIONS )
			{
				netadr_t adr = pMsg->GetNetChannel()->GetRemoteAddress();
				CClientInfo *pClient = FindClient( &adr );
				if ( pClient )
				{
					SwitchToNextOpenTeam( pClient );
				}
			}
		}	
		break;

	case MM_Checkpoint::CHECKPOINT_PREGAME:

		if ( m_Session.IsArbitrated() && !m_Session.IsHost() )
		{
			m_Session.RegisterForArbitration();
		}

		// Start the countdown timer to map load
		SwitchToState( MMSTATE_PREGAME );
		break;

	case MM_Checkpoint::CHECKPOINT_GAME_LOBBY:
		
		// returning to game lobby, pregame canceled
		// reset the countdown
		SessionNotification( SESSION_NOTIFY_COUNTDOWN, -1 );
		if ( m_Session.IsHost() )
		{
			SwitchToState( MMSTATE_ACCEPTING_CONNECTIONS );
		}
		else
		{
			SwitchToState( MMSTATE_SESSION_CONNECTED );
		}
		break;

	case MM_Checkpoint::CHECKPOINT_LOADING_COMPLETE:

		if ( m_Session.IsHost() )
		{
			// Mark this client as loaded
			netadr_t adr = pMsg->GetNetChannel()->GetRemoteAddress();
			CClientInfo *pClient = FindClient( &adr );
			if ( pClient )
			{
				pClient->m_bLoaded = true;

				if ( GameIsActive() )
				{
					// Send a reply and reset the netchannel timeout
					SendMessage( pMsg, pClient );
					SetChannelTimeout( &pClient->m_adr, HEARTBEAT_TIMEOUT );
				}
			}
		}
		else
		{
			// The host is also loaded, so reset the netchannel timeout
			SetChannelTimeout( &m_Host.m_adr, HEARTBEAT_TIMEOUT );
		}
		break;

	case MM_Checkpoint::CHECKPOINT_CONNECT:

		// If we're already connected or in the game, don't connect again.
		if ( m_CurrentState == MMSTATE_CONNECTED_TO_SERVER || 
			 m_CurrentState == MMSTATE_INGAME )
		{
			break;
		}

		if ( m_Session.IsHost() )
		{
			// Send the message to everyone
			SendToRemoteClients( pMsg );
		}
		else
		{
			// The host is asking us to connect to the game server
			if ( m_CurrentState != MMSTATE_LOADING )
			{
				// Set the longer timeout for loading
				SetChannelTimeout( &m_Host.m_adr, HEARTBEAT_TIMEOUT_LOADING );
				SwitchToState( MMSTATE_LOADING );
			}
		}

		// Make sure we're not preventing full startup
		SetPreventFullServerStartup( false, "CHECKPOINT_CONNECT\n" );

		if ( !IsServer() )
		{
			char cmd[MAX_PATH];
			Q_snprintf( cmd, sizeof( cmd ), "connect %d.%d.%d.%d", m_Host.m_adr.ip[0], m_Host.m_adr.ip[1], m_Host.m_adr.ip[2], m_Host.m_adr.ip[3] );		
			Cbuf_AddText( cmd );

			SessionNotification( SESSION_NOTIFY_CONNECTED_TOSERVER );
		}
		break;

	case MM_Checkpoint::CHECKPOINT_SESSION_DISCONNECT:

		PerformDisconnect();
		break;

	case MM_Checkpoint::CHECKPOINT_REPORT_STATS:

		// Start stats reporting
		g_ClientDLL->StartStatsReporting( m_Session.GetSessionHandle(), m_Session.IsArbitrated() );
		m_Local.m_bReportedStats = true;

		m_fHeartbeatInterval = HEARTBEAT_INTERVAL_SHORT;

		SwitchToState( MMSTATE_REPORTING_STATS );

		// Host needs to wait for clients to finish reporting
		if ( !m_Session.IsHost() )
		{
			EndStatsReporting();
		}
		break;

	case MM_Checkpoint::CHECKPOINT_REPORTING_COMPLETE:
		{
			// Mark this client as finished reporting stats
			bool bFinished = false;
			netadr_t adr = pMsg->GetNetChannel()->GetRemoteAddress();
			for ( int i = 0; i < m_Remote.Count(); ++i )
			{
				CClientInfo *pClient = m_Remote[i];
				if ( pClient->m_adr.CompareAdr( adr ) )
				{
					pClient->m_bReportedStats = true;
				}

				if ( !pClient->m_bReportedStats )
				{
					bFinished = false;
				}
			}

			if ( bFinished && m_Local.m_bReportedStats )
			{
				EndStatsReporting();	
			}
		}
		break;

	case MM_Checkpoint::CHECKPOINT_POSTGAME:

		g_pXboxSystem->SessionEnd( m_Session.GetSessionHandle(), false );

		engineClient->ClientCmd( "disconnect" );

		EngineVGui()->ActivateGameUI();

		if ( m_Session.IsArbitrated() )
		{
			// Tell gameui to return to the main menu
			SessionNotification( SESSION_NOTIFY_ENDGAME_RANKED );

			SwitchToState( MMSTATE_INITIAL );
		}
		else
		{
			if ( m_Session.IsHost() )
			{
				// Make ourselves available to queries again
				m_HostData.gameState = GAMESTATE_INLOBBY;
				UpdateSessionReplyData( XNET_QOS_LISTEN_ENABLE|XNET_QOS_LISTEN_SET_DATA );
			}

			// Tell gameui to activate the lobby
			SessionNotification( m_Session.IsHost() ? SESSION_NOTIFY_ENDGAME_HOST : SESSION_NOTIFY_ENDGAME_CLIENT );

			// Fill the lobby with all of the clients
			if ( !m_Session.IsHost() )
			{
				SendPlayerInfoToLobby( &m_Host, m_nHostOwnerId );
				SendPlayerInfoToLobby( &m_Local );
			}
			else
			{
				SendPlayerInfoToLobby( &m_Local, 0 );
			}

			for ( int i = 0; i < m_Remote.Count(); ++i )
			{
				SendPlayerInfoToLobby( m_Remote[i] );
			}

			if ( m_Session.IsHost() )
			{
				SwitchToState( MMSTATE_ACCEPTING_CONNECTIONS );
			}
			else
			{
				SwitchToState( MMSTATE_SESSION_CONNECTED );
			}
		}
		break;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Stop any asynchronous operation that is currently running
//-----------------------------------------------------------------------------
void CMatchmaking::CancelCurrentOperation()
{
	switch( m_CurrentState )
	{
	case MMSTATE_CREATING:
		m_Session.CancelCreateSession();
		break;

	case MMSTATE_SEARCHING:
		CancelSearch();
		break;

	case MMSTATE_WAITING_QOS:
		CancelQosLookup();
		break;

	case MMSTATE_SESSION_CONNECTING:
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Send player info to be displayed in the session lobby
//-----------------------------------------------------------------------------
void CMatchmaking::SendPlayerInfoToLobby( CClientInfo *pClient, int iHostIdx )
{
	for ( int i = 0; i < pClient->m_cPlayers; ++i )
	{
		EngineVGui()->UpdatePlayerInfo( pClient->m_xuids[i], pClient->m_szGamertags[i], pClient->m_iTeam[i], pClient->m_cVoiceState[i], GetPlayersNeeded(), iHostIdx == i );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update the start game countdown timer
//-----------------------------------------------------------------------------
void CMatchmaking::UpdatePregame()
{
	int elapsedTime = GetTime() - m_fCountdownStartTime;
	if ( elapsedTime > REGISTRATION_MAXWAITTIME )
	{
		// Check the registration timer.
		if ( m_Session.IsHost() && m_Session.IsArbitrated() && !m_Local.m_bRegistered )
		{
			// Time's up, register ourselves
			m_Local.m_bRegistered = true;
			m_Session.RegisterForArbitration();
		}
	}

	// Check the countdown timer.  When it's zero, start the game.
	if ( elapsedTime < STARTGAME_COUNTDOWN )
	{
		SessionNotification( SESSION_NOTIFY_COUNTDOWN, STARTGAME_COUNTDOWN - elapsedTime );
		return;
	}

	// Send the zero count
	SessionNotification( SESSION_NOTIFY_COUNTDOWN, 0 );

	// Set a longer timeout for loading
	if ( m_Session.IsHost() )
	{
		for ( int i = 0; i < m_Remote.Count(); ++i )
		{
			SetChannelTimeout( &m_Remote[i]->m_adr, HEARTBEAT_TIMEOUT_LOADING );
		}

		// Set commentary & cheats off
		ConVarRef commentary( "commentary" );
		commentary.SetValue( false );
		ConVarRef sv_cheats( "sv_cheats" );
		sv_cheats.SetValue( 0 );
	}
	else
	{
		SetChannelTimeout( &m_Host.m_adr, HEARTBEAT_TIMEOUT_LOADING );
	}

	g_pXboxSystem->SessionStart( m_Session.GetSessionHandle(), 0, false );

	if ( !IsServer() )
	{
		SetPreventFullServerStartup( true, "SESSION_NOTIFY_COUNTDOWN == 0 and not the host\n" );
	}

	SwitchToState( MMSTATE_LOADING );
}

//-----------------------------------------------------------------------------
// Purpose: Receive notifications from the session
//-----------------------------------------------------------------------------
void CMatchmaking::SessionNotification( const SESSION_NOTIFY notification, const int param )
{
	// Notify GameUI
	EngineVGui()->SessionNotification( notification, param );

	switch( notification )
	{
	case SESSION_NOTIFY_CREATED_HOST:
		m_bEnteredLobby = true;
		OnHostSessionCreated();
		break;

	case SESSION_NOTIFY_CREATED_CLIENT:
		Msg( "Client: CreateSession successful\n" );

		// Session has been created according to the advertised specifications.
		// Now send a connection request to the session host.
		SwitchToState( MMSTATE_SESSION_CONNECTING );
		break;

	case SESSION_NOFIFY_MODIFYING_SESSION:
		SwitchToState( MMSTATE_MODIFYING );
		break;

	case SESSION_NOTIFY_MODIFYING_COMPLETED_HOST:
		SwitchToState( MMSTATE_ACCEPTING_CONNECTIONS );
		break;

	case SESSION_NOTIFY_MODIFYING_COMPLETED_CLIENT:
		SwitchToState( MMSTATE_SESSION_CONNECTED );
		break;

	case SESSION_NOTIFY_SEARCH_COMPLETED:
		// Waiting for the player to choose a session
		SwitchToState( MMSTATE_BROWSING );
		break;

	case SESSION_NOTIFY_CONNECTED_TOSESSION:
		m_bEnteredLobby = true;
		SwitchToState( MMSTATE_SESSION_CONNECTED );
		break;

	case SESSION_NOTIFY_CONNECTED_TOSERVER:
		SwitchToState( MMSTATE_CONNECTED_TO_SERVER );
		break;

	case SESSION_NOTIFY_MIGRATION_COMPLETED:
		if ( !m_Session.IsHost() )
		{
			// Finished
			EndMigration();
		}
		else
		{
			// Get ready to send join requests too peers
			for ( int i = 0; i < m_Remote.Count(); ++i )
			{
				m_Remote[i]->m_bMigrated = false;

				AddRemoteChannel( &m_Remote[i]->m_adr );
			}

			m_nSendCount = 0;
			m_fSendTimer = 0;

			SwitchToState( MMSTATE_HOSTMIGRATE_WAITINGFORCLIENTS );
		}
		break;

	case SESSION_NOTIFY_FAIL_MIGRATE:
		// X360TBD: How to handle this error?
		Warning( "Migrate Failed\n" );
		break;

	case SESSION_NOTIFY_REGISTER_COMPLETED:
		if( !m_Session.IsHost() )
		{
			// Tell the host we're registered
			MM_RegisterResponse msg;
			SendMessage( &msg, &m_Host.m_adr );
		}
		else
		{
			// Process the results of registration
			ProcessRegistrationResults();
		}
		break;

	case SESSION_NOTIFY_ENDGAME_HOST:
	case SESSION_NOTIFY_ENDGAME_CLIENT:
		m_fHeartbeatInterval = HEARTBEAT_INTERVAL_SHORT;
		break;

	case SESSION_NOTIFY_LOST_HOST:
	case SESSION_NOTIFY_LOST_SERVER:
		SwitchToState( MMSTATE_SESSION_DISCONNECTING );
		break;

	case SESSION_NOTIFY_FAIL_REGISTER:
	case SESSION_NOTIFY_FAIL_CREATE:
	case SESSION_NOTIFY_FAIL_SEARCH:
	case SESSION_NOTIFY_CONNECT_SESSIONFULL:
	case SESSION_NOTIFY_CONNECT_NOTAVAILABLE:
	case SESSION_NOTIFY_CONNECT_FAILED:

		// Reset the session
		SwitchToState( MMSTATE_INITIAL );
		break;

	default:
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Switch between matchmaking states
//-----------------------------------------------------------------------------
void CMatchmaking::SwitchToState( int newState )
{
	// Clean up from the previous state
	switch( m_CurrentState )
	{
	case MMSTATE_INITIAL:
		break;

	case MMSTATE_BROWSING:
		// Clean up the search results
		ClearSearchResults();
		break;

	default:
		break;
	}

	// Initialize the next state
	switch( newState )
	{
	case MMSTATE_INITIAL:
		m_bCleanup = true;
		break;

	case MMSTATE_CREATING:
	case MMSTATE_SEARCHING:
	case MMSTATE_ACCEPTING_CONNECTIONS:
		break;

	case MMSTATE_MODIFYING:
		m_fSendTimer = GetTime();
		break;

	case MMSTATE_WAITING_QOS:
		m_fWaitTimer = GetTime();
		break;

	case MMSTATE_SESSION_CONNECTING:
		ConnectToHost();
		break;

	case MMSTATE_PREGAME:
		m_fCountdownStartTime = GetTime();
		break;

	case MMSTATE_LOADING:
		m_fHeartbeatInterval = HEARTBEAT_INTERVAL_LONG;
		break;

	case MMSTATE_SESSION_DISCONNECTING:
		m_fWaitTimer = GetTime();
		break;

	case MMSTATE_REPORTING_STATS:
		m_fWaitTimer = GetTime();
		break;

	default:
		break;
	}

	m_CurrentState = newState;
}

void CMatchmaking::UpdateVoiceStatus( void )
{
#if defined( _X360 )

	if ( m_flVoiceBlinkTime > GetTime() )
		return;

	m_flVoiceBlinkTime = GetTime() + VOICE_ICON_BLINK_TIME;
	CClientInfo *pClient = &m_Local;

	bool bIsHost = m_Session.IsHost();
	bool bShouldSendInfo = false;

	if ( pClient )
	{
		for ( int i = 0; i < pClient->m_cPlayers; ++i )
		{
			if ( pClient->m_xuids[i] == 0 )
				continue;

			byte cOldVoiceState = pClient->m_cVoiceState[i];

			if ( Audio_GetXVoice()->IsHeadsetPresent( pClient->m_iControllers[i] ) == false )
			{
				pClient->m_cVoiceState[i] = VOICE_STATUS_OFF;
			}
			else
			{
				if ( Audio_GetXVoice()->IsPlayerTalking( pClient->m_xuids[i], true ) )
				{
					pClient->m_cVoiceState[i] = VOICE_STATUS_TALKING;
				}
				else
				{
					pClient->m_cVoiceState[i] = VOICE_STATUS_IDLE;
				}
			}

			if ( cOldVoiceState != pClient->m_cVoiceState[i] )
			{
				bShouldSendInfo = true;
			}

			if ( bShouldSendInfo == true )
			{
				EngineVGui()->UpdatePlayerInfo( pClient->m_xuids[i], pClient->m_szGamertags[i], pClient->m_iTeam[i], pClient->m_cVoiceState[i], GetPlayersNeeded(), bIsHost );
			}
		}


		if ( bShouldSendInfo )
		{
			MM_ClientInfo info;
			ClientInfoToNetMessage( &info, pClient );
		
			if ( bIsHost == true )
			{
				// Tell all the clients
				ProcessClientInfo( &info );
			}
			else
			{
				// Tell all the clients
				SendMessage( &info, &m_Host );
			}
		}
	}

#endif

}

//-----------------------------------------------------------------------------
//	Update matchmaking and any active session 
//-----------------------------------------------------------------------------
void CMatchmaking::RunFrame()
{
	if ( !m_bInitialized )
	{
		RunFrameInvite();
		return;
	}

	if ( NET_IsMultiplayer() )
	{
		NET_ProcessSocket( NS_MATCHMAKING, this );

		if ( m_Session.IsSystemLink() )
		{
			NET_ProcessSocket( NS_SYSTEMLINK, this );
		}
	}

#if defined( _X360 )
	if ( Audio_GetXVoice() != NULL )
	{
		if ( !GameIsActive() )
		{
			if ( Audio_GetXVoice()->VoiceUpdateData() == true )
			{
				CLC_VoiceData voice;
				Audio_GetXVoice()->GetVoiceData( &voice );

				if ( m_Session.IsHost() )
				{
					// Send this message on to everyone else
					SendToRemoteClients( &voice, true );
				}
				else
				{
					// Send to the host
					SendMessage( &voice, &m_Host );
				}

				Audio_GetXVoice()->VoiceResetLocalData();
			}

			UpdateVoiceStatus();
		}
	}
#endif

	// Tell the session to run its update
	m_Session.RunFrame();

	// Check state:
	switch( m_CurrentState )
	{
	case MMSTATE_CREATING:
		// Waiting for success or failure from CreateSession()
		// GameUI is displaying a "Creating Game" dialog.
		break;

	case MMSTATE_ACCEPTING_CONNECTIONS:
		// Host is sitting in the Lobby waiting for connection requests. Once the game
		// is full enough (player count >= min players) the host will be able to start the game.
		UpdateAcceptingConnections();
		break;

	case MMSTATE_SEARCHING:
		UpdateSearch();
		break;

	case MMSTATE_WAITING_QOS:
		UpdateQosLookup();
		break;

	case MMSTATE_SESSION_CONNECTING:
		UpdateConnecting();
		break;

	case MMSTATE_PREGAME:
		UpdatePregame();
		break;

	case MMSTATE_MODIFYING:
		UpdateSessionModify();
		break;

	case MMSTATE_HOSTMIGRATE_WAITINGFORCLIENTS:
		if ( GetTime() - m_fSendTimer > HOSTMIGRATION_RETRYINTERVAL )
		{
			if ( m_nSendCount > HOSTMIGRATION_MAXRETRIES )
			{
				EndMigration();
			}
			else
			{
				TellClientsToMigrate();
			}
		}
		break;

	case MMSTATE_HOSTMIGRATE_WAITINGFORHOST:
		if ( GetTime() - m_fWaitTimer > HOSTMIGRATION_MAXWAITTIME )
		{
			// Give up on that host and try the next one in the list
			Msg( "Giving up on this host\n" );
			ClientDropped( m_pNewHost );

			StartHostMigration();
		}
		break;

	case MMSTATE_SESSION_DISCONNECTING:
		// Wait for the host reply, or timeout
		if ( GetTime() - m_fWaitTimer > DISCONNECT_WAITTIME )
		{
			PerformDisconnect();
		}
		break;

	case MMSTATE_REPORTING_STATS:
		if ( GetTime() - m_fWaitTimer > REPORTSTATS_WAITTIME )
		{
			EndStatsReporting();
		}
		break;
	}

	CleanupMarkedChannels();
	SendHeartbeat();

	if ( m_bCleanup )
	{
		Cleanup();
	}

	RunFrameInvite();
}

//-----------------------------------------------------------------------------
// Purpose: Let the invite system determine a good moment to start connecting to inviter's host
//-----------------------------------------------------------------------------
void CMatchmaking::RunFrameInvite()
{
	if ( m_InviteState != INVITE_NONE )
	{
		JoinInviteSession( &m_InviteSessionInfo );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get Quality-of-Service with LIVE
//-----------------------------------------------------------------------------
MM_QOS_t CMatchmaking::GetQosWithLIVE()
{
	return MM_GetQos();
}

//-----------------------------------------------------------------------------
// Debugging helpers 
//-----------------------------------------------------------------------------
void CMatchmaking::ShowSessionInfo()
{
	Msg( "[MM] Filled Slots:\n[MM] Public: %d of %d\n[MM] Private: %d of %d\n", 
			m_Session.GetSessionSlots( SLOTS_FILLEDPUBLIC ), 
			m_Session.GetSessionSlots( SLOTS_TOTALPUBLIC ),
			m_Session.GetSessionSlots( SLOTS_FILLEDPRIVATE ),
			m_Session.GetSessionSlots( SLOTS_TOTALPRIVATE ) );

	Msg( "[MM] Current state: %d\n", m_CurrentState );
	Msg( "[MM] Send timer: %f\n", GetTime() - m_fSendTimer );
	Msg( "[MM] Wait timer: %f\n", GetTime() - m_fWaitTimer );
	
	int total = 0;
	for ( int i = 0; i < m_nTotalTeams; ++i )
	{
		total += CountPlayersOnTeam( i );
	}
	Msg( "[MM] Total players: %d\n", total );

	EngineVGui()->SessionNotification( SESSION_NOTIFY_DUMPSTATS );
}

//-----------------------------------------------------------------------------
// Debugging helpers 
//-----------------------------------------------------------------------------
void CMatchmaking::TestSendMessage()
{
	for ( int i = 0; i < m_Remote.Count(); ++i )
	{
		AddRemoteChannel( &m_Remote[i]->m_adr );
	}
	NET_StringCmd msg;
	SendToRemoteClients( &msg );
}

bool CMatchmaking::PreventFullServerStartup()
{
	return m_bPreventFullServerStartup;
}

void CMatchmaking::SetPreventFullServerStartup( bool bState, char const *fmt, ... )
{
	char desc[ 256 ];
	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( desc, sizeof( desc ), fmt, argptr );
	va_end( argptr );

	DevMsg( 1, "Setting state from prevent %s to prevent %s:  %s",
		m_bPreventFullServerStartup ? "yes" : "no",
		bState ? "yes" : "no",
		desc );

	m_bPreventFullServerStartup = bState;
}

CON_COMMAND( mm_session_info, "Dump session information" )
{
	if ( g_pMatchmaking )
	{
		g_pMatchmaking->ShowSessionInfo();
	}
}

CON_COMMAND( mm_message, "Send a message to all remote clients" )
{
	if ( g_pMatchmaking )
	{
		g_pMatchmaking->TestSendMessage();
	}
}

CON_COMMAND( mm_stats, "" )
{
	if ( g_pMatchmaking )
	{
		g_pMatchmaking->TestStats();
	}
}
