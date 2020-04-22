//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#ifndef TEXTUREHEAP_H
#define TEXTUREHEAP_H

#if defined( _X360 )

#include "locald3dtypes.h"

class CTextureHeap
{
public:
	IDirect3DTexture		*AllocTexture( int width, int height, int levels, DWORD usage, D3DFORMAT format, bool bFallback, bool bNoD3DMemory );
	IDirect3DCubeTexture	*AllocCubeTexture( int width, int levels, DWORD usage, D3DFORMAT format, bool bFallback, bool bNoD3DMemory );
	IDirect3DVolumeTexture	*AllocVolumeTexture( int width, int height, int depth, int levels, DWORD usage, D3DFORMAT format );
	IDirect3DSurface		*AllocRenderTargetSurface( int width, int height, D3DFORMAT format, bool bMultiSample = false , int base = -1);

	// Perform the real d3d allocation, returns true if succesful, false otherwise.
	// Only valid for a texture created with no d3d bits, otherwise no-op.
	bool					AllocD3DMemory( IDirect3DBaseTexture *pTexture );

	// Release header and d3d bits.
	void					FreeTexture( IDirect3DBaseTexture *pTexture );

	// Returns the amount of memory needed or allocated for the texture.
	int						GetSize( IDirect3DBaseTexture *pTexture );

	// Crunch the heap.
	void					Compact();

	// Get current backbuffer multisample type
	D3DMULTISAMPLE_TYPE		GetBackBufferMultiSampleType();
};

extern CTextureHeap g_TextureHeap;

#endif
#endif // TEXTUREHEAP_H
