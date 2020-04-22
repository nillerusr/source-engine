//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//


#ifdef _WIN32
#include <windows.h>
#endif
#include "bitmap/imageformat.h"
#include "basetypes.h"
#include "tier0/dbg.h"
#include <malloc.h>
#include <memory.h>
#include "nvtc.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "utlmemory.h"
#include "tier1/strtools.h"
#include "s3tc_decode.h"
#include "utlvector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// This is in s3tc.lib. Nvidia added it specially for us. It can be set to 4, 8, or 12.
// When set to 8 or 12, it generates a palette given an 8x4 or 12x4 texture.
extern int S3TC_BLOCK_WIDTH;


class S3Palette
{
public:
	S3RGBA m_Colors[4];
};


class S3TCBlock_DXT1
{
public:
	unsigned short m_Ref1;		// The two colors that this block blends betwixt.
	unsigned short m_Ref2;
	unsigned int m_PixelBits;
};


class S3TCBlock_DXT5
{
public:
	unsigned char m_AlphaRef[2];
	unsigned char m_AlphaBits[6];

	unsigned short m_Ref1;		// The two colors that this block blends betwixt.
	unsigned short m_Ref2;
	unsigned int m_PixelBits;
};



// ------------------------------------------------------------------------------------------ //
// S3TCBlock
// ------------------------------------------------------------------------------------------ //

int ReadBitInt( const char *pBits, int iBaseBit, int nBits )
{
	int ret = 0;
	for ( int i=0; i < nBits; i++ )
	{
		int iBit = iBaseBit + i;
		int val = ((pBits[iBit>>3] >> (iBit&7)) & 1) << i;
		ret |= val;
	}
	return ret;
}

void WriteBitInt( char *pBits, int iBaseBit, int nBits, int val )
{
	for ( int i=0; i < nBits; i++ )
	{
		int iBit = iBaseBit + i;
		pBits[iBit>>3] &= ~(1 << (iBit & 7));
		if ( (val >> i) & 1 )
			pBits[iBit>>3] |= (1 << (iBit & 7));
	}
}

int S3TC_BytesPerBlock( ImageFormat format )
{
	if ( format == IMAGE_FORMAT_DXT1 || format == IMAGE_FORMAT_ATI1N )
	{
		return 8;
	}
	else
	{
		Assert( format == IMAGE_FORMAT_DXT5 || format == IMAGE_FORMAT_ATI2N );
		return 16;
	}
}


/*

// We're not using this, but I'll keep it around for reference.
void S3TC_BuildPalette( ImageFormat format, const char *pS3Block, S3RGBA palette[4] )
{
	if ( format == IMAGE_FORMAT_DXT1 )
	{
		const S3TCBlock_DXT1 *pBlock = reinterpret_cast<const S3TCBlock_DXT1 *>( pS3Block );

		palette[0] = S3TC_RGBAFrom565( pBlock->m_Ref1, 255 );

		if ( pBlock->m_Ref1 <= pBlock->m_Ref2 )
		{
			// Opaque and transparent texels are defined. The lookup is 3 colors. 11 means
			// a black, transparent pixel.
			palette[1] = S3TC_RGBAFrom565( pBlock->m_Ref2, 255 );
			palette[2] = S3TC_RGBABlend( palette[0], palette[1], 1, 1, 2 );	
			palette[3].r = palette[3].g = palette[3].b = palette[3].a = 0;
		}
		else
		{
			// Only opaque texels are defined. The lookup is 4 colors.
			palette[1] = S3TC_RGBAFrom565( pBlock->m_Ref2, 255 );
			palette[2] = S3TC_RGBABlend( palette[0], palette[1], 2, 1, 3 );
			palette[3] = S3TC_RGBABlend( palette[0], palette[1], 1, 2, 3 );
		}
	}
	else
	{
		Assert( format == IMAGE_FORMAT_DXT5 );
	}
}

*/


S3PaletteIndex S3TC_GetPixelPaletteIndex( ImageFormat format, const char *pS3Block, int x, int y )
{
	Assert( x >= 0 && x < 4 );
	Assert( y >= 0 && y < 4 );
	int iQuadPixel = y*4 + x;
	S3PaletteIndex ret = { 0, 0 };

	if ( format == IMAGE_FORMAT_DXT1 )
	{
		const S3TCBlock_DXT1 *pBlock = reinterpret_cast<const S3TCBlock_DXT1 *>( pS3Block );
		ret.m_ColorIndex = (pBlock->m_PixelBits >> (iQuadPixel << 1)) & 3;
		ret.m_AlphaIndex = 0;
	}
	else
	{
		Assert( format == IMAGE_FORMAT_DXT5 );

		const S3TCBlock_DXT5 *pBlock = reinterpret_cast<const S3TCBlock_DXT5 *>( pS3Block );
		
		int64 &alphaBits = *((int64*)pBlock->m_AlphaBits);
		ret.m_ColorIndex = (unsigned char)((pBlock->m_PixelBits >> (iQuadPixel << 1)) & 3);
		ret.m_AlphaIndex = (unsigned char)((alphaBits >> (iQuadPixel * 3)) & 7);
	}

	return ret;
}


void S3TC_SetPixelPaletteIndex( ImageFormat format, char *pS3Block, int x, int y, S3PaletteIndex iPaletteIndex )
{
	Assert( x >= 0 && x < 4 );
	Assert( y >= 0 && y < 4 );
	Assert( iPaletteIndex.m_ColorIndex >= 0 && iPaletteIndex.m_ColorIndex < 4 );
	Assert( iPaletteIndex.m_AlphaIndex >= 0 && iPaletteIndex.m_AlphaIndex < 8 );

	int iQuadPixel = y*4 + x;
	int iColorBit = iQuadPixel * 2;
	
	if ( format == IMAGE_FORMAT_DXT1 )
	{
		S3TCBlock_DXT1 *pBlock = reinterpret_cast<S3TCBlock_DXT1 *>( pS3Block );

		pBlock->m_PixelBits &= ~( 3 << iColorBit );
		pBlock->m_PixelBits |= (unsigned int)iPaletteIndex.m_ColorIndex << iColorBit;
	}
	else
	{
		Assert( format == IMAGE_FORMAT_DXT5 );
		 
		S3TCBlock_DXT5 *pBlock = reinterpret_cast<S3TCBlock_DXT5 *>( pS3Block );

		// Copy the color portion in.
		pBlock->m_PixelBits &= ~( 3 << iColorBit );
		pBlock->m_PixelBits |= (unsigned int)iPaletteIndex.m_ColorIndex << iColorBit;
		 
		 // Copy the alpha portion in.
		WriteBitInt( (char*)pBlock->m_AlphaBits, iQuadPixel*3, 3, iPaletteIndex.m_AlphaIndex );
	}
}


const char* S3TC_GetBlock( 
	const void *pCompressed, 
	ImageFormat format, 
	int nBlocksWidth,
	int xBlock,
	int yBlock )
{
	int nBytesPerBlock = S3TC_BytesPerBlock( format );
	return &((const char*)pCompressed)[ ((yBlock * nBlocksWidth) + xBlock) * nBytesPerBlock ];
}


char* S3TC_GetBlock( 
	void *pCompressed, 
	ImageFormat format, 
	int nBlocksWidth,
	int xBlock,
	int yBlock )
{
	return (char*)S3TC_GetBlock( (const void *)pCompressed, format, nBlocksWidth, xBlock, yBlock );
}


void GenerateRepresentativePalette(
	ImageFormat format,
	S3RGBA **pOriginals,	// Original RGBA colors in the texture. This allows it to avoid doubly compressing.
	int nBlocks,
	int lPitch,	// (in BYTES)
	char mergedBlocks[16*MAX_S3TC_BLOCK_BYTES]
	)
{
	Error( "GenerateRepresentativePalette: not implemented" );	
#if 0														// this code was ifdefed out. no idea under what circumstances it was meant to be called.

	Assert( nBlocks == 2 || nBlocks == 3 );

	S3RGBA values[12*4];
	memset( values, 0xFF, sizeof( values ) );
	int width = nBlocks * 4;
	for ( int i=0; i < nBlocks; i++ )
	{
		for ( int y=0; y < 4; y++ )
		{
			for ( int x=0; x < 4; x++ )
			{
				int outIndex = y*width+(i*4+x);
				values[outIndex] = pOriginals[i][y * (lPitch/4) + x];
			}
		}			
	}
	
	DDSURFACEDESC descIn;
	DDSURFACEDESC descOut;
	memset( &descIn, 0, sizeof(descIn) );
	memset( &descOut, 0, sizeof(descOut) );

	descIn.dwSize = sizeof(descIn);
	descIn.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_LPSURFACE | DDSD_PIXELFORMAT;
	descIn.dwWidth = width;
	descIn.dwHeight = 4;
	descIn.lPitch = width * 4;
	descIn.lpSurface = values;
	descIn.ddpfPixelFormat.dwSize = sizeof( DDPIXELFORMAT );

	descIn.ddpfPixelFormat.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
	descIn.ddpfPixelFormat.dwRGBBitCount = 32;
	descIn.ddpfPixelFormat.dwRBitMask = 0xff0000;
	descIn.ddpfPixelFormat.dwGBitMask = 0x00ff00;
	descIn.ddpfPixelFormat.dwBBitMask = 0x0000ff;
	descIn.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;

	descOut.dwSize = sizeof( descOut );
	
	float weight[3] = {0.3086f, 0.6094f, 0.0820f};
	
	S3TC_BLOCK_WIDTH = nBlocks * 4;
	
	DWORD encodeFlags = S3TC_ENCODE_RGB_FULL;
	if ( format == IMAGE_FORMAT_DXT5 )
		encodeFlags |= S3TC_ENCODE_ALPHA_INTERPOLATED;

	S3TCencode( &descIn, NULL, &descOut, mergedBlocks, encodeFlags, weight );

	S3TC_BLOCK_WIDTH = 4;
#endif
}

void S3TC_MergeBlocks( 
	char **blocks,
	S3RGBA **pOriginals,	// Original RGBA colors in the texture. This allows it to avoid doubly compressing.
	int nBlocks,
	int lPitch,	// (in BYTES)
	ImageFormat format
	)
{
	// Figure out a good palette to represent all of these blocks.
	char mergedBlocks[16*MAX_S3TC_BLOCK_BYTES]; 
	GenerateRepresentativePalette( format, pOriginals, nBlocks, lPitch, mergedBlocks );

	// Build a remap table to remap block 2's colors to block 1's colors.
	if ( format == IMAGE_FORMAT_DXT1 )
	{
		// Grab the palette indices that s3tc.lib made for us.
		const char *pBase = (const char*)mergedBlocks;
		pBase += 4;

		for ( int iBlock=0; iBlock < nBlocks; iBlock++ )
		{
			S3TCBlock_DXT1 *pBlock = ((S3TCBlock_DXT1*)blocks[iBlock]);
			
			// Remap all of the block's pixels.
			for ( int x=0; x < 4; x++ )
			{
				for ( int y=0; y < 4; y++ )
				{
					int iBaseBit = (y*nBlocks*4 + x + iBlock*4) * 2;
					
					S3PaletteIndex index = {0, 0};
					index.m_ColorIndex = ReadBitInt( pBase, iBaseBit, 2 );
					
					S3TC_SetPixelPaletteIndex( format, (char*)pBlock, x, y, index );
				}
			}

			// Copy block 1's palette to block 2.
			pBlock->m_Ref1 = ((S3TCBlock_DXT1*)mergedBlocks)->m_Ref1;
			pBlock->m_Ref2 = ((S3TCBlock_DXT1*)mergedBlocks)->m_Ref2;
		}
	}
	else
	{
		Assert( format == IMAGE_FORMAT_DXT5 );

		// Skip past the alpha palette.
		const char *pAlphaPalette = mergedBlocks;
		const char *pAlphaBits = mergedBlocks + 2;
		
		// Skip past the alpha pixel bits and past the color palette.
		const char *pColorPalette = pAlphaBits + 6*nBlocks;
		const char *pColorBits = pColorPalette + 4;

		for ( int iBlock=0; iBlock < nBlocks; iBlock++ )
		{
			S3TCBlock_DXT5 *pBlock = ((S3TCBlock_DXT5*)blocks[iBlock]);
			
			// Remap all of the block's pixels.
			for ( int x=0; x < 4; x++ )
			{
				for ( int y=0; y < 4; y++ )
				{
					int iBasePixel = (y*nBlocks*4 + x + iBlock*4);
					
					S3PaletteIndex index;
					index.m_ColorIndex = ReadBitInt( pColorBits, iBasePixel * 2, 2 );
					index.m_AlphaIndex = ReadBitInt( pAlphaBits, iBasePixel * 3, 3 );
					
					S3TC_SetPixelPaletteIndex( format, (char*)pBlock, x, y, index );
				}
			}

			// Copy block 1's palette to block 2.
			pBlock->m_AlphaRef[0] = pAlphaPalette[0];
			pBlock->m_AlphaRef[1] = pAlphaPalette[1];
			pBlock->m_Ref1 = *((unsigned short*)pColorPalette);
			pBlock->m_Ref2 = *((unsigned short*)(pColorPalette + 2));
		}
	}
}


S3PaletteIndex S3TC_GetPaletteIndex( 
	unsigned char *pFaceData,
	ImageFormat format,
	int imageWidth,
	int x,
	int y )
{
	char *pBlock = S3TC_GetBlock( pFaceData, format, imageWidth>>2, x>>2, y>>2 );
	return S3TC_GetPixelPaletteIndex( format, pBlock, x&3, y&3 );
}


void S3TC_SetPaletteIndex( 
	unsigned char *pFaceData,
	ImageFormat format,
	int imageWidth,
	int x,
	int y,
	S3PaletteIndex paletteIndex )
{
	char *pBlock = S3TC_GetBlock( pFaceData, format, imageWidth>>2, x>>2, y>>2 );
	S3TC_SetPixelPaletteIndex( format, pBlock, x&3, y&3, paletteIndex );
}




