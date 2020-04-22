//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#ifndef SND_WAVE_MIXER_PRIVATE_H
#define SND_WAVE_MIXER_PRIVATE_H

#ifdef _WIN32
#pragma once
#endif

#include "soundsystem/snd_audio_source.h"
#include "soundsystem/snd_device.h"
#include "snd_wave_mixer.h"



//-----------------------------------------------------------------------------
// Purpose: Linear iterator over source data.
//			Keeps track of position in source, and maintains necessary buffers
//-----------------------------------------------------------------------------
class CWaveData
{
public:
	virtual ~CWaveData( void ) {}
	virtual CAudioSourceWave &Source( void ) = 0;
	virtual int ReadSourceData( void **pData, int sampleIndex, int sampleCount, bool forward = true ) = 0;
};

class CAudioMixerWave : public CAudioMixer
{
public:
	CAudioMixerWave( CWaveData *data );
	virtual ~CAudioMixerWave( void );

	virtual bool MixDataToDevice( IAudioDevice *pDevice, channel_t *pChannel, int startSample, int sampleCount, int outputRate, bool forward = true );
	virtual void IncrementSamples( channel_t *pChannel, int startSample, int sampleCount,int outputRate, bool forward = true );
	virtual bool SkipSamples( IAudioDevice *pDevice, channel_t *pChannel, int startSample, int sampleCount,int outputRate, bool forward = true  );
	virtual void Mix( IAudioDevice *pDevice, 
					channel_t *pChannel, 
					void *pData, 
					int outputOffset, 
					int inputOffset, 
					fixedint fracRate, 
					int outCount, 
					int timecompress,
					bool forward = true ) = 0;

	virtual int	 GetOutputData( void **pData, int samplePosition, int sampleCount, bool forward = true );

	virtual CAudioSource *GetSource( void );

	virtual int GetSamplePosition( void );
	virtual int GetScubPosition( void );

	virtual bool SetSamplePosition( int position, bool scrubbing = false );
	virtual void SetLoopPosition( int position );
	virtual int	GetStartPosition( void );

	virtual bool	GetActive( void );
	virtual void	SetActive( bool active );

	virtual void	SetModelIndex( int index );
	virtual int		GetModelIndex( void ) const;

	virtual void	SetDirection( bool forward );
	virtual bool	GetDirection( void ) const;

	virtual void	SetAutoDelete( bool autodelete );
	virtual bool	GetAutoDelete( void ) const;

	virtual void	SetVolume( float volume );
	virtual channel_t *GetChannel();

protected:
	int					m_sample;
	int					m_absoluteSample;
	int					m_scubSample;
	int					m_startpos;
	int					m_loop;
	int					m_fracOffset;
	CWaveData			*m_pData;

	int					m_absoluteStartPos;

	bool				m_bActive;
	// Associated playback model in faceposer
	int					m_nModelIndex;

	bool				m_bForward;

	bool				m_bAutoDelete;

	channel_t			*m_pChannel;
};


#endif // SND_WAVE_MIXER_PRIVATE_H
