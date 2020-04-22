//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#define DISABLE_PROTECTED_THINGS
#include "togl/rendermechanism.h"
#include "locald3dtypes.h"
#include "colorformatdx8.h"
#include "shaderapidx8_global.h"
#include "bitmap/imageformat.h"
#include "shaderapi/ishaderutil.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "shaderdevicedx8.h"


// Must be last
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Figures out what texture formats we support
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------

// Texture formats supported by DX driver[vertextexture][render target][non filterable]
static D3DFORMAT	g_D3DColorFormat[NUM_IMAGE_FORMATS][2][2][2];
static UINT			g_DisplayAdapter;
static D3DDEVTYPE	g_DeviceType;
static ImageFormat	g_DeviceFormat;
static bool			g_bSupportsD24S8;
static bool			g_bSupportsD24X8;
static bool			g_bSupportsD16;
static bool			g_bSupportsD24X4S4;
static bool			g_bSupportsD15S1;

//-----------------------------------------------------------------------------
// Determines what formats we actually *do* support
//-----------------------------------------------------------------------------
static bool TestTextureFormat( D3DFORMAT format, bool bIsRenderTarget, 
							   bool bIsVertexTexture, bool bIsFilterableRequired ) 
{
	int nUsage = bIsRenderTarget ? D3DUSAGE_RENDERTARGET : 0;
	if ( bIsVertexTexture )
	{
		// vertex textures never need filtering
		nUsage |= D3DUSAGE_QUERY_VERTEXTEXTURE;
	}
	if ( bIsFilterableRequired )
	{
		nUsage |= D3DUSAGE_QUERY_FILTER;
	}

	HRESULT hr;

	// IHV depth texture formats require a slightly different check...
	if ( !IsX360() && bIsRenderTarget && ( ( format == NVFMT_RAWZ ) || ( format == NVFMT_INTZ   ) ||
										   ( format == D3DFMT_D16 ) || ( format == D3DFMT_D24S8 ) ||
										   ( format == ATIFMT_D16 ) || ( format == ATIFMT_D24S8 ) ) )
	{
		hr = D3D()->CheckDeviceFormat(
			g_DisplayAdapter, g_DeviceType, ImageLoader::ImageFormatToD3DFormat( g_DeviceFormat ),
			D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, format );
	}
	else if ( !IsX360() || !bIsRenderTarget )
	{
		// See if we can do it!
		hr = D3D()->CheckDeviceFormat( 
			g_DisplayAdapter, g_DeviceType, ImageLoader::ImageFormatToD3DFormat( g_DeviceFormat ),
			nUsage, D3DRTYPE_TEXTURE, format );
	}
	else // 360
	{
		// 360 can only validate render targets as surface display format
		hr = D3D()->CheckDeviceFormat( g_DisplayAdapter, g_DeviceType, format, 0, D3DRTYPE_SURFACE, format );
	}

    return SUCCEEDED( hr );
}

D3DFORMAT GetNearestD3DColorFormat( ImageFormat fmt,
									bool isRenderTarget, bool bIsVertexTexture,
									bool bIsFilterableRequired)
{
	switch(fmt)
	{
	case IMAGE_FORMAT_RGBA8888:
	case IMAGE_FORMAT_ABGR8888:
	case IMAGE_FORMAT_ARGB8888:
	case IMAGE_FORMAT_BGRA8888:
		if (TestTextureFormat(D3DFMT_A8R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A8R8G8B8;
		if (TestTextureFormat(D3DFMT_A4R4G4B4, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A4R4G4B4;
		break;

#if defined( _X360 )
	case IMAGE_FORMAT_LINEAR_RGBA8888:
	case IMAGE_FORMAT_LINEAR_ABGR8888:
	case IMAGE_FORMAT_LINEAR_ARGB8888:
	case IMAGE_FORMAT_LINEAR_BGRA8888:
		// same as above - all xxxx8888 RGBA ordering funnels to d3d a8r8g8b8
		if ( TestTextureFormat( D3DFMT_LIN_A8R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ) )
			return D3DFMT_LIN_A8R8G8B8;
		break;
#endif

#if defined( _X360 )
	case IMAGE_FORMAT_LINEAR_BGRX8888:
		if ( TestTextureFormat( D3DFMT_LIN_X8R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ) )
			return D3DFMT_LIN_X8R8G8B8;
		break;
#endif

	case IMAGE_FORMAT_BGRX8888:
		// We want this format to return exactly it's equivalent so that
		// when we create render targets to blit to from the framebuffer,
		// the CopyRect won't fail due to format mismatches.
		if (TestTextureFormat(D3DFMT_X8R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_X8R8G8B8;

		// fall through. . . .
	case IMAGE_FORMAT_RGB888:
	case IMAGE_FORMAT_BGR888:
#if !defined( _X360 )
		if (TestTextureFormat(D3DFMT_R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_R8G8B8;
#endif
		if (TestTextureFormat(D3DFMT_X8R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_X8R8G8B8;
		if (TestTextureFormat(D3DFMT_A8R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A8R8G8B8;
		if (TestTextureFormat(D3DFMT_R5G6B5, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_R5G6B5;
		if (TestTextureFormat(D3DFMT_X1R5G5B5, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_X1R5G5B5;
		if (TestTextureFormat(D3DFMT_A1R5G5B5, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A1R5G5B5;
		break;

	case IMAGE_FORMAT_BGR565:
	case IMAGE_FORMAT_RGB565:
		if (TestTextureFormat(D3DFMT_R5G6B5, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_R5G6B5;
		if (TestTextureFormat(D3DFMT_X1R5G5B5, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_X1R5G5B5;
		if (TestTextureFormat(D3DFMT_A1R5G5B5, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A1R5G5B5;
#if !defined( _X360 )
		if (TestTextureFormat(D3DFMT_R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_R8G8B8;
#endif
		if (TestTextureFormat(D3DFMT_X8R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_X8R8G8B8;
		if (TestTextureFormat(D3DFMT_A8R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A8R8G8B8;
		break;

	case IMAGE_FORMAT_BGRX5551:
		if (TestTextureFormat(D3DFMT_X1R5G5B5, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_X1R5G5B5;
		if (TestTextureFormat(D3DFMT_A1R5G5B5, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A1R5G5B5;
		if (TestTextureFormat(D3DFMT_R5G6B5, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_R5G6B5;
#if !defined( _X360 )
		if (TestTextureFormat(D3DFMT_R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_R8G8B8;
#endif
		if (TestTextureFormat(D3DFMT_X8R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_X8R8G8B8;
		if (TestTextureFormat(D3DFMT_A8R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A8R8G8B8;
		break;

#if defined( _X360 )
	case IMAGE_FORMAT_LINEAR_BGRX5551:
		if ( TestTextureFormat( D3DFMT_LIN_X1R5G5B5, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ) )
			return D3DFMT_LIN_X1R5G5B5;
		break;
#endif

	case IMAGE_FORMAT_BGRA5551:
		if (TestTextureFormat(D3DFMT_A1R5G5B5, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A1R5G5B5;
		if (TestTextureFormat(D3DFMT_A4R4G4B4, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A4R4G4B4;
		if (TestTextureFormat(D3DFMT_A8R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A8R8G8B8;
		break;

	case IMAGE_FORMAT_BGRA4444:
		if (TestTextureFormat(D3DFMT_A4R4G4B4, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A4R4G4B4;
		if (TestTextureFormat(D3DFMT_A8R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A8R8G8B8;
		break;

	case IMAGE_FORMAT_I8:
		if (TestTextureFormat(D3DFMT_L8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_L8;
		if (TestTextureFormat(D3DFMT_A8R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A8R8G8B8;
		break;

#if defined( _X360 )
	case IMAGE_FORMAT_LINEAR_I8:
		if ( TestTextureFormat( D3DFMT_LIN_L8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ) )
			return D3DFMT_LIN_L8;
		break;
#endif

	case IMAGE_FORMAT_IA88:
		if (TestTextureFormat(D3DFMT_A8L8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A8L8;
		if (TestTextureFormat(D3DFMT_A8R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A8R8G8B8;
		break;

	case IMAGE_FORMAT_A8:
		if (TestTextureFormat(D3DFMT_A8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A8;
		if (TestTextureFormat(D3DFMT_A8R8G8B8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_A8R8G8B8;
		break;

	case IMAGE_FORMAT_DXT1:
	case IMAGE_FORMAT_DXT1_ONEBITALPHA:
	case IMAGE_FORMAT_DXT1_RUNTIME:
		if (TestTextureFormat(D3DFMT_DXT1, isRenderTarget, bIsVertexTexture, bIsFilterableRequired))
			return D3DFMT_DXT1;
		break;

	case IMAGE_FORMAT_DXT3:
		if (TestTextureFormat(D3DFMT_DXT3, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ))
			return D3DFMT_DXT3;
		break;

	case IMAGE_FORMAT_DXT5:
	case IMAGE_FORMAT_DXT5_RUNTIME:
		if (TestTextureFormat(D3DFMT_DXT5, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ))
			return D3DFMT_DXT5;
		break;

	case IMAGE_FORMAT_UV88:
		if (TestTextureFormat(D3DFMT_V8U8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ))
			return D3DFMT_V8U8;
		break;

	case IMAGE_FORMAT_UVWQ8888:
		if (TestTextureFormat(D3DFMT_Q8W8V8U8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ))
			return D3DFMT_Q8W8V8U8;
		break;

	case IMAGE_FORMAT_UVLX8888:
		if (TestTextureFormat(D3DFMT_X8L8V8U8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ))
			return D3DFMT_X8L8V8U8;
		break;

	case IMAGE_FORMAT_RGBA16161616F:
		if ( TestTextureFormat( D3DFMT_A16B16G16R16F, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ) )
			return D3DFMT_A16B16G16R16F;
		if ( TestTextureFormat( D3DFMT_A16B16G16R16, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ) )
			return D3DFMT_A16B16G16R16;
		break;

	case IMAGE_FORMAT_RGBA16161616:
		if ( TestTextureFormat( D3DFMT_A16B16G16R16, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ) )
			return D3DFMT_A16B16G16R16;
		if ( TestTextureFormat( D3DFMT_A16B16G16R16F, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ) )
			return D3DFMT_A16B16G16R16F;
		break;

#if defined( _X360 )
	case IMAGE_FORMAT_LINEAR_RGBA16161616:
		if ( TestTextureFormat( D3DFMT_LIN_A16B16G16R16, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ) )
			return D3DFMT_LIN_A16B16G16R16;
		break;
#endif

	case IMAGE_FORMAT_R32F:
		if ( TestTextureFormat( D3DFMT_R32F, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ) )
			return D3DFMT_R32F;
		break;

	case IMAGE_FORMAT_RGBA32323232F:
		if ( TestTextureFormat( D3DFMT_A32B32G32R32F, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ) )
			return D3DFMT_A32B32G32R32F;
		break;

#if defined( _X360 )
	case IMAGE_FORMAT_X360_DST16:
		return D3DFMT_D16;

	case IMAGE_FORMAT_X360_DST24:
		return D3DFMT_D24S8;

	case IMAGE_FORMAT_X360_DST24F:
		return D3DFMT_D24FS8;

	case IMAGE_FORMAT_LE_BGRX8888:
		return D3DFMT_LE_X8R8G8B8;

	case IMAGE_FORMAT_LE_BGRA8888:
		return D3DFMT_LE_A8R8G8B8;
#endif

	// nVidia overloads DST formats as texture formats
	case IMAGE_FORMAT_NV_DST16:
		if ( TestTextureFormat( D3DFMT_D16, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ) )
			return D3DFMT_D16;
		break;

	case IMAGE_FORMAT_NV_DST24:
		if ( TestTextureFormat( D3DFMT_D24S8, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ) )
			return D3DFMT_D24S8;
		break;
	case IMAGE_FORMAT_NV_INTZ:
		if ( TestTextureFormat( NVFMT_INTZ, isRenderTarget, bIsVertexTexture, false ) )
			return NVFMT_INTZ;
		break;

	case IMAGE_FORMAT_NV_RAWZ:
		if ( TestTextureFormat( NVFMT_RAWZ, isRenderTarget, bIsVertexTexture, false ) )
			return NVFMT_RAWZ;
		break;

	case IMAGE_FORMAT_NV_NULL:
		if ( TestTextureFormat( NVFMT_NULL, isRenderTarget, bIsVertexTexture, false ) )
			return NVFMT_NULL;
		break;

	case IMAGE_FORMAT_ATI_DST16:
		if ( TestTextureFormat( ATIFMT_D16, isRenderTarget, bIsVertexTexture, false ) )
			return ATIFMT_D16;
		break;

	case IMAGE_FORMAT_ATI_DST24:
		if ( TestTextureFormat( ATIFMT_D24S8, isRenderTarget, bIsVertexTexture, false ) )
			return ATIFMT_D24S8;
		break;

	case IMAGE_FORMAT_ATI2N:
		if ( TestTextureFormat( ATIFMT_ATI2N, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ) )
			return ATIFMT_ATI2N;
		break;

	case IMAGE_FORMAT_ATI1N:
		if ( TestTextureFormat( ATIFMT_ATI1N, isRenderTarget, bIsVertexTexture, bIsFilterableRequired ) )
			return ATIFMT_ATI1N;
		break;
	}

	return D3DFMT_UNKNOWN;
}

void InitializeColorInformation( UINT displayAdapter, D3DDEVTYPE deviceType, 
								 ImageFormat displayFormat )
{
	g_DisplayAdapter = displayAdapter;
	g_DeviceType = deviceType;
	g_DeviceFormat = displayFormat;

	int fmt = 0;
	while ( fmt < NUM_IMAGE_FORMATS )
	{
		for ( int nVertexTexture = 0; nVertexTexture <= 1; ++nVertexTexture )
		{
			for ( int nRenderTarget = 0; nRenderTarget <= 1; ++nRenderTarget )
			{
				for ( int nFilterable = 0; nFilterable <= 1; ++nFilterable )
				{
					g_D3DColorFormat[fmt][nVertexTexture][nRenderTarget][nFilterable] = 
						GetNearestD3DColorFormat( (ImageFormat)fmt, nRenderTarget != 0, nVertexTexture != 0, nFilterable != 0 );
				}
			}
		}
		++fmt;
	}

	// Check the depth formats
    HRESULT hr = D3D()->CheckDeviceFormat( 
		g_DisplayAdapter, g_DeviceType, ImageLoader::ImageFormatToD3DFormat( g_DeviceFormat ),
        D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24S8 );
	g_bSupportsD24S8 = !FAILED(hr);

    hr = D3D()->CheckDeviceFormat( 
		g_DisplayAdapter, g_DeviceType, ImageLoader::ImageFormatToD3DFormat( g_DeviceFormat ),
        D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24X8 );
	g_bSupportsD24X8 = !FAILED(hr);

    hr = D3D()->CheckDeviceFormat( 
		g_DisplayAdapter, g_DeviceType, ImageLoader::ImageFormatToD3DFormat( g_DeviceFormat ),
        D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D16 );
	g_bSupportsD16 = !FAILED(hr);

#if !defined( _X360 )
	hr = D3D()->CheckDeviceFormat( 
		g_DisplayAdapter, g_DeviceType, ImageLoader::ImageFormatToD3DFormat( g_DeviceFormat ),
		D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24X4S4 );
	g_bSupportsD24X4S4 = !FAILED(hr);
#else
	g_bSupportsD24X4S4 = false;
#endif

#if !defined( _X360 )
	hr = D3D()->CheckDeviceFormat( 
		g_DisplayAdapter, g_DeviceType, ImageLoader::ImageFormatToD3DFormat( g_DeviceFormat ),
		D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D15S1 );
	g_bSupportsD15S1 = !FAILED(hr);
#else
	g_bSupportsD15S1 = false;
#endif
}


//-----------------------------------------------------------------------------
// Returns true if compressed textures are supported
//-----------------------------------------------------------------------------
bool D3DSupportsCompressedTextures()
{
	return	(g_D3DColorFormat[IMAGE_FORMAT_DXT1][0][0][0] != D3DFMT_UNKNOWN) &&
			(g_D3DColorFormat[IMAGE_FORMAT_DXT3][0][0][0] != D3DFMT_UNKNOWN) &&
			(g_D3DColorFormat[IMAGE_FORMAT_DXT5][0][0][0] != D3DFMT_UNKNOWN);
}


//-----------------------------------------------------------------------------
// Returns closest supported format
//-----------------------------------------------------------------------------
ImageFormat FindNearestSupportedFormat( ImageFormat format, bool bIsVertexTexture, bool bIsRenderTarget, bool bFilterableRequired )
{
	return ImageLoader::D3DFormatToImageFormat( g_D3DColorFormat[format][bIsVertexTexture][bIsRenderTarget][bFilterableRequired] );
}


//-----------------------------------------------------------------------------
// Returns true if compressed textures are supported
//-----------------------------------------------------------------------------
bool D3DSupportsDepthTexture(D3DFORMAT format)
{
	// See if we can do it!
    HRESULT hr = D3D()->CheckDeviceFormat( 
		g_DisplayAdapter, g_DeviceType, ImageLoader::ImageFormatToD3DFormat(g_DeviceFormat),
        D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, format);

	return !FAILED(hr);
}


//-----------------------------------------------------------------------------
// Returns true if the depth format is compatible with the display
//-----------------------------------------------------------------------------
static inline bool IsDepthFormatCompatible( int nAdapter, ImageFormat displayFormat, ImageFormat renderTargetFormat, D3DFORMAT depthFormat )
{
	D3DFORMAT d3dDisplayFormat = ImageLoader::ImageFormatToD3DFormat( displayFormat );
	D3DFORMAT d3dRenderTargetFormat = ImageLoader::ImageFormatToD3DFormat( renderTargetFormat );

	// Verify that the depth format is compatible.
	HRESULT hr = D3D()->CheckDepthStencilMatch(	nAdapter, DX8_DEVTYPE,
		d3dDisplayFormat, d3dRenderTargetFormat, depthFormat);
	return !FAILED(hr);
}

//-----------------------------------------------------------------------------
// Finds the nearest supported depth buffer format
//-----------------------------------------------------------------------------
D3DFORMAT FindNearestSupportedDepthFormat( int nAdapter, ImageFormat displayFormat, ImageFormat renderTargetFormat, D3DFORMAT depthFormat )
{
	// This is the default case, used for rendering to the main render target
	Assert( displayFormat != IMAGE_FORMAT_UNKNOWN && renderTargetFormat != IMAGE_FORMAT_UNKNOWN );

	switch (depthFormat)
	{
#if defined( _X360 )
	case D3DFMT_D24FS8:
		return D3DFMT_D24FS8;

	case D3DFMT_LIN_D24S8:
		if ( g_bSupportsD24S8 && IsDepthFormatCompatible( nAdapter, displayFormat, renderTargetFormat, D3DFMT_LIN_D24S8 ) )
			return D3DFMT_LIN_D24S8;
#endif
	case D3DFMT_D24S8:
		if ( g_bSupportsD24S8 && IsDepthFormatCompatible( nAdapter, displayFormat, renderTargetFormat, D3DFMT_D24S8 ) )
			return D3DFMT_D24S8;
#if !defined( _X360 )
		if ( g_bSupportsD24X4S4 && IsDepthFormatCompatible( nAdapter, displayFormat, renderTargetFormat, D3DFMT_D24X4S4 ) )
			return D3DFMT_D24X4S4;
		if ( g_bSupportsD15S1 && IsDepthFormatCompatible( nAdapter, displayFormat, renderTargetFormat, D3DFMT_D15S1 ) )
			return D3DFMT_D15S1;
#endif
		if ( g_bSupportsD24X8 && IsDepthFormatCompatible( nAdapter, displayFormat, renderTargetFormat, D3DFMT_D24X8 ) )
			return D3DFMT_D24X8;
		if ( g_bSupportsD16 && IsDepthFormatCompatible( nAdapter, displayFormat, renderTargetFormat, D3DFMT_D16 ) )
			return D3DFMT_D16;
		break;

	case D3DFMT_D24X8:
		if ( g_bSupportsD24X8 && IsDepthFormatCompatible( nAdapter, displayFormat, renderTargetFormat, D3DFMT_D24X8 ) )
			return D3DFMT_D24X8;
		if ( g_bSupportsD24S8 && IsDepthFormatCompatible( nAdapter, displayFormat, renderTargetFormat, D3DFMT_D24S8 ) )
			return D3DFMT_D24S8;
#if !defined( _X360 )
		if ( g_bSupportsD24X4S4 && IsDepthFormatCompatible( nAdapter, displayFormat, renderTargetFormat, D3DFMT_D24X4S4 ) )
			return D3DFMT_D24X4S4;
#endif
        if ( g_bSupportsD16 && IsDepthFormatCompatible( nAdapter, displayFormat, renderTargetFormat, D3DFMT_D16 ) )
			return D3DFMT_D16;
#if !defined( _X360 )
		if ( g_bSupportsD15S1 && IsDepthFormatCompatible( nAdapter, displayFormat, renderTargetFormat, D3DFMT_D15S1 ) )
			return D3DFMT_D15S1;
#endif
		break;

	case D3DFMT_D16:
		if ( g_bSupportsD16 && IsDepthFormatCompatible( nAdapter, displayFormat, renderTargetFormat, D3DFMT_D16 ) )
			return D3DFMT_D16;
#if !defined( _X360 )
		if ( g_bSupportsD15S1 && IsDepthFormatCompatible( nAdapter, displayFormat, renderTargetFormat, D3DFMT_D15S1 ) )
			return D3DFMT_D15S1;
#endif
		if ( g_bSupportsD24X8 && IsDepthFormatCompatible( nAdapter, displayFormat, renderTargetFormat, D3DFMT_D24X8 ) )
			return D3DFMT_D24X8;
		if ( g_bSupportsD24S8 && IsDepthFormatCompatible( nAdapter, displayFormat, renderTargetFormat, D3DFMT_D24S8 ) )
			return D3DFMT_D24S8;
#if !defined( _X360 )
		if ( g_bSupportsD24X4S4 && IsDepthFormatCompatible( nAdapter, displayFormat, renderTargetFormat, D3DFMT_D24X4S4 ) )
			return D3DFMT_D24X4S4;
#endif
		break;
	}

	Assert( 0 );
	return D3DFMT_D16;
}


//-----------------------------------------------------------------------------
// Is a display buffer valid?
//-----------------------------------------------------------------------------
static inline bool IsFrameBufferFormatValid( UINT displayAdapter, D3DDEVTYPE deviceType, 
	D3DFORMAT displayFormat, D3DFORMAT backBufferFormat, bool bIsWindowed )
{
	HRESULT hr = D3D()->CheckDeviceType( displayAdapter, deviceType, displayFormat, 
                                        backBufferFormat, bIsWindowed );
	return !FAILED(hr);
}


//-----------------------------------------------------------------------------
// Finds the nearest supported frame buffer format
//-----------------------------------------------------------------------------
ImageFormat FindNearestSupportedBackBufferFormat( UINT displayAdapter, 
	  D3DDEVTYPE deviceType, ImageFormat displayFormat, ImageFormat backBufferFormat, bool bIsWindowed )
{
	D3DFORMAT d3dDisplayFormat = ImageLoader::ImageFormatToD3DFormat( displayFormat );
	switch (backBufferFormat)
	{
	case IMAGE_FORMAT_RGBA8888:
	case IMAGE_FORMAT_ABGR8888:
	case IMAGE_FORMAT_ARGB8888:
	case IMAGE_FORMAT_BGRA8888:
	case IMAGE_FORMAT_BGRA4444:		// This is not supported ever; bump up to 32 bit
		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_A8R8G8B8, bIsWindowed ))
			return IMAGE_FORMAT_BGRA8888;

		// Bye, bye dest alpha
		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_X8R8G8B8, bIsWindowed ))
			return IMAGE_FORMAT_BGRX8888;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_R5G6B5, bIsWindowed ))
			return IMAGE_FORMAT_BGR565;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_A1R5G5B5, bIsWindowed ))
			return IMAGE_FORMAT_BGRA5551;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_X1R5G5B5, bIsWindowed ))
			return IMAGE_FORMAT_BGRX5551;

		return IMAGE_FORMAT_UNKNOWN;

	case IMAGE_FORMAT_RGB888:
	case IMAGE_FORMAT_BGR888:
	case IMAGE_FORMAT_RGB888_BLUESCREEN:
	case IMAGE_FORMAT_BGRX8888:
		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_X8R8G8B8, bIsWindowed ))
			return IMAGE_FORMAT_BGRX8888;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_A8R8G8B8, bIsWindowed ))
			return IMAGE_FORMAT_BGRA8888;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_R5G6B5, bIsWindowed ))
			return IMAGE_FORMAT_BGR565;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_A1R5G5B5, bIsWindowed ))
			return IMAGE_FORMAT_BGRA5551;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_X1R5G5B5, bIsWindowed ))
			return IMAGE_FORMAT_BGRX5551;

		return IMAGE_FORMAT_UNKNOWN;

	case IMAGE_FORMAT_RGB565:
	case IMAGE_FORMAT_BGR565:
		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_R5G6B5, bIsWindowed ))
			return IMAGE_FORMAT_BGR565;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_A1R5G5B5, bIsWindowed ))
			return IMAGE_FORMAT_BGRA5551;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_X1R5G5B5, bIsWindowed ))
			return IMAGE_FORMAT_BGRX5551;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_X8R8G8B8, bIsWindowed ))
			return IMAGE_FORMAT_BGRX8888;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_A8R8G8B8, bIsWindowed ))
			return IMAGE_FORMAT_BGRA8888;

		return IMAGE_FORMAT_UNKNOWN;

	case IMAGE_FORMAT_BGRX5551:
		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_X1R5G5B5, bIsWindowed ))
			return IMAGE_FORMAT_BGRX5551;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_A1R5G5B5, bIsWindowed ))
			return IMAGE_FORMAT_BGRA5551;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_R5G6B5, bIsWindowed ))
			return IMAGE_FORMAT_BGR565;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_X8R8G8B8, bIsWindowed ))
			return IMAGE_FORMAT_BGRX8888;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_A8R8G8B8, bIsWindowed ))
			return IMAGE_FORMAT_BGRA8888;

		return IMAGE_FORMAT_UNKNOWN;

	case IMAGE_FORMAT_BGRA5551:
		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_A1R5G5B5, bIsWindowed ))
			return IMAGE_FORMAT_BGRA5551;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_X1R5G5B5, bIsWindowed ))
			return IMAGE_FORMAT_BGRX5551;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_R5G6B5, bIsWindowed ))
			return IMAGE_FORMAT_BGR565;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_A8R8G8B8, bIsWindowed ))
			return IMAGE_FORMAT_BGRA8888;

		if (IsFrameBufferFormatValid( displayAdapter, deviceType, d3dDisplayFormat, D3DFMT_X8R8G8B8, bIsWindowed ))
			return IMAGE_FORMAT_BGRX8888;

		return IMAGE_FORMAT_UNKNOWN;
	}

	return IMAGE_FORMAT_UNKNOWN;
}

#if defined( _X360 )
const char *D3DFormatName( D3DFORMAT d3dFormat )
{
	if ( IS_D3DFORMAT_SRGB( d3dFormat ) )
	{
		// sanitize the format from possible sRGB state for comparison purposes
		d3dFormat = MAKE_NON_SRGB_FMT( d3dFormat );
	}

	switch ( d3dFormat )
	{
	case D3DFMT_A8R8G8B8:
		return "D3DFMT_A8R8G8B8";
	case D3DFMT_LIN_A8R8G8B8:
		return "D3DFMT_LIN_A8R8G8B8";
	case D3DFMT_X8R8G8B8:
		return "D3DFMT_X8R8G8B8";
	case D3DFMT_LIN_X8R8G8B8:
		return "D3DFMT_LIN_X8R8G8B8";
	case D3DFMT_R5G6B5:
		return "D3DFMT_R5G6B5";
	case D3DFMT_X1R5G5B5:
		return "D3DFMT_X1R5G5B5";
	case D3DFMT_A1R5G5B5:
		return "D3DFMT_A1R5G5B5";
	case D3DFMT_A4R4G4B4:
		return "D3DFMT_A4R4G4B4";
	case D3DFMT_L8:
		return "D3DFMT_L8";
	case D3DFMT_A8L8:
		return "D3DFMT_A8L8";
	case D3DFMT_A8:
		return "D3DFMT_A8";
	case D3DFMT_DXT1:
		return "D3DFMT_DXT1";
	case D3DFMT_DXT3:
		return "D3DFMT_DXT3";
	case D3DFMT_DXT5:
		return "D3DFMT_DXT5";
	case D3DFMT_V8U8:
		return "D3DFMT_V8U8";
	case D3DFMT_Q8W8V8U8:
		return "D3DFMT_Q8W8V8U8";
	case D3DFMT_D16:
		return "D3DFMT_D16";
	case D3DFMT_D24S8:
		return "D3DFMT_D24S8";
	case D3DFMT_D24FS8:
		return "D3DFMT_D24FS8";
	case D3DFMT_LIN_D24S8:
		return "D3DFMT_LIN_D24S8";
	case D3DFMT_A16B16G16R16:
		return "D3DFMT_A16B16G16R16";
	case D3DFMT_LIN_A16B16G16R16:
		return "D3DFMT_LIN_A16B16G16R16";
	}
	return "???";
}
#endif
