//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:  baseclientstate.cpp: implementation of the CBaseClientState class.
//
//===============================================================================

#include "client_pch.h"
#include "baseclientstate.h"
#include "vstdlib/random.h"
#include <inetchannel.h>
#include <netmessages.h>
#include <proto_oob.h>
#include <ctype.h>
#include "cl_main.h"
#include "net.h"
#include "dt_recv_eng.h"
#include "ents_shared.h"
#include "net_synctags.h"
#include "filesystem_engine.h"
#include "host_cmd.h"
#include "GameEventManager.h"
#include "sv_rcon.h"
#include "cl_rcon.h"
#ifndef SWDS
#include "vgui_baseui_interface.h"
#include "cl_pluginhelpers.h"
#include "vgui_askconnectpanel.h"
#endif
#include "sv_steamauth.h"
#include "tier0/icommandline.h"
#include "tier0/vcrmode.h"
#include "snd_audio_source.h"
#include "cl_steamauth.h"
#include "server.h"
#include "steam/steam_api.h"
#include "matchmaking.h"
#include "sv_plugin.h"
#include "sys_dll.h"
#include "host.h"
#if defined( REPLAY_ENABLED )
#include "replay_internal.h"
#include "replayserver.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef ENABLE_RPT
void CL_NotifyRPTOfDisconnect( );
#endif // ENABLE_RPT

#if !defined( NO_STEAM ) 
void UpdateNameFromSteamID( IConVar *pConVar, CSteamID *pSteamID )
{
#ifndef SWDS
	if ( !pConVar || !pSteamID || !Steam3Client().SteamFriends() )
		return;

	const char *pszName = Steam3Client().SteamFriends()->GetFriendPersonaName( *pSteamID );
	pConVar->SetValue( pszName );
#endif // SWDS
}

void SetNameToSteamIDName( IConVar *pConVar )
{
#ifndef SWDS
	if ( Steam3Client().SteamUtils() && Steam3Client().SteamFriends() && Steam3Client().SteamUser() )
	{
		CSteamID steamID = Steam3Client().SteamUser()->GetSteamID();
		UpdateNameFromSteamID( pConVar, &steamID );
	}
#endif // SWDS
}
#endif // NO_STEAM


void CL_NameCvarChanged( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	ConVarRef var( pConVar );

	static bool bPreventRent = false;
	if ( !bPreventRent )
	{
		bPreventRent = true;
#if !defined( NO_STEAM )
		SetNameToSteamIDName( pConVar );
#endif
		// Remove any evil characters (ex: zero width no-break space)
		char pchName[MAX_PLAYER_NAME_LENGTH];
		V_strcpy_safe( pchName, var.GetString() );
		Q_RemoveAllEvilCharacters( pchName );
		pConVar->SetValue( pchName );

		// store off the last known name, that isn't default, in the registry
		// this is a transition step so it can be used to display in friends
		if ( 0 != Q_stricmp( var.GetString(), var.GetDefault()  ) 
			&& 0 != Q_stricmp( var.GetString(), "player" ) )
		{
			Sys_SetRegKeyValue( "Software\\Valve\\Steam", "LastGameNameUsed", var.GetString() );
		}

		bPreventRent = false;
	}
}




#ifndef SWDS
void askconnect_accept_f()
{
	char szHostName[256];
	if ( IsAskConnectPanelActive( szHostName, sizeof( szHostName ) ) )
	{
		char szCommand[512];
		V_snprintf( szCommand, sizeof( szCommand ), "connect %s redirect", szHostName );
		Cbuf_AddText( szCommand );
		HideAskConnectPanel();
	}
}
ConCommand askconnect_accept( "askconnect_accept", askconnect_accept_f, "Accept a redirect request by the server.", FCVAR_DONTRECORD );
#endif

#ifndef SWDS
extern IVEngineClient *engineClient;
// ---------------------------------------------------------------------------------------- //
static void SendClanTag( const char *pTag )
{
	KeyValues *kv = new KeyValues( "ClanTagChanged" );
	kv->SetString( "tag", pTag );
	engineClient->ServerCmdKeyValues( kv );
}
#endif

// ---------------------------------------------------------------------------------------- //
void CL_ClanIdChanged( IConVar *pConVar, const char *pOldString, float flOldValue )
{
#ifndef SWDS
	// Get the clan ID we're trying to select
	ConVarRef var( pConVar );
	uint32 newId = var.GetInt();
	if ( newId == 0 )
	{
		// Default value, equates to no tag
		SendClanTag( "" );
		return;
	}

#if !defined( NO_STEAM )
	// Make sure this player is actually part of the desired clan
	ISteamFriends *pFriends = Steam3Client().SteamFriends();
	if ( pFriends )
	{
		int iGroupCount = pFriends->GetClanCount();
		for ( int k = 0; k < iGroupCount; ++ k )
		{
			CSteamID clanID = pFriends->GetClanByIndex( k );
			if ( clanID.GetAccountID() == newId )
			{
				// valid clan, accept the change
				CSteamID clanIDNew( newId, Steam3Client().SteamUtils()->GetConnectedUniverse(), k_EAccountTypeClan );
				SendClanTag( pFriends->GetClanTag( clanIDNew ) );
				return;
			}
		}
	}
#endif // NO_STEAM

	// Couldn't validate the ID, so clear to the default (no tag)
	var.SetValue( 0 );
#endif // !SWDS
}

ConVar	cl_resend	( "cl_resend","6", FCVAR_NONE, "Delay in seconds before the client will resend the 'connect' attempt", true, CL_MIN_RESEND_TIME, true, CL_MAX_RESEND_TIME );
ConVar	cl_name		( "name","unnamed", FCVAR_ARCHIVE | FCVAR_USERINFO | FCVAR_PRINTABLEONLY | FCVAR_SERVER_CAN_EXECUTE, "Current user name", CL_NameCvarChanged );
ConVar	password	( "password", "", FCVAR_ARCHIVE | FCVAR_SERVER_CANNOT_QUERY | FCVAR_DONTRECORD, "Current server access password" );
ConVar  cl_interpolate( "cl_interpolate", "1.0", FCVAR_USERINFO | FCVAR_DEVELOPMENTONLY | FCVAR_NOT_CONNECTED, "Interpolate entities on the client." );
ConVar  cl_clanid( "cl_clanid", "0", FCVAR_ARCHIVE | FCVAR_USERINFO | FCVAR_HIDDEN, "Current clan ID for name decoration", CL_ClanIdChanged );
ConVar  cl_show_connectionless_packet_warnings( "cl_show_connectionless_packet_warnings", "0", FCVAR_NONE, "Show console messages about ignored connectionless packets on the client." );

// ---------------------------------------------------------------------------------------- //
// C_ServerClassInfo implementation.
// ---------------------------------------------------------------------------------------- //

C_ServerClassInfo::C_ServerClassInfo()
{
	m_ClassName = NULL;
	m_DatatableName = NULL;
	m_InstanceBaselineIndex = INVALID_STRING_INDEX;
}

C_ServerClassInfo::~C_ServerClassInfo()
{
	delete [] m_ClassName;
	delete [] m_DatatableName;
}

// Returns false if you should stop reading entities.
inline static bool CL_DetermineUpdateType( CEntityReadInfo &u )
{
	if ( !u.m_bIsEntity || ( u.m_nNewEntity > u.m_nOldEntity ) )
	{
		// If we're at the last entity, preserve whatever entities followed it in the old packet.
		// If newnum > oldnum, then the server skipped sending entities that it wants to leave the state alone for.
		if ( !u.m_pFrom	 || ( u.m_nOldEntity > u.m_pFrom->last_entity ) )
		{
			Assert( !u.m_bIsEntity );
			u.m_UpdateType = Finished;
			return false;
		}

		// Preserve entities until we reach newnum (ie: the server didn't send certain entities because
		// they haven't changed).
		u.m_UpdateType = PreserveEnt;
	}
	else
	{
		if( u.m_UpdateFlags & FHDR_ENTERPVS )
		{
			u.m_UpdateType = EnterPVS;
		}
		else if( u.m_UpdateFlags & FHDR_LEAVEPVS )
		{
			u.m_UpdateType = LeavePVS;
		}
		else
		{
			u.m_UpdateType = DeltaEnt;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: When a delta command is received from the server
//  We need to grab the entity # out of it any the bit settings, too.
//  Returns -1 if there are no more entities.
// Input  : &bRemove - 
//			&bIsNew - 
// Output : int
//-----------------------------------------------------------------------------
static inline void CL_ParseDeltaHeader( CEntityReadInfo &u )
{
	u.m_UpdateFlags = FHDR_ZERO;

#ifdef DEBUG_NETWORKING
	int startbit = u.m_pBuf->GetNumBitsRead();
#endif
	SyncTag_Read( u.m_pBuf, "Hdr" );	

	u.m_nNewEntity = u.m_nHeaderBase + 1 + u.m_pBuf->ReadUBitVar();


	u.m_nHeaderBase = u.m_nNewEntity;

	// leave pvs flag
	if ( u.m_pBuf->ReadOneBit() == 0 )
	{
		// enter pvs flag
		if ( u.m_pBuf->ReadOneBit() != 0 )
		{
			u.m_UpdateFlags |= FHDR_ENTERPVS;
		}
	}
	else
	{
		u.m_UpdateFlags |= FHDR_LEAVEPVS;

		// Force delete flag
		if ( u.m_pBuf->ReadOneBit() != 0 )
		{
			u.m_UpdateFlags |= FHDR_DELETE;
		}
	}
	// Output the bitstream...
#ifdef DEBUG_NETWORKING
	int lastbit = u.m_pBuf->GetNumBitsRead();
	{
		void	SpewBitStream( unsigned char* pMem, int bit, int lastbit );
		SpewBitStream( (byte *)u.m_pBuf->m_pData, startbit, lastbit );
	}
#endif
}

CBaseClientState::CBaseClientState()
{
	m_Socket = NS_CLIENT;
	m_pServerClasses = NULL;
	m_StringTableContainer = NULL;
	m_NetChannel = NULL;
	m_nSignonState = SIGNONSTATE_NONE;
	m_nChallengeNr = 0;
	m_flConnectTime = 0;
	m_nRetryNumber = 0;
	m_szRetryAddress[0] = 0;
	m_ulGameServerSteamID = 0;
	m_retryChallenge = 0;
	m_bRestrictServerCommands = true;
	m_bRestrictClientCommands = true;
	m_nServerCount = 0;
	m_nCurrentSequence = 0;
	m_nDeltaTick = 0;
	m_bPaused = 0;
	m_flPausedExpireTime = -1.f;
	m_nViewEntity = 0;
	m_nPlayerSlot = 0;
	m_szLevelFileName[0] = 0;
	m_szLevelBaseName[0] = 0;
	m_nMaxClients = 0;
	Q_memset( m_pEntityBaselines, 0, sizeof( m_pEntityBaselines ) );
	m_nServerClasses = 0;
	m_nServerClassBits = 0;
	m_szEncrytionKey[0] = 0;
}

CBaseClientState::~CBaseClientState()
{

}

void CBaseClientState::Clear( void )
{
	m_nServerCount = -1;
	m_nDeltaTick = -1;
	
	m_ClockDriftMgr.Clear();

	m_nCurrentSequence = 0;
	m_nServerClasses = 0;
	m_nServerClassBits = 0;
	m_nPlayerSlot = 0;
	m_szLevelFileName[0] = 0;
	m_szLevelBaseName[ 0 ] = 0;
	m_nMaxClients = 0;

	if ( m_pServerClasses )
	{
		delete[] m_pServerClasses;
		m_pServerClasses = NULL;
	}

	if ( m_StringTableContainer  )
	{
#ifndef SHARED_NET_STRING_TABLES
		m_StringTableContainer->RemoveAllTables();
#endif
	
		m_StringTableContainer = NULL;
	}

	FreeEntityBaselines();

	RecvTable_Term( false );

	if ( m_NetChannel ) 
		m_NetChannel->Reset();
	
	m_bPaused = 0;
	m_flPausedExpireTime = -1.f;
	m_nViewEntity = 0;
	m_nChallengeNr = 0;
	m_flConnectTime = 0.0f;
}

void CBaseClientState::FileReceived( const char * fileName, unsigned int transferID )
{
	ConMsg( "CBaseClientState::FileReceived: %s.\n", fileName );
}

void CBaseClientState::FileDenied(const char *fileName, unsigned int transferID )
{
	ConMsg( "CBaseClientState::FileDenied: %s.\n", fileName );
}

void CBaseClientState::FileRequested(const char *fileName, unsigned int transferID )
{
	ConMsg( "File '%s' requested from %s.\n", fileName, m_NetChannel->GetAddress() );

	m_NetChannel->SendFile( fileName, transferID ); // CBaseCLisntState always sends file
}

void CBaseClientState::FileSent(const char *fileName, unsigned int transferID )
{
	ConMsg( "File '%s' sent.\n", fileName );
}

#define REGISTER_NET_MSG( name )				\
	NET_##name * p##name = new NET_##name();	\
	p##name->m_pMessageHandler = this;			\
	chan->RegisterMessage( p##name );			\

#define REGISTER_SVC_MSG( name )				\
	SVC_##name * p##name = new SVC_##name();	\
	p##name->m_pMessageHandler = this;			\
	chan->RegisterMessage( p##name );			\

void CBaseClientState::ConnectionStart(INetChannel *chan)
{
	REGISTER_NET_MSG( Tick );
	REGISTER_NET_MSG( StringCmd );
	REGISTER_NET_MSG( SetConVar );
	REGISTER_NET_MSG( SignonState );

	REGISTER_SVC_MSG( Print );
	REGISTER_SVC_MSG( ServerInfo );
	REGISTER_SVC_MSG( SendTable );
	REGISTER_SVC_MSG( ClassInfo );
	REGISTER_SVC_MSG( SetPause );
	REGISTER_SVC_MSG( CreateStringTable );
	REGISTER_SVC_MSG( UpdateStringTable );
	REGISTER_SVC_MSG( VoiceInit );
	REGISTER_SVC_MSG( VoiceData );
	REGISTER_SVC_MSG( Sounds );
	REGISTER_SVC_MSG( SetView );
	REGISTER_SVC_MSG( FixAngle );
	REGISTER_SVC_MSG( CrosshairAngle );
	REGISTER_SVC_MSG( BSPDecal );
	REGISTER_SVC_MSG( GameEvent );
	REGISTER_SVC_MSG( UserMessage );
	REGISTER_SVC_MSG( EntityMessage );
	REGISTER_SVC_MSG( PacketEntities );
	REGISTER_SVC_MSG( TempEntities );
	REGISTER_SVC_MSG( Prefetch );
	REGISTER_SVC_MSG( Menu );
	REGISTER_SVC_MSG( GameEventList );
	REGISTER_SVC_MSG( GetCvarValue );
	REGISTER_SVC_MSG( CmdKeyValues );
	REGISTER_SVC_MSG( SetPauseTimed );
}

void CBaseClientState::ConnectionClosing( const char *reason )
{
	ConMsg( "Disconnect: %s.\n", reason?reason:"unknown reason" );
	Disconnect( reason ? reason : "Connection closing", true );
}

//-----------------------------------------------------------------------------
// Purpose: A svc_signonnum has been received, perform a client side setup
// Output : void CL_SignonReply
//-----------------------------------------------------------------------------
bool CBaseClientState::SetSignonState ( int state, int count )
{
	//	ConDMsg ("CL_SignonReply: %i\n", cl.signon);

	if ( state < SIGNONSTATE_NONE || state > SIGNONSTATE_CHANGELEVEL )
	{
		ConMsg ("Received signon %i when at %i\n", state, m_nSignonState );
		Assert( 0 );
		return false;
	}

	if ( (state > SIGNONSTATE_CONNECTED) &&	(state <= m_nSignonState) && !m_NetChannel->IsPlayback() )
	{
		ConMsg ("Received signon %i when at %i\n", state, m_nSignonState);
		Assert( 0 );
		return false;
	}

	if ( (count != m_nServerCount) && (count != -1) && (m_nServerCount != -1) && !m_NetChannel->IsPlayback() )
	{
		ConMsg ("Received wrong spawn count %i when at %i\n", count, m_nServerCount );
		Assert( 0 );
		return false;
	}

	if ( state == SIGNONSTATE_FULL )
	{
#if defined( REPLAY_ENABLED ) && !defined( DEDICATED )
		if ( g_pClientReplayContext )
		{
			g_pClientReplayContext->OnSignonStateFull();
		}
#endif

		if ( IsX360() && 
			g_pMatchmaking->PreventFullServerStartup() )
		{
			return true;
		}
	}

	m_nSignonState = state;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: called by CL_Connect and CL_CheckResend
// If we are in ca_connecting state and we have gotten a challenge
//   response before the timeout, send another "connect" request.
// Output : void CL_SendConnectPacket
//-----------------------------------------------------------------------------
void CBaseClientState::SendConnectPacket (int challengeNr, int authProtocol, uint64 unGSSteamID, bool bGSSecure )
{
	COM_TimestampedLog( "SendConnectPacket" );

	netadr_t adr;
	char szServerName[MAX_OSPATH];
	const char *CDKey = "NOCDKEY";

	Q_strncpy(szServerName, m_szRetryAddress, MAX_OSPATH);

	if ( !NET_StringToAdr (szServerName, &adr) )
	{
		ConMsg ("Bad server address (%s)\n", szServerName );
		Disconnect( "Bad server address", true );
		// Host_Disconnect(); MOTODO
		return;
	}

	if ( adr.GetPort() == (unsigned short)0 )
	{
		adr.SetPort( PORT_SERVER );
	}

	ALIGN4 char		msg_buffer[MAX_ROUTABLE_PAYLOAD] ALIGN4_POST;
	bf_write	msg( msg_buffer, sizeof(msg_buffer) );

	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteByte( C2S_CONNECT );
	msg.WriteLong( PROTOCOL_VERSION );
	msg.WriteLong( authProtocol );
	msg.WriteLong( challengeNr );
	msg.WriteLong( m_retryChallenge );
	msg.WriteString( GetClientName() );	// Name
	msg.WriteString( password.GetString() );		// password
	msg.WriteString( GetSteamInfIDVersionInfo().szVersionString );	// product version
//	msg.WriteByte( ( g_pServerPluginHandler->GetNumLoadedPlugins() > 0 ) ? 1 : 0 ); // have any client-side server plug-ins been loaded?

	switch ( authProtocol )
	{
		// Fall through, bogus protocol type, use CD key hash.
		case PROTOCOL_HASHEDCDKEY:	CDKey = GetCDKeyHash();
									msg.WriteString( CDKey );		// cdkey
									break;

		case PROTOCOL_STEAM:		if ( !PrepareSteamConnectResponse( unGSSteamID, bGSSecure, adr, msg ) )
									{
										return;
									}
									break;

		default: 					Host_Error( "Unexepected authentication protocol %i!\n", authProtocol );
									return;
	}

	// Mark time of this attempt for retransmit requests
	m_flConnectTime = net_time;

	// remember challengenr for TCP connection
	m_nChallengeNr = challengeNr;

	// Remember Steam ID, if any
	m_ulGameServerSteamID = unGSSteamID;

	// Send protocol and challenge value
	NET_SendPacket( NULL, m_Socket, adr, msg.GetData(), msg.GetNumBytesWritten() );
}


//-----------------------------------------------------------------------------
// Purpose: append steam specific data to a connection response
//-----------------------------------------------------------------------------
bool CBaseClientState::PrepareSteamConnectResponse( uint64 unGSSteamID, bool bGSSecure, const netadr_t &adr, bf_write &msg )
{
	// X360TBD: Network - Steam Dedicated Server hack
	if ( IsX360() )
	{
		return true;
	}

#if !defined( NO_STEAM ) && !defined( SWDS  ) 
	if ( !Steam3Client().SteamUser() )
	{
		COM_ExplainDisconnection( true, "#GameUI_ServerRequireSteam" );
		Disconnect( "#GameUI_ServerRequireSteam", true );
		return false;
	}
#endif
	
	netadr_t checkAdr = adr;
	if ( adr.GetType() == NA_LOOPBACK || adr.IsLocalhost() )
	{
		checkAdr.SetIP( net_local_adr.GetIPHostByteOrder() );
	}

#ifndef SWDS
	// now append the steam3 cookie
	char steam3Cookie[ STEAM_KEYSIZE ];
	uint32 steam3CookieLen = 0;

	Steam3Client().GetAuthSessionTicket( steam3Cookie, sizeof(steam3Cookie), &steam3CookieLen, checkAdr.GetIPHostByteOrder(), checkAdr.GetPort(), unGSSteamID, bGSSecure );

	if ( steam3CookieLen == 0 )
	{
		COM_ExplainDisconnection( true, "#GameUI_ServerRequireSteam" );
		Disconnect( "#GameUI_ServerRequireSteam", true );
		return false;
	}

	msg.WriteShort( steam3CookieLen );
	if ( steam3CookieLen > 0 )
		msg.WriteBytes( steam3Cookie, steam3CookieLen );
#endif

	return true;
}

// Tracks how we connected to the current server.
static ConVar cl_connectmethod( "cl_connectmethod", "", FCVAR_USERINFO | FCVAR_HIDDEN, "Method by which we connected to the current server." );

/* static */ bool CBaseClientState::ConnectMethodAllowsRedirects()
{
	// Only HLTV should be allowed to redirect clients, but malicious servers can answer a connect
	// attempt as HLTV and then redirect elsewhere. A somewhat-more complete fix for this would
	// involve tracking our redirected status and refusing to interact with non-HLTV servers. For
	// now, however, we just blacklist server browser / matchmaking connect methods from allowing
	// redirects and allow it for other types.
	const char *pConnectMethod = cl_connectmethod.GetString();
	if ( V_strcmp( pConnectMethod, "serverbrowser_internet" ) == 0 ||
		 V_strncmp( pConnectMethod, "quickpick", 9 ) == 0 ||
		 V_strncmp( pConnectMethod, "quickplay", 9 ) == 0 ||
		 V_strcmp( pConnectMethod, "matchmaking" ) == 0 ||
		 V_strcmp( pConnectMethod, "coaching" ) == 0 )
	 {
		 return false;
	 }
	return true;
}

void CBaseClientState::Connect(const char* adr, const char *pszSourceTag)
{
#if !defined( NO_STEAM )
	// Get our name from steam. Needs to be done before connecting
	// because we won't have triggered a check by changing our name.
	IConVar *pVar = g_pCVar->FindVar( "name" );
	if ( pVar )
	{
		SetNameToSteamIDName( pVar );
	}
#endif


	Q_strncpy( m_szRetryAddress, adr, sizeof(m_szRetryAddress) );
	m_retryChallenge = (RandomInt(0,0x0FFF) << 16) | RandomInt(0,0xFFFF);
	m_ulGameServerSteamID = 0;
	m_sRetrySourceTag = pszSourceTag;
	cl_connectmethod.SetValue( m_sRetrySourceTag.String() );

	// For the check for resend timer to fire a connection / getchallenge request.
	SetSignonState( SIGNONSTATE_CHALLENGE, -1 );
	
	// Force connection request to fire.
	m_flConnectTime = -FLT_MAX;  

	m_nRetryNumber = 0;
}

INetworkStringTable *CBaseClientState::GetStringTable( const char * name ) const
{
	if ( !m_StringTableContainer )
	{
		Assert( m_StringTableContainer );
		return NULL;
	}

	return m_StringTableContainer->FindTable( name );
}

void CBaseClientState::ForceFullUpdate( void )
{
	if ( m_nDeltaTick == -1 )
		return;

	FreeEntityBaselines();
	m_nDeltaTick = -1;
	DevMsg( "Requesting full game update...\n");
}

void CBaseClientState::FullConnect( netadr_t &adr )
{
	// Initiate the network channel
	
	COM_TimestampedLog( "CBaseClientState::FullConnect" );

	m_NetChannel = NET_CreateNetChannel( m_Socket, &adr, "CLIENT", this );

	Assert( m_NetChannel );
	
	m_NetChannel->StartStreaming( m_nChallengeNr );	// open TCP stream

	// Bump connection time to now so we don't resend a connection
	// Request	
	m_flConnectTime = net_time; 

	// We'll request a full delta from the baseline
	m_nDeltaTick = -1;

	// We can send a cmd right away
	m_flNextCmdTime = net_time;

	// Mark client as connected
	SetSignonState( SIGNONSTATE_CONNECTED, -1 );
#if !defined(SWDS)
	RCONClient().SetAddress( m_NetChannel->GetRemoteAddress() );
#endif

	// Fire an event when we get our connection
	IGameEvent *event = g_GameEventManager.CreateEvent( "client_connected" );
	if ( event )
	{
		event->SetString( "address", m_NetChannel->GetRemoteAddress().ToString( true )	);
		event->SetInt(    "ip", m_NetChannel->GetRemoteAddress().GetIPNetworkByteOrder() ); // <<< Network byte order?
		event->SetInt(    "port", m_NetChannel->GetRemoteAddress().GetPort() );
		g_GameEventManager.FireEventClientSide( event );
	}
	
}

void CBaseClientState::ConnectionCrashed(const char *reason)
{
	DebuggerBreakIfDebugging_StagingOnly();
	ConMsg( "Connection lost: %s.\n", reason?reason:"unknown reason" );
	Disconnect( reason ? reason : "Connection crashed", true );
}

void CBaseClientState::Disconnect( const char *pszReason, bool bShowMainMenu )
{
	m_flConnectTime = -FLT_MAX;
	m_nRetryNumber = 0;
	m_ulGameServerSteamID = 0;

	if ( m_nSignonState == SIGNONSTATE_NONE )
		return;

#if !defined( SWDS ) && defined( ENABLE_RPT )
	CL_NotifyRPTOfDisconnect( );
#endif

	m_nSignonState = SIGNONSTATE_NONE;

	netadr_t adr;
	if ( m_NetChannel )
	{
		adr = m_NetChannel->GetRemoteAddress();
	}
	else 
	{
		NET_StringToAdr (m_szRetryAddress, &adr);
	}

#ifndef SWDS
	netadr_t checkAdr = adr;
	if ( adr.GetType() == NA_LOOPBACK || adr.IsLocalhost() )
	{
		checkAdr.SetIP( net_local_adr.GetIPHostByteOrder() );
	}

	Steam3Client().CancelAuthTicket();
#endif

	if ( m_NetChannel )
	{
		m_NetChannel->Shutdown( ( pszReason && *pszReason ) ? pszReason : "Disconnect by user." );
		m_NetChannel = NULL;
	}
}

void CBaseClientState::RunFrame (void)
{
	VPROF("CBaseClientState::RunFrame");
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	if ( (m_nSignonState > SIGNONSTATE_NEW) && m_NetChannel && g_GameEventManager.HasClientListenersChanged() )
	{
		// assemble a list of all events we listening to and tell the server
		CLC_ListenEvents msg;
		g_GameEventManager.WriteListenEventList( &msg );
		m_NetChannel->SendNetMsg( msg );
	}

	if ( m_nSignonState == SIGNONSTATE_CHALLENGE )
	{
		CheckForResend();
	}
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CBaseClientState::CheckForResend (void)
{
	// resend if we haven't gotten a reply yet
	// We only resend during the connection process.
	if ( m_nSignonState != SIGNONSTATE_CHALLENGE )
		return;

	// Wait at least the resend # of seconds.
	if ( ( net_time - m_flConnectTime ) < cl_resend.GetFloat())
		return;

	netadr_t	adr;

	if (!NET_StringToAdr (m_szRetryAddress, &adr))
	{
		ConMsg ("Bad server address (%s)\n", m_szRetryAddress);
		//Host_Disconnect();
		Disconnect( "Bad server address", true );
		return;
	}
	if (adr.GetPort() == 0)
	{
		adr.SetPort( PORT_SERVER );
	}

	// Only retry so many times before failure.
	if ( m_nRetryNumber >= GetConnectionRetryNumber() )
	{
		COM_ExplainDisconnection( true, "Connection failed after %i retries.\n", CL_CONNECTION_RETRIES );
		// Host_Disconnect();
		Disconnect( "Connection failed", true );
		return;
	}
	
	// Mark time of this attempt.
	m_flConnectTime = net_time;	// for retransmit requests

	// Display appropriate message
	if ( Q_strncmp(m_szRetryAddress, "localhost", 9) )
	{
		if ( m_nRetryNumber == 0 )
			ConMsg ("Connecting to %s...\n", m_szRetryAddress);
		else
			ConMsg ("Retrying %s...\n", m_szRetryAddress);
	}

	// Fire an event when we attempt connection
	if ( m_nRetryNumber == 0 )
	{
		IGameEvent *event = g_GameEventManager.CreateEvent( "client_beginconnect" );
		if ( event )
		{
			event->SetString( "address", m_szRetryAddress);
			event->SetInt(    "ip", adr.GetIPNetworkByteOrder() ); // <<< Network byte order?
			event->SetInt(    "port", adr.GetPort() );
			//event->SetInt(    "retry_number", m_nRetryNumber );
			event->SetString( "source", m_sRetrySourceTag );
			g_GameEventManager.FireEventClientSide( event );
		}
	}

	m_nRetryNumber++;

	// Request another challenge value.
	{
		ALIGN4 char		msg_buffer[MAX_ROUTABLE_PAYLOAD] ALIGN4_POST;
		bf_write	msg( msg_buffer, sizeof(msg_buffer) );

		msg.WriteLong( CONNECTIONLESS_HEADER );
		msg.WriteByte( A2S_GETCHALLENGE );
		msg.WriteLong( m_retryChallenge );
		msg.WriteString( "0000000000" ); // pad out
		NET_SendPacket( NULL, m_Socket, adr, msg.GetData(), msg.GetNumBytesWritten() );
	}
}

bool CBaseClientState::ProcessConnectionlessPacket( netpacket_t *packet )
{
	VPROF( "ProcessConnectionlessPacket" );

	Assert( packet );

	bf_read &msg = packet->message;	// handy shortcut 

	int c = msg.ReadByte();

	// ignoring a specific packet that DOTA2 broadcasts
	if ( c == C2C_MOD )
		return false;

	char string[MAX_ROUTABLE_PAYLOAD];

	netadr_t adrServerConnectingTo;
	NET_StringToAdr ( m_szRetryAddress, &adrServerConnectingTo );
	if ( ( packet->from.GetType() != NA_LOOPBACK ) && ( packet->from.GetIPNetworkByteOrder() != adrServerConnectingTo.GetIPNetworkByteOrder() ) )
	{
		if ( cl_show_connectionless_packet_warnings.GetBool() )
		{
			ConDMsg ( "Discarding connectionless packet ( CL '%c' ) from %s.\n", c, packet->from.ToString() );
		}
		return false;
	}

	switch ( c )
	{
		
	case S2C_CONNECTION:	if ( m_nSignonState == SIGNONSTATE_CHALLENGE )
							{
								int myChallenge = msg.ReadLong();
								if ( myChallenge != m_retryChallenge )
								{
									Msg( "Server connection did not have the correct challenge, ignoring.\n" );
									return false;
								}

								// server accepted our connection request
								FullConnect( packet->from );
							}
							break;
		
	case S2C_CHALLENGE:		// Response from getchallenge we sent to the server we are connecting to
							// Blow it off if we are not connected.
							if ( m_nSignonState == SIGNONSTATE_CHALLENGE )
							{
								int magicVersion = msg.ReadLong();
								if ( magicVersion != S2C_MAGICVERSION )
								{
									COM_ExplainDisconnection( true, "#GameUI_ServerConnectOutOfDate"  );
									Disconnect( "#GameUI_ServerConnectOutOfDate", true );
									return false;
								}

								int challenge = msg.ReadLong();
								int myChallenge = msg.ReadLong();
								if ( myChallenge != m_retryChallenge )
								{
									Msg( "Server challenge did not have the correct challenge, ignoring.\n" );
									return false;
								}

								int authprotocol = msg.ReadLong();
								uint64 unGSSteamID = 0;
								bool bGSSecure = false;
								if ( authprotocol == PROTOCOL_STEAM )
								{
									if ( msg.ReadShort() != 0 )
									{
										Msg( "Invalid Steam key size.\n" );
										Disconnect( "Invalid Steam key size", true );
										return false;
									}
									if ( msg.GetNumBytesLeft() > sizeof(unGSSteamID) ) 
									{
										if ( !msg.ReadBytes( &unGSSteamID, sizeof(unGSSteamID) ) )
										{
											Msg( "Invalid GS Steam ID.\n" );
											Disconnect( "Invalid GS Steam ID", true );
											return false;
										}

										bGSSecure = ( msg.ReadByte() == 1 );
									}
									// The host can disable access to secure servers if you load unsigned code (mods, plugins, hacks)
									if ( bGSSecure && !Host_IsSecureServerAllowed() )
									{
										COM_ExplainDisconnection( true, "#GameUI_ServerInsecure" );
										Disconnect( "#GameUI_ServerInsecure", true );
										return false;
									}
								}
								SendConnectPacket( challenge, authprotocol, unGSSteamID, bGSSecure );
							}
							break;

	case S2C_CONNREJECT:	if ( m_nSignonState == SIGNONSTATE_CHALLENGE )  // Spoofed?
							{
								int myChallenge = msg.ReadLong();
								if ( myChallenge != m_retryChallenge )
								{
									Msg( "Received connection rejection that didn't match my challenge, ignoring.\n" );
									return false;
								}

								msg.ReadString( string, sizeof(string) );
								// Force failure dialog to come up now.
								COM_ExplainDisconnection( true, "%s", string );
								Disconnect( string, true );
								// Host_Disconnect();
							}
							break;

	// Unknown?
	default:
							// Otherwise, don't do anything.
							if ( cl_show_connectionless_packet_warnings.GetBool() )
							{
								ConDMsg ( "Bad connectionless packet ( CL '%c' ) from %s.\n", c, packet->from.ToString() );
							}
							return false;
	}

	return true;
}

bool CBaseClientState::ProcessTick( NET_Tick *msg )
{
	VPROF( "ProcessTick" );

	m_NetChannel->SetRemoteFramerate( msg->m_flHostFrameTime, msg->m_flHostFrameTimeStdDeviation );

	// Note: CClientState separates the client and server clock states and drifts
	// the client's clock to match the server's, but right here, we keep the two clocks in sync.
	SetClientTickCount( msg->m_nTick );
	SetServerTickCount( msg->m_nTick );

	if ( m_StringTableContainer )
	{
		m_StringTableContainer->SetTick( GetServerTickCount() );
	}

	return (GetServerTickCount()>0);
}

void CBaseClientState::SendStringCmd(const char * command)
{
	if ( m_NetChannel) 
	{
		NET_StringCmd stringCmd( command );
		m_NetChannel->SendNetMsg( stringCmd );
	}
}

bool CBaseClientState::ProcessStringCmd( NET_StringCmd *msg )
{
	VPROF( "ProcessStringCmd" );

	// Don't restrict commands from the server in single player or if cl_restrict_stuffed_commands is 0.
	if ( !m_bRestrictServerCommands || sv.IsActive() )
	{
		Cbuf_AddText ( msg->m_szCommand );
		return true;
	}

	// Check that we can add the two execution markers
	if ( !Cbuf_HasRoomForExecutionMarkers( 2 ) )
	{
		AssertMsg( false, "CBaseClientState::ProcessStringCmd called but there is no room for the execution markers. Ignoring command." );
		return true;
	}

	// Run the command, but make sure the command parser knows to only execute commands marked with FCVAR_SERVER_CAN_EXECUTE.
	Cbuf_AddTextWithMarkers( eCmdExecutionMarker_Enable_FCVAR_SERVER_CAN_EXECUTE, msg->m_szCommand, eCmdExecutionMarker_Disable_FCVAR_SERVER_CAN_EXECUTE );

	return true;
}


bool CBaseClientState::ProcessSetConVar( NET_SetConVar *msg )
{
	VPROF( "ProcessSetConVar" );

	// Never process on local client, since the ConVar is directly linked here
	if ( m_NetChannel->IsLoopback() )
		return true;

	for ( int i=0; i<msg->m_ConVars.Count(); i++ )
	{
		const char *name = msg->m_ConVars[i].name;
		const char *value = msg->m_ConVars[i].value;

		// De-constify
		ConVarRef var( name );

		if ( !var.IsValid() )
		{
			ConMsg( "SetConVar: No such cvar ( %s set to %s), skipping\n",
				name, value );
			continue; 
		}

		// Make sure server is only setting replicated game ConVars
		if ( !var.IsFlagSet( FCVAR_REPLICATED ) )
		{
			ConMsg( "SetConVar: Can't set server cvar %s to %s, not marked as FCVAR_REPLICATED on client\n",
				name, value );
			continue;
		}

		// Set value directly ( don't call through cv->DirectSet!!! )
		if ( !sv.IsActive() )
		{
			var.SetValue( value );
			DevMsg( "SetConVar: %s = \"%s\"\n", name, value );
		}
	}

	return true;
}

bool CBaseClientState::ProcessSignonState( NET_SignonState *msg )
{
	VPROF( "ProcessSignonState" );

	return SetSignonState( msg->m_nSignonState, msg->m_nSpawnCount ) ;	
}

bool CBaseClientState::ProcessPrint( SVC_Print *msg )
{
	VPROF( "ProcessPrint" );

	ConMsg( "%s", msg->m_szText );
	return true;
}

bool CBaseClientState::ProcessMenu( SVC_Menu *msg )
{
	VPROF( "ProcessMenu" );

#if !defined(SWDS)
	PluginHelpers_Menu( msg );	
#endif
	return true;
}

bool CBaseClientState::ProcessServerInfo( SVC_ServerInfo *msg )
{
	VPROF( "ProcessServerInfo" );

#ifndef SWDS
	EngineVGui()->UpdateProgressBar(PROGRESS_PROCESSSERVERINFO);
#endif

	COM_TimestampedLog( " CBaseClient::ProcessServerInfo" );
	
	if (  msg->m_nProtocol != PROTOCOL_VERSION 
#if  defined( DEMO_BACKWARDCOMPATABILITY ) && (! defined( SWDS ) )
		&& !( demoplayer->IsPlayingBack() && msg->m_nProtocol >= PROTOCOL_VERSION_12 )
#endif
		)
	{
		ConMsg ( "Server returned version %i, expected %i.\n", msg->m_nProtocol, PROTOCOL_VERSION );
		return false; 
	}

	// Parse servercount (i.e., # of servers spawned since server .exe started)
	// So that we can detect new server startup during download, etc.
	m_nServerCount = msg->m_nServerCount;

	m_nMaxClients		= msg->m_nMaxClients;

	m_nServerClasses	= msg->m_nMaxClasses;
	m_nServerClassBits	= Q_log2( m_nServerClasses ) + 1;
	
	if ( m_nMaxClients < 1 || m_nMaxClients > ABSOLUTE_PLAYER_LIMIT )
	{
		ConMsg ("Bad maxclients (%u) from server.\n", m_nMaxClients);
		return false;
	}

	if ( m_nServerClasses < 1 || m_nServerClasses > MAX_SERVER_CLASSES )
	{
		ConMsg ("Bad maxclasses (%u) from server.\n", m_nServerClasses);
		return false;
	}

#ifndef SWDS
	if ( !sv.IsActive() && 
		!( m_NetChannel->IsLoopback() || m_NetChannel->IsNull() ) )
	{
		// if you are joing a remote server it MUST be multipler and have maxplayer set to more than 1
		// this prevents a spoofed ServerInfo packet from making the client think its in singleplayer
		// and turning off a bunch of security checks
		if ( m_nMaxClients <= 1 )
		{
			ConMsg ("Bad maxclients (%u) from server.\n", m_nMaxClients);
			return false;
		}

		// reset server enforced cvars
		g_pCVar->RevertFlaggedConVars( FCVAR_REPLICATED );	

		// Cheats were disabled; revert all cheat cvars to their default values.
		// This must be done heading into multiplayer games because people can play
		// demos etc and set cheat cvars with sv_cheats 0.
		g_pCVar->RevertFlaggedConVars( FCVAR_CHEAT );

		DevMsg( "FCVAR_CHEAT cvars reverted to defaults.\n" );
	}
#endif

	// clear all baselines still around from last game
	FreeEntityBaselines();

	// force changed flag to being reset
	g_GameEventManager.HasClientListenersChanged( true );
	
	m_nPlayerSlot = msg->m_nPlayerSlot;
	m_nViewEntity = m_nPlayerSlot + 1; 
	
	if ( msg->m_fTickInterval < MINIMUM_TICK_INTERVAL ||
		 msg->m_fTickInterval > MAXIMUM_TICK_INTERVAL )
	{
		ConMsg ("Interval_per_tick %f out of range [%f to %f]\n",
			msg->m_fTickInterval, MINIMUM_TICK_INTERVAL, MAXIMUM_TICK_INTERVAL );
		return false;
	}
	
	if ( !COM_CheckGameDirectory( msg->m_szGameDir ) )
	{
		return false;
	}

	Q_strncpy( m_szLevelBaseName, msg->m_szMapName, sizeof( m_szLevelBaseName ) );

#if !defined(SWDS)
	audiosourcecache->LevelInit( m_szLevelBaseName );
#endif

	ConVarRef skyname( "sv_skyname" );
	if ( skyname.IsValid() )
	{
		skyname.SetValue( msg->m_szSkyName );
	}

	m_nDeltaTick = -1;	// no valid snapshot for this game yet
	
	// fire a client side event about server data

	IGameEvent *event = g_GameEventManager.CreateEvent( "server_spawn" );

	if ( event )
	{
		event->SetString( "hostname", msg->m_szHostName );
		event->SetString( "address", m_NetChannel->GetRemoteAddress().ToString( true )	);
		event->SetInt(    "ip", m_NetChannel->GetRemoteAddress().GetIPNetworkByteOrder() ); // <<< Network byte order?
		event->SetInt(    "port", m_NetChannel->GetRemoteAddress().GetPort() );
		event->SetString( "game", msg->m_szGameDir );
		event->SetString( "mapname", msg->m_szMapName );
		event->SetInt(    "maxplayers", msg->m_nMaxClients );
		event->SetInt(	  "password", 0 );				// TODO
		event->SetString( "os", va("%c", toupper( msg->m_cOS ) ) );
		event->SetInt(    "dedicated", msg->m_bIsDedicated ? 1 : 0 );
		if ( m_ulGameServerSteamID != 0 )
		{
			event->SetString( "steamid", CSteamID(m_ulGameServerSteamID).Render() );
		}

		g_GameEventManager.FireEventClientSide( event );
	}

	// Set default filename, but this is finalized by ClientState later, so it should not be depended on yet. See PrepareLevelResources call
	Host_DefaultMapFileName( msg->m_szMapName, m_szLevelFileName, sizeof( m_szLevelFileName ) );

	COM_TimestampedLog( " CBaseClient::ProcessServerInfo(done)" );

	return true;
}

bool CBaseClientState::ProcessSendTable( SVC_SendTable *msg )
{
	VPROF( "ProcessSendTable" );

	if ( !RecvTable_RecvClassInfos( &msg->m_DataIn, msg->m_bNeedsDecoder ) )
	{
		Host_EndGame(true, "ProcessSendTable: RecvTable_RecvClassInfos failed.\n" );
		return false;
	}

	return true;
}

bool CBaseClientState::ProcessClassInfo( SVC_ClassInfo *msg )
{
	VPROF( "ProcessClassInfo" );

	COM_TimestampedLog( " CBaseClient::ProcessClassInfo" );

	if ( msg->m_bCreateOnClient )
	{
		ConMsg ( "Can't create class tables.\n");
		Assert( 0 );
		return false;
	}

	if( m_pServerClasses )
	{
		delete [] m_pServerClasses;
	}

	m_nServerClasses = msg->m_Classes.Count();
	m_pServerClasses = new C_ServerClassInfo[m_nServerClasses];

	if ( !m_pServerClasses )
	{
		Host_EndGame(true, "ProcessClassInfo: can't allocate %d C_ServerClassInfos.\n", m_nServerClasses);
		return false;
	}

	// copy class names and class IDs from message to CClientState
	for (int i=0; i<m_nServerClasses; i++)
	{
		SVC_ClassInfo::class_t * svclass = &msg->m_Classes[ i ];

		if( svclass->classID >= m_nServerClasses )
		{
			Host_EndGame(true, "ProcessClassInfo: invalid class index (%d).\n", svclass->classID);
			return false;
		}

		C_ServerClassInfo * svclassinfo = &m_pServerClasses[svclass->classID];

		int len = Q_strlen(svclass->classname) + 1;
		svclassinfo->m_ClassName = new char[ len ];
		Q_strncpy( svclassinfo->m_ClassName, svclass->classname, len );
		len = Q_strlen(svclass->datatablename) + 1;
		svclassinfo->m_DatatableName = new char[ len ];
		Q_strncpy( svclassinfo->m_DatatableName,svclass->datatablename, len );
	}

	COM_TimestampedLog( " CBaseClient::ProcessClassInfo(done)" );

	return LinkClasses();	// link server and client classes
}

bool CBaseClientState::ProcessSetPause( SVC_SetPause *msg )
{
	VPROF( "ProcessSetPause" );

	m_bPaused = msg->m_bPaused;
	return true;
}

bool CBaseClientState::ProcessSetPauseTimed( SVC_SetPauseTimed *msg )
{
	VPROF( "ProcessSetPauseTimed" );

	m_bPaused = msg->m_bPaused;
	m_flPausedExpireTime = msg->m_flExpireTime;
	return true;
}


bool CBaseClientState::ProcessCreateStringTable( SVC_CreateStringTable *msg )
{
	VPROF( "ProcessCreateStringTable" );

#ifndef SWDS
	EngineVGui()->UpdateProgressBar(PROGRESS_PROCESSSTRINGTABLE);
#endif

	COM_TimestampedLog( " CBaseClient::ProcessCreateStringTable(%s)", msg->m_szTableName );
	m_StringTableContainer->AllowCreation( true );

    int startbit = msg->m_DataIn.GetNumBitsRead();

#ifndef SHARED_NET_STRING_TABLES

	CNetworkStringTable *table = (CNetworkStringTable*)
		m_StringTableContainer->CreateStringTableEx( msg->m_szTableName, msg->m_nMaxEntries, msg->m_nUserDataSize, msg->m_nUserDataSizeBits, msg->m_bIsFilenames );

	Assert ( table );

	table->SetTick( GetServerTickCount() ); // set creation tick

	HookClientStringTable( msg->m_szTableName );

	if ( msg->m_bDataCompressed )
	{
		unsigned int msgUncompressedSize = msg->m_DataIn.ReadLong();
		unsigned int msgCompressedSize = msg->m_DataIn.ReadLong();
		unsigned int uncompressedSize = msgUncompressedSize;
		/// XXX(JohnS): 11/08/2016 - The PAD_NUMBER() call below was overflowing on UINT32_MAX-3 values.  Enforcing
		//              resource-usage limits at this level is a lost cause without a massive overhaul, but clamp these
		//              to somewhat reasonable ranges to prevent overflows with less-audited code in the engine.
		bool bSuccess = false;
		if ( msg->m_DataIn.TotalBytesAvailable() > 0 &&
		     msgCompressedSize <= (unsigned int)msg->m_DataIn.TotalBytesAvailable() &&
		     msgCompressedSize < UINT_MAX/2 &&
		     msgUncompressedSize < UINT_MAX/2 )
		{
			// allocate buffer for uncompressed data, align to 4 bytes boundary
			char *uncompressedBuffer = new char[PAD_NUMBER( msgUncompressedSize, 4 )];
			char *compressedBuffer = new char[PAD_NUMBER( msgCompressedSize, 4 )];

			msg->m_DataIn.ReadBits( compressedBuffer, msgCompressedSize * 8 );

			// uncompress data
			bSuccess = COM_BufferToBufferDecompress( uncompressedBuffer, &uncompressedSize, compressedBuffer, msgCompressedSize );
			bSuccess &= ( uncompressedSize == msgUncompressedSize );

			if ( bSuccess )
			{
				bf_read data( uncompressedBuffer, uncompressedSize );
				table->ParseUpdate( data, msg->m_nNumEntries );
			}

			delete[] uncompressedBuffer;
			delete[] compressedBuffer;
		}

		if ( !bSuccess )
		{
			Assert( false );
			Warning("Malformed message in CBaseClientState::ProcessCreateStringTable\n");
		}
	}
	else
	{
		table->ParseUpdate( msg->m_DataIn, msg->m_nNumEntries );
	}

#endif

	m_StringTableContainer->AllowCreation( false );

	int endbit = msg->m_DataIn.GetNumBitsRead();

	COM_TimestampedLog( " CBaseClient::ProcessCreateStringTable(%s)-done", msg->m_szTableName );

	return ( endbit - startbit ) == msg->m_nLength;	
}

bool CBaseClientState::ProcessUpdateStringTable( SVC_UpdateStringTable *msg )
{
	VPROF( "ProcessUpdateStringTable" );

	int startbit = msg->m_DataIn.GetNumBitsRead();

#ifndef SHARED_NET_STRING_TABLES

	//m_StringTableContainer is NULL on level transitions, Seems to be caused by a UpdateStringTable packet comming in before the ServerInfo packet
	//  I'm not sure this is safe, but at least we won't crash. The realy odd thing is this can happen on the server as well.//tmauer
	if(m_StringTableContainer != NULL)
	{
		CNetworkStringTable *table = (CNetworkStringTable*)
			m_StringTableContainer->GetTable( msg->m_nTableID );

		table->ParseUpdate( msg->m_DataIn, msg->m_nChangedEntries );
	}
	else
	{
		Warning("m_StringTableContainer is NULL in CBaseClientState::ProcessUpdateStringTable\n");
	}

#endif

	int endbit = msg->m_DataIn.GetNumBitsRead();

	return ( endbit - startbit ) == msg->m_nLength;
}



bool CBaseClientState::ProcessSetView( SVC_SetView *msg )
{
	VPROF( "ProcessSetView" );

	m_nViewEntity = msg->m_nEntityIndex;
	return true;
}

bool CBaseClientState::ProcessPacketEntities( SVC_PacketEntities *msg )
{
	VPROF( "ProcessPacketEntities" );

	// First update is the final signon stage where we actually receive an entity (i.e., the world at least)

	if ( m_nSignonState < SIGNONSTATE_SPAWN )
	{
		ConMsg("Received packet entities while connecting!\n");
		return false;
	}
	
	if ( m_nSignonState == SIGNONSTATE_SPAWN )
	{
		if ( !msg->m_bIsDelta )
		{
			// We are done with signon sequence.
			SetSignonState( SIGNONSTATE_FULL, m_nServerCount );
		}
		else
		{
			ConMsg("Received delta packet entities while spawing!\n");
			return false;
		}
	}

	// overwrite a -1 delta_tick only if packet was uncompressed
	if ( (m_nDeltaTick >= 0) || !msg->m_bIsDelta )
	{
		// we received this snapshot successfully, now this is our delta reference
		m_nDeltaTick = GetServerTickCount();
	}

	return true;
}

void CBaseClientState::ReadPacketEntities( CEntityReadInfo &u )
{
	VPROF( "ReadPacketEntities" );

	// Loop until there are no more entities to read
	
	u.NextOldEntity();

	while ( u.m_UpdateType < Finished )
	{
		u.m_nHeaderCount--;

		u.m_bIsEntity = ( u.m_nHeaderCount >= 0 ) ? true : false;

		if ( u.m_bIsEntity  )
		{
			CL_ParseDeltaHeader( u );
		}

		u.m_UpdateType = PreserveEnt;
		
		while( u.m_UpdateType == PreserveEnt )
		{
			// Figure out what kind of an update this is.
			if( CL_DetermineUpdateType( u ) )
			{
				switch( u.m_UpdateType )
				{
					case EnterPVS:		ReadEnterPVS( u );
										break;

					case LeavePVS:		ReadLeavePVS( u );
										break;

					case DeltaEnt:		ReadDeltaEnt( u );
										break;

					case PreserveEnt:	ReadPreserveEnt( u );
										break;

					default:			DevMsg(1, "ReadPacketEntities: unknown updatetype %i\n", u.m_UpdateType );
										break;
				}
			}
		}
	}

	// Now process explicit deletes 
	if ( u.m_bAsDelta && u.m_UpdateType == Finished )
	{
		ReadDeletions( u );
	}

	// Something didn't parse...
	if ( u.m_pBuf->IsOverflowed() )							
	{	
		Host_Error ( "CL_ParsePacketEntities:  buffer read overflow\n" );
	}

	// If we get an uncompressed packet, then the server is waiting for us to ack the validsequence
	// that we got the uncompressed packet on. So we stop reading packets here and force ourselves to
	// send the clc_move on the next frame.

	if ( !u.m_bAsDelta )
	{
		m_flNextCmdTime = 0.0; // answer ASAP to confirm full update tick
	} 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pHead - 
//			*pClassName - 
// Output : static ClientClass*
//-----------------------------------------------------------------------------
ClientClass* CBaseClientState::FindClientClass(const char *pClassName)
{
	if ( !pClassName )
		return NULL;

	for(ClientClass *pCur=ClientDLL_GetAllClasses(); pCur; pCur=pCur->m_pNext)
	{
		if( Q_stricmp(pCur->m_pNetworkName, pClassName) == 0)
			return pCur;
	}

	return NULL;
}


bool CBaseClientState::LinkClasses()
{
//	// Verify that we have received info about all classes.
//	for ( int i=0; i < m_nServerClasses; i++ )
//	{
//		if ( !m_pServerClasses[i].m_DatatableName )
//		{
//			Host_EndGame(true, "CL_ParseClassInfo_EndClasses: class %d not initialized.\n", i);
//			return false;
//		}
//	}

	// Match the server classes to the client classes.
	for ( int i=0; i < m_nServerClasses; i++ )
	{
		C_ServerClassInfo *pServerClass = &m_pServerClasses[i];
		if ( !pServerClass->m_DatatableName )
			continue;

		// (this can be null in which case we just use default behavior).
		pServerClass->m_pClientClass = FindClientClass(pServerClass->m_ClassName);

		if ( pServerClass->m_pClientClass )
		{
			// If the class names match, then their datatables must match too.
			// It's ok if the client is missing a class that the server has. In that case,
			// if the server actually tries to use it, the client will bomb out.
			const char *pServerName = pServerClass->m_DatatableName;
			const char *pClientName = pServerClass->m_pClientClass->m_pRecvTable->GetName();

			if ( Q_stricmp( pServerName, pClientName ) != 0 )
			{
				Host_EndGame( true, "CL_ParseClassInfo_EndClasses: server and client classes for '%s' use different datatables (server: %s, client: %s)",
					pServerClass->m_ClassName, pServerName, pClientName );
				
				return false;
			}

			// copy class ID
			pServerClass->m_pClientClass->m_ClassID = i;
		}
		else
		{
			Msg( "Client missing DT class %s\n", pServerClass->m_ClassName );
		}
	}

	return true;
}

PackedEntity *CBaseClientState::GetEntityBaseline(int iBaseline, int nEntityIndex)
{
	Assert( (iBaseline == 0) || (iBaseline == 1) );
	return m_pEntityBaselines[iBaseline][nEntityIndex];
}

void CBaseClientState::FreeEntityBaselines()
{
	for ( int i=0; i<2; i++ )
	{
		for ( int j=0; j<MAX_EDICTS; j++ )
		if ( m_pEntityBaselines[i][j] )
		{
			delete m_pEntityBaselines[i][j];
			m_pEntityBaselines[i][j] = NULL;
		}
	}
}

void CBaseClientState::SetEntityBaseline(int iBaseline, ClientClass *pClientClass, int index, char *packedData, int length)
{
	VPROF( "CBaseClientState::SetEntityBaseline" );

	Assert( index >= 0 && index < MAX_EDICTS );
	Assert( pClientClass );
	Assert( (iBaseline == 0) || (iBaseline == 1) );
	
	PackedEntity *entitybl = m_pEntityBaselines[iBaseline][index];

	if ( !entitybl )
	{
		entitybl = m_pEntityBaselines[iBaseline][index] = new PackedEntity();
	}

	entitybl->m_pClientClass = pClientClass;
	entitybl->m_nEntityIndex = index;
	entitybl->m_pServerClass = NULL;

	// Copy out the data we just decoded.
	entitybl->AllocAndCopyPadded( packedData, length );
}

void CBaseClientState::CopyEntityBaseline( int iFrom, int iTo )
{
	Assert ( iFrom != iTo );
	

	for ( int i=0; i<MAX_EDICTS; i++ )
	{
		PackedEntity *blfrom = m_pEntityBaselines[iFrom][i];
		PackedEntity *blto = m_pEntityBaselines[iTo][i];

		if( !blfrom )
		{
			// make sure blto doesn't exists
			if ( blto )
			{
				// ups, we already had this entity but our ack got lost
				// we have to remove it again to stay in sync
				delete m_pEntityBaselines[iTo][i];
				m_pEntityBaselines[iTo][i] = NULL;
			}
			continue;
		}

		if ( !blto )
		{
			// create new to baseline if none existed before
			blto = m_pEntityBaselines[iTo][i] = new PackedEntity();
			blto->m_pClientClass = NULL;
			blto->m_pServerClass = NULL;
			blto->m_ReferenceCount = 0;
		}

		Assert( blfrom->m_nEntityIndex == i );
		Assert( !blfrom->IsCompressed() );

		blto->m_nEntityIndex	= blfrom->m_nEntityIndex; 
		blto->m_pClientClass	= blfrom->m_pClientClass;
		blto->m_pServerClass	= blfrom->m_pServerClass;
		blto->AllocAndCopyPadded( blfrom->GetData(), blfrom->GetNumBytes() );
	}
}

ClientClass *CBaseClientState::GetClientClass( int index )
{
	Assert( index < m_nServerClasses );
	return m_pServerClasses[index].m_pClientClass;
}

bool CBaseClientState::GetClassBaseline( int iClass, void const **pData, int *pDatalen )
{
	ErrorIfNot( 
		iClass >= 0 && iClass < m_nServerClasses, 
		("GetDynamicBaseline: invalid class index '%d'", iClass) );

	// We lazily update these because if you connect to a server that's already got some dynamic baselines,
	// you'll get the baselines BEFORE you get the class descriptions.
	C_ServerClassInfo *pInfo = &m_pServerClasses[iClass];

	INetworkStringTable *pBaselineTable = GetStringTable( INSTANCE_BASELINE_TABLENAME );

	ErrorIfNot( pBaselineTable != NULL,	("GetDynamicBaseline: NULL baseline table" ) );

	if ( pInfo->m_InstanceBaselineIndex == INVALID_STRING_INDEX )
	{
		// The key is the class index string.
		char str[64];
		Q_snprintf( str, sizeof( str ), "%d", iClass );

		pInfo->m_InstanceBaselineIndex = pBaselineTable->FindStringIndex( str );

		if ( pInfo->m_InstanceBaselineIndex == INVALID_STRING_INDEX )
		{
			for (int i = 0; i < pBaselineTable->GetNumStrings(); ++i )
			{
				DevMsg( "%i: %s\n", i, pBaselineTable->GetString( i ) );
			}

			// Gets a callstack, whereas ErrorIfNot(), does not.
			Assert( 0 );
		}
		ErrorIfNot( 
			pInfo->m_InstanceBaselineIndex != INVALID_STRING_INDEX,
			("GetDynamicBaseline: FindStringIndex(%s-%s) failed.", str, pInfo->m_ClassName );
			);
	}
	*pData = pBaselineTable->GetStringUserData( pInfo->m_InstanceBaselineIndex,	pDatalen );

	return *pData != NULL;
}

bool CBaseClientState::ProcessGameEventList( SVC_GameEventList *msg )
{
	VPROF( "ProcessGameEventList" );

	return g_GameEventManager.ParseEventList( msg );
}


bool CBaseClientState::ProcessGetCvarValue( SVC_GetCvarValue *msg )
{
	VPROF( "ProcessGetCvarValue" );

	// Prepare the response.
	CLC_RespondCvarValue returnMsg;
	
	returnMsg.m_iCookie = msg->m_iCookie;
	returnMsg.m_szCvarName = msg->m_szCvarName;
	returnMsg.m_szCvarValue = "";
	returnMsg.m_eStatusCode = eQueryCvarValueStatus_CvarNotFound;

	char tempValue[256];
	
	// Does any ConCommand exist with this name?
	const ConVar *pVar = g_pCVar->FindVar( msg->m_szCvarName );
	if ( pVar )
	{
		if ( pVar->IsFlagSet( FCVAR_SERVER_CANNOT_QUERY ) )
		{
			// The server isn't allowed to query this.
			returnMsg.m_eStatusCode = eQueryCvarValueStatus_CvarProtected;
		}
		else
		{
			returnMsg.m_eStatusCode = eQueryCvarValueStatus_ValueIntact;
			
			if ( pVar->IsFlagSet( FCVAR_NEVER_AS_STRING ) )
			{
				// The cvar won't store a string, so we have to come up with a string for it ourselves.
				if ( fabs( pVar->GetFloat() - pVar->GetInt() ) < 0.001f )
				{
					Q_snprintf( tempValue, sizeof( tempValue ), "%d", pVar->GetInt() );
				}
				else
				{
					Q_snprintf( tempValue, sizeof( tempValue ), "%f", pVar->GetFloat() );
				}
				returnMsg.m_szCvarValue = tempValue;
			}
			else
			{
				// The easy case..
				returnMsg.m_szCvarValue = pVar->GetString();
			}
		}				
	}
	else
	{
		if ( g_pCVar->FindCommand( msg->m_szCvarName ) )
			returnMsg.m_eStatusCode = eQueryCvarValueStatus_NotACvar; // It's a command, not a cvar.
		else
			returnMsg.m_eStatusCode = eQueryCvarValueStatus_CvarNotFound;
	}

	// Send back.
	m_NetChannel->SendNetMsg( returnMsg );
	return true;
}

// Returns dem file protocol version, or, if not playing a demo, just returns PROTOCOL_VERSION
int CBaseClientState::GetDemoProtocolVersion() const
{
#ifndef SWDS												// why is demo play undefined? it shuold be fine on a dedicated server
	if ( demoplayer->IsPlayingBack() )
	{
		return demoplayer->GetProtocolVersion();
	}
#endif
	return PROTOCOL_VERSION;
}

bool CBaseClientState::ProcessCmdKeyValues( SVC_CmdKeyValues *msg )
{
	return true;
}

bool CBaseClientState::IsClientConnectionViaMatchMaking( void )
{
	return ( V_strnistr( cl_connectmethod.GetString(), "quickplay", 9 ) || V_strnistr( cl_connectmethod.GetString(), "matchmaking", 11 ) );
}


