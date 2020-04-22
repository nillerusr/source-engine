//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================


#ifndef QUICKTIME_VIDEO_H
#define QUICKTIME_VIDEO_H

#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IFileSystem;
class IMaterialSystem;
class CQuickTimeMaterial;

//-----------------------------------------------------------------------------
// Global interfaces - you already did the needed includes, right?
//-----------------------------------------------------------------------------
extern IFileSystem		*g_pFileSystem;
extern IMaterialSystem	*materials;

//-----------------------------------------------------------------------------
// Quicktime includes - conditional compilation on #define QUICKTIME in VPC
//   The intent is to have a default functionality fallback if not defined
//   which provides a dynamically generated texture (moving line on background)
//-----------------------------------------------------------------------------
#if defined ( OSX )
	#include <quicktime/QTML.h>
	#include <quicktime/Movies.h>
#elif defined ( WIN32 )
	#include <QTML.h>
	#include <Movies.h>
	#include <windows.h>
#elif
    #error "Quicktime not supported on this target platform"	
#endif


#include "video/ivideoservices.h"
#include "videosubsystem.h"

#include "utlvector.h"





// -----------------------------------------------------------------------------
// CQuickTimeVideoSubSystem - Implementation of IVideoSubSystem
// -----------------------------------------------------------------------------
class CQuickTimeVideoSubSystem : public CTier2AppSystem< IVideoSubSystem >
{
	typedef CTier2AppSystem< IVideoSubSystem > BaseClass;

	public:
		CQuickTimeVideoSubSystem();
		~CQuickTimeVideoSubSystem();

		// Inherited from IAppSystem 
		virtual bool					Connect( CreateInterfaceFn factory );
		virtual void					Disconnect();
		virtual void				   *QueryInterface( const char *pInterfaceName );
		virtual InitReturnVal_t			Init();
		virtual void					Shutdown();

		// Inherited from IVideoSubSystem
		 
		// SubSystem Identification functions
		virtual VideoSystem_t			GetSystemID();
		virtual VideoSystemStatus_t		GetSystemStatus();
		virtual VideoSystemFeature_t	GetSupportedFeatures();
		virtual const char			   *GetVideoSystemName();
		
		// Setup & Shutdown Services
		virtual bool					InitializeVideoSystem( IVideoCommonServices *pCommonServices );
		virtual bool					ShutdownVideoSystem();
		
		virtual VideoResult_t			VideoSoundDeviceCMD( VideoSoundDeviceOperation_t operation, void *pDevice = nullptr, void *pData = nullptr );
		
		// get list of file extensions and features we support
		virtual int						GetSupportedFileExtensionCount();
		virtual const char			   *GetSupportedFileExtension( int num );
		virtual VideoSystemFeature_t	GetSupportedFileExtensionFeatures( int num );

		// Video Playback and Recording Services
		virtual VideoResult_t			PlayVideoFileFullScreen( const char *filename, void *mainWindow, int windowWidth, int windowHeight, int desktopWidth, int desktopHeight, bool windowed, float forcedMinTime, VideoPlaybackFlags_t playbackFlags );

		// Create/destroy a video material
		virtual IVideoMaterial		   *CreateVideoMaterial( const char *pMaterialName, const char *pVideoFileName, VideoPlaybackFlags_t flags );
		virtual VideoResult_t			DestroyVideoMaterial( IVideoMaterial *pVideoMaterial );

		// Create/destroy a video encoder		
		virtual IVideoRecorder		   *CreateVideoRecorder();
		virtual VideoResult_t			DestroyVideoRecorder( IVideoRecorder *pRecorder );
		
		virtual VideoResult_t			CheckCodecAvailability( VideoEncodeCodec_t codec );
		
		virtual VideoResult_t			GetLastResult();

	private:
	
		bool							SetupQuickTime();
		bool							ShutdownQuickTime();

		VideoResult_t					SetResult( VideoResult_t status );

		bool							m_bQuickTimeInitialized;
		VideoResult_t					m_LastResult;
		
		VideoSystemStatus_t				m_CurrentStatus;
		VideoSystemFeature_t			m_AvailableFeatures;

		IVideoCommonServices		   *m_pCommonServices;
		
		CUtlVector< IVideoMaterial* >	m_MaterialList;
		CUtlVector< IVideoRecorder* >	m_RecorderList;
		
		static const VideoSystemFeature_t	DEFAULT_FEATURE_SET;
};

#endif // QUICKTIME_VIDEO_H
