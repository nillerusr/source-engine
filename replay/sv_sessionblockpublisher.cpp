//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "sv_sessionblockpublisher.h"
#include "sv_replaycontext.h"
#include "demofile/demoformat.h"
#include "sv_recordingsession.h"
#include "sv_recordingsessionblock.h"
#include "sv_sessioninfopublisher.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CSessionBlockPublisher::CSessionBlockPublisher( CServerRecordingSession *pSession,
											    CSessionInfoPublisher *pSessionInfoPublisher )
:	m_pSession( pSession ),
	m_pSessionInfoPublisher( pSessionInfoPublisher )
{
	// Cache the dump interval so it can't be modified during a round - doing so would require
	// an update on all clients.
	extern ConVar replay_block_dump_interval;
	m_nDumpInterval = MAX( MIN_SERVER_DUMP_INTERVAL, replay_block_dump_interval.GetInt() );

	// Write the first block 15 or so seconds from now
	m_flLastBlockWriteTime = g_pEngine->GetHostTime();
}

CSessionBlockPublisher::~CSessionBlockPublisher()
{
}

void CSessionBlockPublisher::PublishAllSynchronous()
{
	while ( !IsDone() )
	{
		Think();
	}
}

void CSessionBlockPublisher::AbortPublish()
{
	FOR_EACH_LL( m_lstPublishingBlocks, it )
	{
		CServerRecordingSessionBlock *pCurBlock = m_lstPublishingBlocks[ it ];
		IFilePublisher *&pPublisher = pCurBlock->m_pPublisher;	// Shorthand

		if ( !pPublisher )
			continue;

		// Already done?
		if ( pPublisher->IsDone() )
			continue;

		pPublisher->AbortAndCleanup();
	}

	// Remove all blocks
	m_lstPublishingBlocks.RemoveAll();
}

void CSessionBlockPublisher::OnStartRecording()
{
}

void CSessionBlockPublisher::OnStopRecord( bool bAborting )
{
	if ( !bAborting )
	{
		// Write one final session block.
		WriteAndPublishSessionBlock();
	}
}

ReplayHandle_t CSessionBlockPublisher::GetSessionHandle() const
{
	return m_pSession->GetHandle();
}

void CSessionBlockPublisher::WriteAndPublishSessionBlock()
{
	// Make sure there is data to write
	uint8 *pSessionBuffer;
	int nSessionBufferSize;
	g_pEngine->GetSessionRecordBuffer( &pSessionBuffer, &nSessionBufferSize );	// This will get called the last client disconnects from the server - but in waiting for players state we won't have a demo buffer
	if ( !pSessionBuffer || nSessionBufferSize == 0 )
		return;

	// Create a new block
	CServerRecordingSessionBlock *pNewBlock = SV_CastBlock( SV_GetRecordingSessionBlockManager()->CreateAndGenerateHandle() );
	if ( !pNewBlock )
	{
		Warning( "Failed to create replay \"%s\"\n", pNewBlock->m_szFullFilename );
		delete pNewBlock;
		return;
	}
	
	if ( m_pSession->m_nServerStartRecordTick < 0 )
	{
		Warning( "Error: Current recording start tick was not properly setup.  Aborting block write.\n" );
		delete pNewBlock;
		return;
	}

	// Figure out what the current block is
	const int iCurrentSessionBlock = m_pSession->GetNumBlocks();

	// Add an entry to the server index with the "writing" status set
	const char *pFullFilename = Replay_va(
		"%s%s_part_%u.%s", SV_GetTmpDir(),
		SV_GetRecordingSessionManager()->GetCurrentSessionName(), iCurrentSessionBlock, BLOCK_FILE_EXTENSION
	);
	V_strcpy( pNewBlock->m_szFullFilename, pFullFilename );
	pNewBlock->m_nWriteStatus = CServerRecordingSessionBlock::WRITESTATUS_INVALID;	// Must be set here to trigger write
	pNewBlock->m_nRemoteStatus = CBaseRecordingSessionBlock::STATUS_WRITING;
	pNewBlock->m_iReconstruction = iCurrentSessionBlock;
	pNewBlock->m_hSession = m_pSession->GetHandle();

	// Match the session's lock - the block will be unlocked once recording has stopped and all publishing is complete.
	pNewBlock->SetLocked( true );

	// Commit the replay to the history manager's list
	SV_GetRecordingSessionBlockManager()->Add( pNewBlock );

	// Also store a pointer to the block in the session - NOTE: session will not attempt to free this pointer
	m_pSession->AddBlock( pNewBlock, false );

	// Cache the block temporarily while the binary block itself writes to disk - NOTE: will not attempt to free
	m_lstPublishingBlocks.AddToTail( pNewBlock );

	// Write the block now
	PublishBlock( pNewBlock );	// pNewBlock->m_nWriteStatus modified here

	IF_REPLAY_DBG( Warning( "%f: (%i) Publishing new block, %s\n", g_pEngine->GetHostTime(), iCurrentSessionBlock, pNewBlock->GetFilename() ) );
}

void CSessionBlockPublisher::GatherBlockData( uint8 *pSessionBuffer, int nSessionBufferSize, CServerRecordingSessionBlock *pBlock, unsigned char **ppSafeBlockData, int *pBlockSize )
{
	const int nHeaderSize = sizeof( demoheader_t );

	int nBlockOffset = 0;
	const int nBlockSize = nSessionBufferSize;
	int nTotalSize = nBlockSize;

	demoheader_t *pHeader = NULL;

	// If this is the first block, pass in a header to be written.  Otherwise, just write the block.
	if ( !pBlock->m_iReconstruction )
	{
		// Setup start tick in the header
		pHeader = g_pEngine->GetReplayDemoHeader();

		// Add header size
		nBlockOffset = nHeaderSize;
		nTotalSize += nHeaderSize;
	}

	// Make a copy of the block
	unsigned char *pBuffer = new unsigned char[ nTotalSize ];
	unsigned char *pBlockCopy = pBuffer + nBlockOffset;

	// Only write the header if necessary
	if ( pHeader )
	{
		demoheader_t littleEndianHeader = *pHeader;
		littleEndianHeader.playback_time = FLT_MAX;
		littleEndianHeader.playback_ticks = INT_MAX;
		littleEndianHeader.playback_frames = INT_MAX;

		// Byteswap
		ByteSwap_demoheader_t( littleEndianHeader );

		// Write header
		V_memcpy( pBuffer, &littleEndianHeader, sizeof( littleEndianHeader ) );
	}

	// Note that pBlockCopy is based on pBuffer, which was allocated with nBlockSize PLUS
	// header size - this will not overflow.
	V_memcpy( pBlockCopy, pSessionBuffer, nBlockSize );

	// Copy to "out" parameters
	*pBlockSize = nTotalSize;
	*ppSafeBlockData = pBuffer;
}

void CSessionBlockPublisher::PublishBlock( CServerRecordingSessionBlock *pBlock )
{
	uint8 *pSessionBuffer;
	int nSessionBufferSize;
	if ( !g_pEngine->GetSessionRecordBuffer( &pSessionBuffer, &nSessionBufferSize ) )
	{
		Warning( "Block publish failed!\n" );
		return;
	}

	unsigned char *pSafeBlockData;
	int nBlockSize;
	GatherBlockData( pSessionBuffer, nSessionBufferSize, pBlock, &pSafeBlockData, &nBlockSize );

	// We've got what we need and can reset the put ptr
	g_pEngine->ResetReplayRecordBuffer();

	AssertMsg( !pBlock->m_pPublisher, "No publisher should exist for this block yet!" );

	// Set status to working
	pBlock->m_nWriteStatus = CServerRecordingSessionBlock::WRITESTATUS_WORKING;

	// Get the number of bytes written
	pBlock->m_uFileSize = nBlockSize;

	// Make sure the main thread doesn't unload the block while it's being published
	pBlock->SetLocked( true );

	// Asynchronously publish to fileserver
	PublishFileParams_t params;
	params.m_pOutFilename = pBlock->m_szFullFilename;
	params.m_pSrcData = pSafeBlockData;
	params.m_nSrcSize = nBlockSize;
	params.m_pCallbackHandler = this;
	params.m_nCompressorType = COMPRESSORTYPE_BZ2;
	params.m_bHash = true;
	params.m_bFreeSrcData = true;
	params.m_bDeleteFile = false;
	params.m_pUserData = pBlock;
	pBlock->m_pPublisher = SV_PublishFile( params );
}

void CSessionBlockPublisher::OnPublishComplete( const IFilePublisher *pPublisher, void *pUserData )
{
	CServerRecordingSessionBlock *pBlock = (CServerRecordingSessionBlock *)pUserData;
	Assert( pBlock );

	// Set block status
	if ( pPublisher->GetStatus() == IFilePublisher::PUBLISHSTATUS_OK )
	{
		pBlock->m_nWriteStatus = CServerRecordingSessionBlock::WRITESTATUS_SUCCESS;
	}
	else
	{
		pBlock->m_nWriteStatus = CServerRecordingSessionBlock::WRITESTATUS_FAILED;

		// Publish failed - handle as needed
		g_pServerReplayContext->OnPublishFailed();
	}

	// Did the block compress OK?
	if ( pPublisher->Compressed() )
	{
		// Cache compressor type
		pBlock->m_nCompressorType = pPublisher->GetCompressorType();

		const int nCompressedSize = pPublisher->GetCompressedSize();
		const float flRatio = (float)pBlock->m_uFileSize / nCompressedSize;
		IF_REPLAY_DBG( Warning( "Block compression ratio: %.3f:1\n", flRatio ) );

		// Update size
		pBlock->m_uUncompressedSize = pBlock->m_uFileSize;
		pBlock->m_uFileSize = nCompressedSize;
	}

	// Get the MD5
	if ( pPublisher->Hashed() )
	{
		pPublisher->GetHash( pBlock->m_aHash );
	}

	// Now that m_nWriteStatus has been set in the block, the session info will be updated
	// accordingly the next time PublishThink() is run.

	// Mark the block as dirty since it was modified
	Assert( pBlock->m_nWriteStatus != CServerRecordingSessionBlock::WRITESTATUS_INVALID );
	SV_GetRecordingSessionBlockManager()->FlagForFlush( pBlock, false );

	IF_REPLAY_DBG( Warning( "Publish complete for block %s\n", pBlock->GetDebugName() ) );
}

void CSessionBlockPublisher::OnPublishAborted( const IFilePublisher *pPublisher )
{
	CServerRecordingSessionBlock *pBlock = FindBlockFromPublisher( pPublisher );

	// Update the block's status
	if ( pBlock )
	{
		pBlock->m_nWriteStatus = CServerRecordingSessionBlock::WRITESTATUS_FAILED;
	}

	g_pServerReplayContext->OnPublishFailed();
}

CServerRecordingSessionBlock *CSessionBlockPublisher::FindBlockFromPublisher( const IFilePublisher *pPublisher )
{
	FOR_EACH_LL( m_lstPublishingBlocks, i )
	{
		CServerRecordingSessionBlock *pCurBlock = m_lstPublishingBlocks[ i ];
		if ( pCurBlock->m_pPublisher == pPublisher )
		{
			return pCurBlock;
		}
	}

	AssertMsg( 0, "Could not find block with the given publisher!" );
	return NULL;
}

void CSessionBlockPublisher::Think()
{
	// NOTE: This member function gets called even if replay is disabled.  This is intentional.

	VPROF_BUDGET( "CSessionBlockPublisher::Think", VPROF_BUDGETGROUP_REPLAY );

	PublishThink();
}

void CSessionBlockPublisher::PublishThink()
{
	AssertMsg( m_pSession->IsLocked(), "The session isn't locked, which means blocks can be being deleted and will probably cause a crash." );

	// Go through all currently publishing blocks and free/think
	FOR_EACH_LL( m_lstPublishingBlocks, it )
	{
		CServerRecordingSessionBlock *pCurBlock = m_lstPublishingBlocks[ it ];
		IFilePublisher *&pPublisher = pCurBlock->m_pPublisher;	// Shorthand

		if ( !pPublisher )
			continue;

		// If the publisher's done, free it
		if ( pPublisher->IsDone() )
		{
			delete pPublisher;
			pPublisher = NULL;
		}
		else
		{
			// Let the publisher think
			pPublisher->Think();
		}
	}

	// Write a new session block out right now?
	float flHostTime = g_pEngine->GetHostTime();
	if ( m_flLastBlockWriteTime != 0.0f &&
		 flHostTime - m_flLastBlockWriteTime >= m_nDumpInterval &&
		 m_pSession->m_bRecording )
	{
		Assert( m_nDumpInterval > 0 );

		// Write it
		WriteAndPublishSessionBlock();

		// Update the time
		m_flLastBlockWriteTime = flHostTime;
	}

	// Check status of any replays that are being written
	bool bUpdateSessionInfo  = false;
	for( int it = m_lstPublishingBlocks.Head(); it != m_lstPublishingBlocks.InvalidIndex(); )
	{
		CServerRecordingSessionBlock *pCurBlock = m_lstPublishingBlocks[ it ];

		// Updated when write status is set to success or failure
		int nPendingRequestStatus = CBaseRecordingSessionBlock::STATUS_INVALID;

		// If set to anything besides InvalidIndex(), it will be removed from the list
		int itRemove = m_lstPublishingBlocks.InvalidIndex();
		bool bWriteBlockInfoToDisk = false;

		switch ( pCurBlock->m_nWriteStatus )
		{
		case CServerRecordingSessionBlock::WRITESTATUS_INVALID:
			AssertMsg( 0, "Why is m_nWriteStatus WRITESTATUS_INVALID here?" );
			break;

		case CServerRecordingSessionBlock::WRITESTATUS_WORKING:	// Do nothing if still writing
			break;

		case CServerRecordingSessionBlock::WRITESTATUS_SUCCESS:
			IF_REPLAY_DBG2( Warning( "  Block %i marked as succeeded.\n", pCurBlock->m_iReconstruction ) );
			pCurBlock->m_nRemoteStatus = CBaseRecordingSessionBlock::STATUS_READYFORDOWNLOAD;
			nPendingRequestStatus = pCurBlock->m_nRemoteStatus;
			bWriteBlockInfoToDisk = true;
			itRemove = it;
			break;

		case CServerRecordingSessionBlock::WRITESTATUS_FAILED:
		default:	// Error?
			IF_REPLAY_DBG2( Warning( "  Block %i marked as failed.\n", pCurBlock->m_iReconstruction ) );
			pCurBlock->m_nRemoteStatus = CBaseRecordingSessionBlock::STATUS_ERROR;
			pCurBlock->m_nHttpError = CBaseRecordingSessionBlock::ERROR_WRITEFAILED;
			nPendingRequestStatus = pCurBlock->m_nRemoteStatus;
			bWriteBlockInfoToDisk = true;
			itRemove = it;

			// TODO: Retry
		}

		if ( bWriteBlockInfoToDisk )
		{
			// Save the master index file
			Assert( pCurBlock->m_nWriteStatus != CServerRecordingSessionBlock::WRITESTATUS_INVALID );
			SV_GetRecordingSessionBlockManager()->FlagForFlush( pCurBlock, false );
		}

		// Find the owning session
		Assert( pCurBlock->m_hSession == m_pSession->GetHandle() );

		// Refresh session info file
		if ( nPendingRequestStatus != CBaseRecordingSessionBlock::STATUS_INVALID )
		{
			// Update it after this loop
			bUpdateSessionInfo = true;
		}

		// Update iterator
		it = m_lstPublishingBlocks.Next( it );

		// Remove?
		if ( itRemove != m_lstPublishingBlocks.InvalidIndex() )
		{
			IF_REPLAY_DBG( Warning( "Removing block %i from publisher\n", pCurBlock->m_iReconstruction ) );
			// Free/clear publisher
			delete pCurBlock->m_pPublisher;
			pCurBlock->m_pPublisher = NULL;

			// Removes from the list but doesn't free, since any pointer here points to a block somewhere
			m_lstPublishingBlocks.Unlink( itRemove );
		}
	}

	// Publish session info file now if it isn't already publishing
	if ( bUpdateSessionInfo )
	{
		m_pSessionInfoPublisher->Publish();
	}
}

bool CSessionBlockPublisher::IsDone() const
{
	return m_lstPublishingBlocks.Count() == 0;
}

#ifdef _DEBUG
void CSessionBlockPublisher::Validate()
{
	FOR_EACH_LL( m_lstPublishingBlocks, i )
	{
		CServerRecordingSessionBlock *pCurBlock = m_lstPublishingBlocks[ i ];
		Assert( pCurBlock->m_nRemoteStatus == CBaseRecordingSessionBlock::STATUS_READYFORDOWNLOAD );
	}
}
#endif

//----------------------------------------------------------------------------------------
