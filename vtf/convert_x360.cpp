//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//	Purpose: Force pc .VTF to preferred .VTF 360 format conversion
//	
//=====================================================================================//

#include "tier1/utlvector.h"
#include "mathlib/mathlib.h"
#include "tier1/strtools.h"
#include "cvtf.h"
#include "tier1/utlbuffer.h"
#include "tier0/dbg.h"
#include "tier1/utlmemory.h"
#include "bitmap/imageformat.h"

// if the entire vtf file is smaller than this threshold, add entirely to preload
#define PRELOAD_VTF_THRESHOLD			2048

struct ResourceCopy_t
{
	void				*m_pData;
	int					m_DataLength;
	ResourceEntryInfo	m_EntryInfo;
};

//-----------------------------------------------------------------------------
// Converts to an alternate format
//-----------------------------------------------------------------------------
ImageFormat PreferredFormat( IVTFTexture *pVTFTexture, ImageFormat fmt, int width, int height, int mipCount, int faceCount )
{
	switch ( fmt )
	{
		case IMAGE_FORMAT_RGBA8888: 
		case IMAGE_FORMAT_ABGR8888:
		case IMAGE_FORMAT_ARGB8888:
		case IMAGE_FORMAT_BGRA8888:
			return IMAGE_FORMAT_BGRA8888;

		// 24bpp gpu formats don't exist, must convert
		case IMAGE_FORMAT_BGRX8888:
		case IMAGE_FORMAT_RGB888:
		case IMAGE_FORMAT_BGR888:
		case IMAGE_FORMAT_RGB888_BLUESCREEN:
		case IMAGE_FORMAT_BGR888_BLUESCREEN:
			return IMAGE_FORMAT_BGRX8888;

		case IMAGE_FORMAT_BGRX5551:
		case IMAGE_FORMAT_RGB565:
		case IMAGE_FORMAT_BGR565:
			return IMAGE_FORMAT_BGR565;

		// no change
		case IMAGE_FORMAT_I8:
		case IMAGE_FORMAT_IA88:
		case IMAGE_FORMAT_A8:
		case IMAGE_FORMAT_BGRA4444:
		case IMAGE_FORMAT_BGRA5551:
		case IMAGE_FORMAT_UV88:
		case IMAGE_FORMAT_UVWQ8888:
		case IMAGE_FORMAT_RGBA16161616:
		case IMAGE_FORMAT_UVLX8888:
		case IMAGE_FORMAT_DXT1_ONEBITALPHA:
		case IMAGE_FORMAT_DXT1:
		case IMAGE_FORMAT_DXT3:
		case IMAGE_FORMAT_DXT5:
		case IMAGE_FORMAT_ATI1N:
		case IMAGE_FORMAT_ATI2N:
			break;

		case IMAGE_FORMAT_RGBA16161616F:
			return IMAGE_FORMAT_RGBA16161616;
	}

	return fmt;
}

//-----------------------------------------------------------------------------
// Determines target dimensions
//-----------------------------------------------------------------------------
bool ComputeTargetDimensions( const char *pDebugName, IVTFTexture *pVTFTexture, int picmip, int &width, int &height, int &mipCount, int &mipSkipCount, bool &bNoMip )
{
	width  = pVTFTexture->Width();
	height = pVTFTexture->Height();

	// adhere to texture's internal lod setting
	int nClampX = 1<<30;
	int nClampY = 1<<30;
	TextureLODControlSettings_t const *pLODInfo = reinterpret_cast<TextureLODControlSettings_t const *> ( pVTFTexture->GetResourceData( VTF_RSRC_TEXTURE_LOD_SETTINGS, NULL ) );
	if ( pLODInfo )
	{
		if ( pLODInfo->m_ResolutionClampX )
		{
			nClampX = min( nClampX, 1 << pLODInfo->m_ResolutionClampX );
		}
		if ( pLODInfo->m_ResolutionClampX_360 )
		{
			nClampX = min( nClampX, 1 << pLODInfo->m_ResolutionClampX_360 );
		}
		if ( pLODInfo->m_ResolutionClampY )
		{
			nClampY = min( nClampY, 1 << pLODInfo->m_ResolutionClampY );
		}
		if ( pLODInfo->m_ResolutionClampY_360 )
		{
			nClampY = min( nClampY, 1 << pLODInfo->m_ResolutionClampY_360 );
		}
	}

	// spin down to desired texture size
	mipSkipCount = 0;
	while ( mipSkipCount < picmip || width > nClampX || height > nClampY )
	{
		if ( width == 1 && height == 1 )
			break;
		width >>= 1;
		height >>= 1;
		if ( width < 1 )
			width = 1;
		if ( height < 1 )
			height = 1;
		mipSkipCount++;
	}

	bNoMip = false;
	if ( pVTFTexture->Flags() & TEXTUREFLAGS_NOMIP )
	{
		bNoMip = true;
	}

	// determine mip quantity based on desired width/height
	if ( bNoMip )
	{
		// avoid serializing unused mips
		mipCount = 1;
	}
	else
	{
		mipCount = ImageLoader::GetNumMipMapLevels( width, height );
	}

	// success
	return true;
}

//-----------------------------------------------------------------------------
// Align the buffer to specified boundary
//-----------------------------------------------------------------------------
int AlignBuffer( CUtlBuffer &buf, int alignment )
{
	int		curPosition;
	int		newPosition;
	byte	padByte = 0;

	// advance to aligned position
	buf.SeekPut( CUtlBuffer::SEEK_TAIL, 0 );
	curPosition = buf.TellPut();
	newPosition = AlignValue( curPosition, alignment );
	buf.EnsureCapacity( newPosition );

	// write empty
	for ( int i=0; i<newPosition-curPosition; i++ )
	{
		buf.Put( &padByte, 1 );
	}

	return newPosition;
}

//-----------------------------------------------------------------------------
// Convert the x86 image data to 360
//-----------------------------------------------------------------------------
bool ConvertImageFormatEx( 
	unsigned char	*pSourceImage,
	int				sourceImageSize, 
	ImageFormat		sourceFormat,
	unsigned char	*pTargetImage,
	int				targetImageSize, 
	ImageFormat		targetFormat,
	int				width, 
	int				height,
	bool			bSrgbGammaConvert )
{
	// format conversion expects pc oriented data
	// but, formats that are >8 bits per channels need to be element pre-swapped
	ImageLoader::PreConvertSwapImageData( pSourceImage, sourceImageSize, sourceFormat );

	bool bRetVal = ImageLoader::ConvertImageFormat( 
				pSourceImage, 
				sourceFormat, 
				pTargetImage, 
				targetFormat, 
				width, 
				height );
	if ( !bRetVal )
	{
		return false;
	}

	// convert to proper channel order for 360 d3dformats
	ImageLoader::PostConvertSwapImageData( pTargetImage, targetImageSize, targetFormat ); 

	// Convert colors from sRGB gamma space into 360 piecewise linear gamma space
	if ( bSrgbGammaConvert == true )
	{
		if ( targetFormat == IMAGE_FORMAT_BGRA8888 || targetFormat == IMAGE_FORMAT_BGRX8888 )
		{
			//Msg( "   Converting 8888 texture from sRGB gamma to 360 PWL gamma *** %dx%d\n", width, height );
			for ( int i = 0; i < ( targetImageSize / 4 ); i++ ) // targetImageSize is the raw data length in bytes
			{
				unsigned char *pRGB[3] = { NULL, NULL, NULL };

				if ( IsPC() )
				{
					// pTargetImage is the raw image data
					pRGB[0] = &( pTargetImage[ ( i * 4 ) + 1 ] ); // Red
					pRGB[1] = &( pTargetImage[ ( i * 4 ) + 2 ] ); // Green
					pRGB[2] = &( pTargetImage[ ( i * 4 ) + 3 ] ); // Blue
				}
				else // 360
				{
					// pTargetImage is the raw image data
					pRGB[0] = &( pTargetImage[ ( i * 4 ) + 1 ] ); // Red
					pRGB[1] = &( pTargetImage[ ( i * 4 ) + 2 ] ); // Green
					pRGB[2] = &( pTargetImage[ ( i * 4 ) + 3 ] ); // Blue
				}
				
				// Modify RGB data in place
				for ( int j = 0; j < 3; j++ ) // For red, green, blue
				{
					float flSrgbGamma = float( *( pRGB[j] ) ) / 255.0f;
					float fl360Gamma = SrgbGammaTo360Gamma( flSrgbGamma );

					fl360Gamma = clamp( fl360Gamma, 0.0f, 1.0f );
					*( pRGB[j] ) = ( unsigned char ) ( clamp( ( ( fl360Gamma * 255.0f ) + 0.5f ), 0.0f, 255.0f ) );
				}
			}
		}
		else if ( ( targetFormat == IMAGE_FORMAT_DXT1_ONEBITALPHA ) || ( targetFormat == IMAGE_FORMAT_DXT1 ) || ( targetFormat == IMAGE_FORMAT_DXT3 ) || ( targetFormat == IMAGE_FORMAT_DXT5 ) )
		{
			//Msg( "   Converting DXT texture from sRGB gamma to 360 PWL gamma *** %dx%d\n", width, height );
			int nStrideBytes = 8;
			int nOffsetBytes = 0;
			if ( ( targetFormat == IMAGE_FORMAT_DXT3 ) || ( targetFormat == IMAGE_FORMAT_DXT5 ) )
			{
				nOffsetBytes = 8;
				nStrideBytes = 16;
			}

			for ( int i = 0; i < ( targetImageSize / nStrideBytes ); i++ ) // For each color or color/alpha block
			{
				// Get 16bit 565 colors into an unsigned short
				unsigned short n565Color0 = 0;
				n565Color0 |= ( ( unsigned short )( unsigned char )( pTargetImage[ ( i * nStrideBytes ) + nOffsetBytes + 0 ] ) ) << 8;
				n565Color0 |= ( ( unsigned short )( unsigned char )( pTargetImage[ ( i * nStrideBytes ) + nOffsetBytes + 1 ] ) );

				unsigned short n565Color1 = 0;
				n565Color1 |= ( ( unsigned short )( unsigned char )( pTargetImage[ ( i * nStrideBytes ) + nOffsetBytes + 2 ] ) ) << 8;
				n565Color1 |= ( ( unsigned short )( unsigned char )( pTargetImage[ ( i * nStrideBytes ) + nOffsetBytes + 3 ] ) );

				// Convert to 888
				unsigned char v888Color0[3];
				v888Color0[0] = ( ( ( n565Color0 >> 11 ) & 0x1f ) << 3 );
				v888Color0[1] = ( ( ( n565Color0 >> 5 ) & 0x3f ) << 2 );
				v888Color0[2] = ( ( n565Color0 & 0x1f ) << 3 );

				// Since we have one bit less of red and blue, add some of the error back in
				if ( v888Color0[0] != 0 ) // Don't mess with black pixels
					v888Color0[0] |= 0x04; // Add 0.5 of the error
				if ( v888Color0[2] != 0 ) // Don't mess with black pixels
					v888Color0[2] |= 0x04; // Add 0.5 of the error

				unsigned char v888Color1[3];
				v888Color1[0] = ( ( ( n565Color1 >> 11 ) & 0x1f ) << 3 );
				v888Color1[1] = ( ( ( n565Color1 >> 5 ) & 0x3f ) << 2 );
				v888Color1[2] = ( ( n565Color1 & 0x1f ) << 3 );

				// Since we have one bit less of red and blue, add some of the error back in
				if ( v888Color1[0] != 0 ) // Don't mess with black pixels
					v888Color1[0] |= 0x04; // Add 0.5 of the error
				if ( v888Color1[2] != 0 ) // Don't mess with black pixels
					v888Color1[2] |= 0x04; // Add 0.5 of the error

				// Convert to float
				float vFlColor0[3];
				vFlColor0[0] = float( v888Color0[0] ) / 255.0f;
				vFlColor0[1] = float( v888Color0[1] ) / 255.0f;
				vFlColor0[2] = float( v888Color0[2] ) / 255.0f;

				float vFlColor1[3];
				vFlColor1[0] = float( v888Color1[0] ) / 255.0f;
				vFlColor1[1] = float( v888Color1[1] ) / 255.0f;
				vFlColor1[2] = float( v888Color1[2] ) / 255.0f;

				// Modify float RGB data and write to output 888 colors
				unsigned char v888Color0New[3];
				unsigned char v888Color1New[3];
				for ( int j = 0; j < 3; j++ ) // For red, green, blue
				{
					for ( int k = 0; k < 2; k++ ) // For color0 and color1
					{
						float *pFlValue = ( k == 0 ) ? &( vFlColor0[j] ) : &( vFlColor1[j] );
						unsigned char *p8BitValue = ( k == 0 ) ? &( v888Color0New[j] ) : &( v888Color1New[j] );

						float flSrgbGamma = *pFlValue;
						float fl360Gamma = SrgbGammaTo360Gamma( flSrgbGamma );

						fl360Gamma = clamp( fl360Gamma, 0.0f, 1.0f );
						//*p8BitValue = ( unsigned char ) ( clamp( ( ( fl360Gamma * 255.0f ) + 0.5f ), 0.0f, 255.0f ) );
						*p8BitValue = ( unsigned char ) ( clamp( ( ( fl360Gamma * 255.0f ) ), 0.0f, 255.0f ) );
					}
				}

				// Convert back to 565
				v888Color0New[0] &= 0xf8; // 5 bits
				v888Color0New[1] &= 0xfc; // 6 bits
				v888Color0New[2] &= 0xf8; // 5 bits
				unsigned short n565Color0New = ( ( unsigned short )v888Color0New[0] << 8 ) | ( ( unsigned short )v888Color0New[1] << 3 ) | ( ( unsigned short )v888Color0New[2] >> 3 );

				v888Color1New[0] &= 0xf8; // 5 bits
				v888Color1New[1] &= 0xfc; // 6 bits
				v888Color1New[2] &= 0xf8; // 5 bits
				unsigned short n565Color1New = ( ( unsigned short )v888Color1New[0] << 8 ) | ( ( unsigned short )v888Color1New[1] << 3 ) | ( ( unsigned short )v888Color1New[2] >> 3 );

				// If we're targeting DXT1, make sure we haven't made a non transparent color block transparent
				if ( ( targetFormat == IMAGE_FORMAT_DXT1 ) || ( targetFormat == IMAGE_FORMAT_DXT1_ONEBITALPHA ) )
				{
					// If new block is transparent but old block wasn't
					if ( ( n565Color0New <= n565Color1New ) && ( n565Color0 > n565Color1 ) )
					{
						if ( ( v888Color0New[0] == v888Color1New[0] ) && ( v888Color0[0] != v888Color1[0] ) )
						{
							if ( v888Color0New[0] == 0xf8 )
								v888Color1New[0] -= 0x08;
							else
								v888Color0New[0] += 0x08;
						}

						if ( ( v888Color0New[1] == v888Color1New[1] ) && ( v888Color0[1] != v888Color1[1] ) )
						{
							if ( v888Color0New[1] == 0xfc )
								v888Color1New[1] -= 0x04;
							else
								v888Color0New[1] += 0x04;
						}

						if ( ( v888Color0New[2] == v888Color1New[2] ) && ( v888Color0[2] != v888Color1[2] ) )
						{
							if ( v888Color0New[2] == 0xf8 )
								v888Color1New[2] -= 0x08;
							else
								v888Color0New[2] += 0x08;
						}

						n565Color0New = ( ( unsigned short )v888Color0New[0] << 8 ) | ( ( unsigned short )v888Color0New[1] << 3 ) | ( ( unsigned short )v888Color0New[2] >> 3 );
						n565Color1New = ( ( unsigned short )v888Color1New[0] << 8 ) | ( ( unsigned short )v888Color1New[1] << 3 ) | ( ( unsigned short )v888Color1New[2] >> 3 );
					}
				}

				// Copy new colors back to color block
				pTargetImage[ ( i * nStrideBytes ) + nOffsetBytes + 0 ] = ( unsigned char )( ( n565Color0New >> 8 ) & 0x00ff );
				pTargetImage[ ( i * nStrideBytes ) + nOffsetBytes + 1 ] = ( unsigned char )( n565Color0New & 0x00ff );

				pTargetImage[ ( i * nStrideBytes ) + nOffsetBytes + 2 ] = ( unsigned char )( ( n565Color1New >> 8 ) & 0x00ff );
				pTargetImage[ ( i * nStrideBytes ) + nOffsetBytes + 3 ] = ( unsigned char )( n565Color1New & 0x00ff );
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Write the source data as the desired format into a target buffer
//-----------------------------------------------------------------------------
bool SerializeImageData( IVTFTexture *pSourceVTF, int frame, int face, int mip, ImageFormat targetFormat, CUtlBuffer &targetBuf )
{
	int					width;
	int					height;
	int					targetImageSize;
	byte				*pSourceImage;
	int					sourceImageSize;
	int					targetSize;
	CUtlMemory<byte>	targetImage;

	width = pSourceVTF->Width() >> mip;
	height = pSourceVTF->Height() >> mip;
	if ( width < 1 )
		width = 1;
	if ( height < 1)
		height = 1;

	sourceImageSize = ImageLoader::GetMemRequired( width, height, 1, pSourceVTF->Format(), false );
	pSourceImage = pSourceVTF->ImageData( frame, face, mip );

	targetImageSize = ImageLoader::GetMemRequired( width, height, 1, targetFormat, false );
	targetImage.EnsureCapacity( targetImageSize );
	byte *pTargetImage = (byte*)targetImage.Base();

	// conversion may skip bytes, ensure all bits initialized
	memset( pTargetImage, 0xFF, targetImageSize );

	// format conversion expects pc oriented data
	bool bRetVal = ConvertImageFormatEx( 
				pSourceImage, 
				sourceImageSize,
				pSourceVTF->Format(), 
				pTargetImage, 
				targetImageSize,
				targetFormat, 
				width, 
				height,
				( pSourceVTF->Flags() & TEXTUREFLAGS_SRGB ) ? true : false );
	if ( !bRetVal )
	{
		return false;
	}

//X360TBD: incorrect byte order
//	// fixup mip dependent data
//	if ( ( pSourceVTF->Flags() & TEXTUREFLAGS_ONEOVERMIPLEVELINALPHA ) && ( targetFormat == IMAGE_FORMAT_BGRA8888 ) )
//	{
//		unsigned char ooMipLevel = ( unsigned char )( 255.0f * ( 1.0f / ( float )( 1 << mip ) ) );
//		int i;
//
//		for ( i=0; i<width*height; i++ )
//		{
//			pTargetImage[i*4+3] = ooMipLevel;
//		}
//	}

	targetSize = targetBuf.Size() + targetImageSize;
	targetBuf.EnsureCapacity( targetSize );
	targetBuf.Put( pTargetImage, targetImageSize );
	if ( !targetBuf.IsValid() )
	{
		return false;
	}

	// success
	return true;
}

//-----------------------------------------------------------------------------
// Generate the 360 target into a buffer
//-----------------------------------------------------------------------------
bool ConvertVTFTo360Format( const char *pDebugName, CUtlBuffer &sourceBuf, CUtlBuffer &targetBuf, CompressFunc_t pCompressFunc )
{
	bool				bRetVal;
	IVTFTexture			*pSourceVTF;
	int					targetWidth;
	int					targetHeight;
	int					targetMipCount;
	VTFFileHeaderX360_t	targetHeader;
	int					frame;
	int					face;
	int					mip;
	ImageFormat			targetFormat;
	int					targetLowResWidth;
	int					targetLowResHeight;
	int					targetFlags;
	int					mipSkipCount;
	int					targetFaceCount;
	int					preloadDataSize;
	int					targetImageDataOffset;
	int					targetFrameCount;
	VTFFileHeaderV7_1_t	*pVTFHeader71;
	bool				bNoMip;
	CByteswap			byteSwapWriter;
	CUtlVector< ResourceCopy_t > targetResources;
	bool bHasLowResData = false;
	unsigned int resourceTypes[MAX_RSRC_DICTIONARY_ENTRIES];
	unsigned char targetLowResSample[4];
	int numTypes;
	
	// Only need to byte swap writes if we are running the coversion on the PC, and data will be read from 360
	byteSwapWriter.ActivateByteSwapping( !IsX360() );

	// need mathlib
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	// default failure
	bRetVal = false;

	pSourceVTF = NULL;

	// unserialize the vtf with just the header
	pSourceVTF = CreateVTFTexture();
	if ( !pSourceVTF->Unserialize( sourceBuf, true, 0 ) )
		goto cleanUp;

	// volume textures not supported
	if ( pSourceVTF->Depth() != 1 )
		goto cleanUp;

	if ( !ImageLoader::IsFormatValidForConversion( pSourceVTF->Format() ) )
		goto cleanUp;

	if ( !ComputeTargetDimensions( pDebugName, pSourceVTF, 0, targetWidth, targetHeight, targetMipCount, mipSkipCount, bNoMip ) )
		goto cleanUp;

	// must crack vtf file to determine if mip levels exist from header
	// vtf interface does not expose the true presence of this data
	pVTFHeader71 = (VTFFileHeaderV7_1_t*)sourceBuf.Base();
	if ( mipSkipCount >= pVTFHeader71->numMipLevels )
	{
		// can't skip mips that aren't there
		// ideally should just reconstruct them
		goto cleanUp;
	}

	// unserialize the vtf with all the data configured with the desired starting mip
	sourceBuf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	if ( !pSourceVTF->Unserialize( sourceBuf, false, mipSkipCount ) )
	{
		Msg( "ConvertVTFTo360Format: Error reading in %s\n", pDebugName );
		goto cleanUp;
	}

	// add the default resource image
	ResourceCopy_t resourceCopy;
	resourceCopy.m_EntryInfo.eType = VTF_LEGACY_RSRC_IMAGE;
	resourceCopy.m_EntryInfo.resData = 0;
	resourceCopy.m_pData = NULL;
	resourceCopy.m_DataLength = 0;
	targetResources.AddToTail( resourceCopy );

	// get the resources
	numTypes = pSourceVTF->GetResourceTypes( resourceTypes, MAX_RSRC_DICTIONARY_ENTRIES );
	for ( int i=0; i<numTypes; i++ )
	{
		size_t resourceLength;
		void *pResourceData;

		switch ( resourceTypes[i] & ~RSRCF_MASK )
		{
		case VTF_LEGACY_RSRC_LOW_RES_IMAGE:
		case VTF_LEGACY_RSRC_IMAGE:
		case VTF_RSRC_TEXTURE_LOD_SETTINGS:
		case VTF_RSRC_TEXTURE_SETTINGS_EX:
		case VTF_RSRC_TEXTURE_CRC:
			// not needed, presence already folded into conversion
			continue;

		default:
			pResourceData = pSourceVTF->GetResourceData( resourceTypes[i], &resourceLength );
			if ( pResourceData )
			{
				resourceCopy.m_EntryInfo.eType = resourceTypes[i] & ~RSRCF_MASK;
				resourceCopy.m_EntryInfo.resData = 0;
				resourceCopy.m_pData = new char[resourceLength];
				resourceCopy.m_DataLength = resourceLength;
				V_memcpy( resourceCopy.m_pData, pResourceData, resourceLength );
				targetResources.AddToTail( resourceCopy );
			}
			break;
		}
	}

	if ( targetResources.Count() > MAX_X360_RSRC_DICTIONARY_ENTRIES )
	{
		Msg( "ConvertVTFTo360Format: More resources than expected in %s\n", pDebugName );
		goto cleanUp;
	}

	targetFlags = pSourceVTF->Flags();
	targetFrameCount = pSourceVTF->FrameCount();

	// skip over spheremap
	targetFaceCount = pSourceVTF->FaceCount();
	if ( targetFaceCount == CUBEMAP_FACE_COUNT )
	{
		targetFaceCount = CUBEMAP_FACE_COUNT-1;
	}

	// determine target format
	targetFormat = PreferredFormat( pSourceVTF, pSourceVTF->Format(), targetWidth, targetHeight, targetMipCount, targetFaceCount );

	// reset nomip flags
	if ( bNoMip )
	{
		targetFlags |= TEXTUREFLAGS_NOMIP;
	}
	else
	{
		targetFlags &= ~TEXTUREFLAGS_NOMIP;
	}

	// the lowres texture is used for coarse light sampling lookups
	bHasLowResData = ( pSourceVTF->LowResFormat() != -1 ) && pSourceVTF->LowResWidth() && pSourceVTF->LowResHeight();
	if ( bHasLowResData )
	{
		// ensure lowres data is serialized in preferred runtime expected format
		targetLowResWidth  = pSourceVTF->LowResWidth();
		targetLowResHeight = pSourceVTF->LowResHeight();
	}
	else
	{
		// discarding low res data, ensure lowres data is culled
		targetLowResWidth  = 0;
		targetLowResHeight = 0;
	}

	// start serializing output data
	// skip past header
	// serialize in order, 0) Header 1) ResourceDictionary, 3) Resources, 4) image
	// preload may extend into image
	targetBuf.EnsureCapacity( sizeof( VTFFileHeaderX360_t ) + targetResources.Count() * sizeof( ResourceEntryInfo ) );
	targetBuf.SeekPut( CUtlBuffer::SEEK_CURRENT, sizeof( VTFFileHeaderX360_t ) + targetResources.Count() * sizeof( ResourceEntryInfo ) );
	
	// serialize low res
	if ( targetLowResWidth && targetLowResHeight )
	{
		CUtlMemory<byte> targetLowResImage;

		int sourceLowResImageSize = ImageLoader::GetMemRequired( pSourceVTF->LowResWidth(), pSourceVTF->LowResHeight(), 1, pSourceVTF->LowResFormat(), false );
		int targetLowResImageSize = ImageLoader::GetMemRequired( targetLowResWidth, targetLowResHeight, 1, IMAGE_FORMAT_RGB888, false );
	
		// conversion may skip bytes, ensure all bits initialized
		targetLowResImage.EnsureCapacity( targetLowResImageSize );
		byte* pTargetLowResImage = (byte*)targetLowResImage.Base();
		memset( pTargetLowResImage, 0xFF, targetLowResImageSize );

		// convert and save lowres image in final format
		bRetVal = ConvertImageFormatEx( 
					pSourceVTF->LowResImageData(), 
					sourceLowResImageSize,
					pSourceVTF->LowResFormat(), 
					pTargetLowResImage, 
					targetLowResImageSize,
					IMAGE_FORMAT_RGB888, 
					targetLowResWidth, 
					targetLowResHeight,
					false );
		if ( !bRetVal )
		{
			goto cleanUp;
		}

		// boil to a single linear color
		Vector linearColor;
		linearColor.x = linearColor.y = linearColor.z = 0;
		for ( int j = 0; j < targetLowResWidth * targetLowResHeight; j++ )
		{
			linearColor.x += SrgbGammaToLinear( pTargetLowResImage[j*3+0] * 1.0f/255.0f );
			linearColor.y += SrgbGammaToLinear( pTargetLowResImage[j*3+1] * 1.0f/255.0f );
			linearColor.z += SrgbGammaToLinear( pTargetLowResImage[j*3+2] * 1.0f/255.0f );
		}
		VectorScale( linearColor, 1.0f/(targetLowResWidth * targetLowResHeight), linearColor );

		// serialize as a single texel
		targetLowResSample[0] = 255.0f * SrgbLinearToGamma( linearColor[0] );
		targetLowResSample[1] = 255.0f * SrgbLinearToGamma( linearColor[1] );
		targetLowResSample[2] = 255.0f * SrgbLinearToGamma( linearColor[2] );

		// identifies color presence
		targetLowResSample[3] = 0xFF;
	}
	else
	{
		targetLowResSample[0] = 0;
		targetLowResSample[1] = 0;
		targetLowResSample[2] = 0;
		targetLowResSample[3] = 0;
	}

	// serialize resource data
	for ( int i=0; i<targetResources.Count(); i++ )
	{
		int resourceDataLength = targetResources[i].m_DataLength;
		if ( resourceDataLength == 4 )
		{
			// data goes directly into structure, as is
			targetResources[i].m_EntryInfo.eType |= RSRCF_HAS_NO_DATA_CHUNK;
			V_memcpy( &targetResources[i].m_EntryInfo.resData, targetResources[i].m_pData, 4 );
		}
		else if ( resourceDataLength != 0 )
		{
			targetResources[i].m_EntryInfo.resData = targetBuf.TellPut();
			int swappedLength = 0;
			byteSwapWriter.SwapBufferToTargetEndian( &swappedLength, &resourceDataLength );
			targetBuf.PutInt( swappedLength );
			if ( !targetBuf.IsValid() )
			{
				goto cleanUp;
			}
	
			// put the data
			targetBuf.Put( targetResources[i].m_pData, resourceDataLength );	
			if ( !targetBuf.IsValid() )
			{
				goto cleanUp;
			}
		}
	}
	
	// mark end of preload data
	// preload data might be updated and pushed to extend into the image data mip chain
	preloadDataSize = targetBuf.TellPut();

	// image starts on an aligned boundary
	AlignBuffer( targetBuf, 4 );
	
	// start of image data
	targetImageDataOffset = targetBuf.TellPut();
	if ( targetImageDataOffset >= 65536 )
	{
		// possible bug, or may have to offset to 32 bits
		Msg( "ConvertVTFTo360Format: non-image portion exceeds 16 bit boundary %s\n", pDebugName );
		goto cleanUp;
	}

	// format conversion, data is stored by ascending mips, 1x1 up to NxN
	// data is stored ascending to allow picmipped loads
	for ( mip = targetMipCount - 1; mip >= 0; mip-- )
	{
		for ( frame = 0; frame < targetFrameCount; frame++ )
		{
			for ( face = 0; face < targetFaceCount; face++ )
			{
				if ( !SerializeImageData( pSourceVTF, frame, face, mip, targetFormat, targetBuf ) )
				{
					goto cleanUp;
				}
			}
		}
	}

	if ( preloadDataSize < VTFFileHeaderSize( VTF_X360_MAJOR_VERSION, VTF_X360_MINOR_VERSION ) )
	{
		// preload size must be at least what game attempts to initially read
		preloadDataSize = VTFFileHeaderSize( VTF_X360_MAJOR_VERSION, VTF_X360_MINOR_VERSION ); 
	}

	if ( targetBuf.TellPut() <= PRELOAD_VTF_THRESHOLD )
	{
		// the entire file is too small, preload entirely
		preloadDataSize = targetBuf.TellPut();
	}

	if ( preloadDataSize >= 65536 )
	{
		// possible overflow due to large frames, faces, and format, may have to offset to 32 bits
		Msg( "ConvertVTFTo360Format: preload portion exceeds 16 bit boundary %s\n", pDebugName );
		goto cleanUp;
	}

	// finalize header
	V_memset( &targetHeader, 0, sizeof( VTFFileHeaderX360_t ) );

	V_memcpy( targetHeader.fileTypeString, "VTFX", 4 );
	targetHeader.version[0] = VTF_X360_MAJOR_VERSION;
	targetHeader.version[1] = VTF_X360_MINOR_VERSION;
	targetHeader.headerSize = sizeof( VTFFileHeaderX360_t ) + targetResources.Count() * sizeof( ResourceEntryInfo );

	targetHeader.flags = targetFlags;
	targetHeader.width = targetWidth;
	targetHeader.height = targetHeight;
	targetHeader.depth = 1;
	targetHeader.numFrames = targetFrameCount;
	targetHeader.preloadDataSize = preloadDataSize;
	targetHeader.mipSkipCount = mipSkipCount;
	targetHeader.numResources = targetResources.Count();
	VectorCopy( pSourceVTF->Reflectivity(), targetHeader.reflectivity );
	targetHeader.bumpScale = pSourceVTF->BumpScale();
	targetHeader.imageFormat = targetFormat;
	targetHeader.lowResImageSample[0] = targetLowResSample[0];
	targetHeader.lowResImageSample[1] = targetLowResSample[1];
	targetHeader.lowResImageSample[2] = targetLowResSample[2];
	targetHeader.lowResImageSample[3] = targetLowResSample[3];

	if ( !IsX360() )
	{
		byteSwapWriter.SwapFieldsToTargetEndian( &targetHeader );
	}

	// write out finalized header
	targetBuf.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
	targetBuf.Put( &targetHeader, sizeof( VTFFileHeaderX360_t ) );
	if ( !targetBuf.IsValid() )
	{
		goto cleanUp;
	}

	// fixup and write out finalized resource dictionary
	for ( int i=0; i<targetResources.Count(); i++ )
	{
		switch ( targetResources[i].m_EntryInfo.eType & ~RSRCF_MASK )
		{
			case VTF_LEGACY_RSRC_IMAGE:
				targetResources[i].m_EntryInfo.resData = targetImageDataOffset;
				break;
		}

		if ( !( targetResources[i].m_EntryInfo.eType & RSRCF_HAS_NO_DATA_CHUNK ) )
		{
			// swap the offset holders only
			byteSwapWriter.SwapBufferToTargetEndian( &targetResources[i].m_EntryInfo.resData );
		}

		targetBuf.Put( &targetResources[i].m_EntryInfo, sizeof( ResourceEntryInfo ) );
		if ( !targetBuf.IsValid() )
		{
			goto cleanUp;
		}
	}

	targetBuf.SeekPut( CUtlBuffer::SEEK_TAIL, 0 );

	if ( preloadDataSize < targetBuf.TellPut() && pCompressFunc )
	{
		// only compress files that are not entirely in preload
		CUtlBuffer compressedBuffer;
		targetBuf.SeekGet( CUtlBuffer::SEEK_HEAD, targetImageDataOffset );
		bool bCompressed = pCompressFunc( targetBuf, compressedBuffer );
		if ( bCompressed )
		{
			// copy all the header data off
			CUtlBuffer headerBuffer;
			headerBuffer.EnsureCapacity( targetImageDataOffset );
			headerBuffer.Put( targetBuf.Base(), targetImageDataOffset );

			// reform the target with the header and then the compressed data
			targetBuf.Clear();
			targetBuf.Put( headerBuffer.Base(), targetImageDataOffset );
			targetBuf.Put( compressedBuffer.Base(), compressedBuffer.TellPut() );

			VTFFileHeaderX360_t *pHeader = (VTFFileHeaderX360_t *)targetBuf.Base();
			if ( !IsX360() )
			{
				// swap it back into pc space
				byteSwapWriter.SwapFieldsToTargetEndian( pHeader );
			}

			pHeader->compressedSize = compressedBuffer.TellPut();			

			if ( !IsX360() )
			{
				// swap it back into 360 space
				byteSwapWriter.SwapFieldsToTargetEndian( pHeader );
			}
		}

		targetBuf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	}

	// success
	bRetVal = true;

cleanUp:
	if ( pSourceVTF )
	{
		DestroyVTFTexture( pSourceVTF );
	}

	for ( int i=0; i<targetResources.Count(); i++ )
	{
		delete [] (char *)targetResources[i].m_pData;
		targetResources[i].m_pData = NULL;
	}

	return bRetVal;
}

//-----------------------------------------------------------------------------
// Copy the 360 preload data into a buffer. Used by tools to request the preload,
// as part of the preload build process. Caller doesn't have to know cracking details.
// Not to be used at gametime.
//-----------------------------------------------------------------------------
bool GetVTFPreload360Data( const char *pDebugName, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut )
{
	preloadBufferOut.Purge();

	fileBufferIn.ActivateByteSwapping( IsPC() );

	VTFFileHeaderX360_t	header;
	fileBufferIn.GetObjects( &header );

	if ( V_strnicmp( header.fileTypeString, "VTFX", 4 ) ||	
		header.version[0] != VTF_X360_MAJOR_VERSION || 
		header.version[1] != VTF_X360_MINOR_VERSION )
	{
		// bad format
		return false;
	}

	preloadBufferOut.EnsureCapacity( header.preloadDataSize );
	preloadBufferOut.Put( fileBufferIn.Base(), header.preloadDataSize );

	return true;
}
