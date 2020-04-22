//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Standard file menu
//
//=============================================================================

#include "toolutils/toolfilemenubutton.h"
#include "toolutils/toolmenubutton.h"
#include "tier1/KeyValues.h"
#include "tier1/utlstring.h"
#include "vgui_controls/menu.h"
#include "vgui_controls/frame.h"
#include "vgui_controls/button.h"
#include "vgui_controls/listpanel.h"
#include "toolutils/enginetools_int.h"
#include "p4lib/ip4.h"
#include "vgui_controls/perforcefilelistframe.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Global function to create the file menu
//-----------------------------------------------------------------------------
CToolMenuButton* CreateToolFileMenuButton( vgui::Panel *parent, const char *panelName, 
	const char *text, vgui::Panel *pActionTarget, IFileMenuCallbacks *pCallbacks )
{
	return new CToolFileMenuButton( parent, panelName, text, pActionTarget, pCallbacks );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CToolFileMenuButton::CToolFileMenuButton( vgui::Panel *pParent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget, IFileMenuCallbacks *pFileMenuCallback ) :
	BaseClass( pParent, panelName, text, pActionSignalTarget ), m_pFileMenuCallback( pFileMenuCallback )
{
	Assert( pFileMenuCallback );

	AddMenuItem( "new", "#ToolFileNew", new KeyValues( "OnNew" ), pActionSignalTarget, NULL, "file_new" );
	AddMenuItem( "open", "#ToolFileOpen", new KeyValues( "OnOpen" ), pActionSignalTarget, NULL, "file_open"  );
	AddMenuItem( "save", "#ToolFileSave", new KeyValues( "OnSave" ), pActionSignalTarget, NULL, "file_save"  );
	AddMenuItem( "saveas", "#ToolFileSaveAs", new KeyValues( "OnSaveAs" ), pActionSignalTarget  );
	AddMenuItem( "close", "#ToolFileClose", new KeyValues( "OnClose" ), pActionSignalTarget  );
 	AddSeparator();
	
	// Add the Perforce menu options only if there is a valid P4 interface (SDK users won't have this)
	if ( p4 )
	{
		m_pPerforce = new vgui::Menu( this, "Perforce" );
		m_pMenu->AddCascadingMenuItem( "#ToolPerforce", this, m_pPerforce );
		m_nPerforceAdd = m_pPerforce->AddMenuItem( "perforce_add", "#ToolPerforceAdd", new KeyValues( "OnPerforceAdd" ), this );
		m_nPerforceOpen = m_pPerforce->AddMenuItem( "perforce_open", "#ToolPerforceOpen", new KeyValues( "OnPerforceOpen" ), this );
		m_nPerforceRevert = m_pPerforce->AddMenuItem( "perforce_revert", "#ToolPerforceRevert", new KeyValues( "OnPerforceRevert" ), this );
		m_nPerforceSubmit = m_pPerforce->AddMenuItem( "perforce_submit", "#ToolPerforceSubmit", new KeyValues( "OnPerforceSubmit" ), this );
		m_pPerforce->AddSeparator();
		m_nPerforceP4Win = m_pPerforce->AddMenuItem( "perforce_p4win", "#ToolPerforceP4Win", new KeyValues( "OnPerforceP4Win" ), this );
		m_nPerforceListOpenFiles = m_pPerforce->AddMenuItem( "perforce_listopenfiles", "#ToolPerforceListOpenFiles", new KeyValues( "OnPerforceListOpenFiles" ), this );
	}

	m_pRecentFiles = new vgui::Menu( this, "RecentFiles" );
	m_nRecentFiles = m_pMenu->AddCascadingMenuItem( "#ToolFileRecent", pActionSignalTarget, m_pRecentFiles );

	AddMenuItem( "clearrecent", "#ToolFileClearRecent", new KeyValues ( "OnClearRecent" ), pActionSignalTarget );
	AddSeparator();
	AddMenuItem( "exit", "#ToolFileExit", new KeyValues ( "OnExit" ), pActionSignalTarget );

	SetMenu( m_pMenu );
}


//-----------------------------------------------------------------------------
// Gets called when the menu is shown
//-----------------------------------------------------------------------------
void CToolFileMenuButton::OnShowMenu( vgui::Menu *menu )
{
	BaseClass::OnShowMenu( menu );

	// Update the menu
	int nEnableMask = m_pFileMenuCallback->GetFileMenuItemsEnabled();

	int id = m_Items.Find( "new" );
	SetItemEnabled( id, (nEnableMask & IFileMenuCallbacks::FILE_NEW) != 0 );
	id = m_Items.Find( "open" );
	SetItemEnabled( id, (nEnableMask & IFileMenuCallbacks::FILE_OPEN) != 0 );
	id = m_Items.Find( "save" );
	SetItemEnabled( id, (nEnableMask & IFileMenuCallbacks::FILE_SAVE) != 0 );
	id = m_Items.Find( "saveas" );
	SetItemEnabled( id, (nEnableMask & IFileMenuCallbacks::FILE_SAVEAS) != 0 );
	id = m_Items.Find( "close" );
	SetItemEnabled( id, (nEnableMask & IFileMenuCallbacks::FILE_CLOSE) != 0 );
	id = m_Items.Find( "clearrecent" );
	SetItemEnabled( id, (nEnableMask & IFileMenuCallbacks::FILE_CLEAR_RECENT) != 0 );

	m_pRecentFiles->DeleteAllItems();

	if ( (nEnableMask & IFileMenuCallbacks::FILE_RECENT) == 0 )
	{
		m_pMenu->SetItemEnabled( m_nRecentFiles, false );
	}
	else
	{
		m_pMenu->SetItemEnabled( m_nRecentFiles, true );
		m_pFileMenuCallback->AddRecentFilesToMenu( m_pRecentFiles );
	}

	// We only have the Perforce menu items if we have valid p4 interface
	if ( p4 )
	{
		bool bP4Connected = p4->IsConnectedToServer();
		char pPerforceFile[MAX_PATH];
		if ( bP4Connected && m_pFileMenuCallback->GetPerforceFileName( pPerforceFile, sizeof(pPerforceFile) ) )
		{
			bool bIsUnnamed = !Q_IsAbsolutePath( pPerforceFile );
			bool bOpenedForEdit = p4->GetFileState( pPerforceFile ) != P4FILE_UNOPENED;
			bool bFileInPerforce = p4->IsFileInPerforce( pPerforceFile );

			m_pPerforce->SetItemEnabled( m_nPerforceAdd, !bIsUnnamed && !bFileInPerforce && !bOpenedForEdit );
			m_pPerforce->SetItemEnabled( m_nPerforceOpen, !bIsUnnamed && bFileInPerforce && !bOpenedForEdit );
			m_pPerforce->SetItemEnabled( m_nPerforceRevert, !bIsUnnamed && bOpenedForEdit );
			m_pPerforce->SetItemEnabled( m_nPerforceSubmit, !bIsUnnamed && bOpenedForEdit );
			m_pPerforce->SetItemEnabled( m_nPerforceP4Win, !bIsUnnamed && bFileInPerforce || bOpenedForEdit );
			m_pPerforce->SetItemEnabled( m_nPerforceListOpenFiles, true );
		}
		else
		{
			m_pPerforce->SetItemEnabled( m_nPerforceAdd, false );
			m_pPerforce->SetItemEnabled( m_nPerforceOpen, false );
			m_pPerforce->SetItemEnabled( m_nPerforceRevert, false );
			m_pPerforce->SetItemEnabled( m_nPerforceSubmit, false );
			m_pPerforce->SetItemEnabled( m_nPerforceP4Win, false );
			m_pPerforce->SetItemEnabled( m_nPerforceListOpenFiles, bP4Connected );
		}
	}
}


//-----------------------------------------------------------------------------
// Perforce functions
//-----------------------------------------------------------------------------
void CToolFileMenuButton::OnPerforceAdd( )
{
	char pPerforceFile[MAX_PATH];
	if ( m_pFileMenuCallback->GetPerforceFileName( pPerforceFile, sizeof(pPerforceFile) ) )
	{
		CPerforceFileListFrame *pPerforceFrame = new CPerforceFileListFrame( m_pFileMenuCallback->GetRootPanel(), "Add Movie File to Perforce?", "Movie File", PERFORCE_ACTION_FILE_ADD );
		pPerforceFrame->AddFile( pPerforceFile );
		pPerforceFrame->DoModal( );
	}
}


//-----------------------------------------------------------------------------
// Check out a file
//-----------------------------------------------------------------------------
void CToolFileMenuButton::OnPerforceOpen( )
{
	char pPerforceFile[MAX_PATH];
	if ( m_pFileMenuCallback->GetPerforceFileName( pPerforceFile, sizeof(pPerforceFile) ) )
	{
		CPerforceFileListFrame *pPerforceFrame = new CPerforceFileListFrame( m_pFileMenuCallback->GetRootPanel(), "Check Out Movie File from Perforce?", "Movie File", PERFORCE_ACTION_FILE_EDIT );
		pPerforceFrame->AddFile( pPerforceFile );
		pPerforceFrame->DoModal( );
	}
}


//-----------------------------------------------------------------------------
// Revert a file
//-----------------------------------------------------------------------------
void CToolFileMenuButton::OnPerforceRevert( )
{
	char pPerforceFile[MAX_PATH];
	if ( m_pFileMenuCallback->GetPerforceFileName( pPerforceFile, sizeof(pPerforceFile) ) )
	{
		CPerforceFileListFrame *pPerforceFrame = new CPerforceFileListFrame( m_pFileMenuCallback->GetRootPanel(), "Revert Movie File Changes from Perforce?", "Movie File", PERFORCE_ACTION_FILE_REVERT );
		pPerforceFrame->AddFile( pPerforceFile );
		pPerforceFrame->DoModal( );
	}
}


//-----------------------------------------------------------------------------
// Submit a file
//-----------------------------------------------------------------------------
void CToolFileMenuButton::OnPerforceSubmit( )
{
	char pPerforceFile[MAX_PATH];
	if ( m_pFileMenuCallback->GetPerforceFileName( pPerforceFile, sizeof(pPerforceFile) ) )
	{
		CPerforceFileListFrame *pPerforceFrame = new CPerforceFileListFrame( m_pFileMenuCallback->GetRootPanel(), "Submit Movie File Changes to Perforce?", "Movie File", PERFORCE_ACTION_FILE_SUBMIT );
		pPerforceFrame->AddFile( pPerforceFile );
		pPerforceFrame->DoModal( );
	}
}


//-----------------------------------------------------------------------------
// Open a file in p4win
//-----------------------------------------------------------------------------
void CToolFileMenuButton::OnPerforceP4Win( )
{
	char pPerforceFile[MAX_PATH];
	if ( m_pFileMenuCallback->GetPerforceFileName( pPerforceFile, sizeof(pPerforceFile) ) )
	{
		if ( p4->IsFileInPerforce( pPerforceFile ) )
		{
			p4->OpenFileInP4Win( pPerforceFile );
		}
	}
}


//-----------------------------------------------------------------------------
// Show a file in p4win
//-----------------------------------------------------------------------------
void CToolFileMenuButton::OnPerforceListOpenFiles( )
{
	CUtlVector<P4File_t> openedFiles;
	p4->GetOpenedFileListInPath( "GAME", openedFiles );
	COperationFileListFrame *pOpenedFiles = new COperationFileListFrame( m_pFileMenuCallback->GetRootPanel(), "Opened Files In Perforce", "File Name", false, true );

	int nCount = openedFiles.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		const char *pOpenType = NULL;
		switch( openedFiles[i].m_eOpenState )
		{
		case P4FILE_OPENED_FOR_ADD:
			pOpenType = "Add";
			break;
		case P4FILE_OPENED_FOR_EDIT:
			pOpenType = "Edit";
			break;
		case P4FILE_OPENED_FOR_DELETE:
			pOpenType = "Delete";
			break;
		case P4FILE_OPENED_FOR_INTEGRATE:
			pOpenType = "Integrate";
			break;
		}
		 
		if ( pOpenType )
		{
			pOpenedFiles->AddOperation( pOpenType, p4->String( openedFiles[i].m_sLocalFile ) );	
		}
	}

	pOpenedFiles->DoModal( );
}
