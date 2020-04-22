//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef RESAMPLE_H
#define RESAMPLE_H
#ifdef _WIN32
#pragma once
#endif

void DecimateSampleRateBy2_16( const short *pInputBuffer, short *pOutputBuffer, int sampleCount, int channelCount );
void DecompressADPCMSamples( const byte *pFormatChunk, const byte *pDataChunk, int dataSize, short *pOutputBuffer );
int ADPCMSampleCount( const byte *pFormatChunk, const byte *pDataChunk, int dataSize );
void Convert8To16( const byte *pInputBuffer, short *pOutputBuffer, int sampleCount, int channelCount );

#endif // RESAMPLE_H
