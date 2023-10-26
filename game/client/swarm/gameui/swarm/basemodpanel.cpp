//========= Copyright  1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "cbase.h"
#include "basemodpanel.h"
#include "./GameUI/IGameUI.h"
#include "ienginevgui.h"
#include "engine/ienginesound.h"
#include "EngineInterface.h"
#include "tier0/dbg.h"
#include "ixboxsystem.h"
#include "GameUI_Interface.h"
#include "game/client/IGameClientExports.h"
#include "gameui/igameconsole.h"
#include "inputsystem/iinputsystem.h"
#include "FileSystem.h"
#include "filesystem/IXboxInstaller.h"
#include "tier2/renderutils.h"

#ifdef _X360
	#include "xbox/xbox_launch.h"
#endif

// BaseModUI High-level windows
#include "VTransitionScreen.h"
#include "VAchievements.h"
#include "vaddonassociation.h"
#include "VAddons.h"
#include "VAttractScreen.h"
#include "VAudio.h"
#include "VAudioVideo.h"
#include "VCloud.h"
#include "VControllerOptions.h"
#include "VControllerOptionsButtons.h"
#include "VControllerOptionsSticks.h"
#include "VDownloads.h"
#include "VFoundGames.h"
#include "VFoundGroupGames.h"
#include "vfoundpublicgames.h"
#include "VGameLobby.h"
#include "VGameOptions.h"
#include "VGameSettings.h"
#include "VGenericConfirmation.h"
#include "VGenericWaitScreen.h"
#include "vgetlegacydata.h"
#include "VInGameDifficultySelect.h"
#include "VInGameMainMenu.h"
#include "VInGameChapterSelect.h"
#include "VInGameKickPlayerList.h"
#include "VKeyboardMouse.h"
#include "vkeyboard.h"
#include "VVoteOptions.h"
#include "VLoadingProgress.h"
#include "VMainMenu.h"
#include "VMultiplayer.h"
#include "VOptions.h"
#include "VSignInDialog.h"
#include "VFooterPanel.h"
#include "VPasswordEntry.h"
#include "VVideo.h"
#include "VSteamCloudConfirmation.h"
#include "vcustomcampaigns.h"
#include "vdownloadcampaign.h"
#include "vjukebox.h"
#include "vleaderboard.h"
#include "gameconsole.h"
#include "vgui/ISystem.h"
#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/AnimationController.h"
#include "gameui_util.h"
#include "vguimatsurface/imatsystemsurface.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "tier0/icommandline.h"
#include "fmtstr.h"
#include "smartptr.h"
#include "nb_header_footer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace BaseModUI;
using namespace vgui;

//setup in GameUI_Interface.cpp
extern class IMatchSystem *matchsystem;
extern const char *COM_GetModDirectory( void );
extern IGameConsole *IGameConsole();

//=============================================================================
CBaseModPanel* CBaseModPanel::m_CFactoryBasePanel = 0;

#ifndef _CERT
#ifdef _X360
ConVar ui_gameui_debug( "ui_gameui_debug", "1" );
#else
ConVar ui_gameui_debug( "ui_gameui_debug", "0", FCVAR_RELEASE );
#endif
int UI_IsDebug()
{
	return (*(int *)(&ui_gameui_debug)) ? ui_gameui_debug.GetInt() : 0;
}
#endif

#if defined( _X360 )
static void InstallStatusChanged( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	// spew out status
	if ( ((ConVar *)pConVar)->GetBool() && g_pXboxInstaller )
	{
		g_pXboxInstaller->SpewStatus();
	}
}
ConVar xbox_install_status( "xbox_install_status", "0", 0, "Show install status", InstallStatusChanged );
#endif

// Use for show demos to force the correct campaign poster
ConVar demo_campaign_name( "demo_campaign_name", "L4D2C5", FCVAR_DEVELOPMENTONLY, "Short name of campaign (i.e. L4D2C5), used to show correct poster in demo mode." );

ConVar ui_lobby_noresults_create_msg_time( "ui_lobby_noresults_create_msg_time", "2.5", FCVAR_DEVELOPMENTONLY );

//=============================================================================
CBaseModPanel::CBaseModPanel(): BaseClass(0, "CBaseModPanel"),
	m_bClosingAllWindows( false ),
	m_lastActiveUserId( 0 )
{
#if !defined( _X360 ) && !defined( NOSTEAM )
	// Set Steam overlay position
	if ( steamapicontext && steamapicontext->SteamUtils() )
	{
		steamapicontext->SteamUtils()->SetOverlayNotificationPosition( k_EPositionTopRight );
	}

	// Set special DLC parameters mask
	static ConVarRef mm_dlcs_mask_extras( "mm_dlcs_mask_extras" );
	if ( mm_dlcs_mask_extras.IsValid() && steamapicontext && steamapicontext->SteamUtils() )
	{
		int iDLCmask = mm_dlcs_mask_extras.GetInt();

		// Low Violence and Germany (or bordering countries) = CS.GUNS
		char const *cc = steamapicontext->SteamUtils()->GetIPCountry();
		char const *ccGuns = ":DE:DK:PL:CZ:AT:CH:FR:LU:BE:NL:";
		if ( engine->IsLowViolence() && Q_stristr( ccGuns, CFmtStr( ":%s:", cc ) ) )
		{
			// iDLCmask |= ( 1 << ? );
		}

		// PreOrder DLC AppId Ownership = BAT
		if ( steamapicontext->SteamApps()->BIsSubscribedApp( 565 ) )
		{
			// iDLCmask |= ( 1 << ? );
		}

		mm_dlcs_mask_extras.SetValue( iDLCmask );
	}

#endif

	MakePopup( false );

	Assert(m_CFactoryBasePanel == 0);
	m_CFactoryBasePanel = this;

	g_pVGuiLocalize->AddFile("Resource/swarm_%language%.txt");
	g_pVGuiLocalize->AddFile( "Resource/basemodui_%language%.txt");

	m_LevelLoading = false;
	
	for ( int k = 0; k < WPRI_COUNT; ++ k )
	{
		m_ActiveWindow[k] = WT_NONE;
	}

	// delay 3 frames before doing activation on initialization
	// needed to allow engine to exec startup commands (background map signal is 1 frame behind) 
	m_DelayActivation = 3;

	m_UIScheme = vgui::scheme()->LoadSchemeFromFileEx( 0, "resource/SwarmSchemeNew.res", "SwarmScheme" );
	SetScheme( m_UIScheme );

	// Only one user on the PC, so set it now
	SetLastActiveUserId( IsPC() ? 0 : -1 );

	// Precache critical font characters for the 360, dampens severity of these runtime i/o hitches
	IScheme *pScheme = vgui::scheme()->GetIScheme( m_UIScheme );
	m_hDefaultFont = pScheme->GetFont( "Default", true );
	vgui::surface()->PrecacheFontCharacters( m_hDefaultFont, NULL );
	vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "DefaultBold", true ), NULL );
	vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "DefaultLarge", true ), NULL );
	vgui::surface()->PrecacheFontCharacters( pScheme->GetFont( "FrameTitle", true ), NULL );

#ifdef _X360
	x360_audio_english.SetValue( XboxLaunch()->GetForceEnglish() );
#endif

	m_FooterPanel = new CBaseModFooterPanel( this, "FooterPanel" );
	m_hOptionsDialog = NULL;

	m_bWarmRestartMode = false;
	m_ExitingFrameCount = 0;

	m_flBlurScale = 0;
	m_flLastBlurTime = 0;

	m_iBackgroundImageID = -1;
	m_iProductImageID = -1;

	m_backgroundMusic = "Misc.MainUI";
	m_nBackgroundMusicGUID = 0;

	m_nProductImageWide = 0;
	m_nProductImageTall = 0;
	m_flMovieFadeInTime = 0.0f;
	m_pBackgroundMaterial = NULL;
	m_pBackgroundTexture = NULL;

	// Subscribe to event notifications
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
}

//=============================================================================
CBaseModPanel::~CBaseModPanel()
{
	ReleaseStartupGraphic();

	// Unsubscribe from event notifications
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	if ( m_FooterPanel )
	{
//		delete m_FooterPanel;
		m_FooterPanel->MarkForDeletion();
		m_FooterPanel = NULL;
	}	

	Assert(m_CFactoryBasePanel == this);
	m_CFactoryBasePanel = 0;

	surface()->DestroyTextureID( m_iBackgroundImageID );
	surface()->DestroyTextureID( m_iProductImageID );

	// Shutdown UI game data
	CUIGameData::Shutdown();
}

//=============================================================================
CBaseModPanel& CBaseModPanel::GetSingleton()
{
	Assert(m_CFactoryBasePanel != 0);
	return *m_CFactoryBasePanel;
}

//=============================================================================
CBaseModPanel* CBaseModPanel::GetSingletonPtr()
{
	return m_CFactoryBasePanel;
}

//=============================================================================
void CBaseModPanel::ReloadScheme()
{
}

//=============================================================================
CBaseModFrame* CBaseModPanel::OpenWindow(const WINDOW_TYPE & wt, CBaseModFrame * caller, bool hidePrevious, KeyValues *pParameters)
{
	CBaseModFrame *newNav = m_Frames[ wt ].Get();
	bool setActiveWindow = true;

	// Window priority is used to track which windows are visible at all times
	// it is used to resolve the situations when a game requests an error box to popup
	// while a loading progress bar is active.
	// Windows with a higher priority force all other windows to get hidden.
	// After the high-priority window goes away it falls back to restore the low priority windows.
	WINDOW_PRIORITY nWindowPriority = WPRI_NORMAL;

	switch ( wt )
	{
	case WT_PASSWORDENTRY:
		setActiveWindow = false;
		break;
	}

	switch ( wt )
	{
	case WT_GENERICWAITSCREEN:
		nWindowPriority = WPRI_WAITSCREEN;
		break;
	case WT_GENERICCONFIRMATION:
		nWindowPriority = WPRI_MESSAGE;
		break;
	case WT_LOADINGPROGRESSBKGND:
		nWindowPriority = WPRI_BKGNDSCREEN;
		break;
	case WT_LOADINGPROGRESS:
		nWindowPriority = WPRI_LOADINGPLAQUE;
		break;
	case WT_PASSWORDENTRY:
		nWindowPriority = WPRI_TOPMOST;
		break;
	case WT_TRANSITIONSCREEN:
		nWindowPriority = WPRI_TOPMOST;
		break;
	}

	if ( !newNav )
	{
		switch ( wt )
		{
		case WT_ACHIEVEMENTS:
			m_Frames[wt] = new Achievements(this, "Achievements");
			break;

		case WT_AUDIO:
			m_Frames[wt] = new Audio(this, "Audio");
			break;

		case WT_AUDIOVIDEO:
			m_Frames[wt] = new AudioVideo(this, "AudioVideo");
			break;

		case WT_CLOUD:
#if defined( _X360 )
			// not for xbox
			Assert( 0 );
			break;
#else
			m_Frames[wt] = new Cloud(this, "Cloud");
#endif
			break;

		case WT_CONTROLLER:
			m_Frames[wt] = new ControllerOptions(this, "ControllerOptions");
			break;

		case WT_CONTROLLER_STICKS:
			m_Frames[wt] = new ControllerOptionsSticks(this, "ControllerOptionsSticks");
			break;

		case WT_CONTROLLER_BUTTONS:
			m_Frames[wt] = new ControllerOptionsButtons(this, "ControllerOptionsButtons");
			break;

		case WT_DOWNLOADS:
#if defined( _X360 )
			// not for xbox
			Assert( 0 );
			break;
#else
			m_Frames[wt] = new Downloads(this, "Downloads");
#endif
			break;

		case WT_GAMELOBBY:
			m_Frames[wt] = new GameLobby(this, "GameLobby");
			break;

		case WT_GAMEOPTIONS:
			m_Frames[wt] = new GameOptions(this, "GameOptions");
			break;

		case WT_GAMESETTINGS:
			m_Frames[wt] = new GameSettings(this, "GameSettings");
			break;

		case WT_GENERICCONFIRMATION:
			m_Frames[wt] = new GenericConfirmation(this, "GenericConfirmation");
			break;

		case WT_INGAMEDIFFICULTYSELECT:
			m_Frames[wt] = new InGameDifficultySelect(this, "InGameDifficultySelect");
			break;

		case WT_INGAMEMAINMENU:
			m_Frames[wt] = new InGameMainMenu(this, "InGameMainMenu");
			break;

		case WT_INGAMECHAPTERSELECT:
			m_Frames[wt] = new InGameChapterSelect(this, "InGameChapterSelect");
			break;

		case WT_INGAMEKICKPLAYERLIST:
			m_Frames[wt] = new InGameKickPlayerList(this, "InGameKickPlayerList");
			break;

		case WT_VOTEOPTIONS:
			m_Frames[wt] = new VoteOptions(this, "VoteOptions");
			break;

		case WT_KEYBOARDMOUSE:
#if defined( _X360 )
			// not for xbox
			Assert( 0 );
			break;
#else
			m_Frames[wt] = new VKeyboard(this, "VKeyboard");
#endif
			break;

		case WT_LOADINGPROGRESSBKGND:
			m_Frames[wt] = new LoadingProgress( this, "LoadingProgress", LoadingProgress::LWT_BKGNDSCREEN );
			break;

		case WT_LOADINGPROGRESS:
			m_Frames[wt] = new LoadingProgress( this, "LoadingProgress", LoadingProgress::LWT_LOADINGPLAQUE );
			break;

		case WT_MAINMENU:
			m_Frames[wt] = new MainMenu(this, "MainMenu");
			break;

		case WT_MULTIPLAYER:
			m_Frames[wt] = new Multiplayer(this, "Multiplayer");
			break;

		case WT_OPTIONS:
			m_Frames[wt] = new Options(this, "Options");
			break;

		case WT_SIGNINDIALOG:
			m_Frames[wt] = new SignInDialog(this, "SignInDialog");
			break;

		case WT_GENERICWAITSCREEN:
			m_Frames[ wt ] = new GenericWaitScreen( this, "GenericWaitScreen" );
			break;

		case WT_PASSWORDENTRY:
			m_Frames[ wt ] = new PasswordEntry( this, "PasswordEntry" );
			break;

		case WT_ATTRACTSCREEN:
			m_Frames[ wt ] = new CAttractScreen( this, "AttractScreen" );
			break;

		case WT_ALLGAMESEARCHRESULTS:
			m_Frames[ wt ] = new FoundGames( this, "FoundGames" );
			break;

		case WT_FOUNDPUBLICGAMES:
			m_Frames[ wt ] = new FoundPublicGames( this, "FoundPublicGames" );
			break;

		case WT_TRANSITIONSCREEN:
			m_Frames[wt] = new CTransitionScreen( this, "TransitionScreen" );
			break;

		case WT_VIDEO:
			m_Frames[wt] = new Video(this, "Video");
			break;

		case WT_STEAMCLOUDCONFIRM:
#if defined( _X360 )
			// not for xbox
			Assert( 0 );
			break;
#else
			m_Frames[wt] = new SteamCloudConfirmation(this, "SteamCloudConfirmation");
#endif
			break;

		case WT_STEAMGROUPSERVERS:
			m_Frames[ wt ] = new FoundGroupGames( this, "FoundGames" );
			break;

		case WT_CUSTOMCAMPAIGNS:
#if defined( _X360 )
			// not for xbox
			Assert( 0 );
			break;
#else
			m_Frames[ wt ] = new CustomCampaigns( this, "CustomCampaigns" );
#endif
			break;

		case WT_DOWNLOADCAMPAIGN:
#if defined( _X360 )
			// not for xbox
			Assert( 0 );
			break;
#else
			m_Frames[ wt ] = new DownloadCampaign( this, "DownloadCampaign" );
#endif
			break;

		case WT_LEADERBOARD:
			m_Frames[ wt ] = new Leaderboard( this );
			break;

		case WT_ADDONS:
#if defined( _X360 )
			// not for xbox
			Assert( 0 );
			break;
#else
			m_Frames[wt] = new Addons( this, "Addons" );
#endif
			break;

		case WT_JUKEBOX:
#if defined( _X360 )
			// not for xbox
			Assert( 0 );
			break;
#else
			m_Frames[wt] = new VJukebox( this, "Jukebox" );
#endif
			break;

		case WT_ADDONASSOCIATION:
#if defined( _X360 )
			// not for xbox
			Assert( 0 );
			break;
#else
			m_Frames[wt] = new AddonAssociation( this, "AddonAssociation" );
#endif
			break;

		case WT_GETLEGACYDATA:
#if defined( _X360 )
			// not for xbox
			Assert( 0 );
			break;
#else
			m_Frames[wt] = new GetLegacyData( this, "GetLegacyData" );
#endif
			break;

		default:
			Assert( false );	// unknown window type
			break;
		}

		//
		// Finish setting up the window
		//

		newNav = m_Frames[wt].Get();
		if ( !newNav )
			return NULL;

		newNav->SetWindowPriority( nWindowPriority );
		newNav->SetWindowType(wt);
		newNav->SetVisible( false );
	}

	newNav->SetDataSettings( pParameters );

	if (setActiveWindow)
	{
		m_ActiveWindow[ nWindowPriority ] = wt;
		newNav->AddActionSignalTarget(this);
		newNav->SetCanBeActiveWindowType(true);
	}
	else if ( nWindowPriority == WPRI_MESSAGE )
	{
		m_ActiveWindow[ nWindowPriority ] = wt;
	}

	//
	// Now the window has been created, set it up
	//

	if ( UI_IsDebug() && (wt != WT_LOADINGPROGRESS) )
	{
		Msg( "[GAMEUI] OnOpen( `%s`, caller = `%s`, hidePrev = %d, setActive = %d, wt=%d, wpri=%d )\n",
			newNav->GetName(), caller ? caller->GetName() : "<NULL>", int(hidePrevious),
			int( setActiveWindow ), wt, nWindowPriority );
		KeyValuesDumpAsDevMsg( pParameters, 1 );
	}

	newNav->SetNavBack(caller);

	if (hidePrevious && caller != 0)
	{
		caller->SetVisible( false );
	}
	else if (caller != 0)
	{
		caller->FindAndSetActiveControl();
		//caller->SetAlpha(128);
	}

	// Check if a higher priority window is open
	if ( GetActiveWindowPriority() > newNav->GetWindowPriority() )
	{
		if ( UI_IsDebug() )
		{
			CBaseModFrame *pOther = m_Frames[ GetActiveWindowType() ].Get();
			Warning( "[GAMEUI] OpenWindow: Another window %p`%s` is having priority %d, deferring `%s`!\n",
				pOther, pOther ? pOther->GetName() : "<<null>>",
				GetActiveWindowPriority(), newNav->GetName() );
		}

		// There's a higher priority window that was open at the moment,
		// hide our window for now, it will get restored later.
		// newNav->SetVisible( false );
	}
	else
	{
		newNav->InvalidateLayout(false, false);
		newNav->OnOpen();
	}

	if ( UI_IsDebug() && (wt != WT_LOADINGPROGRESS) )
	{
		DbgShowCurrentUIState();
	}

	return newNav;
}

//=============================================================================
CBaseModFrame * CBaseModPanel::GetWindow( const WINDOW_TYPE& wt )
{
	return m_Frames[wt].Get();
}

//=============================================================================
WINDOW_TYPE CBaseModPanel::GetActiveWindowType()
{
	for ( int k = WPRI_COUNT; k -- > 0; )
	{
		if ( m_ActiveWindow[ k ] != WT_NONE )
		{
			CBaseModFrame *pFrame = m_Frames[ m_ActiveWindow[k] ].Get();
			if ( !pFrame || !pFrame->IsVisible() )
				continue;
			
			return m_ActiveWindow[ k ];
		}
	}
	return WT_NONE;
}

//=============================================================================
WINDOW_PRIORITY CBaseModPanel::GetActiveWindowPriority()
{
	for ( int k = WPRI_COUNT; k -- > 0; )
	{
		if ( m_ActiveWindow[ k ] != WT_NONE )
		{
			CBaseModFrame *pFrame = m_Frames[ m_ActiveWindow[k] ].Get();
			if ( !pFrame || !pFrame->IsVisible() )
				continue;

			return WINDOW_PRIORITY(k);
		}
	}
	return WPRI_NONE;
}

//=============================================================================
void CBaseModPanel::SetActiveWindow( CBaseModFrame * frame )
{
	if( !frame )
		return;
	
	m_ActiveWindow[ frame->GetWindowPriority() ] = frame->GetWindowType();

	if ( GetActiveWindowPriority() > frame->GetWindowPriority() )
	{
		if ( UI_IsDebug() )
		{
			CBaseModFrame *pOther = m_Frames[ GetActiveWindowType() ].Get();
			Warning( "[GAMEUI] SetActiveWindow: Another window %p`%s` is having priority %d, deferring `%s`!\n",
				pOther, pOther ? pOther->GetName() : "<<null>>",
				GetActiveWindowPriority(), frame->GetName() );
		}

		// frame->SetVisible( false );
	}
	else
	{
		frame->OnOpen();
	}
}

//=============================================================================
void CBaseModPanel::OnFrameClosed( WINDOW_PRIORITY pri, WINDOW_TYPE wt )
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CBaseModPanel::OnFrameClosed( %d, %d )\n", pri, wt );
		DbgShowCurrentUIState();
	}

	// Mark the frame that just closed as NULL so that nobody could find it
	m_Frames[wt] = NULL;

	if ( m_bClosingAllWindows )
	{
		if ( UI_IsDebug() )
		{
			Msg( "[GAMEUI] Closing all windows\n" );
		}
		return;
	}

	if( pri <= WPRI_NORMAL )
		return;

	for ( int k = 0; k < WPRI_COUNT; ++ k )
	{
		if ( m_ActiveWindow[k] == wt )
			m_ActiveWindow[k] = WT_NONE;
	}

	//
	// We only care to resurrect windows of lower priority when
	// higher priority windows close
	//

	for ( int k = WPRI_COUNT; k -- > 0; )
	{
		if ( m_ActiveWindow[ k ] == WT_NONE )
			continue;

		CBaseModFrame *pFrame = m_Frames[ m_ActiveWindow[k] ].Get();
		if ( !pFrame )
			continue;

		// pFrame->AddActionSignalTarget(this);

		pFrame->InvalidateLayout(false, false);
		pFrame->OnOpen();
		pFrame->SetVisible( true );
		pFrame->Activate();

		if ( UI_IsDebug() )
		{
			Msg( "[GAMEUI] CBaseModPanel::OnFrameClosed( %d, %d ) -> Activated `%s`, pri=%d\n",
				pri, wt, pFrame->GetName(), pFrame->GetWindowPriority() );
			DbgShowCurrentUIState();
		}

		return;
	}
}

void CBaseModPanel::DbgShowCurrentUIState()
{
	if ( UI_IsDebug() < 2 )
		return;

	Msg( "[GAMEUI] Priorities WT: " );
	for ( int i = 0; i < WPRI_COUNT; ++ i )
	{
		Msg( " %d ", m_ActiveWindow[i] );
	}
	Msg( "\n" );
	for ( int i = 0; i < WT_WINDOW_COUNT; ++ i )
	{
		CBaseModFrame *pFrame = m_Frames[i].Get();
		if ( pFrame )
		{
			Msg( "        %2d. `%s` pri%d vis%d\n",
				i, pFrame->GetName(), pFrame->GetWindowPriority(), pFrame->IsVisible() );
		}
		else
		{
			Msg( "        %2d. NULL\n", i );
		}
	}
}

bool CBaseModPanel::IsLevelLoading()
{
	return m_LevelLoading;
}

//=============================================================================
void CBaseModPanel::CloseAllWindows( int ePolicyFlags )
{
	CAutoPushPop< bool > auto_m_bClosingAllWindows( m_bClosingAllWindows, true );

	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CBaseModPanel::CloseAllWindows( 0x%x )\n", ePolicyFlags );
	}

	// make sure we also close any active flyout menus that might be hanging out.
	FlyoutMenu::CloseActiveMenu();

	for (int i = 0; i < WT_WINDOW_COUNT; ++i)
	{
		CBaseModFrame *pFrame = m_Frames[i].Get();
		if ( !pFrame )
			continue;

		int nPriority = pFrame->GetWindowPriority();

		switch ( nPriority )
		{
		case WPRI_LOADINGPLAQUE:
			if ( !(ePolicyFlags & CLOSE_POLICY_EVEN_LOADING) )
			{
				if ( UI_IsDebug() )
				{
					Msg( "[GAMEUI] CBaseModPanel::CloseAllWindows() - Keeping loading type %d of priority %d.\n", i, nPriority );
				}

				continue;
				m_ActiveWindow[ WPRI_LOADINGPLAQUE ] = WT_NONE;
			}
			break;

		case WPRI_MESSAGE:
			if ( !(ePolicyFlags & CLOSE_POLICY_EVEN_MSGS) )
			{
				if ( UI_IsDebug() )
				{
					Msg( "[GAMEUI] CBaseModPanel::CloseAllWindows() - Keeping msgbox type %d of priority %d.\n", i, nPriority );
				}

				continue;
				m_ActiveWindow[ WPRI_MESSAGE ] = WT_NONE;
			}
			break;

		case WPRI_BKGNDSCREEN:
			if ( ePolicyFlags & CLOSE_POLICY_KEEP_BKGND )
			{
				if ( UI_IsDebug() )
				{
					Msg( "[GAMEUI] CBaseModPanel::CloseAllWindows() - Keeping bkgnd type %d of priority %d.\n", i, nPriority );
				}

				continue;
				m_ActiveWindow[ WPRI_BKGNDSCREEN ] = WT_NONE;
			}
			break;
		}

		// Close the window
		pFrame->Close();
		m_Frames[i] = NULL;
	}

	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] After close all windows:\n" );
		DbgShowCurrentUIState();
	}

	m_ActiveWindow[ WPRI_NORMAL ] = WT_NONE;
}

#if defined( _X360 ) && defined( _DEMO )
void CBaseModPanel::OnDemoTimeout()
{
	if ( !engine->IsInGame() && !engine->IsConnected() && !engine->IsDrawingLoadingImage() )
	{
		// exit is terminal and unstoppable
		StartExitingProcess( false );
	}
	else
	{
		engine->ExecuteClientCmd( "disconnect" );
	}
}
#endif

bool CBaseModPanel::ActivateBackgroundEffects()
{
	// PC needs to keep start music, can't loop MP3's
	if ( IsPC() && !IsBackgroundMusicPlaying() )
	{
		StartBackgroundMusic( 1.0f );
	}

	return true;
}

//=============================================================================
void CBaseModPanel::OnGameUIActivated()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CBaseModPanel::OnGameUIActivated( delay = %d )\n", m_DelayActivation );
	}

	if ( m_DelayActivation )
	{
		return;
	}

	COM_TimestampedLog( "CBaseModPanel::OnGameUIActivated()" );

#if defined( _X360 )
	if ( !engine->IsInGame() && !engine->IsConnected() && !engine->IsDrawingLoadingImage() )
	{
#if defined( _DEMO )
		if ( engine->IsDemoExiting() )
		{
			// just got activated, maybe from a disconnect
			// exit is terminal and unstoppable
			SetVisible( true );
			StartExitingProcess( false );
			return;
		}
#endif
		if ( !GameUI().IsInLevel() && !GameUI().IsInBackgroundLevel() )
		{
			// not using a background map
			// start the menu movie and music now, as the main menu is about to open
			// these are very large i/o operations on the xbox
			// they must occur before the installer takes over the DVD
			// otherwise the transfer rate is so slow and we sync stall for 10-15 seconds
			ActivateBackgroundEffects();
		}
		// the installer runs in the background during the main menu
		g_pXboxInstaller->Start();

#if defined( _DEMO )
		// ui valid can now adhere to demo timeout rules
		engine->EnableDemoTimeout( true );
#endif
	}
#endif

	SetVisible( true );

	// This is terrible, why are we directing the window that we open when we are only trying to activate the UI?
	if ( WT_GAMELOBBY == GetActiveWindowType() )
	{
		return;
	}
	else if ( !IsX360() && WT_LOADINGPROGRESS == GetActiveWindowType() )
	{
		// Ignore UI activations when loading poster is up
		return;
	}
	else if ( ( !m_LevelLoading && !engine->IsConnected() ) || GameUI().IsInBackgroundLevel() )
	{
		bool bForceReturnToFrontScreen = false;
		WINDOW_TYPE wt = GetActiveWindowType();
		switch ( wt )
		{
		default:
			break;
		case WT_NONE:
		case WT_INGAMEMAINMENU:
		case WT_GENERICCONFIRMATION:
			// bForceReturnToFrontScreen = !g_pMatchFramework->GetMatchmaking()->ShouldPreventOpenFrontScreen();
			bForceReturnToFrontScreen = true; // this used to be some magic about mid-disconnecting-states on PC...
			break;
		}
		if ( !IsPC() || bForceReturnToFrontScreen )
		{
			OpenFrontScreen();
		}
	}
	else if ( engine->IsConnected() && !m_LevelLoading )
	{
		CBaseModFrame *pInGameMainMenu = m_Frames[ WT_INGAMEMAINMENU ].Get();

		if ( !pInGameMainMenu || !pInGameMainMenu->IsAutoDeleteSet() )
		{
			// Prevent in game menu from opening if it already exists!
			// It might be hiding behind a modal window that needs to keep focus
			OpenWindow( WT_INGAMEMAINMENU, 0 );
		}
	}
}

//=============================================================================
void CBaseModPanel::OnGameUIHidden()
{
	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CBaseModPanel::OnGameUIHidden()\n" );
	}

#if defined( _X360 )
	// signal the installer to stop
	g_pXboxInstaller->Stop();
#endif

// 	// We want to check here if we have any pending message boxes and
// 	// if so, then we cannot just simply destroy all the UI elements
// 	for ( int k = WPRI_NORMAL + 1; k < WPRI_LOADINGPLAQUE; ++ k )
// 	{
// 		WINDOW_TYPE wt = m_ActiveWindow[k];
// 		if ( wt != WT_NONE )
// 		{
// 			Msg( "[GAMEUI] CBaseModPanel::OnGameUIHidden() - not destroying UI because of wt %d pri %d\n",
// 				wt, k );
// 			return;
// 		}
// 	}

	SetVisible(false);
	
	// Notify the options dialog that game UI is closing
	if ( m_hOptionsDialog.Get() )
	{
		PostMessage( m_hOptionsDialog.Get(), new KeyValues( "GameUIHidden" ) );
	}

	// Notify the in game menu that game UI is closing
	CBaseModFrame *pInGameMainMenu = GetWindow( WT_INGAMEMAINMENU );
	if ( pInGameMainMenu )
	{
		PostMessage( pInGameMainMenu, new KeyValues( "GameUIHidden" ) );
	}

	// Close achievements
	if ( CBaseModFrame *pFrame = GetWindow( WT_ACHIEVEMENTS ) )
	{
		pFrame->Close();
	}
}

void CBaseModPanel::OpenFrontScreen()
{
	WINDOW_TYPE frontWindow = WT_NONE;
#ifdef _X360
	// make sure we are in the startup menu.
	if ( !GameUI().IsInBackgroundLevel() )
	{
		engine->ClientCmd( "startupmenu" );
	}

	if ( g_pMatchFramework->GetMatchSession() )
	{
		Warning( "CBaseModPanel::OpenFrontScreen during active game ignored!\n" );
		return;
	}

	if( XBX_GetNumGameUsers() > 0 )
	{
		if ( CBaseModFrame *pAttractScreen = GetWindow( WT_ATTRACTSCREEN ) )
		{
			frontWindow = WT_ATTRACTSCREEN;
		}
		else
		{
			frontWindow = WT_MAINMENU;
		}
	}
	else
	{
		frontWindow = WT_ATTRACTSCREEN;
	}
#else
	frontWindow = WT_MAINMENU;
#endif // _X360

	if( frontWindow != WT_NONE )
	{
		if( GetActiveWindowType() != frontWindow )
		{
			CloseAllWindows();
			OpenWindow( frontWindow, NULL );
		}
	}
}

//=============================================================================
void CBaseModPanel::RunFrame()
{
	if ( s_NavLock > 0 )
	{
		--s_NavLock;
	}

	GetAnimationController()->UpdateAnimations( Plat_FloatTime() );

	CBaseModFrame::RunFrameOnListeners();

	CUIGameData::Get()->RunFrame();

	if ( m_DelayActivation )
	{
		m_DelayActivation--;
		if ( !m_LevelLoading && !m_DelayActivation )
		{
			if ( UI_IsDebug() )
			{
				Msg( "[GAMEUI] Executing delayed UI activation\n");
			}
			OnGameUIActivated();
		}
	}

	bool bDoBlur = true;
	WINDOW_TYPE wt = GetActiveWindowType();
	switch ( wt )
	{
	case WT_NONE:
	case WT_MAINMENU:
	case WT_LOADINGPROGRESSBKGND:
	case WT_LOADINGPROGRESS:
	case WT_AUDIOVIDEO:
		bDoBlur = false;
		break;
	}
	if ( GetWindow( WT_ATTRACTSCREEN ) || ( enginevguifuncs && !enginevguifuncs->IsGameUIVisible() ) )
	{
		// attract screen might be open, but not topmost due to notification dialogs
		bDoBlur = false;
	}

	if ( !bDoBlur )
	{
		bDoBlur = GameClientExports()->ClientWantsBlurEffect();
	}

	float nowTime = Plat_FloatTime();
	float deltaTime = nowTime - m_flLastBlurTime;
	if ( deltaTime > 0 )
	{
		m_flLastBlurTime = nowTime;
		m_flBlurScale += deltaTime * bDoBlur ? 0.05f : -0.05f;
		m_flBlurScale = clamp( m_flBlurScale, 0, 0.85f );
		engine->SetBlurFade( m_flBlurScale );
	}

	if ( IsX360() && m_ExitingFrameCount )
	{
		CTransitionScreen *pTransitionScreen = static_cast< CTransitionScreen* >( GetWindow( WT_TRANSITIONSCREEN ) );
		if ( pTransitionScreen && pTransitionScreen->IsTransitionComplete() )
		{
			if ( m_ExitingFrameCount > 1 )
			{
				m_ExitingFrameCount--;
				if ( m_ExitingFrameCount == 1 )
				{
					// enough frames have transpired, send the single shot quit command
					if ( m_bWarmRestartMode )
					{
						// restarts self, skips any intros
						engine->ClientCmd_Unrestricted( "quit_x360 restart\n" );
					}
					else
					{
						// cold restart, quits to any startup app
						engine->ClientCmd_Unrestricted( "quit_x360\n" );
					}
				}
			}
		}
	}
}


//=============================================================================
void CBaseModPanel::OnLevelLoadingStarted( char const *levelName, bool bShowProgressDialog )
{
	Assert( !m_LevelLoading );

#if defined( _X360 )
	// stop the installer
	g_pXboxInstaller->Stop();
	g_pXboxInstaller->SpewStatus();

	// If the installer has finished while we are in the menus, then this is the ONLY place we
	// know that there is no open files and we can redirect the search paths
	if ( g_pXboxInstaller->ForceCachePaths() )
	{
		// the search paths got changed
		// notify other systems who may have hooked absolute paths
		engine->SearchPathsChangedAfterInstall();
	}
#endif

	CloseAllWindows();

	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] OnLevelLoadingStarted - opening loading progress (%s)...\n",
			levelName ? levelName : "<< no level specified >>" );
	}

	LoadingProgress *pLoadingProgress = static_cast<LoadingProgress*>( OpenWindow( WT_LOADINGPROGRESS, 0 ) );

	KeyValues *pMissionInfo = NULL;
	KeyValues *pChapterInfo = NULL;
	
	bool bShowPoster = false;
	char chGameMode[64] = {0};

	//
	// If playing on listen server then "levelName" is set to the map being loaded,
	// so it is authoritative - it might be a background map or a real level.
	//
	if ( levelName )
	{
		// Derive the mission info from the server game details
		KeyValues *pGameSettings = g_pMatchFramework->GetMatchNetworkMsgController()->GetActiveServerGameDetails( NULL );
		if ( !pGameSettings )
		{
			// In this particular case we need to speculate about game details
			// this happens when user types "map c5m2 versus easy" from console, so there's no
			// active server spawned yet, nor is the local client connected to any server.
			// We have to force server DLL to apply the map command line to the settings and then
			// speculatively construct the settings key.
			if ( IServerGameDLL *pServerDLL = ( IServerGameDLL * ) g_pMatchFramework->GetMatchExtensions()->GetRegisteredExtensionInterface( INTERFACEVERSION_SERVERGAMEDLL ) )
			{
				KeyValues *pApplyServerSettings = new KeyValues( "::ExecGameTypeCfg" );
				KeyValues::AutoDelete autodelete_pApplyServerSettings( pApplyServerSettings );

				pApplyServerSettings->SetString( "map/mapname", levelName );

				pServerDLL->ApplyGameSettings( pApplyServerSettings );
			}

			static ConVarRef r_mp_gamemode( "mp_gamemode" );
			if ( r_mp_gamemode.IsValid() )
			{
				pGameSettings = new KeyValues( "CmdLineSettings" );
				pGameSettings->SetString( "game/mode", r_mp_gamemode.GetString() );
			}
		}
		
		KeyValues::AutoDelete autodelete_pGameSettings( pGameSettings );
		if ( pGameSettings )
		{
			// It is critical to get map info by the actual levelname that is being loaded, because
			// for level transitions the server is still in the old map and the game settings returned
			// will reflect the old state of the server.
			pChapterInfo = g_pMatchExtSwarm->GetMapInfoByBspName( pGameSettings, levelName, &pMissionInfo );
			Q_strncpy( chGameMode, pGameSettings->GetString( "game/mode", "" ), ARRAYSIZE( chGameMode ) );
		}
	}
	
	IMatchSession *pSession = g_pMatchFramework->GetMatchSession();
	if ( !pChapterInfo && pSession  )
	{
		if ( KeyValues *pSettings = pSession->GetSessionSettings() )
		{
			pChapterInfo = g_pMatchExtSwarm->GetMapInfo( pSettings, &pMissionInfo );
			Q_strncpy( chGameMode, pSettings->GetString( "game/mode", "" ), ARRAYSIZE( chGameMode ) );
		}
	}

	//
	// If we are just loading into some unknown map, then fake chapter information
	// (static lifetime of fake keyvalues so that we didn't worry about ownership)
	//
	if ( !pMissionInfo )
	{
		static KeyValues *s_pFakeMissionInfo = new KeyValues( "" );
		pMissionInfo = s_pFakeMissionInfo;
		pMissionInfo->SetString( "displaytitle", "#L4D360UI_Lobby_Unknown_Campaign" );
	}
	if ( !pChapterInfo )
	{
		static KeyValues *s_pFakeChapterInfo = new KeyValues( "1" );
		pChapterInfo = s_pFakeChapterInfo;
		pChapterInfo->SetString( "displayname", levelName ? levelName : "#L4D360UI_Lobby_Unknown_Campaign" );
		pChapterInfo->SetString( "map", levelName ? levelName : "" );
	}
	
	//
	// If we are transitioning maps from a real level then we don't want poster.
	// We always want the poster when loading the first chapter of a campaign (vote for restart)
	//
	bShowPoster = true; //( !GameUI().IsInLevel() ||
					//GameModeIsSingleChapter( chGameMode ) ||
					//( pChapterInfo && pChapterInfo->GetInt( "chapter" ) == 1 ) ) &&
		//pLoadingProgress->ShouldShowPosterForLevel( pMissionInfo, pChapterInfo );

	LoadingProgress::LoadingType type;
	if ( bShowPoster )
	{
		type = LoadingProgress::LT_POSTER;

		// These names match the order of the enum Avatar_t in imatchmaking.h

		const char *pPlayerNames[NUM_LOADING_CHARACTERS] = { NULL, NULL, NULL, NULL };
		const char *pAvatarNames[NUM_LOADING_CHARACTERS] = { "", "", "", "" };

		unsigned char botFlags = 0xFF;

		if ( IMatchSession *pSession = g_pMatchFramework->GetMatchSession() )
		{
			KeyValues *pSettings = pSession->GetSessionSettings();
			if ( pSettings )
				pSettings = pSettings->FindKey( "members" );

			int numMachines = pSettings->GetInt( "numMachines", 0 );
			for ( int iMachine = 0; iMachine < numMachines; ++ iMachine )
			{
				char chMachine[32];
				sprintf( chMachine, "machine%d", iMachine );
				KeyValues *pMachine = pSettings->FindKey( chMachine );

				int numPlayers = pMachine->GetInt( "numPlayers", 0 );
				for ( int iPlayer = 0; iPlayer < numPlayers; ++ iPlayer )
				{
					char chPlayer[32];
					sprintf( chPlayer, "player%d", iPlayer );
					KeyValues *pPlayer = pMachine->FindKey( chPlayer );

					XUID xuidPlayer = pPlayer->GetUint64( "xuid", 0ull );
					char const *szPlayerName = pPlayer->GetString( "name", "" );
					szPlayerName = CUIGameData::Get()->GetPlayerName( xuidPlayer, szPlayerName );
					char const *szAvatar = pPlayer->GetString( "game/avatar", "" );

					// Find the avatar
					int iAvatar;
					for ( iAvatar = 0; iAvatar < ARRAYSIZE( pAvatarNames ); ++ iAvatar )
					{
						if ( !Q_stricmp( pAvatarNames[iAvatar], szAvatar ) )
							break;
					}
					if ( iAvatar < ARRAYSIZE( pPlayerNames ) )
					{
						pPlayerNames[ iAvatar ] = szPlayerName;
						botFlags &= ~(1 << iAvatar);
					}
				}
			}
		}
		pLoadingProgress->SetPosterData( pMissionInfo, pChapterInfo, pPlayerNames, botFlags, chGameMode );
	}
	else if ( GameUI().IsInLevel() && !GameUI().IsInBackgroundLevel() )
	{
		// Transitions between levels 
		type = LoadingProgress::LT_TRANSITION;
	}
	else
	{
		// Loading the menu the first time
		type = LoadingProgress::LT_MAINMENU;
	}

	pLoadingProgress->SetLoadingType( type );
	pLoadingProgress->SetProgress( 0.0f );

	m_LevelLoading = true;
}

void CBaseModPanel::OnEngineLevelLoadingSession( KeyValues *pEvent )
{
	// We must keep the default loading poster because it will be replaced by
	// the real campaign loading poster shortly
	float flProgress = 0.0f;
	if ( LoadingProgress *pLoadingProgress = static_cast<LoadingProgress*>( GetWindow( WT_LOADINGPROGRESS ) ) )
	{
		flProgress = pLoadingProgress->GetProgress();
		pLoadingProgress->Close();
		m_Frames[ WT_LOADINGPROGRESS ] = NULL;
	}
	CloseAllWindows( CLOSE_POLICY_DEFAULT );

	// Pop up a fake bkgnd poster
	if ( LoadingProgress *pLoadingProgress = static_cast<LoadingProgress*>( OpenWindow( WT_LOADINGPROGRESSBKGND, NULL ) ) )
	{
		pLoadingProgress->SetLoadingType( LoadingProgress::LT_POSTER );
		pLoadingProgress->SetProgress( flProgress );
	}
}

//=============================================================================
void CBaseModPanel::OnLevelLoadingFinished( KeyValues *kvEvent )
{
	int bError = kvEvent->GetInt( "error" );
	const char *failureReason = kvEvent->GetString( "reason" );
	
	Assert( m_LevelLoading );

	if ( UI_IsDebug() )
	{
		Msg( "[GAMEUI] CBaseModPanel::OnLevelLoadingFinished( %s, %s )\n", bError ? "Had Error" : "No Error", failureReason );
	}

#if defined( _X360 )
	if ( GameUI().IsInBackgroundLevel() )
	{
		// start the installer when running the background map has finished
		g_pXboxInstaller->Start();
	}
#endif

	LoadingProgress *pLoadingProgress = static_cast<LoadingProgress*>( GetWindow( WT_LOADINGPROGRESS ) );
	if ( pLoadingProgress )
	{
		pLoadingProgress->SetProgress( 1.0f );

		// always close loading progress, this frees costly resources
		pLoadingProgress->Close();
	}

	m_LevelLoading = false;

	CBaseModFrame *pFrame = CBaseModPanel::GetSingleton().GetWindow( WT_GENERICCONFIRMATION );
	if ( !pFrame )
	{
		// no confirmation up, hide the UI
		GameUI().HideGameUI();
	}

	// if we are loading into the lobby, then skip the UIActivation code path
	// this can happen if we accepted an invite to player who is in the lobby while we were in-game
	if ( WT_GAMELOBBY != GetActiveWindowType() )
	{
		// if we are loading into the front-end, then activate the main menu (or attract screen, depending on state)
		// or if a message box is pending force open game ui
		if ( GameUI().IsInBackgroundLevel() || pFrame )
		{
			GameUI().OnGameUIActivated();
		}
	}

	if ( bError )
	{
		GenericConfirmation* pMsg = ( GenericConfirmation* ) OpenWindow( WT_GENERICCONFIRMATION, NULL, false );		
		if ( pMsg )
		{
			GenericConfirmation::Data_t data;
			data.pWindowTitle = "#L4D360UI_MsgBx_DisconnectedFromServer";			
			data.bOkButtonEnabled = true;
			data.pMessageText = failureReason;
			pMsg->SetUsageData( data );
		}		
	}
}

class CMatchSessionCreationAsyncOperation : public IMatchAsyncOperation
{
public:
	CMatchSessionCreationAsyncOperation() : m_eState( AOS_RUNNING ) {}

public:
	virtual bool IsFinished() { return false; }
	virtual AsyncOperationState_t GetState() { return m_eState; }
	virtual uint64 GetResult() { return 0ull; }
	virtual void Abort();
	virtual void Release() { Assert( 0 ); } // we are a global object, cannot release

public:
	IMatchAsyncOperation * Prepare() { m_eState = AOS_RUNNING; return this; }

protected:
	AsyncOperationState_t m_eState;
}
g_MatchSessionCreationAsyncOperation;

void CMatchSessionCreationAsyncOperation::Abort()
{
	m_eState = AOS_ABORTING;
	
	Assert( g_pMatchFramework->GetMatchSession() );
	g_pMatchFramework->CloseSession();

	CBaseModPanel::GetSingleton().CloseAllWindows();
	CBaseModPanel::GetSingleton().OpenFrontScreen();
}

void CBaseModPanel::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !Q_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{
		char const *szState = pEvent->GetString( "state", "" );
		if ( !Q_stricmp( "ready", szState ) )
		{
			// Session has finished creating:
			IMatchSession *pSession = g_pMatchFramework->GetMatchSession();
			if ( !pSession )
				return;

			KeyValues *pSettings = pSession->GetSessionSettings();
			if ( !pSettings )
				return;

			char const *szNetwork = pSettings->GetString( "system/network", "" );
			int numLocalPlayers = pSettings->GetInt( "members/numPlayers", 1 );
			
			WINDOW_TYPE wtGameLobby = WT_GAMELOBBY;
			if ( !Q_stricmp( "offline", szNetwork ) &&
				 numLocalPlayers <= 1 )
			{
				// We have a single-player offline session
				wtGameLobby = WT_GAMESETTINGS;
			}
			else
			{
				// Automatically start the map, no configuration required
				pSession->Command( KeyValues::AutoDeleteInline( new KeyValues( "Start" ) ) );
				return;
			}

			// We have created a session
			CloseAllWindows();

			// Special case when we are creating a public session after empty search
			if ( !Q_stricmp( pSettings->GetString( "options/createreason" ), "searchempty" ) &&
				 !Q_stricmp( pSettings->GetString( "system/access" ), "public" ) )
			{
				// We are creating a public lobby after our search turned out empty
				char const *szWaitScreenText = "#Matchmaking_NoResultsCreating";
				CUIGameData::Get()->OpenWaitScreen( szWaitScreenText, ui_lobby_noresults_create_msg_time.GetFloat() );
				CUIGameData::Get()->CloseWaitScreen( NULL, NULL );

				// Delete the "createreason" key from the session settings
				pSession->UpdateSessionSettings( KeyValues::AutoDeleteInline( KeyValues::FromString( "delete",
					" delete { options { createreason delete } } " ) ) );
			}

			CBaseModFrame *pLobbyWindow = OpenWindow( wtGameLobby, NULL, true, pSettings ); // derive from session
			if ( CBaseModFrame *pWaitScreen = GetWindow( WT_GENERICWAITSCREEN ) )
			{
				// Normally "CloseAllWindows" above would take down the waitscreen, but
				// we could pop it up for the special case of empty search results
				pWaitScreen->SetNavBack( pLobbyWindow );
			}

			// Check for a special case when we lost connection to host and that's why we are going to lobby
			if ( KeyValues *pOnEngineDisconnectReason = g_pMatchFramework->GetEventsSubscription()->GetEventData( "OnEngineDisconnectReason" ) )
			{
				if ( !Q_stricmp( "lobby", pOnEngineDisconnectReason->GetString( "disconnecthdlr" ) ) )
				{
					CUIGameData::Get()->OpenWaitScreen( "#L4D360UI_MsgBx_DisconnectedFromServer" );
					CUIGameData::Get()->CloseWaitScreen( NULL, NULL );
				}
			}
		}
		else if ( !Q_stricmp( "created", szState ) )
		{
			//
			// This section of code catches when we just connected to a lobby that
			// is playing a campaign that we do not have installed.
			// In this case we abort loading, forcefully close all windows including
			// loading poster and game lobby and display the download info msg.
			//

			IMatchSession *pSession = g_pMatchFramework->GetMatchSession();
			if ( !pSession )
				return;

			KeyValues *pSettings = pSession->GetSessionSettings();

			KeyValues *pInfoMission = NULL;
			// TODO:
			KeyValues *pInfoChapter = NULL;//GetMapInfoRespectingAnyChapter( pSettings, &pInfoMission );

			bool bValidMission = true;

			// TODO: Check if we have the map installed by querying local mission chooser source

			if ( bValidMission )
				return;

			// If we do not have a valid chapter/mission, then we need to quit
			if ( pInfoChapter && pInfoMission &&
				( !*pInfoMission->GetName() || pInfoMission->GetInt( "version" ) == pSettings->GetInt( "game/missioninfo/version", -1 ) ) )
				return;

			if ( pSettings )
				pSettings = pSettings->MakeCopy();

			engine->ExecuteClientCmd( "disconnect" );
			g_pMatchFramework->CloseSession();

			CloseAllWindows( CLOSE_POLICY_EVEN_MSGS | CLOSE_POLICY_EVEN_LOADING );
			OpenFrontScreen();

			const char *szCampaignWebsite = pSettings->GetString( "game/missioninfo/website", NULL );
			if ( szCampaignWebsite && *szCampaignWebsite )
			{
				OpenWindow( WT_DOWNLOADCAMPAIGN,
					GetWindow( CBaseModPanel::GetSingleton().GetActiveWindowType() ),
					true, pSettings );
			}
			else
			{
				GenericConfirmation::Data_t data;

				data.pWindowTitle = "#L4D360UI_Lobby_MissingContent";
				data.pMessageText = "#L4D360UI_Lobby_MissingContent_Message";
				data.bOkButtonEnabled = true;

				GenericConfirmation* confirmation = 
					static_cast< GenericConfirmation* >( OpenWindow( WT_GENERICCONFIRMATION, NULL, true ) );

				confirmation->SetUsageData(data);
			}
		}
		else if ( !Q_stricmp( "progress", szState ) )
		{
			struct WaitText_t
			{
				char const *m_szProgress;
				char const *m_szText;
				int m_eCloseAllWindowsFlags;
			};

			int eDefaultFlags = CLOSE_POLICY_EVEN_MSGS | CLOSE_POLICY_KEEP_BKGND;
			WaitText_t arrWaits[] = {
				{ "creating",	"#Matchmaking_creating",	eDefaultFlags },
				{ "joining",	"#Matchmaking_joining",		eDefaultFlags },
				{ "searching",	"#Matchmaking_searching",	eDefaultFlags },
			};

			char const *szProgress = pEvent->GetString( "progress", "" );
			WaitText_t const *pWaitText = NULL;
			for ( int k = 0; k < ARRAYSIZE( arrWaits ); ++ k )
			{
				if ( !Q_stricmp( arrWaits[k].m_szProgress, szProgress ) )
				{
					pWaitText = &arrWaits[k];
					break;
				}
			}

			// Wait screen options to cancel async process
			KeyValues *pSettings = new KeyValues( "WaitScreen" );
			KeyValues::AutoDelete autodelete_pSettings( pSettings );
			pSettings->SetPtr( "options/asyncoperation", g_MatchSessionCreationAsyncOperation.Prepare() );

			// For PC we don't want to cancel lobby creation
			if ( IsPC() && !Q_stricmp( "creating", szProgress ) )
				pSettings = NULL;

			// Put up a wait screen
			if ( pWaitText )
			{
				if ( pWaitText->m_eCloseAllWindowsFlags != -1 )
					CloseAllWindows( pWaitText->m_eCloseAllWindowsFlags );

				char const *szWaitScreenText = pWaitText->m_szText;
				float flMinDisplayTime = 0.0f;
				
				if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
				{
					KeyValues *pMatchSettings = pMatchSession->GetSessionSettings();
					if ( !Q_stricmp( szProgress, "creating" ) &&
						 !Q_stricmp( pMatchSettings->GetString( "options/createreason" ), "searchempty" ) &&
						 !Q_stricmp( pMatchSettings->GetString( "system/access" ), "public" ) )
					{
						// We are creating a public lobby after our search turned out empty
						szWaitScreenText = "#Matchmaking_NoResultsCreating";
					}
				}

				CUIGameData::Get()->OpenWaitScreen( szWaitScreenText, flMinDisplayTime, pSettings, 7.0f );
			}
			else if ( !Q_stricmp( "searchresult", szProgress ) )
			{
				char const *arrText[] = { "#Matchmaking_SearchResults",
					"#Matchmaking_SearchResults1", "#Matchmaking_SearchResults2", "#Matchmaking_SearchResults3" };
				int numResults = pEvent->GetInt( "numResults", 0 );
				if ( numResults < 0 || numResults >= ARRAYSIZE( arrText ) )
					numResults = 0;
				CUIGameData::Get()->OpenWaitScreen( arrText[numResults], 0.0f, pSettings );
			}
		}
	}
	else if ( !Q_stricmp( "OnEngineLevelLoadingSession", szEvent ) )
	{
		OnEngineLevelLoadingSession( pEvent );
	}
	else if ( !Q_stricmp( "OnEngineLevelLoadingFinished", szEvent ) )
	{
		OnLevelLoadingFinished( pEvent );
	}
}

//=============================================================================
bool CBaseModPanel::UpdateProgressBar( float progress, const char *statusText )
{
	if ( !m_LevelLoading )
	{
		// Assert( m_LevelLoading );
		// Warning( "WARN: CBaseModPanel::UpdateProgressBar called outside of level loading, discarded!\n" );
		return false;
	}

	LoadingProgress *loadingProgress = static_cast<LoadingProgress*>( OpenWindow( WT_LOADINGPROGRESS, 0 ) );

	// Even if the progress hasn't advanced, we want to go ahead and refresh if it has been more than 1/10 seconds since last refresh to keep the spinny thing going.
	static float s_LastEngineTime = -1.0f;
	// clock the anim at 10hz
	float time = Plat_FloatTime();
	float deltaTime = time - s_LastEngineTime;

	if ( loadingProgress && ( ( loadingProgress->IsDrawingProgressBar() && ( loadingProgress->GetProgress() < progress ) ) || ( deltaTime > 0.06f ) ) )
	{
		// update progress
		loadingProgress->SetProgress( progress );
		s_LastEngineTime = time;
		return true;
	}

	// no update required
	return false;
}

void CBaseModPanel::SetHelpText( const char* helpText )
{
	if ( m_FooterPanel )
	{
		m_FooterPanel->SetHelpText( helpText );
	}
}

void CBaseModPanel::SetOkButtonEnabled( bool bEnabled )
{
	if ( m_FooterPanel )
	{
		CBaseModFooterPanel::FooterButtons_t buttons = m_FooterPanel->GetButtons();
		if ( bEnabled )
			buttons |= FB_ABUTTON;
		else
			buttons &= ~FB_ABUTTON;
		m_FooterPanel->SetButtons( buttons, m_FooterPanel->GetFormat(), m_FooterPanel->GetHelpTextEnabled() );
	}
}

void CBaseModPanel::SetCancelButtonEnabled( bool bEnabled )
{
	if ( m_FooterPanel )
	{
		CBaseModFooterPanel::FooterButtons_t buttons = m_FooterPanel->GetButtons();
		if ( bEnabled )
			buttons |= FB_BBUTTON;
		else
			buttons &= ~FB_BBUTTON;
		m_FooterPanel->SetButtons( buttons, m_FooterPanel->GetFormat(), m_FooterPanel->GetHelpTextEnabled() );
	}
}

BaseModUI::CBaseModFooterPanel* CBaseModPanel::GetFooterPanel()
{
	// EVIL HACK
	if ( !this )
	{
		Assert( 0 );
		Warning( "CBaseModPanel::GetFooterPanel() called on NULL CBaseModPanel!!!\n" );
		return NULL;
	}
	return m_FooterPanel;
}

void CBaseModPanel::SetLastActiveUserId( int userId )
{
	if ( m_lastActiveUserId != userId )
	{
		DevWarning( "SetLastActiveUserId: %d -> %d\n", m_lastActiveUserId, userId );
	}

	m_lastActiveUserId = userId;
}

int CBaseModPanel::GetLastActiveUserId( )
{
	return m_lastActiveUserId;
}

//-----------------------------------------------------------------------------
// Purpose: moves the game menu button to the right place on the taskbar
//-----------------------------------------------------------------------------
static void BaseUI_PositionDialog(vgui::PHandle dlg)
{
	if (!dlg.Get())
		return;

	int x, y, ww, wt, wide, tall;
	vgui::surface()->GetWorkspaceBounds( x, y, ww, wt );
	dlg->GetSize(wide, tall);

	// Center it, keeping requested size
	dlg->SetPos(x + ((ww - wide) / 2), y + ((wt - tall) / 2));
}


//=============================================================================
void CBaseModPanel::OpenOptionsDialog( Panel *parent )
{
	if ( IsPC() )
	{			
		if ( !m_hOptionsDialog.Get() )
		{
			m_hOptionsDialog = new COptionsDialog( parent );
			BaseUI_PositionDialog( m_hOptionsDialog );
		}

		m_hOptionsDialog->Activate();
	}
}

//=============================================================================
void CBaseModPanel::OpenKeyBindingsDialog( Panel *parent )
{
	if ( IsPC() )
	{			
		if ( !m_hOptionsDialog.Get() )
		{
			m_hOptionsDialog = new COptionsDialog( parent, OPTIONS_DIALOG_ONLY_BINDING_TABS );
			BaseUI_PositionDialog( m_hOptionsDialog );
		}

		m_hOptionsDialog->Activate();
	}
}

//=============================================================================
void CBaseModPanel::OnNavigateTo( const char* panelName )
{
	CBaseModFrame* currentFrame = 
		static_cast<CBaseModFrame*>(FindChildByName(panelName, false));

	if (currentFrame && currentFrame->GetCanBeActiveWindowType())
	{
		m_ActiveWindow[ currentFrame->GetWindowPriority() ] = currentFrame->GetWindowType();
	}
}

//=============================================================================
void CBaseModPanel::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetBgColor(pScheme->GetColor("Blank", Color(0, 0, 0, 0)));

	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );

	char filename[MAX_PATH];
	V_snprintf( filename, sizeof( filename ), "VGUI/swarm/loading/BGFX01" ); // TODO: engine->GetStartupImage( filename, sizeof( filename ), screenWide, screenTall );
	m_iBackgroundImageID = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( m_iBackgroundImageID, filename, true, false );

	m_iProductImageID = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( m_iProductImageID, "console/startup_loading", true, false );

	// need these to be anchored now, can't come into existence during load
	PrecacheLoadingTipIcons();

	int logoW = 384;
	int logoH = 192;

	bool bIsWidescreen;
#if !defined( _X360 )
	float aspectRatio = (float)screenWide/(float)screenTall;
	bIsWidescreen = aspectRatio >= 1.5999f;
#else
	static ConVarRef mat_xbox_iswidescreen( "mat_xbox_iswidescreen" );
	bIsWidescreen = mat_xbox_iswidescreen.GetBool();
#endif
	if ( !bIsWidescreen )
	{
		// smaller in standard res
		logoW = 320;
		logoH = 160;
	}

	m_nProductImageWide = vgui::scheme()->GetProportionalScaledValue( logoW );
	m_nProductImageTall = vgui::scheme()->GetProportionalScaledValue( logoH );

	if ( aspectRatio >= 1.6f )
	{
		// use the widescreen version
		Q_snprintf( m_szFadeFilename, sizeof( m_szFadeFilename ), "materials/console/%s_widescreen.vtf", "SwarmSelectionScreen" );
	}
	else
	{
		Q_snprintf( m_szFadeFilename, sizeof( m_szFadeFilename ), "materials/console/%s_widescreen.vtf", "SwarmSelectionScreen" );
	}

	// TODO: GetBackgroundMusic
#if 0

	bool bUseMono = false;
#if defined( _X360 )
	// cannot use the very large stereo version during the install
	 bUseMono = g_pXboxInstaller->IsInstallEnabled() && !g_pXboxInstaller->IsFullyInstalled();
#if defined( _DEMO )
	bUseMono = true;
#endif
#endif

	char backgroundMusic[MAX_PATH];
	engine->GetBackgroundMusic( backgroundMusic, sizeof( backgroundMusic ), bUseMono );

	// the precache will be a memory or stream wave as needed 
	// on 360 the sound system will detect the install state and force it to a memory wave to finalize the the i/o now
	// it will be a stream resource if the installer is dormant
	// On PC it will be a streaming MP3
	if ( enginesound->PrecacheSound( backgroundMusic, true, false ) )
	{
		// successfully precached
		m_backgroundMusic = backgroundMusic;
	}
#endif
}

void CBaseModPanel::DrawColoredText( vgui::HFont hFont, int x, int y, unsigned int color, const char *pAnsiText )
{
	wchar_t szconverted[256];
	int len = g_pVGuiLocalize->ConvertANSIToUnicode( pAnsiText, szconverted, sizeof( szconverted ) );
	if ( len <= 0 )
	{
		return;
	}

	int r = ( color >> 24 ) & 0xFF;
	int g = ( color >> 16 ) & 0xFF;
	int b = ( color >> 8 ) & 0xFF;
	int a = ( color >> 0 ) & 0xFF;

	vgui::surface()->DrawSetTextFont( hFont );
	vgui::surface()->DrawSetTextPos( x, y );
	vgui::surface()->DrawSetTextColor( r, g, b, a );
	vgui::surface()->DrawPrintText( szconverted, len );
}

void CBaseModPanel::DrawCopyStats()
{
#if defined( _X360 )
	int wide, tall;
	GetSize( wide, tall );

	int xPos = 0.1f * wide;
	int yPos = 0.1f * tall;

	// draw copy status
	char textBuffer[256];
	const CopyStats_t *pCopyStats = g_pXboxInstaller->GetCopyStats();	

	V_snprintf( textBuffer, sizeof( textBuffer ), "Version: %d (%s)", g_pXboxInstaller->GetVersion(), XBX_GetLanguageString() );
	DrawColoredText( m_hDefaultFont, xPos, yPos, 0xffff00ff, textBuffer );
	yPos += 20;

	V_snprintf( textBuffer, sizeof( textBuffer ), "DVD Hosted: %s", g_pFullFileSystem->IsDVDHosted() ? "Enabled" : "Disabled" );
	DrawColoredText( m_hDefaultFont, xPos, yPos, 0xffff00ff, textBuffer );
	yPos += 20;

	bool bDrawProgress = true;
	if ( g_pFullFileSystem->IsInstalledToXboxHDDCache() )
	{
		DrawColoredText( m_hDefaultFont, xPos, yPos, 0x00ff00ff, "Existing Image Found." );
		yPos += 20;
		bDrawProgress = false;
	}
	if ( !g_pXboxInstaller->IsInstallEnabled() )
	{
		DrawColoredText( m_hDefaultFont, xPos, yPos, 0xff0000ff, "Install Disabled." );
		yPos += 20;
		bDrawProgress = false;
	}
	if ( g_pXboxInstaller->IsFullyInstalled() )
	{
		DrawColoredText( m_hDefaultFont, xPos, yPos, 0x00ff00ff, "Install Completed." );
		yPos += 20;
	}

	if ( bDrawProgress )
	{
		yPos += 20;
		V_snprintf( textBuffer, sizeof( textBuffer ), "From: %s (%.2f MB)", pCopyStats->m_srcFilename, (float)pCopyStats->m_ReadSize/(1024.0f*1024.0f) );
		DrawColoredText( m_hDefaultFont, xPos, yPos, 0xffff00ff, textBuffer );
		V_snprintf( textBuffer, sizeof( textBuffer ), "To: %s (%.2f MB)", pCopyStats->m_dstFilename, (float)pCopyStats->m_WriteSize/(1024.0f*1024.0f)  );
		DrawColoredText( m_hDefaultFont, xPos, yPos + 20, 0xffff00ff, textBuffer );

		float elapsed = 0;
		float rate = 0;
		if ( pCopyStats->m_InstallStartTime )
		{
			elapsed = (float)(GetTickCount() - pCopyStats->m_InstallStartTime) * 0.001f;
		}
		if ( pCopyStats->m_InstallStopTime )
		{
			elapsed = (float)(pCopyStats->m_InstallStopTime - pCopyStats->m_InstallStartTime) * 0.001f;
		}
		if ( elapsed )
		{
			rate = pCopyStats->m_TotalWriteSize/elapsed;
		}
		V_snprintf( textBuffer, sizeof( textBuffer ), "Progress: %d/%d MB Elapsed: %d secs (%.2f MB/s)", pCopyStats->m_BytesCopied/(1024*1024), g_pXboxInstaller->GetTotalSize()/(1024*1024), (int)elapsed, rate/(1024.0f*1024.0f) );
		DrawColoredText( m_hDefaultFont, xPos, yPos + 40, 0xffff00ff, textBuffer );
	}
#endif
}

//=============================================================================
void CBaseModPanel::PaintBackground()
{
	if ( !m_LevelLoading &&
		!GameUI().IsInLevel() &&
		!GameUI().IsInBackgroundLevel() )
	{
		int wide, tall;
		GetSize( wide, tall );

		if ( engine->IsTransitioningToLoad() )
		{
			// ensure the background is clear
			// the loading progress is about to take over in a few frames
			// this keeps us from flashing a different graphic
			surface()->DrawSetColor( 0, 0, 0, 255 );
			surface()->DrawSetTexture( m_iBackgroundImageID );
			surface()->DrawTexturedRect( 0, 0, wide, tall );
		}
		else
		{
			ActivateBackgroundEffects();

			if ( ASWBackgroundMovie() )
			{
				ASWBackgroundMovie()->Update();
				if ( ASWBackgroundMovie()->SetTextureMaterial() != -1 )
				{
					surface()->DrawSetColor( 255, 255, 255, 255 );
					int x, y, w, h;
					GetBounds( x, y, w, h );

					// center, 16:9 aspect ratio
					int width_at_ratio = h * (16.0f / 9.0f);
					x = ( w * 0.5f ) - ( width_at_ratio * 0.5f );

					surface()->DrawTexturedRect( x, y, x + width_at_ratio, y + h );

					if ( !m_flMovieFadeInTime )
					{
						// do the fade a little bit after the movie starts (needs to be stable)
						// the product overlay will fade out
						m_flMovieFadeInTime	= Plat_FloatTime() + TRANSITION_TO_MOVIE_DELAY_TIME;
					}

					float flFadeDelta = RemapValClamped( Plat_FloatTime(), m_flMovieFadeInTime, m_flMovieFadeInTime + TRANSITION_TO_MOVIE_FADE_TIME, 1.0f, 0.0f );
					if ( flFadeDelta > 0.0f )
					{
						if ( !m_pBackgroundMaterial )
						{
							PrepareStartupGraphic();
						}
						DrawStartupGraphic( flFadeDelta );
					}
				}
			}
		}
	}

#if defined( _X360 )
	if ( !m_LevelLoading && !GameUI().IsInLevel() && xbox_install_status.GetBool() )
	{
		DrawCopyStats();
	}
#endif
}

IVTFTexture *LoadVTF( CUtlBuffer &temp, const char *szFileName )
{
	if ( !g_pFullFileSystem->ReadFile( szFileName, NULL, temp ) )
		return NULL;

	IVTFTexture *texture = CreateVTFTexture();
	if ( !texture->Unserialize( temp ) )
	{
		Error( "Invalid or corrupt background texture %s\n", szFileName );
		return NULL;
	}
	texture->ConvertImageFormat( IMAGE_FORMAT_RGBA8888, false );
	return texture;
}

void CBaseModPanel::PrepareStartupGraphic()
{
	CUtlBuffer buf;
	// load in the background vtf
	buf.Clear();
	m_pBackgroundTexture = LoadVTF( buf, m_szFadeFilename );
	if ( !m_pBackgroundTexture )
	{
		Error( "Can't find background image '%s'\n", m_szFadeFilename );
		return;
	}

	// Allocate a white material
	m_pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	m_pVMTKeyValues->SetString( "$basetexture", m_szFadeFilename + 10 );
	m_pVMTKeyValues->SetInt( "$ignorez", 1 );
	m_pVMTKeyValues->SetInt( "$nofog", 1 );
	m_pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	m_pVMTKeyValues->SetInt( "$nocull", 1 );
	m_pVMTKeyValues->SetInt( "$vertexalpha", 1 );
	m_pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	m_pBackgroundMaterial = g_pMaterialSystem->CreateMaterial( "__background", m_pVMTKeyValues );
}

void CBaseModPanel::ReleaseStartupGraphic()
{
	if ( m_pBackgroundMaterial )
	{
		m_pBackgroundMaterial->Release();
	}

	if ( m_pBackgroundTexture )
	{
		DestroyVTFTexture( m_pBackgroundTexture );
		m_pBackgroundTexture = NULL;
	}
}

// we have to draw the startup fade graphic using this function so it perfectly matches the one drawn by the engine during load
void DrawScreenSpaceRectangleAlpha( IMaterial *pMaterial, 
							  int nDestX, int nDestY, int nWidth, int nHeight,	// Rect to draw into in screen space
							  float flSrcTextureX0, float flSrcTextureY0,		// which texel you want to appear at destx/y
							  float flSrcTextureX1, float flSrcTextureY1,		// which texel you want to appear at destx+width-1, desty+height-1
							  int nSrcTextureWidth, int nSrcTextureHeight,		// needed for fixup
							  void *pClientRenderable,							// Used to pass to the bind proxies
							  int nXDice, int nYDice,							// Amount to tessellate the mesh
							  float fDepth, float flAlpha )									// what Z value to put in the verts (def 0.0)
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	if ( ( nWidth <= 0 ) || ( nHeight <= 0 ) )
		return;

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->Bind( pMaterial, pClientRenderable );

	int xSegments = MAX( nXDice, 1);
	int ySegments = MAX( nYDice, 1);

	CMeshBuilder meshBuilder;

	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, xSegments * ySegments );

	int nScreenWidth, nScreenHeight;
	pRenderContext->GetRenderTargetDimensions( nScreenWidth, nScreenHeight );
	float flLeftX = nDestX - 0.5f;
	float flRightX = nDestX + nWidth - 0.5f;

	float flTopY = nDestY - 0.5f;
	float flBottomY = nDestY + nHeight - 0.5f;

	float flSubrectWidth = flSrcTextureX1 - flSrcTextureX0;
	float flSubrectHeight = flSrcTextureY1 - flSrcTextureY0;

	float flTexelsPerPixelX = ( nWidth > 1 ) ? flSubrectWidth / ( nWidth - 1 ) : 0.0f;
	float flTexelsPerPixelY = ( nHeight > 1 ) ? flSubrectHeight / ( nHeight - 1 ) : 0.0f;

	float flLeftU = flSrcTextureX0 + 0.5f - ( 0.5f * flTexelsPerPixelX );
	float flRightU = flSrcTextureX1 + 0.5f + ( 0.5f * flTexelsPerPixelX );
	float flTopV = flSrcTextureY0 + 0.5f - ( 0.5f * flTexelsPerPixelY );
	float flBottomV = flSrcTextureY1 + 0.5f + ( 0.5f * flTexelsPerPixelY );

	float flOOTexWidth = 1.0f / nSrcTextureWidth;
	float flOOTexHeight = 1.0f / nSrcTextureHeight;
	flLeftU *= flOOTexWidth;
	flRightU *= flOOTexWidth;
	flTopV *= flOOTexHeight;
	flBottomV *= flOOTexHeight;

	// Get the current viewport size
	int vx, vy, vw, vh;
	pRenderContext->GetViewport( vx, vy, vw, vh );

	// map from screen pixel coords to -1..1
	flRightX = FLerp( -1, 1, 0, vw, flRightX );
	flLeftX = FLerp( -1, 1, 0, vw, flLeftX );
	flTopY = FLerp( 1, -1, 0, vh ,flTopY );
	flBottomY = FLerp( 1, -1, 0, vh, flBottomY );

	// Dice the quad up...
	if ( xSegments > 1 || ySegments > 1 )
	{
		// Screen height and width of a subrect
		float flWidth  = (flRightX - flLeftX) / (float) xSegments;
		float flHeight = (flTopY - flBottomY) / (float) ySegments;

		// UV height and width of a subrect
		float flUWidth  = (flRightU - flLeftU) / (float) xSegments;
		float flVHeight = (flBottomV - flTopV) / (float) ySegments;

		for ( int x=0; x < xSegments; x++ )
		{
			for ( int y=0; y < ySegments; y++ )
			{
				// Top left
				meshBuilder.Position3f( flLeftX   + (float) x * flWidth, flTopY - (float) y * flHeight, fDepth );
				meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
				meshBuilder.TexCoord2f( 0, flLeftU   + (float) x * flUWidth, flTopV + (float) y * flVHeight);
				meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
				meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
				meshBuilder.Color4ub( 255, 255, 255, 255.0f * flAlpha );
				meshBuilder.AdvanceVertex();

				// Top right (x+1)
				meshBuilder.Position3f( flLeftX   + (float) (x+1) * flWidth, flTopY - (float) y * flHeight, fDepth );
				meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
				meshBuilder.TexCoord2f( 0, flLeftU   + (float) (x+1) * flUWidth, flTopV + (float) y * flVHeight);
				meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
				meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
				meshBuilder.Color4ub( 255, 255, 255, 255.0f * flAlpha );
				meshBuilder.AdvanceVertex();

				// Bottom right (x+1), (y+1)
				meshBuilder.Position3f( flLeftX   + (float) (x+1) * flWidth, flTopY - (float) (y+1) * flHeight, fDepth );
				meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
				meshBuilder.TexCoord2f( 0, flLeftU   + (float) (x+1) * flUWidth, flTopV + (float)(y+1) * flVHeight);
				meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
				meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
				meshBuilder.Color4ub( 255, 255, 255, 255.0f * flAlpha );
				meshBuilder.AdvanceVertex();

				// Bottom left (y+1)
				meshBuilder.Position3f( flLeftX   + (float) x * flWidth, flTopY - (float) (y+1) * flHeight, fDepth );
				meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
				meshBuilder.TexCoord2f( 0, flLeftU   + (float) x * flUWidth, flTopV + (float)(y+1) * flVHeight);
				meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
				meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
				meshBuilder.Color4ub( 255, 255, 255, 255.0f * flAlpha );
				meshBuilder.AdvanceVertex();
			}
		}
	}
	else // just one quad
	{
		for ( int corner=0; corner<4; corner++ )
		{
			bool bLeft = (corner==0) || (corner==3);
			meshBuilder.Position3f( (bLeft) ? flLeftX : flRightX, (corner & 2) ? flBottomY : flTopY, fDepth );
			meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
			meshBuilder.TexCoord2f( 0, (bLeft) ? flLeftU : flRightU, (corner & 2) ? flBottomV : flTopV );
			meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
			meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
			meshBuilder.Color4ub( 255, 255, 255, 255.0f * flAlpha );
			meshBuilder.AdvanceVertex();
		}
	}

	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
}

void CBaseModPanel::DrawStartupGraphic( float flNormalizedAlpha )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	int w = GetWide();
	int h = GetTall();
	int tw = m_pBackgroundTexture->Width();
	int th = m_pBackgroundTexture->Height();

	float depth = 0.5f;
	int width_at_ratio = h * (16.0f / 9.0f);
	int x = ( w * 0.5f ) - ( width_at_ratio * 0.5f );
	DrawScreenSpaceRectangleAlpha( m_pBackgroundMaterial, x, 0, width_at_ratio, h, 8, 8, tw-8, th-8, tw, th, NULL,1,1,depth,flNormalizedAlpha );
}

void CBaseModPanel::OnCommand(const char *command)
{
	if ( !Q_stricmp( command, "QuitRestartNoConfirm" ) )
	{
		if ( IsX360() )
		{
			StartExitingProcess( false );
		}
	}
	else if ( !Q_stricmp( command, "RestartWithNewLanguage" ) )
	{
		if ( !IsX360() )
		{
			const char *pUpdatedAudioLanguage = Audio::GetUpdatedAudioLanguage();

			if ( pUpdatedAudioLanguage[ 0 ] != '\0' )
			{
				char szSteamURL[50];
				char szAppId[50];

				// hide everything while we quit
				SetVisible( false );
				vgui::surface()->RestrictPaintToSinglePanel( GetVPanel() );
				engine->ClientCmd_Unrestricted( "quit\n" );

				// Construct Steam URL. Pattern is steam://run/<appid>/<language>. (e.g. Ep1 In French ==> steam://run/380/french)
				Q_strcpy(szSteamURL, "steam://run/");
				itoa( engine->GetAppID(), szAppId, 10 );
				Q_strcat( szSteamURL, szAppId, sizeof( szSteamURL ) );
				Q_strcat( szSteamURL, "/", sizeof( szSteamURL ) );
				Q_strcat( szSteamURL, pUpdatedAudioLanguage, sizeof( szSteamURL ) );

				// Set Steam URL for re-launch in registry. Launcher will check this registry key and exec it in order to re-load the game in the proper language
				vgui::system()->SetRegistryString("HKEY_CURRENT_USER\\Software\\Valve\\Source\\Relaunch URL", szSteamURL );
			}
		}
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

bool CBaseModPanel::IsReadyToWriteConfig( void )
{
	// For cert we only want to write config files is it has been at least 3 seconds
#ifdef _X360
	static ConVarRef r_host_write_last_time( "host_write_last_time" );
	return ( Plat_FloatTime() > r_host_write_last_time.GetFloat() + 3.05f );
#endif
	return false;
}

const char *CBaseModPanel::GetUISoundName(  UISound_t UISound )
{
	switch ( UISound )
	{
	case UISOUND_BACK:
		return "UI/menu_back.wav";
	case UISOUND_ACCEPT:
		return "UI/menu_accept.wav";
	case UISOUND_INVALID:
		return "UI/menu_invalid.wav";
	case UISOUND_COUNTDOWN:
		return "UI/menu_countdown.wav";
	case UISOUND_FOCUS:
		return "UI/menu_focus.wav";
	case UISOUND_CLICK:
		return "UI/buttonclick.wav";
	case UISOUND_DENY:
		return "UI/menu_invalid.wav";
	}
	return NULL;
}

void CBaseModPanel::PlayUISound( UISound_t UISound )
{
	const char *pSound = GetUISoundName( UISound );
	if ( pSound )
	{
		vgui::surface()->PlaySound( pSound );
	}
}

//=============================================================================
// Start system shutdown. Cannot be stopped.
// A Restart is cold restart, plays the intro movie again.
//=============================================================================
void CBaseModPanel::StartExitingProcess( bool bWarmRestart )
{
	if ( !IsX360() )
	{
		// xbox only
		Assert( 0 );
		return;
	}

	if ( m_ExitingFrameCount )
	{
		// already fired
		return;
	}

#if defined( _X360 )
	// signal the installer to stop
	g_pXboxInstaller->Stop();
#endif

	// cold restart or warm
	m_bWarmRestartMode = bWarmRestart;

	// the exiting screen will transition to obscure all the game and UI
	OpenWindow( WT_TRANSITIONSCREEN, 0, false );

	// must let a non trivial number of screen swaps occur to stabilize image
	// ui runs in a constrained state, while shutdown is occurring
	m_ExitingFrameCount = 15;

	// exiting cannot be stopped
	// do not allow any input to occur
	g_pInputSystem->DetachFromWindow();

	// start shutting down systems
	engine->StartXboxExitingProcess();
}

void CBaseModPanel::OnSetFocus()
{
	BaseClass::OnSetFocus();
	if ( IsPC() )
	{
		GameConsole().Hide();
	}
}

void CBaseModPanel::OnMovedPopupToFront()
{
	if ( IsPC() )
	{
		GameConsole().Hide();
	}
}

bool CBaseModPanel::IsBackgroundMusicPlaying()
{
	if ( m_backgroundMusic.IsEmpty() )
		return false;

	if ( m_nBackgroundMusicGUID == 0 )
		return false;
	
	return enginesound->IsSoundStillPlaying( m_nBackgroundMusicGUID );
}

// per Morasky
#define BACKGROUND_MUSIC_DUCK	0.15f

bool CBaseModPanel::StartBackgroundMusic( float fVol )
{
	if ( IsBackgroundMusicPlaying() )
		return true;
	
	if ( m_backgroundMusic.IsEmpty() )
		return false;

	// trying to exit, cannot start it
	if ( m_ExitingFrameCount )
		return false;
	
	CSoundParameters params;
	if ( !soundemitterbase->GetParametersForSound( m_backgroundMusic.Get(), params, GENDER_NONE ) )
		return false;

	enginesound->EmitAmbientSound( params.soundname, params.volume * fVol, params.pitch );
	m_nBackgroundMusicGUID = enginesound->GetGuidForLastSoundEmitted();
		
	return ( m_nBackgroundMusicGUID != 0 );
}

void CBaseModPanel::UpdateBackgroundMusicVolume( float fVol )
{
	if ( !IsBackgroundMusicPlaying() )
		return;

	// mixes too loud against soft ui sounds
	enginesound->SetVolumeByGuid( m_nBackgroundMusicGUID, BACKGROUND_MUSIC_DUCK * fVol );
}

void CBaseModPanel::ReleaseBackgroundMusic()
{
	if ( m_backgroundMusic.IsEmpty() )
		return;

	if ( m_nBackgroundMusicGUID == 0 )
		return;

	// need to stop the sound now, do not queue the stop
	// we must release the 2-5 MB held by this resource
	enginesound->StopSoundByGuid( m_nBackgroundMusicGUID );
#if defined( _X360 )
	// TODO: enginesound->UnloadSound( m_backgroundMusic );
#endif

	m_nBackgroundMusicGUID = 0;
}

void CBaseModPanel::SafeNavigateTo( Panel *pExpectedFrom, Panel *pDesiredTo, bool bAllowStealFocus )
{
	Panel *pOriginalFocus = ipanel()->GetPanel( GetCurrentKeyFocus(), GetModuleName() );
	bool bSomeoneElseHasFocus = pOriginalFocus && (pOriginalFocus != pExpectedFrom);
	bool bActuallyChangingFocus = (pExpectedFrom != pDesiredTo);
	bool bNeedToReturnKeyFocus = !bAllowStealFocus && bSomeoneElseHasFocus && bActuallyChangingFocus;

	pDesiredTo->NavigateTo();

	if ( bNeedToReturnKeyFocus )
	{
		pDesiredTo->NavigateFrom();
		pOriginalFocus->NavigateTo();
	}
}

