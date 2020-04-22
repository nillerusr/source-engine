//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cl_performancecontroller.h"
#include "cl_replaycontext.h"
#include "globalvars_base.h"
#include "cl_replaycontext.h"
#include "replay/replay.h"
#include "replay/ireplaycamera.h"
#include "replay/replayutils.h"
#include "replay/ireplayperformanceplaybackhandler.h"
#include "replay/ireplayperformanceeditor.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "replaysystem.h"
#include "cl_replaymanager.h"
#include "vprof.h"
#include "cl_performance_common.h"
#include "engine/ivdebugoverlay.h"
#include "utlbuffer.h"

#undef Yield
#include "vstdlib/jobthread.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

#ifdef _DEBUG
ConVar replay_simulate_long_save( "replay_simulate_long_save", "0", FCVAR_DONTRECORD, "Simulate a long save.  Seconds." );
#endif

//----------------------------------------------------------------------------------------

class CSaveJob : public CJob
{
public:
	CSaveJob( KeyValues *pInData, const char *pFullFilename )
	:	m_pInData( pInData )
	{
		SetFlags( GetFlags() | JF_IO );

		V_strncpy( m_szFullFilename, pFullFilename, sizeof( m_szFullFilename ) );
	}

	virtual JobStatus_t DoExecute()
	{
		if ( !m_pInData )
			return JOB_FAILED;

		if ( !V_strlen( m_szFullFilename ) )
			return JOB_FAILED;

#ifdef _DEBUG
		const int nDelay = replay_simulate_long_save.GetInt();
		if ( nDelay )
		{
			ThreadSleep( nDelay * 1000 );
		}
#endif

		return m_pInData->SaveToFile( g_pFullFileSystem, m_szFullFilename, "MOD" ) ? JOB_OK : JOB_FAILED;
	}

private:
	KeyValues *m_pInData;
	char m_szFullFilename[MAX_OSPATH];
};

//----------------------------------------------------------------------------------------

CPerformanceController::CPerformanceController()
:	m_pRoot( NULL ),
	m_pCurEvent( NULL ),
	m_pDbgRoot( NULL ),
	m_pPlaybackHandler( NULL ),
	m_pSetViewEvent( NULL ),
	m_pSaveJob( NULL ),
	m_bViewOverrideMode( false ),
	m_bDirty( false ),
	m_bLastSaveStatus( false ),
	m_bRewinding( false ),
	m_pSavedPerformance( NULL ),
	m_pScratchPerformance( NULL ),
	m_hReplay( REPLAY_HANDLE_INVALID ),
	m_nState( STATE_DORMANT ),
	m_pEditor( NULL ),
	m_flLastCamSetViewTime( 0.0f ),
	m_flTimeScale( 1.0f )
{
}

CPerformanceController::~CPerformanceController()
{
	Cleanup();
}

void CPerformanceController::Cleanup()
{
	AssertMsg(
		!m_pScratchPerformance || m_pScratchPerformance != m_pSavedPerformance,
		"Sanity check failed.  We should never be assigning saved to scratch or vice versa."
	);

	m_pSavedPerformance = NULL;

	if ( m_pScratchPerformance )
	{
		delete m_pScratchPerformance;
		m_pScratchPerformance = NULL;
	}

	CleanupStream();
	CleanupDbgStream();

	ClearDirtyFlag();

	m_pCurEvent = NULL;
	m_pEditor = NULL;
	m_pPlaybackHandler = NULL;
	m_hReplay = REPLAY_HANDLE_INVALID;
	m_nState = STATE_DORMANT;
	m_flLastCamSetViewTime = 0.0f;
	m_flTimeScale = 1.0f;

	// Remove all queued events
	FOR_EACH_LL( m_EventQueue, i )
	{
		m_EventQueue[ i ]->deleteThis();
	}
	m_EventQueue.RemoveAll();
}

void CPerformanceController::CleanupStream()
{
	if ( m_pRoot )
	{
		m_pRoot->deleteThis();
		m_pRoot = NULL;
	}
}

void CPerformanceController::CleanupDbgStream()
{
	if ( m_pDbgRoot )
	{
		m_pDbgRoot->deleteThis();
		m_pDbgRoot = NULL;
	}
}

float CPerformanceController::GetTime() const
{
	Assert( m_pCurEvent );
	return atof( m_pCurEvent->GetName() );
}

void CPerformanceController::SetEditor( IReplayPerformanceEditor *pEditor )
{
	AssertMsg( pEditor, "This is bad.  You must supply a valid editor pointer." );

	// Cache editor
	m_pEditor = pEditor;
}

void CPerformanceController::StartRecording( CReplay *pReplay, bool bSnip )
{
	Assert( !IsRecording() );

	AssertMsg(
		m_nState == STATE_PLAYING || !m_pRoot,
		"Unless we're playing, root should be NULL here"
	);

	if ( m_nState == STATE_DORMANT )
	{
		Assert( !m_pSavedPerformance );

		// Create the performance KeyValues
		m_pRoot = new KeyValues( "performance" );
	}
	else if ( m_nState == STATE_PLAYING )
	{
		// Nuke everything after the current event, or does nothing if we've past the end of the playback stream
		if ( bSnip )
		{
			Snip();
		}

		// When we go from playback to recording, we need to reset override view
		g_pClient->GetReplayCamera()->ClearOverrideView();
	}

	// Update the state
	m_nState = STATE_RECORDING;

	// Mark as dirty
	m_bDirty = true;
}

void CPerformanceController::Stop()
{
	Assert( !m_bRewinding );

	ClearRewinding();
	Cleanup();
}

bool CPerformanceController::DumpStreamToFileAsync( const char *pFullFilename )
{
	// m_pRoot can be NULL if the user only set an in and/or out point, and wants to save.
	if ( !m_pRoot )
		return true;

	// Save the file
	m_pSaveJob = new CSaveJob( m_pRoot, pFullFilename );
	if ( !m_pSaveJob )
		return false;

	IThreadPool *pThreadPool = CL_GetThreadPool();
	if ( !pThreadPool )
		return false;

	pThreadPool->AddJob( m_pSaveJob );

	return true;
}

bool CPerformanceController::FlushReplay()
{
	// Get the replay
	CReplay *pReplay = GetReplay( m_hReplay );
	if ( !pReplay )
		return false;

	// Add the performance to the replay and save
	Assert( !m_pSavedPerformance || pReplay->HasPerformance( m_pSavedPerformance ) );
	CL_GetReplayManager()->FlagReplayForFlush( pReplay, true );

	return true;
}

bool CPerformanceController::SaveAsync()
{
	if ( !m_pRoot )
		return false;

	if ( !m_pScratchPerformance )
	{
		AssertMsg( 0, "Scratch performance should always be valid at this point." );
		return false;
	}
	
	// NOTE: m_pSavedPerformance should always be valid here, as 'save' is disabled until it
	// has an actual performance to save to.

	// Copy the relevant data from scratch -> saved - we want to preserve the filename
	// the saved performance, and have no reason to copy over duplicate data (eg the replay
	// handle).
	m_pSavedPerformance->CopyTicks( m_pScratchPerformance );

	// Copy title
	V_wcsncpy( m_pSavedPerformance->m_wszTitle, m_pScratchPerformance->m_wszTitle, sizeof( m_pSavedPerformance->m_wszTitle ) );

	// Use the saved performance's filename
	DumpStreamToFileAsync( m_pSavedPerformance->GetFullPerformanceFilename() );

	// Save the replay file
	FlushReplay();

	// Clear dirty flag
	ClearDirtyFlag();

	return true;
}

bool CPerformanceController::SaveAsAsync( const wchar_t *pTitle )
{
	//
	// NOTE: This function assumes the following:
	//
	//   * We've already dealt with checking the given title versus existing performances
	//     in the replay and that the user has selected to overwrite.
	//

	CReplay *pReplay = m_pEditor->GetReplay();
	if ( !pReplay )
	{
		AssertMsg( 0, "Replay must exist!" );
		return false;
	}

	// Find existing performance in replay, if it exists.
	CReplayPerformance *pExistingPerformance = pReplay->GetPerformanceWithTitle( pTitle );
	if ( !pExistingPerformance )
	{
		// Create and add a new performance to the replay with a unique filename - do not generate a title since we will
		// use the incoming title.
		CReplayPerformance *pCopy = pReplay->AddNewPerformance( false, true );

		// Copy the ticks, which is all we care about
		pCopy->CopyTicks( m_pScratchPerformance );

		// Set the title
		pCopy->SetTitle( pTitle );

		// Dump to the new file and save the replay
		if ( !DumpStreamToFileAsync( pCopy->GetFullPerformanceFilename() ) ||
			 !FlushReplay() )
		{
			 return false;
		}

		// If we didn't spawn a thread, we want this to be true here, since the replay flushed
		// and DumpStreamToFileAsync() succeeded.
		m_bLastSaveStatus = true;

		// Saved performance is now replaced with the newly created performance
		m_pSavedPerformance = pCopy;

		// Clear dirty flag
		ClearDirtyFlag();

		return true;
	}

	// Overwriting an existing performance?
	else
	{
		// Performance with the given name already exists - overwrite it (again, this function
		// assumes that any UI around asking the user if they're sure they want to replace has
		// already been navigated, and the user has selected to overwrite).
		m_pSavedPerformance = pExistingPerformance;
	}

	// Copy the title to the scratch
	V_wcsncpy( m_pScratchPerformance->m_wszTitle, pTitle, MAX_TAKE_TITLE_LENGTH * sizeof( wchar_t ) );

	// Attempt to save
	if ( !SaveAsync() )
		return false;

	// Clear dirty flag
	ClearDirtyFlag();

	return true;
}

bool CPerformanceController::IsSaving() const
{
	return m_pSaveJob != NULL;
}

void CPerformanceController::SaveThink()
{
	if ( !m_pSaveJob )
		return;

	if ( m_pSaveJob->IsFinished() )
	{
		// Cache save status
		m_bLastSaveStatus = m_pSaveJob->GetStatus() == JOB_OK;

		m_pSaveJob->Release();
		m_pSaveJob = NULL;
	}
}

bool CPerformanceController::GetLastSaveStatus() const
{
	return m_bLastSaveStatus;
}

void CPerformanceController::ClearDirtyFlag()
{
	m_bDirty = false;
}

bool CPerformanceController::IsRecording() const
{
	return m_nState == STATE_RECORDING;
}

bool CPerformanceController::IsPlaying() const
{
	return m_nState == STATE_PLAYING;
}

bool CPerformanceController::IsPlaybackDataLeft()
{
	return m_pCurEvent && m_pCurEvent->GetNextTrueSubKey();
}

bool CPerformanceController::IsDirty() const
{
	return m_bDirty;
}

void CPerformanceController::NotifyDirty()
{
	AssertMsg( GetPerformance() != NULL, "Can't mark empty performance as dirty." );
	m_bDirty = true;
}

void CPerformanceController::OnSignonStateFull()
{
	if ( !g_pEngineClient->IsDemoPlayingBack() )
		return;

	// User hit rewind button (which reloads the map)?
	if ( m_bRewinding )
	{
		// Setup controller for playback from existing data.
		SetupPlaybackExistingStream();

		// Clear rewinding 
		ClearRewinding();
		
		// Let the editor know the rewind has completed.
		m_pEditor->OnRewindComplete();
	}
	else
	{
		AssertMsg( !m_pScratchPerformance, "Scratch replay should not be valid yet." );

		// If we've gotten this far and the replay is invalid, we're likely playing back a
		// regular demo and didn't early out somewhere up the chain.
		CReplay *pReplay = g_pReplayDemoPlayer->GetCurrentReplay();
		if ( !pReplay )
			return;

		// Cache replay
		m_hReplay = pReplay->GetHandle();

		// Play a performance from the beginning.
		CReplayPerformance *pPerformance = g_pReplayDemoPlayer->GetCurrentPerformance();
		if ( pPerformance )
		{
			SetupPlaybackFromPerformance( pPerformance );

			// Make a copy of the performance we're playing back so the user can make changes
			// w/o fucking up the original.
			m_pScratchPerformance = pPerformance->MakeCopy();
		}
		else
		{
			CreateNewScratchPerformance( pReplay );
		}
	}
}

float CPerformanceController::GetPlaybackTimeScale() const
{
	return m_flTimeScale;
}

void CPerformanceController::CreateNewScratchPerformance( CReplay *pReplay )
{
	// Create a new performance, but don't add it to the replay yet
	m_pScratchPerformance = CL_GetPerformanceManager()->CreatePerformance( pReplay );

	// Give it a default name
	m_pScratchPerformance->AutoNameIfHasNoTitle( pReplay->m_szMapName );

	// Generate a filename for the new performance
	m_pScratchPerformance->SetFilename( CL_GetPerformanceManager()->GeneratePerformanceFilename( pReplay ) );
}

//----------------------------------------------------------------------------------------

void CPerformanceController::NotifyPauseState( bool bPaused )
{
	if ( m_bPaused == bPaused )
		return;

	m_bPaused = bPaused;

	// Unpause?
	if ( !bPaused )
	{
		// Add queued events
		for( int i = m_EventQueue.Tail(); i != m_EventQueue.InvalidIndex(); i = m_EventQueue.Previous( i ) )
		{
			KeyValues *pCurEvent = m_EventQueue[ i ];
			AddEvent( pCurEvent );
		}

		m_EventQueue.RemoveAll();
	}
}

CReplayPerformance *CPerformanceController::GetPerformance()
{
	return m_pScratchPerformance;
}

CReplayPerformance *CPerformanceController::GetSavedPerformance()
{
	return m_pSavedPerformance;
}

bool CPerformanceController::HasSavedPerformance()
{
	return m_pSavedPerformance != NULL;
}

void CPerformanceController::Snip()
{
	if ( !m_pCurEvent )
		return;

	const float flTime = GetTime();

	// Go through all events and delete anything on or after flSnipTime
	for ( KeyValues *pCurEvent = m_pRoot->GetFirstTrueSubKey(); pCurEvent != NULL; )
	{
		// Get next first, in case we delete
		KeyValues *pNext = pCurEvent->GetNextTrueSubKey();

		const float flCurEventTime = atof( pCurEvent->GetName() );
		if ( flCurEventTime >= flTime )
		{
			// Delete the key
			m_pRoot->RemoveSubKey( pCurEvent );
			pCurEvent->deleteThis();
		}

		pCurEvent = pNext;
	}
}

bool CPerformanceController::IsCameraChangeEvent( int nType ) const
{
	return nType >= EVENTTYPE_CAMERA_CHANGE_BEGIN && nType <= EVENTTYPE_CAMERA_CHANGE_END;
}

void CPerformanceController::NotifyRewinding()
{
	m_bRewinding = true;
	m_flLastCamSetViewTime = 0.0f;
}

void CPerformanceController::ClearRewinding()
{
	m_bRewinding = false;
}

//----------------------------------------------------------------------------------------

#define CREATE_EVENT( time_, type_ ) \
	new KeyValues( Replay_va( "%f", time_ ), "type", type_ )

#define RECORD_EVENT_( event_, time_, type_ ) \
	event_ = CREATE_EVENT( time_, type_ ); \
	AddEvent( event_ )

#define RECORD_EVENT( event_, time_, type_ ) \
	KeyValues *event_ = RECORD_EVENT_( event_, time_, type_ )

#define QUEUE_OR_RECORD_EVENT( event_, time_, type_ ) \
	if ( !m_pRoot ) \
		return; \
	\
	KeyValues *event_; \
	if ( m_bPaused ) \
	{ \
		KeyValues *pQueuedEvent = CREATE_EVENT( time_, type_ ); \
		event_ = pQueuedEvent; \
		m_EventQueue.AddToHead( pQueuedEvent ); \
		RemoveDuplicateEventsFromQueue(); \
	} \
	else \
	{ \
		RECORD_EVENT_( event_, time_, type_ ); \
	}

void CPerformanceController::RemoveDuplicateEventsFromQueue()
{
	// Add queued events - only add the most recent camera change event, and the most recent
	// player change event.
	bool bFoundCameraChange = false;
	bool bFoundPlayerChange = false;
	bool bFoundSetView = false;
	bool bFoundTimeScale = false;

	for( int i = m_EventQueue.Head(); i != m_EventQueue.InvalidIndex(); )
	{
		KeyValues *pCurEvent = m_EventQueue[ i ];
		const int nType = pCurEvent->GetInt( "type" );

		bool bDitchEvent = false;
		bool bSetupCut = false;

		// Determine whether we should record the event or not
		if ( nType == EVENTTYPE_CHANGEPLAYER )
		{
			bDitchEvent = bFoundPlayerChange;
			bFoundPlayerChange = true;
		}
		else if ( IsCameraChangeEvent( nType ) )
		{
			bDitchEvent = bFoundCameraChange;
			bFoundCameraChange = true;
		}
		else if ( nType == EVENTTYPE_CAMERA_SETVIEW )
		{
			bDitchEvent = bFoundSetView;
			bFoundSetView = true;
			bSetupCut = true;	// If we end up keeping this event, it should be a cut.
		}
		else if ( nType == EVENTTYPE_TIMESCALE )
		{
			bDitchEvent = bFoundTimeScale;
			bFoundTimeScale = true;
		}

		// Setup as cut
		if ( bSetupCut )
		{
			pCurEvent->SetInt( "cut", 1 );
		}

		int itNext = m_EventQueue.Next( i );

		if ( bDitchEvent )
		{
#if _DEBUG
			CUtlBuffer buf;
			pCurEvent->RecursiveSaveToFile( buf, 1 );
			IF_REPLAY_DBG( Warning( "Ditching event of type %s\n...", ( const char * )buf.Base() ) );
#endif

			// Free the event
			pCurEvent->deleteThis();
			m_EventQueue.Remove( i );
		}

		i = itNext;
	}
}

void CPerformanceController::AddEvent( KeyValues *pEvent )
{
	IF_REPLAY_DBG2(
		CUtlBuffer buf;
	pEvent->RecursiveSaveToFile( buf, 1 );
	Warning( "Recording event:\n%s\n", ( const char * )buf.Base() );
	);
	m_pRoot->AddSubKey( pEvent );
}

void CPerformanceController::AddEvent_Camera_Change_FirstPerson( float flTime, int nEntityIndex )
{
	QUEUE_OR_RECORD_EVENT( pEvent, flTime, EVENTTYPE_CAMERA_CHANGE_FIRSTPERSON );
	pEvent->SetInt( "ent", nEntityIndex );
}

void CPerformanceController::AddEvent_Camera_Change_ThirdPerson( float flTime, int nEntityIndex )
{
	QUEUE_OR_RECORD_EVENT( pEvent, flTime, EVENTTYPE_CAMERA_CHANGE_THIRDPERSON );
	pEvent->SetInt( "ent", nEntityIndex );
}

void CPerformanceController::AddEvent_Camera_Change_Free( float flTime )
{
	QUEUE_OR_RECORD_EVENT( pEvent, flTime, EVENTTYPE_CAMERA_CHANGE_FREE );
}

void CPerformanceController::AddEvent_Camera_ChangePlayer( float flTime, int nEntIndex )
{
	QUEUE_OR_RECORD_EVENT( pEvent, flTime, EVENTTYPE_CHANGEPLAYER );
	pEvent->SetInt( "ent", nEntIndex );
}

void CPerformanceController::AddEvent_Camera_SetView( const SetViewParams_t &params )
{
	QUEUE_OR_RECORD_EVENT( pEvent, params.m_flTime, EVENTTYPE_CAMERA_SETVIEW );
	pEvent->SetString( "pos", Replay_va( "%f %f %f", params.m_pOrigin->x, params.m_pOrigin->y, params.m_pOrigin->z ) );
	pEvent->SetString( "ang", Replay_va( "%f %f %f", params.m_pAngles->x, params.m_pAngles->y, params.m_pAngles->z ) );
	pEvent->SetFloat( "fov", params.m_flFov );
	pEvent->SetFloat( "a", params.m_flAccel );
	pEvent->SetFloat( "s", params.m_flSpeed );
	pEvent->SetFloat( "rf", params.m_flRotationFilter );
}

void CPerformanceController::AddEvent_TimeScale( float flTime, float flScale )
{
	QUEUE_OR_RECORD_EVENT( pEvent, flTime, EVENTTYPE_TIMESCALE );
	pEvent->SetFloat( "scale", flScale );

	m_flTimeScale = flScale;
}

//----------------------------------------------------------------------------------------

bool CPerformanceController::SetupPlaybackHandler()
{
	IReplayPerformancePlaybackHandler *pHandler = g_pClient->GetPerformancePlaybackHandler();
	if ( !pHandler )
		return false;

	// Cache
	m_pPlaybackHandler = pHandler;
	
	return true;
}

void CPerformanceController::FinishBeginPerformancePlayback()
{
	// Root should be setup by now
	Assert( m_pRoot );

	// Make sure the camera isn't setup for camera override
	// TODO: Definitely need this here?
	g_pClient->GetReplayCamera()->ClearOverrideView();

	// Set to initial event
	m_pCurEvent = m_pRoot->GetFirstTrueSubKey();

	IF_REPLAY_DBG(
		m_pDbgRoot = m_pRoot->MakeCopy();
	);

	m_nState = STATE_PLAYING;
	m_bViewOverrideMode = false;
}

void CPerformanceController::SetupPlaybackExistingStream()
{
	// m_pRoot can be NULL here if the user is watching the original replay and has rewound
	// without changing anything.
	if ( !m_pRoot )
		return;

	if ( !SetupPlaybackHandler() )
		return;

	FinishBeginPerformancePlayback();
}

void CPerformanceController::SetupPlaybackFromPerformance( CReplayPerformance *pPerformance )
{
	AssertMsg( !m_pSavedPerformance, "This probably hit because either SaveNow() or Discard() were not called.  One of those should always be called on disconnect after watching or editing a replay." );
	AssertMsg( !m_pScratchPerformance, "Scratch performance should be NULL here." );

	if ( !pPerformance )
		return;

	if ( !pPerformance->m_pReplay )
	{
		AssertMsg( 0, "Performance passed in with an invalid replay pointer!  This bad!" );
		return;
	}

	if ( !SetupPlaybackHandler() )
		return;

	// Cache off the performance and replay for playback
	m_pSavedPerformance = pPerformance;
	m_pScratchPerformance = NULL;
	m_hReplay = pPerformance->m_pReplay->GetHandle();

	// Read the file
	Assert( !m_pRoot );
	const char *pFilename = pPerformance->GetFullPerformanceFilename();
	m_pRoot = new KeyValues( pFilename );
	if ( !m_pRoot->LoadFromFile( g_pFullFileSystem, pFilename ) )
	{
		Warning( "Failed to load replay file, \"%s\"!\n", pFilename );
		return;
	}

	FinishBeginPerformancePlayback();
}

void CPerformanceController::ReadSetViewEvent( KeyValues *pEventSubKey, Vector &origin, QAngle &angles, float &fov,
		  									   float *pAccel, float *pSpeed, float *pRotFilter )
{
	const char *pViewStr[2];

	pViewStr[0] = pEventSubKey->GetString( "pos" );
	pViewStr[1] = pEventSubKey->GetString( "ang" );

	sscanf( pViewStr[0], "%f %f %f", &origin.x, &origin.y, &origin.z );
	sscanf( pViewStr[1], "%f %f %f", &angles.x, &angles.y, &angles.z );
	fov = pEventSubKey->GetFloat( "fov", 90 );

	if ( pAccel && pSpeed && pRotFilter )
	{
		*pAccel = pEventSubKey->GetFloat( "a" );
		*pSpeed = pEventSubKey->GetFloat( "s" );
		*pRotFilter = pEventSubKey->GetFloat( "rf" );
	}
}

void CPerformanceController::PlaybackThink()
{
	static Vector aOrigin[3];
	static QAngle aAngles[3];
	static float aFov[3];
	float flAccel = 0.0f, flSpeed = 0.0f, flRotFilter = 0.0f;

	KeyValues *pSearch = NULL;
	float t;

	if ( !IsPlaying() )
		return;

	if ( !m_pCurEvent )
		return;

	if ( !m_pPlaybackHandler )
		return;

	CReplay *pReplay = GetReplay( m_hReplay );
	if ( !pReplay )
		return;

	const CGlobalVarsBase *g_pClientGlobalVariables = g_pEngineClient->GetClientGlobalVars();

	const int nReplaySpawnTick = pReplay->m_nSpawnTick;
	Assert( nReplaySpawnTick >= 0 );
	const float flCurTime = g_pClientGlobalVariables->curtime - g_pEngine->TicksToTime( nReplaySpawnTick );

	float flEventTime = 0;
	bool bShouldCut = false;

	while ( 1 )
	{
		// Get event time
		flEventTime = GetTime();

		// Get out if this event shouldn't fire yet
		if ( flEventTime > flCurTime )
			break;

		IF_REPLAY_DBG2(
			CUtlBuffer buf;
			m_pCurEvent->RecursiveSaveToFile( buf, 1 );
			Warning( "%s\n", ( const char * )buf.Base() );
		);

		switch ( m_pCurEvent->GetInt( "type", EVENTTYPE_INVALID ) )
		{
		case EVENTTYPE_CAMERA_CHANGE_FIRSTPERSON:
			m_bViewOverrideMode = false;
			m_pPlaybackHandler->OnEvent_Camera_Change_FirstPerson( flEventTime, m_pCurEvent->GetInt( "ent" ) );
			break;

		case EVENTTYPE_CAMERA_CHANGE_THIRDPERSON:
			m_bViewOverrideMode = true;
			m_pPlaybackHandler->OnEvent_Camera_Change_ThirdPerson( flEventTime, m_pCurEvent->GetInt( "ent" ) );
			break;

		case EVENTTYPE_CAMERA_CHANGE_FREE:
			m_bViewOverrideMode = true;
			m_pPlaybackHandler->OnEvent_Camera_Change_Free( flEventTime );
			break;

		case EVENTTYPE_CHANGEPLAYER:
			m_pPlaybackHandler->OnEvent_Camera_ChangePlayer( flEventTime, m_pCurEvent->GetInt( "ent" ) );
			break;

		case EVENTTYPE_CAMERA_SETVIEW:
			AssertMsg( m_bViewOverrideMode, "Camera mode needs to be set before a setview can take effect." );

			if ( m_bViewOverrideMode )
			{
				// Get sample for current time
				ReadSetViewEvent( m_pCurEvent, aOrigin[0], aAngles[0], aFov[0], &flAccel, &flSpeed, &flRotFilter );
//				g_pEngineClient->Con_NPrintf( 0, "sample 0 time: %f", flEventTime );
				m_flLastCamSetViewTime = flEventTime;
				m_pSetViewEvent = m_pCurEvent;	// Stomp any previous set view - we want the last one
				bShouldCut = bShouldCut || m_pCurEvent->GetBool( "cut" );	// We cut if any set-view event cuts, otherwise we will interpolate
			}
			break;

		case EVENTTYPE_TIMESCALE:
			m_flTimeScale = m_pCurEvent->GetFloat( "scale" );
			m_pPlaybackHandler->OnEvent_TimeScale( flEventTime, m_flTimeScale );
			break;

		default:
			AssertMsg( 0, "Unknown event in performance playback!\n" );
			Warning( "Unknown event in performance playback!\n" );
		}

		// Get next event (or NULL if there isn't one)
		m_pCurEvent = m_pCurEvent->GetNextTrueSubKey();

		// Get out if no more events
		if ( !m_pCurEvent )
			break;
	}

	// If in override mode, interpolate and setup camera
	if ( m_bViewOverrideMode && m_pSetViewEvent )
	{
		if ( bShouldCut )
		{
			DBG2( "CUT\n" );
			aOrigin[2] = aOrigin[0];
			aAngles[2] = aAngles[0];
			aFov[2] = aFov[0];
		}
		else
		{
			// Default second sample to first, in case we don't find a sample to interpolate with
			aOrigin[1] = aOrigin[0];
			aAngles[1] = aAngles[0];
			aFov[1] = aFov[0];

			// Parameter for interpolation
			t = 0.0f;

			// Seek forward to half a second from current event time and see if there
			// are any other set view events.
			pSearch = m_pSetViewEvent->GetNextTrueSubKey();
			while ( pSearch )
			{
				// Another sample not available
				float flSearchTime = atof( pSearch->GetName() );
				if ( flSearchTime > m_flLastCamSetViewTime + 0.5f )
					break;

				if ( pSearch->GetInt( "type", EVENTTYPE_INVALID ) == EVENTTYPE_CAMERA_SETVIEW )
				{
					// Found next sample within half a second - calc interpolation parameter & get data
					float flDiff = flSearchTime - m_flLastCamSetViewTime;
					Assert( flDiff > 0.0f );
					if ( flDiff > 0.0f )
					{
						t = clamp ( ( flCurTime - m_flLastCamSetViewTime ) / flDiff, 0.0f, 1.0f );

						// If the next set-view is a cut, we don't want to interpolate
						if ( pSearch->GetBool( "cut" ) )
						{
							const int iSrc = clamp( (int)( .5f + t ), 0, 1 );	// Round t to 0 or 1, so we set the camera to the current frame if t < 0.5f, and we set the camera to the 'cut'/next frame if t >= 0.5.
							aOrigin[2] = aOrigin[ iSrc ];
							aAngles[2] = aAngles[ iSrc ];
							aFov[2] = aFov[ iSrc ];
						}
						else
						{
							ReadSetViewEvent( pSearch, aOrigin[1], aAngles[1], aFov[1], &flAccel, &flSpeed, &flRotFilter );
						}
					}
					break;
				}

				pSearch = pSearch->GetNextTrueSubKey();
			}

			// Interpolate
			aOrigin[2] = Lerp( t, aOrigin[0], aOrigin[1] );
			aAngles[2] = Lerp( t, aAngles[0], aAngles[1] );	// NOTE: Calls QuaternionSlerp() internally
			aFov[2] = Lerp( t, aFov[0], aFov[1] );
		}

		// Setup current view
		SetViewParams_t params( flEventTime, &aOrigin[2], &aAngles[2], aFov[2], flAccel, flSpeed, flRotFilter );
		m_pPlaybackHandler->OnEvent_Camera_SetView( params );
	}

	IF_REPLAY_DBG( DebugRender() );
}

void CPerformanceController::DebugRender()
{
	KeyValues *pIt = m_pDbgRoot->GetFirstTrueSubKey();

	Vector prevpos, pos;
	QAngle angles;
	float fov;
	bool bPrev = false;

	g_pDebugOverlay->ClearDeadOverlays();

	while ( pIt )
	{
		if ( pIt->GetInt( "type", EVENTTYPE_INVALID ) == EVENTTYPE_CAMERA_SETVIEW )
		{
			ReadSetViewEvent( pIt, pos, angles, fov, NULL, NULL, NULL );

			// Skip first view event since no previous
			if ( !bPrev )
			{
				bPrev = true;
			}
			else
			{
				const bool bCut = pIt->GetBool( "cut" );
				const int r = bCut ? 0 : 255;
				const int g = bCut ? 255 : 0;
				const int b = 0;
				Vector tickpos = pos + Vector(10,0,0);
				g_pDebugOverlay->AddLineOverlay( prevpos, pos, r, g, b, true, 0.0f );
				g_pDebugOverlay->AddLineOverlay( pos, tickpos, 0, 255, 255, true, 0.0f );
			}

			prevpos = pos;
		}

		pIt = pIt->GetNextTrueSubKey();
	}
}

//----------------------------------------------------------------------------------------

void CPerformanceController::Think()
{
	VPROF_BUDGET( "CReplayPerformancePlayer::Think", VPROF_BUDGETGROUP_REPLAY );

	CBaseThinker::Think();

	PlaybackThink();
}

float CPerformanceController::GetNextThinkTime() const
{
	return 0.0f;
}

//----------------------------------------------------------------------------------------