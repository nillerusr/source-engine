//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

// This module implements the voice record and compression functions 

#include "audio_pch.h"
#if !defined( _X360 )
#include "dsound.h"
#endif
#include <assert.h>
#include "voice.h"
#include "tier0/vcrmode.h"
#include "ivoicerecord.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// ------------------------------------------------------------------------------
// Globals.
// ------------------------------------------------------------------------------

typedef HRESULT (WINAPI *DirectSoundCaptureCreateFn)(const GUID FAR *lpGUID, LPDIRECTSOUNDCAPTURE *pCapture, LPUNKNOWN pUnkOuter);



// ------------------------------------------------------------------------------
// Static helpers
// ------------------------------------------------------------------------------



// ------------------------------------------------------------------------------
// VoiceRecord_DSound
// ------------------------------------------------------------------------------

class VoiceRecord_DSound : public IVoiceRecord
{
protected:

	virtual				~VoiceRecord_DSound();


// IVoiceRecord.
public:

						VoiceRecord_DSound();
	virtual void		Release();

	virtual bool		RecordStart();
	virtual void		RecordStop();
	
	// Initialize. The format of the data we expect from the provider is
	// 8-bit signed mono at the specified sample rate.
	virtual bool		Init(int sampleRate);

	virtual void		Idle();

	// Get the most recent N samples.
	virtual int			GetRecordedData(short *pOut, int nSamplesWanted);

private:
	void				Term();				// Delete members.
	void				Clear();			// Clear members.
	void				UpdateWrapping();

	inline DWORD		NumCaptureBufferBytes()	{return m_nCaptureBufferBytes;}


private:
	HINSTANCE					m_hInstDS;

	LPDIRECTSOUNDCAPTURE		m_pCapture;
	LPDIRECTSOUNDCAPTUREBUFFER	m_pCaptureBuffer;

	// How many bytes our capture buffer has.
	DWORD						m_nCaptureBufferBytes;
	
	// We need to know when the capture buffer loops, so we install an event and 
	// update this in the event.
	DWORD						m_WrapOffset;
	HANDLE						m_hWrapEvent;

	// This is our (unwrapped) position that tells how much data we've given to the app.
	DWORD						m_LastReadPos;
};



VoiceRecord_DSound::VoiceRecord_DSound()
{
	Clear();
}


VoiceRecord_DSound::~VoiceRecord_DSound()
{
	Term();
}


void VoiceRecord_DSound::Release()
{
	delete this;
}


bool VoiceRecord_DSound::RecordStart()
{
	//When we start recording we want to make sure we don't provide any audio
	//that occurred before now. So set m_LastReadPos to the current
	//read position of the audio device
	if (m_pCaptureBuffer == NULL)
	{
		return false;
	}

	Idle();

	DWORD dwStatus;
	HRESULT hr = m_pCaptureBuffer->GetStatus(&dwStatus);
	if (FAILED(hr) || !(dwStatus & DSCBSTATUS_CAPTURING))
		return false;

	DWORD dwReadPos;
	hr = m_pCaptureBuffer->GetCurrentPosition(NULL, &dwReadPos);
	if (!FAILED(hr))
	{
		m_LastReadPos = dwReadPos + m_WrapOffset;
	}

	return true;
}


void VoiceRecord_DSound::RecordStop()
{
}

static bool IsRunningWindows7()
{
	if ( IsPC() )
	{
		OSVERSIONINFOEX osvi;
		ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

		if ( GetVersionEx ((OSVERSIONINFO *)&osvi) )
		{
			if ( osvi.dwMajorVersion > 6 || (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion >= 1) )
				return true;
		}
	}
	return false;
}

bool VoiceRecord_DSound::Init(int sampleRate)
{
	HRESULT hr;
	DSCBUFFERDESC dscDesc;
	DirectSoundCaptureCreateFn createFn;

	
	Term();


	WAVEFORMATEX recordFormat =
	{
		WAVE_FORMAT_PCM,		// wFormatTag
		1,						// nChannels
		(uint32)sampleRate,				// nSamplesPerSec
		(uint32)sampleRate*2,			// nAvgBytesPerSec
		2,						// nBlockAlign
		16,						// wBitsPerSample
		sizeof(WAVEFORMATEX)	// cbSize
	};


	
	// Load the DSound DLL.
	m_hInstDS = LoadLibrary("dsound.dll");
	if(!m_hInstDS)
		goto HandleError;

	createFn = (DirectSoundCaptureCreateFn)GetProcAddress(m_hInstDS, "DirectSoundCaptureCreate");
	if(!createFn)
		goto HandleError;

	const GUID FAR *pGuid = &DSDEVID_DefaultVoiceCapture;
	if ( IsRunningWindows7() )
	{
		pGuid = NULL;
	}
	hr = createFn(pGuid, &m_pCapture, NULL);
	if(FAILED(hr))
		goto HandleError;

	// Create the capture buffer.
	memset(&dscDesc, 0, sizeof(dscDesc));
	dscDesc.dwSize = sizeof(dscDesc);
	dscDesc.dwFlags = 0;
	dscDesc.dwBufferBytes = recordFormat.nAvgBytesPerSec;
	dscDesc.lpwfxFormat = &recordFormat;

	hr = m_pCapture->CreateCaptureBuffer(&dscDesc, &m_pCaptureBuffer, NULL);
	if(FAILED(hr))
		goto HandleError;


	// Figure out how many bytes we got in our capture buffer.
	DSCBCAPS caps;
	memset(&caps, 0, sizeof(caps));
	caps.dwSize = sizeof(caps);

	hr = m_pCaptureBuffer->GetCaps(&caps);
	if(FAILED(hr))
		goto HandleError;

	m_nCaptureBufferBytes = caps.dwBufferBytes;


	// Set it up so we get notification when the buffer wraps.
	m_hWrapEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if(!m_hWrapEvent)
		goto HandleError;

	DSBPOSITIONNOTIFY dsbNotify;
	dsbNotify.dwOffset = dscDesc.dwBufferBytes - 1;
	dsbNotify.hEventNotify = m_hWrapEvent;
	
	// Get the IDirectSoundNotify interface.
	LPDIRECTSOUNDNOTIFY pNotify;
	hr = m_pCaptureBuffer->QueryInterface(IID_IDirectSoundNotify, (void**)&pNotify);
	if(FAILED(hr))
		goto HandleError;
	
	hr = pNotify->SetNotificationPositions(1, &dsbNotify);
	pNotify->Release();
	if(FAILED(hr))
		goto HandleError;

	// Start capturing.
	hr = m_pCaptureBuffer->Start(DSCBSTART_LOOPING);
	if(FAILED(hr))
		return false;

	return true;


HandleError:;
	Term();
	return false;
}


void VoiceRecord_DSound::Term()
{
	if(m_pCaptureBuffer)
		m_pCaptureBuffer->Release();

	if(m_pCapture)
		m_pCapture->Release();

	if(m_hWrapEvent)
		DeleteObject(m_hWrapEvent);

	if(m_hInstDS)
	{
		FreeLibrary(m_hInstDS);
		m_hInstDS = NULL;
	}

	Clear();
}


void VoiceRecord_DSound::Clear()
{
	m_pCapture = NULL;
	m_pCaptureBuffer = NULL;
	m_WrapOffset = 0;
	m_LastReadPos = 0;
	m_hWrapEvent = NULL;
	m_hInstDS = NULL;
}


void VoiceRecord_DSound::Idle()
{
	UpdateWrapping();
}

int VoiceRecord_DSound::GetRecordedData( short *pOut, int nSamples )
{
	if(!m_pCaptureBuffer)
	{
		assert(false);
		return 0;
	}

	DWORD dwStatus;
	HRESULT hr = m_pCaptureBuffer->GetStatus(&dwStatus);
	if(FAILED(hr) || !(dwStatus & DSCBSTATUS_CAPTURING))
		return 0;

	Idle();	// Update wrapping..

	DWORD nBytesWanted = (DWORD)( nSamples << 1 );

	DWORD dwReadPos;
	hr = m_pCaptureBuffer->GetCurrentPosition( NULL, &dwReadPos);
	if(FAILED(hr))
		return 0;

	dwReadPos += m_WrapOffset;

	// Read the range (dwReadPos-nSamplesWanted, dwReadPos), but don't re-read data we've already read.
	DWORD readStart = Max( dwReadPos - nBytesWanted, (DWORD)0u );
	if ( readStart < m_LastReadPos )
	{
		readStart = m_LastReadPos;
	}

	// Lock the buffer.
	LPVOID pData[2];
	DWORD dataLen[2];

	hr = m_pCaptureBuffer->Lock(
		readStart % NumCaptureBufferBytes(),	// Offset.
		dwReadPos - readStart,					// Number of bytes to lock.
		&pData[0],								// Buffer 1.
		&dataLen[0],							// Buffer 1 length.
		&pData[1],								// Buffer 2.
		&dataLen[1],							// Buffer 2 length.
		0										// Flags.
		);

	if(FAILED(hr))
		return 0;

	// Hopefully we didn't get too much data back!
	if((dataLen[0]+dataLen[1]) > nBytesWanted )
	{
		assert(false);
		m_pCaptureBuffer->Unlock(pData[0], dataLen[0], pData[1], dataLen[1]);
		return 0;
	}

	// Copy the data to the output.
	memcpy(pOut, pData[0], dataLen[0]);
	memcpy(&pOut[dataLen[0]/2], pData[1], dataLen[1]);

	m_pCaptureBuffer->Unlock(pData[0], dataLen[0], pData[1], dataLen[1]);

	// Last Read Position
	m_LastReadPos = dwReadPos;
	// Return sample count (not bytes)
	return (dataLen[0] + dataLen[1]) >> 1;
}


void VoiceRecord_DSound::UpdateWrapping()
{
	if(!m_pCaptureBuffer)
		return;

	// Has the buffer wrapped?
	if ( VCRHook_WaitForSingleObject(m_hWrapEvent, 0) == WAIT_OBJECT_0 )
	{
		m_WrapOffset += m_nCaptureBufferBytes;
	}
}



IVoiceRecord* CreateVoiceRecord_DSound(int sampleRate)
{
	VoiceRecord_DSound *pRecord = new VoiceRecord_DSound;
	if(pRecord && pRecord->Init(sampleRate))
	{
		return pRecord;
	}
	else
	{
		if(pRecord)
			pRecord->Release();

		return NULL;
	}
}

