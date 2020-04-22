//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef AUDIOWAVEOUTPUT_H
#define AUDIOWAVEOUTPUT_H
#ifdef _WIN32
#pragma once
#endif

#include "sound.h"
#include "utlvector.h"

#define OUTPUT_BUFFER_COUNT		32
#define MAX_CHANNELS			16

class CAudioMixer;

class CAudioMixerState
{
public:
	CAudioMixer		*mixer;
	int				submit_mixer_sample;
};

class CAudioBuffer
{
public:
	WAVEHDR			*hdr;
	bool			submitted;
	int				submit_sample_count;

	CUtlVector< CAudioMixerState > m_Referenced;
};

#define OUTPUT_SAMPLE_RATE		44100
#define	PAINTBUFFER_SIZE		1024

typedef struct
{
	int left;
	int	right;
} portable_samplepair_t;

class CAudioDeviceSWMix : public IAudioDevice
{
public:
	virtual void	Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, int rateScaleFix, int outCount, int timecompress, bool forward = true );
	virtual void	Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, int rateScaleFix, int outCount, int timecompress, bool forward = true );
	virtual void	Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, int rateScaleFix, int outCount, int timecompress, bool forward = true );
	virtual void	Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, int rateScaleFix, int outCount, int timecompress, bool forward = true );
	virtual int		MaxSampleCount( void );
	virtual void	MixBegin( void );

	void			TransferBufferStereo16( short *pOutput, int sampleCount );

private:
	portable_samplepair_t		m_paintbuffer[ PAINTBUFFER_SIZE ];
};

class CAudioWaveOutput : public CAudioOutput
{
public:
	CAudioWaveOutput( void );
	~CAudioWaveOutput( void );

	// returns the size of each sample in bytes
	virtual int SampleSize( void ) { return 2; }
	
	// returns the sampling rate of the data
	virtual int SampleRate( void ) { return OUTPUT_SAMPLE_RATE; }

	// returns the mono/stereo status of this device (true if stereo)
	virtual bool IsStereo( void ) { return true; }

	// mix a buffer up to time (time is absolute)
	virtual void Update( float time );

	virtual void Flush( void );

	virtual void AddSource( CAudioMixer *pSource );
	virtual void StopSounds( void );
	virtual int FindSourceIndex( CAudioMixer *pSource );

	virtual int GetOutputPosition( void );
	virtual float GetAmountofTimeAhead( void );
	virtual int GetNumberofSamplesAhead( void );

	virtual CAudioMixer *GetMixerForSource( CAudioSource *pSource );


private:
	void	OpenDevice( void );
	bool	ValidDevice( void ) { return m_deviceHandle != 0; }
	void	ClearDevice( void ) { m_deviceHandle = NULL; }
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

	HWAVEOUT		m_deviceHandle;

	float			m_mixTime;
	float			m_baseTime;
	int				m_sampleIndex;
	CAudioBuffer	m_buffers[ OUTPUT_BUFFER_COUNT ];

	CAudioMixer		*m_sourceList[MAX_CHANNELS];
	int				m_nEstimatedSamplesAhead;
public:
	CAudioDeviceSWMix	m_audioDevice;

};
#endif // AUDIOWAVEOUTPUT_H
