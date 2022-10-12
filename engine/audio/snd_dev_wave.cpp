//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "audio_pch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern bool snd_firsttime;
extern bool MIX_ScaleChannelVolume( paintbuffer_t *ppaint, channel_t *pChannel, int volume[CCHANVOLUMES], int mixchans );
extern void S_SpatializeChannel( int volume[6], int master_vol, const Vector *psourceDir, float gain, float mono );

// 64K is > 1 second at 16-bit, 22050 Hz
// 44k: UNDONE - need to double buffers now that we're playing back at 44100?
#define	WAV_BUFFERS				64
#define	WAV_MASK				0x3F
#define	WAV_BUFFER_SIZE			0x0400


//-----------------------------------------------------------------------------
//
// NOTE: This only allows 16-bit, stereo wave out
//
//-----------------------------------------------------------------------------
class CAudioDeviceWave : public CAudioDeviceBase
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

	const char *DeviceName( void )			{ return "Windows WAVE"; }
	int			DeviceChannels( void )		{ return 2; }
	int			DeviceSampleBits( void )	{ return 16; }
	int			DeviceSampleBytes( void )	{ return 2; }
	int			DeviceDmaSpeed( void )		{ return SOUND_DMA_SPEED; }
	int			DeviceSampleCount( void )	{ return m_deviceSampleCount; }

private:
	void	OpenWaveOut( void );
	void	CloseWaveOut( void );
	void	AllocateOutputBuffers();
	void	FreeOutputBuffers();
	void*	AllocOutputMemory( int nSize, HGLOBAL &hMemory );
	void	FreeOutputMemory( HGLOBAL &hMemory );
	bool	ValidWaveOut( void ) const;

	int			m_deviceSampleCount;

	int			m_buffersSent;
	int			m_buffersCompleted;
	int			m_pauseCount;

	// This is a single allocation for all wave headers (there are OUTPUT_BUFFER_COUNT of them)
	HGLOBAL		m_hWaveHdr;

	// This is a single allocation for all wave data (there are OUTPUT_BUFFER_COUNT of them)
	HANDLE		m_hWaveData;

	HWAVEOUT	m_waveOutHandle;

	// Memory for the wave data + wave headers
	void		*m_pBuffer;
	LPWAVEHDR	m_pWaveHdr;
};


//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------
IAudioDevice *Audio_CreateWaveDevice( void )
{
	CAudioDeviceWave *wave = NULL;
	if ( !wave )
	{
		wave = new CAudioDeviceWave;
	}

	if ( wave->Init() )
		return wave;

	delete wave;
	wave = NULL;

	return NULL;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CAudioDeviceWave::Init( void )
{
	m_bSurround = false;
	m_bSurroundCenter = false;
	m_bHeadphone = false;
	m_buffersSent = 0;
	m_buffersCompleted = 0;
	m_pauseCount = 0;
	m_waveOutHandle = 0;
	m_pBuffer = NULL;
	m_pWaveHdr = NULL;
	m_hWaveHdr = NULL;
	m_hWaveData = NULL;

	OpenWaveOut();

	if ( snd_firsttime )
	{
		DevMsg( "Wave sound initialized\n" );
	}
	return ValidWaveOut();
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
	waveFormat.nSamplesPerSec = DeviceDmaSpeed(); // DeviceSampleRate
	waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign; 

	MMRESULT errorCode = waveOutOpen( &m_waveOutHandle, WAVE_MAPPER, &waveFormat, 0, 0L, CALLBACK_NULL );
	while ( errorCode != MMSYSERR_NOERROR )
	{
		if ( errorCode != MMSYSERR_ALLOCATED )
		{
			DevWarning( "waveOutOpen failed\n" );
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
			DevWarning( "waveOutOpen failure--hardware already in use\n" );
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
		DevWarning( "Sound: Out of memory.\n");
		CloseWaveOut();
		return NULL;
	}

	HPSTR lpData = (char *)GlobalLock( hMemory );
	if ( !lpData )
	{ 
		DevWarning( "Sound: Failed to lock.\n");
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
// Allocate output buffers
//-----------------------------------------------------------------------------
void CAudioDeviceWave::AllocateOutputBuffers()
{
	// Allocate and lock memory for the waveform data.  
	int nBufferSize = WAV_BUFFER_SIZE * WAV_BUFFERS;
	HPSTR lpData = (char *)AllocOutputMemory( nBufferSize, m_hWaveData );
	if ( !lpData )
		return;

	// Allocate and lock memory for the waveform header
	int nHdrSize = sizeof( WAVEHDR ) * WAV_BUFFERS;
	LPWAVEHDR lpWaveHdr = (LPWAVEHDR)AllocOutputMemory( nHdrSize, m_hWaveHdr );
	if ( !lpWaveHdr )
		return;

	// After allocation, set up and prepare headers.
	for ( int i=0 ; i < WAV_BUFFERS; i++ )
	{
		LPWAVEHDR lpHdr = lpWaveHdr + i;
		lpHdr->dwBufferLength = WAV_BUFFER_SIZE; 
		lpHdr->lpData = lpData + (i * WAV_BUFFER_SIZE);

		MMRESULT nResult = waveOutPrepareHeader( m_waveOutHandle, lpHdr, sizeof(WAVEHDR) );
		if ( nResult != MMSYSERR_NOERROR )
		{
			DevWarning( "Sound: failed to prepare wave headers\n" );
			CloseWaveOut();
			return;
		}
	}

	m_deviceSampleCount = nBufferSize / DeviceSampleBytes();
	
	m_pBuffer = (void *)lpData;
	m_pWaveHdr = lpWaveHdr;
}


//-----------------------------------------------------------------------------
// Free output buffers
//-----------------------------------------------------------------------------
void CAudioDeviceWave::FreeOutputBuffers()
{
	// Unprepare headers.
	if ( m_pWaveHdr )
	{
		for ( int i=0 ; i < WAV_BUFFERS; i++ )
		{
			waveOutUnprepareHeader( m_waveOutHandle, m_pWaveHdr+i, sizeof(WAVEHDR) );
		}
	}
	m_pWaveHdr = NULL;
	m_pBuffer = NULL;

	FreeOutputMemory( m_hWaveData );
	FreeOutputMemory( m_hWaveHdr );
}


//-----------------------------------------------------------------------------
// Mixing setup
//-----------------------------------------------------------------------------
int CAudioDeviceWave::PaintBegin( float mixAheadTime, int soundtime, int paintedtime )
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


//-----------------------------------------------------------------------------
// Actually performs the mixing
//-----------------------------------------------------------------------------
void CAudioDeviceWave::PaintEnd( void )
{
	LPWAVEHDR	h;
	int			wResult;
	int			cblocks;

	//
	// find which sound blocks have completed
	//
	while (1)
	{
		if ( m_buffersCompleted == m_buffersSent )
		{
			//DevMsg ("Sound overrun\n");
			break;
		}

		if ( ! (m_pWaveHdr[ m_buffersCompleted & WAV_MASK].dwFlags & WHDR_DONE) )
		{
			break;
		}

		m_buffersCompleted++;	// this buffer has been played
	}

	//
	// submit a few new sound blocks
	//
	// 22K sound support
	// 44k: UNDONE - double blocks out now that we're at 44k playback? 
	cblocks = 4 << 1; 

	while (((m_buffersSent - m_buffersCompleted) >> SAMPLE_16BIT_SHIFT) < cblocks)
	{
		h = m_pWaveHdr + ( m_buffersSent&WAV_MASK );

		m_buffersSent++;
		/* 
		 * Now the data block can be sent to the output device. The 
		 * waveOutWrite function returns immediately and waveform 
		 * data is sent to the output device in the background. 
		 */ 
		wResult = waveOutWrite( m_waveOutHandle, h, sizeof(WAVEHDR) ); 

		if (wResult != MMSYSERR_NOERROR)
		{ 
			Warning( "Failed to write block to device\n");
			Shutdown();
			return; 
		} 
	}
}

int CAudioDeviceWave::GetOutputPosition( void )
{
	int s = m_buffersSent * WAV_BUFFER_SIZE;

	s >>= SAMPLE_16BIT_SHIFT;

	s &= (DeviceSampleCount()-1);

	return s / DeviceChannels();
}


//-----------------------------------------------------------------------------
// Pausing
//-----------------------------------------------------------------------------
void CAudioDeviceWave::Pause( void )
{
	m_pauseCount++;
	if (m_pauseCount == 1)
	{
		waveOutReset( m_waveOutHandle );
	}
}


void CAudioDeviceWave::UnPause( void )
{
	if ( m_pauseCount > 0 )
	{
		m_pauseCount--;
	}
}

bool CAudioDeviceWave::IsActive( void )
{
	return ( m_pauseCount == 0 );
}

float CAudioDeviceWave::MixDryVolume( void )
{
	return 0;
}


bool CAudioDeviceWave::Should3DMix( void )
{
	return false;
}


void CAudioDeviceWave::ClearBuffer( void )
{
	int		clear;

	if ( !m_pBuffer )
		return;

	clear = 0;

	Q_memset(m_pBuffer, clear, DeviceSampleCount() * DeviceSampleBytes() );
}

void CAudioDeviceWave::UpdateListener( const Vector& position, const Vector& forward, const Vector& right, const Vector& up )
{
}


void CAudioDeviceWave::MixBegin( int sampleCount )
{
	MIX_ClearAllPaintBuffers( sampleCount, false );
}


void CAudioDeviceWave::MixUpsample( int sampleCount, int filtertype )
{
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();
	int ifilter = ppaint->ifilter;
	
	Assert (ifilter < CPAINTFILTERS);

	S_MixBufferUpsample2x( sampleCount, ppaint->pbuf, &(ppaint->fltmem[ifilter][0]), CPAINTFILTERMEM, filtertype );

	ppaint->ifilter++;
}

void CAudioDeviceWave::Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();

	if (!MIX_ScaleChannelVolume( ppaint, pChannel, volume, 1))
		return;

	Mix8MonoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, (byte *)pData, inputOffset, rateScaleFix, outCount );
}


void CAudioDeviceWave::Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();

	if (!MIX_ScaleChannelVolume( ppaint, pChannel, volume, 2 ))
		return;

	Mix8StereoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, (byte *)pData, inputOffset, rateScaleFix, outCount );
}


void CAudioDeviceWave::Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();

	if (!MIX_ScaleChannelVolume( ppaint, pChannel, volume, 1 ))
		return;

	Mix16MonoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, pData, inputOffset, rateScaleFix, outCount );
}


void CAudioDeviceWave::Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();

	if (!MIX_ScaleChannelVolume( ppaint, pChannel, volume, 2 ))
		return;

	Mix16StereoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, pData, inputOffset, rateScaleFix, outCount );
}


void CAudioDeviceWave::ChannelReset( int entnum, int channelIndex, float distanceMod )
{
}


void CAudioDeviceWave::TransferSamples( int end )
{
	int		lpaintedtime = g_paintedtime;
	int		endtime = end;
	
	// resumes playback...

	if ( m_pBuffer )
	{
		S_TransferStereo16( m_pBuffer, PAINTBUFFER, lpaintedtime, endtime );
	}
}

void CAudioDeviceWave::SpatializeChannel( int volume[CCHANVOLUMES/2], int master_vol, const Vector& sourceDir, float gain, float mono )
{
	VPROF("CAudioDeviceWave::SpatializeChannel");
	S_SpatializeChannel( volume, master_vol, &sourceDir, gain, mono );
}

void CAudioDeviceWave::StopAllSounds( void )
{
}


void CAudioDeviceWave::ApplyDSPEffects( int idsp, portable_samplepair_t *pbuffront, portable_samplepair_t *pbufrear, portable_samplepair_t *pbufcenter, int samplecount )
{
	//SX_RoomFX( endtime, filter, timefx );
	DSP_Process( idsp, pbuffront, pbufrear, pbufcenter, samplecount );
}
