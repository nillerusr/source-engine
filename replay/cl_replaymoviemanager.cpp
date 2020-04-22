//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cl_replaymoviemanager.h"
#include "replay/ireplaymoviemanager.h"
#include "replay/ireplaymovierenderer.h"
#include "replay/replay.h"
#include "replay/replayutils.h"
#include "replay/rendermovieparams.h"
#include "replay/shared_defs.h"
#include "cl_replaymovie.h"
#include "cl_renderqueue.h"
#include "cl_replaycontext.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "replaysystem.h"
#include "cl_replaymanager.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/materialsystem_config.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

#define MOVIE_MANAGER_VERSION		1

//----------------------------------------------------------------------------------------

extern IMaterialSystem *materials;

//----------------------------------------------------------------------------------------

CReplayMovieManager::CReplayMovieManager()
:	m_pPendingMovie( NULL ),
	m_pVidModeSettings( NULL ),
	m_pRenderMovieSettings( NULL ),
	m_bIsRendering( false ),
	m_bRenderingCancelled( false )
{
	m_pVidModeSettings = new MaterialSystem_Config_t();
	m_pRenderMovieSettings = new RenderMovieParams_t();

	m_wszCachedMovieTitle[0] = L'\0';
}

CReplayMovieManager::~CReplayMovieManager()
{
	delete m_pVidModeSettings;
}

bool CReplayMovieManager::Init()
{
	// Create "rendered" dir
	const char *pRenderedDir = Replay_va( "%s%s%c", CL_GetReplayBaseDir(), SUBDIR_RENDERED, CORRECT_PATH_SEPARATOR );
	g_pFullFileSystem->CreateDirHierarchy( pRenderedDir );

	return BaseClass::Init();
}

int CReplayMovieManager::GetMovieCount()
{
	return Count();
}

void CReplayMovieManager::GetMovieList( CUtlLinkedList< IReplayMovie * > &list )
{
	FOR_EACH_OBJ( this, i )
	{
		list.AddToTail( ToMovie( m_vecObjs[ i ] ) );
	}
}

IReplayMovie *CReplayMovieManager::GetMovie( ReplayHandle_t hMovie )
{
	return ToMovie( Find( hMovie ) );
}

void CReplayMovieManager::AddMovie( CReplayMovie *pNewMovie )
{
	Add( pNewMovie );
	Save();
}

CReplayMovie *CReplayMovieManager::Create()
{
	return new CReplayMovie();
}

const char *CReplayMovieManager::GetRelativeIndexPath() const
{
	return Replay_va( "%s%c", SUBDIR_MOVIES, CORRECT_PATH_SEPARATOR );
}

IReplayMovie *CReplayMovieManager::CreateAndAddMovie( ReplayHandle_t hReplay )
{
	CReplayMovie *pNewMovie = CreateAndGenerateHandle();	// Sets m_hThis (which is accessed via GetHandle())

	// Cache replay handle.
	pNewMovie->m_hReplay = hReplay;

	// Copy cached render settings to the movie itself.
	V_memcpy( &pNewMovie->m_RenderSettings, &m_pRenderMovieSettings->m_Settings, sizeof( ReplayRenderSettings_t ) );

	AddMovie( pNewMovie );

	return pNewMovie;
}

void CReplayMovieManager::DeleteMovie( ReplayHandle_t hMovie )
{
	// Cache owner replay
	CReplayMovie *pMovie = Find( hMovie );
	CReplay *pOwnerReplay = pMovie ? CL_GetReplayManager()->GetReplay( pMovie->m_hReplay ) : NULL;

	Remove( hMovie );

	// If no more movies for the given replay, mark as unrendered & save
	if ( pOwnerReplay && GetNumMoviesDependentOnReplay( pOwnerReplay ) == 0 )
	{
		pOwnerReplay->m_bRendered = false;
		CL_GetReplayManager()->FlagReplayForFlush( pOwnerReplay, false );
	}
}

int CReplayMovieManager::GetNumMoviesDependentOnReplay( const CReplay *pReplay )
{
	if ( !pReplay )
		return 0;

	// Go through all movies and find any that depend on the given replay
	int nNumMovies = 0;
	FOR_EACH_OBJ( this, i )
	{
		CReplayMovie *pMovie = ToMovie( m_vecObjs[ i ] );
		if ( pMovie->m_hReplay == pReplay->GetHandle() )
		{
			++nNumMovies;
		}
	}

	return nNumMovies;
}

void CReplayMovieManager::SetPendingMovie( IReplayMovie *pMovie )
{
	m_pPendingMovie = pMovie;
}

IReplayMovie *CReplayMovieManager::GetPendingMovie()
{
	return m_pPendingMovie;
}

void CReplayMovieManager::FlagMovieForFlush( IReplayMovie *pMovie, bool bImmediate )
{
	FlagForFlush( CastMovie( pMovie ), bImmediate );
}

CReplayMovie *CReplayMovieManager::CastMovie( IReplayMovie *pMovie )
{
	return static_cast< CReplayMovie * >( pMovie );
}

int CReplayMovieManager::GetVersion() const
{
	return MOVIE_MANAGER_VERSION;
}

IReplayContext *CReplayMovieManager::GetReplayContext() const
{
	return g_pClientReplayContextInternal;
}

float CReplayMovieManager::GetNextThinkTime() const
{
	return 0.1f;
}

void CReplayMovieManager::CacheMovieTitle( const wchar_t *pTitle )
{
	V_wcsncpy( m_wszCachedMovieTitle, pTitle, sizeof( m_wszCachedMovieTitle ) );
}

void CReplayMovieManager::GetCachedMovieTitleAndClear( wchar_t *pOut, int nOutBufLength )
{
	const int nLength = wcslen( m_wszCachedMovieTitle );
	wcsncpy( pOut, m_wszCachedMovieTitle, nOutBufLength );
	pOut[ nLength ] = L'\0';
	m_wszCachedMovieTitle[0] = L'\0';
}

void CReplayMovieManager::AddReplayForRender( CReplay *pReplay, int iPerformance )
{
	if ( !g_pClientReplayContextInternal->ReconstructReplayIfNecessary( pReplay ) )
	{
		CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_Err_Render_ReconstructFailed" );
		return;
	}

	// Store in the demo player's list
	g_pReplayDemoPlayer->AddReplayToList( pReplay->GetHandle(), iPerformance );
}

void CReplayMovieManager::ClearRenderCancelledFlag()
{
	m_bRenderingCancelled = false;
}

void CReplayMovieManager::RenderMovie( RenderMovieParams_t const& params )
{
	// Save state
	m_bIsRendering = true;

	// Change video settings for recording
	SetupVideo( params );

	// Clear any old replays in the player
	g_pReplayDemoPlayer->ClearReplayList();

	// Render all unrendered replays
	if ( params.m_hReplay == REPLAY_HANDLE_INVALID )
	{
		CRenderQueue *pRenderQueue = g_pClientReplayContextInternal->m_pRenderQueue;
		const int nQueueCount = pRenderQueue->GetCount();
		for ( int i = 0; i < nQueueCount; ++i )
		{
			ReplayHandle_t hCurReplay;
			int iCurPerformance;

			if ( !pRenderQueue->GetEntryData( i, &hCurReplay, &iCurPerformance ) )
				continue;

			CReplay *pReplay = CL_GetReplayManager()->GetReplay( hCurReplay );
			if ( !pReplay )
				continue;

			AddReplayForRender( pReplay, iCurPerformance );
		}
	}
	else
	{
		// Cache the title
		CReplayMovieManager *pMovieManager = CL_GetMovieManager();
		pMovieManager->CacheMovieTitle( params.m_wszTitle );

		// Only render the specified replay
		AddReplayForRender( CL_GetReplayManager()->GetReplay( params.m_hReplay ), params.m_iPerformance );
	}

	g_pReplayDemoPlayer->PlayNextReplay();
}

void CReplayMovieManager::RenderNextMovie()
{
	m_bIsRendering = true;

	g_pReplayDemoPlayer->PlayNextReplay();
}

void CReplayMovieManager::SetupHighDetailModels()
{
	g_pEngine->Cbuf_AddText( "r_rootlod 0\n" );
}

void CReplayMovieManager::SetupHighDetailTextures()
{
	g_pEngine->Cbuf_AddText( "mat_picmip -1\n" );
}

void CReplayMovieManager::SetupHighQualityAntialiasing()
{
	int nNumSamples = 1;
	int nQualityLevel = 0;

	if ( materials->SupportsCSAAMode(8, 2) )
	{
		nNumSamples = 8;
		nQualityLevel = 2;
	}
	else if ( materials->SupportsMSAAMode(8) )
	{
		nNumSamples = 8;
		nQualityLevel = 0;
	}
	else if ( materials->SupportsCSAAMode(4, 4) )
	{
		nNumSamples = 4;
		nQualityLevel = 4;
	}
	else if ( materials->SupportsCSAAMode(4, 2) )
	{
		nNumSamples = 4;
		nQualityLevel = 2;
	}
	else if ( materials->SupportsMSAAMode(6) )
	{
		nNumSamples = 6;
		nQualityLevel = 0;
	}
	else if ( materials->SupportsMSAAMode(4) )
	{
		nNumSamples = 4;
		nQualityLevel = 0;
	}
	else if ( materials->SupportsMSAAMode(2) )
	{
		nNumSamples = 2;
		nQualityLevel = 0;
	}

	g_pEngine->Cbuf_AddText( Replay_va( "mat_antialias %i\n", nNumSamples ) );
	g_pEngine->Cbuf_AddText( Replay_va( "mat_aaquality %i\n", nQualityLevel ) );
}

void CReplayMovieManager::SetupHighQualityFiltering()
{
	g_pEngine->Cbuf_AddText( "mat_forceaniso\n" );
}

void CReplayMovieManager::SetupHighQualityShadowDetail()
{
	if ( materials->SupportsShadowDepthTextures() )
	{
		g_pEngine->Cbuf_AddText( "r_shadowrendertotexture 1\n" );
		g_pEngine->Cbuf_AddText( "r_flashlightdepthtexture 1\n" );
	}
	else
	{
		g_pEngine->Cbuf_AddText( "r_shadowrendertotexture 1\n" );
		g_pEngine->Cbuf_AddText( "r_flashlightdepthtexture 0\n" );
	}
}

void CReplayMovieManager::SetupHighQualityHDR()
{
	ConVarRef mat_dxlevel( "mat_dxlevel" );
	if ( mat_dxlevel.GetInt() < 80 )
		return;

	g_pEngine->Cbuf_AddText( Replay_va( "mat_hdr_level %i\n", materials->SupportsHDRMode( HDR_TYPE_INTEGER ) ? 2 : 1 ) );
}

void CReplayMovieManager::SetupHighQualityWaterDetail()
{
#ifndef _X360
	g_pEngine->Cbuf_AddText( "r_waterforceexpensive 1\n" );
#endif
	g_pEngine->Cbuf_AddText( "r_waterforcereflectentities 1\n" );
}

void CReplayMovieManager::SetupMulticoreRender()
{
	g_pEngine->Cbuf_AddText( "mat_queue_mode 0\n" );
}

void CReplayMovieManager::SetupHighQualityShaderDetail()
{
	g_pEngine->Cbuf_AddText( "mat_reducefillrate 0\n" );
}

void CReplayMovieManager::SetupColorCorrection()
{
	g_pEngine->Cbuf_AddText( "mat_colorcorrection 1\n" );
}

void CReplayMovieManager::SetupMotionBlur()
{
	g_pEngine->Cbuf_AddText( "mat_motion_blur_enabled 1\n" );
}

void CReplayMovieManager::SetupVideo( RenderMovieParams_t const &params )
{
	// Get current video config
	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();

	// Cache config
	V_memcpy( m_pVidModeSettings, &config, sizeof( config ) );

	// Cache quit when done
	V_memcpy( m_pRenderMovieSettings, &params, sizeof( params ) );

	g_pEngine->Cbuf_Execute();
}

void CReplayMovieManager::CompleteRender( bool bSuccess, bool bShowBrowser )
{
	// Store state
	m_bIsRendering = false;

	// Shutdown renderer
	IReplayMovieRenderer *pRenderer = CL_GetMovieRenderer();
	if ( pRenderer )
	{
		pRenderer->ShutdownRenderer();
	}

	if ( !bSuccess )
	{
		// Delete the movie from the manager
		IReplayMovie *pMovie = CL_GetMovieManager()->GetPendingMovie();
		if ( pMovie )
		{
			CL_GetMovieManager()->DeleteMovie( pMovie->GetMovieHandle() );
		}
	}

	// Clear render queue if we're done
	if ( bShowBrowser )
	{
		CL_GetRenderQueue()->Clear();
	}

	// Notify UI that rendering is complete
	g_pClient->OnRenderComplete( *m_pRenderMovieSettings, m_bRenderingCancelled, bSuccess, bShowBrowser );

	// Quit now?
	if ( m_pRenderMovieSettings->m_bQuitWhenFinished )
	{
		g_pEngine->HostState_Shutdown();
		return;
	}

	// Otherwise, play a sound.
	g_pClient->PlaySound( "replay\\rendercomplete.wav" );
}

void CReplayMovieManager::CancelRender()
{
	m_bRenderingCancelled = true;
	CompleteRender( false, true );
	g_pEngine->Host_Disconnect( false );	// CReplayDemoPlayer::StopPlayback() will be called
}

void CReplayMovieManager::GetMoviesAsQueryableItems( CUtlLinkedList< IQueryableReplayItem *, int > &lstMovies )
{
	lstMovies.RemoveAll();
	FOR_EACH_OBJ( this, i )
	{
		lstMovies.AddToHead( ToMovie( m_vecObjs[ i ] ) );
	}
}

const char *CReplayMovieManager::GetRenderDir() const
{
	return Replay_va( "%s%s%c", g_pClientReplayContextInternal->GetBaseDir(), SUBDIR_RENDERED, CORRECT_PATH_SEPARATOR );
}

const char *CReplayMovieManager::GetRawExportDir() const
{
	static CFmtStr s_fmtExportDir;

	CReplayTime time;
	time.InitDateAndTimeToNow();

	int nDay, nMonth, nYear;
	time.GetDate( nDay, nMonth, nYear );

	int nHour, nMin, nSec;
	time.GetTime( nHour, nMin, nSec );

	s_fmtExportDir.sprintf(
		"%smovie_%02i%02i%04i_%02i%02i%02i%c",
		GetRenderDir(),
		nMonth, nDay, nYear,
		nHour, nMin, nSec,
		CORRECT_PATH_SEPARATOR
	);

	return s_fmtExportDir.Access();
}

//----------------------------------------------------------------------------------------
