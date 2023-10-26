//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __UIGAMEDATA_H__
#define __UIGAMEDATA_H__

#include "vgui_controls/Panel.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/Button.h"
#include "tier1/utllinkedlist.h"
#include "tier1/UtlMap.h"
#include "tier1/keyvalues.h"
#include "tier1/fmtstr.h"

#ifndef _X360
#include "steam/steam_api.h"
#endif // _X360

#include "matchmaking/imatchframework.h"
#include "matchmaking/imatchsystem.h"
#include "matchmaking/iplayer.h"
#include "matchmaking/iplayermanager.h"
#include "matchmaking/iservermanager.h"

#include "ixboxsystem.h"

#include "basemodpanel.h"
#include "basemodframe.h"
#include "UIAvatarImage.h"
#include "tokenset.h"

#include "EngineInterface.h"

#include "matchmaking/mm_helpers.h"

namespace BaseModUI {

class CAsyncCtxUIOnDeviceAttached;

extern const tokenset_t< const char * > s_characterPortraits[];

//=============================================================================
//
//=============================================================================

//
// ISelectStorageDeviceClient
//
//		Client interface for device selector:
//			async flow is as follows:
//			client calls into SelectStorageDevice with its parameters established:
//				GetCtrlrIndex, ForceSelector, AllowDeclined
//			XUI blade shows up (or implicitly determines which device should be picked based on settings).
//			if OnSelectError callback fires, then the process failed.
//			if OnDeviceNotSelected fires, then the process is over, device not picked
//			if OnDeviceFull fires, then device has insufficient capacity and cannot be used
//			if OnDeviceSelected fires, then device has been picked and async operations on containers started
//				should wait for AfterDeviceMounted callback
//			when AfterDeviceMounted callback fires the device is fully mounted and ready
//
class ISelectStorageDeviceClient
{
public:
	virtual int  GetCtrlrIndex() = 0;			// Controller index (0, 1, 2 or 3)
	virtual bool ForceSelector() = 0;			// Whether device selector should be forcefully shown
	virtual bool AllowDeclined() = 0;			// Whether declining storage device is allowed
	virtual bool AllowAnyController() = 0;		// Whether any connected controller can be selecting storage or only game-committed

	enum FailReason_t
	{
		FAIL_ERROR,
		FAIL_NOT_SELECTED,
		FAIL_FULL,
		FAIL_CORRUPT
	};
	virtual void OnDeviceFail( FailReason_t eReason ) = 0; // Storage device has not been set
	
	virtual void OnDeviceSelected() = 0;		// After device has been picked in XUI blade, but before mounting symbolic roots and opening containers
	virtual void AfterDeviceMounted() = 0;		// After device has been successfully mounted, configs processed, etc.
};


//
// CChangeStorageDevice
//
//		Should be used when user wants to change storage device
//
class CChangeStorageDevice : public ISelectStorageDeviceClient
{
public:
	explicit CChangeStorageDevice( int iCtrlr );
	virtual ~CChangeStorageDevice() {}

public:
	virtual int  GetCtrlrIndex() { return m_iCtrlr; }
	virtual bool ForceSelector() { return m_bForce; }
	virtual bool AllowDeclined() { return m_bAllowDeclined; }
	virtual bool AllowAnyController() { return m_bAnyController; }

	virtual void OnDeviceFail( FailReason_t eReason ); // Storage device has not been set

	virtual void OnDeviceSelected();		// After device has been picked in XUI blade, but before mounting symbolic roots and opening containers
	virtual void AfterDeviceMounted();		// After device has been successfully mounted, configs processed, etc.

public:
	// Fired as a follow-up after all async operations finish and
	// all confirmation boxes are closed down by user
	virtual void DeviceChangeCompleted( bool bChanged );

public:
	int m_iCtrlr;
	bool m_bForce;
	bool m_bAllowDeclined;
	bool m_bAnyController;
	int m_nConfirmationData;
};


//
// UI game data
//

class CUIGameData : public IMatchEventsSink
{
public:
	CUIGameData();
	~CUIGameData();

	static CUIGameData* Get();
	static void Shutdown();

	void RunFrame();
	void RunFrame_Storage();
	void RunFrame_Invite();

	void Invite_Confirm();
	void Invite_Connecting();
	bool Invite_IsStorageDeviceValid();

	void OnGameUIPostInit();

	bool CanPlayer2Join();

	void OpenFriendRequestPanel(int index, uint64 playerXuid);
	void OpenInviteUI( char const *szInviteUiType );
	void ExecuteOverlayCommand( char const *szCommand );

	// Listening for match events
	virtual void OnEvent( KeyValues *pEvent );

	bool SignedInToLive();
	bool AnyUserSignedInToLiveWithMultiplayerDisabled();
	bool CheckAndDisplayErrorIfNotSignedInToLive( CBaseModFrame *pCallerFrame );
	bool CheckAndDisplayErrorIfOffline( CBaseModFrame *pCallerFrame, char const *szMsg );

	void DisplayOkOnlyMsgBox( CBaseModFrame *pCallerFrame, const char *szTitle, const char *szMsg );
	CBaseModFrame * GetParentWindowForSystemMessageBox();

	const char *GetLocalPlayerName( int iController );

	bool SelectStorageDevice( ISelectStorageDeviceClient *pSelectClient );
	void OnDeviceAttached();
	void OnCompletedAsyncDeviceAttached( CAsyncCtxUIOnDeviceAttached * job );

	void OnGameUIHidden();

	void SetLookSensitivity(float sensitivity);
	float GetLookSensitivity();

	bool IsXUIOpen();

	void OpenWaitScreen( const char * messageText, float minDisplayTime = 3.0f, KeyValues *pSettings = NULL, float maxDisplayTime = 0.0f );
	void UpdateWaitPanel( const char * messageText, float minDisplayTime = 3.0f );
	void UpdateWaitPanel( const wchar_t * messageText, float minDisplayTime = 3.0f );
	void CloseWaitScreen( vgui::Panel * callbackPanel, const char * messageName );

	void NeedConnectionProblemWaitScreen( void );
	void ShowPasswordUI( char const *pchCurrentPW );
	void FinishPasswordUI( bool bOk );

	vgui::IImage * GetAvatarImage( XUID playerID );
	char const * GetPlayerName( XUID playerID, char const *szPlayerNameSpeculative );

#if !defined( _X360 ) && !defined( NO_STEAM )
	STEAM_CALLBACK( CUIGameData, Steam_OnPersonaStateChanged, PersonaStateChange_t, m_CallbackPersonaStateChanged );
#endif

	void ReloadScheme();

	//
	// Implementation of async jobs
	//	An async job is enqueued by calling "ExecuteAsync" with the proper job context.
	//	Job's function "ExecuteAsync" is called on a separate thread.
	//	After the job finishes the "Completed" function is called on the
	//	main thread.
	//
	class CAsyncJobContext
	{
	public:
		CAsyncJobContext( float flLeastExecuteTime = 0.0f ) : m_flLeastExecuteTime( flLeastExecuteTime ), m_hThreadHandle( NULL )  {}
		virtual ~CAsyncJobContext() {}

		virtual void ExecuteAsync() = 0;		// Executed on the secondary thread
		virtual void Completed() = 0;			// Executed on the main thread

	public:
		void * volatile m_hThreadHandle;		// Handle to an async job thread waiting for
		float m_flLeastExecuteTime;				// Least amount of time this job should keep executing
	};

	CAsyncJobContext *m_pAsyncJob;
	void ExecuteAsync( CAsyncJobContext *pAsync );

private:
	bool IsActiveSplitScreenPlayerSpectating( void );

protected:
	static CUIGameData* m_Instance;
	static bool m_bModuleShutDown;
	bool m_CGameUIPostInit;

	float m_LookSensitivity;

	float m_flShowConnectionProblemTimer;
	float m_flTimeLastFrame;
	bool  m_bShowConnectionProblemActive;

	CUtlMap< XUID, CGameUiAvatarImage * > m_mapUserXuidToAvatar;
	CUtlMap< XUID, CUtlString > m_mapUserXuidToName;

	//XUI info
	bool m_bXUIOpen;

	//storage device info
	bool			m_bWaitingForStorageDeviceHandle;
	AsyncHandle_t	m_hStorageDeviceChangeHandle;
	uint			m_iStorageID;
	int				m_iStorageController;
	ISelectStorageDeviceClient *m_pSelectStorageClient;
	void OnSetStorageDeviceId( int iController, uint nDeviceId );
};

}

extern ConVar x360_audio_english;

extern ConVar demo_ui_enable;
extern ConVar demo_connect_string;

uint64 GetDlcInstalledMask();

bool GameModeHasDifficulty( char const *szGameMode );
bool GameModeHasRoundLimit( char const *szGameMode );
bool GameModeIsSingleChapter( char const *szGameMode );
char const * GameModeGetDefaultDifficulty( char const *szGameMode );


// 
// RemapText_t arrText[] = {
// 	{ "", "#SessionError_Unknown", RemapText_t::MATCH_FULL },
// 	{ "n/a", "#SessionError_NotAvailable", RemapText_t::MATCH_FULL },
// 	{ "create", "#SessionError_Create", RemapText_t::MATCH_FULL },
// 	{ "connect", "#SessionError_Connect", RemapText_t::MATCH_FULL },
// 	{ "full", "#SessionError_Full", RemapText_t::MATCH_FULL },
// 	{ "lock", "#SessionError_Lock", RemapText_t::MATCH_FULL },
// 	{ "kicked", "#SessionError_Kicked", RemapText_t::MATCH_FULL },
// 	{ "migrate", "#SessionError_Migrate", RemapText_t::MATCH_FULL },
// 	{ "SteamServersDisconnected", "#SessionError_SteamServersDisconnected", RemapText_t::MATCH_FULL },
// 	{ NULL, NULL, RemapText_t::MATCH_FULL }
// };
// szReason = RemapText_t::RemapRawText( arrText, szReason );
// 
struct RemapText_t
{
	char const *m_szRawText;
	char const *m_szRemapText;

	enum MatchPolicy_t
	{
		MATCH_FULL,
		MATCH_SUBSTR,
		MATCH_START
	};
	MatchPolicy_t m_eMatchPolicy;

	inline bool Match( char const *szRawText )
	{
		switch( m_eMatchPolicy )
		{
		case MATCH_FULL:
			return !Q_stricmp( szRawText, m_szRawText );
		case MATCH_SUBSTR:
			return Q_stristr( szRawText, m_szRawText ) != NULL;
		case MATCH_START:
			return StringHasPrefix( szRawText, m_szRawText );
		default:
			return false;
		}
	}

	inline static char const * RemapRawText( RemapText_t *pRemapTable, char const *szRawText )
	{
		for ( ; pRemapTable && pRemapTable->m_szRawText; ++ pRemapTable )
		{
			if ( pRemapTable->Match( szRawText ) )
			{
				return pRemapTable->m_szRemapText;
			}
		}
		return szRawText;
	}
};


#endif // __UIGAMEDATA_H__
