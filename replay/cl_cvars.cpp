//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "convar.h"
#include "replaysystem.h"
#include "replay/ireplayscreenshotsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

void OnReplayScreenshotResolutionChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConVar *pCvar = static_cast<ConVar*>(var);

	if ( pCvar->GetInt() != (int)flOldValue )
	{
		// Re-allocate screenshot memory in the client since we are will be taking screenshots
		// of a different dimension.
		if ( g_pClient )
		{
			g_pClient->GetReplayScreenshotSystem()->UpdateReplayScreenshotCache();
		}
	}
}

//----------------------------------------------------------------------------------------

ConVar replay_postdeathrecordtime( "replay_postdeathrecordtime", "5", FCVAR_DONTRECORD, "The amount of time (seconds) to be recorded after you die for a given replay.", true, 0.0f, true, 10.0f );
ConVar replay_postwinreminderduration( "replay_postwinreminderduration", "5", FCVAR_DONTRECORD, "The number of seconds to show a Replay reminder, post-win/lose.", true, 0.0f, false, 0.0f );
ConVar replay_sessioninfo_updatefrequency( "replay_sessioninfo_updatefrequency", "5", FCVAR_DONTRECORD, "If a replay has not been downloaded, the replay browser will update the status of a given replay on the server based on this cvar (in seconds).", true, 5.0f, true, 120.0f );
ConVar replay_enableeventbasedscreenshots( "replay_enableeventbasedscreenshots", "0", FCVAR_DONTRECORD | FCVAR_ARCHIVE, "If disabled, only take a screenshot when a replay is saved.  If enabled, take up to replay_maxscreenshotsperreplay screenshots, with a minimum of replay_mintimebetweenscreenshots seconds in between, at key events.  Events include kills, ubers (if you are a medic), sentry kills (if you are an engineer), etc.  NOTE: Turning this on may affect performance!" );
ConVar replay_screenshotresolution( "replay_screenshotresolution", "0", FCVAR_DONTRECORD, "0 for low-res screenshots (width=512), 1 for hi-res (width=1024)", true, 0.0f, true, 1.0f, OnReplayScreenshotResolutionChanged );
ConVar replay_maxscreenshotsperreplay( "replay_maxscreenshotsperreplay", "8", FCVAR_DONTRECORD, "The maximum number of screenshots that can be taken for any given replay.", true, 8, false, 0 );
ConVar replay_mintimebetweenscreenshots( "replay_mintimebetweenscreenshots", "5", FCVAR_DONTRECORD, "The minimum time (in seconds) that must pass between screenshots being taken.", true, 1, false, 0 );
ConVar replay_screenshotkilldelay( "replay_screenshotkilldelay", ".4", FCVAR_DONTRECORD, "Delay before taking a screenshot when you kill someone, in seconds.", true, 0.0f, true, 1.0f );
ConVar replay_screenshotsentrykilldelay( "replay_screenshotsentrykilldelay", ".30", FCVAR_DONTRECORD, "Delay before taking a screenshot when you kill someone, in seconds.", true, 0.0f, true, 1.0f );
ConVar replay_deathcammaxverticaloffset( "replay_deathcammaxverticaloffset", "150", FCVAR_DONTRECORD, "Vertical offset for player death camera" );
ConVar replay_sentrycammaxverticaloffset( "replay_sentrycammaxverticaloffset", "10", FCVAR_DONTRECORD, "Vertical offset from a sentry on sentry kill", true, 10.0f, false, 0.0f );
ConVar replay_playerdeathscreenshotdelay( "replay_playerdeathscreenshotdelay", "2", FCVAR_DONTRECORD, "Amount of time to wait after player is killed before taking a screenshot" );
ConVar replay_sentrycamoffset_frontback( "replay_sentrycamoffset_frontback", "-50", FCVAR_DONTRECORD, "Front/back offset for sentry POV screenshot" );
ConVar replay_sentrycamoffset_leftright( "replay_sentrycamoffset_leftright", "-25", FCVAR_DONTRECORD, "Left/right offset for sentry POV screenshot" );
ConVar replay_sentrycamoffset_updown( "replay_sentrycamoffset_updown", "22", FCVAR_DONTRECORD, "Up/down offset for sentry POV screenshot" );
ConVar replay_maxconcurrentdownloads( "replay_maxconcurrentdownloads", "3", FCVAR_DONTRECORD, "The maximum number of concurrent downloads allowed.", true, 1.0f, true, 16.0f );
ConVar replay_forcereconstruct( "replay_forcereconstruct", "0", FCVAR_DONTRECORD, "Force the reconstruction of replays each time." );

#if _DEBUG
ConVar replay_simulate_size_discrepancy( "replay_simulate_size_discrepancy", "0", FCVAR_DONTRECORD, "(Client-side) Simulate a downloaded session info saying a block should be X bytes and the downloaded data being Y bytes" );
ConVar replay_simulate_bad_hash( "replay_simulate_bad_hash", "0", FCVAR_DONTRECORD, "(Client-side) Simulate a downloaded session info specifying an MD5 digest that is different than what is calculated for a downloaded block on the client" );
ConVar replay_simulate_evil_download_size( "replay_simulate_evil_download_size", "0", FCVAR_DONTRECORD, "(Client-side) Simulate a maliciously large block or session info file size." );
ConVar replay_fake_render( "replay_fake_render", "0", FCVAR_DONTRECORD, "(Client-side) For fast render simulation - don't actually render any frames." );
ConVar replay_simulatedownloadfailure( "replay_simulatedownloadfailure", "0", FCVAR_DONTRECORD, "(Client-side) Simulate download failures. 0 = no failures; 1 = all HTTP downloads; 2 = session info files; 3 = session blocks" );
#endif

ConVar replay_dodiskcleanup( "replay_dodiskcleanup", "1", FCVAR_HIDDEN | FCVAR_ARCHIVE, "If 1, cleanup unneeded recording session blocks." );

ConVar replay_voice_during_playback( "replay_voice_during_playback", "0", FCVAR_ARCHIVE, "Play player voice chat during replay playback" );

//----------------------------------------------------------------------------------------