//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef SND_AUDIO_SOURCE_H
#define SND_AUDIO_SOURCE_H
#pragma once


// fixed point stuff for real-time resampling
#define FIX_BITS			28
#define FIX_SCALE			(1 << FIX_BITS)
#define FIX_MASK			((1 << FIX_BITS)-1)
#define FIX_FLOAT(a)		((int)((a) * FIX_SCALE))
#define FIX(a)				(((int)(a)) << FIX_BITS)
#define FIX_INTPART(a)		(((int)(a)) >> FIX_BITS)
#define FIX_FRACTION(a,b)	(FIX(a)/(b))
#define FIX_FRACPART(a)		((a) & FIX_MASK)

typedef unsigned int fixedint;

typedef struct channel_s channel_t;

class CAudioSource;

class IAudioDevice
{
public:
	// Add a virtual destructor to silence the clang warning.
	// This is harmless but not important since the only derived class
	// doesn't have a destructor.
	virtual ~IAudioDevice() {}

	virtual void MixBegin( void ) = 0;
	virtual void Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, int rateScaleFix, int outCount, int timecompress, bool forward = true ) = 0;
	virtual void Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, int rateScaleFix, int outCount, int timecompress, bool forward = true ) = 0;
	virtual void Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, int rateScaleFix, int outCount, int timecompress, bool forward = true ) = 0;
	virtual void Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, int rateScaleFix, int outCount, int timecompress, bool forward = true ) = 0;
	virtual int MaxSampleCount( void ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: This is an instance of an audio source.
//			Mixers are attached to channels and reference an audio source.
//			Mixers are specific to the sample format and source format.
//			Mixers are never re-used, so they can track instance data like
//			sample position, fractional sample, stream cache, faders, etc.
//-----------------------------------------------------------------------------
class CAudioMixer
{
public:
	virtual ~CAudioMixer( void ) {}

	// UNDONE: time compress
	virtual bool MixDataToDevice( IAudioDevice *pDevice, channel_t *pChannel, int startSample, int sampleCount, int outputRate, bool forward = true ) = 0;
	virtual void IncrementSamples( channel_t *pChannel, int startSample, int sampleCount,int outputRate, bool forward = true ) = 0;
	virtual bool SkipSamples( channel_t *pChannel, int startSample, int sampleCount, int outputRate, bool forward = true ) = 0;

	virtual CAudioSource *GetSource( void ) = 0;

	virtual int GetSamplePosition( void ) = 0;

	virtual bool SetSamplePosition( int position ) = 0;
	virtual void SetLoopPosition( int position ) = 0;
	virtual int	GetStartPosition( void ) = 0;

	virtual bool	GetActive( void ) = 0;
	virtual void	SetActive( bool active ) = 0;

	virtual void	SetModelIndex( int index ) = 0;
	virtual int		GetModelIndex( void ) const = 0;

	virtual void	SetDirection( bool forward ) = 0;
	virtual bool	GetDirection( void ) const = 0;

	virtual void	SetAutoDelete( bool autodelete ) = 0;
	virtual bool	GetAutoDelete( void ) const = 0;
};

//-----------------------------------------------------------------------------
// Purpose: A source is an abstraction for a stream, cached file, or procedural
//			source of audio.
//-----------------------------------------------------------------------------
class CSentence;

class CAudioSource
{
public:
	CAudioSource( void );
	virtual ~CAudioSource( void );

	// Create an instance (mixer) of this audio source
	virtual CAudioMixer			*CreateMixer( void ) = 0;
	virtual int					GetOutputData( void **pData, int samplePosition, int sampleCount, bool forward = true ) = 0;
	virtual int					SampleRate( void ) = 0;
	virtual int					SampleSize( void ) = 0;
	virtual int					SampleCount( void ) = 0;
	virtual float				TrueSampleSize( void ) = 0;
	virtual bool				IsLooped( void ) = 0;
	virtual bool				IsStreaming( void ) = 0;
	virtual float				GetRunningLength( void ) = 0;

	virtual CSentence			*GetSentence( void ) { return NULL; };

};


extern CAudioSource *AudioSource_Create( const char *pName );

#endif // SND_AUDIO_SOURCE_H
