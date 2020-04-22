//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef S3TC_DECODE_H
#define S3TC_DECODE_H
#ifdef _WIN32
#pragma once
#endif


#include "bitmap/imageformat.h"
enum ImageFormat;


class S3RGBA
{
public:
	unsigned char b, g, r, a;
};

class S3PaletteIndex
{
public:
	unsigned char m_AlphaIndex;
	unsigned char m_ColorIndex;
};


#define MAX_S3TC_BLOCK_BYTES	16


S3PaletteIndex S3TC_GetPixelPaletteIndex( ImageFormat format, const char *pS3Block, int x, int y );

void S3TC_SetPixelPaletteIndex( ImageFormat format, char *pS3Block, int x, int y, S3PaletteIndex iPaletteIndex );



// Note: width, x, and y are in texels, not S3 blocks.
S3PaletteIndex S3TC_GetPaletteIndex( 
	unsigned char *pFaceData,
	ImageFormat format,
	int imageWidth,
	int x,
	int y );


// Note: width, x, and y are in texels, not S3 blocks.
void S3TC_SetPaletteIndex( 
	unsigned char *pFaceData,
	ImageFormat format,
	int imageWidth,
	int x,
	int y,
	S3PaletteIndex paletteIndex );


const char* S3TC_GetBlock( 
	const void *pCompressed, 
	ImageFormat format, 
	int nBlocksWide, // How many blocks wide is the image (pixels wide / 4).
	int xBlock,
	int yBlock );


char* S3TC_GetBlock( 
	void *pCompressed, 
	ImageFormat format, 
	int nBlocksWide, // How many blocks wide is the image (pixels wide / 4).
	int xBlock,
	int yBlock );


// Merge the two palettes and copy the colors
void S3TC_MergeBlocks( 
	char **blocks,
	S3RGBA **pOriginals,	// Original RGBA colors in the texture. This allows it to avoid doubly compressing.
	int nBlocks,
	int lPitch,	// (in BYTES)
	ImageFormat format
	);


// Convert an RGB565 color to RGBA8888.
inline S3RGBA S3TC_RGBAFrom565( unsigned short color, unsigned char alphaValue=255 )
{
	S3RGBA ret;
	ret.a = alphaValue;
	ret.r = (unsigned char)( (color >> 11) << 3 );
	ret.g = (unsigned char)( ((color >> 5) & 0x3F) << 2 );
	ret.b = (unsigned char)( (color & 0x1F) << 3 );
	return ret;
}

// Blend from one color to another..
inline S3RGBA S3TC_RGBABlend( const S3RGBA &a, const S3RGBA &b, int aMul, int bMul, int div )
{
	S3RGBA ret;
	ret.r = (unsigned char)(( (int)a.r * aMul + (int)b.r * bMul ) / div );
	ret.g = (unsigned char)(( (int)a.g * aMul + (int)b.g * bMul ) / div );
	ret.b = (unsigned char)(( (int)a.b * aMul + (int)b.b * bMul ) / div );
	ret.a = (unsigned char)(( (int)a.a * aMul + (int)b.a * bMul ) / div );
	return ret;
}


#endif // S3TC_DECODE_H
