//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IWAVEOUT_H
#define IWAVEOUT_H
#pragma once


// Interface by the voice_tweak app for simple wave output. You must extern a 
// specific factory function to create and initialize an instance.
class IWaveOut
{
public:
	virtual			~IWaveOut() {}
	virtual void	Release() = 0;

	// Give it samples to mix into the output. The input samples are 16-bit signed mono.
	virtual bool	PutSamples(short *pSamples, int nSamples) = 0;

	// Do idle time processing.
	virtual void	Idle() = 0;

	// Returns the number of samples you've put that haven't been played yet (sitting in the buffer 
	// waiting to be played).
	virtual int		GetNumBufferedSamples() = 0;
};


#endif // IWAVEOUT_H
