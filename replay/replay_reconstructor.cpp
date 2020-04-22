//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "replay_reconstructor.h"
#include "replay/replay.h"
#include "cl_recordingsessionblock.h"
#include "cl_recordingsession.h"
#include "cl_replaycontext.h"
#include "cl_replaymanager.h"
#include "UtlSortVector.h"
#include "demofile/demoformat.h"
#include "lzss.h"
#include "compression.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

class CReplay_LessFunc
{
public:
	bool Less( const CClientRecordingSessionBlock *pBlock1, const CClientRecordingSessionBlock *pBlock2, void *pCtx )
	{
		return ( pBlock1->m_iReconstruction < pBlock2->m_iReconstruction );
	}
};

//----------------------------------------------------------------------------------------

bool Replay_Reconstruct( CReplay *pReplay, bool bDeleteBlocks/*=true*/ )
{
	// Get the session for the given replay
	CClientRecordingSession *pSession = CL_CastSession( CL_GetRecordingSessionManager()->FindSession( pReplay->m_hSession ) );
	if ( !pSession )
	{
		CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Recon_BadSession" ); 
		return false;
	}

	// Dynamically load blocks for the session
	pSession->LoadBlocksForSession();

	// How many blocks needed
	const int nNumBlocksNeeded = pReplay->m_iMaxSessionBlockRequired + 1;

	// Enough blocks to proceed?
	const CBaseRecordingSession::BlockContainer_t &vecAllBlocks = pSession->GetBlocks();
	if ( vecAllBlocks.Count() < nNumBlocksNeeded )
	{
		CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_NotEnoughBlocksForReconstruction" );
		return false;
	}

	// Add needed blocks in sorted order
	CUtlSortVector< const CClientRecordingSessionBlock *, CReplay_LessFunc > vecReplayBlocks;
	FOR_EACH_VEC( vecAllBlocks, i )
	{
		const CClientRecordingSessionBlock *pCurBlock = CL_CastBlock( vecAllBlocks[ i ] );

		// Don't add more blocks than are needed
		if ( pCurBlock->m_iReconstruction >= nNumBlocksNeeded )
			continue;

		// Sorted insert
		vecReplayBlocks.Insert( pCurBlock );
	}

	// Now we need to do an integrity check on all blocks
	int iLastReconstructionIndex = 0;		// All replay reconstruction will start with block 0
	FOR_EACH_VEC( vecReplayBlocks, i )
	{
		const CClientRecordingSessionBlock *pCurBlock = vecReplayBlocks[ i ];

		// Haven't downloaded yet or failed to download for some reason?
		if ( !pCurBlock->DownloadedSuccessfully() )
		{
			CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Recon_BlocksNotDLd" );
			return false;
		}

		// Check against reconstruction indices and make sure the list is continuous
		if ( pCurBlock->m_iReconstruction - iLastReconstructionIndex > 1 )
		{
			CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Recon_NonContinuous" );
			return false;
		}

		// Cache for next iteration
		iLastReconstructionIndex = pCurBlock->m_iReconstruction;
	}

	// Open the target, reconstruction file - "<session_name>_<replay handle>.dem"
	CUtlString strReconstructedFileFilename;
	strReconstructedFileFilename.Format( "%s%s_%i.dem", CL_GetReplayManager()->GetIndexPath(), pSession->m_strName.Get(), pReplay->GetHandle() );
	FileHandle_t hReconstructedFile = g_pFullFileSystem->Open( strReconstructedFileFilename.Get(), "wb" );
	if ( hReconstructedFile == FILESYSTEM_INVALID_HANDLE )
	{
		CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Recon_OpenOutFile" );
		return false;
	}

	// Now that we have an ordered list of replays to reconstruct, create the mother file
	bool bFailed = false;
	FOR_EACH_VEC( vecReplayBlocks, i )
	{
		const CClientRecordingSessionBlock *pCurBlock = vecReplayBlocks[ i ];

		// Open the partial file for the current replay
		const char *pFilename = pCurBlock->m_szFullFilename;
		
		FileHandle_t hBlockFile = g_pFullFileSystem->Open( pFilename, "rb" );
		if ( hBlockFile == FILESYSTEM_INVALID_HANDLE )
		{
			CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Recon_BlockDNE" );
			bFailed = true;
			break;
		}

		int nSize = g_pFullFileSystem->Size( hBlockFile );
		if ( nSize == 0 )
		{
			CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Recon_ZeroLengthBlock" );
			bFailed = true;
			break;
		}

		char *pBuffer = (char *)new char[ nSize ];
		if ( !pBuffer )
		{
			CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Recon_OutOfMemory" );
			bFailed = true;
		}
		else
		{
			// Read the file
			if ( nSize != g_pFullFileSystem->Read( pBuffer, nSize, hBlockFile ) )
			{
				CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Recon_FailedToRead" );
				bFailed = true;
			}
			else
			{
				// Decompress if necessary
				CompressorType_t nCompressorType = (CompressorType_t)pCurBlock->m_nCompressorType;
				if ( nCompressorType != COMPRESSORTYPE_INVALID )
				{
					ICompressor *pCompressor = CreateCompressor( nCompressorType );

					if ( !pCompressor )
					{
						CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Recon_DecompressorCreate" );
						bFailed = true;
					}
					else
					{
						const unsigned int nCompressedSize = nSize;
						unsigned int nUncompressedSize = pCurBlock->m_uUncompressedSize;
						char *pUncompressedBuffer = new char[ nUncompressedSize ];

						if ( !pUncompressedBuffer )
						{
							CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Recon_Alloc" );
							bFailed = true;
						}
						else
						{
							if ( !nUncompressedSize )
							{
								CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Recon_UncompressedSizeIsZero" );
								bFailed = true;
							}
							else if ( !pCompressor->Decompress( pUncompressedBuffer, &nUncompressedSize, pBuffer, nCompressedSize ) )
							{
								CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Recon_Decompression" );
								bFailed = true;
							}
						}

						if ( bFailed )
						{
							delete [] pUncompressedBuffer;
						}
						else
						{
							// Overwrite buffer pointer and buffer size
							pBuffer = pUncompressedBuffer;
							nSize = nUncompressedSize;
						}

						// Free compressor
						delete pCompressor;
					}
				}

				// Append the read data to the mother file
				if ( g_pFullFileSystem->Write( pBuffer, nSize, hReconstructedFile ) == nSize )
				{
					g_pFullFileSystem->Close( hBlockFile );
				}
				else
				{
					CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Recon_FailedToWrite" );
					bFailed = true;
				}
			}
		}

		// Free
		delete [] pBuffer;
	}

	if ( !bFailed )
	{
		// Add dem_stop - embed the calculated end tick
		const int nLengthInTicks = g_pEngine->TimeToTicks( pReplay->m_flLength );
		int nLastTick = 0;
		if ( nLengthInTicks > 0 )
		{
			nLastTick = pReplay->m_nSpawnTick + nLengthInTicks;
		}
		unsigned char szEndTickBuf[4];
		*( (int32 *)szEndTickBuf ) = nLastTick;
		unsigned char szStopBuf[] = { dem_stop, szEndTickBuf[0], szEndTickBuf[1], szEndTickBuf[2], szEndTickBuf[3], 0 };
		int nStopSize = sizeof( szStopBuf );
		if ( g_pFullFileSystem->Write( szStopBuf, nStopSize, hReconstructedFile ) != nStopSize )
		{
			Warning( "Replay: Failed to write stop bits to reconstructed replay file \"%s\"\n", strReconstructedFileFilename.Get() );
			// Should still run fine
		}

		// Save reconstructed filename, which will serve to indicate whether the
		// replay's been successfully reconstructed or not.
		pReplay->m_strReconstructedFilename = strReconstructedFileFilename;

		// Mark the replay for flush
		CL_GetReplayManager()->FlagForFlush( pReplay, true );

		// Delete blocks - removes from session, session block manager, and from disk if no other replays are depending on them.
		if ( bDeleteBlocks )
		{
			pSession->DeleteBlocks();
		}
	}

	// Close reconstructed file
	g_pFullFileSystem->Close( hReconstructedFile );

	return true;
}

//----------------------------------------------------------------------------------------