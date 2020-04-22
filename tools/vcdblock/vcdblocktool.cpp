//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#include "vcdblocktool.h"
#include "vgui_controls/Menu.h"
#include "tier1/KeyValues.h"
#include "vgui/IInput.h"
#include "vgui/KeyCode.h"
#include "vgui_controls/FileOpenDialog.h"
#include "filesystem.h"
#include "vgui/ilocalize.h"
#include "tier0/icommandline.h"
#include "materialsystem/imaterialsystem.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "vcdblockdoc.h"
#include "infotargetbrowserpanel.h"
#include "infotargetpropertiespanel.h"
#include "dme_controls/AttributeStringChoicePanel.h"
#include "tier3/tier3.h"
#include "tier2/fileutils.h"
#include "vgui/ivgui.h"
#include "view_shared.h"

// for tracing
#include "cmodel.h"
#include "engine/ienginetrace.h"

using namespace vgui;


const char *GetVGuiControlsModuleName()
{
	return "VcdBlockTool";
}

//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool ConnectTools( CreateInterfaceFn factory )
{
	return (materials != NULL) && (g_pMatSystemSurface != NULL) && (mdlcache != NULL) && 
		(studiorender != NULL) && (g_pMaterialSystemHardwareConfig != NULL);
}

void DisconnectTools( )
{
}


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CVcdBlockTool	*g_pVcdBlockTool = NULL;

void CreateTools()
{
	g_pVcdBlockTool = new CVcdBlockTool();
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CVcdBlockTool::CVcdBlockTool()
{
	m_bInNodeDropMode = false;
	m_pMenuBar = NULL;
	m_pDoc = NULL;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CVcdBlockTool::Init( )
{
	m_pDoc = NULL;
	m_RecentFiles.LoadFromRegistry( GetRegistryName() );

	// NOTE: This has to happen before BaseClass::Init
	g_pVGuiLocalize->AddFile( "resource/toolvcdblock_%language%.txt" );

	if ( !BaseClass::Init( ) )
		return false;

	{
		m_hPreviewTarget = CreateElement<CDmeVMFEntity>( "preview target", DMFILEID_INVALID );
		m_hPreviewTarget->SetValue( "classname", "info_target" );
	}

	return true;
}

void CVcdBlockTool::Shutdown()
{
	m_RecentFiles.SaveToRegistry( GetRegistryName() );
	g_pDataModel->DestroyElement( m_hPreviewTarget );

	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// returns the document
//-----------------------------------------------------------------------------
inline CVcdBlockDoc *CVcdBlockTool::GetDocument()
{
	return m_pDoc;
}

	
//-----------------------------------------------------------------------------
// Tool activation/deactivation
//-----------------------------------------------------------------------------
void CVcdBlockTool::OnToolActivate()
{
	BaseClass::OnToolActivate();

	enginetools->Command( "commentary 1\n" );
}

void CVcdBlockTool::OnToolDeactivate()
{
	BaseClass::OnToolDeactivate();

	enginetools->Command( "commentary 0\n" );
}


//-----------------------------------------------------------------------------
// Enter mode where we preview dropping nodes
//-----------------------------------------------------------------------------
void CVcdBlockTool::EnterTargetDropMode()
{
	// Can only do it in editor mode
	if ( IsGameInputEnabled() )
		return;
	 
	m_bInNodeDropMode = true;
	SetMode( true, IsFullscreen() );
	{
		CDisableUndoScopeGuard guard;
		m_hPreviewTarget->DrawInEngine( true ); 
	}
	SetMiniViewportText( "Left Click To Place Info Target\nESC to exit" );
	enginetools->Command( "noclip\n" );
}

void CVcdBlockTool::LeaveTargetDropMode()
{
	Assert( m_bInNodeDropMode );

	m_bInNodeDropMode = false;
	SetMode( false, IsFullscreen() );
	{
		CDisableUndoScopeGuard guard;
		m_hPreviewTarget->DrawInEngine( false );
	}
	SetMiniViewportText( NULL );
	enginetools->Command( "noclip\n" );
}


//-----------------------------------------------------------------------------
// figure out if the click is in the miniviewport, and where it's aiming
//-----------------------------------------------------------------------------
bool CVcdBlockTool::IsMiniViewportCursor( int x, int y, Vector &org, Vector &forward )
{
	// when dealing with the miniviewport, it just wants the screen area
	int minx, miny, width, height;
	GetMiniViewportEngineBounds( minx, miny, width, height );
	x = x - minx;
	y = y - miny;
	if (x >= 0 && x < width && y >= 0 && y < height)
	{
		CViewSetup view;
		if (enginetools->GetPlayerView( view, 0, 0, width, height ))
		{
			// get a ray into the world
			enginetools->CreatePickingRay( view, x, y, org, forward );
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CVcdBlockTool::QuickLoad( void )
{
	m_bHasPlayerPosition = true;
	float flFov;
	clienttools->GetLocalPlayerEyePosition( m_vecPlayerOrigin, m_vecPlayerAngles, flFov );

	enginetools->Command( "load quick\n" );
	enginetools->Execute( );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CVcdBlockTool::QuickSave( void )
{
	enginetools->Command( "save quick\n" );
}
	
//-----------------------------------------------------------------------------
// Gets the position of the preview object
//-----------------------------------------------------------------------------
void CVcdBlockTool::GetPlacementInfo( Vector &vecOrigin, QAngle &angAngles )
{
	// Places the placement objects
	float flFov;
	clienttools->GetLocalPlayerEyePosition( vecOrigin, angAngles, flFov );

	Vector vecForward;
	AngleVectors( angAngles, &vecForward );
	VectorMA( vecOrigin, 40.0f, vecForward, vecOrigin );

	// Eliminate pitch
	angAngles.x = 0.0f;
}


//-----------------------------------------------------------------------------
// Place the preview object before rendering
//-----------------------------------------------------------------------------
void CVcdBlockTool::ClientPreRender()
{
	BaseClass::ClientPreRender();
	if ( !m_bInNodeDropMode )
		return;

	// Places the placement objects
	Vector vecOrigin;
	QAngle angAngles;
	GetPlacementInfo( vecOrigin, angAngles );

	CDisableUndoScopeGuard guard;
	m_hPreviewTarget->SetRenderOrigin( vecOrigin );
	m_hPreviewTarget->SetRenderAngles( angAngles );
}


//-----------------------------------------------------------------------------
// Let tool override key events (ie ESC and ~)
//-----------------------------------------------------------------------------
bool CVcdBlockTool::TrapKey( ButtonCode_t key, bool down )
{
	// Don't hook keyboard if not topmost
	if ( !IsActiveTool() )
		return false; // didn't trap, continue processing

	// Don't hook these methods if the game isn't running
	if ( !enginetools->IsInGame() )
		return false;

	if ( !m_bInNodeDropMode )
	{
		if ( key == MOUSE_LEFT )
		{
			if ( m_bInNodeDragMode && down == false )
			{
				m_bInNodeDragMode = false;
			}
			else if ( !m_bInNodeDragMode && down == true )
			{
				int x, y;
				input()->GetCursorPos(x, y);

				Vector org, forward;
				if (IsMiniViewportCursor( x, y, org, forward ))
				{
					// trace to something solid
					Ray_t ray;
					CTraceFilterWorldAndPropsOnly traceFilter;
					CBaseTrace tr;
					ray.Init( org, org + forward * 1000.0 );
					enginetools->TraceRayServer( ray, MASK_OPAQUE, &traceFilter, &tr );

#if 1
					CDmeVMFEntity *pSelection = m_pDoc->GetInfoTargetForLocation( tr.startpos, tr.endpos );
					if (pSelection)
					{
						GetInfoTargetBrowser()->SelectNode( pSelection );
						m_bInNodeDragMode = true;
						m_iDragX = x;
						m_iDragY = y;
						return true;
					}
#else
					if (tr.fraction < 1.0f)
					{
						// needs a better angle initialization
						Vector vecOrigin;
						QAngle angAngles;
						GetPlacementInfo( vecOrigin, angAngles );
						m_pDoc->AddNewInfoTarget( tr.endpos, angAngles );
						return true;	// trapping this key, stop processing
					}
#endif
				}
			}
		}
		return BaseClass::TrapKey( key, down );
	}

	if ( !down )
		return false;

	if ( key == KEY_ESCAPE )
	{
		LeaveTargetDropMode();
		return true;	// trapping this key, stop processing
	}

	if ( key == MOUSE_LEFT )
	{
		Vector vecOrigin;
		QAngle angAngles;
		GetPlacementInfo( vecOrigin, angAngles );
		m_pDoc->AddNewInfoTarget( vecOrigin, angAngles );
		return true;	// trapping this key, stop processing
	}

	return false; // didn't trap, continue processing
}


//-----------------------------------------------------------------------------
// Used to hook DME VMF entities into the render lists
//-----------------------------------------------------------------------------
void CVcdBlockTool::DrawEntitiesInEngine( bool bDrawInEngine )
{
	if ( !m_pDoc )
		return;

	const CDmrElementArray<CDmElement> entities( m_pDoc->GetEntityList() );
	if ( !entities.IsValid() )
		return;

	CDisableUndoScopeGuard guard;
	int nCount = entities.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeVMFEntity *pEntity = CastElement<CDmeVMFEntity>( entities[i] );
		Assert( pEntity );
		pEntity->DrawInEngine( bDrawInEngine );
	}
}

void CVcdBlockTool::ClientLevelInitPostEntity()
{
	BaseClass::ClientLevelInitPostEntity();
	DrawEntitiesInEngine( true );

	AttachAllEngineEntities();
}

void CVcdBlockTool::ClientLevelShutdownPreEntity()
{
	DrawEntitiesInEngine( false );
	BaseClass::ClientLevelShutdownPreEntity();
}

	
//-----------------------------------------------------------------------------
// Derived classes can implement this to get a new scheme to be applied to this tool
//-----------------------------------------------------------------------------
vgui::HScheme CVcdBlockTool::GetToolScheme() 
{ 
	return vgui::scheme()->LoadSchemeFromFile( "Resource/BoxRocket.res", "BoxRocket" );
}


//-----------------------------------------------------------------------------
//
// The View menu
//
//-----------------------------------------------------------------------------
class CVcdBlockViewMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CVcdBlockViewMenuButton, CToolMenuButton );
public:
	CVcdBlockViewMenuButton( CVcdBlockTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget );
	virtual void OnShowMenu(vgui::Menu *menu);

private:
	CVcdBlockTool *m_pTool;
};

CVcdBlockViewMenuButton::CVcdBlockViewMenuButton( CVcdBlockTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget )
	: BaseClass( parent, panelName, text, pActionSignalTarget )
{
	m_pTool = parent;

	AddCheckableMenuItem( "properties", "#VcdBlockProperties", new KeyValues( "OnToggleProperties" ), pActionSignalTarget );
	AddCheckableMenuItem( "entityreport", "#VcdBlockEntityReport", new KeyValues( "O|ÔglaEq”ípyOeport" ), pActionSignalTarget );

	AddSeparator();

	AddMenuItem( "defaultlayout", "#VcdBlockViewDefault", new KeyValues( "OnDefaultLayout" ), pActionSignalTarget );

	SetMenu(m_pMenu);
}

void CVcdBlockViewMenuButton::OnShowMenu(vgui::Menu *menu)
{
	BaseClass::OnShowMenu( menu );

	// Update the menu
	int id;

	CVcdBlockDoc *pDoc = m_pTool->GetDocument();
	if ( pDoc )
	{
		id = m_Items.Find( "properties" );
		m_pMenu->SetItemEnabled( id, true );

		Panel *p;
		p = m_pTool->GetProperties();
		Assert( p );
		m_pMenu->SetMenuItemChecked( id, ( p && p->GetParent() ) ? true : false );

		id = m_Items.Find( "entityreport" );
		m_pMenu->SetItemEnabled( id, true );
		
		p = m_pTool->GetInfoTargetBrowser();
		Assert( p );
		m_pMenu->SetMenuItemChecked( id, ( p && p->GetParent() ) ? true : false );
	}
	else
	{
		id = m_Items.Find( "properties" );
		m_pMenu->SetItemEnabled( id, false );
		id = m_Items.Find( "entityreport" );
		m_pMenu->SetItemEnabled( id, false );
	}
}


//-----------------------------------------------------------------------------
//
// The Tool menu
//
//-----------------------------------------------------------------------------
class CVcdBlockToolMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CVcdBlockToolMenuButton, CToolMenuButton );
public:
	CVcdBlockToolMenuButton( CVcdBlockTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget );
	virtual void OnShowMenu(vgui::Menu *menu);

private:
	CVcdBlockTool *m_pTool;
};

CVcdBlockToolMenuButton::CVcdBlockToolMenuButton( CVcdBlockTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget )
	: BaseClass( parent, panelName, text, pActionSignalTarget )
{
	m_pTool = parent;

	AddMenuItem( "addnewnodes", "#VcdBlockAddNewNodes", new KeyValues( "AddNewNodes" ), pActionSignalTarget, NULL, "VcdBlockAddNewNodes" );
	AddMenuItem( "copyeditstovmf", "#VcdBlockCopyEditsToVMF", new KeyValues( "CopyEditsToVMF" ), pActionSignalTarget, NULL, "VcdBlockCopyEditsToVMF" );

	AddSeparator();

	AddCheckableMenuItem( "rememberposition", "#VcdBlockRememberPosition", new KeyValues( "RememberPosition" ), pActionSignalTarget, NULL, "VcdBlockRememberPosition" );

	SetMenu(m_pMenu);
}

void CVcdBlockToolMenuButton::OnShowMenu(vgui::Menu *menu)
{
	BaseClass::OnShowMenu( menu );

	// Update the menu
	int id;

	CVcdBlockDoc *pDoc = m_pTool->GetDocument();

	id = m_Items.Find( "addnewnodes" );
	m_pMenu->SetItemEnabled( id, pDoc != NULL );

	id = m_Items.Find( "rememberposition" );
	m_pMenu->SetMenuItemChecked( id, m_pTool->GetRememberPlayerPosition() );
}


//-----------------------------------------------------------------------------
// Initializes the menu bar
//-----------------------------------------------------------------------------
vgui::MenuBar *CVcdBlockTool::CreateMenuBar( CBaseToolSystem *pParent ) 
{
	m_pMenuBar = new CToolFileMenuBar( pParent, "Main Menu Bar" );

	// Sets info in the menu bar
	char title[ 64 ];
	ComputeMenuBarTitle( title, sizeof( title ) );
	m_pMenuBar->SetInfo( title );
	m_pMenuBar->SetToolName( GetToolName() );

	// Add menu buttons
	CToolMenuButton *pFileButton = CreateToolFileMenuButton( m_pMenuBar, "File", "&File", GetActionTarget(), this );
	CToolMenuButton *pEditButton = CreateToolEditMenuButton( this, "Edit", "&Edit", GetActionTarget() );
	CVcdBlockToolMenuButton *pToolButton = new CVcdBlockToolMenuButton( this, "VcdBlock", "&VcdBlock", GetActionTarget() );
	CVcdBlockViewMenuButton *pViewButton = new CVcdBlockViewMenuButton( this, "View", "&View", GetActionTarget() );
	CToolMenuButton *pSwitchButton = CreateToolSwitchMenuButton( m_pMenuBar, "Switcher", "&Tools", GetActionTarget() );

	m_pMenuBar->AddButton( pFileButton );
	m_pMenuBar->AddButton( pEditButton );
	m_pMenuBar->AddButton( pToolButton );
	m_pMenuBar->AddButton( pViewButton );
	m_pMenuBar->AddButton( pSwitchButton );

	return m_pMenuBar;
}


//-----------------------------------------------------------------------------
// Updates the menu bar based on the current file
//-----------------------------------------------------------------------------
void CVcdBlockTool::UpdateMenuBar( )
{
	if ( !m_pDoc )
	{
		m_pMenuBar->SetFileName( "#VcdBlockNoFile" );
		return;
	}

	const char *pEditFile = m_pDoc->GetEditFileName();
	if ( !pEditFile[0] )
	{
		m_pMenuBar->SetFileName( "#VcdBlockNoFile" );
		return;
	}

	if ( m_pDoc->IsDirty() )
	{
		char sz[ 512 ];
		Q_snprintf( sz, sizeof( sz ), "* %s", pEditFile );
		m_pMenuBar->SetFileName( sz );
	}
	else
	{
		m_pMenuBar->SetFileName( pEditFile );
	}
}


//-----------------------------------------------------------------------------
// Gets at tool windows
//-----------------------------------------------------------------------------
CInfoTargetPropertiesPanel *CVcdBlockTool::GetProperties()
{
	return m_hProperties.Get();
}

CInfoTargetBrowserPanel *CVcdBlockTool::GetInfoTargetBrowser()
{
	return m_hInfoTargetBrowser.Get();
}


//-----------------------------------------------------------------------------
// Shows element properties
//-----------------------------------------------------------------------------
void CVcdBlockTool::ShowElementProperties( )
{
	if ( !m_pDoc )
		return;

	// It should already exist
	Assert( m_hProperties.Get() );
	if ( m_hProperties.Get() )
	{
		m_hProperties->SetObject( m_hCurrentEntity );
	}
}


//-----------------------------------------------------------------------------
// Destroys all tool windows
//-----------------------------------------------------------------------------
void CVcdBlockTool::ShowEntityInEntityProperties( CDmeVMFEntity *pEntity )
{
	Assert( m_hProperties.Get() );
	m_hCurrentEntity = pEntity;
	m_hProperties->SetObject( m_hCurrentEntity );
}

	
//-----------------------------------------------------------------------------
// Destroys all tool windows
//-----------------------------------------------------------------------------
void CVcdBlockTool::DestroyToolContainers()
{
	int c = ToolWindow::GetToolWindowCount();
	for ( int i = c - 1; i >= 0 ; --i )
	{
		ToolWindow *kill = ToolWindow::GetToolWindow( i );
		delete kill;
	}
}


//-----------------------------------------------------------------------------
// Sets up the default layout
//-----------------------------------------------------------------------------
void CVcdBlockTool::OnDefaultLayout()
{
	int y = m_pMenuBar->GetTall();

	int usew, useh;
	GetSize( usew, useh );

	DestroyToolContainers();

	Assert( ToolWindow::GetToolWindowCount() == 0 );

	CInfoTargetPropertiesPanel *properties = GetProperties();
	CInfoTargetBrowserPanel *pEntityReport = GetInfoTargetBrowser();

	// Need three containers
	ToolWindow *pPropertyWindow = m_ToolWindowFactory.InstanceToolWindow( GetClientArea(), false, properties, "#VcdBlockProperties", false );
	ToolWindow *pEntityReportWindow = m_ToolWindowFactory.InstanceToolWindow( GetClientArea(), false, pEntityReport, "#VcdBlockEntityReport", false );

	int halfScreen = usew / 2;
	int bottom = useh - y;
	int sy = (bottom - y) / 2;
	SetMiniViewportBounds( halfScreen, y, halfScreen, sy - y );
	pEntityReportWindow->SetBounds( 0, y, halfScreen, bottom );
	pPropertyWindow->SetBounds( halfScreen, sy, halfScreen, bottom - sy );
}

void CVcdBlockTool::OnToggleProperties()
{
	if ( m_hProperties.Get() )
	{ 
		ToggleToolWindow( m_hProperties.Get(), "#VcdBlockProperties" );
	}
}

void CVcdBlockTool::OnToggleEntityReport()
{
	if ( m_hInfoTargetBrowser.Get() )
	{ 
		ToggleToolWindow( m_hInfoTargetBrowser.Get(), "#VcdBlockEntityReport" );
	}
}



//-----------------------------------------------------------------------------
// Creates
//-----------------------------------------------------------------------------
void CVcdBlockTool::CreateTools( CVcdBlockDoc *doc )
{
	if ( !m_hProperties.Get() )
	{
		m_hProperties = new CInfoTargetPropertiesPanel( m_pDoc, this );
	}

	if ( !m_hInfoTargetBrowser.Get() )
	{
		m_hInfoTargetBrowser = new CInfoTargetBrowserPanel( m_pDoc, this, "InfoTargetBrowserPanel" );
	}

	RegisterToolWindow( m_hProperties );
	RegisterToolWindow( m_hInfoTargetBrowser );
}


//-----------------------------------------------------------------------------
// Initializes the tools
//-----------------------------------------------------------------------------
void CVcdBlockTool::InitTools()
{
	ShowElementProperties();

	// FIXME: There are no tool windows here; how should this work?
	// These panels are saved
	windowposmgr->RegisterPanel( "properties", m_hProperties, false );
	windowposmgr->RegisterPanel( "entityreport", m_hInfoTargetBrowser, false );

	if ( !windowposmgr->LoadPositions( "cfg/vcdblock.txt", this, &m_ToolWindowFactory, "VcdBlock" ) )
	{
		OnDefaultLayout();
	}
}


void CVcdBlockTool::DestroyTools()
{
	m_hCurrentEntity = NULL;

	if ( m_hProperties.Get() && m_hInfoTargetBrowser.Get() )
	{
		windowposmgr->SavePositions( "cfg/vcdblock.txt", "VcdBlock" );
	}

	int c = ToolWindow::GetToolWindowCount();
	for ( int i = c - 1; i >= 0 ; --i )
	{
		ToolWindow *kill = ToolWindow::GetToolWindow( i );
		delete kill;
	}

	UnregisterAllToolWindows();

	if ( m_hProperties.Get() )
	{
		windowposmgr->UnregisterPanel( m_hProperties.Get() );
		delete m_hProperties.Get();
		m_hProperties = NULL;
	}

	if ( m_hInfoTargetBrowser.Get() )
	{
		windowposmgr->UnregisterPanel( m_hInfoTargetBrowser.Get() );
		delete m_hInfoTargetBrowser.Get();
		m_hInfoTargetBrowser = NULL;
	}
}


void CVcdBlockTool::ShowToolWindow( Panel *tool, char const *toolName, bool visible )
{
	Assert( tool );

	if ( tool->GetParent() == NULL && visible )
	{
		m_ToolWindowFactory.InstanceToolWindow( this, false, tool, toolName, false );
	}
	else if ( !visible )
	{
		ToolWindow *tw = dynamic_cast< ToolWindow * >( tool->GetParent()->GetParent() );
		Assert( tw );
		tw->RemovePage( tool );
	}
}

void CVcdBlockTool::ToggleToolWindow( Panel *tool, char const *toolName )
{
	Assert( tool );

	if ( tool->GetParent() == NULL )
	{
		ShowToolWindow( tool, toolName, true );
	}
	else
	{
		ShowToolWindow( tool, toolName, false );
	}
}


//-----------------------------------------------------------------------------
// Creates the action menu
//-----------------------------------------------------------------------------
vgui::Menu *CVcdBlockTool::CreateActionMenu( vgui::Panel *pParent )
{
	vgui::Menu *pActionMenu = new Menu( pParent, "ActionMenu" );
	pActionMenu->AddMenuItem( "#ToolHide", new KeyValues( "Command", "command", "HideActionMenu" ), GetActionTarget() );
	return pActionMenu;
}


//-----------------------------------------------------------------------------
// Inherited from IFileMenuCallbacks
//-----------------------------------------------------------------------------
int	CVcdBlockTool::GetFileMenuItemsEnabled( )
{
	int nFlags = FILE_ALL;
	if ( m_RecentFiles.IsEmpty() )
	{
		nFlags &= ~(FILE_RECENT | FILE_CLEAR_RECENT);
	}
	return nFlags;
}

void CVcdBlockTool::AddRecentFilesToMenu( vgui::Menu *pMenu )
{
	m_RecentFiles.AddToMenu( pMenu, GetActionTarget(), "OnRecent" );
}

bool CVcdBlockTool::GetPerforceFileName( char *pFileName, int nMaxLen )
{
	if ( !m_pDoc )
		return false;

	Q_strncpy( pFileName, m_pDoc->GetEditFileName(), nMaxLen );
	return pFileName[0] != 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CVcdBlockTool::OnExit()
{
	enginetools->Command( "quit\n" );
}

//-----------------------------------------------------------------------------
// Handle commands from the action menu and other menus
//-----------------------------------------------------------------------------
void CVcdBlockTool::OnCommand( const char *cmd )
{
	if ( !V_stricmp( cmd, "HideActionMenu" ) )
	{
		if ( GetActionMenu() )
		{
			GetActionMenu()->SetVisible( false );
		}
	}
	else if ( const char *pSuffix = StringAfterPrefix( cmd, "OnRecent" ) )
	{
		int idx = Q_atoi( pSuffix );
		OpenFileFromHistory( idx );
	}
	else if( const char *pSuffix = StringAfterPrefix( cmd, "OnTool" ) )
	{
		int idx = Q_atoi( pSuffix );
		enginetools->SwitchToTool( idx );
	}
	else if ( !V_stricmp( cmd, "OnUndo" ) )
	{
		OnUndo();
	}
	else if ( !V_stricmp( cmd, "OnRedo" ) )
	{
		OnRedo();
	}
	else if ( !V_stricmp( cmd, "OnDescribeUndo" ) )
	{
		OnDescribeUndo();
	}
	else
	{
		BaseClass::OnCommand( cmd );
	}
}


//-----------------------------------------------------------------------------
// Command handlers
//-----------------------------------------------------------------------------
void CVcdBlockTool::OnNew()
{
	int nFlags = 0;
	const char *pSaveFileName = NULL;
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		nFlags = FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY;
		pSaveFileName = m_pDoc->GetEditFileName();
	}

	// Bring up the file open dialog to choose a .bsp file
	OpenFile( "bsp", pSaveFileName, "vle", nFlags );
}


//-----------------------------------------------------------------------------
// Called when the File->Open menu is selected
//-----------------------------------------------------------------------------
void CVcdBlockTool::OnOpen( )
{
	int nFlags = 0;
	const char *pSaveFileName = NULL;
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		nFlags = FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY;
		pSaveFileName = m_pDoc->GetEditFileName();
	}

	OpenFile( "vle", pSaveFileName, "vle", nFlags );
}


bool CVcdBlockTool::OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	OnCloseNoSave();

	if ( !LoadDocument( pFileName ) )
		return false;

	m_RecentFiles.Add( pFileName, pFileFormat );
	m_RecentFiles.SaveToRegistry( GetRegistryName() );
	UpdateMenuBar();
	return true;
}

void CVcdBlockTool::Save()
{
	if ( m_pDoc )
	{
		SaveFile( m_pDoc->GetEditFileName(), "vle", FOSM_SHOW_PERFORCE_DIALOGS );
	}
}

void CVcdBlockTool::OnSaveAs()
{
	if ( m_pDoc )
	{
		SaveFile( NULL, "vle", FOSM_SHOW_PERFORCE_DIALOGS );
	}
}

void CVcdBlockTool::OnRestartLevel()
{
	// FIXME: We may want to use this instead of completely restarting,
	// but it's less well tested.  Should be a lot faster though

	// Reloads the map, entities only, will reload every entity 
	//	enginetools->Command( "respawn_entities\n" );
	enginetools->Command( "restart" );
	enginetools->Execute();

	const CDmrElementArray<> entities = m_pDoc->GetEntityList();
	if ( !entities.IsValid() )
		return;

	int nCount = entities.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeVMFEntity *pEntity = CastElement<CDmeVMFEntity>( entities[i] );
		Assert( pEntity );
		pEntity->MarkDirty( false );
	}
}

void CVcdBlockTool::SaveAndTest()
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		SaveFile( m_pDoc->GetEditFileName(), "vle", FOSM_SHOW_PERFORCE_DIALOGS, 
			new KeyValues( "RestartLevel" ) );
	}
	else
	{
		OnRestartLevel();
	}
}

void CVcdBlockTool::RestartMap()
{
	OnRestartLevel();
}

bool CVcdBlockTool::OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	if ( !m_pDoc )
		return true;

	m_pDoc->SetEditFileName( pFileName );
	m_pDoc->SaveToFile( );

	m_RecentFiles.Add( pFileName, pFileFormat );
	m_RecentFiles.SaveToRegistry( GetRegistryName() );
	UpdateMenuBar();
	return true;
}

void CVcdBlockTool::OnClose()
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		SaveFile( m_pDoc->GetEditFileName(), "vle", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY, 
			new KeyValues( "OnClose" ) );
		return;
	}

	OnCloseNoSave();
}

void CVcdBlockTool::OnCloseNoSave()
{
	DestroyTools();

	if ( m_pDoc )
	{
		CAppNotifyScopeGuard sg( "CVcdBlockTool::OnCloseNoSave", NOTIFY_CHANGE_OTHER );

		delete m_pDoc;
		m_pDoc = NULL;

		if ( m_hProperties )
		{
			m_hProperties->SetObject( NULL );
		}
	}

	UpdateMenuBar( );
}

void CVcdBlockTool::OnMarkNotDirty()
{
	if ( m_pDoc )
	{
		m_pDoc->SetDirty( false );
	}
}


void CVcdBlockTool::OnCopyEditsToVMF()
{
	if ( m_pDoc )
	{
		m_pDoc->CopyEditsToVMF();
	}
}


void CVcdBlockTool::OnRememberPosition()
{
	m_bRememberPlayerPosition = !m_bRememberPlayerPosition;
}


void CVcdBlockTool::AttachAllEngineEntities()
{
	if ( !clienttools || !m_pDoc )
		return;

	/*
	NOTE: This doesn't work for infotargets because they are server-only entities
	for ( EntitySearchResult sr = clienttools->FirstEntity(); sr != NULL; sr = clienttools->NextEntity( sr ) )
	{
		if ( !sr )
			continue;

		HTOOLHANDLE handle = clienttools->AttachToEntity( sr );

		const char *pClassName = clienttools->GetClassname( handle );
		if ( Q_strcmp( pClassName, "class C_InfoTarget" ) == 0 )
		{
			Vector vecOrigin = clienttools->GetAbsOrigin( handle );
			QAngle angAngles = clienttools->GetAbsAngles( handle );

			// Find the associated commentary node entry in our doc
			CDmeVMFEntity *pNode = m_pDoc->GetInfoTargetForLocation( vecOrigin, angAngles );
			if ( pNode )
			{
				pNode->AttachToEngineEntity( handle );
			}
		}
	}
	*/
}


//-----------------------------------------------------------------------------
// Open a specific file
//-----------------------------------------------------------------------------
void CVcdBlockTool::OpenSpecificFile( const char *pFileName )
{
	int nFlags = 0;
	const char *pSaveFileName = NULL;
	if ( m_pDoc )
	{
		// File is already open
		if ( !Q_stricmp( m_pDoc->GetEditFileName(), pFileName ) )
			return;

		if ( m_pDoc->IsDirty() )
		{
			nFlags = FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY;
			pSaveFileName = m_pDoc->GetEditFileName();
		}
	}

	OpenFile( pFileName, "bsp", pSaveFileName, "vle", nFlags );
}


//-----------------------------------------------------------------------------
// Show the save document query dialog
//-----------------------------------------------------------------------------
void CVcdBlockTool::OpenFileFromHistory( int slot )
{
	const char *pFileName = m_RecentFiles.GetFile( slot );
	if ( !pFileName )
		return;
	OpenSpecificFile( pFileName );
}


//-----------------------------------------------------------------------------
// Derived classes can implement this to get notified when files are saved/loaded
//-----------------------------------------------------------------------------
void CVcdBlockTool::OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues )
{
	if ( bWroteFile )
	{
		OnMarkNotDirty();
	}

	if ( !pContextKeyValues )
		return;

	if ( state != FileOpenStateMachine::SUCCESSFUL )
		return;

	if ( !Q_stricmp( pContextKeyValues->GetName(), "OnClose" ) )
	{
		OnCloseNoSave();
		return;
	}

	if ( !Q_stricmp( pContextKeyValues->GetName(), "OnQuit" ) )
	{
		OnCloseNoSave();
		vgui::ivgui()->PostMessage( GetVPanel(), new KeyValues( "OnExit" ), 0 );
		return;
	}

	if ( !Q_stricmp( pContextKeyValues->GetName(), "RestartLevel" ) )
	{
		OnRestartLevel();
		return;
	}
}


//-----------------------------------------------------------------------------
// Show the File browser dialog
//-----------------------------------------------------------------------------
void CVcdBlockTool::SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	char pStartingDir[ MAX_PATH ];

	if ( !Q_stricmp( pFileFormat, "bsp" ) )
	{
		GetModSubdirectory( "maps", pStartingDir, sizeof(pStartingDir) );
		pDialog->SetTitle( "Choose Valve BSP File", true );
		pDialog->SetStartDirectoryContext( "vcdblock_bsp_session", pStartingDir );
		pDialog->AddFilter( "*.bsp", "Valve BSP File (*.bsp)", true );
	}
	else
	{
		GetModContentSubdirectory( "maps", pStartingDir, sizeof(pStartingDir) );
		pDialog->SetTitle( "Choose Valve VLE File", true );
		pDialog->SetStartDirectoryContext( "vcdblock_vle_session", pStartingDir );
		pDialog->AddFilter( "*.vle", "Valve VLE File (*.vle)", true );
	}
}


//-----------------------------------------------------------------------------
// Can we quit?
//-----------------------------------------------------------------------------
bool CVcdBlockTool::CanQuit()
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		// Show Save changes Yes/No/Cancel and re-quit if hit yes/no
		SaveFile( m_pDoc->GetEditFileName(), "vle", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY, 
			new KeyValues( "OnQuit" ) );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Various command handlers related to the Edit menu
//-----------------------------------------------------------------------------
void CVcdBlockTool::OnUndo()
{
	CDisableUndoScopeGuard guard;
	g_pDataModel->Undo();
}

void CVcdBlockTool::OnRedo()
{
	CDisableUndoScopeGuard guard;
	g_pDataModel->Redo();
}

void CVcdBlockTool::OnDescribeUndo()
{
	CUtlVector< UndoInfo_t > list;
	g_pDataModel->GetUndoInfo( list );

	Msg( "%i operations in stack\n", list.Count() );

	for ( int i = list.Count() - 1; i >= 0; --i )
	{
		UndoInfo_t& entry = list[ i ];
		if ( entry.terminator )
		{
			Msg( "[ '%s' ] - %i operations\n", entry.undo, entry.numoperations );
		}

		Msg( "   +%s\n", entry.desc );
	}
}


//-----------------------------------------------------------------------------
// VcdBlock menu items
//-----------------------------------------------------------------------------
void CVcdBlockTool::OnAddNewNodes()
{
	if ( !m_pDoc )
		return;

	EnterTargetDropMode();
}


//-----------------------------------------------------------------------------
// Background
//-----------------------------------------------------------------------------
const char *CVcdBlockTool::GetLogoTextureName()
{
	return NULL;
}


//-----------------------------------------------------------------------------
// Inherited from IVcdBlockDocCallback
//-----------------------------------------------------------------------------
void CVcdBlockTool::OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	if ( GetInfoTargetBrowser() )
	{
		if (nNotifyFlags & NOTIFY_CHANGE_TOPOLOGICAL)
		{
			GetInfoTargetBrowser()->UpdateEntityList();
		}
		else if (nNotifyFlags & NOTIFY_CHANGE_ATTRIBUTE_VALUE)
		{
			GetInfoTargetBrowser()->Refresh();
		}
	}

	UpdateMenuBar();

	/*
	if ( bRefreshUI && m_hProperties.Get() )
	{
		m_hProperties->Refresh();
	}
	*/
}


//-----------------------------------------------------------------------------
// Loads up a new document
//-----------------------------------------------------------------------------
bool CVcdBlockTool::LoadDocument( const char *pDocName )
{
	Assert( !m_pDoc );

	DestroyTools();

	m_pDoc = new CVcdBlockDoc( this );
	if ( !m_pDoc->LoadFromFile( pDocName ) )
	{
		delete m_pDoc;
		m_pDoc = NULL;
		Warning( "Fatal error loading '%s'\n", pDocName );
		return false;
	}

	ShowMiniViewport( true );
	return true;
}


//-----------------------------------------------------------------------------
// The engine entities now exist, find them.
//-----------------------------------------------------------------------------

void CVcdBlockTool::ServerLevelInitPostEntity()
{
	if (!m_pDoc)
		return;

	m_pDoc->ServerLevelInitPostEntity();

	CreateTools( m_pDoc );
	InitTools();

	if (m_bRememberPlayerPosition && m_bHasPlayerPosition)
	{
		m_bHasPlayerPosition = false;
		servertools->SnapPlayerToPosition( m_vecPlayerOrigin, m_vecPlayerAngles );
	}

}
