//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "bitmap/psd.h"
#include "tier0/dbg.h"
#include "tier1/utlbuffer.h"
#include "filesystem.h"
#include "tier2/tier2.h"
#include "tier2/utlstreambuffer.h"
#include "bitmap/imageformat.h"
#include "bitmap/bitmap.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// The PSD signature bytes
//-----------------------------------------------------------------------------
#define PSD_SIGNATURE 0x38425053
#define PSD_IMGRES_SIGNATURE 0x3842494D

//-----------------------------------------------------------------------------
// Format of the PSD header on disk
// NOTE: PSD file header, everything is bigendian
//-----------------------------------------------------------------------------
#pragma pack (1)

enum PSDMode_t
{
	MODE_GREYSCALE = 1,
	MODE_PALETTIZED = 2,
	MODE_RGBA = 3,
	MODE_CMYK = 4,
	MODE_MULTICHANNEL = 7,
	MODE_LAB = 9,

	MODE_COUNT = 10,
};

//////////////////////////////////////////////////////////////////////////
//
// BEGIN PSD FILE:
//
//	PSDHeader_t
//  unsigned int	numBytesPalette;
//	byte			palette[ numBytesPalette ]; = { (all red palette entries), (all green palette entries), (all blue palette entries) }, where numEntries = numBytesPalette/3;
//	unsigned int	numBytesImgResources;
//	byte			imgresources[ numBytesImgResources ]; = { sequence of PSDImgResEntry_t }
//	unsigned int	numBytesLayers;
//	byte			layers[ numBytesLayers ];
//	unsigned short	uCompressionInfo;
//	< ~ image data ~ >
//
// END PSD FILE
//
//////////////////////////////////////////////////////////////////////////

struct PSDHeader_t 
{
    unsigned int	m_nSignature;
    unsigned short	m_nVersion;
    unsigned char	m_pReserved[6];
    unsigned short	m_nChannels;
    unsigned int	m_nRows;
    unsigned int	m_nColumns;
    unsigned short	m_nDepth;
    unsigned short	m_nMode;
};

struct PSDPalette_t
{
	unsigned char *m_pRed;
	unsigned char *m_pGreen;
	unsigned char *m_pBlue;
};

//-----------------------------------------------------------------------------
// NOTE: This is how we could load files using file mapping
//-----------------------------------------------------------------------------
//HANDLE File = CreateFile(FileName,GENERIC_READ,0,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
//Assert(File != INVALID_HANDLE_VALUE);
//HANDLE FileMap = CreateFileMapping(File,0,PAGE_READONLY,0,0,0);
//Assert(FileMap != INVALID_HANDLE_VALUE);
//void *FileData = MapViewOfFile(FileMap,FILE_MAP_READ,0,0,0);


//-----------------------------------------------------------------------------
// Is it a PSD file?
//-----------------------------------------------------------------------------
bool IsPSDFile( CUtlBuffer &buf )
{
	int nGet = buf.TellGet();
	PSDHeader_t header;
	buf.Get( &header, sizeof(header) );
	buf.SeekGet( CUtlBuffer::SEEK_HEAD, nGet );

	if ( BigLong( header.m_nSignature ) != PSD_SIGNATURE )
		return false;
	if ( BigShort( header.m_nVersion ) != 1 )
		return false;
	return ( BigShort( header.m_nDepth ) == 8 );
}

bool IsPSDFile( const char *pFileName, const char *pPathID )
{
	CUtlBuffer buf;
	if ( !g_pFullFileSystem->ReadFile( pFileName, pPathID, buf, sizeof(PSDHeader_t) ) )
	{
		Warning( "Unable to read file %s\n", pFileName );
		return false;
	}
	return IsPSDFile( buf );
}


//-----------------------------------------------------------------------------
// Returns information about the PSD file
//-----------------------------------------------------------------------------
bool PSDGetInfo( CUtlBuffer &buf, int *pWidth, int *pHeight, ImageFormat *pImageFormat, float *pSourceGamma )
{
	int nGet = buf.TellGet();
	PSDHeader_t header;
	buf.Get( &header, sizeof(header) );
	buf.SeekGet( CUtlBuffer::SEEK_HEAD, nGet );

	if ( BigLong( header.m_nSignature ) != PSD_SIGNATURE )
		return false;
	if ( BigShort( header.m_nVersion ) != 1 )
		return false;
	if ( BigShort( header.m_nDepth ) != 8 )
		return false;

	*pWidth = BigLong( header.m_nColumns );
	*pHeight = BigLong( header.m_nRows );
	*pImageFormat = BigShort( header.m_nChannels ) == 3 ? IMAGE_FORMAT_RGB888 : IMAGE_FORMAT_RGBA8888;
	*pSourceGamma = ARTWORK_GAMMA;

	return true;
}

bool PSDGetInfo( const char *pFileName, const char *pPathID, int *pWidth, int *pHeight, ImageFormat *pImageFormat, float *pSourceGamma )
{
	CUtlBuffer buf;
	if ( !g_pFullFileSystem->ReadFile( pFileName, pPathID, buf, sizeof(PSDHeader_t) ) )
	{
		Warning( "Unable to read file %s\n", pFileName );
		return false;
	}
	return PSDGetInfo( buf, pWidth, pHeight, pImageFormat, pSourceGamma );
}

//-----------------------------------------------------------------------------
// Get PSD file image resources
//-----------------------------------------------------------------------------
PSDImageResources PSDGetImageResources( CUtlBuffer &buf )
{
	int nGet = buf.TellGet();

	// Header
	PSDHeader_t header;
	buf.Get( &header, sizeof( header ) );

	// Then palette
	unsigned int numBytesPalette = BigLong( buf.GetUnsignedInt() );
	buf.SeekGet( CUtlBuffer::SEEK_CURRENT, numBytesPalette );

	// Then image resources
	unsigned int numBytesImgResources = BigLong( buf.GetUnsignedInt() );
	PSDImageResources imgres( numBytesImgResources, ( unsigned char * ) buf.PeekGet() );

	// Restore the seek
	buf.SeekGet( CUtlBuffer::SEEK_HEAD, nGet );

	return imgres;
}

//-----------------------------------------------------------------------------
// Converts from CMYK to RGB
//-----------------------------------------------------------------------------
static inline void CMYKToRGB( RGBA8888_t &color )
{
	unsigned char nCyan = 255 - color.r;
	unsigned char nMagenta = 255 - color.g;
	unsigned char nYellow = 255 - color.b;
	unsigned char nBlack = 255 - color.a;

	int nCyanBlack		= (int)nCyan + (int)nBlack;
	int nMagentaBlack	= (int)nMagenta + (int)nBlack;
	int nYellowBlack	= (int)nYellow + (int)nBlack;
	color.r = ( nCyanBlack < 255 ) ? 255 - nCyanBlack : 0;
	color.g = ( nMagentaBlack < 255 ) ? 255 - nMagentaBlack : 0;
	color.b = ( nYellowBlack < 255 ) ? 255 - nYellowBlack : 0;
	color.a = 255;
}


//-----------------------------------------------------------------------------
// Deals with uncompressed channels
//-----------------------------------------------------------------------------
static void PSDConvertToRGBA8888( int nChannelsCount, PSDMode_t mode, PSDPalette_t &palette, Bitmap_t &bitmap )
{
	bool bShouldFillInAlpha = false;
	unsigned char *pDest = bitmap.GetBits();

	switch( mode )
	{
	case MODE_RGBA:
		bShouldFillInAlpha = ( nChannelsCount == 3 );
		break;

	case MODE_PALETTIZED:
		{
			// Convert from palette
			bShouldFillInAlpha = ( nChannelsCount == 1 );
			for( int j=0; j < bitmap.Height(); ++j )
			{
				for ( int k = 0; k < bitmap.Width(); ++k, pDest += 4 )
				{
					unsigned char nPaletteIndex = pDest[0];
					pDest[0] = palette.m_pRed[nPaletteIndex];
					pDest[1] = palette.m_pGreen[nPaletteIndex];
					pDest[2] = palette.m_pBlue[nPaletteIndex];
				}
			}
		}
		break;

	case MODE_GREYSCALE:
		{
			// Monochrome
			bShouldFillInAlpha = ( nChannelsCount == 1 );
			for( int j=0; j < bitmap.Height(); ++j )
			{
				for ( int k = 0; k < bitmap.Width(); ++k, pDest += 4 )
				{
					pDest[1] = pDest[0];
					pDest[2] = pDest[0];
				}
			}
		}
		break;

	case MODE_CMYK:
		{
			// NOTE: The conversion will fill in alpha by default
			bShouldFillInAlpha = false;
			for( int j=0; j < bitmap.Height(); ++j )
			{
				for ( int k = 0; k < bitmap.Width(); ++k, pDest += 4 )
				{
					CMYKToRGB( *((RGBA8888_t*)pDest) );
				}
			}
		}
		break;
	}

	if ( bShouldFillInAlpha )
	{
		// No alpha channel, fill in white
		unsigned char *pDestAlpha = bitmap.GetBits();
		for( int j=0; j < bitmap.Height(); ++j )
		{
			for ( int k = 0; k < bitmap.Width(); ++k, pDestAlpha += 4 )
			{
				pDestAlpha[3] = 0xFF;
			}
		}
	}
}

	
//-----------------------------------------------------------------------------
// Deals with uncompressed channels
//-----------------------------------------------------------------------------
static int s_pChannelIndex[MODE_COUNT+1][4] = 
{ 
	{ -1, -1, -1, -1 },
	{ 0, 3, -1, -1 },		// MODE_GREYSCALE
	{ 0, 3, -1, -1 },		// MODE_PALETTIZED
	{ 0, 1, 2, 3 },			// MODE_RGBA
	{ 0, 1, 2, 3 },			// MODE_CMYK
	{ -1, -1, -1, -1 },
	{ -1, -1, -1, -1 },
	{ -1, -1, -1, -1 },		// MODE_MULTICHANNEL
	{ -1, -1, -1, -1 },
	{ -1, -1, -1, -1 },		// MODE_LAB
	{ 3, -1, -1, -1 },		// Secret second pass mode for CMYK
};


static void PSDReadUncompressedChannels( CUtlBuffer &buf, int nChannelsCount, PSDMode_t mode, PSDPalette_t &palette, Bitmap_t &bitmap )
{
	unsigned char *pChannelRow = (unsigned char*)_alloca( bitmap.Width() );
	for ( int i=0; i<nChannelsCount; ++i )
	{
		int nIndex = s_pChannelIndex[mode][i];
		Assert( nIndex != -1 );

		unsigned char *pDest = bitmap.GetBits();
		for( int j=0; j < bitmap.Height(); ++j )
		{
			buf.Get( pChannelRow, bitmap.Width() );

			// Collate the channels together
			for ( int k = 0; k < bitmap.Width(); ++k, pDest += 4 )
			{
				pDest[nIndex] = pChannelRow[k];
			}
		}
	}

	PSDConvertToRGBA8888( nChannelsCount, mode, palette, bitmap );
}


//-----------------------------------------------------------------------------
// Deals with compressed channels
//-----------------------------------------------------------------------------
static void PSDReadCompressedChannels( CUtlBuffer &buf, int nChannelsCount, PSDMode_t mode, PSDPalette_t &palette, Bitmap_t &bitmap )
{
	unsigned char *pChannelRow = (unsigned char*)_alloca( bitmap.Width() );
	for ( int i=0; i<nChannelsCount; ++i )
	{
		int nIndex = s_pChannelIndex[mode][i];
		Assert( nIndex != -1 );

		unsigned char *pDest = bitmap.GetBits();
		for( int j=0; j < bitmap.Height(); ++j )
		{
			unsigned char *pSrc = pChannelRow;
			unsigned int nPixelsRemaining = bitmap.Width();
			while ( nPixelsRemaining > 0 )
			{
				int nCount = buf.GetChar();
				if ( nCount >= 0 )
				{
					// If nCount is between 0 + 7F, it means copy the next nCount+1 bytes directly
					++nCount;
					Assert( (unsigned int)nCount <= nPixelsRemaining );
					buf.Get( pSrc, nCount );
				}
				else
				{
					// If nCount is between 80 and FF, it means replicate the next byte -Count+1 times
					nCount = -nCount + 1;
					Assert( (unsigned int)nCount <= nPixelsRemaining );
					unsigned char nPattern = buf.GetUnsignedChar();
					memset( pSrc, nPattern, nCount );
				}
				pSrc += nCount;
				nPixelsRemaining -= nCount;
			}
			Assert( nPixelsRemaining == 0 );

			// Collate the channels together
			for ( int k = 0; k < bitmap.Width(); ++k, pDest += 4 )
			{
				pDest[nIndex] = pChannelRow[k];
			}
		}
	}

	PSDConvertToRGBA8888( nChannelsCount, mode, palette, bitmap );
}


//-----------------------------------------------------------------------------
// Reads the PSD file into the specified buffer
//-----------------------------------------------------------------------------
bool PSDReadFileRGBA8888( CUtlBuffer &buf, Bitmap_t &bitmap )
{
	PSDHeader_t header;
	buf.Get( &header, sizeof(header) );

	if ( BigLong( header.m_nSignature ) != PSD_SIGNATURE )
		return false;
	if ( BigShort( header.m_nVersion ) != 1 )
		return false;
	if ( BigShort( header.m_nDepth ) != 8 )
		return false;

	PSDMode_t mode = (PSDMode_t)BigShort( header.m_nMode );
	int nChannelsCount = BigShort( header.m_nChannels );

	if ( mode == MODE_MULTICHANNEL || mode == MODE_LAB )
		return false;

	switch ( mode )
	{
	case MODE_RGBA:
		if ( nChannelsCount < 3 )
			return false;
		break;

	case MODE_GREYSCALE:
	case MODE_PALETTIZED:
		if ( nChannelsCount != 1 && nChannelsCount != 2 )
			return false;
		break;

	case MODE_CMYK:
		if ( nChannelsCount < 4 )
			return false;
		break;

	default:
		Warning( "Unsupported PSD color mode!\n" );
		return false;
	}

	int nWidth = BigLong( header.m_nColumns );
	int nHeight = BigLong( header.m_nRows );

	// Skip parts of memory we don't care about
	int nColorModeSize = BigLong( buf.GetUnsignedInt() );
	Assert( nColorModeSize % 3 == 0 );
	unsigned char *pPaletteBits = (unsigned char*)_alloca( nColorModeSize );
	PSDPalette_t palette;
	palette.m_pRed = palette.m_pGreen = palette.m_pBlue = 0;
	if ( nColorModeSize )
	{
		int nPaletteSize = nColorModeSize / 3;
		buf.Get( pPaletteBits, nColorModeSize );
		palette.m_pRed = pPaletteBits;
		palette.m_pGreen = palette.m_pRed + nPaletteSize;
		palette.m_pBlue = palette.m_pGreen + nPaletteSize;
	}
 	int nImageResourcesSize = BigLong( buf.GetUnsignedInt() );
	buf.SeekGet( CUtlBuffer::SEEK_CURRENT, nImageResourcesSize );
	int nLayersSize = BigLong( buf.GetUnsignedInt() );
	buf.SeekGet( CUtlBuffer::SEEK_CURRENT, nLayersSize );

	unsigned short nCompressionType = BigShort( buf.GetShort() );

	bitmap.Init( nWidth, nHeight, IMAGE_FORMAT_RGBA8888 );

	bool bSecondPassCMYKA = ( nChannelsCount > 4 && mode == MODE_CMYK );
	if ( nCompressionType == 0 )
	{
		PSDReadUncompressedChannels( buf, ( nChannelsCount > 4 ) ? 4 : nChannelsCount, mode, palette, bitmap );
	}
	else
	{
		// Skip the data that indicates the length of each compressed row in bytes
		// NOTE: There are two bytes per row per channel
		unsigned int nLineLengthData = sizeof(unsigned short) * bitmap.Height() * nChannelsCount;
		buf.SeekGet( CUtlBuffer::SEEK_CURRENT, nLineLengthData );
		PSDReadCompressedChannels( buf, ( nChannelsCount > 4 ) ? 4 : nChannelsCount, mode, palette, bitmap );
	}

	// Read the alpha in a second pass for CMYKA
	if ( bSecondPassCMYKA )
	{
		if ( nCompressionType == 0 )
		{
			PSDReadUncompressedChannels( buf, 1, MODE_COUNT, palette, bitmap );
		}
		else
		{
			PSDReadCompressedChannels( buf, 1, MODE_COUNT, palette, bitmap );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Loads the heightfield from a file
//-----------------------------------------------------------------------------
bool PSDReadFileRGBA8888( const char *pFileName, const char *pPathID, Bitmap_t &bitmap )
{
	CUtlStreamBuffer buf( pFileName, pPathID, CUtlBuffer::READ_ONLY );
	if ( !g_pFullFileSystem->ReadFile( pFileName, pPathID, buf, sizeof(PSDHeader_t) ) )
	{
		Warning( "Unable to read file %s\n", pFileName );
		return false;
	}
	return PSDReadFileRGBA8888( buf, bitmap );
}


//////////////////////////////////////////////////////////////////////////
//
// PSD Helper structs implementation
//
//////////////////////////////////////////////////////////////////////////

PSDImageResources::ResElement PSDImageResources::FindElement( Resource eType ) const
{
	ResElement res;
	memset( &res, 0, sizeof( res ) );

	unsigned char const *pvBuffer = m_pvBuffer, * const pvBufferEnd = m_pvBuffer + m_numBytes;
	while ( pvBuffer < pvBufferEnd )
	{
		// 4 : signature
		// 2 : type
		// 4 : reserved
		// 2 : length
		// bytes[ length ]

		unsigned long uSignature = BigLong( *( unsigned long * )( pvBuffer ) );
		pvBuffer += 4;
		if ( uSignature != PSD_IMGRES_SIGNATURE )
			break;

		unsigned short uType = BigShort( *( unsigned short * )( pvBuffer ) );
		pvBuffer += 6;

		unsigned short uLength = BigShort( *( unsigned short * )( pvBuffer ) );
		pvBuffer += 2;

		if ( uType == eType )
		{
			res.m_eType = eType;
			res.m_numBytes = uLength;
			res.m_pvData = pvBuffer;
			break;
		}
		else
		{
			pvBuffer += ( ( uLength + 1 ) &~1 );
		}
	}

	return res;
}

PSDResFileInfo::ResFileInfoElement PSDResFileInfo::FindElement( ResFileInfo eType ) const
{
	ResFileInfoElement res;
	memset( &res, 0, sizeof( res ) );

	unsigned char const *pvBuffer = m_res.m_pvData, * const pvBufferEnd = pvBuffer + m_res.m_numBytes;
	while ( pvBuffer < pvBufferEnd )
	{
		// 2 : = 0x1C02
		// 1 : type
		// 2 : length
		// bytes[ length ]
		
		unsigned short uResLabel = BigShort( *( unsigned short * )( pvBuffer ) );
		pvBuffer += 2;

		unsigned char uType = *pvBuffer;
		pvBuffer += 1;

		unsigned short uLength = BigShort( *( unsigned short * )( pvBuffer ) );
		pvBuffer += 2;

		if ( uType == eType && uResLabel == 0x1C02 )
		{
			res.m_eType = eType;
			res.m_numBytes = uLength;
			res.m_pvData = pvBuffer;
			break;
		}
		else
		{
			pvBuffer += uLength;
		}
	}

	return res;
}
