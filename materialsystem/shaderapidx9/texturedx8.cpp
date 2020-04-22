//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#define DISABLE_PROTECTED_THINGS
#include "locald3dtypes.h"
#include "texturedx8.h"
#include "shaderapidx8_global.h"
#include "colorformatdx8.h"
#include "shaderapi/ishaderutil.h"
#include "materialsystem/imaterialsystem.h"
#include "utlvector.h"
#include "recording.h"
#include "shaderapi/ishaderapi.h"
#include "filesystem.h"
#include "locald3dtypes.h"
#include "textureheap.h"
#include "tier1/utlbuffer.h"
#include "tier1/callqueue.h"
#include "tier0/vprof.h"
#include "vtf/vtf.h"
#include "tier0/icommandline.h"

#include "tier0/memdbgon.h"

#ifdef _WIN32
#pragma warning (disable:4189 4701)
#endif

static int s_TextureCount = 0;
static bool s_bTestingVideoMemorySize = false;

//-----------------------------------------------------------------------------
// Stats...
//-----------------------------------------------------------------------------

int TextureCount()
{
	return s_TextureCount;
}

static bool IsVolumeTexture( IDirect3DBaseTexture* pBaseTexture )
{
	if ( !pBaseTexture )
	{
		return false;
	}

	return ( pBaseTexture->GetType() == D3DRTYPE_VOLUMETEXTURE );
}

static HRESULT GetLevelDesc( IDirect3DBaseTexture* pBaseTexture, UINT level, D3DSURFACE_DESC* pDesc )
{
	MEM_ALLOC_D3D_CREDIT();

	if ( !pBaseTexture )
	{
		return ( HRESULT )-1;
	}

	HRESULT hr;
	switch( pBaseTexture->GetType() )
	{
	case D3DRTYPE_TEXTURE:
		hr = ( ( IDirect3DTexture * )pBaseTexture )->GetLevelDesc( level, pDesc );
		break;
	case D3DRTYPE_CUBETEXTURE:
		hr = ( ( IDirect3DCubeTexture * )pBaseTexture )->GetLevelDesc( level, pDesc );
		break;
	default:
		return ( HRESULT )-1;
	}
	return hr;
}

static HRESULT GetSurfaceFromTexture( IDirect3DBaseTexture* pBaseTexture, UINT level, 
									  D3DCUBEMAP_FACES cubeFaceID, IDirect3DSurface** ppSurfLevel )
{
	MEM_ALLOC_D3D_CREDIT();

	if ( !pBaseTexture )
	{
		return ( HRESULT )-1;
	}

	HRESULT hr;

	switch( pBaseTexture->GetType() )
	{
	case D3DRTYPE_TEXTURE:
		hr = ( ( IDirect3DTexture * )pBaseTexture )->GetSurfaceLevel( level, ppSurfLevel );
		break;
	case D3DRTYPE_CUBETEXTURE:
		if (cubeFaceID !=0)
		{
			//Debugger();
		}
		
		hr = ( ( IDirect3DCubeTexture * )pBaseTexture )->GetCubeMapSurface( cubeFaceID, level, ppSurfLevel );
		break;
	default:
		Assert(0);
		return ( HRESULT )-1;
	}
	return hr;
}

//-----------------------------------------------------------------------------
// Gets the image format of a texture
//-----------------------------------------------------------------------------
static ImageFormat GetImageFormat( IDirect3DBaseTexture* pTexture )
{
	MEM_ALLOC_D3D_CREDIT();

	if ( pTexture )
	{
		HRESULT hr;
		if ( !IsVolumeTexture( pTexture ) )
		{
			D3DSURFACE_DESC desc;
			hr = GetLevelDesc( pTexture, 0, &desc );
			if ( !FAILED( hr ) )
				return ImageLoader::D3DFormatToImageFormat( desc.Format );
		}
		else
		{
			D3DVOLUME_DESC desc;
			IDirect3DVolumeTexture *pVolumeTexture = static_cast<IDirect3DVolumeTexture*>( pTexture );
			hr = pVolumeTexture->GetLevelDesc( 0, &desc );
			if ( !FAILED( hr ) )
				return ImageLoader::D3DFormatToImageFormat( desc.Format );
		}
	}

	// Bogus baby!
	return (ImageFormat)-1;
}


//-----------------------------------------------------------------------------
// Allocates the D3DTexture
//-----------------------------------------------------------------------------
IDirect3DBaseTexture* CreateD3DTexture( int width, int height, int nDepth, 
		ImageFormat dstFormat, int numLevels, int nCreationFlags, char *debugLabel )		// OK to skip the last param
{
	if ( nDepth <= 0 )
	{
		nDepth = 1;
	}

	bool isCubeMap = ( nCreationFlags & TEXTURE_CREATE_CUBEMAP ) != 0;
	bool bIsRenderTarget = ( nCreationFlags & TEXTURE_CREATE_RENDERTARGET ) != 0;
	bool bManaged = ( nCreationFlags & TEXTURE_CREATE_MANAGED ) != 0;
	bool bSysmem = ( nCreationFlags & TEXTURE_CREATE_SYSMEM ) != 0;
	bool bIsDepthBuffer = ( nCreationFlags & TEXTURE_CREATE_DEPTHBUFFER ) != 0;
	bool isDynamic = ( nCreationFlags & TEXTURE_CREATE_DYNAMIC ) != 0;
	bool bAutoMipMap = ( nCreationFlags & TEXTURE_CREATE_AUTOMIPMAP ) != 0;
	bool bVertexTexture = ( nCreationFlags & TEXTURE_CREATE_VERTEXTEXTURE ) != 0;
	bool bAllowNonFilterable = ( nCreationFlags & TEXTURE_CREATE_UNFILTERABLE_OK ) != 0;
	bool bVolumeTexture = ( nDepth > 1 );
	bool bIsFallback = ( nCreationFlags & TEXTURE_CREATE_FALLBACK ) != 0;
	bool bNoD3DBits = ( nCreationFlags & TEXTURE_CREATE_NOD3DMEMORY ) != 0;
	bool bSRGB = (nCreationFlags & TEXTURE_CREATE_SRGB) != 0;			// for Posix/GL only

	// NOTE: This function shouldn't be used for creating depth buffers!
	Assert( !bIsDepthBuffer );

	D3DFORMAT d3dFormat = D3DFMT_UNKNOWN;

	D3DPOOL pool = bManaged ? D3DPOOL_MANAGED : D3DPOOL_DEFAULT;
	if ( bSysmem )
		pool = D3DPOOL_SYSTEMMEM;

	if ( IsX360() )
	{
		// 360 does not support vertex textures
		// 360 render target creation path is for the target as a texture source (NOT the EDRAM version)
		// use normal texture format rules
		Assert( !bVertexTexture );
		if ( !bVertexTexture )
		{
			d3dFormat = ImageLoader::ImageFormatToD3DFormat( FindNearestSupportedFormat( dstFormat, false, false, false ) );
		}
	}
	else
	{
		d3dFormat = ImageLoader::ImageFormatToD3DFormat( FindNearestSupportedFormat( dstFormat, bVertexTexture, bIsRenderTarget, bAllowNonFilterable ) );
	}

	if ( d3dFormat == D3DFMT_UNKNOWN )
	{
		Warning( "ShaderAPIDX8::CreateD3DTexture: Invalid color format!\n" );
		Assert( 0 );
		return 0;
	}

	IDirect3DBaseTexture* pBaseTexture = NULL;
	IDirect3DTexture* pD3DTexture = NULL;
	IDirect3DCubeTexture* pD3DCubeTexture = NULL;
	IDirect3DVolumeTexture* pD3DVolumeTexture = NULL;
	HRESULT hr = S_OK;
	DWORD usage = 0;

	if ( bIsRenderTarget )
	{
		usage |= D3DUSAGE_RENDERTARGET;
	}
	if ( isDynamic )
	{
		usage |= D3DUSAGE_DYNAMIC;
	}
	if ( bAutoMipMap )
	{
		usage |= D3DUSAGE_AUTOGENMIPMAP;
	}

#ifdef DX_TO_GL_ABSTRACTION
	{
		if (bSRGB)
		{
			usage |= D3DUSAGE_TEXTURE_SRGB;		// does not exist in real DX9... just for GL to know that this is an SRGB tex
		}
	}
#endif

	if ( isCubeMap )
	{
#if !defined( _X360 )
		hr = Dx9Device()->CreateCubeTexture( 
				width,
				numLevels,
				usage,
				d3dFormat,
				pool, 
				&pD3DCubeTexture,
				NULL
	#if defined( DX_TO_GL_ABSTRACTION )			
				, debugLabel					// tex create funcs take extra arg for debug name on GL
	#endif
				   );
#else
		pD3DCubeTexture = g_TextureHeap.AllocCubeTexture( width, numLevels, usage, d3dFormat, bIsFallback, bNoD3DBits );
#endif
		pBaseTexture = pD3DCubeTexture;
	}
	else if ( bVolumeTexture )
	{
#if !defined( _X360 )
		hr = Dx9Device()->CreateVolumeTexture( 
				width, 
				height, 
				nDepth,
				numLevels, 
				usage, 
				d3dFormat, 
				pool, 
				&pD3DVolumeTexture,
				NULL
	#if defined( DX_TO_GL_ABSTRACTION )			
				, debugLabel					// tex create funcs take extra arg for debug name on GL
	#endif
				  );
#else
		Assert( !bIsFallback && !bNoD3DBits );
		pD3DVolumeTexture = g_TextureHeap.AllocVolumeTexture( width, height, nDepth, numLevels, usage, d3dFormat );
#endif
		pBaseTexture = pD3DVolumeTexture;
	}
	else
	{
#if !defined( _X360 )
		// Override usage and managed params if using special hardware shadow depth map formats...
		if ( ( d3dFormat == NVFMT_RAWZ ) || ( d3dFormat == NVFMT_INTZ   ) || 
		     ( d3dFormat == D3DFMT_D16 ) || ( d3dFormat == D3DFMT_D24S8 ) || 
			 ( d3dFormat == ATIFMT_D16 ) || ( d3dFormat == ATIFMT_D24S8 ) )
		{
			// Not putting D3DUSAGE_RENDERTARGET here causes D3D debug spew later, but putting the flag causes this create to fail...
			usage = D3DUSAGE_DEPTHSTENCIL;
			bManaged = false;
		}

		// Override managed param if using special null texture format
		if ( d3dFormat == NVFMT_NULL )
		{
			bManaged = false;
		}

		hr = Dx9Device()->CreateTexture(
				width,
				height,
				numLevels, 
				usage,
				d3dFormat,
				pool,
				&pD3DTexture,
				NULL
	#if defined( DX_TO_GL_ABSTRACTION )			
				, debugLabel					// tex create funcs take extra arg for debug name on GL
	#endif
				 );

#else
		pD3DTexture = g_TextureHeap.AllocTexture( width, height, numLevels, usage, d3dFormat, bIsFallback, bNoD3DBits );
#endif
		pBaseTexture = pD3DTexture;
	}

    if ( FAILED( hr ) )
	{
#ifdef ENABLE_NULLREF_DEVICE_SUPPORT
		if( CommandLine()->FindParm( "-nulldevice" ) )
		{
			Warning( "ShaderAPIDX8::CreateD3DTexture: Null device used. Texture not created.\n" );
			return 0;
		}
#endif

		switch ( hr )
		{
		case D3DERR_INVALIDCALL:
			Warning( "ShaderAPIDX8::CreateD3DTexture: D3DERR_INVALIDCALL\n" );
			break;
		case D3DERR_OUTOFVIDEOMEMORY:
			// This conditional is here so that we don't complain when testing
			// how much video memory we have. . this is kinda gross.
			if ( !s_bTestingVideoMemorySize )
			{
				Warning( "ShaderAPIDX8::CreateD3DTexture: D3DERR_OUTOFVIDEOMEMORY\n" );
			}
			break;
		case E_OUTOFMEMORY:
			Warning( "ShaderAPIDX8::CreateD3DTexture: E_OUTOFMEMORY\n" );
			break;
		default:
			break;
		}
		return 0;
	}

#ifdef MEASURE_DRIVER_ALLOCATIONS
	int nMipCount = numLevels;
	if ( !nMipCount )
	{
		while ( width > 1 || height > 1 )
		{
			width >>= 1;
			height >>= 1;
			++nMipCount;
		}
	}

	int nMemUsed = nMipCount * 1.1f * 1024;
	if ( isCubeMap )
	{
		nMemUsed *= 6;
	}

	VPROF_INCREMENT_GROUP_COUNTER( "texture count", COUNTER_GROUP_NO_RESET, 1 );
	VPROF_INCREMENT_GROUP_COUNTER( "texture driver mem", COUNTER_GROUP_NO_RESET, nMemUsed );
	VPROF_INCREMENT_GROUP_COUNTER( "total driver mem", COUNTER_GROUP_NO_RESET, nMemUsed );
#endif

	++s_TextureCount;

	return pBaseTexture;
}


//-----------------------------------------------------------------------------
// Texture destruction
//-----------------------------------------------------------------------------
void ReleaseD3DTexture( IDirect3DBaseTexture* pD3DTex )
{
	int ref = pD3DTex->Release();
	Assert( ref == 0 );
}

void DestroyD3DTexture( IDirect3DBaseTexture* pD3DTex )
{
	if ( pD3DTex )
	{
#ifdef MEASURE_DRIVER_ALLOCATIONS
		D3DRESOURCETYPE type = pD3DTex->GetType();
		int nMipCount = pD3DTex->GetLevelCount();
		if ( type == D3DRTYPE_CUBETEXTURE )
		{
			nMipCount *= 6;
		}
		int nMemUsed = nMipCount * 1.1f * 1024;
		VPROF_INCREMENT_GROUP_COUNTER( "texture count", COUNTER_GROUP_NO_RESET, -1 );
		VPROF_INCREMENT_GROUP_COUNTER( "texture driver mem", COUNTER_GROUP_NO_RESET, -nMemUsed );
		VPROF_INCREMENT_GROUP_COUNTER( "total driver mem", COUNTER_GROUP_NO_RESET, -nMemUsed );
#endif

#if !defined( _X360 )
		CMatRenderContextPtr pRenderContext( materials );
		ICallQueue *pCallQueue;
		if ( ( pCallQueue = pRenderContext->GetCallQueue() ) != NULL )
		{
			pCallQueue->QueueCall( ReleaseD3DTexture, pD3DTex );
		}
		else
		{
			ReleaseD3DTexture( pD3DTex );
		}
#else
		g_TextureHeap.FreeTexture( pD3DTex );
#endif
		--s_TextureCount;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pTex - 
// Output : int
//-----------------------------------------------------------------------------
int GetD3DTextureRefCount( IDirect3DBaseTexture *pTex )
{
	if ( !pTex )
		return 0;

	pTex->AddRef();
	int ref = pTex->Release();

	return ref;
}

//-----------------------------------------------------------------------------
// See version 13 for a function that converts a texture to a mipmap (ConvertToMipmap)
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Lock, unlock a texture...
//-----------------------------------------------------------------------------

static RECT s_LockedSrcRect;
static D3DLOCKED_RECT s_LockedRect;
#ifdef DBGFLAG_ASSERT
static bool s_bInLock = false;
#endif

bool LockTexture( ShaderAPITextureHandle_t bindId, int copy, IDirect3DBaseTexture* pTexture, int level, 
	D3DCUBEMAP_FACES cubeFaceID, int xOffset, int yOffset, int width, int height, bool bDiscard,
	CPixelWriter& writer )
{
	Assert( !s_bInLock );
	
	IDirect3DSurface* pSurf;
	HRESULT hr = GetSurfaceFromTexture( pTexture, level, cubeFaceID, &pSurf );
	if ( FAILED( hr ) )
		return false;

	s_LockedSrcRect.left = xOffset;
	s_LockedSrcRect.right = xOffset + width;
	s_LockedSrcRect.top = yOffset;
	s_LockedSrcRect.bottom = yOffset + height;

	unsigned int flags = D3DLOCK_NOSYSLOCK;
	flags |= bDiscard ? D3DLOCK_DISCARD : 0;
	RECORD_COMMAND( DX8_LOCK_TEXTURE, 6 );
	RECORD_INT( bindId );
	RECORD_INT( copy );
	RECORD_INT( level );
	RECORD_INT( cubeFaceID );
	RECORD_STRUCT( &s_LockedSrcRect, sizeof(s_LockedSrcRect) );
	RECORD_INT( flags );

	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "D3DLockTexture" );

	hr = pSurf->LockRect( &s_LockedRect, &s_LockedSrcRect, flags );
	pSurf->Release();

	if ( FAILED( hr ) )
		return false;

	writer.SetPixelMemory( GetImageFormat(pTexture), s_LockedRect.pBits, s_LockedRect.Pitch );

#ifdef DBGFLAG_ASSERT
	s_bInLock = true;
#endif
	return true;
}

void UnlockTexture( ShaderAPITextureHandle_t bindId, int copy, IDirect3DBaseTexture* pTexture, int level, 
	D3DCUBEMAP_FACES cubeFaceID )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	Assert( s_bInLock );

	IDirect3DSurface* pSurf;
	HRESULT hr = GetSurfaceFromTexture( pTexture, level, cubeFaceID, &pSurf );
	if (FAILED(hr))
		return;

#ifdef RECORD_TEXTURES 
	int width = s_LockedSrcRect.right - s_LockedSrcRect.left;
	int height = s_LockedSrcRect.bottom - s_LockedSrcRect.top;
	int imageFormatSize = ImageLoader::SizeInBytes( GetImageFormat( pTexture ) );
	Assert( imageFormatSize != 0 );
	int validDataBytesPerRow = imageFormatSize * width;
	int storeSize = validDataBytesPerRow * height;
	static CUtlVector< unsigned char > tmpMem;
	if( tmpMem.Size() < storeSize )
	{
		tmpMem.AddMultipleToTail( storeSize - tmpMem.Size() );
	}
	unsigned char *pDst = tmpMem.Base();
	unsigned char *pSrc = ( unsigned char * )s_LockedRect.pBits;
	RECORD_COMMAND( DX8_SET_TEXTURE_DATA, 3 );
	RECORD_INT( validDataBytesPerRow );
	RECORD_INT( height );
	int i;
	for( i = 0; i < height; i++ )
	{
		memcpy( pDst, pSrc, validDataBytesPerRow );
		pDst += validDataBytesPerRow;
		pSrc += s_LockedRect.Pitch;
	}
	RECORD_STRUCT( tmpMem.Base(), storeSize );
#endif // RECORD_TEXTURES 
	
	RECORD_COMMAND( DX8_UNLOCK_TEXTURE, 4 );
	RECORD_INT( bindId );
	RECORD_INT( copy );
	RECORD_INT( level );
	RECORD_INT( cubeFaceID );

	hr = pSurf->UnlockRect();
	pSurf->Release();
#ifdef DBGFLAG_ASSERT
	s_bInLock = false;
#endif
}

//-----------------------------------------------------------------------------
// Compute texture size based on compression
//-----------------------------------------------------------------------------

static inline int DetermineGreaterPowerOfTwo( int val )
{
	int num = 1;
	while (val > num)
	{
		num <<= 1;
	}

	return num;
}

inline int DeterminePowerOfTwo( int val )
{
	int pow = 0;
	while ((val & 0x1) == 0x0)
	{
		val >>= 1;
		++pow;
	}

	return pow;
}


//-----------------------------------------------------------------------------
// Blit in bits
//-----------------------------------------------------------------------------
// NOTE: IF YOU CHANGE THIS, CHANGE THE VERSION IN PLAYBACK.CPP!!!!
// OPTIMIZE??: could lock the texture directly instead of the surface in dx9.
#if !defined( _X360 )
static void BlitSurfaceBits( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	// Get the level of the texture we want to write into
	IDirect3DSurface* pTextureLevel;

	if (info.m_CubeFaceID !=0)
	{
		//Debugger();
	}


	HRESULT hr = GetSurfaceFromTexture( info.m_pTexture, info.m_nLevel, info.m_CubeFaceID, &pTextureLevel );
	if ( FAILED( hr ) )
		return;

	RECT			srcRect;
	RECT			*pSrcRect = NULL;
	D3DLOCKED_RECT	lockedRect;

	srcRect.left   = xOffset;
	srcRect.right  = xOffset + info.m_nWidth;
	srcRect.top    = yOffset;
	srcRect.bottom = yOffset + info.m_nHeight;

#if defined( SHADERAPIDX9 ) && !defined( _X360 ) && !defined( DX_TO_GL_ABSTRACTION )
	if ( !info.m_bTextureIsLockable )
	{
		// Copy from system memory to video memory using D3D9Device->UpdateSurface
		bool bSuccess = false;

		D3DSURFACE_DESC desc;
		Verify( pTextureLevel->GetDesc( &desc ) == S_OK );
		ImageFormat dstFormat = ImageLoader::D3DFormatToImageFormat( desc.Format );
		D3DFORMAT dstFormatD3D = ImageLoader::ImageFormatToD3DFormat( dstFormat );

		IDirect3DSurface* pSrcSurface = NULL;
		bool bCopyBitsToSrcSurface = true;

#if defined(IS_WINDOWS_PC) && defined(SHADERAPIDX9)
		// D3D9Ex fast path: create a texture wrapping our own system memory buffer
		// if the source and destination formats are exactly the same and the stride
		// is tightly packed. no locking/blitting required.
		// NOTE: the fast path does not work on sub-4x4 DXT compressed textures.
		extern bool g_ShaderDeviceUsingD3D9Ex;
		if ( g_ShaderDeviceUsingD3D9Ex &&
			( info.m_SrcFormat == dstFormat || ( info.m_SrcFormat == IMAGE_FORMAT_DXT1_ONEBITALPHA && dstFormat == IMAGE_FORMAT_DXT1 ) ) &&
			( !ImageLoader::IsCompressed( dstFormat ) || (info.m_nWidth >= 4 || info.m_nHeight >= 4) ) )
		{
			if ( srcStride == 0 || srcStride == info.m_nWidth * ImageLoader::SizeInBytes( info.m_SrcFormat ) )
			{
				IDirect3DTexture9* pTempTex = NULL;
				if ( Dx9Device()->CreateTexture( info.m_nWidth, info.m_nHeight, 1, 0, dstFormatD3D, D3DPOOL_SYSTEMMEM, &pTempTex, (HANDLE*) &info.m_pSrcData ) == S_OK )
				{
					IDirect3DSurface* pTempSurf = NULL;
					if ( pTempTex->GetSurfaceLevel( 0, &pTempSurf ) == S_OK )
					{
						pSrcSurface = pTempSurf;
						bCopyBitsToSrcSurface = false;
					}
					pTempTex->Release();
				}
			}
		}
#endif

		// If possible to create a texture of this size, create a temporary texture in
		// system memory and then use the UpdateSurface method to copy between textures.
		if ( !pSrcSurface && ( g_pHardwareConfig->Caps().m_SupportsNonPow2Textures ||
							( IsPowerOfTwo( info.m_nWidth ) && IsPowerOfTwo( info.m_nHeight ) ) ) )
		{
			int tempW = info.m_nWidth, tempH = info.m_nHeight, mip = 0;
			if ( info.m_nLevel > 0 && ( ( tempW | tempH ) & 3 ) && ImageLoader::IsCompressed( dstFormat ) )
			{
				// Loading lower mip levels of DXT compressed textures is sort of tricky
				// because we can't create textures that aren't multiples of 4, and we can't
				// pass subrectangles of DXT textures into UpdateSurface. Create a temporary
				// texture which is 1 or 2 mip levels larger and then lock the appropriate
				// mip level to grab its correctly-dimensioned surface. -henryg 11/18/2011
				mip = ( info.m_nLevel > 1 && ( ( tempW | tempH ) & 1 ) ) ? 2 : 1;
				tempW <<= mip;
				tempH <<= mip;
			}
			
			IDirect3DTexture9* pTempTex = NULL;
			IDirect3DSurface* pTempSurf = NULL;
			if ( Dx9Device()->CreateTexture( tempW, tempH, mip+1, 0, dstFormatD3D, D3DPOOL_SYSTEMMEM, &pTempTex, NULL ) == S_OK )
			{
				if ( pTempTex->GetSurfaceLevel( mip, &pTempSurf ) == S_OK )
				{
					pSrcSurface = pTempSurf;
					bCopyBitsToSrcSurface = true;
				}
				pTempTex->Release();
			}
		}

		// Create an offscreen surface if the texture path wasn't an option.
		if ( !pSrcSurface )
		{
			IDirect3DSurface* pTempSurf = NULL;
			if ( Dx9Device()->CreateOffscreenPlainSurface( info.m_nWidth, info.m_nHeight, dstFormatD3D, D3DPOOL_SYSTEMMEM, &pTempSurf, NULL ) == S_OK )
			{
				pSrcSurface = pTempSurf;
				bCopyBitsToSrcSurface = true;
			}
		}

		// Lock and fill the surface
		if ( bCopyBitsToSrcSurface && pSrcSurface )
		{
			if ( pSrcSurface->LockRect( &lockedRect, NULL, D3DLOCK_NOSYSLOCK ) == S_OK )
			{
				unsigned char *pImage = (unsigned char *)lockedRect.pBits;
				ShaderUtil()->ConvertImageFormat( info.m_pSrcData, info.m_SrcFormat,
					pImage, dstFormat, info.m_nWidth, info.m_nHeight, srcStride, lockedRect.Pitch );
				pSrcSurface->UnlockRect();
			}
			else
			{
				// Lock failed.
				pSrcSurface->Release();
				pSrcSurface = NULL;
			}
		}
	
		// Perform the UpdateSurface call that blits between system and video memory
		if ( pSrcSurface )
		{
			POINT pt = { xOffset, yOffset };
			bSuccess = ( Dx9Device()->UpdateSurface( pSrcSurface, NULL, pTextureLevel, &pt ) == S_OK );
			pSrcSurface->Release();
		}
		
		if ( !bSuccess )
		{
			Warning( "CShaderAPIDX8::BlitTextureBits: couldn't lock texture rect or use UpdateSurface\n" );
		}

		pTextureLevel->Release();
		return;
	}
#endif

	Assert( info.m_bTextureIsLockable );

#ifndef RECORD_TEXTURES
	RECORD_COMMAND( DX8_LOCK_TEXTURE, 6 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
	RECORD_STRUCT( &srcRect, sizeof(srcRect) );
	RECORD_INT( D3DLOCK_NOSYSLOCK );
#endif

	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - D3DLockRect", __FUNCTION__ );

		// lock the region (could be the full surface or less)
		if ( FAILED( pTextureLevel->LockRect( &lockedRect, &srcRect, D3DLOCK_NOSYSLOCK ) ) )
		{
			Warning( "CShaderAPIDX8::BlitTextureBits: couldn't lock texture rect\n" );
			pTextureLevel->Release();
			return;
		}
	}

	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - ConvertImageFormat", __FUNCTION__ );

		// garymcthack : need to make a recording command for this.
		ImageFormat dstFormat = GetImageFormat( info.m_pTexture );
		unsigned char *pImage = (unsigned char *)lockedRect.pBits;
		ShaderUtil()->ConvertImageFormat( info.m_pSrcData, info.m_SrcFormat,
							pImage, dstFormat, info.m_nWidth, info.m_nHeight, srcStride, lockedRect.Pitch );
	}

#ifndef RECORD_TEXTURES
	RECORD_COMMAND( DX8_UNLOCK_TEXTURE, 4 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
#endif

	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - UnlockRect", __FUNCTION__ );

		if ( FAILED( pTextureLevel->UnlockRect() ) ) 
		{
			Warning( "CShaderAPIDX8::BlitTextureBits: couldn't unlock texture rect\n" );
			pTextureLevel->Release();
			return;
		}
	}

	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - pTextureLevel->Release", __FUNCTION__ );
	pTextureLevel->Release();
}
#endif

//-----------------------------------------------------------------------------
// Puts 2D texture data into 360 gpu memory.
//-----------------------------------------------------------------------------
#if defined( _X360 )
static void BlitSurfaceBits( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
	// xbox textures are NOT backed in gpu memory contiguously
	// stride details are critical - see [Xbox 360 Texture Storage]
	// a d3dformat identifier on the xbox is tiled, the same d3dformat on the pc is expected linear to the app
	// we purposely hide the tiling here, otherwise much confusion for the pc
	// the *entire* target must be un-tiled *only* before any *subrect* blitting linear work
	// the *entire* target must then be re-tiled after the *subrect* blit
	// procedural textures require this to subrect blit their new portions correctly
	// the tiling dance can be avoided if the source and target match in tiled state during a full rect blit

	if ( info.m_bSrcIsTiled )
	{
		// not supporting subrect blitting from a tiled source
		Assert( 0 );
		return;
	}

	CUtlBuffer formatConvertMemory;
	unsigned char *pSrcData = info.m_pSrcData;

	ImageFormat	dstFormat = GetImageFormat( info.m_pTexture );
	if ( dstFormat != info.m_SrcFormat )
	{
		if ( !info.m_bCanConvertFormat )
		{
			// texture is expected to be in target format
			// not supporting conversion of a tiled source
			Assert( 0 );
			return;
		}

		int srcSize = ImageLoader::GetMemRequired( info.m_nWidth, info.m_nHeight, 1, info.m_SrcFormat, false );
		int dstSize = ImageLoader::GetMemRequired( info.m_nWidth, info.m_nHeight, 1, dstFormat, false );
		formatConvertMemory.EnsureCapacity( dstSize );

		// due to format conversion, source is in non-native order
		ImageLoader::PreConvertSwapImageData( (unsigned char*)info.m_pSrcData, srcSize, info.m_SrcFormat, info.m_nWidth, srcStride );

		// slow conversion operation
		if ( !ShaderUtil()->ConvertImageFormat( 
				info.m_pSrcData,
				info.m_SrcFormat,
				(unsigned char*)formatConvertMemory.Base(),
				dstFormat,
				info.m_nWidth,
				info.m_nHeight,
				srcStride,
				0 ) )
		{
			// conversion failed
			Assert( 0 );
			return;
		}

		// due to format conversion, source must have been in non-native order
		ImageLoader::PostConvertSwapImageData( (unsigned char*)formatConvertMemory.Base(), dstSize, dstFormat );

		pSrcData = (unsigned char*)formatConvertMemory.Base();
	}

	// get the top mip level info (needed for proper sub mip access)
	XGTEXTURE_DESC baseDesc;
	XGGetTextureDesc( info.m_pTexture, 0, &baseDesc );
	bool bDstIsTiled = XGIsTiledFormat( baseDesc.Format ) == TRUE;

	// get the target mip level info
	XGTEXTURE_DESC mipDesc;
	XGGetTextureDesc( info.m_pTexture, info.m_nLevel, &mipDesc );
	bool bFullSurfBlit = ( mipDesc.Width == (unsigned)info.m_nWidth && mipDesc.Height == (unsigned)info.m_nHeight );

	// get the mip level of the texture we want to write into
	IDirect3DSurface* pTextureLevel;
	HRESULT hr = GetSurfaceFromTexture( info.m_pTexture, info.m_nLevel, info.m_CubeFaceID, &pTextureLevel );
	if ( FAILED( hr ) )
	{
		Warning( "CShaderAPIDX8::BlitTextureBits: GetSurfaceFromTexture() failure\n" );
		return;
	}

	CUtlBuffer scratchMemory;
	D3DLOCKED_RECT lockedRect;

	hr = pTextureLevel->LockRect( &lockedRect, NULL, D3DLOCK_NOSYSLOCK );
	if ( FAILED( hr ) )
	{
		Warning( "CShaderAPIDX8::BlitTextureBits: couldn't lock texture rect\n" );
		goto cleanUp;
	}
	unsigned char *pTargetImage = (unsigned char *)lockedRect.pBits;

	POINT p;
	p.x = xOffset;
	p.y = yOffset;

	RECT r;
	r.left = 0;
	r.top = 0;
	r.right = info.m_nWidth;
	r.bottom = info.m_nHeight;

	int blockSize = mipDesc.Width/mipDesc.WidthInBlocks;
	if ( !srcStride )
	{
		srcStride = (mipDesc.Width/blockSize)*mipDesc.BytesPerBlock;
	}

	// subrect blitting path
	if ( !bDstIsTiled )
	{
		// Copy the subrect without conversion
		hr = XGCopySurface(
				pTargetImage,
				mipDesc.RowPitch,
				mipDesc.Width,
				mipDesc.Height,
				mipDesc.Format,
				&p,
				pSrcData,
				srcStride,
				mipDesc.Format,
				&r,
				0,
				0 );
		if ( FAILED( hr ) )
		{
			Warning( "CShaderAPIDX8::BlitTextureBits: failed subrect copy\n" );
			goto cleanUp;
		}
	}
	else
	{
		int tileFlags = 0;
		if ( !( mipDesc.Flags & XGTDESC_PACKED ) )
			tileFlags |= XGTILE_NONPACKED;
		if ( mipDesc.Flags & XGTDESC_BORDERED )
			tileFlags |= XGTILE_BORDER;

		// tile the temp store back into the target surface
		XGTileTextureLevel(
			baseDesc.Width,
			baseDesc.Height,
			info.m_nLevel,
			XGGetGpuFormat( baseDesc.Format ),
			tileFlags,
			pTargetImage,
			&p,
			pSrcData,
			srcStride,
			&r );
	}

	hr = pTextureLevel->UnlockRect();
	if ( FAILED( hr ) ) 
	{
		Warning( "CShaderAPIDX8::BlitTextureBits: couldn't unlock texture rect\n" );
		goto cleanUp;
	}

cleanUp:
	pTextureLevel->Release();
}
#endif

//-----------------------------------------------------------------------------
// Blit in bits
//-----------------------------------------------------------------------------
#if !defined( _X360 )
static void BlitVolumeBits( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
	D3DBOX srcBox;
	D3DLOCKED_BOX lockedBox;
	srcBox.Left = xOffset;
	srcBox.Right = xOffset + info.m_nWidth;
	srcBox.Top = yOffset;
	srcBox.Bottom = yOffset + info.m_nHeight;
	srcBox.Front = info.m_nZOffset;
	srcBox.Back = info.m_nZOffset + 1;

#ifndef RECORD_TEXTURES
	RECORD_COMMAND( DX8_LOCK_TEXTURE, 6 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
	RECORD_STRUCT( &srcRect, sizeof(srcRect) );
	RECORD_INT( D3DLOCK_NOSYSLOCK );
#endif

	IDirect3DVolumeTexture *pVolumeTexture = static_cast<IDirect3DVolumeTexture*>( info.m_pTexture );
	if ( FAILED( pVolumeTexture->LockBox( info.m_nLevel, &lockedBox, &srcBox, D3DLOCK_NOSYSLOCK ) ) )
	{
		Warning( "BlitVolumeBits: couldn't lock volume texture rect\n" );
		return;
	}

	// garymcthack : need to make a recording command for this.
	ImageFormat dstFormat = GetImageFormat( info.m_pTexture );
	unsigned char *pImage = (unsigned char *)lockedBox.pBits;
	ShaderUtil()->ConvertImageFormat( info.m_pSrcData, info.m_SrcFormat,
						pImage, dstFormat, info.m_nWidth, info.m_nHeight, srcStride, lockedBox.RowPitch );

#ifndef RECORD_TEXTURES
	RECORD_COMMAND( DX8_UNLOCK_TEXTURE, 4 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
#endif

	if ( FAILED( pVolumeTexture->UnlockBox( info.m_nLevel ) ) ) 
	{
		Warning( "BlitVolumeBits: couldn't unlock volume texture rect\n" );
		return;
	}
}
#endif

//-----------------------------------------------------------------------------
// Puts 3D texture data into 360 gpu memory.
// Does not support any subvolume or slice blitting.
//-----------------------------------------------------------------------------
#if defined( _X360 )
static void BlitVolumeBits( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
	if ( xOffset || yOffset || info.m_nZOffset || srcStride )
	{
		// not supporting any subvolume blitting
		// the entire volume per mip must be blitted
		Assert( 0 );
		return;
	}

	ImageFormat	dstFormat = GetImageFormat( info.m_pTexture );
	if ( dstFormat != info.m_SrcFormat )
	{
		// texture is expected to be in target format
		// not supporting conversion
		Assert( 0 );
		return;
	}

	// get the top mip level info (needed for proper sub mip access)
	XGTEXTURE_DESC baseDesc;
	XGGetTextureDesc( info.m_pTexture, 0, &baseDesc );
	bool bDstIsTiled = XGIsTiledFormat( baseDesc.Format ) == TRUE;
	if ( info.m_bSrcIsTiled && !bDstIsTiled )
	{
		// not supporting a tiled source into an untiled target
		Assert( 0 );
		return;
	}

	// get the mip level info
	XGTEXTURE_DESC mipDesc;
	XGGetTextureDesc( info.m_pTexture, info.m_nLevel, &mipDesc );
	bool bFullSurfBlit = ( mipDesc.Width == (unsigned int)info.m_nWidth && mipDesc.Height == (unsigned int)info.m_nHeight );

	if ( !bFullSurfBlit )
	{
		// not supporting subrect blitting
		Assert( 0 );
		return;
	}

	D3DLOCKED_BOX lockedBox;

	// get the mip level of the volume we want to write into
	IDirect3DVolumeTexture *pVolumeTexture = static_cast<IDirect3DVolumeTexture*>( info.m_pTexture );
	HRESULT hr = pVolumeTexture->LockBox( info.m_nLevel, &lockedBox, NULL, D3DLOCK_NOSYSLOCK );
	if ( FAILED( hr ) )
	{
		Warning( "CShaderAPIDX8::BlitVolumeBits: Couldn't lock volume box\n" );
		return;
	}

	unsigned char *pSrcData = info.m_pSrcData;
	unsigned char *pTargetImage = (unsigned char *)lockedBox.pBits;

	int tileFlags = 0;
	if ( !( mipDesc.Flags & XGTDESC_PACKED ) )
		tileFlags |= XGTILE_NONPACKED;
	if ( mipDesc.Flags & XGTDESC_BORDERED )
		tileFlags |= XGTILE_BORDER;

	if ( !info.m_bSrcIsTiled && bDstIsTiled )
	{
		// tile the source directly into the target surface
		XGTileVolumeTextureLevel(
			baseDesc.Width,
			baseDesc.Height,
			baseDesc.Depth,
			info.m_nLevel,
			XGGetGpuFormat( baseDesc.Format ),
			tileFlags,
			pTargetImage,
			NULL,
			pSrcData,
			mipDesc.RowPitch,
			mipDesc.SlicePitch,
			NULL );
	}
	else if ( !info.m_bSrcIsTiled && !bDstIsTiled )
	{
		// not implemented yet
		Assert( 0 );
	}
	else
	{
		// not implemented yet
		Assert( 0 );
	}

	hr = pVolumeTexture->UnlockBox( info.m_nLevel );
	if ( FAILED( hr ) )
	{
		Warning( "CShaderAPIDX8::BlitVolumeBits: couldn't unlock volume box\n" );
		return;
	}
}
#endif

// FIXME: How do I blit from D3DPOOL_SYSTEMMEM to D3DPOOL_MANAGED?  I used to use CopyRects for this.  UpdateSurface doesn't work because it can't blit to anything besides D3DPOOL_DEFAULT.
// We use this only in the case where we need to create a < 4x4 miplevel for a compressed texture.  We end up creating a 4x4 system memory texture, and blitting it into the proper miplevel.
// 6) LockRects should be used for copying between SYSTEMMEM and
// MANAGED.  For such a small copy, you'd avoid a significant 
// amount of overhead from the old CopyRects code.  Ideally, you 
// should just lock the bottom of MANAGED and generate your 
// sub-4x4 data there.
	
// NOTE: IF YOU CHANGE THIS, CHANGE THE VERSION IN PLAYBACK.CPP!!!!
static void BlitTextureBits( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
#ifdef RECORD_TEXTURES
	RECORD_COMMAND( DX8_BLIT_TEXTURE_BITS, 14 );
	RECORD_INT( info.m_TextureHandle );
	RECORD_INT( info.m_nCopy );
	RECORD_INT( info.m_nLevel );
	RECORD_INT( info.m_CubeFaceID );
	RECORD_INT( xOffset );
	RECORD_INT( yOffset );
	RECORD_INT( info.m_nZOffset );
	RECORD_INT( info.m_nWidth );
	RECORD_INT( info.m_nHeight );
	RECORD_INT( info.m_SrcFormat );
	RECORD_INT( srcStride );
	RECORD_INT( GetImageFormat( info.m_pTexture ) );
	// strides are in bytes.
	int srcDataSize;
	if ( srcStride == 0 )
	{
		srcDataSize = ImageLoader::GetMemRequired( info.m_nWidth, info.m_nHeight, 1, info.m_SrcFormat, false );
	}
	else
	{
		srcDataSize = srcStride * info.m_nHeight;
	}
	RECORD_INT( srcDataSize );
	RECORD_STRUCT( info.m_pSrcData, srcDataSize );
#endif // RECORD_TEXTURES
	
	if ( !IsVolumeTexture( info.m_pTexture ) )
	{
		Assert( info.m_nZOffset == 0 );
		BlitSurfaceBits( info, xOffset, yOffset, srcStride );
	}
	else
	{
		BlitVolumeBits( info, xOffset, yOffset, srcStride );
	}
}

//-----------------------------------------------------------------------------
// Texture image upload
//-----------------------------------------------------------------------------
void LoadTexture( TextureLoadInfo_t &info )
{
	MEM_ALLOC_D3D_CREDIT();

	Assert( info.m_pSrcData );
	Assert( info.m_pTexture );

#ifdef _DEBUG
	ImageFormat format = GetImageFormat( info.m_pTexture );
	Assert( (format != -1) && (format == FindNearestSupportedFormat( format, false, false, false )) );
#endif

	// Copy in the bits...
	BlitTextureBits( info, 0, 0, 0 );
}

void LoadVolumeTextureFromVTF( TextureLoadInfo_t &info, IVTFTexture* pVTF, int iVTFFrame )
{
	if ( !info.m_pTexture || info.m_pTexture->GetType() != D3DRTYPE_VOLUMETEXTURE )
	{
		Assert( 0 );
		return;
	}

	IDirect3DVolumeTexture9 *pVolTex = static_cast<IDirect3DVolumeTexture*>( info.m_pTexture );

	D3DVOLUME_DESC desc;
	if ( pVolTex->GetLevelDesc( 0, &desc ) != S_OK )
	{
		Warning( "LoadVolumeTextureFromVTF: couldn't get texture level description\n" );
		return;
	}

	int iMipCount = pVolTex->GetLevelCount();
	if ( pVTF->Depth() != (int)desc.Depth || pVTF->Width() != (int)desc.Width || pVTF->Height() != (int)desc.Height || pVTF->MipCount() < iMipCount )
	{
		Warning( "LoadVolumeTextureFromVTF: VTF dimensions do not match texture\n" );
		return;
	}

	TextureLoadInfo_t sliceInfo = info;

#if !defined( _X360 ) && !defined( DX_TO_GL_ABSTRACTION )
	IDirect3DVolumeTexture9 *pStagingTexture = NULL;
	if ( !info.m_bTextureIsLockable )
	{
		IDirect3DVolumeTexture9 *pTemp;
		if ( Dx9Device()->CreateVolumeTexture( desc.Width, desc.Height, desc.Depth, iMipCount, 0, desc.Format, D3DPOOL_SYSTEMMEM, &pTemp, NULL ) != S_OK )
		{
			Warning( "LoadVolumeTextureFromVTF: failed to create temporary staging texture\n" );
			return;
		}
		sliceInfo.m_pTexture = static_cast<IDirect3DBaseTexture*>( pTemp );
		sliceInfo.m_bTextureIsLockable = true;
		pStagingTexture = pTemp;
	}
#endif

	for ( int iMip = 0; iMip < iMipCount; ++iMip )
	{
		int w, h, d;
		pVTF->ComputeMipLevelDimensions( iMip, &w, &h, &d );
		sliceInfo.m_nLevel = iMip;
		sliceInfo.m_nWidth = w;
		sliceInfo.m_nHeight = h;
		for ( int iSlice = 0; iSlice < d; ++iSlice )
		{
			sliceInfo.m_nZOffset = iSlice;
			sliceInfo.m_pSrcData = pVTF->ImageData( iVTFFrame, 0, iMip, 0, 0, iSlice );
			BlitTextureBits( sliceInfo, 0, 0, 0 );
		}
	}

#if !defined( _X360 ) && !defined( DX_TO_GL_ABSTRACTION )
	if ( pStagingTexture )
	{
		if ( Dx9Device()->UpdateTexture( pStagingTexture, pVolTex ) != S_OK )
		{
			Warning( "LoadVolumeTextureFromVTF: volume UpdateTexture failed\n" );
		}
		pStagingTexture->Release();
	}
#endif
}

void LoadCubeTextureFromVTF( TextureLoadInfo_t &info, IVTFTexture* pVTF, int iVTFFrame )
{
	if ( !info.m_pTexture || info.m_pTexture->GetType() != D3DRTYPE_CUBETEXTURE )
	{
		Assert( 0 );
		return;
	}

	IDirect3DCubeTexture9 *pCubeTex = static_cast<IDirect3DCubeTexture9*>( info.m_pTexture );

	D3DSURFACE_DESC desc;
	if ( pCubeTex->GetLevelDesc( 0, &desc ) != S_OK )
	{
		Warning( "LoadCubeTextureFromVTF: couldn't get texture level description\n" );
		return;
	}

	int iMipCount = pCubeTex->GetLevelCount();
	if ( pVTF->Depth() != 1 || pVTF->Width() != (int)desc.Width || pVTF->Height() != (int)desc.Height || pVTF->FaceCount() < 6 || pVTF->MipCount() < iMipCount )
	{
		Warning( "LoadCubeTextureFromVTF: VTF dimensions do not match texture\n" );
		return;
	}

	TextureLoadInfo_t faceInfo = info;

#if !defined( _X360 ) && !defined( DX_TO_GL_ABSTRACTION )
	IDirect3DCubeTexture9 *pStagingTexture = NULL;
	if ( !info.m_bTextureIsLockable )
	{
		IDirect3DCubeTexture9 *pTemp;
		if ( Dx9Device()->CreateCubeTexture( desc.Width, iMipCount, 0, desc.Format, D3DPOOL_SYSTEMMEM, &pTemp, NULL ) != S_OK )
		{
			Warning( "LoadCubeTextureFromVTF: failed to create temporary staging texture\n" );
			return;
		}
		faceInfo.m_pTexture = static_cast<IDirect3DBaseTexture*>( pTemp );
		faceInfo.m_bTextureIsLockable = true;
		pStagingTexture = pTemp;
	}
#endif

	for ( int iMip = 0; iMip < iMipCount; ++iMip )
	{
		int w, h, d;
		pVTF->ComputeMipLevelDimensions( iMip, &w, &h, &d );
		faceInfo.m_nLevel = iMip;
		faceInfo.m_nWidth = w;
		faceInfo.m_nHeight = h;
		for ( int iFace = 0; iFace < 6; ++iFace )
		{
			faceInfo.m_CubeFaceID = (D3DCUBEMAP_FACES) iFace;
			faceInfo.m_pSrcData = pVTF->ImageData( iVTFFrame, iFace, iMip );
			BlitTextureBits( faceInfo, 0, 0, 0 );
		}
	}

#if !defined( _X360 ) && !defined( DX_TO_GL_ABSTRACTION )
	if ( pStagingTexture )
	{
		if ( Dx9Device()->UpdateTexture( pStagingTexture, pCubeTex ) != S_OK )
		{
			Warning( "LoadCubeTextureFromVTF: cube UpdateTexture failed\n" );
		}
		pStagingTexture->Release();
	}
#endif
}

void LoadTextureFromVTF( TextureLoadInfo_t &info, IVTFTexture* pVTF, int iVTFFrame )
{
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL0 );

	if ( !info.m_pTexture || info.m_pTexture->GetType() != D3DRTYPE_TEXTURE )
	{
		Assert( 0 );
		return;
	}

	IDirect3DTexture9 *pTex = static_cast<IDirect3DTexture9*>( info.m_pTexture );

	D3DSURFACE_DESC desc;
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - GetLevelDesc", __FUNCTION__ );
		if ( pTex->GetLevelDesc( 0, &desc ) != S_OK )
		{
			Warning( "LoadTextureFromVTF: couldn't get texture level description\n" );
			return;
		}
	}

	int iMipCount = pTex->GetLevelCount();
	if ( pVTF->Depth() != 1 || pVTF->Width() != (int)desc.Width || pVTF->Height() != (int)desc.Height || pVTF->MipCount() < iMipCount || pVTF->FaceCount() <= (int)info.m_CubeFaceID )
	{
		Warning( "LoadTextureFromVTF: VTF dimensions do not match texture\n" );
		return;
	}

	// Info may have a cube face ID if we are falling back to 2D sphere map support
	TextureLoadInfo_t mipInfo = info;
	int iVTFFaceNum = info.m_CubeFaceID;
	mipInfo.m_CubeFaceID = (D3DCUBEMAP_FACES)0;

#if !defined( _X360 ) && !defined( DX_TO_GL_ABSTRACTION )
	// If blitting more than one mip level of an unlockable texture, create a temporary
	// texture for all mip levels only call UpdateTexture once. For textures with
	// only a single mip level, fall back on the support in BlitSurfaceBits. -henryg
	IDirect3DTexture9 *pStagingTexture = NULL;
	if ( !info.m_bTextureIsLockable && iMipCount > 1 )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - CreateSysmemTexture", __FUNCTION__ );
		
		IDirect3DTexture9 *pTemp;
		if ( Dx9Device()->CreateTexture( desc.Width, desc.Height, iMipCount, 0, desc.Format, D3DPOOL_SYSTEMMEM, &pTemp, NULL ) != S_OK )
		{
			Warning( "LoadTextureFromVTF: failed to create temporary staging texture\n" );
			return;
		}

		mipInfo.m_pTexture = static_cast<IDirect3DBaseTexture*>( pTemp );
		mipInfo.m_bTextureIsLockable = true;
		pStagingTexture = pTemp;
	}
#endif

	// Get the clamped resolutions from the VTF, then apply any clamping we've done from the higher level code.
	// (For example, we chop off the bottom of the mipmap pyramid at 32x32--that is reflected in iMipCount, so 
	// honor that here).
	int finest = 0, coarsest = 0;
	pVTF->GetMipmapRange( &finest, &coarsest );
	finest = Min( finest, iMipCount - 1 );
	coarsest = Min( coarsest, iMipCount - 1 );
	Assert( finest <= coarsest && coarsest < iMipCount );

	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - BlitTextureBits", __FUNCTION__ );
		for ( int iMip = finest; iMip <= coarsest; ++iMip )
		{
			int w, h, d;
			pVTF->ComputeMipLevelDimensions( iMip, &w, &h, &d );
			mipInfo.m_nLevel = iMip;
			mipInfo.m_nWidth = w;
			mipInfo.m_nHeight = h;
			mipInfo.m_pSrcData = pVTF->ImageData( iVTFFrame, iVTFFaceNum, iMip );

			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - BlitTextureBits - %d", __FUNCTION__, iMip );

			BlitTextureBits( mipInfo, 0, 0, 0 );
		}
	}

#if !defined( _X360 ) && !defined( DX_TO_GL_ABSTRACTION )
	if ( pStagingTexture )
	{
		if ( ( coarsest - finest + 1 ) == iMipCount )
		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - UpdateTexture", __FUNCTION__ );
			if ( Dx9Device()->UpdateTexture( pStagingTexture, pTex ) != S_OK )
			{
				Warning( "LoadTextureFromVTF: UpdateTexture failed\n" );
			}
		}
		else
		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - UpdateSurface", __FUNCTION__ );

			for ( int mip = finest; mip <= coarsest; ++mip )
			{
				tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - UpdateSurface - %d", __FUNCTION__, mip );

				IDirect3DSurface9 *pSrcSurf = NULL, 
					              *pDstSurf = NULL;

				if ( pStagingTexture->GetSurfaceLevel( mip, &pSrcSurf ) != S_OK )
					Warning( "LoadTextureFromVTF: couldn't get surface level %d for system surface\n", mip );

				if ( pTex->GetSurfaceLevel( mip, &pDstSurf ) != S_OK )
					Warning( "LoadTextureFromVTF: couldn't get surface level %d for dest surface\n", mip );

				{
					tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - UpdateSurface - Call ", __FUNCTION__, mip );
					if ( !pSrcSurf || !pDstSurf || Dx9Device()->UpdateSurface( pSrcSurf, NULL, pDstSurf, NULL ) != S_OK ) 
						Warning( "LoadTextureFromVTF: surface update failed.\n" );
				}

				if ( pSrcSurf )
					pSrcSurf->Release();

				if ( pDstSurf )
					pDstSurf->Release();
			}
		}

		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - Cleanup", __FUNCTION__ );
		pStagingTexture->Release();
	}
#endif
}


//-----------------------------------------------------------------------------
// Upload to a sub-piece of a texture
//-----------------------------------------------------------------------------
void LoadSubTexture( TextureLoadInfo_t &info, int xOffset, int yOffset, int srcStride )
{
	Assert( info.m_pSrcData );
	Assert( info.m_pTexture );

#if defined( _X360 )
	// xboxissue - not supporting subrect swizzling
	Assert( !info.m_bSrcIsTiled );
#endif

#ifdef _DEBUG
	ImageFormat format = GetImageFormat( info.m_pTexture );
	Assert( (format == FindNearestSupportedFormat(format, false, false, false )) && (format != -1) );
#endif

	// Copy in the bits...
	BlitTextureBits( info, xOffset, yOffset, srcStride );
}


//-----------------------------------------------------------------------------
// Returns the size of texture memory, in MB
//-----------------------------------------------------------------------------
// Helps with startup time.. we don't use the texture memory size for anything anyways
#define DONT_CHECK_MEM

int ComputeTextureMemorySize( const GUID &nDeviceGUID, D3DDEVTYPE deviceType )
{
#if defined( _X360 )
	return 0;
#elif defined( DONT_CHECK_MEM )
	return (deviceType == D3DDEVTYPE_REF) ? (64 * 1024 * 1024) : 102236160;
#else

	FileHandle_t file = g_pFullFileSystem->Open( "vidcfg.bin", "rb", "EXECUTABLE_PATH" );
	if ( file )
	{
		GUID deviceId;
		int texSize;
		g_pFullFileSystem->Read( &deviceId, sizeof(deviceId), file );
		g_pFullFileSystem->Read( &texSize, sizeof(texSize), file );
		g_pFullFileSystem->Close( file );
		if ( nDeviceGUID == deviceId )
		{
			return texSize;
		}
	}
	// How much texture memory?
	if (deviceType == D3DDEVTYPE_REF)
		return 64 * 1024 * 1024;

	// Sadly, the only way to compute texture memory size
	// is to allocate a crapload of textures until we can't any more
	ImageFormat fmt = FindNearestSupportedFormat( IMAGE_FORMAT_BGR565, false, false, false );
	int textureSize = ShaderUtil()->GetMemRequired( 256, 256, 1, fmt, false );

	int totalSize = 0;
	CUtlVector< IDirect3DBaseTexture* > textures;

	s_bTestingVideoMemorySize = true;
	while (true)
	{
		RECORD_COMMAND( DX8_CREATE_TEXTURE, 7 );
		RECORD_INT( textures.Count() );
		RECORD_INT( 256 );
		RECORD_INT( 256 );
		RECORD_INT( ImageLoader::ImageFormatToD3DFormat(fmt) );
		RECORD_INT( 1 );
		RECORD_INT( false );
		RECORD_INT( 1 );

		IDirect3DBaseTexture* pTex = CreateD3DTexture( 256, 256, 1, fmt, 1, 0 );
		if (!pTex)
			break;
		totalSize += textureSize;

		textures.AddToTail( pTex );
	} 
	s_bTestingVideoMemorySize = false;

	// Free all the temp textures
	for (int i = textures.Size(); --i >= 0; )
	{
		RECORD_COMMAND( DX8_DESTROY_TEXTURE, 1 );
		RECORD_INT( i );

		DestroyD3DTexture( textures[i] );
	}

	file = g_pFullFileSystem->Open( "vidcfg.bin", "wb", "EXECUTABLE_PATH" );
	if ( file )
	{
		g_pFullFileSystem->Write( &nDeviceGUID, sizeof(GUID), file );
		g_pFullFileSystem->Write( &totalSize, sizeof(totalSize), file );
		g_pFullFileSystem->Close( file );
	}

	return totalSize;
#endif
}
