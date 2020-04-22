//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================


#include "filesystem.h"
#include "tier1/strtools.h"
#include "tier1/utllinkedlist.h"
#include "tier1/KeyValues.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/itexture.h"
#include "vtf/vtf.h"
#include "pixelwriter.h"
#include "tier3/tier3.h"
#include "platform.h"
#include "bink_material.h"
#include "tier0/memdbgon.h"

extern "C" {
#include "yuv_rgb.h"
}


// makes a copy of a string
char *COPY_STRING( const char *pString )
{
	if ( pString == nullptr )
		return nullptr;

	size_t strLen = V_strlen( pString );

	char *pNewStr = new char[ strLen+ 1 ];
	if ( strLen > 0 )
		V_memcpy( pNewStr, pString, strLen );

	pNewStr[strLen] = nullchar;

	return pNewStr;
}

int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
	int ret, stream_index;
	AVStream *st;
	const AVCodec *dec = NULL;

	ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
	if (ret < 0)
	{
		Warning("Could not find %s stream\n",
				av_get_media_type_string(type));
		return ret;
	}
	else
	{
		stream_index = ret;
		st = fmt_ctx->streams[stream_index];

		/* find decoder for the stream */
		dec = avcodec_find_decoder(st->codecpar->codec_id);
		if (!dec)
		{
			Warning("Failed to find %s codec\n",
					av_get_media_type_string(type));
			return AVERROR(EINVAL);
		}

		/* Allocate a codec context for the decoder */
		*dec_ctx = avcodec_alloc_context3(dec);
		if (!*dec_ctx)
		{
			Warning("Failed to allocate the %s codec context\n",
					av_get_media_type_string(type));
			return AVERROR(ENOMEM);
		}

		/* Copy codec parameters from input stream to output codec context */
		if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0)
		{
			Warning("Failed to copy %s codec parameters to decoder context\n",
					av_get_media_type_string(type));
			return ret;
		}

		/* Init the decoders */
		if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0)
		{
			Warning("Failed to open %s codec\n",
					av_get_media_type_string(type));
			return ret;
		}

		*stream_idx = stream_index;
	}

	return 0;
}

// ===========================================================================
// CBinkMaterialRGBTextureRegenerator - Inherited from ITextureRegenerator
//	Copies and converts the buffer bits to texture bits
//	Currently only supports 32-bit BGR
// ===========================================================================
CBinkMaterialRGBTextureRegenerator::CBinkMaterialRGBTextureRegenerator() :
	m_nSourceWidth( 0 ),
	m_nSourceHeight( 0 )
{
}


CBinkMaterialRGBTextureRegenerator::~CBinkMaterialRGBTextureRegenerator() 
{
	// nothing to do
}

void CBinkMaterialRGBTextureRegenerator::SetSourceImage( uint8_t *SrcImage, int nWidth, int nHeight )
{
	m_SrcImage = SrcImage;
	m_nSourceWidth	= nWidth;
	m_nSourceHeight = nHeight;
}

void CBinkMaterialRGBTextureRegenerator::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
{
	AssertExit( pVTFTexture != nullptr );

	// Error condition, should only have 1 frame, 1 face, 1 mip level
	if ( ( pVTFTexture->FrameCount() > 1 ) || ( pVTFTexture->FaceCount() > 1 ) || ( pVTFTexture->MipCount() > 1 ) || ( pVTFTexture->Depth() > 1 ) )
	{
		WarningAssert( "Texture Properties Incorrect ");
		memset( pVTFTexture->ImageData(), 0xAA, pVTFTexture->ComputeTotalSize() );
		return;
	}

	// Make sure we have a valid video image source	
/*	if ( m_SrcGWorld == nullptr )
	{
		WarningAssert( "Video texture source not set" );
		memset( pVTFTexture->ImageData(), 0xCC, pVTFTexture->ComputeTotalSize() );
		return;
	}*/

	// Verify the destination texture is set up correctly
	Assert( pVTFTexture->Format() == IMAGE_FORMAT_RGB888 );
	Assert( pVTFTexture->RowSizeInBytes( 0 ) >= pVTFTexture->Width() * 4 );
	Assert( pVTFTexture->Width() >= m_nSourceWidth );
	Assert( pVTFTexture->Height() >= m_nSourceHeight );

	// Copy directly from the Quicktime GWorld
	BYTE   *pImageData	= pVTFTexture->ImageData();
	int dstStride = pVTFTexture->RowSizeInBytes( 0 );

	BYTE *pSrcData = m_SrcImage;

	for (int y = 0; y < m_nSourceHeight; y++ )
	{
		memcpy( pImageData, pSrcData, m_nSourceWidth*3 );

		pImageData += dstStride;
		pSrcData += m_nSourceWidth*3;
	}
}


void CBinkMaterialRGBTextureRegenerator::Release()
{
	// we don't invoke the destructor here, we're not using the no-release extensions
}



// ===========================================================================
// CBinkMaterial class - creates a material, opens a QuickTime movie
//		   and plays the movie onto the material
// ===========================================================================

//-----------------------------------------------------------------------------
// CBinkMaterial Constructor
//-----------------------------------------------------------------------------
CBinkMaterial::CBinkMaterial() :
	m_pFileName( nullptr ),
	m_bInitCalled( false ),
	m_AVFrame( nullptr ),
	m_AVPkt( nullptr )
{
	memset( m_AVVideoData, 0, sizeof(m_AVVideoData) );
	memset( m_AVVideoLinesize, 0, sizeof(m_AVVideoLinesize) );

	Reset();
}


//-----------------------------------------------------------------------------
// CBinkMaterial Destructor
//-----------------------------------------------------------------------------
CBinkMaterial::~CBinkMaterial()
{
	SetFileName( nullptr );

	DestroyProceduralTexture();
	DestroyProceduralMaterial();

	av_frame_free( &m_AVFrame );
	av_packet_free( &m_AVPkt );

	if( m_AVVideoData[0] )
		av_free(m_AVVideoData[0]);

	if( m_AVFmtCtx )
		avformat_close_input( &m_AVFmtCtx );
}


void CBinkMaterial::Reset()
{
	printf("CBinkMaterial::Reset()\n");

	SetFileName( nullptr );

	DestroyProceduralTexture();
	DestroyProceduralMaterial();

	m_TexCordU = 0.0f;
	m_TexCordV = 0.0f;

	m_VideoFrameWidth = 0;
	m_VideoFrameHeight = 0;

	m_AVPixFormat = 0;
	m_PlaybackFlags = VideoPlaybackFlags::NO_PLAYBACK_OPTIONS;

	m_bMovieInitialized = false;
	m_bMoviePlaying = false;
	m_bMovieFinishedPlaying = false;
	m_bMoviePaused = false;
	m_bLoopMovie = false;

	m_bHasAudio = false;
	m_bMuted = false;

	m_CurrentVolume = 0.0f;

	m_QTMovieTimeScale = 0;
	m_QTMovieDuration = 0;
	m_QTMovieDurationinSec = 0.0f;
	m_QTMovieFrameRate.SetFPS( 0, false );

	if( !m_AVFrame )
		m_AVFrame = av_frame_alloc();
	if( !m_AVPkt)
		m_AVPkt = av_packet_alloc();

	m_RGBData = nullptr;

	m_AVFmtCtx = nullptr;
	m_AVAudioStream = nullptr;
	m_AVVideoStream = nullptr;

	AssertMsg( m_AVFrame, "av_frame_alloc return nullptr\n" );
	AssertMsg( m_AVPkt, "av_packet_alloc return nullptr\n"  );

	if( m_AVVideoData[0] )
	{
		av_free(m_AVVideoData[0]);
		m_AVVideoData[0] = nullptr;
	}

	if( m_AVFmtCtx )
	{
		avformat_close_input( &m_AVFmtCtx );
		m_AVFmtCtx = nullptr;
	}

	m_LastResult = VideoResult::SUCCESS;
}


void CBinkMaterial::SetFileName( const char *theMovieFileName )
{
	SAFE_DELETE_ARRAY( m_pFileName );

	if ( theMovieFileName != nullptr )
	{
		AssertMsg( V_strlen( theQTMovieFileName ) <= MAX_FILENAME_LEN, "Bad Quicktime Movie Filename" );
		m_pFileName = COPY_STRING( theMovieFileName );
	}
}


VideoResult_t CBinkMaterial::SetResult( VideoResult_t status )
{
	m_LastResult = status;
	return status;
}


//-----------------------------------------------------------------------------
// Video information functions
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Returns the resolved filename of the video, as it might differ from
// what the user supplied, (also with absolute path)
//-----------------------------------------------------------------------------
const char *CBinkMaterial::GetVideoFileName()
{
	return m_pFileName;
}


VideoFrameRate_t &CBinkMaterial::GetVideoFrameRate()
{
	return m_QTMovieFrameRate;
}


VideoResult_t CBinkMaterial::GetLastResult()
{
	return m_LastResult;
}


//-----------------------------------------------------------------------------
// Audio Functions
//-----------------------------------------------------------------------------
bool CBinkMaterial::HasAudio()
{
	return m_bHasAudio;
}


bool CBinkMaterial::SetVolume( float fVolume )
{
	clamp( fVolume, 0.0f, 1.0f );

	m_CurrentVolume = fVolume;

	SetResult( VideoResult::AUDIO_ERROR_OCCURED );
	return false;
}


float CBinkMaterial::GetVolume()
{
	return m_CurrentVolume;
}


void CBinkMaterial::SetMuted( bool bMuteState )
{
	AssertExitFunc( m_bMoviePlaying, SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE) );

	SetResult( VideoResult::SUCCESS );

	if ( bMuteState == m_bMuted  )		// no change?
	{
		return;
	}

	m_bMuted = bMuteState;

	if ( m_bHasAudio )
	{

	}

	SetResult( VideoResult::SUCCESS );
}


bool CBinkMaterial::IsMuted()
{
	return m_bMuted;
}


VideoResult_t CBinkMaterial::SoundDeviceCommand( VideoSoundDeviceOperation_t operation, void *pDevice, void *pData )
{
	AssertExitV( m_bMovieInitialized || m_bMoviePlaying, VideoResult::OPERATION_OUT_OF_SEQUENCE );

	switch( operation )
	{
		// On win32, we try and create an audio context from a GUID
		case VideoSoundDeviceOperation::SET_DIRECT_SOUND_DEVICE:
		{
#if defined ( WIN32 )
			SAFE_RELEASE_AUDIOCONTEXT( m_AudioContext );
			return ( CreateMovieAudioContext( m_bHasAudio, m_QTMovie, &m_AudioContext ) ? SetResult( VideoResult::SUCCESS ) : SetResult( VideoResult::AUDIO_ERROR_OCCURED ) );
#else
			// On any other OS, we don't support this operation
			return SetResult( VideoResult::OPERATION_NOT_SUPPORTED );
#endif
		}
		case VideoSoundDeviceOperation::SET_SOUND_MANAGER_DEVICE:
		{
#if defined ( OSX )
			SAFE_RELEASE_AUDIOCONTEXT( m_AudioContext );
			return ( CreateMovieAudioContext( m_bHasAudio, m_QTMovie, &m_AudioContext ) ? SetResult( VideoResult::SUCCESS ) : SetResult( VideoResult::AUDIO_ERROR_OCCURED ) );
#else
			// On any other OS, we don't support this operation
			return SetResult( VideoResult::OPERATION_NOT_SUPPORTED );
#endif
		}
		case VideoSoundDeviceOperation::SET_LIB_AUDIO_DEVICE:
		case VideoSoundDeviceOperation::HOOK_X_AUDIO:
		case VideoSoundDeviceOperation::SET_MILES_SOUND_DEVICE:
		{
			return SetResult( VideoResult::OPERATION_NOT_SUPPORTED );
		}
		default:
		{
			return SetResult( VideoResult::BAD_INPUT_PARAMETERS );
		}
	}

}


//-----------------------------------------------------------------------------
// Initializes the video material
//-----------------------------------------------------------------------------
bool CBinkMaterial::Init( const char *pMaterialName, const char *pFileName, VideoPlaybackFlags_t flags )
{
	printf("CBinkMaterial::Init\n");

	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( IS_NOT_EMPTY( pFileName ) );
	AssertExitF( m_bInitCalled == false );

	m_PlaybackFlags	= flags;

	OpenMovie( pFileName );	// Open up the Quicktime file

	if ( !m_bMovieInitialized )
	{
		return false;					// Something bad happened when we went to open
	}

	// Now we can properly setup our regenerators
//	m_TextureRegen.SetSourceGWorld( m_MovieGWorld, m_VideoFrameWidth, m_VideoFrameHeight );

	CreateProceduralTexture( pMaterialName );
	CreateProceduralMaterial( pMaterialName );

	// Start movie playback
	if ( !BITFLAGS_SET( m_PlaybackFlags, VideoPlaybackFlags::DONT_AUTO_START_VIDEO ) )
	{
		StartVideo();
	}

	m_bInitCalled = true;				// Look, if you only got one shot...

	return true;
}


void CBinkMaterial::Shutdown( void )
{
	StopVideo();
	Reset();
}


//-----------------------------------------------------------------------------
// Video playback state functions
//-----------------------------------------------------------------------------
bool CBinkMaterial::IsVideoReadyToPlay()
{
	return m_bMovieInitialized;
}


bool CBinkMaterial::IsVideoPlaying()
{
	return m_bMoviePlaying;
}


//-----------------------------------------------------------------------------
// Checks to see if the video has a new frame ready to be rendered and 
// downloaded into the texture and eventually display
//-----------------------------------------------------------------------------
bool CBinkMaterial::IsNewFrameReady( void )
{
	// Are we waiting to start playing the first frame? if so, tell them we are ready!
	if ( m_bMovieInitialized == true  )
	{
		return true;
	}

	// paused?
	if ( m_bMoviePaused )
	{
		return false;
	}

//	float curMovieTime;
	// Enough time passed to get to next frame??
/*	if ( curMovieTime < m_NextInterestingTimeToPlay )
	{
		// nope.. use the previous frame
		return false;
	}*/

	// we have a new frame we want then..
	return true;
}


bool CBinkMaterial::IsFinishedPlaying()
{
	return m_bMovieFinishedPlaying;
}


void CBinkMaterial::SetLooping( bool bLoopVideo )
{
	m_bLoopMovie = bLoopVideo;
}


bool CBinkMaterial::IsLooping()
{
	return m_bLoopMovie;
}


void CBinkMaterial::SetPaused( bool bPauseState )
{
	if ( !m_bMoviePlaying || m_bMoviePaused == bPauseState )
	{
		Assert( m_bMoviePlaying );
		return;
	}

	if ( bPauseState )			// Pausing the movie?
	{
		// Save off current time and set paused state
//		m_MoviePauseTime = GetMovieTime( m_QTMovie, nullptr );
//		StopMovie( m_QTMovie );
	}
	else  // unpausing the movie
	{
		// Reset the movie to the paused time
//		SetMovieTimeValue( m_QTMovie, m_MoviePauseTime );
//		StartMovie( m_QTMovie );
//		Assert( GetMoviesError() == noErr );
	}

	m_bMoviePaused = bPauseState;
}


bool CBinkMaterial::IsPaused()
{
	return ( m_bMoviePlaying ) ? m_bMoviePaused : false;
}


// Begins playback of the movie
bool CBinkMaterial::StartVideo()
{
	if ( !m_bMovieInitialized )
	{
		Assert( false );
		SetResult( VideoResult::OPERATION_ALREADY_PERFORMED );
		return false;
	}

	m_NextInterestingTimeToPlay = Plat_FloatTime();

	printf("Movie start time = %lf\n", Plat_FloatTime());

	// Transition to playing state
	m_bMovieInitialized = false;
	m_bMoviePlaying = true;

	Update();

	return true;
}


// stops movie for good, frees resources, but retains texture & material of last frame rendered
bool CBinkMaterial::StopVideo()
{
	if ( !m_bMoviePlaying )
	{
		SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
		return false;
	}

	m_bMoviePlaying = false;
	m_bMoviePaused = false;
	m_bMovieFinishedPlaying = true;

	// free resources
	CloseFile();

	SetResult( VideoResult::SUCCESS );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Updates our scene
// Output : true = movie playing ok, false = time to end movie
// supposed to be: Returns true on a new frame of video being downloaded into the texture
//-----------------------------------------------------------------------------
bool CBinkMaterial::Update( void )
{
	AssertExitF( m_bMoviePlaying );


	// are we paused? can't update if so...
	if ( m_bMoviePaused )
		return true;			// reuse the last frame

	// Get current time in the movie
	float curMovieTime; // = GetMovieTime( m_QTMovie, nullptr );

	if( m_NextInterestingTimeToPlay > Plat_FloatTime() )
		return true;

	m_NextInterestingTimeToPlay += m_MovieFrameDuration;

	/* read frames from the file */

	int ret;
	while( (ret = av_read_frame(m_AVFmtCtx, m_AVPkt)) >= 0 )
	{
		if (m_AVPkt->stream_index == m_AVVideoStreamID)
		{
			avcodec_send_packet(m_AVVideoDecCtx, m_AVPkt);

			ret = avcodec_receive_frame(m_AVVideoDecCtx, m_AVFrame);
			if (ret < 0)
			{
				av_packet_unref(m_AVPkt);
				return true;
			}

			// write the frame data to output file
			if (m_AVVideoDecCtx->codec->type == AVMEDIA_TYPE_VIDEO)
			{
				av_image_copy(m_AVVideoData, m_AVVideoLinesize, (const uint8_t **)(m_AVFrame->data), m_AVFrame->linesize, m_AVPixFormat, m_VideoFrameWidth, m_VideoFrameHeight);
			}

			av_frame_unref(m_AVFrame);
			break;
		}

		av_packet_unref(m_AVPkt);
	}


	if( ret < 0 )
	{
		StopVideo();
		return false;
	}



	yuv420_rgb24_std( m_VideoFrameWidth, m_VideoFrameHeight, m_AVVideoData[0],
			m_AVVideoData[0]+m_VideoFrameHeight*m_VideoFrameWidth,
			m_AVVideoData[0]+m_VideoFrameWidth*m_VideoFrameHeight+((m_VideoFrameWidth+1)/2)*((m_VideoFrameHeight+1)/2),
			m_VideoFrameWidth, (m_VideoFrameWidth+1)/2, m_RGBData, m_VideoFrameWidth*3, YCBCR_601
		);

	m_Texture->Download();

	SetResult( VideoResult::SUCCESS );
	return true;
}


//-----------------------------------------------------------------------------
// Returns the material
//-----------------------------------------------------------------------------
IMaterial *CBinkMaterial::GetMaterial()
{
	return m_Material;
}


//-----------------------------------------------------------------------------
// Returns the texcoord range
//-----------------------------------------------------------------------------
void CBinkMaterial::GetVideoTexCoordRange( float *pMaxU, float *pMaxV )
{
	AssertExit( pMaxU != nullptr && pMaxV != nullptr );

	if ( m_Texture == nullptr )		// no texture?
	{
		*pMaxU = *pMaxV = 1.0f;
		return;
	}

	*pMaxU = m_TexCordU;
	*pMaxV = m_TexCordV;
}


//-----------------------------------------------------------------------------
// Returns the frame size of the QuickTime Video in pixels 
//-----------------------------------------------------------------------------
void CBinkMaterial::GetVideoImageSize( int *pWidth, int *pHeight )
{
	Assert( pWidth != nullptr && pHeight != nullptr );

	*pWidth  = m_VideoFrameWidth;
	*pHeight = m_VideoFrameHeight;
}


float CBinkMaterial::GetVideoDuration()
{
	return m_QTMovieDurationinSec;
}


int CBinkMaterial::GetFrameCount()
{
	return m_QTMovieFrameCount;
}


//-----------------------------------------------------------------------------
// Sets the frame for an QuickTime  Material (use instead of SetTime)
//-----------------------------------------------------------------------------
bool CBinkMaterial::SetFrame( int FrameNum )
{
	if ( !m_bMoviePlaying )
	{
		Assert( false );
		SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
		return false;
	}

	float	theTime = (float) FrameNum * m_QTMovieFrameRate.GetFPS();
	return SetTime( theTime );
}


int CBinkMaterial::GetCurrentFrame()
{
	AssertExitV( m_bMoviePlaying, -1 );

	float curTime; // = m_bMoviePaused ? m_MoviePauseTime : GetMovieTime( m_QTMovie, nullptr );

	return curTime / m_QTMovieFrameRate.GetUnitsPerFrame();
}


float CBinkMaterial::GetCurrentVideoTime()
{
	AssertExitV( m_bMoviePlaying, -1.0f );

	float curTime; // = m_bMoviePaused ? m_MoviePauseTime : GetMovieTime( m_QTMovie, nullptr );

	return curTime / m_QTMovieFrameRate.GetUnitsPerSecond();
}


bool CBinkMaterial::SetTime( float flTime )
{
	AssertExitF( m_bMoviePlaying );
	AssertExitF( flTime >= 0 && flTime < m_QTMovieDurationinSec );

	float newTime = ( flTime * m_QTMovieFrameRate.GetUnitsPerSecond() + 0.5f) ;

	clamp( newTime,  m_MovieFirstFrameTime, m_QTMovieDuration ); 

	// Are we paused?
	if ( m_bMoviePaused )
	{
		m_MoviePauseTime = newTime;
		return true;
	}

	float curMovieTime; // = GetMovieTime( m_QTMovie, nullptr );

	// Don't stop and reset movie if we are within 1 frame of the requested time
	if ( newTime <= curMovieTime - m_QTMovieFrameRate.GetUnitsPerFrame() || newTime >= curMovieTime + m_QTMovieFrameRate.GetUnitsPerFrame() )
	{
		// Reset the movie to the requested time
/*		StopMovie( m_QTMovie );
		SetMovieTimeValue( m_QTMovie, newTime );
		StartMovie( m_QTMovie );

		Assert( GetMoviesError() == noErr );*/
	}

	return true;
}


//-----------------------------------------------------------------------------
// Initializes, shuts down the procedural texture
//-----------------------------------------------------------------------------
void CBinkMaterial::CreateProceduralTexture( const char *pTextureName )
{
	printf("CBinkMaterial::CreateProceduralTexture\n");

	AssertIncRange( m_VideoFrameWidth, cMinVideoFrameWidth, cMaxVideoFrameWidth );
	AssertIncRange( m_VideoFrameHeight, cMinVideoFrameHeight, cMaxVideoFrameHeight );
	AssertStr( pTextureName );

	// Either make the texture the same dimensions as the video,
	// or choose power-of-two textures which are at least as big as the video
	bool actualSizeTexture = BITFLAGS_SET( m_PlaybackFlags, VideoPlaybackFlags::TEXTURES_ACTUAL_SIZE );

	int nWidth  = ( actualSizeTexture ) ? ALIGN_VALUE( m_VideoFrameWidth, TEXTURE_SIZE_ALIGNMENT ) : ComputeGreaterPowerOfTwo( m_VideoFrameWidth ); 
	int nHeight = ( actualSizeTexture ) ? ALIGN_VALUE( m_VideoFrameHeight, TEXTURE_SIZE_ALIGNMENT ) : ComputeGreaterPowerOfTwo( m_VideoFrameHeight ); 

	// initialize the procedural texture as 32-it RGBA, w/o mipmaps
	m_Texture.InitProceduralTexture( pTextureName, "VideoCacheTextures", nWidth, nHeight, 
				IMAGE_FORMAT_RGB888, TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP |
				TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_NOLOD );

	// Use this to get the updated frame from the remote connection	
	m_Texture->SetTextureRegenerator( &m_TextureRegen /* , false */ );

	// compute the texcoords
	int nTextureWidth = m_Texture->GetActualWidth();
	int nTextureHeight = m_Texture->GetActualHeight();

	m_TexCordU = ( nTextureWidth > 0 ) ? (float) m_VideoFrameWidth / (float) nTextureWidth : 0.0f;
	m_TexCordV = ( nTextureHeight > 0 ) ? (float) m_VideoFrameHeight / (float) nTextureHeight : 0.0f;
}


void CBinkMaterial::DestroyProceduralTexture()
{
	if ( m_Texture != nullptr )
	{
		// DO NOT Call release on the Texture Regenerator, as it will destroy this object!  bad bad bad
		// instead we tell it to assign a NULL regenerator and flag it to not call release
		m_Texture->SetTextureRegenerator( nullptr /*, false */ );
		// Texture, texture go away...
		m_Texture.Shutdown( true );
	}
}


//-----------------------------------------------------------------------------
// Initializes, shuts down the procedural material
//-----------------------------------------------------------------------------
void CBinkMaterial::CreateProceduralMaterial( const char *pMaterialName )
{
	// create keyvalues if necessary
	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	{
		pVMTKeyValues->SetString( "$basetexture", m_Texture->GetName() );
		pVMTKeyValues->SetInt( "$nobasetexture", 1 );
		pVMTKeyValues->SetInt( "$nofog", 1 );
		pVMTKeyValues->SetInt( "$spriteorientation", 3 );
		pVMTKeyValues->SetInt( "$translucent", 1 );
		pVMTKeyValues->SetInt( "$nolod", 1 );
		pVMTKeyValues->SetInt( "$nomip", 1 );
		pVMTKeyValues->SetInt( "$gammacolorread", 0 );
	}

	// FIXME: gak, this is backwards.  Why doesn't the material just see that it has a funky basetexture?
	m_Material.Init( pMaterialName, pVMTKeyValues );
	m_Material->Refresh();
}


void CBinkMaterial::DestroyProceduralMaterial()
{
	// Store the internal material pointer for later use
	IMaterial *pMaterial = m_Material;
	m_Material.Shutdown();
	materials->UncacheUnusedMaterials();

	// Now be sure to free that material because we don't want to reference it again later, we'll recreate it!
	if ( pMaterial != nullptr )
	{
		pMaterial->DeleteIfUnreferenced();
	}
}



//-----------------------------------------------------------------------------
// Opens a movie file using quicktime
//-----------------------------------------------------------------------------
void CBinkMaterial::OpenMovie( const char *theMovieFileName )
{
	AssertExit( IS_NOT_EMPTY( theMovieFileName ) );
/*
	// Set graphics port 
#if defined ( WIN32 )
	SetGWorld ( (CGrafPtr) GetNativeWindowPort( nil ), nil ); 
#elif defined ( OSX		)
	SetGWorld( nil, nil );
#endif
*/

	SetFileName( theMovieFileName );
	printf("CBinkMaterial::OpenMovie( \"%s\" )\n", theMovieFileName);

	if (avformat_open_input(&m_AVFmtCtx, theMovieFileName, NULL, NULL) < 0)
 	{
		Warning("Could not open source file %s\n", theMovieFileName);
 		SetResult( VideoResult::FILE_ERROR_OCCURED ) ;
		Reset();
		return;
	}

	if (avformat_find_stream_info(m_AVFmtCtx, NULL) < 0)
	{
		Warning("Could not find stream information for %s\n", theMovieFileName);
 		SetResult( VideoResult::FILE_ERROR_OCCURED ) ;
		Reset();
		return;
	}

	if (open_codec_context(&m_AVVideoStreamID, &m_AVVideoDecCtx, m_AVFmtCtx, AVMEDIA_TYPE_VIDEO) == 0)
	{
		m_AVVideoStream = m_AVFmtCtx->streams[m_AVVideoStreamID];

		/* allocate image where the decoded image will be put */
		m_VideoFrameWidth = m_AVVideoDecCtx->width;
		m_VideoFrameHeight = m_AVVideoDecCtx->height;
		m_AVPixFormat = m_AVVideoDecCtx->pix_fmt;
		size_t size = av_image_alloc(m_AVVideoData, m_AVVideoLinesize,
							m_VideoFrameWidth, m_VideoFrameHeight, m_AVPixFormat, 1);

		m_RGBData = calloc( m_VideoFrameWidth*m_VideoFrameHeight*3, 1 );

		printf("m_AVVideoData size = %zu\nm_VideoFrameWidth=%d\nm_VideoFrameHeight=%d\n", size, m_VideoFrameWidth, m_VideoFrameHeight);

		if (size < 0)
		{
			Warning("Could not allocate raw video buffer\n", theMovieFileName);
 			SetResult( VideoResult::SYSTEM_ERROR_OCCURED ) ;
			Reset();
 			return;
		}
	}
	else
	{
			Warning("open_codec_context failed for %s\n", theMovieFileName);
 			SetResult( VideoResult::SYSTEM_ERROR_OCCURED ) ;
			Reset();
			return;
	}

	m_MovieFrameDuration = 1.0/((double)m_AVVideoStream->r_frame_rate.num/(double)m_AVVideoStream->r_frame_rate.den);
	m_TextureRegen.SetSourceImage( m_RGBData, m_VideoFrameWidth, m_VideoFrameHeight );
	printf("Video FPS: %lf\n", (double)m_AVVideoStream->r_frame_rate.num/(double)m_AVVideoStream->r_frame_rate.den);

#if 0
	Handle	MovieFileDataRef = nullptr;
	OSType	MovieFileDataRefType = 0;

	CFStringRef	imageStrRef = CFStringCreateWithCString ( NULL,  theQTMovieFileName, 0 ); 
	AssertExitFunc( imageStrRef != nullptr, SetResult( VideoResult::SYSTEM_ERROR_OCCURED ) );

	OSErr status = QTNewDataReferenceFromFullPathCFString( imageStrRef, (QTPathStyle) kQTNativeDefaultPathStyle, 0, &MovieFileDataRef, &MovieFileDataRefType );
	AssertExitFunc( status == noErr, SetResult( VideoResult::FILE_ERROR_OCCURED ) );

	CFRelease( imageStrRef );

//	status = NewMovieFromDataRef( &m_QTMovie, newMovieActive, nil, MovieFileDataRef, MovieFileDataRefType );
//	SAFE_DISPOSE_HANDLE( MovieFileDataRef );

	if ( status != noErr )
	{
		Assert( false );
		Reset();
		SetResult( VideoResult::VIDEO_ERROR_OCCURED );
		return;
	}

	// disabling audio?
	if ( BITFLAGS_SET( m_PlaybackFlags, VideoPlaybackFlags::NO_AUDIO ) )
	{
		m_bHasAudio = false;
	}
	else
	{
		// does movie have audio?
//		Track audioTrack = GetMovieIndTrackType( m_QTMovie, 1, SoundMediaType, movieTrackMediaType );
//		m_bHasAudio = ( audioTrack != nullptr );
	}

	// Now we need to extract the time info from the QT Movie 
//	m_QTMovieTimeScale	= GetMovieTimeScale( m_QTMovie );
//	m_QTMovieDuration	= GetMovieDuration( m_QTMovie );

	// compute movie duration	
/*	m_QTMovieDurationinSec = float ( double( m_QTMovieDuration ) / double( m_QTMovieTimeScale ) );
	if ( !MovieGetStaticFrameRate( m_QTMovie, m_QTMovieFrameRate ) )
	{
		WarningAssert( "Couldn't Get Frame Rate" );
	}*/
	
	// and get an estimated frame count
	m_QTMovieFrameCount = m_QTMovieDuration / m_QTMovieTimeScale;
	
	if ( m_QTMovieFrameRate.GetUnitsPerSecond() == m_QTMovieTimeScale )
	{
		m_QTMovieFrameCount = m_QTMovieDuration / m_QTMovieFrameRate.GetUnitsPerFrame();
	}
	else
	{
		m_QTMovieFrameCount = (int) ( (float) m_QTMovieDurationinSec * m_QTMovieFrameRate.GetFPS() + 0.5f );
	}
	
	// what size do we set the output rect to?
//	GetMovieNaturalBoundsRect(m_QTMovie, &m_QTMovieRect);
	
	m_VideoFrameWidth = m_QTMovieRect.right;
	m_VideoFrameHeight = m_QTMovieRect.bottom;
	
	// Sanity check...
	AssertExitFunc( m_QTMovieRect.top == 0 && m_QTMovieRect.left == 0 &&
					m_QTMovieRect.right >= cMinVideoFrameWidth && m_QTMovieRect.right <= cMaxVideoFrameWidth && 
					m_QTMovieRect.bottom >= cMinVideoFrameHeight && m_QTMovieRect.bottom <= cMaxVideoFrameHeight &&
					m_QTMovieRect.right % 4 == 0,
					SetResult( VideoResult::VIDEO_ERROR_OCCURED ) );

	// Setup the QuiuckTime Graphics World for the Movie
/*	status = QTNewGWorld( &m_MovieGWorld, k32BGRAPixelFormat, &m_QTMovieRect, nil, nil, 0 );
	AssertExit( status == noErr );

	// Setup the playback gamma according to the convar
	SetGWorldDecodeGamma( m_MovieGWorld, VideoPlaybackGamma::USE_GAMMA_CONVAR );

	// Assign the GWorld to this movie
	SetMovieGWorld( m_QTMovie, m_MovieGWorld, nil );

	// Setup Movie Audio, unless suppressed
	if ( !CreateMovieAudioContext( m_bHasAudio, m_QTMovie, &m_AudioContext, true, &m_CurrentVolume ) )
	{
		SetResult( VideoResult::AUDIO_ERROR_OCCURED );
		WarningAssert( "Couldn't Set Audio" );
	}
	
	// Get the time of the first frame
	OSType	qTypes[1] = { VisualMediaCharacteristic };
	short	qFlags = nextTimeStep | nextTimeEdgeOK;			// use nextTimeStep instead of nextTimeMediaSample for MPEG 1-2 compatibility
		
	GetMovieNextInterestingTime( m_QTMovie, qFlags, 1, qTypes, (TimeValue) 0, fixed1, &m_MovieFirstFrameTime, NULL );
	AssertExitFunc( GetMoviesError() == noErr, SetResult( VideoResult::VIDEO_ERROR_OCCURED ) );

	// Preroll the movie
	if ( BITFLAGS_SET( m_PlaybackFlags, VideoPlaybackFlags::PRELOAD_VIDEO ) )
	{
		Fixed playRate = GetMoviePreferredRate( m_QTMovie );
		status = PrerollMovie( m_QTMovie, m_MovieFirstFrameTime, playRate );
		AssertExitFunc( status == noErr, SetResult( VideoResult::VIDEO_ERROR_OCCURED ) );
	}*/
	
#endif
	m_bMovieInitialized = true;
}


void CBinkMaterial::CloseFile()
{
	av_freep( &m_AVVideoData[0] );
	avformat_close_input( &m_AVFmtCtx );
	m_AVFmtCtx = nullptr;
	free(m_RGBData);

	SetFileName( nullptr );
}


