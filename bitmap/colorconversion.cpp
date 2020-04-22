//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifdef IS_WINDOWS_PC
#include <windows.h>
#endif
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
#include "nvtc.h"

#ifdef POSIX
typedef int32 *DWORD_PTR;
#endif

#include "ATI_Compress.h"
#include "bitmap/float_bm.h"

#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"

// Should be last include
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Various important function types for each color format
//-----------------------------------------------------------------------------

typedef void (*UserFormatToRGBA8888Func_t )( const uint8 *src, uint8 *dst, int numPixels );
typedef void (*RGBA8888ToUserFormatFunc_t )( const uint8 *src, uint8 *dst, int numPixels );


namespace ImageLoader
{

// Color Conversion functions
static void RGBA8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToABGR8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToRGB888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToBGR888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToRGB565( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToI8( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToIA88( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToP8( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToA8( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToRGB888_BLUESCREEN( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToBGR888_BLUESCREEN( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToARGB8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToBGRA8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToBGRX8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToBGR565( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToBGRX5551( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToBGRA5551( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToBGRA4444( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToUV88( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToUVWQ8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA8888ToUVLX8888( const uint8 *src, uint8 *dst, int numPixels );
//static void RGBA8888ToRGBA16161616F( const uint8 *src, uint8 *dst, int numPixels );

static void ABGR8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGB888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void BGR888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGB565ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void I8ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void IA88ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void P8ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void A8ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGB888_BLUESCREENToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void BGR888_BLUESCREENToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void ARGB8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void BGRA8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void BGRX8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void BGR565ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void BGRX5551ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void BGRA5551ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void BGRA4444ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void UV88ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void UVWQ8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void UVLX8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
static void RGBA16161616ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );
//static void RGBA16161616FToRGBA8888( const uint8 *src, uint8 *dst, int numPixels );


static UserFormatToRGBA8888Func_t GetUserFormatToRGBA8888Func_t( ImageFormat srcImageFormat )
{
	switch( srcImageFormat )
	{
	case IMAGE_FORMAT_RGBA8888:
		return RGBA8888ToRGBA8888;
	case IMAGE_FORMAT_ABGR8888:
		return ABGR8888ToRGBA8888;
	case IMAGE_FORMAT_RGB888:
		return RGB888ToRGBA8888;
	case IMAGE_FORMAT_BGR888:
		return BGR888ToRGBA8888;
	case IMAGE_FORMAT_RGB565:
		return NULL;
	case IMAGE_FORMAT_I8:
		return I8ToRGBA8888;
	case IMAGE_FORMAT_IA88:
		return IA88ToRGBA8888;
	case IMAGE_FORMAT_A8:
		return A8ToRGBA8888;
	case IMAGE_FORMAT_RGB888_BLUESCREEN:
		return RGB888_BLUESCREENToRGBA8888;
	case IMAGE_FORMAT_BGR888_BLUESCREEN:
		return BGR888_BLUESCREENToRGBA8888;
	case IMAGE_FORMAT_ARGB8888:
		return ARGB8888ToRGBA8888;
	case IMAGE_FORMAT_BGRA8888:
		return BGRA8888ToRGBA8888;
	case IMAGE_FORMAT_BGRX8888:
		return BGRX8888ToRGBA8888;
	case IMAGE_FORMAT_BGR565:
		return BGR565ToRGBA8888;
	case IMAGE_FORMAT_BGRX5551:
		return BGRX5551ToRGBA8888;
	case IMAGE_FORMAT_BGRA5551:
		return BGRA5551ToRGBA8888;
	case IMAGE_FORMAT_BGRA4444:
		return BGRA4444ToRGBA8888;
	case IMAGE_FORMAT_UV88:
		return UV88ToRGBA8888;
	case IMAGE_FORMAT_UVWQ8888:
		return UVWQ8888ToRGBA8888;
	case IMAGE_FORMAT_UVLX8888:
		return UVLX8888ToRGBA8888;
	case IMAGE_FORMAT_RGBA16161616:
		return RGBA16161616ToRGBA8888;
	case IMAGE_FORMAT_RGBA16161616F:
		return NULL;

#if defined( _X360 )
	case IMAGE_FORMAT_LINEAR_RGBA8888:
		return RGBA8888ToRGBA8888;
	case IMAGE_FORMAT_LINEAR_ABGR8888:
		return ABGR8888ToRGBA8888;
	case IMAGE_FORMAT_LINEAR_RGB888:
		return RGB888ToRGBA8888;
	case IMAGE_FORMAT_LINEAR_BGR888:
		return BGR888ToRGBA8888;
	case IMAGE_FORMAT_LINEAR_I8:
		return I8ToRGBA8888;
	case IMAGE_FORMAT_LINEAR_ARGB8888:
		return ARGB8888ToRGBA8888;
	case IMAGE_FORMAT_LINEAR_BGRA8888:
		return BGRA8888ToRGBA8888;
	case IMAGE_FORMAT_LINEAR_BGRX8888:
		return BGRX8888ToRGBA8888;
	case IMAGE_FORMAT_LINEAR_BGRX5551:
		return BGRX5551ToRGBA8888;
	case IMAGE_FORMAT_LINEAR_RGBA16161616:
		return RGBA16161616ToRGBA8888;
#endif

	default:
		return NULL;
	}
}

static RGBA8888ToUserFormatFunc_t GetRGBA8888ToUserFormatFunc_t( ImageFormat dstImageFormat )
{
	switch( dstImageFormat )
	{
	case IMAGE_FORMAT_RGBA8888:
		return RGBA8888ToRGBA8888;
	case IMAGE_FORMAT_ABGR8888:
		return RGBA8888ToABGR8888;
	case IMAGE_FORMAT_RGB888:
		return RGBA8888ToRGB888;
	case IMAGE_FORMAT_BGR888:
		return RGBA8888ToBGR888;
	case IMAGE_FORMAT_RGB565:
		return NULL;
	case IMAGE_FORMAT_I8:
		return RGBA8888ToI8;
	case IMAGE_FORMAT_IA88:
		return RGBA8888ToIA88;
	case IMAGE_FORMAT_A8:
		return RGBA8888ToA8;
	case IMAGE_FORMAT_RGB888_BLUESCREEN:
		return RGBA8888ToRGB888_BLUESCREEN;
	case IMAGE_FORMAT_BGR888_BLUESCREEN:
		return RGBA8888ToBGR888_BLUESCREEN;
	case IMAGE_FORMAT_ARGB8888:
		return RGBA8888ToARGB8888;
	case IMAGE_FORMAT_BGRA8888:
		return RGBA8888ToBGRA8888;
	case IMAGE_FORMAT_BGRX8888:
		return RGBA8888ToBGRX8888;
	case IMAGE_FORMAT_BGR565:
		return RGBA8888ToBGR565;
	case IMAGE_FORMAT_BGRX5551:
		return RGBA8888ToBGRX5551;
	case IMAGE_FORMAT_BGRA5551:
		return RGBA8888ToBGRA5551;
	case IMAGE_FORMAT_BGRA4444:
		return RGBA8888ToBGRA4444;
	case IMAGE_FORMAT_UV88:
		return RGBA8888ToUV88;
	case IMAGE_FORMAT_UVWQ8888:
		return RGBA8888ToUVWQ8888;
	case IMAGE_FORMAT_UVLX8888:
		return RGBA8888ToUVLX8888;
	case IMAGE_FORMAT_RGBA16161616F:
		return NULL;

#if defined( _X360 )
	case IMAGE_FORMAT_LINEAR_RGBA8888:
		return RGBA8888ToRGBA8888;
	case IMAGE_FORMAT_LINEAR_ABGR8888:
		return RGBA8888ToABGR8888;
	case IMAGE_FORMAT_LINEAR_RGB888:
		return RGBA8888ToRGB888;
	case IMAGE_FORMAT_LINEAR_BGR888:
		return RGBA8888ToBGR888;
	case IMAGE_FORMAT_LINEAR_I8:
		return RGBA8888ToI8;
	case IMAGE_FORMAT_LINEAR_ARGB8888:
		return RGBA8888ToARGB8888;
	case IMAGE_FORMAT_LINEAR_BGRA8888:
		return RGBA8888ToBGRA8888;
	case IMAGE_FORMAT_LINEAR_BGRX8888:
		return RGBA8888ToBGRX8888;
	case IMAGE_FORMAT_LINEAR_BGRX5551:
		return RGBA8888ToBGRX5551;
#endif

	default:
		return NULL;
	}
}



#pragma pack(1)

struct DXTColBlock
{
	WORD col0;
	WORD col1;

	// no bit fields - use bytes
	BYTE row[4];
};

struct DXTAlphaBlock3BitLinear
{
	BYTE alpha0;
	BYTE alpha1;

	BYTE stuff[6];
};

#pragma pack()

static inline void GetColorBlockColorsBGRA8888( DXTColBlock *pBlock, BGRA8888_t *col_0, 
											    BGRA8888_t *col_1, BGRA8888_t *col_2, 
												BGRA8888_t *col_3, WORD & wrd  )
{
	// input data is assumed to be x86 order
	// swap to target platform for proper dxt decoding
	WORD color0 = LittleShort( pBlock->col0 );
	WORD color1 = LittleShort( pBlock->col1 );

	// convert to full precision correctly.
	// If this was a perf problem, we could optimize it. But this isn't used in any hotpaths 
	// (now) so let's just do the correct but slow fp math.
	col_0->a = 0xff;
	col_0->r = ( uint8 ) round( ( ( BGR565_t* ) &color0 )->r * 255.0f / 31.0f );
	col_0->g = ( uint8 ) round( ( ( BGR565_t* ) &color0 )->g * 255.0f / 63.0f );
	col_0->b = ( uint8 ) round( ( ( BGR565_t* ) &color0 )->b * 255.0f / 31.0f );

	col_1->a = 0xff;
	col_1->r = ( uint8 ) round( ( ( BGR565_t* ) &color1 )->r * 255.0f / 31.0f );
	col_1->g = ( uint8 ) round( ( ( BGR565_t* ) &color1 )->g * 255.0f / 63.0f );
	col_1->b = ( uint8 ) round( ( ( BGR565_t* ) &color1 )->b * 255.0f / 31.0f );

	if ( color0 > color1 )
	{
		// Four-color block: derive the other two colors.    
		// 00 = color_0, 01 = color_1, 10 = color_2, 11 = color_3
		// These two bit codes correspond to the 2-bit fields 
		// stored in the 64-bit block.

		wrd = ((WORD)col_0->r * 2 + (WORD)col_1->r )/3;
											// no +1 for rounding
											// as bits have been shifted to 888
		col_2->r = (BYTE)wrd;

		wrd = ((WORD)col_0->g * 2 + (WORD)col_1->g )/3;
		col_2->g = (BYTE)wrd;

		wrd = ((WORD)col_0->b * 2 + (WORD)col_1->b )/3;
		col_2->b = (BYTE)wrd;
		col_2->a = 0xff;

		wrd = ((WORD)col_0->r + (WORD)col_1->r *2 )/3;
		col_3->r = (BYTE)wrd;

		wrd = ((WORD)col_0->g + (WORD)col_1->g *2 )/3;
		col_3->g = (BYTE)wrd;

		wrd = ((WORD)col_0->b + (WORD)col_1->b *2 )/3;
		col_3->b = (BYTE)wrd;
		col_3->a = 0xff;
	}
	else
	{
		// Three-color block: derive the other color.
		// 00 = color_0,  01 = color_1,  10 = color_2,  
		// 11 = transparent.
		// These two bit codes correspond to the 2-bit fields 
		// stored in the 64-bit block. 

		// explicit for each component, unlike some refrasts...????
		
		wrd = ((WORD)col_0->r + (WORD)col_1->r )/2;
		col_2->r = (BYTE)wrd;
		wrd = ((WORD)col_0->g + (WORD)col_1->g )/2;
		col_2->g = (BYTE)wrd;
		wrd = ((WORD)col_0->b + (WORD)col_1->b )/2;
		col_2->b = (BYTE)wrd;
		col_2->a = 0xff;

		col_3->r = 0x00;		// random color to indicate alpha
		col_3->g = 0xff;
		col_3->b = 0xff;
		col_3->a = 0x00;
	}
}			

template <class CDestPixel> 
static inline void DecodeColorBlock( CDestPixel *pOutputImage, DXTColBlock *pColorBlock, int width,
					                 BGRA8888_t *col_0, BGRA8888_t *col_1, 
					                 BGRA8888_t *col_2, BGRA8888_t *col_3 )
{
	// width is width of image in pixels
	DWORD bits;
	int r,n;

	// bit masks = 00000011, 00001100, 00110000, 11000000
	const DWORD masks[] = { 3 << 0, 3 << 2, 3 << 4, 3 << 6 };
	const int   shift[] = { 0, 2, 4, 6 };

	// r steps through lines in y
	for ( r=0; r < 4; r++, pOutputImage += width-4 )	// no width*4 as DWORD ptr inc will *4
	{
		// width * 4 bytes per pixel per line
		// each j dxtc row is 4 lines of pixels

		// n steps through pixels
		for ( n=0; n < 4; n++ )
		{
			bits = pColorBlock->row[r] & masks[n];
			bits >>= shift[n];

			switch( bits )
			{
			case 0:
				*pOutputImage = *col_0;
				pOutputImage++;		// increment to next output pixel
				break;
			case 1:
				*pOutputImage = *col_1;
				pOutputImage++;
				break;
			case 2:
				*pOutputImage = *col_2;
				pOutputImage++;
				break;
			case 3:
				*pOutputImage = *col_3;
				pOutputImage++;
				break;
			default:
				Assert( 0 );
				pOutputImage++;
				break;
			}
		}
	}
}

template <class CDestPixel> 
static inline void DecodeAlpha3BitLinear( CDestPixel *pImPos, DXTAlphaBlock3BitLinear *pAlphaBlock, int width, int nChannelSelect = 3 )
{
	static BYTE		gBits[4][4];
	static WORD		gAlphas[8];
	static BGRA8888_t	gACol[4][4];

	gAlphas[0] = pAlphaBlock->alpha0;
	gAlphas[1] = pAlphaBlock->alpha1;

	// 8-alpha or 6-alpha block?    

	if( gAlphas[0] > gAlphas[1] )
	{
		// 8-alpha block:  derive the other 6 alphas.    
		// 000 = alpha_0, 001 = alpha_1, others are interpolated

		gAlphas[2] = ( 6 * gAlphas[0] +     gAlphas[1]) / 7;	// bit code 010
		gAlphas[3] = ( 5 * gAlphas[0] + 2 * gAlphas[1]) / 7;	// Bit code 011    
		gAlphas[4] = ( 4 * gAlphas[0] + 3 * gAlphas[1]) / 7;	// Bit code 100    
		gAlphas[5] = ( 3 * gAlphas[0] + 4 * gAlphas[1]) / 7;	// Bit code 101
		gAlphas[6] = ( 2 * gAlphas[0] + 5 * gAlphas[1]) / 7;	// Bit code 110    
		gAlphas[7] = (     gAlphas[0] + 6 * gAlphas[1]) / 7;	// Bit code 111
	}    
	else
	{
		// 6-alpha block:  derive the other alphas.    
		// 000 = alpha_0, 001 = alpha_1, others are interpolated

		gAlphas[2] = (4 * gAlphas[0] +     gAlphas[1]) / 5;	// Bit code 010
		gAlphas[3] = (3 * gAlphas[0] + 2 * gAlphas[1]) / 5;	// Bit code 011    
		gAlphas[4] = (2 * gAlphas[0] + 3 * gAlphas[1]) / 5;	// Bit code 100    
		gAlphas[5] = (    gAlphas[0] + 4 * gAlphas[1]) / 5;	// Bit code 101
		gAlphas[6] = 0;										// Bit code 110
		gAlphas[7] = 255;									// Bit code 111
	}

	// Decode 3-bit fields into array of 16 BYTES with same value

	// first two rows of 4 pixels each:
	// pRows = (Alpha3BitRows*) & ( pAlphaBlock->stuff[0] );
	const DWORD mask = 0x00000007;		// bits = 00 00 01 11

	DWORD bits = *( (DWORD*) & ( pAlphaBlock->stuff[0] ));

	gBits[0][0] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[0][1] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[0][2] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[0][3] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[1][0] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[1][1] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[1][2] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[1][3] = (BYTE)( bits & mask );

	// now for last two rows:
	bits = *( (DWORD*) & ( pAlphaBlock->stuff[3] ));		// last 3 bytes

	gBits[2][0] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[2][1] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[2][2] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[2][3] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[3][0] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[3][1] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[3][2] = (BYTE)( bits & mask );
	bits >>= 3;
	gBits[3][3] = (BYTE)( bits & mask );

	// decode the codes into alpha values
	int row, pix;
	for ( row = 0; row < 4; row++ )
	{
		for ( pix=0; pix < 4; pix++ )
		{
			gACol[row][pix].a = (BYTE) gAlphas[ gBits[row][pix] ];

			Assert( gACol[row][pix].r == 0 );
			Assert( gACol[row][pix].g == 0 );
			Assert( gACol[row][pix].b == 0 );
		}
	}

	// Write out alpha values to the image bits
	for ( row=0; row < 4; row++, pImPos += width-4 )
	{
		for ( pix = 0; pix < 4; pix++ )
		{
			// zero the alpha bits of image pixel
			switch ( nChannelSelect )
			{
				case 0:
					pImPos->r = ( *(( BGRA8888_t *) &(gACol[row][pix])) ).a;
					pImPos->g = 0;	// Danger...stepping on the other color channels
					pImPos->b = 0;
					pImPos->a = 0;
					break;
				case 1:
					pImPos->g = ( *(( BGRA8888_t *) &(gACol[row][pix])) ).a;
					break;
				case 2:
					pImPos->b = ( *(( BGRA8888_t *) &(gACol[row][pix])) ).a;
					break;
				default:
				case 3:
					pImPos->a = ( *(( BGRA8888_t *) &(gACol[row][pix])) ).a;
					break;
			}

			pImPos++;
		}
	}
}

template <class CDestPixel> 
static void ConvertFromDXT1( const uint8 *src, CDestPixel *dst, int width, int height )
{
	Assert( sizeof( BGRA8888_t ) == 4 );
	Assert( sizeof( RGBA8888_t ) == 4 );
	Assert( sizeof( RGB888_t ) == 3 );
	Assert( sizeof( BGR888_t ) == 3 );
	Assert( sizeof( BGR565_t ) == 2 );
	Assert( sizeof( BGRA5551_t ) == 2 );
	Assert( sizeof( BGRA4444_t ) == 2 );

	int realWidth = 0;
	int realHeight = 0;
	CDestPixel *realDst = NULL;

	// Deal with the case where we have a dimension smaller than 4.
	if ( width < 4 || height < 4 )
	{
		realWidth = width;
		realHeight = height;
		// round up to the nearest four
		width = ( width + 3 ) & ~3;
		height = ( height + 3 ) & ~3;
		realDst = dst;
		dst = ( CDestPixel * )_alloca( width * height * sizeof( CDestPixel ) );
		Assert( dst );
	}
	Assert( !( width % 4 ) );
	Assert( !( height % 4 ) );

	int xblocks, yblocks;
	xblocks = width >> 2;
	yblocks = height >> 2;
	CDestPixel *pDstScan = dst;
	DWORD *pSrcScan = ( DWORD * )src;

	DXTColBlock *pBlock;
	BGRA8888_t col_0, col_1, col_2, col_3;
	WORD wrdDummy;

	int i, j;
	for ( j = 0; j < yblocks; j++ )
	{
		// 8 bytes per block
		pBlock = ( DXTColBlock * )( ( uint8 * )pSrcScan + j * xblocks * 8 );
		for ( i=0; i < xblocks; i++, pBlock++ )
		{
			GetColorBlockColorsBGRA8888( pBlock, &col_0, &col_1, &col_2, &col_3, wrdDummy );

			// now decode the color block into the bitmap bits
			// inline func:
			pDstScan = dst + i*4 + j*4*width;
			DecodeColorBlock<CDestPixel>( pDstScan, pBlock, width, &col_0, &col_1, &col_2, &col_3 );
		}
	}

	// Deal with the case where we have a dimension smaller than 4.
	if ( realDst )
	{
		int x, y;
		for ( y = 0; y < realHeight; y++ )
		{
			for ( x = 0; x < realWidth; x++ )
			{
				realDst[x+(y*realWidth)] = dst[x+(y*width)];
			}
		}
	}
}

template <class CDestPixel> 
static void ConvertFromDXT5( const uint8 *src, CDestPixel *dst, int width, int height )
{
	int realWidth = 0;
	int realHeight = 0;
	CDestPixel *realDst = NULL;

	// Deal with the case where we have a dimension smaller than 4.
	if ( width < 4 || height < 4 )
	{
		realWidth = width;
		realHeight = height;
		// round up to the nearest four
		width = ( width + 3 ) & ~3;
		height = ( height + 3 ) & ~3;
		realDst = dst;
		dst = ( CDestPixel * )_alloca( width * height * sizeof( CDestPixel ) );
		Assert( dst );
	}
	Assert( !( width % 4 ) );
	Assert( !( height % 4 ) );

	int xblocks, yblocks;
	xblocks = width >> 2;
	yblocks = height >> 2;
	
	CDestPixel *pDstScan = dst;
	DWORD *pSrcScan = ( DWORD * )src;

	DXTColBlock				*pBlock;
	DXTAlphaBlock3BitLinear *pAlphaBlock;

	BGRA8888_t col_0, col_1, col_2, col_3;
	WORD wrd;

	int i,j;
	for ( j=0; j < yblocks; j++ )
	{
		// 8 bytes per block
		// 1 block for alpha, 1 block for color
		pBlock = (DXTColBlock*) ( (uint8 *)pSrcScan + j * xblocks * 16 );

		for ( i=0; i < xblocks; i++, pBlock ++ )
		{
			// inline
			// Get alpha block
			pAlphaBlock = (DXTAlphaBlock3BitLinear*) pBlock;

			// inline func:
			// Get color block & colors
			pBlock++;

			GetColorBlockColorsBGRA8888( pBlock, &col_0, &col_1, &col_2, &col_3, wrd );

			pDstScan = dst + i*4 + j*4*width;

			// Decode the color block into the bitmap bits
			// inline func:
			DecodeColorBlock<CDestPixel>( pDstScan, pBlock, width, &col_0, &col_1, &col_2, &col_3 );

			// Overwrite the previous alpha bits with the alpha block
			//  info
			DecodeAlpha3BitLinear( pDstScan, pAlphaBlock, width );
		}
	}

	// Deal with the case where we have a dimension smaller than 4.
	if ( realDst )
	{
		int x, y;
		for( y = 0; y < realHeight; y++ )
		{
			for( x = 0; x < realWidth; x++ )
			{
				realDst[x+(y*realWidth)] = dst[x+(y*width)];
			}
		}
	}
}

template <class CDestPixel> 
static void ConvertFromDXT5IgnoreAlpha( const uint8 *src, CDestPixel *dst, int width, int height )
{
	int realWidth = 0;
	int realHeight = 0;
	CDestPixel *realDst = NULL;

	// Deal with the case where we have a dimension smaller than 4.
	if ( width < 4 || height < 4 )
	{
		realWidth = width;
		realHeight = height;
		// round up to the nearest four
		width = ( width + 3 ) & ~3;
		height = ( height + 3 ) & ~3;
		realDst = dst;
		dst = ( CDestPixel * )_alloca( width * height * sizeof( CDestPixel ) );
		Assert( dst );
	}
	Assert( !( width % 4 ) );
	Assert( !( height % 4 ) );

	int xblocks, yblocks;
	xblocks = width >> 2;
	yblocks = height >> 2;
	
	CDestPixel *pDstScan = dst;
	DWORD *pSrcScan = ( DWORD * )src;

	DXTColBlock *pBlock;

	BGRA8888_t col_0, col_1, col_2, col_3;
	WORD wrd;

	int i,j;
	for ( j=0; j < yblocks; j++ )
	{
		// 8 bytes per block
		// 1 block for alpha, 1 block for color
		pBlock = (DXTColBlock*) ( (uint8 *)pSrcScan + j * xblocks * 16 );

		for( i=0; i < xblocks; i++, pBlock ++ )
		{
			// inline func:
			// Get color block & colors
			pBlock++;

			GetColorBlockColorsBGRA8888( pBlock, &col_0, &col_1, &col_2, &col_3, wrd );

			pDstScan = dst + i*4 + j*4*width;

			// Decode the color block into the bitmap bits
			// inline func:
			DecodeColorBlock<CDestPixel>( pDstScan, pBlock, width, &col_0, &col_1, &col_2, &col_3 );
		}
	}

	// Deal with the case where we have a dimension smaller than 4.
	if( realDst )
	{
		int x, y;
		for( y = 0; y < realHeight; y++ )
		{
			for( x = 0; x < realWidth; x++ )
			{
				realDst[x+(y*realWidth)] = dst[x+(y*width)];
			}
		}
	}
}


template <class CDestPixel> 
static void ConvertFromATIxN( const uint8 *src, CDestPixel *dst, int width, int height, bool bATI2N )
{
	int realWidth = 0;
	int realHeight = 0;
	CDestPixel *realDst = NULL;

	// Deal with the case where we have a dimension smaller than 4.
	if ( width < 4 || height < 4 )
	{
		realWidth = width;
		realHeight = height;
		// round up to the nearest four
		width = ( width + 3 ) & ~3;
		height = ( height + 3 ) & ~3;
		realDst = dst;
		dst = ( CDestPixel * )_alloca( width * height * sizeof( CDestPixel ) );
		Assert( dst );
	}
	Assert( !( width % 4 ) );
	Assert( !( height % 4 ) );

	int xblocks, yblocks;
	xblocks = width >> 2;
	yblocks = height >> 2;

	CDestPixel *pDstScan = dst;
	DWORD *pSrcScan = ( DWORD * )src;

	DXTAlphaBlock3BitLinear	*pBlock;

	int nBytesPerBlock = bATI2N ? 16 : 8;

	int i,j;
	for ( j=0; j < yblocks; j++ )
	{
		// 8 bytes per block
		// 1 block for x, 1 block for y
		pBlock = (DXTAlphaBlock3BitLinear*) ( (uint8 *)pSrcScan + j * xblocks * nBytesPerBlock );

		for ( i=0; i < xblocks; i++, pBlock++ )
		{
			pDstScan = dst + i*4 + j*4*width;
			DecodeAlpha3BitLinear( pDstScan, pBlock, width, 0 );

			if ( bATI2N )
			{
				pBlock++;
				DecodeAlpha3BitLinear( pDstScan, pBlock, width, 1 );
			}
		}
	}

	// Deal with the case where we have a dimension smaller than 4.
	if ( realDst )
	{
		int x, y;
		for( y = 0; y < realHeight; y++ )
		{
			for( x = 0; x < realWidth; x++ )
			{
				realDst[x+(y*realWidth)] = dst[x+(y*width)];
			}
		}
	}
}

static DWORD GetDXTCEncodeType( ImageFormat imageFormat )
{
	switch ( imageFormat )
	{
	case IMAGE_FORMAT_DXT1:
		return S3TC_ENCODE_RGB_FULL;
	case IMAGE_FORMAT_DXT1_ONEBITALPHA:
		return S3TC_ENCODE_RGB_FULL | S3TC_ENCODE_RGB_ALPHA_COMPARE;
	case IMAGE_FORMAT_DXT3:
		return S3TC_ENCODE_RGB_FULL | S3TC_ENCODE_ALPHA_EXPLICIT;
	case IMAGE_FORMAT_DXT5:
		return S3TC_ENCODE_RGB_FULL | S3TC_ENCODE_ALPHA_INTERPOLATED;
	default:
		return 0;
	}
}

// Convert RGBA input to ATI1N or ATI2N format
bool ConvertToATIxN(  const uint8 *src, ImageFormat srcImageFormat,
					  uint8 *dst, ImageFormat dstImageFormat,
					  int width, int height, int srcStride, int dstStride )
{
#if !defined( _X360 ) && !defined( POSIX )

	// from rgb(a) to ATIxN
	if( srcStride != 0 || dstStride != 0 )
		return false;

	// If we're not the right format for the ATI compressor, we bail
	if ( srcImageFormat != IMAGE_FORMAT_ARGB8888 )
		return false;

	// Define source image parameters and copy the bits into buffer
	ATI_TC_Texture srcTexture;
	srcTexture.dwSize = sizeof( srcTexture );
	srcTexture.dwWidth = width;
	srcTexture.dwHeight = height;
	srcTexture.dwPitch = srcStride;
	srcTexture.format = ATI_TC_FORMAT_ARGB_8888;
	srcTexture.dwDataSize = ATI_TC_CalculateBufferSize( &srcTexture );
	srcTexture.pData = (ATI_TC_BYTE*) malloc( srcTexture.dwDataSize );
	memcpy( srcTexture.pData, src, srcTexture.dwDataSize );

	ATI_TC_Texture destTexture;
	destTexture.dwSize = sizeof( destTexture );
	destTexture.dwWidth = width;
	destTexture.dwHeight = height;
	destTexture.dwPitch = 0;
	destTexture.format = dstImageFormat == IMAGE_FORMAT_ATI2N ? ATI_TC_FORMAT_ATI2N : ATI_TC_FORMAT_ATI1N;	// Assume it can only be one of these two...
	destTexture.dwDataSize = ATI_TC_CalculateBufferSize( &destTexture );
	destTexture.pData = (ATI_TC_BYTE*) dst;

	ATI_TC_ERROR errATI = ATI_TC_ConvertTexture( &srcTexture, &destTexture, NULL, NULL, NULL, NULL );		// Convert it!
	
	free( srcTexture.pData );																				// Free temporary buffers

	if ( errATI != ATI_TC_OK )
		return false;

	return true;
#else
	Assert( 0 );
	return false;
#endif
}


bool ConvertToDXTLegacy(  const uint8 *src, ImageFormat srcImageFormat,
 						  uint8 *dst, ImageFormat dstImageFormat, 
					      int width, int height, int srcStride, int dstStride )
{
#if !defined( _X360 ) && !defined( POSIX )
	// from rgb(a) to dxtN
	if( srcStride != 0 || dstStride != 0 )
		return false;

	DDSURFACEDESC descIn;
	DDSURFACEDESC descOut;
	memset( &descIn, 0, sizeof(descIn) );
	memset( &descOut, 0, sizeof(descOut) );
	float weight[3] = {0.3086f, 0.6094f, 0.0820f};
	DWORD dwEncodeType = GetDXTCEncodeType( dstImageFormat );
	
	// Setup descIn
	descIn.dwSize = sizeof(descIn);
	descIn.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_LPSURFACE | 
		/*DDSD_PITCH | */ DDSD_PIXELFORMAT;
	descIn.dwWidth = width;
	descIn.dwHeight = height;
	descIn.lPitch = width * ImageLoader::SizeInBytes( srcImageFormat );
	descIn.lpSurface = ( LPVOID *) src;
	descIn.ddpfPixelFormat.dwSize = sizeof( DDPIXELFORMAT );
	switch ( srcImageFormat )
	{
	case IMAGE_FORMAT_RGBA8888:
		descIn.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
		descIn.ddpfPixelFormat.dwRGBBitCount = 32;
		descIn.ddpfPixelFormat.dwRBitMask = 0x0000ff;
		descIn.ddpfPixelFormat.dwGBitMask = 0x00ff00;
		descIn.ddpfPixelFormat.dwBBitMask = 0xff0000;
		// must set this anyway or S3TC will lock up!!!
		descIn.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;
		break;
	case IMAGE_FORMAT_BGRA8888:
		descIn.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
		descIn.ddpfPixelFormat.dwRGBBitCount = 32;
		descIn.ddpfPixelFormat.dwRBitMask = 0xFF0000;
		descIn.ddpfPixelFormat.dwGBitMask = 0x00ff00;
		descIn.ddpfPixelFormat.dwBBitMask = 0x0000FF;
		// must set this anyway or S3TC will lock up!!!
		descIn.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;
		break;
	case IMAGE_FORMAT_BGRX8888:
		descIn.ddpfPixelFormat.dwFlags = DDPF_RGB;
		descIn.ddpfPixelFormat.dwRGBBitCount = 32;
		descIn.ddpfPixelFormat.dwRBitMask = 0xFF0000;
		descIn.ddpfPixelFormat.dwGBitMask = 0x00ff00;
		descIn.ddpfPixelFormat.dwBBitMask = 0x0000FF;
		// must set this anyway or S3TC will lock up!!!
		descIn.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;
		break;
	case IMAGE_FORMAT_RGB888:
		descIn.ddpfPixelFormat.dwFlags = DDPF_RGB;
		descIn.ddpfPixelFormat.dwRGBBitCount = 24;
		descIn.ddpfPixelFormat.dwRBitMask = 0x0000ff;
		descIn.ddpfPixelFormat.dwGBitMask = 0x00ff00;
		descIn.ddpfPixelFormat.dwBBitMask = 0xff0000;
		descIn.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;
		break;
	default:
		return false;
	}
	
	// Setup descOut
	descOut.dwSize = sizeof( descOut );
	
	// Encode the texture
	S3TCencode( &descIn, NULL, &descOut, dst, dwEncodeType, weight );
	return true;
#else
	Assert( 0 );
	return false;
#endif
}

template < typename SrcPixel_t >
void CompressSTB( uint8 *pDstBytes, ImageFormat dstFmt, const uint8 *pSrcBytes, int nWidth, int nHeight )
{
	const bool cbWriteAlpha = ( dstFmt == IMAGE_FORMAT_DXT5 );
	const uint32 cDstStride = ( dstFmt == IMAGE_FORMAT_DXT1 ) ? 8 : 16;

	const uint32 cPixX = (uint32) nWidth;
	const uint32 cPixY = (uint32) nHeight;
	const uint32 cSrcPitch = cPixX * sizeof( SrcPixel_t );
	const uint32 cLastX = cPixX - 1;
	const uint32 cLastY = cPixY - 1;

	// STB always takes blocks as 4x4 of RGBA8888_t
	RGBA8888_t srcBlock[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	SrcPixel_t* pSrcs[4] = { 0, 0, 0, 0 };

	for ( uint32 y = 0; y < cPixY; y += 4 ) 
	{
		// This handles clamping for cPixY % 4 != 0
		pSrcs[ 0 ] = ( SrcPixel_t* ) ( pSrcBytes + cSrcPitch * Min( y + 0, cLastY ) );
		pSrcs[ 1 ] = ( SrcPixel_t* ) ( pSrcBytes + cSrcPitch * Min( y + 1, cLastY ) );
		pSrcs[ 2 ] = ( SrcPixel_t* ) ( pSrcBytes + cSrcPitch * Min( y + 2, cLastY ) );
		pSrcs[ 3 ] = ( SrcPixel_t* ) ( pSrcBytes + cSrcPitch * Min( y + 3, cLastY ) );

		for ( uint x = 0; x < cPixX; x += 4 ) 
		{
			for ( uint i = 0; i < 4; ++i )
			{
				uint32 offsetX = Min( x + i, cLastX );
				srcBlock[ 0 + i ] =  pSrcs[ 0 ][ offsetX ];
				srcBlock[ 4 + i ] =  pSrcs[ 1 ][ offsetX ];
				srcBlock[ 8 + i ] =  pSrcs[ 2 ][ offsetX ];
				srcBlock[ 12 + i ] = pSrcs[ 3 ][ offsetX ];
			}

			stb_compress_dxt_block( pDstBytes, ( const uint8* ) srcBlock, cbWriteAlpha, STB_DXT_NORMAL );
			pDstBytes += cDstStride;
		}
	}
}

inline ImageFormat GetTrueImageFormat( ImageFormat fmt )
{
	switch ( fmt )
	{
	case IMAGE_FORMAT_DXT1_RUNTIME:
		return IMAGE_FORMAT_DXT1;
	case IMAGE_FORMAT_DXT5_RUNTIME:
		return IMAGE_FORMAT_DXT5;
	default: /* expected */
		break;
	}

	return fmt;
}

bool ConvertToDXTRuntime( const uint8 *src, ImageFormat srcImageFormat,
						  uint8 *dst, ImageFormat dstImageFormat,
						  int width, int height, int srcStride, int dstStride )
{
	if ( srcStride != 0 || dstStride != 0 )
		return false;

	dstImageFormat = GetTrueImageFormat( dstImageFormat );

	switch ( srcImageFormat )
	{
		case IMAGE_FORMAT_RGBA8888: CompressSTB<RGBA8888_t>( dst, dstImageFormat, src, width, height ); return true;
		case IMAGE_FORMAT_RGB888:   CompressSTB<RGB888_t>  ( dst, dstImageFormat, src, width, height ); return true;
		case IMAGE_FORMAT_BGRA8888: CompressSTB<BGRA8888_t>( dst, dstImageFormat, src, width, height ); return true;
		case IMAGE_FORMAT_BGRX8888: CompressSTB<BGRX8888_t>( dst, dstImageFormat, src, width, height ); return true;
		default:
			Assert( !"Unexpected format here, wtf." );
			break;
	};

	return false;
}

bool ConvertToDXT( const uint8 *src, ImageFormat srcImageFormat,
 				   uint8 *dst, ImageFormat dstImageFormat, 
				   int width, int height, int srcStride, int dstStride )
{
	// The STB compressor (the new compressor) is faster and higher quality in most cases, and has less error overall
	// than the S3TC compressor. So use it by default, unless we're working with a format that STB doesn't support.
	bool bUseNewCompressor = dstImageFormat != IMAGE_FORMAT_DXT1_ONEBITALPHA 
		                  && dstImageFormat != IMAGE_FORMAT_DXT3;

//	bool bUseNewCompressor = dstImageFormat == IMAGE_FORMAT_DXT1_RUNTIME 
//		                  || dstImageFormat == IMAGE_FORMAT_DXT5_RUNTIME;

	if ( bUseNewCompressor )
		return ConvertToDXTRuntime( src, srcImageFormat, dst, dstImageFormat, width, height, srcStride, dstStride );

	return ConvertToDXTLegacy( src, srcImageFormat, dst, dstImageFormat, width, height, srcStride, dstStride );
}

// HDRFIXME: This assumes that the 16-bit integer values are 4.12 fixed-point.
void ConvertImageFormat_RGBA16161616_To_RGB323232F( unsigned short *pSrcImage, float *pDstImage, int width, int height )
{
	int srcSize = width * height * 4;
	unsigned short *pSrcEnd = pSrcImage + srcSize;
	unsigned short *pSrcScan = pSrcImage;
	float *pDstScan = pDstImage;
	for ( ; pSrcScan < pSrcEnd; pSrcScan += 4, pDstScan += 3 )
	{
		pDstScan[0] = ( ( float )pSrcScan[0] ) * ( 1.0f / ( ( float )( 1 << 12 ) ) );
		pDstScan[1] = ( ( float )pSrcScan[1] ) * ( 1.0f / ( ( float )( 1 << 12 ) ) );
		pDstScan[2] = ( ( float )pSrcScan[2] ) * ( 1.0f / ( ( float )( 1 << 12 ) ) );
	}
}

// HDRFIXME: This assumes that the 16-bit integer values are 4.12 fixed-point.
void ConvertImageFormat_RGB323232F_To_RGBA16161616( float *pSrcImage, unsigned short *pDstImage, int width, int height )
{
	int srcSize = width * height * 3;
	float *pSrcEnd = pSrcImage + srcSize;
	float *pSrcScan = pSrcImage;
	unsigned short *pDstScan = pDstImage;
	for ( ; pSrcScan < pSrcEnd; pSrcScan += 3, pDstScan += 4 )
	{

		pDstScan[0] = ( unsigned short )min( 65535.0f, ( pSrcScan[0] * ( ( ( float )( 1 << 12 ) ) ) ) );
		pDstScan[1] = ( unsigned short )min( 65535.0f, ( pSrcScan[1] * ( ( ( float )( 1 << 12 ) ) ) ) );
		pDstScan[2] = ( unsigned short )min( 65535.0f, ( pSrcScan[2] * ( ( ( float )( 1 << 12 ) ) ) ) );
		pDstScan[3] = 65535;
	}
}

void ConvertImageFormat_RGBA16161616F_To_RGB323232F( float16 *pSrcImage, float *pDstImage, int width, int height )
{
	int srcSize = width * height * 4;
	float16 *pSrcEnd = pSrcImage + srcSize;
	float16 *pSrcScan = pSrcImage;
	float *pDstScan = pDstImage;
	for( ; pSrcScan < pSrcEnd; pSrcScan += 4, pDstScan += 3 )
	{
		pDstScan[0] = pSrcScan[0].GetFloat();
		pDstScan[1] = pSrcScan[1].GetFloat();
		pDstScan[2] = pSrcScan[2].GetFloat();
	}
}

void ConvertImageFormat_RGBA16161616F_To_RGBA323232F( float16 *pSrcImage, float *pDstImage, int width, int height , size_t src_stride)
{
	size_t s_stride=src_stride/2;
	for(int y=0; y<height; y++)
	{
		float16 const *pSrcScan=pSrcImage;
		float *pDstScan = pDstImage;
		for(int x=0; x<width; x++)
		{
			pDstScan[0] = pSrcScan[0].GetFloat();
			pDstScan[1] = pSrcScan[1].GetFloat();
			pDstScan[2] = pSrcScan[2].GetFloat();
			pDstScan[3] = pSrcScan[3].GetFloat();
			pDstScan+=4;
			pSrcScan+=4;
		}
		pSrcImage += s_stride;
		pDstImage+=4*width;
	}
}


void ConvertImageFormat_RGB323232F_To_RGBA16161616F( float *pSrcImage, float16 *pDstImage, int width, int height )
{
	int srcSize = width * height * 3;
	float *pSrcEnd = pSrcImage + srcSize;
	float *pSrcScan = pSrcImage;
	float16 *pDstScan = pDstImage;
	for( ; pSrcScan < pSrcEnd; pSrcScan += 3, pDstScan += 4 )
	{
		pDstScan[0].SetFloat( pSrcScan[0] );
		pDstScan[1].SetFloat( pSrcScan[1] );
		pDstScan[2].SetFloat( pSrcScan[2] );
	}
}

void ConvertImageFormat_RGB323232F_To_RGBA8888( float *pSrcImage, uint8 *dst, int width, int height )
{
	FloatBitMap_t flbm;
	flbm.AllocateRGB( width, height );

	// Set the pixels
	for ( int y = 0; y < height; ++ y )
	{
		for ( int x = 0; x < width; ++ x )
		{
			float *pf = &pSrcImage[ 3 * ( x + width * y ) ];
			PixRGBAF fpix;
			fpix.Red = pf[0];
			fpix.Green = pf[1];
			fpix.Blue = pf[2];
			fpix.Alpha = 0.f;
			
			flbm.WritePixelRGBAF( x, y, fpix );
		}
	}
	// memcpy( flbm.RGBAData, pSrcImage, width * height * 4 );
	
	flbm.CompressTo8Bits( 8.0 );

	// Now, get the pixels
	for ( int y = 0; y < height; ++ y )
	{
		for ( int x = 0; x < width; ++ x )
		{
			PixRGBAF fpix = flbm.PixelRGBAF( x, y );
			PixRGBA8 pix8 = PixRGBAF_to_8( fpix );

			uint8 *pch = &dst[ 4 * ( x + width * y ) ];

			pch[0] = pix8.Red;
			pch[1] = pix8.Green;
			pch[2] = pix8.Blue;
			pch[3] = pix8.Alpha;
		}
	}
}

void ConvertImageFormat_RGB323232F_To_BGRA8888( float *pSrcImage, uint8 *dst, int width, int height )
{
	FloatBitMap_t flbm;
	flbm.AllocateRGB( width, height );

	// Set the pixels
	for ( int y = 0; y < height; ++ y )
	{
		for ( int x = 0; x < width; ++ x )
		{
			float *pf = &pSrcImage[ 3 * ( x + width * y ) ];
			PixRGBAF fpix;
			fpix.Red = pf[0];
			fpix.Green = pf[1];
			fpix.Blue = pf[2];
			fpix.Alpha = 0.f;

			flbm.WritePixelRGBAF( x, y, fpix );
		}
	}
	// memcpy( flbm.RGBAData, pSrcImage, width * height * 4 );

	flbm.CompressTo8Bits( 8.0 );

	// Now, get the pixels
	for ( int y = 0; y < height; ++ y )
	{
		for ( int x = 0; x < width; ++ x )
		{
			PixRGBAF fpix = flbm.PixelRGBAF( x, y );
			PixRGBA8 pix8 = PixRGBAF_to_8( fpix );

			uint8 *pch = &dst[ 4 * ( x + width * y ) ];

			pch[0] = pix8.Blue;
			pch[1] = pix8.Green;
			pch[2] = pix8.Red;
			pch[3] = pix8.Alpha;
		}
	}
}

// HDRFIXME: This assumes that the 16-bit integer values are 4.12 fixed-point.
void ConvertImageFormat_RGBA16161616_To_RGBA16161616F( unsigned short *pSrcImage, float *pDstImage, int width, int height )
{
	int srcSize = width * height * 4;
	unsigned short *pSrcEnd = pSrcImage + srcSize;
	unsigned short *pSrcScan = pSrcImage;
	float16 *pDstScan = ( float16 * )pDstImage;
	for( ; pSrcScan < pSrcEnd; pSrcScan += 4, pDstScan += 4 )
	{
		pDstScan[0].SetFloat( pSrcScan[0] * ( 1.0f / ( float )( 1 << 16 ) ) );
		pDstScan[1].SetFloat( pSrcScan[1] * ( 1.0f / ( float )( 1 << 16 ) ) );
		pDstScan[2].SetFloat( pSrcScan[2] * ( 1.0f / ( float )( 1 << 16 ) ) );
		pDstScan[3].SetFloat( pSrcScan[3] * ( 1.0f / ( float )( 1 << 16 ) ) );
	}
}

void ConvertImageFormat_RGBA16161616F_To_RGBA16161616( float16 *pSrcImage, unsigned short *pDstImage, int width, int height )
{
	int srcSize = width * height * 4;
	float16 *pSrcEnd = pSrcImage + srcSize;
	float16 *pSrcScan = pSrcImage;
	unsigned short *pDstScan = pDstImage;
	for( ; pSrcScan < pSrcEnd; pSrcScan += 4, pDstScan += 4 )
	{
		int i;
		for( i = 0; i < 4; i++ )
		{
			float val;
			val = pSrcScan[i].GetFloat();
			val *= ( float )( 1 << 12 );
			val = max( val, 0.f );
			val = min( val, 65535.0f );
			pDstScan[i] = ( unsigned short )val;
		}
	}
}

bool ConvertImageFormat( const uint8 *src, ImageFormat srcImageFormat,
 					     uint8 *dst, ImageFormat dstImageFormat, 
						 int width, int height, int srcStride, int dstStride )
{
	// HDRFIXME: WE NEED A BIGGER INTERMEDIATE FORMAT!!!!!
	if ( srcImageFormat == IMAGE_FORMAT_RGBA16161616 )
	{
		if ( dstImageFormat == IMAGE_FORMAT_RGB323232F )
		{
			Assert( srcStride == 0 && dstStride == 0 );
			ConvertImageFormat_RGBA16161616_To_RGB323232F( ( unsigned short * )src, ( float * )dst, width, height );
			return true;
		}
		if ( dstImageFormat == IMAGE_FORMAT_RGBA16161616F )
		{
			Assert( srcStride == 0 && dstStride == 0 );
			ConvertImageFormat_RGBA16161616_To_RGBA16161616F( ( unsigned short * )src, ( float * )dst, width, height );
			return true;
		}
	}
	else if ( srcImageFormat == IMAGE_FORMAT_RGBA16161616F )
	{	
		if ( dstImageFormat == IMAGE_FORMAT_RGB323232F )
		{
			Assert( srcStride == 0 && dstStride == 0 );
			ConvertImageFormat_RGBA16161616F_To_RGB323232F( ( float16 * )src, ( float * )dst, width, height );
			return true;
		}
		if ( dstImageFormat == IMAGE_FORMAT_RGBA32323232F )
		{
			Assert( dstStride == 0 );
			ConvertImageFormat_RGBA16161616F_To_RGBA323232F( ( float16 * )src, ( float * )dst, width, height, srcStride );
			return true;
		}
		if ( dstImageFormat == IMAGE_FORMAT_RGBA16161616 )
		{
			Assert( srcStride == 0 && dstStride == 0 );
			ConvertImageFormat_RGBA16161616F_To_RGBA16161616( ( float16 * )src, ( unsigned short * )dst, width, height );
			return true;
		}
	}
	else if ( srcImageFormat == IMAGE_FORMAT_RGB323232F )
	{
		if ( dstImageFormat == IMAGE_FORMAT_RGBA16161616 )
		{
			Assert( srcStride == 0 && dstStride == 0 );
			ConvertImageFormat_RGB323232F_To_RGBA16161616( ( float * )src, ( unsigned short * )dst, width, height );
			return true;
		}
		if ( dstImageFormat == IMAGE_FORMAT_RGBA16161616F )
		{
			Assert( srcStride == 0 && dstStride == 0 );
			ConvertImageFormat_RGB323232F_To_RGBA16161616F( ( float * )src, ( float16 * )dst, width, height );
			return true;
		}
		if ( dstImageFormat == IMAGE_FORMAT_RGBA8888 )
		{
			Assert( srcStride == 0 );
			ConvertImageFormat_RGB323232F_To_RGBA8888( ( float * )src, dst, width, height );
			return true;
		}
		if ( dstImageFormat == IMAGE_FORMAT_BGRA8888 )
		{
			Assert( srcStride == 0 );
			ConvertImageFormat_RGB323232F_To_BGRA8888( ( float * )src, dst, width, height );
			return true;
		}
	}
	
	// Fast path for just copying a compressed texture
	if ( ( ( dstImageFormat == IMAGE_FORMAT_DXT1 || dstImageFormat == IMAGE_FORMAT_DXT1_RUNTIME ||
		     dstImageFormat == IMAGE_FORMAT_DXT3 ||
		     dstImageFormat == IMAGE_FORMAT_DXT5 || dstImageFormat == IMAGE_FORMAT_DXT5_RUNTIME ||
		     dstImageFormat == IMAGE_FORMAT_ATI1N ||
		     dstImageFormat == IMAGE_FORMAT_ATI2N ) && ( srcImageFormat == dstImageFormat ) ) ||
		 ( dstImageFormat == IMAGE_FORMAT_DXT5 && srcImageFormat == IMAGE_FORMAT_DXT5_RUNTIME ) ||
		 ( dstImageFormat == IMAGE_FORMAT_DXT1 && srcImageFormat == IMAGE_FORMAT_DXT1_RUNTIME ) ||
		 ( dstImageFormat == IMAGE_FORMAT_DXT5_RUNTIME && srcImageFormat == IMAGE_FORMAT_DXT5 ) ||
		 ( dstImageFormat == IMAGE_FORMAT_DXT1_RUNTIME && srcImageFormat == IMAGE_FORMAT_DXT1 ) )
	{
		// Fast path for compressed textures . . stride doesn't make as much sense.
//		Assert( srcStride == 0 && dstStride == 0 );

		int memRequired;
		memRequired = GetMemRequired( width, height, 1, srcImageFormat, false );
		memcpy( dst, src, memRequired );
		return true;
	}
	else if ( ( srcImageFormat == IMAGE_FORMAT_RGBA8888 ||		
			   srcImageFormat == IMAGE_FORMAT_RGB888    ||														// RGBA source
			   srcImageFormat == IMAGE_FORMAT_BGRA8888  ||														//
			   srcImageFormat == IMAGE_FORMAT_BGRX8888 ) &&	   													// and
			 ( dstImageFormat == IMAGE_FORMAT_DXT1  ||															//
			   dstImageFormat == IMAGE_FORMAT_DXT3  ||	   														// DXT compressed dest
			   dstImageFormat == IMAGE_FORMAT_DXT5  ||
			   dstImageFormat == IMAGE_FORMAT_DXT1_RUNTIME ||
			   dstImageFormat == IMAGE_FORMAT_DXT5_RUNTIME ) )
	{
		return ConvertToDXT( src, srcImageFormat, dst, dstImageFormat, width, height, srcStride, dstStride );
	}
	else if ( ( srcImageFormat == IMAGE_FORMAT_ARGB8888 ) &&									 				// RGBA source and
			  ( dstImageFormat == IMAGE_FORMAT_ATI1N    || dstImageFormat == IMAGE_FORMAT_ATI2N ) )				// ATI compressed dest
	{
		return ConvertToATIxN( src, srcImageFormat, dst, dstImageFormat, width, height, srcStride, dstStride );
	}
	else if ( ( dstImageFormat == IMAGE_FORMAT_RGBA8888 ||
			   dstImageFormat == IMAGE_FORMAT_BGRX8888 ||
			   dstImageFormat == IMAGE_FORMAT_BGRA8888 ||
			   dstImageFormat == IMAGE_FORMAT_BGRA4444 ||
			   dstImageFormat == IMAGE_FORMAT_BGRA5551 ||
			   dstImageFormat == IMAGE_FORMAT_BGRX5551 ||
			   dstImageFormat == IMAGE_FORMAT_BGR565 ||
			   dstImageFormat == IMAGE_FORMAT_BGR888 ||
			   dstImageFormat == IMAGE_FORMAT_RGB888 ) &&
			 ( srcImageFormat == IMAGE_FORMAT_DXT1  ||
			   srcImageFormat == IMAGE_FORMAT_DXT3  ||
			   srcImageFormat == IMAGE_FORMAT_DXT5  ||
			   srcImageFormat == IMAGE_FORMAT_ATI1N ||
			   srcImageFormat == IMAGE_FORMAT_ATI2N ) )
	{
		// from dxtN to rgb(a)
		if ( srcStride != 0 || dstStride != 0 )
		{
			return false;
		}
		if ( srcImageFormat == IMAGE_FORMAT_DXT1 )
		{
			if ( dstImageFormat == IMAGE_FORMAT_RGBA8888 )
			{
				ConvertFromDXT1( src, ( RGBA8888_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGRA8888 ||
			    dstImageFormat == IMAGE_FORMAT_BGRX8888 )
			{
				ConvertFromDXT1( src, ( BGRA8888_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_RGB888 )
			{
				ConvertFromDXT1( src, ( RGB888_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGR888 )
			{
				ConvertFromDXT1( src, ( BGR888_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGR565 )
			{
				ConvertFromDXT1( src, ( BGR565_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGRA5551 ||
				dstImageFormat == IMAGE_FORMAT_BGRX5551 )
			{
				ConvertFromDXT1( src, ( BGRA5551_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGRA4444 )
			{
				ConvertFromDXT1( src, ( BGRA4444_t * )dst, width, height );
				return true;
			}
		}
		else if ( srcImageFormat == IMAGE_FORMAT_ATI2N )
		{
			if ( dstImageFormat == IMAGE_FORMAT_BGRA8888 )
			{
				ConvertFromATIxN( src, ( BGRA8888_t * )dst, width, height, true );
				return true;
			}
		}
		else if ( srcImageFormat == IMAGE_FORMAT_ATI1N )
		{
			if ( dstImageFormat == IMAGE_FORMAT_BGRA8888 )
			{
				ConvertFromATIxN( src, ( BGRA8888_t * )dst, width, height, false );
				return true;
			}
		}
		else if ( srcImageFormat == IMAGE_FORMAT_DXT5 )
		{
			if ( dstImageFormat == IMAGE_FORMAT_RGBA8888 )
			{
				ConvertFromDXT5( src, ( RGBA8888_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGRA8888 ||
			    dstImageFormat == IMAGE_FORMAT_BGRX8888 )
			{
				ConvertFromDXT5( src, ( BGRA8888_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_RGB888 )
			{
				ConvertFromDXT5IgnoreAlpha( src, ( RGB888_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGR888 )
			{
				ConvertFromDXT5IgnoreAlpha( src, ( BGR888_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGR565 )
			{
				ConvertFromDXT5IgnoreAlpha( src, ( BGR565_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGRA5551 ||
				dstImageFormat == IMAGE_FORMAT_BGRX5551 )
			{
				ConvertFromDXT5( src, ( BGRA5551_t * )dst, width, height );
				return true;
			}
			if ( dstImageFormat == IMAGE_FORMAT_BGRA4444 )
			{
				ConvertFromDXT5( src, ( BGRA4444_t * )dst, width, height );
				return true;
			}
		}
		return false;
	}
	else if ( dstImageFormat == IMAGE_FORMAT_DXT1  ||
			  dstImageFormat == IMAGE_FORMAT_DXT3  ||
			  dstImageFormat == IMAGE_FORMAT_DXT5  ||
			  dstImageFormat == IMAGE_FORMAT_ATI1N ||
			  dstImageFormat == IMAGE_FORMAT_ATI2N ||
			  srcImageFormat == IMAGE_FORMAT_DXT1  ||
			  srcImageFormat == IMAGE_FORMAT_DXT3  ||
			  srcImageFormat == IMAGE_FORMAT_DXT5  ||
			  srcImageFormat == IMAGE_FORMAT_ATI1N ||
			  srcImageFormat == IMAGE_FORMAT_ATI2N )
	{
		// DxtN to DxtN
		Assert( IsPC() );
		return false;
	}
	else
	{
		// uncompressed textures
		int line;
		int srcPixelSize = SizeInBytes(srcImageFormat);
		int dstPixelSize = SizeInBytes(dstImageFormat);
		
		if ( srcStride == 0 )
		{
			srcStride = srcPixelSize * width;
		}
		if ( dstStride == 0 )
		{
			dstStride = dstPixelSize * width;
		}
		
		// Fast path...
		if( ( srcImageFormat == dstImageFormat ) || 
			((srcImageFormat == IMAGE_FORMAT_BGRA8888) && (dstImageFormat == IMAGE_FORMAT_BGRX8888)) )
		{
			if ( IsX360() && ( srcStride == dstStride ) && ( width*srcPixelSize == srcStride ) )
			{
				// fastest path
				memcpy( dst, src, height*srcStride ); 
				return true;
			}

			for ( line = 0; line < height; ++line )
			{
				memcpy( dst, src, width*srcPixelSize ); 
				dst += dstStride;
				src += srcStride;
			}
			return true;
		}
		
		// format conversion
		uint8 *lineBufRGBA8888 = (uint8 *)_alloca(width*4);
		
		UserFormatToRGBA8888Func_t userFormatToRGBA8888Func;
		RGBA8888ToUserFormatFunc_t RGBA8888ToUserFormatFunc;
		
		userFormatToRGBA8888Func = GetUserFormatToRGBA8888Func_t( srcImageFormat );
		RGBA8888ToUserFormatFunc = GetRGBA8888ToUserFormatFunc_t( dstImageFormat );
		
		if ( !userFormatToRGBA8888Func || !RGBA8888ToUserFormatFunc )
		{
			return false;
		}

		for ( line = 0; line < height; line++ )
		{
			userFormatToRGBA8888Func( src + line * srcStride, lineBufRGBA8888, width );
			RGBA8888ToUserFormatFunc( lineBufRGBA8888, dst + line * dstStride, width );
		}
		return true;
	}
}



//-----------------------------------------------------------------------------
// Color conversion routines
//-----------------------------------------------------------------------------
void ConvertIA88ImageToNormalMapRGBA8888( const uint8 *src, int width, 
										  int height, uint8 *dst,
										  float bumpScale )
{
	float heightScale = ( 1.0f / 255.0f ) * bumpScale;
	float c, cx, cy;
	float maxDim = ( width > height ) ? width : height;
	float ooMaxDim = 1.0f / maxDim;

	int s, t;
	for( t = 0; t < height; t++ )
	{
		uint8 *dstPixel = &dst[t * width * 4];
		for( s = 0; s < width; s++ )
		{
			c = src[( t * width + s ) * 2];
			cx = src[( t * width + ((s+1)%width) ) * 2];
			cy = src[( ((t+1)%height) * width + s ) * 2];

			/*
			// \Z (out of screen)
			//  \
			//   \
			//    \
			//     \-----------  X
			//     |
			//     |
			//     |
			//     |
			//     |
			//     Y
			*/
			
			Vector xVect, yVect, normal;
			xVect[0] = ooMaxDim;
			xVect[1] = 0.0f;
			xVect[2] = (cx - c) * heightScale;

			yVect[0] = 0.0f;
			yVect[1] = ooMaxDim;
			yVect[2] = (cy - c) * heightScale;

			CrossProduct( xVect, yVect, normal );
			VectorNormalize( normal );

			/* Repack the normalized vector into an RGB unsigned byte
			vector in the normal map image. */
			dstPixel[0] = ( uint8 )( 128 + 127*normal[0] );
			dstPixel[1] = ( uint8 )( 128 + 127*normal[1] );
			dstPixel[2] = ( uint8 )( 128 + 127*normal[2] );
			dstPixel[3] = src[( ( t * width + s ) * 2 ) + 1];
			dstPixel += 4;
		}
	}
}

void ConvertNormalMapRGBA8888ToDUDVMapUVWQ8888( const uint8 *src, int width, int height,
										                     uint8 *dst_ )
{
	unsigned const char *lastPixel = src + width * height * 4;
	char *dst = ( char * )dst_; // NOTE: this is signed!!!!

	for( ; src < lastPixel; src += 4, dst += 4 )
	{
		dst[0] = ( char )( ( ( int )src[0] ) - 127 );
		dst[1] = ( char )( ( ( int )src[1] ) - 127 );
		dst[2] = ( char )( ( ( int )src[2] ) - 127 );
		dst[3] = ( char )( ( ( int )src[3] ) - 127 );
	}
}

void ConvertNormalMapRGBA8888ToDUDVMapUVLX8888( const uint8 *src, int width, int height,
										                     uint8 *dst_ )
{
	unsigned const char *lastPixel = src + width * height * 4;
	char *dst = ( char * )dst_; // NOTE: this is signed!!!!

	for( ; src < lastPixel; src += 4, dst += 4 )
	{
		dst[0] = ( char )( ( ( int )src[0] ) - 127 );
		dst[1] = ( char )( ( ( int )src[1] ) - 127 );

		uint8 *pUDst = (uint8 *)dst;
		pUDst[2] = src[3];
		pUDst[3] = 0xFF;
	}
}

void ConvertNormalMapRGBA8888ToDUDVMapUV88( const uint8 *src, int width, int height,
										                     uint8 *dst_ )
{
	unsigned const char *lastPixel = src + width * height * 4;
	char *dst = ( char * )dst_; // NOTE: this is signed!!!!

	for( ; src < lastPixel; src += 4, dst += 2 )
	{
		dst[0] = ( char )( ( ( int )src[0] ) - 127 );
		dst[1] = ( char )( ( ( int )src[1] ) - 127 );
	}
}

void NormalizeNormalMapRGBA8888( uint8 *src, int numTexels )
{
	uint8 *lastPixel = src + numTexels * 4;

	for( uint8 *pixel = src; pixel < lastPixel; pixel += 4 )
	{
		Vector tmpVect;
		tmpVect[0] = ( ( float )pixel[0] - 128.0f ) * ( 1.0f / 127.0f );
		tmpVect[1] = ( ( float )pixel[1] - 128.0f ) * ( 1.0f / 127.0f );
		tmpVect[2] = ( ( float )pixel[2] - 128.0f ) * ( 1.0f / 127.0f );

		VectorNormalize( tmpVect );

		pixel[0] = ( uint8 )( 128 + 127 * tmpVect[0] );
		pixel[1] = ( uint8 )( 128 + 127 * tmpVect[1] );
		pixel[2] = ( uint8 )( 128 + 127 * tmpVect[2] );
	}
}

//-----------------------------------------------------------------------------
// Image rotation
//-----------------------------------------------------------------------------
bool RotateImageLeft( const uint8 *src, uint8 *dst, 
					  int widthHeight, ImageFormat imageFormat )
{
#define SRC(x,y) src[((x)+(y)*widthHeight)*sizeInBytes]
#define DST(x,y) dst[((x)+(y)*widthHeight)*sizeInBytes]
	if( IsCompressed( imageFormat ) )
	{
		return false;
	}

	int x, y;

	uint8 tmp[4][16];
	int halfWidthHeight = widthHeight >> 1;
	int sizeInBytes = SizeInBytes( imageFormat );
	Assert( sizeInBytes <= 16 && sizeInBytes > 0 );

	for( y = 0; y < halfWidthHeight; y++ )
	{
		for( x = 0; x < halfWidthHeight; x++ )
		{
			memcpy( tmp[0], &SRC( x, y ), sizeInBytes );
			memcpy( tmp[1], &SRC( y, widthHeight-x-1 ), sizeInBytes );
			memcpy( tmp[2], &SRC( widthHeight-x-1, widthHeight-y-1 ), sizeInBytes );
			memcpy( tmp[3], &SRC( widthHeight-y-1, x ), sizeInBytes );
			memcpy( &DST( x, y ),                             tmp[3], sizeInBytes );
			memcpy( &DST( y, widthHeight-x-1 ),               tmp[0], sizeInBytes );
			memcpy( &DST( widthHeight-x-1, widthHeight-y-1 ), tmp[1], sizeInBytes );
			memcpy( &DST( widthHeight-y-1, x ),               tmp[2], sizeInBytes );
		}
	}
#undef SRC
#undef DST
	return true;
}

bool RotateImage180( const uint8 *src, uint8 *dst, 
					  int widthHeight, ImageFormat imageFormat )
{
	// OPTIMIZE: do this transformation directly.
	if( RotateImageLeft( src, dst, widthHeight, imageFormat ) )
	{
		return RotateImageLeft( dst, dst, widthHeight, imageFormat );
	}
	return false;
}

bool FlipImageVertically( void *pSrc, void *pDst, int nWidth, int nHeight, ImageFormat imageFormat, int nDstStride )
{
	if( IsCompressed( imageFormat ) )
		return false;

	int nSizeInBytes = SizeInBytes( imageFormat );
	int nRowBytes = nSizeInBytes * nWidth;
	int nSrcStride = nRowBytes;
	if ( nDstStride == 0 )
	{
		nDstStride = nRowBytes;
	}

	uint8 *pSrcRow = (uint8*)pSrc;
	uint8 *pDstRow = (uint8*)pDst + ((nHeight-1) * nDstStride);
	if ( pSrc == pDst )
	{
		uint8* pTemp = (uint8*)_alloca( nRowBytes );
		int nHalfHeight = nHeight >> 1;
		for ( int i = 0; i < nHalfHeight; i++ )
		{
			memcpy( pTemp, pSrcRow, nRowBytes );
			memcpy( pSrcRow, pDstRow, nRowBytes );
			memcpy( pDstRow, pTemp, nRowBytes );

			pSrcRow += nSrcStride;
			pDstRow -= nDstStride;
		}
	}
	else
	{
		for ( int i = 0; i < nHeight; i++ )
		{
			memcpy( pDstRow, pSrcRow, nRowBytes );
			pSrcRow += nSrcStride;
			pDstRow -= nDstStride;
		}
	}

	return true;
}

bool FlipImageHorizontally( void *pSrc, void *pDst, int nWidth, int nHeight, ImageFormat imageFormat, int nDstStride )
{
	if( IsCompressed( imageFormat ) )
		return false;

	uint8 tmp[16];
	int nSizeInBytes = SizeInBytes( imageFormat );
	int nRowBytes = nSizeInBytes * nWidth;
	Assert( nSizeInBytes <= 16 && nSizeInBytes > 0 );
	int nSrcStride = nRowBytes;
	if ( nDstStride == 0 )
	{
		nDstStride = nRowBytes;
	}

	int x, y;
	uint8 *pSrcRow = (uint8*)pSrc;
	uint8 *pDstRow = (uint8*)pDst;
	if ( pSrc == pDst )
	{
		int nHalfWidth = nWidth >> 1;
		for( y = 0; y < nHeight; y++ )
		{
			uint8 *pSrcPixel = pSrcRow;
			uint8 *pDstPixel = pDstRow + nRowBytes - nSizeInBytes;
			for( x = 0; x < nHalfWidth; x++ )
			{
				memcpy( tmp, pSrcPixel, nSizeInBytes );
				memcpy( pSrcPixel, pDstPixel, nSizeInBytes );
				memcpy( pDstPixel, tmp, nSizeInBytes );

				pSrcPixel += nSizeInBytes;
				pDstPixel -= nSizeInBytes;
			}

			pSrcRow += nSrcStride;
			pDstRow += nDstStride;
		}
	}
	else
	{
		for( y = 0; y < nHeight; y++ )
		{
			uint8 *pSrcPixel = pSrcRow;
			uint8 *pDstPixel = pDstRow + nRowBytes - nSizeInBytes;
			for( x = 0; x < nWidth; x++ )
			{
				memcpy( pDstPixel, pSrcPixel, nSizeInBytes );
				pSrcPixel += nSizeInBytes;
				pDstPixel -= nSizeInBytes;
			}

			pSrcRow += nSrcStride;
			pDstRow += nDstStride;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Image rotation
//-----------------------------------------------------------------------------
bool SwapAxes( uint8 *src, int widthHeight, ImageFormat imageFormat )
{
#define SRC(x,y) src[((x)+(y)*widthHeight)*sizeInBytes]
	if( IsCompressed( imageFormat ) )
	{
		return false;
	}

	int x, y;

	uint8 tmp[4];
	int sizeInBytes = SizeInBytes( imageFormat );
	Assert( sizeInBytes <= 4 && sizeInBytes > 0 );

	for( y = 0; y < widthHeight; y++ )
	{
		for( x = 0; x < y; x++ )
		{
			memcpy( tmp, &SRC( x, y ), sizeInBytes );
			memcpy( &SRC( x, y ), &SRC( y, x ), sizeInBytes );
			memcpy( &SRC( y, x ), tmp, sizeInBytes );
		}
	}
#undef SRC
	return true;
}

void RGBA8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	memcpy( dst, src, 4 * numPixels );
}

void RGBA8888ToABGR8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 4 )
	{
		dst[0] = src[3];
		dst[1] = src[2];
		dst[2] = src[1];
		dst[3] = src[0];
	}
}

void RGBA8888ToRGB888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 3 )
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
	}
}

void RGBA8888ToBGR888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 3 )
	{
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
	}
}

void RGBA8888ToRGB565( const uint8 *src, uint8 *dst, int numPixels )
{
	Assert( 0 );
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 2 )
	{
	}
}

void RGBA8888ToI8( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 1 )
	{
		dst[0] = ( uint8 )( 0.299f * src[0] + 0.587f * src[1] + 0.114f * src[2] );
	}
}

void RGBA8888ToIA88( const uint8 *src, uint8 *dst, int numPixels )
{
	// fixme: need to find the proper rgb weighting
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 2 )
	{
		dst[0] = ( uint8 )( 0.299f * src[0] + 0.587f * src[1] + 0.114f * src[2] );
		dst[1] = src[3];
	}
}

void RGBA8888ToP8( const uint8 *src, uint8 *dst, int numPixels )
{
	Assert( 0 );
}

void RGBA8888ToA8( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 1 )
	{
		dst[0] = src[3];
	}
}

void RGBA8888ToRGB888_BLUESCREEN( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 3 )
	{
		if( src[3] == 0 )
		{
			dst[0] = 0;
			dst[1] = 0;
			dst[2] = 255;
		}
		else
		{
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
		}
	}
}

void RGBA8888ToBGR888_BLUESCREEN( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 3 )
	{
		if( src[3] == 0 )
		{
			dst[2] = 0;
			dst[1] = 0;
			dst[0] = 255;
		}
		else
		{
			dst[2] = src[0];
			dst[1] = src[1];
			dst[0] = src[2];
		}
	}
}

void RGBA8888ToARGB8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 4 )
	{
		dst[0] = src[3];
		dst[1] = src[0];
		dst[2] = src[1];
		dst[3] = src[2];
	}
}

void RGBA8888ToBGRA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 4 )
	{
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		dst[3] = src[3];
	}
}

void RGBA8888ToBGRX8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 4 )
	{
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
	}
}

void RGBA8888ToBGR565( const uint8 *src, uint8 *dst, int numPixels )
{
	unsigned short* pDstShort = (unsigned short*)dst;
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, pDstShort ++ )
	{
		*pDstShort = ((src[0] >> 3) << 11) |
					 ((src[1] >> 2) << 5) |
					  (src[2] >> 3);
	}
}

void RGBA8888ToBGRX5551( const uint8 *src, uint8 *dst, int numPixels )
{
	unsigned short* pDstShort = (unsigned short*)dst;
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, pDstShort ++ )
	{
		*pDstShort = ((src[0] >> 3) << 10) |
					 ((src[1] >> 3) << 5) |
					  (src[2] >> 3);
	}
}

void RGBA8888ToBGRA5551( const uint8 *src, uint8 *dst, int numPixels )
{
	unsigned short* pDstShort = (unsigned short*)dst;
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, pDstShort ++ )
	{
		*pDstShort = ((src[0] >> 3) << 10) |
					 ((src[1] >> 3) << 5) |
					  (src[2] >> 3) |
					  (src[3] >> 7) << 15;
	}
}

void RGBA8888ToBGRA4444( const uint8 *src, uint8 *dst, int numPixels )
{
	unsigned short* pDstShort = (unsigned short*)dst;
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, pDstShort ++ )
	{
		*pDstShort = ((src[0] >> 4) << 8) |
					 ((src[1] >> 4) << 4) |
					  (src[2] >> 4) |
					 ((src[3] >> 4) << 12);
	}
}

void RGBA8888ToUV88( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 2 )
	{
		dst[0] = src[0];
		dst[1] = src[1];
	}
}

void RGBA8888ToUVWQ8888( const uint8 *src, uint8 *dst, int numPixels )
{
	RGBA8888ToRGBA8888( src, dst, numPixels );
}

void RGBA8888ToUVLX8888( const uint8 *src, uint8 *dst, int numPixels )
{
	RGBA8888ToRGBA8888( src, dst, numPixels );
}

void ABGR8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 4 )
	{
		dst[0] = src[3];
		dst[1] = src[2];
		dst[2] = src[1];
		dst[3] = src[0];
	}
}

void RGB888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 3;
	for ( ; src < endSrc; src += 3, dst += 4 )
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = 255;
	}
}

void BGR888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 3;
	for ( ; src < endSrc; src += 3, dst += 4 )
	{
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		dst[3] = 255;
	}
}

void RGB565ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	Assert( 0 );
	const uint8 *endSrc = src + numPixels * 2;
	for ( ; src < endSrc; src += 2, dst += 4 )
	{
	}
}

void I8ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels;
	for ( ; src < endSrc; src += 1, dst += 4 )
	{
		dst[0] = src[0];
		dst[1] = src[0];
		dst[2] = src[0];
		dst[3] = 255;
	}
}

void IA88ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 2;
	for ( ; src < endSrc; src += 2, dst += 4 )
	{
		dst[0] = src[0];
		dst[1] = src[0];
		dst[2] = src[0];
		dst[3] = src[1];
	}
}

void P8ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	Assert( 0 );
}

void A8ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels;
	for ( ; src < endSrc; src += 1, dst += 4 )
	{
		dst[0] = src[0];
		dst[1] = src[0];
		dst[2] = src[0];
		dst[3] = src[0];
	}
}

void RGB888_BLUESCREENToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 3;
	for ( ; src < endSrc; src += 3, dst += 4 )
	{
		if( src[0] == 0 && src[1] == 0 && src[2] == 255 )
		{
			dst[0] = 0;
			dst[1] = 0;
			dst[2] = 0;
			dst[3] = 0;
		}
		else
		{
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = 255;
		}
	}
}

void BGR888_BLUESCREENToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 3;
	for ( ; src < endSrc; src += 3, dst += 4 )
	{
		if( src[2] == 0 && src[1] == 0 && src[0] == 255 )
		{
			dst[0] = 0;
			dst[1] = 0;
			dst[2] = 0;
			dst[3] = 0;
		}
		else
		{
			dst[2] = src[0];
			dst[1] = src[1];
			dst[0] = src[2];
			dst[3] = 255;
		}
	}
}

void ARGB8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 4 )
	{
		dst[0] = src[1];
		dst[1] = src[2];
		dst[2] = src[3];
		dst[3] = src[0];
	}
}

void BGRA8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 4 )
	{
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		dst[3] = src[3];
	}
}

void BGRX8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8 *endSrc = src + numPixels * 4;
	for ( ; src < endSrc; src += 4, dst += 4 )
	{
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		dst[3] = 255;
	}
}

void BGR565ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	unsigned short* pSrcShort = (unsigned short*)src;
	unsigned short* pEndSrc = pSrcShort + numPixels;
	for ( ; pSrcShort < pEndSrc; pSrcShort++, dst += 4 )
	{
		int blue = (*pSrcShort & 0x1F);
		int green = (*pSrcShort >> 5) & 0x3F;
		int red = (*pSrcShort >> 11) & 0x1F;

		// Expand to 8 bits
		dst[0] = (red << 3) | (red >> 2);
		dst[1] = (green << 2) | (green >> 4);
		dst[2] = (blue << 3) | (blue >> 2);
		dst[3] = 255;
	}
}

void BGRX5551ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	unsigned short* pSrcShort = (unsigned short*)src;
	unsigned short* pEndSrc = pSrcShort + numPixels;
	for ( ; pSrcShort < pEndSrc; pSrcShort++, dst += 4 )
	{
		int blue = (*pSrcShort & 0x1F);
		int green = (*pSrcShort >> 5) & 0x1F;
		int red = (*pSrcShort >> 10) & 0x1F;

		// Expand to 8 bits
		dst[0] = (red << 3) | (red >> 2);
		dst[1] = (green << 3) | (green >> 2);
		dst[2] = (blue << 3) | (blue >> 2);
		dst[3] = 255;
	}
}

void BGRA5551ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	unsigned short* pSrcShort = (unsigned short*)src;
	unsigned short* pEndSrc = pSrcShort + numPixels;
	for ( ; pSrcShort < pEndSrc; pSrcShort++, dst += 4 )
	{
		int blue = (*pSrcShort & 0x1F);
		int green = (*pSrcShort >> 5) & 0x1F;
		int red = (*pSrcShort >> 10) & 0x1F;
		int alpha = *pSrcShort & ( 1 << 15 );

		// Expand to 8 bits
		dst[0] = (red << 3) | (red >> 2);
		dst[1] = (green << 3) | (green >> 2);
		dst[2] = (blue << 3) | (blue >> 2);
		// garymcthack
		if( alpha )
		{
			dst[3] = 255;
		}
		else
		{
			dst[3] = 0;
		}
	}
}

void BGRA4444ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	unsigned short* pSrcShort = (unsigned short*)src;
	unsigned short* pEndSrc = pSrcShort + numPixels;
	for ( ; pSrcShort < pEndSrc; pSrcShort++, dst += 4 )
	{
		int blue = (*pSrcShort & 0xF);
		int green = (*pSrcShort >> 4) & 0xF;
		int red = (*pSrcShort >> 8) & 0xF;
		int alpha = (*pSrcShort >> 12) & 0xF;

		// Expand to 8 bits
		// FIXME: shouldn't this be (red << 4) | red?
		dst[0] = (red << 4) | (red >> 4);
		dst[1] = (green << 4) | (green >> 4);
		dst[2] = (blue << 4) | (blue >> 4);
		dst[3] = (alpha << 4) | (alpha >> 4);
	}
}

void UV88ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	const uint8* pEndSrc = src + numPixels * 2;
	for ( ; src < pEndSrc; src += 2, dst += 4 )
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = 0;
		dst[3] = 0;
	}
}

void UVWQ8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	RGBA8888ToRGBA8888( src, dst, numPixels );
}

void UVLX8888ToRGBA8888( const uint8 *src, uint8 *dst, int numPixels )
{
	RGBA8888ToRGBA8888( src, dst, numPixels );
}

// HDRFIXME: This assumes that the 16-bit integer values are 4.12 fixed-point.
void RGBA16161616ToRGBA8888( const uint8 *src_, uint8 *dst, int numPixels )
{
	unsigned short *src = ( unsigned short * )src_;
	unsigned short *pEndSrc = src + numPixels * 4;
	for ( ; src < pEndSrc; src += 4, dst += 4 )
	{
		dst[0] = min( 255, src[0] >> 4 );
		dst[1] = min( 255, src[1] >> 4 );
		dst[2] = min( 255, src[2] >> 4 );
		dst[3] = min( 255, src[3] >> 8 );
	}
}

} // ImageLoader namespace ends



