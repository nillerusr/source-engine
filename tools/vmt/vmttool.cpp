//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Act busy tool; main UI smarts class
//
//=============================================================================

#include "toolutils/basetoolsystem.h"
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
#include "vmtdoc.h"
#include "vgui/ilocalize.h"
#include "dme_controls/elementpropertiestree.h"
#include "matsys_controls/vmtpanel.h"
#include "vmttool.h"
#include "movieobjects/dmeeditortypedictionary.h"
#include "dme_controls/attributestringchoicepanel.h"
#include "matsys_controls/mdlsequencepicker.h"
#include "istudiorender.h"
#include "materialsystem/imaterialsystem.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "toolutils/toolwindowfactory.h"
#include "toolutils/basepropertiescontainer.h"
#include "toolutils/savewindowpositions.h"
#include "tier3/tier3.h"
#include "tier2/fileutils.h"
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
char const *GetVGuiControlsModuleName()
{
	return "VMTTool";
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
class CVMTTool : public CBaseToolSystem, public IFileMenuCallbacks, public IVMTDocCallback
{
	DECLARE_CLASS_SIMPLE( CVMTTool, CBaseToolSystem );

public:
	CVMTTool();

	// Inherited from IToolSystem
	virtual char const *GetToolName() { return "Material Editor"; }
	virtual const char *GetBindingsContextFile() { return "cfg/VMTTool.kb"; }
	virtual bool	Init();
    virtual void	Shutdown();
	virtual bool	CanQuit( );
	virtual void	PostMessage( HTOOLHANDLE hEntity, KeyValues *message );

	// Inherited from IFileMenuCallbacks
	virtual int		GetFileMenuItemsEnabled( );
	virtual void	AddRecentFilesToMenu( vgui::Menu *menu );
	virtual bool	GetPerforceFileName( char *pFileName, int nMaxLen ); 
	virtual vgui::Panel* GetRootPanel() { return this; }

	// Inherited from IVMTDocCallback
	virtual void	OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags );
	virtual void	AddShaderParameter( const char *pParam, const char *pWidget, const char *pTextType );
	virtual void	RemoveShaderParameter( const char *pParam );
	virtual void	AddFlagParameter( const char *pParam );
	virtual void	AddToolParameter( const char *pParam, const char *pWidget, const char *pTextType );
	virtual void	RemoveAllFlagParameters();
	virtual void	RemoveAllToolParameters();

	// Inherited from CBaseToolSystem
	virtual vgui::HScheme GetToolScheme();
	virtual vgui::Menu *CreateActionMenu( vgui::Panel *pParent );
	virtual void OnCommand( const char *cmd );
	virtual const char *GetRegistryName() { return "VMT"; }
	virtual vgui::MenuBar *CreateMenuBar( CBaseToolSystem *pParent );
	virtual void OnToolActivate();
	virtual void OnToolDeactivate();
	virtual CVMTDoc *GetDocument();
	virtual CBasePropertiesContainer	*GetProperties();
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

	// Methods related to the view menu
	MESSAGE_FUNC( OnToggleProperties, "OnToggleProperties" );
	MESSAGE_FUNC( OnToggleVMTPreview, "OnToggleVMTPreview" );
	MESSAGE_FUNC( OnShowFlags, "OnShowFlags" );
	MESSAGE_FUNC( OnShowToolParams, "OnShowToolParams" );
	MESSAGE_FUNC( OnDefaultLayout, "OnDefaultLayout" );

	// Methods related to the material menu
	MESSAGE_FUNC( OnSetParamsToDefault, "OnSetParamsToDefault" );
	
	// Returns the VMT preview window
	CVMTPanel *GetVMTPreview();

	// Which parameters are visible?
	bool IsFlagParamsVisible() const;
	bool IsToolParamsVisible() const;

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
	bool LoadDocument( char const *pDocName );

	// Updates the menu bar based on the current file
	void UpdateMenuBar( );

	// Shows element properties
	void ShowElementProperties( );

	// Create custom editors
	void InitEditorDict();

	void	CreateTools( CVMTDoc *doc );
	void	InitTools();
	void	DestroyTools();

	void	ToggleToolWindow( Panel *tool, char const *toolName );
	void	ShowToolWindow( Panel *tool, char const *toolName, bool visible );

	void	DestroyToolContainers();

	virtual char const *GetLogoTextureName();

private:	
	// All editable data
	CVMTDoc			*m_pDoc;

	// The menu bar
	CToolFileMenuBar *m_pMenuBar;

	// Element properties for editing material
	vgui::DHANDLE< CBasePropertiesContainer >	m_hProperties;

	// The sequence picker!
	vgui::DHANDLE< CVMTPanel > m_hVMTPreview;

	// The VMT editor type
	CDmeEditorType *m_pVMTType;

	// Separate undo context for the act busy tool
	CToolWindowFactory< ToolWindow > m_ToolWindowFactory;

	// List of tool + flag parameters
	CUtlVector<CUtlString> m_ToolParams;
	CUtlVector<CUtlString> m_FlagParams;

	bool m_bToolParamsVisible;
	bool m_bFlagParamsVisible;

	CUtlVector< DmElementHandle_t > m_toolElements;
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CVMTTool	*g_pVMTTool = NULL;

void CreateTools()
{
	g_pVMTTool = new CVMTTool();
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CVMTTool::CVMTTool()
{
	m_pVMTType = NULL;
	m_pMenuBar = NULL;
	m_pDoc = NULL;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CVMTTool::Init()
{
	m_bToolParamsVisible = false;
	m_bFlagParamsVisible = true;
	m_pDoc = NULL;
	m_RecentFiles.LoadFromRegistry( GetRegistryName() );

	// NOTE: This has to happen before BaseClass::Init
	g_pVGuiLocalize->AddFile( "resource/toolvmt_%language%.txt" );

	if ( !BaseClass::Init() )
		return false;

	InitEditorDict();

	return true;
}

void CVMTTool::Shutdown()
{
	m_RecentFiles.SaveToRegistry( GetRegistryName() );

	{
		CDisableUndoScopeGuard sg;
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
void CVMTTool::OnToolActivate()
{
	BaseClass::OnToolActivate();
}

void CVMTTool::OnToolDeactivate()
{
	BaseClass::OnToolDeactivate();
}


//-----------------------------------------------------------------------------
// Hides standard fields
//-----------------------------------------------------------------------------
void CVMTTool::HideStandardFields( CDmeEditorType *pEditorType, int nFieldFlags )
{
	CDmeEditorAttributeInfo *pInfo;

	if ( nFieldFlags & EDITOR_FIELD_NAME )
	{
		pInfo = CreateElement< CDmeEditorAttributeInfo >( "name info", pEditorType->GetFileId() );
		pEditorType->AddAttributeInfo( "name", pInfo ); 
		pInfo->m_bIsVisible = false;
		m_toolElements.AddToTail( pInfo->GetHandle() );
	}

	if ( nFieldFlags & EDITOR_FIELD_TYPE )
	{
		pInfo = CreateElement< CDmeEditorAttributeInfo >( "type info", pEditorType->GetFileId() );
		pInfo->m_bIsVisible = false;
		pEditorType->AddAttributeInfo( "type", pInfo ); 
		m_toolElements.AddToTail( pInfo->GetHandle() );
	}

	if ( nFieldFlags & EDITOR_FIELD_ID )
	{
		pInfo = CreateElement< CDmeEditorAttributeInfo >( "id info", pEditorType->GetFileId() );
		pInfo->m_bIsVisible = false;
		pEditorType->AddAttributeInfo( "id", pInfo ); 
		m_toolElements.AddToTail( pInfo->GetHandle() );
	}

	if ( nFieldFlags & EDITOR_FIELD_EDITORTYPE )
	{
		pInfo = CreateElement< CDmeEditorAttributeInfo >( "editor type info", pEditorType->GetFileId() );
		pInfo->m_bIsVisible = false;
		pEditorType->AddAttributeInfo( "editorType", pInfo ); 
		m_toolElements.AddToTail( pInfo->GetHandle() );
	}
}


//-----------------------------------------------------------------------------
// Update the editor dict based on the current material parameters
//-----------------------------------------------------------------------------
void CVMTTool::AddShaderParameter( const char *pParam, const char *pWidget, const char *pTextType )
{
	// anims only accept activity names
	CDmeEditorAttributeInfo *pInfo = CreateElement< CDmeEditorAttributeInfo >( "shader param info", m_pVMTType->GetFileId() );
	m_pVMTType->AddAttributeInfo( pParam, pInfo ); 
	pInfo->m_Widget = pWidget;
	if ( pTextType )
	{
		pInfo->SetValue( "texttype", pTextType );
	}
	m_toolElements.AddToTail( pInfo->GetHandle() );
}


//-----------------------------------------------------------------------------
// Update the editor dict based on the current material parameters
//-----------------------------------------------------------------------------
void CVMTTool::RemoveShaderParameter( const char *pParam )
{
	m_pVMTType->RemoveAttributeInfo( pParam );
}


//-----------------------------------------------------------------------------
// Which parameters are visible?
//-----------------------------------------------------------------------------
inline bool CVMTTool::IsFlagParamsVisible() const
{
	return m_bFlagParamsVisible;
}

inline bool CVMTTool::IsToolParamsVisible() const
{
	return m_bToolParamsVisible;
}

	
//-----------------------------------------------------------------------------
// Adds flags, tool parameters
//-----------------------------------------------------------------------------
void CVMTTool::AddFlagParameter( const char *pParam )
{
	Assert( m_pVMTType->GetAttributeInfo( pParam ) == NULL );

	int i = m_FlagParams.AddToTail( );
	m_FlagParams[i] = pParam;

	CDmeEditorAttributeInfo *pInfo = CreateElement< CDmeEditorAttributeInfo >( "flag param info", m_pVMTType->GetFileId() );
	m_pVMTType->AddAttributeInfo( pParam, pInfo ); 
	pInfo->m_bIsVisible = m_bFlagParamsVisible;
	pInfo->m_Widget = NULL;
	m_toolElements.AddToTail( pInfo->GetHandle() );
}

void CVMTTool::AddToolParameter( const char *pParam, const char *pWidget, const char *pTextType )
{
	Assert( m_pVMTType->GetAttributeInfo( pParam ) == NULL );

	int i = m_ToolParams.AddToTail( );
	m_ToolParams[i] = pParam;

	CDmeEditorAttributeInfo *pInfo = CreateElement< CDmeEditorAttributeInfo >( "tool param info", m_pVMTType->GetFileId() );
	m_pVMTType->AddAttributeInfo( pParam, pInfo ); 
	pInfo->m_bIsVisible = m_bToolParamsVisible;
	pInfo->m_Widget = pWidget;
	if ( pTextType )
	{
		pInfo->SetValue( "texttype", pTextType );
	}
	m_toolElements.AddToTail( pInfo->GetHandle() );
}

void CVMTTool::RemoveAllFlagParameters()
{
	int nCount = m_FlagParams.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		RemoveShaderParameter( m_FlagParams[i] );
	}
	m_FlagParams.RemoveAll();
}

void CVMTTool::RemoveAllToolParameters()
{
	int nCount = m_ToolParams.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		RemoveShaderParameter( m_ToolParams[i] );
	}
	m_ToolParams.RemoveAll();
}

	
//-----------------------------------------------------------------------------
// Create custom editors
//-----------------------------------------------------------------------------
void CVMTTool::InitEditorDict()
{
	CDmeEditorAttributeInfo *pInfo;

	// FIXME: This eventually will move to an .fgd-like file.
	g_pEditorTypeDict = CreateElement< CDmeEditorTypeDictionary >( "DmeEditorTypeDictionary", DMFILEID_INVALID );
	m_toolElements.AddToTail( g_pEditorTypeDict->GetHandle() );

	m_pVMTType = CreateElement< CDmeEditorType >( "vmt", DMFILEID_INVALID );
	HideStandardFields( m_pVMTType, EDITOR_FIELD_NAME | EDITOR_FIELD_TYPE | EDITOR_FIELD_ID | EDITOR_FIELD_EDITORTYPE );
	g_pEditorTypeDict->AddEditorType( m_pVMTType );
	m_toolElements.AddToTail( m_pVMTType->GetHandle() );

	// Create a picker for the shader name
	pInfo = CreateElement< CDmeEditorAttributeInfo >( "shader name info", DMFILEID_INVALID );
	m_pVMTType->AddAttributeInfo( "shader", pInfo ); 
	pInfo->m_Widget = "shaderpicker";
	pInfo->SetValue( "texttype", "shaderName" );
	m_toolElements.AddToTail( pInfo->GetHandle() );
}



//-----------------------------------------------------------------------------
//
// The Material menu
//
//-----------------------------------------------------------------------------
class CVMTMaterialMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CVMTMaterialMenuButton, CToolMenuButton );
public:
	CVMTMaterialMenuButton( vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget );
	virtual void OnShowMenu(vgui::Menu *menu);
};

CVMTMaterialMenuButton::CVMTMaterialMenuButton( vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget )
	: BaseClass( parent, panelName, text, pActionSignalTarget )
{
	AddMenuItem( "default", "#VMTSetToDefault", new KeyValues( "OnSetParamsToDefault" ), pActionSignalTarget );
	SetMenu(m_pMenu);
}

void CVMTMaterialMenuButton::OnShowMenu(vgui::Menu *menu)
{
	BaseClass::OnShowMenu( menu );

	// Update the menu
	int id;

	CVMTDoc *pDoc = g_pVMTTool->GetDocument();
	if ( pDoc )
	{
		id = m_Items.Find( "default" );
		m_pMenu->SetItemEnabled( id, true );
	}
	else
	{
		id = m_Items.Find( "default" );
		m_pMenu->SetItemEnabled( id, false );
	}
}


//-----------------------------------------------------------------------------
//
// The View menu
//
//-----------------------------------------------------------------------------
class CVMTViewMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CVMTViewMenuButton, CToolMenuButton );
public:
	CVMTViewMenuButton( vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget );
	virtual void OnShowMenu(vgui::Menu *menu);
};

CVMTViewMenuButton::CVMTViewMenuButton( vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget )
	: BaseClass( parent, panelName, text, pActionSignalTarget )
{
	AddCheckableMenuItem( "properties", "#VMTProperties", new KeyValues( "OnToggleProperties" ), pActionSignalTarget );
	AddCheckableMenuItem( "preview", "#VMTPreview", new KeyValues( "OnToggleVMTPreview" ), pActionSignalTarget );

	AddSeparator();

	AddCheckableMenuItem( "showflagparams", "#VMTShowFlags", new KeyValues( "OnShowFlags" ), pActionSignalTarget );
	AddCheckableMenuItem( "showtoolparams", "#VMTShowToolParams", new KeyValues( "OnShowToolParams" ), pActionSignalTarget );

	AddSeparator();

	AddMenuItem( "defaultlayout", "#VMTViewDefault", new KeyValues( "OnDefaultLayout" ), pActionSignalTarget );

	SetMenu(m_pMenu);
}

void CVMTViewMenuButton::OnShowMenu(vgui::Menu *menu)
{
	BaseClass::OnShowMenu( menu );

	// Update the menu
	int id;

	CVMTDoc *pDoc = g_pVMTTool->GetDocument();
	if ( pDoc )
	{
		id = m_Items.Find( "properties" );
		m_pMenu->SetItemEnabled( id, true );

		Panel *p;
		p = g_pVMTTool->GetProperties();
		Assert( p );
		m_pMenu->SetMenuItemChecked( id, ( p && p->GetParent() ) ? true : false );

		id = m_Items.Find( "preview" );
		m_pMenu->SetItemEnabled( id, true );
		
		p = g_pVMTTool->GetVMTPreview();
		Assert( p );
		m_pMenu->SetMenuItemChecked( id, ( p && p->GetParent() ) ? true : false );

		id = m_Items.Find( "showflagparams" );
		m_pMenu->SetItemEnabled( id, true );
		m_pMenu->SetMenuItemChecked( id, g_pVMTTool->IsFlagParamsVisible() );

		id = m_Items.Find( "showtoolparams" );
		m_pMenu->SetItemEnabled( id, true );
		m_pMenu->SetMenuItemChecked( id, g_pVMTTool->IsToolParamsVisible() );
	}
	else
	{
		id = m_Items.Find( "properties" );
		m_pMenu->SetItemEnabled( id, false );
		id = m_Items.Find( "preview" );
		m_pMenu->SetItemEnabled( id, false );
		id = m_Items.Find( "showflagparams" );
		m_pMenu->SetItemEnabled( id, false );
		id = m_Items.Find( "showtoolparams" );
		m_pMenu->SetItemEnabled( id, false );
	}
}


//-----------------------------------------------------------------------------
// Initializes the menu bar
//-----------------------------------------------------------------------------
vgui::MenuBar *CVMTTool::CreateMenuBar( CBaseToolSystem *pParent ) 
{
	m_pMenuBar = new CToolFileMenuBar( pParent, "VMTMenuBar" );

	// Sets info in the menu bar
	char title[ 64 ];
	ComputeMenuBarTitle( title, sizeof( title ) );
    m_pMenuBar->SetInfo( title );
	m_pMenuBar->SetToolName( GetToolName() );
	UpdateMenuBar();

	// Add menu buttons
	CToolMenuButton *pFileButton = CreateToolFileMenuButton( m_pMenuBar, "File", "&File", GetActionTarget(), this );
	CToolMenuButton *pEditButton = CreateToolEditMenuButton( m_pMenuBar, "Edit", "&Edit", GetActionTarget() );
	CToolMenuButton *pMaterialButton = new CVMTMaterialMenuButton( m_pMenuBar, "Material", "&Material", GetActionTarget() );
	CToolMenuButton *pSwitchButton = CreateToolSwitchMenuButton( m_pMenuBar, "Switcher", "&Tools", GetActionTarget() );
	CVMTViewMenuButton *pViewButton = new CVMTViewMenuButton( m_pMenuBar, "View", "&View", GetActionTarget() );

	m_pMenuBar->AddButton( pFileButton );
	m_pMenuBar->AddButton( pEditButton );
	m_pMenuBar->AddButton( pMaterialButton );
	m_pMenuBar->AddButton( pViewButton );
	m_pMenuBar->AddButton( pSwitchButton );

	return m_pMenuBar;
}


//-----------------------------------------------------------------------------
// Updates the menu bar based on the current file
//-----------------------------------------------------------------------------
void CVMTTool::UpdateMenuBar( )
{
	if ( !m_pDoc )
	{
		m_pMenuBar->SetFileName( "#VMTNoFile" );
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
int CVMTTool::GetFileMenuItemsEnabled( )
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

void CVMTTool::AddRecentFilesToMenu( vgui::Menu *pMenu )
{
	m_RecentFiles.AddToMenu( pMenu, GetActionTarget(), "OnRecent" );
}

	
//-----------------------------------------------------------------------------
// Returns the file name for perforce
//-----------------------------------------------------------------------------
bool CVMTTool::GetPerforceFileName( char *pFileName, int nMaxLen ) 
{ 
	if ( !m_pDoc )
		return false;
    Q_strncpy( pFileName, m_pDoc->GetFileName(), nMaxLen );
	return true;
}


//-----------------------------------------------------------------------------
// Derived classes can implement this to get a new scheme to be applied to this tool
//-----------------------------------------------------------------------------
vgui::HScheme CVMTTool::GetToolScheme() 
{ 
	return vgui::scheme()->LoadSchemeFromFile( "Resource/BoxRocket.res", "BoxRocket" );
}


//-----------------------------------------------------------------------------
// Creates the action menu
//-----------------------------------------------------------------------------
vgui::Menu *CVMTTool::CreateActionMenu( vgui::Panel *pParent )
{
	vgui::Menu *pActionMenu = new Menu( pParent, "ActionMenu" );
	pActionMenu->AddMenuItem( "#ToolHide", new KeyValues( "Command", "command", "HideActionMenu" ), GetActionTarget() );
	return pActionMenu;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CVMTTool::OnExit()
{
	// Throw up a "save" dialog?
	enginetools->Command( "quit\n" );
}


//-----------------------------------------------------------------------------
// Handle commands from the action menu and other menus
//-----------------------------------------------------------------------------
void CVMTTool::OnCommand( const char *cmd )
{
	if ( !V_stricmp( cmd, "HideActionMenu" ) )
	{
		if ( GetActionMenu() )
		{
			GetActionMenu()->SetVisible( false );
		}
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
		g_pVMTTool->OpenFileFromHistory( idx, cmd );
	}
	else if ( const char *pSuffix = StringAfterPrefix( cmd, "OnTool" ) )
	{
		int idx = Q_atoi( pSuffix );
		enginetools->SwitchToTool( idx );
	}
	else
	{
		BaseClass::OnCommand( cmd );
	}
}


//-----------------------------------------------------------------------------
// Messages from the engine
//-----------------------------------------------------------------------------
void CVMTTool::PostMessage( HTOOLHANDLE hEntity, KeyValues *message )
{
	if ( !Q_stricmp( message->GetName(), "EditMaterial" ) )
	{
		const char *pMaterialName = message->GetString( "material", "debug/debugempty" );

		char pLocalPath[ MAX_PATH ];
		char pAbsPath[ MAX_PATH ];
		if ( pMaterialName[0] == '/' && pMaterialName[1] == '/' && pMaterialName[2] != '/' )
		{
			Q_strncpy( pAbsPath, pMaterialName, sizeof(pAbsPath) );
			Q_DefaultExtension( pAbsPath, ".vmt", sizeof(pAbsPath) ); 
		}
		else
		{
			Q_snprintf( pLocalPath, sizeof(pLocalPath), "materials/%s", pMaterialName );
			Q_DefaultExtension( pLocalPath, ".vmt", sizeof(pLocalPath) );
			g_pFileSystem->RelativePathToFullPath( pLocalPath, "GAME", pAbsPath, sizeof(pAbsPath) );
		}

		Q_FixSlashes( pAbsPath );
		OpenSpecificFile( pAbsPath );
	}
}


//-----------------------------------------------------------------------------
// Derived classes can implement this to get notified when files are saved/loaded
//-----------------------------------------------------------------------------
void CVMTTool::OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues )
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
void CVMTTool::SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	// Compute starting directory
	char pStartingDir[ MAX_PATH ];
	GetModSubdirectory( "materials", pStartingDir, sizeof(pStartingDir) );

	if ( bOpenFile )
	{
		pDialog->SetTitle( "Open Material .VMT File", true );
	}
	else
	{
		pDialog->SetTitle( "Save Material .VMT File As", true );
	}

	pDialog->SetStartDirectoryContext( "vmt_session", pStartingDir );
	pDialog->AddFilter( "*.vmt", "VMT (*.vmt)", true );
}


//-----------------------------------------------------------------------------
// Called by SaveFile to allow clients to actually write the file out
//-----------------------------------------------------------------------------
bool CVMTTool::OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
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
// Called by SaveFile to allow clients to actually write the file out
//-----------------------------------------------------------------------------
bool CVMTTool::OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	if ( !m_pDoc )
		return true;

	m_pDoc->SetFileName( pFileName );
	if ( !m_pDoc->SaveToFile( ) )
		return false;

	m_RecentFiles.Add( pFileName, pFileFormat );
	m_RecentFiles.SaveToRegistry( GetRegistryName() );
	UpdateMenuBar();
	return true;
}


//-----------------------------------------------------------------------------
// Command handlers
//-----------------------------------------------------------------------------
void CVMTTool::PerformNew()
{
	OnCloseNoSave();
	NewDocument();
}

void CVMTTool::OnNew()
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		SaveFile( m_pDoc->GetFileName(), "vmt", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY,
			new KeyValues( "OnNew" ) );
		return;
	}
	PerformNew();
}

void CVMTTool::OnOpen( )
{
	int nFlags = 0;
	const char *pSaveFileName = NULL;
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		nFlags = FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY;
		pSaveFileName = m_pDoc->GetFileName();
	}

	OpenFile( "vmt", pSaveFileName, "vmt", nFlags );
}

void CVMTTool::OnSave()
{
	if ( m_pDoc )
	{
		SaveFile( m_pDoc->GetFileName(), "vmt", FOSM_SHOW_PERFORCE_DIALOGS );
	}
}

void CVMTTool::OnSaveAs()
{
	if ( m_pDoc )
	{
		SaveFile( NULL, "vmt", FOSM_SHOW_PERFORCE_DIALOGS );
	}
}

void CVMTTool::OnClose()
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		SaveFile( m_pDoc->GetFileName(), "vmt", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY, 
			new KeyValues( "OnClose" ) );
		return;
	}

	OnCloseNoSave();
}

void CVMTTool::OnCloseNoSave()
{
	DestroyTools();

	if ( m_pDoc )
	{
		CAppNotifyScopeGuard sg( "CVMTTool::OnCloseNoSave", NOTIFY_CHANGE_OTHER );

		delete m_pDoc;
		m_pDoc = NULL;

		if ( m_hProperties )
		{
			m_hProperties->SetObject( NULL );
		}
	}
	
	UpdateMenuBar( );
}

void CVMTTool::OnMarkNotDirty()
{
	if ( m_pDoc )
	{
		m_pDoc->SetDirty( false );
	}
}


//-----------------------------------------------------------------------------
// Open a specific file
//-----------------------------------------------------------------------------
void CVMTTool::OpenSpecificFile( const char *pFileName )
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
	}

	OpenFile( pFileName, "vmt", pSaveFileName, "vmt", nFlags );
}


//-----------------------------------------------------------------------------
// Show the save document query dialog
//-----------------------------------------------------------------------------
void CVMTTool::OpenFileFromHistory( int slot, const char *pCommand )
{
	const char *pFileName = m_RecentFiles.GetFile( slot );
	if ( !pFileName )
		return;
	OpenSpecificFile( pFileName );
}

bool CVMTTool::CanQuit()
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		// Show Save changes Yes/No/Cancel and re-quit if hit yes/no
		SaveFile( m_pDoc->GetFileName(), "vmt", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY, 
			new KeyValues( "OnQuit" ) );
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Various command handlers related to the Edit menu
//-----------------------------------------------------------------------------
void CVMTTool::OnUndo()
{
	CDisableUndoScopeGuard guard;
	g_pDataModel->Undo();
}

void CVMTTool::OnRedo()
{
	CDisableUndoScopeGuard guard;
	g_pDataModel->Redo();
}

void CVMTTool::OnDescribeUndo()
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
// Inherited from IVMTDocCallback
//-----------------------------------------------------------------------------
void CVMTTool::OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
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
// Output : CVMTDoc
//-----------------------------------------------------------------------------
CVMTDoc *CVMTTool::GetDocument()
{
	return m_pDoc;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : virtual CBasePropertiesContainer
//-----------------------------------------------------------------------------
CBasePropertiesContainer *CVMTTool::GetProperties()
{
	return m_hProperties.Get();
}

CVMTPanel *CVMTTool::GetVMTPreview()
{
	return m_hVMTPreview.Get();
}


//-----------------------------------------------------------------------------
// Initializes the tools
//-----------------------------------------------------------------------------
void CVMTTool::InitTools()
{
	ShowElementProperties();

	// FIXME: There are no tool windows here; how should this work?
	// These panels are saved
	windowposmgr->RegisterPanel( "properties", m_hProperties, false );
	windowposmgr->RegisterPanel( "vmtpanel", m_hVMTPreview, false );

	if ( !windowposmgr->LoadPositions( "cfg/vmt.txt", this, &m_ToolWindowFactory, "VMT" ) )
	{
		OnDefaultLayout();
	}
}


//-----------------------------------------------------------------------------
// Loads up a new document
//-----------------------------------------------------------------------------
bool CVMTTool::LoadDocument( char const *pDocName )
{
	Assert( !m_pDoc );

	DestroyTools();

	m_pDoc = new CVMTDoc( this );
	if ( !m_pDoc->LoadFromFile( pDocName ) )
	{
		delete m_pDoc;
		m_pDoc = NULL;
		Warning( "Fatal error loading '%s'\n", pDocName );
		return false;
	}

	CreateTools( m_pDoc );
	InitTools();
	return true;
}


//-----------------------------------------------------------------------------
// Loads up a new document
//-----------------------------------------------------------------------------
void CVMTTool::NewDocument( )
{
	Assert( !m_pDoc );

	m_pDoc = new CVMTDoc( this );
	m_pDoc->CreateNew( );

	CreateTools( m_pDoc );
	UpdateMenuBar( );
	InitTools();
}


//-----------------------------------------------------------------------------
// Shows element properties
//-----------------------------------------------------------------------------
void CVMTTool::ShowElementProperties( )
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

void CVMTTool::DestroyTools()
{
	windowposmgr->SavePositions( "cfg/vmt.txt", "VMT" );

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
	if ( m_hVMTPreview.Get() )
	{
		windowposmgr->UnregisterPanel( m_hVMTPreview.Get() );
		delete m_hVMTPreview.Get();
		m_hVMTPreview = NULL;
	}
}

void CVMTTool::CreateTools( CVMTDoc *doc )
{
	if ( !m_hProperties.Get() )
	{
		m_hProperties = new CBasePropertiesContainer( NULL, m_pDoc, g_pEditorTypeDict );
	}
	if ( !m_hVMTPreview.Get() )
	{
		m_hVMTPreview = new CVMTPanel( NULL, "VMT Preview" );
		SETUP_PANEL( m_hVMTPreview.Get() );
		m_hVMTPreview->SetMaterial( m_pDoc->GetPreviewMaterial() );
	}
	RegisterToolWindow( m_hProperties );
	RegisterToolWindow( m_hVMTPreview );
}

void CVMTTool::ShowToolWindow( Panel *tool, char const *toolName, bool visible )
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

void CVMTTool::ToggleToolWindow( Panel *tool, char const *toolName )
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

void CVMTTool::DestroyToolContainers()
{
	int c = ToolWindow::GetToolWindowCount();
	for ( int i = c - 1; i >= 0 ; --i )
	{
		ToolWindow *kill = ToolWindow::GetToolWindow( i );
		delete kill;
	}
}

void CVMTTool::OnDefaultLayout()
{
	int y = m_pMenuBar->GetTall();

	int usew, useh;
	GetSize( usew, useh );

	DestroyToolContainers();

	Assert( ToolWindow::GetToolWindowCount() == 0 );

	CBasePropertiesContainer *properties = GetProperties();
	CVMTPanel *pVMTPreview = GetVMTPreview();

	// Need three containers
	ToolWindow *pPropertyWindow = m_ToolWindowFactory.InstanceToolWindow( GetClientArea(), false, properties, "#VMTProperties", false );
	ToolWindow *pVMTPreviewWindow = m_ToolWindowFactory.InstanceToolWindow( GetClientArea(), false, pVMTPreview, "#VMTPreview", false );

	int halfScreen = usew / 2;
	int bottom = useh - y;
	int sy = (bottom - y) / 2;

	SetMiniViewportBounds( halfScreen, y, halfScreen, sy - y );
	pPropertyWindow->SetBounds( 0, y, halfScreen, bottom );
	pVMTPreviewWindow->SetBounds( halfScreen, sy, halfScreen, bottom - sy );
}

void CVMTTool::OnToggleProperties()
{
	if ( m_hProperties.Get() )
	{ 
		ToggleToolWindow( m_hProperties.Get(), "#VMTProperties" );
	}
}

void CVMTTool::OnToggleVMTPreview()
{
	if ( m_hVMTPreview.Get() )
	{ 
		ToggleToolWindow( m_hVMTPreview.Get(), "#VMTPreview" );
	}
}


//-----------------------------------------------------------------------------
// Show/hide tool params + flags
//-----------------------------------------------------------------------------
void CVMTTool::OnShowFlags()
{
	m_bFlagParamsVisible = !m_bFlagParamsVisible;

	int nCount = m_FlagParams.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeEditorAttributeInfo *pInfo = m_pVMTType->GetAttributeInfo( m_FlagParams[i] );
		Assert( pInfo );
		pInfo->m_bIsVisible = m_bFlagParamsVisible;
	}
	if ( m_hProperties.Get() )
	{
		m_hProperties->Refresh();
	}
}

void CVMTTool::OnShowToolParams()
{
	m_bToolParamsVisible = !m_bToolParamsVisible;

	int nCount = m_ToolParams.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeEditorAttributeInfo *pInfo = m_pVMTType->GetAttributeInfo( m_ToolParams[i] );
		Assert( pInfo );
		pInfo->m_bIsVisible = m_bToolParamsVisible;
	}
	if ( m_hProperties.Get() )
	{
		m_hProperties->Refresh();
	}
}


//-----------------------------------------------------------------------------
// Sets shader parameters to the default for that shader
//-----------------------------------------------------------------------------
void CVMTTool::OnSetParamsToDefault()
{
	if ( m_pDoc )
	{
		m_pDoc->SetParamsToDefault();
	}
}

char const *CVMTTool::GetLogoTextureName()
{
	return "vgui/tools/vmt/vmt_logo";
}