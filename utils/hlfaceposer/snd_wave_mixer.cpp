//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//===========================================================================//
#include <stdio.h>
#include <windows.h>
#include "snd_audio_source.h"
#include "snd_wave_source.h"
#include "snd_wave_mixer_private.h"
#include "snd_wave_mixer_adpcm.h"
#include "ifaceposersound.h"
#include "AudioWaveOutput.h"
#include "tier2/riff.h"

typedef struct channel_s
{
	int		leftvol;
	int		rightvol;
	int		rleftvol;
	int		rrightvol;
	float	pitch;
} channel_t;

//-----------------------------------------------------------------------------
// These mixers provide an abstraction layer between the audio device and 
// mixing/decoding code.  They allow data to be decoded and mixed using 
// optimized, format sensitive code by calling back into the device that
// controls them.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: maps mixing to 8-bit mono mixer
//-----------------------------------------------------------------------------
class CAudioMixerWave8Mono : public CAudioMixerWave
{
public:
	CAudioMixerWave8Mono( CWaveData *data ) : CAudioMixerWave( data ) {}
	virtual void Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress, bool forward = true )
	{
		pDevice->Mix8Mono( pChannel, (char *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress, forward );
	}
};

//-----------------------------------------------------------------------------
// Purpose: maps mixing to 8-bit stereo mixer
//-----------------------------------------------------------------------------
class CAudioMixerWave8Stereo : public CAudioMixerWave
{
public:
	CAudioMixerWave8Stereo( CWaveData *data ) : CAudioMixerWave( data ) {}
	virtual void Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress, bool forward = true )
	{
		pDevice->Mix8Stereo( pChannel, (char *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress, forward );
	}
};

//-----------------------------------------------------------------------------
// Purpose: maps mixing to 16-bit mono mixer
//-----------------------------------------------------------------------------
class CAudioMixerWave16Mono : public CAudioMixerWave
{
public:
	CAudioMixerWave16Mono( CWaveData *data ) : CAudioMixerWave( data ) {}
	virtual void Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress, bool forward = true )
	{
		pDevice->Mix16Mono( pChannel, (short *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress, forward );
	}
};


//-----------------------------------------------------------------------------
// Purpose: maps mixing to 16-bit stereo mixer
//-----------------------------------------------------------------------------
class CAudioMixerWave16Stereo : public CAudioMixerWave
{
public:
	CAudioMixerWave16Stereo( CWaveData *data ) : CAudioMixerWave( data ) {}
	virtual void Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress, bool forward = true )
	{
		pDevice->Mix16Stereo( pChannel, (short *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress, forward );
	}
};


//-----------------------------------------------------------------------------
// Purpose: Create an approprite mixer type given the data format
// Input  : *data - data access abstraction
//			format - pcm or adpcm (1 or 2 -- RIFF format)
//			channels - number of audio channels (1 = mono, 2 = stereo)
//			bits - bits per sample
// Output : CAudioMixer * abstract mixer type that maps mixing to appropriate code
//-----------------------------------------------------------------------------
CAudioMixer *CreateWaveMixer( CWaveData *data, int format, int channels, int bits )
{
	if ( format == WAVE_FORMAT_PCM )
	{
		if ( channels > 1 )
		{
			if ( bits == 8 )
				return new CAudioMixerWave8Stereo( data );
			else
				return new CAudioMixerWave16Stereo( data );
		}
		else
		{
			if ( bits == 8 )
				return new CAudioMixerWave8Mono( data );
			else
				return new CAudioMixerWave16Mono( data );
		}
	}
	else if ( format == WAVE_FORMAT_ADPCM )
	{
		return CreateADPCMMixer( data );
	}
	return NULL;
}

#include "hlfaceposer.h"
//-----------------------------------------------------------------------------
// Purpose: Init the base WAVE mixer.
// Input  : *data - data access object
//-----------------------------------------------------------------------------
CAudioMixerWave::CAudioMixerWave( CWaveData *data ) : m_pData(data), m_pChannel(NULL)
{
	m_loop = 0;
	m_sample = 0;
	m_absoluteSample = 0;
	m_scrubSample = -1;
	m_fracOffset = 0;
	m_bActive = false;
	m_nModelIndex = -1;
	m_bForward = true;
	m_bAutoDelete = true;
	m_pChannel = new channel_t;
	m_pChannel->leftvol   = 127;
	m_pChannel->rightvol  = 127;
	m_pChannel->pitch		= 1.0;
}

//-----------------------------------------------------------------------------
// Purpose: Frees the data access object (we own it after construction)
//-----------------------------------------------------------------------------
CAudioMixerWave::~CAudioMixerWave( void )
{
	delete m_pData;
	delete m_pChannel;
}


//-----------------------------------------------------------------------------
// Purpose: Decode and read the data
//			by default we just pass the request on to the data access object
//			other mixers may need to buffer or decode the data for some reason
//
// Input  : **pData - dest pointer
//			sampleCount - number of samples needed
// Output : number of samples available in this batch
//-----------------------------------------------------------------------------
int	CAudioMixerWave::GetOutputData( void **pData, int samplePosition, int sampleCount, bool forward /*= true*/ )
{
	if ( samplePosition != m_sample )
	{
		// Seek
		m_sample = samplePosition;
		m_absoluteSample = samplePosition;
	}

	return m_pData->ReadSourceData( pData, m_sample, sampleCount, forward );
}


//-----------------------------------------------------------------------------
// Purpose: calls through the wavedata to get the audio source
// Output : CAudioSource
//-----------------------------------------------------------------------------
CAudioSource *CAudioMixerWave::GetSource( void )
{
	if ( m_pData )
		return &m_pData->Source();

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Gets the current sample location in playback
// Output : int (samples from start of wave)
//-----------------------------------------------------------------------------
int CAudioMixerWave::GetSamplePosition( void )
{
	return m_sample;
}


//-----------------------------------------------------------------------------
// Purpose: Gets the current sample location in playback
// Output : int (samples from start of wave)
//-----------------------------------------------------------------------------
int CAudioMixerWave::GetScrubPosition( void )
{
	if (m_scrubSample != -1)
	{
		return m_scrubSample;
	}
	return m_sample;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : position - 
//-----------------------------------------------------------------------------
bool CAudioMixerWave::SetSamplePosition( int position, bool scrubbing )
{
	position = max( 0, position );

	m_sample = position;
	m_absoluteSample = position;
	m_startpos = m_sample;
	if (scrubbing)
	{
		m_scrubSample = position;
	}
	else
	{
		m_scrubSample = -1;
	}
		
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : position - 
//-----------------------------------------------------------------------------
void CAudioMixerWave::SetLoopPosition( int position )
{
	m_loop = position;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CAudioMixerWave::GetStartPosition( void )
{
	return m_startpos;
}

bool CAudioMixerWave::GetActive( void )
{
	return m_bActive;
}

void CAudioMixerWave::SetActive( bool active )
{
	m_bActive = active;
}

void CAudioMixerWave::SetModelIndex( int index )
{
	m_nModelIndex = index;
}

int CAudioMixerWave::GetModelIndex( void ) const
{
	return m_nModelIndex;
}

void CAudioMixerWave::SetDirection( bool forward )
{
	m_bForward = forward;
}

bool CAudioMixerWave::GetDirection( void ) const
{
	return m_bForward;
}

void CAudioMixerWave::SetAutoDelete( bool autodelete )
{
	m_bAutoDelete = autodelete;
}

bool CAudioMixerWave::GetAutoDelete( void ) const
{
	return m_bAutoDelete;
}

void CAudioMixerWave::SetVolume( float volume )
{
	int ivolume = (int)( clamp( volume, 0.0f, 1.0f ) * 127.0f );

	m_pChannel->leftvol   = ivolume;
	m_pChannel->rightvol  = ivolume;
}

channel_t *CAudioMixerWave::GetChannel()
{
	Assert( m_pChannel );
	return m_pChannel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pChannel - 
//			sampleCount - 
//			outputRate - 
//-----------------------------------------------------------------------------
void CAudioMixerWave::IncrementSamples( channel_t *pChannel, int startSample, int sampleCount,int outputRate, bool forward /*= true*/ )
{
	int inputSampleRate = (int)(pChannel->pitch * m_pData->Source().SampleRate());
	float rate = (float)inputSampleRate / outputRate;

	int startpos = startSample;

	if ( !forward )
	{
		int requestedstart = startSample - (int)( sampleCount * rate );
		if ( requestedstart < 0 )
			return;

		startpos = max( 0, requestedstart );
		SetSamplePosition( startpos );
	}

	while ( sampleCount > 0 )
	{
		int inputSampleCount;
		int outputSampleCount = sampleCount;
		
		if ( outputRate != inputSampleRate )
		{
			inputSampleCount = (int)(sampleCount * rate);
		}
		else
		{
			inputSampleCount = sampleCount;
		}
		
		sampleCount -= outputSampleCount;
		if ( forward )
		{
			m_sample += inputSampleCount;
			m_absoluteSample += inputSampleCount;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: The device calls this to request data.  The mixer must provide the
//			full amount of samples or have silence in its output stream.
// Input  : *pDevice - requesting device
//			sampleCount - number of samples at the output rate
//			outputRate - sampling rate of the request
// Output : Returns true to keep mixing, false to delete this mixer
//-----------------------------------------------------------------------------
bool CAudioMixerWave::SkipSamples( channel_t *pChannel, int startSample, int sampleCount, int outputRate, bool forward /*= true*/ )
{
	int offset = 0;

	int inputSampleRate = (int)(pChannel->pitch * m_pData->Source().SampleRate());
	float rate = (float)inputSampleRate / outputRate;

	sampleCount = min( sampleCount, PAINTBUFFER_SIZE );

	int startpos = startSample;

	if ( !forward )
	{
		int requestedstart = startSample - (int)( sampleCount * rate );
		if ( requestedstart < 0 )
			return false;

		startpos = max( 0, requestedstart );
		SetSamplePosition( startpos );
	}

	while ( sampleCount > 0 )
	{
		int inputSampleCount;
		char *pData = NULL;
		int outputSampleCount = sampleCount;
		
		if ( outputRate != inputSampleRate )
		{
			inputSampleCount = (int)(sampleCount * rate);
			if ( !forward )
			{
				startSample = max( 0, startSample - inputSampleCount );
			}
			int availableSamples = GetOutputData( (void **)&pData, startSample, inputSampleCount, forward );
			if ( !availableSamples )
				break;

			if ( availableSamples < inputSampleCount )
			{
				outputSampleCount = (int)(availableSamples / rate);
				inputSampleCount = availableSamples;
			}

			// compute new fraction part of sample index
			float flOffset = (m_fracOffset / FIX_SCALE) + (rate * outputSampleCount);
			flOffset = flOffset - (float)((int)flOffset);
			m_fracOffset = FIX_FLOAT( flOffset );
		}
		else
		{
			if ( !forward )
			{
				startSample = max( 0, startSample - sampleCount );
			}
			int availableSamples = GetOutputData( (void **)&pData, startSample, sampleCount, forward );
			if ( !availableSamples )
				break;
			outputSampleCount = availableSamples;
			inputSampleCount = availableSamples;

		}
		offset += outputSampleCount;
		sampleCount -= outputSampleCount;
		if ( forward )
		{
			m_sample += inputSampleCount;
			m_absoluteSample += inputSampleCount;
		}

		if ( m_loop != 0 && m_sample >= m_loop )
		{
			SetSamplePosition( m_startpos );
		}

	}

	if ( sampleCount > 0 )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: The device calls this to request data.  The mixer must provide the
//			full amount of samples or have silence in its output stream.
// Input  : *pDevice - requesting device
//			sampleCount - number of samples at the output rate
//			outputRate - sampling rate of the request
// Output : Returns true to keep mixing, false to delete this mixer
//-----------------------------------------------------------------------------
bool CAudioMixerWave::MixDataToDevice( IAudioDevice *pDevice, channel_t *pChannel, int startSample, int sampleCount, int outputRate, bool forward /*= true*/ )
{
	int offset = 0;

	int inputSampleRate = (int)(pChannel->pitch * m_pData->Source().SampleRate());
	float rate = (float)inputSampleRate / outputRate;
	fixedint fracstep = FIX_FLOAT( rate );

	sampleCount = min( sampleCount, PAINTBUFFER_SIZE );

	int startpos = startSample;

	if ( !forward )
	{
		int requestedstart = startSample - (int)( sampleCount * rate );
		if ( requestedstart < 0 )
			return false;

		startpos = max( 0, requestedstart );
		SetSamplePosition( startpos );
	}

	while ( sampleCount > 0 )
	{
		int inputSampleCount;
		char *pData = NULL;
		int outputSampleCount = sampleCount;
		
		
		if ( outputRate != inputSampleRate )
		{
			inputSampleCount = (int)(sampleCount * rate);

			int availableSamples = GetOutputData( (void **)&pData, startpos, inputSampleCount, forward );
			if ( !availableSamples )
				break;

			if ( availableSamples < inputSampleCount )
			{
				outputSampleCount = (int)(availableSamples / rate);
				inputSampleCount = availableSamples;
			}

			Mix( pDevice, pChannel, pData, offset, m_fracOffset, fracstep, outputSampleCount, 0, forward );
			
			// compute new fraction part of sample index
			float flOffset = (m_fracOffset / FIX_SCALE) + (rate * outputSampleCount);
			flOffset = flOffset - (float)((int)flOffset);
			m_fracOffset = FIX_FLOAT( flOffset );
		}
		else
		{
			int availableSamples = GetOutputData( (void **)&pData, startpos, sampleCount, forward );
			if ( !availableSamples )
				break;

			outputSampleCount = availableSamples;
			inputSampleCount = availableSamples;

			Mix( pDevice, pChannel, pData, offset, m_fracOffset, FIX(1), outputSampleCount, 0, forward );
		}
		offset += outputSampleCount;
		sampleCount -= outputSampleCount;

		if ( forward )
		{
			m_sample += inputSampleCount;
			m_absoluteSample += inputSampleCount;
		}

		if ( m_loop != 0 && m_sample >= m_loop )
		{
			SetSamplePosition( m_startpos );
		}

	}

	if ( sampleCount > 0 )
		return false;

	return true;
}
