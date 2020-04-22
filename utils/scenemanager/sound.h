//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef SOUND_H
#define SOUND_H
#pragma once

class CAudioMixer;

class CAudioInput
{
public:
	// factory to create a suitable audio input for this system
	static CAudioInput *Create( void );

	// base class needs virtual destructor
	virtual ~CAudioInput( void ) {}

	// ------------------- interface ------------------------

	// Returns the current count of available samples
	virtual int SampleCount( void ) = 0;
	
	// returns the size of each sample in bytes
	virtual int SampleSize( void ) = 0;
	
	// returns the sampling rate of the data
	virtual int SampleRate( void ) = 0;

	// returns a pointer to the available data
	virtual void *SampleData( void ) = 0;

	// release the available data (mark as done)
	virtual void SampleRelease( void ) = 0;

	// returns the mono/stereo status of this device (true if stereo)
	virtual bool IsStereo( void ) = 0;

	// begin sampling
	virtual void Start( void ) = 0;

	// stop sampling
	virtual void Stop( void ) = 0;
};

class CAudioSource;

class CAudioOutput
{
public:
	// factory to create a suitable audio output for this system
	static CAudioOutput *Create( void );

	// base class needs virtual destructor
	virtual ~CAudioOutput( void ) {}

	// ------------------- interface ------------------------

	// returns the size of each sample in bytes
	virtual int		SampleSize( void ) = 0;
	
	// returns the sampling rate of the data
	virtual int		SampleRate( void ) = 0;

	// returns the mono/stereo status of this device (true if stereo)
	virtual bool	IsStereo( void ) = 0;

	// move up to time (time is absolute)
	virtual void	Update( float dt ) = 0;

	virtual void	Flush( void ) = 0;

	// Hook up a filter to the input channel
	virtual void	AddSource( CAudioMixer *pSource ) = 0;

	virtual void	StopSounds( void ) = 0;

	virtual void	FreeChannel( int channel ) = 0;

	virtual int		FindSourceIndex( CAudioMixer *pSource ) = 0;

	virtual float	GetAmountofTimeAhead( void ) = 0;

	virtual int		GetNumberofSamplesAhead( void ) = 0;

	virtual CAudioMixer *GetMixerForSource( CAudioSource *pDevice ) = 0;
};


int AudioResample( void *pInput,	int inCount,	int inSize,		bool inStereo,	int inRate,
				    void *pOutput,	int outCount,	int outSize,	bool outStereo, int outRate );

#endif		// SOUND_H
