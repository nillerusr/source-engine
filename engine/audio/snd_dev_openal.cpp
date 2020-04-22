//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "audio_pch.h"
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#ifdef OSX
#include <OpenAL/MacOSX_OALExtensions.h>
#endif


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef DEDICATED  // have to test this because VPC is forcing us to compile this file.

extern bool snd_firsttime;
extern bool MIX_ScaleChannelVolume( paintbuffer_t *ppaint, channel_t *pChannel, int volume[CCHANVOLUMES], int mixchans );
extern void S_SpatializeChannel( int volume[6], int master_vol, const Vector *psourceDir, float gain, float mono );

#define NUM_BUFFERS_SOURCES		128
#define	BUFF_MASK				(NUM_BUFFERS_SOURCES - 1 )
#define	BUFFER_SIZE			0x0400


//-----------------------------------------------------------------------------
//
// NOTE: This only allows 16-bit, stereo wave out
//
//-----------------------------------------------------------------------------
class CAudioDeviceOpenAL : public CAudioDeviceBase
{
public:
	bool		IsActive( void );
	bool		Init( void );
	void		Shutdown( void );
	void		PaintEnd( void );
	int			GetOutputPosition( void );
	void		ChannelReset( int entnum, int channelIndex, float distanceMod );
	void		Pause( void );
	void		UnPause( void );
	float		MixDryVolume( void );
	bool		Should3DMix( void );
	void		StopAllSounds( void );

	int			PaintBegin( float mixAheadTime, int soundtime, int paintedtime );
	void		ClearBuffer( void );
	void		UpdateListener( const Vector& position, const Vector& forward, const Vector& right, const Vector& up );
	void		MixBegin( int sampleCount );
	void		MixUpsample( int sampleCount, int filtertype );
	void		Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
	void		Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
	void		Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
	void		Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );

	void		TransferSamples( int end );
	void		SpatializeChannel( int volume[CCHANVOLUMES/2], int master_vol, const Vector& sourceDir, float gain, float mono);
	void		ApplyDSPEffects( int idsp, portable_samplepair_t *pbuffront, portable_samplepair_t *pbufrear, portable_samplepair_t *pbufcenter, int samplecount );

	const char *DeviceName( void )			{ return "OpenAL"; }
	int			DeviceChannels( void )		{ return 2; }
	int			DeviceSampleBits( void )	{ return 16; }
	int			DeviceSampleBytes( void )	{ return 2; }
	int			DeviceDmaSpeed( void )		{ return SOUND_DMA_SPEED; }
	int			DeviceSampleCount( void )	{ return m_deviceSampleCount; }

private:
	void	OpenWaveOut( void );
	void	CloseWaveOut( void );
	bool	ValidWaveOut( void ) const;

	ALuint  m_Buffer[NUM_BUFFERS_SOURCES];
	ALuint  m_Source[1];
	int		m_SndBufSize;
	
	void *m_sndBuffers;
	
	int			m_deviceSampleCount;

	int			m_buffersSent;
	int			m_buffersCompleted;
	int			m_pauseCount;
	bool		m_bSoundsShutdown;
};


IAudioDevice *Audio_CreateOpenALDevice( void )
{
	CAudioDeviceOpenAL *wave = NULL;
	if ( !wave )
	{
		wave = new CAudioDeviceOpenAL;
	}
	
	if ( wave->Init() )
		return wave;
	
	delete wave;
	wave = NULL;
	
	return NULL;
}


void OnSndSurroundCvarChanged( IConVar *pVar, const char *pOldString, float flOldValue );
void OnSndSurroundLegacyChanged( IConVar *pVar, const char *pOldString, float flOldValue );

//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CAudioDeviceOpenAL::Init( void )
{
	m_SndBufSize = 0;
	m_sndBuffers = NULL;
	m_pauseCount = 0;

	m_bSurround = false;
	m_bSurroundCenter = false;
	m_bHeadphone = false;
	m_buffersSent = 0;
	m_buffersCompleted = 0;
	m_pauseCount = 0;
	m_bSoundsShutdown = false;
	
	static bool first = true;
	if ( first )
	{
		snd_surround.SetValue( 2 );
		snd_surround.InstallChangeCallback( &OnSndSurroundCvarChanged );
		snd_legacy_surround.InstallChangeCallback( &OnSndSurroundLegacyChanged );
		first = false;
	}
	
	OpenWaveOut();

	if ( snd_firsttime )
	{
		DevMsg( "Wave sound initialized\n" );
	}
	return ValidWaveOut();
}

void CAudioDeviceOpenAL::Shutdown( void )
{
	CloseWaveOut();
}


//-----------------------------------------------------------------------------
// WAV out device
//-----------------------------------------------------------------------------
inline bool CAudioDeviceOpenAL::ValidWaveOut( void ) const 
{ 
	return m_sndBuffers != 0; 
}


//-----------------------------------------------------------------------------
// Opens the windows wave out device
//-----------------------------------------------------------------------------
void CAudioDeviceOpenAL::OpenWaveOut( void )
{
	m_buffersSent = 0;
	m_buffersCompleted = 0;
	
	ALenum      error;
	ALCcontext    *newContext = NULL;
	ALCdevice    *newDevice = NULL;
	
	// Create a new OpenAL Device
	// Pass NULL to specify the system‚use default output device
	const ALCchar *initStr = (const ALCchar *)"\'( (sampling-rate 44100 ))";
    
	newDevice = alcOpenDevice(initStr);
	if (newDevice != NULL)
	{
		// Create a new OpenAL Context
		// The new context will render to the OpenAL Device just created 
		ALCint attr[] = { ALC_FREQUENCY, DeviceDmaSpeed(), ALC_SYNC, AL_FALSE, 0 };
		
		newContext = alcCreateContext(newDevice, attr );
		if (newContext != NULL)
		{
			// Make the new context the Current OpenAL Context
			alcMakeContextCurrent(newContext);
			
			// Create some OpenAL Buffer Objects
			alGenBuffers( NUM_BUFFERS_SOURCES, m_Buffer);
			if((error = alGetError()) != AL_NO_ERROR) 
			{
				DevMsg("Error Generating Buffers: ");
				return;
			}
			
			// Create some OpenAL Source Objects
			alGenSources(1, m_Source);
			if(alGetError() != AL_NO_ERROR) 
			{
				DevMsg("Error generating sources! \n");
				return;
			}
			
			alListener3f( AL_POSITION,0.0f,0.0f,0.0f);
			int i;
			for ( i = 0; i < 1; i++ )
			{
				alSource3f( m_Source[i],AL_POSITION,0.0f,0.0f,0.0f );
				alSourcef( m_Source[i], AL_PITCH, 1.0f );
				alSourcef( m_Source[i], AL_GAIN, 1.0f );
			}
			
		}
	}
	
	m_SndBufSize = NUM_BUFFERS_SOURCES*BUFFER_SIZE;
	m_deviceSampleCount = m_SndBufSize / DeviceSampleBytes();

	if ( !m_sndBuffers )
	{
		m_sndBuffers = malloc( m_SndBufSize );
		memset( m_sndBuffers, 0x0, m_SndBufSize );
	}
}


//-----------------------------------------------------------------------------
// Closes the windows wave out device
//-----------------------------------------------------------------------------
void CAudioDeviceOpenAL::CloseWaveOut( void ) 
{ 
	if ( ValidWaveOut() )
	{
		ALCcontext  *context = NULL;
		ALCdevice  *device = NULL;
	
		m_bSoundsShutdown = true;
		alSourceStop( m_Source[0] );
		
		// Delete the Sources
		alDeleteSources(1, m_Source);
		// Delete the Buffers
		alDeleteBuffers(NUM_BUFFERS_SOURCES, m_Buffer);
		
		//Get active context
		context = alcGetCurrentContext();
		//Get device for active context
		device = alcGetContextsDevice(context);
		alcMakeContextCurrent( NULL );
		alcSuspendContext(context);
		//Release context
		alcDestroyContext(context);
		//Close device
		alcCloseDevice(device);	
	}
	
	if ( m_sndBuffers )
	{
		free( m_sndBuffers );
		m_sndBuffers = NULL;
	}
}



//-----------------------------------------------------------------------------
// Mixing setup
//-----------------------------------------------------------------------------
int CAudioDeviceOpenAL::PaintBegin( float mixAheadTime, int soundtime, int paintedtime )
{
	//  soundtime - total samples that have been played out to hardware at dmaspeed
	//  paintedtime - total samples that have been mixed at speed
	//  endtime - target for samples in mixahead buffer at speed

	unsigned int endtime = soundtime + mixAheadTime * DeviceDmaSpeed();
	
	int samps = DeviceSampleCount() >> (DeviceChannels()-1);

	if ((int)(endtime - soundtime) > samps)
		endtime = soundtime + samps;

	if ((endtime - paintedtime) & 0x3)
	{
		// The difference between endtime and painted time should align on 
		// boundaries of 4 samples.  This is important when upsampling from 11khz -> 44khz.
		endtime -= (endtime - paintedtime) & 0x3;
	}

	return endtime;
}


#ifdef OSX
ALvoid  alBufferDataStaticProc(const ALint bid, ALenum format, ALvoid* data, ALsizei size, ALsizei freq)
{
    static alBufferDataStaticProcPtr proc = NULL;
    
    if (proc == NULL) {
        proc = (alBufferDataStaticProcPtr) alGetProcAddress((const ALCchar*) "alBufferDataStatic");
    }
    
    if (proc)
        proc(bid, format, data, size, freq);
	
}
#endif


//-----------------------------------------------------------------------------
// Actually performs the mixing
//-----------------------------------------------------------------------------
void CAudioDeviceOpenAL::PaintEnd( void )
{
	if ( !m_sndBuffers /*|| m_bSoundsShutdown*/ )
		return;
	
	ALint state;
	ALenum      error;
	int iloop;
	
	int	cblocks = 4 << 1; 
	ALint processed = 1;
	ALuint lastUnqueuedBuffer = 0;
	ALuint unqueuedBuffer = -1;
	int nProcessedLoop = 200; // spin for a max of 200 times de-queing buffers, fixes a hang on exit
	while ( processed > 0 && --nProcessedLoop > 0 )
	{
		alGetSourcei( m_Source[ 0 ], AL_BUFFERS_PROCESSED, &processed);
		error = alGetError();
		if (error != AL_NO_ERROR)
			break;
		
		if ( processed > 0 )
		{
			lastUnqueuedBuffer = unqueuedBuffer;
			alSourceUnqueueBuffers( m_Source[ 0 ], 1, &unqueuedBuffer );
			error = alGetError();
			if ( error != AL_NO_ERROR && error != AL_INVALID_NAME ) 
			{
				DevMsg( "Error alSourceUnqueueBuffers %d\n", error );
				break;
			}
			else
			{
				m_buffersCompleted++;	// this buffer has been played
			}
		}
	}
	
	//
	// submit a few new sound blocks
	//
	// 44K sound support
	while (((m_buffersSent - m_buffersCompleted) >> SAMPLE_16BIT_SHIFT) < cblocks)
	{	
		int iBuf = m_buffersSent&BUFF_MASK; 
#ifdef OSX
		alBufferDataStaticProc( m_Buffer[iBuf], AL_FORMAT_STEREO16, (char *)m_sndBuffers + iBuf*BUFFER_SIZE, BUFFER_SIZE, DeviceDmaSpeed() );
#else
		alBufferData( m_Buffer[iBuf], AL_FORMAT_STEREO16, (char *)m_sndBuffers + iBuf*BUFFER_SIZE, BUFFER_SIZE, DeviceDmaSpeed() );
#endif
		if ( (error = alGetError()) != AL_NO_ERROR ) 
		{
			DevMsg( "Error alBufferData %d %d\n", iBuf, error );
		}  
		
		alSourceQueueBuffers( m_Source[0], 1, &m_Buffer[iBuf] );
		if ( (error = alGetError() ) != AL_NO_ERROR ) 
		{
			DevMsg( "Error alSourceQueueBuffers %d %d\n", iBuf, error );
		}  
		m_buffersSent++;
	}
	
	// make sure the stream is playing
	alGetSourcei( m_Source[ 0 ], AL_SOURCE_STATE, &state);
	if ( state != AL_PLAYING )
	{
		DevMsg( "Restarting sound playback\n" );
		alSourcePlay( m_Source[0] );
		if((error = alGetError()) != AL_NO_ERROR) 
		{
			DevMsg( "Error alSourcePlay %d\n", error );
		}  
	}
}

int CAudioDeviceOpenAL::GetOutputPosition( void )
{
	int s = m_buffersSent * BUFFER_SIZE;

	s >>= SAMPLE_16BIT_SHIFT;

	s &= (DeviceSampleCount()-1);

	return s / DeviceChannels();
}


//-----------------------------------------------------------------------------
// Pausing
//-----------------------------------------------------------------------------
void CAudioDeviceOpenAL::Pause( void )
{
	m_pauseCount++;
	if (m_pauseCount == 1)
	{
		alSourceStop( m_Source[0] );
	}
}


void CAudioDeviceOpenAL::UnPause( void )
{
	if ( m_pauseCount > 0 )
	{
		m_pauseCount--;
	}
	
	if ( m_pauseCount == 0 )
	{ 
		alSourcePlay( m_Source[0] );
	}
}

bool CAudioDeviceOpenAL::IsActive( void )
{
	return ( m_pauseCount == 0 );
}

float CAudioDeviceOpenAL::MixDryVolume( void )
{
	return 0;
}


bool CAudioDeviceOpenAL::Should3DMix( void )
{
	return false;
}


void CAudioDeviceOpenAL::ClearBuffer( void )
{
	if ( !m_sndBuffers )
		return;

	Q_memset( m_sndBuffers, 0x0, DeviceSampleCount() * DeviceSampleBytes() );
}

void CAudioDeviceOpenAL::UpdateListener( const Vector& position, const Vector& forward, const Vector& right, const Vector& up )
{
}


void CAudioDeviceOpenAL::MixBegin( int sampleCount )
{
	MIX_ClearAllPaintBuffers( sampleCount, false );
}


void CAudioDeviceOpenAL::MixUpsample( int sampleCount, int filtertype )
{
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();
	int ifilter = ppaint->ifilter;
	
	Assert (ifilter < CPAINTFILTERS);

	S_MixBufferUpsample2x( sampleCount, ppaint->pbuf, &(ppaint->fltmem[ifilter][0]), CPAINTFILTERMEM, filtertype );

	ppaint->ifilter++;
}

void CAudioDeviceOpenAL::Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();

	if (!MIX_ScaleChannelVolume( ppaint, pChannel, volume, 1))
		return;

	Mix8MonoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, (byte *)pData, inputOffset, rateScaleFix, outCount );
}


void CAudioDeviceOpenAL::Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();

	if (!MIX_ScaleChannelVolume( ppaint, pChannel, volume, 2 ))
		return;

	Mix8StereoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, (byte *)pData, inputOffset, rateScaleFix, outCount );
}


void CAudioDeviceOpenAL::Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();

	if (!MIX_ScaleChannelVolume( ppaint, pChannel, volume, 1 ))
		return;

	Mix16MonoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, pData, inputOffset, rateScaleFix, outCount );
}


void CAudioDeviceOpenAL::Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();

	if (!MIX_ScaleChannelVolume( ppaint, pChannel, volume, 2 ))
		return;

	Mix16StereoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, pData, inputOffset, rateScaleFix, outCount );
}


void CAudioDeviceOpenAL::ChannelReset( int entnum, int channelIndex, float distanceMod )
{
}


void CAudioDeviceOpenAL::TransferSamples( int end )
{
	int		lpaintedtime = g_paintedtime;
	int		endtime = end;
	
	// resumes playback...

	if ( m_sndBuffers )
	{
		S_TransferStereo16( m_sndBuffers, PAINTBUFFER, lpaintedtime, endtime );
	}
}

void CAudioDeviceOpenAL::SpatializeChannel( int volume[CCHANVOLUMES/2], int master_vol, const Vector& sourceDir, float gain, float mono )
{
	VPROF("CAudioDeviceOpenAL::SpatializeChannel");
	S_SpatializeChannel( volume, master_vol, &sourceDir, gain, mono );
}

void CAudioDeviceOpenAL::StopAllSounds( void )
{
	m_bSoundsShutdown = true;
	alSourceStop( m_Source[0] );
}



void CAudioDeviceOpenAL::ApplyDSPEffects( int idsp, portable_samplepair_t *pbuffront, portable_samplepair_t *pbufrear, portable_samplepair_t *pbufcenter, int samplecount )
{
	//SX_RoomFX( endtime, filter, timefx );
	DSP_Process( idsp, pbuffront, pbufrear, pbufcenter, samplecount );
}


static uint32 GetOSXSpeakerConfig()
{
	return 2;
}

static uint32 GetSpeakerConfigForSurroundMode( int surroundMode, const char **pConfigDesc )
{
	uint32 newSpeakerConfig = 2;
	*pConfigDesc = "stereo speaker";
	return newSpeakerConfig;
}



void OnSndSurroundCvarChanged( IConVar *pVar, const char *pOldString, float flOldValue )
{
	// if the old value is -1, we're setting this from the detect routine for the first time
	// no need to reset the device
	if ( flOldValue == -1 )
		return;
	
	// get the user's previous speaker config
	uint32 speaker_config = GetOSXSpeakerConfig();
	
	// get the new config
	uint32 newSpeakerConfig = 0;
	const char *speakerConfigDesc = "";
	
	ConVarRef var( pVar );
	newSpeakerConfig = GetSpeakerConfigForSurroundMode( var.GetInt(), &speakerConfigDesc );
	// make sure the config has changed
	if (newSpeakerConfig == speaker_config)
		return;
	
	// set new configuration
	//SetWindowsSpeakerConfig(newSpeakerConfig);
	
	Msg("Speaker configuration has been changed to %s.\n", speakerConfigDesc);
	
	// restart sound system so it takes effect
	//g_pSoundServices->RestartSoundSystem();
}

void OnSndSurroundLegacyChanged( IConVar *pVar, const char *pOldString, float flOldValue )
{
}

#endif // !DEDICATED

