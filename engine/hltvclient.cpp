//========= Copyright Valve Corporation, All rights reserved. ============//
//
// hltvclient.cpp: implementation of the CHLTVClient class.
//
// $NoKeywords: $
//
//===========================================================================//

#include <tier0/vprof.h>
#include "hltvclient.h"
#include "netmessages.h"
#include "hltvserver.h"
#include "framesnapshot.h"
#include "networkstringtable.h"
#include "dt_send_eng.h"
#include "GameEventManager.h"
#include "cmd.h"
#include "ihltvdirector.h"
#include "host.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar tv_maxrate( "tv_maxrate", "8000", 0, "Max SourceTV spectator bandwidth rate allowed, 0 == unlimited" );
static ConVar tv_relaypassword( "tv_relaypassword", "", FCVAR_NOTIFY | FCVAR_PROTECTED | FCVAR_DONTRECORD, "SourceTV password for relay proxies" );
static ConVar tv_chattimelimit( "tv_chattimelimit", "8", 0, "Limits spectators to chat only every n seconds" );
static ConVar tv_chatgroupsize( "tv_chatgroupsize", "0", 0, "Set the default chat group size" );

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CHLTVClient::CHLTVClient(int slot, CBaseServer *pServer)
{
	Clear();

	Assert( hltv == pServer );

	m_nClientSlot = slot;
	m_Server = pServer;
	m_pHLTV = dynamic_cast<CHLTVServer*>(pServer);
	m_nEntityIndex = m_pHLTV->GetHLTVSlot() + 1;
	m_nLastSendTick = 0;
	m_fLastSendTime = 0.0f;
	m_flLastChatTime = 0.0f;
	m_bNoChat = false;

	if ( tv_chatgroupsize.GetInt() > 0  )
	{
		Q_snprintf( m_szChatGroup, sizeof(m_szChatGroup), "group%d", slot%tv_chatgroupsize.GetInt()  );
	}
	else
	{
		Q_strncpy( m_szChatGroup, "all", sizeof(m_szChatGroup) );
	}
}

CHLTVClient::~CHLTVClient()
{

}

bool CHLTVClient::SendSignonData( void )
{
	// check class table CRCs
	if ( m_nSendtableCRC != SendTable_GetCRC() )
	{
		Disconnect( "Server uses different class tables" );
		return false;
	}
	else
	{
		// use your class infos, CRC is correct
		SVC_ClassInfo classmsg( true, m_Server->serverclasses );
		m_NetChannel->SendNetMsg( classmsg );
	}

	 return CBaseClient::SendSignonData();
}

bool CHLTVClient::ProcessClientInfo( CLC_ClientInfo *msg )
{
	if ( !CBaseClient::ProcessClientInfo( msg ) )
		return false;

	return true;
}

bool CHLTVClient::ProcessMove(CLC_Move *msg)
{
	// HLTV clients can't move
	return true;
}

bool CHLTVClient::ProcessListenEvents( CLC_ListenEvents *msg )
{
	// HLTV clients can't subscribe to events, we just send them
	return true;
}

bool CHLTVClient::ProcessRespondCvarValue( CLC_RespondCvarValue *msg )
{
	return true;
}

bool CHLTVClient::ProcessFileCRCCheck( CLC_FileCRCCheck *msg )
{
	return true;
}

bool CHLTVClient::ProcessSaveReplay( CLC_SaveReplay *msg )
{
	return true;
}

bool CHLTVClient::ProcessVoiceData(CLC_VoiceData *msg)
{
	// HLTV clients can't speak
	return true;
}

void CHLTVClient::ConnectionClosing(const char *reason)
{
	Disconnect ( (reason!=NULL)?reason:"Connection closing" );	
}

void CHLTVClient::ConnectionCrashed(const char *reason)
{
	DebuggerBreakIfDebugging_StagingOnly();

	Disconnect ( (reason!=NULL)?reason:"Connection lost" );	
}

void CHLTVClient::PacketStart(int incoming_sequence, int outgoing_acknowledged)
{
	// During connection, only respond if client sends a packet
	m_bReceivedPacket = true; 
}

void CHLTVClient::PacketEnd()
{
	
}

void CHLTVClient::FileRequested(const char *fileName, unsigned int transferID )
{
	DevMsg( "CHLTVClient::FileRequested: %s.\n", fileName );
	m_NetChannel->DenyFile( fileName, transferID );
}

void CHLTVClient::FileDenied(const char *fileName, unsigned int transferID )
{
	DevMsg( "CHLTVClient::FileDenied: %s.\n", fileName );
}

void CHLTVClient::FileReceived( const char *fileName, unsigned int transferID )
{
	DevMsg( "CHLTVClient::FileReceived: %s.\n", fileName );
}

void CHLTVClient::FileSent(const char *fileName, unsigned int transferID )
{
}

CClientFrame *CHLTVClient::GetDeltaFrame( int nTick )
{
	return m_pHLTV->GetDeltaFrame( nTick );
}


bool CHLTVClient::ExecuteStringCommand( const char *pCommandString )
{
	// first let the baseclass handle it
	if ( CBaseClient::ExecuteStringCommand( pCommandString ) )
		return true;

	if ( !pCommandString || !pCommandString[0] )
		return true;

	CCommand args;
	if ( !args.Tokenize( pCommandString ) )
		return true;

	const char *cmd = args[ 0 ];

	if ( !Q_stricmp( cmd, "spec_next" ) || 
		 !Q_stricmp( cmd, "spec_prev" ) ||
		 !Q_stricmp( cmd, "spec_mode" ) )
	{
		ClientPrintf("Camera settings can't be changed during a live broadcast.\n");
		return true;
	}
	
	if ( !Q_stricmp( cmd, "say" ) && args.ArgC() > 1 )
	{
		// if tv_chattimelimit = 0, chat is turned off
		if ( tv_chattimelimit.GetFloat() <= 0 )
			return true;

		if ( (m_flLastChatTime + tv_chattimelimit.GetFloat()) > net_time )
			return true;

		m_flLastChatTime = net_time;

		char chattext[128];

		Q_snprintf( chattext, sizeof(chattext), "%s : %s", GetClientName(), args[1]  );
		
		m_pHLTV->BroadcastLocalChat( chattext, m_szChatGroup );
		
		return true;
	}
	else if ( !Q_strcmp( cmd, "tv_chatgroup" )  )
	{
		if (  args.ArgC() > 1 )
		{
			Q_strncpy( m_szChatGroup, args[1], sizeof(m_szChatGroup) );
		}
		else
		{
			ClientPrintf("Your current chat group is \"%s\"\n", m_szChatGroup );
		}
		return true;
	}
	else if ( !Q_strcmp( cmd, "status" ) )
	{
		int		slots, proxies,	clients;
		char	gd[MAX_OSPATH];
		Q_FileBase( com_gamedir, gd, sizeof( gd ) );
		
		if ( m_pHLTV->IsMasterProxy() )
		{
			ClientPrintf("SourceTV Master \"%s\", delay %.0f\n", 
				m_pHLTV->GetName(),	m_pHLTV->GetDirector()->GetDelay() );
		}
		else // if ( m_Server->IsRelayProxy() )
		{
			if ( m_pHLTV->GetRelayAddress() )
			{
				ClientPrintf("SourceTV Relay \"%s\", connected.\n",
					m_pHLTV->GetName() );
			}
			else
			{
				ClientPrintf("SourceTV Relay \"%s\", not connect.\n", m_pHLTV->GetName() );
			}
		}

		ClientPrintf("IP %s:%i, Online %s, Version %i (%s)\n",
			net_local_adr.ToString( true ), m_pHLTV->GetUDPPort(),
			COM_FormatSeconds( m_pHLTV->GetOnlineTime() ), build_number(),
#ifdef _WIN32
			"Win32" );
#else
			"Linux" );
#endif

		ClientPrintf("Game Time %s, Mod \"%s\", Map \"%s\", Players %i\n", COM_FormatSeconds( m_pHLTV->GetTime() ),
			gd, m_pHLTV->GetMapName(), m_pHLTV->GetNumPlayers() );
		m_pHLTV->GetLocalStats( proxies, slots, clients );

		ClientPrintf("Local Slots %i, Spectators %i, Proxies %i\n", 
			slots, clients-proxies, proxies );

		m_pHLTV->GetGlobalStats( proxies, slots, clients);

		ClientPrintf("Total Slots %i, Spectators %i, Proxies %i\n", 
			slots, clients-proxies, proxies);
	}
	else
	{
		DevMsg( "CHLTVClient::ExecuteStringCommand: Unknown command %s.\n", pCommandString );
	}

	return true;
}

bool CHLTVClient::ShouldSendMessages( void )
{
	if ( !IsActive() )
	{
		// during signon behave like normal client
		return CBaseClient::ShouldSendMessages();
	}

	// HLTV clients use snapshot rate used by HLTV server, not given by HLTV client

	// if the reliable message overflowed, drop the client
	if ( m_NetChannel->IsOverflowed() )
	{
		m_NetChannel->Reset();
		Disconnect ("%s overflowed reliable buffer\n", m_Name );
		return false;
	}

	// send a packet if server has a new tick we didn't already send
	bool bSendMessage = ( m_nLastSendTick != m_Server->m_nTickCount );

	// send a packet at least every 2 seconds
	if ( !bSendMessage && (m_fLastSendTime + 2.0f) < net_time )
	{
		bSendMessage = true;	// force sending a message even if server didn't update
	}

	if ( bSendMessage && !m_NetChannel->CanPacket() )
	{
		// we would like to send a message, but bandwidth isn't available yet
		// in HLTV we don't send choke information, doesn't matter
		bSendMessage = false;
	}

	return bSendMessage;
}

void CHLTVClient::SpawnPlayer( void )
{
	// set view entity

	SVC_SetView setView( m_pHLTV->m_nViewEntity );

	SendNetMsg( setView );

	m_pHLTV->BroadcastLocalTitle( this ); 

	m_flLastChatTime = net_time;

	CBaseClient::SpawnPlayer();
}


void CHLTVClient::SetRate(int nRate, bool bForce )
{
	if ( !bForce )
	{
		if ( m_bIsHLTV )
		{
			// allow higher bandwidth rates for HLTV proxies
			nRate = clamp( nRate, MIN_RATE, MAX_RATE );
		}
		else if ( tv_maxrate.GetInt() > 0 )
		{
			// restrict rate for normal clients to hltv_maxrate
			nRate = clamp( nRate, MIN_RATE, tv_maxrate.GetInt() );
		}
	}

	CBaseClient::SetRate( nRate, bForce );
}

void CHLTVClient::SetUpdateRate(int udpaterate, bool bForce)
{
	// for HLTV clients ignore update rate settings, speed is tv_snapshotrate
	m_fSnapshotInterval = 1.0f / 100.0f;
}

bool CHLTVClient::ProcessSetConVar(NET_SetConVar *msg)
{
	if ( !CBaseClient::ProcessSetConVar( msg ) )
		return false;

	// if this is the first time we get user settings, check password etc
	if ( m_nSignonState == SIGNONSTATE_CONNECTED )
	{
		const char *checkpwd = NULL; 

		m_bIsHLTV = m_ConVars->GetInt( "tv_relay", 0 ) != 0;

		if ( m_bIsHLTV )
		{
			// if the connecting client is a TV relay, check the password
			checkpwd = tv_relaypassword.GetString();

			if ( checkpwd && checkpwd[0] && Q_stricmp( checkpwd, "none") )
			{
				if ( Q_stricmp( m_szPassword, checkpwd ) )
				{
					Disconnect("Bad relay password");
					return false;
				}
			}
		}
		else
		{
			// if client is a normal spectator, check if we can to forward him to other relays
			if ( m_pHLTV->DispatchToRelay( this ) )
			{
				return false;
			}

			// if client stays here, check the normal password
			checkpwd = m_pHLTV->GetPassword();

			if ( checkpwd )
			{
	
				if ( Q_stricmp( m_szPassword, checkpwd ) )
				{
					Disconnect("Bad spectator password");
					return false;
				}
			}

			// check if server is LAN only
			if ( !m_pHLTV->CheckIPRestrictions( m_NetChannel->GetRemoteAddress(), PROTOCOL_HASHEDCDKEY ) )
			{
				Disconnect( "SourceTV server is restricted to local spectators (class C).\n" );
				return false;
			}

		}
	}

	return true;
}

void CHLTVClient::UpdateUserSettings()
{
	// set voice loopback
	m_bNoChat = m_ConVars->GetInt( "tv_nochat", 0 ) != 0;
	
	CBaseClient::UpdateUserSettings();
}

void CHLTVClient::SendSnapshot( CClientFrame * pFrame )
{
	VPROF_BUDGET( "CHLTVClient::SendSnapshot", "HLTV" );

	ALIGN4 byte		buf[NET_MAX_PAYLOAD] ALIGN4_POST;
	bf_write	msg( "CHLTVClient::SendSnapshot", buf, sizeof(buf) );

	// if we send a full snapshot (no delta-compression) before, wait until client
	// received and acknowledge that update. don't spam client with full updates

	if ( m_pLastSnapshot == pFrame->GetSnapshot() )
	{
		// never send the same snapshot twice
		m_NetChannel->Transmit();	
		return;
	}

	if ( m_nForceWaitForTick > 0 )
	{
		// just continue transmitting reliable data
		Assert( !m_bFakePlayer );	// Should never happen
		m_NetChannel->Transmit();	
		return;
	}

	CClientFrame	*pDeltaFrame = GetDeltaFrame( m_nDeltaTick ); // NULL if delta_tick is not found
	CHLTVFrame		*pLastFrame = (CHLTVFrame*) GetDeltaFrame( m_nLastSendTick );

	if ( pLastFrame )
	{
		// start first frame after last send
		pLastFrame = (CHLTVFrame*) pLastFrame->m_pNext;
	}

	// add all reliable messages between ]lastframe,currentframe]
	// add all tempent & sound messages between ]lastframe,currentframe]
	while ( pLastFrame && pLastFrame->tick_count <= pFrame->tick_count )
	{	
		m_NetChannel->SendData( pLastFrame->m_Messages[HLTV_BUFFER_RELIABLE], true );	

		if ( pDeltaFrame )
		{
			// if we send entities delta compressed, also send unreliable data
			m_NetChannel->SendData( pLastFrame->m_Messages[HLTV_BUFFER_UNRELIABLE], false );
		}

		pLastFrame = (CHLTVFrame*) pLastFrame->m_pNext;
	}

	// now create client snapshot packet

	// send tick time
	NET_Tick tickmsg( pFrame->tick_count, host_frametime_unbounded, host_frametime_stddeviation );
	tickmsg.WriteToBuffer( msg );

	// Update shared client/server string tables. Must be done before sending entities
	m_Server->m_StringTables->WriteUpdateMessage( NULL, GetMaxAckTickCount(), msg );

	// TODO delta cache whole snapshots, not just packet entities. then use net_Align
	// send entity update, delta compressed if deltaFrame != NULL
	m_Server->WriteDeltaEntities( this, pFrame, pDeltaFrame, msg );

	// write message to packet and check for overflow
	if ( msg.IsOverflowed() )
	{
		if ( !pDeltaFrame )
		{
			// if this is a reliable snapshot, drop the client
			Disconnect( "ERROR! Reliable snapshot overflow." );
			return;
		}
		else
		{
			// unreliable snapshots may be dropped
			ConMsg ("WARNING: msg overflowed for %s\n", m_Name);
			msg.Reset();
		}
	}

	// remember this snapshot
	m_pLastSnapshot = pFrame->GetSnapshot();
	m_nLastSendTick = pFrame->tick_count;

	// Don't send the datagram to fakeplayers
	if ( m_bFakePlayer )
	{
		m_nDeltaTick = pFrame->tick_count;
		return;
	}

	bool bSendOK;

	// is this is a full entity update (no delta) ?
	if ( !pDeltaFrame )
	{
		// transmit snapshot as reliable data chunk
		bSendOK = m_NetChannel->SendData( msg );
		bSendOK = bSendOK && m_NetChannel->Transmit();

		// remember this tickcount we send the reliable snapshot
		// so we can continue sending other updates if this has been acknowledged
		m_nForceWaitForTick = pFrame->tick_count;
	}
	else
	{
		// just send it as unreliable snapshot
		bSendOK = m_NetChannel->SendDatagram( &msg ) > 0;
	}

	if ( !bSendOK )
	{
		Disconnect( "ERROR! Couldn't send snapshot." );
	}
}
