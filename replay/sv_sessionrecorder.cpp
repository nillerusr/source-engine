//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "sv_sessionrecorder.h"
#include "replay/replayutils.h"
#include "replay/shared_defs.h"
#include "baserecordingsessionblock.h"
#include "replaysystem.h"
#include "baserecordingsessionblockmanager.h"
#include "sv_recordingsessionmanager.h"
#include "sv_replaycontext.h"
#include "sv_sessionpublishmanager.h"
#include "sv_recordingsession.h"
#include "sv_recordingsessionblock.h"
#include "fmtstr.h"
#include "vprof.h"
#include "iserver.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#undef CreateEvent

//----------------------------------------------------------------------------------------

#define SERVER_REPLAY_INDEX_FILENAME			".replayindex"
#define SERVER_REPLAY_ERROR_LOST				"The server crashed before the replay could be finalized.  Replay lost."

//----------------------------------------------------------------------------------------

CSessionRecorder::CSessionRecorder()
:	m_bRecordingAborted( false ),
	m_nCurrentRecordingStartTick( -1 )
{
}

CSessionRecorder::~CSessionRecorder()
{
}

bool CSessionRecorder::Init()
{
	g_pFullFileSystem->CreateDirHierarchy( Replay_va( "%s%s", SV_GetBasePath(), SUBDIR_SESSIONS ) );
	return true;
}

void CSessionRecorder::AbortCurrentSessionRecording()
{
	StopRecording( true );

	CSessionPublishManager *pCurrentPublishManager = GetCurrentPublishManager();
	if ( !pCurrentPublishManager )
	{
		AssertMsg( 0, "Could not get current publish manager." );
		return;
	}

	pCurrentPublishManager->AbortPublish();

	m_bRecordingAborted = true;
}

void CSessionRecorder::SetCurrentRecordingStartTick( int nStartTick )
{
	m_nCurrentRecordingStartTick = nStartTick;
}

void CSessionRecorder::PublishAllSynchronous()
{
	FOR_EACH_LL( m_lstPublishManagers, i )
	{
		m_lstPublishManagers[ i ]->PublishAllSynchronous();
	}
}

void CSessionRecorder::StartRecording()
{
	m_bRecordingAborted = false;

	IServer *pServer = ReplayServerAsIServer();
	if ( !pServer || !pServer->IsActive() )
	{
		ConMsg( "ERROR: Replay not active.\n" );
		return;
	}

	// We only care about local fileserver path in the case that we aren't offloading files to an external sfileserver
	const char *pWritePath = g_pServerReplayContext->GetLocalFileServerPath();
	if ( ( !pWritePath || !pWritePath[0] ) )
	{
		ConMsg( "\n*\n* ERROR: Failed to begin record: make sure \"replay_local_fileserver_path\" refers to a valid path!\n** replay_local_fileserver_path is currently set to: \"%s\"\n*\n\n", pWritePath );
		return;
	}

	IReplayServer *pReplayServer = ReplayServer();
	if ( pReplayServer->IsRecording() )
	{
		ConMsg( "ERROR: Replay already recording.\n" );
		return;
	}

	// Tell the replay server to begin recording
	pReplayServer->StartRecording();

	// Notify session manager
	CBaseRecordingSession *pSession = SV_GetRecordingSessionManager()->OnSessionStart( m_nCurrentRecordingStartTick, NULL );

	// Create a new publish manager and add it.  The dump interval and any additional setup is done there.
	CreateAndAddNewPublishManager( static_cast< CServerRecordingSession * >( pSession ) );
}

void CSessionRecorder::CreateAndAddNewPublishManager( CServerRecordingSession *pSession )
{
	CSessionPublishManager *pNewPublishManager = new CSessionPublishManager( pSession );

	// Let the publish manager know that it is the 'current' publish manager.
	pNewPublishManager->OnStartRecording();

	// Add to the head of the list, since the desired convention is for the list to be
	// sorted from newest to oldest.
	m_lstPublishManagers.AddToHead( pNewPublishManager );
}

float CSessionRecorder::GetNextThinkTime() const
{
	return 0.0f;
}

void CSessionRecorder::Think()
{
	CBaseThinker::Think();

	VPROF_BUDGET( "CSessionRecorder::Think", VPROF_BUDGETGROUP_REPLAY );

	// This gets called even if replay is disabled.  This is intentional.
	PublishThink();
}

CSessionPublishManager *CSessionRecorder::GetCurrentPublishManager() const
{
	if ( !m_lstPublishManagers.Count() )
		return NULL;

	return m_lstPublishManagers[ m_lstPublishManagers.Head() ];
}

void CSessionRecorder::PublishThink()
{
	UpdateSessionLocks();
}

void CSessionRecorder::UpdateSessionLocks()
{
	for ( int i = m_lstPublishManagers.Head(); i != m_lstPublishManagers.InvalidIndex(); )
	{
		CSessionPublishManager *pCurManager = m_lstPublishManagers[ i ];

		// Cache off 'next' in case we delete the current object
		const int itNext = m_lstPublishManagers.Next( i );

		if ( pCurManager->IsDone() )
		{
#ifdef _DEBUG
			pCurManager->Validate();
#endif

			// We can unlock the associated session now.
			pCurManager->UnlockSession();

			// Remove and delete it.
			m_lstPublishManagers.Remove( i );
			delete pCurManager;

			IF_REPLAY_DBG( Warning( "\n---\n*\n* All publishing done for session.  %i still publishing.\n*\n---\n", m_lstPublishManagers.Count() ) );
		}
		else
		{
			pCurManager->Think();
		}

		i = itNext;
	}
}

void CSessionRecorder::StopRecording( bool bAborting )
{
#if !defined( DEDICATED )
	if ( g_pEngineClient->IsPlayingReplayDemo() )
		return;
#endif
	if ( !ReplayServer() )
		return;

	DBG( "StopRecording()\n" );

	CServerRecordingSession *pSession = SV_GetRecordingSessionInProgress();
	if ( pSession )
	{
		// Mark the session as not recording
		pSession->OnStopRecording();

		// Get the current publish manager and notify it that recording has stopped.
		CSessionPublishManager *pManager = GetCurrentPublishManager();
		if ( pManager )
		{
			pManager->OnStopRecord( bAborting );
		}

		// Notify session manager - the session will be flagged for unload or deletion, but
		// will not actually be free'd until it is "unlocked" by the publish manager.
		SV_GetRecordingSessionManager()->OnSessionEnd();
	}

	// Stop recording
	ReplayServer()->StopRecording();

	// Clear replay_recording
	extern ConVar replay_recording;
	replay_recording.SetValue( 0 );
}

//----------------------------------------------------------------------------------------
