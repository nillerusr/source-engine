//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "errorsystem.h"
#include "replay/ienginereplay.h"
#include "vgui/ILocalize.h"
#include "shared_replaycontext.h"

#if !defined( DEDICATED )

#include "cl_downloader.h"
#include "cl_sessionblockdownloader.h"
#include "cl_recordingsessionblock.h"
#include "replay/iclientreplay.h"

extern IClientReplay *g_pClient;

#endif	// !defined( DEDICATED )

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

extern IEngineReplay *g_pEngine;
extern vgui::ILocalize *g_pVGuiLocalize;

//----------------------------------------------------------------------------------------

CErrorSystem::CErrorSystem( IErrorReporter *pErrorReporter )
:	m_pErrorReporter( pErrorReporter )
{
}

CErrorSystem::~CErrorSystem()
{
	Clear();
}

void CErrorSystem::Clear()
{
	FOR_EACH_LL( m_lstErrors, i )
	{
		wchar_t *pText = m_lstErrors[ i ];
		delete [] pText;
	}

	m_lstErrors.RemoveAll();
}

void CErrorSystem::AddError( const wchar_t *pError )
{
	if ( !pError || !pError[0] )
		return;

	// Cache a copied version of the string
	const int nLen = wcslen( pError ) + 1;
	wchar_t *pNewError = new wchar_t[ nLen ];
	const int nSize = nLen * sizeof( wchar_t );
	V_wcsncpy( pNewError, pError, nSize );

	m_lstErrors.AddToTail( pNewError );
}

void CErrorSystem::AddError( const char *pError )
{
	if ( !pError || !pError[0] )
		return;

	wchar_t wszError[1024];
	V_UTF8ToUnicode( pError, wszError, sizeof( wszError ) );

	AddError( wszError );
}

void CErrorSystem::AddErrorFromTokenName( const char *pToken )
{
	if ( g_pVGuiLocalize )
	{
		AddError( g_pVGuiLocalize->Find( pToken ) );
	}
	else
	{
		AddError( pToken );
	}
}

void CErrorSystem::AddFormattedErrorFromTokenName( const char *pFormatToken/*=NULL*/, KeyValues *pFormatArgs/*=NULL*/ )
{
	if ( !pFormatToken )
	{
		AssertMsg( 0, "Error token should always be valid." );
		return;
	}

	wchar_t wszErrorStr[1024];
	if ( g_pVGuiLocalize )
	{
		g_pVGuiLocalize->ConstructString( wszErrorStr, sizeof( wszErrorStr ), pFormatToken, pFormatArgs );
	}
	else
	{
		V_UTF8ToUnicode( pFormatToken, wszErrorStr, sizeof( wszErrorStr ) );
	}


	// Add the error
	AddError( wszErrorStr );

	// Delete args
	pFormatArgs->deleteThis();
}

#if !defined( DEDICATED )

int g_nGenericErrorCounter = 0;

void CErrorSystem::OGS_ReportSessionBlockDownloadError( const CHttpDownloader *pDownloader, const CClientRecordingSessionBlock *pBlock,
														int nLocalFileSize, int nMaxBlock, const bool *pSizesDiffer,
														const bool *pHashFail, uint8 *pLocalHash )
														
{
	// Create a download error and queue for upload
	KeyValues *pDownloadError = pDownloader->GetOgsRow( g_nGenericErrorCounter );
	g_pClient->UploadOgsData( pDownloadError, false );

	// Create block download error
	KeyValues *pBlockDownloadError = new KeyValues( "TF2ReplayBlockDownloadErrors" );
	pBlockDownloadError->SetInt( "ErrorCounter", g_nGenericErrorCounter );
	pBlockDownloadError->SetInt( "NumCurrentDownloads", CSessionBlockDownloader::sm_nNumCurrentDownloads );
	pBlockDownloadError->SetInt( "MaxBlock", nMaxBlock );
	pBlockDownloadError->SetInt( "RemoteStatus", (int)pBlock->m_nRemoteStatus );
	pBlockDownloadError->SetInt( "ReconstructionIndex", pBlock->m_iReconstruction );
	pBlockDownloadError->SetInt( "RemoteFileSize", (int)pBlock->m_uFileSize );
	pBlockDownloadError->SetInt( "LocalFileSize", nLocalFileSize );
	pBlockDownloadError->SetInt( "NumDownloadAttempts", pBlock->GetNumDownloadAttempts() );

	// Only include these if appropriate - otherwise, let them be NULL for the given row
	if ( pSizesDiffer )
	{
		pBlockDownloadError->SetInt( "SizesDiffer", (int)*pSizesDiffer );
	}

	if ( pHashFail )
	{
		pBlockDownloadError->SetInt( "HashFail", (int)*pHashFail );

		// Include hashes
		char szRemoteHash[64], szLocalHash[64];
		V_binarytohex( pBlock->m_aHash, sizeof( pBlock->m_aHash ), szRemoteHash, sizeof( szRemoteHash ) );
		V_binarytohex( pLocalHash, sizeof( pBlock->m_aHash ), szLocalHash, sizeof( szLocalHash ) );
		pBlockDownloadError->SetString( "RemoteHash", szRemoteHash );
		pBlockDownloadError->SetString( "LocalHash", szLocalHash );
	}

	// Upload block download error
	g_pClient->UploadOgsData( pBlockDownloadError, false );

	// Upload generic error and link to this specific block error.
	OGS_ReportGenericError( "Block download failed" );
}

void CErrorSystem::OGS_ReportSessioInfoDownloadError( const CHttpDownloader *pDownloader, const char *pErrorToken )
{
	// Create a download error and queue for upload
	KeyValues *pDownloadError = pDownloader->GetOgsRow( g_nGenericErrorCounter );
	g_pClient->UploadOgsData( pDownloadError, false );
	
	// Create session info download error
	KeyValues *pSessionInfoDownloadError = new KeyValues( "TF2ReplaySessionInfoDownloadErrors" );
	pSessionInfoDownloadError->SetInt( "ErrorCounter", g_nGenericErrorCounter++ );
	pSessionInfoDownloadError->SetString( "SessionInfoDownloadErrorID", pErrorToken );
	g_pClient->UploadOgsData( pSessionInfoDownloadError, false );

	// Upload generic error and link to this specific block error.
	OGS_ReportGenericError( "Session info download failed" );
}

// Note: we use the ErrorCounter as part of the key and so it must be unique. This means that all
// special error tables (ie., session info download errors) must call back into this base function
// to write out the base error and increment the counter.
void CErrorSystem::OGS_ReportGenericError( const char *pGenericErrorToken )
{
	KeyValues *pGenericError = new KeyValues( "TF2ReplayErrors" );
	pGenericError->SetInt( "ErrorCounter", g_nGenericErrorCounter++ );
	pGenericError->SetString( "ReplayErrorID", pGenericErrorToken );

	// Upload the generic error row now
	g_pClient->UploadOgsData( pGenericError, true );

	// Next error!
	++g_nGenericErrorCounter;
}

#endif	// !defined( DEDICATED )

float CErrorSystem::GetNextThinkTime() const
{
	return g_pEngine->GetHostTime() + 5.0f;
}

void CErrorSystem::Think()
{
	CBaseThinker::Think();

	if ( m_lstErrors.Count() == 0 )
		return;

	const int nMaxLen = 4096;
	wchar_t wszErrorText[ nMaxLen ] = L"";
	FOR_EACH_LL( m_lstErrors, i )
	{
		const wchar_t *pError = m_lstErrors[ i ];
		if ( wcslen( wszErrorText ) + wcslen( pError ) + 1 >= nMaxLen )
			break;

		wcscat( wszErrorText, pError );
		wcscat( wszErrorText, L"\n" );
	}

	// Report now
	m_pErrorReporter->ReportErrorsToUser( wszErrorText );

	// Clear
	Clear();
}

//----------------------------------------------------------------------------------------

