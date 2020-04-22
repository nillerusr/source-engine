//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cl_replaymanager.h"
#include "replay/ienginereplay.h"
#include "replay/iclientreplay.h"
#include "replay/ireplaymoviemanager.h"
#include "replay/ireplayfactory.h"
#include "replay/replayutils.h"
#include "replay/ireplaymovierenderer.h"
#include "replay/shared_defs.h"
#include "baserecordingsession.h"
#include "cl_screenshotmanager.h"
#include "cl_recordingsession.h"
#include "cl_recordingsessionblock.h"
#include "replaysystem.h"
#include "cl_replaymoviemanager.h"
#include "replay_dbg.h"
#include "inetchannel.h"
#include "cl_replaycontext.h"
#include <time.h>
#include "vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

extern IEngineClientReplay *g_pEngineClient;
extern ConVar replay_postdeathrecordtime;

//----------------------------------------------------------------------------------------

#define REPLAY_INDEX_VERSION	0

//----------------------------------------------------------------------------------------

CReplayManager::CReplayManager()
:	m_pPendingReplay( NULL ),
	m_pReplayLastLife( NULL ),
	m_pReplayThisLife( NULL ),
	m_flPlayerSpawnCreateReplayFailTime( 0.0f )
{
}

CReplayManager::~CReplayManager()
{
}

bool CReplayManager::Init( CreateInterfaceFn fnCreateFactory )
{
	// Get out if the user is running an unsupported mod or platform
	if ( !g_pEngine->IsSupportedModAndPlatform() )
		return false;

	// Clear anything already loaded (since we reuse the same instance)
	Clear();

	// Register replay factory
	m_pReplayFactory = GetReplayFactory( fnCreateFactory );		Assert( m_pReplayFactory );

	// Load all replays from disk
	if ( !BaseClass::Init() )
	{
		Warning( "Failed to load replay history!\n" );
	}

	// Session manager init'd by this point - go through and link up replays to sessions
	CL_GetRecordingSessionManager()->OnReplaysLoaded();

	return true;
}

void CReplayManager::Shutdown()
{
	// Get out if the user is running an unsupported mod or platform
	if ( !g_pEngine->IsSupportedModAndPlatform() )
		return;

	// Make sure we aren't waiting to write
	BaseClass::Shutdown();	// Saves
}

IReplayFactory *CReplayManager::GetReplayFactory( CreateInterfaceFn fnCreateFactory )
{
	return (IReplayFactory *)fnCreateFactory( INTERFACE_VERSION_REPLAY_FACTORY, NULL );
}

void CReplayManager::OnSessionStart()
{
	// The pending replay doesn't exist yet at this point as far as I've seen, since the "replay_sessioninfo"
	// event comes down a frame or more after the "replay_recording" replicated cvar is set to 1, which is
	// what triggers AttemptToSetupNewReplay().
	if ( !m_pPendingReplay )
	{
		AttemptToSetupNewReplay();
	}

	if ( m_pPendingReplay )
	{
		// Link up the pending replay to the recording session in progress
		if ( m_pPendingReplay->m_hSession == REPLAY_HANDLE_INVALID )
		{
			ReplayHandle_t hSessionInProgress = CL_GetRecordingSessionManager()->GetRecordingSessionInProgress()->GetHandle();
			m_pPendingReplay->m_hSession = hSessionInProgress;
		}

		// Make sure the spawn tick has the proper server start tick subtracted out
		if ( m_pPendingReplay->m_nSpawnTick < 0 )
		{
			const int nServerStartTick = CL_GetRecordingSessionManager()->m_ServerRecordingState.m_nStartTick;	Assert( nServerStartTick > 0 );
			m_pPendingReplay->m_nSpawnTick = MAX( 0, -m_pPendingReplay->m_nSpawnTick - nServerStartTick );
		}
	}
}

void CReplayManager::OnSessionEnd()
{
	// Complete the pending replay, if there is one
	CompletePendingReplay();
}

const char *CReplayManager::GetRelativeIndexPath() const
{
	return Replay_va( "%s%c", SUBDIR_REPLAYS, CORRECT_PATH_SEPARATOR );
}

CReplay *CReplayManager::Create()
{
	return m_pReplayFactory->Create();
}

IReplayContext *CReplayManager::GetReplayContext() const
{
	return g_pClientReplayContextInternal;
}

bool CReplayManager::ShouldLoadObj( const CReplay *pReplay ) const
{
	return pReplay && pReplay->m_bComplete;
}

void CReplayManager::OnObjLoaded( CReplay *pReplay )
{
	if ( !pReplay )
		return;

	pReplay->m_bSavedDuringThisSession = false;
}

int CReplayManager::GetVersion() const
{
	return REPLAY_INDEX_VERSION;
}

void CReplayManager::ClearPendingReplay()
{
	m_pPendingReplay = NULL;
}

void CReplayManager::SanityCheckReplay( CReplay *pReplay )
{
	if ( !pReplay )
		return;

	// DEBUG: Make sure this replay does not already exist in the list
	FOR_EACH_VEC( Replays(), i )
	{
		if ( Replays()[ i ]->GetHandle() == pReplay->GetHandle() )
		{
			IF_REPLAY_DBG( Warning( "Replay %i already found in history!\n", pReplay->GetHandle() ) );
		}
	}

	if ( pReplay->m_nDeathTick < pReplay->m_nSpawnTick )
	{
		IF_REPLAY_DBG( Warning( "Spawn tick (%i) is greater than death tick (%i)!\n", pReplay->m_nSpawnTick, pReplay->m_nDeathTick ) );
	}
}

void CReplayManager::SaveDanglingReplay()
{
	if ( !m_pReplayThisLife )
		return;

	if ( m_pReplayThisLife->m_bRequestedByUser )
	{
		CompletePendingReplay();
		FlagReplayForFlush( m_pReplayThisLife, false );
	}
}

void CReplayManager::FreeLifeIfNotSaved( CReplay *&pReplay )
{
	if ( pReplay )
	{
		if ( !pReplay->m_bSaved && !IsDirty( pReplay ) )
		{
			CleanupReplay( pReplay );		
		}
		else
		{
			// If it's been saved, don't free the memory, just clear the pointer
			pReplay = NULL;
		}
	}
}

void CReplayManager::CleanupReplay( CReplay *&pReplay )
{
	if ( !pReplay )
		return;

	// Get rid of a replay that was never committed:
	// Remove screenshots taken
	CL_GetScreenshotManager()->DeleteScreenshotsForReplay( pReplay );

	// Free
	delete pReplay;
	pReplay = NULL;
}

void CReplayManager::OnReplayRecordingCvarChanged()
{
	DBG( "OnReplayRecordingCvarChanged()\n" );

	// If set to 0, get out - we don't care
	extern ConVar replay_recording;
	if ( !replay_recording.GetBool() )
	{
		DBG( "   replay_recording is false...aborting\n" );
		return;
	}

	// If OnPlayerSpawn() hasn't failed to create the scratch replay, get out
	if ( m_flPlayerSpawnCreateReplayFailTime == 0.0f )
	{
		DBG( "   m_flPlayerSpawnCreateReplayFailTime == 0.0f...aborting.\n" );
		return;
	}

	DBG( "   Calling AttemptToSetupNewReplay()\n" );

	// Try to create & setup again
	AttemptToSetupNewReplay();

	// Reset
	m_flPlayerSpawnCreateReplayFailTime = 0.0f;
}

void CReplayManager::OnClientSideDisconnect()
{
	SaveDanglingReplay();
	ClearPendingReplay();

	FreeLifeIfNotSaved( m_pReplayLastLife );
	FreeLifeIfNotSaved( m_pReplayThisLife );

	m_flPlayerSpawnCreateReplayFailTime = 0.0f;
}

void CReplayManager::CommitPendingReplayAndBeginDownload()
{
	// Update the last session block we should download
	CClientRecordingSession *pSession = CL_CastSession( CL_GetRecordingSessionManager()->FindSession( m_pReplayThisLife->m_hSession ) );
	const int iPostDeathBlockIndex = pSession->UpdateLastBlockToDownload();

	// Update the # of blocks required to reconstruct the replay
	m_pReplayThisLife->m_iMaxSessionBlockRequired = iPostDeathBlockIndex;

	Commit( m_pReplayThisLife );
}

void CReplayManager::CompletePendingReplay()
{
	// Get out if no pending replay
	if ( !m_pPendingReplay )
		return;

	// Get session associated w/ the replay
	CBaseRecordingSession *pSession = CL_GetRecordingSessionManager()->FindSession( m_pPendingReplay->m_hSession );
	
	// Sometimes the session isn't valid here, like when we're first joining a server
	if ( !pSession )
		return;

	Assert( pSession->m_nServerStartRecordTick >= 0 );

	// Cache death tick
	m_pPendingReplay->m_nDeathTick = g_pEngineClient->GetLastServerTickTime() - pSession->m_nServerStartRecordTick;

	SanityCheckReplay( m_pPendingReplay );

	// Calc replay length
	m_pPendingReplay->m_flLength = g_pEngine->TicksToTime(
		m_pPendingReplay->m_nDeathTick -
		m_pPendingReplay->m_nSpawnTick +
		g_pEngine->TimeToTicks( replay_postdeathrecordtime.GetFloat() )
	);

	// Cache player slot so we can start playback of replays from recorder player's perspective
	m_pPendingReplay->m_nPlayerSlot = g_pEngineClient->GetPlayerSlot() + 1;

	// Setup status
	m_pPendingReplay->m_nStatus = CReplay::REPLAYSTATUS_DOWNLOADPHASE;

	// The replay is now "complete," ie has all the data needed
	m_pPendingReplay->m_bComplete = true;

	// Let derived classes do whatever it wants
	m_pPendingReplay->OnComplete();

	// If the replay was requested by the user already, update the # of blocks we should download & commit the replay
	if ( m_pPendingReplay->m_bRequestedByUser )
	{
#ifdef DBGFLAG_ASSERT
		Assert( m_pReplayThisLife->m_bComplete );
#endif
		CommitPendingReplayAndBeginDownload();
	}

	// Before we copy the pointer to "this life," end recording so the replay can do any cleanup (eg listening for game events)
	m_pPendingReplay->OnEndRecording();

	// Cache off scratch replay to "this life"
	m_pReplayThisLife = m_pPendingReplay;

	ClearPendingReplay();
}

bool CReplayManager::Commit( CReplay *pNewReplay )
{
	if ( !g_pClientReplayContextInternal->IsInitialized() || !pNewReplay )
		return false;

	SanityCheckReplay( pNewReplay );

	// NOTE: Marks index as dirty, as well as pNewReplay
	Add( pNewReplay );

	// Save now
	Save();

	return true;
}

//
// IReplayManager implementation
//
CReplay	*CReplayManager::GetReplay( ReplayHandle_t hReplay )
{
	if ( m_pReplayThisLife && m_pReplayThisLife->GetHandle() == hReplay )
		return m_pReplayThisLife;

	return Find( hReplay );
}

void CReplayManager::DeleteReplay( ReplayHandle_t hReplay, bool bNotifyUI )
{
	CReplay *pReplay = GetReplay( hReplay );	Assert( pReplay );

	// The session manager will delete the .dem, the session .dmx and remove the session
	// item itself if this is the last replay associated with it.
	CL_GetRecordingSessionManager()->OnReplayDeleted( pReplay );

	// Notify the replay browser if necessary
	if ( bNotifyUI )
	{
		extern IClientReplay *g_pClient;
		g_pClient->OnDeleteReplay( hReplay );
	}

	// Remove it
	Remove( pReplay );

	// If the replay deleted was just saved and we haven't respawned yet,
	// we need to clear out some stuff so GetReplay() doesn't crash.
	if ( m_pReplayThisLife == pReplay )
	{
		m_pReplayThisLife = NULL;
		m_pPendingReplay = NULL;
	}

	if ( m_pReplayLastLife == pReplay )
	{
		m_pReplayLastLife = NULL;
	}
}

void CReplayManager::FlagReplayForFlush( CReplay *pReplay, bool bForceImmediate )
{
	FlagForFlush( pReplay, bForceImmediate );
}

int CReplayManager::GetUnrenderedReplayCount()
{
	int nCount = 0;
	FOR_EACH_VEC( m_vecObjs, i )
	{
		CReplay *pCurReplay = m_vecObjs[ i ];
		if ( !pCurReplay->m_bRendered &&
			  pCurReplay->m_nStatus == CReplay::REPLAYSTATUS_READYTOCONVERT )
		{
			++nCount;
		}
	}
	return nCount;
}

void CReplayManager::InitReplay( CReplay *pReplay )
{
	// Setup record time right now
	pReplay->m_RecordTime.InitDateAndTimeToNow();

	// Store start time
	pReplay->m_flStartTime = g_pEngine->GetHostTime();

	// Get map name (w/o the path)
	V_FileBase( g_pEngineClient->GetLevelName(), m_pPendingReplay->m_szMapName, sizeof( m_pPendingReplay->m_szMapName ) );

	// Give the replay a default name
	pReplay->AutoNameTitleIfEmpty();
}

CReplay *CReplayManager::CreatePendingReplay()
{
	Assert( m_pPendingReplay == NULL );
	m_pPendingReplay = CreateAndGenerateHandle();

	// If we've already begun recording, link to the session now, otherwise link once
	// we start recording.
	CBaseRecordingSession *pSessionInProgress = CL_GetRecordingSessionInProgress();
	if ( pSessionInProgress )
	{
		m_pPendingReplay->m_hSession = pSessionInProgress->GetHandle();
	}

	InitReplay( m_pPendingReplay );

	// Setup replay handle for screenshots
	CL_GetScreenshotManager()->SetScreenshotReplay( m_pPendingReplay->GetHandle() );

	return m_pPendingReplay;
}

void CReplayManager::AttemptToSetupNewReplay()
{
	DBG( "AttemptToSetupNewReplay()\n" );

	if ( !g_pReplay->IsRecording() || g_pEngineClient->IsPlayingReplayDemo() )
	{
		DBG( "   Aborting...not recording, or playing back replay.\n" );
		m_flPlayerSpawnCreateReplayFailTime = g_pEngine->GetHostTime();
		return;
	}

	// Create the replay if necessary - we only do setup if we're creating
	// a new replay, because on a full update this function may be called
	// even though we're not actually spawning.
	if ( !m_pPendingReplay )
	{
		DBG( "   Creating new replay...\n" );

		// If there is a "last life" replay that was not saved already, delete it
		FreeLifeIfNotSaved( m_pReplayLastLife );

		// Cache last life
		m_pReplayLastLife = m_pReplayThisLife;

		// Create the scratch replay (sets m_pPendingReplay and returns it)
		CReplay *pPendingReplay = CreatePendingReplay();

		SanityCheckReplay( pPendingReplay );

		// "This life" is the scratch replay
		m_pReplayThisLife = pPendingReplay;

		// Setup spawn tick
		const int nServerStartTick = CL_GetRecordingSessionManager()->m_ServerRecordingState.m_nStartTick;
		pPendingReplay->m_nSpawnTick = g_pEngineClient->GetLastServerTickTime() - nServerStartTick;
		if ( nServerStartTick == 0 )
		{
			// Didn't receive the replay_sessioninfo event yet - make spawn tick negative so when the
			// event IS received, we can detect that the server start tick still needs to be subtracted.
			pPendingReplay->m_nSpawnTick *= -1;
		}

		// Setup post-death record time
		extern ConVar replay_postdeathrecordtime;
		pPendingReplay->m_nPostDeathRecordTime = replay_postdeathrecordtime.GetFloat();

		// Let the replay know we're recording
		pPendingReplay->OnBeginRecording();
	}
	else
	{
		DBG( "   NOT creating new replay.\n" );
	}

	// Served its purpose
	m_flPlayerSpawnCreateReplayFailTime = 0.0f;
}

void CReplayManager::Think()
{
	VPROF_BUDGET( "CReplayManager::Think", VPROF_BUDGETGROUP_REPLAY );

	BaseClass::Think();

	DebugThink();

	// Only update pending replay, since it's recording
	// NOTE: we use Sys_FloatTime() here, since the client sets the next update time with engine->Time(),
	// which also uses Sys_FloatTime().
	if ( m_pPendingReplay && m_pPendingReplay->m_flNextUpdateTime <= Sys_FloatTime() )
	{
		m_pPendingReplay->Update();		// Allow the replay's Update() function to set the next update time
	}
}

void CReplayManager::DebugThink()
{
	// Debugging
	if ( replay_debug.GetBool() )
	{
		const char *pReplayNames[] = { "Scratch", "This life", "Last life" };
		CReplay *pReplays[] = { m_pPendingReplay, m_pReplayThisLife, m_pReplayLastLife };
		for ( int i = 0; i < 3; ++i )
		{
			CReplay *pCurReplay = pReplays[ i ];
			if ( !pCurReplay )
			{
				g_pEngineClient->Con_NPrintf( i, "%s: NULL", pReplayNames[ i ] );
				continue;
			}

			g_pEngineClient->Con_NPrintf( i, "%s:  handle=%i  [%i, %i]  C? %s R? %s  MaxBlock: %i", pReplayNames[ i ],
				pCurReplay->GetHandle(), pCurReplay->m_nSpawnTick,
				pCurReplay->m_nDeathTick, pCurReplay->m_bComplete ? "YES" : "NO",
				pCurReplay->m_bRequestedByUser ? "YES" : "NO",
				pCurReplay->m_iMaxSessionBlockRequired
			);
		
			// Screenshot handle
			int nCurLine = 5;
			g_pEngineClient->Con_NPrintf( nCurLine, "Screenshot replay: handle=%i", CL_GetScreenshotManager()->GetScreenshotReplay() );
			nCurLine += 2;

			// Saved replay handles
			g_pEngineClient->Con_NPrintf( nCurLine++, "REPLAYS:" );
			FOR_EACH_REPLAY( j )
			{
				CReplay *pReplay = GET_REPLAY_AT( j );
				g_pEngineClient->Con_NPrintf( nCurLine++, "%i: handle=%i  ticks=[%i %i]", i, pReplay->GetHandle(),
					pReplay->m_nSpawnTick, pReplay->m_nDeathTick );
			}

			// Current tick:
			g_pEngineClient->Con_NPrintf( ++nCurLine, "MAIN tick: %f", g_pEngineClient->GetLastServerTickTime() );
			g_pEngineClient->Con_NPrintf( ++nCurLine, "   server tick: %f", g_pEngineClient->GetLastServerTickTime() );
			nCurLine += 2;
		}
	}
}

float CReplayManager::GetNextThinkTime() const
{
	return g_pEngine->GetHostTime() + 0.1f;
}

CReplay *CReplayManager::GetPlayingReplay()
{
	return g_pReplayDemoPlayer->GetCurrentReplay();
}

CReplay *CReplayManager::GetReplayForCurrentLife()
{
	return m_pReplayThisLife;
}

void CReplayManager::GetReplays( CUtlLinkedList< CReplay *, int > &lstReplays )
{
	lstReplays.RemoveAll();
	FOR_EACH_REPLAY( i )
	{
		lstReplays.AddToTail( GET_REPLAY_AT( i ) );
	}
}

void CReplayManager::GetReplaysAsQueryableItems( CUtlLinkedList< IQueryableReplayItem *, int > &lstReplays )
{
	lstReplays.RemoveAll();
	FOR_EACH_REPLAY( i )
	{
		lstReplays.AddToHead( dynamic_cast< IQueryableReplayItem * >( GET_REPLAY_AT( i ) ) );
	}

	if ( m_pPendingReplay &&
	    !m_pPendingReplay->m_bComplete &&
		 m_pPendingReplay->m_bRequestedByUser )
	{
		Assert( lstReplays.Find( m_pPendingReplay ) == lstReplays.InvalidIndex() );
		lstReplays.AddToHead( m_pPendingReplay );
	}
}

int CReplayManager::GetNumReplaysDependentOnSession( ReplayHandle_t hSession )
{
	int nResult = 0;
	FOR_EACH_REPLAY( i )
	{
		CReplay *pCurReplay = GET_REPLAY_AT( i );
		if ( pCurReplay->m_hSession == hSession )
		{
			++nResult;
		}
	}
	return nResult;
}

const char *CReplayManager::GetReplaysDir() const
{
	return GetIndexPath();
}
	
float CReplayManager::GetDownloadProgress( const CReplay *pReplay )
{
	// Give each downloadable session block equal weight since we won't know the size of blocks that
	// have not been created/written yet on the server.

	// Go through all blocks in the replay and figure out how many bytes have been downloaded
	float flSum = 0.0f;

	CClientRecordingSession *pSession = CL_CastSession( CL_GetRecordingSessionManager()->FindSession( pReplay->m_hSession ) );		Assert( pSession );
	if ( !pSession )
		return 0.0f;

	const CBaseRecordingSession::BlockContainer_t &vecBlocks = pSession->GetBlocks();
	FOR_EACH_VEC( vecBlocks, i )
	{
		CClientRecordingSessionBlock *pCurBlock = CL_CastBlock( vecBlocks[ i ] );
		if ( !pReplay->IsSignificantBlock( pCurBlock->m_iReconstruction ) )
			continue;

		// Calculate progress for this block
		Assert( pCurBlock->m_uFileSize > 0 );
		const float flSubProgress = pCurBlock->m_uFileSize == 0 ? 0.0f : clamp( (float)pCurBlock->m_uBytesDownloaded / pCurBlock->m_uFileSize, 0.0f, 1.0f );

		flSum += flSubProgress;
	}

	// Account for blocks that haven't been created yet
	// NOTE: This will cause a bug in download progress if the round ends and cuts the number of
	// expected blocks down - but that situation is probably less likely to occur than the situation
	// where a client is expecting more blocks that *will* be created.  To avoid pops in the latter
	// situation, we account for those blocks here.
	const int nTotalSubBlocks = pReplay->m_iMaxSessionBlockRequired + 1;

	// Calculate mean
	Assert( nTotalSubBlocks > 0 );
	return nTotalSubBlocks == 0 ? 0.0f : ( flSum / (float)nTotalSubBlocks );
}

//----------------------------------------------------------------------------------------
