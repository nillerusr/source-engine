//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements all the functions exported by the GameUI dll
//
// $NoKeywords: $
//===========================================================================//

#ifdef WIN32
#if !defined( _X360 )
#include <windows.h>
#endif
#include <io.h>
#include <direct.h>
#elif defined( POSIX )
#include <sys/time.h>
#else
#error
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <tier0/dbg.h>

#ifdef SendMessage
#undef SendMessage
#endif
																
#include "filesystem.h"
#include "GameUI_Interface.h"
#include "Sys_Utils.h"
#include "string.h"
#include "tier0/icommandline.h"

// interface to engine
#include "EngineInterface.h"

#include "replay/ienginereplay.h"
#include "replay/ireplaysystem.h"

#include "VGuiSystemModuleLoader.h"
#include "bitmap/tgaloader.h"

#include "GameConsole.h"
#include "LoadingDialog.h"
#include "CDKeyEntryDialog.h"
#include "ModInfo.h"
#include "game/client/IGameClientExports.h"
#include "materialsystem/imaterialsystem.h"
#include "engine/imatchmaking.h"
#include "ixboxsystem.h"
#include "iachievementmgr.h"
#include "IGameUIFuncs.h"
#include <ienginevgui.h>
#include "steam/steam_api.h"
#include "BonusMapsDatabase.h"
#include "BonusMapsDialog.h"
#include "sourcevr/isourcevirtualreality.h"

// vgui2 interface
// note that GameUI project uses ..\vgui2\include, not ..\utils\vgui\include
#include "BasePanel.h"

#include <vgui/Cursor.h>
#include <KeyValues.h>
#include <vgui/ILocalize.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/PHandle.h>
#include "tier3/tier3.h"
#include "tier0/vcrmode.h"
#include "matsys_controls/matsyscontrols.h"
#include "steam/steam_api.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#include "tier0/dbg.h"
#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

IGameUIFuncs *gameuifuncs = NULL;
IEngineVGui *enginevguifuncs = NULL;
IMatchmaking *matchmaking = NULL;
IXboxSystem *xboxsystem = NULL;		// 360 only
vgui::ISurface *enginesurfacefuncs = NULL;
IVEngineClient *engine = NULL;
IEngineSound *enginesound = NULL;
IAchievementMgr *achievementmgr = NULL;
IEngineClientReplay *g_pEngineClientReplay = NULL;
ISourceVirtualReality *g_pSourceVR = NULL;

static CSteamAPIContext g_SteamAPIContext;
CSteamAPIContext *steamapicontext = &g_SteamAPIContext;

static CBasePanel *staticPanel = NULL;

class CGameUI;
CGameUI *g_pGameUI = NULL;

class CLoadingDialog;
vgui::DHANDLE<CLoadingDialog> g_hLoadingDialog;
vgui::VPANEL g_hLoadingBackgroundDialog = NULL;

static CGameUI g_GameUI;
static WHANDLE g_hMutex = NULL;
static WHANDLE g_hWaitMutex = NULL;

static IGameClientExports *g_pGameClientExports = NULL;
IGameClientExports *GameClientExports()
{
	return g_pGameClientExports;
}

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CGameUI &GameUI()
{
	return g_GameUI;
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameUI, IGameUI, GAMEUI_INTERFACE_VERSION, g_GameUI);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameUI::CGameUI()
{
	g_pGameUI = this;
	m_bTryingToLoadFriends = false;
	m_iFriendsLoadPauseFrames = 0;
	m_iGameIP = 0;
	m_iGameConnectionPort = 0;
	m_iGameQueryPort = 0;
	m_bActivatedUI = false;
	m_szPreviousStatusText[0] = 0;
	m_bIsConsoleUI = false;
	m_bHasSavedThisMenuSession = false;
	m_bOpenProgressOnStart = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGameUI::~CGameUI()
{
	g_pGameUI = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Initialization
//-----------------------------------------------------------------------------
void CGameUI::Initialize( CreateInterfaceFn factory )
{
	ConnectTier1Libraries( &factory, 1 );
	ConnectTier2Libraries( &factory, 1 );
	ConVar_Register( FCVAR_CLIENTDLL );
	ConnectTier3Libraries( &factory, 1 );

	enginesound = (IEngineSound *)factory(IENGINESOUND_CLIENT_INTERFACE_VERSION, NULL);
	engine = (IVEngineClient *)factory( VENGINE_CLIENT_INTERFACE_VERSION, NULL );

	steamapicontext->Init();

	ConVarRef var( "gameui_xbox" );
	m_bIsConsoleUI = var.IsValid() && var.GetBool();

	vgui::VGui_InitInterfacesList( "GameUI", &factory, 1 );
	vgui::VGui_InitMatSysInterfacesList( "GameUI", &factory, 1 );

	// load localization file
	g_pVGuiLocalize->AddFile( "Resource/gameui_%language%.txt", "GAME", true );

	// load mod info
	ModInfo().LoadCurrentGameInfo();

	// load localization file for kb_act.lst
	g_pVGuiLocalize->AddFile( "Resource/valve_%language%.txt", "GAME", true );

	enginevguifuncs = (IEngineVGui *)factory( VENGINE_VGUI_VERSION, NULL );
	enginesurfacefuncs = (vgui::ISurface *)factory(VGUI_SURFACE_INTERFACE_VERSION, NULL);
	gameuifuncs = (IGameUIFuncs *)factory( VENGINE_GAMEUIFUNCS_VERSION, NULL );
	matchmaking = (IMatchmaking *)factory( VENGINE_MATCHMAKING_VERSION, NULL );
	xboxsystem = (IXboxSystem *)factory( XBOXSYSTEM_INTERFACE_VERSION, NULL );
	g_pEngineClientReplay = (IEngineClientReplay *)factory( ENGINE_REPLAY_CLIENT_INTERFACE_VERSION, NULL );

	if ( ModInfo().SupportsVR() && CommandLine()->CheckParm( "-vr" ) )
	{
		g_pSourceVR = (ISourceVirtualReality *)factory( SOURCE_VIRTUAL_REALITY_INTERFACE_VERSION, NULL );
	}

	// NOTE: g_pEngineReplay intentionally not checked here
	if ( !enginesurfacefuncs || !gameuifuncs || !enginevguifuncs || !xboxsystem || (IsX360() && !matchmaking) )
	{
		Error( "CGameUI::Initialize() failed to get necessary interfaces\n" );
	}

	// setup base panel
	staticPanel = new CBasePanel();
	staticPanel->SetBounds(0, 0, 400, 300 );
	staticPanel->SetPaintBorderEnabled( false );
	staticPanel->SetPaintBackgroundEnabled( true );
	staticPanel->SetPaintEnabled( false );
	staticPanel->SetVisible( true );
	staticPanel->SetMouseInputEnabled( false );
	staticPanel->SetKeyBoardInputEnabled( false );

	vgui::VPANEL rootpanel = enginevguifuncs->GetPanel( PANEL_GAMEUIDLL );
	staticPanel->SetParent( rootpanel );
}

void CGameUI::PostInit()
{
	if ( IsX360() )
	{
		enginesound->PrecacheSound( "UI/buttonrollover.wav", true, true );
		enginesound->PrecacheSound( "UI/buttonclick.wav", true, true );
		enginesound->PrecacheSound( "UI/buttonclickrelease.wav", true, true );
		enginesound->PrecacheSound( "player/suit_denydevice.wav", true, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the specified panel as the background panel for the loading
//		dialog.  If NULL, default background is used.  If you set a panel,
//		it should be full-screen with an opaque background, and must be a VGUI popup.
//-----------------------------------------------------------------------------
void CGameUI::SetLoadingBackgroundDialog( vgui::VPANEL panel )
{
	g_hLoadingBackgroundDialog = panel;
}

void CGameUI::BonusMapUnlock( const char *pchFileName, const char *pchMapName )
{
	if ( !pchFileName || pchFileName[ 0 ] == '\0' || 
		 !pchMapName || pchMapName[ 0 ] == '\0' )
	{
		if ( !g_pBonusMapsDialog )
			return;

		g_pBonusMapsDialog->SetSelectedBooleanStatus( "lock", false );
		return;
	}

	if ( BonusMapsDatabase()->SetBooleanStatus( "lock", pchFileName, pchMapName, false ) )
	{
		BonusMapsDatabase()->RefreshMapData();

		if ( !g_pBonusMapsDialog )
		{
			// It unlocked without the bonus maps menu open, so flash the menu item
			CBasePanel *pBasePanel = BasePanel();
			if ( pBasePanel )
			{
				if ( GameUI().IsConsoleUI() )
				{
					if ( Q_stricmp( pchFileName, "scripts/advanced_chambers" ) == 0 )
					{
						pBasePanel->SetMenuItemBlinkingState( "OpenNewGameDialog", true );
					}
				}
				else
				{
					pBasePanel->SetMenuItemBlinkingState( "OpenBonusMapsDialog", true );
				}
			}

			BonusMapsDatabase()->SetBlink( true );
		}
		else
			g_pBonusMapsDialog->RefreshData();	// Update the open dialog
	}
}

void CGameUI::BonusMapComplete( const char *pchFileName, const char *pchMapName )
{
	if ( !pchFileName || pchFileName[ 0 ] == '\0' || 
		 !pchMapName || pchMapName[ 0 ] == '\0' )
	{
		if ( !g_pBonusMapsDialog )
			return;

		g_pBonusMapsDialog->SetSelectedBooleanStatus( "complete", true );
		BonusMapsDatabase()->RefreshMapData();
		g_pBonusMapsDialog->RefreshData();
		return;
	}

	if ( BonusMapsDatabase()->SetBooleanStatus( "complete", pchFileName, pchMapName, true ) )
	{
		BonusMapsDatabase()->RefreshMapData();

		// Update the open dialog
		if ( g_pBonusMapsDialog )
			g_pBonusMapsDialog->RefreshData();
	}
}

void CGameUI::BonusMapChallengeUpdate( const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest )
{
	if ( !pchFileName || pchFileName[ 0 ] == '\0' || 
		 !pchMapName || pchMapName[ 0 ] == '\0' || 
		 !pchChallengeName || pchChallengeName[ 0 ] == '\0' )
	{
		return;
	}
	else
	{
		if ( BonusMapsDatabase()->UpdateChallengeBest( pchFileName, pchMapName, pchChallengeName, iBest ) )
		{
			// The challenge best changed, so write it to the file
			BonusMapsDatabase()->WriteSaveData();
			BonusMapsDatabase()->RefreshMapData();

			// Update the open dialog
			if ( g_pBonusMapsDialog )
				g_pBonusMapsDialog->RefreshData();
		}
	}
}

void CGameUI::BonusMapChallengeNames( char *pchFileName, char *pchMapName, char *pchChallengeName )
{
	if ( !pchFileName || !pchMapName || !pchChallengeName )
		return;

	BonusMapsDatabase()->GetCurrentChallengeNames( pchFileName, pchMapName, pchChallengeName );
}

void CGameUI::BonusMapChallengeObjectives( int &iBronze, int &iSilver, int &iGold )
{
	BonusMapsDatabase()->GetCurrentChallengeObjectives( iBronze, iSilver, iGold );
}

void CGameUI::BonusMapDatabaseSave( void )
{
	BonusMapsDatabase()->WriteSaveData();
}

int CGameUI::BonusMapNumAdvancedCompleted( void )
{
	return BonusMapsDatabase()->NumAdvancedComplete();
}

void CGameUI::BonusMapNumMedals( int piNumMedals[ 3 ] )
{
	BonusMapsDatabase()->NumMedals( piNumMedals );
}

//-----------------------------------------------------------------------------
// Purpose: connects to client interfaces
//-----------------------------------------------------------------------------
void CGameUI::Connect( CreateInterfaceFn gameFactory )
{
	g_pGameClientExports = (IGameClientExports *)gameFactory(GAMECLIENTEXPORTS_INTERFACE_VERSION, NULL);

	achievementmgr = engine->GetAchievementMgr();

	if (!g_pGameClientExports)
	{
		Error("CGameUI::Initialize() failed to get necessary interfaces\n");
	}

	m_GameFactory = gameFactory;
}

//-----------------------------------------------------------------------------
// Purpose: Callback function; sends platform Shutdown message to specified window
//-----------------------------------------------------------------------------
int __stdcall SendShutdownMsgFunc(WHANDLE hwnd, int lparam)
{
	Sys_PostMessage(hwnd, Sys_RegisterWindowMessage("ShutdownValvePlatform"), 0, 1);
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Searches for GameStartup*.mp3 files in the sound/ui folder and plays one
//-----------------------------------------------------------------------------
void CGameUI::PlayGameStartupSound()
{
	if ( IsX360() )
		return;

	if ( CommandLine()->FindParm( "-nostartupsound" ) )
		return;

	FileFindHandle_t fh;

	CUtlVector<char *> fileNames;
	char path[ 512 ];

	bool bHolidayFound = false;

	// only want to run the holiday check for TF2
	const char *pGameName = CommandLine()->ParmValue( "-game", "hl2" );
	if ( ( Q_stricmp( pGameName, "tf" ) == 0 ) || ( Q_stricmp( pGameName, "tf_beta" ) == 0 ) )
	{
		// check for a holiday sound file
		const char *pszHoliday = NULL;
	
		if ( GameClientExports() )
		{
			pszHoliday = GameClientExports()->GetHolidayString();
			if ( pszHoliday && pszHoliday[0] )
			{
				Q_snprintf( path, sizeof( path ), "sound/ui/holiday/gamestartup_%s*.mp3", pszHoliday );
				Q_FixSlashes( path );

				char const *fn = g_pFullFileSystem->FindFirstEx( path, "MOD", &fh );
				{
					if ( fn )
					{
						bHolidayFound = true;
					}
				}
			}
		}
	}

	// only want to do this if we haven't found a holiday file
	if ( !bHolidayFound )
	{
		Q_snprintf( path, sizeof( path ), "sound/ui/gamestartup*.mp3" );
		Q_FixSlashes( path );
	}

	char const *fn = g_pFullFileSystem->FindFirstEx( path, "MOD", &fh );
	if ( fn )
	{
		do
		{
			char ext[ 10 ];
			Q_ExtractFileExtension( fn, ext, sizeof( ext ) );

			if ( !Q_stricmp( ext, "mp3" ) )
			{
				char temp[ 512 ];
				if ( bHolidayFound )
				{
					Q_snprintf( temp, sizeof( temp ), "ui/holiday/%s", fn );
				}
				else
				{
					Q_snprintf( temp, sizeof( temp ), "ui/%s", fn );
				}

				char *found = new char[ strlen( temp ) + 1 ];
				Q_strncpy( found, temp, strlen( temp ) + 1 );

				Q_FixSlashes( found );
				fileNames.AddToTail( found );
			}
	
			fn = g_pFullFileSystem->FindNext( fh );

		} while ( fn );

		g_pFullFileSystem->FindClose( fh );
	}

	// did we find any?
	if ( fileNames.Count() > 0 )
	{
#ifdef WIN32
		SYSTEMTIME SystemTime;
		GetSystemTime( &SystemTime );
		int index = SystemTime.wMilliseconds % fileNames.Count();
#else
		struct timeval tm;
		gettimeofday( &tm, NULL );
		int index = tm.tv_usec/1000 % fileNames.Count();
#endif

		if ( fileNames.IsValidIndex( index ) && fileNames[index] )
		{
			// Play the Saxxy music if we're in saxxy mode.
#if defined( SAXXYMAINMENU_ENABLED )
			bool bIsTF = false;
			const char *pGameDir = engine->GetGameDirectory();
			if ( pGameDir )
			{
				// Is the game TF?
				const int nStrLen = V_strlen( pGameDir );
				bIsTF = nStrLen
					&& nStrLen >= 2 &&
					pGameDir[nStrLen-2] == 't' &&
					pGameDir[nStrLen-1] == 'f';
			}

			// escape chars "*#" make it stream, and be affected by snd_musicvolume
			const char *pSoundFile = bIsTF ? "ui/holiday/gamestartup_saxxy.mp3" : fileNames[index];
#else
			const char *pSoundFile = fileNames[index];
#endif

			char found[ 512 ];
			Q_snprintf( found, sizeof( found ), "play *#%s", pSoundFile );

			engine->ClientCmd_Unrestricted( found );
		}

		fileNames.PurgeAndDeleteElements();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called to setup the game UI
//-----------------------------------------------------------------------------
void CGameUI::Start()
{
	// determine Steam location for configuration
	if ( !FindPlatformDirectory( m_szPlatformDir, sizeof( m_szPlatformDir ) ) )
		return;

	if ( IsPC() )
	{
		// setup config file directory
		char szConfigDir[512];
		Q_strncpy( szConfigDir, m_szPlatformDir, sizeof( szConfigDir ) );
		Q_strncat( szConfigDir, "config", sizeof( szConfigDir ), COPY_ALL_CHARACTERS );

		Msg( "Steam config directory: %s\n", szConfigDir );

		g_pFullFileSystem->AddSearchPath(szConfigDir, "CONFIG");
		g_pFullFileSystem->CreateDirHierarchy("", "CONFIG");

		// user dialog configuration
		vgui::system()->SetUserConfigFile("InGameDialogConfig.vdf", "CONFIG");

		g_pFullFileSystem->AddSearchPath( "platform", "PLATFORM" );
	}

	// localization
	g_pVGuiLocalize->AddFile( "Resource/platform_%language%.txt");
	g_pVGuiLocalize->AddFile( "Resource/vgui_%language%.txt");

	Sys_SetLastError( SYS_NO_ERROR );

	if ( IsPC() )
	{
		if ( !IsPosix() )
		{
			// Alfred says this is really, really old code that does some wacky crap that only
			//  happened in the first version of HL and it's the only game that does this and
			//  it was a steam testing type thing and we don't need to do it on Posix, etc.

			g_hMutex = Sys_CreateMutex( "ValvePlatformUIMutex" );
			g_hWaitMutex = Sys_CreateMutex( "ValvePlatformWaitMutex" );
			if ( g_hMutex == 0 || g_hWaitMutex == 0 || Sys_GetLastError() == SYS_ERROR_INVALID_HANDLE )
			{
				// error, can't get handle to mutex
				if (g_hMutex)
				{
					Sys_ReleaseMutex(g_hMutex);
				}
				if (g_hWaitMutex)
				{
					Sys_ReleaseMutex(g_hWaitMutex);
				}
				g_hMutex = NULL;
				g_hWaitMutex = NULL;
				Error("Steam Error: Could not access Steam, bad mutex\n");
				return;
			}
			unsigned int waitResult = Sys_WaitForSingleObject(g_hMutex, 0);
			if (!(waitResult == SYS_WAIT_OBJECT_0 || waitResult == SYS_WAIT_ABANDONED))
			{
				// mutex locked, need to deactivate Steam (so we have the Friends/ServerBrowser data files)
				// get the wait mutex, so that Steam.exe knows that we're trying to acquire ValveTrackerMutex
				waitResult = Sys_WaitForSingleObject(g_hWaitMutex, 0);
#ifdef WIN32
				if (waitResult == SYS_WAIT_OBJECT_0 || waitResult == SYS_WAIT_ABANDONED)
				{
					Sys_EnumWindows(SendShutdownMsgFunc, 1);
				}
#endif
			}
		}
			
		// Delay playing the startup music until the first frame
		m_bPlayGameStartupSound = true;

		// now we are set up to check every frame to see if we can friends/server browser
		m_bTryingToLoadFriends = true;
		m_iFriendsLoadPauseFrames = 1;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Validates the user has a cdkey in the registry
//-----------------------------------------------------------------------------
void CGameUI::ValidateCDKey()
{
	// this check is disabled, since we have no plans for an offline version of hl2
#if 0
	//!! hack, write out a regkey for now so developers don't have to type it in
	//!! undo this before release
	vgui::system()->SetRegistryString("HKEY_CURRENT_USER\\Software\\Valve\\Source\\Settings\\EncryptedCDKey", "QOgi:JXrJj<Eb8abkESf4Pg;OfofJwDzRsyH>AdjtyPnV[FB");

	// see what's in the registry
	if (!CCDKeyEntryDialog::IsValidWeakCDKeyInRegistry())
	{
		m_hCDKeyEntryDialog = new CCDKeyEntryDialog(NULL, false);
		m_hCDKeyEntryDialog->Activate();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Finds which directory the platform resides in
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGameUI::FindPlatformDirectory(char *platformDir, int bufferSize)
{
	platformDir[0] = '\0';

	if ( platformDir[0] == '\0' )
	{
		// we're not under steam, so setup using path relative to game
		if ( IsPC() )
		{
#ifdef WIN32
			if ( ::GetModuleFileName( ( HINSTANCE )GetModuleHandle( NULL ), platformDir, bufferSize ) )
			{
				char *lastslash = strrchr(platformDir, '\\'); // this should be just before the filename
				if ( lastslash )
				{
					*lastslash = 0;
					Q_strncat(platformDir, "\\platform\\", bufferSize, COPY_ALL_CHARACTERS );
					return true;
				}
			}
#else
			if ( getcwd( platformDir, bufferSize ) )
			{
				V_AppendSlash( platformDir, bufferSize );
				Q_strncat(platformDir, "platform", bufferSize, COPY_ALL_CHARACTERS );
				V_AppendSlash( platformDir, bufferSize );
				return true;
			}
#endif			
		}
		else
		{
			// xbox fetches the platform path from exisiting platform search path
			// path to executeable is not correct for xbox remote configuration
			if ( g_pFullFileSystem->GetSearchPath( "PLATFORM", false, platformDir, bufferSize ) )
			{
				char *pSeperator = strchr( platformDir, ';' );
				if ( pSeperator )
					*pSeperator = '\0';
				return true;
			}
		}

		Error( "Unable to determine platform directory\n" );
		return false;
	}

	return (platformDir[0] != 0);
}

//-----------------------------------------------------------------------------
// Purpose: Called to Shutdown the game UI system
//-----------------------------------------------------------------------------
void CGameUI::Shutdown()
{
	// notify all the modules of Shutdown
	g_VModuleLoader.ShutdownPlatformModules();

	// unload the modules them from memory
	g_VModuleLoader.UnloadPlatformModules();

	ModInfo().FreeModInfo();
	
	// release platform mutex
	// close the mutex
	if (g_hMutex)
	{
		Sys_ReleaseMutex(g_hMutex);
	}
	if (g_hWaitMutex)
	{
		Sys_ReleaseMutex(g_hWaitMutex);
	}
	
	BonusMapsDatabase()->WriteSaveData();

	steamapicontext->Clear();
	

	ConVar_Unregister();
	DisconnectTier3Libraries();
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();
}

//-----------------------------------------------------------------------------
// Purpose: just wraps an engine call to activate the gameUI
//-----------------------------------------------------------------------------
void CGameUI::ActivateGameUI()
{
	engine->ExecuteClientCmd("gameui_activate");
}

//-----------------------------------------------------------------------------
// Purpose: just wraps an engine call to hide the gameUI
//-----------------------------------------------------------------------------
void CGameUI::HideGameUI()
{
	engine->ExecuteClientCmd("gameui_hide");
}

//-----------------------------------------------------------------------------
// Purpose: Toggle allowing the engine to hide the game UI with the escape key
//-----------------------------------------------------------------------------
void CGameUI::PreventEngineHideGameUI()
{
	engine->ExecuteClientCmd("gameui_preventescape");
}

//-----------------------------------------------------------------------------
// Purpose: Toggle allowing the engine to hide the game UI with the escape key
//-----------------------------------------------------------------------------
void CGameUI::AllowEngineHideGameUI()
{
	engine->ExecuteClientCmd("gameui_allowescape");
}

//-----------------------------------------------------------------------------
// Purpose: Activate the game UI
//-----------------------------------------------------------------------------
void CGameUI::OnGameUIActivated()
{
	m_bActivatedUI = true;

	// hide/show the main panel to Activate all game ui
	staticPanel->SetVisible( true );

	// pause the server in single player
	if ( engine->GetMaxClients() <= 1 )
	{
		engine->ClientCmd_Unrestricted( "setpause" );
	}

	SetSavedThisMenuSession( false );

	// notify taskbar
	BasePanel()->OnGameUIActivated();

	if ( GameClientExports() )
	{
		const char *pGameName = CommandLine()->ParmValue( "-game", "hl2" );
		// only want to run this for TF2
		if ( ( Q_stricmp( pGameName, "tf" ) == 0 ) || ( Q_stricmp( pGameName, "tf_beta" ) == 0 ) )
		{
			GameClientExports()->OnGameUIActivated();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hides the game ui, in whatever state it's in
//-----------------------------------------------------------------------------
void CGameUI::OnGameUIHidden()
{
	if ( GameClientExports() )
	{
		const char *pGameName = CommandLine()->ParmValue( "-game", "hl2" );
		// only want to run this for TF2
		if ( ( Q_stricmp( pGameName, "tf" ) == 0 ) || ( Q_stricmp( pGameName, "tf_beta" ) == 0 ) )
		{
			GameClientExports()->OnGameUIHidden();
		}
	}

	// unpause the game when leaving the UI
	if ( engine->GetMaxClients() <= 1 )
	{
		engine->ClientCmd_Unrestricted("unpause");
	}

	BasePanel()->OnGameUIHidden();
}

//-----------------------------------------------------------------------------
// Purpose: paints all the vgui elements
//-----------------------------------------------------------------------------
void CGameUI::RunFrame()
{
	if ( IsX360() && m_bOpenProgressOnStart )
	{
		StartProgressBar();
		m_bOpenProgressOnStart = false;
	}

	// resize the background panel to the screen size
	int wide, tall;
	vgui::surface()->GetScreenSize(wide, tall);
	staticPanel->SetSize(wide,tall);

	// Run frames
	g_VModuleLoader.RunFrame();
	BasePanel()->RunFrame();

	// Play the start-up music the first time we run frame
	if ( IsPC() && m_bPlayGameStartupSound )
	{
		PlayGameStartupSound();
		m_bPlayGameStartupSound = false;
	}

	if ( IsPC() && ( ( IsPosix() && m_bTryingToLoadFriends ) || 
					( m_bTryingToLoadFriends && m_iFriendsLoadPauseFrames-- < 1 && g_hMutex && g_hWaitMutex ) ) )
	{
		// try and load Steam platform files
		unsigned int waitResult = Sys_WaitForSingleObject(g_hMutex, 0);
		if ( IsPosix() || ( waitResult == SYS_WAIT_OBJECT_0 || waitResult == SYS_WAIT_ABANDONED ))
		{
			// we got the mutex, so load Friends/Serverbrowser
			// clear the loading flag
			m_bTryingToLoadFriends = false;
			g_VModuleLoader.LoadPlatformModules(&m_GameFactory, 1, false);

			// release the wait mutex
			if ( !IsPosix() )
				Sys_ReleaseMutex(g_hWaitMutex);

			// notify the game of our game name
			const char *fullGamePath = engine->GetGameDirectory();
			const char *pathSep = strrchr( fullGamePath, '/' );
			if ( !pathSep )
			{
				pathSep = strrchr( fullGamePath, '\\' );
			}
			if ( pathSep )
			{
				KeyValues *pKV = new KeyValues("ActiveGameName" );
				pKV->SetString( "name", pathSep + 1 );
				pKV->SetInt( "appid", engine->GetAppID() );
				KeyValues *modinfo = new KeyValues("ModInfo");
				if ( modinfo->LoadFromFile( g_pFullFileSystem, "gameinfo.txt" ) )
				{
					pKV->SetString( "game", modinfo->GetString( "game", "" ) );
				}
				modinfo->deleteThis();
				
				g_VModuleLoader.PostMessageToAllModules( pKV );
			}

			// notify the ui of a game connect if we're already in a game
			if (m_iGameIP)
			{
				SendConnectedToGameMessage();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game connects to a server
//-----------------------------------------------------------------------------
void CGameUI::OLD_OnConnectToServer(const char *game, int IP, int port)
{
	// Nobody should use this anymore because the query port and the connection port can be different.
	// Use OnConnectToServer2 instead.
	Assert( false );
	OnConnectToServer2( game, IP, port, port );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game connects to a server
//-----------------------------------------------------------------------------
void CGameUI::OnConnectToServer2(const char *game, int IP, int connectionPort, int queryPort)
{
	m_iGameIP = IP;
	m_iGameConnectionPort = connectionPort;
	m_iGameQueryPort = queryPort;

	SendConnectedToGameMessage();
}


void CGameUI::SendConnectedToGameMessage()
{
	KeyValues *kv = new KeyValues( "ConnectedToGame" );
	kv->SetInt( "ip", m_iGameIP );
	kv->SetInt( "connectionport", m_iGameConnectionPort );
	kv->SetInt( "queryport", m_iGameQueryPort );

	g_VModuleLoader.PostMessageToAllModules( kv );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game disconnects from a server
//-----------------------------------------------------------------------------
void CGameUI::OnDisconnectFromServer( uint8 eSteamLoginFailure )
{
	m_iGameIP = 0;
	m_iGameConnectionPort = 0;
	m_iGameQueryPort = 0;

	g_VModuleLoader.PostMessageToAllModules(new KeyValues("DisconnectedFromGame"));

	if ( eSteamLoginFailure == STEAMLOGINFAILURE_NOSTEAMLOGIN )
	{
		if ( g_hLoadingDialog )
		{
			g_hLoadingDialog->DisplayNoSteamConnectionError();
		}
	}
	else if ( eSteamLoginFailure == STEAMLOGINFAILURE_VACBANNED )
	{
		if ( g_hLoadingDialog )
		{
			g_hLoadingDialog->DisplayVACBannedError();
		}
	}
	else if ( eSteamLoginFailure == STEAMLOGINFAILURE_LOGGED_IN_ELSEWHERE )
	{
		if ( g_hLoadingDialog )
		{
			g_hLoadingDialog->DisplayLoggedInElsewhereError();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: activates the loading dialog on level load start
//-----------------------------------------------------------------------------
void CGameUI::OnLevelLoadingStarted( bool bShowProgressDialog )
{
	g_VModuleLoader.PostMessageToAllModules( new KeyValues( "LoadingStarted" ) );

	// notify
	BasePanel()->OnLevelLoadingStarted();

	if ( bShowProgressDialog )
	{
		StartProgressBar();
	}

	// Don't play the start game sound if this happens before we get to the first frame
	m_bPlayGameStartupSound = false;
}

//-----------------------------------------------------------------------------
// Purpose: closes any level load dialog
//-----------------------------------------------------------------------------
void CGameUI::OnLevelLoadingFinished(bool bError, const char *failureReason, const char *extendedReason)
{
	StopProgressBar( bError, failureReason, extendedReason );

	// notify all the modules
	g_VModuleLoader.PostMessageToAllModules( new KeyValues( "LoadingFinished" ) );

	// hide the UI
	HideGameUI();

	// notify
	BasePanel()->OnLevelLoadingFinished();
}

//-----------------------------------------------------------------------------
// Purpose: Updates progress bar
// Output : Returns true if screen should be redrawn
//-----------------------------------------------------------------------------
bool CGameUI::UpdateProgressBar(float progress, const char *statusText)
{
	// if either the progress bar or the status text changes, redraw the screen
	bool bRedraw = false;

	if ( ContinueProgressBar( progress ) )
	{
		bRedraw = true;
	}
		
	if ( SetProgressBarStatusText( statusText ) )
	{
		bRedraw = true;
	}

	return bRedraw;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::StartProgressBar()
{
	if ( !g_hLoadingDialog.Get() )
	{
		g_hLoadingDialog = new CLoadingDialog(staticPanel);
	}

	// open a loading dialog
	m_szPreviousStatusText[0] = 0;
	g_hLoadingDialog->SetProgressPoint(0.0f);
	g_hLoadingDialog->Open();
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the screen should be updated
//-----------------------------------------------------------------------------
bool CGameUI::ContinueProgressBar( float progressFraction )
{
	if (!g_hLoadingDialog.Get())
		return false;

	g_hLoadingDialog->Activate();
	return g_hLoadingDialog->SetProgressPoint(progressFraction);
}

//-----------------------------------------------------------------------------
// Purpose: stops progress bar, displays error if necessary
//-----------------------------------------------------------------------------
void CGameUI::StopProgressBar(bool bError, const char *failureReason, const char *extendedReason)
{
	if (!g_hLoadingDialog.Get() && bError)
	{
		g_hLoadingDialog = new CLoadingDialog(staticPanel);
	}

	if (!g_hLoadingDialog.Get())
		return;

	if ( !IsX360() && bError )
	{
		// turn the dialog to error display mode
		g_hLoadingDialog->DisplayGenericError(failureReason, extendedReason);
	}
	else
	{
		// close loading dialog
		g_hLoadingDialog->Close();
		g_hLoadingDialog = NULL;
	}
	// should update the background to be in a transition here
}

//-----------------------------------------------------------------------------
// Purpose: sets loading info text
//-----------------------------------------------------------------------------
bool CGameUI::SetProgressBarStatusText(const char *statusText)
{
	if (!g_hLoadingDialog.Get())
		return false;

	if (!statusText)
		return false;

	if (!stricmp(statusText, m_szPreviousStatusText))
		return false;

	g_hLoadingDialog->SetStatusText(statusText);
	Q_strncpy(m_szPreviousStatusText, statusText, sizeof(m_szPreviousStatusText));
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::SetSecondaryProgressBar(float progress /* range [0..1] */)
{
	if (!g_hLoadingDialog.Get())
		return;

	g_hLoadingDialog->SetSecondaryProgress(progress);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::SetSecondaryProgressBarText(const char *statusText)
{
	if (!g_hLoadingDialog.Get())
		return;

	g_hLoadingDialog->SetSecondaryProgressText(statusText);
}

//-----------------------------------------------------------------------------
// Purpose: Returns prev settings
//-----------------------------------------------------------------------------
bool CGameUI::SetShowProgressText( bool show )
{
	if (!g_hLoadingDialog.Get())
		return false;

	return g_hLoadingDialog->SetShowProgressText( show );
}


//-----------------------------------------------------------------------------
// Purpose: returns true if we're currently playing the game
//-----------------------------------------------------------------------------
bool CGameUI::IsInLevel()
{
	const char *levelName = engine->GetLevelName();
	if (levelName && levelName[0] && !engine->IsLevelMainMenuBackground())
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're at the main menu and a background level is loaded
//-----------------------------------------------------------------------------
bool CGameUI::IsInBackgroundLevel()
{
	const char *levelName = engine->GetLevelName();
	if (levelName && levelName[0] && engine->IsLevelMainMenuBackground())
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're in a multiplayer game
//-----------------------------------------------------------------------------
bool CGameUI::IsInMultiplayer()
{
	return (IsInLevel() && engine->GetMaxClients() > 1);
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're playing back a replay
//-----------------------------------------------------------------------------
bool CGameUI::IsInReplay()
{
	return g_pEngineClientReplay && g_pEngineClientReplay->IsPlayingReplayDemo();
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're console ui
//-----------------------------------------------------------------------------
bool CGameUI::IsConsoleUI()
{
	return m_bIsConsoleUI;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we've saved without closing the menu
//-----------------------------------------------------------------------------
bool CGameUI::HasSavedThisMenuSession()
{
	return m_bHasSavedThisMenuSession;
}

void CGameUI::SetSavedThisMenuSession( bool bState )
{
	m_bHasSavedThisMenuSession = bState;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::ShowNewGameDialog( int chapter )
{
	char val[32];
	Q_snprintf( val, sizeof(val), "%d", chapter);
	staticPanel->OnOpenNewGameDialog(val);
}

//-----------------------------------------------------------------------------
// Purpose: Makes the loading background dialog visible, if one has been set
//-----------------------------------------------------------------------------
void CGameUI::ShowLoadingBackgroundDialog()
{
	if ( g_hLoadingBackgroundDialog )
	{
		vgui::ipanel()->SetParent( g_hLoadingBackgroundDialog, staticPanel->GetVPanel() );
		vgui::ipanel()->PerformApplySchemeSettings( g_hLoadingBackgroundDialog );
		vgui::ipanel()->SetVisible( g_hLoadingBackgroundDialog, true );		
		vgui::ipanel()->MoveToFront( g_hLoadingBackgroundDialog );
		vgui::ipanel()->SendMessage( g_hLoadingBackgroundDialog, new KeyValues( "activate" ), staticPanel->GetVPanel() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hides the loading background dialog, if one has been set
//-----------------------------------------------------------------------------
void CGameUI::HideLoadingBackgroundDialog()
{
	if ( g_hLoadingBackgroundDialog )
	{
		vgui::ipanel()->SetParent( g_hLoadingBackgroundDialog, NULL );
		vgui::ipanel()->SetVisible( g_hLoadingBackgroundDialog, false );		
		vgui::ipanel()->MoveToBack( g_hLoadingBackgroundDialog );
		vgui::ipanel()->SendMessage( g_hLoadingBackgroundDialog, new KeyValues( "deactivate" ), staticPanel->GetVPanel() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether a loading background dialog has been set
//-----------------------------------------------------------------------------
bool CGameUI::HasLoadingBackgroundDialog()
{
	return ( 0 != g_hLoadingBackgroundDialog );
}

//-----------------------------------------------------------------------------
// Purpose: Xbox 360 calls from engine to GameUI 
//-----------------------------------------------------------------------------
void CGameUI::SessionNotification( const int notification, const int param )
{
	BasePanel()->SessionNotification( notification, param );
}
void CGameUI::SystemNotification( const int notification )
{
	BasePanel()->SystemNotification( notification );
}
void CGameUI::ShowMessageDialog( const uint nType, vgui::Panel *pOwner )
{
	BasePanel()->ShowMessageDialog( nType, pOwner );
}
void CGameUI::CloseMessageDialog( const uint nType )
{
	BasePanel()->CloseMessageDialog( nType );
}
void CGameUI::UpdatePlayerInfo( uint64 nPlayerId, const char *pName, int nTeam, byte cVoiceState, int nPlayersNeeded, bool bHost )
{
	BasePanel()->UpdatePlayerInfo( nPlayerId, pName, nTeam, cVoiceState, nPlayersNeeded, bHost );
}
void CGameUI::SessionSearchResult( int searchIdx, void *pHostData, XSESSION_SEARCHRESULT *pResult, int ping )
{
	BasePanel()->SessionSearchResult( searchIdx, pHostData, pResult, ping );
}
void CGameUI::OnCreditsFinished( void )
{
	BasePanel()->OnCreditsFinished();
}
bool CGameUI::ValidateStorageDevice( int *pStorageDeviceValidated )
{
	return BasePanel()->ValidateStorageDevice( pStorageDeviceValidated );
}

void CGameUI::SetProgressOnStart()
{
	m_bOpenProgressOnStart = true;
}

void CGameUI::OnConfirmQuit( void )
{
	BasePanel()->OnOpenQuitConfirmationDialog();
}

bool CGameUI::IsMainMenuVisible( void )
{
	CBasePanel *pBasePanel = BasePanel();
	if ( pBasePanel )
		return (pBasePanel->IsVisible() && pBasePanel->GetMenuAlpha() > 0 );
	return false;
}

// Client DLL is providing us with a panel that it wants to replace the main menu with
void CGameUI::SetMainMenuOverride( vgui::VPANEL panel )
{
	CBasePanel *pBasePanel = BasePanel();
	if ( pBasePanel )
	{
		pBasePanel->SetMainMenuOverride( panel );
	}
}

// Client DLL is telling us that a main menu command was issued, probably from its custom main menu panel
void CGameUI::SendMainMenuCommand( const char *pszCommand )
{
	CBasePanel *pBasePanel = BasePanel();
	if ( pBasePanel )
	{
		pBasePanel->RunMenuCommand( pszCommand );
	}
}