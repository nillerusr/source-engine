//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUIRENDERENGINE_H
#define IUIRENDERENGINE_H

#ifdef _WIN32
#pragma once
#endif

#include "panoramatypes.h"
#include "tier1/refcount.h"
#if defined( SOURCE2_PANORAMA )
class IRenderContext;
#include "rendermessages.pb.h"
class IRenderContext;
#else
#include "../../panorama/renderer/rendermessages.pb.h"
#endif

class CMsgFillBrushCollection;

namespace panorama
{

enum E2DTextureFormat
{
	k_EFormatRGBA8,
	k_EFormatBGRA8,
	k_EFormatBGR8, // 8 bits per channel, last 8 bits in the dword are ignored
	k_EFormatA8,
	k_EFormatYUV420,
	k_EFormatR16G16B16A16,
	k_EFormatDXT1,
	k_EFormatDXT5,
};


enum EAlphaChannelType
{
	k_EAlphaChannelType_None,
	k_EAlphaChannelType_Normal,
	k_EAlphaChannelType_PreMultiplied,
};


//
// Class to represent all textures
//
class IUITexture
{
public:

	virtual ~IUITexture() { }

	virtual uint32 GetTextureID() = 0;
	virtual uint32 GetOriginalWidth() { return GetTextureWidth(); }
	virtual uint32 GetOriginalHeight() { return GetTextureHeight(); }
	virtual uint32 GetTextureWidth() = 0;
	virtual uint32 GetTextureHeight() = 0;
	virtual uint32 GetStride() = 0;
	virtual E2DTextureFormat GetFormat() = 0;
	virtual EAlphaChannelType GetAlphaChannelType() = 0;
	virtual bool BIsReady() = 0;
};


//
// Class to handle double buffered textures of various standard formats (RGBA8, BGRA8, alpha-premulitplied/or-not, etc)
//
class IUIDoubleBufferedTexture : public IUITexture
{
public:
	virtual ~IUIDoubleBufferedTexture() {}

	// Update the data for rendering next frame
	virtual int32 UpdateTextureData( void *pTextureData ) = 0;
};


//
// YUV420 textures are special, because they need 3 textures one for Y, U, and V, and then
// they need special rendering rules in a pixel shader to scale/color convert.
//
class IUIDoubleBufferedYUV420Texture : public IUITexture
{
public:
	virtual ~IUIDoubleBufferedYUV420Texture() {}

	// Update the YUV420 data for rendering next frame
	virtual bool BUpdateTextureData( void *pYBuffer, void *pUBuffer, void *pVBuffer, uint unStrideY, uint unStrideU, uint unStrideV ) = 0;
};

//
// Render thread callback object interface
//
#if defined( SOURCE2_PANORAMA )
class CRenderThreadCallback : public CRefCount
{
public:

	// Callback function to override and perform direct rendering within
	virtual void RenderThreadCallback(ISceneView *pSceneView, IRenderContext **pRenderContext, ISceneLayer *pSceneLayer, float x0, float y0, float x1, float y1) = 0;

};
#endif

//-----------------------------------------------------------------------------
// Purpose: Interface to do drawing onto windows.  This is the full publically exposed
// drawing interface that new panel types can use for drawing, the implementation has
// more stuff available inside the framework.
//-----------------------------------------------------------------------------
class IUIRenderEngine
{
public:

	// Draw a filled quad
	virtual void DrawFilledRect( float x0, float y0, float x1, float y1, const CMsgFillBrushCollection &c, EAntialiasing antialiasing = k_EAntialisingEnabled ) = 0;

	// Draw a textured quad
	virtual void DrawTexturedRect( uint32 unTextureID, ETextureSampleMode eSampleMode, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1 ) = 0;

	// Draw text (utf-8 input)
	virtual void DrawTextRegion( const char *pchText, const char *pchFontName, const CMsgFillBrushCollection &c, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1, ::google::protobuf::RepeatedPtrField< ::CMsgTextRangeFormat > *pmsgRangeFormats ) = 0;

	// Draw text (wchar input)
	virtual void DrawTextRegion( const wchar_t *pchText, const char *pchFontName, const CMsgFillBrushCollection &c, float flSize, float flLineHeight, EFontWeight weight, EFontStyle style, ETextAlign align, ETextDecoration decoration, bool bWrap, bool bEllipsis, int nLetterSpacing, float x0, float y0, float x1, float y1, ::google::protobuf::RepeatedPtrField< ::CMsgTextRangeFormat > *pmsgRangeFormats ) = 0;

	// Draw one of the special syncronized textures
	virtual void DrawSyncronizedTexturedRect( uint32 unTextureID, ETextureSampleMode eSampleMode, int32 unSerialize, float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1 ) = 0;

	// Queue a texture delete, won't actually delete until the layout threads active frame next reaches rendering
	virtual void QueueDeleteTexture( IUITexture *pTexture ) = 0;

	// Called to create a texture, you call this directly on the main thread and the returned texture interface is thread safe,
	// so you can access its id/size and delete it from the main thread as well.  Drawing calls are not synchronized with texture creation,
	// but the contract is you must create the texture before attempting to draw for it's id.
	virtual bool BCreateTexture( IUITexture **pTextureOutput, void *pubTextureData, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType ) = 0;

#if defined( SOURCE2_PANORAMA )
	virtual bool BCreateTexture( IUITexture **pTextureOutput, const char *pResourceFile ) = 0;

	// Tell the render thread to call the panel back on the specified method, which will then be able to do 
	// direct render system calls on the render thread
	virtual void RequestRenderCallback( CRenderThreadCallback *pCallbackObj, float x0, float y0, float x1, float y1,
		float flPaddingLeft, float flPaddingRight, float flPaddingTop, float flPaddingBottom, bool bNeedsRedrawEveryFrame ) = 0;
#endif

	// Called to create a double buffered texture, you call this directly on the main thread
	// and the returned texture interface is thread safe, so you can update the texture data directly.  The textures are 
	// double buffered, so it should be hard to block the render thread, but some locking does occur.  Unlike normal texture
	// drawing your draw calls are not synchronized with texture data updates, so you could end up skipping frames or such.
	virtual bool BCreateDoubleBufferedTexture( IUIDoubleBufferedTexture **pDoubleBufferedOutput, uint32 unWidth, uint32 unHeight, uint32 unStride, E2DTextureFormat eFormat, EAlphaChannelType eAlphaChannelType, bool bSerializedUploads ) = 0;

	// Called to create a double buffered YUV420 texture (for movie rendering), you call this directly on the main thread
	// and the returned texture interface is thread safe, so you can update the texture data directly.  The textures are 
	// double buffered, so it should be hard to block the render thread, but some locking does occur.  Unlike normal texture
	// drawing your draw calls are not synchronized with texture data updates, so you could end up skipping frames or such.
	virtual bool BCreateDoubleBufferedYUV420Texture( IUIDoubleBufferedYUV420Texture **pDoubleBufferedYUV420Output, uint32 unWidth, uint32 unHeight ) = 0;
};

}
#endif // IUIRENDERENGINE_H
