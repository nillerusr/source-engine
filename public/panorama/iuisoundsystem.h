//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUISOUNDSYSTEM_H
#define IUISOUNDSYSTEM_H

#ifdef _WIN32
#pragma once
#endif

#if !defined( SOURCE2_PANORAMA )
#include "audio/iaudiointerface.h"
#endif

#ifdef SUPPORTS_AUDIO
class IAudioOutputStream;
#endif

namespace panorama
{

typedef void * HAUDIOSAMPLE;

//
// Interface that handles sound
//
class IUISoundSystem
{
public:
	virtual ~IUISoundSystem() {}

	// Play a sound referenced by name.
	// flVolume should be 0.0 to 1.0, 0.5 would equal -10DB. volume is modulated by the mixer volume for the specified sound type.
	// flVolumePan sets the position on mono samples, or panning on stereo, 0.0 is full left, 1.0 is full right, 0.5 is no attenuation.
	// flRepeats sets the number of times to repeat the sound, 0.0f will result in infinite
	virtual HAUDIOSAMPLE PlaySound( const char *pchSoundName, ESoundType soundType,
		float flVolume = 1.0f, float flVolumePan = 0.5f, float flRepeats = 1.0f ) = 0;

	// Set the base volume / panning for the sound sample
	virtual void SetSoundSampleVolumePan( HAUDIOSAMPLE hSample, float flVolume, float flVolumePan ) = 0;

	// Fade out the sound sample and stop it 
	virtual void FadeOutAndStopSoundSample( HAUDIOSAMPLE hSample, float flFadeOutSeconds ) = 0;

	// Set a volume ramp that will change the volume of the sample smoothly, note that this acts as a filter and scales the volume from
	// the base set when playing or with SetSoundSampleVolumePan, the base is not actually modified and still must be set as audible for
	// this ramp to have any impact.
	virtual void VolumeRampSoundSample( HAUDIOSAMPLE hSample, float flVolumeTarget, float flTransitionSeconds ) = 0;

	// retrieves the user-specified sound volume for the specified sound type.
	//
	// Normally you don't need to use this as PlaySound auto-applies the right volume;
	// but it is needed on raw audio streams created with CreateAudioOutputStream.
	//
	// The value returned here honors the muted state.
	virtual float GetSoundVolume( ESoundType soundType ) = 0;

	// here you can set the volume for the specified sound type programmatically
	virtual void SetSoundVolume( ESoundType soundType, float flVolume ) = 0;

	// and you can set a global mute state
	virtual void SetSoundMuted( bool bMute ) = 0;

#ifdef SUPPORTS_AUDIO
	virtual IAudioOutputStream *CreateAudioOutputStream( int nRate, int nChannels, int nBits ) = 0;
	virtual void FreeAudioOutputStream( IAudioOutputStream *pStream ) = 0;
#endif

	// Push that the system now requires a larger mix ahead buffer to prevent skipping, "large" is somewhat undefined
	// at this level, but you are trading off latency on sounds beginning to get a bigger buffer pre-mixed by miles
	// to avoid skipping/stuttering.
	virtual void PushAudioBigMixAheadBuffer() = 0;

	// Pop that the system now requires a larger mix ahead buffer to prevent skipping, "large" is somewhat undefined
	// at this level, but you are trading off latency on sounds beginning to get a bigger buffer pre-mixed by miles
	// to avoid skipping/stuttering.  Popping moves back towards the lower latency setup once nothing needs the no-skipping
	// larger buffer setup.
	virtual void PopAudioBigMixAheadBuffer() = 0;

	// Give time to audio service
	virtual void ServiceAudio() = 0;

	// Consider pausing audio based on the last time audio was started. Used when the app loses focus.
	virtual void ConsiderPausingAudio() = 0;
};

} // namespace panorama

#endif // IUISOUNDSYSTEM_H
