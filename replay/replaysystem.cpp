//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "replaysystem.h"
#include "tier2/tier2.h"
#include "iserver.h"
#include "iclient.h"
#include "icliententitylist.h"
#include "igameevents.h"
#include "replay/ireplaymovierenderer.h"
#include "replay/ireplayscreenshotsystem.h"
#include "replay/replayutils.h"
#include "replay/replaylib.h"
#include "sv_sessionrecorder.h"
#include "sv_recordingsession.h"
#include "cl_screenshotmanager.h"
#include "netmessages.h"
#include "thinkmanager.h"
#include "managertest.h"
#include "vprof.h"
#include "sv_fileservercleanup.h"

#if !defined( _X360 )
#include "winlite.h"
#include "xbox/xboxstubs.h"
#endif

// TODO: Deal with linux build includes
#ifdef IS_WINDOWS_PC
#include <winsock.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

#undef CreateEvent	// Can't call IGameEventManager2::CreateEvent() without this

//----------------------------------------------------------------------------------------

#if !defined( DEDICATED )
IEngineClientReplay			*g_pEngineClient = NULL;
CClientReplayContext		*g_pClientReplayContextInternal = NULL;
IVDebugOverlay				*g_pDebugOverlay = NULL;
IDownloadSystem				*g_pDownloadSystem = NULL;
#endif

vgui::ILocalize				*g_pVGuiLocalize = NULL;
CServerReplayContext		*g_pServerReplayContext = NULL;
IClientReplay				*g_pClient = NULL;
IServerReplay				*g_pServer = NULL;
IEngineReplay				*g_pEngine = NULL;
IGameEventManager2			*g_pGameEventManager = NULL;
IEngineTrace				*g_pEngineTraceClient = NULL;
IReplayDemoPlayer			*g_pReplayDemoPlayer = NULL;
IClientEntityList			*entitylist = NULL;		// icliententitylist.h forces the use of this name by externing in the header

//----------------------------------------------------------------------------------------

#define REPLAY_INIT( exp_ ) \
	if ( !( exp_ ) ) \
	{ \
		Warning( "CReplaySystem::Connect() failed on: \"%s\"!\n", #exp_ ); \
		return false; \
	}

//----------------------------------------------------------------------------------------

class CReplaySystem : public CTier2AppSystem< IReplaySystem >
{
	typedef CTier2AppSystem< IReplaySystem > BaseClass;

public:
	virtual bool Connect( CreateInterfaceFn fnFactory )
	{
		REPLAY_INIT( fnFactory );
		REPLAY_INIT( BaseClass::Connect( fnFactory ) );

		ConVar_Register( FCVAR_CLIENTDLL );

		REPLAY_INIT( g_pFullFileSystem );

		g_pEngine = (IEngineReplay *)fnFactory( ENGINE_REPLAY_INTERFACE_VERSION, NULL );
		REPLAY_INIT( g_pEngine );

		REPLAY_INIT( g_pEngine->IsSupportedModAndPlatform() );
		
#if !defined( DEDICATED )
		g_pEngineClient = (IEngineClientReplay *)fnFactory( ENGINE_REPLAY_CLIENT_INTERFACE_VERSION, NULL );
		REPLAY_INIT( g_pEngineClient );

		g_pEngineTraceClient = (IEngineTrace *)fnFactory( INTERFACEVERSION_ENGINETRACE_CLIENT, NULL );
		REPLAY_INIT( g_pEngineTraceClient );

		g_pReplayDemoPlayer = (IReplayDemoPlayer *)fnFactory( INTERFACEVERSION_REPLAYDEMOPLAYER, NULL );
		REPLAY_INIT( g_pReplayDemoPlayer );

		g_pVGuiLocalize = (vgui::ILocalize *)fnFactory( VGUI_LOCALIZE_INTERFACE_VERSION, NULL );
		REPLAY_INIT( g_pVGuiLocalize );

		g_pDebugOverlay = ( IVDebugOverlay * )fnFactory( VDEBUG_OVERLAY_INTERFACE_VERSION, NULL );
		REPLAY_INIT( g_pDebugOverlay );
#endif

		g_pGameEventManager = (IGameEventManager2 *)fnFactory( INTERFACEVERSION_GAMEEVENTSMANAGER2, NULL );
		REPLAY_INIT( g_pGameEventManager );

#if !defined( DEDICATED )
		g_pDownloadSystem = (IDownloadSystem *)fnFactory( INTERFACEVERSION_DOWNLOADSYSTEM, NULL );
		REPLAY_INIT( g_pDownloadSystem );

		// Create client context now if not running a dedicated server
		if ( !g_pEngine->IsDedicated() )
		{
			g_pClientReplayContextInternal = new CClientReplayContext();
		}

		// ...and create server replay context if we are
		else
#endif
		{
			g_pServerReplayContext = new CServerReplayContext();
		}

#if defined( DEDICATED )
		REPLAY_INIT( ReplayLib_Init( g_pEngine->GetGameDir(), NULL ) )	// Init without the client replay context
#else
		REPLAY_INIT( ReplayLib_Init( g_pEngine->GetGameDir(), g_pClientReplayContextInternal ) );
#endif

		Test();

		return true;
	}

	virtual void Disconnect()
	{
		BaseClass::Disconnect(); 
	}

	virtual InitReturnVal_t Init()
	{
		InitReturnVal_t nRetVal = BaseClass::Init();
		if ( nRetVal != INIT_OK )
			return nRetVal;

		return INIT_OK;
	}

	virtual void Shutdown()
	{
		BaseClass::Shutdown();

#if !defined( DEDICATED )
		delete g_pClientReplayContextInternal;
		g_pClientReplayContextInternal = NULL;
#endif

		delete g_pServerReplayContext;
		g_pServerReplayContext = NULL;
	}

	virtual void Think()
	{
		VPROF_BUDGET( "CReplaySystem::Think", VPROF_BUDGETGROUP_REPLAY );

		g_pThinkManager->Think();
	}

	virtual bool IsReplayEnabled()
	{
		extern ConVar replay_enable;
		return replay_enable.GetInt() != 0;
	}

	virtual bool IsRecording()
	{
		// NOTE: demoplayer->IsPlayingBack() needs to be checked here, as "replay_enable" and
		// "replay_recording" will inevitably get stored with signon data in any playing demo.
		// If the !demoplayer->IsPlayingBack() line is omitted below, Replay_IsRecording()
		// becomes useless during demo playback and will always return true.
		extern ConVar replay_recording;
#if !defined( DEDICATED )
		return IsReplayEnabled() &&
			   replay_recording.GetInt() &&
			   !g_pEngineClient->IsDemoPlayingBack();
#else
		return IsReplayEnabled() &&
			   replay_recording.GetInt();
#endif
	}

	//----------------------------------------------------------------------------------------
	// Client-specific implementation:
	//----------------------------------------------------------------------------------------
	
	virtual bool CL_Init( CreateInterfaceFn fnClientFactory )
	{
#if !defined( DEDICATED )
		g_pClient = (IClientReplay *)fnClientFactory( CLIENT_REPLAY_INTERFACE_VERSION, NULL );
		if ( !g_pClient )
			return false;

		entitylist = (IClientEntityList *)fnClientFactory( VCLIENTENTITYLIST_INTERFACE_VERSION, NULL );
		if ( !entitylist )
			return false;

		if ( !g_pClientReplayContextInternal->Init( fnClientFactory ) )
			return false;

		return true;
#else
		return false;
#endif
	}

	virtual void CL_Shutdown()
	{
#if !defined( DEDICATED )
		if ( g_pClientReplayContextInternal && g_pClientReplayContextInternal->IsInitialized() )
		{
			g_pClientReplayContextInternal->Shutdown();
		}
#endif
	}

	virtual void CL_Render()
	{
#if !defined( DEDICATED )
		// If the replay system wants to take a screenshot, do it now
		if ( g_pClientReplayContextInternal->m_pScreenshotManager->ShouldCaptureScreenshot() )
		{
			g_pClientReplayContextInternal->m_pScreenshotManager->DoCaptureScreenshot();
			return;
		}

		// Currently rendering?  NOTE: GetMovieRenderer() only returns a valid ptr during rendering
		IReplayMovieRenderer *pReplayMovieRenderer = g_pClientReplayContextInternal->GetMovieRenderer();
		if ( !pReplayMovieRenderer )
			return;

		pReplayMovieRenderer->RenderVideo();
#endif
	}

	virtual IClientReplayContext *CL_GetContext()
	{
#if !defined( DEDICATED )
		return g_pClientReplayContextInternal;
#else
		return NULL;
#endif
	}

	//----------------------------------------------------------------------------------------
	// Server-specific implementation:
	//----------------------------------------------------------------------------------------
	
	virtual bool SV_Init( CreateInterfaceFn fnServerFactory )
	{
		if ( !g_pEngine->IsDedicated() || !g_pServerReplayContext )
			return false;

		g_pServer = (IServerReplay *)fnServerFactory( SERVER_REPLAY_INTERFACE_VERSION, NULL );
		if ( !g_pServer )
			return false;

		Assert( !ReplayServer() );

		return g_pServerReplayContext->Init( fnServerFactory );
	}

	virtual void SV_Shutdown()
	{
		if ( g_pServerReplayContext && g_pServerReplayContext->IsInitialized() )
		{
			g_pServerReplayContext->Shutdown();
		}
	}

	virtual IServerReplayContext *SV_GetContext()
	{
		return g_pServerReplayContext;
	}

	virtual bool SV_ShouldBeginRecording( bool bIsInWaitingForPlayers )
	{
		extern ConVar replay_enable;

		return !bIsInWaitingForPlayers &&
#if !defined( DEDICATED )
			   !g_pEngineClient->IsPlayingReplayDemo() &&
#endif
			   replay_enable.GetBool();
	}

	virtual void SV_NotifyReplayRequested()
	{
		if ( !g_pEngine->IsSupportedModAndPlatform() )
			return;

		CServerRecordingSession *pSession = SV_GetRecordingSessionInProgress();
		if ( !pSession )
			return;

		// A replay was requested - notify the session so we don't throw it away at the end of the round
		pSession->NotifyReplayRequested();
	}

	virtual void SV_SendReplayEvent( const char *pEventName, int nClientSlot )
	{
		// Attempt to create the event
		IGameEvent *pEvent = g_pGameEventManager->CreateEvent( pEventName, true );
		if ( !pEvent )
			return;

		SV_SendReplayEvent( pEvent, nClientSlot );
	}

	virtual void SV_SendReplayEvent( IGameEvent *pEvent, int nClientSlot/*=-1*/ )
	{
		IServer *pGameServer = g_pEngine->GetGameServer();

		if ( !pEvent )
			return;

		// Write event info to SVC_GameEvent msg
		char buffer_data[MAX_EVENT_BYTES];
		SVC_GameEvent msg;
		msg.SetReliable( false );
		msg.m_DataOut.StartWriting( buffer_data, sizeof( buffer_data ) );
		if ( !g_pGameEventManager->SerializeEvent( pEvent, &msg.m_DataOut ) )
		{
			DevMsg( "Replay_SendReplayEvent(): failed to serialize event '%s'.\n", pEvent->GetName() );
			goto free_event;
		}

		// Send to all clients?
		if ( nClientSlot == -1 )
		{
			for ( int i = 0; i < pGameServer->GetClientCount(); ++i )
			{
				IClient *pClient = pGameServer->GetClient( i );
				if ( pClient )
				{
					// Send the message
					pClient->SendNetMsg( msg );
				}
			}
		}
		else	// Send to just the one client?
		{
			IClient *pClient = pGameServer->GetClient( nClientSlot );
			if ( pClient )
			{
				// Send the message
				pClient->SendNetMsg( msg );
			}
		}

	free_event:
		g_pGameEventManager->FreeEvent( pEvent );
	}

	virtual void SV_EndRecordingSession( bool bForceSynchronousPublish/*=false*/ )
	{
		if ( !g_pEngine->IsSupportedModAndPlatform() )
			return;

		if ( !ReplayServer() )
			return;

		SV_GetSessionRecorder()->StopRecording( false );

		if ( bForceSynchronousPublish )
		{
			// Publish all files
			SV_GetSessionRecorder()->PublishAllSynchronous();

			// This should unlock all sessions
			SV_GetSessionRecorder()->UpdateSessionLocks();

			// Let the session manager do any cleanup - this will remove files associated with a ditched
			// session.  For example, if the server was shut down in the middle of a round where no one
			// saved a replay, the files will be published synchronously above and then cleaned up
			// synchronously here.
			SV_GetRecordingSessionManager()->DoSessionCleanup();

			// Since the recording session manager will kick off a cleanup job, we need to wait for it
			// here since we're shutting down.
			SV_GetFileserverCleaner()->BlockForCompletion();
		}
	}

	void Test()
	{
#if !defined( DEDICATED ) && _DEBUG
		// This gets called after interfaces are hooked up, and before any of the 
		// internal replay systems get init'd.
		CTestManager::Test();
#endif
	}
};

//----------------------------------------------------------------------------------------

static CReplaySystem s_Replay;
IReplaySystem *g_pReplay = &s_Replay;

//----------------------------------------------------------------------------------------

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CReplaySystem, IReplaySystem, REPLAY_INTERFACE_VERSION,
								   s_Replay );

//----------------------------------------------------------------------------------------

void Replay_MsgBox( const char *pText )
{
	g_pClient->DisplayReplayMessage( pText, false, true, NULL );
}

void Replay_MsgBox( const wchar_t *pText )
{
	g_pClient->DisplayReplayMessage( pText, false, true, NULL );
}

const char *Replay_GetDownloadURLPath()
{
	static char s_szDownloadURLPath[MAX_OSPATH];
	extern ConVar replay_fileserver_path;	// NOTE: replicated

	V_strcpy_safe( s_szDownloadURLPath, replay_fileserver_path.GetString() );
	V_StripTrailingSlash( s_szDownloadURLPath );
	V_FixSlashes( s_szDownloadURLPath, '/' );

	// Get rid of starting slash
	if ( s_szDownloadURLPath[0] == '/' )
		return &s_szDownloadURLPath[1];

	return s_szDownloadURLPath;
}

const char *Replay_GetDownloadURL()
{
#if 0
	// Get the local host name
	char szHostname[MAX_OSPATH];
	if ( gethostname( szHostname, sizeof( szHostname ) ) == -1 )
	{
		Error( "Failed to send to Replay to client - couldn't get local IP.\n" );
		return "";
	}
#endif

	// Construct the URL based on replicated cvars
	static char s_szFileURL[ MAX_OSPATH ];
	extern ConVar replay_fileserver_protocol;
	extern ConVar replay_fileserver_host;
	extern ConVar replay_fileserver_port;
	V_snprintf(
		s_szFileURL, sizeof( s_szFileURL ),
		"%s://%s:%i/%s/",
		replay_fileserver_protocol.GetString(),
		replay_fileserver_host.GetString(),
		replay_fileserver_port.GetInt(),
		Replay_GetDownloadURLPath()
	);

	// Cleanup
	V_FixDoubleSlashes( s_szFileURL + V_strlen("http://") );

	return s_szFileURL;
}

//----------------------------------------------------------------------------------------
// Purpose: (client/server) Crack a URL into a base and a path
// NOTE: the URL *must contain a port* !
//
//   Example: http://some.base.url:8080/a/path/here.txt cracks into:
//     pBaseURL = "http://some.base.url:8080"
//     pURLPath = "/a/path/here.txt"
//----------------------------------------------------------------------------------------
void Replay_CrackURL( const char *pURL, char *pBaseURLOut, char *pURLPathOut )
{
	const char *pColon;
	const char *pURLPath;

	// Must at least have "http://"
	if ( V_strlen(pURL) < 6 )
		goto fail;

	// Skip protocol ':' (eg http://)
	pColon = V_strstr( pURL, ":" );
	if ( !pColon )
		goto fail;

	// Find next colon
	pColon = V_strstr( pColon + 1, ":" );
	if ( !pColon )
		goto fail;

	// Copies "http[s]://<address>:<port>
	pURLPath = V_strstr( pColon, "/" );
	V_strncpy( pBaseURLOut, pURL, pURLPath - pURL + 1 );
	V_strcpy( pURLPathOut, pURLPath );

	return;

fail:
	AssertMsg( 0, "Replay_CrackURL() was passed an invalid URL and has failed.  This should never happen." );
}

#ifndef DEDICATED
void Replay_HudMsg( const char *pText, const char *pSound, bool bUrgent )
{
	g_pClient->DisplayReplayMessage( pText, bUrgent, false, pSound );
}
#endif

//----------------------------------------------------------------------------------------
