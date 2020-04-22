//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef TEXTUREDX8_H
#define TEXTUREDX8_H

#ifdef _WIN32
#pragma once
#endif

#include "togl/rendermechanism.h"
#include "bitmap/imageformat.h"
#include "locald3dtypes.h"
#include "shaderapi/ishaderapi.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CPixelWriter;


//-----------------------------------------------------------------------------
// Returns the size of texture memory
//-----------------------------------------------------------------------------
int ComputeTextureMemorySize( const GUID &nDeviceId, D3DDEVTYPE deviceType );


//-----------------------------------------------------------------------------
// Texture creation
//-----------------------------------------------------------------------------
IDirect3DBaseTexture *CreateD3DTexture( int width, int height, int depth, 
	ImageFormat dstFormat, int numLevels, int creationFlags, char *debugLabel=NULL );	// OK to not-supply the last param


//-----------------------------------------------------------------------------
// Texture destruction
//-----------------------------------------------------------------------------
void DestroyD3DTexture( IDirect3DBaseTexture *pTex );

int GetD3DTextureRefCount( IDirect3DBaseTexture *pTex );


//-----------------------------------------------------------------------------
// Texture heap methods
//-----------------------------------------------------------------------------
#if defined( _X360 )
void SetD3DTextureImmobile( IDirect3DBaseTexture *pTex, bool bImmobile );
void CompactTextureHeap();
#endif

//-----------------------------------------------------------------------------
// Stats...
//-----------------------------------------------------------------------------
int TextureCount();


//-----------------------------------------------------------------------------
// Info for texture loading
//-----------------------------------------------------------------------------
struct TextureLoadInfo_t
{
	ShaderAPITextureHandle_t	m_TextureHandle;
	int							m_nCopy;
	IDirect3DBaseTexture		*m_pTexture;
	int							m_nLevel; 
	D3DCUBEMAP_FACES			m_CubeFaceID;
	int							m_nWidth;
	int							m_nHeight;
	int16						m_nZOffset;				// What z-slice of the volume texture are we loading?
#if defined( _X360 )
	bool						m_bSrcIsTiled;			// format may not be, but data could be
	bool						m_bCanConvertFormat;	// allow format conversion
#else
	bool						m_bTextureIsLockable;
#endif
	ImageFormat					m_SrcFormat;
	unsigned char				*m_pSrcData;
};


//-----------------------------------------------------------------------------
// Texture image upload
//-----------------------------------------------------------------------------
void LoadTexture( TextureLoadInfo_t &info );
void LoadTextureFromVTF( TextureLoadInfo_t &info, IVTFTexture* pVTF, int iVTFFrame );
void LoadCubeTextureFromVTF( TextureLoadInfo_t &info, IVTFTexture* pVTF, int iVTFFrame );
void LoadVolumeTextureFromVTF( TextureLoadInfo_t &info, IVTFTexture* pVTF, int iVTFFrame );

//-----------------------------------------------------------------------------
// Upload to a sub-piece of a texture
//-----------------------------------------------------------------------------
void LoadSubTexture( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride );

//-----------------------------------------------------------------------------
// Lock, unlock a texture...
//-----------------------------------------------------------------------------
bool LockTexture( ShaderAPITextureHandle_t textureHandle, int copy, IDirect3DBaseTexture* pTexture, int level, 
	D3DCUBEMAP_FACES cubeFaceID, int xOffset, int yOffset, int width, int height, bool bDiscard,
	CPixelWriter& writer );

void UnlockTexture( ShaderAPITextureHandle_t textureHandle, int copy, IDirect3DBaseTexture* pTexture, int level, 
	D3DCUBEMAP_FACES cubeFaceID );

#endif // TEXTUREDX8_H