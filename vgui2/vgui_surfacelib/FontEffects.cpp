//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Font effects that operate on linear rgba data
//
//=====================================================================================//

#include "tier0/platform.h"
#include <tier0/dbg.h>
#include <math.h>
#include "FontEffects.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Adds center line to font
//-----------------------------------------------------------------------------
void ApplyRotaryEffectToTexture( int rgbaWide, int rgbaTall, unsigned char *rgba, bool bRotary )
{
	if ( !bRotary )
		return;

	int y = rgbaTall * 0.5;

	unsigned char *line = &rgba[(y * rgbaWide) * 4];

	// Draw a line down middle
	for (int x = 0; x < rgbaWide; x++, line+=4)
	{
		line[0] = 127;
		line[1] = 127;
		line[2] = 127;
		line[3] = 255;
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds scanlines to the texture
//-----------------------------------------------------------------------------
void ApplyScanlineEffectToTexture( int rgbaWide, int rgbaTall, unsigned char *rgba, int iScanLines )
{
	if ( iScanLines < 2 )
		return;

	float scale;
	scale = 0.7f;

	// darken all the areas except the scanlines
	for (int y = 0; y < rgbaTall; y++)
	{
		// skip the scan lines
		if (y % iScanLines == 0)
			continue;

		unsigned char *pBits = &rgba[(y * rgbaWide) * 4];

		// darken the other lines
		for (int x = 0; x < rgbaWide; x++, pBits += 4)
		{
			pBits[0] *= scale;
			pBits[1] *= scale;
			pBits[2] *= scale;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds a dropshadow the the font texture
//-----------------------------------------------------------------------------
void ApplyDropShadowToTexture( int rgbaWide, int rgbaTall, unsigned char *rgba, int iDropShadowOffset )
{
	if ( !iDropShadowOffset )
		return;

	// walk the original image from the bottom up
	// shifting it down and right, and turning it black (the dropshadow)
	for (int y = rgbaTall - 1; y >= iDropShadowOffset; y--)
	{
		for (int x = rgbaWide - 1; x >= iDropShadowOffset; x--)
		{
			unsigned char *dest = &rgba[(x +  (y * rgbaWide)) * 4];
			if (dest[3] == 0)
			{
				// there is nothing in this spot, copy in the dropshadow
				unsigned char *src = &rgba[(x - iDropShadowOffset + ((y - iDropShadowOffset) * rgbaWide)) * 4];
				dest[0] = 0;
				dest[1] = 0;
				dest[2] = 0;
				dest[3] = src[3];
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds an outline to the font texture
//-----------------------------------------------------------------------------
void ApplyOutlineToTexture( int rgbaWide, int rgbaTall, unsigned char *rgba, int iOutlineSize )
{
	if ( !iOutlineSize )
		return;

	int x, y;
	for( y = 0; y < rgbaTall; y++ )
	{
		for( x = 0; x < rgbaWide; x++ )
		{
			unsigned char *src = &rgba[(x + (y * rgbaWide)) * 4];
			if( src[3] == 0 )
			{
				// We have a valid font texel.  Make all the alpha == 0 neighbors black.
				int shadowX, shadowY;
				for( shadowX = -(int)iOutlineSize; shadowX <= (int)iOutlineSize; shadowX++ )
				{
					for( shadowY = -(int)iOutlineSize; shadowY <= (int)iOutlineSize; shadowY++ )
					{
						if( shadowX == 0 && shadowY == 0 )
						{
							continue;
						}
						int testX, testY;
						testX = shadowX + x;
						testY = shadowY + y;
						if( testX < 0 || testX >= rgbaWide ||
							testY < 0 || testY >= rgbaTall )
						{
							continue;
						}
						unsigned char *test = &rgba[(testX + (testY * rgbaWide)) * 4];
						if( test[0] != 0 && test[1] != 0 && test[2] != 0 && test[3] != 0 )
						{
							src[0] = 0;
							src[1] = 0;
							src[2] = 0;
							src[3] = 255;
						}
					}
				}
			}
		}
	}
}

namespace
{

	unsigned char CalculatePixelBlur(const unsigned char* src, int nStride, const float* distribution, int nValues)
	{
		float accum = 0.0;
		for ( int n = 0; n != nValues; ++n )
		{
			accum += distribution[n]*static_cast<float>(src[n*nStride]);
		}

		return static_cast<unsigned char>(accum);
	}

}

//-----------------------------------------------------------------------------
// Purpose: blurs the texture
//-----------------------------------------------------------------------------
void ApplyGaussianBlurToTexture( int rgbaWide, int rgbaTall, unsigned char *rgba, int nBlur )
{
	if ( !nBlur  )
		return;

	// generate the gaussian field
	float *pGaussianDistribution = (float*) stackalloc( (nBlur*2+1) * sizeof(float) );
	double sigma = 0.683 * nBlur;
	for (int x = 0; x <= (nBlur * 2); x++)
	{
		int val = x - nBlur;
		pGaussianDistribution[x] = (float)( 1.0f / sqrt(2 * 3.14 * sigma * sigma)) * pow(2.7, -1 * (val * val) / (2 * sigma * sigma));
	}

	// alloc a new buffer
	unsigned char *src = (unsigned char *) stackalloc( rgbaWide * rgbaTall * 4);

	// copy in
	memcpy(src, rgba, rgbaWide * rgbaTall * 4);

	//make an initial horizontal pass
	for ( int x = 0; x < rgbaWide; x++ )
	{
		const float* dist = pGaussianDistribution;
		int nValues = nBlur*2 + 1;
		int nOffset = 0;
		if ( x < nBlur )
		{
			nOffset += nBlur - x;
			dist += nOffset;
			nValues -= nOffset;
		}

		if ( x >= rgbaWide - nBlur )
		{
			nValues = rgbaWide - (x - nOffset);
		}

		for ( int y = 0; y < rgbaTall; y++ )
		{
			const unsigned char* read_from = src + (y*rgbaWide + x + nOffset - nBlur)*4 + 3;
			unsigned char* dst = rgba + (y*rgbaWide + x)*4;
			unsigned char alpha = CalculatePixelBlur(read_from, 4, dist, nValues);
			dst[0] = dst[1] = dst[2] = alpha > 0 ? 255 : 0;
			dst[3] = alpha;
		}
	}

	// refresh the source buffer for a second vertical pass
	memcpy(src, rgba, rgbaWide * rgbaTall * 4);

	for ( int y = 0; y < rgbaTall; y++ )
	{
		const float* dist = pGaussianDistribution;
		int nValues = nBlur*2 + 1;
		int nOffset = 0;
		if ( y < nBlur )
		{
			nOffset += nBlur - y;
			dist += nOffset;
			nValues -= nOffset;
		}

		if ( y >= rgbaTall - nBlur )
		{
			nValues = rgbaTall - (y - nOffset);
		}

		for ( int x = 0; x < rgbaWide; x++ )
		{
			const unsigned char* read_from = src + ((y + nOffset - nBlur)*rgbaWide + x)*4 + 3;
			unsigned char* dst = rgba + (y*rgbaWide + x)*4;
			unsigned char alpha = CalculatePixelBlur(read_from, 4*rgbaWide, dist, nValues);
			dst[0] = dst[1] = dst[2] = alpha > 0 ? 255 : 0;
			dst[3] = alpha;
		}
	}
}
