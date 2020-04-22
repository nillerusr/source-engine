//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#if defined( WIN32 )
#include "winlite.h"
#include <WinInet.h>
#endif

#include "cl_downloader.h"
#include "engine/requestcontext.h"
#include "replaysystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

extern IEngineClientReplay	*g_pEngineClient;
extern IEngineReplay		*g_pEngine;

//----------------------------------------------------------------------------------------

CHttpDownloader::CHttpDownloader( IDownloadHandler *pHandler )
:	m_pHandler( pHandler ),
	m_flNextThinkTime( 0.0f ),
	m_uBytesDownloaded( 0 ),
	m_uSize( 0 ),
	m_pThreadState( NULL ),
	m_bDone( false ),
	m_nHttpError( HTTP_ERROR_NONE ),
	m_nHttpStatus( HTTP_INVALID ),
	m_pBytesDownloaded( NULL ),
	m_pUserData( NULL )
{
}

CHttpDownloader::~CHttpDownloader()
{
	CleanupThreadIfDone();
}

bool CHttpDownloader::CleanupThreadIfDone()
{
	if ( !m_pThreadState || !m_pThreadState->threadDone )
		return false;

	// NOTE: The context's "data" member will have already been cleaned up by the
	// download thread at this point.
	delete m_pThreadState;
	m_pThreadState = NULL;

	return true;
}

bool CHttpDownloader::BeginDownload( const char *pURL, const char *pGamePath, void *pUserData, uint32 *pBytesDownloaded )
{
	if ( !pURL || !pURL[0] )
		return false;

	m_pThreadState = new RequestContext_t();
	if ( !m_pThreadState )
		return false;

	// Cache any user data
	m_pUserData = pUserData;

	// Cache bytes downloaded
	m_pBytesDownloaded = pBytesDownloaded;

	// Setup request context
	Replay_CrackURL( pURL, m_pThreadState->baseURL, m_pThreadState->urlPath );
	m_pThreadState->bAsHTTP = true;

	if ( pGamePath )
	{
		m_pThreadState->bSuppressFileWrite = false;
		V_strcpy( m_pThreadState->gamePath, pGamePath );

		// Generate the actual filename to save.  Well, it's not
		// absolute, but this will work.
		V_strcpy_safe( m_pThreadState->absLocalPath, g_pEngine->GetGameDir() );
		V_AppendSlash( m_pThreadState->absLocalPath, sizeof(m_pThreadState->absLocalPath) );
		V_strcat_safe( m_pThreadState->absLocalPath, pGamePath );
	}
	else
	{
		m_pThreadState->bSuppressFileWrite = true;
	}

	// Cache URL - for debugging
	V_strcpy( m_szURL, pURL );

	// Spawn the download thread
	extern IDownloadSystem *g_pDownloadSystem;
	return g_pDownloadSystem->CreateDownloadThread( m_pThreadState ) != 0;
}

void CHttpDownloader::AbortDownloadAndCleanup()
{
	// Make sure that this function isn't executed simultaneously by
	// multiple threads in order to avoid use-after-free crashes during
	// shutdown.
	AUTO_LOCK( m_lock );

	if ( !m_pThreadState )
		return;

	// Thread already completed?
	if ( m_pThreadState->threadDone )
	{
		CleanupThreadIfDone();
		return;
	}

	// Loop until the thread cleans up
	m_pThreadState->shouldStop = true;
	while ( !m_pThreadState->threadDone )
		;

	// Cache state for handler
	m_nHttpError = m_pThreadState->error;
	m_nHttpStatus = HTTP_ABORTED;	// Force this to be safe
	m_uBytesDownloaded = 0;
	m_uSize = m_pThreadState->nBytesTotal;
	m_bDone = true;

	InvokeHandler();
	CleanupThreadIfDone();
}

void CHttpDownloader::Think()
{
	const float flHostTime = g_pEngine->GetHostTime();
	if ( m_flNextThinkTime > flHostTime )
		return;

	if ( !m_pThreadState )
		return;

	// If thread is done, cleanup now
	if ( CleanupThreadIfDone() )
		return;

	// If we haven't already set shouldStop, check the download status
	if ( !m_pThreadState->shouldStop )
	{
		// Security measure: make sure the file size isn't outrageous
		const bool bEvilFileSize = m_pThreadState->nBytesTotal &&
			m_pThreadState->nBytesTotal >= DOWNLOAD_MAX_SIZE;
#if _DEBUG
		extern ConVar replay_simulate_evil_download_size;
		if ( replay_simulate_evil_download_size.GetBool() || bEvilFileSize )
#else
		if ( bEvilFileSize )
#endif
		{
			AbortDownloadAndCleanup();
			return;
		}

		bool bConnecting = false;	// For fall-through in HTTP_CONNECTING case.

#if _DEBUG
		extern ConVar replay_simulatedownloadfailure;
		if ( replay_simulatedownloadfailure.GetInt() == 1 )
		{
			m_pThreadState->status = HTTP_ERROR;
		}
#endif

		switch ( m_pThreadState->status )
		{
		case HTTP_CONNECTING:

			// Call connecting handler
			if ( m_pHandler )
			{
				m_pHandler->OnConnecting( this );
			}

			bConnecting = true;
			
			// Fall-through

		case HTTP_FETCH:

			m_uBytesDownloaded = (uint32)m_pThreadState->nBytesCurrent;
			m_uSize = m_pThreadState->nBytesTotal;

			Assert( m_uBytesDownloaded <= m_uSize );

			// Call fetch handle
			if ( !bConnecting && m_pHandler )
			{
				m_pHandler->OnFetch( this );
			}

			break;

		case HTTP_ABORTED:
		case HTTP_DONE:
		case HTTP_ERROR:

			// Cache state
			m_nHttpError = m_pThreadState->error;
			m_nHttpStatus = m_pThreadState->status;
			m_uBytesDownloaded = (uint32)m_pThreadState->nBytesCurrent;
			m_uSize = m_pThreadState->nBytesTotal;	// NOTE: Need to do this here in the case that a file is small enough that we never hit HTTP_FETCH
			m_bDone = true;

			// Call handler
			InvokeHandler();

			// Tell the thread to cleanup so we can free it
			m_pThreadState->shouldStop = true;

			break;
		}
	}

	// Write bytes for user if changed
	if ( m_pBytesDownloaded && *m_pBytesDownloaded != m_uBytesDownloaded )
	{
		*m_pBytesDownloaded = m_uBytesDownloaded;
		IF_REPLAY_DBG( Warning( "%s: Downloaded %i/%i bytes\n", m_szURL, m_uBytesDownloaded, m_uSize ) );
	}

	// Set next think time
	m_flNextThinkTime = flHostTime + 0.1f;
}

void CHttpDownloader::InvokeHandler()
{
	if ( !m_pHandler )
		return;

	// NOTE: Don't delete the downloader in OnDownloadComplete()!
	m_pHandler->OnDownloadComplete( this, m_pThreadState->data );
}

// This does not increment the "ErrorCounter" field and should only be called from code
// that eventually calls into OGS_ReportGenericError().
KeyValues *CHttpDownloader::GetOgsRow( int nErrorCounter ) const
{
	KeyValues *pResult = new KeyValues( "TF2ReplayHttpDownloadErrors" );
	pResult->SetInt( "ErrorCounter", nErrorCounter );
	pResult->SetInt( "BytesDownloaded", (int)m_uBytesDownloaded );
	pResult->SetInt( "BytesTotal", (int)m_uSize );
	pResult->SetInt( "HttpStatus", m_nHttpStatus );
	pResult->SetInt( "HttpError", m_nHttpError );
	pResult->SetString( "URL", m_szURL );

	return pResult;
}

/*static*/ const char *CHttpDownloader::GetHttpErrorToken( HTTPError_t nError )
{
	switch ( nError )
	{
		case HTTP_ERROR_ZERO_LENGTH_FILE:	return "#HTTPError_ZeroLengthFile";
		case HTTP_ERROR_CONNECTION_CLOSED:	return "#HTTPError_ConnectionClosed";
		case HTTP_ERROR_INVALID_URL:		return "#HTTPError_InvalidURL";
		case HTTP_ERROR_INVALID_PROTOCOL:	return "#HTTPError_InvalidProtocol";
		case HTTP_ERROR_CANT_BIND_SOCKET:	return "#HTTPError_CantBindSocket";
		case HTTP_ERROR_CANT_CONNECT:		return "#HTTPError_CantConnect";
		case HTTP_ERROR_NO_HEADERS:			return "#HTTPError_NoHeaders";
		case HTTP_ERROR_FILE_NONEXISTENT:	return "#HTTPError_NonExistent";
	}

	return "#HTTPError_Unknown";
}

//----------------------------------------------------------------------------------------

#ifdef _DEBUG

CHttpDownloader *g_pTestDownload = NULL;
ConVar replay_forcedownloadurl( "replay_forcedownloadurl", "" );

CON_COMMAND( replay_testdownloader_start, "" )
{
	const char *pGamePath = Replay_va( "%s%s", CL_GetRecordingSessionBlockManager()->GetSavePath(), "testdownload" );

	g_pTestDownload = new CHttpDownloader();
	g_pTestDownload->BeginDownload( args[1], pGamePath );
}

CON_COMMAND( replay_testdownloader_abort, "" )
{
	if ( !g_pTestDownload )
		return;

	g_pTestDownload->AbortDownloadAndCleanup();

	delete g_pTestDownload;
	g_pTestDownload = NULL;
}

#endif