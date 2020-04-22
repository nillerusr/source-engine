//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "snd_dev_wave.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#pragma warning( disable: 4201 )
#include <mmsystem.h>
#pragma warning( default: 4201 )
#include <stdio.h>
#include <math.h>
#include "soundsystem/snd_audio_source.h"
#include "soundsystem.h"
#include "soundsystem/snd_device.h"
#include "tier1/utlvector.h"
#include "filesystem.h"
#include "sentence.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CAudioMixer;


//-----------------------------------------------------------------------------
// Important constants
//-----------------------------------------------------------------------------

// 64K is > 1 second at 16-bit, 22050 Hz
// 44k: UNDONE - need to double buffers now that we're playing back at 44100?
#define OUTPUT_CHANNEL_COUNT	2
#define BYTES_PER_SAMPLE		2
#define OUTPUT_SAMPLE_RATE		SOUND_DMA_SPEED
#define OUTPUT_BUFFER_COUNT		64
#define OUTPUT_BUFFER_MASK		0x3F
#define OUTPUT_BUFFER_SAMPLE_COUNT	(OUTPUT_BUFFER_SIZE_BYTES / BYTES_PER_SAMPLE)
#define OUTPUT_BUFFER_SIZE_BYTES	1024
#define	PAINTBUFFER_SIZE		1024
#define MAX_CHANNELS			16


//-----------------------------------------------------------------------------
// Implementation of IAudioDevice for WAV files
//-----------------------------------------------------------------------------
class CAudioDeviceWave : public IAudioDevice
{
public:
	// Inherited from IAudioDevice
	virtual bool	Init( void );
	virtual void	Shutdown( void );
	virtual const char *DeviceName( void ) const;
	virtual int		DeviceChannels( void ) const;
	virtual int		DeviceSampleBits( void ) const;
	virtual int		DeviceSampleBytes( void ) const;
	virtual int		DeviceSampleRate( void ) const;
	virtual int		DeviceSampleCount( void ) const;
	virtual void	Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress, bool forward = true );
	virtual void	Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress, bool forward = true );
	virtual void	Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress, bool forward = true );
	virtual void	Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress, bool forward = true );
	virtual int		PaintBufferSampleCount( void ) const;
	virtual void	MixBegin( void );

	// mix a buffer up to time (time is absolute)
	void			Update( float time );
	void			Flush( void );
	void			TransferBufferStereo16( short *pOutput, int sampleCount );
	int				GetOutputPosition( void );
	float			GetAmountofTimeAhead( void );
	int				GetNumberofSamplesAhead( void );
	void			AddSource( CAudioMixer *pSource );
	void			StopSounds( void );
	int				FindSourceIndex( CAudioMixer *pSource );
	CAudioMixer		*GetMixerForSource( CAudioSource *source );

private:
	class CAudioMixerState
	{
	public:
		CAudioMixer		*mixer;
	};

	class CAudioBuffer
	{
	public:
		WAVEHDR			*hdr;
		bool			submitted;
		int				submit_sample_count;

		CUtlVector< CAudioMixerState > m_Referenced;
	};

	struct portable_samplepair_t
	{
		int left;
		int	right;
	};

	void	OpenWaveOut( void );
	void	CloseWaveOut( void );
	void	AllocateOutputBuffers();
	void	FreeOutputBuffers();
	void*	AllocOutputMemory( int nSize, HGLOBAL &hMemory );
	void	FreeOutputMemory( HGLOBAL &hMemory );

	bool	ValidWaveOut( void ) const;
	CAudioBuffer *GetEmptyBuffer( void );
	void	SilenceBuffer( short *pSamples, int sampleCount );

	void	SetChannel( int channelIndex, CAudioMixer *pSource );
	void	FreeChannel( int channelIndex );

	void	RemoveMixerChannelReferences( CAudioMixer *mixer );
	void	AddToReferencedList( CAudioMixer *mixer, CAudioBuffer *buffer );
	void	RemoveFromReferencedList( CAudioMixer *mixer, CAudioBuffer *buffer );
	bool	IsSourceReferencedByActiveBuffer( CAudioMixer *mixer );
	bool	IsSoundInReferencedList( CAudioMixer *mixer, CAudioBuffer *buffer );

	// Compute how many samples we've mixed since most recent buffer submission
	void	ComputeSampleAheadAmount( void );

	// This is a single allocation for all wave headers (there are OUTPUT_BUFFER_COUNT of them)
	HGLOBAL			m_hWaveHdr;

	// This is a single allocation for all wave data (there are OUTPUT_BUFFER_COUNT of them)
	HANDLE			m_hWaveData;

	HWAVEOUT		m_waveOutHandle;
	float			m_mixTime;
	float			m_baseTime;
	int				m_sampleIndex;
	CAudioBuffer	m_buffers[ OUTPUT_BUFFER_COUNT ];
	CAudioMixer		*m_sourceList[MAX_CHANNELS];
	int				m_nEstimatedSamplesAhead;

	portable_samplepair_t		m_paintbuffer[ PAINTBUFFER_SIZE ];
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
IAudioDevice *Audio_CreateWaveDevice( void )
{
	return new CAudioDeviceWave;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CAudioDeviceWave::Init( void )
{
	m_hWaveData = NULL;
	m_hWaveHdr = NULL;
	m_waveOutHandle = NULL;

	for ( int i = 0; i < OUTPUT_BUFFER_COUNT; i++ )
	{
		CAudioBuffer *buffer = &m_buffers[ i ];
		Assert( buffer );
		buffer->hdr = NULL;
		buffer->submitted = false;
		buffer->submit_sample_count = false;
	}

	OpenWaveOut();

	m_mixTime = m_baseTime = -1;
	m_sampleIndex = 0;
	memset( m_sourceList, 0, sizeof(m_sourceList) );

	m_nEstimatedSamplesAhead = (int)( ( float ) OUTPUT_SAMPLE_RATE / 10.0f );

	return true;
}

void CAudioDeviceWave::Shutdown( void )
{
	CloseWaveOut();
}

	
//-----------------------------------------------------------------------------
// WAV out device
//-----------------------------------------------------------------------------
inline bool CAudioDeviceWave::ValidWaveOut( void ) const 
{ 
	return m_waveOutHandle != 0; 
}


//-----------------------------------------------------------------------------
// Opens the windows wave out device
//-----------------------------------------------------------------------------
void CAudioDeviceWave::OpenWaveOut( void )
{
	WAVEFORMATEX waveFormat;
	memset( &waveFormat, 0, sizeof(waveFormat) );

	// Select a PCM, 16-bit stereo playback device
	waveFormat.cbSize = sizeof(waveFormat);
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nChannels = DeviceChannels();
	waveFormat.wBitsPerSample = DeviceSampleBits();
	waveFormat.nSamplesPerSec = DeviceSampleRate();
	waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign; 

	MMRESULT errorCode = waveOutOpen( &m_waveOutHandle, WAVE_MAPPER, &waveFormat, 0, 0L, CALLBACK_NULL );
	while ( errorCode != MMSYSERR_NOERROR )
	{
		if ( errorCode != MMSYSERR_ALLOCATED )
		{
			DWarning( "soundsystem", 1, "waveOutOpen failed\n" );
			m_waveOutHandle = 0;
			return;
		}

		int nRetVal = MessageBox( NULL,
			"The sound hardware is in use by another app.\n\n"
			"Select Retry to try to start sound again or Cancel to run with no sound.",
			"Sound not available",
			MB_RETRYCANCEL | MB_SETFOREGROUND | MB_ICONEXCLAMATION);

		if ( nRetVal != IDRETRY )
		{
			DWarning( "soundsystem", 1, "waveOutOpen failure--hardware already in use\n" );
			m_waveOutHandle = 0;
			return;
		}

		errorCode = waveOutOpen( &m_waveOutHandle, WAVE_MAPPER, &waveFormat, 0, 0L, CALLBACK_NULL );
	}

	AllocateOutputBuffers();
}


//-----------------------------------------------------------------------------
// Closes the windows wave out device
//-----------------------------------------------------------------------------
void CAudioDeviceWave::CloseWaveOut( void ) 
{ 
	if ( ValidWaveOut() )
	{
		waveOutReset( m_waveOutHandle );
		FreeOutputBuffers();
		waveOutClose( m_waveOutHandle );
		m_waveOutHandle = NULL; 
	}
}


//-----------------------------------------------------------------------------
// Alloc output memory
//-----------------------------------------------------------------------------
void* CAudioDeviceWave::AllocOutputMemory( int nSize, HGLOBAL &hMemory )
{
	// Output memory for waveform data+hdrs must be 
	// globally allocated with GMEM_MOVEABLE and GMEM_SHARE flags.
	hMemory = GlobalAlloc( GMEM_MOVEABLE | GMEM_SHARE, nSize ); 
	if ( !hMemory ) 
	{ 
		DWarning( "soundsystem", 1, "Sound: Out of memory.\n");
		CloseWaveOut();
		return NULL;
	}

	HPSTR lpData = (char *)GlobalLock( hMemory );
	if ( !lpData )
	{ 
		DWarning( "soundsystem", 1, "Sound: Failed to lock.\n");
		GlobalFree( hMemory );
		hMemory = NULL;
		CloseWaveOut();
		return NULL;
	} 
	memset( lpData, 0, nSize );
	return lpData;
}


//-----------------------------------------------------------------------------
// Free output memory
//-----------------------------------------------------------------------------
void CAudioDeviceWave::FreeOutputMemory( HGLOBAL &hMemory )
{
	if ( hMemory )
	{
		GlobalUnlock( hMemory ); 
		GlobalFree( hMemory );
		hMemory = NULL;
	}
}


//-----------------------------------------------------------------------------
// Allocate, free output buffers
//-----------------------------------------------------------------------------
void CAudioDeviceWave::AllocateOutputBuffers()
{
	// Allocate and lock memory for the waveform data.  
	int nBufferSize = OUTPUT_BUFFER_SIZE_BYTES * OUTPUT_BUFFER_COUNT;
	HPSTR lpData = (char *)AllocOutputMemory( nBufferSize, m_hWaveData );
	if ( !lpData )
		return;

	// Allocate and lock memory for the waveform header
	int nHdrSize = sizeof( WAVEHDR ) * OUTPUT_BUFFER_COUNT;
	LPWAVEHDR lpWaveHdr = (LPWAVEHDR)AllocOutputMemory( nHdrSize, m_hWaveHdr );
	if ( !lpWaveHdr )
		return;

	// After allocation, set up and prepare headers.
	for ( int i=0 ; i < OUTPUT_BUFFER_COUNT; i++ )
	{
		LPWAVEHDR lpHdr = lpWaveHdr + i;
		lpHdr->dwBufferLength = OUTPUT_BUFFER_SIZE_BYTES; 
		lpHdr->lpData = lpData + (i * OUTPUT_BUFFER_SIZE_BYTES);

		MMRESULT nResult = waveOutPrepareHeader( m_waveOutHandle, lpHdr, sizeof(WAVEHDR) );
		if ( nResult != MMSYSERR_NOERROR )
		{
			DWarning( "soundsystem", 1, "Sound: failed to prepare wave headers\n" );
			CloseWaveOut();
			return;
		}

		m_buffers[i].hdr = lpHdr;
	}
}


void CAudioDeviceWave::FreeOutputBuffers()
{
	// Unprepare headers.
	for ( int i=0 ; i < OUTPUT_BUFFER_COUNT; i++ )
	{
		if ( m_buffers[i].hdr )
		{
			waveOutUnprepareHeader( m_waveOutHandle, m_buffers[i].hdr, sizeof(WAVEHDR) );
			m_buffers[i].hdr = NULL;
		}

		m_buffers[i].submitted = false;
		m_buffers[i].submit_sample_count = 0;
		m_buffers[i].m_Referenced.Purge();
	}

	FreeOutputMemory( m_hWaveData );
	FreeOutputMemory( m_hWaveHdr );
}

	
//-----------------------------------------------------------------------------
// Device parameters
//-----------------------------------------------------------------------------
const char *CAudioDeviceWave::DeviceName( void ) const			
{ 
	return "Windows WAVE"; 
}

int CAudioDeviceWave::DeviceChannels( void ) const		
{ 
	return 2; 
}

int CAudioDeviceWave::DeviceSampleBits( void ) const	
{ 
	return (BYTES_PER_SAMPLE * 8); 
}

int CAudioDeviceWave::DeviceSampleBytes( void ) const	
{ 
	return BYTES_PER_SAMPLE; 
}

int CAudioDeviceWave::DeviceSampleRate( void ) const		
{ 
	return OUTPUT_SAMPLE_RATE; 
}

int CAudioDeviceWave::DeviceSampleCount( void )	const
{ 
	return OUTPUT_BUFFER_SAMPLE_COUNT; 
}

int CAudioDeviceWave::PaintBufferSampleCount( void ) const
{
	return PAINTBUFFER_SIZE;
}


//-----------------------------------------------------------------------------
// Mixing routines
//-----------------------------------------------------------------------------
void CAudioDeviceWave::Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress, bool forward )
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


void CAudioDeviceWave::Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress, bool forward )
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


void CAudioDeviceWave::Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress, bool forward )
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


void CAudioDeviceWave::Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress, bool forward )
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


void CAudioDeviceWave::MixBegin( void )
{
	memset( m_paintbuffer, 0, sizeof(m_paintbuffer) );
}

void CAudioDeviceWave::TransferBufferStereo16( short *pOutput, int sampleCount )
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

void CAudioDeviceWave::RemoveMixerChannelReferences( CAudioMixer *mixer )
{
	for ( int i = 0; i < OUTPUT_BUFFER_COUNT; i++ )
	{
		RemoveFromReferencedList( mixer, &m_buffers[ i ] );
	}
}

void CAudioDeviceWave::AddToReferencedList( CAudioMixer *mixer, CAudioBuffer *buffer )
{
	// Already in list
	for ( int i = 0; i < buffer->m_Referenced.Size(); i++ )
	{
		if ( buffer->m_Referenced[ i ].mixer == mixer )
			return;
	}

	// Just remove it
	int idx = buffer->m_Referenced.AddToTail();

	CAudioMixerState *state = &buffer->m_Referenced[ idx ];
	state->mixer = mixer;
}

void CAudioDeviceWave::RemoveFromReferencedList( CAudioMixer *mixer, CAudioBuffer *buffer )
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

bool CAudioDeviceWave::IsSoundInReferencedList( CAudioMixer *mixer, CAudioBuffer *buffer )
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

bool CAudioDeviceWave::IsSourceReferencedByActiveBuffer( CAudioMixer *mixer )
{
	if ( !ValidWaveOut() )
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

CAudioDeviceWave::CAudioBuffer *CAudioDeviceWave::GetEmptyBuffer( void )
{
	CAudioBuffer *pOutput = NULL;
	if ( ValidWaveOut() )
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

void CAudioDeviceWave::SilenceBuffer( short *pSamples, int sampleCount )
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

void CAudioDeviceWave::Flush( void )
{
	waveOutReset( m_waveOutHandle );
}

// mix a buffer up to time (time is absolute)
void CAudioDeviceWave::Update( float time )
{
	if ( !ValidWaveOut() )
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
			if ( tempCount > PaintBufferSampleCount() )
			{
				sampleCount = PaintBufferSampleCount();
			}
			else
			{
				sampleCount = tempCount;
			}

			MixBegin();
			for ( int i = 0; i < MAX_CHANNELS; i++ )
			{
				CAudioMixer *pSource = m_sourceList[i];
				if ( !pSource )
					continue;

				int currentsample = pSource->GetSamplePosition();
				bool forward = pSource->GetDirection();

				if ( pSource->GetActive() )
				{
					if ( !pSource->MixDataToDevice( this, pSource->GetChannel(), currentsample, sampleCount, DeviceSampleRate(), forward ) )
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
						pSource->IncrementSamples( pSource->GetChannel(), currentsample, sampleCount, DeviceSampleRate(), forward );
					}
				}

			}

			TransferBufferStereo16( pSamples, sampleCount );

			m_sampleIndex += sampleCount;
			tempCount -= sampleCount;
			pSamples += sampleCount * 2;
		}
		// if the buffers aren't aligned on sample boundaries, this will hard-lock the machine!

		pBuffer->submit_sample_count = GetOutputPosition();

		waveOutWrite( m_waveOutHandle, pBuffer->hdr, sizeof(*(pBuffer->hdr)) );
	}
}

/*
int CAudioDeviceWave::GetNumberofSamplesAhead( void )
{
	ComputeSampleAheadAmount();
	return m_nEstimatedSamplesAhead;
}

float CAudioDeviceWave::GetAmountofTimeAhead( void )
{
	ComputeSampleAheadAmount();
	return ( (float)m_nEstimatedSamplesAhead / (float)OUTPUT_SAMPLE_RATE );
}

// Find the most recent submitted sample that isn't flagged as whdr_done
void CAudioDeviceWave::ComputeSampleAheadAmount( void )
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
*/

int CAudioDeviceWave::FindSourceIndex( CAudioMixer *pSource )
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

CAudioMixer *CAudioDeviceWave::GetMixerForSource( CAudioSource *source )
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

void CAudioDeviceWave::AddSource( CAudioMixer *pSource )
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


void CAudioDeviceWave::StopSounds( void )
{
	for ( int i = 0; i < MAX_CHANNELS; i++ )
	{
		if ( m_sourceList[i] )
		{
			FreeChannel( i );
		}
	}
}


void CAudioDeviceWave::SetChannel( int channelIndex, CAudioMixer *pSource )
{
	if ( channelIndex < 0 || channelIndex >= MAX_CHANNELS )
		return;

	m_sourceList[channelIndex] = pSource;
}

void CAudioDeviceWave::FreeChannel( int channelIndex )
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

int CAudioDeviceWave::GetOutputPosition( void )
{
	if ( !m_waveOutHandle )
		return 0;

	MMTIME mmtime;
	mmtime.wType = TIME_SAMPLES;
	waveOutGetPosition( m_waveOutHandle, &mmtime, sizeof( MMTIME ) );

	// Convert time to sample count
	return ( mmtime.u.sample );
}

