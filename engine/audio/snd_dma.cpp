//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Main control for any streaming sound output device.
//
//===========================================================================//

#include "audio_pch.h"
#include "const.h"
#include "cdll_int.h"
#include "client_class.h"
#include "icliententitylist.h"
#include "tier0/vcrmode.h"
#include "con_nprint.h"
#include "tier0/icommandline.h"
#include "vox_private.h"
#include "../../traceinit.h"
#include "../../cmd.h"
#include "toolframework/itoolframework.h"
#include "vstdlib/random.h"
#include "vstdlib/jobthread.h"
#include "vaudio/ivaudio.h"
#include "../../client.h"
#include "../../cl_main.h"
#include "utldict.h"
#include "mempool.h"
#include "../../enginetrace.h"			// for traceline
#include "../../public/bspflags.h"		// for traceline
#include "../../public/gametrace.h"		// for traceline
#include "vphysics_interface.h"		// for surface props
#include "../../ispatialpartitioninternal.h"	// for entity enumerator
#include "../../debugoverlay.h"
#include "icliententity.h"
#include "../../cmodel_engine.h"
#include "../../staticpropmgr.h"
#include "../../server.h"
#include "edict.h"
#include "../../pure_server.h"
#include "filesystem/IQueuedLoader.h"
#include "voice.h"
#if defined( _X360 )
#include "xbox/xbox_console.h"
#include "xmp.h"
#endif

#include "replay/iclientreplaycontext.h"
#include "replay/ireplaymovierenderer.h"

#include "video/ivideoservices.h"
extern IVideoServices *g_pVideo;

/*
#include "gl_model_private.h"
#include "world.h"
#include "vphysics_interface.h"
#include "client_class.h"
#include "server_class.h"
*/

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

///////////////////////////////////
// DEBUGGING
//
// Turn this on to print channel output msgs.
//
//#define DEBUG_CHANNELS

#define SNDLVL_TO_DIST_MULT( sndlvl ) ( sndlvl ? ((pow( 10.0f, snd_refdb.GetFloat() / 20 ) / pow( 10.0f, (float)sndlvl / 20 )) / snd_refdist.GetFloat()) : 0 )
#define DIST_MULT_TO_SNDLVL( dist_mult ) (soundlevel_t)(int)( dist_mult ? ( 20 * log10( pow( 10.0f, snd_refdb.GetFloat() / 20 ) / (dist_mult * snd_refdist.GetFloat()) ) ) : 0 )

extern ConVar dsp_spatial;
extern IPhysicsSurfaceProps	*physprop;

extern bool IsReplayRendering();

static void S_Play( const CCommand &args );
static void S_PlayVol( const CCommand &args );
void S_SoundList(void);
static void S_Say ( const CCommand &args );
void S_Update_(float);
void S_StopAllSounds(bool clear);
void S_StopAllSoundsC(void);
void S_ShutdownMixThread();
const char *GetClientClassname( SoundSource soundsource );

float SND_GetGainObscured( channel_t *ch, bool fplayersound, bool flooping, bool bAttenuated );
void DSP_ChangePresetValue( int idsp, int channel, int iproc, float value );
bool DSP_CheckDspAutoEnabled( void );
void DSP_SetDspAuto( int dsp_preset );
float dB_To_Radius ( float db );
int dsp_room_GetInt ( void );

bool MXR_LoadAllSoundMixers( void );
void MXR_ReleaseMemory( void );
int MXR_GetMixGroupListFromDirName( const char *pDirname, byte *pList, int listMax );
void MXR_GetMixGroupFromSoundsource( channel_t *pchan, SoundSource soundsource,  soundlevel_t soundlevel);
float MXR_GetVolFromMixGroup( int rgmixgroupid[8], int *plast_mixgroupid );
char *MXR_GetGroupnameFromId( int mixgroupid );
int MXR_GetMixgroupFromName( const char *pszgroupname );
void MXR_DebugShowMixVolumes( void );
#ifdef _DEBUG
static void MXR_DebugSetMixGroupVolume( const CCommand &args );
#endif //_DEBUG
void MXR_UpdateAllDuckerVolumes( void );

void ChannelSetVolTargets( channel_t *pch, int *pvolumes, int ivol_offset, int cvol );
void ChannelUpdateVolXfade( channel_t *pch );
void ChannelClearVolumes( channel_t *pch );
float VOX_GetChanVol(channel_t *ch);
void ConvertListenerVectorTo2D( Vector *pvforward, Vector *pvright );
int ChannelGetMaxVol( channel_t *pch );

// Forceably ends voice tweak mode (only occurs during snd_restart
void VoiceTweak_EndVoiceTweakMode();
bool VoiceTweak_IsStillTweaking();
// Only does anything for voice tweak channel so if view entity changes it doesn't fade out to zero volume
void Voice_Spatialize( channel_t *channel );

// =======================================================================
// Internal sound data & structures
// =======================================================================

channel_t   channels[MAX_CHANNELS];

int	total_channels;
CActiveChannels g_ActiveChannels;
static double g_LastSoundFrame = 0.0f;		// last full frame of sound
static double g_LastMixTime = 0.0f;			// last time we did mixing
static float g_EstFrameTime = 0.1f;			// estimated frame time running average

// x360 override to fade out game music when the user is playing music through the dashboard
static float g_DashboardMusicMixValue = 1.0f;
static float g_DashboardMusicMixTarget = 1.0f;
const float g_DashboardMusicFadeRate = 0.5f;	// Fades one half full-scale volume per second (two seconds for complete fadeout)

// sound mixers
int g_csoundmixers	= 0;					// total number of soundmixers found
int g_cgrouprules	= 0;					// total number of group rules found
int g_cgroupclass	= 0;

// this is used to enable/disable music playback on x360 when the user selects his own soundtrack to play
void S_EnableMusic( bool bEnable )
{
	if ( bEnable )
	{
		g_DashboardMusicMixTarget = 1.0f;
	}
	else
	{
		g_DashboardMusicMixTarget = 0.0f;
	}
}

bool IsSoundSourceLocalPlayer( int soundsource )
{
	if ( soundsource == SOUND_FROM_UI_PANEL )
		return true;

	return ( soundsource == g_pSoundServices->GetViewEntity() );
}

CThreadMutex g_SndMutex;

#define THREAD_LOCK_SOUND() AUTO_LOCK( g_SndMutex )

const int MASK_BLOCK_AUDIO = CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW;

void CActiveChannels::Add( channel_t *pChannel )
{
	Assert( pChannel->activeIndex == 0 );
	m_list[m_count] = pChannel - channels;
	m_count++;
	pChannel->activeIndex = m_count;
}

void CActiveChannels::Remove( channel_t *pChannel )
{
	if ( pChannel->activeIndex == 0 )
		return;
	int activeIndex = pChannel->activeIndex - 1;
	Assert( activeIndex >= 0 && activeIndex < m_count );
	Assert( pChannel == &channels[m_list[activeIndex]] );
	m_count--;
	// Not the last one?  Swap the last one with this one and fix its index
	if ( activeIndex < m_count )
	{
		m_list[activeIndex] = m_list[m_count];
		channels[m_list[activeIndex]].activeIndex = activeIndex+1;
	}
	pChannel->activeIndex = 0;
}


void CActiveChannels::GetActiveChannels( CChannelList &list )
{
	list.m_count = m_count;
	if ( m_count )
	{
		Q_memcpy( list.m_list, m_list, sizeof(m_list[0])*m_count );
	}

	for ( int i = SOUND_BUFFER_SPECIAL_START; i < g_paintBuffers.Count(); ++i )
	{
		paintbuffer_t *pSpecialBuffer = MIX_GetPPaintFromIPaint( i );
		if ( pSpecialBuffer->nSpecialDSP != 0 )
		{
			list.m_nSpecialDSPs.AddToTail( pSpecialBuffer->nSpecialDSP );
		}
	}

	list.m_hasSpeakerChannels = true;
	list.m_has11kChannels = true;
	list.m_has22kChannels = true;
	list.m_has44kChannels = true;
	list.m_hasDryChannels = true;
}

void CActiveChannels::Init()
{
	m_count = 0;
}

bool				snd_initialized = false;

Vector				listener_origin;
static Vector		listener_forward;
Vector				listener_right;
static Vector		listener_up;
static bool			s_bIsListenerUnderwater;
static vec_t		sound_nominal_clip_dist=SOUND_NORMAL_CLIP_DIST;

// @TODO (toml 05-08-02): put this somewhere more reasonable
vec_t S_GetNominalClipDist()
{
	return sound_nominal_clip_dist;
}

int				g_soundtime = 0;		// sample PAIRS output since start
int   			g_paintedtime = 0; 		// sample PAIRS mixed since start

float			g_ReplaySoundTimeFracAccumulator = 0.0f;	// Used by replay

float			g_ClockSyncArray[NUM_CLOCK_SYNCS] = {0};
int				g_SoundClockPaintTime[NUM_CLOCK_SYNCS] = {0};

// default 10ms
ConVar snd_delay_sound_shift("snd_delay_sound_shift","0.01");
// this forces the clock to resync on the next delayed/sync sound
void S_SyncClockAdjust( clocksync_index_t syncIndex )
{
	g_ClockSyncArray[syncIndex] = 0;
	g_SoundClockPaintTime[syncIndex] = 0;
}

float S_ComputeDelayForSoundtime( float soundtime, clocksync_index_t syncIndex )
{
	// reset clock and return 0
	if ( g_ClockSyncArray[syncIndex] == 0 )
	{
		// Put the current time marker one tick back to impose a minimum delay on the first sample
		// this shifts the drift over so the sounds are more likely to delay (rather than skip)
		// over the burst
		// NOTE: The first sound after a sync MUST have a non-zero delay for the delay channel
		// detection logic to work (otherwise we keep resetting the clock)
		g_ClockSyncArray[syncIndex] = soundtime - host_state.interval_per_tick;
		g_SoundClockPaintTime[syncIndex] = g_paintedtime;
	}

	// how much time has passed in the game since we did a clock sync?
	float gameDeltaTime = soundtime - g_ClockSyncArray[syncIndex];

	// how many samples have been mixed since we did a clock sync?
	int paintedSamples = g_paintedtime - g_SoundClockPaintTime[syncIndex];
	int dmaSpeed = g_AudioDevice->DeviceDmaSpeed();
	int gameSamples = (gameDeltaTime * dmaSpeed);
	int delaySamples = gameSamples - paintedSamples;
	float delay = delaySamples / float(dmaSpeed);

	if ( gameDeltaTime < 0 || fabs(delay) > 0.500f )
	{
		// Note that the equations assume a correlation between game time and real time
		// some kind of clock error.  This can happen with large host_timescale or when the 
		// framerate hitches drastically (game time is a smaller clamped value wrt real time).  
		// The current sync estimate has probably drifted due to this or some other problem, recompute.
		//Msg("Clock ERROR!: %.2f %.2f\n", gameDeltaTime, delay);
		S_SyncClockAdjust(syncIndex);
		return 0;
	}
	return delay + snd_delay_sound_shift.GetFloat();
}

static	int		s_buffers = 0;
static	int		s_oldsampleOutCount = 0;
static	float	s_lastsoundtime = 0.0f;

bool s_bOnLoadScreen = false;

static CClassMemoryPool< CSfxTable > s_SoundPool( MAX_SFX );
struct SfxDictEntry
{
	CSfxTable *pSfx;
};

static CUtlMap< FileNameHandle_t, SfxDictEntry > s_Sounds( 0, 0, DefLessFunc( FileNameHandle_t ) );

class CDummySfx : public CSfxTable
{
public:
	virtual const char *getname()
	{
		return name;
	}

	void setname( const char *pName )
	{
		Q_strncpy( name, pName, sizeof( name ) );
		OnNameChanged(name);
	}

private:
	char name[MAX_PATH];
};

static CDummySfx dummySfx;

// returns true if ok to procede with TraceRay calls
bool SND_IsInGame( void )
{
	return cl.IsActive();
}


CSfxTable::CSfxTable()
{
	m_namePoolIndex = s_Sounds.InvalidIndex();
	pSource = NULL;
	m_bUseErrorFilename = false;
	m_bIsUISound = false;
	m_bIsLateLoad = false;
	m_bMixGroupsCached = false;
	m_pDebugName = NULL;
}


void CSfxTable::SetNamePoolIndex( int index )
{
	m_namePoolIndex = index;
	if ( m_namePoolIndex != s_Sounds.InvalidIndex() )
	{
		OnNameChanged(getname());
	}
#ifdef _DEBUG
	m_pDebugName = strdup( getname() );
#endif
}

void CSfxTable::OnNameChanged( const char *pName )
{
	if ( pName && g_cgrouprules )
	{
		char szString[MAX_PATH];
		Q_strncpy( szString, pName, sizeof(szString) );
		Q_FixSlashes( szString, '/' );
		m_mixGroupCount = MXR_GetMixGroupListFromDirName( szString, m_mixGroupList, ARRAYSIZE(m_mixGroupList) );
		m_bMixGroupsCached = true;
	}
}
//-----------------------------------------------------------------------------
// Purpose: Wrapper for sfxtable->getname()
// Output : char const
//-----------------------------------------------------------------------------
const char *CSfxTable::getname()
{
	if ( s_Sounds.InvalidIndex() != m_namePoolIndex )
	{
		char* pString = tmpstr512();
		if ( g_pFileSystem )
			g_pFileSystem->String( s_Sounds.Key( m_namePoolIndex ), pString, 512 );
		else 
		{
			pString[0] = 0;
		}
		return pString;
	}
	
	return NULL;
}

FileNameHandle_t CSfxTable::GetFileNameHandle()
{
	if ( s_Sounds.InvalidIndex() != m_namePoolIndex )
	{
		return s_Sounds.Key( m_namePoolIndex );
	}
	return NULL;
}

const char *CSfxTable::GetFileName()
{
	if ( IsX360() && m_bUseErrorFilename )
	{
		// Redirecting error sounds to a valid empty wave, prevents a bad loading retry pattern during gameplay
		// which may event sounds skipped by preload, because they don't exist.
		return "common/null.wav";
	}

	const char *pName = getname();
	return pName ? PSkipSoundChars( pName ) : NULL;	
}

bool CSfxTable::IsPrecachedSound()
{
	const char *pName = getname();

	if ( sv.IsActive() )
	{
		// Server uses zero to mark invalid sounds
		return sv.LookupSoundIndex( pName ) != 0 ? true : false;
	}

	// Client uses -1
	// WE SHOULD FIX THIS!!!
	return ( cl.LookupSoundIndex( pName ) != -1 ) ? true : false;
}

float		g_DuckScale = 1.0f;

// Structure used for fading in and out client sound volume.
typedef struct
{
	float		initial_percent;

	// How far to adjust client's volume down by.
	float		percent;  

	// GetHostTime() when we started adjusting volume
	float		starttime;   

	// # of seconds to get to faded out state
	float		fadeouttime; 
    // # of seconds to hold
	float		holdtime;  
	// # of seconds to restore
	float		fadeintime;
} soundfade_t;

static soundfade_t soundfade;  // Client sound fading singleton object

// 0)headphones 2)stereo speakers 4)quad 5)5point1 
// autodetected from windows settings
ConVar snd_surround( "snd_surround_speakers", "-1", FCVAR_INTERNAL_USE );
ConVar snd_legacy_surround( "snd_legacy_surround", "0", FCVAR_ARCHIVE );
ConVar snd_noextraupdate( "snd_noextraupdate", "0" );
ConVar snd_show( "snd_show", "0", FCVAR_CHEAT, "Show sounds info" );
ConVar snd_visualize ("snd_visualize", "0", FCVAR_CHEAT, "Show sounds location in world" );
ConVar snd_pitchquality( "snd_pitchquality", "1", FCVAR_ARCHIVE );		// 1) use high quality pitch shifters

// master volume
static ConVar volume( "volume", "1.0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX, "Sound volume", true, 0.0f, true, 1.0f );
// user configurable music volume
ConVar snd_musicvolume( "snd_musicvolume", "1.0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_XBOX, "Music volume", true, 0.0f, true, 1.0f );	

ConVar snd_mixahead( "snd_mixahead", "0.1", FCVAR_ARCHIVE );
ConVar snd_mix_async( "snd_mix_async", "0" );
#ifdef _DEBUG
static ConCommand snd_mixvol("snd_mixvol", MXR_DebugSetMixGroupVolume, "Set named Mixgroup to mix volume.");
#endif

// vaudio DLL
IVAudio *vaudio = NULL;
CSysModule *g_pVAudioModule = NULL;

//-----------------------------------------------------------------------------
// Resource loading for sound
//-----------------------------------------------------------------------------
class CResourcePreloadSound : public CResourcePreload
{
public:
	CResourcePreloadSound() : m_SoundNames( 0, 0, true )
	{
	}

	virtual bool CreateResource( const char *pName )
	{
		CSfxTable *pSfx = S_PrecacheSound( pName );
		if ( !pSfx )
		{
			return false;
		}
		
		m_SoundNames.AddString( pSfx->GetFileName() );
		return true;
	}

	virtual void PurgeUnreferencedResources()
	{
		bool bSpew = ( g_pQueuedLoader->GetSpewDetail() & LOADER_DETAIL_PURGES ) != 0;

		for ( int i = s_Sounds.FirstInorder(); i != s_Sounds.InvalidIndex(); i = s_Sounds.NextInorder( i ) )
		{
			// the master sound table grows forever
			// remove sound sources from the master sound table that were not in the preload list
			CSfxTable *pSfx = s_Sounds[i].pSfx;
			if ( pSfx && pSfx->pSource )
			{
				if ( pSfx->m_bIsUISound )
				{
					// never purge ui
					continue;
				}

				UtlSymId_t symbol = m_SoundNames.Find( pSfx->GetFileName() );
				if ( symbol == UTL_INVAL_SYMBOL )
				{
					// sound was not part of preload, purge it
					if ( bSpew )
					{
						Msg( "CResourcePreloadSound: Purging: %s\n", pSfx->GetFileName() );
					}

					pSfx->pSource->CacheUnload();
					delete pSfx->pSource;
					pSfx->pSource = NULL;
				}
			}
		}

		m_SoundNames.RemoveAll();

		if ( !g_pQueuedLoader->IsSameMapLoading() )
		{
			wavedatacache->Flush();
		}
	}

	virtual void PurgeAll()
	{
		bool bSpew = ( g_pQueuedLoader->GetSpewDetail() & LOADER_DETAIL_PURGES ) != 0;

		for ( int i = s_Sounds.FirstInorder(); i != s_Sounds.InvalidIndex(); i = s_Sounds.NextInorder( i ) )
		{
			// the master sound table grows forever
			// remove sound sources from the master sound table that were not in the preload list
			CSfxTable *pSfx = s_Sounds[i].pSfx;
			if ( pSfx && pSfx->pSource )
			{
				if ( pSfx->m_bIsUISound )
				{
					// never purge ui
					if ( bSpew )
					{
						Msg( "CResourcePreloadSound: Skipping: %s\n", pSfx->GetFileName() );
					}
					continue;
				}

				// sound was not part of preload, purge it
				if ( bSpew )
				{
					Msg( "CResourcePreloadSound: Purging: %s\n", pSfx->GetFileName() );
				}
				
				pSfx->pSource->CacheUnload();
				delete pSfx->pSource;
				pSfx->pSource = NULL;
			}
		}

		m_SoundNames.RemoveAll();

		wavedatacache->Flush();
	}
	
private:
	CUtlSymbolTable	m_SoundNames;
};
static CResourcePreloadSound s_ResourcePreloadSound;

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float S_GetMasterVolume( void )
{
	float scale = 1.0f;
	if ( soundfade.percent != 0 )
	{
		scale = clamp( (float)soundfade.percent / 100.0f, 0.0f, 1.0f );
		scale = 1.0f - scale;
	}
	return volume.GetFloat() * scale;
}


void S_SoundInfo_f(void)
{
	if ( !g_AudioDevice->IsActive() )
	{
		Msg( "Sound system not started\n" );
		return;
	}

	Msg( "Sound Device:   %s\n", g_AudioDevice->DeviceName() );
    Msg( "  Channels:     %d\n", g_AudioDevice->DeviceChannels() );
    Msg( "  Samples:      %d\n", g_AudioDevice->DeviceSampleCount() );
    Msg( "  Bits/Sample:  %d\n", g_AudioDevice->DeviceSampleBits() );
    Msg( "  Rate:         %d\n", g_AudioDevice->DeviceDmaSpeed() );
	Msg( "total_channels: %d\n", total_channels);

	if ( IsX360() )
	{
		// dump a glimpse of the mixing state
		CChannelList list;
		g_ActiveChannels.GetActiveChannels( list );

		Msg( "\nActive Channels: (%d)\n", list.Count() );
		for ( int i = 0; i < list.Count(); i++ )
		{
			channel_t *pChannel = list.GetChannel(i);
			Msg( "%s (Mixer: 0x%p)\n", pChannel->sfx->GetFileName(), pChannel->pMixer );
		}
	}
}


/*
================
S_Startup
================
*/

void S_Startup( void )
{
	if ( !snd_initialized )
		return;

	if ( !g_AudioDevice || g_AudioDevice == Audio_GetNullDevice() )
	{
		g_AudioDevice = IAudioDevice::AutoDetectInit( CommandLine()->CheckParm( "-wavonly" ) != 0 );
		if ( !g_AudioDevice )
		{
			Error( "Unable to init audio" );
		}
	}
}

static ConCommand play("play", S_Play, "Play a sound.", FCVAR_SERVER_CAN_EXECUTE );
static ConCommand playflush( "playflush", S_Play, "Play a sound, reloading from disk in case of changes." );
static ConCommand playvol( "playvol", S_PlayVol, "Play a sound at a specified volume." );
static ConCommand speak( "speak", S_Say, "Play a constructed sentence." );
static ConCommand stopsound( "stopsound", S_StopAllSoundsC, 0, FCVAR_CHEAT);		// Marked cheat because it gives an advantage to players minimising ambient noise.
static ConCommand soundlist( "soundlist", S_SoundList, "List all known sounds." );
static ConCommand soundinfo( "soundinfo", S_SoundInfo_f, "Describe the current sound device." );

bool IsValidSampleRate( int rate )
{
	return rate == SOUND_11k || rate == SOUND_22k || rate == SOUND_44k;
}

void VAudioInit()
{
	if ( IsPC() )
	{
		if ( !IsPosix() )
		{
			// vaudio_miles.dll will load this...
			g_pFileSystem->GetLocalCopy( "mss32.dll" );
		}

		g_pVAudioModule = FileSystem_LoadModule( "vaudio_minimp3" );
		if ( g_pVAudioModule )
		{
			CreateInterfaceFn vaudioFactory = Sys_GetFactory( g_pVAudioModule );
			vaudio = (IVAudio *)vaudioFactory( VAUDIO_INTERFACE_VERSION, NULL );
		}
	}
}

/*
================
S_Init
================
*/
void S_Init( void )
{
	if ( sv.IsDedicated() && !CommandLine()->CheckParm( "-forcesound" ) )
		return;

	DevMsg( "Sound Initialization: Start\n" );

	// KDB: init sentence array
	TRACEINIT( VOX_Init(), VOX_Shutdown() );

	VAudioInit();

	if ( CommandLine()->CheckParm( "-nosound" ) )
	{
		g_AudioDevice = Audio_GetNullDevice();
		TRACEINIT( audiosourcecache->Init( host_parms.memsize >> 2 ), audiosourcecache->Shutdown() );
		return;
	}

	snd_initialized = true;

	g_ActiveChannels.Init();
	S_Startup();

	MIX_InitAllPaintbuffers();

	SND_InitScaletable();

	MXR_LoadAllSoundMixers();

	S_StopAllSounds( true );

	TRACEINIT( audiosourcecache->Init( host_parms.memsize >> 2 ), audiosourcecache->Shutdown() );

	AllocDsps( true );

	if ( IsX360() )
	{
		g_pQueuedLoader->InstallLoader( RESOURCEPRELOAD_SOUND, &s_ResourcePreloadSound );
	}

	DevMsg( "Sound Initialization: Finish, Sampling Rate: %i\n", g_AudioDevice->DeviceDmaSpeed() );

#ifdef _X360
	BOOL bPlaybackControl;
	// get initial state of the x360 media player
	if ( XMPTitleHasPlaybackControl( &bPlaybackControl ) == ERROR_SUCCESS )
	{
		S_EnableMusic(bPlaybackControl!=0);
	}

	Assert( g_pVideo != NULL );
	
	if ( g_pVideo != NULL )
	{
		if ( g_pVideo->SoundDeviceCommand( VideoSoundDeviceOperation::HOOK_X_AUDIO, NULL ) != VideoResult::SUCCESS )
		{
			Assert( 0 );
		}
	}
#endif
}


// =======================================================================
// Shutdown sound engine
// =======================================================================
void S_Shutdown(void)
{
#if !defined( _X360 )
	if ( VoiceTweak_IsStillTweaking() )
	{
		VoiceTweak_EndVoiceTweakMode();
	}
#endif

	S_StopAllSounds( true );
	S_ShutdownMixThread();

	TRACESHUTDOWN( audiosourcecache->Shutdown() );

	SNDDMA_Shutdown();

	for ( int i = s_Sounds.FirstInorder(); i != s_Sounds.InvalidIndex(); i = s_Sounds.NextInorder( i ) )
	{
		if ( s_Sounds[i].pSfx )
		{
			delete s_Sounds[i].pSfx->pSource;
			s_Sounds[i].pSfx->pSource = NULL;
		}
	}
	s_Sounds.RemoveAll();
	s_SoundPool.Clear();

	// release DSP resources
	FreeDsps( true );

	MXR_ReleaseMemory();

	// release sentences resources
	TRACESHUTDOWN( VOX_Shutdown() );
	
	if ( IsPC() )
	{
		// shutdown vaudio
		if ( vaudio )
			delete vaudio;
		FileSystem_UnloadModule( g_pVAudioModule );
		g_pVAudioModule = NULL;
		vaudio = NULL;
	}

	MIX_FreeAllPaintbuffers();
	snd_initialized = false;
	g_paintedtime = 0;
	g_soundtime = 0;
	g_ReplaySoundTimeFracAccumulator = 0.0f;
	s_buffers = 0;
	s_oldsampleOutCount = 0;
	s_lastsoundtime = 0.0f;
#if !defined( _X360 )
	Voice_Deinit();
#endif
}

bool S_IsInitted()
{
	return snd_initialized;
}

// =======================================================================
// Load a sound
// =======================================================================

//-----------------------------------------------------------------------------
// Return sfx and set pfInCache to 1 if 
// name is in name cache. Otherwise, alloc
// a new spot in name cache and return 0 
// in pfInCache.
//-----------------------------------------------------------------------------
CSfxTable *S_FindName( const char *szName, int *pfInCache )
{
	int			i;
	CSfxTable	*sfx = NULL;
	char		szBuff[MAX_PATH];
	const char	*pName;

	if ( !szName )
	{
		Error( "S_FindName: NULL\n" );
	}

	pName = szName;
	if ( IsX360() )
	{
		Q_strncpy( szBuff, pName, sizeof( szBuff ) );
		int len = Q_strlen( szBuff )-4;
		if ( len > 0 && !Q_strnicmp( szBuff+len, ".mp3", 4 ) )
		{
			// convert unsupported .mp3 to .wav
			Q_strcpy( szBuff+len, ".wav" );
		}
		pName = szBuff;

		if ( pName[0] == CHAR_STREAM )
		{
			// streaming (or not) is hardcoded to alternate criteria
			// prevent the same sound from creating disparate instances
			pName++;
		}
	}

	// see if already loaded
	FileNameHandle_t fnHandle = g_pFileSystem->FindOrAddFileName( pName );
	i = s_Sounds.Find( fnHandle );
	if ( i != s_Sounds.InvalidIndex() )
	{
		sfx = s_Sounds[i].pSfx;
		Assert( sfx );
		if ( pfInCache )
		{
			// indicate whether or not sound is currently in the cache.
			*pfInCache = ( sfx->pSource && sfx->pSource->IsCached() ) ? 1 : 0;
		}
		return sfx;
	}
	else
	{
		SfxDictEntry entry;
		entry.pSfx = ( CSfxTable * )s_SoundPool.Alloc();

		Assert( entry.pSfx );

		i = s_Sounds.Insert( fnHandle, entry );
		sfx = s_Sounds[i].pSfx;

		sfx->SetNamePoolIndex( i );
		sfx->pSource = NULL;

		if ( pfInCache )
		{
			*pfInCache = 0;
		}
	}
	return sfx;
}

//-----------------------------------------------------------------------------
// S_LoadSound
//
// Check to see if wave data is in the cache. If so, return pointer to data.
// If not, allocate cache space for wave data, load wave file into temporary heap
// space, and dump/convert file data into cache.
//-----------------------------------------------------------------------------
double g_flAccumulatedSoundLoadTime = 0.0f;
CAudioSource *S_LoadSound( CSfxTable *pSfx, channel_t *ch )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	VPROF("S_LoadSound");
	if ( !pSfx->pSource )
	{
		if ( IsX360() )
		{
			if ( SND_IsInGame() && !g_pQueuedLoader->IsMapLoading() )
			{
				// sound should be present (due to reslists), but NOT allowing a load hitch during gameplay 
				// loading a sound during gameplay is a bad experience, causes a very expensive sync i/o to fetch the header
				// and in the case of a memory wave, the actual audio data
				bool bFound = false;
				if ( !pSfx->m_bIsLateLoad )
				{
					if ( pSfx->getname() != PSkipSoundChars( pSfx->getname() ) )
					{
						// the sound might already exist as an undecorated audio source
						FileNameHandle_t fnHandle = g_pFileSystem->FindOrAddFileName( pSfx->GetFileName() );
						int i = s_Sounds.Find( fnHandle );
						if ( i != s_Sounds.InvalidIndex() )
						{
							CSfxTable *pOtherSfx = s_Sounds[i].pSfx;
							Assert( pOtherSfx );
							CAudioSource *pOtherSource = pOtherSfx->pSource;
							if ( pOtherSource && pOtherSource->IsCached() )
							{
								// Can safely let the "load" continue because the headers are expected to be in the preload
								// that are now persisted and the wave data cache will find an existing audio buffer match,
								// so no sync i/o should occur from either.
								bFound = true;
							}
						}
					}

					if ( !bFound )
					{
						// warn once
						DevWarning( "S_LoadSound: Late load '%s', skipping.\n", pSfx->getname() ); 
						pSfx->m_bIsLateLoad = true;
					}
				}

				if ( !bFound )
				{
					return NULL;
				}
			}
			else if ( pSfx->m_bIsLateLoad )
			{
				// outside of gameplay, let the load happen
				pSfx->m_bIsLateLoad = false;
			}
		}

		double st = Plat_FloatTime();

		bool bStream = false;
		bool bUserVox = false;

		// sound chars can explicitly categorize usage
		bStream = TestSoundChar( pSfx->getname(), CHAR_STREAM );
		if ( !bStream )
		{
			bUserVox = TestSoundChar( pSfx->getname(), CHAR_USERVOX );
		}

		// override streaming
		if ( IsX360() )
		{
			const char *s_CriticalSounds[] = 
			{
				"common",
				"items",
				"physics",
				"player",
				"ui",
				"weapons",
			};

			// stream everything but critical sounds
			bStream = true;
			const char *pFileName = pSfx->GetFileName();
			for ( int i = 0; i<ARRAYSIZE( s_CriticalSounds ); i++ )
			{
				size_t len = strlen( s_CriticalSounds[i] );
				if ( !Q_strnicmp( pFileName, s_CriticalSounds[i], len ) && ( pFileName[len] == '\\' || pFileName[len] == '/' ) )
				{
					// never stream these, regardless of sound chars
					bStream = false;
					break;
				}
			}
		}

		if ( bStream )
		{
			// setup as a streaming resource
			pSfx->pSource = Audio_CreateStreamedWave( pSfx );
		}
		else
		{
			if ( bUserVox )
			{
				if ( !IsX360() )
				{
					pSfx->pSource = Voice_SetupAudioSource( ch->soundsource, ch->entchannel );
				}
				else
				{
					// not supporting
					Assert( 0 );
				}
			}
			else
			{
				// load all into memory directly
				pSfx->pSource = Audio_CreateMemoryWave( pSfx );
			}
		}

		double ed = Plat_FloatTime();
		g_flAccumulatedSoundLoadTime += ( ed - st );
	}
	else 
	{
		pSfx->pSource->CheckAudioSourceCache();
	}

	if ( !pSfx->pSource )
	{
		return NULL;
	}

	// first time to load?  Create the mixer
	if ( ch && !ch->pMixer )
	{
		ch->pMixer = pSfx->pSource->CreateMixer( ch->initialStreamPosition );
		if ( !ch->pMixer )
		{
			return NULL;
		}
	}

	return pSfx->pSource;
}

//-----------------------------------------------------------------------------
//	S_PrecacheSound
//
//	Reserve space for the name of the sound in a global array.
//	Load the data for the non-streaming sound. Streaming sounds
//	defer loading of data until just before playback.
//-----------------------------------------------------------------------------
CSfxTable *S_PrecacheSound( const char *name )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	if ( !g_AudioDevice )
		return NULL;

	if ( !g_AudioDevice->IsActive() )
		return NULL;

	CSfxTable *sfx = S_FindName( name, NULL );
	if ( sfx )
	{
		// cache sound
		S_LoadSound( sfx, NULL );
	}
	else
	{
		Assert( !"S_PrecacheSound:  Failed to create sfx" );
	}

	return sfx;
}


void S_InternalReloadSound( CSfxTable *sfx )
{
	if ( !sfx || !sfx->pSource )
		return;

	sfx->pSource->CacheUnload();

	delete sfx->pSource;
	sfx->pSource = NULL;

	char pExt[10];
	Q_ExtractFileExtension( sfx->getname(), pExt, sizeof(pExt) );
	int nSource = !Q_stricmp( pExt, "mp3" ) ? CAudioSource::AUDIO_SOURCE_MP3 : CAudioSource::AUDIO_SOURCE_WAV;
//	audiosourcecache->RebuildCacheEntry( nSource, sfx->IsPrecachedSound(), sfx );
	audiosourcecache->GetInfo( nSource, sfx->IsPrecachedSound(), sfx ); // Do a size/date check and rebuild the cache entry if necessary.
}


//-----------------------------------------------------------------------------
//	Refresh a sound in the cache
//-----------------------------------------------------------------------------
void S_ReloadSound( const char *name )
{
	if ( IsX360() )
	{
		// not supporting
		Assert( 0 );
		return;
	}

	if ( !g_AudioDevice )
		return;

	if ( !g_AudioDevice->IsActive() )
		return;

	CSfxTable *sfx = S_FindName( name, NULL );
#ifdef _DEBUG
	if ( sfx )
	{
		Assert( Q_stricmp( sfx->getname(), name ) == 0 );
	}
#endif
	
	S_InternalReloadSound( sfx );
}


// See comments on CL_HandlePureServerWhitelist for details of what we're doing here.
void S_ReloadFilesInList( IFileList *pFilesToReload )
{
	if ( !IsPC() )
		return;

	S_StopAllSounds( true );
	wavedatacache->Flush();
	audiosourcecache->ForceRecheckDiskInfo();	// Force all cached audio data to recheck size/date info next time it's accessed.
	

	CUtlVector< CSfxTable * > processed;

	int iLast = s_Sounds.LastInorder();
	for ( int i = s_Sounds.FirstInorder(); i != iLast; i = s_Sounds.NextInorder( i ) )
	{
		FileNameHandle_t fnHandle = s_Sounds.Key( i );
		char filename[MAX_PATH * 3];
		if ( !g_pFileSystem->String( fnHandle, filename, sizeof( filename ) ) )
		{
			Assert( !"S_HandlePureServerWhitelist - can't get a filename." );
			continue;
		}
	
		// If the file isn't cached in yet, then the filesystem hasn't touched its file, so don't bother.
		CSfxTable *sfx = s_Sounds[i].pSfx;
		if ( sfx && ( processed.Find( sfx ) == processed.InvalidIndex() ) )
		{
			char fullFilename[MAX_PATH*2];
			if ( IsSoundChar( filename[0] ) )
				Q_snprintf( fullFilename, sizeof( fullFilename ), "sound/%s", &filename[1] );
			else
				Q_snprintf( fullFilename, sizeof( fullFilename ), "sound/%s", filename );

			
			if ( !pFilesToReload->IsFileInList( fullFilename ) )
				continue;

			processed.AddToTail( sfx );

			S_InternalReloadSound( sfx );
		}
	}
}

//-----------------------------------------------------------------------------
// Unfortunate confusing terminology.
// Here prefetching means hinting to the audio source (which may be a stream)
// to get its async data in flight.
//-----------------------------------------------------------------------------
void S_PrefetchSound( char const *name, bool bPlayOnce )
{
	CSfxTable	*sfx;

	if ( !g_AudioDevice )
		return;

	if ( !g_AudioDevice->IsActive() )
		return;

	sfx = S_FindName( name, NULL );
	if ( sfx )
	{
		// cache sound
		S_LoadSound( sfx, NULL );
	}

	if ( !sfx || !sfx->pSource )
	{
		return;
	}

	// hint the sound to start loading
	sfx->pSource->Prefetch();

	if ( bPlayOnce )
	{
		sfx->pSource->SetPlayOnce( true );
	}
}

void S_MarkUISound( CSfxTable *pSfx )
{
	pSfx->m_bIsUISound = true;
}

unsigned int RemainingSamples( channel_t *pChannel )
{
	if ( !pChannel || !pChannel->sfx || !pChannel->sfx->pSource )
		return 0;

	unsigned int timeleft = pChannel->sfx->pSource->SampleCount();

	if ( pChannel->sfx->pSource->IsLooped() )
	{
		return pChannel->sfx->pSource->SampleRate();
	}

	if ( pChannel->pMixer )
	{
		timeleft -= pChannel->pMixer->GetSamplePosition();
	}

	return timeleft;
}

// chooses the voice stealing algorithm
ConVar voice_steal("voice_steal", "2");

/*
=================
SND_StealDynamicChannel
Select a channel from the dynamic channel allocation area.  For the given entity, 
override any other sound playing on the same channel (see code comments below for
exceptions).
=================
*/
channel_t *SND_StealDynamicChannel(SoundSource soundsource, int entchannel, const Vector &origin, CSfxTable *sfx, float flDelay, bool bDoNotOverwriteExisting)
{
	int canSteal[MAX_DYNAMIC_CHANNELS];
	int canStealCount = 0;

	int sameSoundCount = 0;
	unsigned int sameSoundRemaining = 0xFFFFFFFF;
	int sameSoundIndex = -1;
	int sameVol = 0xFFFF;
	int availableChannel = -1;
	bool bDelaySame = false;

	int nExactMatch[MAX_DYNAMIC_CHANNELS];
	int nExactCount = 0;
	// first pass to replace sounds on same ent/channel, and search for free or stealable channels otherwise
	for ( int ch_idx = 0; ch_idx < MAX_DYNAMIC_CHANNELS ; ch_idx++)
	{
		channel_t *ch = &channels[ch_idx];
		
		if ( ch->activeIndex )
		{
			// channel CHAN_AUTO never overrides sounds on same channel
			if ( entchannel != CHAN_AUTO )
			{
				int checkChannel = entchannel;
				if ( checkChannel == -1 )
				{
					if ( ch->entchannel != CHAN_STREAM && ch->entchannel != CHAN_VOICE && ch->entchannel != CHAN_VOICE2 )
					{
						checkChannel = ch->entchannel;
					}
				}
				if ( ch->soundsource == soundsource && (soundsource != -1) && ch->entchannel == checkChannel )
				{
					// we found an exact match for this entity and this channel, but the sound we want to play is considered
					// low priority so instead of stomping this entry pretend we couldn't find a free slot to play and let
					// the existing sound keep going
					if ( bDoNotOverwriteExisting )
						return NULL;

					if ( ch->flags.delayed_start )
					{
						nExactMatch[nExactCount] = ch_idx;
						nExactCount++;
						continue;
					}
					return ch;	// always override sound from same entity
				}
			}

			// Never steal the channel of a streaming sound that is currently playing or
			// voice over IP data that is playing or any sound on CHAN_VOICE( acting )
			if ( ch->entchannel == CHAN_STREAM || ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_VOICE2 )
				continue;

			// don't let monster sounds override player sounds
			if ( g_pSoundServices->IsPlayer( ch->soundsource ) && !g_pSoundServices->IsPlayer(soundsource) )
				continue;

			if ( ch->sfx == sfx )
			{
				bDelaySame = ch->flags.delayed_start ? true : bDelaySame;
				sameSoundCount++;
				int maxVolume = ChannelGetMaxVol( ch );
				unsigned int remaining = RemainingSamples(ch);
				if ( maxVolume < sameVol || (maxVolume == sameVol && remaining < sameSoundRemaining) )
				{
					sameSoundIndex = ch_idx;
					sameVol = maxVolume;
					sameSoundRemaining = remaining;
				}
			}
			canSteal[canStealCount++] = ch_idx;
		}
		else
		{
			if ( availableChannel < 0 )
			{
				availableChannel = ch_idx;
			}
		}
	}


	// coalesce the timeline for this channel
	if ( nExactCount > 0 )
	{
		uint nFreeSampleTime = g_paintedtime + (flDelay * SOUND_DMA_SPEED);
		channel_t *pReturn = &channels[nExactMatch[0]];
		uint nMinRemaining = RemainingSamples( pReturn );
		if ( pReturn->nFreeChannelAtSampleTime == 0 || pReturn->nFreeChannelAtSampleTime > nFreeSampleTime )
		{
			pReturn->nFreeChannelAtSampleTime = nFreeSampleTime;
		}
		for ( int i = 1; i < nExactCount; i++ )
		{
			channel_t *pChannel = &channels[nExactMatch[i]];
			if ( pChannel->nFreeChannelAtSampleTime == 0 || pChannel->nFreeChannelAtSampleTime > nFreeSampleTime )
			{
				pChannel->nFreeChannelAtSampleTime = nFreeSampleTime;
			}
			uint nRemain = RemainingSamples( pChannel );
			if ( nRemain < nMinRemaining )
			{
				pReturn = pChannel;
				nMinRemaining = nRemain;
			}
		}
		// if there's only one, mark it to be freed but don't reuse it.
		// otherwise mark all others to be freed and use the closest one to being done
		if ( nExactCount > 1 )
		{
			return pReturn;
		}
	}

	// Limit the number of times a given sfx/wave can play simultaneously
	if ( voice_steal.GetInt() > 1 && sameSoundIndex >= 0 )
	{
		// if sounds of this type are normally delayed, then add an extra slot for stealing
		// NOTE: In HL2 these are usually NPC gunshot sounds - and stealing too soon will cut
		// them off early.  This is a safe heuristic to avoid that problem.  There's probably a better
		// long-term solution involving only counting channels that are actually going to play (delay included)
		// at the same time as this one.
		int maxSameSounds = bDelaySame ? 5 : 4;
		float distSqr = 0.0f;
		if ( sfx->pSource )
		{
			distSqr = origin.DistToSqr(listener_origin);
			if ( sfx->pSource->IsLooped() )
			{
				maxSameSounds = 3;
			}
		}

		// don't play more than N copies of the same sound, steal the quietest & closest one otherwise
		if ( sameSoundCount >= maxSameSounds )
		{
			channel_t *ch = &channels[sameSoundIndex];
			// you're already playing a closer version of this sound, don't steal
			if ( distSqr > 0.0f && ch->origin.DistToSqr(listener_origin) < distSqr && entchannel != CHAN_WEAPON )
				return NULL;

			//Msg("Sound playing %d copies, stole %s (%d)\n", sameSoundCount, ch->sfx->getname(), sameVol );
			return ch;
		}
	}

	// if there's a free channel, just take that one - don't steal
	if ( availableChannel >= 0 )
		return &channels[availableChannel];

	// Still haven't found a suitable channel, so choose the one with the least amount of time left to play
	float life_left = FLT_MAX;
	int first_to_die = -1;
	bool bAllowVoiceSteal = voice_steal.GetBool();

	for ( int i = 0; i < canStealCount; i++ )
	{
		int ch_idx = canSteal[i];
		channel_t *ch = &channels[ch_idx];
		float timeleft = 0;
		if ( bAllowVoiceSteal )
		{
			int maxVolume = ChannelGetMaxVol( ch );
			if ( maxVolume < 5 )
			{
				//Msg("Sound quiet, stole %s for %s\n", ch->sfx->getname(), sfx->getname() );
				return ch;
			}

			if ( ch->sfx && ch->sfx->pSource )
			{
				unsigned int sampleCount = RemainingSamples( ch );
				timeleft = (float)sampleCount / (float)ch->sfx->pSource->SampleRate();
			}
		}
		else
		{
			// UNDONE: Kill this when voice_steal 0,1,2 has been tested
			// UNDONE: This is the old buggy code that we're trying to replace
			if ( ch->sfx )
			{
				// basically steals the first one you come to
				timeleft = 1;	//ch->end - paintedtime
			}
		}

		if ( timeleft < life_left )
		{
			life_left = timeleft;
			first_to_die = ch_idx;
		}
	}
	if ( first_to_die >= 0 )
	{
		//Msg("Stole %s, timeleft %d\n", channels[first_to_die].sfx->getname(), life_left );
		return &channels[first_to_die];
	}

	return NULL;
}

channel_t *SND_PickDynamicChannel(SoundSource soundsource, int entchannel, const Vector &origin, CSfxTable *sfx, float flDelay, bool bDoNotOverwriteExisting)
{
	channel_t *pChannel = SND_StealDynamicChannel( soundsource, entchannel, origin, sfx, flDelay, bDoNotOverwriteExisting );
	if ( !pChannel )
		return NULL;

	if (pChannel->sfx)
	{
		// Don't restart looping sounds for the same entity
		CAudioSource *pSource = pChannel->sfx->pSource;
		if ( pSource )
		{
			if ( pSource->IsLooped() )
			{
				if ( pChannel->soundsource == soundsource && pChannel->entchannel == entchannel && pChannel->sfx == sfx )
				{
					// same looping sound, same ent, same channel, don't restart the sound
					return NULL;
				}
			}
		}
		// be sure and release previous channel
		// if sentence.
		//	("Stealing channel from %s\n", channels[first_to_die].sfx->getname() );
		S_FreeChannel(pChannel);
	}

	return pChannel;
}

  			

/*
=====================
SND_PickStaticChannel
=====================
Pick an empty channel from the static sound area, or allocate a new
channel.  Only fails if we're at max_channels (128!!!) or if 
we're trying to allocate a channel for a stream sound that is 
already playing.

*/
channel_t *SND_PickStaticChannel(int soundsource, CSfxTable *pSfx)
{
	int i;
	channel_t *ch = NULL;

	// Check for replacement sound, or find the best one to replace
 	for (i = MAX_DYNAMIC_CHANNELS; i<total_channels; i++)
		if (channels[i].sfx == NULL)
			break;

	if (i < total_channels) 
	{
		// reuse an empty static sound channel
		ch = &channels[i];
	}
	else
	{
		// no empty slots, alloc a new static sound channel
		if (total_channels == MAX_CHANNELS)
		{
			DevMsg ("total_channels == MAX_CHANNELS\n");
			return NULL;
		}

		// get a channel for the static sound
		ch = &channels[total_channels];
		total_channels++;
	}

	return ch;
}


void S_SpatializeChannel( int pVolume[CCHANVOLUMES/2], int master_vol, const Vector *psourceDir, float gain, float mono )
{
	float lscale, rscale, scale;
	vec_t dotRight;
	Vector sourceDir = *psourceDir;

	dotRight = DotProduct(listener_right, sourceDir);

	// clear volumes
	for (int i = 0; i < CCHANVOLUMES/2; i++)
		pVolume[i] = 0;

	if (mono > 0.0)
	{
		// sound has radius, within which spatialization becomes mono:

		// mono is 0.0 -> 1.0, from radius 100% to radius 50%

		// at radius * 0.5, dotRight is 0 (ie: sound centered left/right)
		// at radius * 1.0, dotRight == dotRight

		dotRight   *= (1.0 - mono);
	}

	rscale = 1.0 + dotRight;
	lscale = 1.0 - dotRight;

 // add in distance effect
	scale = gain * rscale / 2;
	pVolume[IFRONT_RIGHT] = (int) (master_vol * scale);

	scale = gain * lscale / 2;
	pVolume[IFRONT_LEFT] = (int) (master_vol * scale);

	pVolume[IFRONT_RIGHT] = clamp( pVolume[IFRONT_RIGHT], 0, 255 );
	pVolume[IFRONT_LEFT] = clamp( pVolume[IFRONT_LEFT], 0, 255 );

}

bool S_IsMusic( channel_t *pChannel )
{
	if ( !pChannel->flags.bdry )
		return false;

	CSfxTable *sfx = pChannel->sfx;
	if ( !sfx )
		return false;

	CAudioSource *source = sfx->pSource;
	if ( !source )
		return false;

	// Don't save restore looping sounds as you can end up with an entity restarting them again and have 
	//  them accumulate, etc.
	if ( source->IsLooped() )
		return false;

	CAudioMixer *pMixer = pChannel->pMixer;
	if ( !pMixer )
		return false;

	for ( int i = 0; i < 8; i++ )
	{
		if ( pChannel->mixgroups[i] != -1 )
		{
			char *pGroupName = MXR_GetGroupnameFromId( pChannel->mixgroups[i] );
			if ( !Q_strcmp( pGroupName, "Music" ) )
			{
				return true;
			}
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: For save/restore of currently playing music
// Input  : list - 
//-----------------------------------------------------------------------------
void S_GetCurrentlyPlayingMusic( CUtlVector< musicsave_t >& musiclist )
{
	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	for ( int i = 0; i < list.Count(); i++ )
	{
		channel_t *pChannel = &channels[list.GetChannelIndex(i)];
		if ( !S_IsMusic( pChannel ) )
			continue;

		musicsave_t song;
		Q_strncpy( song.songname, pChannel->sfx->getname(), sizeof( song.songname ) );
		song.sampleposition = pChannel->pMixer->GetPositionForSave();
		song.master_volume = pChannel->master_vol;

		musiclist.AddToTail( song );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *song - 
//-----------------------------------------------------------------------------
void S_RestartSong( const musicsave_t *song )
{
	Assert( song );

	// Start the song
	CSfxTable *pSound = S_PrecacheSound( song->songname );
	if ( pSound )
	{
		StartSoundParams_t params;
		params.staticsound = true;
		params.soundsource = SOUND_FROM_WORLD;
		params.entchannel = CHAN_STATIC;
		params.pSfx = pSound;
		params.origin = vec3_origin;
		params.fvol = ( (float)song->master_volume / 255.0f );
		params.soundlevel = SNDLVL_NONE;
		params.flags = SND_NOFLAGS;
		params.pitch = PITCH_NORM;
		params.initialStreamPosition = song->sampleposition;

		S_StartSound( params );

		if ( IsPC() )
		{
			// Now find the channel this went on and skip ahead in the mixer
			for (int i = 0; i < total_channels; i++)
			{
				channel_t *ch = &channels[i]; 

				if ( !ch->pMixer ||
					 !ch->pMixer->GetSource() )
				{
					continue;
				}

				if ( ch->pMixer->GetSource() != pSound->pSource )
				{
					continue;
				}

				ch->pMixer->SetPositionFromSaved( song->sampleposition );
				break;
			}
		}
	}
}

soundlevel_t SND_GetSndlvl ( channel_t *pchannel );

// calculate ammount of sound to be mixed to dsp, based on distance from listener


ConVar dsp_dist_min("dsp_dist_min", "0.0", FCVAR_DEMO|FCVAR_CHEAT);		// range at which sounds are mixed at dsp_mix_min
ConVar dsp_dist_max("dsp_dist_max", "1440.0", FCVAR_DEMO|FCVAR_CHEAT);	// range at which sounds are mixed at dsp_mix_max	

ConVar dsp_mix_min("dsp_mix_min", "0.2", FCVAR_DEMO );		// dsp mix at dsp_dist_min distance "near"
ConVar dsp_mix_max("dsp_mix_max", "0.8", FCVAR_DEMO );		// dsp mix at dsp_dist_max distance "far"
ConVar dsp_db_min("dsp_db_min", "80", FCVAR_DEMO );			// sounds with sndlvl below this get dsp_db_mixdrop % less dsp mix
ConVar dsp_db_mixdrop("dsp_db_mixdrop", "0.5", FCVAR_DEMO );	// sounds with sndlvl below dsp_db_min get dsp_db_mixdrop % less mix

float	DSP_ROOM_MIX	= 1.0;	// mix volume of dsp_room sounds when added back to 'dry' sounds
float	DSP_NOROOM_MIX	= 1.0;	// mix volume of facing + facing away sounds. added to dsp_room_mix sounds

extern ConVar dsp_off;

// returns 0-1.0 dsp mix value.  If sound source is at a range >= DSP_DIST_MAX, return a mix value of
// DSP_MIX_MAX.  This mix value is used later to determine wet/dry mix ratio of sounds.

// This ramp changes with db level of sound source,  and is set in the dsp room presets by room size
// empirical data: 0.78 is nominal mix for sound 100% at far end of room, 0.24 is mix for sound 25% into room

float SND_GetDspMix( channel_t *pchannel, int idist)
{
	float mix;
	float dist = (float)idist;
	float dist_min = dsp_dist_min.GetFloat();
	float dist_max = dsp_dist_max.GetFloat();
	float mix_min;
	float mix_max;

	// only set dsp mix_min & mix_max when sound is first started

	if ( pchannel->dsp_mix_min < 0 && pchannel->dsp_mix_max < 0 )
	{
		mix_min  = dsp_mix_min.GetFloat();		// set via dsp_room preset
		mix_max  = dsp_mix_max.GetFloat();		// set via dsp_room preset

		// set mix_min & mix_max based on db level of sound:
		// sounds below dsp_db_min decrease dsp_mix_min & dsp_mix_max by N%
		// ie: quiet sounds get less dsp mix than loud sounds

		soundlevel_t sndlvl = SND_GetSndlvl( pchannel ); 
		soundlevel_t sndlvl_min = (soundlevel_t)(dsp_db_min.GetInt());
		
		if (sndlvl <= sndlvl_min)
		{
			mix_min *= dsp_db_mixdrop.GetFloat();
			mix_max *= dsp_db_mixdrop.GetFloat();
		}

		pchannel->dsp_mix_min = mix_min;
		pchannel->dsp_mix_max = mix_max;
	}
	else
	{
		mix_min = pchannel->dsp_mix_min;
		mix_max = pchannel->dsp_mix_max;
	}

	// dspmix is 0 (100% mix to facing buffer) if dsp_off

	if ( dsp_off.GetInt() )
		return 0.0;

	// doppler wavs are mixed dry

	if ( pchannel->wavtype == CHAR_DOPPLER )
		return 0.0;

	// linear ramp - get dry mix %
	
	// dist: 0->(max - min)

	dist = clamp( dist, dist_min, dist_max ) - dist_min;

	// dist: 0->1.0

	dist = dist / (dist_max - dist_min);

	// mix: min->max

	mix = ((mix_max - mix_min) * dist) + mix_min;
				
	return mix;
}

// calculate crossfade between wav left (close sound) and wav right (far sound) based on
// distance fron listener

#define DVAR_DIST_MIN	(20.0  * 12.0)		// play full 'near' sound at 20' or less
#define DVAR_DIST_MAX	(110.0 * 12.0)		// play full 'far' sound at 110' or more
#define DVAR_MIX_MIN	0.0
#define DVAR_MIX_MAX	1.0

// calculate mixing parameter for CHAR_DISTVAR wavs
// returns 0 - 1.0, 1.0 is 100% far sound (wav right)

float SND_GetDistanceMix( channel_t *pchannel, int idist)
{
	float mix;
	float dist = (float)idist;
	
	// doppler wavs are 100% near - their spatialization is calculated later.

	if ( pchannel->wavtype == CHAR_DOPPLER )
		return 0.0;

	// linear ramp - get dry mix %
	
	// dist 0->(max - min)

	dist = clamp( dist, (float) DVAR_DIST_MIN, (float) DVAR_DIST_MAX ) - (float) DVAR_DIST_MIN;

	// dist 0->1.0

	dist = dist / (DVAR_DIST_MAX - DVAR_DIST_MIN);

	// mix min->max

	mix = ((DVAR_MIX_MAX - DVAR_MIX_MIN) * dist) + DVAR_MIX_MIN;
	
	return mix;
}

// given facing direction of source, and channel, 
// return -1.0 - 1.0, where -1.0 is source facing away from listener
// and 1.0 is source facing listener


float SND_GetFacingDirection( channel_t *pChannel, const QAngle &source_angles )
{
	Vector SF;				// sound source forward direction unit vector
	Vector SL;				// sound -> listener unit vector
	float dotSFSL;

	// no facing direction unless wavtyp CHAR_DIRECTIONAL

	if ( pChannel->wavtype != CHAR_DIRECTIONAL )
		return 1.0;
	
	VectorSubtract(listener_origin, pChannel->origin, SL);
	VectorNormalize(SL);

	// compute forward vector for sound entity

	AngleVectors( source_angles, &SF, NULL, NULL );

	// dot source forward unit vector with source to listener unit vector to get -1.0 - 1.0 facing.
	// ie: projection of SF onto SL

	dotSFSL = DotProduct( SF, SL );
		
	return dotSFSL;
}

// calculate point of closest approach - caller must ensure that the 
// forward facing vector of the entity playing this sound points in exactly the direction of 
// travel of the sound. ie: for bullets or tracers, forward vector must point in traceline direction.
// return true if sound is to be played, false if sound cannot be heard (shot away from player)

bool SND_GetClosestPoint( channel_t *pChannel, QAngle &source_angles, Vector &vnearpoint )
{
	// S - sound source origin
	// L - listener origin
	
	Vector SF;				// sound source forward direction unit vector
	Vector SL;				// sound -> listener vector
	Vector SD;				// sound->closest point vector
	vec_t dSLSF;			// magnitude of project of SL onto SF

	// P = SF (SF . SL) + S

	// only perform this calculation for doppler wavs

	if ( pChannel->wavtype != CHAR_DOPPLER )
		return false;

	// get vector 'SL' from sound source to listener

	VectorSubtract(listener_origin, pChannel->origin, SL);

	// compute sound->forward vector 'SF' for sound entity

	AngleVectors( source_angles, &SF );
	VectorNormalize( SF );
	
	dSLSF = DotProduct( SL, SF );


	if ( dSLSF <= 0 && !toolframework->IsToolRecording() )
	{
		// source is pointing away from listener, don't play anything
		// unless we're recording in the tool, since we may play back from in front of the source
		return false;
	}
		
	// project dSLSF along forward unit vector from sound source
	
	VectorMultiply( SF, dSLSF, SD );

	// output vector - add SD to sound source origin

	VectorAdd( SD, pChannel->origin, vnearpoint );

	return true;
}


// given point of nearest approach and sound source facing angles, 
// return vector pointing into quadrant in which to play 
// doppler left wav (incomming) and doppler right wav (outgoing).

// doppler left is point in space to play left doppler wav
// doppler right is point in space to play right doppler wav

// Also modifies channel pitch based on distance to nearest approach point

#define DOPPLER_DIST_LEFT_TO_RIGHT	(4*12)		// separate left/right sounds by 4'

#define DOPPLER_DIST_MAX			(20*12)		// max distance - causes min pitch
#define DOPPLER_DIST_MIN			(1*12)		// min distance - causes max pitch
#define DOPPLER_PITCH_MAX			1.5			// max pitch change due to distance
#define DOPPLER_PITCH_MIN			0.25		// min pitch change due to distance

#define DOPPLER_RANGE_MAX			(10*12)		// don't play doppler wav unless within this range
												// UNDONE: should be set by caller!

void SND_GetDopplerPoints( channel_t *pChannel, QAngle &source_angles, Vector &vnearpoint, Vector &source_doppler_left, Vector &source_doppler_right)
{
	Vector SF;			// direction sound source is facing (forward)
	Vector LN;			// vector from listener to closest approach point
	Vector DL;
	Vector DR;

	// nearpoint is closest point of approach, when playing CHAR_DOPPLER sounds

	// SF is normalized vector in direction sound source is facing

	AngleVectors( source_angles, &SF );
	VectorNormalize( SF );

	// source_doppler_left - location in space to play doppler left wav (incomming)
	// source_doppler_right	- location in space to play doppler right wav (outgoing)
	
	VectorMultiply( SF, -1*DOPPLER_DIST_LEFT_TO_RIGHT, DL );
	VectorMultiply( SF, DOPPLER_DIST_LEFT_TO_RIGHT, DR );

	VectorAdd( vnearpoint, DL, source_doppler_left );
	VectorAdd( vnearpoint, DR, source_doppler_right );
	
	// set pitch of channel based on nearest distance to listener

	// LN is vector from listener to closest approach point

	VectorSubtract(vnearpoint, listener_origin, LN);

	float pitch;
	float dist = VectorLength( LN );
	
	// dist varies 0->1

	dist = clamp(dist, (float)DOPPLER_DIST_MIN, (float)DOPPLER_DIST_MAX);
	dist = (dist - DOPPLER_DIST_MIN) / (DOPPLER_DIST_MAX - DOPPLER_DIST_MIN);

	// pitch varies from max to min

	pitch = DOPPLER_PITCH_MAX - dist * (DOPPLER_PITCH_MAX - DOPPLER_PITCH_MIN);
	
	pChannel->basePitch = (int)(pitch * 100.0);
}

// console variables used to construct gain curve - don't change these!

extern ConVar snd_foliage_db_loss; 
extern ConVar snd_gain;
extern ConVar snd_refdb;
extern ConVar snd_refdist;
extern ConVar snd_gain_max;
extern ConVar snd_gain_min;

ConVar snd_showstart( "snd_showstart", "0", FCVAR_CHEAT );	// showstart always skips info on player footsteps!
												// 1 - show sound name, channel, volume, time 
												// 2 - show dspmix, distmix, dspface, l/r/f/r vols
												// 3 - show sound origin coords
												// 4 - show gain of dsp_room
												// 5 - show dB loss due to obscured sound
												// 6 - reserved
												// 7 - show 2 and total gain & dist in ft. to sound source

#define SND_DB_MAX				140.0	// max db of any sound source
#define SND_DB_MED				90.0	// db at which compression curve changes
#define SND_DB_MIN				60.0	// min db of any sound source

#define SND_GAIN_PLAYER_WEAPON_DB 2.0	// increase player weapon gain by N dB

// dB = 20 log (amplitude/32768)		0 to -90.3dB
// amplitude = 32768 * 10 ^ (dB/20)		0 to +/- 32768
// gain = amplitude/32768				0 to 1.0

float Gain_To_dB ( float gain )
{
	float dB = 20 * log ( gain );
	return dB;
}

float dB_To_Gain ( float dB )
{
	float gain = powf (10, dB / 20.0);
	return gain;
}

float Gain_To_Amplitude ( float gain )
{
	return gain * 32768;
}

float Amplitude_To_Gain ( float amplitude )
{
	return amplitude / 32768;
}

soundlevel_t SND_GetSndlvl ( channel_t *pchannel )
{
	return DIST_MULT_TO_SNDLVL( pchannel->dist_mult );
}


// The complete gain calculation, with SNDLVL given in dB is:
//
// GAIN = 1/dist * snd_refdist * 10 ^ ( ( SNDLVL - snd_refdb - (dist * snd_foliage_db_loss / 1200)) / 20 )
// 
//		for gain > SND_GAIN_THRESH, start curve smoothing with
//
// GAIN = 1 - 1 / (Y * GAIN ^ SND_GAIN_POWER)
// 
//		 where Y = -1 / ( (SND_GAIN_THRESH ^ SND_GAIN_POWER) * (SND_GAIN_THRESH - 1) )
//

float SND_GetGainFromMult( float gain, float dist_mult, vec_t dist );

// gain curve construction

float SND_GetGain( channel_t *ch, bool fplayersound, bool fmusicsound, bool flooping, vec_t dist, bool bAttenuated )
{
	VPROF_("SND_GetGain",2,VPROF_BUDGETGROUP_OTHER_SOUND,false,BUDGETFLAG_OTHER);
	if ( ch->flags.m_bCompatibilityAttenuation )
	{
		// Convert to the original attenuation value.
		soundlevel_t soundlevel = DIST_MULT_TO_SNDLVL( ch->dist_mult );
		float flAttenuation = SNDLVL_TO_ATTN( soundlevel );

		// Now get the goldsrc dist_mult and use the same calculation it uses in SND_Spatialize.
		// Straight outta Goldsrc!!!
		vec_t nominal_clip_dist = 1000.0;
		float flGoldsrcDistMult = flAttenuation / nominal_clip_dist;
		dist *= flGoldsrcDistMult;
		float flReturnValue = 1.0f - dist;
		flReturnValue = clamp( flReturnValue, 0.f, 1.f );
		return flReturnValue;
	}
	else
	{
		float gain = snd_gain.GetFloat();

		if ( fmusicsound )
		{
			gain = gain * snd_musicvolume.GetFloat();
			gain = gain * g_DashboardMusicMixValue;
		}

		if ( ch->dist_mult )
		{
			gain = SND_GetGainFromMult( gain, ch->dist_mult, dist );
		}

		if ( fplayersound )
		{
			
			// player weapon sounds get extra gain - this compensates
			// for npc distance effect weapons which mix louder as L+R into L,R
			// Hack.

			if ( ch->entchannel == CHAN_WEAPON )
				gain = gain * dB_To_Gain( SND_GAIN_PLAYER_WEAPON_DB );
		}

		// modify gain if sound source not visible to player

		gain = gain * SND_GetGainObscured( ch, fplayersound, flooping, bAttenuated );

		if (snd_showstart.GetInt() == 6)
		{
			DevMsg( "(gain %1.3f : dist ft %1.1f) ", gain, (float)dist/12.0 );
			snd_showstart.SetValue(5);	// display once
		}

		return gain; 
	}
}

// always ramp channel gain changes over time
// returns ramped gain, given new target gain

#define SND_GAIN_FADE_TIME	0.25		// xfade seconds between obscuring gain changes

float SND_FadeToNewGain( channel_t *ch, float gain_new )
{

	if ( gain_new == -1.0 )
	{
		// if -1 passed in, just keep fading to existing target

		gain_new = ch->ob_gain_target;
	}

	// if first time updating, store new gain into gain & target, return
	// if gain_new is close to existing gain, store new gain into gain & target, return

	if ( ch->flags.bfirstpass || (fabs (gain_new - ch->ob_gain) < 0.01))
	{
		ch->ob_gain			= gain_new;
		ch->ob_gain_target	= gain_new;
		ch->ob_gain_inc		= 0.0;
		return gain_new;
	}

	// set up new increment to new target
	
	float frametime = g_pSoundServices->GetHostFrametime();
	float speed;
	speed = ( frametime / SND_GAIN_FADE_TIME ) * (gain_new - ch->ob_gain);

	ch->ob_gain_inc = fabs(speed);

	// ch->ob_gain_inc = fabs(gain_new - ch->ob_gain) / 10.0;
	
	ch->ob_gain_target = gain_new;

	// if not hit target, keep approaching
	
	if ( fabs( ch->ob_gain - ch->ob_gain_target ) > 0.01 )
	{
		ch->ob_gain = Approach( ch->ob_gain_target, ch->ob_gain, ch->ob_gain_inc );
	}
	else
	{
		// close enough, set gain = target
		ch->ob_gain = ch->ob_gain_target;
	}

	return ch->ob_gain;
}

#define SND_TRACE_UPDATE_MAX  2			// max of N channels may be checked for obscured source per frame

static int g_snd_trace_count = 0;		// total tracelines for gain obscuring made this frame

// All new sounds must traceline once,
// but cap the max number of tracelines performed per frame
// for longer or looping sounds to SND_TRACE_UPDATE_MAX.

bool SND_ChannelOkToTrace( channel_t *ch )
{
	// always trace first time sound is spatialized (doesn't update counter)

	if ( ch->flags.bfirstpass )
	{
		ch->flags.bTraced = true;
		return true;
	}

	// if already traced max channels this frame, return

	if ( g_snd_trace_count >= SND_TRACE_UPDATE_MAX )
		return false;

	// ok to trace if this sound hasn't yet been traced in this round

	if ( ch->flags.bTraced )
		return false;

	// set flag - don't traceline this sound again until all others have
	// been traced

	ch->flags.bTraced = true;

	g_snd_trace_count++;				// total traces this frame

	return true;
}

// determine if we need to reset all flags for traceline limiting - 
// this happens if we hit a frame whein no tracelines occur ie: all currently 
// playing sounds are blocked.

void SND_ChannelTraceReset( void )
{
	if ( g_snd_trace_count )
		return;

	// if no tracelines performed this frame, then reset all 
	// trace flags

	for (int i = 0; i < total_channels; i++)
		channels[i].flags.bTraced = false; 
}

bool SND_IsLongWave( channel_t *pChannel )
{
	CAudioSource *pSource = pChannel->sfx ? pChannel->sfx->pSource : NULL;
	if ( pSource )
	{
		if ( pSource->IsStreaming() )
			return true;

	// UNDONE: Do this on long wave files too?
#if 0
		float length = (float)pSource->SampleCount() / (float)pSource->SampleRate();
		if ( length > 0.75f )
			return true;
#endif
	}

	return false;
}


ConVar snd_obscured_gain_db( "snd_obscured_gain_dB", "-2.70", FCVAR_CHEAT ); // dB loss due to obscured sound source

// drop gain on channel if sound emitter obscured by
// world, unbroken windows, closed doors, large solid entities etc.

float SND_GetGainObscured( channel_t *ch, bool fplayersound, bool flooping, bool bAttenuated )
{
	float gain = 1.0;
	int count = 1;
	float snd_gain_db;					// dB loss due to obscured sound source

	// Unattenuated sounds don't get obscured.
	if ( !bAttenuated )
		return 1.0f;

	if ( fplayersound )
		return gain;

	// During signon just apply regular state machine since world hasn't been
	//  created or settled yet...

	if ( !SND_IsInGame() )
	{
		if ( !toolframework->InToolMode() )
		{
			gain = SND_FadeToNewGain( ch, -1.0 );
		}

		return gain;
	}

	// don't do gain obscuring more than once on short one-shot sounds

	if ( !ch->flags.bfirstpass && !ch->flags.isSentence && !flooping && !SND_IsLongWave(ch) )
	{
		gain = SND_FadeToNewGain( ch, -1.0 );
		return gain;
	}

	snd_gain_db = snd_obscured_gain_db.GetFloat();

	// if long or looping sound, process N channels per frame - set 'processed' flag, clear by
	// cycling through all channels - this maintains a cap on traces per frame

	if ( !SND_ChannelOkToTrace( ch ) )
	{
		// just keep updating fade to existing target gain - no new trace checking

		gain = SND_FadeToNewGain( ch, -1.0 );
		return gain;
	}
	// set up traceline from player eyes to sound emitting entity origin

	Vector endpoint = ch->origin;
	
	trace_t tr;
	CTraceFilterWorldOnly filter;	// UNDONE: also test for static props?
	Ray_t ray;
	ray.Init( MainViewOrigin(), endpoint );
	g_pEngineTraceClient->TraceRay( ray, MASK_BLOCK_AUDIO, &filter, &tr );

	if (tr.DidHit() && tr.fraction < 0.99)
	{
		// can't see center of sound source:
		// build extents based on dB sndlvl of source,
		// test to see how many extents are visible,
		// drop gain by snd_gain_db per extent hidden

		Vector endpoints[4];
		soundlevel_t sndlvl = DIST_MULT_TO_SNDLVL( ch->dist_mult );
		float radius;
		Vector vsrc_forward;
		Vector vsrc_right;
		Vector vsrc_up;
		Vector vecl;
		Vector vecr;
		Vector vecl2;
		Vector vecr2;
		int i;

		// get radius
		
		if ( ch->radius > 0 )
			radius = ch->radius;
		else
			radius = dB_To_Radius( sndlvl);		// approximate radius from soundlevel
		
		// set up extent endpoints - on upward or downward diagonals, facing player

		for (i = 0; i < 4; i++)
			endpoints[i] = endpoint;

		// vsrc_forward is normalized vector from sound source to listener

		VectorSubtract( listener_origin, endpoint, vsrc_forward );
		VectorNormalize( vsrc_forward );
		VectorVectors( vsrc_forward, vsrc_right, vsrc_up );

		VectorAdd( vsrc_up, vsrc_right, vecl );
		
		// if src above listener, force 'up' vector to point down - create diagonals up & down

		if ( endpoint.z > listener_origin.z + (10 * 12) )
			vsrc_up.z = -vsrc_up.z;

		VectorSubtract( vsrc_up, vsrc_right, vecr );
		VectorNormalize( vecl );
		VectorNormalize( vecr );

		// get diagonal vectors from sound source 

		vecl2 = radius * vecl;
		vecr2 = radius * vecr;
		vecl = (radius / 2.0) * vecl;
		vecr = (radius / 2.0) * vecr;

		// endpoints from diagonal vectors

		endpoints[0] += vecl;
		endpoints[1] += vecr;
		endpoints[2] += vecl2;
		endpoints[3] += vecr2;

		// drop gain for each point on radius diagonal that is obscured

		for (count = 0, i = 0; i < 4; i++)
		{
			// UNDONE: some endpoints are in walls - in this case, trace from the wall hit location

			ray.Init( MainViewOrigin(), endpoints[i] );
			g_pEngineTraceClient->TraceRay( ray, MASK_BLOCK_AUDIO, &filter, &tr );
			
			if (tr.DidHit() && tr.fraction < 0.99 && !tr.startsolid )
			{
				count++;	// skip first obscured point: at least 2 points + center should be obscured to hear db loss
				if (count > 1)
					gain = gain * dB_To_Gain( snd_gain_db );
			}
		}
	}

	
	if ( flooping && snd_showstart.GetInt() == 7)
	{
		static float g_drop_prev = 0;
		float drop = (count-1) * snd_gain_db;

		if (drop != g_drop_prev)
		{
			DevMsg( "dB drop: %1.4f \n", drop);
			g_drop_prev = drop;
		}
	}

	// crossfade to new gain

	gain = SND_FadeToNewGain( ch, gain );

	return gain;
}

// convert sound db level to approximate sound source radius,
// used only for determining how much of sound is obscured by world

#define SND_RADIUS_MAX		(20.0 * 12.0)	// max sound source radius
#define SND_RADIUS_MIN		(2.0 * 12.0)	// min sound source radius

inline float dB_To_Radius ( float db )
{
	float radius = SND_RADIUS_MIN + (SND_RADIUS_MAX - SND_RADIUS_MIN) * (db - SND_DB_MIN) / (SND_DB_MAX - SND_DB_MIN);

	return radius;
}

struct snd_spatial_t
{
	int chan;			// 0..4 cycles through up to 5 channels
	int cycle;			// 0..2 cycles through 3 vectors per channel
	int dist[5][3];		// stores last 3 channel distance values [channel][cycle]

	float value_prev[5];	// previous value per channel

	double last_change;
};

bool g_ssp_init = false;
snd_spatial_t g_ssp;

// return 0..1 percent difference between a & b

float PercentDifference( float a, float b )
{
	float vp;

	if (!(int)a && !(int)b)
		return 0.0;

	if (!(int)a || !(int)b)
		return 1.0;

	if (a > b)
		vp = b / a;
	else
		vp = a / b;

	return (1.0 - vp);
}

// NOTE: Do not change SND_WALL_TRACE_LEN without also changing PRC_MDY6 delay value in snd_dsp.cpp!

#define SND_WALL_TRACE_LEN (100.0*12.0)		// trace max of 100' = max of 100 milliseconds of linear delay
#define SND_SPATIAL_WAIT	(0.25)			// seconds to wait between traces

// change mod delay value on chan 0..3 to v (inches)

void DSP_SetSpatialDelay( int chan, float v )
{
	// remap delay value 0..1200 to 1.0 to -1.0 for modulation

	float value = ( v / SND_WALL_TRACE_LEN) - 1.0;					// -1.0...0
	value = value * 2.0;											// -2.0...0
	value += 1.0;													// -1.0...1.0 (0...1200)
	value *= -1.0;													// 1.0...-1.0 (0...1200)

	// assume first processor in dsp_spatial is the modulating delay unit for DSP_ChangePresetValue

	int iproc = 0;

	DSP_ChangePresetValue( idsp_spatial, chan, iproc, value );		
/*

	if (chan & 0x01)
		DevMsg("RDly: %3.0f \n", v/12 );
	else
		DevMsg("LDly: %3.0f \n", v/12 );
*/
}

// use non-feedback delay to stereoize (or make quad, or quad + center) the mono dsp_room fx, 
// This simulates the average sum of delays caused by reflections
// from the left and right walls relative to the player.  The average delay
// difference between left & right wall is (l + r)/2.  This becomes the average
// delay difference between left & right ear. 
// call at most once per frame to update player->wall spatial delays

void SND_SetSpatialDelays()
{
	VPROF("SoundSpatialDelays");
	float dist, v, vp;
	Vector v_dir, v_dir2;
	int chan_max = (g_AudioDevice->IsSurround() ? 4 : 2) + (g_AudioDevice->IsSurroundCenter() ? 1 : 0);  // 2, 4, 5 channels
	
	// use listener_forward2d, which doesn't change when player looks up/down.

	Vector listener_forward2d;
	
	ConvertListenerVectorTo2D( &listener_forward2d, &listener_right );

	// init struct if 1st time through

	if ( !g_ssp_init )
	{
		Q_memset(&g_ssp, 0, sizeof(snd_spatial_t));
		g_ssp_init = true;
	}

	// return if dsp_spatial is 0
	
	if ( !dsp_spatial.GetInt() ) 
		return;
	
	// if listener has not been updated, do nothing

	if ((listener_origin  == vec3_origin) && 
		(listener_forward == vec3_origin) &&
		(listener_right	  == vec3_origin) &&
		(listener_up	  == vec3_origin) )
		return;

	if ( !SND_IsInGame() )
		return;

	// get time
	
	double dtime = g_pSoundServices->GetHostTime();
	
	// compare to previous time - if starting new check - don't check for new room until timer expires

	if (!g_ssp.chan && !g_ssp.cycle)
	{
		if (fabs(dtime - g_ssp.last_change) < SND_SPATIAL_WAIT)
			return;
	}

	// cycle through forward, left, rearward vectors, averaging to get left/right delay
	// count[chan][cycle] 0,1 0,2 0,3   1,1 1,2 1,3    2,1 2,2 2,3 ...

	g_ssp.cycle++;
	
	if (g_ssp.cycle == 3)
	{
		g_ssp.cycle = 0;

		// cycle through front left, front right, rear left, rear right, front center delays

		g_ssp.chan++;
	
		if (g_ssp.chan >= chan_max )
			g_ssp.chan = 0;
	}

	// set up traceline from player eyes to surrounding walls

	switch( g_ssp.chan )
	{
	default:
	case 0: // front left: trace max 100' 'cone' to player's left
		if ( g_AudioDevice->IsSurround() )
		{
			// 4-5 speaker case - front left
			v_dir = (-listener_right + listener_forward2d) / 2.0;
			v_dir = g_ssp.cycle ? (g_ssp.cycle == 1 ? -listener_right * 0.5: listener_forward2d * 0.5) : v_dir;
		}
		else
		{
			// 2 speaker case - left
			v_dir = listener_right * -1.0;
			v_dir2 = g_ssp.cycle ? (g_ssp.cycle == 1 ? listener_forward2d * 0.5 : -listener_forward2d * 0.5) : v_dir;
			v_dir = (v_dir + v_dir2) / 2.0;
		}
		break;

	case 1: // front right: trace max 100' 'cone' to player's right
		if ( g_AudioDevice->IsSurround() )
		{
			// 4-5 speaker case - front right
			v_dir = (listener_right + listener_forward2d) / 2.0;
			v_dir = g_ssp.cycle ? (g_ssp.cycle == 1 ? listener_right * 0.5: listener_forward2d * 0.5) : v_dir;
		}
		else
		{
			// 2 speaker case - right
			v_dir = listener_right;
			v_dir2 = g_ssp.cycle ? (g_ssp.cycle == 1 ? listener_forward2d * 0.5 : -listener_forward2d * 0.5) : v_dir;
			v_dir = (v_dir + v_dir2) / 2.0;
		}
		break;

	case 2: // rear left: trace max 100' 'cone' to player's rear left
		v_dir = (listener_right + listener_forward2d) / -2.0;
		v_dir = g_ssp.cycle ? (g_ssp.cycle == 1 ? -listener_right * 0.5 : -listener_forward2d * 0.5) : v_dir;
		break;

	case 3: // rear right: trace max 100' 'cone' to player's rear right
		v_dir = (listener_right - listener_forward2d) / 2.0;
		v_dir = g_ssp.cycle ? (g_ssp.cycle == 1 ? listener_right * 0.5: -listener_forward2d * 0.5) : v_dir;
		break;
		
	case 4: // front center: trace max 100' 'cone' to player's front
		v_dir = listener_forward2d;
		v_dir2 = g_ssp.cycle ? (g_ssp.cycle == 1 ? listener_right * 0.15 : -listener_right * 0.15) : v_dir;
		v_dir = (v_dir + v_dir2);
		break;
	}

	Vector endpoint;	
	trace_t tr;
	CTraceFilterWorldOnly filter;

	endpoint = MainViewOrigin() + v_dir * SND_WALL_TRACE_LEN;
	Ray_t ray;
	ray.Init( MainViewOrigin(), endpoint );
	g_pEngineTraceClient->TraceRay( ray, MASK_BLOCK_AUDIO, &filter, &tr );

	dist = SND_WALL_TRACE_LEN;

	if ( tr.DidHit() )
	{
		dist = VectorLength( tr.endpos - MainViewOrigin() );	
	}
	
	g_ssp.dist[g_ssp.chan][g_ssp.cycle] = dist;

	// set new result in dsp_spatial delay params when all delay values have been filled in

	if (!g_ssp.cycle && !g_ssp.chan)
	{
		// update delay for each channel

		for (int chan = 0; chan < chan_max; chan++)
		{
			// compute average of 3 traces per channel

			v = (g_ssp.dist[chan][0] + g_ssp.dist[chan][1] + g_ssp.dist[chan][2]) / 3.0;
			vp = g_ssp.value_prev[chan];

			// only change if 10% difference from previous

			if ((vp != v) && int(v) && (PercentDifference( v, vp ) >= 0.1))		
			{
				// update when we have data for all L/R && RL/RR channels...

				if (chan & 0x1)
				{
					float vr = fpmin( v, (50*12.0f) );
					float vl = fpmin(g_ssp.value_prev[chan-1], (50*12.0f));

/* UNDONE: not needed, now that this applies only to dsp 'room' buffer

					// ensure minimum separation = average distance to walls

					float dmin = (vl + vr) / 2.0;		// average distance to walls
					float d = vl - vr;					// l/r separation

					// if separation is less than average, increase min

					if (abs(d) < dmin/2)
					{
						if (vl > vr)
							vl += dmin/2 - d;
						else
							vr += dmin/2 - d;
					}
*/
					DSP_SetSpatialDelay(chan-1, vl);
					DSP_SetSpatialDelay(chan, vr);
				}
				
				// update center chan

				if (chan == 4)
				{
					float vl = fpmin( v, (50*12.0f) );
					DSP_SetSpatialDelay(chan, vl);
				}
			}
			
			g_ssp.value_prev[chan] = v;

		}

		// update wait timer now that all values have been checked

		g_ssp.last_change = dtime;		
	}	
}

// Dsp Automatic Selection:

//	a) enabled by setting dsp_room to DSP_AUTOMATIC.  Subsequently, dsp_automatic is the actual dsp value for dsp_room.
//	b) disabled by setting dsp_room to anything else

//	c) while enabled, detection nodes are placed as player moves into a new space
//     i.	at each node, a new dsp setting is calculated and dsp_automatic is set to an appropriate preset
//     ii.	new nodes are set when player moves out of sight of previous node
//     iii. moving into line of sight of a detection node causes closest node to player to set dsp_automatic

// see void DAS_CheckNewRoomDSP( ) for main entrypoint

ConVar das_debug( "adsp_debug", "0", FCVAR_ARCHIVE );
												// >0: draw blue dsp detection node location
												// >1: draw green room trace height detection bars
												// 3: draw yellow horizontal trace bars for room width/depth detection
												// 4: draw yellow upward traces for height detection
												// 5: draw teal box around all props around player
												// 6: draw teal box around room as detected

#define DAS_CWALLS				20				// # of wall traces to save for calculating room dimensions
#define DAS_ROOM_TRACE_LEN		(400.0*12.0)	// max size of trace to check for room dimensions

#define DAS_AUTO_WAIT	0.25					// wait min of n seconds between dsp_room changes and update checks

#define DAS_WIDTH_MIN	0.4						// min % change in avg width of any wall pair to cause new dsp
#define DAS_REFL_MIN	0.5						// min % change in avg refl of any wall to cause new dsp
#define DAS_SKYHIT_MIN	0.8						// min % change in # of sky hits per wall

#define DAS_DIST_MIN	(4.0 * 12.0)			// min distance between room dsp changes
#define DAS_DIST_MAX	(40.0 * 12.0)			// max distance to preserve room dsp changes

#define DAS_DIST_MIN_OUTSIDE	(6.0 * 12.0)	// min distance between room dsp changes outside
#define DAS_DIST_MAX_OUTSIDE	(100.0 * 12.0)	// max distance to preserve room dsp changes outside

#define IVEC_DIAG_UP	8						// start of diagonal up vectors
#define IVEC_UP			18						// up vector
#define IVEC_DOWN		19						// down vector

#define DAS_REFLECTIVITY_NORM	0.5
#define DAS_REFLECTIVITY_SKY	0.0

// auto dsp room struct

struct das_room_t
{
	int   dist[DAS_CWALLS];		// distance in units from player to axis aligned and diagonal walls
	float reflect[DAS_CWALLS];	// acoustic reflectivity per wall
	float skyhits[DAS_CWALLS];	// every sky hit adds 0.1
	Vector hit[DAS_CWALLS];		// location of trace hit on wall - used for calculating average centers
	Vector norm[DAS_CWALLS];	// wall normal at hit location

	Vector vplayer;				// 'frozen' location above player's head

	Vector vplayer_eyes;		// 'frozen' location player's eyes

	int width_max;				// max width
	int length_max;				// max length
	int height_max;				// max height
		
	float refl_avg;				// running average of reflectivity of all walls
	float refl_walls[6];		// left,right,front,back,ceiling,floor reflectivities	

	float sky_pct;				// percent of sky hits
	
	Vector room_mins;			// room bounds
	Vector room_maxs;

	double last_dsp_change;		// time since last dsp change

	float diffusion;			// 0..1.0 check radius (avg of width_avg) for # of props - scale diffusion based on # found
	short iwall;					// cycles through walls 0..5, ensuring only one trace per frame
	short ent_count;	 			// count of entities found in radius
	bool bskyabove;				// true if sky found above player (ie: outside)
	bool broomready;			// true if all distances are filled in and room is ready to check
	short lowceiling;				// if non-zero, ceiling directly above player if < 112 units
};

// dsp detection node

struct das_node_t
{
	Vector vplayer;				// position

	bool fused;					// true if valid node
	bool fseesplayer;			// true if node sees player on last check
	short dsp_preset;				// preset
		
	int range_min;				// min,max detection ranges
	int range_max;
	
	int dist;					// last distance to player

	// room parameters when node was created:

	das_room_t room;
};

#define DAS_CNODES	40					// keep around last n nodes - must be same as DSP_CAUTO_PRESETS!!!

das_node_t g_das_nodes[DAS_CNODES];		// all dsp detection nodes
das_node_t *g_pdas_last_node = NULL;	// last node that saw player

int g_das_check_next;					// next node to check
int g_das_store_next;					// next place to store node
bool g_das_all_checked;					// true if all nodes checked
int g_das_checked_count;				// count of nodes checked in latest pass

das_room_t g_das_room;					// room detector

bool g_bdas_room_init = 0;
bool g_bdas_init_nodes = 0;
bool g_bdas_create_new_node = 0;

bool DAS_TraceNodeToPlayer( das_room_t *proom, das_node_t *pnode );
void DAS_InitAutoRoom( das_room_t *proom);
void DAS_DebugDrawTrace ( trace_t *ptr, int r, int g, int b, float duration, int imax );

Vector g_das_vec3[DAS_CWALLS];	// trace vectors to walls, ceiling, floor

void DAS_InitNodes( void )
{
	Q_memset(g_das_nodes, 0, sizeof(das_node_t) * DAS_CNODES);
	g_das_check_next = 0;
	g_das_store_next = 0;
	g_das_all_checked = 0;
	g_das_checked_count = 0;

	// init all rooms

	for (int i = 0; i < DAS_CNODES; i++)
		DAS_InitAutoRoom( &(g_das_nodes[i].room) );

	// init trace vectors
	// set up trace vectors for max, min width
	float vl = DAS_ROOM_TRACE_LEN;
	float vlu = DAS_ROOM_TRACE_LEN * 0.52;
	float vlu2 = DAS_ROOM_TRACE_LEN * 0.48;	// don't use 'perfect' diagonals

	g_das_vec3[0].Init(vl, 0.0, 0.0);				// x left
	g_das_vec3[1].Init(-vl, 0.0, 0.0);				// x right

	g_das_vec3[2].Init(0.0, vl, 0.0);				// y front
	g_das_vec3[3].Init(0.0, -vl, 0.0);				// y back

	g_das_vec3[4].Init(-vlu, vlu2, 0.0);			// diagonal front left
	g_das_vec3[5].Init(vlu, -vlu2, 0.0);			// diagonal rear right

	g_das_vec3[6].Init(vlu, vlu2, 0.0);				// diagonal front right
	g_das_vec3[7].Init(-vlu, -vlu2, 0.0);			// diagonal rear left

	// set up trace vectors for max height - on x=y diagonal

	g_das_vec3[8].Init(vlu, vlu2, vlu/2.0);			// front right up A x,y,z/2		(IVEC_DIAG_UP)
	g_das_vec3[9].Init(vlu, vlu2, vlu);				// front right up B x,y,z
	g_das_vec3[10].Init(vlu/2.0, vlu2/2.0, vlu);	// front right up C x/2,y/2,z

	g_das_vec3[11].Init(-vlu, -vlu2, vlu/2.0);		// rear left up A -x,-y,z/2
	g_das_vec3[12].Init(-vlu, -vlu2, vlu);			// rear left up B -x,-y,z
	g_das_vec3[13].Init(-vlu/2.0, -vlu2/2.0, vlu);	// rear left up C -x/2,-y/2,z

	// set up trace vectors for max height - on x axis & y axis

	g_das_vec3[14].Init(-vlu, 0, vlu);				// left up B -x,0,z
	g_das_vec3[15].Init(0, vlu/2.0, vlu);			// front up C -x/2,0,z

	g_das_vec3[16].Init(0, -vlu, vlu);				// rear up B x,0,z
	g_das_vec3[17].Init(vlu/2.0, 0, vlu);			// right up C x/2,0,z

	g_das_vec3[18].Init(0.0, 0.0, vl);				// up	(IVEC_UP)
	g_das_vec3[19].Init(0.0, 0.0, -vl);				// down (IVEC_DOWN)
}

void DAS_InitAutoRoom( das_room_t *proom)
{
		Q_memset(proom, 0, sizeof (das_room_t));
}

// reset all nodes for next round of visibility checks between player & nodes

void DAS_ResetNodes( void )
{
	for (int i = 0; i < DAS_CNODES; i++)
	{
		g_das_nodes[i].fseesplayer = false;
		g_das_nodes[i].dist = 0;
	}

	g_das_all_checked = false;
	g_das_checked_count = 0;
	g_bdas_create_new_node = false;
}

// utility function - return next index, wrap at max

int DAS_GetNextIndex( int *pindex, int max )
{
	int i = *pindex;
	int j;

	j = i+1;
	if ( j >= max )
		j = 0;

	*pindex = j;

	return i;
}

// returns true if dsp node is within range of player

bool DAS_NodeInRange( das_room_t *proom, das_node_t *pnode )
{
	float dist;

	dist = VectorLength( proom->vplayer - pnode->vplayer );

	// player can still see previous room selection point, and it's less than n feet away, 
	// then flag this node as visible

	pnode->dist = dist;
	
	return ( dist <= pnode->range_max );
}

// update next valid node - set up internal node state if it can see player
// called once per frame
// returns true if all nodes have been checked

bool DAS_CheckNextNode( das_room_t *proom )
{
	int i, j;

	if ( g_das_all_checked )
		return true;

	// find next valid node

	for (j = 0; j < DAS_CNODES; j++)
	{
		// track number of nodes checked

		g_das_checked_count++;

		// get next node in range to check

		i = DAS_GetNextIndex( &g_das_check_next, DAS_CNODES );

		if ( g_das_nodes[i].fused && DAS_NodeInRange( proom, &(g_das_nodes[i]) ) )
		{
			// trace to see if player can still see node, 
			// if so stop checking

			if ( DAS_TraceNodeToPlayer( proom, &(g_das_nodes[i]) ))
				goto checknode_exit;
		}
	}

checknode_exit:

	// flag that all nodes have been checked

	if ( g_das_checked_count >= DAS_CNODES )
		g_das_all_checked = true;	

	return g_das_all_checked;
}


int DAS_GetNextNodeIndex()
{
	return g_das_store_next;
}
// store new node for room

void DAS_StoreNode( das_room_t *proom, int dsp_preset)
{
	// overwrite node in cyclic list
	
	int i = DAS_GetNextIndex( &g_das_store_next, DAS_CNODES );

	g_das_nodes[i].dsp_preset = dsp_preset;
	g_das_nodes[i].fused = true;
	g_das_nodes[i].vplayer = proom->vplayer;

	// calculate node scanning range_max based on room size
	
	if ( !proom->bskyabove )
	{
		// inside range - halls & tunnels have nodes every 5*width
		g_das_nodes[i].range_max = fpmin((int)DAS_DIST_MAX, min(proom->width_max * 5, proom->length_max) ); 
		g_das_nodes[i].range_min = DAS_DIST_MIN;
	}
	else
	{
		// outside range
		g_das_nodes[i].range_max = DAS_DIST_MAX_OUTSIDE;
		g_das_nodes[i].range_min = DAS_DIST_MIN_OUTSIDE;
	}

	g_das_nodes[i].fseesplayer = false;
	g_das_nodes[i].dist = 0;

	g_das_nodes[i].room = *proom;

	// update last node visible as this node

	g_pdas_last_node = &(g_das_nodes[i]);
}

// check all updated nodes,
// return dsp_preset of largest node (by area) that can see player
// return -1 if no preset found

// NOTE: outside nodes can't see player if player is inside and vice versa
// foutside is true if player is outside

int DAS_GetDspPreset( bool foutside )
{
	int dsp_preset = -1;

	int i;
	// int dist_min = 100000;
	int area_max = 0;
	int area;

	// find node that represents room with greatest floor area, return its preset.
	
	for (i = 0; i < DAS_CNODES; i++)
	{
		if (g_das_nodes[i].fused && g_das_nodes[i].fseesplayer)
		{
			area = (g_das_nodes[i].room.width_max * g_das_nodes[i].room.length_max);
			
			if ( g_das_nodes[i].room.bskyabove == foutside )
			{
				if (area > area_max)
				{
					area_max = area;
					dsp_preset = g_das_nodes[i].dsp_preset;

					// save pointer to last node that saw player

					g_pdas_last_node = &(g_das_nodes[i]);
				}
			}			
/*		

			// find nearest node, return its preset

			if (g_das_nodes[i].dist < dist_min)
			{
				if ( g_das_nodes[i].room.bskyabove == foutside )
				{
					dist_min = g_das_nodes[i].dist;
					dsp_preset = g_das_nodes[i].dsp_preset;

					// save pointer to last node that saw player

					g_pdas_last_node = &(g_das_nodes[i]);
		
				}
			}
*/
		}
	}
	
	return dsp_preset;
}

// custom trace filter: 
// a) never hit player or monsters or entities
// b) always hit world, or moveables or static props

class CTraceFilterDAS : public ITraceFilter
{
public:
	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		IClientUnknown *pUnk = static_cast<IClientUnknown*>(pHandleEntity);
		IClientEntity *pEntity;

		if ( !pUnk )
			return false;

		// don't hit non-collideable props

		if ( StaticPropMgr()->IsStaticProp( pHandleEntity ) )
		{

			ICollideable *pCollide = StaticPropMgr()->GetStaticProp( pHandleEntity);
			if (!pCollide)
				return false;
		}

		// don't hit any ents

		pEntity = pUnk->GetIClientEntity();
		
		if ( pEntity )
			return false;

		return true;
	}

	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_EVERYTHING_FILTER_PROPS;
	}
};

#define DAS_TRACE_MASK		(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW)

// returns true if clear line exists between node and player
// if node can see player, sets up node distance and flag fseesplayer

bool DAS_TraceNodeToPlayer( das_room_t *proom, das_node_t *pnode )
{
	trace_t trP;
	CTraceFilterDAS filterP;
	bool fseesplayer = false;
	float dist;
	Ray_t ray;
	ray.Init( proom->vplayer, pnode->vplayer );
	
	g_pEngineTraceClient->TraceRay( ray, DAS_TRACE_MASK, &filterP, &trP );
	dist = VectorLength( proom->vplayer - pnode->vplayer );

	// player can still see previous room selection point, and it's less than n feet away, 
	// then flag this node as visible

	if ( !trP.DidHit() && (dist <= DAS_DIST_MAX) )
	{
		fseesplayer = true;
		pnode->dist = dist;
	}
	
	pnode->fseesplayer = fseesplayer;

	return fseesplayer;
}

// update room boundary maxs, mins

void DAS_SetRoomBounds( das_room_t *proom, Vector &hit, bool bheight )
{
	Vector maxs, mins;

	maxs = proom->room_maxs;
	mins = proom->room_mins;

	if (!bheight)
	{
		if (hit.x > maxs.x)
			maxs.x = hit.x;

		if (hit.x < mins.x)
			mins.x = hit.x;

		if (hit.z > maxs.z)
			maxs.z = hit.z;

		if (hit.z < mins.z)
			mins.z = hit.z;
	}

	if (bheight)
	{
		if (hit.y > maxs.y)
			maxs.y = hit.y;

		if (hit.y < mins.y)
			mins.y = hit.y;
	}

	proom->room_maxs = maxs;
	proom->room_mins = mins;
}

// when all walls are updated, calculate max length, width, height, reflectivity, sky hit%, room center
// returns true if room parameters are in good location to place a node
// returns false if room parameters are not in good location to place a node
// note: false occurs if up vector doesn't hit sky, but one or more up diagonal vectors do hit sky

bool DAS_CalcRoomProps( das_room_t *proom )
{
	int length_max = 0;
	int width_max = 0;
	int height_max = 0;
	int dist[4];
	float area1, area2;
	int height;
	int i;
	int j;
	int k;
	bool b_diaghitsky = false;

	// reject this location if up vector doesn't hit sky, but 
	// one or more up diagonals do hit sky - 
	// in this case, player is under a slight overhang, narrow bridge, or
	// standing just inside a window or doorway. keep looking for better node location
	
	for (i = IVEC_DIAG_UP; i < IVEC_UP; i++)
	{
		if (proom->skyhits[i] > 0.0)
			b_diaghitsky = true;
	}

	if (b_diaghitsky && !(proom->skyhits[IVEC_UP] > 0.0))
		return false;

	// get all distance pairs

	for (i = 0; i < IVEC_DIAG_UP; i+=2)
		dist[i/2] = proom->dist[i] + proom->dist[i+1];	// 1st pair is width
	
	// if areas differ by more than 25%
	// select the pair with the greater area

	// if areas do not differ by more than 25%, select the pair with the 
	// longer measured distance. Filters incorrect selection due to diagonals.

	area1 = (float)(dist[0] * dist[1]);
	area2 = (float)(dist[2] * dist[3]);

	area1 = (int)area1 == 0 ? 1.0 : area1;
	area2 = (int)area2 == 0 ? 1.0 : area2;
	
	if ( PercentDifference(area1, area2) > 0.25 )
	{
		// areas are more than 25% different - select pair with greater area

		j = area1 > area2 ? 0 : 2;
	}
	else
	{
		// select pair with longer measured distance

		int iMaxDist = 0; // index to max dist
		int dmax = 0;

		for (i = 0; i < 4; i++)
		{
			if (dist[i] > dmax)
			{
				dmax = dist[i];
				iMaxDist = i;
			}
		}

		j = iMaxDist > 1 ? 2 : 0;
	}

	
	// width is always the smaller of the dimensions

	width_max = min (dist[j], dist[j+1]);
	length_max = max (dist[j], dist[j+1]);

	// get max height

	for (i = IVEC_DIAG_UP; i < IVEC_DOWN; i++)
	{
		height = proom->dist[i];

		if (height > height_max)
			height_max = height;
	}

	proom->length_max = length_max;
	proom->width_max = width_max;
	proom->height_max = height_max;
			
	// get room max,min from chosen width, depth
	// 0..3 or 4..7

	for ( i = j*2; i < 4+(j*2); i++)
		DAS_SetRoomBounds( proom, proom->hit[i], false );
	
	// get room height min from down trace

	proom->room_mins.z = proom->hit[IVEC_DOWN].z;

	// reset room height max to player trace height

	proom->room_maxs.z = proom->vplayer.z;

	// draw box around room max,min

	if (das_debug.GetInt() == 6)
	{
		// draw box around all objects detected
		Vector maxs = proom->room_maxs;
		Vector mins = proom->room_mins;
		Vector orig = (maxs + mins) / 2.0;
		Vector absMax = maxs - orig;
		Vector absMin = mins - orig;

		CDebugOverlay::AddBoxOverlay( orig, absMax, absMin, vec3_angle, 255, 0, 255, 0, 60.0f );
	}
	// calculate average reflectivity

	float refl = 0.0;

	// average reflectivity for walls

	// 0..3 or 4..7

	for ( k = 0, i = j*2; i < 4+(j*2); i++, k++)
	{
		refl += proom->reflect[i];
		proom->refl_walls[k] = proom->reflect[i];
	}
	
	// assume ceiling is open

	proom->refl_walls[4] = 0.0;	 

	// get ceiling reflectivity, if any non zero

	for ( i = IVEC_DIAG_UP; i < IVEC_DOWN; i++)
	{
		if (proom->reflect[i] == 0.0)
		{
			// if any upward trace hit sky, exit;
			// ceiling reflectivity is 0.0

			proom->refl_walls[4] = 0.0;

			i = IVEC_DOWN;	// exit loop
		}
		else
		{

			// upward trace didn't hit sky, keep checking

			proom->refl_walls[4] = proom->reflect[i];	
		}
	}

	// add in ceiling reflectivity, if any
	
	refl += proom->refl_walls[4];

	// get floor reflectivity
		
	refl += proom->reflect[IVEC_DOWN];
	proom->refl_walls[5] = proom->reflect[IVEC_DOWN];

	proom->refl_avg = refl / 6.0;

	// calculate sky hit percent for this wall

	float sky_pct = 0.0;

	// 0..3 or 4..7

	for ( i = j*2; i < 4+(j*2); i++)
		sky_pct += proom->skyhits[i];
	
	for ( i = IVEC_DIAG_UP; i < IVEC_DOWN; i++)
	{
		if (proom->skyhits[i] > 0.0)
		{
			// if any upward trace hit sky, exit loop
			sky_pct += proom->skyhits[i];
			i = IVEC_DOWN;
		}
	}

	// get floor skyhit

	sky_pct += proom->skyhits[IVEC_DOWN];

	proom->sky_pct = sky_pct;

	// check for sky above
	proom->bskyabove = false;

	for (i = IVEC_DIAG_UP; i < IVEC_DOWN; i++)
	{
		if (proom->skyhits[i] > 0.0)
			proom->bskyabove = true;
	}

	return true;
}

// return true if trace hit solid
// return false if trace hit sky or didn't hit anything

bool DAS_HitSolid( trace_t *ptr )
{
	// if hit nothing return false

	if (!ptr->DidHit())
		return false;
	
	// if hit sky, return false (not solid)
	if (ptr->surface.flags & SURF_SKY)
		return false;

	return true;
}

// returns true if trace hit sky

bool DAS_HitSky( trace_t *ptr )
{
	if (ptr->DidHit() && (ptr->surface.flags & SURF_SKY))
		return true;
	if (!ptr->DidHit() )
	{
		float dz = ptr->endpos.z - ptr->startpos.z;
		if ( dz > 200*12.0f )
			return true;
	}
	return false;
}


bool DAS_ScanningForHeight( das_room_t *proom )
{
	return (proom->iwall >= IVEC_DIAG_UP);
}

bool DAS_ScanningForWidth( das_room_t *proom )
{
	return (proom->iwall < IVEC_DIAG_UP);
}

bool DAS_ScanningForFloor( das_room_t *proom )
{
	return (proom->iwall == IVEC_DOWN);
}

ConVar das_door_height("adsp_door_height", "112"); // standard door height hl2
ConVar das_wall_height("adsp_wall_height", "128"); // standard wall height hl2
ConVar das_low_ceiling("adsp_low_ceiling", "108"); // low ceiling height hl2


// set origin for tracing out to walls to point above player's head
// allows calculations over walls and floor obstacles, and above door openings

// WARNING: the current settings are optimal for skipping floor and ceiling clutter,
// and for detecting rooms without 'looking' through doors or windows. Don't change these cvars for hl2!

void DAS_SetTraceHeight( das_room_t *proom, trace_t *ptrU, trace_t *ptrD )
{
	// NOTE: when tracing down through player's box, endpos and startpos are reversed and 
	// startsolid and allsolid are true.

	int zup = abs(ptrU->endpos.z - ptrU->startpos.z);		// height above player's head
	int zdown = abs(ptrD->endpos.z - ptrD->startpos.z);		// distance to floor from player's head
	int h;
	h = zup + zdown;
	
	int door_height = das_door_height.GetInt();
	int wall_height = das_wall_height.GetInt();
	int low_ceiling = das_low_ceiling.GetInt();
	
	if (h > low_ceiling && h <= wall_height)
	{
		// low ceiling - trace out just above standard door height @ 112
		if (h > door_height)
			proom->vplayer.z = fpmin(ptrD->endpos.z, ptrD->startpos.z) + door_height + 1;	
		else
			proom->vplayer.z = fpmin(ptrD->endpos.z, ptrD->startpos.z) + h - 1;
	}
	else if ( h > wall_height )
	{
		// tall ceiling - trace out over standard walls @ 128

		proom->vplayer.z = fpmin(ptrD->endpos.z, ptrD->startpos.z) + wall_height + 1;
	}
	else
	{
		// very low ceiling, trace out from just below ceiling
		proom->vplayer.z = fpmin(ptrD->endpos.z, ptrD->startpos.z) + h - 1;
		proom->lowceiling = h;
	}		

	Assert (proom->vplayer.z <= ptrU->endpos.z);

	if (das_debug.GetInt() > 1)
	{
		// draw line to height, and between floor and ceiling

		CDebugOverlay::AddLineOverlay( ptrD->endpos, ptrU->endpos, 0, 255, 0, 255, false, 20 );
		
		Vector mins;
		Vector maxs;
		mins.Init(-1,-1,-2.0);
		maxs.Init(1,1,0);

		CDebugOverlay::AddBoxOverlay( proom->vplayer, mins, maxs, vec3_angle, 255, 0, 0, 0, 20 );

		CDebugOverlay::AddBoxOverlay( ptrU->endpos, mins, maxs, vec3_angle, 0, 255, 0, 0, 20 );
		CDebugOverlay::AddBoxOverlay( ptrD->endpos, mins, maxs, vec3_angle, 0, 255, 0, 0, 20 );

	}
}

// prepare room struct for new round of checks:
// clear out struct,
// init trace height origin by finding space above player's head
// returns true if player is in valid position to begin checks from 

bool DAS_StartTraceChecks( das_room_t *proom )
{
	// starting new check: store player position, init maxs, mins

	proom->vplayer_eyes = MainViewOrigin();
	proom->vplayer = MainViewOrigin();

	proom->height_max = 0;
	proom->width_max = 0;
	proom->length_max = 0;
	proom->room_maxs.Init (0.0, 0.0, 0.0);
	proom->room_mins.Init (10000.0, 10000.0, 10000.0);

	proom->lowceiling = 0;

	// find point between player's head and ceiling - trace out to walls from here

	trace_t trU, trD;	
	CTraceFilterDAS filterU, filterD;

	Vector v_dir = g_das_vec3[IVEC_DOWN];	// down - find floor

	Vector endpoint = proom->vplayer + v_dir;

	Ray_t ray;
	ray.Init( proom->vplayer, endpoint );
	
	g_pEngineTraceClient->TraceRay( ray,  DAS_TRACE_MASK, &filterD, &trD );

	// if player jumping or in air, don't continue

	if (trD.DidHit() && abs(trD.endpos.z - trD.startpos.z) > 72)
		return false;

	v_dir = g_das_vec3[IVEC_UP];			// up - find ceiling

	endpoint = proom->vplayer + v_dir;

	ray.Init( proom->vplayer, endpoint );

	g_pEngineTraceClient->TraceRay( ray, DAS_TRACE_MASK, &filterU, &trU );

	// if down trace hits floor, set trace height, otherwise default is player eye location

	if ( DAS_HitSolid( &trD) )
		DAS_SetTraceHeight( proom, &trU, &trD );
	
	return true;
}

void DAS_DebugDrawTrace ( trace_t *ptr, int r, int g, int b, float duration, int imax)
{

	// das_debug == 3: draw horizontal trace bars for room width/depth detection
	// das_debug == 4: draw upward traces for height detection

	if (das_debug.GetInt() != imax)
		return;

	CDebugOverlay::AddLineOverlay( ptr->startpos, ptr->endpos, r, g, b, 255, false, duration );
	
	Vector mins;
	Vector maxs;
	mins.Init(-1,-1,-2.0);
	maxs.Init(1,1,0);

	CDebugOverlay::AddBoxOverlay( ptr->endpos, mins, maxs, vec3_angle, r, g, b, 0, duration );

}

// wall surface data

struct das_surfdata_t
{
	float dist;				// distance to player
	float reflectivity;		// acoustic reflectivity of material on surface
	Vector hit;				// trace hit location
	Vector norm;			// wall normal at hit location
};

// trace hit wall surface, get info about surface and store in surfdata struct
// if scanning for height, bounce a second trace off of ceiling and get dist to floor

void DAS_GetSurfaceData( das_room_t *proom, trace_t *ptr, das_surfdata_t *psurfdata )
{

	float dist;				// distance to player
	float reflectivity;		// acoustic reflectivity of material on surface
	Vector hit;				// trace hit location
	Vector norm;			// wall normal at hit location
	surfacedata_t *psurf;

	psurf = physprop->GetSurfaceData( ptr->surface.surfaceProps );
	
	reflectivity = psurf ? psurf->audio.reflectivity : DAS_REFLECTIVITY_NORM;

	// keep wall hit location and normal, to calc room bounds and center

	norm = ptr->plane.normal;

	// get length to hit location

	dist = VectorLength(ptr->endpos - ptr->startpos);

	// if started tracing from within player box, startpos & endpos may be flipped

	if (ptr->endpos.z >= ptr->startpos.z)
		hit = ptr->endpos;	
	else
		hit = ptr->startpos;

	// if checking for max height by bouncing several vectors off of ceiling:
	// ignore returned normal from 1st bounce, just search straight down from trace hit location

	if ( DAS_ScanningForHeight( proom ) && !DAS_ScanningForFloor( proom ) )
	{
		trace_t tr2;
		CTraceFilterDAS filter2;

		norm.Init(0.0, 0.0, -1.0);

		Vector endpoint = hit + ( norm * DAS_ROOM_TRACE_LEN );
		
		Ray_t ray;
		ray.Init( hit, endpoint );

		g_pEngineTraceClient->TraceRay( ray, DAS_TRACE_MASK, &filter2, &tr2 );

		//DAS_DebugDrawTrace( &tr2, 255, 255, 0, 10, 1);

		if (tr2.DidHit())
		{
			// get distance between surfaces

			dist = VectorLength(tr2.endpos - tr2.startpos);
		}
	}

	// set up surface struct and return

	psurfdata->dist = dist;
	psurfdata->hit = hit;
	psurfdata->norm = norm;
	psurfdata->reflectivity = reflectivity;

}


// algorithm for detecting approximate size of space around player. Handles player in corner & non-axis aligned rooms.
// also handles player on catwalk or player under small bridge/overhang.  
// The goal is to only change the dsp room description if the the player moves into 
// a space which is SIGNIFICANTLY different from the previously set dsp space.

// save player position. find a point above player's head and trace out from here.

// from player position, get max width and max length:

// from player position, 
// a) trace x,-x, y,-y axes 
// b) trace xy, -xy, x-y, -x-y diagonals
// c) select largest room size detected from max width, max length


// from player position, get height
// a) trace out along front-up (or left-up, back-up, right-up), save hit locations
// b) trace down -z from hit locations
// c) save max height

// when max width, max length, max height all updated, get new player position

// get average room size & wall materials:
// update averages with one traceline per frame only
// returns true if room is fully updated and ready to check

bool DAS_UpdateRoomSize( das_room_t *proom )
{
	Vector endpoint;
	Vector startpoint;
	Vector v_dir;
	int iwall;
	bool bskyhit = false;
	das_surfdata_t surfdata;

	// do nothing if room already fully checked

	if ( proom->broomready )
		return true;

	// cycle through all walls, floor, ceiling
	// get wall index 

	iwall = proom->iwall;
	
	// get height above player and init proom for new round of checks

	if (iwall == 0)
	{
		if (!DAS_StartTraceChecks( proom ))
			return false;		// bad location to check room - player is jumping etc.
	}

	// get trace vector

	v_dir = g_das_vec3[iwall];

	// trace out from trace origin, in axis-aligned direction or along diagonals

	// if looking for max height, trace from top of player's eyes

	if ( DAS_ScanningForHeight( proom ) )
	{
		startpoint = proom->vplayer_eyes;
		endpoint = proom->vplayer_eyes + v_dir;
	}
	else
	{
		startpoint = proom->vplayer;
		endpoint = proom->vplayer + v_dir;
	}

	// try less expensive world-only trace first (no props, no ents - just try to hit walls)

	trace_t tr;
	CTraceFilterWorldOnly filter;

	Ray_t ray;
	ray.Init( startpoint, endpoint );

	g_pEngineTraceClient->TraceRay( ray, CONTENTS_SOLID, &filter, &tr );

	// if didn't hit world, or we hit sky when looking horizontally,
	// retrace, this time including props

	if ( !DAS_HitSolid( &tr ) && DAS_ScanningForWidth( proom ) )
	{
		CTraceFilterDAS filterDas;

		ray.Init( startpoint, endpoint );
		g_pEngineTraceClient->TraceRay( ray, DAS_TRACE_MASK, &filterDas, &tr );
	}
	
	if (das_debug.GetInt() > 2)
	{
		// draw trace lines

		if ( DAS_HitSolid( &tr ) )
			DAS_DebugDrawTrace( &tr, 0, 255, 255, 10, DAS_ScanningForHeight( proom ) + 3);	
		else
			DAS_DebugDrawTrace( &tr, 255, 0, 0, 10, DAS_ScanningForHeight( proom ) + 3);	// red lines if sky hit or no hit
	}

	// init surface data with defaults, in case we didn't hit world

	surfdata.dist			= DAS_ROOM_TRACE_LEN;
	surfdata.reflectivity	= DAS_REFLECTIVITY_SKY;	// assume sky or open area
	surfdata.hit			= endpoint;				// trace hit location
	surfdata.norm			= -v_dir;

	// check for sky hits

	if ( DAS_HitSky( &tr ) )
	{
		bskyhit = true;

		if ( DAS_ScanningForWidth( proom ) )
			// ignore horizontal sky hits for distance calculations
			surfdata.dist = 1.0;
		else
			surfdata.dist = surfdata.dist; // debug
	}

	// get length of trace if it hit world

	// if hit solid and not sky (tr.DidHit() && !bskyhit)
	// get surface information

	if ( DAS_HitSolid( &tr) )
		DAS_GetSurfaceData( proom, &tr, &surfdata );
	
	// store surface data

	proom->dist[iwall]		= surfdata.dist;
	proom->reflect[iwall]	= clamp(surfdata.reflectivity, 0.0f, 1.0f);
	proom->skyhits[iwall]	= bskyhit ? 0.1 : 0.0;
	proom->hit[iwall]		= surfdata.hit;
	proom->norm[iwall]		= surfdata.norm;

	// update wall counter

	proom->iwall++;
	
	if (proom->iwall == DAS_CWALLS)
	{
		bool b_good_node_location;

		// calculate room mins, maxs, reflectivity etc

		b_good_node_location = DAS_CalcRoomProps( proom );

		// reset wall counter

		proom->iwall = 0;
		proom->broomready = b_good_node_location;	// room ready to check if good node location

		return b_good_node_location;	
	}

	return false;			// room not yet fully updated
}

// create entity enumerator for counting ents & summing volume of ents in room

class CDasEntEnum : public IPartitionEnumerator
{
	public:
	int m_count;		// # of ents in space
	float m_volume;		// space occupied by ents

	public:
	
	void Reset()
	{
		m_count = 0;
		m_volume = 0.0;
	}

	// called with each handle...

	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{		
		float vol;

		// get bounding box of entity
		// Generate a collideable
		
		ICollideable *pCollideable = g_pEngineTraceClient->GetCollideable( pHandleEntity );

		if ( !pCollideable )
			return ITERATION_CONTINUE;

		// Check for solid

		if ( !IsSolid( pCollideable->GetSolid(), pCollideable->GetSolidFlags() ) )
			return ITERATION_CONTINUE;
		
		m_count++;
		
		// compute volume of space occupied by entity
		Vector mins = pCollideable->OBBMins();
		Vector maxs = pCollideable->OBBMaxs();
		
		vol = fabs((maxs.x - mins.x) * (maxs.y - mins.y) * (maxs.z - mins.z));

		m_volume += vol;	// add to total vol

		if (das_debug.GetInt() == 5)
		{
			// draw box around all objects detected

			Vector orig = pCollideable->GetCollisionOrigin();
			CDebugOverlay::AddBoxOverlay( orig, mins, maxs, pCollideable->GetCollisionAngles(), 255, 0, 255, 0, 60.0f );
		}

		return ITERATION_CONTINUE;
	}
};

// determine # of solid ents/props within detected room boundaries
// and set diffusion based on count of ents and spatial volume of ents

void DAS_SetDiffusion( das_room_t *proom )
{
	// BRJ 7/12/05
	// This was commented out because the y component of proom->room_mins, proom->room_maxs was never
	// being computed, causing a bogus box to be sent to the partition system. The results of
	// this computation (namely the diffusion + ent_count fields of das_room_t) were never being used. 
	// Therefore, we'll avoid the enumeration altogether

	proom->diffusion = 0.0f;
	proom->ent_count = 0;

	/*
	CDasEntEnum enumerator;
	SpatialPartitionListMask_t mask = PARTITION_CLIENT_SOLID_EDICTS;	// count only solid ents in room
	int count;
	float vol;
	float volroom;
	float dfn;
	
	enumerator.Reset();
	
	SpatialPartition()->EnumerateElementsInBox(mask, proom->room_mins, proom->room_maxs, true, &enumerator );	
	
	count = enumerator.m_count;
	vol = enumerator.m_volume;

	// compute diffusion from volume
	
	// how much space around player is filled with props?

	volroom = (proom->room_maxs.x - proom->room_mins.x) * (proom->room_maxs.y - proom->room_mins.y) * (proom->room_maxs.z - proom->room_mins.z);
	volroom = fabs(volroom);

	if ( !(int)volroom )
		volroom = 1.0;

	dfn = vol / volroom;		// % of total volume occupied by props

	dfn = clamp (dfn, 0.0, 1.0);

	proom->diffusion = dfn;
	proom->ent_count = count;
	*/
}

// debug routine to display current room params

void DAS_DisplayRoomDEBUG( das_room_t *proom, bool fnew, float preset )
{
	float dx,dy,dz;
	Vector ctr;
	float count;

	if (das_debug.GetInt() == 0)
		return;

	dx = proom->length_max / 12.0;
	dy = proom->width_max / 12.0;
	dz = proom->height_max / 12.0;
	
	float refl = proom->refl_avg;
	
	count = (float)(proom->ent_count);
	float fsky = (proom->bskyabove ? 1.0 : 0.0);

	if (fnew)
		DevMsg( "NEW DSP NODE: size:(%.0f,%.0f) height:(%.0f) dif %.4f : refl %.4f : cobj: %.0f : sky %.0f \n", dx, dy, dz, proom->diffusion, refl, count, fsky);

	if (!fnew && preset < 0.0)
		return;

	if (preset >= 0.0)
	{
		if (proom == NULL)
			return;

		DevMsg( "DSP PRESET: %.0f size:(%.0f,%.0f) height:(%.0f) dif %.4f : refl %.4f : cobj: %.0f : sky %.0f \n", preset, dx, dy, dz, proom->diffusion, refl, count, fsky);
		return;
	}

	// draw box around new node location

	Vector mins;
	Vector maxs;
	mins.Init(-8,-8,-16);
	maxs.Init(8,8,0);

	CDebugOverlay::AddBoxOverlay( proom->vplayer, mins, maxs, vec3_angle, 0, 0, 255, 0, 1000.0f );

	// draw red box around node origin

	mins.Init(-0.5,-0.5,-1.0);
	maxs.Init(0.5,0.5,0);

	CDebugOverlay::AddBoxOverlay( proom->vplayer, mins, maxs, vec3_angle, 255, 0, 0, 0, 1000.0f );

	CDebugOverlay::AddTextOverlay( proom->vplayer, 0, 10, 1.0, "DSP NODE" );
}

// check newly calculated room parameters against current stored params.
// if different, return true.
// NOTE: only call when all proom params have been calculated.
// return false if this is not a good location for creating a new node

bool DAS_CheckNewRoom( das_room_t *proom )
{	
	bool bnewroom;
	float dw,dw2,dr,ds,dh;
	int cchanged = 0;
	das_room_t *proom_prev = NULL;
	Vector2D v2d;
	Vector v3d;
	float dist;

	// player can't see previous node, determine if this is a good place to lay down 
	// a new node.  Get room at last seen node for comparison

	if (g_pdas_last_node)
		proom_prev = &(g_pdas_last_node->room);

	// no previous room node saw player, go create new room node

	if (!proom_prev)
	{
		bnewroom = true;
		goto check_ret;
	}

	// if player not at least n feet from last node, return false
	
	v3d = proom->vplayer - proom_prev->vplayer;
	v2d.Init(v3d.x, v3d.y);

	dist = Vector2DLength(v2d);

	if (dist <= DAS_DIST_MIN)
		return false;

	// see if room size has changed significantly since last node

	bnewroom = true;

	dw = 0.0;
	dw2 = 0.0;
	dh = 0.0;
	dr = 0.0;

	if ( proom_prev->width_max != 0 )
		dw = (float)proom->width_max / (float)proom_prev->width_max;	// max width delta

	if ( proom_prev->length_max != 0 )
		dw2 = (float)proom->length_max / (float)proom_prev->length_max;	// max length delta

	if ( proom_prev->height_max != 0 )
		dh = (float)proom->height_max / (float)proom_prev->height_max;	// max height delta

	if ( proom_prev->refl_avg != 0.0 )
		dr = proom->refl_avg / proom_prev->refl_avg;					// reflectivity delta

	ds = fabs( proom->sky_pct - proom_prev->sky_pct);					// sky hits delta

	if (dw > 1.0) dw = 1.0 / dw;
	if (dw2 > 1.0) dw = 1.0 / dw2;
	if (dh > 1.0) dh = 1.0 / dh;
	if (dr > 1.0) dr = 1.0 / dr;

	if ( (1.0 - dw) >= DAS_WIDTH_MIN )
		cchanged++;
		
	if ( (1.0 - dw2) >= DAS_WIDTH_MIN )
		cchanged++;

//	if ( (1.0 - dh) >= DAS_WIDTH_MIN )	// don't change room based on height change
//		cchanged++;

	// new room only if at least 1 changed

	if (cchanged >= 1)
		goto check_ret;

//	if ( (1.0 - dr) >= DAS_REFL_MIN )	// don't change room based on reflectivity change
//		goto check_ret;

//	if (ds >= DAS_SKYHIT_MIN )
//		goto check_ret;

	// new room if sky above changes state

	if (proom->bskyabove != proom_prev->bskyabove)
		goto check_ret;

	// room didn't change significantly, return false

	bnewroom = false;

check_ret:
	
	if ( bnewroom )
	{
		// if low ceiling detected < 112 units, and max height is > low ceiling height by 20%, discard - no change
		// this detects player in doorway, under pipe or narrow bridge
		
		if ( proom->lowceiling && (proom->lowceiling < proom->height_max))
		{
			float h = (float)(proom->lowceiling) / (float)proom->height_max;

			if (h < 0.8)
				return false;
		}

		DAS_SetDiffusion( proom );
	}

	DAS_DisplayRoomDEBUG( proom, bnewroom, -1.0 );

	return bnewroom;
}


extern int DSP_ConstructPreset( bool bskyabove, int width, int length, int height, float fdiffusion, float freflectivity, float *psurf_refl, int inode, int cnodes );

// select new dsp_room based on size, wall materials
// (or modulate params for current dsp)
// returns new preset # for dsp_automatic

int DAS_GetRoomDSP( das_room_t *proom, int inode )
{
	
	// preset constructor
	// call dsp module with params, get dsp preset back

	bool bskyabove		= proom->bskyabove;
	int width			= proom->width_max;
	int length			= proom->length_max;
	int height			= proom->height_max;
	float fdiffusion	= proom->diffusion;
	float freflectivity = proom->refl_avg;
	float surf_refl[6];

	// fill array of surface reflectivities - for left,right,front,back,ceiling,floor

	for (int i = 0; i < 6; i++)
		surf_refl[i] = proom->refl_walls[i];	
	
	return DSP_ConstructPreset( bskyabove, width, length, height, fdiffusion, freflectivity, surf_refl, inode, DAS_CNODES );

}


// main entry point: call once per frame to update dsp_automatic
// for automatic room detection.  dsp_room must be set to DSP_AUTOMATIC to enable.
// NOTE: this routine accumulates traceline information over several frames - it
// never traces more than 3 times per call, and normally just once per call.

void DAS_CheckNewRoomDSP( )
{
	VPROF("DAS_CheckNewRoomDSP");
	das_room_t *proom = &g_das_room;
	int dsp_preset;
	bool bRoom_ready = false;

	// if listener has not been updated, do nothing

	if ((listener_origin  == vec3_origin) && 
		(listener_forward == vec3_origin) &&
		(listener_right	  == vec3_origin) &&
		(listener_up	  == vec3_origin) )
		return;

	if ( !SND_IsInGame() )
		return;

	// make sure we init nodes & vectors first time this is called

	if ( !g_bdas_init_nodes )
	{
		g_bdas_init_nodes = 1;
		DAS_InitNodes();
	}

	if ( !DSP_CheckDspAutoEnabled())
	{
		// make sure room params are reinitialized each time autoroom is selected

		g_bdas_room_init = 0;		
		return;
	}

	if ( !g_bdas_room_init )
	{
		g_bdas_room_init = 1;

		DAS_InitAutoRoom( proom );
	}

	// get time
	
	double dtime = g_pSoundServices->GetHostTime();
	
	// compare to previous time - don't check for new room until timer expires
	// ie: wait at least DAS_AUTO_WAIT seconds between preset changes

	if ( fabs(dtime - proom->last_dsp_change) < DAS_AUTO_WAIT )
		return;

	// first, update room size parameters, see if room is ready to check - if room is updated, return true right away

	// 3 traces per frame while accumulating room size info

	for (int i = 0 ; i < 3; i++)
		bRoom_ready = DAS_UpdateRoomSize( proom );

	if (!bRoom_ready)
		return;
		

	if ( !g_bdas_create_new_node )
	{
		// next, check all nodes for line of sight to player - if all checked, return true right away

		if ( !DAS_CheckNextNode( proom ) )
		{
			// check all nodes first

			return;
		}

		// find out if any previously stored nodes can see player,
		// if so, get closest node's dsp preset

		dsp_preset = DAS_GetDspPreset( proom->bskyabove );

		if (dsp_preset != -1)
		{
			// an existing node can see player - just set preset and return
				
			if (dsp_preset != dsp_room_GetInt())
			{		
				// changed preset, so update timestamp

				proom->last_dsp_change = g_pSoundServices->GetHostTime();

				if (g_pdas_last_node)
					DAS_DisplayRoomDEBUG( &(g_pdas_last_node->room), false, (float)dsp_preset );
			}

			DSP_SetDspAuto( dsp_preset );

			goto check_new_room_exit;
		} 
	}

	g_bdas_create_new_node = true;

	// no nodes can see player, need to try to create a new one

	// check for 'new' room around player

	if ( DAS_CheckNewRoom( proom ) )
	{
		// new room found - update dsp_automatic

		dsp_preset = DAS_GetRoomDSP( proom, DAS_GetNextNodeIndex() );

		DSP_SetDspAuto( dsp_preset );
				
		// changed preset, so update timestamp

		proom->last_dsp_change = g_pSoundServices->GetHostTime();

		// save room as new node

		DAS_StoreNode( proom, dsp_preset );

		goto check_new_room_exit;
	}

check_new_room_exit:

	// reset new node creation flag - start checking for visible nodes again

	g_bdas_create_new_node = false;

	// reset room checking flag - start checking room around player again

	proom->broomready = false;

	// reset node checking flag - start checking nodes around player again

	DAS_ResetNodes();

	return;
}

// remap contents of volumes[] arrary if sound originates from player, or is music, and is 100% 'mono' 
// ie: same volume in all channels

void RemapPlayerOrMusicVols(  channel_t *ch, int volumes[CCHANVOLUMES/2], bool fplayersound, bool fmusicsound, float mono )
{
	VPROF_("RemapPlayerOrMusicVols", 2, VPROF_BUDGETGROUP_OTHER_SOUND, false, BUDGETFLAG_OTHER );

	if ( !fplayersound && !fmusicsound )
		return;	// no remapping

	if ( ch->flags.bSpeaker )
		return; // don't remap speaker sounds rebroadcast on player

	// get total volume

	float vol_total = 0.0;
	int k;

	for (k = 0; k < CCHANVOLUMES/2; k++)
		vol_total += (float)volumes[k];

	if ( !g_AudioDevice->IsSurround() )
	{
		if (mono < 1.0)
			return;

		// remap 2 chan non-spatialized versions of player and music sounds
		// note: this is required to keep volumes same as 4 & 5 ch cases!
		
		float vol_dist_music[] =  {1.0, 1.0};	// FL, FR music volumes
		float vol_dist_player[] = {1.0, 1.0};	// FL, FR player volumes
		float *pvol_dist;

		pvol_dist = (fplayersound ? vol_dist_player : vol_dist_music);

		for (k = 0; k < 2; k++)
			volumes[k] = clamp((int)(vol_total * pvol_dist[k]), 0, 255);

		return;
	}

	// surround sound configuration...

	if ( fplayersound ) // && (ch->bstereowav && ch->wavtype != CHAR_DIRECTIONAL && ch->wavtype != CHAR_DISTVARIANT) )
	{
		// NOTE: player sounds also get n% overall volume boost.
		
		//float vol_dist5[]   = {0.29, 0.29, 0.09, 0.09, 0.63};	// FL, FR, RL, RR, FC - 5 channel (mono source) volume distribution		
		//float vol_dist5st[] = {0.29, 0.29, 0.09, 0.09, 0.63};	// FL, FR, RL, RR, FC - 5 channel (stereo source) volume distribution
		
		float vol_dist5[]   = {0.30, 0.30, 0.09, 0.09, 0.59};	// FL, FR, RL, RR, FC - 5 channel (mono source) volume distribution		
		float vol_dist5st[] = {0.30, 0.30, 0.09, 0.09, 0.59};	// FL, FR, RL, RR, FC - 5 channel (stereo source) volume distribution
		
		float vol_dist4[]   = {0.50, 0.50, 0.15, 0.15, 0.00};	// FL, FR, RL, RR, 0  - 4 channel (mono source) volume distribution
		float vol_dist4st[] = {0.50, 0.50, 0.15, 0.15, 0.00};	// FL, FR, RL, RR, 0  - 4 channel (stereo source)volume distribution

		float *pvol_dist;
		
		if ( ch->flags.bstereowav && (ch->wavtype == CHAR_OMNI || ch->wavtype == CHAR_SPATIALSTEREO || ch->wavtype == 0))
		{
			pvol_dist = (g_AudioDevice->IsSurroundCenter() ? vol_dist5st : vol_dist4st);	
		}
		else
		{
			pvol_dist = (g_AudioDevice->IsSurroundCenter() ? vol_dist5 : vol_dist4);
		}

		for (k = 0; k < 5; k++)
			volumes[k] = clamp((int)(vol_total * pvol_dist[k]), 0, 255);

		return;
	}

	// Special case for music in surround mode

	if ( fmusicsound )
	{
		float vol_dist5[] = {0.5, 0.5, 0.25, 0.25, 0.0};	// FL, FR, RL, RR, FC - 5 channel distribution
		float vol_dist4[] = {0.5, 0.5, 0.25, 0.25, 0.0};	// FL, FR, RL, RR, 0  - 4 channel distribution
		float *pvol_dist;

		pvol_dist = (g_AudioDevice->IsSurroundCenter() ? vol_dist5 : vol_dist4);

		for (k = 0; k < 5; k++)
			volumes[k] = clamp((int)(vol_total * pvol_dist[k]), 0, 255);

		return;
	}

	return;
}

static int s_nSoundGuid = 0;

void SND_ActivateChannel( channel_t *pChannel )
{
	Q_memset( pChannel, 0, sizeof(*pChannel) );
	g_ActiveChannels.Add( pChannel );
	pChannel->guid = ++s_nSoundGuid;
}

/*
=================
SND_Spatialize
=================
*/
void SND_Spatialize(channel_t *ch)
{
	VPROF("SND_Spatialize");

    vec_t dist;
    Vector source_vec;
	Vector source_vec_DL;
	Vector source_vec_DR;
	Vector source_doppler_left;
	Vector source_doppler_right;

	bool fdopplerwav = false;
	bool fplaydopplerwav = false;
	bool fvalidentity;
	float gain;
	float scale = 1.0;
	bool fplayersound = false;
	bool fmusicsound = false;
	float mono = 0.0;
	bool bAttenuated = true;

	ch->dspface = 1.0;				// default facing direction: always facing player
	ch->dspmix = 0;					// default mix 0% dsp_room fx
	ch->distmix = 0;				// default 100% left (near) wav

#if !defined( _X360 )
	if ( ch->sfx && 
		ch->sfx->pSource && 
		ch->sfx->pSource->GetType() == CAudioSource::AUDIO_SOURCE_VOICE )
	{
		Voice_Spatialize( ch );
	}
#endif

	if ( IsSoundSourceLocalPlayer( ch->soundsource ) && !toolframework->InToolMode() )
	{
		// sounds coming from listener actually come from a short distance directly in front of listener
		// in tool mode however, the view entity is meaningless, since we're viewing from arbitrary locations in space
		fplayersound = true;
	}

	// assume 'dry', playeverwhere sounds are 'music' or 'voiceover'

	if ( ch->flags.bdry && ch->dist_mult <= 0 )
	{
		fmusicsound = true;
		fplayersound = false;
	}

	// update channel's position in case ent that made the sound is moving.
	QAngle source_angles;
	source_angles.Init(0.0, 0.0, 0.0);
	Vector entOrigin = ch->origin;
	
	bool looping = false;

	CAudioSource *pSource = ch->sfx ? ch->sfx->pSource : NULL;
	if ( pSource )
	{
		looping = pSource->IsLooped();
	}
	
	SpatializationInfo_t si;
	si.info.Set( 
		ch->soundsource,
		ch->entchannel,
		ch->sfx ? ch->sfx->getname() : "",
		ch->origin,
		ch->direction,
		ch->master_vol,
		DIST_MULT_TO_SNDLVL( ch->dist_mult ),
		looping,
		ch->pitch,
		listener_origin,
		ch->speakerentity );

	si.type = SpatializationInfo_t::SI_INSPATIALIZATION;
	si.pOrigin = &entOrigin;
	si.pAngles = &source_angles;
	si.pflRadius = NULL;
	if ( ch->soundsource != 0 && ch->radius == 0 )
	{
		si.pflRadius = &ch->radius;
	}

	{
		VPROF_("SoundServices->GetSoundSpatializtion", 2, VPROF_BUDGETGROUP_OTHER_SOUND, false, BUDGETFLAG_OTHER );
		fvalidentity = g_pSoundServices->GetSoundSpatialization( ch->soundsource, si );	
	}

	if ( ch->flags.bUpdatePositions )
	{
		AngleVectors( source_angles, &ch->direction );
		ch->origin = entOrigin;
	}
	else
	{
		VectorAngles( ch->direction, source_angles );
	}

	if ( ch->userdata != 0 )
	{
		g_pSoundServices->GetToolSpatialization( ch->userdata, ch->guid, si );
		if ( ch->flags.bUpdatePositions )
		{
			AngleVectors( source_angles, &ch->direction );
			ch->origin = entOrigin;
		}
	}

#if 0
	// !!!UNDONE - above code assumes the ENT hasn't been removed or respawned as another ent!
	// !!!UNDONE - fix this by flagging some entities (ie: glass) as immobile.  Don't spatialize them.
	if ( !fvalidendity)
	{
		// Turn off the sound while the entity doesn't exist or is not in the PVS.
		goto ClearAllVolumes;
	}
#endif // 0

	
	fdopplerwav = ((ch->wavtype == CHAR_DOPPLER) && !fplayersound);
	if ( fdopplerwav )
	{
		VPROF_("SND_Spatialize doppler", 2, VPROF_BUDGETGROUP_OTHER_SOUND, false, BUDGETFLAG_OTHER );
		Vector vnearpoint;				// point of closest approach to listener, 
										// along sound source forward direction (doppler wavs)

		vnearpoint = ch->origin;		// default nearest sound approach point

		// calculate point of closest approach for CHAR_DOPPLER wavs, replace source_vec

		fplaydopplerwav = SND_GetClosestPoint( ch, source_angles, vnearpoint );

		// if doppler sound was 'shot' away from listener, don't play it

		if ( !fplaydopplerwav )
			goto ClearAllVolumes;

		// find location of doppler left & doppler right points

		SND_GetDopplerPoints( ch, source_angles, vnearpoint, source_doppler_left, source_doppler_right);
	
		// source_vec_DL is vector from listener to doppler left point
		// source_vec_DR is vector from listener to doppler right point

		VectorSubtract(source_doppler_left, listener_origin, source_vec_DL );
		VectorSubtract(source_doppler_right, listener_origin, source_vec_DR );

		// normalized vectors to left and right doppler locations

		dist = VectorNormalize( source_vec_DL );
		VectorNormalize( source_vec_DR );

		// don't play doppler if out of range
		// unless recording in the tool, since we may play back in range
		if ( dist > DOPPLER_RANGE_MAX && !toolframework->IsToolRecording() )
			goto ClearAllVolumes;
	}
	else
	{
		// source_vec is vector from listener to sound source

		if ( fplayersound )
		{
			// get 2d forward direction vector, ignoring pitch angle
			Vector listener_forward2d;

			ConvertListenerVectorTo2D( &listener_forward2d, &listener_right );

			// player sounds originate from 1' in front of player, 2d

			VectorMultiply(listener_forward2d, 12.0, source_vec );
		}
		else
		{
			VectorSubtract(ch->origin, listener_origin, source_vec);
		}

		// normalize source_vec and get distance from listener to source

		dist = VectorNormalize( source_vec );
	}
	
	// calculate dsp mix based on distance to listener & sound level (linear approximation)

	ch->dspmix = SND_GetDspMix( ch, dist );

	// calculate sound source facing direction for CHAR_DIRECTIONAL wavs

	if ( !fplayersound )
	{
		ch->dspface = SND_GetFacingDirection( ch, source_angles );
		
		// calculate mixing parameter for CHAR_DISTVAR wavs

		ch->distmix = SND_GetDistanceMix( ch, dist );
	}

	// for sounds with a radius, spatialize left/right/front/rear evenly within the radius

	if ( ch->radius > 0 && dist < ch->radius && !fdopplerwav )
	{
		float interval = ch->radius * 0.5;
		mono = dist - interval;
		if ( mono < 0.0 )
			mono = 0.0;
		mono /= interval;
		
		mono = 1.0 - mono;

		// mono is 0.0 -> 1.0 from radius 100% to radius 50%
	}

	// don't pan sounds with no attenuation
	if ( ch->dist_mult <= 0 && !fdopplerwav )
	{
		// sound is centered left/right/front/back
		
		mono = 1.0;
		bAttenuated = false;
	}

	if ( ch->wavtype == CHAR_OMNI )
	{
		// omni directional sound sources are mono mix, all speakers
		// ie: they only attenuate by distance, not by source direction.

		mono = 1.0;
		bAttenuated = false;
	}

	// calculate gain based on distance, atmospheric attenuation, interposed objects
	// perform compression as gain approaches 1.0

	gain = SND_GetGain( ch, fplayersound, fmusicsound, looping, dist, bAttenuated );

	// map gain through global mixer by soundtype

	// gain *= SND_GetVolFromSoundtype( ch->soundtype );
	int last_mixgroupid;

	gain *= MXR_GetVolFromMixGroup( ch->mixgroups, &last_mixgroupid );

	// if playing a word, get volume scale of word - scale gain
		
	scale = VOX_GetChanVol(ch);

	gain *= scale;

	// save spatialized volume and mixgroupid for display later

	ch->last_mixgroupid = last_mixgroupid;

	if ( fdopplerwav )
	{
		VPROF_("SND_Spatialize doppler", 2, VPROF_BUDGETGROUP_OTHER_SOUND, false, BUDGETFLAG_OTHER );
		// fill out channel volumes for both doppler sound source locations
		int volumes[CCHANVOLUMES/2];

		// left doppler location

		g_AudioDevice->SpatializeChannel( volumes,  ch->master_vol, source_vec_DL, gain, mono );
		
		// load volumes into channel as crossfade targets

		ChannelSetVolTargets( ch, volumes, IFRONT_LEFT, CCHANVOLUMES/2 );

		// right doppler location

		g_AudioDevice->SpatializeChannel( volumes, ch->master_vol, source_vec_DR, gain, mono );		
		
		// load volumes into channel as crossfade targets

		ChannelSetVolTargets( ch, volumes, IFRONT_LEFTD, CCHANVOLUMES/2 );
	}
	else
	{
		// fill out channel volumes for single sound source location
		int volumes[CCHANVOLUMES/2];

		g_AudioDevice->SpatializeChannel( volumes, ch->master_vol, source_vec, gain, mono );
		
		// Special case for stereo sounds originating from player in surround mode
		// and special case for musci: remap volumes directly to channels.
	
		RemapPlayerOrMusicVols( ch, volumes, fplayersound, fmusicsound, mono );

		// load volumes into channel as crossfade volume targets

		ChannelSetVolTargets( ch, volumes, IFRONT_LEFT, CCHANVOLUMES/2 );
	}


	// prevent left/right/front/rear/center volumes from changing too quickly & producing pops

	ChannelUpdateVolXfade( ch );

	// end of first time spatializing sound

	if ( SND_IsInGame() || toolframework->InToolMode() )
	{
		ch->flags.bfirstpass = false;
	}

	// calculate total volume for display later
	ch->last_vol = gain * (ch->master_vol/255.0);

	return;

ClearAllVolumes:

	// Clear all volumes and return. 
	// This shuts the sound off permanently.

	ChannelClearVolumes( ch );

	// end of first time spatializing sound

	ch->flags.bfirstpass = false;
}           

ConVar snd_defer_trace("snd_defer_trace","1");
void SND_SpatializeFirstFrameNoTrace( channel_t *pChannel)
{
	if ( snd_defer_trace.GetBool() )
	{
		// set up tracing state to be non-obstructed
		pChannel->flags.bfirstpass = false;
		pChannel->flags.bTraced = true;
		pChannel->ob_gain = 1.0;
		pChannel->ob_gain_inc = 1.0;
		pChannel->ob_gain_target = 1.0;
		// now spatialize without tracing
		SND_Spatialize(pChannel);
		// now reset tracing state to firstpass so the trace gets done on next spatialize
		pChannel->ob_gain = 0.0;
		pChannel->ob_gain_inc = 0.0;
		pChannel->ob_gain_target = 0.0;
		pChannel->flags.bfirstpass = true;
		pChannel->flags.bTraced = false;
	}
	else
	{
		pChannel->ob_gain = 0.0;
		pChannel->ob_gain_inc = 0.0;
		pChannel->ob_gain_target = 0.0;
		pChannel->flags.bfirstpass = true;
		pChannel->flags.bTraced = false;
		SND_Spatialize(pChannel);
	}
}


// search through all channels for a channel that matches this
// soundsource, entchannel and sfx, and perform alteration on channel
// as indicated by 'flags' parameter. If shut down request and
// sfx contains a sentence name, shut off the sentence.
// returns TRUE if sound was altered,
// returns FALSE if sound was not found (sound is not playing)

int S_AlterChannel( int soundsource, int entchannel, CSfxTable *sfx, int vol, int pitch, int flags )
{
	THREAD_LOCK_SOUND();
	int ch_idx;

	const char *name = sfx->getname();
	if ( name && TestSoundChar( name, CHAR_SENTENCE ) )
	{
		// This is a sentence name.
		// For sentences: assume that the entity is only playing one sentence
		// at a time, so we can just shut off
		// any channel that has ch->isentence >= 0 and matches the
		// soundsource.

		CChannelList list;
		g_ActiveChannels.GetActiveChannels( list );
		for ( int i = 0; i < list.Count(); i++ )
		{
			ch_idx = list.GetChannelIndex(i);
			if (channels[ch_idx].soundsource == soundsource
				&& channels[ch_idx].entchannel == entchannel
				&& channels[ch_idx].sfx != NULL )
			{

				if (flags & SND_CHANGE_PITCH)
					channels[ch_idx].basePitch = pitch;
				
				if (flags & SND_CHANGE_VOL)
					channels[ch_idx].master_vol = vol;
				
				if (flags & SND_STOP)
				{
					S_FreeChannel(&channels[ch_idx]);
				}
			
				return TRUE;
			}
		}
		// channel not found
		return FALSE;

	}

	// regular sound or streaming sound
	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );

	bool bSuccess = false;

	for ( int i = 0; i < list.Count(); i++ )
	{
		ch_idx = list.GetChannelIndex(i);
		if ( channels[ch_idx].soundsource == soundsource && 
			 ( ( flags & SND_IGNORE_NAME ) || 
			   ( channels[ch_idx].entchannel == entchannel && channels[ch_idx].sfx == sfx ) ) )
		{
			if (flags & SND_CHANGE_PITCH)
				channels[ch_idx].basePitch = pitch;
			
			if (flags & SND_CHANGE_VOL)
				channels[ch_idx].master_vol = vol;
			
			if (flags & SND_STOP)
			{
				S_FreeChannel(&channels[ch_idx]);
			}
		
			if ( ( flags & SND_IGNORE_NAME ) == 0 )
				return TRUE;
			else
				bSuccess = true;
		}
   }

	return ( bSuccess ) ? ( TRUE ) : ( FALSE );
}

// set channel flags during initialization based on 
// source name

void S_SetChannelWavtype( channel_t *target_chan, CSfxTable *pSfx )
{	
	// if 1st or 2nd character of name is CHAR_DRYMIX, sound should be mixed dry with no dsp (ie: music)

	if ( TestSoundChar(pSfx->getname(), CHAR_DRYMIX) )
		target_chan->flags.bdry = true;
	else
		target_chan->flags.bdry = false;

	if ( TestSoundChar(pSfx->getname(), CHAR_FAST_PITCH) )
		target_chan->flags.bfast_pitch = true;
	else
		target_chan->flags.bfast_pitch = false;

	// get sound spatialization encoding
	
	target_chan->wavtype = 0;

	if ( TestSoundChar( pSfx->getname(), CHAR_DOPPLER ))
		target_chan->wavtype = CHAR_DOPPLER;
	
	if ( TestSoundChar( pSfx->getname(), CHAR_DIRECTIONAL ))
		target_chan->wavtype = CHAR_DIRECTIONAL;

	if ( TestSoundChar( pSfx->getname(), CHAR_DISTVARIANT ))
		target_chan->wavtype = CHAR_DISTVARIANT;

	if ( TestSoundChar( pSfx->getname(), CHAR_OMNI ))
		target_chan->wavtype = CHAR_OMNI;

	if ( TestSoundChar( pSfx->getname(), CHAR_SPATIALSTEREO ))
		target_chan->wavtype = CHAR_SPATIALSTEREO;
}


// Sets bstereowav flag in channel if source is true stere wav
// sets default wavtype for stereo wavs to CHAR_DISTVARIANT - 
// ie: sound varies with distance (left is close, right is far)
// Must be called after S_SetChannelWavtype

void S_SetChannelStereo( channel_t *target_chan, CAudioSource *pSource )
{
	if ( !pSource )
	{
		target_chan->flags.bstereowav = false;
		return;
	}
	
	// returns true only if source data is a stereo wav file. 
	// ie: mp3, voice, sentence are all excluded.

	target_chan->flags.bstereowav = pSource->IsStereoWav();

	// Default stereo wavtype:

	// just player standard stereo wavs on player entity - no override.

	if ( IsSoundSourceLocalPlayer( target_chan->soundsource ) )
		return;
	
	// default wavtype for stereo wavs is OMNI - except for drymix or sounds with 0 attenuation

	if ( target_chan->flags.bstereowav && !target_chan->wavtype && !target_chan->flags.bdry && target_chan->dist_mult )
		// target_chan->wavtype = CHAR_DISTVARIANT;
		target_chan->wavtype = CHAR_OMNI;
}

// =======================================================================
// Channel volume management routines:

// channel volumes crossfade between values over time
// to prevent pops due to rapid spatialization changes
// =======================================================================

// return true if all volumes and target volumes for channel are less/equal to 'vol'

bool BChannelLowVolume( channel_t *pch, int vol_min )
{
	int max = -1;
	int max_target = -1;
	int vol;
	int vol_target;

	for (int i = 0; i < CCHANVOLUMES; i++)
	{
		vol = (int)(pch->fvolume[i]);
		vol_target = (int)(pch->fvolume_target[i]);

		if (vol > max)
			max = vol;

		if (vol_target > max_target)
			max_target = vol_target;
	}
		
	return (max <= vol_min && max_target <= vol_min);
}

// Get the loudest actual volume for a channel (not counting targets).
float ChannelLoudestCurVolume( const channel_t * RESTRICT pch )
{
	float loudest = pch->fvolume[0];
	for (int i = 1; i < CCHANVOLUMES; i++)
	{
		loudest = fpmax(loudest, pch->fvolume[i]);
	}
	return loudest;
}

// clear all volumes, targets, crossfade increments

void ChannelClearVolumes( channel_t *pch )
{
	for (int i = 0; i < CCHANVOLUMES; i++)
	{
		pch->fvolume[i] = 0.0;
		pch->fvolume_target[i] = 0.0;
		pch->fvolume_inc[i] = 0.0;
	}
}

// return current volume as integer

int ChannelGetVol( channel_t *pch, int ivol )
{
	Assert(ivol < CCHANVOLUMES);
	return (int)(pch->fvolume[ivol]);	
}

// return maximum current output volume 

int ChannelGetMaxVol( channel_t *pch )
{
	float max = 0.0;
	
	for (int i = 0; i < CCHANVOLUMES; i++)
	{
		if (pch->fvolume[i] > max)
			max = pch->fvolume[i];
	}

	return (int)max;
}

// set current volume (clears crossfading - instantaneous value change)

void ChannelSetVol( channel_t *pch, int ivol, int vol )
{
	Assert(ivol < CCHANVOLUMES);
	
	pch->fvolume[ivol] = (float)(clamp(vol, 0, 255));	

	pch->fvolume_target[ivol] = pch->fvolume[ivol];
	pch->fvolume_inc[ivol] = 0.0;
}

// copy current channel volumes into target array, starting at ivol, copying cvol entries

void ChannelCopyVolumes( channel_t *pch, int *pvolume_dest, int ivol_start, int cvol )
{
	Assert (ivol_start < CCHANVOLUMES);
	Assert (ivol_start + cvol <= CCHANVOLUMES);

	for (int i = 0; i < cvol; i++)
		pvolume_dest[i] = (int)(pch->fvolume[i + ivol_start]);
}

// volume has hit target, shut off crossfading increment

inline void ChannelStopVolXfade( channel_t *pch, int ivol )
{
	pch->fvolume[ivol] = pch->fvolume_target[ivol];
	pch->fvolume_inc[ivol] = 0.0;
}

#define	VOL_XFADE_TIME	0.070	// channel volume crossfade time in seconds

#define VOL_INCR_MAX	20.0	// never change volume by more than +/-N units per frame

// set volume target and volume increment (for crossfade) for channel & speaker

void ChannelSetVolTarget( channel_t *pch, int ivol, int volume_target )
{
	float frametime = g_pSoundServices->GetHostFrametime();
	float speed;
	float vol_target = (float)(clamp(volume_target, 0, 255));
	float vol_current;

	Assert(ivol < CCHANVOLUMES);
	
	// set volume target

	pch->fvolume_target[ivol] = vol_target;

	// current volume

	vol_current = pch->fvolume[ivol];

	// if first time spatializing, set target = volume with no crossfade
	// if current & target volumes are close - don't bother crossfading

	if ( pch->flags.bfirstpass || (fabs(vol_target - vol_current) < 5.0)) 
	{
		// set current volume = target, no increment

		ChannelStopVolXfade( pch, ivol);
		return;
	}

	// get crossfade increment 'speed' (volume change per frame)

	speed = ( frametime / VOL_XFADE_TIME ) * (vol_target - vol_current);

	// make sure we never increment by more than +/- VOL_INCR_MAX volume units per frame
	
	speed = clamp(speed, (float) -VOL_INCR_MAX, (float) VOL_INCR_MAX);

	pch->fvolume_inc[ivol] = speed;	
}

// set volume targets, using array pvolume as source volumes.
// set into channel volumes starting at ivol_offset index
// set cvol volumes

void ChannelSetVolTargets( channel_t *pch, int *pvolumes, int ivol_offset, int cvol )
{
	int volume_target;
	
	Assert(ivol_offset + cvol <= CCHANVOLUMES);

	for (int i = 0; i < cvol; i++)
	{
		volume_target = pvolumes[i];

		ChannelSetVolTarget( pch, ivol_offset + i, volume_target );
	}
}


// Call once per frame, per channel:
// update all volume crossfades, from fvolume -> fvolume_target
// if current volume reaches target, set increment to 0

void ChannelUpdateVolXfade( channel_t *pch )
{
	float fincr;

	for (int i = 0; i < CCHANVOLUMES; i++)
	{
		fincr = pch->fvolume_inc[i];

		if (fincr != 0.0)
		{
			pch->fvolume[i] += fincr;

			// test for hit target

			if (fincr > 0.0)
			{
				if (pch->fvolume[i] >= pch->fvolume_target[i])
					ChannelStopVolXfade( pch, i );
			}
			else
			{
				if (pch->fvolume[i] <= pch->fvolume_target[i])
					ChannelStopVolXfade( pch, i );
			}
		}
	}
}

// =======================================================================
// S_StartDynamicSound
// =======================================================================
// Start a sound effect for the given entity on the given channel (ie; voice, weapon etc).  
// Try to grab a channel out of the 8 dynamic spots available.
// Currently used for looping sounds, streaming sounds, sentences, and regular entity sounds.
// NOTE: volume is 0.0 - 1.0 and attenuation is 0.0 - 1.0 when passed in.
// Pitch changes playback pitch of wave by % above or below 100.  Ignored if pitch == 100

// NOTE: it's not a good idea to play looping sounds through StartDynamicSound, because
// if the looping sound starts out of range, or is bumped from the buffer by another sound
// it will never be restarted.  Use StartStaticSound (pass CHAN_STATIC to EMIT_SOUND or
// SV_StartSound.

int S_StartDynamicSound( StartSoundParams_t& params )
{
	Assert( params.staticsound == false );

	channel_t *target_chan;
	int		vol;

	if ( !g_AudioDevice || !g_AudioDevice->IsActive())
		return 0;

	if (!params.pSfx)
		return 0;

	// For debugging to see the actual name of the sound...
	char sndname[ MAX_OSPATH ];
	Q_strncpy( sndname, params.pSfx->getname(), sizeof( sndname ) );

	// Msg("Start sound %s\n", pSfx->getname() );

	// override the entchannel to CHAN_STREAM if this is a 
	// non-voice stream sound.
	if ( TestSoundChar(sndname, CHAR_STREAM ) && params.entchannel != CHAN_VOICE && params.entchannel != CHAN_VOICE2 )
		params.entchannel = CHAN_STREAM;
	
	vol = params.fvol*255;
	
	if (vol > 255)
	{
		DevMsg("S_StartDynamicSound: %s volume > 255", sndname );
		vol = 255;
	}

	THREAD_LOCK_SOUND();

	if ( params.flags & (SND_STOP|SND_CHANGE_VOL|SND_CHANGE_PITCH) )
	{
		if ( S_AlterChannel( params.soundsource, params.entchannel, params.pSfx, vol, params.pitch, params.flags) )
			return 0;
		if ( params.flags & SND_STOP )
			return 0;
		// fall through - if we're not trying to stop the sound, 
		// and we didn't find it (it's not playing), go ahead and start it up
	}

	if (params.pitch == 0)
	{
		DevMsg ("Warning: S_StartDynamicSound (%s) Ignored, called with pitch 0\n", sndname );
		return 0;
	}

	// pick a channel to play on
	target_chan = SND_PickDynamicChannel(params.soundsource, params.entchannel, params.origin, params.pSfx, params.delay, (params.flags & SND_DO_NOT_OVERWRITE_EXISTING_ON_CHANNEL) != 0 );
	if ( !target_chan )
		return 0;

	int channelIndex = (int)( target_chan - channels );
	g_AudioDevice->ChannelReset( params.soundsource, channelIndex, target_chan->dist_mult );

#ifdef DEBUG_CHANNELS
	{
		char szTmp[128];
		Q_snprintf(szTmp, sizeof( szTmp ), "Sound %s playing on Dynamic game channel %d\n", sndname, IWavstreamOfCh(target_chan));
		Plat_DebugString(szTmp);
	}
#endif
	
	bool bIsSentence = TestSoundChar( sndname, CHAR_SENTENCE );

	SND_ActivateChannel( target_chan );
	ChannelClearVolumes( target_chan );

	target_chan->userdata = params.userdata;
	target_chan->initialStreamPosition = params.initialStreamPosition;

	VectorCopy(params.origin, target_chan->origin);
	VectorCopy(params.direction, target_chan->direction);
	
	// never update positions if source entity is 0
	target_chan->flags.bUpdatePositions = params.bUpdatePositions && (params.soundsource == 0 ? 0 : 1);

	// reference_dist / (reference_power_level / actual_power_level)
	target_chan->flags.m_bCompatibilityAttenuation = SNDLEVEL_IS_COMPATIBILITY_MODE( params.soundlevel );
	if ( target_chan->flags.m_bCompatibilityAttenuation )
	{
		// Translate soundlevel from its 'encoded' value to a real soundlevel that we can use in the sound system.
		params.soundlevel = SNDLEVEL_FROM_COMPATIBILITY_MODE( params.soundlevel );
	}

	target_chan->dist_mult = SNDLVL_TO_DIST_MULT( params.soundlevel );

	S_SetChannelWavtype( target_chan, params.pSfx );

	target_chan->master_vol = vol;
	target_chan->soundsource = params.soundsource;
	target_chan->entchannel = params.entchannel;
	target_chan->basePitch = params.pitch;
	target_chan->flags.isSentence = false;
	target_chan->radius = 0;
	target_chan->sfx = params.pSfx;
	target_chan->special_dsp = params.specialdsp;
	target_chan->flags.fromserver = params.fromserver;
	target_chan->flags.bSpeaker = (params.flags & SND_SPEAKER) ? 1 : 0;
	target_chan->speakerentity = params.speakerentity;

	target_chan->flags.m_bShouldPause = (params.flags & SND_SHOULDPAUSE) ? 1 : 0;

	// initialize dsp room mixing params
	target_chan->dsp_mix_min = -1;
	target_chan->dsp_mix_max = -1;

	CAudioSource *pSource = NULL;

	if ( bIsSentence )
	{
		// this is a sentence
		// link all words and load the first word

		// NOTE: sentence names stored in the cache lookup are
		// prepended with a '!'.  Sentence names stored in the
		// sentence file do not have a leading '!'. 
		VOX_LoadSound( target_chan, PSkipSoundChars( sndname ) );
	}
	else
	{
		// regular or streamed sound fx
		pSource = S_LoadSound( params.pSfx, target_chan );
		if ( pSource && !IsValidSampleRate( pSource->SampleRate() ) )
		{
			Warning( "*** Invalid sample rate (%d) for sound '%s'.\n", pSource->SampleRate(), sndname );
		}

		if ( !pSource && !params.pSfx->m_bIsLateLoad )
		{
			Warning( "Failed to load sound \"%s\", file probably missing from disk/repository\n", sndname );
		}

	}

	if (!target_chan->pMixer)
	{
		// couldn't load the sound's data, or sentence has 0 words (this is not an error)
		S_FreeChannel( target_chan );
		return 0;
	}

	int nSndShowStart = snd_showstart.GetInt();

	// TODO: Support looping sounds through speakers.
	// If the sound is from a speaker, and it's looping, ignore it.
	if ( target_chan->flags.bSpeaker )
	{
		if ( params.pSfx->pSource && params.pSfx->pSource->IsLooped() )
		{
			if (nSndShowStart > 0 && nSndShowStart < 7 && nSndShowStart != 4)
			{
				DevMsg("DynamicSound : Speaker ignored looping sound: %s\n", sndname );
			}

			S_FreeChannel( target_chan );
			return 0;
		}
	}

	S_SetChannelStereo( target_chan, pSource );

	if (nSndShowStart == 5)	
	{
		snd_showstart.SetValue(6);		// debug: show gain for next spatialize only
		nSndShowStart = 6;
	}

	// get sound type before we spatialize
	MXR_GetMixGroupFromSoundsource( target_chan, params.soundsource, params.soundlevel );

	// skip the trace on the first spatialization.  This channel may be stolen
	// by another sound played this frame.  Defer the trace to the mix loop
	SND_SpatializeFirstFrameNoTrace(target_chan);

	if (nSndShowStart > 0 && nSndShowStart < 7 && nSndShowStart != 4)
	{
		channel_t *pTargetChan = target_chan;

		DevMsg( "DynamicSound %s : src %d : channel %d : %d dB : vol %.2f : time %.3f\n", sndname, params.soundsource, params.entchannel, params.soundlevel, params.fvol, g_pSoundServices->GetHostTime() );
		if (nSndShowStart == 2 || nSndShowStart == 5)
			DevMsg( "\t dspmix %1.2f : distmix %1.2f : dspface %1.2f : lvol %1.2f : cvol %1.2f : rvol %1.2f : rlvol %1.2f : rrvol %1.2f\n", 
				pTargetChan->dspmix, pTargetChan->distmix, pTargetChan->dspface,
				pTargetChan->fvolume[IFRONT_LEFT], pTargetChan->fvolume[IFRONT_CENTER], pTargetChan->fvolume[IFRONT_RIGHT], pTargetChan->fvolume[IREAR_LEFT], pTargetChan->fvolume[IREAR_RIGHT] );
		if (nSndShowStart == 3)
			DevMsg( "\t x: %4f y: %4f z: %4f\n", pTargetChan->origin.x, pTargetChan->origin.y, pTargetChan->origin.z );

		if ( snd_visualize.GetInt() )
		{
			CDebugOverlay::AddTextOverlay( pTargetChan->origin, 2.0f, sndname );
		}
	}

	// If a client can't hear a sound when they FIRST receive the StartSound message,
	// the client will never be able to hear that sound. This is so that out of 
	// range sounds don't fill the playback buffer.  For streaming sounds, we bypass this optimization.
	
	if ( BChannelLowVolume( target_chan, 0 ) && !toolframework->IsToolRecording() )
	{
		// Looping sounds don't use this optimization because they should stick around until they're killed.
		// Also bypass for speech (GetSentence)
		if ( !params.pSfx->pSource || (!params.pSfx->pSource->IsLooped() && !params.pSfx->pSource->GetSentence()) )
		{
			// if this is long sound, play the whole thing.
			if (!SND_IsLongWave( target_chan ))
			{
				// DevMsg("S_StartDynamicSound: spatialized to 0 vol & ignored %s", sndname);
				S_FreeChannel( target_chan );
				return 0;		// not audible at all
			}
		}
	}

	// Init client entity mouth movement vars
	target_chan->flags.m_bIgnorePhonemes = ( params.flags & SND_IGNORE_PHONEMES ) != 0;
	SND_InitMouth(target_chan);

	if ( IsX360() && params.delay < 0 )
	{
		params.delay = 0;
		target_chan->flags.delayed_start = true;
	}

	// Pre-startup delay.  Compute # of samples over which to mix in zeros from data source before
	//  actually reading first set of samples
	if ( params.delay != 0.0f )
	{
		Assert( target_chan->sfx );
		Assert( target_chan->sfx->pSource );

		// delay count is computed at the sampling rate of the source because the output rate will 
		// match the source rate when the sound is mixed
		float rate = target_chan->sfx->pSource->SampleRate();
		int delaySamples = (int)( params.delay * rate );

		if ( params.delay > 0 )
		{
			target_chan->pMixer->SetStartupDelaySamples( delaySamples );
			target_chan->flags.delayed_start = true;
		}
		else
		{
			int skipSamples = -delaySamples;
			int totalSamples = target_chan->sfx->pSource->SampleCount();
			if ( target_chan->sfx->pSource->IsLooped() )
			{
				skipSamples = skipSamples % totalSamples;
			}
			if ( skipSamples >= totalSamples )
			{
				S_FreeChannel( target_chan );
				return 0;
			}
			target_chan->pitch = target_chan->basePitch * 0.01f;
			target_chan->pMixer->SkipSamples( target_chan, skipSamples, rate, 0 );
			target_chan->ob_gain_target		= 1.0f;
			target_chan->ob_gain			= 1.0f;
			target_chan->ob_gain_inc		= 0.0;
			target_chan->flags.bfirstpass	= false;
			target_chan->flags.delayed_start = true;
		}
	}

	g_pSoundServices->OnSoundStarted( target_chan->guid, params, sndname );
	return target_chan->guid;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : CSfxTable
//-----------------------------------------------------------------------------
CSfxTable *S_DummySfx( const char *name )
{
	dummySfx.setname( name );
	return &dummySfx;
}

/*
=================
S_StartStaticSound
=================
Start playback of a sound, loaded into the static portion of the channel array.
Currently, this should be used for looping ambient sounds, looping sounds
that should not be interrupted until complete, non-creature sentences,
and one-shot ambient streaming sounds.  Can also play 'regular' sounds one-shot,
in case designers want to trigger regular game sounds.
Pitch changes playback pitch of wave by % above or below 100.  Ignored if pitch == 100

  NOTE: volume is 0.0 - 1.0 and attenuation is 0.0 - 1.0 when passed in.
*/

int S_StartStaticSound( StartSoundParams_t& params )
{
	Assert( params.staticsound == true );

	channel_t *ch;
	CAudioSource *pSource = NULL;

	if ( !g_AudioDevice->IsActive() )
		return 0;

	if ( !params.pSfx )
		return 0;

	// For debugging to see the actual name of the sound...
	char sndname[ MAX_OSPATH ];
	Q_strncpy( sndname, params.pSfx->getname(), sizeof( sndname ) );
//	Msg("Start static sound %s\n", pSfx->getname() );

	int vol = params.fvol * 255;
	if ( vol > 255 )
	{
		DevMsg( "S_StartStaticSound: %s volume > 255", sndname );
		vol = 255;
	}

	int nSndShowStart = snd_showstart.GetInt();

	if ((params.flags & SND_STOP) && nSndShowStart > 0)
		DevMsg("S_StartStaticSound: %s Stopped.\n", sndname);

	if ((params.flags & SND_STOP) || (params.flags & SND_CHANGE_VOL) || (params.flags & SND_CHANGE_PITCH))
	{
		if (S_AlterChannel(params.soundsource, params.entchannel, params.pSfx, vol, params.pitch, params.flags) || (params.flags & SND_STOP))
			return 0;
	}
	
	if ( params.pitch == 0 )
	{
		DevMsg( "Warning: S_StartStaticSound Ignored, called with pitch 0\n");
		return 0;
	}
	
	// First, make sure the sound source entity is even in the PVS.
	float flSoundRadius = 0.0f;	
	
	bool looping = false;

	/*
	CAudioSource *pSource = pSfx ? pSfx->pSource : NULL;
	if ( pSource )
	{
		looping = pSource->IsLooped();
	}
	*/

	SpatializationInfo_t si;
	si.info.Set( 
		params.soundsource,
		params.entchannel,
		params.pSfx ? sndname : "",
		params.origin,
		params.direction,
		vol,
		params.soundlevel,
		looping,
		params.pitch,
		listener_origin,
		params.speakerentity );

	si.type = SpatializationInfo_t::SI_INCREATION;
	
	si.pOrigin = NULL;
	si.pAngles = NULL;
	si.pflRadius = &flSoundRadius;

	g_pSoundServices->GetSoundSpatialization( params.soundsource, si );

	// pick a channel to play on from the static area
	THREAD_LOCK_SOUND();

	ch = SND_PickStaticChannel(params.soundsource, params.pSfx); // Autolooping sounds are always fixed origin(?)
	if ( !ch )
		return 0;

	SND_ActivateChannel( ch );
	ChannelClearVolumes( ch );

	ch->userdata = params.userdata;
	ch->initialStreamPosition = params.initialStreamPosition;

	if ( ch->userdata != 0 )
	{
		g_pSoundServices->GetToolSpatialization( ch->userdata, ch->guid, si );
	}

	int channelIndex = ch - channels;
	g_AudioDevice->ChannelReset( params.soundsource, channelIndex, ch->dist_mult );

#ifdef DEBUG_CHANNELS
	{
		char szTmp[128];
		Q_snprintf(szTmp, sizeof( szTmp ), "Sound %s playing on Static game channel %d\n", sfxin->name, IWavstreamOfCh(ch));
		Plat_DebugString(szTmp);
	}
#endif
	
	if ( TestSoundChar(sndname, CHAR_SENTENCE) )
	{
		// this is a sentence. link words to play in sequence.

		// NOTE: sentence names stored in the cache lookup are
		// prepended with a '!'.  Sentence names stored in the
		// sentence file do not have a leading '!'. 

		// link all words and load the first word
		VOX_LoadSound( ch, PSkipSoundChars(sndname) );
	}
	else
	{
		// load regular or stream sound
		pSource = S_LoadSound( params.pSfx, ch );
		if ( pSource && !IsValidSampleRate( pSource->SampleRate() ) )
		{
			Warning( "*** Invalid sample rate (%d) for sound '%s'.\n", pSource->SampleRate(), sndname );
		}

		if ( !pSource && !params.pSfx->m_bIsLateLoad )
		{
			Warning( "Failed to load sound \"%s\", file probably missing from disk/repository\n", sndname );
		}

		ch->sfx = params.pSfx;
		ch->flags.isSentence = false;
	}

	if ( !ch->pMixer )
	{
		// couldn't load sounds' data, or sentence has 0 words (not an error)
		S_FreeChannel( ch );
		return 0;
	}

	VectorCopy (params.origin, ch->origin);
	VectorCopy (params.direction, ch->direction);

	// never update positions if source entity is 0
	ch->flags.bUpdatePositions = params.bUpdatePositions && (params.soundsource == 0 ? 0 : 1);

	ch->master_vol = vol;

	ch->flags.m_bCompatibilityAttenuation = SNDLEVEL_IS_COMPATIBILITY_MODE( params.soundlevel );
	if ( ch->flags.m_bCompatibilityAttenuation )
	{
		// Translate soundlevel from its 'encoded' value to a real soundlevel that we can use in the sound system.
		params.soundlevel = SNDLEVEL_FROM_COMPATIBILITY_MODE( params.soundlevel );
	}

	ch->dist_mult = SNDLVL_TO_DIST_MULT( params.soundlevel );
	
	S_SetChannelWavtype( ch, params.pSfx );

	ch->basePitch = params.pitch;
	ch->soundsource = params.soundsource;
	ch->entchannel = params.entchannel;
	ch->special_dsp = params.specialdsp;
	ch->flags.fromserver = params.fromserver;
	ch->flags.bSpeaker = (params.flags & SND_SPEAKER) ? 1 : 0;
	ch->speakerentity = params.speakerentity;

	ch->flags.m_bShouldPause = (params.flags & SND_SHOULDPAUSE) ? 1 : 0;

	// TODO: Support looping sounds through speakers.
	// If the sound is from a speaker, and it's looping, ignore it.
	if ( ch->flags.bSpeaker )
	{
		if ( params.pSfx->pSource && params.pSfx->pSource->IsLooped() )
		{
			if (nSndShowStart > 0 && nSndShowStart < 7 && nSndShowStart != 4)
			{
				DevMsg("StaticSound : Speaker ignored looping sound: %s\n", sndname);
			}

			S_FreeChannel( ch );
			return 0;
		}
	}

	// set the default radius
	ch->radius = flSoundRadius;

	S_SetChannelStereo( ch, pSource );

	// initialize dsp room mixing params
	ch->dsp_mix_min = -1;
	ch->dsp_mix_max = -1;

	if (nSndShowStart == 5)
	{
		snd_showstart.SetValue(6);		// display gain once only
		nSndShowStart = 6;
	}

	// get sound type before we spatialize

	MXR_GetMixGroupFromSoundsource( ch, params.soundsource, params.soundlevel );

	// skip the trace on the first spatialization.  This channel may be stolen
	// by another sound played this frame.  Defer the trace to the mix loop
	SND_SpatializeFirstFrameNoTrace(ch);

	// Init client entity mouth movement vars
	ch->flags.m_bIgnorePhonemes = ( params.flags & SND_IGNORE_PHONEMES ) != 0;
	SND_InitMouth( ch );

	if ( IsX360() && params.delay < 0 )
	{
		// X360TEMP: Can't support yet, but going to.
		params.delay = 0;
	}

	// Pre-startup delay.  Compute # of samples over which to mix in zeros from data source before
	// actually reading first set of samples
	if ( params.delay != 0.0f )
	{
		Assert( ch->sfx );
		Assert( ch->sfx->pSource );
		
		float rate = ch->sfx->pSource->SampleRate();

		int delaySamples = (int)( params.delay * rate * params.pitch * 0.01f );

		ch->pMixer->SetStartupDelaySamples( delaySamples );

		if ( params.delay > 0 )
		{
			ch->pMixer->SetStartupDelaySamples( delaySamples );
			ch->flags.delayed_start = true;
		}
		else
		{
			int skipSamples = -delaySamples;
			int totalSamples = ch->sfx->pSource->SampleCount();

			if ( ch->sfx->pSource->IsLooped() )
			{
				skipSamples = skipSamples % totalSamples;
			}

			if ( skipSamples >= totalSamples )
			{
				S_FreeChannel( ch );
				return 0;
			}

			ch->pitch = ch->basePitch * 0.01f;
			ch->pMixer->SkipSamples( ch, skipSamples, rate, 0 );
			ch->ob_gain_target	= 1.0f;
			ch->ob_gain			= 1.0f;
			ch->ob_gain_inc		= 0.0f;
			ch->flags.bfirstpass = false;
		}
	}

	if ( S_IsMusic( ch ) )
	{
		// See if we have "music" of same name playing from "world" which means we save/restored this sound already.  If so,
		//  kill the new version and update the soundsource
		CChannelList list;
		g_ActiveChannels.GetActiveChannels( list );
		for ( int i = 0; i < list.Count(); i++ )
		{
			channel_t *pChannel = list.GetChannel(i);
			// Don't mess with the channel we just created, of course
			if ( ch == pChannel )
				continue;
			if ( ch->sfx != pChannel->sfx )
				continue;
			if ( pChannel->soundsource != SOUND_FROM_WORLD )
				continue;
			if ( !S_IsMusic( pChannel ) )
				continue;

			DevMsg( 1, "Hooking duplicate restored song track %s\n", sndname );

			// the new channel will have an updated soundsource and probably
			// has an updated pitch or volume since we are receiving this sound message
			// after the sound has started playing (usually a volume change)
			// copy that data out of the source
			pChannel->soundsource = ch->soundsource;
			pChannel->master_vol = ch->master_vol;
			pChannel->basePitch = ch->basePitch;
			pChannel->pitch = ch->pitch;
			S_FreeChannel( ch );
			
			return 0;
		}
	}

	g_pSoundServices->OnSoundStarted( ch->guid, params, sndname );

	if (nSndShowStart > 0 && nSndShowStart < 7 && nSndShowStart != 4)
	{
		DevMsg( "StaticSound %s : src %d : channel %d : %d dB : vol %.2f : radius %.0f : time %.3f\n", sndname, params.soundsource, params.entchannel, params.soundlevel, params.fvol, flSoundRadius, g_pSoundServices->GetHostTime() );
		if (nSndShowStart == 2 || nSndShowStart == 5)
			DevMsg( "\t dspmix %1.2f : distmix %1.2f : dspface %1.2f : lvol %1.2f : cvol %1.2f : rvol %1.2f : rlvol %1.2f : rrvol %1.2f\n", 
				ch->dspmix, ch->distmix, ch->dspface, 
				ch->fvolume[IFRONT_LEFT], ch->fvolume[IFRONT_CENTER], ch->fvolume[IFRONT_RIGHT], ch->fvolume[IREAR_LEFT], ch->fvolume[IREAR_RIGHT] );
		if (nSndShowStart == 3)
			DevMsg( "\t x: %4f y: %4f z: %4f\n", ch->origin.x, ch->origin.y, ch->origin.z );
	}

	return ch->guid;
}

#ifdef STAGING_ONLY
static ConVar snd_filter( "snd_filter", "", FCVAR_CHEAT );
#endif // STAGING_ONLY

int S_StartSound( StartSoundParams_t& params )
{

	if( ! params.pSfx )
	{
		return 0;
	}

#ifdef STAGING_ONLY
	if ( snd_filter.GetString()[ 0 ] && !Q_stristr( params.pSfx->getname(), snd_filter.GetString() ) )
	{
		return 0;
	}
#endif // STAGING_ONLY

	if ( IsX360() && params.delay < 0 && !params.initialStreamPosition && params.pSfx )
	{
		// calculate an initial stream position from the expected sample position
		float rate = params.pSfx->pSource->SampleRate();
		int samplePosition = (int)( -params.delay * rate * params.pitch * 0.01f );
		params.initialStreamPosition = params.pSfx->pSource->SampleToStreamPosition( samplePosition );
	}

	if ( params.staticsound )
	{
		VPROF_( "StartStaticSound", 0, VPROF_BUDGETGROUP_OTHER_SOUND, false, BUDGETFLAG_OTHER );	
		return S_StartStaticSound( params );
	}
	else
	{
		VPROF_( "StartDynamicSound", 0, VPROF_BUDGETGROUP_OTHER_SOUND, false, BUDGETFLAG_OTHER );
		return S_StartDynamicSound( params );
	}
}

// Restart all the sounds on the specified channel
inline bool IsChannelLooped( int iChannel )
{
	return (channels[iChannel].sfx &&
			channels[iChannel].sfx->pSource && 
			channels[iChannel].sfx->pSource->IsLooped() );
}

int S_GetCurrentStaticSounds( SoundInfo_t *pResult, int nSizeResult, int entchannel )
{
	int nSpaceRemaining = nSizeResult;
	for (int i = MAX_DYNAMIC_CHANNELS; i < total_channels && nSpaceRemaining; i++)
	{
		if ( channels[i].entchannel == entchannel && channels[i].sfx )
		{
			pResult->Set( channels[i].soundsource, 
						  channels[i].entchannel, 
						  channels[i].sfx->getname(), 
						  channels[i].origin,
						  channels[i].direction,
						  ( (float)channels[i].master_vol / 255.0 ),
						  DIST_MULT_TO_SNDLVL( channels[i].dist_mult ),
						  IsChannelLooped( i ),
						  channels[i].basePitch,
						  listener_origin,
						  channels[i].speakerentity );
			pResult++;
			nSpaceRemaining--;
		}
	}
	return (nSizeResult - nSpaceRemaining);
}


// Stop all sounds for entity on a channel.
void S_StopSound(int soundsource, int entchannel)
{
	THREAD_LOCK_SOUND();
	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	for ( int i = 0; i < list.Count(); i++ )
	{
		channel_t *pChannel = list.GetChannel(i);
		if (pChannel->soundsource == soundsource
			&& pChannel->entchannel == entchannel)
		{
			S_FreeChannel( pChannel );
		}
	}
}

channel_t *S_FindChannelByGuid( int guid )
{
	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	for ( int i = 0; i < list.Count(); i++ )
	{
		channel_t *pChannel = list.GetChannel(i);
		if ( pChannel->guid == guid )
		{
			return pChannel;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : guid - 
//-----------------------------------------------------------------------------
void S_StopSoundByGuid( int guid )
{
	THREAD_LOCK_SOUND();
	channel_t *pChannel = S_FindChannelByGuid( guid );
	if ( pChannel )
	{
		S_FreeChannel( pChannel );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : guid - 
//-----------------------------------------------------------------------------
float S_SoundDurationByGuid( int guid )
{
	channel_t *pChannel = S_FindChannelByGuid( guid );
	if ( !pChannel || !pChannel->sfx )
		return 0.0f;

	// NOTE: Looping sounds will return the length of a single loop
	// Use S_IsLoopingSoundByGuid to see if they are looped
	float flRate = pChannel->sfx->pSource->SampleRate() * pChannel->basePitch * 0.01f;
	int nTotalSamples = pChannel->sfx->pSource->SampleCount();
	return (flRate != 0.0f) ? nTotalSamples / flRate : 0.0f;
}


//-----------------------------------------------------------------------------
// Is this sound a looping sound?
//-----------------------------------------------------------------------------
bool S_IsLoopingSoundByGuid( int guid )
{
	channel_t *pChannel = S_FindChannelByGuid( guid );
	if ( !pChannel || !pChannel->sfx )
		return false;

	return( pChannel->sfx->pSource->IsLooped() );
}


//-----------------------------------------------------------------------------
// Purpose: Note that the guid is preincremented, so we can just return the current value as the "last sound" indicator
// Input  :  - 
// Output : int
//-----------------------------------------------------------------------------
int S_GetGuidForLastSoundEmitted()
{
	return s_nSoundGuid;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : guid - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool S_IsSoundStillPlaying( int guid )
{
	channel_t *pChannel = S_FindChannelByGuid( guid );
	return pChannel != NULL ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : guid - 
//			fvol - 
//-----------------------------------------------------------------------------
void S_SetVolumeByGuid( int guid, float fvol )
{
	channel_t *pChannel = S_FindChannelByGuid( guid );
	pChannel->master_vol = 255.0f * clamp( fvol, 0.0f, 1.0f );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : guid - 
// Output : float
//-----------------------------------------------------------------------------
float S_GetElapsedTimeByGuid( int guid )
{
	channel_t *pChannel = S_FindChannelByGuid( guid );
	if ( !pChannel )
		return 0.0f;

	CAudioMixer *mixer = pChannel->pMixer;
	if ( !mixer )
		return 0.0f;

	float elapsed = mixer->GetSamplePosition() / ( mixer->GetSource()->SampleRate() * pChannel->pitch * 0.01f );
	return elapsed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sndlist - 
//-----------------------------------------------------------------------------
void S_GetActiveSounds( CUtlVector< SndInfo_t >& sndlist )
{
	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	for ( int i = 0; i < list.Count(); i++ )
	{
		channel_t *ch = list.GetChannel(i);

		SndInfo_t info;

		info.m_nGuid			= ch->guid;
		info.m_filenameHandle	= ch->sfx ? ch->sfx->GetFileNameHandle() : NULL;
		info.m_nSoundSource		= ch->soundsource;
		info.m_nChannel			= ch->entchannel;
		// If a sound is being played through a speaker entity (e.g., on a monitor,), this is the
		//  entity upon which to show the lips moving, if the sound has sentence data
		info.m_nSpeakerEntity	= ch->speakerentity;
		info.m_flVolume			= (float)ch->master_vol / 255.0f;
		info.m_flLastSpatializedVolume = ch->last_vol;
		// Radius of this sound effect (spatialization is different within the radius)
		info.m_flRadius			= ch->radius;
		info.m_nPitch			= ch->basePitch;
		info.m_pOrigin			= &ch->origin;
		info.m_pDirection		= &ch->direction;

		// if true, assume sound source can move and update according to entity
		info.m_bUpdatePositions = ch->flags.bUpdatePositions;
		// true if playing linked sentence
		info.m_bIsSentence		= ch->flags.isSentence;
		// if true, bypass all dsp processing for this sound (ie: music)	
		info.m_bDryMix			= ch->flags.bdry;
		// true if sound is playing through in-game speaker entity.
		info.m_bSpeaker			= ch->flags.bSpeaker;
		// true if sound is using special DSP effect
		info.m_bSpecialDSP		= ( ch->special_dsp != 0 );
		// for snd_show, networked sounds get colored differently than local sounds
		info.m_bFromServer		= ch->flags.fromserver; 

		sndlist.AddToTail( info );
	}
}

void S_StopAllSounds( bool bClear )
{
	THREAD_LOCK_SOUND();
	int		i;

	if ( !g_AudioDevice )
		return;

	if ( !g_AudioDevice->IsActive() )
		return;

	total_channels = MAX_DYNAMIC_CHANNELS;	// no statics

	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	for ( i = 0; i < list.Count(); i++ )
	{
		channel_t *pChannel = list.GetChannel(i);
		if ( channels[i].sfx )
		{
			DevMsg( 1, "%2d:Stopped sound %s\n", i, channels[i].sfx->getname() );
		}
		S_FreeChannel( pChannel );
	}

	Q_memset( channels, 0, MAX_CHANNELS * sizeof(channel_t) );

	if ( bClear )
	{
		S_ClearBuffer();
	}

	// Clear any remaining soundfade
	memset( &soundfade, 0, sizeof( soundfade ) );

	g_AudioDevice->StopAllSounds();
	Assert( g_ActiveChannels.GetActiveCount() == 0 );
}

void S_StopAllSoundsC( void )
{
	S_StopAllSounds( true );
}

void S_OnLoadScreen( bool value )
{
	s_bOnLoadScreen = value;
}

void S_ClearBuffer( void )
{
	if ( !g_AudioDevice )
		return;

	g_AudioDevice->ClearBuffer();
	DSP_ClearState();
	MIX_ClearAllPaintBuffers( PAINTBUFFER_SIZE, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : percent - 
//			holdtime - 
//			intime - 
//			outtime - 
//-----------------------------------------------------------------------------
void S_SoundFade( float percent, float holdtime, float intime, float outtime )
{
	soundfade.starttime				= g_pSoundServices->GetHostTime();  

	soundfade.initial_percent		= percent;       
	soundfade.fadeouttime			= outtime;    
	soundfade.holdtime				= holdtime;   
	soundfade.fadeintime			= intime;
}

//-----------------------------------------------------------------------------
// Purpose: Modulates sound volume on the client.
//-----------------------------------------------------------------------------
void S_UpdateSoundFade(void)
{
	float	totaltime;
	float	f;
	// Determine current fade value.

	// Assume no fading remains
	soundfade.percent = 0;  

	totaltime = soundfade.fadeouttime + soundfade.fadeintime + soundfade.holdtime;

	float elapsed = g_pSoundServices->GetHostTime() - soundfade.starttime;

	// Clock wrapped or reset (BUG) or we've gone far enough
	if ( elapsed < 0.0f || elapsed >= totaltime || totaltime <= 0.0f )
	{
		return;
	}

	// We are in the fade time, so determine amount of fade.
	if ( soundfade.fadeouttime > 0.0f && ( elapsed < soundfade.fadeouttime ) )
	{
		// Ramp up
		f = elapsed / soundfade.fadeouttime;
	}
	// Inside the hold time
	else if ( elapsed <= ( soundfade.fadeouttime + soundfade.holdtime ) )
	{
		// Stay
		f = 1.0f;
	}
	else
	{
		// Ramp down
		f = ( elapsed - ( soundfade.fadeouttime + soundfade.holdtime ) ) / soundfade.fadeintime;
		// backward interpolated...
		f = 1.0f - f;
	}

	// Spline it.
	f = SimpleSpline( f );
	f = clamp( f, 0.0f, 1.0f );

	soundfade.percent = soundfade.initial_percent * f;
}


//=============================================================================

// Global Voice Ducker - enabled in vcd scripts, when characters deliver important dialog.  Overrides all
// other mixer ducking, and ducks all other sounds except dialog.

ConVar snd_ducktovolume( "snd_ducktovolume", "0.55", FCVAR_ARCHIVE );
ConVar snd_duckerattacktime( "snd_duckerattacktime", "0.5", FCVAR_ARCHIVE );
ConVar snd_duckerreleasetime( "snd_duckerreleasetime", "2.5", FCVAR_ARCHIVE );
ConVar snd_duckerthreshold("snd_duckerthreshold", "0.15", FCVAR_ARCHIVE );

static void S_UpdateVoiceDuck( int voiceChannelCount, int voiceChannelMaxVolume, float frametime )
{
	float volume_when_ducked = snd_ducktovolume.GetFloat();
	int volume_threshold = (int)(snd_duckerthreshold.GetFloat() * 255.0);

	float duckTarget = 1.0;
	if ( voiceChannelCount > 0 )
	{
		voiceChannelMaxVolume = clamp(voiceChannelMaxVolume, 0, 255);
		
		// duckTarget = RemapVal( voiceChannelMaxVolume, 0, 255, 1.0, volume_when_ducked );
	
		// KB: Change: ducker now active if any character is speaking above threshold volume.
		// KB: Active ducker drops all volumes to volumes * snd_duckvolume

		if ( voiceChannelMaxVolume > volume_threshold )
			duckTarget = volume_when_ducked;
	}
	float rate = ( duckTarget < g_DuckScale ) ? snd_duckerattacktime.GetFloat() : snd_duckerreleasetime.GetFloat();
	g_DuckScale = Approach( duckTarget, g_DuckScale, frametime * ((1-volume_when_ducked) / rate) );
}

// set 2d forward vector, given 3d right vector.
// NOTE: this should only be used for a listener forward
// vector from a listener right vector. It is not a general use routine.

void ConvertListenerVectorTo2D( Vector *pvforward, Vector *pvright )
{
	// get 2d forward direction vector, ignoring pitch angle
	QAngle angles2d;
	Vector source2d;
	Vector listener_forward2d;

	source2d = *pvright;
	source2d.z = 0.0;

	VectorNormalize(source2d);

	// convert right vector to euler angles (yaw & pitch)

	VectorAngles(source2d, angles2d);

	// get forward angle of listener

	angles2d[PITCH]	= 0;
	angles2d[YAW] += 90; // rotate 90 ccw
	angles2d[ROLL] = 0;
	
	if (angles2d[YAW] >= 360)
		angles2d[YAW] -= 360;

	AngleVectors(angles2d, &listener_forward2d);

	VectorNormalize(listener_forward2d);

	*pvforward = listener_forward2d;
}

// If this is nonzero, we will only spatialize some of the static 
// channels each frame. The round robin will spatialize 1 / (2 ^ x) 
// of the spatial channels each frame.
ConVar snd_spatialize_roundrobin( "snd_spatialize_roundrobin", "0", FCVAR_ALLOWED_IN_COMPETITIVE, "Lowend optimization: if nonzero, spatialize only a fraction of sound channels each frame. 1/2^x of channels will be spatialized per frame." );
/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update( const AudioState_t *pAudioState )
{
	VPROF("S_Update");
	channel_t	*ch;
	channel_t	*combine;
	static unsigned int s_roundrobin = 0 ; ///< number of times this function is called.
									  ///< used instead of host_frame because that number
									  ///< isn't necessarily available here (sez Yahn).

	if ( !g_AudioDevice->IsActive() )
		return;

	g_SndMutex.Lock();

	// Update any client side sound fade
	S_UpdateSoundFade();

	if ( pAudioState )
	{
		VectorCopy( pAudioState->m_Origin, listener_origin );
		AngleVectors( pAudioState->m_Angles, &listener_forward, &listener_right, &listener_up ); 
		s_bIsListenerUnderwater = pAudioState->m_bIsUnderwater;
	}
	else
	{
		VectorCopy( vec3_origin, listener_origin );
		VectorCopy( vec3_origin, listener_forward );
		VectorCopy( vec3_origin, listener_right );
		VectorCopy( vec3_origin, listener_up );
		s_bIsListenerUnderwater = false;
	}

	g_AudioDevice->UpdateListener( listener_origin, listener_forward, listener_right, listener_up );
 
	combine = NULL;

	int voiceChannelCount = 0;
	int voiceChannelMaxVolume = 0;

	// reset traceline counter for this frame
	g_snd_trace_count = 0;

	// calculate distance to nearest walls, update dsp_spatial
	// updates one wall only per frame (one trace per frame)
	SND_SetSpatialDelays();

	// updates dsp_room if automatic room detection enabled
	DAS_CheckNewRoomDSP();

	// update spatialization for static and dynamic sounds	
	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );

	if (snd_spatialize_roundrobin.GetInt() == 0)
	{
		// spatialize each channel each time
		for ( int i = 0; i < list.Count(); i++ )
		{
			ch = list.GetChannel(i);
			Assert(ch->sfx);
			Assert(ch->activeIndex > 0);

			SND_Spatialize(ch);         // respatialize channel

			if ( ch->sfx->pSource && ch->sfx->pSource->IsVoiceSource() )
			{
				voiceChannelCount++;
				voiceChannelMaxVolume = max(voiceChannelMaxVolume, ChannelGetMaxVol( ch) );
			}
		}
	}
	else	// lowend performance improvement: spatialize only some  channels each frame.
	{
		unsigned int robinmask = (1 << snd_spatialize_roundrobin.GetInt()) - 1;

		// now do static channels
		for ( int i = 0 ; i < list.Count() ; ++i )
		{
			ch = list.GetChannel(i);
			Assert(ch->sfx);
			Assert(ch->activeIndex > 0);

			// need to check bfirstpass because sound tracing may have been deferred
			if ( ch->flags.bfirstpass || (robinmask & s_roundrobin) == ( i & robinmask ) )
			{
				SND_Spatialize(ch);         // respatialize channel
			}

			if ( ch->sfx->pSource && ch->sfx->pSource->IsVoiceSource() )
			{
				voiceChannelCount++;
				voiceChannelMaxVolume = max( voiceChannelMaxVolume, ChannelGetMaxVol( ch) );
			}
		}

		++s_roundrobin;
	}



	SND_ChannelTraceReset();

	// set new target for voice ducking
	float frametime = g_pSoundServices->GetHostFrametime();
	S_UpdateVoiceDuck( voiceChannelCount, voiceChannelMaxVolume, frametime );

	// update x360 music volume
	g_DashboardMusicMixValue = Approach( g_DashboardMusicMixTarget, g_DashboardMusicMixValue, g_DashboardMusicFadeRate * frametime );

	//
	// debugging output
	//
	if (snd_show.GetInt())
	{
		con_nprint_t np;
		np.time_to_live = 2.0f;
		np.fixed_width_font = true;

		int total = 0;

		CChannelList activeChannels;
		g_ActiveChannels.GetActiveChannels( activeChannels );
		for ( int i = 0; i < activeChannels.Count(); i++ )
		{
			channel_t *channel = activeChannels.GetChannel(i);
			if ( !channel->sfx )
				continue;

			np.index = total + 2;
			if ( channel->flags.fromserver )
			{
				np.color[0] = 1.0;
				np.color[1] = 0.8;
				np.color[2] = 0.1;
			}
			else
			{
				np.color[0] = 0.1;
				np.color[1] = 0.9;
				np.color[2] = 1.0;
			}

			unsigned int sampleCount = RemainingSamples( channel );
			float timeleft = (float)sampleCount / (float)channel->sfx->pSource->SampleRate();
			bool bLooping = channel->sfx->pSource->IsLooped();

			if (snd_surround.GetInt() < 4)
			{
				Con_NXPrintf ( &np, "%02i l(%03d) r(%03d) vol(%03d) ent(%03d) pos(%6d %6d %6d) timeleft(%f) looped(%d) %50s", 
					total+ 1, 
					(int)channel->fvolume[IFRONT_LEFT],
					(int)channel->fvolume[IFRONT_RIGHT],
					channel->master_vol,
					channel->soundsource,
					(int)channel->origin[0],
					(int)channel->origin[1],
					(int)channel->origin[2],
					timeleft,
					bLooping, 
					channel->sfx->getname());
			}
			else
			{
				Con_NXPrintf ( &np, "%02i l(%03d) c(%03d) r(%03d) rl(%03d) rr(%03d) vol(%03d) ent(%03d) pos(%6d %6d %6d) timeleft(%f) looped(%d) %50s", 
					total+ 1, 
					(int)channel->fvolume[IFRONT_LEFT],
					(int)channel->fvolume[IFRONT_CENTER],
					(int)channel->fvolume[IFRONT_RIGHT],
					(int)channel->fvolume[IREAR_LEFT],
					(int)channel->fvolume[IREAR_RIGHT],
					channel->master_vol,
					channel->soundsource,
					(int)channel->origin[0],
					(int)channel->origin[1],
					(int)channel->origin[2],
					timeleft,
					bLooping,
					channel->sfx->getname());
			}

			if ( snd_visualize.GetInt() )
			{
				CDebugOverlay::AddTextOverlay( channel->origin, 0.05f, channel->sfx->getname() );
			}

			total++;
		}

		while ( total <= 128 )
		{
			Con_NPrintf( total + 2, "" );
			total++;
		}
	}

	g_SndMutex.Unlock();

	if ( s_bOnLoadScreen )
		return;

	// not time to update yet?
	double tNow = Plat_FloatTime();
	// this is the last time we ran a sound frame
	g_LastSoundFrame = tNow;
	// this is the last time we did mixing (extraupdate also advances this if it mixes)
	g_LastMixTime = tNow;
	// mix some sound
	// try to stay at least one frame + mixahead ahead in the mix.
	g_EstFrameTime = (g_EstFrameTime * 0.9f) + (g_pSoundServices->GetHostFrametime() * 0.1f);
	S_Update_( g_EstFrameTime + snd_mixahead.GetFloat() );
}

CON_COMMAND( snd_dumpclientsounds, "Dump sounds to VXConsole" )
{
	con_nprint_t np;
	np.time_to_live = 2.0f;
	np.fixed_width_font = true;

	int total = 0;

	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	for ( int i = 0; i < list.Count(); i++ )
	{
		channel_t *ch = list.GetChannel(i);
		if ( !ch->sfx )
			continue;

		unsigned int sampleCount = RemainingSamples( ch );
		float timeleft = (float)sampleCount / (float)ch->sfx->pSource->SampleRate();
		bool bLooping = ch->sfx->pSource->IsLooped();
		const char *pszclassname = GetClientClassname(ch->soundsource);

		Msg( "%02i %s l(%03d) c(%03d) r(%03d) rl(%03d) rr(%03d) vol(%03d) pos(%6d %6d %6d) timeleft(%f) looped(%d) %50s chan:%d ent(%03d):%s\n", 
			total+ 1, 
			ch->flags.fromserver ? "SERVER" : "CLIENT",
			(int)ch->fvolume[IFRONT_LEFT], 
			(int)ch->fvolume[IFRONT_CENTER],
			(int)ch->fvolume[IFRONT_RIGHT], 
			(int)ch->fvolume[IREAR_LEFT], 
			(int)ch->fvolume[IREAR_RIGHT], 
			ch->master_vol,
			(int)ch->origin[0],
			(int)ch->origin[1],
			(int)ch->origin[2],
			timeleft,
			bLooping, 
			ch->sfx->getname(),
			ch->entchannel,
			ch->soundsource,
			pszclassname ? pszclassname : "NULL" );

		total++;
	}
}

//-----------------------------------------------------------------------------
// Set g_soundtime to number of full samples that have been transfered out to hardware
// since start.
//-----------------------------------------------------------------------------
void GetSoundTime(void)
{
	int		fullsamples;
	int		sampleOutCount;

	// size of output buffer in *full* 16 bit samples
	// A 2 channel device has a *full* sample consisting of a 16 bit LR pair.
	// A 1 channel device has a *full* sample consiting of a 16 bit single sample.
	fullsamples = g_AudioDevice->DeviceSampleCount() / g_AudioDevice->DeviceChannels();

	// NOTE: it is possible to miscount buffers if it has wrapped twice between
	// calls to S_Update.  However, since the output buffer size is > 1 second of sound, 
	// this should only occur for framerates lower than 1hz

	// sampleOutCount is counted in 16 bit *full* samples, of number of samples output to hardware
	// for current output buffer
	sampleOutCount = g_AudioDevice->GetOutputPosition();
	if ( sampleOutCount < s_oldsampleOutCount )
	{
		// buffer wrapped
		s_buffers++;
		if ( g_paintedtime > 0x70000000 )
		{	
			// time to chop things off to avoid 32 bit limits
			s_buffers = 0;
			g_paintedtime = fullsamples;
			S_StopAllSounds( true );
		}
	}
	
	s_oldsampleOutCount = sampleOutCount;

	if ( cl_movieinfo.IsRecording() || IsReplayRendering() )
	{
		// when recording a replay, we look at the record frame rate, not the engine frame rate
		
#if defined( REPLAY_ENABLED )
		extern IClientReplayContext *g_pClientReplayContext;
		if ( IsReplayRendering() )
		{
			IReplayMovieRenderer *pMovieRenderer = (g_pClientReplayContext != NULL) ? g_pClientReplayContext->GetMovieRenderer() : NULL;
		
			if ( pMovieRenderer && pMovieRenderer->IsAudioSyncFrame() )
			{
				float t = g_pSoundServices->GetHostTime();
				if ( s_lastsoundtime != t )
				{
					float frameTime = pMovieRenderer->GetRecordingFrameDuration();
					float fSamples = frameTime * (float) g_AudioDevice->DeviceDmaSpeed() + g_ReplaySoundTimeFracAccumulator;
					
					float intPart = (float) floor( fSamples );
					g_ReplaySoundTimeFracAccumulator = fSamples - intPart;
					
					g_soundtime += (int) intPart;
					s_lastsoundtime = t;
				}
			}
		
		}
		else	// cl_movieinfo.IsRecording() 
				// in movie, just mix one frame worth of sound
#endif
		{
		
			float t = g_pSoundServices->GetHostTime();
			if ( s_lastsoundtime != t )
			{
				g_soundtime += g_pSoundServices->GetHostFrametime() * g_AudioDevice->DeviceDmaSpeed();
				
				s_lastsoundtime = t;
			}
		}
	}
	else
	{
		// g_soundtime indicates how many *full* samples have actually been
		// played out to dma
		g_soundtime = s_buffers*fullsamples + sampleOutCount;
	}
}

void S_ExtraUpdate( void )
{
	if ( !g_AudioDevice || !g_pSoundServices )
		return;

	if ( !g_AudioDevice->IsActive() )
		return;

	if ( s_bOnLoadScreen )
		return;

	if ( snd_noextraupdate.GetInt() || cl_movieinfo.IsRecording() || IsReplayRendering() )
		return;		// don't pollute timings

	// If listener position and orientation has not yet been updated (ie: no call to S_Update since level load)
	// then don't mix.  Important - mixing with listener at 'false' origin causes
	// some sounds to incorrectly spatialize to 0 volume, killing them before they can play.

	if ((listener_origin  == vec3_origin) && 
		(listener_forward == vec3_origin) &&
		(listener_right	  == vec3_origin) &&
		(listener_up	  == vec3_origin) )
		return;

	// Only mix if you have used up 90% of the mixahead buffer
	double tNow = Plat_FloatTime();
	float delta = (tNow - g_LastMixTime);
	// we know we were at least snd_mixahead seconds ahead of the output the last time we did mixing
	// if we're not close to running out just exit to avoid small mix batches
	if ( delta > 0 && delta < (snd_mixahead.GetFloat() * 0.9f) )
		return;
	g_LastMixTime = tNow;

	g_pSoundServices->OnExtraUpdate();
	// Shouldn't have to do any work here if your framerate hasn't dropped
	S_Update_( snd_mixahead.GetFloat() );
}

extern void DEBUG_StartSoundMeasure(int type, int samplecount );
extern void DEBUG_StopSoundMeasure(int type, int samplecount );

void S_Update_Guts( float mixAheadTime )
{
	VPROF( "S_Update_Guts" );
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	DEBUG_StartSoundMeasure(4, 0);

	// Update our perception of audio time.
	// 'g_soundtime' tells how many samples have
	// been played out of the dma buffer since sound system startup.
	// 'g_paintedtime' indicates how many samples we've actually mixed
	// and sent to the dma buffer since sound system startup.
	GetSoundTime();

//	if ( g_soundtime > g_paintedtime )
//	{
//		// if soundtime > paintedtime, then the dma buffer
//		// has played out more sound than we've actually
//		// mixed.  We need to call S_Update_ more often.
//
//		DevMsg ("S_Update_ : Underflow\n"); 
//		paintedtime = g_soundtime;		
//	}
//	(kdb) above code doesn't handle underflow correctly 
//	should actually zero out the paintbuffer to advance to the new
//	time.

	// mix ahead of current position
	unsigned endtime = g_AudioDevice->PaintBegin( mixAheadTime, g_soundtime, g_paintedtime );

	int samples = endtime - g_paintedtime;
	samples = samples < 0 ? 0 : samples;
	if ( samples )
	{
		THREAD_LOCK_SOUND();

		DEBUG_StartSoundMeasure( 2, samples );

		MIX_PaintChannels( endtime, s_bIsListenerUnderwater );

		MXR_DebugShowMixVolumes();

		MXR_UpdateAllDuckerVolumes();

		DEBUG_StopSoundMeasure( 2, 0 );
	}

	g_AudioDevice->PaintEnd();
	DEBUG_StopSoundMeasure( 4, samples );
}

#if !defined( _X360 )
#define THREADED_MIX_TIME 33
#else
#define THREADED_MIX_TIME XMA_POLL_RATE
#endif

ConVar snd_ShowThreadFrameTime( "snd_ShowThreadFrameTime", "0" );

bool g_bMixThreadExit;
ThreadHandle_t g_hMixThread;
void S_Update_Thread()
{
	float frameTime = THREADED_MIX_TIME * 0.001f;
	double lastFrameTime = Plat_FloatTime();

	while ( !g_bMixThreadExit )
	{
		// mixing (for 360) needs to be updated at a steady rate
		// large update times causes the mixer to demand more audio data
		// the 360 decoder has finite latency and cannot fulfill spike requests
		float t0 = Plat_FloatTime();
		S_Update_Guts( frameTime + snd_mixahead.GetFloat() );
		int updateTime = ( Plat_FloatTime() - t0 ) * 1000.0f;

		// try to maintain a steadier rate by compensating for fluctuating mix times
		int sleepTime = THREADED_MIX_TIME - updateTime;
		if ( sleepTime > 0 )
		{
			ThreadSleep( sleepTime );
		}

		// mimic a frametime needed for sound update
		double t1 = Plat_FloatTime();
		frameTime = t1 - lastFrameTime;
		lastFrameTime = t1;

		if ( snd_ShowThreadFrameTime.GetBool() )
		{
			Msg( "S_Update_Thread: frameTime: %d ms\n", (int)( frameTime * 1000.0f ) );
		}
	}
}

void S_ShutdownMixThread()
{
	if ( g_hMixThread )
	{
		g_bMixThreadExit = true;
		ThreadJoin( g_hMixThread );
		ReleaseThreadHandle( g_hMixThread );
		g_hMixThread = NULL;
	}
}

void S_Update_( float mixAheadTime )
{
	if ( !IsConsole() || !snd_mix_async.GetBool() )
	{
		S_ShutdownMixThread();
		S_Update_Guts( mixAheadTime );
	}
	else
	{
		if ( !g_hMixThread )
		{
			g_bMixThreadExit = false;
			g_hMixThread = ThreadExecuteSolo( "SndMix", S_Update_Thread );
			if ( IsX360() )
			{
				ThreadSetAffinity( g_hMixThread, XBOX_PROCESSOR_5 );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Threaded mixing enable. Purposely hiding enable/disable details.
//-----------------------------------------------------------------------------
void S_EnableThreadedMixing( bool bEnable )
{
	if ( snd_mix_async.GetBool() != bEnable )
	{
		snd_mix_async.SetValue( bEnable );
	}
}

/*
===============================================================================

console functions

===============================================================================
*/
extern void DSP_DEBUGSetParams(int ipreset, int iproc, float *pvalues, int cparams);
extern void DSP_DEBUGReloadPresetFile( void );

void S_DspParms( const CCommand &args )
{
	if ( args.ArgC() == 1)
	{
		// if dsp_parms with no arguments, reload entire preset file

		 DSP_DEBUGReloadPresetFile();

		 return;
	}

	if ( args.ArgC() < 4 )
	{
		Msg( "Usage: dsp_parms PRESET# PROC# param0 param1 ...up to param15 \n" );
		return;
	}
	
	int cparam = min( args.ArgC() - 4, 16);

	float params[16];
	Q_memset( params, 0, sizeof(float) * 16 );

	// get preset & proc
	int idsp, iproc;
	idsp = Q_atof( args[1] );
	iproc = Q_atof( args[2] );

	// get params
	for (int i = 0; i < cparam; i++)
	{
		params[i] = Q_atof( args[i+4] );
	}

	// set up params & switch preset
	DSP_DEBUGSetParams(idsp, iproc, params, cparam);
}

static ConCommand dsp_parm("dsp_reload", S_DspParms );

void S_Play( const char *pszName, bool flush = false )
{
	int			inCache;
	char		szName[256];
	CSfxTable	*pSfx;
	
	Q_strncpy( szName, pszName, sizeof( szName ) );
	if ( !Q_strrchr( pszName, '.' ) )
	{
		Q_strncat( szName, ".wav", sizeof( szName ), COPY_ALL_CHARACTERS );
	}

	pSfx = S_FindName( szName, &inCache );
	if ( inCache && flush )
	{
		pSfx->pSource->CacheUnload();
	}

	StartSoundParams_t params;
	params.staticsound = false;
	params.soundsource = g_pSoundServices->GetViewEntity();
	params.entchannel = CHAN_REPLACE;
	params.pSfx = pSfx;
	params.origin = listener_origin;
	params.fvol = 1.0f;
	params.soundlevel = SNDLVL_NONE;
	params.flags = 0;
	params.pitch = PITCH_NORM;

	S_StartSound( params );
}

static void S_Play( const CCommand &args )
{
	bool bFlush = !Q_stricmp( args[0], "playflush" );
	for ( int i = 1; i < args.ArgC(); ++i )
	{
		S_Play( args[i], bFlush );
	}
}

static void S_PlayVol( const CCommand &args )
{
	static int hash=543;
	float vol;
	char name[256];
	CSfxTable *pSfx;
	
	for ( int i = 1; i<args.ArgC(); i += 2 )
	{
		if ( !Q_strrchr( args[i], '.') )
		{
			Q_strncpy( name, args[i], sizeof( name ) );
			Q_strncat( name, ".wav", sizeof( name ), COPY_ALL_CHARACTERS );
		}
		else
		{
			Q_strncpy( name, args[i], sizeof( name ) );
		}

		pSfx = S_PrecacheSound( name );
		vol = Q_atof( args[i+1] );

		StartSoundParams_t params;
		params.staticsound = false;
		params.soundsource = hash++;
		params.entchannel = CHAN_AUTO;
		params.pSfx = pSfx;
		params.origin = listener_origin;
		params.fvol = vol;
		params.soundlevel = SNDLVL_NONE;
		params.flags = 0;
		params.pitch = PITCH_NORM;

		S_StartDynamicSound( params );
	}
}

static void S_PlayDelay( const CCommand &args )
{
	if ( args.ArgC() != 3 )
	{
		Msg( "Usage:  sndplaydelay delay_in_sec (negative to skip ahead) soundname\n" );
		return;
	}

	char szName[256];
	CSfxTable *pSfx;

	float delay = Q_atof( args[ 1 ] );
	
	Q_strncpy(szName, args[ 2 ], sizeof( szName ) );
	if ( !Q_strrchr( args[ 2 ], '.' ) )
	{
		Q_strncat( szName, ".wav", sizeof( szName ), COPY_ALL_CHARACTERS );
	}

	pSfx = S_FindName( szName, NULL );
	
	StartSoundParams_t params;
	params.staticsound = false;
	params.soundsource = g_pSoundServices->GetViewEntity();
	params.entchannel = CHAN_REPLACE;
	params.pSfx = pSfx;
	params.origin = listener_origin;
	params.fvol = 1.0f;
	params.soundlevel = SNDLVL_NONE;
	params.flags = 0;
	params.pitch = PITCH_NORM;
	params.delay = delay;

	S_StartSound( params );

}
static ConCommand sndplaydelay( "sndplaydelay", S_PlayDelay, "Usage:  sndplaydelay delay_in_sec (negative to skip ahead) soundname", FCVAR_SERVER_CAN_EXECUTE );

static bool SortByNameLessFunc( const int &lhs, const int &rhs )
{
	CSfxTable *pSfx1 = s_Sounds[lhs].pSfx;
	CSfxTable *pSfx2 = s_Sounds[rhs].pSfx;

	return CaselessStringLessThan( pSfx1->getname(), pSfx2->getname() );
}

void S_SoundList(void)
{
	CSfxTable		*sfx;
	CAudioSource	*pSource;
	int				size, total;

	total = 0;
	for ( int i = s_Sounds.FirstInorder(); i != s_Sounds.InvalidIndex(); i = s_Sounds.NextInorder( i ) )
	{
		sfx = s_Sounds[i].pSfx;

		pSource = sfx->pSource;
		if ( !pSource || !pSource->IsCached() )
			continue;

		size = pSource->SampleSize() * pSource->SampleCount();
		total += size;

		if ( pSource->IsLooped() )
			Msg ("L");
		else
			Msg (" ");
		Msg("(%2db) %6i : %s\n", pSource->SampleSize(),  size, sfx->getname());
	}
	Msg( "Total resident: %i\n", total );
}

#if defined( _X360 )
CON_COMMAND( vx_soundlist, "Dump sounds to VXConsole" )
{
	CSfxTable		*sfx;
	CAudioSource	*pSource;
	int				dataSize;
	char			*pFormatStr;
	int				sampleRate;
	int				sampleBits;
	int				streamed;
	int				looped;
	int				channels;
	int				numSamples;

	int numSounds = s_Sounds.Count();
	xSoundList_t* pSoundList = new xSoundList_t[numSounds];

	int i = 0;
	for ( int iSrcSound=s_Sounds.FirstInorder(); iSrcSound != s_Sounds.InvalidIndex(); iSrcSound = s_Sounds.NextInorder( iSrcSound ) )
	{
		dataSize = -1;
		sampleRate = -1;
		sampleBits = -1;
		pFormatStr = "???";
		streamed = -1;
		looped = -1;
		channels = -1;
		numSamples = -1;

		sfx     = s_Sounds[iSrcSound].pSfx;
		pSource = sfx->pSource;
		if ( pSource && pSource->IsCached() )
		{
			numSamples = pSource->SampleCount();
			dataSize = pSource->DataSize();
			sampleRate = pSource->SampleRate();
			streamed = pSource->IsStreaming();
			looped = pSource->IsLooped();
			channels = pSource->IsStereoWav() ? 2 : 1;

			if ( pSource->Format() == WAVE_FORMAT_ADPCM )
			{
				pFormatStr = "ADPCM";
				sampleBits = 16;
			}
			else if ( pSource->Format() == WAVE_FORMAT_PCM )
			{
				pFormatStr = "PCM";
				sampleBits = (pSource->SampleSize() * 8)/channels;
			}
			else if ( pSource->Format() == WAVE_FORMAT_XMA )
			{
				pFormatStr = "XMA";
				sampleBits = 16;
			}
		}

		V_strncpy( pSoundList[i].name, sfx->getname(), sizeof( pSoundList[i].name ) );
		V_strncpy( pSoundList[i].formatName, pFormatStr, sizeof( pSoundList[i].formatName ) );
		pSoundList[i].rate = sampleRate;
		pSoundList[i].bits = sampleBits;
		pSoundList[i].channels = channels;
		pSoundList[i].looped = looped;
		pSoundList[i].dataSize = dataSize;
		pSoundList[i].numSamples = numSamples;
		pSoundList[i].streamed = streamed;
		++i;
	}

	XBX_rSoundList( numSounds, pSoundList );
	delete [] pSoundList;
}
#endif

extern unsigned g_snd_time_debug;
extern unsigned g_snd_call_time_debug;
extern unsigned g_snd_count_debug;
extern unsigned g_snd_samplecount;
extern unsigned g_snd_frametime;
extern unsigned g_snd_frametime_total;
extern int g_snd_profile_type;

// start measuring sound perf, 100 reps
// type 1 - dsp, 2 - mix, 3 - load sound, 4 - all sound
// set type via ConVar snd_profile

void DEBUG_StartSoundMeasure(int type, int samplecount )
{
	if (type != g_snd_profile_type)
		return;

	if (samplecount)
		g_snd_samplecount += samplecount;

	g_snd_call_time_debug = Plat_MSTime();
}

// show sound measurement after 25 reps - show as % of total frame
// type 1 - dsp, 2 - mix, 3 - load sound, 4 - all sound

// BUGBUG: snd_profile 4 reports a lower average because it's average cost
// PER CALL and most calls (via SoundExtraUpdate()) don't do any work and 
// bring the average down.  If you want an average PER FRAME instead, it's generally higher.
void DEBUG_StopSoundMeasure(int type, int samplecount )
{
	if (type != g_snd_profile_type)
		return;

	if (samplecount)
		g_snd_samplecount += samplecount;

	// add total time since last frame

	g_snd_frametime_total += Plat_MSTime() - g_snd_frametime;

	// performance timing

	g_snd_time_debug += Plat_MSTime() - g_snd_call_time_debug;

	if (++g_snd_count_debug >= 100)
	{
		switch (g_snd_profile_type)
		{
		case 1: 
			Msg("dsp: (%2.2f) millisec   ", ((float)g_snd_time_debug) / 100.0); 
			Msg("(%2.2f) pct of frame \n", 100.0 * ((float)g_snd_time_debug) / ((float)g_snd_frametime_total)); 
			break;
		case 2: 
			Msg("mix+dsp:(%2.2f) millisec   ", ((float)g_snd_time_debug) / 100.0);
			Msg("(%2.2f) pct of frame \n", 100.0 * ((float)g_snd_time_debug) / ((float)g_snd_frametime_total)); 
			break;
		case 3: 
			//if ( (((float)g_snd_time_debug) / 100.0) < 0.01 )
			//	break;
			Msg("snd load: (%2.2f) millisec   ", ((float)g_snd_time_debug) / 100.0); 
			Msg("(%2.2f) pct of frame \n", 100.0 * ((float)g_snd_time_debug) / ((float)g_snd_frametime_total)); 
			break;
		case 4: 
			Msg("sound: (%2.2f) millisec   ", ((float)g_snd_time_debug) / 100.0); 
			Msg("(%2.2f) pct of frame (%d samples) \n", 100.0 * ((float)g_snd_time_debug) / ((float)g_snd_frametime_total), g_snd_samplecount); 
			break;
		}
		
		g_snd_count_debug = 0;
		g_snd_time_debug = 0;
		g_snd_samplecount = 0;	
		g_snd_frametime_total = 0;
	}

	g_snd_frametime = Plat_MSTime();
}

// speak a sentence from console; works by passing in "!sentencename"
// or "sentence"

extern ConVar dsp_room;

static void S_Say( const CCommand &args )
{
	CSfxTable *pSfx;

	if ( !g_AudioDevice->IsActive() )
		return;

	char sound[256];
	Q_strncpy( sound, args[1], sizeof( sound ) );		
	
	// DEBUG - test performance of dsp code
	if ( !Q_stricmp( sound, "dsp" ) )
	{
		unsigned time;
		int i;
		int count = 10000;
		int idsp; 

		for (i = 0; i < PAINTBUFFER_SIZE; i++)
		{
			g_paintbuffer[i].left = RandomInt(0,2999);
			g_paintbuffer[i].right = RandomInt(0,2999);
		}

		Msg ("Start profiling 10,000 calls to DSP\n");
		
		idsp = dsp_room.GetInt();
		
		// get system time

		time = Plat_MSTime();
		
		for (i = 0; i < count; i++)
		{
			// SX_RoomFX(PAINTBUFFER_SIZE, TRUE, TRUE);

			DSP_Process(idsp, g_paintbuffer, NULL, NULL, PAINTBUFFER_SIZE);

		}
		// display system time delta 
		Msg("%d milliseconds \n", Plat_MSTime() - time);
		return;
	} 
	
	if ( !Q_stricmp(sound, "paint") )
	{
		unsigned time;
		int count = 10000;
		static int hash=543;
		int psav = g_paintedtime;

		Msg ("Start profiling MIX_PaintChannels\n");
		
		pSfx = S_PrecacheSound("ambience/labdrone1.wav");

		StartSoundParams_t params;
		params.staticsound = false;
		params.soundsource = hash++;
		params.entchannel = CHAN_AUTO;
		params.pSfx = pSfx;
		params.origin = listener_origin;
		params.fvol = 1.0f;
		params.soundlevel = SNDLVL_NONE;
		params.flags = 0;
		params.pitch = PITCH_NORM;

		S_StartDynamicSound( params );

		// get system time
		time = Plat_MSTime();

		// paint a boatload of sound

		MIX_PaintChannels( g_paintedtime + 512*count, s_bIsListenerUnderwater );		

		// display system time delta 
		Msg("%d milliseconds \n", Plat_MSTime() - time);
		g_paintedtime = psav;
		return;
	}

	// DEBUG
	if ( !TestSoundChar( sound, CHAR_SENTENCE ) )
	{
		// build a fake sentence name, then play the sentence text

		Q_strncpy(sound, "xxtestxx ", sizeof( sound ) );
		Q_strncat(sound, args[1], sizeof( sound ), COPY_ALL_CHARACTERS );

		int addIndex = g_Sentences.AddToTail();
		sentence_t *pSentence = &g_Sentences[addIndex];
		pSentence->pName = sound;
		pSentence->length = 0;

		// insert null terminator after sentence name
		sound[8] = 0;

		pSfx = S_PrecacheSound ("!xxtestxx");
		if (!pSfx)
		{
			Msg ("S_Say: can't cache %s\n", sound);
			return;
		}

		StartSoundParams_t params;
		params.staticsound = false;
		params.soundsource = g_pSoundServices->GetViewEntity();
		params.entchannel = CHAN_REPLACE;
		params.pSfx = pSfx;
		params.origin = vec3_origin;
		params.fvol = 1.0f;
		params.soundlevel = SNDLVL_NONE;
		params.flags = 0;
		params.pitch = PITCH_NORM;

		S_StartDynamicSound ( params );
		
		// remove last
		g_Sentences.Remove( g_Sentences.Size() - 1 );
	}
	else
	{
		pSfx = S_FindName(sound, NULL);
		if (!pSfx)
		{
			Msg ("S_Say: can't find sentence name %s\n", sound);
			return;
		}

		StartSoundParams_t params;
		params.staticsound = false;
		params.soundsource = g_pSoundServices->GetViewEntity();
		params.entchannel = CHAN_REPLACE;
		params.pSfx = pSfx;
		params.origin = vec3_origin;
		params.fvol = 1.0f;
		params.soundlevel = SNDLVL_NONE;
		params.flags = 0;
		params.pitch = PITCH_NORM;

		S_StartDynamicSound( params );
	}
}


//------------------------------------------------------------------------------
//
// Sound Mixers
//
// Sound mixers are referenced by name from Soundscapes, and are used to provide
// custom volume control over various sound categories, called 'mix groups'
//
// see scripts/soundmixers.txt for data format
//------------------------------------------------------------------------------

#define CMXRGROUPMAX		64					// up to n mixgroups
#define CMXRGROUPRULESMAX	(CMXRGROUPMAX + 16)	// max number of group rules
#define	CMXRSOUNDMIXERSMAX	32					// up to n sound mixers per project

// mix groups - these equivalent to submixes on an audio mixer

// list of rules for determining sound membership in mix groups.
// All conditions which are not null are ANDed together 
#define CMXRCLASSMAX	16
#define CMXRNAMEMAX		32

struct classlistelem_t
{
	char			szclassname[CMXRNAMEMAX];	// name of entities' class, such as CAI_BaseNPC or CHL2_Player
};


struct grouprule_t
{	
	char			szmixgroup[CMXRNAMEMAX];	// mix group name
	int				mixgroupid;					// mix group unique id
	char			szdir[CMXRNAMEMAX];			// substring to search for in ch->sfx
	int				classId;					// index of classname
	int				chantype;					// channel type (CHAN_WEAPON, etc)
	int				soundlevel_min;				// min soundlevel
	int				soundlevel_max;				// max soundlevel

	int				priority;					// 0..100 higher priority sound groups duck all lower pri groups if enabled
	int				is_ducked;					// if 1, sound group is ducked by all higher priority 'causes_duck" sounds
	int				causes_ducking;				// if 1, sound group ducks other 'is_ducked' sounds of lower priority
	float			duck_target_pct;			// if sound group is ducked, target percent of original volume

	float			total_vol;					// total volume of all sounds in this group, if group can cause ducking
	float			ducker_threshold;			// ducking is caused by this group if total_vol > ducker_threshold
												// and causes_ducking is enabled.
	float			duck_target_vol;			// target volume while ducking	
	float			duck_ramp_val;				// current value of ramp - moves towards duck_target_vol
};

// sound mixer

struct soundmixer_t
{
	char			szsoundmixer[CMXRNAMEMAX];					// name of this soundmixer
	float			mapMixgroupidToValue[CMXRGROUPMAX];			// sparse array of mix group values for this soundmixer
};

int g_mapMixgroupidToGrouprulesid[CMXRGROUPMAX];				// map mixgroupid (one per unique group name)
																// back to 1st entry of this name in g_grouprules

// sound mixer globals

classlistelem_t g_groupclasslist[CMXRCLASSMAX];
soundmixer_t	g_soundmixers[CMXRSOUNDMIXERSMAX];	// all sound mixers	
grouprule_t		g_grouprules[CMXRGROUPRULESMAX];	// all rules for determining mix group membership


// set current soundmixer index g_isoundmixer, search for match in soundmixers
// Only change current soundmixer if new name is different from current name.

int g_isoundmixer = -1;								// index of current sound mixer
char g_szsoundmixer_cur[64];						// current soundmixer name

ConVar snd_soundmixer("snd_soundmixer", "Default_Mix");		// current soundmixer name


void MXR_SetCurrentSoundMixer( const char *szsoundmixer )
{
	// if soundmixer name is not different from current name, return

	if ( !Q_stricmp(szsoundmixer, g_szsoundmixer_cur) )
	{
		return;
	}

	for (int i = 0; i < g_csoundmixers; i++)
	{
		if ( !Q_stricmp(g_soundmixers[i].szsoundmixer, szsoundmixer) )
		{
			g_isoundmixer = i;

			// save new current sound mixer name
			V_strcpy_safe(g_szsoundmixer_cur, szsoundmixer);

			return;
		}
	}
}

ConVar snd_showclassname("snd_showclassname", "0");		// if 1, show classname of ent making sound
														// if 2, show all mixgroup matches
														// if 3, show all mixgroup matches with current soundmixer for ent
// get the client class name if an entity was specified
const char *GetClientClassname( SoundSource soundsource )
{
	IClientEntity *pClientEntity = NULL;
	if ( entitylist )
	{
		pClientEntity = entitylist->GetClientEntity( soundsource );
		if ( pClientEntity )
		{
			ClientClass *pClientClass = pClientEntity->GetClientClass();
			// check npc sounds 
			if ( pClientClass )
			{
				return pClientClass->GetName();
			}
		}
	}

	return NULL;
}

// builds a cached list of rules that match the directory name on the sound
int MXR_GetMixGroupListFromDirName( const char *pDirname, byte *pList, int listMax )
{
	// if we call this before the groups are parsed we'll get bad data
	Assert(g_cgrouprules>0);
	int count = 0;
	for ( int i = 0; i < listMax; i++ )
	{
		pList[i] = 255;
	}

	for ( int i = 0; i < g_cgrouprules; i++ )
	{
		grouprule_t *prule = &g_grouprules[i];
		if ( prule->szdir[ 0 ] && Q_stristr( pDirname, prule->szdir ) )
		{
			pList[count] = i;
			count++;
			if ( count >= listMax )
				return count;
		}
	}
	return count;
}


// determine which mixgroups sound is in, and save those mixgroupids in sound.
// use current soundmixer indicated with g_isoundmixer, and contents of g_rgpgrouprules.
// Algorithm: 
//		1. all conditions in a row are AND conditions, 
//		2. all rows sharing the same groupname are OR conditions.
// so - if a sound matches all conditions of a row, it is given that row's mixgroup id
//		if a sound doesn't match all conditions of a row, the next row is checked.

// returns 0, default mixgroup if no match
void MXR_GetMixGroupFromSoundsource( channel_t *pchan, SoundSource soundsource,  soundlevel_t soundlevel)
{
	int i;
	grouprule_t *prule;
	bool fmatch;
	bool classMatch[CMXRCLASSMAX];

	// init all mixgroups for channel
	for ( i = 0; i < 8; i++ )
	{
		pchan->mixgroups[i] = -1;
	}

	char sndname[MAX_OSPATH];
	Q_strncpy( sndname, pchan->sfx->getname(), sizeof( sndname ) );
	// Use forward slashes here
	Q_FixSlashes( sndname, '/' );
	const char *pszclassname = GetClientClassname(soundsource);

	for ( i = 0; i < g_cgroupclass; i++ )
	{
		classMatch[i] = false;
		if ( pszclassname && Q_stristr(pszclassname, g_groupclasslist[i].szclassname ) )
		{
			classMatch[i] = true;
		}
	}

	if ( snd_showclassname.GetInt() == 1)
	{
		// utility: show classname of ent making sound

		if (pszclassname)
		{
			DevMsg("(%s:%s) \n", pszclassname, sndname);	
		}
	}

	// check all group rules for a match, save
	// up to 8 matches in channel mixgroup.

	int cmixgroups = 0;
	if (!pchan->sfx->m_bMixGroupsCached)
	{
		pchan->sfx->OnNameChanged( pchan->sfx->getname() );
	}
	
	// since this is a sorted list (in group rule order) we only need to test against the next matching rule
	// this avoids a search inside the loop
	int currentDirRuleIndex = 0;
	int currentDirRule = pchan->sfx->m_mixGroupList[0];

	for (i = 0; i < g_cgrouprules; i++)
	{
		prule = &g_grouprules[i];
		fmatch = true;

		// check directory or name substring
#if _DEBUG
		// check dir table is correct in CSfxTable cache
		if ( prule->szdir[ 0 ] && Q_stristr( sndname, prule->szdir ) )
		{
			Assert(currentDirRule == i);
		}
		else
		{
			Assert(currentDirRule != i);
		}
		if ( prule->classId >= 0 )
		{
			// rule has a valid class id and table is correct
			Assert(prule->classId < g_cgroupclass);
			if ( pszclassname && Q_stristr(pszclassname, g_groupclasslist[prule->classId].szclassname) )
			{
				Assert(classMatch[prule->classId] == true);
			}
			else
			{
				Assert(classMatch[prule->classId] == false);
			}
		}
#endif
		// this is the next matching dir for this sound, no need to search
		// becuse the list is sorted and we visit all elements
		if ( currentDirRule == i )
		{
			Assert(prule->szdir[0]);
			currentDirRuleIndex++;
			currentDirRule = 255;
			if ( currentDirRuleIndex < pchan->sfx->m_mixGroupCount )
			{
				currentDirRule = pchan->sfx->m_mixGroupList[currentDirRuleIndex];
			}
		}
		else if ( prule->szdir[ 0 ] )
		{
			fmatch = false;	// substring doesn't match, keep looking
		}

		// check class name

		if ( fmatch && prule->classId >= 0 )
		{
			fmatch = classMatch[prule->classId];
		}

		// check channel type

		if ( fmatch && prule->chantype >= 0)
		{
			if ( pchan->entchannel != prule->chantype  )
				fmatch = false;	// channel type doesn't match, keep looking
		}

		// check sndlvlmin/max

		if ( fmatch && prule->soundlevel_min >= 0)
		{
			if ( soundlevel < prule->soundlevel_min )
				fmatch = false;	// soundlevel is less than min, keep looking
		}

		if ( fmatch && prule->soundlevel_max >= 0)
		{
			if ( soundlevel > prule->soundlevel_max )
				fmatch = false; // soundlevel is greater than max, keep looking
		}

		if ( fmatch )
		{
			pchan->mixgroups[cmixgroups] = prule->mixgroupid;
			cmixgroups++;
			if (cmixgroups >= 8)
				return;		// too many matches, stop looking
		}
		
		if (fmatch && snd_showclassname.GetInt() >= 2)
		{
			// show all mixgroups for this sound
			if (cmixgroups == 1)
			{
				DevMsg("\n%s:%s: ", g_szsoundmixer_cur, sndname);	
			}
			if (prule->szmixgroup[0])
			{
			//	int rgmixgroupid[8];
			//	for (int i = 0; i < 8; i++)
			//		rgmixgroupid[i] = -1;
			//	rgmixgroupid[0] = prule->mixgroupid;
			//	float vol = MXR_GetVolFromMixGroup( rgmixgroupid );
			//	DevMsg("%s(%1.2f) ", prule->szmixgroup, vol);
				DevMsg("%s ", prule->szmixgroup);
			}
		}
	}
}

struct debug_showvols_t
{
	char *psz;			// group name
	int	  mixgroupid;	// groupid
	float vol;			// group volume
	float totalvol;		// total volume of all sounds playing in this group
};


// display routine for MXR_DebugShowMixVolumes

#define MXR_DEBUG_INCY	(1.0/40.0)			// vertical text spacing
#define MXR_DEBUG_GREENSTART 0.3			// start position on screen of bar

#define MXR_DEBUG_MAXVOL		1.0			// max volume scale
#define MXR_DEBUG_REDLIMIT		1.0			// volume limit into yellow
#define MXR_DEBUG_YELLOWLIMIT	0.7			// volume limit into red

#define MXR_DEBUG_VOLSCALE 48				// length of graph in characters
#define MXR_DEBUG_CHAR			'-'			// bar character

extern ConVar dsp_volume;
int g_debug_mxr_displaycount = 0;

void MXR_DebugGraphMixVolumes( debug_showvols_t *groupvols, int cgroups)
{
	float flXpos, flYpos, flXposBar, duration;
	int r,g,b,a;
	int rb, gb, bb, ab;
	flXpos = 0;
	flYpos = 0;
	char text[128];
	char bartext[MXR_DEBUG_VOLSCALE*3];

	duration = 0.01;

	g_debug_mxr_displaycount++;

	if (!(g_debug_mxr_displaycount % 10))
		return;		// only display every 10 frames


	r = 96; g = 86; b = 226; a = 255; ab = 255;
	
	// show volume, dsp_volume

	Q_snprintf( text, 128, "Game Volume: %1.2f", volume.GetFloat());
	CDebugOverlay::AddScreenTextOverlay(flXpos, flYpos, duration, r, g, b,a,  text); 
	flYpos += MXR_DEBUG_INCY;

	Q_snprintf( text, 128, "DSP Volume: %1.2f", dsp_volume.GetFloat());
	CDebugOverlay::AddScreenTextOverlay(flXpos, flYpos, duration, r, g, b,a,  text); 
	flYpos += MXR_DEBUG_INCY;
	
	for (int i = 0; i < cgroups; i++)
	{	
		// r += 64; g += 64; b += 16;

		r = r % 255; g = g % 255; b = b % 255;
		
		Q_snprintf( text, 128, "%s: %1.2f (%1.2f)", groupvols[i].psz, 
					groupvols[i].vol * g_DuckScale, groupvols[i].totalvol * g_DuckScale);

		CDebugOverlay::AddScreenTextOverlay(flXpos, flYpos, duration, r, g, b,a,  text);

		// draw volume bar graph
		
		float vol = (groupvols[i].totalvol * g_DuckScale) / MXR_DEBUG_MAXVOL;
		
		// draw first 70% green
		float vol1 = 0.0;
		float vol2 = 0.0;
		float vol3 = 0.0;
		int cbars;

		vol1 = clamp(vol, 0.0f, 0.7f);
		vol2 = clamp(vol, 0.0f, 0.95f);
		vol3 = vol;

		flXposBar = flXpos + MXR_DEBUG_GREENSTART;

		if (vol1 > 0.0)
		{
			//flXposBar = flXpos + MXR_DEBUG_GREENSTART;

			rb = 0; gb= 255; bb = 0;		// green bar
			Q_memset(bartext, 0, sizeof(bartext));

			cbars = (int)((float)vol1 * (float)MXR_DEBUG_VOLSCALE);
			cbars = clamp(cbars, 0, MXR_DEBUG_VOLSCALE*3-1);
			Q_memset(bartext, MXR_DEBUG_CHAR, cbars);

			CDebugOverlay::AddScreenTextOverlay(flXposBar, flYpos, duration, rb, gb, bb,ab,  bartext);
		}

		
		// yellow bar
		if (vol2 > MXR_DEBUG_YELLOWLIMIT)	
		{
			rb = 255; gb = 255; bb = 0;	
			Q_memset(bartext, 0, sizeof(bartext));

			cbars = (int)((float)vol2 * (float)MXR_DEBUG_VOLSCALE);
			cbars = clamp(cbars, 0, MXR_DEBUG_VOLSCALE*3-1);
			Q_memset(bartext, MXR_DEBUG_CHAR, cbars);

			CDebugOverlay::AddScreenTextOverlay(flXposBar, flYpos, duration, rb, gb, bb,ab,  bartext);
		}

		// red bar
		if (vol3 > MXR_DEBUG_REDLIMIT)
		{
			//flXposBar = flXpos + MXR_DEBUG_REDSTART;
			rb = 255; gb = 0; bb = 0;
			Q_memset(bartext, 0, sizeof(bartext));

			cbars = (int)((float)vol3 * (float)MXR_DEBUG_VOLSCALE);
			cbars = clamp(cbars, 0, MXR_DEBUG_VOLSCALE*3-1);
			Q_memset(bartext, MXR_DEBUG_CHAR, cbars);

			CDebugOverlay::AddScreenTextOverlay(flXposBar, flYpos, duration, rb, gb, bb,ab,  bartext);
		}

		flYpos += MXR_DEBUG_INCY;
	}
}

ConVar snd_disable_mixer_duck("snd_disable_mixer_duck", "0");	// if 1, soundmixer ducking is disabled

// given mix group id, return current duck volume

float MXR_GetDuckVolume( int mixgroupid )
{

	if ( snd_disable_mixer_duck.GetInt() )
		return 1.0;

	Assert ( mixgroupid < g_cgrouprules );

	int	grouprulesid = g_mapMixgroupidToGrouprulesid[mixgroupid];

	// if this mixgroup is not ducked, return 1.0

	if ( !g_grouprules[grouprulesid].is_ducked )
		return 1.0;

	// return current duck value for this group, scaled by current fade in/out ramp

	return g_grouprules[grouprulesid].duck_ramp_val;

}

#define SND_DUCKER_UPDATETIME	0.1		// seconds to wait between ducker updates

double g_mxr_ducktime = 0.0;			// time of last update to ducker

// Get total volume currently playing in all groups,
// process duck volumes for all groups
// Call once per frame - updates occur at 10hz

void MXR_UpdateAllDuckerVolumes( void )
{
	if ( snd_disable_mixer_duck.GetInt() )
		return;

	// check timer since last update, only update at 10hz

	int i;
	double dtime = g_pSoundServices->GetHostTime();
	
	// don't update until timer expires

	if (fabs(dtime - g_mxr_ducktime) < SND_DUCKER_UPDATETIME)
			return;
	
	g_mxr_ducktime = dtime;

	// clear out all total volume values for groups

	for ( i = 0; i < g_cgrouprules; i++)
		g_grouprules[i].total_vol = 0.0;
	
	// for every channel in a mix group which can cause ducking:
	// get total volume, store total in grouprule:
	
	CChannelList list;
	int ch_idx;

	channel_t *pchan;
	bool b_found_ducked_channel = false;

	g_ActiveChannels.GetActiveChannels( list );

	for ( i = 0; i < list.Count(); i++ )
	{
		ch_idx = list.GetChannelIndex(i);
		pchan = &channels[ch_idx];

		if (pchan->last_vol > 0.0)
		{
			// account for all mix groups this channel belongs to...

			for (int j = 0; j < 8; j++)
			{
				int imixgroup = pchan->mixgroups[j];

				if (imixgroup < 0)
					continue;
				
				int	grouprulesid = g_mapMixgroupidToGrouprulesid[imixgroup];
			
				if (g_grouprules[grouprulesid].causes_ducking)
					g_grouprules[grouprulesid].total_vol += pchan->last_vol;

				if (g_grouprules[grouprulesid].is_ducked)
					b_found_ducked_channel = true;
			}
		}		
	}
	
	// if no channels playing which may be ducked, do nothing

	if ( !b_found_ducked_channel )
		return;

	// for all groups that can be ducked:
	// see if a higher priority sound group has a volume > threshold, 
	// if so, then duck this group by setting duck_target_vol to duck_target_pct.
	// if no sound group is causing ducking in this group, reset duck_target_vol to 1.0

	for (i = 0; i < g_cgrouprules; i++)
	{
		if (g_grouprules[i].is_ducked)
		{
			int priority = g_grouprules[i].priority;

			float duck_volume = 1.0;				// clear to 1.0 if no channel causing ducking

			// make sure we interact appropriately with global voice ducking...
			// if global voice ducking is active, skip sound group ducking and just set duck_volume target to 1.0

			if ( g_DuckScale >= 1.0 )
			{	
				// check all sound groups for higher priority duck trigger

				for (int j = 0; j < g_cgrouprules; j++)
				{
					if (g_grouprules[j].priority > priority && 
						g_grouprules[j].causes_ducking &&
						g_grouprules[j].total_vol > g_grouprules[j].ducker_threshold)
					{
						// a higher priority group is causing this group to be ducked
						// set duck volume target to the ducked group's duck target percent
						// and break

						duck_volume = g_grouprules[i].duck_target_pct;
						
						// UNDONE: to prevent edge condition caused by crossing threshold, may need to have secondary
						// UNDONE: timer which allows ducking at 0.2 hz

						break;
					}
				}
			}

			g_grouprules[i].duck_target_vol = duck_volume; 
		}
	}

	// update all ducker ramps if current duck value is not target
	// if ramp is greater than duck_volume, approach at 'attack rate'
	// if ramp is less than duck_volume, approach at 'decay rate'

	for (i = 0; i < g_cgrouprules; i++)
	{
		float target	= g_grouprules[i].duck_target_vol;
		float current	= g_grouprules[i].duck_ramp_val;
			
		if (g_grouprules[i].is_ducked && (current != target))
		{

			float ramptime = target < current ? snd_duckerattacktime.GetFloat() : snd_duckerreleasetime.GetFloat();

			// delta is volume change per update (we can do this 
			// since we run at an approximate fixed update rate of 10hz)

			float delta	= (1.0 - g_grouprules[i].duck_target_pct);
			
			delta *= ( SND_DUCKER_UPDATETIME / ramptime );
			
			if (current > target)
				delta = -delta;

			// update ramps

			current += delta;

			if (current < target && delta < 0)
				current = target;
			if (current > target && delta > 0)
				current = target;

			g_grouprules[i].duck_ramp_val = current;
		}
	}

}

ConVar snd_showmixer("snd_showmixer", "0");	// set to 1 to show mixer every frame

// show the current soundmixer output

void MXR_DebugShowMixVolumes( void )
{
	if (snd_showmixer.GetInt() == 0)
		return;

	// for the current soundmixer:
	// make a totalvolume bucket for each mixgroup type in the soundmixer.
	// for every active channel, add its spatialized volume to 
	// totalvolume bucket for that channel's selected mixgroup
	
	// display all mixgroup/volume/totalvolume values as horizontal bars

	debug_showvols_t groupvols[CMXRGROUPMAX];

	int i;
	int cgroups = 0;

	if (g_isoundmixer < 0)
	{
		DevMsg("No sound mixer selected!");
		return;
	}
	
	soundmixer_t *pmixer = &g_soundmixers[g_isoundmixer];

	// for every entry in mapMixgroupidToValue which is not -1, 
	// set up groupvols

	for (i = 0; i < CMXRGROUPMAX; i++)
	{
		if (pmixer->mapMixgroupidToValue[i] >= 0)
		{
			groupvols[cgroups].mixgroupid = i;
			groupvols[cgroups].psz = MXR_GetGroupnameFromId( i );
			groupvols[cgroups].totalvol = 0.0;
			groupvols[cgroups].vol = pmixer->mapMixgroupidToValue[i];
			cgroups++;
		}
	}

	// for every active channel, get its volume and 
	// the selected mixgroupid, add to groupvols totalvol

	CChannelList list;
	int ch_idx;
	channel_t *pchan;

	g_ActiveChannels.GetActiveChannels( list );

	for ( i = 0; i < list.Count(); i++ )
	{
		ch_idx = list.GetChannelIndex(i);
		pchan = &channels[ch_idx];
		if (pchan->last_vol > 0.0)
		{
			// find entry in groupvols
			for (int j = 0; j < CMXRGROUPMAX; j++)
			{
				if (pchan->last_mixgroupid == groupvols[j].mixgroupid)
				{
					groupvols[j].totalvol += pchan->last_vol;
					break;
				}
			}
		}	
	}

	// groupvols is now fully initialized - just display it

	MXR_DebugGraphMixVolumes( groupvols, cgroups);
}

#ifdef _DEBUG

// set the named mixgroup volume to vol for the current soundmixer
static void MXR_DebugSetMixGroupVolume( const CCommand &args )
{
	if ( args.ArgC() != 3 )
	{
		DevMsg("Parameters: mix group name, volume");
		return;
	}

	const char *szgroupname = args[1];
	float vol = atof( args[2] );

	int imixgroup = MXR_GetMixgroupFromName( szgroupname );

	if ( g_isoundmixer < 0 )
		return;
	
	soundmixer_t *pmixer = &g_soundmixers[g_isoundmixer];

	pmixer->mapMixgroupidToValue[imixgroup] = vol;
}

#endif //_DEBUG

// given array of groupids (ie: the sound is in these groups),
// return a mix volume.

// return first mixgroup id in the provided array
// which maps to a non -1 volume value for this
// sound mixer

float MXR_GetVolFromMixGroup( int rgmixgroupid[8], int *plast_mixgroupid )
{
	
	// if no soundmixer currently set, return 1.0 volume

	if (g_isoundmixer < 0)
	{
		*plast_mixgroupid = 0;
		return 1.0;
	}

	float duckgain = 1.0;

	if (g_csoundmixers)
	{
		soundmixer_t *pmixer = &g_soundmixers[g_isoundmixer];

		if (pmixer)
		{
			// search mixgroupid array, return first match (non -1)

			for (int i = 0; i < 8; i++)
			{
				int imixgroup = rgmixgroupid[i];

				if (imixgroup < 0)
					continue;

				// save lowest duck gain value for any of the mix groups this sound is in

				float duckgain_new = MXR_GetDuckVolume( imixgroup );

				if ( duckgain_new < duckgain)
					duckgain = duckgain_new;

				Assert(imixgroup < CMXRGROUPMAX);
				
				// return first mixgroup id in the passed in array
				// that maps to a non -1 volume value for this
				// sound mixer

				if ( pmixer->mapMixgroupidToValue[imixgroup] >= 0)
				{
					*plast_mixgroupid = imixgroup;
					
					// get gain due to mixer settings

					float gain = pmixer->mapMixgroupidToValue[imixgroup];
	
					// modify gain with ducker settings for this group

					return gain * duckgain;
				}
			}
		}
	}

	*plast_mixgroupid = 0;
	return duckgain;
}

// get id of mixgroup name

int MXR_GetMixgroupFromName( const char *pszgroupname )
{
	// scan group rules for mapping from name to id
	if ( !pszgroupname )
		return -1;
	
	if ( Q_strlen(pszgroupname) == 0 )
		return -1;

	for (int i = 0; i < g_cgrouprules; i++)
	{
		if ( !Q_stricmp(g_grouprules[i].szmixgroup, pszgroupname ) )
			return g_grouprules[i].mixgroupid;
	}	

	return -1;
}

// get mixgroup name from id
char *MXR_GetGroupnameFromId( int mixgroupid)
{
	// scan group rules for mapping from name to id
	if (mixgroupid < 0)
		return NULL;

	for (int i = 0; i < g_cgrouprules; i++)
	{
		if ( g_grouprules[i].mixgroupid == mixgroupid)
			return g_grouprules[i].szmixgroup;
	}	

	return NULL;
}

// assign a unique mixgroup id to each unique named mix group
// within grouprules. Note: all mixgroupids in grouprules must be -1
// when this routine starts.

void MXR_AssignGroupIds( void )
{
	int cmixgroupid = 0;

	for (int i = 0; i < g_cgrouprules; i++)
	{
		int mixgroupid = MXR_GetMixgroupFromName( g_grouprules[i].szmixgroup );

		if (mixgroupid == -1)
		{
			// groupname is not yet assigned, provide a unique mixgroupid.

			g_grouprules[i].mixgroupid = cmixgroupid;

			// save reverse mapping, from mixgroupid to the first grouprules entry for this name

			g_mapMixgroupidToGrouprulesid[cmixgroupid] = i;

			cmixgroupid++;
		}	
	}
}

int MXR_AddClassname( const char *pName )
{
	char szclassname[CMXRNAMEMAX];
	Q_strncpy( szclassname, pName, CMXRNAMEMAX );
	for ( int i = 0; i < g_cgroupclass; i++ )
	{
		if ( !Q_stricmp( szclassname, g_groupclasslist[i].szclassname ) )
			return i;
	}
	if ( g_cgroupclass >= CMXRCLASSMAX )
	{
		Assert(g_cgroupclass < CMXRCLASSMAX);
		return -1;
	}
	Q_memcpy(g_groupclasslist[g_cgroupclass].szclassname, pName, min((size_t)CMXRNAMEMAX-1, strlen(pName)));
	g_cgroupclass++;
	return g_cgroupclass-1;
}

#define CHAR_LEFT_PAREN		'{'
#define CHAR_RIGHT_PAREN	'}'

// load group rules and sound mixers from file

bool MXR_LoadAllSoundMixers( void )
{
	// init soundmixer globals

	g_isoundmixer = -1;
	g_szsoundmixer_cur[0] = 0;

	g_csoundmixers	= 0;					// total number of soundmixers found
	g_cgrouprules	= 0;					// total number of group rules found

	Q_memset(g_soundmixers, 0, sizeof(g_soundmixers));
	Q_memset(g_grouprules, 0, sizeof(g_grouprules));

	// load file

	// build rules

	// build array of sound mixers

	char szFile[MAX_OSPATH];
	const char *pstart;
	bool bResult = false;
	char *pbuffer;

	Q_snprintf( szFile, sizeof( szFile ), "scripts/soundmixers.txt" );

	pbuffer = (char *)COM_LoadFile( szFile, 5, NULL ); // Use malloc - free at end of this routine
	if ( !pbuffer )
	{
		Error( "MXR_LoadAllSoundMixers: unable to open '%s'\n", szFile );
		return bResult;
	}

	pstart = pbuffer;
	
	// first pass: load g_grouprules[]

	// starting at top of file, 
	// scan for first '{', skipping all comment lines
		// get strings for: groupname, directory, classname, chan, sndlvl_min, sndlvl_max
		// convert chan to CHAN_ lookup
		// convert sndlvl_min, sndl_max to ints
		// store all in g_grouprules, update g_cgrouprules;
	// get next line
	// when hit '}' we're done with grouprules

	// check for first CHAR_LEFT_PAREN

	while (1)
	{
		pstart = COM_Parse( pstart );
	
		if ( strlen(com_token) <= 0)
			break; // eof

		if ( com_token[0] != CHAR_LEFT_PAREN )
			continue;

		break;
	}

	while (1)
	{
		pstart = COM_Parse( pstart );
		if (com_token[0] == CHAR_RIGHT_PAREN)
			break;
		
		grouprule_t *pgroup = &g_grouprules[g_cgrouprules];

		// copy mixgroup name, directory, classname
		// if no value specified, set to 0 length string

		if (com_token[0])
			Q_memcpy(pgroup->szmixgroup, com_token, min((size_t)CMXRNAMEMAX-1, strlen(com_token)));
		
		pstart = COM_Parse( pstart );
		if (com_token[0])
			Q_memcpy(pgroup->szdir, com_token, min((size_t)CMXRNAMEMAX-1, strlen(com_token)));

		pgroup->classId = -1;
		pstart = COM_Parse( pstart );
		if (com_token[0])
		{
			pgroup->classId = MXR_AddClassname( com_token );
		}

		// make sure all copied strings are null terminated
		pgroup->szmixgroup[CMXRNAMEMAX-1]	= 0;
		pgroup->szdir[CMXRNAMEMAX-1]		= 0;

		// lookup chan
		pstart = COM_Parse( pstart );
		if (com_token[0])
		{
			if (!Q_stricmp(com_token, "CHAN_STATIC"))
				pgroup->chantype = CHAN_STATIC;
			else if (!Q_stricmp(com_token, "CHAN_WEAPON"))
				pgroup->chantype = CHAN_WEAPON;
			else if (!Q_stricmp(com_token, "CHAN_VOICE"))
				pgroup->chantype = CHAN_VOICE;
			else if (!Q_stricmp(com_token, "CHAN_VOICE2"))
				pgroup->chantype = CHAN_VOICE2;
			else if (!Q_stricmp(com_token, "CHAN_BODY"))
				pgroup->chantype = CHAN_BODY;
			else if (!Q_stricmp(com_token, "CHAN_ITEM"))
				pgroup->chantype = CHAN_ITEM;
		}
		else
			pgroup->chantype = -1;

		// get sndlvls
		
		pstart = COM_Parse( pstart );
		if (com_token[0])
			pgroup->soundlevel_min = atoi(com_token);
		else
			pgroup->soundlevel_min = -1;

		pstart = COM_Parse( pstart );
		if (com_token[0])
			pgroup->soundlevel_max = atoi(com_token);
		else
			pgroup->soundlevel_max = -1;

		// get duck priority, IsDucked, Causes_ducking, duck_target_pct

		pstart = COM_Parse( pstart );
		if (com_token[0])
			pgroup->priority = atoi(com_token);
		else
			pgroup->priority = 50;

		pstart = COM_Parse( pstart );
		if (com_token[0])
			pgroup->is_ducked = atoi(com_token);
		else
			pgroup->is_ducked = 0;

		pstart = COM_Parse( pstart );
		if (com_token[0])
			pgroup->causes_ducking = atoi(com_token);
		else
			pgroup->causes_ducking = 0;

		pstart = COM_Parse( pstart );
		if (com_token[0])
			pgroup->duck_target_pct = ((float)(atoi(com_token))) / 100.0f;
		else
			pgroup->duck_target_pct = 0.5f;

		pstart = COM_Parse( pstart );
		if (com_token[0])
			pgroup->ducker_threshold = ((float)(atoi(com_token))) / 100.0f;
		else
			pgroup->ducker_threshold = 0.5f;

		pgroup->duck_ramp_val = 1.0;
		pgroup->duck_target_vol = 1.0;
		pgroup->total_vol = 0.0;

		// set mixgroup id to -1
		pgroup->mixgroupid = -1;

		// update rule count

		g_cgrouprules++;
		
		if (g_cgrouprules >= CMXRGROUPRULESMAX)
		{
			// UNDONE: error! too many rules
			break;
		}
	}
	
	// now process all groupids in groups, such that
	// each mixgroup gets a unique id.

	MXR_AssignGroupIds();

	// now load g_soundmixers

	// while not at end of file...
		// scan for "<name>", if found save as new soundmixer name
			// while not '}'
			// scan for "<name>", save as groupname
			// scan for "<num>", save as mix value
	
	while(1)
	{
		pstart = COM_Parse( pstart );

		if ( strlen(com_token) <= 0)
			break;	// eof

		// save name in soundmixer

		soundmixer_t *pmixer = &g_soundmixers[g_csoundmixers];

		Q_memcpy(pmixer->szsoundmixer, com_token, min((size_t)CMXRNAMEMAX-1, strlen(com_token)));

		// init all mixer values to -1.

		for (int j = 0; j < CMXRGROUPMAX; j++)
		{
			pmixer->mapMixgroupidToValue[j] = -1.0;
		}

		// load all groupnames for this soundmixer

		while (1)
		{
			pstart = COM_Parse( pstart );

			if (com_token[0] == CHAR_LEFT_PAREN)
				continue;	// skip {

			if (com_token[0] == CHAR_RIGHT_PAREN)
				break;	// finished with this sounmixer
			
			// lookup mixgroupid for groupname
			int mixgroupid = MXR_GetMixgroupFromName( com_token );
			float value;
			
			// get mix value
			pstart = COM_Parse( pstart );
			value = atof( com_token );

			// store value for mixgroupid
			Assert(mixgroupid <= CMXRGROUPMAX);

			pmixer->mapMixgroupidToValue[mixgroupid] = value;
		}

		g_csoundmixers++;
		if (g_csoundmixers >= CMXRSOUNDMIXERSMAX)
		{
			// UNDONE: error! to many sound mixers
			break;
		}
	}

	bResult = true;

// loadmxr_exit:
	free( pbuffer );
	return bResult;
}

void MXR_ReleaseMemory( void )
{
	// free all resources
}

float S_GetMono16Samples( const char *pszName, CUtlVector< short >& sampleList )
{
	CSfxTable *pSfx = S_PrecacheSound( PSkipSoundChars( pszName ) );
	if ( !pSfx )
		return 0.0f;

	CAudioSource *pWave = pSfx->pSource;
	if ( !pWave )
		return 0.0f;

	int nType = pWave->GetType();
	if ( nType != CAudioSource::AUDIO_SOURCE_WAV )
		return 0.0f;

	CAudioMixer *pMixer = pWave->CreateMixer();
	if ( !pMixer )
		return 0.0f;

	float duration = AudioSource_GetSoundDuration( pSfx );

	// Determine start/stop positions
	int totalsamples = (int)( duration * pWave->SampleRate() );
	if ( totalsamples <= 0 )
		return 0;

	bool bStereo = pWave->IsStereoWav();
	int mix_sample_size = pMixer->GetMixSampleSize();
	int nNumChannels = bStereo ? 2 : 1;

	char *pData = NULL;

	int pos = 0;
	int remaining = totalsamples;
	while ( remaining > 0 )
	{
		int blockSize = min( remaining, 1000 );

		char copyBuf[AUDIOSOURCE_COPYBUF_SIZE];
		int copied = pWave->GetOutputData( (void **)&pData, pos, blockSize, copyBuf );
		if ( !copied )
		{
			break;
		}

		remaining -= copied;
		pos += copied;

		// Now get samples out of output data
		switch ( nNumChannels )
		{
		default:
		case 1:
			{
				for ( int i = 0; i < copied; ++i )
				{
					int offset = i * mix_sample_size;

					short sample = 0;
					if ( mix_sample_size == 1 )
					{
						char s = *( char * )( pData + offset );
						// Upscale it to fit into a short
						sample = s << 8;
					}
					else if ( mix_sample_size == 2 )
					{
						sample = *( short * )( pData + offset );
					}
					else if ( mix_sample_size == 4 )
					{
						// Not likely to have 4 bytes mono!!!
						Assert( 0 );

						int s = *( int * )( pData + offset );
						sample = s >> 16;
					}
					else
					{
						Assert( 0 );
					}

					sampleList.AddToTail( sample );
				}
			}
			break;

		case 2:
			{
				for ( int i = 0; i < copied; ++i )
				{
					int offset = i * mix_sample_size;

					short left = 0;
					short right = 0;
					
					if ( mix_sample_size == 1 )
					{
						// Not possible!!!, must be at least 2 bytes!!!
						Assert( 0 );

						char v = *( char * )( pData + offset );
						left = right = ( v << 8 );
					}
					else if ( mix_sample_size == 2 )
					{
						// One byte per channel
						left  = (short)( ( *(char *)( pData + offset ) ) << 8 );
						right = (short)( ( *(char *)( pData + offset + 1 ) ) << 8 );
					}
					else if ( mix_sample_size == 4 )
					{
						// 2 bytes per channel
						left = *( short * )( pData + offset );
						right = *( short * )( pData + offset + 2 );
					}
					else
					{
						Assert( 0 );
					}

					short sample = ( left + right ) >> 1;
					sampleList.AddToTail( sample );
				}
			}
			break;
		}
	}

	delete pMixer;

	return duration;
}
