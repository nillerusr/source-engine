//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IIMAGESOURCE_H
#define IIMAGESOURCE_H

#ifdef _WIN32
#pragma once
#endif

namespace panorama
{

enum EImageFormat
{
	k_EImageFormatUnknown,
	k_EImageFormatR8G8B8A8,
	k_EImageFormatB8G8R8A8_PreMultiplied,
	k_EImageFormatA8
};

class CPanel2D;

//
// Data source for image data used to render an asset
//
class IImageSource: public panorama::IUIJSObject
{
public:
	virtual bool BIsValid() = 0;
	virtual uint32 GetTextureID() = 0;
	virtual int GetWidth() = 0;
	virtual int GetHeight() = 0;
	virtual EImageFormat ImageFormat() = 0;
	virtual bool BIsAnimating() = 0;

	// Ref counting
	virtual int GetRefCount() = 0;
	virtual int AddRef() = 0;
	virtual int Release() = 0;
	
	virtual const char *GetJSTypeName() { return "IImageSource"; }

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName ) = 0;
#endif
protected:

	friend class CImageResourceManager;
};


//
// main interface to load images for display in the ui
//
const int k_ResizeNone = -1;
class IUIImageManager
{
public:

    // load image data from a URL (file://blah, http://blah/bob.tga, etc), pchDefaultResourceURL may be null, and is what will be used while the real resource is loaded
	// if it is set.  As such it must be a local file.  bPrioritizeLoad will make your request jump to the head of the queue if it is an image over HTTP, use sparingly!
	// nResizeWidth and nResizeHeight do nothing if -1, if one is set and the other -1 the image is resized before being made into a texture, but aspect ratio is maintained
	// with the specified dimension at the set size, if both are set the image will be stretched if needed.
    virtual IImageSource *LoadImageFromURL( const IUIPanel *pPanel, const char *pchDefaultResourceURL, const char *pchResourceURL, bool bPrioritizeLoad, EImageFormat imgFormatOut, int32 nResizeWidth = k_ResizeNone, int32 nResizeHeight = k_ResizeNone, bool bAllowAnimation = true ) = 0;

	// load image data from image file (png/jpg/tga) bytes you already have in memory, pchDefaultResourceURL may be null, and is what will be used while the real resource is loaded
	// if it is set.  As such it must be a local file.
	virtual IImageSource *LoadImageFileFromMemory( const IUIPanel *pPanel, const char *pchResourceURLDefault, const CUtlBuffer &bufFile, int nResizeWidth = panorama::k_ResizeNone, int nResizeHeight = panorama::k_ResizeNone, bool bAllowAnimation = true ) = 0;

	// load image data from RGBA bytes you already have in memory, pchDefaultResourceURL may be null, and is what will be used while the real resource is loaded
	// if it is set.  As such it must be a local file.
	virtual IImageSource *LoadImageFromMemory( const IUIPanel *pPanel, const char *pchDefaultResourceURL, const CUtlBuffer &bufData, int nWide, int nTall, EImageFormat imgFormatIn = k_EImageFormatR8G8B8A8, int nResizeWidth = k_ResizeNone, int nResizeHeight = k_ResizeNone, bool bAllowAnimation = true ) = 0;
    
	virtual CUtlString GetPchImageSourcePath( IImageSource *pImageSource ) = 0;

	virtual void ReloadChangedImage( IImageSource *pImageToReload ) = 0;

	virtual void ReloadChangedFile( const char *pchFile ) = 0;

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName ) = 0;
#endif
};

DECLARE_PANEL_EVENT1( ImageLoaded, IImageSource * );
DECLARE_PANEL_EVENT1( ImageFailedLoad, IImageSource * );

} // namespace panorama

#endif // IIMAGESOURCE_H