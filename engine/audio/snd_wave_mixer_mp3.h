//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Mixer for ADPCM encoded audio
//
//=============================================================================//

#ifndef SND_WAVE_MIXER_MP3_H
#define SND_WAVE_MIXER_MP3_H
#pragma once

#include "vaudio/ivaudio.h"

static const int MP3_BUFFER_SIZE = 16384;

class CAudioMixerWaveMP3 : public CAudioMixerWave, public IAudioStreamEvent
{
public:
	CAudioMixerWaveMP3( IWaveData *data );
	~CAudioMixerWaveMP3( void );

	virtual void Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress );
	virtual int	 GetOutputData( void **pData, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );

	// need to override this to fixup blocks
	// UNDONE: This doesn't quite work with MP3 - we need a MP3 position, not a sample position
	void SetSampleStart( int newPosition );

	int GetPositionForSave() { return GetStream() ? GetStream()->GetPosition() : 0; }
	void SetPositionFromSaved(int position) { if ( GetStream() ) GetStream()->SetPosition(position); }

	// IAudioStreamEvent
	virtual int StreamRequestData( void *pBuffer, int bytesRequested, int offset );

	virtual void SetStartupDelaySamples( int delaySamples );
	virtual int GetMixSampleSize() { return CalcSampleSize( 16, m_channelCount ); }

	virtual int GetStreamOutputRate() { return GetStream() ? GetStream()->GetOutputRate() : 0; }

private:
	IAudioStream			*GetStream();
	bool					DecodeBlock( void );
	void					GetID3HeaderOffset();

	// Lazily initialized, use GetStream
	IAudioStream			*m_pStream;
	bool					m_bStreamInit;

	char					m_samples[MP3_BUFFER_SIZE];
	int						m_sampleCount;
	int						m_samplePosition;
	int						m_channelCount;
	int						m_offset;
	int						m_delaySamples;
	int						m_headerOffset;
};

CAudioMixerWaveMP3 *CreateMP3Mixer( IWaveData *data );

#endif // SND_WAVE_MIXER_MP3_H
