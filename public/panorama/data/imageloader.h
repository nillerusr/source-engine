//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IMAGESOURCE_H
#define IMAGESOURCE_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/data/iimagesource.h"
#include "panorama/controls/panelptr.h"
#include "tier1/utlbuffer.h"
#include "tier1/fileio.h"
#include "tier1/utlmap.h"
#include "refcount.h"
#include "../../panorama/uifileresource.h"
#include "tier1/utldelegate.h"

namespace panorama
{

// Helper to determine byte size of a pixel in a given format
int GetFormatPixelBytes( EImageFormat format );

class CMovie;
class IUIRenderEngine;
class IUITexture;
class CImageData;


// Helper for converting rgba8 to a8 throwing out rgb channels
void ConvertRGBA8ToA8( CUtlBuffer &bufIn, CUtlBuffer &bufOut, uint32 unWide, uint32 unTall );

class CImageProxySource;
class CImageLoaderTask;

#if defined( SOURCE2_PANORAMA )
class CLoadFromVTexTask;
#endif

enum ESourceFormats
{
	k_ESourceFormatUnknown,
	k_ESourceFormatTGA,
	k_ESourceFormatPNG,
	k_ESourceFormatJPG,
	k_ESourceFormatRawRGBA,
	k_ESourceFormatGIF,
	k_ESourceFormatVTEX
};

class CImageData;
typedef void (ImageDecodeCallback_t)( bool bSuccess, CImageData *pNewImage, CUtlBuffer *pBufDecoded );

class CImageDecodeWorkItem
{
public:
	CImageDecodeWorkItem( IUIRenderEngine *pRenderEngine, CUtlBuffer &bufDataInMayModify, const char *pchFilePath, int nWide, int nTall, int nResizeWidth, int nResizeHeight,
		EImageFormat formatOut, bool bAllowAnimation, CUtlDelegate< ImageDecodeCallback_t > del );
	~CImageDecodeWorkItem();

	void RunWorkItem();
	void DispatchResult();

private:

	bool m_bSuccess;
	IUIRenderEngine *m_pSurface;
	CUtlBuffer *m_pBuffer;
	CUtlString m_strFilePath;
	int m_nWide;
	int m_nTall;
	int m_nResizeWidth;
	int m_nResizeHeight;
	EImageFormat m_eFormat;
	bool m_bAllowAnimation;
	CImageData *m_pNewImage;

	CUtlDelegate< ImageDecodeCallback_t > m_Del;
};

class CImageDecodeWorkThreadPool;
class CImageDecodeThread : public CThread
{
public:
	CImageDecodeThread( CImageDecodeWorkThreadPool *pParent )
	{
		m_bExit = false;
		m_pParent = pParent;
	}

	~CImageDecodeThread()
	{

	}

	void Stop() { m_bExit = true; }

	virtual int Run() OVERRIDE;

private:
	volatile bool m_bExit;
	CImageDecodeWorkThreadPool *m_pParent;
};

class CImageDecodeWorkThreadPool
{
public:
	CImageDecodeWorkThreadPool();
	~CImageDecodeWorkThreadPool();

	// Run frame on main thread
	void RunFrame();

	void AddWorkItem( CImageDecodeWorkItem  *pWorkItem );
	
private:

	friend class CImageDecodeThread;
	CImageDecodeThread * m_pWorkThreads[1];

	CThreadMutex m_AsyncIoLock;
	CThreadEvent m_ThreadEvent;
	CUtlLinkedList< CImageDecodeWorkItem *, int > m_llAsyncIORequests;
	CUtlLinkedList< CImageDecodeWorkItem *, int > m_llAsyncIOResults;
};

//
// A container of images we have loaded
//
class CImageResourceManager : public IUIImageManager
{
public:
	CImageResourceManager( IUIRenderEngine *pSurface );
	~CImageResourceManager();
	virtual void Shutdown();

	virtual IImageSource *LoadImageFromURL( const IUIPanel *pPanel, const char *pchResourceURLDefault, const char *pchResourceURL, bool bPrioritizeLoad, EImageFormat imgFormatOut, int32 nResizeWidth = panorama::k_ResizeNone, int32 nResizeHeight = panorama::k_ResizeNone, bool bAllowAnimation = true );
	virtual IImageSource *LoadImageFileFromMemory( const IUIPanel *pPanel, const char *pchResourceURLDefault, const CUtlBuffer &bufFile, int nResizeWidth = panorama::k_ResizeNone, int nResizeHeight = panorama::k_ResizeNone, bool bAllowAnimation = true );
	virtual IImageSource *LoadImageFromMemory( const IUIPanel *pPanel, const char *pchResourceURLDefault, const CUtlBuffer &bufRGBA, int nWide, int nTall, EImageFormat imgFormatIn = k_EImageFormatR8G8B8A8, int nResizeWidth = panorama::k_ResizeNone, int nResizeHeight = panorama::k_ResizeNone, bool bAllowAnimation = true );
	virtual CUtlString GetPchImageSourcePath( IImageSource *pImageSource ) OVERRIDE;

	virtual void ReloadChangedFile( const char *pchFile ) OVERRIDE;
	virtual void ReloadChangedImage( IImageSource *pImageToReload ) OVERRIDE;

	bool OnImageUnreferenced( CImageProxySource *pImage );

	void RunFrame();

	void QueueImageDecodeWorkItem( CImageDecodeWorkItem *pWorkItem ); 

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

private:
	IImageSource *LoadImageInternal( const IUIPanel *pPanel, CFileResource &fileResourceDefault, CFileResource &fileResource, bool bPrioritizeLoad, EImageFormat imgFormatOut, int32 nResizeWidth, int32 nResizeHeight, bool bAllowAnimation );
	CImageProxySource *GetDefaultImage( CFileResource &fileDefault, EImageFormat imgFormatOut, bool bAllowAnimation );

	friend class CLoadFileURLTask;
	friend class CLoadFileLocalTask;
	friend class CImageLoaderTask;
#if defined( SOURCE2_PANORAMA )
	friend class CLoadFromVTexTask;
#endif

	// Internally adds image to our tracking maps
	void AddImageToManager( CFileResource &resource, CImageProxySource *pImageProxy, int32 nResizeWidth, int32 nResizeHeight, bool bAllowAnimation );
	bool RemoveImageFromManager( IImageSource *pImage );

	// Loads a resource, returns vector index
	bool OnImageLoaded( CFileResource & resource, CImageData *pImage, int32 nResizeWidth, int32 nResizeHeight, bool bAllowAnimation );
	bool OnFailedImageLoad( CFileResource & resource, int32 nResizeWidth, int32 nResizeHeight, bool bAllowAnimation );
	void AddLoad( CFileResource &resource, EImageFormat eFormat, bool bPrioritizeLoad, int32 nResizeWidth, int32 nResizeHeight, bool bAllowAnimation );

	// Synchronous load only used for initial global default image
	bool LoadLocalFileSynchronous( CFileResource &resource, EImageFormat eFormat, bool bAllowAnimation );

#if defined( SOURCE2_PANORAMA )
	bool FixupFileResourceToCompiledImage( CFileResource &fileResource );
#endif

	struct UrlImageKey_t
	{
		CFileResource fileResource;
		int32 nTargetWidth;
		int32 nTargetHeight;
		bool bAllowAnimation;

		// Sort on size only
		bool operator <( const UrlImageKey_t &l ) const
		{
			if ( nTargetWidth < l.nTargetWidth )
				return true;
			else if ( nTargetWidth > l.nTargetWidth )
				return false;

			if ( nTargetHeight < l.nTargetHeight )
				return true;
			else if ( nTargetHeight > l.nTargetHeight )
				return false;

			if ( bAllowAnimation && !l.bAllowAnimation )
				return true;
			else if ( !bAllowAnimation && l.bAllowAnimation )
				return false;

			return fileResource < l.fileResource;
		}
	};
	CUtlMap< UrlImageKey_t, CImageProxySource *, int, CDefLess< UrlImageKey_t > > m_mapImagesByURL;
	CUtlMap< IImageSource *, UrlImageKey_t, int, CDefLess< IImageSource *> > m_mapAllImages;

	CUtlVector< CImageLoaderTask * > m_vecLoaderTasksToStart;
	CUtlRBTree< CImageLoaderTask *, int, CDefLess< CImageLoaderTask * > > m_treeLoadTasks;

	IUIRenderEngine *m_pSurface;

	CImageDecodeWorkThreadPool *m_pImageDecodePool;

	bool m_bInited;
};

} // namespace panorama

#endif // IMAGESOURCE_H