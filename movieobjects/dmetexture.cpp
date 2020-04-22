//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmetexture.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects_interfaces.h"

#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"

//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeBaseTexture, CDmeBaseTexture );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeBaseTexture::OnConstruction()
{
	m_pVTFTexture = CreateVTFTexture();

	m_bClampS.Init( this, "clampS" );
	m_bClampT.Init( this, "clampT" );
	m_bClampU.Init( this, "clampU" );
	m_bNoLod.Init( this, "noMipmapLOD" );
	m_bNiceFiltered.Init( this, "niceFiltered" );
	m_bNormalMap.Init( this, "normalMap" );
	m_flBumpScale.Init( this, "bumpScale" );
	m_nCompressType.Init( this, "compressType" );
	m_nFilterType.Init( this, "filterType" );
	m_nMipmapType.Init( this, "mipmapType" );
}

void CDmeBaseTexture::OnDestruction()
{
	if ( m_pVTFTexture )
	{
		DestroyVTFTexture( m_pVTFTexture );
		m_pVTFTexture = NULL;
	}
}


//-----------------------------------------------------------------------------
// accessor for cached ITexture
//-----------------------------------------------------------------------------
ITexture *CDmeBaseTexture::GetCachedTexture()
{
	return m_Texture;
}


//-----------------------------------------------------------------------------
// Computes texture flags
//-----------------------------------------------------------------------------
int CDmeBaseTexture::CalcTextureFlags( int nDepth ) const
{
	int nFlags = 0;
	if ( m_bClampS )
	{
		nFlags |= TEXTUREFLAGS_CLAMPS;
	}
	if ( m_bClampT )
	{
		nFlags |= TEXTUREFLAGS_CLAMPT;
	}
	if ( m_bClampU )
	{
		nFlags |= TEXTUREFLAGS_CLAMPU;
	}
	if ( m_bNoLod )
	{
		nFlags |= TEXTUREFLAGS_NOLOD;
	}
	if ( m_bNiceFiltered )
	{
		nFlags |= TEXTUREFLAGS_NICEFILTERED;
	}
	if ( m_bNormalMap )
	{
		nFlags |= TEXTUREFLAGS_NORMAL;
	}
	if ( m_bNormalMap )
	{
		nFlags |= TEXTUREFLAGS_NORMAL;
	}
	if ( m_bNoDebugOverride )
	{
		nFlags |= TEXTUREFLAGS_NODEBUGOVERRIDE;
	}

	switch ( m_nCompressType )
	{
	case DMETEXTURE_COMPRESS_DEFAULT:
	case DMETEXTURE_COMPRESS_DXT1:
		break;
	case DMETEXTURE_COMPRESS_NONE:
		nFlags |= TEXTUREFLAGS_NOCOMPRESS;
		break;
	case DMETEXTURE_COMPRESS_DXT5:
		nFlags |= TEXTUREFLAGS_HINT_DXT5;
		break;
	}

	switch ( m_nFilterType )
	{
	case DMETEXTURE_FILTER_DEFAULT:
	case DMETEXTURE_FILTER_BILINEAR:
		break;
	case DMETEXTURE_FILTER_ANISOTROPIC:
		nFlags |= TEXTUREFLAGS_ANISOTROPIC;
		break;
	case DMETEXTURE_FILTER_TRILINEAR:
		nFlags |= TEXTUREFLAGS_TRILINEAR;
		break;
	case DMETEXTURE_FILTER_POINT:
		nFlags |= TEXTUREFLAGS_POINTSAMPLE;
		break;
	}

	switch ( m_nMipmapType )
	{
	case DMETEXTURE_MIPMAP_DEFAULT:
	case DMETEXTURE_MIPMAP_ALL_LEVELS:
		break;
	case DMETEXTURE_MIPMAP_NONE:
		nFlags |= TEXTUREFLAGS_NOMIP;
		break;
	}

	if ( nDepth > 1 )
	{
		// FIXME: Volume textures don't currently support DXT compression
		nFlags &= ~TEXTUREFLAGS_HINT_DXT5;
		nFlags |= TEXTUREFLAGS_NOCOMPRESS;

		// FIXME: Volume textures don't currently support NICE filtering
		nFlags &= ~TEXTUREFLAGS_NICEFILTERED;
	}

	return nFlags;
}


//-----------------------------------------------------------------------------
// Computes the desired texture format based on flags
//-----------------------------------------------------------------------------
ImageFormat CDmeBaseTexture::ComputeDesiredImageFormat( ImageFormat srcFormat, int nWidth, int nHeight, int nDepth, int nFlags )
{
	// HDRFIXME: Need to figure out what format to use here.
	if ( srcFormat == IMAGE_FORMAT_RGB323232F )
		return IMAGE_FORMAT_RGBA16161616F;

	/*
	if( bDUDVTarget)
	{
		if ( bCopyAlphaToLuminance && ( nFlags & ( TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA ) ) )
			return IMAGE_FORMAT_UVLX8888;
		return IMAGE_FORMAT_UV88;
	}
	*/

	// can't compress textures that are smaller than 4x4
	if ( (nFlags & TEXTUREFLAGS_NOCOMPRESS) || (nFlags & TEXTUREFLAGS_PROCEDURAL) ||
		( nWidth < 4 ) || ( nHeight < 4 ) || ( nDepth > 1 ) )
	{
		if ( nFlags & ( TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA ) )
			return IMAGE_FORMAT_BGRA8888;
		return IMAGE_FORMAT_BGR888;
	}
	
	if( nFlags & TEXTUREFLAGS_HINT_DXT5 )
		return IMAGE_FORMAT_DXT5;

	// compressed with alpha blending
	if ( nFlags & TEXTUREFLAGS_EIGHTBITALPHA )
		return IMAGE_FORMAT_DXT5;
	
	if ( nFlags & TEXTUREFLAGS_ONEBITALPHA )
		return IMAGE_FORMAT_DXT5; // IMAGE_FORMAT_DXT1_ONEBITALPHA

	return IMAGE_FORMAT_DXT1;
}


//-----------------------------------------------------------------------------
//
// Normal texture 
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeTexture, CDmeTexture );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeTexture::OnConstruction()
{
	m_Images.Init( this, "images" );
}

void CDmeTexture::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Build a VTF representing the 
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// resolve
//-----------------------------------------------------------------------------
void CDmeTexture::Resolve()
{
	// FIXME: Could change this to not shutdown if only the bits have changed
	m_Texture.Shutdown();

	int nFrameCount = m_Images.Count();
	if ( nFrameCount == 0 )
		return;

	// Use the size of the 0th image
	CDmeImage *pImage = m_Images[0];
	int nWidth = pImage->m_Width;
	int nHeight = pImage->m_Height;
	int nDepth = pImage->m_Depth;
	ImageFormat srcFormat = pImage->Format();

	// FIXME: How should this work exactly?
	int nFlags = CalcTextureFlags( nDepth );

	m_pVTFTexture->Init( nWidth, nHeight, nDepth, srcFormat, nFlags, nFrameCount );


	ImageFormat format = ComputeDesiredImageFormat( pImage->Format(), nWidth, nHeight, nDepth, nFlags );

//	m_Texture.InitProceduralTexture( GetName(), "DmeTexture", nWidth, nHeight, nDepth, nFrameCount, format, nFlags );
//	m_Texture->SetTextureRegenerator( this );

	// Fill in the texture bits
}

/*
//-----------------------------------------------------------------------------
// Fill in the texture bits
//-----------------------------------------------------------------------------
void CDmeTexture::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
{
}
*/

//-----------------------------------------------------------------------------
//
// Cube texture 
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeCubeTexture, CDmeCubeTexture );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeCubeTexture::OnConstruction()
{
	m_ImagesPosX.Init( this, "imagesPosX" );
	m_ImagesNegX.Init( this, "imagesNegX" );
	m_ImagesPosY.Init( this, "imagesPosY" );
	m_ImagesNegY.Init( this, "imagesNegY" );
	m_ImagesPosZ.Init( this, "imagesPosZ" );
	m_ImagesNegZ.Init( this, "imagesNegZ" );
}

void CDmeCubeTexture::OnDestruction()
{
}
