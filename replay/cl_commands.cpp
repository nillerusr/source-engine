//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "cl_screenshotmanager.h"
#include "convar.h"
#include "replaysystem.h"
#include "netmessages.h"
#include "cl_replaymanager.h"
#include "cl_sessionblockdownloader.h"
#include "cl_recordingsession.h"
#include "cl_renderqueue.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

extern ConVar replay_postdeathrecordtime;

//----------------------------------------------------------------------------------------

CON_COMMAND( save_replay, "Save a replay of the current life if possible." )
{
	// Is the user running a listen server?
	if ( g_pEngineClient->IsListenServer() )
	{
		Replay_HudMsg( "#Replay_NoListenServer", "replay\\record_fail.wav", true );
		return;
	}

	// Replay enabled on the server?
	if ( !g_pReplay->IsReplayEnabled() )
	{
		Replay_HudMsg( "#Replay_NotEnabled", "replay\\record_fail.wav", true );
		return;
	}

	// Are we recording?
	if ( !g_pReplay->IsRecording() )
	{
		Replay_HudMsg( "#Replay_NotRecording", "replay\\record_fail.wav", true );
		return;
	}

	// Is replay disabled on the client?
	if ( g_pClientReplayContextInternal->IsClientSideReplayDisabled() )
	{
		Replay_HudMsg( "#Replay_ClientSideDisabled", NULL, true );
		return;
	}

	// Get the replay for the current life
	CReplay *pReplayForCurrentLife = CL_GetReplayManager()->m_pReplayThisLife;

	// Already saved this replay?
	if ( !pReplayForCurrentLife || pReplayForCurrentLife->m_bRequestedByUser || pReplayForCurrentLife->m_bSaved )
	{
		Replay_HudMsg( "#Replay_AlreadySaved", "replay\\record_fail.wav" );
		return;
	}

	// Take a screenshot and write it to disk if one hasn't been taken already
	if ( !pReplayForCurrentLife->GetScreenshotCount() )
	{
		CaptureScreenshotParams_t params;
		V_memset( &params, 0, sizeof( params ) );
		params.m_flDelay = 0.0f;
		params.m_bPrimary = true;
		CL_GetScreenshotManager()->CaptureScreenshot( params );
	}

	// Send a message to the server, regardless of whether the player is alive or dead, requesting
	// that a demo be written.  Format a file name with the client's steam id and a timestamp
	// (gpGlobals->tickcount).
	CLC_SaveReplay msgSaveReplay;
	g_pEngineClient->GetNetChannel()->SendNetMsg( msgSaveReplay, true );

	// Get the session
	CClientRecordingSession *pSession = CL_CastSession( CL_GetRecordingSessionManager()->Find( pReplayForCurrentLife->m_hSession ) );
	if ( !pSession )
	{
		AssertMsg( 0, "Replay points to a non-existent session - should never happen!" );
		CL_GetErrorSystem()->AddErrorFromTokenName( "#Replay_ReplayBadSession" );
		return;
	}

	// Replay for current life is complete (ie, player is dead and replay is ready to be committed)
	if ( pReplayForCurrentLife->m_bComplete )
	{
		CL_GetReplayManager()->CommitPendingReplayAndBeginDownload();
	}
	else
	{
		// Mark the replay as requested by the user, so we can commit automatically as soon as
		// the replay is complete (ie when the player dies, etc.).
		pReplayForCurrentLife->m_bRequestedByUser = true;
	}

	// Cache replay pointer in owning session
	pSession->CacheReplay( pReplayForCurrentLife );

	// Make sure downloading is enabled
	pSession->EnsureDownloadingEnabled();

	// Add the new entry to the replay browser
	g_pClient->OnSaveReplay( pReplayForCurrentLife->GetHandle(), true );
}

//----------------------------------------------------------------------------------------

CON_COMMAND( replay_add_fake_replays, "Adds a set of fake replays" )
{
	if ( args.ArgC() < 2 )
	{
		DevMsg( "Use \'replay_add_fake_replays\' <num fake replays to add> <today only>\n" );
		return;
	}

//	bool bTodayOnly = args.ArgC() >= 3 && args[2][0] == '1';
	for ( int i = 0; i < atoi(args[1]); ++i )
	{
		// TODO:
	}
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_confirmquit, "Make sure all replays are rendered before quitting", FCVAR_HIDDEN | FCVAR_DONTRECORD )
{
	// TODO: Check to see if any replays are downloading - warn user.  If user wants to
	// quit anyway, make sure to set any blocks to not downloaded, save, and delete any
	// files that were only partially downloaded.
	
	// Unrendered replays? Display the quit confirmation dialog with the option to render all and quit
	if ( CL_GetReplayManager()->GetUnrenderedReplayCount() > 0 && g_pClient->OnConfirmQuit() )
	{
		// Play a sound.
		g_pClient->PlaySound( "replay\\confirmquit.wav" );
	}
	else
	{
		g_pEngine->Cbuf_AddText( "quit" );
		g_pEngine->Cbuf_AddText( "\n" );
	}
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_deleteclientreplays, "Deletes all replays from client replay history, as well as all files associated with each replay.", FCVAR_DONTRECORD )
{
	CUtlVector< ReplayHandle_t > vecReplayHandles;
	FOR_EACH_REPLAY( i )
	{
		vecReplayHandles.AddToTail( GET_REPLAY_AT( i )->GetHandle() );
	}

	FOR_EACH_VEC( vecReplayHandles, i )
	{
		CL_GetReplayManager()->DeleteReplay( vecReplayHandles[ i ], true );
	}
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_removeclientreplay, "Remove the replay at the given index.", FCVAR_DONTRECORD )
{
	if ( args.ArgC() != 2 )
	{
		Msg( "Not enough parameters.\n" );
		return;
	}

	CL_GetReplayManager()->DeleteReplay( atoi(args[ 1 ]), true );
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_printclientreplays, "Prints out all client replay info", FCVAR_DONTRECORD )
{
	FOR_EACH_REPLAY( i )
	{
		const CReplay *pReplay = GET_REPLAY_AT( i );
		if ( !pReplay )
			continue;

		int nMonth, nDay, nYear;
		pReplay->m_RecordTime.GetDate( nDay, nMonth, nYear );

		int nHour, nMin, nSec;
		pReplay->m_RecordTime.GetTime( nHour, nMin, nSec );

		int nSpawnTick = pReplay->m_nSpawnTick;
		int nDeathTick = pReplay->m_nDeathTick;

		// TODO: All of this should go into a virtual function in CReplay, rather than some here and some in DumpGameSpecificData()
		char szTitle[MAX_REPLAY_TITLE_LENGTH];
		g_pVGuiLocalize->ConvertUnicodeToANSI( pReplay->m_wszTitle, szTitle, sizeof( szTitle ) );
		Msg( "replay %i: \"%s\"\n", i, szTitle );
		Msg( "  handle: %i\n", pReplay->GetHandle() );
		Msg( "  spawn/death tick: %i / %i\n", nSpawnTick, nDeathTick );
		Msg( "  date: %i/%i/%i\n", nMonth, nDay, nYear );
		Msg( "  time: %i:%i:%i\n", nHour, nMin, nSec );
		Msg( "  map: %s\n", pReplay->m_szMapName );

		CClientRecordingSession *pSession = CL_CastSession( CL_GetRecordingSessionManager()->FindSession( pReplay->m_hSession ) );
		const char *pSessionName = pSession ? pSession->m_strName.Get() : NULL;
		Msg( "  session name: %s\n", pSessionName ? pSessionName : "" );

		if ( pSession )
		{
			Msg( "     last block downloaded: %i\n", pSession->GetGreatestConsecutiveBlockDownloaded() );
			Msg( "     last block to download: %i\n", pSession->GetLastBlockToDownload() );
		}

		int nScreenshotCount = pReplay->GetScreenshotCount();
		Msg( "\n" );
		Msg( "  # screenshots: %i\n", nScreenshotCount );
		Msg( "  session handle: %i\n", (int)pReplay->m_hSession );

		for ( int i = 0; i < nScreenshotCount; ++i )
		{
			const CReplayScreenshot *pScreenshot = pReplay->GetScreenshot( i );
			Msg( "    screenshot %i:\n", i );
			Msg( "      dimensions: w=%i, h=%i\n", pScreenshot->m_nWidth, pScreenshot->m_nHeight );
			Msg( "      base filename: %s\n", pScreenshot->m_szBaseFilename );
		}

		int nPerfCount = pReplay->GetPerformanceCount();
		Msg( "\n" );
		Msg( "# performances: %i\n", nPerfCount );
		for ( int i = 0; i < nPerfCount; ++i )
		{
			const CReplayPerformance *pCurPerformance = pReplay->GetPerformance( i );
			g_pVGuiLocalize->ConvertUnicodeToANSI( pCurPerformance->m_wszTitle, szTitle, sizeof( szTitle ) );
			Msg( "    performance %i:\n", i );
			Msg( "      title: %s\n", szTitle );
			Msg( "      ticks: in=%i  out=%i\n", pCurPerformance->m_nTickIn, pCurPerformance->m_nTickOut );
			Msg( "      filename: %s\n", pCurPerformance->m_szBaseFilename );
		}
		Msg( "\n" );

		pReplay->DumpGameSpecificData();
		
		// Print replay status
		const char *pStatus;
		switch ( pReplay->m_nStatus )
		{
		case CReplay::REPLAYSTATUS_INVALID:	pStatus = "invalid"; break;
		case CReplay::REPLAYSTATUS_DOWNLOADPHASE:	pStatus = "download phase"; break;
		case CReplay::REPLAYSTATUS_READYTOCONVERT:	pStatus = "ready to convert"; break;
		case CReplay::REPLAYSTATUS_RENDERING:	pStatus = "rendering"; break;
		case CReplay::REPLAYSTATUS_RENDERED:	pStatus = "rendered"; break;
		case CReplay::REPLAYSTATUS_ERROR:	pStatus = "error"; break;
		default: pStatus = "";
		}
		Msg( "  status: %s\n\n\n", pStatus );
	}
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_renderpause, "Pause Replay rendering.", FCVAR_DONTRECORD )
{
	if ( !CL_GetMovieManager()->IsRendering() )
		return;

	if ( g_pReplayDemoPlayer->IsReplayPaused() )
	{
		Msg( "Replay rendering already paused.\n" );
		return;
	}

	// Pause playback
	g_pReplayDemoPlayer->PauseReplay();
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_renderunpause, "Unpause Replay rendering.", FCVAR_DONTRECORD )
{
	if ( !CL_GetMovieManager()->IsRendering() )
		return;

	if ( !g_pReplayDemoPlayer->IsReplayPaused() )
	{
		Msg( "Replay rendering not paused.\n" );
		return;
	}

	// Unpause
	g_pReplayDemoPlayer->ResumeReplay();
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_printqueuedtakes, "Print a list of takes queued for rendering.", FCVAR_DONTRECORD )
{
	const int nCount = CL_GetRenderQueue()->GetCount();
	if ( !nCount )
	{
		ConMsg( "No takes queued for render.\n" );
		return;
	}

	ConMsg( "Takes queued for render:\n" );
	ConMsg( "      %65s%65s\n", "Replay Name", "Take Name" );
	for ( int i = 0; i < nCount; ++i )
	{
		ReplayHandle_t hReplay;
		int iPerf;
		CL_GetRenderQueue()->GetEntryData( i, &hReplay, &iPerf );
		const CReplay *pReplay = CL_GetReplayManager()->GetReplay( hReplay );
		if ( !pReplay )
			continue;

		char szTakeName[MAX_REPLAY_TITLE_LENGTH];

		if ( iPerf == -1 )
		{
			V_strcpy( szTakeName, "original" );
		}
		else
		{
			const CReplayPerformance *pPerformance = pReplay->GetPerformance( iPerf );
			if ( !pPerformance )
				continue;
			V_wcstostr( pPerformance->m_wszTitle, -1, szTakeName, sizeof( szTakeName ) );
		}

		char szReplayTitle[MAX_REPLAY_TITLE_LENGTH];
		V_wcstostr( pReplay->m_wszTitle, -1, szReplayTitle, sizeof( szReplayTitle ) );

		ConMsg( "   %02i:%65s%65s\n", i, szReplayTitle, szTakeName );
	}
}

//----------------------------------------------------------------------------------------

CON_COMMAND_F( replay_clearqueuedtakes, "Clear takes from render queue.", FCVAR_DONTRECORD )
{
	CL_GetRenderQueue()->Clear();
	ConMsg( "Cleared.\n" );
}

//----------------------------------------------------------------------------------------
