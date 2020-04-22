//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "audio_pch.h"

#if defined( USE_SDL )
#include "snd_dev_sdl.h"
#endif
#ifdef OSX
#include "snd_dev_openal.h"
#include "snd_dev_mac_audioqueue.h"

ConVar snd_audioqueue( "snd_audioqueue", "1" );

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool snd_firsttime = true;

/* 
 * Global variables. Must be visible to window-procedure function 
 *  so it can unlock and free the data block after it has been played. 
 */ 
IAudioDevice *g_AudioDevice = NULL;

/*
==================
S_BlockSound
==================
*/
void S_BlockSound( void )
{
	if ( !g_AudioDevice )
		return;

	g_AudioDevice->Pause();
}

/*
==================
S_UnblockSound
==================
*/
void S_UnblockSound( void )
{
	if ( !g_AudioDevice )
		return;

	g_AudioDevice->UnPause();
}

/*
==================
AutoDetectInit

Try to find a sound device to mix for.
Returns a CAudioNULLDevice if nothing is found.
==================
*/
IAudioDevice *IAudioDevice::AutoDetectInit( bool waveOnly )
{
	IAudioDevice *pDevice = NULL;

	if ( IsPC() )
	{
#if defined( WIN32 ) && !defined( USE_SDL )
		if ( waveOnly )
		{
			pDevice = Audio_CreateWaveDevice();
			if ( !pDevice )
				goto NULLDEVICE;
		}

		if ( !pDevice )
		{
			if ( snd_firsttime )
			{
				pDevice = Audio_CreateDirectSoundDevice();
			}
		}

		// if DirectSound didn't succeed in initializing, try to initialize
		// waveOut sound, unless DirectSound failed because the hardware is
		// already allocated (in which case the user has already chosen not
		// to have sound)
		// UNDONE: JAY: This doesn't test for the hardware being in use anymore, REVISIT
		if ( !pDevice )
		{
			pDevice = Audio_CreateWaveDevice();
		}
#elif defined(OSX)
		if ( !CommandLine()->CheckParm( "-snd_openal" ) )
		{
			DevMsg( "Using AudioQueue Interface\n" );
			pDevice = Audio_CreateMacAudioQueueDevice();
		}
		if ( !pDevice )
		{
			DevMsg( "Using OpenAL Interface\n" );
			pDevice = Audio_CreateOpenALDevice(); // fall back to openAL if the audio queue fails
		}
#elif defined( USE_SDL )
		DevMsg( "Trying SDL Audio Interface\n" );
		pDevice = Audio_CreateSDLAudioDevice();

#ifdef NEVER
		// Jul 2012. mikesart. E-mail exchange with Ryan Gordon after figuring out that
		// Audio_CreatePulseAudioDevice() wasn't working on Ubuntu 12.04 (lots of stuttering).
		//
		// > I installed libpulse-dev, rebuilt SDL, and now SDL is using pulse
		// > audio and everything is working great. However I'm wondering if we
		// > need to fall back to PulseAudio in our codebase if SDL is doing that
		// > for us. I mean, is it worth me going through and debugging our Pulse
		// > Audio path or should I just remove it?
		// 
		// Remove it...it never worked well, and only remained in case there were
		// concerns about relying on SDL. The SDL codepath is way easier to read,
		// simpler to maintain, and handles all sorts of strange audio backends,
		// including Pulse.
		if ( !pDevice )
		{
			DevMsg( "Trying PulseAudio Interface\n" );
			pDevice = Audio_CreatePulseAudioDevice(); // fall back to PulseAudio if SDL fails
		}
#endif // NEVER

#else
#error
#endif
	}
#if defined( _X360 )
	else
	{
		pDevice = Audio_CreateXAudioDevice( true );
		if ( pDevice )
		{
			// xaudio requires threaded mixing
			S_EnableThreadedMixing( true );
		}
	}
#endif

#if defined( WIN32 ) && !defined( USE_SDL )
NULLDEVICE:
#endif
	snd_firsttime = false;

	if ( !pDevice )
	{
		if ( snd_firsttime )
			DevMsg( "No sound device initialized\n" );

		return Audio_GetNullDevice();
	}

	return pDevice;
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown( void )
{
	if ( g_AudioDevice != Audio_GetNullDevice() )
	{
		if ( g_AudioDevice )
		{
			g_AudioDevice->Shutdown();
			delete g_AudioDevice;
		}

		// the NULL device is always valid
		g_AudioDevice = Audio_GetNullDevice();
	}
}

