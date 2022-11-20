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

#include "togl/rendermechanism.h"

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
		gGL->glGenRenderbuffersEXT( 1, &m_rboName );
				
		// so we have enough info to go ahead and bind the RBO and put storage on it?
		// try it.
		gGL->glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, m_rboName );

		// quietly clamp if sample count exceeds known limit for the device
		int sampleCount = layout->m_key.m_texSamples;
		
		if (sampleCount > ctx->Caps().m_maxSamples)
		{
			sampleCount = ctx->Caps().m_maxSamples;	// clamp
		}
		
		GLenum	msaaFormat = (layout->m_key.m_texFlags & kGLMTexSRGB) ? layout->m_format->m_glIntFormatSRGB : layout->m_format->m_glIntFormat;
		gGL->glRenderbufferStorageMultisampleEXT(	GL_RENDERBUFFER_EXT,
												sampleCount,	// not "layout->m_key.m_texSamples"
												msaaFormat,
												layout->m_key.m_xSize,
												layout->m_key.m_ySize );	

		if (gl_texmsaalog.GetInt())
		{
			printf( "\n == MSAA Tex %p %s : MSAA RBO is intformat %s (%x)", this, m_debugLabel?m_debugLabel:"", GLMDecode( eGL_ENUM, msaaFormat ), msaaFormat );
		}

		gGL->glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, 0 );
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
		
		if (gGL->m_bHave_GL_APPLE_texture_range)
			gGL->glTexParameteri( m_layout->m_key.m_texGLTarget, GL_TEXTURE_MINIMIZE_STORAGE_APPLE, 1 );
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
		gGL->glDeleteRenderbuffersEXT( 1, &m_rboName );
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
					
					gGL->glGetCompressedTexImage(	target,					// target
												desc->m_req.m_mip,		// level
												sliceAddress );			// destination
				}
				else
				{
					// uncompressed path
					// http://www.opengl.org/sdk/docs/man/xhtml/glGetTexImage.xml
					
					gGL->glGetTexImage(			target,						// target
											desc->m_req.m_mip,			// level
											format->m_glDataFormat,		// dataformat
											format->m_glDataType,		// datatype
											sliceAddress );				// destination
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
	{
		mayUseSubImage = gl_enabletexsubimage.GetInt() != 0;
	}
			
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
		Assert( m_layout->m_format->m_glDataFormat != GL_DEPTH_STENCIL_EXT );
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
	if (gGL->m_bHave_GL_APPLE_client_storage)
	{
		gGL->glPixelStorei( GL_UNPACK_CLIENT_STORAGE_APPLE, m_texClientStorage );
	}

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
				gGL->glCompressedTexImage2D( target,						// target
										desc->m_req.m_mip,			// level
										intformat,					// internalformat - don't use format->m_glIntFormat because we have the SRGB select going on above
										slice->m_xSize,				// width
										slice->m_ySize,				// height
										0,							// border
										slice->m_storageSize,		// imageSize
										sliceAddress );				// data
				
				
			}
			else
			{
				if (mayUseSubImage)
				{
					// go subimage2D if it's a replacement, not a creation


					gGL->glPixelStorei( GL_UNPACK_ROW_LENGTH, slice->m_xSize );			// in pixels
					gGL->glPixelStorei( GL_UNPACK_SKIP_PIXELS, writeBox.xmin );		// in pixels
					gGL->glPixelStorei( GL_UNPACK_SKIP_ROWS, writeBox.ymin );		// in pixels

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

						/*
							//http://www.opengl.org/sdk/docs/man/xhtml/glTexSubImage2D.xml
							glTexSubImage2D(	target,
												desc->m_req.m_mip,			// level
												0,							// xoffset
												0,							// yoffset
												slice->m_xSize,				// width
												slice->m_ySize,				// height
												glDataFormat,				// format
												glDataType,					// type
												sliceAddress				// data
												);					
						*/				
				}
				else
				{
					if (m_layout->m_key.m_texFlags & kGLMTexRenderable)
					{
						if (gl_rt_forcergba.GetInt())
						{
							if (glDataFormat == GL_BGRA)
							{
								// change it
								glDataFormat = GL_RGBA;
							}
						}
					}
					
					// uncompressed path
					// http://www.opengl.org/documentation/specs/man_pages/hardcopy/GL/html/gl/teximage2d.html
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
				// uncompressed path
				// http://www.opengl.org/sdk/docs/man/xhtml/glTexImage3D.xml
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

	if (gGL->m_bHave_GL_APPLE_client_storage)
	{
		gGL->glPixelStorei( GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE );
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
