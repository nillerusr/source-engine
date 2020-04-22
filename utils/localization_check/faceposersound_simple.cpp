//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#pragma warning( disable: 4201 )
#include <mmsystem.h>
#pragma warning( default: 4201 )

#include <stdio.h>
#include <math.h>
#include "snd_audio_source.h"
#include "AudioWaveOutput.h"
#include "ifaceposersound.h"
#include "utlvector.h"
#include "filesystem.h"
#include "sentence.h"

typedef struct channel_s
{
	int		leftvol;
	int		rightvol;
	int		rleftvol;
	int		rrightvol;
	float	pitch;
} channel_t;

#define WAVE_FORMAT_STEREO		(WAVE_FORMAT_1S08|WAVE_FORMAT_1S16|WAVE_FORMAT_2S08|WAVE_FORMAT_2S16|WAVE_FORMAT_4S08|WAVE_FORMAT_4S16)
#define WAVE_FORMATS_UNDERSTOOD	(0xFFF)
#define WAVE_FORMAT_11K			(WAVE_FORMAT_1M08|WAVE_FORMAT_1M16)
#define WAVE_FORMAT_22K			(WAVE_FORMAT_2M08|WAVE_FORMAT_2M16)
#define WAVE_FORMAT_44K			(WAVE_FORMAT_4M08|WAVE_FORMAT_4M16)

void CAudioDeviceSWMix::Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, int rateScaleFix, int outCount, int timecompress, bool forward )
{
	int sampleIndex = 0;
	fixedint sampleFrac = inputOffset;

	int fixup = 0;
	int fixupstep = 1;

	if ( !forward )
	{
		fixup = outCount - 1;
		fixupstep = -1;
	}

	for ( int i = 0; i < outCount; i++, fixup += fixupstep )
	{
		int dest = max( outputOffset + fixup, 0 );

		m_paintbuffer[ dest ].left += pChannel->leftvol * pData[sampleIndex];
		m_paintbuffer[ dest ].right += pChannel->rightvol * pData[sampleIndex];
		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac);
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}


void CAudioDeviceSWMix::Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, int rateScaleFix, int outCount, int timecompress, bool forward )
{
	int sampleIndex = 0;
	fixedint sampleFrac = inputOffset;

	int fixup = 0;
	int fixupstep = 1;

	if ( !forward )
	{
		fixup = outCount - 1;
		fixupstep = -1;
	}

	for ( int i = 0; i < outCount; i++, fixup += fixupstep )
	{
		int dest = max( outputOffset + fixup, 0 );

		m_paintbuffer[ dest ].left += pChannel->leftvol * pData[sampleIndex];
		m_paintbuffer[ dest ].right += pChannel->rightvol * pData[sampleIndex+1];
		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}


void CAudioDeviceSWMix::Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, int rateScaleFix, int outCount, int timecompress, bool forward )
{
	int sampleIndex = 0;
	fixedint sampleFrac = inputOffset;

	int fixup = 0;
	int fixupstep = 1;

	if ( !forward )
	{
		fixup = outCount - 1;
		fixupstep = -1;
	}

	for ( int i = 0; i < outCount; i++, fixup += fixupstep )
	{
		int dest = max( outputOffset + fixup, 0 );

		m_paintbuffer[ dest ].left += (pChannel->leftvol * pData[sampleIndex])>>8;
		m_paintbuffer[ dest ].right += (pChannel->rightvol * pData[sampleIndex])>>8;
		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac);
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}


void CAudioDeviceSWMix::Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, int rateScaleFix, int outCount, int timecompress, bool forward )
{
	int sampleIndex = 0;
	fixedint sampleFrac = inputOffset;

	int fixup = 0;
	int fixupstep = 1;

	if ( !forward )
	{
		fixup = outCount - 1;
		fixupstep = -1;
	}

	for ( int i = 0; i < outCount; i++, fixup += fixupstep )
	{
		int dest = max( outputOffset + fixup, 0 );

		m_paintbuffer[ dest ].left += (pChannel->leftvol * pData[sampleIndex])>>8;
		m_paintbuffer[ dest ].right += (pChannel->rightvol * pData[sampleIndex+1])>>8;

		sampleFrac += rateScaleFix;
		sampleIndex += FIX_INTPART(sampleFrac)<<1;
		sampleFrac = FIX_FRACPART(sampleFrac);
	}
}


int CAudioDeviceSWMix::MaxSampleCount( void )
{
	return PAINTBUFFER_SIZE;
}

void CAudioDeviceSWMix::MixBegin( void )
{
	memset( m_paintbuffer, 0, sizeof(m_paintbuffer) );
}

void CAudioDeviceSWMix::TransferBufferStereo16( short *pOutput, int sampleCount )
{
	for ( int i = 0; i < sampleCount; i++ )
	{
		if ( m_paintbuffer[i].left > 32767 )
			m_paintbuffer[i].left = 32767;
		else if ( m_paintbuffer[i].left < -32768 )
			m_paintbuffer[i].left = -32768;

		if ( m_paintbuffer[i].right > 32767 )
			m_paintbuffer[i].right = 32767;
		else if ( m_paintbuffer[i].right < -32768 )
			m_paintbuffer[i].right = -32768;

		*pOutput++ = (short)m_paintbuffer[i].left;
		*pOutput++ = (short)m_paintbuffer[i].right;
	}
}

CAudioWaveOutput::CAudioWaveOutput( void )
{
	for ( int i = 0; i < OUTPUT_BUFFER_COUNT; i++ )
	{
		CAudioBuffer *buffer = &m_buffers[ i ];
		Assert( buffer );
		buffer->hdr = NULL;
		buffer->submitted = false;
		buffer->submit_sample_count = false;
	}

	ClearDevice();
	OpenDevice();

	m_mixTime = -1;
	m_sampleIndex = 0;
	memset( m_sourceList, 0, sizeof(m_sourceList) );

	m_nEstimatedSamplesAhead = (int)( ( float ) OUTPUT_SAMPLE_RATE / 10.0f );
}

void CAudioWaveOutput::RemoveMixerChannelReferences( CAudioMixer *mixer )
{
	for ( int i = 0; i < OUTPUT_BUFFER_COUNT; i++ )
	{
		RemoveFromReferencedList( mixer, &m_buffers[ i ] );
	}
}

void CAudioWaveOutput::AddToReferencedList( CAudioMixer *mixer, CAudioBuffer *buffer )
{
	// Already in list
	for ( int i = 0; i < buffer->m_Referenced.Size(); i++ )
	{
		if ( buffer->m_Referenced[ i ].mixer == mixer )
		{
			return;
		}
	}

	// Just remove it
	int idx = buffer->m_Referenced.AddToTail();

	CAudioMixerState *state = &buffer->m_Referenced[ idx ];
	state->mixer = mixer;
	state->submit_mixer_sample = mixer->GetSamplePosition();

}

void CAudioWaveOutput::RemoveFromReferencedList( CAudioMixer *mixer, CAudioBuffer *buffer )
{
	for ( int i = 0; i < buffer->m_Referenced.Size(); i++ )
	{
		if ( buffer->m_Referenced[ i ].mixer == mixer )
		{
			buffer->m_Referenced.Remove( i );
			break;
		}
	}
}

bool CAudioWaveOutput::IsSoundInReferencedList( CAudioMixer *mixer, CAudioBuffer *buffer )
{
	for ( int i = 0; i < buffer->m_Referenced.Size(); i++ )
	{
		if ( buffer->m_Referenced[ i ].mixer == mixer )
		{
			return true;
		}
	}
	return false;
}

bool CAudioWaveOutput::IsSourceReferencedByActiveBuffer( CAudioMixer *mixer )
{
	if ( !ValidDevice() )
		return false;

	CAudioBuffer *buffer;
	for ( int i = 0; i < OUTPUT_BUFFER_COUNT; i++ )
	{
		buffer = &m_buffers[ i ];
		if ( !buffer->submitted )
			continue;

		if ( buffer->hdr->dwFlags & WHDR_DONE )
			continue;

		// See if it's referenced
		if ( IsSoundInReferencedList( mixer, buffer ) )
			return true;
	}

	return false;
}

CAudioWaveOutput::~CAudioWaveOutput( void )
{
	if ( ValidDevice() )
	{
		waveOutReset( m_deviceHandle );
		for ( int i = 0; i < OUTPUT_BUFFER_COUNT; i++ )
		{
			if ( m_buffers[i].hdr )
			{
				waveOutUnprepareHeader( m_deviceHandle, m_buffers[i].hdr, sizeof(*m_buffers[i].hdr) );
				delete[] m_buffers[i].hdr->lpData;
				delete m_buffers[i].hdr;
			}
			m_buffers[i].hdr = NULL;
			m_buffers[i].submitted = false;
			m_buffers[i].submit_sample_count = 0;
			m_buffers[i].m_Referenced.Purge();
		}
		waveOutClose( m_deviceHandle );
		ClearDevice();
	}
}



CAudioBuffer *CAudioWaveOutput::GetEmptyBuffer( void )
{
	CAudioBuffer *pOutput = NULL;
	if ( ValidDevice() )
	{
		for ( int i = 0; i < OUTPUT_BUFFER_COUNT; i++ )
		{
			if ( !(m_buffers[ i ].submitted ) || 
				m_buffers[i].hdr->dwFlags & WHDR_DONE )
			{
				pOutput = &m_buffers[i];
				pOutput->submitted = true;
				pOutput->m_Referenced.Purge();
				break;
			}
		}
	}
	
	return pOutput;
}

void CAudioWaveOutput::SilenceBuffer( short *pSamples, int sampleCount )
{
	int i;

	for ( i = 0; i < sampleCount; i++ )
	{
		// left
		*pSamples++ = 0;
		// right
		*pSamples++ = 0;
	}
}

void CAudioWaveOutput::Flush( void )
{
	waveOutReset( m_deviceHandle );
}

// mix a buffer up to time (time is absolute)
void CAudioWaveOutput::Update( float time )
{
}

int CAudioWaveOutput::GetNumberofSamplesAhead( void )
{
	ComputeSampleAheadAmount();
	return m_nEstimatedSamplesAhead;
}

float CAudioWaveOutput::GetAmountofTimeAhead( void )
{
	ComputeSampleAheadAmount();
	return ( (float)m_nEstimatedSamplesAhead / (float)OUTPUT_SAMPLE_RATE );
}

// Find the most recent submitted sample that isn't flagged as whdr_done
void CAudioWaveOutput::ComputeSampleAheadAmount( void )
{
	m_nEstimatedSamplesAhead = 0;

	int newest_sample_index = -1;
	int newest_sample_count = 0;

	CAudioBuffer *buffer;

	if ( ValidDevice() )
	{

		for ( int i = 0; i < OUTPUT_BUFFER_COUNT; i++ )
		{
			buffer = &m_buffers[ i ];
			if ( !buffer->submitted )
				continue;

			if ( buffer->hdr->dwFlags & WHDR_DONE )
				continue;

			if ( buffer->submit_sample_count > newest_sample_count )
			{
				newest_sample_index = i;
				newest_sample_count = buffer->submit_sample_count;
			}
		}
	}

	if ( newest_sample_index == -1 )
		return;


	buffer = &m_buffers[ newest_sample_index ];
	int currentPos = GetOutputPosition() ;
	m_nEstimatedSamplesAhead = currentPos - buffer->submit_sample_count;
}

int CAudioWaveOutput::FindSourceIndex( CAudioMixer *pSource )
{
	for ( int i = 0; i < MAX_CHANNELS; i++ )
	{
		if ( pSource == m_sourceList[i] )
		{
			return i;
		}
	}
	return -1;
}

CAudioMixer *CAudioWaveOutput::GetMixerForSource( CAudioSource *source )
{
	for ( int i = 0; i < MAX_CHANNELS; i++ )
	{
		if ( !m_sourceList[i] )
			continue;

		if ( source == m_sourceList[i]->GetSource() )
		{
			return m_sourceList[i];
		}
	}
	return NULL;
}

void CAudioWaveOutput::AddSource( CAudioMixer *pSource )
{
	int slot = 0;
	for ( int i = 0; i < MAX_CHANNELS; i++ )
	{
		if ( !m_sourceList[i] )
		{
			slot = i;
			break;
		}
	}

	if ( m_sourceList[slot] )
	{
		FreeChannel( slot );
	}
	SetChannel( slot, pSource );

	pSource->SetActive( true );
}


void CAudioWaveOutput::StopSounds( void )
{
	for ( int i = 0; i < MAX_CHANNELS; i++ )
	{
		if ( m_sourceList[i] )
		{
			FreeChannel( i );
		}
	}
}


void CAudioWaveOutput::SetChannel( int channelIndex, CAudioMixer *pSource )
{
	if ( channelIndex < 0 || channelIndex >= MAX_CHANNELS )
		return;

	m_sourceList[channelIndex] = pSource;
}

void CAudioWaveOutput::FreeChannel( int channelIndex )
{
	if ( channelIndex < 0 || channelIndex >= MAX_CHANNELS )
		return;

	if ( m_sourceList[channelIndex] )
	{
		RemoveMixerChannelReferences( m_sourceList[channelIndex] );

		delete m_sourceList[channelIndex];
		m_sourceList[channelIndex] = NULL;
	}
}

int CAudioWaveOutput::GetOutputPosition( void )
{
	if ( !m_deviceHandle )
		return 0;

	MMTIME mmtime;
	mmtime.wType = TIME_SAMPLES;
	waveOutGetPosition( m_deviceHandle, &mmtime, sizeof( MMTIME ) );

	// Convert time to sample count
	return ( mmtime.u.sample );
}

void CAudioWaveOutput::OpenDevice( void )
{
	WAVEFORMATEX waveFormat;

	memset( &waveFormat, 0, sizeof(waveFormat) );
	// Select a PCM, 16-bit stereo playback device
	waveFormat.cbSize = sizeof(waveFormat);
	waveFormat.nAvgBytesPerSec = OUTPUT_SAMPLE_RATE * 2 * 2;
	waveFormat.nBlockAlign = 2 * 2;	// channels * sample size
	waveFormat.nChannels = 2; // stereo
	waveFormat.nSamplesPerSec = OUTPUT_SAMPLE_RATE;
	waveFormat.wBitsPerSample = 16;
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;

	MMRESULT errorCode = waveOutOpen( &m_deviceHandle, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL );
	if ( errorCode == MMSYSERR_NOERROR )
	{
		int bufferSize = 4 * ( OUTPUT_SAMPLE_RATE / OUTPUT_BUFFER_COUNT ); // total of 1 second of data

		// Got one!
		for ( int i = 0; i < OUTPUT_BUFFER_COUNT; i++ )
		{
			m_buffers[i].hdr = new WAVEHDR;
			m_buffers[i].hdr->lpData = new char[ bufferSize ];
			long align = (long)m_buffers[i].hdr->lpData;
			if ( align & 3 )
			{
				m_buffers[i].hdr->lpData = (char *) ( (align+3) &~3 );
			}
			m_buffers[i].hdr->dwBufferLength = bufferSize - (align&3);
			m_buffers[i].hdr->dwFlags = 0;

			if ( waveOutPrepareHeader( m_deviceHandle, m_buffers[i].hdr, sizeof(*m_buffers[i].hdr) ) != MMSYSERR_NOERROR )
			{
				ClearDevice();
				return;
			}
		}
	}
	else
	{
		ClearDevice();
	}
}

// factory to create a suitable audio output for this system
CAudioOutput *CAudioOutput::Create( void )
{
	// sound device is a singleton for now
	static CAudioOutput *pWaveOut = NULL;

	if ( !pWaveOut )
	{
		pWaveOut = new CAudioWaveOutput;
	}

	return pWaveOut;
}

struct CSoundFile
{
	char				filename[ 512 ];
	CAudioSource		*source;
	long				filetime;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CFacePoserSound : public IFacePoserSound
{
public:
				CFacePoserSound();
				~CFacePoserSound( void );

	void		Init( void );
	void		Shutdown( void );
	void		Update( float dt );
	void		Flush( void );

	CAudioSource *LoadSound( const char *wavfile );
	void		PlaySound( StudioModel *source, float volume, const char *wavfile, CAudioMixer **ppMixer );
	void		PlaySound( CAudioSource *source, float volume, CAudioMixer **ppMixer );
	void		PlayPartialSound( StudioModel *model, float volume, const char *wavfile, CAudioMixer **ppMixer, int startSample, int endSample );

	bool		IsSoundPlaying( CAudioMixer *pMixer );
	CAudioMixer *FindMixer( CAudioSource *source );

	void		StopAll( void );
	void		StopSound( CAudioMixer *mixer );

	void		RenderWavToDC( HDC dc, RECT& outrect, COLORREF clr, float starttime, float endtime, 
		CAudioSource *pWave, bool selected = false, int selectionstart = 0, int selectionend = 0 );

	// void		InstallPhonemecallback( IPhonemeTag *pTagInterface );
	float		GetAmountofTimeAhead( void );

	int			GetNumberofSamplesAhead( void );

	CAudioOuput	*GetAudioOutput( void );

	virtual void		EnsureNoModelReferences( CAudioSource *source );

private:
	CAudioOutput *m_pAudio;

};

static CFacePoserSound g_FacePoserSound;

IFacePoserSound *sound = ( IFacePoserSound * )&g_FacePoserSound;

CFacePoserSound::CFacePoserSound() :
	m_pAudio( 0 )
{
}

CFacePoserSound::~CFacePoserSound( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAudioOuput	*CFacePoserSound::GetAudioOutput( void )
{
	return (CAudioOuput *)m_pAudio;
}

void CFacePoserSound::Init( void )
{
	m_pAudio = CAudioOutput::Create();
}

void CFacePoserSound::Shutdown( void )
{
	delete m_pAudio;
}

float CFacePoserSound::GetAmountofTimeAhead( void )
{
	return 0.0f;
}

int CFacePoserSound::GetNumberofSamplesAhead( void )
{
	return 0;
}


CAudioSource *CFacePoserSound::LoadSound( const char *wavfile )
{
	if ( !m_pAudio )
		return NULL;

	CAudioSource *wave = AudioSource_Create( wavfile );
	return wave;
}

void CFacePoserSound::PlaySound( StudioModel *model, float volume, const char *wavfile, CAudioMixer **ppMixer )
{
}

void CFacePoserSound::PlayPartialSound( StudioModel *model, float volume, const char *wavfile, CAudioMixer **ppMixer, int startSample, int endSample )
{
}

void CFacePoserSound::PlaySound( CAudioSource *source, float volume, CAudioMixer **ppMixer )
{
}

void CFacePoserSound::Update( float dt )
{
}

void CFacePoserSound::Flush( void )
{
}

void CFacePoserSound::StopAll( void )
{
}

void CFacePoserSound::StopSound( CAudioMixer *mixer )
{
}

void CFacePoserSound::RenderWavToDC( HDC dc, RECT& outrect, COLORREF clr, 
	float starttime, float endtime, CAudioSource *pWave, 
	bool selected /*= false*/, int selectionstart /*= 0*/, int selectionend /*= 0*/ )
{
}

bool CFacePoserSound::IsSoundPlaying( CAudioMixer *pMixer )
{
	return false;
}

CAudioMixer *CFacePoserSound::FindMixer( CAudioSource *source )
{
	return NULL;
}


void CFacePoserSound::EnsureNoModelReferences( CAudioSource *source )
{
}