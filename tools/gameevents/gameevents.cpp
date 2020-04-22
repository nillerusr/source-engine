//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "toolutils/basetoolsystem.h"
#include "toolutils/recentfilelist.h"
#include "toolutils/toolmenubar.h"
#include "toolutils/toolswitchmenubutton.h"
#include "toolutils/toolfilemenubutton.h"
#include "toolutils/toolmenubutton.h"
#include "vgui_controls/Menu.h"
#include "tier1/KeyValues.h"
#include "toolutils/enginetools_int.h"
#include "toolframework/ienginetool.h"
#include "vgui/IInput.h"
#include "vgui/KeyCode.h"
#include "vgui_controls/FileOpenDialog.h"
#include "filesystem.h"
#include "vgui/ilocalize.h"
#include "dme_controls/elementpropertiestree.h"
#include "tier0/icommandline.h"
#include "materialsystem/imaterialsystem.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "tier3/tier3.h"
#include "tier2/fileutils.h"
#include "gameeventeditdoc.h"
#include "gameeventeditpanel.h"
#include "toolutils/toolwindowfactory.h"
#include "toolutils/savewindowpositions.h"	// for windowposmgr
#include "toolutils/ConsolePage.h"
#include "igameevents.h"

IGameEventManager2 *gameeventmanager = NULL;

using namespace vgui;


const char *GetVGuiControlsModuleName()
{
	return "GameEvents";
}

//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool ConnectTools( CreateInterfaceFn factory )
{
	gameeventmanager = ( IGameEventManager2 * )factory( INTERFACEVERSION_GAMEEVENTSMANAGER2, NULL );

	if ( !gameeventmanager )
	{
		Warning( "Game Event Tool missing required interface\n" );
		return false;
	}

	return (materials != NULL) && (g_pMatSystemSurface != NULL);
}

void DisconnectTools( )
{
}


//-----------------------------------------------------------------------------
// Implementation of the game events tool
//-----------------------------------------------------------------------------
class CGameEventTool : public CBaseToolSystem, public IFileMenuCallbacks
{
	DECLARE_CLASS_SIMPLE( CGameEventTool, CBaseToolSystem );

public:
	CGameEventTool();

	// Inherited from IToolSystem
	virtual const char *GetToolName() { return "Game Events"; }
	virtual const char *GetBindingsContextFile() { return "cfg/GameEvents.kb"; }
	virtual bool	Init( );
    virtual void	Shutdown();

	// Inherited from IFileMenuCallbacks
	virtual int		GetFileMenuItemsEnabled( );
	virtual void	AddRecentFilesToMenu( vgui::Menu *menu );
	virtual bool	GetPerforceFileName( char *pFileName, int nMaxLen ) { return false; }
	virtual vgui::Panel* GetRootPanel() { return this; }

	// Inherited from CBaseToolSystem
	virtual vgui::HScheme GetToolScheme();
	virtual vgui::Menu *CreateActionMenu( vgui::Panel *pParent );
	virtual void OnCommand( const char *cmd );
	virtual const char *GetRegistryName() { return "SampleTool"; }
	virtual vgui::MenuBar *CreateMenuBar( CBaseToolSystem *pParent );

public:
	MESSAGE_FUNC( OnNew, "OnNew" );
	MESSAGE_FUNC( OnOpen, "OnOpen" );
	MESSAGE_FUNC( OnSave, "OnSave" );
	MESSAGE_FUNC( OnSaveAs, "OnSaveAs" );
	MESSAGE_FUNC( OnClose, "OnClose" );
	MESSAGE_FUNC( OnCloseNoSave, "OnCloseNoSave" );
	MESSAGE_FUNC( OnMarkNotDirty, "OnMarkNotDirty" );
	MESSAGE_FUNC( OnExit, "OnExit" );

	void NewDocument();

	void		OpenFileFromHistory( int slot );
	virtual void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual void OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues );

	// Get at the edit panel
	CGameEventEditPanel *GetGameEventEditPanel();

private:
	// Loads up a new document
	bool LoadDocument( const char *pDocName );

	// Creates, destroys tools
	void CreateTools( CGameEventEditDoc *doc );
	void InitTools();
	void DestroyTools();

	void OnDefaultLayout();

	// Updates the menu bar based on the current file
	void UpdateMenuBar( );

	// Shows element properties
	void ShowElementProperties( );

	virtual const char *GetLogoTextureName();

	CConsolePage *GetConsole();
	void OnToggleConsole();

	void ShowToolWindow( Panel *tool, char const *toolName, bool visible );
	void ToggleToolWindow( Panel *tool, char const *toolName );

	CToolWindowFactory< ToolWindow > m_ToolWindowFactory;


	// The entity report
	vgui::DHANDLE< CGameEventEditPanel > m_hGameEventEditPanel;

	// Document
	CGameEventEditDoc *m_pDoc;

	// The menu bar
	CToolFileMenuBar *m_pMenuBar;

	// console tab in viewport
	vgui::DHANDLE< CConsolePage >				m_hConsole;
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CGameEventTool	*g_pSampleTool = NULL;

void CreateTools()
{
	g_pSampleTool = new CGameEventTool();
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CGameEventTool::CGameEventTool()
{
	m_pMenuBar = NULL;
	m_pDoc = NULL;
}

//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CGameEventTool::Init( )
{
	m_RecentFiles.LoadFromRegistry( GetRegistryName() );

	// NOTE: This has to happen before BaseClass::Init
	g_pVGuiLocalize->AddFile( "resource/toolgameevents_%language%.txt" );

	if ( !BaseClass::Init( ) )
		return false;

	return true;
}

void CGameEventTool::Shutdown()
{
	m_RecentFiles.SaveToRegistry( GetRegistryName() );
	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Derived classes can implement this to get a new scheme to be applied to this tool
//-----------------------------------------------------------------------------
vgui::HScheme CGameEventTool::GetToolScheme() 
{ 
	return vgui::scheme()->LoadSchemeFromFile( "Resource/BoxRocket.res", "SampleTool" );
}


//-----------------------------------------------------------------------------
// Initializes the menu bar
//-----------------------------------------------------------------------------
vgui::MenuBar *CGameEventTool::CreateMenuBar( CBaseToolSystem *pParent ) 
{
	m_pMenuBar = new CToolFileMenuBar( pParent, "Main Menu Bar" );

	// Sets info in the menu bar
	char title[ 64 ];
	ComputeMenuBarTitle( title, sizeof( title ) );
	m_pMenuBar->SetInfo( title );
	m_pMenuBar->SetToolName( GetToolName() );

	// Add menu buttons
	CToolMenuButton *pFileButton = CreateToolFileMenuButton( m_pMenuBar, "File", "&File", GetActionTarget(), this );
	CToolMenuButton *pSwitchButton = CreateToolSwitchMenuButton( m_pMenuBar, "Switcher", "&Tools", GetActionTarget() );

	m_pMenuBar->AddButton( pFileButton );
	m_pMenuBar->AddButton( pSwitchButton );

	return m_pMenuBar;
}


//-----------------------------------------------------------------------------
// Creates the action menu
//-----------------------------------------------------------------------------
vgui::Menu *CGameEventTool::CreateActionMenu( vgui::Panel *pParent )
{
	vgui::Menu *pActionMenu = new Menu( pParent, "ActionMenu" );
	pActionMenu->AddMenuItem( "#ToolHide", new KeyValues( "Command", "command", "HideActionMenu" ), GetActionTarget() );
	return pActionMenu;
}

//-----------------------------------------------------------------------------
// Inherited from IFileMenuCallbacks
//-----------------------------------------------------------------------------
int	CGameEventTool::GetFileMenuItemsEnabled( )
{
	int nFlags = FILE_ALL;
	if ( m_RecentFiles.IsEmpty() )
	{
		nFlags &= ~(FILE_RECENT | FILE_CLEAR_RECENT);
	}
	return nFlags;
}

void CGameEventTool::AddRecentFilesToMenu( vgui::Menu *pMenu )
{
	m_RecentFiles.AddToMenu( pMenu, GetActionTarget(), "OnRecent" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CGameEventTool::OnExit()
{
	enginetools->Command( "quit\n" );
}

//-----------------------------------------------------------------------------
// Handle commands from the action menu and other menus
//-----------------------------------------------------------------------------
void CGameEventTool::OnCommand( const char *cmd )
{
	if ( !V_stricmp( cmd, "HideActionMenu" ) )
	{
		if ( GetActionMenu() )
		{
			GetActionMenu()->SetVisible( false );
		}
	}
	else if ( const char *pOnRecentSuffix = StringAfterPrefix( cmd, "OnRecent" ) )
	{
		int idx = Q_atoi( pOnRecentSuffix );
		OpenFileFromHistory( idx );
	}
	else if( const char *pOnToolSuffix = StringAfterPrefix( cmd, "OnTool" ) )
	{
		int idx = Q_atoi( pOnToolSuffix );
		enginetools->SwitchToTool( idx );
	}
	else
	{
		BaseClass::OnCommand( cmd );
	}
}


//-----------------------------------------------------------------------------
// Command handlers
//-----------------------------------------------------------------------------
void CGameEventTool::OnNew()
{
	if ( m_pDoc )
	{
		if ( m_pDoc->IsDirty() )
		{
			SaveFile( m_pDoc->GetTXTFileName(), "xt", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY,
				new KeyValues( "OnNew" ) );
			return;
		}
	}

	NewDocument();
}

//-----------------------------------------------------------------------------
// Loads up a new document
//-----------------------------------------------------------------------------
void CGameEventTool::NewDocument( )
{
	Assert( !m_pDoc );

	m_pDoc = new CGameEventEditDoc(/* this */);

	CreateTools( m_pDoc );
	UpdateMenuBar();
	InitTools();
}

//-----------------------------------------------------------------------------
// Called when the File->Open menu is selected
//-----------------------------------------------------------------------------
void CGameEventTool::OnOpen( )
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

bool CGameEventTool::OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	OnCloseNoSave();
	if ( !LoadDocument( pFileName ) )
		return false;

	m_RecentFiles.Add( pFileName, pFileFormat );
	m_RecentFiles.SaveToRegistry( GetRegistryName() );
	UpdateMenuBar();
	return true;
}

//-----------------------------------------------------------------------------
// Updates the menu bar based on the current file
//-----------------------------------------------------------------------------
void CGameEventTool::UpdateMenuBar( )
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


void CGameEventTool::OnSave()
{
	// FIXME: Implement
}

void CGameEventTool::OnSaveAs()
{
	SaveFile( NULL, NULL, 0 );
}

bool CGameEventTool::OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	// FIXME: Implement

	m_RecentFiles.Add( pFileName, pFileFormat );
	return true;
}

void CGameEventTool::OnClose()
{
	// FIXME: Implement
}

void CGameEventTool::OnCloseNoSave()
{
	// FIXME: Implement
}

void CGameEventTool::OnMarkNotDirty()
{
	// FIXME: Implement
}


//-----------------------------------------------------------------------------
// Show the save document query dialog
//-----------------------------------------------------------------------------
void CGameEventTool::OpenFileFromHistory( int slot )
{
	const char *pFileName = m_RecentFiles.GetFile( slot );
	OnReadFileFromDisk( pFileName, NULL, 0 );
}


//-----------------------------------------------------------------------------
// Called when file operations complete
//-----------------------------------------------------------------------------
void CGameEventTool::OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues )
{
	// FIXME: Implement
}


//-----------------------------------------------------------------------------
// Show the File browser dialog
//-----------------------------------------------------------------------------
void CGameEventTool::SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	char pStartingDir[ MAX_PATH ];
	GetModSubdirectory( NULL, pStartingDir, sizeof(pStartingDir) );

	pDialog->SetTitle( "Choose SampleTool .txt file", true );
	pDialog->SetStartDirectoryContext( "sample_session", pStartingDir );
	pDialog->AddFilter( "*.txt", "SampleTool (*.txt)", true );
}

const char *CGameEventTool::GetLogoTextureName()
{
	return "vgui/tools/sampletool/sampletool_logo";
}

CGameEventEditPanel *CGameEventTool::GetGameEventEditPanel()
{
	return m_hGameEventEditPanel.Get();
}

//-----------------------------------------------------------------------------
// Creates
//-----------------------------------------------------------------------------
void CGameEventTool::CreateTools( CGameEventEditDoc *doc )
{
	if ( !m_hGameEventEditPanel.Get() )
	{
		m_hGameEventEditPanel = new CGameEventEditPanel( m_pDoc, this );
	}

	if ( !m_hConsole.Get() )
	{
		m_hConsole = new CConsolePage( NULL, false );
	}

	RegisterToolWindow( m_hGameEventEditPanel );
	RegisterToolWindow( m_hConsole );
}

//-----------------------------------------------------------------------------
// Initializes the tools
//-----------------------------------------------------------------------------
void CGameEventTool::InitTools()
{
	//ShowElementProperties();
	
	windowposmgr->RegisterPanel( "gameeventedit", m_hGameEventEditPanel, false );
	windowposmgr->RegisterPanel( "Console", m_hConsole, false ); // No context menu
	
	if ( !windowposmgr->LoadPositions( "cfg/commedit.txt", this, &m_ToolWindowFactory, "CommEdit" ) )
	{
		OnDefaultLayout();
	}
	
}

void CGameEventTool::DestroyTools()
{
	UnregisterAllToolWindows();

	if ( m_hGameEventEditPanel.Get() )
	{
		windowposmgr->UnregisterPanel( m_hGameEventEditPanel.Get() );
		delete m_hGameEventEditPanel.Get();
		m_hGameEventEditPanel = NULL;
	}

	if ( m_hConsole.Get() )
	{
		windowposmgr->UnregisterPanel( m_hConsole.Get() );
		delete m_hConsole.Get();
		m_hConsole = NULL;
	}
}

//-----------------------------------------------------------------------------
// Loads up a new document
//-----------------------------------------------------------------------------
bool CGameEventTool::LoadDocument( const char *pDocName )
{
	Assert( !m_pDoc );

	DestroyTools();

	m_pDoc = new CGameEventEditDoc(/* this */);
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

CConsolePage *CGameEventTool::GetConsole()
{
	return m_hConsole;
}


void CGameEventTool::ShowToolWindow( Panel *tool, char const *toolName, bool visible )
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

void CGameEventTool::ToggleToolWindow( Panel *tool, char const *toolName )
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


void CGameEventTool::OnToggleConsole()
{
	if ( m_hConsole.Get() )
	{
		ToggleToolWindow( m_hConsole.Get(), "#BxConsole" );
	}
}


//-----------------------------------------------------------------------------
// Sets up the default layout
//-----------------------------------------------------------------------------
void CGameEventTool::OnDefaultLayout()
{
	int y = m_pMenuBar->GetTall();

	int usew, useh;
	GetSize( usew, useh );

	int c = ToolWindow::GetToolWindowCount();
	for ( int i = c - 1; i >= 0 ; --i )
	{
		ToolWindow *kill = ToolWindow::GetToolWindow( i );
		delete kill;
	}

	Assert( ToolWindow::GetToolWindowCount() == 0 );

	CGameEventEditPanel *pEditPanel = GetGameEventEditPanel();
	CConsolePage *pConsole = GetConsole();

	// Need three containers
	ToolWindow *pEditPanelWindow = m_ToolWindowFactory.InstanceToolWindow( GetClientArea(), false, pEditPanel, "#CommEditProperties", false );
	ToolWindow *pMiniViewport = dynamic_cast< ToolWindow* >( GetMiniViewport() );
	pMiniViewport->AddPage( pConsole, "#BxConsole", false );

	int quarterScreen = usew / 4;
	SetMiniViewportBounds( quarterScreen, y, 3*quarterScreen, useh - y );
	pEditPanelWindow->SetBounds( 0, y, quarterScreen, useh - y );
}


