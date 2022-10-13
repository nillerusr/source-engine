//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "bink_video.h"
#include "video_macros.h"

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
#include "bink_material.h"

// ===========================================================================
// Singleton to expose Bink video subsystem
// ===========================================================================
static CBinkVideoSubSystem g_BinkSystem;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CBinkVideoSubSystem, IVideoSubSystem, VIDEO_SUBSYSTEM_INTERFACE_VERSION, g_BinkSystem );


// ===========================================================================
// List of file extensions and features supported by this subsystem
// ===========================================================================
VideoFileExtensionInfo_t s_BinkExtensions[] = 
{
	{ ".bik", VideoSystem::BINK,  VideoSystemFeature::PLAY_VIDEO_FILE_IN_MATERIAL },
};

const int s_BinkExtensionCount = ARRAYSIZE( s_BinkExtensions );
const VideoSystemFeature_t	CBinkVideoSubSystem::DEFAULT_FEATURE_SET = VideoSystemFeature::PLAY_VIDEO_FILE_IN_MATERIAL;

// ===========================================================================
// CBinkVideoSubSystem class
// ===========================================================================
CBinkVideoSubSystem::CBinkVideoSubSystem() :
	m_bBinkInitialized( false ),
	m_LastResult( VideoResult::SUCCESS ),
	m_CurrentStatus( VideoSystemStatus::NOT_INITIALIZED ),
	m_AvailableFeatures( CBinkVideoSubSystem::DEFAULT_FEATURE_SET ),
	m_pCommonServices( nullptr )
{

}

CBinkVideoSubSystem::~CBinkVideoSubSystem()
{
	ShutdownBink();		// Super redundant safety check
}

// ===========================================================================
// IAppSystem methods
// ===========================================================================
bool CBinkVideoSubSystem::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
	{
		return false;
	}

	if ( g_pFullFileSystem == nullptr || materials == nullptr ) 
	{
		Msg( "Bink video subsystem failed to connect to missing a required system\n" );
		return false;
	}
	return true;
}

void CBinkVideoSubSystem::Disconnect()
{
	BaseClass::Disconnect();
}

void* CBinkVideoSubSystem::QueryInterface( const char *pInterfaceName )
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


InitReturnVal_t CBinkVideoSubSystem::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
	{
		return nRetVal;
	}

	return INIT_OK;

}

void CBinkVideoSubSystem::Shutdown()
{
	// Make sure we shut down Bink
	ShutdownBink();

	BaseClass::Shutdown();
}


// ===========================================================================
// IVideoSubSystem identification methods  
// ===========================================================================
VideoSystem_t CBinkVideoSubSystem::GetSystemID()
{
	return VideoSystem::BINK;
}


VideoSystemStatus_t CBinkVideoSubSystem::GetSystemStatus()
{
	return m_CurrentStatus;
}


VideoSystemFeature_t CBinkVideoSubSystem::GetSupportedFeatures()
{
	return m_AvailableFeatures;
}


const char* CBinkVideoSubSystem::GetVideoSystemName()
{
	return "BINK";
}


// ===========================================================================
// IVideoSubSystem setup and shutdown services
// ===========================================================================
bool CBinkVideoSubSystem::InitializeVideoSystem( IVideoCommonServices *pCommonServices )
{
	m_AvailableFeatures = DEFAULT_FEATURE_SET;			// Put here because of issue with static const int, binary OR and DEBUG builds

	AssertPtr( pCommonServices );
	m_pCommonServices = pCommonServices;

	return ( m_bBinkInitialized ) ? true : SetupBink();
}


bool CBinkVideoSubSystem::ShutdownVideoSystem()
{
	return (  m_bBinkInitialized ) ? ShutdownBink() : true;
}


VideoResult_t CBinkVideoSubSystem::VideoSoundDeviceCMD( VideoSoundDeviceOperation_t operation, void *pDevice, void *pData )
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
int CBinkVideoSubSystem::GetSupportedFileExtensionCount()
{
	return s_BinkExtensionCount;
}

 
const char* CBinkVideoSubSystem::GetSupportedFileExtension( int num )
{
	return ( num < 0 || num >= s_BinkExtensionCount ) ? nullptr : s_BinkExtensions[num].m_FileExtension;
}

 
VideoSystemFeature_t CBinkVideoSubSystem::GetSupportedFileExtensionFeatures( int num )
{
	 return ( num < 0 || num >= s_BinkExtensionCount ) ? VideoSystemFeature::NO_FEATURES : s_BinkExtensions[num].m_VideoFeatures;
}


// ===========================================================================
// IVideoSubSystem Video Playback and Recording Services
// ===========================================================================
VideoResult_t CBinkVideoSubSystem::PlayVideoFileFullScreen( const char *filename, void *mainWindow, int windowWidth, int windowHeight, int desktopWidth, int desktopHeight, bool windowed, float forcedMinTime, VideoPlaybackFlags_t playbackFlags )
{
    return SetResult( VideoResult::FEATURE_NOT_AVAILABLE );
}


// ===========================================================================
// IVideoSubSystem Video Material Services
//   note that the filename is absolute and has already resolved any paths
// ===========================================================================
IVideoMaterial* CBinkVideoSubSystem::CreateVideoMaterial( const char *pMaterialName, const char *pVideoFileName, VideoPlaybackFlags_t flags )
{
	SetResult( VideoResult::BAD_INPUT_PARAMETERS );
	AssertExitN( m_CurrentStatus == VideoSystemStatus::OK && IS_NOT_EMPTY( pMaterialName ) || IS_NOT_EMPTY( pVideoFileName ) );

	CBinkMaterial *pVideoMaterial = new CBinkMaterial();
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


VideoResult_t CBinkVideoSubSystem::DestroyVideoMaterial( IVideoMaterial *pVideoMaterial )
{
	AssertExitV( m_CurrentStatus == VideoSystemStatus::OK, SetResult( VideoResult::SYSTEM_NOT_AVAILABLE ) );
	AssertPtrExitV( pVideoMaterial, SetResult( VideoResult::BAD_INPUT_PARAMETERS ) );

	if ( m_MaterialList.Find( pVideoMaterial ) != -1 )
	{
		CBinkMaterial *pObject = (CBinkMaterial*) pVideoMaterial;
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
IVideoRecorder* CBinkVideoSubSystem::CreateVideoRecorder()
{
	SetResult( VideoResult::FEATURE_NOT_AVAILABLE );
	return nullptr;
}


VideoResult_t CBinkVideoSubSystem::DestroyVideoRecorder( IVideoRecorder *pRecorder )
{
	return SetResult( VideoResult::FEATURE_NOT_AVAILABLE );
}

VideoResult_t CBinkVideoSubSystem::CheckCodecAvailability( VideoEncodeCodec_t codec )
{
	AssertExitV( m_CurrentStatus == VideoSystemStatus::OK, SetResult( VideoResult::SYSTEM_NOT_AVAILABLE ) );
	AssertExitV( codec >= VideoEncodeCodec::DEFAULT_CODEC && codec < VideoEncodeCodec::CODEC_COUNT, SetResult( VideoResult::BAD_INPUT_PARAMETERS ) );

	return SetResult( VideoResult::FEATURE_NOT_AVAILABLE );
}


// ===========================================================================
// Status support
// ===========================================================================
VideoResult_t CBinkVideoSubSystem::GetLastResult()
{
	return m_LastResult;
}


VideoResult_t CBinkVideoSubSystem::SetResult( VideoResult_t status )
{
	m_LastResult = status;
	return status;
}


// ===========================================================================
// Bink Initialization & Shutdown
// ===========================================================================
bool CBinkVideoSubSystem::SetupBink()
{
	SetResult( VideoResult::INITIALIZATION_ERROR_OCCURED);
	AssertExitF( m_bBinkInitialized == false );

	// This is set early to indicate we have already been through here, even if we error out for some reason
	m_bBinkInitialized = true;
	m_CurrentStatus = VideoSystemStatus::OK;
	m_AvailableFeatures = DEFAULT_FEATURE_SET;
	// $$INIT CODE HERE$$


	// Note that we are now open for business....	
	m_bBinkInitialized = true;
	SetResult( VideoResult::SUCCESS );

	return true;
}


bool CBinkVideoSubSystem::ShutdownBink()
{
	if ( m_bBinkInitialized && m_CurrentStatus == VideoSystemStatus::OK )
	{

	}

	m_bBinkInitialized = false;
	m_CurrentStatus = VideoSystemStatus::NOT_INITIALIZED;
	m_AvailableFeatures = VideoSystemFeature::NO_FEATURES;
	SetResult( VideoResult::SUCCESS );

	return true;
}

