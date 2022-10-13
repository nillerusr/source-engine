//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================


#ifndef WEBM_VIDEO_H
#define WEBM_VIDEO_H

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
// WebM Header files, anything that seems strange here is to make it so their headers
// can mesh with ours
//-----------------------------------------------------------------------------
#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_codec.h"
#include "vpx/vpx_encoder.h"
#include "vpx/vpx_image.h"
#include "vpx/vp8cx.h"
#ifdef UNUSED
#undef UNUSED
#endif

// libwebm, support for reading/writing webm files
#include "mkvreader.hpp"
#include "mkvparser.hpp"
#include "mkvmuxer.hpp"
#include "mkvwriter.hpp"
#include "mkvmuxerutil.hpp"

#include "vorbis/vorbisenc.h"
#include "vorbis/codec.h"

#include "video/ivideoservices.h"
#include "videosubsystem.h"

#include "utlvector.h"
#include "tier1/KeyValues.h"
#include "tier0/platform.h"

// -----------------------------------------------------------------------------
// CQuickTimeVideoSubSystem - Implementation of IVideoSubSystem
// -----------------------------------------------------------------------------
class CWebMVideoSubSystem : public CTier2AppSystem< IVideoSubSystem >
{
	typedef CTier2AppSystem< IVideoSubSystem > BaseClass;

	public:
		CWebMVideoSubSystem();
		~CWebMVideoSubSystem();

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
	
		bool							SetupWebM();
		bool							ShutdownWebM();

		VideoResult_t					SetResult( VideoResult_t status );

		bool							m_bWebMInitialized;
		VideoResult_t					m_LastResult;
		
		VideoSystemStatus_t				m_CurrentStatus;
		VideoSystemFeature_t			m_AvailableFeatures;

		IVideoCommonServices		   *m_pCommonServices;
		
		CUtlVector< IVideoMaterial* >	m_MaterialList;
		CUtlVector< IVideoRecorder* >	m_RecorderList;
		
		static const VideoSystemFeature_t	DEFAULT_FEATURE_SET;
};

#endif // WEBM_VIDEO_H
