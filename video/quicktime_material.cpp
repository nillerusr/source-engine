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


#include "quicktime_material.h"

#if defined ( WIN32 )
	#include <WinDef.h>
	#include <../dx9sdk/include/dsound.h>
#endif

#include "tier0/memdbgon.h"


// ===========================================================================
// CQuicktimeMaterialRGBTextureRegenerator - Inherited from ITextureRegenerator
//    Copies and converts the buffer bits to texture bits
//    Currently only supports 32-bit BGRA
// ===========================================================================
CQuicktimeMaterialRGBTextureRegenerator::CQuicktimeMaterialRGBTextureRegenerator() :
	m_SrcGWorld( nullptr ),
	m_nSourceWidth( 0 ),
	m_nSourceHeight( 0 )
{
}


CQuicktimeMaterialRGBTextureRegenerator::~CQuicktimeMaterialRGBTextureRegenerator() 
{
	// nothing to do
}

	
void CQuicktimeMaterialRGBTextureRegenerator::SetSourceGWorld( GWorldPtr theGWorld, int nWidth, int nHeight )
{
	m_SrcGWorld		= theGWorld;
	m_nSourceWidth	= nWidth;
	m_nSourceHeight = nHeight;
}


void CQuicktimeMaterialRGBTextureRegenerator::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
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
	if ( m_SrcGWorld == nullptr )
	{
		WarningAssert( "Video texture source not set" );
		memset( pVTFTexture->ImageData(), 0xCC, pVTFTexture->ComputeTotalSize() );
		return;
	}

	// Verify the destination texture is set up correctly
	Assert( pVTFTexture->Format() == IMAGE_FORMAT_BGRA8888 );
	Assert( pVTFTexture->RowSizeInBytes( 0 ) >= pVTFTexture->Width() * 4 );
	Assert( pVTFTexture->Width() >= m_nSourceWidth );
	Assert( pVTFTexture->Height() >= m_nSourceHeight );

	// Copy directly from the Quicktime GWorld
	PixMapHandle thePixMap = GetGWorldPixMap( m_SrcGWorld );
	
	if ( LockPixels( thePixMap ) )
	{
		BYTE   *pImageData	= pVTFTexture->ImageData();
		int		dstStride	= pVTFTexture->RowSizeInBytes( 0 );
		BYTE   *pSrcData	= (BYTE*) GetPixBaseAddr( thePixMap );
		long	srcStride	= QTGetPixMapHandleRowBytes( thePixMap );
		int		rowSize		= m_nSourceWidth * 4;
		
		for (int y = 0; y < m_nSourceHeight; y++ )
		{
			memcpy( pImageData, pSrcData, rowSize );
			pImageData+= dstStride;
			pSrcData+= srcStride;
		}
		
		UnlockPixels( thePixMap );
	}
	else
	{
		WarningAssert( "LockPixels Failed" );
	}
}


void CQuicktimeMaterialRGBTextureRegenerator::Release()
{
	// we don't invoke the destructor here, we're not using the no-release extensions
}



// ===========================================================================
// CQuickTimeMaterial class - creates a material, opens a QuickTime movie
//           and plays the movie onto the material
// ===========================================================================

//-----------------------------------------------------------------------------
// CQuickTimeMaterial Constructor
//-----------------------------------------------------------------------------
CQuickTimeMaterial::CQuickTimeMaterial() :
	m_pFileName( nullptr ),
	m_MovieGWorld( nullptr ),
	m_QTMovie( nullptr ),
	m_AudioContext( nullptr ),
	m_bInitCalled( false )
{
	Reset();
}


//-----------------------------------------------------------------------------
// CQuickTimeMaterial Destructor
//-----------------------------------------------------------------------------
CQuickTimeMaterial::~CQuickTimeMaterial()
{
	Reset();
}


void CQuickTimeMaterial::Reset()
{
	SetQTFileName( nullptr );

	DestroyProceduralTexture();
	DestroyProceduralMaterial();

	m_TexCordU = 0.0f;
	m_TexCordV = 0.0f;
	
	m_VideoFrameWidth = 0;
	m_VideoFrameHeight = 0;
	
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
	
	SAFE_RELEASE_AUDIOCONTEXT( m_AudioContext );
	SAFE_DISPOSE_GWORLD( m_MovieGWorld );
	SAFE_DISPOSE_MOVIE( m_QTMovie );
	
	m_LastResult = VideoResult::SUCCESS;
}


void CQuickTimeMaterial::SetQTFileName( const char *theQTMovieFileName )
{
	SAFE_DELETE_ARRAY( m_pFileName );

	if ( theQTMovieFileName != nullptr )
	{
		AssertMsg( V_strlen( theQTMovieFileName ) <= MAX_QT_FILENAME_LEN, "Bad Quicktime Movie Filename" );
		m_pFileName = COPY_STRING( theQTMovieFileName );
	}
	
}


VideoResult_t CQuickTimeMaterial::SetResult( VideoResult_t status )
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
const char *CQuickTimeMaterial::GetVideoFileName()
{
	return m_pFileName;
}


VideoFrameRate_t &CQuickTimeMaterial::GetVideoFrameRate()
{
	return m_QTMovieFrameRate;
}


VideoResult_t CQuickTimeMaterial::GetLastResult()
{
	return m_LastResult;
}


//-----------------------------------------------------------------------------
// Audio Functions
//-----------------------------------------------------------------------------
bool CQuickTimeMaterial::HasAudio()
{
	return m_bHasAudio;
}


bool CQuickTimeMaterial::SetVolume( float fVolume )
{
	clamp( fVolume, 0.0f, 1.0f );
	
	m_CurrentVolume = fVolume;

	if ( m_AudioContext != nullptr && m_bHasAudio )
	{
		short  movieVolume = (short) ( m_CurrentVolume * 256.0f );
	
		SetMovieVolume( m_QTMovie, movieVolume );
		
		SetResult( VideoResult::SUCCESS );
		return true;
	}
	
	SetResult( VideoResult::AUDIO_ERROR_OCCURED );
	return false;
}


float CQuickTimeMaterial::GetVolume()
{
	return m_CurrentVolume;
}


void CQuickTimeMaterial::SetMuted( bool bMuteState )
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
		OSStatus result = SetMovieAudioMute( m_QTMovie, m_bMuted, 0 );
		AssertExitFunc( result == noErr, SetResult( VideoResult::AUDIO_ERROR_OCCURED) );
	}
	
	SetResult( VideoResult::SUCCESS );
}


bool CQuickTimeMaterial::IsMuted()
{
	return m_bMuted;
}


VideoResult_t CQuickTimeMaterial::SoundDeviceCommand( VideoSoundDeviceOperation_t operation, void *pDevice, void *pData )
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
bool CQuickTimeMaterial::Init( const char *pMaterialName, const char *pFileName, VideoPlaybackFlags_t flags )
{
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitF( IS_NOT_EMPTY( pFileName ) );
	AssertExitF( m_bInitCalled == false );

	m_PlaybackFlags	= flags;
	
	OpenQTMovie( pFileName );	// Open up the Quicktime file

	if ( !m_bMovieInitialized )
	{
		return false;		    		// Something bad happened when we went to open
	}

	// Now we can properly setup our regenerators
	m_TextureRegen.SetSourceGWorld( m_MovieGWorld, m_VideoFrameWidth, m_VideoFrameHeight );

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


void CQuickTimeMaterial::Shutdown( void )
{
	StopVideo();
	Reset();
}


//-----------------------------------------------------------------------------
// Video playback state functions
//-----------------------------------------------------------------------------
bool CQuickTimeMaterial::IsVideoReadyToPlay()
{
	return m_bMovieInitialized;
}


bool CQuickTimeMaterial::IsVideoPlaying()
{
	return m_bMoviePlaying;
}


//-----------------------------------------------------------------------------
// Checks to see if the video has a new frame ready to be rendered and 
// downloaded into the texture and eventually display
//-----------------------------------------------------------------------------
bool CQuickTimeMaterial::IsNewFrameReady( void )
{
	// Are we waiting to start playing the first frame? if so, tell them we are ready!
	if ( m_bMovieInitialized == true  )
	{
		return true;
	}

	// We better be playing the movie 
	AssertExitF( m_bMoviePlaying );
	
	// paused?
	if ( m_bMoviePaused )
	{
		return false;	
	}

	TimeValue curMovieTime = GetMovieTime( m_QTMovie, nullptr );
	
	if ( curMovieTime >= m_QTMovieDuration || m_NextInterestingTimeToPlay == NO_MORE_INTERESTING_TIMES )
	{
		// if we are looping, we have another frame, otherwise no
		return m_bLoopMovie;
	}
	
	// Enough time passed to get to next frame??
	if ( curMovieTime < m_NextInterestingTimeToPlay )
	{
		// nope.. use the previous frame
		return false;
	}

	// we have a new frame we want then..
	return true;
}


bool CQuickTimeMaterial::IsFinishedPlaying()
{
	return m_bMovieFinishedPlaying;
}


void CQuickTimeMaterial::SetLooping( bool bLoopVideo )
{
	m_bLoopMovie = bLoopVideo;
}


bool CQuickTimeMaterial::IsLooping()
{
	return m_bLoopMovie;
}


void CQuickTimeMaterial::SetPaused( bool bPauseState )
{
	if ( !m_bMoviePlaying || m_bMoviePaused == bPauseState )
	{
		Assert( m_bMoviePlaying );
		return;
	}
	
	if ( bPauseState )			// Pausing the movie?
	{
		// Save off current time and set paused state
		m_MoviePauseTime = GetMovieTime( m_QTMovie, nullptr );
		StopMovie( m_QTMovie );
	}
	else  // unpausing the movie
	{
		// Reset the movie to the paused time
		SetMovieTimeValue( m_QTMovie, m_MoviePauseTime );
		StartMovie( m_QTMovie );
		Assert( GetMoviesError() == noErr );
	}
	
	m_bMoviePaused = bPauseState;
}


bool CQuickTimeMaterial::IsPaused()
{
	return ( m_bMoviePlaying ) ? m_bMoviePaused : false;
}


// Begins playback of the movie
bool CQuickTimeMaterial::StartVideo()
{
	if ( !m_bMovieInitialized )
	{
		Assert( false );
		SetResult( VideoResult::OPERATION_ALREADY_PERFORMED );
		return false;
	}
	
	// Start the movie playing at the first frame
	SetMovieTimeValue( m_QTMovie, m_MovieFirstFrameTime );
	Assert( GetMoviesError() == noErr );
	
	StartMovie( m_QTMovie );
	Assert( GetMoviesError() == noErr );
	
	// Transition to playing state
	m_bMovieInitialized = false;
	m_bMoviePlaying = true;

	// Deliberately set the next interesting time to the current time to	
	// insure that the ::update() call causes the textures to be downloaded
	m_NextInterestingTimeToPlay = m_MovieFirstFrameTime;
	Update();

	return true;
}


// stops movie for good, frees resources, but retains texture & material of last frame rendered
bool CQuickTimeMaterial::StopVideo()
{
	if ( !m_bMoviePlaying )
	{
		SetResult( VideoResult::OPERATION_OUT_OF_SEQUENCE );
		return false;
	}

	StopMovie( m_QTMovie );

	m_bMoviePlaying = false;
	m_bMoviePaused = false;
	m_bMovieFinishedPlaying = true;

	// free resources	
	CloseQTFile();
	
	SetResult( VideoResult::SUCCESS );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Updates our scene
// Output : true = movie playing ok, false = time to end movie
// supposed to be: Returns true on a new frame of video being downloaded into the texture
//-----------------------------------------------------------------------------
bool CQuickTimeMaterial::Update( void )
{
	AssertExitF( m_bMoviePlaying );
	
	OSType	qTypes[1] = { VisualMediaCharacteristic };

	// are we paused? can't update if so...
	if ( m_bMoviePaused )
	{
		return true;			// reuse the last frame
	}
	
	// Get current time in the movie
	TimeValue curMovieTime = GetMovieTime( m_QTMovie, nullptr );
	
	// Did we hit the end of the movie?
	if ( curMovieTime >= m_QTMovieDuration )
	{
		// If we're not looping, then report that we are done updating
		if ( m_bLoopMovie == false )
		{
			StopVideo();
			return false;
		}
		
		// Reset the movie to the start time
		SetMovieTimeValue( m_QTMovie, m_MovieFirstFrameTime );
		AssertExitF( GetMoviesError() == noErr );
		
		// Assure fall through to render a new frame
		m_NextInterestingTimeToPlay = m_MovieFirstFrameTime;
	}
	
	// Are we on the last frame of the movie? (but not past the end of any audio?)
	if ( m_NextInterestingTimeToPlay == NO_MORE_INTERESTING_TIMES )
	{
		return true;		// reuse last frame
	}

	// Enough time passed to get to next frame?
	if ( curMovieTime < m_NextInterestingTimeToPlay )
	{
		// nope.. use the previous frame
		return true;
	}

	// move the movie along
	UpdateMovie( m_QTMovie );
	AssertExitF( GetMoviesError() == noErr );

	// Let QuickTime render the frame
	MoviesTask( m_QTMovie, 0L );
	AssertExitF( GetMoviesError() == noErr );

	// Get the next frame after the current time (the movie may have advanced a bit during UpdateMovie() and MovieTasks()
	GetMovieNextInterestingTime( m_QTMovie, nextTimeStep | nextTimeEdgeOK, 1, qTypes, GetMovieTime( m_QTMovie, nullptr ), fixed1, &m_NextInterestingTimeToPlay, nullptr );
	
	// hit the end of the movie?
	if ( GetMoviesError() == invalidTime || m_NextInterestingTimeToPlay == END_OF_QUICKTIME_MOVIE )
	{
		m_NextInterestingTimeToPlay = NO_MORE_INTERESTING_TIMES;
	}

	// Regenerate our texture, it'll grab from the GWorld Directly
	m_Texture->Download();

	SetResult( VideoResult::SUCCESS );
	return true;
}


//-----------------------------------------------------------------------------
// Returns the material
//-----------------------------------------------------------------------------
IMaterial *CQuickTimeMaterial::GetMaterial()
{
	return m_Material;
}							   


//-----------------------------------------------------------------------------
// Returns the texcoord range
//-----------------------------------------------------------------------------
void CQuickTimeMaterial::GetVideoTexCoordRange( float *pMaxU, float *pMaxV )
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
void CQuickTimeMaterial::GetVideoImageSize( int *pWidth, int *pHeight )
{
	Assert( pWidth != nullptr && pHeight != nullptr );
	
	*pWidth  = m_VideoFrameWidth;
	*pHeight = m_VideoFrameHeight;
}


float CQuickTimeMaterial::GetVideoDuration()
{
	return m_QTMovieDurationinSec;
}


int CQuickTimeMaterial::GetFrameCount()
{
	return m_QTMovieFrameCount;
}


//-----------------------------------------------------------------------------
// Sets the frame for an QuickTime  Material (use instead of SetTime)
//-----------------------------------------------------------------------------
bool CQuickTimeMaterial::SetFrame( int FrameNum )
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


int CQuickTimeMaterial::GetCurrentFrame()
{
	AssertExitV( m_bMoviePlaying, -1 );

	TimeValue curTime = m_bMoviePaused ? m_MoviePauseTime : GetMovieTime( m_QTMovie, nullptr );
	
	return curTime / m_QTMovieFrameRate.GetUnitsPerFrame();
}


float CQuickTimeMaterial::GetCurrentVideoTime()
{
	AssertExitV( m_bMoviePlaying, -1.0f );
	
	TimeValue curTime = m_bMoviePaused ? m_MoviePauseTime : GetMovieTime( m_QTMovie, nullptr );
	
	return curTime / m_QTMovieFrameRate.GetUnitsPerSecond();
}


bool CQuickTimeMaterial::SetTime( float flTime )
{
	AssertExitF( m_bMoviePlaying );
	AssertExitF( flTime >= 0 && flTime < m_QTMovieDurationinSec );

	TimeValue newTime = (TimeValue) ( flTime * m_QTMovieFrameRate.GetUnitsPerSecond() + 0.5f) ;
	
	clamp( newTime,  m_MovieFirstFrameTime, m_QTMovieDuration ); 

	// Are we paused? 	
	if ( m_bMoviePaused )
	{
		m_MoviePauseTime = newTime;
		return true;
	}

	TimeValue curMovieTime = GetMovieTime( m_QTMovie, nullptr );
	
	// Don't stop and reset movie if we are within 1 frame of the requested time
	if ( newTime <= curMovieTime - m_QTMovieFrameRate.GetUnitsPerFrame() || newTime >= curMovieTime + m_QTMovieFrameRate.GetUnitsPerFrame() )
	{
		// Reset the movie to the requested time
		StopMovie( m_QTMovie );
		SetMovieTimeValue( m_QTMovie, newTime );
		StartMovie( m_QTMovie );
		
		Assert( GetMoviesError() == noErr );
	}

	return true;	
}


//-----------------------------------------------------------------------------
// Initializes, shuts down the procedural texture
//-----------------------------------------------------------------------------
void CQuickTimeMaterial::CreateProceduralTexture( const char *pTextureName )
{
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
				IMAGE_FORMAT_BGRA8888, TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP |
				TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_NOLOD );
		
	// Use this to get the updated frame from the remote connection		
	m_Texture->SetTextureRegenerator( &m_TextureRegen /* , false */ );
	
	// compute the texcoords
	int nTextureWidth = m_Texture->GetActualWidth();
	int nTextureHeight = m_Texture->GetActualHeight();
	
	m_TexCordU = ( nTextureWidth > 0 ) ? (float) m_VideoFrameWidth / (float) nTextureWidth : 0.0f;
	m_TexCordV = ( nTextureHeight > 0 ) ? (float) m_VideoFrameHeight / (float) nTextureHeight : 0.0f;
}


void CQuickTimeMaterial::DestroyProceduralTexture()
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
void CQuickTimeMaterial::CreateProceduralMaterial( const char *pMaterialName )
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


void CQuickTimeMaterial::DestroyProceduralMaterial()
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
void CQuickTimeMaterial::OpenQTMovie( const char *theQTMovieFileName )
{
	AssertExit( IS_NOT_EMPTY( theQTMovieFileName ) );

    // Set graphics port 
#if defined ( WIN32 )
	SetGWorld ( (CGrafPtr) GetNativeWindowPort( nil ), nil ); 
#elif defined ( OSX		)
	SetGWorld( nil, nil );
#endif
	
	SetQTFileName( theQTMovieFileName );

	Handle	MovieFileDataRef = nullptr;
	OSType	MovieFileDataRefType = 0;

	CFStringRef	imageStrRef = CFStringCreateWithCString ( NULL,  theQTMovieFileName, 0 ); 
	AssertExitFunc( imageStrRef != nullptr, SetResult( VideoResult::SYSTEM_ERROR_OCCURED ) );
	
	OSErr status = QTNewDataReferenceFromFullPathCFString( imageStrRef, (QTPathStyle) kQTNativeDefaultPathStyle, 0, &MovieFileDataRef, &MovieFileDataRefType );
	AssertExitFunc( status == noErr, SetResult( VideoResult::FILE_ERROR_OCCURED ) );

	CFRelease( imageStrRef );

    status = NewMovieFromDataRef( &m_QTMovie, newMovieActive, nil, MovieFileDataRef, MovieFileDataRefType );
	SAFE_DISPOSE_HANDLE( MovieFileDataRef );
	
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
		Track audioTrack = GetMovieIndTrackType( m_QTMovie, 1, SoundMediaType, movieTrackMediaType );
		m_bHasAudio = ( audioTrack != nullptr );
	}

	// Now we need to extract the time info from the QT Movie 
	m_QTMovieTimeScale	= GetMovieTimeScale( m_QTMovie );
	m_QTMovieDuration	= GetMovieDuration( m_QTMovie );

	// compute movie duration	
	m_QTMovieDurationinSec = float ( double( m_QTMovieDuration ) / double( m_QTMovieTimeScale ) );
	if ( !MovieGetStaticFrameRate( m_QTMovie, m_QTMovieFrameRate ) )
	{
		WarningAssert( "Couldn't Get Frame Rate" );
	}
	
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
	GetMovieNaturalBoundsRect(m_QTMovie, &m_QTMovieRect);
	
	m_VideoFrameWidth = m_QTMovieRect.right;
	m_VideoFrameHeight = m_QTMovieRect.bottom;
	
	// Sanity check...
	AssertExitFunc( m_QTMovieRect.top == 0 && m_QTMovieRect.left == 0 &&
					m_QTMovieRect.right >= cMinVideoFrameWidth && m_QTMovieRect.right <= cMaxVideoFrameWidth && 
	    		    m_QTMovieRect.bottom >= cMinVideoFrameHeight && m_QTMovieRect.bottom <= cMaxVideoFrameHeight &&
	    		    m_QTMovieRect.right % 4 == 0,
					SetResult( VideoResult::VIDEO_ERROR_OCCURED ) );

	// Setup the QuiuckTime Graphics World for the Movie
	status = QTNewGWorld( &m_MovieGWorld, k32BGRAPixelFormat, &m_QTMovieRect, nil, nil, 0 );
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
	}
	
	m_bMovieInitialized = true;
}


void CQuickTimeMaterial::CloseQTFile()
{
	if ( m_QTMovie == nullptr )
	{
		return;
	}

	SAFE_RELEASE_AUDIOCONTEXT( m_AudioContext );
	SAFE_DISPOSE_GWORLD( m_MovieGWorld );
	SAFE_DISPOSE_MOVIE( m_QTMovie );
 
	SetQTFileName( nullptr );
}


