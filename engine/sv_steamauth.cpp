//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: steam state machine that handles authenticating steam users
//
//===========================================================================//

#ifdef _WIN32
#if !defined( _X360 )
#include "winlite.h"
#include <winsock2.h> // INADDR_ANY defn
#endif
#elif POSIX
#include <netinet/in.h>
#endif

#include "sv_steamauth.h"
#include "sv_filter.h"
#include "inetchannel.h"
#include "netadr.h"
#include "server.h"
#include "proto_oob.h"
#include "host.h"
#include "tier0/vcrmode.h"
#include "sv_plugin.h"
#include "sv_log.h"
#include "filesystem_engine.h"
#include "filesystem_init.h"
#include "tier0/icommandline.h"
#include "steam/steam_gameserver.h"
#include "hltvserver.h"
#include "sys_dll.h"
#if defined( REPLAY_ENABLED )
#include "replayserver.h"
#endif

extern ConVar sv_lan;
extern ConVar sv_visiblemaxplayers;
extern ConVar sv_region;
extern ConVar tv_enable;

static void sv_setsteamblockingcheck_f( IConVar *pConVar, const char *pOldString, float flOldValue );

ConVar  sv_steamblockingcheck( "sv_steamblockingcheck", "0", 0, 
							  "Check each new player for Steam blocking compatibility, 1 = message only, 2 >= drop if any member of owning clan blocks,"
							  "3 >= drop if any player has blocked, 4 >= drop if player has blocked anyone on server", sv_setsteamblockingcheck_f );

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#pragma warning( disable: 4355 ) // disables ' 'this' : used in base member initializer list'

ConVar sv_master_share_game_socket( "sv_master_share_game_socket", "1", 0, 
	"Use the game's socket to communicate to the master server. "
	"If this is 0, then it will create a socket on -steamport + 1 "
	"to communicate to the master server on." );

static char s_szTempMsgBuf[16000];

static void MsgAndLog( const char *fmt, ... )
{
	va_list ap;
	va_start(ap, fmt);
	V_vsprintf_safe( s_szTempMsgBuf, fmt, ap );

	// Does Log always print to the console?
	//if ( !engine->IsDedicatedServer() )
	//	Msg("%s", s_szTempMsgBuf );
	Log("%s", s_szTempMsgBuf );
}

static void WarningAndLog( const char *fmt, ... )
{
	va_list ap;
	va_start(ap, fmt);
	V_vsprintf_safe( s_szTempMsgBuf, fmt, ap );

	// Does Log always print to the console?
	Warning("%s", s_szTempMsgBuf );
	Log("%s", s_szTempMsgBuf );
}


//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
static CSteam3Server s_Steam3Server;
CSteam3Server  &Steam3Server()
{
	return s_Steam3Server;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSteam3Server::CSteam3Server() 
#if !defined(NO_STEAM)
:
	m_CallbackLogonSuccess( this, &CSteam3Server::OnLogonSuccess ),
	m_CallbackLogonFailure( this, &CSteam3Server::OnLogonFailure ),
	m_CallbackLoggedOff( this, &CSteam3Server::OnLoggedOff ),
	m_CallbackValidateAuthTicketResponse( this, &CSteam3Server::OnValidateAuthTicketResponse ),
	m_CallbackPlayerCompatibilityResponse( this, &CSteam3Server::OnComputeNewPlayerCompatibilityResponse ),
	m_CallbackGSPolicyResponse( this, &CSteam3Server::OnGSPolicyResponse )
#endif
{
	m_bHasActivePlayers = false;
	m_bLogOnResult = false;
	m_eServerMode = eServerModeInvalid;
	m_eServerType = eServerTypeNormal;
    m_bWantsSecure = false;		// default to insecure currently, this may change
    m_bInitialized = false;
	m_bWantsPersistentAccountLogon = false;
	m_bLogOnFinished = false;
	m_bMasterServerUpdaterSharingGameSocket = false;
	m_steamIDLanOnly.InstancedSet( 0,0, k_EUniversePublic, k_EAccountTypeInvalid );
	m_SteamIDGS.InstancedSet( 1, 0, k_EUniverseInvalid, k_EAccountTypeInvalid );
	m_QueryPort = 0;
}

//-----------------------------------------------------------------------------
// Purpose: detect current server mode based on cvars & settings
//-----------------------------------------------------------------------------
EServerMode CSteam3Server::GetCurrentServerMode()
{
	if ( sv_lan.GetBool() )
	{
		return eServerModeNoAuthentication;
	}
	else if ( CommandLine()->FindParm( "-insecure" ) )
	{
		return eServerModeAuthentication;
	}
	else 
	{
		return eServerModeAuthenticationAndSecure;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSteam3Server::~CSteam3Server()
{
	Shutdown();
}


void CSteam3Server::Activate( EServerType serverType )
{
	// we are active, check if sv_lan changed or we're trying to change server type
	if ( GetCurrentServerMode() == m_eServerMode && m_eServerType == serverType )
	{
		// we are active and LANmode/servertype didnt change. done.
		return;
	}

	if ( BIsActive() )
	{
		// shut down before we change server mode
		Shutdown();
	}

	m_unIP = INADDR_ANY;
	m_usPort = 26900;

	if ( CommandLine()->FindParm( "-steamport" ) )
	{
		m_usPort = CommandLine()->ParmValue( "-steamport", 26900 );
	}

	ConVarRef ipname( "ip" );
	if ( ipname.IsValid() )
	{
		netadr_t ipaddr;
		NET_StringToAdr( ipname.GetString(), &ipaddr );
		if ( !ipaddr.IsLoopback() && !ipaddr.IsLocalhost() )
		{
			m_unIP = ipaddr.GetIPHostByteOrder();
		}
	}

	m_eServerMode = GetCurrentServerMode();
	m_eServerType = serverType;

	char gamedir[MAX_OSPATH];
	Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );

	// Figure out the game port. If we're doing a SrcTV relay, then ignore the NS_SERVER port and don't tell Steam that we have a game server.
	uint16 usGamePort = 0;
	if ( serverType == eServerTypeNormal )
	{
		usGamePort = NET_GetUDPPort( NS_SERVER );
	}

	uint16 usMasterServerUpdaterPort;
	if ( sv_master_share_game_socket.GetBool() )
	{
		m_bMasterServerUpdaterSharingGameSocket = true;
		usMasterServerUpdaterPort = MASTERSERVERUPDATERPORT_USEGAMESOCKETSHARE;
		if ( serverType == eServerTypeTVRelay )
			m_QueryPort = NET_GetUDPPort( NS_HLTV );
		else
			m_QueryPort = usGamePort;
	}
	else
	{
		m_bMasterServerUpdaterSharingGameSocket = false;
		usMasterServerUpdaterPort = m_usPort;
		m_QueryPort = m_usPort;
	}
#ifndef _X360

	switch ( m_eServerMode )
	{
		case eServerModeNoAuthentication:
			MsgAndLog( "Initializing Steam libraries for LAN server\n" );
			break;
		case eServerModeAuthentication:
			MsgAndLog( "Initializing Steam libraries for INSECURE Internet server.  Authentication and VAC not requested.\n" );
			break;
		case eServerModeAuthenticationAndSecure:
			MsgAndLog( "Initializing Steam libraries for secure Internet server\n" );
			break;
		default:
			WarningAndLog( "Bogus eServermode %d!\n", m_eServerMode );
			Assert( !"Bogus server mode?!" );
			break;
	}

	SteamAPI_SetTryCatchCallbacks( false ); // We don't use exceptions, so tell steam not to use try/catch in callback handlers
	if ( CommandLine()->FindParm("-hushsteam") || !SteamGameServer_InitSafe(
			m_unIP,
			m_usPort+1,	// Steam lives on -steamport + 1, master server updater lives on -steamport.
			usGamePort,
			usMasterServerUpdaterPort,
			m_eServerMode,
			GetSteamInfIDVersionInfo().szVersionString ) )
	{
steam_no_good:
#if !defined( NO_STEAM )
		WarningAndLog( "*********************************************************\n" );
		WarningAndLog( "*\tUnable to load Steam support library.*\n" );
		WarningAndLog( "*\tThis server will operate in LAN mode only.*\n" );
		WarningAndLog( "*********************************************************\n" );
#endif
		m_eServerMode = eServerModeNoAuthentication;
		sv_lan.SetValue( true );
		return;
	}
	Init(); // Steam API context init
	if ( SteamGameServer() == NULL )
	{
		Assert( false );
		goto steam_no_good;
	}

	// Note that SteamGameServer_InitSafe() calls SteamAPI_SetBreakpadAppID() for you, which is what we don't want if we wish
	// to report crashes under a different AppId. Reset it back to our crashing one now.
	if ( sv.IsDedicated() )
	{
		SteamAPI_SetBreakpadAppID( GetSteamInfIDVersionInfo().ServerAppID );
	}

	// Set some stuff that should NOT change while the server is
	// running
	SteamGameServer()->SetProduct( GetSteamInfIDVersionInfo().szProductString );
	SteamGameServer()->SetGameDescription( serverGameDLL->GetGameDescription() );
	SteamGameServer()->SetDedicatedServer( sv.IsDedicated() );
	SteamGameServer()->SetModDir( gamedir );

	// Use anonymous logon, or persistent?
	if ( m_sAccountToken.IsEmpty() )
	{
		m_bWantsPersistentAccountLogon = false;
		MsgAndLog( "No account token specified; logging into anonymous game server account.  (Use sv_setsteamaccount to login to a persistent account.)\n" );
		SteamGameServer()->LogOnAnonymous();
	}
	else
	{
		m_bWantsPersistentAccountLogon = true;
		MsgAndLog( "Logging into Steam game server account\n" );

		// TODO: Change this to use just the token when the SDK is updated
		SteamGameServer()->LogOn( m_sAccountToken );
	}

#endif

	SendUpdatedServerDetails();
}


//-----------------------------------------------------------------------------
// Purpose: game server stopped, shutdown Steam game server session
//-----------------------------------------------------------------------------
void CSteam3Server::Shutdown()
{
	if ( !BIsActive() )
		return;

	SteamGameServer_Shutdown();
	m_bHasActivePlayers = false;
	m_bLogOnResult = false;
	m_SteamIDGS = k_steamIDNotInitYetGS;
	m_eServerMode = eServerModeInvalid;

	Clear(); // Steam API context shutdown
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the userid's are the same
//-----------------------------------------------------------------------------
bool CSteam3Server::CompareUserID( const USERID_t & id1, const USERID_t & id2 )
{
	if ( id1.idtype != id2.idtype )
		return false;

	switch ( id1.idtype )
	{
	case IDTYPE_STEAM:
	case IDTYPE_VALVE:
		{
			return (id1.steamid == id2.steamid );
		}
	default:
		break;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if this userid is already on this server
//-----------------------------------------------------------------------------
bool CSteam3Server::CheckForDuplicateSteamID( const CBaseClient *client )
{
	// in LAN mode we allow reuse of SteamIDs
	if ( BLanOnly() ) 
		return false;

	// Compare connecting client's ID to other IDs on the server
	for ( int i=0 ; i< sv.GetClientCount() ; i++ )
	{
		const IClient *cl = sv.GetClient( i );

		// Not connected, no SteamID yet
		if ( !cl->IsConnected() || cl->IsFakeClient() )
			continue;

		if ( cl->GetNetworkID().idtype != IDTYPE_STEAM )
			continue;

		// don't compare this client against himself in the list
		if ( client == cl )
			continue;

		if ( !CompareUserID( client->GetNetworkID(), cl->GetNetworkID() ) )
			continue;

		// SteamID is reused
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Called when secure policy is set
//-----------------------------------------------------------------------------
const CSteamID &CSteam3Server::GetGSSteamID()
{
	return m_SteamIDGS;
}


#if !defined(NO_STEAM)
//-----------------------------------------------------------------------------
// Purpose: Called when secure policy is set
//-----------------------------------------------------------------------------
void CSteam3Server::OnGSPolicyResponse( GSPolicyResponse_t *pPolicyResponse )
{
	if ( !BIsActive() )
		return;

	if ( SteamGameServer() && SteamGameServer()->BSecure() )
	{
		MsgAndLog( "VAC secure mode is activated.\n" );
	}
	else
	{
		MsgAndLog( "VAC secure mode disabled.\n" );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSteam3Server::OnLogonSuccess( SteamServersConnected_t *pLogonSuccess )
{
	if ( !BIsActive() )
		return;

	if ( !m_bLogOnResult )
	{
		m_bLogOnResult = true;
	}

	if ( !BLanOnly() )
	{
		MsgAndLog( "Connection to Steam servers successful.\n" );
		if ( SteamGameServer() )
		{
			uint32 ip = SteamGameServer()->GetPublicIP();
			MsgAndLog( "   Public IP is %d.%d.%d.%d.\n", (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255 );
		}
	}

	if ( SteamGameServer() )
	{
		m_SteamIDGS = SteamGameServer()->GetSteamID();
		if ( m_SteamIDGS.BAnonGameServerAccount() )
		{
			MsgAndLog( "Assigned anonymous gameserver Steam ID %s.\n", m_SteamIDGS.Render() );
		}
		else if ( m_SteamIDGS.BPersistentGameServerAccount() )
		{
			MsgAndLog( "Assigned persistent gameserver Steam ID %s.\n", m_SteamIDGS.Render() );
		}
		else
		{
			WarningAndLog( "Assigned Steam ID %s, which is of an unexpected type!\n", m_SteamIDGS.Render() );
			Assert( !"Unexpected steam ID type!" );
		}
	}
	else
	{
		m_SteamIDGS = k_steamIDNotInitYetGS;
	}

	// send updated server details
	// OnLogonSuccess() gets called each time we logon, so if we get dropped this gets called
	// again and we get need to retell the AM our details
	SendUpdatedServerDetails();
}


//-----------------------------------------------------------------------------
// Purpose: callback on unable to connect to the steam3 backend
// Input  : eResult - 
//-----------------------------------------------------------------------------
void CSteam3Server::OnLogonFailure( SteamServerConnectFailure_t *pLogonFailure )
{
	if ( !BIsActive() )
		return;

	//bool bRetrying = false;
	if ( !m_bLogOnResult )
	{
		if ( pLogonFailure->m_eResult == k_EResultServiceUnavailable )
		{
			if ( !BLanOnly() )
			{
				MsgAndLog( "Connection to Steam servers successful (SU).\n" );
			}
		}
		else
		{
			// we tried to be in secure mode but failed
			// force into insecure mode
			// eventually change this to set sv_lan as well
			if ( !BLanOnly() )
			{
				WarningAndLog( "Could not establish connection to Steam servers.  (Result = %d)\n", pLogonFailure->m_eResult );

				// If this was a permanent failure, switch to anonymous
				// TODO: Requires SDK update
				/*if ( m_bWantsPersistentAccountLogon && ( pLogonFailure->m_eResult == k_EResultInvalidParam || pLogonFailure->m_eResult == k_EResultAccountNotFound ) )
				{
					WarningAndLog( "Invalid game server account token. Retrying Steam connection with anonymous logon\n" );

					m_bWantsPersistentAccountLogon = false;
					bRetrying = true;
					SteamGameServer()->LogOnAnonymous();
				}*/
			}
		}
	}

	m_bLogOnResult = true;
	//m_bLogOnResult = !bRetrying;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : eResult - 
//-----------------------------------------------------------------------------
void CSteam3Server::OnLoggedOff( SteamServersDisconnected_t *pLoggedOff )
{
	if ( !BLanOnly() )
	{
		WarningAndLog( "Connection to Steam servers lost.  (Result = %d)\n", pLoggedOff->m_eResult );
	}
}

void CSteam3Server::OnComputeNewPlayerCompatibilityResponse( ComputeNewPlayerCompatibilityResult_t *pCompatibilityResult )
{
	CBaseClient *client = ClientFindFromSteamID( pCompatibilityResult->m_SteamIDCandidate );
	if ( !client )
		return;
	if ( sv_steamblockingcheck.GetInt() )
	{
		if ( sv_steamblockingcheck.GetInt() >= 2 )
		{
			if ( pCompatibilityResult->m_cClanPlayersThatDontLikeCandidate > 0 )
			{
				client->Disconnect( "Another player on this server ( member of owning clan ) does not want to play with this player." );
				return;
			}
		}		
		if ( sv_steamblockingcheck.GetInt() >= 3 )
		{
			if ( pCompatibilityResult->m_cPlayersThatDontLikeCandidate > 0 )
			{
				client->Disconnect( "Another player on this server does not want to play with this player." );
				return;
			}
		}
		if ( sv_steamblockingcheck.GetInt() >= 4 )
		{
			if ( pCompatibilityResult->m_cPlayersThatCandidateDoesntLike > 0 )
			{
				client->Disconnect( "Existing player on this server is on this players block list." );
				return;
			}
		}

		if ( pCompatibilityResult->m_cClanPlayersThatDontLikeCandidate > 0 ||
			pCompatibilityResult->m_cPlayersThatDontLikeCandidate > 0 ||
			pCompatibilityResult->m_cPlayersThatCandidateDoesntLike > 0 )
		{
			MsgAndLog( "Player %s is blocked by %d players and %d clan members and has blocked %d players on server\n", client->GetClientName(), 
					pCompatibilityResult->m_cPlayersThatDontLikeCandidate,
					pCompatibilityResult->m_cClanPlayersThatDontLikeCandidate,
					pCompatibilityResult->m_cPlayersThatCandidateDoesntLike	);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSteam3Server::OnValidateAuthTicketResponse( ValidateAuthTicketResponse_t *pValidateAuthTicketResponse )
{
	//Msg("Steam backend:Got approval for %x\n", pGSClientApprove->m_SteamID.ConvertToUint64() );
	// We got the approval message from the back end.
    // Note that if we dont get it, we default to approved anyway
	// dont need to send anything back

	if ( !BIsActive() )
		return;

	CBaseClient *client = ClientFindFromSteamID( pValidateAuthTicketResponse->m_SteamID );
	if ( !client )
		return;

	if ( pValidateAuthTicketResponse->m_eAuthSessionResponse != k_EAuthSessionResponseOK )
	{
		OnValidateAuthTicketResponseHelper( client, pValidateAuthTicketResponse->m_eAuthSessionResponse );
		return;
	}

	if ( Filter_IsUserBanned( client->GetNetworkID() ) )
	{
		sv.RejectConnection( client->GetNetChannel()->GetRemoteAddress(), client->GetClientChallenge(), "#GameUI_ServerRejectBanned" );
		client->Disconnect( va( "STEAM UserID %s is banned", client->GetNetworkIDString() ) );
	}
	else if ( CheckForDuplicateSteamID( client ) )
	{
		client->Disconnect(  "STEAM UserID %s is already\nin use on this server", client->GetNetworkIDString() );					
	}
	else
	{
		char msg[ 512 ];
		sprintf( msg, "\"%s<%i><%s><>\" STEAM USERID validated\n", client->GetClientName(), client->GetUserID(), client->GetNetworkIDString() );

		DevMsg( "%s", msg );
		g_Log.Printf( "%s", msg );

		g_pServerPluginHandler->NetworkIDValidated( client->GetClientName(), client->GetNetworkIDString() );

		// Tell IServerGameClients if its version is high enough.
		if ( g_iServerGameClientsVersion >= 4 )
		{
			serverGameClients->NetworkIDValidated( client->GetClientName(), client->GetNetworkIDString() );
		}
	}

	if ( sv_steamblockingcheck.GetInt() >= 1 )
	{
		SteamGameServer()->ComputeNewPlayerCompatibility( pValidateAuthTicketResponse->m_SteamID );
	}
	client->SetFullyAuthenticated();
}


//-----------------------------------------------------------------------------
// Purpose: helper for the two places that deny a user connect
// Input  : steamID - id to kick
//			eDenyReason - reason
//			pchOptionalText - some kicks also have a string with them
//-----------------------------------------------------------------------------
void CSteam3Server::OnValidateAuthTicketResponseHelper( CBaseClient *cl, EAuthSessionResponse eAuthSessionResponse )
{
	INetChannel *netchan = cl->GetNetChannel();

	// If the client is timing out, the Steam failure is probably related (e.g. game crashed). Let's just print that the client timed out.
	if ( netchan && netchan->IsTimingOut() )
	{
		cl->Disconnect( CLIENTNAME_TIMED_OUT, cl->GetClientName() );
		return;
	}

	// Emit a more detailed diagnostic.
	WarningAndLog( "STEAMAUTH: Client %s received failure code %d\n", cl->GetClientName(), (int)eAuthSessionResponse );

	switch ( eAuthSessionResponse )
	{
		case k_EAuthSessionResponseUserNotConnectedToSteam:
			if ( !BLanOnly() ) 
				cl->Disconnect( INVALID_STEAM_LOGON_NOT_CONNECTED );
			break;
		case k_EAuthSessionResponseLoggedInElseWhere:
			if ( !BLanOnly() ) 
				cl->Disconnect( INVALID_STEAM_LOGGED_IN_ELSEWHERE );
			break;
		case k_EAuthSessionResponseNoLicenseOrExpired:
			cl->Disconnect( "This Steam account does not own this game. \nPlease login to the correct Steam account" );
			break;
		case k_EAuthSessionResponseVACBanned:
			if ( !BLanOnly() ) 
				cl->Disconnect( INVALID_STEAM_VACBANSTATE );
			break;
		case k_EAuthSessionResponseAuthTicketCanceled:
			if ( !BLanOnly() ) 
				cl->Disconnect( INVALID_STEAM_LOGON_TICKET_CANCELED );
			break;
		case k_EAuthSessionResponseAuthTicketInvalidAlreadyUsed:
		case k_EAuthSessionResponseAuthTicketInvalid:
			if ( !BLanOnly() ) 
				cl->Disconnect( INVALID_STEAM_TICKET );
			break;
		case k_EAuthSessionResponseVACCheckTimedOut:
			cl->Disconnect( "An issue with your computer is blocking the VAC system. You cannot play on secure servers.\n\nhttps://support.steampowered.com/kb_article.php?ref=2117-ILZV-2837" );
			break;
		default:
			cl->Disconnect( "Client dropped by server" );
			break;
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : steamIDFind - 
// Output : IClient
//-----------------------------------------------------------------------------
CBaseClient *CSteam3Server::ClientFindFromSteamID( CSteamID & steamIDFind )
{
	for ( int i=0 ; i< sv.GetClientCount() ; i++ )
	{
		CBaseClient *cl = (CBaseClient *)sv.GetClient( i );

		// Not connected, no SteamID yet
		if ( !cl->IsConnected() || cl->IsFakeClient() )
			continue;

		if ( cl->GetNetworkID().idtype != IDTYPE_STEAM )
			continue;

		USERID_t id = cl->GetNetworkID();
		if (id.steamid == steamIDFind )
		{
			return cl;
		}
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: tell Steam that a new user connected
//-----------------------------------------------------------------------------
bool CSteam3Server::NotifyClientConnect( CBaseClient *client, uint32 unUserID, netadr_t & adr, const void *pvCookie, uint32 ucbCookie )
{
	if ( !BIsActive() ) 
		return true;

	if ( !client || client->IsFakeClient() )
		return false;

	// Make sure their ticket is long enough
	if ( ucbCookie <= sizeof(uint64) )
	{
		WarningAndLog("Client UserID %x connected with invalid ticket size %d\n", unUserID, ucbCookie );
		return false;
	}

	// steamID is prepended to the ticket
	CUtlBuffer buffer( pvCookie, ucbCookie, CUtlBuffer::READ_ONLY );
	uint64 ulSteamID = buffer.GetInt64();

	CSteamID steamID( ulSteamID );
	if ( steamID.GetEUniverse() != SteamGameServer()->GetSteamID().GetEUniverse() )
	{
		WarningAndLog("Client %d %s connected to universe %d, but game server %s is running in universe %d\n", unUserID, steamID.Render(),
			steamID.GetEUniverse(), SteamGameServer()->GetSteamID().Render(), SteamGameServer()->GetSteamID().GetEUniverse() );
		return false;
	}
	if ( !steamID.IsValid() || !steamID.BIndividualAccount() )
	{
		WarningAndLog("Client %d connected from %s with invalid Steam ID %s\n", unUserID, adr.ToString(), steamID.Render() );
		return false;
	}

	// skip the steamID
	pvCookie = (uint8 *)pvCookie + sizeof( uint64 );
	ucbCookie -= sizeof( uint64 );
	EBeginAuthSessionResult eResult = SteamGameServer()->BeginAuthSession( pvCookie, ucbCookie, steamID );
	switch ( eResult )
	{
	case k_EBeginAuthSessionResultOK:
		//Msg("S3: BeginAuthSession request for %x was good.\n", steamID.ConvertToUint64( ) );
		break;
	case k_EBeginAuthSessionResultInvalidTicket:
		WarningAndLog("S3: Client connected with invalid ticket: UserID: %x\n", unUserID );
		return false;
	case k_EBeginAuthSessionResultDuplicateRequest:
		WarningAndLog("S3: Duplicate client connection: UserID: %x SteamID %x\n", unUserID, steamID.ConvertToUint64( ) );
		return false;
	case k_EBeginAuthSessionResultInvalidVersion:
		WarningAndLog("S3: Client connected with invalid ticket ( old version ): UserID: %x\n", unUserID );
		return false;
	case k_EBeginAuthSessionResultGameMismatch:
		// This error would be very useful to present to the client.
		WarningAndLog("S3: Client connected with ticket for the wrong game: UserID: %x\n", unUserID );
		return false;
	case k_EBeginAuthSessionResultExpiredTicket:
		WarningAndLog("S3: Client connected with expired ticket: UserID: %x\n", unUserID );
		return false;
	default:
		WarningAndLog("S3: Client failed auth session for unknown reason. UserID: %x\n", unUserID );
		return false;
	}

	// first checks ok, we know now the SteamID
	client->SetSteamID( steamID );

	SendUpdatedServerDetails();

	return true;
}

bool CSteam3Server::NotifyLocalClientConnect( CBaseClient *client )
{
	CSteamID steamID;

	if ( SteamGameServer() )
	{
		steamID = SteamGameServer()->CreateUnauthenticatedUserConnection();
	}
	
	client->SetSteamID( steamID );

	SendUpdatedServerDetails();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *client - 
//-----------------------------------------------------------------------------
void CSteam3Server::NotifyClientDisconnect( CBaseClient *client )
{
	if ( !client || !BIsActive() || !client->IsConnected() || !client->m_SteamID.IsValid() )
		return;

	// Check if the client has a local (anonymous) steam account.  This is the
	// case for bots.  Currently it's also the case for people who connect
	// directly to the SourceTV port.
	if ( client->m_SteamID.GetEAccountType() == k_EAccountTypeAnonGameServer )
	{
		SteamGameServer()->SendUserDisconnect( client->m_SteamID );

		// Clear the steam ID, as it was a dummy one that should not be used again
		client->m_SteamID = CSteamID();
	}
	else
	{

		// All bots should have an anonymous account ID
		Assert( !client->IsFakeClient() );

		USERID_t id = client->GetNetworkID();
		if ( id.idtype != IDTYPE_STEAM )
			return;
	
		// Msg("S3: Sending client disconnect for %x\n", steamIDClient.ConvertToUint64( ) );
		SteamGameServer()->EndAuthSession( client->m_SteamID );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSteam3Server::NotifyOfLevelChange()
{
	// we're changing levels, so we may not respond for a while
	if ( m_bHasActivePlayers )
	{
		m_bHasActivePlayers = false;
		SendUpdatedServerDetails();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSteam3Server::NotifyOfServerNameChange()
{
	SendUpdatedServerDetails();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSteam3Server::RunFrame()
{
	bool bHasPlayers = ( sv.GetNumClients() > 0 );

	if ( m_bHasActivePlayers != bHasPlayers )
	{
		m_bHasActivePlayers = bHasPlayers;
		SendUpdatedServerDetails();
	}

	static double s_fLastRunCallback = 0.0f;
	double fCurtime = Plat_FloatTime();
	if ( fCurtime - s_fLastRunCallback > 0.1f )
	{
		s_fLastRunCallback = fCurtime;
		SteamGameServer_RunCallbacks();
	}
}


//-----------------------------------------------------------------------------
// Purpose: lets the steam3 servers know our full details
// Input  : bChangingLevels - true if we're going to heartbeat slowly for a while
//-----------------------------------------------------------------------------
void CSteam3Server::SendUpdatedServerDetails()
{
	if ( !BIsActive() || SteamGameServer() == NULL )
		return;

	// Fetch counts that include the dummy slots for SourceTV and reply.
	int nNumClients = sv.GetNumClients();
	int nMaxClients = sv.GetMaxClients();
	int nFakeClients = sv.GetNumFakeClients();

	// Now remove any dummy slots reserved for the Source TV or replay
	// listeners.  The fact that these are "players" should be a Source-specific
	// implementation artifact, and this kludge --- I mean ELEGANT SOLUTION ---
	// should not be propagated to the Steam layer.  Steam should be able to report
	// exactly what we give it to the master server, etc.
	for ( int i = 0 ; i < sv.GetClientCount() ; ++i )
	{
		CBaseClient *cl = (CBaseClient *)sv.GetClient( i );
		if ( !cl->IsConnected() )
			continue;

		bool bHideClient = false;
		if ( cl->IsReplay() || cl->IsHLTV() )
		{
			Assert( cl->IsFakeClient() );
			bHideClient = true;
		}

		if ( cl->IsFakeClient() && !cl->ShouldReportThisFakeClient() )
		{
			bHideClient = true;
		}

		if ( bHideClient )
		{
			--nNumClients;
			--nMaxClients;
			--nFakeClients;

			// And make sure we don't have any local player authentication
			// records within steam for this guy.
			if ( cl->m_SteamID.IsValid() )
			{
				Assert( cl->m_SteamID.BAnonGameServerAccount() );
				SteamGameServer()->SendUserDisconnect( cl->m_SteamID );
				cl->m_SteamID = CSteamID();
			}
		}
	}

	// Apply convar to force reported max player count LAST
	if ( sv_visiblemaxplayers.GetInt() > 0 && sv_visiblemaxplayers.GetInt() < nMaxClients )
		nMaxClients = sv_visiblemaxplayers.GetInt();

	SteamGameServer()->SetMaxPlayerCount( nMaxClients );
	SteamGameServer()->SetBotPlayerCount( nFakeClients );
	SteamGameServer()->SetPasswordProtected( sv.GetPassword() != NULL );
	SteamGameServer()->SetRegion( sv_region.GetString() );
	SteamGameServer()->SetServerName( sv.GetName() );
	if ( hltv && hltv->IsTVRelay() )
	{
		// If we're a relay we can't use the local server data for these
		SteamGameServer()->SetMapName( hltv->GetMapName() );
		SteamGameServer()->SetMaxPlayerCount( hltv->GetMaxClients() );
		SteamGameServer()->SetBotPlayerCount( 0 );
	}
	else
	{
		const char *pszMap = NULL;
		if ( g_iServerGameDLLVersion >= 9 )
			pszMap = serverGameDLL->GetServerBrowserMapOverride();
		if ( pszMap == NULL || *pszMap == '\0' )
			pszMap = sv.GetMapName();
		SteamGameServer()->SetMapName( pszMap );
	}
	if ( hltv && hltv->IsActive() )
	{
		// This is also the case when we're a relay, in which case we never set a game port, so we'll only have a spectator port
		SteamGameServer()->SetSpectatorPort( NET_GetUDPPort( NS_HLTV ) );
		SteamGameServer()->SetSpectatorServerName( hltv->GetName() );
	}
	else
	{
		SteamGameServer()->SetSpectatorPort( 0 );
	}

	UpdateGroupSteamID( false );

	// Form the game data to send

	CUtlString sGameData;

	// Start with whatever the game has
	if ( g_iServerGameDLLVersion >= 9 )
		sGameData = serverGameDLL->GetServerBrowserGameData();

	// Add the value of our steam blocking flag
	char rgchTag[32];
	V_sprintf_safe( rgchTag, "steamblocking:%d", sv_steamblockingcheck.GetInt() );
	if ( !sGameData.IsEmpty() )
	{
		sGameData.Append( "," );
	}
	sGameData.Append( rgchTag );

	SteamGameServer()->SetGameData( sGameData );

//	Msg( "CSteam3Server::SendUpdatedServerDetails: nNumClients=%d, nMaxClients=%d, nFakeClients=%d:\n", nNumClients, nMaxClients, nFakeClients );
//	for ( int i = 0 ; i < sv.GetClientCount() ; ++i )
//	{
//		IClient	*c = sv.GetClient( i );
//		Msg("  %d: %s, connected=%d, replay=%d, fake=%d\n", i, c->GetClientName(), c->IsConnected() ? 1 : 0, c->IsReplay() ? 1 : 0, c->IsFakeClient() ? 1 : 0 );
//	}
}

bool CSteam3Server::IsMasterServerUpdaterSharingGameSocket()
{
	return m_bMasterServerUpdaterSharingGameSocket;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Heartbeat_f()
{

	if( Steam3Server().SteamGameServer() )
	{
		Steam3Server().SteamGameServer()->ForceHeartbeat();
	}
}

static ConCommand heartbeat( "heartbeat", Heartbeat_f, "Force heartbeat of master servers", 0 );


//-----------------------------------------------------------------------------
// Purpose: Select Steam gameserver account to login to
//-----------------------------------------------------------------------------
void sv_setsteamaccount_f( const CCommand &args )
{
	if ( Steam3Server().SteamGameServer() && Steam3Server().SteamGameServer()->BLoggedOn() )
	{
		Warning( "Warning: Game server already logged into steam.  You need to use the sv_setsteamaccount command earlier.\n");
		return;
	}

	if ( sv_lan.GetBool() )
	{
		Warning( "Warning: sv_setsteamaccount is not applicable in LAN mode.\n");
	}

	if ( args.ArgC() != 2 )
	{
		Warning( "Usage: sv_setsteamaccount <login_token>\n");
		return;
	}

	Steam3Server().SetAccount( args[1] );
}

static ConCommand sv_setsteamaccount( "sv_setsteamaccount", sv_setsteamaccount_f, "token\nSet game server account token to use for logging in to a persistent game server account", 0 );


static void sv_setsteamgroup_f( IConVar *pConVar, const char *pOldString, float flOldValue );

ConVar			sv_steamgroup( "sv_steamgroup", "", FCVAR_NOTIFY, "The ID of the steam group that this server belongs to. You can find your group's ID on the admin profile page in the steam community.", sv_setsteamgroup_f );


void CSteam3Server::UpdateGroupSteamID( bool bForce )
{
	if ( sv_steamgroup.GetInt() == 0 && !bForce )
		return;
	uint unAccountID = Q_atoi( sv_steamgroup.GetString() );
	m_SteamIDGroupForBlocking.Set( unAccountID, m_SteamIDGS.GetEUniverse(), k_EAccountTypeClan );
	if ( SteamGameServer() )
		SteamGameServer()->AssociateWithClan( m_SteamIDGroupForBlocking );
}

static void sv_setsteamgroup_f( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	if ( sv_lan.GetBool() )
	{
		Warning( "Warning: sv_steamgroup is not applicable in LAN mode.\n");
	}
	Steam3Server().UpdateGroupSteamID( true );
}

static void sv_setsteamblockingcheck_f( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	if ( sv_lan.GetBool() )
	{
		Warning( "Warning: sv_steamblockingcheck is not applicable in LAN mode.\n");
	}
}
