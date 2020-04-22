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
//=============================================================================//

#ifndef SND_WAVE_MIXER_PRIVATE_H
#define SND_WAVE_MIXER_PRIVATE_H
#pragma once

#include "snd_audio_source.h"
#include "snd_wave_mixer.h"
#include "sound_private.h"
#include "snd_wave_source.h"

class IWaveData;

abstract_class CAudioMixerWave : public CAudioMixer
{
public:
							CAudioMixerWave( IWaveData *data );
	virtual					~CAudioMixerWave( void );

	int						MixDataToDevice( IAudioDevice *pDevice, channel_t *pChannel, int sampleCount, int outputRate, int outputOffset );
	int						SkipSamples( channel_t *pChannel, int sampleCount, int outputRate, int outputOffset );
	bool					ShouldContinueMixing( void );

	virtual void			Mix( IAudioDevice *pDevice, channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress ) = 0;
	virtual int				GetOutputData( void **pData, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );

	virtual CAudioSource*	GetSource( void );
	virtual int				GetSamplePosition( void );
	virtual float			ModifyPitch( float pitch );
	virtual float			GetVolumeScale( void );
	
	// Move the current position to newPosition
	virtual void			SetSampleStart( int newPosition );
	
	// End playback at newEndPosition
	virtual void			SetSampleEnd( int newEndPosition );

	virtual void			SetStartupDelaySamples( int delaySamples );
	
	// private helper routines

	char *					LoadMixBuffer( channel_t *pChannel, int sample_load_request, int *psamples_loaded, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );
	int						MixDataToDevice_( IAudioDevice *pDevice, channel_t *pChannel, int sampleCount, int outputRate, int outputOffset, bool bSkipAllSamples );
	int						GetSampleLoadRequest( double rate, int sampleCount, bool bInterpolated_pitch );

	virtual bool			IsReadyToMix();
	virtual int				GetPositionForSave() { return GetSamplePosition(); }
	virtual void			SetPositionFromSaved( int savedPosition ) { SetSampleStart(savedPosition); }

protected:
	double				m_fsample_index;			// index of next sample to output
	int					m_sample_max_loaded;		// count of total samples loaded - ie: the index of 
													// the next sample to be loaded.
	int					m_sample_loaded_index;		// index of last sample loaded

	IWaveData			*m_pData;
	double 				m_forcedEndSample;
	bool				m_finished;
	int					m_delaySamples;
};


#endif // SND_WAVE_MIXER_PRIVATE_H
