//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
//=============================================================================

#ifndef VIDEOSUBSYSTEM_H
#define VIDEOSUBSYSTEM_H

#if defined ( WIN32 )
    #pragma once
#endif

#include "tier2/tier2.h"
#include "appframework/IAppSystem.h"



//-----------------------------------------------------------------------------
// Common structure used to store supported file types
//-----------------------------------------------------------------------------
struct VideoFileExtensionInfo_t
{
	const char			   *m_FileExtension;
	VideoSystem_t			m_VideoSubSystem;
	VideoSystemFeature_t	m_VideoFeatures;
};




class IVideoCommonServices
{
	public:
		virtual bool			CalculateVideoDimensions( int videoWidth, int videoHeight, int displayWidth, int displayHeight, VideoPlaybackFlags_t playbackFlags, 
													  int *pOutputWidth, int *pOutputHeight, int *pXOffset, int *pYOffset ) = 0;

		virtual	float			GetSystemVolume() = 0;
													  
		virtual VideoResult_t	InitFullScreenPlaybackInputHandler( VideoPlaybackFlags_t playbackFlags, float forcedMinTime, bool windowed ) = 0;
		
		virtual bool			ProcessFullScreenInput( bool &bAbortEvent, bool &bPauseEvent, bool &bQuitEvent ) = 0;
		
		virtual VideoResult_t	TerminateFullScreenPlaybackInputHandler() = 0;
		
};


//-----------------------------------------------------------------------------
// Main VIDEO_SERVICES interface
//-----------------------------------------------------------------------------
#define VIDEO_SUBSYSTEM_INTERFACE_VERSION   "IVideoSubSystem002"

class IVideoSubSystem : public IAppSystem
{
	public:
		// SubSystem Identification functions
		virtual VideoSystem_t			GetSystemID() = 0;
		virtual VideoSystemStatus_t		GetSystemStatus() = 0;
		virtual VideoSystemFeature_t	GetSupportedFeatures() = 0;
		virtual const char			   *GetVideoSystemName() = 0;
		
		// Setup & Shutdown Services
		virtual bool					InitializeVideoSystem( IVideoCommonServices *pCommonServices ) = 0;
		virtual bool					ShutdownVideoSystem() = 0;
		
		virtual VideoResult_t			VideoSoundDeviceCMD( VideoSoundDeviceOperation_t operation, void *pDevice, void *pData = nullptr ) = 0;
		
		// get list of file extensions and features we support
		virtual int						GetSupportedFileExtensionCount() = 0;
		virtual const char			   *GetSupportedFileExtension( int num ) = 0;
		virtual VideoSystemFeature_t	GetSupportedFileExtensionFeatures( int num ) = 0;

		// Video Playback and Recording Services
		
		virtual VideoResult_t			PlayVideoFileFullScreen( const char *filename, void *mainWindow, int windowWidth, int windowHeight, int desktopWidth, int desktopHeight, bool windowed, float forcedMinTime, VideoPlaybackFlags_t playbackFlags ) = 0;

		// Create/destroy a video material
		virtual IVideoMaterial		   *CreateVideoMaterial( const char *pMaterialName, const char *pVideoFileName, VideoPlaybackFlags_t flags ) = 0;
		virtual VideoResult_t			DestroyVideoMaterial( IVideoMaterial* pVideoMaterial ) = 0;

		// Create/destroy a video encoder		
		virtual IVideoRecorder		   *CreateVideoRecorder() = 0;
		virtual VideoResult_t			DestroyVideoRecorder( IVideoRecorder *pRecorder ) = 0;

		virtual VideoResult_t			CheckCodecAvailability( VideoEncodeCodec_t codec ) = 0;
		
		virtual VideoResult_t			GetLastResult() = 0;

};



#endif
