//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "quicktime_video.h"

#include "quicktime_common.h"
#include "quicktime_material.h"
#include "quicktime_recorder.h"


#include "filesystem.h"
#include "tier0/icommandline.h"
#include "tier1/strtools.h"
#include "tier1/utllinkedlist.h"
#include "tier1/KeyValues.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/itexture.h"
#include "vtf/vtf.h"
#include "pixelwriter.h"
#include "tier2/tier2.h"
#include "platform.h"


#if defined ( WIN32 )
	#include <WinDef.h>
	#include <../dx9sdk/include/dsound.h>
#endif

#include "tier0/memdbgon.h"


// ===========================================================================
// Singleton to expose Quicktime video subsystem
// ===========================================================================
static CQuickTimeVideoSubSystem g_QuickTimeSystem;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CQuickTimeVideoSubSystem, IVideoSubSystem, VIDEO_SUBSYSTEM_INTERFACE_VERSION, g_QuickTimeSystem );


// ===========================================================================
// Convars used by Quicktime
//  - these need to be referenced somewhere to keep the compiler from 
//    optimizing them away
// ===========================================================================

ConVar QuickTime_EncodeGamma( "video_quicktime_encode_gamma", "3", FCVAR_ARCHIVE , "QuickTime Video Encode Gamma Target- 0=no gamma adjust  1=platform default gamma  2 = gamma 1.8  3 = gamma 2.2  4 = gamma 2.5", true, 0.0f, true, 4.0f );
ConVar QuickTime_PlaybackGamma( "video_quicktime_decode_gamma", "0", FCVAR_ARCHIVE , "QuickTime Video Playback Gamma Target- 0=no gamma adjust  1=platform default gamma  2 = gamma 1.8  3 = gamma 2.2  4 = gamma 2.5", true, 0.0f, true, 4.0f );

// ===========================================================================
// List of file extensions and features supported by this subsystem
// ===========================================================================
VideoFileExtensionInfo_t s_QuickTimeExtensions[] = 
{
	{ ".mov", VideoSystem::QUICKTIME,  VideoSystemFeature::FULL_PLAYBACK | VideoSystemFeature::FULL_ENCODE },
	{ ".mp4", VideoSystem::QUICKTIME,  VideoSystemFeature::FULL_PLAYBACK | VideoSystemFeature::FULL_ENCODE },
};

const int s_QuickTimeExtensionCount = ARRAYSIZE( s_QuickTimeExtensions );

const VideoSystemFeature_t	CQuickTimeVideoSubSystem::DEFAULT_FEATURE_SET = VideoSystemFeature::FULL_PLAYBACK | VideoSystemFeature::FULL_ENCODE;

#ifdef OSX
PFNGetGWorldPixMap GetGWorldPixMap = NULL;
PFNGetPixBaseAddr GetPixBaseAddr = NULL;
PFNLockPixels LockPixels = NULL;
PFNUnlockPixels UnlockPixels = NULL;
PFNDisposeGWorld DisposeGWorld = NULL;
PFNSetGWorld SetGWorld = NULL;
PFNGetPixRowBytes GetPixRowBytes = NULL;
#endif

// ===========================================================================
// CQuickTimeVideoSubSystem class
// ===========================================================================
CQuickTimeVideoSubSystem::CQuickTimeVideoSubSystem() :
	m_bQuickTimeInitialized( false ),
	m_LastResult( VideoResult::SUCCESS ),
	m_CurrentStatus( VideoSystemStatus::NOT_INITIALIZED ),
	m_AvailableFeatures( CQuickTimeVideoSubSystem::DEFAULT_FEATURE_SET ),
	m_pCommonServices( nullptr )
{

}


CQuickTimeVideoSubSystem::~CQuickTimeVideoSubSystem()
{
	ShutdownQuickTime();		// Super redundant safety check
}


// ===========================================================================
// IAppSystem methods
// ===========================================================================
bool CQuickTimeVideoSubSystem::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
	{
		return false;
	}

	if ( g_pFullFileSystem == nullptr || materials == nullptr ) 
	{
		Msg( "QuickTime video subsystem failed to connect to missing a required system\n" );
		return false;
	}
	return true;
}


void CQuickTimeVideoSubSystem::Disconnect()
{
	BaseClass::Disconnect();
}


void* CQuickTimeVideoSubSystem::QueryInterface( const char *pInterfaceName )
{

	if ( IS_NOT_EMPTY( pInterfaceName ) )
	{
		if ( V_strncmp(	pInterfaceName, VIDEO_SUBSYSTEM_INTERFACE_VERSION, Q_strlen( VIDEO_SUBSYSTEM_INTERFACE_VERSION ) + 1) == STRINGS_MATCH )
		{
			return (IVideoSubSystem*) this;
		}
	}

	return nullptr;
}


InitReturnVal_t CQuickTimeVideoSubSystem::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
	{
		return nRetVal;
	}

	return INIT_OK;

}


void CQuickTimeVideoSubSystem::Shutdown()
{
	// Make sure we shut down quicktime
	ShutdownQuickTime();
	
	BaseClass::Shutdown();
}


// ===========================================================================
// IVideoSubSystem identification methods  
// ===========================================================================
VideoSystem_t CQuickTimeVideoSubSystem::GetSystemID()
{
	return VideoSystem::QUICKTIME;
}


VideoSystemStatus_t CQuickTimeVideoSubSystem::GetSystemStatus()
{
	return m_CurrentStatus;
}


VideoSystemFeature_t CQuickTimeVideoSubSystem::GetSupportedFeatures()
{
	return m_AvailableFeatures;
}


const char* CQuickTimeVideoSubSystem::GetVideoSystemName()
{
	return "Quicktime";
}


// ===========================================================================
// IVideoSubSystem setup and shutdown services
// ===========================================================================
bool CQuickTimeVideoSubSystem::InitializeVideoSystem( IVideoCommonServices *pCommonServices )
{
	m_AvailableFeatures = DEFAULT_FEATURE_SET;			// Put here because of issue with static const int, binary OR and DEBUG builds
	
	AssertPtr( pCommonServices );
	m_pCommonServices = pCommonServices;
	
#ifdef OSX
	if ( !GetGWorldPixMap )
		GetGWorldPixMap = (PFNGetGWorldPixMap)dlsym( RTLD_DEFAULT, "GetGWorldPixMap" );
	if ( !GetPixBaseAddr )
		GetPixBaseAddr = (PFNGetPixBaseAddr)dlsym( RTLD_DEFAULT, "GetPixBaseAddr" );
	if ( !LockPixels )
		LockPixels = (PFNLockPixels)dlsym( RTLD_DEFAULT, "LockPixels" );
	if ( !UnlockPixels )
		UnlockPixels = (PFNUnlockPixels)dlsym( RTLD_DEFAULT, "UnlockPixels" );
	if ( !DisposeGWorld )
		DisposeGWorld = (PFNDisposeGWorld)dlsym( RTLD_DEFAULT, "DisposeGWorld" );
	if ( !SetGWorld )
		SetGWorld = (PFNSetGWorld)dlsym( RTLD_DEFAULT, "SetGWorld" );
	if ( !GetPixRowBytes )
		GetPixRowBytes = (PFNGetPixRowBytes)dlsym( RTLD_DEFAULT, "GetPixRowBytes" );
	if ( !GetGWorldPixMap || !GetPixBaseAddr || !LockPixels || !UnlockPixels || !DisposeGWorld || !SetGWorld || !GetPixRowBytes )
		return false;
#endif

	return ( m_bQuickTimeInitialized ) ? true : SetupQuickTime();
}


bool CQuickTimeVideoSubSystem::ShutdownVideoSystem()
{
	return (  m_bQuickTimeInitialized ) ? ShutdownQuickTime() : true;
}


VideoResult_t CQuickTimeVideoSubSystem::VideoSoundDeviceCMD( VideoSoundDeviceOperation_t operation, void *pDevice, void *pData )
{
	switch ( operation ) 
	{
		case VideoSoundDeviceOperation::SET_DIRECT_SOUND_DEVICE:
		{
			return SetResult( VideoResult::OPERATION_NOT_SUPPORTED );
		}
		
		case VideoSoundDeviceOperation::SET_MILES_SOUND_DEVICE:
		case VideoSoundDeviceOperation::HOOK_X_AUDIO:
		{
			return SetResult( VideoResult::OPERATION_NOT_SUPPORTED );
		}
		
		default:
		{
			return SetResult( VideoResult::UNKNOWN_OPERATION );
		}
	}
}


// ===========================================================================
// IVideoSubSystem supported extensions & features
// ===========================================================================
int CQuickTimeVideoSubSystem::GetSupportedFileExtensionCount()
{
	return s_QuickTimeExtensionCount;
}

 
const char* CQuickTimeVideoSubSystem::GetSupportedFileExtension( int num )
{
	return ( num < 0 || num >= s_QuickTimeExtensionCount ) ? nullptr : s_QuickTimeExtensions[num].m_FileExtension;
}

 
VideoSystemFeature_t CQuickTimeVideoSubSystem::GetSupportedFileExtensionFeatures( int num )
{
	 return ( num < 0 || num >= s_QuickTimeExtensionCount ) ? VideoSystemFeature::NO_FEATURES : s_QuickTimeExtensions[num].m_VideoFeatures;
}


// ===========================================================================
// IVideoSubSystem Video Playback and Recording Services
// ===========================================================================
VideoResult_t CQuickTimeVideoSubSystem::PlayVideoFileFullScreen( const char *filename, void *mainWindow, int windowWidth, int windowHeight, int desktopWidth, int desktopHeight, bool windowed, float forcedMinTime, VideoPlaybackFlags_t playbackFlags )
{
	OSErr status;

	// See if the caller is asking for a feature we can not support....
	VideoPlaybackFlags_t unsupportedFeatures = VideoPlaybackFlags::PRELOAD_VIDEO;

	if ( playbackFlags & unsupportedFeatures )
	{
		return SetResult( VideoResult::FEATURE_NOT_AVAILABLE );
	}

	// Make sure we are initialized and ready
	if ( !m_bQuickTimeInitialized )
	{
		SetupQuickTime();
	}
	
	AssertExitV( m_CurrentStatus == VideoSystemStatus::OK, SetResult( VideoResult::SYSTEM_NOT_AVAILABLE ) );

	// Set graphics port 
#if defined ( WIN32 )
	SetGWorld ( (CGrafPtr) GetNativeWindowPort( nil ), nil ); 
#elif defined ( OSX )
	SystemUIMode oldMode;
	SystemUIOptions oldOptions;
	GetSystemUIMode( &oldMode, &oldOptions );

	if ( !windowed )
	{
		status = SetSystemUIMode( kUIModeAllHidden, (SystemUIOptions) 0 );
		Assert( status == noErr );
	}
	SetGWorld( nil, nil );
#endif

	// -------------------------------------------------
	// Open the quicktime file with audio
	Movie				theQTMovie = NULL;
	Rect				theQTMovieRect;
	QTAudioContextRef	theAudioContext = NULL;

	Handle	MovieFileDataRef = nullptr;
	OSType	MovieFileDataRefType = 0;

	CFStringRef	imageStrRef = CFStringCreateWithCString ( NULL,  filename, 0 ); 
	AssertExitV( imageStrRef != nullptr, SetResult( VideoResult::SYSTEM_ERROR_OCCURED ) );
	
	status = QTNewDataReferenceFromFullPathCFString( imageStrRef, (QTPathStyle) kQTNativeDefaultPathStyle, 0, &MovieFileDataRef, &MovieFileDataRefType );
	AssertExitV( status == noErr, SetResult( VideoResult::FILE_ERROR_OCCURED ) );

	CFRelease( imageStrRef );

    status = NewMovieFromDataRef( &theQTMovie, newMovieActive, nil, MovieFileDataRef, MovieFileDataRefType );
	SAFE_DISPOSE_HANDLE( MovieFileDataRef );
	
    if ( status != noErr )
    {
#if defined ( OSX )
		SetSystemUIMode( oldMode, oldOptions );
#endif
		Assert( false );
		return SetResult( VideoResult::FILE_ERROR_OCCURED );
    }
     
	// Get info about the video
	GetMovieNaturalBoundsRect(theQTMovie, &theQTMovieRect);

	TimeValue theQTMovieDuration = GetMovieDuration( theQTMovie );

	// what size do we set the output rect to?
	// Integral scaling is much faster, so always scale the video as such
	int	nNewWidth  = (int) theQTMovieRect.right;
	int nNewHeight = (int) theQTMovieRect.bottom;

	// Determine the window we are rendering video into
	int displayWidth  = windowWidth;
	int displayHeight = windowHeight;

	// on mac OSX, if we are fullscreen, quicktime is bypassing our targets and going to the display directly, so use its dimensions
	if ( IsOSX() && windowed == false )
	{
		displayWidth = desktopWidth;
		displayHeight = desktopHeight;
	}

	// get the size of the target video output
	int	nBufferWidth = nNewWidth;
	int nBufferHeight = nNewHeight;
	
	int displayXOffset = 0;
	int displayYOffset = 0;

	if ( !m_pCommonServices->CalculateVideoDimensions( nNewWidth, nNewHeight, displayWidth, displayHeight, playbackFlags, &nBufferWidth, &nBufferHeight, &displayXOffset, &displayYOffset ) )
	{
#if defined ( OSX )
		SetSystemUIMode( oldMode, oldOptions );
#endif
		return SetResult( VideoResult::VIDEO_ERROR_OCCURED );
	}

	theQTMovieRect.left = (short) displayXOffset;
	theQTMovieRect.right = (short) ( displayXOffset + nBufferWidth );
	theQTMovieRect.top = (short) displayYOffset;
	theQTMovieRect.bottom = (short) ( displayYOffset + nBufferHeight );

	SetMovieBox( theQTMovie, &theQTMovieRect );

	// Check to see if we should include audio playback
	bool enableMovieAudio = !BITFLAGS_SET( playbackFlags, VideoPlaybackFlags::NO_AUDIO );

	if ( !CreateMovieAudioContext( enableMovieAudio, theQTMovie, &theAudioContext, true) )
	{
#if defined ( OSX )
		SetSystemUIMode( oldMode, oldOptions );
#endif
		return SetResult( VideoResult::AUDIO_ERROR_OCCURED );
	}

	// need to get the graphics port associated with the main window
#if defined( WIN32 )    
	CreatePortAssociation( mainWindow, NULL, 0 );
	GrafPtr theGrafPtr = GetNativeWindowPort( mainWindow );
#elif defined( OSX )
	GrafPtr theGrafPtr = GetWindowPort( (OpaqueWindowPtr*)mainWindow );
#endif

	// Setup the playback gamma according to the convar
	SetGWorldDecodeGamma( (CGrafPtr) theGrafPtr, VideoPlaybackGamma::USE_GAMMA_CONVAR );

	// Assign the GWorld to this movie
	SetMovieGWorld( theQTMovie, (CGrafPtr) theGrafPtr, NULL );
	
	// Setup the keyboard and message handler for fullscreen playback
	if ( SetResult( m_pCommonServices->InitFullScreenPlaybackInputHandler( playbackFlags, forcedMinTime, windowed ) ) != VideoResult::SUCCESS )
	{
#if defined ( OSX )
		SetSystemUIMode( oldMode, oldOptions );
#endif
		return GetLastResult();
	}

	// Other Movie playback	state init
	bool bPaused = false;

	// Init Movie info
    TimeRecord  movieStartTime;
	TimeRecord	moviePauseTime;
    GoToBeginningOfMovie( theQTMovie );
    GetMovieTime( theQTMovie, &movieStartTime );

	// Start movie playback 
	StartMovie( theQTMovie );

	// loop while movie is playing
	while ( true )
	{
		bool bAbortEvent, bPauseEvent, bQuitEvent;
	
		if ( m_pCommonServices->ProcessFullScreenInput( bAbortEvent, bPauseEvent, bQuitEvent ) )
		{
			// check for aborting the movie
			if ( bAbortEvent || bQuitEvent )
			{
				goto abort_playback;
			}
	
			// check for pausing the movie?
			if ( bPauseEvent )
			{
				if ( bPaused == false )		// pausing the movie?
				{
					GetMovieTime( theQTMovie, &moviePauseTime );
					StopMovie( theQTMovie );
					bPaused = true;
				}
				else	// unpause the movie
				{
					SetMovieTime( theQTMovie, &moviePauseTime );
					StartMovie( theQTMovie );
					bPaused = false;
				}
			}
		}
												  
		// hit the end of the movie?
		TimeValue now = GetMovieTime( theQTMovie, nullptr );
		if ( now >= theQTMovieDuration )
		{
			// Loop this movie until aborted?
			if ( playbackFlags & BITFLAGS_SET( playbackFlags, VideoPlaybackFlags::LOOP_VIDEO ) )
			{
				Assert( ANY_BITFLAGS_SET( playbackFlags, VideoPlaybackFlags::ABORT_ON_ESC | VideoPlaybackFlags::ABORT_ON_RETURN | VideoPlaybackFlags::ABORT_ON_SPACE | VideoPlaybackFlags::ABORT_ON_ANY_KEY ) );			// Movie will loop forever otherwise
				StopMovie( theQTMovie );
				SetMovieTime( theQTMovie, &movieStartTime );
				StartMovie( theQTMovie );
			}
			else
			{
				break;		// finished actually, exit loop
			}
		}
		
		// if the movie is paused, sleep for 5ms to keeps the CPU from spinning so hard
		if ( bPaused )
		{
			ThreadSleep( 1 );
		}
		else
		{
			// Keep the movie running....
			MoviesTask( theQTMovie, 0L );
		}

	}

	// Close it all down
abort_playback:	
	StopMovie( theQTMovie );

	SAFE_RELEASE_AUDIOCONTEXT( theAudioContext );
	SAFE_DISPOSE_MOVIE( theQTMovie );

	m_pCommonServices->TerminateFullScreenPlaybackInputHandler();

#if defined ( OSX )
	SetSystemUIMode( oldMode, oldOptions );
#endif

	return SetResult( VideoResult::SUCCESS );	
}


// ===========================================================================
// IVideoSubSystem Video Material Services
//   note that the filename is absolute and has already resolved any paths
// ===========================================================================
IVideoMaterial* CQuickTimeVideoSubSystem::CreateVideoMaterial( const char *pMaterialName, const char *pVideoFileName, VideoPlaybackFlags_t flags )
{
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitN( m_CurrentStatus == VideoSystemStatus::OK && IS_NOT_EMPTY( pMaterialName ) || IS_NOT_EMPTY( pVideoFileName ) );

	CQuickTimeMaterial	*pVideoMaterial = new CQuickTimeMaterial();
	if ( pVideoMaterial == nullptr || pVideoMaterial->Init( pMaterialName, pVideoFileName, flags ) == false )
	{
		SAFE_DELETE( pVideoMaterial );
		SetResult( VideoResult::VIDEO_ERROR_OCCURED );
		return nullptr;
	}

	IVideoMaterial	*pInterface = (IVideoMaterial*) pVideoMaterial;
	m_MaterialList.AddToTail( pInterface );
	
	SetResult( VideoResult::SUCCESS );
	return pInterface;
}


VideoResult_t CQuickTimeVideoSubSystem::DestroyVideoMaterial( IVideoMaterial *pVideoMaterial )
{
	AssertExitV( m_CurrentStatus == VideoSystemStatus::OK, SetResult( VideoResult::SYSTEM_NOT_AVAILABLE ) );
	AssertPtrExitV( pVideoMaterial, SetResult( VideoResult::BAD_INPUT_PARAMETERS ) );
	
	if ( m_MaterialList.Find( pVideoMaterial ) != -1 )
	{
		CQuickTimeMaterial *pObject = (CQuickTimeMaterial*) pVideoMaterial;
		pObject->Shutdown();
		delete pObject;
		
		m_MaterialList.FindAndFastRemove( pVideoMaterial );
		
		return SetResult( VideoResult::SUCCESS );
	}
	
	return SetResult (VideoResult::MATERIAL_NOT_FOUND );

}


// ===========================================================================
// IVideoSubSystem Video Recorder Services
// ===========================================================================
IVideoRecorder* CQuickTimeVideoSubSystem::CreateVideoRecorder()
{
	SetResult( VideoResult::SYSTEM_NOT_AVAILABLE );
	AssertExitN( m_CurrentStatus == VideoSystemStatus::OK );

	CQuickTimeVideoRecorder *pVideoRecorder = new CQuickTimeVideoRecorder();
	
	if ( pVideoRecorder != nullptr )
	{
		IVideoRecorder *pInterface = (IVideoRecorder*) pVideoRecorder;
		m_RecorderList.AddToTail( pInterface );
		
		SetResult( VideoResult::SUCCESS );
		return pInterface;
	}

	SetResult( VideoResult::VIDEO_ERROR_OCCURED );
	return nullptr;
}


VideoResult_t CQuickTimeVideoSubSystem::DestroyVideoRecorder( IVideoRecorder *pRecorder )
{
	AssertExitV( m_CurrentStatus == VideoSystemStatus::OK, SetResult( VideoResult::SYSTEM_NOT_AVAILABLE ) );
	AssertPtrExitV( pRecorder, SetResult( VideoResult::BAD_INPUT_PARAMETERS ) );

	if ( m_RecorderList.Find( pRecorder ) != -1 )
	{
		CQuickTimeVideoRecorder *pVideoRecorder = (CQuickTimeVideoRecorder*) pRecorder;
		delete pVideoRecorder;
		
		m_RecorderList.FindAndFastRemove( pRecorder );
		
		return SetResult( VideoResult::SUCCESS );
	}
	
	return SetResult( VideoResult::RECORDER_NOT_FOUND );
}

VideoResult_t CQuickTimeVideoSubSystem::CheckCodecAvailability( VideoEncodeCodec_t codec )
{
	AssertExitV( m_CurrentStatus == VideoSystemStatus::OK, SetResult( VideoResult::SYSTEM_NOT_AVAILABLE ) );
	AssertExitV( codec >= VideoEncodeCodec::DEFAULT_CODEC && codec < VideoEncodeCodec::CODEC_COUNT, SetResult( VideoResult::BAD_INPUT_PARAMETERS ) );

	// map the requested codec in 
	CodecType	theCodec;
	switch( codec )
	{
		case VideoEncodeCodec::MPEG2_CODEC:
		{
			theCodec = kMpegYUV420CodecType;
			break;
		}
		case VideoEncodeCodec::MPEG4_CODEC:
		{
			theCodec = kMPEG4VisualCodecType;
			break;
		}
		case VideoEncodeCodec::H261_CODEC:
		{
			theCodec = kH261CodecType;
			break;
		}
		case VideoEncodeCodec::H263_CODEC:
		{
			theCodec = kH263CodecType;
			break;
		}
		case VideoEncodeCodec::H264_CODEC:
		{
			theCodec = kH264CodecType;
			break;
		}
		case VideoEncodeCodec::MJPEG_A_CODEC:
		{
			theCodec = kMotionJPEGACodecType;
			break;
		}
		case VideoEncodeCodec::MJPEG_B_CODEC:
		{
			theCodec = kMotionJPEGBCodecType;
			break;
		}
		case VideoEncodeCodec::SORENSON3_CODEC:
		{
			theCodec = kSorenson3CodecType;
			break;
		}
		case VideoEncodeCodec::CINEPACK_CODEC:
		{
			theCodec = kCinepakCodecType;
			break;
		}
		default:										// should never hit this because we are already range checked
		{
			theCodec = CQTVideoFileComposer::DEFAULT_CODEC;
			break;
		}
	}

	// Determine if codec is available...
	CodecInfo	theInfo;
	
	OSErr status = GetCodecInfo( &theInfo, theCodec, 0 );
	if ( status == noCodecErr )
	{
		return SetResult( VideoResult::CODEC_NOT_AVAILABLE );;
	}
	AssertExitV( status == noErr, SetResult( VideoResult::CODEC_NOT_AVAILABLE ) );

	return SetResult( VideoResult::SUCCESS );
}


// ===========================================================================
// Status support
// ===========================================================================
VideoResult_t CQuickTimeVideoSubSystem::GetLastResult()
{
	return m_LastResult;
}


VideoResult_t CQuickTimeVideoSubSystem::SetResult( VideoResult_t status )
{
	m_LastResult = status;
	return status;
}


// ===========================================================================
// Quicktime Initialization & Shutdown
// ===========================================================================
bool CQuickTimeVideoSubSystem::SetupQuickTime()
{
	SetResult( VideoResult::INITIALIZATION_ERROR_OCCURED);
	AssertExitF( m_bQuickTimeInitialized == false );

	// This is set early to indicate we have already been through here, even if we error out for some reason
    m_bQuickTimeInitialized = true;
	m_CurrentStatus = VideoSystemStatus::NOT_INITIALIZED;
	m_AvailableFeatures = VideoSystemFeature::NO_FEATURES;
    
	if ( CommandLine()->FindParm( "-noquicktime" ) )
	{
		// Don't even try. leave status as NOT_INITIALIZED
		return true;
	}

  // Windows PC build 
#if defined ( WIN32 )
	OSErr status = InitializeQTML( 0 ); 
    
    // if -2903 (qtmlDllLoadErr) then quicktime not installed on this system
    if ( status != noErr )
    {
		if  ( status == qtmlDllLoadErr )
		{
			m_CurrentStatus = VideoSystemStatus::NOT_INITIALIZED;
			return true;
		}
		
		Msg( "Unknown QuickTime Initialization Error = %d \n", (int) status );
		Assert( false );
		
		m_CurrentStatus = VideoSystemStatus::NOT_INITIALIZED;
		return true;
    }
    
	// Make sure we have version 7.04 or greater of quicktime 
    long version = 0;
    status = Gestalt( gestaltQuickTime, &version );
    if ( ( status != noErr ) || ( version < 0x07608000 ) )
    {
		TerminateQTML();
		m_CurrentStatus = VideoSystemStatus::NOT_CURRENT_VERSION;
		Msg( "QuickTime Version reports to be ver= %8.8x, less than 7.6 (07608000) required\n", version );
		Assert( false );
		
        return true;
    }
    
#endif    
  
	// Windows PC, or Mac OSX build
	OSErr status2 = EnterMovies();           // Initialize QuickTime Movie Toolbox
	if ( status2 != noErr )
	{
		// Windows PC -- shutdown Quicktime
#if defined ( WIN32 )	
		TerminateQTML();
#endif
		
		Msg( "Quicktime Error when attempting to EnterMovies, err = %d \n", (int) status2 );
		Assert( false );
		
		m_CurrentStatus = VideoSystemStatus::INITIALIZATION_ERROR;
		return true;
	}
    
	m_CurrentStatus	 = VideoSystemStatus::OK;
	m_AvailableFeatures = DEFAULT_FEATURE_SET;

	// if the Library load didn't hook up the compression functions, remove them from our feature list
#pragma warning ( disable : 4551 )
	
    if ( !CompressImage )
    {
		m_AvailableFeatures = m_AvailableFeatures & ~( VideoSystemFeature::ENCODE_VIDEO_TO_FILE | VideoSystemFeature::ENCODE_AUDIO_TO_FILE );
    }
#pragma warning ( default : 4551 )

	// Note that we are now open for business....	
	m_bQuickTimeInitialized = true;
	SetResult( VideoResult::SUCCESS );
    
	return true;
}


bool CQuickTimeVideoSubSystem::ShutdownQuickTime()
{
	if ( m_bQuickTimeInitialized && m_CurrentStatus == VideoSystemStatus::OK )
	{
		ExitMovies();                               // Terminate QuickTime  

		// Windows PC only shutdown			
#if defined ( WIN32 )
		  TerminateQTML();                          // Terminate QTML  
#endif
	}
				
	m_bQuickTimeInitialized = false;
	m_CurrentStatus = VideoSystemStatus::NOT_INITIALIZED;
	m_AvailableFeatures = VideoSystemFeature::NO_FEATURES;
	SetResult( VideoResult::SUCCESS );

	return true;
}


// ===========================================================================
// Functions defined in Quicktime Common
// ===========================================================================

// makes a copy of a string
char *COPY_STRING( const char *pString )
{
	if ( pString == nullptr )
	{
		return nullptr;
	}

	size_t strLen = V_strlen( pString );
	
	char *pNewStr = new char[ strLen+ 1 ];
	if ( strLen > 0 )
	{
		V_memcpy( pNewStr, pString, strLen );
	}
	pNewStr[strLen] = nullchar;

	return pNewStr;
}




//-----------------------------------------------------------------------------
// Adapted from QuickTime Frame Rate code from Apple OSX Reference Library
// found at http://developer.apple.com/library/mac/#qa/qa2001/qa1262.html
//-----------------------------------------------------------------------------

#define   kCharacteristicHasVideoFrameRate  FOUR_CHAR_CODE('vfrr')
#define   kCharacteristicIsAnMpegTrack		FOUR_CHAR_CODE('mpeg')

bool MediaGetStaticFrameRate( Media inMovieMedia, VideoFrameRate_t &theFrameRate, bool AssumeConstantIntervals );

//-----------------------------------------------------------------------------
bool MovieGetStaticFrameRate( Movie inMovie, VideoFrameRate_t &theFrameRate )
{
	theFrameRate.Clear();

	AssertExitF( inMovie != nullptr );

	Boolean			isMPEG = false;
	Media			movieMedia = nullptr;
	MediaHandler	movieMediaHandler = nullptr;
	bool			success = false;


	// get the media identifier for the media that contains the first
	//  video track's sample data, and also get the media handler for this media.

	Track videoTrack = GetMovieIndTrackType( inMovie, 1, kCharacteristicHasVideoFrameRate, movieTrackCharacteristic | movieTrackEnabledOnly ); 	// get first video track 
	if ( videoTrack == nullptr || GetMoviesError() != noErr )
		goto error_out;
			
	// get media ref. for track's sample data 
	movieMedia = GetTrackMedia( videoTrack );
	if ( movieMedia == nullptr || GetMoviesError() != noErr )
		goto error_out;
		
	// get a reference to the media handler component
	movieMediaHandler = GetMediaHandler( movieMedia );
	if ( movieMediaHandler == nullptr || GetMoviesError() != noErr )
		goto error_out;

	// is this the MPEG-1/MPEG-2 media handler?
	if ( MediaHasCharacteristic( movieMediaHandler, kCharacteristicIsAnMpegTrack, &isMPEG ) != noErr )
		goto error_out;
		
	if (isMPEG)	// working with MPEG-1/MPEG-2 media
	{
		MHInfoEncodedFrameRateRecord encodedFrameRate;
		Size encodedFrameRateSize = sizeof( encodedFrameRate );

		// get the static frame rate
		if ( MediaGetPublicInfo( movieMediaHandler, kMHInfoEncodedFrameRate, &encodedFrameRate, &encodedFrameRateSize ) != noErr )
			goto error_out;

		TimeScale 	MovieTimeScale = GetMovieTimeScale( inMovie );
		
		Assert( MovieTimeScale > 0 && encodedFrameRate.encodedFrameRate > 0 );
		
		theFrameRate.SetRaw( MovieTimeScale, int ( (double) MovieTimeScale / Fix2X( encodedFrameRate.encodedFrameRate ) + 0.5 ) );
	}
	else  // working with non-MPEG-1/MPEG-2 media
	{
		if ( !MediaGetStaticFrameRate( movieMedia, theFrameRate, true ) )
			goto error_out;
	}
		
	
	success = true;
	
error_out:

	return success;
}



// ============================================================================
// Given a reference to the media that contains the sample data for a track,
// calculate the static frame rate.
// ============================================================================
bool MediaGetStaticFrameRate( Media inMovieMedia, VideoFrameRate_t &theFrameRate, bool AssumeConstantIntervals )
{
	Assert( inMovieMedia != nullptr );
	
	theFrameRate.Clear();

	// Method #1 - from Apple 
	if ( !AssumeConstantIntervals )	
	{
		// get the number of samples in the media
		long sampleCount = GetMediaSampleCount( inMovieMedia );
		if ( GetMoviesError() != noErr || sampleCount == 0 )
			return false;

		// find the media duration
		TimeValue64 duration = GetMediaDisplayDuration( inMovieMedia );
		if ( GetMoviesError() != noErr || duration == 0 )
			return false;

		// get the media time scale
		TimeValue64 timeScale = GetMediaTimeScale( inMovieMedia );
		if ( GetMoviesError() != noErr || timeScale == 0 )
			return false;

		// calculate the frame rate: = (sample count * media time scale) / media duration
		float FPS = (double) sampleCount * (double) timeScale / (double) duration;

		theFrameRate.SetFPS( FPS );
		return true;
	}
	
	// FPS rate method #2 - assumes all frames are at a constant interval
	// gets FPS in terms of units per second (preferred)
		
	TimeValue64  sample_time     = 0;
	TimeValue64  sample_duration = -1;
	
	GetMediaNextInterestingDisplayTime( inMovieMedia, nextTimeMediaSample | nextTimeEdgeOK, (TimeValue64) 0 , fixed1, &sample_time, &sample_duration );
	if ( sample_time == -1 || sample_duration == 0 || GetMoviesError() != noErr )
		return false;

	TimeValue64  sample_time2     = 0;
	TimeValue64  sample_duration2 = -1;

	GetMediaNextInterestingDisplayTime( inMovieMedia, nextTimeMediaSample | nextTimeEdgeOK, sample_time + sample_duration , fixed1, &sample_time2, &sample_duration2 );
	if ( sample_time2 == -1 || sample_duration2 == 0 || GetMoviesError() != noErr )
		return false;

	TimeScale	MediaTimeScale = GetMediaTimeScale( inMovieMedia );
	if ( MediaTimeScale <= 0 || GetMoviesError() != noErr )
		return false;

	Assert( sample_time2 == sample_time + sample_duration );
	Assert( sample_duration == sample_duration2 );

	theFrameRate.SetRaw( MediaTimeScale, (int) sample_duration );

	return true;
}



// ============================================================================
// SetGWorldDecodeGamma - configure a GWorld to perform any needed gamma 
//  correction
// 
//   kQTUsePlatformDefaultGammaLevel = 0,  /* When decompressing into this PixMap, gamma-correct to the platform's standard gamma. */
//   kQTUseSourceGammaLevel        = -1L,  /* When decompressing into this PixMap, don't perform gamma-correction. */
//   kQTCCIR601VideoGammaLevel     = 0x00023333 /* 2.2, standard television video gamma.*/
//   Fixed cGamma1_8 = 0x0001CCCC;		// Gamma 1.8
//   Fixed cGamma2_5 = 0x00028000;      // Gamma 2.5
// ============================================================================

bool SetGWorldDecodeGamma( CGrafPtr theGWorld, VideoPlaybackGamma_t gamma )
{
	AssertExitF( theGWorld != nullptr );
	AssertIncRange( gamma, VideoPlaybackGamma::USE_GAMMA_CONVAR, VideoPlaybackGamma::GAMMA_2_5 );

	Fixed decodeGamma = kQTUseSourceGammaLevel;

	if ( gamma == VideoPlaybackGamma::USE_GAMMA_CONVAR )
	{
		int useGamma = QuickTime_PlaybackGamma.GetInt();
		
		if ( useGamma < (int) VideoPlaybackGamma::NO_GAMMA_ADJUST || useGamma >= VideoPlaybackGamma::GAMMA_COUNT )
			return false;
			
		gamma = (VideoPlaybackGamma_t) useGamma;
	}
	
	switch( gamma )
	{
		case VideoPlaybackGamma::NO_GAMMA_ADJUST:
		{
			decodeGamma = kQTUseSourceGammaLevel;
			break;
		}
		case VideoPlaybackGamma::PLATFORM_DEFAULT_GAMMMA:
		{
			decodeGamma = kQTUsePlatformDefaultGammaLevel;
			break;
		}
		case VideoPlaybackGamma::GAMMA_1_8:
		{
			decodeGamma = 0x0001CCCC;		// Gamma 1.8
			break;
		}
		
		case VideoPlaybackGamma::GAMMA_2_2:
		{
			decodeGamma = 0x00023333;		// Gamma 2.2
			break;
		}
		case VideoPlaybackGamma::GAMMA_2_5:
		{
			decodeGamma = 0x00028000;      // Gamma 2.5
			break;
		}
		default:
		{
			Assert( false );
			break;
		}
	}

	// Get the pix map for the GWorld and adjust the gamma correction on it
	PixMapHandle thePixMap = GetGWorldPixMap( theGWorld );
	AssertExitF( thePixMap != nullptr );

	// Set the Gamma level for the pixmap
	OSErr Status = QTSetPixMapHandleGammaLevel( thePixMap, decodeGamma );
	AssertExitF( Status == noErr );

	// Set the Requested Gamma level for the pixmap
	Status = QTSetPixMapHandleRequestedGammaLevel( thePixMap, decodeGamma );
	AssertExitF( Status == noErr );

	return true;
}


// ============================================================================
// Setup a quicktime Audio Context for a movie
// ============================================================================
bool CreateMovieAudioContext( bool enableAudio, Movie inMovie, QTAudioContextRef *pAudioContext, bool setVolume, float *pCurrentVolume )
{
	AssertExitF( inMovie != nullptr && pAudioContext != nullptr );

	if ( enableAudio )
	{
#if defined ( WIN32 )
		WCHAR strGUID[39];
		int numBytes = StringFromGUID2( DSDEVID_DefaultPlayback, (LPOLESTR) strGUID, 39);			// CLSID_DirectSound is not what you want here

		// create the audio context
		CFStringRef deviceNameStrRef = CFStringCreateWithCharacters(kCFAllocatorDefault, (const UniChar*) strGUID, (CFIndex) (numBytes -1) );

		OSStatus result = QTAudioContextCreateForAudioDevice( NULL, deviceNameStrRef, NULL, pAudioContext );
		AssertExitF( result == noErr );
		
		SAFE_RELEASE_CFREF( deviceNameStrRef );
		
#elif defined ( OSX )
	
	    OSStatus result = QTAudioContextCreateForAudioDevice( NULL, NULL, NULL, pAudioContext );
		AssertExitF( result == noErr );
#endif
		// valid?
		AssertPtr( *pAudioContext );
	}
	else	// no audio
	{
		*pAudioContext = nullptr;
	}
	
	// Set the audio context
    OSStatus result = SetMovieAudioContext( inMovie, *pAudioContext );
    AssertExitF( result == noErr );
	
	if ( setVolume && *pAudioContext != nullptr )
	{
		ConVarRef volumeConVar( "volume" );
		float sysVolume = ( volumeConVar.IsValid() ) ? volumeConVar.GetFloat() : 1.0f;
		clamp( sysVolume, 0.0f, 1.0f );
		
		if ( pCurrentVolume != nullptr )
		{
			*pCurrentVolume = sysVolume;
		}
	
		short  movieVolume = (short) ( sysVolume * 256.0f );
	
		SetMovieVolume( inMovie, movieVolume );
	}

	return true;
}
