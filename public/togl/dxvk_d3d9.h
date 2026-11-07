//========= Copyright Valve Corporation, All rights reserved. ============//
//
// DXVK-native: the real Direct3D 9 API on Linux, implemented on top of Vulkan by
// libdxvk_d3d9.so. This header pulls in DXVK's Windows base types and d3d9 headers
// (DXVK's include/native, installed as <prefix>/include/dxvk) in a way that does not
// clash with the engine's own POSIX definitions of some Windows types.
//
// The window handle (HWND) that D3D9 sees is the SDL_Window* (DXVK's SDL2 WSI).
//
//=============================================================================//

#ifndef DXVK_D3D9_H
#define DXVK_D3D9_H

#ifndef DXVK
#error "dxvk_d3d9.h is only for DXVK builds"
#endif

#include "tier0/platform.h"
#include "tier0/basetypes.h"

// Engine macros that break the C++ headers DXVK includes (<cstring>, ...)
#pragma push_macro("strncpy")
#pragma push_macro("_snprintf")
#undef strncpy
#undef _snprintf

// The engine's POSIX typedefs differ from DXVK's for these two: ULONG is unsigned long
// (8 bytes) in the engine but a 32-bit integer in Windows/DXVK (COM AddRef/Release return
// it), HMODULE is an integer in the engine. Give DXVK's versions another name.
#define ULONG   DXVK_ULONG
#define HMODULE DXVK_HMODULE

#include <windows.h>
#include <d3d9.h>

// Only the D3DX math (+ shader) declarations: the full d3dx9.h pulls in GDI/COM types
// that DXVK's native headers do not have. The D3DX functions the engine uses are
// implemented in materialsystem/shaderapidx9/dxvk_d3dx9.cpp (there is no d3dx9 library).
#define __D3DX9_H__
#include <limits.h>
#include <float.h>
#define D3DX_DEFAULT         ((UINT)-1)
#define D3DX_DEFAULT_NONPOW2 ((UINT)-2)
#define D3DX_DEFAULT_FLOAT   FLT_MAX
#include <d3dx9math.h>

// ID3DXBuffer (from d3dx9core.h, which needs GDI types), needed by d3dx9shader.h
typedef struct ID3DXBuffer *LPD3DXBUFFER;
#undef INTERFACE
#define INTERFACE ID3DXBuffer
DECLARE_INTERFACE_(ID3DXBuffer, IUnknown)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **out) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;
	STDMETHOD_(void *, GetBufferPointer)(THIS) PURE;
	STDMETHOD_(DWORD, GetBufferSize)(THIS) PURE;
};
#undef INTERFACE
#include <d3dx9shader.h>

#undef ULONG
#undef HMODULE

// DXVK's windows_base.h defines ZeroMemory as a macro; the engine declares its own
// ZeroMemory() function on Linux (tier0/mem.h).
#undef ZeroMemory
#undef RtlZeroMemory

#pragma pop_macro("_snprintf")
#pragma pop_macro("strncpy")

#endif // DXVK_D3D9_H

// ImageLoader::D3DFormatToImageFormat & co. need D3DFORMAT: declared in imageformat.h only
// when this header was included (see there). Handles "imageformat.h first" order.
#if defined( IMAGEFORMAT_H ) && !defined( DXVK_IMAGEFORMAT_D3D_DECLARED )
#define DXVK_IMAGEFORMAT_D3D_DECLARED
namespace ImageLoader
{
	ImageFormat D3DFormatToImageFormat( D3DFORMAT format );
	D3DFORMAT ImageFormatToD3DFormat( ImageFormat format );
}
#endif
