//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "audio_pch.h"
#include "circularbuffer.h"
#include "voice.h"
#include "voice_wavefile.h"
#include "r_efx.h"
#include "cdll_int.h"
#include "voice_gain.h"
#include "voice_mixer_controls.h"

#include "ivoicerecord.h"
#include "ivoicecodec.h"
#include "filesystem.h"
#include "../../filesystem_engine.h"
#include "tier1/utlbuffer.h"
#if defined( _X360 )
#include "xauddefs.h"
#endif

#include "steam/steam_api.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CSteamAPIContext g_SteamAPIContext;
static CSteamAPIContext *steamapicontext = NULL;

void Voice_EndChannel( int iChannel );
void VoiceTweak_EndVoiceTweakMode();
void EngineTool_OverrideSampleRate( int& rate );

// A fallback codec that should be the most likely to work for local/offline use
#define VOICE_FALLBACK_CODEC	"vaudio_celt"

// Special entity index used for tweak mode.
#define TWEAKMODE_ENTITYINDEX				-500

// Special channel index passed to Voice_AddIncomingData when in tweak mode.
#define TWEAKMODE_CHANNELINDEX				-100


// How long does the sign stay above someone's head when they talk?
#define SPARK_TIME		0.2

// How long a voice channel has to be inactive before we free it.
#define DIE_COUNTDOWN	0.5

#define VOICE_RECEIVE_BUFFER_SIZE			(VOICE_OUTPUT_SAMPLE_RATE_MAX * BYTES_PER_SAMPLE)

#define LOCALPLAYERTALKING_TIMEOUT			0.2f	// How long it takes for the client to decide the server isn't sending acks
													// of voice data back.

// If this is defined, then the data is converted to 8-bit and sent otherwise uncompressed.
// #define VOICE_SEND_RAW_TEST

// The format we sample voice in.
WAVEFORMATEX g_VoiceSampleFormat =
{
	WAVE_FORMAT_PCM,		// wFormatTag
	1,						// nChannels
	// These two can be dynamically changed by voice_init
	VOICE_OUTPUT_SAMPLE_RATE_LOW,				// nSamplesPerSec
	VOICE_OUTPUT_SAMPLE_RATE_LOW*2,				// nAvgBytesPerSec
	2,						// nBlockAlign
	16,						// wBitsPerSample
	sizeof(WAVEFORMATEX)	// cbSize
};

static bool Voice_SetSampleRate( DWORD rate )
{
	if ( g_VoiceSampleFormat.nSamplesPerSec != rate ||
	     g_VoiceSampleFormat.nAvgBytesPerSec != rate * 2 )
	{
		g_VoiceSampleFormat.nSamplesPerSec = rate;
		g_VoiceSampleFormat.nAvgBytesPerSec = rate * 2;
		return true;
	}

	return false;
}

int Voice_SamplesPerSec()
{
	int rate = g_VoiceSampleFormat.nSamplesPerSec;
	EngineTool_OverrideSampleRate( rate );
	return rate;
}

int Voice_AvgBytesPerSec()
{
	int rate = g_VoiceSampleFormat.nSamplesPerSec;
	EngineTool_OverrideSampleRate( rate );
	return ( rate * g_VoiceSampleFormat.wBitsPerSample ) >> 3;
}

ConVar voice_avggain( "voice_avggain", "0.5" );
ConVar voice_maxgain( "voice_maxgain", "10" );
ConVar voice_scale( "voice_scale", "1", FCVAR_ARCHIVE );

ConVar voice_loopback( "voice_loopback", "0", FCVAR_USERINFO );
ConVar voice_fadeouttime( "voice_fadeouttime", "0.1" );	// It fades to no sound at the tail end of your voice data when you release the key.

// Debugging cvars.
ConVar voice_profile( "voice_profile", "0" );
ConVar voice_showchannels( "voice_showchannels", "0" );	// 1 = list channels
															// 2 = show timing info, etc
ConVar voice_showincoming( "voice_showincoming", "0" );	// show incoming voice data

ConVar voice_enable( "voice_enable", "1", FCVAR_ARCHIVE );		// Globally enable or disable voice.
#ifdef VOICE_VOX_ENABLE
ConVar voice_threshold( "voice_threshold", "2000", FCVAR_ARCHIVE );
#endif // VOICE_VOX_ENABLE

// Have it force your mixer control settings so waveIn comes from the microphone. 
// CD rippers change your waveIn to come from the CD drive
ConVar voice_forcemicrecord( "voice_forcemicrecord", "1", FCVAR_ARCHIVE );

// This should not be lower than the maximum difference between clients' frame durations (due to cmdrate/updaterate),
// plus some jitter allowance.
ConVar voice_buffer_ms( "voice_buffer_ms", "100", FCVAR_INTERNAL_USE,
                        "How many milliseconds of voice to buffer to avoid dropouts due to jitter and frame time differences." );

int g_nVoiceFadeSamples		= 1;							// Calculated each frame from the cvar.
float g_VoiceFadeMul		= 1;							// 1 / (g_nVoiceFadeSamples - 1).

// While in tweak mode, you can't hear anything anyone else is saying, and your own voice data
// goes directly to the speakers.
bool g_bInTweakMode = false;
int g_VoiceTweakSpeakingVolume = 0;

bool g_bVoiceAtLeastPartiallyInitted = false;

// The codec and sample rate passed to Voice_Init. "" and -1 if voice is not initialized
char g_szVoiceCodec[_MAX_PATH] = { 0 };
int g_nVoiceRequestedSampleRate = -1;

const char *Voice_ConfiguredCodec() { return g_szVoiceCodec; }
int Voice_ConfiguredSampleRate() { return g_nVoiceRequestedSampleRate; }

// Timing info for each frame.
static double	g_CompressTime = 0;
static double	g_DecompressTime = 0;
static double	g_GainTime = 0;
static double	g_UpsampleTime = 0;

class CVoiceTimer
{
public:
	inline void		Start()
	{
		if( voice_profile.GetInt() )
		{
			m_StartTime = Plat_FloatTime();
		}
	}
	
	inline void		End(double *out)
	{
		if( voice_profile.GetInt() )
		{
			*out += Plat_FloatTime() - m_StartTime;
		}
	}

	double			m_StartTime;
};


static bool		g_bLocalPlayerTalkingAck = false;
static float	g_LocalPlayerTalkingTimeout = 0;


CSysModule *g_hVoiceCodecDLL = 0;

// Voice recorder. Can be waveIn, DSound, or whatever.
static IVoiceRecord *g_pVoiceRecord = NULL;
static IVoiceCodec  *g_pEncodeCodec = NULL;

static bool			g_bVoiceRecording = false;	// Are we recording at the moment?
static bool			g_bVoiceRecordStopping = false;	// Are we waiting to stop?
bool				g_bUsingSteamVoice = false;

#ifdef WIN32
extern IVoiceRecord* CreateVoiceRecord_DSound(int nSamplesPerSec);
#elif defined( OSX )
extern IVoiceRecord* CreateVoiceRecord_AudioQueue(int sampleRate);
#endif

#ifdef POSIX
extern IVoiceRecord* CreateVoiceRecord_OpenAL(int sampleRate);
#endif


static bool VoiceRecord_Start()
{
	if ( g_bUsingSteamVoice )
	{
		if ( steamapicontext && steamapicontext->SteamUser() )
		{
			steamapicontext->SteamUser()->StartVoiceRecording();
			return true;
		}
	}
	else if ( g_pVoiceRecord )
	{
		return g_pVoiceRecord->RecordStart();
	}
	return false;
}

static void VoiceRecord_Stop()
{
	if ( g_bUsingSteamVoice )
	{
		if ( steamapicontext && steamapicontext->SteamUser() )
		{
			steamapicontext->SteamUser()->StopVoiceRecording();
		}
	}
	else if ( g_pVoiceRecord )
	{
		return g_pVoiceRecord->RecordStop();
	}
}

//
// Used for storing incoming voice data from an entity.
//
class CVoiceChannel
{
public:
									CVoiceChannel();

	// Called when someone speaks and a new voice channel is setup to hold the data.
	void							Init(int nEntity);

public:
	int								m_iEntity;		// Number of the entity speaking on this channel (index into cl_entities).
													// This is -1 when the channel is unused.

	
	CSizedCircularBuffer
		<VOICE_RECEIVE_BUFFER_SIZE>	m_Buffer;		// Circular buffer containing the voice data.

	// Used for upsampling..
	double							m_LastFraction;	// Fraction between m_LastSample and the next sample.
	short							m_LastSample;
	
	bool							m_bStarved;		// Set to true when the channel runs out of data for the mixer.
													// The channel is killed at that point.

	float							m_TimePad;		// Set to TIME_PADDING when the first voice packet comes in from a sender.
													// We add time padding (for frametime differences)
													// by waiting a certain amount of time before starting to output the sound.

	IVoiceCodec						*m_pVoiceCodec;	// Each channel gets is own IVoiceCodec instance so the codec can maintain state.

	CAutoGain						m_AutoGain;		// Gain we're applying to this channel

	CVoiceChannel					*m_pNext;
	
	bool							m_bProximity;
	int								m_nViewEntityIndex;
	int								m_nSoundGuid;
};


CVoiceChannel::CVoiceChannel()
{
	m_iEntity = -1;
	m_pVoiceCodec = NULL;
	m_nViewEntityIndex = -1;
	m_nSoundGuid = -1;
}

void CVoiceChannel::Init(int nEntity)
{
	m_iEntity = nEntity;
	m_bStarved = false;
	m_Buffer.Flush();
	m_TimePad = Clamp( voice_buffer_ms.GetFloat(), 1.f, 5000.f ) / 1000.f;
	m_LastSample = 0;
	m_LastFraction = 0.999;

	m_AutoGain.Reset( 128, voice_maxgain.GetFloat(), voice_avggain.GetFloat(), voice_scale.GetFloat() );
}



// Incoming voice channels.
CVoiceChannel g_VoiceChannels[VOICE_NUM_CHANNELS];


// These are used for recording the wave data into files for debugging.
#define MAX_WAVEFILEDATA_LEN	1024*1024
char *g_pUncompressedFileData = NULL;
int g_nUncompressedDataBytes = 0;
const char *g_pUncompressedDataFilename = NULL;

char *g_pDecompressedFileData = NULL;
int g_nDecompressedDataBytes = 0;
const char *g_pDecompressedDataFilename = NULL;

char *g_pMicInputFileData = NULL;
int g_nMicInputFileBytes = 0;
int g_CurMicInputFileByte = 0;
double g_MicStartTime;

static ConVar voice_writevoices( "voice_writevoices", "0", 0, "Saves each speaker's voice data into separate .wav files\n" );
class CVoiceWriterData
{
public:
	CVoiceWriterData() :
		m_pChannel( NULL ),
		m_nCount( 0 ),
		m_Buffer()
	{
	}

	// Copy ctor is needed to insert into CVoiceWriter's CUtlRBTree.
	CVoiceWriterData(const CVoiceWriterData& other) :
		m_pChannel( other.m_pChannel ),
		m_nCount( other.m_nCount ),
		m_Buffer( )
	{
		m_Buffer.CopyBuffer( other.m_Buffer );
	}

	static bool Less( const CVoiceWriterData &lhs, const CVoiceWriterData &rhs )
	{
		return lhs.m_pChannel < rhs.m_pChannel;
	}

	CVoiceChannel	*m_pChannel;
	int				m_nCount;
	CUtlBuffer		m_Buffer;

private:
	CVoiceWriterData& operator=(const CVoiceWriterData&);
};

class CVoiceWriter
{
public:
	CVoiceWriter() :
		m_VoiceWriter( 0, 0, CVoiceWriterData::Less )
	{
	}

	void Flush()
	{
		for ( int i = m_VoiceWriter.FirstInorder(); i != m_VoiceWriter.InvalidIndex(); i = m_VoiceWriter.NextInorder( i ) )
		{
			CVoiceWriterData *data = &m_VoiceWriter[ i ];

			if ( data->m_Buffer.TellPut() <= 0 )
				continue;
			data->m_Buffer.Purge();
		}
	}

	void Finish()
	{
		if ( !g_pSoundServices->IsConnected() )
		{
			Flush();
			return;
		}

		for ( int i = m_VoiceWriter.FirstInorder(); i != m_VoiceWriter.InvalidIndex(); i = m_VoiceWriter.NextInorder( i ) )
		{
			CVoiceWriterData *data = &m_VoiceWriter[ i ];
			
			if ( data->m_Buffer.TellPut() <= 0 )
				continue;

			int index = data->m_pChannel - g_VoiceChannels;
			Assert( index >= 0 && index < (int)ARRAYSIZE( g_VoiceChannels ) );

			char path[ MAX_PATH ];
			Q_snprintf( path, sizeof( path ), "%s/voice", g_pSoundServices->GetGameDir() );
			g_pFileSystem->CreateDirHierarchy( path );

			char fn[ MAX_PATH ];
			Q_snprintf( fn, sizeof( fn ), "%s/pl%02d_slot%d-time%d.wav", path, index, data->m_nCount, (int)g_pSoundServices->GetClientTime() );

			WriteWaveFile( fn, (const char *)data->m_Buffer.Base(), data->m_Buffer.TellPut(), g_VoiceSampleFormat.wBitsPerSample, g_VoiceSampleFormat.nChannels, Voice_SamplesPerSec() );

			Msg( "Writing file %s\n", fn );

			++data->m_nCount;
			data->m_Buffer.Purge();
		}
	}


	void AddDecompressedData( CVoiceChannel *ch, const byte *data, size_t datalen )
	{
		if ( !voice_writevoices.GetBool() )
			return;

		CVoiceWriterData search;
		search.m_pChannel = ch;
		int idx = m_VoiceWriter.Find( search ); 
		if ( idx == m_VoiceWriter.InvalidIndex() )
		{
			idx = m_VoiceWriter.Insert( search );
		}

		CVoiceWriterData *slot = &m_VoiceWriter[ idx ];
		slot->m_Buffer.Put( data, datalen );
	}
private:

	CUtlRBTree< CVoiceWriterData > m_VoiceWriter;
};

static CVoiceWriter g_VoiceWriter;

inline void ApplyFadeToSamples(short *pSamples, int nSamples, int fadeOffset, float fadeMul)
{
	for(int i=0; i < nSamples; i++)
	{
		float percent = (i+fadeOffset) * fadeMul;
		pSamples[i] = (short)(pSamples[i] * (1 - percent));
	}
}


bool Voice_Enabled( void )
{
	return voice_enable.GetBool();
}


int Voice_GetOutputData(
	const int iChannel,			//! The voice channel it wants samples from.
	char *copyBufBytes,			//! The buffer to copy the samples into.
	const int copyBufSize,		//! Maximum size of copyBuf.
	const int samplePosition,	//! Which sample to start at.
	const int sampleCount		//! How many samples to get.
)
{
	CVoiceChannel *pChannel = &g_VoiceChannels[iChannel];	
	short *pCopyBuf = (short*)copyBufBytes;


	int maxOutSamples = copyBufSize / BYTES_PER_SAMPLE;

	// Find out how much we want and get it from the received data channel.	
	CCircularBuffer *pBuffer = &pChannel->m_Buffer;
	int nBytesToRead = pBuffer->GetReadAvailable();
	nBytesToRead = min(min(nBytesToRead, (int)maxOutSamples), sampleCount * BYTES_PER_SAMPLE);
	int nSamplesGotten = pBuffer->Read(pCopyBuf, nBytesToRead) / BYTES_PER_SAMPLE;

	// Are we at the end of the buffer's data? If so, fade data to silence so it doesn't clip.
	int readSamplesAvail = pBuffer->GetReadAvailable() / BYTES_PER_SAMPLE;
	if(readSamplesAvail < g_nVoiceFadeSamples)
	{
		int bufferFadeOffset = max((readSamplesAvail + nSamplesGotten) - g_nVoiceFadeSamples, 0);
		int globalFadeOffset = max(g_nVoiceFadeSamples - (readSamplesAvail + nSamplesGotten), 0);
		
		ApplyFadeToSamples(
			&pCopyBuf[bufferFadeOffset], 
			nSamplesGotten - bufferFadeOffset,
			globalFadeOffset,
			g_VoiceFadeMul);
	}
	
	// If there weren't enough samples in the received data channel, 
	//   pad it with a copy of the most recent data, and if there 
	//   isn't any, then use zeros.
	if ( nSamplesGotten < sampleCount )
	{
		int wantedSampleCount = min( sampleCount, maxOutSamples );
		int nAdditionalNeeded = (wantedSampleCount - nSamplesGotten);
		if ( nSamplesGotten > 0 )
		{
			short *dest = (short *)&pCopyBuf[ nSamplesGotten ];
			int nSamplesToDuplicate = min( nSamplesGotten, nAdditionalNeeded );
			const short *src = (short *)&pCopyBuf[ nSamplesGotten - nSamplesToDuplicate ];

			Q_memcpy( dest, src, nSamplesToDuplicate * BYTES_PER_SAMPLE );

			//Msg( "duplicating %d samples\n", nSamplesToDuplicate );

			nAdditionalNeeded -= nSamplesToDuplicate;
			if ( nAdditionalNeeded > 0  )
			{
				dest = (short *)&pCopyBuf[ nSamplesGotten + nSamplesToDuplicate ];
				Q_memset(dest, 0, nAdditionalNeeded * BYTES_PER_SAMPLE);

				// Msg( "zeroing %d samples\n", nAdditionalNeeded );

				Assert( ( nAdditionalNeeded + nSamplesGotten + nSamplesToDuplicate ) == wantedSampleCount );
			}
		}
		else
		{
			Q_memset( &pCopyBuf[ nSamplesGotten ], 0, nAdditionalNeeded * BYTES_PER_SAMPLE );
		}
		nSamplesGotten = wantedSampleCount;
	}

	// If the buffer is out of data, mark this channel to go away.
	if(pBuffer->GetReadAvailable() == 0)
	{
		pChannel->m_bStarved = true;
	}

	if(voice_showchannels.GetInt() >= 2)
	{
		Msg("Voice - mixed %d samples from channel %d\n", nSamplesGotten, iChannel);
	}

	VoiceSE_MoveMouth(pChannel->m_iEntity, (short*)copyBufBytes, nSamplesGotten);
	return nSamplesGotten;
}


void Voice_OnAudioSourceShutdown( int iChannel )
{
	Voice_EndChannel( iChannel );
}


// ------------------------------------------------------------------------ //
// Internal stuff.
// ------------------------------------------------------------------------ //

CVoiceChannel* GetVoiceChannel(int iChannel, bool bAssert=true)
{
	if(iChannel < 0 || iChannel >= VOICE_NUM_CHANNELS)
	{
		if(bAssert)
		{
			Assert(false);
		}
		return NULL;
	}
	else
	{
		return &g_VoiceChannels[iChannel];
	}
}

// Helper for doing a default-init with some codec if we weren't passed specific parameters
bool Voice_InitWithDefault( const char *pCodecName )
{
	if ( !pCodecName || !*pCodecName )
	{
		return false;
	}

	int nRate = Voice_GetDefaultSampleRate( pCodecName );
	if ( nRate < 0 )
	{
		Msg( "Voice_InitWithDefault: Unable to determine defaults for codec \"%s\"\n", pCodecName );
		return false;
	}

	return Voice_Init( pCodecName, Voice_GetDefaultSampleRate( pCodecName ) );
}

bool Voice_Init( const char *pCodecName, int nSampleRate )
{
	if ( voice_enable.GetInt() == 0 )
	{
		return false;
	}

	if ( !pCodecName || !pCodecName[0] )
	{
		return false;
	}

	bool bSpeex = Q_stricmp( pCodecName, "vaudio_speex" ) == 0;
	bool bCelt  = Q_stricmp( pCodecName, "vaudio_celt" )  == 0;
	bool bSteam = Q_stricmp( pCodecName, "steam" )        == 0;
	// Miles has not been in use for voice in a long long time.  Not worth the surface to support ancient demos that may
	// use it (and probably do not work for other reasons)
	// "vaudio_miles"

	if ( !( bSpeex || bCelt || bSteam ) )
	{
		Msg( "Voice_Init Failed: invalid voice codec %s.\n", pCodecName );
		return false;
	}

	Voice_Deinit();

	g_bVoiceAtLeastPartiallyInitted = true;
	V_strncpy( g_szVoiceCodec, pCodecName, sizeof(g_szVoiceCodec) );
	g_nVoiceRequestedSampleRate = nSampleRate;

	g_bUsingSteamVoice = bSteam;

	if ( !steamapicontext )
	{
		steamapicontext = &g_SteamAPIContext;
		steamapicontext->Init();
	}

	if ( g_bUsingSteamVoice )
	{
		if ( !steamapicontext->SteamFriends() || !steamapicontext->SteamUser() )
		{
			Msg( "Voice_Init: Requested Steam voice, but cannot access API.  Voice will not function\n" );
			return false;
		}
	}

	// For steam, nSampleRate 0 means "use optimal steam sample rate".
	if ( bSteam && nSampleRate == 0 )
	{
		Msg( "Voice_Init: Using Steam voice optimal sample rate %d\n",
		     steamapicontext->SteamUser()->GetVoiceOptimalSampleRate() );
		// Steam's sample rate may change and not be supported by our rather unflexible sound engine. However, steam
		// will resample as necessary in DecompressVoice, so we can pretend we're outputting at native rates.
		//
		// Behind the scenes, we'll request steam give us the encoded stream at its "optimal" rate, then we'll try to
		// decompress the output at this rate, making it transparent to us that the encoded stream is not at our output
		// rate.
		Voice_SetSampleRate( SOUND_DMA_SPEED );
	}
	else
	{
		Voice_SetSampleRate( nSampleRate );
	}

	if(!VoiceSE_Init())
		return false;

	// Get the voice input device.
#ifdef OSX
	g_pVoiceRecord = CreateVoiceRecord_AudioQueue( Voice_SamplesPerSec() );
	if ( !g_pVoiceRecord )
	{
		// Fall back to OpenAL
		g_pVoiceRecord = CreateVoiceRecord_OpenAL( Voice_SamplesPerSec() );
	}
#elif defined( WIN32 )
	g_pVoiceRecord = CreateVoiceRecord_DSound( Voice_SamplesPerSec() );
#else
	g_pVoiceRecord = CreateVoiceRecord_OpenAL( Voice_SamplesPerSec() );
#endif

	if( !g_pVoiceRecord )
	{
		Msg( "Unable to initialize sound capture. You won't be able to speak to other players." );
	}

	// Init codec DLL for non-steam
	if ( !bSteam )
	{
		// CELT's qualities are 0-3, we historically just passed 4 to the other two even though they don't really map to the
		// same thing.
		//
		// Changing the quality level we use here will require either extending SVC_VoiceInit to pass down which quality is
		// in use or using a different codec name (vaudio_celtHD!) for backwards compatibility
		int quality = bCelt ? 3 : 4;

		// Get the codec.
		CreateInterfaceFn createCodecFn = NULL;
		g_hVoiceCodecDLL = FileSystem_LoadModule(pCodecName);

		if ( !g_hVoiceCodecDLL || (createCodecFn = Sys_GetFactory(g_hVoiceCodecDLL)) == NULL ||
		     (g_pEncodeCodec = (IVoiceCodec*)createCodecFn(pCodecName, NULL)) == NULL || !g_pEncodeCodec->Init( quality ) )
		{
			Msg("Unable to load voice codec '%s'. Voice disabled. (module %i, iface %i, codec %i)\n",
			    pCodecName, !!g_hVoiceCodecDLL, !!createCodecFn, !!g_pEncodeCodec);
			Voice_Deinit();
			return false;
		}

		for (int i=0; i < VOICE_NUM_CHANNELS; i++)
		{
			CVoiceChannel *pChannel = &g_VoiceChannels[i];

			if ((pChannel->m_pVoiceCodec = (IVoiceCodec*)createCodecFn(pCodecName, NULL)) == NULL || !pChannel->m_pVoiceCodec->Init( quality ))
			{
				Voice_Deinit();
				return false;
			}
		}
	}

	// XXX(JohnS): These don't do much in Steam codec mode, but code below uses their presence to mean 'voice fully
	//             initialized' and other things assume they will succeed.
	InitMixerControls();

	// Steam mode uses steam for raw input so this isn't meaningful and could have side-effects
	if( voice_forcemicrecord.GetInt() && !bSteam )
	{
		if( g_pMixerControls )
			g_pMixerControls->SelectMicrophoneForWaveInput();
	}

	return true;
}


void Voice_EndChannel(int iChannel)
{
	Assert(iChannel >= 0 && iChannel < VOICE_NUM_CHANNELS);

	CVoiceChannel *pChannel = &g_VoiceChannels[iChannel];
	
	if ( pChannel->m_iEntity != -1 )
	{
		int iEnt = pChannel->m_iEntity;
		pChannel->m_iEntity = -1;

		if ( pChannel->m_bProximity == true )
		{
			VoiceSE_EndChannel( iChannel, iEnt );
		}
		else
		{
			VoiceSE_EndChannel( iChannel, pChannel->m_nViewEntityIndex );
		}
		
		g_pSoundServices->OnChangeVoiceStatus( iEnt, false );
		VoiceSE_CloseMouth( iEnt );

		pChannel->m_nViewEntityIndex = -1;
		pChannel->m_nSoundGuid = -1;

		// If the tweak mode channel is ending
		if ( iChannel == 0 && 
			g_bInTweakMode )
		{
			VoiceTweak_EndVoiceTweakMode();
		}
	}
}


void Voice_EndAllChannels()
{
	for(int i=0; i < VOICE_NUM_CHANNELS; i++)
	{
		Voice_EndChannel(i);
	}
}

bool EngineTool_SuppressDeInit();

void Voice_Deinit()
{
	// This call tends to be expensive and when voice is not enabled it will continually
	// call in here, so avoid the work if possible.
	if( !g_bVoiceAtLeastPartiallyInitted )
		return;

	if ( EngineTool_SuppressDeInit() )
		return;

	Voice_EndAllChannels();

	Voice_RecordStop();

	for(int i=0; i < VOICE_NUM_CHANNELS; i++)
	{
		CVoiceChannel *pChannel = &g_VoiceChannels[i];
		
		if ( pChannel->m_pVoiceCodec )
		{
			pChannel->m_pVoiceCodec->Release();
			pChannel->m_pVoiceCodec = NULL;
		}
	}

	if( g_pEncodeCodec )
	{
		g_pEncodeCodec->Release();
		g_pEncodeCodec = NULL;
	}

	if(g_hVoiceCodecDLL)
	{
		FileSystem_UnloadModule(g_hVoiceCodecDLL);
		g_hVoiceCodecDLL = NULL;
	}

	if(g_pVoiceRecord)
	{
		g_pVoiceRecord->Release();
		g_pVoiceRecord = NULL;
	}

	VoiceSE_Term();

	g_bVoiceAtLeastPartiallyInitted = false;
	g_szVoiceCodec[0] = '\0';
	g_nVoiceRequestedSampleRate = -1;
	g_bUsingSteamVoice = false;
}

bool Voice_GetLoopback()
{
	return !!voice_loopback.GetInt();
}


void Voice_LocalPlayerTalkingAck()
{
	if(!g_bLocalPlayerTalkingAck)
	{
		// Tell the client DLL when this changes.
		g_pSoundServices->OnChangeVoiceStatus(-2, TRUE);
	}

	g_bLocalPlayerTalkingAck = true;
	g_LocalPlayerTalkingTimeout = 0;
}


void Voice_UpdateVoiceTweakMode()
{
	if(!g_bInTweakMode || !g_pVoiceRecord)
		return;

	CVoiceChannel *pTweakChannel = GetVoiceChannel( 0 );

	if ( pTweakChannel->m_nSoundGuid != -1 &&
		!S_IsSoundStillPlaying( pTweakChannel->m_nSoundGuid ) )
	{
		VoiceTweak_EndVoiceTweakMode();
		return;
	}

	char uchVoiceData[4096];
	bool bFinal = false;
	int nDataLength = Voice_GetCompressedData(uchVoiceData, sizeof(uchVoiceData), bFinal);

	Voice_AddIncomingData(TWEAKMODE_CHANNELINDEX, uchVoiceData, nDataLength, 0);
}


void Voice_Idle(float frametime)
{
	if( voice_enable.GetInt() == 0 )
	{
		Voice_Deinit();
		return;
	}

	if( g_bLocalPlayerTalkingAck )
	{
		g_LocalPlayerTalkingTimeout += frametime;
		if(g_LocalPlayerTalkingTimeout > LOCALPLAYERTALKING_TIMEOUT)
		{
			g_bLocalPlayerTalkingAck = false;
			
			// Tell the client DLL.
			g_pSoundServices->OnChangeVoiceStatus(-2, FALSE);
		}
	}

	// Precalculate these to speedup the voice fadeout.
	g_nVoiceFadeSamples = max((int)(voice_fadeouttime.GetFloat() * g_VoiceSampleFormat.nSamplesPerSec ), 2);
	g_VoiceFadeMul = 1.0f / (g_nVoiceFadeSamples - 1);

	if(g_pVoiceRecord)
		g_pVoiceRecord->Idle();

	// If we're in voice tweak mode, feed our own data back to us.
	Voice_UpdateVoiceTweakMode();

	// Age the channels.
	int nActive = 0;
	for(int i=0; i < VOICE_NUM_CHANNELS; i++)
	{
		CVoiceChannel *pChannel = &g_VoiceChannels[i];
		
		if(pChannel->m_iEntity != -1)
		{
			if(pChannel->m_bStarved)
			{
				// Kill the channel. It's done playing.
				Voice_EndChannel(i);
				pChannel->m_nSoundGuid = -1;
			}
			else
			{
				float oldpad = pChannel->m_TimePad;
				pChannel->m_TimePad -= frametime;
				if(oldpad > 0 && pChannel->m_TimePad <= 0)
				{
					// Start its audio.
					pChannel->m_nViewEntityIndex = g_pSoundServices->GetViewEntity();
					pChannel->m_nSoundGuid = VoiceSE_StartChannel( i, pChannel->m_iEntity, pChannel->m_bProximity, pChannel->m_nViewEntityIndex );
					g_pSoundServices->OnChangeVoiceStatus(pChannel->m_iEntity, TRUE);
					
					VoiceSE_InitMouth(pChannel->m_iEntity);
				}

				++nActive;
			}
		}
	}

	if(nActive == 0)
		VoiceSE_EndOverdrive();

	VoiceSE_Idle(frametime);

	// voice_showchannels.
	if( voice_showchannels.GetInt() >= 1 )
	{
		for(int i=0; i < VOICE_NUM_CHANNELS; i++)
		{
			CVoiceChannel *pChannel = &g_VoiceChannels[i];
			
			if(pChannel->m_iEntity == -1)
				continue;

			Msg("Voice - chan %d, ent %d, bufsize: %d\n", i, pChannel->m_iEntity, pChannel->m_Buffer.GetReadAvailable());
		}
	}

	// Show profiling data?
	if( voice_profile.GetInt() )
	{
		Msg("Voice - compress: %7.2fu, decompress: %7.2fu, gain: %7.2fu, upsample: %7.2fu, total: %7.2fu\n", 
			g_CompressTime*1000000.0, 
			g_DecompressTime*1000000.0, 
			g_GainTime*1000000.0, 
			g_UpsampleTime*1000000.0,
			(g_CompressTime+g_DecompressTime+g_GainTime+g_UpsampleTime)*1000000.0
			);

		g_CompressTime = g_DecompressTime = g_GainTime = g_UpsampleTime = 0;
	}
}


bool Voice_IsRecording()
{
	return g_bVoiceRecording && !g_bInTweakMode;
}


bool Voice_RecordStart(
	const char *pUncompressedFile, 
	const char *pDecompressedFile,
	const char *pMicInputFile)
{
	if( !g_pEncodeCodec && !g_bUsingSteamVoice )
		return false;

	g_VoiceWriter.Flush();

	Voice_RecordStop();

	if ( !g_bUsingSteamVoice )
	{
		g_pEncodeCodec->ResetState();
	}
	
	if(pMicInputFile)
	{
		int a, b, c;
		ReadWaveFile(pMicInputFile, g_pMicInputFileData, g_nMicInputFileBytes, a, b, c);
		g_CurMicInputFileByte = 0;
		g_MicStartTime = Plat_FloatTime();
	}

	if(pUncompressedFile)
	{
		g_pUncompressedFileData = new char[MAX_WAVEFILEDATA_LEN];
		g_nUncompressedDataBytes = 0;
		g_pUncompressedDataFilename = pUncompressedFile;
	}

	if(pDecompressedFile)
	{
		g_pDecompressedFileData = new char[MAX_WAVEFILEDATA_LEN];
		g_nDecompressedDataBytes = 0;
		g_pDecompressedDataFilename = pDecompressedFile;
	}

	g_bVoiceRecording = false;
	if ( g_pVoiceRecord )
	{
		g_bVoiceRecording = VoiceRecord_Start();
		if ( g_bVoiceRecording )
		{
			if ( steamapicontext && steamapicontext->SteamFriends() && steamapicontext->SteamUser() )
			{
				// Tell Friends' Voice chat that the local user has started speaking
				steamapicontext->SteamFriends()->SetInGameVoiceSpeaking( steamapicontext->SteamUser()->GetSteamID(), true );
			}

			g_pSoundServices->OnChangeVoiceStatus( -1, true );		// Tell the client DLL.
		}
	}

	return g_bVoiceRecording;
}


void Voice_UserDesiresStop()
{
	if ( g_bVoiceRecordStopping )
		return;

	g_bVoiceRecordStopping = true;
	g_pSoundServices->OnChangeVoiceStatus( -1, false );		// Tell the client DLL.

	// If we're using Steam voice, we'll keep recording until Steam tells us we
	// received all the data.
	if ( g_bUsingSteamVoice )
	{
		steamapicontext->SteamUser()->StopVoiceRecording();
	}
	else
	{
		VoiceRecord_Stop();
	}
}


bool Voice_RecordStop()
{
	// Write the files out for debugging.
	if(g_pMicInputFileData)
	{
		delete [] g_pMicInputFileData;
		g_pMicInputFileData = NULL;
	}

	if(g_pUncompressedFileData)
	{
		WriteWaveFile(g_pUncompressedDataFilename, g_pUncompressedFileData, g_nUncompressedDataBytes, g_VoiceSampleFormat.wBitsPerSample, g_VoiceSampleFormat.nChannels, Voice_SamplesPerSec() );
		delete [] g_pUncompressedFileData;
		g_pUncompressedFileData = NULL;
	}

	if(g_pDecompressedFileData)
	{
		WriteWaveFile(g_pDecompressedDataFilename, g_pDecompressedFileData, g_nDecompressedDataBytes, g_VoiceSampleFormat.wBitsPerSample, g_VoiceSampleFormat.nChannels, Voice_SamplesPerSec() );
		delete [] g_pDecompressedFileData;
		g_pDecompressedFileData = NULL;
	}
	
	g_VoiceWriter.Finish();

	VoiceRecord_Stop();

	if ( g_bVoiceRecording )
	{
		if ( steamapicontext->SteamFriends() && steamapicontext->SteamUser() )
		{
			// Tell Friends' Voice chat that the local user has stopped speaking
			steamapicontext->SteamFriends()->SetInGameVoiceSpeaking( steamapicontext->SteamUser()->GetSteamID(), false );
		}
	}

	g_bVoiceRecording = false;
	g_bVoiceRecordStopping = false;
	return(true);
}


int Voice_GetCompressedData(char *pchDest, int nCount, bool bFinal)
{
	// Check g_bVoiceRecordStopping in case g_bUsingSteamVoice changes on us
	// while waiting for the end of voice data.
	if ( g_bUsingSteamVoice || g_bVoiceRecordStopping )
	{
		uint32 cbCompressedWritten = 0;
		uint32 cbUncompressedWritten = 0;
		uint32 cbCompressed = 0;
		uint32 cbUncompressed = 0;
		// We're going to always request steam give us the encoded stream at the optimal rate, unless our final output
		// rate is lower than it.  We'll pass our output rate when we actually extract the data, which Steam will
		// happily upsample from its optimal rate for us.
		int nEncodeRate = min( (int)steamapicontext->SteamUser()->GetVoiceOptimalSampleRate(), Voice_SamplesPerSec() );
		EVoiceResult result = steamapicontext->SteamUser()->GetAvailableVoice( &cbCompressed, &cbUncompressed, nEncodeRate );
		if ( result == k_EVoiceResultOK )
		{
			result = steamapicontext->SteamUser()->GetVoice( true, pchDest, nCount, &cbCompressedWritten, 
			                                                 g_pUncompressedFileData != NULL, g_pUncompressedFileData,
			                                                 MAX_WAVEFILEDATA_LEN - g_nUncompressedDataBytes,
			                                                 &cbUncompressedWritten, nEncodeRate );

			if ( g_pUncompressedFileData )
			{
				g_nUncompressedDataBytes += cbUncompressedWritten;
			}
			g_pSoundServices->OnChangeVoiceStatus( -3, true );
		}
		else
		{
			if ( result == k_EVoiceResultNotRecording && g_bVoiceRecording )
			{
				Voice_RecordStop();
			}

			g_pSoundServices->OnChangeVoiceStatus( -3, false );
		}
		return cbCompressedWritten;
	}

	IVoiceCodec *pCodec = g_pEncodeCodec;
	if( g_pVoiceRecord && pCodec )
	{
#ifdef VOICE_VOX_ENABLE	
		static ConVarRef voice_vox( "voice_vox" );
#endif // VOICE_VOX_ENABLE	

		short tempData[8192];
		int samplesWanted = min(nCount/BYTES_PER_SAMPLE, (int)sizeof(tempData)/BYTES_PER_SAMPLE);
		int gotten = g_pVoiceRecord->GetRecordedData(tempData, samplesWanted);

		// If they want to get the data from a file instead of the mic, use that.
		if(g_pMicInputFileData)
		{
			double curtime = Plat_FloatTime();
			int nShouldGet = (curtime - g_MicStartTime) * Voice_SamplesPerSec();
			gotten = min(sizeof(tempData)/BYTES_PER_SAMPLE,
			             (size_t)min(nShouldGet, (g_nMicInputFileBytes - g_CurMicInputFileByte) / BYTES_PER_SAMPLE));
			memcpy(tempData, &g_pMicInputFileData[g_CurMicInputFileByte], gotten*BYTES_PER_SAMPLE);
			g_CurMicInputFileByte += gotten * BYTES_PER_SAMPLE;
			g_MicStartTime = curtime;
		}
#ifdef VOICE_VOX_ENABLE	
		else if ( gotten && voice_vox.GetBool() == true )
		{
			// If the voice data is essentially silent, don't transmit
			short *pData = tempData;
			int averageData = 0;
			int minData = 16384;
			int maxData = -16384;
			for ( int i=0; i<gotten; ++i )
			{
				short val = *pData;
				averageData += val;
				minData = min( val, minData );
				maxData = max( val, maxData );
				++pData;
			}
			averageData /= gotten;
			int deltaData = maxData - minData;

			if ( deltaData < voice_threshold.GetFloat() && maxData < voice_threshold.GetFloat() )
			{
				// -3 signals that we're silent
				g_pSoundServices->OnChangeVoiceStatus( -3, false );
				return 0;
			}
		}
#endif // VOICE_VOX_ENABLE

#ifdef VOICE_SEND_RAW_TEST
		int nCompressedBytes = min( gotten, nCount );
		for ( int i=0; i < nCompressedBytes; i++ )
		{
			pchDest[i] = (char)(tempData[i] >> 8);
		}
#else			
		int nCompressedBytes = pCodec->Compress((char*)tempData, gotten, pchDest, nCount, !!bFinal);
#endif

		// Write to our file buffers..
		if(g_pUncompressedFileData)
		{
			int nToWrite = min(gotten*BYTES_PER_SAMPLE, MAX_WAVEFILEDATA_LEN - g_nUncompressedDataBytes);
			memcpy(&g_pUncompressedFileData[g_nUncompressedDataBytes], tempData, nToWrite);
			g_nUncompressedDataBytes += nToWrite;
		}
#ifdef VOICE_VOX_ENABLE
		// -3 signals that we're talking
		g_pSoundServices->OnChangeVoiceStatus( -3, (nCompressedBytes > 0) );
#endif // VOICE_VOX_ENABLE		
		return nCompressedBytes;
	}
	else
	{
#ifdef VOICE_VOX_ENABLE	
		// -3 signals that we're silent
		g_pSoundServices->OnChangeVoiceStatus( -3, false );
#endif // VOICE_VOX_ENABLE
		return 0;
	}
}


//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
// Purpose: Assigns a channel to an entity by searching for either a channel
//			already assigned to that entity or picking the least recently used
//			channel. If the LRU channel is picked, it is flushed and all other
//			channels are aged.
// Input  : nEntity - entity number to assign to a channel.
// Output : A channel index to which the entity has been assigned.
//------------------------------------------------------------------------------
int Voice_AssignChannel(int nEntity, bool bProximity)
{
	if(g_bInTweakMode)
		return VOICE_CHANNEL_IN_TWEAK_MODE;

	// See if a channel already exists for this entity and if so, just return it.
	int iFree = -1;
	for(int i=0; i < VOICE_NUM_CHANNELS; i++)
	{
		CVoiceChannel *pChannel = &g_VoiceChannels[i];

		if(pChannel->m_iEntity == nEntity)
		{
			return i;
		}
		else if(pChannel->m_iEntity == -1 && ( pChannel->m_pVoiceCodec || g_bUsingSteamVoice ) )
		{
			// Won't exist in steam voice mode
			if ( pChannel->m_pVoiceCodec )
			{
				pChannel->m_pVoiceCodec->ResetState();
			}
			iFree = i;
			break;
		}
	}

	// If they're all used, then don't allow them to make a new channel.
	if(iFree == -1)
	{
		return VOICE_CHANNEL_ERROR;
	}

	CVoiceChannel *pChannel = &g_VoiceChannels[iFree];
	pChannel->Init(nEntity);
	pChannel->m_bProximity = bProximity;
	VoiceSE_StartOverdrive();

	return iFree;
}


//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
// Purpose: Determines which channel has been assigened to a given entity.
// Input  : nEntity - entity number.
// Output : The index of the channel assigned to the entity, VOICE_CHANNEL_ERROR
//			if no channel is currently assigned to the given entity.
//------------------------------------------------------------------------------
int Voice_GetChannel(int nEntity)
{
	for(int i=0; i < VOICE_NUM_CHANNELS; i++)
		if(g_VoiceChannels[i].m_iEntity == nEntity)
			return i;

	return VOICE_CHANNEL_ERROR;
}


double UpsampleIntoBuffer(
	const short *pSrc,
	int nSrcSamples,
	CCircularBuffer *pBuffer,
	double startFraction,
	double rate)
{
	double maxFraction = nSrcSamples - 1;

	while(1)
	{
		if(startFraction >= maxFraction)
			break;

		int iSample = (int)startFraction;
		double frac = startFraction - floor(startFraction);

		double val1 = pSrc[iSample];
		double val2 = pSrc[iSample+1];
		short newSample = (short)(val1 + (val2 - val1) * frac);
		pBuffer->Write(&newSample, sizeof(newSample));

		startFraction += rate;
	}

	return startFraction - floor(startFraction);
}


//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
// Purpose: Adds received voice data to 
// Input  : 
// Output : 
//------------------------------------------------------------------------------
int Voice_AddIncomingData(int nChannel, const char *pchData, int nCount, int iSequenceNumber)
{
	CVoiceChannel *pChannel;

	// If in tweak mode, we call this during Idle with -1 as the channel, so any channel data from the network
	// gets rejected.
	if(g_bInTweakMode)
	{
		if(nChannel == TWEAKMODE_CHANNELINDEX)
			nChannel = 0;
		else
			return 0;
	}

	if ( ( pChannel = GetVoiceChannel(nChannel)) == NULL || ( !g_bUsingSteamVoice && !pChannel->m_pVoiceCodec ) )
	{
		return(0);
	}

	pChannel->m_bStarved = false;	// This only really matters if you call Voice_AddIncomingData between the time the mixer
									// asks for data and Voice_Idle is called.

	// Decompress.
	// @note Tom Bui: suggested destination buffer for Steam voice is 22kb
	char decompressed[22528];

#ifdef VOICE_SEND_RAW_TEST

		int nDecompressed = nCount;
		for ( int i=0; i < nDecompressed; i++ )
			((short*)decompressed)[i] = pchData[i] << 8;

#else

	int nDecompressed = 0;
	if ( g_bUsingSteamVoice )
	{
		uint32 nBytesWritten = 0;
		EVoiceResult result = steamapicontext->SteamUser()->DecompressVoice( pchData, nCount,
		                                                                     decompressed, sizeof( decompressed ),
		                                                                     &nBytesWritten, Voice_SamplesPerSec() );
		if ( result == k_EVoiceResultOK )
		{
			nDecompressed = nBytesWritten / BYTES_PER_SAMPLE;
		}
	}
	else
	{
		nDecompressed = pChannel->m_pVoiceCodec->Decompress(pchData, nCount, decompressed, sizeof(decompressed));
	}

#endif

	if ( g_bInTweakMode )
	{
		short *data = (short *)decompressed;
		g_VoiceTweakSpeakingVolume = 0;

		// Find the highest value
		for ( int i=0; i<nDecompressed; ++i )
		{
			g_VoiceTweakSpeakingVolume = max((int)abs(data[i]), g_VoiceTweakSpeakingVolume);
		}

		// Smooth it out
		g_VoiceTweakSpeakingVolume &= 0xFE00;
	}

	pChannel->m_AutoGain.ProcessSamples((short*)decompressed, nDecompressed);

	// Upsample into the dest buffer. We could do this in a mixer but it complicates the mixer.
	pChannel->m_LastFraction = UpsampleIntoBuffer( (short*)decompressed,
	                                               nDecompressed,
	                                               &pChannel->m_Buffer,
	                                               pChannel->m_LastFraction,
	                                               (double)Voice_SamplesPerSec()/g_VoiceSampleFormat.nSamplesPerSec );
	pChannel->m_LastSample = decompressed[nDecompressed];

	// Write to our file buffer..
	if(g_pDecompressedFileData)
	{
		int nToWrite = min(nDecompressed*2, MAX_WAVEFILEDATA_LEN - g_nDecompressedDataBytes);
		memcpy(&g_pDecompressedFileData[g_nDecompressedDataBytes], decompressed, nToWrite);
		g_nDecompressedDataBytes += nToWrite;
	}

	g_VoiceWriter.AddDecompressedData( pChannel, (const byte *)decompressed, nDecompressed * 2 );

	if( voice_showincoming.GetInt() != 0 )
	{
		Msg("Voice - %d incoming samples added to channel %d\n", nDecompressed, nChannel);
	}

	return(nChannel);
}


#if DEAD
//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
// Purpose: Flushes a given receive channel.
// Input  : nChannel - index of channel to flush.
//------------------------------------------------------------------------------
void Voice_FlushChannel(int nChannel)
{
	if ((nChannel < 0) || (nChannel >= VOICE_NUM_CHANNELS))
	{
		Assert(false);
		return;
	}

	g_VoiceChannels[nChannel].m_Buffer.Flush();
}
#endif


//------------------------------------------------------------------------------
// IVoiceTweak implementation.
//------------------------------------------------------------------------------

int VoiceTweak_StartVoiceTweakMode()
{
	// If we're already in voice tweak mode, return an error.
	if(g_bInTweakMode)
	{
		Assert(!"VoiceTweak_StartVoiceTweakMode called while already in tweak mode.");
		return 0;
	}

	if ( !g_pMixerControls && voice_enable.GetBool() )
	{
		Voice_ForceInit();
	}

	if( !g_pMixerControls )
		return 0;

	Voice_EndAllChannels();
	Voice_RecordStart(NULL, NULL, NULL);
	Voice_AssignChannel(TWEAKMODE_ENTITYINDEX, false );
	g_bInTweakMode = true;
	InitMixerControls();

	return 1;
}

void VoiceTweak_EndVoiceTweakMode()
{
	if(!g_bInTweakMode)
	{
		Assert(!"VoiceTweak_EndVoiceTweakMode called when not in tweak mode.");
		return;
	}

	g_bInTweakMode = false;
	Voice_RecordStop();
}

void VoiceTweak_SetControlFloat(VoiceTweakControl iControl, float flValue)
{
	if(!g_pMixerControls)
		return;

	if(iControl == MicrophoneVolume)
	{
		g_pMixerControls->SetValue_Float(IMixerControls::MicVolume, flValue);
	}
	else if ( iControl == MicBoost )
	{
		g_pMixerControls->SetValue_Float( IMixerControls::MicBoost, flValue );
	}
	else if(iControl == OtherSpeakerScale)
	{
		voice_scale.SetValue( flValue );
	}
}

void Voice_ForceInit()
{
	if ( g_pMixerControls || !voice_enable.GetBool() )
	{
		// Nothing to do
		return;
	}

	// Lacking a better default, just peak at what the server's sv_voicecodec is set to
	static ConVarRef sv_voicecodec( "sv_voicecodec" );
	if ( !Voice_InitWithDefault( sv_voicecodec.GetString() ) )
	{
		// Try ultimate fallback
		Voice_InitWithDefault( VOICE_FALLBACK_CODEC );
	}
}

float VoiceTweak_GetControlFloat(VoiceTweakControl iControl)
{
	Voice_ForceInit();

	if(!g_pMixerControls)
		return 0;

	if(iControl == MicrophoneVolume)
	{
		float value = 1;
		g_pMixerControls->GetValue_Float(IMixerControls::MicVolume, value);
		return value;
	}
	else if(iControl == OtherSpeakerScale)
	{
		return voice_scale.GetFloat();
	}
	else if(iControl == SpeakingVolume)
	{
		return g_VoiceTweakSpeakingVolume * 1.0f / 32768;
	}
	else if ( iControl == MicBoost )
	{
		float flValue = 1;
		g_pMixerControls->GetValue_Float( IMixerControls::MicBoost, flValue );
		return flValue;
	}
	else
	{
		return 1;
	}
}

bool VoiceTweak_IsStillTweaking()
{
	return g_bInTweakMode;
}

void Voice_Spatialize( channel_t *channel )
{
	if ( !g_bInTweakMode )
		return;

	Assert( channel->sfx );
	Assert( channel->sfx->pSource );
	Assert( channel->sfx->pSource->GetType() == CAudioSource::AUDIO_SOURCE_VOICE );

	// Place the tweak mode sound back at the view entity
	CVoiceChannel *pVoiceChannel = GetVoiceChannel( 0 );
	Assert( pVoiceChannel );
	if ( !pVoiceChannel )
		return;

	if ( pVoiceChannel->m_nSoundGuid != channel->guid )
		return;

	// No change
	if ( g_pSoundServices->GetViewEntity() == pVoiceChannel->m_nViewEntityIndex )
		return;

	DevMsg( 1, "Voice_Spatialize changing voice tweak entity from %d to %d\n", pVoiceChannel->m_nViewEntityIndex, g_pSoundServices->GetViewEntity() );

	pVoiceChannel->m_nViewEntityIndex = g_pSoundServices->GetViewEntity();
	channel->soundsource = pVoiceChannel->m_nViewEntityIndex;
}

IVoiceTweak g_VoiceTweakAPI =
{
	VoiceTweak_StartVoiceTweakMode,
	VoiceTweak_EndVoiceTweakMode,
	VoiceTweak_SetControlFloat,
	VoiceTweak_GetControlFloat,
	VoiceTweak_IsStillTweaking,
};


