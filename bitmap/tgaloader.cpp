//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <stdio.h>
#include "bitmap/tgaloader.h"
#include "tier0/dbg.h"
#include "basetypes.h"
#include <math.h>
#include "tier1/utlvector.h"
#include "tier1/utlbuffer.h"
#include "filesystem.h"
#include "tier2/tier2.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace TGALoader
{

//-----------------------------------------------------------------------------
// Format of the TGA header on disk
//-----------------------------------------------------------------------------

#pragma pack (1)

struct TGAHeader_t
{
	unsigned char 	id_length;
	unsigned char	colormap_type;
	unsigned char	image_type;
	unsigned short	colormap_index;
	unsigned short	colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin;
	unsigned short	y_origin;
	unsigned short	width;
	unsigned short	height;
	unsigned char	pixel_size;
	unsigned char	attributes;
};


//-----------------------------------------------------------------------------
// read a row into an RGBA8888 array.
//-----------------------------------------------------------------------------

typedef void (*ReadRowFunc_t)( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDstMemory );


//-----------------------------------------------------------------------------
// output a RGBA8888 row into the destination format.
//-----------------------------------------------------------------------------

typedef void (*OutputRowFunc_t)( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDstMemory );


//-----------------------------------------------------------------------------
// Important constants
//-----------------------------------------------------------------------------

#define TGA_MAX_COLORMAP_SIZE ( 256 * 4 )
#define TGA_MAX_ROW_LENGTH_IN_PIXELS IMAGE_MAX_DIM


//-----------------------------------------------------------------------------
// Globals... blech
//-----------------------------------------------------------------------------
static unsigned char g_ColorMap[TGA_MAX_COLORMAP_SIZE];

// run-length state from row to row for RLE images
static bool g_IsRunLengthPacket;
static int g_PixelsLeftInPacket;

static unsigned char g_SrcGammaTable[256];
static unsigned char g_DstGammaTable[256];

typedef CUtlMemory<unsigned char> CTempImage;


//-----------------------------------------------------------------------------
// Reads in a file, sticks it into a UtlVector
//-----------------------------------------------------------------------------

static bool ReadFile( char const* pFileName, CTempImage& image, int maxbytes = -1 )
{
	Assert( pFileName );
	Assert( g_pFullFileSystem );
	if( !g_pFullFileSystem )
	{
		return false;
	}

	FileHandle_t fileHandle;
	fileHandle = g_pFullFileSystem->Open( pFileName, "rb" );
	if( !fileHandle )
		return false;

	// How big is the file?
	long pos;
	if (maxbytes < 0)
	{
		pos = g_pFullFileSystem->Size( fileHandle );
	}
	else
	{
		pos = maxbytes;
	}
	
	// Allocate enough space
	image.EnsureCapacity( pos );

	// Back to the start of the file
	g_pFullFileSystem->Seek( fileHandle, 0, FILESYSTEM_SEEK_HEAD );

	// Read the file into the vector memory
	int len = g_pFullFileSystem->Read( image.Base(), pos, fileHandle );

	// Close the file
	g_pFullFileSystem->Close( fileHandle );

	// It's an error if we didn't read in enough goodies
	return len == pos;
}


//-----------------------------------------------------------------------------
// Reads in the TGA Header
//-----------------------------------------------------------------------------
static void ReadHeader( CUtlBuffer& buf, TGAHeader_t& header )
{
	buf.Get( &header, sizeof(TGAHeader_t) );
}


//-----------------------------------------------------------------------------
// Figures out TGA information
//-----------------------------------------------------------------------------
bool GetInfo( CUtlBuffer& buf, int *width, int *height, 
						ImageFormat *imageFormat, float *sourceGamma )
{
	TGAHeader_t header;

	ReadHeader( buf, header );

	switch( header.image_type )
	{
	case 1: // 8 bit uncompressed TGA image
	case 3: // 8 bit monochrome uncompressed TGA image
	case 9: // 8 bit compressed TGA image
		*imageFormat = IMAGE_FORMAT_I8; 
		break;
	case 2: // 24/32 bit uncompressed TGA image
	case 10: // 24/32 bit compressed TGA image
		if( header.pixel_size == 32 )
		{
			*imageFormat = IMAGE_FORMAT_ABGR8888;
		}
		else if( header.pixel_size == 24 )
		{
			*imageFormat = IMAGE_FORMAT_BGR888;
		}
		else
		{
			return false;
		}
		break;

	default:
		return false;
		break;
	}

	*width = header.width;
	*height = header.height;
	*sourceGamma = ARTWORK_GAMMA;

	return true;
}


//-----------------------------------------------------------------------------
// Returns the minimum amount you have to load to get information about the TGA file
//-----------------------------------------------------------------------------
int TGAHeaderSize()
{
	return sizeof( TGAHeader_t );
}


//-----------------------------------------------------------------------------
// Gets info about a TGA file
//-----------------------------------------------------------------------------
bool GetInfo( char const* pFileName, int *width, int *height, 
				ImageFormat *imageFormat, float *sourceGamma )
{
	// temporary memory
	CTempImage image;

	// try to read in the file
	if (!ReadFile( pFileName, image, sizeof(TGAHeader_t) ))
		return false;

	// Serialization buffer
	CUtlBuffer buf( image.Base(), image.NumAllocated(), CUtlBuffer::READ_ONLY );

	return GetInfo( buf, width, height, imageFormat, sourceGamma );
}


//-----------------------------------------------------------------------------
// Various output methods
//-----------------------------------------------------------------------------

void OutputRowRGBA8888( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, pDst += 4 )
	{
		unsigned char* pSrc = (unsigned char*)buf.PeekGet();
		pDst[0] = pSrc[0];
		pDst[1] = pSrc[1];
		pDst[2] = pSrc[2];
		pDst[3] = pSrc[3];
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, 4 );
	}
}

void OutputRowABGR8888( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, pDst += 4 )
	{
		unsigned char* pSrc = (unsigned char*)buf.PeekGet();
		pDst[3] = pSrc[0];
		pDst[2] = pSrc[1];
		pDst[1] = pSrc[2];
		pDst[0] = pSrc[3];
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, 4 );
	}
}

void OutputRowRGB888( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, pDst += 3 )
	{
		unsigned char* pSrc = (unsigned char*)buf.PeekGet();
		pDst[0] = pSrc[0];
		pDst[1] = pSrc[1];
		pDst[2] = pSrc[2];
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, 4 );
	}
}

void OutputRowBGR888( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, pDst += 3 )
	{
		unsigned char* pSrc = (unsigned char*)buf.PeekGet();
		pDst[2] = pSrc[0];
		pDst[1] = pSrc[1];
		pDst[0] = pSrc[2];
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, 4 );
	}
}

void OutputRowRGB565( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDst )
{
	Assert( 0 );
}

void OutputRowI8( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, ++pDst )
	{
		unsigned char* pSrc = (unsigned char*)buf.PeekGet();

		if( ( pSrc[0] == pSrc[1] ) && ( pSrc[1] == pSrc[2] ) )
		{
			pDst[0] = pSrc[0];
		}
		else
		{
			pDst[0] = ( unsigned char )( 0.299f * pSrc[0] + 0.587f * pSrc[1] + 0.114f * pSrc[2] );
		}

		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, 4 );
	}
}

void OutputRowIA88( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, pDst += 2 )
	{
		unsigned char* pSrc = (unsigned char*)buf.PeekGet();

		if( ( pSrc[0] == pSrc[1] ) && ( pSrc[1] == pSrc[2] ) )
		{
			pDst[0] = pSrc[0];
		}
		else
		{
			pDst[0] = ( unsigned char )( 0.299f * pSrc[0] + 0.587f * pSrc[1] + 0.114f * pSrc[2] );
		}
		pDst[1] = pSrc[3];

		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, 4 );
	}
}

void OutputRowA8( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, ++pDst )
	{
		unsigned char* pSrc = (unsigned char*)buf.PeekGet();
		pDst[0] = pSrc[3];
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, 4 );
	}
}

void OutputRowRGB888BlueScreen( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, pDst += 3 )
	{
		unsigned char* pSrc = (unsigned char*)buf.PeekGet();
		pDst[0] = (unsigned char)(( ( int )pSrc[0] * ( int )pSrc[3] ) >> 8);
		pDst[1] = (unsigned char)(( ( int )pSrc[1] * ( int )pSrc[3] ) >> 8);
		pDst[2] = (( ( ( ( int )pSrc[2] * ( int )pSrc[3] ) ) >> 8 ) + ( 255 - pSrc[3] ));
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, 4 );
	}
}

void OutputRowBGR888BlueScreen( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, pDst += 3 )
	{
		unsigned char* pSrc = (unsigned char*)buf.PeekGet();
		pDst[2] = (unsigned char)(( ( int )pSrc[0] * ( int )pSrc[3] ) >> 8);
		pDst[1] = (unsigned char)(( ( int )pSrc[1] * ( int )pSrc[3] ) >> 8);
		pDst[0] = (unsigned char)(( ( ( ( int )pSrc[2] * ( int )pSrc[3] ) ) >> 8 ) + ( 255 - pSrc[3] ));
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, 4 );
	}
}

void OutputRowARGB8888( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, pDst += 4 )
	{
		unsigned char* pSrc = (unsigned char*)buf.PeekGet();
		pDst[0] = pSrc[3];
		pDst[1] = pSrc[0];
		pDst[2] = pSrc[1];
		pDst[3] = pSrc[2];
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, 4 );
	}
}

void OutputRowBGRA8888( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, pDst += 4 )
	{
		unsigned char* pSrc = (unsigned char*)buf.PeekGet();
		pDst[0] = pSrc[2];
		pDst[1] = pSrc[1];
		pDst[2] = pSrc[0];
		pDst[3] = pSrc[3];
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, 4 );
	}
}

void OutputRowBGRX8888( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, pDst += 4 )
	{
		unsigned char* pSrc = (unsigned char*)buf.PeekGet();
		pDst[0] = pSrc[2];
		pDst[1] = pSrc[1];
		pDst[2] = pSrc[0];
		pDst[3] = 255;
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, 4 );
	}
}

void OutputRowBGR565( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, pDst += 2 )
	{
		unsigned char* pSrc = (unsigned char*)buf.PeekGet();
		unsigned short rgba = (pSrc[2] & 0x1F) | ((pSrc[1] & 0x3F) << 5) | 
			((pSrc[0] & 0x1F) << 11);

		pDst[0] = rgba & 0xFF;
		pDst[1] = rgba >> 8;
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, 4 );
	}
}

void OutputRowBGRX5551( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, pDst += 2 )
	{
		unsigned char* pSrc = (unsigned char*)buf.PeekGet();
		unsigned short rgba = (pSrc[2] & 0x1F) | ((pSrc[1] & 0x1F) << 5) | 
			((pSrc[0] & 0x1F) << 10) | 0x8000;

		pDst[0] = rgba & 0xFF;
		pDst[1] = rgba >> 8;
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, 4 );
	}
}

static OutputRowFunc_t GetOutputRowFunc( ImageFormat imageFormat )
{
	switch( imageFormat )
	{
	case IMAGE_FORMAT_RGBA8888:
		return &OutputRowRGBA8888;
	case IMAGE_FORMAT_ABGR8888:
		return &OutputRowABGR8888;
	case IMAGE_FORMAT_RGB888:
		return &OutputRowRGB888;
	case IMAGE_FORMAT_BGR888:
		return &OutputRowBGR888;
	case IMAGE_FORMAT_RGB565:
		return &OutputRowRGB565;
	case IMAGE_FORMAT_I8:
		return &OutputRowI8;
	case IMAGE_FORMAT_IA88:
		return &OutputRowIA88;
	case IMAGE_FORMAT_A8:
		return &OutputRowA8;
	case IMAGE_FORMAT_RGB888_BLUESCREEN:
		return &OutputRowRGB888BlueScreen;
	case IMAGE_FORMAT_BGR888_BLUESCREEN:
		return &OutputRowBGR888BlueScreen;
	case IMAGE_FORMAT_ARGB8888:
		return &OutputRowARGB8888;
	case IMAGE_FORMAT_BGRA8888:
		return &OutputRowBGRA8888;
	case IMAGE_FORMAT_BGRX8888:
		return &OutputRowBGRX8888;
	case IMAGE_FORMAT_BGR565:
		return &OutputRowBGR565;
	case IMAGE_FORMAT_BGRX5551:
		return &OutputRowBGRX5551;
#ifdef _X360
	case IMAGE_FORMAT_LINEAR_RGB888:
		return &OutputRowRGB888;
	case IMAGE_FORMAT_LINEAR_BGR888:
		return &OutputRowBGR888;
#endif
	default:
		return NULL;
		break;
	}
}

#if 0
static void InitSourceGammaConversionTable( float srcGamma )
{
	static float lastSrcGamma = -1;
	if (lastSrcGamma == srcGamma)
		return;

	lastSrcGamma = srcGamma;
	ImageLoader::ConstructGammaTable( g_SrcGammaTable, srcGamma, 1.0f );
}

static void InitDestGammaConversionTable( float dstGamma )
{
	static float lastDstGamma = -1;
	if (lastDstGamma == dstGamma)
		return;

	lastDstGamma = dstGamma;
	ImageLoader::ConstructGammaTable( g_DstGammaTable, 1.0f, dstGamma );
}
#endif

//-----------------------------------------------------------------------------
// Reads an 8-bit palettized TGA image
//-----------------------------------------------------------------------------

void ReadRow8BitUncompressedWithColormap( CUtlBuffer& buf, 
		TGAHeader_t const& header, unsigned char* pDst )
{
	int i;
	unsigned char* colormapEntry;

	switch( header.colormap_size )
	{
	case 8:
		for( i = 0; i < header.width; ++i, pDst += 4 )
		{
			int pal = buf.GetUnsignedChar();

			colormapEntry = &g_ColorMap[pal];
			pDst[0] = colormapEntry[0];
			pDst[1] = colormapEntry[0];
			pDst[2] = colormapEntry[0];
			pDst[3] = 255;
		}
		break;

	case 24:
		for( i = 0; i < header.width; ++i, pDst += 4 )
		{
			int pal = buf.GetUnsignedChar();

			colormapEntry = &g_ColorMap[pal * 3];
			pDst[0] = colormapEntry[2];
			pDst[1] = colormapEntry[1];
			pDst[2] = colormapEntry[0];
			pDst[3] = 255;
		}
		break;

	case 32:
		for( i = 0; i < header.width; ++i, pDst += 4 )
		{
			int pal = buf.GetUnsignedChar();

			colormapEntry = &g_ColorMap[pal * 4];
			pDst[0] = colormapEntry[3];
			pDst[1] = colormapEntry[2];
			pDst[2] = colormapEntry[1];
			pDst[3] = colormapEntry[0];
		}
		break;

	default:
		Assert( 0 );
		break;
	}
}


//-----------------------------------------------------------------------------
// Reads an 8-bit greyscale TGA image
//-----------------------------------------------------------------------------

void ReadRow8BitUncompressedWithoutColormap( CUtlBuffer& buf, 
		TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, pDst += 4 )
	{
		pDst[0] = pDst[1] = pDst[2] = buf.GetUnsignedChar();
		pDst[3] = 255;
	}
}

//-----------------------------------------------------------------------------
// Reads a 24-bit TGA image
//-----------------------------------------------------------------------------

void ReadRow24BitUncompressedWithoutColormap( CUtlBuffer& buf, 
		TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, pDst += 4 )
	{
		pDst[2] = buf.GetUnsignedChar();
		pDst[1] = buf.GetUnsignedChar();
		pDst[0] = buf.GetUnsignedChar();
		pDst[3] = 255;
	}
}

//-----------------------------------------------------------------------------
// Reads a 32-bit TGA image
//-----------------------------------------------------------------------------

void ReadRow32BitUncompressedWithoutColormap( CUtlBuffer& buf, 
		TGAHeader_t const& header, unsigned char* pDst )
{
	for( int i = 0; i < header.width; ++i, pDst += 4 )
	{
		pDst[2] = buf.GetUnsignedChar();
		pDst[1] = buf.GetUnsignedChar();
		pDst[0] = buf.GetUnsignedChar();
		pDst[3] = buf.GetUnsignedChar();
	}
}


//-----------------------------------------------------------------------------
// Decompresses a run-length encoded row of bytes
//-----------------------------------------------------------------------------

static void DecompressRow( CUtlBuffer& buf, TGAHeader_t const& header, unsigned char* pDst )
{
	int bytesPerPixel = header.pixel_size >> 3;
	int pixelsLeftInRow = header.width;
	int numPixelsToProcess;

#ifdef DBGFLAG_ASSERT
	unsigned char *pLast = pDst + header.width * bytesPerPixel;
#endif

	unsigned char repeat[4] = {};
	do
	{
		if( !g_PixelsLeftInPacket )
		{
			// start a new packet.
			unsigned char packetHeader = buf.GetUnsignedChar();
			g_PixelsLeftInPacket = 1 + ( packetHeader & 0x7f );
			if( packetHeader & 0x80 )
			{
				g_IsRunLengthPacket = true;

				// Read what I'm supposed to repeat
				for (int i = 0; i < bytesPerPixel; ++i)
				{
					repeat[i] = buf.GetUnsignedChar();
				}
			}
			else
			{
				g_IsRunLengthPacket = false;
			}
		}

		// already in the middle of a packet of data.
		numPixelsToProcess = g_PixelsLeftInPacket;
		if( numPixelsToProcess > pixelsLeftInRow )
		{
			numPixelsToProcess = pixelsLeftInRow;
		}
		if( g_IsRunLengthPacket )
		{
			for( int i = numPixelsToProcess; --i >= 0; pDst += bytesPerPixel )
			{
				for (int j = 0; j < bytesPerPixel; ++j )
				{
					pDst[j] = repeat[j];
				}
			}
		}
		else
		{
			buf.Get( pDst, numPixelsToProcess * bytesPerPixel );
			pDst += numPixelsToProcess * bytesPerPixel;
		}

		g_PixelsLeftInPacket -= numPixelsToProcess;
		pixelsLeftInRow -= numPixelsToProcess;

	} while( pixelsLeftInRow );

	Assert( pDst == pLast );
}


//-----------------------------------------------------------------------------
// Reads a compressed 8-bit palettized TGA image
//-----------------------------------------------------------------------------
void ReadRow8BitCompressedWithColormap( CUtlBuffer& buf, 
		TGAHeader_t const& header, unsigned char* pDst )
{
	unsigned char rowI_8[TGA_MAX_ROW_LENGTH_IN_PIXELS];

	DecompressRow( buf, header, rowI_8 );

	CUtlBuffer uncompressedBuf( rowI_8, TGA_MAX_ROW_LENGTH_IN_PIXELS, CUtlBuffer::READ_ONLY );
	ReadRow8BitUncompressedWithColormap( uncompressedBuf, header, pDst );
}


//-----------------------------------------------------------------------------
// Reads a compressed 8-bit greyscale TGA image
//-----------------------------------------------------------------------------
void ReadRow8BitCompressedWithoutColormap( CUtlBuffer& buf, 
		TGAHeader_t const& header, unsigned char* pDst )
{
	unsigned char rowI_8[TGA_MAX_ROW_LENGTH_IN_PIXELS];

	DecompressRow( buf, header, rowI_8 );

	CUtlBuffer uncompressedBuf( rowI_8, TGA_MAX_ROW_LENGTH_IN_PIXELS, CUtlBuffer::READ_ONLY );
	ReadRow8BitUncompressedWithoutColormap( uncompressedBuf, header, pDst );
}

//-----------------------------------------------------------------------------
// Reads a compressed 24-bit TGA image
//-----------------------------------------------------------------------------

void ReadRow24BitCompressedWithoutColormap( CUtlBuffer& buf, 
		TGAHeader_t const& header, unsigned char* pDst )
{
	unsigned char rowBGR_888[TGA_MAX_ROW_LENGTH_IN_PIXELS * 3];

	DecompressRow( buf, header, rowBGR_888 );

	CUtlBuffer uncompressedBuf( rowBGR_888, TGA_MAX_ROW_LENGTH_IN_PIXELS * 3, CUtlBuffer::READ_ONLY );
	ReadRow24BitUncompressedWithoutColormap( uncompressedBuf, header, pDst );
}

//-----------------------------------------------------------------------------
// Reads a compressed 32-bit TGA image
//-----------------------------------------------------------------------------

void ReadRow32BitCompressedWithoutColormap( CUtlBuffer& buf, 
		TGAHeader_t const& header, unsigned char* pDst )
{
	unsigned char rowBGRA_8888[TGA_MAX_ROW_LENGTH_IN_PIXELS << 2];

	DecompressRow( buf, header, rowBGRA_8888 );

	CUtlBuffer uncompressedBuf( rowBGRA_8888, TGA_MAX_ROW_LENGTH_IN_PIXELS << 2, CUtlBuffer::READ_ONLY );
	ReadRow32BitUncompressedWithoutColormap( uncompressedBuf, header, pDst );
}

//-----------------------------------------------------------------------------
// Method used to read the TGA
//-----------------------------------------------------------------------------

static ReadRowFunc_t GetReadRowFunc( TGAHeader_t const& header )
{
	switch( header.image_type )
	{
	case 1: // 8 bit uncompressed TGA image
	case 3: // 8 bit monochrome uncompressed TGA image
		if( header.colormap_length )
		{
			return &ReadRow8BitUncompressedWithColormap;
		}
		else
		{
			return &ReadRow8BitUncompressedWithoutColormap;
		}
	case 9: // 8 bit compressed TGA image
		if( header.colormap_length )
		{
			return &ReadRow8BitCompressedWithColormap;
		}
		else
		{
			return &ReadRow8BitCompressedWithoutColormap;
		}
	case 2: // 24/32 bit uncompressed TGA image
		switch( header.pixel_size )
		{
		case 24:
			return &ReadRow24BitUncompressedWithoutColormap;
			break;
		case 32:
			return &ReadRow32BitUncompressedWithoutColormap;
			break;
		default:
			//Error( "unsupported tga colordepth: %d", TGAHeader_t.pixel_size" );
			return 0;
			break;
		}
	case 10: // 24/32 bit compressed TGA image
		if( header.colormap_length )
		{
			// Error( "colormaps not support with 24/32 bit TGAs." );
			return 0;
		}
		else
		{
			switch( header.pixel_size )
			{
			case 24:
				return &ReadRow24BitCompressedWithoutColormap;
				break;
			case 32:
				return &ReadRow32BitCompressedWithoutColormap;
				break;
			default:
				//Error( "unsupported tga colordepth: %d", TGAHeader_t.pixel_size" );
				return NULL;
				break;
			}
		}
	default:
		// Error( "unsupported tga pixel format" );
		return 0;
		break;
	}
}


//-----------------------------------------------------------------------------
// Reads the color map
//-----------------------------------------------------------------------------

static bool ReadColormap( CUtlBuffer& buf, TGAHeader_t const& header )
{
	int numColormapBytes = header.colormap_length * ( header.colormap_size >> 3 );
	if( numColormapBytes > TGA_MAX_COLORMAP_SIZE )
	{
		// Error( "colormap bigger than TGA_MAX_COLORMAP_SIZE" );
		return false;
	}

	// read colormap
	buf.Get( g_ColorMap, numColormapBytes );

	return true;
}


//-----------------------------------------------------------------------------
// Reads the source image
//-----------------------------------------------------------------------------
static bool ReadSourceImage( CUtlBuffer& buf, TGAHeader_t& header, CTempImage& image )
{
	// Figure out our reading and riting
	ReadRowFunc_t ReadRowFunc = GetReadRowFunc( header );
	if( !ReadRowFunc )
		return false;

	// HACK: Fixme: We really shouldn't be using globals here
	// Init RLE vars
	g_PixelsLeftInPacket = 0;

	// Only allocate the memory once
	int memRequired = ImageLoader::GetMemRequired( header.width, header.height, 1,
		IMAGE_FORMAT_RGBA8888, false );
	image.EnsureCapacity( memRequired );

	// read each row and process it. Note the image is upside-down from
	// the way we want it.
	unsigned char* pDstBits;
	// flip the image vertically if necessary.
	if (header.attributes & 0x20)
	{
		for( int row = 0; row < header.height; ++row )
		{
			pDstBits = image.Base() + 
				row * header.width * ImageLoader::SizeInBytes(IMAGE_FORMAT_RGBA8888);
			ReadRowFunc( buf, header, pDstBits );
		}
	}
	else
	{
		for( int row = header.height; --row >= 0; )
		{
			pDstBits = image.Base() + 
				row * header.width * ImageLoader::SizeInBytes(IMAGE_FORMAT_RGBA8888);
			ReadRowFunc( buf, header, pDstBits );
		}
	}

	return true;
}

#if 0
//-----------------------------------------------------------------------------
// Outputs the final image
//-----------------------------------------------------------------------------
static bool OutputImage( CTempImage& image, TGAHeader_t& header, 
						 ImageFormat imageFormat, unsigned char* pDst )
{
	// How do we write?
	OutputRowFunc_t OutputRowFunc = GetOutputRowFunc( imageFormat );
	if( !OutputRowFunc )
		return false;

	CUtlBuffer buf( image.Base(), image.NumAllocated(), CUtlBuffer::READ_ONLY );
	unsigned char* pDstBits;
	for( int row = 0; row < header.height; ++row )
	{
		pDstBits = pDst + 
			row * header.width * ImageLoader::SizeInBytes(imageFormat);
		OutputRowFunc( buf, header, pDstBits );
	}

	return true;
}
#endif

//-----------------------------------------------------------------------------
// Parses the lovely bits previously read from disk
//-----------------------------------------------------------------------------
bool Load( unsigned char *pOutputImage, CUtlBuffer& buf, int width, 
		int height, ImageFormat imageFormat, float targetGamma, bool mipmap )
{
	TGAHeader_t header;

	// Read the TGA header
	ReadHeader( buf, header );

	// skip TARGA image comment
	if( header.id_length != 0 )
	{
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, header.id_length );
	}

	// Read the color map for palettized images
	if( header.colormap_length != 0 )
	{
		if (!ReadColormap( buf, header ))
			return false;
	}
	
	// Stores the RGBA8888 temp version of the image which we'll use to
	// to do mipmapping...
	CTempImage tmpImage;
	if (!ReadSourceImage( buf, header, tmpImage ))
		return false;

	// Erg... what if header.width * header.height > width * height? 
	// Then don't do anything, this is an error condition...
	if ((width * height) < (header.width * header.height))
		return false;

	// Now that we've got the source image, generate the mip-map levels
	ImageLoader::GenerateMipmapLevels( tmpImage.Base(), pOutputImage,
		header.width, header.height, 1, imageFormat, ARTWORK_GAMMA, targetGamma,
		mipmap ? 0 : 1 );

	return true;
}


//-----------------------------------------------------------------------------
// Reads a TGA image from a file
//-----------------------------------------------------------------------------
bool Load( unsigned char *pOutputImage, const char *pFileName, int width, int height, 
			ImageFormat imageFormat, float targetGamma, bool mipmap )
{
	Assert( pOutputImage && pFileName );

	// memory for the file
	CTempImage vec;

	// Read that puppy in!
	if (!ReadFile( pFileName, vec ))
		return false;

	// Make an unserialization buffer
	CUtlBuffer buf( vec.Base(), vec.NumAllocated(), CUtlBuffer::READ_ONLY );
	
	// Do the dirty deed
	return Load( pOutputImage, buf, width, height, imageFormat, targetGamma, mipmap );
}


//-----------------------------------------------------------------------------
// Creates a map in linear space
//-----------------------------------------------------------------------------
bool LoadRGBA8888( CUtlBuffer& buf, CUtlMemory<unsigned char> &outputData, int &outWidth, int &outHeight )
{
	TGAHeader_t header;

	// Read the TGA header
	ReadHeader( buf, header );

	// skip TARGA image comment
	if( header.id_length != 0 )
	{
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, header.id_length );
	}

	// Read the color map for palettized images
	if( header.colormap_length != 0 )
	{
		if (!ReadColormap( buf, header ))
			return false;
	}
	
	// Stores the RGBA8888 temp version of the image which we'll use to
	// to do mipmapping...
	int memSize = ImageLoader::GetMemRequired( 
		header.width, header.height, 1, IMAGE_FORMAT_RGBA8888, false );

	outputData.EnsureCapacity( memSize );
	if (!ReadSourceImage( buf, header, outputData ))
		return false;

	outWidth = header.width;
	outHeight = header.height;
	return true;
}

//-----------------------------------------------------------------------------
// Reads a TGA, keeps it in RGBA8888
//-----------------------------------------------------------------------------

bool LoadRGBA8888( const char *pFileName, CUtlMemory<unsigned char> &outputData, int &outWidth, int &outHeight )
{
	Assert( pFileName );

	// memory for the file
	CTempImage vec;

	// Read that puppy in!
	if (!ReadFile( pFileName, vec ))
		return false;

	// Make an unserialization buffer
	CUtlBuffer buf( vec.Base(), vec.NumAllocated(), CUtlBuffer::READ_ONLY );
	
	// Do the dirty deed
	return LoadRGBA8888( buf, outputData, outWidth, outHeight );
}

} // end namespace TGALoader

