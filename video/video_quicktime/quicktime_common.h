//========= Copyright Valve Corporation, All rights reserved. ============//
//
// File: quicktime_common.h
//
//  QuickTime limits and constants shared among all QuickTime functions
//
//=============================================================================


#ifndef QUICKTIME_COMMON_H
#define QUICKTIME_COMMON_H

#ifdef _WIN32
#pragma once
#endif

#ifdef OSX
// The OSX 10.7 SDK dropped support for the functions below, so we manually pull them in
#include <dlfcn.h>
typedef PixMapHandle
(*PFNGetGWorldPixMap)(GWorldPtr offscreenGWorld);
typedef Ptr
(*PFNGetPixBaseAddr)(PixMapHandle pm);
typedef Boolean
(*PFNLockPixels)(PixMapHandle pm);
typedef void
(*PFNUnlockPixels)(PixMapHandle pm);
typedef void
(*PFNDisposeGWorld)(GWorldPtr offscreenGWorld);
typedef void
(*PFNSetGWorld)(CGrafPtr   port,GDHandle   gdh);
typedef SInt32
(*PFNGetPixRowBytes)(PixMapHandle pm);


extern PFNGetGWorldPixMap GetGWorldPixMap;
extern PFNGetPixBaseAddr GetPixBaseAddr;
extern PFNLockPixels LockPixels;
extern PFNUnlockPixels UnlockPixels;
extern PFNDisposeGWorld DisposeGWorld;
extern PFNSetGWorld SetGWorld;
extern PFNGetPixRowBytes GetPixRowBytes;
#endif


// constant that define the bounds of various inputs
static const int	cMinVideoFrameWidth = 16;
static const int	cMinVideoFrameHeight = 16;
static const int	cMaxVideoFrameWidth = 2 * 2048;
static const int	cMaxVideoFrameHeight = 2 * 2048;

static const int	cMinFPS = 1;
static const int	cMaxFPS = 600;

static const float  cMinDuration = 0.016666666f;		// 1/60th second
static const float  cMaxDuration = 3600.0f;				// 1 Hour

static const int	cMinSampleRate = 11025;				// 1/4 CD sample rate
static const int	cMaxSampleRate = 88200;				// 2x CD rate

#define NO_MORE_INTERESTING_TIMES   -2
#define END_OF_QUICKTIME_MOVIE		-1

// ===========================================================================
// Macros & Utility functions
// ===========================================================================

#define SAFE_DISPOSE_HANDLE( _handle )		if ( _handle != nullptr ) { DisposeHandle( (Handle) _handle ); _handle = nullptr; }
#define SAFE_DISPOSE_GWORLD( _gworld )		if ( _gworld != nullptr ) { DisposeGWorld( _gworld ); _gworld = nullptr; }
#define SAFE_DISPOSE_MOVIE( _movie )		if ( _movie != nullptr ) { DisposeMovie( _movie ); ThreadSleep(10); Assert( GetMoviesError() == noErr ); _movie = nullptr; }
#define SAFE_RELEASE_AUDIOCONTEXT( _cxt )	if ( _cxt != nullptr ) { QTAudioContextRelease( _cxt ); _cxt = nullptr; }
#define SAFE_RELEASE_CFREF( _ref )			if ( _ref != nullptr ) { CFRelease( _ref ); _ref = nullptr; }
// Utility functions

extern char *COPY_STRING( const char *pString );

bool	MovieGetStaticFrameRate( Movie inMovie, VideoFrameRate_t &theFrameRate );

bool	SetGWorldDecodeGamma( CGrafPtr theGWorld, VideoPlaybackGamma_t gamma );

bool	CreateMovieAudioContext( bool enableAudio, Movie inMovie, QTAudioContextRef *pAudioConectext, bool setVolume = false, float *pCurrentVolume = nullptr );

VideoPlaybackGamma_t GetSystemPlaybackGamma();


//-----------------------------------------------------------------------------
// Computes a power of two at least as big as the passed-in number
//-----------------------------------------------------------------------------
static inline int ComputeGreaterPowerOfTwo( int n )
{
	int i = 1;
	while ( i < n )
	{
		i <<= 1;
	}
	return i;
}


#endif	// QUICKTIME_COMMON_H
