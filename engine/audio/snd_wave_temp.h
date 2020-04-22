//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Create an output wave stream.  Used to record audio for in-engine movies or
// mixer debugging.
//
//=====================================================================================//

#ifndef SND_WAVE_TEMP_H
#define SND_WAVE_TEMP_H
#ifdef _WIN32
#pragma once
#endif

extern void WaveCreateTmpFile( const char *filename, int rate, int bits, int channels );
extern void WaveAppendTmpFile( const char *filename, void *buffer, int sampleBits, int numSamples );
extern void WaveFixupTmpFile( const char *filename );

#endif // SND_WAVE_TEMP_H
