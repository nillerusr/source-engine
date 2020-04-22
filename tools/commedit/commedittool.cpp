//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#include "commedittool.h"
#include "vgui_controls/Menu.h"
#include "tier1/KeyValues.h"
#include "vgui/IInput.h"
#include "vgui/KeyCode.h"
#include "vgui_controls/FileOpenDialog.h"
#include "vgui_controls/PropertySheet.h"
#include "filesystem.h"
#include "vgui/ilocalize.h"
#include "dme_controls/elementpropertiestree.h"
#include "tier0/icommandline.h"
#include "materialsystem/imaterialsystem.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "commeditdoc.h"
#include "commentarynodebrowserpanel.h"
#include "commentarypropertiespanel.h"
#include "dme_controls/AttributeStringChoicePanel.h"
#include "tier2/fileutils.h"
#include "tier3/tier3.h"
#include "vgui/ivgui.h"
#include "toolutils/ConsolePage.h"


using namespace vgui;


enum
{
	FILEOPEN_NEW_BSP,
	FILEOPEN_EXISTING_TXT,
};


const char *GetVGuiControlsModuleName()
{
	return "CommEditTool";
}

//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool ConnectTools( CreateInterfaceFn factory )
{
	return (materials != NULL) && (g_pMatSystemSurface != NULL) && (g_pMDLCache != NULL) && (studiorender != NULL) && (g_pMaterialSystemHardwareConfig != NULL);
}

void DisconnectTools( )
{
}


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CCommEditTool	*g_pCommEditTool = NULL;

void CreateTools()
{
	g_pCommEditTool = new CCommEditTool();
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CCommEditTool::CCommEditTool()
{
	m_bInNodeDropMode = false;
	m_pMenuBar = NULL;
	m_pDoc = NULL;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CCommEditTool::Init( )
{
	m_pDoc = NULL;
	m_RecentFiles.LoadFromRegistry( GetRegistryName() );

	// NOTE: This has to happen before BaseClass::Init
	g_pVGuiLocalize->AddFile( "resource/toolcommedit_%language%.txt" );

	if ( !BaseClass::Init( ) )
		return false;

	{
		m_hPreviewNode = CreateElement<CDmeCommentaryNodeEntity>( "preview node", DMFILEID_INVALID );
		m_hPreviewNode->SetValue( "classname", "point_commentary_node" );
		m_hPreviewTarget = CreateElement<CDmeCommentaryNodeEntity>( "preview target", DMFILEID_INVALID );
		m_hPreviewTarget->SetValue( "classname", "info_target" );
	}

	return true;
}

void CCommEditTool::Shutdown()
{
	m_RecentFiles.SaveToRegistry( GetRegistryName() );

	g_pDataModel->DestroyElement( m_hPreviewNode );
	g_pDataModel->DestroyElement( m_hPreviewTarget );

	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// returns the document
//-----------------------------------------------------------------------------
inline CCommEditDoc *CCommEditTool::GetDocument()
{
	return m_pDoc;
}

	
//-----------------------------------------------------------------------------
// Tool activation/deactivation
//-----------------------------------------------------------------------------
void CCommEditTool::OnToolActivate()
{
	BaseClass::OnToolActivate();

	enginetools->Command( "commentary 1\n" );
}

void CCommEditTool::OnToolDeactivate()
{
	BaseClass::OnToolDeactivate();

	enginetools->Command( "commentary 0\n" );
}


//-----------------------------------------------------------------------------
// Enter mode where we preview dropping nodes
//-----------------------------------------------------------------------------
void CCommEditTool::EnterNodeDropMode()
{
	// Can only do it in editor mode
	if ( IsGameInputEnabled() )
		return;
	 
	m_bInNodeDropMode = true;
	m_bDroppingCommentaryNodes = true;
	SetMode( true, IsFullscreen() );
	{
		CDisableUndoScopeGuard guard;
		m_hPreviewNode->DrawInEngine( true ); 
	}
	SetMiniViewportText( "Left Click To Place Commentary\nRight Click To Toggle Modes\nESC to exit" );
	enginetools->Command( "noclip\n" );
}

void CCommEditTool::LeaveNodeDropMode()
{
	Assert( m_bInNodeDropMode );

	m_bInNodeDropMode = false;
	SetMode( false, IsFullscreen() );
	{
		CDisableUndoScopeGuard guard;
		m_hPreviewNode->DrawInEngine( false );
		m_hPreviewTarget->DrawInEngine( false );
	}
	SetMiniViewportText( NULL );
	enginetools->Command( "noclip\n" );
}

	
//-----------------------------------------------------------------------------
// Gets the position of the preview object
//-----------------------------------------------------------------------------
void CCommEditTool::GetPlacementInfo( Vector &vecOrigin, QAngle &angAngles )
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
void CCommEditTool::ClientPreRender()
{
	BaseClass::ClientPreRender();
	if ( !m_bInNodeDropMode )
		return;

	// Places the placement objects
	Vector vecOrigin;
	QAngle angAngles;
	GetPlacementInfo( vecOrigin, angAngles );

	CDisableUndoScopeGuard guard;
	m_hPreviewNode->SetRenderOrigin( vecOrigin );
	m_hPreviewNode->SetRenderAngles( angAngles );

	m_hPreviewTarget->SetRenderOrigin( vecOrigin );
	m_hPreviewTarget->SetRenderAngles( angAngles );
}

	
//-----------------------------------------------------------------------------
// Let tool override key events (ie ESC and ~)
//-----------------------------------------------------------------------------
bool CCommEditTool::TrapKey( ButtonCode_t key, bool down )
{
	// Don't hook keyboard if not topmost
	if ( !IsActiveTool() )
		return false; // didn't trap, continue processing

	if ( !m_bInNodeDropMode )
	{
		if ( !IsGameInputEnabled() && !IsFullscreen() && ( key == KEY_BACKQUOTE ) && down )
		{
			BringConsoleToFront();
			return true;
		}
		return BaseClass::TrapKey( key, down );
	}

	if ( !down )
		return false;

	if ( key == KEY_ESCAPE )
	{
		LeaveNodeDropMode();
		return true;	// trapping this key, stop processing
	}

	if ( key == MOUSE_LEFT )
	{
		Vector vecOrigin;
		QAngle angAngles;
		GetPlacementInfo( vecOrigin, angAngles );
		if ( m_bDroppingCommentaryNodes )
		{
			m_pDoc->AddNewCommentaryNode( vecOrigin, angAngles );
		}
		else
		{
			m_pDoc->AddNewInfoTarget( vecOrigin, angAngles );
		}
		return true;	// trapping this key, stop processing
	}

	if ( key == MOUSE_RIGHT )
	{
		m_bDroppingCommentaryNodes = !m_bDroppingCommentaryNodes;
		if ( m_bDroppingCommentaryNodes )
		{
			SetMiniViewportText( "Left Click To Place Commentary\nRight Click To Toggle Modes\nESC to exit" );
		}
		else
		{
			SetMiniViewportText( "Left Click To Place Target\nRight Click To Toggle Modes\nESC to exit" );
		}
		CDisableUndoScopeGuard guard;
		m_hPreviewNode->DrawInEngine( m_bDroppingCommentaryNodes );
		m_hPreviewTarget->DrawInEngine( !m_bDroppingCommentaryNodes );
		return true;	// trapping this key, stop processing
	}

	return false; // didn't trap, continue processing
}


//-----------------------------------------------------------------------------
// Used to hook DME VMF entities into the render lists
//-----------------------------------------------------------------------------
void CCommEditTool::DrawCommentaryNodeEntitiesInEngine( bool bDrawInEngine )
{
	if ( !m_pDoc )
		return;

	CDmrCommentaryNodeEntityList entities = m_pDoc->GetEntityList();
	if ( !entities.IsValid() )
		return;

	CDisableUndoScopeGuard guard;
	int nCount = entities.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeCommentaryNodeEntity *pEntity = entities[i];
		Assert( pEntity );
		if ( pEntity )
		{
			pEntity->DrawInEngine( bDrawInEngine );
		}
	}
}

void CCommEditTool::ClientLevelInitPostEntity()
{
	BaseClass::ClientLevelInitPostEntity();
	DrawCommentaryNodeEntitiesInEngine( true );

	AttachAllEngineEntities();
}

void CCommEditTool::ClientLevelShutdownPreEntity()
{
	DrawCommentaryNodeEntitiesInEngine( false );
	BaseClass::ClientLevelShutdownPreEntity();
}

	
//-----------------------------------------------------------------------------
// Derived classes can implement this to get a new scheme to be applied to this tool
//-----------------------------------------------------------------------------
vgui::HScheme CCommEditTool::GetToolScheme() 
{ 
	return vgui::scheme()->LoadSchemeFromFile( "Resource/BoxRocket.res", "BoxRocket" );
}


//-----------------------------------------------------------------------------
//
// The View menu
//
//-----------------------------------------------------------------------------
class CCommEditViewMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CCommEditViewMenuButton, CToolMenuButton );
public:
	CCommEditViewMenuButton( CCommEditTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget );
	virtual void OnShowMenu(vgui::Menu *menu);

private:
	CCommEditTool *m_pTool;
};

CCommEditViewMenuButton::CCommEditViewMenuButton( CCommEditTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget )
	: BaseClass( parent, panelName, text, pActionSignalTarget )
{
	m_pTool = parent;

	AddCheckableMenuItem( "properties", "#CommEditProperties", new KeyValues( "OnToggleProperties" ), pActionSignalTarget );
	AddCheckableMenuItem( "commentarynodebrowser", "#CommEditEntityReport", new KeyValues( "OnToggleEntityReport" ), pActionSignalTarget );
	AddCheckableMenuItem( "console", "#BxConsole", new KeyValues( "ToggleConsole" ), pActionSignalTarget );

	AddSeparator();

	AddMenuItem( "defaultlayout", "#CommEditViewDefault", new KeyValues( "OnDefaultLayout" ), pActionSignalTarget );

	SetMenu(m_pMenu);
}

void CCommEditViewMenuButton::OnShowMenu(vgui::Menu *menu)
{
	BaseClass::OnShowMenu( menu );

	// Update the menu
	int id;

	CCommEditDoc *pDoc = m_pTool->GetDocument();
	if ( pDoc )
	{
		id = m_Items.Find( "properties" );
		m_pMenu->SetItemEnabled( id, true );

		Panel *p;
		p = m_pTool->GetProperties();
		Assert( p );
		m_pMenu->SetMenuItemChecked( id, ( p && p->GetParent() ) ? true : false );

		id = m_Items.Find( "commentarynodebrowser" );
		m_pMenu->SetItemEnabled( id, true );
		
		p = m_pTool->GetCommentaryNodeBrowser();
		Assert( p );
		m_pMenu->SetMenuItemChecked( id, ( p && p->GetParent() ) ? true : false );

		id = m_Items.Find( "console" );
		m_pMenu->SetItemEnabled( id, true );

		CConsolePage *console = m_pTool->GetConsole();
		m_pMenu->SetMenuItemChecked( id, console->GetParent() );
	}
	else
	{
		id = m_Items.Find( "properties" );
		m_pMenu->SetItemEnabled( id, false );
		id = m_Items.Find( "commentarynodebrowser" );
		m_pMenu->SetItemEnabled( id, false );
		id = m_Items.Find( "console" );
		m_pMenu->SetItemEnabled( id, false );
	}
}


//-----------------------------------------------------------------------------
//
// The Tool menu
//
//-----------------------------------------------------------------------------
class CCommEditToolMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CCommEditToolMenuButton, CToolMenuButton );
public:
	CCommEditToolMenuButton( CCommEditTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget );
	virtual void OnShowMenu(vgui::Menu *menu);

private:
	CCommEditTool *m_pTool;
};

CCommEditToolMenuButton::CCommEditToolMenuButton( CCommEditTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget )
	: BaseClass( parent, panelName, text, pActionSignalTarget )
{
	m_pTool = parent;

	AddMenuItem( "addnewnodes", "#CommEditAddNewNodes", new KeyValues( "AddNewNodes" ), pActionSignalTarget, NULL, "CommEditAddNewNodes" );

	SetMenu(m_pMenu);
}

void CCommEditToolMenuButton::OnShowMenu(vgui::Menu *menu)
{
	BaseClass::OnShowMenu( menu );

	// Update the menu
	int id;

	CCommEditDoc *pDoc = m_pTool->GetDocument();
	id = m_Items.Find( "addnewnodes" );
	m_pMenu->SetItemEnabled( id, pDoc != NULL );
}


//-----------------------------------------------------------------------------
// Initializes the menu bar
//-----------------------------------------------------------------------------
vgui::MenuBar *CCommEditTool::CreateMenuBar( CBaseToolSystem *pParent ) 
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
	CCommEditToolMenuButton *pToolButton = new CCommEditToolMenuButton( this, "CommEdit", "&CommEdit", GetActionTarget() );
	CCommEditViewMenuButton *pViewButton = new CCommEditViewMenuButton( this, "View", "&View", GetActionTarget() );
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
void CCommEditTool::UpdateMenuBar( )
{
	if ( !m_pDoc )
	{
		m_pMenuBar->SetFileName( "#CommEditNoFile" );
		return;
	}

	const char *pTXTFile = m_pDoc->GetTXTFileName();
	if ( !pTXTFile[0] )
	{
		m_pMenuBar->SetFileName( "#CommEditNoFile" );
		return;
	}

	if ( m_pDoc->IsDirty() )
	{
		char sz[ 512 ];
		Q_snprintf( sz, sizeof( sz ), "* %s", pTXTFile );
		m_pMenuBar->SetFileName( sz );
	}
	else
	{
		m_pMenuBar->SetFileName( pTXTFile );
	}
}


//-----------------------------------------------------------------------------
// Gets at tool windows
//-----------------------------------------------------------------------------
CCommentaryPropertiesPanel *CCommEditTool::GetProperties()
{
	return m_hProperties.Get();
}

CCommentaryNodeBrowserPanel *CCommEditTool::GetCommentaryNodeBrowser()
{
	return m_hCommentaryNodeBrowser.Get();
}

CConsolePage *CCommEditTool::GetConsole()
{
	return m_hConsole;
}


//-----------------------------------------------------------------------------
// Shows element properties
//-----------------------------------------------------------------------------
void CCommEditTool::ShowElementProperties( )
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
void CCommEditTool::ShowEntityInEntityProperties( CDmeCommentaryNodeEntity *pEntity )
{
	Assert( m_hProperties.Get() );
	m_hCurrentEntity = pEntity;
	m_hProperties->SetObject( m_hCurrentEntity );
}

	
//-----------------------------------------------------------------------------
// Destroys all tool windows
//-----------------------------------------------------------------------------
void CCommEditTool::DestroyToolContainers()
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
void CCommEditTool::OnDefaultLayout()
{
	int y = m_pMenuBar->GetTall();

	int usew, useh;
	GetSize( usew, useh );

	DestroyToolContainers();

	Assert( ToolWindow::GetToolWindowCount() == 0 );

	CCommentaryPropertiesPanel *properties = GetProperties();
	CCommentaryNodeBrowserPanel *pEntityReport = GetCommentaryNodeBrowser();
	CConsolePage *pConsole = GetConsole();

	// Need three containers
	ToolWindow *pPropertyWindow = m_ToolWindowFactory.InstanceToolWindow( GetClientArea(), false, properties, "#CommEditProperties", false );
	ToolWindow *pEntityReportWindow = m_ToolWindowFactory.InstanceToolWindow( GetClientArea(), false, pEntityReport, "#CommEditEntityReport", false );

	ToolWindow *pMiniViewport = dynamic_cast< ToolWindow* >( GetMiniViewport() );
	pMiniViewport->AddPage( pConsole, "#BxConsole", false );

	int halfScreen = usew / 2;
	int bottom = useh - y;
	int sy = (bottom - y) / 2;
	SetMiniViewportBounds( halfScreen, y, halfScreen, sy - y );
	pEntityReportWindow->SetBounds( 0, y, halfScreen, bottom - y );
	pPropertyWindow->SetBounds( halfScreen, sy, halfScreen, bottom - sy );
}

void CCommEditTool::OnToggleProperties()
{
	if ( m_hProperties.Get() )
	{ 
		ToggleToolWindow( m_hProperties.Get(), "#CommEditProperties" );
	}
}

void CCommEditTool::OnToggleEntityReport()
{
	if ( m_hCommentaryNodeBrowser.Get() )
	{ 
		ToggleToolWindow( m_hCommentaryNodeBrowser.Get(), "#CommEditEntityReport" );
	}
}

void CCommEditTool::OnToggleConsole()
{
	if ( m_hConsole.Get() )
	{
		ToggleToolWindow( m_hConsole.Get(), "#BxConsole" );
	}
}

void CCommEditTool::BringConsoleToFront()
{
	CConsolePage *p = GetConsole();
	Panel *pPage = p ? p->GetParent() : NULL;
	if ( pPage == NULL )
	{
		OnToggleConsole();
	}
	else
	{
		ToolWindow *tw = dynamic_cast< ToolWindow * >( pPage->GetParent() );
		if ( tw )
		{
			if ( tw->GetActivePage() != p )
			{
				tw->SetActivePage( p );
				vgui::surface()->SetForegroundWindow( tw->GetVPanel() );
				p->TextEntryRequestFocus();
			}
			else
			{
				PropertySheet *pSheet = tw->GetPropertySheet();
				int nPageCount = pSheet->GetNumPages();
				int i;
				for ( i = 0; i < nPageCount; ++i )
				{
					if ( p == pSheet->GetPage(i) )
						break;
				}
				i = ( i + 1 ) % nPageCount;
				pSheet->SetActivePage( pSheet->GetPage(i) );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Creates
//-----------------------------------------------------------------------------
void CCommEditTool::CreateTools( CCommEditDoc *doc )
{
	if ( !m_hProperties.Get() )
	{
		m_hProperties = new CCommentaryPropertiesPanel( m_pDoc, this );
	}

	if ( !m_hCommentaryNodeBrowser.Get() )
	{
		m_hCommentaryNodeBrowser = new CCommentaryNodeBrowserPanel( m_pDoc, this, "CommentaryNodeBrowserPanel" );
	}

	if ( !m_hConsole.Get() )
	{
		m_hConsole = new CConsolePage( NULL, false );
	}

	RegisterToolWindow( m_hProperties );
	RegisterToolWindow( m_hCommentaryNodeBrowser );
	RegisterToolWindow( m_hConsole );
}


//-----------------------------------------------------------------------------
// Initializes the tools
//-----------------------------------------------------------------------------
void CCommEditTool::InitTools()
{
	ShowElementProperties();

	// FIXME: There are no tool windows here; how should this work?
	// These panels are saved
	windowposmgr->RegisterPanel( "properties", m_hProperties, false );
	windowposmgr->RegisterPanel( "commentarynodebrowser", m_hCommentaryNodeBrowser, false );
	windowposmgr->RegisterPanel( "Console", m_hConsole, false ); // No context menu

	if ( !windowposmgr->LoadPositions( "cfg/commedit.txt", this, &m_ToolWindowFactory, "CommEdit" ) )
	{
		OnDefaultLayout();
	}
}


void CCommEditTool::DestroyTools()
{
	m_hCurrentEntity = NULL;

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

	if ( m_hCommentaryNodeBrowser.Get() )
	{
		windowposmgr->UnregisterPanel( m_hCommentaryNodeBrowser.Get() );
		delete m_hCommentaryNodeBrowser.Get();
		m_hCommentaryNodeBrowser = NULL;
	}

	if ( m_hConsole.Get() )
	{
		windowposmgr->UnregisterPanel( m_hConsole.Get() );
		delete m_hConsole.Get();
		m_hConsole = NULL;
	}
}


void CCommEditTool::ShowToolWindow( Panel *tool, char const *toolName, bool visible )
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

void CCommEditTool::ToggleToolWindow( Panel *tool, char const *toolName )
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
vgui::Menu *CCommEditTool::CreateActionMenu( vgui::Panel *pParent )
{
	vgui::Menu *pActionMenu = new Menu( pParent, "ActionMenu" );
	pActionMenu->AddMenuItem( "#ToolHide", new KeyValues( "Command", "command", "HideActionMenu" ), GetActionTarget() );
	return pActionMenu;
}


//-----------------------------------------------------------------------------
// Inherited from IFileMenuCallbacks
//-----------------------------------------------------------------------------
int	CCommEditTool::GetFileMenuItemsEnabled( )
{
	int nFlags = FILE_ALL;
	if ( m_RecentFiles.IsEmpty() )
	{
		nFlags &= ~(FILE_RECENT | FILE_CLEAR_RECENT);
	}
	return nFlags;
}

void CCommEditTool::AddRecentFilesToMenu( vgui::Menu *pMenu )
{
	m_RecentFiles.AddToMenu( pMenu, GetActionTarget(), "OnRecent" );
}

bool CCommEditTool::GetPerforceFileName( char *pFileName, int nMaxLen )
{
	if ( !m_pDoc )
		return false;

	Q_strncpy( pFileName, m_pDoc->GetTXTFileName(), nMaxLen );
	return pFileName[0] != 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CCommEditTool::OnExit()
{
	windowposmgr->SavePositions( "cfg/commedit.txt", "CommEdit" );

	enginetools->Command( "quit\n" );
}

//-----------------------------------------------------------------------------
// Handle commands from the action menu and other menus
//-----------------------------------------------------------------------------
void CCommEditTool::OnCommand( const char *cmd )
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
	else if ( const char *pSuffixTool = StringAfterPrefix( cmd, "OnTool" ) )
	{
		int idx = Q_atoi( pSuffixTool );
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
void CCommEditTool::OnNew()
{
	int nFlags = 0;
	const char *pSaveFileName = NULL;
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		nFlags = FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY;
		pSaveFileName = m_pDoc->GetTXTFileName();
	}

	// Bring up the file open dialog to choose a .bsp file
	OpenFile( "bsp", pSaveFileName, "txt", nFlags );
}


//-----------------------------------------------------------------------------
// Called when the File->Open menu is selected
//-----------------------------------------------------------------------------
void CCommEditTool::OnOpen( )
{
	int nFlags = 0;
	const char *pSaveFileName = NULL;
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		nFlags = FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY;
		pSaveFileName = m_pDoc->GetTXTFileName();
	}

	OpenFile( "txt", pSaveFileName, "txt", nFlags );
}

bool CCommEditTool::OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	OnCloseNoSave();
	if ( !LoadDocument( pFileName ) )
		return false;

	m_RecentFiles.Add( pFileName, pFileFormat );
	m_RecentFiles.SaveToRegistry( GetRegistryName() );
	UpdateMenuBar();
	return true;
}

void CCommEditTool::Save()
{
	if ( m_pDoc )
	{
		SaveFile( m_pDoc->GetTXTFileName(), "txt", FOSM_SHOW_PERFORCE_DIALOGS );
	}
}

void CCommEditTool::OnSaveAs()
{
	if ( m_pDoc )
	{
		SaveFile( NULL, "txt", FOSM_SHOW_PERFORCE_DIALOGS );
	}
}

void CCommEditTool::OnRestartLevel()
{
	enginetools->Command( "restart" );
	enginetools->Execute();

	CDmrCommentaryNodeEntityList entities = m_pDoc->GetEntityList();
	int nCount = entities.IsValid() ? entities.Count() : 0;
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeCommentaryNodeEntity *pEntity = entities[i];
		Assert( pEntity );
		pEntity->MarkDirty( false );
	}
}

void CCommEditTool::SaveAndTest()
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		SaveFile( m_pDoc->GetTXTFileName(), "txt", FOSM_SHOW_PERFORCE_DIALOGS, 
			new KeyValues( "RestartLevel" ) );
	}
	else
	{
		OnRestartLevel();
	}
}

bool CCommEditTool::OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	if ( !m_pDoc )
		return false;

	m_pDoc->SetTXTFileName( pFileName );
	m_pDoc->SaveToFile( );

	m_RecentFiles.Add( pFileName, pFileFormat );
	m_RecentFiles.SaveToRegistry( GetRegistryName() );
	UpdateMenuBar();
	return true;
}

void CCommEditTool::OnClose()
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		SaveFile( m_pDoc->GetTXTFileName(), "txt", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY, 
			new KeyValues( "OnClose" ) );
		return;
	}

	OnCloseNoSave();
}

void CCommEditTool::OnCloseNoSave()
{
	DestroyTools();

	if ( m_pDoc )
	{
		CAppNotifyScopeGuard sg( "CCommEditTool::OnCloseNoSave", 0 );

		delete m_pDoc;
		m_pDoc = NULL;

		if ( m_hProperties )
		{
			m_hProperties->SetObject( NULL );
		}
	}

	UpdateMenuBar( );
}

void CCommEditTool::CenterView( CDmeCommentaryNodeEntity *pEntity )
{
	EntitySearchResult pPlayer = clienttools->GetLocalPlayer();

	Vector vecOrigin = pEntity->GetRenderOrigin();
	QAngle angles = pEntity->GetRenderAngles();

	Vector vecForward, vecUp, vecRight;
	AngleVectors( angles, &vecForward, &vecRight, &vecUp );
	VectorMA( vecOrigin, 40.0f, vecForward, vecOrigin );
	vecForward *= -1.0f;
	VectorAngles( vecForward, vecUp, angles );

	servertools->SnapPlayerToPosition( vecOrigin, angles, ( IClientEntity* )pPlayer );
}


void CCommEditTool::OnMarkNotDirty()
{
	if ( m_pDoc )
	{
		m_pDoc->SetDirty( false );
	}
}

void CCommEditTool::AttachAllEngineEntities()
{
	if ( !clienttools || !m_pDoc )
		return;

	for ( EntitySearchResult sr = clienttools->FirstEntity(); sr != NULL; sr = clienttools->NextEntity( sr ) )
	{
		if ( !sr )
			continue;

		HTOOLHANDLE handle = clienttools->AttachToEntity( sr );

		const char *pClassName = clienttools->GetClassname( handle );
		if ( Q_strcmp( pClassName, "class C_PointCommentaryNode" ) == 0 )
		{
			Vector vecOrigin = clienttools->GetAbsOrigin( handle );
			QAngle angAngles = clienttools->GetAbsAngles( handle );

			// Find the associated commentary node entry in our doc
			CDmeCommentaryNodeEntity *pNode = m_pDoc->GetCommentaryNodeForLocation( vecOrigin, angAngles );
			if ( pNode )
			{
				pNode->AttachToEngineEntity( handle );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Show the save document query dialog
//-----------------------------------------------------------------------------
void CCommEditTool::OpenSpecificFile( const char *pFileName )
{
	int nFlags = 0;
	const char *pSaveFileName = NULL;
	if ( m_pDoc )
	{
		// File is already open
		if ( !Q_stricmp( m_pDoc->GetTXTFileName(), pFileName ) )
			return;

		if ( m_pDoc->IsDirty() )
		{
			nFlags = FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY;
			pSaveFileName = m_pDoc->GetTXTFileName();
		}
		else
		{
			OnCloseNoSave();
		}
	}

	OpenFile( pFileName, "txt", pSaveFileName, "txt", nFlags );
}

void CCommEditTool::OpenFileFromHistory( int slot )
{
	const char *pFileName = m_RecentFiles.GetFile( slot );
	if ( !pFileName )
		return;
	OpenSpecificFile( pFileName );
}


//-----------------------------------------------------------------------------
// Derived classes can implement this to get notified when files are saved/loaded
//-----------------------------------------------------------------------------
void CCommEditTool::OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues )
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
void CCommEditTool::SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	char pStartingDir[ MAX_PATH ];

	GetModSubdirectory( "maps", pStartingDir, sizeof(pStartingDir) );

	if ( !Q_stricmp( pFileFormat, "bsp" ) )
	{
		// Open a bsp file to create a new commentary file
		pDialog->SetTitle( "Choose BSP File", true );
		pDialog->SetStartDirectoryContext( "commedit_bsp_session", pStartingDir );
		pDialog->AddFilter( "*.bsp", "Valve BSP File (*.bsp)", true );
	}
	else
	{
		// Open existing commentary text files
		pDialog->SetTitle( "Choose Valve Commentary File", true );
		pDialog->SetStartDirectoryContext( "commedit_txt_session", pStartingDir );
		pDialog->AddFilter( "*.txt", "Valve Commentary File (*.txt)", true );
	}
}


//-----------------------------------------------------------------------------
// Can we quit?
//-----------------------------------------------------------------------------
bool CCommEditTool::CanQuit()
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		// Show Save changes Yes/No/Cancel and re-quit if hit yes/no
		SaveFile( m_pDoc->GetTXTFileName(), "txt", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY, 
			new KeyValues( "OnQuit" ) );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Various command handlers related to the Edit menu
//-----------------------------------------------------------------------------
void CCommEditTool::OnUndo()
{
	CDisableUndoScopeGuard guard;
	g_pDataModel->Undo();
}

void CCommEditTool::OnRedo()
{
	CDisableUndoScopeGuard guard;
	g_pDataModel->Redo();
}

void CCommEditTool::OnDescribeUndo()
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
// CommEdit menu items
//-----------------------------------------------------------------------------
void CCommEditTool::OnAddNewNodes()
{
	if ( !m_pDoc )
		return;

	EnterNodeDropMode();
}


//-----------------------------------------------------------------------------
// Background
//-----------------------------------------------------------------------------
const char *CCommEditTool::GetLogoTextureName()
{
	return NULL;
}


//-----------------------------------------------------------------------------
// Inherited from ICommEditDocCallback
//-----------------------------------------------------------------------------
void CCommEditTool::OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	if ( GetCommentaryNodeBrowser() )
	{
		GetCommentaryNodeBrowser()->UpdateEntityList();
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
bool CCommEditTool::LoadDocument( const char *pDocName )
{
	Assert( !m_pDoc );

	DestroyTools();

	m_pDoc = new CCommEditDoc( this );
	if ( !m_pDoc->LoadFromFile( pDocName ) )
	{
		delete m_pDoc;
		m_pDoc = NULL;
		Warning( "Fatal error loading '%s'\n", pDocName );
		return false;
	}

	ShowMiniViewport( true );

	CreateTools( m_pDoc );
	InitTools();
	return true;
}


//-----------------------------------------------------------------------------
// Create the entities that are in our TXT file
//-----------------------------------------------------------------------------
const char* CCommEditTool::GetEntityData( const char *pActualEntityData )
{
//	if ( !m_pDoc )
		return pActualEntityData;

//	return m_pDoc->GenerateEntityData( pActualEntityData );
}
