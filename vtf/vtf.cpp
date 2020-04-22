//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The VTF file format I/O class to help simplify access to VTF files
//
//=====================================================================================//

#undef fopen
#include "bitmap/imageformat.h"
#include "cvtf.h"
#include "utlbuffer.h"
#include "tier0/dbg.h"
#include "mathlib/vector.h"
#include "mathlib/mathlib.h"
#include "tier1/strtools.h"
#include "tier0/mem.h"
#include "s3tc_decode.h"
#include "utlvector.h"
#include "vprof_telemetry.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// byteswap data descriptions
BEGIN_BYTESWAP_DATADESC( VTFFileBaseHeader_t )
	DEFINE_ARRAY( fileTypeString, FIELD_CHARACTER, 4 ),
	DEFINE_ARRAY( version, FIELD_INTEGER, 2 ),
	DEFINE_FIELD( headerSize, FIELD_INTEGER ),
END_DATADESC()

BEGIN_BYTESWAP_DATADESC_( VTFFileHeaderV7_1_t, VTFFileBaseHeader_t )
	DEFINE_FIELD( width, FIELD_SHORT ),
	DEFINE_FIELD( height, FIELD_SHORT ),
	DEFINE_FIELD( flags, FIELD_INTEGER ),
	DEFINE_FIELD( numFrames, FIELD_SHORT ),
	DEFINE_FIELD( startFrame, FIELD_SHORT ),
	DEFINE_FIELD( reflectivity, FIELD_VECTOR ),
	DEFINE_FIELD( bumpScale, FIELD_FLOAT ),
	DEFINE_FIELD( imageFormat, FIELD_INTEGER ),
	DEFINE_FIELD( numMipLevels, FIELD_CHARACTER ),
	DEFINE_FIELD( lowResImageFormat, FIELD_INTEGER ),
	DEFINE_FIELD( lowResImageWidth, FIELD_CHARACTER ),
	DEFINE_FIELD( lowResImageHeight, FIELD_CHARACTER ),
END_DATADESC()

BEGIN_BYTESWAP_DATADESC_( VTFFileHeaderV7_2_t, VTFFileHeaderV7_1_t )
	DEFINE_FIELD( depth, FIELD_SHORT ),
END_DATADESC()

BEGIN_BYTESWAP_DATADESC_( VTFFileHeaderV7_3_t, VTFFileHeaderV7_2_t )
	DEFINE_FIELD( numResources, FIELD_INTEGER ),
END_DATADESC()

BEGIN_BYTESWAP_DATADESC_( VTFFileHeader_t, VTFFileHeaderV7_2_t )
END_DATADESC()

BEGIN_BYTESWAP_DATADESC_( VTFFileHeaderX360_t, VTFFileBaseHeader_t )
	DEFINE_FIELD( flags, FIELD_INTEGER ),
	DEFINE_FIELD( width, FIELD_SHORT ),
	DEFINE_FIELD( height, FIELD_SHORT ),
	DEFINE_FIELD( depth, FIELD_SHORT ),
	DEFINE_FIELD( numFrames, FIELD_SHORT ),
	DEFINE_FIELD( preloadDataSize, FIELD_SHORT ),
	DEFINE_FIELD( mipSkipCount, FIELD_CHARACTER ),
	DEFINE_FIELD( numResources, FIELD_CHARACTER ),
	DEFINE_FIELD( reflectivity, FIELD_VECTOR ),
	DEFINE_FIELD( bumpScale, FIELD_FLOAT ),
	DEFINE_FIELD( imageFormat, FIELD_INTEGER ),
	DEFINE_ARRAY( lowResImageSample, FIELD_CHARACTER, 4 ),
	DEFINE_FIELD( compressedSize, FIELD_INTEGER ),
END_DATADESC()

#if defined( POSIX ) || defined( _X360 )
// stub functions
const char* S3TC_GetBlock(
        const void *pCompressed,
        ImageFormat format,
        int nBlocksWide, // How many blocks wide is the image (pixels wide / 4).
        int xBlock,
        int yBlock )
{
	return NULL;
}

char* S3TC_GetBlock(
        void *pCompressed,
        ImageFormat format,
        int nBlocksWide, // How many blocks wide is the image (pixels wide / 4).
        int xBlock,
        int yBlock )
{
	return NULL;
}

S3PaletteIndex S3TC_GetPaletteIndex(
        unsigned char *pFaceData,
        ImageFormat format,
        int imageWidth,
        int x,
        int y )
{
	S3PaletteIndex nullPalette;
	memset(&nullPalette, 0x0, sizeof(nullPalette));
	return nullPalette;
}

// Merge the two palettes and copy the colors
void S3TC_MergeBlocks(
        char **blocks,
        S3RGBA **pOriginals, 
        int nBlocks,
        int lPitch,     // (in BYTES)
        ImageFormat format
        )
{
}

// Note: width, x, and y are in texels, not S3 blocks.
void S3TC_SetPaletteIndex(
        unsigned char *pFaceData,
        ImageFormat format,
        int imageWidth,
        int x,
        int y,
        S3PaletteIndex paletteIndex )
{
}
#endif

// This gives a vertex number to each of the 4 verts on each face.
// We use this to match the verts and determine which edges need to be blended together.
// The vert ordering is lower-left, top-left, top-right, bottom-right.
int g_leftFaceVerts[4] = { 2, 6, 7, 3 };
int g_frontFaceVerts[4] = { 2, 3, 5, 4 };
int g_downFaceVerts[4] = { 4, 0, 6, 2 };
int g_rightFaceVerts[4] = { 5, 1, 0, 4 };
int g_backFaceVerts[4] = { 7, 6, 0, 1 };
int g_upFaceVerts[4] = { 3, 7, 1, 5 };

int *g_FaceVerts[6] =
{
	g_rightFaceVerts,
	g_leftFaceVerts,
	g_backFaceVerts,
	g_frontFaceVerts,
	g_upFaceVerts,
	g_downFaceVerts
};

// For skyboxes..
// These were constructed for the engine skybox, which looks like this
// (assuming X goes forward, Y goes left, and Z goes up).
//
//				 6 ------------- 5
//			   /  			   /  
//			 /	 |			 /	 |
//		   /	 |		   /	 |
//		 2 ------------- 1		 |
//		  		 |		  		 |
//		 |		  		 |		  
//		 |		 7 ------|------ 4
//		 |	   /		 |	   /
//		 |	 /			 |	 /
//		   /			   /
//		 3 ------------- 0
//
int g_skybox_rightFaceVerts[4] = { 7, 6, 5, 4 };
int g_skybox_leftFaceVerts[4] = { 0, 1, 2, 3 };
int g_skybox_backFaceVerts[4] = { 3, 2, 6, 7 };
int g_skybox_frontFaceVerts[4] = { 4, 5, 1, 0 };
int g_skybox_upFaceVerts[4] = { 6, 2, 1, 5 };
int g_skybox_downFaceVerts[4] = { 3, 7, 4, 0 };

int *g_skybox_FaceVerts[6] =
{
	g_skybox_rightFaceVerts,
	g_skybox_leftFaceVerts,
	g_skybox_backFaceVerts,
	g_skybox_frontFaceVerts,
	g_skybox_upFaceVerts,
	g_skybox_downFaceVerts
};

//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------
IVTFTexture *CreateVTFTexture()
{
	return new CVTFTexture;
}

void DestroyVTFTexture( IVTFTexture *pTexture )
{
	delete pTexture;
}

//-----------------------------------------------------------------------------
// Allows us to only load in the first little bit of the VTF file to get info
//-----------------------------------------------------------------------------
int VTFFileHeaderSize( int nMajorVersion, int nMinorVersion )
{
	if ( nMajorVersion == -1 )
	{
		nMajorVersion = VTF_MAJOR_VERSION;
	}

	if ( nMinorVersion == -1 )
	{
		nMinorVersion = VTF_MINOR_VERSION;
	}

	switch ( nMajorVersion )
	{
	case VTF_MAJOR_VERSION:
		switch ( nMinorVersion )
		{
		case 0: // fall through
		case 1:
			return sizeof( VTFFileHeaderV7_1_t );
		case 2:
			return sizeof( VTFFileHeaderV7_2_t );
		case 3:
			return sizeof( VTFFileHeaderV7_3_t ) + sizeof( ResourceEntryInfo ) * MAX_RSRC_DICTIONARY_ENTRIES;
		case VTF_MINOR_VERSION:
			int size1 = sizeof( VTFFileHeader_t );
			int size2 = sizeof( ResourceEntryInfo ) * MAX_RSRC_DICTIONARY_ENTRIES;
			int result = size1 + size2;
			//printf("\n VTFFileHeaderSize (%i %i) is %i + %i -> %i",nMajorVersion,nMinorVersion, size1, size2, result );
			return result;
		}
		break;
	
	case VTF_X360_MAJOR_VERSION:
		return sizeof( VTFFileHeaderX360_t ) + sizeof( ResourceEntryInfo ) * MAX_X360_RSRC_DICTIONARY_ENTRIES;
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CVTFTexture::CVTFTexture()
{
	m_nVersion[0] = 0;
	m_nVersion[1] = 0;

	m_nWidth = 0;
	m_nHeight = 0;
	m_nDepth = 1;
	m_Format = IMAGE_FORMAT_UNKNOWN;

	m_nMipCount = 0;
	m_nFaceCount = 0;
	m_nFrameCount = 0;

	// FIXME: Is the start frame needed?
	m_iStartFrame = 0;

	m_flAlphaThreshhold = -1.0f;
	m_flAlphaHiFreqThreshhold = -1.0f;

	m_flBumpScale = 1.0f;
	m_vecReflectivity.Init( 1.0, 1.0, 1.0f );

	m_nFlags = 0;
	m_pImageData = NULL;
	m_nImageAllocSize = 0;

	// LowRes data
	m_LowResImageFormat = IMAGE_FORMAT_UNKNOWN;
	m_nLowResImageWidth = 0;
	m_nLowResImageHeight = 0;
	m_pLowResImageData = NULL;
	m_nLowResImageAllocSize = 0;

#if defined( _X360 )
	m_nMipSkipCount = 0;
	*(unsigned int *)m_LowResImageSample = 0;
#endif

	Assert( m_arrResourcesInfo.Count() == 0 );
	Assert( m_arrResourcesData.Count() == 0 );
	Assert( m_arrResourcesData_ForReuse.Count() == 0 );

	memset( &m_Options, 0, sizeof( m_Options ) );
	m_Options.cbSize = sizeof( m_Options );

	m_nFinestMipmapLevel = 0;
	m_nCoarsestMipmapLevel = 0;
}

CVTFTexture::~CVTFTexture()
{
	Shutdown();
}

//-----------------------------------------------------------------------------
// Compute the mip count based on the size + flags
//-----------------------------------------------------------------------------
int CVTFTexture::ComputeMipCount() const
{
	if ( IsX360() && ( m_nVersion[0] == VTF_X360_MAJOR_VERSION ) && ( m_nFlags & TEXTUREFLAGS_NOMIP ) )
	{
		// 360 vtf format culled unused mips at conversion time
		return 1;
	}

	// NOTE: No matter what, all mip levels should be created because
	// we have to worry about various fallbacks
	return ImageLoader::GetNumMipMapLevels( m_nWidth, m_nHeight, m_nDepth );
}


//-----------------------------------------------------------------------------
// Allocate data blocks with an eye toward re-using memory
//-----------------------------------------------------------------------------

static bool GenericAllocateReusableData( unsigned char **ppData, int *pNumAllocated, int numRequested )
{
	// If we're asking for memory and we have way more than we expect, free some.
	if ( *pNumAllocated < numRequested || ( numRequested > 0 && *pNumAllocated > 16 * numRequested ) )
	{
		delete [] *ppData;
		*ppData = new unsigned char[ numRequested ];
		if ( *ppData )
		{
			*pNumAllocated = numRequested;
			return true;
		}

		*pNumAllocated = 0;
		return false;
	}

	return true;
}

bool CVTFTexture::AllocateImageData( int nMemorySize )
{
	return GenericAllocateReusableData( &m_pImageData, &m_nImageAllocSize, nMemorySize );
}

bool CVTFTexture::ResourceMemorySection::AllocateData( int nMemorySize )
{
	if ( GenericAllocateReusableData( &m_pData, &m_nDataAllocSize, nMemorySize ) )
	{
		m_nDataLength = nMemorySize;
		return true;
	}

	return false;
}

bool CVTFTexture::AllocateLowResImageData( int nMemorySize )
{
	return GenericAllocateReusableData( &m_pLowResImageData, &m_nLowResImageAllocSize, nMemorySize );
}

inline bool IsMultipleOf4( int value )
{
	// NOTE: This catches powers of 2 less than 4 also
	return ( value <= 2 ) || ( (value & 0x3) == 0 );
}


//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
bool CVTFTexture::Init( int nWidth, int nHeight, int nDepth, ImageFormat fmt, int iFlags, int iFrameCount, int nForceMipCount )
{
	if ( nDepth == 0 )
	{
		nDepth = 1;
	}

	if (iFlags & TEXTUREFLAGS_ENVMAP)
	{
		if (nWidth != nHeight)
		{
			Warning( "Height and width must be equal for cubemaps!\n" );
			return false;
		}
		if (nDepth != 1)
		{
			Warning( "Depth must be 1 for cubemaps!\n" );
			return false;
		}
	}

	if ( !IsMultipleOf4( nWidth ) || !IsMultipleOf4( nHeight ) || !IsMultipleOf4( nDepth ) )
	{
		Warning( "Image dimensions must be multiple of 4!\n" );
		return false;
	}

	if ( fmt == IMAGE_FORMAT_DEFAULT )
	{
		fmt = IMAGE_FORMAT_RGBA8888;
	}

	m_nWidth = nWidth;
	m_nHeight = nHeight;
	m_nDepth = nDepth;
	m_Format = fmt;
	m_nFlags = iFlags;

	// THIS CAUSED A BUG!!!  We want all of the mip levels in the vtf file even with nomip in case we have lod.
	// NOTE: But we don't want more than 1 mip level for procedural textures
	if ( (iFlags & (TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_PROCEDURAL)) == (TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_PROCEDURAL) )
	{
		nForceMipCount = 1;
	}

	if ( nForceMipCount == -1 )
	{
		m_nMipCount = ComputeMipCount();
	}
	else
	{
		m_nMipCount = nForceMipCount;
	}

	m_nFrameCount = iFrameCount;

	m_nFaceCount = (iFlags & TEXTUREFLAGS_ENVMAP) ? CUBEMAP_FACE_COUNT : 1;
	if ( IsX360() && ( iFlags & TEXTUREFLAGS_ENVMAP ) )
	{
		// 360 has no reason to support sphere map
		m_nFaceCount = CUBEMAP_FACE_COUNT-1;
	}

#if defined( _X360 )
	m_nMipSkipCount = 0;
#endif

	// Need to do this because Shutdown deallocates the low-res image
	m_nLowResImageWidth = m_nLowResImageHeight = 0;

	// Allocate me some bits!
	int iMemorySize = ComputeTotalSize();
	if ( !AllocateImageData( iMemorySize ) )
		return false;

	// As soon as we have image indicate so in the resources
	if ( iMemorySize )
		FindOrCreateResourceEntryInfo( VTF_LEGACY_RSRC_IMAGE );
	else
		RemoveResourceEntryInfo( VTF_LEGACY_RSRC_IMAGE );

	return true;
}

//-----------------------------------------------------------------------------
// Methods to initialize the low-res image
//-----------------------------------------------------------------------------
void CVTFTexture::InitLowResImage( int nWidth, int nHeight, ImageFormat fmt )
{
	m_nLowResImageWidth = nWidth;
	m_nLowResImageHeight = nHeight;
	m_LowResImageFormat = fmt;

	// Allocate low-res bits
	int iLowResImageSize = ImageLoader::GetMemRequired( m_nLowResImageWidth, 
		m_nLowResImageHeight, 1, m_LowResImageFormat, false );

	if ( !AllocateLowResImageData( iLowResImageSize ) )
		return;

	// As soon as we have low-res image indicate so in the resources
	if ( iLowResImageSize )
		FindOrCreateResourceEntryInfo( VTF_LEGACY_RSRC_LOW_RES_IMAGE );
	else
		RemoveResourceEntryInfo( VTF_LEGACY_RSRC_LOW_RES_IMAGE );
}


//-----------------------------------------------------------------------------
// Methods to set other texture fields
//-----------------------------------------------------------------------------
void CVTFTexture::SetBumpScale( float flScale )
{
	m_flBumpScale = flScale;
}

void CVTFTexture::SetReflectivity( const Vector &vecReflectivity )
{
	VectorCopy( vecReflectivity, m_vecReflectivity );
}

// Sets threshhold values for alphatest mipmapping
void CVTFTexture::SetAlphaTestThreshholds( float flBase, float flHighFreq )
{
	m_flAlphaThreshhold = flBase;
	m_flAlphaHiFreqThreshhold = flHighFreq;
}

//-----------------------------------------------------------------------------
// Release and reset the resources.
//-----------------------------------------------------------------------------
void CVTFTexture::ReleaseResources()
{
	m_arrResourcesInfo.RemoveAll();

	for ( ResourceMemorySection *pRms = m_arrResourcesData.Base(),
		 *pRmsEnd = pRms + m_arrResourcesData.Count(); pRms < pRmsEnd; ++pRms )
	{
		delete [] pRms->m_pData;
	}
	m_arrResourcesData.RemoveAll();

	for ( ResourceMemorySection *pRms = m_arrResourcesData_ForReuse.Base(),
		 *pRmsEnd = pRms + m_arrResourcesData_ForReuse.Count(); pRms < pRmsEnd; ++pRms )
	{
		delete [] pRms->m_pData;
	}
	m_arrResourcesData_ForReuse.RemoveAll();
}

//-----------------------------------------------------------------------------
// Shutdown
//-----------------------------------------------------------------------------
void CVTFTexture::Shutdown()
{
#if defined( _X360 )
	// must be first to ensure X360 aliased pointers are unhooked, otherwise memory corruption
	ReleaseImageMemory();
#endif

	delete[] m_pImageData;
	m_pImageData = NULL;
	m_nImageAllocSize = 0;

	delete[] m_pLowResImageData;
	m_pLowResImageData = NULL;
	m_nLowResImageAllocSize = 0;

	ReleaseResources();
}

//-----------------------------------------------------------------------------
// These are methods to help with optimization of file access
//-----------------------------------------------------------------------------
void CVTFTexture::LowResFileInfo( int *pStartLocation, int *pSizeInBytes ) const
{
	// Once the header is read in, they indicate where to start reading
	// other data, and how many bytes to read....

	if ( ResourceEntryInfo const *pLowResData = FindResourceEntryInfo( VTF_LEGACY_RSRC_LOW_RES_IMAGE ) )
	{
		*pStartLocation = pLowResData->resData;
		*pSizeInBytes = ImageLoader::GetMemRequired( m_nLowResImageWidth, 
			m_nLowResImageHeight, 1, m_LowResImageFormat, false );
	}
	else
	{
		*pStartLocation = 0;
		*pSizeInBytes = 0;
	}
}

void CVTFTexture::ImageFileInfo( int nFrame, int nFace, int nMipLevel, int *pStartLocation, int *pSizeInBytes) const
{
	int i;
	int iMipWidth;
	int iMipHeight;
	int iMipDepth;

	ResourceEntryInfo const *pImageDataInfo = FindResourceEntryInfo( VTF_LEGACY_RSRC_IMAGE );

	if ( pImageDataInfo == NULL )
	{
		// This should never happen for real, but can happen if someone intentionally fed us a bad VTF.
		Assert( pImageDataInfo );
		( *pStartLocation ) = 0;
		( *pSizeInBytes ) = 0;
		return;
	}

	// The image data start offset
	int nOffset = pImageDataInfo->resData;

	// get to the right miplevel
	for( i = m_nMipCount - 1; i > nMipLevel; --i )
	{
		ComputeMipLevelDimensions( i, &iMipWidth, &iMipHeight, &iMipDepth );
		int iMipLevelSize = ImageLoader::GetMemRequired( iMipWidth, iMipHeight, iMipDepth, m_Format, false );
		nOffset += iMipLevelSize * m_nFrameCount * m_nFaceCount;
	}

	// get to the right frame
	ComputeMipLevelDimensions( nMipLevel, &iMipWidth, &iMipHeight, &iMipDepth );
	int nFaceSize = ImageLoader::GetMemRequired( iMipWidth, iMipHeight, iMipDepth, m_Format, false );

	// For backwards compatibility, we don't read in the spheremap fallback on
	// older format .VTF files...
	int nFacesToRead = m_nFaceCount;
	if ( IsCubeMap() )
	{
		if ((m_nVersion[0] == 7) && (m_nVersion[1] < 1))
		{
			nFacesToRead = 6;
			if (nFace == CUBEMAP_FACE_SPHEREMAP)
			{
				--nFace;
			}
		}
	}

	int nFrameSize = nFacesToRead * nFaceSize;
	nOffset += nFrameSize * nFrame;
	
	// get to the right face
	nOffset += nFace * nFaceSize;
	
	*pStartLocation = nOffset;
	*pSizeInBytes = nFaceSize;
}

int CVTFTexture::FileSize( int nMipSkipCount ) const
{
	ResourceEntryInfo const *pImageDataInfo = FindResourceEntryInfo( VTF_LEGACY_RSRC_IMAGE );

	// Can be null when someone gives us an intentionally malformed VTF.
	if ( pImageDataInfo == NULL )
	{
		// Still do the assert so we can catch this in debug--we don't expect this for well formed files.
		Assert( pImageDataInfo != NULL );
		return 0;
	}

	int nOffset = pImageDataInfo->resData;

	int nFaceSize = ComputeFaceSize( nMipSkipCount );
	int nImageSize = nFaceSize * m_nFaceCount * m_nFrameCount;
	return nOffset + nImageSize;
}

//-----------------------------------------------------------------------------
// Unserialization of low-res data
//-----------------------------------------------------------------------------
bool CVTFTexture::LoadLowResData( CUtlBuffer &buf )
{
	// Allocate low-res bits
	InitLowResImage( m_nLowResImageWidth, m_nLowResImageHeight, m_LowResImageFormat );
	int nLowResImageSize = ImageLoader::GetMemRequired( m_nLowResImageWidth, 
		m_nLowResImageHeight, 1, m_LowResImageFormat, false );
	buf.Get( m_pLowResImageData, nLowResImageSize );

	bool bValid = buf.IsValid();

	return bValid;
}

//-----------------------------------------------------------------------------
// Unserialization of image data
//-----------------------------------------------------------------------------
bool CVTFTexture::LoadImageData( CUtlBuffer &buf, const VTFFileHeader_t &header, int nSkipMipLevels )
{
	// Fix up the mip count + size based on how many mip levels we skip...
	if (nSkipMipLevels > 0)
	{
		Assert( m_nMipCount > nSkipMipLevels );
		if (header.numMipLevels < nSkipMipLevels)
		{
			// NOTE: This can only happen with older format .vtf files
			Warning("Warning! Encountered old format VTF file; please rebuild it!\n");
			return false;
		}

		ComputeMipLevelDimensions( nSkipMipLevels, &m_nWidth, &m_nHeight, &m_nDepth );
		m_nMipCount -= nSkipMipLevels;
	}

	// read the texture image (including mipmaps if they are there and needed.)
	int iImageSize = ComputeFaceSize();
	iImageSize *= m_nFaceCount * m_nFrameCount;

	// For backwards compatibility, we don't read in the spheremap fallback on
	// older format .VTF files...
	int nFacesToRead = m_nFaceCount;
	if ( IsCubeMap() )
	{
		if ((header.version[0] == 7) && (header.version[1] < 1))
			nFacesToRead = 6;
	}

	// Even if we are preloading partial data, always do the full allocation here. We'll use LOD clamping to ensure we only 
	// reference data that is available.
	if ( !AllocateImageData( iImageSize ) )
		return false;

	// We may only have part of the data available--if so we will stream in the rest later. 
	// If we have the data available but we're ignoring it (for example during development), then we 
	// need to skip over the data we're ignoring below, otherwise we'll be sad pandas.
	bool bMipDataPresent = true;
	int nFirstAvailableMip = 0;
	int nLastAvailableMip = m_nMipCount - 1;
	TextureStreamSettings_t const *pStreamSettings = ( TextureStreamSettings_t const * ) GetResourceData( VTF_RSRC_TEXTURE_STREAM_SETTINGS, NULL );
	if ( pStreamSettings ) 
	{
		nFirstAvailableMip = Max( 0, pStreamSettings->m_firstAvailableMip - nSkipMipLevels );
		nLastAvailableMip = Max( 0, pStreamSettings->m_lastAvailableMip - nSkipMipLevels );
		bMipDataPresent = false;
	} 
	
	// If we have coarse mips but not the fine mips (yet)
	if ( ( header.flags & TEXTUREFLAGS_STREAMABLE ) == TEXTUREFLAGS_STREAMABLE_COARSE )
	{
		nFirstAvailableMip = Max( 0, Max( nFirstAvailableMip, STREAMING_START_MIPMAP ) - nSkipMipLevels );
	}

	if ( header.flags & TEXTUREFLAGS_STREAMABLE_FINE )
	{
		// Don't need to subtract nSkipMipLevels: m_nMipCount has subtracted that above--assuming this assert doesn't fire.
		Assert( m_nMipCount == header.numMipLevels - nSkipMipLevels );
		nLastAvailableMip = Min( nLastAvailableMip, STREAMING_START_MIPMAP - 1 );
	}

	// Valid settings? 
	Assert( nFirstAvailableMip >= 0 && nFirstAvailableMip <= nLastAvailableMip && nLastAvailableMip < m_nMipCount );

	// Store the clamp settings
	m_nFinestMipmapLevel = nFirstAvailableMip;
	m_nCoarsestMipmapLevel = nLastAvailableMip;

	// NOTE: The mip levels are stored ascending from smallest (1x1) to largest (NxN)
	// in order to allow for truncated reads of the minimal required data
	for (int iMip = m_nMipCount; --iMip >= 0; )
	{
		// NOTE: This is for older versions...
		if (header.numMipLevels - nSkipMipLevels <= iMip)
			continue;

		int iMipSize = ComputeMipSize( iMip );

		// Skip over any levels we don't have data for--we'll get them later. 
		if ( iMip > nLastAvailableMip || iMip < nFirstAvailableMip )
		{
			// If the data is there but we're ignoring it, need to update the get pointer.
			if ( bMipDataPresent ) 
			{
				for ( int iFrame = 0; iFrame < m_nFrameCount; ++iFrame )
					for ( int iFace = 0; iFace < nFacesToRead; ++iFace )
						buf.SeekGet( CUtlBuffer::SEEK_CURRENT, iMipSize );
			}
			continue;
		}

		for (int iFrame = 0; iFrame < m_nFrameCount; ++iFrame)
		{
			for (int iFace = 0; iFace < nFacesToRead; ++iFace)
			{
				// printf("\n tex %p mip %i frame %i face %i  size %i  buf offset %i", this, iMip, iFrame, iFace, iMipSize, buf.TellGet() );
				unsigned char *pMipBits = ImageData( iFrame, iFace, iMip );
				buf.Get( pMipBits, iMipSize );
			}
		}
	}

	return buf.IsValid();
}

void *CVTFTexture::SetResourceData( uint32 eType, void const *pData, size_t nNumBytes )
{
	Assert( ( eType & RSRCF_MASK ) == 0 );
	eType &= ~RSRCF_MASK;

	// Very inefficient to set less than 4 bytes of data
	Assert( !nNumBytes || ( nNumBytes >= sizeof( uint32 ) ) );

	if ( nNumBytes )
	{
		ResourceEntryInfo *pInfo = FindOrCreateResourceEntryInfo( eType );
		int idx = pInfo - m_arrResourcesInfo.Base();
		ResourceMemorySection &rms = m_arrResourcesData[ idx ];

		if ( nNumBytes == sizeof( pInfo->resData ) )
		{
			// store 4 bytes directly
			pInfo->eType |= RSRCF_HAS_NO_DATA_CHUNK;
			if ( pData )
				pInfo->resData = reinterpret_cast< const int * >( pData )[0];
			return &pInfo->resData;
		}
		else
		{
			if ( !rms.AllocateData( nNumBytes ) )
			{
				RemoveResourceEntryInfo( eType );
				return NULL;
			}

			if ( pData )
				memcpy( rms.m_pData, pData, nNumBytes );
			return rms.m_pData;
		}
	}
	else
	{
		RemoveResourceEntryInfo( eType );
		return NULL;
	}
}

void *CVTFTexture::GetResourceData( uint32 eType, size_t *pDataSize ) const
{
	Assert( ( eType & RSRCF_MASK ) == 0 );
	eType &= ~RSRCF_MASK;

	ResourceEntryInfo const *pInfo = FindResourceEntryInfo( eType );
	if ( pInfo )
	{
		if ( ( pInfo->eType & RSRCF_HAS_NO_DATA_CHUNK ) == 0 )
		{
			int idx = pInfo - m_arrResourcesInfo.Base();
			ResourceMemorySection const &rms = m_arrResourcesData[ idx ];
			if ( pDataSize )
			{
				*pDataSize = rms.m_nDataLength;
			}
			return rms.m_pData;
		}
		else
		{
			if ( pDataSize )
			{
				*pDataSize = sizeof( pInfo->resData );
			}
			return (void *)&pInfo->resData;
		}
	}
	else
	{
		if ( pDataSize )
			*pDataSize = 0;
	}

	return NULL;
}

bool CVTFTexture::HasResourceEntry( uint32 eType ) const
{
	return ( FindResourceEntryInfo( eType ) != NULL );
}

unsigned int CVTFTexture::GetResourceTypes( unsigned int *arrTypesBuffer, int numTypesBufferElems ) const
{
	for ( ResourceEntryInfo const *pInfo = m_arrResourcesInfo.Base(),
		  *pInfoEnd = pInfo + m_arrResourcesInfo.Count();
		  numTypesBufferElems-- > 0 && pInfo < pInfoEnd; )
	{
		*( arrTypesBuffer++ ) = ( ( pInfo++ )->eType & ~RSRCF_MASK );
	}

	return m_arrResourcesInfo.Count();
}


//-----------------------------------------------------------------------------
// Serialization/Unserialization of resource data
//-----------------------------------------------------------------------------
bool CVTFTexture::ResourceMemorySection::LoadData( CUtlBuffer &buf, CByteswap &byteSwap )
{
	// Read the size
	int iDataSize = 0;
	buf.Get( &iDataSize, sizeof( iDataSize ) );
	byteSwap.SwapBufferToTargetEndian( &iDataSize );

	// Read the actual data
	if ( !AllocateData( iDataSize ) )
		return false;

	buf.Get( m_pData, iDataSize );

	// Test valid
	bool bValid = buf.IsValid();

	return bValid;
}

bool CVTFTexture::ResourceMemorySection::WriteData( CUtlBuffer &buf ) const
{
	Assert( m_nDataLength && m_pData );
	int iBufSize = m_nDataLength;
	
	buf.Put( &iBufSize, sizeof( iBufSize ) );
	buf.Put( m_pData, m_nDataLength );

	return buf.IsValid();
}


//-----------------------------------------------------------------------------
// Checks if the file data needs to be swapped
//-----------------------------------------------------------------------------
bool CVTFTexture::SetupByteSwap( CUtlBuffer &buf )
{
	VTFFileBaseHeader_t *header = (VTFFileBaseHeader_t*)buf.PeekGet();

	if ( header->version[0] == SwapLong( VTF_MAJOR_VERSION ) )
	{
		m_Swap.ActivateByteSwapping( true );
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Unserialization
//-----------------------------------------------------------------------------
static bool ReadHeaderFromBufferPastBaseHeader( CUtlBuffer &buf, VTFFileHeader_t &header )
{
	unsigned char *pBuf = (unsigned char*)(&header) + sizeof(VTFFileBaseHeader_t);
	if ( header.version[1] == VTF_MINOR_VERSION )
	{
		buf.Get( pBuf, sizeof(VTFFileHeader_t) - sizeof(VTFFileBaseHeader_t) );
	}
	else if ( header.version[1] == 3 )
	{
		buf.Get( pBuf, sizeof(VTFFileHeaderV7_3_t) - sizeof(VTFFileBaseHeader_t) );
	}
	else if ( header.version[1] == 2 )
	{
		buf.Get( pBuf, sizeof(VTFFileHeaderV7_2_t) - sizeof(VTFFileBaseHeader_t) );

		#if defined( _X360 ) || defined (POSIX)
			// read 15 dummy bytes to be properly positioned with 7.2 PC data
			byte dummy[15];
			buf.Get( dummy, 15 );
		#endif
	}
	else if ( header.version[1] == 1 || header.version[1] == 0 )
	{
		// previous version 7.0 or 7.1
		buf.Get( pBuf, sizeof(VTFFileHeaderV7_1_t) - sizeof(VTFFileBaseHeader_t) );

		#if defined( _X360 ) || defined (POSIX)
			// read a dummy byte to be properly positioned with 7.0/1 PC data
			byte dummy;
			buf.Get( &dummy, 1 );
		#endif
	}
	else
	{
		Warning( "*** Encountered VTF file with an invalid minor version!\n" );
		return false;
	}

	return buf.IsValid();
}

bool CVTFTexture::ReadHeader( CUtlBuffer &buf, VTFFileHeader_t &header )
{
	if ( IsX360() && SetupByteSwap( buf ) )
	{
		VTFFileBaseHeader_t baseHeader;
		m_Swap.SwapFieldsToTargetEndian( &baseHeader, (VTFFileBaseHeader_t*)buf.PeekGet() );

		// Swap the header inside the UtlBuffer
		if ( baseHeader.version[0] == VTF_MAJOR_VERSION )
		{
			if ( baseHeader.version[1] == 0 || baseHeader.version[1] == 1 )
			{
				// version 7.0 or 7.1
				m_Swap.SwapFieldsToTargetEndian( (VTFFileHeaderV7_1_t*)buf.PeekGet() );
			}
			else if ( baseHeader.version[1] == 2 )
			{
				// version 7.2
				m_Swap.SwapFieldsToTargetEndian( (VTFFileHeaderV7_2_t*)buf.PeekGet() );
			}
			else if ( baseHeader.version[1] == 3 )
			{
				m_Swap.SwapFieldsToTargetEndian( (VTFFileHeaderV7_3_t*)buf.PeekGet() );
			}
			else if ( baseHeader.version[1] == VTF_MINOR_VERSION )
			{
				m_Swap.SwapFieldsToTargetEndian( (VTFFileHeader_t*)buf.PeekGet() );
			}
		}
	}

	memset( &header, 0, sizeof(VTFFileHeader_t) );
	buf.Get( &header, sizeof(VTFFileBaseHeader_t) );
	if ( !buf.IsValid() )
	{
		Warning( "*** Error unserializing VTF file... is the file empty?\n" );
		return false;
	}

	// Validity check
	if ( Q_strncmp( header.fileTypeString, "VTF", 4 ) )
	{
		Warning( "*** Tried to load a non-VTF file as a VTF file!\n" );
		return false;
	}

	if ( header.version[0] != VTF_MAJOR_VERSION )
	{
		Warning( "*** Encountered VTF file with an invalid version!\n" );
		return false;
	}

	if ( !ReadHeaderFromBufferPastBaseHeader( buf, header ) )
	{
		Warning( "*** Encountered VTF file with an invalid full header!\n" );
		return false;
	}

	// version fixups 
	switch ( header.version[1] )
	{
	case 0:
	case 1:
		header.depth = 1;
		// fall-through
	case 2:
		header.numResources = 0;
		// fall-through
	case 3:
		header.flags &= VERSIONED_VTF_FLAGS_MASK_7_3;
		// fall-through
	case VTF_MINOR_VERSION:
		break;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Unserialization
//-----------------------------------------------------------------------------
bool CVTFTexture::Unserialize( CUtlBuffer &buf, bool bHeaderOnly, int nSkipMipLevels )
{
	return UnserializeEx( buf, bHeaderOnly, 0, nSkipMipLevels );
}

bool CVTFTexture::UnserializeEx( CUtlBuffer &buf, bool bHeaderOnly, int nForceFlags, int nSkipMipLevels )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s (header only: %d, nForceFlags: %d, skipMips: %d)", __FUNCTION__, bHeaderOnly ? 1 : 0, nForceFlags, nSkipMipLevels );

	// When unserializing, we can skip a certain number of mip levels,
	// and we also can just load everything but the image data
	VTFFileHeader_t header;

	if ( !ReadHeader( buf, header ) )
		return false;

	// Pretend these flags are also set.
	header.flags |= nForceFlags;

	if ( (header.flags & TEXTUREFLAGS_ENVMAP) && (header.width != header.height) )
	{
		Warning( "*** Encountered VTF non-square cubemap!\n" );
		return false;
	}
	if ( (header.flags & TEXTUREFLAGS_ENVMAP) && (header.depth != 1) )
	{
		Warning( "*** Encountered VTF volume texture cubemap!\n" );
		return false;
	}
	if ( header.width <= 0 || header.height <= 0 || header.depth <= 0 )
	{
		Warning( "*** Encountered VTF invalid texture size!\n" );
		return false;
	}
	if ( ( header.imageFormat < IMAGE_FORMAT_UNKNOWN ) || ( header.imageFormat >= NUM_IMAGE_FORMATS ) )
	{
		Warning( "*** Encountered VTF invalid image format!\n" );
		return false;
	}
	
	// If the header says we should be doing a texture allocation of more than 32M, just tell the caller we failed.
	const int cMaxImageSizeLog2 = Q_log2( 32 * 1024 * 1024 );
	if ( ( Q_log2( header.width ) + Q_log2( header.height ) + Q_log2( header.depth ) + Q_log2( header.numFrames ) > cMaxImageSizeLog2 ) || ( header.numResources > MAX_RSRC_DICTIONARY_ENTRIES ) )
	{
		STAGING_ONLY_EXEC( DevWarning( "Asked for a large texture to be created (%d h x %d w x %d d x %d f). Nope.\n", header.width, header.height, header.depth, header.numFrames ) );
		return false;
	}

	m_nWidth = header.width;
	m_nHeight = header.height;
	m_nDepth = header.depth;
	m_Format = header.imageFormat;
	m_nFlags = header.flags;
	m_nFrameCount = header.numFrames;


	m_nFaceCount = (m_nFlags & TEXTUREFLAGS_ENVMAP) ? CUBEMAP_FACE_COUNT : 1;

	// NOTE: We're going to store space for all mip levels, even if we don't 
	// have data on disk for them. This is for backward compatibility
	m_nMipCount = ComputeMipCount();

	m_nFinestMipmapLevel = 0;
	m_nCoarsestMipmapLevel = m_nMipCount - 1;

	m_vecReflectivity = header.reflectivity;
	m_flBumpScale = header.bumpScale;

	// FIXME: Why is this needed?
	m_iStartFrame = header.startFrame;

	// This is to make sure old-format .vtf files are read properly
	m_nVersion[0] = header.version[0];
	m_nVersion[1] = header.version[1];

	if ( header.lowResImageWidth == 0 || header.lowResImageHeight == 0 )
	{
		m_nLowResImageWidth = 0;
		m_nLowResImageHeight = 0;
	}
	else
	{
		m_nLowResImageWidth = header.lowResImageWidth;
		m_nLowResImageHeight = header.lowResImageHeight;
	}
	m_LowResImageFormat = header.lowResImageFormat;

	// invalid image format
	if ( ( m_LowResImageFormat < IMAGE_FORMAT_UNKNOWN ) || ( m_LowResImageFormat >= NUM_IMAGE_FORMATS ) )
		return false;

	// Keep the allocated memory chunks of data
	if ( int( header.numResources ) < m_arrResourcesData.Count() )
	{
		m_arrResourcesData_ForReuse.EnsureCapacity( m_arrResourcesData_ForReuse.Count() + m_arrResourcesData.Count() - header.numResources );
		for ( ResourceMemorySection const *pRms = &m_arrResourcesData[ header.numResources ],
			*pRmsEnd = m_arrResourcesData.Base() + m_arrResourcesData.Count(); pRms < pRmsEnd; ++ pRms )
		{
			if ( pRms->m_pData )
			{
				int idxReuse = m_arrResourcesData_ForReuse.AddToTail( *pRms );
				m_arrResourcesData_ForReuse[ idxReuse ].m_nDataLength = 0; // Data for reuse shouldn't have length set
			}
		}
	}
	m_arrResourcesData.SetCount( header.numResources );

	// Read the dictionary of resources info
	if ( header.numResources > 0 )
	{
		m_arrResourcesInfo.RemoveAll();
		m_arrResourcesInfo.SetCount( header.numResources );
		
		buf.Get( m_arrResourcesInfo.Base(), m_arrResourcesInfo.Count() * sizeof( ResourceEntryInfo ) );
		if ( !buf.IsValid() )
			return false;

		if ( IsX360() )
		{
			// Byte-swap the dictionary data offsets
			for ( int k = 0; k < m_arrResourcesInfo.Count(); ++ k )
			{
				ResourceEntryInfo &rei = m_arrResourcesInfo[k];
				if ( ( rei.eType & RSRCF_HAS_NO_DATA_CHUNK ) == 0 )
				{
					m_Swap.SwapBufferToTargetEndian( &rei.resData );
				}
			}
		}
	}
	else
	{
		// Older version (7.0 - 7.2):
		//	- low-res image data first (optional)
		//	- then image data
		m_arrResourcesInfo.RemoveAll();

		// Low-res image data
		int nLowResImageSize = ImageLoader::GetMemRequired( m_nLowResImageWidth, 
			m_nLowResImageHeight, 1, m_LowResImageFormat, false );
		if ( nLowResImageSize )
		{
			ResourceEntryInfo &rei = *FindOrCreateResourceEntryInfo( VTF_LEGACY_RSRC_LOW_RES_IMAGE );
			rei.resData = buf.TellGet();
		}
		
		// Image data
		ResourceEntryInfo &rei = *FindOrCreateResourceEntryInfo( VTF_LEGACY_RSRC_IMAGE );
		rei.resData = buf.TellGet() + nLowResImageSize;
	}

	// Caller wants the header component only, avoids reading large image data sets
	if ( bHeaderOnly )
		return true;

	// Load the low res image
	if ( ResourceEntryInfo const *pLowResDataInfo = FindResourceEntryInfo( VTF_LEGACY_RSRC_LOW_RES_IMAGE ) )
	{
		buf.SeekGet( CUtlBuffer::SEEK_HEAD, pLowResDataInfo->resData );
		if ( !LoadLowResData( buf ) )
			return false;
	}

	// Load any new resources
	if ( !LoadNewResources( buf ) )
	{
		return false;
	}

	// Load the image data
	if ( ResourceEntryInfo const *pImageDataInfo = FindResourceEntryInfo( VTF_LEGACY_RSRC_IMAGE ) )
	{
		buf.SeekGet( CUtlBuffer::SEEK_HEAD, pImageDataInfo->resData );
		if ( !LoadImageData( buf, header, nSkipMipLevels ) )
			return false;
	}
	else
	{
		// No image data
		return false;
	}

	return true;
}

void CVTFTexture::GetMipmapRange( int* pOutFinest, int* pOutCoarsest )
{
	if ( pOutFinest )
		*pOutFinest = m_nFinestMipmapLevel;

	if ( pOutCoarsest )
		*pOutCoarsest = m_nCoarsestMipmapLevel;
}

bool CVTFTexture::LoadNewResources( CUtlBuffer &buf )
{
	// Load the new resources
	for ( int idxRsrc = 0; idxRsrc < m_arrResourcesInfo.Count(); ++idxRsrc )
	{
		ResourceEntryInfo &rei = m_arrResourcesInfo[ idxRsrc ];
		ResourceMemorySection &rms = m_arrResourcesData[ idxRsrc ];

		if ( ( rei.eType & RSRCF_HAS_NO_DATA_CHUNK ) == 0 )
		{
			switch( rei.eType )
			{
			case VTF_LEGACY_RSRC_LOW_RES_IMAGE:
			case VTF_LEGACY_RSRC_IMAGE:
				// these legacy resources are loaded differently
				continue;

			default:
				buf.SeekGet( CUtlBuffer::SEEK_HEAD, rei.resData );
				if ( !rms.LoadData( buf, m_Swap ) )
					return false;
			}
		}
	}

	return true;
}

ResourceEntryInfo const *CVTFTexture::FindResourceEntryInfo( uint32 eType ) const
{
	Assert( ( eType & RSRCF_MASK ) == 0 );

	ResourceEntryInfo const *pRange[2];
	pRange[0] = m_arrResourcesInfo.Base();
	pRange[1] = pRange[0] + m_arrResourcesInfo.Count();

	if ( IsPC() )
	{
		// Quick-search in a sorted array
		ResourceEntryInfo const *pMid;
find_routine:
		if ( pRange[0] != pRange[1] )
		{
			pMid = pRange[0] + ( pRange[1] - pRange[0] ) / 2;
			if ( int diff = int( pMid->eType & ~RSRCF_MASK ) - int( eType ) )
			{
				int off = !( diff > 0 );
				pRange[ !off ] = pMid + off;
				goto find_routine;
			}
			else
				return pMid;
		}
		else
			return NULL;
	}
	else
	{
		// 360 eschews a sorted format due to endian issues
		// use a linear search for compatibility with reading pc formats
		for ( ; pRange[0] < pRange[1]; ++pRange[0] )
		{
			if ( ( pRange[0]->eType & ~RSRCF_MASK ) == eType )
				return pRange[0];
		}
	}

	return NULL;
}

ResourceEntryInfo * CVTFTexture::FindResourceEntryInfo( uint32 eType )
{
	return const_cast< ResourceEntryInfo * >(
		( ( CVTFTexture const * ) this )->FindResourceEntryInfo( eType ) );
}

ResourceEntryInfo * CVTFTexture::FindOrCreateResourceEntryInfo( uint32 eType )
{
	Assert( ( eType & RSRCF_MASK ) == 0 );

	int k = 0;
	for ( ; k < m_arrResourcesInfo.Count(); ++ k )
	{
		uint32 rsrcType = ( m_arrResourcesInfo[ k ].eType & ~RSRCF_MASK );
		if ( rsrcType == eType )
		{
			// found
			return &m_arrResourcesInfo[ k ];
		}

		// sort for PC only, 360 uses linear sort for compatibility with PC endian
		if ( IsPC() )
		{
			if ( rsrcType > eType )
				break;
		}
	}

	ResourceEntryInfo rei;
	memset( &rei, 0, sizeof( rei ) );
	rei.eType = eType;

	// Inserting before "k"
	if ( m_arrResourcesData_ForReuse.Count() )
	{
		m_arrResourcesData.InsertBefore( k, m_arrResourcesData_ForReuse[ m_arrResourcesData_ForReuse.Count() - 1 ] );
		m_arrResourcesData_ForReuse.FastRemove( m_arrResourcesData_ForReuse.Count() - 1 );
	}
	else
	{
		m_arrResourcesData.InsertBefore( k );
	}

	m_arrResourcesInfo.InsertBefore( k, rei );
	return &m_arrResourcesInfo[k];
}

bool CVTFTexture::RemoveResourceEntryInfo( uint32 eType )
{
	Assert( ( eType & RSRCF_MASK ) == 0 );

	for ( int k = 0; k < m_arrResourcesInfo.Count(); ++ k )
	{
		if ( ( m_arrResourcesInfo[ k ].eType & ~RSRCF_MASK ) == eType )
		{
			m_arrResourcesInfo.Remove( k );

			if ( m_arrResourcesData[k].m_pData )
			{
				int idxReuse = m_arrResourcesData_ForReuse.AddToTail( m_arrResourcesData[k] );
				m_arrResourcesData_ForReuse[ idxReuse ].m_nDataLength = 0; // Data for reuse shouldn't have length set
			}

			m_arrResourcesData.Remove( k );

			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Serialization of image data
//-----------------------------------------------------------------------------
bool CVTFTexture::WriteImageData( CUtlBuffer &buf )
{
	// NOTE: We load the bits this way because we store the bits in memory
	// differently that the way they are stored on disk; we store on disk
	// differently so we can only load up 
	// NOTE: The smallest mip levels are stored first!!
	for (int iMip = m_nMipCount; --iMip >= 0; )
	{
		int iMipSize = ComputeMipSize( iMip );

		for (int iFrame = 0; iFrame < m_nFrameCount; ++iFrame)
		{
			for (int iFace = 0; iFace < m_nFaceCount; ++iFace)
			{
				unsigned char *pMipBits = ImageData( iFrame, iFace, iMip );
				buf.Put( pMipBits, iMipSize );
			}
		}
	}

	return buf.IsValid();
}

// Inserts padding to have a multiple of "iAlignment" bytes in the buffer
// Returns number of pad bytes written
static int PadBuffer( CUtlBuffer &buf, int iAlignment )
{
	unsigned int uiCurrentBytes = buf.TellPut();
	int iPadBytes = AlignValue( uiCurrentBytes, iAlignment ) - uiCurrentBytes;

	// Fill data
	for ( int i=0; i<iPadBytes; i++ )
	{
		buf.PutChar( '\0' );
	}

	buf.SeekPut( CUtlBuffer::SEEK_HEAD, uiCurrentBytes + iPadBytes );

	return iPadBytes;
}

//-----------------------------------------------------------------------------
// Serialization
//-----------------------------------------------------------------------------
bool CVTFTexture::Serialize( CUtlBuffer &buf )
{
	if ( IsX360() )
	{
		// Unsupported path, 360 has no reason and cannot serialize
		Assert( 0 );
		return false;
	}

	if ( !m_pImageData )
	{
		Warning("*** Unable to serialize... have no image data!\n");
		return false;
	}

	VTFFileHeader_t header;
	memset( &header, 0, sizeof( header ) );
	Q_strncpy( header.fileTypeString, "VTF", 4 );
	header.version[0] = VTF_MAJOR_VERSION;
	header.version[1] = VTF_MINOR_VERSION;
	header.headerSize = sizeof(VTFFileHeader_t) + m_arrResourcesInfo.Count() * sizeof( ResourceEntryInfo );

	header.width = m_nWidth;
	header.height = m_nHeight;
	header.depth = m_nDepth;
	header.flags = m_nFlags;
	header.numFrames = m_nFrameCount;
	header.numMipLevels = m_nMipCount;
	header.imageFormat = m_Format;
	VectorCopy( m_vecReflectivity, header.reflectivity );
	header.bumpScale = m_flBumpScale;

	// FIXME: Why is this needed?
	header.startFrame = m_iStartFrame;

	header.lowResImageWidth = m_nLowResImageWidth;
	header.lowResImageHeight = m_nLowResImageHeight;
	header.lowResImageFormat = m_LowResImageFormat;

	header.numResources = m_arrResourcesInfo.Count();

	buf.Put( &header, sizeof(VTFFileHeader_t) );
	if ( !buf.IsValid() )
		return false;

	// Write the dictionary of resource entry infos
	int iSeekOffsetResInfoFixup = buf.TellPut();
	buf.Put( m_arrResourcesInfo.Base(), m_arrResourcesInfo.Count() * sizeof( ResourceEntryInfo ) );
	if ( !buf.IsValid() )
		return false;

	// Write the low res image first
	if ( ResourceEntryInfo *pRei = FindResourceEntryInfo( VTF_LEGACY_RSRC_LOW_RES_IMAGE ) )
	{
		pRei->resData = buf.TellPut();

		Assert( m_pLowResImageData );
		int iLowResImageSize = ImageLoader::GetMemRequired( m_nLowResImageWidth, 
			m_nLowResImageHeight, 1, m_LowResImageFormat, false );
		buf.Put( m_pLowResImageData, iLowResImageSize );
		if ( !buf.IsValid() )
			return false;
	}

	// Serialize the new resources
	for ( int iRsrc = 0; iRsrc < m_arrResourcesInfo.Count(); ++ iRsrc )
	{
		ResourceEntryInfo &rei = m_arrResourcesInfo[ iRsrc ];

		switch ( rei.eType )
		{
		case VTF_LEGACY_RSRC_LOW_RES_IMAGE:
		case VTF_LEGACY_RSRC_IMAGE:
			// written differently
			continue;

		default:
			{
				if ( rei.eType & RSRCF_HAS_NO_DATA_CHUNK )
					continue;
				rei.resData = buf.TellPut();
				ResourceMemorySection &rms = m_arrResourcesData[ iRsrc ];
				if ( !rms.WriteData( buf ) )
					return false;
			}
		}
	}

	// Write image data last
	if ( ResourceEntryInfo *pRei = FindResourceEntryInfo( VTF_LEGACY_RSRC_IMAGE ) )
	{
		pRei->resData = buf.TellPut();
		WriteImageData( buf );
	}
	else
		return false;

	// Now fixup the resources dictionary
	int iTotalBytesPut = buf.TellPut();
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, iSeekOffsetResInfoFixup );
	buf.Put( m_arrResourcesInfo.Base(), m_arrResourcesInfo.Count() * sizeof( ResourceEntryInfo ) );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, iTotalBytesPut );

	// Return if the buffer is valid
	return buf.IsValid();
}

//-----------------------------------------------------------------------------
// Attributes...
//-----------------------------------------------------------------------------
int CVTFTexture::Width() const
{
	return m_nWidth;
}

int CVTFTexture::Height() const
{
	return m_nHeight;
}

int CVTFTexture::Depth() const
{
	return m_nDepth;
}

int CVTFTexture::MipCount() const
{
	return m_nMipCount;
}

ImageFormat CVTFTexture::Format() const
{
	return m_Format;
}

int CVTFTexture::FaceCount() const
{
	return m_nFaceCount;
}

int CVTFTexture::FrameCount() const
{
	return m_nFrameCount;
}

int CVTFTexture::Flags() const
{
	return m_nFlags;
}

bool CVTFTexture::IsCubeMap() const
{
	return (m_nFlags & TEXTUREFLAGS_ENVMAP) != 0; 
}

bool CVTFTexture::IsNormalMap() const
{
	return (m_nFlags & TEXTUREFLAGS_NORMAL) != 0; 
}

bool CVTFTexture::IsVolumeTexture() const
{
	return (m_nDepth > 1); 
}

float CVTFTexture::BumpScale() const
{
	return m_flBumpScale;
}

const Vector &CVTFTexture::Reflectivity() const
{
	return m_vecReflectivity;
}

unsigned char *CVTFTexture::ImageData()
{
	return m_pImageData;
}

int CVTFTexture::LowResWidth() const
{
	return m_nLowResImageWidth;
}

int CVTFTexture::LowResHeight() const
{
	return m_nLowResImageHeight;
}

ImageFormat CVTFTexture::LowResFormat() const
{
	return m_LowResImageFormat;
}

unsigned char *CVTFTexture::LowResImageData()
{
	return m_pLowResImageData;
}

int CVTFTexture::RowSizeInBytes( int nMipLevel ) const
{
	int nWidth = (m_nWidth >> nMipLevel);
	if (nWidth < 1)
	{
		nWidth = 1;
	}
	return ImageLoader::SizeInBytes( m_Format ) * nWidth;
}


//-----------------------------------------------------------------------------
// returns the size of one face of a particular mip level
//-----------------------------------------------------------------------------
int CVTFTexture::FaceSizeInBytes( int nMipLevel ) const
{
	int nWidth = (m_nWidth >> nMipLevel);
	if (nWidth < 1)
	{
		nWidth = 1;
	}
	int nHeight = (m_nHeight >> nMipLevel);
	if (nHeight < 1)
	{
		nHeight = 1;
	}
	return ImageLoader::SizeInBytes( m_Format ) * nWidth * nHeight;
}


//-----------------------------------------------------------------------------
// Returns a pointer to the data associated with a particular frame, face, and mip level
//-----------------------------------------------------------------------------
unsigned char *CVTFTexture::ImageData( int iFrame, int iFace, int iMipLevel )
{
	Assert( m_pImageData );
	int iOffset = GetImageOffset( iFrame, iFace, iMipLevel, m_Format );
	return &m_pImageData[iOffset];
}


//-----------------------------------------------------------------------------
// Returns a pointer to the data associated with a particular frame, face, mip level, and offset
//-----------------------------------------------------------------------------
unsigned char *CVTFTexture::ImageData( int iFrame, int iFace, int iMipLevel, int x, int y, int z )
{
#ifdef _DEBUG
	int nWidth, nHeight, nDepth;
	ComputeMipLevelDimensions( iMipLevel, &nWidth, &nHeight, &nDepth );
	Assert( (x >= 0) && (x <= nWidth) && (y >= 0) && (y <= nHeight) && (z >= 0) && (z <= nDepth) );
#endif

	int nFaceBytes = FaceSizeInBytes( iMipLevel );
	int nRowBytes = RowSizeInBytes( iMipLevel );
	int nTexelBytes = ImageLoader::SizeInBytes( m_Format );

	unsigned char *pMipBits = ImageData( iFrame, iFace, iMipLevel );
	pMipBits += z * nFaceBytes + y * nRowBytes + x * nTexelBytes;
	return pMipBits;
}

//-----------------------------------------------------------------------------
// Computes the size (in bytes) of a single mipmap of a single face of a single frame 
//-----------------------------------------------------------------------------
inline int CVTFTexture::ComputeMipSize( int iMipLevel, ImageFormat fmt ) const
{
	Assert( iMipLevel < m_nMipCount );
	int w, h, d;
	ComputeMipLevelDimensions( iMipLevel, &w, &h, &d );
	return ImageLoader::GetMemRequired( w, h, d, fmt, false );		
}

int CVTFTexture::ComputeMipSize( int iMipLevel ) const
{
	// Version for the public interface; don't want to expose the fmt parameter
	return ComputeMipSize( iMipLevel, m_Format );
}


//-----------------------------------------------------------------------------
// Computes the size of a single face of a single frame 
// All mip levels starting at the specified mip level are included
//-----------------------------------------------------------------------------
inline int CVTFTexture::ComputeFaceSize( int iStartingMipLevel, ImageFormat fmt ) const
{
	int iSize = 0;
	int w = m_nWidth;
	int h = m_nHeight;
	int d = m_nDepth;

	for( int i = 0; i < m_nMipCount; ++i )
	{
		if (i >= iStartingMipLevel)
		{
			iSize += ImageLoader::GetMemRequired( w, h, d, fmt, false );
		}
		w >>= 1;
		h >>= 1;
		d >>= 1;
		if ( w < 1 )
		{
			w = 1;
		}
		if ( h < 1 )
		{
			h = 1;
		}
		if ( d < 1 )
		{
			d = 1;
		}
	}
	return iSize;
}

int CVTFTexture::ComputeFaceSize( int iStartingMipLevel ) const
{
	// Version for the public interface; don't want to expose the fmt parameter
	return ComputeFaceSize( iStartingMipLevel, m_Format );
}


//-----------------------------------------------------------------------------
// Computes the total size of all faces, all frames
//-----------------------------------------------------------------------------
inline int CVTFTexture::ComputeTotalSize( ImageFormat fmt ) const
{
	// Compute the number of bytes required to store a single face/frame
	int iMemRequired = ComputeFaceSize( 0, fmt );

	// Now compute the total image size
	return m_nFaceCount * m_nFrameCount * iMemRequired;
}

int CVTFTexture::ComputeTotalSize( ) const
{
	// Version for the public interface; don't want to expose the fmt parameter
	return ComputeTotalSize( m_Format );
}


//-----------------------------------------------------------------------------
// Computes the location of a particular frame, face, and mip level
//-----------------------------------------------------------------------------
int CVTFTexture::GetImageOffset( int iFrame, int iFace, int iMipLevel, ImageFormat fmt ) const
{
	Assert( iFrame < m_nFrameCount );
	Assert( iFace < m_nFaceCount );
	Assert( iMipLevel < m_nMipCount );

	int i;
	int iOffset = 0;

	if ( IsX360() && ( m_nVersion[0] == VTF_X360_MAJOR_VERSION ) )
	{
		// 360 data is stored same as disk, 1x1 up to NxN
		// get to the right miplevel
		int iMipWidth, iMipHeight, iMipDepth;
		for ( i = m_nMipCount - 1; i > iMipLevel; --i )
		{
			ComputeMipLevelDimensions( i, &iMipWidth, &iMipHeight, &iMipDepth );
			int iMipLevelSize = ImageLoader::GetMemRequired( iMipWidth, iMipHeight, iMipDepth, fmt, false );
			iOffset += m_nFrameCount * m_nFaceCount * iMipLevelSize;
		}

		// get to the right frame
		ComputeMipLevelDimensions( iMipLevel, &iMipWidth, &iMipHeight, &iMipDepth );
		int nFaceSize = ImageLoader::GetMemRequired( iMipWidth, iMipHeight, iMipDepth, fmt, false );
		iOffset += iFrame * m_nFaceCount * nFaceSize;
		
		// get to the right face
		iOffset += iFace * nFaceSize;

		return iOffset;
	}

	// get to the right frame
	int iFaceSize = ComputeFaceSize( 0, fmt );
	iOffset = iFrame * m_nFaceCount * iFaceSize;

	// Get to the right face
	iOffset += iFace * iFaceSize;

	// Get to the right mip level
	for (i = 0; i < iMipLevel; ++i)
	{
		iOffset += ComputeMipSize( i, fmt );
	}
	
	return iOffset;
}


//-----------------------------------------------------------------------------
// Computes the dimensions of a particular mip level
//-----------------------------------------------------------------------------
void CVTFTexture::ComputeMipLevelDimensions( int iMipLevel, int *pMipWidth, int *pMipHeight, int *pMipDepth ) const
{
	Assert( iMipLevel < m_nMipCount );

	*pMipWidth = m_nWidth >> iMipLevel;
	*pMipHeight = m_nHeight >> iMipLevel;
	*pMipDepth = m_nDepth >> iMipLevel;
	if ( *pMipWidth < 1 )
	{
		*pMipWidth = 1;
	}
	if ( *pMipHeight < 1 )
	{
		*pMipHeight = 1;
	}
	if ( *pMipDepth < 1 )
	{
		*pMipDepth = 1;
	}
}


//-----------------------------------------------------------------------------
// Computes the size of a subrect at a particular mip level
//-----------------------------------------------------------------------------
void CVTFTexture::ComputeMipLevelSubRect( Rect_t *pSrcRect, int nMipLevel, Rect_t *pSubRect ) const
{
	Assert( pSrcRect->x >= 0 && pSrcRect->y >= 0 && 
		(pSrcRect->x + pSrcRect->width <= m_nWidth) &&  
		(pSrcRect->y + pSrcRect->height <= m_nHeight) );
	
	if (nMipLevel == 0)
	{
		*pSubRect = *pSrcRect;
		return;
	}

	float flInvShrink = 1.0f / (float)(1 << nMipLevel);
	pSubRect->x = pSrcRect->x * flInvShrink;
	pSubRect->y = pSrcRect->y * flInvShrink;
	pSubRect->width = (int)ceil( (pSrcRect->x + pSrcRect->width) * flInvShrink ) - pSubRect->x;
	pSubRect->height = (int)ceil( (pSrcRect->y + pSrcRect->height) * flInvShrink ) - pSubRect->y;
}


//-----------------------------------------------------------------------------
// Converts the texture's image format. Use IMAGE_FORMAT_DEFAULT
// if you want to be able to use various tool functions below
//-----------------------------------------------------------------------------
void CVTFTexture::ConvertImageFormat( ImageFormat fmt, bool bNormalToDUDV )
{
	if ( !m_pImageData )
	{
		return;
	}

	if ( fmt == IMAGE_FORMAT_DEFAULT )
	{
		fmt = IMAGE_FORMAT_RGBA8888;
	}

	if ( bNormalToDUDV && !( fmt == IMAGE_FORMAT_UV88 || fmt == IMAGE_FORMAT_UVWQ8888 || fmt == IMAGE_FORMAT_UVLX8888 ) )
	{
		Assert( 0 );
		return;
	}

	if ( m_Format == fmt )
	{
		return;
	}

	if ( IsX360() && ( m_nVersion[0] == VTF_X360_MAJOR_VERSION ) )
	{
		// 360 textures should be baked in final format
		Assert( 0 );
		return;
	}

	// FIXME: Should this be re-written to not do an allocation?
	int iConvertedSize = ComputeTotalSize( fmt );

	unsigned char *pConvertedImage = new unsigned char[ iConvertedSize ];

	// This can happen for large, bogus textures.
	if ( !pConvertedImage )
		return;

	for (int iMip = 0; iMip < m_nMipCount; ++iMip)
	{
		int nMipWidth, nMipHeight, nMipDepth;
		ComputeMipLevelDimensions( iMip, &nMipWidth, &nMipHeight, &nMipDepth );

 		int nSrcFaceStride = ImageLoader::GetMemRequired( nMipWidth, nMipHeight, 1, m_Format, false ); 
 		int nDstFaceStride = ImageLoader::GetMemRequired( nMipWidth, nMipHeight, 1, fmt, false ); 

		for (int iFrame = 0; iFrame < m_nFrameCount; ++iFrame)
		{
			for (int iFace = 0; iFace < m_nFaceCount; ++iFace)
			{
				unsigned char *pSrcData = ImageData( iFrame, iFace, iMip );
				unsigned char *pDstData = pConvertedImage + 
					GetImageOffset( iFrame, iFace, iMip, fmt );

				for ( int z = 0; z < nMipDepth; ++z, pSrcData += nSrcFaceStride, pDstData += nDstFaceStride )
				{
					if( bNormalToDUDV )
					{
						if( fmt == IMAGE_FORMAT_UV88 )
						{
							ImageLoader::ConvertNormalMapRGBA8888ToDUDVMapUV88( pSrcData,
								nMipWidth, nMipHeight, pDstData );
						}
						else if( fmt == IMAGE_FORMAT_UVWQ8888 )
						{
							ImageLoader::ConvertNormalMapRGBA8888ToDUDVMapUVWQ8888( pSrcData,
								nMipWidth, nMipHeight, pDstData );
						}
						else if ( fmt == IMAGE_FORMAT_UVLX8888 )
						{
							ImageLoader::ConvertNormalMapRGBA8888ToDUDVMapUVLX8888( pSrcData,
								nMipWidth, nMipHeight, pDstData );
						}
						else
						{
							Assert( 0 );
							return;
						}
					}
					else
					{
						ImageLoader::ConvertImageFormat( pSrcData, m_Format, 
							pDstData, fmt, nMipWidth, nMipHeight );
					}
				}
			}
		}
	}

	if ( !AllocateImageData(iConvertedSize) )
		return;

	memcpy( m_pImageData, pConvertedImage, iConvertedSize );
	m_Format = fmt;

	if ( !ImageLoader::IsCompressed( fmt ) )
	{
		int nAlphaBits = ImageLoader::ImageFormatInfo( fmt ).m_NumAlphaBits;
		if ( nAlphaBits > 1 )
		{
			m_nFlags |= TEXTUREFLAGS_EIGHTBITALPHA;
			m_nFlags &= ~TEXTUREFLAGS_ONEBITALPHA;
		}
		if ( nAlphaBits <= 1 )
		{
			m_nFlags &= ~TEXTUREFLAGS_EIGHTBITALPHA;
			if ( nAlphaBits == 0 )
			{
				m_nFlags &= ~TEXTUREFLAGS_ONEBITALPHA;
			}
		}
	}
	else
	{
		// Only DXT5 has alpha bits
		if ( ( fmt == IMAGE_FORMAT_DXT1 ) || ( fmt == IMAGE_FORMAT_ATI2N ) || ( fmt == IMAGE_FORMAT_ATI1N ) )
		{
			m_nFlags &= ~(TEXTUREFLAGS_ONEBITALPHA|TEXTUREFLAGS_EIGHTBITALPHA);
		}
	}

	delete [] pConvertedImage;
}


//-----------------------------------------------------------------------------
// Enums + structures related to conversion from cube to spheremap
//-----------------------------------------------------------------------------
struct SphereCalc_t
{
	Vector dir;
	float m_flRadius;
	float m_flOORadius;
	float m_flRadiusSq;
	LookDir_t m_LookDir;
	Vector m_vecLookDir;
	unsigned char m_pColor[4];
	unsigned char **m_ppCubeFaces;
	int	m_iSize;
};


//-----------------------------------------------------------------------------
//
// Methods associated with computing a spheremap from a cubemap
//
//-----------------------------------------------------------------------------
static void CalcInit( SphereCalc_t *pCalc, int iSize, unsigned char **ppCubeFaces, LookDir_t lookDir = LOOK_DOWN_Z )
{
	// NOTE: Width + height should be the same
	pCalc->m_flRadius = iSize * 0.5f;
	pCalc->m_flRadiusSq = pCalc->m_flRadius * pCalc->m_flRadius;
	pCalc->m_flOORadius = 1.0f / pCalc->m_flRadius;
	pCalc->m_LookDir = lookDir;
    pCalc->m_ppCubeFaces = ppCubeFaces;
	pCalc->m_iSize = iSize;

	switch( lookDir)
	{
	case LOOK_DOWN_X:
		pCalc->m_vecLookDir.Init( 1, 0, 0 );
		break;

	case LOOK_DOWN_NEGX:
		pCalc->m_vecLookDir.Init( -1, 0, 0 );
		break;

	case LOOK_DOWN_Y:
		pCalc->m_vecLookDir.Init( 0, 1, 0 );
		break;

	case LOOK_DOWN_NEGY:
		pCalc->m_vecLookDir.Init( 0, -1, 0 );
		break;

	case LOOK_DOWN_Z:
		pCalc->m_vecLookDir.Init( 0, 0, 1 );
		break;

	case LOOK_DOWN_NEGZ:
		pCalc->m_vecLookDir.Init( 0, 0, -1 );
		break;
	}
}

static void TransformNormal( SphereCalc_t *pCalc, Vector& normal )
{
	Vector vecTemp = normal;

	switch( pCalc->m_LookDir)
	{
	// Look down +x
	case LOOK_DOWN_X:
		normal[0] = vecTemp[2];
		normal[2] = -vecTemp[0];
		break;

	// Look down -x
	case LOOK_DOWN_NEGX:
		normal[0] = -vecTemp[2];
		normal[2] = vecTemp[0];
		break;

	// Look down +y
	case LOOK_DOWN_Y:
		normal[0] = -vecTemp[0];
		normal[1] = vecTemp[2];
		normal[2] = vecTemp[1];
		break;

	// Look down -y
	case LOOK_DOWN_NEGY:
		normal[0] = vecTemp[0];
		normal[1] = -vecTemp[2];
		normal[2] = vecTemp[1];
		break;

	// Look down +z
	case LOOK_DOWN_Z:
		return;

	// Look down -z
	case LOOK_DOWN_NEGZ:
		normal[0] = -vecTemp[0];
		normal[2] = -vecTemp[2];
		break;
	}
}

//-----------------------------------------------------------------------------
// Given a iFace normal, determine which cube iFace to sample
//-----------------------------------------------------------------------------
static int CalcFaceIndex( const Vector& normal )
{
	float absx, absy, absz;

	absx = normal[0] >= 0 ? normal[0] : -normal[0];
	absy = normal[1] >= 0 ? normal[1] : -normal[1];
	absz = normal[2] >= 0 ? normal[2] : -normal[2];

	if ( absx > absy )
	{
		if ( absx > absz )
		{
			// left/right
			if ( normal[0] >= 0 )
				return CUBEMAP_FACE_RIGHT;
			return CUBEMAP_FACE_LEFT;
		}
	}
	else
	{
		if ( absy > absz )
		{
			// front / back
			if ( normal[1] >= 0 )
				return CUBEMAP_FACE_BACK;
			return CUBEMAP_FACE_FRONT;
		}
	}

	// top / bottom
	if ( normal[2] >= 0 )
		return CUBEMAP_FACE_UP;
	return CUBEMAP_FACE_DOWN;
}

static void CalcColor( SphereCalc_t *pCalc, int iFace, const Vector &normal, unsigned char *color )
{
	float x, y, w;

	int size = pCalc->m_iSize;
	float hw = 0.5 * size;
	
	if ( (iFace == CUBEMAP_FACE_LEFT) || (iFace == CUBEMAP_FACE_RIGHT) )
	{
		w = hw / normal[0];
		x = -normal[2];
		y = -normal[1];
		if ( iFace == CUBEMAP_FACE_LEFT )
			y = -y;
	}
	else if ( (iFace == CUBEMAP_FACE_FRONT) || (iFace == CUBEMAP_FACE_BACK) )
	{
		w = hw / normal[1];
		x = normal[0];
		y = normal[2];
		if ( iFace == CUBEMAP_FACE_FRONT )
			x = -x;
	}
	else
	{
		w = hw / normal[2];
		x = -normal[0];
		y = -normal[1];
		if ( iFace == CUBEMAP_FACE_UP )
			x = -x;
	}

	x = (x * w) + hw - 0.5;
	y = (y * w) + hw - 0.5;

	int u = (int)(x+0.5);
	int v = (int)(y+0.5);

	if ( u < 0 ) u = 0;
	else if ( u > (size-1) ) u = (size-1);

	if ( v < 0 ) v = 0;
	else if ( v > (size-1) ) v = (size-1);

	int offset = (v * size + u) * 4;

	unsigned char *pPix = pCalc->m_ppCubeFaces[iFace] + offset;
	color[0] = pPix[0];
	color[1] = pPix[1];
	color[2] = pPix[2];
	color[3] = pPix[3];
}

//-----------------------------------------------------------------------------
// Computes the spheremap color at a particular (x,y) texcoord
//-----------------------------------------------------------------------------
static void CalcSphereColor( SphereCalc_t *pCalc, float x, float y )
{
	Vector normal;
	float flRadiusSq = x*x + y*y;
	if (flRadiusSq > pCalc->m_flRadiusSq)
	{
		// Force a glancing reflection
		normal.Init( 0, 1, 0 );
	}
	else
	{
		// Compute the z distance based on x*x + y*y + z*z = r*r 
		float z = sqrt( pCalc->m_flRadiusSq - flRadiusSq );

		// Here's the untransformed surface normal
		normal.Init( x, y, z );
		normal *= pCalc->m_flOORadius;
	}

	// Transform the normal based on the actual view direction
	TransformNormal( pCalc, normal );

	// Compute the reflection vector (full spheremap solution)
	// R = 2 * (N dot L)N - L
	Vector vecReflect;
	float nDotL = DotProduct( normal, pCalc->m_vecLookDir );
	VectorMA( pCalc->m_vecLookDir, -2.0f * nDotL, normal, vecReflect );
	vecReflect *= -1.0f;

	int iFace = CalcFaceIndex( vecReflect );
	CalcColor( pCalc, iFace, vecReflect, pCalc->m_pColor );
}

//-----------------------------------------------------------------------------
// Computes the spheremap color at a particular (x,y) texcoord
//-----------------------------------------------------------------------------
static void CalcHemisphereColor( SphereCalc_t *pCalc, float x, float y )
{
	Vector normal;
	float flRadiusSq = x*x + y*y;
	if (flRadiusSq > pCalc->m_flRadiusSq)
	{
		normal.Init( x, y, 0.0f );
		VectorNormalize( normal );
		normal *= pCalc->m_flRadiusSq;
		flRadiusSq = pCalc->m_flRadiusSq;
	}

	// Compute the z distance based on x*x + y*y + z*z = r*r 
	float z = sqrt( pCalc->m_flRadiusSq - flRadiusSq );

	// Here's the untransformed surface normal
	normal.Init( x, y, z );
	normal *= pCalc->m_flOORadius;

	// Transform the normal based on the actual view direction
	TransformNormal( pCalc, normal );

//	printf( "x: %f y: %f normal: %f %f %f\n", x, y, normal.x, normal.y, normal.z );
	
	/*
	// Compute the reflection vector (full spheremap solution)
	// R = 2 * (N dot L)N - L
	Vector vecReflect;
	float nDotL = DotProduct( normal, pCalc->m_vecLookDir );
	VectorMA( pCalc->m_vecLookDir, -2.0f * nDotL, normal, vecReflect );
	vecReflect *= -1.0f;
*/

	int iFace = CalcFaceIndex( normal );
	CalcColor( pCalc, iFace, normal, pCalc->m_pColor );
#if 0
	pCalc->m_pColor[0] = normal[0] * 127 + 127;
	pCalc->m_pColor[1] = normal[1] * 127 + 127;
	pCalc->m_pColor[2] = normal[2] * 127 + 127;
#endif
}

//-----------------------------------------------------------------------------
// Makes a single frame of spheremap
//-----------------------------------------------------------------------------
void CVTFTexture::ComputeSpheremapFrame( unsigned char **ppCubeFaces, unsigned char *pSpheremap, LookDir_t lookDir )
{
	SphereCalc_t sphere;
	CalcInit( &sphere, m_nWidth, ppCubeFaces, lookDir );
	int offset = 0;
	for ( int y = 0; y < m_nHeight; y++ )
	{
		for ( int x = 0; x < m_nWidth; x++ )
		{
			int r = 0, g = 0, b = 0, a = 0;
			float u = (float)x - m_nWidth * 0.5f;
			float v = m_nHeight * 0.5f - (float)y;

			CalcSphereColor( &sphere, u, v );
			r += sphere.m_pColor[0];
			g += sphere.m_pColor[1];
			b += sphere.m_pColor[2];
			a += sphere.m_pColor[3];

			CalcSphereColor( &sphere, u + 0.25, v );
			r += sphere.m_pColor[0];
			g += sphere.m_pColor[1];
			b += sphere.m_pColor[2];
			a += sphere.m_pColor[3];

			v += 0.25;
			CalcSphereColor( &sphere, u + 0.25, v );
			r += sphere.m_pColor[0];
			g += sphere.m_pColor[1];
			b += sphere.m_pColor[2];
			a += sphere.m_pColor[3];

			CalcSphereColor( &sphere, u, v );
			r += sphere.m_pColor[0];
			g += sphere.m_pColor[1];
			b += sphere.m_pColor[2];
			a += sphere.m_pColor[3];

			pSpheremap[ offset + 0 ] = r >> 2;
			pSpheremap[ offset + 1 ] = g >> 2;
			pSpheremap[ offset + 2 ] = b >> 2;
			pSpheremap[ offset + 3 ] = a >> 2;
			offset += 4;
		}
	}
}

void CVTFTexture::ComputeHemispheremapFrame( unsigned char **ppCubeFaces, unsigned char *pSpheremap, LookDir_t lookDir )
{
	SphereCalc_t sphere;
	CalcInit( &sphere, m_nWidth, ppCubeFaces, lookDir );
	int offset = 0;
	for ( int y = 0; y < m_nHeight; y++ )
	{
		for ( int x = 0; x < m_nWidth; x++ )
		{
			int r = 0, g = 0, b = 0, a = 0;
			float u = (float)x - m_nWidth * 0.5f;
			float v = m_nHeight * 0.5f - (float)y;

			CalcHemisphereColor( &sphere, u, v );
			r += sphere.m_pColor[0];
			g += sphere.m_pColor[1];
			b += sphere.m_pColor[2];
			a += sphere.m_pColor[3];

			CalcHemisphereColor( &sphere, u + 0.25, v );
			r += sphere.m_pColor[0];
			g += sphere.m_pColor[1];
			b += sphere.m_pColor[2];
			a += sphere.m_pColor[3];

			v += 0.25;
			CalcHemisphereColor( &sphere, u + 0.25, v );
			r += sphere.m_pColor[0];
			g += sphere.m_pColor[1];
			b += sphere.m_pColor[2];
			a += sphere.m_pColor[3];

			CalcHemisphereColor( &sphere, u, v );
			r += sphere.m_pColor[0];
			g += sphere.m_pColor[1];
			b += sphere.m_pColor[2];
			a += sphere.m_pColor[3];

			pSpheremap[ offset + 0 ] = r >> 2;
			pSpheremap[ offset + 1 ] = g >> 2;
			pSpheremap[ offset + 2 ] = b >> 2;
			pSpheremap[ offset + 3 ] = a >> 2;
			offset += 4;
		}
	}
}

//-----------------------------------------------------------------------------
// Generate spheremap based on the current images (only works for cubemaps)
// The look dir indicates the direction of the center of the sphere
//-----------------------------------------------------------------------------
void CVTFTexture::GenerateSpheremap( LookDir_t lookDir )
{
	if (!IsCubeMap())
		return;

	// HDRFIXME: Need to re-enable this.
//	Assert( m_Format == IMAGE_FORMAT_RGBA8888 );

	// We'll be doing our work in IMAGE_FORMAT_RGBA8888 mode 'cause it's easier
	unsigned char *pCubeMaps[6];

	// Allocate the bits for the spheremap
	Assert( m_nDepth == 1 );
	int iMemRequired = ComputeFaceSize( 0, IMAGE_FORMAT_RGBA8888 );
	unsigned char *pSphereMapBits = new unsigned char [ iMemRequired ];

	// Generate a spheremap for each frame of the cubemap
	for (int iFrame = 0; iFrame < m_nFrameCount; ++iFrame)
	{
		// Point to our own textures (highest mip level)
		for (int iFace = 0; iFace < 6; ++iFace)
		{
			pCubeMaps[iFace] = ImageData( iFrame, iFace, 0 );
		}

		// Compute the spheremap of the top LOD
		// HDRFIXME: Make this work?
		if( m_Format == IMAGE_FORMAT_RGBA8888 )
		{
			ComputeSpheremapFrame( pCubeMaps, pSphereMapBits, lookDir );
		}

		// Compute the mip levels of the spheremap, converting from RGBA8888 to our format
		unsigned char *pFinalSphereMapBits = ImageData( iFrame, CUBEMAP_FACE_SPHEREMAP, 0 );
		ImageLoader::GenerateMipmapLevels( pSphereMapBits, pFinalSphereMapBits, 
			m_nWidth, m_nHeight, m_nDepth, m_Format, 2.2, 2.2, m_nMipCount );
	}

	// Free memory
	delete [] pSphereMapBits;
}

void CVTFTexture::GenerateHemisphereMap( unsigned char *pSphereMapBitsRGBA, int targetWidth, 
		int targetHeight, LookDir_t lookDir, int iFrame )
{
	Assert( m_Format == IMAGE_FORMAT_RGBA8888 );

	unsigned char *pCubeMaps[6];

	// Point to our own textures (highest mip level)
	for (int iFace = 0; iFace < 6; ++iFace)
	{
		pCubeMaps[iFace] = ImageData( iFrame, iFace, 0 );
	}

	// Compute the spheremap of the top LOD
	ComputeHemispheremapFrame( pCubeMaps, pSphereMapBitsRGBA, lookDir );
}

//-----------------------------------------------------------------------------
// Rotate the image depending on what iFace we've got...
// We need to do this because we define the cube textures in a different
// format from DX8.
//-----------------------------------------------------------------------------
static void FixCubeMapFacing( unsigned char* pImage, int cubeFaceID, int size, ImageFormat fmt )
{
	int retVal;	
	switch( cubeFaceID )
	{
	case CUBEMAP_FACE_RIGHT:	// +x
		retVal = ImageLoader::RotateImageLeft( pImage, pImage, size, fmt );
		Assert( retVal );
		retVal = ImageLoader::FlipImageVertically( pImage, pImage, size, size, fmt );
		Assert( retVal );
		break;

	case CUBEMAP_FACE_LEFT:		// -x
		retVal = ImageLoader::RotateImageLeft( pImage, pImage, size, fmt );
		Assert( retVal );
		retVal = ImageLoader::FlipImageHorizontally( pImage, pImage, size, size, fmt );
		Assert( retVal );
		break;

	case CUBEMAP_FACE_BACK:		// +y
		retVal = ImageLoader::RotateImage180( pImage, pImage, size, fmt );
		Assert( retVal );
		retVal = ImageLoader::FlipImageHorizontally( pImage, pImage, size, size, fmt );
		Assert( retVal );
		break;

	case CUBEMAP_FACE_FRONT:	// -y
		retVal = ImageLoader::FlipImageHorizontally( pImage, pImage, size, size, fmt );
		Assert( retVal );
		break;

	case CUBEMAP_FACE_UP:		// +z
		retVal = ImageLoader::RotateImageLeft( pImage, pImage, size, fmt );
		Assert( retVal );
		retVal = ImageLoader::FlipImageVertically( pImage, pImage, size, size, fmt );
		Assert( retVal );
		break;

	case CUBEMAP_FACE_DOWN:		// -z
		retVal = ImageLoader::FlipImageHorizontally( pImage, pImage, size, size, fmt );
		Assert( retVal );
		retVal = ImageLoader::RotateImageLeft( pImage, pImage, size, fmt );
		Assert( retVal );
		break;
	}
}

//-----------------------------------------------------------------------------
// Fixes the cubemap faces orientation from our standard to what the material system needs
//-----------------------------------------------------------------------------
void CVTFTexture::FixCubemapFaceOrientation( )
{
	if (!IsCubeMap())
		return;

	Assert( !ImageLoader::IsCompressed( m_Format ) );
	for (int iMipLevel = 0; iMipLevel < m_nMipCount; ++iMipLevel)
	{
		int iMipSize, iTemp, nDepth;
		ComputeMipLevelDimensions( iMipLevel, &iMipSize, &iTemp, &nDepth );
		Assert( (iMipSize == iTemp) && (nDepth == 1) );

		for (int iFrame = 0; iFrame < m_nFrameCount; ++iFrame)
		{
			for (int iFace = 0; iFace < 6; ++iFace)
			{
				FixCubeMapFacing( ImageData( iFrame, iFace, iMipLevel ), iFace, iMipSize, m_Format );
			}
		}
	}
}

void CVTFTexture::NormalizeTopMipLevel()
{
	if( !( m_nFlags & TEXTUREFLAGS_NORMAL ) )
		return;

	int nSrcWidth, nSrcHeight, nSrcDepth;
	int srcMipLevel = 0;
	ComputeMipLevelDimensions( srcMipLevel, &nSrcWidth, &nSrcHeight, &nSrcDepth );
	for (int iFrame = 0; iFrame < m_nFrameCount; ++iFrame)
	{
		for (int iFace = 0; iFace < m_nFaceCount; ++iFace)
		{
			unsigned char *pSrcLevel = ImageData( iFrame, iFace, srcMipLevel );
			ImageLoader::NormalizeNormalMapRGBA8888( pSrcLevel, nSrcWidth * nSrcHeight * nSrcDepth );
		}
	}
}

//-----------------------------------------------------------------------------
// Generates mipmaps from the base mip levels
//-----------------------------------------------------------------------------
void CVTFTexture::GenerateMipmaps()
{
	// Go ahead and generate mipmaps even if we don't want 'em in the vtf.
	//	if( ( Flags() & ( TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD ) ) == ( TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD ) )
//	{
//		return;
//	}

	Assert( m_Format == IMAGE_FORMAT_RGBA8888 || m_Format == IMAGE_FORMAT_RGB323232F );

	// FIXME: Should we be doing anything special for normalmaps other than a final normalization pass?
	ImageLoader::ResampleInfo_t info;
	info.m_nSrcWidth = m_nWidth;
	info.m_nSrcHeight = m_nHeight;
	info.m_nSrcDepth = m_nDepth;
	info.m_flSrcGamma = 2.2f;
	info.m_flDestGamma = 2.2f;
	info.m_nFlags = 0;
	bool bNormalMap    = ( Flags() & TEXTUREFLAGS_NORMAL ) || ( m_Options.flags0 & VtfProcessingOptions::OPT_NORMAL_DUDV );
	bool bAlphaTest    = ( ( m_Options.flags0 & VtfProcessingOptions::OPT_MIP_ALPHATEST ) != 0 );

	if ( bAlphaTest )
	{
		info.m_nFlags |= ImageLoader::RESAMPLE_ALPHATEST;
		if ( m_flAlphaThreshhold >= 0 )
		{
			info.m_flAlphaThreshhold = m_flAlphaThreshhold;
		}
		if ( m_flAlphaHiFreqThreshhold >= 0 )
		{
			info.m_flAlphaHiFreqThreshhold = m_flAlphaHiFreqThreshhold;
		}
	}

	if ( m_Options.flags0 & VtfProcessingOptions::OPT_FILTER_NICE )
	{
		info.m_nFlags |= ImageLoader::RESAMPLE_NICE_FILTER;
	}

	if ( Flags() & TEXTUREFLAGS_CLAMPS )
	{
		info.m_nFlags |= ImageLoader::RESAMPLE_CLAMPS;
	}

	if ( Flags() & TEXTUREFLAGS_CLAMPT )
	{
		info.m_nFlags |= ImageLoader::RESAMPLE_CLAMPT;
	}

	if ( Flags() & TEXTUREFLAGS_CLAMPU )
	{
		info.m_nFlags |= ImageLoader::RESAMPLE_CLAMPU;
	}

	// Compute how many mips are above "visible mip0"
	int numMipsClampedLod = 0;
	if ( TextureLODControlSettings_t const *pLodSettings = ( TextureLODControlSettings_t const * ) GetResourceData( VTF_RSRC_TEXTURE_LOD_SETTINGS, NULL ) )
	{
		int iClampX = 1 << min( pLodSettings->m_ResolutionClampX, pLodSettings->m_ResolutionClampX_360 );
		int iClampY = 1 << min( pLodSettings->m_ResolutionClampX, pLodSettings->m_ResolutionClampX_360 );

		while ( iClampX < m_nWidth || iClampY < m_nHeight )
		{
			++ numMipsClampedLod;
			iClampX <<= 1;
			iClampY <<= 1;
		}
	}

	for ( int iMipLevel = 1; iMipLevel < m_nMipCount; ++iMipLevel )
	{
		ComputeMipLevelDimensions( iMipLevel, &info.m_nDestWidth, &info.m_nDestHeight, &info.m_nDestDepth );

		if ( m_Options.flags0 & VtfProcessingOptions::OPT_PREMULT_COLOR_ONEOVERMIP )
		{
			for ( int ch = 0; ch < 3; ++ ch )
				info.m_flColorScale[ch] = 1.0f / ( float )( 1 << iMipLevel );
		}
		
		// don't use the 0th mip level since NICE filtering blows up!
		int nSrcMipLevel = iMipLevel - 4;
		if ( nSrcMipLevel < 0 )
			nSrcMipLevel = 0;

		// Decay options
		bool bMipBlendActive = false;
		char chChannels[4] = { 'R', 'G', 'B', 'A' };
		for ( int ch = 0; ch < 4; ++ ch )
		{
			int iLastNonDecayMip = numMipsClampedLod + int( m_Options.numNotDecayMips[ch] );
			if ( iLastNonDecayMip > m_nMipCount )
				iLastNonDecayMip = m_nMipCount - 1;
			int numDecayMips = m_nMipCount - iLastNonDecayMip - 1;
			if ( numDecayMips < 1 )
				numDecayMips = 1;

			// Decay is only active starting from numDecayMips
			if ( !( ( ( iMipLevel == m_nMipCount - 1 ) || ( iMipLevel > iLastNonDecayMip ) ) && // last 1x1 mip  or  past clamped and skipped
				    ( m_Options.flags0 & ( VtfProcessingOptions::OPT_DECAY_R << ch ) ) ) )	// the channel has decay
				continue;

			// Color goal
			info.m_flColorGoal[ch] = m_Options.clrDecayGoal[ch];

			// Color scale
			if ( iMipLevel == m_nMipCount - 1 )
			{
				info.m_flColorScale[ch] = 0.0f;
			}
			else if ( m_Options.flags0 & ( VtfProcessingOptions::OPT_DECAY_EXP_R << ch ) )
			{
				info.m_flColorScale[ch] = pow( m_Options.fDecayExponentBase[ch], iMipLevel - iLastNonDecayMip );
			}
			else
			{
				info.m_flColorScale[ch] = 1.0f - float( iMipLevel - iLastNonDecayMip ) / float( numDecayMips );
			}

			if ( !bMipBlendActive )
			{
				bMipBlendActive = true;
				printf( "Blending mip%d %dx%d to", iMipLevel, info.m_nDestWidth, info.m_nDestHeight );
			}

			printf( "   %c=%d ~%d%%", chChannels[ch], m_Options.clrDecayGoal[ch], int( (1.f - info.m_flColorScale[ch]) * 100.0f + 0.5f ) );
		}
		if ( bMipBlendActive )
			printf( "\n" );

		if ( bNormalMap )
		{
			info.m_nFlags |= ImageLoader::RESAMPLE_NORMALMAP;
			// Normal maps xyz decays to 127.f
			for ( int ch = 0; ch < 3; ++ ch )
				info.m_flColorGoal[ch] = 127.0f;
		}

		for ( int iFrame = 0; iFrame < m_nFrameCount; ++iFrame )
		{
			for ( int iFace = 0; iFace < m_nFaceCount; ++iFace )
			{
				unsigned char *pSrcLevel = ImageData( iFrame, iFace, nSrcMipLevel );
				unsigned char *pDstLevel = ImageData( iFrame, iFace, iMipLevel );

				info.m_pSrc = pSrcLevel;
				info.m_pDest = pDstLevel;
				ComputeMipLevelDimensions( nSrcMipLevel, &info.m_nSrcWidth, &info.m_nSrcHeight, &info.m_nSrcDepth );
				if( m_Format == IMAGE_FORMAT_RGB323232F )
				{
					ImageLoader::ResampleRGB323232F( info );
				}
				else
				{
					ImageLoader::ResampleRGBA8888( info );
				}
				if ( Flags() & TEXTUREFLAGS_NORMAL )
				{
					ImageLoader::NormalizeNormalMapRGBA8888( pDstLevel, info.m_nDestWidth * info.m_nDestHeight * info.m_nDestDepth );
				}
			}
		}
	}
}

void CVTFTexture::PutOneOverMipLevelInAlpha()
{
	Assert( m_Format == IMAGE_FORMAT_RGBA8888 );

	for (int iMipLevel = 0; iMipLevel < m_nMipCount; ++iMipLevel)
	{
		int nMipWidth, nMipHeight, nMipDepth;
		ComputeMipLevelDimensions( iMipLevel, &nMipWidth, &nMipHeight, &nMipDepth );
		int size = nMipWidth * nMipHeight * nMipDepth;
		unsigned char ooMipLevel = ( unsigned char )( 255.0f * ( 1.0f / ( float )( 1 << iMipLevel ) ) );

		for (int iFrame = 0; iFrame < m_nFrameCount; ++iFrame)
		{
			for (int iFace = 0; iFace < m_nFaceCount; ++iFace)
			{
				unsigned char *pDstLevel = ImageData( iFrame, iFace, iMipLevel );
				unsigned char *pDst;
				for( pDst = pDstLevel; pDst < pDstLevel + size * 4; pDst += 4 )
				{
					pDst[3] = ooMipLevel;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Computes the reflectivity
//-----------------------------------------------------------------------------
void CVTFTexture::ComputeReflectivity( )
{
	// HDRFIXME: fix this when we ahve a new intermediate format
	if( m_Format != IMAGE_FORMAT_RGBA8888 )
	{
		m_vecReflectivity.Init( 0.2f, 0.2f, 0.2f );
		return;
	}

	Assert( m_Format == IMAGE_FORMAT_RGBA8888 );

	int divisor = 0;
	m_vecReflectivity.Init( 0.0f, 0.0f, 0.0f );
	for( int iFrame = 0; iFrame < m_nFrameCount; ++iFrame )
	{
		for( int iFace = 0; iFace < m_nFaceCount; ++iFace )
		{
			Vector vecFaceReflect;
			unsigned char* pSrc = ImageData( iFrame, iFace, 0 );
			int nNumPixels = m_nWidth * m_nHeight * m_nDepth;

			VectorClear( vecFaceReflect );
			for (int i = 0; i < nNumPixels; ++i, pSrc += 4 )
			{
				vecFaceReflect[0] += TextureToLinear( pSrc[0] );
				vecFaceReflect[1] += TextureToLinear( pSrc[1] );
				vecFaceReflect[2] += TextureToLinear( pSrc[2] );
			}	

			vecFaceReflect /= nNumPixels;

			m_vecReflectivity += vecFaceReflect;
			++divisor;
		}
	}
	m_vecReflectivity /= divisor;
}

//-----------------------------------------------------------------------------
// Computes the alpha flags
//-----------------------------------------------------------------------------
void CVTFTexture::ComputeAlphaFlags()
{
	// HDRFIXME: hack hack hack
	if( m_Format != IMAGE_FORMAT_RGBA8888 )
	{
		m_nFlags &= ~( TEXTUREFLAGS_EIGHTBITALPHA | TEXTUREFLAGS_ONEBITALPHA );
		m_Options.flags0 &= ~( VtfProcessingOptions::OPT_MIP_ALPHATEST );
		return;
	}
	Assert( m_Format == IMAGE_FORMAT_RGBA8888 );

	m_nFlags &= ~(TEXTUREFLAGS_EIGHTBITALPHA | TEXTUREFLAGS_ONEBITALPHA);
	
	if( m_Options.flags0 & VtfProcessingOptions::OPT_SET_ALPHA_ONEOVERMIP )
	{
		m_nFlags |= TEXTUREFLAGS_EIGHTBITALPHA;
		return;
	}
	
	for( int iFrame = 0; iFrame < m_nFrameCount; ++iFrame )
	{
		for( int iFace = 0; iFace < m_nFaceCount; ++iFace )
		{
			for( int iMipLevel = 0; iMipLevel < m_nMipCount; ++iMipLevel )
			{
				// If we're all 0 or all 255, assume it's opaque
				bool bHasZero = false;
				bool bHas255 = false;

				unsigned char* pSrcBits = ImageData( iFrame, iFace, iMipLevel );

				int nMipWidth, nMipHeight, nMipDepth;
				ComputeMipLevelDimensions( iMipLevel, &nMipWidth, &nMipHeight, &nMipDepth );
				int nNumPixels = nMipWidth * nMipHeight * nMipDepth;

				while ( --nNumPixels >= 0 )
				{
					if ( pSrcBits[3] == 0 )
					{
						bHasZero = true;
					}
					else if ( pSrcBits[3] == 255 )
					{
						bHas255 = true;
					}
					else
					{
						// Have grey at all? 8 bit alpha baby
						m_nFlags &= ~TEXTUREFLAGS_ONEBITALPHA;
						m_nFlags |= TEXTUREFLAGS_EIGHTBITALPHA;
						return;
					}

					pSrcBits += 4;
				}

				// If we have both 0 at 255, we're at least one-bit alpha
				if ( bHasZero && bHas255 )
				{
					m_nFlags |= TEXTUREFLAGS_ONEBITALPHA;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Gets the texture all internally consistent assuming you've loaded
// mip 0 of all faces of all frames
//-----------------------------------------------------------------------------
void CVTFTexture::PostProcess( bool bGenerateSpheremap, LookDir_t lookDir, bool bAllowFixCubemapOrientation )
{
	// HDRFIXME: Make sure that all of the below functions check for the proper formats if we get rid of this assert.
//	Assert( m_Format == IMAGE_FORMAT_RGBA8888 );

	// Set up the cube map faces
	if (IsCubeMap())
	{
		// Rotate the cubemaps so they're appropriate for the material system
		if ( bAllowFixCubemapOrientation )
			FixCubemapFaceOrientation();

		// FIXME: We could theoretically not compute spheremap mip levels
		// in generate spheremaps; should we? The trick is when external
		// clients can be expected to call it

		// Compute the spheremap fallback for cubemaps if we weren't able to load up one...
		if (bGenerateSpheremap)
			GenerateSpheremap(lookDir);
	}

	// Normalize the top mip level if necessary.
	NormalizeTopMipLevel();
	
	// Generate mipmap levels
	GenerateMipmaps();

	if( m_Options.flags0 & VtfProcessingOptions::OPT_SET_ALPHA_ONEOVERMIP )
	{
		PutOneOverMipLevelInAlpha();
	}
	
	// Compute reflectivity
	ComputeReflectivity();

	// Are we 8-bit or 1-bit alpha?
	// NOTE: We have to do this *after*	computing the spheremap fallback for
	// cubemaps or it'll throw the flags off
	ComputeAlphaFlags();
}

void CVTFTexture::SetPostProcessingSettings( VtfProcessingOptions const *pOptions )
{
	memset( &m_Options, 0, sizeof( m_Options ) );
	memcpy( &m_Options, pOptions, min( (uint32)sizeof( m_Options ), pOptions->cbSize ) );
	m_Options.cbSize = sizeof( m_Options );

	// Optionally perform the fixups
}

//-----------------------------------------------------------------------------
// Generate the low-res image bits
//-----------------------------------------------------------------------------
bool CVTFTexture::ConstructLowResImage()
{
	// HDRFIXME: hack hack hack
	if( m_Format != IMAGE_FORMAT_RGBA8888 )
	{
		return true;
	}
	Assert( m_Format == IMAGE_FORMAT_RGBA8888 );
	Assert( m_pLowResImageData );

	CUtlMemory<unsigned char> lowResSizeImage;
	lowResSizeImage.EnsureCapacity( m_nLowResImageWidth * m_nLowResImageHeight * 4 );

	ImageLoader::ResampleInfo_t info;
	info.m_pSrc = ImageData(0, 0, 0);
	info.m_pDest = lowResSizeImage.Base();
	info.m_nSrcWidth = m_nWidth;
	info.m_nSrcHeight = m_nHeight;
	info.m_nDestWidth = m_nLowResImageWidth;
	info.m_nDestHeight = m_nLowResImageHeight;
	info.m_flSrcGamma = 2.2f;
	info.m_flDestGamma = 2.2f;
	info.m_nFlags = ImageLoader::RESAMPLE_NICE_FILTER;

	if( !ImageLoader::ResampleRGBA8888( info ) )
		return false;
	
	// convert to the low-res size version with the correct image format
	unsigned char *tmpImage = lowResSizeImage.Base();
	return ImageLoader::ConvertImageFormat( tmpImage, IMAGE_FORMAT_RGBA8888, 
		m_pLowResImageData, m_LowResImageFormat, m_nLowResImageWidth, m_nLowResImageHeight ); 
}
	
// -----------------------------------------------------------------------------
// Cubemap edge-filtering functions.
// -----------------------------------------------------------------------------
void CVTFTexture::SetupFaceVert( int iMipLevel, int iVert, CEdgePos &out )
{
	int nMipWidth, nMipHeight, nMipDepth;
	ComputeMipLevelDimensions( iMipLevel, &nMipWidth, &nMipHeight, &nMipDepth );

	out.x = out.y = 0;
	if ( iVert == 0 || iVert == 3 )
	{
		out.y = nMipHeight - 1;
	}

	if ( iVert == 2 || iVert == 3 )
	{
		out.x = nMipWidth - 1;
	}
}

void CVTFTexture::SetupEdgeIncrement( CEdgePos &start, CEdgePos &end, CEdgePos &inc )
{
	inc.x = inc.y = 0;
	if ( start.x != end.x )
	{
		Assert( start.y == end.y );
		inc.x = (start.x < end.x) ? 1 : -1;
	}
	else if ( start.y != end.y )
	{
		Assert( start.x == end.x );
		inc.y = (start.y < end.y) ? 1 : -1;
	}
	else
	{
		Assert( false );
	}
}

void CVTFTexture::SetupTextureEdgeIncrements( 
	int iMipLevel,
	int iFace1Edge,
	int iFace2Edge,
	bool bFlipFace2Edge,
	CEdgeIncrements *incs )
{
	// Figure out the coordinates of the verts we're blending.
	SetupFaceVert( iMipLevel, iFace1Edge, incs->iFace1Start );
	SetupFaceVert( iMipLevel, (iFace1Edge+1)%4, incs->iFace1End );

	if ( bFlipFace2Edge )
	{
		SetupFaceVert( iMipLevel, (iFace2Edge+1)%4, incs->iFace2Start );
		SetupFaceVert( iMipLevel, iFace2Edge, incs->iFace2End );
	}
	else
	{
		SetupFaceVert( iMipLevel, iFace2Edge, incs->iFace2Start );
		SetupFaceVert( iMipLevel, (iFace2Edge+1)%4, incs->iFace2End );
	}

	// Figure out the increments from start to end.
	SetupEdgeIncrement( incs->iFace1Start, incs->iFace1End, incs->iFace1Inc );
	SetupEdgeIncrement( incs->iFace2Start, incs->iFace2End, incs->iFace2Inc );
}

void BlendTexels( unsigned char **texels, int nTexels )
{
	int sum[4] = { 0, 0, 0, 0 };
	int i;
	for ( i=0; i < nTexels; i++ )
	{
		sum[0] += texels[i][0];
		sum[1] += texels[i][1];
		sum[2] += texels[i][2];
		sum[3] += texels[i][3];
	}
	for ( i=0; i < nTexels; i++ )
	{
		texels[i][0] = (unsigned char)( sum[0] / nTexels );
		texels[i][1] = (unsigned char)( sum[1] / nTexels );
		texels[i][2] = (unsigned char)( sum[2] / nTexels );
		texels[i][3] = (unsigned char)( sum[3] / nTexels );
	}
}

void CVTFTexture::BlendCubeMapFaceEdges(
	int iFrame,
	int iMipLevel,
	const CEdgeMatch *pMatch )
{
	int nMipWidth, nMipHeight, nMipDepth;
	ComputeMipLevelDimensions( iMipLevel, &nMipWidth, &nMipHeight, &nMipDepth );
	Assert( nMipDepth == 1 );
	if ( nMipWidth <= 1 || nMipHeight <= 1 )
		return;

	unsigned char *pFace1Data = ImageData( iFrame, pMatch->m_iFaces[0], iMipLevel );
	unsigned char *pFace2Data = ImageData( iFrame, pMatch->m_iFaces[1], iMipLevel );

	CEdgeIncrements incs;
	SetupTextureEdgeIncrements( iMipLevel, pMatch->m_iEdges[0], pMatch->m_iEdges[1], pMatch->m_bFlipFace2Edge, &incs ); 

	// Do all pixels but the first and the last one (those will be handled when blending corners).
	CEdgePos iFace1Cur = incs.iFace1Start + incs.iFace1Inc;
	CEdgePos iFace2Cur = incs.iFace2Start + incs.iFace2Inc;
	
	if ( m_Format == IMAGE_FORMAT_DXT1 || m_Format == IMAGE_FORMAT_DXT5 )
	{
		if ( iFace1Cur != incs.iFace1End )
		{
			while ( iFace1Cur != incs.iFace1End )
			{
				// Copy the palette index from image 1 to image 2.
				S3PaletteIndex paletteIndex = S3TC_GetPaletteIndex( pFace1Data, m_Format, nMipWidth, iFace1Cur.x, iFace1Cur.y );
				S3TC_SetPaletteIndex( pFace2Data, m_Format, nMipWidth, iFace2Cur.x, iFace2Cur.y, paletteIndex );
				
				iFace1Cur += incs.iFace1Inc;
				iFace2Cur += incs.iFace2Inc;
			}
		}
	}
	else if ( m_Format == IMAGE_FORMAT_RGBA8888 )
	{
		if ( iFace1Cur != incs.iFace1End )
		{
			while ( iFace1Cur != incs.iFace1End )
			{
				// Now we know the 2 pixels. Average them and copy the averaged value to both pixels.
				unsigned char *texels[2] = 
				{
					pFace1Data + ((iFace1Cur.y * nMipWidth) + iFace1Cur.x) * 4,
					pFace2Data + ((iFace2Cur.y * nMipWidth) + iFace2Cur.x) * 4
				};

				BlendTexels( texels, 2 );
				
				iFace1Cur += incs.iFace1Inc;
				iFace2Cur += incs.iFace2Inc;
			}
		}
	}
	else
	{
		Error( "BlendCubeMapFaceEdges: unsupported image format (%d)", (int)m_Format );
	}
}

void CVTFTexture::BlendCubeMapFaceCorners(
	int iFrame,
	int iMipLevel,
	const CCornerMatch *pMatch )
{
	int nMipWidth, nMipHeight, nMipDepth;
	ComputeMipLevelDimensions( iMipLevel, &nMipWidth, &nMipHeight, &nMipDepth );
	Assert( nMipDepth == 1 );

	// Setup the coordinates of each texel.
	CEdgePos texelPos[3];
	unsigned char *pImageData[3];
	int iEdge;
	for ( iEdge=0; iEdge < 3; iEdge++ )
	{
		SetupFaceVert( iMipLevel, pMatch->m_iFaceEdges[iEdge], texelPos[iEdge] );
		pImageData[iEdge] = ImageData( iFrame, pMatch->m_iFaces[iEdge], iMipLevel );
	}

	if ( m_Format == IMAGE_FORMAT_DXT1 || m_Format == IMAGE_FORMAT_DXT5 )
	{
		if ( nMipWidth < 4 || nMipHeight < 4 )
			return;
  
		// Copy the first palette index to the other blocks.
		S3PaletteIndex paletteIndex = S3TC_GetPaletteIndex( pImageData[0], m_Format, nMipWidth, texelPos[0].x, texelPos[0].y );
		S3TC_SetPaletteIndex( pImageData[1], m_Format, nMipWidth, texelPos[1].x, texelPos[1].y, paletteIndex );
		S3TC_SetPaletteIndex( pImageData[2], m_Format, nMipWidth, texelPos[2].x, texelPos[2].y, paletteIndex );
	}
	else if ( m_Format == IMAGE_FORMAT_RGBA8888 )
	{
		// Setup pointers to the 3 corner texels.
		unsigned char *texels[3];
		for ( iEdge=0; iEdge < 3; iEdge++ )
		{
			CEdgePos facePos;
			SetupFaceVert( iMipLevel, pMatch->m_iFaceEdges[iEdge], facePos );
		
			texels[iEdge] = pImageData[iEdge];
			texels[iEdge] += (facePos.y * nMipWidth + facePos.x) * 4;
		}

		// Now blend the texels.
		BlendTexels( texels, 3 );
	}
	else
	{
		Assert( false );
	}
}

void CVTFTexture::BuildCubeMapMatchLists( 
	CEdgeMatch edgeMatches[NUM_EDGE_MATCHES], 
	CCornerMatch cornerMatches[NUM_CORNER_MATCHES],
	bool bSkybox )
{

	int **faceVertsList = bSkybox ? g_skybox_FaceVerts : g_FaceVerts;

	// For each face, look for matching edges on other faces.
	int nTotalEdgesMatched = 0;
	for ( int iFace = 0; iFace < 6; iFace++ )
	{
		for ( int iEdge=0; iEdge < 4; iEdge++ )
		{
			int i1 = faceVertsList[iFace][iEdge];
			int i2 = faceVertsList[iFace][(iEdge+1)%4];

			// Only look for faces with indices < what we have so we don't do each edge twice.
			for ( int iOtherFace=0; iOtherFace < iFace; iOtherFace++ )
			{
				for ( int iOtherEdge=0; iOtherEdge < 4; iOtherEdge++ )
				{
					int o1 = faceVertsList[iOtherFace][iOtherEdge];
					int o2 = faceVertsList[iOtherFace][(iOtherEdge+1)%4];
				
					if ( (i1 == o1 && i2 == o2) || (i2 == o1 && i1 == o2) )
					{
						CEdgeMatch *pMatch = &edgeMatches[nTotalEdgesMatched];

						pMatch->m_iFaces[0] = iFace;
						pMatch->m_iEdges[0] = iEdge;

						pMatch->m_iFaces[1] = iOtherFace;
						pMatch->m_iEdges[1] = iOtherEdge;

						pMatch->m_iCubeVerts[0] = o1;
						pMatch->m_iCubeVerts[1] = o2;

						pMatch->m_bFlipFace2Edge = i1 != o1;

						++nTotalEdgesMatched;
					}
				}
			}
		}
	}

	Assert( nTotalEdgesMatched == 12 );

	// For each corner vert, find the 3 edges touching it.
	for ( int iVert=0; iVert < NUM_CORNER_MATCHES; iVert++ )
	{
		int iTouchingFace = 0;

		for ( int iFace=0; iFace < 6; iFace++ )
		{
			for ( int iFaceVert=0; iFaceVert < 4; iFaceVert++ )
			{
				if ( faceVertsList[iFace][iFaceVert] == iVert )
				{
					cornerMatches[iVert].m_iFaces[iTouchingFace] = iFace;
					cornerMatches[iVert].m_iFaceEdges[iTouchingFace] = iFaceVert;
					++iTouchingFace;
				}
			}
		}
		Assert( iTouchingFace == 3 );
	}
}

void CVTFTexture::BlendCubeMapEdgePalettes(
	int iFrame,
	int iMipLevel,
	const CEdgeMatch *pMatch )
{
	Assert( m_Format == IMAGE_FORMAT_DXT1 || m_Format == IMAGE_FORMAT_DXT5 );

	int nMipWidth, nMipHeight, nMipDepth;
	ComputeMipLevelDimensions( iMipLevel, &nMipWidth, &nMipHeight, &nMipDepth );
	Assert( nMipDepth == 1 );
	if ( nMipWidth <= 8 || nMipHeight <= 8 )
		return;

	unsigned char *pFace1Data = ImageData( iFrame, pMatch->m_iFaces[0], iMipLevel );
	unsigned char *pFace2Data = ImageData( iFrame, pMatch->m_iFaces[1], iMipLevel );
	S3RGBA *pFace1Original = &m_OriginalData[ GetImageOffset( iFrame, pMatch->m_iFaces[0], iMipLevel, IMAGE_FORMAT_RGBA8888 ) / 4 ];
	S3RGBA *pFace2Original = &m_OriginalData[ GetImageOffset( iFrame, pMatch->m_iFaces[1], iMipLevel, IMAGE_FORMAT_RGBA8888 ) / 4 ];

	CEdgeIncrements incs;
	SetupTextureEdgeIncrements( iMipLevel, pMatch->m_iEdges[0], pMatch->m_iEdges[1], pMatch->m_bFlipFace2Edge, &incs ); 
	
	// Divide the coordinates by 4 since we're dealing with S3 blocks here.
	incs.iFace1Start /= 4; incs.iFace1End /= 4; incs.iFace2Start /= 4; incs.iFace2End /= 4;
	
	// Now walk along the edges, blending the edge pixels.
	CEdgePos iFace1Cur = incs.iFace1Start + incs.iFace1Inc;
	CEdgePos iFace2Cur = incs.iFace2Start + incs.iFace2Inc;
	while ( iFace1Cur != incs.iFace1End ) // We intentionally want to not process the last block here..
	{
		// Merge the palette of these two blocks.
		char *blocks[2] = 
		{
			S3TC_GetBlock( pFace1Data, m_Format, nMipWidth>>2, iFace1Cur.x, iFace1Cur.y ),
			S3TC_GetBlock( pFace2Data, m_Format, nMipWidth>>2, iFace2Cur.x, iFace2Cur.y )
		};

		S3RGBA *originals[2] =
		{
			&pFace1Original[(iFace1Cur.y * 4 * nMipWidth) + iFace1Cur.x * 4],
			&pFace2Original[(iFace2Cur.y * 4 * nMipWidth) + iFace2Cur.x * 4]
		};

		S3TC_MergeBlocks( 
			blocks, 
			originals,
			2,
			nMipWidth*4,
			m_Format );
		
		iFace1Cur += incs.iFace1Inc;
		iFace2Cur += incs.iFace2Inc;
	}
}

void CVTFTexture::BlendCubeMapCornerPalettes(
	int iFrame,
	int iMipLevel,
	const CCornerMatch *pMatch )
{
	int nMipWidth, nMipHeight, nMipDepth;
	ComputeMipLevelDimensions( iMipLevel, &nMipWidth, &nMipHeight, &nMipDepth );
	Assert( nMipDepth == 1 );
	if ( nMipWidth < 4 || nMipHeight < 4 )
		return;

	// Now setup an S3TC block pointer for each of the corner blocks on each face.
	char *blocks[3];
	S3RGBA *originals[3];

	for ( int iEdge=0; iEdge < 3; iEdge++ )
	{
		CEdgePos facePos;
		SetupFaceVert( iMipLevel, pMatch->m_iFaceEdges[iEdge], facePos );
		facePos /= 4; // To get the S3 block index.

		int iFaceIndex = pMatch->m_iFaces[iEdge];
		unsigned char *pFaceData = ImageData( iFrame, iFaceIndex, iMipLevel );
		S3RGBA *pFaceOriginal = &m_OriginalData[ GetImageOffset( iFrame, iFaceIndex, iMipLevel, IMAGE_FORMAT_RGBA8888 ) / 4 ];

		blocks[iEdge] = S3TC_GetBlock( pFaceData, m_Format, nMipWidth>>2, facePos.x, facePos.y );
		originals[iEdge] = &pFaceOriginal[ (facePos.y * 4 * nMipWidth) + facePos.x * 4 ];
	}

	S3TC_MergeBlocks( 
		blocks,
		originals,
		3,
		nMipWidth*4,
		m_Format );
}

void CVTFTexture::MatchCubeMapS3TCPalettes(
	CEdgeMatch edgeMatches[NUM_EDGE_MATCHES],
	CCornerMatch cornerMatches[NUM_CORNER_MATCHES]
	)
{
	for (int iMipLevel = 0; iMipLevel < m_nMipCount; ++iMipLevel)
	{
		for (int iFrame = 0; iFrame < m_nFrameCount; ++iFrame)
		{
			// First, match all the edge palettes (this part skips the first and last 4 texels
			// along the edge since those S3 blocks are handled in the corner case).
			for ( int iEdgeMatch=0; iEdgeMatch < NUM_EDGE_MATCHES; iEdgeMatch++ )
			{
				BlendCubeMapEdgePalettes( 
					iFrame,
					iMipLevel,
					&edgeMatches[iEdgeMatch] );
			}

			for ( int iCornerMatch=0; iCornerMatch < NUM_CORNER_MATCHES; iCornerMatch++ )
			{
				BlendCubeMapCornerPalettes(
					iFrame,
					iMipLevel,
					&cornerMatches[iCornerMatch] );
			}
		}
	}	
}

void CVTFTexture::MatchCubeMapBorders( int iStage, ImageFormat finalFormat, bool bSkybox )
{
	// HDRFIXME: hack hack hack
	if( m_Format != IMAGE_FORMAT_RGBA8888 )
	{
		return;
	}
	if ( !IsCubeMap() )
		return;
	
	Assert( IsCubeMap() );
	Assert( m_nFaceCount >= 6 );

	if ( iStage == 1 )
	{
		// Stage 1 is while the image is still RGBA8888. If we're not going to S3 compress the image,
		// then it is easiest to match the borders now.
		Assert( m_Format == IMAGE_FORMAT_RGBA8888 );
		if ( finalFormat == IMAGE_FORMAT_DXT1 || finalFormat == IMAGE_FORMAT_DXT5 )
		{
			// If we're going to S3 compress the image eventually, then store off the original version
			// because we can use that while matching the S3 compressed edges (we have to do some tricky
			// repalettizing).
			int nTotalBytes = ComputeTotalSize();
			m_OriginalData.SetSize( nTotalBytes / 4 );
			memcpy( m_OriginalData.Base(), ImageData(), nTotalBytes );
			
			// Swap R and B in these because IMAGE_FORMAT_RGBA8888 is swapped from the way S3RGBAs are.
			for ( int i=0; i < nTotalBytes/4; i++ )
				V_swap( m_OriginalData[i].r, m_OriginalData[i].b );
		
			return;
		}
		else
		{
			// Drop down below and do the edge matching.
		}
	}
	else
	{
		if ( finalFormat == IMAGE_FORMAT_DXT1 || finalFormat == IMAGE_FORMAT_DXT5 )
		{
			Assert( m_Format == finalFormat );
		}
		else
		{	
			// If we're not winding up S3 compressed, then we already fixed the cubemap borders.
			return;
		}
	}

	// Figure out 
	CEdgeMatch edgeMatches[NUM_EDGE_MATCHES];
	CCornerMatch cornerMatches[NUM_CORNER_MATCHES];

	BuildCubeMapMatchLists( edgeMatches, cornerMatches, bSkybox );

	// If we're S3 compressed, then during the first pass, we need to match the palettes of all
	// bordering S3 blocks.
	if ( m_Format == IMAGE_FORMAT_DXT1 || m_Format == IMAGE_FORMAT_DXT5 )
	{
		MatchCubeMapS3TCPalettes( edgeMatches, cornerMatches );
	}

	for (int iMipLevel = 0; iMipLevel < m_nMipCount; ++iMipLevel)
	{
		for (int iFrame = 0; iFrame < m_nFrameCount; ++iFrame)
		{
			for ( int iEdgeMatch=0; iEdgeMatch < NUM_EDGE_MATCHES; iEdgeMatch++ )
			{
				BlendCubeMapFaceEdges( 
					iFrame,
					iMipLevel,
					&edgeMatches[iEdgeMatch] );
			}

			for ( int iCornerMatch=0; iCornerMatch < NUM_CORNER_MATCHES; iCornerMatch++ )
			{
				BlendCubeMapFaceCorners(
					iFrame,
					iMipLevel,
					&cornerMatches[iCornerMatch] );
			}
		}
	}
}


/* 

Test code used to draw the cubemap into a scratchpad file. Useful for debugging, or at least 
it was once.

	IScratchPad3D *pPad = ScratchPad3D_Create();

	int nMipWidth, nMipHeight;
	ComputeMipLevelDimensions( 0, &nMipWidth, &nMipHeight );

	CUtlVector<unsigned char> data;
	data.SetSize( nMipWidth*nMipHeight );

	float cubeSize = 200;
	Vector vertPositions[8] = 
	{
		Vector( 0, cubeSize, 0 ),
		Vector( 0, cubeSize, cubeSize ),
		Vector( cubeSize, 0, 0 ),
		Vector( cubeSize, 0, cubeSize ),

		Vector( 0, 0, 0 ),
		Vector( 0, 0, cubeSize ),
		Vector( cubeSize, cubeSize, 0 ),
		Vector( cubeSize, cubeSize, cubeSize )
	};
	char *faceNames[6] = { "right","left","back","front","up","down" };

	for ( int iVert=0; iVert < 8; iVert++ )
	{
		char str[512];
		Q_snprintf( str, sizeof( str ), "%d", iVert );
		CTextParams params;
		params.m_flLetterWidth = 20;
		params.m_vPos = vertPositions[iVert];
		pPad->DrawText( str, params );
	}

	for ( int iFace=0; iFace < 6; iFace++ )
	{
		unsigned char *pFace1Data = ImageData( 0, iFace, 0 );
		for ( int y=0; y < nMipHeight; y++ )
		{
			for( int x=0; x < nMipWidth; x++ )
			{
				S3PaletteIndex index = S3TC_GetPaletteIndex(
					pFace1Data,
					m_Format,
					nMipWidth,
					x, y );

				const char *pBlock = S3TC_GetBlock( pFace1Data, m_Format, nMipWidth/4, x/4, y/4 );
				unsigned char a0 = pBlock[0];
				unsigned char a1 = pBlock[1];
				
				if ( index.m_AlphaIndex == 0 )
				{
					data[y*nMipWidth+x] = a0;
				}
				else if ( index.m_AlphaIndex == 1 )
				{
					data[y*nMipWidth+x] = a1;
				}
				else if ( a0 > a1 )
				{
					data[y*nMipWidth+x] = ((8-(int)index.m_AlphaIndex)*a0 + ((int)index.m_AlphaIndex-1)*a1) / 7;
				}
				else
				{
					if ( index.m_AlphaIndex == 6 )
						data[y*nMipWidth+x] = 0;
					else if ( index.m_AlphaIndex == 7 )
						data[y*nMipWidth+x] = 255;
					else
						data[y*nMipWidth+x] = ((6-(int)index.m_AlphaIndex)*a0 + ((int)index.m_AlphaIndex-1)*a1) / 5;
				}
			}
		}

		Vector vCorners[4];
		for ( int iCorner=0; iCorner < 4; iCorner++ )
			vCorners[iCorner] = vertPositions[g_FaceVerts[iFace][iCorner]];

		pPad->DrawImageBW( data.Base(), nMipWidth, nMipHeight, nMipWidth, false, true, vCorners );
		
		CTextParams params;
		params.m_vPos = (vCorners[0] + vCorners[1] + vCorners[2] + vCorners[3]) / 4;
		params.m_bCentered = true;
		params.m_vColor.Init( 1, 0, 0 );
		params.m_bTwoSided = true;
		params.m_flLetterWidth = 10;
		
		Vector vNormal = (vCorners[1] - vCorners[0]).Cross( vCorners[2] - vCorners[1] );
		VectorNormalize( vNormal );
		params.m_vPos += vNormal*5;
		VectorAngles( vNormal, params.m_vAngles );

		pPad->DrawText( faceNames[iFace], params );
		
		pPad->Flush();
	}
*/

