//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cl_sessioninfodownloader.h"
#include "replay/ienginereplay.h"
#include "replay/shared_defs.h"
#include "cl_recordingsession.h"
#include "cl_recordingsessionblock.h"
#include "cl_replaycontext.h"
#include "cl_sessionblockdownloader.h"
#include "KeyValues.h"
#include "convar.h"
#include "dbg.h"
#include "vprof.h"
#include "sessioninfoheader.h"
#include "utlbuffer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

extern IEngineReplay *g_pEngine;

//----------------------------------------------------------------------------------------

CSessionInfoDownloader::CSessionInfoDownloader()
:	m_pDownloader( NULL ),
	m_pSession( NULL ),
	m_flLastDownloadTime( 0.0f ),
	m_nError( ERROR_NONE ),
	m_nHttpError( HTTP_ERROR_NONE ),
	m_bDone( false )
{
}

CSessionInfoDownloader::~CSessionInfoDownloader()
{
	Assert( m_pDownloader == NULL );	// We should have deleted the downloader already
}

void CSessionInfoDownloader::CleanupDownloader()
{
	if ( m_pDownloader )
	{
		m_pDownloader->AbortDownloadAndCleanup();
		m_pDownloader = NULL;
	}
}

void CSessionInfoDownloader::DownloadSessionInfoAndUpdateBlocks( CBaseRecordingSession *pSession )
{
	Assert( m_pDownloader == NULL );

	// Cache session
	m_pSession = pSession;

	// Download the session info now
	m_pDownloader = new CHttpDownloader( this );
	m_pDownloader->BeginDownload( pSession->GetSessionInfoURL(), NULL );
}

float CSessionInfoDownloader::GetNextThinkTime() const
{
	extern ConVar replay_sessioninfo_updatefrequency;
	return m_flLastDownloadTime + replay_sessioninfo_updatefrequency.GetFloat();
}

void CSessionInfoDownloader::Think()
{
	VPROF_BUDGET( "CSessionInfoDownloader::Think", VPROF_BUDGETGROUP_REPLAY );

	CBaseThinker::Think();

	// If we're not downloading, no need to think
	if ( !m_pDownloader )
		return;

	// If the download's complete
	if ( m_pDownloader->IsDone() && m_pDownloader->CanDelete() )
	{
		// We're done - CanDelete() will now return true
		delete m_pDownloader;
		m_pDownloader = NULL;
	}
	else
	{
		// Otherwise, think...
		m_pDownloader->Think();
	}
}

void CSessionInfoDownloader::OnDownloadComplete( CHttpDownloader *pDownloader, const unsigned char *pData )
{
	Assert( pDownloader );

	// Clear out any previous error
	m_nError = ERROR_NONE;
	m_nHttpError = HTTP_ERROR_NONE;

	bool bUpdatedSomething = false;

#if _DEBUG
	extern ConVar replay_simulatedownloadfailure;
	const bool bForceError = replay_simulatedownloadfailure.GetInt() == 2;
#else
	const bool bForceError = false;
#endif

	if ( pDownloader->GetStatus() != HTTP_DONE || bForceError )
	{
		m_nError = ERROR_DOWNLOAD_FAILED;
		m_nHttpError = pDownloader->GetError();
		DBG( "Session info download FAILED.\n" );
	}
	else
	{
		Assert( pData );

		// Read header
		SessionInfoHeader_t header;
		if ( !ReadSessionInfoHeader( pData, pDownloader->GetSize(), header ) )
		{
			m_nError = ERROR_NOT_ENOUGH_DATA;
		}
		else
		{
			IF_REPLAY_DBG( Warning( "Session info downloaded successfully for session %s\n", header.m_szSessionName ) );

			// Get number of blocks for data validation
			const int nNumBlocks = header.m_nNumBlocks;
			if ( nNumBlocks <= 0 )
			{
				m_nError = ERROR_BAD_NUM_BLOCKS;
			}
			else
			{
				// The block has either been found or created - now, fill it with data
				const char *pSessionName = header.m_szSessionName;
				if ( !pSessionName || !pSessionName[0] )
				{
					m_nError = ERROR_NO_SESSION_NAME;
				}
				else
				{
					// Now that we have the session name, find it in the client session manager
					CClientRecordingSession *pSession = CL_CastSession( CL_GetRecordingSessionManager()->FindSessionByName( pSessionName ) );		Assert( pSession );
					if ( !pSession )
					{
						AssertMsg( 0, "Session should always exist by this point" );
						m_nError = ERROR_UNKNOWN_SESSION;
					}
					else
					{
						// Get recording state for session
						pSession->m_bRecording = header.m_bRecording;

						const CompressorType_t nHeaderCompressorType = header.m_nCompressorType;
						uint8 *pPayload = (uint8 *)pData + sizeof( SessionInfoHeader_t );
						uint8 *pUncompressedPayload = NULL;
						unsigned int uUncompressedPayloadSize = header.m_uPayloadSizeUC;

						// Validate the payload with the MD5 digest
						bool bPayloadValid = g_pEngine->MD5_HashBuffer( header.m_aHash, (const uint8 *)pPayload, header.m_uPayloadSize, NULL );

						if ( !bPayloadValid )
						{
							m_nError = ERROR_PAYLOAD_HASH_FAILED;
						}
						else
						{
							if ( nHeaderCompressorType == COMPRESSORTYPE_INVALID )
							{
								// Uncompressed payload is read to go
								pUncompressedPayload = pPayload;
							}
							else
							{
								// Attempt to decompress the payload
								ICompressor *pCompressor = CreateCompressor( header.m_nCompressorType );
								if ( !pCompressor )
								{
									bPayloadValid = false;
									m_nError = ERROR_COULD_NOT_CREATE_COMPRESSOR;
								}
								else
								{
									// Uncompressed size big enough to read at least one block?
									if ( header.m_uPayloadSizeUC <= MIN_SESSION_INFO_PAYLOAD_SIZE )
									{
										bPayloadValid = false;
										m_nError = ERROR_INVALID_UNCOMPRESSED_SIZE;
									}
									else
									{
										// Attempt to decompress payload now
										pUncompressedPayload = new uint8[ uUncompressedPayloadSize ];
										if ( !pCompressor->Decompress( (char *)pUncompressedPayload, &uUncompressedPayloadSize, (const char *)pPayload, header.m_uPayloadSize ) )
										{
											bPayloadValid = false;
											m_nError = ERROR_PAYLOAD_DECOMPRESS_FAILED;
										}
									}
								}
							}

							if ( bPayloadValid )
							{
								AssertMsg( pUncompressedPayload, "This should never be NULL here." );
								AssertMsg( uUncompressedPayloadSize >= MIN_SESSION_INFO_PAYLOAD_SIZE, "This size should always be valid here." );

								RecordingSessionBlockSpec_t DummyBlock;
								CUtlBuffer buf( pUncompressedPayload, uUncompressedPayloadSize, CUtlBuffer::READ_ONLY );

								// Optimization: start the read at the first block we care about, which is one block after the last consecutive, downloaded block.
								// This optimization should come in handy on servers that run lengthy rounds, so we're not reiterating over inconsequential blocks
								// (i.e. they were already downloaded).
								int iStartBlock = pSession->GetGreatestConsecutiveBlockDownloaded() + 1;
								if ( iStartBlock > 0 )
								{
									buf.SeekGet( CUtlBuffer::SEEK_HEAD, iStartBlock * sizeof( RecordingSessionBlockSpec_t ) );
								}

								// If for some reason the seek caused a 'get' overflow, try reading from the start of the buffer.
								if ( !buf.IsValid() )
								{
									iStartBlock = 0;
									buf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
								}

								// Read blocks, starting from the calculated start block.
								for ( int i = iStartBlock; i < header.m_nNumBlocks; ++i )
								{
									// Attempt to read the current block from the buffer
									buf.Get( &DummyBlock, sizeof( DummyBlock ) );
									if ( !buf.IsValid() )
									{
										m_nError = ERROR_BLOCK_READ_FAILED;
										break;
									}

									IF_REPLAY_DBG( Warning( "processing block with recon index: %i\n", DummyBlock.m_iReconstruction ) );

									// Get reconstruction index
									const int iBlockReconstruction = (ReplayHandle_t)DummyBlock.m_iReconstruction;
									if ( iBlockReconstruction < 0 )
									{
										m_nError = ERROR_INVALID_ORDER;
										continue;
									}

									// Check status
									const int nRemoteStatus = (int)DummyBlock.m_uRemoteStatus;
									if ( nRemoteStatus < 0 || nRemoteStatus >= CBaseRecordingSessionBlock::MAX_STATUS )
									{
										// Status not found or invalid status
										m_nError = ERROR_INVALID_REPLAY_STATUS;
										continue;
									}

									// Get the block file size
									const uint32 uFileSize = (uint32)DummyBlock.m_uFileSize;

									// Get the uncompressed block size
									const uint32 uUncompressedSize = (uint32)DummyBlock.m_uUncompressedSize;

									// Get the compressor type
									const int nCompressorType = (uint32)DummyBlock.m_nCompressorType;

									// Attempt to find the block in the session
									CClientRecordingSessionBlock *pBlock = CL_CastBlock( CL_GetRecordingSessionBlockManager()->FindBlockForSession( m_pSession->GetHandle(), iBlockReconstruction ) );

									// If the block exists and has already been downloaded, we have nothing more to update
									if ( pBlock && !pBlock->NeedsUpdate() )
										continue;

									bool bBlockDataChanged = false;

									// If the block doesn't exist in the session block manager, create it now
									if ( !pBlock )
									{
										CClientRecordingSessionBlock *pNewBlock = CL_CastBlock( CL_GetRecordingSessionBlockManager()->CreateAndGenerateHandle() );
										pNewBlock->m_iReconstruction = iBlockReconstruction;
						//				pNewBlock->m_strFullFilename = Replay_va(
										const char *pFullFilename = Replay_va(
											"%s%s_part_%i.%s",
											pNewBlock->GetPath(),
											pSession->m_strName.Get(), pNewBlock->m_iReconstruction,
											BLOCK_FILE_EXTENSION
										);
										V_strcpy( pNewBlock->m_szFullFilename, pFullFilename );
										pNewBlock->m_hSession = pSession->GetHandle();

										// Add to session block manager
										CL_GetRecordingSessionBlockManager()->Add( pNewBlock );

										// Add the block to the session (marks session as dirty)
										pSession->AddBlock( pNewBlock, false );

										// Use the new block
										pBlock = pNewBlock;

										bBlockDataChanged = true;
									}

									IF_REPLAY_DBG2( Warning( "   Block %i status=%s\n", pBlock->m_iReconstruction, pBlock->GetRemoteStatusStringSafe( pBlock->m_nRemoteStatus ) ) );

									// Now that we've got a block, replicate the server data/fill it in
									if ( pBlock->m_nRemoteStatus != nRemoteStatus )
									{
										pBlock->m_nRemoteStatus = (CBaseRecordingSessionBlock::RemoteStatus_t)nRemoteStatus;
										bBlockDataChanged = true;
									}

									// Block file size needs to be set?
									if ( pBlock->m_uFileSize != uFileSize )
									{
										pBlock->m_uFileSize = uFileSize;
										bBlockDataChanged = true;
									}

									// Uncompressed block file size needs to be set?
									if ( pBlock->m_uUncompressedSize != uUncompressedSize )
									{
										Assert( nCompressorType >= COMPRESSORTYPE_INVALID );
										pBlock->m_uUncompressedSize = uUncompressedSize;
										bBlockDataChanged = true;
									}

									// Compressor type needs to be set?
									if ( pBlock->m_nCompressorType != (CompressorType_t)nCompressorType )
									{
										pBlock->m_nCompressorType = (CompressorType_t)nCompressorType;
										bBlockDataChanged = true;
									}

									// Attempt to read the hash if we haven't already done so
									if ( !pBlock->HasValidHash() )
									{
										V_memcpy( pBlock->m_aHash, DummyBlock.m_aHash, sizeof( pBlock->m_aHash ) );
										bBlockDataChanged = true;
									}

									// Shift the block's state from waiting to ready-to-download if the block is ready on the server.
									if ( pBlock->m_nDownloadStatus == CClientRecordingSessionBlock::DOWNLOADSTATUS_WAITING &&
										 nRemoteStatus == CBaseRecordingSessionBlock::STATUS_READYFORDOWNLOAD )
									{
										pBlock->m_nDownloadStatus = CClientRecordingSessionBlock::DOWNLOADSTATUS_READYTODOWNLOAD;
									}

									// Save
									if ( bBlockDataChanged )
									{
										CL_GetRecordingSessionBlockManager()->FlagForFlush( pBlock, false );

										bUpdatedSomething = true;
									}
								}

								// If this session is not recording, make sure the max number of blocks in the session is in check.
								if ( !pSession->m_bRecording && pSession->GetLastBlockToDownload() >= nNumBlocks )
								{
									// This will adjust all replay max blocks
									pSession->AdjustLastBlockToDownload( nNumBlocks - 1 );
								}

								// If we've updated something, set cache the current time in the session
								if ( bUpdatedSomething )
								{
									pSession->RefreshLastUpdateTime();
								}
								else if ( pSession->GetLastUpdateTime() >= 0.0f && ( g_pEngine->GetHostTime() - pSession->GetLastUpdateTime() > DOWNLOAD_TIMEOUT_THRESHOLD ) )
								{
									pSession->OnDownloadTimeout();
								}
							}
						}
					}
				}
			}
		}
	}

	// Display a message for the given download error
	if ( m_nError != ERROR_NONE )
	{
		// Report an error to the user
		const char *pErrorToken = GetErrorString( m_nError, m_nHttpError );
		if ( m_nError == ERROR_DOWNLOAD_FAILED )
		{
			KeyValues *pParams = new KeyValues( "args", "url", pDownloader->GetURL() );
			CL_GetErrorSystem()->AddFormattedErrorFromTokenName( pErrorToken, pParams );
		}
		else
		{
			CL_GetErrorSystem()->AddErrorFromTokenName( pErrorToken );
		}

		// Report error to OGS
		CL_GetErrorSystem()->OGS_ReportSessioInfoDownloadError( pDownloader, pErrorToken );
	}

	// Flag as done
	m_bDone = true;

	// Cache download time
	m_flLastDownloadTime = g_pEngine->GetHostTime();
}

const char *CSessionInfoDownloader::GetErrorString( int nError, HTTPError_t nHttpError ) const
{
	switch ( nError )
	{
	case ERROR_NO_SESSION_NAME:				return "#Replay_DL_Err_SI_NoSessionName";
	case ERROR_REPLAY_NOT_FOUND:			return "#Replay_DL_Err_SI_ReplayNotFound";
	case ERROR_INVALID_REPLAY_STATUS:		return "#Replay_DL_Err_SI_InvalidReplayStatus";
	case ERROR_INVALID_ORDER:				return "#Replay_DL_Err_SI_InvalidOrder";
	case ERROR_UNKNOWN_SESSION:				return "#Replay_DL_Err_SI_Unknown_Session";
	case ERROR_BLOCK_READ_FAILED:			return "#Replay_DL_Err_SI_BlockReadFailed";
	case ERROR_NOT_ENOUGH_DATA:				return "#Replay_DL_Err_SI_NotEnoughData";
	case ERROR_COULD_NOT_CREATE_COMPRESSOR:	return "#Replay_DL_Err_SI_CouldNotCreateCompressor";
	case ERROR_INVALID_UNCOMPRESSED_SIZE:	return "#Replay_DL_Err_SI_InvalidUncompressedSize";
	case ERROR_PAYLOAD_DECOMPRESS_FAILED:	return "#Replay_DL_Err_SI_PayloadDecompressFailed";
	case ERROR_PAYLOAD_HASH_FAILED:			return "#Replay_DL_Err_SI_PayloadHashFailed";

	case ERROR_DOWNLOAD_FAILED:
		switch ( m_nHttpError )
		{
		case HTTP_ERROR_ZERO_LENGTH_FILE:	return "#Replay_DL_Err_SI_DownloadFailed_ZeroLengthFile";
		case HTTP_ERROR_CONNECTION_CLOSED:	return "#Replay_DL_Err_SI_DownloadFailed_ConnectionClosed";
		case HTTP_ERROR_INVALID_URL:		return "#Replay_DL_Err_SI_DownloadFailed_InvalidURL";
		case HTTP_ERROR_INVALID_PROTOCOL:	return "#Replay_DL_Err_SI_DownloadFailed_InvalidProtocol";
		case HTTP_ERROR_CANT_BIND_SOCKET:	return "#Replay_DL_Err_SI_DownloadFailed_CantBindSocket";
		case HTTP_ERROR_CANT_CONNECT:		return "#Replay_DL_Err_SI_DownloadFailed_CantConnect";
		case HTTP_ERROR_NO_HEADERS:			return "#Replay_DL_Err_SI_DownloadFailed_NoHeaders";
		case HTTP_ERROR_FILE_NONEXISTENT:	return "#Replay_DL_Err_SI_DownloadFailed_FileNonExistent";
		default:							return "#Replay_DL_Err_SI_DownloadFailed_UnknownError";
		}
	}
	return "#Replay_DL_Err_SI_Unknown";
}

//----------------------------------------------------------------------------------------
