//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <winlite.h>
#undef CreateDialog
#ifdef WIN32
#include <direct.h>
#include <io.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include "vstdlib/pch_vstdlib.h"
#include "tier0/memdbgoff.h"
#include "vgui_controls/pch_vgui_controls.h"
#include "vgui_controls/Frame.h"
#include "tier0/memdbgon.h"

#include "tier3/tier3.h"

// steam3 API
//#include "steam/steam_querypackets.h"
#include "steam/steam_api.h"
#include "steam/isteamuser.h"
#include "steam/isteammatchmaking.h"
#include "steam/isteamfriends.h"

#include "ServerBrowser/IServerBrowser.h"
#include "IVguiModule.h"
#include "vgui_controls/Controls.h"

#include "netadr.h"
#include "filesystem.h"
#include "proto_oob.h"
#include "ModList.h"
#include "IRunGameEngine.h"

#include "OfflineMode.h"

// serverbrowser files

#include "igamelist.h"
#include "ServerListCompare.h"
#include "ServerBrowser.h"
#include "VACBannedConnRefusedDialog.h"
#include "DialogGameInfo.h"
#include "ServerContextMenu.h"
#include "DialogServerPassword.h"
#include "DialogAddServer.h"

// game list
#include "BaseGamesPage.h"
#include "BlacklistedServers.h"
#include "InternetGames.h"
#include "FavoriteGames.h"
#include "SpectateGames.h"
#include "LanGames.h"
#include "FriendsGames.h"
#include "HistoryGames.h"
#include "SpectateGames.h"
#include "CustomGames.h"
#include "ServerBrowserDialog.h"
#include "QuickListPanel.h"
#include "vgui_controls/PanelListPanel.h"

#include "replay/ienginereplay.h"

extern bool GameSupportsReplay();
extern bool IsReplayServer( gameserveritem_t &server );

#pragma warning( disable: 4355 )  //  warning C4355: 'this' : used in base member initializer list

#if defined( STEAM )
#define IsSteam()	true
#else
#define IsSteam()	false
#endif

using namespace vgui;
