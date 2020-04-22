//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#include "client_pch.h"
#include "sound.h"
#include <inetchannel.h>
#include "checksum_engine.h"
#include "con_nprint.h"
#include "r_local.h"
#include "gl_lightmap.h"
#include "console.h"
#include "traceinit.h"
#include "cl_demo.h"
#include "cdll_engine_int.h"
#include "debugoverlay.h"
#include "filesystem_engine.h"
#include "icliententity.h"
#include "dt_recv_eng.h"
#include "vgui_baseui_interface.h"
#include "testscriptmgr.h"
#include <tier0/vprof.h>
#include <proto_oob.h>
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "gl_matsysiface.h"
#include "staticpropmgr.h"
#include "ispatialpartitioninternal.h"
#include "cbenchmark.h"
#include "vox.h"
#include "LocalNetworkBackdoor.h"
#include <tier0/icommandline.h>
#include "GameEventManager.h"
#include "host_saverestore.h"
#include "ivideomode.h"
#include "host_phonehome.h"
#include "decal.h"
#include "sv_rcon.h"
#include "cl_rcon.h"
#include "vgui_baseui_interface.h"
#include "snd_audio_source.h"
#include "iregistry.h"
#include "sys.h"
#include <vstdlib/random.h>
#include "tier0/etwprof.h"
#include "tier0/vcrmode.h"
#include "sys_dll.h"
#include "video/ivideoservices.h"
#include "cl_steamauth.h"
#include "filesystem/IQueuedLoader.h"
#include "tier2/tier2.h"
#include "host_state.h"
#include "enginethreads.h"
#include "vgui/ISystem.h"
#include "pure_server.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "LoadScreenUpdate.h"
#include "tier0/systeminformation.h"
#include "steam/steam_api.h"
#include "SourceAppInfo.h"
#include "cl_steamauth.h"
#include "sv_steamauth.h"
#include "engine/ivmodelinfo.h"
#ifdef _X360
#include "xbox/xbox_launch.h"
#endif
#if defined( REPLAY_ENABLED )
#include "replay_internal.h"
#endif

#include "igame.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IVEngineClient *engineClient;

void R_UnloadSkys( void );
void CL_ResetEntityBits( void );
void EngineTool_UpdateScreenshot();
void WriteConfig_f( ConVar *var, const char *pOldString );

// If we get more than 250 messages in the incoming buffer queue, dump any above this #
#define MAX_INCOMING_MESSAGES		250
// Size of command send buffer
#define MAX_CMD_BUFFER				4000

CGlobalVarsBase g_ClientGlobalVariables( true );
IVideoRecorder *g_pVideoRecorder = NULL;

extern ConVar rcon_password;
extern ConVar host_framerate;
extern ConVar cl_clanid;

ConVar sv_unlockedchapters( "sv_unlockedchapters", "1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX, "Highest unlocked game chapter." );

static ConVar tv_nochat	( "tv_nochat", "0", FCVAR_ARCHIVE | FCVAR_USERINFO, "Don't receive chat messages from other SourceTV spectators" );
static ConVar cl_LocalNetworkBackdoor( "cl_localnetworkbackdoor", "1", 0, "Enable network optimizations for single player games." );
static ConVar cl_ignorepackets( "cl_ignorepackets", "0", FCVAR_CHEAT, "Force client to ignore packets (for debugging)." );
static ConVar cl_playback_screenshots( "cl_playback_screenshots", "0", 0, "Allows the client to playback screenshot and jpeg commands in demos." );

#if defined( STAGING_ONLY ) || defined( _DEBUG )
static ConVar cl_block_usercommand( "cl_block_usercommand", "0", FCVAR_CHEAT, "Force client to not send usercommand (for debugging)." );
#endif // STAGING_ONLY || _DEBUG

ConVar dev_loadtime_map_start( "dev_loadtime_map_start", "0.0", FCVAR_HIDDEN);
ConVar dev_loadtime_map_elapsed( "dev_loadtime_map_elapsed", "0.0", FCVAR_HIDDEN );

MovieInfo_t cl_movieinfo;

// FIXME: put these on hunk?
dlight_t		cl_dlights[MAX_DLIGHTS];
dlight_t		cl_elights[MAX_ELIGHTS];
CFastPointLeafNum g_DLightLeafAccessors[MAX_DLIGHTS];
CFastPointLeafNum g_ELightLeafAccessors[MAX_ELIGHTS];

bool cl_takesnapshot = false;
static bool cl_takejpeg = false;
static bool cl_takesnapshot_internal = false;

static int cl_jpegquality = DEFAULT_JPEG_QUALITY;
static ConVar jpeg_quality( "jpeg_quality", "90", 0, "jpeg screenshot quality." );

static int	cl_snapshotnum = 0;
static char cl_snapshotname[MAX_OSPATH];
static char cl_snapshot_subdirname[MAX_OSPATH];

// Must match game .dll definition
// HACK HACK FOR E3 -- Remove this after E3
#define	HIDEHUD_ALL			( 1<<2 )

void PhonemeMP3Shutdown( void );

struct ResourceLocker 
{
	ResourceLocker()
	{
		g_pFileSystem->AsyncFinishAll();
		g_pFileSystem->AsyncSuspend();

		// Need to temporarily disable queued material system, then lock it
		m_QMS = Host_AllowQueuedMaterialSystem( false );
		m_MatLock = g_pMaterialSystem->Lock();
	}

	~ResourceLocker()
	{
		// Restore QMS
		materials->Unlock( m_MatLock );
		Host_AllowQueuedMaterialSystem( m_QMS );
		g_pFileSystem->AsyncResume();

		// ??? What?  Why?
		//// Need to purge cached materials due to a sv_pure change.
		//g_pMaterialSystem->UncacheAllMaterials();
	}

	bool			m_QMS;
	MaterialLock_t	m_MatLock;
};

// Reloads a list of files if they are still loaded
void CL_ReloadFilesInList( IFileList *pFilesToReload )
{
	if ( !pFilesToReload )
	{
		return;
	}

	ResourceLocker crashPreventer;

	// Handle materials..
	materials->ReloadFilesInList( pFilesToReload );

	// Handle models.. NOTE: this MUST come after materials->ReloadFilesInList because the
	// models need to know which materials got flushed.
	modelloader->ReloadFilesInList( pFilesToReload );

	S_ReloadFilesInList( pFilesToReload );

	// Let the client at it (for particles)
	if ( g_ClientDLL )
	{
		g_ClientDLL->ReloadFilesInList( pFilesToReload );
	}
}

void CL_HandlePureServerWhitelist( CPureServerWhitelist *pWhitelist, /* out */ IFileList *&pFilesToReload )
{
	// Free the old whitelist and get the new one.
	if ( cl.m_pPureServerWhitelist )
		cl.m_pPureServerWhitelist->Release();

	cl.m_pPureServerWhitelist = pWhitelist;
	if ( cl.m_pPureServerWhitelist )
		cl.m_pPureServerWhitelist->AddRef();

	g_pFileSystem->RegisterFileWhitelist( pWhitelist, &pFilesToReload );

	// Now that we've flushed any files that shouldn't have been on disk, we should have a CRC
	// set that we can check with the server.
	cl.m_bCheckCRCsWithServer = true;
}

void PrintSvPureWhitelistClassification( const CPureServerWhitelist *pWhiteList )
{
	if ( pWhiteList == NULL )
	{
		Msg( "The server is using sv_pure -1 (no file checking).\n" );
		return;
	}

	// Load up the default whitelist
	CPureServerWhitelist *pStandardList = CPureServerWhitelist::Create( g_pFullFileSystem );
	pStandardList->Load( 0 );
	if ( *pStandardList == *pWhiteList )
	{
		Msg( "The server is using sv_pure 0.  (Enforcing consistency for select files only)\n" );
	}
	else
	{
		pStandardList->Load( 2 );
		if ( *pStandardList == *pWhiteList )
		{
			Msg( "The server is using sv_pure 2.  (Fully pure)\n" );
		}
		else
		{
			Msg( "The server is using sv_pure 1.  (Custom pure server rules.)\n" );
		}
	}
	pStandardList->Release();
}

void CL_PrintWhitelistInfo()
{
	PrintSvPureWhitelistClassification( cl.m_pPureServerWhitelist );
	if ( cl.m_pPureServerWhitelist )
	{
		cl.m_pPureServerWhitelist->PrintWhitelistContents();
	}
}

// Console command to force a whitelist on the system.
#ifdef _DEBUG
void whitelist_f( const CCommand &args )
{
	int pureLevel = 2;
	if ( args.ArgC() == 2 )
	{
		pureLevel = atoi( args[1] );
	}
	else
	{
		Warning( "Whitelist 0, 1, or 2\n" );
	}

	if ( pureLevel == 0 )
	{
		Warning( "whitelist 0: CL_HandlePureServerWhitelist( NULL )\n" );
		IFileList *pFilesToReload = NULL;
		CL_HandlePureServerWhitelist( NULL, pFilesToReload );
		CL_ReloadFilesInList( pFilesToReload );
	}
	else
	{
		CPureServerWhitelist *pWhitelist = CPureServerWhitelist::Create( g_pFileSystem );
		pWhitelist->Load( pureLevel == 1 );
		IFileList *pFilesToReload = NULL;
		CL_HandlePureServerWhitelist( pWhitelist, pFilesToReload );
		CL_ReloadFilesInList( pFilesToReload );
		pWhitelist->Release();
	}
}
ConCommand whitelist( "whitelist", whitelist_f );
#endif

const CPrecacheUserData* CL_GetPrecacheUserData( INetworkStringTable *table, int index )
{
	int testLength;
	const CPrecacheUserData *data = ( CPrecacheUserData * )table->GetStringUserData( index, &testLength );
	if ( data )
	{
		ErrorIfNot( 
			testLength == sizeof( *data ),
			("CL_GetPrecacheUserData(%d,%d) - length (%d) invalid.", table->GetTableId(), index, testLength)
		);

	}
	return data;
}


//-----------------------------------------------------------------------------
// Purpose: setup the demo flag, split from CL_IsHL2Demo so CL_IsHL2Demo can be inline
//-----------------------------------------------------------------------------
static bool s_bIsHL2Demo = false;
void CL_InitHL2DemoFlag()
{
#if defined(_X360)
	s_bIsHL2Demo = false;
#else
	static bool initialized = false;
	if ( !initialized )
	{
		if ( Steam3Client().SteamApps() && !Q_stricmp( COM_GetModDirectory(), "hl2" ) )
		{
			initialized = true;

			// if user didn't buy HL2 yet, this must be the free demo
			if ( VCRGetMode() != VCR_Playback )
			{
				s_bIsHL2Demo = !Steam3Client().SteamApps()->BIsSubscribedApp( GetAppSteamAppId( k_App_HL2 ) );
			}
#if !defined( NO_VCR )
			VCRGenericValue( "e", &s_bIsHL2Demo, sizeof( s_bIsHL2Demo ) );
#endif
		}
		
		if ( !Q_stricmp( COM_GetModDirectory(), "hl2" ) && CommandLine()->CheckParm( "-demo" ) ) 
		{
			s_bIsHL2Demo = true;
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the user is playing the HL2 Demo (rather than the full game)
//-----------------------------------------------------------------------------
bool CL_IsHL2Demo()
{
	CL_InitHL2DemoFlag();
	return s_bIsHL2Demo;
}

static bool s_bIsPortalDemo = false;
void CL_InitPortalDemoFlag()
{
#if defined(_X360)
	s_bIsPortalDemo = false;
#else
	static bool initialized = false;
	if ( !initialized )
	{
		if ( Steam3Client().SteamApps() && !Q_stricmp( COM_GetModDirectory(), "portal" ) )
		{
			initialized = true;
		
			// if user didn't buy Portal yet, this must be the free demo
			if ( VCRGetMode() != VCR_Playback )
			{
				s_bIsPortalDemo = !Steam3Client().SteamApps()->BIsSubscribedApp( GetAppSteamAppId( k_App_PORTAL ) );
			}
				
#if !defined( NO_VCR )
			VCRGenericValue( "e", &s_bIsPortalDemo, sizeof( s_bIsPortalDemo ) );
#endif
		}
		
		if ( !Q_stricmp( COM_GetModDirectory(), "portal" ) && CommandLine()->CheckParm( "-demo" ) ) 
		{
			s_bIsPortalDemo = true;
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the user is playing the Portal Demo (rather than the full game)
//-----------------------------------------------------------------------------
bool CL_IsPortalDemo()
{
	CL_InitPortalDemoFlag();
	return s_bIsPortalDemo;
}


#ifdef _XBOX
extern void Host_WriteConfiguration( const char *dirname, const char *filename );
//-----------------------------------------------------------------------------
// Convar callback to write the user configuration 
//-----------------------------------------------------------------------------
void WriteConfig_f( ConVar *var, const char *pOldString )
{
	Host_WriteConfiguration( "xboxuser.cfg" );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: If the client is in the process of connecting and the cl.signon hits
//  is complete, make sure the client thinks its totally connected.
//-----------------------------------------------------------------------------
void CL_CheckClientState( void )
{
	// Setup the local network backdoor (we do this each frame so it can be toggled on and off).
	bool useBackdoor = cl_LocalNetworkBackdoor.GetInt() && 
						(cl.m_NetChannel ? cl.m_NetChannel->IsLoopback() : false) &&
						sv.IsActive() &&
						!demorecorder->IsRecording() &&
						!demoplayer->IsPlayingBack() &&
						Host_IsSinglePlayerGame();

	CL_SetupLocalNetworkBackDoor( useBackdoor );
}



//-----------------------------------------------------------------------------
// bool CL_CheckCRCs( const char *pszMap )
//-----------------------------------------------------------------------------
bool CL_CheckCRCs( const char *pszMap )
{
	CRC32_t mapCRC;        // If this is the worldmap, CRC against server's map
	MD5Value_t mapMD5;
	V_memset( mapMD5.bits, 0, MD5_DIGEST_LENGTH );

	// Don't verify CRC if we are running a local server (i.e., we are playing single player, or we are the server in multiplay
	if ( sv.IsActive() ) // Single player
		return true;

	if ( IsX360() )
	{
		return true;
	}

	bool couldHash = false;
	if ( g_ClientGlobalVariables.network_protocol > PROTOCOL_VERSION_17 )
	{
		couldHash = MD5_MapFile( &mapMD5, pszMap );
	}
	else
	{
		CRC32_Init(&mapCRC);
		couldHash = CRC_MapFile( &mapCRC, pszMap );
	}

	if (!couldHash )
	{
		// Does the file exist?
		FileHandle_t fp = 0;
		int nSize = -1;

		nSize = COM_OpenFile( pszMap, &fp );
		if ( fp )
			g_pFileSystem->Close( fp );

		if ( nSize != -1 )
		{
			COM_ExplainDisconnection( true, "Couldn't CRC map %s, disconnecting\n", pszMap);
			Host_Error( "Bad map" );
		}
		else
		{
			COM_ExplainDisconnection( true, "Missing map %s,  disconnecting\n", pszMap);
			Host_Error( "Map is missing" );
		}

		return false;
	}

	bool hashValid = false;
	if ( g_ClientGlobalVariables.network_protocol > PROTOCOL_VERSION_17 )
	{
		hashValid = MD5_Compare( cl.serverMD5, mapMD5 );
	}

	// Hacked map
	if ( !hashValid && !demoplayer->IsPlayingBack())
	{
		if ( IsX360() )
		{
			Warning( "Disconnect: BSP CRC failed!\n" );
		}
		COM_ExplainDisconnection( true, "Your map [%s] differs from the server's.\n", pszMap );
		Host_Error( "Client's map differs from the server's" );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nMaxClients - 
//-----------------------------------------------------------------------------
void CL_ReallocateDynamicData( int maxclients )
{
	Assert( entitylist );
	if ( entitylist )
	{
		entitylist->SetMaxEntities( MAX_EDICTS );
	}
}

/*
=================
CL_ReadPackets

Updates the local time and reads/handles messages on client net connection.
=================
*/

void CL_ReadPackets ( bool bFinalTick )
{
	VPROF_BUDGET( "CL_ReadPackets", VPROF_BUDGETGROUP_OTHER_NETWORKING );
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	if ( !Host_ShouldRun() )
		return;
	
	// update client times/tick

	cl.oldtickcount = cl.GetServerTickCount();
	if ( !cl.IsPaused() )
	{
		cl.SetClientTickCount( cl.GetClientTickCount() + 1 );
		
		// While clock correction is off, we have the old behavior of matching the client and server clocks.
		if ( !CClockDriftMgr::IsClockCorrectionEnabled() )
			cl.SetServerTickCount( cl.GetClientTickCount() );

		g_ClientGlobalVariables.tickcount = cl.GetClientTickCount();
		g_ClientGlobalVariables.curtime = cl.GetTime();
	}
	// 0 or tick_rate if simulating
	g_ClientGlobalVariables.frametime = cl.GetFrameTime();

	// read packets, if any in queue
	if ( demoplayer->IsPlayingBack() && cl.m_NetChannel )
	{
		tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "ReadPacket" );

		// process data from demo file
		cl.m_NetChannel->ProcessPlayback();
	}
	else
	{
		if ( !cl_ignorepackets.GetInt() )
		{
			tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "ProcessSocket" );
			// process data from net socket
			NET_ProcessSocket( NS_CLIENT, &cl );
		}
	}

	// check timeout, but not if running _DEBUG engine
#if !defined( _DEBUG )
	// Only check on final frame because that's when the server might send us a packet in single player.  This avoids
	//  a bug where if you sit in the game code in the debugger then you get a timeout here on resuming the engine
	//  because the timestep is > 1 tick because of the debugging delay but the server hasn't sent the next packet yet.  ywb 9/5/03
	if ( (cl.m_NetChannel?cl.m_NetChannel->IsTimedOut():false) &&
		 bFinalTick &&
		 !demoplayer->IsPlayingBack() &&
		 cl.IsConnected() )
	{
		ConMsg ("\nServer connection timed out.\n");

		// Show the vgui dialog on timeout
		COM_ExplainDisconnection( false, "Lost connection to server.");
		if ( IsPC() )
		{
			EngineVGui()->ShowErrorMessage();
		}

		Host_Disconnect( true, "Lost connection" );
		return;
	}
#endif

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CL_ClearState ( void )
{
	// clear out the current whitelist
	IFileList *pFilesToReload = NULL;
	CL_HandlePureServerWhitelist( NULL, pFilesToReload );
	CL_ReloadFilesInList( pFilesToReload );

	CL_ResetEntityBits();

	R_UnloadSkys();

	// clear decal index directories
	Decal_Init();

	StaticPropMgr()->LevelShutdownClient();

	// shutdown this level in the client DLL
	if ( g_ClientDLL )
	{
		if ( host_state.worldmodel )
		{
			char mapname[256];
			CL_SetupMapName( modelloader->GetName( host_state.worldmodel ), mapname, sizeof( mapname ) );
			phonehome->Message( IPhoneHome::PHONE_MSG_MAPEND, mapname );
		}
		audiosourcecache->LevelShutdown();
		g_ClientDLL->LevelShutdown();
	}

	R_LevelShutdown();
	if ( IsX360() )
	{
		// Reset material system temporary memory (frees up memory for map loading)
		bool bOnLevelShutdown = true;
		materials->ResetTempHWMemory( bOnLevelShutdown );
	}
	
	if ( g_pLocalNetworkBackdoor )
		g_pLocalNetworkBackdoor->ClearState();

	// clear other arrays	
	memset (cl_dlights, 0, sizeof(cl_dlights));
	memset (cl_elights, 0, sizeof(cl_elights));

	// Wipe the hunk ( unless the server is active )
	Host_FreeStateAndWorld( false );
	Host_FreeToLowMark( false );

	PhonemeMP3Shutdown();

	// Wipe the remainder of the structure.
	cl.Clear();
}

//-----------------------------------------------------------------------------
// Purpose: Used for sorting sounds
// Input  : &sound1 - 
//			&sound2 - 
// Output : static bool
//-----------------------------------------------------------------------------
static bool CL_SoundMessageLessFunc( SoundInfo_t const &sound1, SoundInfo_t const &sound2 )
{
	return sound1.nSequenceNumber < sound2.nSequenceNumber;
}

static CUtlRBTree< SoundInfo_t, int > g_SoundMessages( 0, 0, CL_SoundMessageLessFunc );
extern ConVar snd_show;

//-----------------------------------------------------------------------------
// Purpose: Add sound to queue
// Input  : sound - 
//-----------------------------------------------------------------------------
void CL_AddSound( const SoundInfo_t &sound )
{
	g_SoundMessages.Insert( sound );
}

//-----------------------------------------------------------------------------
// Purpose: Play sound packet
// Input  : sound - 
//-----------------------------------------------------------------------------
void CL_DispatchSound( const SoundInfo_t &sound )
{
	int nSoundNum = sound.nSoundNum;

	CSfxTable *pSfx;

	char name[ MAX_QPATH ];

	name[ 0 ] = 0;
	if ( sound.bIsSentence )
	{
		// make dummy sfx for sentences
		const char *pSentenceName = VOX_SentenceNameFromIndex( sound.nSoundNum );
		if ( !pSentenceName )
		{
			pSentenceName = "";
		}

		V_snprintf( name, sizeof( name ), "%c%s", CHAR_SENTENCE, pSentenceName );		
		pSfx = S_DummySfx( name );
	}
	else
	{
		V_strncpy( name, cl.GetSoundName( sound.nSoundNum ), sizeof( name ) );

		const char *pchTranslatedName = g_ClientDLL->TranslateEffectForVisionFilter( "sounds", name );
		if ( V_strcmp( pchTranslatedName, name ) != 0 )
		{
			V_strncpy( name, pchTranslatedName, sizeof( name ) );
			nSoundNum = cl.LookupSoundIndex( name );
		}

		pSfx = cl.GetSound( nSoundNum );
	}

	if ( snd_show.GetInt() >= 2 )
	{
		DevMsg( "%i (seq %i) %s : src %d : ch %d : %d dB : vol %.2f : time %.3f (%.4f delay) @%.1f %.1f %.1f\n", 
			host_framecount,
			sound.nSequenceNumber,
			name, 
			sound.nEntityIndex, 
			sound.nChannel, 
			sound.Soundlevel, 
			sound.fVolume, 
			cl.GetTime(),
			sound.fDelay,
			sound.vOrigin.x,
			sound.vOrigin.y,
			sound.vOrigin.z );
	}

	StartSoundParams_t params;
	params.staticsound = (sound.nChannel == CHAN_STATIC) ? true : false;
	params.soundsource = sound.nEntityIndex;
	params.entchannel = params.staticsound ? CHAN_STATIC : sound.nChannel;
	params.pSfx = pSfx;
	params.origin = sound.vOrigin;
	params.fvol = sound.fVolume;
	params.soundlevel = sound.Soundlevel;
	params.flags = sound.nFlags;
	params.pitch = sound.nPitch;
	params.specialdsp = sound.nSpecialDSP;
	params.fromserver = true;
	params.delay = sound.fDelay;
	// we always want to do this when this flag is set - even if the delay is zero we need to precisely
	// schedule this sound
	if ( sound.nFlags & SND_DELAY )
	{
		// anything adjusted less than 100ms forward was probably scheduled this frame
		if ( sound.fDelay > -0.100f )
		{
			float soundtime = cl.m_flLastServerTickTime + sound.fDelay;
			// this adjusts for host_thread_mode or any other cases where we're running more than one
			// tick at a time, but we get network updates on the first tick
			soundtime -= ((g_ClientGlobalVariables.simTicksThisFrame-1) * host_state.interval_per_tick);
			// this sound was networked over from the server, use server clock
			params.delay = S_ComputeDelayForSoundtime( soundtime, CLOCK_SYNC_SERVER );
#if 0
			static float lastSoundTime = 0;
			Msg("[%.3f] Play %s at %.3f %.1fsms delay\n", soundtime - lastSoundTime, name, soundtime, params.delay * 1000.0f );
			lastSoundTime = soundtime;
#endif
			if ( params.delay <= 0 )
			{
				// leave a little delay to flag the channel in the low-level sound system
				params.delay = 1e-6f;
			}
		}
		else
		{
			params.delay = sound.fDelay;
		}
	}
	params.speakerentity = sound.nSpeakerEntity;

	// Give the client DLL a chance to run arbitrary code to affect the sound parameters before we
	// play.
	g_ClientDLL->ClientAdjustStartSoundParams( params );

	if ( params.staticsound )
	{
		S_StartSound( params );
	}
	else
	{
		// Don't actually play non-static sounds if playing a demo and skipping ahead
		// but always stop sounds
		if ( demoplayer->IsSkipping() && !(sound.nFlags&SND_STOP) )
		{
			return;
		}
		S_StartSound( params );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called after reading network messages to play sounds encoded in the network packet
//-----------------------------------------------------------------------------
void CL_DispatchSounds( void )
{
	int i;
	// Walk list in sequence order
	i = g_SoundMessages.FirstInorder();
	while ( i != g_SoundMessages.InvalidIndex() )
	{
		SoundInfo_t const *msg = &g_SoundMessages[ i ];
		Assert( msg );
		if ( msg )
		{
			// Play the sound
			CL_DispatchSound( *msg );
		}
		i = g_SoundMessages.NextInorder( i );
	}

	// Reset the queue each time we empty it!!!
	g_SoundMessages.RemoveAll();
}


//-----------------------------------------------------------------------------
// Retry last connection (e.g., after we enter a password)
//-----------------------------------------------------------------------------
void CL_Retry()
{
	if ( !cl.m_szRetryAddress[ 0 ] )
	{
		ConMsg( "Can't retry, no previous connection\n" );
		return;
	}

	// Check that we can add the two execution markers
	bool bCanAddExecutionMarkers = Cbuf_HasRoomForExecutionMarkers( 2 );

	ConMsg( "Commencing connection retry to %s\n", cl.m_szRetryAddress );

	// We need to temporarily disable this execution marker so the connect command succeeds if it was executed by the server.
	// We would still need this even if we called CL_Connect directly because the connect process may execute commands which we want to succeed.
	const char *pszCommand = va( "connect %s %s\n", cl.m_szRetryAddress, cl.m_sRetrySourceTag.String() );
	if ( cl.m_bRestrictServerCommands && bCanAddExecutionMarkers )
		Cbuf_AddTextWithMarkers( eCmdExecutionMarker_Disable_FCVAR_SERVER_CAN_EXECUTE, pszCommand, eCmdExecutionMarker_Enable_FCVAR_SERVER_CAN_EXECUTE );
	else
		Cbuf_AddText( pszCommand );
}

CON_COMMAND_F( retry, "Retry connection to last server.", FCVAR_DONTRECORD | FCVAR_SERVER_CAN_EXECUTE | FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	CL_Retry();
}


/*
=====================
CL_Connect_f

User command to connect to server
=====================
*/

void CL_Connect( const char *address, const char *pszSourceTag )
{
	// If it's not a single player connection to "localhost", initialize networking & stop listenserver
	if ( Q_strncmp( address, "localhost", 9 ) )
	{
		Host_Disconnect(false);	

		// allow remote
		NET_SetMutiplayer( true );		

		// start progress bar immediately for remote connection
		EngineVGui()->EnabledProgressBarForNextLoad();

		SCR_BeginLoadingPlaque();

		EngineVGui()->UpdateProgressBar(PROGRESS_BEGINCONNECT);
	}
	else
	{
		// we are connecting/reconnecting to local game
		// so don't stop listenserver 
		cl.Disconnect( "Connecting to local host", false );
	}

	// This happens as part of the load process anyway, but on slower systems it causes the server to timeout the
	// connection.  Use the opportunity to flush anything before starting a new connection.
	UpdateMaterialSystemConfig();

	cl.Connect( address, pszSourceTag );

	// Reset error conditions
	gfExtendedError = false;
}

CON_COMMAND_F( connect, "Connect to specified server.", FCVAR_DONTRECORD )
{
	// Default command processing considers ':' a command separator,
	// and we donly want spaces to count.  So we'll need to re-split the arg string
	CUtlVector<char*> vecArgs;
	V_SplitString( args.ArgS(), " ", vecArgs );

	// How many arguments?
	if ( vecArgs.Count() == 1  )
	{
		CL_Connect( vecArgs[0], "" );
	}
	else if ( vecArgs.Count() == 2 )
	{
		CL_Connect( vecArgs[0], vecArgs[1] );
	}
	else
	{
		ConMsg( "Usage:  connect <server>\n" );
	}
	vecArgs.PurgeAndDeleteElements();
}

CON_COMMAND_F( redirect, "Redirect client to specified server.", FCVAR_DONTRECORD | FCVAR_SERVER_CAN_EXECUTE )
{
	if ( !CBaseClientState::ConnectMethodAllowsRedirects() )
	{
		ConMsg( "redirect: Current connection method does not allow silent redirects.\n");
		return;
	}

	// Default command processing considers ':' a command separator,
	// and we donly want spaces to count.  So we'll need to re-split the arg string
	CUtlVector<char*> vecArgs;
	V_SplitString( args.ArgS(), " ", vecArgs );

	if ( vecArgs.Count() == 1  )
	{
		CL_Connect( vecArgs[0], "redirect" );
	}
	else
	{
		ConMsg( "Usage:  redirect <server>\n" );
	}
	vecArgs.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// Takes the map name, strips path and extension
//-----------------------------------------------------------------------------
void CL_SetupMapName( const char* pName, char* pFixedName, int maxlen )
{
	const char* pSlash = strrchr( pName, '\\' );
	const char* pSlash2 = strrchr( pName, '/' );
	if (pSlash2 > pSlash)
		pSlash = pSlash2;
	if (pSlash)
		++pSlash;
	else 
		pSlash = pName;

	Q_strncpy( pFixedName, pSlash, maxlen );
	char* pExt = strchr( pFixedName, '.' );
	if (pExt)
		*pExt = 0;
}

CPureServerWhitelist* CL_LoadWhitelist( INetworkStringTable *pTable, const char *pName )
{
	// If there is no entry for the pure server whitelist, then sv_pure is off and the client can do whatever it wants.
	int iString = pTable->FindStringIndex( pName );
	if ( iString == INVALID_STRING_INDEX )
		return NULL;

	int dataLen; 
	const void *pData = pTable->GetStringUserData( iString, &dataLen );
	if ( pData )
	{
		CUtlBuffer buf( pData, dataLen, CUtlBuffer::READ_ONLY );
		
		CPureServerWhitelist *pWhitelist = CPureServerWhitelist::Create( g_pFullFileSystem );
		pWhitelist->Decode( buf );
		return pWhitelist;
	}
	else
	{
		return NULL;
	}
}


void CL_CheckForPureServerWhitelist( /* out */ IFileList *&pFilesToReload )
{
#ifdef DISABLE_PURE_SERVER_STUFF
	return;
#endif

	// Don't do sv_pure stuff in SP games or HLTV/replay
	if ( cl.m_nMaxClients <= 1 || cl.ishltv || demoplayer->IsPlayingBack()
#ifdef REPLAY_ENABLED
		|| cl.isreplay
#endif // ifdef REPLAY_ENABLED
		)
		return;
	
	CPureServerWhitelist *pWhitelist = NULL;
	if ( cl.m_pServerStartupTable )
		pWhitelist = CL_LoadWhitelist( cl.m_pServerStartupTable, "PureServerWhitelist" );
		
	PrintSvPureWhitelistClassification( pWhitelist );
	CL_HandlePureServerWhitelist( pWhitelist, pFilesToReload );
	if ( pWhitelist )
	{
		pWhitelist->Release();
	}
}

int CL_GetServerQueryPort()
{
	// Yes, this is ugly getting this data out of a string table. Would be better to have it in our network protocol,
	// but we don't have a way to change the protocol without breaking things for people.
	if ( !cl.m_pServerStartupTable )
		return 0;
		
	int iString = cl.m_pServerStartupTable->FindStringIndex( "QueryPort" );
	if ( iString == INVALID_STRING_INDEX )
		return 0;
		
	int dataLen; 
	const void *pData = cl.m_pServerStartupTable->GetStringUserData( iString, &dataLen );
	if ( pData && dataLen == sizeof( int ) )
		return *((const int*)pData);
	else
		return 0;
}

/*
==================
CL_RegisterResources

Clean up and move to next part of sequence.
==================
*/
void CL_RegisterResources( void )
{
	// All done precaching.
	host_state.SetWorldModel( cl.GetModel( 1 ) );
	if ( !host_state.worldmodel )
	{
		Host_Error( "CL_RegisterResources:  host_state.worldmodel/cl.GetModel( 1 )==NULL\n" );
	}

	// Force main window to repaint... (only does something if running shaderapi
	videomode->InvalidateWindow();
}

void CL_FullyConnected( void )
{
	CETWScope timer( "CL_FullyConnected" );

	EngineVGui()->UpdateProgressBar( PROGRESS_FULLYCONNECTED );

	// This has to happen here, in phase 3, because it is in this phase
	// that raycasts against the world is supported (owing to the fact
	// that the world entity has been created by this point)
	StaticPropMgr()->LevelInitClient();

	if ( IsX360() )
	{
		// Notify the loader the end of the loading context, preloads are about to be purged
		g_pQueuedLoader->EndMapLoading( false );
	}

	// flush client-side dynamic models that have no refcount
	modelloader->FlushDynamicModels();

	// loading completed
	// can NOW safely purge unused models and their data hierarchy (materials, shaders, etc)
	modelloader->PurgeUnusedModels();

	// Purge the preload stores, oreder is critical
	g_pMDLCache->ShutdownPreloadData();

	// NOTE: purposely disabling for singleplayer, memory spike causing issues, preload's stay in
	// UNDONE: discard preload for TF to save memory
	// g_pFileSystem->DiscardPreloadData();

	// We initialize this list before the load, but don't perform reloads until after flushes have happened to avoid
	// unnecessary reloads of items that wont be used on this map.
	if ( cl.m_pPendingPureFileReloads )
	{
		CL_ReloadFilesInList( cl.m_pPendingPureFileReloads );
		cl.m_pPendingPureFileReloads->Release();
		cl.m_pPendingPureFileReloads = NULL;
	}

	// ***************************************************************
	// NO MORE PRELOAD DATA AVAILABLE PAST THIS POINT!!!
	// ***************************************************************

 	g_ClientDLL->LevelInitPostEntity();

	// communicate to tracker that we're in a game
	int ip = cl.m_NetChannel->GetRemoteAddress().GetIPNetworkByteOrder();
	short port = cl.m_NetChannel->GetRemoteAddress().GetPort();
	if (!port)
	{
		ip = net_local_adr.GetIPNetworkByteOrder();
		port = net_local_adr.GetPort();
	}

	int iQueryPort = CL_GetServerQueryPort();
	EngineVGui()->NotifyOfServerConnect(com_gamedir, ip, port, iQueryPort);

	GetTestScriptMgr()->CheckPoint( "FinishedMapLoad" );

	EngineVGui()->UpdateProgressBar( PROGRESS_READYTOPLAY );

	if ( !IsX360() || cl.m_nMaxClients == 1 )
	{
		// Need this to persist for multiplayer respawns, 360 can't reload
		CM_DiscardEntityString();
	}

	g_pMDLCache->EndMapLoad();

#if defined( _MEMTEST )
	Cbuf_AddText( "mem_dump\n" );
#endif

	if ( developer.GetInt() > 0 )
	{
		ConDMsg( "Signon traffic \"%s\":  incoming %s, outgoing %s\n",
			cl.m_NetChannel->GetName(),
			Q_pretifymem( cl.m_NetChannel->GetTotalData( FLOW_INCOMING ), 3 ),
			Q_pretifymem( cl.m_NetChannel->GetTotalData( FLOW_OUTGOING ), 3 ) );
	}

	if ( IsX360() )
	{
		// Reset material system temporary memory (once loading is complete), ready for in-map use
		bool bOnLevelShutdown = false;
		materials->ResetTempHWMemory( bOnLevelShutdown );
	}

	// allow normal screen updates
	SCR_EndLoadingPlaque();
	EndLoadingUpdates();

	// FIXME: Please oh please move this out of this spot...
	// It so does not belong here. Instead, we want some phase of the
	// client DLL where it knows its read in all entities
	if ( IsPC() )
	{
		int i;
		if( (i = CommandLine()->FindParm( "-buildcubemaps" )) != 0 )
		{
			int numIterations = 1;
			if( CommandLine()->ParmCount() > i + 1 )
			{
				numIterations = atoi( CommandLine()->GetParm(i+1) );
			}
			if( numIterations == 0 )
			{
				numIterations = 1;
			}
			char cmd[1024] = { 0 };
			V_snprintf( cmd, sizeof( cmd ), "buildcubemaps %u\nquit\n", numIterations );
			Cbuf_AddText( cmd );
		}
		else if( CommandLine()->FindParm( "-navanalyze" ) )
		{
			Cbuf_AddText( "nav_edit 1;nav_analyze_scripted\n" );
		}
		else if( CommandLine()->FindParm( "-navforceanalyze" ) )
		{
			Cbuf_AddText( "nav_edit 1;nav_analyze_scripted force\n" );
		}
		else if ( CommandLine()->FindParm("-exit") )
		{
			Cbuf_AddText( "quit\n" );
		}
	}

	// background maps are for main menu UI, QMS not needed or used, easier context
	if ( !engineClient->IsLevelMainMenuBackground() )
	{
		// map load complete, safe to allow QMS
		Host_AllowQueuedMaterialSystem( true );
	}

	// This is a Hack, but we need to suppress rendering for a bit in single player to let values settle on the client
	if ( (cl.m_nMaxClients == 1) && !demoplayer->IsPlayingBack() )
	{
		scr_nextdrawtick = host_tickcount + TIME_TO_TICKS( 0.25f );
	}

#ifdef _X360
	// At this point, check for a valid controller connection.  If it's been lost, then we need to pop our game UI up
	XINPUT_CAPABILITIES caps;
	if ( XInputGetCapabilities( XBX_GetPrimaryUserId(), XINPUT_FLAG_GAMEPAD, &caps ) == ERROR_DEVICE_NOT_CONNECTED )
	{
		EngineVGui()->ActivateGameUI();
	}
#endif // _X360

	// Now that we're connected, toggle the clan tag so it gets sent to the server
	int id = cl_clanid.GetInt();
	cl_clanid.SetValue( 0 );
	cl_clanid.SetValue( id );

	MemAlloc_CompactHeap();

	extern double g_flAccumulatedModelLoadTime;
	extern double g_flAccumulatedSoundLoadTime;
	extern double g_flAccumulatedModelLoadTimeStudio;
	extern double g_flAccumulatedModelLoadTimeVCollideSync;
	extern double g_flAccumulatedModelLoadTimeVCollideAsync;
	extern double g_flAccumulatedModelLoadTimeVirtualModel;
	extern double g_flAccumulatedModelLoadTimeStaticMesh;
	extern double g_flAccumulatedModelLoadTimeBrush;
	extern double g_flAccumulatedModelLoadTimeSprite;
	extern double g_flAccumulatedModelLoadTimeMaterialNamesOnly;
//	extern double g_flLoadStudioHdr;

	COM_TimestampedLog( "Sound Loading time %.4f", g_flAccumulatedSoundLoadTime );
	COM_TimestampedLog( "Model Loading time %.4f", g_flAccumulatedModelLoadTime );
	COM_TimestampedLog( "  Model Loading time studio %.4f", g_flAccumulatedModelLoadTimeStudio );
	COM_TimestampedLog( "    Model Loading time GetVCollide %.4f -sync", g_flAccumulatedModelLoadTimeVCollideSync );
	COM_TimestampedLog( "    Model Loading time GetVCollide %.4f -async", g_flAccumulatedModelLoadTimeVCollideAsync );
	COM_TimestampedLog( "    Model Loading time GetVirtualModel %.4f", g_flAccumulatedModelLoadTimeVirtualModel );
	COM_TimestampedLog( "    Model loading time Mod_GetModelMaterials only %.4f", g_flAccumulatedModelLoadTimeMaterialNamesOnly );
	COM_TimestampedLog( "  Model Loading time world %.4f", g_flAccumulatedModelLoadTimeBrush );
	COM_TimestampedLog( "  Model Loading time sprites %.4f", g_flAccumulatedModelLoadTimeSprite );
	COM_TimestampedLog( "  Model Loading time meshes %.4f", g_flAccumulatedModelLoadTimeStaticMesh );
//	COM_TimestampedLog( "    Model Loading time meshes studiohdr load %.4f", g_flLoadStudioHdr );

	COM_TimestampedLog( "*** Map Load Complete" );

	float map_loadtime_start = dev_loadtime_map_start.GetFloat();
	if (map_loadtime_start > 0.0)
	{
		float elapsed = Plat_FloatTime() - map_loadtime_start;
		dev_loadtime_map_elapsed.SetValue( elapsed );

		// Clear this for next time so we know we did.
		dev_loadtime_map_start.SetValue( 0.0f );
	}
}


/*
=====================
CL_NextDemo

Called to play the next demo in the demo loop
=====================
*/
void CL_NextDemo (void)
{
	char	str[1024];

	if (cl.demonum == -1)
		return;		// don't play demos

	SCR_BeginLoadingPlaque ();

	if ( cl.demos[cl.demonum].IsEmpty() || cl.demonum == MAX_DEMOS )
	{
		cl.demonum = 0;
		if ( cl.demos[cl.demonum].IsEmpty() )
		{
			scr_disabled_for_loading = false;

			ConMsg ("No demos listed with startdemos\n");
			cl.demonum = -1;
			return;
		}
		else if ( !demoplayer->ShouldLoopDemos() )
		{
			cl.demonum = -1;
			scr_disabled_for_loading = false;
			Host_Disconnect( true );

			demoplayer->OnLastDemoInLoopPlayed();

			return;
		}
	}

	Q_snprintf (str,sizeof( str ), "%s %s", CommandLine()->FindParm("-timedemoloop") ? "timedemo" : "playdemo", cl.demos[cl.demonum].Get());
	Cbuf_AddText (str);
	cl.demonum++;
}

ConVar cl_screenshotname( "cl_screenshotname", "", 0, "Custom Screenshot name" );

// We'll take a snapshot at the next available opportunity
void CL_TakeScreenshot(const char *name)
{
	cl_takesnapshot = true;
	cl_takejpeg = false;
	cl_takesnapshot_internal = false;

	if ( name != NULL )
	{
		Q_strncpy( cl_snapshotname, name, sizeof( cl_snapshotname ) );		
	}
	else
	{
		cl_snapshotname[0] = 0;

		if ( Q_strlen( cl_screenshotname.GetString() ) > 0 )
		{
			Q_snprintf( cl_snapshotname, sizeof( cl_snapshotname ), "%s", cl_screenshotname.GetString() );		
		}
	}

	cl_snapshot_subdirname[0] = 0;
}

CON_COMMAND_F( screenshot, "Take a screenshot.", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	GetTestScriptMgr()->SetWaitCheckPoint( "screenshot" );

	// Don't playback screenshots unless specifically requested.	
	if ( demoplayer->IsPlayingBack() && !cl_playback_screenshots.GetBool() )
		return;

	if( args.ArgC() == 2 )
	{
		CL_TakeScreenshot( args[ 1 ] );
	}
	else
	{
		CL_TakeScreenshot( NULL );
	}
}

CON_COMMAND_F( devshots_screenshot, "Used by the -makedevshots system to take a screenshot. For taking your own screenshots, use the 'screenshot' command instead.", FCVAR_DONTRECORD )
{
	CL_TakeScreenshot( NULL );

	// See if we got a subdirectory to store the devshots in
	if ( args.ArgC() == 2 )
	{
		Q_strncpy( cl_snapshot_subdirname, args[1], sizeof( cl_snapshot_subdirname ) );		

		// Use the first available shot in each subdirectory
		cl_snapshotnum = 0;
	}
}

// We'll take a snapshot at the next available opportunity
void CL_TakeJpeg(const char *name, int quality)
{
	// Don't playback screenshots unless specifically requested.	
	if ( demoplayer->IsPlayingBack() && !cl_playback_screenshots.GetBool() )
		return;
	
	cl_takesnapshot = true;
	cl_takejpeg = true;
	cl_jpegquality = clamp( quality, 1, 100 );
	cl_takesnapshot_internal = false;

	if ( name != NULL )
	{
		Q_strncpy( cl_snapshotname, name, sizeof( cl_snapshotname ) );		
	}
	else
	{
		cl_snapshotname[0] = 0;
	}
}

CON_COMMAND( jpeg, "Take a jpeg screenshot:  jpeg <filename> <quality 1-100>." )
{
	if( args.ArgC() >= 2 )
	{
		if ( args.ArgC() == 3 )
		{
			CL_TakeJpeg( args[ 1 ], Q_atoi( args[2] ) );
		}
		else
		{
			CL_TakeJpeg( args[ 1 ], jpeg_quality.GetInt() );
		}
	}
	else
	{
		CL_TakeJpeg( NULL, jpeg_quality.GetInt() );
	}
}

static void screenshot_internal( const CCommand &args )
{

	if( args.ArgC() != 2 )
	{
		Assert( args.ArgC() >= 2 );
		Warning( "__screenshot_internal - wrong number of arguments" );
		return;
	}
	Q_strncpy( cl_snapshotname, args[1], ARRAYSIZE(cl_snapshotname) );
	cl_takesnapshot = true;
	cl_takejpeg = true;
	cl_jpegquality = 70;
	cl_takesnapshot_internal = true;
}

ConCommand screenshot_internal_command( "__screenshot_internal", screenshot_internal, "Internal command to take a screenshot without renumbering or notifying Steam.", FCVAR_DONTRECORD | FCVAR_HIDDEN );

void CL_TakeSnapshotAndSwap()
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	bool bReadPixelsFromFrontBuffer = g_pMaterialSystemHardwareConfig->ReadPixelsFromFrontBuffer();
	if ( bReadPixelsFromFrontBuffer )
	{
		Shader_SwapBuffers();
	}
	
	if (cl_takesnapshot)
	{
		// Disable threading for the duration of the screenshots, because we need to get pointers to the (complete) 
		// back buffer right now.
		bool bEnabled = materials->AllowThreading( false, g_nMaterialSystemThread );

		char base[MAX_OSPATH];
		char filename[MAX_OSPATH];
		IClientEntity *world = entitylist->GetClientEntity( 0 );

		g_pFileSystem->CreateDirHierarchy( "screenshots", "DEFAULT_WRITE_PATH" );

		if ( cl_takesnapshot_internal )
		{

			// !KLUDGE! Don't save this screenshot to steam
			ConVarRef cl_savescreenshotstosteam( "cl_savescreenshotstosteam" );
			bool bSaveValue = cl_savescreenshotstosteam.GetBool();
			cl_savescreenshotstosteam.SetValue( false );

			Q_snprintf( filename, sizeof( filename ), "screenshots/%s.jpg", cl_snapshotname );
			videomode->TakeSnapshotJPEG( filename, cl_jpegquality );

			cl_savescreenshotstosteam.SetValue( bSaveValue );
		}
		else
		{
			if ( world && world->GetModel() )
			{
				Q_FileBase( modelloader->GetName( ( model_t *)world->GetModel() ), base, sizeof( base ) );

				if ( IsX360() )
				{
					// map name has an additional extension
					V_StripExtension( base, base, sizeof( base ) );
				}
			}
			else
			{
				Q_strncpy( base, "Snapshot", sizeof( base ) );
			}

			char extension[MAX_OSPATH];
			Q_snprintf( extension, sizeof( extension ), "%s.%s", GetPlatformExt(), cl_takejpeg ? "jpg" : "tga" );

			// Using a subdir? If so, create it
			if ( cl_snapshot_subdirname[0] )
			{
				Q_snprintf( filename, sizeof( filename ), "screenshots/%s/%s", base, cl_snapshot_subdirname );
				g_pFileSystem->CreateDirHierarchy( filename, "DEFAULT_WRITE_PATH" );
			}

			if ( cl_snapshotname[0] )
			{
				Q_strncpy( base, cl_snapshotname, sizeof( base ) );
				Q_snprintf( filename, sizeof( filename ), "screenshots/%s%s", base, extension );

				int iNumber = 0;
				char renamedfile[MAX_OSPATH];

				while ( 1 )
				{
					Q_snprintf( renamedfile, sizeof( renamedfile ), "screenshots/%s_%04d%s", base, iNumber++, extension );	
					if( !g_pFileSystem->GetFileTime( renamedfile ) )
						break;
				}

				if ( iNumber > 0 && g_pFileSystem->GetFileTime( filename ) )
				{
					g_pFileSystem->RenameFile(filename, renamedfile);
				}

				cl_screenshotname.SetValue( "" );
			}
			else
			{
				while( 1 )
				{
					if ( cl_snapshot_subdirname[0] )
					{
						Q_snprintf( filename, sizeof( filename ), "screenshots/%s/%s/%s%04d%s", base, cl_snapshot_subdirname, base, cl_snapshotnum++, extension  );
					}
					else
					{
						Q_snprintf( filename, sizeof( filename ), "screenshots/%s%04d%s", base, cl_snapshotnum++, extension  );
					}

					if( !g_pFileSystem->GetFileTime( filename ) )
					{
						// woo hoo!  The file doesn't exist already, so use it.
						break;
					}
				}
			}
			if ( cl_takejpeg )
			{
				videomode->TakeSnapshotJPEG( filename, cl_jpegquality );
				g_ServerRemoteAccess.UploadScreenshot( filename );
			}
			else
			{
				videomode->TakeSnapshotTGA( filename );
			}
		}
		cl_takesnapshot = false;
		cl_takesnapshot_internal = false;
		GetTestScriptMgr()->CheckPoint( "screenshot" );

		// Restore threading if it was previously enabled (if it wasn't this will do nothing).
		materials->AllowThreading( bEnabled, g_nMaterialSystemThread );
	}

	// If recording movie and the console is totally up, then write out this frame to movie file.
	if ( cl_movieinfo.IsRecording() && !Con_IsVisible() && !scr_drawloading )
	{
		videomode->WriteMovieFrame( cl_movieinfo );
		++cl_movieinfo.movieframe;
	}

	if( !bReadPixelsFromFrontBuffer )
	{
		Shader_SwapBuffers();
	}

	// take a screenshot for savegames if necessary
	saverestore->UpdateSaveGameScreenshots();

	// take screenshot for bx movie maker
	EngineTool_UpdateScreenshot();
}

static float s_flPreviousHostFramerate = 0;
ConVar cl_simulate_no_quicktime( "cl_simulate_no_quicktime", "0", FCVAR_HIDDEN );
void CL_StartMovie( const char *filename, int flags, int nWidth, int nHeight, float flFrameRate, int nJpegQuality, VideoSystem_t videoSystem )
{
	Assert( g_pVideoRecorder == NULL );

	// StartMove depends on host_framerate not being 0.
	s_flPreviousHostFramerate = host_framerate.GetFloat();
	host_framerate.SetValue( flFrameRate );

	cl_movieinfo.Reset();
	Q_strncpy( cl_movieinfo.moviename, filename, sizeof( cl_movieinfo.moviename ) );
	cl_movieinfo.type = flags;
	cl_movieinfo.jpeg_quality = nJpegQuality;

	bool bSuccess = true;
	if ( cl_movieinfo.DoVideo() || cl_movieinfo.DoVideoSound() )
	{
// HACK:  THIS MUST MATCH snd_device.h.  Should be exposed more cleanly!!!
#define SOUND_DMA_SPEED		44100		// hardware playback rate

		// MGP - switched over to using valve video services from avi
		if ( videoSystem == VideoSystem::NONE && g_pVideo )
		{
			// Find a video system based on features if they didn't specify a specific one.
			VideoSystemFeature_t neededFeatures = VideoSystemFeature::NO_FEATURES;
			if ( cl_movieinfo.DoVideo() )
				neededFeatures |= VideoSystemFeature::ENCODE_VIDEO_TO_FILE;
			if ( cl_movieinfo.DoVideoSound() )
				neededFeatures |= VideoSystemFeature::ENCODE_AUDIO_TO_FILE;
		
			videoSystem = g_pVideo->FindNextSystemWithFeature( neededFeatures );
		}

		if ( !cl_simulate_no_quicktime.GetBool() && g_pVideo && videoSystem != VideoSystem::NONE )
		{
			g_pVideoRecorder = g_pVideo->CreateVideoRecorder( videoSystem );
			if ( g_pVideoRecorder != NULL )
			{
				VideoFrameRate_t theFps;
	 			if ( IsIntegralValue( flFrameRate ) )
	 			{
	 				theFps.SetFPS( RoundFloatToInt( flFrameRate ), false );
	 			}
	 			else if ( IsIntegralValue( flFrameRate * 1001.0f / 1000.0f ) ) // 1001 is the ntsc divisor (30*1000/1001 = 29.97, etc)
	 			{
	 				theFps.SetFPS( RoundFloatToInt( flFrameRate + 0.05f ), true );
	 			}
	 			else
	 			{
					theFps.SetFPS( RoundFloatToInt( flFrameRate ), false );
	 			} 	

				const int nSize = 256;
				CFmtStrN<nSize> fmtFullFilename( "%s%c%s", com_gamedir, CORRECT_PATH_SEPARATOR, filename );

				char szFullFilename[nSize];
				V_FixupPathName( szFullFilename, nSize, fmtFullFilename.Access() );
#ifdef USE_WEBM_FOR_REPLAY
				V_DefaultExtension( szFullFilename, ".webm", sizeof( szFullFilename ) );
#else
				V_DefaultExtension( szFullFilename, ".mp4", sizeof( szFullFilename ) );
#endif

				g_pVideoRecorder->CreateNewMovieFile( szFullFilename, cl_movieinfo.DoVideoSound() );
#ifdef USE_WEBM_FOR_REPLAY
				g_pVideoRecorder->SetMovieVideoParameters( VideoEncodeCodec::WEBM_CODEC, nJpegQuality, nWidth, nHeight, theFps );
#else
				g_pVideoRecorder->SetMovieVideoParameters( VideoEncodeCodec::DEFAULT_CODEC, nJpegQuality, nWidth, nHeight, theFps );
#endif
				
				if ( cl_movieinfo.DoVideo() )
				{
					g_pVideoRecorder->SetMovieSourceImageParameters( VideoEncodeSourceFormat::BGR_24BIT, nWidth, nHeight );
				}
				
				if ( cl_movieinfo.DoVideoSound() )
				{
					g_pVideoRecorder->SetMovieSourceAudioParameters( AudioEncodeSourceFormat::AUDIO_16BIT_PCMStereo, SOUND_DMA_SPEED, AudioEncodeOptions::LIMIT_AUDIO_TRACK_TO_VIDEO_DURATION  );
				}
			}
			else
			{
				bSuccess = false;
			}
		}
		else
		{
			bSuccess = false;
		}
	}

	if ( bSuccess )
	{
		SND_MovieStart();
	}
	else
	{
#ifdef USE_WEBM_FOR_REPLAY
		Warning( "Failed to launch startmovie!\n" );
#else
		Warning( "Failed to launch startmovie!  If you are trying to use h264, please make sure you have QuickTime installed.\n" );
#endif
		CL_EndMovie();
	}
}

void CL_EndMovie()
{
	if ( !CL_IsRecordingMovie() )
		return;

	host_framerate.SetValue( s_flPreviousHostFramerate );
	s_flPreviousHostFramerate = 0.0f;

	SND_MovieEnd();

	if ( g_pVideoRecorder && ( cl_movieinfo.DoVideo() || cl_movieinfo.DoVideoSound() ) )
	{
		g_pVideoRecorder->FinishMovie();
		
		g_pVideo->DestroyVideoRecorder( g_pVideoRecorder );
		g_pVideoRecorder = NULL;
	}
	
	cl_movieinfo.Reset();
}

bool CL_IsRecordingMovie()
{
	return cl_movieinfo.IsRecording();
}

/*
===============
CL_StartMovie_f

Sets the engine up to dump frames
===============
*/

CON_COMMAND_F( startmovie, "Start recording movie frames.", FCVAR_DONTRECORD )
{
	if ( cmd_source != src_command )
		return;

	if( args.ArgC() < 2 )
	{
		ConMsg( "startmovie <filename>\n [\n" );
		ConMsg( " (default = TGAs + .wav file)\n" );
#ifdef USE_WEBM_FOR_REPLAY
		ConMsg( " webm = WebM encoded audio and video\n" );
#else
		ConMsg( " h264 = H.264-encoded audio and video (must have QuickTime installed!)\n" );
#endif
		ConMsg( " raw = TGAs + .wav file, same as default\n" );
		ConMsg( " tga = TGAs\n" );
		ConMsg( " jpg/jpeg = JPegs\n" );
		ConMsg( " wav = Write .wav audio file\n" );
		ConMsg( " jpeg_quality nnn = set jpeq quality to nnn (range 1 to 100), default %d\n", DEFAULT_JPEG_QUALITY );
		ConMsg( " ]\n" );
		ConMsg( "examples:\n" );
		ConMsg( "   startmovie testmovie jpg wav jpeg_qality 75\n" );
#ifdef USE_WEBM_FOR_REPLAY
		ConMsg( "   startmovie testmovie webm\n" );
#else
		ConMsg( "   startmovie testmovie h264   <--- requires QuickTime\n" );
		ConMsg( "AVI is no longer supported.\n" );
#endif
		return;
	}

	if ( CL_IsRecordingMovie() )
	{
		ConMsg( "Already recording movie!\n" );
		return;
	}

	int flags = MovieInfo_t::FMOVIE_TGA | MovieInfo_t::FMOVIE_WAV;
	VideoSystem_t videoSystem = VideoSystem::NONE;
	int nJpegQuality = DEFAULT_JPEG_QUALITY;

	if ( args.ArgC() > 2 )
	{
		flags = 0;
		for ( int i = 2; i < args.ArgC(); ++i )
		{
			if ( !Q_stricmp( args[ i ], "avi" ) )
			{
				//flags |= MovieInfo_t::FMOVIE_VID | MovieInfo_t::FMOVIE_VIDSOUND;
				//videoSystem = VideoSystem::AVI;
#ifdef USE_WEBM_FOR_REPLAY
				Warning( "AVI is not supported on this platform!  Use \"webm\".\n" );
#else
				Warning( "AVI is no longer supported!  Make sure QuickTime is installed and use \"h264\" - if you install QuickTime, you will need to reboot before using startmovie.\n" );
#endif
				return;
			}
#ifdef USE_WEBM_FOR_REPLAY
			else if ( !Q_stricmp( args[ i ], "webm" ) )
			{
				flags |= MovieInfo_t::FMOVIE_VID | MovieInfo_t::FMOVIE_VIDSOUND;
				videoSystem = VideoSystem::WEBM;
			}
			else if ( !Q_stricmp( args[ i ], "h264" ) )
			{
				Warning( "h264 is not supported on this platform!  Use \"webm\".\n" );
				return;
			}
#else
			else if ( !Q_stricmp( args[ i ], "h264" ) )
			{
				flags |= MovieInfo_t::FMOVIE_VID | MovieInfo_t::FMOVIE_VIDSOUND;
				videoSystem = VideoSystem::QUICKTIME;
			}
			else if ( !Q_stricmp( args[ i ], "webm" ) )
			{
				Warning( "WebM is not supported on this platform!  Make sure QuickTime is installed and use \"h264\" - if you install QuickTime, you will need to reboot before using startmovie.\n" );
				return;
			}
#endif
			if ( !Q_stricmp( args[ i ], "raw" ) )
			{
				flags |= MovieInfo_t::FMOVIE_TGA | MovieInfo_t::FMOVIE_WAV;
			}
			if ( !Q_stricmp( args[ i ], "tga" ) )
			{
				flags |= MovieInfo_t::FMOVIE_TGA;
			}
			if ( !Q_stricmp( args[ i ], "jpeg" ) || !Q_stricmp( args[ i ], "jpg" ) )
			{
				flags &= ~MovieInfo_t::FMOVIE_TGA;
				flags |= MovieInfo_t::FMOVIE_JPG;
			}
			if ( !Q_stricmp( args[ i ], "jpeg_quality" ) )
			{
				nJpegQuality = clamp( Q_atoi( args[ ++i ] ), 1, 100 );
			}
			if ( !Q_stricmp( args[ i ], "wav" ) )
			{
				flags |= MovieInfo_t::FMOVIE_WAV;
			}
			
		}
	}

	if ( flags == 0 )
	{
#ifdef USE_WEBM_FOR_REPLAY
		Warning( "Missing or unknown recording types, must specify one or both of 'webm' or 'raw'\n" );
#else
		Warning( "Missing or unknown recording types, must specify one or both of 'h264' or 'raw'\n" );
#endif
		return;
	}

	float flFrameRate = host_framerate.GetFloat();
	if ( flFrameRate == 0.0f )
	{
		flFrameRate = 30.0f;
	}
	CL_StartMovie( args[ 1 ], flags, videomode->GetModeStereoWidth(), videomode->GetModeStereoHeight(), flFrameRate, nJpegQuality, videoSystem );
	ConMsg( "Started recording movie, frames will record after console is cleared...\n" );
}


//-----------------------------------------------------------------------------
// Ends frame dumping
//-----------------------------------------------------------------------------
CON_COMMAND_F( endmovie, "Stop recording movie frames.", FCVAR_DONTRECORD )
{
	if( !CL_IsRecordingMovie() )
	{
		ConMsg( "No movie started.\n" );
	}
	else
	{
		CL_EndMovie();
		ConMsg( "Stopped recording movie...\n" );
	}
}

/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
CON_COMMAND_F( rcon, "Issue an rcon command.", FCVAR_DONTRECORD )
{
	char	message[1024];   // Command message
	char    szParam[ 256 ];
	message[0] = 0;
	for (int i=1 ; i<args.ArgC() ; i++)
	{
		const char *pParam = args[i];
		// put quotes around empty arguments so we can pass things like this: rcon sv_password ""
		// otherwise the "" on the end is lost
		if ( strchr( pParam, ' ' ) || ( Q_strlen( pParam ) == 0 ) )
		{
			Q_snprintf( szParam, sizeof( szParam ), "\"%s\"", pParam );
			Q_strncat( message, szParam, sizeof( message ), COPY_ALL_CHARACTERS );
		}
		else
		{
			Q_strncat( message, pParam, sizeof( message ), COPY_ALL_CHARACTERS );
		}
		if ( i != ( args.ArgC() - 1 ) )
		{
			Q_strncat (message, " ", sizeof( message ), COPY_ALL_CHARACTERS);
		}
	}

	RCONClient().SendCmd( message );
}


CON_COMMAND_F( box, "Draw a debug box.", FCVAR_CHEAT )
{
	if( args.ArgC() != 7 )
	{
		ConMsg ("box x1 y1 z1 x2 y2 z2\n");
		return;
	}

	Vector mins, maxs;
	for (int i = 0; i < 3; ++i)
	{
		mins[i] = atof(args[i + 1]); 
		maxs[i] = atof(args[i + 4]); 
	}
	CDebugOverlay::AddBoxOverlay( vec3_origin, mins, maxs, vec3_angle, 255, 0, 0, 0, 100 );
}

/*
==============
CL_View_f  

Debugging changes the view entity to the specified index
===============
*/
CON_COMMAND_F( cl_view, "Set the view entity index.", FCVAR_CHEAT )
{
	int nNewView;

	if( args.ArgC() != 2 )
	{
		ConMsg ("cl_view entity#\nCurrent %i\n", cl.m_nViewEntity );
		return;
	}

	if ( cl.m_nMaxClients > 1 )
		return;

	nNewView = atoi( args[1] );
	if (!nNewView)
		return;

	if ( nNewView > entitylist->GetHighestEntityIndex() )
		return;

	cl.m_nViewEntity = nNewView;
	videomode->MarkClientViewRectDirty(); // Force recalculation
	ConMsg("View entity set to %i\n", nNewView);
}


static int CL_AllocLightFromArray( dlight_t *pLights, int lightCount, int key )
{
	int		i;

	// first look for an exact key match
	if (key)
	{
		for ( i = 0; i < lightCount; i++ )
		{
			if (pLights[i].key == key)
				return i;
		}
	}

	// then look for anything else
	for ( i = 0; i < lightCount; i++ )
	{
		if (pLights[i].die < cl.GetTime())
			return i;
	}

	return 0;
}

bool g_bActiveDlights = false;
bool g_bActiveElights = false;
/*
===============
CL_AllocDlight

===============
*/
dlight_t *CL_AllocDlight (int key)
{
	int i = CL_AllocLightFromArray( cl_dlights, MAX_DLIGHTS, key );
	dlight_t *dl = &cl_dlights[i];
	R_MarkDLightNotVisible( i );
	memset (dl, 0, sizeof(*dl));
	dl->key = key;
	r_dlightchanged |= (1 << i);
	r_dlightactive |= (1 << i);
	g_bActiveDlights = true;
	return dl;
}


/*
===============
CL_AllocElight

===============
*/
dlight_t *CL_AllocElight (int key)
{
	int i = CL_AllocLightFromArray( cl_elights, MAX_ELIGHTS, key );
	dlight_t *el = &cl_elights[i];
	memset (el, 0, sizeof(*el));
	el->key = key;
	g_bActiveElights = true;
	return el;
}


/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights (void)
{
	int			i;
	dlight_t	*dl;
	float		time;
	
	time = cl.GetFrameTime();
	if ( time <= 0.0f )
		return;

	g_bActiveDlights = false;
	g_bActiveElights = false;
	dl = cl_dlights;

	r_dlightchanged = 0;
	r_dlightactive = 0;

	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (!dl->IsRadiusGreaterThanZero())
		{
			R_MarkDLightNotVisible( i );
			continue;
		}

		if ( dl->die < cl.GetTime() )
		{
			r_dlightchanged |= (1 << i);
			dl->radius = 0;
		}
		else if (dl->decay)
		{
			r_dlightchanged |= (1 << i);

			dl->radius -= time*dl->decay;
			if (dl->radius < 0)
			{
				dl->radius = 0;
			}
		}

		if (dl->IsRadiusGreaterThanZero())
		{
			g_bActiveDlights = true;
			r_dlightactive |= (1 << i);
		}
		else
		{
			R_MarkDLightNotVisible( i );
		}
	}

	dl = cl_elights;
	for (i=0 ; i<MAX_ELIGHTS ; i++, dl++)
	{
		if (!dl->IsRadiusGreaterThanZero())
			continue;

		if (dl->die < cl.GetTime())
		{
			dl->radius = 0;
			continue;
		}
		
		dl->radius -= time*dl->decay;
		if (dl->radius < 0)
		{
			dl->radius = 0;
		}
		if ( dl->IsRadiusGreaterThanZero() )
		{
			g_bActiveElights = true;
		}
	}
}


void CL_ExtraMouseUpdate( float frametime )
{
	// Not ready for commands yet.
	if ( !cl.IsActive() )
		return;

	if ( !Host_ShouldRun() )
		return;

	// Don't create usercmds here during playback, they were encoded into the packet already
#if defined( REPLAY_ENABLED )
	if ( demoplayer->IsPlayingBack() && !cl.ishltv && !cl.isreplay )
		return;
#else
	if ( demoplayer->IsPlayingBack() && !cl.ishltv )
		return;
#endif

	// Have client .dll create and store usercmd structure
	g_ClientDLL->ExtraMouseSample( frametime, !cl.m_bPaused );
}

/*
=================
CL_SendMove

Constructs the movement command and sends it to the server if it's time.
=================
*/
void CL_SendMove( void )
{
#if defined( STAGING_ONLY ) || defined( _DEBUG )
	if ( cl_block_usercommand.GetBool() )
		return;
#endif // STAGING_ONLY || _DEBUG

	byte data[ MAX_CMD_BUFFER ];
	
	int nextcommandnr = cl.lastoutgoingcommand + cl.chokedcommands + 1;

	// send the client update packet

	CLC_Move moveMsg;

	moveMsg.m_DataOut.StartWriting( data, sizeof( data ) );

	// Determine number of backup commands to send along
	int cl_cmdbackup = 2;
	moveMsg.m_nBackupCommands = clamp( cl_cmdbackup, 0, MAX_BACKUP_COMMANDS );

	// How many real new commands have queued up
	moveMsg.m_nNewCommands = 1 + cl.chokedcommands;
	moveMsg.m_nNewCommands = clamp( moveMsg.m_nNewCommands, 0, MAX_NEW_COMMANDS );

	int numcmds = moveMsg.m_nNewCommands + moveMsg.m_nBackupCommands;

	int from = -1;	// first command is deltaed against zeros 

	bool bOK = true;

	for ( int to = nextcommandnr - numcmds + 1; to <= nextcommandnr; to++ )
	{
		bool isnewcmd = to >= (nextcommandnr - moveMsg.m_nNewCommands + 1);

		// first valid command number is 1
		bOK = bOK && g_ClientDLL->WriteUsercmdDeltaToBuffer( &moveMsg.m_DataOut, from, to, isnewcmd );
		from = to;
	}

	if ( bOK )
	{
		// only write message if all usercmds were written correctly, otherwise parsing would fail
		cl.m_NetChannel->SendNetMsg( moveMsg );
	}
}

void CL_Move(float accumulated_extra_samples, bool bFinalTick )
{
	if ( !cl.IsConnected() )
		return;

	if ( !Host_ShouldRun() )
		return;

	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s", __FUNCTION__ );

	// only send packets on the final tick in one engine frame
	bool bSendPacket = true;	

	// Don't create usercmds here during playback, they were encoded into the packet already
	if ( demoplayer->IsPlayingBack() )
	{
#if defined( REPLAY_ENABLED )
		if ( cl.ishltv || cl.isreplay )
#else
		if ( cl.ishltv )
#endif
		{
			// still do it when playing back a HLTV/replay demo
			bSendPacket = false;
		}
		else
		{
            return;
		}
	}

	// don't send packets if update time not reached or chnnel still sending
	// in loopback mode don't send only if host_limitlocal is enabled

	if ( ( !cl.m_NetChannel->IsLoopback() || host_limitlocal.GetInt() ) &&
		 ( ( net_time < cl.m_flNextCmdTime ) || !cl.m_NetChannel->CanPacket()  || !bFinalTick ) )
	{
		bSendPacket = false;
	}

	if ( cl.IsActive() )
	{
		VPROF( "CL_Move" );

		int nextcommandnr = cl.lastoutgoingcommand + cl.chokedcommands + 1;

		// Have client .dll create and store usercmd structure
		g_ClientDLL->CreateMove( 
			nextcommandnr, 
			host_state.interval_per_tick - accumulated_extra_samples,
			!cl.IsPaused() );

		// Store new usercmd to dem file
		if ( demorecorder->IsRecording() )
		{
			// Back up one because we've incremented outgoing_sequence each frame by 1 unit
			demorecorder->RecordUserInput( nextcommandnr );
		}

		if ( bSendPacket )
		{
			CL_SendMove();
		}
		else
		{
			// netchanll will increase internal outgoing sequnce number too
			cl.m_NetChannel->SetChoked();	
			// Mark command as held back so we'll send it next time
			cl.chokedcommands++;
		}
	}

	if ( !bSendPacket )
		return;

		// Request non delta compression if high packet loss, show warning message
	bool hasProblem = cl.m_NetChannel->IsTimingOut() && !demoplayer->IsPlayingBack() &&	cl.IsActive();

	// Request non delta compression if high packet loss, show warning message
	if ( hasProblem )
	{
		con_nprint_t np;
		np.time_to_live = 1.0;
		np.index = 2;
		np.fixed_width_font = false;
		np.color[ 0 ] = 1.0;
		np.color[ 1 ] = 0.2;
		np.color[ 2 ] = 0.2;
		
		float flTimeOut = cl.m_NetChannel->GetTimeoutSeconds();
		Assert( flTimeOut != -1.0f );
		float flRemainingTime = flTimeOut - cl.m_NetChannel->GetTimeSinceLastReceived();
		Con_NXPrintf( &np, "WARNING:  Connection Problem" );
		np.index = 3;
		Con_NXPrintf( &np, "Auto-disconnect in %.1f seconds", flRemainingTime );

		cl.ForceFullUpdate(); // sets m_nDeltaTick to -1
	}

	if ( cl.IsActive() )
	{
		NET_Tick mymsg( cl.m_nDeltaTick, host_frametime_unbounded, host_frametime_stddeviation );
		cl.m_NetChannel->SendNetMsg( mymsg );
	}

	//COM_Log( "cl.log", "Sending command number %i(%i) to server\n", cl.m_NetChan->m_nOutSequenceNr, cl.m_NetChan->m_nOutSequenceNr & CL_UPDATE_MASK );

	// Remember outgoing command that we are sending
	cl.lastoutgoingcommand = cl.m_NetChannel->SendDatagram( NULL );

	cl.chokedcommands = 0;

	// calc next packet send time

	if ( cl.IsActive() )
	{
		// use full update rate when active
		float commandInterval = 1.0f / cl_cmdrate->GetFloat();
		float maxDelta = min ( host_state.interval_per_tick, commandInterval );
		float delta = clamp( (float)(net_time - cl.m_flNextCmdTime), 0.0f, maxDelta );
		cl.m_flNextCmdTime = net_time + commandInterval - delta;
	}
	else
	{
		// during signon process send only 5 packets/second
		cl.m_flNextCmdTime = net_time + ( 1.0f / 5.0f );
	}

}

#define TICK_INTERVAL			(host_state.interval_per_tick)
#define ROUND_TO_TICKS( t )		( TICK_INTERVAL * TIME_TO_TICKS( t ) )

void CL_LatchInterpolationAmount()
{
	if ( !cl.IsConnected() )
		return;

	float dt = cl.m_NetChannel->GetTimeSinceLastReceived();
	float flClientInterpolationAmount = ROUND_TO_TICKS( cl.GetClientInterpAmount() );

	float flInterp = 0.0f;
	if ( flClientInterpolationAmount > 0.001 )
	{
		flInterp = clamp( dt / flClientInterpolationAmount, 0.0f, 3.0f );
	}
	cl.m_NetChannel->SetInterpolationAmount( flInterp );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pMessage - 
//-----------------------------------------------------------------------------
void CL_HudMessage( const char *pMessage )
{
	if ( g_ClientDLL )
	{
		g_ClientDLL->HudText( pMessage );
	}
}

CON_COMMAND_F( cl_showents, "Dump entity list to console.", FCVAR_CHEAT )
{
	for ( int i = 0; i < entitylist->GetMaxEntities(); i++ )
	{
		char entStr[256], classStr[256];
		IClientNetworkable *pEnt;

		if((pEnt = entitylist->GetClientNetworkable(i)) != NULL)
		{
			entStr[0] = 0;
			Q_snprintf(classStr, sizeof( classStr ), "'%s'", pEnt->GetClientClass()->m_pNetworkName);
		}
		else
		{
			Q_snprintf(entStr, sizeof( entStr ), "(missing), ");
			Q_snprintf(classStr, sizeof( classStr ), "(missing)");
		}

		if ( pEnt )
			ConMsg("Ent %3d: %s class %s\n", i, entStr, classStr);
	}
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the background level should be loaded on startup
//-----------------------------------------------------------------------------
bool CL_ShouldLoadBackgroundLevel( const CCommand &args )
{
	if ( InEditMode() )
		return false;

	// If TF2 and PC we don't want to load the background map.
	bool bIsTF2 = false;
	if ( ( Q_stricmp( COM_GetModDirectory(), "tf" ) == 0 ) || ( Q_stricmp( COM_GetModDirectory(), "tf_beta" ) == 0 ) )
	{
		bIsTF2 = true;
	}

	if ( bIsTF2 && IsPC() )
		return false;

	if ( args.ArgC() == 2 )
	{
		// presence of args identifies an end-of-game situation
		if ( IsX360() )
		{
			// 360 needs to get UI in the correct state to transition to the Background level
			// from the credits.
			EngineVGui()->OnCreditsFinished();
			return true;
		}

		if ( !Q_stricmp( args[1], "force" ) )
		{
			// Adrian: Have to do this so the menu shows up if we ever call this while in a level.
			Host_Disconnect( true );
			// pc can't get into background maps fast enough, so just show main menu
			return false;
		}

		if ( !Q_stricmp( args[1], "playendgamevid" ) )
		{
			// Bail back to the menu and play the end game video.
			CommandLine()->AppendParm( "-endgamevid", NULL ); 
			CommandLine()->RemoveParm( "-recapvid" );
			game->PlayStartupVideos();
			CommandLine()->RemoveParm( "-endgamevid" );
			cl.Disconnect( "Finished playing end game videos", true );
			return false;
		}

		if ( !Q_stricmp( args[1], "playrecapvid" ) )
		{
			// Bail back to the menu and play the recap video
			CommandLine()->AppendParm( "-recapvid", NULL ); 
			CommandLine()->RemoveParm( "-endgamevid" );
			HostState_Restart();
			return false;
		}
	}
	
	// if force is set, then always return true
	if (CommandLine()->CheckParm("-forcestartupmenu"))
		return true;

	// don't load the map in developer or console mode
	if ( developer.GetInt() || 
		CommandLine()->CheckParm("-console") || 
		CommandLine()->CheckParm("-dev") )
		return false;

	// don't load the map if we're going straight into a level
	if ( CommandLine()->CheckParm("+map") ||
		CommandLine()->CheckParm("+connect") ||
		CommandLine()->CheckParm("+playdemo") ||
		CommandLine()->CheckParm("+timedemo") ||
		CommandLine()->CheckParm("+timedemoquit") ||
		CommandLine()->CheckParm("+load") ||
		CommandLine()->CheckParm("-makereslists"))
		return false;

#ifdef _X360
	// check if we are accepting an invite
	if ( XboxLaunch()->GetLaunchFlags() & LF_INVITERESTART )
		return false;
#endif

	// nothing else is going on, so load the startup level

	return true;
}

#define DEFAULT_BACKGROUND_NAME	"background01"

int g_iRandomChapterIndex = -1;

int CL_GetBackgroundLevelIndex( int nNumChapters )
{
	if ( g_iRandomChapterIndex != -1 )
		return g_iRandomChapterIndex;

	int iChapterIndex = sv_unlockedchapters.GetInt();
	if ( iChapterIndex <= 0 )
	{
		// expected to be [1..N]
		iChapterIndex = 1;
	}

	if ( sv_unlockedchapters.GetInt() >= ( nNumChapters-1 ) )
	{
		RandomSeed( Plat_MSTime() );
		g_iRandomChapterIndex = iChapterIndex = RandomInt( 1, nNumChapters );
	}

	return iChapterIndex;
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the background level to load
//-----------------------------------------------------------------------------
void CL_GetBackgroundLevelName( char *pszBackgroundName, int bufSize, bool bMapName )
{
	Q_strncpy( pszBackgroundName, DEFAULT_BACKGROUND_NAME, bufSize );

	KeyValues *pChapterFile = new KeyValues( pszBackgroundName );

	if ( pChapterFile->LoadFromFile( g_pFileSystem, "scripts/ChapterBackgrounds.txt" ) )
	{
		KeyValues *pChapterRoot = pChapterFile;

		const char *szChapterIndex;
		int nNumChapters = 1;
		KeyValues *pChapters = pChapterFile->GetNextKey();
		if ( bMapName && pChapters )
		{
			const char *pszName = pChapters->GetName();
			if ( pszName && pszName[0] && !Q_strncmp( "BackgroundMaps", pszName, 14 ) )
			{
				pChapterRoot = pChapters;
				pChapters = pChapters->GetFirstSubKey();
			}
			else
			{
				pChapters = NULL;
			}
		}
		else
		{
			pChapters = NULL;
		}

		if ( !pChapters )
		{
			pChapters = pChapterFile->GetFirstSubKey();
		}

		// Find the highest indexed chapter
		while ( pChapters )
		{
			szChapterIndex = pChapters->GetName();

			if ( szChapterIndex )
			{
				int nChapter = atoi(szChapterIndex);

				if( nChapter > nNumChapters )
					nNumChapters = nChapter;
			}
			
			pChapters = pChapters->GetNextKey();
		}	

		int nChapterToLoad = CL_GetBackgroundLevelIndex( nNumChapters );

		// Find the chapter background with this index
		char buf[4];
		Q_snprintf( buf, sizeof(buf), "%d", nChapterToLoad );
		KeyValues *pLoadChapter = pChapterRoot->FindKey(buf);

		// Copy the background name
		if ( pLoadChapter )
		{
			Q_strncpy( pszBackgroundName, pLoadChapter->GetString(), bufSize );
		}
	}

	pChapterFile->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Callback to open the game menus
//-----------------------------------------------------------------------------
void CL_CheckToDisplayStartupMenus( const CCommand &args )
{
	if ( CL_ShouldLoadBackgroundLevel( args ) )
	{
		char szBackgroundName[_MAX_PATH];
		CL_GetBackgroundLevelName( szBackgroundName, sizeof(szBackgroundName), true );

		char cmd[_MAX_PATH];
		Q_snprintf( cmd, sizeof(cmd), "map_background %s\n", szBackgroundName );
		Cbuf_AddText( cmd );
	}
}

static float s_fDemoRevealGameUITime = -1;
float s_fDemoPlayMusicTime = -1;
static bool s_bIsRavenHolmn = false;
//-----------------------------------------------------------------------------
// Purpose: run the special demo logic when transitioning from the trainstation levels
//----------------------------------------------------------------------------
void CL_DemoTransitionFromTrainstation()
{
	// kick them out to GameUI instead and bring up the chapter page with raveholm unlocked
	sv_unlockedchapters.SetValue(6); // unlock ravenholm
	Cbuf_AddText( "sv_cheats 1; fadeout 1.5; sv_cheats 0;");
	Cbuf_Execute();
	s_fDemoRevealGameUITime = Sys_FloatTime() + 1.5;
	s_bIsRavenHolmn = false;
}

void CL_DemoTransitionFromRavenholm()
{
	Cbuf_AddText( "sv_cheats 1; fadeout 2; sv_cheats 0;");
	Cbuf_Execute();
	s_fDemoRevealGameUITime = Sys_FloatTime() + 1.9;
	s_bIsRavenHolmn = true;
}

void CL_DemoTransitionFromTestChmb()
{
	Cbuf_AddText( "sv_cheats 1; fadeout 2; sv_cheats 0;");
	Cbuf_Execute();
	s_fDemoRevealGameUITime = Sys_FloatTime() + 1.9;	
}


//-----------------------------------------------------------------------------
// Purpose: make the gameui appear after a certain interval
//----------------------------------------------------------------------------
void V_RenderVGuiOnly();
bool V_CheckGamma();
void CL_DemoCheckGameUIRevealTime( ) 
{
	if ( s_fDemoRevealGameUITime > 0 )
	{
		if ( s_fDemoRevealGameUITime < Sys_FloatTime() )
		{
			s_fDemoRevealGameUITime = -1;

			SCR_BeginLoadingPlaque();
			Cbuf_AddText( "disconnect;");

			CCommand args;
			CL_CheckToDisplayStartupMenus( args );

			s_fDemoPlayMusicTime = Sys_FloatTime() + 1.0;
		}
	}

	if ( s_fDemoPlayMusicTime > 0 )
	{
		V_CheckGamma();
		V_RenderVGuiOnly();
		if ( s_fDemoPlayMusicTime < Sys_FloatTime() )
		{
			s_fDemoPlayMusicTime = -1;
			EngineVGui()->ActivateGameUI();

			if ( CL_IsHL2Demo() )
			{
				if ( s_bIsRavenHolmn )
				{
					Cbuf_AddText( "play music/ravenholm_1.mp3;" );
				}
				else
				{
					EngineVGui()->ShowNewGameDialog(6);// bring up the new game dialog in game UI
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: setup a debug string that is uploaded on crash
//----------------------------------------------------------------------------
extern bool g_bV3SteamInterface;
char g_minidumpinfo[ 4096 ] = {0};
PAGED_POOL_INFO_t g_pagedpoolinfo = { 0 };
void DisplaySystemVersion( char *osversion, int maxlen );

void CL_SetPagedPoolInfo()
{
	if ( IsX360() )
		return;
#if !defined( _X360 ) && !defined(NO_STEAM) && !defined(SWDS)
	Plat_GetPagedPoolInfo( &g_pagedpoolinfo );
#endif
}

void CL_SetSteamCrashComment()
{
	if ( IsX360() )
		return;

	char map[ 80 ];
	char videoinfo[ 2048 ];
	char misc[ 256 ];
	char driverinfo[ 2048 ];
	char osversion[ 256 ];

	map[ 0 ] = 0;
	driverinfo[ 0 ] = 0;
	videoinfo[ 0 ] = 0;
	misc[ 0 ] = 0;
	osversion[ 0 ] = 0;

	if ( host_state.worldmodel )
	{
		CL_SetupMapName( modelloader->GetName( host_state.worldmodel ), map, sizeof( map ) );
	}

	DisplaySystemVersion( osversion, sizeof( osversion ) );

	MaterialAdapterInfo_t info;
	materials->GetDisplayAdapterInfo( materials->GetCurrentAdapter(), info );

	const char *dxlevel = "Unk";
	int nDxLevel = g_pMaterialSystemHardwareConfig->GetDXSupportLevel();
	if ( g_pMaterialSystemHardwareConfig )
	{
		dxlevel = COM_DXLevelToString( nDxLevel ) ;
	}

	// Make a string out of the high part and low parts of driver version
	char szDXDriverVersion[ 64 ];
	Q_snprintf( szDXDriverVersion, sizeof( szDXDriverVersion ), "%ld.%ld.%ld.%ld", 
		( long )( info.m_nDriverVersionHigh>>16 ), 
		( long )( info.m_nDriverVersionHigh & 0xffff ), 
		( long )( info.m_nDriverVersionLow>>16 ), 
		( long )( info.m_nDriverVersionLow & 0xffff ) );

	Q_snprintf( driverinfo, sizeof(driverinfo), "Driver Name:  %s\nDriver Version: %s\nVendorId / DeviceId:  0x%x / 0x%x\nSubSystem / Rev:  0x%x / 0x%x\nDXLevel:  %s [%d]\nVid:  %i x %i",
		info.m_pDriverName,
		szDXDriverVersion,
		info.m_VendorID,
		info.m_DeviceID,
		info.m_SubSysID,
		info.m_Revision,
		dxlevel ? dxlevel : "Unk", nDxLevel, 
		videomode->GetModeWidth(), videomode->GetModeHeight() );

	ConVarRef mat_picmip( "mat_picmip" );
	ConVarRef mat_forceaniso( "mat_forceaniso" );
	ConVarRef mat_trilinear( "mat_trilinear" );
	ConVarRef mat_antialias( "mat_antialias" );
	ConVarRef mat_aaquality( "mat_aaquality" );
	ConVarRef r_shadowrendertotexture( "r_shadowrendertotexture" );
	ConVarRef r_flashlightdepthtexture( "r_flashlightdepthtexture" );
#ifndef _X360
	ConVarRef r_waterforceexpensive( "r_waterforceexpensive" );
#endif
		ConVarRef r_waterforcereflectentities( "r_waterforcereflectentities" );
		ConVarRef mat_vsync( "mat_vsync" );
		ConVarRef r_rootlod( "r_rootlod" );
		ConVarRef mat_reducefillrate( "mat_reducefillrate" );
		ConVarRef mat_motion_blur_enabled( "mat_motion_blur_enabled" );
		ConVarRef mat_queue_mode( "mat_queue_mode" );

#ifdef _X360
	Q_snprintf( videoinfo, sizeof(videoinfo), "picmip: %i forceansio: %i trilinear: %i antialias: %i vsync: %i rootlod: %i reducefillrate: %i\n"\
		"shadowrendertotexture: %i r_flashlightdepthtexture %i waterforcereflectentities: %i mat_motion_blur_enabled: %i",
										mat_picmip.GetInt(), mat_forceaniso.GetInt(), mat_trilinear.GetInt(), mat_antialias.GetInt(), mat_aaquality.GetInt(),
										mat_vsync.GetInt(), r_rootlod.GetInt(), mat_reducefillrate.GetInt(), 
										r_shadowrendertotexture.GetInt(), r_flashlightdepthtexture.GetInt(),
										r_waterforcereflectentities.GetInt(),
										mat_motion_blur_enabled.GetInt() );
#else
		Q_snprintf( videoinfo, sizeof(videoinfo), "picmip: %i forceansio: %i trilinear: %i antialias: %i vsync: %i rootlod: %i reducefillrate: %i\n"\
			"shadowrendertotexture: %i r_flashlightdepthtexture %i waterforceexpensive: %i waterforcereflectentities: %i mat_motion_blur_enabled: %i mat_queue_mode %i",
											mat_picmip.GetInt(), mat_forceaniso.GetInt(), mat_trilinear.GetInt(), mat_antialias.GetInt(), 
											mat_vsync.GetInt(), r_rootlod.GetInt(), mat_reducefillrate.GetInt(), 
											r_shadowrendertotexture.GetInt(), r_flashlightdepthtexture.GetInt(),
											r_waterforceexpensive.GetInt(), r_waterforcereflectentities.GetInt(),
											mat_motion_blur_enabled.GetInt(), mat_queue_mode.GetInt() );
#endif
	int latency = 0;
	if ( cl.m_NetChannel )
	{
		latency = (int)( 1000.0f * cl.m_NetChannel->GetAvgLatency( FLOW_OUTGOING ) );
	}

	Q_snprintf( misc, sizeof( misc ), "skill:%i rate %i update %i cmd %i latency %i msec", 
		skill.GetInt(),
		cl_rate->GetInt(),
		(int)cl_updaterate->GetFloat(),
		(int)cl_cmdrate->GetFloat(),
		latency
	);

	const char *pNetChannel = "Not Connected";
	if ( cl.m_NetChannel )
	{
		pNetChannel = cl.m_NetChannel->GetRemoteAddress().ToString();
	}

		CL_SetPagedPoolInfo();

		Q_snprintf( g_minidumpinfo, sizeof(g_minidumpinfo),
				"Map: %s\n"\
				"Game: %s\n"\
				"Build: %i\n"\
				"Misc: %s\n"\
				"Net: %s\n"\
				"cmdline:%s\n"\
				"driver: %s\n"\
				"video: %s\n"\
				"OS: %s\n",
				map, com_gamedir, build_number(), misc, pNetChannel, CommandLine()->GetCmdLine(), driverinfo, videoinfo, osversion );

		char full[ 4096 ];
		Q_snprintf( full, sizeof( full ), "%sPP PAGES: used: %d, free %d\n", g_minidumpinfo, (int)g_pagedpoolinfo.numPagesUsed, (int)g_pagedpoolinfo.numPagesFree );

#ifndef NO_STEAM
	SteamAPI_SetMiniDumpComment( full );
#endif
}


//
// register commands
//
static ConCommand startupmenu( "startupmenu", &CL_CheckToDisplayStartupMenus, "Opens initial menu screen and loads the background bsp, but only if no other level is being loaded, and we're not in developer mode." );

ConVar cl_language( "cl_language", "english", FCVAR_USERINFO, "Language (from HKCU\\Software\\Valve\\Steam\\Language)" );
void CL_InitLanguageCvar()
{
	if ( Steam3Client().SteamApps() )
	{
		cl_language.SetValue( Steam3Client().SteamApps()->GetCurrentGameLanguage() );
	}
	else
	{
		cl_language.SetValue( "english" );
	}
}

void CL_ChangeCloudSettingsCvar( IConVar *var, const char *pOldValue, float flOldValue );
ConVar cl_cloud_settings( "cl_cloud_settings", "1", FCVAR_HIDDEN, "Cloud enabled from (from HKCU\\Software\\Valve\\Steam\\Apps\\appid\\Cloud)", CL_ChangeCloudSettingsCvar );
void CL_ChangeCloudSettingsCvar( IConVar *var, const char *pOldValue, float flOldValue )
{
	// !! bug do i need to do something linux-wise here.
	if ( IsPC() && Steam3Client().SteamRemoteStorage() )
	{
		ConVarRef ref( var->GetName() );
		Steam3Client().SteamRemoteStorage()->SetCloudEnabledForApp( ref.GetBool() );
		if ( cl_cloud_settings.GetInt() == STEAMREMOTESTORAGE_CLOUD_ON && flOldValue == STEAMREMOTESTORAGE_CLOUD_OFF )
		{
			// If we were just turned on, get our configuration from remote storage.
			engineClient->ReadConfiguration( false );
			engineClient->ClientCmd_Unrestricted( "refresh_options_dialog" );
		}

	}
}

void CL_InitCloudSettingsCvar()
{
	if ( IsPC()	&& Steam3Client().SteamRemoteStorage() )
	{
		int iCloudSettings = STEAMREMOTESTORAGE_CLOUD_OFF;
		if ( Steam3Client().SteamRemoteStorage()->IsCloudEnabledForApp() )
			iCloudSettings = STEAMREMOTESTORAGE_CLOUD_ON;
		
		cl_cloud_settings.SetValue( iCloudSettings );
	}
	else
	{
		// If not on PC or steam not available, set to 0 to make sure no replication occurs or is attempted
		cl_cloud_settings.SetValue( STEAMREMOTESTORAGE_CLOUD_OFF );
	}
}


/*
=================
CL_Init
=================
*/
void CL_Init (void)
{	
	cl.Clear();
	
	CL_InitLanguageCvar();
	CL_InitCloudSettingsCvar();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CL_Shutdown( void )
{
}

CON_COMMAND_F( cl_fullupdate, "Forces the server to send a full update packet", FCVAR_CHEAT )
{
	cl.ForceFullUpdate();
}


#ifdef STAGING_ONLY

CON_COMMAND( cl_download, "Downloads a file from server." )
{
	if ( args.ArgC() != 2 )
		return;

	if ( !cl.m_NetChannel )
		return;
	
	cl.m_NetChannel->RequestFile( args[ 1 ] ); // just for testing stuff
}

#endif // STAGING_ONLY


CON_COMMAND_F( setinfo, "Adds a new user info value", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( args.ArgC() != 3 )
	{
		Msg("Syntax: setinfo <key> <value>\n");
		return;
	}

	const char *name = args[ 1 ];
	const char *value = args[ 2 ];

	// Prevent players manually changing their name (their Steam account provides it now)
	if ( Q_stricmp( name, "name" ) == 0 )
		return;

	// Discard any convar change request if contains funky characters
	bool bFunky = false;
	for (const char *s = name ; *s != '\0' ; ++s )
	{
		if ( !V_isalnum(*s) && *s != '_' )
		{
			bFunky = true;
			break;
		}
	}
	if ( bFunky )
	{
		Msg( "Ignoring convar change request for variable '%s', which contains invalid character(s)\n", name );
		return;
	}

	ConCommandBase *pCommand = g_pCVar->FindCommandBase( name );

	ConVarRef sv_cheats( "sv_cheats" );

	if ( pCommand )
	{
		if ( pCommand->IsCommand() )		
		{
			Msg("Name %s is already registered as console command\n", name );
			return;
		}

		if ( !pCommand->IsFlagSet(FCVAR_USERINFO) )
		{
			Msg("Convar %s is already registered but not as user info value\n", name );
			return;
		}

		if ( pCommand->IsFlagSet( FCVAR_NOT_CONNECTED ) )
		{
#ifndef DEDICATED
			// Connected to server?
			if ( cl.IsConnected() )
			{
				extern IBaseClientDLL *g_ClientDLL;
				if ( pCommand->IsFlagSet( FCVAR_USERINFO ) && g_ClientDLL && g_ClientDLL->IsConnectedUserInfoChangeAllowed( NULL ) )
				{
					// Client.dll is allowing the convar change
				}
				else
				{
					ConMsg( "Can't change %s when playing, disconnect from the server or switch team to spectators\n", pCommand->GetName() );
					return;
				}
			}
#endif
		}

		if ( IsPC() )
		{
#if !defined(NO_STEAM)
			EUniverse eUniverse = GetSteamUniverse();
			if ( (( eUniverse != k_EUniverseBeta ) && ( eUniverse != k_EUniverseDev )) && pCommand->IsFlagSet( FCVAR_DEVELOPMENTONLY ) )
				return;
#endif 
		}

		if ( pCommand->IsFlagSet( FCVAR_CHEAT ) && sv_cheats.GetBool() == 0  )
		{
			Msg("Convar %s is marked as cheat and cheats are off\n", name );
			return;
		}
	}
	else
	{
		// cvar not found, create it now
		char *pszString = V_strdup( name );

		pCommand = new ConVar( pszString, "", FCVAR_USERINFO, "Custom user info value" );
	}

	ConVar *pConVar = (ConVar*)pCommand;

	pConVar->SetValue( value );

	if ( cl.IsConnected() )
	{
		// send changed cvar to server
		NET_SetConVar convar( name, value );
		cl.m_NetChannel->SendNetMsg( convar );
	}
}


CON_COMMAND( cl_precacheinfo, "Show precache info (client)." )
{
	if ( args.ArgC() == 2 )
	{
		cl.DumpPrecacheStats( args[ 1 ] );
		return;
	}
	
	// Show all data
	cl.DumpPrecacheStats( MODEL_PRECACHE_TABLENAME );
	cl.DumpPrecacheStats( DECAL_PRECACHE_TABLENAME );
	cl.DumpPrecacheStats( SOUND_PRECACHE_TABLENAME );
	cl.DumpPrecacheStats( GENERIC_PRECACHE_TABLENAME );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *object - 
//			stringTable - 
//			stringNumber - 
//			*newString - 
//			*newData - 
//-----------------------------------------------------------------------------
void Callback_ModelChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData )
{
	if ( stringTable == cl.m_pModelPrecacheTable )
	{
		// Index 0 is always NULL, just ignore it
		// Index 1 == the world, don't 
		if ( stringNumber > 1 )
		{
//			DevMsg( "Preloading model %s\n", newString );
			cl.SetModel( stringNumber );
		}
	}
	else
	{
		Assert( 0 ) ; // Callback_*Changed called with wrong stringtable
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *object - 
//			stringTable - 
//			stringNumber - 
//			*newString - 
//			*newData - 
//-----------------------------------------------------------------------------
void Callback_GenericChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData )
{
	if ( stringTable == cl.m_pGenericPrecacheTable )
	{
		// Index 0 is always NULL, just ignore it
		if ( stringNumber >= 1 )
		{
			cl.SetGeneric( stringNumber );
		}
	}
	else
	{
		Assert( 0 ) ; // Callback_*Changed called with wrong stringtable
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *object - 
//			stringTable - 
//			stringNumber - 
//			*newString - 
//			*newData - 
//-----------------------------------------------------------------------------
void Callback_SoundChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData )
{
	if ( stringTable == cl.m_pSoundPrecacheTable )
	{
		// Index 0 is always NULL, just ignore it
		if ( stringNumber >= 1 )
		{
			cl.SetSound( stringNumber );
		}
	}
	else
	{
		Assert( 0 ) ; // Callback_*Changed called with wrong stringtable
	}
}

void Callback_DecalChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData )
{
	if ( stringTable == cl.m_pDecalPrecacheTable )
	{
		cl.SetDecal( stringNumber );
	}
	else
	{
		Assert( 0 ) ; // Callback_*Changed called with wrong stringtable
	}
}


void Callback_InstanceBaselineChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData )
{
	Assert( stringTable == cl.m_pInstanceBaselineTable );
	// cl.UpdateInstanceBaseline( stringNumber );
}

void Callback_UserInfoChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData )
{
	Assert( stringTable == cl.m_pUserInfoTable );

	// stringnumber == player slot

	player_info_t *player = (player_info_t*)newData;

	if ( !player )
		return; // player left the game

	// request custom user files if necessary
	for ( int i=0; i<MAX_CUSTOM_FILES; i++ )
	{
		cl.CheckOthersCustomFile( player->customFiles[i] );
	}

	// fire local client event game event
	IGameEvent * event = g_GameEventManager.CreateEvent( "player_info" );

	if ( event )
	{
		event->SetInt( "userid", player->userID );
		event->SetInt( "friendsid", player->friendsID );
		event->SetInt( "index", stringNumber );
		event->SetString( "name", player->name );
		event->SetString( "networkid", player->guid );
		event->SetBool( "bot", player->fakeplayer );

		g_GameEventManager.FireEventClientSide( event );
	}
}

void Callback_DynamicModelsChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData )
{
#ifndef SWDS
	extern IVModelInfoClient *modelinfoclient;
	if ( modelinfoclient )
	{
		modelinfoclient->OnDynamicModelsStringTableChange( stringNumber, newString, newData );
	}
#endif
}

void CL_HookClientStringTables()
{
	// install hooks
	int numTables = cl.m_StringTableContainer->GetNumTables();

	for ( int i =0; i<numTables; i++)
	{
		// iterate through server tables
		CNetworkStringTable *pTable = 
			(CNetworkStringTable*)cl.m_StringTableContainer->GetTable( i );

		if ( !pTable )
			continue;

		cl.HookClientStringTable( pTable->GetTableName() );
	}
}
// Installs the all, and invokes cb for all existing items
void CL_InstallAndInvokeClientStringTableCallbacks()
{
	// install hooks
	int numTables = cl.m_StringTableContainer->GetNumTables();

	for ( int i =0; i<numTables; i++)
	{
		// iterate through server tables
		CNetworkStringTable *pTable = 
			(CNetworkStringTable*)cl.m_StringTableContainer->GetTable( i );

		if ( !pTable )
			continue;

		pfnStringChanged pOldFunction = pTable->GetCallback();

		cl.InstallStringTableCallback( pTable->GetTableName() );

		pfnStringChanged pNewFunction = pTable->GetCallback();
		if ( !pNewFunction )
			continue;

		// We already had it installed (e.g., from client .dll) so all of the callbacks have been called and don't need a second dose
		if ( pNewFunction == pOldFunction )
			continue;

		for ( int j = 0; j < pTable->GetNumStrings(); ++j )
		{
			int userDataSize;
			const void *pUserData = pTable->GetStringUserData( j, &userDataSize );
			(*pNewFunction)( NULL, pTable, j, pTable->GetString( j ), pUserData );
		}
	}
}

// Singleton client state
CClientState	cl;
