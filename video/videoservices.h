//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
//=============================================================================

#ifndef VIDEOSERVICES_H
#define VIDEOSERVICES_H

#if defined ( WIN32 )
    #pragma once
#endif


#include "video/ivideoservices.h"

#include "videosubsystem.h"


struct CVideFileoExtInfo_t
{
	const char			   *m_pExtension;					// extension including "."
	VideoSystem_t			m_VideoSystemSupporting;
	VideoSystemFeature_t	m_VideoFeaturesSupporting;
};


struct CActiveVideoObjectRecord_t
{
	void   *m_pObject;
	int		m_VideoSystem;
};


//-----------------------------------------------------------------------------
// Main VIDEO_SERVICES interface
//-----------------------------------------------------------------------------

class CValveVideoServices : public CTier3AppSystem< IVideoServices >
{
	typedef CTier3AppSystem< IVideoServices > BaseClass;

	public:
		CValveVideoServices();
		~CValveVideoServices();
	
		// Inherited from IAppSystem 
		virtual bool					Connect( CreateInterfaceFn factory );
		virtual void					Disconnect();
		virtual void				   *QueryInterface( const char *pInterfaceName );
		virtual InitReturnVal_t			Init();
		virtual void					Shutdown();
		
		// Inherited from IVideoServices
	
		// Query the available video systems
		virtual int						GetAvailableVideoSystemCount();
		virtual VideoSystem_t			GetAvailableVideoSystem( int n );
		
		virtual bool					IsVideoSystemAvailable( VideoSystem_t videoSystem );
		virtual VideoSystemStatus_t		GetVideoSystemStatus( VideoSystem_t videoSystem );
		virtual VideoSystemFeature_t	GetVideoSystemFeatures( VideoSystem_t videoSystem );
		virtual const char			   *GetVideoSystemName( VideoSystem_t videoSystem );

		virtual VideoSystem_t			FindNextSystemWithFeature( VideoSystemFeature_t features, VideoSystem_t startAfter = VideoSystem::NONE );
		
		virtual VideoResult_t			GetLastResult();
		
		// deal with video file extensions and video system mappings
		virtual	int						GetSupportedFileExtensionCount( VideoSystem_t videoSystem );		
		virtual const char             *GetSupportedFileExtension( VideoSystem_t videoSystem, int extNum = 0 );
		virtual VideoSystemFeature_t    GetSupportedFileExtensionFeatures( VideoSystem_t videoSystem, int extNum = 0 );


		virtual	VideoSystem_t			LocateVideoSystemForPlayingFile( const char *pFileName, VideoSystemFeature_t playMode = VideoSystemFeature::PLAY_VIDEO_FILE_IN_MATERIAL );
		virtual VideoResult_t			LocatePlayableVideoFile( const char *pSearchFileName, const char *pPathID, VideoSystem_t *pPlaybackSystem, char *pPlaybackFileName, int fileNameMaxLen, VideoSystemFeature_t playMode = VideoSystemFeature::FULL_PLAYBACK );

		// Create/destroy a video material
		virtual IVideoMaterial		   *CreateVideoMaterial( const char *pMaterialName, const char *pVideoFileName, const char *pPathID = nullptr, 
															 VideoPlaybackFlags_t playbackFlags = VideoPlaybackFlags::DEFAULT_MATERIAL_OPTIONS, 
															 VideoSystem_t videoSystem = VideoSystem::DETERMINE_FROM_FILE_EXTENSION, bool PlayAlternateIfNotAvailable = true );
															 
		virtual VideoResult_t			DestroyVideoMaterial( IVideoMaterial* pVideoMaterial );
		virtual int						GetUniqueMaterialID();

		// Create/destroy a video encoder
		virtual VideoResult_t			IsRecordCodecAvailable( VideoSystem_t videoSystem, VideoEncodeCodec_t codec );
		
		virtual IVideoRecorder		   *CreateVideoRecorder( VideoSystem_t videoSystem );
		virtual VideoResult_t			DestroyVideoRecorder( IVideoRecorder *pVideoRecorder );

		// Plays a given video file until it completes or the user presses ESC, SPACE, or ENTER
		virtual VideoResult_t			PlayVideoFileFullScreen( const char *pFileName, const char *pPathID, void *mainWindow, int windowWidth, int windowHeight, int desktopWidth, int desktopHeight, bool windowed, float forcedMinTime, 
																 VideoPlaybackFlags_t playbackFlags = VideoPlaybackFlags::DEFAULT_FULLSCREEN_OPTIONS, 
																 VideoSystem_t videoSystem = VideoSystem::DETERMINE_FROM_FILE_EXTENSION, bool PlayAlternateIfNotAvailable = true );

		// Sets the sound devices that the video will decode to
		virtual VideoResult_t			SoundDeviceCommand( VideoSoundDeviceOperation_t operation, void *pDevice = nullptr, void *pData = nullptr, VideoSystem_t videoSystem = VideoSystem::ALL_VIDEO_SYSTEMS );

		// Get the name of a codec as a string
		const wchar_t					*GetCodecName( VideoEncodeCodec_t nCodec );

	private:
	
		VideoResult_t					ResolveToPlayableVideoFile( const char *pFileName, const char *pPathID, VideoSystem_t videoSystem, VideoSystemFeature_t requiredFeature, bool PlayAlternateIfNotAvailable, 
																	char *pResolvedFileName, int resolvedFileNameMaxLen, VideoSystem_t *pResolvedVideoSystem );
	
	
		VideoSystem_t					LocateSystemAndFeaturesForFileName( const char *pFileName, VideoSystemFeature_t *pFeatures = nullptr, VideoSystemFeature_t requiredFeatures = VideoSystemFeature::NO_FEATURES );
		
		bool							IsMatchAnyExtension( const char *pFileName );
	
		bool							ConnectVideoLibraries( CreateInterfaceFn factory );
		bool							DisconnectVideoLibraries();
		
		int								DestroyAllVideoInterfaces();

		int								GetIndexForSystem( VideoSystem_t n );
		VideoSystem_t					GetSystemForIndex( int n );
		
		VideoResult_t					SetResult( VideoResult_t resultCode );
		
		const char					   *GetFileExtension( const char *pFileName );
		
		
		static const int				SYSTEM_NOT_FOUND = -1;
		
		VideoResult_t					m_LastResult;
		
		int								m_nInstalledSystems;
		bool							m_bInitialized;
		
		CSysModule					   *m_VideoSystemModule[VideoSystem::VIDEO_SYSTEM_COUNT];
		IVideoSubSystem				   *m_VideoSystems[VideoSystem::VIDEO_SYSTEM_COUNT];
		VideoSystem_t					m_VideoSystemType[VideoSystem::VIDEO_SYSTEM_COUNT];
		VideoSystemFeature_t			m_VideoSystemFeatures[VideoSystem::VIDEO_SYSTEM_COUNT];
		
		CUtlVector< VideoFileExtensionInfo_t >	m_ExtInfo;			// info about supported file extensions
		
		CUtlVector< CActiveVideoObjectRecord_t > m_RecorderList;
		CUtlVector< CActiveVideoObjectRecord_t > m_MaterialList;
		
		int								m_nMaterialCount;				
			
};


class CVideoCommonServices : public IVideoCommonServices
{
	public:
	
		CVideoCommonServices();
		~CVideoCommonServices();
	
	
		virtual bool			CalculateVideoDimensions( int videoWidth, int videoHeight, int displayWidth, int displayHeight, VideoPlaybackFlags_t playbackFlags, 
													  int *pOutputWidth, int *pOutputHeight, int *pXOffset, int *pYOffset );

		virtual	float			GetSystemVolume();
													  
		virtual VideoResult_t	InitFullScreenPlaybackInputHandler( VideoPlaybackFlags_t playbackFlags, float forcedMinTime, bool windowed );
		
		virtual bool			ProcessFullScreenInput( bool &bAbortEvent, bool &bPauseEvent, bool &bQuitEvent );
		
		virtual VideoResult_t	TerminateFullScreenPlaybackInputHandler();


	private:
		
		void					ResetInputHandlerState();
	
		bool					m_bInputHandlerInitialized;
	
		bool					m_bScanAll;
		bool					m_bScanEsc;
		bool					m_bScanReturn;
		bool					m_bScanSpace;
		bool					m_bPauseEnabled;
		bool					m_bAbortEnabled;
		bool					m_bEscLast;
		bool					m_bReturnLast;
		bool					m_bSpaceLast;
		bool					m_bForceMinPlayTime;
		
		bool					m_bWindowed;
		VideoPlaybackFlags_t	m_playbackFlags;
		float					m_forcedMinTime;
		
		double					m_StartTime;
		

};


#endif		// VIDEOSERVICES_H
