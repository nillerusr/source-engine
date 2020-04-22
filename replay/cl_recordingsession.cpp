//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cl_recordingsession.h"
#include "cl_sessioninfodownloader.h"
#include "cl_recordingsessionmanager.h"
#include "cl_replaymanager.h"
#include "cl_recordingsessionblock.h"
#include "cl_sessionblockdownloader.h"
#include "replay/ienginereplay.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

extern IEngineReplay *g_pEngine;

//----------------------------------------------------------------------------------------

#define MAX_SESSION_INFO_DOWNLOAD_ATTEMPTS	3

//----------------------------------------------------------------------------------------

CClientRecordingSession::CClientRecordingSession( IReplayContext *pContext )
:	CBaseRecordingSession( pContext ),
	m_iLastBlockToDownload( -1 ),
	m_iGreatestConsecutiveBlockDownloaded( -1 ),
	m_nSessionInfoDownloadAttempts( 0 ),
	m_flLastUpdateTime( -1.0f ),
	m_pSessionInfoDownloader( NULL ),
	m_bTimedOut( false ),
	m_bAllBlocksDownloaded( false )
{
}

CClientRecordingSession::~CClientRecordingSession()
{
	delete m_pSessionInfoDownloader;
}

bool CClientRecordingSession::AllReplaysReconstructed() const
{
	FOR_EACH_LL( m_lstReplays, it )
	{
		const CReplay *pCurReplay = m_lstReplays[ it ];
		if ( !pCurReplay->HasReconstructedReplay() )
			return false;
	}

	return true;
}

void CClientRecordingSession::DeleteBlocks()
{
	// Only delete blocks if all replays have been reconstructed for this session
	if ( !AllReplaysReconstructed() )
		return;

	// Delete each block
	FOR_EACH_VEC( m_vecBlocks, i )
	{
		m_pContext->GetRecordingSessionBlockManager()->DeleteBlock( m_vecBlocks[ i ] );
	}

	// Clear out the list
	m_vecBlocks.RemoveAll();

	// Clear these out so we don't try to download the blocks again
	m_iLastBlockToDownload = -1;
	m_iGreatestConsecutiveBlockDownloaded = -1;
}

void CClientRecordingSession::SyncSessionBlocks()
{
	// If the last update time hasn't been initialized yet, initialize it now since this will be the first time
	// we are attempting to download the session info file.
	if ( m_flLastUpdateTime < 0.0f )
	{
		m_flLastUpdateTime = g_pEngine->GetHostTime();
	}

	Assert( !m_pSessionInfoDownloader );
	IF_REPLAY_DBG( Warning( "Downloading session info...\n" ) );
	m_pSessionInfoDownloader = new CSessionInfoDownloader();
	m_pSessionInfoDownloader->DownloadSessionInfoAndUpdateBlocks( this );
}

void CClientRecordingSession::OnReplayDeleted( CReplay *pReplay )
{
	m_lstReplays.FindAndRemove( pReplay );

	// This will load session blocks and delete them from disk if possible.  In the case
	// that all other replays for a session have already been reconstructed and pReplay
	// was the last replay that was never reconstructed, we should delete session's blocks now.
	// Note that these calls will only (a) load blocks if they aren't loaded already, and
	// (b) Delete blocks if all associated replays have been reconstructed.
	LoadBlocksForSession();
	DeleteBlocks();
}

bool CClientRecordingSession::Read( KeyValues *pIn )
{
	if ( !BaseClass::Read( pIn ) )
		return false;

	m_iLastBlockToDownload = pIn->GetInt( "last_block_to_download", -1 );
	m_iGreatestConsecutiveBlockDownloaded = pIn->GetInt( "last_consec_block_downloaded", -1 );
//	m_bTimedOut = pIn->GetBool( "timed_out" );
	m_uServerSessionID = pIn->GetUint64( "server_session_id" );
	m_bAllBlocksDownloaded = pIn->GetBool( "all_blocks_downloaded" );

	return true;
}

void CClientRecordingSession::Write( KeyValues *pOut )
{
	BaseClass::Write( pOut );

	pOut->SetInt( "last_block_to_download", m_iLastBlockToDownload );
	pOut->SetInt( "last_consec_block_downloaded", m_iGreatestConsecutiveBlockDownloaded );
//	pOut->SetInt( "timed_out", (int)m_bTimedOut );
	pOut->SetUint64( "server_session_id", m_uServerSessionID );
	pOut->SetInt( "all_blocks_downloaded", (int)m_bAllBlocksDownloaded );
}

void CClientRecordingSession::AdjustLastBlockToDownload( int iNewLastBlockToDownload )
{
	Assert( m_iLastBlockToDownload > iNewLastBlockToDownload );
	m_iLastBlockToDownload = iNewLastBlockToDownload;

	// Adjust any replays that refer to this session
	FOR_EACH_LL( m_lstReplays, i )
	{
		CReplay *pCurReplay = m_lstReplays[ i ];
		if ( pCurReplay->m_iMaxSessionBlockRequired > iNewLastBlockToDownload )
		{
			// Adjust replay
			pCurReplay->m_iMaxSessionBlockRequired = iNewLastBlockToDownload;
		}
	}
}

int CClientRecordingSession::UpdateLastBlockToDownload()
{
	// Here we calculate the block we'll need in order to reconstruct the replay at the post-death time,
	// based on replay_postdeathrecordtime, NOT the current time.  The index calculated here may be greater
	// than the actual last block the server writes, since the round may end or the map may change.  This
	// is adjusted for when we actually download the blocks.
	extern ConVar replay_postdeathrecordtime;
	CClientRecordingSessionManager::ServerRecordingState_t *pServerState = &CL_GetRecordingSessionManager()->m_ServerRecordingState;

	const int nCurBlock = pServerState->m_nCurrentBlock;
	const int nDumpInterval = pServerState->m_nDumpInterval;		Assert( nDumpInterval > 0 );
	const int nAddedBlocks = (int)ceil( replay_postdeathrecordtime.GetFloat() / nDumpInterval );	// Round up
	const int iPostDeathBlock = nCurBlock + nAddedBlocks;

	IF_REPLAY_DBG( Warning( "nCurBlock: %i\n", nCurBlock ) );
	IF_REPLAY_DBG( Warning( "nDumpInterval: %i\n", nDumpInterval ) );
	IF_REPLAY_DBG( Warning( "nAddedBlocks: %i\n", nAddedBlocks ) );
	IF_REPLAY_DBG( Warning( "iPostDeathBlock: %i\n", iPostDeathBlock ) );

	// Never assign less blocks than we already need
	m_iLastBlockToDownload = MAX( m_iLastBlockToDownload, iPostDeathBlock );

	CL_GetRecordingSessionManager()->FlagForFlush( this, false );
	
	IF_REPLAY_DBG( ConColorMsg( 0, Color(0,255,0), "Max block currently needed: %i\n", m_iLastBlockToDownload ) );

	return m_iLastBlockToDownload;
}

void CClientRecordingSession::Think()
{
	CBaseThinker::Think();

	// If the session info downloader's done and can be deleted, free it.
	if ( m_pSessionInfoDownloader &&
		 m_pSessionInfoDownloader->IsDone() &&
		 m_pSessionInfoDownloader->CanDelete() )
	{
		// Failure?
		if ( m_pSessionInfoDownloader->m_nError != CSessionInfoDownloader::ERROR_NONE )
		{
			// If there was an error, increment the error count and update the appropriate replays if
			// we've tried a sufficient number of times.
			++m_nSessionInfoDownloadAttempts;
			if ( m_nSessionInfoDownloadAttempts >= MAX_SESSION_INFO_DOWNLOAD_ATTEMPTS )
			{
				FOR_EACH_LL( m_lstReplays, i )
				{
					CReplay *pCurReplay = m_lstReplays[ i ];

					// If this replay has already been set to "ready to convert" state (or beyond), skip.
					if ( pCurReplay->m_nStatus >= CReplay::REPLAYSTATUS_READYTOCONVERT )
						continue;

					// Update status
					pCurReplay->m_nStatus = CReplay::REPLAYSTATUS_ERROR;

					// Display an error message
					ShowDownloadFailedMessage( pCurReplay );

					// Save now
					CL_GetReplayManager()->FlagReplayForFlush( pCurReplay, true );
				}
			}
		}

		IF_REPLAY_DBG( Warning( "...session info download complete.  Freeing.\n" ) );
		delete m_pSessionInfoDownloader;
		m_pSessionInfoDownloader = NULL;
	}
}

float CClientRecordingSession::GetNextThinkTime() const
{
	return g_pEngine->GetHostTime() + 0.5f;
}

void CClientRecordingSession::UpdateAllBlocksDownloaded()
{
	// We're only "done" if this session is no longer recording and all blocks are downloaded.
	const bool bOld = m_bAllBlocksDownloaded;
	m_bAllBlocksDownloaded = !m_bRecording && ( m_iGreatestConsecutiveBlockDownloaded >= m_iLastBlockToDownload );

	// Flag as modified if changed
	if ( bOld != m_bAllBlocksDownloaded )
	{
		CL_GetRecordingSessionManager()->FlagForFlush( this, false );
	}
}

void CClientRecordingSession::EnsureDownloadingEnabled()
{
	m_bAllBlocksDownloaded = false;
}

void CClientRecordingSession::UpdateGreatestConsecutiveBlockDownloaded()
{
	// Assumes m_vecBlocks is sorted in ascending order (for both reconstruction indices and handle, which should be parallel)
	int j = 0;
	int iGreatestConsecutiveBlockDownloaded = 0;
	FOR_EACH_VEC( m_vecBlocks, i )
	{
		CClientRecordingSessionBlock *pCurBlock = CL_CastBlock( m_vecBlocks[ i ] );

		AssertMsg( pCurBlock->m_iReconstruction == j, "Session blocks must be sorted!" );

		// If the block hasn't been downloaded, stop here
		if ( pCurBlock->m_nDownloadStatus != CClientRecordingSessionBlock::DOWNLOADSTATUS_DOWNLOADED )
			break;

		// Block has been downloaded - update the counter
		iGreatestConsecutiveBlockDownloaded = MAX( iGreatestConsecutiveBlockDownloaded, pCurBlock->m_iReconstruction );

		++j;
	}

	Assert( iGreatestConsecutiveBlockDownloaded >= 0 );
	Assert( iGreatestConsecutiveBlockDownloaded < m_vecBlocks.Count() );

	// Cache
	m_iGreatestConsecutiveBlockDownloaded = iGreatestConsecutiveBlockDownloaded;

	// Mark session as dirty
	CL_GetRecordingSessionManager()->FlagForFlush( this, false );
}

void CClientRecordingSession::UpdateReplayStatuses( CClientRecordingSessionBlock *pBlock )
{
	AssertMsg( m_vecBlocks.Find( pBlock ) != m_vecBlocks.InvalidIndex(), "Block doesn't belong to session or was not added" );

	// If the download was successful, update the greatest consecutive block downloaded index
	if ( pBlock->m_nDownloadStatus == CClientRecordingSessionBlock::DOWNLOADSTATUS_DOWNLOADED )
	{
		UpdateGreatestConsecutiveBlockDownloaded();
		UpdateAllBlocksDownloaded();
	}

	// Block in error state?
	const bool bFailed = pBlock->m_nDownloadStatus == CClientRecordingSessionBlock::DOWNLOADSTATUS_ERROR;

	// Go through all replays that refer to this session and update their status if necessary
	FOR_EACH_LL( m_lstReplays, i )
	{
		CReplay *pCurReplay = m_lstReplays[ i ];

		// If this replay has already been set to "ready to convert" state (or beyond), skip.
		if ( pCurReplay->m_nStatus >= CReplay::REPLAYSTATUS_READYTOCONVERT )
			continue;

		bool bFlush = false;

		// If the download failed and the block is required for this replay, mark as such
		if ( bFailed && pCurReplay->m_iMaxSessionBlockRequired >= pBlock->m_iReconstruction )
		{
			pCurReplay->m_nStatus = CReplay::REPLAYSTATUS_ERROR;
			bFlush = true;

			// Display an error message
			ShowDownloadFailedMessage( pCurReplay );
		}

		// Have we downloaded all blocks required for the given replay?
		else if ( !bFailed && pCurReplay->m_iMaxSessionBlockRequired <= m_iGreatestConsecutiveBlockDownloaded )
		{
			// Update replay's status and mark as dirty
			pCurReplay->m_nStatus = CReplay::REPLAYSTATUS_READYTOCONVERT;

			// Display a message on the client
			g_pClient->DisplayReplayMessage( "#Replay_DownloadComplete", false, false, "replay\\downloadcomplete.wav" );

			bFlush = true;
		}

		// Mark replay as dirty?
		if ( bFlush )
		{
			CL_GetReplayManager()->FlagForFlush( pCurReplay, false );
		}
	}
}

void CClientRecordingSession::OnDownloadTimeout()
{
	m_bTimedOut = true;

	// Go through all replays that refer to this session and update their status if necessary
	FOR_EACH_LL( m_lstReplays, i )
	{
		CReplay *pCurReplay = m_lstReplays[ i ];

		// If this replay has already been set to "ready to convert" state (or beyond), skip.
		if ( pCurReplay->m_nStatus >= CReplay::REPLAYSTATUS_READYTOCONVERT )
			continue;

		// Check to see if we have enough block info for the current replay
		if ( m_iGreatestConsecutiveBlockDownloaded >= pCurReplay->m_iMaxSessionBlockRequired )
			continue;

		// Update replay status
		pCurReplay->m_nStatus = CReplay::REPLAYSTATUS_ERROR;

		// Display an error message
		ShowDownloadFailedMessage( pCurReplay );

		// Save the replay
		CL_GetReplayManager()->FlagForFlush( pCurReplay, false );
	}
}

void CClientRecordingSession::RefreshLastUpdateTime()
{
	m_flLastUpdateTime = g_pEngine->GetHostTime();
}

void CClientRecordingSession::ShowDownloadFailedMessage( const CReplay *pReplay )
{
	// Don't show the download failed message for replays that were saved during this run of the game.
	if ( !pReplay || !pReplay->m_bSavedDuringThisSession )
		return;

	// Display an error message
	g_pClient->DisplayReplayMessage( "#Replay_DownloadFailed", true, false, "replay\\downloadfailed.wav" );
}

void CClientRecordingSession::CacheReplay( CReplay *pReplay )
{
	Assert( m_lstReplays.Find( pReplay ) == m_lstReplays.InvalidIndex() );
	m_lstReplays.AddToTail( pReplay );

	// We should no longer auto-delete this session if CacheReplay() is being called.  This
	// can happen if the user connects to a server, saves a replay, deletes the replay (at
	// which point auto-delete is flagged for the recording session), and then saves another
	// replay.  In this situation, we obviously don't want to delete the session anymore.
	if ( m_bAutoDelete )
	{
		m_bAutoDelete = false;
	}
}

bool CClientRecordingSession::ShouldSyncBlocksWithServer() const
{
	// Already downloaded all blocks?
	if ( m_bAllBlocksDownloaded )
		return false;

	// If block count is out of sync with the m_iLastBlockDownloaded we need to sync up
	const bool bReachedMaxDownloadAttempts = m_nSessionInfoDownloadAttempts >= MAX_SESSION_INFO_DOWNLOAD_ATTEMPTS;
	const bool bNeedToDownloadBlocks = m_iLastBlockToDownload >= 0;
//	const bool bAlreadyDownloadedAllNeededBlocks = m_iLastBlockToDownload <= m_iGreatestConsecutiveBlockDownloaded;
	const bool bAlreadyDownloadedAllNeededBlocks = m_iLastBlockToDownload < m_vecBlocks.Count();	// NOTE/TODO: Shouldn't this look at m_iGreatestConsecutiveBlockDownloaded?  Tried for a week, but it caused bugs.  Reverting for now.  TODO
	const bool bTimedOut = false;//TimedOut();

	const bool bResult = !bReachedMaxDownloadAttempts &&
						  bNeedToDownloadBlocks &&
						 !bAlreadyDownloadedAllNeededBlocks &&
						 !bTimedOut;

	if ( bResult )
	{
		IF_REPLAY_DBG( Warning( "Blocks out of sync for session %i - downloading session info now.\n", GetHandle() ) );
	}
	else
	{
		DBG3( "NOT syncing because:\n" );
		if ( bReachedMaxDownloadAttempts )			DBG3( "   - Reached maximum download attempts\n" );
		if ( !bNeedToDownloadBlocks )				DBG3( "   - No replay saved yet\n" );
		if ( bAlreadyDownloadedAllNeededBlocks )	DBG3( "   - Already downloaded all needed blocks\n" );
		if ( bTimedOut )							DBG3( "   - Download timed out (session info file didn't change after 90 seconds)\n" );
	}

	return bResult;
}

void CClientRecordingSession::PopulateWithRecordingData( int nCurrentRecordingStartTick )
{
	BaseClass::PopulateWithRecordingData( nCurrentRecordingStartTick );

	CClientRecordingSessionManager::ServerRecordingState_t *pServerState = &CL_GetRecordingSessionManager()->m_ServerRecordingState;
	m_strName = pServerState->m_strSessionName;

	// Get download URL from replicated cvars
	m_strBaseDownloadURL = Replay_GetDownloadURL();

	// Get server session ID
	m_uServerSessionID = g_pClient->GetServerSessionId();
}

bool CClientRecordingSession::ShouldDitchSession() const
{
	return BaseClass::ShouldDitchSession() || m_lstReplays.Count() == 0;
}

void CClientRecordingSession::OnDelete()
{
	// Abort any session block downloads now
	CL_GetSessionBlockDownloader()->AbortDownloadsAndCleanup( this );
	if ( m_pSessionInfoDownloader )
	{
		m_pSessionInfoDownloader->CleanupDownloader();
	}

	// Delete blocks
	BaseClass::OnDelete();
}

//----------------------------------------------------------------------------------------
