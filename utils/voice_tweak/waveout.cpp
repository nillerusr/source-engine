//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include <windows.h>
#include <assert.h>
#include <mmsystem.h>
#include "waveout.h"
#include "ivoicecodec.h"


class CWaveOutHdr
{
public:
	WAVEHDR		m_Hdr;
	CWaveOutHdr	*m_pNext;
	char		m_Data[1];
};


class CWaveOut : public IWaveOut
{
// IWaveOut overrides.
public:
					CWaveOut();
	virtual			~CWaveOut();
	virtual void	Release();
	virtual bool	PutSamples(short *pSamples, int nSamples);
	virtual void	Idle();
	virtual int		GetNumBufferedSamples();


public:
	bool			Init(int sampleRate);
	void			Term();


private:
	void			KillOldHeaders();


private:
	HWAVEOUT		m_hWaveOut;
	CWaveOutHdr		m_Headers;		// Head of a linked list of WAVEHDRs.
	int				m_nBufferedSamples;
};


CWaveOut::CWaveOut()
{
	m_hWaveOut = NULL;
	m_Headers.m_pNext = NULL;
	m_nBufferedSamples = 0;
}

CWaveOut::~CWaveOut()
{
	Term();
}

void CWaveOut::Release()
{
	delete this;
}

bool CWaveOut::PutSamples(short *pInSamples, int nInSamples)
{
	int granularity = 2048;
	while( nInSamples )
	{
		int nSamples = (nInSamples > granularity) ? granularity : nInSamples;
		short *pSamples = pInSamples;
		nInSamples -= nSamples;
		pInSamples += nSamples;

		if(!m_hWaveOut)
			return false;

		// Kill any old headers..
		KillOldHeaders();

		// Allocate a header..
		CWaveOutHdr *pHdr;
		if(!(pHdr = (CWaveOutHdr*)malloc(sizeof(CWaveOutHdr) - 1 + nSamples*2)))
			return false;
		
		// Make a new one.
		memset(&pHdr->m_Hdr, 0, sizeof(pHdr->m_Hdr));
		pHdr->m_Hdr.lpData = pHdr->m_Data;
		pHdr->m_Hdr.dwBufferLength = nSamples * 2;
		memcpy(pHdr->m_Data, pSamples, nSamples*2);

		MMRESULT mmr = waveOutPrepareHeader(m_hWaveOut, &pHdr->m_Hdr, sizeof(pHdr->m_Hdr));
		if(mmr != MMSYSERR_NOERROR)
			return false;

		mmr = waveOutWrite(m_hWaveOut, &pHdr->m_Hdr, sizeof(pHdr->m_Hdr));
		if(mmr != MMSYSERR_NOERROR)
		{
			delete pHdr;
			waveOutUnprepareHeader(m_hWaveOut, &pHdr->m_Hdr, sizeof(pHdr->m_Hdr));
			return false;
		}

		m_nBufferedSamples += nSamples;

		// Queue up this header until waveOut is done with it.
		pHdr->m_pNext = m_Headers.m_pNext;
		m_Headers.m_pNext = pHdr;
	}
	
	return true;
}

void CWaveOut::Idle()
{
	KillOldHeaders();
}

int CWaveOut::GetNumBufferedSamples()
{
	return m_nBufferedSamples;
}

bool CWaveOut::Init(int sampleRate)
{
	Term();


	WAVEFORMATEX format =
	{
		WAVE_FORMAT_PCM,			// wFormatTag
		1,							// nChannels
		sampleRate,					// nSamplesPerSec
		sampleRate*BYTES_PER_SAMPLE,// nAvgBytesPerSec
		BYTES_PER_SAMPLE,			// nBlockAlign
		BYTES_PER_SAMPLE * 8,		// wBitsPerSample
		sizeof(WAVEFORMATEX)
	};

	MMRESULT mmr = waveOutOpen(
		&m_hWaveOut,
		0,
		&format,
		0,
		0,
		CALLBACK_NULL);

	return mmr == MMSYSERR_NOERROR;
}

void CWaveOut::Term()
{
	if(m_hWaveOut)
	{
		waveOutClose(m_hWaveOut);
		m_hWaveOut = NULL;
	}
}

void CWaveOut::KillOldHeaders()
{
	// Look for any headers windows is done with.
	CWaveOutHdr *pNext;
	CWaveOutHdr **ppPrev = &m_Headers.m_pNext;
	for(CWaveOutHdr *pCur=m_Headers.m_pNext; pCur; pCur=pNext)
	{
		pNext = pCur->m_pNext;

		if(pCur->m_Hdr.dwFlags & WHDR_DONE)
		{
			m_nBufferedSamples -= (int)(pCur->m_Hdr.dwBufferLength / 2);
			assert(m_nBufferedSamples >= 0);
			waveOutUnprepareHeader(m_hWaveOut, &pCur->m_Hdr, sizeof(pCur->m_Hdr));
			*ppPrev = pCur->m_pNext;
			free(pCur);
		}
		else
		{
			ppPrev = &pCur->m_pNext;
		}
	}
}



IWaveOut* CreateWaveOut(int sampleRate)
{
	CWaveOut *pRet = new CWaveOut;
	if(pRet && pRet->Init(sampleRate))
	{
		return pRet;
	}
	else
	{
		delete pRet;
		return NULL;
	}
}


