//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#ifndef BINK_MATERIAL_H
#define BINK_MATERIAL_H

#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IFileSystem;
class IMaterialSystem;
class CBinkMaterial;

//-----------------------------------------------------------------------------
// Global interfaces - you already did the needed includes, right?
//-----------------------------------------------------------------------------
extern IFileSystem		*g_pFileSystem;
extern IMaterialSystem	*materials;

#include "video/ivideoservices.h"

#include "video_macros.h"
#include "bink_common.h"

#include "materialsystem/itexture.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/MaterialSystemUtil.h"

extern "C"
{
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

// -----------------------------------------------------------------------------
// Texture regenerator - callback to get new movie pixels into the texture
// -----------------------------------------------------------------------------
class CBinkMaterialRGBTextureRegenerator : public ITextureRegenerator
{
	public:
		CBinkMaterialRGBTextureRegenerator();
		~CBinkMaterialRGBTextureRegenerator();
	
		void				SetSourceImage( uint8_t *SrcImage, int nWidth, int nHeight );

		// Inherited from ITextureRegenerator
		virtual void		RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect );
		virtual void		Release();

	private:
		uint8_t				*m_SrcImage;
		int					m_nSourceWidth;
		int					m_nSourceHeight;
};



// -----------------------------------------------------------------------------
// Class used to play a QuickTime video onto a texture 
// -----------------------------------------------------------------------------
class CBinkMaterial : public IVideoMaterial
{
	public:
		CBinkMaterial();
		~CBinkMaterial();
		
		static const int			MAX_FILENAME_LEN = 255;
		static const int			MAX_MATERIAL_NAME_LEN = 255;	
		static const int			TEXTURE_SIZE_ALIGNMENT = 8;

		// Initializes, shuts down the material
		bool						Init( const char *pMaterialName, const char *pFileName, VideoPlaybackFlags_t flags );
		void						Shutdown();

		// Video information functions		
		virtual const char		   *GetVideoFileName();									// Gets the file name of the video this material is playing
		virtual VideoResult_t		GetLastResult();									// Gets detailed info on the last operation
		
		virtual VideoFrameRate_t   &GetVideoFrameRate();								// Returns the frame rate of the associated video in FPS

		// Audio Functions
		virtual bool				HasAudio();											// Query if the video has an audio track
		
		virtual bool				SetVolume( float fVolume );							// Adjust the playback volume
		virtual float				GetVolume();										// Query the current volume
		virtual void				SetMuted( bool bMuteState );						// Mute/UnMutes the audio playback
		virtual bool				IsMuted();											// Query muted status
		
		virtual VideoResult_t		SoundDeviceCommand( VideoSoundDeviceOperation_t operation, void *pDevice = nullptr, void *pData = nullptr );		// Assign Sound Device for this Video Material
		
		// Video playback state functions
		virtual bool				IsVideoReadyToPlay();								// Queries if the video material was initialized successfully and is ready for playback, but not playing or finished
		virtual bool				IsVideoPlaying();									// Is the video currently playing (and needs update calls, etc)
		virtual bool				IsNewFrameReady();									// Do we have a new frame to get & display?
		virtual bool				IsFinishedPlaying();								// Have we reached the end of the movie

		virtual bool				StartVideo();										// Starts the video playing
		virtual bool				StopVideo();										// Terminates the video playing

		virtual void				SetLooping( bool bLoopVideo );						// Sets the video to loop (or not)
		virtual bool				IsLooping();										// Queries if the video is looping
		
		virtual void				SetPaused( bool bPauseState );						// Pauses or Unpauses video playback
		virtual bool				IsPaused();											// Queries if the video is paused

		// Position in playback functions
		virtual float				GetVideoDuration();									// Returns the duration of the associated video in seconds
		virtual int					GetFrameCount();									// Returns the total number of (unique) frames in the video
		
		virtual bool				SetFrame( int FrameNum );							// Sets the current frame # in the video to play next 
		virtual int					GetCurrentFrame();									// Gets the current frame # for the video playback, 0 Based
		
		virtual bool				SetTime( float flTime );							// Sets the video playback to specified time (in seconds)
		virtual float				GetCurrentVideoTime();								// Gets the current time in the video playback
		
		// Update function
		virtual bool				Update();											// Updates the video frame to reflect the time passed, true = new frame available

		// Material / Texture Info functions
		virtual IMaterial		   *GetMaterial();										// Gets the IMaterial associated with an video material

		virtual void				GetVideoTexCoordRange( float *pMaxU, float *pMaxV ) ;		// Returns the max texture coordinate of the video portion of the material surface ( 0.0, 0.0 to U, V )
		virtual void				GetVideoImageSize( int *pWidth, int *pHeight );				// Returns the frame size of the Video Image Frame in pixels ( the stored in a subrect of the material itself)

	private:
		friend class CBinkMaterialRGBTextureRegenerator;

		void 						Reset();											// clears internal state
		void 						SetFileName( const char *theMovieFileName );
		VideoResult_t				SetResult( VideoResult_t status );
		
		// Initializes, shuts down the video stream
		void 						OpenMovie( const char *theMovieFileName );
		void 						CloseFile();

		// Initializes, shuts down the procedural texture
		void						CreateProceduralTexture( const char *pTextureName );
		void						DestroyProceduralTexture();

		// Initializes, shuts down the procedural material
		void 						CreateProceduralMaterial( const char *pMaterialName );
		void 						DestroyProceduralMaterial();

		CBinkMaterialRGBTextureRegenerator	m_TextureRegen;

		VideoResult_t				m_LastResult;
		
		CMaterialReference			m_Material;						// Ref to Material used for rendering the video frame
		CTextureReference			m_Texture;						// Ref to the renderable texture which contains the most recent video frame (in a sub-rect)

		float						m_TexCordU;						// Max U texture coordinate of the texture sub-rect which holds the video frame
		float						m_TexCordV;						// Max V texture coordinate of the texture sub-rect which holds the video frame

		int							m_VideoFrameWidth;				// Size of the movie frame in pixels
		int							m_VideoFrameHeight;

		char					   *m_pFileName;					// resolved filename of the movie being played
		VideoPlaybackFlags_t		m_PlaybackFlags;				// option flags user supplied

		bool						m_bInitCalled;
		bool						m_bMovieInitialized;
		bool						m_bMoviePlaying;
		bool						m_bMovieFinishedPlaying;
		bool						m_bMoviePaused;
		bool						m_bLoopMovie;
		
		bool						m_bHasAudio;
		bool						m_bMuted;
		
		float						m_CurrentVolume;
		
		// QuickTime Stuff

		float					m_QTMovieTimeScale;
		float					m_QTMoviefloat;				// Units per second
		float					m_QTMovieDuration;				// movie duration in float Units Per Second
		float						m_QTMovieDurationinSec;			// movie duration in seconds
		VideoFrameRate_t			m_QTMovieFrameRate;				// Frame Rate of movie
		int							m_QTMovieFrameCount;

		double			m_MovieFirstFrameTime;
		double			m_NextInterestingTimeToPlay;
		float					m_MoviePauseTime; 

		// AV stuff
		AVFrame *m_AVFrame;
		AVPacket *m_AVPkt;
		AVFormatContext *m_AVFmtCtx;

		int m_AVVideoStreamID;
		int m_AVAudioStreamID;

		AVCodecContext *m_AVVideoDecCtx ;
		AVCodecContext *m_AVAudioDecCtx ;

		AVStream *m_AVVideoStream;
		AVStream *m_AVAudioStream;

		int m_AVPixFormat;

		double m_MovieFrameDuration;

		uint8_t *m_AVVideoData[4];
		uint8_t *m_RGBData;

		int m_AVVideoLinesize[4];

};

#endif // BINK_MATERIAL_H
