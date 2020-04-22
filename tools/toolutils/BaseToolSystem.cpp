//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#include "toolutils/basetoolsystem.h"
#include "toolframework/ienginetool.h"
#include "vgui/IPanel.h"
#include "vgui_controls/Controls.h"
#include "vgui_controls/Menu.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/FileOpenDialog.h"
#include "vgui_controls/MessageBox.h"
#include "vgui/Cursor.h"
#include "vgui/iinput.h"
#include "vgui/ivgui.h"
#include "vgui_controls/AnimationController.h"
#include "ienginevgui.h"
#include "toolui.h"
#include "toolutils/toolmenubar.h"
#include "vgui/ilocalize.h"
#include "toolutils/enginetools_int.h"
#include "toolutils/vgui_tools.h"
#include "icvar.h"
#include "tier1/convar.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "filesystem.h"
#include "vgui_controls/savedocumentquery.h"
#include "vgui_controls/perforcefilelistframe.h"
#include "toolutils/miniviewport.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imesh.h"
#include "toolutils/BaseStatusBar.h"
#include "movieobjects/movieobjects.h"
#include "vgui_controls/KeyBoardEditorDialog.h"
#include "vgui_controls/KeyBindingHelpDialog.h"
#include "dmserializers/idmserializers.h"
#include "tier2/renderutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;

extern IMaterialSystem *MaterialSystem();

class CGlobalFlexController : public IGlobalFlexController
{
public:
	virtual int	FindGlobalFlexController( const char *name )
	{
		return clienttools->FindGlobalFlexcontroller( name );
	}

	virtual const char *GetGlobalFlexControllerName( int idx )
	{
		return clienttools->GetGlobalFlexControllerName( idx );
	}
};

static CGlobalFlexController g_GlobalFlexController;

extern IGlobalFlexController *g_pGlobalFlexController;

//-----------------------------------------------------------------------------
// Singleton interfaces
//-----------------------------------------------------------------------------
IServerTools	*servertools = NULL;
IClientTools	*clienttools = NULL;


//-----------------------------------------------------------------------------
// External functions
//-----------------------------------------------------------------------------
void RegisterTool( IToolSystem *tool );


//-----------------------------------------------------------------------------
// Base tool system constructor
//-----------------------------------------------------------------------------
CBaseToolSystem::CBaseToolSystem( const char *pToolName /*="CBaseToolSystem"*/ ) :
	BaseClass( NULL, pToolName ),
	m_pBackground( 0 ),
	m_pLogo( 0 )
{
	RegisterTool( this );
	SetAutoDelete( false );
	m_bGameInputEnabled = false;
	m_bFullscreenMode = false;
	m_bIsActive = false;
	m_bFullscreenToolModeEnabled = false;
	m_MostRecentlyFocused = NULL;
	SetKeyBoardInputEnabled( true );
	input()->RegisterKeyCodeUnhandledListener( GetVPanel() );

	m_pFileOpenStateMachine = new vgui::FileOpenStateMachine( this, this );
	m_pFileOpenStateMachine->AddActionSignalTarget( this );
}


void CBaseToolSystem::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetKeyBoardInputEnabled( true );
}

//-----------------------------------------------------------------------------
// Called at the end of engine startup (after client .dll and server .dll have been loaded)
//-----------------------------------------------------------------------------
bool CBaseToolSystem::Init( )
{
	// Read shared localization info
	g_pVGuiLocalize->AddFile( "resource/dmecontrols_%language%.txt" );
	g_pVGuiLocalize->AddFile( "resource/toolshared_%language%.txt" );
	g_pVGuiLocalize->AddFile( "Resource/vgui_%language%.txt" );
	g_pVGuiLocalize->AddFile( "Resource/platform_%language%.txt" );
	g_pVGuiLocalize->AddFile( "resource/boxrocket_%language%.txt" );

	// Create the tool workspace
	SetParent( VGui_GetToolRootPanel() );

	// Deal with scheme
	vgui::HScheme hToolScheme = GetToolScheme();
	if ( hToolScheme != 0 )
	{
		SetScheme( hToolScheme );
	}

	m_KeyBindingsHandle = Panel::CreateKeyBindingsContext( GetBindingsContextFile(), "GAME" );
	SetKeyBindingsContext( m_KeyBindingsHandle );
	LoadKeyBindings();

	const char *pszBackground = GetBackgroundTextureName();
	if ( pszBackground )
	{
		m_pBackground = materials->FindMaterial( GetBackgroundTextureName() , TEXTURE_GROUP_VGUI );
		m_pBackground->IncrementReferenceCount();
	}
	const char *pszLogo = GetLogoTextureName();
	if ( pszLogo )
	{
		m_pLogo = materials->FindMaterial( GetLogoTextureName(), TEXTURE_GROUP_VGUI );
		m_pLogo->IncrementReferenceCount();
	}

	// Make the tool workspace the size of the screen
	int w, h;
	surface()->GetScreenSize( w, h );
	SetBounds( 0, 0, w, h );
	SetPaintBackgroundEnabled( true );
	SetPaintBorderEnabled( false );
	SetPaintEnabled( false );
	SetCursor( vgui::dc_none );
	SetVisible( false );

	// Create the tool UI
	m_pToolUI = new CToolUI( this, "ToolUI", this );

	// Create the mini viewport
	m_hMiniViewport = CreateMiniViewport( GetClientArea() );
	Assert( m_hMiniViewport.Get() );

	return true;
}

void CBaseToolSystem::ShowMiniViewport( bool state )
{
	if ( !m_hMiniViewport.Get() )
		return;

	m_hMiniViewport->SetVisible( state );
}


void CBaseToolSystem::SetMiniViewportBounds( int x, int y, int width, int height )
{
	if ( m_hMiniViewport )
	{
		m_hMiniViewport->SetBounds( x, y, width, height );
	}
}

void CBaseToolSystem::SetMiniViewportText( const char *pText )
{
	if ( m_hMiniViewport )
	{
		m_hMiniViewport->SetOverlayText( pText );
	}
}

void CBaseToolSystem::GetMiniViewportEngineBounds( int &x, int &y, int &width, int &height )
{
	if ( m_hMiniViewport )
	{
		m_hMiniViewport->GetEngineBounds( x, y, width, height );
	}
}

vgui::Panel	*CBaseToolSystem::GetMiniViewport( void )
{ 
	return m_hMiniViewport;
}

//-----------------------------------------------------------------------------
// Shut down
//-----------------------------------------------------------------------------
void CBaseToolSystem::Shutdown()
{
	if ( m_pBackground )
	{
		m_pBackground->DecrementReferenceCount();
	}
	if ( m_pLogo )
	{
		m_pLogo->DecrementReferenceCount();
	}

	if ( m_hMiniViewport.Get() )
	{
		delete m_hMiniViewport.Get();
	}

	// Make sure anything "marked for deletion"
	//  actually gets deleted before this dll goes away
	vgui::ivgui()->RunFrame();
}


//-----------------------------------------------------------------------------
// Can the tool quit?
//-----------------------------------------------------------------------------
bool CBaseToolSystem::CanQuit()
{
	return true;
}


//-----------------------------------------------------------------------------
// Client, server init + shutdown
//-----------------------------------------------------------------------------
bool CBaseToolSystem::ServerInit( CreateInterfaceFn serverFactory )
{
	servertools = ( IServerTools * )serverFactory( VSERVERTOOLS_INTERFACE_VERSION, NULL );
	if ( !servertools )
	{
		Error( "CBaseToolSystem::PostInit:  Unable to get '%s' interface from game .dll\n", VSERVERTOOLS_INTERFACE_VERSION );
	}

	return true;
}

bool CBaseToolSystem::ClientInit( CreateInterfaceFn clientFactory )
{
	clienttools = ( IClientTools * )clientFactory( VCLIENTTOOLS_INTERFACE_VERSION, NULL );
	if ( !clienttools )
	{
		Error( "CBaseToolSystem::PostInit:  Unable to get '%s' interface from client .dll\n", VCLIENTTOOLS_INTERFACE_VERSION );
	}
	else
	{
		g_pGlobalFlexController = &g_GlobalFlexController; // don't set this until clienttools is connected
	}

	return true;
}

void CBaseToolSystem::ServerShutdown()
{
	servertools = NULL;
}

void CBaseToolSystem::ClientShutdown()
{
	clienttools = NULL;
}

	
//-----------------------------------------------------------------------------
// Level init, shutdown for server
//-----------------------------------------------------------------------------
void CBaseToolSystem::ServerLevelInitPreEntity()
{
}

void CBaseToolSystem::ServerLevelInitPostEntity()
{
}

void CBaseToolSystem::ServerLevelShutdownPreEntity()
{
}

void CBaseToolSystem::ServerLevelShutdownPostEntity()
{
}


//-----------------------------------------------------------------------------
// Think methods
//-----------------------------------------------------------------------------
void CBaseToolSystem::ServerFrameUpdatePreEntityThink()
{
}

void CBaseToolSystem::Think( bool finalTick )
{
	// run vgui animations
	vgui::GetAnimationController()->UpdateAnimations( enginetools->Time() );
}

void CBaseToolSystem::PostMessage( HTOOLHANDLE hEntity, KeyValues *message )
{
	if ( !Q_stricmp( message->GetName(), "ReleaseLayoffTexture" ) )
	{
		if ( m_hMiniViewport.Get() )
		{
			m_hMiniViewport->ReleaseLayoffTexture();
		}
		return;
	}
}

void CBaseToolSystem::ServerFrameUpdatePostEntityThink()
{
}

void CBaseToolSystem::ServerPreClientUpdate()
{
}

void CBaseToolSystem::ServerPreSetupVisibility()
{
}

const char* CBaseToolSystem::GetEntityData( const char *pActualEntityData )
{
	return pActualEntityData;
}

//-----------------------------------------------------------------------------
// Level init, shutdown for client
//-----------------------------------------------------------------------------
void CBaseToolSystem::ClientLevelInitPreEntity()
{
}

void CBaseToolSystem::ClientLevelInitPostEntity()
{
}

void CBaseToolSystem::ClientLevelShutdownPreEntity()
{
}

void CBaseToolSystem::ClientLevelShutdownPostEntity()
{
}

void CBaseToolSystem::ClientPreRender()
{
}

void CBaseToolSystem::ClientPostRender()
{
}


//-----------------------------------------------------------------------------
// Tool activation/deactivation
//-----------------------------------------------------------------------------
void CBaseToolSystem::OnToolActivate()
{
	m_bIsActive = true;
	UpdateUIVisibility( );

	// FIXME: Note that this is necessary because IsGameInputEnabled depends on m_bIsActive at the moment
	OnModeChanged();

	input()->SetModalSubTree( VGui_GetToolRootPanel(), GetVPanel(), IsGameInputEnabled() );
	input()->SetModalSubTreeReceiveMessages( !IsGameInputEnabled() );

	m_pToolUI->UpdateMenuBarTitle();
}

void CBaseToolSystem::OnToolDeactivate()
{
	m_bIsActive = false;
 	UpdateUIVisibility( );

	// FIXME: Note that this is necessary because IsGameInputEnabled depends on m_bIsActive at the moment
	OnModeChanged();

	input()->ReleaseModalSubTree();
}

//-----------------------------------------------------------------------------
// Let tool override key events (ie ESC and ~)
//-----------------------------------------------------------------------------
bool CBaseToolSystem::TrapKey( ButtonCode_t key, bool down )
{
	// Don't hook keyboard if not topmost
	if ( !m_bIsActive )
		return false; // didn't trap, continue processing

	// If in fullscreen toolMode, don't let ECSAPE bring up the game menu
	if ( !m_bGameInputEnabled && m_bFullscreenMode && ( key == KEY_ESCAPE ) )
		return true; // trapping this key, stop processing

	if ( down )
	{
		if ( key == TOGGLE_WINDOWED_KEY_CODE )
		{
			SetMode( m_bGameInputEnabled, !m_bFullscreenMode );
			return true; // trapping this key, stop processing
		}

		if ( key == TOGGLE_INPUT_KEY_CODE )
		{
			if ( input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL ) )
			{
				ToggleForceToolCamera();
			}
			else
			{
				SetMode( !m_bGameInputEnabled, m_bFullscreenMode );
			}
			return true; // trapping this key, stop processing
		}

		// If in IFM mode, let ~ switch to gameMode and toggle console
		if ( !IsGameInputEnabled() && ( key == '~' || key == '`' ) )
		{
			SetMode( true, m_bFullscreenMode );
			return false; // didn't trap, continue processing
		}
	}

	return false; // didn't trap, continue processing
}


//-----------------------------------------------------------------------------
// Shows, hides the tool ui (menu, client area, status bar)
//-----------------------------------------------------------------------------
void CBaseToolSystem::SetToolUIVisible( bool bVisible )
{
	if ( bVisible != m_pToolUI->IsVisible() )
	{
		m_pToolUI->SetVisible( bVisible );
		m_pToolUI->InvalidateLayout();
	}
}


//-----------------------------------------------------------------------------
// Computes whether vgui is visible or not
//-----------------------------------------------------------------------------
void CBaseToolSystem::UpdateUIVisibility()
{
	bool bIsVisible = m_bIsActive && ( !IsGameInputEnabled() || !m_bFullscreenMode );
	ShowUI( bIsVisible );
}


//-----------------------------------------------------------------------------
// Changes game input + fullscreen modes
//-----------------------------------------------------------------------------
void CBaseToolSystem::EnableFullscreenToolMode( bool bEnable )
{
	m_bFullscreenToolModeEnabled = bEnable;
}


//-----------------------------------------------------------------------------
// Changed whether camera is forced to be tool camera
//-----------------------------------------------------------------------------
void CBaseToolSystem::ToggleForceToolCamera()
{
}


//-----------------------------------------------------------------------------
// Changes game input + fullscreen modes
//-----------------------------------------------------------------------------
void CBaseToolSystem::SetMode( bool bGameInputEnabled, bool bFullscreen )
{
	Assert( m_bIsActive );

	if ( !m_bFullscreenToolModeEnabled )
	{
		if ( !bGameInputEnabled )
		{
			bFullscreen = false;
		}
	}

	if ( ( m_bFullscreenMode == bFullscreen ) && ( m_bGameInputEnabled == bGameInputEnabled ) )
		return;

	bool bOldGameInputEnabled = m_bGameInputEnabled;

	m_bFullscreenMode = bFullscreen;
	m_bGameInputEnabled = bGameInputEnabled;
	UpdateUIVisibility();

	if ( bOldGameInputEnabled != m_bGameInputEnabled )
	{
		Warning( "Input is now being sent to the %s\n", m_bGameInputEnabled ? "Game" : "Tools" );
		
		// The subtree starts at the tool system root panel.  If game input is enabled then
		//  the subtree should not receive or process input messages, otherwise it should
		Assert( input()->GetModalSubTree() );
		if ( input()->GetModalSubTree() )
		{
			input()->SetModalSubTreeReceiveMessages( !m_bGameInputEnabled );
		}
	}

	if ( m_pToolUI )
	{
		m_pToolUI->UpdateMenuBarTitle();
	}

	OnModeChanged( );
}


//-----------------------------------------------------------------------------
// Keybinding
//-----------------------------------------------------------------------------
void CBaseToolSystem::LoadKeyBindings()
{
	ReloadKeyBindings( m_KeyBindingsHandle );
}

void CBaseToolSystem::ShowKeyBindingsEditor( Panel *panel, KeyBindingContextHandle_t handle )
{
	if ( !m_hKeyBindingsEditor.Get() )
	{
		// Show the editor
		m_hKeyBindingsEditor = new CKeyBoardEditorDialog( GetClientArea(), panel, handle );
		m_hKeyBindingsEditor->DoModal();
	}
}

void CBaseToolSystem::ShowKeyBindingsHelp( Panel *panel, KeyBindingContextHandle_t handle, vgui::KeyCode boundKey, int modifiers )
{
	if ( m_hKeyBindingsHelp.Get() )
	{
		m_hKeyBindingsHelp->HelpKeyPressed();
		return;
	}

	m_hKeyBindingsHelp = new CKeyBindingHelpDialog( GetClientArea(), panel, handle, boundKey, modifiers );
}

vgui::KeyBindingContextHandle_t CBaseToolSystem::GetKeyBindingsHandle()
{
	return m_KeyBindingsHandle;
}

void CBaseToolSystem::OnEditKeyBindings()
{
	Panel *tool = GetMostRecentlyFocusedTool();
	if ( tool )
	{
		ShowKeyBindingsEditor( tool, tool->GetKeyBindingsContext() );
	}
}

void CBaseToolSystem::OnKeyBindingHelp()
{
	Panel *tool = GetMostRecentlyFocusedTool();
	if ( tool )
	{
		CUtlVector< BoundKey_t * > list;
		LookupBoundKeys( "keybindinghelp", list );
		if ( list.Count() > 0 )
		{
			ShowKeyBindingsHelp( tool, tool->GetKeyBindingsContext(), (KeyCode)list[ 0 ]->keycode, list[ 0 ]->modifiers );
		}
	}
}


//-----------------------------------------------------------------------------
// Registers tool window
//-----------------------------------------------------------------------------
void CBaseToolSystem::RegisterToolWindow( vgui::PHandle hPanel )
{
	int i = m_Tools.AddToTail( hPanel );
	m_Tools[i]->SetKeyBindingsContext( m_KeyBindingsHandle );
}

void CBaseToolSystem::UnregisterAllToolWindows()
{
	m_Tools.RemoveAll();
	m_MostRecentlyFocused = NULL;
}

Panel *CBaseToolSystem::GetMostRecentlyFocusedTool()
{
	VPANEL focus = input()->GetFocus();
	int c = m_Tools.Count();
	for ( int i = 0; i < c; ++i )
	{
		Panel *p = m_Tools[ i ].Get();
		if ( !p )
			continue;

		// Not a visible tool
		if ( !p->GetParent() )
			continue;

		bool hasFocus = p->HasFocus();
		bool focusOnChild = focus && ipanel()->HasParent(focus, p->GetVPanel());

		if ( !hasFocus && !focusOnChild )
		{
			continue;
		}

		return p;
	}

	return m_MostRecentlyFocused.Get();
}

void CBaseToolSystem::PostMessageToActiveTool( KeyValues *pKeyValues, float flDelay )
{
	Panel *pMostRecent = GetMostRecentlyFocusedTool();
	if ( pMostRecent )
	{
		Panel::PostMessage( pMostRecent->GetVPanel(), pKeyValues, flDelay );
	}
}

void CBaseToolSystem::PostMessageToActiveTool( const char *msg, float flDelay )
{
	Panel *pMostRecent = GetMostRecentlyFocusedTool();
	if ( pMostRecent )
	{
		Panel::PostMessage( pMostRecent->GetVPanel(), new KeyValues( msg ), flDelay );
	}
}

void CBaseToolSystem::PostMessageToAllTools( KeyValues *message )
{
	int nCount = enginetools->GetToolCount();
	for ( int i = 0; i < nCount; ++i )
	{
		IToolSystem *pToolSystem = const_cast<IToolSystem*>( enginetools->GetToolSystem( i ) );
		pToolSystem->PostMessage( HTOOLHANDLE_INVALID, message );
	}
}

void CBaseToolSystem::OnThink()
{
	BaseClass::OnThink();

	VPANEL focus = input()->GetFocus();
	int c = m_Tools.Count();
	for ( int i = 0; i < c; ++i )
	{
		Panel *p = m_Tools[ i ].Get();
		if ( !p )
			continue;

		// Not a visible tool
		if ( !p->GetParent() )
			continue;

		bool hasFocus = p->HasFocus();
		bool focusOnChild = focus && ipanel()->HasParent(focus, p->GetVPanel());

		if ( !hasFocus && !focusOnChild )
			continue;

		m_MostRecentlyFocused = p;
		break;
	}
}


//-----------------------------------------------------------------------------
// Let tool override viewport for engine
//-----------------------------------------------------------------------------
void CBaseToolSystem::AdjustEngineViewport( int& x, int& y, int& width, int& height )
{
	if ( !m_hMiniViewport.Get() )
		return;

	bool enabled;
	int vpx, vpy, vpw, vph;
	
	m_hMiniViewport->GetViewport( enabled, vpx, vpy, vpw, vph );

	if ( !enabled )
		return;

	x = vpx;
	y = vpy;
	width = vpw;
	height = vph;
}

//-----------------------------------------------------------------------------
// Let tool override view/camera
//-----------------------------------------------------------------------------
bool CBaseToolSystem::SetupEngineView( Vector &origin, QAngle &angles, float &fov )
{
	return false;
}

//-----------------------------------------------------------------------------
// Let tool override microphone
//-----------------------------------------------------------------------------
bool CBaseToolSystem::SetupAudioState( AudioState_t &audioState )
{
	return false;
}

//-----------------------------------------------------------------------------
// Should the game be allowed to render the view?
//-----------------------------------------------------------------------------
bool CBaseToolSystem::ShouldGameRenderView()
{
	// Render through mini viewport unless in fullscreen mode
	if ( !IsVisible() )
	{
		return true;
	}

	if ( !m_hMiniViewport.Get() )
		return true;

	if ( !m_hMiniViewport->IsVisible() )
	{
		return true;
	}

	// Route through mini viewport
	return false;
}

bool CBaseToolSystem::IsThirdPersonCamera()
{
	return false;
}

bool CBaseToolSystem::IsToolRecording()
{
	return false;
}

IMaterialProxy *CBaseToolSystem::LookupProxy( const char *proxyName )
{
	return NULL;
}


bool CBaseToolSystem::GetSoundSpatialization( int iUserData, int guid, SpatializationInfo_t& info )
{
	// Always hearable (no changes)
	return true;
}

void CBaseToolSystem::HostRunFrameBegin()
{
}

void CBaseToolSystem::HostRunFrameEnd()
{
}

void CBaseToolSystem::RenderFrameBegin()
{
	// If we can't see the engine window, do nothing
	if ( !IsVisible() || !IsActiveTool() )
		return;

	if ( !m_hMiniViewport.Get() || !m_hMiniViewport->IsVisible() )
		return;

	m_hMiniViewport->RenderFrameBegin();
}

void CBaseToolSystem::RenderFrameEnd()
{
}

void CBaseToolSystem::VGui_PreRender( int paintMode )
{
}

void CBaseToolSystem::VGui_PostRender( int paintMode )
{
}

void CBaseToolSystem::VGui_PreSimulate()
{
	if ( !m_bIsActive )
		return;

	// only show the gameUI when in gameMode
	vgui::VPANEL gameui = enginevgui->GetPanel( PANEL_GAMEUIDLL );
	if ( gameui != 0 )
	{
		bool wantsToBeSeen = IsGameInputEnabled() && (enginetools->IsGamePaused() || !enginetools->IsInGame() || enginetools->IsConsoleVisible());
		vgui::ipanel()->SetVisible(gameui, wantsToBeSeen);
	}

	// if there's no map loaded and we're in fullscreen toolMode, switch to gameMode
	// otherwise there's nothing to see or do...
	if ( !IsGameInputEnabled() && !IsVisible() && !enginetools->IsInGame() )
	{
		SetMode( true, m_bFullscreenMode );
	}
}

void CBaseToolSystem::VGui_PostSimulate()
{
}

const char *CBaseToolSystem::MapName() const
{
	return enginetools->GetCurrentMap();
}


//-----------------------------------------------------------------------------
// Shows or hides the UI
//-----------------------------------------------------------------------------
bool CBaseToolSystem::ShowUI( bool bVisible )
{
	bool bPrevVisible = IsVisible();
	if ( bPrevVisible == bVisible )
		return bPrevVisible;

	SetMouseInputEnabled( bVisible );
	SetVisible( bVisible );

	// Hide loading image if using bx movie UI
	// A bit of a hack because it more or less tunnels through to the client .dll, but the moviemaker assumes
	//  single player anyway...
	if ( bVisible )
	{
		ConVar *pCv = ( ConVar * )cvar->FindVar( "cl_showpausedimage" );
		if ( pCv )
		{
			pCv->SetValue( 0 );
		}
	}

	return bPrevVisible;
}


//-----------------------------------------------------------------------------
// Gets the action target to sent to panels so that the tool system's OnCommand is called
//-----------------------------------------------------------------------------
vgui::Panel *CBaseToolSystem::GetActionTarget()
{
	return this;
}


//-----------------------------------------------------------------------------
// Derived classes implement this to create a custom menubar
//-----------------------------------------------------------------------------
vgui::MenuBar *CBaseToolSystem::CreateMenuBar(CBaseToolSystem *pParent )
{
	return new vgui::MenuBar( pParent, "ToolMenuBar" );
}

//-----------------------------------------------------------------------------
// Purpose: Derived classes implement this to create a custom status bar, or return NULL for no status bar
//-----------------------------------------------------------------------------
vgui::Panel *CBaseToolSystem::CreateStatusBar( vgui::Panel *pParent )
{
	return new CBaseStatusBar( this, "Status Bar" );
}

//-----------------------------------------------------------------------------
// Gets at the action menu
//-----------------------------------------------------------------------------
vgui::Menu *CBaseToolSystem::GetActionMenu()
{
	return m_hActionMenu;
}


//-----------------------------------------------------------------------------
// Returns the client area
//-----------------------------------------------------------------------------
vgui::Panel* CBaseToolSystem::GetClientArea()
{
	return m_pToolUI->GetClientArea();
}


//-----------------------------------------------------------------------------
// Pops up the action menu
//-----------------------------------------------------------------------------
void CBaseToolSystem::OnMousePressed( vgui::MouseCode code )
{
	if ( code == MOUSE_RIGHT )
	{
		InitActionMenu();
	}
	else
	{
		BaseClass::OnMousePressed( code );
	}
}


//-----------------------------------------------------------------------------
// Creates the action menu
//-----------------------------------------------------------------------------
void CBaseToolSystem::InitActionMenu()
{
	ShutdownActionMenu();

	// Let the tool system create the action menu
	m_hActionMenu = CreateActionMenu( this );

	if ( m_hActionMenu.Get() )
	{
		m_hActionMenu->SetVisible(true);
		PositionActionMenu();
		m_hActionMenu->RequestFocus();
	}
}


//-----------------------------------------------------------------------------
// Destroy action menu
//-----------------------------------------------------------------------------
void CBaseToolSystem::ShutdownActionMenu()
{
	if ( m_hActionMenu.Get() )
	{
		m_hActionMenu->MarkForDeletion();
		m_hActionMenu = NULL;
	}
}

void CBaseToolSystem::UpdateMenu( vgui::Menu *menu )
{
	// Nothing
}

//-----------------------------------------------------------------------------
// Positions the action menu when it's time to pop it up
//-----------------------------------------------------------------------------
void CBaseToolSystem::PositionActionMenu()
{
	// get cursor position, this is local to this text edit window
	int cursorX, cursorY;
	input()->GetCursorPos(cursorX, cursorY);
	
	// relayout the menu immediately so that we know it's size
	m_hActionMenu->InvalidateLayout(true);

	// Get the menu size
	int menuWide, menuTall;
	m_hActionMenu->GetSize( menuWide, menuTall );
	
	// work out where the cursor is and therefore the best place to put the menu
	int wide, tall;
	GetSize( wide, tall );

	if (wide - menuWide > cursorX)
	{
		// menu hanging right
		if (tall - menuTall > cursorY)
		{
			// menu hanging down
			m_hActionMenu->SetPos(cursorX, cursorY);
		}
		else
		{
			// menu hanging up
			m_hActionMenu->SetPos(cursorX, cursorY - menuTall);
		}
	}
	else
	{
		// menu hanging left
		if (tall - menuTall > cursorY)
		{
			// menu hanging down
			m_hActionMenu->SetPos(cursorX - menuWide, cursorY);
		}
		else
		{
			// menu hanging up
			m_hActionMenu->SetPos(cursorX - menuWide, cursorY - menuTall);
		}
	}
}


//-----------------------------------------------------------------------------
// Handles the clear recent files message
//-----------------------------------------------------------------------------
void CBaseToolSystem::OnClearRecent()
{
	m_RecentFiles.Clear();
	m_RecentFiles.SaveToRegistry( GetRegistryName() );
}

	
//-----------------------------------------------------------------------------
// Called by the file open state machine
//-----------------------------------------------------------------------------
void CBaseToolSystem::OnFileStateMachineFinished( KeyValues *pKeyValues )
{
	KeyValues *pContext = pKeyValues->GetFirstTrueSubKey();
	bool bWroteFile = pKeyValues->GetInt( "wroteFile", 0 ) != 0;
	vgui::FileOpenStateMachine::CompletionState_t state = (vgui::FileOpenStateMachine::CompletionState_t)pKeyValues->GetInt( "completionState", vgui::FileOpenStateMachine::IN_PROGRESS );
	const char *pFileType = pKeyValues->GetString( "fileType" );
	OnFileOperationCompleted( pFileType, bWroteFile, state, pContext );
}


//-----------------------------------------------------------------------------
// Show the File browser dialog
//-----------------------------------------------------------------------------
void CBaseToolSystem::OpenFile( const char *pOpenFileType, const char *pSaveFileName, const char *pSaveFileType, int nFlags, KeyValues *pContextKeyValues )
{
	m_pFileOpenStateMachine->OpenFile( pOpenFileType, pContextKeyValues, pSaveFileName, pSaveFileType, nFlags );
}

void CBaseToolSystem::OpenFile( const char *pOpenFileName, const char *pOpenFileType, const char *pSaveFileName, const char *pSaveFileType, int nFlags, KeyValues *pContextKeyValues )
{
	m_pFileOpenStateMachine->OpenFile( pOpenFileName, pOpenFileType, pContextKeyValues, pSaveFileName, pSaveFileType, nFlags );
}


//-----------------------------------------------------------------------------
// Used to save a specified file, and deal with all the lovely dialogs
//-----------------------------------------------------------------------------
void CBaseToolSystem::SaveFile( const char *pFileName, const char *pFileType, int nFlags, KeyValues *pContextKeyValues )
{
	m_pFileOpenStateMachine->SaveFile( pContextKeyValues, pFileName, pFileType, nFlags );
}


//-----------------------------------------------------------------------------
// Paints the background
//-----------------------------------------------------------------------------
void CBaseToolSystem::PaintBackground()
{
	int w, h;
	GetSize( w, h );

	int x, y;
	GetPos( x, y );
	LocalToScreen( x, y );

	CMatRenderContextPtr pRenderContext( materials );
	if ( m_pBackground )
	{
		int texWide = m_pBackground->GetMappingWidth();
		int texTall = m_pBackground->GetMappingHeight();

		float maxu = (float)w / (float)texWide;
		float maxv = (float)h / (float)texTall;

		RenderQuad( m_pBackground, x, y, w, h, surface()->GetZPos(), 0.0f, 0.0f, maxu, maxv, Color( 255, 255, 255, 255 ) );
	}

	bool hasDoc = HasDocument();
	if ( m_pLogo )
	{
		int texWide = m_pLogo->GetMappingWidth();
		float logoAspectRatio = 0.442;

		if ( hasDoc )
		{
			int logoW = texWide / 2;
			int logoH = logoW * logoAspectRatio;

			x = w - logoW - 15;
			y = h - logoH - 30;

			w = logoW;
			h = logoH;
		}
		else
		{
			int logoW = texWide;
			int logoH = logoW * logoAspectRatio;

			x = ( w - logoW ) / 2;
			y = ( h - logoH ) / 2;

			w = logoW;
			h = logoH;
		}

		int alpha = hasDoc ? 0 : 255;

		RenderQuad( m_pLogo, x, y, w, h, surface()->GetZPos(), 0.0f, 0.0f, 1.0f, 1.0f, Color( 255, 255, 255, alpha ) );
	}
}

const char *CBaseToolSystem::GetBackgroundTextureName()
{
	return "vgui/tools/ifm/ifm_background";
}

bool CBaseToolSystem::HasDocument()
{
	return false;
}

CMiniViewport *CBaseToolSystem::CreateMiniViewport( vgui::Panel *parent )
{
	int w, h;
	surface()->GetScreenSize( w, h );

	CMiniViewport *vp = new CMiniViewport( parent, "MiniViewport" );
	Assert( vp );
	vp->SetVisible( true );
	int menuBarHeight = 28;
	int titleBarHeight = 22;
	int offset = 4;
	vp->SetBounds( ( 2 * w / 3  ) - offset, menuBarHeight + offset, w / 3, h / 3 + titleBarHeight);
	return vp;
}

void CBaseToolSystem::ComputeMenuBarTitle( char *buf, size_t buflen )
{
	Q_snprintf( buf, buflen, ": %s [ %s - Switch Mode ] [ %s - Full Screen ]", IsGameInputEnabled() ? "Game Mode" : "Tool Mode", TOGGLE_INPUT_KEY_NAME, TOGGLE_WINDOWED_KEY_NAME );
}

void CBaseToolSystem::OnUnhandledMouseClick( int code )
{
	if ( (MouseCode)code == MOUSE_LEFT )
	{
		// If tool ui is visible and we're running game in a window 
		// and they click on the ifm it'll be unhandled, but in this case
		//  we'll switch back to the IFM mode
		if ( !IsFullscreen() && IsGameInputEnabled() )
		{
			SetMode( false, m_bFullscreenMode );
		}
	}
}
