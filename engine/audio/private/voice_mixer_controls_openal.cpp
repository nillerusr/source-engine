//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#ifdef OSX
#include <Carbon/Carbon.h>
#include <CoreAudio/CoreAudio.h>
#endif

#include "tier0/platform.h"
#include "ivoicerecord.h"
#include "voice_mixer_controls.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



#ifndef OSX


class CMixerControls : public IMixerControls
{
public:
	CMixerControls() {}
	virtual			~CMixerControls() {}
	
	virtual void	Release() {}
	virtual bool	GetValue_Float(Control iControl, float &value ) {return false;}
	virtual bool	SetValue_Float(Control iControl, float value) {return false;}
	virtual bool	SelectMicrophoneForWaveInput() {return false;}
	virtual const char *GetMixerName() {return "Linux"; }
	
private:
};

IMixerControls* g_pMixerControls = NULL;
void InitMixerControls()
{
	if ( !g_pMixerControls )
	{
		g_pMixerControls = new CMixerControls;
	}
}

void ShutdownMixerControls()
{
	delete g_pMixerControls;
	g_pMixerControls = NULL;
}

#elif defined(OSX)

class CMixerControls : public IMixerControls
{
public:
	CMixerControls();
	virtual			~CMixerControls();
	
	virtual void	Release();
	virtual bool	GetValue_Float(Control iControl, float &value);
	virtual bool	SetValue_Float(Control iControl, float value);
	virtual bool	SelectMicrophoneForWaveInput();
	virtual const char *GetMixerName();
	
private:
	AudioObjectID GetDefaultInputDevice();
	char *m_szMixerName;
	AudioObjectID m_theDefaultDeviceID;
};


CMixerControls::CMixerControls()
{
	m_szMixerName = NULL;
	
	m_theDefaultDeviceID = GetDefaultInputDevice();
	
	OSStatus theStatus;
	UInt32 outSize = sizeof(UInt32);	
	theStatus = AudioDeviceGetPropertyInfo( m_theDefaultDeviceID,
										   0,
										   TRUE,
										   kAudioDevicePropertyDeviceName,
										   &outSize,
										   NULL);
	if ( theStatus == noErr )
	{	
		m_szMixerName = (char *)malloc( outSize*sizeof(char));
		
		theStatus = AudioDeviceGetProperty( m_theDefaultDeviceID,
										   0,
										   TRUE,
										   kAudioDevicePropertyDeviceName,
										   &outSize,
										   m_szMixerName);
		
		if ( theStatus != noErr )
		{
			free( m_szMixerName );
			m_szMixerName = NULL;
		}
	}
}

CMixerControls::~CMixerControls()
{
	if ( m_szMixerName )
		free( m_szMixerName );
}

void CMixerControls::Release()
{
}

bool CMixerControls::SelectMicrophoneForWaveInput()
{
	return true; // not needed
}


const char *CMixerControls::GetMixerName()
{
	return m_szMixerName;
}


bool CMixerControls::GetValue_Float(Control iControl, float &value)
{
	switch( iControl)
	{
		case MicBoost:
		{
			value = 0.0f;
			return true;
		}
		case MicVolume:
		{
			OSStatus theError = noErr;
			for ( int iChannel = 0; iChannel < 3; iChannel++ )
			{
				// scan the channel list until you find a channel set to non-zero, then use that
				Float32 theVolume = 0;
				UInt32 theSize = sizeof(Float32);
				AudioObjectPropertyAddress theAddress = { kAudioDevicePropertyVolumeScalar,	kAudioDevicePropertyScopeInput, iChannel };

				theError = AudioObjectGetPropertyData(m_theDefaultDeviceID,
															   &theAddress,
															   0,
															   NULL,
															   &theSize,
															   &theVolume);
				value = theVolume;
				if ( theError == noErr && theVolume != 0.0f )
					break;
			}
			
			return theError == noErr;
		}
			
		case MicMute:
			// Mic playback muting. You usually want this set to false, otherwise the sound card echoes whatever you say into the mic.
		{
			Float32 theMute = 0;
			UInt32 theSize = sizeof(Float32);
			AudioObjectPropertyAddress theAddress = { kAudioDevicePropertyMute,	kAudioDevicePropertyScopeInput,	1 };
			
			OSStatus theError = AudioObjectGetPropertyData(m_theDefaultDeviceID,
														   &theAddress,
														   0,
														   NULL,
														   &theSize,
														   &theMute);
			value = theMute;
			return theError == noErr;
		}		
		default:
			assert( !"Invalid Control type" );	
			value = 0.0f;
			return false;
	};
}


bool CMixerControls::SetValue_Float(Control iControl, float value)
{
	switch( iControl)
	{
		case MicBoost:
		{
			return false;
		}
		case MicVolume:
		{
			if ( value <= 0.0 )
				return false; // don't let the volume be set to zero
			
			Float32 theVolume = value;
			UInt32 size = sizeof(Float32);
			Boolean	canset	= false;
			AudioObjectID defaultInputDevice = m_theDefaultDeviceID;
			
			size = sizeof(canset);
			OSStatus err = AudioDeviceGetPropertyInfo( defaultInputDevice, 0, true, kAudioDevicePropertyVolumeScalar, &size, &canset);
			if(err==noErr && canset==true) 
			{
				size = sizeof(theVolume);
				err = AudioDeviceSetProperty( defaultInputDevice, NULL, 0, true, kAudioDevicePropertyVolumeScalar, size, &theVolume);
				return err==noErr;
			}
			
			// try seperate channels
			// get channels
			UInt32	channels[2];
			size = sizeof(channels);
			err = AudioDeviceGetProperty(defaultInputDevice, 0, true, kAudioDevicePropertyPreferredChannelsForStereo, &size,&channels);
			if(err!=noErr)
				return false;
			
			// set volume
			size = sizeof(float);
			err = AudioDeviceSetProperty(defaultInputDevice, 0, channels[0], true, kAudioDevicePropertyVolumeScalar, size, &theVolume);
			//AssertMsg1( noErr==err, "error setting volume of channel %d\n",(int)channels[0]);
			err = AudioDeviceSetProperty(defaultInputDevice, 0, channels[1], true, kAudioDevicePropertyVolumeScalar, size, &theVolume);
			//AssertMsg1( noErr==err, "error setting volume of channel %d\n",(int)channels[1]);
			
			return err == noErr;
			
		}
		case MicMute:
			// Mic playback muting. You usually want this set to false, otherwise the sound card echoes whatever you say into the mic.
		{
			Float32 theMute = value;
			UInt32 theMuteSize = sizeof(Float32);
			OSStatus theError = paramErr;
			theError = AudioDeviceSetProperty( m_theDefaultDeviceID,
											  NULL,
											  0,
											  TRUE,
											  kAudioDevicePropertyMute,
											  theMuteSize,
											  &theMute);
			return theError == noErr;
		}		
		default:
			assert( !"Invalid Control type" );	
			return false;
	};
}


AudioObjectID CMixerControls::GetDefaultInputDevice()
{
	AudioObjectID theDefaultDeviceID = kAudioObjectUnknown;
	AudioObjectPropertyAddress theDefaultDeviceAddress = { kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
	UInt32 theDefaultDeviceSize = sizeof(AudioObjectID);
	OSStatus theError = AudioObjectGetPropertyData (kAudioObjectSystemObject, &theDefaultDeviceAddress, 0, NULL, &theDefaultDeviceSize, &theDefaultDeviceID);
	return theDefaultDeviceID;
}


IMixerControls* g_pMixerControls = NULL;
void InitMixerControls()
{
	if ( !g_pMixerControls )
	{
		g_pMixerControls = new CMixerControls;
	}
}

void ShutdownMixerControls()
{
	delete g_pMixerControls;
	g_pMixerControls = NULL;
}



#else
#error
#endif

