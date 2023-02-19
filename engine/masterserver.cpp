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
#include "utlmap.h"

extern ConVar sv_tags;
extern ConVar sv_lan;

#define S2A_EXTRA_DATA_HAS_GAMETAG_DATA                         0x01            // Next bytes are the game tag string
#define RETRY_INFO_REQUEST_TIME 0.4 // seconds
#define MASTER_RESPONSE_TIMEOUT 1.5 // seconds
#define INFO_REQUEST_TIMEOUT 5.0 // seconds

static char g_MasterServers[][64] =
{
	"185.192.97.130:27010",
	"168.138.92.21:27016"
};

#ifdef DEDICATED
#define IsLan() false
#else
#define IsLan() sv_lan.GetInt()
#endif

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
	void PingServer( netadr_t &svadr );

	void ProcessConnectionlessPacket( netpacket_t *packet );

	void AddMaster_f( const CCommand &args );
	void Heartbeat_f( void );

	void RunFrame();
	void RetryServersInfoRequest();

	void ReplyInfo( const netadr_t &adr, uint sequence );
	newgameserver_t &ProcessInfo( bf_read &buf );

	// SeversInfo
	void RequestInternetServerList( const char *gamedir, IServerListResponse *response );
	void RequestLANServerList( const char *gamedir, IServerListResponse *response );
	void AddServerAddresses( netadr_t **adr, int count );
	void RequestServerInfo( const netadr_t &adr );
	void StopRefresh();

private:
	// List of known master servers
	adrlist_t *m_pMasterAddresses;

	bool m_bInitialized;
	bool m_bRefreshing;

	int m_iServersResponded;

	double m_flStartRequestTime;
	double m_flRetryRequestTime;
	double m_flMasterRequestTime;

	uint m_iInfoSequence;
	char m_szGameDir[256];

	// If nomaster is true, the server will not send heartbeats to the master server
	bool	m_bNoMasters;

	CUtlMap<netadr_t, bool> m_serverAddresses;
	CUtlMap<uint, double> m_serversRequestTime;

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
	m_iServersResponded = 0;

	m_serverListResponse = NULL;
	SetDefLessFunc( m_serverAddresses );
	SetDefLessFunc( m_serversRequestTime );
	m_bRefreshing = false;
	m_iInfoSequence = 1;

	Init();
}

CMaster::~CMaster( void )
{
}

void CMaster::RunFrame()
{
	CheckHeartbeat();

	if( !m_bRefreshing )
		return;

	if( m_serverListResponse &&
		m_flStartRequestTime < Plat_FloatTime()-INFO_REQUEST_TIMEOUT )
	{
		StopRefresh();
		m_serverListResponse->RefreshComplete( NServerResponse::nServerFailedToRespond );
		return;
	}

	if( m_iServersResponded > 0 &&
			m_iServersResponded >= m_serverAddresses.Count() &&
			m_flMasterRequestTime < Plat_FloatTime() - MASTER_RESPONSE_TIMEOUT )
	{
		StopRefresh();
		m_serverListResponse->RefreshComplete( NServerResponse::nServerResponded );
		return;
	}

	if( m_flRetryRequestTime < Plat_FloatTime() - RETRY_INFO_REQUEST_TIME )
	{
		m_flRetryRequestTime = Plat_FloatTime();

		if( m_serverAddresses.Count() == 0 ) // Retry masterserver request
		{
			g_pServersInfo->RequestInternetServerList(m_szGameDir, NULL);
			return;
		}

		if( m_iServersResponded < m_serverAddresses.Count() )
			RetryServersInfoRequest();
	}
}

void CMaster::StopRefresh()
{
	if( !m_bRefreshing )
		return;

	m_iServersResponded = 0;
	m_bRefreshing = false;
	m_serverAddresses.RemoveAll();
	m_serversRequestTime.RemoveAll();
}

void CMaster::ReplyInfo( const netadr_t &adr, uint sequence )
{
	static char gamedir[MAX_OSPATH];
	Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );

	CUtlBuffer buf;
	buf.EnsureCapacity( 2048 );

	buf.PutUnsignedInt( LittleDWord( CONNECTIONLESS_HEADER ) );
	buf.PutUnsignedChar( S2C_INFOREPLY );

	buf.PutUnsignedInt(sequence);
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
		buf.PutString( pchTags );

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
			if( !m_bRefreshing )
				break;

			ip = msg.ReadLong();
			port = msg.ReadShort();

			while( ip != 0 && port != 0 )
			{
				netadr_t adr(ip, port);

				unsigned short index = m_serverAddresses.Find(adr);
				if( index != m_serverAddresses.InvalidIndex() )
				{
					ip = msg.ReadLong();
					port = msg.ReadShort();
					continue;
				}

				m_serverAddresses.Insert(adr, false);
				RequestServerInfo(adr);

				ip = msg.ReadLong();
				port = msg.ReadShort();
			}
			break;
		}
		case C2S_INFOREQUEST:
		{
			ReplyInfo(packet->from, msg.ReadLong());
			break;
		}
		case S2C_INFOREPLY:
		{
			if( !m_bRefreshing )
				break;

			uint sequence = msg.ReadLong();
			newgameserver_t &s = ProcessInfo( msg );

			unsigned short index = m_serverAddresses.Find(packet->from);
			unsigned short rindex = m_serversRequestTime.Find(sequence);

			if( index == m_serverAddresses.InvalidIndex() ||
				rindex == m_serversRequestTime.InvalidIndex() )
				break;

			double requestTime = m_serversRequestTime[rindex];

			if( m_serverAddresses[index] ) // shit happens
				return;

			m_serverAddresses[index] = true;
			s.m_nPing = (Plat_FloatTime()-requestTime)*1000.0;
			s.m_NetAdr = packet->from;
			m_serverListResponse->ServerResponded( s );

			m_iServersResponded++;
			break;
		}
	}
}

void CMaster::RequestServerInfo( const netadr_t &adr )
{
	static ALIGN4 char string[256] ALIGN4_POST;    // Buffer for sending heartbeat
	bf_write msg( string, sizeof(string) );

	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteByte( C2S_INFOREQUEST );
	msg.WriteLong( m_iInfoSequence );
	m_serversRequestTime.Insert(m_iInfoSequence, Plat_FloatTime());

	m_iInfoSequence++;
	NET_SendPacket( NULL, NS_CLIENT, adr, msg.GetData(), msg.GetNumBytesWritten() );
}

void CMaster::RetryServersInfoRequest()
{
	FOR_EACH_MAP_FAST( m_serverAddresses, i )
	{
		bool bResponded = m_serverAddresses.Element(i);
		if( bResponded )
			continue;

		const netadr_t adr = m_serverAddresses.Key(i);
		RequestServerInfo( adr );
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
		IsLan() ||           // Lan servers don't heartbeat
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
		IsLan() ||           // Lan servers don't heartbeat
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
		Sys_Error( "Error allocating %zd bytes for master address.", sizeof( adrlist_t ) );

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

	for( int i = 0; i < ARRAYSIZE(g_MasterServers);i++ )
	{
		// Convert to netadr_t
		if ( NET_StringToAdr ( g_MasterServers[i], &adr ) )
		{
			// Add to master list
			AddServer( &adr );
		}
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
void CMaster::AddMaster_f ( const CCommand &args )
{
	CUtlString cmd( ( args.ArgC() > 1 ) ? args[ 1 ] : "" );

	netadr_t adr;

	if( !NET_StringToAdr(cmd.String(), &adr) )
	{
		Warning("Invalid address\n");
		return;
	}

	this->AddServer(&adr);
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
void AddMaster_f( const CCommand &args )
{
	master->AddMaster_f( args );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Heartbeat1_f( void )
{
	master->Heartbeat_f();
}

static ConCommand setmaster("addmaster", AddMaster_f );
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
	strncpy( m_szGameDir, gamedir, sizeof(m_szGameDir) );

	if( response )
	{
		StopRefresh();
		m_bRefreshing = true;
		m_serverListResponse = response;
		m_flRetryRequestTime = m_flStartRequestTime = m_flMasterRequestTime = Plat_FloatTime();
	}

	ALIGN4 char buf[256] ALIGN4_POST;
	bf_write msg(buf, sizeof(buf));

	msg.WriteByte( C2M_CLIENTQUERY );
	msg.WriteString(gamedir);

	adrlist_t *p;

	p = m_pMasterAddresses;
	while ( p )
	{
		NET_SendPacket(NULL, NS_CLIENT, p->adr, msg.GetData(), msg.GetNumBytesWritten() );
		p = p->next;
	}
}

void CMaster::RequestLANServerList(const char *gamedir, IServerListResponse *response)
{

}

void CMaster::AddServerAddresses( netadr_t **adr, int count )
{

}
