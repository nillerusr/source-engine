//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#ifdef _WIN32
#include "winerror.h"
#endif
#include "achievementmgr.h"
#include "icommandline.h"
#include "KeyValues.h"
#include "filesystem.h"
#include "inputsystem/InputEnums.h"
#include "usermessages.h"
#include "fmtstr.h"
#ifdef CLIENT_DLL
#include "achievement_notification_panel.h"
#include "c_playerresource.h"

#else
#include "enginecallback.h"
#endif // CLIENT_DLL
#ifndef _X360
#include "steam/isteamuserstats.h"
#include "steam/isteamfriends.h"
#include "steam/isteamutils.h"
#else
#include "xbox/xbox_win32stubs.h"
#endif
#include "tier3/tier3.h"
#include "vgui/ILocalize.h"
#ifdef _X360
#include "GameUI/IGameUI.h"
#include "ixboxsystem.h"
#include "ienginevgui.h"
#endif  // _X360
#include "matchmaking/imatchframework.h"
#include "tier0/vprof.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

ConVar	cc_achievement_debug("achievement_debug", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "Turn on achievement debug msgs." );
const char *COM_GetModDirectory();

#define matchmaking g_pMatchFramework->GetMatchmaking()

extern ConVar developer;

#ifdef _X360
static CDllDemandLoader g_GameUI( "gameui" );	// FIXME: This is duplicated!
#endif // _X360

#ifdef DEDICATED
// Hack this for now until we get steam_api recompiling in the Steam codebase.
ISteamUserStats *SteamUserStats()
{
	return NULL;
}
#endif

#if defined( XBX_GetPrimaryUserId )
#undef XBX_GetPrimaryUserId
#endif

static int AchievementIDCompare( CBaseAchievement * const *ach1, CBaseAchievement * const *ach2 )
{
	return (*ach1)->GetAchievementID() < (*ach2)->GetAchievementID();
}

static int AchievementOrderCompare( CBaseAchievement * const *ach1, CBaseAchievement * const *ach2 )
{
	return (*ach1)->GetDisplayOrder() - (*ach2)->GetDisplayOrder();
}



//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CAchievementMgr::CAchievementMgr() : CAutoGameSystemPerFrame( "CAchievementMgr" )
#if !defined(NO_STEAM)
, m_CallbackUserStatsReceived( this, &CAchievementMgr::Steam_OnUserStatsReceived ),
m_CallbackUserStatsStored( this, &CAchievementMgr::Steam_OnUserStatsStored )
#endif
{
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		SetDefLessFunc( m_mapAchievement[i] );
		m_flLastClassChangeTime[i] = 0;
		m_flTeamplayStartTime[i] = 0;
		m_iMiniroundsCompleted[i] = 0;
		m_bDirty[i] = false;

		m_AchievementsAwarded[i].RemoveAll();
		m_AchievementsAwardedDuringCurrentGame[i].RemoveAll();
		m_bUserSlotActive[i] = false;
	}

	m_szMap[0] = 0;
	m_bCheatsEverOn = false;
	m_flTimeLastUpload = 0;

	m_flWaitingForStoreStatsCallback = 0.0f;
	m_bCallStoreStatsAfterCallback = false;

#ifdef _X360
	// Mark that we're not waiting for an async call to finish
	m_pendingAchievementState.Purge();
#endif // _X360
}

//-----------------------------------------------------------------------------
// Purpose: Initializer
//-----------------------------------------------------------------------------
bool CAchievementMgr::Init()
{
	// We can be created on either client (for multiplayer games) or server
	// (for single player), so register ourselves with the engine so UI has a uniform place 
	// to go get the pointer to us

#ifdef _DEBUG
	// There can be only one achievement manager instance; no one else should be registered
	IAchievementMgr *pAchievementMgr = engine->GetAchievementMgr();
	Assert( NULL == pAchievementMgr );
#endif // _DEBUG

	// register ourselves
	engine->SetAchievementMgr( this );

	// register for events
#ifdef GAME_DLL
	ListenForGameEvent( "entity_killed" );
	ListenForGameEvent( "game_init" );
#else
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "player_stats_updated" );
	ListenForGameEvent( "achievement_write_failed" );
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		usermessages->HookMessage( "AchievementEvent", MsgFunc_AchievementEvent );
	}
#endif // CLIENT_DLL



	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: called at init time after all systems are init'd.  We have to
//			do this in PostInit because the Steam app ID is not available earlier
//-----------------------------------------------------------------------------
void CAchievementMgr::PostInit()
{
	// get current game dir
	const char *pGameDir = COM_GetModDirectory();

	CBaseAchievementHelper *pAchievementHelper = CBaseAchievementHelper::s_pFirst;
	while ( pAchievementHelper )
	{
		for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			// create and initialize all achievements and insert them in our maps
			CBaseAchievement *pAchievement = pAchievementHelper->m_pfnCreate();
			pAchievement->m_pAchievementMgr = this;
			pAchievement->Init();
			pAchievement->CalcProgressMsgIncrement();
			pAchievement->SetUserSlot( i );

			// only add an achievement if it does not have a game filter (only compiled into the game it
			// applies to, or truly cross-game) or, if it does have a game filter, the filter matches current game.
			// (e.g. EP 1/2/... achievements are in shared binary but are game specific, they have a game filter for runtime check.)
			const char *pGameDirFilter = pAchievement->m_pGameDirFilter;
			if ( !pGameDirFilter || ( 0 == Q_strcmp( pGameDir, pGameDirFilter ) ) )
			{
				m_mapAchievement[i].Insert( pAchievement->GetAchievementID(), pAchievement );
			}
			else
			{
				// achievement is not for this game, don't use it
				delete pAchievement;
			}
		}

		pAchievementHelper = pAchievementHelper->m_pNext;
	}

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		// Add each of the achievements to a CUtlVector ordered by achievement ID
		FOR_EACH_MAP( m_mapAchievement[i], iter )
		{
			m_vecAchievement[i].AddToTail( m_mapAchievement[i][iter] );
		}
		m_vecAchievement[i].Sort( AchievementIDCompare );

		// Create the vector of achievements in display order
		m_vecAchievementInOrder[i].AddVectorToTail( m_vecAchievement[i] );
		m_vecAchievementInOrder[i].Sort( AchievementOrderCompare );

		// Clear the progress and achieved data for each splitscreen player
		ClearAchievementData( i );
	}

	if ( IsPC() )
	{
		UserConnected( STEAM_PLAYER_SLOT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Shuts down the achievement manager
//-----------------------------------------------------------------------------
void CAchievementMgr::Shutdown()
{

	SaveGlobalState();

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		FOR_EACH_MAP( m_mapAchievement[i], iter )
		{
			delete m_mapAchievement[i][iter];
		}
		m_mapAchievement[i].RemoveAll();
		m_vecAchievement[i].RemoveAll();
		m_vecAchievementInOrder[i].RemoveAll();
		m_vecKillEventListeners[i].RemoveAll();
		m_vecMapEventListeners[i].RemoveAll();
		m_vecComponentListeners[i].RemoveAll();
		m_AchievementsAwarded[i].RemoveAll();
		m_AchievementsAwardedDuringCurrentGame[i].RemoveAll();
	}

	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
}

//-----------------------------------------------------------------------------
// Purpose: Cleans up all achievements and then re-initializes them
//-----------------------------------------------------------------------------
void CAchievementMgr::InitializeAchievements( )
{
	Shutdown();
	PostInit();
}

#ifdef CLIENT_DLL
extern const ConVar *sv_cheats;
#endif

//-----------------------------------------------------------------------------
// Purpose: Do per-frame handling
//-----------------------------------------------------------------------------
void CAchievementMgr::Update( float frametime )
{
#ifdef CLIENT_DLL
	if ( !sv_cheats )
	{
		sv_cheats = cvar->FindVar( "sv_cheats" );
	}
#endif

#ifndef _DEBUG
	// keep track if cheats have ever been turned on during this level
	if ( !WereCheatsEverOn() )
	{
		if ( sv_cheats && sv_cheats->GetBool() )
		{
			m_bCheatsEverOn = true;
		}
	}
#endif

#ifdef _X360
	bool bWarningShown = false;
	for ( int i = m_pendingAchievementState.Count()-1; i >= 0; i-- )	// Iterate backwards to make deletion safe
	{
		// Check for a pending achievement write
		uint nResultCode;
		int nReturn = xboxsystem->GetOverlappedResult( m_pendingAchievementState[i].pOverlappedResult, &nResultCode, false );
		if ( nReturn == ERROR_IO_PENDING || nReturn == ERROR_IO_INCOMPLETE )
			continue;

		// Warn if the write failed
		if ( nReturn != ERROR_SUCCESS )
		{
			if ( bWarningShown == false )
			{
				// Create a game message to pop up a warning to the user
				IGameEvent *event = gameeventmanager->CreateEvent( "achievement_write_failed" );
				if ( event )
				{
					gameeventmanager->FireEvent( event );
					bWarningShown = true;
				}
			}

			// We need to unaward the achievement in this case!
			CBaseAchievement *pAchievement = GetAchievementByID( m_pendingAchievementState[i].nAchievementID, m_pendingAchievementState[i].nUserSlot );
			if ( pAchievement != NULL )
			{
				pAchievement->SetAchieved( false );
				m_bDirty[m_pendingAchievementState[i].nUserSlot] = true;
				m_AchievementsAwardedDuringCurrentGame->FindAndRemove( m_pendingAchievementState[i].nAchievementID );
				// FIXME: This doesn't account for incremental progress, but if *will* re-achieve these if you get them again
			}
		}

		// We've either succeeded or failed at this point, in both cases we don't care anymore!
		xboxsystem->ReleaseAsyncHandle( m_pendingAchievementState[i].pOverlappedResult );
		m_pendingAchievementState.FastRemove( i );
	}

	const float flStoreStatsTimeout = 10.0f;
	if ( m_flWaitingForStoreStatsCallback > 0.0f && gpGlobals->curtime > m_flWaitingForStoreStatsCallback + flStoreStatsTimeout )
	{
		m_flWaitingForStoreStatsCallback = 0.0f;
	}
#endif // _X360
}

//-----------------------------------------------------------------------------
// Purpose: called on level init
//-----------------------------------------------------------------------------
void CAchievementMgr::LevelInitPreEntity()
{
	m_bCheatsEverOn = false;


#	ifdef GAME_DLL
		// For single-player games, achievement mgr must live on the server.  (Only the server has detailed knowledge of game state.)
		Assert( !GameRules()->IsMultiplayer() );
#	else
		// For multiplayer games, achievement mgr must live on the client.  (Only the client can read/write player state from Steam/XBox Live.)
		Assert( GameRules()->IsMultiplayer() );
#	endif


	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		// clear list of achievements listening for events
		m_vecKillEventListeners[i].RemoveAll();
		m_vecMapEventListeners[i].RemoveAll();
		m_vecComponentListeners[i].RemoveAll();

		m_AchievementsAwarded[i].RemoveAll();

		m_flLastClassChangeTime[i] = 0;
		m_flTeamplayStartTime[i] = 0;
		m_iMiniroundsCompleted[i] = 0;
	}

	// client and server have map names available in different forms (full path on client, just file base name on server), 
	// cache it in base file name form here so we don't have to have different code paths each time we access it
#ifdef CLIENT_DLL	
	Q_FileBase( engine->GetLevelName(), m_szMap, ARRAYSIZE( m_szMap ) );
#else
	Q_strncpy( m_szMap, gpGlobals->mapname.ToCStr(), ARRAYSIZE( m_szMap ) );
#endif // CLIENT_DLL

	if ( IsX360() )
	{
		// need to remove the .360 extension on the end of the map name
		char *pExt = Q_stristr( m_szMap, ".360" );
		if ( pExt )
		{
			*pExt = '\0';
		}
	}

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		// Don't bother listening if a slot is not active
		if ( m_bUserSlotActive[i] )
		{
			// look through all achievements, see which ones we want to have listen for events
			FOR_EACH_MAP( m_mapAchievement[i], iAchievement )
			{
				CBaseAchievement *pAchievement = m_mapAchievement[i][iAchievement];

				// if the achievement only applies to a specific map, and it's not the current map, skip it
				const char *pMapNameFilter = pAchievement->m_pMapNameFilter;
				if ( pMapNameFilter && ( 0 != Q_strcmp( m_szMap, pMapNameFilter ) ) )
					continue;

				// if the achievement needs kill events, add it as a listener
				if ( pAchievement->GetFlags() & ACH_LISTEN_KILL_EVENTS )
				{
					m_vecKillEventListeners[i].AddToTail( pAchievement );
				}
				// if the achievement needs map events, add it as a listener
				if ( pAchievement->GetFlags() & ACH_LISTEN_MAP_EVENTS )
				{
					m_vecMapEventListeners[i].AddToTail( pAchievement );
				}
				// if the achievement needs map events, add it as a listener
				if ( pAchievement->GetFlags() & ACH_LISTEN_COMPONENT_EVENTS )
				{
					m_vecComponentListeners[i].AddToTail( pAchievement );
				}
				if ( pAchievement->IsActive() )
				{
					pAchievement->ListenForEvents();
				}
			}
			m_flLevelInitTime[i] = gpGlobals->curtime;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: called on level shutdown
//-----------------------------------------------------------------------------
void CAchievementMgr::LevelShutdownPreEntity()
{
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		// make all achievements stop listening for events 
		FOR_EACH_MAP( m_mapAchievement[i], iAchievement )
		{
			CBaseAchievement *pAchievement = m_mapAchievement[i][iAchievement];
			pAchievement->StopListeningForAllEvents();
		}
	}

	// save global state if we have any changes
	SaveGlobalStateIfDirty();

// 12/2/2008ywb:  HACK, this needs to know the nUserSlot!!!
	int nUserSlot = 0;
	UploadUserData( nUserSlot );
}

//-----------------------------------------------------------------------------
// Purpose: returns achievement for specified ID
//-----------------------------------------------------------------------------
CBaseAchievement *CAchievementMgr::GetAchievementByID( int iAchievementID, int nUserSlot )
{
	int iAchievement = m_mapAchievement[nUserSlot].Find( iAchievementID );
	if ( iAchievement != m_mapAchievement[nUserSlot].InvalidIndex() )
	{
		return m_mapAchievement[nUserSlot][iAchievement];
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: returns achievement with specified name.  NOTE: this iterates through
//			all achievements to find the name, intended for debugging purposes.
//			Use GetAchievementByID for fast lookup.
//-----------------------------------------------------------------------------
CBaseAchievement *CAchievementMgr::GetAchievementByName( const char *pchName, int nUserSlot )
{
	VPROF("GetAchievementByName");
	FOR_EACH_MAP_FAST( m_mapAchievement[nUserSlot], i )
	{
		CBaseAchievement *pAchievement = m_mapAchievement[nUserSlot][i];
		if ( pAchievement && 0 == ( Q_stricmp( pchName, pAchievement->GetName() ) ) )
			return pAchievement;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the achievement with the specified name has been achieved
//-----------------------------------------------------------------------------
bool CAchievementMgr::HasAchieved( const char *pchName, int nUserSlot )
{
	CBaseAchievement *pAchievement = GetAchievementByName( pchName, nUserSlot );
	if ( pAchievement )
		return pAchievement->IsAchieved();
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: downloads user data from Steam or XBox Live
//-----------------------------------------------------------------------------
void CAchievementMgr::UserDisconnected( int nUserSlot )
{
	m_bUserSlotActive[nUserSlot] = false;

	ClearAchievementData( nUserSlot );
}


//-----------------------------------------------------------------------------
// Read our achievement data from the title data.
//-----------------------------------------------------------------------------
void CAchievementMgr::ReadAchievementsFromTitleData( int iController, int iSlot )
{
	IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
	if ( !pPlayer )
		return;

	// Set the incremental achievement progress and components
	for ( int i=0; i<m_vecAchievement[iSlot].Count(); ++i )
	{
		CBaseAchievement *pAchievement = m_vecAchievement[iSlot][i];
		if ( pAchievement )
		{
			pAchievement->ReadProgress( pPlayer );
		}
	}

/*
	//DISABLED:  Avatar award support

	for ( int i=0; i<m_vecAward[iSlot].Count(); ++i )
	{
		CBaseAchievement *pAward = m_vecAward[iSlot][i];
		if ( pAward )
		{
			pAward->ReadProgress( pPlayer );
		}
	}
*/
}



//-----------------------------------------------------------------------------
// Purpose: downloads user data from Steam or XBox Live
//-----------------------------------------------------------------------------
void CAchievementMgr::UserConnected( int nUserSlot )
{
#ifdef CLIENT_DLL
	if ( IsPC() )
	{
		// ASSERT( STEAM_PLAYER_SLOT == nUserSlot )
		if ( steamapicontext->SteamUserStats() )
		{
			// request stat download; will get called back at OnUserStatsReceived when complete
			steamapicontext->SteamUserStats()->RequestCurrentStats();
		}

		m_bUserSlotActive[STEAM_PLAYER_SLOT] = true;

	}
	else if ( IsX360() )
	{
#if defined( _X360 )

		if ( XBX_GetUserIsGuest( nUserSlot ) )
			return;

		const int iController = XBX_GetUserId( nUserSlot );

		if ( iController == XBX_INVALID_USER_ID )
			return;

		if ( XUserGetSigninState( iController ) == eXUserSigninState_NotSignedIn )
			return;

		m_bUserSlotActive[nUserSlot] = true;

		// Download achievements from XBox Live
		bool bDownloadSuccessful = true;
		const int nTotalAchievements = GetAchievementCount();
		uint bytes;
		int ret = xboxsystem->EnumerateAchievements( iController, 0, 0, nTotalAchievements, &bytes, 0, false );
		if ( ret != ERROR_SUCCESS )
		{
			Warning( "Enumerate Achievements failed! Error %d", ret );
			bDownloadSuccessful = false;
		}

		// Enumerate the achievements from Live
		void *pBuffer = new byte[bytes];
		if ( bDownloadSuccessful )
		{
			ret = xboxsystem->EnumerateAchievements( iController, 0, 0, nTotalAchievements, pBuffer, bytes, false );

			if ( ret != nTotalAchievements )
			{
				Warning( "Enumerate Achievements failed! Error %d", ret );
				bDownloadSuccessful = false;
			}
		}

		if ( bDownloadSuccessful )
		{
			// Give live a chance to mark achievements as unlocked, in case the achievement manager
			// wasn't able to get that data (storage device missing, read failure, etc)
			XACHIEVEMENT_DETAILS *pXboxAchievements = (XACHIEVEMENT_DETAILS*)pBuffer;
			for ( int i = 0; i < nTotalAchievements; ++i )
			{
				CBaseAchievement *pAchievement = GetAchievementByID( pXboxAchievements[i].dwId, nUserSlot );
				if ( !pAchievement )
					continue;

				// Give Live a chance to claim the achievement as unlocked
				if ( AchievementEarned( pXboxAchievements[i].dwFlags ) )
				{
					pAchievement->SetAchieved( true );
				}
			}
		}

		delete pBuffer;

#endif // X360
	}
#endif // CLIENT_DLL
}

const char *COM_GetModDirectory()
{
	static char modDir[MAX_PATH];
	if ( Q_strlen( modDir ) == 0 )
	{
		const char *gamedir = CommandLine()->ParmValue("-game", CommandLine()->ParmValue( "-defaultgamedir", "hl2" ) );
		Q_strncpy( modDir, gamedir, sizeof(modDir) );
		if ( strchr( modDir, '/' ) || strchr( modDir, '\\' ) )
		{
			Q_StripLastDir( modDir, sizeof(modDir) );
			int dirlen = Q_strlen( modDir );
			Q_strncpy( modDir, gamedir + dirlen, sizeof(modDir) - dirlen );
		}
	}

	return modDir;
}

//-----------------------------------------------------------------------------
// Purpose: Uploads user data to Steam or XBox Live
//-----------------------------------------------------------------------------
void CAchievementMgr::UploadUserData( int nUserSlot )
{
#ifdef CLIENT_DLL
	if ( IsPC() && ( nUserSlot == STEAM_PLAYER_SLOT ) )
	{
		if ( steamapicontext->SteamUserStats() )
		{
			if ( m_flWaitingForStoreStatsCallback > 0.0f )
			{
				m_bCallStoreStatsAfterCallback = true;
				return;
			}
			// Upload current Steam client achievements & stats state to Steam.  Will get called back at OnUserStatsStored when complete.
			// Only values previously set via SteamUserStats() get uploaded
			if ( steamapicontext->SteamUserStats()->StoreStats() )
			{
				m_flWaitingForStoreStatsCallback = gpGlobals->curtime;
			}
			m_flTimeLastUpload = Plat_FloatTime();
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: saves global state to file
//-----------------------------------------------------------------------------

void CAchievementMgr::SaveGlobalState( )
{
	if ( IsPC() )
	{
		VPROF_BUDGET( "CAchievementMgr::SaveGlobalState", "Achievements" );
		// FIXMEL4DTOMAINMERGE
		// Requires IMatchmaking!!!
#if 0
		TitleData2 achievementTitleData;

		// Clear the data structure out
		memset( &achievementTitleData.sizeofachievements, 0, sizeof ( achievementTitleData.sizeofachievements ) );

		for ( int i = 0; i < m_vecAchievement[STEAM_PLAYER_SLOT].Count(); ++i )
		{
			CBaseAchievement *pAchievement = m_vecAchievement[STEAM_PLAYER_SLOT][i];

			achievementTitleData.achievements.uAchievementCount[i] = (uint32)pAchievement->GetCount();

			if ( pAchievement->HasComponents() )
			{
				achievementTitleData.achievements.uAchievementComponentBits[i] = pAchievement->GetComponentBits();
			}
		}
		matchmaking->UpdatePlayerData2( 0, achievementTitleData, TitleData2::CONTENTTYPE_ACHIEVEMENTS );
#endif
		m_bDirty[0] = false;
	}
	else if ( IsX360() )
	{
#ifdef _X360
#	if 1
#		pragma message( __FILE__ "(" __LINE__AS_STRING ") : warning custom: Disabling XBox achievement code for now. Easier than real fixes so we can get 360 compiling" )
#	else
		for ( int j = 0; j < MAX_SPLITSCREEN_PLAYERS; ++j )
		{
			if ( IsUserConnected( j ) )
			{
				TitleData2 achievementTitleData;

				// Clear the data structure out
				memset( &achievementTitleData.sizeofachievements, 0, sizeof ( achievementTitleData.sizeofachievements ) );

				for ( int i = 0; i < m_vecAchievement[j].Count(); ++i )
				{
					CBaseAchievement *pAchievement = m_vecAchievement[j][i];

					achievementTitleData.achievements.uAchievementCount[i] = (uint32)pAchievement->GetCount();

					if ( pAchievement->HasComponents() )
					{
						achievementTitleData.achievements.uAchievementComponentBits[i] = pAchievement->GetComponentBits();
					}
				}
				matchmaking->UpdatePlayerData2( XBX_GetUserId( j ), achievementTitleData, TitleData2::CONTENTTYPE_ACHIEVEMENTS );
			}
		}
#	endif
#endif
	}


}

//-----------------------------------------------------------------------------
// Purpose: saves global state to file if there have been any changes
//-----------------------------------------------------------------------------
void CAchievementMgr::SaveGlobalStateIfDirty( )
{
	if ( m_bDirty )
	{
		SaveGlobalState( );
	}
}

//-----------------------------------------------------------------------------
// Purpose: awards specified achievement
//-----------------------------------------------------------------------------
void CAchievementMgr::AwardAchievement( int iAchievementID, int nUserSlot )
{
#ifdef CLIENT_DLL
	CBaseAchievement *pAchievement = GetAchievementByID( iAchievementID, nUserSlot );
	Assert( pAchievement );
	if ( !pAchievement )
		return;

	if ( !CheckAchievementsEnabled() )
	{
		Msg( "Achievements disabled, ignoring achievement unlock for %s\n", pAchievement->GetName() );
		return;
	}

	if ( pAchievement->IsAchieved() )
	{
		if ( cc_achievement_debug.GetInt() > 0 )
		{
			Msg( "Achievement award called but already achieved: %s\n", pAchievement->GetName() );
		}
		return;
	}
	pAchievement->SetAchieved( true );

	if ( cc_achievement_debug.GetInt() > 0 )
	{
		Msg( "Achievement awarded: %s\n", pAchievement->GetName() );
	}

	// save state at next good opportunity.  (Don't do it immediately, may hitch at bad time.)
	m_bDirty[nUserSlot] = true;	

	if ( IsPC() )
	{		
		if ( steamapicontext->SteamUserStats() )
		{
			VPROF_BUDGET( "AwardAchievement", VPROF_BUDGETGROUP_STEAM );
			// set this achieved in the Steam client
			bool bRet = steamapicontext->SteamUserStats()->SetAchievement( pAchievement->GetName() );
			//		Assert( bRet );
			if ( bRet )
			{
				// upload achievement to steam
				UploadUserData( nUserSlot );
				m_AchievementsAwarded[nUserSlot].AddToTail( iAchievementID );
			}
		}


	}
	else if ( IsX360() )
	{
#ifdef _X360
		if ( xboxsystem )
		{
			PendingAchievementInfo_t pendingAchievementState = { iAchievementID, nUserSlot, NULL };
			xboxsystem->AwardAchievement( XBX_GetUserId( nUserSlot ), iAchievementID, &pendingAchievementState.pOverlappedResult );

			// Save off the results for checking later
			m_pendingAchievementState.AddToTail( pendingAchievementState );
		}
#endif
	}

	SaveGlobalStateIfDirty();

	// Add this one to the list of achievements earned during current session
	m_AchievementsAwardedDuringCurrentGame[nUserSlot].AddToTail( iAchievementID );
#endif // CLIENT_DLL
}

//-----------------------------------------------------------------------------
// Purpose: updates specified achievement
//-----------------------------------------------------------------------------
void CAchievementMgr::UpdateAchievement( int iAchievementID, int nData, int nUserSlot )
{
	CBaseAchievement *pAchievement = GetAchievementByID( iAchievementID, nUserSlot );
	Assert( pAchievement );
	if ( !pAchievement )
		return;

	if ( !CheckAchievementsEnabled() )
	{
		Msg( "Achievements disabled, ignoring achievement update for %s\n", pAchievement->GetName() );
		return;
	}

	if ( pAchievement->IsAchieved() )
	{
		if ( cc_achievement_debug.GetInt() > 0 )
		{
			Msg( "Achievement update called but already achieved: %s\n", pAchievement->GetName() );
		}
		return;
	}

	pAchievement->UpdateAchievement( nData );
}

//-----------------------------------------------------------------------------
// Purpose: clears state for all achievements
//-----------------------------------------------------------------------------
void CAchievementMgr::PreRestoreSavedGame()
{
	for ( int j = 0; j < MAX_SPLITSCREEN_PLAYERS; ++j )
	{
		FOR_EACH_MAP( m_mapAchievement[j], i )
		{
			m_mapAchievement[j][i]->PreRestoreSavedGame();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: clears state for all achievements
//-----------------------------------------------------------------------------
void CAchievementMgr::PostRestoreSavedGame()
{
	for ( int j = 0; j < MAX_SPLITSCREEN_PLAYERS; ++j )
	{
		FOR_EACH_MAP( m_mapAchievement[j], i )
		{
			m_mapAchievement[j][i]->PostRestoreSavedGame();
		}
	}
}

extern bool IsInCommentaryMode( void );

ConVar	cc_achievement_disable("achievement_disable", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "Turn off achievements." );

//-----------------------------------------------------------------------------
// Purpose: checks if achievements are enabled
//-----------------------------------------------------------------------------
bool CAchievementMgr::CheckAchievementsEnabled( )
{
	// if PC, Steam must be running and user logged in
	if ( cc_achievement_disable.GetBool() )
		return false;

	if ( IsPC() && !LoggedIntoSteam() )
	{
		Msg( "Achievements disabled: Steam not running.\n" );
		return false;
	}

	//No achievements in demo version.
#ifdef _DEMO
	return false;
#endif

#if defined( _X360 )
#	if 1
#		pragma message( __FILE__ "(" __LINE__AS_STRING ") : warning custom: Disabling XBox achievement code for now. Easier than real fixes so we can get 360 compiling" )
		Msg( "Achievements disabled: 360 broken for now.\n" );
		return false;
#	else
		uint state = XUserGetSigninState( XBX_GetActiveUserId() );
		if ( state == eXUserSigninState_NotSignedIn )
		{
			Msg( "Achievements disabled: not signed in to XBox user account.\n" );
			return false;
		}
#	endif
#endif

	// can't be in commentary mode, user is invincible
	if ( IsInCommentaryMode() )
	{
		Msg( "Achievements disabled: in commentary mode.\n" );
		return false;
	}

#ifdef CLIENT_DLL
	// achievements disabled if playing demo (Playback demo)
	if ( engine->IsPlayingDemo() )
	{
		Msg( "Achievements disabled: demo playing.\n" );
		return false;
	}
#endif // CLIENT_DLL

	if ( IsPC() )
	{
		// Don't award achievements if cheats are turned on.  
		if ( WereCheatsEverOn() )
		{
			// Cheats get turned on automatically if you run with -dev which many people do internally, so allow cheats if developer is turned on and we're not running
			// on Steam public
#ifdef CLIENT_DLL
			if ( ( developer.GetInt() == 0 ) || !steamapicontext->SteamUtils() || ( k_EUniversePublic == steamapicontext->SteamUtils()->GetConnectedUniverse() ) )
#else
			if ( developer.GetInt() == 0 )
#endif
			{
				Msg( "Achievements disabled: cheats turned on in this app session.\n" );
				return false;
			}
		}
	}

#ifdef INFESTED_DLL
#ifndef _DEBUG
	// no achievements in singleplayer
	if ( gpGlobals->maxClients <= 1 )
	{
		DevMsg( "Achievements disabled in singleplayer.\n" );
		return false;
	}
#endif
#endif

	return true;
}

#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: Returns the whether all of the local player's team mates are
//			on her friends list, and if there are at least the specified # of
//			teammates.  Involves cross-process calls to Steam so this is mildly
//			expensive, don't call this every frame.
//-----------------------------------------------------------------------------
bool CalcPlayersOnFriendsList( int iMinFriends )
{
	// Got message during connection
	if ( !g_PR )
		return false;

	Assert( g_pGameRules->IsMultiplayer() );

	// Do a cheap rejection: check teammate count first to see if we even need to bother checking w/Steam
	// Subtract 1 for the local player.
	if ( CalcPlayerCount()-1 < iMinFriends )
		return false;

	// determine local player team
	int iLocalPlayerIndex =  GetLocalPlayerIndex();
	uint64 XPlayerUid = 0;

	if ( IsPC() )
	{
		if ( !steamapicontext->SteamFriends() || !steamapicontext->SteamUtils() || !g_pGameRules->IsMultiplayer() )
			return false;
	}
	else if ( IsX360() )
	{
		if ( !g_pMatchFramework )
			return false;

		XPlayerUid = XBX_GetActiveUserId();
	}
	else
	{
		// other platforms...?
		return false;
	}
	// Loop through the players
	int iTotalFriends = 0;
	for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		// find all players who are on the local player's team
		if( ( iPlayerIndex != iLocalPlayerIndex ) && ( g_PR->IsConnected( iPlayerIndex ) ) )
		{
			if ( IsPC() )
			{
				player_info_t pi;
				if ( !engine->GetPlayerInfo( iPlayerIndex, &pi ) )
					continue;
				if ( !pi.friendsID )
					continue;

				// check and see if they're on the local player's friends list
				CSteamID steamID( pi.friendsID, 1, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
				if ( !steamapicontext->SteamFriends()->HasFriend( steamID, /*k_EFriendFlagImmediate*/ 0x04 ) )
					continue;
			}
			else if ( IsX360() )
			{
#ifdef _X360
#	if 1
#		pragma message( __FILE__ "(" __LINE__AS_STRING ") : warning custom: Disabling XBox achievement code for now. Easier than real fixes so we can get 360 compiling" )
#	else
				// check and see if they're on the local player's friends list
				BOOL bFriend = FALSE;
				XUserAreUsersFriends( XPlayerUid, &pi.xuid, 1, &bFriend, NULL );
				if ( !bFriend )
					continue;

				iTotalFriends++;
#	endif
#endif
			}
		}
	}

	return (iTotalFriends >= iMinFriends);
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether there are a specified # of teammates who all belong
//			to same clan as local player. Involves cross-process calls to Steam 
//			so this is mildly expensive, don't call this every frame.
//-----------------------------------------------------------------------------
bool CalcHasNumClanPlayers( int iClanTeammates )
{
	Assert( g_pGameRules->IsMultiplayer() );

	if ( IsPC() )
	{
		// Do a cheap rejection: check teammate count first to see if we even need to bother checking w/Steam
		// Subtract 1 for the local player.
		if ( CalcPlayerCount()-1 < iClanTeammates )
			return false;

		if ( !steamapicontext->SteamFriends() || !steamapicontext->SteamUtils() || !g_pGameRules->IsMultiplayer() )
			return false;

		// determine local player team
		int iLocalPlayerIndex =  GetLocalPlayerIndex();

		for ( int iClan = 0; iClan < steamapicontext->SteamFriends()->GetClanCount(); iClan++ )
		{
			int iClanMembersOnTeam = 0;
			CSteamID clanID = steamapicontext->SteamFriends()->GetClanByIndex( iClan );
			// enumerate all players
			for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
			{
				if( ( iPlayerIndex != iLocalPlayerIndex ) && ( g_PR->IsConnected( iPlayerIndex ) ) )
				{
					player_info_t pi;
					if ( engine->GetPlayerInfo( iPlayerIndex, &pi ) && ( pi.friendsID ) )
					{	
						// check and see if they're on the local player's friends list
						CSteamID steamID( pi.friendsID, 1, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
						if ( steamapicontext->SteamFriends()->IsUserInSource( steamID, clanID ) )
						{
							iClanMembersOnTeam++;
							if ( iClanMembersOnTeam == iClanTeammates )
								return true;
						}
					}
				}
			}
		}

		return false;
	}
	else if ( IsX360() )
	{
		// TODO: implement for 360
		return false;
	}
	else 
	{
		// other platforms...?
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the # of teammates of the local player
//-----------------------------------------------------------------------------
int	CalcTeammateCount()
{
	Assert( g_pGameRules->IsMultiplayer() );

	// determine local player team
	int iLocalPlayerIndex =  GetLocalPlayerIndex();
	int iLocalPlayerTeam = g_PR->GetTeam( iLocalPlayerIndex );

	int iNumTeammates = 0;
	for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		// find all players who are on the local player's team
		if( ( iPlayerIndex != iLocalPlayerIndex ) && ( g_PR->IsConnected( iPlayerIndex ) ) && ( g_PR->GetTeam( iPlayerIndex ) == iLocalPlayerTeam ) )
		{
			iNumTeammates++;
		}
	}
	return iNumTeammates;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the # of teammates of the local player
//-----------------------------------------------------------------------------
int	CalcPlayerCount()
{
	int iCount = 0;
	for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		// find all players who are on the local player's team
		if( g_PR->IsConnected( iPlayerIndex ) )
		{
			iCount++;
		}
	}
	return iCount;
}

#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: Resets all achievements.  For debugging purposes only
//-----------------------------------------------------------------------------
void CAchievementMgr::ResetAchievements()
{
#ifdef CLIENT_DLL
	if ( !IsPC() )
	{
		DevMsg( "Only available on PC\n" );
		return;
	}

	if ( !LoggedIntoSteam() )
	{
		Msg( "Steam not running, achievements disabled. Cannot reset achievements.\n" );
		return;
	}

	FOR_EACH_MAP( m_mapAchievement[STEAM_PLAYER_SLOT], i )
	{
		CBaseAchievement *pAchievement = m_mapAchievement[STEAM_PLAYER_SLOT][i];
		ResetAchievement_Internal( pAchievement );
	}
	UploadUserData( STEAM_PLAYER_SLOT );
	SaveGlobalState();
#endif // CLIENT_DLL
}

void CAchievementMgr::ResetAchievement( int iAchievementID )
{
#ifdef CLIENT_DLL
	if ( !IsPC() )
	{
		DevMsg( "Only available on PC\n" );
		return;
	}

	if ( !LoggedIntoSteam() )
	{
		Msg( "Steam not running, achievements disabled. Cannot reset achievements.\n" );
		return;
	}

	CBaseAchievement *pAchievement = GetAchievementByID( iAchievementID, STEAM_PLAYER_SLOT );
	Assert( pAchievement );
	if ( pAchievement )
	{
		ResetAchievement_Internal( pAchievement );
		UploadUserData( STEAM_PLAYER_SLOT );
		SaveGlobalState();
	}
#endif // CLIENT_DLL
}

//-----------------------------------------------------------------------------
// Purpose: Resets all achievements.  For debugging purposes only
//-----------------------------------------------------------------------------
void CAchievementMgr::PrintAchievementStatus()
{
	if ( IsPC() && !LoggedIntoSteam() )
	{
		Msg( "Steam not running, achievements disabled. Cannot view or unlock achievements.\n" );
		return;
	}

	Msg( "%42s %-20s %s\n", "Name:", "Status:", "Point value:" );
	int iTotalAchievements = 0, iTotalPoints = 0;
	FOR_EACH_MAP( m_mapAchievement[STEAM_PLAYER_SLOT], i )
	{
		CBaseAchievement *pAchievement = m_mapAchievement[STEAM_PLAYER_SLOT][i];

		Msg( "%42s ", pAchievement->GetName() );	

		CFailableAchievement *pFailableAchievement = dynamic_cast<CFailableAchievement *>( pAchievement );
		if ( pAchievement->IsAchieved() )
		{
			Msg( "%-20s", "ACHIEVED" );
		}
		else if ( pFailableAchievement && pFailableAchievement->IsFailed() )
		{
			Msg( "%-20s", "FAILED" );
		}
		else 
		{
			char szBuf[255];
			Q_snprintf( szBuf, ARRAYSIZE( szBuf ), "(%d/%d)%s", pAchievement->GetCount(), pAchievement->GetGoal(),
				pAchievement->IsActive() ? "" : " (inactive)" );
			Msg( "%-20s", szBuf );
		}
		Msg( " %d   ", pAchievement->GetPointValue() );
		pAchievement->PrintAdditionalStatus();
		Msg( "\n" );
		iTotalAchievements++;
		iTotalPoints += pAchievement->GetPointValue();
	}
	Msg( "Total achievements: %d  Total possible points: %d\n", iTotalAchievements, iTotalPoints );
}

//-----------------------------------------------------------------------------
// Purpose: called when a game event is fired
//-----------------------------------------------------------------------------
void CAchievementMgr::FireGameEvent( IGameEvent *event )
{
#ifdef CLIENT_DLL
	int nSplitScreenPlayer = event->GetInt( "splitscreenplayer" );
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSplitScreenPlayer );
#endif

	VPROF_( "CAchievementMgr::FireGameEvent", 1, VPROF_BUDGETGROUP_STEAM, false, 0 );
	const char *name = event->GetName();
	if ( 0 == Q_strcmp( name, "entity_killed" ) )
	{
#ifdef GAME_DLL
		CBaseEntity *pVictim = UTIL_EntityByIndex( event->GetInt( "entindex_killed", 0 ) );
		CBaseEntity *pAttacker = UTIL_EntityByIndex( event->GetInt( "entindex_attacker", 0 ) );
		CBaseEntity *pInflictor = UTIL_EntityByIndex( event->GetInt( "entindex_inflictor", 0 ) );
		OnKillEvent( pVictim, pAttacker, pInflictor, event );
#endif // GAME_DLL
	}
	else if ( 0 == Q_strcmp( name, "game_init" ) )
	{
#ifdef GAME_DLL
		// clear all state as though we were loading a saved game, but without loading the game
		PreRestoreSavedGame();
		PostRestoreSavedGame();
#endif // GAME_DLL
	}
#ifdef CLIENT_DLL
	else if ( 0 == Q_strcmp( name, "player_death" ) )
	{
		CBaseEntity *pVictim = ClientEntityList().GetEnt( engine->GetPlayerForUserID( event->GetInt("userid") ) );
		CBaseEntity *pAttacker = ClientEntityList().GetEnt( engine->GetPlayerForUserID( event->GetInt("attacker") ) );
		OnKillEvent( pVictim, pAttacker, NULL, event );
	}
	else if ( 0 == Q_strcmp( name, "localplayer_changeclass" ) )
	{
		// keep track of when the player last changed class
		m_flLastClassChangeTime[nSplitScreenPlayer] =  gpGlobals->curtime;
	}
	else if ( 0 == Q_strcmp( name, "localplayer_changeteam" ) )
	{
		// keep track of the time of transitions to and from a game team
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pLocalPlayer )
		{
			int iTeam = pLocalPlayer->GetTeamNumber();
			if ( iTeam > TEAM_SPECTATOR )
			{
				if ( 0 == m_flTeamplayStartTime[nSplitScreenPlayer] )
				{
					// player transitioned from no/spectator team to a game team, mark the time
					m_flTeamplayStartTime[nSplitScreenPlayer] = gpGlobals->curtime;
				}				
			}
			else
			{
				// player transitioned to no/spectator team, clear the teamplay start time
				m_flTeamplayStartTime[nSplitScreenPlayer] = 0;
			}			
		}		
	}
	else if ( 0 == Q_strcmp( name, "teamplay_round_start" ) )
	{
		if ( event->GetBool( "full_reset" ) )
		{
			// we're starting a full round, clear miniround count
			m_iMiniroundsCompleted[nSplitScreenPlayer] = 0;
		}
	}
	else if ( 0 == Q_strcmp( name, "teamplay_round_win" ) )
	{
		if ( false == event->GetBool( "full_round", true ) )
		{
			// we just finished a miniround but the round is continuing, increment miniround count
			m_iMiniroundsCompleted[nSplitScreenPlayer]++;
		}
	}
	else if ( 0 == Q_strcmp( name, "player_stats_updated" ) )
	{
		FOR_EACH_MAP( m_mapAchievement[nSplitScreenPlayer], i )
		{
			CBaseAchievement *pAchievement = m_mapAchievement[nSplitScreenPlayer][i];
			pAchievement->OnPlayerStatsUpdate();
		}
	}
	else if ( 0 == Q_strcmp( name, "achievement_write_failed" ) )
	{
#ifdef _X360
		// We didn't succeed and we're not waiting, so we failed
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnProfileUnavailable", "iController", XBX_GetUserId( nSplitScreenPlayer ) ) );
#endif
	}

#endif // CLIENT_DLL
}

//-----------------------------------------------------------------------------
// Purpose: called when a player or character has been killed
//-----------------------------------------------------------------------------
void CAchievementMgr::OnKillEvent( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event )
{
#ifdef CLIENT_DLL
	int nSplitScreenPlayer = SINGLE_PLAYER_SLOT;
	nSplitScreenPlayer = event->GetInt( "splitscreenplayer" );
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSplitScreenPlayer );
#endif

	// can have a NULL victim on client if victim has never entered local player's PVS
	if ( !pVictim )
		return;

	// if single-player game, calculate if the attacker is the local player and if the victim is the player enemy
	bool bAttackerIsPlayer = false;
	bool bVictimIsPlayerEnemy = false;
#ifdef GAME_DLL
	if ( !g_pGameRules->IsMultiplayer() )
	{
		CBasePlayer *pLocalPlayer = UTIL_GetLocalPlayer();
		if ( pLocalPlayer )
		{
			if ( pAttacker == pLocalPlayer )
			{
				bAttackerIsPlayer = true;
			}

			CBaseCombatCharacter *pBCC = dynamic_cast<CBaseCombatCharacter *>( pVictim );
			if ( pBCC && ( D_HT == pBCC->IRelationType( pLocalPlayer ) ) )
			{
				bVictimIsPlayerEnemy = true;
			}
		}
	}
#else
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	bVictimIsPlayerEnemy = !pLocalPlayer->InSameTeam( pVictim );
	if ( pAttacker == pLocalPlayer )
	{
		bAttackerIsPlayer = true;
	}
#endif // GAME_DLL

	for ( int j = 0; j < MAX_SPLITSCREEN_PLAYERS; ++j )
	{
		// look through all the kill event listeners and notify any achievements whose filters we pass
		FOR_EACH_VEC( m_vecKillEventListeners[j], iAchievement )
		{
			CBaseAchievement *pAchievement = m_vecKillEventListeners[j][iAchievement];

			if ( !pAchievement->IsActive() )
				continue;

			// if this achievement only looks for kills where attacker is player and that is not the case here, skip this achievement
			if ( ( pAchievement->GetFlags() & ACH_FILTER_ATTACKER_IS_PLAYER ) && !bAttackerIsPlayer )
				continue;

			// if this achievement only looks for kills where victim is killer enemy and that is not the case here, skip this achievement
			if ( ( pAchievement->GetFlags() & ACH_FILTER_VICTIM_IS_PLAYER_ENEMY ) && !bVictimIsPlayerEnemy )
				continue;

#if GAME_DLL
			// if this achievement only looks for a particular victim class name and this victim is a different class, skip this achievement
			const char *pVictimClassNameFilter = pAchievement->m_pVictimClassNameFilter;
			if ( pVictimClassNameFilter && !pVictim->ClassMatches( pVictimClassNameFilter ) )
				continue;

			// if this achievement only looks for a particular inflictor class name and this inflictor is a different class, skip this achievement
			const char *pInflictorClassNameFilter = pAchievement->m_pInflictorClassNameFilter;
			if ( pInflictorClassNameFilter &&  ( ( NULL == pInflictor ) || !pInflictor->ClassMatches( pInflictorClassNameFilter ) ) )
				continue;

			// if this achievement only looks for a particular attacker class name and this attacker is a different class, skip this achievement
			const char *pAttackerClassNameFilter = pAchievement->m_pAttackerClassNameFilter;
			if ( pAttackerClassNameFilter && ( ( NULL == pAttacker ) || !pAttacker->ClassMatches( pAttackerClassNameFilter ) ) )
				continue;

			// if this achievement only looks for a particular inflictor entity name and this inflictor has a different name, skip this achievement
			const char *pInflictorEntityNameFilter = pAchievement->m_pInflictorEntityNameFilter;
			if ( pInflictorEntityNameFilter && ( ( NULL == pInflictor ) || !pInflictor->NameMatches( pInflictorEntityNameFilter ) ) )
				continue;
#endif // GAME_DLL

			// we pass all filters for this achievement, notify the achievement of the kill
			pAchievement->Event_EntityKilled( pVictim, pAttacker, pInflictor, event );
		}
	}
}

void CAchievementMgr::OnAchievementEvent( int iAchievementID, int nUserSlot )
{
	// handle event for specific achievement
	CBaseAchievement *pAchievement = GetAchievementByID( iAchievementID, nUserSlot );
	Assert( pAchievement );
	if ( pAchievement )
	{
		if ( !pAchievement->IsAchieved() )
		{
			pAchievement->IncrementCount();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when a map-fired achievement event occurs
//-----------------------------------------------------------------------------
void CAchievementMgr::OnMapEvent( const char *pchEventName, int nUserSlot )
{
	Assert( pchEventName && *pchEventName );
	if ( !pchEventName || !*pchEventName ) 
		return;

	// see if this event matches the prefix for an achievement component
	FOR_EACH_VEC( m_vecComponentListeners[nUserSlot], iAchievement )
	{
		CBaseAchievement *pAchievement = m_vecComponentListeners[nUserSlot][iAchievement];
		Assert( pAchievement->m_pszComponentPrefix );
		if ( 0 == Q_strncmp( pchEventName, pAchievement->m_pszComponentPrefix, pAchievement->m_iComponentPrefixLen ) )
		{
			// prefix matches, tell the achievement a component was found
			pAchievement->OnComponentEvent( pchEventName );
			return;
		}
	}

	// look through all the map event listeners
	FOR_EACH_VEC( m_vecMapEventListeners[nUserSlot], iAchievement )
	{
		CBaseAchievement *pAchievement = m_vecMapEventListeners[nUserSlot][iAchievement];
		pAchievement->OnMapEvent( pchEventName );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns an achievement as it's abstract object. This interface is used by gameui.dll for getting achievement info.
// Input  : index - 
// Output : IBaseAchievement*
//-----------------------------------------------------------------------------
IAchievement* CAchievementMgr::GetAchievementByIndex( int index, int nUserSlot )
{
	Assert( m_vecAchievement[nUserSlot].IsValidIndex(index) );
	return (IAchievement*)m_vecAchievement[nUserSlot][index];
}


//-----------------------------------------------------------------------------
// Purpose: Returns an achievement as it's abstract object. This interface is used by gameui.dll for getting achievement info.
// Input  : orderIndex - 
// Output : IBaseAchievement*
//-----------------------------------------------------------------------------
IAchievement* CAchievementMgr::GetAchievementByDisplayOrder( int orderIndex, int nUserSlot )
{
	Assert( m_vecAchievementInOrder[nUserSlot].IsValidIndex(orderIndex) );
	return (IAchievement*)m_vecAchievementInOrder[nUserSlot][orderIndex];
}


//-----------------------------------------------------------------------------
// Purpose: Returns total achievement count. This interface is used by gameui.dll for getting achievement info.
// Input  :  - 
// Output : Count of achievements in manager's vector.
//-----------------------------------------------------------------------------
int CAchievementMgr::GetAchievementCount()
{
	return m_vecAchievement[SINGLE_PLAYER_SLOT].Count();
}

//-----------------------------------------------------------------------------
// Purpose: Handles events from the matchmaking framework.
//-----------------------------------------------------------------------------
void CAchievementMgr::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( FStrEq( szEvent, "OnProfileDataLoaded" ) )
	{
		// This event is sent when the title data blocks have been loaded.
		int iController = pEvent->GetInt( "iController" );
#ifdef _X360
		int nSlot = XBX_GetSlotByUserId( iController );
#else
		int nSlot = STEAM_PLAYER_SLOT;
#endif
		ReadAchievementsFromTitleData( iController, nSlot );
	}
#ifdef _X360
	else if ( FStrEq( szEvent, "OnProfilesChanged" ) )
	{
		// This is essentially a RESET
		for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			UserDisconnected( i );
		}

		// Mark the valid users as connected and try to download achievement data from LIVE
		for ( unsigned int i = 0; i < XBX_GetNumGameUsers(); ++i )
		{
			UserConnected( i );  
		}
	}
#endif
}

#if !defined(NO_STEAM)
//-----------------------------------------------------------------------------
// Purpose: called when stat download is complete
//-----------------------------------------------------------------------------
void CAchievementMgr::Steam_OnUserStatsReceived( UserStatsReceived_t *pUserStatsReceived )
{
#ifdef CLIENT_DLL
	Assert( steamapicontext->SteamUserStats() );
	if ( !steamapicontext->SteamUserStats() )
		return;

	if ( pUserStatsReceived->m_eResult != k_EResultOK )
	{
//		DevMsg( "CAchievementMgr: failed to download stats from Steam, EResult %d\n", pUserStatsReceived->m_eResult );
		return;
	}

	// run through the achievements and set their achieved state according to Steam data
	for ( int i = 0; i < m_vecAchievement[STEAM_PLAYER_SLOT].Count(); ++i )
	{
		CBaseAchievement *pAchievement = m_vecAchievement[STEAM_PLAYER_SLOT][i];
#ifndef INFESTED_DLL
		char szFieldName[64];
#endif
		int nCount = 0;
		int nComponentBits = 0;
		bool bAchieved = false;
		bool bRet1 = steamapicontext->SteamUserStats()->GetAchievement( pAchievement->GetName(), &bAchieved );

#ifdef INFESTED_DLL
		if ( bRet1 )
#else
		// TODO: these look hardcoded for L4D2 - remove?
		Q_snprintf( szFieldName, sizeof( szFieldName ), "TD2.Achievements.Count.%02d", i );
		bool bRet2 = steamapicontext->SteamUserStats()->GetStat( szFieldName, &nCount );

		Q_snprintf( szFieldName, sizeof( szFieldName ), "TD2.Achievements.Comp.%02d", i );
		bool bRet3 = steamapicontext->SteamUserStats()->GetStat( szFieldName, &nComponentBits );

		if ( bRet1 && bRet2 && bRet3 )
#endif
		{
			// set local achievement state
			pAchievement->SetAchieved( bAchieved );
			pAchievement->SetCount( nCount );

			if ( pAchievement->HasComponents() )
			{
				pAchievement->SetComponentBits( nComponentBits );
			}
		}
		else
		{
			DevMsg( "ISteamUserStats::GetAchievement failed for %s\n", pAchievement->GetName() );
		}

		pAchievement->EvaluateIsAlreadyAchieved();

		if ( pAchievement->StoreProgressInSteam() )
		{
			int iValue;
			char pszProgressName[1024];
			Q_snprintf( pszProgressName, 1024, "%s_STAT", pAchievement->GetName() );
			bool bRet = steamapicontext->SteamUserStats()->GetStat( pszProgressName, &iValue );
			if ( bRet )
			{
				pAchievement->SetCount( iValue );
			}
			else
			{
				DevMsg( "ISteamUserStats::GetStat failed to get progress value from Steam for achievement %s\n", pszProgressName );
			}

			if ( pAchievement->HasComponents() )
			{
				Q_snprintf( pszProgressName, 1024, "%s_COMP", pAchievement->GetName() );
				bool bRet = steamapicontext->SteamUserStats()->GetStat( pszProgressName, &iValue );
				if ( bRet )
				{
					pAchievement->SetComponentBits( iValue );
				}
				else
				{
					DevMsg( "ISteamUserStats::GetStat failed to get component value from Steam for achievement %s\n", pszProgressName );
				}
			}
		}	
	}

	// send an event to anyone else who needs Steam user stat data
	IGameEvent *event = gameeventmanager->CreateEvent( "user_data_downloaded" );
	if ( event )
	{
#ifdef GAME_DLL
		gameeventmanager->FireEvent( event );
#else
		gameeventmanager->FireEventClientSide( event );
#endif
	}



#endif // CLIENT_DLL
}

//-----------------------------------------------------------------------------
// Purpose: called when stat upload is complete
//-----------------------------------------------------------------------------
void CAchievementMgr::Steam_OnUserStatsStored( UserStatsStored_t *pUserStatsStored )
{
	if ( k_EResultOK != pUserStatsStored->m_eResult )
	{
		DevMsg( "CAchievementMgr: failed to upload stats to Steam, EResult %d\n", pUserStatsStored->m_eResult );
	} 

	else
	{
		if ( m_AchievementsAwarded[STEAM_PLAYER_SLOT].Count() > 0 )
		{
#ifndef GAME_DLL
			// send a message to the server about our achievement
			if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			{
				C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer( FirstValidSplitScreenSlot() );
				if ( pLocalPlayer )
				{
					char cmd[256];
					int iPlayerID = pLocalPlayer->GetUserID();
					unsigned short mask = UTIL_GetAchievementEventMask();

					Q_snprintf( cmd, sizeof( cmd ), "achievement_earned %d %d", m_AchievementsAwarded[STEAM_PLAYER_SLOT][0] ^ mask, ( iPlayerID ^ m_AchievementsAwarded[STEAM_PLAYER_SLOT][0] ) ^ mask );
					engine->ClientCmd_Unrestricted( cmd );
				}
			}
#endif			
			m_AchievementsAwarded[STEAM_PLAYER_SLOT].Remove( 0 );
		}

		// for each achievement that has not been achieved
		FOR_EACH_MAP( m_mapAchievement[STEAM_PLAYER_SLOT], iAchievement )
		{
			CBaseAchievement *pAchievement = m_mapAchievement[STEAM_PLAYER_SLOT][iAchievement];

			if ( !pAchievement->IsAchieved() )
			{
				pAchievement->OnSteamUserStatsStored();
			}
		}
	}

	m_flWaitingForStoreStatsCallback = 0.0f;
	if ( m_bCallStoreStatsAfterCallback )		// while waiting for this callback, we tried to store stats again.  Re-send the request now.
	{
		m_bCallStoreStatsAfterCallback = false;
		UploadUserData( STEAM_PLAYER_SLOT );
	}
}
#endif // !defined(NO_STEAM)


void CAchievementMgr::ResetAchievement_Internal( CBaseAchievement *pAchievement )
{
#ifdef CLIENT_DLL
	Assert( pAchievement );

	if ( steamapicontext->SteamUserStats() )
	{
		steamapicontext->SteamUserStats()->ClearAchievement( pAchievement->GetName() );		
	}

	pAchievement->SetAchieved( false );
	pAchievement->SetCount( 0 );	
	if ( pAchievement->HasComponents() )
	{
		pAchievement->SetComponentBits( 0 );
	}
	pAchievement->SetProgressShown( 0 );
	pAchievement->StopListeningForAllEvents();
	if ( pAchievement->IsActive() )
	{
		pAchievement->ListenForEvents();
	}
#endif // CLIENT_DLL
}

#ifdef CLIENT_DLL

void MsgFunc_AchievementEvent( bf_read &msg )
{
	int iAchievementID = (int) msg.ReadShort();
	CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
	if ( !pAchievementMgr )
		return;
	pAchievementMgr->OnAchievementEvent( iAchievementID, STEAM_PLAYER_SLOT );
}

#ifdef _DEBUG
CON_COMMAND_F( achievement_reset_all, "Clears all achievements", FCVAR_CHEAT )
{
	CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
	if ( !pAchievementMgr )
		return;
	pAchievementMgr->ResetAchievements();
}

CON_COMMAND_F( achievement_reset, "<internal name> Clears specified achievement", FCVAR_CHEAT )
{
	CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
	if ( !pAchievementMgr )
		return;

	if ( 2 != args.ArgC() )
	{
		Msg( "Usage: achievement_reset <internal name>\n" );
		return;
	}
	CBaseAchievement *pAchievement = pAchievementMgr->GetAchievementByName( args[1], STEAM_PLAYER_SLOT );
	if ( !pAchievement )
	{
		Msg( "Achievement %s not found\n", args[1] );
		return;
	}
	pAchievementMgr->ResetAchievement( pAchievement->GetAchievementID() );

}

CON_COMMAND_F( achievement_status, "Shows status of all achievement", FCVAR_CHEAT )
{
	CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
	if ( !pAchievementMgr )
		return;
	pAchievementMgr->PrintAchievementStatus();
}

CON_COMMAND_F( achievement_unlock, "<internal name> Unlocks achievement", FCVAR_CHEAT )
{
	CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
	if ( !pAchievementMgr )
		return;

	if ( 2 != args.ArgC() )
	{
		Msg( "Usage: achievement_unlock <internal name>\n" );
		return;
	}
	CBaseAchievement *pAchievement = pAchievementMgr->GetAchievementByName( args[1], STEAM_PLAYER_SLOT );
	if ( !pAchievement )
	{
		Msg( "Achievement %s not found\n", args[1] );
		return;
	}
	pAchievementMgr->AwardAchievement( pAchievement->GetAchievementID(), STEAM_PLAYER_SLOT );
}

CON_COMMAND_F( achievement_unlock_all, "Unlocks all achievements", FCVAR_CHEAT )
{
	CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
	if ( !pAchievementMgr )
		return;

	int iCount = pAchievementMgr->GetAchievementCount();
	for ( int i = 0; i < iCount; i++ )
	{
		IAchievement *pAchievement = pAchievementMgr->GetAchievementByIndex( i, STEAM_PLAYER_SLOT );
		if ( !pAchievement->IsAchieved() )
		{
			pAchievementMgr->AwardAchievement( pAchievement->GetAchievementID(), STEAM_PLAYER_SLOT );
		}
	}	
}

CON_COMMAND_F( achievement_evaluate, "<internal name> Causes failable achievement to be evaluated", FCVAR_CHEAT )
{
	CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
	if ( !pAchievementMgr )
		return;

	if ( 2 != args.ArgC() )
	{
		Msg( "Usage: achievement_evaluate <internal name>\n" );
		return;
	}
	CBaseAchievement *pAchievement = pAchievementMgr->GetAchievementByName( args[1], STEAM_PLAYER_SLOT );
	if ( !pAchievement )
	{
		Msg( "Achievement %s not found\n", args[1] );
		return;
	}

	CFailableAchievement *pFailableAchievement = dynamic_cast<CFailableAchievement *>( pAchievement );
	Assert( pFailableAchievement );
	if ( pFailableAchievement )
	{
		pFailableAchievement->OnEvaluationEvent();
	}
}

CON_COMMAND_F( achievement_test_friend_count, "Counts the # of teammates on local player's friends list", FCVAR_CHEAT )
{
	CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
	if ( !pAchievementMgr )
		return;
	if ( 2 != args.ArgC() )
	{
		Msg( "Usage: achievement_test_friend_count <min # of teammates>\n" );
		return;
	}
	int iMinFriends = atoi( args[1] );
	bool bRet = CalcPlayersOnFriendsList( iMinFriends );
	Msg( "You %s have at least %d friends in the game.\n", bRet ? "do" : "do not", iMinFriends );
}

CON_COMMAND_F( achievement_test_clan_count, "Determines if specified # of teammates belong to same clan w/local player", FCVAR_CHEAT )
{
	CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
	if ( !pAchievementMgr )
		return;

	if ( 2 != args.ArgC() )
	{
		Msg( "Usage: achievement_test_clan_count <# of clan teammates>\n" );
		return;
	}
	int iClanPlayers = atoi( args[1] );
	bool bRet = CalcHasNumClanPlayers( iClanPlayers );
	Msg( "There %s %d players who you're in a Steam group with.\n", bRet ? "are" : "are not", iClanPlayers );
}

CON_COMMAND_F( achievement_mark_dirty, "Mark achievement data as dirty", FCVAR_CHEAT )
{
	CAchievementMgr *pAchievementMgr = dynamic_cast<CAchievementMgr *>( engine->GetAchievementMgr() );
	if ( !pAchievementMgr )
		return;
	pAchievementMgr->SetDirty( true, STEAM_PLAYER_SLOT );
}
#endif // _DEBUG

#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: helper function to get entity model name
//-----------------------------------------------------------------------------
const char *GetModelName( CBaseEntity *pBaseEntity )
{
	CBaseAnimating *pBaseAnimating = dynamic_cast<CBaseAnimating *>( pBaseEntity );
	if ( pBaseAnimating )
	{
		CStudioHdr *pStudioHdr = pBaseAnimating->GetModelPtr();
		if ( pStudioHdr )
		{
			return pStudioHdr->pszName();
		}
	}

	return "";
}

//-----------------------------------------------------------------------------
// Purpose: Gets the list of achievements achieved during the current game
//-----------------------------------------------------------------------------
const CUtlVector<int>& CAchievementMgr::GetAchievedDuringCurrentGame( int nPlayerSlot )
{
	return m_AchievementsAwardedDuringCurrentGame[nPlayerSlot];
}

//-----------------------------------------------------------------------------
// Purpose: Reset the list of achievements achieved during the current game
//-----------------------------------------------------------------------------
void CAchievementMgr::ResetAchievedDuringCurrentGame( int nPlayerSlot )
{
	m_AchievementsAwardedDuringCurrentGame[nPlayerSlot].RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Clears achievement data for the a particular user slot
//-----------------------------------------------------------------------------
void CAchievementMgr::ClearAchievementData( int nUserSlot )
{
	FOR_EACH_VEC( m_vecAchievement[nUserSlot], i )
	{
		m_mapAchievement[nUserSlot][i]->ClearAchievementData();
	}
}

