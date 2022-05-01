//========= Copyright Valve Corporation, All rights reserved. ============//
//                       TOGL CODE LICENSE
//
//  Copyright 2011-2014 Valve Corporation
//  All Rights Reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//
// cglmtex.cpp
//
//===============================================================================

#include "togles/rendermechanism.h"

extern "C" {
#include "decompress.h"
}

#include "tier0/icommandline.h"
#include "glmtexinlines.h"

// memdbgon -must- be the last include file in a .cpp file.
#include "tier0/memdbgon.h"

#if defined(OSX)
#include "appframework/ilaunchermgr.h"
extern ILauncherMgr *g_pLauncherMgr;
#endif

//===============================================================================

#if GLMDEBUG
CGLMTex *g_pFirstCGMLTex;
#endif

ConVar gl_pow2_tempmem( "gl_pow2_tempmem", "0", FCVAR_INTERNAL_USE,
                        "If set, use power-of-two allocations for temporary texture memory during uploads. "
                        "May help with fragmentation on certain systems caused by heavy churn of large allocations." );

#define TEXSPACE_LOGGING 0

// encoding layout to an index where the bits read
//	4	:	1 if compressed
//	2	:	1 if not power of two
//	1	:	1 if mipmapped

bool pwroftwo (int val )
{
	return (val & (val-1)) == 0;
}

int	sEncodeLayoutAsIndex( GLMTexLayoutKey *key )
{
	int index = 0;
	
	if (key->m_texFlags & kGLMTexMipped)
	{
		index |= 1;
	}

	if ( ! ( pwroftwo(key->m_xSize) && pwroftwo(key->m_ySize) && pwroftwo(key->m_zSize) ) )
	{
		// if not all power of two
		index |= 2;
	}

	if (GetFormatDesc( key->m_texFormat )->m_chunkSize >1 )
	{
		index |= 4;
	}

	return index;
}

static unsigned long g_texGlobalBytes[8];

//===============================================================================

const GLMTexFormatDesc g_formatDescTable[] = 
{
	//  not yet handled by this table:
	//	D3DFMT_INDEX16, D3DFMT_VERTEXDATA	// D3DFMT_INDEX32,
		// WTF { D3DFMT_R5G6R5 ???,		GL_RGB,								GL_RGB,					GL_UNSIGNED_SHORT_5_6_5,		1, 2 },
		// WTF { D3DFMT_A ???,				GL_ALPHA8,							GL_ALPHA,				GL_UNSIGNED_BYTE,				1, 1 },
		// ??? D3DFMT_V8U8,
		// ??? D3DFMT_Q8W8V8U8,
		// ??? D3DFMT_X8L8V8U8,
		// ??? D3DFMT_R32F,
		// ??? D3DFMT_D24X4S4 unsure how to handle or if it is ever used..
		// ??? D3DFMT_D15S1 ever used ?
		// ??? D3DFMT_D24X8 ever used?

	// summ-name		d3d-format				gl-int-format						gl-int-format-srgb					gl-data-format			gl-data-type					chunksize, bytes-per-sqchunk
	{ "_D16",			D3DFMT_D16,				GL_DEPTH_COMPONENT16,				0,									GL_DEPTH_COMPONENT,		GL_UNSIGNED_SHORT,				1, 2 },
	{ "_D24X8",			D3DFMT_D24X8,			GL_DEPTH_COMPONENT24,				0,									GL_DEPTH_COMPONENT,		GL_UNSIGNED_INT,				1, 4 },	// ??? unsure on this one
	{ "_D24S8",			D3DFMT_D24S8,			GL_DEPTH24_STENCIL8_EXT,			0,									GL_DEPTH_STENCIL_EXT,	GL_UNSIGNED_INT_24_8_EXT,		1, 4 },

	{ "_A8R8G8B8",		D3DFMT_A8R8G8B8,		GL_RGBA8,							GL_SRGB8_ALPHA8_EXT,				GL_BGRA,				GL_UNSIGNED_INT_8_8_8_8_REV,	1, 4 },
	{ "_A4R4G4B4",		D3DFMT_A4R4G4B4,		GL_RGBA4,							0,									GL_BGRA,				GL_UNSIGNED_SHORT_4_4_4_4_REV,	1, 2 },
	{ "_X8R8G8B8",		D3DFMT_X8R8G8B8,		GL_RGB8,							GL_SRGB8_EXT,						GL_BGRA,				GL_UNSIGNED_INT_8_8_8_8_REV,	1, 4 },
	
	{ "_X1R5G5B5",		D3DFMT_X1R5G5B5,		GL_RGB5,							0,									GL_BGRA,				GL_UNSIGNED_SHORT_1_5_5_5_REV,	1, 2 },
	{ "_A1R5G5B5",		D3DFMT_A1R5G5B5,		GL_RGB5_A1,							0,									GL_BGRA,				GL_UNSIGNED_SHORT_1_5_5_5_REV,	1, 2 },

	{ "_L8",			D3DFMT_L8,				GL_LUMINANCE8,						GL_SLUMINANCE8_EXT,					GL_LUMINANCE,			GL_UNSIGNED_BYTE,				1, 1 },
	{ "_A8L8",			D3DFMT_A8L8,			GL_LUMINANCE8_ALPHA8,				GL_SLUMINANCE8_ALPHA8_EXT,			GL_LUMINANCE_ALPHA,		GL_UNSIGNED_BYTE,				1, 2 },

	{ "_DXT1",			D3DFMT_DXT1,			GL_COMPRESSED_RGB_S3TC_DXT1_EXT,	GL_COMPRESSED_SRGB_S3TC_DXT1_EXT,		GL_RGB,				GL_UNSIGNED_BYTE,				4, 8 },
	{ "_DXT3",			D3DFMT_DXT3,			GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,	GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT,	GL_RGBA,			GL_UNSIGNED_BYTE,				4, 16 },
	{ "_DXT5",			D3DFMT_DXT5,			GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,	GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT,	GL_RGBA,			GL_UNSIGNED_BYTE,				4, 16 },

	{ "_A16B16G16R16F",	D3DFMT_A16B16G16R16F,	GL_RGBA16F_ARB,						0,									GL_RGBA,				GL_HALF_FLOAT_ARB,				1, 8 },
	{ "_A16B16G16R16",	D3DFMT_A16B16G16R16,	GL_RGBA16,							0,									GL_RGBA,				GL_UNSIGNED_SHORT,				1, 8 },		// 16bpc integer tex

	{ "_A32B32G32R32F",	D3DFMT_A32B32G32R32F,	GL_RGBA32F_ARB,						0,									GL_RGBA,				GL_FLOAT,						1, 16 },

	{ "_R8G8B8",		D3DFMT_R8G8B8,			GL_RGB8,							GL_SRGB8_EXT,						GL_BGR,					GL_UNSIGNED_BYTE,				1, 3 },

	{ "_A8",			D3DFMT_A8,				GL_ALPHA8,							0,									GL_ALPHA,				GL_UNSIGNED_BYTE,				1, 1 },
	{ "_R5G6B5",		D3DFMT_R5G6B5,			GL_RGB,								GL_SRGB_EXT,						GL_RGB,					GL_UNSIGNED_SHORT_5_6_5,		1, 2 },

	// fakey tex formats: the stated GL format and the memory layout may not agree (U8V8 for example)
	
	// _Q8W8V8U8 we just pass through as RGBA bytes.  Shader does scale/bias fix
	{ "_Q8W8V8U8",		D3DFMT_Q8W8V8U8,		GL_RGBA8,							0,									GL_BGRA,				GL_UNSIGNED_INT_8_8_8_8_REV,	1, 4 },		// straight ripoff of D3DFMT_A8R8G8B8

	// U8V8 is exposed to the client as 2-bytes per texel, but we download it as 3-byte RGB.
	// WriteTexels needs to do that conversion from rg8 to rgb8 in order to be able to download it correctly
	{ "_V8U8",			D3DFMT_V8U8,			GL_RGB8,							0,									GL_RG,					GL_BYTE,						1, 2 },
	
	{ "_R32F",			D3DFMT_R32F,			GL_R32F,							GL_R32F,							GL_RED,					GL_FLOAT,						1, 4 },
//$ TODO: Need to merge bitmap changes over from Dota to get these formats.
#if 0
	{ "_A2R10G10B10",	D3DFMT_A2R10G10B10,		GL_RGB10_A2,						GL_RGB10_A2,						GL_RGBA,				GL_UNSIGNED_INT_10_10_10_2,		1, 4 },
	{ "_A2B10G10R10",	D3DFMT_A2B10G10R10,		GL_RGB10_A2,						GL_RGB10_A2,						GL_BGRA,				GL_UNSIGNED_INT_10_10_10_2,		1, 4 },
#endif

	/*
		// NV shadow depth tex
		D3DFMT_NV_INTZ		= 0x5a544e49,	// MAKEFOURCC('I','N','T','Z')
		D3DFMT_NV_RAWZ		= 0x5a574152,	// MAKEFOURCC('R','A','W','Z')

		// NV null tex
		D3DFMT_NV_NULL		= 0x4c4c554e,	// MAKEFOURCC('N','U','L','L')

		// ATI shadow depth tex
		D3DFMT_ATI_D16		= 0x36314644,	// MAKEFOURCC('D','F','1','6')
		D3DFMT_ATI_D24S8	= 0x34324644,	// MAKEFOURCC('D','F','2','4')

		// ATI 1N and 2N compressed tex
		D3DFMT_ATI_2N		= 0x32495441,	// MAKEFOURCC('A', 'T', 'I', '2')
		D3DFMT_ATI_1N		= 0x31495441,	// MAKEFOURCC('A', 'T', 'I', '1')
	*/
};

int	g_formatDescTableCount = sizeof(g_formatDescTable) / sizeof( g_formatDescTable[0] );

const GLMTexFormatDesc *GetFormatDesc( D3DFORMAT format )
{
	for( int i=0; i<g_formatDescTableCount; i++)
	{
		if (g_formatDescTable[i].m_d3dFormat == format)
		{
			
			return &g_formatDescTable[i];
		}
	}
	return (const GLMTexFormatDesc *)NULL;	// not found
}

//===============================================================================


void	InsertTexelComponentFixed( float value, int width, unsigned long *valuebuf )
{
	unsigned long	range = (1<<width);
	unsigned long	scaled = (value * (float) range) * (range-1) / (range);
	
	if (scaled >= range)	DebuggerBreak();
	
	*valuebuf = (*valuebuf << width) | scaled;	
}

// return true if successful
bool	GLMGenTexels( GLMGenTexelParams *params )
{
	unsigned char chunkbuf[256];	// can't think of any chunk this big..
		
	const GLMTexFormatDesc *format = GetFormatDesc( params->m_format );

	if (!format)
	{
		return FALSE;	// fail
	}
	
	// this section just generates one square chunk in the desired format
	unsigned long *temp32 = (unsigned long*)chunkbuf;
	unsigned int chunksize = 0;		// we can sanity check against the format table with this
	
	switch( params->m_format )
	{
		// comment shows byte order in RAM
		// lowercase is bit arrangement in a byte
		
		case D3DFMT_A8R8G8B8:	// B G R A
			InsertTexelComponentFixed( params->a, 8, temp32 );	// A is inserted first and winds up at most significant bits after insertions follow
			InsertTexelComponentFixed( params->r, 8, temp32 );
			InsertTexelComponentFixed( params->g, 8, temp32 );
			InsertTexelComponentFixed( params->b, 8, temp32 );
			chunksize = 4;
		break;
		
		case D3DFMT_A4R4G4B4:	// [ggggbbbb] [aaaarrrr] RA	 (nibbles)
			InsertTexelComponentFixed( params->a, 4, temp32 );
			InsertTexelComponentFixed( params->r, 4, temp32 );
			InsertTexelComponentFixed( params->g, 4, temp32 );
			InsertTexelComponentFixed( params->b, 4, temp32 );
			chunksize = 2;
		break;		

		case D3DFMT_X8R8G8B8:	// B G R X
			InsertTexelComponentFixed( 0.0, 8, temp32 );
			InsertTexelComponentFixed( params->r, 8, temp32 );
			InsertTexelComponentFixed( params->g, 8, temp32 );
			InsertTexelComponentFixed( params->b, 8, temp32 );
			chunksize = 4;
		break;
		
		case D3DFMT_X1R5G5B5:	// [gggbbbbb] [xrrrrrgg]
			InsertTexelComponentFixed( 0.0, 1, temp32 );
			InsertTexelComponentFixed( params->r, 5, temp32 );
			InsertTexelComponentFixed( params->g, 5, temp32 );
			InsertTexelComponentFixed( params->b, 5, temp32 );
			chunksize = 2;
		break;

		case D3DFMT_A1R5G5B5:	// [gggbbbbb] [arrrrrgg]
			InsertTexelComponentFixed( params->a, 1, temp32 );
			InsertTexelComponentFixed( params->r, 5, temp32 );
			InsertTexelComponentFixed( params->g, 5, temp32 );
			InsertTexelComponentFixed( params->b, 5, temp32 );
			chunksize = 2;
		break;

		case D3DFMT_L8:			// L							// caller, use R for L
			InsertTexelComponentFixed( params->r, 8, temp32 );
			chunksize = 1;
		break;
		
		case D3DFMT_A8L8:		// L A							// caller, use R for L and A for A
			InsertTexelComponentFixed( params->a, 8, temp32 );
			InsertTexelComponentFixed( params->r, 8, temp32 );
			chunksize = 2;
		break;
		
		case D3DFMT_R8G8B8:		// B G R
			InsertTexelComponentFixed( params->r, 8, temp32 );
			InsertTexelComponentFixed( params->g, 8, temp32 );
			InsertTexelComponentFixed( params->b, 8, temp32 );
			chunksize = 3;
		break;

		case D3DFMT_A8:			// A
			InsertTexelComponentFixed( params->a, 8, temp32 );
			chunksize = 1;
		break;
		
		case D3DFMT_R5G6B5:		// [gggbbbbb] [rrrrrggg]
			InsertTexelComponentFixed( params->r, 5, temp32 );
			InsertTexelComponentFixed( params->g, 6, temp32 );
			InsertTexelComponentFixed( params->b, 5, temp32 );
			chunksize = 2;
		break;

		case D3DFMT_DXT1:
		{
			memset( temp32, 0, 8 );		// zap 8 bytes
			
			// two 565 RGB words followed by 32 bits of 2-bit interp values for a 4x4 block
			// we write the same color to both slots and all zeroes for the mask (one color total)
			
			unsigned long dxt1_color = 0;
			
			// generate one such word and clone it
			InsertTexelComponentFixed( params->r, 5, &dxt1_color );
			InsertTexelComponentFixed( params->g, 6, &dxt1_color );
			InsertTexelComponentFixed( params->b, 5, &dxt1_color );
			
			// dupe
			dxt1_color = dxt1_color | (dxt1_color<<16);
			
			// write into chunkbuf
			*(unsigned long*)&chunkbuf[0] = dxt1_color;
			
			// color mask bits after that are already set to all zeroes.  chunk is done.
			chunksize = 8;
		}
		break;
		
		case D3DFMT_DXT3:
		{
			memset( temp32, 0, 16 );		// zap 16 bytes
			
			// eight bytes of alpha (16 4-bit alpha nibbles)
			// followed by a DXT1 block
			
			unsigned long dxt3_alpha = 0;
			for( int i=0; i<8; i++)
			{
				// splat same alpha through block
				InsertTexelComponentFixed( params->a, 4, &dxt3_alpha );
			}

			unsigned long dxt3_color = 0;
			
			// generate one such word and clone it
			InsertTexelComponentFixed( params->r, 5, &dxt3_color );
			InsertTexelComponentFixed( params->g, 6, &dxt3_color );
			InsertTexelComponentFixed( params->b, 5, &dxt3_color );
			
			// dupe
			dxt3_color = dxt3_color | (dxt3_color<<16);
			
			// write into chunkbuf
			*(unsigned long*)&chunkbuf[0] = dxt3_alpha;
			*(unsigned long*)&chunkbuf[4] = dxt3_alpha;
			*(unsigned long*)&chunkbuf[8] = dxt3_color;
			*(unsigned long*)&chunkbuf[12] = dxt3_color;

			chunksize = 16;
		}			
		break;

		case D3DFMT_DXT5:
		{
			memset( temp32, 0, 16 );		// zap 16 bytes
			
			// DXT5 has 8 bytes of compressed alpha, then 8 bytes of compressed RGB like DXT1.
			
			// the 8 alpha bytes are 2 bytes of endpoint alpha values, then 16x3 bits of interpolants.
			// so to write a single alpha value, just figure out the value, store it in both the first two bytes then store zeroes.
			
			InsertTexelComponentFixed( params->a, 8, (unsigned long*)&chunkbuf[0] );
			InsertTexelComponentFixed( params->a, 8, (unsigned long*)&chunkbuf[0] );
			// rest of the alpha mask was already zeroed.
			
			// now do colors
			unsigned long dxt5_color = 0;
			
			// generate one such word and clone it
			InsertTexelComponentFixed( params->r, 5, &dxt5_color );
			InsertTexelComponentFixed( params->g, 6, &dxt5_color );
			InsertTexelComponentFixed( params->b, 5, &dxt5_color );
			
			// dupe
			dxt5_color = dxt5_color | (dxt5_color<<16);
			
			// write into chunkbuf
			*(unsigned long*)&chunkbuf[8] = dxt5_color;
			*(unsigned long*)&chunkbuf[12] = dxt5_color;

			chunksize = 16;
		}
		break;


		case D3DFMT_A32B32G32R32F:		
		{
			*(float*)&chunkbuf[0] = params->r;
			*(float*)&chunkbuf[4] = params->g;
			*(float*)&chunkbuf[8] = params->b;
			*(float*)&chunkbuf[12] = params->a;
			
			chunksize = 16;
		}
		break;

		case D3DFMT_A16B16G16R16:
			memset( chunkbuf, 0, 8 );
			// R and G wind up in the first 32 bits
			// B and A wind up in the second 32 bits
			
			InsertTexelComponentFixed( params->a, 16, (unsigned long*)&chunkbuf[4] );	// winds up as MSW of second word (note [4]) - thus last in RAM
			InsertTexelComponentFixed( params->b, 16, (unsigned long*)&chunkbuf[4] );

			InsertTexelComponentFixed( params->g, 16, (unsigned long*)&chunkbuf[0] );
			InsertTexelComponentFixed( params->r, 16, (unsigned long*)&chunkbuf[0] );	// winds up as LSW of first word, thus first in RAM
			
			chunksize = 8;
		break;
		
		// not done yet		
		

		//case D3DFMT_D16:				
		//case D3DFMT_D24X8:			
		//case D3DFMT_D24S8:			

		//case D3DFMT_A16B16G16R16F:	
		
		default:
			return FALSE;	// fail
		break;
	}
	
	// once the chunk buffer is filled..
	
	// sanity check the reported chunk size.
	if (static_cast<int>(chunksize) != format->m_bytesPerSquareChunk)
	{
		DebuggerBreak();
		return FALSE;
	}
	
	// verify that the amount you want to write will not exceed the limit byte count
	unsigned long destByteCount = chunksize * params->m_chunkCount;
	
	if (static_cast<int>(destByteCount) > params->m_byteCountLimit)
	{
		DebuggerBreak();
		return FALSE;
	}
	
	// write the bytes.
	unsigned char *destP = (unsigned char*)params->m_dest;
	for( int chunk=0; chunk < params->m_chunkCount; chunk++)
	{
		for( uint byteindex = 0; byteindex < chunksize; byteindex++)
		{
			*destP++ = chunkbuf[byteindex];
		}
	}
	params->m_bytesWritten = destP - (unsigned char*)params->m_dest;
	
	return TRUE;
}


//===============================================================================
bool LessFunc_GLMTexLayoutKey( const GLMTexLayoutKey &a, const GLMTexLayoutKey &b )
{
	#define	DO_LESS(fff) if (a.fff != b.fff) { return (a.fff< b.fff); }

	DO_LESS(m_texGLTarget);
	DO_LESS(m_texFormat);
	DO_LESS(m_texFlags);
	DO_LESS(m_texSamples);
	DO_LESS(m_xSize);
	DO_LESS(m_ySize)
	DO_LESS(m_zSize);
		
	#undef DO_LESS
	
	return false;	// they are equal
}

CGLMTexLayoutTable::CGLMTexLayoutTable()
{
	m_layoutMap.SetLessFunc( LessFunc_GLMTexLayoutKey );
}

GLMTexLayout *CGLMTexLayoutTable::NewLayoutRef( GLMTexLayoutKey *pDesiredKey )
{
	GLMTexLayoutKey tempKey;
	GLMTexLayoutKey *key = pDesiredKey;

	// look up 'key' in the map and see if it's a hit, if so, bump the refcount and return
	// if not, generate a completed layout based on the key, add to map, set refcount to 1, return that
	
	const GLMTexFormatDesc	*formatDesc = GetFormatDesc( key->m_texFormat );

	//bool					compression = (formatDesc->m_chunkSize > 1) != 0;
	if (!formatDesc)
	{
		GLMStop();	// bad news
	}

	if ( gGL->m_bHave_GL_EXT_texture_sRGB_decode )
	{
		if ( ( formatDesc->m_glIntFormatSRGB != 0 ) && ( ( key->m_texFlags & kGLMTexSRGB ) == 0 ) )
		{
			tempKey = *pDesiredKey;
			key = &tempKey;

			// Slam on SRGB texture flag, and we'll use GL_EXT_texture_sRGB_decode to selectively turn it off in the samplers
			key->m_texFlags |= kGLMTexSRGB;
		}
	}

	unsigned short index = m_layoutMap.Find( *key );
	if (index != m_layoutMap.InvalidIndex())
	{
		// found it
		//printf(" -hit- ");
		GLMTexLayout *layout = m_layoutMap[ index ];
		
		// bump ref count
		layout->m_refCount ++;
		
		return layout;
	}
	else
	{
		//printf(" -miss- ");
		// need to make a new one
		// to allocate it, we need to know how big to make it (slice count)
		
		// figure out how many mip levels are in play
		int mipCount = 1;
		if (key->m_texFlags & kGLMTexMipped)
		{
			int largestAxis = key->m_xSize;
			
			if (key->m_ySize > largestAxis)
				largestAxis = key->m_ySize;
				
			if (key->m_zSize > largestAxis)
				largestAxis = key->m_zSize;
			
			mipCount = 0;
			while( largestAxis > 0 )
			{
				mipCount ++;
				largestAxis >>= 1;
			}
		}

		int faceCount = 1;
		if (key->m_texGLTarget == GL_TEXTURE_CUBE_MAP)
		{
			faceCount = 6;
		}
		
		int sliceCount = mipCount * faceCount;
		
		if (key->m_texFlags & kGLMTexMultisampled)
		{
			Assert( (key->m_texGLTarget == GL_TEXTURE_2D) );
			Assert( sliceCount == 1 );
			
			// assume non mipped
			Assert( (key->m_texFlags & kGLMTexMipped) == 0 );
			Assert( (key->m_texFlags & kGLMTexMippedAuto) == 0 );
			
			// assume renderable and srgb
			Assert( (key->m_texFlags & kGLMTexRenderable) !=0 );
			//Assert( (key->m_texFlags & kGLMTexSRGB) !=0 );			//FIXME don't assert on making depthstencil surfaces which are non srgb
			
			// double check sample count (FIXME need real limit check here against device/driver)
			Assert( (key->m_texSamples==2) || (key->m_texSamples==4) || (key->m_texSamples==6) || (key->m_texSamples==8) );
		}
		
		// now we know enough to allocate and populate the new tex layout.
		
		// malloc the new layout
		int layoutSize = sizeof( GLMTexLayout ) + (sliceCount * sizeof( GLMTexLayoutSlice ));
		GLMTexLayout *layout = (GLMTexLayout *)malloc( layoutSize );
		memset( layout, 0, layoutSize );
		
		// clone the key in there
		memset( &layout->m_key, 0x00, sizeof(layout->m_key) );
		layout->m_key = *key;

		// set refcount
		layout->m_refCount = 1;
		
		// save the format desc
		layout->m_format = (GLMTexFormatDesc *)formatDesc;
		
		// we know the mipcount from before
		layout->m_mipCount = mipCount;
		
		// we know the face count too
		layout->m_faceCount = faceCount;
		
		// slice count is the product
		layout->m_sliceCount = mipCount * faceCount;
		
		// we can now fill in the slices.
		GLMTexLayoutSlice	*slicePtr = &layout->m_slices[0];
		int					storageOffset = 0;
		
		//bool compressed = (formatDesc->m_chunkSize > 1);	// true if DXT
		
		for( int mip = 0; mip < mipCount; mip ++ )
		{
			for( int face = 0; face < faceCount; face++ )
			{
				// note application of chunk size which is 1 for uncompressed, and 4 for compressed tex (DXT)
				// note also that the *dimensions* must scale down to 1
				// but that the *storage* cannot go below 4x4.
				// we introduce the "storage sizes" which are clamped, to compute the storage footprint.
				
				int storage_x,storage_y,storage_z;
				
				slicePtr->m_xSize = layout->m_key.m_xSize >> mip;
				slicePtr->m_xSize = MAX( slicePtr->m_xSize, 1 );				// dimension can't go to zero
				storage_x = MAX( slicePtr->m_xSize, formatDesc->m_chunkSize );	// storage extent can't go below chunk size
				
				slicePtr->m_ySize = layout->m_key.m_ySize >> mip;
				slicePtr->m_ySize = MAX( slicePtr->m_ySize, 1 );				// dimension can't go to zero
				storage_y = MAX( slicePtr->m_ySize, formatDesc->m_chunkSize );	// storage extent can't go below chunk size
				
				slicePtr->m_zSize = layout->m_key.m_zSize >> mip;
				slicePtr->m_zSize = MAX( slicePtr->m_zSize, 1 );				// dimension can't go to zero
				storage_z = MAX( slicePtr->m_zSize, 1);							// storage extent for Z cannot go below '1'.
				
				//if (compressed)  NO NO NO do not lie about the dimensionality, just fudge the storage.
				//{
				//	// round up to multiple of 4 in X and Y axes
				//	slicePtr->m_xSize = (slicePtr->m_xSize+3) & (~3);
				//	slicePtr->m_ySize = (slicePtr->m_ySize+3) & (~3);
				//}
				
				int xchunks = (storage_x / formatDesc->m_chunkSize );
				int ychunks = (storage_y / formatDesc->m_chunkSize );
				
				slicePtr->m_storageSize = (xchunks * ychunks * formatDesc->m_bytesPerSquareChunk) * storage_z;				
				slicePtr->m_storageOffset = storageOffset;
				
				storageOffset += slicePtr->m_storageSize;
				storageOffset = ( (storageOffset+0x0F) & (~0x0F));		// keep each MIP starting on a 16 byte boundary.
				
				slicePtr++;
			}		
		}
		
		layout->m_storageTotalSize = storageOffset;
		//printf("\n size %08x for key (x=%d y=%d z=%d, fmt=%08x, bpsc=%d)", layout->m_storageTotalSize, key->m_xSize, key->m_ySize, key->m_zSize, key->m_texFormat, formatDesc->m_bytesPerSquareChunk );
		
		// generate summary
		// "target, format, +/- mips, base size"
		char scratch[1024];

		char	*targetname = "?";
		switch( key->m_texGLTarget )
		{
			case GL_TEXTURE_2D:			targetname = "2D  ";		break;
			case GL_TEXTURE_3D:			targetname = "3D  ";		break;
			case GL_TEXTURE_CUBE_MAP:	targetname = "CUBE";		break;
		}
		
		sprintf( scratch, "[%s %s %dx%dx%d mips=%d slices=%d flags=%02lX%s]",
					targetname,
					formatDesc->m_formatSummary,
					layout->m_key.m_xSize, layout->m_key.m_ySize, layout->m_key.m_zSize,
					mipCount,
					sliceCount,
					layout->m_key.m_texFlags,
					(layout->m_key.m_texFlags & kGLMTexSRGB) ? " SRGB" : ""					
				);
		layout->m_layoutSummary = strdup( scratch );
		//GLMPRINTF(("-D- new tex layout [ %s ]", scratch ));
		
		// then insert into map. disregard returned index.
		m_layoutMap.Insert( layout->m_key, layout );
		
		return layout;
	}
}

void CGLMTexLayoutTable::DelLayoutRef( GLMTexLayout *layout )
{
	// locate layout in hash, drop refcount
	// (some GC step later on will harvest expired layouts - not like it's any big challenge to re-generate them)
	
	unsigned short index = m_layoutMap.Find( layout->m_key );
	if (index != m_layoutMap.InvalidIndex())
	{
		// found it
		GLMTexLayout *layout = m_layoutMap[ index ];
		
		// drop ref count
		layout->m_refCount --;
		
		//assert( layout->m_refCount >= 0 );
	}
	else
	{
		// that's bad
		GLMStop();
	}
}

void CGLMTexLayoutTable::DumpStats( )
{
	for (uint i=0; i<m_layoutMap.Count(); i++ )
	{
		GLMTexLayout *layout = m_layoutMap[ i ];
		
		// print it out
		printf("\n%05d instances %08d bytes  %08d totbytes  %s", layout->m_refCount, layout->m_storageTotalSize, (layout->m_refCount*layout->m_storageTotalSize), layout->m_layoutSummary );
	}
}

ConVar gl_texmsaalog ( "gl_texmsaalog", "0");

ConVar gl_rt_forcergba ( "gl_rt_forcergba", "1" );	// on teximage of a renderable tex, pass GL_RGBA in place of GL_BGRA

ConVar gl_minimize_rt_tex ( "gl_minimize_rt_tex", "0" );	// if 1, set the GL_TEXTURE_MINIMIZE_STORAGE_APPLE texture parameter to cut off mipmaps for RT's
ConVar gl_minimize_all_tex ( "gl_minimize_all_tex", "1" );	// if 1, set the GL_TEXTURE_MINIMIZE_STORAGE_APPLE texture parameter to cut off mipmaps for textures which are unmipped
ConVar gl_minimize_tex_log ( "gl_minimize_tex_log", "0" );	// if 1, printf the names of the tex that got minimized

CGLMTex::CGLMTex( GLMContext *ctx, GLMTexLayout *layout, uint levels, const char *debugLabel )
{
#if GLMDEBUG
	m_pPrevTex = NULL;
	m_pNextTex = g_pFirstCGMLTex;
	if ( m_pNextTex )
	{
		Assert( m_pNextTex->m_pPrevTex == NULL );
		m_pNextTex->m_pPrevTex = this;
	}
	g_pFirstCGMLTex = this;
#endif

	// caller has responsibility to make 'ctx' current, but we check to be sure.
	ctx->CheckCurrent();
						
	m_nLastResolvedBatchCounter = ctx->m_nBatchCounter;
		
	// note layout requested
	m_layout = layout;
	m_texGLTarget = m_layout->m_key.m_texGLTarget;
	
	m_nSamplerType = SAMPLER_TYPE_UNUSED;
	switch ( m_texGLTarget )
	{
		case GL_TEXTURE_CUBE_MAP:	m_nSamplerType = SAMPLER_TYPE_CUBE; break;
		case GL_TEXTURE_2D:			m_nSamplerType = SAMPLER_TYPE_2D; break;
		case GL_TEXTURE_3D:			m_nSamplerType = SAMPLER_TYPE_3D; break;
		default:
			Assert( 0 );
			break;
	}

	m_maxActiveMip = -1;	//index of highest mip that has been written - increase as each mip arrives
	m_minActiveMip = 999;	//index of lowest mip that has been written - lower it as each mip arrives
	
	// note context owner
	m_ctx = ctx;
	
	// clear the bind point flags
	//m_bindPoints.ClearAll();
	
	// clear the RT attach count
	m_rtAttachCount = 0;
	
	// come up with a GL name for this texture.
	m_texName = ctx->CreateTex( m_texGLTarget, m_layout->m_format->m_glIntFormat );

	m_pBlitSrcFBO = NULL;
	m_pBlitDstFBO = NULL;

	// Sense whether to try and apply client storage upon teximage/subimage.
	//  This should only be true if we're running on OSX 10.6 or it was explicitly
	//  enabled with -gl_texclientstorage on the command line.
	m_texClientStorage = ctx->m_bTexClientStorage;
	
	// flag that we have not yet been explicitly kicked into VRAM..
	m_texPreloaded = false;
	
	// clone the debug label if there is one.
	m_debugLabel = debugLabel ? strdup(debugLabel) : NULL;

	// if tex is MSAA renderable, make an RBO, else zero the RBO name and dirty bit
	if (layout->m_key.m_texFlags & kGLMTexMultisampled)
	{
		gGL->glGenRenderbuffers( 1, &m_rboName );
				
		// so we have enough info to go ahead and bind the RBO and put storage on it?
		// try it.
		gGL->glBindRenderbuffer( GL_RENDERBUFFER, m_rboName );

		// quietly clamp if sample count exceeds known limit for the device
		int sampleCount = layout->m_key.m_texSamples;
		
		if (sampleCount > ctx->Caps().m_maxSamples)
		{
			sampleCount = ctx->Caps().m_maxSamples;	// clamp
		}
		
		GLenum	msaaFormat = (layout->m_key.m_texFlags & kGLMTexSRGB) ? layout->m_format->m_glIntFormatSRGB : layout->m_format->m_glIntFormat;
		gGL->glRenderbufferStorageMultisample(	GL_RENDERBUFFER,
												sampleCount,	// not "layout->m_key.m_texSamples"
												msaaFormat,
												layout->m_key.m_xSize,
												layout->m_key.m_ySize );	

		if (gl_texmsaalog.GetInt())
		{
			printf( "\n == MSAA Tex %p %s : MSAA RBO is intformat %s (%x)", this, m_debugLabel?m_debugLabel:"", GLMDecode( eGL_ENUM, msaaFormat ), msaaFormat );
		}

		gGL->glBindRenderbuffer( GL_RENDERBUFFER, 0 );
	}
	else
	{
		m_rboName = 0;
	}
		
	// at this point we have the complete description of the texture, and a name for it, but no data and no actual GL object.
	// we know this name has bever seen duty before, so we're going to hard-bind it to TMU 0, displacing any other tex that might have been bound there.
	// any previously bound tex will be unbound and appropriately marked as a result.
	// the active TMU will be set as a side effect.
	CGLMTex *pPrevTex = ctx->m_samplers[0].m_pBoundTex;
	ctx->BindTexToTMU( this, 0 );

	m_SamplingParams.SetToDefaults();
	m_SamplingParams.SetToTarget( m_texGLTarget );
	
	// OK, our texture now exists and is bound on the active TMU.  Not drawable yet though.
		
	// Create backing storage and fill it
	if ( !(layout->m_key.m_texFlags & kGLMTexRenderable) && m_texClientStorage )
	{
		m_backing = (char *)malloc( m_layout->m_storageTotalSize );
		memset( m_backing, 0, m_layout->m_storageTotalSize );
		
		// track bytes allocated for non-RT's
		int formindex = sEncodeLayoutAsIndex( &layout->m_key );
		
		g_texGlobalBytes[ formindex ] += m_layout->m_storageTotalSize;
		
		#if TEXSPACE_LOGGING
			printf( "\n Tex %s added %d bytes in form %d which is now %d bytes", m_debugLabel ? m_debugLabel : "-", m_layout->m_storageTotalSize, formindex, g_texGlobalBytes[ formindex ] );
			printf( "\n\t\t[ %d %d %d %d  %d %d %d %d ]",
				   g_texGlobalBytes[ 0 ],g_texGlobalBytes[ 1 ],g_texGlobalBytes[ 2 ],g_texGlobalBytes[ 3 ],
				   g_texGlobalBytes[ 4 ],g_texGlobalBytes[ 5 ],g_texGlobalBytes[ 6 ],g_texGlobalBytes[ 7 ]
				   );
		#endif
	}
	else
	{
		m_backing = NULL;
		
		m_texClientStorage = false;
	}		

	// init lock count
	// lock reqs are tracked by the owning context
	m_lockCount = 0;

	m_sliceFlags.SetCount( m_layout->m_sliceCount );
	for( int i=0; i< m_layout->m_sliceCount; i++)
	{
		m_sliceFlags[i] = 0;
			// kSliceValid			=	false	(we have not teximaged each slice yet)
			// kSliceStorageValid	=	false	(the storage allocated does not reflect what is in the tex)
			// kSliceLocked			=	false	(the slices are not locked)
			// kSliceFullyDirty		=	false	(this does not come true til first lock)
	}
	
	// texture minimize parameter keeps driver from allocing mips when it should not, by being explicit about the ones that have no mips.
	
	bool setMinimizeParameter = false;
	bool minimize_rt = (gl_minimize_rt_tex.GetInt()!=0);
	bool minimize_all = (gl_minimize_all_tex.GetInt()!=0);
	
	if (layout->m_key.m_texFlags & kGLMTexRenderable)
	{
		// it's an RT.  if mips were not explicitly requested, and "gl_minimize_rt_tex" is true, set the minimize parameter.
		if (  (minimize_rt || minimize_all) && ( !(layout->m_key.m_texFlags & kGLMTexMipped) ) )
		{
			setMinimizeParameter = true;
		}
	}
	else
	{
		// not an RT. if mips were not requested, and "gl_minimize_all_tex" is true, set the minimize parameter.
		if ( minimize_all && ( !(layout->m_key.m_texFlags & kGLMTexMipped) ) )
		{
			setMinimizeParameter = true;
		}
	}

	if (setMinimizeParameter)
	{
		if (gl_minimize_tex_log.GetInt())
		{
			printf("\n minimizing storage for tex '%s' [%s] ", m_debugLabel?m_debugLabel:"-", m_layout->m_layoutSummary );
		}
	}

	// after a lot of pain with texture completeness...
	// always push black into all slices of all newly created textures.
	
	#if 0
		bool pushRenderableSlices = (m_layout->m_key.m_texFlags & kGLMTexRenderable) != 0;
		bool pushTexSlices = true;	// just do it everywhere  (m_layout->m_mipCount>1) && (m_layout->m_format->m_chunkSize !=1) ;
		if (pushTexSlices)
		{
			// fill storage with mostly-opaque purple
			
			GLMGenTexelParams genp;
			memset( &genp, 0, sizeof(genp) );
			
			genp.m_format = m_layout->m_format->m_d3dFormat;
			const GLMTexFormatDesc *format = GetFormatDesc( genp.m_format );
			
			genp.m_dest				= m_backing;		// dest addr
			genp.m_chunkCount		= m_layout->m_storageTotalSize / format->m_bytesPerSquareChunk; // fill the whole slab
			genp.m_byteCountLimit	= m_layout->m_storageTotalSize;	// limit writes to this amount

			genp.r = 1.0;
			genp.g = 0.0;
			genp.b = 1.0;
			genp.a = 0.75;
			
			GLMGenTexels( &genp );
		}
	#endif
	
	//if (pushRenderableSlices || pushTexSlices)
	if ( !( ( layout->m_key.m_texFlags & kGLMTexMipped ) && ( levels == ( unsigned ) m_layout->m_mipCount ) ) )
	{
		for( int face=0; face <m_layout->m_faceCount; face++)
		{
			for( int mip=0; mip <m_layout->m_mipCount; mip++)
			{
				// we're not really going to lock, we're just going to write the blank data from the backing store we just made
				GLMTexLockDesc	desc;
				
				desc.m_req.m_tex = this;
				desc.m_req.m_face = face;
				desc.m_req.m_mip = mip;

				desc.m_sliceIndex = CalcSliceIndex( face, mip );

				GLMTexLayoutSlice *slice = &m_layout->m_slices[ desc.m_sliceIndex ];
				
				desc.m_req.m_region.xmin = desc.m_req.m_region.ymin = desc.m_req.m_region.zmin = 0;
				desc.m_req.m_region.xmax = slice->m_xSize;
				desc.m_req.m_region.ymax = slice->m_ySize;
				desc.m_req.m_region.zmax = slice->m_zSize;

				desc.m_sliceBaseOffset = slice->m_storageOffset;	// doesn't really matter... we're just pushing zeroes..
				desc.m_sliceRegionOffset = 0;

				WriteTexels( &desc, true, (layout->m_key.m_texFlags & kGLMTexRenderable)!=0 );					// write whole slice - but disable data source if it's an RT, as there's no backing
			}
		}
	}
	GLMPRINTF(("-A- -**TEXNEW '%-60s' name=%06d  size=%09d  storage=%08x label=%s ", m_layout->m_layoutSummary, m_texName, m_layout->m_storageTotalSize, m_backing, m_debugLabel ? m_debugLabel : "-" ));

	ctx->BindTexToTMU( pPrevTex, 0 );
}

CGLMTex::~CGLMTex( )
{
#if GLMDEBUG
	if ( m_pPrevTex )
	{
		Assert( m_pPrevTex->m_pNextTex == this );
		m_pPrevTex->m_pNextTex = m_pNextTex;
	}
	else
	{
		Assert( g_pFirstCGMLTex == this );
		g_pFirstCGMLTex = m_pNextTex;
	}
	if ( m_pNextTex )
	{
		Assert( m_pNextTex->m_pPrevTex == this );
		m_pNextTex->m_pPrevTex = m_pPrevTex;
	}
	m_pNextTex = m_pPrevTex = NULL;
#endif

	if ( !(m_layout->m_key.m_texFlags & kGLMTexRenderable) )
	{
		int formindex = sEncodeLayoutAsIndex( &m_layout->m_key );

		g_texGlobalBytes[ formindex ] -= m_layout->m_storageTotalSize;
		
		#if TEXSPACE_LOGGING
			printf( "\n Tex %s freed %d bytes in form %d which is now %d bytes", m_debugLabel ? m_debugLabel : "-", m_layout->m_storageTotalSize, formindex, g_texGlobalBytes[ formindex ] );
			printf( "\n\t\t[ %d %d %d %d  %d %d %d %d ]",
				   g_texGlobalBytes[ 0 ],g_texGlobalBytes[ 1 ],g_texGlobalBytes[ 2 ],g_texGlobalBytes[ 3 ],
				   g_texGlobalBytes[ 4 ],g_texGlobalBytes[ 5 ],g_texGlobalBytes[ 6 ],g_texGlobalBytes[ 7 ]
				   );			   		
		#endif
	}
	
	GLMPRINTF(("-A- -**TEXDEL '%-60s' name=%06d  size=%09d  storage=%08x label=%s ", m_layout->m_layoutSummary, m_texName, m_layout->m_storageTotalSize, m_backing, m_debugLabel ? m_debugLabel : "-" ));
	// check first to see if we were still bound anywhere or locked... these should be failures.
	
	if ( m_pBlitSrcFBO )
	{
		m_ctx->DelFBO( m_pBlitSrcFBO );
		m_pBlitSrcFBO = NULL;
	}

	if ( m_pBlitDstFBO )
	{
		m_ctx->DelFBO( m_pBlitDstFBO );
		m_pBlitDstFBO = NULL;
	}

	if ( m_rboName )
	{
		gGL->glDeleteRenderbuffers( 1, &m_rboName );
		m_rboName = 0;
	}

	// if all that is OK, then delete the underlying tex
	if ( m_texName )
	{
		m_ctx->DestroyTex( m_texGLTarget, m_layout, m_texName );
		m_texName = 0;
	}
		
	// release our usage of the layout
	m_ctx->m_texLayoutTable->DelLayoutRef( m_layout );
	m_layout = NULL;
	
	if (m_backing)
	{
		free( m_backing );
		m_backing = NULL;
	}
	
	if (m_debugLabel)
	{
		free( m_debugLabel );
		m_debugLabel = NULL;
	}
	
	m_ctx = NULL;
}

int	CGLMTex::CalcSliceIndex( int face, int mip )
{
	// faces of the same mip level are adjacent. "face major" storage
	int index = (mip * m_layout->m_faceCount) + face;

	return index;
}

void CGLMTex::CalcTexelDataOffsetAndStrides( int sliceIndex, int x, int y, int z, int *offsetOut, int *yStrideOut, int *zStrideOut )
{
	int offset = 0;
	int yStride = 0;
	int zStride = 0;
	
	GLMTexFormatDesc *format = m_layout->m_format;
	if (format->m_chunkSize==1)	
	{
		// figure out row stride and layer stride
		yStride = format->m_bytesPerSquareChunk * m_layout->m_slices[sliceIndex].m_xSize;	// bytes per texel row (y stride)
		zStride = yStride * m_layout->m_slices[sliceIndex].m_ySize;							// bytes per texel layer (if 3D tex)
		
		offset = x * format->m_bytesPerSquareChunk;		// lateral offset
		offset += (y * yStride);							// scanline offset
		offset += (z * zStride);							// should be zero for 2D tex
	}
	else
	{
		yStride = format->m_bytesPerSquareChunk * (m_layout->m_slices[sliceIndex].m_xSize / format->m_chunkSize);
		zStride = yStride * (m_layout->m_slices[sliceIndex].m_ySize / format->m_chunkSize);
		
		// compressed format.  scale the x,y,z values into chunks.
		// assert if any of them are not multiples of a chunk.
		int chunkx = x / format->m_chunkSize;
		int chunky = y / format->m_chunkSize;
		int chunkz = z / format->m_chunkSize;
		
		if ( (chunkx * format->m_chunkSize) != x)
		{
			GLMStop();
		}
		
		if ( (chunky * format->m_chunkSize) != y)
		{
			GLMStop();
		}
		
		if ( (chunkz * format->m_chunkSize) != z)
		{
			GLMStop();
		}
		
		offset = chunkx * format->m_bytesPerSquareChunk;	// lateral offset
		offset += (chunky * yStride);						// chunk row offset
		offset += (chunkz * zStride);						// should be zero for 2D tex		
	}
	
	*offsetOut	= offset;
	*yStrideOut	= yStride;
	*zStrideOut	= zStride;
}

extern void convert_texture( GLenum &internalformat, GLsizei width, GLsizei height, GLenum &format, GLenum &type, void *data );

void CGLMTex::ReadTexels( GLMTexLockDesc *desc, bool readWholeSlice )
{
	GLMRegion	readBox;
	
	if (readWholeSlice)
	{
		readBox.xmin = readBox.ymin = readBox.zmin = 0;

		readBox.xmax = m_layout->m_slices[ desc->m_sliceIndex ].m_xSize;
		readBox.ymax = m_layout->m_slices[ desc->m_sliceIndex ].m_ySize;
		readBox.zmax = m_layout->m_slices[ desc->m_sliceIndex ].m_zSize;
	}
	else
	{
		readBox = desc->m_req.m_region;
	}

	CGLMTex *pPrevTex = m_ctx->m_samplers[0].m_pBoundTex;
	m_ctx->BindTexToTMU( this, 0 );		// SelectTMU(n) is a side effect

	if (readWholeSlice)
	{
		// make this work first.... then write the partial path
		// (Hmmmm, I don't think we will ever actually need a partial path - 
		// since we have no notion of a partially valid slice of storage

		GLMTexFormatDesc *format = m_layout->m_format;
		GLenum target = m_layout->m_key.m_texGLTarget;

		void *sliceAddress = m_backing + m_layout->m_slices[ desc->m_sliceIndex ].m_storageOffset;	// this would change for PBO
		//int sliceSize = m_layout->m_slices[ desc->m_sliceIndex ].m_storageSize;

		// interestingly enough, we can use the same path for both 2D and 3D fetch

		switch( target )
		{
			case GL_TEXTURE_CUBE_MAP:

				// adjust target to steer to the proper face, then fall through to the 2D texture path.
				target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + desc->m_req.m_face;
				
			case GL_TEXTURE_2D:
			case GL_TEXTURE_3D:
			{
				// check compressed or not
				if (format->m_chunkSize != 1)
				{
					// compressed path
					// http://www.opengl.org/sdk/docs/man/xhtml/glGetCompressedTexImage.xml
					// TODO(nillerusr): implement me!
/*
					gGL->glGetCompressedTexImage(	target,					// target
												desc->m_req.m_mip,		// level
												sliceAddress );			// destination
*/
				}
				else
				{
					// uncompressed path
					// http://www.opengl.org/sdk/docs/man/xhtml/glGetTexImage.xml
					GLuint fbo;
					GLint Rfbo = 0, Dfbo = 0;

					gGL->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &Dfbo );
					gGL->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &Rfbo );

					gGL->glGenFramebuffers(1, &fbo);
					gGL->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
					gGL->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, m_ctx->m_samplers[0].m_pBoundTex->m_texName, 0);

					GLenum fmt = format->m_glDataFormat;
					GLenum dataType = format->m_glDataType;

					convert_texture(fmt, 0, 0, fmt, dataType, NULL);
					gGL->glReadPixels(0, 0, m_layout->m_slices[ desc->m_sliceIndex ].m_xSize, m_layout->m_slices[ desc->m_sliceIndex ].m_ySize, fmt, dataType, sliceAddress);

					gGL->glBindFramebuffer(GL_READ_FRAMEBUFFER, Rfbo);
					gGL->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, Dfbo);

					gGL->glDeleteFramebuffers(1, &fbo);
				}
			}
			break;
		}
	}
	else
	{
		GLMStop();
	}

	m_ctx->BindTexToTMU( pPrevTex, 0 );
}

struct mem_s
{
	const int value;
	const char *str;
} g_glEnums[] =
{
{	0x0000,	"GL_ZERO"	},
{	0x0001,	"GL_ONE"	},
{	0x0004,	"GL_TRIANGLES"	},
{	0x0005,	"GL_TRIANGLE_STRIP"	},
{	0x0006,	"GL_TRIANGLE_FAN"	},
{	0x0007,	"GL_QUADS"	},
{	0x0008,	"GL_QUAD_STRIP"	},
{	0x0009,	"GL_POLYGON"	},
{	0x0200,	"GL_NEVER"	},
{	0x0201,	"GL_LESS"	},
{	0x0202,	"GL_EQUAL"	},
{	0x0203,	"GL_LEQUAL"	},
{	0x0204,	"GL_GREATER"	},
{	0x0205,	"GL_NOTEQUAL"	},
{	0x0206,	"GL_GEQUAL"	},
{	0x0207,	"GL_ALWAYS"	},
{	0x0300,	"GL_SRC_COLOR"	},
{	0x0301,	"GL_ONE_MINUS_SRC_COLOR"	},
{	0x0302,	"GL_SRC_ALPHA"	},
{	0x0303,	"GL_ONE_MINUS_SRC_ALPHA"	},
{	0x0304,	"GL_DST_ALPHA"	},
{	0x0305,	"GL_ONE_MINUS_DST_ALPHA"	},
{	0x0306,	"GL_DST_COLOR"	},
{	0x0307,	"GL_ONE_MINUS_DST_COLOR"	},
{	0x0308,	"GL_SRC_ALPHA_SATURATE"	},
{	0x0400,	"GL_FRONT_LEFT"	},
{	0x0401,	"GL_FRONT_RIGHT"	},
{	0x0402,	"GL_BACK_LEFT"	},
{	0x0403,	"GL_BACK_RIGHT"	},
{	0x0404,	"GL_FRONT"	},
{	0x0405,	"GL_BACK"	},
{	0x0406,	"GL_LEFT"	},
{	0x0407,	"GL_RIGHT"	},
{	0x0408,	"GL_FRONT_AND_BACK"	},
{	0x0409,	"GL_AUX0"	},
{	0x040A,	"GL_AUX1"	},
{	0x040B,	"GL_AUX2"	},
{	0x040C,	"GL_AUX3"	},
{	0x0500,	"GL_INVALID_ENUM"	},
{	0x0501,	"GL_INVALID_VALUE"	},
{	0x0502,	"GL_INVALID_OPERATION"	},
{	0x0503,	"GL_STACK_OVERFLOW"	},
{	0x0504,	"GL_STACK_UNDERFLOW"	},
{	0x0505,	"GL_OUT_OF_MEMORY"	},
{	0x0506,	"GL_INVALID_FRAMEBUFFER_OPERATION"	},
{	0x0600,	"GL_2D"	},
{	0x0601,	"GL_3D"	},
{	0x0602,	"GL_3D_COLOR"	},
{	0x0603,	"GL_3D_COLOR_TEXTURE"	},
{	0x0604,	"GL_4D_COLOR_TEXTURE"	},
{	0x0700,	"GL_PASS_THROUGH_TOKEN"	},
{	0x0701,	"GL_POINT_TOKEN"	},
{	0x0702,	"GL_LINE_TOKEN"	},
{	0x0703,	"GL_POLYGON_TOKEN"	},
{	0x0704,	"GL_BITMAP_TOKEN"	},
{	0x0705,	"GL_DRAW_PIXEL_TOKEN"	},
{	0x0706,	"GL_COPY_PIXEL_TOKEN"	},
{	0x0707,	"GL_LINE_RESET_TOKEN"	},
{	0x0800,	"GL_EXP"	},
{	0x0801,	"GL_EXP2"	},
{	0x0900,	"GL_CW"	},
{	0x0901,	"GL_CCW"	},
{	0x0A00,	"GL_COEFF"	},
{	0x0A01,	"GL_ORDER"	},
{	0x0A02,	"GL_DOMAIN"	},
{	0x0B00,	"GL_CURRENT_COLOR"	},
{	0x0B01,	"GL_CURRENT_INDEX"	},
{	0x0B02,	"GL_CURRENT_NORMAL"	},
{	0x0B03,	"GL_CURRENT_TEXTURE_COORDS"	},
{	0x0B04,	"GL_CURRENT_RASTER_COLOR"	},
{	0x0B05,	"GL_CURRENT_RASTER_INDEX"	},
{	0x0B06,	"GL_CURRENT_RASTER_TEXTURE_COORDS"	},
{	0x0B07,	"GL_CURRENT_RASTER_POSITION"	},
{	0x0B08,	"GL_CURRENT_RASTER_POSITION_VALID"	},
{	0x0B09,	"GL_CURRENT_RASTER_DISTANCE"	},
{	0x0B10,	"GL_POINT_SMOOTH"	},
{	0x0B11,	"GL_POINT_SIZE"	},
{	0x0B12,	"GL_POINT_SIZE_RANGE"	},
{	0x0B12,	"GL_SMOOTH_POINT_SIZE_RANGE"	},
{	0x0B13,	"GL_POINT_SIZE_GRANULARITY"	},
{	0x0B13,	"GL_SMOOTH_POINT_SIZE_GRANULARITY"	},
{	0x0B20,	"GL_LINE_SMOOTH"	},
{	0x0B21,	"GL_LINE_WIDTH"	},
{	0x0B22,	"GL_LINE_WIDTH_RANGE"	},
{	0x0B22,	"GL_SMOOTH_LINE_WIDTH_RANGE"	},
{	0x0B23,	"GL_LINE_WIDTH_GRANULARITY"	},
{	0x0B23,	"GL_SMOOTH_LINE_WIDTH_GRANULARITY"	},
{	0x0B24,	"GL_LINE_STIPPLE"	},
{	0x0B25,	"GL_LINE_STIPPLE_PATTERN"	},
{	0x0B26,	"GL_LINE_STIPPLE_REPEAT"	},
{	0x0B30,	"GL_LIST_MODE"	},
{	0x0B31,	"GL_MAX_LIST_NESTING"	},
{	0x0B32,	"GL_LIST_BASE"	},
{	0x0B33,	"GL_LIST_INDEX"	},
{	0x0B40,	"GL_POLYGON_MODE"	},
{	0x0B41,	"GL_POLYGON_SMOOTH"	},
{	0x0B42,	"GL_POLYGON_STIPPLE"	},
{	0x0B43,	"GL_EDGE_FLAG"	},
{	0x0B44,	"GL_CULL_FACE"	},
{	0x0B45,	"GL_CULL_FACE_MODE"	},
{	0x0B46,	"GL_FRONT_FACE"	},
{	0x0B50,	"GL_LIGHTING"	},
{	0x0B51,	"GL_LIGHT_MODEL_LOCAL_VIEWER"	},
{	0x0B52,	"GL_LIGHT_MODEL_TWO_SIDE"	},
{	0x0B53,	"GL_LIGHT_MODEL_AMBIENT"	},
{	0x0B54,	"GL_SHADE_MODEL"	},
{	0x0B55,	"GL_COLOR_MATERIAL_FACE"	},
{	0x0B56,	"GL_COLOR_MATERIAL_PARAMETER"	},
{	0x0B57,	"GL_COLOR_MATERIAL"	},
{	0x0B60,	"GL_FOG"	},
{	0x0B61,	"GL_FOG_INDEX"	},
{	0x0B62,	"GL_FOG_DENSITY"	},
{	0x0B63,	"GL_FOG_START"	},
{	0x0B64,	"GL_FOG_END"	},
{	0x0B65,	"GL_FOG_MODE"	},
{	0x0B66,	"GL_FOG_COLOR"	},
{	0x0B70,	"GL_DEPTH_RANGE"	},
{	0x0B71,	"GL_DEPTH_TEST"	},
{	0x0B72,	"GL_DEPTH_WRITEMASK"	},
{	0x0B73,	"GL_DEPTH_CLEAR_VALUE"	},
{	0x0B74,	"GL_DEPTH_FUNC"	},
{	0x0B80,	"GL_ACCUM_CLEAR_VALUE"	},
{	0x0B90,	"GL_STENCIL_TEST"	},
{	0x0B91,	"GL_STENCIL_CLEAR_VALUE"	},
{	0x0B92,	"GL_STENCIL_FUNC"	},
{	0x0B93,	"GL_STENCIL_VALUE_MASK"	},
{	0x0B94,	"GL_STENCIL_FAIL"	},
{	0x0B95,	"GL_STENCIL_PASS_DEPTH_FAIL"	},
{	0x0B96,	"GL_STENCIL_PASS_DEPTH_PASS"	},
{	0x0B97,	"GL_STENCIL_REF"	},
{	0x0B98,	"GL_STENCIL_WRITEMASK"	},
{	0x0BA0,	"GL_MATRIX_MODE"	},
{	0x0BA1,	"GL_NORMALIZE"	},
{	0x0BA2,	"GL_VIEWPORT"	},
{	0x0BA3,	"GL_MODELVIEW_STACK_DEPTH"	},
{	0x0BA4,	"GL_PROJECTION_STACK_DEPTH"	},
{	0x0BA5,	"GL_TEXTURE_STACK_DEPTH"	},
{	0x0BA6,	"GL_MODELVIEW_MATRIX"	},
{	0x0BA7,	"GL_PROJECTION_MATRIX"	},
{	0x0BA8,	"GL_TEXTURE_MATRIX"	},
{	0x0BB0,	"GL_ATTRIB_STACK_DEPTH"	},
{	0x0BB1,	"GL_CLIENT_ATTRIB_STACK_DEPTH"	},
{	0x0BC0,	"GL_ALPHA_TEST"	},
{	0x0BC1,	"GL_ALPHA_TEST_FUNC"	},
{	0x0BC2,	"GL_ALPHA_TEST_REF"	},
{	0x0BD0,	"GL_DITHER"	},
{	0x0BE0,	"GL_BLEND_DST"	},
{	0x0BE1,	"GL_BLEND_SRC"	},
{	0x0BE2,	"GL_BLEND"	},
{	0x0BF0,	"GL_LOGIC_OP_MODE"	},
{	0x0BF1,	"GL_INDEX_LOGIC_OP"	},
{	0x0BF2,	"GL_COLOR_LOGIC_OP"	},
{	0x0C00,	"GL_AUX_BUFFERS"	},
{	0x0C01,	"GL_DRAW_BUFFER"	},
{	0x0C02,	"GL_READ_BUFFER"	},
{	0x0C10,	"GL_SCISSOR_BOX"	},
{	0x0C11,	"GL_SCISSOR_TEST"	},
{	0x0C20,	"GL_INDEX_CLEAR_VALUE"	},
{	0x0C21,	"GL_INDEX_WRITEMASK"	},
{	0x0C22,	"GL_COLOR_CLEAR_VALUE"	},
{	0x0C23,	"GL_COLOR_WRITEMASK"	},
{	0x0C30,	"GL_INDEX_MODE"	},
{	0x0C31,	"GL_RGBA_MODE"	},
{	0x0C32,	"GL_DOUBLEBUFFER"	},
{	0x0C33,	"GL_STEREO"	},
{	0x0C40,	"GL_RENDER_MODE"	},
{	0x0C50,	"GL_PERSPECTIVE_CORRECTION_HINT"	},
{	0x0C51,	"GL_POINT_SMOOTH_HINT"	},
{	0x0C52,	"GL_LINE_SMOOTH_HINT"	},
{	0x0C53,	"GL_POLYGON_SMOOTH_HINT"	},
{	0x0C54,	"GL_FOG_HINT"	},
{	0x0C60,	"GL_TEXTURE_GEN_S"	},
{	0x0C61,	"GL_TEXTURE_GEN_T"	},
{	0x0C62,	"GL_TEXTURE_GEN_R"	},
{	0x0C63,	"GL_TEXTURE_GEN_Q"	},
{	0x0C70,	"GL_PIXEL_MAP_I_TO_I"	},
{	0x0C71,	"GL_PIXEL_MAP_S_TO_S"	},
{	0x0C72,	"GL_PIXEL_MAP_I_TO_R"	},
{	0x0C73,	"GL_PIXEL_MAP_I_TO_G"	},
{	0x0C74,	"GL_PIXEL_MAP_I_TO_B"	},
{	0x0C75,	"GL_PIXEL_MAP_I_TO_A"	},
{	0x0C76,	"GL_PIXEL_MAP_R_TO_R"	},
{	0x0C77,	"GL_PIXEL_MAP_G_TO_G"	},
{	0x0C78,	"GL_PIXEL_MAP_B_TO_B"	},
{	0x0C79,	"GL_PIXEL_MAP_A_TO_A"	},
{	0x0CB0,	"GL_PIXEL_MAP_I_TO_I_SIZE"	},
{	0x0CB1,	"GL_PIXEL_MAP_S_TO_S_SIZE"	},
{	0x0CB2,	"GL_PIXEL_MAP_I_TO_R_SIZE"	},
{	0x0CB3,	"GL_PIXEL_MAP_I_TO_G_SIZE"	},
{	0x0CB4,	"GL_PIXEL_MAP_I_TO_B_SIZE"	},
{	0x0CB5,	"GL_PIXEL_MAP_I_TO_A_SIZE"	},
{	0x0CB6,	"GL_PIXEL_MAP_R_TO_R_SIZE"	},
{	0x0CB7,	"GL_PIXEL_MAP_G_TO_G_SIZE"	},
{	0x0CB8,	"GL_PIXEL_MAP_B_TO_B_SIZE"	},
{	0x0CB9,	"GL_PIXEL_MAP_A_TO_A_SIZE"	},
{	0x0CF0,	"GL_UNPACK_SWAP_BYTES"	},
{	0x0CF1,	"GL_UNPACK_LSB_FIRST"	},
{	0x0CF2,	"GL_UNPACK_ROW_LENGTH"	},
{	0x0CF3,	"GL_UNPACK_SKIP_ROWS"	},
{	0x0CF4,	"GL_UNPACK_SKIP_PIXELS"	},
{	0x0CF5,	"GL_UNPACK_ALIGNMENT"	},
{	0x0D00,	"GL_PACK_SWAP_BYTES"	},
{	0x0D01,	"GL_PACK_LSB_FIRST"	},
{	0x0D02,	"GL_PACK_ROW_LENGTH"	},
{	0x0D03,	"GL_PACK_SKIP_ROWS"	},
{	0x0D04,	"GL_PACK_SKIP_PIXELS"	},
{	0x0D05,	"GL_PACK_ALIGNMENT"	},
{	0x0D10,	"GL_MAP_COLOR"	},
{	0x0D11,	"GL_MAP_STENCIL"	},
{	0x0D12,	"GL_INDEX_SHIFT"	},
{	0x0D13,	"GL_INDEX_OFFSET"	},
{	0x0D14,	"GL_RED_SCALE"	},
{	0x0D15,	"GL_RED_BIAS"	},
{	0x0D16,	"GL_ZOOM_X"	},
{	0x0D17,	"GL_ZOOM_Y"	},
{	0x0D18,	"GL_GREEN_SCALE"	},
{	0x0D19,	"GL_GREEN_BIAS"	},
{	0x0D1A,	"GL_BLUE_SCALE"	},
{	0x0D1B,	"GL_BLUE_BIAS"	},
{	0x0D1C,	"GL_ALPHA_SCALE"	},
{	0x0D1D,	"GL_ALPHA_BIAS"	},
{	0x0D1E,	"GL_DEPTH_SCALE"	},
{	0x0D1F,	"GL_DEPTH_BIAS"	},
{	0x0D30,	"GL_MAX_EVAL_ORDER"	},
{	0x0D31,	"GL_MAX_LIGHTS"	},
{	0x0D32,	"GL_MAX_CLIP_PLANES"	},
{	0x0D33,	"GL_MAX_TEXTURE_SIZE"	},
{	0x0D34,	"GL_MAX_PIXEL_MAP_TABLE"	},
{	0x0D35,	"GL_MAX_ATTRIB_STACK_DEPTH"	},
{	0x0D36,	"GL_MAX_MODELVIEW_STACK_DEPTH"	},
{	0x0D37,	"GL_MAX_NAME_STACK_DEPTH"	},
{	0x0D38,	"GL_MAX_PROJECTION_STACK_DEPTH"	},
{	0x0D39,	"GL_MAX_TEXTURE_STACK_DEPTH"	},
{	0x0D3A,	"GL_MAX_VIEWPORT_DIMS"	},
{	0x0D3B,	"GL_MAX_CLIENT_ATTRIB_STACK_DEPTH"	},
{	0x0D50,	"GL_SUBPIXEL_BITS"	},
{	0x0D51,	"GL_INDEX_BITS"	},
{	0x0D52,	"GL_RED_BITS"	},
{	0x0D53,	"GL_GREEN_BITS"	},
{	0x0D54,	"GL_BLUE_BITS"	},
{	0x0D55,	"GL_ALPHA_BITS"	},
{	0x0D56,	"GL_DEPTH_BITS"	},
{	0x0D57,	"GL_STENCIL_BITS"	},
{	0x0D58,	"GL_ACCUM_RED_BITS"	},
{	0x0D59,	"GL_ACCUM_GREEN_BITS"	},
{	0x0D5A,	"GL_ACCUM_BLUE_BITS"	},
{	0x0D5B,	"GL_ACCUM_ALPHA_BITS"	},
{	0x0D70,	"GL_NAME_STACK_DEPTH"	},
{	0x0D80,	"GL_AUTO_NORMAL"	},
{	0x0D90,	"GL_MAP1_COLOR_4"	},
{	0x0D91,	"GL_MAP1_INDEX"	},
{	0x0D92,	"GL_MAP1_NORMAL"	},
{	0x0D93,	"GL_MAP1_TEXTURE_COORD_1"	},
{	0x0D94,	"GL_MAP1_TEXTURE_COORD_2"	},
{	0x0D95,	"GL_MAP1_TEXTURE_COORD_3"	},
{	0x0D96,	"GL_MAP1_TEXTURE_COORD_4"	},
{	0x0D97,	"GL_MAP1_VERTEX_3"	},
{	0x0D98,	"GL_MAP1_VERTEX_4"	},
{	0x0DB0,	"GL_MAP2_COLOR_4"	},
{	0x0DB1,	"GL_MAP2_INDEX"	},
{	0x0DB2,	"GL_MAP2_NORMAL"	},
{	0x0DB3,	"GL_MAP2_TEXTURE_COORD_1"	},
{	0x0DB4,	"GL_MAP2_TEXTURE_COORD_2"	},
{	0x0DB5,	"GL_MAP2_TEXTURE_COORD_3"	},
{	0x0DB6,	"GL_MAP2_TEXTURE_COORD_4"	},
{	0x0DB7,	"GL_MAP2_VERTEX_3"	},
{	0x0DB8,	"GL_MAP2_VERTEX_4"	},
{	0x0DD0,	"GL_MAP1_GRID_DOMAIN"	},
{	0x0DD1,	"GL_MAP1_GRID_SEGMENTS"	},
{	0x0DD2,	"GL_MAP2_GRID_DOMAIN"	},
{	0x0DD3,	"GL_MAP2_GRID_SEGMENTS"	},
{	0x0DE0,	"GL_TEXTURE_1D"	},
{	0x0DE1,	"GL_TEXTURE_2D"	},
{	0x0DF0,	"GL_FEEDBACK_BUFFER_POINTER"	},
{	0x0DF1,	"GL_FEEDBACK_BUFFER_SIZE"	},
{	0x0DF2,	"GL_FEEDBACK_BUFFER_TYPE"	},
{	0x0DF3,	"GL_SELECTION_BUFFER_POINTER"	},
{	0x0DF4,	"GL_SELECTION_BUFFER_SIZE"	},
{	0x1000,	"GL_TEXTURE_WIDTH"	},
{	0x1001,	"GL_TEXTURE_HEIGHT"	},
{	0x1003,	"GL_TEXTURE_INTERNAL_FORMAT"	},
{	0x1004,	"GL_TEXTURE_BORDER_COLOR"	},
{	0x1005,	"GL_TEXTURE_BORDER"	},
{	0x1100,	"GL_DONT_CARE"	},
{	0x1101,	"GL_FASTEST"	},
{	0x1102,	"GL_NICEST"	},
{	0x1200,	"GL_AMBIENT"	},
{	0x1201,	"GL_DIFFUSE"	},
{	0x1202,	"GL_SPECULAR"	},
{	0x1203,	"GL_POSITION"	},
{	0x1204,	"GL_SPOT_DIRECTION"	},
{	0x1205,	"GL_SPOT_EXPONENT"	},
{	0x1206,	"GL_SPOT_CUTOFF"	},
{	0x1207,	"GL_CONSTANT_ATTENUATION"	},
{	0x1208,	"GL_LINEAR_ATTENUATION"	},
{	0x1209,	"GL_QUADRATIC_ATTENUATION"	},
{	0x1300,	"GL_COMPILE"	},
{	0x1301,	"GL_COMPILE_AND_EXECUTE"	},
{	0x1400,	"GL_BYTE "	},
{	0x1401,	"GL_UBYTE"	},
{	0x1402,	"GL_SHORT"	},
{	0x1403,	"GL_USHRT"	},
{	0x1404,	"GL_INT  "	},
{	0x1405,	"GL_UINT "	},
{	0x1406,	"GL_FLOAT"	},
{	0x1407,	"GL_2_BYTES"	},
{	0x1408,	"GL_3_BYTES"	},
{	0x1409,	"GL_4_BYTES"	},
{	0x140A,	"GL_DOUBLE"	},
{	0x140B,	"GL_HALF_FLOAT"	},
{	0x1500,	"GL_CLEAR"	},
{	0x1501,	"GL_AND"	},
{	0x1502,	"GL_AND_REVERSE"	},
{	0x1503,	"GL_COPY"	},
{	0x1504,	"GL_AND_INVERTED"	},
{	0x1505,	"GL_NOOP"	},
{	0x1506,	"GL_XOR"	},
{	0x1507,	"GL_OR"	},
{	0x1508,	"GL_NOR"	},
{	0x1509,	"GL_EQUIV"	},
{	0x150A,	"GL_INVERT"	},
{	0x150B,	"GL_OR_REVERSE"	},
{	0x150C,	"GL_COPY_INVERTED"	},
{	0x150D,	"GL_OR_INVERTED"	},
{	0x150E,	"GL_NAND"	},
{	0x150F,	"GL_SET"	},
{	0x1600,	"GL_EMISSION"	},
{	0x1601,	"GL_SHININESS"	},
{	0x1602,	"GL_AMBIENT_AND_DIFFUSE"	},
{	0x1603,	"GL_COLOR_INDEXES"	},
{	0x1700,	"GL_MODELVIEW"	},
{	0x1700,	"GL_MODELVIEW0_ARB"	},
{	0x1701,	"GL_PROJECTION"	},
{	0x1702,	"GL_TEXTURE"	},
{	0x1800,	"GL_COLOR"	},
{	0x1801,	"GL_DEPTH"	},
{	0x1802,	"GL_STENCIL"	},
{	0x1900,	"GL_COLOR_INDEX"	},
{	0x1901,	"GL_STENCIL_INDEX"	},
{	0x1902,	"GL_DEPTH_COMPONENT"	},
{	0x1903,	"GL_RED"	},
{	0x1904,	"GL_GREEN"	},
{	0x1905,	"GL_BLUE"	},
{	0x1906,	"GL_ALPHA"	},
{	0x1907,	"GL_RGB"	},
{	0x1908,	"GL_RGBA"	},
{	0x1909,	"GL_LUMINANCE"	},
{	0x190A,	"GL_LUMINANCE_ALPHA"	},
{	0x1A00,	"GL_BITMAP"	},
{	0x1B00,	"GL_POINT"	},
{	0x1B01,	"GL_LINE"	},
{	0x1B02,	"GL_FILL"	},
{	0x1C00,	"GL_RENDER"	},
{	0x1C01,	"GL_FEEDBACK"	},
{	0x1C02,	"GL_SELECT"	},
{	0x1D00,	"GL_FLAT"	},
{	0x1D01,	"GL_SMOOTH"	},
{	0x1E00,	"GL_KEEP"	},
{	0x1E01,	"GL_REPLACE"	},
{	0x1E02,	"GL_INCR"	},
{	0x1E03,	"GL_DECR"	},
{	0x1F00,	"GL_VENDOR"	},
{	0x1F01,	"GL_RENDERER"	},
{	0x1F02,	"GL_VERSION"	},
{	0x1F03,	"GL_EXTENSIONS"	},
{	0x2000,	"GL_S"	},
{	0x2001,	"GL_T"	},
{	0x2002,	"GL_R"	},
{	0x2003,	"GL_Q"	},
{	0x2100,	"GL_MODULATE"	},
{	0x2101,	"GL_DECAL"	},
{	0x2200,	"GL_TEXTURE_ENV_MODE"	},
{	0x2201,	"GL_TEXTURE_ENV_COLOR"	},
{	0x2300,	"GL_TEXTURE_ENV"	},
{	0x2400,	"GL_EYE_LINEAR"	},
{	0x2401,	"GL_OBJECT_LINEAR"	},
{	0x2402,	"GL_SPHERE_MAP"	},
{	0x2500,	"GL_TEXTURE_GEN_MODE"	},
{	0x2501,	"GL_OBJECT_PLANE"	},
{	0x2502,	"GL_EYE_PLANE"	},
{	0x2600,	"GL_NEAREST"	},
{	0x2601,	"GL_LINEAR"	},
{	0x2700,	"GL_NEAREST_MIPMAP_NEAREST"	},
{	0x2701,	"GL_LINEAR_MIPMAP_NEAREST"	},
{	0x2702,	"GL_NEAREST_MIPMAP_LINEAR"	},
{	0x2703,	"GL_LINEAR_MIPMAP_LINEAR"	},
{	0x2800,	"GL_TEXTURE_MAG_FILTER"	},
{	0x2801,	"GL_TEXTURE_MIN_FILTER"	},
{	0x2802,	"GL_TEXTURE_WRAP_S"	},
{	0x2803,	"GL_TEXTURE_WRAP_T"	},
{	0x2900,	"GL_CLAMP"	},
{	0x2901,	"GL_REPEAT"	},
{	0x2A00,	"GL_POLYGON_OFFSET_UNITS"	},
{	0x2A01,	"GL_POLYGON_OFFSET_POINT"	},
{	0x2A02,	"GL_POLYGON_OFFSET_LINE"	},
{	0x2A10,	"GL_R3_G3_B2"	},
{	0x2A20,	"GL_V2F"	},
{	0x2A21,	"GL_V3F"	},
{	0x2A22,	"GL_C4UB_V2F"	},
{	0x2A23,	"GL_C4UB_V3F"	},
{	0x2A24,	"GL_C3F_V3F"	},
{	0x2A25,	"GL_N3F_V3F"	},
{	0x2A26,	"GL_C4F_N3F_V3F"	},
{	0x2A27,	"GL_T2F_V3F"	},
{	0x2A28,	"GL_T4F_V4F"	},
{	0x2A29,	"GL_T2F_C4UB_V3F"	},
{	0x2A2A,	"GL_T2F_C3F_V3F"	},
{	0x2A2B,	"GL_T2F_N3F_V3F"	},
{	0x2A2C,	"GL_T2F_C4F_N3F_V3F"	},
{	0x2A2D,	"GL_T4F_C4F_N3F_V4F"	},
{	0x3000,	"GL_CLIP_PLANE0"	},
{	0x3001,	"GL_CLIP_PLANE1"	},
{	0x3002,	"GL_CLIP_PLANE2"	},
{	0x3003,	"GL_CLIP_PLANE3"	},
{	0x3004,	"GL_CLIP_PLANE4"	},
{	0x3005,	"GL_CLIP_PLANE5"	},
{	0x4000,	"GL_LIGHT0"	},
{	0x4001,	"GL_LIGHT1"	},
{	0x4002,	"GL_LIGHT2"	},
{	0x4003,	"GL_LIGHT3"	},
{	0x4004,	"GL_LIGHT4"	},
{	0x4005,	"GL_LIGHT5"	},
{	0x4006,	"GL_LIGHT6"	},
{	0x4007,	"GL_LIGHT7"	},
{	0x8000,	"GL_ABGR_EXT"	},
{	0x8001,	"GL_CONSTANT_COLOR"	},
{	0x8002,	"GL_ONE_MINUS_CONSTANT_COLOR"	},
{	0x8003,	"GL_CONSTANT_ALPHA"	},
{	0x8004,	"GL_ONE_MINUS_CONSTANT_ALPHA"	},
{	0x8005,	"GL_BLEND_COLOR"	},
{	0x8006,	"GL_FUNC_ADD"	},
{	0x8007,	"GL_MIN"	},
{	0x8008,	"GL_MAX"	},
{	0x8009,	"GL_BLEND_EQUATION_RGB"	},
{	0x8009,	"GL_BLEND_EQUATION"	},
{	0x800A,	"GL_FUNC_SUBTRACT"	},
{	0x800B,	"GL_FUNC_REVERSE_SUBTRACT"	},
{	0x8010,	"GL_CONVOLUTION_1D"	},
{	0x8011,	"GL_CONVOLUTION_2D"	},
{	0x8012,	"GL_SEPARABLE_2D"	},
{	0x8013,	"GL_CONVOLUTION_BORDER_MODE"	},
{	0x8014,	"GL_CONVOLUTION_FILTER_SCALE"	},
{	0x8015,	"GL_CONVOLUTION_FILTER_BIAS"	},
{	0x8016,	"GL_REDUCE"	},
{	0x8017,	"GL_CONVOLUTION_FORMAT"	},
{	0x8018,	"GL_CONVOLUTION_WIDTH"	},
{	0x8019,	"GL_CONVOLUTION_HEIGHT"	},
{	0x801A,	"GL_MAX_CONVOLUTION_WIDTH"	},
{	0x801B,	"GL_MAX_CONVOLUTION_HEIGHT"	},
{	0x801C,	"GL_POST_CONVOLUTION_RED_SCALE"	},
{	0x801D,	"GL_POST_CONVOLUTION_GREEN_SCALE"	},
{	0x801E,	"GL_POST_CONVOLUTION_BLUE_SCALE"	},
{	0x801F,	"GL_POST_CONVOLUTION_ALPHA_SCALE"	},
{	0x8020,	"GL_POST_CONVOLUTION_RED_BIAS"	},
{	0x8021,	"GL_POST_CONVOLUTION_GREEN_BIAS"	},
{	0x8022,	"GL_POST_CONVOLUTION_BLUE_BIAS"	},
{	0x8023,	"GL_POST_CONVOLUTION_ALPHA_BIAS"	},
{	0x8024,	"GL_HISTOGRAM"	},
{	0x8025,	"GL_PROXY_HISTOGRAM"	},
{	0x8026,	"GL_HISTOGRAM_WIDTH"	},
{	0x8027,	"GL_HISTOGRAM_FORMAT"	},
{	0x8028,	"GL_HISTOGRAM_RED_SIZE"	},
{	0x8029,	"GL_HISTOGRAM_GREEN_SIZE"	},
{	0x802A,	"GL_HISTOGRAM_BLUE_SIZE"	},
{	0x802B,	"GL_HISTOGRAM_ALPHA_SIZE"	},
{	0x802C,	"GL_HISTOGRAM_LUMINANCE_SIZE"	},
{	0x802D,	"GL_HISTOGRAM_SINK"	},
{	0x802E,	"GL_MINMAX"	},
{	0x802F,	"GL_MINMAX_FORMAT"	},
{	0x8030,	"GL_MINMAX_SINK"	},
{	0x8031,	"GL_TABLE_TOO_LARGE"	},
{	0x8032,	"GL_UNSIGNED_BYTE_3_3_2"	},
{	0x8033,	"GL_UNSIGNED_SHORT_4_4_4_4"	},
{	0x8034,	"GL_UNSIGNED_SHORT_5_5_5_1"	},
{	0x8035,	"GL_UNSIGNED_INT_8_8_8_8"	},
{	0x8036,	"GL_UNSIGNED_INT_10_10_10_2"	},
{	0x8037,	"GL_POLYGON_OFFSET_FILL"	},
{	0x8038,	"GL_POLYGON_OFFSET_FACTOR"	},
{	0x803A,	"GL_RESCALE_NORMAL"	},
{	0x803B,	"GL_ALPHA4"	},
{	0x803C,	"GL_ALPHA8"	},
{	0x803D,	"GL_ALPHA12"	},
{	0x803E,	"GL_ALPHA16"	},
{	0x803F,	"GL_LUMINANCE4"	},
{	0x8040,	"GL_LUMINANCE8"	},
{	0x8041,	"GL_LUMINANCE12"	},
{	0x8042,	"GL_LUMINANCE16"	},
{	0x8043,	"GL_LUMINANCE4_ALPHA4"	},
{	0x8044,	"GL_LUMINANCE6_ALPHA2"	},
{	0x8045,	"GL_LUMINANCE8_ALPHA8"	},
{	0x8046,	"GL_LUMINANCE12_ALPHA4"	},
{	0x8047,	"GL_LUMINANCE12_ALPHA12"	},
{	0x8048,	"GL_LUMINANCE16_ALPHA16"	},
{	0x8049,	"GL_INTENSITY"	},
{	0x804A,	"GL_INTENSITY4"	},
{	0x804B,	"GL_INTENSITY8"	},
{	0x804C,	"GL_INTENSITY12"	},
{	0x804D,	"GL_INTENSITY16"	},
{	0x804F,	"GL_RGB4"	},
{	0x8050,	"GL_RGB5"	},
{	0x8051,	"GL_RGB8"	},
{	0x8052,	"GL_RGB10"	},
{	0x8053,	"GL_RGB12"	},
{	0x8054,	"GL_RGB16"	},
{	0x8055,	"GL_RGBA2"	},
{	0x8056,	"GL_RGBA4"	},
{	0x8057,	"GL_RGB5_A1"	},
{	0x8058,	"GL_RGBA8"	},
{	0x8059,	"GL_RGB10_A2"	},
{	0x805A,	"GL_RGBA12"	},
{	0x805B,	"GL_RGBA16"	},
{	0x805C,	"GL_TEXTURE_RED_SIZE"	},
{	0x805D,	"GL_TEXTURE_GREEN_SIZE"	},
{	0x805E,	"GL_TEXTURE_BLUE_SIZE"	},
{	0x805F,	"GL_TEXTURE_ALPHA_SIZE"	},
{	0x8060,	"GL_TEXTURE_LUMINANCE_SIZE"	},
{	0x8061,	"GL_TEXTURE_INTENSITY_SIZE"	},
{	0x8063,	"GL_PROXY_TEXTURE_1D"	},
{	0x8064,	"GL_PROXY_TEXTURE_2D"	},
{	0x8066,	"GL_TEXTURE_PRIORITY"	},
{	0x8067,	"GL_TEXTURE_RESIDENT"	},
{	0x8068,	"GL_TEXTURE_BINDING_1D"	},
{	0x8069,	"GL_TEXTURE_BINDING_2D"	},
{	0x806A,	"GL_TEXTURE_BINDING_3D"	},
{	0x806B,	"GL_PACK_SKIP_IMAGES"	},
{	0x806C,	"GL_PACK_IMAGE_HEIGHT"	},
{	0x806D,	"GL_UNPACK_SKIP_IMAGES"	},
{	0x806E,	"GL_UNPACK_IMAGE_HEIGHT"	},
{	0x806F,	"GL_TEXTURE_3D"	},
{	0x8070,	"GL_PROXY_TEXTURE_3D"	},
{	0x8071,	"GL_TEXTURE_DEPTH"	},
{	0x8072,	"GL_TEXTURE_WRAP_R"	},
{	0x8073,	"GL_MAX_3D_TEXTURE_SIZE"	},
{	0x8074,	"GL_VERTEX_ARRAY"	},
{	0x8075,	"GL_NORMAL_ARRAY"	},
{	0x8076,	"GL_COLOR_ARRAY"	},
{	0x8077,	"GL_INDEX_ARRAY"	},
{	0x8078,	"GL_TEXTURE_COORD_ARRAY"	},
{	0x8079,	"GL_EDGE_FLAG_ARRAY"	},
{	0x807A,	"GL_VERTEX_ARRAY_SIZE"	},
{	0x807B,	"GL_VERTEX_ARRAY_TYPE"	},
{	0x807C,	"GL_VERTEX_ARRAY_STRIDE"	},
{	0x807E,	"GL_NORMAL_ARRAY_TYPE"	},
{	0x807F,	"GL_NORMAL_ARRAY_STRIDE"	},
{	0x8081,	"GL_COLOR_ARRAY_SIZE"	},
{	0x8082,	"GL_COLOR_ARRAY_TYPE"	},
{	0x8083,	"GL_COLOR_ARRAY_STRIDE"	},
{	0x8085,	"GL_INDEX_ARRAY_TYPE"	},
{	0x8086,	"GL_INDEX_ARRAY_STRIDE"	},
{	0x8088,	"GL_TEXTURE_COORD_ARRAY_SIZE"	},
{	0x8089,	"GL_TEXTURE_COORD_ARRAY_TYPE"	},
{	0x808A,	"GL_TEXTURE_COORD_ARRAY_STRIDE"	},
{	0x808C,	"GL_EDGE_FLAG_ARRAY_STRIDE"	},
{	0x808E,	"GL_VERTEX_ARRAY_POINTER"	},
{	0x808F,	"GL_NORMAL_ARRAY_POINTER"	},
{	0x8090,	"GL_COLOR_ARRAY_POINTER"	},
{	0x8091,	"GL_INDEX_ARRAY_POINTER"	},
{	0x8092,	"GL_TEXTURE_COORD_ARRAY_POINTER"	},
{	0x8093,	"GL_EDGE_FLAG_ARRAY_POINTER"	},
{	0x809D,	"GL_MULTISAMPLE_ARB"	},
{	0x809D,	"GL_MULTISAMPLE"	},
{	0x809E,	"GL_SAMPLE_ALPHA_TO_COVERAGE_ARB"	},
{	0x809E,	"GL_SAMPLE_ALPHA_TO_COVERAGE"	},
{	0x809F,	"GL_SAMPLE_ALPHA_TO_ONE_ARB"	},
{	0x809F,	"GL_SAMPLE_ALPHA_TO_ONE"	},
{	0x80A0,	"GL_SAMPLE_COVERAGE_ARB"	},
{	0x80A0,	"GL_SAMPLE_COVERAGE"	},
{	0x80A0,	"GL_SAMPLE_MASK_EXT"	},
{	0x80A1,	"GL_1PASS_EXT"	},
{	0x80A2,	"GL_2PASS_0_EXT"	},
{	0x80A3,	"GL_2PASS_1_EXT"	},
{	0x80A4,	"GL_4PASS_0_EXT"	},
{	0x80A5,	"GL_4PASS_1_EXT"	},
{	0x80A6,	"GL_4PASS_2_EXT"	},
{	0x80A7,	"GL_4PASS_3_EXT"	},
{	0x80A8,	"GL_SAMPLE_BUFFERS"	},
{	0x80A9,	"GL_SAMPLES"	},
{	0x80AA,	"GL_SAMPLE_COVERAGE_VALUE"	},
{	0x80AB,	"GL_SAMPLE_COVERAGE_INVERT"	},
{	0x80AC,	"GL_SAMPLE_PATTERN_EXT"	},
{	0x80B1,	"GL_COLOR_MATRIX"	},
{	0x80B2,	"GL_COLOR_MATRIX_STACK_DEPTH"	},
{	0x80B3,	"GL_MAX_COLOR_MATRIX_STACK_DEPTH"	},
{	0x80B4,	"GL_POST_COLOR_MATRIX_RED_SCALE"	},
{	0x80B5,	"GL_POST_COLOR_MATRIX_GREEN_SCALE"	},
{	0x80B6,	"GL_POST_COLOR_MATRIX_BLUE_SCALE"	},
{	0x80B7,	"GL_POST_COLOR_MATRIX_ALPHA_SCALE"	},
{	0x80B8,	"GL_POST_COLOR_MATRIX_RED_BIAS"	},
{	0x80B9,	"GL_POST_COLOR_MATRIX_GREEN_BIAS"	},
{	0x80BA,	"GL_POST_COLOR_MATRIX_BLUE_BIAS"	},
{	0x80BB,	"GL_POST_COLOR_MATRIX_ALPHA_BIAS"	},
{	0x80BF,	"GL_TEXTURE_COMPARE_FAIL_VALUE_ARB"	},
{	0x80C8,	"GL_BLEND_DST_RGB"	},
{	0x80C9,	"GL_BLEND_SRC_RGB"	},
{	0x80CA,	"GL_BLEND_DST_ALPHA"	},
{	0x80CB,	"GL_BLEND_SRC_ALPHA"	},
{	0x80CC,	"GL_422_EXT"	},
{	0x80CD,	"GL_422_REV_EXT"	},
{	0x80CE,	"GL_422_AVERAGE_EXT"	},
{	0x80CF,	"GL_422_REV_AVERAGE_EXT"	},
{	0x80D0,	"GL_COLOR_TABLE"	},
{	0x80D1,	"GL_POST_CONVOLUTION_COLOR_TABLE"	},
{	0x80D2,	"GL_POST_COLOR_MATRIX_COLOR_TABLE"	},
{	0x80D3,	"GL_PROXY_COLOR_TABLE"	},
{	0x80D4,	"GL_PROXY_POST_CONVOLUTION_COLOR_TABLE"	},
{	0x80D5,	"GL_PROXY_POST_COLOR_MATRIX_COLOR_TABLE"	},
{	0x80D6,	"GL_COLOR_TABLE_SCALE"	},
{	0x80D7,	"GL_COLOR_TABLE_BIAS"	},
{	0x80D8,	"GL_COLOR_TABLE_FORMAT"	},
{	0x80D9,	"GL_COLOR_TABLE_WIDTH"	},
{	0x80DA,	"GL_COLOR_TABLE_RED_SIZE"	},
{	0x80DB,	"GL_COLOR_TABLE_GREEN_SIZE"	},
{	0x80DC,	"GL_COLOR_TABLE_BLUE_SIZE"	},
{	0x80DD,	"GL_COLOR_TABLE_ALPHA_SIZE"	},
{	0x80DE,	"GL_COLOR_TABLE_LUMINANCE_SIZE"	},
{	0x80DF,	"GL_COLOR_TABLE_INTENSITY_SIZE"	},
{	0x80E0,	"GL_BGR_EXT"	},
{	0x80E0,	"GL_BGR"	},
{	0x80E1,	"GL_BGRA_EXT"	},
{	0x80E1,	"GL_BGRA"	},
{	0x80E1,	"GL_BGRA"	},
{	0x80E2,	"GL_COLOR_INDEX1_EXT"	},
{	0x80E3,	"GL_COLOR_INDEX2_EXT"	},
{	0x80E4,	"GL_COLOR_INDEX4_EXT"	},
{	0x80E5,	"GL_COLOR_INDEX8_EXT"	},
{	0x80E6,	"GL_COLOR_INDEX12_EXT"	},
{	0x80E7,	"GL_COLOR_INDEX16_EXT"	},
{	0x80E8,	"GL_MAX_ELEMENTS_VERTICES_EXT"	},
{	0x80E8,	"GL_MAX_ELEMENTS_VERTICES"	},
{	0x80E9,	"GL_MAX_ELEMENTS_INDICES_EXT"	},
{	0x80E9,	"GL_MAX_ELEMENTS_INDICES"	},
{	0x80ED,	"GL_TEXTURE_INDEX_SIZE_EXT"	},
{	0x80F0,	"GL_CLIP_VOLUME_CLIPPING_HINT_EXT"	},
{	0x8126,	"GL_POINT_SIZE_MIN_ARB"	},
{	0x8126,	"GL_POINT_SIZE_MIN"	},
{	0x8127,	"GL_POINT_SIZE_MAX_ARB"	},
{	0x8127,	"GL_POINT_SIZE_MAX"	},
{	0x8128,	"GL_POINT_FADE_THRESHOLD_SIZE_ARB"	},
{	0x8128,	"GL_POINT_FADE_THRESHOLD_SIZE"	},
{	0x8129,	"GL_POINT_DISTANCE_ATTENUATION_ARB"	},
{	0x8129,	"GL_POINT_DISTANCE_ATTENUATION"	},
{	0x812D,	"GL_CLAMP_TO_BORDER_ARB"	},
{	0x812D,	"GL_CLAMP_TO_BORDER"	},
{	0x812F,	"GL_CLAMP_TO_EDGE"	},
{	0x813A,	"GL_TEXTURE_MIN_LOD"	},
{	0x813B,	"GL_TEXTURE_MAX_LOD"	},
{	0x813C,	"GL_TEXTURE_BASE_LEVEL"	},
{	0x813D,	"GL_TEXTURE_MAX_LEVEL"	},
{	0x8151,	"GL_CONSTANT_BORDER"	},
{	0x8153,	"GL_REPLICATE_BORDER"	},
{	0x8154,	"GL_CONVOLUTION_BORDER_COLOR"	},
{	0x8191,	"GL_GENERATE_MIPMAP"	},
{	0x8192,	"GL_GENERATE_MIPMAP_HINT"	},
{	0x81A5,	"GL_DEPTH_COMPONENT16_ARB"	},
{	0x81A5,	"GL_DEPTH_COMPONENT16"	},
{	0x81A6,	"GL_DEPTH_COMPONENT24_ARB"	},
{	0x81A6,	"GL_DEPTH_COMPONENT24"	},
{	0x81A7,	"GL_DEPTH_COMPONENT32_ARB"	},
{	0x81A7,	"GL_DEPTH_COMPONENT32"	},
{	0x81A8,	"GL_ARRAY_ELEMENT_LOCK_FIRST_EXT"	},
{	0x81A9,	"GL_ARRAY_ELEMENT_LOCK_COUNT_EXT"	},
{	0x81AA,	"GL_CULL_VERTEX_EXT"	},
{	0x81AB,	"GL_CULL_VERTEX_EYE_POSITION_EXT"	},
{	0x81AC,	"GL_CULL_VERTEX_OBJECT_POSITION_EXT"	},
{	0x81AD,	"GL_IUI_V2F_EXT"	},
{	0x81AE,	"GL_IUI_V3F_EXT"	},
{	0x81AF,	"GL_IUI_N3F_V2F_EXT"	},
{	0x81B0,	"GL_IUI_N3F_V3F_EXT"	},
{	0x81B1,	"GL_T2F_IUI_V2F_EXT"	},
{	0x81B2,	"GL_T2F_IUI_V3F_EXT"	},
{	0x81B3,	"GL_T2F_IUI_N3F_V2F_EXT"	},
{	0x81B4,	"GL_T2F_IUI_N3F_V3F_EXT"	},
{	0x81B5,	"GL_INDEX_TEST_EXT"	},
{	0x81B6,	"GL_INDEX_TEST_FUNC_EXT"	},
{	0x81B7,	"GL_INDEX_TEST_REF_EXT"	},
{	0x81B8,	"GL_INDEX_MATERIAL_EXT"	},
{	0x81B9,	"GL_INDEX_MATERIAL_PARAMETER_EXT"	},
{	0x81BA,	"GL_INDEX_MATERIAL_FACE_EXT"	},
{	0x81F8,	"GL_LIGHT_MODEL_COLOR_CONTROL_EXT"	},
{	0x81F8,	"GL_LIGHT_MODEL_COLOR_CONTROL"	},
{	0x81F9,	"GL_SINGLE_COLOR_EXT"	},
{	0x81F9,	"GL_SINGLE_COLOR"	},
{	0x81FA,	"GL_SEPARATE_SPECULAR_COLOR_EXT"	},
{	0x81FA,	"GL_SEPARATE_SPECULAR_COLOR"	},
{	0x81FB,	"GL_SHARED_TEXTURE_PALETTE_EXT"	},
{	0x8210,	"GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING"	},
{	0x8211,	"GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE"	},
{	0x8212,	"GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE"	},
{	0x8213,	"GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE"	},
{	0x8214,	"GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE"	},
{	0x8215,	"GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE"	},
{	0x8216,	"GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE"	},
{	0x8217,	"GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE"	},
{	0x8218,	"GL_FRAMEBUFFER_DEFAULT"	},
{	0x8219,	"GL_FRAMEBUFFER_UNDEFINED"	},
{	0x821A,	"GL_DEPTH_STENCIL_ATTACHMENT"	},
{	0x8225,	"GL_COMPRESSED_RED"	},
{	0x8226,	"GL_COMPRESSED_RG"	},
{	0x8227,	"GL_RG"	},
{	0x8228,	"GL_RG_INTEGER"	},
{	0x8229,	"GL_R8"	},
{	0x822A,	"GL_R16"	},
{	0x822B,	"GL_RG8"	},
{	0x822C,	"GL_RG16"	},
{	0x822D,	"GL_R16F"	},
{	0x822E,	"GL_R32F"	},
{	0x822F,	"GL_RG16F"	},
{	0x8230,	"GL_RG32F"	},
{	0x8231,	"GL_R8I"	},
{	0x8232,	"GL_R8UI"	},
{	0x8233,	"GL_R16I"	},
{	0x8234,	"GL_R16UI"	},
{	0x8235,	"GL_R32I"	},
{	0x8236,	"GL_R32UI"	},
{	0x8237,	"GL_RG8I"	},
{	0x8238,	"GL_RG8UI"	},
{	0x8239,	"GL_RG16I"	},
{	0x823A,	"GL_RG16UI"	},
{	0x823B,	"GL_RG32I"	},
{	0x823C,	"GL_RG32UI"	},
{	0x8330,	"GL_PIXEL_TRANSFORM_2D_EXT"	},
{	0x8331,	"GL_PIXEL_MAG_FILTER_EXT"	},
{	0x8332,	"GL_PIXEL_MIN_FILTER_EXT"	},
{	0x8333,	"GL_PIXEL_CUBIC_WEIGHT_EXT"	},
{	0x8334,	"GL_CUBIC_EXT"	},
{	0x8335,	"GL_AVERAGE_EXT"	},
{	0x8336,	"GL_PIXEL_TRANSFORM_2D_STACK_DEPTH_EXT"	},
{	0x8337,	"GL_MAX_PIXEL_TRANSFORM_2D_STACK_DEPTH_EXT"	},
{	0x8338,	"GL_PIXEL_TRANSFORM_2D_MATRIX_EXT"	},
{	0x8349,	"GL_FRAGMENT_MATERIAL_EXT"	},
{	0x834A,	"GL_FRAGMENT_NORMAL_EXT"	},
{	0x834C,	"GL_FRAGMENT_COLOR_EXT"	},
{	0x834D,	"GL_ATTENUATION_EXT"	},
{	0x834E,	"GL_SHADOW_ATTENUATION_EXT"	},
{	0x834F,	"GL_TEXTURE_APPLICATION_MODE_EXT"	},
{	0x8350,	"GL_TEXTURE_LIGHT_EXT"	},
{	0x8351,	"GL_TEXTURE_MATERIAL_FACE_EXT"	},
{	0x8352,	"GL_TEXTURE_MATERIAL_PARAMETER_EXT"	},
{	0x8362,	"GL_UNSIGNED_BYTE_2_3_3_REV"	},
{	0x8363,	"GL_UNSIGNED_SHORT_5_6_5"	},
{	0x8364,	"GL_UNSIGNED_SHORT_5_6_5_REV"	},
{	0x8365,	"GL_UNSIGNED_SHORT_4_4_4_4_REV"	},
{	0x8366,	"GL_UNSIGNED_SHORT_1_5_5_5_REV"	},
{	0x8367,	"GL_UNSIGNED_INT_8_8_8_8_REV"	},
{	0x8368,	"GL_UNSIGNED_INT_2_10_10_10_REV"	},
{	0x8370,	"GL_MIRRORED_REPEAT_ARB"	},
{	0x8370,	"GL_MIRRORED_REPEAT"	},
{	0x83F0,	"GL_COMPRESSED_RGB_S3TC_DXT1_EXT"	},
{	0x83F1,	"GL_COMPRESSED_RGBA_S3TC_DXT1_EXT"	},
{	0x83F2,	"GL_COMPRESSED_RGBA_S3TC_DXT3_EXT"	},
{	0x83F3,	"GL_COMPRESSED_RGBA_S3TC_DXT5_EXT"	},
{	0x8439,	"GL_TANGENT_ARRAY_EXT"	},
{	0x843A,	"GL_BINORMAL_ARRAY_EXT"	},
{	0x843B,	"GL_CURRENT_TANGENT_EXT"	},
{	0x843C,	"GL_CURRENT_BINORMAL_EXT"	},
{	0x843E,	"GL_TANGENT_ARRAY_TYPE_EXT"	},
{	0x843F,	"GL_TANGENT_ARRAY_STRIDE_EXT"	},
{	0x8440,	"GL_BINORMAL_ARRAY_TYPE_EXT"	},
{	0x8441,	"GL_BINORMAL_ARRAY_STRIDE_EXT"	},
{	0x8442,	"GL_TANGENT_ARRAY_POINTER_EXT"	},
{	0x8443,	"GL_BINORMAL_ARRAY_POINTER_EXT"	},
{	0x8444,	"GL_MAP1_TANGENT_EXT"	},
{	0x8445,	"GL_MAP2_TANGENT_EXT"	},
{	0x8446,	"GL_MAP1_BINORMAL_EXT"	},
{	0x8447,	"GL_MAP2_BINORMAL_EXT"	},
{	0x8450,	"GL_FOG_COORD_SRC"	},
{	0x8450,	"GL_FOG_COORDINATE_SOURCE_EXT"	},
{	0x8450,	"GL_FOG_COORDINATE_SOURCE"	},
{	0x8451,	"GL_FOG_COORD"	},
{	0x8451,	"GL_FOG_COORDINATE_EXT"	},
{	0x8451,	"GL_FOG_COORDINATE"	},
{	0x8452,	"GL_FRAGMENT_DEPTH_EXT"	},
{	0x8452,	"GL_FRAGMENT_DEPTH"	},
{	0x8453  ,	"GL_CURRENT_FOG_COORD"	},
{	0x8453  ,	"GL_CURRENT_FOG_COORDINATE"	},
{	0x8453,	"GL_CURRENT_FOG_COORDINATE_EXT"	},
{	0x8454,	"GL_FOG_COORD_ARRAY_TYPE"	},
{	0x8454,	"GL_FOG_COORDINATE_ARRAY_TYPE_EXT"	},
{	0x8454,	"GL_FOG_COORDINATE_ARRAY_TYPE"	},
{	0x8455,	"GL_FOG_COORD_ARRAY_STRIDE"	},
{	0x8455,	"GL_FOG_COORDINATE_ARRAY_STRIDE_EXT"	},
{	0x8455,	"GL_FOG_COORDINATE_ARRAY_STRIDE"	},
{	0x8456,	"GL_FOG_COORD_ARRAY_POINTER"	},
{	0x8456,	"GL_FOG_COORDINATE_ARRAY_POINTER_EXT"	},
{	0x8456,	"GL_FOG_COORDINATE_ARRAY_POINTER"	},
{	0x8457,	"GL_FOG_COORD_ARRAY"	},
{	0x8457,	"GL_FOG_COORDINATE_ARRAY_EXT"	},
{	0x8457,	"GL_FOG_COORDINATE_ARRAY"	},
{	0x8458,	"GL_COLOR_SUM_ARB"	},
{	0x8458,	"GL_COLOR_SUM_EXT"	},
{	0x8458,	"GL_COLOR_SUM"	},
{	0x8459,	"GL_CURRENT_SECONDARY_COLOR_EXT"	},
{	0x8459,	"GL_CURRENT_SECONDARY_COLOR"	},
{	0x845A,	"GL_SECONDARY_COLOR_ARRAY_SIZE_EXT"	},
{	0x845A,	"GL_SECONDARY_COLOR_ARRAY_SIZE"	},
{	0x845B,	"GL_SECONDARY_COLOR_ARRAY_TYPE_EXT"	},
{	0x845B,	"GL_SECONDARY_COLOR_ARRAY_TYPE"	},
{	0x845C,	"GL_SECONDARY_COLOR_ARRAY_STRIDE_EXT"	},
{	0x845C,	"GL_SECONDARY_COLOR_ARRAY_STRIDE"	},
{	0x845D,	"GL_SECONDARY_COLOR_ARRAY_POINTER_EXT"	},
{	0x845D,	"GL_SECONDARY_COLOR_ARRAY_POINTER"	},
{	0x845E,	"GL_SECONDARY_COLOR_ARRAY_EXT"	},
{	0x845E,	"GL_SECONDARY_COLOR_ARRAY"	},
{	0x845F,	"GL_CURRENT_RASTER_SECONDARY_COLOR"	},
{	0x846D,	"GL_ALIASED_POINT_SIZE_RANGE"	},
{	0x846E,	"GL_ALIASED_LINE_WIDTH_RANGE"	},
{	0x84C0,	"GL_TEXTURE0"	},
{	0x84C1,	"GL_TEXTURE1"	},
{	0x84C2,	"GL_TEXTURE2"	},
{	0x84C3,	"GL_TEXTURE3"	},
{	0x84C4,	"GL_TEXTURE4"	},
{	0x84C5,	"GL_TEXTURE5"	},
{	0x84C6,	"GL_TEXTURE6"	},
{	0x84C7,	"GL_TEXTURE7"	},
{	0x84C8,	"GL_TEXTURE8"	},
{	0x84C9,	"GL_TEXTURE9"	},
{	0x84CA,	"GL_TEXTURE10"	},
{	0x84CB,	"GL_TEXTURE11"	},
{	0x84CC,	"GL_TEXTURE12"	},
{	0x84CD,	"GL_TEXTURE13"	},
{	0x84CE,	"GL_TEXTURE14"	},
{	0x84CF,	"GL_TEXTURE15"	},
{	0x84D0,	"GL_TEXTURE16"	},
{	0x84D1,	"GL_TEXTURE17"	},
{	0x84D2,	"GL_TEXTURE18"	},
{	0x84D3,	"GL_TEXTURE19"	},
{	0x84D4,	"GL_TEXTURE20"	},
{	0x84D5,	"GL_TEXTURE21"	},
{	0x84D6,	"GL_TEXTURE22"	},
{	0x84D7,	"GL_TEXTURE23"	},
{	0x84D8,	"GL_TEXTURE24"	},
{	0x84D9,	"GL_TEXTURE25"	},
{	0x84DA,	"GL_TEXTURE26"	},
{	0x84DB,	"GL_TEXTURE27"	},
{	0x84DC,	"GL_TEXTURE28"	},
{	0x84DD,	"GL_TEXTURE29"	},
{	0x84DE,	"GL_TEXTURE30"	},
{	0x84DF,	"GL_TEXTURE31"	},
{	0x84E0,	"GL_ACTIVE_TEXTURE"	},
{	0x84E1,	"GL_CLIENT_ACTIVE_TEXTURE"	},
{	0x84E2,	"GL_MAX_TEXTURE_UNITS"	},
{	0x84E3,	"GL_TRANSPOSE_MODELVIEW_MATRIX"	},
{	0x84E4,	"GL_TRANSPOSE_PROJECTION_MATRIX"	},
{	0x84E5,	"GL_TRANSPOSE_TEXTURE_MATRIX"	},
{	0x84E6,	"GL_TRANSPOSE_COLOR_MATRIX"	},
{	0x84E7,	"GL_SUBTRACT"	},
{	0x84E8,	"GL_MAX_RENDERBUFFER_SIZE"	},
{	0x84E9,	"GL_COMPRESSED_ALPHA"	},
{	0x84EA,	"GL_COMPRESSED_LUMINANCE"	},
{	0x84EB,	"GL_COMPRESSED_LUMINANCE_ALPHA"	},
{	0x84EC,	"GL_COMPRESSED_INTENSITY"	},
{	0x84ED,	"GL_COMPRESSED_RGB"	},
{	0x84EE,	"GL_COMPRESSED_RGBA"	},
{	0x84EF,	"GL_TEXTURE_COMPRESSION_HINT"	},
{	0x84F5,	"GL_TEXTURE_RECTANGLE_EXT"	},
{	0x84F6,	"GL_TEXTURE_BINDING_RECTANGLE_EXT"	},
{	0x84F7,	"GL_PROXY_TEXTURE_RECTANGLE_EXT"	},
{	0x84F8,	"GL_MAX_RECTANGLE_TEXTURE_SIZE_EXT"	},
{	0x84F9,	"GL_DEPTH_STENCIL"	},
{	0x84FA,	"GL_UNSIGNED_INT_24_8"	},
{	0x84FD,	"GL_MAX_TEXTURE_LOD_BIAS"	},
{	0x84FE,	"GL_TEXTURE_MAX_ANISOTROPY_EXT"	},
{	0x84FF,	"GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT"	},
{	0x8500,	"GL_TEXTURE_FILTER_CONTROL"	},
{	0x8501,	"GL_TEXTURE_LOD_BIAS"	},
{	0x8502,	"GL_MODELVIEW1_STACK_DEPTH_EXT"	},
{	0x8506,	"GL_MODELVIEW_MATRIX1_EXT"	},
{	0x8507,	"GL_INCR_WRAP"	},
{	0x8508,	"GL_DECR_WRAP"	},
{	0x8509,	"GL_VERTEX_WEIGHTING_EXT"	},
{	0x850A,	"GL_MODELVIEW1_ARB"	},
{	0x850B,	"GL_CURRENT_VERTEX_WEIGHT_EXT"	},
{	0x850C,	"GL_VERTEX_WEIGHT_ARRAY_EXT"	},
{	0x850D,	"GL_VERTEX_WEIGHT_ARRAY_SIZE_EXT"	},
{	0x850E,	"GL_VERTEX_WEIGHT_ARRAY_TYPE_EXT"	},
{	0x850F,	"GL_VERTEX_WEIGHT_ARRAY_STRIDE_EXT"	},
{	0x8510,	"GL_VERTEX_WEIGHT_ARRAY_POINTER_EXT"	},
{	0x8511,	"GL_NORMAL_MAP_ARB"	},
{	0x8511,	"GL_NORMAL_MAP_EXT"	},
{	0x8511,	"GL_NORMAL_MAP"	},
{	0x8512,	"GL_REFLECTION_MAP_ARB"	},
{	0x8512,	"GL_REFLECTION_MAP_EXT"	},
{	0x8512,	"GL_REFLECTION_MAP"	},
{	0x8513,	"GL_TEXTURE_CUBE_MAP_ARB"	},
{	0x8513,	"GL_TEXTURE_CUBE_MAP_EXT"	},
{	0x8513,	"GL_TEXTURE_CUBE_MAP"	},
{	0x8514,	"GL_TEXTURE_BINDING_CUBE_MAP_ARB"	},
{	0x8514,	"GL_TEXTURE_BINDING_CUBE_MAP_EXT"	},
{	0x8514,	"GL_TEXTURE_BINDING_CUBE_MAP"	},
{	0x8515,	"GL_TEXTURE_CUBE_MAP_POSITIVE_X"	},
{	0x8516,	"GL_TEXTURE_CUBE_MAP_NEGATIVE_X"	},
{	0x8517,	"GL_TEXTURE_CUBE_MAP_POSITIVE_Y"	},
{	0x8518,	"GL_TEXTURE_CUBE_MAP_NEGATIVE_Y"	},
{	0x8519,	"GL_TEXTURE_CUBE_MAP_POSITIVE_Z"	},
{	0x851A,	"GL_TEXTURE_CUBE_MAP_NEGATIVE_Z"	},
{	0x851B,	"GL_PROXY_TEXTURE_CUBE_MAP"	},
{	0x851C,	"GL_MAX_CUBE_MAP_TEXTURE_SIZE"	},
{	0x851D,	"GL_VERTEX_ARRAY_RANGE_APPLE"	},
{	0x851E,	"GL_VERTEX_ARRAY_RANGE_LENGTH_APPLE"	},
{	0x851F,	"GL_VERTEX_ARRAY_STORAGE_HINT_APPLE"	},
{	0x8520,	"GL_MAX_VERTEX_ARRAY_RANGE_ELEMENT_APPLE"	},
{	0x8521,	"GL_VERTEX_ARRAY_RANGE_POINTER_APPLE"	},
{	0x8570,	"GL_COMBINE_ARB"	},
{	0x8570,	"GL_COMBINE_EXT"	},
{	0x8570,	"GL_COMBINE"	},
{	0x8571,	"GL_COMBINE_RGB_ARB"	},
{	0x8571,	"GL_COMBINE_RGB_EXT"	},
{	0x8571,	"GL_COMBINE_RGB"	},
{	0x8572,	"GL_COMBINE_ALPHA_ARB"	},
{	0x8572,	"GL_COMBINE_ALPHA_EXT"	},
{	0x8572,	"GL_COMBINE_ALPHA"	},
{	0x8573,	"GL_RGB_SCALE_ARB"	},
{	0x8573,	"GL_RGB_SCALE_EXT"	},
{	0x8573,	"GL_RGB_SCALE"	},
{	0x8574,	"GL_ADD_SIGNED_ARB"	},
{	0x8574,	"GL_ADD_SIGNED_EXT"	},
{	0x8574,	"GL_ADD_SIGNED"	},
{	0x8575,	"GL_INTERPOLATE_ARB"	},
{	0x8575,	"GL_INTERPOLATE_EXT"	},
{	0x8575,	"GL_INTERPOLATE"	},
{	0x8576,	"GL_CONSTANT_ARB"	},
{	0x8576,	"GL_CONSTANT_EXT"	},
{	0x8576,	"GL_CONSTANT"	},
{	0x8577,	"GL_PRIMARY_COLOR_ARB"	},
{	0x8577,	"GL_PRIMARY_COLOR_EXT"	},
{	0x8577,	"GL_PRIMARY_COLOR"	},
{	0x8578,	"GL_PREVIOUS_ARB"	},
{	0x8578,	"GL_PREVIOUS_EXT"	},
{	0x8578,	"GL_PREVIOUS"	},
{	0x8580,	"GL_SOURCE0_RGB_ARB"	},
{	0x8580,	"GL_SOURCE0_RGB_EXT"	},
{	0x8580,	"GL_SOURCE0_RGB"	},
{	0x8580,	"GL_SRC0_RGB"	},
{	0x8581,	"GL_SOURCE1_RGB_ARB"	},
{	0x8581,	"GL_SOURCE1_RGB_EXT"	},
{	0x8581,	"GL_SOURCE1_RGB"	},
{	0x8581,	"GL_SRC1_RGB"	},
{	0x8582,	"GL_SOURCE2_RGB_ARB"	},
{	0x8582,	"GL_SOURCE2_RGB_EXT"	},
{	0x8582,	"GL_SOURCE2_RGB"	},
{	0x8582,	"GL_SRC2_RGB"	},
{	0x8583,	"GL_SOURCE3_RGB_ARB"	},
{	0x8583,	"GL_SOURCE3_RGB_EXT"	},
{	0x8583,	"GL_SOURCE3_RGB"	},
{	0x8583,	"GL_SRC3_RGB"	},
{	0x8584,	"GL_SOURCE4_RGB_ARB"	},
{	0x8584,	"GL_SOURCE4_RGB_EXT"	},
{	0x8584,	"GL_SOURCE4_RGB"	},
{	0x8584,	"GL_SRC4_RGB"	},
{	0x8585,	"GL_SOURCE5_RGB_ARB"	},
{	0x8585,	"GL_SOURCE5_RGB_EXT"	},
{	0x8585,	"GL_SOURCE5_RGB"	},
{	0x8585,	"GL_SRC5_RGB"	},
{	0x8586,	"GL_SOURCE6_RGB_ARB"	},
{	0x8586,	"GL_SOURCE6_RGB_EXT"	},
{	0x8586,	"GL_SOURCE6_RGB"	},
{	0x8586,	"GL_SRC6_RGB"	},
{	0x8587,	"GL_SOURCE7_RGB_ARB"	},
{	0x8587,	"GL_SOURCE7_RGB_EXT"	},
{	0x8587,	"GL_SOURCE7_RGB"	},
{	0x8587,	"GL_SRC7_RGB"	},
{	0x8588,	"GL_SOURCE0_ALPHA_ARB"	},
{	0x8588,	"GL_SOURCE0_ALPHA_EXT"	},
{	0x8588,	"GL_SOURCE0_ALPHA"	},
{	0x8588,	"GL_SRC0_ALPHA"	},
{	0x8589,	"GL_SOURCE1_ALPHA_ARB"	},
{	0x8589,	"GL_SOURCE1_ALPHA_EXT"	},
{	0x8589,	"GL_SOURCE1_ALPHA"	},
{	0x8589,	"GL_SRC1_ALPHA"	},
{	0x858A,	"GL_SOURCE2_ALPHA_ARB"	},
{	0x858A,	"GL_SOURCE2_ALPHA_EXT"	},
{	0x858A,	"GL_SOURCE2_ALPHA"	},
{	0x858A,	"GL_SRC2_ALPHA"	},
{	0x858B,	"GL_SOURCE3_ALPHA_ARB"	},
{	0x858B,	"GL_SOURCE3_ALPHA_EXT"	},
{	0x858B,	"GL_SOURCE3_ALPHA"	},
{	0x858B,	"GL_SRC3_ALPHA"	},
{	0x858C,	"GL_SOURCE4_ALPHA_ARB"	},
{	0x858C,	"GL_SOURCE4_ALPHA_EXT"	},
{	0x858C,	"GL_SOURCE4_ALPHA"	},
{	0x858C,	"GL_SRC4_ALPHA"	},
{	0x858D,	"GL_SOURCE5_ALPHA_ARB"	},
{	0x858D,	"GL_SOURCE5_ALPHA_EXT"	},
{	0x858D,	"GL_SOURCE5_ALPHA"	},
{	0x858D,	"GL_SRC5_ALPHA"	},
{	0x858E,	"GL_SOURCE6_ALPHA_ARB"	},
{	0x858E,	"GL_SOURCE6_ALPHA_EXT"	},
{	0x858E,	"GL_SOURCE6_ALPHA"	},
{	0x858E,	"GL_SRC6_ALPHA"	},
{	0x858F,	"GL_SOURCE7_ALPHA_ARB"	},
{	0x858F,	"GL_SOURCE7_ALPHA_EXT"	},
{	0x858F,	"GL_SOURCE7_ALPHA"	},
{	0x858F,	"GL_SRC7_ALPHA"	},
{	0x8590,	"GL_OPERAND0_RGB_ARB"	},
{	0x8590,	"GL_OPERAND0_RGB_EXT"	},
{	0x8590,	"GL_OPERAND0_RGB"	},
{	0x8591,	"GL_OPERAND1_RGB_ARB"	},
{	0x8591,	"GL_OPERAND1_RGB_EXT"	},
{	0x8591,	"GL_OPERAND1_RGB"	},
{	0x8592,	"GL_OPERAND2_RGB_ARB"	},
{	0x8592,	"GL_OPERAND2_RGB_EXT"	},
{	0x8592,	"GL_OPERAND2_RGB"	},
{	0x8593,	"GL_OPERAND3_RGB_ARB"	},
{	0x8593,	"GL_OPERAND3_RGB_EXT"	},
{	0x8593,	"GL_OPERAND3_RGB"	},
{	0x8594,	"GL_OPERAND4_RGB_ARB"	},
{	0x8594,	"GL_OPERAND4_RGB_EXT"	},
{	0x8594,	"GL_OPERAND4_RGB"	},
{	0x8595,	"GL_OPERAND5_RGB_ARB"	},
{	0x8595,	"GL_OPERAND5_RGB_EXT"	},
{	0x8595,	"GL_OPERAND5_RGB"	},
{	0x8596,	"GL_OPERAND6_RGB_ARB"	},
{	0x8596,	"GL_OPERAND6_RGB_EXT"	},
{	0x8596,	"GL_OPERAND6_RGB"	},
{	0x8597,	"GL_OPERAND7_RGB_ARB"	},
{	0x8597,	"GL_OPERAND7_RGB_EXT"	},
{	0x8597,	"GL_OPERAND7_RGB"	},
{	0x8598,	"GL_OPERAND0_ALPHA_ARB"	},
{	0x8598,	"GL_OPERAND0_ALPHA_EXT"	},
{	0x8598,	"GL_OPERAND0_ALPHA"	},
{	0x8599,	"GL_OPERAND1_ALPHA_ARB"	},
{	0x8599,	"GL_OPERAND1_ALPHA_EXT"	},
{	0x8599,	"GL_OPERAND1_ALPHA"	},
{	0x859A,	"GL_OPERAND2_ALPHA_ARB"	},
{	0x859A,	"GL_OPERAND2_ALPHA_EXT"	},
{	0x859A,	"GL_OPERAND2_ALPHA"	},
{	0x859B,	"GL_OPERAND3_ALPHA_ARB"	},
{	0x859B,	"GL_OPERAND3_ALPHA_EXT"	},
{	0x859B,	"GL_OPERAND3_ALPHA"	},
{	0x859C,	"GL_OPERAND4_ALPHA_ARB"	},
{	0x859C,	"GL_OPERAND4_ALPHA_EXT"	},
{	0x859C,	"GL_OPERAND4_ALPHA"	},
{	0x859D,	"GL_OPERAND5_ALPHA_ARB"	},
{	0x859D,	"GL_OPERAND5_ALPHA_EXT"	},
{	0x859D,	"GL_OPERAND5_ALPHA"	},
{	0x859E,	"GL_OPERAND6_ALPHA_ARB"	},
{	0x859E,	"GL_OPERAND6_ALPHA_EXT"	},
{	0x859E,	"GL_OPERAND6_ALPHA"	},
{	0x859F,	"GL_OPERAND7_ALPHA_ARB"	},
{	0x859F,	"GL_OPERAND7_ALPHA_EXT"	},
{	0x859F,	"GL_OPERAND7_ALPHA"	},
{	0x85AE,	"GL_PERTURB_EXT"	},
{	0x85AF,	"GL_TEXTURE_NORMAL_EXT"	},
{	0x85B4,	"GL_STORAGE_CLIENT_APPLE"	},
{	0x85B5,	"GL_VERTEX_ARRAY_BINDING_APPLE"	},
{	0x85BD,	"GL_STORAGE_PRIVATE_APPLE"	},
{	0x85BE,	"GL_STORAGE_CACHED_APPLE"	},
{	0x85BF,	"GL_STORAGE_SHARED_APPLE"	},
{	0x8620,	"GL_VERTEX_PROGRAM_ARB"	},
{	0x8620,	"GL_VERTEX_PROGRAM_NV"	},
{	0x8621,	"GL_VERTEX_STATE_PROGRAM_NV"	},
{	0x8622,	"GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB"	},
{	0x8622,	"GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB"	},
{	0x8622,	"GL_VERTEX_ATTRIB_ARRAY_ENABLED"	},
{	0x8623,	"GL_ATTRIB_ARRAY_SIZE_NV"	},
{	0x8623,	"GL_VERTEX_ATTRIB_ARRAY_SIZE_ARB"	},
{	0x8623,	"GL_VERTEX_ATTRIB_ARRAY_SIZE_ARB"	},
{	0x8623,	"GL_VERTEX_ATTRIB_ARRAY_SIZE"	},
{	0x8624,	"GL_ATTRIB_ARRAY_STRIDE_NV"	},
{	0x8624,	"GL_VERTEX_ATTRIB_ARRAY_STRIDE_ARB"	},
{	0x8624,	"GL_VERTEX_ATTRIB_ARRAY_STRIDE_ARB"	},
{	0x8624,	"GL_VERTEX_ATTRIB_ARRAY_STRIDE"	},
{	0x8625,	"GL_ATTRIB_ARRAY_TYPE_NV"	},
{	0x8625,	"GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB"	},
{	0x8625,	"GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB"	},
{	0x8625,	"GL_VERTEX_ATTRIB_ARRAY_TYPE"	},
{	0x8626,	"GL_CURRENT_ATTRIB_NV"	},
{	0x8626,	"GL_CURRENT_VERTEX_ATTRIB_ARB"	},
{	0x8626,	"GL_CURRENT_VERTEX_ATTRIB_ARB"	},
{	0x8626,	"GL_CURRENT_VERTEX_ATTRIB"	},
{	0x8627,	"GL_PROGRAM_LENGTH_ARB"	},
{	0x8627,	"GL_PROGRAM_LENGTH_NV"	},
{	0x8628,	"GL_PROGRAM_STRING_ARB"	},
{	0x8628,	"GL_PROGRAM_STRING_NV"	},
{	0x8629,	"GL_MODELVIEW_PROJECTION_NV"	},
{	0x862A,	"GL_IDENTITY_NV"	},
{	0x862B,	"GL_INVERSE_NV"	},
{	0x862C,	"GL_TRANSPOSE_NV"	},
{	0x862D,	"GL_INVERSE_TRANSPOSE_NV"	},
{	0x862E,	"GL_MAX_PROGRAM_MATRIX_STACK_DEPTH_ARB"	},
{	0x862E,	"GL_MAX_TRACK_MATRIX_STACK_DEPTH_NV"	},
{	0x862F,	"GL_MAX_PROGRAM_MATRICES_ARB"	},
{	0x862F,	"GL_MAX_TRACK_MATRICES_NV"	},
{	0x8630,	"GL_MATRIX0_NV"	},
{	0x8631,	"GL_MATRIX1_NV"	},
{	0x8632,	"GL_MATRIX2_NV"	},
{	0x8633,	"GL_MATRIX3_NV"	},
{	0x8634,	"GL_MATRIX4_NV"	},
{	0x8635,	"GL_MATRIX5_NV"	},
{	0x8636,	"GL_MATRIX6_NV"	},
{	0x8637,	"GL_MATRIX7_NV"	},
{	0x8640,	"GL_CURRENT_MATRIX_STACK_DEPTH_ARB"	},
{	0x8640,	"GL_CURRENT_MATRIX_STACK_DEPTH_NV"	},
{	0x8641,	"GL_CURRENT_MATRIX_ARB"	},
{	0x8641,	"GL_CURRENT_MATRIX_NV"	},
{	0x8642,	"GL_PROGRAM_POINT_SIZE_EXT"	},
{	0x8642,	"GL_VERTEX_PROGRAM_POINT_SIZE_ARB"	},
{	0x8642,	"GL_VERTEX_PROGRAM_POINT_SIZE_ARB"	},
{	0x8642,	"GL_VERTEX_PROGRAM_POINT_SIZE_NV"	},
{	0x8642,	"GL_VERTEX_PROGRAM_POINT_SIZE"	},
{	0x8643,	"GL_VERTEX_PROGRAM_TWO_SIDE_ARB"	},
{	0x8643,	"GL_VERTEX_PROGRAM_TWO_SIDE_ARB"	},
{	0x8643,	"GL_VERTEX_PROGRAM_TWO_SIDE_NV"	},
{	0x8643,	"GL_VERTEX_PROGRAM_TWO_SIDE"	},
{	0x8644,	"GL_PROGRAM_PARAMETER_NV"	},
{	0x8645,	"GL_ATTRIB_ARRAY_POINTER_NV"	},
{	0x8645,	"GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB"	},
{	0x8645,	"GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB"	},
{	0x8645,	"GL_VERTEX_ATTRIB_ARRAY_POINTER"	},
{	0x8646,	"GL_PROGRAM_TARGET_NV"	},
{	0x8647,	"GL_PROGRAM_RESIDENT_NV"	},
{	0x8648,	"GL_TRACK_MATRIX_NV"	},
{	0x8649,	"GL_TRACK_MATRIX_TRANSFORM_NV"	},
{	0x864A,	"GL_VERTEX_PROGRAM_BINDING_NV"	},
{	0x864B,	"GL_PROGRAM_ERROR_POSITION_ARB"	},
{	0x864B,	"GL_PROGRAM_ERROR_POSITION_NV"	},
{	0x8650,	"GL_VERTEX_ATTRIB_ARRAY0_NV"	},
{	0x8651,	"GL_VERTEX_ATTRIB_ARRAY1_NV"	},
{	0x8652,	"GL_VERTEX_ATTRIB_ARRAY2_NV"	},
{	0x8653,	"GL_VERTEX_ATTRIB_ARRAY3_NV"	},
{	0x8654,	"GL_VERTEX_ATTRIB_ARRAY4_NV"	},
{	0x8655,	"GL_VERTEX_ATTRIB_ARRAY5_NV"	},
{	0x8656,	"GL_VERTEX_ATTRIB_ARRAY6_NV"	},
{	0x8657,	"GL_VERTEX_ATTRIB_ARRAY7_NV"	},
{	0x8658,	"GL_VERTEX_ATTRIB_ARRAY8_NV"	},
{	0x8659,	"GL_VERTEX_ATTRIB_ARRAY9_NV"	},
{	0x865A,	"GL_VERTEX_ATTRIB_ARRAY10_NV"	},
{	0x865B,	"GL_VERTEX_ATTRIB_ARRAY11_NV"	},
{	0x865C,	"GL_VERTEX_ATTRIB_ARRAY12_NV"	},
{	0x865D,	"GL_VERTEX_ATTRIB_ARRAY13_NV"	},
{	0x865E,	"GL_VERTEX_ATTRIB_ARRAY14_NV"	},
{	0x865F,	"GL_VERTEX_ATTRIB_ARRAY15_NV"	},
{	0x8660,	"GL_MAP1_VERTEX_ATTRIB0_4_NV"	},
{	0x8661,	"GL_MAP1_VERTEX_ATTRIB1_4_NV"	},
{	0x8662,	"GL_MAP1_VERTEX_ATTRIB2_4_NV"	},
{	0x8663,	"GL_MAP1_VERTEX_ATTRIB3_4_NV"	},
{	0x8664,	"GL_MAP1_VERTEX_ATTRIB4_4_NV"	},
{	0x8665,	"GL_MAP1_VERTEX_ATTRIB5_4_NV"	},
{	0x8666,	"GL_MAP1_VERTEX_ATTRIB6_4_NV"	},
{	0x8667,	"GL_MAP1_VERTEX_ATTRIB7_4_NV"	},
{	0x8668,	"GL_MAP1_VERTEX_ATTRIB8_4_NV"	},
{	0x8669,	"GL_MAP1_VERTEX_ATTRIB9_4_NV"	},
{	0x866A,	"GL_MAP1_VERTEX_ATTRIB10_4_NV"	},
{	0x866B,	"GL_MAP1_VERTEX_ATTRIB11_4_NV"	},
{	0x866C,	"GL_MAP1_VERTEX_ATTRIB12_4_NV"	},
{	0x866D,	"GL_MAP1_VERTEX_ATTRIB13_4_NV"	},
{	0x866E,	"GL_MAP1_VERTEX_ATTRIB14_4_NV"	},
{	0x866F,	"GL_MAP1_VERTEX_ATTRIB15_4_NV"	},
{	0x8670,	"GL_MAP2_VERTEX_ATTRIB0_4_NV"	},
{	0x8671,	"GL_MAP2_VERTEX_ATTRIB1_4_NV"	},
{	0x8672,	"GL_MAP2_VERTEX_ATTRIB2_4_NV"	},
{	0x8673,	"GL_MAP2_VERTEX_ATTRIB3_4_NV"	},
{	0x8674,	"GL_MAP2_VERTEX_ATTRIB4_4_NV"	},
{	0x8675,	"GL_MAP2_VERTEX_ATTRIB5_4_NV"	},
{	0x8676,	"GL_MAP2_VERTEX_ATTRIB6_4_NV"	},
{	0x8677,	"GL_MAP2_VERTEX_ATTRIB7_4_NV"	},
{	0x8677,	"GL_PROGRAM_BINDING_ARB"	},
{	0x8677,	"GL_PROGRAM_NAME_ARB"	},
{	0x8678,	"GL_MAP2_VERTEX_ATTRIB8_4_NV"	},
{	0x8679,	"GL_MAP2_VERTEX_ATTRIB9_4_NV"	},
{	0x867A,	"GL_MAP2_VERTEX_ATTRIB10_4_NV"	},
{	0x867B,	"GL_MAP2_VERTEX_ATTRIB11_4_NV"	},
{	0x867C,	"GL_MAP2_VERTEX_ATTRIB12_4_NV"	},
{	0x867D,	"GL_MAP2_VERTEX_ATTRIB13_4_NV"	},
{	0x867E,	"GL_MAP2_VERTEX_ATTRIB14_4_NV"	},
{	0x867F,	"GL_MAP2_VERTEX_ATTRIB15_4_NV"	},
{	0x86A0,	"GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB"	},
{	0x86A0,	"GL_TEXTURE_COMPRESSED_IMAGE_SIZE"	},
{	0x86A1,	"GL_TEXTURE_COMPRESSED_ARB"	},
{	0x86A1,	"GL_TEXTURE_COMPRESSED"	},
{	0x86A2,	"GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB"	},
{	0x86A2,	"GL_NUM_COMPRESSED_TEXTURE_FORMATS"	},
{	0x86A3,	"GL_COMPRESSED_TEXTURE_FORMATS_ARB"	},
{	0x86A3,	"GL_COMPRESSED_TEXTURE_FORMATS"	},
{	0x86A4,	"GL_MAX_VERTEX_UNITS_ARB"	},
{	0x86A5,	"GL_ACTIVE_VERTEX_UNITS_ARB"	},
{	0x86A6,	"GL_WEIGHT_SUM_UNITY_ARB"	},
{	0x86A7,	"GL_VERTEX_BLEND_ARB"	},
{	0x86A8,	"GL_CURRENT_WEIGHT_ARB"	},
{	0x86A9,	"GL_WEIGHT_ARRAY_TYPE_ARB"	},
{	0x86AA,	"GL_WEIGHT_ARRAY_STRIDE_ARB"	},
{	0x86AB,	"GL_WEIGHT_ARRAY_SIZE_ARB"	},
{	0x86AC,	"GL_WEIGHT_ARRAY_POINTER_ARB"	},
{	0x86AD,	"GL_WEIGHT_ARRAY_ARB"	},
{	0x86AE,	"GL_DOT3_RGB_ARB"	},
{	0x86AE,	"GL_DOT3_RGB"	},
{	0x86AF,	"GL_DOT3_RGBA_ARB"	},
{	0x86AF,	"GL_DOT3_RGBA"	},
{	0x8722,	"GL_MODELVIEW2_ARB"	},
{	0x8723,	"GL_MODELVIEW3_ARB"	},
{	0x8724,	"GL_MODELVIEW4_ARB"	},
{	0x8725,	"GL_MODELVIEW5_ARB"	},
{	0x8726,	"GL_MODELVIEW6_ARB"	},
{	0x8727,	"GL_MODELVIEW7_ARB"	},
{	0x8728,	"GL_MODELVIEW8_ARB"	},
{	0x8729,	"GL_MODELVIEW9_ARB"	},
{	0x872A,	"GL_MODELVIEW10_ARB"	},
{	0x872B,	"GL_MODELVIEW11_ARB"	},
{	0x872C,	"GL_MODELVIEW12_ARB"	},
{	0x872D,	"GL_MODELVIEW13_ARB"	},
{	0x872E,	"GL_MODELVIEW14_ARB"	},
{	0x872F,	"GL_MODELVIEW15_ARB"	},
{	0x8730,	"GL_MODELVIEW16_ARB"	},
{	0x8731,	"GL_MODELVIEW17_ARB"	},
{	0x8732,	"GL_MODELVIEW18_ARB"	},
{	0x8733,	"GL_MODELVIEW19_ARB"	},
{	0x8734,	"GL_MODELVIEW20_ARB"	},
{	0x8735,	"GL_MODELVIEW21_ARB"	},
{	0x8736,	"GL_MODELVIEW22_ARB"	},
{	0x8737,	"GL_MODELVIEW23_ARB"	},
{	0x8738,	"GL_MODELVIEW24_ARB"	},
{	0x8739,	"GL_MODELVIEW25_ARB"	},
{	0x873A,	"GL_MODELVIEW26_ARB"	},
{	0x873B,	"GL_MODELVIEW27_ARB"	},
{	0x873C,	"GL_MODELVIEW28_ARB"	},
{	0x873D,	"GL_MODELVIEW29_ARB"	},
{	0x873E,	"GL_MODELVIEW30_ARB"	},
{	0x873F,	"GL_MODELVIEW31_ARB"	},
{	0x8742,	"GL_MIRROR_CLAMP_EXT"	},
{	0x8743,	"GL_MIRROR_CLAMP_TO_EDGE_EXT"	},
{	0x8764,	"GL_BUFFER_SIZE_ARB"	},
{	0x8764,	"GL_BUFFER_SIZE"	},
{	0x8765,	"GL_BUFFER_USAGE_ARB"	},
{	0x8765,	"GL_BUFFER_USAGE"	},
{	0x8780,	"GL_VERTEX_SHADER_EXT"	},
{	0x8781,	"GL_VERTEX_SHADER_BINDING_EXT"	},
{	0x8782,	"GL_OP_INDEX_EXT"	},
{	0x8783,	"GL_OP_NEGATE_EXT"	},
{	0x8784,	"GL_OP_DOT3_EXT"	},
{	0x8785,	"GL_OP_DOT4_EXT"	},
{	0x8786,	"GL_OP_MUL_EXT"	},
{	0x8787,	"GL_OP_ADD_EXT"	},
{	0x8788,	"GL_OP_MADD_EXT"	},
{	0x8789,	"GL_OP_FRAC_EXT"	},
{	0x878A,	"GL_OP_MAX_EXT"	},
{	0x878B,	"GL_OP_MIN_EXT"	},
{	0x878C,	"GL_OP_SET_GE_EXT"	},
{	0x878D,	"GL_OP_SET_LT_EXT"	},
{	0x878E,	"GL_OP_CLAMP_EXT"	},
{	0x878F,	"GL_OP_FLOOR_EXT"	},
{	0x8790,	"GL_OP_ROUND_EXT"	},
{	0x8791,	"GL_OP_EXP_BASE_2_EXT"	},
{	0x8792,	"GL_OP_LOG_BASE_2_EXT"	},
{	0x8793,	"GL_OP_POWER_EXT"	},
{	0x8794,	"GL_OP_RECIP_EXT"	},
{	0x8795,	"GL_OP_RECIP_SQRT_EXT"	},
{	0x8796,	"GL_OP_SUB_EXT"	},
{	0x8797,	"GL_OP_CROSS_PRODUCT_EXT"	},
{	0x8798,	"GL_OP_MULTIPLY_MATRIX_EXT"	},
{	0x8799,	"GL_OP_MOV_EXT"	},
{	0x879A,	"GL_OUTPUT_VERTEX_EXT"	},
{	0x879B,	"GL_OUTPUT_COLOR0_EXT"	},
{	0x879C,	"GL_OUTPUT_COLOR1_EXT"	},
{	0x879D,	"GL_OUTPUT_TEXTURE_COORD0_EXT"	},
{	0x879E,	"GL_OUTPUT_TEXTURE_COORD1_EXT"	},
{	0x879F,	"GL_OUTPUT_TEXTURE_COORD2_EXT"	},
{	0x87A0,	"GL_OUTPUT_TEXTURE_COORD3_EXT"	},
{	0x87A1,	"GL_OUTPUT_TEXTURE_COORD4_EXT"	},
{	0x87A2,	"GL_OUTPUT_TEXTURE_COORD5_EXT"	},
{	0x87A3,	"GL_OUTPUT_TEXTURE_COORD6_EXT"	},
{	0x87A4,	"GL_OUTPUT_TEXTURE_COORD7_EXT"	},
{	0x87A5,	"GL_OUTPUT_TEXTURE_COORD8_EXT"	},
{	0x87A6,	"GL_OUTPUT_TEXTURE_COORD9_EXT"	},
{	0x87A7,	"GL_OUTPUT_TEXTURE_COORD10_EXT"	},
{	0x87A8,	"GL_OUTPUT_TEXTURE_COORD11_EXT"	},
{	0x87A9,	"GL_OUTPUT_TEXTURE_COORD12_EXT"	},
{	0x87AA,	"GL_OUTPUT_TEXTURE_COORD13_EXT"	},
{	0x87AB,	"GL_OUTPUT_TEXTURE_COORD14_EXT"	},
{	0x87AC,	"GL_OUTPUT_TEXTURE_COORD15_EXT"	},
{	0x87AD,	"GL_OUTPUT_TEXTURE_COORD16_EXT"	},
{	0x87AE,	"GL_OUTPUT_TEXTURE_COORD17_EXT"	},
{	0x87AF,	"GL_OUTPUT_TEXTURE_COORD18_EXT"	},
{	0x87B0,	"GL_OUTPUT_TEXTURE_COORD19_EXT"	},
{	0x87B1,	"GL_OUTPUT_TEXTURE_COORD20_EXT"	},
{	0x87B2,	"GL_OUTPUT_TEXTURE_COORD21_EXT"	},
{	0x87B3,	"GL_OUTPUT_TEXTURE_COORD22_EXT"	},
{	0x87B4,	"GL_OUTPUT_TEXTURE_COORD23_EXT"	},
{	0x87B5,	"GL_OUTPUT_TEXTURE_COORD24_EXT"	},
{	0x87B6,	"GL_OUTPUT_TEXTURE_COORD25_EXT"	},
{	0x87B7,	"GL_OUTPUT_TEXTURE_COORD26_EXT"	},
{	0x87B8,	"GL_OUTPUT_TEXTURE_COORD27_EXT"	},
{	0x87B9,	"GL_OUTPUT_TEXTURE_COORD28_EXT"	},
{	0x87BA,	"GL_OUTPUT_TEXTURE_COORD29_EXT"	},
{	0x87BB,	"GL_OUTPUT_TEXTURE_COORD30_EXT"	},
{	0x87BC,	"GL_OUTPUT_TEXTURE_COORD31_EXT"	},
{	0x87BD,	"GL_OUTPUT_FOG_EXT"	},
{	0x87BE,	"GL_SCALAR_EXT"	},
{	0x87BF,	"GL_VECTOR_EXT"	},
{	0x87C0,	"GL_MATRIX_EXT"	},
{	0x87C1,	"GL_VARIANT_EXT"	},
{	0x87C2,	"GL_INVARIANT_EXT"	},
{	0x87C3,	"GL_LOCAL_CONSTANT_EXT"	},
{	0x87C4,	"GL_LOCAL_EXT"	},
{	0x87C5,	"GL_MAX_VERTEX_SHADER_INSTRUCTIONS_EXT"	},
{	0x87C6,	"GL_MAX_VERTEX_SHADER_VARIANTS_EXT"	},
{	0x87C7,	"GL_MAX_VERTEX_SHADER_INVARIANTS_EXT"	},
{	0x87C8,	"GL_MAX_VERTEX_SHADER_LOCAL_CONSTANTS_EXT"	},
{	0x87C9,	"GL_MAX_VERTEX_SHADER_LOCALS_EXT"	},
{	0x87CA,	"GL_MAX_OPTIMIZED_VERTEX_SHADER_INSTRUCTIONS_EXT"	},
{	0x87CB,	"GL_MAX_OPTIMIZED_VERTEX_SHADER_VARIANTS_EXT"	},
{	0x87CC,	"GL_MAX_OPTIMIZED_VERTEX_SHADER_LOCAL_CONSTANTS_EXT"	},
{	0x87CD,	"GL_MAX_OPTIMIZED_VERTEX_SHADER_INVARIANTS_EXT"	},
{	0x87CE,	"GL_MAX_OPTIMIZED_VERTEX_SHADER_LOCALS_EXT"	},
{	0x87CF,	"GL_VERTEX_SHADER_INSTRUCTIONS_EXT"	},
{	0x87D0,	"GL_VERTEX_SHADER_VARIANTS_EXT"	},
{	0x87D1,	"GL_VERTEX_SHADER_INVARIANTS_EXT"	},
{	0x87D2,	"GL_VERTEX_SHADER_LOCAL_CONSTANTS_EXT"	},
{	0x87D3,	"GL_VERTEX_SHADER_LOCALS_EXT"	},
{	0x87D4,	"GL_VERTEX_SHADER_OPTIMIZED_EXT"	},
{	0x87D5,	"GL_X_EXT"	},
{	0x87D6,	"GL_Y_EXT"	},
{	0x87D7,	"GL_Z_EXT"	},
{	0x87D8,	"GL_W_EXT"	},
{	0x87D9,	"GL_NEGATIVE_X_EXT"	},
{	0x87DA,	"GL_NEGATIVE_Y_EXT"	},
{	0x87DB,	"GL_NEGATIVE_Z_EXT"	},
{	0x87DC,	"GL_NEGATIVE_W_EXT"	},
{	0x87DF,	"GL_NEGATIVE_ONE_EXT"	},
{	0x87E0,	"GL_NORMALIZED_RANGE_EXT"	},
{	0x87E1,	"GL_FULL_RANGE_EXT"	},
{	0x87E2,	"GL_CURRENT_VERTEX_EXT"	},
{	0x87E3,	"GL_MVP_MATRIX_EXT"	},
{	0x87E4,	"GL_VARIANT_VALUE_EXT"	},
{	0x87E5,	"GL_VARIANT_DATATYPE_EXT"	},
{	0x87E6,	"GL_VARIANT_ARRAY_STRIDE_EXT"	},
{	0x87E7,	"GL_VARIANT_ARRAY_TYPE_EXT"	},
{	0x87E8,	"GL_VARIANT_ARRAY_EXT"	},
{	0x87E9,	"GL_VARIANT_ARRAY_POINTER_EXT"	},
{	0x87EA,	"GL_INVARIANT_VALUE_EXT"	},
{	0x87EB,	"GL_INVARIANT_DATATYPE_EXT"	},
{	0x87EC,	"GL_LOCAL_CONSTANT_VALUE_EXT"	},
{	0x87Ed,	"GL_LOCAL_CONSTANT_DATATYPE_EXT"	},
{	0x8800,	"GL_STENCIL_BACK_FUNC_ATI"	},
{	0x8800,	"GL_STENCIL_BACK_FUNC"	},
{	0x8801,	"GL_STENCIL_BACK_FAIL_ATI"	},
{	0x8801,	"GL_STENCIL_BACK_FAIL"	},
{	0x8802,	"GL_STENCIL_BACK_PASS_DEPTH_FAIL_ATI"	},
{	0x8802,	"GL_STENCIL_BACK_PASS_DEPTH_FAIL"	},
{	0x8803,	"GL_STENCIL_BACK_PASS_DEPTH_PASS_ATI"	},
{	0x8803,	"GL_STENCIL_BACK_PASS_DEPTH_PASS"	},
{	0x8804,	"GL_FRAGMENT_PROGRAM_ARB"	},
{	0x8805,	"GL_PROGRAM_ALU_INSTRUCTIONS_ARB"	},
{	0x8806,	"GL_PROGRAM_TEX_INSTRUCTIONS_ARB"	},
{	0x8807,	"GL_PROGRAM_TEX_INDIRECTIONS_ARB"	},
{	0x8808,	"GL_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB"	},
{	0x8809,	"GL_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB"	},
{	0x880A,	"GL_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB"	},
{	0x880B,	"GL_MAX_PROGRAM_ALU_INSTRUCTIONS_ARB"	},
{	0x880C,	"GL_MAX_PROGRAM_TEX_INSTRUCTIONS_ARB"	},
{	0x880D,	"GL_MAX_PROGRAM_TEX_INDIRECTIONS_ARB"	},
{	0x880E,	"GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB"	},
{	0x880F,	"GL_MAX_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB"	},
{	0x8810,	"GL_MAX_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB"	},
{	0x8814,	"GL_RGBA_FLOAT32_APPLE"	},
{	0x8814,	"GL_RGBA_FLOAT32_ATI"	},
{	0x8814,	"GL_RGBA32F_ARB"	},
{	0x8815,	"GL_RGB_FLOAT32_APPLE"	},
{	0x8815,	"GL_RGB_FLOAT32_ATI"	},
{	0x8815,	"GL_RGB32F_ARB"	},
{	0x8816,	"GL_ALPHA_FLOAT32_APPLE"	},
{	0x8816,	"GL_ALPHA_FLOAT32_ATI"	},
{	0x8816,	"GL_ALPHA32F_ARB"	},
{	0x8817,	"GL_INTENSITY_FLOAT32_APPLE"	},
{	0x8817,	"GL_INTENSITY_FLOAT32_ATI"	},
{	0x8817,	"GL_INTENSITY32F_ARB"	},
{	0x8818,	"GL_LUMINANCE_FLOAT32_APPLE"	},
{	0x8818,	"GL_LUMINANCE_FLOAT32_ATI"	},
{	0x8818,	"GL_LUMINANCE32F_ARB"	},
{	0x8819,	"GL_LUMINANCE_ALPHA_FLOAT32_APPLE"	},
{	0x8819,	"GL_LUMINANCE_ALPHA_FLOAT32_ATI"	},
{	0x8819,	"GL_LUMINANCE_ALPHA32F_ARB"	},
{	0x881A,	"GL_RGBA_FLOAT16_APPLE"	},
{	0x881A,	"GL_RGBA_FLOAT16_ATI"	},
{	0x881A,	"GL_RGBA16F_ARB"	},
{	0x881B,	"GL_RGB_FLOAT16_APPLE"	},
{	0x881B,	"GL_RGB_FLOAT16_ATI"	},
{	0x881B,	"GL_RGB16F_ARB"	},
{	0x881C,	"GL_ALPHA_FLOAT16_APPLE"	},
{	0x881C,	"GL_ALPHA_FLOAT16_ATI"	},
{	0x881C,	"GL_ALPHA16F_ARB"	},
{	0x881D,	"GL_INTENSITY_FLOAT16_APPLE"	},
{	0x881D,	"GL_INTENSITY_FLOAT16_ATI"	},
{	0x881D,	"GL_INTENSITY16F_ARB"	},
{	0x881E,	"GL_LUMINANCE_FLOAT16_APPLE"	},
{	0x881E,	"GL_LUMINANCE_FLOAT16_ATI"	},
{	0x881E,	"GL_LUMINANCE16F_ARB"	},
{	0x881F,	"GL_LUMINANCE_ALPHA_FLOAT16_APPLE"	},
{	0x881F,	"GL_LUMINANCE_ALPHA_FLOAT16_ATI"	},
{	0x881F,	"GL_LUMINANCE_ALPHA16F_ARB"	},
{	0x8820,	"GL_RGBA_FLOAT_MODE_ARB"	},
{	0x8824,	"GL_MAX_DRAW_BUFFERS_ARB"	},
{	0x8824,	"GL_MAX_DRAW_BUFFERS"	},
{	0x8825,	"GL_DRAW_BUFFER0_ARB"	},
{	0x8825,	"GL_DRAW_BUFFER0"	},
{	0x8826,	"GL_DRAW_BUFFER1_ARB"	},
{	0x8826,	"GL_DRAW_BUFFER1"	},
{	0x8827,	"GL_DRAW_BUFFER2_ARB"	},
{	0x8827,	"GL_DRAW_BUFFER2"	},
{	0x8828,	"GL_DRAW_BUFFER3_ARB"	},
{	0x8828,	"GL_DRAW_BUFFER3"	},
{	0x8829,	"GL_DRAW_BUFFER4_ARB"	},
{	0x8829,	"GL_DRAW_BUFFER4"	},
{	0x882A,	"GL_DRAW_BUFFER5_ARB"	},
{	0x882A,	"GL_DRAW_BUFFER5"	},
{	0x882B,	"GL_DRAW_BUFFER6_ARB"	},
{	0x882B,	"GL_DRAW_BUFFER6"	},
{	0x882C,	"GL_DRAW_BUFFER7_ARB"	},
{	0x882C,	"GL_DRAW_BUFFER7"	},
{	0x882D,	"GL_DRAW_BUFFER8_ARB"	},
{	0x882D,	"GL_DRAW_BUFFER8"	},
{	0x882E,	"GL_DRAW_BUFFER9_ARB"	},
{	0x882E,	"GL_DRAW_BUFFER9"	},
{	0x882F,	"GL_DRAW_BUFFER10_ARB"	},
{	0x882F,	"GL_DRAW_BUFFER10"	},
{	0x8830,	"GL_DRAW_BUFFER11_ARB"	},
{	0x8830,	"GL_DRAW_BUFFER11"	},
{	0x8831,	"GL_DRAW_BUFFER12_ARB"	},
{	0x8831,	"GL_DRAW_BUFFER12"	},
{	0x8832,	"GL_DRAW_BUFFER13_ARB"	},
{	0x8832,	"GL_DRAW_BUFFER13"	},
{	0x8833,	"GL_DRAW_BUFFER14_ARB"	},
{	0x8833,	"GL_DRAW_BUFFER14"	},
{	0x8834,	"GL_DRAW_BUFFER15_ARB"	},
{	0x8834,	"GL_DRAW_BUFFER15"	},
{	0x883D,	"GL_ALPHA_BLEND_EQUATION_ATI"	},
{	0x883D,	"GL_BLEND_EQUATION_ALPHA_EXT"	},
{	0x883D,	"GL_BLEND_EQUATION_ALPHA"	},
{	0x884A,	"GL_TEXTURE_DEPTH_SIZE_ARB"	},
{	0x884A,	"GL_TEXTURE_DEPTH_SIZE"	},
{	0x884B,	"GL_DEPTH_TEXTURE_MODE_ARB"	},
{	0x884B,	"GL_DEPTH_TEXTURE_MODE"	},
{	0x884C,	"GL_TEXTURE_COMPARE_MODE_ARB"	},
{	0x884C,	"GL_TEXTURE_COMPARE_MODE"	},
{	0x884D,	"GL_TEXTURE_COMPARE_FUNC_ARB"	},
{	0x884D,	"GL_TEXTURE_COMPARE_FUNC"	},
{	0x884E,	"GL_COMPARE_R_TO_TEXTURE_ARB"	},
{	0x884E,	"GL_COMPARE_R_TO_TEXTURE"	},
{	0x884E,	"GL_COMPARE_REF_DEPTH_TO_TEXTURE_EXT"	},
{	0x8861,	"GL_POINT_SPRITE_ARB"	},
{	0x8861,	"GL_POINT_SPRITE"	},
{	0x8862,	"GL_COORD_REPLACE_ARB"	},
{	0x8862,	"GL_COORD_REPLACE"	},
{	0x8864,	"GL_QUERY_COUNTER_BITS_ARB"	},
{	0x8864,	"GL_QUERY_COUNTER_BITS"	},
{	0x8865,	"GL_CURRENT_QUERY_ARB"	},
{	0x8865,	"GL_CURRENT_QUERY"	},
{	0x8866,	"GL_QUERY_RESULT_ARB"	},
{	0x8866,	"GL_QUERY_RESULT"	},
{	0x8867,	"GL_QUERY_RESULT_AVAILABLE_ARB"	},
{	0x8867,	"GL_QUERY_RESULT_AVAILABLE"	},
{	0x8869,	"GL_MAX_VERTEX_ATTRIBS_ARB"	},
{	0x8869,	"GL_MAX_VERTEX_ATTRIBS_ARB"	},
{	0x8869,	"GL_MAX_VERTEX_ATTRIBS"	},
{	0x886A,	"GL_VERTEX_ATTRIB_ARRAY_NORMALIZED_ARB"	},
{	0x886A,	"GL_VERTEX_ATTRIB_ARRAY_NORMALIZED_ARB"	},
{	0x886A,	"GL_VERTEX_ATTRIB_ARRAY_NORMALIZED"	},
{	0x8871,	"GL_MAX_TEXTURE_COORDS_ARB"	},
{	0x8871,	"GL_MAX_TEXTURE_COORDS_ARB"	},
{	0x8871,	"GL_MAX_TEXTURE_COORDS_ARB"	},
{	0x8871,	"GL_MAX_TEXTURE_COORDS"	},
{	0x8872,	"GL_MAX_TEXTURE_IMAGE_UNITS_ARB"	},
{	0x8872,	"GL_MAX_TEXTURE_IMAGE_UNITS_ARB"	},
{	0x8872,	"GL_MAX_TEXTURE_IMAGE_UNITS_ARB"	},
{	0x8872,	"GL_MAX_TEXTURE_IMAGE_UNITS"	},
{	0x8874,	"GL_PROGRAM_ERROR_STRING_ARB"	},
{	0x8875,	"GL_PROGRAM_FORMAT_ASCII_ARB"	},
{	0x8876,	"GL_PROGRAM_FORMAT_ARB"	},
{	0x8890,	"GL_DEPTH_BOUNDS_TEST_EXT"	},
{	0x8891,	"GL_DEPTH_BOUNDS_EXT"	},
{	0x8892,	"GL_ARRAY_BUFFER_ARB"	},
{	0x8892,	"GL_ARRAY_BUFFER"	},
{	0x8893,	"GL_ELEMENT_ARRAY_BUFFER_ARB"	},
{	0x8893,	"GL_ELEMENT_ARRAY_BUFFER"	},
{	0x8894,	"GL_ARRAY_BUFFER_BINDING_ARB"	},
{	0x8894,	"GL_ARRAY_BUFFER_BINDING"	},
{	0x8895,	"GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB"	},
{	0x8895,	"GL_ELEMENT_ARRAY_BUFFER_BINDING"	},
{	0x8896,	"GL_VERTEX_ARRAY_BUFFER_BINDING_ARB"	},
{	0x8896,	"GL_VERTEX_ARRAY_BUFFER_BINDING"	},
{	0x8897,	"GL_NORMAL_ARRAY_BUFFER_BINDING_ARB"	},
{	0x8897,	"GL_NORMAL_ARRAY_BUFFER_BINDING"	},
{	0x8898,	"GL_COLOR_ARRAY_BUFFER_BINDING_ARB"	},
{	0x8898,	"GL_COLOR_ARRAY_BUFFER_BINDING"	},
{	0x8899,	"GL_INDEX_ARRAY_BUFFER_BINDING_ARB"	},
{	0x8899,	"GL_INDEX_ARRAY_BUFFER_BINDING"	},
{	0x889A,	"GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB"	},
{	0x889A,	"GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING"	},
{	0x889B,	"GL_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB"	},
{	0x889B,	"GL_EDGE_FLAG_ARRAY_BUFFER_BINDING"	},
{	0x889C,	"GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING_ARB"	},
{	0x889C,	"GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING"	},
{	0x889D,	"GL_FOG_COORD_ARRAY_BUFFER_BINDING_ARB"	},
{	0x889D,	"GL_FOG_COORD_ARRAY_BUFFER_BINDING"	},
{	0x889D,	"GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING_ARB"	},
{	0x889D,	"GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING"	},
{	0x889E,	"GL_WEIGHT_ARRAY_BUFFER_BINDING_ARB"	},
{	0x889E,	"GL_WEIGHT_ARRAY_BUFFER_BINDING"	},
{	0x889F,	"GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB"	},
{	0x889F,	"GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING"	},
{	0x88A0,	"GL_PROGRAM_INSTRUCTIONS_ARB"	},
{	0x88A1,	"GL_MAX_PROGRAM_INSTRUCTIONS_ARB"	},
{	0x88A2,	"GL_PROGRAM_NATIVE_INSTRUCTIONS_ARB"	},
{	0x88A3,	"GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB"	},
{	0x88A4,	"GL_PROGRAM_TEMPORARIES_ARB"	},
{	0x88A5,	"GL_MAX_PROGRAM_TEMPORARIES_ARB"	},
{	0x88A6,	"GL_PROGRAM_NATIVE_TEMPORARIES_ARB"	},
{	0x88A7,	"GL_MAX_PROGRAM_NATIVE_TEMPORARIES_ARB"	},
{	0x88A8,	"GL_PROGRAM_PARAMETERS_ARB"	},
{	0x88A9,	"GL_MAX_PROGRAM_PARAMETERS_ARB"	},
{	0x88AA,	"GL_PROGRAM_NATIVE_PARAMETERS_ARB"	},
{	0x88AB,	"GL_MAX_PROGRAM_NATIVE_PARAMETERS_ARB"	},
{	0x88AC,	"GL_PROGRAM_ATTRIBS_ARB"	},
{	0x88AD,	"GL_MAX_PROGRAM_ATTRIBS_ARB"	},
{	0x88AE,	"GL_PROGRAM_NATIVE_ATTRIBS_ARB"	},
{	0x88AF,	"GL_MAX_PROGRAM_NATIVE_ATTRIBS_ARB"	},
{	0x88B0,	"GL_PROGRAM_ADDRESS_REGISTERS_ARB"	},
{	0x88B1,	"GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB"	},
{	0x88B2,	"GL_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB"	},
{	0x88B3,	"GL_MAX_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB"	},
{	0x88B4,	"GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB"	},
{	0x88B5,	"GL_MAX_PROGRAM_ENV_PARAMETERS_ARB"	},
{	0x88B6,	"GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB"	},
{	0x88B7,	"GL_TRANSPOSE_CURRENT_MATRIX_ARB"	},
{	0x88B8,	"GL_READ_ONLY_ARB"	},
{	0x88B8,	"GL_READ_ONLY"	},
{	0x88B9,	"GL_WRITE_ONLY_ARB"	},
{	0x88B9,	"GL_WRITE_ONLY"	},
{	0x88BA,	"GL_READ_WRITE_ARB"	},
{	0x88BA,	"GL_READ_WRITE"	},
{	0x88BB,	"GL_BUFFER_ACCESS_ARB"	},
{	0x88BB,	"GL_BUFFER_ACCESS"	},
{	0x88BC,	"GL_BUFFER_MAPPED_ARB"	},
{	0x88BC,	"GL_BUFFER_MAPPED"	},
{	0x88BD,	"GL_BUFFER_MAP_POINTER_ARB"	},
{	0x88BD,	"GL_BUFFER_MAP_POINTER"	},
{	0x88C0,	"GL_MATRIX0_ARB"	},
{	0x88C1,	"GL_MATRIX1_ARB"	},
{	0x88C2,	"GL_MATRIX2_ARB"	},
{	0x88C3,	"GL_MATRIX3_ARB"	},
{	0x88C4,	"GL_MATRIX4_ARB"	},
{	0x88C5,	"GL_MATRIX5_ARB"	},
{	0x88C6,	"GL_MATRIX6_ARB"	},
{	0x88C7,	"GL_MATRIX7_ARB"	},
{	0x88C8,	"GL_MATRIX8_ARB"	},
{	0x88C9,	"GL_MATRIX9_ARB"	},
{	0x88CA,	"GL_MATRIX10_ARB"	},
{	0x88CB,	"GL_MATRIX11_ARB"	},
{	0x88CC,	"GL_MATRIX12_ARB"	},
{	0x88CD,	"GL_MATRIX13_ARB"	},
{	0x88CE,	"GL_MATRIX14_ARB"	},
{	0x88CF,	"GL_MATRIX15_ARB"	},
{	0x88D0,	"GL_MATRIX16_ARB"	},
{	0x88D1,	"GL_MATRIX17_ARB"	},
{	0x88D2,	"GL_MATRIX18_ARB"	},
{	0x88D3,	"GL_MATRIX19_ARB"	},
{	0x88D4,	"GL_MATRIX20_ARB"	},
{	0x88D5,	"GL_MATRIX21_ARB"	},
{	0x88D6,	"GL_MATRIX22_ARB"	},
{	0x88D7,	"GL_MATRIX23_ARB"	},
{	0x88D8,	"GL_MATRIX24_ARB"	},
{	0x88D9,	"GL_MATRIX25_ARB"	},
{	0x88DA,	"GL_MATRIX26_ARB"	},
{	0x88DB,	"GL_MATRIX27_ARB"	},
{	0x88DC,	"GL_MATRIX28_ARB"	},
{	0x88DD,	"GL_MATRIX29_ARB"	},
{	0x88DE,	"GL_MATRIX30_ARB"	},
{	0x88DF,	"GL_MATRIX31_ARB"	},
{	0x88E0,	"GL_STREAM_DRAW_ARB"	},
{	0x88E0,	"GL_STREAM_DRAW"	},
{	0x88E1,	"GL_STREAM_READ_ARB"	},
{	0x88E1,	"GL_STREAM_READ"	},
{	0x88E2,	"GL_STREAM_COPY_ARB"	},
{	0x88E2,	"GL_STREAM_COPY"	},
{	0x88E4,	"GL_STATIC_DRAW_ARB"	},
{	0x88E4,	"GL_STATIC_DRAW"	},
{	0x88E5,	"GL_STATIC_READ_ARB"	},
{	0x88E5,	"GL_STATIC_READ"	},
{	0x88E6,	"GL_STATIC_COPY_ARB"	},
{	0x88E6,	"GL_STATIC_COPY"	},
{	0x88E8,	"GL_DYNAMIC_DRAW_ARB"	},
{	0x88E8,	"GL_DYNAMIC_DRAW"	},
{	0x88E9,	"GL_DYNAMIC_READ_ARB"	},
{	0x88E9,	"GL_DYNAMIC_READ"	},
{	0x88EA,	"GL_DYNAMIC_COPY_ARB"	},
{	0x88EA,	"GL_DYNAMIC_COPY"	},
{	0x88EB,	"GL_PIXEL_PACK_BUFFER_ARB"	},
{	0x88EB,	"GL_PIXEL_PACK_BUFFER"	},
{	0x88EC,	"GL_PIXEL_UNPACK_BUFFER_ARB"	},
{	0x88EC,	"GL_PIXEL_UNPACK_BUFFER"	},
{	0x88ED,	"GL_PIXEL_PACK_BUFFER_BINDING_ARB"	},
{	0x88ED,	"GL_PIXEL_PACK_BUFFER_BINDING"	},
{	0x88EF,	"GL_PIXEL_UNPACK_BUFFER_BINDING_ARB"	},
{	0x88EF,	"GL_PIXEL_UNPACK_BUFFER_BINDING"	},
{	0x88F0,	"GL_DEPTH24_STENCIL8_EXT"	},
{	0x88F0,	"GL_DEPTH24_STENCIL8"	},
{	0x88F1,	"GL_TEXTURE_STENCIL_SIZE_EXT"	},
{	0x88F1,	"GL_TEXTURE_STENCIL_SIZE"	},
{	0x88FD,	"GL_VERTEX_ATTRIB_ARRAY_INTEGER_EXT"	},
{	0x88FE,	"GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ARB"	},
{	0x88FF,	"GL_MAX_ARRAY_TEXTURE_LAYERS_EXT"	},
{	0x8904,	"GL_MIN_PROGRAM_TEXEL_OFFSET_EXT"	},
{	0x8905,	"GL_MAX_PROGRAM_TEXEL_OFFSET_EXT"	},
{	0x8910,	"GL_STENCIL_TEST_TWO_SIDE_EXT"	},
{	0x8911,	"GL_ACTIVE_STENCIL_FACE_EXT"	},
{	0x8912,	"GL_MIRROR_CLAMP_TO_BORDER_EXT"	},
{	0x8914,	"GL_SAMPLES_PASSED_ARB"	},
{	0x8914,	"GL_SAMPLES_PASSED"	},
{	0x891A,	"GL_CLAMP_VERTEX_COLOR_ARB"	},
{	0x891B,	"GL_CLAMP_FRAGMENT_COLOR_ARB"	},
{	0x891C,	"GL_CLAMP_READ_COLOR_ARB"	},
{	0x891D,	"GL_FIXED_ONLY_ARB"	},
{	0x8920,	"GL_FRAGMENT_SHADER_EXT"	},
{	0x896D,	"GL_SECONDARY_INTERPOLATOR_EXT"	},
{	0x896E,	"GL_NUM_FRAGMENT_REGISTERS_EXT"	},
{	0x896F,	"GL_NUM_FRAGMENT_CONSTANTS_EXT"	},
{	0x8A0C,	"GL_ELEMENT_ARRAY_APPLE"	},
{	0x8A0D,	"GL_ELEMENT_ARRAY_TYPE_APPLE"	},
{	0x8A0E,	"GL_ELEMENT_ARRAY_POINTER_APPLE"	},
{	0x8A0F,	"GL_COLOR_FLOAT_APPLE"	},
{	0x8A11,	"GL_UNIFORM_BUFFER"	},
{	0x8A28,	"GL_UNIFORM_BUFFER_BINDING"	},
{	0x8A29,	"GL_UNIFORM_BUFFER_START"	},
{	0x8A2A,	"GL_UNIFORM_BUFFER_SIZE"	},
{	0x8A2B,	"GL_MAX_VERTEX_UNIFORM_BLOCKS"	},
{	0x8A2C,	"GL_MAX_GEOMETRY_UNIFORM_BLOCKS"	},
{	0x8A2D,	"GL_MAX_FRAGMENT_UNIFORM_BLOCKS"	},
{	0x8A2E,	"GL_MAX_COMBINED_UNIFORM_BLOCKS"	},
{	0x8A2F,	"GL_MAX_UNIFORM_BUFFER_BINDINGS"	},
{	0x8A30,	"GL_MAX_UNIFORM_BLOCK_SIZE"	},
{	0x8A31,	"GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS"	},
{	0x8A32,	"GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS"	},
{	0x8A33,	"GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS"	},
{	0x8A34,	"GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT"	},
{	0x8A35,	"GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH"	},
{	0x8A36,	"GL_ACTIVE_UNIFORM_BLOCKS"	},
{	0x8A37,	"GL_UNIFORM_TYPE"	},
{	0x8A38,	"GL_UNIFORM_SIZE"	},
{	0x8A39,	"GL_UNIFORM_NAME_LENGTH"	},
{	0x8A3A,	"GL_UNIFORM_BLOCK_INDEX"	},
{	0x8A3B,	"GL_UNIFORM_OFFSET"	},
{	0x8A3C,	"GL_UNIFORM_ARRAY_STRIDE"	},
{	0x8A3D,	"GL_UNIFORM_MATRIX_STRIDE"	},
{	0x8A3E,	"GL_UNIFORM_IS_ROW_MAJOR"	},
{	0x8A3F,	"GL_UNIFORM_BLOCK_BINDING"	},
{	0x8A40,	"GL_UNIFORM_BLOCK_DATA_SIZE"	},
{	0x8A41,	"GL_UNIFORM_BLOCK_NAME_LENGTH"	},
{	0x8A42,	"GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS"	},
{	0x8A43,	"GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES"	},
{	0x8A44,	"GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER"	},
{	0x8A45,	"GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER"	},
{	0x8A46,	"GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER"	},
{	0x8B30,	"GL_FRAGMENT_SHADER_ARB"	},
{	0x8B30,	"GL_FRAGMENT_SHADER"	},
{	0x8B31,	"GL_VERTEX_SHADER_ARB"	},
{	0x8B31,	"GL_VERTEX_SHADER"	},
{	0x8B40,	"GL_PROGRAM_OBJECT_ARB"	},
{	0x8B48,	"GL_SHADER_OBJECT_ARB"	},
{	0x8B49,	"GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB"	},
{	0x8B49,	"GL_MAX_FRAGMENT_UNIFORM_COMPONENTS"	},
{	0x8B4A,	"GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB"	},
{	0x8B4A,	"GL_MAX_VERTEX_UNIFORM_COMPONENTS"	},
{	0x8B4B,	"GL_MAX_VARYING_COMPONENTS_EXT"	},
{	0x8B4B,	"GL_MAX_VARYING_FLOATS_ARB"	},
{	0x8B4B,	"GL_MAX_VARYING_FLOATS"	},
{	0x8B4C,	"GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB"	},
{	0x8B4C,	"GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS"	},
{	0x8B4D,	"GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB"	},
{	0x8B4D,	"GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS"	},
{	0x8B4E,	"GL_OBJECT_TYPE_ARB"	},
{	0x8B4F,	"GL_OBJECT_SUBTYPE_ARB"	},
{	0x8B4F,	"GL_SHADER_TYPE"	},
{	0x8B50,	"GL_FLOAT_VEC2_ARB"	},
{	0x8B50,	"GL_FLOAT_VEC2"	},
{	0x8B51,	"GL_FLOAT_VEC3_ARB"	},
{	0x8B51,	"GL_FLOAT_VEC3"	},
{	0x8B52,	"GL_FLOAT_VEC4_ARB"	},
{	0x8B52,	"GL_FLOAT_VEC4"	},
{	0x8B53,	"GL_INT_VEC2_ARB"	},
{	0x8B53,	"GL_INT_VEC2"	},
{	0x8B54,	"GL_INT_VEC3_ARB"	},
{	0x8B54,	"GL_INT_VEC3"	},
{	0x8B55,	"GL_INT_VEC4_ARB"	},
{	0x8B55,	"GL_INT_VEC4"	},
{	0x8B56,	"GL_BOOL_ARB"	},
{	0x8B56,	"GL_BOOL"	},
{	0x8B57,	"GL_BOOL_VEC2_ARB"	},
{	0x8B57,	"GL_BOOL_VEC2"	},
{	0x8B58,	"GL_BOOL_VEC3_ARB"	},
{	0x8B58,	"GL_BOOL_VEC3"	},
{	0x8B59,	"GL_BOOL_VEC4_ARB"	},
{	0x8B59,	"GL_BOOL_VEC4"	},
{	0x8B5A,	"GL_FLOAT_MAT2_ARB"	},
{	0x8B5A,	"GL_FLOAT_MAT2"	},
{	0x8B5B,	"GL_FLOAT_MAT3_ARB"	},
{	0x8B5B,	"GL_FLOAT_MAT3"	},
{	0x8B5C,	"GL_FLOAT_MAT4_ARB"	},
{	0x8B5C,	"GL_FLOAT_MAT4"	},
{	0x8B5D,	"GL_SAMPLER_1D_ARB"	},
{	0x8B5D,	"GL_SAMPLER_1D"	},
{	0x8B5E,	"GL_SAMPLER_2D_ARB"	},
{	0x8B5E,	"GL_SAMPLER_2D"	},
{	0x8B5F,	"GL_SAMPLER_3D_ARB"	},
{	0x8B5F,	"GL_SAMPLER_3D"	},
{	0x8B60,	"GL_SAMPLER_CUBE_ARB"	},
{	0x8B60,	"GL_SAMPLER_CUBE"	},
{	0x8B61,	"GL_SAMPLER_1D_SHADOW_ARB"	},
{	0x8B61,	"GL_SAMPLER_1D_SHADOW"	},
{	0x8B62,	"GL_SAMPLER_2D_SHADOW_ARB"	},
{	0x8B62,	"GL_SAMPLER_2D_SHADOW"	},
{	0x8B63,	"GL_SAMPLER_2D_RECT_ARB"	},
{	0x8B64,	"GL_SAMPLER_2D_RECT_SHADOW_ARB"	},
{	0x8B65,	"GL_FLOAT_MAT2x3"	},
{	0x8B66,	"GL_FLOAT_MAT2x4"	},
{	0x8B67,	"GL_FLOAT_MAT3x2"	},
{	0x8B68,	"GL_FLOAT_MAT3x4"	},
{	0x8B69,	"GL_FLOAT_MAT4x2"	},
{	0x8B6A,	"GL_FLOAT_MAT4x3"	},
{	0x8B80,	"GL_DELETE_STATUS"	},
{	0x8B80,	"GL_OBJECT_DELETE_STATUS_ARB"	},
{	0x8B81,	"GL_COMPILE_STATUS"	},
{	0x8B81,	"GL_OBJECT_COMPILE_STATUS_ARB"	},
{	0x8B82,	"GL_LINK_STATUS"	},
{	0x8B82,	"GL_OBJECT_LINK_STATUS_ARB"	},
{	0x8B83,	"GL_OBJECT_VALIDATE_STATUS_ARB"	},
{	0x8B83,	"GL_VALIDATE_STATUS"	},
{	0x8B84,	"GL_INFO_LOG_LENGTH"	},
{	0x8B84,	"GL_OBJECT_INFO_LOG_LENGTH_ARB"	},
{	0x8B85,	"GL_ATTACHED_SHADERS"	},
{	0x8B85,	"GL_OBJECT_ATTACHED_OBJECTS_ARB"	},
{	0x8B86,	"GL_ACTIVE_UNIFORMS"	},
{	0x8B86,	"GL_OBJECT_ACTIVE_UNIFORMS_ARB"	},
{	0x8B87,	"GL_ACTIVE_UNIFORM_MAX_LENGTH"	},
{	0x8B87,	"GL_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB"	},
{	0x8B88,	"GL_OBJECT_SHADER_SOURCE_LENGTH_ARB"	},
{	0x8B88,	"GL_SHADER_SOURCE_LENGTH"	},
{	0x8B89,	"GL_ACTIVE_ATTRIBUTES"	},
{	0x8B89,	"GL_OBJECT_ACTIVE_ATTRIBUTES_ARB"	},
{	0x8B8A,	"GL_ACTIVE_ATTRIBUTE_MAX_LENGTH"	},
{	0x8B8A,	"GL_OBJECT_ACTIVE_ATTRIBUTE_MAX_LENGTH_ARB"	},
{	0x8B8B,	"GL_FRAGMENT_SHADER_DERIVATIVE_HINT_ARB"	},
{	0x8B8B,	"GL_FRAGMENT_SHADER_DERIVATIVE_HINT"	},
{	0x8B8C,	"GL_SHADING_LANGUAGE_VERSION_ARB"	},
{	0x8B8C,	"GL_SHADING_LANGUAGE_VERSION"	},
{	0x8B8D,	"GL_CURRENT_PROGRAM"	},
{	0x8C10,	"GL_TEXTURE_RED_TYPE_ARB"	},
{	0x8C10,	"GL_TEXTURE_RED_TYPE"	},
{	0x8C11,	"GL_TEXTURE_GREEN_TYPE_ARB"	},
{	0x8C11,	"GL_TEXTURE_GREEN_TYPE"	},
{	0x8C12,	"GL_TEXTURE_BLUE_TYPE_ARB"	},
{	0x8C12,	"GL_TEXTURE_BLUE_TYPE"	},
{	0x8C13,	"GL_TEXTURE_ALPHA_TYPE_ARB"	},
{	0x8C13,	"GL_TEXTURE_ALPHA_TYPE"	},
{	0x8C14,	"GL_TEXTURE_LUMINANCE_TYPE_ARB"	},
{	0x8C15,	"GL_TEXTURE_INTENSITY_TYPE_ARB"	},
{	0x8C16,	"GL_TEXTURE_DEPTH_TYPE_ARB"	},
{	0x8C16,	"GL_TEXTURE_DEPTH_TYPE"	},
{	0x8C17,	"GL_UNSIGNED_NORMALIZED_ARB"	},
{	0x8C17,	"GL_UNSIGNED_NORMALIZED"	},
{	0x8C18,	"GL_TEXTURE_1D_ARRAY_EXT"	},
{	0x8C19,	"GL_PROXY_TEXTURE_1D_ARRAY_EXT"	},
{	0x8C1A,	"GL_TEXTURE_2D_ARRAY_EXT"	},
{	0x8C1B,	"GL_PROXY_TEXTURE_2D_ARRAY_EXT"	},
{	0x8C1C,	"GL_TEXTURE_BINDING_1D_ARRAY_EXT"	},
{	0x8C1D,	"GL_TEXTURE_BINDING_2D_ARRAY_EXT"	},
{	0x8C29,	"GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_EXT"	},
{	0x8C3A,	"GL_R11F_G11F_B10F_EXT"	},
{	0x8C3B,	"GL_UNSIGNED_INT_10F_11F_11F_REV_EXT"	},
{	0x8C3C,	"GL_RGBA_SIGNED_COMPONENTS_EXT"	},
{	0x8C3D,	"GL_RGB9_E5_EXT"	},
{	0x8C3E,	"GL_UNSIGNED_INT_5_9_9_9_REV_EXT"	},
{	0x8C3F,	"GL_TEXTURE_SHARED_SIZE_EXT"	},
{	0x8C40,	"GL_SRGB_EXT"	},
{	0x8C40,	"GL_SRGB"	},
{	0x8C41,	"GL_SRGB8_EXT"	},
{	0x8C41,	"GL_SRGB8"	},
{	0x8C42,	"GL_SRGB_ALPHA_EXT"	},
{	0x8C42,	"GL_SRGB_ALPHA"	},
{	0x8C43,	"GL_SRGB8_ALPHA8_EXT"	},
{	0x8C43,	"GL_SRGB8_ALPHA8"	},
{	0x8C44,	"GL_SLUMINANCE_ALPHA_EXT"	},
{	0x8C44,	"GL_SLUMINANCE_ALPHA"	},
{	0x8C45,	"GL_SLUMINANCE8_ALPHA8_EXT"	},
{	0x8C45,	"GL_SLUMINANCE8_ALPHA8"	},
{	0x8C46,	"GL_SLUMINANCE_EXT"	},
{	0x8C46,	"GL_SLUMINANCE"	},
{	0x8C47,	"GL_SLUMINANCE8_EXT"	},
{	0x8C47,	"GL_SLUMINANCE8"	},
{	0x8C48,	"GL_COMPRESSED_SRGB_EXT"	},
{	0x8C48,	"GL_COMPRESSED_SRGB"	},
{	0x8C49,	"GL_COMPRESSED_SRGB_ALPHA_EXT"	},
{	0x8C49,	"GL_COMPRESSED_SRGB_ALPHA"	},
{	0x8C4A,	"GL_COMPRESSED_SLUMINANCE_EXT"	},
{	0x8C4A,	"GL_COMPRESSED_SLUMINANCE"	},
{	0x8C4B,	"GL_COMPRESSED_SLUMINANCE_ALPHA_EXT"	},
{	0x8C4B,	"GL_COMPRESSED_SLUMINANCE_ALPHA"	},
{	0x8C4C,	"GL_COMPRESSED_SRGB_S3TC_DXT1_EXT"	},
{	0x8C4D,	"GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT"	},
{	0x8C4E,	"GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT"	},
{	0x8C4F,	"GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT"	},
{	0x8C76,	"GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH_EXT"	},
{	0x8C7F,	"GL_TRANSFORM_FEEDBACK_BUFFER_MODE_EXT"	},
{	0x8C80,	"GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS_EXT"	},
{	0x8C83,	"GL_TRANSFORM_FEEDBACK_VARYINGS_EXT"	},
{	0x8C84,	"GL_TRANSFORM_FEEDBACK_BUFFER_START_EXT"	},
{	0x8C85,	"GL_TRANSFORM_FEEDBACK_BUFFER_SIZE_EXT"	},
{	0x8C87,	"GL_PRIMITIVES_GENERATED_EXT"	},
{	0x8C88,	"GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN_EXT"	},
{	0x8C89,	"GL_RASTERIZER_DISCARD_EXT"	},
{	0x8C8A,	"GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS_EXT"	},
{	0x8C8B,	"GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS_EXT"	},
{	0x8C8C,	"GL_INTERLEAVED_ATTRIBS_EXT"	},
{	0x8C8D,	"GL_SEPARATE_ATTRIBS_EXT"	},
{	0x8C8E,	"GL_TRANSFORM_FEEDBACK_BUFFER_EXT"	},
{	0x8C8F,	"GL_TRANSFORM_FEEDBACK_BUFFER_BINDING_EXT"	},
{	0x8CA0,	"GL_POINT_SPRITE_COORD_ORIGIN"	},
{	0x8CA1,	"GL_LOWER_LEFT"	},
{	0x8CA2,	"GL_UPPER_LEFT"	},
{	0x8CA3,	"GL_STENCIL_BACK_REF"	},
{	0x8CA4,	"GL_STENCIL_BACK_VALUE_MASK"	},
{	0x8CA5,	"GL_STENCIL_BACK_WRITEMASK"	},
{	0x8CA6,	"GL_DRAW_FRAMEBUFFER_BINDING_EXT"	},
{	0x8CA6,	"GL_FRAMEBUFFER_BINDING_EXT"	},
{	0x8CA6,	"GL_FRAMEBUFFER_BINDING"	},
{	0x8CA7,	"GL_RENDERBUFFER_BINDING_EXT"	},
{	0x8CA7,	"GL_RENDERBUFFER_BINDING"	},
{	0x8CA8,	"GL_READ_FRAMEBUFFER_EXT"	},
{	0x8CA8,	"GL_READ_FRAMEBUFFER"	},
{	0x8CA9,	"GL_DRAW_FRAMEBUFFER_EXT"	},
{	0x8CA9,	"GL_DRAW_FRAMEBUFFER"	},
{	0x8CAA,	"GL_READ_FRAMEBUFFER_BINDING_EXT"	},
{	0x8CAA,	"GL_READ_FRAMEBUFFER_BINDING"	},
{	0x8CAB,	"GL_RENDERBUFFER_SAMPLES_EXT"	},
{	0x8CAB,	"GL_RENDERBUFFER_SAMPLES"	},
{	0x8CAC,	"GL_DEPTH_COMPONENT32F"	},
{	0x8CAD,	"GL_DEPTH32F_STENCIL8"	},
{	0x8CD0,	"GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT"	},
{	0x8CD0,	"GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE"	},
{	0x8CD1,	"GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT"	},
{	0x8CD1,	"GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME"	},
{	0x8CD2,	"GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_EXT"	},
{	0x8CD2,	"GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL"	},
{	0x8CD3,	"GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_EXT"	},
{	0x8CD3,	"GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE"	},
{	0x8CD4,	"GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_EXT"	},
{	0x8CD4,	"GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER_EXT"	},
{	0x8CD4,	"GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER"	},
{	0x8CD5,	"GL_FRAMEBUFFER_COMPLETE_EXT"	},
{	0x8CD5,	"GL_FRAMEBUFFER_COMPLETE"	},
{	0x8CD6,	"GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT"	},
{	0x8CD6,	"GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT"	},
{	0x8CD7,	"GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT"	},
{	0x8CD7,	"GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT"	},
{	0x8CD9,	"GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT"	},
{	0x8CDA,	"GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT"	},
{	0x8CDB,	"GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT"	},
{	0x8CDB,	"GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER"	},
{	0x8CDC,	"GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT"	},
{	0x8CDC,	"GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER"	},
{	0x8CDD,	"GL_FRAMEBUFFER_UNSUPPORTED_EXT"	},
{	0x8CDD,	"GL_FRAMEBUFFER_UNSUPPORTED"	},
{	0x8CDF,	"GL_MAX_COLOR_ATTACHMENTS_EXT"	},
{	0x8CDF,	"GL_MAX_COLOR_ATTACHMENTS"	},
{	0x8CE0,	"GL_COLOR_ATTACHMENT0_EXT"	},
{	0x8CE0,	"GL_COLOR_ATTACHMENT0"	},
{	0x8CE1,	"GL_COLOR_ATTACHMENT1_EXT"	},
{	0x8CE1,	"GL_COLOR_ATTACHMENT1"	},
{	0x8CE2,	"GL_COLOR_ATTACHMENT2_EXT"	},
{	0x8CE2,	"GL_COLOR_ATTACHMENT2"	},
{	0x8CE3,	"GL_COLOR_ATTACHMENT3_EXT"	},
{	0x8CE3,	"GL_COLOR_ATTACHMENT3"	},
{	0x8CE4,	"GL_COLOR_ATTACHMENT4_EXT"	},
{	0x8CE4,	"GL_COLOR_ATTACHMENT4"	},
{	0x8CE5,	"GL_COLOR_ATTACHMENT5_EXT"	},
{	0x8CE5,	"GL_COLOR_ATTACHMENT5"	},
{	0x8CE6,	"GL_COLOR_ATTACHMENT6_EXT"	},
{	0x8CE6,	"GL_COLOR_ATTACHMENT6"	},
{	0x8CE7,	"GL_COLOR_ATTACHMENT7_EXT"	},
{	0x8CE7,	"GL_COLOR_ATTACHMENT7"	},
{	0x8CE8,	"GL_COLOR_ATTACHMENT8_EXT"	},
{	0x8CE8,	"GL_COLOR_ATTACHMENT8"	},
{	0x8CE9,	"GL_COLOR_ATTACHMENT9_EXT"	},
{	0x8CE9,	"GL_COLOR_ATTACHMENT9"	},
{	0x8CEA,	"GL_COLOR_ATTACHMENT10_EXT"	},
{	0x8CEA,	"GL_COLOR_ATTACHMENT10"	},
{	0x8CEB,	"GL_COLOR_ATTACHMENT11_EXT"	},
{	0x8CEB,	"GL_COLOR_ATTACHMENT11"	},
{	0x8CEC,	"GL_COLOR_ATTACHMENT12_EXT"	},
{	0x8CEC,	"GL_COLOR_ATTACHMENT12"	},
{	0x8CED,	"GL_COLOR_ATTACHMENT13_EXT"	},
{	0x8CED,	"GL_COLOR_ATTACHMENT13"	},
{	0x8CEE,	"GL_COLOR_ATTACHMENT14_EXT"	},
{	0x8CEE,	"GL_COLOR_ATTACHMENT14"	},
{	0x8CEF,	"GL_COLOR_ATTACHMENT15_EXT"	},
{	0x8CEF,	"GL_COLOR_ATTACHMENT15"	},
{	0x8D00,	"GL_DEPTH_ATTACHMENT_EXT"	},
{	0x8D00,	"GL_DEPTH_ATTACHMENT"	},
{	0x8D20,	"GL_STENCIL_ATTACHMENT_EXT"	},
{	0x8D20,	"GL_STENCIL_ATTACHMENT"	},
{	0x8D40,	"GL_FRAMEBUFFER_EXT"	},
{	0x8D40,	"GL_FRAMEBUFFER"	},
{	0x8D41,	"GL_RENDERBUFFER_EXT"	},
{	0x8D41,	"GL_RENDERBUFFER"	},
{	0x8D42,	"GL_RENDERBUFFER_WIDTH_EXT"	},
{	0x8D42,	"GL_RENDERBUFFER_WIDTH"	},
{	0x8D43,	"GL_RENDERBUFFER_HEIGHT_EXT"	},
{	0x8D43,	"GL_RENDERBUFFER_HEIGHT"	},
{	0x8D44,	"GL_RENDERBUFFER_INTERNAL_FORMAT_EXT"	},
{	0x8D44,	"GL_RENDERBUFFER_INTERNAL_FORMAT"	},
{	0x8D46,	"GL_STENCIL_INDEX1_EXT"	},
{	0x8D46,	"GL_STENCIL_INDEX1"	},
{	0x8D47,	"GL_STENCIL_INDEX4_EXT"	},
{	0x8D47,	"GL_STENCIL_INDEX4"	},
{	0x8D48,	"GL_STENCIL_INDEX8_EXT"	},
{	0x8D48,	"GL_STENCIL_INDEX8"	},
{	0x8D49,	"GL_STENCIL_INDEX16_EXT"	},
{	0x8D49,	"GL_STENCIL_INDEX16"	},
{	0x8D50,	"GL_RENDERBUFFER_RED_SIZE_EXT"	},
{	0x8D50,	"GL_RENDERBUFFER_RED_SIZE"	},
{	0x8D51,	"GL_RENDERBUFFER_GREEN_SIZE_EXT"	},
{	0x8D51,	"GL_RENDERBUFFER_GREEN_SIZE"	},
{	0x8D52,	"GL_RENDERBUFFER_BLUE_SIZE_EXT"	},
{	0x8D52,	"GL_RENDERBUFFER_BLUE_SIZE"	},
{	0x8D53,	"GL_RENDERBUFFER_ALPHA_SIZE_EXT"	},
{	0x8D53,	"GL_RENDERBUFFER_ALPHA_SIZE"	},
{	0x8D54,	"GL_RENDERBUFFER_DEPTH_SIZE_EXT"	},
{	0x8D54,	"GL_RENDERBUFFER_DEPTH_SIZE"	},
{	0x8D55,	"GL_RENDERBUFFER_STENCIL_SIZE_EXT"	},
{	0x8D55,	"GL_RENDERBUFFER_STENCIL_SIZE"	},
{	0x8D56,	"GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT"	},
{	0x8D56,	"GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE"	},
{	0x8D57,	"GL_MAX_SAMPLES_EXT"	},
{	0x8D57,	"GL_MAX_SAMPLES"	},
{	0x8D70,	"GL_RGBA32UI_EXT"	},
{	0x8D71,	"GL_RGB32UI_EXT"	},
{	0x8D72,	"GL_ALPHA32UI_EXT"	},
{	0x8D73,	"GL_INTENSITY32UI_EXT"	},
{	0x8D74,	"GL_LUMINANCE32UI_EXT"	},
{	0x8D75,	"GL_LUMINANCE_ALPHA32UI_EXT"	},
{	0x8D76,	"GL_RGBA16UI_EXT"	},
{	0x8D77,	"GL_RGB16UI_EXT"	},
{	0x8D78,	"GL_ALPHA16UI_EXT"	},
{	0x8D79,	"GL_INTENSITY16UI_EXT"	},
{	0x8D7A,	"GL_LUMINANCE16UI_EXT"	},
{	0x8D7B,	"GL_LUMINANCE_ALPHA16UI_EXT"	},
{	0x8D7C,	"GL_RGBA8UI_EXT"	},
{	0x8D7D,	"GL_RGB8UI_EXT"	},
{	0x8D7E,	"GL_ALPHA8UI_EXT"	},
{	0x8D7F,	"GL_INTENSITY8UI_EXT"	},
{	0x8D80,	"GL_LUMINANCE8UI_EXT"	},
{	0x8D81,	"GL_LUMINANCE_ALPHA8UI_EXT"	},
{	0x8D82,	"GL_RGBA32I_EXT"	},
{	0x8D83,	"GL_RGB32I_EXT"	},
{	0x8D84,	"GL_ALPHA32I_EXT"	},
{	0x8D85,	"GL_INTENSITY32I_EXT"	},
{	0x8D86,	"GL_LUMINANCE32I_EXT"	},
{	0x8D87,	"GL_LUMINANCE_ALPHA32I_EXT"	},
{	0x8D88,	"GL_RGBA16I_EXT"	},
{	0x8D89,	"GL_RGB16I_EXT"	},
{	0x8D8A,	"GL_ALPHA16I_EXT"	},
{	0x8D8B,	"GL_INTENSITY16I_EXT"	},
{	0x8D8C,	"GL_LUMINANCE16I_EXT"	},
{	0x8D8D,	"GL_LUMINANCE_ALPHA16I_EXT"	},
{	0x8D8E,	"GL_RGBA8I_EXT"	},
{	0x8D8F,	"GL_RGB8I_EXT"	},
{	0x8D90,	"GL_ALPHA8I_EXT"	},
{	0x8D91,	"GL_INTENSITY8I_EXT"	},
{	0x8D92,	"GL_LUMINANCE8I_EXT"	},
{	0x8D93,	"GL_LUMINANCE_ALPHA8I_EXT"	},
{	0x8D94,	"GL_RED_INTEGER_EXT"	},
{	0x8D95,	"GL_GREEN_INTEGER_EXT"	},
{	0x8D96,	"GL_BLUE_INTEGER_EXT"	},
{	0x8D97,	"GL_ALPHA_INTEGER_EXT"	},
{	0x8D98,	"GL_RGB_INTEGER_EXT"	},
{	0x8D99,	"GL_RGBA_INTEGER_EXT"	},
{	0x8D9A,	"GL_BGR_INTEGER_EXT"	},
{	0x8D9B,	"GL_BGRA_INTEGER_EXT"	},
{	0x8D9C,	"GL_LUMINANCE_INTEGER_EXT"	},
{	0x8D9D,	"GL_LUMINANCE_ALPHA_INTEGER_EXT"	},
{	0x8D9E,	"GL_RGBA_INTEGER_MODE_EXT"	},
{	0x8DA7,	"GL_FRAMEBUFFER_ATTACHMENT_LAYERED_EXT"	},
{	0x8DA8,	"GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT"	},
{	0x8DA9,	"GL_FRAMEBUFFER_INCOMPLETE_LAYER_COUNT_EXT"	},
{	0x8DAD,	"GL_FLOAT_32_UNSIGNED_INT_24_8_REV"	},
{	0x8DB9,	"GL_FRAMEBUFFER_SRGB_EXT"	},
{	0x8DBA,	"GL_FRAMEBUFFER_SRGB_CAPABLE_EXT"	},
{	0x8DBB,	"GL_COMPRESSED_RED_RGTC1"	},
{	0x8DBC,	"GL_COMPRESSED_SIGNED_RED_RGTC1"	},
{	0x8DBD,	"GL_COMPRESSED_RG_RGTC2"	},
{	0x8DBE,	"GL_COMPRESSED_SIGNED_RG_RGTC2"	},
{	0x8DC0,	"GL_SAMPLER_1D_ARRAY_EXT"	},
{	0x8DC1,	"GL_SAMPLER_2D_ARRAY_EXT"	},
{	0x8DC2,	"GL_SAMPLER_BUFFER_EXT"	},
{	0x8DC3,	"GL_SAMPLER_1D_ARRAY_SHADOW_EXT"	},
{	0x8DC4,	"GL_SAMPLER_2D_ARRAY_SHADOW_EXT"	},
{	0x8DC5,	"GL_SAMPLER_CUBE_SHADOW_EXT"	},
{	0x8DC6,	"GL_UNSIGNED_INT_VEC2_EXT"	},
{	0x8DC7,	"GL_UNSIGNED_INT_VEC3_EXT"	},
{	0x8DC8,	"GL_UNSIGNED_INT_VEC4_EXT"	},
{	0x8DC9,	"GL_INT_SAMPLER_1D_EXT"	},
{	0x8DCA,	"GL_INT_SAMPLER_2D_EXT"	},
{	0x8DCB,	"GL_INT_SAMPLER_3D_EXT"	},
{	0x8DCC,	"GL_INT_SAMPLER_CUBE_EXT"	},
{	0x8DCD,	"GL_INT_SAMPLER_2D_RECT_EXT"	},
{	0x8DCE,	"GL_INT_SAMPLER_1D_ARRAY_EXT"	},
{	0x8DCF,	"GL_INT_SAMPLER_2D_ARRAY_EXT"	},
{	0x8DD0,	"GL_INT_SAMPLER_BUFFER_EXT"	},
{	0x8DD1,	"GL_UNSIGNED_INT_SAMPLER_1D_EXT"	},
{	0x8DD2,	"GL_UNSIGNED_INT_SAMPLER_2D_EXT"	},
{	0x8DD3,	"GL_UNSIGNED_INT_SAMPLER_3D_EXT"	},
{	0x8DD4,	"GL_UNSIGNED_INT_SAMPLER_CUBE_EXT"	},
{	0x8DD5,	"GL_UNSIGNED_INT_SAMPLER_2D_RECT_EXT"	},
{	0x8DD6,	"GL_UNSIGNED_INT_SAMPLER_1D_ARRAY_EXT"	},
{	0x8DD7,	"GL_UNSIGNED_INT_SAMPLER_2D_ARRAY_EXT"	},
{	0x8DD8,	"GL_UNSIGNED_INT_SAMPLER_BUFFER_EXT"	},
{	0x8DD9,	"GL_GEOMETRY_SHADER_EXT"	},
{	0x8DDA,	"GL_GEOMETRY_VERTICES_OUT_EXT"	},
{	0x8DDB,	"GL_GEOMETRY_INPUT_TYPE_EXT"	},
{	0x8DDC,	"GL_GEOMETRY_OUTPUT_TYPE_EXT"	},
{	0x8DDD,	"GL_MAX_GEOMETRY_VARYING_COMPONENTS_EXT"	},
{	0x8DDE,	"GL_MAX_VERTEX_VARYING_COMPONENTS_EXT"	},
{	0x8DDF,	"GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_EXT"	},
{	0x8DE0,	"GL_MAX_GEOMETRY_OUTPUT_VERTICES_EXT"	},
{	0x8DE1,	"GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_EXT"	},
{	0x8DE2,	"GL_MAX_VERTEX_BINDABLE_UNIFORMS_EXT"	},
{	0x8DE3,	"GL_MAX_FRAGMENT_BINDABLE_UNIFORMS_EXT"	},
{	0x8DE4,	"GL_MAX_GEOMETRY_BINDABLE_UNIFORMS_EXT"	},
{	0x8DED,	"GL_MAX_BINDABLE_UNIFORM_SIZE_EXT"	},
{	0x8DEE,	"GL_UNIFORM_BUFFER_EXT"	},
{	0x8DEF,	"GL_UNIFORM_BUFFER_BINDING_EXT"	},
{	0x8E4C,	"GL_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION_EXT"	},
{	0x8E4D,	"GL_FIRST_VERTEX_CONVENTION_EXT"	},
{	0x8E4E,	"GL_LAST_VERTEX_CONVENTION_EXT"	},
{	0x8E4F,	"GL_PROVOKING_VERTEX_EXT"	}
};

const char *get_enum_str(uint val)
{
	for(int i = 0; i < ARRAYSIZE(g_glEnums); i++)
	{
		if( g_glEnums[i].value == val )
			return g_glEnums[i].str;
	}
	return "UNKNOWN";
}

typedef union {
    uint16_t bin;
    struct {
        uint16_t sign:1;
        uint16_t exp:5;
        uint16_t mant:10;
    } x;
} halffloat_t;

typedef union {
    float f;
    uint32_t bin;
    struct {
        uint32_t sign:1;
        uint32_t exp:8;
        uint32_t mant:23;
    } x;
} fullfloat_t;

static inline float float_h2f(halffloat_t t)
{
    fullfloat_t tmp;
    tmp.x.sign = t.x.sign;  // copy sign
    if(t.x.exp==0 /*&& t.mant==0*/) {
    // 0 and denormal?
        tmp.x.exp=0;
        tmp.x.mant=0;
    } else if (t.x.exp==31) {
    // Inf / NaN
        tmp.x.exp=255;
        tmp.x.mant=(t.x.mant<<13);
    } else {
        tmp.x.mant=(t.x.mant<<13);
        tmp.x.exp = t.x.exp+0x38;
    }

    return tmp.f;
}

static inline halffloat_t float_f2h(float f)
{
    fullfloat_t tmp;
    halffloat_t ret;
    tmp.f = f;
    ret.x.sign = tmp.x.sign;
    if (tmp.x.exp == 0) {
        // O and denormal
        ret.bin = 0;
    } else if (tmp.x.exp==255) {
        // Inf / NaN
        ret.x.exp = 31;
        ret.x.mant = tmp.x.mant>>13;
    } else if(tmp.x.exp>0x71) {
        // flush to 0
        ret.x.exp = 0;
        ret.x.mant = 0;
    } else if(tmp.x.exp<0x8e) {
        // clamp to max
        ret.x.exp = 30;
        ret.x.mant = 1023;
    } else {
        ret.x.exp = tmp.x.exp - 38;
        ret.x.mant = tmp.x.mant>>13;
    }

    return ret;
}

void convert_texture( GLenum &internalformat, GLsizei width, GLsizei height, GLenum &format, GLenum &type, void *data )
{
	if( format == GL_BGRA ) format = GL_RGBA;
	if( format == GL_BGR ) format = GL_RGB;

	if( internalformat == GL_SRGB8 && format == GL_RGBA )
		internalformat = GL_SRGB8_ALPHA8;

	if( format == GL_LUMINANCE || format == GL_LUMINANCE_ALPHA )
		internalformat = format;

	if( data )
	{
		if( internalformat == GL_RGBA16 && !gGL->m_bHave_GL_EXT_texture_norm16 )
		{
			uint16_t *_data = (uint16_t*)data;
			uint8_t *new_data = (uint8_t*)data;

			for( int i = 0; i < width*height*4; i+=4 )
			{
				new_data[i] = _data[i] >> 8;
				new_data[i+1] = _data[i+1] >> 8;
				new_data[i+2] = _data[i+2] >> 8;
				new_data[i+3] = _data[i+3] >> 8;
			}
		}
	}

	if( internalformat == GL_RGBA16 && !gGL->m_bHave_GL_EXT_texture_norm16 )
	{
		internalformat = GL_RGBA;
		format = GL_RGBA;
		type = GL_UNSIGNED_BYTE;
	}

	if( type == GL_UNSIGNED_INT_8_8_8_8_REV )
		type = GL_UNSIGNED_BYTE;
}

GLboolean isDXTc(GLenum format) {
    switch (format) {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            return 1;
    }
    return 0;
}

GLboolean isDXTcSRGB(GLenum format) {
    switch (format) {
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            return 1;
    }
    return 0;
}

static GLboolean isDXTcAlpha(GLenum format) {
    switch (format) {
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            return 1;
    }
    return 0;
}

GLvoid *uncompressDXTc(GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, int transparent0, int* simpleAlpha, int* complexAlpha, const GLvoid *data) {
    // uncompress a DXTc image
    // get pixel size of uncompressed image => fixed RGBA
    int pixelsize = 4;
/*	if (format==COMPRESSED_RGB_S3TC_DXT1_EXT)
        pixelsize = 3;*/
    // check with the size of the input data stream if the stream is in fact uncompressed
    if (imageSize == width*height*pixelsize || data==NULL) {
        // uncompressed stream
        return (GLvoid*)data;
    }
    // alloc memory
    GLvoid *pixels = malloc(((width+3)&~3)*((height+3)&~3)*pixelsize);
    // uncompress loop
    int blocksize;
    switch (format) {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
            blocksize = 8;
            break;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            blocksize = 16;
            break;
    }
    uintptr_t src = (uintptr_t) data;
    for (int y=0; y<height; y+=4) {
        for (int x=0; x<width; x+=4) {
            switch(format) {
                case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
                case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
                case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
                    DecompressBlockDXT1(x, y, width, (uint8_t*)src, transparent0, simpleAlpha, complexAlpha, (uint32_t*)pixels);
                    break;
                case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
                case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
                    DecompressBlockDXT3(x, y, width, (uint8_t*)src, transparent0, simpleAlpha, complexAlpha, (uint32_t*)pixels);
                    break;
                case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
                    DecompressBlockDXT5(x, y, width, (uint8_t*)src, transparent0, simpleAlpha, complexAlpha, (uint32_t*)pixels);
                    break;
            }
            src+=blocksize;
        }
    }
    return pixels;
}

void CompressedTexImage2D(GLenum target, GLint level, GLenum internalformat,
                            GLsizei width, GLsizei height, GLint border,
                            GLsizei imageSize, const GLvoid *data) 
{
    if (internalformat==GL_RGBA8)
        internalformat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;

	if ((width<=0) || (height<=0)) {
        return;
    }
	
	GLenum format = GL_RGBA;
	GLenum intformat = GL_RGBA;	
    GLenum type = GL_UNSIGNED_BYTE;
	GLvoid *pixels = NULL;        
	
    if (isDXTc(internalformat)) {
        pixels = NULL;
        type = GL_UNSIGNED_BYTE;

        int srgb = isDXTcSRGB(internalformat);
        int simpleAlpha = 0;
        int complexAlpha = 0;
        int transparent0 = (internalformat==GL_COMPRESSED_RGBA_S3TC_DXT1_EXT || internalformat==GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT)?1:0;
        if (data) {
            pixels = uncompressDXTc(width, height, internalformat, imageSize, transparent0, &simpleAlpha, &complexAlpha, data);
        } else {
            if(isDXTcAlpha(internalformat)) {
                simpleAlpha = complexAlpha = 1;
            }
        }

		if( srgb )
			intformat = GL_SRGB8_ALPHA8;
	}

	gGL->glTexImage2D(target, level, intformat, width, height, border, format, type, pixels);
	if( data != pixels )
		free(pixels);
}

// TexSubImage should work properly on every driver stack and GPU--enabling by default.
ConVar	gl_enabletexsubimage( "gl_enabletexsubimage", "1" );

void CGLMTex::WriteTexels( GLMTexLockDesc *desc, bool writeWholeSlice, bool noDataWrite )
{
	//if ( m_nBindlessHashNumEntries )
	//	return;
	
	GLMRegion	writeBox;

	bool needsExpand = false;
	char *expandTemp = NULL;
	
	switch( m_layout->m_format->m_d3dFormat)
	{
		case D3DFMT_V8U8:
		{
			needsExpand = true;
			writeWholeSlice = true;
			
			// shoot down client storage if we have to generate a new flavor of the data
			m_texClientStorage = false;
		}
		break;
	}
	
	if (writeWholeSlice)
	{
		writeBox.xmin = writeBox.ymin = writeBox.zmin = 0;

		writeBox.xmax = m_layout->m_slices[ desc->m_sliceIndex ].m_xSize;
		writeBox.ymax = m_layout->m_slices[ desc->m_sliceIndex ].m_ySize;
		writeBox.zmax = m_layout->m_slices[ desc->m_sliceIndex ].m_zSize;
	}
	else
	{
		writeBox = desc->m_req.m_region;
	}

	// first thing is to get the GL texture bound to a TMU, or just select one if already bound
	// to get this running we will just always slam TMU 0 and let the draw time code fix it back
	// a later optimization would be to hoist the bind call to the caller, do it exactly once
	
	CGLMTex *pPrevTex = m_ctx->m_samplers[0].m_pBoundTex;
	m_ctx->BindTexToTMU( this, 0 );		// SelectTMU(n) is a side effect

	GLMTexFormatDesc *format = m_layout->m_format;
	
	GLenum target		= m_layout->m_key.m_texGLTarget;
	GLenum glDataFormat	= format->m_glDataFormat;				// this could change if expansion kicks in 
	GLenum glDataType	= format->m_glDataType;
	
	GLMTexLayoutSlice *slice = &m_layout->m_slices[ desc->m_sliceIndex ];
	void *sliceAddress = m_backing ? (m_backing + slice->m_storageOffset) : NULL;	// this would change for PBO

	// allow use of subimage if the target is texture2D and it has already been teximage'd
	bool mayUseSubImage = false;

	if ( (target==GL_TEXTURE_2D) && (m_sliceFlags[ desc->m_sliceIndex ] & kSliceValid) )
		mayUseSubImage = true;

	// check flavor, 2D, 3D, or cube map
	// we also have the choice to use subimage if this is a tex already created. (open question as to benefit)
	
	
	// SRGB select. At this level (writetexels) we firmly obey the m_texFlags.
	// (mechanism not policy)
	
	GLenum intformat = (m_layout->m_key.m_texFlags & kGLMTexSRGB) ? format->m_glIntFormatSRGB : format->m_glIntFormat;
	if (CommandLine()->FindParm("-disable_srgbtex"))
	{
		// force non srgb flavor - experiment to make ATI r600 happy on 10.5.8 (maybe x1600 too!)
		intformat = format->m_glIntFormat;
	}
	
	Assert( intformat != 0 );
	
	if (m_layout->m_key.m_texFlags & kGLMTexSRGB)
	{
		Assert( m_layout->m_format->m_glDataFormat != GL_DEPTH_COMPONENT );
		Assert( m_layout->m_format->m_glDataFormat != GL_DEPTH_STENCIL );
		Assert( m_layout->m_format->m_glDataFormat != GL_ALPHA );
	}
	
	// adjust min and max mip written
	if (desc->m_req.m_mip > m_maxActiveMip)
	{
		m_maxActiveMip = desc->m_req.m_mip;

		gGL->glTexParameteri( target, GL_TEXTURE_MAX_LEVEL, desc->m_req.m_mip);
	}
	
	if (desc->m_req.m_mip < m_minActiveMip)
	{
		m_minActiveMip = desc->m_req.m_mip;
		
		gGL->glTexParameteri( target, GL_TEXTURE_BASE_LEVEL, desc->m_req.m_mip);
	}

	if (needsExpand)
	{
		int expandSize = 0;
		
		switch( m_layout->m_format->m_d3dFormat)
		{
			case D3DFMT_V8U8:
			{
				// figure out new size based on 3byte RGB format
				// easy, just take the two byte size and grow it by 50%
				expandSize = (slice->m_storageSize * 3) / 2;
				expandTemp = (char*)malloc( expandSize );
				
				char *src = (char*)sliceAddress;
				char *dst = expandTemp;

				// transfer RG's to RGB's
				while(expandSize>0)
				{
					*dst = *src++;	// move first byte
					*dst = *src++;	// move second byte
					*reinterpret_cast<uint8*>(dst) = 0xBB;	// pad third byte
					
					expandSize -= 3;
				}
				
				// move the slice pointer
				sliceAddress = expandTemp;
				
				// change the data format we tell GL about
				glDataFormat = GL_RGB;
			}
			break;
			
			default:	Assert(!"Don't know how to expand that format..");
		}
		
	}

	// set up the client storage now, one way or another
	// If this extension isn't supported, we just end up with two copies of the texture, one in the GL and one in app memory.
	//  So it's safe to just go on as if this extension existed and hold the possibly-unnecessary extra RAM.

	switch( target )
	{
		case GL_TEXTURE_CUBE_MAP:

			// adjust target to steer to the proper face, then fall through to the 2D texture path.
			target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + desc->m_req.m_face;
		case GL_TEXTURE_2D:
		{
			// check compressed or not
			if (format->m_chunkSize != 1)
			{
				Assert( writeWholeSlice );	//subimage not implemented in this path yet
				// compressed path
				// http://www.opengl.org/sdk/docs/man/xhtml/glCompressedTexImage2D.xml
				if( gGL->m_bHave_GL_EXT_texture_compression_dxt1 )
					gGL->glCompressedTexImage2D( target, desc->m_req.m_mip, intformat, slice->m_xSize, slice->m_ySize, 0, slice->m_storageSize, sliceAddress );
				else
					CompressedTexImage2D( target, desc->m_req.m_mip, intformat, slice->m_xSize, slice->m_ySize, 0, slice->m_storageSize, sliceAddress );
			}
			else
			{
				if (mayUseSubImage)
				{
					// go subimage2D if it's a replacement, not a creation

					gGL->glPixelStorei( GL_UNPACK_ROW_LENGTH, slice->m_xSize );			// in pixels
					gGL->glPixelStorei( GL_UNPACK_SKIP_PIXELS, writeBox.xmin );		// in pixels
					gGL->glPixelStorei( GL_UNPACK_SKIP_ROWS, writeBox.ymin );		// in pixels

					convert_texture(intformat, writeBox.xmax - writeBox.xmin, writeBox.ymax - writeBox.ymin, glDataFormat, glDataType, sliceAddress);

					gGL->glTexSubImage2D(	target,
										desc->m_req.m_mip,				// level
										writeBox.xmin,					// xoffset into dest
										writeBox.ymin,					// yoffset into dest
										writeBox.xmax - writeBox.xmin,	// width	(was slice->m_xSize)
										writeBox.ymax - writeBox.ymin,	// height	(was slice->m_ySize)
										glDataFormat,					// format
										glDataType,						// type
										sliceAddress					// data (will be offsetted by the SKIP_PIXELS and SKIP_ROWS - let GL do the math to find the first source texel)
										);					

					gGL->glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
					gGL->glPixelStorei( GL_UNPACK_SKIP_PIXELS, 0 );
					gGL->glPixelStorei( GL_UNPACK_SKIP_ROWS, 0 );				
				}
				else
				{					
					// uncompressed path
					// http://www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/teximage2d.html
					convert_texture(intformat, m_layout->m_slices[ desc->m_sliceIndex ].m_xSize, m_layout->m_slices[ desc->m_sliceIndex ].m_ySize, glDataFormat, glDataType, noDataWrite ? NULL : sliceAddress);
					
					gGL->glTexImage2D(			target,						// target
											desc->m_req.m_mip,			// level
											intformat,					// internalformat - don't use format->m_glIntFormat because we have the SRGB select going on above
											slice->m_xSize,				// width
											slice->m_ySize,				// height
											0,							// border
											glDataFormat,				// dataformat
											glDataType,					// datatype
											noDataWrite ? NULL : sliceAddress );	// data (optionally suppressed in case ResetSRGB desires)

					if (m_layout->m_key.m_texFlags & kGLMTexMultisampled)
					{
						if (gl_texmsaalog.GetInt())
						{
							printf( "\n == MSAA Tex %p %s : glTexImage2D for flat tex using intformat %s (%x)", this, m_debugLabel?m_debugLabel:"", GLMDecode( eGL_ENUM, intformat ), intformat );
							printf( "\n" );			
						}
					}

					m_sliceFlags[ desc->m_sliceIndex ] |= kSliceValid; // for next time, we can subimage..
				}
			}
		}
		break;
			
		case GL_TEXTURE_3D:
		{
			// check compressed or not
			if (format->m_chunkSize != 1)
			{
				// compressed path
				// http://www.opengl.org/sdk/docs/man/xhtml/glCompressedTexImage3D.xml
				
				gGL->glCompressedTexImage3D(	target,						// target
										desc->m_req.m_mip,			// level
										intformat,					// internalformat
										slice->m_xSize,				// width
										slice->m_ySize,				// height
										slice->m_zSize,				// depth
										0,							// border
										slice->m_storageSize,		// imageSize
										sliceAddress );				// data
			}
			else
			{
				convert_texture(intformat, m_layout->m_slices[ desc->m_sliceIndex ].m_xSize, m_layout->m_slices[ desc->m_sliceIndex ].m_ySize, glDataFormat, glDataType, noDataWrite ? NULL : sliceAddress);				
				gGL->glTexImage3D(			target,						// target
										desc->m_req.m_mip,			// level
										intformat,					// internalformat
										slice->m_xSize,				// width
										slice->m_ySize,				// height
										slice->m_zSize,				// depth
										0,							// border
										glDataFormat,				// dataformat
										glDataType,					// datatype
										noDataWrite ? NULL : sliceAddress );	// data (optionally suppressed in case ResetSRGB desires)
			}
		}
		break;
	}

	if ( expandTemp )
	{
		free( expandTemp );
	}

	m_ctx->BindTexToTMU( pPrevTex, 0 );
}
	

void CGLMTex::Lock( GLMTexLockParams *params, char** addressOut, int* yStrideOut, int *zStrideOut )
{
#if GL_TELEMETRY_GPU_ZONES
	CScopedGLMPIXEvent glmPIXEvent( "CGLMTex::Lock" );
	g_TelemetryGPUStats.m_nTotalTexLocksAndUnlocks++;
#endif

	// locate appropriate slice in layout record
	int sliceIndex = CalcSliceIndex( params->m_face, params->m_mip );
	
	GLMTexLayoutSlice *slice = &m_layout->m_slices[sliceIndex];

	// obtain offset
	//int sliceBaseOffset = slice->m_storageOffset;
	
	// cross check region req against slice bounds - figure out if it matches, exceeds, or is less than the whole slice.
	char exceed =	(params->m_region.xmin < 0) || (params->m_region.xmax > slice->m_xSize) || 
					(params->m_region.ymin < 0) || (params->m_region.ymax > slice->m_ySize) || 
					(params->m_region.zmin < 0) || (params->m_region.zmax > slice->m_zSize);

	char partial =	(params->m_region.xmin > 0) || (params->m_region.xmax < slice->m_xSize) ||
					(params->m_region.ymin > 0) || (params->m_region.ymax < slice->m_ySize) ||
					(params->m_region.zmin > 0) || (params->m_region.zmax < slice->m_zSize);
					
	bool	copyout = false;	// set if a readback of the texture slice from GL is needed

	if (exceed)
	{
		// illegal rect, out of bounds		
		GLMStop();
	}
	
	// on return, these things need to be true
	
	// a - there needs to be storage allocated, which we will return an address within
	// b - the region corresponding to the slice being locked, will have valid data there for the whole slice.
	// c - the slice is marked as locked
	// d - the params of the lock request have been saved in the lock table (in the context)
	
	// so step 1 is unambiguous.  If there's no backing storage, make some.
	if (!m_backing)
	{
		if ( gl_pow2_tempmem.GetBool() )
		{
			uint32_t unStoragePow2 = m_layout->m_storageTotalSize;
			// Round up to next power of 2
			unStoragePow2--;
			unStoragePow2 |= unStoragePow2 >> 1;
			unStoragePow2 |= unStoragePow2 >> 2;
			unStoragePow2 |= unStoragePow2 >> 4;
			unStoragePow2 |= unStoragePow2 >> 8;
			unStoragePow2 |= unStoragePow2 >> 16;
			unStoragePow2++;
			m_backing = (char *)calloc( unStoragePow2, 1 );
		}
		else
		{
			m_backing = (char *)calloc( m_layout->m_storageTotalSize, 1 );
		}

		// clear the kSliceStorageValid bit on all slices
		for( int i=0; i<m_layout->m_sliceCount; i++)
		{
			m_sliceFlags[i] &= ~kSliceStorageValid;
		}
	}
	
	// work on this slice now
	
	// storage is known to exist at this point, but we need to check if its contents are valid for this slice.
	// this is tracked per-slice so we don't hoist all the texels back out of GL across all slices if caller only
	// wanted to lock some of them.
	
	// (i.e. if we just alloced it, it's blank)
	// if storage is invalid, but the texture itself is valid, hoist the texels back to the storage and mark it valid.
	// if storage is invalid, and texture itself is also invalid, go ahead and mark storage as valid and fully dirty... to force teximage.
	
	//	???????????? we need to go over this more carefully re "slice valid" (it has been teximaged) vs "storage valid" (it has been copied out).
		
	unsigned char *sliceFlags = &m_sliceFlags[ sliceIndex ];
	
	if (params->m_readback)
	{
		// caller is letting us know that it wants to readback the real texels.
		*sliceFlags |= kSliceStorageValid;
		*sliceFlags |= kSliceValid;
		*sliceFlags &= ~(kSliceFullyDirty);
		copyout = true;
	}
	else
	{
		// caller is pushing texels.
		if (! (*sliceFlags & kSliceStorageValid) )
		{
			// storage is invalid.  check texture state
			if ( *sliceFlags & kSliceValid )
			{
				// kSliceValid set: the texture itself has a valid slice, but we don't have it in our backing copy, so copy it out.
				copyout = true;
			}
			else
			{
				// kSliceValid not set: the texture does not have a valid slice to copy out - it hasn't been teximage'd yet.
				// set the "full dirty" bit to make sure we teximage the whole thing on unlock.
				*sliceFlags |= kSliceFullyDirty;
				
				// assert if they did not ask to lock the full slice size on this go-round
				if (partial)
				{
					// choice here - 
					// 1 - stop cold, we don't know how to subimage yet.
					// 2 - grin and bear it, mark whole slice dirty (ah, we already did... so, do nothing).
					// choice 2: // GLMStop();
				}
			}
			
			// one way or another, upon reaching here the slice storage is valid for read.
			*sliceFlags |= kSliceStorageValid;
		}
	}

	
	// when we arrive here, there is storage, and the content of the storage for this slice is valid
	// (or zeroes if it's the first lock)

	// log the lock request in the context.
	int newdesc = m_ctx->m_texLocks.AddToTail();
	
	GLMTexLockDesc *desc = &m_ctx->m_texLocks[newdesc];
	
	desc->m_req = *params;
	desc->m_active = true;
	desc->m_sliceIndex = sliceIndex;
	desc->m_sliceBaseOffset = m_layout->m_slices[sliceIndex].m_storageOffset;

	// to calculate the additional offset we need to look at the rect's min corner
	// combined with the per-texel size and Y/Z stride
	// also cross check it for 4x multiple if there is compression in play
	
	int offsetInSlice = 0;
	int	yStride = 0;
	int zStride = 0;
	
	CalcTexelDataOffsetAndStrides( sliceIndex, params->m_region.xmin, params->m_region.ymin, params->m_region.zmin, &offsetInSlice, &yStride, &zStride );

	// for compressed case...
	// since there is presently no way to texsubimage a DXT when the rect does not cover the whole width,
	// we will probably need to inflate the dirty rect in the recorded lock req so that the entire span is
	// pushed across at unlock time.

	desc->m_sliceRegionOffset = offsetInSlice + desc->m_sliceBaseOffset;

	if (copyout)
	{
		// read the whole slice
		// (odds are we'll never request anything but a whole slice to be read..)
		ReadTexels( desc, true );
	}	// this would be a good place to fill with scrub value if in debug...
	
	*addressOut = m_backing + desc->m_sliceRegionOffset;
	*yStrideOut = yStride;
	*zStrideOut = zStride;

	m_lockCount++;
}

void CGLMTex::Unlock( GLMTexLockParams *params )
{
#if GL_TELEMETRY_GPU_ZONES
	CScopedGLMPIXEvent glmPIXEvent( "CGLMTex::Unlock" );
	g_TelemetryGPUStats.m_nTotalTexLocksAndUnlocks++;
#endif

	// look for an active lock request on this face and mip (doesn't necessarily matter which one, if more than one)
	// and mark it inactive.
	// --> if you can't find one, fail. first line of defense against mismatched locks/unlocks..

	int i=0;
	bool found = false;
	while( !found && (i<m_ctx->m_texLocks.Count()) )
	{
		GLMTexLockDesc *desc = &m_ctx->m_texLocks[i];
		
		// is lock at index 'i' targeted at the texture/face/mip in question?
		if ( (desc->m_req.m_tex == this) && (desc->m_req.m_face == params->m_face) & (desc->m_req.m_mip == params->m_mip) && (desc->m_active) )
		{
			// matched and active, so retire it
			desc->m_active = false;
			
			// stop searching
			found = true;
		}
		i++;
	}

	if (!found)
	{
		GLMStop();	// bad news
	}
	
	// found - so drop lock count
	m_lockCount--;
	
	if (m_lockCount <0)
	{
		GLMStop();	// bad news
	}			

	if (m_lockCount==0)
	{
		// there should not be any active locks remaining on this texture.

		// motivation to defer all texel pushing til *all* open locks are closed out - 
		// if/when we back the texture with a PBO, we will need to unmap that PBO before teximaging from it;
		// by waiting for all the locks to clear this gives us an unambiguous signal to act on.
		
		// scan through all the retired locks for this texture and push the texels for each one.
		// after each one is dispatched, remove it from the pile.
		
		int j=0;
		while( j<m_ctx->m_texLocks.Count() )
		{
			GLMTexLockDesc *desc = &m_ctx->m_texLocks[j];
			
			if ( desc->m_req.m_tex == this )
			{
				// if it's active, something is wrong
				if (desc->m_active)
				{
					GLMStop();
				}
				
				// write the texels
				bool fullyDirty = false;
				
				fullyDirty |= ((m_sliceFlags[ desc->m_sliceIndex ] & kSliceFullyDirty) != 0);

				// this is not optimal and will result in full downloads on any dirty.
				// we're papering over the fact that subimage isn't done yet.
				// but this is safe if the slice of storage is all valid.
				
				// at some point we'll need to actually compare the lock box against the slice bounds.
				
				// fullyDirty |= (m_sliceFlags[ desc->m_sliceIndex ] & kSliceStorageValid);
				
				WriteTexels( desc, fullyDirty  );

				// logical place to trigger preloading
				// only do it for an RT tex, if it is not yet attached to any FBO.
				// also, only do it if the slice number is the last slice in the tex.
				if ( desc->m_sliceIndex == (m_layout->m_sliceCount-1) )
				{
					if ( !(m_layout->m_key.m_texFlags & kGLMTexRenderable) || (m_rtAttachCount==0) )
					{
						m_ctx->PreloadTex( this );
						// printf("( slice %d of %d )", desc->m_sliceIndex, m_layout->m_sliceCount );
					}
				}

				m_ctx->m_texLocks.FastRemove( j );	// remove from the pile, don't advance index
			}
			else
			{
				j++; // move on to next one
			}
		}
		
		// clear the locked and full-dirty flags for all slices
		for( int slice=0; slice < m_layout->m_sliceCount; slice++)
		{
			m_sliceFlags[slice] &= ~( kSliceLocked | kSliceFullyDirty );
		}
		
		// The 3D texture upload code seems to rely on the host copy, probably
		// because it reuploads the whole thing each slice; we only use 3D textures
		// for the 32x32x32 colorpsace conversion lookups and debugging the problem
		// would not save any more memory.
		if ( !m_texClientStorage && ( m_texGLTarget == GL_TEXTURE_2D ) )
		{
			free(m_backing);
			m_backing = NULL;
		}
	}
}

#if defined( OSX )

void CGLMTex::HandleSRGBMismatch( bool srgb, int &srgbFlipCount )
{
	bool srgbCapableTex = false; // not yet known
	bool renderableTex = false; // not yet known.

	srgbCapableTex	= m_layout->m_format->m_glIntFormatSRGB != 0;
	renderableTex	= ( m_layout->m_key.m_texFlags & kGLMTexRenderable ) != 0;
	// we can fix it if it's not a renderable, and an sRGB enabled format variation is available.

	if ( srgbCapableTex && !renderableTex )
	{
		char *texname = m_debugLabel;
		if (!texname) texname = "-";

		m_srgbFlipCount++;

#if GLMDEBUG
		//policy: print the ones that have flipped 1 or N times
		static bool print_allflips		= CommandLine()->FindParm("-glmspewallsrgbflips");
		static bool print_firstflips	= CommandLine()->FindParm("-glmspewfirstsrgbflips");
		static bool print_freqflips	= CommandLine()->FindParm("-glmspewfreqsrgbflips");
		static bool print_crawls		= CommandLine()->FindParm("-glmspewsrgbcrawls");
		static bool print_maxcrawls	= CommandLine()->FindParm("-glmspewsrgbmaxcrawls");
		bool print_it = false;

		if (print_allflips)
		{
			print_it = true;
		}
		if (print_firstflips)		// report on first flip
		{
			print_it |= m_srgbFlipCount==1;
		}
		if (print_freqflips)	// report on 50th flip
		{
			print_it |= m_srgbFlipCount==50;
		}

		if ( print_it )
		{
			char	*formatStr;
			formatStr = "srgb change (samp=%d): tex '%-30s' %08x %s (srgb=%d, %d times)";

			if (strlen(texname) >= 30)
			{
				formatStr = "srgb change (samp=%d): tex '%s' %08x %s (srgb=%d, %d times)";
			}

			printf( "\n" );
			printf( formatStr, index, texname, m_layout->m_layoutSummary, (int)srgb, m_srgbFlipCount );

#ifdef POSIX
			if (print_crawls)
			{
				static char *interesting_crawl_substrs[] = { "CShader::OnDrawElements", NULL };		// add more as needed

				CStackCrawlParams	cp;
				memset( &cp, 0, sizeof(cp) );
				cp.m_frameLimit = 20;

				g_pLauncherMgr->GetStackCrawl(&cp);

				for( int i=0; i< cp.m_frameCount; i++)
				{
					// for each row of crawl, decide if name is interesting
					bool hit = print_maxcrawls;

					for( char **match = interesting_crawl_substrs; (!hit) && (*match != NULL); match++)
					{
						if (strstr(cp.m_crawlNames[i], *match))
						{
							hit = true;
						}
					}

					if (hit)
					{
						printf( "\n\t%s", cp.m_crawlNames[i] );
					}
				}
				printf( "\n");
			}
#endif
		}
#endif // GLMDEBUG

#if GLMDEBUG && 0
		//"toi" = texture of interest
		static char s_toi[256] = "colorcorrection";
		if (strstr( texname, s_toi ))
		{
			// breakpoint on this if you like
			GLMPRINTF(( "srgb change %d for %s", m_srgbFlipCount, texname ));
		}
#endif

		// re-submit the tex unless we're stifling it
		static bool s_nosrgbflips = CommandLine()->FindParm( "-glmnosrgbflips" );
		if ( !s_nosrgbflips )
		{
			ResetSRGB( srgb, false );
		}
	}
	else
	{
		//GLMPRINTF(("-Z- srgb sampling conflict: NOT fixing tex %08x [%s] (srgb req: %d) because (tex-srgb-capable=%d  tex-renderable=%d)", m_textures[index], m_textures[index]->m_tex->m_layout->m_layoutSummary, (int)glsamp->m_srgb, (int)srgbCapableTex, (int)renderableTex ));
		// we just leave the sampler state where it is, and that's life
	}
}


void CGLMTex::ResetSRGB( bool srgb, bool noDataWrite )
{
	// see if requested SRGB state differs from the known one
	bool			wasSRGB = (m_layout->m_key.m_texFlags & kGLMTexSRGB);
	GLMTexLayout	*oldLayout = m_layout;	// need to m_ctx->m_texLayoutTable->DelLayoutRef on this one if we flip

	if (srgb != wasSRGB)
	{
		// we're going to need a new layout (though the storage size should be the same - check it)
		GLMTexLayoutKey newKey = m_layout->m_key;
		
		newKey.m_texFlags &= (~kGLMTexSRGB);	// turn off that bit
		newKey.m_texFlags |= srgb ? kGLMTexSRGB : 0;	// turn on that bit if it should be so
		
		// get new layout 
		GLMTexLayout *newLayout = m_ctx->m_texLayoutTable->NewLayoutRef( &newKey );

		// if SRGB requested, verify that the layout we just got can do it.
		// if it can't, delete the new layout ref and bail.
		if (srgb && (newLayout->m_format->m_glIntFormatSRGB == 0))
		{
			Assert( !"Can't enable SRGB mode on this format" );			
			m_ctx->m_texLayoutTable->DelLayoutRef( newLayout );
			return;
		}

		// check sizes and fail if no match
		if( newLayout->m_storageTotalSize != oldLayout->m_storageTotalSize )
		{
			Assert( !"Bug: layout sizes don't match on SRGB change" );
			m_ctx->m_texLayoutTable->DelLayoutRef( newLayout );
			return;
		}

		// commit to new layout
		m_layout = newLayout;
		m_texGLTarget = m_layout->m_key.m_texGLTarget;
	
		// check same size
		Assert( m_layout->m_storageTotalSize == oldLayout->m_storageTotalSize );
			
		Assert( newLayout != oldLayout );

		// release old
		m_ctx->m_texLayoutTable->DelLayoutRef( oldLayout );
		oldLayout = NULL;

		// force texel re-DL

		// note this messes with TMU 0 as side effect of WriteTexels
		// so we save and restore the TMU 0 binding first
		
		// since we're likely to be called in dxabstract when it is syncing sampler state, we can't go trampling the bindings.
		// a refinement would be to have each texture make a note of which TMU they're bound on, and just use that active TMU for DL instead of 0.
		CGLMTex *tmu0save = m_ctx->m_samplers[0].m_pBoundTex;
		
		for( int face=0; face <m_layout->m_faceCount; face++)
		{
			for( int mip=0; mip <m_layout->m_mipCount; mip++)
			{
				// we're not really going to lock, we're just going to rewrite the orig data
				GLMTexLockDesc	desc;
				
				desc.m_req.m_tex = this;
				desc.m_req.m_face = face;
				desc.m_req.m_mip = mip;

				desc.m_sliceIndex = CalcSliceIndex( face, mip );

				GLMTexLayoutSlice *slice = &m_layout->m_slices[ desc.m_sliceIndex ];
				
				desc.m_req.m_region.xmin = desc.m_req.m_region.ymin = desc.m_req.m_region.zmin = 0;
				desc.m_req.m_region.xmax = slice->m_xSize;
				desc.m_req.m_region.ymax = slice->m_ySize;
				desc.m_req.m_region.zmax = slice->m_zSize;

				desc.m_sliceBaseOffset = slice->m_storageOffset;	// doesn't really matter... we're just pushing zeroes..
				desc.m_sliceRegionOffset = 0;

				WriteTexels( &desc, true, noDataWrite );	// write whole slice. and avoid pushing real bits if the caller requests (RT's)
			}
		}

		// put it back
		m_ctx->BindTexToTMU( tmu0save, 0 );
	}
}
#endif

bool CGLMTex::IsRBODirty() const 
{ 
	return m_nLastResolvedBatchCounter != m_ctx->m_nBatchCounter; 
}

void CGLMTex::ForceRBONonDirty() 
{ 
	m_nLastResolvedBatchCounter = m_ctx->m_nBatchCounter; 
}

void CGLMTex::ForceRBODirty() 
{ 
	m_nLastResolvedBatchCounter = m_ctx->m_nBatchCounter - 1; 
}
