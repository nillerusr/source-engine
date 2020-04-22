//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The 360 VTF file format I/O class to help simplify access to 360 VTF files.
// 360 Formatted VTF's are stored ascending 1x1 up to NxN. Disk format and unserialized
// formats are expected to be the same.
//
//=====================================================================================//

#include "bitmap/imageformat.h"
#include "cvtf.h"
#include "utlbuffer.h"
#include "tier0/dbg.h"
#include "tier0/mem.h"
#include "tier2/fileutils.h"
#include "byteswap.h"
#include "filesystem.h"
#include "mathlib/mathlib.h"
#include "tier1/lzmaDecoder.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Callback for UpdateOrCreate utility function - swaps a vtf file.
//-----------------------------------------------------------------------------
static bool VTFCreateCallback( const char *pSourceName, const char *pTargetName, const char *pPathID, void *pExtraData )
{
	// Generate the file
	CUtlBuffer sourceBuf;
	CUtlBuffer targetBuf;
	bool bOk = g_pFullFileSystem->ReadFile( pSourceName, pPathID, sourceBuf );
	if ( bOk )
	{
		bOk = ConvertVTFTo360Format( pSourceName, sourceBuf, targetBuf, NULL );
		if ( bOk )
		{
			bOk = g_pFullFileSystem->WriteFile( pTargetName, pPathID, targetBuf );
		}
	}

	if ( !bOk )
	{
		Warning( "Failed to create %s\n", pTargetName );
	}
	return bOk;
}

//-----------------------------------------------------------------------------
// Calls utility function to create .360 version of a vtf file.
//-----------------------------------------------------------------------------
int CVTFTexture::UpdateOrCreate( const char *pFilename, const char *pPathID, bool bForce )
{
	return ::UpdateOrCreate( pFilename, NULL, 0, pPathID, VTFCreateCallback, bForce, NULL );
}

//-----------------------------------------------------------------------------
// Determine size of file, possibly smaller if skipping top mip levels.
//-----------------------------------------------------------------------------
int CVTFTexture::FileSize( bool bPreloadOnly, int nMipSkipCount ) const
{
	if ( bPreloadOnly )
	{
		// caller wants size of preload
		return m_iPreloadDataSize;
	}

	const ResourceEntryInfo *pEntryInfo = FindResourceEntryInfo( VTF_LEGACY_RSRC_IMAGE );
	if ( !pEntryInfo )
	{
		// has to exist
		Assert( 0 );
		return 0;
	}
	int iImageDataOffset = pEntryInfo->resData;

	if ( m_iCompressedSize )
	{
		// file is compressed, mip skipping is non-applicable at this stage
		return iImageDataOffset + m_iCompressedSize;
	}

	// caller gets file size, possibly truncated due to mip skipping
	int nFaceSize = ComputeFaceSize( nMipSkipCount );
	return iImageDataOffset + m_nFrameCount * m_nFaceCount * nFaceSize;
}

//-----------------------------------------------------------------------------
// Unserialization of image data from buffer
//-----------------------------------------------------------------------------
bool CVTFTexture::LoadImageData( CUtlBuffer &buf, bool bBufferIsVolatile, int nMipSkipCount )
{
	ResourceEntryInfo *pEntryInfo = FindResourceEntryInfo( VTF_LEGACY_RSRC_IMAGE );
	if ( !pEntryInfo )
	{
		// has to exist
		Assert( 0 );
		return false;
	}
	int iImageDataOffset = pEntryInfo->resData;

	// Fix up the mip count + size based on how many mip levels we skip...
	if ( nMipSkipCount > 0 )
	{
		if ( nMipSkipCount >= m_nMipCount )
		{
			nMipSkipCount = 0;
		}
		ComputeMipLevelDimensions( nMipSkipCount, &m_nWidth, &m_nHeight, &m_nDepth );
		m_nMipCount -= nMipSkipCount;
		m_nMipSkipCount += nMipSkipCount;
	}

	int iImageSize = ComputeFaceSize();
	iImageSize = m_nFrameCount * m_nFaceCount * iImageSize;

	// seek to start of image data
	// The mip levels are stored on disk ascending from smallest (1x1) to largest (NxN) to allow for picmip truncated reads
	buf.SeekGet( CUtlBuffer::SEEK_HEAD, iImageDataOffset ); 

	CLZMA lzma;
	if ( m_iCompressedSize )
	{
		unsigned char *pCompressedData = (unsigned char *)buf.PeekGet();
		if ( !lzma.IsCompressed( pCompressedData ) )
		{
			// huh? header says it was compressed
			Assert( 0 );
			return false;
		}
	
		// have to decode entire image
		unsigned int originalSize = lzma.GetActualSize( pCompressedData );
		AllocateImageData( originalSize );
		unsigned int outputLength = lzma.Uncompress( pCompressedData, m_pImageData );
		return ( outputLength == originalSize );		
	}

	bool bOK;
	if ( bBufferIsVolatile )
	{
		AllocateImageData( iImageSize );
		buf.Get( m_pImageData, iImageSize );
		bOK = buf.IsValid();
	}
	else
	{
		// safe to alias
		m_pImageData = (unsigned char *)buf.PeekGet( iImageSize, 0 );
		bOK = ( m_pImageData != NULL );
	}

	return bOK;
}

//-----------------------------------------------------------------------------
// Unserialization
//-----------------------------------------------------------------------------
bool CVTFTexture::ReadHeader( CUtlBuffer &buf, VTFFileHeaderX360_t &header )
{
	memset( &header, 0, sizeof( VTFFileHeaderX360_t ) );
	buf.GetObjects( &header );
	if ( !buf.IsValid() )
	{
		Warning( "*** Error getting header from a X360 VTF file.\n" );
		return false;
	}

	// Validity check
	if ( Q_strncmp( header.fileTypeString, "VTFX", 4 ) )
	{
		Warning( "*** Tried to load a PC VTF file as a X360 VTF file!\n" );
		return false;
	}

	if ( header.version[0] != VTF_X360_MAJOR_VERSION && header.version[1] != VTF_X360_MINOR_VERSION )
	{
		Warning( "*** Encountered X360 VTF file with an invalid version!\n" );
		return false;
	}

	if ( ( header.flags & TEXTUREFLAGS_ENVMAP ) && ( header.width != header.height ) )
	{
		Warning( "*** Encountered X360 VTF non-square cubemap!\n" );
		return false;
	}

	if ( ( header.flags & TEXTUREFLAGS_ENVMAP ) && ( header.depth != 1 ) )
	{
		Warning( "*** Encountered X360 VTF volume texture cubemap!\n" );
		return false;
	}

	if ( header.width <= 0 || header.height <= 0 || header.depth <= 0 )
	{
		Warning( "*** Encountered X360 VTF invalid texture size!\n" );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Unserialization. Can optionally alias image components to a non-volatile buffer,
// which prevents unecessary copies.  Disk format and memory format of the image
// components are explicitly the same.
//-----------------------------------------------------------------------------
bool CVTFTexture::UnserializeFromBuffer( CUtlBuffer &buf, bool bBufferIsVolatile, bool bHeaderOnly, bool bPreloadOnly, int nMipSkipCount )
{
	VTFFileHeaderX360_t header;
	ResourceEntryInfo	*pEntryInfo;

	if ( !ReadHeader( buf, header ) )
	{
		return false;
	}

	// must first release any prior owned memory or reset aliases, otherwise corruption if types intermingled
	ReleaseImageMemory();
	ReleaseResources();

	m_nVersion[0] = header.version[0];
	m_nVersion[1] = header.version[1];

	m_nWidth = header.width;
	m_nHeight = header.height;
	m_nDepth = header.depth;
	m_Format = header.imageFormat;
	m_nFlags = header.flags;
	m_nFrameCount = header.numFrames;
	m_nFaceCount = ( m_nFlags & TEXTUREFLAGS_ENVMAP ) ? CUBEMAP_FACE_COUNT-1 : 1;
	m_nMipCount = ComputeMipCount();
	m_nMipSkipCount = header.mipSkipCount;
	m_vecReflectivity = header.reflectivity;
	m_flBumpScale = header.bumpScale;
	m_iPreloadDataSize = header.preloadDataSize;
	m_iCompressedSize = header.compressedSize;

	m_LowResImageFormat = IMAGE_FORMAT_RGB888;
	if ( header.lowResImageSample[3] )
	{
		// nonzero denotes validity of color value
		m_nLowResImageWidth = 1;
		m_nLowResImageHeight = 1;
		*(unsigned int *)m_LowResImageSample = *(unsigned int *)header.lowResImageSample;
	}
	else
	{
		m_nLowResImageWidth = 0;
		m_nLowResImageHeight = 0;
		*(unsigned int *)m_LowResImageSample = 0;
	}

	// 360 always has the image resource
	Assert( header.numResources >= 1 );
	m_arrResourcesInfo.SetCount( header.numResources );
	m_arrResourcesData.SetCount( header.numResources );

	// Read the dictionary of resources info
	buf.Get( m_arrResourcesInfo.Base(), m_arrResourcesInfo.Count() * sizeof( ResourceEntryInfo ) );
	if ( !buf.IsValid() )
	{
		return false;
	}

	pEntryInfo = FindResourceEntryInfo( VTF_LEGACY_RSRC_IMAGE );
	if ( !pEntryInfo )
	{
		// not optional, has to be present
		Assert( 0 );
		return false;
	}

	if ( bHeaderOnly )
	{
		// caller wants header components only
		// resource data chunks are NOT unserialized!
		return true;
	}

	if ( !LoadNewResources( buf ) )
	{
		return false;
	}

	if ( bPreloadOnly )
	{
		// caller wants preload portion only, everything up to the image
		return true;
	}

	if ( !LoadImageData( buf, bBufferIsVolatile, nMipSkipCount ) )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Discard image data to free up memory.
//-----------------------------------------------------------------------------
void CVTFTexture::ReleaseImageMemory()
{
	// valid sizes identify locally owned memory
	if ( m_nImageAllocSize )
	{
		delete [] m_pImageData;
		m_nImageAllocSize = 0;
	}

	// block pointers could be owned or aliased, always clear
	// ensures other caller's don't free an aliased pointer
	m_pImageData = NULL;
}

//-----------------------------------------------------------------------------
// Attributes...
//-----------------------------------------------------------------------------
bool CVTFTexture::IsPreTiled() const
{
	return false; 
}

int CVTFTexture::MappingWidth() const
{
	return m_nWidth << m_nMipSkipCount;
}

int CVTFTexture::MappingHeight() const
{
	return m_nHeight << m_nMipSkipCount;
}

int CVTFTexture::MappingDepth() const
{
	return m_nDepth << m_nMipSkipCount;
}

int CVTFTexture::MipSkipCount() const
{
	return m_nMipSkipCount;
}

unsigned char *CVTFTexture::LowResImageSample()
{
	return &m_LowResImageSample[0];
}
