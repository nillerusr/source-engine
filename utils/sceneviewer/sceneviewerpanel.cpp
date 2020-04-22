//========= Copyright Valve Corporation, All rights reserved. ============//
//
// CSceneViewerPanel Definition
//
//=============================================================================


// Standard includes
#include <direct.h>


// Valve includes
#include "tier1/utldict.h"
#include "tier1/KeyValues.h"
#include "tier3/tier3.h"
#include "vstdlib/random.h"
#include "vgui/IVGui.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui_controls/MenuButton.h"
#include "vgui_controls/FileOpenDialog.h"
#include "vgui_controls/MessageBox.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmefaceset.h"
#include "movieobjects/dmematerial.h"
#include "movieobjects/dmobjserializer.h"
#include "dme_controls/dmedagrenderpanel.h"
#include "vgui/keycode.h"
#include "filesystem.h"


// Local includes
#include "SceneViewerPanel.h"


#define DEFAULT_FILE_FORMAT "model"

//-----------------------------------------------------------------------------
// Creates the scene viewer panel
//-----------------------------------------------------------------------------
vgui::Panel *CreateSceneViewerPanel()
{
	// add our main window
	vgui::Panel *pSceneViewer = new CSceneViewerPanel;
	pSceneViewer->SetParent( g_pVGuiSurface->GetEmbeddedPanel() );
	return pSceneViewer;
}



//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CSceneViewerMenuButton : public vgui::MenuButton
{
	DECLARE_CLASS_SIMPLE( CSceneViewerMenuButton, vgui::MenuButton );
public:
	CSceneViewerMenuButton( CSceneViewerPanel *parent, const char *panelName, const char *text );

	virtual void OnShowMenu(vgui::Menu *menu) = 0;

	// Add a simple text item to the menu
	virtual int AddMenuItem( char const *itemName, const char *itemText, KeyValues *message, Panel *target, const KeyValues *userData = NULL );
	virtual int AddCheckableMenuItem( char const *itemName, const char *itemText, KeyValues *message, Panel *target, const KeyValues *userData = NULL );

	void		Reset();

protected:

	vgui::Menu			*m_pMenu;
	vgui::Panel			*m_pActionTarget;

	CSceneViewerPanel	*m_pUI;

	CUtlDict< int, unsigned short >	m_Items;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CSceneViewerMenuButton::CSceneViewerMenuButton( 
	CSceneViewerPanel *parent, 
	const char *panelName, 
	const char *text )
: BaseClass( (vgui::Panel * )parent, panelName, text )
, m_pUI( parent )
, m_pActionTarget( (vgui::Panel *)parent )
{
	m_pMenu = new vgui::Menu( this, "Menu" );
}
	

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerMenuButton::Reset()
{
	m_Items.RemoveAll();
	m_pMenu->DeleteAllItems();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CSceneViewerMenuButton::AddMenuItem( char const *itemName, const char *itemText, KeyValues *message, Panel *target, const KeyValues *userData /*= NULL*/ )
{
	int id = m_pMenu->AddMenuItem(itemText, message, target, userData);
	m_Items.Insert( itemName, id );
	return id;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CSceneViewerMenuButton::AddCheckableMenuItem( char const *itemName, const char *itemText, KeyValues *message, Panel *target, const KeyValues *userData /*= NULL*/ )
{
	int id = m_pMenu->AddCheckableMenuItem(itemText, message, target, userData);
	m_Items.Insert( itemName, id );
	return id;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CSceneViewerEditMenuButton : public CSceneViewerMenuButton
{
	DECLARE_CLASS_SIMPLE( CSceneViewerEditMenuButton, CSceneViewerMenuButton );
public:
	CSceneViewerEditMenuButton( CSceneViewerPanel *parent, const char *panelName, const char *text );
	virtual void OnShowMenu(vgui::Menu *menu);

private:

	vgui::Menu		*m_pRecentFiles;
	int				m_nRecentFiles;
};

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CSceneViewerMenuBar : public vgui::MenuBar
{
	DECLARE_CLASS_SIMPLE( CSceneViewerMenuBar, vgui::MenuBar );

public:

	CSceneViewerMenuBar(Panel *parent, const char *panelName);

	virtual void PerformLayout();

	void SetFileName( char const *filename );

private:

	vgui::Label		*m_pFileName;
};

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CSceneViewerMenuBar::CSceneViewerMenuBar(Panel *parent, const char *panelName) :
BaseClass( parent, panelName )
{
	m_pFileName = new vgui::Label( this, "IFMFileName", "" );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerMenuBar::PerformLayout()
{
	BaseClass::PerformLayout();

	int w, h;
	GetSize( w, h );

	int barx, bary;
	GetContentSize( barx, bary );

	int faredge = w - 2;
	int nearedge = barx + 2;

	int mid = ( nearedge + faredge ) * 0.5f;
	
	int cw, ch;
	m_pFileName->GetContentSize( cw, ch );
	m_pFileName->SetBounds( mid - cw * 0.5f, 0, cw, h );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerMenuBar::SetFileName( char const *name )
{
	m_pFileName->SetText( name );
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CSceneViewerPanel::CSceneViewerPanel()
: vgui::Panel( NULL, "SceneViewer" )
{
	m_pMenuBar = new CSceneViewerMenuBar( this, "Main Menu Bar" );
	m_pMenuBar->SetSize( 10, 28 );

	// Next create a menu
	vgui::Menu *pMenu = new vgui::Menu(NULL, "File Menu");
	pMenu->AddMenuItem( "&New", new KeyValues( "New" ), this );
	pMenu->AddMenuItem( "&Open", new KeyValues( "Open" ), this );
	pMenu->AddMenuItem( "&Save", new KeyValues( "Save" ), this );
	pMenu->AddMenuItem( "Save &As", new KeyValues( "SaveAs" ), this );
	pMenu->AddMenuItem( "Save &Current As", new KeyValues( "SaveCurrentAs" ), this );
	pMenu->AddMenuItem( "E&xit", new KeyValues( "Exit" ), this );

	m_pMenuBar->AddMenu( "&File", pMenu );

	CSceneViewerEditMenuButton *editMenu = new CSceneViewerEditMenuButton( this, "Edit Menu", "&Edit" );

	m_pMenuBar->AddButton( editMenu );

	vgui::Menu *pWindowMenu = new vgui::Menu( NULL, "Windows Menu" );
	pWindowMenu->AddMenuItem( "3D &View", new KeyValues( "Show3DView" ), this );
	pWindowMenu->AddMenuItem( "&Combo Editor", new KeyValues( "ShowComboEditor" ), this );
	pWindowMenu->AddMenuItem( "&Asset Builder", new KeyValues( "ShowAssetBuilder" ), this );
	pWindowMenu->AddMenuItem( "&Nerd Editor", new KeyValues( "ShowNerdEditor" ), this );
	pWindowMenu->AddMenuItem( "C&onsole", new KeyValues( "ShowConsole" ), this );

	m_pMenuBar->AddMenu( "&Windows", pWindowMenu );

	m_pClientArea = new vgui::Panel( this, "ClientArea" ) ;

	m_pClipViewPanel = new CClipViewPanel( m_pClientArea, "Clip Viewer" );
	m_pClipViewPanel->SetBounds( 10, 40, 500, 500 );

	m_pCombinationEditor = new CDmeCombinationSystemEditorFrame( m_pClientArea, "Combination Control Builder" );
	m_pCombinationEditor->SetBounds( 100, 100, 512, 512 );
	m_pCombinationEditor->SetVisible( false );
	m_pCombinationEditor->SetDeleteSelfOnClose( false );
	m_pCombinationEditor->AddActionSignalTarget( this );

	m_pAssetBuilder = new CAssetBuilderFrame( m_pClientArea, "Asset Builder" );
	m_pAssetBuilder->SetParent( g_pVGuiSurface->GetEmbeddedPanel() );
	m_pAssetBuilder->SetVisible( false );
	m_pAssetBuilder->SetBounds( 50, 50, 512, 512 );
	m_pAssetBuilder->SetDeleteSelfOnClose( false );

	m_pNerdEditor = new CElementPropertiesTree( m_pClientArea, this, NULL );
	m_pNerdEditor->SetParent( g_pVGuiSurface->GetEmbeddedPanel() );
	m_pNerdEditor->SetVisible( false );
	m_pNerdEditor->SetBounds( 50, 50, 512, 512 );
	m_pNerdEditor->SetDeleteSelfOnClose( false );

	m_pFileOpenStateMachine = new vgui::FileOpenStateMachine( this, this );
	m_pFileOpenStateMachine->AddActionSignalTarget( this );

	m_pConsole = new vgui::CConsoleDialog( this, "ConsoleDialog", false );
	m_pConsole->AddActionSignalTarget( this );
	m_bConsolePositioned = false;

	m_pRoot = NULL;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CSceneViewerEditMenuButton::CSceneViewerEditMenuButton( CSceneViewerPanel *parent, const char *panelName, const char *text )
: BaseClass( parent, panelName, text )
{
	AddMenuItem( "undo", "&Undo", new KeyValues ( "Command", "command", "OnUndo"), parent);
	AddMenuItem( "redo", "&Redo", new KeyValues ("Command", "command", "OnRedo"), parent);
	AddMenuItem( "describe", "Describe Undo Stack", new KeyValues( "Command", "command", "OnDescribeUndoStack" ), parent );
	AddMenuItem( "properties", "&Properties...", new KeyValues( "Command", "command", "OnEdit" ), parent );

	SetMenu(m_pMenu);
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerEditMenuButton::OnShowMenu(vgui::Menu *menu)
{
	// Update the menu
	// new and exit are always active
	// Open is avail if there's no open document
	// save and saveas are active it there's a document and it's dirty
	// close is active if there's a document
	int id;
	
	char sz[ 512 ];

	id = m_Items.Find( "undo" );
	if ( g_pDataModel->CanUndo() )
	{
		m_pMenu->SetItemEnabled( id, true );
		Q_snprintf( sz, sizeof( sz ), "Undo '%s'", g_pDataModel->GetUndoDesc() );
		m_pMenu->UpdateMenuItem( id, sz, new KeyValues( "Command", "command", "OnUndo" ) );
	}
	else
	{
		m_pMenu->SetItemEnabled( id, false );
		m_pMenu->UpdateMenuItem( id, "Undo...", new KeyValues( "Command", "command", "OnUndo" ) );
	}
	id = m_Items.Find( "redo" );
	if ( g_pDataModel->CanRedo() )
	{
		m_pMenu->SetItemEnabled( id, true );
		Q_snprintf( sz, sizeof( sz ), "Redo '%s'", g_pDataModel->GetRedoDesc() );
		m_pMenu->UpdateMenuItem( id, sz, new KeyValues( "Command", "command", "OnRedo" ) );
	}
	else
	{
		m_pMenu->SetItemEnabled( id, false );
		m_pMenu->UpdateMenuItem( id, "Redo...", new KeyValues( "Command", "command", "OnRedo" ) );
	}
	id = m_Items.Find( "describe" );
	if ( g_pDataModel->CanUndo() )
	{
		m_pMenu->SetItemEnabled( id, true );
	}
	else
	{
		m_pMenu->SetItemEnabled( id, false );
	}
	id = m_Items.Find( "properties" );
	m_pMenu->SetItemEnabled( id, m_pUI->GetScene() ? true : false );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CSceneViewerPanel::~CSceneViewerPanel()
{
	if ( m_pRoot )
	{
		g_pDataModel->UnloadFile( m_pRoot->GetFileId() );
	}
}



//-----------------------------------------------------------------------------
// Console support
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnCommandSubmitted( const char *command )
{
	CCommand args;
	args.Tokenize( command );

	ConCommandBase *pCommandBase = g_pCVar->FindCommandBase( args[0] );
	if ( !pCommandBase )
	{
		ConWarning( "Unknown command or convar '%s'!\n", args[0] );
		return;
	}

	if ( pCommandBase->IsCommand() )
	{
		ConCommand *pCommand = static_cast<ConCommand*>( pCommandBase );
		pCommand->Dispatch( args );
		return;
	}

	ConVar *pConVar = static_cast< ConVar* >( pCommandBase );
	if ( args.ArgC() == 1)
	{
		if ( pConVar->IsFlagSet( FCVAR_NEVER_AS_STRING ) )
		{
			ConMsg( "%s = %f\n", args[0], pConVar->GetFloat() );
		}
		else
		{
			ConMsg( "%s = %s\n", args[0], pConVar->GetString() );
		}
		return;
	}

	if ( pConVar->IsFlagSet( FCVAR_NEVER_AS_STRING ) )
	{
		pConVar->SetValue( (float)atof( args[1] ) );
	}
	else
	{
		pConVar->SetValue( args.ArgS() );
	}
}


//-----------------------------------------------------------------------------
// The clip viewer has key focus
//-----------------------------------------------------------------------------
vgui::VPANEL CSceneViewerPanel::GetCurrentKeyFocus()
{
	return m_pClipViewPanel->GetVPanel();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
vgui::VPANEL CSceneViewerPanel::GetCurrentMouseFocus()
{
	return m_pClipViewPanel->GetVPanel();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnKeyCodePressed( vgui::KeyCode code )
{
	BaseClass::OnKeyCodePressed( code );

	if ( code == KEY_BACKQUOTE )
	{
		if ( !m_pConsole->IsVisible() )
		{
			m_pConsole->Activate();
		}
		else
		{
			m_pConsole->Hide();
		}
	}
	else if ( code == KEY_F5 )
	{
		Reload();
	}
}


//-----------------------------------------------------------------------------
// VGUI commands
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnCommand( char const *cmd )
{
	if ( !Q_stricmp( cmd, "OnEdit" ) )
	{
		OnEdit();
	}
	else if ( !Q_stricmp( cmd, "OnUndo" ) )
	{
		OnUndo();
	}
	else if ( !Q_stricmp( cmd, "OnRedo" ) )
	{
		OnRedo();
	}
	else
	{
		BaseClass::OnCommand( cmd );
	}
}


//-----------------------------------------------------------------------------
// Callback From m_pFileOpenStateMachine
//-----------------------------------------------------------------------------
void CSceneViewerPanel::SetupFileOpenDialog(
	vgui::FileOpenDialog *pDialog,
	bool bOpenFile, const char *pFileFormat,
	KeyValues *pContextKeyValues )
{
	if ( m_fileDirectory.IsEmpty() )
	{
		char pStartingDir[ MAX_PATH ];
		if ( !vgui::system()->GetRegistryString(
			"HKEY_CURRENT_USER\\Software\\Valve\\sceneviewer\\dmxfiles\\opendir", pStartingDir, sizeof( pStartingDir ) ) )
		{
			// Compute starting directory
			_getcwd( pStartingDir, sizeof( pStartingDir ) );
		}
		m_fileDirectory = pStartingDir;
	}

	pDialog->SetStartDirectoryContext( "sceneviewer_browser", m_fileDirectory.Get() );

	if ( !bOpenFile && !Q_strcmp( pContextKeyValues->GetName(), "SaveCurrentAs" ) )
	{
		pDialog->AddFilter( "*.obj", "OBJ File (*.obj)", false, "obj" );
		pDialog->AddFilter( "*.*", "All Files (*.*)", !bOpenFile, "obj" );
		pDialog->SetTitle( "Save Current State As OBJ File", true );
	}
	else
	{
		pDialog->AddFilter( "*.obj", "OBJ File (*.obj)", false, "obj" );
		pDialog->AddFilter( "*.dmx", "DMX File (*.dmx)", bOpenFile, DEFAULT_FILE_FORMAT );
		pDialog->AddFilter( "*.*", "All Files (*.*)", !bOpenFile, DEFAULT_FILE_FORMAT );

		if ( bOpenFile )
		{
			pDialog->SetTitle( "Open DMX/OBJ File", true );
		}
		else
		{
			pDialog->SetTitle( "Save DMX/OBJ File As", true );
		}
	}
}


//-----------------------------------------------------------------------------
// Callback From m_pFileOpenStateMachine
//-----------------------------------------------------------------------------
bool CSceneViewerPanel::OnReadFileFromDisk(
	const char *pFileName,
	const char *pFileFormat,
	KeyValues *pContextKeyValues )
{
	// Regardless if the file sucessfully loaded or not, save the directory portion to use next time
	{
		char buf[ MAX_PATH ];
		Q_strncpy( buf, pFileName, sizeof( buf ) );
		Q_FixSlashes( buf );
		Q_StripFilename( buf );
		m_fileDirectory = buf;
		_fullpath( buf, m_fileDirectory, sizeof( buf ) );
		m_fileDirectory = buf;
		vgui::system()->SetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\sceneviewer\\dmxfiles\\opendir", m_fileDirectory.Get() );
	}

	return Load( pFileName );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CSceneViewerPanel::SaveCurrentAs(
	const char *pFilename )
{
	if ( !m_pRoot )
		return false;

	// Find each mesh under pRoot
	CDmeDag *pModel = m_pRoot->GetValueElement< CDmeDag >( "model" );
	if ( !pModel )
		return false;

	CDmeDag *pDag;
	CDmeMesh *pMesh;

	// Loop through each mesh and set the bind state to something funny
	CUtlStack< CDmeDag * > traverseStack;
	traverseStack.Push( pModel );

	while ( traverseStack.Count() )
	{
		traverseStack.Pop( pDag );
		if ( !pDag )
			continue;

		// Push all children onto stack in reverse order
		for ( int nChildIndex = pDag->GetChildCount() - 1; nChildIndex >= 0; --nChildIndex )
		{
			traverseStack.Push( pDag->GetChild( nChildIndex ) );
		}

		// See if there's a mesh associated with this dag
		pMesh = CastElement< CDmeMesh >( pDag->GetShape() );
		if ( !pMesh )
			continue;

		// Create a new base state
		CDmeVertexData *pBind = pMesh->FindBaseState( "bind" );
		if ( !pBind )
			continue;

		CDmeVertexData *pNewBind = pMesh->FindOrCreateBaseState( "__sceneviewer_newBind" );
		pBind->CopyTo( pNewBind );
		pBind->SetName( "__sceneviewer_oldBind" );
		pNewBind->SetName( "bind" );
		pMesh->SetBaseStateToDeltas( pNewBind );
	}

	CDmObjSerializer().WriteOBJ( pFilename, m_pRoot, false );

	traverseStack.Push( pModel );
	while ( traverseStack.Count() )
	{
		traverseStack.Pop( pDag );
		if ( !pDag )
			continue;

		// Push all children onto stack in reverse order
		for ( int nChildIndex = pDag->GetChildCount() - 1; nChildIndex >= 0; --nChildIndex )
		{
			traverseStack.Push( pDag->GetChild( nChildIndex ) );
		}

		// See if there's a mesh associated with this dag
		pMesh = CastElement< CDmeMesh >( pDag->GetShape() );
		if ( !pMesh )
			continue;

		CDmeVertexData *pOldBind = pMesh->FindBaseState( "__sceneviewer_oldBind" );
		CDmeVertexData *pNewBind = pMesh->FindBaseState( "bind" );
		if ( !pOldBind || !pNewBind )
			continue;

		pMesh->DeleteBaseState( "bind" );
		pOldBind->SetName( "bind" );
		g_pDataModel->DestroyElement( pNewBind->GetHandle() );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Callback From m_pFileOpenStateMachine
//-----------------------------------------------------------------------------
bool CSceneViewerPanel::OnWriteFileToDisk(
	const char *pFilename,
	const char *pPassedFileFormat,
	KeyValues *pContextKeyValues )
{
	if ( !m_pRoot )
		return false;

	if ( !Q_strcmp( pContextKeyValues->GetName(), "SaveCurrentAs" ) )
		return SaveCurrentAs( pFilename );

	if ( !Q_stricmp( pPassedFileFormat, "obj" ) )
		return CDmObjSerializer().WriteOBJ( pFilename, m_pRoot, true );

	const int fLen( Q_strlen( pFilename ) );
	if ( fLen > 4 && !Q_stricmp( pFilename + fLen - 4, ".obj" ) )
		return CDmObjSerializer().WriteOBJ( pFilename, m_pRoot, true );

	const char *pEncoding = g_pDataModel->GetDefaultEncoding( pPassedFileFormat );
	if ( pEncoding == NULL || g_pDataModel->FindSerializer( pEncoding ) == NULL )
	{
		// I'd like a better way to figure out what the 'default' format should be
		pEncoding = "binary";
	}
	bool retVal = g_pDataModel->SaveToFile( pFilename, NULL, pEncoding, pPassedFileFormat, m_pRoot ); 
	if ( !retVal || !g_pFullFileSystem->FileExists( pFilename ) )
	{
		char pBuf[1024];
		Q_snprintf( pBuf, sizeof(pBuf), "DMX Write Failed!\n\nCouldn't Save \"%s\"\n\nAs DMX Format \"%s\"", pFilename, pPassedFileFormat );
		vgui::MessageBox *pMessageBox = new vgui::MessageBox( "DMX Write Failed", pBuf, GetParent() );
		pMessageBox->AddActionSignalTarget( this );
		pMessageBox->SetOKButtonVisible( true );
		pMessageBox->SetOKButtonText( "Ok" );
		pMessageBox->DoModal();
		retVal = false;
	}

	return retVal;
}


//-----------------------------------------------------------------------------
// Removes all references to Dme stuff from the vgui elements
//-----------------------------------------------------------------------------
void CSceneViewerPanel::Clear()
{
	if ( m_pClipViewPanel )
	{
		m_pClipViewPanel->SetScene( NULL );
		m_pClipViewPanel->SetAnimationList( NULL );
		m_pClipViewPanel->SetVertexAnimationList( NULL );
		m_pClipViewPanel->SetCombinationOperator( NULL );
	}

	if ( m_pCombinationEditor )
	{
		m_pCombinationEditor->SetCombinationOperator( NULL );
	}

	if ( m_pNerdEditor )
	{
		m_pNerdEditor->SetObject( NULL );
	}

	// Unload Any Old Model
	if ( m_pRoot )
	{
		CDisableUndoScopeGuard guard;
		g_pDataModel->RemoveFileId( m_pRoot->GetFileId() );
	}

	m_pRoot = NULL;

	m_filename = "";
}


//-----------------------------------------------------------------------------
// Import combination rules from this operator
//-----------------------------------------------------------------------------
void GetComboVals( CDmeCombinationOperator *pComboOp, CUtlStringMap< Vector > &controlValues )
{
	controlValues.Clear();

	const int nControls = pComboOp->GetControlCount();
	for ( int i = 0; i < nControls; ++i )
	{
		Vector &cVal = controlValues[ pComboOp->GetControlName( i ) ];

		const Vector2D &sVal = pComboOp->GetStereoControlValue( i, COMBO_CONTROL_NORMAL );
		cVal.x = sVal.x;
		cVal.y = sVal.y;
		cVal.z = pComboOp->GetMultiControlLevel( i, COMBO_CONTROL_NORMAL );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void SetComboVals( CDmeCombinationOperator *pComboOp, CUtlStringMap< Vector > &controlValues )
{
	const int nControls = pComboOp->GetControlCount();
	for ( int i = 0; i < nControls; ++i )
	{
		if ( !controlValues.Defined( pComboOp->GetControlName( i ) ) )
			continue;

		const Vector &cVal = controlValues[ pComboOp->GetControlName( i ) ];
		pComboOp->SetControlValue( i, cVal.AsVector2D(), COMBO_CONTROL_NORMAL );
		pComboOp->SetMultiControlLevel( i, cVal.z, COMBO_CONTROL_NORMAL );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::SendFrameToDagRenderPanel( vgui::Panel *pPanel )
{
	const int nChildren = pPanel->GetChildCount();
	for ( int i = 0; i < nChildren; ++i )
	{
		vgui::Panel *pChild = pPanel->GetChild( i );
		CDmeDagRenderPanel *pDagRenderPanel = dynamic_cast< CDmeDagRenderPanel * >( pChild );
		if ( pDagRenderPanel )
		{
			KeyValues *pKV( new KeyValues( "Frame" ) );
			pPanel->PostMessage( pDagRenderPanel, pKV );
			return;
		}
		SendFrameToDagRenderPanel( pChild );
	}
}


//-----------------------------------------------------------------------------
// Load a Dme file from disk ( or OBJ )
//-----------------------------------------------------------------------------
bool CSceneViewerPanel::Load( const char *pFilename, CUtlStringMap< Vector > *pOldComboVals )
{
	CDisableUndoScopeGuard guard;

	// Remove any old data
	Clear();

	CDmElement *pRoot( NULL );

	const int fLen( Q_strlen( pFilename ) );
	if ( fLen > 4 && !Q_stricmp( pFilename + fLen - 4, ".obj" ) )
	{
		pRoot = CDmObjSerializer().ReadOBJ( pFilename );
	}
	else {
		// Load the Dme file from disk
		g_pDataModel->RestoreFromFile( pFilename, NULL, NULL, &pRoot );
	}

	if ( !pRoot )
		return false;

	if ( pOldComboVals )
	{
		CDmeCombinationOperator *pComboOp = pRoot->GetValueElement< CDmeCombinationOperator >( "combinationOperator" );
		if ( pComboOp )
		{
			SetComboVals( pComboOp, *pOldComboVals );
		}
	}

	m_filename = pFilename;
	m_pRoot = pRoot;

	SetScene();
	return true;
}


//-----------------------------------------------------------------------------
// Reloads a DMX scene from disk, hopefully leaving the GUI intact!
//-----------------------------------------------------------------------------
bool CSceneViewerPanel::Reload()
{
	if ( m_filename.IsEmpty() )
	{
		Error( "ERROR: Reload() Failed - No File loaded\n" );
		return false;
	}

	Msg( "Reload( \"%s\" )\n", m_filename.Get() );
	CDmeCombinationOperator *pComboOp = m_pRoot->GetValueElement< CDmeCombinationOperator >( "combinationOperator" );
	CUtlStringMap< Vector > oldComboVals;
	if ( pComboOp )
	{
		GetComboVals( pComboOp, oldComboVals );
	}

	CUtlString tmpFilename( m_filename );
	const bool retVal = Load( tmpFilename, oldComboVals.GetNumStrings() > 0 ? &oldComboVals : NULL );

	return retVal;
}


//-----------------------------------------------------------------------------
// Sets all of the various editors to reference the right stuff
//-----------------------------------------------------------------------------
void CSceneViewerPanel::SetScene()
{
	CDmeDag *pDag = m_pRoot->GetValueElement< CDmeModel >( "model" );
	if ( !pDag )
	{
		pDag = m_pRoot->GetValueElement< CDmeDag >( "skeleton" );
	}
	m_pClipViewPanel->SetScene( pDag );
	m_pClipViewPanel->SetAnimationList( m_pRoot->GetValueElement< CDmeAnimationList >( "animationList" ) );
	m_pClipViewPanel->SetVertexAnimationList( m_pRoot->GetValueElement< CDmeAnimationList >( "vertexAnimationList" ) );

	CDmeCombinationOperator *pComboOp = m_pRoot->GetValueElement< CDmeCombinationOperator >( "combinationOperator" );
	m_pClipViewPanel->SetCombinationOperator( pComboOp );
	m_pCombinationEditor->SetCombinationOperator( pComboOp );

	if ( m_pNerdEditor )
	{
		m_pNerdEditor->SetObject( m_pRoot );
	}

	if ( pComboOp && pComboOp->GetControlCount() != 0 )
	{
		// TODO: Show the right TAB...

		// Generate wrinkle data only if it doesn't already exist
		pComboOp->GenerateWrinkleDeltas( false );
	}

	SendFrameToDagRenderPanel( m_pClipViewPanel );
}


//-----------------------------------------------------------------------------
// Data for a cube
//-----------------------------------------------------------------------------
static Vector g_pPosition[8] = 
{
	Vector( -10.0f, -10.0f, -10.0f ),
	Vector(  10.0f, -10.0f, -10.0f ),
	Vector( -10.0f,  10.0f, -10.0f ),
	Vector(  10.0f,  10.0f, -10.0f ),
	Vector( -10.0f, -10.0f,  10.0f ),
	Vector(  10.0f, -10.0f,  10.0f ),
	Vector( -10.0f,  10.0f,  10.0f ),
	Vector(  10.0f,  10.0f,  10.0f ),
};

static float g_pBalance[8] = 
{
	0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f
};

static float g_pSpeed[8] = 
{
	0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f
};

static int g_pBoneIndices[] = 
{
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	1, 0, 0,
	1, 0, 0,
	1, 0, 0,
	1, 0, 0,
};

static float g_pBoneWeights[] = 
{
	1.0f, 0.0f, 0.0f, 
	1.0f, 0.0f, 0.0f,
	1.0f, 0.0f, 0.0f, 
	1.0f, 0.0f, 0.0f,
	1.0f, 0.0f, 0.0f, 
	1.0f, 0.0f, 0.0f,
	1.0f, 0.0f, 0.0f, 
	1.0f, 0.0f, 0.0f,
};

static Vector g_pNormal[24] = 
{
	Vector( -1.0f,  0.0f,  0.0f ),
	Vector( -1.0f,  0.0f,  0.0f ),
	Vector( -1.0f,  0.0f,  0.0f ),
	Vector( -1.0f,  0.0f,  0.0f ),
	Vector(  1.0f,  0.0f,  0.0f ),
	Vector(  1.0f,  0.0f,  0.0f ),
	Vector(  1.0f,  0.0f,  0.0f ),
	Vector(  1.0f,  0.0f,  0.0f ),
	Vector(  0.0f, -1.0f,  0.0f ),
	Vector(  0.0f, -1.0f,  0.0f ),
	Vector(  0.0f, -1.0f,  0.0f ),
	Vector(  0.0f, -1.0f,  0.0f ),
	Vector(  0.0f,  1.0f,  0.0f ),
	Vector(  0.0f,  1.0f,  0.0f ),
	Vector(  0.0f,  1.0f,  0.0f ),
	Vector(  0.0f,  1.0f,  0.0f ),
	Vector(  0.0f,  0.0f, -1.0f ),
	Vector(  0.0f,  0.0f, -1.0f ),
	Vector(  0.0f,  0.0f, -1.0f ),
	Vector(  0.0f,  0.0f, -1.0f ),
	Vector(  0.0f,  0.0f,  1.0f ),
	Vector(  0.0f,  0.0f,  1.0f ),
	Vector(  0.0f,  0.0f,  1.0f ),
	Vector(  0.0f,  0.0f,  1.0f ),
};

static Vector2D g_pUV[4] = 
{
	Vector2D( 1.0f,  1.0f ),
	Vector2D( 0.0f,  1.0f ),
	Vector2D( 0.0f,  0.0f ),
	Vector2D( 1.0f,  0.0f ),
};

static Color g_pColor[8] = 
{
	Color(   0,   0,   0, 255 ),
	Color( 255,   0,   0, 255 ),
	Color(   0, 255,   0, 255 ),
	Color( 255, 255,   0, 255 ),
	Color(   0,   0, 255, 255 ),
	Color( 255,   0, 255, 255 ),
	Color(   0, 255, 255, 255 ),
	Color( 255, 255, 255, 255 ),
};

static int g_pPositionIndices[24] = 
{
	0, 4, 6, 2, // -x face
	1, 3, 7, 5,	// +x face
	0, 1, 5, 4,	// -y face
	2, 6, 7, 3,	// +y face
	0, 2, 3, 1,	// -z face
	4, 5, 7, 6,	// +z face
};

static int g_pBalanceIndices[24] = 
{
	0, 4, 6, 2, // -x face
	1, 3, 7, 5,	// +x face
	0, 1, 5, 4,	// -y face
	2, 6, 7, 3,	// +y face
	0, 2, 3, 1,	// -z face
	4, 5, 7, 6,	// +z face
};

static int g_pSpeedIndices[24] = 
{
	0, 4, 6, 2, // -x face
	1, 3, 7, 5,	// +x face
	0, 1, 5, 4,	// -y face
	2, 6, 7, 3,	// +y face
	0, 2, 3, 1,	// -z face
	4, 5, 7, 6,	// +z face
};

static int g_pNormalIndices[24] = 
{
	 0,  1,  2,  3,
	 4,  5,  6,  7,
	 8,  9, 10, 11,
	12, 13, 14, 15,
	16, 17, 18, 19,
	20, 21, 22, 23,
};

static int g_pUVIndices[24] = 
{
	0, 1, 2, 3,
	0, 1, 2, 3,
	0, 1, 2, 3,
	0, 1, 2, 3,
	0, 1, 2, 3,
	0, 1, 2, 3,
};

static int g_pColorIndices[24] = 
{
	6, 6, 6, 6, // -x face
	1, 1, 1, 1,	// +x face
	2, 2, 2, 2,	// -y face
	3, 3, 3, 3,	// +y face
	4, 4, 4, 4,	// -z face
	5, 5, 5, 5,	// +z face
};

static int g_pFaceIndices[] = 
{
	 0,  1,  2,  3, -1,
	 4,  5,  6,  7, -1,
	 8,  9, 10, 11, -1,
	12, 13, 14, 15, -1,
	16, 17, 18, 19, -1,
	20, 21, 22, 23, -1,
};


static Vector g_pPositionDelta[] = 
{
	Vector(  20.0f,  0.0f,  0.0f ),
	Vector(  20.0f,  0.0f,  0.0f ),
	Vector(  20.0f,  0.0f,  0.0f ),
	Vector(  20.0f,  0.0f,  0.0f ),
};

static int g_pPositionDeltaIndices[] = 
{
	1, 3, 5, 7
};

static Vector g_pPositionDelta1a[] = 
{
	Vector(  -10.0f,  0.0f,  0.0f ),
	Vector(  -10.0f,  0.0f,  0.0f ),
	Vector(  -10.0f,  0.0f,  0.0f ),
	Vector(  -10.0f,  0.0f,  0.0f ),
};

static int g_pPositionDeltaIndices1a[] = 
{
	1, 3, 5, 7
};


static Vector g_pPositionDelta2a[] = 
{
	Vector(  0.0f,  0.0f,  20.0f ),
	Vector(  0.0f,  0.0f,  20.0f ),
};

static int g_pPositionDeltaIndices2a[] = 
{
	4, 6
};

static Vector g_pPositionDelta2[] = 
{
	Vector(  0.0f,  0.0f,  20.0f ),
	Vector(  0.0f,  0.0f,  20.0f ),
	Vector(  0.0f,  0.0f,  20.0f ),
	Vector(  0.0f,  0.0f,  20.0f ),
};

static int g_pPositionDeltaIndices2[] = 
{
	4, 5, 6, 7
};

static Vector g_pPositionDelta2c[] = 
{
	Vector(  0.0f,  0.0f,  20.0f ),
	Vector(  0.0f,  0.0f,  20.0f ),
};

static int g_pPositionDeltaIndices2c[] = 
{
	5, 7
};

static Vector g_pPositionDelta12[] = 
{
	Vector( -20.0f,  0.0f, -20.0f ),
	Vector( -20.0f,  0.0f, -20.0f ),
};

static int g_pPositionDeltaIndices12[] = 
{
	5, 7
};

static Vector g_pPositionDelta3[] = 
{
	Vector( 0.0f,  20.0f, 0.0f ),
	Vector( 0.0f,  20.0f, 0.0f ),
	Vector( 0.0f,  20.0f, 0.0f ),
	Vector( 0.0f,  20.0f, 0.0f ),
	Vector( 0.0f,  20.0f, 0.0f ),
	Vector( 0.0f,  20.0f, 0.0f ),
	Vector( 0.0f,  20.0f, 0.0f ),
	Vector( 0.0f,  20.0f, 0.0f ),
};

static int g_pPositionDeltaIndices3[] = 
{
	0, 1, 2, 3, 4, 5, 6, 7
};

//-----------------------------------------------------------------------------
// Sets up a new mesh dag
//-----------------------------------------------------------------------------
CDmeModel *CSceneViewerPanel::CreateNewMeshDag( CDmeMesh **ppMesh, DmFileId_t fileid )
{
	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", "effects/laser1" );
	pVMTKeyValues->SetInt( "$additive", 1 );
	pVMTKeyValues->SetInt( "$translucent", 1 );
	pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	pVMTKeyValues->SetInt( "$model", 1 );
	g_pMaterialSystem->CreateMaterial( "__sceneViewerTest", pVMTKeyValues );

	CDmeModel *pModel = CreateElement< CDmeModel >( "New Model", fileid );
	pModel->AddJoint( "joint0" );
	pModel->AddJoint( "joint1" );

	CDmeDag *pMeshDag = CreateElement< CDmeDag >( "New Mesh Dag", fileid );
	CDmeMesh *pMesh = CreateElement< CDmeMesh >( "New Mesh", fileid );
	CDmeVertexData *pVertexData = pMesh->FindOrCreateBaseState( "bind" );
	CDmeFaceSet *pFaceSet = CreateElement< CDmeFaceSet >( "New Face Set", fileid );
	CDmeMaterial *pMaterial = CreateElement< CDmeMaterial >( "New Material", fileid );
	pMaterial->SetMaterial( "__sceneViewerTest" );
	pMesh->SetCurrentBaseState( "bind" );

	int nPerVertexBoneCount = 3;
	FieldIndex_t jointWeight, jointIndex;
	FieldIndex_t pos = pVertexData->CreateField( CDmeVertexData::FIELD_POSITION );
	FieldIndex_t norm = pVertexData->CreateField( CDmeVertexData::FIELD_NORMAL );
	FieldIndex_t uv = pVertexData->CreateField( CDmeVertexData::FIELD_TEXCOORD );
	FieldIndex_t color = pVertexData->CreateField( CDmeVertexData::FIELD_COLOR );
	FieldIndex_t balance = pVertexData->CreateField( CDmeVertexData::FIELD_BALANCE );
	FieldIndex_t speed = pVertexData->CreateField( CDmeVertexData::FIELD_MORPH_SPEED );
	pVertexData->CreateJointWeightsAndIndices( nPerVertexBoneCount, &jointWeight, &jointIndex );

	int nPosCount = ARRAYSIZE(g_pPosition);
	int nNormCount = ARRAYSIZE(g_pNormal);
	int nUVCount = ARRAYSIZE(g_pUV);
	int nColorCount = ARRAYSIZE(g_pColor);
	int nBalanceCount = ARRAYSIZE(g_pBalance);
	int nSpeedCount = ARRAYSIZE(g_pSpeed);
	int nBoneIndices = ARRAYSIZE(g_pBoneIndices);
	int nBoneWeights = ARRAYSIZE(g_pBoneWeights);
	Assert( nBoneIndices == nPerVertexBoneCount * nPosCount );
	Assert( nBoneWeights == nPerVertexBoneCount * nPosCount );

	pVertexData->AddVertexData( pos, nPosCount );
	pVertexData->SetVertexData( pos, 0, nPosCount, AT_VECTOR3, g_pPosition );

	pVertexData->AddVertexData( jointIndex, nBoneIndices );
	pVertexData->SetVertexData( jointIndex, 0, nBoneIndices, AT_INT, g_pBoneIndices );

	pVertexData->AddVertexData( jointWeight, nBoneWeights );
	pVertexData->SetVertexData( jointWeight, 0, nBoneWeights, AT_FLOAT, g_pBoneWeights );

	pVertexData->AddVertexData( norm, nNormCount );
	pVertexData->SetVertexData( norm, 0, nNormCount, AT_VECTOR3, g_pNormal );
	 
	pVertexData->AddVertexData( uv, nUVCount );
	pVertexData->SetVertexData( uv, 0, nUVCount, AT_VECTOR2, g_pUV );

	pVertexData->AddVertexData( color, nColorCount );
	pVertexData->SetVertexData( color, 0, nColorCount, AT_COLOR, g_pColor );

	pVertexData->AddVertexData( balance, nBalanceCount );
	pVertexData->SetVertexData( balance, 0, nBalanceCount, AT_FLOAT, g_pBalance );

	pVertexData->AddVertexData( speed, nSpeedCount );
	pVertexData->SetVertexData( speed, 0, nSpeedCount, AT_FLOAT, g_pSpeed );

	int nIndexCount = ARRAYSIZE(g_pPositionIndices);
	Assert( nIndexCount == ARRAYSIZE(g_pNormalIndices) );
	Assert( nIndexCount == ARRAYSIZE(g_pUVIndices) );
	Assert( nIndexCount == ARRAYSIZE(g_pColorIndices) );
	Assert( nIndexCount == ARRAYSIZE(g_pBalanceIndices) );
	Assert( nIndexCount == ARRAYSIZE(g_pSpeedIndices) );

	pVertexData->AddVertexIndices( nIndexCount );
	pVertexData->SetVertexIndices( pos, 0, nIndexCount, g_pPositionIndices );
	pVertexData->SetVertexIndices( norm, 0, nIndexCount, g_pNormalIndices );
	pVertexData->SetVertexIndices( uv, 0, nIndexCount, g_pUVIndices );
	pVertexData->SetVertexIndices( color, 0, nIndexCount, g_pColorIndices );
	pVertexData->SetVertexIndices( balance, 0, nIndexCount, g_pBalanceIndices );
	pVertexData->SetVertexIndices( speed, 0, nIndexCount, g_pSpeedIndices );

	int nFaceIndexCount = ARRAYSIZE(g_pFaceIndices);
	pFaceSet->AddIndices( nFaceIndexCount );
	pFaceSet->SetIndices( 0, nFaceIndexCount, g_pFaceIndices );
	pFaceSet->SetMaterial( pMaterial );

	CDmeVertexDeltaData *pDeltaData = pMesh->FindOrCreateDeltaState( "delta1" );
	FieldIndex_t deltaPos = pDeltaData->CreateField( CDmeVertexDeltaData::FIELD_POSITION );
	int nDeltaPosCount = ARRAYSIZE( g_pPositionDelta );

	pDeltaData->AddVertexData( deltaPos, nDeltaPosCount );
	pDeltaData->SetVertexData( deltaPos, 0, nDeltaPosCount, AT_VECTOR3, g_pPositionDelta );
	pDeltaData->SetVertexIndices( deltaPos, 0, nDeltaPosCount, g_pPositionDeltaIndices );

	pDeltaData = pMesh->FindOrCreateDeltaState( "delta1a" );
	deltaPos = pDeltaData->CreateField( CDmeVertexDeltaData::FIELD_POSITION );
	nDeltaPosCount = ARRAYSIZE( g_pPositionDelta1a );

	pDeltaData->AddVertexData( deltaPos, nDeltaPosCount );
	pDeltaData->SetVertexData( deltaPos, 0, nDeltaPosCount, AT_VECTOR3, g_pPositionDelta1a );
	pDeltaData->SetVertexIndices( deltaPos, 0, nDeltaPosCount, g_pPositionDeltaIndices1a );

	CDmeVertexDeltaData *pDeltaData2 = pMesh->FindOrCreateDeltaState( "delta2" );
	FieldIndex_t deltaPos2 = pDeltaData2->CreateField( CDmeVertexDeltaData::FIELD_POSITION );
	int nDeltaPosCount2 = ARRAYSIZE( g_pPositionDelta2 );

	pDeltaData2->AddVertexData( deltaPos2, nDeltaPosCount2 );
	pDeltaData2->SetVertexData( deltaPos2, 0, nDeltaPosCount2, AT_VECTOR3, g_pPositionDelta2 );
	pDeltaData2->SetVertexIndices( deltaPos2, 0, nDeltaPosCount2, g_pPositionDeltaIndices2 );

	pDeltaData2 = pMesh->FindOrCreateDeltaState( "delta2a" );
	deltaPos2 = pDeltaData2->CreateField( CDmeVertexDeltaData::FIELD_POSITION );
	nDeltaPosCount2 = ARRAYSIZE( g_pPositionDelta2a );

	pDeltaData2->AddVertexData( deltaPos2, nDeltaPosCount2 );
	pDeltaData2->SetVertexData( deltaPos2, 0, nDeltaPosCount2, AT_VECTOR3, g_pPositionDelta2a );
	pDeltaData2->SetVertexIndices( deltaPos2, 0, nDeltaPosCount2, g_pPositionDeltaIndices2a );

	pDeltaData2 = pMesh->FindOrCreateDeltaState( "delta2c" );
	deltaPos2 = pDeltaData2->CreateField( CDmeVertexDeltaData::FIELD_POSITION );
	nDeltaPosCount2 = ARRAYSIZE( g_pPositionDelta2c );

	pDeltaData2->AddVertexData( deltaPos2, nDeltaPosCount2 );
	pDeltaData2->SetVertexData( deltaPos2, 0, nDeltaPosCount2, AT_VECTOR3, g_pPositionDelta2c );
	pDeltaData2->SetVertexIndices( deltaPos2, 0, nDeltaPosCount2, g_pPositionDeltaIndices2c );

	CDmeVertexDeltaData *pDeltaData12 = pMesh->FindOrCreateDeltaState( "delta1_delta2" );
	FieldIndex_t deltaPos12 = pDeltaData12->CreateField( CDmeVertexDeltaData::FIELD_POSITION );
	int nDeltaPosCount12 = ARRAYSIZE( g_pPositionDelta12 );

	pDeltaData12->AddVertexData( deltaPos12, nDeltaPosCount12 );
	pDeltaData12->SetVertexData( deltaPos12, 0, nDeltaPosCount12, AT_VECTOR3, g_pPositionDelta12 );
	pDeltaData12->SetVertexIndices( deltaPos12, 0, nDeltaPosCount12, g_pPositionDeltaIndices12 );

	CDmeVertexDeltaData *pDeltaData3 = pMesh->FindOrCreateDeltaState( "delta3" );
	FieldIndex_t deltaPos3 = pDeltaData3->CreateField( CDmeVertexDeltaData::FIELD_POSITION );
	int nDeltaPosCount3 = ARRAYSIZE( g_pPositionDelta3 );

	pDeltaData3->AddVertexData( deltaPos3, nDeltaPosCount3 );
	pDeltaData3->SetVertexData( deltaPos3, 0, nDeltaPosCount3, AT_VECTOR3, g_pPositionDelta3 );
	pDeltaData3->SetVertexIndices( deltaPos3, 0, nDeltaPosCount3, g_pPositionDeltaIndices3 );

	// Test offset in dag
	matrix3x4_t meshOffset;
	SetIdentityMatrix( meshOffset );
	MatrixSetColumn( Vector( 0, 5, 0 ), 3, meshOffset );
	pMeshDag->GetTransform()->SetTransform( meshOffset );

	pMesh->AddFaceSet( pFaceSet );
	pModel->AddChild( pMeshDag );
	pModel->CaptureJointsToBaseState( "bind" );
	pMeshDag->SetShape( pMesh );

	pMesh->ComputeDeltaStateNormals();

	*ppMesh = pMesh;
	return pModel;
}


//-----------------------------------------------------------------------------
// Sets up a new mesh animation
//-----------------------------------------------------------------------------
CDmeAnimationList *CSceneViewerPanel::CreateNewJointAnimation( CDmeModel *pModel )
{
	CDmeAnimationList *pAnimationList = CreateElement< CDmeAnimationList >( "New Animation List" );
	CDmeChannelsClip *pAnimation = CreateElement< CDmeChannelsClip >( "New Animation" );
	pAnimationList->AddAnimation( pAnimation );

	CDmeChannel *pChannel = CreateElement< CDmeChannel >( "New Channel" );
	pAnimation->m_Channels.AddToTail( pChannel );
	pChannel->SetOutput( pModel->GetJointTransform(1), "position" );

	CDmeVector3Log *pLog = pChannel->CreateLog<Vector>( );
	DmeTime_t time( 0 );
	Vector pos;
	pAnimation->SetStartTime( time );
	for ( int i = 0; i < 30; ++i )
	{
		pos.Random( -20, 20 );
		pLog->SetKey( time, pos );
		time += DmeTime_t( 1.0f );
	}
	pAnimation->SetDuration( time - pAnimation->GetStartTime() );
	return pAnimationList;
}


//-----------------------------------------------------------------------------
// Create random animation
//-----------------------------------------------------------------------------
static void CreateRandomAnimation( CDmeChannelsClip *pAnimation, const char *pName, CDmeCombinationOperator *pComboOp )
{
	CDmeChannel *pChannel = CreateElement< CDmeChannel >( "Vertex Anim Channel" );
	pAnimation->m_Channels.AddToTail( pChannel );

	int nControlIndex = pComboOp->FindControlIndex( pName );
	if ( nControlIndex >= 0 )
	{
		pComboOp->AttachChannelToControlValue( nControlIndex, COMBO_CONTROL_NORMAL, pChannel );
	}

	CDmeVector2Log *pLog = pChannel->CreateLog<Vector2D>( );
	DmeTime_t time( 0 );
	Vector pos;
	for ( int i = 0; i < 30; ++i )
	{
		float flValue = RandomFloat( 0.0f, 1.0f );
		float flValue2 = RandomFloat( 0.0f, 1.0f );
		pLog->SetKey( time, Vector2D( flValue, flValue2 ) );
		time += DmeTime_t( 1.0f );
	}

	CreateLaggedVertexAnimation( pAnimation, 30 );
}


//-----------------------------------------------------------------------------
// Sets up a new vertex animation
//-----------------------------------------------------------------------------
CDmeAnimationList *CSceneViewerPanel::CreateNewVertexAnimation( CDmeMesh *pMesh, CDmeCombinationOperator *pComboOp )
{
	CDmeAnimationList *pAnimationList = CreateElement< CDmeAnimationList >( "New Vertex Animation List" );
	CDmeChannelsClip *pAnimation = CreateElement< CDmeChannelsClip >( "New Vertex Animation" );
	pAnimationList->AddAnimation( pAnimation );

	CreateRandomAnimation( pAnimation, "delta1", pComboOp );
	CreateRandomAnimation( pAnimation, "delta2", pComboOp );
	CreateRandomAnimation( pAnimation, "delta3", pComboOp );

	pAnimation->SetStartTime( DMETIME_ZERO );
	pAnimation->SetDuration( DmeTime_t( 30.0f ) - pAnimation->GetStartTime() );
	return pAnimationList;
}


//-----------------------------------------------------------------------------
// Sets up a new clip
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnNew()
{
	Clear();

	return;

	// This is not undoable...
	CDisableUndoScopeGuard guard;

	Vector vecTranslation;
	matrix3x4_t mat;

	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( "new.dmx" );

	CDmeMesh *pMesh;
	CDmeModel *pModel = CreateNewMeshDag( &pMesh, fileid );

	CDmeCombinationOperator *pComboOp = CreateElement< CDmeCombinationOperator >( "New Combination Operator", fileid );
	ControlIndex_t nIndex = pComboOp->FindOrCreateControl( "delta1", true );
	pComboOp->AddRawControl( nIndex, "delta1" );
	pComboOp->AddRawControl( nIndex, "delta1a" );

	nIndex =  pComboOp->FindOrCreateControl( "delta2", true );
	pComboOp->AddRawControl( nIndex, "delta2a" );
	pComboOp->AddRawControl( nIndex, "delta2" );
	pComboOp->AddRawControl( nIndex, "delta2c" );

	pComboOp->FindOrCreateControl( "delta3", false, true );
	pComboOp->UsingLaggedData( true );
	pComboOp->AddTarget( pModel );

	const char *ppDominators[] = { "delta1", "delta2" };
	const char *ppSuppressors[] = { "delta3" };
	pComboOp->AddDominationRule( ARRAYSIZE(ppDominators), ppDominators, ARRAYSIZE(ppSuppressors), ppSuppressors );
	pComboOp->AddDominationRule( ARRAYSIZE(ppSuppressors), ppSuppressors, ARRAYSIZE(ppDominators), ppDominators );

	CDmeAnimationList *pAnimationList = CreateNewJointAnimation( pModel );
	CDmeAnimationList *pVertexAnimationList = CreateNewVertexAnimation( pMesh, pComboOp );
	m_pClipViewPanel->SetScene( pModel );
	m_pClipViewPanel->SetCombinationOperator( pComboOp );
	m_pClipViewPanel->SetAnimationList( pAnimationList );
	m_pClipViewPanel->SetVertexAnimationList( pVertexAnimationList );

	// NOTE: The root element here is what we expect to see in a file
	CDmElement *pRoot = CreateElement< CDmElement >( "root", fileid );
	pRoot->SetValue( "skeleton", pModel );
	pRoot->SetValue( "model", pModel );
	pRoot->SetValue( "animationList", pAnimationList );
	pRoot->SetValue( "vertexAnimationList", pVertexAnimationList );
	pRoot->SetValue( "combinationOperator", pComboOp );

	m_pCombinationEditor->SetCombinationOperator( pComboOp );

	// Unload any old model
	if ( m_pRoot )
	{
		g_pDataModel->RemoveFileId( m_pRoot->GetFileId() );
	}
	m_pRoot = pRoot;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnOpen()
{
	int nFlags = 0;
	const char *pFileName = NULL;
	if ( m_pRoot )
	{
		nFlags = vgui::FOSM_SHOW_PERFORCE_DIALOGS;
		pFileName = g_pDataModel->GetFileName( m_pRoot->GetFileId() );
	}

	KeyValues *pContextKeyValues = new KeyValues( "FileOpen" );
	m_pFileOpenStateMachine->OpenFile( DEFAULT_FILE_FORMAT, pContextKeyValues, pFileName, NULL, nFlags );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnSave()
{
	if ( !m_pRoot )
	{
		OnSaveAs();
		return;
	}

	KeyValues *pContextKeyValues = new KeyValues( "FileSave" );
	m_pFileOpenStateMachine->SaveFile(
		pContextKeyValues,
		g_pDataModel->GetFileName( m_pRoot->GetFileId() ),
		DEFAULT_FILE_FORMAT,
		vgui::FOSM_SHOW_PERFORCE_DIALOGS );
}


//-----------------------------------------------------------------------------
// Save As Dialog Box
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnSaveAs()
{
	if ( !m_pRoot )
	{
		vgui::MessageBox *pError = new vgui::MessageBox(
			"#SceneViewer_NothingToSave",
			"#SceneViewer_NothingToSave", this );
		pError->DoModal();
		return;
	}

	KeyValues *pContextKeyValues = new KeyValues( "FileSave" );
	m_pFileOpenStateMachine->SaveFile( pContextKeyValues, NULL, DEFAULT_FILE_FORMAT, vgui::FOSM_SHOW_PERFORCE_DIALOGS );
}


//-----------------------------------------------------------------------------
// Save Current As Dialog Box
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnSaveCurrentAs()
{
	if ( !m_pRoot )
	{
		vgui::MessageBox *pError = new vgui::MessageBox(
			"#SceneViewer_NothingToSave",
			"#SceneViewer_NothingToSave", this );
		pError->DoModal();
		return;
	}

	KeyValues *pContextKeyValues = new KeyValues( "SaveCurrentAs" );
	m_pFileOpenStateMachine->SaveFile( pContextKeyValues, NULL, DEFAULT_FILE_FORMAT, vgui::FOSM_SHOW_PERFORCE_DIALOGS );
}


//-----------------------------------------------------------------------------
// Exit Application
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnExit()
{
	vgui::ivgui()->Stop();
}


//-----------------------------------------------------------------------------
// Save As Dialog Box
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnDescribeUndoStack()
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
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnSizeChanged( int /* newWidth */, int /* newHeight */ )
{
	if ( m_pClipViewPanel->GetAutoResize() != AUTORESIZE_NO )
	{
		OnPinAndZoomIt();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnPinAndZoomIt()
{
	int mx, my;
	m_pMenuBar->GetPos( mx, my );
	int mw, mh;
	m_pMenuBar->GetSize( mw, mh );

	m_pClipViewPanel->SetPinCorner( PIN_TOPLEFT, 0, my + mh + 2 );
	m_pClipViewPanel->SetAutoResize( PIN_TOPLEFT, AUTORESIZE_DOWNANDRIGHT, 0, my + mh + 2, 0, 0 );

	int w, h;
	this->GetSize( w, h );
	m_pClipViewPanel->SetBounds( 0, my + mh + 2, w, h - ( my + mh + 2 ) );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnLoadFile( const char *fullpath )
{
	Load( fullpath );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnUndo()
{
	g_pDataModel->Undo();
	if ( m_hProperties.Get() )
	{
		m_hProperties->Refresh();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnRedo()
{
	g_pDataModel->Redo();
	if ( m_hProperties.Get() )
	{
		m_hProperties->Refresh();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnEdit()
{
	if ( m_hProperties.Get() )
	{
		m_hProperties.Get()->MoveToFront();
		return;
	}

	if ( m_hProperties.Get() )
	{
		delete m_hProperties.Get();
	}

	m_hProperties = new CElementPropertiesTree( this, NULL, GetScene() );

	if ( m_hProperties.Get() )
	{
		m_hProperties->Init();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnShow3DView()
{
	if ( m_pClipViewPanel )
	{
		m_pClipViewPanel->SetVisible( true );
		m_pClipViewPanel->MoveToFront();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnHide3DView()
{
	if ( m_pClipViewPanel )
	{
		m_pClipViewPanel->SetVisible( false );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnShowComboEditor()
{
	if ( m_pCombinationEditor )
	{
		m_pCombinationEditor->SetVisible( true );
		m_pCombinationEditor->MoveToFront();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnHideComboEditor()
{
	if ( m_pCombinationEditor )
	{
		m_pCombinationEditor->SetVisible( false );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnShowAssetBuilder()
{
	if ( m_pAssetBuilder )
	{
		m_pAssetBuilder->SetVisible( true );
		m_pAssetBuilder->MoveToFront();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnHideAssetBuilder()
{
	if ( m_pAssetBuilder )
	{
		m_pAssetBuilder->SetVisible( false );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnShowConsole()
{
	if ( !m_pAssetBuilder )
		return;

	m_pConsole->SetVisible( true );
	m_pConsole->MoveToFront();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnHideConsole()
{
	if ( !m_pConsole )
		return;

	m_pConsole->SetVisible( false );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnShowNerdEditor()
{
	if ( m_pNerdEditor )
	{
		m_pNerdEditor->SetVisible( true );
		m_pNerdEditor->MoveToFront();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnHideNerdEditor()
{
	if ( m_pNerdEditor )
	{
		m_pNerdEditor->SetVisible( false );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::OnCombinationOperatorChanged()
{
	if ( m_pClipViewPanel )
	{
		m_pClipViewPanel->RefreshCombinationOperator();
	}
}


//-----------------------------------------------------------------------------
// The editor panel should always fill the space...
//-----------------------------------------------------------------------------
void CSceneViewerPanel::PerformLayout()
{
	// Make the editor panel fill the space
	int iWidth, iHeight;

	vgui::VPANEL parent = GetParent() ? GetParent()->GetVPanel() : vgui::surface()->GetEmbeddedPanel(); 
	vgui::ipanel()->GetSize( parent, iWidth, iHeight );
	SetSize( iWidth, iHeight );
	m_pMenuBar->SetSize( iWidth, 28 );

	// Make the client area also fill the space not used by the menu bar
	int iTemp, iMenuHeight;
	m_pMenuBar->GetSize( iTemp, iMenuHeight );
	m_pClientArea->SetPos( 0, iMenuHeight );
	m_pClientArea->SetSize( iWidth, iHeight - iMenuHeight );

	if ( !m_bConsolePositioned )
	{
		m_pConsole->SetSize( iWidth / 2, iHeight / 2 );
		m_pConsole->MoveToCenterOfScreen();
		m_bConsolePositioned = true;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSceneViewerPanel::NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	// Do nothing for now... need to add IDmNotify support from DagRenderPanel
}
