//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Handles joining clients together in a matchmaking session before a multiplayer
//			game, tracking new players and dropped players during the game, and reporting
//			game results and stats after the game is complete.
//
//=============================================================================//

#include "vstdlib/random.h"
#include "proto_oob.h"
#include "cdll_engine_int.h"
#include "matchmaking.h"
#include "Session.h"
#include "convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar mm_minplayers( "mm_minplayers", "2", 0, "Number of players required to start an unranked game" );
static ConVar mm_max_spectators( "mm_max_spectators", "4", 0, "Max players allowed on the spectator team" );

//-----------------------------------------------------------------------------
// Purpose: Start a Matchmaking session as the host 
//-----------------------------------------------------------------------------
void CMatchmaking::StartHost( bool bSystemLink )
{
	NET_SetMutiplayer( true );

	InitializeLocalClient( true );

	// Set all of the session contexts and properties. These were filled
	// in by GameUI after the user set them in the game options menu
 	for ( int i = 0; i < m_SessionContexts.Count(); ++i )
 	{
 		XUSER_CONTEXT &ctx = m_SessionContexts[i];
 		m_Session.SetContext( ctx.dwContextId, ctx.dwValue, false );
 	}

	for ( int i = 0; i < m_SessionProperties.Count(); ++i )
	{
		XUSER_PROPERTY &prop = m_SessionProperties[i];
		m_Session.SetProperty( prop.dwPropertyId, prop.value.type, &prop.value.nData, false );
	}

	m_Session.SetSessionSlots( SLOTS_TOTALPUBLIC, m_nGameSize - m_nPrivateSlots );
	m_Session.SetSessionSlots( SLOTS_TOTALPRIVATE, m_nPrivateSlots );

	m_Session.SetIsSystemLink( bSystemLink );
	m_Session.SetIsHost( true );
	m_Session.SetOwnerId( XBX_GetPrimaryUserId() );

	// Session creation is asynchronous
	if ( !m_Session.CreateSession() )
	{
		SessionNotification( SESSION_NOTIFY_FAIL_CREATE );
		return;
	}

	// Waiting for session creation results
	SwitchToState( MMSTATE_CREATING );
}

//-----------------------------------------------------------------------------
// Purpose: Successfully created the session
//-----------------------------------------------------------------------------
void CMatchmaking::OnHostSessionCreated()
{
	Msg( "Host: CreateSession successful\n" );

	// Setup the game info that will be sent back to searching clients
	m_HostData.gameState = GAMESTATE_INLOBBY;
	KeyValues *pScenario = m_pSessionKeys->FindKey( "CONTEXT_SCENARIO" );
	if ( pScenario )
	{
		Q_strncpy( m_HostData.scenario, pScenario->GetString( "displaystring", "Unknown" ), sizeof( m_HostData.scenario ) );
	}
	KeyValues *pTime = m_pSessionKeys->FindKey( "PROPERTY_MAX_GAME_TIME" );
	if ( pTime )
	{
		m_HostData.gameTime = pTime->GetInt( "valuestring", 0 );
	}
	UpdateSessionReplyData( XNET_QOS_LISTEN_ENABLE|XNET_QOS_LISTEN_SET_DATA );

	int iTeam = ChooseTeam();
	for ( int i = 0; i < m_Local.m_cPlayers; ++i )
	{
		m_Local.m_iTeam[i] = iTeam;
	}

	AddPlayersToSession( &m_Local );
	SendPlayerInfoToLobby( &m_Local, 0 );

	// Send session properties to client.dll so it can properly
	// set up game rules, cvars, etc.
	g_ClientDLL->SetupGameProperties( m_SessionContexts, m_SessionProperties );

	// Session has been created and advertised. Start listening for connection requests.
	SwitchToState( MMSTATE_ACCEPTING_CONNECTIONS );
}

//-----------------------------------------------------------------------------
// Purpose: Handle queries for system link servers
//-----------------------------------------------------------------------------
void CMatchmaking::HandleSystemLinkSearch( netpacket_t *pPacket )
{
	// Should we respond to this probe?
	if ( !m_Session.IsSystemLink() || 
		 !m_Session.IsHost() ||
		  m_Session.IsFull() ||
		  m_CurrentState < MMSTATE_ACCEPTING_CONNECTIONS )
	{
		return;
	}

	uint64 nonce = pPacket->message.ReadLongLong();

	// Send back info about our session
	XSESSION_INFO info;
	m_Session.GetSessionInfo( &info );

	char msg_buffer[MAX_ROUTABLE_PAYLOAD];
	bf_write msg( msg_buffer, sizeof(msg_buffer) );

	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteByte( HTP_SYSTEMLINK_REPLY );
	msg.WriteLongLong( nonce );
	msg.WriteBytes( &info, sizeof( info ) );
	msg.WriteByte( m_Session.GetSessionSlots( SLOTS_TOTALPUBLIC ) - m_Session.GetSessionSlots( SLOTS_FILLEDPUBLIC ) );
	msg.WriteByte( m_Session.GetSessionSlots( SLOTS_TOTALPRIVATE ) - m_Session.GetSessionSlots( SLOTS_FILLEDPRIVATE ) );
	msg.WriteByte( m_Session.GetSessionSlots( SLOTS_FILLEDPUBLIC ) );
	msg.WriteByte( m_Session.GetSessionSlots( SLOTS_FILLEDPRIVATE ) );
	msg.WriteByte( m_nTotalTeams );
	msg.WriteByte( m_HostData.gameState );
	msg.WriteByte( m_HostData.gameTime );
	msg.WriteBytes( m_Local.m_szGamertags[0], MAX_PLAYER_NAME_LENGTH );
	msg.WriteBytes( m_HostData.scenario, MAX_MAP_NAME );
	msg.WriteByte( m_SessionProperties.Count() );
	msg.WriteByte( m_SessionContexts.Count() );

	uint nScenarioId = g_ClientDLL->GetPresenceID( "CONTEXT_SCENARIO" );	
	uint nScenarioValue = 0;

	for ( int i = 0; i < m_SessionProperties.Count(); ++i )
	{
		XUSER_PROPERTY &prop = m_SessionProperties[i];
		msg.WriteBytes( &prop, sizeof( prop ) );
	}
	for ( int i = 0; i < m_SessionContexts.Count(); ++i )
	{
		XUSER_CONTEXT &ctx = m_SessionContexts[i];
		msg.WriteBytes( &ctx, sizeof( ctx ) );

		// Get the scenario id so the correct info can be displayed in the session browser
		if ( ctx.dwContextId == nScenarioId )
		{
			nScenarioValue = ctx.dwValue;
		}
	}
	msg.WriteByte( nScenarioValue );
	msg.WriteLongLong( m_HostData.xuid );

	netadr_t adr;
	adr.SetType( NA_BROADCAST );
	adr.SetPort( PORT_SYSTEMLINK );

	// Send message
	NET_SendPacket( NULL, NS_SYSTEMLINK, adr, msg.GetData(), msg.GetNumBytesWritten() );
}

//-----------------------------------------------------------------------------
// Purpose: Process a client request to join the matchmaking session. If the
//			request is accepted, transmit session info to the new client, and
//			notify all connected clients of the new addition.
//-----------------------------------------------------------------------------
void CMatchmaking::HandleJoinRequest( netpacket_t *pPacket )
{
	MM_JoinResponse joinResponse;
	CClientInfo tempClient;

	Msg( "Received a join request\n" );

	// Check the state
	if ( !IsAcceptingConnections() )
	{
		Msg( "State %d: Not accepting connections.\n", m_CurrentState );
		joinResponse.m_ResponseType = joinResponse.JOINRESPONSE_NOTHOSTING;
	}
	else
	{
		// Extract some packet data to see if this client was invited
		tempClient.m_id			= pPacket->message.ReadLongLong();	// 64 bit
		tempClient.m_cPlayers	= pPacket->message.ReadByte();
		tempClient.m_bInvited	= (pPacket->message.ReadOneBit() != 0);

		for ( int i = 0; i < m_Remote.Count(); i++ )
		{
			CClientInfo *pClient = m_Remote[i];

			if ( pClient )
			{
				if ( pClient->m_id == tempClient.m_id )
				{
					ClientDropped( pClient );
					break;
				}
			}
		}

		// Make sure there's room for new players
		int nSlotsOpen = m_Session.GetSessionSlots( SLOTS_TOTALPUBLIC ) - m_Session.GetSessionSlots( SLOTS_FILLEDPUBLIC );
		if ( tempClient.m_bInvited )
		{
			// Only invited clients can take private slots
			nSlotsOpen += m_Session.GetSessionSlots( SLOTS_TOTALPRIVATE ) - m_Session.GetSessionSlots( SLOTS_FILLEDPRIVATE );
		}

		if ( tempClient.m_cPlayers > nSlotsOpen )
		{
			Msg( "Session Full.\n" );
			joinResponse.m_ResponseType = joinResponse.JOINRESPONSE_SESSIONFULL;
		}
		else
		{
			// There is room for the new client - fill out the rest of the response fields
			joinResponse.m_ResponseType		= joinResponse.JOINRESPONSE_APPROVED;
			joinResponse.m_id				= m_Local.m_id;
			joinResponse.m_Nonce			= m_Session.GetSessionNonce();
			joinResponse.m_SessionFlags		= m_Session.GetSessionFlags();
			joinResponse.m_nOwnerId			= XBX_GetPrimaryUserId();
			joinResponse.m_nTotalTeams		= m_nTotalTeams;
			joinResponse.m_ContextCount		= m_SessionContexts.Count();
			joinResponse.m_PropertyCount	= m_SessionProperties.Count();

			for ( int i = 0; i < m_SessionContexts.Count(); ++i )
			{
				joinResponse.m_SessionContexts.AddToTail( m_SessionContexts[i] );
			}

			for ( int i = 0; i < m_SessionProperties.Count(); ++i )
			{
				joinResponse.m_SessionProperties.AddToTail( m_SessionProperties[i] );
			}

			if ( !GameIsActive() )
			{
				// If the game is in progress, the new client will choose a team after connecting
				joinResponse.m_iTeam = ChooseTeam(); 
			}
			else
			{
				// Tell the client to join the game in progress
				joinResponse.m_iTeam = -1;
				joinResponse.m_ResponseType = joinResponse.JOINRESPONSE_APPROVED_JOINGAME;
			}
		}
	}

	netadr_t *pFromAdr = &pPacket->from;

	// Create a network channel for this client
	INetChannel *pNetChannel = AddRemoteChannel( pFromAdr );
	if ( !pNetChannel )
	{
		// Handle the error
		return;
	}

	// Send the response
	SendMessage( &joinResponse, pFromAdr );

	if ( joinResponse.m_ResponseType == joinResponse.JOINRESPONSE_APPROVED ||
		joinResponse.m_ResponseType == joinResponse.JOINRESPONSE_APPROVED_JOINGAME )
	{
		Msg( "Join request approved\n" );

		// Create a new client
		CClientInfo *pNewClient = new CClientInfo();

		pNewClient->m_adr		= pPacket->from;
		pNewClient->m_id		= tempClient.m_id;			// 64 bit
		pNewClient->m_cPlayers	= tempClient.m_cPlayers;
		pNewClient->m_bInvited	= tempClient.m_bInvited;
		pPacket->message.ReadBytes( &pNewClient->m_xnaddr, sizeof( pNewClient->m_xnaddr ) );
		for ( int i = 0; i < tempClient.m_cPlayers; ++i )
		{
			pNewClient->m_xuids[i] = pPacket->message.ReadLongLong();	// 64 bit
			pPacket->message.ReadBytes( &pNewClient->m_cVoiceState, sizeof( pNewClient->m_cVoiceState ) );
			pNewClient->m_iTeam[i] = joinResponse.m_iTeam;
			pPacket->message.ReadString( pNewClient->m_szGamertags[i], sizeof( pNewClient->m_szGamertags[i] ), true );
		}

		// Tell everyone about the new client, and vice versa
		MM_ClientInfo newClientInfo;
		ClientInfoToNetMessage( &newClientInfo, pNewClient );

		for ( int i = 0; i < m_Remote.Count(); ++i )
		{
			CClientInfo *pRemote = m_Remote[i];

			MM_ClientInfo oldClientInfo;
			ClientInfoToNetMessage( &oldClientInfo, pRemote );

			SendMessage( &newClientInfo, pRemote );
			SendMessage( &oldClientInfo, pNewClient );
		}

		// Tell new client about the host (us)
		MM_ClientInfo hostInfo;
		ClientInfoToNetMessage( &hostInfo, &m_Local );
		SendMessage( &hostInfo, pNewClient );

		// Tell ourselves about the new client
		ProcessClientInfo( &newClientInfo );

		if ( GameIsActive() )
		{
			// Set a longer timeout for communication loss during loading
			SetChannelTimeout( &pNewClient->m_adr, HEARTBEAT_TIMEOUT_LOADING );

			// Tell the client to connect to the server
			MM_Checkpoint msg;
			msg.m_Checkpoint = MM_Checkpoint::CHECKPOINT_CONNECT;
			SendMessage( &msg, pNewClient );
		}
		else
		{
			// UpdateServerNegotiation();
		}
	}
	else
	{
		// Join request was denied - close the channel
		RemoveRemoteChannel( pFromAdr, "Join request denied" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check the state of the lobby
//-----------------------------------------------------------------------------
void CMatchmaking::UpdateAcceptingConnections()
{
	// Update host status
	UpdateSessionReplyData( XNET_QOS_LISTEN_SET_DATA );

	// Do nothing else
}

//-----------------------------------------------------------------------------
// Purpose: Set the host data that gets sent in replies to client searches
//-----------------------------------------------------------------------------
void CMatchmaking::UpdateSessionReplyData( uint flags )
{
#if defined( _X360 )
	if ( m_Session.GetSessionHandle() == INVALID_HANDLE_VALUE )
		return;

	// Enable listening for client Quality Of Service probes
	XNKID id = m_Session.GetSessionId();
	uint ret = XNetQosListen( &id, (BYTE*)&m_HostData, sizeof( m_HostData ), 0, flags );
	if ( ret )
	{
		Warning( "Failed to update QOS Listener\n" );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Change properties of the current session
//-----------------------------------------------------------------------------
void CMatchmaking::ModifySession()
{
	// Notify all clients of the new settings and wait for replies
	for ( int i = 0; i < m_Remote.Count(); ++i )
	{
		m_Remote[i]->m_bModified = false;
	}

	SendModifySessionMessage();

	SessionNotification( SESSION_NOFIFY_MODIFYING_SESSION );
}

//-----------------------------------------------------------------------------
// Purpose: Send the new session properties to all clients
//-----------------------------------------------------------------------------
void CMatchmaking::SendModifySessionMessage()
{
	MM_JoinResponse msg;
	msg.m_ResponseType = MM_JoinResponse::JOINRESPONSE_MODIFY_SESSION;
	msg.m_ContextCount = m_SessionContexts.Count();
	msg.m_PropertyCount = m_SessionProperties.Count();

	for ( int i = 0; i < m_SessionProperties.Count(); ++i )
	{
		msg.m_SessionProperties.AddToTail( m_SessionProperties[i] );
	}
	for ( int i = 0; i < m_SessionContexts.Count(); ++i )
	{
		msg.m_SessionContexts.AddToTail( m_SessionContexts[i] );
	}

	SendToRemoteClients( &msg );

	// Wait for clients to respond before continuing
	m_fWaitTimer = GetTime();
}

//-----------------------------------------------------------------------------
// Purpose: Waiting for clients to modify their session properties
//-----------------------------------------------------------------------------
void CMatchmaking::UpdateSessionModify()
{
	if ( !m_Session.IsHost() )
		return;

	bool bFinished = true;

	if ( GetTime() - m_fWaitTimer < SESSIONMODIRY_MAXWAITTIME )
	{
		// Check if all clients have finished modifying the session
		for ( int i = 0; i < m_Remote.Count(); ++i )
		{
			if ( !m_Remote[i]->m_bModified )
			{
				bFinished = false;
			}
		}
	}

	if ( bFinished )
	{
		EndSessionModify();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finish session modification
//-----------------------------------------------------------------------------
void CMatchmaking::EndSessionModify()
{
	// Drop any clients that didn't respond
	for ( int i = 0; i < m_Remote.Count(); ++i )
	{
		if ( !m_Remote[i]->m_bModified )
		{
			KickPlayerFromSession( m_Remote[i]->m_id );
		}
	}

	// Send session properties to client.dll so it can properly
	// set up game rules, cvars, etc.
	g_ClientDLL->SetupGameProperties( m_SessionContexts, m_SessionProperties );

	SessionNotification( SESSION_NOTIFY_MODIFYING_COMPLETED_HOST );
}

//-----------------------------------------------------------------------------
// Purpose: Handle a client registration response
//-----------------------------------------------------------------------------
bool CMatchmaking::ProcessRegisterResponse( MM_RegisterResponse *msg )
{
	// Check if all clients have registered
	bool bAllRegistered = true;
	INetChannel *pChannel = msg->GetNetChannel();
	for ( int i = 0; i < m_Remote.Count(); ++i )
	{
		CClientInfo *pClient = m_Remote[i];
		if ( pClient->m_adr.CompareAdr( pChannel->GetRemoteAddress() ) )
		{
			pClient->m_bRegistered = true;
		}

		if ( !pClient->m_bRegistered )
		{
			bAllRegistered = false;
		}
	}

	if ( bAllRegistered )
	{
		// Everyone's registered, register ourselves
		m_Local.m_bRegistered = true;
		m_Session.RegisterForArbitration();
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Find out if all clients registered for arbitration
//-----------------------------------------------------------------------------
void CMatchmaking::ProcessRegistrationResults()
{
	// Clear all the client registration flags
	for ( int i = 0; i < m_Remote.Count(); ++i )
	{
		m_Remote[i]->m_bRegistered = false;
	}

	XSESSION_REGISTRATION_RESULTS *pResults = m_Session.GetRegistrationResults();
	Assert( pResults );

	int numRegistrants = pResults->wNumRegistrants;
	Msg( "%d players registered for arbitration\n", numRegistrants );

	for ( int i = 0; i < numRegistrants; ++i )
	{
		for ( int j = 0; j < m_Remote.Count(); ++j )
		{
			if ( m_Remote[j]->m_id == pResults->rgRegistrants[i].qwMachineID )
			{
				m_Remote[j]->m_bRegistered = true;
			}
		}
	}

	// Now drop any clients that didn't register
	for ( int i = m_Remote.Count()-1; i >= 0; --i )
	{
		if ( !m_Remote[i]->m_bRegistered )
		{
			ClientDropped( m_Remote[i] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: All clients are in the lobby and ready - perform pre-game setup
//-----------------------------------------------------------------------------
bool CMatchmaking::StartGame()
{
	if ( !m_Session.IsHost() )
		return false;

	if ( GetPlayersNeeded() != 0 )
		return false;

	StartCountdown();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: All clients are in the lobby and ready - perform pre-game setup
//-----------------------------------------------------------------------------
bool CMatchmaking::CancelStartGame()
{
	if ( !m_Session.IsHost() )
		return false;

	CancelCountdown();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Start the countdown timer for game start
//-----------------------------------------------------------------------------
void CMatchmaking::StartCountdown()
{
	if ( m_Session.IsArbitrated() )
	{
		// Initialize the client flags
		for ( int i = 0; i < m_Remote.Count(); ++i )
		{
			m_Remote[i]->m_bRegistered = false;
		}
		m_Local.m_bRegistered = false;
	}

	// Block searches while we're loading the game, because we can't reply anyway
	UpdateSessionReplyData( XNET_QOS_LISTEN_DISABLE );

	// Send the start game message to everyone
	MM_Checkpoint msg;
	msg.m_Checkpoint = MM_Checkpoint::CHECKPOINT_PREGAME;
	SendToRemoteClients( &msg );
	ProcessCheckpoint( &msg );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMatchmaking::CancelCountdown()
{
	// Accept searches again
	UpdateSessionReplyData( XNET_QOS_LISTEN_ENABLE|XNET_QOS_LISTEN_SET_DATA );

	MM_Checkpoint msg;
	msg.m_Checkpoint = MM_Checkpoint::CHECKPOINT_GAME_LOBBY;
	SendToRemoteClients( &msg );
	ProcessCheckpoint( &msg );
}

//-----------------------------------------------------------------------------
// Purpose: Tell the clients to connect to the server
//-----------------------------------------------------------------------------
void CMatchmaking::TellClientsToConnect()
{
	if ( IsServer() )
	{
		MM_Checkpoint msg;
		msg.m_Checkpoint = MM_Checkpoint::CHECKPOINT_CONNECT;
		if ( m_Session.IsHost() )
		{
			ProcessCheckpoint( &msg );
		}
		else
		{
			SendMessage( &msg, &m_Host );
		}

		SwitchToState( MMSTATE_CONNECTED_TO_SERVER );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Tell the clients the game is over
//-----------------------------------------------------------------------------
void CMatchmaking::EndGame()
{
	if ( m_Session.IsHost() && GameIsActive() )
	{
		MM_Checkpoint msg;
		if ( !m_Session.IsSystemLink() )
		{
			// Tell clients to report stats to live.
			for ( int i = 0; i < m_Remote.Count(); ++i )
			{
				m_Remote[i]->m_bReportedStats = false;
			}
			m_Local.m_bReportedStats = false;

			msg.m_Checkpoint = MM_Checkpoint::CHECKPOINT_REPORT_STATS;
		}
		else
		{
			msg.m_Checkpoint = MM_Checkpoint::CHECKPOINT_POSTGAME;
		}

		SendToRemoteClients( &msg );
		ProcessCheckpoint( &msg );
	}
}

//-----------------------------------------------------------------------------
// Purpose: End the stats reporting phase
//-----------------------------------------------------------------------------
void CMatchmaking::EndStatsReporting()
{
	if ( m_CurrentState == MMSTATE_REPORTING_STATS )
	{
		if ( !m_Session.IsHost() )
		{
			// Notify the host that we've finished reporting stats
			MM_Checkpoint msg;
			msg.m_Checkpoint = MM_Checkpoint::CHECKPOINT_REPORTING_COMPLETE;
			SendMessage( &msg, &m_Host );
		}
		else
		{
			// Remove anyone who didn't report stats
			// Drop any clients that didn't respond
			for ( int i = 0; i < m_Remote.Count(); ++i )
			{
				if ( !m_Remote[i]->m_bReportedStats )
				{
					KickPlayerFromSession( m_Remote[i]->m_id );
				}
			}

			// Everyone has reported stats
			MM_Checkpoint msg;
			msg.m_Checkpoint = MM_Checkpoint::CHECKPOINT_POSTGAME;
			SendToRemoteClients( &msg );
			ProcessCheckpoint( &msg );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Count players on a given team
//-----------------------------------------------------------------------------
int CMatchmaking::CountPlayersOnTeam( int idxTeam )
{
	int numPlayers = 0;
	for ( int i = 0; i < m_Remote.Count(); ++i )
	{
		if ( !m_Remote[i] )
			continue;

		CClientInfo &ciRemote = *m_Remote[i];

		for ( int jp = 0; jp < ciRemote.m_cPlayers; ++ jp )
		{
			if ( ciRemote.m_iTeam[jp] == idxTeam )
			{
				++ numPlayers;
			}
		}
	}
	for ( int jp = 0; jp < m_Local.m_cPlayers; ++ jp )
	{
		if ( m_Local.m_iTeam[jp] == idxTeam )
		{
			++ numPlayers;
		}
	}
	if ( !m_Session.IsHost() )
	{
		for ( int jp = 0; jp < m_Host.m_cPlayers; ++ jp )
		{
			if ( m_Host.m_iTeam[jp] == idxTeam )
			{
				++ numPlayers;
			}
		}
	}
	return numPlayers;
}

//-----------------------------------------------------------------------------
// Purpose: Auto-assign players as they first enter the lobby
//-----------------------------------------------------------------------------
int CMatchmaking::ChooseTeam()
{
	int iTeam = -1;
	for ( int i = 0; i < m_nTotalTeams - 1; ++i )
	{
		int numI = CountPlayersOnTeam( i ), numIp1 = CountPlayersOnTeam( i + 1 );

		if ( numI < numIp1 )
		{
			iTeam = i;
		}
		else if ( numI > numIp1 )
		{
			iTeam = i + 1;
		}
	}

	if ( iTeam == -1 )
	{
		iTeam = RandomInt( 0, m_nTotalTeams - 1 );
	}

	return iTeam;
}

//-----------------------------------------------------------------------------
// Purpose: Switch this client to the next team with open slots
//-----------------------------------------------------------------------------
void CMatchmaking::SwitchToNextOpenTeam( CClientInfo *pClient )
{
	int oldTeam = pClient->m_iTeam[0];
	int maxPlayersPerTeam = m_nGameSize / m_nTotalTeams + 3;

	// Choose the next team for this client
	int iTeam = oldTeam;
	do
	{
		iTeam = (iTeam + 1) % m_nTotalTeams;
		if ( CountPlayersOnTeam( iTeam ) < maxPlayersPerTeam )
			break;

	} while( iTeam != oldTeam );

	MM_ClientInfo info;
	ClientInfoToNetMessage( &info, pClient );
	for ( int i = 0; i < pClient->m_cPlayers; ++i )
	{
		info.m_iTeam[i] = iTeam;
	}

	// Send to ourselves and everyone else
	ProcessClientInfo( &info );
}

//-----------------------------------------------------------------------------
// Purpose: Check if the teams are full enough to start
//-----------------------------------------------------------------------------
int CMatchmaking::GetPlayersNeeded()
{
	// System link can always be started
	if ( m_Session.IsSystemLink() )
		return 0;

	// check if we can start the game
	int total = 0;
	
	for ( int i = 0; i < m_nTotalTeams; ++i )
	{
		total += CountPlayersOnTeam( i );
	}


	return max( 0, mm_minplayers.GetInt() - max( 0, total ) );
}
