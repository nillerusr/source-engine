//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <math.h>
#include "snd_audio_source.h"
#include "AudioWaveOutput.h"
#include "ISceneManagerSound.h"
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

#define INPUT_BUFFER_COUNT 32

class CAudioWaveInput : public CAudioInput
{
public:
	CAudioWaveInput( void );
	~CAudioWaveInput( void );

	// Returns the current count of available samples
	int SampleCount( void );
	
	// returns the size of each sample in bytes
	int SampleSize( void ) { return m_sampleSize; }
	
	// returns the sampling rate of the data
	int SampleRate( void ) { return m_sampleRate; }

	// returns a pointer to the actual data
	void *SampleData( void );

	// release the available data (mark as done)
	void SampleRelease( void );

	// returns the mono/stereo status of this device (true if stereo)
	bool IsStereo( void )  { return m_isStereo; }

	// begin sampling
	void Start( void );

	// stop sampling
	void Stop( void );

	void WaveMessage( HWAVEIN hdevice, UINT uMsg, DWORD dwParam1, DWORD dwParam2 );

private:
	void	OpenDevice( void );
	bool	ValidDevice( void ) { return m_deviceId >= 0; }
	void	ClearDevice( void ) { m_deviceId = (UINT)-1; }

	// returns true if the new format is better
	bool	BetterFormat( DWORD dwNewFormat, DWORD dwOldFormat );

	void	InitReadyList( void );
	void	AddToReadyList( WAVEHDR *pBuffer );
	void	PopReadyList( void );

	WAVEHDR	*m_pReadyList;

	int		m_sampleSize;
	int		m_sampleRate;
	bool	m_isStereo;

	UINT	m_deviceId;
	HWAVEIN	m_deviceHandle;

	WAVEHDR	*m_buffers[ INPUT_BUFFER_COUNT ];
};

extern "C" void CALLBACK WaveData( HWAVEIN hwi, UINT uMsg, CAudioWaveInput *pAudio, DWORD dwParam1, DWORD dwParam2 );

CAudioWaveInput::CAudioWaveInput( void )
{
	memset( m_buffers, 0, sizeof( m_buffers ) );
	int deviceCount = (int)waveInGetNumDevs();
	UINT	deviceId = (UINT)-1;
	DWORD	deviceFormat = 0;

	int i;
	for ( i = 0; i < deviceCount; i++ )
	{
		WAVEINCAPS waveCaps;
		MMRESULT errorCode = waveInGetDevCaps( (UINT)i, &waveCaps, sizeof(waveCaps) );
		if ( errorCode == MMSYSERR_NOERROR )
		{
			// valid device
			if ( BetterFormat( waveCaps.dwFormats, deviceFormat ) )
			{
				deviceId = i;
				deviceFormat = waveCaps.dwFormats;
			}
		}
	}

	if ( !deviceFormat )
	{
		m_deviceId = (UINT)-1;
		m_sampleSize = 0;
		m_sampleRate = 0;
		m_isStereo = false;
	}
	else
	{
		m_deviceId = deviceId;
		m_sampleRate = 44100;
		m_isStereo = false;
		if ( deviceFormat & WAVE_FORMAT_4M16 )
		{
			m_sampleSize = 2;
		}
		else if ( deviceFormat & WAVE_FORMAT_4M08 )
		{
			m_sampleSize = 1;
		}
		else
		{
			// ERROR!
		}

		OpenDevice();
	}

	InitReadyList();
}

CAudioWaveInput::~CAudioWaveInput( void )
{
	if ( ValidDevice() )
	{
		Stop();
		waveInReset( m_deviceHandle );
		waveInClose( m_deviceHandle );
		for ( int i = 0; i < INPUT_BUFFER_COUNT; i++ )
		{
			if ( m_buffers[i] )
			{
				waveInUnprepareHeader( m_deviceHandle, m_buffers[i], sizeof( *m_buffers[i] ) );
				delete[] m_buffers[i]->lpData;
				delete m_buffers[i];
			}
			m_buffers[i] = NULL;
		}
		ClearDevice();
	}
}

void CALLBACK WaveData( HWAVEIN hwi, UINT uMsg, CAudioWaveInput *pAudio, DWORD dwParam1, DWORD dwParam2 )
{
	if ( pAudio )
	{
		pAudio->WaveMessage( hwi, uMsg, dwParam1, dwParam2 );
	}
}

void CAudioWaveInput::WaveMessage( HWAVEIN hdevice, UINT uMsg, DWORD dwParam1, DWORD dwParam2 )
{
	if ( hdevice != m_deviceHandle )
		return;
	switch( uMsg )
	{
	case WIM_DATA:
		WAVEHDR *pHeader = (WAVEHDR *)dwParam1;
		AddToReadyList( pHeader );
		break;
	}
}

void CAudioWaveInput::OpenDevice( void )
{
	if ( !ValidDevice() )
		return;

	WAVEFORMATEX format;

	memset( &format, 0, sizeof(format) );
	format.nAvgBytesPerSec = m_sampleRate * m_sampleSize;
	format.nChannels = 1;
	format.wBitsPerSample = m_sampleSize * 8;
	format.nSamplesPerSec = m_sampleRate;
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nBlockAlign = m_sampleSize;

	MMRESULT errorCode = waveInOpen( &m_deviceHandle, m_deviceId, &format, (DWORD)WaveData, (DWORD)this, CALLBACK_FUNCTION );
	if ( errorCode == MMSYSERR_NOERROR )
	{
		// valid device opened
		int bufferSize = m_sampleSize * m_sampleRate / INPUT_BUFFER_COUNT; // total of one second of data

		// allocate buffers
		for ( int i = 0; i < INPUT_BUFFER_COUNT; i++ )
		{
			m_buffers[i] = new WAVEHDR;
			m_buffers[i]->lpData = new char[ bufferSize ];
			m_buffers[i]->dwBufferLength = bufferSize;
			m_buffers[i]->dwUser = 0;
			m_buffers[i]->dwFlags = 0;
	
			waveInPrepareHeader( m_deviceHandle, m_buffers[i], sizeof( *m_buffers[i] ) );
			waveInAddBuffer( m_deviceHandle, m_buffers[i], sizeof( *m_buffers[i] ) );
		}
	}
	else
	{
		ClearDevice();
	}
}

void CAudioWaveInput::Start( void )
{
	if ( !ValidDevice() )
		return;

	waveInStart( m_deviceHandle );
}

void CAudioWaveInput::Stop( void )
{
	if ( !ValidDevice() )
		return;

	waveInStop( m_deviceHandle );
}

void CAudioWaveInput::InitReadyList( void )
{
	m_pReadyList = NULL;
}

void CAudioWaveInput::AddToReadyList( WAVEHDR *pBuffer )
{
	WAVEHDR **pList = &m_pReadyList;

	waveInUnprepareHeader( m_deviceHandle, pBuffer, sizeof(*pBuffer) );
	// insert at the tail of the list
	while ( *pList )
	{
		pList = reinterpret_cast<WAVEHDR **>(&((*pList)->dwUser));
	}
	pBuffer->dwUser = NULL;
	*pList = pBuffer;
}


void CAudioWaveInput::PopReadyList( void )
{
	if ( m_pReadyList )
	{
		WAVEHDR *pBuffer = m_pReadyList;
		m_pReadyList = reinterpret_cast<WAVEHDR *>(m_pReadyList->dwUser);
		waveInPrepareHeader( m_deviceHandle, pBuffer, sizeof(*pBuffer) );
		waveInAddBuffer( m_deviceHandle, pBuffer, sizeof(*pBuffer) );
	}
}



#define WAVE_FORMAT_STEREO		(WAVE_FORMAT_1S08|WAVE_FORMAT_1S16|WAVE_FORMAT_2S08|WAVE_FORMAT_2S16|WAVE_FORMAT_4S08|WAVE_FORMAT_4S16)
#define WAVE_FORMATS_UNDERSTOOD	(0xFFF)
#define WAVE_FORMAT_11K			(WAVE_FORMAT_1M08|WAVE_FORMAT_1M16)
#define WAVE_FORMAT_22K			(WAVE_FORMAT_2M08|WAVE_FORMAT_2M16)
#define WAVE_FORMAT_44K			(WAVE_FORMAT_4M08|WAVE_FORMAT_4M16)

static int HighestBit( DWORD dwFlags )
{
	int i = 31;
	while ( i )
	{
		if ( dwFlags & (1<<i) )
			return i;
		i--;
	}

	return 0;
}

bool CAudioWaveInput::BetterFormat( DWORD dwNewFormat, DWORD dwOldFormat )
{
	dwNewFormat &= WAVE_FORMATS_UNDERSTOOD & (~WAVE_FORMAT_STEREO);
	dwOldFormat &= WAVE_FORMATS_UNDERSTOOD & (~WAVE_FORMAT_STEREO);

	// our target format is 44.1KHz, mono, 16-bit
	if ( HighestBit(dwOldFormat) >= HighestBit(dwNewFormat) )
		return false;

	return true;
}


int CAudioWaveInput::SampleCount( void )
{
	if ( !ValidDevice() )
		return 0;

	if ( m_pReadyList )
	{
		switch( SampleSize() )
		{
		case 2:
			return m_pReadyList->dwBytesRecorded >> 1;
		case 1:
			return m_pReadyList->dwBytesRecorded;
		default:
			break;
		}
	}
	return 0;
}

void *CAudioWaveInput::SampleData( void )
{
	if ( !ValidDevice() )
		return NULL;

	if ( m_pReadyList )
	{
		return m_pReadyList->lpData;
	}

	return NULL;
}


// release the available data (mark as done)
void CAudioWaveInput::SampleRelease( void )
{
	PopReadyList();
}


// factory to create a suitable audio input for this system
CAudioInput *CAudioInput::Create( void )
{
	// sound source is a singleton for now
	static CAudioInput *pSource = NULL;

	if ( !pSource )
	{
		pSource = new CAudioWaveInput;
	}

	return pSource;
}

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
	channel_t channel;

	channel.leftvol = 200;
	channel.rightvol = 200;
	channel.pitch = 1.0;

	if ( !ValidDevice() )
		return;

	// reset the system
	if ( m_mixTime < 0 || time < m_baseTime )
	{
		m_baseTime = time;
		m_mixTime = 0;
	}

	// put time in our coordinate frame
	time -= m_baseTime;

	if ( time > m_mixTime )
	{
		CAudioBuffer *pBuffer = GetEmptyBuffer();
		
		// no free buffers, mixing is ahead of the playback!
		if ( !pBuffer || !pBuffer->hdr )
		{
			//Con_Printf( "out of buffers\n" );
			return;
		}

		// UNDONE: These numbers are constants
		// calc number of samples (2 channels * 2 bytes per sample)
		int sampleCount = pBuffer->hdr->dwBufferLength >> 2;
		//float oldTime = m_mixTime;

		m_mixTime += sampleCount * (1.0f / OUTPUT_SAMPLE_RATE);

		short *pSamples = reinterpret_cast<short *>(pBuffer->hdr->lpData);
		
		SilenceBuffer( pSamples, sampleCount );

		int tempCount = sampleCount;

		while ( tempCount > 0 )
		{
			if ( tempCount > m_audioDevice.MaxSampleCount() )
				sampleCount = m_audioDevice.MaxSampleCount();
			else
				sampleCount = tempCount;

			m_audioDevice.MixBegin();
			for ( int i = 0; i < MAX_CHANNELS; i++ )
			{
				CAudioMixer *pSource = m_sourceList[i];
				if ( !pSource )
					continue;

				int currentsample = pSource->GetSamplePosition();
				bool forward = pSource->GetDirection();

				if ( pSource->GetActive() )
				{
					if ( !pSource->MixDataToDevice( &m_audioDevice, &channel, currentsample, sampleCount, SampleRate(), forward ) )
					{
						// Source becomes inactive when last submitted sample is finally
						//  submitted.  But it lingers until it's no longer referenced
						pSource->SetActive( false );
					}
					else
					{
						AddToReferencedList( pSource, pBuffer );
					}
				} 
				else 
				{
					if ( !IsSourceReferencedByActiveBuffer( pSource ) )
					{
						if ( !pSource->GetAutoDelete() )
						{
							FreeChannel( i );
						}
					}
					else
					{
						pSource->IncrementSamples( &channel, currentsample, sampleCount, SampleRate(), forward );
					}
				}

			}

			m_audioDevice.TransferBufferStereo16( pSamples, sampleCount );

			m_sampleIndex += sampleCount;
			tempCount -= sampleCount;
			pSamples += sampleCount * 2;
		}
		// if the buffers aren't aligned on sample boundaries, this will hard-lock the machine!

		pBuffer->submit_sample_count = GetOutputPosition();

		waveOutWrite( m_deviceHandle, pBuffer->hdr, sizeof(*(pBuffer->hdr)) );
	}
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
class CSceneManagerSound : public ISceneManagerSound
{
public:
				~CSceneManagerSound( void );

	void		Init( void );
	void		Shutdown( void );
	void		Update( float dt );
	void		Flush( void );

	CAudioSource *LoadSound( const char *wavfile );
	void		PlaySound( const char *wavfile, CAudioMixer **ppMixer );

	void		PlaySound( CAudioSource *source, CAudioMixer **ppMixer );
	bool		IsSoundPlaying( CAudioMixer *pMixer );
	CAudioMixer *FindMixer( CAudioSource *source );

	void		StopAll( void );
	void		StopSound( CAudioMixer *mixer );

	CAudioOuput	*GetAudioOutput( void );

	virtual CAudioSource	*FindOrAddSound( const char *filename );

private:

	CAudioOutput *m_pAudio;

	float		m_flElapsedTime;

	CUtlVector < CSoundFile > m_ActiveSounds;
};

static CSceneManagerSound g_FacePoserSound;

ISceneManagerSound *sound = ( ISceneManagerSound * )&g_FacePoserSound;

CSceneManagerSound::~CSceneManagerSound( void )
{
	OutputDebugString( va( "Removing %i sounds\n", m_ActiveSounds.Size() ) );
	for ( int i = 0 ; i < m_ActiveSounds.Size(); i++ )
	{
		CSoundFile *p = &m_ActiveSounds[ i ];
		OutputDebugString( va( "Removing sound:  %s\n", p->filename ) );
		delete p->source;
	}

	m_ActiveSounds.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAudioOuput	*CSceneManagerSound::GetAudioOutput( void )
{
	return (CAudioOuput *)m_pAudio;
}

CAudioSource *CSceneManagerSound::FindOrAddSound( const char *filename )
{
	CSoundFile *s;

	int i;
	for ( i = 0; i < m_ActiveSounds.Size(); i++ )
	{
		s = &m_ActiveSounds[ i ];
		Assert( s );
		if ( !stricmp( s->filename, filename ) )
		{
			long filetime = filesystem->GetFileTime( filename );
			if ( filetime != s->filetime )
			{
				Con_Printf( "Reloading sound %s\n", filename );
				delete s->source;
				s->source = LoadSound( filename );
				s->filetime = filetime;
			}
			return s->source;
		}
	}

	i = m_ActiveSounds.AddToTail();
	s = &m_ActiveSounds[ i ];
	strcpy( s->filename, filename );
	s->source = LoadSound( filename );
	s->filetime = filesystem->GetFileTime( filename );

	return s->source;
}

void CSceneManagerSound::Init( void )
{
	m_flElapsedTime = 0.0f;
	m_pAudio = CAudioOutput::Create();
}

void CSceneManagerSound::Shutdown( void )
{
}

CAudioSource *CSceneManagerSound::LoadSound( const char *wavfile )
{
	if ( !m_pAudio )
		return NULL;

	CAudioSource *wave = AudioSource_Create( wavfile );
	return wave;
}

void CSceneManagerSound::PlaySound( const char *wavfile, CAudioMixer **ppMixer )
{
	if ( m_pAudio )
	{
		CAudioSource *wave = FindOrAddSound( wavfile );
		if ( !wave )
			return;

		CAudioMixer *pMixer = wave->CreateMixer();
		if ( ppMixer )
		{
			*ppMixer = pMixer;
		}
		m_pAudio->AddSource( pMixer );
	}
}

void CSceneManagerSound::PlaySound( CAudioSource *source, CAudioMixer **ppMixer )
{
	if ( ppMixer )
	{
		*ppMixer = NULL;
	}

	if ( m_pAudio )
	{
		CAudioMixer *mixer = source->CreateMixer();
		if ( ppMixer )
		{
			*ppMixer = mixer;
		}
		m_pAudio->AddSource( mixer );
	}
}

void CSceneManagerSound::Update( float dt )
{
	if ( m_pAudio )
	{
		m_pAudio->Update( m_flElapsedTime );
	}

	m_flElapsedTime += dt;
}

void CSceneManagerSound::Flush( void )
{
	if ( m_pAudio )
	{
		m_pAudio->Flush();
	}
}

void CSceneManagerSound::StopAll( void )
{
	if ( m_pAudio )
	{
		m_pAudio->StopSounds();
	}
}

void CSceneManagerSound::StopSound( CAudioMixer *mixer )
{
	int idx = m_pAudio->FindSourceIndex( mixer );
	if ( idx != -1 )
	{
		m_pAudio->FreeChannel( idx );
	}
}

bool CSceneManagerSound::IsSoundPlaying( CAudioMixer *pMixer )
{
	if ( !m_pAudio || !pMixer )
	{
		return false;
	}

	//
	int index = m_pAudio->FindSourceIndex( pMixer );
	if ( index != -1 )
		return true;

	return false;
}

CAudioMixer *CSceneManagerSound::FindMixer( CAudioSource *source )
{
	if ( !m_pAudio )
		return NULL;

	return m_pAudio->GetMixerForSource( source );
}
