//========= Copyright 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// This module implements the voice record and compression functions 

//#include "audio_pch.h"
//#include "voice.h"
#include "tier0/platform.h"
#include "ivoicerecord.h"

#include <assert.h>

#ifndef POSIX
class VoiceRecord_OpenAL : public IVoiceRecord
{
public:
	VoiceRecord_OpenAL() {}
	virtual				~VoiceRecord_OpenAL() {}
	virtual void		Release() {}
	virtual bool		RecordStart() { return true; }
	virtual void		RecordStop() {}
	virtual bool		Init(int sampleRate) { return true; }
	virtual void		Idle() {}
	virtual int			GetRecordedData(short *pOut, int nSamplesWanted ) { return 0; }
};
IVoiceRecord* CreateVoiceRecord_DSound(int sampleRate) { return new VoiceRecord_OpenAL; }

#else

#define min(a,b)  (((a) < (b)) ? (a) : (b))
#ifdef OSX
#include <Carbon/Carbon.h>
#include <OpenAL/al.h>
#else
#include <AL/al.h>
#endif
#include "openal/alc.h"

// ------------------------------------------------------------------------------
// VoiceRecord_OpenAL
// ------------------------------------------------------------------------------

class VoiceRecord_OpenAL : public IVoiceRecord
{
protected:
	
	virtual				~VoiceRecord_OpenAL();
	
	
	// IVoiceRecord.
public:
	
	VoiceRecord_OpenAL();
	virtual void		Release();
	
	virtual bool		RecordStart();
	virtual void		RecordStop();
	
	// Initialize. The format of the data we expect from the provider is
	// 8-bit signed mono at the specified sample rate.
	virtual bool		Init(int sampleRate);
	
	virtual void		Idle();
	
	// Get the most recent N samples.
	virtual int			GetRecordedData(short *pOut, int nSamplesWanted );
	
private:
	bool				InitalizeInterfaces();	// Initialize the openal capture buffers and other interfaces
	void				ReleaseInterfaces();	// Release openal buffers and other interfaces
	void				ClearInterfaces();				// Clear members.
	
	
private:
	ALCdevice    *m_Device;
	
	int	m_nSampleRate;
};



VoiceRecord_OpenAL::VoiceRecord_OpenAL() :
m_nSampleRate( 0 ), m_Device( NULL )
{
	ClearInterfaces();
}

VoiceRecord_OpenAL::~VoiceRecord_OpenAL()
{
	ReleaseInterfaces();
}


void VoiceRecord_OpenAL::Release()
{
	delete this;
}

bool VoiceRecord_OpenAL::RecordStart()
{

	// Re-initialize the capture buffer if neccesary (should always be)
	if ( !m_Device )
	{
		InitalizeInterfaces();
	}
	
	if ( !m_Device )
		return false;

	alcGetError(m_Device);
	
	alcCaptureStart(m_Device);

	const ALenum error = alcGetError(m_Device);
	return error == AL_NO_ERROR;
}


void VoiceRecord_OpenAL::RecordStop()
{
	// Stop capturing.
	if ( m_Device )
	{
		alcCaptureStop( m_Device );
	}
	
	// Release the capture buffer interface and any other resources that are no
	// longer needed
	ReleaseInterfaces();
}

bool VoiceRecord_OpenAL::InitalizeInterfaces()
{	
	m_Device = alcCaptureOpenDevice( NULL, m_nSampleRate, AL_FORMAT_MONO16, m_nSampleRate * 10 * 2);
	const ALenum error = alcGetError(m_Device);
	const bool result = error == AL_NO_ERROR;
	return m_Device != NULL && result;
}

bool VoiceRecord_OpenAL::Init(int sampleRate)
{
	m_nSampleRate = sampleRate;
	
	ReleaseInterfaces();
	
	return true;
}


void VoiceRecord_OpenAL::ReleaseInterfaces()
{
    alcCaptureCloseDevice(m_Device);	
	ClearInterfaces();
}


void VoiceRecord_OpenAL::ClearInterfaces()
{
	m_Device = NULL;
}


void VoiceRecord_OpenAL::Idle()
{
}


int VoiceRecord_OpenAL::GetRecordedData(short *pOut, int nSamples )
{
	int frameCount = 0;
	alcGetIntegerv( m_Device,ALC_CAPTURE_SAMPLES,1,&frameCount );
	if ( frameCount > 0 )
	{
		frameCount = min( nSamples, frameCount );
		alcCaptureSamples( m_Device, pOut, frameCount );
		if ( alcGetError(m_Device) != ALC_NO_ERROR )
		{
			return 0;
		}
		return frameCount;
	}
	return 0;
}



IVoiceRecord* CreateVoiceRecord_OpenAL(int sampleRate)
{
	VoiceRecord_OpenAL *pRecord = new VoiceRecord_OpenAL;
	if ( pRecord && pRecord->Init(sampleRate) )
	{
		return pRecord;
	}
	else
	{
		if ( pRecord )
		{
			pRecord->Release();
		}

		return NULL;
	}
}
#endif
