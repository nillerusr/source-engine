//========= Copyright Valve Corporation, All rights reserved. ============//
//
//----------------------------------------------------------------------------------------

#ifndef QUICKTIME_RECORDER_H
#define QUICKTIME_RECORDER_H

#ifdef _WIN32
#pragma once
#endif

//--------------------------------------------------------------------------------

#if defined( OSX )
	#include <quicktime/QTML.h>
	#include <quicktime/Movies.h>
	#include <quicktime/QuickTimeComponents.h>
#elif defined( WIN32 )
	#include "QTML.h"
	#include "Movies.h"
	#include "QuickTimeComponents.h"
#else
    #error "Quicktime encoding is not supported on this platform"
#endif

#include "video/ivideoservices.h"

#include "video_macros.h"
#include "quicktime_common.h"


// comment out to prevent logging of creation data
//#define LOG_ENCODER_OPERATIONS
//#define LOG_ENCODER_AUDIO_OPERATIONS

// comment out to log images of frames
//#define LOG_FRAMES_TO_TGA
//#define LOG_FRAMES_TO_TGA_INTERVAL   16

#if defined( LOG_ENCODER_OPERATIONS ) || defined( LOG_ENCODER_AUDIO_OPERATIONS ) ||  defined ( LOG_FRAMES_TO_TGA ) || defined ( ENABLE_EXTERNAL_ENCODER_LOGGING )
	#include <filesystem.h>
#endif



// ------------------------------------------------------------------------
// CQTVideoFileComposer
// Class to manages to creation of a video file from a series of
// supplied bitmap images.
//
// At this time, the time interval between frames must be the same for all
// frames of the movie
// ------------------------------------------------------------------------
class CQTVideoFileComposer
{
	public:
		static const CodecType		DEFAULT_CODEC = kH264CodecType;
		static const CodecQ			DEFAULT_ENCODE_QUALITY = codecNormalQuality;
		static const Fixed			DEFAULT_GAMMA = kQTUseSourceGammaLevel;
		
		static const int			MIN_AUDIO_SAMPLE_GROUP_SIZE = 64;
		static const int			MAX_AUDIO_GROUP_SIZE_IN_SEC = 4;
		
		CQTVideoFileComposer();
		~CQTVideoFileComposer();

		bool				CreateNewMovie( const char *fileName, bool hasAudio );

		bool				SetMovieVideoParameters( int width, int height, VideoFrameRate_t movieFPS, VideoEncodeCodec_t desiredCodec, int encodeQuality, VideoEncodeGamma_t gamma );
		bool				SetMovieSourceImageParameters( int srcWidth, int srcHeight, VideoEncodeSourceFormat_t srcImageFormat );
		bool				SetMovieSourceAudioParameters( AudioEncodeSourceFormat_t srcAudioFormat = AudioEncodeSourceFormat::AUDIO_NONE, int audioSampleRate = 0, AudioEncodeOptions_t audioOptions = AudioEncodeOptions::NO_AUDIO_OPTIONS, int audioSampleGroupSize = 0 );

		bool				AppendVideoFrameToMedia( void *ImageBuffer, int strideAdjustBytes );
		bool				AppendAudioSamplesToMedia( void *soundBuffer, size_t bufferSize );
	
		bool				AbortMovie();
		bool				FinishMovie( bool SaveMovieToDisk = true );
		
		size_t				GetFrameBufferSize()						{ return m_SrcImageSize; }
		
		bool				CheckFilename( const char *filename );
	
		void				SetResult( VideoResult_t status );
		VideoResult_t		GetResult();
		bool				IsReadyToRecord();
		
		int					GetFrameCount()			{ return m_nFramesAdded; }
		int					GetSampleCount()		{ return m_nSamplesAdded; }
		
		VideoFrameRate_t	GetFPS()				{ return m_MovieRecordFPS; }
		int					GetSampleRate()			{ return m_AudioSourceFrequency; }
		
		bool				HasAudio()				{ return m_bHasAudioTrack; }
	
#ifdef ENABLE_EXTERNAL_ENCODER_LOGGING
		bool				LogMessage(  const char *msg );
#endif
	
	private:
		enum AudioGrouping_t
		{
			AG_NONE = 0,
			AG_FIXED_SIZE,
			AG_PER_FRAME
		};
	
		// disable copy constructor and copy assignment
		CQTVideoFileComposer( CQTVideoFileComposer &rhs );
		CQTVideoFileComposer&	operator= ( CQTVideoFileComposer &rhs );
		
		bool				CheckForReadyness();
		bool				BeginMovieCreation();
		bool				EndMovieCreation( bool saveMovieData );

		bool				SyncAndFlushAudio();
		int					GetAudioSampleCountThruFrame( int frameNo );

		VideoResult_t		m_LastResult;

		// Current State of Movie Creation;
		bool				m_bMovieCreated;
		bool				m_bHasAudioTrack;
		
		bool				m_bMovieConfigured;
		bool				m_bSourceImagesConfigured;
		bool				m_bSourceAudioConfigured;
		
		bool				m_bComposingMovie;
		bool				m_bMovieCompleted;
		
		int					m_nFramesAdded;
		
		int					m_nAudioFramesAdded;
		int					m_nSamplesAdded;
		int					m_nSamplesAddedToMedia;

		// parameters of the movie to create;		
		int					m_MovieFrameWidth;
		int					m_MovieFrameHeight;
		
		VideoFrameRate_t	m_MovieRecordFPS;
		TimeScale			m_MovieTimeScale;
		TimeValue			m_DurationPerFrame;

		// Audio recording options		
		AudioEncodeOptions_t m_AudioOptions;					// Option flags specifed by user
		AudioGrouping_t		m_SampleGrouping;					// Mode to group samples
		int 				m_nAudioSampleGroupSize;			// number of samples to collect per sample group
		
		int					m_AudioSourceFrequency;				// Source frequency of the supplied audio
		int					m_AudioBytesPerSample;

		bool				m_bBufferSourceAudio;
		bool				m_bLimitAudioDurationToVideo;
		
		byte			   *m_srcAudioBuffer;					// buffer to hold audio samples
		size_t				m_srcAudioBufferSize;
		size_t				m_srcAudioBufferCurrentSize;
		
		int					m_AudioSampleFrameCounter;			

		char			   *m_FileName;

		int					m_SrcImageWidth;
		int					m_SrcImageHeight;
		size_t				m_SrcImageSize;
		int					m_ScrImageMaxCompressedSize;
		byte			   *m_SrcImageBuffer;
		Handle				m_SrcImageCompressedBuffer;
		OSType				m_SrcPixelFormat;
		int					m_SrcBytesPerPixel;
		
		OSType				m_GWorldPixelFormat;		
		int					m_GWorldBytesPerPixel;
		int					m_GWorldImageWidth;
		int					m_GWorldImageHeight;

		Handle				m_srcSoundDescription;


		// parameters used by QuickTime
		CodecQ              m_EncodeQuality;
		CodecType			m_VideoCodecToUse;
		Fixed				m_EncodeGamma;
		
		Rect				m_GWorldRect;
		GWorldPtr			m_theSrcGWorld;
		
//		short				m_ResRefNum;			// QuickTime Movie Resource Ref number

		Handle				m_MovieFileDataRef;
		OSType				m_MovieFileDataRefType;
		DataHandler			m_MovieFileDataHandler;

		
		Movie				m_theMovie;
		Track				m_theVideoTrack;
		Track				m_theAudioTrack;
		Media				m_theVideoMedia;
		Media				m_theAudioMedia;
		
#ifdef LOG_ENCODER_OPERATIONS
		FileHandle_t		m_LogFile;
		void				LogMsg( PRINTF_FORMAT_STRING const char* pMsg, ... );
#endif	

#ifdef LOG_FRAMES_TO_TGA
		char				m_TGAFileBase[MAX_PATH];
#endif		
		

};



class CQuickTimeVideoRecorder : public IVideoRecorder
{
	public:
								CQuickTimeVideoRecorder();
							   ~CQuickTimeVideoRecorder();

		virtual bool			EstimateMovieFileSize( size_t *pEstSize, int movieWidth, int movieHeight, VideoFrameRate_t movieFps, float movieDuration, VideoEncodeCodec_t theCodec, int videoQuality,  AudioEncodeSourceFormat_t srcAudioFormat = AudioEncodeSourceFormat::AUDIO_NONE, int audioSampleRate = 0 );

		virtual bool			CreateNewMovieFile( const char *pFilename, bool hasAudioTrack = false );
		
		virtual bool			SetMovieVideoParameters( VideoEncodeCodec_t theCodec, int videoQuality,  int movieFrameWidth, int movieFrameHeight, VideoFrameRate_t movieFPS, VideoEncodeGamma_t gamma = VideoEncodeGamma::NO_GAMMA_ADJUST );
		virtual bool			SetMovieSourceImageParameters( VideoEncodeSourceFormat_t srcImageFormat, int imgWidth, int imgHeight );
		virtual bool			SetMovieSourceAudioParameters( AudioEncodeSourceFormat_t srcAudioFormat = AudioEncodeSourceFormat::AUDIO_NONE, int audioSampleRate = 0, AudioEncodeOptions_t audioOptions = AudioEncodeOptions::NO_AUDIO_OPTIONS, int audioSampleGroupSize = 0 );
		
		virtual bool			IsReadyToRecord();
		virtual VideoResult_t	GetLastResult();
		
		virtual bool			AppendVideoFrame( void *pFrameBuffer, int nStrideAdjustBytes = 0 );
		virtual bool			AppendAudioSamples( void *pSampleBuffer, size_t sampleSize );
		
		virtual int				GetFrameCount();
		virtual int				GetSampleCount();
		virtual int				GetSampleRate();
		virtual VideoFrameRate_t GetFPS();
		
		virtual bool			AbortMovie();
		virtual bool			FinishMovie( bool SaveMovieToDisk = true );

#ifdef ENABLE_EXTERNAL_ENCODER_LOGGING
		virtual bool            LogMessage(  const char *msg );
#endif
					   
	private:
		
		void					SetResult( VideoResult_t resultCode );
		
		float					GetDataRate( int quality, int width, int height );
		
		CQTVideoFileComposer	*m_pEncoder;
		VideoResult_t			m_LastResult;
		bool					m_bHasAudio;
		bool					m_bMovieFinished;
		
};



#endif // QUICKTIME_RECORDER_H
