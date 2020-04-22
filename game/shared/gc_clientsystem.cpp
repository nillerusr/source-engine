//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//
//=============================================================================
#include "cbase.h"
#include "gc_clientsystem.h"
#include "econ_item_system.h"
#include "econ_item_inventory.h"
#include "quest_objective_manager.h"
#ifdef GAME_DLL
#include "tf_wartracker.h"
#endif
//#include "gcsdk/msgprotobuf.h"

//
// TODO: NO_STEAM support!
//

using namespace GCSDK;

// Retry for sending a ClientHello if we don't hear back from the GC.  Generally this shouldn't be necessary, as the GC
// should be aware of our session via the GCH and should not need us to pester it.
const float k_flClientHelloRetry = 30.f;

// Client GC System.
//static CGCClientSystem s_CGCClientSystem;
static CGCClientSystem* s_pCGCGameSpecificClientSystem = NULL; // set this in the game specific derived version if needed
void SetGCClientSystem( CGCClientSystem* pGCClientSystem )
{
	s_pCGCGameSpecificClientSystem = pGCClientSystem;
}

CGCClientSystem *GCClientSystem()
{
	AssertMsg( s_pCGCGameSpecificClientSystem, "GCClientSystem is not initialized in the game specific client system constructor" );
	return s_pCGCGameSpecificClientSystem;
}

#ifdef STAGING_ONLY
CON_COMMAND( dump_cache, "Dump the contents of a user's SOCache" )
{
	if( args.ArgC() < 2 )
	{
		Msg( "Usage: dump_cache <steamID>\n" );
		return;
	}

	CSteamID userSteamID( V_atoui64( args[1] ), 1, GetUniverse(), k_EAccountTypeIndividual );

	CGCClientSharedObjectCache* pSOCache = GCClientSystem()->GetSOCache( userSteamID );
	if ( pSOCache )
	{
		pSOCache->Dump();
	}
}
#endif

#ifdef GAME_DLL
CON_COMMAND( dump_all_caches, "Dump the contents all subsribed SOCaches" )
{
	GCClientSystem()->GetGCClient()->Dump();
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CGCClientSystem::CGCClientSystem()
: CAutoGameSystemPerFrame( "CGCClientSystem" )
#ifdef CLIENT_DLL
	, m_GCClient( NULL, false )
#else
	, m_GCClient( NULL, true )
	, m_CallbackLogonSuccess( this, &CGCClientSystem::OnLogonSuccess )
#endif
{
	m_bInittedGC = false;
	m_bConnectedToGC = false;
	m_bLoggedOn = false;
	m_timeLastSendHello = 0.0;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CGCClientSystem::~CGCClientSystem()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
GCSDK::CGCClient *CGCClientSystem::GetGCClient()
{
	Assert ( this != NULL );
	return &m_GCClient;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CGCClientSystem::BSendMessage( uint32 unMsgType, const uint8 *pubData, uint32 cubData )
{
	return m_GCClient.BSendMessage( unMsgType, pubData, cubData );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CGCClientSystem::BSendMessage( const GCSDK::CGCMsgBase& msg )									
{ 
	return m_GCClient.BSendMessage( msg ); 
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CGCClientSystem::BSendMessage( const GCSDK::CProtoBufMsgBase& msg )									
{ 
	return m_GCClient.BSendMessage( msg ); 
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
GCSDK::CGCClientSharedObjectCache *CGCClientSystem::GetSOCache( const CSteamID &steamID )
{
	return m_GCClient.FindSOCache( steamID, false );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
GCSDK::CGCClientSharedObjectCache *CGCClientSystem::FindOrAddSOCache( const CSteamID &steamID )
{
	return m_GCClient.FindSOCache( steamID, true );
}



//-----------------------------------------------------------------------------
void CGCClientSystem::PostInit()
{
	// Call into the BaseClass.
	CAutoGameSystemPerFrame::PostInit();

	#ifdef CLIENT_DLL
		// Install callback to be notified when our steam logged on status changes.
		ClientSteamContext().InstallCallback( UtlMakeDelegate( this, &CGCClientSystem::SteamLoggedOnCallback ) );

		// Except when debugging internally, we really should never launch the game
		// while not logged on!
		AssertMsg( ClientSteamContext().BLoggedOn(), "No Steam logged on for GC setup!" );

		ThinkConnection();
	#endif
}

//-----------------------------------------------------------------------------
#ifndef CLIENT_DLL
void CGCClientSystem::GameServerActivate()
{
	ThinkConnection();
}
#endif


//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
void CGCClientSystem::SteamLoggedOnCallback( const SteamLoggedOnChange_t &loggedOnState )
{
	ThinkConnection();
}

#else

//-----------------------------------------------------------------------------
void CGCClientSystem::OnLogonSuccess( SteamServersConnected_t *pLogonSuccess )
{
	ThinkConnection();
}

#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGCClientSystem::LevelInitPreEntity()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGCClientSystem::LevelShutdownPostEntity()
{
#ifdef GAME_DLL
	QuestObjectiveManager()->Shutdown();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGCClientSystem::Shutdown()
{
	// Shutdown the GC.
	m_GCClient.Uninit();

	// Reset the init flag.
	m_bInittedGC = false;
	m_bConnectedToGC = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGCClientSystem::SetupGC()
{
	// Pre-Init.
	PreInitGC();
	InventoryManager()->PreInitGC();

	// Init.
	InitGC();

	// Post-Init.
	PostInitGC();
	InventoryManager()->PostInitGC();
	QuestObjectiveManager()->Initialize();

#ifdef GAME_DLL
	GetWarTrackerManager()->Initialize();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGCClientSystem::InitGC()
{
	// Check to see if we have already initialized the GCClient.
	if ( m_bInittedGC )
		return;

	// Locate our steam client interface.
	#ifdef CLIENT_DLL
		ISteamClient *pSteamClient = SteamClient();
		HSteamUser hSteamUser = SteamAPI_GetHSteamUser();
		HSteamPipe hSteamPipe = SteamAPI_GetHSteamPipe();
	#else
		ISteamClient *pSteamClient = g_pSteamClientGameServer;
		HSteamUser hSteamUser = SteamGameServer_GetHSteamUser();
		HSteamPipe hSteamPipe = SteamGameServer_GetHSteamPipe();
	#endif
	if ( pSteamClient == NULL )
	{
		Warning( "CGCClientSystem - no ISteamClient interface!\n" );
		Assert( pSteamClient );
		return;
	}

	// Get the SteamGameCoordinator and initialize the GCClient
	void *pGenericInterface =  pSteamClient->GetISteamGenericInterface( hSteamUser, hSteamPipe, STEAMGAMECOORDINATOR_INTERFACE_VERSION );

	ISteamGameCoordinator *pGameCoordinator = static_cast<ISteamGameCoordinator*>( pGenericInterface );
	if ( pGameCoordinator )
	{
		m_GCClient.BInit( pGameCoordinator );

		// Initialized the GCClient
		m_bInittedGC = true;
	}
}


//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
void CGCClientSystem::Update( float frametime )
{
	ThinkConnection();
	if ( m_bInittedGC )
		m_GCClient.BMainLoop( k_nThousand, (uint64)( frametime * 1000000.0f ) );
}
#else
void CGCClientSystem::PreClientUpdate()	
{ 
	ThinkConnection();
	if ( m_bInittedGC )
		m_GCClient.BMainLoop( k_nThousand, ( uint64 )( gpGlobals->frametime * 1000000.0f ) ); 	
}
#endif

//-----------------------------------------------------------------------------
void CGCClientSystem::ThinkConnection()
{

	// Currently logged on?
	#ifdef CLIENT_DLL	
		bool bLoggedOn = ClientSteamContext().BLoggedOn();
	#else
		bool bLoggedOn = steamgameserverapicontext && steamgameserverapicontext->SteamGameServer() && steamgameserverapicontext->SteamGameServer()->BLoggedOn();
	#endif
	if ( bLoggedOn )
	{

		// We're logged on.  Is this a rising edge?
		if ( !m_bLoggedOn )
		{

			// Re-init logon
			m_bLoggedOn = true;

			// server should automatically send us a HELLO pretty quickly after it detects us logon.  Give it some time to send
			m_timeLastSendHello = Plat_FloatTime() + 2.0f;
			Assert( !m_bConnectedToGC );
			SetupGC();
		}

		// Check if we need to send a HELLO message to re-sync connection with the GC
		Assert( m_bInittedGC );
		if ( !m_bConnectedToGC )
		{
			if ( m_timeLastSendHello < Plat_FloatTime() - k_flClientHelloRetry )
			{
				m_timeLastSendHello = Plat_FloatTime();

				#ifdef CLIENT_DLL
					GCSDK::CProtoBufMsg<CMsgClientHello> msg( k_EMsgGCClientHello );
					msg.Body().set_version( engine->GetClientVersion() );

					//CGCClientSharedObjectCache *pSOCache = m_GCClient.FindSOCache( ClientSteamContext().GetLocalPlayerSteamID(), false );
					//if ( pSOCache != NULL && pSOCache->BIsInitialized() )
					//{
					//	msg.Body().set_socache_version( pSOCache->GetVersion() );
					//}

				#else
					GCSDK::CProtoBufMsg<CMsgServerHello> msg( k_EMsgGCServerHello );
					msg.Body().set_version( engine->GetServerVersion() );

					//CGCClientSharedObjectCache *pSOCache = m_GCClient.FindSOCache( steamgameserverapicontext->SteamGameServer()->GetSteamID(), false );
					//if ( pSOCache != NULL && pSOCache->BIsInitialized() )
					//{
					//	msg.Body().set_socache_version( pSOCache->GetVersion() );
					//}
				#endif
				BSendMessage( msg );
			}
		}

	}
	else
	{

		// We're not logged on.  Clear all connection state flags
		m_bLoggedOn = false;
		m_bConnectedToGC = false;
		m_timeLastSendHello = -999.9;
	}
}

#ifdef CLIENT_DLL

void CGCClientSystem::ReceivedClientWelcome( const CMsgClientWelcome &msg )
{
	m_bConnectedToGC = true;
	Msg( "Connection to game coordinator established.\n" );
	
	IGameEvent *pEvent = gameeventmanager->CreateEvent( "gc_new_session" );
	if ( pEvent )
	{
		gameeventmanager->FireEventClientSide( pEvent );
	}

//	GTFGCClientSystem()->UpdateGCServerInfo();
//
//	// Validate version
//	int engineServerVersion = engine->GetServerVersion();
//	g_gcServerVersion = (int)msg.Body().version();
//
//	// Version checking is enforced if both sides do not report zero as their version
//	if ( engineServerVersion && g_gcServerVersion && engineServerVersion != g_gcServerVersion )
//	{
//		// If we're out of date exit
//		Msg("Version out of date (GC wants %d, we are %d)!\n", g_gcServerVersion, engine->GetServerVersion() );
//
//		// If we hibernating, quit now, otherwise we will quit on hibernation
//		if ( g_ServerGameDLL.m_bIsHibernating )
//		{
//			engine->ServerCommand( "quit\n" );
//		}
//	}
//	else
//	{
//		Msg("GC Connection established for server version %d\n", engine->GetServerVersion() );
//	}

}

class CGCClientJobClientWelcome : public GCSDK::CGCClientJob
{
public:
	CGCClientJobClientWelcome( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }

	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
	{
		CProtoBufMsg<CMsgClientWelcome> msg( pNetPacket );
		GCClientSystem()->ReceivedClientWelcome( msg.Body() );
		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCClientJobClientWelcome, "CGCClientJobClientWelcome", k_EMsgGCClientWelcome, k_EServerTypeGCClient );

void CGCClientSystem::ReceivedClientGoodbye( const CMsgClientGoodbye &msg )
{
	switch ( msg.reason() )
	{
		case GCGoodbyeReason_GC_GOING_DOWN:
			Warning( "The item server is shutting down. Items will be unavailable temporarily.\n" );
			break;

		case GCGoodbyeReason_NO_SESSION:
			if ( m_bConnectedToGC )
			{
				Warning( "The connection to the item server has been interrupted.  Attempting to re-negotiate connection now.\n" );
			}
			break;

		default:
			Warning( "Received goodbye message from the item server with unknown reason code %d.\n", (int)msg.reason() );
			break;
	}

	m_bConnectedToGC = false;
}

class CGCClientJobClientGoodbye : public GCSDK::CGCClientJob
{
public:
	CGCClientJobClientGoodbye( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }

	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
	{
		CProtoBufMsg<CMsgClientGoodbye> msg( pNetPacket );
		GCClientSystem()->ReceivedClientGoodbye( msg.Body() );
		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCClientJobClientGoodbye, "CGCClientJobClientGoodbye", k_EMsgGCClientGoodbye, k_EServerTypeGCClient );

#else

void CGCClientSystem::ReceivedServerWelcome( const CMsgServerWelcome &msg )
{
	if ( !m_bConnectedToGC )
	{
		m_bConnectedToGC = true;
		Msg( "Connection to game coordinator established.\n" );
	}

//	GTFGCClientSystem()->UpdateGCServerInfo();
//
//	// Validate version
//	int engineServerVersion = engine->GetServerVersion();
//	g_gcServerVersion = (int)msg.Body().version();
//
//	// Version checking is enforced if both sides do not report zero as their version
//	if ( engineServerVersion && g_gcServerVersion && engineServerVersion != g_gcServerVersion )
//	{
//		// If we're out of date exit
//		Msg("Version out of date (GC wants %d, we are %d)!\n", g_gcServerVersion, engine->GetServerVersion() );
//
//		// If we hibernating, quit now, otherwise we will quit on hibernation
//		if ( g_ServerGameDLL.m_bIsHibernating )
//		{
//			engine->ServerCommand( "quit\n" );
//		}
//	}
//	else
//	{
//		Msg("GC Connection established for server version %d\n", engine->GetServerVersion() );
//	}

}

class CGCClientJobServerWelcome : public GCSDK::CGCClientJob
{
public:
	CGCClientJobServerWelcome( GCSDK::CGCClient *pGCServer ) : GCSDK::CGCClientJob( pGCServer ) { }

	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
	{
		CProtoBufMsg<CMsgServerWelcome> msg( pNetPacket );
		GCClientSystem()->ReceivedServerWelcome( msg.Body() );
		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCClientJobServerWelcome, "CGCClientJobServerWelcome", k_EMsgGCServerWelcome, k_EServerTypeGCClient );

void CGCClientSystem::ReceivedServerGoodbye( const CMsgServerGoodbye &msg )
{
	switch ( msg.reason() )
	{
		case GCGoodbyeReason_GC_GOING_DOWN:
			Warning( "The item server is shutting down. Items will be unavailable temporarily.\n" );
			break;

		case GCGoodbyeReason_NO_SESSION:
			if ( m_bConnectedToGC )
			{
				Warning( "The connection to the game coordinator has been interrupted.  Attempting to re-negotiate connection now.\n" );
			}
			break;

		default:
			Warning( "Received goodbye message from game coordinator with unknown reason code %d.\n", (int)msg.reason() );
			break;
	}

	m_bConnectedToGC = false;
}

class CGCClientJobServerGoodbye : public GCSDK::CGCClientJob
{
public:
	CGCClientJobServerGoodbye( GCSDK::CGCClient *pGCClient ) : GCSDK::CGCClientJob( pGCClient ) { }

	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
	{
		CProtoBufMsg<CMsgServerGoodbye> msg( pNetPacket );
		GCClientSystem()->ReceivedServerGoodbye( msg.Body() );
		return true;
	}
};
GC_REG_JOB( GCSDK::CGCClient, CGCClientJobServerGoodbye, "CGCClientJobServerGoodbye", k_EMsgGCServerGoodbye, k_EServerTypeGCClient );

#endif // #ifdef CLIENT_DLL, #else

//-----------------------------------------------------------------------------
