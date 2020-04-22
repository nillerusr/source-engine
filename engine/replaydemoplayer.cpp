//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#if defined( REPLAY_ENABLED )

#include "replaydemoplayer.h"
#include "replay/ireplaymoviemanager.h"
#include "replay/ireplayperformancemanager.h"
#include "replay/ireplaymovierenderer.h"
#include "replay/ireplayperformancecontroller.h"
#include "replay/ireplaymanager.h"
#include "replay/replay.h"
#include "replay/replayutils.h"
#include "replay/shared_defs.h"
#include "replay/iclientreplay.h"
#include "replay/performance.h"
#include "replay_internal.h"
#include "cmd.h"
#include "KeyValues.h"
#include "cdll_engine_int.h"
#include "host.h"
#include "fmtstr.h"
#include "vgui_baseui_interface.h"

#ifndef DEDICATED
#include "screen.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

CReplayDemoPlayer s_ReplayDemoPlayer;
IDemoPlayer *g_pReplayDemoPlayer = &s_ReplayDemoPlayer;

//----------------------------------------------------------------------------------------

ConVar replay_ignorereplayticks( "replay_ignorereplayticks", "0" );

//----------------------------------------------------------------------------------------

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CReplayDemoPlayer, IReplayDemoPlayer, INTERFACEVERSION_REPLAYDEMOPLAYER, s_ReplayDemoPlayer );

//----------------------------------------------------------------------------------------

CReplayDemoPlayer::CReplayDemoPlayer()
:	m_pMovie( NULL ),
	m_nCurReplayIndex( 0 ),
	m_flStartRenderTime( 0.0f ),
	m_bInStartPlayback( false ),
	m_bStopCommandEncountered( false ),
	m_bFullSignonStateReached( false )
{
}

void CReplayDemoPlayer::ClearReplayList()
{
	m_vecReplaysToPlay.PurgeAndDeleteElements();
}

void CReplayDemoPlayer::AddReplayToList( ReplayHandle_t hReplay, int iPerformance )
{
	// Make sure the replay handle's OK
	CReplay *pReplay = g_pReplayManager->GetReplay( hReplay );
	if ( !pReplay )
		return;

	// Create new info
	PlaybackInfo_t *pNewPlaybackInfo = new PlaybackInfo_t();

	// Cache handle & performance
	pNewPlaybackInfo->m_hReplay = hReplay;
	pNewPlaybackInfo->m_iPerformance = iPerformance;

	// Figure out replay spawn/spawn+length ticks
	pNewPlaybackInfo->m_nStartTick = pReplay->m_nSpawnTick;
	pNewPlaybackInfo->m_nEndTick = -1;

	const int nLengthInTicks = TIME_TO_TICKS( pReplay->m_flLength );
	if ( nLengthInTicks > 0 )
	{
		pNewPlaybackInfo->m_nEndTick = pReplay->m_nSpawnTick + nLengthInTicks;
	}

	// If a performance was specified, override ticks as appropriate
	if ( iPerformance >= 0 )
	{
		// Get the performance from the replay
		const CReplayPerformance *pPerformance = pReplay->GetPerformance( iPerformance );
		if ( pPerformance->m_nTickIn >= 0 )
		{
			pNewPlaybackInfo->m_nStartTick = pPerformance->m_nTickIn;
		}
		if ( pPerformance->m_nTickOut >= 0 )
		{
			pNewPlaybackInfo->m_nEndTick = pPerformance->m_nTickOut;
		}
	}

	// Cache
	m_vecReplaysToPlay.AddToTail( pNewPlaybackInfo );
}

CReplay *CReplayDemoPlayer::GetCurrentReplay()
{
	PlaybackInfo_t *pCurrentPlaybackInfo = GetCurrentPlaybackInfo();
	if ( !pCurrentPlaybackInfo )
		return NULL;

	return g_pReplayManager->GetReplay( pCurrentPlaybackInfo->m_hReplay );
}

const CReplay *CReplayDemoPlayer::GetCurrentReplay() const
{
	return const_cast< CReplayDemoPlayer * >( this )->GetCurrentReplay();
}

CReplayPerformance *CReplayDemoPlayer::GetCurrentPerformance()
{
	const PlaybackInfo_t *pCurrentPlaybackInfo = GetCurrentPlaybackInfo();
	if ( !pCurrentPlaybackInfo )
		return NULL;

	CReplay *pReplay = GetCurrentReplay();
	if ( !pReplay )
		return NULL;

	if ( pCurrentPlaybackInfo->m_iPerformance < 0 )
		return NULL;

	return pReplay->GetPerformance( pCurrentPlaybackInfo->m_iPerformance );
}

CReplayDemoPlayer::PlaybackInfo_t *CReplayDemoPlayer::GetCurrentPlaybackInfo()
{
	if ( m_vecReplaysToPlay.Count() == 0 )
		return NULL;

	return m_vecReplaysToPlay[ m_nCurReplayIndex ];
}

const CReplayDemoPlayer::PlaybackInfo_t *CReplayDemoPlayer::GetCurrentPlaybackInfo() const
{
	return const_cast< CReplayDemoPlayer * >( this )->GetCurrentPlaybackInfo();
}

void CReplayDemoPlayer::PauseReplay()
{
	PausePlayback( -1.0f );
}

bool CReplayDemoPlayer::IsReplayPaused()
{
	return IsPlaybackPaused();
}

void CReplayDemoPlayer::ResumeReplay()
{
	ResumePlayback();
}

void CReplayDemoPlayer::OnSignonStateFull()
{
	m_bFullSignonStateReached = true;
}

netpacket_t *CReplayDemoPlayer::ReadPacket( void )
{
	if ( !m_bStopCommandEncountered )
	{
		return BaseClass::ReadPacket();
	}

	return NULL;
}

float CReplayDemoPlayer::GetPlaybackTimeScale()
{
	if ( g_pReplayPerformanceController )
	{
		return g_pReplayPerformanceController->GetPlaybackTimeScale();
	}

	return 1.0f;
}

void CReplayDemoPlayer::OnStopCommand()
{
	if ( m_bStopCommandEncountered )
		return;

	m_bStopCommandEncountered = true;

	if ( !g_pClientReplay->OnEndOfReplayReached() )
	{
		BaseClass::OnStopCommand();
	}
}

bool CReplayDemoPlayer::StartPlayback( const char *pFilename, bool bAsTimeDemo )
{
	if ( !Replay_IsSupportedModAndPlatform() )
		return false;

	CInStartPlaybackGuard InStartPlaybackGuard( m_bInStartPlayback );

	const PlaybackInfo_t *pPlaybackInfo = GetCurrentPlaybackInfo();
	if ( !pPlaybackInfo )
		return false;

	// always display progress bar to ensure load screen background is redraw
	EngineVGui()->EnabledProgressBarForNextLoad();

	if ( !BaseClass::StartPlayback( pFilename, bAsTimeDemo ) )
	{
		DisplayFailedToPlayMsg( pPlaybackInfo->m_iPerformance );
		return false;
	}

	CReplay *pReplay = GetCurrentReplay();
	if ( !pReplay )
		return false;

	// Set this flag so we can detect whether the replay made it all the way to full signon state,
	// otherwise we'll display a message StopPlayback() is called.
	m_bFullSignonStateReached = false;

	// Reset so when we see a dem_stop we will handle it
	m_bStopCommandEncountered = false;

	// Reset skip to tick now
	m_nSkipToTick = -1;

	// Reset the rendering cancelled flag now
	g_pReplayMovieManager->ClearRenderCancelledFlag();

	// Setup playback timeframe
	if ( pPlaybackInfo->m_nStartTick >= 0 && !replay_ignorereplayticks.GetBool() )
	{
		SkipToTick( pPlaybackInfo->m_nStartTick, false, false );
	}
	if ( pPlaybackInfo->m_nEndTick >= 0 && !replay_ignorereplayticks.GetBool() )
	{
		SetEndTick( pPlaybackInfo->m_nEndTick );
	}

	if ( g_pReplayMovieManager->IsRendering() )
	{
#ifdef USE_WEBM_FOR_REPLAY
		const char *pExtension = ".webm";
#else
		const char *pExtension = ".mov";
#endif

		// Start recording the movie
		char szIdealFilename[ MAX_OSPATH ];
		V_FileBase( pFilename, szIdealFilename, sizeof( szIdealFilename ) );
		V_strcat( szIdealFilename, va( "_%i", pReplay->m_nSpawnTick ), sizeof( szIdealFilename ) );
		V_SetExtension( szIdealFilename, pExtension, sizeof( szIdealFilename ) );

		char szRenderPath[ MAX_OSPATH ];
		V_snprintf( szRenderPath, sizeof( szRenderPath ), "%s%c%s%c%s%c%s",
			com_gamedir, CORRECT_PATH_SEPARATOR, SUBDIR_REPLAY, CORRECT_PATH_SEPARATOR,
			SUBDIR_CLIENT, CORRECT_PATH_SEPARATOR, SUBDIR_RENDERED
		);

		char szActualFilename[ MAX_OSPATH ];
		Replay_GetFirstAvailableFilename( szActualFilename, sizeof( szActualFilename ), szIdealFilename,
			pExtension, szRenderPath, 0 );

		// Create an entry in the movie manager & save to disk
		m_pMovie = g_pReplayMovieManager->CreateAndAddMovie( pReplay->GetHandle() );
		m_pMovie->SetMovieFilename( szActualFilename );
		wchar_t wszMovieTitle[MAX_REPLAY_TITLE_LENGTH] = L"";
		if ( pPlaybackInfo->m_iPerformance < 0 )
		{
			g_pReplayMovieManager->GetCachedMovieTitleAndClear( wszMovieTitle, MAX_REPLAY_TITLE_LENGTH );
		}
		else
		{
			const CReplayPerformance *pPerformance = pReplay->GetPerformance( pPlaybackInfo->m_iPerformance );	AssertMsg( pPerformance, "Performance should always be valid!" );
			if ( pPerformance )
			{
				V_wcsncpy( wszMovieTitle, pPerformance->m_wszTitle, sizeof( wszMovieTitle ) );
			}
		}
		m_pMovie->SetMovieTitle( wszMovieTitle );
		g_pReplayMovieManager->SetPendingMovie( m_pMovie );
		g_pReplayMovieManager->FlagMovieForFlush( m_pMovie, true );

		// Setup the start render time
		m_flStartRenderTime = realtime;
	}
	else
	{
		m_pMovie = NULL;
	}

	return true;
}

void CReplayDemoPlayer::PlayNextReplay()
{
	const PlaybackInfo_t *pPlaybackInfo = GetCurrentPlaybackInfo();
	if ( !pPlaybackInfo )
		return;

	CReplay *pReplay = g_pReplayManager->GetReplay( pPlaybackInfo->m_hReplay );
	if ( !pReplay )
		return;

	Assert ( pReplay->m_nStatus != CReplay::REPLAYSTATUS_DOWNLOADPHASE );

	// Reconstruct now if necessary
	g_pClientReplayContext->ReconstructReplayIfNecessary( pReplay );

	// Open the demo file so we can read the start tick
	const char *pFilename = pReplay->m_strReconstructedFilename.Get();
	if ( !g_pFullFileSystem->FileExists( pFilename ) )
	{
		Warning( "\n**  File %s does not exist!\n\n", pFilename );
		DisplayFailedToPlayMsg( pPlaybackInfo->m_iPerformance );
		return;
	}

	// Construct a con-command to play the demo, starting at the spawn tick.
	// Play the replay using the 'playreplay' command - pass in the performance as well, -1
	// meaning play without a performance.
	const char *pCmd = "replay_hidebrowser\ngameui_hide\nprogress_enable\n";

	// Execute the command
	Cbuf_AddText( pCmd );
	Cbuf_Execute();

	// Use the replay demo player
	extern IDemoPlayer *g_pReplayDemoPlayer;
	demoplayer = g_pReplayDemoPlayer;

	// Open the demo file
	if ( demoplayer->StartPlayback( pFilename, false ) )
	{
		// Remove extension
		char szBasename[ MAX_OSPATH ];
		V_StripExtension( pFilename, szBasename, sizeof( szBasename ) );
		extern IBaseClientDLL *g_ClientDLL;
		g_ClientDLL->OnDemoPlaybackStart( szBasename );
	}
	else
	{
		SCR_EndLoadingPlaque();
	}
}

void CReplayDemoPlayer::PlayReplay( ReplayHandle_t hReplay, int iPerformance )
{
	// Cache the replay (this function will only ever cache one)
	s_ReplayDemoPlayer.ClearReplayList();
	s_ReplayDemoPlayer.AddReplayToList( hReplay, iPerformance );
	s_ReplayDemoPlayer.PlayNextReplay();
}

void CReplayDemoPlayer::OnLastDemoInLoopPlayed()
{
	g_pReplayMovieManager->CompleteRender( true, true );
}

float CReplayDemoPlayer::CalcMovieLength() const
{
	const PlaybackInfo_t *pPlaybackInfo = GetCurrentPlaybackInfo();
	if ( !pPlaybackInfo )
		return 0.0f;

	const CReplay *pReplay = GetCurrentReplay();
	if ( !pReplay )
		return 0.0f;

	const int nStartTick = pPlaybackInfo->m_nStartTick >= 0 ? pPlaybackInfo->m_nStartTick : pReplay->m_nSpawnTick;
	const int nEndTick = pPlaybackInfo->m_nEndTick >= 0 ? pPlaybackInfo->m_nEndTick : ( pReplay->m_nSpawnTick + TIME_TO_TICKS( pReplay->m_flLength ) );

	const bool bInvalidStartTick = nStartTick < 0;
	const bool bInvalidEndTick = nEndTick < 0;

	if ( bInvalidEndTick )
	{
		if ( !bInvalidStartTick )
		{
			// Valid start tick, invalid end tick
			return TICKS_TO_TIME( nStartTick ) + pReplay->m_flLength;
		}
	}
	else	// Valid end tick.
	{
		if ( !bInvalidStartTick )
		{
			// Valid start tick, valid end tick
			return TICKS_TO_TIME( nEndTick - nStartTick );
		}
	}

	// Failed to calculate length
	return 0.0f;
}

void CReplayDemoPlayer::StopPlayback()
{
	if ( !IsPlayingBack() )
		return;

	BaseClass::StopPlayback();

	if ( m_bInStartPlayback )
		return;

	bool bDoneWithBatch = m_nCurReplayIndex >= m_vecReplaysToPlay.Count() - 1;
	bool bRenderCancelled = g_pReplayMovieManager->RenderingCancelled();

	if ( g_pReplayMovieManager->IsRendering() )
	{
		// Update the replay's state
		CReplay *pReplay = GetCurrentReplay();
		if ( !pReplay )
			return;

		pReplay->m_bRendered = true;	// We have rendered this replay at least once

		// Save replay
		g_pReplayManager->FlagReplayForFlush( pReplay, false );

		// Update the movie's state - the render succeeded
		m_pMovie->SetIsRendered( true );
		
		// Compute the time it took to render
		m_pMovie->SetRenderTime( MAX( 0, realtime - m_flStartRenderTime ) );

		// Sets the recorded date & time of the movie
		m_pMovie->CaptureRecordTime();

		// Get movie length
		m_pMovie->SetLength( CalcMovieLength() );

		// Save movie
		g_pReplayMovieManager->FlagMovieForFlush( m_pMovie, true );

		// Kill the renderer, show the browser if we're done rendering all replays
		g_pReplayMovieManager->CompleteRender( true, bDoneWithBatch );
	}
	else if ( !bRenderCancelled )	// Without this check, batch rendering will continue to try and render after cancel
	{
		CReplay *pReplay = GetCurrentReplay();
		if ( !pReplay )
			return;

		// Get the 'saved' performance from the performance controller, since the performance we initiated playback
		// with may not be the one we want to select in the replay browser.  The user may have save as a new performance,
		// in which case we'll want to highlight that one.
		CReplayPerformance *pSavedPerformance = g_pReplayPerformanceController->GetSavedPerformance();

		// Get the index - FindPerformance() will set the output index to -1 if it can't find the performance
		int iHighlightPerformance;
		pReplay->FindPerformance( pSavedPerformance, iHighlightPerformance );

		// Notify UI that playback is complete
		g_pClientReplay->OnPlaybackComplete( pReplay->GetHandle(), iHighlightPerformance );

		// Hide the replay performance editor
		g_pClientReplay->HidePerformanceEditor();

		// End playback/recording as needed
		g_pReplayPerformanceController->Stop();
	}

	// Get the playback info before we incremeent the current replay
	const PlaybackInfo_t *pPlaybackInfo = GetCurrentPlaybackInfo();

	// Play the next replay, if one was queued
	++m_nCurReplayIndex;

	if ( !bDoneWithBatch && !bRenderCancelled )
	{
		g_pReplayMovieManager->RenderNextMovie();
	}
	else
	{
		m_nCurReplayIndex = 0;
		m_vecReplaysToPlay.PurgeAndDeleteElements();

		if ( !m_bFullSignonStateReached )
		{
			DisplayFailedToPlayMsg( pPlaybackInfo ? pPlaybackInfo->m_iPerformance : -1 );
		}
	}
}

bool CReplayDemoPlayer::ShouldLoopDemos()
{
	return false;
}

void CReplayDemoPlayer::DisplayFailedToPlayMsg( int iPerformance )
{
	g_pClientReplay->DisplayReplayMessage(
		iPerformance < 0 ? "#Replay_Err_User_FailedToPlayReplay" : "#Replay_Err_User_FailedToPlayTake",
		false, true, NULL
	);
}

//----------------------------------------------------------------------------------------

#endif	// #if defined( REPLAY_ENABLED )
