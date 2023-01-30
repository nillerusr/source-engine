//======177== (C) Copyright 1999, 2000 Valve, L.L.C. All rights reserved. ========
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
// $NoKeywords: $
//=============================================================================
#include "quakedef.h"
#include "server.h"
#include "master.h"
#include "proto_oob.h"
#include "host.h"
#include "eiface.h"
#include "server.h"

extern ConVar sv_tags;
extern ConVar sv_lan;

#define S2A_EXTRA_DATA_HAS_GAMETAG_DATA                         0x01            // Next bytes are the game tag string

//-----------------------------------------------------------------------------
// Purpose: List of master servers and some state info about them
//-----------------------------------------------------------------------------
typedef struct adrlist_s
{
	// Next master in chain
	struct adrlist_s	*next;
	// Challenge request sent to master
	qboolean			heartbeatwaiting;
	// Challenge request send time
	float				heartbeatwaitingtime; 
	// Last one is Main master
	int					heartbeatchallenge;
	// Time we sent last heartbeat
	double				last_heartbeat;
	// Master server address
	netadr_t			adr;
} adrlist_t;

//-----------------------------------------------------------------------------
// Purpose: Implements the master server interface
//-----------------------------------------------------------------------------
class CMaster : public IMaster, public IServersInfo
{
public:
	CMaster( void );
	virtual ~CMaster( void );

	// Heartbeat functions.
	void Init( void );
	void Shutdown( void );
	// Sets up master address
	void ShutdownConnection(void);
	void SendHeartbeat( struct adrlist_s *p );
	void AddServer( struct netadr_s *adr );
	void UseDefault ( void );
	void CheckHeartbeat (void);
	void RespondToHeartbeatChallenge( netadr_t &from, bf_read &msg );

	void ProcessConnectionlessPacket( netpacket_t *packet );

	void SetMaster_f( void );
	void Heartbeat_f( void );

	void RunFrame();
	void RequestServersInfo();
	void ReplyInfo( const netadr_t &adr );
	newgameserver_t &ProcessInfo( bf_read &buf );

	// SeversInfo
	void RequestInternetServerList( const char *gamedir, IServerListResponse *response );
	void RequestLANServerList( const char *gamedir, IServerListResponse *response );
	void AddServerAddresses( netadr_t **adr, int count );
private:
	// List of known master servers
	adrlist_t *m_pMasterAddresses;

	bool m_bInitialized;

	// If nomaster is true, the server will not send heartbeats to the master server
	bool	m_bNoMasters;

	CUtlLinkedList<netadr_t> m_serverAddresses;

	IServerListResponse *m_serverListResponse;
};

static CMaster s_MasterServer;
IMaster *master = (IMaster *)&s_MasterServer;

IServersInfo *g_pServersInfo = (IServersInfo*)&s_MasterServer;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CMaster, IServersInfo, SERVERLIST_INTERFACE_VERSION, s_MasterServer );

#define	HEARTBEAT_SECONDS	140.0

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMaster::CMaster( void )
{
	m_pMasterAddresses	= NULL;
	m_bNoMasters		= false;
	m_bInitialized = false;

	m_serverListResponse = NULL;

	Init();
}

CMaster::~CMaster( void )
{
}

void CMaster::RunFrame()
{
	CheckHeartbeat();
}

void CMaster::ReplyInfo( const netadr_t &adr )
{
	static char gamedir[MAX_OSPATH];
	Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );

	CUtlBuffer buf;
	buf.EnsureCapacity( 2048 );

	buf.PutUnsignedInt( LittleDWord( CONNECTIONLESS_HEADER ) );
	buf.PutUnsignedChar( S2C_INFOREPLY );

	buf.PutUnsignedChar( PROTOCOL_VERSION ); // Hardcoded protocol version number
	buf.PutString( sv.GetName() );
	buf.PutString( sv.GetMapName() );
	buf.PutString( gamedir );
	buf.PutString( serverGameDLL->GetGameDescription() );

	// player info
	buf.PutUnsignedChar( sv.GetNumClients() );
	buf.PutUnsignedChar( sv.GetMaxClients() );
	buf.PutUnsignedChar( sv.GetNumFakeClients() );

	// Password?
	buf.PutUnsignedChar( sv.GetPassword() != NULL ? 1 : 0 );

	// Write a byte with some flags that describe what is to follow.
	const char *pchTags = sv_tags.GetString();
	int nFlags = 0;

	if ( pchTags && pchTags[0] != '\0' )
		nFlags |= S2A_EXTRA_DATA_HAS_GAMETAG_DATA;

	buf.PutUnsignedInt( nFlags );

	if ( nFlags & S2A_EXTRA_DATA_HAS_GAMETAG_DATA )
	{
		buf.PutString( pchTags );
	}

	NET_SendPacket( NULL, NS_SERVER, adr, (unsigned char *)buf.Base(), buf.TellPut() );
}

newgameserver_t &CMaster::ProcessInfo(bf_read &buf)
{
	static newgameserver_t s;
	memset( &s, 0, sizeof(s) );

	s.m_nProtocolVersion = buf.ReadByte();

	buf.ReadString( s.m_szServerName, sizeof(s.m_szServerName) );
	buf.ReadString( s.m_szMap, sizeof(s.m_szMap) );
	buf.ReadString( s.m_szGameDir, sizeof(s.m_szGameDir) );

	buf.ReadString( s.m_szGameDescription, sizeof(s.m_szGameDescription) );

	// player info
	s.m_nPlayers = buf.ReadByte();
	s.m_nMaxPlayers = buf.ReadByte();
	s.m_nBotPlayers = buf.ReadByte();

	// Password?
	s.m_bPassword = buf.ReadByte();

	s.m_iFlags = buf.ReadLong();

	if( s.m_iFlags & S2A_EXTRA_DATA_HAS_GAMETAG_DATA )
	{
		buf.ReadString( s.m_szGameTags, sizeof(s.m_szGameTags) );
	}

	return s;
}

void CMaster::ProcessConnectionlessPacket( netpacket_t *packet )
{
	static ALIGN4 char string[2048] ALIGN4_POST;    // Buffer for sending heartbeat

	uint ip; uint16 port;

	bf_read msg = packet->message;
	char c = msg.ReadChar();

	if ( c == 0  )
		return;

	switch( c )
	{
		case M2S_CHALLENGE:
		{
			RespondToHeartbeatChallenge( packet->from, msg );
			break;
		}
		case M2C_QUERY:
		{
			if( m_serverAddresses.Count() > 0 )
				m_serverAddresses.RemoveAll();

			ip = msg.ReadLong();
			port = msg.ReadShort();

			while( ip != 0 && port != 0 )
			{
				netadr_t adr(ip, port);

				m_serverAddresses.AddToHead(adr);

				ip = msg.ReadLong();
				port = msg.ReadShort();
			}

			RequestServersInfo();
			break;
		}
		case C2S_INFOREQUEST:
		{
			ReplyInfo(packet->from);
			break;
		}
		case S2C_INFOREPLY:
		{
			newgameserver_t &s = ProcessInfo( msg );
			Msg("hostname = %s\nplayers: %d/%d\nbots: %d\n", s.m_szServerName, s.m_nPlayers, s.m_nMaxPlayers, s.m_nBotPlayers);

			s.m_NetAdr = packet->from;
			m_serverListResponse->ServerResponded( s );
			break;
		}
		case A2A_PING:
		{
			const char p = A2A_ACK;
			NET_SendPacket( NULL, NS_SERVER, packet->from, (unsigned char*)&p, 1);
			break;
		}
	}
}

void CMaster::RequestServersInfo()
{
	static ALIGN4 char string[256] ALIGN4_POST;    // Buffer for sending heartbeat

	bf_write msg( string, sizeof(string) );

	FOR_EACH_LL( m_serverAddresses, i )
	{
		const netadr_t adr = m_serverAddresses[i];

		Msg("Request server info %s\n", adr.ToString());

		msg.WriteLong( CONNECTIONLESS_HEADER );
		msg.WriteByte( C2S_INFOREQUEST );

		NET_SendPacket( NULL, NS_CLIENT, adr, msg.GetData(), msg.GetNumBytesWritten() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sends a heartbeat to the master server
// Input  : *p - x00\x00\x00\x00\x00\x00
//-----------------------------------------------------------------------------
void CMaster::SendHeartbeat ( adrlist_t *p )
{
	static ALIGN4 char string[256] ALIGN4_POST;    // Buffer for sending heartbeat
	char        szGD[ MAX_OSPATH ];

	if ( !p )
		return;

	// Still waiting on challenge response?
	if ( p->heartbeatwaiting )
		return;

	// Waited too long
	if ( (realtime - p->heartbeatwaitingtime ) >= HB_TIMEOUT )
		return;

	// Send to master
	// TODO(nillerusr): send engine version in this packet
	Q_FileBase( com_gamedir, szGD, sizeof( szGD ) );

	bf_write buf( string, sizeof(string) );
	buf.WriteByte( S2M_HEARTBEAT );
	buf.WriteLong( p->heartbeatchallenge );
	buf.WriteShort( PROTOCOL_VERSION );
	buf.WriteString( szGD );

	NET_SendPacket( NULL, NS_SERVER, p->adr, buf.GetData(), buf.GetNumBytesWritten() );
}

//-----------------------------------------------------------------------------
// Purpose: Requests a challenge so we can then send a heartbeat
//-----------------------------------------------------------------------------
void CMaster::CheckHeartbeat (void)
{
	adrlist_t *p;
	ALIGN4 char buf[256] ALIGN4_POST;

	if ( m_bNoMasters ||      // We are ignoring heartbeats
		sv_lan.GetInt() ||           // Lan servers don't heartbeat
		(sv.GetMaxClients() <= 1) ||  // not a multiplayer server.
		!sv.IsActive() )			  // only heartbeat if a server is running.
		return;

	p = m_pMasterAddresses;
	while ( p )
	{
		// Time for another try?
		if ( ( realtime - p->last_heartbeat) < HEARTBEAT_SECONDS)  // not time to send yet
		{
			p = p->next;
			continue;
		}

		// Should we resend challenge request?
		if ( p->heartbeatwaiting &&
			( ( realtime - p->heartbeatwaitingtime ) < HB_TIMEOUT ) )
		{
			p = p->next;
			continue;
		}

		int32 challenge = RandomInt( 0, INT_MAX );

		p->heartbeatwaiting     = true;
		p->heartbeatwaitingtime = realtime;

		p->last_heartbeat       = realtime;  // Flag at start so we don't just keep trying for hb's when
		p->heartbeatchallenge = challenge;

		bf_write msg("Master Join", buf, sizeof(buf));

		msg.WriteByte( S2M_GETCHALLENGE );
		msg.WriteLong( challenge );

		// Send to master asking for a challenge #
		NET_SendPacket( NULL, NS_SERVER, p->adr, msg.GetData(), msg.GetNumBytesWritten() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Server is shutting down, unload master servers list, tell masters that we are closing the server
//-----------------------------------------------------------------------------
void CMaster::ShutdownConnection( void )
{
	adrlist_t *p;

	if ( !host_initialized )
		return;

	if ( m_bNoMasters ||      // We are ignoring heartbeats
		sv_lan.GetInt() ||           // Lan servers don't heartbeat
		(sv.GetMaxClients() <= 1) )   // not a multiplayer server.
		return;

	const char packet = S2M_SHUTDOWN;

	p = m_pMasterAddresses;
	while ( p )
	{
		NET_SendPacket( NULL, NS_SERVER, p->adr, (unsigned char*)&packet, 1);
		p->last_heartbeat = -99999.0;
		p = p->next;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add server to the master list
// Input  : *adr - 
//-----------------------------------------------------------------------------
void CMaster::AddServer( netadr_t *adr )
{
	adrlist_t *n;

	// See if it's there
	n = m_pMasterAddresses;
	while ( n )
	{
		if ( n->adr.CompareAdr( *adr ) )
			break;
		n = n->next;
	}

	// Found it in the list.
	if ( n )
		return;

	n = ( adrlist_t * ) malloc ( sizeof( adrlist_t ) );
	if ( !n )
		Sys_Error( "Error allocating %i bytes for master address.", sizeof( adrlist_t ) );

	memset( n, 0, sizeof( adrlist_t ) );

	n->adr = *adr;

	// Queue up a full heartbeat to all master servers.
	n->last_heartbeat = -99999.0;

	// Link it in.
	n->next = m_pMasterAddresses;
	m_pMasterAddresses = n;
}

//-----------------------------------------------------------------------------
// Purpose: Add built-in default master if woncomm.lst doesn't parse
//-----------------------------------------------------------------------------
void CMaster::UseDefault ( void )
{
	netadr_t adr;

	// Convert to netadr_t
	if ( NET_StringToAdr ( DEFAULT_MASTER_ADDRESS, &adr ) )
	{
		// Add to master list
		AddServer( &adr );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaster::RespondToHeartbeatChallenge( netadr_t &from, bf_read &msg )
{
	adrlist_t *p;
	uint challenge, challenge2;

	// No masters, just ignore.
	if ( !m_pMasterAddresses )
		return;

	p = m_pMasterAddresses;
	while ( p )
	{
		if ( from.CompareAdr( p->adr ) )
			break;

		p = p->next;
	}

	// Not a known master server.
	if ( !p )
		return;

	challenge = msg.ReadLong();
	challenge2 = msg.ReadLong();

	if( p->heartbeatchallenge != challenge2 )
	{
		Warning("unexpected master server info query packet (wrong challenge!)\n");
		return;
	}

	// Kill timer
	p->heartbeatwaiting   = false;
	p->heartbeatchallenge = challenge;

	// Send the actual heartbeat request to this master server.
	SendHeartbeat( p );
}

//-----------------------------------------------------------------------------
// Purpose: Add/remove master servers
//-----------------------------------------------------------------------------
void CMaster::SetMaster_f (void)
{

}


//-----------------------------------------------------------------------------
// Purpose: Send a new heartbeat to the master
//-----------------------------------------------------------------------------
void CMaster::Heartbeat_f (void)
{
	adrlist_t *p;

	p = m_pMasterAddresses;
	while ( p )
	{
		// Queue up a full hearbeat
		p->last_heartbeat = -9999.0;
		p->heartbeatwaitingtime = -9999.0;
		p = p->next;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SetMaster_f( void )
{
	master->SetMaster_f();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Heartbeat1_f( void )
{
	master->Heartbeat_f();
}

static ConCommand setmaster("setmaster", SetMaster_f );
static ConCommand heartbeat("heartbeat", Heartbeat1_f, "Force heartbeat of master servers" ); 

//-----------------------------------------------------------------------------
// Purpose: Adds master server console commands
//-----------------------------------------------------------------------------
void CMaster::Init( void )
{
	// Already able to initialize at least once?
	if ( m_bInitialized )
		return;

	// So we don't do this a send time.sv_mas
	m_bInitialized = true;

	UseDefault();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMaster::Shutdown(void)
{
	adrlist_t *p, *n;

	// Free the master list now.
	p = m_pMasterAddresses;
	while ( p )
	{
		n = p->next;
		free( p );
		p = n;
	}

	m_pMasterAddresses = NULL;
}

// ServersInfo
void CMaster::RequestInternetServerList(const char *gamedir, IServerListResponse *response)
{
	if( m_bNoMasters ) return;

	m_serverListResponse = response;

	ALIGN4 char buf[256] ALIGN4_POST;
	bf_write msg(buf, sizeof(buf));

	msg.WriteByte( C2M_CLIENTQUERY );
	msg.WriteString(gamedir);

	// TODO(nillerusr): add switching between masters?
	NET_SendPacket(NULL, NS_CLIENT, m_pMasterAddresses->adr, msg.GetData(), msg.GetNumBytesWritten() );
}

void CMaster::RequestLANServerList(const char *gamedir, IServerListResponse *response)
{

}

void CMaster::AddServerAddresses( netadr_t **adr, int count )
{

}

void Master_Request_f()
{
	g_pServersInfo->RequestInternetServerList("cstrike", NULL);
}

ConCommand master_request( "master_request", Master_Request_f );
