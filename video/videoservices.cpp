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
#include "vgui/ILocalize.h"
#include "vtf/vtf.h"
#include "pixelwriter.h"
#include "tier3/tier3.h"
#include "platform.h"

#include "videoservices.h"
#include "video_macros.h"

#include "tier0/memdbgon.h"

#if defined( WIN32 )
	#include <windows.h>
#elif defined( OSX )
	#include <Carbon/Carbon.h>
#endif

#if defined( USE_SDL )
	#include "SDL.h"
	#include "appframework/ilaunchermgr.h"
#endif

//-----------------------------------------------------------------------------
// Platform specific video system controls & definitions
//-----------------------------------------------------------------------------

enum EPlatform_t
{
	PLATFORM_NONE		= 0,
	PLATFORM_WIN32		= 0x01,
	PLATFORM_OSX		= 0x02,
	PLATFORM_XBOX_360	= 0x04,
	PLATFORM_PS3		= 0x08,
	PLATFORM_LINUX		= 0x10
};

DEFINE_ENUM_BITWISE_OPERATORS( EPlatform_t );

#if defined( IS_WINDOWS_PC )
	const EPlatform_t	thisPlatform = PLATFORM_WIN32;
#elif defined( OSX )
	const EPlatform_t	thisPlatform = PLATFORM_OSX;
#elif defined( _X360 )
	const EPlatform_t	thisPlatform = PLATFORM_XBOX_360;
#elif defined( _PS3 )
	const EPlatform_t	thisPlatform = PLATFORM_PS3;
#elif defined ( _LINUX ) 
	const EPlatform_t	thisPlatform = PLATFORM_LINUX;
#else
  #error "UNABLE TO DETERMINE PLATFORM"
#endif


#if defined( OSX ) || defined( LINUX )
ILauncherMgr *g_pLauncherMgr = NULL;
#endif


struct VideoSystemInfo_t
{
	VideoSystem_t	m_SystemID;
	EPlatform_t		m_Platforms;
	const char		*m_pModuleName;
	const char		*m_pInterfaceName;
};

static VideoSystemInfo_t s_VideoAppSystems[] = 
{
	{ VideoSystem::QUICKTIME,	PLATFORM_WIN32 | PLATFORM_OSX,							"video_quicktime",  VIDEO_SUBSYSTEM_INTERFACE_VERSION },
	{ VideoSystem::BINK,		PLATFORM_WIN32 | PLATFORM_OSX | PLATFORM_XBOX_360 | PLATFORM_LINUX,		"video_bink",	    VIDEO_SUBSYSTEM_INTERFACE_VERSION },
	//{ VideoSystem::AVI,			PLATFORM_WIN32,											"avi",				VIDEO_SUBSYSTEM_INTERFACE_VERSION },
	//{ VideoSystem::WMV,			PLATFORM_WIN32,											"wmv",				VIDEO_SUBSYSTEM_INTERFACE_VERSION },
	{ VideoSystem::WEBM,	PLATFORM_LINUX,							"video_webm",  VIDEO_SUBSYSTEM_INTERFACE_VERSION },
	
	{ VideoSystem::NONE,		PLATFORM_NONE, nullptr, nullptr }			// Required to terminate the list
};



//-----------------------------------------------------------------------------
// Setup Singleton for accessing Valve Video Services
//-----------------------------------------------------------------------------
static CValveVideoServices g_VALVeVIDEO;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CValveVideoServices, IVideoServices, VIDEO_SERVICES_INTERFACE_VERSION, g_VALVeVIDEO );


static CVideoCommonServices g_VALVEVIDEOCommon;


//-----------------------------------------------------------------------------
// Valve Video Services implementation
//-----------------------------------------------------------------------------
CValveVideoServices::CValveVideoServices() :
	m_nInstalledSystems( 0 ),
	m_bInitialized( false ),
	m_nMaterialCount( 0 )
{
	for ( int i = 0; i < VideoSystem::VIDEO_SYSTEM_COUNT; i++ )
	{
		m_VideoSystemModule[i]	= nullptr;
		m_VideoSystems[i]		= nullptr;
		m_VideoSystemType[i]	= VideoSystem::NONE;
		m_VideoSystemFeatures[i] = VideoSystemFeature::NO_FEATURES;
	}

}


CValveVideoServices::~CValveVideoServices()
{
	DisconnectVideoLibraries( );
}
	

bool CValveVideoServices::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
	{
		return false;
	}

	if ( g_pFullFileSystem == nullptr || materials == nullptr )
	{
		Msg( "Valve Video Services unable to connect due to missing dependent system\n" );
		return false;
	}

#if defined( USE_SDL )
	g_pLauncherMgr = (ILauncherMgr *)factory( SDLMGR_INTERFACE_VERSION, NULL );
#endif
	
	if ( !ConnectVideoLibraries( factory ) )
	{
		return false;
	}
	
	return ( true );
}


void CValveVideoServices::Disconnect()
{
	DisconnectVideoLibraries();
}


void* CValveVideoServices::QueryInterface( const char *pInterfaceName )
{
	if ( Q_strncmp(	pInterfaceName, VIDEO_SERVICES_INTERFACE_VERSION, Q_strlen( VIDEO_SERVICES_INTERFACE_VERSION ) + 1) == 0 )
	{
		return (IVideoServices*) this;
	}

	return nullptr;
}


bool CValveVideoServices::ConnectVideoLibraries( CreateInterfaceFn factory )
{
	// Don't connect twice..
	AssertExitF( m_bInitialized == false );

	int n = 0;
	
	while ( IS_NOT_EMPTY( s_VideoAppSystems[n].m_pModuleName ) && s_VideoAppSystems[n].m_SystemID != VideoSystem::NONE )
	{
		if (BITFLAGS_SET( s_VideoAppSystems[n].m_Platforms, thisPlatform ) )
		{
			bool success = false;
			CSysModule *pModule = Sys_LoadModule(s_VideoAppSystems[n].m_pModuleName );
			if( pModule != nullptr )
			{
				CreateInterfaceFn fn = Sys_GetFactory( pModule );
				if ( fn != nullptr )
				{
				
					IVideoSubSystem *pVideoSystem = (IVideoSubSystem*) fn( s_VideoAppSystems[n].m_pInterfaceName, NULL );
					if ( pVideoSystem != nullptr && pVideoSystem->Connect( factory ) )
					{
						if ( pVideoSystem->InitializeVideoSystem( &g_VALVEVIDEOCommon ) )
						{
							int slotNum = (int) pVideoSystem->GetSystemID();

							if ( IS_IN_RANGECOUNT( slotNum, VideoSystem::VIDEO_SYSTEM_FIRST, VideoSystem::VIDEO_SYSTEM_COUNT ) )
							{
								Assert( m_VideoSystemModule[slotNum] == nullptr );
								m_VideoSystemModule[slotNum] = pModule;
								m_VideoSystems[slotNum] = pVideoSystem;
								
								m_nInstalledSystems++;
								success = true;
							}
						}
					}
				}
				
				if ( success == false )
				{
					
					Msg( "Error occurred while attempting to load and initialize Video Subsystem\n Video Subsystem module '%s'\n Video Subsystem Interface  '%s'", s_VideoAppSystems[n].m_pModuleName, s_VideoAppSystems[n].m_pInterfaceName );
					Sys_UnloadModule( pModule );
				}
			}			
		}
		
		n++;			
	}			

	// now we query each video system for its capabilities, and supported file extensions
	for ( int i = VideoSystem::VIDEO_SYSTEM_FIRST; i < VideoSystem::VIDEO_SYSTEM_COUNT; i++ )
	{
		IVideoSubSystem *pSubSystem = m_VideoSystems[i];
		if ( pSubSystem != nullptr )
		{
			m_VideoSystemType[i]		= pSubSystem->GetSystemID();
			m_VideoSystemFeatures[i]	= pSubSystem->GetSupportedFeatures();
			
			// get every file extension it handles, and the info about it
			int eCount = pSubSystem->GetSupportedFileExtensionCount();
			Assert( eCount > 0 );
			
			for ( int n = 0; n < eCount; n++ )
			{
				VideoFileExtensionInfo_t	extInfoRec;
				
				extInfoRec.m_FileExtension = pSubSystem->GetSupportedFileExtension( n );
				extInfoRec.m_VideoSubSystem = pSubSystem->GetSystemID();
				extInfoRec.m_VideoFeatures = pSubSystem->GetSupportedFileExtensionFeatures( n );
				
				AssertPtr( extInfoRec.m_FileExtension );
				
				m_ExtInfo.AddToTail( extInfoRec );
			}
		}
	}

	m_bInitialized = true;
	
	return true;
}


bool CValveVideoServices::DisconnectVideoLibraries()
{
	if ( !m_bInitialized )
	{
		return false;
	}

	// free up any objects/resources still out there
	DestroyAllVideoInterfaces();

	for ( int i = 0; i < VideoSystem::VIDEO_SYSTEM_COUNT; i++ )
	{
		if ( m_VideoSystems[i] != nullptr )
		{
			m_VideoSystems[i]->ShutdownVideoSystem();
			m_VideoSystems[i]->Disconnect();
			m_VideoSystems[i] = nullptr;
		}
		
		if ( m_VideoSystemModule[i] != nullptr )
		{
			Sys_UnloadModule( m_VideoSystemModule[i] );
			m_VideoSystemModule[i] = nullptr;
		}
	
		m_VideoSystemType[i]	 = VideoSystem::NONE;
		m_VideoSystemFeatures[i] = VideoSystemFeature::NO_FEATURES;
	}

	m_bInitialized = false;
	
	return true;
}


int CValveVideoServices::DestroyAllVideoInterfaces()
{
	int n = m_RecorderList.Count() + m_MaterialList.Count();

	for ( int i = m_RecorderList.Count() -1; i >= 0; i-- )
	{
		DestroyVideoRecorder( (IVideoRecorder*) m_RecorderList[i].m_pObject );
	}

	for ( int i = m_MaterialList.Count() -1; i >= 0; i-- )
	{
		DestroyVideoMaterial( (IVideoMaterial*) m_MaterialList[i].m_pObject );
	}
	
	return n;
}


InitReturnVal_t CValveVideoServices::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
	{
		return nRetVal;
	}

	// Initialize all loaded subsystems
	for ( int n = VideoSystem::VIDEO_SYSTEM_FIRST; n < VideoSystem::VIDEO_SYSTEM_COUNT; n++ )
	{
		if ( m_VideoSystems[n] != nullptr )
		{
			nRetVal = m_VideoSystems[n]->Init();
			if ( nRetVal != INIT_OK )
			{
				return nRetVal;
			}
		}
	}

	return INIT_OK;
}


void CValveVideoServices::Shutdown()
{
	DestroyAllVideoInterfaces();

	// Shutdown all loaded subsystems
	for ( int n = VideoSystem::VIDEO_SYSTEM_FIRST; n < VideoSystem::VIDEO_SYSTEM_COUNT; n++ )
	{
		if ( m_VideoSystems[n] != nullptr )
		{
			m_VideoSystems[n]->Shutdown();
		}
	}
	
	BaseClass::Shutdown();
}
		
	
// ===========================================================================	
// Inherited from IVideoServices
// ===========================================================================	
	
// Query the available video systems
int CValveVideoServices::GetAvailableVideoSystemCount()
{
	return m_nInstalledSystems;
}


// returns the enumerated video system, *IF* it is installed and working
VideoSystem_t CValveVideoServices::GetAvailableVideoSystem( int n )
{
	if ( n< 0 || n >= m_nInstalledSystems ) 
	{
		return VideoSystem::NONE;
	}
	
	for ( int i = VideoSystem::VIDEO_SYSTEM_FIRST, c = 0; i < VideoSystem::VIDEO_SYSTEM_COUNT; i++ )
	{
		if ( m_VideoSystems[i] != nullptr )
		{
			if ( c == n ) 
			{
				return m_VideoSystemType[i];
			}
			c++;
		}
	}
	
	return VideoSystem::NONE;
}


// ===========================================================================	
// returns the index for the video system...
// ... provided that system is installed and available to do something
// ===========================================================================	
int CValveVideoServices::GetIndexForSystem( VideoSystem_t n )
{
	if ( n >= VideoSystem::VIDEO_SYSTEM_FIRST && n < VideoSystem::VIDEO_SYSTEM_COUNT && m_nInstalledSystems > 0 )
	{
		int i = (int) n;
		if ( m_VideoSystems[i] != nullptr && m_VideoSystemFeatures[i] != VideoSystemFeature::NO_FEATURES )
		{
			return i;
		}
	}
	
	return SYSTEM_NOT_FOUND;
}


VideoSystem_t CValveVideoServices::GetSystemForIndex( int n )
{
	if ( n >= VideoSystem::VIDEO_SYSTEM_FIRST && n < VideoSystem::VIDEO_SYSTEM_COUNT && m_nInstalledSystems > 0 )
	{
		if ( m_VideoSystems[n] != nullptr && m_VideoSystemFeatures[n] != VideoSystemFeature::NO_FEATURES )
		{
			return (VideoSystem_t) n;
		}
	}

	return VideoSystem::NONE;
}


// ===========================================================================	
// video system query functions
// ===========================================================================	
bool CValveVideoServices::IsVideoSystemAvailable( VideoSystem_t videoSystem )
{
	int n = GetIndexForSystem( videoSystem ); 
	return ( n != SYSTEM_NOT_FOUND ) ? true : false;
}


VideoSystemStatus_t CValveVideoServices::GetVideoSystemStatus( VideoSystem_t videoSystem )
{
	int n = GetIndexForSystem( videoSystem ); 
	return ( n!= SYSTEM_NOT_FOUND ) ? m_VideoSystems[n]->GetSystemStatus() : VideoSystemStatus::NOT_INSTALLED;
}


VideoSystemFeature_t CValveVideoServices::GetVideoSystemFeatures( VideoSystem_t videoSystem )
{
	int n = GetIndexForSystem( videoSystem ); 
	return ( n!= SYSTEM_NOT_FOUND ) ? m_VideoSystemFeatures[n] : VideoSystemFeature::NO_FEATURES;

}


const char *CValveVideoServices::GetVideoSystemName( VideoSystem_t videoSystem )
{
	int n = GetIndexForSystem( videoSystem ); 
	return ( n!= SYSTEM_NOT_FOUND ) ? m_VideoSystems[n]->GetVideoSystemName() : nullptr;
}


VideoSystem_t CValveVideoServices::FindNextSystemWithFeature( VideoSystemFeature_t features, VideoSystem_t startAfter )
{
	if ( ( features & VideoSystemFeature::ALL_VALID_FEATURES ) == 0 )
	{
		return VideoSystem::NONE;
	}

	int start = VideoSystem::VIDEO_SYSTEM_FIRST;
	if ( startAfter != VideoSystem::NONE && IS_IN_RANGECOUNT( startAfter, VideoSystem::VIDEO_SYSTEM_FIRST, VideoSystem::VIDEO_SYSTEM_COUNT ) )
	{
		start = (int) startAfter;
	}

	for ( int i = start; i < VideoSystem::VIDEO_SYSTEM_COUNT; i++ )
	{
		if ( m_VideoSystems[i] != nullptr && BITFLAGS_SET( m_VideoSystemFeatures[i], features )	)
		{
			return (VideoSystem_t) i;
		}
	}
	
	return VideoSystem::NONE;
}


// ===========================================================================	
// video services status functions
// ===========================================================================	
VideoResult_t CValveVideoServices::GetLastResult()
{
	return m_LastResult;
}


VideoResult_t CValveVideoServices::SetResult( VideoResult_t resultCode )
{
	m_LastResult = resultCode;
	return resultCode;
}
	
		
// ===========================================================================	
// deal with video file extensions and video system mappings
// ===========================================================================	
int CValveVideoServices::GetSupportedFileExtensionCount( VideoSystem_t videoSystem )
{
	int n = GetIndexForSystem( videoSystem ); 
	
	return ( n == SYSTEM_NOT_FOUND ) ? 0 : m_VideoSystems[n]->GetSupportedFileExtensionCount();
}


const char *CValveVideoServices::GetSupportedFileExtension( VideoSystem_t videoSystem, int extNum )
{
	int n = GetIndexForSystem( videoSystem ); 

	int c = ( n == SYSTEM_NOT_FOUND ) ? 0 : m_VideoSystems[n]->GetSupportedFileExtensionCount();;
	
	return ( extNum < 0 || extNum >= c ) ? nullptr : m_VideoSystems[n]->GetSupportedFileExtension( extNum );
	
}


VideoSystemFeature_t CValveVideoServices::GetSupportedFileExtensionFeatures( VideoSystem_t videoSystem, int extNum )
{
	int n = GetIndexForSystem( videoSystem ); 

	int c = ( n == SYSTEM_NOT_FOUND ) ? 0 : m_VideoSystems[n]->GetSupportedFileExtensionCount();
	
	return ( extNum < 0 || extNum >= c ) ? VideoSystemFeature::NO_FEATURES : m_VideoSystems[n]->GetSupportedFileExtensionFeatures( extNum );
}


VideoSystem_t CValveVideoServices::LocateVideoSystemForPlayingFile( const char *pFileName, VideoSystemFeature_t playMode )
{
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitV( IS_NOT_EMPTY( pFileName ), VideoSystem::NONE );
	
	VideoSystem_t theSystem = LocateSystemAndFeaturesForFileName( pFileName, nullptr, playMode );

	SetResult( VideoResult::SUCCESS );
	return theSystem;
}


// ===========================================================================	
// Given a video file name, possibly with a set extension, locate the file
//   or a suitable substitute that is playable on the current system
// ===========================================================================	
VideoResult_t CValveVideoServices::LocatePlayableVideoFile( const char *pSearchFileName, const char *pPathID, VideoSystem_t *pPlaybackSystem, char *pPlaybackFileName, int fileNameMaxLen, VideoSystemFeature_t playMode )
{
	AssertExitV( IS_NOT_EMPTY( pSearchFileName ) || pPlaybackSystem == nullptr || pPlaybackSystem == nullptr || fileNameMaxLen <= 0, SetResult( VideoResult::BAD_INPUT_PARAMETERS ) );

	VideoResult_t Status = ResolveToPlayableVideoFile( pSearchFileName, pPathID, VideoSystem::DETERMINE_FROM_FILE_EXTENSION, playMode, 
									true, pPlaybackFileName, fileNameMaxLen, pPlaybackSystem );

	return SetResult( Status );
}



// ===========================================================================	
// Create/destroy a video material
// ===========================================================================	
IVideoMaterial* CValveVideoServices::CreateVideoMaterial( const char *pMaterialName, const char *pVideoFileName, const char *pPathID, VideoPlaybackFlags_t playbackFlags, VideoSystem_t videoSystem, bool PlayAlternateIfNotAvailable )
{
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitV( IS_NOT_EMPTY( pVideoFileName ), nullptr );
	AssertExitV( videoSystem == VideoSystem::DETERMINE_FROM_FILE_EXTENSION || IS_IN_RANGECOUNT( videoSystem, VideoSystem::VIDEO_SYSTEM_FIRST, VideoSystem::VIDEO_SYSTEM_COUNT ), nullptr );

	// We need to resolve the filename and video system

	char ResolvedFilePath[MAX_PATH];
	VideoSystem_t  actualVideoSystem = videoSystem;

	VideoResult_t Status = ResolveToPlayableVideoFile( pVideoFileName, pPathID, videoSystem, VideoSystemFeature::PLAY_VIDEO_FILE_IN_MATERIAL, PlayAlternateIfNotAvailable,
	                                                   ResolvedFilePath, sizeof(ResolvedFilePath), &actualVideoSystem );

	SetResult( Status );
	if ( Status != VideoResult::SUCCESS )
	{
		return nullptr;
	}

	int sysIndex = GetIndexForSystem( actualVideoSystem );
	
	if ( sysIndex == SYSTEM_NOT_FOUND )
	{
		SetResult( VideoResult::SYSTEM_ERROR_OCCURED );
		return nullptr;
	}
	
	// Create the video material
	IVideoMaterial *pMaterial = m_VideoSystems[sysIndex]->CreateVideoMaterial( pMaterialName, ResolvedFilePath, playbackFlags );

	// Update our list, and return	
	if ( pMaterial != nullptr )
	{
		CActiveVideoObjectRecord_t info;
		info.m_pObject = pMaterial;
		info.m_VideoSystem = sysIndex;
		m_MaterialList.AddToTail( info );
	}

	SetResult( m_VideoSystems[sysIndex]->GetLastResult() );
	return pMaterial;
}


VideoResult_t CValveVideoServices::DestroyVideoMaterial( IVideoMaterial* pVideoMaterial )
{
	AssertPtrExitV( pVideoMaterial, SetResult( VideoResult::BAD_INPUT_PARAMETERS ) );

	for ( int i = 0; i < m_MaterialList.Count(); i++ )
	{
		if ( m_MaterialList[i].m_pObject == pVideoMaterial )
		{
			VideoResult_t Status = m_VideoSystems[ m_MaterialList[i].m_VideoSystem ]->DestroyVideoMaterial( pVideoMaterial );
			m_MaterialList.Remove( i );
			
			return SetResult( Status );
		}
	}

	return SetResult( VideoResult::RECORDER_NOT_FOUND );


	return VideoResult::SUCCESS;
}


int CValveVideoServices::GetUniqueMaterialID()
{
	m_nMaterialCount++;
	return m_nMaterialCount;
}

// ===========================================================================	
// Query availabilily of codec for encoding video
// ===========================================================================	
VideoResult_t CValveVideoServices::IsRecordCodecAvailable( VideoSystem_t videoSystem, VideoEncodeCodec_t codec )
{
	AssertExitV( codec >= VideoEncodeCodec::DEFAULT_CODEC && codec < VideoEncodeCodec::CODEC_COUNT, SetResult( VideoResult::BAD_INPUT_PARAMETERS ) );

	int n = GetIndexForSystem( videoSystem );
	
	if ( n == SYSTEM_NOT_FOUND )
	{
		return SetResult( VideoResult::SYSTEM_NOT_AVAILABLE );
	}

	return m_VideoSystems[n]->CheckCodecAvailability( codec );
}


// ===========================================================================	
// Create/destroy a video encoder		
// ===========================================================================	
IVideoRecorder*	CValveVideoServices::CreateVideoRecorder( VideoSystem_t videoSystem )
{
	int n = GetIndexForSystem( videoSystem );
	
	if ( n == SYSTEM_NOT_FOUND )
	{
		SetResult( VideoResult::SYSTEM_NOT_AVAILABLE );
		return nullptr;
	}

	if ( !BITFLAGS_SET( m_VideoSystemFeatures[n], VideoSystemFeature::ENCODE_VIDEO_TO_FILE ) )
	{
		SetResult( VideoResult::FEATURE_NOT_AVAILABLE );
		return nullptr;
	}

	IVideoRecorder *pRecorder = m_VideoSystems[n]->CreateVideoRecorder();
	
	if ( pRecorder != nullptr )
	{
		CActiveVideoObjectRecord_t info;
		info.m_pObject = pRecorder;
		info.m_VideoSystem = n;
		m_RecorderList.AddToTail( info );
	}

	SetResult( m_VideoSystems[n]->GetLastResult() );
	return pRecorder;
}


VideoResult_t CValveVideoServices::DestroyVideoRecorder( IVideoRecorder *pVideoRecorder )
{
	AssertPtrExitV( pVideoRecorder, SetResult( VideoResult::BAD_INPUT_PARAMETERS ) );

	for ( int i = 0; i < m_RecorderList.Count(); i++ )
	{
		if ( m_RecorderList[i].m_pObject == pVideoRecorder )
		{
			VideoResult_t Status = m_VideoSystems[ m_RecorderList[i].m_VideoSystem ]->DestroyVideoRecorder( pVideoRecorder );
			m_RecorderList.Remove( i );
			
			return SetResult( Status );
		}
	}

	return SetResult( VideoResult::RECORDER_NOT_FOUND );

}


// ===========================================================================	
// Plays a given video file until it completes or the user aborts
// ===========================================================================	
VideoResult_t CValveVideoServices::PlayVideoFileFullScreen( const char *pFileName, const char *pPathID, void *mainWindow, int windowWidth, int windowHeight, int desktopWidth, int desktopHeight, bool windowed, float forcedMinTime, VideoPlaybackFlags_t playbackFlags, VideoSystem_t videoSystem, bool PlayAlternateIfNotAvailable )
{
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitV( IS_NOT_EMPTY( pFileName ), VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitV( videoSystem == VideoSystem::DETERMINE_FROM_FILE_EXTENSION || IS_IN_RANGECOUNT( videoSystem, VideoSystem::VIDEO_SYSTEM_FIRST, VideoSystem::VIDEO_SYSTEM_COUNT ), VideoResult::BAD_INPUT_PARAMETERS );

	char ResolvedFilePath[MAX_PATH];
	VideoSystem_t  actualVideoSystem = videoSystem;

	VideoResult_t Status = ResolveToPlayableVideoFile( pFileName, pPathID, videoSystem, VideoSystemFeature::PLAY_VIDEO_FILE_FULL_SCREEN, PlayAlternateIfNotAvailable,
	                                                   ResolvedFilePath, sizeof(ResolvedFilePath), &actualVideoSystem );
	                                                   
	if ( Status != VideoResult::SUCCESS )
	{
		return Status;
	}

	int sysIndex = GetIndexForSystem( actualVideoSystem );
	
	if ( sysIndex != SYSTEM_NOT_FOUND )
	{
		return SetResult( m_VideoSystems[sysIndex]->PlayVideoFileFullScreen( ResolvedFilePath, mainWindow, windowWidth, windowHeight, desktopWidth, desktopHeight, windowed, forcedMinTime, playbackFlags ) );
	}
	else
	{
		return SetResult( VideoResult::SYSTEM_ERROR_OCCURED );
	}
	
}


// ===========================================================================	
// Functions to connect sound systems to video systems
// ===========================================================================	
VideoResult_t CValveVideoServices::SoundDeviceCommand( VideoSoundDeviceOperation_t operation, void *pDevice, void *pData, VideoSystem_t videoSystem )
{
	AssertExitV( IS_IN_RANGECOUNT( operation, 0, VideoSoundDeviceOperation::OPERATION_COUNT ), SetResult( VideoResult::BAD_INPUT_PARAMETERS ) );
	
	AssertExitV( videoSystem == VideoSystem::ALL_VIDEO_SYSTEMS || IS_IN_RANGECOUNT( videoSystem, VideoSystem::VIDEO_SYSTEM_FIRST, VideoSystem::VIDEO_SYSTEM_COUNT ), SetResult( VideoResult::BAD_INPUT_PARAMETERS ) );

	int startIdx = (int)  VideoSystem::VIDEO_SYSTEM_FIRST;
	int lastIdx = (int) VideoSystem::VIDEO_SYSTEM_COUNT - 1;
	
	if ( videoSystem != VideoSystem::ALL_VIDEO_SYSTEMS )
	{
		startIdx = lastIdx = GetIndexForSystem( videoSystem );
		if ( startIdx == SYSTEM_NOT_FOUND )
		{
			return SetResult( VideoResult::SYSTEM_NOT_AVAILABLE );
		}
	}
	
	VideoResult_t result = VideoResult::SYSTEM_NOT_AVAILABLE;
	
	for ( int i = startIdx; i <= lastIdx; i++ )
	{
		int n = GetIndexForSystem( (VideoSystem_t) i );
		if ( n != SYSTEM_NOT_FOUND )
		{
			result = m_VideoSystems[n]->VideoSoundDeviceCMD( operation, pDevice, pData );
		}
	}
	
	return SetResult( result );
}


// ===========================================================================	
// Sets the sound devices that the video will decode to
// ===========================================================================	
const wchar_t *CValveVideoServices::GetCodecName( VideoEncodeCodec_t nCodec )
{
	static const char *s_pCodecLookup[VideoEncodeCodec::CODEC_COUNT] =
	{
		"#Codec_MPEG2",
		"#Codec_MPEG4",
		"#Codec_H261",
		"#Codec_H263",
		"#Codec_H264",
		"#Codec_MJPEG_A",
		"#Codec_MJPEG_B",
		"#Codec_SORENSON3",
		"#Codec_CINEPACK",
		"#Codec_WEBM",
	};

	if ( nCodec < 0 || nCodec >= VideoEncodeCodec::CODEC_COUNT )
	{
		AssertMsg( 0, "Invalid codec in CValveVideoServices::GetCodecName()" );
		return NULL;
	}

	return g_pVGuiLocalize->Find( s_pCodecLookup[ nCodec ] );
}

// ===========================================================================	
// Functions to determine which file and video system to use
// ===========================================================================	
VideoResult_t CValveVideoServices::ResolveToPlayableVideoFile( const char *pFileName, const char *pPathID, VideoSystem_t videoSystem, VideoSystemFeature_t requiredFeature, 
									bool PlayAlternateIfNotAvailable, char *pResolvedFileName, int resolvedFileNameMaxLen, VideoSystem_t *pResolvedVideoSystem )
{
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitV( IS_NOT_EMPTY( pFileName ), VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitV( videoSystem == VideoSystem::DETERMINE_FROM_FILE_EXTENSION || IS_IN_RANGECOUNT( videoSystem, VideoSystem::VIDEO_SYSTEM_FIRST, VideoSystem::VIDEO_SYSTEM_COUNT ), VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitV( requiredFeature != VideoSystemFeature::NO_FEATURES, VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitV( pResolvedFileName != nullptr && resolvedFileNameMaxLen > 0 && pResolvedVideoSystem != nullptr, VideoResult::BAD_INPUT_PARAMETERS );
	
	// clear results should we return failure
	pResolvedFileName[0] = nullchar;
	*pResolvedVideoSystem = VideoSystem::NONE;
	
	int sysIdx = SYSTEM_NOT_FOUND;
	VideoSystemFeature_t sysFeatures = VideoSystemFeature::NO_FEATURES;

	// check the file extension to see if it specifies searching for any compatible video files
	// if so, override a couple input values
	if ( !IsMatchAnyExtension( pFileName ) )
	{
		goto search_for_video;
	}
	
	// is the requested video system available?
	
	// We start with either the specified video system.. OR.. we choose the system based on the file extension
	// Get the system and if it's valid, it's available features
	if ( videoSystem != VideoSystem::DETERMINE_FROM_FILE_EXTENSION )
	{	
		sysIdx		= GetIndexForSystem( videoSystem );				// Caller specified the video system 
		sysFeatures = ( sysIdx != SYSTEM_NOT_FOUND ) ? m_VideoSystemFeatures[sysIdx] : VideoSystemFeature::NO_FEATURES;
	}
	else
	{
		// We need to determine the system to use based on filename
		sysIdx = GetIndexForSystem( LocateSystemAndFeaturesForFileName( pFileName, &sysFeatures, requiredFeature ) );
	}

	// if we don't have a system to play this video.. and aren't allowed to look for an alternative...
	if ( sysIdx == SYSTEM_NOT_FOUND && PlayAlternateIfNotAvailable == false )
	{
		return SetResult( VideoResult::VIDEO_SYSTEM_NOT_FOUND );		// return failure
	}

	char ActualFilePath[MAX_PATH];
	
	// Examine the requested of inferred video system to see if it can do what we want, 
	// and if so, see if the corresponding file is actually found (we support search paths)
	
	// Decision Path for when we have a preferred/specified video system specified to use
	if ( sysIdx != SYSTEM_NOT_FOUND )	
	{
		bool  fileFound = false;

		// if the request system can do the task, see if we can find the file as supplied by the caller
		if ( BITFLAGS_SET( sysFeatures, requiredFeature ) )
		{
			if ( V_IsAbsolutePath( pFileName ) )
			{
				V_strncpy( ActualFilePath, pFileName, sizeof( ActualFilePath ) );
				fileFound = g_pFullFileSystem->FileExists( pFileName, nullptr );
			}
			else 
			{
				fileFound = ( g_pFullFileSystem->RelativePathToFullPath( pFileName, pPathID, ActualFilePath, sizeof( ActualFilePath ) ) != nullptr );
			}
		}
		else	// The specified video system does not support this (required) feature
		{
			// if we can't search for an alternative file, tell them we don't support this
			if ( !PlayAlternateIfNotAvailable )
			{
				return SetResult( VideoResult::FEATURE_NOT_AVAILABLE );
			}
		}

		// We found the specified file, and the video system has the feature support
		if ( fileFound )
		{
			// copy the resolved filename and system and report success
			V_strncpy( pResolvedFileName, ActualFilePath, resolvedFileNameMaxLen );
			*pResolvedVideoSystem = GetSystemForIndex( sysIdx );
			return SetResult( VideoResult::SUCCESS );
		}

		// ok, we have the feature support but didn't find the file to use...
		if ( !PlayAlternateIfNotAvailable )
		{
			// if we can't search for an alternate file, so report file not found
			return SetResult( VideoResult::VIDEO_FILE_NOT_FOUND );
		}
	}

	// Ok, we didn't find the file and a system that could handle it
	// but hey, we are allowed to look for an alternate video file and system

search_for_video:

	// start with the passed in filespec, and change the extension to wildcard
	char SearchFileSpec[MAX_PATH];
	V_strncpy( SearchFileSpec, pFileName, sizeof(SearchFileSpec) );
	V_SetExtension( SearchFileSpec, ".*", sizeof(SearchFileSpec) );
	
	FileFindHandle_t  searchHandle = 0;

	const char *pMatchingFile = g_pFullFileSystem->FindFirstEx( SearchFileSpec, pPathID, &searchHandle );
	
	while ( pMatchingFile != nullptr )
	{
		const char *pExt = GetFileExtension( pMatchingFile );
		
		if ( pExt != nullptr )
		{
			// compare file extensions
			for ( int i = 0; i < m_ExtInfo.Count(); i++ )
			{
				// do we match a known extension?
				if ( stricmp( pExt, m_ExtInfo[i].m_FileExtension ) == STRINGS_MATCH )
				{
					// do we support the requested feature?
					if ( BITFLAGS_SET( m_ExtInfo[i].m_VideoFeatures, requiredFeature ) )
					{
						// Make sure it's a valid system
						sysIdx = GetIndexForSystem( m_ExtInfo[i].m_VideoSubSystem );
						if ( sysIdx != SYSTEM_NOT_FOUND )
						{
							
							// Start with any optional path we got...
							V_ExtractFilePath( pFileName, ActualFilePath, sizeof( ActualFilePath ) );
							// Append the search match file							
							V_strncat( ActualFilePath, pMatchingFile, sizeof( ActualFilePath ) );
							
							if ( V_IsAbsolutePath( ActualFilePath ) )
							{
								V_strncpy( pResolvedFileName, ActualFilePath, resolvedFileNameMaxLen );
							}
							else
							{
								g_pFullFileSystem->RelativePathToFullPath( ActualFilePath, pPathID, pResolvedFileName, resolvedFileNameMaxLen );
							}
							
							// Return the system
							*pResolvedVideoSystem = GetSystemForIndex( sysIdx );
							
							g_pFullFileSystem->FindClose( searchHandle );
							
							return SetResult( VideoResult::SUCCESS );
						}
					}
				}
			}
		}
	
		// not usable.. keep searching		
		pMatchingFile = g_pFullFileSystem->FindNext( searchHandle );
	}	
	
	// we didn't find anything we could use
	g_pFullFileSystem->FindClose( searchHandle );
		
	return SetResult( VideoResult::VIDEO_FILE_NOT_FOUND );
}


VideoSystem_t CValveVideoServices::LocateSystemAndFeaturesForFileName( const char *pFileName,  VideoSystemFeature_t *pFeatures, VideoSystemFeature_t requiredFeatures )
{
	if ( pFeatures != nullptr)
	{
		*pFeatures = VideoSystemFeature::NO_FEATURES;
	}
	
	AssertExitV( IS_NOT_EMPTY( pFileName ), VideoSystem::NONE );
	
	if ( m_ExtInfo.Count() < 1 )
	{
		return VideoSystem::NONE;
	}
	
	// extract the file extension
	
	char fileExt[MAX_PATH];
	
	const char *pExt = GetFileExtension( pFileName );
	if ( pExt == nullptr )
	{
		return VideoSystem::NONE;
	}

	// lowercase it so we can compare	
	V_strncpy( fileExt, pExt, sizeof(fileExt) );
	V_strlower( fileExt );
	
	for ( int i = 0; i < m_ExtInfo.Count(); i++ )
	{
		if ( V_stricmp( fileExt, m_ExtInfo[i].m_FileExtension ) == STRINGS_MATCH )
		{
			// must it have certain feature support?
			if ( requiredFeatures != VideoSystemFeature::NO_FEATURES )
			{
				if ( !BITFLAGS_SET( m_ExtInfo[i].m_VideoFeatures, requiredFeatures ) )
				{
					continue;
				}
			}
		
			if ( pFeatures != nullptr)
			{
				*pFeatures = m_ExtInfo[i].m_VideoFeatures;
			}
			return m_ExtInfo[i].m_VideoSubSystem;
		}
	}
	
	return VideoSystem::NONE;
}


bool CValveVideoServices::IsMatchAnyExtension( const char *pFileName )
{
	if ( IS_EMPTY_STR( pFileName ) )
	{
		return false;
	}
		
	const char* pExt = GetFileExtension( pFileName );
	if ( pExt == nullptr )
	{
		return false;
	}
	
	return ( V_stricmp( pExt, FILE_EXTENSION_ANY_MATCHING_VIDEO ) == STRINGS_MATCH );
}


const char *CValveVideoServices::GetFileExtension( const char *pFileName )
{
	if ( pFileName == nullptr )
	{
		return nullptr;
	}
	
	const char *pExt = V_GetFileExtension( pFileName );
	
	if ( pExt == nullptr )
	{
		return nullptr;
	}

	if ( pExt != pFileName && *( pExt - 1 ) == '.' )
	{
		pExt--;
	}

	return pExt;
}



// ===========================================================================	
// CVideoCommonServices - services used by any/multiple videoSubsystems
//   Functions are put here to avoid duplication and ensure they stay
//   consistant across all installed subsystems
// ===========================================================================	


#ifdef WIN32		
	typedef SHORT (WINAPI *GetAsyncKeyStateFn_t)( int vKey );

	static HINSTANCE s_UserDLLhInst = nullptr;
	GetAsyncKeyStateFn_t s_pfnGetAsyncKeyState = nullptr;
#endif

CVideoCommonServices::CVideoCommonServices()
{
	ResetInputHandlerState();
}


CVideoCommonServices::~CVideoCommonServices()
{
	if ( m_bInputHandlerInitialized )
	{
		TerminateFullScreenPlaybackInputHandler();
	}

}


void CVideoCommonServices::ResetInputHandlerState()
{
	m_bInputHandlerInitialized = false;
	
	m_bScanAll	= false;
	m_bScanEsc	= false;
	m_bScanReturn	= false;
	m_bScanSpace	= false;
	m_bPauseEnabled	= false;
	m_bAbortEnabled	= false;
	m_bEscLast	= false;
	m_bReturnLast	= false;
	m_bSpaceLast	= false;
	m_bForceMinPlayTime	= false;
		
	m_bWindowed = false;
	
	m_playbackFlags = VideoPlaybackFlags::NO_PLAYBACK_OPTIONS;
	m_forcedMinTime = 0.0f;
	
	m_StartTime = 0;
	
#ifdef WIN32
	s_UserDLLhInst = nullptr;
	s_pfnGetAsyncKeyState = nullptr;
#endif	

}

// ===========================================================================	
// Calculate the proper dimensions to play a video in full screen mode
//  uses the playback flags to supply rules for streaching, scaling	and 
//  centering the video
// ===========================================================================	
bool CVideoCommonServices::CalculateVideoDimensions( int videoWidth, int videoHeight, int displayWidth, int displayHeight, VideoPlaybackFlags_t playbackFlags, 
													int *pOutputWidth, int *pOutputHeight, int *pXOffset, int *pYOffset )
{
	AssertExitF( pOutputWidth != nullptr && pOutputHeight != nullptr && pXOffset != nullptr && pYOffset != nullptr );
	AssertExitF( videoWidth >= 16 && videoHeight >= 16 && displayWidth > 64 && displayHeight > 64 );

	// extract relevant options
	bool bFillWindow	= BITFLAGS_SET( playbackFlags, VideoPlaybackFlags::FILL_WINDOW );
	bool bLockAspect	= BITFLAGS_SET( playbackFlags, VideoPlaybackFlags::LOCK_ASPECT_RATIO );
	bool bIntegralScale = BITFLAGS_SET( playbackFlags, VideoPlaybackFlags::INTEGRAL_SCALE );
	bool bCenterVideo	= BITFLAGS_SET( playbackFlags, VideoPlaybackFlags::CENTER_VIDEO_IN_WINDOW );
	
	int curWidth = videoWidth;
	int curHeight = videoHeight;
	
	// Try and just play it actual size?
	if ( !bFillWindow )	
	{
		// is the window the same size or larger?
		if ( curWidth <= displayWidth && curHeight <= displayHeight )
		{
			goto finish;
		}
		else  // we need to shrink the video output
		{
			// if we aren't locking the aspect ratio, just shrink each axis until it fits
			if ( !bLockAspect )
			{
				while ( curWidth > displayWidth)
				{
					curWidth = ( bIntegralScale ) ? curWidth >> 1 : displayWidth;
				}
				while ( curHeight > displayHeight )
				{
					curHeight = ( bIntegralScale ) ? curHeight >> 1 : displayHeight;
				}
				goto finish;
			}
			else   // we are locking the aspect ratio, and need to shrink the video
			{
				// integral scale only....
				if ( bIntegralScale )	
				{
					while ( curWidth > displayWidth || curHeight > displayHeight)
					{
						curWidth >>= 1;
						curHeight >>= 1;
					}
					goto finish;
				}
				else	// can scale variably..
				{
					float Xfactor = ( displayWidth / curWidth );
					float Yfactor = ( displayHeight / curHeight );
					float scale = MIN( Xfactor, Yfactor );
					
					curWidth = (int)  ( curWidth * scale + 0.35f );
					curHeight = (int) ( curHeight * scale + 0.35f );
					clamp( curWidth, 0, displayWidth );
					clamp( curHeight, 0, displayHeight );
					goto finish;
				}
			
			}
		}
	}
	
	// ok.. we are wanting to fill the window....
	if ( bFillWindow )
	{
		// are we locking the aspect ratio?
		if ( bLockAspect )
		{
			// are we only allowed to scale integrally?
			if ( bIntegralScale )
			{
				while ( (curWidth << 1) <= displayWidth && (curHeight << 1) <= displayHeight )
				{
					curWidth <<= 1;
					curHeight <<= 1;
				}
				goto finish;
			}
			else
			{
				float Xfactor = ( (float)displayWidth / curWidth );
				float Yfactor = ( (float)displayHeight / curHeight );
				float scale = MIN( Xfactor, Yfactor );
				
				curWidth = (int)  ( curWidth * scale + 0.35f );
				curHeight = (int) ( curHeight * scale + 0.35f );
				clamp( curWidth, 0, displayWidth );
				clamp( curHeight, 0, displayHeight );
				goto finish;
			}
		}
		else // we are not locking the aspect ratio...
		{
			if ( bIntegralScale )
			{
				while ( (curWidth << 1) <= displayWidth  )
				{
					curWidth <<= 1;
				}
				while ( (curHeight << 1) <= displayHeight )
				{
					curHeight <<= 1;
				}
				goto finish;
			}
			else
			{	
				curWidth = displayWidth;
				curHeight = displayHeight;
				goto finish;
			}
		}
	}	


finish:
	AssertExitF( displayWidth >= curWidth && displayHeight >= curHeight );

	if ( bCenterVideo )
	{
		*pXOffset = ( displayWidth - curWidth ) >> 1;
		*pYOffset = ( displayHeight - curHeight ) >> 1;
	}
	else
	{
		*pXOffset = 0;
		*pYOffset = 0;
	}

	*pOutputWidth = curWidth;
	*pOutputHeight = curHeight;

	return true;

}


float CVideoCommonServices::GetSystemVolume()
{
	ConVarRef volumeConVar( "volume" );
	float sysVolume = volumeConVar.IsValid() ? volumeConVar.GetFloat() : 1.0f;
	clamp( sysVolume, 0.0f, 1.0f);

	return sysVolume;
}

									  

// ===========================================================================	
// Sets up the state machine to receive messages and poll the keyboard
//   while a full-screen video is playing
// ===========================================================================	
VideoResult_t CVideoCommonServices::InitFullScreenPlaybackInputHandler( VideoPlaybackFlags_t playbackFlags, float forcedMinTime, bool windowed )
{
	// already initialized?
	if ( m_bInputHandlerInitialized )
	{
		WarningAssert( "called twice" );
		return VideoResult::OPERATION_ALREADY_PERFORMED;
	}

#ifdef WIN32
	// We need to be able to poll the state of the input device, but we're not completely setup yet, so this spoofs the ability
	HINSTANCE m_UserDLLhInst = LoadLibrary( "user32.dll" );
	if ( m_UserDLLhInst == NULL )
	{
		return VideoResult::SYSTEM_ERROR_OCCURED;
	}

	s_pfnGetAsyncKeyState = (GetAsyncKeyStateFn_t) GetProcAddress( m_UserDLLhInst, "GetAsyncKeyState" );
	if ( s_pfnGetAsyncKeyState == NULL )	
	{
		FreeLibrary( m_UserDLLhInst );
		return VideoResult::SYSTEM_ERROR_OCCURED;
	}
	
#endif

	// save off playback options	
	m_playbackFlags = playbackFlags;
	m_forcedMinTime = forcedMinTime;
	m_bWindowed = windowed;

	// process the pause and abort options
	m_bScanAll =  ANY_BITFLAGS_SET( playbackFlags, VideoPlaybackFlags::PAUSE_ON_ANY_KEY | VideoPlaybackFlags::ABORT_ON_ANY_KEY );
	
	m_bScanEsc	 = m_bScanAll || ANY_BITFLAGS_SET( playbackFlags, VideoPlaybackFlags::PAUSE_ON_ESC | VideoPlaybackFlags::ABORT_ON_ESC ); 
	m_bScanReturn = m_bScanAll || ANY_BITFLAGS_SET( playbackFlags, VideoPlaybackFlags::PAUSE_ON_RETURN | VideoPlaybackFlags::ABORT_ON_RETURN );
	m_bScanSpace  = m_bScanAll || ANY_BITFLAGS_SET( playbackFlags, VideoPlaybackFlags::PAUSE_ON_SPACE | VideoPlaybackFlags::ABORT_ON_SPACE );

	m_bPauseEnabled = ANY_BITFLAGS_SET( playbackFlags, VideoPlaybackFlags::PAUSE_ON_ESC | VideoPlaybackFlags::PAUSE_ON_RETURN | VideoPlaybackFlags::PAUSE_ON_SPACE | VideoPlaybackFlags::PAUSE_ON_ANY_KEY );
	m_bAbortEnabled = ANY_BITFLAGS_SET( playbackFlags, VideoPlaybackFlags::ABORT_ON_ESC | VideoPlaybackFlags::ABORT_ON_RETURN | VideoPlaybackFlags::ABORT_ON_SPACE | VideoPlaybackFlags::ABORT_ON_ANY_KEY );

	// Setup the scan options
	m_bEscLast	 = false;
	m_bReturnLast = false;
	m_bSpaceLast  = false;

	// Other Movie playback	state init
	m_bForceMinPlayTime = BITFLAGS_SET( playbackFlags, VideoPlaybackFlags::FORCE_MIN_PLAY_TIME ) && ( forcedMinTime > 0.0f );

	// Note the start time
	m_StartTime = Plat_FloatTime();

	// and we're on
	m_bInputHandlerInitialized = true;
	
	return VideoResult::SUCCESS;
}


// ===========================================================================	
//  Pumps the message loops and checks for a supported event
//  returns true if there is an event to check
// ===========================================================================	
bool CVideoCommonServices::ProcessFullScreenInput( bool &bAbortEvent, bool &bPauseEvent, bool &bQuitEvent )
{

	bAbortEvent = false;
	bPauseEvent = false;
	bQuitEvent = false;

	if ( !m_bInputHandlerInitialized )
	{
		WarningAssert( "Not Initialized to call" );
		return false;
	}


	// Pump OS Messages
#if defined( WIN32 )
	MSG msg;
	while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
	{
		// did we get a quit message?
		if ( msg.message == WM_QUIT )
		{
			::PostQuitMessage( msg.wParam );
			return true;			
		}
	
		// todo - look for alt-tab events, etc?
	
		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}
	// Escape, return, or space stops or pauses the playback
	bool bEscPressed	= ( m_bScanEsc )    ? ( s_pfnGetAsyncKeyState( VK_ESCAPE ) & 0x8000 ) != 0 : false;
	bool bReturnPressed	= ( m_bScanReturn ) ? ( s_pfnGetAsyncKeyState( VK_RETURN ) & 0x8000 ) != 0 : false;
	bool bSpacePressed	= ( m_bScanSpace )  ? ( s_pfnGetAsyncKeyState( VK_SPACE ) & 0x8000 ) != 0  : false;
#elif defined(OSX)
	g_pLauncherMgr->PumpWindowsMessageLoop();
	// Escape, return, or space stops or pauses the playback
	bool bEscPressed    = ( m_bScanEsc )    ? CGEventSourceKeyState( kCGEventSourceStateCombinedSessionState, kVK_Escape ) : false;
	bool bReturnPressed = ( m_bScanReturn ) ? CGEventSourceKeyState( kCGEventSourceStateCombinedSessionState, kVK_Return ) : false;
	bool bSpacePressed  = ( m_bScanSpace )  ? CGEventSourceKeyState( kCGEventSourceStateCombinedSessionState, kVK_Space )  : false;
#elif defined(LINUX)
	g_pLauncherMgr->PumpWindowsMessageLoop();

	// Escape, return, or space stops or pauses the playback
	bool bEscPressed	= false;
	bool bReturnPressed	= false;
	bool bSpacePressed	= false;

	g_pLauncherMgr->PeekAndRemoveKeyboardEvents( &bEscPressed, &bReturnPressed, &bSpacePressed );
#endif

	// Manual debounce of the keys, only interested in unpressed->pressed transitions
	bool bEscEvent = ( bEscPressed != m_bEscLast ) && bEscPressed;
	bool bReturnEvent = ( bReturnPressed != m_bReturnLast ) &&  bReturnPressed;
	bool bSpaceEvent = ( bSpacePressed != m_bSpaceLast ) && bSpacePressed;
	bool bAnyKeyEvent = bEscEvent || bReturnEvent || bSpaceEvent;

	m_bEscLast = bEscPressed;
	m_bReturnLast = bReturnPressed;
	m_bSpaceLast = bSpacePressed;

	// Are we forcing a minimum playback time?
	// if so, no Abort or Pause events until the necessary time has elasped
	if ( m_bForceMinPlayTime )
	{
		double elapsedTime = Plat_FloatTime() - m_StartTime;
		if ( (float) elapsedTime > m_forcedMinTime )
		{
			m_bForceMinPlayTime = false;		// turn off forced minimum
		}
	}

	// any key events to check? ( provided minimum enforced playback has occurred )
	if ( m_bForceMinPlayTime == false && bAnyKeyEvent )
	{
		// check for aborting the movie
		if ( m_bAbortEnabled )
		{
			bAbortEvent = ( bAnyKeyEvent && BITFLAGS_SET( m_playbackFlags, VideoPlaybackFlags::ABORT_ON_ANY_KEY ) ) ||
						  ( bEscEvent && BITFLAGS_SET( m_playbackFlags, VideoPlaybackFlags::ABORT_ON_ESC ) ) ||
				          ( bReturnEvent && BITFLAGS_SET( m_playbackFlags, VideoPlaybackFlags::ABORT_ON_RETURN ) ) ||
						  ( bSpaceEvent && BITFLAGS_SET( m_playbackFlags, VideoPlaybackFlags::ABORT_ON_SPACE ) );
			
		}
			
		// check for pausing the movie?
		if ( m_bPauseEnabled )
		{
			bPauseEvent = ( bAnyKeyEvent && BITFLAGS_SET( m_playbackFlags, VideoPlaybackFlags::PAUSE_ON_ANY_KEY ) ) ||
						  ( bEscEvent && BITFLAGS_SET( m_playbackFlags, VideoPlaybackFlags::PAUSE_ON_ESC ) ) ||
				          ( bReturnEvent && BITFLAGS_SET( m_playbackFlags, VideoPlaybackFlags::PAUSE_ON_RETURN ) ) ||
						  ( bSpaceEvent && BITFLAGS_SET( m_playbackFlags, VideoPlaybackFlags::PAUSE_ON_SPACE ) );
		}
	}				

	// notify if any events triggered
	return ( bAbortEvent || bPauseEvent );
}




VideoResult_t CVideoCommonServices::TerminateFullScreenPlaybackInputHandler()
{

	if ( !m_bInputHandlerInitialized )
	{
		WarningAssert( "Not Initialized to call" );
		return VideoResult::OPERATION_OUT_OF_SEQUENCE;
	}

#if defined ( WIN32 )
	FreeLibrary( s_UserDLLhInst );		// and free the dll we needed
#endif

	ResetInputHandlerState();
	
	return VideoResult::SUCCESS;
	
}
