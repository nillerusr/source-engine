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

extern ConVar sv_lan;

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
};

static CMaster s_MasterServer;
IMaster *master = (IMaster *)&s_MasterServer;

IServersInfo *g_pServersInfo = (IServersInfo*)&s_MasterServer;

#define	HEARTBEAT_SECONDS	140.0

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMaster::CMaster( void )
{
	m_pMasterAddresses	= NULL;
	m_bNoMasters		= false;
	m_bInitialized = false;
	Init();
}

CMaster::~CMaster( void )
{
}

void CMaster::RunFrame()
{
	CheckHeartbeat();
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
			bf_write p(string, sizeof(string));
			p.WriteLong(CONNECTIONLESS_HEADER);
			p.WriteByte(S2C_INFOREPLY);
			p.WriteString(sv.GetName());

			NET_SendPacket(NULL, NS_SERVER, packet->from, p.GetData(), p.GetNumBytesWritten());

			break;
		}
		case S2C_INFOREPLY:
		{
			char hostname[1024];
			msg.ReadString(hostname, sizeof(hostname));

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
