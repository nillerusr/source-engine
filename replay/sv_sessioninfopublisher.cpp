//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "sv_sessioninfopublisher.h"
#include "sv_replaycontext.h"
#include "sv_recordingsessionblock.h"
#include "sv_recordingsession.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CSessionInfoPublisher::CSessionInfoPublisher( CServerRecordingSession *pSession )
:	m_pSession( pSession ),
	m_flSessionInfoPublishTime( 0.0f ),
	m_itLastCompletedBlockWrittenToBuffer( ~0 ),
	m_pFilePublisher( NULL ),
	m_bShouldPublish( false )
{
}

CSessionInfoPublisher::~CSessionInfoPublisher()
{
}

void CSessionInfoPublisher::Publish()
{
	m_bShouldPublish = true;
}

bool CSessionInfoPublisher::IsDone() const
{
	return !m_bShouldPublish && m_pFilePublisher == NULL;
}

void CSessionInfoPublisher::OnStopRecord( bool bAborting )
{
}

void CSessionInfoPublisher::AbortPublish()
{
	m_bShouldPublish = false;

	if ( m_pFilePublisher && !m_pFilePublisher->IsDone() )
	{
		m_pFilePublisher->AbortAndCleanup();
	}
}	

void CSessionInfoPublisher::RefreshSessionInfoBlockData( CUtlBuffer &buf )
{
	const CBaseRecordingSession::BlockContainer_t &vecBlocks = m_pSession->GetBlocks();

	// Start from head if this is the first time we're writing, otherwise start
	// from block after the last one written.
	const int itStart = m_itLastCompletedBlockWrittenToBuffer == vecBlocks.InvalidIndex() ?
		0 : m_itLastCompletedBlockWrittenToBuffer + 1;

	for( int i = itStart; i < vecBlocks.Count(); ++i )
	{
		const CServerRecordingSessionBlock *pBlock = SV_CastBlock( vecBlocks[ i ] );

		// NOTE: This will SeekPut() on buf, based on the block's reconstruction index.
		pBlock->WriteSessionInfoDataToBuffer( buf );

		// Cache the last block written whose state isn't going to change
		if ( pBlock->m_nRemoteStatus == CBaseRecordingSessionBlock::STATUS_READYFORDOWNLOAD &&
			 i > m_itLastCompletedBlockWrittenToBuffer )
		{
			m_itLastCompletedBlockWrittenToBuffer = i;
		}

		IF_REPLAY_DBG( Warning( "Writing block w/ recon index %i to session info buffer\n", pBlock->m_iReconstruction ) );
	}
}

void CSessionInfoPublisher::Think()
{
	// Existing publisher?
	if ( m_pFilePublisher )
	{
		// Finished?
		if ( m_pFilePublisher->IsDone() )
		{
			// Free/clear
			delete m_pFilePublisher;
			m_pFilePublisher = NULL;
		}
		else
		{
			// Let the publisher think
			m_pFilePublisher->Think();
		}
	}

	// Publish needed?
	if ( !m_bShouldPublish )
		return;

	// Already publishing?
	if ( m_pFilePublisher )
		return;

	DBG( "Publishing session info...\n" );

	// Write outstanding blocks to the buffer
	RefreshSessionInfoBlockData( m_bufSessionInfo );

	// We now know the uncompressed payload size
	const int nPayloadSize = m_bufSessionInfo.TellPut();

	// Create as much of the header as possible now - the rest will be written in AdjustHeader()
	// once the publisher knows the md5 digest and the compression result.
	Assert( m_pSession->m_strName.Length() < MAX_SESSIONNAME_LENGTH );	// The only way this name is going to get very long is if 
	V_strcpy_safe( m_SessionInfoHeader.m_szSessionName, m_pSession->m_strName.Get() );
	m_SessionInfoHeader.m_nNumBlocks = m_pSession->GetNumBlocks();	
	m_SessionInfoHeader.m_bRecording = m_pSession->m_bRecording;
	m_SessionInfoHeader.m_uPayloadSizeUC = nPayloadSize;

	// Format a path & filename that points to the tmp dir, with <session name>.dmx on the end
	CFmtStr fmtTmpSessionInfoFile( "%s%s.%s", SV_GetTmpDir(), m_pSession->m_strName.Get(), GENERIC_FILE_EXTENSION );

	// Publish the file now (asynchronous)
	PublishFileParams_t params;
	params.m_pOutFilename = fmtTmpSessionInfoFile.Access(),
	params.m_pSrcData = (uint8 *)m_bufSessionInfo.Base();
	params.m_nSrcSize = nPayloadSize;
	params.m_pCallbackHandler = this;
	params.m_nCompressorType = COMPRESSORTYPE_LZSS;
	params.m_bHash = true;
	params.m_bFreeSrcData = false;
	params.m_bDeleteFile = false; 
	params.m_pHeaderData = &m_SessionInfoHeader;
	params.m_nHeaderSize = sizeof( SessionInfoHeader_t );
	params.m_pUserData = NULL;
	m_pFilePublisher = SV_PublishFile( params );

	// Reset flag
	m_bShouldPublish = false;
}

void CSessionInfoPublisher::OnPublishComplete( const IFilePublisher *pPublisher, void *pUserData )
{
	Assert( !pUserData );

	// Handle publish failure
	if ( pPublisher->GetStatus() != IFilePublisher::PUBLISHSTATUS_OK )
	{
		g_pServerReplayContext->OnPublishFailed();
	}
}

void CSessionInfoPublisher::OnPublishAborted( const IFilePublisher *pPublisher )
{
	Assert( pPublisher == m_pFilePublisher );

	g_pServerReplayContext->OnPublishFailed();
}

void CSessionInfoPublisher::AdjustHeader( const IFilePublisher *pPublisher, void *pHeaderData )
{
	SessionInfoHeader_t *pHeader = static_cast< SessionInfoHeader_t * >( pHeaderData );

	// Set compressor type - will return COMPRESSORTYPE_INVALID if compression failed.
	pHeader->m_nCompressorType = pPublisher->GetCompressorType();
	pHeader->m_uPayloadSize = pPublisher->GetCompressedSize();

	// Get MD5 digest
	pPublisher->GetHash( pHeader->m_aHash );
}

void CSessionInfoPublisher::PublishAllSynchronous()
{
	while ( !IsDone() )
	{
		Think();
	}
}

//----------------------------------------------------------------------------------------
