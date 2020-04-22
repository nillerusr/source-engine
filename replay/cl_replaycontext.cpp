//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cl_replaycontext.h"
#include "replaysystem.h"
#include "replay/iclientreplay.h"
#include "replay/ireplaymovierenderer.h"
#include "replay/shared_defs.h"
#include "cl_replaymanager.h"
#include "replay_dbg.h"
#include "baserecordingsessionmanager.h"
#include "baserecordingsessionblockmanager.h"
#include "cl_replaymoviemanager.h"
#include "cl_screenshotmanager.h"
#include "cl_performancemanager.h"
#include "cl_sessionblockdownloader.h"
#include "cl_downloader.h"
#include "cl_recordingsession.h"
#include "cl_recordingsessionblock.h"
#include "cl_renderqueue.h"
#include "replay_reconstructor.h"
#include "globalvars_base.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CClientReplayContext::CClientReplayContext()
:	m_pReplayManager( NULL ),
	m_pScreenshotManager( NULL ),
	m_pMovieRenderer( NULL ),
	m_pMovieManager( NULL ),
	m_pPerformanceManager( NULL ),
	m_pPerformanceController( NULL ),
	m_pTestDownloader( NULL ),
	m_pRenderQueue( NULL ),
	m_bClientSideReplayDisabled( false )
{
}

CClientReplayContext::~CClientReplayContext()
{
	delete m_pSessionBlockDownloader;
	delete m_pReplayManager;
	delete m_pScreenshotManager;
	delete m_pMovieManager;
	delete m_pPerformanceManager;
	delete m_pShared;
	delete m_pTestDownloader;
}

bool CClientReplayContext::Init( CreateInterfaceFn fnFactory )
{
	m_pShared = new CSharedReplayContext( this );
	m_pShared->m_strSubDir = SUBDIR_CLIENT;
	m_pShared->m_pRecordingSessionManager = new CClientRecordingSessionManager( this );
	m_pShared->m_pRecordingSessionBlockManager = new CClientRecordingSessionBlockManager( this );
	m_pShared->m_pErrorSystem = new CErrorSystem( this );

	if ( !m_pShared->Init( fnFactory ) )
		return false;

	m_pPerformanceManager = new CReplayPerformanceManager();
	m_pPerformanceManager->Init();

	m_pReplayManager = new CReplayManager();
	m_pReplayManager->Init( fnFactory );

	m_pScreenshotManager = new CScreenshotManager();
	m_pScreenshotManager->Init();

	m_pMovieManager = new CReplayMovieManager();
	m_pMovieManager->Init();

	m_pRenderQueue = new CRenderQueue();
	if ( !m_pRenderQueue )
		return false;

	m_pSessionBlockDownloader = new CSessionBlockDownloader();
	if ( !m_pSessionBlockDownloader )
		return false;

	m_pPerformanceController = new CPerformanceController();

	// Cleanup any unneeded block data from disk - cleanup is done on the fly, but this will clean up
	// blocks from the olden days, when block data was not cleaned up properly.
	CleanupUnneededBlocks();

	return true;
}

void CClientReplayContext::Shutdown()
{
	// NOTE: Must come first, as any existing downloads are aborted and may cause status
	// changes in replays, etc, which will need to be saved in CReplayManager::Shutdown(), etc.
	m_pSessionBlockDownloader->Shutdown();

	m_pShared->Shutdown();
	m_pReplayManager->Shutdown();
	m_pMovieManager->Shutdown();
}

void CClientReplayContext::DebugThink()
{
	if ( !replay_debug.GetBool() )
		return;

	int iLine = 15;

	// Recording session in progress
	CClientRecordingSession *pRecordingSession = CL_GetRecordingSessionInProgress();
	if ( pRecordingSession )
	{
		g_pEngineClient->Con_NPrintf( iLine++, "SESSION IN PROGRESS:" );
		g_pEngineClient->Con_NPrintf( iLine++, "  BLOCKS: %i", pRecordingSession->GetNumBlocks() );
		g_pEngineClient->Con_NPrintf( iLine++, "  NAME: %s", pRecordingSession->m_strName.Get() );
		g_pEngineClient->Con_NPrintf( iLine++, "  URL: %s", pRecordingSession->m_strBaseDownloadURL.Get() );
		g_pEngineClient->Con_NPrintf( iLine++, "  LAST CONSECUTIVE BLOCK DOWNLOADED: %i", pRecordingSession->GetGreatestConsecutiveBlockDownloaded() );
		g_pEngineClient->Con_NPrintf( iLine++, "  LAST BLOCK TO DOWNLOAD: %i", pRecordingSession->GetLastBlockToDownload() );
	}
	else
	{
		g_pEngineClient->Con_NPrintf( iLine++, "NO SESSION IN PROGRESS" );
	}

	iLine++;

	// Server state
	CClientRecordingSessionManager::ServerRecordingState_t *pServerState = &CL_GetRecordingSessionManager()->m_ServerRecordingState;
	g_pEngineClient->Con_NPrintf( iLine++, "SERVER STATE:" );
	g_pEngineClient->Con_NPrintf( iLine++, "  NAME: %s\n", pServerState->m_strSessionName.Get() );
	g_pEngineClient->Con_NPrintf( iLine++, "  DUMP INTERVAL: %i\n", pServerState->m_nDumpInterval );
	g_pEngineClient->Con_NPrintf( iLine++, "  CURRENT BLOCK: %i\n", pServerState->m_nCurrentBlock );
}

void CClientReplayContext::Think()
{
	DebugThink();

	if ( m_pTestDownloader )
	{
		m_pTestDownloader->Think();
		if ( m_pTestDownloader->IsDone() )
		{
			delete m_pTestDownloader;
			m_pTestDownloader = NULL;
		}
	}

	if ( !g_pReplay->IsReplayEnabled() )
		return;

	m_pShared->Think();
}

CReplay *CClientReplayContext::GetReplay( ReplayHandle_t hReplay )
{
	return m_pReplayManager->GetReplay( hReplay );
}

IReplayManager *CClientReplayContext::GetReplayManager()
{
	return m_pReplayManager;
}

IReplayScreenshotManager *CClientReplayContext::GetScreenshotManager()
{
	return m_pScreenshotManager;
}

IReplayPerformanceManager *CClientReplayContext::GetPerformanceManager()
{
	return m_pPerformanceManager;
}

IReplayPerformanceController *CClientReplayContext::GetPerformanceController()
{
	return m_pPerformanceController;
}

IReplayRenderQueue *CClientReplayContext::GetRenderQueue()
{
	return m_pRenderQueue;
}

void CClientReplayContext::SetMovieRenderer( IReplayMovieRenderer *pMovieRenderer )
{
	m_pMovieRenderer = pMovieRenderer;
}

IReplayMovieRenderer *CClientReplayContext::GetMovieRenderer()
{
	return m_pMovieRenderer;
}

IReplayMovieManager *CClientReplayContext::GetMovieManager()
{
	return m_pMovieManager;
}

void CClientReplayContext::TestDownloader( const char *pURL )
{
	// Don't overwrite existing test
	if ( m_pTestDownloader )
		return;

	// Download the file
	m_pTestDownloader = new CHttpDownloader();
	m_pTestDownloader->BeginDownload( pURL, NULL );
}

void CClientReplayContext::OnSignonStateFull()
{
	// Notify the demo player that we've reached full signon state
	if ( g_pEngineClient->IsPlayingReplayDemo() )
	{
		g_pReplayDemoPlayer->OnSignonStateFull();
	}

	// Play a performance?  This will play a performance from the beginning, if we're loading
	// one (ie the 'watch' button in the details panel of the replay browser), or will continue
	// playback if the user rewound while watching or editing a performance.
	CL_GetPerformanceController()->OnSignonStateFull();

	// If we're rendering, display the viewport
	if ( CL_GetMovieManager()->IsRendering() )
	{
		extern IClientReplay *g_pClient;
		g_pClient->OnRenderStart();

		// Prepare audio system for recording.
		g_pEngineClient->InitSoundRecord();

		// Init renderer
		IReplayMovie *pMovie = CL_GetMovieManager()->GetPendingMovie();
		if ( !m_pMovieRenderer->SetupRenderer( CL_GetMovieManager()->GetRenderMovieSettings(), pMovie ) )
		{
			Warning( "Render failed!\n" );
			CL_GetMovieManager()->CancelRender();
		}
	}

	// If we're not rendering and are playing back a replay, initialize the performance editor -
	// won't actually show until the user presses space, if they do at all.
	else if ( g_pEngineClient->IsPlayingReplayDemo() )
	{
		const CReplay *pReplay = g_pReplayDemoPlayer->GetCurrentReplay();
		if ( pReplay )
		{
			g_pClient->InitPerformanceEditor( pReplay->GetHandle() );
		}
		else
		{
			AssertMsg( 0, "Replay should exist here!" );
			Warning( "No current replay in demo player!\n" );
		}
	}
}

void CClientReplayContext::OnClientSideDisconnect()
{
	if ( !g_pEngine->IsSupportedModAndPlatform() )
		return;

	// Reset replay_recording or we'll continue to think we're recording
	extern ConVar replay_recording;
	replay_recording.SetValue( 0 );

	if ( !g_pEngineClient->IsPlayingReplayDemo() )
	{
		// Saves dangling replay, if there is one, clears out everything
		// NOTE: We need to let the replay manager deal before we end the session, otherwise the
		// state of the session will be cleared.
		m_pReplayManager->OnClientSideDisconnect();

		// Mark the session as no longer recording.
		CClientRecordingSession *pSession = CL_GetRecordingSessionInProgress();
		if ( pSession )
		{
			pSession->OnStopRecording();
		}

		// Sets recording flag to false in session in progress, clears session in progress,
		// and clears server state
		CL_GetRecordingSessionManager()->OnSessionEnd();
	}
}

void CClientReplayContext::PlayReplay( ReplayHandle_t hReplay, int iPerformance, bool bPlaySound )
{
	CReplay *pReplay = m_pReplayManager->GetReplay( hReplay );
	if ( !pReplay )
		return;

	if ( !ReconstructReplayIfNecessary( pReplay ) )
	{
		Replay_MsgBox( iPerformance < 0 ? "#Replay_Err_User_FailedToPlayReplay" : "#Replay_Err_User_FailedToPlayTake" );
		return;
	}

	// Play a sound?
	if ( bPlaySound )
	{
		g_pClient->PlaySound( iPerformance >= 0 ? "replay\\playperformance.wav" : "replay\\playoriginalreplay.wav" );
	}

	// Play the replay!
	g_pReplayDemoPlayer->PlayReplay( hReplay, iPerformance );
}

bool CClientReplayContext::ReconstructReplayIfNecessary( CReplay *pReplay )
{
	// If reconstruction hasn't happened yet, try to reconstruct
	extern ConVar replay_forcereconstruct;
	if ( !pReplay->HasReconstructedReplay() || replay_forcereconstruct.GetBool() )
	{
		if ( !Replay_Reconstruct( pReplay ) )
		{
			CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Reconstruction_Fail" );
			return false;
		}
	}

	return true;
}

void CClientReplayContext::OnPlayerSpawn()
{
	DBG( "OnPlayerSpawn()\n" );
	m_pReplayManager->AttemptToSetupNewReplay();
}

void CClientReplayContext::OnPlayerClassChanged()
{
	DBG( "OnPlayerClassChanged()\n" );
	m_pReplayManager->CompletePendingReplay();
}

void CClientReplayContext::GetPlaybackTimes( float &flOutTime, float &flOutLength, const CReplay *pReplay, const CReplayPerformance *pPerformance )
{
	flOutTime = 0.0f;
	flOutLength = 0.0f;
	
	// Get server start record tick
	const int nServerRecordStartTick = CL_GetRecordingSessionManager()->GetServerStartTickForSession( pReplay->m_hSession );

	// Don't let it be -1.  Take performance in tick into account.
	int nStartTick = MAX( 0, pReplay->m_nSpawnTick );
	if ( pPerformance && pPerformance->m_nTickIn >= 0 )
	{
		nStartTick = pPerformance->m_nTickIn;
	}

	// Calculate length
	const int nReplayEndTick = pReplay->m_nSpawnTick + g_pEngine->TimeToTicks( pReplay->m_flLength );
	const int nEndTick = ( pPerformance && pPerformance->m_nTickOut > 0 ) ? pPerformance->m_nTickOut : nReplayEndTick;
	flOutLength = pPerformance ? g_pEngine->TicksToTime( nEndTick - nStartTick ) : pReplay->m_flLength;

	// Calculate current time
	const int nCurTick = MAX( g_pEngineClient->GetClientGlobalVars()->tickcount - nStartTick - nServerRecordStartTick, 0 );
	flOutTime = MIN( g_pEngine->TicksToTime( nCurTick ), flOutLength );
}

uint64 CClientReplayContext::GetServerSessionId( ReplayHandle_t hReplay )
{
	CReplay *pReplay = GetReplay( hReplay );
	if ( !pReplay )
		return 0;

	CClientRecordingSession *pSession = CL_CastSession( CL_GetRecordingSessionManager()->FindSession( pReplay->m_hSession ) );
	if ( !pSession )
		return 0;

	return pSession->GetServerSessionID();
}

void CClientReplayContext::CleanupUnneededBlocks()
{
	CL_GetRecordingSessionManager()->CleanupUnneededBlocks();
}

void CClientReplayContext::ReportErrorsToUser( wchar_t *pErrorText )
{
	// Display a message now
//	Replay_MsgBox( pErrorText );

	if ( !pErrorText || pErrorText[0] == L'\0' )
		return;

	const int nErrorLen = wcslen( pErrorText );
	static char s_szError[1024];
	wcstombs( s_szError, pErrorText, MIN( 1024, nErrorLen ) );
	Warning( "Replay error system: %s\n", s_szError );
}

void CClientReplayContext::DisableReplayOnClient( bool bDisable )
{
	if ( m_bClientSideReplayDisabled == bDisable )
		return;

	m_bClientSideReplayDisabled = bDisable;

	// Display a message to the user
	Replay_HudMsg( bDisable ? "#Replay_ClientSideDisabled" : "#Replay_ClientSideEnabled", NULL, true );
}

//----------------------------------------------------------------------------------------

CClientRecordingSessionManager *CL_GetRecordingSessionManager()
{
	return static_cast< CClientRecordingSessionManager * >( g_pClientReplayContextInternal->GetRecordingSessionManager() );
}

CClientRecordingSession *CL_GetRecordingSessionInProgress()
{
	return CL_CastSession( CL_GetRecordingSessionManager()->GetRecordingSessionInProgress() );
}

//----------------------------------------------------------------------------------------
