//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "audio_pch.h"
#include "snd_mp3_source.h"
#include "snd_wave_mixer_mp3.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef DEDICATED  // have to test this because VPC is forcing us to compile this file.

extern IVAudio *vaudio;

CAudioMixerWaveMP3::CAudioMixerWaveMP3( IWaveData *data ) : CAudioMixerWave( data ) 
{
	m_sampleCount = 0;
	m_samplePosition = 0;
	m_offset = 0;
	m_delaySamples = 0;
	m_headerOffset = 0;
	m_pStream = NULL;
	m_bStreamInit = false;
	m_channelCount = 0;
}


CAudioMixerWaveMP3::~CAudioMixerWaveMP3( void )
{
	if ( m_pStream )
		delete m_pStream;
}


void CAudioMixerWaveMP3::Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress )
{
	Assert( IsReadyToMix() );
	if ( m_channelCount == 1 )
		pDevice->Mix16Mono( pChannel, (short *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress );
	else
		pDevice->Mix16Stereo( pChannel, (short *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress );
}


// Some MP3 files are wrapped in ID3
void CAudioMixerWaveMP3::GetID3HeaderOffset()
{
	char copyBuf[AUDIOSOURCE_COPYBUF_SIZE];
	byte *pData;

	int bytesRead = m_pData->ReadSourceData( (void **)&pData, 0, 10, copyBuf );
	if ( bytesRead < 10 )
		return;

	m_headerOffset = 0;
	if (( pData[ 0 ] == 0x49 ) &&
		( pData[ 1 ] == 0x44 ) &&
		( pData[ 2 ] == 0x33 ) &&
		( pData[ 3 ] < 0xff ) &&
		( pData[ 4 ] < 0xff ) &&
		( pData[ 6 ] < 0x80 ) &&
		( pData[ 7 ] < 0x80 ) &&
		( pData[ 8 ] < 0x80 ) &&
		( pData[ 9 ] < 0x80 ) )
	{
		// this is in id3 file
		// compute the size of the wrapper and skip it
		m_headerOffset = 10 + ( pData[9] | (pData[8]<<7) | (pData[7]<<14) | (pData[6]<<21) );
   }
}

int CAudioMixerWaveMP3::StreamRequestData( void *pBuffer, int bytesRequested, int offset )
{
	if ( offset < 0 )
	{
		offset = m_offset;
	}
	else
	{
		m_offset = offset;
	}
	// read the data out of the source
	int totalBytesRead = 0;

	if ( offset == 0 )
	{
		// top of file, check for ID3 wrapper
		GetID3HeaderOffset();
	}

	offset += m_headerOffset; // skip any id3 header/wrapper
	
	while ( bytesRequested > 0 )
	{
		char *pOutputBuffer = (char *)pBuffer;
		pOutputBuffer += totalBytesRead;

		void *pData = NULL;
		int bytesRead = m_pData->ReadSourceData( &pData, offset + totalBytesRead, bytesRequested, pOutputBuffer );
		
		if ( !bytesRead )
			break;
		if ( bytesRead > bytesRequested )
		{
			bytesRead = bytesRequested;
		}
		// if the source is buffering it, copy it to the MP3 decomp buffer
		if ( pData != pOutputBuffer )
		{
			memcpy( pOutputBuffer, pData, bytesRead );
		}
		totalBytesRead += bytesRead;
		bytesRequested -= bytesRead;
	}

	m_offset += totalBytesRead;
	return totalBytesRead;
}

bool CAudioMixerWaveMP3::DecodeBlock()
{
	IAudioStream *pStream = GetStream();
	if ( !pStream )
	{
		return false;
	}

	m_sampleCount = pStream->Decode( m_samples, sizeof(m_samples) );
	m_samplePosition = 0;
	return m_sampleCount > 0;
}

IAudioStream *CAudioMixerWaveMP3::GetStream()
{
	if ( !m_bStreamInit )
	{
		m_bStreamInit = true;

		if ( vaudio )
		{
			m_pStream = vaudio->CreateMP3StreamDecoder( static_cast<IAudioStreamEvent *>(this) );
		}
		else
		{
			Warning( "Attempting to play MP3 with no vaudio [ %s ]\n", m_pData->Source().GetFileName() );
		}

		if ( m_pStream )
		{
			m_channelCount = m_pStream->GetOutputChannels();
			//Assert( m_pStream->GetOutputRate() == m_pData->Source().SampleRate() );
		}

		if ( !m_pStream )
		{
			Warning( "Failed to create decoder for MP3 [ %s ]\n", m_pData->Source().GetFileName() );
		}
	}

	return m_pStream;
}

//-----------------------------------------------------------------------------
// Purpose: Read existing buffer or decompress a new block when necessary
// Input  : **pData - output data pointer
//			sampleCount - number of samples (or pairs)
// Output : int - available samples (zero to stop decoding)
//-----------------------------------------------------------------------------
int CAudioMixerWaveMP3::GetOutputData( void **pData, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	if ( m_samplePosition >= m_sampleCount )
	{
		if ( !DecodeBlock() )
			return 0;
	}

	IAudioStream *pStream = GetStream();
	if ( !pStream )
	{
		// Needed for channel count, and with a failed stream init we probably should fail to return data anyway.
		return 0;
	}

	if ( m_samplePosition < m_sampleCount )
	{
		int sampleSize = pStream->GetOutputChannels() * 2;
		*pData = (void *)(m_samples + m_samplePosition);
		int available = m_sampleCount - m_samplePosition;
		int bytesRequired = sampleCount * sampleSize;
		if ( available > bytesRequired )
			available = bytesRequired;

		m_samplePosition += available;

		int samples_loaded = available / sampleSize;

		// update count of max samples loaded in CAudioMixerWave

		CAudioMixerWave::m_sample_max_loaded += samples_loaded;

		// update index of last sample loaded

		CAudioMixerWave::m_sample_loaded_index += samples_loaded;

		return samples_loaded;
	}

	return 0;
}



//-----------------------------------------------------------------------------
// Purpose: Seek to a new position in the file
//			NOTE: In most cases, only call this once, and call it before playing
//			any data.
// Input  : newPosition - new position in the sample clocks of this sample
//-----------------------------------------------------------------------------
void CAudioMixerWaveMP3::SetSampleStart( int newPosition )
{
	// UNDONE: Implement this?
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : delaySamples - 
//-----------------------------------------------------------------------------
void CAudioMixerWaveMP3::SetStartupDelaySamples( int delaySamples )
{
	m_delaySamples = delaySamples;
}

#endif
