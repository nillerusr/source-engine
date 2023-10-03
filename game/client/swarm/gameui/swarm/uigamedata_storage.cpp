//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "basemodpanel.h"
#include "basemodframe.h"
#include "UIGameData.h"
#include "EngineInterface.h"
#include "VGenericConfirmation.h"
#include "vgui/ILocalize.h"
#include "matchmaking/imatchframework.h"
#include "filesystem.h"
#include "fmtstr.h"
#ifndef _X360
#include "steam/steam_api.h"
#endif
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace BaseModUI;
using namespace vgui;

#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS 0
#endif

#ifndef ERROR_IO_INCOMPLETE
#define ERROR_IO_INCOMPLETE 996L
#endif

//
// Storage device selection
//

//-----------------------------------------------------------------------------
// Purpose: A storage device has been connected, update our settings and anything else
//-----------------------------------------------------------------------------
namespace BaseModUI {

class CAsyncCtxUIOnDeviceAttached : public CUIGameData::CAsyncJobContext
{
public:
	explicit CAsyncCtxUIOnDeviceAttached( int iController );
	~CAsyncCtxUIOnDeviceAttached();
	virtual void ExecuteAsync();
	virtual void Completed();
	uint GetContainerOpenResult( void ) { return m_ContainerOpenResult; }

	int GetController() const { return m_iController; }

private:
	uint m_ContainerOpenResult;
	int m_iController;
};

CAsyncCtxUIOnDeviceAttached::CAsyncCtxUIOnDeviceAttached( int iController ) :
	CUIGameData::CAsyncJobContext( 3.0f ),	// Storage device info for at least 3 seconds
	m_ContainerOpenResult( ERROR_SUCCESS ),
	m_iController( iController )
{
	//CUIGameData::Get()->ShowMessageDialog( MD_CHECKING_STORAGE_DEVICE );
}

CAsyncCtxUIOnDeviceAttached::~CAsyncCtxUIOnDeviceAttached()
{
	//CUIGameData::Get()->CloseMessageDialog( 0 );
}

void CAsyncCtxUIOnDeviceAttached::ExecuteAsync()
{
	// Asynchronously do the tasks that don't interact with the command buffer
	g_pFullFileSystem->DiscoverDLC( GetController() );

	// Open user settings and save game container here
	m_ContainerOpenResult = engine->OnStorageDeviceAttached( GetController() );
	if ( m_ContainerOpenResult != ERROR_SUCCESS )
		return;
}

ConVar ui_start_dlc_time_pump( "ui_start_dlc_time_pump", "30" );
ConVar ui_start_dlc_time_loaded( "ui_start_dlc_time_loaded", "150" );
ConVar ui_start_dlc_time_corrupt( "ui_start_dlc_time_corrupt", "300" );

CON_COMMAND_F( ui_pump_dlc_mount_corrupt, "", FCVAR_DEVELOPMENTONLY )
{
	int nStage = -1;
	if ( args.ArgC() > 1 )
	{
		nStage = Q_atoi( args.Arg( 1 ) );
	}
	DevMsg( 2, "ui_pump_dlc_mount_corrupt %d\n", nStage );

	int nCorruptDLCs = g_pFullFileSystem->IsAnyCorruptDLC();
	while ( nStage >= 0 && nStage < nCorruptDLCs )
	{
		static wchar_t wszDlcInfo[ 3 * MAX_PATH ] = {0};
		if ( !g_pFullFileSystem->GetAnyCorruptDLCInfo( nStage, wszDlcInfo, sizeof( wszDlcInfo ) ) )
		{
			++ nStage;
			continue;
		}

		// information text
		if ( wchar_t *wszExplanation = g_pVGuiLocalize->Find( "#L4D360UI_MsgBx_DlcCorruptTxt" ) )
		{
			int wlen = Q_wcslen( wszDlcInfo );
			Q_wcsncpy( wszDlcInfo + wlen, wszExplanation, sizeof( wszDlcInfo ) - 2 * wlen );
		}

		// We've got a corrupt DLC, put it up on the spinner
		CUIGameData::Get()->UpdateWaitPanel( wszDlcInfo, 0.0f );
		engine->ClientCmd( CFmtStr( "echo corruptdlc%d; wait %d; ui_pump_dlc_mount_corrupt %d;",
			nStage + 1, ui_start_dlc_time_corrupt.GetInt(), nStage + 1 ) );
		return;
	}

	// end of dlc mounting phases
	CUIGameData::Get()->OnCompletedAsyncDeviceAttached( NULL );
}

CON_COMMAND_F( ui_pump_dlc_mount_content, "", FCVAR_DEVELOPMENTONLY )
{
	int nStage = -1;
	if ( args.ArgC() > 1 )
	{
		nStage = Q_atoi( args.Arg( 1 ) );
	}
	DevMsg( 2, "ui_pump_dlc_mount_content %d\n", nStage );

	bool bSearchPathMounted = false;
	int numDlcsContent = g_pFullFileSystem->IsAnyDLCPresent( &bSearchPathMounted );

	while ( nStage >= 0 && nStage < numDlcsContent )
	{
		static wchar_t wszDlcInfo[ 3 * MAX_PATH ] = {0};
		unsigned int ulMask;
		if ( !g_pFullFileSystem->GetAnyDLCInfo( nStage, &ulMask, wszDlcInfo, sizeof( wszDlcInfo ) ) )
		{
			++ nStage;
			continue;
		}

		// information text
		if ( wchar_t *wszExplanation = g_pVGuiLocalize->Find( "#L4D360UI_MsgBx_DlcMountedTxt" ) )
		{
			int wlen = Q_wcslen( wszDlcInfo );
			Q_wcsncpy( wszDlcInfo + wlen, wszExplanation, sizeof( wszDlcInfo ) - 2 * wlen );
		}

		// We've got a corrupt DLC, put it up on the spinner
		CUIGameData::Get()->UpdateWaitPanel( wszDlcInfo, 0.0f );
		engine->ClientCmd( CFmtStr( "echo mounteddlc%d (0x%08X); wait %d; ui_pump_dlc_mount_content %d;",
			nStage + 1, ulMask, ui_start_dlc_time_loaded.GetInt(), nStage + 1 ) );
		return;
	}

	// Done displaying found content, show corrupt
	engine->ClientCmd( "ui_pump_dlc_mount_corrupt 0" );
}

CON_COMMAND_F( ui_pump_dlc_mount_stage, "", FCVAR_DEVELOPMENTONLY )
{
	// execute in order
	int nStage = -1;
	if ( args.ArgC() > 1 )
	{
		nStage = Q_atoi( args.Arg( 1 ) );
	}
	DevMsg( 2, "ui_pump_dlc_mount_stage %d\n", nStage );

	static char const *s_arrClientCmdsDlcMount[] = 
	{
		"net_reloadgameevents",
		"hud_reloadscheme",
		"gameinstructor_reload_lessons",
		"scenefilecache_reload",
		"cc_reload",
		"rr_reloadresponsesystems",
		"cl_soundemitter_reload",
		"sv_soundemitter_reload",
	};

	if ( nStage >= 0 && nStage < ARRAYSIZE( s_arrClientCmdsDlcMount ) )
	{
		// execute in phases, each command deferred occurs on main thread as required
		// adding a wait <frames> to let spinner clock a little
		// no way to solve any one phase that blocks for too long...this is good enough
		engine->ClientCmd( CFmtStr( "wait %d; %s; ui_pump_dlc_mount_stage %d;",
			ui_start_dlc_time_pump.GetInt(),
			s_arrClientCmdsDlcMount[ nStage ],
			nStage + 1 ) );
		return;
	}
	
	// Done mounting
	engine->ClientCmd( "ui_pump_dlc_mount_content 0" );
}

void CAsyncCtxUIOnDeviceAttached::Completed()
{
	bool bDLCSearchPathMounted = false;
	if ( GetContainerOpenResult() == ERROR_SUCCESS &&
		 g_pFullFileSystem->IsAnyDLCPresent( &bDLCSearchPathMounted ) )
	{
		if ( !bDLCSearchPathMounted )
		{
			// add the DLC search paths if they exist
			// this must be done on the main thread
			// the DLC search path mount will incur a quick synchronous hit due to zip mounting
			g_pFullFileSystem->AddDLCSearchPaths();

			// new DLC data may trump prior data, so need to signal isolated system reloads
			engine->ClientCmd( "ui_pump_dlc_mount_stage 0" );
			return;
		}
	}

	// No valid DLC was discovered, check if we discovered some corrupt DLC
	if ( g_pFullFileSystem->IsAnyCorruptDLC() )
	{
		// need to show just corrupt DLC information
		engine->ClientCmd( CFmtStr( "ui_pump_dlc_mount_corrupt %d", 0 ) );
		return;
	}

	// Otherwise we are done attaching storage right now
	CUIGameData::Get()->OnCompletedAsyncDeviceAttached( this );
}

}



//
// CChangeStorageDevice
//
//		Should be used when user wants to change storage device
//

static CChangeStorageDevice *s_pChangeStorageDeviceCallback = NULL;

static void CChangeStorageDevice_Continue()
{
	s_pChangeStorageDeviceCallback->DeviceChangeCompleted( true );
	delete s_pChangeStorageDeviceCallback;
	s_pChangeStorageDeviceCallback = NULL;
}

static void CChangeStorageDevice_SelectAgain()
{
	CUIGameData::Get()->SelectStorageDevice( s_pChangeStorageDeviceCallback );
	s_pChangeStorageDeviceCallback = NULL;
}

CChangeStorageDevice::CChangeStorageDevice( int iCtrlr ) :
	m_iCtrlr( iCtrlr ),
	m_bAllowDeclined( true ),
	m_bForce( true ),
	m_nConfirmationData( 0 )
{
	// Just in case clean up (if dialogs were cancelled due to user sign out or such)
	delete s_pChangeStorageDeviceCallback;
	s_pChangeStorageDeviceCallback = NULL;
}

void CChangeStorageDevice::OnDeviceFail( FailReason_t eReason )
{
	// Depending if the user had storage device by this moment
	// or not we will take different actions:
	DWORD dwDevice = XBX_GetStorageDeviceId( GetCtrlrIndex() );

	switch ( eReason )
	{
	case FAIL_ERROR:
	case FAIL_NOT_SELECTED:
		if ( XBX_DescribeStorageDevice( dwDevice ) )
		{
			// That's fine user has a valid storage device, didn't want to change
			DeviceChangeCompleted( false );
			delete this;
			return;
		}
		// otherwise, proceed with the ui msg
	}

	XBX_SetStorageDeviceId( GetCtrlrIndex(), XBX_STORAGE_DECLINED );

	// We don't want to fire notification because there might be unsaved
	// preferences changes that were done without a storage device
	// no: g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfileStorageAvailable", "iController", GetCtrlrIndex() ) );

	m_bAllowDeclined = false;

	GenericConfirmation* confirmation = 
		static_cast<GenericConfirmation*>( CBaseModPanel::GetSingleton().
		OpenWindow( WT_GENERICCONFIRMATION, CUIGameData::Get()->GetParentWindowForSystemMessageBox(), false ) );

	GenericConfirmation::Data_t data;

	switch ( eReason )
	{
	case FAIL_ERROR:
	case FAIL_NOT_SELECTED:
		data.pWindowTitle = "#L4D360UI_MsgBx_AttractDeviceNoneC";
		data.pMessageText = "#L4D360UI_MsgBx_AttractDeviceNoneTxt";
		break;
	case FAIL_FULL:
		data.pWindowTitle = "#L4D360UI_MsgBx_AttractDeviceFullC";
		data.pMessageText = "#L4D360UI_MsgBx_AttractDeviceFullTxt";
		break;
	case FAIL_CORRUPT:
	default:
		data.pWindowTitle = "#L4D360UI_MsgBx_AttractDeviceCorruptC";
		data.pMessageText = "#L4D360UI_MsgBx_AttractDeviceCorruptTxt";
		break;
	}

	data.bOkButtonEnabled = true;
	data.bCancelButtonEnabled = true;

	s_pChangeStorageDeviceCallback = this;

	data.pfnOkCallback = CChangeStorageDevice_Continue;
	data.pfnCancelCallback = CChangeStorageDevice_SelectAgain;

	// WARNING! WARNING! WARNING!
	// The nature of Generic Confirmation is that it will be silently replaced
	// with another Generic Confirmation if a system event occurs
	// e.g. user unplugs controller, user changes storage device, etc.
	// If that happens neither OK nor CANCEL callbacks WILL NOT BE CALLED
	// The state machine cannot depend on either callback advancing the
	// state because in some situations neither callback can fire and the
	// confirmation dismissed/closed/replaced.
	// State machine must implement OnThink and check if the required
	// confirmation box is still present!
	// This code implements some sort of fallback - it deletes the static
	// confirmation data when a new storage device change is requested.
	// Vitaliy -- 9/26/2009
	//
	m_nConfirmationData = confirmation->SetUsageData(data);
}

void CChangeStorageDevice::OnDeviceSelected()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CChangeStorageDevice::OnDeviceSelected( 0x%08X )\n",
			XBX_GetStorageDeviceId( GetCtrlrIndex() ) );
	}

	CUIGameData::Get()->UpdateWaitPanel( "#L4D360UI_WaitScreen_SignOnSucceded" );
}

void CChangeStorageDevice::AfterDeviceMounted()
{
	DeviceChangeCompleted( true );
	
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfileStorageAvailable", "iController", GetCtrlrIndex() ) );

	delete this;
}

void CChangeStorageDevice::DeviceChangeCompleted( bool bChanged )
{
	if ( bChanged )
	{
		Msg( "CChangeStorageDevice::DeviceChangeCompleted for ctrlr%d device 0x%08X\n",
			GetCtrlrIndex(), XBX_GetStorageDeviceId( GetCtrlrIndex() ) );
	}
	else
	{
		Msg( "CChangeStorageDevice::DeviceChangeCompleted - ctrlr%d is keeping device 0x%08X\n",
			GetCtrlrIndex(), XBX_GetStorageDeviceId( GetCtrlrIndex() ) );
	}
}

//=============================================================================
//
//=============================================================================

class CChangeStorageDeviceChained : public CChangeStorageDevice
{
public:
	typedef CChangeStorageDevice BaseClass;

	explicit CChangeStorageDeviceChained( int iCtrlrs[2] ) :
	BaseClass( iCtrlrs[0] ), m_nChainCtrlr( iCtrlrs[1] ) {}

	virtual void DeviceChangeCompleted( bool bChanged )
	{
		// Defer to the base class
		BaseClass::DeviceChangeCompleted( bChanged );

		// If we have a chain target, then call this off again
		if ( m_nChainCtrlr >= 0 )
		{
			CUIGameData::Get()->SelectStorageDevice( new CChangeStorageDevice( m_nChainCtrlr ) );
		}
	}

private:
	int	m_nChainCtrlr;
};

void OnStorageDevicesChangedSelectNewDevice()
{
#ifdef _X360
	int numChangedCtrlrs = 0;
	int nChangedCtrlrs[2] = { -1, -1 };	// We can only have two users (split-screen)
	for ( DWORD i = 0; i < XBX_GetNumGameUsers(); ++ i )
	{
		int iController = XBX_GetUserId( i );

		// Guests can't choose a storage device!
		if ( XBX_GetUserIsGuest( iController ) )
			continue;

		int nStorageID = XBX_GetStorageDeviceId( iController );
		if ( nStorageID == XBX_INVALID_STORAGE_ID )
		{
			// A controller's device has changed, and we'll need to prompt them to replace it
			nChangedCtrlrs[numChangedCtrlrs] = iController;
			numChangedCtrlrs++;
		}
	}

	// If a controller changed, then start off our device change dialogs
	if ( numChangedCtrlrs )
	{
		CUIGameData::Get()->SelectStorageDevice( new CChangeStorageDeviceChained( nChangedCtrlrs ) );
	}
#endif // _X360
}

void CUIGameData::RunFrame_Storage()
{
	// Check to see if a pending async task has already finished
	if ( m_pAsyncJob && !m_pAsyncJob->m_hThreadHandle )
	{
		m_pAsyncJob->Completed();
		delete m_pAsyncJob;
		m_pAsyncJob = NULL;
	}

	if( m_bWaitingForStorageDeviceHandle )
	{
		//the select device blade just closed, get the selected device
		DWORD ret = xboxsystem->GetOverlappedResult( m_hStorageDeviceChangeHandle, NULL, true );
		if ( ret != ERROR_IO_INCOMPLETE )
		{
			// Done waiting
			xboxsystem->ReleaseAsyncHandle( m_hStorageDeviceChangeHandle );

			m_bWaitingForStorageDeviceHandle = false;

			// If we selected something, validate it
			if ( m_iStorageID != XBX_INVALID_STORAGE_ID )
			{
				OnSetStorageDeviceId( m_iStorageController, m_iStorageID );
			}
			else
			{
				CloseWaitScreen( NULL, "ReportNoDeviceSelected" );
				if ( m_pSelectStorageClient )
				{
					m_pSelectStorageClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_NOT_SELECTED );
					m_pSelectStorageClient = NULL;
				}
			}
		}
	}
}

void StorageDevice_SelectAllNow()
{
#ifdef _X360
	int numChangedCtrlrs = 0;
	int nChangedCtrlrs[2] = { -1, -1 };	// We can only have two users (split-screen)
	for ( DWORD i = 0; i < XBX_GetNumGameUsers(); ++ i )
	{
		int iController = XBX_GetUserId( i );

		// Guests can't choose a storage device!
		if ( XBX_GetUserIsGuest( iController ) )
			continue;

		int nStorageID = XBX_GetStorageDeviceId( iController );
		if ( nStorageID == XBX_INVALID_STORAGE_ID )
		{
			// A controller's device has changed, and we'll need to prompt them to replace it
			nChangedCtrlrs[numChangedCtrlrs] = iController;
			numChangedCtrlrs++;
		}
	}

	// If a controller changed, then start off our device change dialogs
	if ( numChangedCtrlrs )
	{
		CUIGameData::Get()->SelectStorageDevice( new CChangeStorageDeviceChained( nChangedCtrlrs ) );
	}
#endif // _X360
}

//=============================================================================
//This is where we open the XUI pannel to let the user select the current storage device.
bool CUIGameData::SelectStorageDevice( ISelectStorageDeviceClient *pSelectClient )
{
#ifdef _X360 

	if ( !pSelectClient )
		return false;

	int iController = pSelectClient->GetCtrlrIndex();
	bool bAllowDeclined = pSelectClient->AllowDeclined();
	bool bForceDisplay = pSelectClient->ForceSelector();
	bool bCheckCtrlr = !pSelectClient->AllowAnyController();

	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] SelectStorageDevice( ctrlr=%d; %d, %d ), waiting=%d\n",
			iController, bAllowDeclined, bForceDisplay, m_bWaitingForStorageDeviceHandle );
	}

	if ( bCheckCtrlr )
	{
		// Check if the game is in guest mode
		if ( XBX_GetPrimaryUserIsGuest() )
		{
			Warning( "[GAMEUI] SelectStorageDevice for guest!\n" );
			pSelectClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_ERROR );
			return false;	// go away, no storage for guests
		}

		int nSlot = -1;
		for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
		{
			int iCtrlr = XBX_GetUserId( k );
			if ( iCtrlr != iController )
				continue;
			else if ( XBX_GetUserIsGuest( k ) )
			{
				Warning( "[GAMEUI] SelectStorageDevice for guest!\n" );
				pSelectClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_ERROR );
				return false;	// go away, game thinks you are a guest
			}
			else
				nSlot = k;
		}
		if ( nSlot < 0 )
		{
			Warning( "[GAMEUI] SelectStorageDevice for not active ctrlr!\n" );
			pSelectClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_ERROR );
			return false;	// this controller is not involved in the game, go away
		}
	}

	// Is the controller signed in?
	if( XUserGetSigninState( iController ) == eXUserSigninState_NotSignedIn )
	{
		Warning( "[GAMEUI] SelectStorageDevice for not signed in user!\n" );
		pSelectClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_ERROR );
		return false; // not signed in, no device selector
	}

	// Maybe a guest buddy?
	XUSER_SIGNIN_INFO xsi;
	if ( ERROR_SUCCESS == XUserGetSigninInfo( iController, XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY, &xsi ) &&
		(xsi.dwInfoFlags & XUSER_INFO_FLAG_GUEST) != 0 )
	{
		Warning( "[GAMEUI] SelectStorageDevice for LIVE-guest!\n" );
		pSelectClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_ERROR );
		return false; // guests don't have device selectors, go away
	}

	if ( ERROR_SUCCESS != XUserGetSigninInfo( iController, XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY, &xsi ) )
	{
		Warning( "[GAMEUI] SelectStorageDevice failed to obtain XUID!\n" );
		pSelectClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_ERROR );
		return false; // failed to obtain XUID?
	}

	//
	// Prevent reentry
	//
	if( m_bWaitingForStorageDeviceHandle )
	{
		Warning( "[GAMEUI] SelectStorageDevice is already busy selecting storage device! Cannot re-enter!\n" );
		pSelectClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_ERROR );
		return false;	// Somebody already selecting a device
	}

#if defined( _DEMO ) && defined( _X360 )
	// Demo mode cannot have access to storage devices anyway
	if ( IsX360() )
	{
		m_iStorageID = XBX_STORAGE_DECLINED;
		m_iStorageController = iController;
		m_pSelectStorageClient = pSelectClient;
		m_pSelectStorageClient->OnDeviceSelected();
		OnCompletedAsyncDeviceAttached( NULL );
		return true;
	}
#endif

	// Check if we already have a valid storage device
	if ( XBX_GetStorageDeviceId( iController ) != XBX_INVALID_STORAGE_ID &&
		( bAllowDeclined || XBX_GetStorageDeviceId( iController ) != XBX_STORAGE_DECLINED ) &&
		! bForceDisplay )
	{
		if ( UI_IsDebug() )
		{
			Msg( "[GAMEUI] SelectStorageDevice - storage id = 0x%08X\n",
				XBX_GetStorageDeviceId( iController ) );
		}

		// Put up a progress that we are loading profile...
		OpenWaitScreen( "#L4D360UI_WaitScreen_SigningOn" );

		m_iStorageID = XBX_GetStorageDeviceId( iController );
		m_iStorageController = iController;
		m_pSelectStorageClient = pSelectClient;

		OnSetStorageDeviceId( iController, XBX_GetStorageDeviceId( iController ) );

		// Already have a storage device
		return true;
	}

	// Put up a progress that we are loading profile...
	OpenWaitScreen( "#L4D360UI_WaitScreen_SigningOn", 0.0f );
	// NOTE: this shouldn't have a 3 sec time-out as a new wait message is taking over
	// the progress when container starts mounting

	//open the dialog
	m_bWaitingForStorageDeviceHandle = true;
	m_hStorageDeviceChangeHandle = xboxsystem->CreateAsyncHandle();
	m_iStorageID = XBX_INVALID_STORAGE_ID;
	m_iStorageController = iController;
	m_pSelectStorageClient = pSelectClient;
	xboxsystem->ShowDeviceSelector( iController, bForceDisplay, &m_iStorageID, &m_hStorageDeviceChangeHandle );
#endif
	return false;
}

//=============================================================================
void CUIGameData::OnDeviceAttached()
{
	//This is straight from CBasePanel
	ExecuteAsync( new CAsyncCtxUIOnDeviceAttached( m_iStorageController ) );
}

//=============================================================================
void CUIGameData::OnCompletedAsyncDeviceAttached( CAsyncCtxUIOnDeviceAttached * job )
{
	ISelectStorageDeviceClient *pStorageDeviceClient = m_pSelectStorageClient;
	m_pSelectStorageClient = NULL;

	static ConVarRef mm_dlcs_mask_extras( "mm_dlcs_mask_extras" );
	if ( mm_dlcs_mask_extras.IsValid() )
	{
#ifdef _X360
		int iDLCmask = mm_dlcs_mask_extras.GetInt();

		if ( engine->IsLowViolence() && XGetGameRegion() == XC_GAME_REGION_EUROPE_REST )
		{
			// iDLCmask |= ( 1 << ? );
		}

		mm_dlcs_mask_extras.SetValue( iDLCmask );
#endif
	}

	uint nRet = job ? job->GetContainerOpenResult() : ERROR_SUCCESS;
	if ( nRet != ERROR_SUCCESS )
	{
		CloseWaitScreen( NULL, "ReportDeviceCorrupt" );
		pStorageDeviceClient->OnDeviceFail( ISelectStorageDeviceClient::FAIL_CORRUPT );
	}
	else
	{
		// Notify that data has loaded
		pStorageDeviceClient->AfterDeviceMounted();

		// Check for opening a new storage device immediately
		if ( m_pSelectStorageClient == NULL )
		{
			// Close down the waiting screen
			CloseWaitScreen( NULL, "OnCompletedAsyncDeviceAttached" );
		}
	}
}

#ifdef _WIN32
//-------------------------
// Purpose: Job wrapper
//-------------------------
static unsigned UIGameDataJobWrapperFn( void *pvContext )
{
	CUIGameData::CAsyncJobContext *pAsync = reinterpret_cast< CUIGameData::CAsyncJobContext * >( pvContext );

	float const flTimeStart = Plat_FloatTime();

	pAsync->ExecuteAsync();

	float const flElapsedTime = Plat_FloatTime() - flTimeStart;

	if ( flElapsedTime < pAsync->m_flLeastExecuteTime )
	{
		ThreadSleep( ( pAsync->m_flLeastExecuteTime - flElapsedTime ) * 1000 );
	}

	ReleaseThreadHandle( ( ThreadHandle_t ) pAsync->m_hThreadHandle );
	pAsync->m_hThreadHandle = NULL;

	return 0;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Enqueues a job function to be called on a separate thread
//-----------------------------------------------------------------------------
void CUIGameData::ExecuteAsync( CAsyncJobContext *pAsync )
{
	Assert( !m_pAsyncJob );
	Assert( pAsync && !pAsync->m_hThreadHandle );
	m_pAsyncJob = pAsync;

#ifdef _WIN32
	ThreadHandle_t hHandle = CreateSimpleThread( UIGameDataJobWrapperFn, reinterpret_cast< void * >( pAsync ) );
	pAsync->m_hThreadHandle = hHandle;

#ifdef _X360
	ThreadSetAffinity( hHandle, XBOX_PROCESSOR_3 );
#endif

#else
	pAsync->ExecuteAsync();
#endif
}



