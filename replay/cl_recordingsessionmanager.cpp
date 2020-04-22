//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cl_recordingsessionmanager.h"
#include "replaysystem.h"
#include "cl_replaymanager.h"
#include "cl_recordingsession.h"
#include "cl_sessionblockdownloader.h"
#include "vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

#define CLIENTRECORDINGSESSIONMANAGER_VERSION	0

//----------------------------------------------------------------------------------------

CClientRecordingSessionManager::CClientRecordingSessionManager( IReplayContext *pContext )
:	CBaseRecordingSessionManager( pContext ),
	m_nNumSessionBlockDownloaders( 0 ),
	m_flNextBlockUpdateTime( 0.0f ),
	m_flNextPossibleDownloadTime( 0.0f )
{
}

CClientRecordingSessionManager::~CClientRecordingSessionManager()
{
}

bool CClientRecordingSessionManager::Init()
{
	AddEventsForListen();

	return BaseClass::Init();
}

void CClientRecordingSessionManager::CleanupUnneededBlocks()
{
	Msg( "Cleaning up unneeded replay block data...\n" );
	FOR_EACH_OBJ( this, i )
	{
		CClientRecordingSession *pCurSession = CL_CastSession( m_vecObjs[ i ] );
		pCurSession->LoadBlocksForSession();
		pCurSession->DeleteBlocks();
	}
	Msg( "Replay cleanup done.\n" );
}

void CClientRecordingSessionManager::AddEventsForListen()
{
	g_pGameEventManager->AddListener( this, "replay_endrecord", false );
	g_pGameEventManager->AddListener( this, "replay_sessioninfo", false );
	g_pGameEventManager->AddListener( this, "player_death", false );
}

const char *CClientRecordingSessionManager::GetNewSessionName() const
{
	return m_ServerRecordingState.m_strSessionName;
}

CBaseRecordingSession *CClientRecordingSessionManager::OnSessionStart( int nCurrentRecordingStartTick, const char *pSessionName )
{
	return BaseClass::OnSessionStart( nCurrentRecordingStartTick, pSessionName );
}

void CClientRecordingSessionManager::OnSessionEnd()
{
	if ( m_pRecordingSession )
	{
		// Update whether all blocks have been downloaded
		AssertMsg( !m_pRecordingSession->m_bRecording, "This flag should have been cleared already!  See CBaseRecordingSession::OnStopRecording()" );
		CL_CastSession( m_pRecordingSession )->UpdateAllBlocksDownloaded();
	}

	BaseClass::OnSessionEnd();

	m_ServerRecordingState.Clear();
}

void CClientRecordingSessionManager::FireGameEvent( IGameEvent *pEvent )
{
	DBG( "CReplayHistoryManager::FireGameEvent()\n" );

	if ( g_pEngineClient->IsDemoPlayingBack() )
		return;
	
	const char *pEventName = pEvent->GetName();

	if ( !V_stricmp( "replay_sessioninfo", pEventName ) )
	{
		DBG( "   replay_sessioninfo\n" );

		bool bDisableReplayOnClient = false;

		const CUtlString strSessionName = pEvent->GetString( "sn" );
		if ( strSessionName.IsEmpty() )
		{
			CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_SessionInfo_BadSessionName" );
			bDisableReplayOnClient = true;
		}

		const int nDumpInterval = pEvent->GetInt( "di", 0 );
		if ( nDumpInterval < MIN_SERVER_DUMP_INTERVAL ||
			 nDumpInterval > MAX_SERVER_DUMP_INTERVAL )
		{
			CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_SessionInfo_BadDumpInterval" );
			bDisableReplayOnClient = true;
		}

		const int nCurrentBlock = pEvent->GetInt( "cb", -1 );
		if ( nCurrentBlock < 0 )
		{
			CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_SessionInfo_BadCurrentBlock" );
			bDisableReplayOnClient = true;
		}

		const int nStartTick = pEvent->GetInt( "st", -1 );
		if ( nStartTick < 0 )
		{
			CL_GetErrorSystem()->AddErrorFromTokenName( "Replay_Err_SessionInfo_BadStartTick" );
			bDisableReplayOnClient = true;
		}

		// Cache session state
		m_ServerRecordingState.m_strSessionName = strSessionName;
		m_ServerRecordingState.m_nDumpInterval = nDumpInterval;
		m_ServerRecordingState.m_nCurrentBlock = nCurrentBlock + 1;		// This will account for any different between when the server actually dumps a block and the client predicts a dump
		m_ServerRecordingState.m_nStartTick = nStartTick;

		// If the server's in a weird state, disable replay on the client so they don't
		// create any replays that don't play, etc.
		g_pClientReplayContextInternal->DisableReplayOnClient( bDisableReplayOnClient );
		if ( bDisableReplayOnClient )
			return;

		OnSessionStart( nStartTick, strSessionName.Get() );

		CL_GetReplayManager()->OnSessionStart();

		// Update the current block based on the dump interval passed down from
		// the server so the client stays in sync w/ the current block index.
		m_flNextBlockUpdateTime = g_pEngine->GetHostTime() + nDumpInterval;
	}

	else if ( !V_stricmp( "replay_endrecord", pEventName ) )
	{
		DBG( "   replay_stoprecord\n" );

		// Clear pending replay URL cache, complete any pending replay
		CL_GetReplayManager()->OnSessionEnd();

		// Notify the session - it will mark itself as no longer recording.
		if ( m_pRecordingSession )
		{
			m_pRecordingSession->OnStopRecording();
		}

		// Resets current session pointer
		OnSessionEnd();
	}

	// When the player dies, we fill out the rest of the data here
	else if ( !V_stricmp( "player_death", pEventName ) &&
			  pEvent->GetInt( "victim_entindex" ) == g_pEngineClient->GetPlayerSlot() + 1 &&
			  g_pClient->ShouldCompletePendingReplay( pEvent ) )
	{
		CL_GetReplayManager()->CompletePendingReplay();
	}
}

int CClientRecordingSessionManager::GetVersion() const
{
	return CLIENTRECORDINGSESSIONMANAGER_VERSION;
}

void CClientRecordingSessionManager::Think()
{
	VPROF_BUDGET( "CClientRecordingSessionManager::Think", VPROF_BUDGETGROUP_REPLAY );

	BaseClass::Think();

	// Manage all session block downloads
	DownloadThink();

	if ( !g_pReplay->IsRecording() )
		return;

	if ( g_pEngineClient->IsDemoPlayingBack() )
		return;

	const float flHostTime = g_pEngine->GetHostTime();

	if ( replay_debug.GetBool() )
	{
		extern ConVar replay_postdeathrecordtime;
		g_pEngineClient->Con_NPrintf( 100, "Time until block dump: ~%f", m_flNextBlockUpdateTime - flHostTime );
		g_pEngineClient->Con_NPrintf( 101, "Post-death record time: %f", replay_postdeathrecordtime.GetFloat() );
	}

	if ( m_flNextBlockUpdateTime <= flHostTime )
	{
		// Increment current block
		++m_ServerRecordingState.m_nCurrentBlock;

		// NOTE: Now the number of blocks in the recording session in progress should be
		// different from the number of blocks in its list - so it should spawn a download
		// thread to grab the session info and create the new block.

		IF_REPLAY_DBG( Warning( "# session blocks updating: %i\n", m_ServerRecordingState.m_nCurrentBlock ) );

		// Setup next think
		m_flNextBlockUpdateTime = flHostTime + m_ServerRecordingState.m_nDumpInterval;
	}
}

void CClientRecordingSessionManager::DownloadThink()
{
	bool bKickedOffDownload = false;
	const float flHostTime = g_pEngine->GetHostTime();

	// For session in progress - check predicted # of blocks on the server based on current number
	// of blocks in our list - if different, download the session info and create any outstanding
	// blocks on the client.
	bool bEnoughTimeHasPassed = flHostTime >= m_flNextPossibleDownloadTime;
	if ( !bEnoughTimeHasPassed )
		return;

	// Go through all sessions to see if we need to create session block downloaders
	FOR_EACH_OBJ( this, i )
	{
		CClientRecordingSession *pCurSession = CL_CastSession( m_vecObjs[ i ] );

		// Already have a session block downloader?  NOTE: The think manager calls its Think().
		if ( pCurSession->HasSessionInfoDownloader() )
			continue;

		// If the # of blocks on the client is out of sync with the number of blocks we need
		// to eventually download, sync with the server, i.e. download the session info and
		// create blocks/sync block data as needed.
		if ( pCurSession->ShouldSyncBlocksWithServer() )
		{
			pCurSession->SyncSessionBlocks();
			bKickedOffDownload = true;
		}
	}

	// Set next possible download time if we just kicked off a download
	if ( bKickedOffDownload )
	{
		m_flNextPossibleDownloadTime = flHostTime + MAX( MIN_SERVER_DUMP_INTERVAL, CL_GetRecordingSessionManager()->m_ServerRecordingState.m_nDumpInterval );
	}
}

CBaseRecordingSession *CClientRecordingSessionManager::Create()
{
	return new CClientRecordingSession( m_pContext );
}

IReplayContext *CClientRecordingSessionManager::GetReplayContext() const
{
	return g_pClientReplayContextInternal;
}

void CClientRecordingSessionManager::OnObjLoaded( CBaseRecordingSession *pSession )
{
	// Make sure the session doesn't try to start downloading if it's done
	CL_CastSession( pSession )->UpdateAllBlocksDownloaded();
}

void CClientRecordingSessionManager::OnReplayDeleted( CReplay *pReplay )
{
	// Notify the session that a replay has been deleted, in case it needs to do any cleanup.
	CClientRecordingSession *pSession = CL_CastSession( FindSession( pReplay->m_hSession ) );
	if ( pSession )
	{
		pSession->OnReplayDeleted( pReplay );
	}

	// Get the # of replays that depend on the given session
	int nNumDependentReplays = CL_GetReplayManager()->GetNumReplaysDependentOnSession( pReplay->m_hSession );
	if ( nNumDependentReplays == 1 )
	{
		// Delete the session - remove the item from the manager itself, delete the
		// .dem file, and any .dmx.
		DeleteSession( pReplay->m_hSession, false );
	}
}

void CClientRecordingSessionManager::OnReplaysLoaded()
{
	// Cache replay pointers in sessions for quick access
	FOR_EACH_REPLAY( i )
	{
		CReplay *pCurReplay = GET_REPLAY_AT( i );
		CClientRecordingSession *pOwnerSession = CL_CastSession( CL_GetRecordingSessionManager()->FindSession( pCurReplay->m_hSession ) );	Assert( pOwnerSession );
		if ( !pOwnerSession )
		{
			CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Load_BadOwnerSession" );
			continue;
		}

		pOwnerSession->CacheReplay( pCurReplay );
	}
}

//----------------------------------------------------------------------------------------
