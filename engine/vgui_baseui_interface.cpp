//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements all the functions exported by the GameUI dll
//
// $NoKeywords: $
//===========================================================================//


#include "client_pch.h"

#include "tier0/platform.h"

#ifdef IS_WINDOWS_PC
#include "winlite.h"
#endif
#include "appframework/ilaunchermgr.h"
#include <vgui_controls/Panel.h>
#include <vgui_controls/EditablePanel.h>
#include <matsys_controls/matsyscontrols.h>
#include <vgui/Cursor.h>
#include <vgui_controls/PHandle.h>
#include "keys.h"
#include "console.h"
#include "gl_matsysiface.h"
#include "cdll_engine_int.h"
#include "demo.h"
#include "sys_dll.h"
#include "sound.h"
#include "soundflags.h"
#include "filesystem_engine.h"
#include "igame.h"
#include "con_nprint.h"
#include "vgui_DebugSystemPanel.h"
#include "tier0/vprof.h"
#include "cl_demoactionmanager.h"
#include "enginebugreporter.h"
#include "engineperftools.h"
#include "icolorcorrectiontools.h"
#include "tier0/icommandline.h"
#include "client.h"
#include "server.h"
#include "sys.h" // Sys_GetRegKeyValue()
#include "vgui_drawtreepanel.h"
#include "vgui_vprofpanel.h"
#include "vgui/VGUI.h"
#include "vgui/IInput.h"
#include <vgui/IInputInternal.h>
#include "vgui_controls/AnimationController.h"
#include "vgui_vprofgraphpanel.h"
#include "vgui_texturebudgetpanel.h"
#include "vgui_budgetpanel.h"
#include "ivideomode.h"
#include "sourcevr/isourcevirtualreality.h"
#include "cl_pluginhelpers.h"
#include "cl_main.h" // CL_IsHL2Demo()
#include "cl_steamauth.h"

// interface to gameui dll
#include <GameUI/IGameUI.h>
#include <GameUI/IGameConsole.h>

// interface to expose vgui root panels
#include <ienginevgui.h>
#include "VGuiMatSurface/IMatSystemSurface.h"

#include "cl_texturelistpanel.h"
#include "cl_demouipanel.h"
#include "cl_foguipanel.h"
#include "cl_txviewpanel.h"

// vgui2 interface
// note that GameUI project uses ..\public\vgui and ..\public\vgui_controls, not ..\utils\vgui\include
#include <vgui/VGUI.h>
#include <vgui/Cursor.h>
#include <KeyValues.h>
#include <vgui/ILocalize.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui_controls/EditablePanel.h>

#include <vgui_controls/MenuButton.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/PHandle.h>

#include "IVguiModule.h"
#include "vgui_baseui_interface.h"
#include "vgui_DebugSystemPanel.h"
#include "toolframework/itoolframework.h"
#include "filesystem/IQueuedLoader.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

#include "vgui_askconnectpanel.h"

#if defined( REPLAY_ENABLED )
#include "replay_internal.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef USE_SDL
extern HWND *pmainwindow;
#endif

extern IVEngineClient *engineClient;
extern bool g_bTextMode;
static int g_syncReportLevel = -1;

void VGui_ActivateMouse();

extern CreateInterfaceFn g_AppSystemFactory;

// functions to reference GameUI and GameConsole functions, from GameUI.dll
IGameUI *staticGameUIFuncs = NULL;
IGameConsole *staticGameConsole = NULL;

// cache some of the state we pass through to matsystemsurface, for visibility
bool s_bWindowsInputEnabled = true;

ConVar r_drawvgui( "r_drawvgui", "1", FCVAR_CHEAT, "Enable the rendering of vgui panels" );
ConVar gameui_xbox( "gameui_xbox", "0", 0 );

void Con_CreateConsolePanel( vgui::Panel *parent );
void CL_CreateEntityReportPanel( vgui::Panel *parent );
void ClearIOStates( void );

// turn this on if you're tuning progress bars
//#define ENABLE_LOADING_PROGRESS_PROFILING

//-----------------------------------------------------------------------------
// Purpose: Console command to hide the gameUI, most commonly called from gameUI.dll
//-----------------------------------------------------------------------------
CON_COMMAND( gameui_hide, "Hides the game UI" )
{
	EngineVGui()->HideGameUI();
}

//-----------------------------------------------------------------------------
// Purpose: Console command to activate the gameUI, most commonly called from gameUI.dll
//-----------------------------------------------------------------------------
CON_COMMAND( gameui_activate, "Shows the game UI" )
{
	EngineVGui()->ActivateGameUI();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( gameui_preventescape, "Escape key doesn't hide game UI" )
{
	EngineVGui()->SetNotAllowedToHideGameUI( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( gameui_allowescapetoshow, "Escape key allowed to show game UI" )
{
	EngineVGui()->SetNotAllowedToShowGameUI( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( gameui_preventescapetoshow, "Escape key doesn't show game UI" )
{
	EngineVGui()->SetNotAllowedToShowGameUI( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( gameui_allowescape, "Escape key allowed to hide game UI" )
{
	EngineVGui()->SetNotAllowedToHideGameUI( false );
}

//-----------------------------------------------------------------------------
// Purpose: Console command to enable progress bar for next load
//-----------------------------------------------------------------------------
void BaseUI_ProgressEnabled_f()
{
	EngineVGui()->EnabledProgressBarForNextLoad();
}
static ConCommand progress_enable("progress_enable", &BaseUI_ProgressEnabled_f );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEnginePanel : public vgui::EditablePanel
{
	typedef vgui::EditablePanel BaseClass;
public:
	CEnginePanel( vgui::Panel *pParent, const char *pName ) : BaseClass( pParent, pName )
	{
		//m_bCanFocus = true;
		SetMouseInputEnabled( true );
		SetKeyBoardInputEnabled( true );
	}

	void EnableMouseFocus( bool state )
	{
		//m_bCanFocus = state;
		SetMouseInputEnabled( state );
		SetKeyBoardInputEnabled( state );
	}

/*	virtual vgui::VPANEL IsWithinTraverse(int x, int y, bool traversePopups)
	{
		if ( !m_bCanFocus )
			return NULL;

		vgui::VPANEL retval = BaseClass::IsWithinTraverse( x, y, traversePopups );
		if ( retval == GetVPanel() )
			return NULL;
		return retval;
	}*/

//private:
//	bool		m_bCanFocus;
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CStaticPanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;

public:
	CStaticPanel( vgui::Panel *pParent, const char *pName ) : vgui::Panel( pParent, pName )
	{
		SetCursor( vgui::dc_none );
		SetKeyBoardInputEnabled( false );
		SetMouseInputEnabled( false );
	}
};

vgui::VPanelHandle g_DrawTreeSelectedPanel;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CFocusOverlayPanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;

public:
	CFocusOverlayPanel( vgui::Panel *pParent, const char *pName );

	virtual void PostChildPaint( void );
	static void GetColorForSlot( int slot, int& r, int& g, int& b )
	{
		r = (int)( 124.0 + slot * 47.3 ) & 255;
		g = (int)( 63.78 - slot * 71.4 ) & 255;
		b = (int)( 188.42 + slot * 13.57 ) & 255;
	}

	bool DrawTitleSafeOverlay( void );
	bool DrawFocusPanelList( void );
};

//-----------------------------------------------------------------------------
//
// Purpose: Centerpoint for handling all user interface in the engine
//
//-----------------------------------------------------------------------------
class CEngineVGui : public IEngineVGuiInternal
{
public:
	CEngineVGui();
	~CEngineVGui();

	// Methods of IEngineVGui
	virtual vgui::VPANEL GetPanel( VGuiPanel_t type );

	// Methods of IEngineVGuiInternal
	virtual void Init();
	virtual void Connect();
	virtual void Shutdown();
	virtual bool SetVGUIDirectories();
	virtual bool IsInitialized() const;
	virtual bool Key_Event( const InputEvent_t &event );
	virtual void UpdateButtonState( const InputEvent_t &event );
	virtual void BackwardCompatibility_Paint();
	virtual void Paint( PaintMode_t mode );
	virtual void PostInit();

	CreateInterfaceFn GetGameUIFactory()
	{
		return m_GameUIFactory;
	}
	
	// handlers for game UI (main menu)
	virtual void ActivateGameUI();
	virtual bool HideGameUI();
	virtual bool IsGameUIVisible();

	// console
	virtual void ShowConsole();
	virtual void HideConsole();
	virtual bool IsConsoleVisible();
	virtual void ClearConsole();

	// level loading
	virtual void OnLevelLoadingStarted();
	virtual void OnLevelLoadingFinished();
	virtual void NotifyOfServerConnect(const char *game, int IP, int connectionPort, int queryPort);
	virtual void NotifyOfServerDisconnect();
	virtual void UpdateProgressBar(LevelLoadingProgress_e progress);
	virtual void UpdateCustomProgressBar( float progress, const wchar_t *desc );
	virtual void StartCustomProgress();
	virtual void FinishCustomProgress();

	virtual void EnabledProgressBarForNextLoad()
	{
		m_bShowProgressDialog = true;
	}

	// Should pause?
	virtual bool ShouldPause();
	virtual void ShowErrorMessage();

	virtual void SetNotAllowedToHideGameUI( bool bNotAllowedToHide )
	{
		m_bNotAllowedToHideGameUI = bNotAllowedToHide;
	}

	virtual void SetNotAllowedToShowGameUI( bool bNotAllowedToShow )
	{
		m_bNotAllowedToShowGameUI = bNotAllowedToShow;
	}

	void SetGameDLLPanelsVisible( bool show )
	{
		if ( !staticGameDLLPanel )
		{
			return;
		}

		staticGameDLLPanel->SetVisible( show );
	}

	virtual void ShowNewGameDialog( int chapter = -1 ); // -1 means just keep the currently select chapter

	// Xbox 360
	virtual void SessionNotification( const int notification, const int param = 0 );
	virtual void SystemNotification( const int notification );
	virtual void ShowMessageDialog( const uint nType, vgui::Panel *pOwner = NULL );
	virtual void UpdatePlayerInfo( uint64 nPlayerId, const char *pName, int nTeam, byte cVoiceState, int nPlayersNeeded, bool bHost );
	virtual void SessionSearchResult( int searchIdx, void *pHostData, XSESSION_SEARCHRESULT *pResult, int ping );
	virtual void OnCreditsFinished( void );

	// Storage device validation:
	//		returns true right away if storage device has been previously selected.
	//		otherwise returns false and will set the variable pointed by pStorageDeviceValidated to 1
	//				  once the storage device is selected by user.
	virtual bool ValidateStorageDevice( int *pStorageDeviceValidated );

	void SetProgressBias( float bias );
	void UpdateProgressBar( float progress );

	virtual void ConfirmQuit( void );

private:
	vgui::Panel *GetRootPanel( VGuiPanel_t type );
	void SetEngineVisible( bool state );
	void DrawMouseFocus( void );
	void CreateVProfPanels( vgui::Panel *pParent );
	void DestroyVProfPanels( );

	virtual void Simulate();

	// debug overlays
	bool IsDebugSystemVisible();
	void HideDebugSystem();

	bool IsShiftKeyDown();
	bool IsAltKeyDown();
	bool IsCtrlKeyDown();

	CON_COMMAND_MEMBER_F( CEngineVGui, "debugsystemui", ToggleDebugSystemUI, "Show/hide the debug system UI.", FCVAR_CHEAT );

private:
	enum { MAX_NUM_FACTORIES = 5 };
	CreateInterfaceFn m_FactoryList[MAX_NUM_FACTORIES];
	int m_iNumFactories;

	CSysModule *m_hStaticGameUIModule;
	CreateInterfaceFn m_GameUIFactory;

	// top level VGUI2 panel
	CStaticPanel *staticPanel;

	// base level panels for other subsystems, rooted on staticPanel
	CEnginePanel *staticClientDLLPanel;
	CEnginePanel *staticClientDLLToolsPanel;
	CEnginePanel *staticGameUIPanel;
	CEnginePanel *staticGameDLLPanel;

	// Want engine tools to be on top of other engine panels
	CEnginePanel *staticEngineToolsPanel;
	CDebugSystemPanel *staticDebugSystemPanel;
	CFocusOverlayPanel *staticFocusOverlayPanel;

#ifdef VPROF_ENABLED
	CVProfPanel *m_pVProfPanel;
	CBudgetPanelEngine *m_pBudgetPanel;
	CTextureBudgetPanel *m_pTextureBudgetPanel;
#endif

	// progress bar
	bool m_bShowProgressDialog;
	LevelLoadingProgress_e m_eLastProgressPoint;

	// progress bar debugging
	int m_nLastProgressPointRepeatCount;
	double m_flLoadingStartTime;
	struct LoadingProgressEntry_t
	{
		double flTime;
		LevelLoadingProgress_e eProgress;
	};
	CUtlVector<LoadingProgressEntry_t> m_LoadingProgress;

	bool					m_bSaveProgress : 1;
	bool					m_bNoShaderAPI : 1;
	// game ui hiding control
	bool					m_bNotAllowedToHideGameUI : 1;
	bool					m_bNotAllowedToShowGameUI : 1;

	vgui::IInputInternal *m_pInputInternal;

	// used to start the progress from an arbitrary position
	float					m_ProgressBias;
};


//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
static CEngineVGui g_EngineVGuiImp;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineVGui, IEngineVGui, VENGINE_VGUI_VERSION, g_EngineVGuiImp );

IEngineVGuiInternal *EngineVGui()
{
	return &g_EngineVGuiImp;
}

//-----------------------------------------------------------------------------
// The loader progress is updated by the queued loader. It uses an initial
// reserved portion of the bar.
//-----------------------------------------------------------------------------
#define PROGRESS_RESERVE 0.50f
class CLoaderProgress : public ILoaderProgress
{
public:
	CLoaderProgress()
	{ 
		// initialize to disabled state
		m_SnappedProgress = -1;
	}

	void BeginProgress()
	{
		g_EngineVGuiImp.SetProgressBias( 0 );
		m_SnappedProgress = 0;
	}

	void UpdateProgress( float progress )
	{
		if ( m_SnappedProgress == - 1 )
		{
			// not enabled
			return;
		}	

		int snappedProgress = progress * 15;

		// Need excessive updates on the 360 to keep the XBox slider inny bar active
		if ( !IsX360() && ( snappedProgress <= m_SnappedProgress ) )
		{
			// prevent excessive updates
			return;
		}
		m_SnappedProgress = snappedProgress;

		// up to reserved
		g_EngineVGuiImp.UpdateProgressBar( PROGRESS_RESERVE * progress );
	}

	void EndProgress()
	{
		// the normal legacy bar now picks up after reserved region
		g_EngineVGuiImp.SetProgressBias( PROGRESS_RESERVE );
		m_SnappedProgress = -1;
	}

private:
	int m_SnappedProgress;
};
static CLoaderProgress s_LoaderProgress;

	
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEngineVGui::CEngineVGui()
{
	staticPanel = NULL;
	staticClientDLLToolsPanel = NULL;
	staticClientDLLPanel = NULL;
	staticGameDLLPanel = NULL;
	staticGameUIPanel = NULL;
	staticEngineToolsPanel = NULL;
	staticDebugSystemPanel = NULL;
	staticFocusOverlayPanel = NULL;

	m_hStaticGameUIModule = NULL;
	m_GameUIFactory = NULL;

#ifdef VPROF_ENABLED
	m_pVProfPanel = NULL;
#endif

	m_bShowProgressDialog = false;
	m_bSaveProgress = false;
	m_bNoShaderAPI = false;
	m_bNotAllowedToHideGameUI = false;
	m_bNotAllowedToShowGameUI = false;
	m_pInputInternal = NULL;
	m_ProgressBias = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CEngineVGui::~CEngineVGui()
{
}


//-----------------------------------------------------------------------------
// add all the base search paths used by VGUI (platform, skins directory, language dirs)
//-----------------------------------------------------------------------------
bool CEngineVGui::SetVGUIDirectories()
{
	// Legacy, not supported anymore
	return true;
}

//-----------------------------------------------------------------------------
// Setup the base vgui panels
//-----------------------------------------------------------------------------
void CEngineVGui::Init()
{
	COM_TimestampedLog( "Loading gameui.dll" );

	// load the GameUI dll
	const char *szDllName = "GameUI";
	m_hStaticGameUIModule = g_pFileSystem->LoadModule(szDllName, "EXECUTABLE_PATH", true); // LoadModule() does a GetLocalCopy() call
	m_GameUIFactory = Sys_GetFactory(m_hStaticGameUIModule);
	if ( !m_GameUIFactory )
	{
		Error( "Could not load: %s\n", szDllName );
	}
	
	// get the initialization func
	staticGameUIFuncs = (IGameUI *)m_GameUIFactory(GAMEUI_INTERFACE_VERSION, NULL);
	if (!staticGameUIFuncs )
	{
		Error( "Could not get IGameUI interface %s from %s\n", GAMEUI_INTERFACE_VERSION, szDllName );
	}

	if ( IsPC() )
	{
		staticGameConsole = (IGameConsole *)m_GameUIFactory(GAMECONSOLE_INTERFACE_VERSION, NULL);
		if ( !staticGameConsole )
		{
			Sys_Error( "Could not get IGameConsole interface %s from %s\n", GAMECONSOLE_INTERFACE_VERSION, szDllName );
		}
	}

	vgui::VGui_InitMatSysInterfacesList( "BaseUI", &g_AppSystemFactory, 1 );

	// Get our langauge string
	char lang[ 64 ];
	lang[0] = 0;
	engineClient->GetUILanguage( lang, sizeof( lang ) );
	if ( lang[0] )
		vgui::system()->SetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Source\\Language", lang );

	COM_TimestampedLog( "AttachToWindow" );

	// Need to be able to play sounds through vgui
	g_pMatSystemSurface->InstallPlaySoundFunc( VGui_PlaySound );

	COM_TimestampedLog( "Load Scheme File" );

	// load scheme
	const char *pStr = "Resource/SourceScheme.res";
	if ( !vgui::scheme()->LoadSchemeFromFile( pStr, "Tracker" ))
	{
		Sys_Error( "Error loading file %s\n", pStr );
		return;
	}

	if ( IsX360() )
	{
		CCommand ccommand;
		if ( CL_ShouldLoadBackgroundLevel( ccommand ) )
		{
			// Must be before the game ui base panel starts up
			// This is a hint to avoid the menu pop due to the impending background map
			// that the game ui is not aware of until 1 frame later.
			staticGameUIFuncs->SetProgressOnStart();
		}
	}

	COM_TimestampedLog( "vgui::ivgui()->Start()" );

	// Start the App running
	vgui::ivgui()->Start();
	vgui::ivgui()->SetSleep(false);

	// setup base panel for the whole VGUI System
	// The root panel for everything ( NULL parent makes it a child of the embedded panel )

	// Ideal hierarchy:

	COM_TimestampedLog( "Building Panels (staticPanel)" );

	// Root -- staticPanel
	//		staticBackgroundImagePanel (from gamui) zpos == 0
	//      staticClientDLLPanel ( zpos == 25 )
	//		staticClientDLLToolsPanel ( zpos == 28
	//		staticGameDLLPanel ( zpos == 30 )
	//		staticEngineToolsPanel ( zpos == 75 )
	//		staticGameUIPanel ( GameUI stuff ) ( zpos == 100 )
	//		staticDebugSystemPanel ( Engine debug stuff ) zpos == 125 )

	staticPanel = new CStaticPanel( NULL, "staticPanel" );	
	staticPanel->SetBounds( 0, 0, videomode->GetModeUIWidth(), videomode->GetModeUIHeight() );
	staticPanel->SetPaintBorderEnabled(false);
	staticPanel->SetPaintBackgroundEnabled(false);
	staticPanel->SetPaintEnabled(false);
	staticPanel->SetVisible(true);
	staticPanel->SetCursor( vgui::dc_none );
	staticPanel->SetZPos(0);
	staticPanel->SetVisible(true);
	staticPanel->SetParent( vgui::surface()->GetEmbeddedPanel() );

	COM_TimestampedLog( "Building Panels (staticClientDLLPanel)" );

	staticClientDLLPanel = new CEnginePanel( staticPanel, "staticClientDLLPanel" );
	staticClientDLLPanel->SetBounds( 0, 0, videomode->GetModeUIWidth(), videomode->GetModeUIHeight() );
	staticClientDLLPanel->SetPaintBorderEnabled(false);
	staticClientDLLPanel->SetPaintBackgroundEnabled(false);
	staticClientDLLPanel->SetKeyBoardInputEnabled( false );	// popups in the client DLL can enable this.
	staticClientDLLPanel->SetPaintEnabled(false);
	staticClientDLLPanel->SetVisible( false );
	staticClientDLLPanel->SetCursor( vgui::dc_none );
	staticClientDLLPanel->SetZPos( 25 );

	CreateAskConnectPanel( staticPanel->GetVPanel() );

	COM_TimestampedLog( "Building Panels (staticClientDLLToolsPanel)" );

	staticClientDLLToolsPanel = new CEnginePanel( staticPanel, "staticClientDLLToolsPanel" );
	staticClientDLLToolsPanel->SetBounds( 0, 0, videomode->GetModeUIWidth(), videomode->GetModeUIHeight() );
	staticClientDLLToolsPanel->SetPaintBorderEnabled(false);
	staticClientDLLToolsPanel->SetPaintBackgroundEnabled(false);
	staticClientDLLToolsPanel->SetKeyBoardInputEnabled( false );	// popups in the client DLL can enable this.
	staticClientDLLToolsPanel->SetPaintEnabled(false);
	staticClientDLLToolsPanel->SetVisible( true );
	staticClientDLLToolsPanel->SetCursor( vgui::dc_none );
	staticClientDLLToolsPanel->SetZPos( 28 );

	staticEngineToolsPanel = new CEnginePanel( staticPanel, "Engine Tools" );
	staticEngineToolsPanel->SetBounds( 0, 0, videomode->GetModeUIWidth(), videomode->GetModeUIHeight() );
	staticEngineToolsPanel->SetPaintBorderEnabled(false);
	staticEngineToolsPanel->SetPaintBackgroundEnabled(false);
	staticEngineToolsPanel->SetPaintEnabled(false);
	staticEngineToolsPanel->SetVisible( true );
	staticEngineToolsPanel->SetCursor( vgui::dc_none );
	staticEngineToolsPanel->SetZPos( 75 );

	COM_TimestampedLog( "Building Panels (staticGameUIPanel)" );

	staticGameUIPanel = new CEnginePanel( staticPanel, "GameUI Panel" );
	staticGameUIPanel->SetBounds( 0, 0, videomode->GetModeUIWidth(), videomode->GetModeUIHeight() );
	staticGameUIPanel->SetPaintBorderEnabled(false);
	staticGameUIPanel->SetPaintBackgroundEnabled(false);
	staticGameUIPanel->SetPaintEnabled(false);
	staticGameUIPanel->SetVisible( true );
	staticGameUIPanel->SetCursor( vgui::dc_none );
	staticGameUIPanel->SetZPos( 100 );

	staticGameDLLPanel = new CEnginePanel( staticPanel, "staticGameDLLPanel" );
	staticGameDLLPanel->SetBounds( 0, 0, videomode->GetModeUIWidth(), videomode->GetModeUIHeight() );
	staticGameDLLPanel->SetPaintBorderEnabled(false);
	staticGameDLLPanel->SetPaintBackgroundEnabled(false);
	staticGameDLLPanel->SetKeyBoardInputEnabled( false );	// popups in the game DLL can enable this.
	staticGameDLLPanel->SetPaintEnabled(false);
	staticGameDLLPanel->SetCursor( vgui::dc_none );
	staticGameDLLPanel->SetZPos( 135 );

	if ( CommandLine()->CheckParm( "-tools" ) != NULL )
	{
		staticGameDLLPanel->SetVisible( true );
	}
	else
	{
		staticGameDLLPanel->SetVisible( false );
	}

	if ( IsPC() )
	{
		COM_TimestampedLog( "Building Panels (staticDebugSystemPanel)" );

		staticDebugSystemPanel = new CDebugSystemPanel( staticPanel, "Engine Debug System" );
		staticDebugSystemPanel->SetZPos( 125 );

		// Install demo playback/editing UI
		CDemoUIPanel::InstallDemoUI( staticEngineToolsPanel );
		CDemoUIPanel2::Install( staticClientDLLPanel, staticEngineToolsPanel, true );

		// Install fog control panel UI
		CFogUIPanel::InstallFogUI( staticEngineToolsPanel );

		// Install texture view panel
		TxViewPanel::Install( staticEngineToolsPanel );

		COM_TimestampedLog( "Install bug reporter" );

		// Create and initialize bug reporting system
		bugreporter->InstallBugReportingUI( staticGameUIPanel, IEngineBugReporter::BR_AUTOSELECT );
		bugreporter->Init();

		COM_TimestampedLog( "Install perf tools" );

		// Create a performance toolkit system
		perftools->InstallPerformanceToolsUI( staticEngineToolsPanel );
		perftools->Init();

		// Create a color correction UI
		colorcorrectiontools->InstallColorCorrectionUI( staticEngineToolsPanel );
		colorcorrectiontools->Init();
	}

	// Make sure this is on top of everything
	staticFocusOverlayPanel = new CFocusOverlayPanel( staticPanel, "FocusOverlayPanel" );
	staticFocusOverlayPanel->SetBounds( 0, 0, videomode->GetModeUIWidth(), videomode->GetModeUIHeight() );
	staticFocusOverlayPanel->SetZPos( 150 );
	staticFocusOverlayPanel->MoveToFront();

	// Create engine vgui panels
	if ( IsPC() )
	{
		Con_CreateConsolePanel( staticEngineToolsPanel );
		CL_CreateEntityReportPanel( staticEngineToolsPanel );
		VGui_CreateDrawTreePanel( staticEngineToolsPanel );
		CL_CreateTextureListPanel( staticEngineToolsPanel );
		CreateVProfPanels( staticEngineToolsPanel );
	}
	staticEngineToolsPanel->LoadControlSettings( "scripts/EngineVGuiLayout.res" );

	COM_TimestampedLog( "materials->CacheUsedMaterials()" );

	// Make sure that these materials are in the materials cache
	materials->CacheUsedMaterials();

	COM_TimestampedLog( "g_pVGuiLocalize->AddFile" );

	// load the base localization file
	g_pVGuiLocalize->AddFile( "Resource/valve_%language%.txt" );

	COM_TimestampedLog( "staticGameUIFuncs->Initialize" );

	staticGameUIFuncs->Initialize( g_AppSystemFactory );

	COM_TimestampedLog( "staticGameUIFuncs->Start" );
	staticGameUIFuncs->Start();

	// don't need to load the "valve" localization file twice
	// Each mod acn have its own language.txt in addition to the valve_%%langauge%%.txt file under defaultgamedir.
	// load mod-specific localization file for kb_act.lst, user.scr, settings.scr, etc.
	char szFileName[MAX_PATH];
	Q_snprintf( szFileName, sizeof( szFileName ) - 1, "resource/%s_%%language%%.txt", GetCurrentMod() );
	szFileName[ sizeof( szFileName ) - 1 ] = '\0';
	g_pVGuiLocalize->AddFile( szFileName );

	// setup console
	if ( staticGameConsole )
	{
		staticGameConsole->Initialize();
		staticGameConsole->SetParent(staticGameUIPanel->GetVPanel());
	}

	if ( IsX360() )
	{
		// provide an interface for loader to send progress notifications
		g_pQueuedLoader->InstallProgress( &s_LoaderProgress ); 
	}

	// show the game UI
	COM_TimestampedLog( "ActivateGameUI()" );
	ActivateGameUI();

	if ( staticGameConsole && 
		!CommandLine()->CheckParm( "-forcestartupmenu" ) && 
		!CommandLine()->CheckParm( "-hideconsole" ) &&
		( CommandLine()->FindParm( "-toconsole" ) || CommandLine()->FindParm( "-console" ) || CommandLine()->FindParm( "-rpt" ) || CommandLine()->FindParm( "-allowdebug" ) ) )
	{
		// activate the console
		staticGameConsole->Activate();
	}

	m_bNoShaderAPI = CommandLine()->FindParm( "-noshaderapi" );
}

void CEngineVGui::PostInit()
{
	staticGameUIFuncs->PostInit();
#if defined( _X360 )
	g_pMatSystemSurface->ClearTemporaryFontCache();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: connects interfaces in gameui
//-----------------------------------------------------------------------------
void CEngineVGui::Connect()
{
	m_pInputInternal = (vgui::IInputInternal *)g_GameSystemFactory( VGUI_INPUTINTERNAL_INTERFACE_VERSION,  NULL );
	staticGameUIFuncs->Connect( g_GameSystemFactory );

}

//-----------------------------------------------------------------------------
// Create/destroy the vprof panels
//-----------------------------------------------------------------------------
void CEngineVGui::CreateVProfPanels( vgui::Panel *pParent )
{
	if ( IsX360() )
		return;

#ifdef VPROF_ENABLED
	m_pVProfPanel = new CVProfPanel( pParent, "VProfPanel" );
	m_pBudgetPanel = new CBudgetPanelEngine( pParent, "BudgetPanel" );
	CreateVProfGraphPanel( pParent );
	m_pTextureBudgetPanel = new CTextureBudgetPanel( pParent, "TextureBudgetPanel" );
#endif
}

void CEngineVGui::DestroyVProfPanels( )
{
	if ( IsX360() )
		return;

#ifdef VPROF_ENABLED
	if ( m_pVProfPanel )
	{
		delete m_pVProfPanel;
		m_pVProfPanel = NULL;
	}
	if ( m_pBudgetPanel )
	{
		delete m_pBudgetPanel;
		m_pBudgetPanel = NULL;
	}
	DestroyVProfGraphPanel();

	if ( m_pTextureBudgetPanel )
	{
		delete m_pTextureBudgetPanel;
		m_pTextureBudgetPanel = NULL;
	}
#endif
}


//-----------------------------------------------------------------------------
// Are we initialized?
//-----------------------------------------------------------------------------
bool CEngineVGui::IsInitialized() const
{
	return staticPanel != NULL;
}

extern bool g_bUsingLegacyAppSystems;
//-----------------------------------------------------------------------------
// Purpose: Called to Shutdown the game UI system
//-----------------------------------------------------------------------------
void CEngineVGui::Shutdown()
{
	if ( IsPC() && CL_IsHL2Demo() ) // if they are playing the demo then open the storefront on shutdown
	{
		vgui::system()->ShellExecute("open", "steam://store_demo/220");
	}

	if ( IsPC() && CL_IsPortalDemo() ) // if they are playing the demo then open the storefront on shutdown
	{
		vgui::system()->ShellExecute("open", "steam://store_demo/400");
	}

	DestroyVProfPanels();
	bugreporter->Shutdown();
	colorcorrectiontools->Shutdown();
	perftools->Shutdown();

	demoaction->Shutdown();

	if ( g_PluginManager )
	{
		g_PluginManager->Shutdown();
	}

	// HACK HACK: There was a bug in the old versions of the viewport which would crash in the case where the client .dll hadn't been fully unloaded, so
	//  we'll leak this panel here instead!!!
	if ( g_bUsingLegacyAppSystems )
	{
		staticClientDLLPanel->SetParent( (vgui::VPANEL)0 );
	}

	// This will delete the engine subpanel since it's a child
	delete staticPanel;
	staticPanel = NULL;
	staticClientDLLToolsPanel = NULL;
	staticClientDLLPanel	= NULL;
	staticEngineToolsPanel = NULL;
	staticDebugSystemPanel = NULL;
	staticFocusOverlayPanel = NULL;
	staticGameDLLPanel = NULL;

	// Give panels a chance to settle so things
	//  Marked for deletion will actually get deleted
	vgui::ivgui()->RunFrame();

	// unload the gameUI
	staticGameUIFuncs->Shutdown();

	staticGameUIFuncs = NULL;
	staticGameConsole = NULL;
	staticGameUIPanel = NULL;

	// stop the App running
	vgui::ivgui()->Stop();
	
	// unload the dll
	Sys_UnloadModule(m_hStaticGameUIModule);
	m_hStaticGameUIModule = NULL;
	m_GameUIFactory = NULL;
	m_pInputInternal = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Retrieve specified root panel
//-----------------------------------------------------------------------------
inline vgui::Panel *CEngineVGui::GetRootPanel( VGuiPanel_t type )
{
	if ( sv.IsDedicated() )
	{
		return NULL;
	}

	switch ( type )
	{
	default:
	case PANEL_ROOT:
		return staticPanel;
	case PANEL_CLIENTDLL:
		return staticClientDLLPanel;
	case PANEL_GAMEUIDLL:
		return staticGameUIPanel;
	case PANEL_TOOLS:
		return staticEngineToolsPanel;
	case PANEL_GAMEDLL:
		return staticGameDLLPanel;
	case PANEL_CLIENTDLL_TOOLS:
		return staticClientDLLToolsPanel;
	}
}

vgui::VPANEL CEngineVGui::GetPanel( VGuiPanel_t type )
{
	return GetRootPanel( type )->GetVPanel();
}

//-----------------------------------------------------------------------------
// Purpose: Toggle engine panel active/inactive
//-----------------------------------------------------------------------------
void CEngineVGui::SetEngineVisible( bool state )
{
	if ( staticClientDLLPanel )
	{
		staticClientDLLPanel->SetVisible( state );
	}
}


//-----------------------------------------------------------------------------
// Should pause?
//-----------------------------------------------------------------------------
bool CEngineVGui::ShouldPause()
{
	if ( IsPC() )
	{
		return bugreporter->ShouldPause() || perftools->ShouldPause();
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEngineVGui::ConfirmQuit()
{
	ActivateGameUI();
	staticGameUIFuncs->OnConfirmQuit();
}
	
//-----------------------------------------------------------------------------
// Purpose: Shows any GameUI related panels
//-----------------------------------------------------------------------------
void CEngineVGui::ActivateGameUI()
{
	if ( m_bNotAllowedToShowGameUI )
		return;

	if (!staticGameUIFuncs)
		return;

#if defined( REPLAY_ENABLED )
	// Don't allow the game UI to be activated when a replay is being rendered
	if ( g_pReplayMovieManager && g_pReplayMovieManager->IsRendering() )
		return;
#endif

	// clear any keys that might be stuck down
	ClearIOStates();

	staticGameUIPanel->SetVisible(true);
	staticGameUIPanel->MoveToFront();	

	staticClientDLLPanel->SetVisible(false);
	staticClientDLLPanel->SetMouseInputEnabled(false);

	vgui::surface()->SetCursor( vgui::dc_arrow );

	//staticGameDLLPanel->SetVisible( true );
	//staticGameDLLPanel->SetMouseInputEnabled( true );

	SetEngineVisible( false );

	staticGameUIFuncs->OnGameUIActivated();
}

//-----------------------------------------------------------------------------
// Purpose: Hides an Game UI related features (not client UI stuff tho!)
//-----------------------------------------------------------------------------
bool CEngineVGui::HideGameUI()
{
	if ( m_bNotAllowedToHideGameUI )
		return false;

	const char *levelName = engineClient->GetLevelName();
	bool bInNonBgLevel = levelName && levelName[0] && !engineClient->IsLevelMainMenuBackground();
	if ( bInNonBgLevel )
	{
		staticGameUIPanel->SetVisible(false);
		staticGameUIPanel->SetPaintBackgroundEnabled(false);
	
		staticClientDLLPanel->SetVisible(true);
		staticClientDLLPanel->MoveToFront();
		staticClientDLLPanel->SetMouseInputEnabled(true);

		//staticGameDLLPanel->SetVisible( false );
		//staticGameDLLPanel->SetMouseInputEnabled(false);

		SetEngineVisible( true );

		staticGameUIFuncs->OnGameUIHidden();
	}
	else
	{
		// Tracker 18820:  Pulling up options/console was perma-pausing the background levels, now we
		//  unpause them when you hit the Esc key even though the UI remains...
		if ( levelName && 
			 levelName[0] && 
			 ( engineClient->GetMaxClients() <= 1 ) && 
			 engineClient->IsPaused() )
		{
			Cbuf_AddText("unpause\n");
		}
	}

	VGui_MoveDrawTreePanelToFront();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Hides the game console (but not the complete GameUI!)
//-----------------------------------------------------------------------------
void CEngineVGui::HideConsole()
{
	if ( IsX360() )
		return;

	if ( staticGameConsole )
	{
		staticGameConsole->Hide();
	}
}

//-----------------------------------------------------------------------------
// Purpose: shows the console
//-----------------------------------------------------------------------------
void CEngineVGui::ShowConsole()
{
	if ( IsX360() )
		return;

	ActivateGameUI();

	if ( staticGameConsole )
	{
		staticGameConsole->Activate();
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the console is currently open
//-----------------------------------------------------------------------------
bool CEngineVGui::IsConsoleVisible()
{
	if ( IsPC() )
	{
		return IsGameUIVisible() && staticGameConsole && staticGameConsole->IsConsoleVisible();
	}
	else
	{
		// xbox has no drop down console
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: clears all text from the console
//-----------------------------------------------------------------------------
void CEngineVGui::ClearConsole()
{
	if ( staticGameConsole )
	{
		staticGameConsole->Clear();
	}
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CEngineVGui::IsGameUIVisible() 
{
	return staticGameUIPanel && staticGameUIPanel->IsVisible();
}


// list of progress bar strings
struct LoadingProgressDescription_t
{
	LevelLoadingProgress_e eProgress;	// current progress
	int nPercent;						// % of the total time this is at
	int nRepeat;						// number of times this is expected to repeat (usually 0)
	const char *pszDesc;				// user description of progress
};

LoadingProgressDescription_t g_ListenServerLoadingProgressDescriptions[] =
{	
	{ PROGRESS_NONE,						0,		0,		NULL },
	{ PROGRESS_SPAWNSERVER,					2,		0,		"#LoadingProgress_SpawningServer" },
	{ PROGRESS_LOADWORLDMODEL,				4,		7,		"#LoadingProgress_LoadMap" },
	{ PROGRESS_CREATENETWORKSTRINGTABLES,	23,		0,		NULL },
	{ PROGRESS_PRECACHEWORLD,				23,		0,		"#LoadingProgress_PrecacheWorld" },
	{ PROGRESS_CLEARWORLD,					23,		0,		NULL },
	{ PROGRESS_LEVELINIT,					34,		0,		"#LoadingProgress_LoadResources" },
	{ PROGRESS_PRECACHE,					35,		239,	NULL },
	{ PROGRESS_ACTIVATESERVER,				68,		0,		NULL },
	{ PROGRESS_SIGNONCHALLENGE,				68,		0,		NULL },
	{ PROGRESS_SIGNONCONNECT,				70,		0,		NULL },
	{ PROGRESS_SIGNONCONNECTED,				73,		0,		"#LoadingProgress_SignonLocal" },
	{ PROGRESS_PROCESSSERVERINFO,			75,		0,		NULL },
	{ PROGRESS_PROCESSSTRINGTABLE,			77,		12,		NULL },	// 16
	{ PROGRESS_SIGNONNEW,					84,		0,		NULL },
	{ PROGRESS_SENDCLIENTINFO,				88,		0,		NULL },
	{ PROGRESS_SENDSIGNONDATA,				91,		0,		"#LoadingProgress_SignonDataLocal" },
	{ PROGRESS_SIGNONSPAWN,					94,		0,		NULL },
	{ PROGRESS_FULLYCONNECTED,				97,		0,		NULL },
	{ PROGRESS_READYTOPLAY,					99,		0,		NULL },
	{ PROGRESS_HIGHESTITEM,					100,	0,		NULL },
};

LoadingProgressDescription_t g_RemoteConnectLoadingProgressDescriptions[] =
{	
	{ PROGRESS_NONE,						0,		0,		NULL },
	{ PROGRESS_CHANGELEVEL,					1,		0,		"#LoadingProgress_Changelevel" },
	{ PROGRESS_BEGINCONNECT,				5,		0,		"#LoadingProgress_BeginConnect" },
	{ PROGRESS_SIGNONCHALLENGE,				10,		0,		"#LoadingProgress_Connecting" },
	{ PROGRESS_SIGNONCONNECTED,				15,		0,		NULL },
	{ PROGRESS_PROCESSSERVERINFO,			20,		0,		"#LoadingProgress_ProcessServerInfo" },
	{ PROGRESS_PROCESSSTRINGTABLE,			25,		11,		NULL },
	{ PROGRESS_LOADWORLDMODEL,				45,		7,		"#LoadingProgress_LoadMap" },
	{ PROGRESS_SIGNONNEW,					75,		0,		NULL },
	{ PROGRESS_SENDCLIENTINFO,				80,		0,		"#LoadingProgress_SendClientInfo" },
	{ PROGRESS_SENDSIGNONDATA,				85,		0,		"#LoadingProgress_SignonData" },
	{ PROGRESS_SIGNONSPAWN,					90,		0,		NULL },
	{ PROGRESS_FULLYCONNECTED,				95,		0,		NULL },
	{ PROGRESS_READYTOPLAY,					99,		0,		NULL },
	{ PROGRESS_HIGHESTITEM,					100,	0,		NULL },
};

static LoadingProgressDescription_t *g_pLoadingProgressDescriptions = NULL;

//-----------------------------------------------------------------------------
// Purpose: returns current progress point description
//-----------------------------------------------------------------------------
LoadingProgressDescription_t &GetProgressDescription(int eProgress)
{
	// search for the item in the current list
	int i = 0;
	while (g_pLoadingProgressDescriptions[i].eProgress != PROGRESS_HIGHESTITEM)
	{
		// find the closest match
		if (g_pLoadingProgressDescriptions[i].eProgress >= eProgress)
			return g_pLoadingProgressDescriptions[i];
	
		++i;
	}

	// not found
	return g_pLoadingProgressDescriptions[0];
}

//-----------------------------------------------------------------------------
// Purpose: transition handler
//-----------------------------------------------------------------------------
void CEngineVGui::OnLevelLoadingStarted()
{
	if (!staticGameUIFuncs)
		return;

	ConVar *pSyncReportConVar = g_pCVar->FindVar( "fs_report_sync_opens" );
	if ( pSyncReportConVar )
	{
		// If convar is set to 2, suppress warnings during level load
		g_syncReportLevel = pSyncReportConVar->GetInt();
		if ( g_syncReportLevel > 1 )
		{
			pSyncReportConVar->SetValue( 0 );
		}
	}
	
	if ( IsX360() )
	{
		// TCR requirement, always!!!
		m_bShowProgressDialog = true;
	}

	// we've starting loading a level/connecting to a server
	staticGameUIFuncs->OnLevelLoadingStarted( m_bShowProgressDialog );

	// reset progress bar timers
	m_flLoadingStartTime = Plat_FloatTime();
	m_LoadingProgress.RemoveAll();
	m_eLastProgressPoint = PROGRESS_NONE;
	m_nLastProgressPointRepeatCount = 0;
	m_ProgressBias = 0;

	// choose which progress bar to use
	if (NET_IsMultiplayer())
	{
		// we're connecting
		g_pLoadingProgressDescriptions = g_RemoteConnectLoadingProgressDescriptions;
	}
	else
	{
		g_pLoadingProgressDescriptions = g_ListenServerLoadingProgressDescriptions;
	}

	if ( m_bShowProgressDialog )
	{
		ActivateGameUI();
	}

	m_bShowProgressDialog = false;
}

//-----------------------------------------------------------------------------
// Purpose: transition handler
//-----------------------------------------------------------------------------
void CEngineVGui::OnLevelLoadingFinished()
{
	if (!staticGameUIFuncs)
		return;

	staticGameUIFuncs->OnLevelLoadingFinished( gfExtendedError, gszDisconnectReason, gszExtendedDisconnectReason );
	m_eLastProgressPoint = PROGRESS_NONE;

	// clear any error message
	gfExtendedError = false;
	gszDisconnectReason[0] = 0;
	gszExtendedDisconnectReason[0] = 0;

#if defined(ENABLE_LOADING_PROGRESS_PROFILING)
	// display progress bar stats (for debugging/tuning progress bar)
	float flEndTime = (float)Plat_FloatTime();
	// add a finished entry
	LoadingProgressEntry_t &entry = m_LoadingProgress[m_LoadingProgress.AddToTail()];
	entry.flTime = flEndTime - m_flLoadingStartTime;
	entry.eProgress = PROGRESS_HIGHESTITEM;
	// dump the info
	Msg("Level load timings:\n");
	float flTotalTime = flEndTime - m_flLoadingStartTime;
	int nRepeatCount = 0;
	float flTimeTaken = 0.0f;
	float flFirstLoadProgressTime = 0.0f;
	for (int i = 0; i < m_LoadingProgress.Count() - 1; i++)
	{
		// keep track of time
		flTimeTaken += (float)m_LoadingProgress[i+1].flTime - m_LoadingProgress[i].flTime;

		// keep track of how often something is repeated
		if (m_LoadingProgress[i+1].eProgress == m_LoadingProgress[i].eProgress)
		{
			if (nRepeatCount == 0)
			{
				flFirstLoadProgressTime = m_LoadingProgress[i].flTime;
			}
			++nRepeatCount;
			continue;
		}

		// work out the time it took to do this
		if (nRepeatCount == 0)
		{
			flFirstLoadProgressTime = m_LoadingProgress[i].flTime;
		}

		int nPerc = (int)(100 * (flFirstLoadProgressTime / flTotalTime));
		int nTickPerc = (int)(100 * ((float)m_LoadingProgress[i].eProgress / (float)PROGRESS_HIGHESTITEM));
		
		// interpolated percentage is in between the real times and the most ticks
		int nInterpPerc = (nPerc + nTickPerc) / 2;
		Msg("\t%d\t%.3f\t\ttime: %d%%\t\tinterp: %d%%\t\trepeat: %d\n", m_LoadingProgress[i].eProgress, flTimeTaken, nPerc, nInterpPerc, nRepeatCount);

		// reset accumlated vars
		nRepeatCount = 0;
		flTimeTaken = 0.0f;
	}
#endif // ENABLE_LOADING_PROGRESS_PROFILING

	HideGameUI();

	// Restore convar setting after level load
	if ( g_syncReportLevel > 1 )
	{
		ConVar *pSyncReportConVar = g_pCVar->FindVar( "fs_report_sync_opens" );
		if ( pSyncReportConVar )
		{
			pSyncReportConVar->SetValue( g_syncReportLevel );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: transition handler
//-----------------------------------------------------------------------------
void CEngineVGui::ShowErrorMessage()
{
	if (!staticGameUIFuncs || !gfExtendedError)
		return;

	staticGameUIFuncs->OnLevelLoadingFinished( gfExtendedError, gszDisconnectReason, gszExtendedDisconnectReason );
	m_eLastProgressPoint = PROGRESS_NONE;

	// clear any error message
	gfExtendedError = false;
	gszDisconnectReason[0] = 0;
	gszExtendedDisconnectReason[0] = 0;

	HideGameUI();
}


//-----------------------------------------------------------------------------
// Purpose: Updates progress
//-----------------------------------------------------------------------------
void CEngineVGui::UpdateProgressBar(LevelLoadingProgress_e progress)
{
	if (!staticGameUIFuncs)
		return;

	if ( !ThreadInMainThread() )
		return;

#if defined(ENABLE_LOADING_PROGRESS_PROFILING)
	// track the progress times, for debugging & tuning
	LoadingProgressEntry_t &entry = m_LoadingProgress[m_LoadingProgress.AddToTail()];
	entry.flTime = Plat_FloatTime() - m_flLoadingStartTime;
	entry.eProgress = progress;
#endif

	if (!g_pLoadingProgressDescriptions)
		return;

	// don't go backwards
	if (progress < m_eLastProgressPoint)
		return;

	// count progress repeats
	if (progress == m_eLastProgressPoint)
	{
		++m_nLastProgressPointRepeatCount;
	}
	else
	{
		m_nLastProgressPointRepeatCount = 0;
	}

	// construct a string describing it
	LoadingProgressDescription_t &desc = GetProgressDescription(progress);

	// calculate partial progress
	float flPerc = desc.nPercent / 100.0f;
	if ( desc.nRepeat > 1 && m_nLastProgressPointRepeatCount )
	{
		// cap the repeat count
		m_nLastProgressPointRepeatCount = min(m_nLastProgressPointRepeatCount, desc.nRepeat);

		// next progress point
		float flNextPerc = GetProgressDescription(progress + 1).nPercent / 100.0f;

		// move along partially towards the next tick
		flPerc += (flNextPerc - flPerc) * ((float)m_nLastProgressPointRepeatCount / desc.nRepeat);
	}

	// the bias allows the loading bar to have an optional reserved initial band
	// isolated from the normal progress descriptions
	flPerc = flPerc * ( 1.0f - m_ProgressBias ) + m_ProgressBias;

	if ( staticGameUIFuncs->UpdateProgressBar( flPerc, desc.pszDesc ) )
	{
		// re-render vgui on screen
		extern void V_RenderVGuiOnly();
		V_RenderVGuiOnly();
	}

	m_eLastProgressPoint = progress;
}

//-----------------------------------------------------------------------------
// Purpose: Updates progress
//-----------------------------------------------------------------------------
void CEngineVGui::UpdateCustomProgressBar( float progress, const wchar_t *desc )
{
	if (!staticGameUIFuncs)
		return;

	char ansi[1024];
	g_pVGuiLocalize->ConvertUnicodeToANSI( desc, ansi, sizeof( ansi ) );

	if ( staticGameUIFuncs->UpdateProgressBar( progress, ansi ) )
	{
		// re-render vgui on screen
		extern void V_RenderVGuiOnly();
		V_RenderVGuiOnly();
	}
}

void CEngineVGui::StartCustomProgress()
{
	if (!staticGameUIFuncs)
		return;

	// we've starting loading a level/connecting to a server
	staticGameUIFuncs->OnLevelLoadingStarted(true);
	m_bSaveProgress = staticGameUIFuncs->SetShowProgressText( true );
}

void CEngineVGui::FinishCustomProgress()
{
	if (!staticGameUIFuncs)
		return;

	staticGameUIFuncs->SetShowProgressText( m_bSaveProgress );
	staticGameUIFuncs->OnLevelLoadingFinished( false, "", "" );
}

void CEngineVGui::SetProgressBias( float bias )
{
	m_ProgressBias = bias;
}

void CEngineVGui::UpdateProgressBar( float progress )
{
	if ( !staticGameUIFuncs )
		return;

	if ( staticGameUIFuncs->UpdateProgressBar( progress, "" ) )
	{
		// re-render vgui on screen
		extern void V_RenderVGuiOnly();
		V_RenderVGuiOnly();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Returns 1 if the key event is handled, 0 if the engine should handle it
//-----------------------------------------------------------------------------
void CEngineVGui::UpdateButtonState( const InputEvent_t &event )
{
	m_pInputInternal->UpdateButtonState( event );
}

		
//-----------------------------------------------------------------------------
// Purpose: Returns 1 if the key event is handled, 0 if the engine should handle it
//-----------------------------------------------------------------------------
bool CEngineVGui::Key_Event( const InputEvent_t &event )
{
	bool bDown = event.m_nType != IE_ButtonReleased;
	ButtonCode_t code = (ButtonCode_t)event.m_nData;

	if ( IsPC() && IsShiftKeyDown() )
	{
		switch( code )
		{
		case KEY_F1:
			if ( bDown )
			{
				Cbuf_AddText( "debugsystemui" );
			}
			return true;

		case KEY_F2:
			if ( bDown )
			{
				Cbuf_AddText( "demoui" );
			}
			return true;
		}
	}

#if defined( _WIN32 )
	// Ignore alt tilde, since the Japanese IME uses this to toggle itself on/off
	if ( IsPC() && code == KEY_BACKQUOTE && ( IsAltKeyDown() || IsCtrlKeyDown() ) )
		return true;
#endif
			   
	// ESCAPE toggles game ui
	if ( bDown && ( code == KEY_ESCAPE || code == KEY_XBUTTON_START || code == STEAMCONTROLLER_START) && !g_ClientDLL->HandleUiToggle() )
	{
		if ( IsPC() )
		{
			if ( IsGameUIVisible()  )
			{
				// Don't allow hiding of the game ui if there's no level
				const char *pLevelName = engineClient->GetLevelName();
				if ( pLevelName && pLevelName[0] )
				{
					Cbuf_AddText( "gameui_hide" );
					if ( IsDebugSystemVisible() )
					{
						Cbuf_AddText( "debugsystemui 0" );
					}
				}
			}
			else
			{
				Cbuf_AddText( "gameui_activate" );
			}
			return true;
		}
		if ( IsX360() && !IsGameUIVisible() )
		{
			// 360 UI does not toggle, engine does "show", but UI needs to handle "hide"
			Cbuf_AddText( "gameui_activate" );
			return true;
		}
	}

	if ( g_pMatSystemSurface && g_pMatSystemSurface->HandleInputEvent( event ) )
	{
		// always let the engine handle the console keys
		// FIXME: Do a lookup of the key bound to toggleconsole
		// want to cache it off so the lookup happens only when keys are bound?
		if ( IsPC() && ( code == KEY_BACKQUOTE ) )
			return false;
		return true;
	}
	return false;
}

void CEngineVGui::Simulate()
{
	toolframework->VGui_PreSimulateAllTools();

	if ( staticPanel )
	{
		VPROF_BUDGET( "CEngineVGui::Simulate", VPROF_BUDGETGROUP_OTHER_VGUI );

		// update vgui animations
		//!! currently this has to be done once per dll, because the anim controller object is in a lib;
		//!! need to make it globally pumped (gameUI.dll has it's own version of this)
		vgui::GetAnimationController()->UpdateAnimations( Sys_FloatTime() );

		int w, h;
#if defined( USE_SDL )
		uint width,height;
		g_pLauncherMgr->RenderedSize( width, height, false );	// false = get
		w = width;
		h = height;
#else
		if ( ::IsIconic( *pmainwindow ) )
		{
			w = videomode->GetModeWidth();
			h = videomode->GetModeHeight();
		}
		else
		{
			RECT rect;
			::GetClientRect(*pmainwindow, &rect);

			w = rect.right;
			h = rect.bottom;
		}
#endif
		// don't hold this reference over RunFrame()
		{
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->Viewport( 0, 0, w, h );
		}

		staticGameUIFuncs->RunFrame();
		vgui::ivgui()->RunFrame();

		// Some debugging helpers
		DrawMouseFocus();
		VGui_UpdateDrawTreePanel();
		VGui_UpdateTextureListPanel();

		vgui::surface()->CalculateMouseVisible();
		VGui_ActivateMouse();
	}

//	if ( !vgui::ivgui()->IsRunning() )
//		Cbuf_AddText( "quit\n" );
	
	toolframework->VGui_PostSimulateAllTools();
}

void CEngineVGui::BackwardCompatibility_Paint()
{
	Paint( (PaintMode_t)(PAINT_UIPANELS | PAINT_INGAMEPANELS) );
}


//-----------------------------------------------------------------------------
// Purpose: paints all the vgui elements
//-----------------------------------------------------------------------------
void CEngineVGui::Paint( PaintMode_t mode )
{
	VPROF_BUDGET( "CEngineVGui::Paint", VPROF_BUDGETGROUP_OTHER_VGUI );

	if ( !staticPanel )
		return;

	// setup the base panel to cover the screen
	vgui::VPANEL pVPanel = vgui::surface()->GetEmbeddedPanel();
	if ( !pVPanel )
		return;

	bool drawVgui = r_drawvgui.GetBool();

	// Don't draw the console at all if vgui is off during a time demo
	if ( demoplayer->IsPlayingTimeDemo() && !drawVgui )
	{
		return;
	}

	if ( !drawVgui || m_bNoShaderAPI )
	{
		return;
	}

	// draw from the main panel down
	vgui::Panel *panel = staticPanel;

	// Force engine's root panel (staticPanel) to be full screen size
	{
		int x, y, w, h;
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->GetViewport( x, y, w, h );
		panel->SetBounds(0, 0, w, h); // ignore x and y here because the viewport takes care of that
	}

	panel->Repaint();

	toolframework->VGui_PreRenderAllTools( mode );

	// Paint both ( backward compatibility support )

	// It's either the full screen, or just the client .dll stuff
	if ( mode & PAINT_UIPANELS )
	{
		// Hide the client .dll, and paint everything else
		bool saveVisible = staticClientDLLPanel->IsVisible();
		bool saveToolsVisible = staticClientDLLToolsPanel->IsVisible();
		staticClientDLLPanel->SetVisible( false );
		staticClientDLLToolsPanel->SetVisible( false );

		vgui::surface()->PaintTraverseEx(pVPanel, true );

		staticClientDLLPanel->SetVisible( saveVisible );
		staticClientDLLToolsPanel->SetVisible( saveToolsVisible );
	}
	
	if ( mode & PAINT_INGAMEPANELS )
	{
		bool bSaveVisible = vgui::ipanel()->IsVisible( pVPanel );
		vgui::ipanel()->SetVisible( pVPanel, false );

		// Remove the client .dll from the main hierarchy so that popups will only paint for the client .dll here
		// NOTE: Disconnect each surface one at a time so that we don't draw popups twice

		// Paint the client .dll only
		vgui::VPANEL ingameRoot = staticClientDLLPanel->GetVPanel();
		vgui::VPANEL saveParent = vgui::ipanel()->GetParent( ingameRoot );
		vgui::ipanel()->SetParent( ingameRoot, 0 );
		vgui::surface()->PaintTraverseEx( ingameRoot, true );
		vgui::ipanel()->SetParent( ingameRoot, saveParent );

		// Overlay the client .dll tools next
		vgui::VPANEL ingameToolsRoot = staticClientDLLToolsPanel->GetVPanel();
		vgui::VPANEL saveToolParent = vgui::ipanel()->GetParent( ingameToolsRoot );
		vgui::ipanel()->SetParent( ingameToolsRoot, 0 );
		vgui::surface()->PaintTraverseEx( ingameToolsRoot, true );
		vgui::ipanel()->SetParent( ingameToolsRoot, saveToolParent );

		vgui::ipanel()->SetVisible( pVPanel, bSaveVisible );
	}

	if ( mode & PAINT_CURSOR )
	{
		vgui::surface()->PaintSoftwareCursor();
	}

	toolframework->VGui_PostRenderAllTools( mode );
}

bool CEngineVGui::IsDebugSystemVisible( void )
{
	return staticDebugSystemPanel ? staticDebugSystemPanel->IsVisible() : false;
}

void CEngineVGui::HideDebugSystem( void )
{
	if ( staticDebugSystemPanel )
	{
		staticDebugSystemPanel->SetVisible( false );
		SetEngineVisible( true );
	}
}


void CEngineVGui::ToggleDebugSystemUI( const CCommand &args )
{
	if ( !staticDebugSystemPanel )
		return;

	bool bVisible;
	if ( args.ArgC() == 1 )
	{
		// toggle the game UI
		bVisible = !IsDebugSystemVisible();
	}
	else
	{
		bVisible = atoi( args[1] ) != 0;
	}

	if ( !bVisible )
	{
		staticDebugSystemPanel->SetVisible( false );
		SetEngineVisible( true );
	}
	else
	{
		// clear any keys that might be stuck down
		ClearIOStates();
		staticDebugSystemPanel->SetVisible( true );
		SetEngineVisible( false );
	}
}

bool CEngineVGui::IsShiftKeyDown( void )
{
	if ( !vgui::input() )
		return false;

	return vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT );
}

bool CEngineVGui::IsAltKeyDown( void )
{
	if ( !vgui::input() )
		return false;

	return vgui::input()->IsKeyDown( KEY_LALT ) || vgui::input()->IsKeyDown( KEY_RALT );
}

bool CEngineVGui::IsCtrlKeyDown( void )
{
	if ( !vgui::input() )
		return false;

	return vgui::input()->IsKeyDown( KEY_LCONTROL ) || vgui::input()->IsKeyDown( KEY_RCONTROL );
}


//-----------------------------------------------------------------------------
// Purpose: notification
//-----------------------------------------------------------------------------
void CEngineVGui::NotifyOfServerConnect(const char *pchGame, int IP, int connectionPort, int queryPort)
{
	if (!staticGameUIFuncs)
		return;

	staticGameUIFuncs->OnConnectToServer2( pchGame, IP, connectionPort, queryPort);
}

//-----------------------------------------------------------------------------
// Purpose: notification
//-----------------------------------------------------------------------------
void CEngineVGui::NotifyOfServerDisconnect()
{
	if (!staticGameUIFuncs)
		return;

	staticGameUIFuncs->OnDisconnectFromServer( g_eSteamLoginFailure );
	g_eSteamLoginFailure = 0;
}

//-----------------------------------------------------------------------------
// Xbox 360: Matchmaking sessions send progress notifications to GameUI
//-----------------------------------------------------------------------------
void CEngineVGui::SessionNotification( const int notification, const int param )
{
	staticGameUIFuncs->SessionNotification( notification, param );
}

void CEngineVGui::SystemNotification( const int notification )
{
	staticGameUIFuncs->SystemNotification( notification );
}

//-----------------------------------------------------------------------------
// Xbox 360: Show a message/error dialog
//-----------------------------------------------------------------------------
void CEngineVGui::ShowMessageDialog( const uint nType, vgui::Panel *pOwner )
{
	ActivateGameUI();
	staticGameUIFuncs->ShowMessageDialog( nType, pOwner );
}

void CEngineVGui::UpdatePlayerInfo( uint64 nPlayerId, const char *pName, int nTeam, byte cVoiceState, int nPlayersNeeded, bool bHost )
{
	staticGameUIFuncs->UpdatePlayerInfo( nPlayerId, pName, nTeam, cVoiceState, nPlayersNeeded, bHost );
}

void CEngineVGui::SessionSearchResult( int searchIdx, void *pHostData, XSESSION_SEARCHRESULT *pResult, int ping )
{
	staticGameUIFuncs->SessionSearchResult( searchIdx, pHostData, pResult, ping );
}

//-----------------------------------------------------------------------------
// A helper to play sounds through vgui
//-----------------------------------------------------------------------------
void VGui_PlaySound( const char *pFileName )
{
	// Point at origin if they didn't specify a sound source.
	Vector vDummyOrigin;
	vDummyOrigin.Init();

	CSfxTable *pSound = (CSfxTable*)S_PrecacheSound(pFileName);
	if ( pSound )
	{
		S_MarkUISound( pSound );

		StartSoundParams_t params;
		params.staticsound = IsX360() ? true : false;
		params.soundsource = cl.m_nViewEntity;
		params.entchannel = CHAN_AUTO;
		params.pSfx = pSound;
		params.origin = vDummyOrigin;
		params.pitch = PITCH_NORM;
		params.soundlevel = SNDLVL_IDLE;
		params.flags = 0;
		params.fvol = 1.0f;

		S_StartSound( params );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGui_ActivateMouse()
{
	if ( !g_ClientDLL )
		return;

	// Don't mess with mouse if not active
	if ( !game->IsActiveApp() )
	{
		g_ClientDLL->IN_DeactivateMouse ();
		return;
	}
		
	/* 
	//
	// MIKE AND ALFRED: these panels should expose whether they want mouse input or not and 
	// CalculateMouseVisible will take them into account.
	//
	// If showing game ui, make sure nothing else is hooking it
	if ( Base().IsGameUIVisible() || Base().IsDebugSystemVisible() )
	{
		g_ClientDLL->IN_DeactivateMouse();
		return;
	}
	*/
			
	if ( vgui::surface()->IsCursorLocked() && !g_bTextMode )
	{
		g_ClientDLL->IN_ActivateMouse ();
	}
	else
	{
		g_ClientDLL->IN_DeactivateMouse ();
	}
}

static ConVar mat_drawTitleSafe( "mat_drawTitleSafe", "0", 0, "Enable title safe overlay" );

CUtlVector< vgui::VPANEL > g_FocusPanelList;

ConVar vgui_drawfocus( "vgui_drawfocus", "0", 0, "Report which panel is under the mouse." );

CFocusOverlayPanel::CFocusOverlayPanel( vgui::Panel *pParent, const char *pName ) : vgui::Panel( pParent, pName )
{
	SetPaintEnabled( false );
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );

	MakePopup();

	SetPostChildPaintEnabled( true );
	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );
}

bool CFocusOverlayPanel::DrawTitleSafeOverlay( void )
{
	if ( !mat_drawTitleSafe.GetBool() )
		return false;

	int backBufferWidth, backBufferHeight;
	materials->GetBackBufferDimensions( backBufferWidth, backBufferHeight );

	int x, y, x1, y1;

	// Required Title safe is TCR documented at inner 90% (RED)
	int insetX = 0.05f * backBufferWidth;
	int insetY = 0.05f * backBufferHeight;

	x = insetX;
	y = insetY;
	x1 = backBufferWidth - insetX;
	y1 = backBufferHeight - insetY;

	vgui::surface()->DrawSetColor( 255, 0, 0, 255 );
	vgui::surface()->DrawOutlinedRect( x, y, x1, y1 );


	// Suggested Title Safe is TCR documented at inner 85% (YELLOW)
	insetX = 0.075f * backBufferWidth;
	insetY = 0.075f * backBufferHeight;

	x = insetX;
	y = insetY;
	x1 = backBufferWidth - insetX;
	y1 = backBufferHeight - insetY;

	vgui::surface()->DrawSetColor( 255, 255, 0, 255 );
	vgui::surface()->DrawOutlinedRect( x, y, x1, y1 );	

	return true;
}

void CFocusOverlayPanel::PostChildPaint( void )
{
	BaseClass::PostChildPaint();

	bool bNeedsMoveToFront = false;

	if ( g_DrawTreeSelectedPanel )
	{
		int x, y, x1, y1;
		vgui::ipanel()->GetClipRect( g_DrawTreeSelectedPanel, x, y, x1, y1 );
		vgui::surface()->DrawSetColor( Color( 255, 0, 0, 255 ) );
		vgui::surface()->DrawOutlinedRect( x, y, x1, y1 );

		bNeedsMoveToFront = true;
	}

	if ( DrawTitleSafeOverlay() )
	{
		bNeedsMoveToFront = true;
	}

	if ( DrawFocusPanelList() )
	{
		bNeedsMoveToFront = true;
	}

	if ( bNeedsMoveToFront )
	{
		// will be valid for the next frame
		MoveToFront();
	}
}

bool CFocusOverlayPanel::DrawFocusPanelList( void )
{
	if( !vgui_drawfocus.GetBool() )
		return false;

	int c = g_FocusPanelList.Size();
	if ( c <= 0 )
		return false;

	int slot = 0;
	int fullscreeninset = 0;

	for ( int i = 0; i < c; i++ )
	{
		if ( slot > 31 )
			break;

		vgui::VPANEL vpanel = g_FocusPanelList[ i ];
		if ( !vpanel )
			continue;

		if ( !vgui::ipanel()->IsVisible( vpanel ) )
			return false;

		// Convert panel bounds to screen space
		int r, g, b;
		GetColorForSlot( slot, r, g, b );

		int x, y, x1, y1;
		vgui::ipanel()->GetClipRect( vpanel, x, y, x1, y1 );

		if ( (x1 - x) == videomode->GetModeUIWidth() && 
			 (y1 - y) == videomode->GetModeUIHeight() )
		{
			x += fullscreeninset;
			y += fullscreeninset;
			x1 -= fullscreeninset;
			y1 -= fullscreeninset;
			fullscreeninset++;
		}
		vgui::surface()->DrawSetColor( Color( r, g, b, 255 ) );
		vgui::surface()->DrawOutlinedRect( x, y, x1, y1 );

		slot++;
	}

	return true;
}


static void VGui_RecursiveFindPanels( CUtlVector< vgui::VPANEL  >& panelList, vgui::VPANEL check, char const *panelname )
{
	vgui::Panel *panel = vgui::ipanel()->GetPanel( check, "ENGINE" );
	if ( !panel )
		return;

	if ( !Q_strncmp( panel->GetName(), panelname, strlen( panelname ) ) )
	{
		panelList.AddToTail( panel->GetVPanel() );
	}

	int childcount = panel->GetChildCount();
	for ( int i = 0; i < childcount; i++ )
	{
		vgui::Panel *child = panel->GetChild( i );
		VGui_RecursiveFindPanels( panelList, child->GetVPanel(), panelname );
	}
}

void VGui_FindNamedPanels( CUtlVector< vgui::VPANEL >& panelList, char const *panelname )
{
	vgui::VPANEL embedded = vgui::surface()->GetEmbeddedPanel();

	// faster version of code below
	// checks through each popup in order, top to bottom windows
	int c = vgui::surface()->GetPopupCount();
	for (int i = c - 1; i >= 0; i--)
	{
		vgui::VPANEL popup = vgui::surface()->GetPopup(i);
		if ( !popup )
			continue;

		if ( embedded == popup )
			continue;

		VGui_RecursiveFindPanels( panelList, popup, panelname );
	}

	VGui_RecursiveFindPanels( panelList, embedded, panelname );
}

CON_COMMAND( vgui_togglepanel, "show/hide vgui panel by name." )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg( "Usage:  vgui_showpanel panelname\n" );
		return;
	}

	bool flip = false;
	bool fg = true;
	bool bg = true;

	if ( args.ArgC() == 5 )
	{
		flip = atoi( args[ 2 ] ) ? true : false;
		fg = atoi( args[ 3 ] ) ? true : false;
		bg = atoi( args[ 4 ] ) ? true : false;
	}
		
	char const *panelname = args[ 1 ];
	if ( !panelname || !panelname[ 0 ] )
		return;

	CUtlVector< vgui::VPANEL > panelList;

	VGui_FindNamedPanels( panelList, panelname );
	if ( !panelList.Size() )
	{
		ConMsg( "No panels starting with %s\n", panelname );
		return;
	}

	for ( int i = 0; i < panelList.Size(); i++ )
	{
		vgui::VPANEL p = panelList[ i ];
		if ( !p )
			continue;

		vgui::Panel *panel = vgui::ipanel()->GetPanel( p, "ENGINE");
		if ( !panel )
			continue;

		Msg( "Toggling %s\n", panel->GetName() );

		if ( fg )
		{
			panel->SetPaintEnabled( flip );
		}
		if ( bg )
		{
			panel->SetPaintBackgroundEnabled( flip );
		}
	}
}

static void VGui_RecursePanel( CUtlVector< vgui::VPANEL >& panelList, int x, int y, vgui::VPANEL check, bool include_hidden )
{
	if( !include_hidden && !vgui::ipanel()->IsVisible( check ) )
	{
		return;
	}

	if ( vgui::ipanel()->IsWithinTraverse( check, x, y, false ) )
	{
		panelList.AddToTail( check );
	}

	int childcount = vgui::ipanel()->GetChildCount( check );
	for ( int i = 0; i < childcount; i++ )
	{
		vgui::VPANEL child = vgui::ipanel()->GetChild( check, i );
		VGui_RecursePanel( panelList, x, y, child, include_hidden );
	}
}

void CEngineVGui::DrawMouseFocus( void )
{
	VPROF( "CEngineVGui::DrawMouseFocus" );

	g_FocusPanelList.RemoveAll();

	if ( !vgui_drawfocus.GetBool() )
		return;
	
	staticFocusOverlayPanel->MoveToFront();

	bool include_hidden = vgui_drawfocus.GetInt() == 2;

	int x, y;
	vgui::input()->GetCursorPos( x, y );

	vgui::VPANEL embedded = vgui::surface()->GetEmbeddedPanel();

	if ( vgui::surface()->IsCursorVisible() && vgui::surface()->IsWithin(x, y) )
	{
		// faster version of code below
		// checks through each popup in order, top to bottom windows
		int c = vgui::surface()->GetPopupCount();
		for (int i = c - 1; i >= 0; i--)
		{
			vgui::VPANEL popup = vgui::surface()->GetPopup(i);
			if ( !popup )
				continue;

			if ( popup == embedded )
				continue;
			if ( !vgui::ipanel()->IsVisible( popup ) )
				continue;

			VGui_RecursePanel( g_FocusPanelList, x, y, popup, include_hidden );
		}

		VGui_RecursePanel( g_FocusPanelList, x, y, embedded, include_hidden );
	}

	// Now draw them
	con_nprint_t np;
	np.time_to_live = 1.0f;
	
	int c = g_FocusPanelList.Size();

	int slot = 0;
	for ( int i = 0; i < c; i++ )
	{
		if ( slot > 31 )
			break;

		vgui::VPANEL vpanel = g_FocusPanelList[ i ];
		if ( !vpanel )
			continue;

		np.index = slot;

		int r, g, b;
		CFocusOverlayPanel::GetColorForSlot( slot, r, g, b );

		np.color[ 0 ] = r / 255.0f;
		np.color[ 1 ] = g / 255.0f;
		np.color[ 2 ] = b / 255.0f;

		Con_NXPrintf( &np, "%3i:  %s\n", slot + 1, vgui::ipanel()->GetName(vpanel) );

		slot++;
	}

	while ( slot <= 31 )
	{
		Con_NPrintf( slot, "" );
		slot++;
	}
}

void VGui_SetGameDLLPanelsVisible( bool show )
{
	EngineVGui()->SetGameDLLPanelsVisible( show );
}

void CEngineVGui::ShowNewGameDialog( int chapter )
{
	staticGameUIFuncs->ShowNewGameDialog( chapter );
}

void CEngineVGui::OnCreditsFinished( void ) 
{
	staticGameUIFuncs->OnCreditsFinished();
}

bool CEngineVGui::ValidateStorageDevice(int *pStorageDeviceValidated)
{
	return staticGameUIFuncs->ValidateStorageDevice( pStorageDeviceValidated );
}

//-----------------------------------------------------------------------------
// Dump the panel hierarchy
//-----------------------------------------------------------------------------
void DumpPanels_r( vgui::VPANEL panel, int level )
{
	int i;

	vgui::IPanel *ipanel = vgui::ipanel();

	const char *pName = ipanel->GetName( panel );

	char indentBuff[32];
	for (i=0; i<level; i++)
	{
		indentBuff[i] = '.';
	}
	indentBuff[i] = '\0';

	ConMsg( "%s%s\n", indentBuff, pName[0] ? pName : "???" );

	int childcount = ipanel->GetChildCount( panel );
	for ( i = 0; i < childcount; i++ )
	{
		vgui::VPANEL child = ipanel->GetChild( panel, i );
		DumpPanels_r( child, level+1 );
	}
}

void DumpPanels_f()
{
	vgui::VPANEL embedded = vgui::surface()->GetEmbeddedPanel();
	DumpPanels_r( embedded, 0 );
}
ConCommand DumpPanels("dump_panels", DumpPanels_f, "Dump Panel Tree" );

#if defined( _X360 )
//-----------------------------------------------------------------------------
// Purpose: For testing message dialogs 
//-----------------------------------------------------------------------------
#include "vgui_controls/MessageDialog.h"
CON_COMMAND( dlg_normal, "Display a sample message dialog" )
{
	EngineVGui()->ShowMessageDialog( MD_STANDARD_SAMPLE );
}

CON_COMMAND( dlg_warning, "Display a sample warning message dialog" )
{
	EngineVGui()->ShowMessageDialog( MD_WARNING_SAMPLE );
}

CON_COMMAND( dlg_error, "Display a sample error message dialog" )
{
	EngineVGui()->ShowMessageDialog( MD_ERROR_SAMPLE );
}
#endif
