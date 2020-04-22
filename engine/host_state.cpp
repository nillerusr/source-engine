//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Runs the state machine for the host & server
//
// $NoKeywords: $
//=============================================================================//


#include "host_state.h"
#include "eiface.h"
#include "quakedef.h"
#include "server.h"
#include "sv_main.h"
#include "host_cmd.h"
#include "host.h"
#include "screen.h"
#include "icliententity.h"
#include "icliententitylist.h"
#include "client.h"
#include "host_jmp.h"
#include "tier0/vprof.h"
#include "tier0/icommandline.h"
#include "filesystem_engine.h"
#include "zone.h"
#include "iengine.h"
#include "snd_audio_source.h"
#include "sv_steamauth.h"
#ifndef SWDS
#include "vgui_baseui_interface.h"
#endif
#include "sv_plugin.h"
#include "cl_main.h"
#include "sv_steamauth.h"
#include "datacache/imdlcache.h"
#include "sys_dll.h"
#include "testscriptmgr.h"
#if defined( REPLAY_ENABLED )
#include "replay_internal.h"
#include "replayserver.h"
#endif
#include "GameEventManager.h"
#include "tier0/etwprof.h"

#include "ccs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern bool		g_bAbortServerSet;
#ifndef SWDS
extern ConVar	reload_materials;
#endif

typedef enum 
{
	HS_NEW_GAME = 0,
	HS_LOAD_GAME,
	HS_CHANGE_LEVEL_SP,
	HS_CHANGE_LEVEL_MP,
	HS_RUN,
	HS_GAME_SHUTDOWN,
	HS_SHUTDOWN,
	HS_RESTART,
} HOSTSTATES;

// a little class that manages the state machine for the host
class CHostState
{
public:
				CHostState();
	void		Init();
	void		FrameUpdate( float time );
	void		SetNextState( HOSTSTATES nextState );

	void		RunGameInit();

	void		SetState( HOSTSTATES newState, bool clearNext );
	void		GameShutdown();

	void		State_NewGame();
	void		State_LoadGame();
	void		State_ChangeLevelMP();
	void		State_ChangeLevelSP();
	void		State_Run( float time );
	void		State_GameShutdown();
	void		State_Shutdown();
	void		State_Restart();

	bool		IsGameShuttingDown();

	void		RememberLocation();
	void		OnClientConnected(); // Client fully connected
	void		OnClientDisconnected(); // Client disconnected

	HOSTSTATES	m_currentState;
	HOSTSTATES	m_nextState;
	Vector		m_vecLocation;
	QAngle		m_angLocation;
	char		m_levelName[256];
	char		m_landmarkName[256];
	char		m_saveName[256];
	float		m_flShortFrameTime;		// run a few one-tick frames to avoid large timesteps while loading assets

	bool		m_activeGame;
	bool		m_bRememberLocation;
	bool		m_bBackgroundLevel;
	bool		m_bWaitingForConnection;
};

static bool Host_ValidGame( void );
static CHostState	g_HostState;


//-----------------------------------------------------------------------------
// external API for manipulating the host state machine
//-----------------------------------------------------------------------------
void HostState_Init()
{
	g_HostState.Init();
}

void HostState_Frame( float time )
{
	g_HostState.FrameUpdate( time );
}

void HostState_RunGameInit()
{
	g_HostState.RunGameInit();
	g_ServerGlobalVariables.bMapLoadFailed = false;
}

//-----------------------------------------------------------------------------
// start a new game as soon as possible
//-----------------------------------------------------------------------------
void HostState_NewGame( char const *pMapName, bool remember_location, bool background )
{
	Q_strncpy( g_HostState.m_levelName, pMapName, sizeof( g_HostState.m_levelName ) );

	g_HostState.m_landmarkName[0] = 0;
	g_HostState.m_bRememberLocation = remember_location;
	g_HostState.m_bWaitingForConnection = true;
	g_HostState.m_bBackgroundLevel = background;
	if ( remember_location )
	{
		g_HostState.RememberLocation();
	}
	g_HostState.SetNextState( HS_NEW_GAME );
}

//-----------------------------------------------------------------------------
// load a new game as soon as possible
//-----------------------------------------------------------------------------
void HostState_LoadGame( char const *pSaveFileName, bool remember_location )
{
#ifndef SWDS
	// Make sure the freaking save file exists....
	if ( !saverestore->SaveFileExists( pSaveFileName ) )
	{
			Warning("Save file %s can't be found!\n", pSaveFileName );
			SCR_EndLoadingPlaque();
			return;
	}

	Q_strncpy( g_HostState.m_saveName, pSaveFileName, sizeof( g_HostState.m_saveName )  );

	// Tell the game .dll we are loading another game
	serverGameDLL->PreSaveGameLoaded( pSaveFileName, sv.IsActive() );

	g_HostState.m_bRememberLocation = remember_location;
	g_HostState.m_bBackgroundLevel = false;
	g_HostState.m_bWaitingForConnection = true;
	if ( remember_location )
	{
		g_HostState.RememberLocation();
	}

	g_HostState.SetNextState( HS_LOAD_GAME );
#endif
}

// change level (single player style - smooth transition)
void HostState_ChangeLevelSP( char const *pNewLevel, char const *pLandmarkName )
{
	Q_strncpy( g_HostState.m_levelName, pNewLevel, sizeof( g_HostState.m_levelName ) );
	Q_strncpy( g_HostState.m_landmarkName, pLandmarkName, sizeof( g_HostState.m_landmarkName ) );
	g_HostState.SetNextState( HS_CHANGE_LEVEL_SP );
}

// change level (multiplayer style - respawn all connected clients)
void HostState_ChangeLevelMP( char const *pNewLevel, char const *pLandmarkName )
{
	Steam3Server().NotifyOfLevelChange();

	Q_strncpy( g_HostState.m_levelName, pNewLevel, sizeof( g_HostState.m_levelName ) );
	Q_strncpy( g_HostState.m_landmarkName, pLandmarkName, sizeof( g_HostState.m_landmarkName ) );
	g_HostState.SetNextState( HS_CHANGE_LEVEL_MP );
}

// shutdown the game as soon as possible
void HostState_GameShutdown()
{
	Steam3Server().NotifyOfLevelChange();

	// This will get called during shutdown, ignore it.
	if ( g_HostState.m_currentState != HS_SHUTDOWN &&
		 g_HostState.m_currentState != HS_RESTART &&
		 g_HostState.m_currentState != HS_GAME_SHUTDOWN
		 )
	{
		g_HostState.SetNextState( HS_GAME_SHUTDOWN );
	}
}

// shutdown the engine/program as soon as possible
void HostState_Shutdown()
{
#if defined( REPLAY_ENABLED )
	// If we're recording the game, finalize the replay so players can download it.
	if ( sv.IsDedicated() && g_pReplay && g_pReplay->IsRecording() )
	{
		// Stop recording and publish all blocks/session info data synchronously.
		g_pReplay->SV_EndRecordingSession( true );
	}
#endif

	g_HostState.SetNextState( HS_SHUTDOWN );
}

//-----------------------------------------------------------------------------
// Purpose: Restart the engine
//-----------------------------------------------------------------------------
void HostState_Restart()
{
	g_HostState.SetNextState( HS_RESTART );
}

bool HostState_IsGameShuttingDown()
{
	return g_HostState.IsGameShuttingDown();
}

bool HostState_IsShuttingDown()
{
	return ( g_HostState.m_currentState == HS_SHUTDOWN ||
		g_HostState.m_currentState == HS_RESTART ||
			g_HostState.m_currentState == HS_GAME_SHUTDOWN );
}


void HostState_OnClientConnected()
{
	g_HostState.OnClientConnected();
}

void HostState_OnClientDisconnected()
{
	g_HostState.OnClientDisconnected();
}

void HostState_SetSpawnPoint(Vector &position, QAngle &angle)
{
	g_HostState.m_angLocation = angle;
	g_HostState.m_vecLocation = position;
	g_HostState.m_bRememberLocation = true;
}

//-----------------------------------------------------------------------------
static void WatchDogHandler()
{
	Host_Error( "WatchdogHandler called - server exiting.\n" );
}

//-----------------------------------------------------------------------------
// Class implementation
//-----------------------------------------------------------------------------

CHostState::CHostState()
{
}

void CHostState::Init()
{
	SetState( HS_RUN, true );
	m_currentState = HS_RUN;
	m_nextState = HS_RUN;
	m_activeGame = false;
	m_levelName[0] = 0;
	m_saveName[0] = 0;
	m_landmarkName[0] = 0;
	m_bRememberLocation = 0;
	m_bBackgroundLevel = false;
	m_vecLocation.Init();
	m_angLocation.Init();
	m_bWaitingForConnection = false;
	m_flShortFrameTime = 1.0;

	CCS_Init();

	Plat_SetWatchdogHandlerFunction( WatchDogHandler );
}

void CHostState::SetState( HOSTSTATES newState, bool clearNext )
{
	m_currentState = newState;
	if ( clearNext )
	{
		m_nextState = newState;
	}
}

void CHostState::SetNextState( HOSTSTATES next )
{
	Assert( m_currentState == HS_RUN );
	m_nextState = next;
}

void CHostState::RunGameInit()
{
	Assert( !m_activeGame );

	if ( serverGameDLL )
	{
		serverGameDLL->GameInit();
	}
	m_activeGame = true;
}

void CHostState::GameShutdown()
{
	if ( m_activeGame )
	{
		serverGameDLL->GameShutdown();
		m_activeGame = false;
	}
}


// These State_ functions execute that state's code right away
// The external API queues up state changes to happen when the state machine is processed.
void CHostState::State_NewGame()
{
	CETWScope timer( "CHostState::State_NewGame" );

	if ( Host_ValidGame() )
	{
		// Demand load game .dll if running with -nogamedll flag, etc.
		if ( !serverGameClients )
		{
			SV_InitGameDLL();
		}

		if ( !serverGameClients )
		{
			Warning( "Can't start game, no valid server.dll loaded\n" );
		}
		else
		{
			if ( Host_NewGame( m_levelName, false, m_bBackgroundLevel ) )
			{
				// succesfully started the new game
				SetState( HS_RUN, true );
				return;
			}
		}
	}

	SCR_EndLoadingPlaque();

	// new game failed
	GameShutdown();
	// run the server at the console
	SetState( HS_RUN, true );
}

void CHostState::State_LoadGame()
{
#ifndef SWDS
	HostState_RunGameInit();
	
	if ( saverestore->LoadGame( m_saveName ) )
	{
		// succesfully started the new game
        GetTestScriptMgr()->CheckPoint( "load_game" );
		SetState( HS_RUN, true );
		return;
	}
#endif

	SCR_EndLoadingPlaque();

	// load game failed
	GameShutdown();
	// run the server at the console
	SetState( HS_RUN, true );
	
	if ( IsX360() )
	{
		// On the 360 we need to return to the background map
		g_ServerGlobalVariables.bMapLoadFailed = true;
		Cbuf_Clear();
		Cbuf_AddText( "startupmenu force" );
		Cbuf_Execute();
	}
}


void CHostState::State_ChangeLevelMP()
{
	if ( Host_ValidGame() )
	{
		Steam3Server().NotifyOfLevelChange();

#ifndef SWDS
		// start progress bar immediately for multiplayer level transitions
		EngineVGui()->EnabledProgressBarForNextLoad();
#endif
		if ( Host_Changelevel( false, m_levelName, m_landmarkName ) )
		{
			SetState( HS_RUN, true );
			return;
		}
	}
	// fail
	ConMsg( "Unable to change level!\n" );
	SetState( HS_RUN, true );

	IGameEvent *event = g_GameEventManager.CreateEvent( "server_changelevel_failed" );
	if ( event )
	{
		event->SetString( "levelname", m_levelName );
		g_GameEventManager.FireEvent( event );
	}
}


void CHostState::State_ChangeLevelSP()
{
	if ( Host_ValidGame() )
	{
		Host_Changelevel( true, m_levelName, m_landmarkName );
		SetState( HS_RUN, true );
		return;
	}
	// fail
	ConMsg( "Unable to change level!\n" );
	SetState( HS_RUN, true );
}

static bool IsClientActive()
{
	if ( !sv.IsActive() )
		return cl.IsActive();

	for ( int i = 0; i < sv.GetClientCount(); i++ )
	{
		CGameClient *pClient = sv.Client( i );
		if ( pClient->IsActive() )
			return true;
	}
	return false;
}

static bool IsClientConnected()
{
	if ( !sv.IsActive() )
		return cl.IsConnected();

	for ( int i = 0; i < sv.GetClientCount(); i++ )
	{
		CGameClient *pClient = sv.Client( i );
		if ( pClient->IsConnected() )
			return true;
	}
	return false;
}

void CHostState::State_Run( float frameTime )
{
	static bool s_bFirstRunFrame = true;

	if ( m_flShortFrameTime > 0 )
	{
		if ( IsClientActive() )
		{
			m_flShortFrameTime = (m_flShortFrameTime > frameTime) ? (m_flShortFrameTime-frameTime) : 0;
		}
		// Only clamp time if client is in process of connecting or is already connected.
		if ( IsClientConnected() )
		{
			frameTime = min( frameTime, host_state.interval_per_tick );
		}
	}

	// Nice big timeout to play it safe while still ensuring that we don't get stuck in
	// infinite loops.
	int nTimerWaitSeconds = 60;
	if ( s_bFirstRunFrame ) 
	{
		// The first frame can take a while especially during fork startup.
		s_bFirstRunFrame = false;
		nTimerWaitSeconds *= 2;
	}
	if ( sv.IsDedicated() )
	{
		Plat_BeginWatchdogTimer( nTimerWaitSeconds );
	}

	Host_RunFrame( frameTime );

	if ( sv.IsDedicated() )
	{
		Plat_EndWatchdogTimer();
	}

	switch( m_nextState )
	{
	case HS_RUN:
		break;

	case HS_LOAD_GAME:
	case HS_NEW_GAME:
#if !defined( SWDS )
		SCR_BeginLoadingPlaque();
#endif
		// FALL THROUGH INTENTIONALLY TO SHUTDOWN

	case HS_SHUTDOWN:
	case HS_RESTART:
		// NOTE: The game must be shutdown before a new game can start, 
		// before a game can load, and before the system can be shutdown.
		// This is done here instead of pathfinding through a state transition graph.
		// That would be equivalent as the only way to get from HS_RUN to HS_LOAD_GAME is through HS_GAME_SHUTDOWN.
	case HS_GAME_SHUTDOWN:
		SetState( HS_GAME_SHUTDOWN, false );
		break;

	case HS_CHANGE_LEVEL_MP:
	case HS_CHANGE_LEVEL_SP:
		SetState( m_nextState, true );
		break;

	default:
		SetState( HS_RUN, true );
		break;
	}
}

void CHostState::State_GameShutdown()
{
	if ( serverGameDLL )
	{
		Steam3Server().NotifyOfLevelChange();
		g_pServerPluginHandler->LevelShutdown();
#if !defined(SWDS)
		audiosourcecache->LevelShutdown();
#endif
	}

	GameShutdown();
#ifndef SWDS
	saverestore->ClearSaveDir();
#endif
	Host_ShutdownServer();

	switch( m_nextState )
	{
	case HS_LOAD_GAME:
	case HS_NEW_GAME:
	case HS_SHUTDOWN:
	case HS_RESTART:
		SetState( m_nextState, true );
		break;
	default:
		SetState( HS_RUN, true );
		break;
	}
}


// Tell the launcher we're done.
void CHostState::State_Shutdown()
{
	CCS_Shutdown();

#if !defined(SWDS)
	CL_EndMovie();
#endif

	eng->SetNextState( IEngine::DLL_CLOSE );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHostState::State_Restart( void )
{
	// Just like a regular shutdown
	State_Shutdown();

	// But signal launcher/front end to restart engine
	eng->SetNextState( IEngine::DLL_RESTART );
}


//-----------------------------------------------------------------------------
// this is the state machine's main processing loop
//-----------------------------------------------------------------------------
void CHostState::FrameUpdate( float time )
{
	CCS_Tick( time );

#if _DEBUG
	int loopCount = 0;
#endif

	if ( setjmp (host_abortserver) )
	{
		Init();
		return;
	}

	g_bAbortServerSet = true;

	while ( true )
	{
		int oldState = m_currentState;

		// execute the current state (and transition to the next state if not in HS_RUN)
		switch( m_currentState )
		{
		case HS_NEW_GAME:
			g_pMDLCache->BeginMapLoad();
			State_NewGame();
			break;
		case HS_LOAD_GAME:
			g_pMDLCache->BeginMapLoad();
			State_LoadGame();
			break;
		case HS_CHANGE_LEVEL_MP:
			g_pMDLCache->BeginMapLoad();
			m_flShortFrameTime = 0.5f;
			State_ChangeLevelMP();
			break;
		case HS_CHANGE_LEVEL_SP:
			g_pMDLCache->BeginMapLoad();
			m_flShortFrameTime = 1.5f; // 1.5s of slower frames
			State_ChangeLevelSP();
			break;
		case HS_RUN:
			State_Run( time );
			break;
		case HS_GAME_SHUTDOWN:
			State_GameShutdown();
			break;
		case HS_SHUTDOWN:
			State_Shutdown();
			break;
		case HS_RESTART:
			g_pMDLCache->BeginMapLoad();
			State_Restart();
			break;
		}

		// only do a single pass at HS_RUN per frame.  All other states loop until they reach HS_RUN 
		if ( oldState == HS_RUN )
			break;

		// shutting down
		if ( oldState == HS_SHUTDOWN ||
			 oldState == HS_RESTART )
			break;

		// Only HS_RUN is allowed to persist across loops!!!
		// Also, detect circular state graphs (more than 8 cycling changes is probably a loop)
		// NOTE: In the current graph, there are at most 2.
#if _DEBUG
		if ( (m_currentState == oldState) || (++loopCount > 8) )
		{
			Host_Error( "state crash!\n" );
		}
#endif
	}
}


bool CHostState::IsGameShuttingDown( void )
{
	return ( ( m_currentState == HS_GAME_SHUTDOWN ) || ( m_nextState == HS_GAME_SHUTDOWN ) );
}

void CHostState::RememberLocation()
{
#ifndef SWDS
	Assert( m_bRememberLocation );

	m_vecLocation = MainViewOrigin();
	VectorAngles( MainViewForward(), m_angLocation );

	IClientEntity *localPlayer = entitylist ? entitylist->GetClientEntity( cl.m_nPlayerSlot + 1 ) : NULL;
	if ( localPlayer )
	{
		m_vecLocation = localPlayer->GetAbsOrigin();
	}

	m_vecLocation.z -= 64.0f; // subtract out a bit of Z to position the player lower
#endif
}

void CHostState::OnClientConnected()
{
	if ( !m_bWaitingForConnection )
		return;
	m_bWaitingForConnection = false;

	if ( m_bRememberLocation )
	{
		m_bRememberLocation = false;
		Cbuf_AddText( va( "setpos_exact %f %f %f\n", m_vecLocation.x, m_vecLocation.y, m_vecLocation.z ) ); 
		Cbuf_AddText( va( "setang_exact %f %f %f\n", m_angLocation.x, m_angLocation.y, m_angLocation.z ) );
	}

#if !defined( SWDS )
	if ( reload_materials.GetBool() )
	{
		// building cubemaps requires the materials to reload after map
		reload_materials.SetValue( 0 );
		Cbuf_AddText( "mat_reloadallmaterials\n" );
	}

	// Spew global texture memory usage if asked to
	if( CommandLine()->CheckParm( "-dumpvidmemstats" ) )
	{
		FileHandle_t fp;
		fp = g_pFileSystem->Open( "vidmemstats.txt", "a" );

		g_pFileSystem->FPrintf( fp, "%s:\n", g_HostState.m_levelName );
	
#ifdef VPROF_ENABLED
		CVProfile *pProf = &g_VProfCurrentProfile;

		int prefixLen = strlen( "TexGroup_Global_" );
		float total = 0.0f;
		for ( int i=0; i < pProf->GetNumCounters(); i++ )
		{
			if ( pProf->GetCounterGroup( i ) == COUNTER_GROUP_TEXTURE_GLOBAL )
			{
				// The counters are in bytes and the panel is all in kilobytes.
				float value = pProf->GetCounterValue( i ) * ( 1.0f /  ( 1024.0f * 1024.0f ) );
				total += value;
				const char *pName = pProf->GetCounterName( i );
				if( Q_strnicmp( pName, "TexGroup_Global_", prefixLen ) == 0 )
				{
					pName += prefixLen;
				}
				g_pFileSystem->FPrintf( fp, "%s: %0.3fMB\n", pName, value );
			}
		}
		g_pFileSystem->FPrintf( fp, "vidmem total: %0.3fMB\n", total );
#endif

#if 0
		g_pFileSystem->FPrintf( fp, "hunk total: %0.3fMB\n", Cache_TotalUsed() * ( 1.0f / ( 1024.0f * 1024.0f ) ) );
		g_pFileSystem->FPrintf( fp, "hunk sound: %0.3fMB\n", Cache_TotalUsed_Sound() * ( 1.0f / ( 1024.0f * 1024.0f ) ) );
		g_pFileSystem->FPrintf( fp, "hunk models: %0.3fMB\n", Cache_TotalUsed_Models() * ( 1.0f / ( 1024.0f * 1024.0f ) ) );
#endif
		g_pFileSystem->FPrintf( fp, "---------------------------------\n" );
		g_pFileSystem->Close( fp );
		Cbuf_AddText( "quit\n" );
	}
#endif
}

void CHostState::OnClientDisconnected() 
{
}

// Determine if this is a valid game
static bool Host_ValidGame( void )
{
	// No multi-client single player games
	if ( sv.IsMultiplayer() )
	{
		if ( deathmatch.GetInt() )
			return true;
	}
	else
	{
		/*
		// Single player must have CD validation
		if ( IsValidCD() )
		{
			return true;
		}
		*/
		return true;
	}

	ConDMsg("Unable to launch game\n");
	return false;
}

