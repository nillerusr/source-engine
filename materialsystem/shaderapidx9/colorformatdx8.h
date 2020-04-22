//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef COLORFORMATDX8_H
#define COLORFORMATDX8_H

#include <pixelwriter.h>
#include "togl/rendermechanism.h"

// FOURCC formats for ATI shadow depth textures
#define ATIFMT_D16		((D3DFORMAT)(MAKEFOURCC('D','F','1','6')))
#define ATIFMT_D24S8	((D3DFORMAT)(MAKEFOURCC('D','F','2','4')))

// FOURCC formats for ATI2N and ATI1N compressed textures (360 and DX10 parts also do these)
#define ATIFMT_ATI2N ((D3DFORMAT) MAKEFOURCC('A', 'T', 'I', '2'))
#define ATIFMT_ATI1N ((D3DFORMAT) MAKEFOURCC('A', 'T', 'I', '1'))

// FOURCC formats for nVidia shadow depth textures
#define NVFMT_RAWZ		((D3DFORMAT)(MAKEFOURCC('R','A','W','Z')))
#define NVFMT_INTZ		((D3DFORMAT)(MAKEFOURCC('I','N','T','Z')))

// FOURCC format for nVidia null texture format
#define NVFMT_NULL		((D3DFORMAT)(MAKEFOURCC('N','U','L','L')))


//-----------------------------------------------------------------------------
// Finds the nearest supported frame buffer format
//-----------------------------------------------------------------------------
ImageFormat FindNearestSupportedBackBufferFormat( unsigned int displayAdapter, D3DDEVTYPE deviceType,
	ImageFormat displayFormat, ImageFormat backBufferFormat, bool bIsWindowed );

//-----------------------------------------------------------------------------
// Initializes the color format informat; call it every time display mode changes
//-----------------------------------------------------------------------------
void InitializeColorInformation( unsigned int displayAdapter, D3DDEVTYPE deviceType, 
								 ImageFormat displayFormat );

//-----------------------------------------------------------------------------
// Returns true if compressed textures are supported
//-----------------------------------------------------------------------------
bool D3DSupportsCompressedTextures();

//-----------------------------------------------------------------------------
// Returns closest supported format
//-----------------------------------------------------------------------------
ImageFormat FindNearestSupportedFormat( ImageFormat format, bool bIsVertexTexture, bool bIsRenderTarget, bool bFilterableRequired );

//-----------------------------------------------------------------------------
// Finds the nearest supported depth buffer format
//-----------------------------------------------------------------------------
D3DFORMAT FindNearestSupportedDepthFormat( int nAdapter, ImageFormat displayFormat, ImageFormat renderTargetFormat, D3DFORMAT depthFormat );

const char *D3DFormatName( D3DFORMAT d3dFormat );

#endif // COLORFORMATDX8_H
