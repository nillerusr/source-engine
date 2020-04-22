//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Act busy tool; main UI smarts class
//
//=============================================================================

#include "toolutils/basetoolsystem.h"
#include "toolutils/recentfilelist.h"
#include "toolutils/toolmenubar.h"
#include "toolutils/toolswitchmenubutton.h"
#include "toolutils/toolfilemenubutton.h"
#include "toolutils/tooleditmenubutton.h"
#include "toolutils/toolmenubutton.h"
#include "vgui_controls/Menu.h"
#include "tier1/KeyValues.h"
#include "toolutils/enginetools_int.h"
#include "toolframework/ienginetool.h"
#include "vgui/IInput.h"
#include "vgui/KeyCode.h"
#include "vgui_controls/FileOpenDialog.h"
#include "filesystem.h"
#include "actbusydoc.h"
#include "vgui/ilocalize.h"
#include "dme_controls/elementpropertiestree.h"
#include "actbusytool.h"
#include "movieobjects/dmeeditortypedictionary.h"
#include "dme_controls/attributestringchoicepanel.h"
#include "matsys_controls/mdlsequencepicker.h"
#include "istudiorender.h"
#include "materialsystem/imaterialsystem.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "toolutils/toolwindowfactory.h"
#include "toolutils/basepropertiescontainer.h"
#include "toolutils/savewindowpositions.h"
#include "tier2/fileutils.h"
#include "tier3/tier3.h"
#include "vgui/ivgui.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Singleton interfaces
//-----------------------------------------------------------------------------
CDmeEditorTypeDictionary *g_pEditorTypeDict;


//-----------------------------------------------------------------------------
// Methods needed by scenedatabase. They have to live here instead of toolutils
// because this is a DLL but toolutils is only a static library
//-----------------------------------------------------------------------------
const char *GetVGuiControlsModuleName()
{
	return "ActBusyTool";
}

//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool ConnectTools( CreateInterfaceFn factory )
{
	return (g_pMDLCache != NULL) && (studiorender != NULL) && (materials != NULL) && (g_pMatSystemSurface != NULL);
}

void DisconnectTools( )
{
}


//-----------------------------------------------------------------------------
// Implementation of the act busy tool
//-----------------------------------------------------------------------------
class CActBusyTool : public CBaseToolSystem, public IFileMenuCallbacks, public IActBusyDocCallback
{
	DECLARE_CLASS_SIMPLE( CActBusyTool, CBaseToolSystem );

public:
	CActBusyTool();

	// Inherited from IToolSystem
	virtual const char *GetToolName() { return "ActBusy Script Editor"; }
	virtual const char *GetBindingsContextFile() { return "cfg/ActBusy.kb"; }
	virtual bool	Init();
    virtual void	Shutdown();
	virtual bool	CanQuit();

	// Inherited from IFileMenuCallbacks
	virtual int		GetFileMenuItemsEnabled( );
	virtual void	AddRecentFilesToMenu( vgui::Menu *menu );
	virtual bool	GetPerforceFileName( char *pFileName, int nMaxLen );
	virtual vgui::Panel* GetRootPanel() { return this; }

	// Inherited from IActBusyDocCallback
	virtual void	OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags );

	// Inherited from CBaseToolSystem
	virtual vgui::HScheme GetToolScheme();
	virtual vgui::Menu *CreateActionMenu( vgui::Panel *pParent );
	virtual void OnCommand( const char *cmd );
	virtual const char *GetRegistryName() { return "ActBusy"; }
	virtual vgui::MenuBar *CreateMenuBar( CBaseToolSystem *pParent );
	virtual void OnToolActivate();
	virtual void OnToolDeactivate();
	virtual CActBusyDoc *GetDocument();
	virtual CBasePropertiesContainer	*GetProperties();
	virtual CMDLSequencePicker			*GetSequencePicker();
	virtual void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual void OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues );

public:
	// Commands related to the file menu
	MESSAGE_FUNC( OnNew, "OnNew" );
	MESSAGE_FUNC( OnOpen, "OnOpen" );
	MESSAGE_FUNC( OnSave, "OnSave" );
	MESSAGE_FUNC( OnSaveAs, "OnSaveAs" );
	MESSAGE_FUNC( OnClose, "OnClose" );
	MESSAGE_FUNC( OnCloseNoSave, "OnCloseNoSave" );
	MESSAGE_FUNC( OnMarkNotDirty, "OnMarkNotDirty" );
	MESSAGE_FUNC( OnExit, "OnExit" );
	void		PerformNew();
	void		OpenFileFromHistory( int slot, const char *pCommand );
	void		OpenSpecificFile( const char *pFileName );

	// Commands related to the edit menu
	KEYBINDING_FUNC( undo, KEY_Z, vgui::MODIFIER_CONTROL, OnUndo, "#undo_help", 0 );
	KEYBINDING_FUNC( redo, KEY_Z, vgui::MODIFIER_CONTROL | vgui::MODIFIER_SHIFT, OnRedo, "#redo_help", 0 );
	void		OnDescribeUndo();

	// Commands related to the actbusy menu
	void		OnNewActBusy();
	void		OnDeleteActBusy();

private:
	// Flags for HideStandardFields
	enum EditorTypeStandardFields_t
	{
		EDITOR_FIELD_NAME		= 0x1,
		EDITOR_FIELD_TYPE		= 0x2,
		EDITOR_FIELD_ID			= 0x4,
		EDITOR_FIELD_EDITORTYPE = 0x8,
	};

	void HideStandardFields( CDmeEditorType *pEditorType, int nFieldFlags );

	// Creates a new document
	void NewDocument( );

	// Loads up a new document
	bool LoadDocument( const char *pDocName );

	// Updates the menu bar based on the current file
	void UpdateMenuBar( );

	// Shows element properties
	void ShowElementProperties( );

	// Create custom editors
	void InitEditorDict();

	void	CreateTools( CActBusyDoc *doc );
	void	InitTools();
	void	DestroyTools();

	void	ToggleToolWindow( Panel *tool, const char *toolName );
	void	ShowToolWindow( Panel *tool, const char *toolName, bool visible );

	void	OnToggleProperties();
	void    OnToggleSequencePicker();
	void	OnDefaultLayout();

	void	DestroyToolContainers();

	virtual const char *GetLogoTextureName();

private:	
	// All editable data
	CActBusyDoc		*m_pDoc;

	// The menu bar
	CToolFileMenuBar *m_pMenuBar;

	// Element properties for editing actbusy
	vgui::DHANDLE< CBasePropertiesContainer >	m_hProperties;
	// The sequence picker!
	vgui::DHANDLE< CMDLSequencePicker > m_hMDLSequencePicker;

	CToolWindowFactory< ToolWindow > m_ToolWindowFactory;

	CUtlVector< DmElementHandle_t > m_toolElements;
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CActBusyTool	*g_pActBusyTool = NULL;

void CreateTools()
{
	g_pActBusyTool = new CActBusyTool();
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CActBusyTool::CActBusyTool()
{
	m_pMenuBar = NULL;
	m_pDoc = NULL;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CActBusyTool::Init( )
{
	m_pDoc = NULL;
	m_RecentFiles.LoadFromRegistry( GetRegistryName() );

	// NOTE: This has to happen before BaseClass::Init
	g_pVGuiLocalize->AddFile( "resource/toolactbusy_%language%.txt" );

	if ( !BaseClass::Init( ) )
		return false;

	g_pDataModel->SetUndoDepth( 256 );

	InitEditorDict();
	return true;
}

void CActBusyTool::Shutdown()
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
// Tool activation/deactivation
//-----------------------------------------------------------------------------
void CActBusyTool::OnToolActivate()
{
	BaseClass::OnToolActivate();
}

void CActBusyTool::OnToolDeactivate()
{
	BaseClass::OnToolDeactivate();
}


//-----------------------------------------------------------------------------
// Hides standard fields
//-----------------------------------------------------------------------------
void CActBusyTool::HideStandardFields( CDmeEditorType *pEditorType, int nFieldFlags )
{
	CDmeEditorAttributeInfo *pInfo;

	if ( nFieldFlags & EDITOR_FIELD_NAME )
	{
		pInfo = CreateElement< CDmeEditorAttributeInfo >( "name info", DMFILEID_INVALID );
		pInfo->m_bIsVisible = false;
		pEditorType->AddAttributeInfo( "name", pInfo ); 
		m_toolElements.AddToTail( pInfo->GetHandle() );
	}

	if ( nFieldFlags & EDITOR_FIELD_TYPE )
	{
		pInfo = CreateElement< CDmeEditorAttributeInfo >( "type info", DMFILEID_INVALID );
		pInfo->m_bIsVisible = false;
		pEditorType->AddAttributeInfo( "type", pInfo ); 
		m_toolElements.AddToTail( pInfo->GetHandle() );
	}

	if ( nFieldFlags & EDITOR_FIELD_ID )
	{
		pInfo = CreateElement< CDmeEditorAttributeInfo >( "id info", DMFILEID_INVALID );
		pInfo->m_bIsVisible = false;
		pEditorType->AddAttributeInfo( "id", pInfo ); 
		m_toolElements.AddToTail( pInfo->GetHandle() );
	}

	if ( nFieldFlags & EDITOR_FIELD_EDITORTYPE )
	{
		pInfo = CreateElement< CDmeEditorAttributeInfo >( "editor type info", DMFILEID_INVALID );
		pInfo->m_bIsVisible = false;
		pEditorType->AddAttributeInfo( "editorType", pInfo ); 
		m_toolElements.AddToTail( pInfo->GetHandle() );
	}
}


//-----------------------------------------------------------------------------
// Create custom editors
//-----------------------------------------------------------------------------
void CActBusyTool::InitEditorDict()
{
	CDmeEditorAttributeInfo *pInfo, *pArrayInfo;

	// FIXME: This eventually will move to an .fgd-like file.
	g_pEditorTypeDict = CreateElement< CDmeEditorTypeDictionary >( "DmeEditorTypeDictionary", DMFILEID_INVALID );
	m_toolElements.AddToTail( g_pEditorTypeDict->GetHandle() );

	CDmeEditorType *pActBusyList = CreateElement< CDmeEditorType >( "actBusyList", DMFILEID_INVALID );
	HideStandardFields( pActBusyList, EDITOR_FIELD_NAME | EDITOR_FIELD_TYPE | EDITOR_FIELD_ID | EDITOR_FIELD_EDITORTYPE );
	g_pEditorTypeDict->AddEditorType( pActBusyList );
	m_toolElements.AddToTail( pActBusyList->GetHandle() );

	pInfo = CreateElement< CDmeEditorAttributeInfo >( "children info", DMFILEID_INVALID );
	pActBusyList->AddAttributeInfo( "children", pInfo ); 
	m_toolElements.AddToTail( pInfo->GetHandle() );
	pArrayInfo = CreateElement< CDmeEditorAttributeInfo >( "hide text info", DMFILEID_INVALID );
	pInfo->SetArrayInfo( pArrayInfo );
 	pArrayInfo->SetValue( "hideText", true );
	m_toolElements.AddToTail( pArrayInfo->GetHandle() );

	CDmeEditorType *pActBusyType = CreateElement< CDmeEditorType >( "actBusy", DMFILEID_INVALID );
	HideStandardFields( pActBusyType, EDITOR_FIELD_TYPE | EDITOR_FIELD_ID | EDITOR_FIELD_EDITORTYPE );
	m_toolElements.AddToTail( pActBusyType->GetHandle() );

	// anims only accept activity names
	pInfo = CreateElement< CDmeEditorAttributeInfo >( "busy anim info", DMFILEID_INVALID );
	pActBusyType->AddAttributeInfo( "busy_anim", pInfo ); 
	pInfo->m_Widget = "sequencepicker";
	pInfo->SetValue( "texttype", "activityName" );
	m_toolElements.AddToTail( pInfo->GetHandle() );

	pInfo = CreateElement< CDmeEditorAttributeInfo >( "entry anim info", DMFILEID_INVALID );
	pActBusyType->AddAttributeInfo( "entry_anim", pInfo ); 
	pInfo->m_Widget = "sequencepicker";
	pInfo->SetValue( "texttype", "activityName" );
	m_toolElements.AddToTail( pInfo->GetHandle() );

	pInfo = CreateElement< CDmeEditorAttributeInfo >( "exit anim info", DMFILEID_INVALID );
	pActBusyType->AddAttributeInfo( "exit_anim", pInfo ); 
	pInfo->m_Widget = "sequencepicker";
	pInfo->SetValue( "texttype", "activityName" );
	m_toolElements.AddToTail( pInfo->GetHandle() );

	// sequences only accept sequence names
	pInfo = CreateElement< CDmeEditorAttributeInfo >( "busy sequence info", DMFILEID_INVALID );
	pActBusyType->AddAttributeInfo( "busy_sequence", pInfo ); 
	pInfo->m_Widget = "sequencepicker";
	pInfo->SetValue( "texttype", "sequenceName" );
	m_toolElements.AddToTail( pInfo->GetHandle() );

	pInfo = CreateElement< CDmeEditorAttributeInfo >( "entry sequence info", DMFILEID_INVALID );
	pActBusyType->AddAttributeInfo( "entry_sequence", pInfo ); 
	pInfo->m_Widget = "sequencepicker";
	pInfo->SetValue( "texttype", "sequenceName" );
	m_toolElements.AddToTail( pInfo->GetHandle() );

	pInfo = CreateElement< CDmeEditorAttributeInfo >( "exit sequence info", DMFILEID_INVALID );
	pActBusyType->AddAttributeInfo( "exit_sequence", pInfo ); 
	pInfo->m_Widget = "sequencepicker";
	pInfo->SetValue( "texttype", "sequenceName" );
	m_toolElements.AddToTail( pInfo->GetHandle() );

	CDmeEditorStringChoicesInfo *pChoicesInfo = CreateElement< CDmeEditorStringChoicesInfo >( "interrupts info", DMFILEID_INVALID );
	pActBusyType->AddAttributeInfo( "interrupts", pChoicesInfo ); 
	pChoicesInfo->m_Widget = "stringchoice";
	m_toolElements.AddToTail( pChoicesInfo->AddChoice( "BA_INT_NONE", "No Interrupts" )->GetHandle() );
	m_toolElements.AddToTail( pChoicesInfo->AddChoice( "BA_INT_DANGER", "Danger" )->GetHandle() );
	m_toolElements.AddToTail( pChoicesInfo->AddChoice( "BA_INT_PLAYER", "Player" )->GetHandle() );
	m_toolElements.AddToTail( pChoicesInfo->AddChoice( "BA_INT_AMBUSH", "Ambush" )->GetHandle() );
	m_toolElements.AddToTail( pChoicesInfo->AddChoice( "BA_INT_COMBAT", "Combat" )->GetHandle() );
	m_toolElements.AddToTail( pChoicesInfo->AddChoice( "BA_INT_ZOMBIESLUMP", "Zombie Slump" )->GetHandle() );
	m_toolElements.AddToTail( pChoicesInfo->GetHandle() );

	g_pEditorTypeDict->AddEditorType( pActBusyType );
}

//-----------------------------------------------------------------------------
//
// The View menu
//
//-----------------------------------------------------------------------------
class CActBusyViewMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CActBusyViewMenuButton, CToolMenuButton );
public:
	CActBusyViewMenuButton( CActBusyTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget );
	virtual void OnShowMenu(vgui::Menu *menu);

private:
	CActBusyTool *m_pTool;
};

CActBusyViewMenuButton::CActBusyViewMenuButton( CActBusyTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget )
	: BaseClass( parent, panelName, text, pActionSignalTarget )
{
	m_pTool = parent;

	AddCheckableMenuItem( "properties", "#ActBusyProperties", new KeyValues( "Command", "command", "OnToggleProperties" ), pActionSignalTarget );
	AddCheckableMenuItem( "picker", "#ActBusyViewSequencePicker", new KeyValues( "Command", "command", "OnToggleSequencePicker" ), pActionSignalTarget );

	AddSeparator();
	AddMenuItem( "defaultlayout", "#ActBusyViewDefault", new KeyValues ( "Command", "command", "OnDefaultLayout"), pActionSignalTarget );

	SetMenu(m_pMenu);
}

void CActBusyViewMenuButton::OnShowMenu(vgui::Menu *menu)
{
	BaseClass::OnShowMenu( menu );

	// Update the menu
	int id;

	CActBusyDoc *pDoc = m_pTool->GetDocument();
	if ( pDoc )
	{
		id = m_Items.Find( "properties" );
		m_pMenu->SetItemEnabled( id, true );

		Panel *p;
		p = m_pTool->GetProperties();
		Assert( p );
		m_pMenu->SetMenuItemChecked( id, ( p && p->GetParent() ) ? true : false );

		id = m_Items.Find( "picker" );
		m_pMenu->SetItemEnabled( id, true );
		
		p = m_pTool->GetSequencePicker();
		Assert( p );
		m_pMenu->SetMenuItemChecked( id, ( p && p->GetParent() ) ? true : false );

	}
	else
	{
		id = m_Items.Find( "properties" );
		m_pMenu->SetItemEnabled( id, false );
		id = m_Items.Find( "picker" );
		m_pMenu->SetItemEnabled( id, false );
	}
}



//-----------------------------------------------------------------------------
//
// The ActBusy menu
//
//-----------------------------------------------------------------------------
class CActBusyMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CActBusyMenuButton, CToolMenuButton );
public:
	CActBusyMenuButton( CActBusyTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget );
	virtual void OnShowMenu(vgui::Menu *menu);

private:
	CActBusyTool *m_pTool;
};

CActBusyMenuButton::CActBusyMenuButton( CActBusyTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget )
	: BaseClass( parent, panelName, text, pActionSignalTarget )
{
	m_pTool = parent;

	AddMenuItem( "NewActBusy", "#ActBusyNewActBusy", new KeyValues( "Command", "command", "OnNewActBusy" ), pActionSignalTarget );
	AddMenuItem( "DeleteActBusy", "#ActBusyDeleteActBusy", new KeyValues( "Command", "command", "OnDeleteActBusy" ), pActionSignalTarget );

	SetMenu(m_pMenu);
}

void CActBusyMenuButton::OnShowMenu(vgui::Menu *menu)
{
	BaseClass::OnShowMenu( menu );

	// Update the menu
	int id;

	CActBusyDoc *pDoc = m_pTool->GetDocument();
	if ( pDoc )
	{
		id = m_Items.Find( "NewActBusy" );
		m_pMenu->SetItemEnabled( id, true );
		id = m_Items.Find( "DeleteActBusy" );
		m_pMenu->SetItemEnabled( id, true );
	}
	else
	{
		id = m_Items.Find( "NewActBusy" );
		m_pMenu->SetItemEnabled( id, false );
		id = m_Items.Find( "DeleteActBusy" );
		m_pMenu->SetItemEnabled( id, false );
	}
}


//-----------------------------------------------------------------------------
// Initializes the menu bar
//-----------------------------------------------------------------------------
vgui::MenuBar *CActBusyTool::CreateMenuBar( CBaseToolSystem *pParent ) 
{
	m_pMenuBar = new CToolFileMenuBar( pParent, "ActBusyMenuBar" );

	// Sets info in the menu bar
	char title[ 64 ];
	ComputeMenuBarTitle( title, sizeof( title ) );
    m_pMenuBar->SetInfo( title );
	m_pMenuBar->SetToolName( GetToolName() );
	UpdateMenuBar();

	// Add menu buttons
	CToolMenuButton *pFileButton = CreateToolFileMenuButton( m_pMenuBar, "File", "&File", GetActionTarget(), this );
	CToolMenuButton *pEditButton = CreateToolEditMenuButton( this, "Edit", "&Edit", GetActionTarget() );
	CActBusyMenuButton *pActBusyButton = new CActBusyMenuButton( this, "ActBusy", "&ActBusy", GetActionTarget() );
	CToolMenuButton *pSwitchButton = CreateToolSwitchMenuButton( m_pMenuBar, "Switcher", "&Tools", GetActionTarget() );
	CActBusyViewMenuButton *pViewButton = new CActBusyViewMenuButton( this, "View", "&View", GetActionTarget() );

	m_pMenuBar->AddButton( pFileButton );
	m_pMenuBar->AddButton( pEditButton );
	m_pMenuBar->AddButton( pActBusyButton );
	m_pMenuBar->AddButton( pViewButton );
	m_pMenuBar->AddButton( pSwitchButton );

	return m_pMenuBar;
}


//-----------------------------------------------------------------------------
// Updates the menu bar based on the current file
//-----------------------------------------------------------------------------
void CActBusyTool::UpdateMenuBar( )
{
	if ( !m_pDoc )
	{
		m_pMenuBar->SetFileName( "#ActBusyNoFile" );
		return;
	}

	if ( m_pDoc->IsDirty() )
	{
		char sz[ 512 ];
		Q_snprintf( sz, sizeof( sz ), "* %s", m_pDoc->GetFileName() );
		m_pMenuBar->SetFileName( sz );
	}
	else
	{
		m_pMenuBar->SetFileName( m_pDoc->GetFileName() );
	}
}


//-----------------------------------------------------------------------------
// Inherited from IFileMenuCallbacks
//-----------------------------------------------------------------------------
int	 CActBusyTool::GetFileMenuItemsEnabled( )
{
	int nFlags;
	if ( !m_pDoc )
	{
		nFlags = FILE_NEW | FILE_OPEN | FILE_RECENT | FILE_CLEAR_RECENT | FILE_EXIT;
	}
	else
	{
		nFlags = FILE_ALL;
	}

	if ( m_RecentFiles.IsEmpty() )
	{
		nFlags &= ~(FILE_RECENT | FILE_CLEAR_RECENT);
	}
	return nFlags;
}

void CActBusyTool::AddRecentFilesToMenu( vgui::Menu *pMenu )
{
	m_RecentFiles.AddToMenu( pMenu, GetActionTarget(), "OnRecent" );
}

	
//-----------------------------------------------------------------------------
// Returns the file name for perforce
//-----------------------------------------------------------------------------
bool CActBusyTool::GetPerforceFileName( char *pFileName, int nMaxLen ) 
{ 
	if ( !m_pDoc )
		return false;
    Q_strncpy( pFileName, m_pDoc->GetFileName(), nMaxLen );
	return true;
}


//-----------------------------------------------------------------------------
// Derived classes can implement this to get a new scheme to be applied to this tool
//-----------------------------------------------------------------------------
vgui::HScheme CActBusyTool::GetToolScheme() 
{ 
	return vgui::scheme()->LoadSchemeFromFile( "Resource/BoxRocket.res", "BoxRocket" );
}


//-----------------------------------------------------------------------------
// Creates the action menu
//-----------------------------------------------------------------------------
vgui::Menu *CActBusyTool::CreateActionMenu( vgui::Panel *pParent )
{
	vgui::Menu *pActionMenu = new Menu( pParent, "ActionMenu" );
	pActionMenu->AddMenuItem( "#ToolHide", new KeyValues( "Command", "command", "HideActionMenu" ), GetActionTarget() );
	return pActionMenu;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CActBusyTool::OnExit()
{
	windowposmgr->SavePositions( "cfg/actbusy.txt", "ActBusy" );

	// Throw up a "save" dialog?
	enginetools->Command( "quit\n" );
}

//-----------------------------------------------------------------------------
// Handle commands from the action menu and other menus
//-----------------------------------------------------------------------------
void CActBusyTool::OnCommand( const char *cmd )
{
	if ( !V_stricmp( cmd, "HideActionMenu" ) )
	{
		if ( GetActionMenu() )
		{
			GetActionMenu()->SetVisible( false );
		}
	}
	else if ( !V_stricmp( cmd, "OnNewActBusy" ) )
	{
		OnNewActBusy();
	}
	else if ( !V_stricmp( cmd, "OnDeleteActBusy" ) )
	{
		OnDeleteActBusy();
	}
	else if ( !V_stricmp( cmd, "OnToggleProperties" ) )
	{
		OnToggleProperties();
	}
	else if ( !V_stricmp( cmd, "OnToggleSequencePicker" ) )
	{
		OnToggleSequencePicker();
	}
	else if ( !V_stricmp( cmd, "OnDefaultLayout" ) )
	{
		OnDefaultLayout();
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
	else if ( const char *pSuffix = StringAfterPrefix( cmd, "OnRecent" ) )
	{
		int idx = Q_atoi( pSuffix );
		g_pActBusyTool->OpenFileFromHistory( idx, cmd );
	}
	else if( const char *pSuffixTool = StringAfterPrefix( cmd, "OnTool" ) )
	{
		int idx = Q_atoi( pSuffixTool );
		enginetools->SwitchToTool( idx );
	}
	else
	{
		BaseClass::OnCommand( cmd );
	}
}



//-----------------------------------------------------------------------------
// Derived classes can implement this to get notified when files are saved/loaded
//-----------------------------------------------------------------------------
void CActBusyTool::OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues )
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
// Called by SaveFile to allow clients to set up the save dialog
//-----------------------------------------------------------------------------
void CActBusyTool::SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	// Compute starting directory
	char pStartingDir[ MAX_PATH ];
	GetModSubdirectory( "scripts", pStartingDir, sizeof(pStartingDir) );

	if ( bOpenFile )
	{
		pDialog->SetTitle( "Open ActBusy .TXT File", true );
	}
	else
	{
		pDialog->SetTitle( "Save ActBusy .TXT File As", true );
	}

	pDialog->SetStartDirectoryContext( "actbusy_session", pStartingDir );
	pDialog->AddFilter( "*.txt", "ActBusy .TXT (*.txt)", true );
}


//-----------------------------------------------------------------------------
// Called by SaveFile to allow clients to actually write the file out
//-----------------------------------------------------------------------------
bool CActBusyTool::OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	OnCloseNoSave();
	return LoadDocument( pFileName );
}

	
//-----------------------------------------------------------------------------
// Called by SaveFile to allow clients to actually write the file out
//-----------------------------------------------------------------------------
bool CActBusyTool::OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	if ( !m_pDoc )
		return true; 

	m_pDoc->SetFileName( pFileName );
	m_pDoc->SaveToFile( );
	UpdateMenuBar();
	return true;
}


//-----------------------------------------------------------------------------
// Command handlers
//-----------------------------------------------------------------------------
void CActBusyTool::PerformNew()
{
	OnCloseNoSave();
	NewDocument();
}

void CActBusyTool::OnNew()
{
	if ( m_pDoc )
	{
		if ( m_pDoc->IsDirty() )
		{
			SaveFile( m_pDoc->GetFileName(), "actbusy", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY,
				new KeyValues( "OnNew" ) );
			return;
		}
	}

	PerformNew();
}

void CActBusyTool::OnOpen()
{
	int nFlags = 0;
	const char *pSaveFileName = NULL;
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		nFlags = FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY;
		pSaveFileName = m_pDoc->GetFileName();
	}

	OpenFile( "actbusy", pSaveFileName, "actbusy", nFlags );
}

void CActBusyTool::OnSave()
{
	if ( m_pDoc )
	{
		SaveFile( m_pDoc->GetFileName(), "actbusy", FOSM_SHOW_PERFORCE_DIALOGS );
	}
}

void CActBusyTool::OnSaveAs()
{
	if ( m_pDoc )
	{
		SaveFile( NULL, "actbusy", FOSM_SHOW_PERFORCE_DIALOGS );
	}
}

void CActBusyTool::OnClose()
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		SaveFile( m_pDoc->GetFileName(), "actbusy", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY, 
			new KeyValues( "OnClose" ) );
		return;
	}

	OnCloseNoSave();
}

void CActBusyTool::OnCloseNoSave()
{
	DestroyTools();

	if ( m_pDoc )
	{
		CAppNotifyScopeGuard( "CActBusyTool::OnCloseNoSave", 0 );

		delete m_pDoc;
		m_pDoc = NULL;

		if ( m_hProperties )
		{
			m_hProperties->SetObject( NULL );
		}
	}
	
	UpdateMenuBar( );
}

void CActBusyTool::OnMarkNotDirty()
{
	if ( m_pDoc )
	{
		m_pDoc->SetDirty( false );
	}
}


//-----------------------------------------------------------------------------
// Open a specific file
//-----------------------------------------------------------------------------
void CActBusyTool::OpenSpecificFile( const char *pFileName )
{
	int nFlags = 0;
	const char *pSaveFileName = NULL;
	if ( m_pDoc )
	{
		// File is already open
		if ( !Q_stricmp( m_pDoc->GetFileName(), pFileName ) )
			return;

		if ( m_pDoc->IsDirty() )
		{
			nFlags = FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY;
			pSaveFileName = m_pDoc->GetFileName();
		}
		else
		{
			OnCloseNoSave();
		}
	}

	OpenFile( pFileName, "actbusy", pSaveFileName, "actbusy", nFlags );
}


//-----------------------------------------------------------------------------
// Show the save document query dialog
//-----------------------------------------------------------------------------
void CActBusyTool::OpenFileFromHistory( int slot, const char *pCommand )
{
	const char *pFileName = m_RecentFiles.GetFile( slot );
	if ( pFileName )
	{
		OpenSpecificFile( pFileName );
	}
}

bool CActBusyTool::CanQuit()
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		// Show Save changes Yes/No/Cancel and re-quit if hit yes/no
		SaveFile( m_pDoc->GetFileName(), "actbusy", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY, 
			new KeyValues( "OnQuit" ) );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Various command handlers related to the Edit menu
//-----------------------------------------------------------------------------
void CActBusyTool::OnUndo()
{
	CDisableUndoScopeGuard guard;
	g_pDataModel->Undo();
}

void CActBusyTool::OnRedo()
{
	CDisableUndoScopeGuard guard;
	g_pDataModel->Redo();
}

void CActBusyTool::OnDescribeUndo()
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
// Commands related to the actbusy menu
//-----------------------------------------------------------------------------
void CActBusyTool::OnNewActBusy()
{
	if ( m_pDoc )
	{
		m_pDoc->CreateActBusy();
	}
}

void CActBusyTool::OnDeleteActBusy()
{
}

	
//-----------------------------------------------------------------------------
// Inherited from IActBusyDocCallback
//-----------------------------------------------------------------------------
void CActBusyTool::OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	UpdateMenuBar();
	if ( ( nNotifySource != NOTIFY_SOURCE_PROPERTIES_TREE ) && m_hProperties.Get() )
	{
		m_hProperties->Refresh();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : CActBusyDoc
//-----------------------------------------------------------------------------
CActBusyDoc *CActBusyTool::GetDocument()
{
	return m_pDoc;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : virtual CBasePropertiesContainer
//-----------------------------------------------------------------------------
CBasePropertiesContainer *CActBusyTool::GetProperties()
{
	return m_hProperties.Get();
}

CMDLSequencePicker *CActBusyTool::GetSequencePicker()
{
	return m_hMDLSequencePicker.Get();
}


//-----------------------------------------------------------------------------
// Initializes the tools
//-----------------------------------------------------------------------------
void CActBusyTool::InitTools()
{
	ShowElementProperties();

	// FIXME: There are no tool windows here; how should this work?
	// These panels are saved
	windowposmgr->RegisterPanel( "picker", m_hMDLSequencePicker, false );
	windowposmgr->RegisterPanel( "properties", m_hProperties, false );

	if ( !windowposmgr->LoadPositions( "cfg/actbusy.txt", this, &m_ToolWindowFactory, "ActBusy" ) )
	{
		OnDefaultLayout();
	}
}


//-----------------------------------------------------------------------------
// Loads up a new document
//-----------------------------------------------------------------------------
bool CActBusyTool::LoadDocument( const char *pDocName )
{
	Assert( !m_pDoc );

	DestroyTools();

	m_pDoc = new CActBusyDoc( this );
	if ( !m_pDoc->LoadFromFile( pDocName ) )
	{
		delete m_pDoc;
		m_pDoc = NULL;
		Warning( "Fatal error loading '%s'\n", pDocName );
		return false;
	}

//	ShowMiniViewport( false );

	CreateTools( m_pDoc );

	m_RecentFiles.Add( pDocName, "actbusy" );
	UpdateMenuBar( );

	InitTools();
	return true;
}


//-----------------------------------------------------------------------------
// Loads up a new document
//-----------------------------------------------------------------------------
void CActBusyTool::NewDocument( )
{
	Assert( !m_pDoc );

	m_pDoc = new CActBusyDoc( this );
	m_pDoc->CreateNew( );

//	ShowMiniViewport( false );

	CreateTools( m_pDoc );
	UpdateMenuBar( );
	InitTools();
}


//-----------------------------------------------------------------------------
// Shows element properties
//-----------------------------------------------------------------------------
void CActBusyTool::ShowElementProperties( )
{
	if ( !m_pDoc )
		return;

	if ( !m_pDoc->GetRootObject() )
		return;

	// It should already exist
	Assert( m_hProperties.Get() );
	if ( m_hProperties.Get() )
	{
		m_hProperties->SetObject( m_pDoc->GetRootObject() );
	}
}

void CActBusyTool::DestroyTools()
{
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
	if ( m_hMDLSequencePicker.Get() )
	{
		windowposmgr->UnregisterPanel( m_hMDLSequencePicker.Get() );
		delete m_hMDLSequencePicker.Get();
		m_hMDLSequencePicker = NULL;
	}
}

void CActBusyTool::CreateTools( CActBusyDoc *doc )
{
	if ( !m_hProperties.Get() )
	{
		m_hProperties = new CBasePropertiesContainer( NULL, m_pDoc, g_pEditorTypeDict );
	}
	if ( !m_hMDLSequencePicker.Get() )
	{
		m_hMDLSequencePicker = new CMDLSequencePicker( NULL );
		SETUP_PANEL( m_hMDLSequencePicker.Get() );
		m_hMDLSequencePicker->Activate();
	}

	RegisterToolWindow( m_hProperties );
	RegisterToolWindow( m_hMDLSequencePicker );
}

void CActBusyTool::ShowToolWindow( Panel *tool, const char *toolName, bool visible )
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

void CActBusyTool::ToggleToolWindow( Panel *tool, const char *toolName )
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

void CActBusyTool::DestroyToolContainers()
{
	int c = ToolWindow::GetToolWindowCount();
	for ( int i = c - 1; i >= 0 ; --i )
	{
		ToolWindow *kill = ToolWindow::GetToolWindow( i );
		delete kill;
	}
}

void CActBusyTool::OnDefaultLayout()
{
	int y = m_pMenuBar->GetTall();

	int usew, useh;
	GetSize( usew, useh );

	DestroyToolContainers();

	Assert( ToolWindow::GetToolWindowCount() == 0 );

	CBasePropertiesContainer *properties = GetProperties();
	CMDLSequencePicker *picker = GetSequencePicker();

	// Need three containers
	ToolWindow *propertyWindow = m_ToolWindowFactory.InstanceToolWindow( GetClientArea(), false, properties, "#ActBusyProperties", false );
	ToolWindow *pickerWindow = m_ToolWindowFactory.InstanceToolWindow( GetClientArea(), false, picker, "#ActBusyViewSequencePicker", false );

	int halfScreen = usew / 2;
	int bottom = useh - y;

	propertyWindow->SetBounds( 0, y, halfScreen, bottom );
	pickerWindow->SetBounds( halfScreen, y, halfScreen, bottom );
}

void CActBusyTool::OnToggleProperties()
{
	ToggleToolWindow( m_hProperties.Get(), "#ActBusyProperties" );
}

void CActBusyTool::OnToggleSequencePicker()
{
	ToggleToolWindow( m_hMDLSequencePicker.Get(), "#ActBusyViewSequencePicker" );
}

const char *CActBusyTool::GetLogoTextureName()
{
	return "vgui/tools/actbusy/actbusy_logo";
}