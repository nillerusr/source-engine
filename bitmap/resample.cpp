//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "nvtc.h"
#include "bitmap/imageformat.h"
#include "basetypes.h"
#include "tier0/dbg.h"
#include <malloc.h>
#include <memory.h>
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "tier1/utlmemory.h"
#include "tier1/strtools.h"
#include "mathlib/compressed_vector.h"

// Should be last include
#include "tier0/memdbgon.h"


namespace ImageLoader
{

//-----------------------------------------------------------------------------
// Gamma correction
//-----------------------------------------------------------------------------
static void ConstructFloatGammaTable( float* pTable, float srcGamma, float dstGamma )
{
	for( int i = 0; i < 256; i++ )
	{
		pTable[i] = 255.0 * pow( (float)i / 255.0f, srcGamma / dstGamma );
	}
}

void ConstructGammaTable( unsigned char* pTable, float srcGamma, float dstGamma )
{
	int v;
	for( int i = 0; i < 256; i++ )
	{
		double f;
		f = 255.0 * pow( (float)i / 255.0f, srcGamma / dstGamma );
		v = ( int )(f + 0.5f);
		if( v < 0 )
		{
			v = 0;
		}
		else if( v > 255 )
		{
			v = 255;
		}
		pTable[i] = ( unsigned char )v;
	}
}

void GammaCorrectRGBA8888( unsigned char *pSrc, unsigned char* pDst, int width, int height, int depth,
						  unsigned char* pGammaTable )
{
	for (int h = 0; h < depth; ++h )
	{
		for (int i = 0; i < height; ++i )
		{
			for (int j = 0; j < width; ++j )
			{
				int idx = (h * width * height + i * width + j) * 4;

				// don't gamma correct alpha
				pDst[idx] = pGammaTable[pSrc[idx]];
				pDst[idx+1] = pGammaTable[pSrc[idx+1]];
				pDst[idx+2] = pGammaTable[pSrc[idx+2]];
			}
		}
	}
}

void GammaCorrectRGBA8888( unsigned char *src, unsigned char* dst, int width, int height, int depth,
						  float srcGamma, float dstGamma )
{
	if (srcGamma == dstGamma)
	{
		if (src != dst)
		{
			memcpy( dst, src, GetMemRequired( width, height, depth, IMAGE_FORMAT_RGBA8888, false ) );
		}
		return;
	}

	static unsigned char gamma[256];
	static float lastSrcGamma = -1;
	static float lastDstGamma = -1;

	if (lastSrcGamma != srcGamma || lastDstGamma != dstGamma)
	{
		ConstructGammaTable( gamma, srcGamma, dstGamma );
		lastSrcGamma = srcGamma;
		lastDstGamma = dstGamma;
	}

	GammaCorrectRGBA8888( src, dst, width, height, depth, gamma );
}


//-----------------------------------------------------------------------------
// Generate a NICE filter kernel
//-----------------------------------------------------------------------------
static void GenerateNiceFilter( float wratio, float hratio, float dratio, int kernelDiameter, float* pKernel, float *pInvKernel )
{
	// Compute a kernel...
	int h, i, j;
	int kernelWidth = kernelDiameter * wratio;
	int kernelHeight = kernelDiameter * hratio;
	int kernelDepth = ( dratio != 0 ) ? kernelDiameter * dratio : 1;

	// This is a NICE filter
	// sinc pi*x * a box from -3 to 3 * sinc ( pi * x/3)
	// where x is the pixel # in the destination (shrunken) image.
	// only problem here is that the NICE filter has a very large kernel
	// (7x7 x wratio x hratio x dratio)
	float dx = 1.0f / (float)wratio;
	float dy = 1.0f / (float)hratio;
	float z, dz;

	if (dratio != 0.0f)
	{
		dz = 1.0f / (float)dratio;
		z = -((float)kernelDiameter - dz) * 0.5f; 
	}
	else
	{
		dz = 0.0f;
		z = 0.0f;
	}

	float total = 0.0f;
	for ( h = 0; h < kernelDepth; ++h )
	{
		float y = -((float)kernelDiameter - dy) * 0.5f; 
		for ( i = 0; i < kernelHeight; ++i )
		{
			float x = -((float)kernelDiameter - dx) * 0.5f; 
			for ( j = 0; j < kernelWidth; ++j )
			{
				int nKernelIndex = kernelWidth * ( i + h * kernelHeight ) + j;

				float d = sqrt( x * x + y * y + z * z );
				if (d > kernelDiameter * 0.5f)
				{
					pKernel[nKernelIndex] = 0.0f;
				}
				else
				{
					float t = M_PI * d;
					if ( t != 0 )
					{
						float sinc = sin( t ) / t;
						float sinc3 = 3.0f * sin( t / 3.0f ) / t;
						pKernel[nKernelIndex] = sinc * sinc3;
					}
					else
					{
						pKernel[nKernelIndex] = 1.0f;
					}
					total += pKernel[nKernelIndex];
				}
				x += dx;
			}
			y += dy;
		}
		z += dz;
	}

	// normalize
	float flInvFactor = ( dratio == 0 ) ? wratio * hratio : dratio * wratio * hratio;
	float flInvTotal = (total != 0.0f) ? 1.0f / total : 1.0f;

	for ( h = 0; h < kernelDepth; ++h )
	{
		for ( i = 0; i < kernelHeight; ++i )
		{
			int nPixel = kernelWidth * ( h * kernelHeight + i );
			for ( j = 0; j < kernelWidth; ++j )
			{
				pKernel[nPixel + j] *= flInvTotal;
				pInvKernel[nPixel + j] = flInvFactor * pKernel[nPixel + j]; 
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Resample an image
//-----------------------------------------------------------------------------
static inline unsigned char Clamp( float x )
{
	int idx = (int)(x + 0.5f);
	if (idx < 0) idx = 0;
	else if (idx > 255) idx = 255;
	return idx;
}

inline bool IsPowerOfTwo( int x )
{
	return (x & ( x - 1 )) == 0;
}


struct KernelInfo_t
{
	float *m_pKernel;
	float *m_pInvKernel;
	int m_nWidth;
	int m_nHeight;
	int m_nDepth;
	int m_nDiameter;
};

enum KernelType_t
{
	KERNEL_DEFAULT = 0,
	KERNEL_NORMALMAP,
	KERNEL_ALPHATEST,
};

typedef void (*ApplyKernelFunc_t)( const KernelInfo_t &kernel, const ResampleInfo_t &info, int wratio, int hratio, int dratio, float* gammaToLinear, float *pAlphaResult );

//-----------------------------------------------------------------------------
// Apply Kernel to an image
//-----------------------------------------------------------------------------
template< int type, bool bNiceFilter >
class CKernelWrapper
{
public:
	static inline int ActualX( int x, const ResampleInfo_t &info )
	{
		if ( info.m_nFlags & RESAMPLE_CLAMPS )
			return clamp( x, 0, info.m_nSrcWidth - 1 );

		// This works since info.m_nSrcWidth is a power of two.
		// Even for negative #s!
		return x & (info.m_nSrcWidth - 1);
	}

	static inline int ActualY( int y, const ResampleInfo_t &info )
	{
		if ( info.m_nFlags & RESAMPLE_CLAMPT )
			return clamp( y, 0, info.m_nSrcHeight - 1 );

		// This works since info.m_nSrcHeight is a power of two.
		// Even for negative #s!
		return y & (info.m_nSrcHeight - 1);
	}

	static inline int ActualZ( int z, const ResampleInfo_t &info )
	{
		if ( info.m_nFlags & RESAMPLE_CLAMPU )
			return clamp( z, 0, info.m_nSrcDepth - 1 );

		// This works since info.m_nSrcDepth is a power of two.
		// Even for negative #s!
		return z & (info.m_nSrcDepth - 1);
	}

	static void ComputeAveragedColor( const KernelInfo_t &kernel, const ResampleInfo_t &info, 
		int startX, int startY, int startZ, float *gammaToLinear, float *total )
	{
		total[0] = total[1] = total[2] = total[3] = 0.0f;
		for ( int j = 0, srcZ = startZ; j < kernel.m_nDepth; ++j, ++srcZ )
		{
			int sz = ActualZ( srcZ, info );
			sz *= info.m_nSrcWidth * info.m_nSrcHeight;

			for ( int k = 0, srcY = startY; k < kernel.m_nHeight; ++k, ++srcY )
			{
				int sy = ActualY( srcY, info );
				sy *= info.m_nSrcWidth;

				int kernelIdx;
				if ( bNiceFilter )
				{
					kernelIdx = kernel.m_nWidth * ( k + j * kernel.m_nHeight );
				}
				else
				{
					kernelIdx = 0;
				}

				for ( int l = 0, srcX = startX; l < kernel.m_nWidth; ++l, ++srcX, ++kernelIdx )
				{
					int sx = ActualX( srcX, info );					
					int srcPixel = (sz + sy + sx) << 2;

					float flKernelFactor;
					if ( bNiceFilter )
					{
						flKernelFactor = kernel.m_pKernel[kernelIdx];
						if ( flKernelFactor == 0.0f )
							continue;
					}
					else
					{
						flKernelFactor = kernel.m_pKernel[0];
					}

					if ( type == KERNEL_NORMALMAP )
					{
						total[0] += flKernelFactor * info.m_pSrc[srcPixel + 0];
						total[1] += flKernelFactor * info.m_pSrc[srcPixel + 1];
						total[2] += flKernelFactor * info.m_pSrc[srcPixel + 2];
						total[3] += flKernelFactor * info.m_pSrc[srcPixel + 3];
					}
					else if ( type == KERNEL_ALPHATEST )
					{
						total[0] += flKernelFactor * gammaToLinear[ info.m_pSrc[srcPixel + 0] ];
						total[1] += flKernelFactor * gammaToLinear[ info.m_pSrc[srcPixel + 1] ];
						total[2] += flKernelFactor * gammaToLinear[ info.m_pSrc[srcPixel + 2] ];
						if ( info.m_pSrc[srcPixel + 3] > 192 )
						{
							total[3] += flKernelFactor * 255.0f;
						}
					}
					else
					{
						total[0] += flKernelFactor * gammaToLinear[ info.m_pSrc[srcPixel + 0] ];
						total[1] += flKernelFactor * gammaToLinear[ info.m_pSrc[srcPixel + 1] ];
						total[2] += flKernelFactor * gammaToLinear[ info.m_pSrc[srcPixel + 2] ];
						total[3] += flKernelFactor * info.m_pSrc[srcPixel + 3];
					}
				}
			}
		}	
	}

	static void AddAlphaToAlphaResult( const KernelInfo_t &kernel, const ResampleInfo_t &info, 
		int startX, int startY, int startZ, float flAlpha, float *pAlphaResult )
	{
		for ( int j = 0, srcZ = startZ; j < kernel.m_nDepth; ++j, ++srcZ )
		{
			int sz = ActualZ( srcZ, info );
			sz *= info.m_nSrcWidth * info.m_nSrcHeight;

			for ( int k = 0, srcY = startY; k < kernel.m_nHeight; ++k, ++srcY )
			{
				int sy = ActualY( srcY, info );
				sy *= info.m_nSrcWidth;

				int kernelIdx;
				if ( bNiceFilter )
				{
					kernelIdx = k * kernel.m_nWidth + j * kernel.m_nWidth * kernel.m_nHeight;
				}
				else
				{
					kernelIdx = 0;
				}

				for ( int l = 0, srcX = startX; l < kernel.m_nWidth; ++l, ++srcX, ++kernelIdx )
				{
					int sx = ActualX( srcX, info );					
					int srcPixel = sz + sy + sx;

					float flKernelFactor;
					if ( bNiceFilter )
					{
						flKernelFactor = kernel.m_pInvKernel[kernelIdx];
						if ( flKernelFactor == 0.0f )
							continue;
					}
					else
					{
						flKernelFactor = kernel.m_pInvKernel[0];
					}

					pAlphaResult[srcPixel] += flKernelFactor * flAlpha;
				}
			}
		}
	}

	static void AdjustAlphaChannel( const KernelInfo_t &kernel, const ResampleInfo_t &info, 
		int wratio, int hratio, int dratio, float *pAlphaResult )
	{
		// Find the delta between the alpha + source image
		for ( int k = 0; k < info.m_nSrcDepth; ++k )
		{
			for ( int i = 0; i < info.m_nSrcHeight; ++i )
			{
				int dstPixel = i * info.m_nSrcWidth + k * info.m_nSrcWidth * info.m_nSrcHeight;
				for ( int j = 0; j < info.m_nSrcWidth; ++j, ++dstPixel )
				{
					pAlphaResult[dstPixel] = fabs( pAlphaResult[dstPixel] - info.m_pSrc[dstPixel * 4 + 3] );
				}
			}
		}

		// Apply the kernel to the image
		int nInitialZ = (dratio >> 1) - ((dratio * kernel.m_nDiameter) >> 1);
		int nInitialY = (hratio >> 1) - ((hratio * kernel.m_nDiameter) >> 1);
		int nInitialX = (wratio >> 1) - ((wratio * kernel.m_nDiameter) >> 1);

		float flAlphaThreshhold = (info.m_flAlphaHiFreqThreshhold >= 0 ) ? 255.0f * info.m_flAlphaHiFreqThreshhold : 255.0f * 0.4f;

		float flInvFactor = (dratio == 0) ? 1.0f / (hratio * wratio) : 1.0f / (hratio * wratio * dratio);

		for ( int h = 0; h < info.m_nDestDepth; ++h )
		{
			int startZ = dratio * h + nInitialZ;
			for ( int i = 0; i < info.m_nDestHeight; ++i )
			{
				int startY = hratio * i + nInitialY;
				int dstPixel = ( info.m_nDestWidth * (i + h * info.m_nDestHeight) ) << 2;
				for ( int j = 0; j < info.m_nDestWidth; ++j, dstPixel += 4 )
				{
					if ( info.m_pDest[ dstPixel + 3 ] == 255 )
						continue;

					int startX = wratio * j + nInitialX;
					float flAlphaDelta = 0.0f;

					for ( int m = 0, srcZ = startZ; m < dratio; ++m, ++srcZ )
					{
						int sz = ActualZ( srcZ, info );
						sz *= info.m_nSrcWidth * info.m_nSrcHeight;

						for ( int k = 0, srcY = startY; k < hratio; ++k, ++srcY )
						{
							int sy = ActualY( srcY, info );
							sy *= info.m_nSrcWidth;

							for ( int l = 0, srcX = startX; l < wratio; ++l, ++srcX )
							{
								// HACK: This temp variable fixes an internal compiler error in vs2005
								int temp = srcX;
								int sx = ActualX( temp, info );

								int srcPixel = sz + sy + sx;
								flAlphaDelta += pAlphaResult[srcPixel];
							}
						}
					}

					flAlphaDelta *= flInvFactor;
					if ( flAlphaDelta > flAlphaThreshhold )
					{
						info.m_pDest[ dstPixel + 3 ] = 255.0f;
					}
				}
			}
		}
	}

	static void ApplyKernel( const KernelInfo_t &kernel, const ResampleInfo_t &info, int wratio, int hratio, int dratio, float* gammaToLinear, float *pAlphaResult )
	{
		float invDstGamma = 1.0f / info.m_flDestGamma;

		// Apply the kernel to the image
		int nInitialZ = (dratio >> 1) - ((dratio * kernel.m_nDiameter) >> 1);
		int nInitialY = (hratio >> 1) - ((hratio * kernel.m_nDiameter) >> 1);
		int nInitialX = (wratio >> 1) - ((wratio * kernel.m_nDiameter) >> 1);

		float flAlphaThreshhold = (info.m_flAlphaThreshhold >= 0 ) ? 255.0f * info.m_flAlphaThreshhold : 255.0f * 0.4f;
		for ( int k = 0; k < info.m_nDestDepth; ++k )
		{
			int startZ = dratio * k + nInitialZ;

			for ( int i = 0; i < info.m_nDestHeight; ++i )
			{
				int startY = hratio * i + nInitialY;
				int dstPixel = (i * info.m_nDestWidth + k * info.m_nDestWidth * info.m_nDestHeight) << 2;

				for ( int j = 0; j < info.m_nDestWidth; ++j, dstPixel += 4 )
				{
					int startX = wratio * j + nInitialX;

					float total[4];
					ComputeAveragedColor( kernel, info, startX, startY, startZ, gammaToLinear, total );

					// NOTE: Can't use a table here, we lose too many bits
					if( type == KERNEL_NORMALMAP )
					{
						for ( int ch = 0; ch < 4; ++ ch )
							info.m_pDest[ dstPixel + ch ] = Clamp( info.m_flColorGoal[ch] + ( info.m_flColorScale[ch] * ( total[ch] - info.m_flColorGoal[ch] ) ) );
					}
					else if ( type == KERNEL_ALPHATEST )
					{
						// If there's more than 40% coverage, then keep the pixel (renormalize the color based on coverage)
						float flAlpha = ( total[3] >= flAlphaThreshhold ) ? 255 : 0; 

						for ( int ch = 0; ch < 3; ++ ch )
							info.m_pDest[ dstPixel + ch ] = Clamp( 255.0f * pow( ( info.m_flColorGoal[ch] + ( info.m_flColorScale[ch] * ( ( total[ch] > 0 ? total[ch] : 0 ) - info.m_flColorGoal[ch] ) ) ) / 255.0f, invDstGamma ) );
						info.m_pDest[ dstPixel + 3 ] = Clamp( flAlpha );

						AddAlphaToAlphaResult( kernel, info, startX, startY, startZ, flAlpha, pAlphaResult );
					}
					else
					{
						for ( int ch = 0; ch < 3; ++ ch )
							info.m_pDest[ dstPixel + ch ] = Clamp( 255.0f * pow( ( info.m_flColorGoal[ch] + ( info.m_flColorScale[ch] * ( ( total[ch] > 0 ? total[ch] : 0 ) - info.m_flColorGoal[ch] ) ) ) / 255.0f, invDstGamma ) );
						info.m_pDest[ dstPixel + 3 ] = Clamp( info.m_flColorGoal[3] + ( info.m_flColorScale[3] * ( total[3] - info.m_flColorGoal[3] ) ) );
					}
				}
			}

			if ( type == KERNEL_ALPHATEST )
			{
				AdjustAlphaChannel( kernel, info, wratio, hratio, dratio, pAlphaResult );
			}
		}
	}
};

typedef CKernelWrapper< KERNEL_DEFAULT, false >		ApplyKernelDefault_t;
typedef CKernelWrapper< KERNEL_NORMALMAP, false >	ApplyKernelNormalmap_t;
typedef CKernelWrapper< KERNEL_ALPHATEST, false >	ApplyKernelAlphatest_t;
typedef CKernelWrapper< KERNEL_DEFAULT, true >		ApplyKernelDefaultNice_t;
typedef CKernelWrapper< KERNEL_NORMALMAP, true >	ApplyKernelNormalmapNice_t;
typedef CKernelWrapper< KERNEL_ALPHATEST, true >	ApplyKernelAlphatestNice_t;

static ApplyKernelFunc_t g_KernelFunc[] =
{
	ApplyKernelDefault_t::ApplyKernel,
	ApplyKernelNormalmap_t::ApplyKernel,
	ApplyKernelAlphatest_t::ApplyKernel,
};

static ApplyKernelFunc_t g_KernelFuncNice[] =
{
	ApplyKernelDefaultNice_t::ApplyKernel,
	ApplyKernelNormalmapNice_t::ApplyKernel,
	ApplyKernelAlphatestNice_t::ApplyKernel,
};

bool ResampleRGBA8888( const ResampleInfo_t& info )
{
	// No resampling needed, just gamma correction
	if ( info.m_nSrcWidth == info.m_nDestWidth && info.m_nSrcHeight == info.m_nDestHeight && info.m_nSrcDepth == info.m_nDestDepth )
	{
		// Here, we need to gamma convert the source image..
		GammaCorrectRGBA8888( info.m_pSrc, info.m_pDest, info.m_nSrcWidth, info.m_nSrcHeight, info.m_nSrcDepth, info.m_flSrcGamma, info.m_flDestGamma );
		return true;
	}

	// fixme: has to be power of two for now.
	if( !IsPowerOfTwo(info.m_nSrcWidth) || !IsPowerOfTwo(info.m_nSrcHeight) || !IsPowerOfTwo(info.m_nSrcDepth) ||
		!IsPowerOfTwo(info.m_nDestWidth) || !IsPowerOfTwo(info.m_nDestHeight) || !IsPowerOfTwo(info.m_nDestDepth) )
	{
		return false;
	}

	// fixme: can only downsample for now.
	if( (info.m_nSrcWidth < info.m_nDestWidth) || (info.m_nSrcHeight < info.m_nDestHeight) || (info.m_nSrcDepth < info.m_nDestDepth) )
	{
		return false;
	}

	// Compute gamma tables...
	static float gammaToLinear[256];
	static float lastSrcGamma = -1;

	if (lastSrcGamma != info.m_flSrcGamma)
	{
		ConstructFloatGammaTable( gammaToLinear, info.m_flSrcGamma, 1.0f );
		lastSrcGamma =  info.m_flSrcGamma;
	}

	int wratio = info.m_nSrcWidth / info.m_nDestWidth;
	int hratio = info.m_nSrcHeight / info.m_nDestHeight;
	int dratio = (info.m_nSrcDepth != info.m_nDestDepth) ? info.m_nSrcDepth / info.m_nDestDepth : 0;
	
	KernelInfo_t kernel;

	float* pTempMemory = 0;
	float* pTempInvMemory = 0;
	static float* kernelCache[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	static float* pInvKernelCache[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	float pKernelMem[1];
	float pInvKernelMem[1];
	if ( info.m_nFlags & RESAMPLE_NICE_FILTER )
	{
		// Kernel size is measured in dst pixels
		kernel.m_nDiameter = 6;

		// Compute a kernel...
		kernel.m_nWidth = kernel.m_nDiameter * wratio;
		kernel.m_nHeight = kernel.m_nDiameter * hratio;
		kernel.m_nDepth = kernel.m_nDiameter * dratio;
		if ( kernel.m_nDepth == 0 )
		{
			kernel.m_nDepth = 1;
		}

		// Cache the filter (2d kernels only)....
		int power = -1;

		if ( (wratio == hratio) && (dratio == 0) )
		{
			power = 0;
			int tempWidth = wratio;
			while (tempWidth > 1)
			{
				++power;
				tempWidth >>= 1;
			}

			// Don't cache anything bigger than 512x512
			if (power >= 10)
			{
				power = -1;
			}
		}

		if (power >= 0)
		{
			if (!kernelCache[power])
			{
				kernelCache[power] = new float[kernel.m_nWidth * kernel.m_nHeight];
				pInvKernelCache[power] = new float[kernel.m_nWidth * kernel.m_nHeight];
				GenerateNiceFilter( wratio, hratio, dratio, kernel.m_nDiameter, kernelCache[power], pInvKernelCache[power] ); 
			}

			kernel.m_pKernel = kernelCache[power];
			kernel.m_pInvKernel = pInvKernelCache[power];
		}
		else
		{
			// Don't cache non-square kernels, or 3d kernels
			pTempMemory = new float[kernel.m_nWidth * kernel.m_nHeight * kernel.m_nDepth];
			pTempInvMemory = new float[kernel.m_nWidth * kernel.m_nHeight * kernel.m_nDepth];
			GenerateNiceFilter( wratio, hratio, dratio, kernel.m_nDiameter, pTempMemory, pTempInvMemory ); 
			kernel.m_pKernel = pTempMemory;
			kernel.m_pInvKernel = pTempInvMemory;
		}
	}
	else
	{
		// Compute a kernel...
		kernel.m_nWidth = wratio;
		kernel.m_nHeight = hratio;
		kernel.m_nDepth = dratio ? dratio : 1;

		kernel.m_nDiameter = 1;

		// Simple implementation of a box filter that doesn't block the stack!
		pKernelMem[0] = 1.0f / (float)(kernel.m_nWidth * kernel.m_nHeight * kernel.m_nDepth);
		pInvKernelMem[0] = 1.0f;
		kernel.m_pKernel = pKernelMem;
		kernel.m_pInvKernel = pInvKernelMem;
	}

	float *pAlphaResult = NULL;
	KernelType_t type;
	if ( info.m_nFlags & RESAMPLE_NORMALMAP )
	{
		type = KERNEL_NORMALMAP;
	}
	else if ( info.m_nFlags & RESAMPLE_ALPHATEST )
	{
		int nSize = info.m_nSrcHeight * info.m_nSrcWidth * info.m_nSrcDepth * sizeof(float);
		pAlphaResult = (float*)malloc( nSize );
		memset( pAlphaResult, 0, nSize );
		type = KERNEL_ALPHATEST;
	}
	else
	{
		type = KERNEL_DEFAULT;
	}

	if ( info.m_nFlags & RESAMPLE_NICE_FILTER )
	{	
		g_KernelFuncNice[type]( kernel, info, wratio, hratio, dratio, gammaToLinear, pAlphaResult );
		if (pTempMemory)
		{
			delete[] pTempMemory;
		}
	}
	else
	{
		g_KernelFunc[type]( kernel, info, wratio, hratio, dratio, gammaToLinear, pAlphaResult );
	}

	if ( pAlphaResult )
	{
		free( pAlphaResult );
	}

	return true;
}

bool ResampleRGBA16161616( const ResampleInfo_t& info )
{
	// HDRFIXME: This is some lame shit right here. (We need to get NICE working, etc, etc.)

	// Make sure everything is power of two.
	Assert( ( info.m_nSrcWidth & ( info.m_nSrcWidth - 1 ) ) == 0 );
	Assert( ( info.m_nSrcHeight & ( info.m_nSrcHeight - 1 ) ) == 0 );
	Assert( ( info.m_nDestWidth & ( info.m_nDestWidth - 1 ) ) == 0 );
	Assert( ( info.m_nDestHeight & ( info.m_nDestHeight - 1 ) ) == 0 );

	// Make sure that we aren't upscsaling the image. . .we do`n't support that very well.
	Assert( info.m_nSrcWidth >= info.m_nDestWidth );
	Assert( info.m_nSrcHeight >= info.m_nDestHeight );

	int nSampleWidth = info.m_nSrcWidth / info.m_nDestWidth;
	int nSampleHeight = info.m_nSrcHeight / info.m_nDestHeight;

	unsigned short *pSrc = ( unsigned short * )info.m_pSrc;
	unsigned short *pDst = ( unsigned short * )info.m_pDest;
	int x, y;
	for( y = 0; y < info.m_nDestHeight; y++ )
	{
		for( x = 0; x < info.m_nDestWidth; x++ )
		{
			int accum[4];
			accum[0] = accum[1] = accum[2] = accum[3] = 0;
			int nSampleY;
			for( nSampleY = 0; nSampleY < nSampleHeight; nSampleY++ )
			{
				int nSampleX;
				for( nSampleX = 0; nSampleX < nSampleWidth; nSampleX++ )
				{
					accum[0] += ( int )pSrc[((x*nSampleWidth+nSampleX)+(y*nSampleHeight+nSampleY)*info.m_nSrcWidth)*4+0];
					accum[1] += ( int )pSrc[((x*nSampleWidth+nSampleX)+(y*nSampleHeight+nSampleY)*info.m_nSrcWidth)*4+1];
					accum[2] += ( int )pSrc[((x*nSampleWidth+nSampleX)+(y*nSampleHeight+nSampleY)*info.m_nSrcWidth)*4+2];
					accum[3] += ( int )pSrc[((x*nSampleWidth+nSampleX)+(y*nSampleHeight+nSampleY)*info.m_nSrcWidth)*4+3];
				}
			}
			int i;
			for( i = 0; i < 4; i++ )
			{
				accum[i] /= ( nSampleWidth * nSampleHeight );
				accum[i] = max( accum[i], 0 );
				accum[i] = min( accum[i], 65535 );
				pDst[(x+y*info.m_nDestWidth)*4+i] = ( unsigned short )accum[i];
			}
		}
	}
	return true;
}

bool ResampleRGB323232F( const ResampleInfo_t& info )
{
	// HDRFIXME: This is some lame shit right here. (We need to get NICE working, etc, etc.)

	// Make sure everything is power of two.
	Assert( ( info.m_nSrcWidth & ( info.m_nSrcWidth - 1 ) ) == 0 );
	Assert( ( info.m_nSrcHeight & ( info.m_nSrcHeight - 1 ) ) == 0 );
	Assert( ( info.m_nDestWidth & ( info.m_nDestWidth - 1 ) ) == 0 );
	Assert( ( info.m_nDestHeight & ( info.m_nDestHeight - 1 ) ) == 0 );

	// Make sure that we aren't upscaling the image. . .we do`n't support that very well.
	Assert( info.m_nSrcWidth >= info.m_nDestWidth );
	Assert( info.m_nSrcHeight >= info.m_nDestHeight );

	int nSampleWidth = info.m_nSrcWidth / info.m_nDestWidth;
	int nSampleHeight = info.m_nSrcHeight / info.m_nDestHeight;

	float *pSrc = ( float * )info.m_pSrc;
	float *pDst = ( float * )info.m_pDest;
	int x, y;
	for( y = 0; y < info.m_nDestHeight; y++ )
	{
		for( x = 0; x < info.m_nDestWidth; x++ )
		{
			float accum[4];
			accum[0] = accum[1] = accum[2] = accum[3] = 0;
			int nSampleY;
			for( nSampleY = 0; nSampleY < nSampleHeight; nSampleY++ )
			{
				int nSampleX;
				for( nSampleX = 0; nSampleX < nSampleWidth; nSampleX++ )
				{
					accum[0] += pSrc[((x*nSampleWidth+nSampleX)+(y*nSampleHeight+nSampleY)*info.m_nSrcWidth)*3+0];
					accum[1] += pSrc[((x*nSampleWidth+nSampleX)+(y*nSampleHeight+nSampleY)*info.m_nSrcWidth)*3+1];
					accum[2] += pSrc[((x*nSampleWidth+nSampleX)+(y*nSampleHeight+nSampleY)*info.m_nSrcWidth)*3+2];
				}
			}
			int i;
			for( i = 0; i < 3; i++ )
			{
				accum[i] /= ( nSampleWidth * nSampleHeight );
				pDst[(x+y*info.m_nDestWidth)*3+i] = accum[i];
			}
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// Generates mipmap levels
//-----------------------------------------------------------------------------
void GenerateMipmapLevels( unsigned char* pSrc, unsigned char* pDst, int width,
	int height,	int depth, ImageFormat imageFormat, float srcGamma, float dstGamma, int numLevels )
{
	int dstWidth = width;
	int dstHeight = height;
	int dstDepth = depth;

	// temporary storage for the mipmaps
	int tempMem = GetMemRequired( dstWidth, dstHeight, dstDepth, IMAGE_FORMAT_RGBA8888, false );
	CUtlMemory<unsigned char> tmpImage;
	tmpImage.EnsureCapacity( tempMem );

	while( true )
	{
		// This generates a mipmap in RGBA8888, linear space
		ResampleInfo_t info;
		info.m_pSrc = pSrc;
		info.m_pDest = tmpImage.Base();
		info.m_nSrcWidth = width;
		info.m_nSrcHeight = height;
		info.m_nSrcDepth = depth;
		info.m_nDestWidth = dstWidth;
		info.m_nDestHeight = dstHeight;
		info.m_nDestDepth = dstDepth;
		info.m_flSrcGamma = srcGamma;
		info.m_flDestGamma = dstGamma;

		ResampleRGBA8888( info );

		// each mipmap level needs to be color converted separately
		ConvertImageFormat( tmpImage.Base(), IMAGE_FORMAT_RGBA8888,
			pDst, imageFormat, dstWidth, dstHeight, 0, 0 );

		if (numLevels == 0)
		{
			// We're done after we've made the 1x1 mip level
			if (dstWidth == 1 && dstHeight == 1 && dstDepth == 1)
				return;
		}
		else
		{
			if (--numLevels <= 0)
				return;
		}

		// Figure out where the next level goes
		int memRequired = ImageLoader::GetMemRequired( dstWidth, dstHeight, dstDepth, imageFormat, false);
		pDst += memRequired;

		// shrink by a factor of 2, but clamp at 1 pixel (non-square textures)
		dstWidth = dstWidth > 1 ? dstWidth >> 1 : 1;
		dstHeight = dstHeight > 1 ? dstHeight >> 1 : 1;
		dstDepth = dstDepth > 1 ? dstDepth >> 1 : 1;
	}
}

void GenerateMipmapLevelsLQ( unsigned char* pSrc, unsigned char* pDst, int width, int height,
		                     ImageFormat imageFormat, int numLevels )
{
	CUtlMemory<unsigned char> tmpImage;

	const unsigned char* pSrcLevel = pSrc;

	int mipmap0Size = GetMemRequired( width, height, 1, IMAGE_FORMAT_RGBA8888, false );

	// TODO: Could work with any 8888 format without conversion.
	if ( imageFormat != IMAGE_FORMAT_RGBA8888 )
	{
		// Damn and blast, had to allocate memory.
		tmpImage.EnsureCapacity( mipmap0Size );
		ConvertImageFormat( tmpImage.Base(), IMAGE_FORMAT_RGBA8888, pSrc, imageFormat, width, height, 0, 0 );
		pSrcLevel = tmpImage.Base();
	}

	// Copy the 0th level over.
	memcpy( pDst, pSrcLevel, mipmap0Size );
	
	int dstWidth = width;
	int dstHeight = height;
	unsigned char* pDstLevel = pDst + mipmap0Size;

	int srcWidth = width;
	int srcHeight = height;

	// Distance from one pixel to the next
	const int cStride = 4;
	
	do 
	{
		dstWidth =  Max( 1, dstWidth  >> 1 );
		dstHeight = Max( 1, dstHeight >> 1 );

		// Distance from one row to the next. 
		const int cSrcPitch = cStride * srcWidth * ( srcHeight > 1 ? 1 : 0);
		const int cSrcStride = srcWidth > 1 ? cStride : 0;

		const unsigned char* pSrcPixel = pSrcLevel;
		unsigned char* pDstPixel = pDstLevel;

		for ( int j = 0; j < dstHeight; ++j )
		{
			for ( int i = 0; i < dstWidth; ++i ) 
			{
				// This doesn't round. It's crappy. It's a simple bilerp. 
				pDstPixel[ 0 ] = ( ( unsigned int ) pSrcPixel[ 0 ] + ( unsigned int ) pSrcPixel[ 0 + cSrcStride ] + ( unsigned int ) pSrcPixel[ 0 + cSrcPitch ] + ( unsigned int ) pSrcPixel[ 0 + cSrcPitch + cSrcStride ] ) >> 2;
				pDstPixel[ 1 ] = ( ( unsigned int ) pSrcPixel[ 1 ] + ( unsigned int ) pSrcPixel[ 1 + cSrcStride ] + ( unsigned int ) pSrcPixel[ 1 + cSrcPitch ] + ( unsigned int ) pSrcPixel[ 1 + cSrcPitch + cSrcStride ] ) >> 2;
				pDstPixel[ 2 ] = ( ( unsigned int ) pSrcPixel[ 2 ] + ( unsigned int ) pSrcPixel[ 2 + cSrcStride ] + ( unsigned int ) pSrcPixel[ 2 + cSrcPitch ] + ( unsigned int ) pSrcPixel[ 2 + cSrcPitch + cSrcStride ] ) >> 2;
				pDstPixel[ 3 ] = ( ( unsigned int ) pSrcPixel[ 3 ] + ( unsigned int ) pSrcPixel[ 3 + cSrcStride ] + ( unsigned int ) pSrcPixel[ 3 + cSrcPitch ] + ( unsigned int ) pSrcPixel[ 3 + cSrcPitch + cSrcStride ] ) >> 2;
				
				pDstPixel += cStride;
				pSrcPixel += cStride * 2; // We advance 2 source pixels for each pixel.
			}

			// Need to bump down a row. 
			pSrcPixel += cSrcPitch;
		}

		// Update for the next go round!
		pSrcLevel = pDstLevel;
		pDstLevel += GetMemRequired( dstWidth, dstHeight, 1, IMAGE_FORMAT_RGBA8888, false );

		srcWidth =  Max( 1, srcWidth  >> 1 );
		srcHeight = Max( 1, srcHeight >> 1 );
	} while ( srcWidth > 1 || srcHeight > 1 );
}


} // ImageLoader namespace ends

