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

extern char *COPY_STRING( const char *pString );

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
