//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#include "foundrytool.h"
#include "toolutils/basetoolsystem.h"
#include "toolutils/recentfilelist.h"
#include "toolutils/toolmenubar.h"
#include "toolutils/toolswitchmenubutton.h"
#include "toolutils/tooleditmenubutton.h"
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
#include "toolutils/savewindowpositions.h"
#include "foundrydoc.h"
#include "toolutils/toolwindowfactory.h"
#include "toolutils/basepropertiescontainer.h"
#include "entityreportpanel.h"
#include "datamodel/dmelement.h"
#include "movieobjects/dmeeditortypedictionary.h"
#include "dmevmfentity.h"
#include "tier3/tier3.h"
#include "tier2/fileutils.h"
#include "vgui/ivgui.h"


using namespace vgui;

//-----------------------------------------------------------------------------
// Singleton interfaces
//-----------------------------------------------------------------------------
CDmeEditorTypeDictionary *g_pEditorTypeDict;


const char *GetVGuiControlsModuleName()
{
	return "FoundryTool";
}

//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool ConnectTools( CreateInterfaceFn factory )
{
	return (materials != NULL) && (g_pMatSystemSurface != NULL);
}

void DisconnectTools( )
{
}


//-----------------------------------------------------------------------------
// Implementation of the Foundry tool
//-----------------------------------------------------------------------------
class CFoundryTool : public CBaseToolSystem, public IFileMenuCallbacks, public IFoundryDocCallback, public IFoundryTool
{
	DECLARE_CLASS_SIMPLE( CFoundryTool, CBaseToolSystem );

public:
	CFoundryTool();

	// Inherited from IToolSystem
	virtual const char *GetToolName() { return "Foundry"; }
	virtual const char *GetBindingsContextFile() { return "cfg/Foundry.kb"; }
	virtual bool	Init( );
    virtual void	Shutdown();
	virtual bool	CanQuit();
	virtual void	OnToolActivate();
	virtual void	OnToolDeactivate();
	virtual const char* GetEntityData( const char *pActualEntityData );
	virtual void	ClientLevelInitPostEntity();
	virtual void	ClientLevelShutdownPreEntity();

	// Inherited from IFileMenuCallbacks
	virtual int		GetFileMenuItemsEnabled( );
	virtual void	AddRecentFilesToMenu( vgui::Menu *menu );
	virtual bool	GetPerforceFileName( char *pFileName, int nMaxLen );

	// Inherited from IFoundryDocCallback
	virtual void	OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags );
	virtual vgui::Panel *GetRootPanel() { return this; }
	virtual void ShowEntityInEntityProperties( CDmeVMFEntity *pEntity );

	// Inherited from CBaseToolSystem
	virtual vgui::HScheme GetToolScheme();
	virtual vgui::Menu *CreateActionMenu( vgui::Panel *pParent );
	virtual void OnCommand( const char *cmd );
	virtual const char *GetRegistryName() { return "FoundryTool"; }
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

	// Commands related to the edit menu
	KEYBINDING_FUNC( undo, KEY_Z, vgui::MODIFIER_CONTROL, OnUndo, "#undo_help", 0 );
	KEYBINDING_FUNC( redo, KEY_Z, vgui::MODIFIER_CONTROL | vgui::MODIFIER_SHIFT, OnRedo, "#redo_help", 0 );
	void		OnDescribeUndo();

	// Methods related to the Foundry menu
	MESSAGE_FUNC( OnReload, "ReloadMap" );
	MESSAGE_FUNC( OnReloadFromSave, "ReloadFromSave" );

	// Methods related to the view menu
	MESSAGE_FUNC( OnToggleProperties, "OnToggleProperties" );
	MESSAGE_FUNC( OnToggleEntityReport, "OnToggleEntityReport" );
	MESSAGE_FUNC( OnDefaultLayout, "OnDefaultLayout" );

	void		PerformNew();
	void		OpenFileFromHistory( int slot );
	void		OpenSpecificFile( const char *pFileName );
	virtual void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual void OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues );

	// returns the document
	CFoundryDoc *GetDocument();

	// Gets at tool windows
	CBasePropertiesContainer *GetProperties();
	CEntityReportPanel *GetEntityReport();

private:
	// Loads up a new document
	bool LoadDocument( const char *pDocName );

	// Updates the menu bar based on the current file
	void UpdateMenuBar( );

	// Shows element properties
	void ShowElementProperties( );

	virtual const char *GetLogoTextureName();

	// Creates, destroys tools
	void CreateTools( CFoundryDoc *doc );
	void DestroyTools();

	// Initializes the tools
	void InitTools();

	// Shows, toggles tool windows
	void ToggleToolWindow( Panel *tool, char const *toolName );
	void ShowToolWindow( Panel *tool, char const *toolName, bool visible );

	// Kills all tool windows
	void DestroyToolContainers();

	// Create custom editors
	void InitEditorDict();

	// Used to hook DME VMF entities into the render lists
	void DrawVMFEntitiesInEngine( bool bDrawInEngine );

private:
	// Document
	CFoundryDoc *m_pDoc;

	// The menu bar
	CToolFileMenuBar *m_pMenuBar;

	// Element properties for editing material
	vgui::DHANDLE< CBasePropertiesContainer >	m_hProperties;

	// The entity report
	vgui::DHANDLE< CEntityReportPanel > m_hEntityReport;

	// The currently viewed entity
	CDmeHandle< CDmeVMFEntity > m_hCurrentEntity;

	// Separate undo context for the act busy tool
	CToolWindowFactory< ToolWindow > m_ToolWindowFactory;

	CUtlVector< DmElementHandle_t > m_toolElements;
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CFoundryTool	*g_pFoundryToolImp = NULL;
IFoundryTool	*g_pFoundryTool = NULL;

void CreateTools()
{
	g_pFoundryTool = g_pFoundryToolImp = new CFoundryTool();
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CFoundryTool::CFoundryTool()
{
	m_pMenuBar = NULL;
	m_pDoc = NULL;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CFoundryTool::Init( )
{
	m_pDoc = NULL;
	m_RecentFiles.LoadFromRegistry( GetRegistryName() );

	// NOTE: This has to happen before BaseClass::Init
	g_pVGuiLocalize->AddFile( "resource/toolfoundry_%language%.txt" );

	if ( !BaseClass::Init( ) )
		return false;

	InitEditorDict();

	return true;
}

void CFoundryTool::Shutdown()
{
	m_RecentFiles.SaveToRegistry( GetRegistryName() );

	{
		CDisableUndoScopeGuard guard;
		int nElements = m_toolElements.Count();
		for ( int i = 0; i < nElements; ++i )
		{
			g_pDataModel->DestroyElement( m_toolElements[ i ] );
		}
	}

	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Create custom editors
//-----------------------------------------------------------------------------
void CFoundryTool::InitEditorDict()
{
	CDmeEditorAttributeInfo *pInfo;

	// FIXME: This eventually will move to an .fgd-like file.
	g_pEditorTypeDict = CreateElement<CDmeEditorTypeDictionary>( "DmeEditorTypeDictionary", DMFILEID_INVALID );
	m_toolElements.AddToTail( g_pEditorTypeDict->GetHandle() );

	CDmeEditorType *pEditorType = CreateElement<CDmeEditorType>( "vmfEntity", DMFILEID_INVALID );
	m_toolElements.AddToTail( pEditorType->GetHandle() );

	pInfo = CreateElement<CDmeEditorAttributeInfo>( "name info", DMFILEID_INVALID );
	pInfo->m_bIsVisible = false;
	pEditorType->AddAttributeInfo( "name", pInfo ); 
	m_toolElements.AddToTail( pInfo->GetHandle() );

	pInfo = CreateElement<CDmeEditorAttributeInfo>( "type info", DMFILEID_INVALID );
	pInfo->m_bIsVisible = false;
	pEditorType->AddAttributeInfo( "type", pInfo );
	m_toolElements.AddToTail( pInfo->GetHandle() );

	pInfo = CreateElement<CDmeEditorAttributeInfo>( "id info", DMFILEID_INVALID );
	pInfo->m_bIsVisible = false;
	pEditorType->AddAttributeInfo( "id", pInfo ); 
	m_toolElements.AddToTail( pInfo->GetHandle() );

	pInfo = CreateElement<CDmeEditorAttributeInfo>( "editor type info", DMFILEID_INVALID );
	pInfo->m_bIsVisible = false;
	pEditorType->AddAttributeInfo( "editorType", pInfo ); 
	m_toolElements.AddToTail( pInfo->GetHandle() );

	pInfo = CreateElement<CDmeEditorAttributeInfo>( "editor info", DMFILEID_INVALID );
	pInfo->m_bIsVisible = false;
	pEditorType->AddAttributeInfo( "editor", pInfo ); 
	m_toolElements.AddToTail( pInfo->GetHandle() );

	pInfo = CreateElement<CDmeEditorAttributeInfo>( "other info", DMFILEID_INVALID );
	pInfo->m_bIsVisible = false;
	pEditorType->AddAttributeInfo( "other", pInfo ); 
	m_toolElements.AddToTail( pInfo->GetHandle() );

	pInfo = CreateElement<CDmeEditorAttributeInfo>( "_visible info", DMFILEID_INVALID );
	pInfo->m_bIsVisible = false;
	pEditorType->AddAttributeInfo( "_visible", pInfo ); 
	m_toolElements.AddToTail( pInfo->GetHandle() );

	pInfo = CreateElement<CDmeEditorAttributeInfo>( "_placeholder info", DMFILEID_INVALID );
	pInfo->m_bIsVisible = false;
	pEditorType->AddAttributeInfo( "_placeholder", pInfo ); 
	m_toolElements.AddToTail( pInfo->GetHandle() );

	g_pEditorTypeDict->AddEditorType( pEditorType );
}


//-----------------------------------------------------------------------------
// returns the document
//-----------------------------------------------------------------------------
inline CFoundryDoc *CFoundryTool::GetDocument()
{
	return m_pDoc;
}

	
//-----------------------------------------------------------------------------
// Tool activation/deactivation
//-----------------------------------------------------------------------------
void CFoundryTool::OnToolActivate()
{
	BaseClass::OnToolActivate();
}

void CFoundryTool::OnToolDeactivate()
{
	BaseClass::OnToolDeactivate();
}


//-----------------------------------------------------------------------------
// Used to hook DME VMF entities into the render lists
//-----------------------------------------------------------------------------
void CFoundryTool::DrawVMFEntitiesInEngine( bool bDrawInEngine )
{
	if ( !m_pDoc )
		return;

	const CDmrElementArray<> entities( m_pDoc->GetEntityList() );
	int nCount = entities.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeVMFEntity* pEntity = CastElement<CDmeVMFEntity>( entities[i] );
		Assert( pEntity );
		pEntity->DrawInEngine( bDrawInEngine );
		pEntity->AttachToEngineEntity( bDrawInEngine ); 
	}
}

void CFoundryTool::ClientLevelInitPostEntity()
{
	BaseClass::ClientLevelInitPostEntity();
	DrawVMFEntitiesInEngine( true );
}

void CFoundryTool::ClientLevelShutdownPreEntity()
{
	DrawVMFEntitiesInEngine( false );
	BaseClass::ClientLevelShutdownPreEntity();
}

	
//-----------------------------------------------------------------------------
// Derived classes can implement this to get a new scheme to be applied to this tool
//-----------------------------------------------------------------------------
vgui::HScheme CFoundryTool::GetToolScheme() 
{ 
	return vgui::scheme()->LoadSchemeFromFile( "Resource/BoxRocket.res", "BoxRocket" );
}


//-----------------------------------------------------------------------------
//
// The View menu
//
//-----------------------------------------------------------------------------
class CFoundryViewMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CFoundryViewMenuButton, CToolMenuButton );
public:
	CFoundryViewMenuButton( CFoundryTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget );
	virtual void OnShowMenu(vgui::Menu *menu);

private:
	CFoundryTool *m_pTool;
};

CFoundryViewMenuButton::CFoundryViewMenuButton( CFoundryTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget )
	: BaseClass( parent, panelName, text, pActionSignalTarget )
{
	m_pTool = parent;

	AddCheckableMenuItem( "properties", "#FoundryProperties", new KeyValues( "OnToggleProperties" ), pActionSignalTarget );
	AddCheckableMenuItem( "entityreport", "#FoundryEntityReport", new KeyValues( "OnToggleEntityReport" ), pActionSignalTarget );

	AddSeparator();

	AddMenuItem( "defaultlayout", "#FoundryViewDefault", new KeyValues( "OnDefaultLayout" ), pActionSignalTarget );

	SetMenu(m_pMenu);
}

void CFoundryViewMenuButton::OnShowMenu(vgui::Menu *menu)
{
	BaseClass::OnShowMenu( menu );

	// Update the menu
	int id;

	CFoundryDoc *pDoc = m_pTool->GetDocument();
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
		
		p = m_pTool->GetEntityReport();
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
class CFoundryToolMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CFoundryToolMenuButton, CToolMenuButton );
public:
	CFoundryToolMenuButton( CFoundryTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget );
	virtual void OnShowMenu(vgui::Menu *menu);

private:
	CFoundryTool *m_pTool;
};

CFoundryToolMenuButton::CFoundryToolMenuButton( CFoundryTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget )
	: BaseClass( parent, panelName, text, pActionSignalTarget )
{
	m_pTool = parent;

	AddMenuItem( "reload", "#FoundryReload", new KeyValues( "ReloadMap" ), pActionSignalTarget );
	AddMenuItem( "reloadsave", "#FoundryReloadFromSave", new KeyValues( "ReloadFromSave" ), pActionSignalTarget );

	SetMenu(m_pMenu);
}

void CFoundryToolMenuButton::OnShowMenu(vgui::Menu *menu)
{
	BaseClass::OnShowMenu( menu );

	// Update the menu
	int id;

	CFoundryDoc *pDoc = m_pTool->GetDocument();
	id = m_Items.Find( "reload" );
	m_pMenu->SetItemEnabled( id, pDoc != NULL );
	id = m_Items.Find( "reloadsave" );
	m_pMenu->SetItemEnabled( id, pDoc != NULL  );
}


//-----------------------------------------------------------------------------
// Initializes the menu bar
//-----------------------------------------------------------------------------
vgui::MenuBar *CFoundryTool::CreateMenuBar( CBaseToolSystem *pParent ) 
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
	CFoundryToolMenuButton *pToolButton = new CFoundryToolMenuButton( this, "Foundry", "F&oundry", GetActionTarget() );
	CFoundryViewMenuButton *pViewButton = new CFoundryViewMenuButton( this, "View", "&View", GetActionTarget() );
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
void CFoundryTool::UpdateMenuBar( )
{
	if ( !m_pDoc )
	{
		m_pMenuBar->SetFileName( "#FoundryNoFile" );
		return;
	}

	const char *pVMFFile = m_pDoc->GetVMFFileName();
	if ( !pVMFFile[0] )
	{
		m_pMenuBar->SetFileName( "#FoundryNoFile" );
		return;
	}

	if ( m_pDoc->IsDirty() )
	{
		char sz[ 512 ];
		Q_snprintf( sz, sizeof( sz ), "* %s", pVMFFile );
		m_pMenuBar->SetFileName( sz );
	}
	else
	{
		m_pMenuBar->SetFileName( pVMFFile );
	}
}


//-----------------------------------------------------------------------------
// Gets at tool windows
//-----------------------------------------------------------------------------
CBasePropertiesContainer *CFoundryTool::GetProperties()
{
	return m_hProperties.Get();
}

CEntityReportPanel *CFoundryTool::GetEntityReport()
{
	return m_hEntityReport.Get();
}


//-----------------------------------------------------------------------------
// Shows element properties
//-----------------------------------------------------------------------------
void CFoundryTool::ShowElementProperties( )
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
void CFoundryTool::ShowEntityInEntityProperties( CDmeVMFEntity *pEntity )
{
	Assert( m_hProperties.Get() );
	m_hCurrentEntity = pEntity;
	m_hProperties->SetObject( m_hCurrentEntity );
}

	
//-----------------------------------------------------------------------------
// Destroys all tool windows
//-----------------------------------------------------------------------------
void CFoundryTool::DestroyToolContainers()
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
void CFoundryTool::OnDefaultLayout()
{
	int y = m_pMenuBar->GetTall();

	int usew, useh;
	GetSize( usew, useh );

	DestroyToolContainers();

	Assert( ToolWindow::GetToolWindowCount() == 0 );

	CBasePropertiesContainer *properties = GetProperties();
	CEntityReportPanel *pEntityReport = GetEntityReport();

	// Need three containers
	ToolWindow *pPropertyWindow = m_ToolWindowFactory.InstanceToolWindow( GetClientArea(), false, properties, "#FoundryProperties", false );
	ToolWindow *pEntityReportWindow = m_ToolWindowFactory.InstanceToolWindow( GetClientArea(), false, pEntityReport, "#FoundryEntityReport", false );

	int halfScreen = usew / 2;
	int bottom = useh - y;
	int sy = (bottom - y) / 2;
	SetMiniViewportBounds( halfScreen, y, halfScreen, sy - y );
	pEntityReportWindow->SetBounds( 0, y, halfScreen, bottom );
	pPropertyWindow->SetBounds( halfScreen, sy, halfScreen, bottom - sy );
}

void CFoundryTool::OnToggleProperties()
{
	if ( m_hProperties.Get() )
	{ 
		ToggleToolWindow( m_hProperties.Get(), "#FoundryProperties" );
	}
}

void CFoundryTool::OnToggleEntityReport()
{
	if ( m_hEntityReport.Get() )
	{ 
		ToggleToolWindow( m_hEntityReport.Get(), "#FoundryEntityReport" );
	}
}



//-----------------------------------------------------------------------------
// Creates
//-----------------------------------------------------------------------------
void CFoundryTool::CreateTools( CFoundryDoc *doc )
{
	if ( !m_hProperties.Get() )
	{
		m_hProperties = new CBasePropertiesContainer( NULL, m_pDoc, g_pEditorTypeDict );
	}

	if ( !m_hEntityReport.Get() )
	{
		m_hEntityReport = new CEntityReportPanel( m_pDoc, this, "EntityReportPanel" );
	}

	RegisterToolWindow( m_hProperties );
	RegisterToolWindow( m_hEntityReport );
}


//-----------------------------------------------------------------------------
// Initializes the tools
//-----------------------------------------------------------------------------
void CFoundryTool::InitTools()
{
	ShowElementProperties();

	// FIXME: There are no tool windows here; how should this work?
	// These panels are saved
	windowposmgr->RegisterPanel( "properties", m_hProperties, false );
	windowposmgr->RegisterPanel( "entityreport", m_hEntityReport, false );

	if ( !windowposmgr->LoadPositions( "cfg/foundry.txt", this, &m_ToolWindowFactory, "Foundry" ) )
	{
		OnDefaultLayout();
	}
}


void CFoundryTool::DestroyTools()
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

	if ( m_hEntityReport.Get() )
	{
		windowposmgr->UnregisterPanel( m_hEntityReport.Get() );
		delete m_hEntityReport.Get();
		m_hEntityReport = NULL;
	}
}


void CFoundryTool::ShowToolWindow( Panel *tool, char const *toolName, bool visible )
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

void CFoundryTool::ToggleToolWindow( Panel *tool, char const *toolName )
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
vgui::Menu *CFoundryTool::CreateActionMenu( vgui::Panel *pParent )
{
	vgui::Menu *pActionMenu = new Menu( pParent, "ActionMenu" );
	pActionMenu->AddMenuItem( "#ToolHide", new KeyValues( "Command", "command", "HideActionMenu" ), GetActionTarget() );
	return pActionMenu;
}

//-----------------------------------------------------------------------------
// Inherited from IFileMenuCallbacks
//-----------------------------------------------------------------------------
int	CFoundryTool::GetFileMenuItemsEnabled( )
{
	int nFlags = FILE_ALL & (~FILE_NEW);
	if ( m_RecentFiles.IsEmpty() )
	{
		nFlags &= ~(FILE_RECENT | FILE_CLEAR_RECENT);
	}
	return nFlags;
}

void CFoundryTool::AddRecentFilesToMenu( vgui::Menu *pMenu )
{
	m_RecentFiles.AddToMenu( pMenu, GetActionTarget(), "OnRecent" );
}

bool CFoundryTool::GetPerforceFileName( char *pFileName, int nMaxLen )
{
	if ( !m_pDoc )
		return false;

	Q_strncpy( pFileName, m_pDoc->GetVMFFileName(), nMaxLen );
	return pFileName[0] != 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CFoundryTool::OnExit()
{
	windowposmgr->SavePositions( "cfg/foundry.txt", "Foundry" );

	enginetools->Command( "quit\n" );
}

//-----------------------------------------------------------------------------
// Handle commands from the action menu and other menus
//-----------------------------------------------------------------------------
void CFoundryTool::OnCommand( const char *cmd )
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
void CFoundryTool::PerformNew()
{
	// Can never do new
	Assert( 0 );
}

void CFoundryTool::OnNew()
{
	if ( m_pDoc )
	{
		if ( m_pDoc->IsDirty() )
		{
			SaveFile( m_pDoc->GetVMFFileName(), "vmf", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY,
				new KeyValues( "OnNew" ) );
			return;
		}
	}
	PerformNew();
}

void CFoundryTool::OnOpen( )
{
	int nFlags = 0;
	const char *pSaveFileName = NULL;
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		nFlags = FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY;
		pSaveFileName = m_pDoc->GetVMFFileName();
	}

	OpenFile( "bsp", pSaveFileName, "vmf", nFlags );
}

bool CFoundryTool::OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	OnCloseNoSave();
	if ( !LoadDocument( pFileName ) )
		return false;
	m_RecentFiles.Add( pFileName, pFileFormat );
	m_RecentFiles.SaveToRegistry( GetRegistryName() );
	UpdateMenuBar();
	return true;
}

void CFoundryTool::OnSave()
{
	if ( m_pDoc )
	{
		SaveFile( NULL, "vmf", FOSM_SHOW_PERFORCE_DIALOGS );
	}
}

void CFoundryTool::OnSaveAs()
{
	if ( m_pDoc )
	{
		SaveFile( m_pDoc->GetVMFFileName(), "vmf", FOSM_SHOW_PERFORCE_DIALOGS );
	}
}

bool CFoundryTool::OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	if ( !m_pDoc )
		return true;

	m_pDoc->SetVMFFileName( pFileName );
	m_pDoc->SaveToFile( );

	m_RecentFiles.Add( pFileName, pFileFormat );
	m_RecentFiles.SaveToRegistry( GetRegistryName() );
	UpdateMenuBar();
	return true;
}

void CFoundryTool::OnClose()
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		SaveFile( m_pDoc->GetVMFFileName(), "vmf", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY, 
			new KeyValues( "OnClose" ) );
		return;
	}

	OnCloseNoSave();
}

void CFoundryTool::OnCloseNoSave()
{
	// FIXME: Implement
}

void CFoundryTool::OnMarkNotDirty()
{
	// FIXME: Implement
}


//-----------------------------------------------------------------------------
// Open a specific file
//-----------------------------------------------------------------------------
void CFoundryTool::OpenSpecificFile( const char *pFileName )
{
	int nFlags = 0;
	const char *pSaveFileName = NULL;
	if ( m_pDoc )
	{
		// File is already open
		if ( !Q_stricmp( m_pDoc->GetVMFFileName(), pFileName ) )
			return;

		if ( m_pDoc->IsDirty() )
		{
			nFlags = FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY;
			pSaveFileName = m_pDoc->GetVMFFileName();
		}
		else
		{
			OnCloseNoSave();
		}
	}

	OpenFile( pFileName, "bsp", pSaveFileName, "vmf", nFlags );
}


//-----------------------------------------------------------------------------
// Show the save document query dialog
//-----------------------------------------------------------------------------
void CFoundryTool::OpenFileFromHistory( int slot )
{
	const char *pFileName = m_RecentFiles.GetFile( slot );
	if ( !pFileName )
		return;
	OpenSpecificFile( pFileName );
}


//-----------------------------------------------------------------------------
// Derived classes can implement this to get notified when files are saved/loaded
//-----------------------------------------------------------------------------
void CFoundryTool::OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues )
{
	if ( bWroteFile )
	{
		OnMarkNotDirty();
	}

	if ( !pContextKeyValues )
		return;

	if ( state != FileOpenStateMachine::SUCCESSFUL )
		return;

	if ( !Q_stricmp( pContextKeyValues->GetName(), "OnNew" ) )
	{
		PerformNew();
		return;
	}

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
}


//-----------------------------------------------------------------------------
// Show the File browser dialog
//-----------------------------------------------------------------------------
void CFoundryTool::SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	char pStartingDir[ MAX_PATH ];

	// We open BSPs, but save-as VMFs
	if ( bOpenFile )
	{
		GetModSubdirectory( "maps", pStartingDir, sizeof(pStartingDir) );
		pDialog->SetTitle( "Choose Valve BSP File", true );
		pDialog->SetStartDirectoryContext( "foundry_bsp_session", pStartingDir );
		pDialog->AddFilter( "*.bsp", "Valve BSP File (*.bsp)", true );
	}
	else
	{
		GetModContentSubdirectory( "maps", pStartingDir, sizeof(pStartingDir) );
		pDialog->SetTitle( "Choose Valve VMF File", true );
		pDialog->SetStartDirectoryContext( "foundry_vmf_session", pStartingDir );
		pDialog->AddFilter( "*.vmf", "Valve VMF File (*.vmf)", true );
	}
}


//-----------------------------------------------------------------------------
// Can we quit?
//-----------------------------------------------------------------------------
bool CFoundryTool::CanQuit()
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		// Show Save changes Yes/No/Cancel and re-quit if hit yes/no
		SaveFile( m_pDoc->GetVMFFileName(), "vmf", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY, 
			new KeyValues( "OnQuit" ) );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Various command handlers related to the Edit menu
//-----------------------------------------------------------------------------
void CFoundryTool::OnUndo()
{
	CDisableUndoScopeGuard guard;
	g_pDataModel->Undo();
}

void CFoundryTool::OnRedo()
{
	CDisableUndoScopeGuard guard;
	g_pDataModel->Redo();
}

void CFoundryTool::OnDescribeUndo()
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
// Foundry menu items
//-----------------------------------------------------------------------------
void CFoundryTool::OnReload()
{
	// Reloads the map, entities only, will reload every entity 
	enginetools->Command( "respawn_entities\n" );
}

void CFoundryTool::OnReloadFromSave()
{
	// Reloads the map from a save point, overrides selected entities
	// for now, this is hardcoded to be info_targets
	enginetools->Command( "load quick\n" );
}


//-----------------------------------------------------------------------------
// Background
//-----------------------------------------------------------------------------
const char *CFoundryTool::GetLogoTextureName()
{
	return "vgui/tools/sampletool/sampletool_logo";
}


//-----------------------------------------------------------------------------
// Inherited from IFoundryDocCallback
//-----------------------------------------------------------------------------
void CFoundryTool::OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
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
bool CFoundryTool::LoadDocument( const char *pDocName )
{
	Assert( !m_pDoc );

	DestroyTools();

	m_pDoc = new CFoundryDoc( this );
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
// Create the entities that are in our VMF file
//-----------------------------------------------------------------------------
const char* CFoundryTool::GetEntityData( const char *pActualEntityData )
{
	if ( !m_pDoc )
		return pActualEntityData;

	return m_pDoc->GenerateEntityData( pActualEntityData );
}
