//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef VOICE_GAIN_H
#define VOICE_GAIN_H
#pragma once


// ----------------------------------------------------------------------- //
// CAutoGain is fed samples and figures out a gain to apply to blocks of samples.

// Right now, this class applies gain one block behind. The assumption is that the blocks are
// small enough that gain settings for one block will usually be right for the next block.

// The ideal way to implement this class would be to have a delay the size of a block
// so it can apply the right gain to the actual block it was calculated for.
// ----------------------------------------------------------------------- //
class CAutoGain
{
public:

			CAutoGain();

	// maxGain and avgToMaxVal are used to derive the gain amount for each block of samples.
	// All samples are scaled by scale.
	void	Reset(int blockSize, float maxGain, float avgToMaxVal, float scale);

	// Process the specified samples and apply gain to them.
	void	ProcessSamples(
		short *pSamples,
		int nSamples);

private:

	enum	{AG_FIX_SHIFT=7};
	typedef long	AGFixed;

	// Parameters affecting the algorithm.
	int		m_BlockSize;			// Derive gain from blocks of this size.
	float	m_MaxGain;
	float	m_AvgToMaxVal;

	// These are calculated as samples are passed in.
	int		m_CurBlockOffset;
	int		m_CurTotal;				// Total of sample values in current block.
	int		m_CurMax;				// Highest (absolute) sample value.

	float	m_Scale;				// All samples are scaled by this amount.

	float	m_CurrentGain;			// Gain at sample 0 in this block.
	float	m_NextGain;				// Gain at the last sample in this block.

	AGFixed	m_FixedCurrentGain;		// Fixed-point m_CurrentGain.	
	AGFixed	m_GainMultiplier;		// (m_NextGain - m_CurrentGain) / (m_BlockSize - 1).
};


#endif // VOICE_GAIN_H
