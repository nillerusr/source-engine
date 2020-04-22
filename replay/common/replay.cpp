//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "replay/replay.h"
#include "replay/iclientreplaycontext.h"
#include "replay/ireplaymanager.h"
#include "replay/replayutils.h"
#include "replay/screenshot.h"
#include "replay/shared_defs.h"
#include "replay/ireplayscreenshotmanager.h"
#include "replay/ireplayperformancemanager.h"
#include "replay/performance.h"
#include "KeyValues.h"
#include "filesystem.h"
#include "vgui/ILocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

extern IClientReplayContext *g_pClientReplayContext;
extern vgui::ILocalize *g_pVGuiLocalize;

//----------------------------------------------------------------------------------------

CReplay::CReplay()
:	m_pDownloadEventHandler( NULL ),
	m_pUserData( NULL ),
	m_bComplete( false ),
	m_bRequestedByUser( false ),
	m_bSaved( false ),
	m_bRendered( false ),
	m_bDirty( false ),
	m_bSavedDuringThisSession( true ),
	m_flLength( 0 ),
	m_nPlayerSlot( -1 ),
	m_nSpawnTick( -1 ),
	m_nDeathTick( -1 ),
	m_iMaxSessionBlockRequired( 0 ),
	m_nStatus( REPLAYSTATUS_INVALID ),
	m_hSession( REPLAY_HANDLE_INVALID ),
	m_pFileURL( NULL ),
	m_nPostDeathRecordTime( 0 ),
	m_flStartTime( 0.0f ),
	m_flNextUpdateTime( 0.0f )
{
	m_wszTitle[0] = L'\0';
	m_szMapName[0] = 0;
}

bool CReplay::IsDownloaded() const
{
	return m_nStatus == REPLAYSTATUS_READYTOCONVERT;
}

const CReplayPerformance *CReplay::GetPerformance( int i ) const
{
	return const_cast< CReplay * >( this )->GetPerformance( i );
}

CReplayPerformance *CReplay::GetPerformance( int i )
{
	if ( i < 0 || i >= m_vecPerformances.Count() )
		return NULL;

	return m_vecPerformances[ i ];
}

bool CReplay::FindPerformance( CReplayPerformance *pPerformance, int &iResult )
{
	const int it = m_vecPerformances.Find( pPerformance );
	if ( it == m_vecPerformances.InvalidIndex() )
	{
		iResult = -1;
		return false;
	}

	iResult = it;
	return true;
}

CReplayPerformance *CReplay::GetPerformanceWithTitle( const wchar_t *pTitle )
{
	FOR_EACH_VEC( m_vecPerformances, i )
	{
		CReplayPerformance *pCurPerformance = m_vecPerformances[ i ];
		if ( !V_wcscmp( pTitle, pCurPerformance->m_wszTitle ) )
		{
			return pCurPerformance;
		}
	}
	return NULL;
}

CReplayPerformance *CReplay::AddNewPerformance( bool bGenTitle/*=true*/, bool bGenFilename/*=true*/ )
{
	// Create a performance
	IReplayPerformanceManager *pPerformanceManager = g_pClientReplayContext->GetPerformanceManager();
	CReplayPerformance *pPerformance = pPerformanceManager->CreatePerformance( this );

	if ( bGenTitle )
	{
		// Give the performance a name
		pPerformance->AutoNameIfHasNoTitle( m_szMapName );
	}

	if ( bGenFilename )
	{
		// Generate a filename for the new performance
		pPerformance->SetFilename( pPerformanceManager->GeneratePerformanceFilename( this ) );
	}

	// Cache
	m_vecPerformances.AddToTail( pPerformance );
	
	return pPerformance;
}

void CReplay::AddPerformance( KeyValues *pIn )
{
	// Create a performance
	IReplayPerformanceManager *pPerformanceManager = g_pClientReplayContext->GetPerformanceManager();
	CReplayPerformance *pPerformance = pPerformanceManager->CreatePerformance( this );

	// Read
	pPerformance->Read( pIn );

	// Cache
	m_vecPerformances.AddToTail( pPerformance );
}

void CReplay::AddPerformance( CReplayPerformance *pPerformance )
{
	Assert( pPerformance );
	m_vecPerformances.AddToTail( pPerformance );
}

const CReplayTime &CReplay::GetItemDate() const
{
	return m_RecordTime;
}

bool CReplay::IsItemRendered() const
{
	return m_bRendered;
}

CReplay *CReplay::GetItemReplay()
{
	return this;
}

ReplayHandle_t	CReplay::GetItemReplayHandle() const
{
	return GetHandle();
}

QueryableReplayItemHandle_t	CReplay::GetItemHandle() const
{
	return GetHandle();
}

const wchar_t *CReplay::GetItemTitle() const
{
	return m_wszTitle;
}

void CReplay::SetItemTitle( const wchar_t *pTitle )
{
	V_wcsncpy( m_wszTitle, pTitle, sizeof( m_wszTitle ) );
}

float CReplay::GetItemLength() const
{
	return m_flLength;
}

void *CReplay::GetUserData()
{
	return m_pUserData;
}

void CReplay::SetUserData( void* pUserData )
{
	m_pUserData = pUserData;
}

bool CReplay::IsItemAMovie() const
{
	return false;
}

void CReplay::AddScreenshot( int nWidth, int nHeight, const char *pBaseFilename )
{
	m_vecScreenshots.AddToTail( new CReplayScreenshot( nWidth, nHeight, pBaseFilename ) );
}

void CReplay::AutoNameTitleIfEmpty()
{
	// Autoname it
	if ( !m_wszTitle[0] )
	{
		Replay_GetAutoName( m_wszTitle, sizeof( m_wszTitle ), m_szMapName );
	}
}

const char *CReplay::GetSubKeyTitle() const
{
	return Replay_va( "replay_%i", GetHandle() );
}

const char *CReplay::GetPath() const
{
	return Replay_va( "%s%s%c", g_pClientReplayContext->GetBaseDir(), SUBDIR_REPLAYS, CORRECT_PATH_SEPARATOR );
}

void CReplay::OnDelete()
{
	BaseClass::OnDelete();

	// Delete reconstructed replay if one exists
	if ( HasReconstructedReplay() )
	{
		g_pFullFileSystem->RemoveFile( m_strReconstructedFilename.Get() );
	}

	// Delete screenshots
	g_pClientReplayContext->GetScreenshotManager()->DeleteScreenshotsForReplay( this );

	// TODO: Delete performance(s)
}

bool CReplay::Read( KeyValues *pIn )
{
	if ( !BaseClass::Read( pIn ) )
		return false;

	m_hSession = (ReplayHandle_t)pIn->GetInt( "session", REPLAY_HANDLE_INVALID );
	V_strcpy_safe( m_szMapName, pIn->GetString( "map", "" ) );
	m_nSpawnTick = pIn->GetInt( "spawn_tick", -1 );
	m_nDeathTick = pIn->GetInt( "death_tick", -1 );
	m_nStatus = static_cast< CReplay::ReplayStatus_t >( pIn->GetInt( "status", (int)CReplay::REPLAYSTATUS_INVALID ) );
	m_bComplete = pIn->GetInt( "complete" ) != 0;
	m_flLength = pIn->GetFloat( "length" );
	m_nPostDeathRecordTime = pIn->GetInt( "postdeathrecordtime" );
	m_bRendered = pIn->GetInt( "rendered" ) != 0;
	m_nPlayerSlot = pIn->GetInt( "player_slot", -1 );
	m_iMaxSessionBlockRequired = pIn->GetInt( "max_block", 0 );	Assert( m_iMaxSessionBlockRequired >= 0 );
	m_flStartTime = pIn->GetFloat( "start_time", -1.0f );		Assert( m_flStartTime >= 0.0f );
	V_wcsncpy( m_wszTitle, pIn->GetWString( "title" ), sizeof( m_wszTitle ) );

	// Read reconstructed filename and infer path
	const char *pReplaysDir = g_pClientReplayContext->GetReplayManager()->GetReplaysDir();
	const char *pReconFilename = pIn->GetString( "recon_filename" );
	if ( pReconFilename[0] != 0 )
	{
		m_strReconstructedFilename = Replay_va( "%s%s", pReplaysDir, pReconFilename );
	}

	// Read screenshots
	KeyValues *pScreenshots = pIn->FindKey( "screenshots" );
	if ( pScreenshots )
	{
		FOR_EACH_TRUE_SUBKEY( pScreenshots, pScreenshot )
		{
			int nWidth = pScreenshot->GetInt( "width" );
			int nHeight = pScreenshot->GetInt( "height" );
			const char *pBaseFilename = pScreenshot->GetString( "base_filename" );
			AddScreenshot( nWidth, nHeight, pBaseFilename );
		}
	}

	// Read performances
	KeyValues *pPerformances = pIn->FindKey( "edits" );
	if ( pPerformances )
	{
		FOR_EACH_TRUE_SUBKEY( pPerformances, pPerformance )
		{
			AddPerformance( pPerformance );
		}
	}

	// Record time
	KeyValues *pRecordTimeSubKey = pIn->FindKey( "record_time" );
	if ( pRecordTimeSubKey )
	{
		m_RecordTime.Read( pRecordTimeSubKey );
	}

	// Mark replay as saved, since it was just loaded from disk
	m_bSaved = true;

	return true;
}

void CReplay::Write( KeyValues *pOut )
{
	BaseClass::Write( pOut );

	pOut->SetString( "map", m_szMapName );
	pOut->SetInt( "session", m_hSession );
	pOut->SetInt( "spawn_tick", m_nSpawnTick );
	pOut->SetInt( "death_tick", m_nDeathTick );
	pOut->SetInt( "status", static_cast< int >( m_nStatus ) );
	pOut->SetInt( "complete", static_cast< int >( m_bComplete ) );
	pOut->SetFloat( "length", m_flLength );
	pOut->SetInt( "postdeathrecordtime", m_nPostDeathRecordTime );
	pOut->SetInt( "rendered", m_bRendered );
	pOut->SetInt( "player_slot", m_nPlayerSlot );
	pOut->SetInt( "max_block", m_iMaxSessionBlockRequired );
	pOut->SetFloat( "start_time", m_flStartTime );
	pOut->SetWString( "title", m_wszTitle );

	// Store only filename for reconstructed .dem
	if ( !m_strReconstructedFilename.IsEmpty() )
	{
		const char *pReconFilename = V_UnqualifiedFileName( m_strReconstructedFilename.Get() );
		if ( pReconFilename[0] )
		{
			pOut->SetString( "recon_filename", pReconFilename );
		}
	}

	// Write screenshots
	KeyValues *pScreenshots = new KeyValues( "screenshots" );
	pOut->AddSubKey( pScreenshots );
	for ( int i = 0; i < m_vecScreenshots.Count(); ++i )
	{
		KeyValues *pScreenshotOut = new KeyValues( "screenshot" );
		CReplayScreenshot *pScreenshot = m_vecScreenshots[ i ];
		pScreenshotOut->SetInt( "width", pScreenshot->m_nWidth );
		pScreenshotOut->SetInt( "height", pScreenshot->m_nHeight );
		pScreenshotOut->SetString( "base_filename", pScreenshot->m_szBaseFilename );
		pScreenshots->AddSubKey( pScreenshotOut );
	}

	// Write performances
	KeyValues *pPerformances = new KeyValues( "edits" );
	pOut->AddSubKey( pPerformances );
	for ( int i = 0; i < m_vecPerformances.Count(); ++i )
	{
		KeyValues *pPerfOut = new KeyValues( "edit" );
		CReplayPerformance *pPerformance = m_vecPerformances[ i ];
		pPerformance->Write( pPerfOut );
		pPerformances->AddSubKey( pPerfOut );
	}

	KeyValues *pRecordTime = new KeyValues( "record_time" );
	pOut->AddSubKey( pRecordTime );
	m_RecordTime.Write( pRecordTime );

	// Mark as saved
	m_bSaved = true;
}

bool CReplay::HasReconstructedReplay() const
{
	return !m_strReconstructedFilename.IsEmpty() &&
		   g_pFullFileSystem->FileExists( m_strReconstructedFilename.Get() );
}

bool CReplay::IsSignificantBlock( int iBlockReconstruction ) const
{
	return iBlockReconstruction <= m_iMaxSessionBlockRequired;
}

void CReplay::OnComplete()
{
	AutoNameTitleIfEmpty();
}

//----------------------------------------------------------------------------------------
