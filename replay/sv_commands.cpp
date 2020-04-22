//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "replaysystem.h"
#include "sv_sessionrecorder.h"
#include "utlbuffer.h"
#include "sessioninfoheader.h"
#include "sv_fileservercleanup.h"
#include "sv_publishtest.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

#define ENSURE_DEDICATED()	if ( !g_pEngine->IsDedicated() ) return;

//----------------------------------------------------------------------------------------

CON_COMMAND( replay_record, "Starts Replay demo recording." )
{
	ENSURE_DEDICATED();
	SV_GetSessionRecorder()->StartRecording();
}

//----------------------------------------------------------------------------------------

CON_COMMAND( replay_stoprecord, "Stop Replay demo recording." )
{
	ENSURE_DEDICATED();
	g_pReplay->SV_EndRecordingSession();
}

//----------------------------------------------------------------------------------------

CON_COMMAND( replay_docleanup, "Deletes stale session data from the fileserver.  \"replay_docleanup force\" will remove all replay session data." )
{
	ENSURE_DEDICATED();

	bool bForceCleanAll = false;
	if ( args.ArgC() == 2 )
	{
		if ( !V_stricmp( args[1], "force" ) )
		{
			bForceCleanAll = true;
		}
		else
		{
			ConMsg( "\n   ** ERROR: '%s' is not a valid paramter - use 'force' to force clean all replay session data.\n\n", args[1] );
			return;
		}
	}

	if ( !SV_DoFileserverCleanup( bForceCleanAll, g_pBlockSpewer ) )
	{
		Msg( "No demos were deleted.\n" );
	}
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_dopublishtest, "Do a replay publish test using the current setup.", FCVAR_DONTRECORD )
{
	ENSURE_DEDICATED();

	g_pBlockSpewer->PrintBlockStart();
	SV_DoTestPublish();
	g_pBlockSpewer->PrintBlockEnd();
}

//----------------------------------------------------------------------------------------

void PrintSessionInfo( const char *pFilename )
{
	CUtlBuffer buf;
	if ( !g_pFullFileSystem->ReadFile( pFilename, NULL, buf ) )
	{
		Msg( "Failed to read file, \"%s\"\n", pFilename );
		return;
	}

	int nFileSize = buf.TellPut();

	SessionInfoHeader_t header;
	if ( !ReadSessionInfoHeader( buf.Base(), nFileSize, header ) )
	{
		Msg( "Failed to read header information.\n" );
		return;
	}

	char szDigestStr[33];
	V_binarytohex( header.m_aHash, sizeof( header.m_aHash ), szDigestStr, sizeof( szDigestStr ) );

	Msg( "\n\theader:\n" );
	Msg( "\n" );
	Msg( "\t%27s: %u\n", "version", header.m_uVersion );
	Msg( "\t%27s: %s\n", "session name", header.m_szSessionName );
	Msg( "\t%27s: %s\n", "currently recording?", header.m_bRecording ? "yes" : "no" );
	Msg( "\t%27s: %i\n", "# blocks", header.m_nNumBlocks );
	Msg( "\t%27s: %s\n", "compressor", GetCompressorNameSafe( header.m_nCompressorType ) );
	Msg( "\t%27s: %s\n", "md5 digest", szDigestStr );
	Msg( "\t%27s: %u bytes\n", "payload size (compressed)", header.m_uPayloadSize );
	Msg( "\t%27s: %u bytes\n", "payload size (uncompressed)", header.m_uPayloadSizeUC );
	Msg( "\n" );

	const uint8 *pPayload = (uint8 *)buf.Base() + sizeof( SessionInfoHeader_t );
	uint32 uUncompressedPayloadSize = header.m_uPayloadSizeUC;
	if ( !g_pEngine->MD5_HashBuffer( header.m_aHash, (const uint8 *)pPayload, header.m_uPayloadSize, NULL ) )
	{
		Msg( "Data validation failed.\n" );
		return;
	}

	const uint8 *pUncompressedPayload;
	bool bFreeUncompressedPayload = true;

	if ( header.m_nCompressorType == COMPRESSORTYPE_INVALID )
	{
		// The payload is already uncompressed - don't free, since this buffer was allocated by the CUtlBuffer "buf"
		pUncompressedPayload = pPayload;
		bFreeUncompressedPayload = false;
	}
	else
	{
		if ( uUncompressedPayloadSize != header.m_uPayloadSizeUC )
		{
			Msg( "Decompressed to a different size (%u) than specified by header (%u)\n", uUncompressedPayloadSize, header.m_uPayloadSizeUC );
			return;
		}

		ICompressor *pCompressor = CreateCompressor( header.m_nCompressorType );
		if ( !pCompressor )
		{
			Msg( "Failed to create compressor.\n" );
			return;
		}

		pUncompressedPayload = new uint8[ uUncompressedPayloadSize ];
		if ( !pUncompressedPayload )
		{
			Msg( "Failed to allocate uncompressed payload.\n" );
			delete [] pCompressor;
			return;
		}

		pCompressor->Decompress( (char *)pUncompressedPayload, &uUncompressedPayloadSize, (const char *)pPayload, header.m_uPayloadSize );

		delete pCompressor;
	}

	if ( uUncompressedPayloadSize <= MIN_SESSION_INFO_PAYLOAD_SIZE )
	{
		Msg( "Uncompressed payload not large enough to read a single block.\n" );
	}
	else
	{
		RecordingSessionBlockSpec_t DummyBlock;
		CUtlBuffer bufPayload( pUncompressedPayload, uUncompressedPayloadSize, CUtlBuffer::READ_ONLY );

		Msg( "\n\tblocks:\n\n" );
		Msg( "\t   index        status                        MD5               compressor    size (uncompressed)  size (compressed)\n" );

		bool bBlockReadFailed = false;
		for ( int i = 0; i < header.m_nNumBlocks; ++i )
		{
			// Attempt to read the current block from the buffer
			bufPayload.Get( &DummyBlock, sizeof( DummyBlock ) );
			if ( !bufPayload.IsValid() )
			{
				bBlockReadFailed = true;
				break;
			}

			V_binarytohex( DummyBlock.m_aHash, sizeof( DummyBlock.m_aHash ), szDigestStr, sizeof( szDigestStr ) );

			Msg( "\t   %5i", DummyBlock.m_iReconstruction );
			Msg( "%20s", CBaseRecordingSessionBlock::GetRemoteStatusStringSafe( (CBaseRecordingSessionBlock::RemoteStatus_t)DummyBlock.m_uRemoteStatus ) );
			Msg( "%35s", szDigestStr );
			Msg( "%8s", GetCompressorNameSafe( (CompressorType_t)DummyBlock.m_nCompressorType ) );
			Msg( "%20u", DummyBlock.m_uFileSize );
			Msg( "%20u", DummyBlock.m_uUncompressedSize );
			Msg( "\n" );
		}
	}

	Msg( "\n" );

	if ( bFreeUncompressedPayload )
	{
		delete [] pUncompressedPayload;
	}
}

CON_COMMAND_F( replay_printsessioninfo, "Print session info", FCVAR_DONTRECORD )
{
	ENSURE_DEDICATED();

	if ( args.ArgC() != 2 )
	{
		Msg( "Usage: replay_printsessioninfo <full path and filename>\n" );
		return;
	}

	PrintSessionInfo( args[1] );
}

//----------------------------------------------------------------------------------------
