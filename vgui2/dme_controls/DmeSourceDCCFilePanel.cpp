//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dme_controls/DmeSourceDCCFilePanel.h"
#include "dme_controls/DmePanel.h"
#include "movieobjects/dmedccmakefile.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/InputDialog.h"
#include "vgui_controls/MessageBox.h"
#include "vgui/keycode.h"
#include "tier1/KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
//
// Hook into the dme panel editor system
//
//-----------------------------------------------------------------------------
IMPLEMENT_DMEPANEL_FACTORY( CDmeSourceDCCFilePanel, DmeSourceDCCFile, "DmeSourceDCCFileDefault", "Maya/XSI Source File Editor", true );


//-----------------------------------------------------------------------------
// Sort by MDL name
//-----------------------------------------------------------------------------
static int __cdecl DccObjectSortFunc( vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString( "dccobject" );
	const char *string2 = item2.kv->GetString( "dccobject" );
	return Q_stricmp( string1, string2 );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor, destructor
//-----------------------------------------------------------------------------
CDmeSourceDCCFilePanel::CDmeSourceDCCFilePanel( vgui::Panel *pParent, const char *pPanelName ) : 
	BaseClass( pParent, pPanelName )
{	
	m_pRootDCCObjects = new vgui::ListPanel( this, "DCCObjectList" );
	m_pRootDCCObjects->AddColumnHeader( 0, "dccobject", "Maya/XSI Object Name", 100, 0 );
	m_pRootDCCObjects->AddActionSignalTarget( this );
	m_pRootDCCObjects->SetSortFunc( 0, DccObjectSortFunc );
	m_pRootDCCObjects->SetSortColumn( 0 );
	//	m_pRootDCCObjects->SetSelectIndividualCells( true );
	m_pRootDCCObjects->SetEmptyListText("No sources");
	//	m_pRootDCCObjects->SetDragEnabled( true );

	m_pDCCObjectBrowser = new vgui::Button( this, "DCCObjectBrowser", "...", this, "OnBrowseDCCObject" );
	m_pDCCObjectName = new vgui::TextEntry( this, "DCCObjectName" );
	m_pDCCObjectName->SendNewLine( true );
	m_pDCCObjectName->AddActionSignalTarget( this );

	m_pAddDCCObject = new vgui::Button( this, "AddDCCObjectButton", "Add", this, "OnAddDCCObject" );
	m_pRemoveDCCObject = new vgui::Button( this, "RemoveDCCObjectButton", "Remove", this, "OnRemoveDCCObject" );

	m_pApplyChanges = new vgui::Button( this, "ApplyChangesButton", "Apply", this, "OnApplyChanges" );

	// Load layout settings; has to happen before pinning occurs in code
	LoadControlSettings( "resource/DmeSourceDCCFilePanel.res" );
}

CDmeSourceDCCFilePanel::~CDmeSourceDCCFilePanel()
{
}


//-----------------------------------------------------------------------------
// Marks the file as dirty (or not)
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::SetDirty()
{
	PostActionSignal( new KeyValues( "DmeElementChanged" ) );
}


//-----------------------------------------------------------------------------
// Refresh the source list
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::RefreshDCCObjectList( )
{
	m_pRootDCCObjects->RemoveAll();
	if ( !m_hSourceDCCFile.Get() )
		return;

	int nCount = m_hSourceDCCFile->m_RootDCCObjects.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		KeyValues *pItemKeys = new KeyValues( "node", "dccobject", m_hSourceDCCFile->m_RootDCCObjects.Get(i) );
		pItemKeys->SetInt( "dccObjectIndex", i );
		m_pRootDCCObjects->AddItem( pItemKeys, 0, false, false );
	}

	m_pRootDCCObjects->SortList();
}


//-----------------------------------------------------------------------------
// Resets the state
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::SetDmeElement( CDmeSourceDCCFile *pSourceDCCFile )
{
	m_hSourceDCCFile = pSourceDCCFile;

	bool bEnabled = ( pSourceDCCFile != NULL );
	m_pDCCObjectBrowser->SetEnabled( bEnabled );
	m_pAddDCCObject->SetEnabled( bEnabled );
	m_pRemoveDCCObject->SetEnabled( bEnabled );
	m_pApplyChanges->SetEnabled( bEnabled );
	if ( !bEnabled )
	{
		m_pRootDCCObjects->RemoveAll();
		m_pDCCObjectName->SetText( "" );
		return;
	}

	RefreshDCCObjectList();
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::OnItemSelectionChanged( )
{
	int nCount = m_pRootDCCObjects->GetSelectedItemsCount();
	bool bEnabled = ( nCount > 0 );
	bool bMultiselect = ( nCount > 1 );
	m_pDCCObjectBrowser->SetEnabled( bEnabled && !bMultiselect );
	m_pDCCObjectName->SetEnabled( bEnabled && !bMultiselect );
	m_pApplyChanges->SetEnabled( bEnabled && !bMultiselect );
	m_pRemoveDCCObject->SetEnabled( bEnabled );
	if ( !bEnabled || bMultiselect )
	{
		m_pDCCObjectName->SetText( "" );
		return;
	}

	int nItemID = m_pRootDCCObjects->GetSelectedItem( 0 );
	KeyValues *pKeyValues = m_pRootDCCObjects->GetItem( nItemID );
	m_pDCCObjectName->SetText( pKeyValues->GetString( "dccobject" ) );
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::OnItemSelected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pRootDCCObjects )
	{
		OnItemSelectionChanged();
		return;
	}
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::OnItemDeselected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pRootDCCObjects )
	{
		OnItemSelectionChanged();
		return;
	}
}


//-----------------------------------------------------------------------------
// Called when return is hit in a text entry field
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::OnTextNewLine( KeyValues *kv )
{
	if ( !m_hSourceDCCFile.Get() )
		return;

	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pDCCObjectName )
	{
		OnDCCObjectNameChanged();
		return;
	}
}


//-----------------------------------------------------------------------------
// Selects a particular DCC object
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::SelectDCCObject( int nDCCObjectIndex )
{
	if ( nDCCObjectIndex < 0 )
	{
		m_pRootDCCObjects->ClearSelectedItems();
		return;
	}

	int nItemID = m_pRootDCCObjects->FirstItem();
	for ( ; nItemID != m_pRootDCCObjects->InvalidItemID(); nItemID = m_pRootDCCObjects->NextItem( nItemID ) )
	{
		KeyValues *kv = m_pRootDCCObjects->GetItem( nItemID );
		if ( kv->GetInt( "dccObjectIndex", -1 ) != nDCCObjectIndex )
			continue;

		m_pRootDCCObjects->SetSingleSelectedItem( nItemID );
		break;
	}
}


//-----------------------------------------------------------------------------
// Called when we're browsing for a DCC object and one was selected
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::OnDCCObjectAdded( const char *pDCCObjectName, KeyValues *pContextKeys )
{
	if ( !m_hSourceDCCFile.Get() )
		return;

	if ( CheckForDuplicateNames( pDCCObjectName ) )
		return;

	int nIndex = -1;
	{
		CDisableUndoScopeGuard guard;
		nIndex = m_hSourceDCCFile->m_RootDCCObjects.AddToTail( pDCCObjectName );
	}
	SetDirty( );
	RefreshDCCObjectList( );
	SelectDCCObject( nIndex );
}


//-----------------------------------------------------------------------------
// Called when the file open dialog for browsing source files selects something
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::OnInputCompleted( KeyValues *kv )
{
	const char *pDCCObjectName = kv->GetString( "text", NULL );
	if ( !pDCCObjectName )						  
		return;

	KeyValues *pDialogKeys = kv->FindKey( "ChangeDCCObject" );
	if ( pDialogKeys )
	{
		m_pDCCObjectName->SetText( pDCCObjectName );
		OnDCCObjectNameChanged();
		return;
	}

	pDialogKeys = kv->FindKey( "AddDCCObject" );
	if ( pDialogKeys )
	{
		OnDCCObjectAdded( pDCCObjectName, pDialogKeys );
		return;
	}
}


//-----------------------------------------------------------------------------
// Shows the DCC object browser (once we have one)
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::ShowDCCObjectBrowser( const char *pTitle, const char *pPrompt, KeyValues *pDialogKeys )
{
	InputDialog *pInput = new InputDialog( this, pTitle, pPrompt );
	pInput->SetMultiline( false );
	pInput->DoModal( pDialogKeys );
}


//-----------------------------------------------------------------------------
// Called when the button to add a file is clicked
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::OnAddDCCObject( )
{
	if ( m_hSourceDCCFile.Get() )
	{
		KeyValues *pDialogKeys = new KeyValues( "AddDCCObject" );
		ShowDCCObjectBrowser( "Add DCC Object", "Enter DCC object name to add", pDialogKeys );
	}
}


//-----------------------------------------------------------------------------
// Called when the button to browse for a source file is clicked
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::OnBrowseDCCObject( )
{
	int nCount = m_pRootDCCObjects->GetSelectedItemsCount();
	if ( nCount == 0 || !m_hSourceDCCFile.Get() )
		return;

	int nItemID = m_pRootDCCObjects->GetSelectedItem( 0 );
	KeyValues *pKeyValues = m_pRootDCCObjects->GetItem( nItemID );
	int nDCCObjectIndex = pKeyValues->GetInt( "dccObjectIndex", -1 );

	KeyValues *pDialogKeys = new KeyValues( "ChangeDCCObject", "dccObjectIndex", nDCCObjectIndex );
	ShowDCCObjectBrowser( "Edit Maya/XSI Object", "Enter new name of Maya/XSI object", pDialogKeys );
}


//-----------------------------------------------------------------------------
// Called when the source file name changes
//-----------------------------------------------------------------------------
bool CDmeSourceDCCFilePanel::CheckForDuplicateNames( const char *pDCCObjectName, int nDCCObjectSkipIndex )
{
	// Look for the existence of this source already
	if ( pDCCObjectName[0] )
	{
		int nCount = m_hSourceDCCFile->m_RootDCCObjects.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			if ( i == nDCCObjectSkipIndex )
				continue;

			if ( !Q_stricmp( pDCCObjectName, m_hSourceDCCFile->m_RootDCCObjects[i] ) )
			{
				vgui::MessageBox *pError = new vgui::MessageBox( "#DmeSourceDCCFile_DuplicateSourceTitle", "#DmeSourceDCCFile_DuplicateSourceText", GetParent() );
				pError->DoModal();
				return true;
			}
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Called when the source file name changes
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::OnDCCObjectNameChanged()
{
	int nCount = m_pRootDCCObjects->GetSelectedItemsCount();
	if ( nCount == 0 || !m_hSourceDCCFile.Get() )
		return;

	int nItemID = m_pRootDCCObjects->GetSelectedItem( 0 );
	KeyValues *pKeyValues = m_pRootDCCObjects->GetItem( nItemID );
	int nDCCObjectIndex = pKeyValues->GetInt( "dccObjectIndex", -1 );
	if ( nDCCObjectIndex < 0 )
		return;

	char pDCCObjectName[MAX_PATH];
	m_pDCCObjectName->GetText( pDCCObjectName, sizeof(pDCCObjectName) );

	if ( CheckForDuplicateNames( pDCCObjectName, nDCCObjectIndex ) )
		return;

	{
		CDisableUndoScopeGuard guard;
		m_hSourceDCCFile->m_RootDCCObjects.Set( nDCCObjectIndex, pDCCObjectName );
	}

	pKeyValues->SetString( "dccobject", pDCCObjectName );
	m_pRootDCCObjects->ApplyItemChanges( nItemID );
	m_pRootDCCObjects->SortList();

	SetDirty( );
}


//-----------------------------------------------------------------------------
// Used for sorting below
//-----------------------------------------------------------------------------
static int IntCompare( const void *pSrc1, const void *pSrc2 )
{
	int i1 = *(int*)pSrc1;
	int i2 = *(int*)pSrc2;
	return i1 - i2;
}


//-----------------------------------------------------------------------------
// Called when the button to remove a DCC object is clicked
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::OnRemoveDCCObject( )
{
	int nCount = m_pRootDCCObjects->GetSelectedItemsCount();
	if ( nCount == 0 || !m_hSourceDCCFile.Get() )
		return;

	int nSelectedCount = 0;
	int *pDCCObjectIndex = (int*)alloca( nCount*sizeof(int) );
	for ( int i = 0; i < nCount; ++i )
	{
		int nItemID = m_pRootDCCObjects->GetSelectedItem( i );
		KeyValues *pKeyValues = m_pRootDCCObjects->GetItem( nItemID );
		int nDCCObjectIndex = pKeyValues->GetInt( "dccObjectIndex", -1 );
		if ( nDCCObjectIndex < 0 )
			continue;
		pDCCObjectIndex[nSelectedCount++] = nDCCObjectIndex;
	}

	if ( nSelectedCount == 0 )
		return;

	// Sort the object indices so we can remove them all
	qsort( pDCCObjectIndex, nSelectedCount, sizeof(int), IntCompare );

	// Update the selection to be reasonable after deletion
	int nItemID = m_pRootDCCObjects->GetSelectedItem( 0 );
	int nRow = m_pRootDCCObjects->GetItemCurrentRow( nItemID );
	Assert( nRow >= 0 );

	{
		CDisableUndoScopeGuard guard;
		// Because we sorted it above, removes will occur properly
		for ( int i = nSelectedCount; --i >= 0; )
		{
			m_hSourceDCCFile->m_RootDCCObjects.Remove( pDCCObjectIndex[i] );
		}
		SetDirty( );
	}
	RefreshDCCObjectList();

	int nVisibleRowCount = m_pRootDCCObjects->GetItemCount();
	if ( nVisibleRowCount == 0 )
		return;

	if ( nRow >= nVisibleRowCount )
	{
		nRow = nVisibleRowCount - 1;
	}

	int nNewItemID = m_pRootDCCObjects->GetItemIDFromRow( nRow );
	m_pRootDCCObjects->SetSingleSelectedItem( nNewItemID );
}


//-----------------------------------------------------------------------------
// Called when a key is typed
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::OnKeyCodeTyped( vgui::KeyCode code )
{
	if ( code == KEY_DELETE )
	{
		OnRemoveDCCObject();
		return;
	}

	BaseClass::OnKeyCodeTyped( code );
}


//-----------------------------------------------------------------------------
// Command handler
//-----------------------------------------------------------------------------
void CDmeSourceDCCFilePanel::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "OnBrowseDCCObject" ) )
	{
		OnBrowseDCCObject();
		return;
	}

	if ( !Q_stricmp( pCommand, "OnAddDCCObject" ) )
	{
		OnAddDCCObject();
		return;
	}

	if ( !Q_stricmp( pCommand, "OnRemoveDCCObject" ) )
	{
		OnRemoveDCCObject();
		return;
	}

	if ( !Q_stricmp( pCommand, "OnApplyChanges" ) )
	{
		OnDCCObjectNameChanged();
		return;
	}

	BaseClass::OnCommand( pCommand );
}

