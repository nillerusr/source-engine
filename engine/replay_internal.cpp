//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#include "replay_internal.h"
#include "replay/ireplaysystem.h"
#include "replayserver.h"
#include "vgui/ILocalize.h"
#include "server.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------------------

IReplaySystem *g_pReplay = NULL;

//----------------------------------------------------------------------------------------

static CSysModule				*gs_hReplayModule = NULL;

IServerReplayContext			*g_pServerReplayContext = NULL;
CreateInterfaceFn				g_fnReplayFactory = NULL;

#if !defined( DEDICATED )
IReplayManager					*g_pReplayManager = NULL;
IReplayMovieRenderer			*g_pReplayMovieRenderer = NULL;
IReplayPerformanceManager		*g_pReplayPerformanceManager = NULL;
IReplayPerformanceController	*g_pReplayPerformanceController = NULL;
IReplayMovieManager				*g_pReplayMovieManager = NULL;
#endif

//----------------------------------------------------------------------------------------

ConVar replay_debug( "replay_debug", "0", FCVAR_DONTRECORD );
	
//----------------------------------------------------------------------------------------

// If you modify this list, you will also need to add the following line to the game's
// client_*.vpc and server_*.vpc files:
//
//    $include "$SRCDIR\vpc_scripts\source_replay.vpc"
//
static const char *s_pSupportedReplayGames[] =
{
	"tf",
	"tf_beta",
};

//----------------------------------------------------------------------------------------

void ReplaySystem_Init( bool bDedicated )
{
#if defined( REPLAY_ENABLED )
	COM_TimestampedLog( "Loading replay.dll" );

	extern IFileSystem *g_pFileSystem;

	// Load the replay DLL
	const char *szDllName = "replay" DLL_EXT_STRING;
	gs_hReplayModule = g_pFileSystem->LoadModule( szDllName );
	g_fnReplayFactory = Sys_GetFactory( gs_hReplayModule );
	if ( !g_fnReplayFactory )
	{
		Error( "Could not load: %s\n", szDllName );
	}
	
	// Get a ptr to the IReplay instance
	g_pReplay = (IReplaySystem *)g_fnReplayFactory( REPLAY_INTERFACE_VERSION, NULL );
	if ( !g_pReplay )
	{
		Error( "Could not get IReplay interface %s from %s\n", REPLAY_INTERFACE_VERSION, szDllName );
	}

	// Initialize the replay DLL
	COM_TimestampedLog( "g_pReplay->Initialize()" );

	extern CreateInterfaceFn g_AppSystemFactory;
	if ( !g_pReplay->Connect( g_AppSystemFactory ) )
	{
		Error( "Connect on replay.dll failed!\n" );
	}

	// Cache some pointers
	g_pServerReplayContext = g_pReplay->SV_GetContext();

	if ( !bDedicated )
	{
		// Add replay localization text files
		extern vgui::ILocalize *g_pVGuiLocalize;
		if ( g_pVGuiLocalize )
		{
			g_pVGuiLocalize->AddFile( "resource/replay_%language%.txt", "GAME" );
			g_pVGuiLocalize->AddFile( "resource/youtube_%language%.txt", "GAME" );
		}
	}
#endif
}

//----------------------------------------------------------------------------------------

void ReplaySystem_Shutdown()
{
#if defined( REPLAY_ENABLED )
	if ( g_pReplay )
	{
		// Shutdown the DLL
		g_pReplay->Shutdown();
		g_pReplay = NULL;
	}

	if ( gs_hReplayModule )
	{
		// Unload the DLL
		Sys_UnloadModule( gs_hReplayModule );
		gs_hReplayModule = NULL;
	}
#endif
}

//----------------------------------------------------------------------------------------

bool Replay_IsSupportedModAndPlatform()
{
	if ( !IsPC() )
		return false;

	int nNumSupportedReplayGames = ARRAYSIZE( s_pSupportedReplayGames );
	for ( int i = 0; i < nNumSupportedReplayGames; ++i )
	{
		const char *pCurGame = s_pSupportedReplayGames[ i ];
		if ( !Q_stricmp( COM_GetModDirectory(), pCurGame ) )
			return true;
	}
	return false;
}

//----------------------------------------------------------------------------------------
