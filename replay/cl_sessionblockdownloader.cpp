//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cl_sessionblockdownloader.h"
#include "replay/ienginereplay.h"
#include "cl_recordingsessionblockmanager.h"
#include "cl_replaycontext.h"
#include "cl_recordingsession.h"
#include "cl_recordingsessionblock.h"
#include "errorsystem.h"
#include "convar.h"
#include "vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

extern IEngineReplay *g_pEngine;
extern ConVar replay_maxconcurrentdownloads;

//----------------------------------------------------------------------------------------

int CSessionBlockDownloader::sm_nNumCurrentDownloads = 0;

//----------------------------------------------------------------------------------------

CSessionBlockDownloader::CSessionBlockDownloader()
:	m_nMaxBlock( -1 )
{
}

void CSessionBlockDownloader::Shutdown()
{
	AbortDownloadsAndCleanup( NULL );

	Assert( sm_nNumCurrentDownloads == 0 );
}

void CSessionBlockDownloader::AbortDownloadsAndCleanup( CClientRecordingSession *pSession )
{
	// NOTE: sm_nNumCurrentDownloads will be decremented in OnDownloadComplete(), which is
	// invoked by CHttpDownloader::AbortDownloadAndCleanup()

	// Abort any remaining downloads - callbacks will be invoked, so this shutdown
	// should be called before any of those objects are cleaned up.
	FOR_EACH_LL( m_lstDownloaders, i )
	{
		CHttpDownloader *pCurDownloader = m_lstDownloaders[ i ];

		// If a session was passed in, make sure it has the same handle as the block
		CClientRecordingSessionBlock *pBlock = (CClientRecordingSessionBlock *)pCurDownloader->GetUserData();
		if ( pSession && ( !pBlock || pBlock->m_hSession != pSession->GetHandle() ) )
			continue;

		pCurDownloader->AbortDownloadAndCleanup();
		delete pCurDownloader;
	}
	m_lstDownloaders.RemoveAll();
}

bool CSessionBlockDownloader::AtMaxConcurrentDownloads() const
{
	return ( sm_nNumCurrentDownloads >= replay_maxconcurrentdownloads.GetInt() );
}

float CSessionBlockDownloader::GetNextThinkTime() const
{
	return g_pEngine->GetHostTime() + 0.5f;
}

void CSessionBlockDownloader::Think()
{
	VPROF_BUDGET( "CSessionBlockDownloader::Think", VPROF_BUDGETGROUP_REPLAY );

	CBaseThinker::Think();

	// Hack to not think right away
	if ( g_pEngine->GetHostTime() < 3 )
		return;

	// Don't go over the desired maximum # of concurrent downloads
	if ( !AtMaxConcurrentDownloads() )
	{
		// Go through all blocks and begin downloading any that the server index downloader
		// has determined are ready
		CClientRecordingSessionBlockManager *pBlockManager = CL_GetRecordingSessionBlockManager();
		FOR_EACH_OBJ( pBlockManager, i )
		{
			CClientRecordingSessionBlock *pBlock = CL_CastBlock( pBlockManager->m_vecObjs[ i ] );

			// Checks to see if the remote status is marked as ready for download
			if ( !pBlock->ShouldDownloadNow() )
				continue;

			// Lookup the session for the block
			CClientRecordingSession *pSession = CL_CastSession( CL_GetRecordingSessionManager()->Find( pBlock->m_hSession ) );
			if ( !pSession )
			{
				AssertMsg( 0, "Session for block not found!  This should never happen!" );
				continue;
			}

			// Do we need any blocks at all from this session?  Is this block within range?
			int iLastBlockToDownload = pSession->GetLastBlockToDownload();
			if ( iLastBlockToDownload < 0 || pBlock->m_iReconstruction > iLastBlockToDownload )
			{
				continue;
			}

			// Begin the download
			CHttpDownloader *pDownloader = new CHttpDownloader( this );
			const char *pFilename = V_UnqualifiedFileName( pBlock->m_szFullFilename );
#ifdef _DEBUG
			extern ConVar replay_forcedownloadurl;
			const char *pForceURL = replay_forcedownloadurl.GetString();
			const char *pURL = pForceURL[0] ? pForceURL : Replay_va( "%s%s", pSession->m_strBaseDownloadURL.Get(), pFilename );
#else
			const char *pURL = Replay_va( "%s%s", pSession->m_strBaseDownloadURL.Get(), pFilename );
#endif
			const char *pGamePath = Replay_va( "%s%s", CL_GetRecordingSessionBlockManager()->GetSavePath(), pFilename );
			pDownloader->BeginDownload( pURL, pGamePath, (void *)pBlock, &pBlock->m_uBytesDownloaded );

			IF_REPLAY_DBG(
				Warning ( "%s block %i from %s to path %s...\n",
					pBlock->GetNumDownloadAttempts() ? "RETRYING download for" : "Downloading" ,
					pBlock->m_iReconstruction, pURL, pGamePath )
			);

			// Add the downloader
			m_lstDownloaders.AddToTail( pDownloader );

			// Update block's status
			pBlock->m_nDownloadStatus = CClientRecordingSessionBlock::DOWNLOADSTATUS_DOWNLOADING;

			// Mark as dirty
			CL_GetRecordingSessionBlockManager()->FlagForFlush( pBlock, false );

			// Update # of concurrent downloads
			++sm_nNumCurrentDownloads;

			// Get out if we're at max downloads now
			if ( AtMaxConcurrentDownloads() )
				break;
		}
	}

	int it = m_lstDownloaders.Head();
	while ( it != m_lstDownloaders.InvalidIndex() )
	{
		// Remove finished downloaders
		CHttpDownloader *pCurDownloader = m_lstDownloaders[ it ];
		if ( pCurDownloader->IsDone() && pCurDownloader->CanDelete() )
		{
			int itRemove = it;

			// Next
			it = m_lstDownloaders.Next( it );
			
			// Remove the downloader from the list
			m_lstDownloaders.Remove( itRemove );

			// Free the downloader
			delete pCurDownloader;
		}
		else
		{
			// Let the downloader think
			pCurDownloader->Think();

			// Next
			it = m_lstDownloaders.Next( it );
		}
	}
}

void CSessionBlockDownloader::OnConnecting( CHttpDownloader *pDownloader )
{
	CClientRecordingSessionBlock *pBlock = (CClientRecordingSessionBlock *)pDownloader->GetUserData();	AssertValidReadPtr( pBlock );
	pBlock->m_nDownloadStatus = CClientRecordingSessionBlock::DOWNLOADSTATUS_CONNECTING;
}

void CSessionBlockDownloader::OnFetch( CHttpDownloader *pDownloader )
{
	CClientRecordingSessionBlock *pBlock = (CClientRecordingSessionBlock *)pDownloader->GetUserData();	AssertValidReadPtr( pBlock );
	pBlock->m_nDownloadStatus = CClientRecordingSessionBlock::DOWNLOADSTATUS_DOWNLOADING;
}

void CSessionBlockDownloader::OnDownloadComplete( CHttpDownloader *pDownloader, const unsigned char *pData )
{
	// TODO: Compare downloaded byte size (pDownloader->GetBytesDownloaded()) to size in block
	// Write block size into session info on server

	int it = m_lstDownloaders.Find( pDownloader );
	if ( it == m_lstDownloaders.InvalidIndex() )
	{
		AssertMsg( 0, "Downloader now found in session block downloader list!  This should never happen!" );
		return;
	}

	CClientRecordingSessionBlock *pBlock = (CClientRecordingSessionBlock *)pDownloader->GetUserData();	AssertValidReadPtr( pBlock );
	const int nSize = pDownloader->GetSize();

	HTTPStatus_t nStatus = pDownloader->GetStatus();

#if _DEBUG
	extern ConVar replay_simulatedownloadfailure;
	if ( replay_simulatedownloadfailure.GetInt() == 3 )
	{
		nStatus = HTTP_ERROR;
	}
#endif

	switch ( nStatus )
	{
	case HTTP_ABORTED:

		pBlock->m_nDownloadStatus = CClientRecordingSessionBlock::DOWNLOADSTATUS_ABORTED;
		break;

	case HTTP_DONE:

		{
			unsigned char aLocalHash[16];

#if _DEBUG
			extern ConVar replay_simulate_size_discrepancy;
			extern ConVar replay_simulate_bad_hash;

			const bool bSizesDiffer = replay_simulate_size_discrepancy.GetBool() || pBlock->m_uFileSize != pDownloader->GetBytesDownloaded();
			const bool bHashFail = replay_simulate_bad_hash.GetBool() || !pBlock->ValidateData( pData, nSize, aLocalHash );
#else
			const bool bSizesDiffer = pBlock->m_uFileSize != pDownloader->GetBytesDownloaded();
			const bool bHashFail = !pBlock->ValidateData( pData, nSize );
#endif

			bool bTryAgain = false;
			if ( bSizesDiffer )
			{
				AssertMsg( 0, "Number of bytes downloaded differs from size specified in session info file." );
				bTryAgain = true;
			}
			else if ( bHashFail )
			{
				DBG( "Download failed - either data validation failed\n" );

				// Data validation failed
				pBlock->m_bDataInvalid = true;
				bTryAgain = true;
			}
			else
			{
				DBG( "Data validation successful.\n" );

				// Data validation succeeded
				pBlock->m_nDownloadStatus = CClientRecordingSessionBlock::DOWNLOADSTATUS_DOWNLOADED;

				// Clear out any previous errors
				pBlock->m_nHttpError = HTTP_ERROR_NONE;
				pBlock->m_bDataInvalid = false;
			}

			// Failed?
			if ( bTryAgain )
			{
				// Attempt to download again if necessary
				pBlock->AttemptToResetForDownload();

				// Report error to OGS.
				CL_GetErrorSystem()->OGS_ReportSessionBlockDownloadError(
					pDownloader, pBlock, pDownloader->GetBytesDownloaded(), m_nMaxBlock, &bSizesDiffer,
					&bHashFail, aLocalHash
				);
			}
		}

		break;

	case HTTP_ERROR:

		// If we've attempted and failed to download the block 3 times, report the error and
		// put the block in error state.
		if ( pBlock->AttemptToResetForDownload() )
			break;

		// Otherwise, we've max'd out attempts - cache the error state
		pBlock->m_nDownloadStatus = CClientRecordingSessionBlock::DOWNLOADSTATUS_ERROR;
		pBlock->m_nHttpError = pDownloader->GetError();

		// Now that the block is in the error state, the replay's status will be updated to
		// the error state as well (see pSession->UpdateReplayStatuses() below).

		// Report the error to user & OGS
		{
			// Create a session block download error.
			CL_GetErrorSystem()->OGS_ReportSessionBlockDownloadError(
				pDownloader, pBlock, pDownloader->GetBytesDownloaded(), m_nMaxBlock, NULL, NULL, NULL
			);

			// Report error to user.
			const char *pToken = CHttpDownloader::GetHttpErrorToken( pDownloader->GetError() );
			CL_GetErrorSystem()->AddFormattedErrorFromTokenName(
				"#Replay_DL_Err_HTTP_Prefix",
				new KeyValues(
					"args",
					"err",
					pToken
				)
			);
		}

		break;

	default:
		AssertMsg( 0, "Invalid download state in CSessionBlockDownloader::OnDownloadComplete()" );
	}

	// Flag block for flush
	CL_GetRecordingSessionBlockManager()->FlagForFlush( pBlock, false );

	CClientRecordingSession *pSession = CL_CastSession( CL_GetRecordingSessionManager()->FindSession( pBlock->m_hSession ) );		Assert( pSession );

	// Update all replays that care about this block
	pSession->UpdateReplayStatuses( pBlock );

	// Decrement # of downloads
	--sm_nNumCurrentDownloads;
}

//----------------------------------------------------------------------------------------
