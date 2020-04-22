//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cl_recordingsessionblock.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CClientRecordingSessionBlock::CClientRecordingSessionBlock( IReplayContext *pContext )
:	CBaseRecordingSessionBlock( pContext ),
	m_uBytesDownloaded( 0 ),
	m_nDownloadStatus( DOWNLOADSTATUS_WAITING ),
	m_nHttpError( HTTP_ERROR_NONE ),
	m_bDataInvalid( false ),
	m_nDownloadAttempts( 0 )
{
}

bool CClientRecordingSessionBlock::NeedsUpdate() const
{
	// TODO: Is this correct?
	return m_nDownloadStatus != DOWNLOADSTATUS_DOWNLOADED;
}

bool CClientRecordingSessionBlock::ShouldDownloadNow() const
{
	return m_nRemoteStatus == STATUS_READYFORDOWNLOAD &&
		   m_nDownloadStatus == DOWNLOADSTATUS_READYTODOWNLOAD;
}

bool CClientRecordingSessionBlock::DownloadedSuccessfully() const
{
	return m_nDownloadStatus == DOWNLOADSTATUS_DOWNLOADED &&
		   m_nHttpError == HTTP_ERROR_NONE;
}

bool CClientRecordingSessionBlock::Read( KeyValues *pIn )
{
	if ( !BaseClass::Read( pIn ) )
		return false;

	m_nDownloadStatus = (DownloadStatus_t)pIn->GetInt( "download_status" );
	m_nHttpError = (HTTPError_t)pIn->GetInt( "download_error" );
	m_uBytesDownloaded = (uint32)pIn->GetInt( "bytes_downloaded", 0 );
	m_bDataInvalid = pIn->GetInt( "data_invalid", 0 ) != 0;

	// Read relative path and construct full path - must have a filename
	const char *pBlockFile = pIn->GetString( "filename" );
	if ( !V_strlen( pBlockFile ) )
	{
		AssertMsg( 0, "No block filename!" );
		return false;
	}

	V_snprintf( m_szFullFilename, sizeof( m_szFullFilename ), "%s%s", GetPath(), pBlockFile );

	return true;
}

void CClientRecordingSessionBlock::Write( KeyValues *pOut )
{
	BaseClass::Write( pOut );

	pOut->SetInt( "download_status", (int)m_nDownloadStatus );
	pOut->SetInt( "download_error", (int)m_nHttpError );
	pOut->SetInt( "bytes_downloaded", m_uBytesDownloaded );
	pOut->SetInt( "data_invalid", (int)m_bDataInvalid );

	// Get just the filename and write that
	pOut->SetString( "filename", V_UnqualifiedFileName( m_szFullFilename ) );
}

void CClientRecordingSessionBlock::OnDelete()
{
	BaseClass::OnDelete();

	// Remove the actual binary block file itself
	g_pFullFileSystem->RemoveFile( m_szFullFilename );
}

bool CClientRecordingSessionBlock::AttemptToResetForDownload()
{
	// Attempt to download again?
	if ( ++m_nDownloadAttempts < 3 )
	{
		DBG( "Trying downloading again.\n" );

		m_nDownloadStatus = CClientRecordingSessionBlock::DOWNLOADSTATUS_READYTODOWNLOAD;
		return true;
	}

	return false;
}

bool CClientRecordingSessionBlock::ValidateData( const void *pData, int nSize, unsigned char *pOutHash/*=NULL*/ ) const
{
	unsigned char aDigest[16];
	if ( !g_pEngine->MD5_HashBuffer( aDigest, (const unsigned char *)pData, nSize, NULL ) )
		return false;

	if ( pOutHash )
	{
		V_memcpy( pOutHash, aDigest, sizeof( aDigest ) );
	}

	return V_memcmp( aDigest, m_aHash, sizeof( m_aHash ) ) == 0;
}

//----------------------------------------------------------------------------------------
