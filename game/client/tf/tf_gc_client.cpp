//========= Copyright Valve Corporation, All rights reserved. ============//
#include "cbase.h"
#include "tf_gc_client.h"
#include "gcsdk/gcsdk_auto.h"
#include "tf_gcmessages.h"
#include "kvpacker.h"
#include "tf_party.h"
// XXX(JohnS): Eventually, we want to send a smaller lobby object to clients. For now, they use the CTFGSLobby, which is
//             in shared code for that reason.
#include "tf_lobby_server.h"
#include "base_gcmessages.pb.h"
#include "igameevents.h"
#include "netadr.h"
#include "protocol.h"
#include "econ_item_inventory.h"
#include "tf_item_inventory.h"
#include "tf_hud_mann_vs_machine_status.h"
#include "econ/confirm_dialog.h"
#include "rtime.h"
#include "ienginevgui.h"
#include "clientmode_tf.h"
#include "tf_match_description.h"
#include "tf_xp_source.h"
#include "tf_notification.h"
#include "c_tf_notification.h"
#include "tf_gc_shared.h"
#include <google/protobuf/text_format.h>
#include "tf_match_join_handlers.h"
#include "tf_matchmaking_dashboard.h"
#include "tf_ladder_data.h"
#include "tf_rating_data.h"

#include "econ_item_description.h"

#include "tf_hud_disconnect_prompt.h"

#include "util_shared.h"
#include <steamnetworkingsockets/isteamnetworkingutils.h>
#include <steamnetworkingsockets/isteamnetworkingsockets.h>
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void SelectGroup( EMatchmakingGroupType eGroup, bool bSelected );

#ifdef _DEBUG
	#define GCMATCHMAKING_DEBUG_LEVEL 4
#else
	#define GCMATCHMAKING_DEBUG_LEVEL 1
#endif
#define GCMatchmakingDebugSpew( lvl, ...) do { if ( lvl <= GCMATCHMAKING_DEBUG_LEVEL ) { Msg( __VA_ARGS__); } } while(false)

#define MM_REJOIN_WAIT_TIME	1.0f

static const char* s_pszCasualCriteriaSaveFileName = "casual_criteria.vdf";


// Ping stuff
static ConVar  tf_datacenter_ping_interval( "tf_datacenter_ping_interval", "180", FCVAR_DEVELOPMENTONLY );

#ifdef TF_GC_PING_DEBUG
	#define CV_TF_DATACENTER_PING_DEBUG_DEFAULT "1"
#else
	#define CV_TF_DATACENTER_PING_DEBUG_DEFAULT "0"
#endif
static ConVar tf_datacenter_ping_debug( "tf_datacenter_ping_debug", CV_TF_DATACENTER_PING_DEBUG_DEFAULT, FCVAR_INTERNAL_USE );

static bool BPingDebug() { return tf_datacenter_ping_debug.GetBool(); }
#define TFPingMsg(...) Msg("[SDR Ping] " __VA_ARGS__)
#define TFPingDbg(...) if ( BPingDebug() ) { TFPingMsg( __VA_ARGS__ ); }

// Allow disabling for staging. Will only send dummy values set by the overrides above
#ifdef TF_GC_PING_DEBUG
#include "tier0/icommandline.h"
static bool BUseSteamDatagram() { return !CommandLine()->CheckParm("-nosteamdatagram" ); }
#else
static bool BUseSteamDatagram() { return true; }
#endif

using namespace GCSDK;

// @FD We need this for TF?
//DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_CONSOLE, "Console" );

static CTFGCClientSystem s_TFGCClientSystem;
CTFGCClientSystem *GTFGCClientSystem() { return &s_TFGCClientSystem; }

// Dialog Prompt Asking users if they want to rejoin a MvM Game
static CTFRejoinConfirmDialog *s_pRejoinLobbyDialog;

//bool g_bClientReceivedGCWelcome = false;
//bool CTFGCClientSystem::HasGCUserSessionBeenCreated() { return g_bClientReceivedGCWelcome; }

//static ConVar tf_spectator_auto_spectate_games( "tf_spectator_auto_spectate_games", "0", 0, "Automatically spectate available games" );
//static ConVar tf_auto_connect( "tf_auto_connect", "", 0, "Automatically connect to the specified server forever" );
static ConVar tf_matchgroups( "tf_matchgroups", "0", FCVAR_ARCHIVE, "Bit masks of match groups to search in for matchmaking" );
//static ConVar tf_auto_create_proxy( "tf_auto_create_proxy", "0", 0, "Automatically create a proxy" );
//ConVar tf_debug_today_message_sorting( "tf_debug_today_message_sorting", "0", 0, "Print out unsorted and sorted today messages to the console" );

#ifdef STAGING_ONLY
CON_COMMAND( cl_check_process_count, "cl_check_process_count" )
{
	int iProcessCount = engine->GetInstancesRunningCount();
	Msg( "cl_check_process_count - %d \n", iProcessCount );
}
#endif

// This triggers a GC packet so isn't great to let clients misuse
#if defined( STAGING_ONLY ) || defined( _DEBUG )
CON_COMMAND( tf_datacenter_ping_refresh, "Force an immediate refresh of datacenter ping" )
{
	GTFGCClientSystem()->InvalidatePingData();
}
#endif // defined( STAGING_ONLY ) || defined( _DEBUG )

CON_COMMAND( tf_datacenter_ping_dump, "Dump current datacenter ping values to console" )
{
	GTFGCClientSystem()->DumpPing();
}

static void OnRejoinMvMLobbyDialogCallBack( bool bConfirmed, void *pContext )
{
	GTFGCClientSystem()->RejoinLobby( bConfirmed );
	s_pRejoinLobbyDialog = NULL;
}

void SubscribeToLocalPlayerSOCache( ISharedObjectListener* pListener )
{
	if ( steamapicontext && steamapicontext->SteamUser() )
	{
		CSteamID steamID = steamapicontext->SteamUser()->GetSteamID();
		GCClientSystem()->GetGCClient()->AddSOCacheListener( steamID, pListener );
	}
	else
	{
		Assert( !"Failed to subscribe to local user's SOCache!" );
	}
}

// Helper to add or replace a ping entry in an update
static void ApplyPingToMsg( CMsgGCDataCenterPing_Update &msg, const CMsgGCDataCenterPing_Update_PingEntry &entry )
{
	// Existing?
	const char *pszName = entry.name().c_str();
	CMsgGCDataCenterPing_Update_PingEntry *pEntry = NULL;
	for ( int j = 0; j < msg.pingdata_size(); ++j )
	{
		CMsgGCDataCenterPing_Update_PingEntry& existingEntry = *msg.mutable_pingdata(j);

		if ( V_stricmp( existingEntry.name().c_str(), pszName ) == 0 )
		{
			pEntry = &existingEntry;
			break;
		}
	}

	// New?
	if ( !pEntry )
	{
		pEntry = msg.add_pingdata();
	}

	pEntry->CopyFrom( entry );
}

const char *CTFGCClientSystem::k_pszSteamLobbyKey_PartyID = "PartyID";

//-----------------------------------------------------------------------------
// Reliable messages
//-----------------------------------------------------------------------------
class ReliableMsgNotificationAcknowledge : public CJobReliableMessageBase < ReliableMsgNotificationAcknowledge,
                                                                            CMsgNotificationAcknowledge,
                                                                            k_EMsgGC_NotificationAcknowledge,
                                                                            CMsgNotificationAcknowledgeReply,
                                                                            k_EMsgGC_NotificationAcknowledgeReply >
{
public:
	const char *MsgName() { return "NotificationAcknowledge"; }
	void InitDebugString( CUtlString &dbgStr )
	{
		dbgStr.Format( "Account %u / Notification %016llx",
		               Msg().Body().account_id(), Msg().Body().notification_id() );
	}
};

CTFGCClientSystem::CTFGCClientSystem()
:	m_pPendingCreateOrUpdatePartyMsg( NULL )
,	m_flSendPartyUpdateMessageTime( FLT_MAX )
,	m_nMostSearchedMapCount( 0 )
,	m_WorldStatus()
,	m_bRegisteredSharedObjects( false )
,	m_bInittedGC( false )
,	m_eAcceptInviteStep( eAcceptInviteStep_None )
,	m_eCreateLobbyStatus( k_EResultOK )
,	m_bWantToActivateInviteUI( false )
,	m_steamIDGCAssignedMatch()
,	m_bAssignedMatchEnded( false )
,	m_eAssignedMatchGroup( k_nMatchGroup_Invalid )
,	m_uAssignedMatchID( 0 )
,	m_bServerAssignmentChanged( false )
,	m_rtLastPingFix( 0 )
,	m_bPendingPingRefresh( false )
,	m_bSentInitialPingFix( false )
,	m_flCheckForRejoinTime( 0 )
,	m_pSOCache( NULL )
,	m_eConnectState( eConnectState_Disconnected )
,	m_bGCUserSessionCreated( false )
,	m_bUserWantsToBeInMatchmaking( false )
,	m_nPendingAutoJoinPartyID( 0 )
,	m_eLocalWizardStep( TF_Matchmaking_WizardStep_INVALID )
,	m_callbackSteamLobbyCreated( this, &CTFGCClientSystem::OnSteamLobbyCreated )
,	m_callbackSteamLobbyEnter( this, &CTFGCClientSystem::OnSteamLobbyEnter )
,	m_callbackSteamLobbyChatMsg( this, &CTFGCClientSystem::OnSteamLobbyChatMsg )
,	m_callbackSteamGameLobbyJoinRequested( this, &CTFGCClientSystem::OnSteamGameLobbyJoinRequested )
,	m_callbackSteamLobbyDataUpdate( this, &CTFGCClientSystem::OnSteamLobbyDataUpdate )
,	m_callbackSteamLobbyChatUpdate( this, &CTFGCClientSystem::OnSteamLobbyChatUpdate )
{
	// replace base GCClientSystem
	SetGCClientSystem( this );

	s_pRejoinLobbyDialog = NULL;

//	if ( g_bClientReceivedGCWelcome )
//	{
//		Msg( "CTFGCClientSystem::CTFGCClientSystem firing event\n" );
//
//		IGameEvent *pEvent = gameeventmanager->CreateEvent( "gc_user_session_created" );
//		if ( pEvent )
//		{
//			gameeventmanager->FireEventClientSide( pEvent );
//		}
//	}
//	else
//	{
//		Msg( "CTFGCClientSystem::CTFGCClientSystem user session not yet created\n" );
//	}
}


CTFGCClientSystem::~CTFGCClientSystem( void )
{
	// Prevent other system from using this pointer after it's destroyed
	SetGCClientSystem( NULL );
}

////-----------------------------------------------------------------------------
//// Purpose: Asynchronous job for getting news
////-----------------------------------------------------------------------------
//class CGCClientJobGetNews : public GCSDK::CGCClientJob
//{
//public:
//	CGCClientJobGetNews( GCSDK::CGCClient *pGCClient, int nAppID ) : GCSDK::CGCClientJob( pGCClient )
//	{
//		m_nAppID = nAppID;
//	}
//
//	virtual bool BYieldingRunJob( void *pvStartParam )
//	{
//		CGCMsg<MsgGCGetNews_t> msgGetNews( k_EMsgGCGetNews );	
//		msgGetNews.Body().m_unAppID = m_nAppID;
//
//		GCSDK::CGCMsg<MsgGCNewsReponse_t> msgResponse( k_EMsgGCNewsResponse );
//		bool bRet = BYldSendMessageAndGetReply( msgGetNews, 150, &msgResponse, k_EMsgGCNewsResponse );
//		//Assert( bRet );
//
//		if ( !bRet )
//		{
//			Warning( "CGCClientJobGetNews failed to get reply\n" );
//			GTFGCClientSystem()->SetGetNewsTime( Plat_FloatTime() + 5.0f );
//			return false;
//		}
//
//		//deserialize KV
//		CUtlBuffer bufNews;
//		bufNews.Put( msgResponse.PubReadCur(), msgResponse.Body().m_cMsgLen );
//
//		KVPacker packer;
//		KeyValues *pNewsKeys = GTFGCClientSystem()->GetNewsKeys();
//		pNewsKeys->Clear();
//		if ( !packer.ReadAsBinary( pNewsKeys, bufNews ) )
//		{
//			Warning( "Failed to deserialize key values from news request\n" );
//			return false;
//		}
//
//		//KeyValuesDumpAsDevMsg( pNewsKeys );
//
//		IGameEvent *pEvent = gameeventmanager->CreateEvent( "news_updated" );
//		if ( pEvent )
//		{
//			gameeventmanager->FireEventClientSide( pEvent );
//		}
//
//		return true;
//	}
//
//private:
//	int m_nAppID;
//};


////-----------------------------------------------------------------------------
//// Purpose: Asynchronous job for pinging the GC with a hello until we get
//// a welcome
////-----------------------------------------------------------------------------
//class CGCClientJobHello : public GCSDK::CGCClientJob
//{
//public:
//	CGCClientJobHello( GCSDK::CGCClient *pGCClient ) 
//	: GCSDK::CGCClientJob( pGCClient )
//	{
//	}
//
//	virtual bool BYieldingRunJob( void *pvStartParam )
//	{
//		CProtoBufMsg<CMsgClientHello> msg( k_EMsgGCClientHello );	
//		msg.Body().set_version( engine->GetClientVersion() );
//
//		while ( !g_bClientReceivedGCWelcome )
//		{
//			// Wait two seconds between messages
//			BYieldingWaitTime( 2 * k_nMillion );
//
//			if ( !m_pGCClient->BSendMessage( msg ) )
//				return false;
//		}
//		return true;
//	}
//};


////-----------------------------------------------------------------------------
//class CGCClientJobFindSourceTVGamesAutoSpectate : public GCSDK::CGCClientJob
//{
//public:
//	CGCClientJobFindSourceTVGamesAutoSpectate( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient )
//	{
//	}
//
//	virtual bool BYieldingRunJob( void *pvStartParam )
//	{
//		CProtoBufMsg<CMsgFindSourceTVGames> msg( k_EMsgGCFindSourceTVGames );	
//		CProtoBufMsg<CMsgSourceTVGamesResponse> msgResponse( k_EMsgGCSourceTVGamesResponse );
//
//		static ConVarRef sv_search_key("sv_search_key");
//		if ( sv_search_key.IsValid() && *sv_search_key.GetString() )
//		{
//			msg.Body().set_search_key( sv_search_key.GetString() );
//		}
//
//		msg.Body().set_start( 0 );
//		msg.Body().set_num_games( 10 );
//
//		bool bRet = BYldSendMessageAndGetReply( msg, 15, &msgResponse, k_EMsgGCSourceTVGamesResponse );
//
//		GTFGCClientSystem()->SetAutoSpectateCheckTime( Plat_FloatTime() + 30.0f ); // try again in 30 seconds
//
//		if ( !bRet )
//		{
//			Warning( "CGCClientJobFindSourceTVGamesDebug failed to get reply\n" );
//			return false;
//		}
//
//		if ( GTFGCClientSystem()->GetSignonState() != SIGNONSTATE_NONE || !msgResponse.Body().games_size() )
//		{
//			return true; // already connected somewhere else
//		}
//		
//		const CSourceTVGame &game = msgResponse.Body().games( RandomInt( 0, msgResponse.Body().games_size() - 1 ));
//
//		GTFGCClientSystem()->StartWatchingGame( game.server_steamid() );
//		return true;
//	}
//
//private:
//};

void CTFGCClientSystem::LoadCasualSearchCriteria()
{
	// Read casual criteria if the file exists
	CUtlBuffer buffer;
	buffer.SetBufferType( true, true );
	if ( g_pFullFileSystem->ReadFile( s_pszCasualCriteriaSaveFileName, NULL, buffer ) &&
		buffer.TellPut() > buffer.TellGet() )
	{
		// Null terminate. Why is buffer this pseudo-text class but has AddNullTerminator private?
		const char zero = '\0';
		buffer.Put( &zero, sizeof( zero ) );

		std::string strIn( (const char *)buffer.PeekGet() );

		google::protobuf::TextFormat::ParseFromString( strIn, m_msgLocalSearchCriteria.mutable_casual_criteria() );

		// let the CCasualCriteriaHelper validate/cleanup the bits that we've just loaded
		CCasualCriteriaHelper casualHelper( m_msgLocalSearchCriteria.casual_criteria() );
		if ( GetParty() != NULL )
		{
			CMsgCreateOrUpdateParty *pMsg = GetCreateOrUpdatePartyMsg();
			pMsg->mutable_search_criteria()->mutable_casual_criteria()->CopyFrom( casualHelper.GetCasualCriteria() );
		}

		m_msgLocalSearchCriteria.mutable_casual_criteria()->CopyFrom( casualHelper.GetCasualCriteria() );

		FireGameEventPartyUpdated();
	}
	else
	{
		// default to the Core maps
		SelectGroup( kMatchmakingType_Core, true );
	}
}

// Initialize steam client datagram lib if we haven't already
static bool CheckInitSteamDatagramClientLib()
{
	if ( !BUseSteamDatagram() )
		return false;

	if ( !steamapicontext || !steamapicontext->SteamHTTP() || !steamapicontext->SteamUtils() )
	{
		Assert( false );
		Warning( "Steam datagram not initialized - no Steam context\n" );
		return false;
	}

	static bool bInittedNetwork = false;
	if ( bInittedNetwork )
		return true;

	// Locate the first PLATFORM path
	char szAbsPlatform[MAX_PATH] = "";
	const char *pszConfigDir = "config";
	g_pFullFileSystem->GetSearchPath( "PLATFORM", false, szAbsPlatform, sizeof(szAbsPlatform) );

	char *semi = strchr( szAbsPlatform, ';' );
	if ( semi )
		*semi = '\0';

	char szAbsConfigDir[MAX_PATH];
	V_ComposeFileName( szAbsPlatform, pszConfigDir, szAbsConfigDir, sizeof(szAbsConfigDir) );
	SteamDatagramErrMsg errMsg;
	if ( !SteamDatagramClient_Init( szAbsConfigDir, k_ESteamDatagramPartner_Steam, (1<<k_ESteamDatagramPartner_Steam), errMsg ) )
	{
		Warning( "Failed to initialize steam datagram client. %s\n", errMsg );
		return false;
	}
	bInittedNetwork = true;

	return true;
}
bool CTFGCClientSystem::Init()
{
	// Get this guy created
	GetMMDashboard();

	// Convars may have initialized before us
	UpdateCustomPingTolerance();

	ListenForGameEvent( "client_disconnect" );
	ListenForGameEvent( "client_beginconnect" );
	ListenForGameEvent( "server_spawn" );

	// init steamdatagram system ASAP so we're more likely to have initial ping data to the clusters ready by the time
	// we ask for it
	CheckInitSteamDatagramClientLib();
	if ( SteamNetworkingUtils() )
		SteamNetworkingUtils()->CheckPingDataUpToDate( 0.0f );

	// Just loading the library starts initial pinging
	m_bPendingPingRefresh = true;

//	m_GameVersion = GAME_VERSION_CURRENT;

	// Default search criteria
	//m_msgLocalSearchCriteria.set_matchgroups( 1 );
	m_msgLocalSearchCriteria.set_matchmaking_mode( TF_Matchmaking_LADDER );

	m_bLocalSquadSurplus = false;
	return true;
}


void CTFGCClientSystem::PostInit()
{
	BaseClass::PostInit();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGCClientSystem::PreInitGC()
{
	if ( !m_bRegisteredSharedObjects )
	{
//		REG_SHARED_OBJECT_SUBCLASS( CTFHeroStandings );
//		REG_SHARED_OBJECT_SUBCLASS( CTFGameAccountClient );
		REG_SHARED_OBJECT_SUBCLASS( CTFParty );
		REG_SHARED_OBJECT_SUBCLASS( CTFGSLobby );
		REG_SHARED_OBJECT_SUBCLASS( CPartyInvite );
//		REG_SHARED_OBJECT_SUBCLASS( CTFBetaParticipation );
		REG_SHARED_OBJECT_SUBCLASS( CTFPartyInvite );
		REG_SHARED_OBJECT_SUBCLASS( CTFRatingData );

		m_bRegisteredSharedObjects = true;
	}

//	if ( m_flGetNewsTime == 0.0f )
//	{
//		m_flGetNewsTime = Plat_FloatTime() + 2.0f;
//	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGCClientSystem::PostInitGC()
{
	GCMatchmakingDebugSpew( 1, "CTFGCClientSystem::PostInitGC\n" );

	if ( steamapicontext && steamapicontext->SteamUser() )
	{
		GCMatchmakingDebugSpew( 1, "CTFGCClientSystem - adding listener\n" );

		CSteamID steamID = steamapicontext->SteamUser()->GetSteamID();
		GCClientSystem()->FindOrAddSOCache( steamID )->AddListener( this );
	}
	else
	{
		Warning( "CTFGCClientSystem - couldn't add listener because Steam wasn't ready\n" );
	}

// @FD We need this?
//	// Force a resend of our SO cache.
//	// This is only necessary because the Steam client doesn't detect a quick relaunch of the game, so the GC doesn't get a SessionStartPlaying call.
//	CProtoBufMsg<CMsgForceSOCacheResend> msg( k_EMsgForceSOCacheResend );
//	GCClientSystem()->BSendMessage( msg );

//	// Start hello job to ping the GC until we get a welcome
//	if ( !g_bClientReceivedGCWelcome )
//	{
//		CGCClientJobHello *pJob = new CGCClientJobHello( GCClientSystem()->GetGCClient() );
//		pJob->StartJob( NULL );
//	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFGCClientSystem::ReceivedClientWelcome( const CMsgClientWelcome &msg )
{
	BaseClass::ReceivedClientWelcome( msg );

	// Send a client init message in response to welcome
	GCSDK::CProtoBufMsg<CMsgTFClientInit> initMsg( k_EMsgGC_TFClientInit );
	initMsg.Body().set_client_version( engine->GetClientVersion() );
	char uilanguage[ 64 ];
	engine->GetUILanguage( uilanguage, sizeof( uilanguage ) );
	initMsg.Body().set_language( PchLanguageToELanguage( uilanguage ) );
	this->BSendMessage( initMsg );

	// Send a ping fix if we know it (e.g. we re-connected to the GC, or got a fix before the GC was ready)
	if ( BHavePingData() )
	{
		GCSDK::CProtoBufMsg<CMsgGCDataCenterPing_Update> pingmsg( k_EMsgGCDataCenterPing_Update );
		pingmsg.Body().CopyFrom( GetPingData() );

		m_bSentInitialPingFix = this->BSendMessage( pingmsg );
	}
	else
	{
		// PingThink will send it next fix
		m_bSentInitialPingFix = false;
	}
}

class CSendCreateOrUpdatePartyMsgJob;

static int s_nNumWizardStepChangesWaitingForReply = 0;

class CSendCreateOrUpdatePartyMsgJob : public GCSDK::CGCClientJob
{
public:

	CSendCreateOrUpdatePartyMsgJob()
	: GCSDK::CGCClientJob( GCClientSystem()->GetGCClient() )
	, msg( k_EMsgGCCreateOrUpdateParty )
	{
		msg.Body().set_client_version( engine->GetClientVersion() );
	}

	CProtoBufMsg<CMsgCreateOrUpdateParty> msg;
	CProtoBufMsg<CMsgCreateOrUpdatePartyReply> msgReply;

	static void UpdateWizardStepFromParty()
	{
		// Make sure we have a party and the data changed
		CTFParty *pParty = GTFGCClientSystem()->GetParty();
		if ( pParty != NULL && GTFGCClientSystem()->m_eLocalWizardStep != pParty->Obj().wizard_step() )
		{
			GTFGCClientSystem()->m_eLocalWizardStep = pParty->Obj().wizard_step();
			GTFGCClientSystem()->FireGameEventPartyUpdated();
		}
	}

	virtual bool BYieldingRunJob( void *pvStartParam )
	{
		Assert( s_nNumWizardStepChangesWaitingForReply >= 0 );
		if ( msg.Body().has_wizard_step() )
			++s_nNumWizardStepChangesWaitingForReply;

		bool bGotReply = BYldSendMessageAndGetReply( msg, 10, &msgReply, k_EMsgGCCreateOrUpdatePartyReply );

		if ( msg.Body().has_wizard_step() )
			--s_nNumWizardStepChangesWaitingForReply;
		Assert( s_nNumWizardStepChangesWaitingForReply >= 0 );

		if ( !bGotReply )
		{
			CTFParty *pParty = GTFGCClientSystem()->GetParty();
			if ( pParty )
			{
				pParty->SetOffline( true );
			}

			GTFGCClientSystem()->FireGameEventPartyUpdated();
			// !FIXME! Here we really should mark the GC Client as not
			// being connected to the GC
			
			Warning( "Timed out getting reply from GC to change party.\n" );
			return true;
		}

		// Any error message?
		EResult result = (EResult)msgReply.Body().result();
		const char *pszMsg = msgReply.Body().message().c_str();
		if ( *pszMsg != '\0' )
		{
			if ( result != k_EResultOK )
			{
				Warning( "%s\n", pszMsg );
			}
			else
			{
				Msg( "%s\n", pszMsg );
			}
		}

		// Check for error.
		switch ( result )
		{
			case k_EResultOK:
				break;
			case k_EResultInvalidProtocolVer:
				//if ( GTFGCClientSystem()->BIsPartyLeader() )
				//{
				//}
				GTFGCClientSystem()->EndMatchmaking();
				ShowMessageBox( "#TF_MM_NotCurrentVersionTitle", "#TF_MM_NotCurrentVersionMessage", "#GameUI_OK" );
				return true;
			default:
				Warning( "CreateOrUpdate returned error code %d\n", result );
				break;
		}

		// If no more messages pending that will change the wizard step, then
		// get in sync with the GC
		if ( s_nNumWizardStepChangesWaitingForReply <= 0 )
		{
			UpdateWizardStepFromParty();
			GTFGCClientSystem()->FireGameEventPartyUpdated();
			return true;
		}

		// Did we request a particular wizard step?
		if ( !msg.Body().has_wizard_step() )
			return true;

		// Do we have a party?
		CTFParty *pParty = GTFGCClientSystem()->GetParty();
		if ( pParty == NULL )
			return true;

		// We got a response.  Definitely not offline anymore
		pParty->SetOffline( false );

		// We should be the party leader
		Assert( GTFGCClientSystem()->BIsPartyLeader() );

		// Party should have a known wizard step
		Assert( pParty->Obj().has_wizard_step() );
		if ( !pParty->Obj().has_wizard_step() )
			return true;

		// If GC did not like our request, or we are starting or stopping searching,
		// the force us to get on the same page as the GC
		if (
			msg.Body().wizard_step() != pParty->Obj().wizard_step() // after processing message, we are not in requested step
			|| msg.Body().wizard_step() == TF_Matchmaking_WizardStep_SEARCHING // we requested to search
			|| pParty->Obj().wizard_step() == TF_Matchmaking_WizardStep_SEARCHING // we are now searching
			|| GTFGCClientSystem()->m_eLocalWizardStep == TF_Matchmaking_WizardStep_SEARCHING // we think we were searching previously
		)
		{
			UpdateWizardStepFromParty();
		}

		return true;
	}
};

CMsgCreateOrUpdateParty *CTFGCClientSystem::GetCreateOrUpdatePartyMsg()
{
	// TODO We should only send updates if something changes, some callers might just be copying same-values back in :-/

	if ( m_pPendingCreateOrUpdatePartyMsg == NULL )
	{
		m_pPendingCreateOrUpdatePartyMsg = new CSendCreateOrUpdatePartyMsgJob;
	}

	if ( m_flSendPartyUpdateMessageTime == FLT_MAX  )
	{
		if ( GetParty() && GetParty()->GetNumMembers() > 1 )
		{
			// If we're in a party, delay the sending of the message to queue up any rapid changes
			// that might occur from users clicking on criteria UI controls
			m_flSendPartyUpdateMessageTime = Plat_FloatTime() + 2.f;
		}
		else
		{
			m_flSendPartyUpdateMessageTime = 0.f;
		}
	}

	return &m_pPendingCreateOrUpdatePartyMsg->msg.Body();
}


//-----------------------------------------------------------------------------
void CTFGCClientSystem::LevelShutdownPostEntity()
{
	BaseClass::LevelShutdownPostEntity();
//	// clear caches, so the player will see his stats update after a game
//	if ( Dashboard() )
//	{
//		Dashboard()->ClearDashboardCaches();
//	}
}

//-----------------------------------------------------------------------------
void CTFGCClientSystem::LevelInitPreEntity()
{
	BaseClass::LevelInitPreEntity();
}

//-----------------------------------------------------------------------------
void CTFGCClientSystem::Shutdown()
{
	GCMatchmakingDebugSpew( 1, "CTFGCClientSystem::ShutdownGC\n" );

	if ( steamapicontext && steamapicontext->SteamUser() )
	{
		GCMatchmakingDebugSpew( 1, "CTFGCClientSystem - adding listener\n" );

		CSteamID steamID = steamapicontext->SteamUser()->GetSteamID();
		GCSDK::CGCClientSharedObjectCache	*pSOCache = GCClientSystem()->GetSOCache( steamID );
		Assert( pSOCache ); // we installed ourselves as a listener, right, so it shouldn't have deleted the cache
		if ( pSOCache )
		{
			pSOCache->RemoveListener( this );
		}
	}
	else
	{
		Warning( "CTFGCClientSystem - couldn't add listener because Steam wasn't ready\n" );
	}

	BaseClass::Shutdown();

	SteamDatagramClient_Kill();
}

//-----------------------------------------------------------------------------
void CTFGCClientSystem::FireGameEvent( IGameEvent *event )
{
	const char *pEventName = event->GetName();
	// Disconnected from gameserver
	if ( !Q_stricmp( pEventName, "client_disconnect" ) )
	{
		m_steamIDCurrentServer.Clear();

		// Do not send end match making if we see the mvm end message
		if ( !Q_stricmp( event->GetString( "message", "" ), "#TF_PVE_Disconnect" ) )
			return;

		// Don't bail if GC has told us to expect to be put into a new party
		if ( m_nPendingAutoJoinPartyID != 0 )
			return;

		m_eConnectState = eConnectState_Disconnected; // clear variable first to avoid recursion

		// Ladder games
		//if ( !Q_stricmp( event->GetString( "message", "" ), "#TF_Competitive_Disconnect" ) ) // FIXME only disconnect if we were previously connected(ing), this fires spuriously from the main menu
		//{
		//	engine->ClientCmd_Unrestricted( "OpenMatchmakingLobby ladder" );
		//	return;
		//}

		CTFParty *pParty = GetParty();

		// Return to party screen upon disconnect
		if ( m_bUserWantsToBeInMatchmaking && ( ( pParty && pParty->GetNumMembers() > 1 ) || m_eLocalWizardStep == TF_Matchmaking_WizardStep_SEARCHING ) )
		{
			switch( GTFGCClientSystem()->GetSearchMode() )
			{
			case TF_Matchmaking_LADDER:
				engine->ClientCmd_Unrestricted( "OpenMatchmakingLobby ladder" );
				break;

			case TF_Matchmaking_CASUAL:
				engine->ClientCmd_Unrestricted( "OpenMatchmakingLobby casual" );
				break;
			case TF_Matchmaking_MVM:
				engine->ClientCmd_Unrestricted( "OpenMatchmakingLobby mvm" );
				break;
			default:
				engine->ClientCmd_Unrestricted( "OpenMatchmakingLobby" );
				Assert( !"Unhandled enum value" );
				break;
			};
		}

		return;
	}

	// Started attempting connection to gameserver
	if ( !Q_stricmp( pEventName, "client_beginconnect" ) )
	{
		Assert( IsConnectStateDisconnected() );

		// TODO does the retry command set this source? It should go through ::ConnectToServer
		const char *pszSource = event->GetString( "source", "" );
		if ( FStrEq( pszSource, "matchmaking" ) )
		{
			// Assume we're doing the right thing until we hit server_spawn and figure otherwise.
			m_steamIDCurrentServer = m_steamIDGCAssignedMatch;
			m_eConnectState = eConnectState_ConnectingToMatchmade;
		}
		else
		{
			if ( !BAllowMatchMakingInGame() )
			{
				EndMatchmaking();
			}

			m_eConnectState = eConnectState_NonmatchmadeServer;
		}
		return;
	}

	// Successfully connected to a gameserver. For MM purposes, we stay in state connecting until server spawn as that
	// ensures there's no intermediate "loading into some server but we're not sure of its steamid yet" state.
	if ( !Q_strcmp( pEventName, "server_spawn" ) )
	{
		GCMatchmakingDebugSpew( 4, "Client reached server_spawn.\n" );
		switch ( m_eConnectState )
		{
			default:
				AssertMsg1( false, "Unknown connect state %d", m_eConnectState );
			// These two can happen when doing weird things with timedemo or listen servers
			case eConnectState_Disconnected:
				m_eConnectState = eConnectState_NonmatchmadeServer;
				GCMatchmakingDebugSpew( 4, "Client connected to non-matchmade.\n" );
				break;

			case eConnectState_ConnectingToMatchmade:
				m_eConnectState = eConnectState_ConnectedToMatchmade;
				GCMatchmakingDebugSpew( 4, "Client connected to matchmade.\n" );
				break;

			case eConnectState_ConnectedToMatchmade:
				break;

			case eConnectState_NonmatchmadeServer:
				break;
		}
		m_steamIDCurrentServer.Clear();
		if ( steamapicontext && steamapicontext->SteamUser() && steamapicontext->SteamUtils() )
		{
			m_steamIDCurrentServer.SetFromString( event->GetString( "steamid", "" ), GetUniverse() );
			GCMatchmakingDebugSpew( 4, "Recognizing MM server id %s\n", m_steamIDCurrentServer.Render() );
		}

		if ( m_eConnectState == eConnectState_ConnectedToMatchmade && !m_steamIDCurrentServer.IsValid() )
		{
			Warning( "Connected to MM server but no GS steamid is set.\n" );
		}

		return;
	}

}

//-----------------------------------------------------------------------------
void CTFGCClientSystem::InvalidatePingData()
{
	// Invalidate current refresh
	TFPingMsg("Forcing ping refresh\n" );
	m_bPendingPingRefresh = true;

	// Wipe all cached data.
	m_rtLastPingFix = 0; // 0 means never. Or time traveler. 50/50.
	m_msgCachedPingUpdate = CMsgGCDataCenterPing_Update();

	if ( BUseSteamDatagram() && SteamNetworkingUtils() )
	{
		SteamNetworkingUtils()->CheckPingDataUpToDate( 0.0f );
	}
}

//-----------------------------------------------------------------------------
void CTFGCClientSystem::PingThink()
{
	ISteamNetworkingUtils *pUtils = SteamNetworkingUtils();
	if ( !pUtils && BUseSteamDatagram() )
	{
		Assert( pUtils );
		return;
	}

	if ( !m_bPendingPingRefresh )
	{
		// No refresh in progress, start one if necessary
		RTime32 rtRefreshInterval = (RTime32)Clamp( tf_datacenter_ping_interval.GetInt(), 0, INT32_MAX );
		RTime32 rtLastRefreshAge = CRTime::RTime32TimeCur() - m_rtLastPingFix;

		if ( rtLastRefreshAge <= rtRefreshInterval )
			{ return; }

		// Start a refresh
		m_bPendingPingRefresh = true;

		// Non-steam datagram will just succeed next heartbeat
		if ( BUseSteamDatagram() )
			{ pUtils->CheckPingDataUpToDate(0.0f); }

		return;
	}

	//
	// Refresh pending, speculatively start building an update and bail out if we find missing data
	//

	CMsgGCDataCenterPing_Update newPingUpdate;

	// Check if our connection status is good enough for ping measurement
	// Without steamdatagram, we'll just always succeed immediately with empty data (plus overrides below)
	if ( BUseSteamDatagram() )
	{
		// Not ready yet?
		if ( pUtils->IsPingMeasurementInProgress() )
			return;

		// Get complete list of points of presence
		CUtlVector<SteamNetworkingPOPID> vecPoPs;
		vecPoPs.SetCount( pUtils->GetPOPCount() );
		vecPoPs.SetCountNonDestructively( pUtils->GetPOPList( vecPoPs.Base(), vecPoPs.Count() ) );

		// Waiting on a ping refresh to complete. Check if we have every datacenter and cache off if so.
		//
		// NOTE that we don't use SDR for actual-routing yet, so we are purposefully using the *router* ping values as
		//      estimates for that DC -- since we will talk to the DC directly and not via the relay.
		for ( SteamNetworkingPOPID id: vecPoPs )
		{
			char szCode[ 8 ];
			GetSteamNetworkingLocationPOPStringFromID( id, szCode );

			CMsgGCDataCenterPing_Update_PingEntry *pMsgPingEntry = newPingUpdate.add_pingdata();
			pMsgPingEntry->set_name( szCode );
			int nPing = pUtils->GetDirectPingToPOP( id );
			if ( nPing >= 0 )
			{
				pMsgPingEntry->set_ping( nPing );
			}
			else
			{
				nPing = pUtils->GetPingToDataCenter( id, nullptr );
				if ( nPing >= 0 )
				{
					pMsgPingEntry->set_ping( nPing );
					pMsgPingEntry->set_ping_status( CMsgGCDataCenterPing_Update_Status_FallbackToDCPing );
				}
			}

			if ( !pMsgPingEntry->has_ping() )
			{
				pMsgPingEntry->set_ping_status( CMsgGCDataCenterPing_Update_Status_Unreachable );
			}
		}
	}
	else
	{
		// Otherwise we're fine
		TFPingMsg( "Not using steam datagram, proceeding with empty cluster ping data\n" );
	}

	// If we're in beta/dev, add the magic "beta" cluster. See tf_datacenter_info on GC.
	EUniverse eUniverse = GetUniverse();
	if ( eUniverse == k_EUniverseBeta || eUniverse == k_EUniverseDev )
	{
		CMsgGCDataCenterPing_Update_PingEntry newEntry;
		newEntry.set_name( "beta" );
		newEntry.set_ping( 5 );
		newEntry.set_ping_status( CMsgGCDataCenterPing_Update_Status_Normal );
		ApplyPingToMsg( newPingUpdate, newEntry );
	}

#ifdef TF_GC_PING_DEBUG
	// Apply overrides
	for ( int j = 0; j < m_msgPingOverrides.pingdata_size(); ++j )
	{
		ApplyPingToMsg( newPingUpdate, m_msgPingOverrides.pingdata(j) );
	}
#endif // def TF_GC_PING_DEBUG

	// We made it through all routers without bailing, can claim to have ping data now
	if ( BConnectedtoGC() )
	{
		GCSDK::CProtoBufMsg<CMsgGCDataCenterPing_Update> msg( k_EMsgGCDataCenterPing_Update );
		msg.Body().CopyFrom( newPingUpdate );

		if ( this->BSendMessage( msg ) )
		{
			TFPingDbg( "Initial ping fix sent\n" );
			m_bSentInitialPingFix = true;
		}
	}

	m_bPendingPingRefresh = false;
	m_rtLastPingFix = CRTime::RTime32TimeCur();
	m_msgCachedPingUpdate = newPingUpdate;

	IGameEvent *event = gameeventmanager->CreateEvent( "ping_updated" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}

	if ( BPingDebug() )
	{
		DumpPing();
	}
}

#ifdef TF_GC_PING_DEBUG
//-----------------------------------------------------------------------------
void CTFGCClientSystem::SetPingOverride( const char *pszDataCenter, uint32 nPing, CMsgGCDataCenterPing_Update_Status eStatus )
{
	CMsgGCDataCenterPing_Update_PingEntry newEntry;
	newEntry.set_name( pszDataCenter );
	newEntry.set_ping( nPing );
	newEntry.set_ping_status( eStatus );
	ApplyPingToMsg( m_msgPingOverrides, newEntry );

	InvalidatePingData();
}

//-----------------------------------------------------------------------------
void CTFGCClientSystem::ClearPingOverrides()
{
	m_msgPingOverrides.clear_pingdata();

	InvalidatePingData();
}
#endif // def TF_GC_PING_DEBUG

//-----------------------------------------------------------------------------
void CTFGCClientSystem::Update( float frametime )
{
	BaseClass::Update( frametime );

//	if ( m_flGetNewsTime != 0.0f && Plat_FloatTime() > m_flGetNewsTime && steamapicontext && steamapicontext->SteamUtils() )
//	{
//		m_flGetNewsTime = 0.0f;
//
//		// get the latest news
//		CGCClientJobGetNews *pJob = new CGCClientJobGetNews( GCClientSystem()->GetGCClient(), (int) engine->GetAppID() );
//		pJob->StartJob( NULL );
//	}

	PingThink();

	// Check if it's time to send a party update message
	if ( Plat_FloatTime() > m_flSendPartyUpdateMessageTime )
	{
		m_flSendPartyUpdateMessageTime = FLT_MAX;

		Assert( m_pPendingCreateOrUpdatePartyMsg );
		if ( m_pPendingCreateOrUpdatePartyMsg )
		{
			// Send the message
			m_pPendingCreateOrUpdatePartyMsg->StartJob( NULL );
			m_pPendingCreateOrUpdatePartyMsg = NULL;
		}
	}


	CTFParty *pParty = GetParty();
	CTFGSLobby *pLobby = GetLobby();

	// Are we in a active lobby?
	bool bInLiveMatch = BConnectedToMatchServer( true );
	// If we do, are we actively connected to said match?
	bool bHaveLiveMatch = BHaveLiveMatch();
	bool bNewServerAssignment = m_bServerAssignmentChanged;
	m_bServerAssignmentChanged = false;

	Assert( !bInLiveMatch || m_steamIDCurrentServer.IsValid() );


	if ( bInLiveMatch )
	{
		// We cannot assume cannot assume the gc will tell of us the match ending -- the GC connection is fallible, and
		// the gameserver is authoritative once we're assigned (as long as the GC doesn't revoke said assignment, see
		// SOChanged)
		CTFGameRules *pTFGameRules = TFGameRules();
		// If we're not loaded enough to look at gamerules, assume the match is live until we reach that state.
		//
		// - Because source engine, we can get a stale TFGameRules from our *last* game *after* starting a new
		//   connection.  Only even think about asking once our connect state hits connected (keyed to server_spawn)
		if ( m_eConnectState == eConnectState_ConnectedToMatchmade &&
		     engine->IsInGame() &&
		     pTFGameRules && pTFGameRules->RecievedBaseline() && pTFGameRules->IsManagedMatchEnded() )
		{
			// We no longer consider this our assigned match.  Only the GC can change the GCAssignedMatch, this bool is
			// our "but we reject this".  SOChanged will clear it if a new assignment overrides things.
			GCMatchmakingDebugSpew( 1, "GS marked assigned match as ended\n" );
			m_bAssignedMatchEnded = true;
		}
	}

	if ( !bHaveLiveMatch )
	{
		// Are we waiting to activate the lobby UI until a certain party appears?
		if ( m_nPendingAutoJoinPartyID != 0 && pParty != NULL && pParty->GetGroupID() == m_nPendingAutoJoinPartyID )
		{
			Msg( "New party was instanced that GC told us to expect.  Entering matchmaking lobby UI\n" );
			engine->ClientCmd_Unrestricted( "OpenMatchmakingLobby ladder" );
			BeginMatchmaking( pParty->GetMatchmakingMode() );
		}

		/// XXX(JohnS): Right now invites just wont work if you're in a match, needs better flow.

		// Do we have a pending invite we need to process?
		if ( m_eAcceptInviteStep == eAcceptInviteStep_ReadyToJoinSteamLobby  )
		{

			// Wait for everything else to go away, if we have anything
			if ( !IsConnectStateDisconnected() )
			{
				//Msg( "Disconnecting from current server to accept invite\n" );
				engine->ClientCmd_Unrestricted( "disconnect" );
			}
			else if ( m_bUserWantsToBeInMatchmaking )
			{
				EndMatchmaking();
			}
			else if ( pLobby == NULL && GetParty() == NULL )
			{
				Assert( m_steamIDLobbyInviteAccepted.IsValid() );
				m_eAcceptInviteStep = eAcceptInviteStep_JoinSteamLobby;

				// OK, start joining the lobby.
				Msg( "Joining lobby %s\n", m_steamIDLobbyInviteAccepted.Render() );
				steamapicontext->SteamMatchmaking()->JoinLobby( m_steamIDLobbyInviteAccepted );
				m_steamIDLobbyInviteAccepted = CSteamID();
			}
		}
	}


	if ( bHaveLiveMatch )
	{
		if ( m_eConnectState != eConnectState_Disconnected && engine->IsInGame() )
		{
			// The dashboard will handle this automatically
		}
		else if ( m_bUserWantsToBeInMatchmaking && bNewServerAssignment )
		{
			// Use the autojoin if in the MM flow and this is a fresh match
			m_AutoJoinHandler.MatchFound();
		}
		else
		{
			// Use the prompt 
			m_PromptJoinHandler.MatchFound();
		}
	}

	FOR_EACH_VEC_BACK( m_vecDelayedLocalPlayerSOListenersToAdd, i )
	{
		SubscribeToLocalPlayerSOCache( m_vecDelayedLocalPlayerSOListenersToAdd[ i ] );
		m_vecDelayedLocalPlayerSOListenersToAdd.Remove( i );
	}
}

void CTFGCClientSystem::SOCacheSubscribed( const CSteamID & steamIDOwner, GCSDK::ESOCacheEvent eEvent )
{
	if ( steamIDOwner == ClientSteamContext().GetLocalPlayerSteamID() )
	{
		// Assert( m_pSOCache == NULL ); // we *can* get two SOCacheSubscribed calls in a row.
		m_pSOCache = GCClientSystem()->GetSOCache( steamIDOwner );
		Assert( m_pSOCache != NULL );

		if ( gameeventmanager )
		{

			// force a party/lobby update whenever our SO cache arrives
			FireGameEventPartyUpdated();
			FireGameEventLobbyUpdated();
		}
	}
}

void CTFGCClientSystem::FireGameEventPartyUpdated()
{
	IGameEvent *event = gameeventmanager->CreateEvent( "party_updated" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}
}

void CTFGCClientSystem::FireGameEventLobbyUpdated()
{
	IGameEvent *event2 = gameeventmanager->CreateEvent( "lobby_updated" );
	if ( event2 )
	{
		gameeventmanager->FireEventClientSide( event2 );
	}
}

void CTFGCClientSystem::SOCreated( const CSteamID & steamIDOwner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent ) { SOChanged( pObject, SOChanged_Create, eEvent ); }
void CTFGCClientSystem::SOUpdated( const CSteamID & steamIDOwner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent ) { SOChanged( pObject, SOChanged_Update, eEvent ); }
void CTFGCClientSystem::SODestroyed( const CSteamID & steamIDOwner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent ) { SOChanged( pObject, SOChanged_Destroy, eEvent ); }

void CTFGCClientSystem::SOChanged( const GCSDK::CSharedObject *pObject, SOChangeType_t changeType, GCSDK::ESOCacheEvent eEvent )
{
	// Broadcasts
	if ( pObject->GetTypeID() == CTFParty::k_nTypeID )
	{
		#if GCMATCHMAKING_DEBUG_LEVEL > 0
			switch ( changeType )
			{
				case SOChanged_Create: GCMatchmakingDebugSpew( 1, "Party created\n"); break;
				case SOChanged_Update: GCMatchmakingDebugSpew( 2, "Party updated\n"); break;
				case SOChanged_Destroy: GCMatchmakingDebugSpew( 1, "Party destroyed\n"); break;
				default: AssertMsg1( false, "Bogus change type %d", changeType );
			}
		#endif

		CTFParty *pParty = GetParty();
		if ( changeType == SOChanged_Destroy )
		{
			Assert( pParty == NULL );
			FireGameEventPartyUpdated();
		}
		else if ( pParty != NULL ) // FIXME we're restarting the game after a crash, rejoining a match, and have no wizard step (or other BeginMatchmaking()) setup, so when we return to UI it's state is running but fucked
		{
			if ( pParty->BOffline() )
			{
				pParty->SetOffline( false );
				// The user says they dont want to be in matchmaking, but the party coming in says that its
				// searching or hanging out in the UI, which is not what we're doing.  This can happen in
				// the following circumstances:
				//		1) With the GC up, start searching for a match
				//		2) Crash the GC
				//		3) Go back to the main menu
				//		4) Reboot the GC
				if ( pParty->GetState() == CSOTFParty_State_UI || pParty->GetState() == CSOTFParty_State_FINDING_MATCH )
				{
					if ( !m_bUserWantsToBeInMatchmaking )
					{
						Msg( "Party was updated/created, but our party is marked offline, we don't want to be matchmaking, and the party is not in a match.  Ending matchmaking\n" );
						EndMatchmaking();
					}
					else 
					{
						Msg( "Party was updated/created, and our party is marked offline, and the party is not in a match.  Sending update to GC with our predicted changes\n" );
						CMsgCreateOrUpdateParty *pMsg = GetCreateOrUpdatePartyMsg();
						pMsg->set_wizard_step( m_eLocalWizardStep );
					}
				}
			}
			else
			{
				bool bShouldGoToMMUI = false;

				// We'll hit this when start the game back up after a crash
				if ( !m_bUserWantsToBeInMatchmaking && ( eEvent == eSOCacheEvent_Subscribed || eEvent == eSOCacheEvent_ListenerAdded ) && m_eAcceptInviteStep != eAcceptInviteStep_JoinParty )
				{
					switch( pParty->GetState() )
					{
						case CSOTFParty_State_UI:
						case CSOTFParty_State_FINDING_MATCH:
							// They backed out of the MM UI somehow, and are getting party updates.  We want out.
							if ( pParty->GetNumMembers() <= 1 )
							{
									
								Msg( "Creating a party when we don't want to be in matchmaking, and we're the only ones in it.  Possibly and old party from an old session. Ending matchmaking.\n" );
								EndMatchmaking();
							}
							else
							{
								Msg( "Creating a party when we don't want to be in matchmaking, and it has other players in it.  Possibly and old party from an old session. Going to MM UI.\n" );
								bShouldGoToMMUI = true;
							}
							break;

						case CSOTFParty_State_IN_MATCH:
						case CSOTFParty_State_AWAITING_RESERVATION_CONFIRMATION:
							// We don't have a match, but we're still in a party.  Leave matchmaking.
							// TODO: Once the lobby panels are no longer a nightmare, let this happen.
							//		 We dont really want to destroy their party, but it's too much of
							//		 a hassle to support now.
							if ( !BHaveLiveMatch() )
							{
								Msg( "Creating a party when we don't want to be in matchmaking, and it has other players in it, and it's live.  Leaving matchmaking\n" );
								EndMatchmaking();
							}
							break;
						default:
							AssertMsg1( false, "Unhandled party state %d", pParty->GetState() );
					}
				}


				if ( bShouldGoToMMUI )
				{
					if ( IsLadderGroup( pParty->GetMatchGroup() ) )
					{
						engine->ClientCmd_Unrestricted( "OpenMatchmakingLobby ladder" );
					}
					else if ( IsCasualGroup( pParty->GetMatchGroup() ) )
					{
						engine->ClientCmd_Unrestricted( "OpenMatchmakingLobby casual" );
					}
					else if ( IsMvMMatchGroup( pParty->GetMatchGroup() ) )
					{
						engine->ClientCmd_Unrestricted( "OpenMatchmakingLobby mvm" );
					}

					BeginMatchmaking( pParty->GetMatchmakingMode() );
				}

				// Check if a party was instanced on us as a process of accepting an invite
				if ( m_eAcceptInviteStep == eAcceptInviteStep_JoinParty )
				{
					m_eAcceptInviteStep = eAcceptInviteStep_None;
					Msg( "Party was instanced as a result of accepting invite.  Entering matchmaking lobby UI\n" );
					engine->ClientCmd_Unrestricted( "OpenMatchmakingLobby invited" );
					BeginMatchmaking( pParty->GetMatchmakingMode() );
				}

				//m_msgLocalSearchCriteria.set_key( pParty->Obj().search_key() );
				m_msgLocalSearchCriteria.set_late_join_ok( pParty->Obj().search_late_join_ok() );
				//m_msgLocalSearchCriteria.set_matchgroups( pParty->Obj().matchgroups() );
				m_msgLocalSearchCriteria.set_matchmaking_mode( pParty->GetMatchmakingMode() );
				m_msgLocalSearchCriteria.set_quickplay_game_type( pParty->GetSearchQuickplayGameType() );
				m_msgLocalSearchCriteria.clear_mvm_missions();
	#ifdef USE_MVM_TOUR
				m_msgLocalSearchCriteria.clear_mvm_mannup_tour();
	#endif // USE_MVM_TOUR
				if ( pParty->GetMatchmakingMode() == TF_Matchmaking_MVM )
				{
					m_msgLocalSearchCriteria.mutable_mvm_missions()->MergeFrom( pParty->Obj().search_mvm_missions() );
	#ifdef USE_MVM_TOUR
					if ( pParty->GetSearchPlayForBraggingRights() )
						m_msgLocalSearchCriteria.set_mvm_mannup_tour( pParty->GetSearchMannUpTourName() );
	#endif // USE_MVM_TOUR
				}
				else if ( pParty->GetMatchmakingMode() == TF_Matchmaking_LADDER )
				{
					m_msgLocalSearchCriteria.set_ladder_game_type( pParty->Obj().search_ladder_game_type() );
				}
				else if ( pParty->GetMatchmakingMode() == TF_Matchmaking_CASUAL )
				{
					m_msgLocalSearchCriteria.mutable_casual_criteria()->CopyFrom( pParty->Obj().search_casual() );
				}
				m_bLocalSquadSurplus = false;
				int iLocalMemberIdx = pParty->GetMemberIndexBySteamID( steamapicontext->SteamUser()->GetSteamID() );
				if ( iLocalMemberIdx >= 0 )
				{
					m_bLocalSquadSurplus = pParty->Obj().members( iLocalMemberIdx ).squad_surplus();
				}
				Assert( pParty->Obj().has_wizard_step() );
				if ( pParty->Obj().has_wizard_step() )
				{
					// If entering or leaving the searching state, clear searching stats
					if ( m_eLocalWizardStep != TF_Matchmaking_WizardStep_SEARCHING || pParty->Obj().wizard_step() != TF_Matchmaking_WizardStep_SEARCHING )
					{
						m_msgMatchmakingProgress.Clear();
					}

					// Get on the same page as the GC.  But if we have a pending request to change the current step,
					// then wait for that to finish.  Otherwise the current step could flicker back and forth.
					if ( s_nNumWizardStepChangesWaitingForReply == 0 )
					{
						m_eLocalWizardStep = pParty->Obj().wizard_step();
					}
				}
			}
		}
		else
		{
			Assert( pParty != NULL );
		}

		FireGameEventPartyUpdated();

		CheckAssociatePartyAndSteamLobby();

		// Check if we're ready to active the Steam overlay to invite a user
		CheckReadyToActivateInvite();
	}
	else if ( pObject->GetTypeID() == CTFGSLobby::k_nTypeID )
	{
		#if GCMATCHMAKING_DEBUG_LEVEL > 0
			switch ( changeType )
			{
				case SOChanged_Create: GCMatchmakingDebugSpew( 1, "Lobby created\n"); break;
				case SOChanged_Update: GCMatchmakingDebugSpew( 2, "Lobby updated\n"); break;
				case SOChanged_Destroy: GCMatchmakingDebugSpew( 1, "Lobby destroyed\n"); break;
				default: AssertMsg1( false, "Bogus change type %d", changeType );
			}
		#endif

		CTFGSLobby *pLobby = GetLobby();

		CSteamID currentServer;
		if ( pLobby && pLobby->GetState() == CSOTFGameServerLobby_State_RUN )
		{
			currentServer = pLobby->GetServerID();
		}

		// We cannot take the Lobby being deleted as a server assignment change, since the GC could crash and fail to
		// recover lobbies.  Or, since lobbies are lazy-loaded from memcache, it may come back up and lazily put us back
		// into our lobby.
		//
		// However, since we have no way to ask the gameserver without being connected to it, we'll treat
		// lobby-destroyed as canon only if we're not connected to the match.  Otherwise, we'll assume the gameserver's
		// m_bAssignedMatchEnded flag is the authority.
		//
		// Thus, this line reads:
		// - If we got a *new and differing* lobby, propagate it to the m_*Assigned* convars.
		// - If our lobby *went away*, clear these convars IF:
		//   - We're not connected to a match server
		//   - OR the gameserver concurs that the match is over
		bool bLobbyChanged = ( currentServer != m_steamIDGCAssignedMatch ) ||
			( pLobby && pLobby->GetMatchID() != m_uAssignedMatchID ) ;
		if ( bLobbyChanged && ( !BConnectedToMatchServer( true ) || pLobby || m_bAssignedMatchEnded ) )
		{
			Msg( "Lobby received with a differing steamID. Lobby's: %s CurrentlyAssigned: %s ConnectedToMatchServer: %d HasLobby: %d AssignedMatchEnded: %d\n"
			   , currentServer.Render()
			   , m_steamIDGCAssignedMatch.Render()
			   , BConnectedToMatchServer( true )
			   , pLobby != NULL
			   , m_bAssignedMatchEnded );

			m_bServerAssignmentChanged = true;
			m_steamIDGCAssignedMatch = currentServer;
			m_bAssignedMatchEnded = pLobby ? false : m_bAssignedMatchEnded;	// If the lobby is still here, we know the match isn't over.
			m_uAssignedMatchID = pLobby ? pLobby->GetMatchID() : 0;
			m_eAssignedMatchGroup = pLobby ? pLobby->GetMatchGroup() : k_nMatchGroup_Invalid;
			// Store match connection history for generic server browser/connection code to reason about which of our
			// connections was match related.
			netadr_t connectAdr; // but y is string
			if ( pLobby && connectAdr.SetFromString( pLobby->GetConnect() ) )
			{
				m_vecMatchServerHistory.AddToTail( connectAdr );
			}
		}

		//CTFParty *pParty = GetParty();
		

		// Lobby is gone, but we're connected to our match server still.
		/*if ( pParty && !pLobby && BConnectedToMatchServer( false ) )
		{
			const IMatchGroupDescription* pDesc = GetMatchGroupDescription( pParty->GetMatchGroup() );

			if ( pDesc && pDesc->BShouldAutomaticallyRequeueOnMatchEnd() )
			{
				SendCreateOrUpdatePartyMsg( TF_Matchmaking_WizardStep_SEARCHING );
			}
		}*/

		FireGameEventLobbyUpdated();
	}
	// Notifications. Sync/add/delete with what's in our notification drawer
	else if ( pObject->GetTypeID() == CTFNotification::k_nTypeID )
	{
		const CTFNotification* pSONotification = ( const CTFNotification* )( pObject );
		Msg( "Notification %llu %s: \"%s\"\n",
		     pSONotification->Obj().notification_id(),
		     changeType == SOChanged_Create ? "created" : changeType == SOChanged_Destroy ? "destroyed" : "updated",
		     pSONotification->Obj().notification_string().c_str() );

		// Update existing notification if found
		bool bFound = false;
		for ( int i = NotificationQueue_GetNumNotifications() - 1; i >= 0; --i )
		{
			CClientNotification *pNotif = dynamic_cast<CClientNotification *>(NotificationQueue_GetByIndex( i ));
			if ( pNotif && pNotif->NotificationID() == pSONotification->Obj().notification_id() )
			{
				Msg( "Notification %llu already displayed, updating\n",
				     pSONotification->Obj().notification_id() );
				bFound = true;
				if ( changeType == SOChanged_Destroy )
				{
					NotificationQueue_Remove( pNotif );
				}
				else
				{
					pNotif->Update( pSONotification );
				}
			}
		}

		// Add them to our notifications drawer if not
		if ( !bFound && changeType != SOChanged_Destroy )
		{
			Msg( "New notification %llu arrived: \"%s\"\n",
			     pSONotification->Obj().notification_id(),
			     pSONotification->Obj().notification_string().c_str() );
			CClientNotification *pClientNotification = new CClientNotification();
			pClientNotification->Update( pSONotification );
			NotificationQueue_Add( pClientNotification );
		}
	}

//	// After here we only care about create or change events
//	if ( changeType == SOChanged_Destroy )
//	{
//		return;
//	}
//
//	if( pObject->GetTypeID() == CTFGameAccountClient::k_nTypeID )
//	{
//		CTFGameAccountClient *pAccount = (CTFGameAccountClient *)pObject;
//		m_unWinCount = pAccount->GetWins();
//		m_unLossCount = pAccount->GetLosses();
//	}
//	else if ( pObject->GetTypeID() == CTFHeroStandings::k_nTypeID )
//	{
//		CTFHeroStandings *pHeroStandings = (CTFHeroStandings *)pObject;
//		// see if we have an entry for this already
//		int nFoundIndex = -1;
//		for ( int i = 0; i < m_aHeroRecords.Count(); i++ )
//		{
//			if ( m_aHeroRecords[i].m_unHeroID == pHeroStandings->GetHeroID() )
//			{
//				nFoundIndex = i;
//				break;
//			}
//		}
//		if ( nFoundIndex == -1 )
//		{
//			GCHeroRecord_t newHeroStanding;
//			nFoundIndex = m_aHeroRecords.InsertNoSort( newHeroStanding );
//		}
//		
//		m_aHeroRecords[ nFoundIndex ].m_unHeroID = pHeroStandings->GetHeroID();
//		m_aHeroRecords[ nFoundIndex ].m_unWinCount = pHeroStandings->GetWins();
//		m_aHeroRecords[ nFoundIndex ].m_unLossCount = pHeroStandings->GetLosses();
//
//		m_aHeroRecords.RedoSort();
//	}
}

//KeyValues *CTFGCClientSystem::GetNewsStory( uint64 unNewsID )
//{
//	if ( !m_pNewsKeys )
//		return NULL;
//
//	for ( KeyValues *pItems = m_pNewsKeys->GetFirstSubKey(); pItems; pItems = pItems->GetNextKey() )
//	{
//		if ( !Q_stricmp( pItems->GetName(), "newsitems" ) )
//		{
//			for ( KeyValues *pItem = pItems->GetFirstSubKey(); pItem; pItem = pItem->GetNextKey() )
//			{
//				if ( !Q_stricmp( pItem->GetName(), "newsitem" ) )
//				{
//					for ( KeyValues *pStory = pItem->GetFirstSubKey(); pStory; pStory = pStory->GetNextKey() )
//					{
//						if ( pStory->GetUint64( "gid" ) == unNewsID )
//						{
//							return pStory;
//						}
//					}
//				}
//			}
//		}
//	}
//	return NULL;
//}
//
//KeyValues *CTFGCClientSystem::GetNewsStoryByIndex( int nNewsIndex )
//{
//	if ( !m_pNewsKeys )
//		return NULL;
//
//	int nCount = 0;
//	for ( KeyValues *pItems = m_pNewsKeys->GetFirstSubKey(); pItems; pItems = pItems->GetNextKey() )
//	{
//		if ( !Q_stricmp( pItems->GetName(), "newsitems" ) )
//		{
//			for ( KeyValues *pItem = pItems->GetFirstSubKey(); pItem; pItem = pItem->GetNextKey() )
//			{
//				if ( !Q_stricmp( pItem->GetName(), "newsitem" ) )
//				{
//					for ( KeyValues *pStory = pItem->GetFirstSubKey(); pStory; pStory = pStory->GetNextKey() )
//					{
//						if ( nCount >= nNewsIndex )
//						{
//							return pStory;
//						}
//						nCount++;
//					}
//				}
//			}
//		}
//	}
//	return NULL;
//}

void CTFGCClientSystem::DumpInvites()
{
	if ( !m_pSOCache )
	{
		Msg( "No SO cache.\n" );
		return;
	}

	CSharedObjectTypeCache *pTypeCache = m_pSOCache->FindBaseTypeCache( CTFPartyInvite::k_nTypeID );
	if ( !pTypeCache )
	{
		Msg( "No invites typecache.\n" );
		return;
	}

	Msg( "Listing invites in typecache:\n" );
	for ( uint32 i = 0; i < pTypeCache->GetCount(); i++ )
	{
		CTFPartyInvite *pInvite = static_cast<CTFPartyInvite*>( pTypeCache->GetObject( i ) );
		Msg( "[%u] PartyID = %llu Sender = %s %s\n", i, pInvite->GetGroupID(), pInvite->GetSenderID().Render(), pInvite->GetSenderName() );
	}
}

void CTFGCClientSystem::DumpPing()
{
	//	RTime32 m_rtLastPingFix;
	//  bool    m_bPendingPingRefresh;
	//  bool    m_bSentInitialPingFix;
	if ( !m_rtLastPingFix )
	{
		TFPingMsg( "No current ping data. Pending refresh: %i, Sent initial fix: %i\n",
		           m_bPendingPingRefresh, m_bSentInitialPingFix );
		return;
	}
	char szLastFix[ k_RTimeRenderBufferSize ] = { 0 };
	CRTime::Render( m_rtLastPingFix, szLastFix );

	TFPingMsg( "Ping data is current as of %s. Pending refresh: %i, Sent initial fix: %i\n",
	           szLastFix, m_bPendingPingRefresh, m_bSentInitialPingFix );
	for ( int i = 0; i < m_msgCachedPingUpdate.pingdata_size(); i++ )
	{
		Msg( "  %5s: %dms, status %i\n",
		     m_msgCachedPingUpdate.pingdata( i ).name().c_str(),
		     m_msgCachedPingUpdate.pingdata( i ).ping(),
			 m_msgCachedPingUpdate.pingdata( i ).ping_status() );
	}
}

//CTFGameAccountClient* CTFGCClientSystem::GetGameAccountClient()
//{	
//	if ( !m_pSOCache )
//		return NULL;
//
//	CSharedObjectTypeCache *pTypeCache = m_pSOCache->GetBaseTypeCache( CTFGameAccountClient::k_nTypeID );
//	if ( pTypeCache && pTypeCache->GetCount() > 0 )
//	{
//		AssertMsg1( pTypeCache->GetCount() == 1, "Client has %d CTFGameAccountClient objects in his cache!  He should only have 1.", pTypeCache->GetCount() );
//		CTFGameAccountClient *pObject = dynamic_cast<CTFGameAccountClient*>( pTypeCache->GetObject( pTypeCache->GetCount() - 1 ) );
//		return pObject;
//	}
//	return NULL;
//}
//
//void CTFGCClientSystem::DumpGameAccountClient()
//{
//	CTFGameAccountClient *pObj = GetGameAccountClient();
//	if ( !pObj )
//	{
//		Msg( "Failed to find CTFGameAccountClient shared object\n" );
//		return;
//	}
//
//	Msg( "CTFGameAccountClient:\n" );
//	pObj->Dump();
//}

//CTFBetaParticipation* CTFGCClientSystem::GetBetaParticipation()
//{	
//	if ( !m_pSOCache )
//		return NULL;
//
//	CSharedObjectTypeCache *pTypeCache = m_pSOCache->GetBaseTypeCache( CTFBetaParticipation::k_nTypeID );
//	if ( pTypeCache && pTypeCache->GetCount() > 0 )
//	{
//		AssertMsg1( pTypeCache->GetCount() == 1, "Client has %d CTFBetaParticipation objects in his cache!  He should only have 1.", pTypeCache->GetCount() );
//		CTFBetaParticipation *pObject = dynamic_cast<CTFBetaParticipation*>( pTypeCache->GetObject( pTypeCache->GetCount() - 1 ) );
//		return pObject;
//	}
//	return NULL;
//}
//
//void CTFGCClientSystem::DumpBetaParticipation()
//{
//	CTFBetaParticipation *pObj = GetBetaParticipation();
//	if ( !pObj )
//	{
//		Msg( "Failed to find beta participation shared object\n" );
//		return;
//	}
//
//	Msg( "Beta participation:\n" );
//	pObj->Dump();
//}

void CTFGCClientSystem::DumpParty()
{
	CTFParty *pParty = GetParty();
	if ( !pParty )
	{
		Msg( "Failed to find party shared object\n" );
		return;
	}

	pParty->SpewDebug();
}

CTFParty* CTFGCClientSystem::GetParty()
{
	if ( !m_pSOCache )
		return NULL;

	CSharedObjectTypeCache *pTypeCache = m_pSOCache->FindBaseTypeCache( CTFParty::k_nTypeID );
	if ( pTypeCache && pTypeCache->GetCount() > 0 )
	{
		AssertMsg1( pTypeCache->GetCount() == 1, "Client has %d party objects in his cache!  He should only have 1.", pTypeCache->GetCount() );
		return static_cast<CTFParty*>( pTypeCache->GetObject( pTypeCache->GetCount() - 1 ) );
	}
	return NULL;
}


void CTFGCClientSystem::CreateNewParty()
{
	Assert( GetParty() == NULL );
	if ( GetParty() )
		return;

	switch( GetSearchMode() )
	{
	case TF_Matchmaking_LADDER:
		RequestSelectWizardStep( TF_Matchmaking_WizardStep_LADDER );
		break;

	case TF_Matchmaking_CASUAL:
		RequestSelectWizardStep( TF_Matchmaking_WizardStep_CASUAL );
		break;

	default:
		// Unhandled for now.
		// TODO: When GetSearchMode() goes away and we just deal with match groups
		//		 fixup all these damn switches everywhere
		Assert( false );
		break;
	};

	// Get the party created.  This will get our search criteria set.  It will
	// be the criteria of whatever our previous party was.  I *think* this is the
	// most intuitive thing to do, but we can instead use whatever the local guy's
	// preferred criteria if this feels weird.
	SendCreateOrUpdatePartyMsg( m_eLocalWizardStep );
}

CTFGSLobby* CTFGCClientSystem::GetLobby()
{	
	if ( !m_pSOCache )
		return NULL;

	CSharedObjectTypeCache *pTypeCache = m_pSOCache->FindBaseTypeCache( CTFGSLobby::k_nTypeID );
	if ( pTypeCache && pTypeCache->GetCount() > 0 )
	{
		AssertMsg1( pTypeCache->GetCount() == 1, "Client has %d lobby objects in his cache!  He should only have 1.", pTypeCache->GetCount() );
		CTFGSLobby *pLobby = dynamic_cast<CTFGSLobby*>( pTypeCache->GetObject( pTypeCache->GetCount() - 1 ) );
		return pLobby;
	}
	return NULL;
}

bool CTFGCClientSystem::BIsPartyLeader()
{
	CTFParty *pParty = GetParty();
	if ( pParty == NULL )
		return true;
	Assert( steamapicontext );
	Assert( steamapicontext->SteamUser() );
	if ( pParty->GetLeader() == steamapicontext->SteamUser()->GetSteamID() )
		return true;
	return false;
}

bool CTFGCClientSystem::BHasOutstandingMatchmakingPartyMessage() const
{
	return m_pPendingCreateOrUpdatePartyMsg != NULL || s_nNumWizardStepChangesWaitingForReply > 0;
}

void CTFGCClientSystem::DumpLobby()
{
	CTFGSLobby *pLobby = GetLobby();
	if ( !pLobby )
	{
		Msg( "Failed to find lobby shared object\n" );
		return;
	}

	pLobby->SpewDebug();
}

#ifdef _DEBUG
static ConVar mm_debug_ignore_connect( "mm_debug_ignore_connect", "0", FCVAR_ARCHIVE, "Debug command to discard matchmaking commands to connect to server" );
#endif

//-----------------------------------------------------------------------------
#ifdef STAGING_ONLY
static ConVar tf_competitive_convar_restrictions_disabled( "tf_competitive_convar_restrictions_disabled", "0", FCVAR_NONE, "If set, this will disable competitive convar restrictions." );
#endif // STAGING_ONLY

bool ForceCompetitiveConvars()
{
#ifdef STAGING_ONLY
	if ( tf_competitive_convar_restrictions_disabled.GetBool() )
	{
		return true;
	}
#endif // STAGING_ONLY

	bool anyFailures = false;

	Assert( ThreadInMainThread() );
	for ( ConCommandBase *ccb = g_pCVar->GetCommands(); ccb; ccb = ccb->GetNext() )
	{
		if ( ccb->IsCommand() )
			continue;

		ConVar *pVar = ( ConVar * ) ccb;

		if ( !pVar->IsCompetitiveRestricted() )
			continue;

		// Hack: This var is created by the dxconfig system, but it doesn't actually exist.
		// Skip it so we have no vars change when running a clean config.
		if ( V_stricmp( pVar->GetName(), "r_decal_cullsize" ) == 0 )
			continue;
		
		if ( !pVar->SetCompetitiveMode( true ) )
			anyFailures = true;
	}

	return !anyFailures;
}

void CTFGCClientSystem::ConnectToServer( const char *connect )
{
	CTFGSLobby *pLobby = GetLobby();
	Assert( pLobby );
	if ( !pLobby )
		return;

	// !TEST! Check convar to stub connection
	#ifdef _DEBUG
		if ( mm_debug_ignore_connect.GetBool() )
		{
			Warning(" IGNORING request to connect to %s as per mm_debug_ignore_connect\n", connect );
			return;
		}
	#endif

	Msg("Connecting to %s\n", connect );
	CUtlString connectCmd;
	connectCmd.Format( "connect %s matchmaking", connect );
	if ( engine )
	{
		const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( m_eAssignedMatchGroup );
		bool bAllowed = !( pMatchDesc && pMatchDesc->m_params.m_bForceClientSettings ) || ForceCompetitiveConvars();
		if ( !bAllowed )
		{
			// ForceCompetitiveConvars() shouldn't fail
			Assert( 0 );
		}

		engine->ClientCmd_Unrestricted( connectCmd.String() );
		//vgui::surface()->PlaySound( "ui/ui_findmatch_join_01.wav" );
	}
	else
	{
		Warning( "Failed to reconnect to game server as engine wasn't ready\n" );
	}
}

//void CTFGCClientSystem::StartWatchingGame( const CSteamID &gameServerSteamID )
//{
//	CSteamID steamIDEmpty;
//	StartWatchingGame( gameServerSteamID, steamIDEmpty );
//}
//
//void CTFGCClientSystem::StartWatchingGame( const CSteamID &gameServerSteamID, const CSteamID &watchServerSteamID )
//{
//	CProtoBufMsg<CMsgWatchGame> msg( k_EMsgGCWatchGame );
//	msg.Body().set_server_steamid( gameServerSteamID.ConvertToUint64() );
//	msg.Body().set_watch_server_steamid( watchServerSteamID.ConvertToUint64() );
//	msg.Body().set_client_version( engine->GetClientVersion() );
//	GCClientSystem()->BSendMessage( msg );
//	Msg( "StartWatchingGame request SteamID: %s, watching SteamID: %s\n", gameServerSteamID.Render(), watchServerSteamID.Render() );
//}
//
//void CTFGCClientSystem::CancelWatchGameRequest()
//{
//	CProtoBufMsg< CMsgCancelWatchGame > msg( k_EMsgGCCancelWatchGame );
//	GCClientSystem()->BSendMessage( msg );
//}
//
//void CTFGCClientSystem::StartWatchingGameResponse( const CMsgWatchGameResponse &response )
//{
//	Msg( "Received CMsgWatchGameResponse result %d.\n", response.watch_game_result() );
//
//	// Tell UI what is going on
//	if ( g_pWatchGameStatus != NULL )
//	{
//		g_pWatchGameStatus->OnWatchGameResult( response.watch_game_result() );
//	}
//
//	if ( response.watch_game_result() != CMsgWatchGameResponse_WatchGameResult_READY )
//	{
//		return;
//	}
//
//	if ( tf_auto_create_proxy.GetBool() )
//	{
//		CreateSourceTVProxy( response.source_tv_public_addr(), response.source_tv_private_addr(), response.source_tv_port() );
//		return;
//	}
//
//	RichPresence()->OnStartedWatchingGame( response.game_server_steamid(), response.watch_server_steamid() );
//
//	netadr_t serverPublicIPAddr( response.source_tv_public_addr(), response.source_tv_port() );
//	netadr_t serverPrivateIPAddr( response.source_tv_private_addr(), response.source_tv_port() );
//
//	CUtlString connect;
//
//	if ( serverPublicIPAddr.GetIP() != serverPrivateIPAddr.GetIP() )
//	{
//		connect.Format( "connect %s %s", serverPublicIPAddr.ToString(), serverPrivateIPAddr.ToString() );
//	}
//	else
//	{
//		connect.Format( "connect %s", serverPublicIPAddr.ToString() );
//	}
//
//	Msg( "StartWatchingGame: Sending console command: %s\n", connect.String() );
//	engine->ClientCmd_Unrestricted( connect );
//}
//

void CTFGCClientSystem::RequestSelectWizardStep( TF_Matchmaking_WizardStep eWizardStep )
{
	// We should only be calling this if we're the party leader
	Assert( BIsPartyLeader() );

	if ( BAllowMatchmakingSearch() )
	{
		// Make sure the wizard step makes sense for the search mode we are using
		switch ( GetSearchMode() )
		{
		case TF_Matchmaking_MVM:
#ifdef USE_MVM_TOUR
			Assert(
				eWizardStep == TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS
				|| eWizardStep == TF_Matchmaking_WizardStep_MVM_TOUR_OF_DUTY
				|| eWizardStep == TF_Matchmaking_WizardStep_MVM_CHALLENGE
				|| eWizardStep == TF_Matchmaking_WizardStep_SEARCHING
				);
#else // new mm
			Assert(
				eWizardStep == TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS
				|| eWizardStep == TF_Matchmaking_WizardStep_MVM_CHALLENGE
				|| eWizardStep == TF_Matchmaking_WizardStep_SEARCHING
				);
#endif // USE_MVM_TOUR
			break;
		case TF_Matchmaking_LADDER:
			Assert(
				eWizardStep == TF_Matchmaking_WizardStep_LADDER
				|| eWizardStep == TF_Matchmaking_WizardStep_SEARCHING );
			break;
		case TF_Matchmaking_CASUAL:
			Assert( eWizardStep == TF_Matchmaking_WizardStep_CASUAL 
				|| eWizardStep == TF_Matchmaking_WizardStep_SEARCHING );
			break;
		default:
			AssertMsg1( false, "Invalid matchmaking mode %d", (int)GetSearchMode() );
		}

		// If we already have a party, or we're asking to start searching, then
		// ask the GC to set our state.
		bool bApplyLocally = false;
		CTFParty* pParty = GetParty();
		if ( ( pParty != NULL ) || ( eWizardStep == TF_Matchmaking_WizardStep_SEARCHING ) )
		{
			SendCreateOrUpdatePartyMsg( eWizardStep );
			bApplyLocally = ( eWizardStep != TF_Matchmaking_WizardStep_SEARCHING ) || ( pParty && pParty->BOffline() );
		}
		else
		{
			// We're just setting local options by ourself right now,
			// nothing exists on the GC.  We can apply this change immediately locally.
			bApplyLocally = true;
		}

		// Can we apply this change immediately?
		if ( bApplyLocally )
		{
			m_eLocalWizardStep = eWizardStep;
			FireGameEventPartyUpdated();
		}
	}
}

EMatchmakingUIState CTFGCClientSystem::GetMatchmakingUIState()
{
	// User shutdown?
	if ( !m_bUserWantsToBeInMatchmaking )
	{
		return eMatchmakingUIState_Inactive;
	}

	// Check if we're connected / connecting to any game server
	switch ( m_eConnectState )
	{
		default:
			AssertMsg1( false, "Unknown connect state %d", m_eConnectState );
		case eConnectState_Disconnected: // we should have gotten a beginconnect message first, right?
			break;

		case eConnectState_ConnectingToMatchmade:
			return eMatchmakingUIState_Connecting;

		case eConnectState_ConnectedToMatchmade:
			return eMatchmakingUIState_InGame;

		case eConnectState_NonmatchmadeServer:
		{
			if ( BAllowMatchMakingInGame() )
				break;

			// Eh???  How did we connect to this other server without
			// exiting matchmaking alrady?
			Assert( !"In eConnectState_NonmatchmadeServer state, but m_bUserWantsToBeInMatchmaking=true" );
			EndMatchmaking();
			return eMatchmakingUIState_Inactive;
		}
	}

	// We're not connected to a server.
	// So we should not be in a game right now, unless it's for ranked games
	if ( !BAllowMatchMakingInGame() )
	{
		Assert( !engine->IsInGame() );
	}

	CTFParty *pParty = GetParty();
	CTFGSLobby *pLobby = GetLobby();

	if ( pLobby )
	{
		switch ( pLobby->GetState() )
		{
			case CSOTFGameServerLobby_State_UNKNOWN:
			default:
				AssertMsg1( false, "Unexpected lobby state %d", pLobby->GetState() );
			case CSOTFGameServerLobby_State_RUN:
			case CSOTFGameServerLobby_State_SERVERSETUP:
				return eMatchmakingUIState_Connecting;
//			case CSOTFGameServerLobby_State_NOTREADY:
//			case CSOTFGameServerLobby_State_SERVERASSIGN:
//				return eMatchmakingUIState_InQueue;
		}
	}

	// Are we in a search party?
	if ( pParty )
	{
		if ( pParty->GetState() == CSOTFParty_State_FINDING_MATCH )
		{
			return eMatchmakingUIState_InQueue;
		}
	}

	return eMatchmakingUIState_Chat;
}

void CTFGCClientSystem::AssertMakesSenseToReadSearchCriteria()
{
//	EMatchmakingUIState eState = GetMatchmakingUIState();
//	switch ( eState )
//	{
//		case eMatchmakingUIState_Chat:
//		case eMatchmakingUIState_InQueue:
//		case eMatchmakingUIState_Connecting:
//			// They might need to update the UI during this state
//			break;
//
//		case eMatchmakingUIState_Inactive:
//		case eMatchmakingUIState_InGame:
//		default:
//			// Why do you want to know?
//			AssertMsg1( false, "Invalid matchmaking UI state %d", eState );
//			break;
//	}
}

bool CTFGCClientSystem::BAllowMatchmakingSearch()
{
	bool bLeavingIncursPenalty = ( GTFGCClientSystem()->GetAssignedMatchAbandonStatus() == k_EAbandonGameStatus_AbandonWithPenalty );
	bool bAllowInGame = ( BAllowMatchMakingInGame() && !bLeavingIncursPenalty );

	EMatchmakingUIState eState = GetMatchmakingUIState();
	switch ( eState )
	{
		case eMatchmakingUIState_Chat:
		case eMatchmakingUIState_InQueue:
			// They might need to update the UI during this state
			return true;

		case eMatchmakingUIState_Inactive:
		case eMatchmakingUIState_Connecting:
		case eMatchmakingUIState_InGame:
			if ( bAllowInGame )
				return true;
			return false;
		default:
			AssertMsg1( false, "Invalid matchmaking UI state %d", eState );
			// Why do you want to know?
			return false;
	}
}

TF_MatchmakingMode CTFGCClientSystem::GetSearchMode()
{
	return m_msgLocalSearchCriteria.matchmaking_mode();
}

void CTFGCClientSystem::GetSearchChallenges( CMvMMissionSet &challenges )
{
	challenges.Clear();

	// O(n^2) goodness...
	for ( int i = 0 ; i < m_msgLocalSearchCriteria.mvm_missions_size() ; ++i )
	{
		int iChallengeIndex = GetItemSchema()->FindMvmMissionByName( m_msgLocalSearchCriteria.mvm_missions( i ).c_str() );
		if ( iChallengeIndex >= 0 )
			challenges.SetMissionBySchemaIndex( iChallengeIndex, true );
	}
}

void CTFGCClientSystem::SetSearchChallenges( const CMvMMissionSet &challenges )
{
	if ( BInternalSetSearchChallenges( challenges ) )
		FireGameEventPartyUpdated();
}

bool CTFGCClientSystem::BInternalSetSearchChallenges( const CMvMMissionSet &challenges )
{
	if ( !BAllowMatchmakingSearch() )
		return false;
	
	if ( !BIsPartyLeader() )
	{
		AssertMsg( false, "Not party leader" );
		return false;
	}

	// No change?
	CMvMMissionSet currentChallenges;
	GetSearchChallenges( currentChallenges );
	if ( currentChallenges == challenges )
	{
		return false;
	}

	// Apply the change locally
	m_msgLocalSearchCriteria.clear_mvm_missions();
	for ( int i = 0 ; i < GetItemSchema()->GetMvmMissions().Count() ; ++i )
	{
		if ( challenges.GetMissionBySchemaIndex( i ) )
		{
			m_msgLocalSearchCriteria.add_mvm_missions( GetItemSchema()->GetMvmMissionName( i ) );
		}
	}
	if ( m_msgLocalSearchCriteria.mvm_missions_size() == 0 )
	{
		m_msgLocalSearchCriteria.add_mvm_missions( "invalid" );
	}

	// Check if we need to send a message
	if ( GetParty() != NULL )
	{
		CMsgMatchSearchCriteria *pSearchCriteria = GetCreateOrUpdatePartyMsg()->mutable_search_criteria();
		pSearchCriteria->clear_mvm_missions();
		pSearchCriteria->mutable_mvm_missions()->MergeFrom( m_msgLocalSearchCriteria.mvm_missions() );
	}

	// Fire event
	return true;
}

bool CTFGCClientSystem::GetSearchJoinLate()
{
	CTFParty *pParty = GetParty();
//	if ( pParty == NULL || m_msgLocalSearchCriteria.has_late_join_ok() )
	if ( pParty == NULL )
	{
		return m_msgLocalSearchCriteria.late_join_ok();
	}
	return pParty->Obj().search_late_join_ok();
}

void CTFGCClientSystem::SetSearchJoinLate( bool bJoinLate )
{
	if ( !BAllowMatchmakingSearch() )
		return;
	
	if ( !BIsPartyLeader() )
	{
		AssertMsg( false, "Not party leader" );
		return;
	}

	if ( m_msgLocalSearchCriteria.late_join_ok() != bJoinLate )
	{
		if ( GetParty() == NULL )
		{
			m_msgLocalSearchCriteria.set_late_join_ok( bJoinLate );
			FireGameEventPartyUpdated();
		}
		else
		{
			CMsgCreateOrUpdateParty *pMsg = GetCreateOrUpdatePartyMsg();
			pMsg->mutable_search_criteria()->set_late_join_ok( bJoinLate );
		}
	}
	//CheckSendAdjustSearchCriteria();
}

EGameCategory CTFGCClientSystem::GetQuickplayGameType()
{
	return (EGameCategory)m_msgLocalSearchCriteria.quickplay_game_type();
}

void CTFGCClientSystem::SetQuickplayGameType( EGameCategory type )
{
	if ( !BAllowMatchmakingSearch() )
		return;
	
	if ( !BIsPartyLeader() )
	{
		AssertMsg( false, "Not party leader" );
		return;
	}

	if ( (EGameCategory)m_msgLocalSearchCriteria.quickplay_game_type() != type )
	{
		if ( GetParty() == NULL )
		{
			m_msgLocalSearchCriteria.set_quickplay_game_type( type );
			FireGameEventPartyUpdated();
		}
		else
		{
			CMsgCreateOrUpdateParty *pMsg = GetCreateOrUpdatePartyMsg();
			pMsg->mutable_search_criteria()->set_quickplay_game_type( type );
		}
	}
	//CheckSendAdjustSearchCriteria();
}

void CTFGCClientSystem::UpdateCustomPingTolerance()
{
	bool bEnabled = ConVarRef( "tf_custom_ping_enabled" ).GetBool();
	uint32 unValue = bEnabled ? (uint32)Max( 0, ConVarRef( "tf_custom_ping" ).GetInt() ) : 0u;

	// Don't queue unnecessary messages
	if ( m_msgLocalSearchCriteria.custom_ping_tolerance() == unValue )
		{ return; }

	if ( GetParty() && BIsPartyLeader() )
	{
		CMsgCreateOrUpdateParty *pMsg = GetCreateOrUpdatePartyMsg();
		auto *pCriteria = pMsg->mutable_search_criteria();
		pCriteria->set_custom_ping_tolerance( unValue );
	}

	m_msgLocalSearchCriteria.set_custom_ping_tolerance( unValue );
}

void CTFGCClientSystem::SelectCasualMap( uint32 nMapDefIndex, bool bSelected )
{
	CCasualCriteriaHelper casualHelper( m_msgLocalSearchCriteria.casual_criteria() );
	casualHelper.SetMapSelected( nMapDefIndex, bSelected );

	if ( casualHelper.IsValid() || !casualHelper.AnySelected() )
	{
		if ( GetParty() != NULL )
		{
			CMsgCreateOrUpdateParty *pMsg = GetCreateOrUpdatePartyMsg();
			pMsg->mutable_search_criteria()->mutable_casual_criteria()->CopyFrom( casualHelper.GetCasualCriteria() );
		}

		m_msgLocalSearchCriteria.mutable_casual_criteria()->CopyFrom( casualHelper.GetCasualCriteria() );

		FireGameEventPartyUpdated();
	}
}

void CTFGCClientSystem::ClearCasualSearchCriteria()
{
	CCasualCriteriaHelper casualHelper( m_msgLocalSearchCriteria.casual_criteria() );
	if ( casualHelper.AnySelected() )
	{
		casualHelper.Clear();

		if ( GetParty() != NULL )
		{
			CMsgCreateOrUpdateParty *pMsg = GetCreateOrUpdatePartyMsg();
			pMsg->mutable_search_criteria()->mutable_casual_criteria()->CopyFrom( casualHelper.GetCasualCriteria() );
		}

		m_msgLocalSearchCriteria.mutable_casual_criteria()->CopyFrom( casualHelper.GetCasualCriteria() );

		FireGameEventPartyUpdated();
	}
}

bool CTFGCClientSystem::IsCasualMapSelected( uint32 nMapDefIndex ) const
{
	CCasualCriteriaHelper casualHelper( m_msgLocalSearchCriteria.casual_criteria() );
	return casualHelper.IsMapSelected( nMapDefIndex );
}

bool CTFGCClientSystem::GetLocalPlayerSquadSurplus()
{
	return m_bLocalSquadSurplus;
}

void CTFGCClientSystem::SetLocalPlayerSquadSurplus( bool bSquadSurplus )
{
	if ( m_bLocalSquadSurplus != bSquadSurplus )
	{
		if ( GetParty() == NULL )
		{
			m_bLocalSquadSurplus = bSquadSurplus;
			FireGameEventPartyUpdated();
		}
		else
		{
			CMsgCreateOrUpdateParty *pMsg = GetCreateOrUpdatePartyMsg();
			pMsg->set_squad_surplus( bSquadSurplus );
		}
	}
	//CheckSendAdjustSearchCriteria();
}

bool CTFGCClientSystem::BLocalPlayerInventoryHasMvmTicket( void )
{
	CPlayerInventory *pLocalInv = TFInventoryManager()->GetLocalInventory();
	if ( pLocalInv == NULL )
		return false;

	static CSchemaItemDefHandle pItemDef_MvmTicket( CTFItemSchema::k_rchMvMTicketItemDefName );
	if ( !pItemDef_MvmTicket )
		return false;

	for ( int i = 0 ; i < pLocalInv->GetItemCount() ; ++i )
	{
		CEconItemView *pItem = pLocalInv->GetItem( i );
		Assert( pItem );
		if ( pItem->GetItemDefinition() == pItemDef_MvmTicket )
			return true;
	}

	return false;
}

int CTFGCClientSystem::GetLocalPlayerInventoryMvmTicketCount( void )
{
	int nCount = 0;

	CPlayerInventory *pLocalInv = TFInventoryManager()->GetLocalInventory();
	if ( pLocalInv )
	{
		static CSchemaItemDefHandle pItemDef_MvmTicket( CTFItemSchema::k_rchMvMTicketItemDefName );
		if ( pItemDef_MvmTicket )
		{
			for ( int i = 0 ; i < pLocalInv->GetItemCount() ; ++i )
			{
				CEconItemView *pItem = pLocalInv->GetItem( i );
				Assert( pItem );
				if ( pItem->GetItemDefinition() == pItemDef_MvmTicket )
				{
					nCount++;
				}
			}
		}
	}

	return nCount;
}

uint32 CTFGCClientSystem::GetLadderType()
{
	return m_msgLocalSearchCriteria.has_ladder_game_type() ? m_msgLocalSearchCriteria.ladder_game_type() : k_nMatchGroup_Invalid;
}

void CTFGCClientSystem::SetLadderType( uint32 nType )
{
	if ( !BIsPartyLeader() )
	{
		AssertMsg( false, "Not party leader" );
		return;
	}

	if ( m_msgLocalSearchCriteria.ladder_game_type() != nType )
	{
		if ( !GetParty() )
		{
			m_msgLocalSearchCriteria.set_ladder_game_type( nType );
			FireGameEventPartyUpdated();
		}
		else
		{
			CMsgCreateOrUpdateParty *pMsg = GetCreateOrUpdatePartyMsg();
			pMsg->mutable_search_criteria()->set_ladder_game_type( nType );
		}
	}
}

bool CTFGCClientSystem::BLocalPlayerInventoryHasSquadSurplusVoucher( void )
{
	CPlayerInventory *pLocalInv = TFInventoryManager()->GetLocalInventory();
	if ( pLocalInv == NULL )
		return false;

	static CSchemaItemDefHandle k_rchMvMSquadSurplusVoucherItemDefName( CTFItemSchema::k_rchMvMSquadSurplusVoucherItemDefName );
	if ( !k_rchMvMSquadSurplusVoucherItemDefName )
		return false;

	for ( int i = 0 ; i < pLocalInv->GetItemCount() ; ++i )
	{
		CEconItemView *pItem = pLocalInv->GetItem( i );
		Assert( pItem );
		if ( pItem->GetItemDefinition() == k_rchMvMSquadSurplusVoucherItemDefName )
			return true;
	}

	return false;
}

int CTFGCClientSystem::GetLocalPlayerInventorySquadSurplusVoucherCount( void )
{
	int nCount = 0;

	CPlayerInventory *pLocalInv = TFInventoryManager()->GetLocalInventory();
	if ( pLocalInv )
	{
		static CSchemaItemDefHandle k_rchMvMSquadSurplusVoucherItemDefName( CTFItemSchema::k_rchMvMSquadSurplusVoucherItemDefName );
		if ( k_rchMvMSquadSurplusVoucherItemDefName )
		{
			for ( int i = 0 ; i < pLocalInv->GetItemCount() ; ++i )
			{
				CEconItemView *pItem = pLocalInv->GetItem( i );
				Assert( pItem );
				if ( pItem->GetItemDefinition() == k_rchMvMSquadSurplusVoucherItemDefName )
				{
					nCount++;
				}
			}
		}
	}

	return nCount;
}

#ifdef USE_MVM_TOUR
bool CTFGCClientSystem::BGetLocalPlayerBadgeInfoForTour( int iTourIndex, uint32 *pnBadgeLevel, uint32 *pnCompletedChallenges )
{
	Assert( iTourIndex >= 0 );
	Assert( iTourIndex < GetItemSchema()->GetMvmTours().Count() );
	Assert( pnBadgeLevel );
	Assert( pnCompletedChallenges );

	*pnBadgeLevel = 0;
	*pnCompletedChallenges = 0;

	CPlayerInventory *pLocalInv = TFInventoryManager()->GetLocalInventory();
	if ( pLocalInv == NULL )
		return false;

	// We can't search for a badge without knowing which attribute to look for.
	static CSchemaAttributeDefHandle pAttribDef_MvmChallengeCompleted( CTFItemSchema::k_rchMvMChallengeCompletedMaskAttribName );
	Assert( pAttribDef_MvmChallengeCompleted );
	if ( !pAttribDef_MvmChallengeCompleted )
		return false;

	if ( iTourIndex < 0 || iTourIndex >= GetItemSchema()->GetMvmTours().Count() )
	{
		AssertMsg1( false, "Invalid tour index %d", iTourIndex );
		return false;
	}
	const CEconItemDefinition *pBadgeDef = GetItemSchema()->GetMvmTours()[iTourIndex].m_pBadgeItemDef;
	if ( pBadgeDef == NULL )
	{
		Assert( pBadgeDef );
		return false;
	}

	for ( int i = 0 ; i < pLocalInv->GetItemCount() ; ++i )
	{
		CEconItemView *pBadge = pLocalInv->GetItem( i );
		Assert( pBadge );
		if ( pBadge->GetItemDefinition() != pBadgeDef )
			continue;

		if ( !pBadge->FindAttribute( pAttribDef_MvmChallengeCompleted, pnCompletedChallenges ) )
		{
			AssertMsg( false, "Badge missing challenges completed attribute?" );
			*pnCompletedChallenges = 0;
		}

		extern uint32 GetItemDescriptionDisplayLevel( const IEconItemInterface *pEconItem );
		*pnBadgeLevel = GetItemDescriptionDisplayLevel( pBadge );
		return true;
	}

	return false;
}

int CTFGCClientSystem::GetSearchMannUpTourIndex()
{
	CTFParty *pParty = GetParty();
//	if ( pParty == NULL || m_msgLocalSearchCriteria.has_late_join_ok() )
	if ( pParty == NULL )
	{
		if ( !m_msgLocalSearchCriteria.play_for_bragging_rights() )
		{
			m_msgLocalSearchCriteria.clear_mvm_mannup_tour();
			return k_iMvmTourIndex_NotMannedUp;
		}
		return GetItemSchema()->FindMvmTourByName( m_msgLocalSearchCriteria.mvm_mannup_tour().c_str() );
	}
	return pParty->GetSearchMannUpTourIndex();
}

void CTFGCClientSystem::SetSearchMannUpTourIndex( int idxTour )
{
	Assert( GetSearchPlayForBraggingRights() );
	if ( BInternalSetSearchMannUpTourIndex( idxTour ) )
		FireGameEventPartyUpdated();
}

bool CTFGCClientSystem::BInternalSetSearchMannUpTourIndex( int idxTour )
{
	if ( !BAllowMatchmakingSearch() )
		return false;

	if ( !BIsPartyLeader() )
	{
		AssertMsg( false, "Not party leader" );
		return false;
	}

	// No change?
	if ( GetSearchMannUpTourIndex() == idxTour )
		return false;

	const char *pszTourName = "";
	if ( idxTour >= 0 )
	{
		pszTourName = GetItemSchema()->GetMvmTours()[ idxTour ].m_sTourInternalName.Get();
	}
	else
	{
		Assert( idxTour == k_iMvmTourIndex_Empty );
	}

	bool bResult = false;
	if ( GetParty() == NULL )
	{
		m_msgLocalSearchCriteria.set_mvm_mannup_tour( pszTourName );
		bResult = true;
	}
	else
	{
		CMsgCreateOrUpdateParty *pMsg = GetCreateOrUpdatePartyMsg();
		pMsg->mutable_search_criteria()->set_mvm_mannup_tour( pszTourName );
	}

	// Check if we need to deselect inappropriate challenges
	if ( idxTour >= 0 )
	{
		CMvMMissionSet challenges;
		GetSearchChallenges( challenges );
		bool bChanged = false;
		for ( int i = 0 ; i < GetItemSchema()->GetMvmMissions().Count() ; ++i )
		{
			if ( GetItemSchema()->FindMvmMissionInTour( idxTour, i ) < 0 )
			{
				if ( challenges.GetMissionBySchemaIndex( i ) )
				{
					challenges.SetMissionBySchemaIndex( i, false );
					bChanged = true;
				}
			}
		}
		if ( bChanged )
		{
			if ( BInternalSetSearchChallenges( challenges ) )
				bResult = true;
		}
	}

	return bResult;
}
#endif // USE_MVM_TOUR

bool CTFGCClientSystem::GetSearchPlayForBraggingRights()
{
	CTFParty *pParty = GetParty();
//	if ( pParty == NULL || m_msgLocalSearchCriteria.has_late_join_ok() )
	if ( pParty == NULL )
	{
		return m_msgLocalSearchCriteria.play_for_bragging_rights();
	}
	return pParty->GetSearchPlayForBraggingRights();
}

void CTFGCClientSystem::SetSearchPlayForBraggingRights( bool bPlayForBraggingRights )
{
	if ( !BAllowMatchmakingSearch() )
		return;
	
	if ( !BIsPartyLeader() )
	{
		AssertMsg( false, "Not party leader" );
		return;
	}

	// Do we need to fire local event?
	bool bFirePartyUpdated = false;

	// Any change?
#ifdef USE_MVM_TOUR
	if ( GetSearchPlayForBraggingRights() != bPlayForBraggingRights )
	{
		if ( GetParty() == NULL )
		{
			if ( m_msgLocalSearchCriteria.play_for_bragging_rights() != bPlayForBraggingRights )
			{
				m_msgLocalSearchCriteria.set_play_for_bragging_rights( bPlayForBraggingRights );
				bFirePartyUpdated = true;
			}
		}
		else
		{
			CMsgCreateOrUpdateParty *pMsg = GetCreateOrUpdatePartyMsg();
			pMsg->mutable_search_criteria()->set_play_for_bragging_rights( bPlayForBraggingRights );
		}

		// Clear tour selection when first entering mann up
		if ( bPlayForBraggingRights )
		{
			if ( BInternalSetSearchMannUpTourIndex( k_iMvmTourIndex_Empty ) )
				bFirePartyUpdated = true;
		}
	}

	// Check if we must deselect the non-Mann-UP challenges
	if ( !bPlayForBraggingRights )
	{
		m_msgLocalSearchCriteria.clear_mvm_mannup_tour();
	}
#else // new mm
	if ( GetSearchPlayForBraggingRights() != bPlayForBraggingRights )
	{
		if ( GetParty() == NULL )
		{
			if ( m_msgLocalSearchCriteria.play_for_bragging_rights() != bPlayForBraggingRights )
			{
				m_msgLocalSearchCriteria.set_play_for_bragging_rights( bPlayForBraggingRights );
			}
		}
		else
		{
			CMsgCreateOrUpdateParty *pMsg = GetCreateOrUpdatePartyMsg();
			pMsg->mutable_search_criteria()->set_play_for_bragging_rights( bPlayForBraggingRights );
		}

		bFirePartyUpdated = true;
	}
#endif // USE_MVM_TOUR

	if ( bFirePartyUpdated )
		FireGameEventPartyUpdated();
}

//void CTFGCClientSystem::CheckSendAdjustSearchCriteria()
//{
//	if ( !BMakesSenseToWriteSearchCriteria() )
//	{
//		AssertMsg1( false, "Invalid matchmaking UI state %d", GetMatchmakingUIState() );
//		return;
//	}
//
//	// We only need to do this if we have a party!
//	if ( GetParty() == NULL )
//	{
//		return;
//	}
//
//	if ( m_msgLocalSearchCriteria.has_map() ||
//		m_msgLocalSearchCriteria.has_challenge() ||
//		m_msgLocalSearchCriteria.has_late_join_ok() ||
//		m_msgLocalSearchCriteria.has_matchgroups() )
//	{
//	}
//}

void CTFGCClientSystem::SendCreateOrUpdatePartyMsg( TF_Matchmaking_WizardStep eWizardStep )
{
	CMsgCreateOrUpdateParty *pMsg = GetCreateOrUpdatePartyMsg();
	pMsg->set_wizard_step( eWizardStep );

	// If we don't have a party yet, populate message with our search criteria
	CTFParty *pParty = GetParty();
	if ( pParty == NULL )
	{
		*pMsg->mutable_search_criteria() = m_msgLocalSearchCriteria;
		pMsg->set_squad_surplus( m_bLocalSquadSurplus );
	}

	// Send the steam lobby, if we have one
	if ( m_steamIDLobby.IsValid() )
	{
		if ( pParty == NULL || pParty->GetSteamLobbyID() != m_steamIDLobby )
		{
			pMsg->set_steam_lobby_id( m_steamIDLobby.ConvertToUint64() );
		}
	}

	pMsg->set_wizard_step( eWizardStep );

	// This is important!  Send it now, even if we have a party.
	m_flSendPartyUpdateMessageTime = 0.f;

//	static ConVarRef sv_search_key("sv_search_key");
//	if ( sv_search_key.IsValid() && *sv_search_key.GetString() )
//	{
//		msg.Body().set_key( sv_search_key.GetString() );
//	}

//	static ConVarRef dota_matchgroups("dota_matchgroups");
//	if ( dota_matchgroups.IsValid() )
//	{
//		// abort if no matchgroups set
//		if ( dota_matchgroups.GetInt() == 0 )
//		{
//			DOTA_SF_AddErrorMessage( "#DOTA_Matchmaking_NoRegion_Error" );
//			return;
//		}
//
//		msg.Body().set_matchgroups( dota_matchgroups.GetInt() );
//	}
}

void CTFGCClientSystem::SendExitMatchmaking( bool bExplicitAbandon )
{
	Msg( "Sending request to exit matchmaking system [ abandon = %d ]\n", bExplicitAbandon );
	CProtoBufMsg<CMsgExitMatchmaking> msg( k_EMsgGCExitMatchmaking );
	msg.Body().set_explicit_abandon( bExplicitAbandon );
	msg.Body().set_party_id( GetParty() ? GetParty()->GetGroupID() : 0 );
	msg.Body().set_lobby_id( GetLobby() ? GetLobby()->GetGroupID() : 0 );
	GCClientSystem()->BSendMessage( msg );

	// We're done!  No more messages!
	if ( m_pPendingCreateOrUpdatePartyMsg )
	{
		delete m_pPendingCreateOrUpdatePartyMsg;
		m_pPendingCreateOrUpdatePartyMsg = NULL;
		m_flSendPartyUpdateMessageTime = FLT_MAX;
		s_nNumWizardStepChangesWaitingForReply = 0;
	}

	if ( bExplicitAbandon && m_steamIDGCAssignedMatch.IsValid() && !m_bAssignedMatchEnded )
	{
		// Consider this match over on our end, since we're not waiting for the lobby to update (the GC may even be gone)
		GCMatchmakingDebugSpew( 1, "Sending request to exit matchmaking, marking assigned match as ended\n" );
		m_bAssignedMatchEnded = true;
	}
}

void CTFGCClientSystem::SaveCasualSearchCriteriaToDisk()
{
	std::string strOut;
	google::protobuf::TextFormat::PrintToString( m_msgLocalSearchCriteria.casual_criteria(), &strOut );
	CUtlBuffer bufOut;
	bufOut.SetBufferType( true, true );
	bufOut.PutString( strOut.c_str() );
	g_pFullFileSystem->WriteFile( s_pszCasualCriteriaSaveFileName, NULL, bufOut );
}

void CTFGCClientSystem::RejoinActiveMatch( void )
{
	// Dialog already exists, just quit
	if ( s_pRejoinLobbyDialog )
		return;

	if ( enginevgui == NULL || GetClientModeTFNormal()->GameUI() == NULL )
		return;

	// Check if this player is in Abandon territory, if so warn them
	EAbandonGameStatus eAbandonStatus = GTFGCClientSystem()->GetAssignedMatchAbandonStatus();
	const char* pszTitle = "#TF_MM_Rejoin_Title";
	const char* pszBody = NULL;
	const char* pszConfirm = "#TF_MM_Rejoin_Confirm";
	const char* pszCancel = NULL;

	switch ( eAbandonStatus )
	{
	case k_EAbandonGameStatus_Safe:
		pszBody = "#TF_MM_Rejoin_BaseText";
		pszCancel = "#TF_MM_Rejoin_Leave";
		break;
	case k_EAbandonGameStatus_AbandonWithoutPenalty:
		pszBody = "#TF_MM_Rejoin_AbandonText_NoPenalty";
		pszCancel = "#TF_MM_Rejoin_Abandon";
		break;
	case k_EAbandonGameStatus_AbandonWithPenalty:
		pszBody = "#TF_MM_Rejoin_AbandonText";
		pszCancel = "#TF_MM_Rejoin_Abandon";
		break;
	}

	s_pRejoinLobbyDialog = vgui::SETUP_PANEL( new CTFRejoinConfirmDialog(
		pszTitle,
		pszBody,
		pszConfirm,
		pszCancel,
		&OnRejoinMvMLobbyDialogCallBack,
		NULL
	));

	if ( s_pRejoinLobbyDialog )
	{
		s_pRejoinLobbyDialog->Show();
		// VGUI is being dumb so I need to manually calculate this windows position
		int sW, sT, dW, dT;
		vgui::surface()->GetScreenSize( sW, sT );
		s_pRejoinLobbyDialog->GetSize( dW, dT );
		s_pRejoinLobbyDialog->SetPos( (sW - dW) / 2, (sT - dT) / 2 );
	}
}

void CTFGCClientSystem::BeginMatchmaking( TF_MatchmakingMode mode )
{
	Assert( !m_bUserWantsToBeInMatchmaking );
	m_bUserWantsToBeInMatchmaking = true;
	m_msgMatchmakingProgress.Clear();
	m_nPendingAutoJoinPartyID = 0;

	// Disconnect from any server we're already in
	if ( ( mode != TF_Matchmaking_LADDER ) && ( mode != TF_Matchmaking_CASUAL ) )
	{
		engine->ClientCmd_Unrestricted( "disconnect" );
	}

	TF_Matchmaking_WizardStep eWizardStep = TF_Matchmaking_WizardStep_INVALID;
	switch ( mode )
	{
		case TF_Matchmaking_MVM:
			eWizardStep = TF_Matchmaking_WizardStep_MVM_PLAY_FOR_BRAGGING_RIGHTS;
			break;

		case TF_Matchmaking_LADDER:
			eWizardStep = TF_Matchmaking_WizardStep_LADDER;
			break;

		case TF_Matchmaking_CASUAL:
			eWizardStep = TF_Matchmaking_WizardStep_CASUAL;
			break;

		default:
			AssertMsg1( false, "Unknown wizard step %d\n", (int)mode );
			break;
	}

	// Check if we don't already have a party, then set some default search options
	CTFParty *pParty = GetParty();
	if ( pParty == NULL )
	{
		m_msgLocalSearchCriteria.set_matchmaking_mode( mode );
		m_eLocalWizardStep = eWizardStep;

		// Default late join option
		m_msgLocalSearchCriteria.set_late_join_ok( false );

		// Default Mann Up state based on whether they have a ticket
		SetSearchPlayForBraggingRights( mode == TF_Matchmaking_MVM && BLocalPlayerInventoryHasMvmTicket() );

		FireGameEventPartyUpdated();
	}
	else
	{

		// Hmmm. we already have a party.  We really should already be in the correct mode.
		if ( pParty->GetMatchmakingMode() != mode && BIsPartyLeader() )
		{
			CMsgCreateOrUpdateParty *pMsg = GetCreateOrUpdatePartyMsg();
			pMsg->mutable_search_criteria()->set_matchmaking_mode( mode );
			pMsg->set_wizard_step( eWizardStep );
		}
	}

	// Post an event so we'll know that we joined the lobby OK
	IGameEvent *pEvent = gameeventmanager->CreateEvent( "mm_lobby_member_join" );
	if ( !pEvent )
		return;
	pEvent->SetString( "steamid", CFmtStr("%llu", steamapicontext->SteamUser()->GetSteamID().ConvertToUint64() ) );
	pEvent->SetInt( "solo", 1 ); // is this always true?
	gameeventmanager->FireEventClientSide( pEvent );

}

bool CTFGCClientSystem::BAllowMatchMakingInGame( void ) const
{
	return !BHaveLiveMatch();
}

void CTFGCClientSystem::EndMatchmaking( bool bSendAbandonLobby /* = false */)
{
	// Set flag, so if GC sends us any further messages, we'll know to ignore them
	m_bUserWantsToBeInMatchmaking = false;
	m_bWantToActivateInviteUI = false;
	m_msgMatchmakingProgress.Clear();

	// If bSendAbandonLobby is false, this will only ask the GC to drop us from our party.  If this message races with
	// us finding a match, the GC will decline.
	// ( If that happens, the rejoin game in progress dialog will pop up and resolve the race, so you can't accidentally
	//   abandon by canceling queue at the right millisecond )
	SendExitMatchmaking( bSendAbandonLobby );

	if ( BConnectedToMatchServer( true ) )
	{
		// If we were connected to a server we matchmade into, then disconnect
		switch ( m_eConnectState )
		{
			default:
				AssertMsg1( false, "Unknown connect state %d", m_eConnectState );
			case eConnectState_NonmatchmadeServer:
			case eConnectState_Disconnected:
				break;

			case eConnectState_ConnectingToMatchmade:
			case eConnectState_ConnectedToMatchmade:
				Msg( "Disconnecting from matchmade server\n" );
				engine->ClientCmd_Unrestricted( "disconnect" );
				break;
		}
	}
}

bool CTFGCClientSystem::BExitMatchmakingAfterDisconnect( void )
{
	return BConnectedToMatchServer( true );
}

void CTFGCClientSystem::LeaveSteamLobby()
{
	if ( m_steamIDLobby.IsValid() )
	{
		Assert( steamapicontext );
		Assert( m_steamIDLobby.IsLobby() );
		if ( steamapicontext )
		{
			Msg( "Leaving steam lobby %s\n", m_steamIDLobby.Render() );
			steamapicontext->SteamMatchmaking()->LeaveLobby( m_steamIDLobby );
		}
		m_steamIDLobby = CSteamID();
	}
}

int CTFGCClientSystem::CheckSteamLobbyCreated()
{
	if ( !m_bUserWantsToBeInMatchmaking )
	{
		Assert( m_bUserWantsToBeInMatchmaking ); // why are you calling this?
		LeaveSteamLobby();
		return -1;
	}

	// Already in a lobby?
	if ( m_steamIDLobby.IsValid() )
		return 1;

	// Do we have the interfaces we need?
	if ( steamapicontext == NULL || steamapicontext->SteamMatchmaking() == NULL )
		return -1;

	// Is a creation request already in progress?
	if ( m_eCreateLobbyStatus == -1 )
		return 0;

	Msg( "Creating Steam lobby\n" );

	m_eCreateLobbyStatus = -1;
	steamapicontext->SteamMatchmaking()->CreateLobby( k_ELobbyTypePrivate, MAX_PLAYERS );
	return 0;
}

void CTFGCClientSystem::RequestActivateInvite()
{

	// What state are we in?
	switch ( GetMatchmakingUIState() )
	{
		case eMatchmakingUIState_Chat:
			break;

		case eMatchmakingUIState_InQueue:
			Warning( "Leaving matchmaking queue due to request to active friend invite UI\n" );
			break;

		default:
			Warning( "Can only invite friends to party when in the chat state, or the searching state\n" );
			m_bWantToActivateInviteUI = false;
			return;
	}

	// Set flag.  We'll try to activate the UI at he earliest opportunity
	m_bWantToActivateInviteUI = true;

	// Create our party I we don't have one, and also
	// get us out of the queue, if we're in it.
	SendCreateOrUpdatePartyMsg( GetWizardStep() );

	// Check if we're ready to activate the UI now
	CheckReadyToActivateInvite();
}

//-----------------------------------------------------------------------------
// Purpose: Ask the GC for the latest global casual criteria stats
//-----------------------------------------------------------------------------
void CTFGCClientSystem::RequestMatchMakerStats() const
{
	CProtoBufMsg<CMsgGCRequestMatchMakerStats> msg( k_EMsgGCRequestMatchMakerStats );
	GCClientSystem()->BSendMessage( msg );
}

//-----------------------------------------------------------------------------
// Purpose: Set our cached global casual criteria stats and figure out the most
//			popular map so we can do some health computations later.
//-----------------------------------------------------------------------------
void CTFGCClientSystem::SetMatchMakerStats( const CMsgGCMatchMakerStatsResponse newStats )
{
	m_MatchMakerStats = newStats;

	// Update m_nMostSearchedMapCount to be the largest in m_CasualCriteriaStats
	m_nMostSearchedMapCount = 0;
	for( int iMap=0; iMap < m_MatchMakerStats.map_count_size(); ++iMap )
	{
		m_nMostSearchedMapCount = Max( m_nMostSearchedMapCount, m_MatchMakerStats.map_count( iMap ) );
	}

	// put data_center_population in dict so we don't have to loop over and strcmp everytime we ask for it
	COMPILE_TIME_ASSERT( ARRAYSIZE( m_dictDataCenterPopulationRatio ) == k_nMatchGroup_Count );
	Assert( m_MatchMakerStats.matchgroup_data_center_population_size() == k_nMatchGroup_Count );
	for ( int iMatchGroup=0; iMatchGroup<k_nMatchGroup_Count; ++iMatchGroup )
	{
		m_dictDataCenterPopulationRatio[ iMatchGroup ].Purge();
		const auto& matchgroup_datacenter_population = m_MatchMakerStats.matchgroup_data_center_population( iMatchGroup );
		for ( int iDataCenter=0; iDataCenter<matchgroup_datacenter_population.data_center_population_size(); ++iDataCenter )
		{
			auto dcp = matchgroup_datacenter_population.data_center_population( iDataCenter );
			m_dictDataCenterPopulationRatio[ iMatchGroup ].Insert( dcp.name().c_str(), dcp.health_ratio() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Given a health ratio, get the health data
//-----------------------------------------------------------------------------
CTFGCClientSystem::MatchMakerHealthData_t CTFGCClientSystem::GetHealthBracketForRatio( float flRatio ) const
{
	CTFGCClientSystem::MatchMakerHealthData_t data;
	data.m_flRatio = flRatio;

	static const Color colorBad( 128, 128, 128, 60 );
	static const Color colorOK( 188, 112, 0, 128 );
	static const Color colorGood( 94, 150, 49, 255 );

	// Walk through our brackets and fine where we fall and setup data accordingly
	if ( flRatio < 0.3f )
	{
		data.m_colorBar = LerpColor( colorBad, colorOK, RemapValClamped( flRatio, 0.2f, 0.3f, 0.f, 1.f ) );
		data.m_strLocToken = "TF_Casual_QueueEstimation_Bad";
	}
	else if ( flRatio < 0.7f )
	{
		data.m_colorBar = LerpColor( colorOK, colorGood, RemapValClamped( flRatio, 0.3f, 0.7f, 0.f, 1.f ) );
		data.m_strLocToken = "TF_Casual_QueueEstimation_OK";
	}
	else
	{
		data.m_colorBar = colorGood;
		data.m_strLocToken = "TF_Casual_QueueEstimation_Good";
	}

	return data;
}

#ifdef STAGING_ONLY
ConVar tf_fake_casual_map_stats( "tf_fake_casual_map_stats", "0" );
#endif

//-----------------------------------------------------------------------------
// Purpose: Really here just so we can shortcircuit some staging_only debug
//-----------------------------------------------------------------------------
inline uint32 GetCountForMap( const CMsgGCMatchMakerStatsResponse msg, int nIndex )
{
#ifdef STAGING_ONLY
	// If we're faking, then fake some stats
	if ( tf_fake_casual_map_stats.GetBool() )
	{
		CUniformRandomStream randomStream;
		randomStream.SetSeed( nIndex + tf_fake_casual_map_stats.GetInt() );
		return randomStream.RandomInt( 0, 100000 );
	}
#endif

	return msg.map_count( nIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Get the overall health of the current local casual criteria. 
//			Currently just takes the best individual map health.
//-----------------------------------------------------------------------------
CTFGCClientSystem::MatchMakerHealthData_t CTFGCClientSystem::GetOverallHealthDataForLocalCriteria() const
{
	uint32 nMostSearchedCount = m_nMostSearchedMapCount;
	uint32 nLargestOfSelected = 0;
	CCasualCriteriaHelper helper( m_msgLocalSearchCriteria.casual_criteria() );

#ifdef STAGING_ONLY
	if ( tf_fake_casual_map_stats.GetBool() )
	{
		nMostSearchedCount = 100000;

		// Force some fake stats if we dont have the baseline message yet
		if ( m_MatchMakerStats.map_count_size() == 0 )
		{
			for( int i=0; i < GetItemSchema()->GetMasterMapsList().Count(); ++i )
			{
				if ( helper.IsMapSelected( i ) )
				{
					nLargestOfSelected = Max( nLargestOfSelected, GetCountForMap( m_MatchMakerStats, i ) );
				}
			}
		}
	}
#endif

	// No data -- we assume bad
	if ( nMostSearchedCount == 0 )
		return GetHealthBracketForRatio( 0.f );

	// Go through all the locallty selected maps and find the one with the best health.
	// Use that to get our estimated overall criteria health.
	for( int i=0; i < m_MatchMakerStats.map_count_size(); ++i )
	{
		if ( helper.IsMapSelected( i ) )
		{
			nLargestOfSelected = Max( nLargestOfSelected, GetCountForMap( m_MatchMakerStats, i ) );
		}
	}

	return GetHealthBracketForRatio( (float)nLargestOfSelected / (float)nMostSearchedCount );
}

//-----------------------------------------------------------------------------
// Purpose: Gets the health of a given map
//-----------------------------------------------------------------------------
CTFGCClientSystem::MatchMakerHealthData_t CTFGCClientSystem::GetHealthDataForMap( uint32 nMapIndex ) const
{
	uint32 nMostSearchedCount = m_nMostSearchedMapCount;
	uint32 nLargestOfSelected = 0;
#ifdef STAGING_ONLY
	if ( tf_fake_casual_map_stats.GetBool() )
	{
		nMostSearchedCount = 100000;
		nLargestOfSelected = GetCountForMap( m_MatchMakerStats, nMapIndex );
	}
#endif

	// No data -- we assume bad
	if ( nMostSearchedCount == 0 )
		return GetHealthBracketForRatio( 0.f );
	
	if ( (int)nMapIndex < m_MatchMakerStats.map_count_size() )
	{
		nLargestOfSelected = GetCountForMap( m_MatchMakerStats, nMapIndex );
	}

	return GetHealthBracketForRatio( (float)nLargestOfSelected / (float)nMostSearchedCount );
}


//CON_COMMAND( tf_resend_so_cache, "Resend SO cache" )
//{
//	// Force a resend of our SO cache.
//	CProtoBufMsg<CMsgForceSOCacheResend> msg( k_EMsgForceSOCacheResend );
//	GCClientSystem()->BSendMessage( msg );
//}
//
//CON_COMMAND( tf_get_news, "Request game news from the GC" )
//{
//	if ( args.ArgC() < 2 )
//		return;
//
//	CGCClientJobGetNews *pJob = new CGCClientJobGetNews( GCClientSystem()->GetGCClient(), atoi( args[1] ) );
//	pJob->StartJob( NULL );
//}

//CON_COMMAND( tf_party_test, "Tests sending a party invite" )
//{
//	CProtoBufMsg<CMsgInviteToParty> msg( k_EMsgGCInviteToParty );
//	msg.Body().set_steam_id( steamapicontext->SteamUser()->GetSteamID().ConvertToUint64() );
////	msg.Body().set_client_version( engine->GetClientVersion() );
//	GCClientSystem()->BSendMessage( msg );
//}

CON_COMMAND( tf_party_debug, "Prints local party objects" )
{
	GTFGCClientSystem()->DumpParty();
}

CON_COMMAND( tf_invite_debug, "Prints local invite objects" )
{
	GTFGCClientSystem()->DumpInvites();
}

CON_COMMAND( tf_lobby_debug, "Prints local lobby objects" )
{
	GTFGCClientSystem()->DumpLobby();
}

//CON_COMMAND( tf_beta_debug, "Prints local dota beta participation object" )
//{
//	GTFGCClientSystem()->DumpBetaParticipation();
//}

//CON_COMMAND( tf_game_account_debug, "Prints game account info" )
//{
//	GTFGCClientSystem()->DumpGameAccountClient();
//}

//class CGCClientJobNestedTest : public GCSDK::CGCClientJob
//{
//public:
//	CGCClientJobNestedTest( GCSDK::CGCClient *pGCClient, int nIndex ) : GCSDK::CGCClientJob( pGCClient ), m_nIndex( nIndex ) {	}
//
//	virtual bool BYieldingRunJob( void *pvStartParam )
//	{
//		Msg( "Nested job %d running!\n", m_nIndex );
//		BYieldingWaitOneFrame();
//		Msg( "Nested job %d done running!\n", m_nIndex );
//		return true;
//	}
//	int m_nIndex;
//};

////-----------------------------------------------------------------------------
//// Purpose: Job for being told when the user GC connection is established
////-----------------------------------------------------------------------------
//class CGCClientJobClientWelcome : public GCSDK::CGCClientJob
//{
//public:
//	CGCClientJobClientWelcome( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }
//
//	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
//	{
//		GCSDK::CProtoBufMsg<CMsgClientWelcome> msg( pNetPacket );
//
//		// Validate version
//		int engineClientVersion = engine->GetClientVersion();
//		int gcClientVersion = (int)msg.Body().version();
//
//		// Version checking is enforced if both sides do not report zero as their version
//		if ( engineClientVersion && gcClientVersion && engineClientVersion != gcClientVersion )
//		{
//			IGameEvent *pEvent = gameeventmanager->CreateEvent( "gc_mismatched_version" );
//			if ( pEvent )
//			{
//				gameeventmanager->FireEventClientSide( pEvent );
//			}
//		}
//
//		g_bClientReceivedGCWelcome = true;
//
//		if ( GTFGCClientSystem() && gameeventmanager )
//		{
//			// when client has reconnected to Steam, wipe dashboard caches.
//			if ( Dashboard() )
//			{
//				Dashboard()->ClearDashboardCaches();
//			}
//
//			Msg( "CGCClientJobUserSessionCreated::BYieldingRunJobFromMsg firing event\n" );
//			IGameEvent *pEvent = gameeventmanager->CreateEvent( "gc_user_session_created" );
//			if ( pEvent )
//			{
//				gameeventmanager->FireEventClientSide( pEvent );
//			}
//		}
//		else
//		{
//			Msg( "CGCClientJobUserSessionCreated::BYieldingRunJobFromMsg not firing event\n" );
//		}
//
//		if ( DOTAChat() )
//		{
//			if ( !DOTAChat()->HasJoinedStartupChannels() )
//			{
//				DOTAChat()->JoinStartupChannels();
//			}
//			else
//			{
//				DOTAChat()->SetRejoinChannels();
//			}
//		}
//		return true;
//	}
//};
//GC_REG_JOB( GCSDK::CGCClient, CGCClientJobClientWelcome, "CGCClientJobClientWelcome", k_EMsgGCClientWelcome, k_EServerTypeGCClient );

////-----------------------------------------------------------------------------
//// Purpose: Job for being told when the user's GC session is created
////-----------------------------------------------------------------------------
//class CGCClientInvitationCreated : public GCSDK::CGCClientJob
//{
//public:
//	CGCClientInvitationCreated( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }
//
//	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
//	{
//		GCSDK::CProtoBufMsg<CMsgInvitationCreated> msg( pNetPacket );
//
//		CUtlString commandline;
//		commandline.Format( "+invite %llu", msg.Body().group_id() );
//		steamapicontext->SteamFriends()->InviteUserToGame( msg.Body().steam_id(), commandline.String() );
//
//		return true;
//	}
//};
//GC_REG_JOB( GCSDK::CGCClient, CGCClientInvitationCreated, "CGCClientInvitationCreated", k_EMsgGCInvitationCreated, k_EServerTypeGCClient );

class CGCClientMatchmakingProgress : public GCSDK::CGCClientJob
{
public:
	CGCClientMatchmakingProgress( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }

	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgMatchmakingProgress> msg( pNetPacket );
		GTFGCClientSystem()->m_msgMatchmakingProgress = msg.Body();
		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCClientMatchmakingProgress, "CGCClientMatchmakingProgress", k_EMsgGCMatchmakingProgress, k_EServerTypeGCClient );


class CGCClientMatchMakerStats : public GCSDK::CGCClientJob
{
public:
	CGCClientMatchMakerStats( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }

	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgGCMatchMakerStatsResponse> msg( pNetPacket );
		GTFGCClientSystem()->SetMatchMakerStats( msg.Body() );

		IGameEvent *event = gameeventmanager->CreateEvent( "matchmaker_stats_updated" );
		if ( event )
		{
			gameeventmanager->FireEventClientSide( event );
		}

		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCClientMatchMakerStats, "CGCClientMatchMakerStats", k_EMsgGCMatchMakerStatsResponse, k_EServerTypeGCClient );

class CGCClientSurveyRequest : public GCSDK::CGCClientJob
{
public:
	CGCClientSurveyRequest( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }

	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgGCSurveyRequest> msg( pNetPacket );
		GTFGCClientSystem()->SetSurveyRequest( msg.Body() );

		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCClientSurveyRequest, "CGCClientSurveyRequest", k_EMsgGC_SurveyQuestionRequest, k_EServerTypeGCClient );


////-----------------------------------------------------------------------------
//class CGCClientJobFindSourceTVGamesDebug : public GCSDK::CGCClientJob
//{
//public:
//	CGCClientJobFindSourceTVGamesDebug( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient )
//	{
//		
//	}
//
//	virtual bool BYieldingRunJob( void *pvStartParam )
//	{
//		CProtoBufMsg<CMsgFindSourceTVGames> msg( k_EMsgGCFindSourceTVGames );	
//		CProtoBufMsg<CMsgSourceTVGamesResponse> msgResponse( k_EMsgGCSourceTVGamesResponse );
//
//		static ConVarRef sv_search_key("sv_search_key");
//		if ( sv_search_key.IsValid() && *sv_search_key.GetString() )
//		{
//			msg.Body().set_search_key( sv_search_key.GetString() );
//		}
//
//		bool bRet = BYldSendMessageAndGetReply( msg, 15, &msgResponse, k_EMsgGCSourceTVGamesResponse );
//		if ( !bRet )
//		{
//			Warning( "CGCClientJobFindSourceTVGamesDebug failed to get reply\n" );
//			return false;
//		}
//
//		Msg( "CGCClientJobFindSourceTVGamesDebug: %d\n", msgResponse.Body().games_size() );
//		for ( int i = 0; i < msgResponse.Body().games_size(); i++ )
//		{
//			const CSourceTVGame &game = msgResponse.Body().games(i);
//			Msg( "  Game %d:\n", i );
//
//			CUtlString sGoodPlayers;
//			for ( int p = 0; p < game.good_players_size(); p++ )
//			{
//				if ( sGoodPlayers.Length() > 0 )
//				{
//					sGoodPlayers += ", ";
//				}
//				sGoodPlayers += game.good_players(p).name().c_str();
//			}
//
//			CUtlString sBadPlayers;
//			for ( int p = 0; p < game.bad_players_size(); p++ )
//			{
//				if ( sBadPlayers.Length() > 0 )
//				{
//					sBadPlayers += ", ";
//				}
//				sBadPlayers += game.bad_players(p).name().c_str();
//			}
//
//			CUtlString sOtherPlayers;
//			for ( int p = 0; p < game.other_players_size(); p++ )
//			{
//				if ( sOtherPlayers.Length() > 0 )
//				{
//					sOtherPlayers += ", ";
//				}
//				sOtherPlayers += game.other_players(p).name().c_str();
//			}
//
//			CSteamID steamIDServer( game.server_steamid() );
//			Msg( "     SteamID: %s\n    Good: %s\n    Bad:  %s\n    Other:  %s\n", steamIDServer.Render(), sGoodPlayers.Get(), sBadPlayers.Get(), sOtherPlayers.Get() );
//		}
//
//		return true;
//	}
//
//private:
//};

////-----------------------------------------------------------------------------
//// Purpose: Receive a broadcast message for notification
////-----------------------------------------------------------------------------
//class CGCClientJobDOTABroadcastNotificationClient : public GCSDK::CGCClientJob
//{
//public:
//	CGCClientJobDOTABroadcastNotificationClient( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }
//
//	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
//	{
//		CProtoBufMsg<CMsgDOTABroadcastNotification> msg( pNetPacket );
//
//		wchar_t wszText[256];
//		if ( g_pVGuiLocalize->ConvertANSIToUnicode( msg.Body().message().c_str(), wszText, sizeof( wszText ) ) <= 0 )
//			return false;
//
//		// create a console chat message
//		KeyValues *pChatMsg = new KeyValues( "Command::Game::Chat" );
//		KeyValues::AutoDelete autodelete( pChatMsg );
//		pChatMsg->SetString( "run", "all" );
//		pChatMsg->SetUint64( "xuid", 0 );
//		pChatMsg->SetString( "name", "Console" );
//		pChatMsg->SetWString( "chat", wszText );
//
//		// make each channel receive the message
//		for ( int i = 0; i < DOTAChat()->GetNumChannels(); ++i )
//		{
//			// receive message immediately
//			DOTAChat()->GetChannel(i)->ReceiveMessage( pChatMsg );
//		}
//
//		return true;
//	}
//};
//GC_REG_JOB( GCSDK::CGCClient, CGCClientJobDOTABroadcastNotificationClient, "CGCClientJobDOTABroadcastNotificationClient", k_EMsgGCBroadcastNotification, k_EServerTypeGCClient );

//-----------------------------------------------------------------------------
//CON_COMMAND( tf_find_source_tv_games, "Request game news from the GC" )
//{
//	CGCClientJobFindSourceTVGamesDebug *pJob = new CGCClientJobFindSourceTVGamesDebug( GCClientSystem()->GetGCClient() );
//	pJob->StartJob( NULL );
//}

//CON_COMMAND( tf_set_lobby_details, "Set game/team names" )
//{
//	if ( args.ArgC() != 6 )
//	{
//		Msg( "Usage: tf_set_lobby_details <game name> <radiant team name> <radiant team logo> <dire team name> <dire team logo>\n" );
//		return;
//	}
//
//	CTFGSLobby *pLobby = GTFGCClientSystem() ? GTFGCClientSystem()->GetLobby() : NULL;
//	if ( !pLobby )
//	{
//		Msg( "No lobby found.\n" );
//		return;
//	}
//	
//	CProtoBufMsg<CMsgPracticeLobbySetDetails> msg( k_EMsgGCPracticeLobbySetDetails );
//	msg.Body().set_lobby_id( pLobby->GetGroupID() );
//	msg.Body().set_game_name( args[1] );
//	msg.Body().add_team_details();		// radiant
//	msg.Body().add_team_details();		// dire
//	msg.Body().mutable_team_details( DOTA_GC_TEAM_GOOD_GUYS )->set_team_name( args[2] );
//	msg.Body().mutable_team_details( DOTA_GC_TEAM_GOOD_GUYS )->set_team_logo( args[3] );
//	msg.Body().mutable_team_details( DOTA_GC_TEAM_BAD_GUYS )->set_team_name( args[4] );
//	msg.Body().mutable_team_details( DOTA_GC_TEAM_BAD_GUYS )->set_team_logo( args[5] );
//	GCClientSystem()->BSendMessage( msg );
//}
//
//CON_COMMAND( request_today_messages, "Ask the GC for a list of today messages" )
//{
//	Msg( "Requesting today messages...\n" );
//	CProtoBufMsg<CMsgDOTARequestTodayMessages> msg( k_EMsgGCRequestTodayMessages );
//	GCClientSystem()->BSendMessage( msg );
//}


//class CUtlSortVectorTodayMessageGreater
//{
//public:
//	bool Less( const CMsgDOTATodayMessages_TodayMessage& lhs, const CMsgDOTATodayMessages_TodayMessage& rhs, void * )
//	{
//		return lhs.date() > rhs.date();
//	}
//};
//
////-----------------------------------------------------------------------------
//// Purpose: Receive a list of today messages
////-----------------------------------------------------------------------------
//class CGCClientJobDOTATodayMessages : public GCSDK::CGCClientJob
//{
//public:
//	CGCClientJobDOTATodayMessages( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }
//
//	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
//	{
//		CProtoBufMsg<CMsgDOTATodayMessages> msg( pNetPacket );
//
//		g_pFullFileSystem->CreateDirHierarchy( "resource/flash3/images/today/", "GAME" );
//
//		CUtlSortVector< CMsgDOTATodayMessages_TodayMessage, CUtlSortVectorTodayMessageGreater > sortedMessageList;
//
//		int nMessages = msg.Body().messages_size();
//		for ( int i = 0; i < nMessages; i++ )
//		{
//			sortedMessageList.Insert( msg.Body().messages( i ) );
//		}
//
//		CMsgDOTATodayMessages sortedMessages;
//		for ( int i = 0; i < sortedMessageList.Count(); i++ )
//		{
//			CMsgDOTATodayMessages_TodayMessage *pNewMessage = sortedMessages.add_messages();
//			*pNewMessage = sortedMessageList[i];
//		}
//		if ( tf_debug_today_message_sorting.GetBool() )
//		{
//			Msg ( "\nUNSORTED ***\n = %s\n", sortedMessages.DebugString().c_str() );
//			Msg ( "\nSORTED ***\n = %s\n", msg.Body().DebugString().c_str() );
//		}
//		GTFGCClientSystem()->SetTodayMessages( &sortedMessages );
//
//		IGameEvent *pEvent = gameeventmanager->CreateEvent( "today_messages_updated" );
//		if ( pEvent )
//		{
//			pEvent->SetInt( "num_messages", nMessages );
//			gameeventmanager->FireEventClientSide( pEvent );
//		}
//		
//		// make sure we have all the today images downloaded
//		for ( int i = 0; i < nMessages; i++ )
//		{
//			const char *pszImageURL = msg.Body().messages( i ).image_url().c_str();
//			if ( pszImageURL && pszImageURL[0] )
//			{
//				const char *pszFilename = V_UnqualifiedFileName( pszImageURL );
//				if ( pszFilename && pszFilename[0] )
//				{
//					char szBuffer[MAX_PATH];
//					szBuffer[0] = '\0';
//					V_strcat( szBuffer, "resource/flash3/images/today/", sizeof( szBuffer ) );
//					V_strcat( szBuffer, pszFilename, sizeof( szBuffer ) );
//					GTFGCClientSystem()->DownloadFile( pszImageURL, szBuffer );
//				}
//			}
//		}
//
//		return true;
//	}
//};
//GC_REG_JOB( GCSDK::CGCClient, CGCClientJobDOTATodayMessages, "CGCClientJobDOTATodayMessages", k_EMsgGCTodayMessages, k_EServerTypeGCClient );
//
///*
//CON_COMMAND( reports_remaining, "Request number of reports remaining this week" )
//{
//	CProtoBufMsg<CMsgDOTAReportsRemainingRequest> msg( k_EMsgGCReportsRemainingRequest );
//	GCClientSystem()->BSendMessage( msg );
//
//	Msg( "Report submitted\n" );
//}
//*/
//
////-----------------------------------------------------------------------------
//// Purpose: Receive number of reports remaining
////-----------------------------------------------------------------------------
//class CGCClientJobDOTAReportsRemaining : public GCSDK::CGCClientJob
//{
//public:
//	CGCClientJobDOTAReportsRemaining( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }
//
//	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
//	{
//		CProtoBufMsg<CMsgDOTAReportsRemainingResponse> msg( pNetPacket );
//		//Msg( "You have %u positive and %u negative reports remaining\n", msg.Body().num_positive_reports_remaining(), msg.Body().num_negative_reports_remaining() );
//		//Msg( "You have %u positive and %u negative reports total\n", msg.Body().num_positive_reports_total(), msg.Body().num_negative_reports_total() );
//
//		IGameEvent *pEvent = gameeventmanager->CreateEvent( "player_report_counts_updated" );
//		if ( pEvent )
//		{
//			pEvent->SetInt( "positive_remaining", msg.Body().num_positive_reports_remaining() );
//			pEvent->SetInt( "negative_remaining", msg.Body().num_negative_reports_remaining() );
//			pEvent->SetInt( "positive_total", msg.Body().num_positive_reports_total() );
//			pEvent->SetInt( "negative_total", msg.Body().num_negative_reports_total() );
//			gameeventmanager->FireEventClientSide( pEvent );
//		}
//
//		return true;
//	}
//};
//GC_REG_JOB( GCSDK::CGCClient, CGCClientJobDOTAReportsRemaining, "CGCClientJobDOTAReportsRemaining", k_EMsgGCReportsRemainingResponse, k_EServerTypeGCClient );
//
////-----------------------------------------------------------------------------
//// Purpose: Handle response to k_EMsgGCWatchGame (k_EMsgGCWatchGameResponse)
////-----------------------------------------------------------------------------
//class CGCClientJobWatchGameResponse : public GCSDK::CGCClientJob
//{
//public:
//	CGCClientJobWatchGameResponse( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }
//
//	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
//	{
//		GCSDK::CProtoBufMsg<CMsgWatchGameResponse> msg( pNetPacket );
//		GTFGCClientSystem()->StartWatchingGameResponse( msg.Body() );
//		return true;
//	}
//};
//GC_REG_JOB( GCSDK::CGCClient, CGCClientJobWatchGameResponse, "CGCClientJobWatchGameResponse", k_EMsgGCWatchGameResponse, k_EServerTypeGCClient );
//
//
//#ifdef _WIN32
//#undef DECLARE_HANDLE
//#undef INVALID_HANDLE_VALUE
//#include <windows.h>
//#include <shellapi.h>
//#undef GetCurrentDirectory
//#elif POSIX
//#include <sys/syscall.h>
//#include <sys/wait.h>
//#include <signal.h>
//#endif
//
//void CTFGCClientSystem::CreateSourceTVProxy( uint32 source_tv_public_addr, uint32 source_tv_private_addr, uint32 source_tv_port )
//{
//	char szExecutablePath[MAX_PATH];
//	if ( g_pFullFileSystem->RelativePathToFullPath( "..", "EXECUTABLE_PATH", szExecutablePath, sizeof( szExecutablePath ) ) )
//	{	
//		Q_FixSlashes( szExecutablePath );
//
//		char szSrcdsProxyBinary[MAX_PATH];
//		Q_snprintf( szSrcdsProxyBinary, sizeof( szSrcdsProxyBinary ), "%s/srcds.exe", szExecutablePath );
//		Q_FixSlashes( szSrcdsProxyBinary );
//
//		netadr_t serverPublicIPAddr( source_tv_public_addr, source_tv_port );
//		netadr_t serverPrivateIPAddr( source_tv_private_addr, source_tv_port );
//
//		netadr_t *addr = NULL;
//
//		if ( serverPublicIPAddr.GetIP() && !serverPublicIPAddr.IsLocalhost() )
//		{
//			addr = &serverPublicIPAddr;
//		}
//		else if ( serverPublicIPAddr.GetIP() != serverPrivateIPAddr.GetIP() && serverPrivateIPAddr.GetIP() && !serverPrivateIPAddr.IsLocalhost() )
//		{
//			addr = &serverPrivateIPAddr;
//		}
//
//		if (!addr)
//		{
//			Msg("unable to determine address for proxy at public:%s private:%s\n", serverPublicIPAddr.ToString(), serverPrivateIPAddr.ToString() );
//			return;
//		}
//
//		CUtlString srcds_args;
//
//		srcds_args.Format( "-console -game dota +tv_relay %s", addr->ToString() );
//
//#ifdef _WIN32
//		int nRet = (int) ShellExecute( NULL, "open", szSrcdsProxyBinary, srcds_args.String(), szExecutablePath, 1 /*SW_SHOWNORMAL*/ );
//
//		if ( nRet > 0 && nRet < 32 )
//		{
//			Warning( "Failed to execute srcds proxy for public:%s private:%s\n", serverPublicIPAddr.ToString(), serverPrivateIPAddr.ToString() );
//			Warning( "  szSrcdsProxyBinary = %s\n", szSrcdsProxyBinary );
//			Warning( "  szExecutablePath = %s\n", szExecutablePath );
//			Warning( "  ShellExecute result = %d\n", nRet );
//		}
//		else
//		{
//			Msg( "Executed srcds proxy: %s args: %s dir:%s\n", szSrcdsProxyBinary, srcds_args.String(), szExecutablePath );
//		}
//#else
//		/* */
//#endif
//	}
//
//}


//-----------------------------------------------------------------------------
// Purpose: Sends an xp acknowledge via k_EMsgGC_AcknowledgeXP to the GC if
//			we have any outstanding xp sources or a mismatch in our last
//			acknowledged xp and our current one.
//-----------------------------------------------------------------------------
void CTFGCClientSystem::AcknowledgePendingXPSources( EMatchGroup eMatchGroup ) const
{
	if ( !m_pSOCache )
		return;

	const IMatchGroupDescription* pMatchDesc = GetMatchGroupDescription( eMatchGroup );
	if ( !pMatchDesc )
		return;

	bool bHasProgressToAcknowledge = false;

	// Check if we have any xp notifications to acknowledge
	CSharedObjectTypeCache *pXPTypeCache = m_pSOCache->FindBaseTypeCache( CXPSource::k_nTypeID );
	if ( pXPTypeCache && pXPTypeCache->GetCount() > 0 )
	{
		bHasProgressToAcknowledge = true;
	}

	if ( !steamapicontext || !steamapicontext->SteamUser() )
		return;

	// Check if the last acknowledged and the current xp are different.
	auto nLastAckd = pMatchDesc->m_pProgressionDesc->GetLocalPlayerLastAckdExperience();
	auto nCurrent = pMatchDesc->m_pProgressionDesc->GetPlayerExperienceBySteamID( steamapicontext->SteamUser()->GetSteamID() );

	if ( nLastAckd != nCurrent )
	{
		bHasProgressToAcknowledge = true;
	}

	if ( bHasProgressToAcknowledge )
	{
		// Send a message acknowledging the XP
		CProtoBufMsg<CMsgAcknowledgeXP> msg( k_EMsgGC_AcknowledgeXP );
		msg.Body().set_match_group( eMatchGroup );
		GCClientSystem()->BSendMessage( msg );
	}
}

void CTFGCClientSystem::AcknowledgeNotification( uint32 nAccountID, uint64 ulNotificationID ) const
{
	if ( !m_pSOCache )
		return;

	CSharedObjectTypeCache *pTypeCache = m_pSOCache->FindBaseTypeCache( CTFNotification::k_nTypeID );
	if ( pTypeCache && pTypeCache->GetCount() > 0 )
	{
		// Send a message acknowledging our notifications
		ReliableMsgNotificationAcknowledge *pReliable = new ReliableMsgNotificationAcknowledge;

		auto &msg = pReliable->Msg().Body();
		msg.set_account_id( nAccountID );
		msg.set_notification_id( ulNotificationID );

		pReliable->Enqueue();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hang onto the survey request
//-----------------------------------------------------------------------------
void CTFGCClientSystem::SetSurveyRequest( const CMsgGCSurveyRequest& msgSurveyRequest )
{
	m_msgSurveyRequest = msgSurveyRequest;
}

//-----------------------------------------------------------------------------
// Purpose: Send survey response and clear the stored survey request
//-----------------------------------------------------------------------------
void CTFGCClientSystem::SendSurveyResponse( int32 nResponse )
{
	Assert( m_msgSurveyRequest.has_question_type() );

	GCSDK::CProtoBufMsg< CMsgGCSurveyResponse > msgSurveyResponse( k_EMsgGC_SurveyQuestionResponse );
	msgSurveyResponse.Body().set_match_id( m_msgSurveyRequest.match_id() );
	msgSurveyResponse.Body().set_question_type( m_msgSurveyRequest.question_type() );
	msgSurveyResponse.Body().set_response( nResponse );
	
	if ( this->BSendMessage( msgSurveyResponse ) )
	{
		ClearSurveyRequest();
	}
}

void CTFGCClientSystem::ClearSurveyRequest()
{
	m_msgSurveyRequest.Clear();
}

void CTFGCClientSystem::CheckAssociatePartyAndSteamLobby()
{
	if ( !m_bUserWantsToBeInMatchmaking )
	{
		LeaveSteamLobby();
		return;
	}

	CTFParty *pParty = GetParty();
	if ( pParty == NULL && m_eAcceptInviteStep == eAcceptInviteStep_None )
	{
		// Left party, leave lobby now. We don't do this when we request to leave the party, because the GC may deny to
		// drop us if we raced with a match starting (See SendExitMatchmaking)
		if ( m_steamIDLobby.IsValid() )
		{
			LeaveSteamLobby();
		}
		return;
	}
	if ( steamapicontext == NULL || steamapicontext->SteamUser() == NULL || steamapicontext->SteamMatchmaking() == NULL )
		return; // WAT.  How did we create our lobby?

	// Let the leader of the party take charge
	if ( BIsPartyLeader() )
	{

		// Make sure we have a Steam lobby.  If we don't, initiate creation of one.
		int r = CheckSteamLobbyCreated();
		if ( r <= 0 )
			return; // in progress, or failed
		Assert( m_steamIDLobby.IsValid() );

		// Do we need to update the party, linking it to the steam lobby?
		if ( pParty->GetSteamLobbyID() != m_steamIDLobby )
		{
			Msg( "Sending GCUpdateParty to associate party %016llX with steam lobby %s\n", pParty->GetGroupID(), m_steamIDLobby.Render() );
			CMsgCreateOrUpdateParty *pMsg = GetCreateOrUpdatePartyMsg();
			pMsg->set_steam_lobby_id( m_steamIDLobby.ConvertToUint64() );
		}

		// Do we need to update the steam lobby, linking it to the party?
		uint64 nCurrentPartyID = 0;
		const char *pszData = steamapicontext->SteamMatchmaking()->GetLobbyData( m_steamIDLobby, k_pszSteamLobbyKey_PartyID );
		sscanf( pszData, "%llX", &nCurrentPartyID );
		if ( nCurrentPartyID != pParty->GetGroupID() )
		{
			Msg( "Setting lobby %s data to associate it with party %016llX\n", m_steamIDLobby.Render(), pParty->GetGroupID() );

			char rchValue[64];
			V_sprintf_safe( rchValue, "%016llX", pParty->GetGroupID() );
			steamapicontext->SteamMatchmaking()->SetLobbyData( m_steamIDLobby, k_pszSteamLobbyKey_PartyID, rchValue );
		}
	}
	else
	{
		// Check if we're not in the lobby associated with this party.
		if ( pParty->GetSteamLobbyID() != m_steamIDLobby )
		{

			// If we're already in a party, get out of it
			if ( m_steamIDLobby.IsValid() )
			{
				Warning( "Leaving steam lobby %s, the party is associated with lobby %s\n", m_steamIDLobby.Render(), pParty->GetSteamLobbyID().Render() );
				LeaveSteamLobby();
			}

			// If the party has a lobby, then join it
			if ( pParty->GetSteamLobbyID().IsValid() )
			{

				// OK, start joining the lobby.
				m_steamIDLobby = pParty->GetSteamLobbyID();
				Msg( "Joining lobby %s\n", m_steamIDLobby.Render() );
				steamapicontext->SteamMatchmaking()->JoinLobby( m_steamIDLobby );
			}
		}

	}
}

void CTFGCClientSystem::OnSteamLobbyCreated( LobbyCreated_t *pInfo )
{
	Assert( !m_steamIDLobby.IsValid() );
	m_eCreateLobbyStatus = pInfo->m_eResult;
	if ( m_eCreateLobbyStatus == k_EResultOK )
	{
		m_steamIDLobby.SetFromUint64( pInfo->m_ulSteamIDLobby );
		Msg( "Steam lobby %s created OK\n", m_steamIDLobby.Render() );

		// If they already bailed, then destroy the newly created lobby
		if ( !m_bUserWantsToBeInMatchmaking )
		{
			LeaveSteamLobby();
			m_eCreateLobbyStatus = k_EResultFail;
			return;
		}

		// Check if we should let the GC know what lobby to associate
		// with our party
		CheckAssociatePartyAndSteamLobby();

		// Check if now we have everything we need to active the UI
		CheckReadyToActivateInvite();
	}
	else
	{
		Warning( "FAILED to create steam lobby, error code %d\n", m_eCreateLobbyStatus );
	}
}

void CTFGCClientSystem::OnSteamGameLobbyJoinRequested( GameLobbyJoinRequested_t *pInfo )
{
	Msg( "OnSteamGameLobbyJoinRequested(%s)\n", pInfo->m_steamIDLobby.Render() );

	// Transform it into a console command and do it!
	char rchCommand[256];
	V_sprintf_safe( rchCommand, "connect_lobby %lld", pInfo->m_steamIDLobby.ConvertToUint64() );
	engine->ClientCmd_Unrestricted( rchCommand );
}

void CTFGCClientSystem::AcceptFriendInviteToJoinLobby( const CSteamID &steamIDLobby )
{
	Msg( "Disconnecting from current server to accept invite\n" );

	// Whatever matchmaking we're doing right right now, get out of it.
	EndMatchmaking();

	// Just queue it to be joined at the earliest opportunity
	Msg( "Ready to join steam lobby at next opportunity\n" );
	m_steamIDLobbyInviteAccepted = steamIDLobby;
	m_eAcceptInviteStep = eAcceptInviteStep_ReadyToJoinSteamLobby;
}

void CTFGCClientSystem::OnSteamLobbyEnter( LobbyEnter_t *pInfo )
{
	CSteamID steamIDLobby( pInfo->m_ulSteamIDLobby );

	// Are we expecting this?
	if ( m_steamIDLobby == steamIDLobby )
	{
		return;
	}
	if ( m_eAcceptInviteStep != eAcceptInviteStep_JoinSteamLobby )
	{
		Assert( m_eAcceptInviteStep == eAcceptInviteStep_None );
		Assert( BIsPartyLeader() );
		return;
	}
	m_eAcceptInviteStep = eAcceptInviteStep_GetLobbyMetadata;
	Assert( !m_steamIDLobby.IsValid() );

	// Make sure it succeeded
	if ( pInfo->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess )
	{
		Warning(" Failed to join Steam lobby with error code %d.  Cannot join party\n", pInfo->m_EChatRoomEnterResponse );
		OnFailedToAcceptInvite();
		return;
	}

	// We're in a lobby.  Remember that, in case we need to bail for some other
	// reason.
	m_steamIDLobby.SetFromUint64( pInfo->m_ulSteamIDLobby );
	Msg( "OnSteamLobbyEnter(%s)\n", m_steamIDLobby.Render() );
	Assert( m_steamIDLobby.IsLobby() );

	// Request the lobby data
	steamapicontext->SteamMatchmaking()->RequestLobbyData( m_steamIDLobby );
}

void CTFGCClientSystem::OnFailedToAcceptInvite()
{
	m_eAcceptInviteStep = eAcceptInviteStep_None;
	EndMatchmaking();

// !KLUDGE! Well, this is a bit crappy us showing a dialog box in this part of the code...

//	IGameEvent *pEvent = gameeventmanager->CreateEvent( "mm_accept_invite_fail" );
//	if ( pEvent )
//	{
//		gameeventmanager->FireEventClientSide( pEvent );
//	}

	ShowMessageBox( "#TF_Matchmaking_AcceptInviteFailTitle", "#TF_Matchmaking_AcceptInviteFailMessage", "#TF_OK" );
}

void CTFGCClientSystem::AddLocalPlayerSOListener( ISharedObjectListener* pListener, bool bImmediately )
{
	if ( bImmediately )
	{
		SubscribeToLocalPlayerSOCache( pListener );
	}
	else
	{
		m_vecDelayedLocalPlayerSOListenersToAdd.AddToTail( pListener );
	}
}

void CTFGCClientSystem::RemoveLocalPlayerSOListener( ISharedObjectListener* pListener )
{
	// Remove if it was a delayed add
	auto idx = m_vecDelayedLocalPlayerSOListenersToAdd.Find( pListener );
	if ( idx != m_vecDelayedLocalPlayerSOListenersToAdd.InvalidIndex() )
	{
		m_vecDelayedLocalPlayerSOListenersToAdd.Remove( idx );
	}

	if ( steamapicontext && steamapicontext->SteamUser() )
	{
		CSteamID steamID = steamapicontext->SteamUser()->GetSteamID();
		GetGCClient()->RemoveSOCacheListener( steamID, pListener );
	}
}


void CTFGCClientSystem::OnSteamLobbyDataUpdate( LobbyDataUpdate_t *pInfo )
{
	CSteamID steamIDLobby( pInfo->m_ulSteamIDLobby );
	Msg( "OnSteamLobbyDataUpdate(%s)\n", steamIDLobby.Render() );

	// Check if we're in the process of accepting an invite
	if ( steamIDLobby != m_steamIDLobby )
	{
		Assert( steamIDLobby == m_steamIDLobby );
		return;
	}

	if ( m_eAcceptInviteStep != eAcceptInviteStep_GetLobbyMetadata )
	{
		return;
	}

	m_eAcceptInviteStep = eAcceptInviteStep_JoinParty;

	// Fetch the party ID
	uint64 nPartyToJoin = 0;
	const char *pszData = steamapicontext->SteamMatchmaking()->GetLobbyData( m_steamIDLobby, k_pszSteamLobbyKey_PartyID );
	sscanf( pszData, "%llX", &nPartyToJoin );
	if ( nPartyToJoin == 0 )
	{
		Warning(" No %s metadata set in steam lobby, cannot join party\n", k_pszSteamLobbyKey_PartyID );
		OnFailedToAcceptInvite();
		return;
	}
	Msg( "Requesting GC add us to party %016llX\n", nPartyToJoin );

	// Join the party.
	CProtoBufMsg<CMsgAcceptInvite> msgAcceptInvite( k_EMsgGCAcceptInvite );
	msgAcceptInvite.Body().set_party_id( nPartyToJoin );
	msgAcceptInvite.Body().set_steamid_lobby( pInfo->m_ulSteamIDLobby );
	msgAcceptInvite.Body().set_client_version( engine->GetClientVersion() );
	GCClientSystem()->BSendMessage( msgAcceptInvite );
}

class CGCClientAcceptInviteResponse : public GCSDK::CGCClientJob
{
public:
	CGCClientAcceptInviteResponse( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }

	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgAcceptInviteResponse> msg( pNetPacket );

		switch ( msg.Body().result_code() )
		{
			case k_EResultOK:
			case k_EResultDuplicateRequest:
				break;

			case k_EResultInvalidProtocolVer:
				GTFGCClientSystem()->EndMatchmaking();
				ShowMessageBox( "#TF_MM_NotCurrentVersionTitle", "#TF_MM_NotCurrentVersionMessage", "#GameUI_OK" );
				break;

			default:
				Warning( "Failed to accept invite, result code %d\n", msg.Body().result_code() );
				GTFGCClientSystem()->OnFailedToAcceptInvite();
				break;
		}

		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCClientAcceptInviteResponse, "CGCClientAcceptInviteResponse", k_EMsgGCAcceptInviteResponse, k_EServerTypeGCClient );

void CTFGCClientSystem::OnSteamLobbyChatUpdate( LobbyChatUpdate_t *pInfo )
{
	if ( m_steamIDLobby.ConvertToUint64() != pInfo->m_ulSteamIDLobby || !m_steamIDLobby.IsValid() )
		return;
	CSteamID steamIDWhoChanged( pInfo->m_ulSteamIDUserChanged );
	if ( !steamIDWhoChanged.IsValid() || steamIDWhoChanged == steamapicontext->SteamUser()->GetSteamID() )
		return;

	if ( BChatMemberStateChangeRemoved( pInfo->m_rgfChatMemberStateChange ) )
	{
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "mm_lobby_member_leave" );
		if ( !pEvent )
			return;
		pEvent->SetString( "steamid", CFmtStr("%llu", steamIDWhoChanged.ConvertToUint64() ) );
		pEvent->SetInt( "flags", pInfo->m_rgfChatMemberStateChange );
		gameeventmanager->FireEventClientSide( pEvent );
	}
	else if ( pInfo->m_rgfChatMemberStateChange & k_EChatMemberStateChangeEntered )
	{
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "mm_lobby_member_join" );
		if ( !pEvent )
			return;
		pEvent->SetString( "steamid", CFmtStr("%llu", steamIDWhoChanged.ConvertToUint64() ) );
		gameeventmanager->FireEventClientSide( pEvent );
	}
}

void PostChatGameEvent( const CSteamID &steamIDPoster, CTFGCClientSystem::ELobbyMsgType eType, const char *pszText )
{
	IGameEvent *pEvent = gameeventmanager->CreateEvent( "mm_lobby_chat" );
	if ( !pEvent )
		return;
	pEvent->SetString( "steamid", CFmtStr("%llu", steamIDPoster.ConvertToUint64() ) );
	pEvent->SetString( "text", pszText );
	pEvent->SetInt( "type", eType );
	gameeventmanager->FireEventClientSide( pEvent );
}

void CTFGCClientSystem::OnSteamLobbyChatMsg( LobbyChatMsg_t *pInfo )
{
	if ( pInfo->m_eChatEntryType != k_EChatEntryTypeChatMsg )
		return;

	CSteamID steamIDPoster;
	EChatEntryType eEntryType;
	int nBufferSize = 2048;
	char *pszText = (char *)calloc( nBufferSize + 4, 1 );
	int nDataSize = steamapicontext->SteamMatchmaking()->GetLobbyChatEntry( pInfo->m_ulSteamIDLobby, pInfo->m_iChatID, &steamIDPoster, pszText, nBufferSize, &eEntryType );
	if ( nDataSize > 0 )
	{
		PostChatGameEvent( steamIDPoster, ELobbyMsgType(pszText[0]), pszText+1 );
	}

	free( pszText );
}

void CTFGCClientSystem::SendSteamLobbyChat( ELobbyMsgType eType, const char *pszText )
{
	if ( steamapicontext == NULL )
		return;

	// If we are going to send it to Steam, let's post the message as a result of the callback
	// on our own message.  That gives them some feedback if their message is not being sent.
	if ( m_steamIDLobby.IsValid() )
	{
		int sz = V_strlen(pszText)+2;
		char *temp = new char[sz];
		temp[0] = eType;
		memcpy( temp+1, pszText, sz-1 );
		steamapicontext->SteamMatchmaking()->SendLobbyChatMsg( m_steamIDLobby, temp, sz );
		delete[] temp;
	}
	else
	{

		// Not sending to Steam, just echo locally
		PostChatGameEvent( steamapicontext->SteamUser()->GetSteamID(), eType, pszText );
	}
}

void CTFGCClientSystem::CheckReadyToActivateInvite()
{

	// Don't bother, if we don't have a pending action to popup the UI
	if ( !m_bWantToActivateInviteUI )
		return;

	// What high-level state are we in?
	switch ( GetMatchmakingUIState() )
	{
		case eMatchmakingUIState_Chat:
		case eMatchmakingUIState_InQueue:
			break;

		default:
			Warning( "We missed our opportunity to active the invite UI\n" );
			m_bWantToActivateInviteUI = false;
			return;
	}

	// Make sure we have a Steam lobby.  If we don't, initiate creation of one.
	int r = CheckSteamLobbyCreated();
	if ( r == 0 )
		return; // in progress

	// Steam lobby creation finished.  No matter what, let's make this the last time we
	// do this polling work.
	m_bWantToActivateInviteUI = false;

	// Do we have a steam lobby?
	if ( r < 0 )
	{
		Warning( "Cannot active invite UI.  Failed to create Steam Lobby\n" );
	}
	else
	{
		Assert( m_steamIDLobby.IsValid() );

		// Let's invite some of these muthas
		Msg( "Activating Steam overlay to process invite to lobby %s\n", m_steamIDLobby.Render() );
		steamapicontext->SteamFriends()->ActivateGameOverlayInviteDialog( m_steamIDLobby );
	}
}

bool CTFGCClientSystem::BConnectedToMatchServer( bool bLiveMatch )
{
	// A false bLiveMatch means we don't require the lobby and the match to be currently running.
	// Meaning, you're connected to the match server, but not necessarily in the match (ie. post-match win podium)
	if ( bLiveMatch && !BHaveLiveMatch() )
	{
		return false;
	}

	// Are we in the process of connecting, or connected to, our assigned server?
	if ( m_eConnectState != eConnectState_ConnectedToMatchmade && m_eConnectState != eConnectState_ConnectingToMatchmade )
	{
		return false;
	}

	// If steamIDCurrentServer isn't set yet (it only happens late in the connect) default to assuming it's the right
	// server if it came from matchmaking.  We'll find out otherwise when we finish loading.
	if ( !bLiveMatch || !m_steamIDCurrentServer.IsValid() || m_steamIDCurrentServer == m_steamIDGCAssignedMatch )
	{
		return true;
	}

	return false;
}

bool CTFGCClientSystem::BHaveLiveMatch() const
{
	// NOTE That we don't check the lobby -- SOChanged() and Update() together decide to set or clear our assigned
	//      match, in a way that considers GC connections being fallible but the gameserver remaining authoritative.
	//      See comments there.
	return m_steamIDGCAssignedMatch.IsValid() && !m_bAssignedMatchEnded;
}

EAbandonGameStatus CTFGCClientSystem::GetAssignedMatchAbandonStatus()
{
	// Bootcamp is a magical cannot-trust-the-game-server land that never has abandons.
	//
	// TODO move this to match description as bNonTrustedMM or something.
	if ( m_eAssignedMatchGroup == k_nMatchGroup_MvM_Practice )
	{
		// Can never abandon from bootcamp
		return k_EAbandonGameStatus_Safe;
	}

	if ( BConnectedToMatchServer( true ) )
	{
		// Gameserver is authoritative if we're connected to this point.
		C_TFPlayer *pTFPlayer = C_TFPlayer::GetLocalTFPlayer();
		CTFGameRules *pTFGameRules = TFGameRules();
		if ( pTFPlayer && pTFGameRules )
		{
			bool bLiveMatch = !pTFGameRules->IsManagedMatchEnded();
			bool bPenalty = !pTFPlayer->GetMatchSafeToLeave();
			if ( !bLiveMatch )
				{ return k_EAbandonGameStatus_Safe; }
			else if ( bPenalty )
				{ return k_EAbandonGameStatus_AbandonWithPenalty; }
			else
				{ return k_EAbandonGameStatus_AbandonWithoutPenalty; }
		}
	}

	// Only fall back to looking at the lobby if we're not in the match, or not loaded enough to look at gamerules.  The
	// gameserver is the authority on MM matches, so this is just getting that data secondhand through the GC.

	// TODO(JohnS): Right now, if we have a match via the GC, assume we're required to return.  Ideally we'd pipe the
	//              MatchSafeToLeave flag through the lobby, but right now you'll be told you must return and can find
	//              out otherwise once you connect.
	if ( BHaveLiveMatch() )
		{ return k_EAbandonGameStatus_AbandonWithPenalty; }

	return k_EAbandonGameStatus_Safe;
}

EMatchGroup CTFGCClientSystem::GetLiveMatchGroup() const
{
	if ( !m_bAssignedMatchEnded )
		return m_eAssignedMatchGroup;

	return k_nMatchGroup_Invalid;
}

void CTFGCClientSystem::RejoinLobby( bool bConfirmed )
{
	// Ask to GC to Rejoin the game
	// For now try to immediately join

	// XXX(JohnS): Ideally we need to craft a BeginMatchmaking() call to put them back into the matchmaking system --
	//             right now, after a crash, you will lose your MM state and end up at the main menu after a
	//             crash-rejoin. We cannot just set m_bUserWantsToBeInMatchmaking because this skips most of the other
	//             BeginMatchmaking() setup like wizard step and results in broken UI
	if ( bConfirmed )
	{
		CTFGSLobby *pLobby = GetLobby();
		if ( pLobby )
		{
			ConnectToServer( pLobby->GetConnect() );
			return;
		}
		// Lobby disappeared
		ShowMessageBox( "#TF_MM_Rejoin_FailedTitle", "#TF_MM_Rejoin_FailedBody", "#GameUI_OK" );
		Log( "Unable to Rejoin existing Lobby game since the Lobby no longer exists." );
	}

	// Canceled or no lobby for rejoin
	EndMatchmaking( BHaveLiveMatch() ? true : false );
}

bool CTFGCClientSystem::JoinMMMatch()
{
	CTFGSLobby *pLobby = GetLobby();
	if ( pLobby )
	{
		ConnectToServer( pLobby->GetConnect() );
		return true;
	}

	return false;
}

void CTFGCClientSystem::LeaveGameAndPrepareToJoinParty( GCSDK::PlayerGroupID_t nPartyID )
{
	// Remember that we are expecting to join a party
	m_nPendingAutoJoinPartyID = nPartyID;
	m_bUserWantsToBeInMatchmaking = false;

	// Clear any other action that might be in progress
	m_bWantToActivateInviteUI = false;
	m_msgMatchmakingProgress.Clear();

	// Leave current server
	engine->ClientCmd_Unrestricted( "disconnect" );
}

bool CTFGCClientSystem::BIsPhoneVerified( void )
{
	CPlayerInventory *pLocalInv = TFInventoryManager()->GetLocalInventory();
	if ( !pLocalInv )
		return false;

	if ( !pLocalInv->GetSOC() )
		return false;

	CEconGameAccountClient *pGameAccountClient = pLocalInv->GetSOC()->GetSingleton< CEconGameAccountClient >();
	return ( pGameAccountClient && pGameAccountClient->Obj().has_phone_verified() && pGameAccountClient->Obj().phone_verified() );
}

bool CTFGCClientSystem::BIsPhoneIdentifying( void )
{
	CPlayerInventory *pLocalInv = TFInventoryManager()->GetLocalInventory();
	if ( !pLocalInv )
		return false;

	if ( !pLocalInv->GetSOC() )
		return false;

	CEconGameAccountClient *pGameAccountClient = pLocalInv->GetSOC()->GetSingleton< CEconGameAccountClient >();
	return ( pGameAccountClient && pGameAccountClient->Obj().has_phone_identifying() && pGameAccountClient->Obj().phone_identifying() );
}

bool CTFGCClientSystem::BHasCompetitiveAccess( void )
{
	CPlayerInventory *pLocalInv = TFInventoryManager()->GetLocalInventory();
	if ( !pLocalInv )
		return false;

	if ( !pLocalInv->GetSOC() )
		return false;

	CEconGameAccountClient *pGameAccountClient = pLocalInv->GetSOC()->GetSingleton< CEconGameAccountClient >();
	return ( pGameAccountClient && pGameAccountClient->Obj().has_competitive_access() && pGameAccountClient->Obj().competitive_access() );
}

class CGCClientMvMVictoryInfo : public GCSDK::CGCClientJob
{
public:
	CGCClientMvMVictoryInfo( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }

	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgMvMVictoryInfo> msg( pNetPacket );

		CTFHudMannVsMachineStatus *pMannVsMachineStatus = GET_HUDELEMENT( CTFHudMannVsMachineStatus );
		if ( pMannVsMachineStatus )
		{
			pMannVsMachineStatus->MVMVictoryGCResponse( msg.Body() );
		}
		else
		{
			Warning( "Received CMsgMvMVictoryInfo but CTFHudMannVsMachineStatus does not exist \n" );
		}
		
		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCClientMvMVictoryInfo, "CGCClientMvMVictoryInfo", k_EMsgGCMvMVictoryInfo, k_EServerTypeGCClient );


class CGCLeaveGameAndPrepareToJoinPartyJob : public GCSDK::CGCClientJob
{
public:
	CGCLeaveGameAndPrepareToJoinPartyJob( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }

	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgLeaveGameAndPrepareToJoinParty> msg( pNetPacket );
		GTFGCClientSystem()->LeaveGameAndPrepareToJoinParty( msg.Body().party_id() );
		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCLeaveGameAndPrepareToJoinPartyJob, "CGCClientMvMVictoryInfo", k_EMsgGCLeaveGameAndPrepareToJoinParty, k_EServerTypeGCClient );


//-----------------------------------------------------------------------------
// Purpose: GC Msg handler to receive the periodic world status message
//-----------------------------------------------------------------------------
class CGCWorldStatusBroadcast : public GCSDK::CGCClientJob
{
public:
	CGCWorldStatusBroadcast( GCSDK::CGCClient *pClient ) : GCSDK::CGCClientJob( pClient ) {}

	virtual bool BYieldingRunGCJob( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgTFWorldStatus> msg( pNetPacket );

		GTFGCClientSystem()->SetWorldStatus( msg.Body() );

		DevMsg( "TF world status heartbeat.\n  Event: %s\n",
			msg.Body().beta_stress_test_event_active() ? "Y" : "N" );
		return true;
	}

};

GC_REG_JOB( GCSDK::CGCClient, CGCWorldStatusBroadcast, "CGCWorldStatusBroadcast", k_EMsgGC_WorldStatusBroadcast, GCSDK::k_EServerTypeGCClient );


#if TF_ANTI_IDLEBOT_VERIFICATION

static void GenerateClientVerificationMD5ForItemList( MD5Context_t& out_md5Context, const CUtlVector<const CEconItemView *>& vecItems, bool bIsVerbose = false )
{
	FOR_EACH_VEC( vecItems, i )
	{
		const CEconItemView *pEconItemView = vecItems[i];
		Assert( pEconItemView );

		CEconItemDescription desc;
		desc.SetHashContext( &out_md5Context );
		desc.SetVerbose( bIsVerbose );
		IEconItemDescription::YieldingFillOutEconItemDescription( &desc, GLocalizationProvider(), pEconItemView );
	}
}

#ifdef DEBUG
CON_COMMAND_F( tf_generate_client_verification, "<itemID0> ... <itemIDn>", FCVAR_CLIENTDLL )
{
	CUtlVector<const CEconItemView *> vecItems;
	for ( int i = 1; i < args.ArgC(); i++ )
	{
#ifdef POSIX
		const itemid_t unItemId = static_cast<itemid_t >( atoll( args[i] ) );
#else
		const itemid_t unItemId = static_cast<itemid_t >( _atoi64( args[i] ) );
#endif // LINUX
		const CEconItemView *pEconItemView = TFInventoryManager()->GetLocalTFInventory()->GetInventoryItemByItemID( unItemId );
		if ( !pEconItemView )
		{
			Msg( "Unable to find item id %llu.\n", unItemId );
			return;
		}

		vecItems.AddToTail( pEconItemView );
	}

	MD5Context_t md5Context;
	MD5Init( &md5Context );
	GenerateClientVerificationMD5ForItemList( md5Context, vecItems );
}
#endif // DEBUG

class CGCClientHelloResponse : public GCSDK::CGCClientJob
{
public:
	CGCClientHelloResponse( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }

	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CGCMsgTFHelloResponse> msg( pNetPacket );

		// Don't send a response if we don't have our inventory from Steam yet. The GC will send down another challenge in
		// a few seconds.
		if ( !TFInventoryManager()->GetLocalTFInventory()->RetrievedInventoryFromSteam() )
			return true;
		
#ifdef DBEUG
		{
			Msg( "Beginning response to client verification challenge:\n" );
		}
#endif // DEBUG

		CUtlVector<const CEconItemView *> vecItems;
		for ( int i = 0; i < msg.Body().version_checksum_size(); i++ )
		{
			const itemid_t unItemId = msg.Body().version_checksum(i);
			const CEconItemView *pEconItemView = TFInventoryManager()->GetLocalTFInventory()->GetInventoryItemByItemID( unItemId );
			if ( !pEconItemView )
			{
				// The GC thought we had this item but we don't agree. It should be impossible to mess with the session on the
				// GC to add/remove items from the SO cache while we've got the lock we use to build the challenge, but we might
				// have traded away an item *after* that. For this, we ignore the challenge and hope the GC sends us down another
				// one with our new current inventory.
				return true;
			}

			vecItems.AddToTail( pEconItemView );
		}

		bool bIsVerbose = false;
		if ( msg.Body().has_version_verbose() )
		{
			bIsVerbose = msg.Body().version_verbose();
		}

		MD5Context_t md5Context;
		MD5Init( &md5Context );
		GenerateClientVerificationMD5ForItemList( md5Context, vecItems, bIsVerbose );
		GCSDK::CProtoBufMsg<CGCMsgTFSync> msgResponse( k_EMsgGC_ClientVerificationChallengeResponse );

		MD5Context_t md5ContextEx = md5Context;
		MD5Value_t md5ResultEx;
		MD5Final( &md5ResultEx.bits[0], &md5ContextEx );
		msgResponse.Body().set_version_checksum_ex( &md5ResultEx.bits[0], MD5_DIGEST_LENGTH );

		const unsigned int unRandomSeed = msg.Body().version_check();

		int key;
		if ( (*((bool *)g_pClientPurchaseInterface - 156) ) )
		{
			key = kTFDescriptionHash_TextmodeArbitraryKey;
		}
		else
		{
			int iInstanceCount = engine->GetInstancesRunningCount();
			key = iInstanceCount > 1 ? kTFDescriptionHash_MultiRunArbitraryKey : kTFDescriptionHash_ValidArbitraryKey;
		}

		const unsigned int unMungedRandomSeed = unRandomSeed + key;
		TFDescription_HashDataMunge( &md5Context, unMungedRandomSeed, bIsVerbose, VarArgs( "%d", unMungedRandomSeed ) );

		MD5Value_t md5Result;
		MD5Final( &md5Result.bits[0], &md5Context );

		msgResponse.Body().set_version_checksum( &md5Result.bits[0], MD5_DIGEST_LENGTH );

		// What language are we running in? We need to send this up so the GC will localize with the same strings we're using.
		char uilanguage[ 64 ];
		uilanguage[0] = 0;
		engine->GetUILanguage( uilanguage, sizeof( uilanguage ) );
		msgResponse.Body().set_version_check( PchLanguageToELanguage( uilanguage ) );

		// Send back up the challenge identifier to maintain sync.
		msgResponse.Body().set_version_check_ex( unRandomSeed ^ kTFDescriptionHash_ChallengeXorShenanigans );

		GCClientSystem()->BSendMessage( msgResponse );

		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCClientHelloResponse, "CGCClientHelloResponse", k_EMsgGC_ClientVerificationChallenge, k_EServerTypeGCClient );

#endif // TF_ANTI_IDLEBOT_VERIFICATION

#ifdef TF_GC_PING_DEBUG
// Ping debug commands for spoofin'
CON_COMMAND( tf_datacenter_ping_override, "Override the ping data we'll report for a specific datacenter." )
{
	if ( args.ArgC() != 4 )
	{
		ConMsg( "Usage: tf_datacenter_ping_override <datacenter> <ping> <status>\n" );
		return;
	}

	const char *pszDC = args[1];
	uint32 nPing = (uint32)Clamp( V_atoi( args[2] ), 0, INT_MAX );
	CMsgGCDataCenterPing_Update_Status eStatus = (CMsgGCDataCenterPing_Update_Status)V_atoi( args[3] );
	GTFGCClientSystem()->SetPingOverride( pszDC, nPing, eStatus );

	ConMsg( "Started overriding datacenter \"%s\" to %ums ping with status %i (enum)\n"
	        "Forcing a ping refresh to submit new data with this override\n",
	        pszDC, nPing, eStatus );
}

CON_COMMAND( tf_datacenter_clear_ping_override, "Stop overriding ping data." )
{
	if ( args.ArgC() != 1 )
	{
		ConMsg( "Usage: tf_datacenter_clear_ping_override\n" );
		return;
	}

	GTFGCClientSystem()->ClearPingOverrides();

	ConMsg( "Stopped overriding any datacenter ping data\n" );
}
#endif
