//========= Copyright Valve Corporation, All rights reserved. ============//

#include <windows.h>
#include <mmreg.h>
#include "../toollib/toollib.h"
#include "tier1/strtools.h"
#include "resample.h"

#define clamp(a,b,c) ( (a) > (c) ? (c) : ( (a) < (b) ? (b) : (a) ) )

const int NUM_COEFFS = 7;
static const float g_ResampleCoefficients[NUM_COEFFS] =
{
	0.0457281f, 0.168088f, 0.332501f, 0.504486f, 0.663202f, 0.803781f, 0.933856f
};


// generates 1 output sample for 2 input samples
inline float DecimateSamplePair(float input0, float input1, const float pCoefficients[7], float xState[2], float yState[7] )
{
	float tmp_0 = xState[0];
	float tmp_1 = xState[1];
	
	xState[0] = input0;
	xState[1] = input1;

	input0 = (input0 - yState[0]) * pCoefficients[0] + tmp_0;
	input1 = (input1 - yState[1]) * pCoefficients[1] + tmp_1;
	tmp_0 = yState[0];
	tmp_1 = yState[1];
	yState[0] = input0;
	yState[1] = input1;

	input0 = (input0 - yState[2]) * pCoefficients[2] + tmp_0;
	input1 = (input1 - yState[3]) * pCoefficients[3] + tmp_1;
	tmp_0 = yState[2];
	tmp_1 = yState[3];
	yState[2] = input0;
	yState[3] = input1;

	input0 = (input0 - yState[4]) * pCoefficients[4] + tmp_0;
	input1 = (input1 - yState[5]) * pCoefficients[5] + tmp_1;
	tmp_0 = yState[4];
	yState[4] = input0;
	yState[5] = input1;

	input0 = (input0 - yState[6]) * pCoefficients[6] + tmp_0;
	yState[6] = input0;

	return (input0 + input1);
}

static void ExtractFloatSamples( float *pOut, const short *pInputBuffer, int sampleCount, int stride )
{
	for ( int i = 0; i < sampleCount; i++ )
	{
		pOut[i] = pInputBuffer[0] * 1.0f / 32768.0f;
		pInputBuffer += stride;
	}
}

static void ExtractShortSamples( short *pOut, const float *pInputBuffer, float scale, int sampleCount, int stride )
{
	for ( int i = 0; i < sampleCount; i++ )
	{
		int sampleOut = (int)(pInputBuffer[i] * scale);
		sampleOut = clamp( sampleOut, -32768, 32767 );

		pOut[0] = (short)(sampleOut);
		pOut += stride;
	}
}

struct decimatestate_t
{
	float xState[2];
	float yState[7];
};
void DecimateSampleBlock( float *pInOut, int sampleCount )
{
	decimatestate_t block;
	int outCount = sampleCount >> 1;
	int pos = 0;
	memset( &block, 0, sizeof(block) );
	do 
	{
		float input0 = pInOut[pos*2+0];
		float input1 = pInOut[pos*2+1];
		pInOut[pos] = DecimateSamplePair( input0, input1, g_ResampleCoefficients, block.xState, block.yState );
		pos++;
	} while( pos < outCount );
}

void DecimateSampleRateBy2_16( const short *pInputBuffer, short *pOutputBuffer, int sampleCount, int channelCount )
{
	float *pTmpBuf = new float[sampleCount];
	for ( int i = 0; i < channelCount; i++ )
	{
		ExtractFloatSamples( pTmpBuf, pInputBuffer+i, sampleCount, channelCount );
		DecimateSampleBlock( pTmpBuf, sampleCount );
		ExtractShortSamples( pOutputBuffer+i, pTmpBuf, 0.5f * 32768.0f, sampleCount>>1, channelCount );
	}
	delete [] pTmpBuf;
}


struct adpcmstate_t
{
	const ADPCMWAVEFORMAT	*pFormat;
	const ADPCMCOEFSET		*pCoefficients;
	int						blockSize;
};

static int error_sign_lut[] =		  { 0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1 };
static int error_coefficients_lut[] = { 230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230 };

void ParseADPCM( adpcmstate_t &out, const byte *pFormatChunk )
{
	out.pFormat = (const ADPCMWAVEFORMAT *)pFormatChunk;
	if ( out.pFormat )
	{
		out.pCoefficients = out.pFormat->aCoef;

		// number of bytes for samples
		out.blockSize = ((out.pFormat->wSamplesPerBlock - 2) * out.pFormat->wfx.nChannels ) / 2;
		// size of channel header
		out.blockSize += 7 * out.pFormat->wfx.nChannels;
	}
}

void DecompressBlockMono( const adpcmstate_t &state, short *pOut, const char *pIn, int count )
{
	int pred = *pIn++;
	int co1 = state.pCoefficients[pred].iCoef1;
	int co2 = state.pCoefficients[pred].iCoef2;

	// read initial delta
	int delta = *((short *)pIn);
	pIn += 2;

	// read initial samples for prediction
	int samp1 = *((short *)pIn);
	pIn += 2;

	int samp2 = *((short *)pIn);
	pIn += 2;

	// write out the initial samples (stored in reverse order)
	*pOut++ = (short)samp2;
	*pOut++ = (short)samp1;

	// subtract the 2 samples in the header
	count -= 2;

	// this is a toggle to read nibbles, first nibble is high
	int high = 1;

	int error, sample=0;

	// now process the block
	while ( count )
	{
		// read the error nibble from the input stream
		if ( high )
		{
			sample = (unsigned char) (*pIn++);
			// high nibble
			error = sample >> 4;
			// cache low nibble for next read
			sample = sample & 0xf;
			// Next read is from cache, not stream
			high = 0;
		}
		else
		{
			// stored in previous read (low nibble)
			error = sample;
			// next read is from stream
			high = 1;
		}
		// convert to signed with LUT
		int errorSign = error_sign_lut[error];

		// interpolate the new sample
		int predSample = (samp1 * co1) + (samp2 * co2);
		// coefficients are fixed point 8-bit, so shift back to 16-bit integer
		predSample >>= 8;

		// Add in current error estimate
		predSample += (errorSign * delta);

		// Correct error estimate
		delta = (delta * error_coefficients_lut[error]) >> 8;
		// Clamp error estimate
		if ( delta < 16 )
			delta = 16;

		// clamp
		if ( predSample > 32767L )
			predSample = 32767L;
		else if ( predSample < -32768L )
			predSample = -32768L;

		// output
		*pOut++ = (short)predSample;
		// move samples over
		samp2 = samp1;
		samp1 = predSample;

		count--;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Decode a single block of stereo ADPCM audio
// Input  : *pOut - 16-bit output buffer
//			*pIn - ADPCM encoded block data
//			count - number of sample pairs to decode
//-----------------------------------------------------------------------------
void DecompressBlockStereo( const adpcmstate_t &state, short *pOut, const char *pIn, int count )
{
	int pred[2], co1[2], co2[2];
	int i;

	for ( i = 0; i < 2; i++ )
	{
		pred[i] = *pIn++;
		co1[i] = state.pCoefficients[pred[i]].iCoef1;
		co2[i] = state.pCoefficients[pred[i]].iCoef2;
	}

	int delta[2], samp1[2], samp2[2];

	for ( i = 0; i < 2; i++, pIn += 2 )
	{
		// read initial delta
		delta[i] = *((short *)pIn);
	}

	// read initial samples for prediction
	for ( i = 0; i < 2; i++, pIn += 2 )
	{
		samp1[i] = *((short *)pIn);
	}
	for ( i = 0; i < 2; i++, pIn += 2 )
	{
		samp2[i] = *((short *)pIn);
	}

	// write out the initial samples (stored in reverse order)
	*pOut++ = (short)samp2[0];	// left
	*pOut++ = (short)samp2[1];	// right
	*pOut++ = (short)samp1[0];	// left
	*pOut++ = (short)samp1[1];	// right

	// subtract the 2 samples in the header
	count -= 2;

	// this is a toggle to read nibbles, first nibble is high
	int high = 1;

	int error, sample=0;

	// now process the block
	while ( count )
	{
		for ( i = 0; i < 2; i++ )
		{
			// read the error nibble from the input stream
			if ( high )
			{
				sample = (unsigned char) (*pIn++);
				// high nibble
				error = sample >> 4;
				// cache low nibble for next read
				sample = sample & 0xf;
				// Next read is from cache, not stream
				high = 0;
			}
			else
			{
				// stored in previous read (low nibble)
				error = sample;
				// next read is from stream
				high = 1;
			}
			// convert to signed with LUT
			int errorSign = error_sign_lut[error];

			// interpolate the new sample
			int predSample = (samp1[i] * co1[i]) + (samp2[i] * co2[i]);
			// coefficients are fixed point 8-bit, so shift back to 16-bit integer
			predSample >>= 8;

			// Add in current error estimate
			predSample += (errorSign * delta[i]);

			// Correct error estimate
			delta[i] = (delta[i] * error_coefficients_lut[error]) >> 8;
			// Clamp error estimate
			if ( delta[i] < 16 )
				delta[i] = 16;

			// clamp
			if ( predSample > 32767L )
				predSample = 32767L;
			else if ( predSample < -32768L )
				predSample = -32768L;

			// output
			*pOut++ = (short)predSample;
			// move samples over
			samp2[i] = samp1[i];
			samp1[i] = predSample;
		}
		count--;
	}
}

int ADPCMSampleCountShortBlock( const adpcmstate_t &state, int shortBlockSize )
{
	if ( shortBlockSize < 8 )
		return 0;

	int sampleCount = state.pFormat->wSamplesPerBlock;

	// short block?, fixup sample count (2 samples per byte, divided by number of channels per sample set)
	sampleCount -= ((state.blockSize - shortBlockSize) * 2) / state.pFormat->wfx.nChannels;
	return sampleCount;
}

int ADPCMSampleCount( const byte *pFormatChunk, const byte *pDataChunk, int dataSize )
{
	adpcmstate_t state;
	ParseADPCM( state, pFormatChunk );
	int numBlocks = dataSize / state.blockSize;
	int mod = dataSize % state.blockSize;
	return numBlocks * state.pFormat->wSamplesPerBlock + ADPCMSampleCountShortBlock(state, mod);
}

void DecompressADPCMSamples( const byte *pFormatChunk, const byte *pDataChunk, int dataSize, short *pOutputBuffer )
{
	adpcmstate_t state;
	ParseADPCM( state, pFormatChunk );

	while ( dataSize > 0 )
	{
		int block = dataSize;
		int sampleCount = state.pFormat->wSamplesPerBlock;
		if ( block > state.blockSize )
		{
			block = state.blockSize;
		}
		else
		{
			sampleCount = ADPCMSampleCountShortBlock( state, block );
		}
		if ( state.pFormat->wfx.nChannels == 1 )
		{
			DecompressBlockMono( state, pOutputBuffer, (const char *)pDataChunk, sampleCount );
		}
		else
		{
			DecompressBlockStereo( state, pOutputBuffer, (const char *)pDataChunk, sampleCount );
		}
		pOutputBuffer += sampleCount * state.pFormat->wfx.nChannels;
		dataSize -= block;
		pDataChunk += block;
	}
}

void Convert8To16( const byte *pInputBuffer, short *pOutputBuffer, int sampleCount, int channelCount )
{
	for ( int i = 0; i < sampleCount*channelCount; i++ )
	{
		unsigned short signedSample = (byte)((int)((unsigned)pInputBuffer[i]) - 128);
		pOutputBuffer[i] = (short) (signedSample | (signedSample<<8));
	}
}

