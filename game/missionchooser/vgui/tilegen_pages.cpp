#include "tilegen_pages.h"
#include "kv_editor.h"
#include "layout_system/tilegen_mission_preprocessor.h"
#include "layout_system_editor/layout_system_kv_editor.h"
#include "MapLayoutPanel.h"
#include "ScrollingWindow.h"
#include <vgui_controls/PanelListPanel.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/TreeView.h>
#include "asw_mission_chooser.h"
#include "asw_key_values_database.h"
#include "filesystem.h"
#include "asw_system.h"
#include "TileGenDialog.h"
#include <vgui_controls/FileOpenDialog.h>
#include <vgui_controls/MessageBox.h>
#include "p4lib/ip4.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ==============================================================================================

CTileGenLayoutPage::CTileGenLayoutPage( Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_pScrollingWindow = new CScrollingWindow( this, "ScrollingWindow" );	
	m_pMapLayoutPanel = new CMapLayoutPanel( this, "MapLayoutPanel" );	
	m_pScrollingWindow->SetChildPanel( m_pMapLayoutPanel );
	m_pScrollingWindow->MoveToLowerCenter();
}

void CTileGenLayoutPage::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "tilegen/TileGenLayoutPage.res", "GAME" );
}

// =============================================================================================

static bool MissionTreeSortFunc( KeyValues *pItem1, KeyValues *pItem2 )
{
	return Q_stricmp( pItem1->GetString( "Text" ), pItem2->GetString( "Text" ) ) < 0;
}

MessageMapItem_t CTilegenKVEditorPage::m_MessageMap[] =
{
	MAP_MESSAGE_CONSTCHARPTR( CTilegenKVEditorPage, "FileSelected", OnFileSelected, "fullpath" ), 
};

IMPLEMENT_PANELMAP( CTilegenKVEditorPage, vgui::EditablePanel );

CTilegenKVEditorPage::CTilegenKVEditorPage( Panel *pParent, const char *pName ) :
BaseClass( pParent, pName )
{
	m_pTree = new vgui::TreeView( this, "Tree" );
	m_pTree->MakeReadyForUse();
	m_pTree->SetSortFunc( MissionTreeSortFunc );

	m_szFilename[0] = '\0';

	m_pFilenameLabel = new vgui::Label( this, "FilenameLabel", "" );
	m_pSaveButton = new vgui::Button( this, "SaveButton", "Save", this, "Save" );
	m_pNewButton = new vgui::Button( this, "NewButton", "New", this, "New" );
}

void CTilegenKVEditorPage::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings( pScheme );
	HFont treeFont = pScheme->GetFont( "DefaultVerySmall" );
	m_pTree->SetFont( treeFont );
}

void CTilegenKVEditorPage::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "Save" ) )
	{
		KeyValues *pKV = GetKeyValues();
		if ( pKV != NULL && !pKV->SaveToFile( g_pFullFileSystem, m_szFilename, "GAME" ) )
		{
			if ( p4 )
			{
				char fullPath[MAX_PATH];
				g_pFullFileSystem->RelativePathToFullPath( m_szFilename, "GAME", fullPath, MAX_PATH );
				if ( p4->IsFileInPerforce( fullPath ) )
				{
					MessageBox *pMessage = new MessageBox( "Check Out?", "File is not writeable. Would you like to check it out from Perforce?", this );
					pMessage->SetCancelButtonVisible( true );
					pMessage->SetOKButtonText( "#MessageBox_Yes" );
					pMessage->SetCancelButtonText( "#MessageBox_No" );
					pMessage->SetCommand( new KeyValues( "CheckOutFromP4", "file", fullPath ) );
					pMessage->DoModal();
					return;
				}
			}
			else
			{
				VGUIMessageBox( this, "Save Error", "Failed to save %s.  Make sure file is checked out from Perforce.", m_szFilename );
			}
		}
		UpdateList();
		return;
	}
	else if ( !Q_stricmp( command, "New" ) )
	{
		SaveNew();
		return;
	}
	BaseClass::OnCommand( command );
}

void CTilegenKVEditorPage::OnCheckOutFromP4( KeyValues *pKV )
{
	const char *pPath = pKV->GetString( "file", NULL );
	if ( pPath != NULL )
	{
		if ( p4->OpenFileForEdit( pPath ) )
		{
			KeyValues *pKV = GetKeyValues();

			if ( !pKV->SaveToFile( g_pFullFileSystem, m_szFilename, "GAME" ) )
			{
				VGUIMessageBox( this, "Save Error!", "Checked out '%s' from Perforce, but failed to save file.", pPath );
			}
		}
		else
		{
			VGUIMessageBox( this, "P4 Error!", "Failed to check out '%s' from Perforce.", pPath );			
		}
	}
	UpdateList();
}

int CTilegenKVEditorPage::RecursiveCreateFolderNodes( int iParentIndex, char *szFilename )
{
	char* szFolder = (char*) Q_strnchr( szFilename, CORRECT_PATH_SEPARATOR, Q_strlen( szFilename ) );
	if ( !szFolder )
	{
		return iParentIndex;
	}

	*szFolder = 0;	
	// check if szFilename node exists already
	for ( int i = 0; i < m_pTree->GetNumChildren( iParentIndex ); i++ )
	{
		int iChild = m_pTree->GetChild( iParentIndex, i );
		KeyValues *pKV = m_pTree->GetItemData( iChild );
		if ( !Q_stricmp( szFilename, pKV->GetString( "Text" ) ) )
		{
			return iChild;
		}
	}
	KeyValues *pFolderEntry = new KeyValues("FolderEntry");
	pFolderEntry->SetString( "Text", szFilename );
	int iItem = m_pTree->AddItem( pFolderEntry, iParentIndex );
	m_pTree->SetItemFgColor( iItem, Color(128, 128, 128, 255) );
	return RecursiveCreateFolderNodes( iItem, szFolder + 1 );
}

void CTilegenKVEditorPage::UpdateListHelper( CASW_KeyValuesDatabase *pMissionDatabase, const char *pFolderName )
{
	m_pTree->RemoveAll();

	KeyValues *kv = new KeyValues( "TVI" );
	kv->SetString( "Text", "Missions" );
	int missionIndex = m_pTree->AddItem( kv, -1 );

	for ( int i = 0; i < pMissionDatabase->GetFileCount(); i++ )
	{
		char szMissionFilename[MAX_PATH];
		Q_strncpy( szMissionFilename, pMissionDatabase->GetFilename( i ), MAX_PATH );
		Q_FixSlashes( szMissionFilename );
		int iNode = RecursiveCreateFolderNodes( missionIndex, szMissionFilename + Q_strlen( pFolderName ) );
		if ( iNode != missionIndex )
		{
			m_pTree->ExpandItem( iNode, true );
		}

		Q_strncpy( szMissionFilename, pMissionDatabase->GetFilename( i ), MAX_PATH );
		Q_FileBase( szMissionFilename, szMissionFilename, sizeof( szMissionFilename ) );
		KeyValues *pMissionEntry = new KeyValues("MissionEntry");
		pMissionEntry->SetString( "Text", szMissionFilename );
		pMissionEntry->SetString( "Filename", pMissionDatabase->GetFilename( i ) );
		int iItem = m_pTree->AddItem( pMissionEntry, iNode );
		m_pTree->SetItemFgColor( iItem, Color(255, 255, 255, 255) );
	}
	m_pTree->ExpandItem( missionIndex, true );
}

// ==============================================================================================

CTilegenLayoutSystemPage::CTilegenLayoutSystemPage( Panel *pParent, const char *pName ) :
	BaseClass( pParent, pName )
{
	m_pPreprocessor = new CTilegenMissionPreprocessor();

	m_pEditor = new CLayoutSystemKVEditor( this, "Editor" );
	m_pEditor->SetPreprocessor( m_pPreprocessor );

	m_pGenerateButton = new vgui::Button( this, "GenerateButton", "Generate", this, "Generate" );
	m_pReloadRulesButton = new vgui::Button( this, "ReloadRulesButton", "Reload Rules", this, "ReloadRules" );
	m_pReloadMissionButton = new vgui::Button( this, "ReloadMissionButton", "Reload", this, "ReloadMission" );

	m_pNewMissionDatabase = new CASW_KeyValuesDatabase();
	m_pNewMissionDatabase->LoadFiles( "tilegen/new_missions/" );

	m_pRulesDatabase = new CASW_KeyValuesDatabase();
	m_pRulesDatabase->LoadFiles( "tilegen/rules/" );

	for ( int i = 0; i < m_pRulesDatabase->GetFileCount(); ++ i )
	{
		m_pPreprocessor->ParseAndStripRules( m_pRulesDatabase->GetFile( i ) );
	}
}

CTilegenLayoutSystemPage::~CTilegenLayoutSystemPage()
{	
	delete m_pNewMissionDatabase;
	delete m_pRulesDatabase;
	delete m_pPreprocessor;
}

void CTilegenLayoutSystemPage::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "tilegen/TilegenLayoutSystemPage.res", "GAME" );
	bool bItemSelected = ( m_szFilename[0] != '\0' );
	m_pReloadMissionButton->SetVisible( bItemSelected );
	m_pSaveButton->SetVisible( bItemSelected );
	m_pGenerateButton->SetVisible( bItemSelected );
}

void CTilegenLayoutSystemPage::OnCommand( const char *pCommand )
{
	if ( Q_stricmp( pCommand, "Generate" ) == 0 )
	{
		if ( g_pTileGenDialog )
		{
			g_pTileGenDialog->GenerateMission( m_szFilename );
		}
		return;
	}
	else if ( Q_stricmp( pCommand, "ReloadMission" ) == 0 )
	{
		KeyValues *pNewKeyValues = m_pNewMissionDatabase->ReloadFile( m_szFilename );
		if ( pNewKeyValues != NULL )
		{
			m_pEditor->SetMissionData( m_pNewMissionDatabase->GetFileByName( m_szFilename ) );
		}	
		return;
	}
	else if ( Q_stricmp( pCommand, "ReloadRules" ) == 0 )
	{
		delete m_pPreprocessor;
		m_pPreprocessor = new CTilegenMissionPreprocessor();
		m_pRulesDatabase->LoadFiles( "tilegen/rules/" );
		for ( int i = 0; i < m_pRulesDatabase->GetFileCount(); ++ i )
		{
			m_pPreprocessor->ParseAndStripRules( m_pRulesDatabase->GetFile( i ) );
		}
		m_pEditor->SetPreprocessor( m_pPreprocessor );
		m_pEditor->RecreateMissionPanel();

		return;
	}
	BaseClass::OnCommand( pCommand );
}

void CTilegenLayoutSystemPage::OnTreeViewItemSelected( int nItemIndex )
{
	KeyValues *pKV = m_pTree->GetItemData( nItemIndex );
	if ( pKV && pKV->FindKey( "Filename" ) )
	{
		const char *pFilename = pKV->GetString( "Filename" );
		KeyValues *pExistingKV = m_pEditor->GetMissionData();
		KeyValues *pNewKV = m_pNewMissionDatabase->GetFileByName( pFilename );
		// @TODO: this is lame, this function gets called twice when the user selects a new node.
		if ( pExistingKV != pNewKV )
		{
			m_pEditor->SetMissionData( pNewKV );
			m_pFilenameLabel->SetText( pFilename + Q_strlen( "tilegen/" ) );
			m_pReloadMissionButton->SetVisible( true );
			m_pSaveButton->SetVisible( true );
			m_pGenerateButton->SetVisible( true );
			Q_snprintf( m_szFilename, MAX_PATH, "%s", pFilename );
		}
	}
}

void CTilegenLayoutSystemPage::OnFileSelected( const char *pFullPath )
{
	if ( g_pFullFileSystem->FileExists( pFullPath ) )
	{
		VGUIMessageBox( this, "Save Error", "A layout system already exists with filename %s.", pFullPath );
		return;
	}
	// create new keys
	KeyValues *pNewKeys = new KeyValues( "Mission" );
	// save them at the full path
	if ( !pNewKeys->SaveToFile( g_pFullFileSystem, pFullPath ) )
	{
		VGUIMessageBox( this, "Save Error", "Failed to save new file %s.", pFullPath );
		return;
	}
	m_pEditor->SetMissionData( pNewKeys );

	m_pReloadMissionButton->SetVisible( true );
	m_pSaveButton->SetVisible( true );
	m_pGenerateButton->SetVisible( true );
	bool bConverted = g_pFullFileSystem->FullPathToRelativePath( pFullPath, m_szFilename, MAX_PATH );
	if ( !bConverted )
	{
		Warning( "Failed to convert this to a relative path: %s\n", pFullPath );
		return;
	}
	m_pFilenameLabel->SetText( m_szFilename + Q_strlen( "tilegen/" ) );	
	m_pNewMissionDatabase->AddFile( pNewKeys, m_szFilename );
	UpdateList();

	if ( p4 )
	{
		MessageBox *pMessage = new MessageBox( "Add to P4?", "Would you like to add this mission to perforce?", this );
		pMessage->SetCancelButtonVisible( true );
		pMessage->SetOKButtonText( "#MessageBox_Yes" );
		pMessage->SetCancelButtonText( "#MessageBox_No" );
		pMessage->SetCommand( new KeyValues( "AddToP4", "file", pFullPath ) );
		pMessage->DoModal();
	}
}

void CTilegenLayoutSystemPage::OnAddToP4( KeyValues *pKV )
{
	const char *pPath = pKV->GetString( "file", NULL );
	if ( pPath != NULL )
	{
		if ( !p4->OpenFileForAdd( pPath ) )
		{
			VGUIMessageBox( this, "P4 Error!", "Failed to add '%s' to Perforce.", pPath );
		}
	}
}

void CTilegenLayoutSystemPage::UpdateList()
{
	UpdateListHelper( m_pNewMissionDatabase, "tilegen/new_missions/" );
}

void CTilegenLayoutSystemPage::SaveNew()
{
	// Pop up the dialog
	FileOpenDialog *pFileDialog = new FileOpenDialog (NULL, "Save New Layout System As...", false);
	char buffer[MAX_PATH];
	Q_snprintf( buffer, sizeof( buffer ), "%s/tilegen/new_missions", g_gamedir );
	pFileDialog->SetStartDirectory( buffer );
	pFileDialog->AddFilter("*.txt", "Layout System file (*.txt)", true);
	pFileDialog->AddActionSignalTarget(this);

	pFileDialog->DoModal(true);
}

KeyValues *CTilegenLayoutSystemPage::GetKeyValues()
{
	return m_pEditor->GetMissionData();
}
