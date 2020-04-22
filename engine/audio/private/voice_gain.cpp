//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//


#include "audio_pch.h"
#include "minmax.h"
#include "voice_gain.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CAutoGain::CAutoGain()
{
	Reset(128, 5.0f, 0.5f, 1);
}


void CAutoGain::Reset(int blockSize, float maxGain, float avgToMaxVal, float scale)
{
	m_BlockSize = blockSize;
	m_MaxGain = maxGain;
	m_AvgToMaxVal = avgToMaxVal;

	m_CurBlockOffset = 0;
	m_CurTotal = 0;
	m_CurMax = 0;

	m_CurrentGain = 1;
	m_NextGain = 1;

	m_Scale = scale;

	m_GainMultiplier = 0;
	m_FixedCurrentGain = 1 << AG_FIX_SHIFT;
}


void CAutoGain::ProcessSamples(
	short *pSamples,
	int nSamples)
{
	short *pCurPos = pSamples;
	int nSamplesLeft = nSamples;

	// Continue until we hit the end of this block.
	while(nSamplesLeft)
	{
		int nToProcess = min(nSamplesLeft, (m_BlockSize - m_CurBlockOffset));
		for(int iSample=0; iSample < nToProcess; iSample++)
		{
			// Update running totals..
			m_CurTotal += abs( pCurPos[iSample] );
			m_CurMax = max( m_CurMax, (int)abs( pCurPos[iSample] ) );

			// Apply gain on this sample.
			AGFixed gain = m_FixedCurrentGain + m_CurBlockOffset * m_GainMultiplier;
			m_CurBlockOffset++;

			int newval = ((int)pCurPos[iSample] * gain) >> AG_FIX_SHIFT;
			newval = min(32767, max(newval, -32768));
			pCurPos[iSample] = (short)newval;
		}
		pCurPos += nToProcess;
		nSamplesLeft -= nToProcess;

		// Did we just end a block? Update our next gain.
		if((m_CurBlockOffset % m_BlockSize) == 0)
		{
			// Now we've interpolated to our next gain, make it our current gain.
			m_CurrentGain = m_NextGain * m_Scale;
			m_FixedCurrentGain = (int)((double)m_CurrentGain * (1 << AG_FIX_SHIFT));

			// Figure out the next gain (the gain we'll interpolate to).
			int avg = m_CurTotal / m_BlockSize;
			float modifiedMax = avg + (m_CurMax - avg) * m_AvgToMaxVal;
			m_NextGain = min(32767.0f / modifiedMax, m_MaxGain) * m_Scale;

			// Setup the interpolation multiplier.
			float fGainMultiplier = (m_NextGain - m_CurrentGain) / (m_BlockSize - 1);
			m_GainMultiplier = (AGFixed)((double)fGainMultiplier * (1 << AG_FIX_SHIFT));

			// Reset counters.
			m_CurTotal = 0;
			m_CurMax = 0;
			m_CurBlockOffset = 0;
		}
	}
}
