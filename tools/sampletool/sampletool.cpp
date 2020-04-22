//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================
#include "cbase.h"

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

using namespace vgui;


const char *GetVGuiControlsModuleName()
{
	return "SampleTool";
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
// Implementation of the sample tool
//-----------------------------------------------------------------------------
class CSampleTool : public CBaseToolSystem, public IFileMenuCallbacks
{
	DECLARE_CLASS_SIMPLE( CSampleTool, CBaseToolSystem );

public:
	CSampleTool();

	// Inherited from IToolSystem
	virtual const char *GetToolName() { return "Sample Tool"; }
	virtual const char *GetBindingsContextFile() { return "cfg/SampleTool.kb"; }
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

	void		OpenFileFromHistory( int slot );
	virtual void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual void OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues );

private:
	// Loads up a new document
	void LoadDocument( const char *pDocName );

	// Updates the menu bar based on the current file
	void UpdateMenuBar( );

	// Shows element properties
	void ShowElementProperties( );

	virtual const char *GetLogoTextureName();

};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CSampleTool	*g_pSampleTool = NULL;

void CreateTools()
{
	g_pSampleTool = new CSampleTool();
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSampleTool::CSampleTool()
{
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CSampleTool::Init( )
{
	m_RecentFiles.LoadFromRegistry( GetRegistryName() );

	// NOTE: This has to happen before BaseClass::Init
//	g_pVGuiLocalize->AddFile( "resource/toolsample_%language%.txt" );

	if ( !BaseClass::Init( ) )
		return false;

	return true;
}

void CSampleTool::Shutdown()
{
	m_RecentFiles.SaveToRegistry( GetRegistryName() );
	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Derived classes can implement this to get a new scheme to be applied to this tool
//-----------------------------------------------------------------------------
vgui::HScheme CSampleTool::GetToolScheme() 
{ 
	return vgui::scheme()->LoadSchemeFromFile( "Resource/BoxRocket.res", "SampleTool" );
}


//-----------------------------------------------------------------------------
// Initializes the menu bar
//-----------------------------------------------------------------------------
vgui::MenuBar *CSampleTool::CreateMenuBar( CBaseToolSystem *pParent ) 
{
	CToolMenuBar *pMenuBar = new CToolMenuBar( pParent, "Main Menu Bar" );

	// Sets info in the menu bar
	char title[ 64 ];
	ComputeMenuBarTitle( title, sizeof( title ) );
	pMenuBar->SetInfo( title );
	pMenuBar->SetToolName( GetToolName() );

	// Add menu buttons
	CToolMenuButton *pFileButton = CreateToolFileMenuButton( pMenuBar, "File", "&File", GetActionTarget(), this );
	CToolMenuButton *pSwitchButton = CreateToolSwitchMenuButton( pMenuBar, "Switcher", "&Tools", GetActionTarget() );

	pMenuBar->AddButton( pFileButton );
	pMenuBar->AddButton( pSwitchButton );

	return pMenuBar;
}


//-----------------------------------------------------------------------------
// Creates the action menu
//-----------------------------------------------------------------------------
vgui::Menu *CSampleTool::CreateActionMenu( vgui::Panel *pParent )
{
	vgui::Menu *pActionMenu = new Menu( pParent, "ActionMenu" );
	pActionMenu->AddMenuItem( "#ToolHide", new KeyValues( "Command", "command", "HideActionMenu" ), GetActionTarget() );
	return pActionMenu;
}

//-----------------------------------------------------------------------------
// Inherited from IFileMenuCallbacks
//-----------------------------------------------------------------------------
int	CSampleTool::GetFileMenuItemsEnabled( )
{
	int nFlags = FILE_ALL;
	if ( m_RecentFiles.IsEmpty() )
	{
		nFlags &= ~(FILE_RECENT | FILE_CLEAR_RECENT);
	}
	return nFlags;
}

void CSampleTool::AddRecentFilesToMenu( vgui::Menu *pMenu )
{
	m_RecentFiles.AddToMenu( pMenu, GetActionTarget(), "OnRecent" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CSampleTool::OnExit()
{
	enginetools->Command( "quit\n" );
}

//-----------------------------------------------------------------------------
// Handle commands from the action menu and other menus
//-----------------------------------------------------------------------------
void CSampleTool::OnCommand( const char *cmd )
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
	else
	{
		BaseClass::OnCommand( cmd );
	}
}


//-----------------------------------------------------------------------------
// Command handlers
//-----------------------------------------------------------------------------
void CSampleTool::OnNew()
{
	// FIXME: Implement
}

void CSampleTool::OnOpen()
{
	OpenFile( "txt" );
}

bool CSampleTool::OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	// FIXME: Implement

	m_RecentFiles.Add( pFileName, pFileFormat );
	return true;
}

void CSampleTool::OnSave()
{
	// FIXME: Implement
}

void CSampleTool::OnSaveAs()
{
	SaveFile( NULL, NULL, 0 );
}

bool CSampleTool::OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	// FIXME: Implement

	m_RecentFiles.Add( pFileName, pFileFormat );
	return true;
}

void CSampleTool::OnClose()
{
	// FIXME: Implement
}

void CSampleTool::OnCloseNoSave()
{
	// FIXME: Implement
}

void CSampleTool::OnMarkNotDirty()
{
	// FIXME: Implement
}


//-----------------------------------------------------------------------------
// Show the save document query dialog
//-----------------------------------------------------------------------------
void CSampleTool::OpenFileFromHistory( int slot )
{
	const char *pFileName = m_RecentFiles.GetFile( slot );
	OnReadFileFromDisk( pFileName, NULL, 0 );
}


//-----------------------------------------------------------------------------
// Called when file operations complete
//-----------------------------------------------------------------------------
void CSampleTool::OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues )
{
	// FIXME: Implement
}


//-----------------------------------------------------------------------------
// Show the File browser dialog
//-----------------------------------------------------------------------------
void CSampleTool::SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	char pStartingDir[ MAX_PATH ];
	GetModSubdirectory( NULL, pStartingDir, sizeof(pStartingDir) );

	pDialog->SetTitle( "Choose SampleTool .txt file", true );
	pDialog->SetStartDirectoryContext( "sample_session", pStartingDir );
	pDialog->AddFilter( "*.txt", "SampleTool (*.txt)", true );
}

const char *CSampleTool::GetLogoTextureName()
{
	return "vgui/tools/sampletool/sampletool_logo";
}