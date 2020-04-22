//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "tier0/fasttimer.h"

#ifdef _WIN32
#include "tier0/memdbgon.h" // needed because in release builds crtdbg.h is handled specially if USE_MEM_DEBUG is defined
#include "tier0/memdbgoff.h"
#include <crtdbg.h>   // For getting at current heap size
#endif

#include "tier1/fmtstr.h"
#include "vstdlib/jobthread.h"

#ifdef USE_SDL
	#include "appframework/ilaunchermgr.h"
	extern ILauncherMgr *g_pLauncherMgr;
#endif

#include "server.h"
#include "host_jmp.h"
#include "screen.h"
#include "keys.h"
#include "cdll_int.h"
#include "eiface.h"
#include "sv_main.h"
#include "sv_log.h"
#include "shadowmgr.h"
#include "zone.h"
#include "gl_cvars.h"
#include "sv_filter.h"
#include "ivideomode.h"
#include "vprof_engine.h"
#include "iengine.h"
#include "tier2/tier2.h"
#include "enginethreads.h"
#include "steam/steam_api.h"
#include "LoadScreenUpdate.h"
#include "datacache/idatacache.h"

#if !defined SWDS
#include "voice.h"
#include "sound.h"
#endif

#include "icvar.h"

#include "sys.h"
#include "client.h"
#include "cl_pred.h"
#include "console.h"
#include "view.h"
#include "host.h"
#include "decal.h"
#include "gl_matsysiface.h"
#include "gl_shader.h"
#include "sys_dll.h"
#include "cmodel_engine.h"
#ifndef SWDS
#include "con_nprint.h"
#endif
#include "filesystem.h"
#include "filesystem_engine.h"
#include "tier0/etwprof.h"
#include "tier0/vcrmode.h"
#include "traceinit.h"
#include "host_saverestore.h"
#include "l_studio.h"
#include "cl_demo.h"
#include "cdll_engine_int.h"
#include "host_cmd.h"
#include "host_state.h"
#include "dt_instrumentation.h"
#include "dt_instrumentation_server.h"
#include "const.h"
#include "bitbuf_errorhandler.h"
#include "soundflags.h"
#include "enginestats.h"
#include "tier1/strtools.h"
#include "testscriptmgr.h"
#include "tmessage.h"
#include "tier0/vprof.h"
#include "tier0/icommandline.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "MapReslistGenerator.h"
#include "DownloadListGenerator.h"
#include "download.h"
#include "staticpropmgr.h"
#include "GameEventManager.h"
#include "iprediction.h"
#include "netmessages.h"
#include "cl_main.h"
#include "hltvserver.h"
#include "hltvtest.h"
#if defined( REPLAY_ENABLED )
#include "replayserver.h"
#include "replay_internal.h"
#endif
#include "sys_mainwind.h"
#include "host_phonehome.h"
#ifndef SWDS
#include "vgui_baseui_interface.h"
#include "cl_steamauth.h"
#endif
#include "sv_remoteaccess.h" // NotifyDedicatedServerUI()
#include "snd_audio_source.h"
#include "sv_steamauth.h"
#include "MapReslistGenerator.h"
#include "DevShotGenerator.h"
#include "sv_plugin.h"
#include "toolframework/itoolframework.h"
#include "ienginetoolinternal.h"
#include "inputsystem/iinputsystem.h"
#include "vgui_askconnectpanel.h"
#include "cvar.h"
#include "saverestoretypes.h"
#include "filesystem/IQueuedLoader.h"
#include "soundservice.h"
#include "profile.h"
#include "steam/isteamremotestorage.h"
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#include "audio_pch.h"
#endif
#if defined( LINUX )
#include <locale.h>
#include "SDL.h"
#endif

#include "ixboxsystem.h"
extern IXboxSystem *g_pXboxSystem;

extern ConVar cl_cloud_settings;
extern ConVar cl_logofile;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
void CL_SetPagedPoolInfo();
extern char	*CM_EntityString( void );
extern ConVar host_map;
extern ConVar sv_cheats;

bool g_bDedicatedServerBenchmarkMode = false;
bool g_bAllowSecureServers = true;
bool g_bLowViolence = false;

// These counters are for debugging in dumps.  If these are non-zero it may indicate some kind of 
// heap problem caused by the setjmp/longjmp error handling
int g_HostServerAbortCount = 0;
int g_HostErrorCount = 0;
int g_HostEndDemo = 0;

char g_szDefaultLogoFileName[] = "materials/vgui/logos/spray.vtf";

int host_frameticks = 0;
int host_tickcount = 0;
int host_currentframetick = 0;

static const char g_pModuleExtension[] = DLL_EXT_STRING;

// Engine player info, no game related infos here
BEGIN_BYTESWAP_DATADESC( player_info_s )
	DEFINE_ARRAY( name, FIELD_CHARACTER, MAX_PLAYER_NAME_LENGTH ),
	DEFINE_FIELD( userID, FIELD_INTEGER ),
	DEFINE_ARRAY( guid, FIELD_CHARACTER, SIGNED_GUID_LEN + 1 ),
	DEFINE_FIELD( friendsID, FIELD_INTEGER ),
	DEFINE_ARRAY( friendsName, FIELD_CHARACTER, MAX_PLAYER_NAME_LENGTH ),
	DEFINE_FIELD( fakeplayer, FIELD_BOOLEAN ),
	DEFINE_FIELD( ishltv, FIELD_BOOLEAN ),
#if defined( REPLAY_ENABLED )
	DEFINE_FIELD( isreplay, FIELD_BOOLEAN ),
#endif
	DEFINE_ARRAY( customFiles, FIELD_INTEGER, MAX_CUSTOM_FILES ),
	DEFINE_FIELD( filesDownloaded, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

//------------------------------------------
enum
{
	FRAME_SEGMENT_INPUT = 0,
	FRAME_SEGMENT_CLIENT,
	FRAME_SEGMENT_SERVER,
	FRAME_SEGMENT_RENDER,
	FRAME_SEGMENT_SOUND,
	FRAME_SEGMENT_CLDLL,
	FRAME_SEGMENT_CMD_EXECUTE,

	NUM_FRAME_SEGMENTS,
};

class CFrameTimer
{
public:
	void ResetDeltas();

	CFrameTimer() : swaptime(0)
	{
		ResetDeltas();
	}

	void MarkFrame();
	void StartFrameSegment( int i )
	{
		starttime[i] = Sys_FloatTime();
	}

	void EndFrameSegment( int i )
	{
		double dt = Sys_FloatTime() - starttime[i];
		deltas[ i ] += dt;
	}
	void MarkSwapTime( )
	{
		double newswaptime = Sys_FloatTime();
		frametime = newswaptime - swaptime;
		swaptime = newswaptime;

		ComputeFrameVariability();
		g_EngineStats.SetFrameTime( frametime );
		g_EngineStats.SetFPSVariability( m_flFPSVariability );

		host_frametime_stddeviation = m_flFPSStdDeviationSeconds;
	}

private:
	enum
	{
		FRAME_HISTORY_COUNT = 50		
	};

	friend void Host_Speeds();
	void ComputeFrameVariability();

	double time_base;
	double times[9];
	double swaptime;
	double frametime;
	double m_flFPSVariability;
	double m_flFPSStdDeviationSeconds;
	double starttime[NUM_FRAME_SEGMENTS];
	double deltas[NUM_FRAME_SEGMENTS];

	float m_pFrameTimeHistory[FRAME_HISTORY_COUNT];
	int m_nFrameTimeHistoryIndex;
};


static CFrameTimer g_HostTimes;


//------------------------------------------


float host_time = 0.0;

static ConVar	violence_hblood( "violence_hblood","1", 0, "Draw human blood" );
static ConVar	violence_hgibs( "violence_hgibs","1", 0, "Show human gib entities" );
static ConVar	violence_ablood( "violence_ablood","1", 0, "Draw alien blood" );
static ConVar	violence_agibs( "violence_agibs","1", 0, "Show alien gib entities" );

// Marked as FCVAR_USERINFO so that the server can cull CC messages before networking them down to us!!!
ConVar closecaption( "closecaption", "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX | FCVAR_USERINFO, "Enable close captioning." );
extern ConVar sv_unlockedchapters;

void Snd_Restart_f()
{
#ifndef SWDS
	extern bool snd_firsttime;

	char szVoiceCodec[_MAX_PATH] = { 0 };
	int nVoiceSampleRate = Voice_ConfiguredSampleRate();

	{
		// This is not valid after voice shuts down
		const char *pPreviousCodec = Voice_ConfiguredCodec();
		if ( pPreviousCodec && *pPreviousCodec )
		{
			V_strncpy( szVoiceCodec, pPreviousCodec, sizeof( szVoiceCodec ) );
		}
	}

	S_Shutdown();
	snd_firsttime = true;
	cl.ClearSounds();
	S_Init();

	// Restart voice if it was running
	if ( szVoiceCodec[0] )
		Voice_Init( szVoiceCodec, nVoiceSampleRate );

	// Do this or else it won't have anything in the cache.
	if ( audiosourcecache && sv.GetMapName()[0] )
	{
		audiosourcecache->LevelInit( sv.GetMapName() );
	}

	// Flush soundscapes so they don't stop. We don't insert text in the buffer here because 
	// cl_soundscape_flush is normally cheat-protected.
	ConCommand *pCommand = (ConCommand*)dynamic_cast< const ConCommand* >( g_pCVar->FindCommand( "cl_soundscape_flush" ) );
	if ( pCommand )
	{
		char const *argv[ 1 ] = { "cl_soundscape_flush" };

		CCommand cmd( 1, argv );
		pCommand->Dispatch( cmd );
	}
#endif
}

static ConCommand snd_restart( "snd_restart", Snd_Restart_f, "Restart sound system." );

// In other C files.
void Shader_Shutdown( void );
void R_Shutdown( void );

bool g_bAbortServerSet = false;

#ifdef _WIN32
static bool s_bInitPME = false;
#endif

CON_COMMAND( mem_dump, "Dump memory stats to text file." )
{
	ConMsg("Writing memory stats to file memstats.txt\n");

#if defined( _MEMTEST )
	const char *pTest = sv.GetMapName();
	if ( !pTest || !pTest[0] )
	{
		// possibly at menu
		pTest = "unknown";
	}
	MemAlloc_SetStatsExtraInfo( pTest,"" );
#endif
	MemAlloc_DumpStats();
}

CON_COMMAND( mem_compact, "" )
{
	MemAlloc_CompactHeap();
}

CON_COMMAND( mem_eat, "" )
{
	MemAlloc_Alloc( 1024* 1024 );
}

CON_COMMAND( mem_test, "" )
{
	MemAlloc_CrtCheckMemory();
}

static ConVar host_competitive_ever_enabled( "host_competitive_ever_enabled", "0", FCVAR_HIDDEN, "Has competitive ever been enabled this run?", true, 0, true, 1, true, 1, false, 1, NULL  );

static ConVar mem_test_each_frame( "mem_test_each_frame", "0", 0, "Run heap check at end of every frame\n" );
static ConVar mem_test_every_n_seconds( "mem_test_every_n_seconds", "0", 0, "Run heap check at a specified interval\n" );

static ConVar	singlestep( "singlestep", "0", FCVAR_CHEAT, "Run engine in single step mode ( set next to 1 to advance a frame )" );
static ConVar	cvarNext( "next", "0", FCVAR_CHEAT, "Set to 1 to advance to next frame ( when singlestep == 1 )" );
// Print a debug message when the client or server cache is missed
ConVar host_showcachemiss( "host_showcachemiss", "0", 0, "Print a debug message when the client or server cache is missed." );
static ConVar mem_dumpstats( "mem_dumpstats", "0", 0, "Dump current and max heap usage info to console at end of frame ( set to 2 for continuous output )\n" );
static ConVar host_ShowIPCCallCount( "host_ShowIPCCallCount", "0", 0, "Print # of IPC calls this number of times per second. If set to -1, the # of IPC calls is shown every frame." );

#if defined( RAD_TELEMETRY_ENABLED )
static void OnChangeTelemetryPause ( IConVar *var, const char *pOldValue, float flOldValue )
{
	tmPause( TELEMETRY_LEVEL0, 1 );
}

static void OnChangeTelemetryResume ( IConVar *var, const char *pOldValue, float flOldValue )
{
	tmPause( TELEMETRY_LEVEL0, 0 );
}

static void OnChangeTelemetryLevel ( IConVar *var, const char *pOldValue, float flOldValue )
{
	char* pIEnd;
	const char *pLevel = (( ConVar* )var)->GetString();

	TelemetrySetLevel( strtoul( pLevel, &pIEnd, 0 ) );
}

static void OnChangeTelemetryFrameCount ( IConVar *var, const char *pOldValue, float flOldValue )
{
	char* pIEnd;
	const char *pFrameCount = (( ConVar* )var)->GetString();

	g_Telemetry.FrameCount = strtoul( pFrameCount, &pIEnd, 0 );
	Msg( " TELEMETRY: Setting Telemetry FrameCount: '%d'\n", g_Telemetry.FrameCount );
}

static void OnChangeTelemetryServer ( IConVar *var, const char *pOldValue, float flOldValue )
{
	const char *pServerAddress = (( ConVar* )var)->GetString();

	Q_strncpy( g_Telemetry.ServerAddress, pServerAddress, ARRAYSIZE( g_Telemetry.ServerAddress ) );
	Msg( " TELEMETRY: Setting Telemetry server: '%s'\n", pServerAddress );
}

static void OnChangeTelemetryDemoStart ( IConVar *var, const char *pOldValue, float flOldValue )
{
	char* pIEnd;
	const char *pVal = (( ConVar* )var)->GetString();

	g_Telemetry.DemoTickStart = strtoul( pVal, &pIEnd, 0 );
	if( g_Telemetry.DemoTickStart > 2000 )
	{
		char cmd[ 256 ]; 

		// If we're far away from the start of the demo file, then jump to ~1000 ticks before.
		Q_snprintf( cmd, sizeof( cmd ), "demo_gototick %d", g_Telemetry.DemoTickStart - 1000 ); 
		Cbuf_AddText( cmd ); 
	}
	Msg( " TELEMETRY: Setting Telemetry DemoTickStart: '%d'\n", g_Telemetry.DemoTickStart );
}

static void OnChangeTelemetryDemoEnd ( IConVar *var, const char *pOldValue, float flOldValue )
{
	char* pIEnd;
	const char *pVal = (( ConVar* )var)->GetString();

	g_Telemetry.DemoTickEnd = strtoul( pVal, &pIEnd, 0 );
	Msg( " TELEMETRY: Setting Telemetry DemoTickEnd: '%d'\n", g_Telemetry.DemoTickEnd );
}

ConVar telemetry_pause( "telemetry_pause", "0", 0, "Pause Telemetry", OnChangeTelemetryPause );
ConVar telemetry_resume( "telemetry_resume", "0", 0, "Resume Telemetry", OnChangeTelemetryResume );
ConVar telemetry_framecount( "telemetry_framecount", "0", 0, "Set Telemetry count of frames to capture", OnChangeTelemetryFrameCount );
ConVar telemetry_level( "telemetry_level", "0", 0, "Set Telemetry profile level: 0 being off.", OnChangeTelemetryLevel );
ConVar telemetry_server( "telemetry_server", "localhost", 0, "Set Telemetry server", OnChangeTelemetryServer );
ConVar telemetry_demostart( "telemetry_demostart", "0", 0, "When playing demo, start telemetry on tick #", OnChangeTelemetryDemoStart );
ConVar telemetry_demoend( "telemetry_demoend", "0", 0, "When playing demo, stop telemetry on tick #", OnChangeTelemetryDemoEnd );
#endif

extern bool gfBackground;

static bool host_checkheap = false;

CCommonHostState host_state;


//-----------------------------------------------------------------------------

enum HostThreadMode
{
	HTM_DISABLED,
	HTM_DEFAULT,
	HTM_FORCED,
};

ConVar host_thread_mode( "host_thread_mode", ( IsX360() ) ? "1" : "0", 0, "Run the host in threaded mode, (0 == off, 1 == if multicore, 2 == force)" );
extern ConVar threadpool_affinity;
void OnChangeThreadAffinity( IConVar *var, const char *pOldValue, float flOldValue )
{
	if ( g_pThreadPool->NumThreads() )
	{
		g_pThreadPool->Distribute( threadpool_affinity.GetBool() );
	}
}

ConVar threadpool_affinity( "threadpool_affinity", "1", 0, "Enable setting affinity", 0, 0, 0, 0, &OnChangeThreadAffinity );

#if 0
extern ConVar threadpool_reserve;
CThreadEvent g_ReleaseThreadReservation( true );
CInterlockedInt g_NumReservedThreads;

void ThreadPoolReserverFunction()
{
	g_ReleaseThreadReservation.Wait();
	--g_NumReservedThreads;
}

void ReserveThreads( int nToReserve )
{
	nToReserve = clamp( nToReserve, 0, g_pThreadPool->NumThreads() );
	g_ReleaseThreadReservation.Set();

	while ( g_NumReservedThreads != 0 )
	{
		ThreadSleep( 0 );
	}

	g_ReleaseThreadReservation.Reset();

	while ( nToReserve-- )
	{
		g_NumReservedThreads++;
		g_pThreadPool->QueueCall( &ThreadPoolReserverFunction )->Release();
	}

	Msg( "%d threads being reserved\n", (int)g_NumReservedThreads );
}

void OnChangeThreadReserve( IConVar *var, const char *pOldValue, float flOldValue )
{
	ReserveThreads( threadpool_reserve.GetInt() );
}

ConVar threadpool_reserve( "threadpool_reserve", "0", 0, "Consume the specified number of threads in the thread pool", 0, 0, 0, 0, &OnChangeThreadReserve );

CON_COMMAND( threadpool_cycle_reserve, "Cycles threadpool reservation by powers of 2" )
{
	int nCores = g_pThreadPool->NumThreads() + 1;
	int nAvailableCores = nCores - g_NumReservedThreads;
	Assert( nAvailableCores );
	int ratio = nCores / nAvailableCores;
	ratio *= 2;
	if ( ratio > nCores )
	{
		ReserveThreads( 0 );
	}
	else
	{
		ReserveThreads( nCores - nCores / ratio );
	}
}

CON_COMMAND( thread_test_tslist, "" )
{
	int nTests = ( args.ArgC() == 1 ) ? 10000 : atoi( args.Arg( 1 ) );
	RunTSListTests( nTests );
}

CON_COMMAND( thread_test_tsqueue, "" )
{
	int nTests = ( args.ArgC() == 1 ) ? 10000 : atoi( args.Arg( 1 ) );
	RunTSQueueTests( nTests );
}

CON_COMMAND( threadpool_run_tests, "" )
{
	int nTests = ( args.ArgC() == 1 ) ? 1 : atoi( args.Arg( 1 ) );
	for ( int i = 0; i < nTests; i++ )
	{
		RunThreadPoolTests();
	}
}
#endif

//-----------------------------------------------------------------------------

/*
A server can always be started, even if the system started out as a client
to a remote system.

A client can NOT be started if the system started as a dedicated server.
Memory is cleared / released when a server or client begins, not when they end.
*/


// Ear position + orientation
static AudioState_t s_AudioState;

engineparms_t host_parms;

bool		host_initialized = false;		// true if into command execution

float		host_frametime = 0.0f;
float		host_frametime_unbounded = 0.0f;
float		host_frametime_stddeviation = 0.0f;
double		realtime = 0;			// without any filtering or bounding
double		host_idealtime = 0;		// "ideal" server time assuming perfect tick rate
float		host_nexttick = 0;		// next server tick in this many ms
float		host_jitterhistory[128] = { 0 };
unsigned int host_jitterhistorypos = 0;

int			host_framecount;
static int	host_hunklevel;

CGameClient	*host_client;			// current client

jmp_buf 	host_abortserver;
jmp_buf     host_enddemo;

static ConVar	host_profile( "host_profile","0" );

ConVar	host_limitlocal( "host_limitlocal", "0", 0, "Apply cl_cmdrate and cl_updaterate to loopback connection" );
ConVar	host_framerate( "host_framerate","0", 0, "Set to lock per-frame time elapse." );
ConVar	host_timescale( "host_timescale","1.0", FCVAR_REPLICATED, "Prescale the clock by this amount." );
ConVar	host_speeds( "host_speeds","0", 0, "Show general system running times." );		// set for running times

ConVar	host_flush_threshold( "host_flush_threshold", "20", 0, "Memory threshold below which the host should flush caches between server instances" );

void HostTimerSpinMsChangedCallback( IConVar *var, const char *pOldString, float flOldValue );
ConVar host_timer_spin_ms( "host_timer_spin_ms", "0", FCVAR_NONE, "Use CPU busy-loop for improved timer precision (dedicated only)", HostTimerSpinMsChangedCallback );

void HostTimerSpinMsChangedCallback( IConVar *var, const char *pOldString, float flOldValue )
{
	const char *pForcedValue = CommandLine()->ParmValue( "+host_timer_spin_ms" );
	if ( pForcedValue != NULL )
	{
		if ( V_strcmp( host_timer_spin_ms.GetString(), pForcedValue ) )
		{
			Msg( "Value for host_timer_spin_ms is locked to %s by command line parameter.\n", pForcedValue );
			host_timer_spin_ms.SetValue( pForcedValue );
		}
	}
}

CON_COMMAND( host_timer_report, "Spew CPU timer jitter for the last 128 frames in microseconds (dedicated only)" )
{
	if ( sv.IsDedicated() )
	{
		for (int i = 1; i <= ARRAYSIZE( host_jitterhistory ); ++i)
		{
			unsigned int slot = ( i + host_jitterhistorypos ) % ARRAYSIZE( host_jitterhistory );
			Msg( "%1.3fms\n", host_jitterhistory[ slot ] * 1000 );
		}
	}
}

#ifdef REL_TO_STAGING_MERGE_TODO							 
// Do this when merging the game DLLs so FCVAR_CHEAT can be set on them at the same time.
ConVar  developer( "developer", "0", FCVAR_CHEAT, "Set developer message level");
#else
ConVar  developer( "developer", "0", 0, "Set developer message level");
#endif

ConVar	skill( "skill","1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX, "Game skill level (1-3).", true, 1, true, 3 );			// 1 - 3
ConVar	deathmatch( "deathmatch","0", FCVAR_NOTIFY | FCVAR_INTERNAL_USE, "Running a deathmatch server." );	// 0, 1, or 2
ConVar	coop( "coop","0", FCVAR_NOTIFY, "Cooperative play." );			// 0 or 1

#ifdef _DEBUG
ConVar	r_ForceRestore( "r_ForceRestore", "0", 0 );
#endif // _DEBUG

ConVar vcr_verbose( "vcr_verbose", "0", 0, "Write extra information into .vcr file." );

#ifndef SWDS
void CL_CheckToDisplayStartupMenus(); // in cl_main.cpp
#endif


bool GetFileFromRemoteStorage( ISteamRemoteStorage *pRemoteStorage, const char *pszRemoteFileName, const char *pszLocalFileName )
{
	bool bSuccess = false;

	// check if file exists in Steam Cloud first
	int32 nFileSize = pRemoteStorage->GetFileSize( pszRemoteFileName );

	if ( nFileSize > 0 )
	{
		CUtlMemory<char> buf( 0, nFileSize );
		if ( pRemoteStorage->FileRead( pszRemoteFileName, buf.Base(), nFileSize ) == nFileSize )
		{

			char filepath[ 512 ];
			Q_strncpy( filepath, pszLocalFileName, sizeof( filepath ) );
			Q_StripFilename( filepath );
			g_pFullFileSystem->CreateDirHierarchy( filepath, "MOD" );

			FileHandle_t hFile = g_pFileSystem->Open( pszLocalFileName, "wb", "MOD" );
			if( hFile )
			{

				bSuccess = g_pFileSystem->Write( buf.Base(), nFileSize, hFile ) == nFileSize;
				g_pFileSystem->Close( hFile );

				if ( bSuccess )
				{
					DevMsg( "[Cloud]: SUCCEESS retrieved %s from remote storage into %s\n", pszRemoteFileName, pszLocalFileName );
				}
				else
				{
					DevMsg( "[Cloud]: FAILED retrieved %s from remote storage into %s\n", pszRemoteFileName, pszLocalFileName );
				}
			}
		}
	}

	return bSuccess;
}


void CCommonHostState::SetWorldModel( model_t *pModel )
{
	worldmodel = pModel;
	if ( pModel )
	{
		worldbrush = pModel->brush.pShared;
	}
	else
	{
		worldbrush = NULL;
	}
}

void Host_DefaultMapFileName( const char *pFullMapName, /* out */ char *pDiskName, unsigned int nDiskNameSize )
{
	if ( IsPC() || !IsX360() )
	{
		// pc names are as is
		Q_snprintf( pDiskName, nDiskNameSize, "maps/%s.bsp", pFullMapName );
	}
	else if ( IsX360() )
	{
		Q_snprintf( pDiskName, nDiskNameSize, "maps/%s.360.bsp", pFullMapName );
	}
}

void Host_SetAudioState( const AudioState_t &audioState )
{
	memcpy( &s_AudioState, &audioState, sizeof(AudioState_t) );
}

bool Host_IsSinglePlayerGame( void )
{
	if ( sv.IsActive() )
	{
		return !sv.IsMultiplayer();
	}
	else
	{
		return cl.m_nMaxClients == 1;
	}
}

void CheckForFlushMemory( const char *pCurrentMapName, const char *pDestMapName )
{
	if ( host_flush_threshold.GetInt() == 0 )
		return;

#if defined(_X360)
	// There are three cases in which we flush memory
	//   Case 1: changing from one map to another
	//          -> flush temp data caches
	//   Case 2: loading any map (inc. A to A) and free memory is below host_flush_threshold MB
	//          -> flush everything
	//   Case 3: loading a 'blacklisted' map (the known biggest memory users, or where texture sets change)
	//          -> flush everything
	static const char *mapBlackList[] = 
	{
		// --hl2--
		"d1_canals_01",
		"d1_canals_05",
		"d1_eli_01",
		"d1_town_01",
		"d2_coast_01",
		"d2_prison_01",
		"d3_c17_01",
		"d3_c17_05",
		"d3_c17_09",
		"d3_citadel_01",
		"d3_breen_01",
		// --ep1--
		"ep1_c17_02",
		"ep1_c17_02b",
		"ep1_c17_05",
		"ep1_c17_06",
		// --ep2--
		"ep2_outland_06a",
		"ep2_outland_09",
		"ep2_outland_11",
		"ep2_outland_12",
		"ep2_outland_12a",
		// --tf--
		"tc_hydro"
	};

	char szCurrentMapName[MAX_PATH];
	char szDestMapName[MAX_PATH];
	if ( pCurrentMapName )
	{
		V_FileBase( pCurrentMapName, szCurrentMapName, sizeof( szCurrentMapName ) );
	}
	else
	{
		szCurrentMapName[0] = '\0';
	}
	pCurrentMapName = szCurrentMapName;

	if ( pDestMapName )
	{
		V_FileBase( pDestMapName, szDestMapName, sizeof( szDestMapName ) );
	}
	else
	{
		szDestMapName[0] = '\0';
	}
	pDestMapName = szDestMapName;

	bool bIsMapChanging = pCurrentMapName[0] && V_stricmp( pCurrentMapName, pDestMapName );

	bool bIsDestMapBlacklisted = false;
	for ( int i = 0; i < ARRAYSIZE( mapBlackList ); i++ )
	{
		if ( pDestMapName && !V_stricmp( pDestMapName, mapBlackList[i] ) )
		{
			bIsDestMapBlacklisted = true;
		}
	}

	DevMsg( "---CURRENT(%s), NEXT(%s)\n", (pCurrentMapName[0] ? pCurrentMapName : "----"), (pDestMapName[0] ? pDestMapName : "----") );
	if ( bIsMapChanging )
	{
		DevMsg( "---CHANGING MAPS!\n" );
	}
	if ( bIsDestMapBlacklisted )
	{
		DevMsg( "---BLACKLISTED!\n" );
	}

	MEMORYSTATUS stat;
	GlobalMemoryStatus( &stat );
	if ( ( stat.dwAvailPhys < host_flush_threshold.GetInt() * 1024 * 1024 ) ||
		 ( bIsDestMapBlacklisted && bIsMapChanging ) )
	{
		// Flush everything; ALL data is reloaded from scratch
		SV_FlushMemoryOnNextServer();
		g_pDataCache->Flush();
		DevWarning( "---FULL FLUSH\n" );
	}
	else if ( bIsMapChanging )
	{
		// Flush temporary data (async anim, non-locked async audio)
		g_pMDLCache->Flush( MDLCACHE_FLUSH_ANIMBLOCK );
		wavedatacache->Flush();
		DevWarning( "---PARTIAL FLUSH\n" );
	}
	DevMsg( "---- --- ----\n" );
#endif
}

void Host_AbortServer()
{
	g_HostServerAbortCount++;
	longjmp( host_abortserver, 1 );
}

/*
================
Host_EndGame
================
*/
void Host_EndGame (bool bShowMainMenu, const char *message, ...)
{
	int oldn;
	va_list		argptr;
	char		string[1024];

	va_start (argptr,message);
	Q_vsnprintf (string,sizeof(string),message,argptr);
	va_end (argptr);
	ConMsg ("Host_EndGame: %s\n",string);

#ifndef SWDS
	scr_disabled_for_loading = true;
#endif

	oldn = cl.demonum;
	cl.demonum = -1;

	Host_Disconnect(bShowMainMenu);

	cl.demonum = oldn;

	if ( sv.IsDedicated() )
	{
		Sys_Error ("Host_EndGame: %s\n",string);	// dedicated servers exit
		return;
	}

	if (cl.demonum != -1)
	{
#ifndef SWDS
		CL_NextDemo ();
#endif		
		g_HostEndDemo++;
		longjmp (host_enddemo, 1);
	}
	else
	{

#ifndef SWDS
		scr_disabled_for_loading = false;
#endif
		if ( g_bAbortServerSet )
		{
			Host_AbortServer();
		}
	}
}

/*
================
Host_Error

This shuts down both the client and server
================
*/
void Host_Error (const char *error, ...)
{
	va_list		argptr;
	char		string[1024];
	static	bool inerror = false;

	DebuggerBreakIfDebugging_StagingOnly();

	g_HostErrorCount++;

	if (inerror)
	{
		Sys_Error ("Host_Error: recursively entered");
	}
	inerror = true;

#ifndef SWDS
	//	CL_WriteMessageHistory();	TODO must be done by network layer
#endif

	va_start (argptr,error);
	Q_vsnprintf(string,sizeof(string),error,argptr);
	va_end (argptr);

	if ( sv.IsDedicated() )
	{
		// dedicated servers just exit
		Sys_Error( "Host_Error: %s\n", string );
		return;
	}

#ifndef SWDS
	// Reenable screen updates
	SCR_EndLoadingPlaque ();		
#endif
	ConMsg( "\nHost_Error: %s\n\n", string );

	Host_Disconnect( true, string );

	cl.demonum = -1;

	inerror = false;

	if ( g_bAbortServerSet )
	{
		Host_AbortServer();
	}
}

#ifndef SWDS

ButtonCode_t nInvalidKeyBindings[] =
{
	KEY_PAD_0,
	KEY_PAD_1,
	KEY_PAD_2,
	KEY_PAD_3,
	KEY_PAD_4,
	KEY_PAD_5,
	KEY_PAD_6,
	KEY_PAD_7,
	KEY_PAD_8,
	KEY_PAD_9,
	KEY_PAD_DIVIDE,
	KEY_PAD_MULTIPLY,
	KEY_PAD_MINUS,
	KEY_PAD_PLUS,
	KEY_PAD_ENTER,
	KEY_PAD_DECIMAL,
	KEY_ESCAPE,
	KEY_SCROLLLOCK,
	KEY_INSERT,
	KEY_DELETE,
	KEY_HOME,
	KEY_END,
	KEY_PAGEUP,
	KEY_PAGEDOWN,
	KEY_BREAK,
	KEY_LSHIFT,
	KEY_RSHIFT,
	KEY_LALT,
	KEY_RALT,
	KEY_LCONTROL,
	KEY_RCONTROL,
	KEY_LWIN,
	KEY_RWIN,
	KEY_APP,
	KEY_UP,
	KEY_LEFT,
	KEY_DOWN,
	KEY_RIGHT,
	KEY_CAPSLOCKTOGGLE,
	KEY_NUMLOCKTOGGLE,
	KEY_SCROLLLOCKTOGGLE,
	KEY_BACKQUOTE,
};

//******************************************
// SetupNewBindings
//
// In the rare case that we ship an update
// where we need a key to be bound to a given
// con-command, this function do its best to
// bind those keys, based on a file called
// newbindings.txt.
//******************************************
void SetupNewBindings()
{
	char szBindCmd[ 256 ];

	// Load the file
	const char *pFilename = "scripts\\newbindings.txt";
	KeyValues *pNewBindingsData = new KeyValues( pFilename );
	if ( !pNewBindingsData->LoadFromFile( g_pFileSystem, pFilename ) )
	{
		pNewBindingsData->deleteThis();
		return;
	}

	FOR_EACH_TRUE_SUBKEY( pNewBindingsData, pSubKey )
	{
		// Get the binding
		const char *pBinding = pSubKey->GetName();
		if ( !pBinding )
			continue;

		// Get the ideal key
		const char *pIdealKey = pSubKey->GetString( "ideal_key", NULL );
		if ( !pIdealKey )
			continue;

		// Force the key binding if the ideal key is bound to an irrelevant command, which is the
		// case for CS:S, which binds F6 to "quick save," which has no used in the game at all.
		const char *pOverrideIfCmd = pSubKey->GetString( "override_if", NULL );
		if ( pOverrideIfCmd )
		{
			const char *pCurrentBindingForKey = ::Key_BindingForKey( g_pInputSystem->StringToButtonCode( pIdealKey ) );
			if ( !pCurrentBindingForKey  || !V_stricmp( pOverrideIfCmd, pCurrentBindingForKey ) )
			{
				V_snprintf( szBindCmd, sizeof( szBindCmd ), "bind \"%s\" \"%s\"", pIdealKey, pBinding );
				Cbuf_AddText( szBindCmd );
				continue;
			}
		}

		// Already have a key for this command?
		if ( ::Key_NameForBinding( pBinding ) )
			continue;

		// No binding.  Is the ideal key available?
		ButtonCode_t bcIdealKey = g_pInputSystem->StringToButtonCode( pIdealKey );
		const char *pCurrentBindingForIdealKey = ::Key_BindingForKey( bcIdealKey );
		if ( !pCurrentBindingForIdealKey )
		{
			// Yes - bind to the ideal key.
			V_snprintf( szBindCmd, sizeof( szBindCmd ), "bind \"%s\" \"%s\"", pIdealKey, pBinding );
			Cbuf_AddText( szBindCmd );
			continue;
		}

		// Ideal key already bound - find another key at random and bind it
		bool bFound = false;
		int nNumAttempts = 1;
		for ( int nCurButton = (int)KEY_0; nCurButton <= (int)KEY_LAST; ++nCurButton, ++nNumAttempts )
		{
			// Don't consider numpad, windows keys, etc
			bool bFoundInvalidKey = false;
			for ( int iKeyIndex = 0; iKeyIndex < sizeof( nInvalidKeyBindings )/sizeof( nInvalidKeyBindings[0] ); iKeyIndex++ )
			{
				if ( nCurButton == (int)nInvalidKeyBindings[iKeyIndex] )
				{
					bFoundInvalidKey = true;
					break;
				}
			}

			if ( bFoundInvalidKey )
				continue;

			// Key available?
			ButtonCode_t bcCurButton = (ButtonCode_t)nCurButton;
			if ( !::Key_BindingForKey( bcCurButton ) )
			{
				// Yes - use it.
				V_snprintf( szBindCmd, sizeof( szBindCmd ), "bind \"%s\" \"%s\"", g_pInputSystem->ButtonCodeToString( bcCurButton ), pBinding );
				Cbuf_AddText( szBindCmd );
				bFound = true;
				break;
			}
		}

		// We tried really hard - did we fail?
		if ( !bFound )
		{
			Warning( "Unable to bind a key for command \"%s\" after %i attempt(s).\n", pBinding, nNumAttempts );
		}
	}
}

//******************************************
// UseDefuaultBinding
//
// If the config.cfg file is not present, this
// function is called to set the default key
// bindings to match those defined in kb_def.lst
//******************************************
void UseDefaultBindings( void )
{
	FileHandle_t f;
	char szFileName[ _MAX_PATH ];
	char token[ 1024 ];
	char szKeyName[ 256 ];

	// read kb_def file to get default key binds
	Q_snprintf( szFileName, sizeof( szFileName ), "%skb_def.lst", SCRIPT_DIR );
	f = g_pFileSystem->Open( szFileName, "r");
	if ( !f )
	{
		ConMsg( "Couldn't open kb_def.lst\n" );
		return;
	}

	// read file into memory
	int size = g_pFileSystem->Size(f);
	char *startbuf = new char[ size ];
	g_pFileSystem->Read( startbuf, size, f );
	g_pFileSystem->Close( f );

	const char *buf = startbuf;
	while ( 1 )
	{
		buf = COM_ParseFile( buf, token, sizeof( token ) );
		if ( strlen( token ) <= 0 )
			break;
		Q_strncpy ( szKeyName, token, sizeof( szKeyName ) );

		buf = COM_ParseFile( buf, token, sizeof( token ) );
		if ( strlen( token ) <= 0 )  // Error
			break;

		// finally, bind key
		Key_SetBinding ( g_pInputSystem->StringToButtonCode( szKeyName ), token );
	}
	delete [] startbuf;		// cleanup on the way out
}

static bool g_bConfigCfgExecuted = false;

//-----------------------------------------------------------------------------
// Purpose: Write out our 360 exclusive settings to internal storage
//-----------------------------------------------------------------------------
void Host_WriteConfiguration_360( void )
{
#ifdef _X360
	if ( XBX_GetStorageDeviceId() == XBX_INVALID_STORAGE_ID || XBX_GetStorageDeviceId() == XBX_STORAGE_DECLINED )
		return;

	// Construct the name for our config settings for this mod
	char strFilename[MAX_PATH];
	Q_snprintf( strFilename, sizeof(strFilename), "cfg:/%s_config.cfg", GetCurrentMod() );

	// Always throw away all keys that are left over.
	CUtlBuffer	configBuff( 0, 0, CUtlBuffer::TEXT_BUFFER);
	configBuff.Printf( "unbindall\n" );

	Key_WriteBindings( configBuff );
	cv->WriteVariables( configBuff );

	ConVarRef mat_monitorgamma( "mat_monitorgamma" );
	ConVarRef mat_monitorgamma_tv_enabled( "mat_monitorgamma_tv_enabled" );

	char strVideoFilename[MAX_PATH];
	CUtlBuffer videoBuff( 0, 0, CUtlBuffer::TEXT_BUFFER);
	Q_snprintf( strVideoFilename, sizeof(strVideoFilename), "cfg:/video_config.cfg" );
	videoBuff.Printf( "mat_monitorgamma %f\n", mat_monitorgamma.GetFloat() );
	videoBuff.Printf( "mat_monitorgamma_tv_enabled %d\n", mat_monitorgamma_tv_enabled.GetBool() );

	// Anything to write?
	if ( configBuff.TellMaxPut() )
	{
		g_pFileSystem->WriteFile( strFilename, NULL, configBuff );
	}

	if ( videoBuff.TellMaxPut() )
	{
		g_pFileSystem->WriteFile( strVideoFilename, NULL, videoBuff );
	}

	g_pXboxSystem->FinishContainerWrites();
#endif // #ifdef _X360
}

/*
===============
Host_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/

void Host_WriteConfiguration( const char *filename, bool bAllVars )
{
	const bool cbIsUserRequested = ( filename != NULL );

	if ( !filename )
		filename = "config.cfg";

	if ( !g_bConfigCfgExecuted )
		return;
	
	if ( !host_initialized )
		return;

	// Don't write config when in default--most of the values are defaults which is not what the player wants.
	// If bAllVars is set, go ahead and write out the file anyways, since it was requested explicitly.
	if ( !cbIsUserRequested && ( CommandLine()->CheckParm( "-default" ) || host_competitive_ever_enabled.GetBool() ) )
		return;
	
	// Write to internal storage on the 360
	if ( IsX360() )
	{
		Host_WriteConfiguration_360();
		return;
	}

	// If in map editing mode don't save configuration
	if (g_bInEditMode)
	{
		ConMsg( "skipping %s output when in map edit mode\n", filename );
		return;
	}

	// dedicated servers initialize the host but don't parse and set the
	// config.cfg cvars
	if ( sv.IsDedicated() )
		return;

	if ( IsPC() && Key_CountBindings() <= 1 )
	{
		ConMsg( "skipping %s output, no keys bound\n", filename );
		return;
	}

	// force any queued convar changes to flush before reading/writing them
	UpdateMaterialSystemConfig();

	// Generate a new .cfg file.
	char		szFileName[MAX_PATH];
	CUtlBuffer	configBuff( 0, 0, CUtlBuffer::TEXT_BUFFER);

	Q_snprintf( szFileName, sizeof(szFileName), "cfg/%s", filename );
	g_pFileSystem->CreateDirHierarchy( "cfg", "MOD" );
	if ( g_pFileSystem->FileExists( szFileName, "MOD" ) && !g_pFileSystem->IsFileWritable( szFileName, "MOD" ) )
	{
		ConMsg( "Config file %s is read-only!!\n", szFileName );
		return;
	}
	
	// Always throw away all keys that are left over.
	configBuff.Printf( "unbindall\n" );

	Key_WriteBindings( configBuff );
	cv->WriteVariables( configBuff, bAllVars );

#if !defined( SWDS )
		bool down;
		if ( g_ClientDLL->IN_IsKeyDown( "in_jlook", down ) && down )
		{
			configBuff.Printf( "+jlook\n" );
		}
#endif // SWDS

	if ( !configBuff.TellMaxPut() )
	{
		// nothing to write
		return;
	}

#if defined(NO_STEAM)
		AssertMsg( false, "SteamCloud not available on Xbox 360. Badger Martin to fix this." );
#else
		ISteamRemoteStorage *pRemoteStorage = SteamClient()?(ISteamRemoteStorage *)SteamClient()->GetISteamGenericInterface(
			SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), STEAMREMOTESTORAGE_INTERFACE_VERSION ):NULL;

		if ( pRemoteStorage )
		{
			int32 availableBytes, totalBytes = 0;
			if ( pRemoteStorage->GetQuota( &totalBytes, &availableBytes ) )
			{
				if ( totalBytes > 0 )
				{
					if ( cl_cloud_settings.GetInt() == STEAMREMOTESTORAGE_CLOUD_ON )
					{
						// TODO put MOD dir in pathname
						if ( pRemoteStorage->FileWrite( szFileName, configBuff.Base(), configBuff.TellMaxPut() ) )
						{
							DevMsg( "[Cloud]: SUCCEESS saving %s in remote storage\n", szFileName );
						}
						else
						{
							// probably a quota issue. TODO what to do ?
							DevMsg( "[Cloud]: FAILED saving %s in remote storage\n", szFileName );
						}

						// write the current logo file
						char szLogoFileName[MAX_PATH]; 
						Q_strncpy( szLogoFileName, cl_logofile.GetString(), sizeof(szLogoFileName) ); // .vtf file

						if ( g_pFileSystem->FileExists( szLogoFileName, "MOD" ) )
						{
							// store logo .VTF file
							FileHandle_t hFile = g_pFileSystem->Open( szLogoFileName, "rb", "MOD" );
							if ( FILESYSTEM_INVALID_HANDLE != hFile )
							{
								unsigned int unSize = g_pFileSystem->Size( hFile );

								byte *pBuffer = (byte*) malloc( unSize );
								if ( g_pFileSystem->Read( pBuffer, unSize, hFile ) == unSize )
								{
									Q_SetExtension( g_szDefaultLogoFileName, ".vtf", sizeof(g_szDefaultLogoFileName) );
									if ( pRemoteStorage->FileWrite( g_szDefaultLogoFileName, pBuffer, unSize ) )
									{
										DevMsg( "[Cloud]: SUCCEESS saving %s in remote storage\n", g_szDefaultLogoFileName );
									}
									else
									{
										DevMsg( "[Cloud]: FAILED saving %s in remote storage\n", g_szDefaultLogoFileName );
									}
								}
								free( pBuffer );
								g_pFileSystem->Close( hFile );
							}

							// store logo .VMT file
							Q_SetExtension( szLogoFileName, ".vmt", sizeof(szLogoFileName) );
							hFile = g_pFileSystem->Open( szLogoFileName, "rb", "MOD" );
							if ( FILESYSTEM_INVALID_HANDLE != hFile )
							{
								unsigned int unSize = g_pFileSystem->Size( hFile );

								byte *pBuffer = (byte*) malloc( unSize );
								if ( g_pFileSystem->Read( pBuffer, unSize, hFile ) == unSize )
								{
									Q_SetExtension( g_szDefaultLogoFileName, ".vmt", sizeof(g_szDefaultLogoFileName) );
									if ( pRemoteStorage->FileWrite( g_szDefaultLogoFileName, pBuffer, unSize ) )
									{
										DevMsg( "[Cloud]: SUCCEESS saving %s in remote storage\n", g_szDefaultLogoFileName );
									}
									else
									{
										DevMsg( "[Cloud]: FAILED saving %s in remote storage\n", g_szDefaultLogoFileName );
									}
								}
								free( pBuffer );
								g_pFileSystem->Close( hFile );
							}
						}
					}
				}
			}

			// even if SteamCloud worked we still safe the same file locally
		}
#endif


	// make a persistent copy that async will use and free
	char *tempBlock = new char[configBuff.TellMaxPut()];
	Q_memcpy( tempBlock, configBuff.Base(), configBuff.TellMaxPut() );

	// async write the buffer, and then free it
	g_pFileSystem->AsyncWrite( szFileName, tempBlock, configBuff.TellMaxPut(), true );

	ConMsg( "Host_WriteConfiguration: Wrote %s\n", szFileName );
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve and set any defaults from the user's gamer profile
//-----------------------------------------------------------------------------
bool XBX_SetProfileDefaultSettings( void )
{
	// These defined values can't play nicely with the PC, so we need to ignore them for that build target
#ifdef _X360
	// These will act as indices into the array that is returned by the API
	enum
	{
		XPS_GAMER_DIFFICULTY,
		XPS_GAMER_ACTION_MOVEMENT_CONTROL,
		XPS_GAMER_YAXIS_INVERSION,
		XPS_OPTION_CONTROLLER_VIBRATION,
		NUM_PROFILE_SETTINGS
	};

	// These are the values we're interested in having returned (must match the indices above)
	const DWORD dwSettingIds[ NUM_PROFILE_SETTINGS ] =
	{
		XPROFILE_GAMER_DIFFICULTY,
		XPROFILE_GAMER_ACTION_MOVEMENT_CONTROL,
		XPROFILE_GAMER_YAXIS_INVERSION,
		XPROFILE_OPTION_CONTROLLER_VIBRATION
	};

	// Must have a valid primary user by this point
	int nPrimaryID = XBX_GetPrimaryUserId();

	// First, we call with a NULL pointer and zero size to retrieve the buffer size we'll get back
	DWORD dwResultSize = 0;	// Must be zero to get the correct size back
	XUSER_READ_PROFILE_SETTING_RESULT *pResults = NULL;
	DWORD dwError = XUserReadProfileSettings(	0,			// Family ID (current title)
												nPrimaryID,	// User ID
												NUM_PROFILE_SETTINGS,
												dwSettingIds,
												&dwResultSize,
												pResults,
												NULL );

	// We need this to inform us that it's given us a size back for the buffer
	if ( dwError != ERROR_INSUFFICIENT_BUFFER )
		return false;

	// Now we allocate that buffer and supply it to the call
	BYTE *pData = (BYTE *) stackalloc( dwResultSize );
	ZeroMemory( pData, dwResultSize );

	pResults = (XUSER_READ_PROFILE_SETTING_RESULT *) pData;

	dwError = XUserReadProfileSettings(	0,			// Family ID (current title)
										nPrimaryID,	// User ID
										NUM_PROFILE_SETTINGS,
										dwSettingIds,
										&dwResultSize,
										pResults,
										NULL );	// Not overlapped, must be synchronous

	// We now have a raw buffer of results
	if ( dwError != ERROR_SUCCESS )
		return false;

	//
	// Skill
	//

	XUSER_PROFILE_SETTING *pSetting = pResults->pSettings + XPS_GAMER_DIFFICULTY;
	Assert( pSetting->data.type == XUSER_DATA_TYPE_INT32 );
	
	int nSkillSetting = pSetting->data.nData;
	int nResultSkill = 0;
	switch( nSkillSetting )
	{
	case XPROFILE_GAMER_DIFFICULTY_NORMAL:
		nResultSkill = 2;
		break;
	
	case XPROFILE_GAMER_DIFFICULTY_HARD:
		nResultSkill = 3;
		break;
	
	case XPROFILE_GAMER_DIFFICULTY_EASY:
	default:
		nResultSkill = 1;
		break;
	}

	// If the mod has no difficulty setting, only easy is allowed
	KeyValues *modinfo = new KeyValues("ModInfo");
	if ( modinfo->LoadFromFile( g_pFileSystem, "gameinfo.txt" ) )
	{
		if ( stricmp(modinfo->GetString("nodifficulty", "0"), "1") == 0 )
			nResultSkill = 1;
	}
	modinfo->deleteThis();

	char szScratch[MAX_PATH];
	Q_snprintf( szScratch, sizeof(szScratch), "skill %d", nResultSkill );
	Cbuf_AddText( szScratch );

	// 
	// Movement control
	//

	pSetting = pResults->pSettings + XPS_GAMER_ACTION_MOVEMENT_CONTROL;
	Assert( pSetting->data.type == XUSER_DATA_TYPE_INT32 );
	Q_snprintf( szScratch, sizeof(szScratch), "joy_movement_stick %d", ( pSetting->data.nData == XPROFILE_ACTION_MOVEMENT_CONTROL_L_THUMBSTICK ) ? 0 : 1 );
	Cbuf_AddText( szScratch );
	Q_snprintf( szScratch, sizeof(szScratch), "joy_movement_stick_default %d", ( pSetting->data.nData == XPROFILE_ACTION_MOVEMENT_CONTROL_L_THUMBSTICK ) ? 0 : 1 );
	Cbuf_AddText( szScratch );
	Cbuf_AddText( "joyadvancedupdate" );

	// 
	// Y-Inversion
	//

	pSetting = pResults->pSettings + XPS_GAMER_YAXIS_INVERSION;
	Assert( pSetting->data.type == XUSER_DATA_TYPE_INT32 );
	Q_snprintf( szScratch, sizeof(szScratch), "joy_inverty %d", pSetting->data.nData );
	Cbuf_AddText( szScratch );
	Q_snprintf( szScratch, sizeof(szScratch), "joy_inverty_default %d", pSetting->data.nData );
	Cbuf_AddText( szScratch );
	
	//
	// Vibration control
	//

	pSetting = pResults->pSettings + XPS_OPTION_CONTROLLER_VIBRATION;
	Assert( pSetting->data.type == XUSER_DATA_TYPE_INT32 );
	Q_snprintf( szScratch, sizeof(szScratch), "cl_rumblescale %d", ( pSetting->data.nData != 0 ) ? 1 : 0 );
	Cbuf_AddText( szScratch );

	// Execute all commands we've queued up
	Cbuf_Execute();
#endif // _X360

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Read our configuration from the 360, filling in defaults on our first run
//-----------------------------------------------------------------------------
void Host_ReadConfiguration_360( void )
{
#ifdef _X360
	
	// Exec our defaults on the first pass
	if ( g_bConfigCfgExecuted == false )
	{
		// First, we exec our default configuration for the 360
		Cbuf_AddText( "exec config.360.cfg game\n" );
		Cbuf_Execute();
	}

	// Can't do any more in this function if we don't have a valid user id
	if ( XBX_GetPrimaryUserId() == INVALID_USER_ID )
		return;

	// Build the config name we're looking for
	char strFileName[MAX_PATH];
	Q_snprintf( strFileName, sizeof(strFileName), "cfg:/%s_config.cfg", GetCurrentMod() );

	bool bStorageDeviceValid = ( XBX_GetStorageDeviceId() != XBX_INVALID_STORAGE_ID && XBX_GetStorageDeviceId() != XBX_STORAGE_DECLINED );
	bool bSaveConfig = false;

	// Call through normal API function once the content container is opened
	if ( CommandLine()->CheckParm( "-forcexboxreconfig" ) || bStorageDeviceValid == false || g_pFileSystem->FileExists( strFileName ) == false )
	{
		// If we've already done this in this session, never do it again (we don't want to stomp their settings under any circumstances)
		if ( g_bConfigCfgExecuted == false )
		{
			// Get and set all our default setting we care about from the Xbox
			XBX_SetProfileDefaultSettings();
		}
		
		// Save out what we have
		bSaveConfig = true;
	}
	else
	{
		// Otherwise, exec the user settings stored on the 360
		char szCommand[MAX_PATH];
		Q_snprintf( szCommand, sizeof( szCommand ), "exec %s_config.cfg x360\n", GetCurrentMod() );
		Cbuf_AddText( szCommand );
		
		// Exec the video config as well
		Q_snprintf( szCommand, sizeof( szCommand ), "exec video_config.cfg x360\n", GetCurrentMod() );
		Cbuf_AddText( szCommand );
		Cbuf_Execute();
	}

	// Mark that we've loaded a config and can now save it
	g_bConfigCfgExecuted = true;

	if ( bSaveConfig )
	{
		// An ugly hack, but we can probably save this safely
		bool saveinit = host_initialized;
		host_initialized = true;
		Host_WriteConfiguration_360();
		host_initialized = saveinit;
	}
#endif // _X360
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : false - 
//-----------------------------------------------------------------------------
void Host_ReadConfiguration()
{
	if ( sv.IsDedicated() )
		return;

	// Rebind keys and set cvars
	if ( !g_pFileSystem )
	{
		Sys_Error( "Host_ReadConfiguration:  g_pFileSystem == NULL\n" );
	}

	// Handle the 360 case
	if ( IsX360() )
	{
		Host_ReadConfiguration_360();
		return;
	}

	bool saveconfig = false;

	ISteamRemoteStorage *pRemoteStorage = SteamClient()?(ISteamRemoteStorage *)SteamClient()->GetISteamGenericInterface(
		SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), STEAMREMOTESTORAGE_INTERFACE_VERSION ):NULL;
	
	if ( pRemoteStorage )
	{
		// if cloud settings is default but remote storage does not exist yet, set it to sync all because this is the first
		// computer the game is run on--default to copying everything to the cloud
		if ( !pRemoteStorage->FileExists( "cfg/config.cfg" ) )
		{
			DevMsg( "[Cloud]: Default setting with remote data non-existent, sync all\n" );
			cl_cloud_settings.SetValue( STEAMREMOTESTORAGE_CLOUD_ON );
		}

		if ( cl_cloud_settings.GetInt() == STEAMREMOTESTORAGE_CLOUD_ON )
		{
			// config files are run through the exec command which got pretty complicated with all the splitscreen
			// stuff. Steam UFS doens't support split screen well (2 users ?)
			GetFileFromRemoteStorage( pRemoteStorage, "cfg/config.cfg", "cfg/config.cfg" );
		}
	}

	if ( g_pFileSystem->FileExists( "//mod/cfg/config.cfg" ) )
	{
		Cbuf_AddText( "exec config.cfg\n" );
	}
	else
	{
		Cbuf_AddText( "exec config_default.cfg\n" );
		saveconfig = true;
	}

	Cbuf_Execute();

	if ( pRemoteStorage )
	{
		if ( cl_cloud_settings.GetInt() == STEAMREMOTESTORAGE_CLOUD_ON )
		{
			// get logo .VTF file
			Q_SetExtension( g_szDefaultLogoFileName, ".vtf", sizeof(g_szDefaultLogoFileName) );
			GetFileFromRemoteStorage( pRemoteStorage, g_szDefaultLogoFileName, g_szDefaultLogoFileName );

			cl_logofile.SetValue( g_szDefaultLogoFileName );

			// get logo .VMT file
			Q_SetExtension( g_szDefaultLogoFileName, ".vmt", sizeof(g_szDefaultLogoFileName) );
			GetFileFromRemoteStorage( pRemoteStorage, g_szDefaultLogoFileName, g_szDefaultLogoFileName );
		}
	}

	// check to see if we actually set any keys, if not, load defaults from kb_def.lst
	// so we at least have basics setup.
	int nNumBinds = Key_CountBindings();
	if ( nNumBinds == 0 )
	{
		UseDefaultBindings();
	}
	else
	{
		// Setup new bindings - if we ship an update that requires that every user has a key bound for
		// X concommand, SetupNewBindings() will do its best to ensure that a key is bound.  We assume
		// that kb_def.lst includes any bindings that SetupNewBindings() might include in the case
		// that nNumBinds == 0 above.
		SetupNewBindings();
	}

	Key_SetBinding( KEY_ESCAPE, "cancelselect" );

	// Make sure that something is always bound to console
	if (NULL == Key_NameForBinding("toggleconsole"))
	{
		// If nothing is bound to it then bind it to '
		Key_SetBinding( KEY_BACKQUOTE, "toggleconsole" );
	}

	SetupDefaultAskConnectAcceptKey();

	g_bConfigCfgExecuted = true;

	if ( saveconfig )
	{
		// An ugly hack, but we can probably save this safely
		bool saveinit = host_initialized;
		host_initialized = true;
		Host_WriteConfiguration();
		host_initialized = saveinit;
	}
}

CON_COMMAND( host_writeconfig, "Store current settings to config.cfg (or specified .cfg file)." )
{
	if ( args.ArgC() > 3 )
	{
		ConMsg( "Usage:  writeconfig <filename.cfg> [full]\n" );
		ConMsg( "<filename.cfg> is required, optionally specify \"full\" to write out all archived convars.\n" );
		ConMsg( "By default, only non-default values are written out.\n" );
		return;
	}

	if ( args.ArgC() >= 2 )
	{
		bool bWriteAll = ( args.ArgC() == 3 && V_stricmp( args[ 2 ], "full" ) == 0 );

		char const *filename = args[ 1 ];
		if ( !filename || !filename[ 0 ] )
		{
			return;
		}

		char outfile[ MAX_QPATH ];
		// Strip path and extension from filename
		Q_FileBase( filename, outfile, sizeof( outfile ) );
		Host_WriteConfiguration( va( "%s.cfg", outfile ), bWriteAll );
		if  ( !bWriteAll )
			ConMsg( "Wrote partial config file \"%s\" out, to write full file use host_writeconfig \"%s\" full\n", outfile, outfile );
	}
	else
	{
		Host_WriteConfiguration( NULL, true );
	}
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Does a quick parse of the config.cfg to read cvars that
//			need to be read before any games systems are initialized
//			assumes only cvars and filesystem are initialized
//-----------------------------------------------------------------------------
void Host_ReadPreStartupConfiguration()
{
	FileHandle_t f = NULL;
	if ( IsX360() )
	{
		// 360 config is less restrictive and can be anywhere in the game path
		f = g_pFileSystem->Open( "//game/cfg/config.360.cfg", "rt" );
	}
	else
	{
		f = g_pFileSystem->Open( "//mod/cfg/config.cfg", "rt" );
	}

	if ( !f )
		return;

	// read file into memory
	int size = g_pFileSystem->Size(f);
	char *configBuffer = new char[ size + 1 ];
	g_pFileSystem->Read( configBuffer, size, f );
	configBuffer[size] = 0;
	g_pFileSystem->Close( f );

	// parse out file
	static const char *s_PreStartupConfigConVars[] =
	{
		"sv_unlockedchapters",		// needed to display the startup graphic while loading
		"snd_legacy_surround",		// needed to init the sound system
		"gameui_xbox",				// needed to initialize the correct UI
		"save_in_memory"			// needed to preread data from the correct location in UI
	};

	// loop through looking for all the cvars to apply
	for (int i = 0; i < ARRAYSIZE(s_PreStartupConfigConVars); i++)
	{
		const char *search = Q_stristr(configBuffer, s_PreStartupConfigConVars[i]);
		if (search)
		{
			// read over the token
			search = COM_Parse(search);

			// read the value
			COM_Parse(search);

			// apply the value
			ConVar *var = (ConVar *)g_pCVar->FindVar( s_PreStartupConfigConVars[i] );
			if ( var )
			{
				var->SetValue( com_token );
			}
		}
	}

	// free
	delete [] configBuffer;
}

void Host_RecomputeSpeed_f( void )
{
	ConMsg( "Recomputing clock speed...\n" );

	CClockSpeedInit::Init();
	ConMsg( "Clock speed: %.0f Mhz\n", CFastTimer::GetClockSpeed() / 1000000.0 );
}

static ConCommand recompute_speed( "recompute_speed", Host_RecomputeSpeed_f, "Recomputes clock speed (for debugging purposes).", FCVAR_CHEAT );

void DTI_Flush_f()
{
	DTI_Flush();
	ServerDTI_Flush();
}

static ConCommand dti_flush( "dti_flush", DTI_Flush_f, "Write out the datatable instrumentation files (you must run with -dti for this to work)." );

/*
==================
Host_ShutdownServer

This only happens at the end of a game, not between levels
==================
*/
void Host_ShutdownServer( void )
{
	if ( !sv.IsActive() )
		return;

	Host_AllowQueuedMaterialSystem( false );
	// clear structures
#if !defined( SWDS )
	g_pShadowMgr->LevelShutdown();
#endif
	StaticPropMgr()->LevelShutdown();

	Host_FreeStateAndWorld( true );
	sv.Shutdown();// sv.Shutdown() references some heap memory, so run it before Host_FreeToLowMark()
	Host_FreeToLowMark( true ); 

	IGameEvent *event = g_GameEventManager.CreateEvent( "server_shutdown" );

	if ( event )
	{
		event->SetString( "reason", "restart" );
		g_GameEventManager.FireEvent( event );
	}

	g_Log.Close();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : time - 
// Output : bool
//-----------------------------------------------------------------------------
void Host_AccumulateTime( float dt )
{
	// Accumulate some time
	realtime += dt;

	bool bUseNormalTickTime = true;
#if !defined(SWDS)
	if ( demoplayer->IsPlayingTimeDemo() )
		bUseNormalTickTime = false;
#endif
	if ( g_bDedicatedServerBenchmarkMode )
		bUseNormalTickTime = false;

	if ( bUseNormalTickTime )
	{
		host_frametime	= dt;
	}
	else
	{
		// Used to help increase reproducibility of timedemos
		host_frametime	= host_state.interval_per_tick;
	}

#if 1
	if ( host_framerate.GetFloat() > 0 
#if !defined(SWDS)
		&& ( CanCheat() || demoplayer->IsPlayingBack() ) 
#endif
		)
	{
		float fps = host_framerate.GetFloat();
		if ( fps > 1 )
		{
			fps = 1.0f/fps;
		}
		host_frametime = fps;

#if !defined(SWDS) && defined( REPLAY_ENABLED )
		extern IDemoPlayer *g_pReplayDemoPlayer;
		if ( demoplayer->IsPlayingBack() && demoplayer == g_pReplayDemoPlayer )
		{
			// adjust time scale if playing back demo
			host_frametime *= demoplayer->GetPlaybackTimeScale();
		}
#endif

		host_frametime_unbounded = host_frametime;
	}
	else if (host_timescale.GetFloat() > 0 
#if !defined(SWDS)
		&& ( CanCheat() || demoplayer->IsPlayingBack() ) 
#endif
		)
	{
		float fullscale = host_timescale.GetFloat();

#if !defined(SWDS)
		if ( demoplayer->IsPlayingBack() )
		{
			// adjust time scale if playing back demo
			fullscale *= demoplayer->GetPlaybackTimeScale();
		}
#endif

		host_frametime *= fullscale;

		host_frametime_unbounded = host_frametime;

#ifndef NO_TOOLFRAMEWORK
		if ( CommandLine()->CheckParm( "-tools" ) == NULL )
		{
#endif
			host_frametime = min( (double)host_frametime, MAX_FRAMETIME * fullscale);
#ifndef NO_TOOLFRAMEWORK
		}
#endif
	}
	else
#ifndef NO_TOOLFRAMEWORK
		if ( CommandLine()->CheckParm( "-tools" ) != NULL )
		{
			host_frametime_unbounded = host_frametime;
		}
		else
#endif // !NO_TOOLFRAMEWORK
	{	// don't allow really long or short frames
		host_frametime_unbounded = host_frametime;
		host_frametime = min( (double)host_frametime, MAX_FRAMETIME );
		host_frametime = max( (double)host_frametime, MIN_FRAMETIME );
	}
#endif

	// Adjust the client clock very slightly to keep it in line with the server clock.
	float adj = cl.GetClockDriftMgr().AdjustFrameTime( host_frametime ) - host_frametime;
	host_frametime += adj;
	host_frametime_unbounded += adj;

	if ( g_pSoundServices )									// not present on linux server
		g_pSoundServices->SetSoundFrametime(dt, host_frametime);

}

#define FPS_AVG_FRAC 0.9f

float g_fFramesPerSecond = 0.0f;

/*
==================
Host_PostFrameRate
==================
*/
void Host_PostFrameRate( float frameTime )
{
	frameTime = clamp( frameTime, 0.0001f, 1.0f );

	float fps = 1.0f / frameTime;
	g_fFramesPerSecond = g_fFramesPerSecond * FPS_AVG_FRAC + ( 1.0f - FPS_AVG_FRAC ) * fps;
}

/*
==================
Host_GetHostInfo
==================
*/
void Host_GetHostInfo(float *fps, int *nActive, int *nMaxPlayers, char *pszMap, int maxlen )
{
	// Count clients, report 
	int clients = sv.GetNumClients();

	*fps = g_fFramesPerSecond;
	*nActive = clients;

	if (pszMap)
	{
		if (sv.m_szMapname && sv.m_szMapname[0])
			Q_strncpy(pszMap, sv.m_szMapname, maxlen );
		else
			pszMap[0] = '\0';
	}

	*nMaxPlayers = sv.GetMaxClients();
}

static bool AppearsNumeric( char const *in )
{
	char const *p = in;
	int special[ 3 ];
	Q_memset( special, 0, sizeof( special ) );

	for ( ; *p; p++ )
	{
		if ( *p == '-' )
		{
			special[0]++;
			continue;
		}

		if ( *p == '+' )
		{
			special[1]++;
			continue;
		}

		if ( *p >= '0' && *p <= '9' )
		{
			continue;
		}

		if ( *p == '.' )
		{
			special[2]++;
			continue;
		}

		return false;
	}

	// Can't have multiple +, -, or decimals
	for ( int i = 0; i < 3; i++ )
	{
		if ( special[ i ] > 1 )
			return false;
	}

	// can't be + and - at same time
	if ( special[ 0 ] && special[ 1 ] )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: If the value is numeric, remove unnecessary trailing zeros
// Input  : *invalue - 
// Output : char const
//-----------------------------------------------------------------------------
char const * Host_CleanupConVarStringValue( char const *invalue )
{
	static char clean[ 256 ];

	Q_snprintf( clean, sizeof( clean ), "%s", invalue );

	// Don't mess with empty string
	// Otherwise, if it appears numeric and has a decimal, try to strip all zeroes after decimal
	if ( Q_strlen( clean ) >= 1 && AppearsNumeric( clean ) && Q_strstr( clean, "." ) )
	{
		char *end = clean + strlen( clean ) - 1;
		while ( *end && end >= clean )
		{
			// Removing trailing zeros
			if ( *end != '0' )
			{
				// Remove decimal, zoo
				if ( *end == '.' )
				{
					if ( end == clean )
					{
						*end = '0';
					}
					else
					{
						*end = 0;
					}
				}
				break;
			}

			*end-- = 0;
		}
	}

	return clean;
}

int Host_CountVariablesWithFlags( int flags, bool nonDefault )
{
	int i = 0;
	const ConCommandBase *var;

	for ( var = g_pCVar->GetCommands() ; var ; var=var->GetNext() )
	{
		if ( var->IsCommand() )
			continue;

		const ConVar *pCvar = ( const ConVar * )var;

		if ( !pCvar->IsFlagSet( flags ) )
			continue;

		// It's == to the default value, don't count
		if ( nonDefault && !Q_strcasecmp( pCvar->GetDefault(), pCvar->GetString() ) )
			continue;

		i++;
	}

	return i;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg - 
//-----------------------------------------------------------------------------
void Host_BuildConVarUpdateMessage( NET_SetConVar *cvarMsg, int flags, bool nonDefault )
{
	int count = Host_CountVariablesWithFlags( flags, nonDefault );

	// Nothing to send
	if ( count <= 0 )
		return;

	// Too many to send, error out and have mod author get a clue.
	if ( count > 255 )
	{
		Sys_Error( "Engine only supports 255 ConVars marked %i\n", flags );
		return;
	}

	const ConCommandBase *var;

	for ( var = g_pCVar->GetCommands() ; var ; var=var->GetNext() )
	{
		if ( var->IsCommand() )
			continue;

		const ConVar *pCvar = ( const ConVar * )var;

		if ( !pCvar->IsFlagSet( flags ) )
			continue;

		// It's == to the default value, don't count
		if ( nonDefault && !Q_strcasecmp( pCvar->GetDefault(), pCvar->GetString() ) )
			continue;

		NET_SetConVar::cvar_t acvar;

		Q_strncpy( acvar.name, pCvar->GetName(), MAX_OSPATH );
		Q_strncpy( acvar.value, Host_CleanupConVarStringValue( pCvar->GetString() ), MAX_OSPATH );

		cvarMsg->m_ConVars.AddToTail( acvar );
	}

	// Make sure this count matches original one!!!
	Assert( cvarMsg->m_ConVars.Count() == count );
}

#if !defined( SWDS )
// FIXME: move me somewhere more appropriate
void CL_SendVoicePacket(bool bFinal)
{
#if !defined( NO_VOICE )
	if ( !Voice_IsRecording() )
		return;

	CLC_VoiceData voiceMsg;

	// Get whatever compressed data there is and and send it.
	char uchVoiceData[2048];

	voiceMsg.m_DataOut.StartWriting( uchVoiceData, sizeof(uchVoiceData) );	

	voiceMsg.m_nLength = Voice_GetCompressedData( uchVoiceData, sizeof(uchVoiceData), bFinal ) * 8;

	if( !voiceMsg.m_nLength )
		return;

	voiceMsg.m_DataOut.SeekToBit( voiceMsg.m_nLength );	 // set correct writing position

	if ( cl.IsActive() )
	{
		cl.m_NetChannel->SendNetMsg( voiceMsg );
	}
#endif
}

#if defined ( _X360 )


void CL_ProcessXboxVoiceData()
{
	if ( Audio_GetXVoice() == NULL )
		return;

	if ( Audio_GetXVoice()->VoiceUpdateData() == true )
	{
		if ( cl.IsActive() )
		{
			Audio_GetXVoice()->VoiceSendData( cl.m_NetChannel );
		}
	}
}

#endif

void CL_ProcessVoiceData()
{
	VPROF_BUDGET( "CL_ProcessVoiceData", VPROF_BUDGETGROUP_OTHER_NETWORKING );

#if !defined( NO_VOICE )
	Voice_Idle(host_frametime);
	CL_SendVoicePacket(false);
#endif

#if defined ( _X360 )

	CL_ProcessXboxVoiceData();
#endif

}
#endif






/*
=====================
Host_UpdateScreen

Refresh the screen
=====================
*/
void Host_UpdateScreen( void )
{
#ifndef SWDS 

#ifdef _DEBUG
	if( r_ForceRestore.GetInt() )
	{
		ForceMatSysRestore();
		r_ForceRestore.SetValue(0);
	}
#endif // _DEBUG

	// Refresh the screen
	SCR_UpdateScreen ();
#endif
}

/*
====================
Host_UpdateSounds

Update sound subsystem and cd audio
====================
*/
void Host_UpdateSounds( void )
{
#if !defined( SWDS )
	// update audio
	if ( cl.IsActive() )
	{
		S_Update( &s_AudioState );	
	}
	else
	{
		S_Update( NULL );
	}
#endif
}

/*
==============================
Host_Speeds

==============================
*/
void CFrameTimer::ResetDeltas()
{
	for ( int i = 0; i < NUM_FRAME_SEGMENTS; i++ )
	{
		deltas[ i ] = 0.0f;
	}
}

void CFrameTimer::MarkFrame()
{
	double frameTime;
	double fps;

	// ConDMsg("%f %f %f\n", time1, time2, time3 );

	float fs_input = (deltas[FRAME_SEGMENT_INPUT])*1000.0;
	float fs_client = (deltas[FRAME_SEGMENT_CLIENT])*1000.0;
	float fs_server = (deltas[FRAME_SEGMENT_SERVER])*1000.0;
	float fs_render = (deltas[FRAME_SEGMENT_RENDER])*1000.0;
	float fs_sound = (deltas[FRAME_SEGMENT_SOUND])*1000.0;
	float fs_cldll = (deltas[FRAME_SEGMENT_CLDLL])*1000.0;
	float fs_exec = (deltas[FRAME_SEGMENT_CMD_EXECUTE])*1000.0;

	ResetDeltas();

	frameTime = host_frametime;
	//frameTime /= 1000.0;
	if ( frameTime < 0.0001 )
	{
		fps = 999.0;
	}
	else
	{
		fps = 1.0 / frameTime;
	}

	if (host_speeds.GetInt())
	{
		int ent_count = 0;
		int i;
		static int last_host_tickcount;

		int ticks = host_tickcount - last_host_tickcount;
		last_host_tickcount = host_tickcount;

		// count used entities
		for (i=0 ; i<sv.num_edicts ; i++)
		{
			if (!sv.edicts[i].IsFree())
				ent_count++;
		}

		char sz[ 256 ];
		Q_snprintf( sz, sizeof( sz ),
			"%3i fps -- inp(%.2f) sv(%.2f) cl(%.2f) render(%.2f) snd(%.2f) cl_dll(%.2f) exec(%.2f) ents(%d) ticks(%d)",
			(int)fps, 
			fs_input, 
			fs_server, 
			fs_client, 
			fs_render, 
			fs_sound, 
			fs_cldll, 
			fs_exec, 
			ent_count, 
			ticks );

#ifndef SWDS
		if ( host_speeds.GetInt() >= 2 )
		{
			Con_NPrintf ( 0, sz );
		}
		else
		{
			ConDMsg ( "%s\n", sz );
		}
#endif
	}

}

#define FRAME_TIME_FILTER_TIME 0.5f

void CFrameTimer::ComputeFrameVariability()
{
	m_pFrameTimeHistory[m_nFrameTimeHistoryIndex] = frametime;
	if ( ++m_nFrameTimeHistoryIndex >= FRAME_HISTORY_COUNT )
	{
		m_nFrameTimeHistoryIndex = 0;
	}

	// Compute a low-pass filter of the frame time over the last half-second
	// Count the number of samples that live within the last half-second
	int i = m_nFrameTimeHistoryIndex;
	int nMaxSamples = 0;
	float flTotalTime = 0.0f;
	while( (nMaxSamples < FRAME_HISTORY_COUNT) && (flTotalTime <= FRAME_TIME_FILTER_TIME) )
	{
		if ( --i < 0 )
		{
			i = FRAME_HISTORY_COUNT - 1;
		}		
		if ( m_pFrameTimeHistory[i] == 0.0f )
			break;

		flTotalTime += m_pFrameTimeHistory[i];
		++nMaxSamples;
	}

	if ( nMaxSamples == 0 )
	{
		m_flFPSVariability = 0.0f;
		m_flFPSStdDeviationSeconds = 0.0f;
		return;
	}

	float flExponent = -2.0f / (int)nMaxSamples;

	i = m_nFrameTimeHistoryIndex;
	float flAverageTime = 0.0f;
	float flExpCurveArea = 0.0f;
	int n = 0;
	while( n < nMaxSamples )
	{
		if ( --i < 0 )
		{
			i = FRAME_HISTORY_COUNT - 1;
		}
		flExpCurveArea += exp( flExponent * n );
		flAverageTime += m_pFrameTimeHistory[i] * exp( flExponent * n );
		++n;
	}

	flAverageTime /= flExpCurveArea;

	float flAveFPS = 0.0f;
	if ( flAverageTime != 0.0f )
	{
		flAveFPS = 1.0f / flAverageTime;
	}

	float flCurrentFPS = 0.0f;
	if ( frametime != 0.0f )
	{
		flCurrentFPS = 1.0f / frametime;
	}

	// Now subtract out the current fps to get variability in FPS
	m_flFPSVariability = fabs( flCurrentFPS - flAveFPS );

	// Now compute variance/stddeviation
	double sum = 0.0f;
	int count =0;
	for ( int j = 0; j < FRAME_HISTORY_COUNT; ++j )
	{
		if ( m_pFrameTimeHistory[ j ] == 0.0f )
			continue;

		double ft = min( (double)m_pFrameTimeHistory[ j ], 0.25 );

		sum += ft;
		++count;

	}

	if ( count <= 1 )
	{
		return;
	}

	double avg = sum / (double)count;
	double devSquared = 0.0f;
	for ( int j = 0; j < FRAME_HISTORY_COUNT; ++j )
	{
		if ( m_pFrameTimeHistory[ j ] == 0.0f )
			continue;

		double ft = min( (double)m_pFrameTimeHistory[ j ], 0.25 );

		double dt = ft - avg;

		devSquared += ( dt * dt );
	}

	double variance = devSquared / (double)( count - 1 );
	m_flFPSStdDeviationSeconds = sqrt( variance );
}

void Host_Speeds()
{
	g_HostTimes.MarkFrame();
#if !defined(SWDS)
	ToClientDemoPlayer( demoplayer )->MarkFrame( g_HostTimes.m_flFPSVariability );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: When singlestep == 1, then you must set next == 1 to run to the 
//  next frame.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool Host_ShouldRun( void )
{
	static int current_tick = -1;

	// See if we are single stepping
	if ( !singlestep.GetInt() )
	{
		return true;
	}

	// Did user set "next" to 1?
	if ( cvarNext.GetInt() )
	{
		// Did we finally finish this frame ( Host_ShouldRun is called in 3 spots to pause
		//  three different things ).
		if ( current_tick != (host_tickcount-1) )
		{
			// Okay, time to reset to halt execution again
			cvarNext.SetValue( 0 );
			return false;
		}

		// Otherwise, keep running this one frame since we are still finishing this frame
		return true;
	}
	else
	{
		// Remember last frame without "next" being reset ( time is locked )
		current_tick = host_tickcount;
		// Time is locked
		return false;
	}
}

static ConVar mem_periodicdumps( "mem_periodicdumps", "0", 0, "Write periodic memstats dumps every n seconds." );
static double g_flLastPeriodicMemDump = -1.0f;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static float g_TimeLastMemTest;
void Host_CheckDumpMemoryStats( void )
{
	if ( mem_test_each_frame.GetBool() )
	{
		if ( !MemAlloc_CrtCheckMemory() )
		{
			DebuggerBreakIfDebugging();
			Error( "Heap is corrupt\n" );
		}
	}
	else if ( mem_test_every_n_seconds.GetInt() > 0 )
	{
		float now = Plat_FloatTime();
		if ( now - g_TimeLastMemTest > mem_test_every_n_seconds.GetInt() )
		{
			g_TimeLastMemTest = now;
			if ( !MemAlloc_CrtCheckMemory() )
			{
				DebuggerBreakIfDebugging();
				Error( "Heap is corrupt\n" );
			}
		}
	}

	if ( mem_periodicdumps.GetFloat() > 0.0f )
	{
		double curtime = Plat_FloatTime();
		if ( curtime - g_flLastPeriodicMemDump > mem_periodicdumps.GetFloat() )
		{
			const char *pTest = sv.GetMapName();
			if ( !pTest || !pTest[0] )
			{
				// possibly at menu
				pTest = "nomap";
			}

			char mapname[ 256 ];
			Q_FileBase( pTest, mapname, sizeof( mapname ) );
#if defined( _MEMTEST )
			MemAlloc_SetStatsExtraInfo( pTest, "" );
#endif
			MemAlloc_DumpStatsFileBase( mapname );
			g_flLastPeriodicMemDump = curtime;
		}
	}


#if defined(_WIN32)
	if ( mem_dumpstats.GetInt() <= 0 )
		return;

	if ( mem_dumpstats.GetInt() == 1 )
		mem_dumpstats.SetValue( 0 ); // reset cvar, dump stats only once

	_CrtMemState state;
	Q_memset( &state, 0, sizeof( state ) );
	_CrtMemCheckpoint( &state );

	unsigned int size = 0;

	for ( int use = 0; use < _MAX_BLOCKS; use++)
	{
		size += state.lSizes[ use ];
	}
	Msg("MEMORY:  Run-time Heap\n------------------------------------\n");

	Msg( "\tHigh water %s\n", Q_pretifymem( state.lHighWaterCount,4 ) );
	Msg( "\tCurrent mem %s\n", Q_pretifymem( size,4 ) );
	Msg("------------------------------------\n");
	int hunk = Hunk_MallocSize();
	Msg("\tAllocated outside hunk:  %s\n", Q_pretifymem( size - hunk ) );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void _Host_SetGlobalTime()
{
	// Server
	g_ServerGlobalVariables.realtime			= realtime;
	g_ServerGlobalVariables.framecount			= host_framecount;
	g_ServerGlobalVariables.absoluteframetime	= host_frametime;
	g_ServerGlobalVariables.interval_per_tick	= host_state.interval_per_tick;
	g_ServerGlobalVariables.serverCount			= Host_GetServerCount();
#ifndef SWDS
	// Client
	g_ClientGlobalVariables.realtime			= realtime;
	g_ClientGlobalVariables.framecount			= host_framecount;
	g_ClientGlobalVariables.absoluteframetime	= host_frametime;
	g_ClientGlobalVariables.interval_per_tick	= host_state.interval_per_tick;
#endif
}

/*
==================
_Host_RunFrame

Runs all active servers
==================
*/

void _Host_RunFrame_Input( float accumulated_extra_samples, bool bFinalTick )
{
	VPROF_BUDGET( "_Host_RunFrame_Input", _T("Input") );

	// Run a test script?
	static bool bFirstFrame = true;
	if ( bFirstFrame )
	{
		bFirstFrame = false;
		const char *pScriptFilename = CommandLine()->ParmValue( "-testscript" );
		if ( pScriptFilename )
		{
			if ( !GetTestScriptMgr()->StartTestScript( pScriptFilename ) )
			{
				Error( "StartTestScript( %s ) failed.", pScriptFilename );
			}
		}
	}

	g_HostTimes.StartFrameSegment( FRAME_SEGMENT_INPUT );

#ifndef SWDS
	// Client can process input
	ClientDLL_ProcessInput( );

	g_HostTimes.StartFrameSegment( FRAME_SEGMENT_CMD_EXECUTE );

	// process console commands
	Cbuf_Execute ();

	g_HostTimes.EndFrameSegment( FRAME_SEGMENT_CMD_EXECUTE );

	// Send any current movement commands to server and flush reliable buffer even if not moving yet.
	CL_Move( accumulated_extra_samples, bFinalTick );

#endif

	g_HostTimes.EndFrameSegment( FRAME_SEGMENT_INPUT );
}

void _Host_RunFrame_Server( bool finaltick )
{
	VPROF_BUDGET( "_Host_RunFrame_Server", VPROF_BUDGETGROUP_GAME );
	VPROF_INCREMENT_COUNTER( "ticks", 1 );
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	// Run the Server frame ( read, run physics, respond )
	g_HostTimes.StartFrameSegment( FRAME_SEGMENT_SERVER );
	SV_Frame ( finaltick );
	g_HostTimes.EndFrameSegment( FRAME_SEGMENT_SERVER );

	// Look for connectionless rcon packets on dedicated servers
	// SV_CheckRcom(); TODO 
}

void _Host_RunFrame_Server_Async( int numticks )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s %d", __FUNCTION__, numticks );

	for ( int tick = 0; tick < numticks; tick++ )
	{ 
		g_ServerGlobalVariables.tickcount = sv.m_nTickCount;
		g_ServerGlobalVariables.simTicksThisFrame = numticks - tick;
		bool bFinalTick = ( tick == (numticks - 1) );
		_Host_RunFrame_Server( bFinalTick );
	}
}


void _Host_RunFrame_Client( bool framefinished )
{
#ifndef SWDS
	VPROF( "_Host_RunFrame_Client" );
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s %d", __FUNCTION__, framefinished );

	g_HostTimes.StartFrameSegment( FRAME_SEGMENT_CLIENT );

	// Get any current state update from server, etc.
	CL_ReadPackets( framefinished );

#if defined( VOICE_OVER_IP )
	// Send any enqueued voice data to the server
	CL_ProcessVoiceData();
#endif // VOICE_OVER_IP

	cl.CheckUpdatingSteamResources();
	cl.CheckFileCRCsWithServer();

	// Resend connection request if needed.
	cl.RunFrame();

	if ( CL_IsHL2Demo() || CL_IsPortalDemo() ) // don't need sv.IsDedicated() because ded servers don't run this
	{
		void CL_DemoCheckGameUIRevealTime();
		CL_DemoCheckGameUIRevealTime();
	}

	Steam3Client().RunFrame();

	g_HostTimes.EndFrameSegment( FRAME_SEGMENT_CLIENT );

	// This takes 1 usec, so it's pretty cheap...
	CL_SetPagedPoolInfo();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Used to set limits on certain convars in multiplayer/sv_cheats mode.
// Returns true if it was called recursively and it early-outed.
//-----------------------------------------------------------------------------
bool CheckVarRange_Generic( ConVar *pVar, int minVal, int maxVal )
{
	if ( !CanCheat() && !Host_IsSinglePlayerGame() )
	{
		int clampedValue = clamp( pVar->GetInt(), minVal, maxVal );
		if ( clampedValue != pVar->GetInt() )
		{
			Warning( "sv_cheats=0 prevented changing %s outside of the range [%d,%d] (was %d).\n", pVar->GetName(), minVal, maxVal, pVar->GetInt() );
			pVar->SetValue( clampedValue );
		}
	}

	return false;
}


void CheckSpecialCheatVars()
{
	static ConVar *mat_picmip = NULL;
	if ( !mat_picmip )
		mat_picmip = g_pCVar->FindVar( "mat_picmip" );

	// In multiplayer, don't allow them to set mat_picmip > 2.	
	if ( mat_picmip )
		CheckVarRange_Generic( mat_picmip, -1, 2 );
	
	CheckVarRange_r_rootlod();
	CheckVarRange_r_lod();
	HandleServerAllowColorCorrection();
}


void _Host_RunFrame_Render()
{
#ifndef SWDS
	VPROF( "_Host_RunFrame_Render" );
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "_Host_RunFrame_Render" );

	CheckSpecialCheatVars();

	int nOrgNoRendering = mat_norendering.GetInt();

	if ( cl_takesnapshot )
	{
		// turn off no-rendering mode, if taking screenshot
		mat_norendering.SetValue( 0 );
	}

	// update video if not running in background
	g_HostTimes.StartFrameSegment( FRAME_SEGMENT_RENDER );

	CL_LatchInterpolationAmount();

	{
		VPROF( "_Host_RunFrame_Render - UpdateScreen" );
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "_Host_RunFrame_Render - UpdateScreen" );
		Host_UpdateScreen();
	}
	{
		VPROF( "_Host_RunFrame_Render - CL_DecayLights" );
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "_Host_RunFrame_Render - CL_DecayLights" );
		CL_DecayLights ();
	}

	g_HostTimes.EndFrameSegment( FRAME_SEGMENT_RENDER );

	saverestore->OnFrameRendered();

#ifdef USE_SDL
	if ( g_pLauncherMgr )
	{
		g_pLauncherMgr->OnFrameRendered();
	}
#endif

	mat_norendering.SetValue( nOrgNoRendering );
#endif
}

void CL_FindInterpolatedAddAngle( float t, float& frac, AddAngle **prev, AddAngle **next )
{
	int c = cl.addangle.Count();

	*prev = NULL;
	*next = NULL;

	AddAngle *pentry = NULL;
	for ( int i = 0; i < c; i++ )
	{
		AddAngle *entry = &cl.addangle[ i ];

		*next = entry;

		// Time is earlier
		if ( t < entry->starttime )
		{
			if ( i == 0 )
			{
				*prev = *next;
				frac = 0.0f;
				return;
			}

			// Avoid div by zero
			if ( entry->starttime == pentry->starttime )
			{
				frac = 0.0f;
				return;
			}

			// Time spans the two entries
			frac = ( t - pentry->starttime ) / ( entry->starttime - pentry->starttime );
			frac = clamp( frac, 0.0f, 1.0f );
			return;
		}

		*prev = *next;
		pentry = entry;
	}
}

void CL_DiscardOldAddAngleEntries( float t )
{
	float killtime = t - host_state.interval_per_tick - 0.1f;

	for ( int i = 0; i < cl.addangle.Count(); i++ )
	{
		AddAngle *p = &cl.addangle[ i ];
		if ( p->starttime <= killtime )
		{
			cl.addangle.Remove( i );
			--i;
		}
	}

	// It's safe to reset the master counter once all the entries decay
	if ( cl.addangle.Count() == 0 )
	{
		cl.prevaddangletotal = cl.addangletotal = 0.0f;
	}
}

#ifndef SWDS
void CL_ApplyAddAngle()
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	float curtime = cl.GetTime() - host_state.interval_per_tick;

	AddAngle *prev = NULL, *next = NULL;
	float frac = 0.0f;

	float addangletotal = 0.0f;

	CL_FindInterpolatedAddAngle( curtime, frac, &prev, &next );

	if ( prev && next )
	{
		addangletotal = prev->total + frac * ( next->total - prev->total );
	}
	else
	{ 
		addangletotal = cl.prevaddangletotal;
	}

	float amove = addangletotal - cl.prevaddangletotal;

	// Update view angles
	cl.viewangles[ 1 ] += amove;
	// Update client .dll view of angles
	g_pClientSidePrediction->SetLocalViewAngles( cl.viewangles );

	// Remember last total
	cl.prevaddangletotal = addangletotal;

	CL_DiscardOldAddAngleEntries( curtime );
}
#endif

void _Host_RunFrame_Sound()
{
#ifndef SWDS

	VPROF_BUDGET( "_Host_RunFrame_Sound", VPROF_BUDGETGROUP_OTHER_SOUND );

	g_HostTimes.StartFrameSegment( FRAME_SEGMENT_SOUND );

	Host_UpdateSounds();

	g_HostTimes.EndFrameSegment( FRAME_SEGMENT_SOUND );
#endif
}

float Host_GetSoundDuration( const char *pSample )
{
#ifndef SWDS
	if (!sv.IsDedicated())
	{
		extern float SV_GetSoundDuration(const char *pSample);
		extern float AudioSource_GetSoundDuration(CSfxTable *pSfx);
		int index = cl.LookupSoundIndex(pSample);
		if (index >= 0)
			return AudioSource_GetSoundDuration(cl.GetSound(index));
		return SV_GetSoundDuration(pSample);
	}
#endif
	return 0.0f;
}

CON_COMMAND( host_runofftime, "Run off some time without rendering/updating sounds\n" )
{
	if ( args.ArgC() != 2 )
	{
		ConMsg( "Usage:  host_runofftime <seconds>\n" );
		return;
	}

	if ( !sv.IsActive() )
	{
		ConMsg( "host_ruofftime:  must be running a server\n" );
		return;
	}

	if ( sv.IsMultiplayer() )
	{
		ConMsg( "host_ruofftime:  only valid in single player\n" );
		return;
	}

	float advanceTime = atof( args[1] );
	if ( advanceTime <= 0.0f )
		return;

	// 15 minutes is a _long_ time!!!
	if ( advanceTime > 15.0f * 60.0f )
	{
		ConMsg( "host_runofftime would run off %.2f minutes!!! ignoring\n",
			advanceTime / 60.0f );
		return;
	}

	ConMsg( "Skipping ahead for %f seconds\n", advanceTime );

	SCR_UpdateScreen();
	SCR_UpdateScreen ();
}

#if !defined( _X360 )
S_API int SteamGameServer_GetIPCCallCount();
#else
S_API int SteamGameServer_GetIPCCallCount() { return 0; }
#endif
void Host_ShowIPCCallCount()
{
	// If set to 0 then get out.
	if ( host_ShowIPCCallCount.GetInt() == 0 )
		return;
	
	static float s_flLastTime = 0;
	static int s_nLastTick = host_tickcount;
	static int s_nLastFrame = host_framecount;
	
	// Figure out how often they want to update.
	double flInterval = 0;
	if ( host_ShowIPCCallCount.GetFloat() > 0 )
	{
		flInterval = 1.0f / host_ShowIPCCallCount.GetFloat();
	}
	
	// This is called every frame so increment the frame counter.
	double flCurTime = Plat_FloatTime();
	if ( flCurTime - s_flLastTime >= flInterval )
	{
		uint32 callCount;
		ISteamClient *pSteamClient = SteamClient();
		if ( pSteamClient )
		{
			callCount = pSteamClient->GetIPCCallCount();
		}
		else
		{
			// Ok, we're a dedicated server and we need to use this to get it.
			callCount = (uint32)SteamGameServer_GetIPCCallCount();
		}

		// Avoid a divide by zero.
		int frameCount = host_framecount - s_nLastFrame;
		int tickCount = host_tickcount - s_nLastTick;
		if ( frameCount == 0 || tickCount == 0 )
			return;
			
		Msg( "host_ShowIPCCallCount: %d IPC calls in the past [%d frames, %d ticks]  Avg: [%.2f/frame, %.2f/tick]\n", 
			callCount, frameCount, tickCount, (float)callCount / frameCount, (float)callCount / tickCount );
			
		s_flLastTime = flCurTime;
		s_nLastTick = host_tickcount;
		s_nLastFrame = host_framecount;
	}
}

void Host_SetClientInSimulation( bool bInSimulation )
{
#ifndef SWDS
	// Tracker 77931:  If the game is paused, then lock the client clock at the previous tick boundary 
	//  (otherwise we'll keep interpolating through the "remainder" time causing the paused characters
	//  to twitch like they have the shakes)
	// TODO:  Since this rounds down on the frame we paused, we could see a slight backsliding.  We could remember the last "remainder" before pause and re-use it and 
	//  set insimulation == false to be mroe exact.  We'd still have to deal with the timing difference between
	//  when pause/unpause happens on the server versus the client
	cl.insimulation = bInSimulation || cl.IsPaused();

	// Compute absolute/render time stamp
	g_ClientGlobalVariables.curtime = cl.GetTime();
	g_ClientGlobalVariables.frametime = cl.GetFrameTime();
#endif
}

static ConVar host_Sleep( "host_sleep", "0", FCVAR_CHEAT, "Force the host to sleep a certain number of milliseconds each frame." );
extern ConVar sv_alternateticks;
#define LOG_FRAME_OUTPUT 0

void _Host_RunFrame (float time)
{
	MDLCACHE_COARSE_LOCK_(g_pMDLCache);
	static double host_remainder = 0.0f;
	double prevremainder;
	bool shouldrender;

#if defined( RAD_TELEMETRY_ENABLED )
	if( g_Telemetry.DemoTickEnd == ( uint32 )-1 )
	{
		Cbuf_AddText( "quit\n" );
	}
#endif

	int numticks;
	{
		// Profile scope specific to the top of this function, protect from setjmp() problems
		VPROF( "_Host_RunFrame_Upto_MarkFrame" );

		if ( host_checkheap )
		{
#if defined(_WIN32)
			if ( _heapchk() != _HEAPOK )
			{
				Sys_Error( "_Host_RunFrame (top):  _heapchk() != _HEAPOK\n" );
			}
#endif
		}

		// When playing back a VCR file, don't do host_sleep. That way, if it was recorded with
		// host_sleep on, it'll play back way Faster.
		if ( host_Sleep.GetInt() && VCRGetMode() != VCR_Playback )
		{
			Sys_Sleep( host_Sleep.GetInt() );
		}

		// Slow down the playback?	
		if ( g_iVCRPlaybackSleepInterval )
		{
			Sys_Sleep( g_iVCRPlaybackSleepInterval );
		}

		MapReslistGenerator().RunFrame();

		static int lastrunoffsecond = -1;

		if ( setjmp ( host_enddemo ) )
			return;			// demo finished.

		// decide the simulation time
		Host_AccumulateTime ( time );
		_Host_SetGlobalTime();

		shouldrender = !sv.IsDedicated();

		// FIXME:  Could track remainder as fractional ticks instead of msec
		prevremainder = host_remainder;
		if ( prevremainder < 0 )
			prevremainder = 0;

	#if !defined(SWDS)
		if ( !demoplayer->IsPlaybackPaused() )
	#endif
		{
			host_remainder += host_frametime;
		}

		numticks = 0;	// how many ticks we will simulate this frame
		if ( host_remainder >= host_state.interval_per_tick )
		{
			numticks = (int)( floor( host_remainder / host_state.interval_per_tick ) );

			// round to nearest even ending tick in alternate ticks mode so the last
			// tick is always simulated prior to updating the network data
			// NOTE: alternate ticks only applies in SP!!!
			if ( Host_IsSinglePlayerGame() &&
				sv_alternateticks.GetBool() )
			{
				int startTick = g_ServerGlobalVariables.tickcount;
				int endTick = startTick + numticks;
				endTick = AlignValue( endTick, 2 );
				numticks = endTick - startTick;
			}

			host_remainder -= numticks * host_state.interval_per_tick;
		}

		host_nexttick = host_state.interval_per_tick - host_remainder;

		g_pMDLCache->MarkFrame();
	}

	{
		// Profile scope, protect from setjmp() problems
		VPROF( "_Host_RunFrame" );
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "_Host_RunFrame" );

		g_HostTimes.StartFrameSegment( FRAME_SEGMENT_CMD_EXECUTE );

		// process console commands
		Cbuf_Execute ();

		// initialize networking for dedicated server after commandline & autoexec.cfg have been parsed
		if ( NET_IsDedicated() && !NET_IsMultiplayer() )
			NET_SetMutiplayer( true );

		g_HostTimes.EndFrameSegment( FRAME_SEGMENT_CMD_EXECUTE );

		// Msg( "Running %i ticks (%f remainder) for frametime %f total %f tick %f delta %f\n", numticks, remainder, host_frametime, host_time );
		g_ServerGlobalVariables.interpolation_amount = 0.0f;
#ifndef SWDS
		g_ClientGlobalVariables.interpolation_amount = 0.0f;

		cl.insimulation = true;
#endif

		host_frameticks = numticks;
		host_currentframetick = 0;

#if !defined( SWDS )
		// This is to make the tool do both sim + rendering on the initial frame
		// cl.IsActive changes in the loop below, as does scr_nextdrawtick
		// We're just caching off the state here so that we have a consistent return value
		// for enginetool->IsInGame the entire frame
		g_pEngineToolInternal->SetIsInGame( cl.IsActive() && ( scr_nextdrawtick == 0 ) );
#endif
		CJob *pGameJob = NULL;

// threaded path only supported in listen server
#ifndef SWDS
		if ( !IsEngineThreaded() )
#endif
		{
#ifndef SWDS
			if ( g_ClientDLL )
			{
				g_ClientDLL->IN_SetSampleTime(host_frametime);
			}
			g_ClientGlobalVariables.simTicksThisFrame = 1;
#endif
			cl.m_tickRemainder = host_remainder;
			g_ServerGlobalVariables.simTicksThisFrame = 1;
			cl.SetFrameTime( host_frametime );
			for ( int tick = 0; tick < numticks; tick++ )
			{ 
				// Emit an ETW event every simulation frame.
				ETWSimFrameMark( sv.IsDedicated() );

				double now = Plat_FloatTime();
				float jitter = now - host_idealtime;

				// Track jitter (delta between ideal time and actual tick execution time)
				host_jitterhistory[ host_jitterhistorypos ] = jitter;
				host_jitterhistorypos = ( host_jitterhistorypos + 1 ) % ARRAYSIZE(host_jitterhistory);

				// Very slowly decay "ideal" towards current wall clock unless delta is large
				if ( fabs( jitter ) > 1.0f )
				{
					host_idealtime = now;
				}
				else
				{
					host_idealtime = 0.99 * host_idealtime + 0.01 * now;
				}

				// process any asynchronous network traffic (TCP), set net_time
				NET_RunFrame( now );

				// Only send updates on final tick so we don't re-encode network data multiple times per frame unnecessarily
				bool bFinalTick = ( tick == (numticks - 1) );

				// initialize networking for dedicated server after commandline & autoexec.cfg have been parsed
				if ( NET_IsDedicated() && !NET_IsMultiplayer() )
					NET_SetMutiplayer( true );

				g_ServerGlobalVariables.tickcount = sv.m_nTickCount;
				// NOTE:  Do we want do this at start or end of this loop?
				++host_tickcount;
				++host_currentframetick;
#ifndef SWDS
				g_ClientGlobalVariables.tickcount = cl.GetClientTickCount();

				// Make sure state is correct
				CL_CheckClientState();
#endif
				//-------------------
				// input processing
				//-------------------
				_Host_RunFrame_Input( prevremainder, bFinalTick );
				prevremainder = 0;
				//-------------------
				//
				// server operations
				//
				//-------------------

				_Host_RunFrame_Server( bFinalTick );

				// Additional networking ops for SPLITPACKET stuff (99.9% of the time this will be an empty list of work)
				NET_SendQueuedPackets();
				//-------------------
				//
				// client operations
				//
				//-------------------
#ifndef SWDS
				if ( !sv.IsDedicated() )
				{
					_Host_RunFrame_Client( bFinalTick );
				}

				toolframework->Think( bFinalTick );
#endif

				host_idealtime += host_state.interval_per_tick;
			}
			
			// run HLTV if active
			if ( hltv )
			{
				tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "hltv->RunFrame()" );
				hltv->RunFrame();
			}

			if ( hltvtest )
			{
				tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "hltvtest->RunFrame()" );
				hltvtest->RunFrame();
			}

#if defined( REPLAY_ENABLED )
			// run replay if active
			if ( replay )
			{
				replay->RunFrame();
			}

			// Update server-side replay history manager
			if ( sv.IsDedicated() && g_pServerReplayContext && g_pServerReplayContext->IsInitialized() )
			{
				g_pServerReplayContext->Think();
			}
#endif

#ifndef SWDS
			// This is a hack to let timedemo pull messages from the queue faster than every 15 msec
			// Also when demoplayer is skipping packets to a certain tick we should process the queue
			// as quickly as we can.
			if ( numticks == 0 && ( demoplayer->IsPlayingTimeDemo() || demoplayer->IsSkipping() ) )
			{
				_Host_RunFrame_Client( true );
			}

			if ( !sv.IsDedicated() )
			{
				tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "Host_SetClientInSimulation" );

				// This causes cl.gettime() to return the true clock being used for rendering (tickcount * rate + remainder)
				Host_SetClientInSimulation( false );
				// Now allow for interpolation on client
				g_ClientGlobalVariables.interpolation_amount = ( cl.m_tickRemainder / host_state.interval_per_tick );

#if defined( REPLAY_ENABLED )
				// Update client-side replay history manager - called here since interpolation_amount is set
				if ( g_pClientReplayContext && g_pClientReplayContext->IsInitialized() )
				{
					g_pClientReplayContext->Think();
				}
#endif

				//-------------------
				// Run prediction if it hasn't been run yet
				//-------------------
				// If we haven't predicted/simulated the player (multiplayer with prediction enabled and
				//  not a listen server with zero frame lag, then go ahead and predict now
				CL_RunPrediction( PREDICTION_NORMAL );

				CL_ApplyAddAngle();

				// The mouse is always simulated for the current frame's time
				// This makes updates smooth in every case
				// continuous controllers affecting the view are also simulated this way
				// but they have a cap applied by IN_SetSampleTime() so they are not also
				// simulated during input gathering
				CL_ExtraMouseUpdate( g_ClientGlobalVariables.frametime );
			}
#endif
#if defined( REPLAY_ENABLED )
			// Let the replay system think
			if ( g_pReplay )
			{
				g_pReplay->Think();
			}
#endif
#if LOG_FRAME_OUTPUT
			if ( !cl.IsPaused() || !sv.IsPaused() )
			{
				Msg("=============SIM: CLIENT %5d + %d, SERVER %5d + %d\t REM: %.2f\n", cl.GetClientTickCount(), numticks, sv.m_nTickCount, numticks, host_remainder*1000.0f );
			}
#endif
		}
#ifndef SWDS
		else
		{
			static int numticks_last_frame = 0;
			static float host_remainder_last_frame = 0, prev_remainder_last_frame = 0, last_frame_time = 0;

			int clientticks;
			int serverticks;

			clientticks = numticks_last_frame;
			cl.m_tickRemainder = host_remainder_last_frame;
			cl.SetFrameTime( last_frame_time );
			if ( g_ClientDLL )
			{
				g_ClientDLL->IN_SetSampleTime(last_frame_time);
			}

			last_frame_time = host_frametime;

			serverticks = numticks;
			g_ClientGlobalVariables.simTicksThisFrame = clientticks;
			g_ServerGlobalVariables.simTicksThisFrame = serverticks;
			g_ServerGlobalVariables.tickcount = sv.m_nTickCount;

			// THREADED: Run Client
			// -------------------
			for ( int tick = 0; tick < clientticks; tick++ )
			{ 
				// process any asynchronous network traffic (TCP), set net_time
				NET_RunFrame(  Plat_FloatTime() );

				// Only send updates on final tick so we don't re-encode network data multiple times per frame unnecessarily
				bool bFinalTick = ( tick == (clientticks - 1) );

				// initialize networking for dedicated server after commandline & autoexec.cfg have been parsed
				if ( NET_IsDedicated() && !NET_IsMultiplayer() )
					NET_SetMutiplayer( true );
				g_ClientGlobalVariables.tickcount = cl.GetClientTickCount();

				// Make sure state is correct
				CL_CheckClientState();
				// Additional networking ops for SPLITPACKET stuff (99.9% of the time this will be an empty list of work)
				NET_SendQueuedPackets();
				//-------------------
				//
				// client operations
				//
				//-------------------
				if ( !sv.IsDedicated() )
				{
					_Host_RunFrame_Client( bFinalTick );
				}
				toolframework->Think( bFinalTick );
			}
			// This is a hack to let timedemo pull messages from the queue faster than every 15 msec
			// Also when demoplayer is skipping packets to a certain tick we should process the queue
			// as quickly as we can.
			if ( clientticks == 0 && ( demoplayer->IsPlayingTimeDemo() || demoplayer->IsSkipping() ) )
			{
				_Host_RunFrame_Client( true );
			}

			// This causes cl.gettime() to return the true clock being used for rendering (tickcount * rate + remainder)
			Host_SetClientInSimulation( false );
			// Now allow for interpolation on client
			g_ClientGlobalVariables.interpolation_amount = ( cl.m_tickRemainder / host_state.interval_per_tick );

			//-------------------
			// Run prediction if it hasn't been run yet
			//-------------------
			// If we haven't predicted/simulated the player (multiplayer with prediction enabled and
			//  not a listen server with zero frame lag, then go ahead and predict now
			CL_RunPrediction( PREDICTION_NORMAL );

			CL_ApplyAddAngle();

			Host_SetClientInSimulation( true );

			// THREADED: Run Input
			// -------------------
			int saveTick = g_ClientGlobalVariables.tickcount;

			for ( int tick = 0; tick < serverticks; tick++ )
			{
				// NOTE:  Do we want do this at start or end of this loop?
				++host_tickcount;
				++host_currentframetick;
				g_ClientGlobalVariables.tickcount = host_tickcount;
				bool bFinalTick = tick==(serverticks-1) ? true : false;
				_Host_RunFrame_Input( prevremainder, bFinalTick );
				prevremainder = 0;
				// process any asynchronous network traffic (TCP), set net_time
				NET_RunFrame(  Plat_FloatTime() );
			}

			Host_SetClientInSimulation( false );

			// The mouse is always simulated for the current frame's time
			// This makes updates smooth in every case
			// continuous controllers affecting the view are also simulated this way
			// but they have a cap applied by IN_SetSampleTime() so they are not also
			// simulated during input gathering
			CL_ExtraMouseUpdate( g_ClientGlobalVariables.frametime );

			g_ClientGlobalVariables.tickcount = saveTick;
			numticks_last_frame = numticks;
			host_remainder_last_frame = host_remainder;

			// THREADED: Run Server
			// -------------------
			// set net_time once before running the server
			NET_SetTime( Plat_FloatTime() );
			pGameJob = new CFunctorJob( CreateFunctor( _Host_RunFrame_Server_Async, serverticks ) );
			if ( IsX360() )
			{
				pGameJob->SetServiceThread( g_nServerThread );
			}
			g_pThreadPool->AddJob( pGameJob );
#if LOG_FRAME_OUTPUT
			if ( !cl.IsPaused() || !sv.IsPaused() )
			{
				Msg("=============SIM: CLIENT %5d + %d, SERVER %5d + %d\t REM: %.2f\n", cl.GetClientTickCount(), clientticks, sv.m_nTickCount, serverticks, host_remainder*1000.0f );
			}
#endif
		}
#endif	// SWDS

		g_Log.RunFrame();

		if ( shouldrender )
		{
#if LOG_FRAME_OUTPUT
			if ( !cl.IsPaused() || !sv.IsPaused() )
			{
				static float lastFrameTime = 0;
				float frametime = g_ClientGlobalVariables.curtime - lastFrameTime;
				Msg("RENDER AT: %6.4f: %.2fms [%.2fms implicit] frametime\n", 
					g_ClientGlobalVariables.curtime, g_ClientGlobalVariables.frametime*1000.0f, frametime * 1000.0f);
				lastFrameTime = g_ClientGlobalVariables.curtime;
			}
#endif
			//-------------------
			// rendering
			//-------------------
			_Host_RunFrame_Render();

			//-------------------
			// sound
			//-------------------
			_Host_RunFrame_Sound();

			if ( g_bVCRSingleStep )
			{
				VCR_EnterPausedState();
			}
		}
		else
		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "modelloader->UpdateDynamicModels" );
			VPROF( "UpdateDynamicModels" );
			CMDLCacheCriticalSection critsec( g_pMDLCache );
			modelloader->UpdateDynamicModels();
		}

		//-------------------
		// simulation
		//-------------------
		g_HostTimes.MarkSwapTime( );
#ifndef SWDS
		if ( !sv.IsDedicated() )
		{
			VPROF( "_Host_RunFrame - ClientDLL_Update" );
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "_Host_RunFrame - ClientDLL_Update" );

			// Client-side simulation
			g_HostTimes.StartFrameSegment( FRAME_SEGMENT_CLDLL );

			ClientDLL_Update();

			g_HostTimes.EndFrameSegment( FRAME_SEGMENT_CLDLL );
		}
#endif
		if ( pGameJob )
		{
			{
				VPROF_BUDGET( "WaitForAsyncServer", "AsyncServer" );
				if ( Host_IsSinglePlayerGame() )
				{
					// This should change to a YieldWait if the server starts wanting to parallel process. If
					// so, will need some route for the server to queue up work it wants to execute outside
					// its frame, otherwise some of it would be performed during the yield. Right now
					// need to wait for server so we don't stall on queued AI operations (toml 7/3/2007)
					pGameJob->ExecuteAndRelease();
				}
				else
				{
					pGameJob->WaitForFinishAndRelease();
				}
			}
			SV_FrameExecuteThreadDeferred();
		}

		//-------------------
		// time
		//-------------------

		Host_Speeds();

		Host_UpdateMapList();

		host_framecount++;
#if !defined(SWDS)
		if ( !demoplayer->IsPlaybackPaused() )
#endif
		{
			host_time = host_tickcount * host_state.interval_per_tick + cl.m_tickRemainder;
		}

		Host_PostFrameRate( host_frametime );

		if ( host_checkheap )
		{
#ifdef _WIN32
			if ( _heapchk() != _HEAPOK )
			{
				Sys_Error( "_Host_RunFrame (bottom):  _heapchk() != _HEAPOK\n" );
			}
#endif
		}

		Host_CheckDumpMemoryStats();

		GetTestScriptMgr()->CheckPoint( "frame_end" );
	} // Profile scope, protect from setjmp() problems

	Host_ShowIPCCallCount();
}
/*
==============================
Host_Frame

==============================
*/
void Host_RunFrame( float time )
{
	static  double	timetotal = 0;
	static  int		timecount = 0;
	static	double  timestart = 0;

#ifndef SWDS
	if ( !scr_drawloading && sv.IsActive() && cl.IsActive() && !sv.m_bLoadgame)
	{
		switch ( host_thread_mode.GetInt() )
		{
		case HTM_DISABLED:	g_bThreadedEngine = false;									break;
		case HTM_DEFAULT:	g_bThreadedEngine = ( g_pThreadPool->NumThreads() > 0 );	break;
		case HTM_FORCED:	g_bThreadedEngine = true;									break;
		}
	}
	else
#endif
	{
		g_bThreadedEngine = false;
	}

	if ( !host_profile.GetBool() )
	{
		_Host_RunFrame( time );
		return;
	}

	double	time1 = Sys_FloatTime();

	_Host_RunFrame( time );

	double	time2 = Sys_FloatTime();

	timetotal += time2 - time1; // time in seconds
	timecount++;

	if (timecount < 1000)
		return;

	float fps = 1000/(time2 - timestart);

	ConMsg ("host_profile : %i clients, %.1f msec, %.1f fps\n",  
		sv.GetNumClients(),  timetotal, fps );

	timecount = 0;
	timetotal = 0;
	timestart = time2;
}


//-----------------------------------------------------------------------------
// A more secure means of enforcing low violence.
//-----------------------------------------------------------------------------
bool IsLowViolence_Secure()
{
#ifndef DEDICATED
	if ( !IsX360() && Steam3Client().SteamApps() )
	{
		// let Steam determine current violence settings 		
		return Steam3Client().SteamApps()->BIsLowViolence();
	}
	else if ( IsX360() )
	{
		// Low violence for the 360 is enabled by the presence of a file.
		if ( g_pFileSystem->FileExists( "cfg/violence.cfg" ) )
		{
			return true;
		}
		
		return false;
	}
#endif
		
	return false;
}


//-----------------------------------------------------------------------------
// If "User Token 2" exists in HKEY_CURRENT_USER/Software/Valve/Half-Life/Settings
// then we disable gore. Obviously not very secure.
//-----------------------------------------------------------------------------
bool IsLowViolence_Registry()
{
	char szSubKey[128];
	int nBufferLen;
	char szBuffer[128];
	bool bReducedGore = false;

	memset( szBuffer, 0, 128 );

	char const *appname = "Source";
	Q_snprintf(szSubKey, sizeof( szSubKey ), "Software\\Valve\\%s\\Settings", appname );

	nBufferLen = 127;
	Q_strncpy( szBuffer, "", sizeof( szBuffer ) );

	Sys_GetRegKeyValue( szSubKey, "User Token 2", szBuffer,	nBufferLen, szBuffer );

	// Gore reduction active?
	bReducedGore = ( Q_strlen( szBuffer ) > 0 ) ? true : false;
	if ( !bReducedGore )
	{
		Sys_GetRegKeyValue( szSubKey, "User Token 3", szBuffer, nBufferLen, szBuffer );

		bReducedGore = ( Q_strlen( szBuffer ) > 0 ) ? true : false;
	}

	char gamedir[MAX_OSPATH];
	Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );

	// also check mod specific directories for LV changes
	Q_snprintf(szSubKey, sizeof( szSubKey ), "Software\\Valve\\%s\\%s\\Settings", appname, gamedir );

	nBufferLen = 127;
	Q_strncpy( szBuffer, "", sizeof( szBuffer ) );

	Sys_GetRegKeyValue( szSubKey, "User Token 2", szBuffer,	nBufferLen, szBuffer );
	if ( Q_strlen( szBuffer ) > 0 )
	{
		bReducedGore = true;
	}

	Sys_GetRegKeyValue( szSubKey, "User Token 3", szBuffer,	nBufferLen, szBuffer );
	if ( Q_strlen( szBuffer ) > 0 )
	{
		bReducedGore = true;
	}
	
	return bReducedGore;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Host_CheckGore( void )
{
	bool bLowViolenceRegistry = false;
	bool bLowViolenceSecure = false;

	//
	// First check the old method of enabling low violence via the registry.
	//
#ifdef WIN32
	bLowViolenceRegistry = IsLowViolence_Registry();
#endif
	//
	// Next check the new method of enabling low violence based on country of purchase
	// and other means that are inaccessible by the user.
	//
	if ( GetCurrentMod() && Q_stricmp( GetCurrentMod(), "cstrike" ) != 0 )
		bLowViolenceSecure = IsLowViolence_Secure();

	//
	// If either method says "yes" to low violence, we're in low violence mode.
	//
	if ( bLowViolenceRegistry || bLowViolenceSecure )
	{
		g_bLowViolence = true;
		
		if ( bLowViolenceRegistry )
		{
			violence_hblood.SetValue( 0 );
			violence_hgibs.SetValue( 0 );
			violence_ablood.SetValue( 0 );
			violence_agibs.SetValue( 0 );
		}
	}
	else
	{
		g_bLowViolence = false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Host_InitProcessor( void )
{
	const CPUInformation& pi = *GetCPUInformation();

	// Compute Frequency in Mhz: 
	char* szFrequencyDenomination = "Mhz";
	double fFrequency = pi.m_Speed / 1000000.0;

	// Adjust to Ghz if nessecary:
	if( fFrequency > 1000.0 )
	{
		fFrequency /= 1000.0;
		szFrequencyDenomination = "Ghz";
	}

	char szFeatureString[256];
	Q_strncpy( szFeatureString, pi.m_szProcessorID, sizeof( szFeatureString ) );
	Q_strncat( szFeatureString, " ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );

	if( pi.m_bSSE )
	{
		if( MathLib_SSEEnabled() ) Q_strncat(szFeatureString, "SSE ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
		else					   Q_strncat(szFeatureString, "(SSE) ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
	}

	if( pi.m_bSSE2 )
	{
		if( MathLib_SSE2Enabled() ) Q_strncat(szFeatureString, "SSE2 ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
		else					   Q_strncat(szFeatureString, "(SSE2) ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
	}

	if( pi.m_bMMX )
	{
		if( MathLib_MMXEnabled() ) Q_strncat(szFeatureString, "MMX ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
		else					   Q_strncat(szFeatureString, "(MMX) ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
	}

	if( pi.m_b3DNow )
	{
		if( MathLib_3DNowEnabled() ) Q_strncat(szFeatureString, "3DNow ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
		else					   Q_strncat(szFeatureString, "(3DNow) ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
	}

	if( pi.m_bRDTSC )	Q_strncat(szFeatureString, "RDTSC ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
	if( pi.m_bCMOV )	Q_strncat(szFeatureString, "CMOV ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );
	if( pi.m_bFCMOV )	Q_strncat(szFeatureString, "FCMOV ", sizeof( szFeatureString ), COPY_ALL_CHARACTERS );

	// Remove the trailing space.  There will always be one.
	szFeatureString[Q_strlen(szFeatureString)-1] = '\0';

	// Dump CPU information:
	if( pi.m_nLogicalProcessors == 1 )
	{
		ConDMsg( "1 CPU, Frequency: %.01f %s,  Features: %s\n", 
			fFrequency,
			szFrequencyDenomination,
			szFeatureString
			);
	} 
	else
	{
		char buffer[256] = "";
		if( pi.m_nPhysicalProcessors != pi.m_nLogicalProcessors )
		{
			Q_snprintf(buffer, sizeof( buffer ), " (%i physical)", (int) pi.m_nPhysicalProcessors );
		}

		ConDMsg( "%i CPUs%s, Frequency: %.01f %s,  Features: %s\n", 
			(int)pi.m_nLogicalProcessors,
			buffer,
			fFrequency,
			szFrequencyDenomination,
			szFeatureString
			);
	}

#if defined( _WIN32 )
	if ( s_bInitPME )
	{
		// Initialize the performance monitoring events code.
		InitPME();
	}
#endif
}

//-----------------------------------------------------------------------------
// Specifically used by the model loading code to mark models
// touched by the current map
//-----------------------------------------------------------------------------
int Host_GetServerCount( void )
{
	if (cl.m_nSignonState >= SIGNONSTATE_NEW)
	{
		// the server count cannot be relied on until the server info message
		// the new state guarantees its validity
		return cl.m_nServerCount;
	}
	else if (sv.m_State >= ss_loading)
	{
		return sv.GetSpawnCount();
	}

	// this is unfortunate, and happens, but the caller is too early in the protocol or a demo
	// cannot identify the correct server count
	// return the same count that demo will use
	return gHostSpawnCount;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Host_PostInit()
{
	if ( serverGameDLL )
	{
		serverGameDLL->PostInit();
	}

#if !defined( SWDS )
	if ( g_ClientDLL )
	{
		g_ClientDLL->PostInit();
	}

	toolframework->PostInit();

	if ( !sv.IsDedicated() )
	{
		// vgui needs other systems to finalize
		EngineVGui()->PostInit();
	}

#if defined( LINUX )
	const char en_US[] = "en_US.UTF-8";
	const char *CurrentLocale = setlocale( LC_ALL, NULL );
	if ( !CurrentLocale )
		CurrentLocale = "c";
	if ( Q_stricmp( CurrentLocale, en_US ) )
	{
		char MessageText[ 512 ];

		V_sprintf_safe( MessageText, "SetLocale('%s') failed. Using '%s'.\n"
									 "You may have limited glyph support.\n"
									 "Please install '%s' locale.",
						en_US, CurrentLocale, en_US );
		SDL_ShowSimpleMessageBox( 0, "Warning", MessageText, GetAssertDialogParent() );
	}
#endif // LINUX

#endif
}

void HLTV_Init()
{
	Assert ( hltv == NULL );
	Assert ( hltvtest == NULL );
}

void HLTV_Shutdown()
{
	if ( hltv )
	{
		hltv->Shutdown();
		delete hltv;
		hltv = NULL;
	}

	if ( hltvtest )
	{
		delete hltvtest;
		hltvtest = NULL;
	}
}

// Check with steam to see if the requested file (requires full path) is a valid, signed binary
bool DLL_LOCAL Host_IsValidSignature( const char *pFilename, bool bAllowUnknown )
{
#if defined( SWDS ) || defined(_X360)
	return true;
#else
	if ( sv.IsDedicated() || IsOSX() || IsLinux() )
	{
		// dedicated servers and Mac and Linux  binaries don't check signatures
		return true;
	}
	else
	{
		if ( Steam3Client().SteamUtils() )
		{
			SteamAPICall_t hAPICall = Steam3Client().SteamUtils()->CheckFileSignature( pFilename );
			bool bAPICallFailed = true;
			while ( !Steam3Client().SteamUtils()->IsAPICallCompleted(hAPICall, &bAPICallFailed) )
			{
				SteamAPI_RunCallbacks();
				ThreadSleep( 1 );
			}

			if( bAPICallFailed )
			{
				Warning( "CheckFileSignature API call on %s failed", pFilename );
			}
			else
			{
				CheckFileSignature_t result;
				Steam3Client().SteamUtils()->GetAPICallResult( hAPICall, &result, sizeof(result), result.k_iCallback, &bAPICallFailed );
				if( bAPICallFailed )
				{
					Warning( "CheckFileSignature API call on %s failed\n", pFilename );
				}
				else
				{
					if( result.m_eCheckFileSignature == k_ECheckFileSignatureValidSignature || result.m_eCheckFileSignature == k_ECheckFileSignatureNoSignaturesFoundForThisApp )
						return true;
					if ( bAllowUnknown && result.m_eCheckFileSignature == k_ECheckFileSignatureNoSignaturesFoundForThisFile )
						return true;
					Warning( "No valid signature found for %s\n", pFilename );
				}
			}
		}
	}

	return false;
#endif // SWDS
}


// Ask steam if it is ok to load this DLL.  Unsigned DLLs should not be loaded unless
// the client is running -insecure (testing a plugin for example)
// This keeps legitimate users with modified binaries from getting VAC banned because of them
bool DLL_LOCAL Host_AllowLoadModule( const char *pFilename, const char *pPathID, bool bAllowUnknown, bool bIsServerOnly /* = false */ )
{
#if defined( SWDS ) || defined ( OSX ) || defined( LINUX )
	// dedicated servers and Mac and Linux binaries don't check signatures
	return true;
#else

	if ( sv.IsDedicated() || bIsServerOnly )
	{
		// dedicated servers and Mac binaries don't check signatures
		return true;
	}
	else
	{
		// check signature
		bool bSignatureIsValid = false;

		// Do we need to do the signature checking? If secure servers are disabled, just skip it.
		if ( Host_IsSecureServerAllowed() )
		{
			if ( Steam3Client().SteamUtils() )
			{
				char szDllname[512];

				V_strncpy( szDllname, pFilename, sizeof(szDllname) );
				V_SetExtension( szDllname, g_pModuleExtension, sizeof(szDllname) );
				if ( pPathID )
				{
					char szFullPath[ 512 ];
					const char *pFullPath = g_pFileSystem->RelativePathToFullPath( szDllname, pPathID, szFullPath, sizeof(szFullPath) );
					if ( !pFullPath )
					{
						Warning("Can't find %s on disk\n", szDllname );
						bSignatureIsValid = false;
					}
					else
					{
						bSignatureIsValid = Host_IsValidSignature( pFullPath, bAllowUnknown );
					}
				}
				else
				{
					bSignatureIsValid = Host_IsValidSignature( szDllname, bAllowUnknown );
				}
			}
			else
			{
				Warning("Steam is not active, running in -insecure mode.\n");
				Host_DisallowSecureServers();
			}
		}
		else
		{
			Warning("Loading unsigned module %s\nAccess to secure servers is disabled.\n", pFilename );
			return true;
		}

		return bSignatureIsValid;
	}
#endif // SWDS
}

bool DLL_LOCAL Host_IsSecureServerAllowed()
{
	if ( CommandLine()->FindParm( "-insecure" ) || CommandLine()->FindParm( "-textmode" ) )
		g_bAllowSecureServers = false;

	return g_bAllowSecureServers;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Host_Init( bool bDedicated )
{
	realtime = 0;
	host_idealtime = 0;

#if defined(_WIN32)
	if ( CommandLine()->FindParm( "-pme" ) )
	{
		s_bInitPME = true;
	}
#endif

	if ( Host_IsSecureServerAllowed() )
	{
		// double check the engine's signature in case it was hooked/modified
		if ( !Host_AllowLoadModule( "engine" DLL_EXT_STRING, "EXECUTABLE_PATH", false, bDedicated ) )
		{
			// not supposed to load this but we will anyway
			Host_DisallowSecureServers();
		}
	}

	ThreadPoolStartParams_t startParams;
	if ( IsX360() )
	{
		// 360 overrides defaults, 2 computation threads distributed to core 1 and 2
		startParams.nThreads = 2;
		startParams.nStackSize = 256*1024;
		startParams.fDistribute = TRS_TRUE;
		startParams.bUseAffinityTable = true;
		startParams.iAffinityTable[0] = XBOX_PROCESSOR_2;
		startParams.iAffinityTable[1] = XBOX_PROCESSOR_4;
		ThreadSetAffinity( NULL, 1 );
	}
	if ( g_pThreadPool )
		g_pThreadPool->Start( startParams, "CmpJob" );

	// From const.h, the loaded game .dll will give us the correct value which is transmitted to the client
	host_state.interval_per_tick = DEFAULT_TICK_INTERVAL;

	InstallBitBufErrorHandler();

	TRACEINIT( Memory_Init(), Memory_Shutdown() );

	TRACEINIT( Con_Init(), Con_Shutdown() );

	TRACEINIT( Cbuf_Init(), Cbuf_Shutdown() );

	TRACEINIT( Cmd_Init(), Cmd_Shutdown() );	

	TRACEINIT( g_pCVar->Init(), g_pCVar->Shutdown() ); // So we can list cvars with "cvarlst"

#ifndef SWDS
	TRACEINIT( V_Init(), V_Shutdown() );
#endif

	TRACEINIT( COM_Init(), COM_Shutdown() );


#ifndef SWDS
	TRACEINIT( saverestore->Init(), saverestore->Shutdown() );
#endif

	TRACEINIT( Filter_Init(), Filter_Shutdown() );

#ifndef SWDS
	TRACEINIT( Key_Init(), Key_Shutdown() );
#endif

	// Check for special -dev flag
	if ( CommandLine()->FindParm( "-dev" ) || ( CommandLine()->FindParm( "-allowdebug" ) && !CommandLine()->FindParm( "-nodev" ) ) )
	{
		sv_cheats.SetValue( 1 );
		developer.SetValue( 1 );
	}
#ifdef _DEBUG
	developer.SetValue( 1 );
#endif

	// Should have read info from steam.inf by now.
	Assert( GetSteamInfIDVersionInfo().AppID != k_uAppIdInvalid );

	if ( CommandLine()->FindParm( "-nocrashdialog" ) )
	{
		// stop the various windows error message boxes from showing up (used by the auto-builder so it doesn't block on error) 
		Sys_NoCrashDialog();
	}

	TRACEINIT( NET_Init( bDedicated ), NET_Shutdown() );     

	TRACEINIT( g_GameEventManager.Init(), g_GameEventManager.Shutdown() );

	TRACEINIT( sv.Init( bDedicated ), sv.Shutdown() );

#if defined( REPLAY_ENABLED )
	if ( Replay_IsSupportedModAndPlatform() )
	{
		TRACEINIT( ReplaySystem_Init( bDedicated ), ReplaySystem_Shutdown() );
	}
#endif

	if ( !CommandLine()->FindParm( "-nogamedll" ) )
	{
		SV_InitGameDLL();
	}

	TRACEINIT( g_Log.Init(), g_Log.Shutdown() );

	TRACEINIT( HLTV_Init(), HLTV_Shutdown() );

	ConDMsg( "Heap: %5.2f Mb\n", host_parms.memsize/(1024.0f*1024.0f) );

#if !defined( SWDS )
	if ( !bDedicated )
	{
		TRACEINIT( CL_Init(), CL_Shutdown() );

		// NOTE: This depends on the mod search path being set up
		TRACEINIT( InitMaterialSystem(), ShutdownMaterialSystem() );

		TRACEINIT( modelloader->Init(), modelloader->Shutdown() );

		TRACEINIT( StaticPropMgr()->Init(), StaticPropMgr()->Shutdown() );

		TRACEINIT( InitStudioRender(), ShutdownStudioRender() );

		//startup vgui
		TRACEINIT( EngineVGui()->Init(), EngineVGui()->Shutdown() );

		TRACEINIT( TextMessageInit(), TextMessageShutdown() );

		TRACEINIT( ClientDLL_Init(), ClientDLL_Shutdown() );

		TRACEINIT( SCR_Init(), SCR_Shutdown() );

		TRACEINIT( R_Init(), R_Shutdown() ); 

		TRACEINIT( Decal_Init(), Decal_Shutdown() );

		// hookup interfaces
		EngineVGui()->Connect();
	}
	else
#endif
	{
		TRACEINIT( InitMaterialSystem(), ShutdownMaterialSystem() );

		TRACEINIT( modelloader->Init(), modelloader->Shutdown() );

		TRACEINIT( StaticPropMgr()->Init(), StaticPropMgr()->Shutdown() );

		TRACEINIT( InitStudioRender(), ShutdownStudioRender() );

		TRACEINIT( Decal_Init(), Decal_Shutdown() );

		cl.m_nSignonState = SIGNONSTATE_NONE; // disable client
	}

#ifndef SWDS
	Host_ReadConfiguration();
	TRACEINIT( S_Init(), S_Shutdown() );
#endif

	// Execute valve.rc
	Cbuf_AddText( "exec valve.rc\n" );

#if defined( REPLAY_ENABLED )
	// Execute replay.cfg if this is TF and they want to use the replay system
	if ( Replay_IsSupportedModAndPlatform() && CommandLine()->CheckParm( "-replay" ) )
	{
		const char *pConfigName = CommandLine()->ParmValue( "-replay", "replay.cfg" );
		Cbuf_AddText( va( "exec %s\n", pConfigName ) );
	}
#endif

	// Execute mod-specfic settings, without falling back based on search path.
	// This lets us set overrides for games while letting mods of those games
	// use the default settings.
	if ( g_pFileSystem->FileExists( "//mod/cfg/modsettings.cfg" ) )
	{
		Cbuf_AddText( "exec modsettings.cfg mod\n" );
	}

	// Mark DLL as active
	//	eng->SetNextState( InEditMode() ? IEngine::DLL_PAUSED : IEngine::DLL_ACTIVE );

	// Deal with Gore Settings
	Host_CheckGore();

	TelemetryTick();

	// Initialize processor subsystem, and print relevant information:
	Host_InitProcessor();

	// Mark hunklevel at end of startup
	Hunk_AllocName( 0, "-HOST_HUNKLEVEL-" );
	host_hunklevel = Hunk_LowMark();

#ifdef SOURCE_MT
	if ( CommandLine()->FindParm( "-swapcores" ) )
	{
		g_nMaterialSystemThread = 1;
		g_nServerThread = 0;
	}
#endif

	Host_AllowQueuedMaterialSystem( false );

	// Finished initializing
	host_initialized = true;

	host_checkheap = CommandLine()->FindParm( "-heapcheck" ) ? true : false;

	if ( host_checkheap )
	{
#if defined( _WIN32 )
		if ( _heapchk() != _HEAPOK )
		{
			Sys_Error( "Host_Init:  _heapchk() != _HEAPOK\n" );
		}
#endif
	}

	// go directly to run state with no active game
	HostState_Init();

	// check for reslist generation
	if ( CommandLine()->FindParm( "-makereslists" ) )
	{
		MapReslistGenerator().StartReslistGeneration();
	}

	// check for devshot generation
	if ( CommandLine()->FindParm( "-makedevshots" ) )
	{
		DevShotGenerator().StartDevShotGeneration();
	}

	// if running outside of steam and NOT a dedicated server then phone home (or if "-phonehome" is passed on the command line)
	if ( !sv.IsDedicated() || CommandLine()->FindParm( "-phonehome" ) )
	{
		// In debug, only run this check if -phonehome is on the command line (so a debug build will "just work").
		if ( IsDebug() && CommandLine()->FindParm( "-phonehome" ) )
		{
			phonehome->Init();
			phonehome->Message( IPhoneHome::PHONE_MSG_ENGINESTART, NULL );
		}
	}

#ifndef SWDS
	// Rebuild audio caches
	if ( !sv.IsDedicated() && S_IsInitted() )
	{
		if ( !MapReslistGenerator().IsEnabled() )
		{
			// only build caches if we aren't' generating reslists (you need reslists to make the caches)
			extern void CheckCacheBuild();
			CheckCacheBuild();
		}
	}
#endif

	Host_PostInit();
	EndLoadingUpdates( );
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetNonInteractiveTempFullscreenBuffer( NULL, MATERIAL_NON_INTERACTIVE_MODE_STARTUP );
	pRenderContext->SetNonInteractivePacifierTexture( NULL, 0, 0, 0 );
}

//-----------------------------------------------------------------------------
// Adds hints to the loader to keep resources that are in the transition volume,
// as they may not be part of the next map's reslist.
//-----------------------------------------------------------------------------
void AddTransitionResources( CSaveRestoreData *pSaveData, const char *pLevelName, const char *pLandmarkName )
{
	if ( !IsX360() || ( g_pFileSystem->GetDVDMode() != DVDMODE_STRICT ) )
	{
		return;
	}

	// get the bit marked for the next level
	int transitionMask = 0;
	for ( int i = 0; i < pSaveData->levelInfo.connectionCount; i++ )
	{
		if ( !Q_stricmp( pLevelName, pSaveData->levelInfo.levelList[i].mapName ) && !Q_stricmp( pLandmarkName, pSaveData->levelInfo.levelList[i].landmarkName ) )
		{
			transitionMask = 1<<i;
			break;
		}
	}
	
	if ( !transitionMask )
	{
		// nothing to do
		return;
	}

	const char *pModelName;
	bool bHasHumans = false;
	for ( int i = 0; i < pSaveData->NumEntities(); i++ )
	{
		if ( pSaveData->GetEntityInfo(i)->flags & transitionMask )
		{
			// this entity will cross the transition and needs to be preserved
			// add to the next map's resource list which effectively keeps it from being purged
			// only care about the actual mdl and not any of its dependants
			pModelName = pSaveData->GetEntityInfo(i)->modelname.ToCStr();
			g_pQueuedLoader->AddMapResource( pModelName );

			// humans require a post pass
			if ( !bHasHumans && V_stristr( pModelName, "models/humans" ) )
			{
				bHasHumans = true;
			}
		}
	}

	if ( bHasHumans )
	{
		// the presence of any human entity in the transition needs to ensure all the human mdls stay
		int count = modelloader->GetCount();
		for ( int i = 0; i < count; i++ )
		{
			pModelName = modelloader->GetName( modelloader->GetModelForIndex( i ) );
			if ( V_stristr( pModelName, "models/humans" ) )
			{
				g_pQueuedLoader->AddMapResource( pModelName );
			}
		}
	}
}

bool Host_Changelevel( bool loadfromsavedgame, const char *mapname, const char *start )
{
	char			_startspot[MAX_QPATH];
	char			*startspot;
	char			oldlevel[MAX_PATH];
#if !defined(SWDS)
	CSaveRestoreData *pSaveData = NULL;
#endif
	bool bTransitionBySave = false;

	if ( !sv.IsActive() )
	{
		ConMsg("Only the server may changelevel\n");
		return false;
	}

#ifndef SWDS
	// FIXME:  Even needed?
	if ( demoplayer->IsPlayingBack() )
	{
		ConMsg("Changelevel invalid during demo playback\n");
		SCR_EndLoadingPlaque();
		return false;
	}

#endif

#ifndef SWDS
	SCR_BeginLoadingPlaque();

	// stop sounds (especially looping!)
	S_StopAllSounds(true);
#endif

	// Prepare new level
	sv.InactivateClients();

	// The qualified name of the map, excluding path/extension
	char szMapName[MAX_PATH] = { 0 };
	// The file to load the map from.
	char szMapFile[MAX_PATH] = { 0 };
	Q_strncpy( szMapName, mapname, sizeof( szMapName ) );
	Host_DefaultMapFileName( szMapName, szMapFile, sizeof( szMapFile ) );

	// Ask serverDLL to prepare this load
	if ( g_iServerGameDLLVersion >= 10 )
	{
		serverGameDLL->PrepareLevelResources( szMapName, sizeof( szMapName ), szMapFile, sizeof( szMapFile ) );
	}

	if ( !modelloader->Map_IsValid( szMapFile ) )
	{
#ifndef SWDS
		SCR_EndLoadingPlaque();
#endif
		// We have already inactivated clients at this point due to PrepareLevelResources being blocking, false alarm,
		// tell them to reconnect (which doesn't mean full reconnect, just start rejoining the map)
		//
		// In the likely case that the game DLL tries another map this is harmless, they'll wait on the game server in
		// the connect process if its in another level change by time they get there.
		sv.ReconnectClients();
		return false;
	}

	// If changing from the same map to the same map, optimize by not closing and reopening
	// the packfile which is embedded in the .bsp; we do this by incrementing the packfile's
	// refcount via BeginMapAccess()/EndMapAccess() through the base filesystem API.
	struct LocalMapAccessScope
	{
		LocalMapAccessScope() : bEnabled( false ) { }
		~LocalMapAccessScope() { if ( bEnabled ) g_pFileSystem->EndMapAccess(); }
		bool bEnabled;
	};

	LocalMapAccessScope mapscope;
	if ( V_strcmp( sv.GetMapName(), szMapName ) == 0 )
	{
		g_pFileSystem->BeginMapAccess();
		mapscope.bEnabled = true;
	}

	g_pFileSystem->AsyncFinishAll();

	if ( !start )
		startspot = NULL;
	else
	{
		Q_strncpy (_startspot, start, sizeof( _startspot ) );
		startspot = _startspot;
	}

	Warning( "---- Host_Changelevel ----\n" );
	CheckForFlushMemory( sv.GetMapName(), szMapName );

#if !defined( SWDS )
	// Always save as an xsave if we're on the X360
	saverestore->SetIsXSave( IsX360() );

	// Add on time passed since the last time we kept track till this transition
	int iAdditionalSeconds = g_ServerGlobalVariables.curtime - saverestore->GetMostRecentElapsedTimeSet();
	int iElapsedSeconds = saverestore->GetMostRecentElapsedSeconds() + iAdditionalSeconds;
	int iElapsedMinutes = saverestore->GetMostRecentElapsedMinutes() + ( iElapsedSeconds / 60 );
	saverestore->SetMostRecentElapsedMinutes( iElapsedMinutes );
	saverestore->SetMostRecentElapsedSeconds( ( iElapsedSeconds % 60 ) );

	if ( bTransitionBySave )
	{
		char comment[80];
		// Pass in the total elapsed time so it gets added to the elapsed time for this map.
		serverGameDLL->GetSaveComment( 
			comment, 
			sizeof( comment ),
			saverestore->GetMostRecentElapsedMinutes(),
			saverestore->GetMostRecentElapsedSeconds() );

		if ( !saverestore->SaveGameSlot( "_transition", comment, false, true, szMapName, startspot ) )
		{
			Warning( "Failed to save data for transition\n" );
			SCR_EndLoadingPlaque();
			return false;
		}

		// Not going to load a save after the transition, so add this map's elapsed time to the total elapsed time
		int totalSeconds = g_ServerGlobalVariables.curtime + saverestore->GetMostRecentElapsedSeconds();
		saverestore->SetMostRecentElapsedMinutes( (int)( totalSeconds / 60.0f ) + saverestore->GetMostRecentElapsedMinutes() );
		saverestore->SetMostRecentElapsedSeconds( (int)fmod( totalSeconds, 60.0f ) );
	}
#endif

	Q_strncpy( oldlevel, sv.GetMapName(), sizeof( oldlevel ) );

#if !defined(SWDS)
	if ( loadfromsavedgame )
	{
		if ( !bTransitionBySave )
		{
			// save the current level's state
			saverestore->SaveGameState( true, &pSaveData );

			if ( !pSaveData )
			{
				Warning( "Failed to save data for transition\n" );
				SCR_EndLoadingPlaque();
				return false;
			}
		}

		// ensure resources in the transition volume stay
		AddTransitionResources( pSaveData, szMapName, startspot );
	}
#endif
	g_pServerPluginHandler->LevelShutdown();

#if !defined(SWDS)
	audiosourcecache->LevelShutdown();
#endif

#if !defined(SWDS)
	saverestore->FinishAsyncSave();
#endif

	if ( sv.RestartOnLevelChange() )
	{
		Cbuf_Clear();
		Cbuf_AddText( "quit\n" );
		return false;
	}

	DownloadListGenerator().OnLevelLoadStart( szMapName );

	if ( !sv.SpawnServer( szMapName, szMapFile, startspot ) )
	{
#ifndef SWDS
		SCR_EndLoadingPlaque();
#endif
		return false;
	}

#ifndef SWDS
	if ( loadfromsavedgame )
	{
		if ( !bTransitionBySave )
		{
			// Finish saving gamestate
			saverestore->Finish( pSaveData );
		}

		g_ServerGlobalVariables.curtime = sv.GetTime();

		audiosourcecache->LevelInit( szMapName );
		g_pServerPluginHandler->LevelInit( szMapName, CM_EntityString(), oldlevel, startspot, true, false );

		sv.SetPaused( true ); // pause until client connects
		sv.m_bLoadgame = true;
	}
	else
#endif
	{
		g_ServerGlobalVariables.curtime = sv.GetTime();
#if !defined(SWDS)
		audiosourcecache->LevelInit( szMapName );
#endif
		g_pServerPluginHandler->LevelInit( szMapName, CM_EntityString(), NULL, NULL, false, false );
	}

	SV_ActivateServer();

#if !defined(SWDS)
	// Offset stored elapsed time by the current elapsed time for this new map
	int maptime = sv.GetTime();
	int minutes = (int)( maptime / 60.0f );
	int seconds = (int)fmod( maptime, 60.0f );
	saverestore->SetMostRecentElapsedMinutes( saverestore->GetMostRecentElapsedMinutes() - minutes );
	saverestore->SetMostRecentElapsedSeconds( saverestore->GetMostRecentElapsedSeconds() - seconds );
#endif

	NotifyDedicatedServerUI("UpdateMap");

	DownloadListGenerator().OnLevelLoadEnd();

	return true;
}

/*
===============================================================================

SERVER TRANSITIONS

===============================================================================
*/
bool Host_NewGame( char *mapName, bool loadGame, bool bBackgroundLevel, const char *pszOldMap, const char *pszLandmark, bool bOldSave )
{
	VPROF( "Host_NewGame" );
	COM_TimestampedLog( "Host_NewGame" );

	char previousMapName[MAX_PATH] = { 0 };
	Q_strncpy( previousMapName, host_map.GetString(), sizeof( previousMapName ) );

#ifndef SWDS
	SCR_BeginLoadingPlaque();
#endif

	// The qualified name of the map, excluding path/extension
	char szMapName[MAX_PATH] = { 0 };
	// The file to load the map from.
	char szMapFile[MAX_PATH] = { 0 };
	Q_strncpy( szMapName, mapName, sizeof( szMapName ) );
	Host_DefaultMapFileName( szMapName, szMapFile, sizeof( szMapFile ) );

	// Steam may not have been started yet, ensure it is available to the game DLL before we ask it to prepare level
	// resources
	SV_InitGameServerSteam();

	// Ask serverDLL to prepare this load
	if ( g_iServerGameDLLVersion >= 10 )
	{
		serverGameDLL->PrepareLevelResources( szMapName, sizeof( szMapName ), szMapFile, sizeof( szMapFile ) );
	}

	if ( !modelloader->Map_IsValid( szMapFile ) )
	{
#ifndef SWDS
		SCR_EndLoadingPlaque();
#endif
		return false;
	}

	DevMsg( "---- Host_NewGame ----\n" );
	host_map.SetValue( szMapName );

	CheckForFlushMemory( previousMapName, szMapName );

	if (MapReslistGenerator().IsEnabled())
	{
		// uncache all the materials, so their files get referenced again for the reslists
		// undone for now, since we're just trying to get a global reslist, not per-map accurate
		//		materials->UncacheAllMaterials();
		MapReslistGenerator().OnLevelLoadStart(szMapName);
		// cache 'em back in!
		//		materials->CacheUsedMaterials();
	}
	DownloadListGenerator().OnLevelLoadStart(szMapName);

	if ( !loadGame )
	{
		VPROF( "Host_NewGame_HostState_RunGameInit" );
		HostState_RunGameInit();
	}

	// init network mode
	VPROF_SCOPE_BEGIN( "Host_NewGame_SpawnServer" );

	NET_SetMutiplayer( sv.IsMultiplayer() );

	NET_ListenSocket( sv.m_Socket, true );	// activated server TCP socket

	// let's not have any servers with no name
	if ( host_name.GetString()[0] == 0 )
	{
		host_name.SetValue( serverGameDLL->GetGameDescription() );
	}

	if ( !sv.SpawnServer ( szMapName, szMapFile, NULL ) )
	{
		return false;
	}

	sv.m_bIsLevelMainMenuBackground = bBackgroundLevel;

	VPROF_SCOPE_END();

	// make sure the time is set
	g_ServerGlobalVariables.curtime = sv.GetTime();

	COM_TimestampedLog( "serverGameDLL->LevelInit" );

#ifndef SWDS
	EngineVGui()->UpdateProgressBar(PROGRESS_LEVELINIT);

	audiosourcecache->LevelInit( szMapName );
#endif

	g_pServerPluginHandler->LevelInit( szMapName, CM_EntityString(), pszOldMap, pszLandmark, loadGame && !bOldSave, bBackgroundLevel );

	if ( loadGame && !bOldSave )
	{
		sv.SetPaused( true );		// pause until all clients connect
		sv.m_bLoadgame = true;
		g_ServerGlobalVariables.curtime = sv.GetTime();
	}

	if( !SV_ActivateServer() )
	{
		return false;
	}

	// Connect the local client when a "map" command is issued.
	if ( !sv.IsDedicated() )
	{
		COM_TimestampedLog( "Stuff 'connect localhost' to console" );

		char str[512];
		Q_snprintf( str, sizeof( str ), "connect localhost:%d listenserver", sv.GetUDPPort() );
		Cbuf_AddText( str );
	}
	else
	{
		// Dedicated server triggers map load here.
		GetTestScriptMgr()->CheckPoint( "FinishedMapLoad" );
	}

#ifndef SWDS
	if ( !loadGame || bOldSave )
	{
		// clear the most recent remember save, so the level will just restart if the player dies
		saverestore->ForgetRecentSave();
	}

	saverestore->SetMostRecentElapsedMinutes( 0 );
	saverestore->SetMostRecentElapsedSeconds( 0 );
#endif

	if (MapReslistGenerator().IsEnabled())
	{
		MapReslistGenerator().OnLevelLoadEnd();
	}
	DownloadListGenerator().OnLevelLoadEnd();
	return true;
}

void Host_FreeStateAndWorld( bool server )
{
	bool bNeedsPurge = false;

	Assert( host_initialized );
	Assert( host_hunklevel );

	// If called by the client and we are running a listen server, just ignore
	if ( !server && sv.IsActive() )
		return;

	// HACKHACK: You can't clear the hunk unless the client data is free
	// since this gets called by the server, it's necessary to wipe the client
	// in case we are on a listen server
#ifndef SWDS
	if ( server && !sv.IsDedicated() )
	{
		CL_ClearState();
	}
#endif

	// The world model relies on the low hunk, so we need to force it to unload
	if ( host_state.worldmodel )
	{
		modelloader->UnreferenceModel( host_state.worldmodel, IModelLoader::FMODELLOADER_SERVER );
		modelloader->UnreferenceModel( host_state.worldmodel, IModelLoader::FMODELLOADER_CLIENT );
		host_state.SetWorldModel( NULL );
		bNeedsPurge = server && true;
	}

	// Unload and reset dynamic models
	if ( server )
	{
		extern IVModelInfo* modelinfo;
		modelinfo->OnLevelChange();
	}
#ifndef SWDS
	else
	{
		extern IVModelInfoClient* modelinfoclient;
		modelinfoclient->OnLevelChange();
	}
#endif

	modelloader->UnloadUnreferencedModels();

	g_TimeLastMemTest = 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Host_FreeToLowMark( bool server )
{
	Assert( host_initialized );
	Assert( host_hunklevel );

	// If called by the client and we are running a listen server, just ignore
	if ( !server && sv.IsActive() )
		return;

	CM_FreeMap();

	if ( host_hunklevel )
	{
		// See if we are going to obliterate any malloc'd pointers
		Hunk_FreeToLowMark(host_hunklevel);
	}
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Host_Shutdown(void)
{
	extern void ShutdownMixerControls();

	if ( host_checkheap )
	{
#ifdef _WIN32
		if ( _heapchk() != _HEAPOK )
		{
			Sys_Error( "Host_Shutdown (top):  _heapchk() != _HEAPOK\n" );
		}
#endif
	}

	// Check for recursive shutdown, should never happen
	static bool shutting_down = false;
	if ( shutting_down )
	{
		Msg( "Recursive shutdown!!!\n" );
		return;
	}
	shutting_down = true;

	phonehome->Message( IPhoneHome::PHONE_MSG_ENGINEEND, NULL );
	phonehome->Shutdown();

#ifndef SWDS
	// Store active configuration settings
	Host_WriteConfiguration(); 
#endif

	// Disconnect from server
	Host_Disconnect(true);

#ifndef SWDS
	// keep ConMsg from trying to update the screen
	scr_disabled_for_loading = true;
#endif

#if defined VOICE_OVER_IP && !defined SWDS && !defined( NO_VOICE ) //!defined(_XBOX)
	Voice_Deinit();
#endif // VOICE_OVER_IP

	// TODO, Trace this
	CM_FreeMap();

	host_initialized = false;

#if defined(VPROF_ENABLED)
	VProfRecord_Shutdown();
#endif

#if !defined SWDS
	if ( !sv.IsDedicated() )
	{
		TRACESHUTDOWN( Decal_Shutdown() );

		TRACESHUTDOWN( R_Shutdown() );

		TRACESHUTDOWN( SCR_Shutdown() );

		TRACESHUTDOWN( S_Shutdown() );

		TRACESHUTDOWN( ClientDLL_Shutdown() );

		TRACESHUTDOWN( TextMessageShutdown() );

		TRACESHUTDOWN( EngineVGui()->Shutdown() );

		TRACESHUTDOWN( StaticPropMgr()->Shutdown() );

		// Model loader must shutdown before StudioRender
		// because it calls into StudioRender
		TRACESHUTDOWN( modelloader->Shutdown() );

		TRACESHUTDOWN( ShutdownStudioRender() );

		TRACESHUTDOWN( ShutdownMaterialSystem() );

		TRACESHUTDOWN( CL_Shutdown() );
	}
	else
#endif
	{
		TRACESHUTDOWN( Decal_Shutdown() );

		TRACESHUTDOWN( modelloader->Shutdown() );

		TRACESHUTDOWN( ShutdownStudioRender() );

		TRACESHUTDOWN( StaticPropMgr()->Shutdown() );

		TRACESHUTDOWN( ShutdownMaterialSystem() );
	}

#if defined( REPLAY_ENABLED )
	if ( Replay_IsSupportedModAndPlatform() )
	{
		TRACESHUTDOWN( ReplaySystem_Shutdown() );
	}
#endif

	TRACESHUTDOWN( HLTV_Shutdown() );

	TRACESHUTDOWN( g_Log.Shutdown() );
	
	TRACESHUTDOWN( g_GameEventManager.Shutdown() );

	TRACESHUTDOWN( sv.Shutdown() );

	TRACESHUTDOWN( NET_Shutdown() );

#ifndef SWDS
	TRACESHUTDOWN( Key_Shutdown() );
#ifndef _X360
	TRACESHUTDOWN( ShutdownMixerControls() );
#endif
#endif

	TRACESHUTDOWN( Filter_Shutdown() );

#ifndef SWDS
	TRACESHUTDOWN( saverestore->Shutdown() );
#endif

	TRACESHUTDOWN( COM_Shutdown() );

	// TRACESHUTDOWN( Host_ShutdownVCR() );
#ifndef SWDS
	TRACESHUTDOWN( V_Shutdown() );
#endif

	TRACESHUTDOWN( g_pCVar->Shutdown() );

	TRACESHUTDOWN( Cmd_Shutdown() );

	TRACESHUTDOWN( Cbuf_Shutdown() );

	TRACESHUTDOWN( Con_Shutdown() );

	TRACESHUTDOWN( Memory_Shutdown() );

	if ( g_pThreadPool )
		g_pThreadPool->Stop();

	DTI_Term();
	ServerDTI_Term();

#if defined(_WIN32)
	if ( s_bInitPME )
	{
		ShutdownPME();
	}
#endif

	if ( host_checkheap )
	{
#ifdef _WIN32
		if ( _heapchk() != _HEAPOK )
		{
			Sys_Error( "Host_Shutdown (bottom):  _heapchk() != _HEAPOK\n" );
		}
#endif
	}
}

//-----------------------------------------------------------------------------
// Centralize access to enabling QMS.
//-----------------------------------------------------------------------------
bool Host_AllowQueuedMaterialSystem( bool bAllow )
{
#if !defined DEDICATED
	// g_bAllowThreadedSound = bAllow;
	// NOTE: Moved this to materialsystem for integrating with other mqm changes
	return g_pMaterialSystem->AllowThreading( bAllow, g_nMaterialSystemThread );
#endif
	return false;
}
