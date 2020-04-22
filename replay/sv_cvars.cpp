//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "convar.h"
#include "replay/shared_defs.h"
#include "sv_replaycontext.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

void OnFileserverHostnameChanged( IConVar *pVar, const char *pOldValue, float flOldValue )
{
	ConVarRef var( pVar );
	if ( !var.IsValid() )
		return;

	if ( g_pServerReplayContext )
	{
		g_pServerReplayContext->UpdateFileserverIPFromHostname( var.GetString() );
	}
	else
	{
		Warning ( "Cannot set ConVar %s yet. Replay is not initialized.", var.GetName() );
	}
}

void OnFileserverProxyHostnameChanged( IConVar *pVar, const char *pOldValue, float flOldValue )
{
	ConVarRef var( pVar );
	if ( !var.IsValid() )
		return;

	if ( g_pServerReplayContext )
	{
		g_pServerReplayContext->UpdateFileserverProxyIPFromHostname( var.GetString() );
	}
	else
	{
		Warning ( "Cannot set ConVar %s yet. Replay is not initialized.", var.GetName() );
	}
}

//----------------------------------------------------------------------------------------

ConVar replay_name( "replay_name", "Replay", FCVAR_GAMEDLL, "Replay bot name" );

ConVar replay_dofileserver_cleanup_on_start( "replay_dofileserver_cleanup_on_start", "1", FCVAR_GAMEDLL, "Cleanup any stale replay data (both locally and on fileserver) at startup." );

//
// FTP offloading
//
ConVar replay_fileserver_autocleanup( "replay_fileserver_autocleanup", "0", FCVAR_GAMEDLL, "Automatically do fileserver cleanup in between rounds?  This is the same as explicitly calling replay_docleanup." );
ConVar replay_fileserver_offload_aborttime( "replay_fileserver_offload_aborttime", "60", FCVAR_GAMEDLL, "The time after which publishing will be aborted for a session block or session info file.", true, 30.0f, true, 60.0f );

//
// For URL construction
//
ConVar replay_fileserver_protocol( "replay_fileserver_protocol", "http", FCVAR_REPLICATED | FCVAR_DONTRECORD, "Can be \"http\" or \"https\"" );
ConVar replay_fileserver_host( "replay_fileserver_host", "", FCVAR_REPLICATED | FCVAR_DONTRECORD, "The hostname of the Web server hosting replays.  This can be an IP or a hostname, e.g. \"1.2.3.4\" or \"www.myserver.com\"" );
ConVar replay_fileserver_port( "replay_fileserver_port", "80", FCVAR_REPLICATED | FCVAR_DONTRECORD, "The port for the Web server hosting replays.  For example, if your replays are stored at \"http://123.123.123.123:4567/tf/replays\", replay_fileserver_port should be 4567." );
ConVar replay_fileserver_path( "replay_fileserver_path", "", FCVAR_REPLICATED | FCVAR_DONTRECORD, "If your replays are stored at \"http://123.123.123.123:4567/tf/replays\", replay_fileserver_path should be set to \"/tf/replays\"" );

ConVar replay_max_publish_threads( "replay_max_publish_threads", "4", FCVAR_GAMEDLL, "The max number of threads allowed for publishing replay data, e.g. FTP threads.", true, 4, true, 8 );
ConVar replay_block_dump_interval( "replay_block_dump_interval", "10", FCVAR_DONTRECORD, "The server will write partial replay files at this interval when recording.", true, MIN_SERVER_DUMP_INTERVAL, true, MAX_SERVER_DUMP_INTERVAL );

ConVar replay_data_lifespan( "replay_data_lifespan", "1", FCVAR_REPLICATED | FCVAR_DONTRECORD, "The number of days before replay data will be removed from the server.  Server operators can expect that any data written more than replay_data_lifespan days will be considered stale, and any subsequent execution of replay_docleanup (or automatic cleanup, which can be enabled with replay_fileserver_autocleanup) will remove that data.", true, 1, true, 30 );
ConVar replay_local_fileserver_path( "replay_local_fileserver_path", "", FCVAR_DONTRECORD, "The file server local path.  For example, \"c:\\MyWebServer\\htdocs\\replays\" or \"/MyWebServer/htdocs/replays\"." );

ConVar replay_buffersize( "replay_buffersize", "32", FCVAR_DONTRECORD, "Maximum size for the replay memory buffer.", true, 16, false, 0 );

ConVar replay_record_voice( "replay_record_voice", "1", FCVAR_GAMEDLL, "If enabled, voice data is recorded into the replay files." );

//----------------------------------------------------------------------------------------
