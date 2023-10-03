//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VFoundGroupGames.h"
#include "EngineInterface.h"

#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

const char *COM_GetModDirectory();

//=============================================================================
FoundGroupGames::FoundGroupGames( Panel *parent, const char *panelName ) : BaseClass( parent, panelName )
{
}

//=============================================================================
void FoundGroupGames::PaintBackground()
{
	char const *szGameMode = m_pDataSettings->GetString( "game/mode" );

	BaseClass::DrawDialogBackground( CFmtStr( "#L4D360UI_FoundGroupGames_Title%s%s", szGameMode[0] ? "_" : "", szGameMode ), NULL,
		"#L4D360UI_FoundGroupGames_Subtitle", NULL, NULL );
}

void FoundGroupGames::OnEvent( KeyValues *pEvent )
{
	char const *szName = pEvent->GetName();

	if ( !Q_stricmp( "OnMatchServerMgrUpdate", szName ) )
	{
		char const *szUpdate = pEvent->GetString( "update", "" );
		if ( !Q_stricmp( "searchstarted", szUpdate ) )
		{
			extern ConVar ui_foundgames_spinner_time;
			m_flSearchStartedTime = Plat_FloatTime();
			m_flSearchEndTime = m_flSearchStartedTime + ui_foundgames_spinner_time.GetFloat();
			OnThink();
		}
		else if ( !Q_stricmp( "searchfinished", szUpdate ) )
		{
			m_flSearchStartedTime = 0.0f;
			UpdateGameDetails();
		}
		else if ( !Q_stricmp( "server", szUpdate ) )
		{
			// Friend's game details have been updated
			// Don't update every individual server, calls a full sort, etc
			// just wait for the searchfinished
			//UpdateGameDetails();
		}
	}
}

//=============================================================================
void FoundGroupGames::StartSearching( void )
{
	 g_pMatchFramework->GetMatchSystem()->GetUserGroupsServerManager()->EnableServersUpdate( true );
}

static void HandleJoinServerSession( FoundGameListItem::Info const &fi )
{
	if ( fi.mInfoType != FoundGameListItem::FGT_SERVER )
		return;

	IMatchServer *pIMatchServer = g_pMatchFramework->GetMatchSystem()->GetUserGroupsServerManager()
		->GetServerByOnlineId( fi.mFriendXUID );
	if ( !pIMatchServer )
		return;

	pIMatchServer->Join();
}

//=============================================================================
void FoundGroupGames::AddServersToList( void )
{
	IServerManager *mgr = g_pMatchFramework->GetMatchSystem()->GetUserGroupsServerManager();

	int numItems = mgr->GetNumServers();
	for( int i = 0; i < numItems; ++i )
	{
		IMatchServer *item = mgr->GetServerByIndex( i );
		KeyValues *pGameDetails = item->GetGameDetails();

		if ( !ShouldAddServerToList( pGameDetails ) )
			continue;

		FoundGameListItem::Info fi;

		fi.mInfoType = FoundGameListItem::FGT_SERVER;
		Q_strncpy( fi.Name, pGameDetails->GetString( "server/name", "#L4D_Default_Hostname" ), sizeof( fi.Name ) );

		fi.mIsJoinable = item->IsJoinable();
		fi.mbInGame = true;

		fi.miPing = pGameDetails->GetInt( "server/ping", 0 );
		fi.mPing = fi.GP_HIGH;
		if ( !Q_stricmp( "lan", pGameDetails->GetString( "system/network", "" ) ) )
			fi.mPing = fi.GP_SYSTEMLINK;

		fi.mpGameDetails = pGameDetails;

		fi.mFriendXUID = item->GetOnlineId();

		const char *szModDir = pGameDetails->GetString( "game/dir", "swarm" );
		if ( Q_stricmp( szModDir, COM_GetModDirectory() ) )
		{
			Q_snprintf( fi.mchOtherTitle, sizeof( fi.mchOtherTitle ), szModDir );
		}

		// Check if this is actually a non-joinable game
		if ( fi.IsOtherTitle() )
		{
			fi.mIsJoinable = false;
		}
		else if ( fi.IsDownloadable() )
		{
			fi.mIsJoinable = false;
		}
		else if ( fi.mbInGame )
		{
			char const *szHint = fi.GetNonJoinableShortHint();
			if ( !*szHint )
			{
				fi.mIsJoinable = true;
				fi.mpfnJoinGame = HandleJoinServerSession;
			}
			else
			{
				fi.mIsJoinable = false;
			}
		}

		AddGameFromDetails( fi );
	}
}

bool FoundGroupGames::ShouldAddServerToList( KeyValues *pGameSettings )
{
	char const *szGameMode = m_pDataSettings->GetString( "game/mode", "" );
	if ( !szGameMode || !*szGameMode )
		return true;

	char const *szServerGameMode = pGameSettings->GetString( "game/mode", "" );
	return !Q_stricmp( szServerGameMode, szGameMode );
}

void FoundGroupGames::SortListItems()
{
	FoundGames::SortListItems();
}

//=============================================================================
bool FoundGroupGames::IsADuplicateServer( FoundGameListItem *item, FoundGameListItem::Info const &fi )
{
	// Only check server address
	FoundGameListItem::Info const &ii = item->GetFullInfo();
	if ( ii.mFriendXUID == fi.mFriendXUID &&
#if defined( _X360 )
		1
#else
		ii.mpGameDetails->GetUint64( "player/xuidOnline" ) == fi.mpGameDetails->GetUint64( "player/xuidOnline" )
#endif
		 )
	{
		return true;
	}

	return false;
}