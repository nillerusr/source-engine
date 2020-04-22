//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// baseserver.cpp: implementation of the CBaseServer class.
//
//////////////////////////////////////////////////////////////////////



#if defined(_WIN32) && !defined(_X360)
#include "winlite.h"		// FILETIME
#elif defined(POSIX)
#include <time.h>
/*
#include <sys/sysinfo.h>          
#include <asm/param.h> // for HZ
*/
#include <sys/resource.h>
#include <netinet/in.h>
#elif defined(_X360)
#else
#error "Includes for CPU usage calcs here"
#endif

#include "filesystem_engine.h"
#include "baseserver.h"
#include "sysexternal.h"
#include "quakedef.h"
#include "host.h"
#include "netmessages.h"
#include "sys.h"
#include "framesnapshot.h"
#include "sv_packedentities.h"
#include "dt_send_eng.h"
#include "dt_recv_eng.h"
#include "networkstringtable.h"
#include "sys_dll.h"
#include "host_cmd.h"
#include "sv_steamauth.h"

#include <proto_oob.h>
#include <vstdlib/random.h>
#include <irecipientfilter.h>
#include <KeyValues.h>
#include <tier0/vprof.h>
#include <cdll_int.h>
#include <eiface.h>
#include <client_class.h>
#include "tier0/icommandline.h"
#include "sv_steamauth.h"
#include "tier0/vcrmode.h"
#include "sv_ipratelimit.h"
#include "cl_steamauth.h"
#include "sv_filter.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CThreadFastMutex g_svInstanceBaselineMutex;
extern	CGlobalVars g_ServerGlobalVariables;

static ConVar	sv_max_queries_sec( "sv_max_queries_sec", "3.0", 0, "Maximum queries per second to respond to from a single IP address." );
static ConVar	sv_max_queries_window( "sv_max_queries_window", "30", 0, "Window over which to average queries per second averages." );
static ConVar	sv_max_queries_sec_global( "sv_max_queries_sec_global", "3000", 0, "Maximum queries per second to respond to from anywhere." );

static ConVar	sv_max_connects_sec( "sv_max_connects_sec", "2.0", 0, "Maximum connections per second to respond to from a single IP address." );
static ConVar	sv_max_connects_window( "sv_max_connects_window", "4", 0, "Window over which to average connections per second averages." );
// This defaults to zero so that somebody spamming the server with packets cannot lock out other clients.
static ConVar	sv_max_connects_sec_global( "sv_max_connects_sec_global", "0", 0, "Maximum connections per second to respond to from anywhere." );

static CIPRateLimit s_queryRateChecker( &sv_max_queries_sec, &sv_max_queries_window, &sv_max_queries_sec_global );
static CIPRateLimit s_connectRateChecker( &sv_max_connects_sec, &sv_max_connects_window, &sv_max_connects_sec_global );

// Give new data to Steam's master server updater every N seconds.
// This is NOT how often packets are sent to master servers, only how often the
// game server talks to Steam's master server updater (which is on the game server's
// machine, not the Steam servers).
#define MASTER_SERVER_UPDATE_INTERVAL		2.0

// Steam has a matching one in matchmakingtypes.h
#define MAX_TAG_STRING_LENGTH		128

int SortServerTags( char* const *p1, char* const *p2 )
{
	return ( Q_strcmp( *p1, *p2 ) > 0 );
}

static void ServerTagsCleanUp( void )
{
	CUtlVector<char*> TagList;
	ConVarRef sv_tags( "sv_tags" );
	if ( sv_tags.IsValid() )
	{
		int i;
		char tmptags[MAX_TAG_STRING_LENGTH];
		tmptags[0] = '\0';

		V_SplitString( sv_tags.GetString(), ",", TagList );

		// make a pass on the tags to eliminate preceding whitespace and empty tags
		for ( i = 0; i < TagList.Count(); i++ )
		{
			if ( i > 0 )
			{
				Q_strncat( tmptags, ",", MAX_TAG_STRING_LENGTH );
			}

			char *pChar = TagList[i];
			while ( *pChar && *pChar == ' ' )
			{
				pChar++;
			}

			// make sure we don't have an empty string (all spaces or ,,)
			if ( *pChar )
			{
				Q_strncat( tmptags, pChar, MAX_TAG_STRING_LENGTH );
			}
		}

		// reset our lists and sort the tags
		TagList.PurgeAndDeleteElements();
		V_SplitString( tmptags, ",", TagList );
		TagList.Sort( SortServerTags );
		tmptags[0] = '\0';

		// create our new, sorted list of tags
		for ( i = 0; i < TagList.Count(); i++ )
		{
			if ( i > 0 )
			{
				Q_strncat( tmptags, ",", MAX_TAG_STRING_LENGTH );
			}

			Q_strncat( tmptags, TagList[i], MAX_TAG_STRING_LENGTH );
		}

		// set our convar and purge our list
		sv_tags.SetValue( tmptags );
		TagList.PurgeAndDeleteElements();
	}
}

static void SvTagsChangeCallback( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	// We're going to modify the sv_tags convar here, which will cause this to be called again. Prevent recursion.
	static bool bTagsChangeCallback = false;
	if ( bTagsChangeCallback )
		return;

	bTagsChangeCallback = true;

	ServerTagsCleanUp();

	ConVarRef var( pConVar );
	if ( Steam3Server().SteamGameServer() )
	{
		Steam3Server().SteamGameServer()->SetGameTags( var.GetString() );
	}

	bTagsChangeCallback = false;
}

ConVar			sv_region( "sv_region","-1", FCVAR_NONE, "The region of the world to report this server in." );
static ConVar	sv_instancebaselines( "sv_instancebaselines", "1", FCVAR_DEVELOPMENTONLY, "Enable instanced baselines. Saves network overhead." );
static ConVar	sv_stats( "sv_stats", "1", 0, "Collect CPU usage stats" );
static ConVar	sv_enableoldqueries( "sv_enableoldqueries", "0", 0, "Enable support for old style (HL1) server queries" );
static ConVar	sv_password( "sv_password", "", FCVAR_NOTIFY | FCVAR_PROTECTED | FCVAR_DONTRECORD, "Server password for entry into multiplayer games" );
ConVar			sv_tags( "sv_tags", "", FCVAR_NOTIFY, "Server tags. Used to provide extra information to clients when they're browsing for servers. Separate tags with a comma.", SvTagsChangeCallback );
ConVar			sv_visiblemaxplayers( "sv_visiblemaxplayers", "-1", 0, "Overrides the max players reported to prospective clients" );
ConVar			sv_alternateticks( "sv_alternateticks", ( IsX360() ) ? "1" : "0", FCVAR_SPONLY, "If set, server only simulates entities on even numbered ticks.\n" );
ConVar			sv_allow_wait_command( "sv_allow_wait_command", "1", FCVAR_REPLICATED, "Allow or disallow the wait command on clients connected to this server." );
ConVar			sv_allow_color_correction( "sv_allow_color_correction", "1", FCVAR_REPLICATED, "Allow or disallow clients to use color correction on this server." );

extern CNetworkStringTableContainer *networkStringTableContainerServer;
extern ConVar sv_stressbots;

int g_CurGameServerID = 1;

// #define ALLOW_DEBUG_DEDICATED_SERVER_OUTSIDE_STEAM

bool AllowDebugDedicatedServerOutsideSteam()
{
#if defined( ALLOW_DEBUG_DEDICATED_SERVER_OUTSIDE_STEAM )
	return true;
#else
	return false;
#endif
}


static void SetMasterServerKeyValue( ISteamGameServer *pUpdater, IConVar *pConVar )
{
	ConVarRef var( pConVar );

	// For protected cvars, don't send the string
	if ( var.IsFlagSet( FCVAR_PROTECTED ) )
	{
		// If it has a value string and the string is not "none"
		if ( ( strlen( var.GetString() ) > 0 ) &&
				stricmp( var.GetString(), "none" ) )
		{
			pUpdater->SetKeyValue( var.GetName(), "1" );
		}
		else
		{
			pUpdater->SetKeyValue( var.GetName(), "0" );
		}
	}
	else
	{
		pUpdater->SetKeyValue( var.GetName(), var.GetString() );
	}

	if ( Steam3Server().BIsActive() )
	{
		sv.RecalculateTags();
	}
}


static void ServerNotifyVarChangeCallback( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	if ( !pConVar->IsFlagSet( FCVAR_NOTIFY ) )
		return;
	
	ISteamGameServer *pUpdater = Steam3Server().SteamGameServer();
	if ( !pUpdater )
	{
		// This will force it to send all the rules whenever the master server updater is there.
		sv.SetMasterServerRulesDirty();
		return;
	}

	SetMasterServerKeyValue( pUpdater, pConVar );
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CBaseServer::CBaseServer() 
{
	// Just get a unique ID to talk to the steam master server updater.
	m_bRestartOnLevelChange = false;
	
	m_StringTables = NULL;
	m_pInstanceBaselineTable = NULL;
	m_pLightStyleTable = NULL;
	m_pUserInfoTable = NULL;
	m_pServerStartupTable = NULL;
	m_pDownloadableFileTable = NULL;

	m_fLastCPUCheckTime = 0;
	m_fStartTime = 0;
	m_fCPUPercent = 0;
	m_Socket = NS_SERVER;
	m_nTickCount = 0;
	
	m_szMapname[0] = 0;
	m_szSkyname[0] = 0;
	m_Password[0] = 0;
	V_memset( worldmapMD5.bits, 0, MD5_DIGEST_LENGTH );

	serverclasses = serverclassbits = 0;
	m_nMaxclients = m_nSpawnCount = 0;
	m_flTickInterval = 0.03;
	m_nUserid = 0;
	m_nNumConnections = 0;
	m_bIsDedicated = false;
	m_fCPUPercent = 0;
	m_fStartTime = 0;
	m_fLastCPUCheckTime = 0;
	
	m_bMasterServerRulesDirty = true;
	m_flLastMasterServerUpdateTime = 0;
	m_CurrentRandomNonce = 0;
	m_LastRandomNonce = 0;
	m_flLastRandomNumberGenerationTime = -3.0f; // force it to calc first frame

	m_bReportNewFakeClients = true;
	m_flPausedTimeEnd = -1.f;
}

CBaseServer::~CBaseServer()
{

}

/*
================
SV_CheckChallenge

Make sure connecting client is not spoofing
================
*/
bool CBaseServer::CheckChallengeNr( netadr_t &adr, int nChallengeValue )
{
	// See if the challenge is valid
	// Don't care if it is a local address.
	if ( adr.IsLoopback() )
		return true;

	// X360TBD: network
	if ( IsX360() )
		return true;

	uint64 challenge = ((uint64)adr.GetIPNetworkByteOrder() << 32) + m_CurrentRandomNonce;
	CRC32_t hash;
	CRC32_Init( &hash );
	CRC32_ProcessBuffer( &hash, &challenge, sizeof(challenge) );
	CRC32_Final( &hash );
	if ( (int)hash == nChallengeValue )
		return true;

	// try with the old random nonce
	challenge &= 0xffffffff00000000ull;
	challenge += m_LastRandomNonce;
	hash = 0;
	CRC32_Init( &hash );
	CRC32_ProcessBuffer( &hash, &challenge, sizeof(challenge) );
	CRC32_Final( &hash );
	if ( (int)hash == nChallengeValue )
		return true;

	return false;
}


const char *CBaseServer::GetPassword() const
{
	const char *password = sv_password.GetString();

	// if password is empty or "none", return NULL
	if ( !password[0] || !Q_stricmp(password, "none" ) )
	{
		return NULL;
	}

	return password;
}


void CBaseServer::SetPassword(const char *password)
{
	if ( password != NULL )
	{
		Q_strncpy( m_Password, password, sizeof(m_Password) );
	}
	else
	{
		m_Password[0] = 0; // clear password
	}
}

#define MAX_REUSE_PER_IP 5 // 5 outstanding connect request within timeout window, to account for NATs

/*
================
CheckIPConnectionReuse

Determine if this IP requesting the connect is connecting too often
================
*/
bool CBaseServer::CheckIPConnectionReuse( netadr_t &adr )
{
	int nSimultaneouslyConnections = 0;

	for ( int slot = 0 ; slot < m_Clients.Count() ; slot++ )
	{
		CBaseClient *client = m_Clients[slot];

		// if the user is connected but not fully in AND the addr's match
		if ( client->IsConnected() &&
			 !client->IsActive() &&
			 !client->IsFakeClient() && 
			 adr.CompareAdr ( client->m_NetChannel->GetRemoteAddress(), true ) )
		{
			nSimultaneouslyConnections++;
		}
	}
	
	if ( nSimultaneouslyConnections > MAX_REUSE_PER_IP ) 
	{
		Msg ("Too many connect packets from %s\n", adr.ToString( true ) );	
		return false; // too many connect packets!!!!
	}
	return true; // this IP is okay
}


int CBaseServer::GetNextUserID()
{
	// Note: we'll usually exit on the first pass of this loop..
	for ( int i=0; i < m_Clients.Count()+1; i++ )
	{
		int nTestID = (m_nUserid + i + 1) % SHRT_MAX;

		// Make sure no client has this user ID.		
		int iClient;
		for ( iClient=0; iClient < m_Clients.Count(); iClient++ )
		{
			if ( m_Clients[iClient]->GetUserID() == nTestID )
				break;
		}

		// Ok, no client has this ID, so return it.		
		if ( iClient == m_Clients.Count() )
			return nTestID;
	}
	
	Assert( !"GetNextUserID: can't find a unique ID." );
	return m_nUserid + 1;
}


/*
================
SV_ConnectClient

Initializes a CSVClient for a new net connection.  This will only be called
once for a player each game, not once for each level change.
================
*/
IClient *CBaseServer::ConnectClient ( netadr_t &adr, int protocol, int challenge, int clientChallenge, int authProtocol, 
							    const char *name, const char *password, const char *hashedCDkey, int cdKeyLen )
{
	COM_TimestampedLog( "CBaseServer::ConnectClient" );

	if ( !IsActive() )
	{
		return NULL;
	}

	if ( !name || !password || !hashedCDkey )
	{
		return NULL;
	}

	// Make sure protocols match up
	if ( !CheckProtocol( adr, protocol, clientChallenge ) )
	{
		return NULL;
	}


	if ( !CheckChallengeNr( adr, challenge ) )
	{
		RejectConnection( adr, clientChallenge, "#GameUI_ServerRejectBadChallenge" );
		return NULL;
	}

	// SourceTV checks password & restrictions later once we know
	// if its a normal spectator client or a relay proxy
	if ( !IsHLTV() && !IsReplay() )
	{
#ifndef NO_STEAM
		// LAN servers restrict to class b IP addresses
		if ( !CheckIPRestrictions( adr, authProtocol ) )
		{
			RejectConnection( adr, clientChallenge, "#GameUI_ServerRejectLANRestrict");
			return NULL;
		}
#endif

		if ( !CheckPassword( adr, password, name ) )
		{
			// failed
			ConMsg ( "%s:  password failed.\n", adr.ToString() );
			// Special rejection handler.
			RejectConnection( adr, clientChallenge, "#GameUI_ServerRejectBadPassword" );
			return NULL;
		}
	}

	COM_TimestampedLog( "CBaseServer::ConnectClient:  GetFreeClient" );

	CBaseClient	*client = GetFreeClient( adr );

	if ( !client )
	{
		RejectConnection( adr, clientChallenge, "#GameUI_ServerRejectServerFull" );
		return NULL;	// no free slot found
	}

	int nNextUserID = GetNextUserID();
	if ( !CheckChallengeType( client, nNextUserID, adr, authProtocol, hashedCDkey, cdKeyLen, clientChallenge ) ) // we use the client pointer to track steam requests
	{
		return NULL;
	}

	ISteamGameServer *pSteamGameServer = Steam3Server().SteamGameServer();
	if ( !pSteamGameServer && authProtocol == PROTOCOL_STEAM )
	{
		Warning("NULL ISteamGameServer in ConnectClient. Steam authentication may fail.\n");
	}

	if ( Filter_IsUserBanned( client->GetNetworkID() ) )
	{
		// Need to make sure the master server is updated with the rejected connection because
		// we called Steam3Server().NotifyClientConnect() in CheckChallengeType() above.
		if ( pSteamGameServer && authProtocol == PROTOCOL_STEAM )
			pSteamGameServer->SendUserDisconnect( client->m_SteamID ); 

		RejectConnection( adr, clientChallenge, "#GameUI_ServerRejectBanned" );
		return NULL;
	}

#if !defined( _HLTVTEST ) && !defined( _REPLAYTEST )
	if ( !FinishCertificateCheck( adr, authProtocol, hashedCDkey, clientChallenge ) )
	{
		// Need to make sure the master server is updated with the rejected connection because
		// we called Steam3Server().NotifyClientConnect() in CheckChallengeType() above.
		if ( pSteamGameServer && authProtocol == PROTOCOL_STEAM )
			pSteamGameServer->SendUserDisconnect( client->m_SteamID ); 

		return NULL;
	}
#endif

	COM_TimestampedLog( "CBaseServer::ConnectClient:  NET_CreateNetChannel" );

	// create network channel
	INetChannel * netchan = NET_CreateNetChannel( m_Socket, &adr, adr.ToString(), client );

	if ( !netchan )
	{
		// Need to make sure the master server is updated with the rejected connection because
		// we called Steam3Server().NotifyClientConnect() in CheckChallengeType() above.
		if ( pSteamGameServer && authProtocol == PROTOCOL_STEAM )
			pSteamGameServer->SendUserDisconnect( client->m_SteamID ); 

		RejectConnection( adr, clientChallenge, "#GameUI_ServerRejectFailedChannel" );
		return NULL;
	}

	// setup netchannl settings
	netchan->SetChallengeNr( challenge );
	
	COM_TimestampedLog( "CBaseServer::ConnectClient:  client->Connect" );

	// make sure client is reset and clear
	client->Connect( name, nNextUserID, netchan, false, clientChallenge );

	m_nUserid = nNextUserID;
	m_nNumConnections++;

	// Will get reset from userinfo, but this value comes from sv_updaterate ( the default )
	client->m_fSnapshotInterval = 1.0f/20.0f;
	client->m_fNextMessageTime = net_time + client->m_fSnapshotInterval;
	// Force a full delta update on first packet.
	client->m_nDeltaTick = -1;
	client->m_nSignonTick = 0;
	client->m_nStringTableAckTick = 0;
	client->m_pLastSnapshot = NULL;
	
	// Tell client connection worked, now use netchannels
	{
		ALIGN4 char		msg_buffer[MAX_ROUTABLE_PAYLOAD] ALIGN4_POST;
		bf_write	msg( msg_buffer, sizeof(msg_buffer) );

		msg.WriteLong( CONNECTIONLESS_HEADER );
		msg.WriteByte( S2C_CONNECTION );
		msg.WriteLong( clientChallenge );
		msg.WriteString( "0000000000" ); // pad out

		NET_SendPacket ( NULL, m_Socket, adr, msg.GetData(), msg.GetNumBytesWritten() );
	}

	// Set up client structure.
	if ( authProtocol == PROTOCOL_HASHEDCDKEY )
	{
		// use hased CD key as player GUID
		Q_strncpy ( client->m_GUID, hashedCDkey, SIGNED_GUID_LEN );
		client->m_GUID[SIGNED_GUID_LEN] = '\0';
	}
	else if ( authProtocol == PROTOCOL_STEAM )
	{
		// StartSteamValidation() above initialized the clients networkid
	}

	if ( netchan && !netchan->IsLoopback() )
		ConMsg("Client \"%s\" connected (%s).\n", client->GetClientName(), netchan->GetAddress() );

	return client;
}

/*
================
RequireValidChallenge

Return true if this server query must provide a valid challenge number
================
*/
bool CBaseServer::RequireValidChallenge( netadr_t &adr )
{
	if ( sv_enableoldqueries.GetBool() == true )
	{
		return false; // don't enforce challenge numbers
	}

	return true;
}

/*
================
ValidChallenge

Return true if this challenge number is correct for this host (for server queries)
================
*/
bool CBaseServer::ValidChallenge( netadr_t & adr, int challengeNr )
{
	if ( !IsActive() )            // Must be running a server.
		return false ;

	if ( !IsMultiplayer() )   // ignore in single player
		return false ;

	if ( RequireValidChallenge( adr) )
	{
		if ( !CheckChallengeNr( adr, challengeNr ) )
		{
			ReplyServerChallenge( adr );
			return false;
		}
	}

	return true; 
}

bool CBaseServer::ValidInfoChallenge( netadr_t & adr, const char *nugget )
{
	if ( !IsActive() )            // Must be running a server.
		return false ;

	if ( !IsMultiplayer() )   // ignore in single player
		return false ;

	if ( IsReplay() )
		return false;

	if ( RequireValidChallenge( adr) )
	{
		if ( Q_stricmp( nugget, A2S_KEY_STRING ) ) // if the string isn't equal then fail out
		{
			return false;
		}
	}

	return true; 
}


bool CBaseServer::ProcessConnectionlessPacket(netpacket_t * packet)
{
	bf_read msg = packet->message;	// handy shortcut 

	char c = msg.ReadChar();

	if ( c== 0  )
	{
		return false;
	}

	switch ( c )
	{
		case A2S_GETCHALLENGE :
			{
				int clientChallenge = msg.ReadLong();
				ReplyChallenge( packet->from, clientChallenge );
			}

			break;
		
		case A2S_SERVERQUERY_GETCHALLENGE:
			ReplyServerChallenge( packet->from );
			break;

		case C2S_CONNECT :
			{
				char cdkey[STEAM_KEYSIZE];
				char name[256];
				char password[256];
				char productVersion[32];
				
				int protocol = msg.ReadLong();
				int authProtocol = msg.ReadLong();
				int challengeNr = msg.ReadLong();
				int clientChallenge = msg.ReadLong();

				// pull the challenge number check early before we do any expensive processing on the connect
				if ( !CheckChallengeNr( packet->from, challengeNr ) )
				{
					RejectConnection( packet->from, clientChallenge, "#GameUI_ServerRejectBadChallenge" );
					break;
				}

				// rate limit the connections
				if ( !s_connectRateChecker.CheckIP( packet->from ) )
					return false;

				msg.ReadString( name, sizeof(name) );
				msg.ReadString( password, sizeof(password) );
				msg.ReadString( productVersion, sizeof(productVersion) );
				
//				bool bClientPlugins = ( msg.ReadByte() > 0 );

				// There's a magic number we use in the steam.inf in P4 that we don't update.
				// We can use this to detect if they are running out of P4, and if so, don't do any version
				// checking.
				const char *pszVersionInP4 = "2000";
				const char *pszVersionString = GetSteamInfIDVersionInfo().szVersionString;
				if ( V_strcmp( pszVersionString, pszVersionInP4 ) && V_strcmp( productVersion, pszVersionInP4 ) )
				{
					int nVersionCheck = Q_strncmp( pszVersionString, productVersion, V_strlen( pszVersionString ) );
					if ( nVersionCheck < 0 )
					{
						RejectConnection( packet->from, clientChallenge, "#GameUI_ServerRejectOldVersion" );
						break;
					}
					if ( nVersionCheck > 0 )
					{
						RejectConnection( packet->from, clientChallenge, "#GameUI_ServerRejectNewVersion" );
						break;
					}
				}

// 				if ( Steam3Server().BSecure() && bClientPlugins )
// 				{
// 					RejectConnection( packet->from, "Cannot connect to a secure server while plug-ins are\nloaded on your client\n" );
//					break;
// 				}

				if ( authProtocol == PROTOCOL_STEAM )
				{
					int keyLen = msg.ReadShort();
					if ( keyLen < 0 || keyLen > sizeof(cdkey) )
					{
						RejectConnection( packet->from, clientChallenge, "#GameUI_ServerRejectBadSteamKey" );
						break;
					}
					msg.ReadBytes( cdkey, keyLen );

					ConnectClient( packet->from, protocol, challengeNr, clientChallenge, authProtocol, name, password, cdkey, keyLen );	// cd key is actually a raw encrypted key	
				}
				else
				{
					msg.ReadString( cdkey, sizeof(cdkey) );
					ConnectClient( packet->from, protocol, challengeNr, clientChallenge, authProtocol, name, password, cdkey, strlen(cdkey) );
				}
			}

			break;
		
		default:
			{
				// rate limit the more expensive server query packets
				if ( !s_queryRateChecker.CheckIP( packet->from ) )
					return false;

				// We don't understand it, let the master server updater at it.
				if ( Steam3Server().SteamGameServer() && Steam3Server().IsMasterServerUpdaterSharingGameSocket() )
				{
					Steam3Server().SteamGameServer()->HandleIncomingPacket( 
						packet->message.GetBasePointer(), 
						packet->message.TotalBytesAvailable(),
						packet->from.GetIPHostByteOrder(),
						packet->from.GetPort()
						);
				
					// This is where it will usually want to respond to something immediately by sending some
					// packets, so check for that immediately.
					ForwardPacketsFromMasterServerUpdater();
				}
			}

			break;
	}

	return true;
}

int CBaseServer::GetNumFakeClients() const
{
	int count = 0; 
	for ( int i = 0; i < m_Clients.Count(); i++ )
	{
		if ( m_Clients[i]->IsFakeClient() )
		{
			count++;
		}
	}
	return count;
}

/*
==================
void SV_CountPlayers

Counts number of connections.  Clients includes regular connections
==================
*/
int CBaseServer::GetNumClients( void ) const
{
	int count	= 0;

	for (int i=0 ; i < m_Clients.Count() ; i++ )
	{
		if ( m_Clients[ i ]->IsConnected() )
		{
			count++;
		}
	}

	return count;
}

/*
==================
void SV_CountPlayers

Counts number of HLTV and Replay connections.  Clients includes regular connections
==================
*/
int CBaseServer::GetNumProxies( void ) const
{
	int count	= 0;

	for (int i=0 ; i < m_Clients.Count() ; i++ )
	{
#if defined( REPLAY_ENABLED )
		if ( m_Clients[ i ]->IsConnected() && (m_Clients[ i ]->IsHLTV() || m_Clients[ i ]->IsReplay() ) )
#else
		if ( m_Clients[ i ]->IsConnected() && m_Clients[ i ]->IsHLTV() )
#endif
		{
			count++;
		}
	}

	return count;
}

int CBaseServer::GetNumPlayers()
{
	int count = 0;
	if ( !GetUserInfoTable())
	{
		return 0;
	}

	const int maxPlayers = GetUserInfoTable()->GetNumStrings();

	for ( int i=0; i < maxPlayers; i++ )
	{
		const player_info_t *pi = (const player_info_t *) m_pUserInfoTable->GetStringUserData( i, NULL );

		if ( !pi )
			continue;

		if ( pi->fakeplayer )
			continue;	// don't count bots

		count++;
	}

	return count;
}

bool CBaseServer::GetPlayerInfo( int nClientIndex, player_info_t *pinfo )
{
	if ( !pinfo )
		return false;

	if ( nClientIndex < 0 || !GetUserInfoTable() || nClientIndex >= GetUserInfoTable()->GetNumStrings() )
	{
		Q_memset( pinfo, 0, sizeof( player_info_t ) );
		return false;
	}

	player_info_t *pi = (player_info_t*) GetUserInfoTable()->GetStringUserData( nClientIndex, NULL );

	if ( !pi )
	{
		Q_memset( pinfo, 0, sizeof( player_info_t ) );
		return false;
	}

	Q_memcpy( pinfo, pi, sizeof( player_info_t ) );

	// Fixup from network order (little endian)
	CByteswap byteswap;
	byteswap.SetTargetBigEndian( false );
	byteswap.SwapFieldsToTargetEndian( pinfo );

	return true;
}


void CBaseServer::UserInfoChanged( int nClientIndex )
{
	player_info_t pi;

	bool oldlock = networkStringTableContainerServer->Lock( false );
	if ( m_Clients[ nClientIndex ]->FillUserInfo( pi ) )
	{
		// Fixup to little endian for networking
		CByteswap byteswap;
		byteswap.SetTargetBigEndian( false );
		byteswap.SwapFieldsToTargetEndian( &pi );

		// update user info settings
		m_pUserInfoTable->SetStringUserData( nClientIndex, sizeof(pi), &pi );
	}
	else
	{
		// delete user data settings
		m_pUserInfoTable->SetStringUserData( nClientIndex, 0, NULL );
	}
	networkStringTableContainerServer->Lock( oldlock );
	
}

void CBaseServer::FillServerInfo(SVC_ServerInfo &serverinfo)
{
	static char gamedir[MAX_OSPATH];
	Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );

	serverinfo.m_nProtocol		= PROTOCOL_VERSION;
	serverinfo.m_nServerCount	= GetSpawnCount();
	V_memcpy( serverinfo.m_nMapMD5.bits, worldmapMD5.bits, MD5_DIGEST_LENGTH );
	serverinfo.m_nMaxClients	= GetMaxClients();
	serverinfo.m_nMaxClasses	= serverclasses;
	serverinfo.m_bIsDedicated	= IsDedicated();
#ifdef _WIN32
	serverinfo.m_cOS			= 'W';
#else
	serverinfo.m_cOS			= 'L';
#endif

	// HACK to signal that the server is "new"
	serverinfo.m_cOS = tolower( serverinfo.m_cOS );

	serverinfo.m_fTickInterval	= GetTickInterval();
	serverinfo.m_szGameDir		= gamedir;
	serverinfo.m_szMapName		= GetMapName();
	serverinfo.m_szSkyName		= m_szSkyname;
	serverinfo.m_szHostName		= GetName();
	serverinfo.m_bIsHLTV		= IsHLTV();
#if defined( REPLAY_ENABLED )
	serverinfo.m_bIsReplay		= IsReplay();
#endif
}

/*
=================
SVC_GetChallenge

Returns a challenge number that can be used
in a subsequent client_connect command.
We do this to prevent denial of service attacks that
flood the server with invalid connection IPs.  With a
challenge, they must give a valid IP address.
=================
*/

void CBaseServer::ReplyChallenge(netadr_t &adr, int clientChallenge )
{
	ALIGN4 char	buffer[STEAM_KEYSIZE+32] ALIGN4_POST;
	bf_write msg(buffer,sizeof(buffer));

	// get a free challenge number
	int challengeNr = GetChallengeNr( adr );
	int	authprotocol = GetChallengeType( adr );

	msg.WriteLong( CONNECTIONLESS_HEADER );
	
	msg.WriteByte( S2C_CHALLENGE );
	msg.WriteLong( S2C_MAGICVERSION ); // This makes it so we can detect that this server is correct
	msg.WriteLong( challengeNr ); // Server to client challenge
	msg.WriteLong( clientChallenge ); // Client to server challenge to ensure our reply is what they asked
	msg.WriteLong( authprotocol );

#if !defined( NO_STEAM ) //#ifndef _XBOX
	if ( authprotocol == PROTOCOL_STEAM )
	{
		msg.WriteShort( 0 ); //  steam2 encryption key not there anymore
		CSteamID steamID = Steam3Server().GetGSSteamID();
		uint64 unSteamID = steamID.ConvertToUint64();
		msg.WriteBytes( &unSteamID, sizeof(unSteamID) );
		msg.WriteByte( Steam3Server().BSecure() );
	}
#else
	msg.WriteShort( 1 );
	msg.WriteByte( 0 );
	uint64 unSteamID = 0;
	msg.WriteBytes( &unSteamID, sizeof(unSteamID) );
	msg.WriteByte( 0 );
#endif
	msg.WriteString( "000000" );	// padding bytes

	NET_SendPacket( NULL, m_Socket, adr, msg.GetData(), msg.GetNumBytesWritten() );
}


/*
=================
ReplyServerChallenge

Returns a challenge number that can be used
in a subsequent server query commands.
We do this to prevent DDoS attacks via bandwidth
amplification.
=================
*/
void CBaseServer::ReplyServerChallenge(netadr_t &adr)
{
	ALIGN4 char	buffer[16] ALIGN4_POST;
	bf_write msg(buffer,sizeof(buffer));

	// get a free challenge number
	int challengeNr = GetChallengeNr( adr );
	
	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteByte( S2C_CHALLENGE );
	msg.WriteLong( challengeNr );
	NET_SendPacket( NULL, m_Socket, adr, msg.GetData(), msg.GetNumBytesWritten() );
}

const char *CBaseServer::GetName( void ) const
{
	return host_name.GetString();
}

int CBaseServer::GetChallengeType(netadr_t &adr)
{
	if ( AllowDebugDedicatedServerOutsideSteam() )
		return PROTOCOL_HASHEDCDKEY;

#ifndef SWDS	
	// don't auth SP games or local mp games if steam isn't running
	if ( Host_IsSinglePlayerGame() || ( !Steam3Client().SteamUser() && !IsDedicated() ))
	{
		return PROTOCOL_HASHEDCDKEY;
	}
	else
#endif
	{
		return PROTOCOL_STEAM;
	}
}

int CBaseServer::GetChallengeNr (netadr_t &adr)
{
	uint64 challenge = ((uint64)adr.GetIPNetworkByteOrder() << 32) + m_CurrentRandomNonce;
	CRC32_t hash;
	CRC32_Init( &hash );
	CRC32_ProcessBuffer( &hash, &challenge, sizeof(challenge) );
	CRC32_Final( &hash );
	return (int)hash;
}

void CBaseServer::GetNetStats( float &avgIn, float &avgOut )
{
	avgIn = avgOut = 0.0f;

	for (int i = 0; i < m_Clients.Count(); i++ )
	{
		CBaseClient	*cl = m_Clients[ i ];

		// Fake clients get killed in here.
		if ( cl->IsFakeClient() )
			continue;
	
		if ( !cl->IsConnected() )
			continue;

		INetChannel *netchan = cl->GetNetChannel();

		avgIn += netchan->GetAvgData(FLOW_INCOMING);
		avgOut += netchan->GetAvgData(FLOW_OUTGOING);
	}
}

void CBaseServer::CalculateCPUUsage( void )
{
	if ( !sv_stats.GetBool() )
	{
		return;
	}

	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	float curtime = Sys_FloatTime();

	if ( m_fStartTime == 0 )
	// record when we started
	{
		m_fStartTime = curtime;
	}

	if( curtime > m_fLastCPUCheckTime + 1 )
	// only do this every 1 second
	{
#if defined ( _WIN32 ) 
		static float lastAvg=0;
		static __int64 lastTotalTime=0,lastNow=0;

		HANDLE handle;
		FILETIME creationTime, exitTime, kernelTime, userTime, nowTime;
 		__int64 totalTime,now;
			
		handle = GetCurrentProcess();

		// get CPU time
		GetProcessTimes (handle, &creationTime, &exitTime,
						&kernelTime, &userTime);
		GetSystemTimeAsFileTime(&nowTime);

		if ( lastNow == 0 )
		{
			memcpy(&lastNow, &creationTime, sizeof(__int64));
		}

		memcpy(&totalTime, &userTime, sizeof(__int64));
		memcpy(&now, &kernelTime, sizeof(__int64));
		totalTime+=now;

		memcpy(&now, &nowTime, sizeof(__int64));

		m_fCPUPercent = (double)(totalTime-lastTotalTime)/(double)(now-lastNow);
		
		// now save this away for next time
		if ( curtime > lastAvg+5 ) 
		// only do it every 5 seconds, so we keep a moving average
		{
			memcpy(&lastNow,&nowTime,sizeof(__int64));
			memcpy(&lastTotalTime,&totalTime,sizeof(__int64));
			lastAvg=m_fLastCPUCheckTime;
		}
#elif defined ( POSIX )
		static struct rusage s_lastUsage;
		static float s_lastAvg = 0;
		struct rusage currentUsage;

		if ( getrusage( RUSAGE_SELF, &currentUsage ) == 0 )
		{
			double flTimeDiff = (double)( currentUsage.ru_utime.tv_sec - s_lastUsage.ru_utime.tv_sec ) +
				(double)(( currentUsage.ru_utime.tv_usec - s_lastUsage.ru_utime.tv_usec ) / 1000000); 
			m_fCPUPercent = flTimeDiff / ( m_fLastCPUCheckTime - s_lastAvg );

			// now save this away for next time
			if( m_fLastCPUCheckTime > s_lastAvg + 5) 
			{
				s_lastUsage = currentUsage;
				s_lastAvg = m_fLastCPUCheckTime;
			}
		}

		// limit checking :)
		if( m_fCPUPercent > 0.9999 )
			m_fCPUPercent = 0.9999;
		if( m_fCPUPercent < 0 )
			m_fCPUPercent = 0;
#else
#error
#endif
		m_fLastCPUCheckTime = curtime; 
	}
}


//-----------------------------------------------------------------------------
// Purpose: Prepare for level transition, etc.
//-----------------------------------------------------------------------------
void CBaseServer::InactivateClients( void )
{
	for (int i = 0; i < m_Clients.Count(); i++ )
	{
		CBaseClient	*cl = m_Clients[ i ];

		// Fake clients get killed in here.
#if defined( REPLAY_ENABLED )
		if ( cl->IsFakeClient() && !cl->IsHLTV() && !cl->IsReplay() )
#else
		if ( cl->IsFakeClient() && !cl->IsHLTV() )
#endif
		{
			// If we don't do this, it'll have a bunch of extra steam IDs for unauthenticated users.
			Steam3Server().NotifyClientDisconnect( cl );
			cl->Clear();
			continue;
		}
		else if ( !cl->IsConnected() )
		{
			continue;
		}

		cl->Inactivate();
	}
}

void CBaseServer::ReconnectClients( void )
{
	for (int i=0 ; i< m_Clients.Count() ; i++ )
	{
		CBaseClient *cl = m_Clients[i];
		
		if ( cl->IsConnected() )
		{
			cl->m_nSignonState = SIGNONSTATE_CONNECTED;
			NET_SignonState signon( cl->m_nSignonState, -1 );
			cl->SendNetMsg( signon );
		}
	}
}

/*
==================
SV_CheckTimeouts

If a packet has not been received from a client in sv_timeout.GetFloat()
seconds, drop the conneciton.

When a client is normally dropped, the CSVClient goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
void CBaseServer::CheckTimeouts (void)
{
	VPROF_BUDGET( "CBaseServer::CheckTimeouts", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	// Don't timeout in _DEBUG builds
	int i;

#if !defined( _DEBUG )
		
	for (i=0 ; i< m_Clients.Count() ; i++ )
	{
		IClient	*cl = m_Clients[ i ];
		
		if ( cl->IsFakeClient() || !cl->IsConnected() )
			continue;

		INetChannel *netchan = cl->GetNetChannel();

		if ( !netchan )
			continue;

	

		if ( netchan->IsTimedOut() )
		{
			cl->Disconnect( CLIENTNAME_TIMED_OUT, cl->GetClientName() );
		}
	}
#endif

	for (i=0 ; i< m_Clients.Count() ; i++ )
	{
		IClient	*cl = m_Clients[ i ];
		
		if ( cl->IsFakeClient() || !cl->IsConnected() )
			continue;
		
		if ( cl->GetNetChannel() && cl->GetNetChannel()->IsOverflowed() )
		{
			cl->Disconnect( "Client %d overflowed reliable channel.", i );
		}
	}
}

// ==================
// check if clients update thier user setting (convars) and call 
// ==================
void CBaseServer::UpdateUserSettings(void)
{
	VPROF_BUDGET( "CBaseServer::UpdateUserSettings", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	for (int i=0 ; i< m_Clients.Count() ; i++ )
	{
		CBaseClient	*cl = m_Clients[ i ];

		cl->CheckFlushNameChange();

		if ( cl->m_bConVarsChanged )
		{
			cl->UpdateUserSettings();
		}
	}
}

// ==================
// check if clients need the serverinfo packet sent
// ==================
void CBaseServer::SendPendingServerInfo()
{
	VPROF_BUDGET( "CBaseServer::SendPendingServerInfo", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	for (int i=0 ; i< m_Clients.Count() ; i++ )
	{
		CBaseClient	*cl = m_Clients[ i ];

		if ( cl->m_bSendServerInfo )
		{
			cl->SendServerInfo();
		}
	}
}

// compresses a packed entity, returns data & bits
const char *CBaseServer::CompressPackedEntity(ServerClass *pServerClass, const char *data, int &bits)
{
	ALIGN4 static char s_packedData[MAX_PACKEDENTITY_DATA] ALIGN4_POST;

	bf_write writeBuf( "CompressPackedEntity", s_packedData, sizeof( s_packedData ) );

	const void *pBaselineData = NULL;
	int nBaselineBits = 0;

	Assert( pServerClass != NULL );
		
	GetClassBaseline( pServerClass, &pBaselineData, &nBaselineBits );
	nBaselineBits *= 8;

	Assert( pBaselineData != NULL );

	SendTable_WriteAllDeltaProps(
		pServerClass->m_pTable,
		pBaselineData,
		nBaselineBits,
		data,
		bits,
		-1,
		&writeBuf );
	
	//overwrite in bits with out bits
	bits = writeBuf.GetNumBitsWritten();

	return s_packedData;
}

// uncompresses a 
const char* CBaseServer::UncompressPackedEntity(PackedEntity *pPackedEntity, int &bits)
{
	UnpackedDataCache_t *pdc = framesnapshotmanager->GetCachedUncompressedEntity( pPackedEntity );

	if ( pdc->bits > 0 )
	{
		// found valid uncompressed version in cache
		bits= pdc->bits;
		return pdc->data;
	}

	// not in cache, so uncompress it

	const void *pBaseline;
	int nBaselineBytes = 0;

	GetClassBaseline( pPackedEntity->m_pServerClass, &pBaseline, &nBaselineBytes );

	Assert( pBaseline != NULL );

	// store this baseline in u.m_pUpdateBaselines
	bf_read oldBuf( "UncompressPackedEntity1", pBaseline, nBaselineBytes );
	bf_read newBuf( "UncompressPackedEntity2", pPackedEntity->GetData(), Bits2Bytes(pPackedEntity->GetNumBits()) );
	bf_write outBuf( "UncompressPackedEntity3", pdc->data, MAX_PACKEDENTITY_DATA );

	Assert( pPackedEntity->m_pClientClass );

	RecvTable_MergeDeltas( 
		pPackedEntity->m_pClientClass->m_pRecvTable,
		&oldBuf,
		&newBuf,
		&outBuf );

	bits = pdc->bits = outBuf.GetNumBitsWritten();
		
	return pdc->data;
}

/*
================
SV_CheckProtocol

Make sure connecting client is using proper protocol
================
*/
bool CBaseServer::CheckProtocol( netadr_t &adr, int nProtocol, int clientChallenge )
{
	if ( nProtocol != PROTOCOL_VERSION )
	{
		// Client is newer than server
		if ( nProtocol > PROTOCOL_VERSION )
		{
			RejectConnection( adr, clientChallenge, "#GameUI_ServerRejectOldProtocol" );
		}
		else
		// Server is newer than client
		{
			RejectConnection( adr, clientChallenge, "#GameUI_ServerRejectNewProtocol" );
		}
		return false;
	}

	// Success
	return true;
}

/*
================
SV_CheckKeyInfo

Determine if client is outside appropriate address range
================
*/
bool CBaseServer::CheckChallengeType( CBaseClient * client, int nNewUserID, netadr_t & adr, int nAuthProtocol, const char *pchLogonCookie, int cbCookie, int clientChallenge )
{
	if ( AllowDebugDedicatedServerOutsideSteam() )
		return true;

	// Check protocol ID
	if ( ( nAuthProtocol <= 0 ) || ( nAuthProtocol > PROTOCOL_LASTVALID ) )
	{
		RejectConnection( adr, clientChallenge, "#GameUI_ServerRejectInvalidConnection");
		return false;
	}

	if ( ( nAuthProtocol == PROTOCOL_HASHEDCDKEY ) && (Q_strlen( pchLogonCookie ) <= 0 ||  Q_strlen(pchLogonCookie) != 32 ) )
	{
		RejectConnection( adr, clientChallenge, "#GameUI_ServerRejectInvalidCertLen" );
		return false;
	}

	Assert( !IsReplay() );

	if ( IsHLTV() )
	{
		// Don't authenticate spectators or add them to the
		// player list in the singleton Steam3Server()
		Assert( nAuthProtocol == PROTOCOL_HASHEDCDKEY );
		Assert( !client->m_SteamID.IsValid() );
	}
	else if ( nAuthProtocol == PROTOCOL_STEAM )
	{
		// Dev hack to allow 360/Steam PC cross platform play
// 		int ip0 = 207;
// 		int ip1 = 173;
// 		int ip2 = 179;
// 		int ip3Min = 230;
// 		int ip3Max = 245;
// 
// 		if ( adr.ip[0] == ip0 &&
// 			adr.ip[1] == ip1 &&
// 			adr.ip[2] == ip2 &&
// 			adr.ip[3] >= ip3Min &&
// 			adr.ip[3] <= ip3Max )
// 		{
// 			return true;
// 		}

		client->SetSteamID( CSteamID() ); // set an invalid SteamID

		// Convert raw certificate back into data
		if ( cbCookie <= 0 || cbCookie >= STEAM_KEYSIZE )
		{
			RejectConnection( adr, clientChallenge, "#GameUI_ServerRejectInvalidSteamCertLen" );
			return false;
		}
		netadr_t checkAdr = adr;
		if ( adr.GetType() == NA_LOOPBACK || adr.IsLocalhost() )
		{
			checkAdr.SetIP( net_local_adr.GetIPHostByteOrder() );
		}

		if ( !Steam3Server().NotifyClientConnect( client, nNewUserID, checkAdr, pchLogonCookie, cbCookie ) 
			&& !Steam3Server().BLanOnly() ) // the userID isn't alloc'd yet so we need to fill it in manually
		{
			RejectConnection( adr, clientChallenge, "#GameUI_ServerRejectSteam" );
			return false;
		}

		//
		// Any rejections below this must call SendUserDisconnect
		//

		// Now that we have auth'd with steam, client->GetSteamID() is now valid and we can verify against the GC lobby
		bool bHasGCLobby = g_iServerGameDLLVersion >= 8 && serverGameDLL->GetServerGCLobby();
		if ( bHasGCLobby )
		{
			if ( !serverGameDLL->GetServerGCLobby()->SteamIDAllowedToConnect( client->m_SteamID ) )
			{
				ISteamGameServer *pSteamGameServer = Steam3Server().SteamGameServer();
				if ( pSteamGameServer )
					pSteamGameServer->SendUserDisconnect( client->m_SteamID);

				RejectConnection( adr, clientChallenge, "#GameUI_ServerRejectMustUseMatchmaking" );
				return false;
			}
		}
	}
	else
	{
		if ( !Steam3Server().NotifyLocalClientConnect( client ) ) // the userID isn't alloc'd yet so we need to fill it in manually
		{
			RejectConnection( adr, clientChallenge, "#GameUI_ServerRejectGS" );
			return false;
		}
	}

	return true;
}

bool CBaseServer::CheckIPRestrictions( const netadr_t &adr, int nAuthProtocol )
{
	// Determine if client is outside appropriate address range
	if ( adr.IsLoopback() )
		return true;

	// X360TBD: network
	if ( IsX360() )
		return true;

	// allow other users if they're on the same ip range
	if ( Steam3Server().BLanOnly() )
	{
		// allow connection, if client is in the same subnet 
		if ( adr.CompareClassBAdr( net_local_adr ) )
			return true;

		// allow connection, if client has a private IP
		if ( adr.IsReservedAdr() )
			return true;
		
		// reject connection
		return false;
	}

	return true;
}

void CBaseServer::SetMasterServerRulesDirty()
{
	m_bMasterServerRulesDirty = true;
}

bool CBaseServer::CheckPassword( netadr_t &adr, const char *password, const char *name )
{
	const char *server_password = GetPassword();

	if ( !server_password )
		return true;	// no password set

	if ( adr.IsLocalhost() || adr.IsLoopback() )
	{
		return true; // local client can always connect
	}

	int iServerPassLen = Q_strlen(server_password);

	if ( iServerPassLen != Q_strlen(password) )
	{
		return false; // different length cannot be equal
	}

	if ( Q_strncmp( password, server_password, iServerPassLen ) == 0)
	{
		return true; // passwords are equal
	}

	return false; // all test failed
}

float CBaseServer::GetTime() const
{
	return m_nTickCount * m_flTickInterval;
}

float CBaseServer::GetFinalTickTime() const
{
	return (m_nTickCount + (host_frameticks - host_currentframetick)) * m_flTickInterval;
}

void CBaseServer::DisconnectClient(IClient *client, const char *reason )
{
	client->Disconnect( reason );
}

void CBaseServer::Clear( void )
{
	if ( m_StringTables )
	{
		m_StringTables->RemoveAllTables();
		m_StringTables = NULL;
	}

	m_pInstanceBaselineTable = NULL;
	m_pLightStyleTable = NULL;
	m_pUserInfoTable = NULL;
	m_pServerStartupTable = NULL;

	m_State = ss_dead;
	
	m_nTickCount = 0;
	
	Q_memset( m_szMapname, 0, sizeof( m_szMapname ) );
	Q_memset( m_szSkyname, 0, sizeof( m_szSkyname ) );

	V_memset( worldmapMD5.bits, 0, MD5_DIGEST_LENGTH );

	MEM_ALLOC_CREDIT();

	// Use a different limit on the signon buffer, so we can save some memory in SP (for xbox).
	if ( IsMultiplayer() || IsDedicated() )
	{
		m_SignonBuffer.EnsureCapacity( NET_MAX_PAYLOAD );
	}
	else
	{
		m_SignonBuffer.EnsureCapacity( 16384 );
	}
	
	m_Signon.StartWriting( m_SignonBuffer.Base(), m_SignonBuffer.Count() );
	m_Signon.SetDebugName( "m_Signon" );

	serverclasses = 0;
	serverclassbits = 0;

	m_LastRandomNonce = m_CurrentRandomNonce = 0;
	m_flPausedTimeEnd = -1.f;
}

/*
================
SV_RejectConnection

Rejects connection request and sends back a message
================
*/
void CBaseServer::RejectConnection( const netadr_t &adr, int clientChallenge, const char *s )
{
	ALIGN4 char		msg_buffer[MAX_ROUTABLE_PAYLOAD] ALIGN4_POST;
	bf_write	msg( msg_buffer, sizeof(msg_buffer) );

	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteByte( S2C_CONNREJECT );
	msg.WriteLong( clientChallenge );
	msg.WriteString( s );

	NET_SendPacket ( NULL, m_Socket, adr, msg.GetData(), msg.GetNumBytesWritten() );
}

void CBaseServer::SetPaused(bool paused)
{
	if ( !IsPausable() )
	{
		return;
	}

	if ( !IsActive() )
		return;

	if ( paused )
	{
		m_State = ss_paused;
	}
	else
	{
		m_State = ss_active;
	}

	SVC_SetPause setpause( paused );
	BroadcastMessage( setpause );
}

//-----------------------------------------------------------------------------
// Purpose: General initialization of the server
//-----------------------------------------------------------------------------
void CBaseServer::Init (bool bIsDedicated)
{
	m_nMaxclients = 0;
	m_nSpawnCount = 0;
	m_nUserid = 1;
	m_nNumConnections = 0;
	m_bIsDedicated = bIsDedicated;
	m_Socket = NS_SERVER;	
	
	m_Signon.SetDebugName( "m_Signon" );
	
	g_pCVar->InstallGlobalChangeCallback( ServerNotifyVarChangeCallback );
	SetMasterServerRulesDirty();
	
	Clear();
}

INetworkStringTable *CBaseServer::GetInstanceBaselineTable( void )
{
	if ( m_pInstanceBaselineTable == NULL )
	{
		m_pInstanceBaselineTable = m_StringTables->FindTable( INSTANCE_BASELINE_TABLENAME );
	}

	return m_pInstanceBaselineTable;
}

INetworkStringTable *CBaseServer::GetLightStyleTable( void )
{
	if ( m_pLightStyleTable == NULL )
	{
		m_pLightStyleTable= m_StringTables->FindTable( LIGHT_STYLES_TABLENAME );
	}

	return m_pLightStyleTable;
}

INetworkStringTable *CBaseServer::GetUserInfoTable( void )
{
	if ( m_pUserInfoTable == NULL )
	{
		if ( m_StringTables == NULL )
		{
			return NULL;
		}
		m_pUserInfoTable = m_StringTables->FindTable( USER_INFO_TABLENAME );
	}

	return m_pUserInfoTable;
}

bool CBaseServer::GetClassBaseline( ServerClass *pClass, void const **pData, int *pDatalen )
{
	if ( sv_instancebaselines.GetInt() )
	{
		ErrorIfNot( pClass->m_InstanceBaselineIndex != INVALID_STRING_INDEX,
			("SV_GetInstanceBaseline: missing instance baseline for class '%s'", pClass->m_pNetworkName)
		);
		
		AUTO_LOCK( g_svInstanceBaselineMutex );
		*pData = GetInstanceBaselineTable()->GetStringUserData(
			pClass->m_InstanceBaselineIndex,
			pDatalen );
		
		return *pData != NULL;
	}
	else
	{
		static char dummy[1] = {0};
		*pData = dummy;
		*pDatalen = 1;
		return true;
	}
}


bool CBaseServer::ShouldUpdateMasterServer()
{
	// If the game server itself is ever running, then it's the one who gets to update the master server.
	// (SourceTV will not update it in this case).
	return true;
}


void CBaseServer::CheckMasterServerRequestRestart()
{
	if ( !Steam3Server().SteamGameServer() || !Steam3Server().SteamGameServer()->WasRestartRequested() )
		return;
	
	// Connection was rejected by the HLMaster (out of date version)

	// hack, vgui console looks for this string; 
	Msg("%cMasterRequestRestart\n", 3);

#ifndef _WIN32
	if (CommandLine()->FindParm(AUTO_RESTART))
	{
		Msg("Your server will be restarted on map change.\n");
		Log("Your server will be restarted on map change.\n");
		SetRestartOnLevelChange( true );
	}
#endif
	
	if ( sv.IsDedicated() ) // under linux assume steam
	{
		Msg("Your server needs to be restarted in order to receive the latest update.\n");
		Log("Your server needs to be restarted in order to receive the latest update.\n");
	}
	else
	{
		Msg("Your server is out of date.  Please update and restart.\n");
	}
}


void CBaseServer::UpdateMasterServer()
{
	VPROF_BUDGET( "CBaseServer::UpdateMasterServer", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	if ( !ShouldUpdateMasterServer() )
		return;

	if ( !Steam3Server().SteamGameServer() )
		return;
	
	// Only update every so often.
	double flCurTime = Plat_FloatTime();
	if ( flCurTime - m_flLastMasterServerUpdateTime < MASTER_SERVER_UPDATE_INTERVAL )
		return;

	m_flLastMasterServerUpdateTime = flCurTime;


	ForwardPacketsFromMasterServerUpdater();
	CheckMasterServerRequestRestart();
	

	if ( NET_IsDedicated() && sv_region.GetInt() == -1 )
    {
		sv_region.SetValue( 255 ); // HACK!HACK! undo me once we want to enforce regions

        //Log_Printf( "You must set sv_region in your server.cfg or use +sv_region on the command line\n" );
		//Con_Printf( "You must set sv_region in your server.cfg or use +sv_region on the command line\n" );
        //Cbuf_AddText( "quit\n" );
        //return;
    }

	static bool bUpdateMasterServers = !CommandLine()->FindParm( "-nomaster" );
	if ( !bUpdateMasterServers )
		return;

	bool bActive = IsActive() && IsMultiplayer();
	if ( serverGameDLL && serverGameDLL->ShouldHideServer() )
		bActive = false;
	
	Steam3Server().SteamGameServer()->EnableHeartbeats( bActive );

	if ( !bActive )
		return;

	UpdateMasterServerRules();
	UpdateMasterServerPlayers();
	Steam3Server().SendUpdatedServerDetails();
}


void CBaseServer::UpdateMasterServerRules()
{
	// Only do this if the rules vars are dirty.
	if ( !m_bMasterServerRulesDirty )
		return;

	ISteamGameServer *pUpdater = Steam3Server().SteamGameServer();
	if ( !pUpdater )
		return;

	pUpdater->ClearAllKeyValues();
	
	// Need to respond with game directory, game name, and any server variables that have been set that
	//  effect rules.  Also, probably need a hook into the .dll to respond with additional rule information.
	ConCommandBase *var;
	for ( var = g_pCVar->GetCommands() ; var ; var=var->GetNext() )
	{
		if ( !(var->IsFlagSet( FCVAR_NOTIFY ) ) )
			continue;
		if ( var->IsCommand() )
			continue;

		ConVar *pConVar = static_cast< ConVar* >( var );
		if ( !pConVar )
			continue;

		SetMasterServerKeyValue( pUpdater, pConVar );
	}

	if (  Steam3Server().SteamGameServer() )
	{
		RecalculateTags();
	}

	// Ok.. it's all updated, only send incremental updates now until we decide they're all dirty.
	m_bMasterServerRulesDirty = false;
}


void CBaseServer::ForwardPacketsFromMasterServerUpdater()
{
	ISteamGameServer *p = Steam3Server().SteamGameServer();
	if ( !p )
		return;
	
	while ( 1 )
	{
		uint32 netadrAddress;
		uint16 netadrPort;
		unsigned char packetData[16 * 1024];
 		int len = p->GetNextOutgoingPacket( packetData, sizeof( packetData ), &netadrAddress, &netadrPort );
		if ( len <= 0 )
			break;
		
		// Send this packet for them..
		netadr_t adr( netadrAddress, netadrPort );
		NET_SendPacket( NULL, m_Socket, adr, packetData, len );
	}
}


/*
=================
SV_ReadPackets

Read's packets from clients and executes messages as appropriate.
=================
*/

void CBaseServer::RunFrame( void )
{
	VPROF_BUDGET( "CBaseServer::RunFrame", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "CBaseServer::RunFrame" );

	NET_ProcessSocket( m_Socket, this );	

#ifdef LINUX
	// Process the linux sv lan port if it's open.
	if ( NET_GetUDPPort( NS_SVLAN ) )
		NET_ProcessSocket( NS_SVLAN, this );
#endif

	CheckTimeouts();	// drop clients that timeed out

	UpdateUserSettings(); // update client settings 
	
	SendPendingServerInfo(); // send outstanding signon packets after ALL user settings have been updated

	CalculateCPUUsage(); // update CPU usage

	UpdateMasterServer();

	if ( m_flLastRandomNumberGenerationTime < 0 || (m_flLastRandomNumberGenerationTime + CHALLENGE_NONCE_LIFETIME) < g_ServerGlobalVariables.realtime  )
	{
		m_LastRandomNonce = m_CurrentRandomNonce;

		// RandomInt maps a uniform distribution on the interval [0,INT_MAX], so make two calls to get the random number.
		// RandomInt will always return the minimum value if the difference in min and max is greater than or equal to INT_MAX.
		m_CurrentRandomNonce = ( ( (uint32)RandomInt( 0, 0xFFFF ) ) << 16 ) | RandomInt( 0, 0xFFFF );
		m_flLastRandomNumberGenerationTime = g_ServerGlobalVariables.realtime;
	}

	// Timed pause - resume game when time expires
	if ( m_flPausedTimeEnd >= 0.f && m_State == ss_paused && Sys_FloatTime() >= m_flPausedTimeEnd )
	{
		SetPausedForced( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *adr - 
//			*pslot - 
//			**ppClient - 
// Output : int
//-----------------------------------------------------------------------------
CBaseClient * CBaseServer::GetFreeClient( netadr_t &adr )
{
	CBaseClient *freeclient = NULL;
	
	for ( int slot = 0 ; slot < m_Clients.Count() ; slot++ )
	{
		CBaseClient *client = m_Clients[slot];

		if ( client->IsFakeClient() )
			continue;

		if ( client->IsConnected() )
		{
			if ( adr.CompareAdr ( client->m_NetChannel->GetRemoteAddress() ) )
			{
				ConMsg ( "%s:reconnect\n", adr.ToString() );

				RemoveClientFromGame( client );

				// perform a silent netchannel shutdown, don't send disconnect msg
				client->m_NetChannel->Shutdown( NULL );
				client->m_NetChannel = NULL;
		
				client->Clear();
				return client;
			}
		}
		else
		{
			// use first found free slot 
			if ( !freeclient )
			{
				freeclient = client; 
			}
		}
	}

	if ( !freeclient )
	{
		int count = m_Clients.Count();

		if ( count >= m_nMaxclients )
		{
			return NULL; // server full
		}

		// we have to create a new client slot
		freeclient = CreateNewClient( count );
		
		m_Clients.AddToTail( freeclient );
	}
	
	// Success
	return freeclient;
}

void CBaseServer::SendClientMessages ( bool bSendSnapshots )
{
	VPROF_BUDGET( "SendClientMessages", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	
	for (int i=0; i< m_Clients.Count(); i++ )
	{
		CBaseClient* client = m_Clients[i];
		
		// Update Host client send state...
		if ( !client->ShouldSendMessages() )
			continue;

		// Connected, but inactive, just send reliable, sequenced info.
		if ( client->m_NetChannel )
		{
			client->m_NetChannel->Transmit();
			client->UpdateSendState();
		}
		else
		{
			Msg("Client has no netchannel.\n");
		}
	}
}

CBaseClient *CBaseServer::CreateFakeClient( const char *name )
{
	netadr_t adr; // it's an empty address

	CBaseClient *fakeclient = GetFreeClient( adr );
				
	if ( !fakeclient )
	{
		// server is full
		return NULL;		
	}

	INetChannel *netchan = NULL;
	if ( sv_stressbots.GetBool() )
	{
		netadr_t adrNull( 0, 0 ); // 0.0.0.0:0 signifies a bot. It'll plumb all the way down to winsock calls but it won't make them.
		netchan = NET_CreateNetChannel( m_Socket, &adrNull, adrNull.ToString(), fakeclient, true );
	}

	// a NULL netchannel signals a fakeclient
	m_nUserid = GetNextUserID();
	m_nNumConnections++;

	fakeclient->SetReportThisFakeClient( m_bReportNewFakeClients );
	fakeclient->Connect( name, m_nUserid, netchan, true, 0 );

	// fake some cvar settings
	//fakeclient->SetUserCVar( "name", name ); // set already by Connect()
	fakeclient->SetUserCVar( "rate", "30000" );
	fakeclient->SetUserCVar( "cl_updaterate", "20" );
	fakeclient->SetUserCVar( "cl_interp_ratio", "1.0" );
	fakeclient->SetUserCVar( "cl_interp", "0.1" );
	fakeclient->SetUserCVar( "cl_interpolate", "0" );
	fakeclient->SetUserCVar( "cl_predict", "1" );
	fakeclient->SetUserCVar( "cl_predictweapons", "1" );
	fakeclient->SetUserCVar( "cl_lagcompensation", "1" );
	fakeclient->SetUserCVar( "closecaption","0" );
	fakeclient->SetUserCVar( "english", "1" );

	fakeclient->SetUserCVar( "cl_clanid", "0" );
	fakeclient->SetUserCVar( "cl_team", "blue" );
	fakeclient->SetUserCVar( "hud_classautokill", "1" );
	fakeclient->SetUserCVar( "tf_medigun_autoheal", "0" );
	fakeclient->SetUserCVar( "cl_autorezoom", "1" );
	fakeclient->SetUserCVar( "fov_desired", "75" );
	fakeclient->SetUserCVar( "tf_remember_lastswitched", "0" );

	fakeclient->SetUserCVar( "cl_autoreload", "0" );
	fakeclient->SetUserCVar( "tf_remember_activeweapon", "0" );
	fakeclient->SetUserCVar( "hud_combattext", "0" );
	fakeclient->SetUserCVar( "cl_flipviewmodels", "0" );

	// create client in game.dll
	fakeclient->ActivatePlayer();

	fakeclient->m_nSignonTick = m_nTickCount;
			
	return fakeclient;
}

void CBaseServer::Shutdown( void )
{
	if ( !IsActive() )
		return;

	m_State = ss_dead;

	// Only drop clients if we have not cleared out entity data prior to this.
	for(  int i=m_Clients.Count()-1; i>=0; i-- )
	{
		CBaseClient * cl = m_Clients[ i ];
		if ( cl->IsConnected() )
		{
			cl->Disconnect( "Server shutting down" );
		}
		else
		{
			// free any memory do this out side here in case the reason the server is shutting down 
			// is because the listen server client typed disconnect, in which case we won't call
			// cl->DropClient, but the client might have some frame snapshot references left over, etc.
			cl->Clear();	
		}

		delete cl;

		m_Clients.Remove( i );
	}

	// Let drop messages go out
	Sys_Sleep( 100 );

	// clear everything
	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: Sends text to all active clients
// Input  : *fmt -
//			... -
//-----------------------------------------------------------------------------
void CBaseServer::BroadcastPrintf (const char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,fmt);
	Q_vsnprintf (string, sizeof( string ), fmt,argptr);
	va_end (argptr);

	SVC_Print print( string );
	BroadcastMessage( print );	
}

void CBaseServer::BroadcastMessage( INetMessage &msg, bool onlyActive, bool reliable )
{
	for ( int i = 0; i < m_Clients.Count(); i++ )
	{
		CBaseClient *cl = m_Clients[ i ];

		if ( (onlyActive && !cl->IsActive()) || !cl->IsSpawned() )
		{
			continue;
		}

		if ( !cl->SendNetMsg( msg, reliable ) )
		{
			if ( msg.IsReliable() || reliable )
			{
				DevMsg( "BroadcastMessage: Reliable broadcast message overflow for client %s", cl->GetClientName() );
			}
		}
	}
}

void CBaseServer::BroadcastMessage( INetMessage &msg, IRecipientFilter &filter )
{
	if ( filter.IsInitMessage() )
	{
		// This really only applies to the first player to connect, but that works in single player well enought
		if ( IsActive() )
		{
			ConDMsg( "SV_BroadcastMessage: Init message being created after signon buffer has been transmitted\n" );
		}
		
		if ( !msg.WriteToBuffer( m_Signon ) )
		{
			Sys_Error( "SV_BroadcastMessage: Init message would overflow signon buffer!\n" );
			return;
		}
	}
	else
	{
		msg.SetReliable( filter.IsReliable() );

		int num = filter.GetRecipientCount();
	
		for ( int i = 0; i < num; i++ )
		{
			int index = filter.GetRecipientIndex( i );

			if ( index < 1 || index > m_Clients.Count() )
			{
				Msg( "SV_BroadcastMessage:  Recipient Filter for message type %i (reliable: %s, init: %s) with bogus client index (%i) in list of %i clients\n", 
						msg.GetType(), 
						filter.IsReliable() ? "yes" : "no",
						filter.IsInitMessage() ? "yes" : "no",
						index, num );

				if ( msg.IsReliable() )
					Host_Error( "Reliable message (type %i) discarded.", msg.GetType() );

				continue;
			}

			CBaseClient *cl = m_Clients[ index - 1 ];

			if ( !cl->IsSpawned() )
			{
				continue;
			}

			if ( !cl->SendNetMsg( msg ) )
			{
				if ( msg.IsReliable() )
				{
					DevMsg( "BroadcastMessage: Reliable filter message overflow for client %s", cl->GetClientName() );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Writes events to the client's network buffer
// Input  : *cl -
//			*pack -
//			*msg -
//-----------------------------------------------------------------------------
static ConVar sv_debugtempentities( "sv_debugtempentities", "0", 0, "Show temp entity bandwidth usage." );

static bool CEventInfo_LessFunc( CEventInfo * const &lhs, CEventInfo * const &rhs )
{
	return lhs->classID < rhs->classID;
}

void CBaseServer::WriteTempEntities( CBaseClient *client, CFrameSnapshot *pCurrentSnapshot, CFrameSnapshot *pLastSnapshot, bf_write &buf, int ev_max )
{
	VPROF_BUDGET( "CBaseServer::WriteTempEntities", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	ALIGN4 char data[NET_MAX_PAYLOAD] ALIGN4_POST;
	SVC_TempEntities msg;
	msg.m_DataOut.StartWriting( data, sizeof(data) );
	bf_write &buffer = msg.m_DataOut; // shortcut
	
	CFrameSnapshot *pSnapshot;
	CEventInfo *pLastEvent = NULL;

	bool bDebug = sv_debugtempentities.GetBool();

	// limit max entities to field bit length
	ev_max = min( ev_max, ((1<<CEventInfo::EVENT_INDEX_BITS)-1) );

	if ( pLastSnapshot )
	{
		pSnapshot = pLastSnapshot->NextSnapshot();
	} 
	else
	{
		pSnapshot = pCurrentSnapshot;
	}

	CUtlRBTree< CEventInfo * >	sorted( 0, ev_max, CEventInfo_LessFunc );

	// Build list of events sorted by send table classID (makes the delta work better in cases with a lot of the same message type )
	while ( pSnapshot && ((int)sorted.Count() < ev_max) )
	{
		for( int i = 0; i < pSnapshot->m_nTempEntities; ++i )
		{
			CEventInfo *event = pSnapshot->m_pTempEntities[ i ];

			if ( client->IgnoreTempEntity( event ) )
				continue; // event is not seen by this player
			
			sorted.Insert( event );
			// More space still
			if ( (int)sorted.Count() >= ev_max )
				break;	
		}

		// stop, we reached our current snapshot
		if ( pSnapshot == pCurrentSnapshot )
			break; 

		// got to next snapshot
		pSnapshot = framesnapshotmanager->NextSnapshot( pSnapshot );
	}

	if ( sorted.Count() <= 0 )
		return;

	for ( int i = sorted.FirstInorder(); 
		i != sorted.InvalidIndex(); 
		i = sorted.NextInorder( i ) )
	{
		CEventInfo *event = sorted[ i ];

		if ( event->fire_delay == 0.0f )
		{
			buffer.WriteOneBit( 0 );
		} 
		else
		{
			buffer.WriteOneBit( 1 );
			buffer.WriteSBitLong( event->fire_delay*100.0f, 8 );
		}

		if ( pLastEvent && 
			pLastEvent->classID == event->classID )
		{
			buffer.WriteOneBit( 0 ); // delta against last temp entity

			int startBit = bDebug ? buffer.GetNumBitsWritten() : 0;

			SendTable_WriteAllDeltaProps( event->pSendTable, 
				pLastEvent->pData,
				pLastEvent->bits,
				event->pData,
				event->bits,
				-1,
				&buffer );

			if ( bDebug )
			{
				int length = buffer.GetNumBitsWritten() - startBit;
				DevMsg("TE %s delta bits: %i\n", event->pSendTable->GetName(), length );
			}
		}
		else
		{
			 // full update, just compressed against zeros in MP

			buffer.WriteOneBit( 1 );

			int startBit = bDebug ? buffer.GetNumBitsWritten() : 0;

			buffer.WriteUBitLong( event->classID, GetClassBits() );

			if ( IsMultiplayer() )
			{
				SendTable_WriteAllDeltaProps( event->pSendTable, 
					NULL,	// will write only non-zero elements
					0,
					event->pData,
					event->bits,
					-1,
					&buffer );
			}
			else
			{
				// write event with zero properties
				buffer.WriteBits( event->pData, event->bits );
			}

			if ( bDebug )
			{
				int length = buffer.GetNumBitsWritten() - startBit;
				DevMsg("TE %s full bits: %i\n", event->pSendTable->GetName(), length );
			}
		}

		if ( IsMultiplayer() )
		{
			// in single player, don't used delta compression, lastEvent remains NULL
			pLastEvent = event;
		}
	}

	// set num entries
	msg.m_nNumEntries = sorted.Count();
	msg.WriteToBuffer( buf );
}

void CBaseServer::SetMaxClients( int number )
{
	m_nMaxclients = clamp( number, 1, ABSOLUTE_PLAYER_LIMIT );
}

extern ConVar tv_enable;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseServer::RecalculateTags( void )
{
	if ( IsHLTV() || IsReplay() )
		return;

	// We're going to modify the sv_tags convar here, which will cause this to be called again. Prevent recursion.
	static bool bRecalculatingTags = false;
	if ( bRecalculatingTags )
		return;

	bRecalculatingTags = true;

	// Games without this interface will have no tagged cvars besides "increased_maxplayers"
	if ( serverGameTags )
	{
		KeyValues *pKV = new KeyValues( "GameTags" );

		serverGameTags->GetTaggedConVarList( pKV );

		KeyValues *p = pKV->GetFirstSubKey();
		while ( p )
		{
			ConVar *pConVar = g_pCVar->FindVar( p->GetString("convar") );
			if ( pConVar )
			{
				const char *pszDef = pConVar->GetDefault();
				const char *pszCur = pConVar->GetString();
				if ( Q_strcmp( pszDef, pszCur ) )
				{
					AddTag( p->GetString("tag") );
				}
				else
				{
					RemoveTag( p->GetString("tag") );
				}
			}

			p = p->GetNextKey();
		}

		pKV->deleteThis();
	}

	// Check maxplayers
	int minmaxplayers = 1;
	int maxmaxplayers = ABSOLUTE_PLAYER_LIMIT;
	int defaultmaxplayers = 1;
	serverGameClients->GetPlayerLimits( minmaxplayers, maxmaxplayers, defaultmaxplayers );
	int nMaxReportedClients = GetMaxClients() - GetNumProxies();
	if ( sv_visiblemaxplayers.GetInt() > 0 && sv_visiblemaxplayers.GetInt() < nMaxReportedClients )
	{
		nMaxReportedClients = sv_visiblemaxplayers.GetInt();
	}
	if ( nMaxReportedClients > defaultmaxplayers )
	{
		AddTag( "increased_maxplayers" );
	}
	else
	{
		RemoveTag( "increased_maxplayers" );
	}

#if defined( REPLAY_ENABLED )
	ConVarRef replay_enable( "replay_enable", true );
	if ( replay_enable.IsValid() && replay_enable.GetBool() )
	{
		AddTag( "replays" );
	}
	else
	{
		RemoveTag( "replays" );
	}
#endif

	bRecalculatingTags = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseServer::AddTag( const char *pszTag )
{
	CUtlVector<char*> TagList;
	V_SplitString( sv_tags.GetString(), ",", TagList );
	for ( int i = 0; i < TagList.Count(); i++ )
	{
		// Already in the tag list?
		if ( !Q_stricmp(TagList[i],pszTag) )
			return;
	}
	TagList.PurgeAndDeleteElements();

	// Append it
	char tmptags[MAX_TAG_STRING_LENGTH];
	tmptags[0] = '\0';
	Q_strncpy( tmptags, pszTag, MAX_TAG_STRING_LENGTH );
	Q_strncat( tmptags, ",", MAX_TAG_STRING_LENGTH );
	Q_strncat( tmptags, sv_tags.GetString(), MAX_TAG_STRING_LENGTH );
	sv_tags.SetValue( tmptags );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseServer::RemoveTag( const char *pszTag )
{
	const char *pszTags = sv_tags.GetString();
	if ( !pszTags || !pszTags[0] )
		return;

	char tmptags[MAX_TAG_STRING_LENGTH];
	tmptags[0] = '\0';

	CUtlVector<char*> TagList;
	bool bFoundIt = false;
	V_SplitString( sv_tags.GetString(), ",", TagList );
	for ( int i = 0; i < TagList.Count(); i++ )
	{
		// Keep any tags other than the specified one
		if ( Q_stricmp(TagList[i],pszTag) )
		{
			Q_strncat( tmptags, TagList[i], MAX_TAG_STRING_LENGTH );
			Q_strncat( tmptags, ",", MAX_TAG_STRING_LENGTH );
		}
		else
		{
			bFoundIt = true;
		}
	}
	TagList.PurgeAndDeleteElements();

	// Didn't find it in our list?
	if ( !bFoundIt )
		return;

	sv_tags.SetValue( tmptags );
}

//-----------------------------------------------------------------------------
// Purpose: Server-only override (ignores sv_pausable).  Can be on a timer.
//-----------------------------------------------------------------------------
void CBaseServer::SetPausedForced( bool bPaused, float flDuration /*= -1.f*/ )
{
	if ( !IsActive() )
		return;

	m_State = ( bPaused ) ? ss_paused : ss_active;
	m_flPausedTimeEnd = ( bPaused && flDuration > 0.f ) ? Sys_FloatTime() + flDuration : -1.f;

	SVC_SetPauseTimed setpause( bPaused, m_flPausedTimeEnd );
	BroadcastMessage( setpause );
}
