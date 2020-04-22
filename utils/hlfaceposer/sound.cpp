//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#pragma warning( disable : 4201 )
#include <mmsystem.h>
#include <stdio.h>
#include <math.h>
#include "snd_audio_source.h"
#include "AudioWaveOutput.h"
#include "ifaceposersound.h"
#include "StudioModel.h"
#include "hlfaceposer.h"
#include "expressions.h"
#include "expclass.h"
#include "PhonemeConverter.h"
#include "utlvector.h"
#include "filesystem.h"
#include "sentence.h"
#include "faceposer_models.h"
#include "iclosecaptionmanager.h"
#include "phonemeeditor.h"
#include "wavebrowser.h"
#include "choreoscene.h"
#include "choreoview.h"
#include "KeyValues.h"

extern ISoundEmitterSystemBase *soundemitter;

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
	bool	ValidDevice( void ) { return m_deviceId != 0xFFFFFFFF; }
	void	ClearDevice( void ) { m_deviceId = 0xFFFFFFFF; }

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
	UINT	deviceId = 0;
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
		m_deviceId = 0xFFFFFFFF;
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

				StudioModel *model = NULL;

				int modelindex = pSource->GetModelIndex();
				if ( modelindex >= 0 )
				{
					model = models->GetStudioModel( modelindex );
				}
				else
				{
					if ( g_pPhonemeEditor->IsActiveTool() || g_pWaveBrowser->IsActiveTool() )
					{
						model = models->GetActiveStudioModel();
	
					}
				}

				if ( model && !model->m_mouth.IsSourceReferenced( pSource->GetSource() ) )
				{
					CChoreoScene *pScene = g_pChoreoView->GetScene();
					bool bIgnorePhonemes = pScene ? pScene->ShouldIgnorePhonemes() : false;
					model->m_mouth.AddSource( pSource->GetSource(), bIgnorePhonemes );
					if ( modelindex < 0 )
					{
						pSource->SetModelIndex( models->GetIndexForStudioModel( model ) );
					}
				}

				int currentsample = pSource->GetSamplePosition();
				bool forward = pSource->GetDirection();

				if ( pSource->GetActive() )
				{
					if ( !pSource->MixDataToDevice( &m_audioDevice, pSource->GetChannel(), currentsample, sampleCount, SampleRate(), forward ) )
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
						pSource->IncrementSamples( pSource->GetChannel(), currentsample, sampleCount, SampleRate(), forward );
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
		StudioModel *model = NULL;
		int modelindex = m_sourceList[channelIndex]->GetModelIndex();
		if ( modelindex >= 0)
		{
			model = models->GetStudioModel( modelindex );
		}

		if ( model )
		{
			model->m_mouth.RemoveSource( m_sourceList[channelIndex]->GetSource() );
		}

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
	void		AddViseme( float intensity, StudioModel *model, int phoneme, float scale );
	void		ProcessCloseCaptionData( StudioModel *model, float curtime, CSentence* sentence );
	void		SetupWeights( void );

	CAudioSource	*FindOrAddSound( const char *filename );

	CAudioOutput *m_pAudio;

	float		m_flElapsedTime;

	CUtlVector < CSoundFile > m_ActiveSounds;
};

static CFacePoserSound g_FacePoserSound;

IFacePoserSound *sound = ( IFacePoserSound * )&g_FacePoserSound;

CFacePoserSound::~CFacePoserSound( void )
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
CAudioOuput	*CFacePoserSound::GetAudioOutput( void )
{
	return (CAudioOuput *)m_pAudio;
}

CAudioSource *CFacePoserSound::FindOrAddSound( const char *filename )
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

void CFacePoserSound::Init( void )
{
	m_flElapsedTime = 0.0f;
	m_pAudio = CAudioOutput::Create();

	// Load SoundOverrides for Faceposer

	KeyValues *manifest = new KeyValues( "scripts/game_sounds_manifest.txt" );
	if ( filesystem->LoadKeyValues( *manifest, IFileSystem::TYPE_SOUNDEMITTER, "scripts/game_sounds_manifest.txt", "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
		{
			if ( !Q_stricmp( sub->GetName(), "faceposer_file" ) )
			{
				soundemitter->AddSoundOverrides( sub->GetString() );
				continue;
			}
		}
	}
	manifest->deleteThis();
}

void CFacePoserSound::Shutdown( void )
{
}

float CFacePoserSound::GetAmountofTimeAhead( void )
{
	if ( !m_pAudio )
		return 0.0f;

	return m_pAudio->GetAmountofTimeAhead();
}

int CFacePoserSound::GetNumberofSamplesAhead( void )
{
	if ( !m_pAudio )
		return 0;

	return m_pAudio->GetNumberofSamplesAhead();
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
		pMixer->SetVolume( volume );
		m_pAudio->AddSource( pMixer );
		if ( model )
		{
			pMixer->SetModelIndex( models->GetIndexForStudioModel( model ) );
		}
	}
}

void CFacePoserSound::PlayPartialSound( StudioModel *model, float volume, const char *wavfile, CAudioMixer **ppMixer, int startSample, int endSample )
{
	if ( !m_pAudio )
		return;

	StopAll();
	CAudioSource *wave = FindOrAddSound( wavfile );
	if ( !wave )
		return;

	CAudioMixer *mixer = wave->CreateMixer();
	if ( ppMixer )
	{
		*ppMixer = mixer;
	}

	mixer->SetSamplePosition( startSample );
	mixer->SetLoopPosition( endSample );
	mixer->SetVolume( volume );
	m_pAudio->AddSource( mixer );
}

void CFacePoserSound::PlaySound( CAudioSource *source, float volume, CAudioMixer **ppMixer )
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
		mixer->SetVolume( volume );
		m_pAudio->AddSource( mixer );
	}
}

enum
{
	PHONEME_CLASS_WEAK = 0,
	PHONEME_CLASS_NORMAL,
	PHONEME_CLASS_STRONG,

	NUM_PHONEME_CLASSES
};

struct Emphasized_Phoneme
{
	char			*classname;
	bool			required;
	bool			valid;
	CExpClass		*cl;
	CExpression		*exp;
	float			*settings;
	float			amount;
};

static Emphasized_Phoneme g_PhonemeClasses[ NUM_PHONEME_CLASSES ] =
{
	{ "phonemes_weak",		false },
	{ "phonemes",			true },
	{ "phonemes_strong",	false },
};

#define STRONG_CROSSFADE_START		0.60f
#define WEAK_CROSSFADE_START		0.40f

void ComputeBlendedSetting( Emphasized_Phoneme *classes, float emphasis_intensity )
{
	// Here's the formula
	// 0.5 is neutral 100 % of the default setting

	// Crossfade starts at STRONG_CROSSFADE_START and is full at STRONG_CROSSFADE_END
	// If there isn't a strong then the intensity of the underlying phoneme is fixed at 2 x STRONG_CROSSFADE_START
	//  so we don't get huge numbers

	bool has_weak = classes[ PHONEME_CLASS_WEAK ].valid;
	bool has_strong = classes[ PHONEME_CLASS_STRONG ].valid;

	Assert( classes[ PHONEME_CLASS_NORMAL ].valid );

	if ( emphasis_intensity > STRONG_CROSSFADE_START )
	{
		if ( has_strong )
		{
			// Blend in some of strong
			float dist_remaining = 1.0f - emphasis_intensity;
			float frac = dist_remaining / ( 1.0f - STRONG_CROSSFADE_START );

			classes[ PHONEME_CLASS_NORMAL ].amount = (frac) * 2.0f * STRONG_CROSSFADE_START;
			classes[ PHONEME_CLASS_STRONG ].amount = 1.0f - frac; 
		}
		else
		{
			emphasis_intensity = min( emphasis_intensity, STRONG_CROSSFADE_START );
			classes[ PHONEME_CLASS_NORMAL ].amount = 2.0f * emphasis_intensity;
		}
	}
	else if ( emphasis_intensity < WEAK_CROSSFADE_START )
	{
		if ( has_weak )
		{
			// Blend in some weak
			float dist_remaining = WEAK_CROSSFADE_START - emphasis_intensity;
			float frac = dist_remaining / ( WEAK_CROSSFADE_START );

			classes[ PHONEME_CLASS_NORMAL ].amount = (1.0f - frac) * 2.0f * WEAK_CROSSFADE_START;
			classes[ PHONEME_CLASS_WEAK ].amount = frac; 
		}
		else
		{
			emphasis_intensity = max( emphasis_intensity, WEAK_CROSSFADE_START );
			classes[ PHONEME_CLASS_NORMAL ].amount = 2.0f * emphasis_intensity;
		}
	}
	else
	{
		classes[ PHONEME_CLASS_NORMAL ].amount = 2.0f * emphasis_intensity;
	}
}

void CFacePoserSound::AddViseme( float intensity, StudioModel *model, int phoneme, float scale )
{
	int i;

	Assert( model );
	CStudioHdr *hdr = model->GetStudioHdr();
	Assert( hdr );
	if ( !hdr )
		return;

	for ( i = 0; i < NUM_PHONEME_CLASSES; i++ )
	{
		Emphasized_Phoneme *info = &g_PhonemeClasses[ i ];
		
		info->valid = false;
		info->exp = NULL;
		info->settings = NULL;
		info->amount = 0.0f;
		
		info->cl = expressions->FindClass( info->classname, true );
		if ( info->cl )
		{
			info->exp = info->cl->FindExpression( ConvertPhoneme( phoneme ) );
		}

		if ( info->required && ( !info->cl || !info->exp ) )
		{
			return;
		}

		if ( info->exp )
		{
			info->valid = true;

			info->settings = info->exp->GetSettings();
			Assert( info->settings );
		}
	}

	ComputeBlendedSetting( g_PhonemeClasses, intensity );

	// Look up the phoneme
	for ( LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++)
	{
		int j = hdr->pFlexcontroller( i )->localToGlobal;

		float add = 0.0f;

		for ( int k = 0 ; k < NUM_PHONEME_CLASSES; k++ )
		{
			Emphasized_Phoneme *info = &g_PhonemeClasses[ k ];
			if ( !info->valid || !info->amount )
				continue;

			add += info->amount * info->settings[ j ];
		}

		if ( add == 0.0f )
			continue;

		float curvalue = model->GetFlexController( i );
		curvalue += add * scale;
		model->SetFlexController( i, curvalue );
	}
}


#define PHONEME_FILTER 0.08f
#define PHONEME_DELAY  0.0f

void CFacePoserSound::SetupWeights( void )
{
	StudioModel *model;
	int c = models->Count();
	for ( int i = 0; i < c; i++ )
	{
		model = models->GetStudioModel( i );
		if ( !model )
			continue;

		// Reset flexes
		CStudioHdr *hdr = model->GetStudioHdr();
		if ( !hdr )
			continue;

		for ( int s = 0; s < model->m_mouth.GetNumVoiceSources(); s++ )
		{
			CVoiceData	*vd = model->m_mouth.GetVoiceSource( s );
			if ( !vd || vd->ShouldIgnorePhonemes() )
				continue;

			CAudioSource *source = vd->GetSource();
			// check for phoneme flexes
			if ( !source )
				continue;

			CAudioMixer *mixer = FindMixer( source );
			if ( !mixer )
				continue;

			CSentence *sentence = source->GetSentence();
			if ( !sentence )
				continue;

			// Zero faces if needed
			models->CheckResetFlexes();

			float pos = (float)mixer->GetScrubPosition();

			// Con_Printf( "pos %f for mixer %p\n", pos, mixer );

			float	soundtime = pos / source->SampleRate();

			float	t = soundtime - PHONEME_DELAY;
			float	dt = PHONEME_FILTER;

			float	sentence_duration = source->GetRunningLength();
			float	emphasis_intensity = sentence->GetIntensity( t, sentence_duration );

			if ( t > 0.0f )
			{
				for ( int w = 0 ; w < sentence->m_Words.Size(); w++ )
				{
					CWordTag *word = sentence->m_Words[ w ];
					if ( !word )
						continue;

					for ( int k = 0; k < word->m_Phonemes.Size(); k++)
					{
						CPhonemeTag *phoneme = word->m_Phonemes[ k ];
						if ( !phoneme )
							continue;

						// if the filter starts within this phoneme, make sure the filter size is 
						// at least least as long as the current phoneme, or until the end of the next phoneme, 
						// whichever is smaller
						if (t > phoneme->GetStartTime() && t < phoneme->GetEndTime())
						{
							CPhonemeTag *next = NULL;
							// try next phoneme, or first phoneme of next word
							if (k < word->m_Phonemes.Size()-1)
							{
								next = word->m_Phonemes[ k+1 ];
							}
							else if ( w < sentence->m_Words.Size() - 1  && sentence->m_Words[ w+1 ]->m_Phonemes.Size() )
							{
								next = sentence->m_Words[ w+1 ]->m_Phonemes[ 0 ];
							}

							// if I have a neighbor
							if (next)
							{
								// and they're touching
								if (next->GetStartTime() == phoneme->GetEndTime())
								{
									// no gap, so increase the blend length to the end of the next phoneme, as long as it's not longer than the current phoneme
									dt = max( dt, min( next->GetEndTime() - t, phoneme->GetEndTime() - phoneme->GetStartTime() ) );
								}
								else
								{
									// dead space, so increase the blend length to the start of the next phoneme, as long as it's not longer than the current phoneme
									dt = max( dt, min( next->GetStartTime() - t, phoneme->GetEndTime() - phoneme->GetStartTime() ) );
								}
							}
							else
							{
								// last phoneme in list, increase the blend length to the length of the current phoneme
								dt = max( dt, phoneme->GetEndTime() - phoneme->GetStartTime() );
							}
						}

						float t1 = ( phoneme->GetStartTime() - t) / dt;
						float t2 = ( phoneme->GetEndTime() - t) / dt;

						if (t1 < 1.0 && t2 > 0)
						{
							float scale;

							// clamp
							if (t2 > 1)
								t2 = 1;
							if (t1 < 0)
								t1 = 0;

							// FIXME: simple box filter.  Should use something fancier
							scale = (t2 - t1);

							AddViseme( emphasis_intensity, model, phoneme->GetPhonemeCode(), scale );
						}
					}
				}
				ProcessCloseCaptionData( model, t, sentence );
			}
		}
	}
}

static int g_nSoundFrameCount = 0;

void CFacePoserSound::ProcessCloseCaptionData( StudioModel *model, float curtime, CSentence* sentence )
{
//	closecaptionmanager->Process( g_nSoundFrameCount, model, curtime, sentence, GetCloseCaptionLanguageId() );
}

void CFacePoserSound::Update( float dt )
{
//	closecaptionmanager->PreProcess( g_nSoundFrameCount );

	if ( m_pAudio )
	{
		SetupWeights();
		m_pAudio->Update( m_flElapsedTime );
	}

//	closecaptionmanager->PostProcess( g_nSoundFrameCount, dt );

	m_flElapsedTime += dt;
	g_nSoundFrameCount++;
}

void CFacePoserSound::Flush( void )
{
	if ( m_pAudio )
	{
		m_pAudio->Flush();
	}
}

void CFacePoserSound::StopAll( void )
{
	int c = models->Count();
	for ( int i = 0; i < c; i++ )
	{
		StudioModel *model = models->GetStudioModel( i );
		if ( model )
		{
			model->m_mouth.ClearVoiceSources();
		}
	}

	if ( m_pAudio )
	{
		m_pAudio->StopSounds();
	}
}

void CFacePoserSound::StopSound( CAudioMixer *mixer )
{
	int idx = m_pAudio->FindSourceIndex( mixer );
	if ( idx != -1 )
	{
		m_pAudio->FreeChannel( idx );
	}
}

void CFacePoserSound::RenderWavToDC( HDC dc, RECT& outrect, COLORREF clr, 
	float starttime, float endtime, CAudioSource *pWave, 
	bool selected /*= false*/, int selectionstart /*= 0*/, int selectionend /*= 0*/ )
{
	channel_t channel;

	channel.leftvol = 127;
	channel.rightvol = 127;
	channel.pitch = 1.0;

	if ( !pWave )
		return;

	CAudioWaveOutput *pWaveOutput = ( CAudioWaveOutput * )m_pAudio;

	CAudioMixer *pMixer = pWave->CreateMixer();

	float timeperpixel = ( endtime - starttime ) / (float)( outrect.right - outrect.left );

	float samplesperpixel = timeperpixel * pWave->SampleRate();

	samplesperpixel = min( samplesperpixel, (float)PAINTBUFFER_SIZE );

	int intsamplesperpixel = (int)samplesperpixel;

	// Determine start/stop positions
	int totalsamples = (int)( pWave->GetRunningLength() * pWave->SampleRate() );

	if ( totalsamples <= 0 )
		return;

	float selectionstarttime = pWave->GetRunningLength() * ( float )selectionstart  / ( float )totalsamples;
	float selectionendtime = pWave->GetRunningLength() * ( float )selectionend  / ( float )totalsamples;


	HPEN oldPen, pen, pen2, pen3, pen4;

	pen = CreatePen( PS_SOLID, 1, RGB( 175, 175, 250 ) );
	pen2 = CreatePen( PS_SOLID, 1, clr );
	pen3 = CreatePen( PS_SOLID, 1, RGB( 127, 200, 249 ) );
	pen4 = CreatePen( PS_SOLID, 2, RGB( 0, 0, 200 ) );

	oldPen = (HPEN)SelectObject( dc, pen );

	MoveToEx( dc, outrect.left, ( outrect.bottom + outrect.top ) / 2, NULL );
	LineTo( dc, outrect.right, ( outrect.bottom + outrect.top ) / 2 );

	SelectObject( dc, pen2 );

	// Now iterate the samples
	float currenttime = 0.0f;
	int pixel = 0;
	int height = ( outrect.bottom - outrect.top ) / 2;
	int midy = ( outrect.bottom + outrect.top ) / 2;
	int bufferlen = ( intsamplesperpixel + 3 ) & ~3;
	short *samples = new short[ 2 * bufferlen ];
	bool drawingselection = false;
	int maxsamples = max( 32, intsamplesperpixel / 16 );
	int currentsample = 0;

	while ( currenttime < endtime )
	{

		pWaveOutput->m_audioDevice.MixBegin();

		int samplecount = min( maxsamples, intsamplesperpixel );

		if ( !pMixer->MixDataToDevice( &pWaveOutput->m_audioDevice, &channel, currentsample, samplecount, pWave->SampleRate(), true ) )
			break;

		currentsample = pMixer->GetSamplePosition();

		// Jump ahead by diff
		int diff = intsamplesperpixel - samplecount;
		if ( diff > 0 )
		{
			if ( !pMixer->SkipSamples( &channel, currentsample, diff, pWave->SampleRate(), true ) )
				break;
		}

		currentsample = pMixer->GetSamplePosition();

		pWaveOutput->m_audioDevice.TransferBufferStereo16( samples, samplecount );

		if ( currenttime >= starttime )
		{
			if ( selected )
			{
				bool boundary = false;
				bool inselection = ( currenttime >= selectionstarttime && 
					currenttime <= selectionendtime );

				if ( inselection )
				{
					if ( !drawingselection )
					{
						drawingselection = true;
						boundary = true;
					}
				}
				else if ( drawingselection )
				{
					boundary = true;
					drawingselection = false;
				}

				if ( inselection || boundary )
				{
					int top, bottom;

					bottom = outrect.bottom;

					HPEN *usePen;
					if ( boundary )
					{
						usePen = &pen4;
						top = outrect.top;
					}
					else
					{
						usePen = &pen3;
						top = outrect.bottom - 19;
					}

					HPEN old = (HPEN)SelectObject( dc, *usePen );
		
					MoveToEx( dc, outrect.left + pixel, top, NULL );
					LineTo( dc, outrect.left + pixel, bottom-1 );

					SelectObject( dc, old );
				}
			}


			int maxvalue = -65536;
			int minvalue = 65536;
			
			short *pData = samples;

			// only take fix samples
			int step = 2;
			int count = 2 * samplecount;

			for ( int i = 0; i < count; i+=step )
			{
				int val = (int)( pData[i] + pData[i+1] ) / 2;

				if ( val > maxvalue )
				{
					maxvalue = val;
				}
				
				if ( val < minvalue )
				{
					minvalue = val;
				}
			}

			float maxv = (float)( maxvalue ) / 32768.0f;
			float minv = (float)( minvalue ) / 32768.0f;

			MoveToEx( dc, outrect.left + pixel, midy + ( int ) ( maxv * height ), NULL );
			LineTo( dc, outrect.left + pixel, midy + ( int ) ( minv * height ) );

			pixel++;
		}
		currenttime += timeperpixel;
	}

	delete[] samples;

	SelectObject( dc, oldPen );
	DeleteObject( pen );
	DeleteObject( pen2 );
	DeleteObject( pen3 );

	delete pMixer;
}

bool CFacePoserSound::IsSoundPlaying( CAudioMixer *pMixer )
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

CAudioMixer *CFacePoserSound::FindMixer( CAudioSource *source )
{
	if ( !m_pAudio )
		return NULL;

	return m_pAudio->GetMixerForSource( source );
}


void CFacePoserSound::EnsureNoModelReferences( CAudioSource *source )
{
	int c = models->Count();
	for ( int i = 0; i < c; i++ )
	{
		StudioModel *model = models->GetStudioModel( i );
		if ( model->m_mouth.IsSourceReferenced( source ) )
		{
			model->m_mouth.RemoveSource( source );
		}
	}
}