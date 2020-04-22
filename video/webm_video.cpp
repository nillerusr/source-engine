//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "webm_video.h"
#include "webm_recorder.h"


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


#include "tier0/memdbgon.h"


// ===========================================================================
// Singleton to expose WebM video subsystem
// ===========================================================================
static CWebMVideoSubSystem g_WebMSystem;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CWebMVideoSubSystem, IVideoSubSystem, VIDEO_SUBSYSTEM_INTERFACE_VERSION, g_WebMSystem );


// ===========================================================================
// List of file extensions and features supported by this subsystem
// ===========================================================================
VideoFileExtensionInfo_t s_WebMExtensions[] = 
{
	{ ".webm", VideoSystem::WEBM,  VideoSystemFeature::FULL_ENCODE },
};

const int s_WebMExtensionCount = ARRAYSIZE( s_WebMExtensions );

const VideoSystemFeature_t	CWebMVideoSubSystem::DEFAULT_FEATURE_SET = VideoSystemFeature::FULL_ENCODE;


// ===========================================================================
// CWebMVideoSubSystem class
// ===========================================================================
CWebMVideoSubSystem::CWebMVideoSubSystem() :
	m_bWebMInitialized( false ),
	m_LastResult( VideoResult::SUCCESS ),
	m_CurrentStatus( VideoSystemStatus::NOT_INITIALIZED ),
	m_AvailableFeatures( CWebMVideoSubSystem::DEFAULT_FEATURE_SET ),
	m_pCommonServices( nullptr )
{

}


CWebMVideoSubSystem::~CWebMVideoSubSystem()
{
	ShutdownWebM();		// Super redundant safety check
}


// ===========================================================================
// IAppSystem methods
// ===========================================================================
bool CWebMVideoSubSystem::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
	{
		return false;
	}

	if ( g_pFullFileSystem == nullptr || materials == nullptr ) 
	{
		Msg( "WebM video subsystem failed to connect to missing a required system\n" );
		return false;
	}
	return true;
}


void CWebMVideoSubSystem::Disconnect()
{
	BaseClass::Disconnect();
}


void* CWebMVideoSubSystem::QueryInterface( const char *pInterfaceName )
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


InitReturnVal_t CWebMVideoSubSystem::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
	{
		return nRetVal;
	}

	return INIT_OK;

}


void CWebMVideoSubSystem::Shutdown()
{
	// Make sure we shut down WebM
	ShutdownWebM();
	
	BaseClass::Shutdown();
}


// ===========================================================================
// IVideoSubSystem identification methods  
// ===========================================================================
VideoSystem_t CWebMVideoSubSystem::GetSystemID()
{
	return VideoSystem::WEBM;
}


VideoSystemStatus_t CWebMVideoSubSystem::GetSystemStatus()
{
	return m_CurrentStatus;
}


VideoSystemFeature_t CWebMVideoSubSystem::GetSupportedFeatures()
{
	return m_AvailableFeatures;
}


const char* CWebMVideoSubSystem::GetVideoSystemName()
{
	return "WebM";
}


// ===========================================================================
// IVideoSubSystem setup and shutdown services
// ===========================================================================
bool CWebMVideoSubSystem::InitializeVideoSystem( IVideoCommonServices *pCommonServices )
{
	m_AvailableFeatures = DEFAULT_FEATURE_SET;			// Put here because of issue with static const int, binary OR and DEBUG builds
	
	AssertPtr( pCommonServices );
	m_pCommonServices = pCommonServices;
	
	return ( m_bWebMInitialized ) ? true : SetupWebM();
}


bool CWebMVideoSubSystem::ShutdownVideoSystem()
{
	return (  m_bWebMInitialized ) ? ShutdownWebM() : true;
}


VideoResult_t CWebMVideoSubSystem::VideoSoundDeviceCMD( VideoSoundDeviceOperation_t operation, void *pDevice, void *pData )
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
int CWebMVideoSubSystem::GetSupportedFileExtensionCount()
{
	return s_WebMExtensionCount;
}

 
const char* CWebMVideoSubSystem::GetSupportedFileExtension( int num )
{
	return ( num < 0 || num >= s_WebMExtensionCount ) ? nullptr : s_WebMExtensions[num].m_FileExtension;
}

 
VideoSystemFeature_t CWebMVideoSubSystem::GetSupportedFileExtensionFeatures( int num )
{
	 return ( num < 0 || num >= s_WebMExtensionCount ) ? VideoSystemFeature::NO_FEATURES : s_WebMExtensions[num].m_VideoFeatures;
}


// ===========================================================================
// IVideoSubSystem Video Playback and Recording Services
// ===========================================================================
VideoResult_t CWebMVideoSubSystem::PlayVideoFileFullScreen( const char *filename, void *mainWindow, int windowWidth, int windowHeight, int desktopWidth, int desktopHeight, bool windowed, float forcedMinTime, VideoPlaybackFlags_t playbackFlags )
{
    return SetResult( VideoResult::FEATURE_NOT_AVAILABLE );
}


// ===========================================================================
// IVideoSubSystem Video Material Services
//   note that the filename is absolute and has already resolved any paths
// ===========================================================================
IVideoMaterial* CWebMVideoSubSystem::CreateVideoMaterial( const char *pMaterialName, const char *pVideoFileName, VideoPlaybackFlags_t flags )
{
    SetResult( VideoResult::FEATURE_NOT_AVAILABLE );
    return NULL;
}


VideoResult_t CWebMVideoSubSystem::DestroyVideoMaterial( IVideoMaterial *pVideoMaterial )
{
	return SetResult (VideoResult::FEATURE_NOT_AVAILABLE );

}


// ===========================================================================
// IVideoSubSystem Video Recorder Services
// ===========================================================================
IVideoRecorder* CWebMVideoSubSystem::CreateVideoRecorder()
{
	SetResult( VideoResult::SYSTEM_NOT_AVAILABLE );
	AssertExitN( m_CurrentStatus == VideoSystemStatus::OK );

	CWebMVideoRecorder *pVideoRecorder = new CWebMVideoRecorder();
	
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


VideoResult_t CWebMVideoSubSystem::DestroyVideoRecorder( IVideoRecorder *pRecorder )
{
	AssertExitV( m_CurrentStatus == VideoSystemStatus::OK, SetResult( VideoResult::SYSTEM_NOT_AVAILABLE ) );
	AssertPtrExitV( pRecorder, SetResult( VideoResult::BAD_INPUT_PARAMETERS ) );

	if ( m_RecorderList.Find( pRecorder ) != -1 )
	{
		CWebMVideoRecorder *pVideoRecorder = (CWebMVideoRecorder*) pRecorder;
		delete pVideoRecorder;
		
		m_RecorderList.FindAndFastRemove( pRecorder );
		
		return SetResult( VideoResult::SUCCESS );
	}
	
	return SetResult( VideoResult::RECORDER_NOT_FOUND );
}

VideoResult_t CWebMVideoSubSystem::CheckCodecAvailability( VideoEncodeCodec_t codec )
{
	AssertExitV( m_CurrentStatus == VideoSystemStatus::OK, SetResult( VideoResult::SYSTEM_NOT_AVAILABLE ) );
	AssertExitV( codec >= VideoEncodeCodec::DEFAULT_CODEC && codec < VideoEncodeCodec::CODEC_COUNT, SetResult( VideoResult::BAD_INPUT_PARAMETERS ) );
	
	return SetResult( VideoResult::FEATURE_NOT_AVAILABLE );
}


// ===========================================================================
// Status support
// ===========================================================================
VideoResult_t CWebMVideoSubSystem::GetLastResult()
{
	return m_LastResult;
}


VideoResult_t CWebMVideoSubSystem::SetResult( VideoResult_t status )
{
	m_LastResult = status;
	return status;
}


// ===========================================================================
// WebM Initialization & Shutdown
// ===========================================================================
bool CWebMVideoSubSystem::SetupWebM()
{
	SetResult( VideoResult::INITIALIZATION_ERROR_OCCURED);
	AssertExitF( m_bWebMInitialized == false );

	// This is set early to indicate we have already been through here, even if we error out for some reason
	m_bWebMInitialized = true;
	m_CurrentStatus = VideoSystemStatus::OK;
	m_AvailableFeatures = DEFAULT_FEATURE_SET;
	// $$INIT CODE HERE$$


	// Note that we are now open for business....	
	m_bWebMInitialized = true;
	SetResult( VideoResult::SUCCESS );
    
	return true;
}


bool CWebMVideoSubSystem::ShutdownWebM()
{
	if ( m_bWebMInitialized && m_CurrentStatus == VideoSystemStatus::OK )
	{

	}
				
	m_bWebMInitialized = false;
	m_CurrentStatus = VideoSystemStatus::NOT_INITIALIZED;
	m_AvailableFeatures = VideoSystemFeature::NO_FEATURES;
	SetResult( VideoResult::SUCCESS );

	return true;
}

