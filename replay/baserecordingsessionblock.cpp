//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "baserecordingsessionblock.h"
#include "replay/replayutils.h"
#include "replay/ireplaycontext.h"
#include "replay/irecordingsessionmanager.h"
#include "replay/shared_defs.h"
#include "KeyValues.h"
#include "qlimits.h"
#include "utlbuffer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CBaseRecordingSessionBlock::CBaseRecordingSessionBlock( IReplayContext *pContext )
:	m_pContext( pContext ),
	m_nRemoteStatus( STATUS_INVALID ),
	m_nHttpError( ERROR_NONE ),
	m_hSession( REPLAY_HANDLE_INVALID ),
	m_bHashValid( false ),
	m_iReconstruction( -1 ),
	m_uFileSize( 0 ),
	m_uUncompressedSize( 0 ),
	m_nCompressorType( COMPRESSORTYPE_INVALID )
{
	m_szFullFilename[ 0 ] = '\0';
	V_memset( m_aHash, 0, sizeof( m_aHash ) );
}

const char *CBaseRecordingSessionBlock::GetSubKeyTitle() const
{
	CBaseRecordingSession *pOwnerSession = m_pContext->GetRecordingSessionManager()->FindSession( m_hSession );
	if ( !pOwnerSession )
	{
		AssertMsg( 0, "Owner session not found" );
		return "";
	}
	return Replay_va( "%s_part_%i", pOwnerSession->m_strName.Get(), m_iReconstruction );
}

const char *CBaseRecordingSessionBlock::GetPath() const
{
	return Replay_va( "%s%s%c", m_pContext->GetBaseDir(), SUBDIR_BLOCKS, CORRECT_PATH_SEPARATOR );
}

bool CBaseRecordingSessionBlock::Read( KeyValues *pIn )
{
	if ( !BaseClass::Read( pIn ) )
		return false;

	m_nRemoteStatus = (RemoteStatus_t)pIn->GetInt( "remote_status", (int)STATUS_INVALID );		Assert( m_nRemoteStatus != STATUS_INVALID );
	m_nHttpError = (Error_t)pIn->GetInt( "error", (int)ERROR_NONE );
	m_iReconstruction = pIn->GetInt( "recon_index", -1 );					Assert( m_iReconstruction >= 0 );
	m_hSession = (ReplayHandle_t)pIn->GetInt( "session", REPLAY_HANDLE_INVALID );	Assert( m_hSession != REPLAY_HANDLE_INVALID );
	m_uFileSize = pIn->GetInt( "size", 0 );									Assert( m_uFileSize > 0 );
	m_uUncompressedSize = pIn->GetInt( "usize", 0 );
	m_nCompressorType = (CompressorType_t)pIn->GetInt( "compressor", 0 );

	ReadHash( pIn, "hash" );

	return true;
}


void CBaseRecordingSessionBlock::Write( KeyValues *pOut )
{
	BaseClass::Write( pOut );

	pOut->SetInt( "remote_status", (int)m_nRemoteStatus );
	pOut->SetInt( "error", (int)m_nHttpError );
	pOut->SetInt( "recon_index", m_iReconstruction );
	pOut->SetInt( "session", (int)m_hSession );
	pOut->SetInt( "size", m_uFileSize );
	pOut->SetInt( "usize", m_uUncompressedSize );
	pOut->SetInt( "compressor", (int)m_nCompressorType );

	WriteHash( pOut, "hash" );

	// NOTE: Filename written in subclasses, since it's handled differently for client vs. server
}

void CBaseRecordingSessionBlock::OnDelete()
{
	BaseClass::OnDelete();

	// NOTE: The actual .block files get deleted in subclasses, since each handle the case differently.
}

bool CBaseRecordingSessionBlock::ReadHash( KeyValues *pIn, const char *pHashName )
{
	const char *pHashStr = pIn->GetString( pHashName );
	bool bResult = false;
	if ( V_strlen( pHashStr ) > 0 )
	{
		int iHash = 0;
		char *p = strtok( const_cast< char * >( pHashStr ), " " );
		while ( p )
		{
			// Should have no more than 3 characters
			if ( V_strlen( p ) > 3 )
			{
				break;
			}

			m_aHash[ iHash++ ] = (uint8)atoi( p );
			p = strtok( NULL, " " );

			bResult = true;
		}
	}

	// Keep track of whether we have a valid hash or not
	m_bHashValid = bResult;

	AssertMsg( bResult, "Invalid hash string" );
	return bResult;
}

void CBaseRecordingSessionBlock::WriteHash( KeyValues *pOut, const char *pHashName ) const
{
	CFmtStr fmtHash( "%03i %03i %03i %03i %03i %03i %03i %03i %03i %03i %03i %03i %03i %03i %03i %03i",
		m_aHash[0], m_aHash[1], m_aHash[2], m_aHash[3], m_aHash[4], m_aHash[5], m_aHash[6], m_aHash[7],
		m_aHash[8], m_aHash[9], m_aHash[10], m_aHash[11], m_aHash[12], m_aHash[13], m_aHash[14], m_aHash[15]
	);
	pOut->SetString( pHashName, fmtHash.Access() );
}

bool CBaseRecordingSessionBlock::HasValidHash() const
{
	return m_bHashValid;
}

void CBaseRecordingSessionBlock::WriteSessionInfoDataToBuffer( CUtlBuffer &buf ) const
{
	RecordingSessionBlockSpec_t blob;

	blob.m_iReconstruction = (int32)m_iReconstruction;
	blob.m_uRemoteStatus = (uint8)m_nRemoteStatus;
	blob.m_uFileSize = m_uFileSize;
	blob.m_nCompressorType = (int8)m_nCompressorType;	// Can be COMPRESSORTYPE_INVALID if not compressed
	blob.m_uUncompressedSize = m_uUncompressedSize;			// Can be 0 if not compressed
	V_memcpy( blob.m_aHash, m_aHash, sizeof( m_aHash ) );

	// Write the blob at the appropriate position in the buffer
	Assert( m_iReconstruction >= 0 );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, m_iReconstruction * sizeof( blob ) );
	buf.Put( &blob, sizeof( RecordingSessionBlockSpec_t ) );
}

//----------------------------------------------------------------------------------------

/*static*/ const char *CBaseRecordingSessionBlock::GetRemoteStatusStringSafe( RemoteStatus_t nStatus )
{
	switch ( nStatus )
	{
	case STATUS_INVALID:			return "invalid";
	case STATUS_ERROR:				return "error";
	case STATUS_WRITING:			return "writing";
	case STATUS_READYFORDOWNLOAD:	return "ready for download";
	default:						return "unknown";
	}
}

//----------------------------------------------------------------------------------------
