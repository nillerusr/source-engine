//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "dme_controls/dmepresetgroupeditorpanel.h"
#include "dme_controls/dmecontrols_utils.h"
#include "movieobjects/dmeanimationset.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/Splitter.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/InputDialog.h"
#include "vgui_controls/TextEntry.h"
#include "vgui/MouseCode.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "tier1/KeyValues.h"
#include "tier1/utldict.h"
#include "dme_controls/presetpicker.h"
#include "vgui_controls/FileOpenDialog.h"
#include "tier2/fileutils.h"
#include "tier1/utlbuffer.h"
#include "dme_controls/inotifyui.h"
#include "../game/shared/iscenetokenprocessor.h"
#include "movieobjects/dmx_to_vcd.h"
#include "studio.h"
#include "phonemeconverter.h"

// Forward declaration
class CDmePresetGroupEditorPanel;


//-----------------------------------------------------------------------------
// Utility scope guards
//-----------------------------------------------------------------------------
DEFINE_SOURCE_UNDO_SCOPE_GUARD( PresetGroup, NOTIFY_SOURCE_PRESET_GROUP_EDITOR );
DEFINE_SOURCE_NOTIFY_SCOPE_GUARD( PresetGroup, NOTIFY_SOURCE_PRESET_GROUP_EDITOR );

#define PRESET_FILE_FORMAT "preset"

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
// CDmePresetRemapPanel
//
// Implementation below because of scoping issues
//
//-----------------------------------------------------------------------------
class CDmePresetRemapPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CDmePresetRemapPanel, vgui::Frame );

public:
	CDmePresetRemapPanel( vgui::Panel *pParent, const char *pTitle );
	~CDmePresetRemapPanel();

	// Shows the modal dialog
	void DoModal( CDmeAnimationSet *pAnimationSet, CDmePresetGroup *pDestGroup );

	// Inherited from Frame
	virtual void OnCommand( const char *pCommand );

	virtual void OnKeyCodeTyped( KeyCode code );

private:
	MESSAGE_FUNC( OnTextChanged, "TextChanged" );
	MESSAGE_FUNC( OnSelectPreset, "SelectPreset" );
	MESSAGE_FUNC( OnRemovePreset, "RemovePreset" );
	MESSAGE_FUNC_PARAMS( OnPresetPicked, "PresetPicked", params );
	MESSAGE_FUNC_PARAMS( OnOpenContextMenu, "OpenContextMenu", kv );

	// Refreshes the list of presets
	void RefreshPresetList( );

	// Applies changes to the preset remap
	void ApplyChangesToPresetRemap();

	// Cleans up the context menu
	void CleanupContextMenu();

	vgui::ListPanel *m_pPresetRemapList;
	vgui::ComboBox	*m_pSourcePresetGroup;
	CDmeHandle< CDmePresetGroup > m_hSourceGroup;
	CDmeHandle< CDmePresetGroup > m_hDestGroup;
	vgui::DHANDLE< vgui::Menu > m_hContextMenu;
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
static int __cdecl DestPresetNameSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString( "dest" );
	const char *string2 = item2.kv->GetString( "dest" );
	return Q_stricmp( string1, string2 );
}

static int __cdecl SrcPresetNameSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString( "src" );
	const char *string2 = item2.kv->GetString( "src" );
	return Q_stricmp( string1, string2 );
}

CDmePresetRemapPanel::CDmePresetRemapPanel( vgui::Panel *pParent, const char *pTitle ) : 
	BaseClass( pParent, "DmePresetRemapPanel" )
{
	m_pSourcePresetGroup = new vgui::ComboBox( this, "SourcePresetGroup", 8, true );
	SetDeleteSelfOnClose( true );

	m_pPresetRemapList = new vgui::ListPanel( this, "PresetRemapList" );
	m_pPresetRemapList->AddColumnHeader( 0, "dest", "Dest Preset", 100, 0 );
	m_pPresetRemapList->AddColumnHeader( 1, "src", "Source Preset", 100, 0 );
	m_pPresetRemapList->SetSelectIndividualCells( false );
	m_pPresetRemapList->SetMultiselectEnabled( true );
	m_pPresetRemapList->SetEmptyListText( "No presets" );
	m_pPresetRemapList->AddActionSignalTarget( this );
	m_pPresetRemapList->SetSortFunc( 0, DestPresetNameSortFunc );
	m_pPresetRemapList->SetSortFunc( 1, SrcPresetNameSortFunc );
	m_pPresetRemapList->SetSortColumn( 0 );

	SetBlockDragChaining( true );

	LoadControlSettingsAndUserConfig( "resource/presetremappanel.res" );

	SetTitle( pTitle, false );
}

CDmePresetRemapPanel::~CDmePresetRemapPanel()
{
	CleanupContextMenu();
}


//-----------------------------------------------------------------------------
// Cleans up the context menu
//-----------------------------------------------------------------------------
void CDmePresetRemapPanel::CleanupContextMenu()
{
	if ( m_hContextMenu.Get() )
	{
		m_hContextMenu->MarkForDeletion();
		m_hContextMenu = NULL;
	}
}


//-----------------------------------------------------------------------------
// Refreshes the list of presets
//-----------------------------------------------------------------------------
void CDmePresetRemapPanel::RefreshPresetList( )
{
	m_pPresetRemapList->RemoveAll();
							   
	CDmaElementArray< CDmePreset > *pPresetList = m_hDestGroup.Get() ? &m_hDestGroup->GetPresets() : NULL;
	if ( !pPresetList )
		return;

	int nCount = pPresetList->Count();
	if ( nCount == 0 )
		return;

	CDmePresetRemap *pRemap = m_hDestGroup->GetPresetRemap();
	bool bUseRemap = ( pRemap && m_hSourceGroup.Get() && !Q_stricmp( pRemap->m_SourcePresetGroup, m_hSourceGroup->GetName() ) );

	for ( int i = 0; i < nCount; ++i )
	{
		CDmePreset *pPreset = pPresetList->Get(i);

		const char *pName = pPreset->GetName();
		if ( !pName || !pName[0] )
		{
			pName = "<no name>";
		}

		KeyValues *kv = new KeyValues( "node" );
		kv->SetString( "dest", pName );
		SetElementKeyValue( kv, "destPreset", pPreset );
		if ( bUseRemap )
		{
			const char *pSource = pRemap->FindSourcePreset( pName );
			CDmePreset *pSrcPreset = pSource ? m_hSourceGroup->FindPreset( pSource ) : NULL;
			kv->SetString( "src", pSrcPreset ? pSrcPreset->GetName() : "" ); 
			SetElementKeyValue( kv, "srcPreset", pSrcPreset );
		}
		else
		{
			kv->SetString( "src", "" ); 
			SetElementKeyValue( kv, "srcPreset", NULL );
		}

		m_pPresetRemapList->AddItem( kv, 0, false, false );
	}
	m_pPresetRemapList->SortList();
}


//-----------------------------------------------------------------------------
// Called by the preset picker when a preset is picked
//-----------------------------------------------------------------------------
void CDmePresetRemapPanel::OnPresetPicked( KeyValues *pParams )
{
	int nSelectedItemCount = m_pPresetRemapList->GetSelectedItemsCount();
	if ( nSelectedItemCount != 1 )
		return;

	CDmePreset *pPreset = GetElementKeyValue< CDmePreset >( pParams, "preset" );
	int nItemID = m_pPresetRemapList->GetSelectedItem( 0 );
	KeyValues *kv = m_pPresetRemapList->GetItem( nItemID );
	kv->SetString( "src", pPreset ? pPreset->GetName() : "" ); 
	SetElementKeyValue( kv, "srcPreset", pPreset );
	m_pPresetRemapList->ApplyItemChanges( nItemID );
}


//-----------------------------------------------------------------------------
// Called when double-clicking on a list entry
//-----------------------------------------------------------------------------
void CDmePresetRemapPanel::OnKeyCodeTyped( KeyCode code )
{
	if ( code == KEY_ENTER )
	{
		OnSelectPreset();
		return;
	}

	if ( code == KEY_DELETE || code == KEY_BACKSPACE )
	{
		OnRemovePreset();
		return;
	}

	BaseClass::OnKeyCodeTyped( code );
}


//-----------------------------------------------------------------------------
// Called by the context menu
//-----------------------------------------------------------------------------
void CDmePresetRemapPanel::OnSelectPreset()
{
	int nSelectedItemCount = m_pPresetRemapList->GetSelectedItemsCount();
	if ( nSelectedItemCount != 1 )
		return;

	CPresetPickerFrame *pPresetPicker = new CPresetPickerFrame( this, "Select Source Preset", false );
	pPresetPicker->AddActionSignalTarget( this );
	pPresetPicker->DoModal( m_hSourceGroup, false, NULL );
}

void CDmePresetRemapPanel::OnRemovePreset()
{
	int nSelectedItemCount = m_pPresetRemapList->GetSelectedItemsCount();
	if ( nSelectedItemCount != 1 )
		return;

	int nItemID = m_pPresetRemapList->GetSelectedItem( 0 );
	KeyValues *kv = m_pPresetRemapList->GetItem( nItemID );
	kv->SetString( "src", "" ); 
	SetElementKeyValue( kv, "srcPreset", NULL );
	m_pPresetRemapList->ApplyItemChanges( nItemID );
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmePresetRemapPanel::OnOpenContextMenu( KeyValues *kv )
{
	CleanupContextMenu();

	int nSelectedItemCount = m_pPresetRemapList->GetSelectedItemsCount();
	if ( nSelectedItemCount != 1 )
		return;

	m_hContextMenu = new vgui::Menu( this, "ActionMenu" );
	m_hContextMenu->AddMenuItem( "#DmePresetRemapPanel_SelectPreset", new KeyValues( "SelectPreset" ), this );
	m_hContextMenu->AddMenuItem( "#DmePresetRemapPanel_RemovePreset", new KeyValues( "RemovePreset" ), this );

	vgui::Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}


//-----------------------------------------------------------------------------
// Called when the dest combo list changes
//-----------------------------------------------------------------------------
void CDmePresetRemapPanel::OnTextChanged()
{
	KeyValues *pCurrentGroup = m_pSourcePresetGroup->GetActiveItemUserData();
	m_hSourceGroup = pCurrentGroup ? GetElementKeyValue<CDmePresetGroup>( pCurrentGroup, "presetGroup" ) : NULL;
	RefreshPresetList();
}


void CDmePresetRemapPanel::DoModal( CDmeAnimationSet *pAnimationSet, CDmePresetGroup *pDestGroup )
{
	m_hDestGroup = pDestGroup;

	m_pSourcePresetGroup->DeleteAllItems();

	bool bSelected = false;

	CDmePresetRemap* pRemap = m_hDestGroup->GetPresetRemap();

	// Populate the combo box with preset group names
	const CDmaElementArray< CDmePresetGroup > &presetGroupList = pAnimationSet->GetPresetGroups();
	int nCount = presetGroupList.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmePresetGroup *pPresetGroup = presetGroupList[i];
		if ( pPresetGroup == m_hDestGroup.Get() )
			continue;
		 
		KeyValues *kv = new KeyValues( "entry" );
		SetElementKeyValue( kv, "presetGroup", pPresetGroup );
		int nItemID = m_pSourcePresetGroup->AddItem( pPresetGroup->GetName(), kv );
		if ( !bSelected || ( pRemap && !Q_stricmp( pRemap->m_SourcePresetGroup, pPresetGroup->GetName() ) ) )
		{
			m_pSourcePresetGroup->ActivateItem( nItemID );
			bSelected = true;
		}
	}

	BaseClass::DoModal( );

	m_pSourcePresetGroup->RequestFocus();
}


//-----------------------------------------------------------------------------
// Applies changes to the preset remap
//-----------------------------------------------------------------------------
void CDmePresetRemapPanel::ApplyChangesToPresetRemap()
{
	int nTextLength = m_pSourcePresetGroup->GetTextLength() + 1;
	char* pSourceName = (char*)_alloca( nTextLength * sizeof(char) );
	m_pSourcePresetGroup->GetText( pSourceName, nTextLength );

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Change Preset Remap" );
	CDmePresetRemap *pPresetRemap = m_hDestGroup->GetOrAddPresetRemap();
	pPresetRemap->m_SourcePresetGroup = pSourceName;
	pPresetRemap->RemoveAll();
	for ( int nItemID = m_pPresetRemapList->FirstItem(); 
		nItemID != m_pPresetRemapList->InvalidItemID(); 
		nItemID = m_pPresetRemapList->NextItem( nItemID ) )
	{
		KeyValues* pKeyValues = m_pPresetRemapList->GetItem( nItemID );
		CDmePreset *pSrcPreset = GetElementKeyValue< CDmePreset >( pKeyValues, "srcPreset" );
		CDmePreset *pDestPreset = GetElementKeyValue< CDmePreset >( pKeyValues, "destPreset" );
		if ( pSrcPreset && pDestPreset )
		{
			pPresetRemap->AddRemap( pSrcPreset->GetName(), pDestPreset->GetName() );
		}
	}
}


//-----------------------------------------------------------------------------
// command handler
//-----------------------------------------------------------------------------
void CDmePresetRemapPanel::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "Ok") )
	{
		ApplyChangesToPresetRemap();
		CloseModal();
		return;
	}

	if ( !Q_stricmp( command, "Cancel") )
	{
		CloseModal();
		return;
	}

	BaseClass::OnCommand( command );
}


//-----------------------------------------------------------------------------
//
// CDmePresetGroupListPanel
//
// Implementation below because of scoping issues
//
//-----------------------------------------------------------------------------
class CDmePresetGroupListPanel : public vgui::ListPanel
{
	DECLARE_CLASS_SIMPLE( CDmePresetGroupListPanel, vgui::ListPanel );

public:
	// constructor, destructor
	CDmePresetGroupListPanel( vgui::Panel *pParent, const char *pName, CDmePresetGroupEditorPanel *pComboPanel );

	virtual void OnCreateDragData( KeyValues *msg );
	virtual bool IsDroppable( CUtlVector< KeyValues * >& msgList );
	virtual void OnPanelDropped( CUtlVector< KeyValues * >& msgList );
	virtual void OnKeyCodeTyped( vgui::KeyCode code );
	virtual void OnMouseDoublePressed( vgui::MouseCode code );
	virtual void OnDroppablePanelPaint( CUtlVector< KeyValues * >& msglist, CUtlVector< Panel * >& dragPanels );

private:
	CDmePresetGroupEditorPanel *m_pPresetGroupPanel;
};


//-----------------------------------------------------------------------------
//
// CDmePresetListPanel
//
// Implementation below because of scoping issues
//
//-----------------------------------------------------------------------------
class CDmePresetListPanel : public vgui::ListPanel
{
	DECLARE_CLASS_SIMPLE( CDmePresetListPanel, vgui::ListPanel );

public:
	// constructor, destructor
	CDmePresetListPanel( vgui::Panel *pParent, const char *pName, CDmePresetGroupEditorPanel *pComboPanel );

	virtual void OnKeyCodeTyped( vgui::KeyCode code );
	virtual void OnCreateDragData( KeyValues *msg );
	virtual bool IsDroppable( CUtlVector< KeyValues * >& msgList );
	virtual void OnPanelDropped( CUtlVector< KeyValues * >& msgList );
	virtual void OnDroppablePanelPaint( CUtlVector< KeyValues * >& msglist, CUtlVector< Panel * >& dragPanels );

private:

	CDmePresetGroupEditorPanel *m_pPresetGroupPanel;
};


//-----------------------------------------------------------------------------
// Sort functions for list panel
//-----------------------------------------------------------------------------
static int __cdecl IndexSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	int nIndex1 = item1.kv->GetInt("index");
	int nIndex2 = item2.kv->GetInt("index");
	return nIndex1 - nIndex2;
}


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CDmePresetGroupEditorPanel::CDmePresetGroupEditorPanel( vgui::Panel *pParent, const char *pName ) :
	BaseClass( pParent, pName )
{
	m_pSplitter = new vgui::Splitter( this, "PresetGroupSplitter", vgui::SPLITTER_MODE_VERTICAL, 1 );
	vgui::Panel *pSplitterLeftSide = m_pSplitter->GetChild( 0 );
	vgui::Panel *pSplitterRightSide = m_pSplitter->GetChild( 1 );

	m_pPresetGroupList = new CDmePresetGroupListPanel( pSplitterLeftSide, "PresetGroupList", this );
	m_pPresetGroupList->AddColumnHeader( 0, "name", "Preset Group Name", 150, 0 );
	m_pPresetGroupList->AddColumnHeader( 1, "visible", "Visible", 70, 0 );
	m_pPresetGroupList->AddColumnHeader( 2, "shared", "Shared", 52, 0 );
	m_pPresetGroupList->AddColumnHeader( 3, "readonly", "Read Only", 52, 0 );
	m_pPresetGroupList->SetSelectIndividualCells( false );
	m_pPresetGroupList->SetMultiselectEnabled( false );
	m_pPresetGroupList->SetEmptyListText( "No preset groups" );
	m_pPresetGroupList->AddActionSignalTarget( this );
	m_pPresetGroupList->SetSortFunc( 0, IndexSortFunc );
	m_pPresetGroupList->SetSortFunc( 1, NULL );
	m_pPresetGroupList->SetColumnSortable( 1, false );
	m_pPresetGroupList->SetSortFunc( 2, NULL );
	m_pPresetGroupList->SetColumnSortable( 2, false );
	m_pPresetGroupList->SetSortFunc( 3, NULL );
	m_pPresetGroupList->SetColumnSortable( 3, false );
	m_pPresetGroupList->SetDropEnabled( true );
	m_pPresetGroupList->SetSortColumn( 0 );
	m_pPresetGroupList->SetDragEnabled( true );
	m_pPresetGroupList->SetDropEnabled( true );
	m_pPresetGroupList->SetIgnoreDoubleClick( true );

	m_pPresetList = new CDmePresetListPanel( pSplitterRightSide, "PresetList", this );
	m_pPresetList->AddColumnHeader( 0, "name", "Preset Name", 150, 0 );
	m_pPresetList->SetSelectIndividualCells( false );
	m_pPresetList->SetEmptyListText( "No presets" );
	m_pPresetList->AddActionSignalTarget( this );
	m_pPresetList->SetSortFunc( 0, IndexSortFunc );
	m_pPresetList->SetSortColumn( 0 );
	m_pPresetList->SetDragEnabled( true );
	m_pPresetList->SetDropEnabled( true );
	m_pPresetList->SetIgnoreDoubleClick( true );

	LoadControlSettingsAndUserConfig( "resource/dmepresetgroupeditorpanel.res" );

	m_hFileOpenStateMachine = new vgui::FileOpenStateMachine( this, this );
	m_hFileOpenStateMachine->AddActionSignalTarget( this );
}


CDmePresetGroupEditorPanel::~CDmePresetGroupEditorPanel()
{
	CleanupContextMenu();
	SaveUserConfig();
}


//-----------------------------------------------------------------------------
// Cleans up the context menu
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::CleanupContextMenu()
{
	if ( m_hContextMenu.Get() )
	{
		m_hContextMenu->MarkForDeletion();
		m_hContextMenu = NULL;
	}
}


//-----------------------------------------------------------------------------
// Sets the combination operator
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::SetAnimationSet( CDmeAnimationSet *pAnimationSet )
{
	m_hAnimationSet = pAnimationSet;
	RefreshAnimationSet();
}

CDmeAnimationSet* CDmePresetGroupEditorPanel::GetAnimationSet()
{
	return m_hAnimationSet;
}


//-----------------------------------------------------------------------------
// Builds the preset group list for the animation set
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::RefreshAnimationSet()
{
	CDmePresetGroup *pSelectedPresetGroup = GetSelectedPresetGroup();

	m_pPresetGroupList->RemoveAll();	
	if ( !m_hAnimationSet.Get() )
		return;
	
	const CDmaElementArray< CDmePresetGroup > &presetGroupList = m_hAnimationSet->GetPresetGroups();
	int nCount = presetGroupList.Count();
	for ( int i = 0; i < nCount; ++i )
	{ 
		CDmePresetGroup *pPresetGroup = presetGroupList[i];
		Assert( pPresetGroup );
		if ( !pPresetGroup )
			continue;

		bool bIsVisible = pPresetGroup->m_bIsVisible;
		KeyValues *kv = new KeyValues( "node", "name", pPresetGroup->GetName() );
		kv->SetString( "visible", bIsVisible ? "Yes" : "No" ); 
		kv->SetString( "shared", pPresetGroup->IsShared() ? "Yes" : "No" ); 
		kv->SetString( "readonly", pPresetGroup->m_bIsReadOnly ? "Yes" : "No" ); 
		SetElementKeyValue( kv, "presetGroup", pPresetGroup );
		kv->SetColor( "cellcolor", pPresetGroup->m_bIsReadOnly ? Color( 255, 0, 0, 255 ) : Color( 255, 255, 255, 255 ) ); 
		kv->SetInt( "index", i );
		int nItemID = m_pPresetGroupList->AddItem( kv, 0, false, false );

		if ( pSelectedPresetGroup == pPresetGroup )
		{
			m_pPresetGroupList->AddSelectedItem( nItemID );
		}
	}

	m_pPresetGroupList->SortList();

	RefreshPresetNames();
}


//-----------------------------------------------------------------------------
// Tells any class that cares that the data in this thing has changed
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::NotifyDataChanged()
{
	PostActionSignal( new KeyValues( "PresetsChanged" ) );
}


//-----------------------------------------------------------------------------
// Refreshes the list of presets in the selected preset group
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::RefreshPresetNames()
{
	CDmePreset *pSelectedPreset = GetSelectedPreset();

	m_pPresetList->RemoveAll();	
	if ( !m_hAnimationSet.Get() )
		return;

	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return;

	const CDmaElementArray< CDmePreset > &presetList = pPresetGroup->GetPresets();
	int nCount = presetList.Count( );
	for ( int i = 0; i < nCount; ++i )
	{
		CDmePreset *pPreset = presetList[i];
		KeyValues *kv = new KeyValues( "node", "name", pPreset->GetName() );
		SetElementKeyValue( kv, "preset", pPreset );
		kv->SetInt( "index", i );
		int nItemID = m_pPresetList->AddItem( kv, 0, false, false );
		if ( pSelectedPreset == pPreset )
		{
			m_pPresetList->AddSelectedItem( nItemID );
		}
	}

	m_pPresetList->SortList();
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
CDmePreset* CDmePresetGroupEditorPanel::GetSelectedPreset()
{
	if ( !m_hAnimationSet.Get() )
		return NULL;

	int nSelectedPresetCount = m_pPresetList->GetSelectedItemsCount();
	if ( nSelectedPresetCount != 1 )
		return NULL;

	int nItemID = m_pPresetList->GetSelectedItem( 0 );
	KeyValues *pKeyValues = m_pPresetList->GetItem( nItemID );

	CDmePreset *pPreset = GetElementKeyValue< CDmePreset >( pKeyValues, "preset" );
	return pPreset;
}


//-----------------------------------------------------------------------------
// Selects a particular preset 
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::SetSelectedPreset( CDmePreset* pPreset )
{
	m_pPresetList->ClearSelectedItems();
	for ( int nItemID = m_pPresetList->FirstItem(); 
		nItemID != m_pPresetList->InvalidItemID(); 
		nItemID = m_pPresetList->NextItem( nItemID ) )
	{
		KeyValues* pKeyValues = m_pPresetList->GetItem( nItemID );
		CDmePreset *pItemPreset = GetElementKeyValue< CDmePreset >( pKeyValues, "preset" );
		if ( pItemPreset == pPreset )
		{
			m_pPresetList->AddSelectedItem( nItemID );
		}
	}
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
CDmePresetGroup* CDmePresetGroupEditorPanel::GetSelectedPresetGroup()
{
	if ( !m_hAnimationSet.Get() )
		return NULL;

	int nSelectedItemCount = m_pPresetGroupList->GetSelectedItemsCount();
	if ( nSelectedItemCount != 1 )
		return NULL;

	int nItemID = m_pPresetGroupList->GetSelectedItem( 0 );
	KeyValues *pKeyValues = m_pPresetGroupList->GetItem( nItemID );
	CDmePresetGroup *pPresetGroup = GetElementKeyValue<CDmePresetGroup>( pKeyValues, "presetGroup" );
	return pPresetGroup;
}


//-----------------------------------------------------------------------------
// Selects a particular preset group
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::SetSelectedPresetGroup( CDmePresetGroup* pPresetGroup )
{
	m_pPresetGroupList->ClearSelectedItems();
	for ( int nItemID = m_pPresetGroupList->FirstItem(); 
		nItemID != m_pPresetGroupList->InvalidItemID(); 
		nItemID = m_pPresetGroupList->NextItem( nItemID ) )
	{
		KeyValues* pKeyValues = m_pPresetGroupList->GetItem( nItemID );
		CDmePresetGroup *pItemPresetGroup = GetElementKeyValue< CDmePresetGroup >( pKeyValues, "presetGroup" );
		if ( pItemPresetGroup == pPresetGroup )
		{
			m_pPresetGroupList->AddSelectedItem( nItemID );
		}
	}
}


//-----------------------------------------------------------------------------
// If it finds a duplicate preset name, reports an error message and returns it found one
//-----------------------------------------------------------------------------
bool CDmePresetGroupEditorPanel::HasDuplicatePresetName( const char *pPresetName, CDmePreset *pIgnorePreset )
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return false;

	CDmePreset *pMatch = pPresetGroup->FindPreset( pPresetName );
	if ( pMatch && pMatch != pIgnorePreset )
	{
		vgui::MessageBox *pError = new vgui::MessageBox( "#DmePresetGroupEditor_DuplicatePresetNameTitle", "#DmePresetGroupEditor_DuplicatePresetNameText", this );
		pError->DoModal();
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Called by OnInputCompleted after we get a new group name
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::PerformAddPreset( const char *pNewPresetName )
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return;

	if ( HasDuplicatePresetName( pNewPresetName ) ) 
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Add Preset" );
	CDmePreset *pPreset = pPresetGroup->FindOrAddPreset( pNewPresetName );
	sg.Release();

	RefreshPresetNames();
	SetSelectedPreset( pPreset );
	NotifyDataChanged();

	KeyValues *pKeyValues = new KeyValues( "AddNewPreset" );
	SetElementKeyValue( pKeyValues, "preset", pPreset );
	PostActionSignal( pKeyValues );
}


//-----------------------------------------------------------------------------
// Called by OnInputCompleted after we get a new group name
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::PerformRenamePreset( const char *pNewPresetName )
{
	CDmePreset *pPreset = GetSelectedPreset();
	if ( !pPreset )
		return;

	if ( HasDuplicatePresetName( pNewPresetName, pPreset ) ) 
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Rename Preset" );
	pPreset->SetName( pNewPresetName );
	sg.Release();

	RefreshPresetNames();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Add a preset 
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnAddPreset()
{
	vgui::InputDialog *pInput = new vgui::InputDialog( this, "Add Preset", "Enter name of new preset" );
	pInput->SetMultiline( false );
	pInput->DoModal( new KeyValues( "OnAddPreset" ) );
}


//-----------------------------------------------------------------------------
// Rename a preset 
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnRenamePreset()
{
	CDmePreset *pPreset = GetSelectedPreset();
	if ( !pPreset )
		return;

	vgui::InputDialog *pInput = new vgui::InputDialog( this, "Rename Preset", "Enter new name of preset" );
	pInput->SetMultiline( false );
	pInput->DoModal( new KeyValues( "OnRenamePreset" ) );
}


//-----------------------------------------------------------------------------
// Remove a preset
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnRemovePreset()
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return;

	CDmePreset *pPreset = GetSelectedPreset();
	if ( !pPreset )
		return;

	int nItemID = m_pPresetList->GetSelectedItem( 0 );
	int nCurrentRow = m_pPresetList->GetItemCurrentRow( nItemID );

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Remove Preset" );
	pPresetGroup->RemovePreset( pPreset );
	sg.Release();

	RefreshPresetNames();
	if ( nCurrentRow >= m_pPresetList->GetItemCount() )
	{
		--nCurrentRow;
	}
	if ( nCurrentRow >= 0 )
	{
		nItemID = m_pPresetList->GetItemIDFromRow( nCurrentRow );
		m_pPresetList->ClearSelectedItems();
		m_pPresetList->AddSelectedItem( nItemID );
	}
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnMovePresetUp()
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	CDmePreset *pPreset = GetSelectedPreset();
	if ( !pPresetGroup || !pPreset )
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Reorder Presets" );
	pPresetGroup->MovePresetUp( pPreset );
	sg.Release();

	RefreshPresetNames();
	SetSelectedPreset( pPreset );
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnMovePresetDown()
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	CDmePreset *pPreset = GetSelectedPreset();
	if ( !pPresetGroup || !pPreset )
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Reorder Presets" );
	pPresetGroup->MovePresetDown( pPreset );
	sg.Release();

	RefreshPresetNames();
	SetSelectedPreset( pPreset );
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Drag/drop reordering of presets
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::MovePresetInFrontOf( CDmePreset *pDragPreset, CDmePreset *pDropPreset )
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Reorder Presets" );
	pPresetGroup->MovePresetInFrontOf( pDragPreset, pDropPreset );
	sg.Release();

	RefreshPresetNames();
	SetSelectedPreset( pDragPreset );
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Fileopen state machine
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnFileStateMachineFinished( KeyValues *pParams )
{
	KeyValues *pContextKeyValues = pParams->GetFirstTrueSubKey();
	if ( Q_stricmp( pContextKeyValues->GetName(), "ImportPresets" ) )
		return;

	CDmElement *pRoot = GetElementKeyValue<CDmElement>( pContextKeyValues, "presets" );
	if ( !pRoot )
		return;

	if ( pParams->GetInt( "completionState", 0 ) != 0 )
	{
		CPresetPickerFrame *pPresetPicker = new CPresetPickerFrame( this, "Select Preset(s) to Import" );
		pPresetPicker->AddActionSignalTarget( this );
		KeyValues *pContextKeyValuesImport = new KeyValues( "ImportPicked" );
		SetElementKeyValue( pContextKeyValuesImport, "presets", pRoot );
		pPresetPicker->DoModal( pRoot, true, pContextKeyValuesImport );
	}
	else
	{
		// Clean up the read-in file
		CDisableUndoScopeGuard sg;
		g_pDataModel->RemoveFileId( pRoot->GetFileId() );
	}
}


//-----------------------------------------------------------------------------
// Finds a control index
//-----------------------------------------------------------------------------
struct ExportedControl_t
{
	CUtlString m_Name;
	bool m_bIsStereo;
	bool m_bIsMulti;
	int m_nFirstIndex;
};

static int FindExportedControlIndex( const char *pControlName, CUtlVector< ExportedControl_t > &uniqueControls )
{
	int nCount = uniqueControls.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( pControlName, uniqueControls[i].m_Name ) )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Builds a unique list of controls found in the presets
//-----------------------------------------------------------------------------
static int BuildExportedControlList( CDmeAnimationSet *pAnimationSet, CDmePresetGroup *pPresetGroup, CUtlVector< ExportedControl_t > &uniqueControls )
{
	int nGlobalIndex = 0;
	const CDmrElementArray< CDmePreset > &presets = pPresetGroup->GetPresets();
	int nPresetCount = presets.Count();
	for ( int iPreset = 0; iPreset < nPresetCount; ++iPreset )
	{
		CDmePreset *pPreset = presets[iPreset];
		const CDmrElementArray< CDmElement > &controls = pPreset->GetControlValues();

		int nControlCount = controls.Count();
		for ( int i = 0; i < nControlCount; ++i )
		{
			const char *pControlName = controls[i]->GetName();
			int nIndex = FindExportedControlIndex( pControlName, uniqueControls );
			if ( nIndex >= 0 )
				continue;
			CDmAttribute *pValueAttribute = controls[i]->GetAttribute( "value" );
			if ( !pValueAttribute || pValueAttribute->GetType() != AT_FLOAT )
				continue;

			CDmElement *pControl = pAnimationSet->FindControl( pControlName );
			if ( !pControl )
				continue;

			int j = uniqueControls.AddToTail();
			ExportedControl_t &control = uniqueControls[j];
			control.m_Name = pControlName;
			control.m_bIsStereo = pControl->GetValue<bool>( "combo" );
			control.m_bIsMulti = pControl->GetValue<bool>( "multi" );
			control.m_nFirstIndex = nGlobalIndex;
			nGlobalIndex += 1 + control.m_bIsStereo + control.m_bIsMulti;
		}
	}
	return nGlobalIndex;
}


//-----------------------------------------------------------------------------
// Fileopen state machine
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	if ( bOpenFile )
	{
		pDialog->SetTitle( "Import Preset File", true );
	}
	else
	{
		pDialog->SetTitle( "Export Preset File", true );
	}

	char pPresetPath[MAX_PATH];
	if ( !Q_stricmp( pFileFormat, PRESET_FILE_FORMAT ) )
	{
		GetModSubdirectory( "models", pPresetPath, sizeof(pPresetPath) );
		pDialog->SetStartDirectoryContext( "preset_importexport", pPresetPath );
		pDialog->AddFilter( "*.*", "All Files (*.*)", false );
		pDialog->AddFilter( "*.pre", "Preset File (*.pre)", true, PRESET_FILE_FORMAT );
	}
	else if ( !Q_stricmp( pFileFormat, "vfe" ) )
	{
		GetModSubdirectory( "expressions", pPresetPath, sizeof(pPresetPath) );
		pDialog->SetStartDirectoryContext( "preset_exportvfe", pPresetPath );
		pDialog->AddFilter( "*.*", "All Files (*.*)", false );
		pDialog->AddFilter( "*.vfe", "Expression File (*.vfe)", true, "vfe" );
	}
	else if ( !Q_stricmp( pFileFormat, "txt" ) )
	{
		GetModSubdirectory( "expressions", pPresetPath, sizeof(pPresetPath) );
		pDialog->SetStartDirectoryContext( "preset_exportvfe", pPresetPath );
		pDialog->AddFilter( "*.*", "All Files (*.*)", false );
		pDialog->AddFilter( "*.txt", "Faceposer Expression File (*.txt)", true, "txt" );
	}
}

bool CDmePresetGroupEditorPanel::OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	CDmElement *pRoot;
	CDisableUndoScopeGuard sgRestore;
	DmFileId_t fileId = g_pDataModel->RestoreFromFile( pFileName, NULL, pFileFormat, &pRoot, CR_FORCE_COPY );
	sgRestore.Release();

	if ( fileId == DMFILEID_INVALID )
		return false;

	// When importing an entire group, we can do it all right here
	if ( !Q_stricmp( pContextKeyValues->GetName(), "ImportPresetGroup" ) )
	{
		CDmePresetGroup *pPresetGroup = CastElement< CDmePresetGroup >( pRoot );
		if ( !pPresetGroup )
			return false;

		CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Import Preset Group" );
		pPresetGroup->SetFileId( m_hAnimationSet->GetFileId(), TD_DEEP );
		m_hAnimationSet->GetPresetGroups( ).AddToTail( pPresetGroup );
		sg.Release();

		// Warn if we import a remap which doesn't exist
		CDmePresetRemap *pPresetRemap = pPresetGroup->GetPresetRemap();
		if ( pPresetRemap )
		{
			if ( m_hAnimationSet->FindPresetGroup( pPresetRemap->m_SourcePresetGroup ) == NULL )
			{
				char pBuf[512];
				Q_snprintf( pBuf, sizeof(pBuf), 
					"Import contains a remap which refers to an unknown preset group \"%s\"!\n", 
					pPresetRemap->m_SourcePresetGroup.Get() );
				vgui::MessageBox *pError = new vgui::MessageBox( "Bad source remap name!", pBuf, this );
				pError->DoModal();
			}
		}

		RefreshAnimationSet();
		NotifyDataChanged();
		return true;
	}

	CDmAttribute* pPresets = pRoot->GetAttribute( "presets", AT_ELEMENT_ARRAY );
	if ( !pPresets )
		return false;

	SetElementKeyValue( pContextKeyValues, "presets", pRoot );
	return true;
}

bool CDmePresetGroupEditorPanel::OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	// Used when exporting an entire preset group
	if ( !Q_stricmp( pContextKeyValues->GetName(), "ExportPresetGroup" ) )
	{
		CDmePresetGroup *pPresetGroup = GetElementKeyValue<CDmePresetGroup>( pContextKeyValues, "presetGroup" );
		if ( !pPresetGroup )
			return false;

		bool bOk = g_pDataModel->SaveToFile( pFileName, NULL, g_pDataModel->GetDefaultEncoding( pFileFormat ), pFileFormat, pPresetGroup );
		return bOk;
	}

	// Used when exporting an entire preset group
	if ( !Q_stricmp( pContextKeyValues->GetName(), "ExportPresetGroupToVFE" ) )
	{
		CDmePresetGroup *pPresetGroup = GetElementKeyValue<CDmePresetGroup>( pContextKeyValues, "presetGroup" );
		if ( !pPresetGroup )
			return false;

		bool bOk = pPresetGroup->ExportToVFE( pFileName, m_hAnimationSet );
		return bOk;
	}

	// Used when exporting an entire preset group
	if ( !Q_stricmp( pContextKeyValues->GetName(), "ExportPresetGroupToTXT" ) )
	{
		CDmePresetGroup *pPresetGroup = GetElementKeyValue<CDmePresetGroup>( pContextKeyValues, "presetGroup" );
		if ( !pPresetGroup )
			return false;

		bool bOk = pPresetGroup->ExportToTXT( pFileName, m_hAnimationSet );
		return bOk;
	}

	// Used when exporting a subset of a preset group
	int nCount = pContextKeyValues->GetInt( "count" );
	if ( nCount == 0 )
		return true;

	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	const char *pPresetGroupName = pPresetGroup ? pPresetGroup->GetName() : "root";

	CDisableUndoScopeGuard sg;
	CDmePresetGroup *pRoot = CreateElement< CDmePresetGroup >( pPresetGroupName, DMFILEID_INVALID );
	CDmaElementArray< CDmePreset >& presets = pRoot->GetPresets( );

	// Build list of selected presets 
	for ( int i = 0; i < nCount; ++i )
	{
		char pBuf[32];
		Q_snprintf( pBuf, sizeof(pBuf), "%d", i );
		CDmePreset *pPreset = GetElementKeyValue<CDmePreset>( pContextKeyValues, pBuf );
		presets.AddToTail( pPreset );
	}

	bool bOk = g_pDataModel->SaveToFile( pFileName, NULL, g_pDataModel->GetDefaultEncoding( pFileFormat ), pFileFormat, pRoot );
	g_pDataModel->DestroyElement( pRoot->GetHandle() );
	return bOk;
}


//-----------------------------------------------------------------------------
// Called when preset picking is cancelled
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnPresetPickCancelled( KeyValues *pParams )
{
	KeyValues *pContextKeyValues = pParams->FindKey( "ImportPicked" );
	if ( pContextKeyValues )
	{
		// Clean up the read-in file
		CDisableUndoScopeGuard sg;
		CDmElement *pRoot = GetElementKeyValue<CDmElement>( pContextKeyValues, "presets" );
		g_pDataModel->RemoveFileId( pRoot->GetFileId() );
		return;
	}
}


//-----------------------------------------------------------------------------
// Actually imports the presets from a file
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::ImportPresets( const CUtlVector< CDmePreset * >& presets )
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Import Presets" );

	int nPresetCount = presets.Count();
	for ( int i = 0; i < nPresetCount; ++i )
	{
		CDmePreset *pPreset = pPresetGroup->FindOrAddPreset( presets[i]->GetName() );
		const CDmaElementArray< CDmElement > &srcValues = presets[i]->GetControlValues( );
		CDmaElementArray< CDmElement > &values = pPreset->GetControlValues( );
		values.RemoveAll();

		int nValueCount = srcValues.Count();
		for ( int j = 0; j < nValueCount; ++j )
		{
			CDmElement *pSrcControlValue = srcValues[j];
			CDmElement *pControlValue = pSrcControlValue->Copy( );
			pControlValue->SetFileId( pPresetGroup->GetFileId(), TD_DEEP );
			values.AddToTail( pControlValue );
		}
	}

	RefreshAnimationSet();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// The 'export presets' context menu option
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnPresetPicked( KeyValues *pParams )
{
	CUtlVector< CDmePreset * > presets;
	int nCount = pParams->GetInt( "count" );
	if ( nCount == 0 )
		return;

	// Build list of selected presets 
	for ( int i = 0; i < nCount; ++i )
	{
		char pBuf[32];
		Q_snprintf( pBuf, sizeof(pBuf), "%d", i );
		CDmePreset *pPreset = GetElementKeyValue<CDmePreset>( pParams, pBuf );
		presets.AddToTail( pPreset );
	}

	if ( pParams->FindKey( "ExportPicked" ) )
	{
		KeyValues *pContextKeyValues = new KeyValues( "ExportPresets" );
		pContextKeyValues->SetInt( "count", nCount );
		for ( int i = 0; i < nCount; ++i )
		{
			char pBuf[32];
			Q_snprintf( pBuf, sizeof(pBuf), "%d", i );
			SetElementKeyValue( pContextKeyValues, pBuf, presets[i] );
		}

		m_hFileOpenStateMachine->SaveFile( pContextKeyValues, NULL, PRESET_FILE_FORMAT, vgui::FOSM_SHOW_PERFORCE_DIALOGS );
		return;
	}

	KeyValues *pContextKeyValues = pParams->FindKey( "ImportPicked" );
	if ( pContextKeyValues )
	{
		ImportPresets( presets );

		// Clean up the read-in file
		{
			CDisableUndoScopeGuard sg;
			CDmElement *pRoot = GetElementKeyValue<CDmElement>( pContextKeyValues, "presets" );
			g_pDataModel->RemoveFileId( pRoot->GetFileId() );
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// The 'export presets' context menu option
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnExportPresets()
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return;

	CPresetPickerFrame *pPresetPicker = new CPresetPickerFrame( this, "Select Preset(s) to Export" );
	pPresetPicker->AddActionSignalTarget( this );
	pPresetPicker->DoModal( pPresetGroup, true, new KeyValues( "ExportPicked" ) );
}


//-----------------------------------------------------------------------------
// The 'import presets' context menu option
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnImportPresets()
{
	KeyValues *pContextKeyValues = new KeyValues( "ImportPresets" );
	m_hFileOpenStateMachine->OpenFile( PRESET_FILE_FORMAT, pContextKeyValues );
}


//-----------------------------------------------------------------------------
// The 'export preset groups to VFE' context menu option
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnExportPresetGroupToVFE()
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return;

	KeyValues *pContextKeyValues = new KeyValues( "ExportPresetGroupToVFE" );
	SetElementKeyValue( pContextKeyValues, "presetGroup", pPresetGroup );
	m_hFileOpenStateMachine->SaveFile( pContextKeyValues, NULL, "vfe", vgui::FOSM_SHOW_PERFORCE_DIALOGS );
}


//-----------------------------------------------------------------------------
// The 'export preset groups to TXT' context menu option
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnExportPresetGroupToTXT()
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return;

	KeyValues *pContextKeyValues = new KeyValues( "ExportPresetGroupToTXT" );
	SetElementKeyValue( pContextKeyValues, "presetGroup", pPresetGroup );
	m_hFileOpenStateMachine->SaveFile( pContextKeyValues, NULL, "txt", vgui::FOSM_SHOW_PERFORCE_DIALOGS );
}


//-----------------------------------------------------------------------------
// The 'export preset groups' context menu option
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnExportPresetGroups()
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return;

	KeyValues *pContextKeyValues = new KeyValues( "ExportPresetGroup" );
	SetElementKeyValue( pContextKeyValues, "presetGroup", pPresetGroup );
	m_hFileOpenStateMachine->SaveFile( pContextKeyValues, NULL, PRESET_FILE_FORMAT, vgui::FOSM_SHOW_PERFORCE_DIALOGS );
}


//-----------------------------------------------------------------------------
// The 'import preset groups' context menu option
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnImportPresetGroups()
{
	KeyValues *pContextKeyValues = new KeyValues( "ImportPresetGroup" );
	m_hFileOpenStateMachine->OpenFile( PRESET_FILE_FORMAT, pContextKeyValues );
}


//-----------------------------------------------------------------------------
// Preset remap editor
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnRemoveDefaultControls()
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Remove Default Controls" );
	CDmrElementArray< CDmePreset > presets = pPresetGroup->GetPresets();
	int nPresetCount = presets.Count();
	for ( int i = 0; i < nPresetCount; ++i )
	{
		CDmePreset *pPreset = presets[i];
		CDmrElementArray< CDmElement > controls = pPreset->GetControlValues();	
		int nControlCount = controls.Count();
		for ( int j = nControlCount; --j >= 0; )
		{
			CDmElement *pControlValue = controls[j];
			CDmElement *pControl = m_hAnimationSet->FindControl( pControlValue->GetName() );
			if ( !pControl )
			{
				controls.Remove( j );
				continue;
			}

			bool bIsDefault = true;
			if ( pControl->GetValue<float>( "defaultValue" ) != pControlValue->GetValue<float>( "value" ) )
			{
				bIsDefault = false;
			}

			bool bIsStereo = pControl->GetValue<bool>( "combo" );
			if ( bIsStereo )
			{
				if ( pControl->GetValue<float>( "defaultBalance" ) != pControlValue->GetValue<float>( "balance" ) )
				{
					bIsDefault = false;
				}
			}

			bool bIsMulti = pControl->GetValue<bool>( "multi" );
			if ( bIsMulti )
			{
				if ( pControl->GetValue<float>( "defaultMultilevel" ) != pControlValue->GetValue<float>( "multilevel" ) )
				{
					bIsDefault = false;
				}
			}

			if ( bIsDefault )
			{
				controls.Remove( j );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Preset remap editor
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnEditPresetRemapping()
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return;

	CDmePresetRemapPanel *pPresetRemapPanel = new CDmePresetRemapPanel( this, "Manage Preset Remapping" );
	pPresetRemapPanel->DoModal( m_hAnimationSet, pPresetGroup );
}


//-----------------------------------------------------------------------------
// Perform preset remap
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnRemapPresets()
{
	CDmePresetGroup *pDestPresetGroup = GetSelectedPresetGroup();
	if ( !pDestPresetGroup || pDestPresetGroup->m_bIsReadOnly )
		return;

	CDmePresetRemap *pPresetRemap = pDestPresetGroup->GetPresetRemap();
	if ( !pPresetRemap )
		return;

	CDmePresetGroup *pSourcePresetGroup = m_hAnimationSet->FindPresetGroup( pPresetRemap->m_SourcePresetGroup );
	if ( !pSourcePresetGroup )
	{
		char pBuf[512];
		Q_snprintf( pBuf, sizeof(pBuf), "Unable to find preset group name %s in animation set %s!\n", 
			pPresetRemap->m_SourcePresetGroup.Get(), m_hAnimationSet->GetName() );
		vgui::MessageBox *pError = new vgui::MessageBox( "Bad source remap name!", pBuf, this );
		pError->DoModal();
		return;
	}

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Remap Presets" );

	int nCount = pPresetRemap->GetRemapCount();
	for ( int i = 0; i < nCount; ++i )
	{
		const char *pSourceName = pPresetRemap->GetRemapSource( i );
		CDmePreset *pSourcePreset = pSourcePresetGroup->FindPreset( pSourceName );

		const char *pDestName = pPresetRemap->GetRemapDest( i );
		CDmePreset *pDestPreset = pDestPresetGroup->FindPreset( pDestName );

		if ( !pSourcePreset || !pDestPreset )
			continue;

		pDestPreset->CopyControlValuesFrom( pSourcePreset );
	}

	sg.Release();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular preset
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnOpenPresetContextMenu( )
{
	if ( !m_hAnimationSet.Get() )
		return;

	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return;

	CDmePreset *pPreset = GetSelectedPreset();

	m_hContextMenu = new vgui::Menu( this, "ActionMenu" );

	// Can only export from read-only groups
	if ( pPresetGroup->m_bIsReadOnly )
	{
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ExportPresets", new KeyValues( "ExportPresets" ), this );
		vgui::Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
		return;
	}

	m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_AddPreset", new KeyValues( "AddPreset" ), this );
	if ( pPreset )
	{
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_RenamePreset", new KeyValues( "RenamePreset" ), this );
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_RemovePreset", new KeyValues( "RemovePreset" ), this );
		m_hContextMenu->AddSeparator();
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_MoveUp", new KeyValues( "MovePresetUp" ), this );
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_MoveDown", new KeyValues( "MovePresetDown" ), this );
	}

	m_hContextMenu->AddSeparator();
	m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ImportPresets", new KeyValues( "ImportPresets" ), this );
	m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ExportPresets", new KeyValues( "ExportPresets" ), this );

	vgui::Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnOpenContextMenu( KeyValues *kv )
{
	CleanupContextMenu();
	if ( !m_hAnimationSet.Get() )
		return;

	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pPresetList )
	{
		OnOpenPresetContextMenu();
		return;
	}

	if ( pPanel != m_pPresetGroupList )
		return;
    
	m_hContextMenu = new vgui::Menu( this, "ActionMenu" );
	m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_AddGroup", new KeyValues( "AddGroup" ), this );
	m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_AddPhonemeGroup", new KeyValues( "AddPhonemeGroup" ), this );

	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( pPresetGroup )
	{
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_RenameGroup", new KeyValues( "RenameGroup" ), this );
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_RemoveGroup", new KeyValues( "RemoveGroup" ), this );
		m_hContextMenu->AddSeparator();
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ToggleVisibility", new KeyValues( "ToggleGroupVisibility" ), this );
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ToggleSharing", new KeyValues( "ToggleGroupSharing" ), this );
		m_hContextMenu->AddSeparator();
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_MoveUp", new KeyValues( "MoveGroupUp" ), this );
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_MoveDown", new KeyValues( "MoveGroupDown" ), this );

		CDmePresetRemap *pPresetRemap = pPresetGroup->GetPresetRemap();
		bool bUseSeparator = !pPresetGroup->m_bIsReadOnly || pPresetRemap;
		if ( bUseSeparator )
		{
			m_hContextMenu->AddSeparator();
		}
		if ( !pPresetGroup->m_bIsReadOnly )
		{
			m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_RemoveDefaultControls", new KeyValues( "RemoveDefaultControls" ), this );
			m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_EditPresetRemapping", new KeyValues( "EditPresetRemapping" ), this );
		}
		if ( pPresetRemap )
		{
			m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_RemapPresets", new KeyValues( "RemapPresets" ), this );
		}
	}
	m_hContextMenu->AddSeparator();
	m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ImportPresets", new KeyValues( "ImportPresetGroups" ), this );
	if ( pPresetGroup )
	{
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ExportPresets", new KeyValues( "ExportPresetGroups" ), this );
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ExportPresetsToFaceposer", new KeyValues( "ExportPresetGroupsToTXT" ), this );
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ExportPresetsToExpression", new KeyValues( "ExportPresetGroupsToVFE" ), this );
	}

	vgui::Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnItemSelected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pPresetGroupList )
	{
		RefreshPresetNames();
		return;
	}
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnItemDeselected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pPresetGroupList )
	{
		RefreshPresetNames();
		return;
	}
}


//-----------------------------------------------------------------------------
// If it finds a duplicate control name, reports an error message and returns it found one
//-----------------------------------------------------------------------------
bool CDmePresetGroupEditorPanel::HasDuplicateGroupName( const char *pGroupName, CDmePresetGroup *pIgnorePreset )
{
	if ( !m_hAnimationSet )
		return false;

	CDmePresetGroup *pMatch = m_hAnimationSet->FindPresetGroup( pGroupName );
	if ( pMatch && pMatch != pIgnorePreset )
	{
		vgui::MessageBox *pError = new vgui::MessageBox( "#DmePresetGroupEditor_DuplicateNameTitle", "#DmePresetGroupEditor_DuplicateNameText", this );
		pError->DoModal();
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Called by OnInputCompleted after we get a new group name
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::PerformAddGroup( const char *pNewGroupName )
{
	if ( !m_hAnimationSet )
		return;

	if ( HasDuplicateGroupName( pNewGroupName ) ) 
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Add Preset Group" );
	CDmePresetGroup *pPresetGroup = m_hAnimationSet->FindOrAddPresetGroup( pNewGroupName );
	sg.Release();

	RefreshAnimationSet();
	SetSelectedPresetGroup( pPresetGroup );
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called by OnInputCompleted after we get a new group name
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::PerformAddPhonemeGroup( const char *pNewGroupName )
{
	if ( !m_hAnimationSet )
		return;

	if ( HasDuplicateGroupName( pNewGroupName ) ) 
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Add Phoneme Preset Group" );
	CDmePresetGroup *pPresetGroup = m_hAnimationSet->FindOrAddPresetGroup( pNewGroupName );

	int nPhonemeCount = NumPhonemes();
	for ( int i = 0; i < nPhonemeCount; ++i )
	{
		if ( !IsStandardPhoneme( i ) )
			continue;

		char pTempBuf[256];
		const char *pPhonemeName = NameForPhonemeByIndex( i );
		if ( !Q_stricmp( pPhonemeName, "<sil>" ) )
		{
			pPhonemeName = "silence";
		}
		Q_snprintf( pTempBuf, sizeof(pTempBuf), "p_%s", pPhonemeName );

		pPresetGroup->FindOrAddPreset( pTempBuf );
	}

	sg.Release();

	RefreshAnimationSet();
	SetSelectedPresetGroup( pPresetGroup );
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called by OnInputCompleted after we get a new group name
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::PerformRenameGroup( const char *pNewGroupName )
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return;

	if ( HasDuplicateGroupName( pNewGroupName, pPresetGroup ) ) 
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Rename Preset Group" );
	pPresetGroup->SetName( pNewGroupName );
	sg.Release();

	RefreshAnimationSet();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called by OnGroupControls after we get a new group name
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnInputCompleted( KeyValues *pKeyValues )
{
	const char *pName = pKeyValues->GetString( "text", NULL );
	if ( !pName || !pName[0] )						  
		return;

	if ( pKeyValues->FindKey( "OnAddGroup" ) )
	{
		PerformAddGroup( pName );
		return;
	}

	if ( pKeyValues->FindKey( "OnAddPhonemeGroup" ) )
	{
		PerformAddPhonemeGroup( pName );
		return;
	}

	if ( pKeyValues->FindKey( "OnRenameGroup" ) )
	{
		PerformRenameGroup( pName );
		return;
	}

	if ( pKeyValues->FindKey( "OnAddPreset" ) )
	{
		PerformAddPreset( pName );
		return;
	}

	if ( pKeyValues->FindKey( "OnRenamePreset" ) )
	{
		PerformRenamePreset( pName );
		return;
	}
}


//-----------------------------------------------------------------------------
// Toggle group visibility
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::ToggleGroupVisibility( CDmePresetGroup *pPresetGroup )
{
	if ( pPresetGroup )
	{
		CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Toggle Preset Group Visibility" );

		pPresetGroup->m_bIsVisible = !pPresetGroup->m_bIsVisible;
	}

	RefreshAnimationSet();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Ungroup controls from each other
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnToggleGroupVisibility( )
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	ToggleGroupVisibility( pPresetGroup );
}


//-----------------------------------------------------------------------------
// Ungroup controls from each other
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnToggleGroupSharing( )
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( pPresetGroup )
	{
		CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Toggle Preset Group Sharing" );

		pPresetGroup->SetShared( !pPresetGroup->IsShared() );
	}

	RefreshAnimationSet();
}


//-----------------------------------------------------------------------------
// Add a preset group
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnAddGroup()
{
	vgui::InputDialog *pInput = new vgui::InputDialog( this, "Add Preset Group", "Enter name of new preset group" );
	pInput->SetMultiline( false );
	pInput->DoModal( new KeyValues( "OnAddGroup" ) );
}


//-----------------------------------------------------------------------------
// Add a preset group
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnAddPhonemeGroup()
{
	vgui::InputDialog *pInput = new vgui::InputDialog( this, "Add Phoneme Preset Group", "Enter name of new preset group", "phoneme" );
	pInput->SetMultiline( false );
	pInput->DoModal( new KeyValues( "OnAddPhonemeGroup" ) );
}


//-----------------------------------------------------------------------------
// Rename a preset group
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnRenameGroup()
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return;

	vgui::InputDialog *pInput = new vgui::InputDialog( this, "Rename Preset Group", "Enter new name of preset group" );
	pInput->SetMultiline( false );
	pInput->DoModal( new KeyValues( "OnRenameGroup" ) );
}


//-----------------------------------------------------------------------------
// Remove a preset group
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnRemoveGroup()
{
	if ( !m_hAnimationSet.Get() )
		return;

	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return;

	if ( !Q_stricmp( pPresetGroup->GetName(), "procedural" ) )
	{
		vgui::MessageBox *pError = new vgui::MessageBox( "#DmePresetGroupEditor_CannotRemovePresetGroupTitle", "#DmePresetGroupEditor_CannotRemovePresetGroupText", this );
		pError->DoModal();
		return;
	}

	int nItemID = m_pPresetGroupList->GetSelectedItem( 0 );
	int nCurrentRow = m_pPresetGroupList->GetItemCurrentRow( nItemID );

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Remove Preset Group" );
	m_hAnimationSet->RemovePresetGroup( pPresetGroup );
	sg.Release();

	RefreshAnimationSet();
	if ( nCurrentRow >= m_pPresetGroupList->GetItemCount() )
	{
		--nCurrentRow;
	}
	if ( nCurrentRow >= 0 )
	{
		nItemID = m_pPresetGroupList->GetItemIDFromRow( nCurrentRow );
		m_pPresetGroupList->ClearSelectedItems();
		m_pPresetGroupList->AddSelectedItem( nItemID );
	}
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnMoveGroupUp()
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup || !m_hAnimationSet.Get() )
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Reorder Preset Groups" );
	m_hAnimationSet->MovePresetGroupUp( pPresetGroup );
	sg.Release();

	RefreshAnimationSet();
	SetSelectedPresetGroup( pPresetGroup );
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnMoveGroupDown()
{
	CDmePresetGroup *pPresetGroup = GetSelectedPresetGroup();
	if ( !pPresetGroup || !m_hAnimationSet.Get() )
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Reorder Preset Groups" );
	m_hAnimationSet->MovePresetGroupDown( pPresetGroup );
	sg.Release();

	RefreshAnimationSet();
	SetSelectedPresetGroup( pPresetGroup );
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Drag/drop reordering of preset groups
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::MovePresetGroupInFrontOf( CDmePresetGroup *pDragGroup, CDmePresetGroup *pDropGroup )
{
	if ( !m_hAnimationSet.Get() )
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Reorder Preset Groups" );
	m_hAnimationSet->MovePresetGroupInFrontOf( pDragGroup, pDropGroup );
	sg.Release();

	RefreshAnimationSet();
	SetSelectedPresetGroup( pDragGroup );
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Drag/drop preset moving
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::MovePresetIntoGroup( CDmePreset *pPreset, CDmePresetGroup *pGroup )
{
	if ( !m_hAnimationSet.Get() || !pPreset || !pGroup )
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Change Preset Group" );

	m_hAnimationSet->RemovePreset( pPreset );
	pGroup->GetPresets().AddToTail( pPreset );
	sg.Release();

	RefreshPresetNames();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
//
//
// CDmePresetGroupListPanel
//
// Declaration above because of scoping issues
//
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CDmePresetGroupListPanel::CDmePresetGroupListPanel( vgui::Panel *pParent, const char *pName, CDmePresetGroupEditorPanel *pComboPanel ) : 
	BaseClass( pParent, pName ), m_pPresetGroupPanel( pComboPanel )
{
}


//-----------------------------------------------------------------------------
// Handle keypresses
//-----------------------------------------------------------------------------
void CDmePresetGroupListPanel::OnMouseDoublePressed( vgui::MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		int x, y, row, column;
		vgui::input()->GetCursorPos( x, y );
		GetCellAtPos( x, y, row, column );
		int itemId = GetItemIDFromRow( row );
		KeyValues *pKeyValues = GetItem( itemId );
		CDmePresetGroup *pPresetGroup = GetElementKeyValue< CDmePresetGroup >( pKeyValues, "presetGroup" );
		m_pPresetGroupPanel->ToggleGroupVisibility( pPresetGroup );
		return;
	}

	BaseClass::OnMouseDoublePressed( code );
}


//-----------------------------------------------------------------------------
// Handle keypresses
//-----------------------------------------------------------------------------
void CDmePresetGroupListPanel::OnKeyCodeTyped( vgui::KeyCode code )
{
	if ( code == KEY_DELETE || code == KEY_BACKSPACE )
	{
		m_pPresetGroupPanel->OnRemoveGroup();
		return;
	}

	if ( vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT ) )
	{
		if ( code == KEY_UP )
		{
			m_pPresetGroupPanel->OnMoveGroupUp();
			return;
		}
		
		if ( code == KEY_DOWN )
		{
			m_pPresetGroupPanel->OnMoveGroupDown();
			return;
		}
	}

	vgui::ListPanel::OnKeyCodeTyped( code );
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmePresetGroupListPanel::OnCreateDragData( KeyValues *msg )
{
	CDmePresetGroup *pPresetGroup = m_pPresetGroupPanel->GetSelectedPresetGroup();
	if ( !pPresetGroup )
		return;

	SetElementKeyValue( msg, "presetGroup", pPresetGroup );
	msg->SetInt( "selfDroppable", 1 );
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
bool CDmePresetGroupListPanel::IsDroppable( CUtlVector< KeyValues * >& msgList )
{
	if ( msgList.Count() > 0 )
	{
		KeyValues *pData( msgList[ 0 ] );
		if ( m_pPresetGroupPanel )
		{
			CDmePresetGroup *pPresetGroup = GetElementKeyValue< CDmePresetGroup >( pData, "presetGroup" );
			if ( pPresetGroup )
				return true;

			CDmePreset *pPreset = GetElementKeyValue< CDmePreset >( pData, "preset" );
			if ( pPreset )
			{
				// Can't drop presets onto read-only preset groups
				int x, y, row, column;
				vgui::input()->GetCursorPos( x, y );
				GetCellAtPos( x, y, row, column );
				KeyValues *pKeyValues = GetItem( row );
				CDmePresetGroup *pDropGroup = pKeyValues ? GetElementKeyValue<CDmePresetGroup>( pKeyValues, "presetGroup" ) : NULL;

				if ( pDropGroup && !pDropGroup->m_bIsReadOnly )
					return true;
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmePresetGroupListPanel::OnPanelDropped( CUtlVector< KeyValues * >& msgList )
{
	if ( msgList.Count() == 0 )
		return;

	KeyValues *pData = msgList[ 0 ];
	if ( !m_pPresetGroupPanel )
		return;

	// Discover the cell the panel is over
	int x, y, row, column;
	vgui::input()->GetCursorPos( x, y );
	GetCellAtPos( x, y, row, column );
	KeyValues *pKeyValues = GetItem( row );

	CDmePresetGroup *pDragGroup = GetElementKeyValue<CDmePresetGroup>( pData, "presetGroup" );
	if ( pDragGroup )
	{
		CDmePresetGroup *pDropGroup = pKeyValues ? GetElementKeyValue<CDmePresetGroup>( pKeyValues, "presetGroup" ) : NULL;
		m_pPresetGroupPanel->MovePresetGroupInFrontOf( pDragGroup, pDropGroup );
		return;
	}

	CDmePreset *pDragPreset = GetElementKeyValue<CDmePreset>( pData, "preset" );
	if ( pDragPreset )
	{
		CDmePresetGroup *pDropGroup = pKeyValues ? GetElementKeyValue<CDmePresetGroup>( pKeyValues, "presetGroup" ) : NULL;
		if ( pDropGroup )
		{
			m_pPresetGroupPanel->MovePresetIntoGroup( pDragPreset, pDropGroup );
		}
		return;
	}
}


//-----------------------------------------------------------------------------
// Mouse is now over a droppable panel
//-----------------------------------------------------------------------------
void CDmePresetGroupListPanel::OnDroppablePanelPaint( CUtlVector< KeyValues * >& msglist, CUtlVector< Panel * >& dragPanels )
{ 
	// Discover the cell the panel is over
	int x, y, w, h, row, column;
	vgui::input()->GetCursorPos( x, y );
	GetCellAtPos( x, y, row, column );
	GetCellBounds( row, 0, x, y, w, h );

	int x2, y2, w2, h2;
	GetCellBounds( row, 3, x2, y2, w2, h2 );
	w = x2 + w2 - x;

	LocalToScreen( x, y );

	surface()->DrawSetColor( GetDropFrameColor() );

	// Draw insertion point
	surface()->DrawFilledRect( x,			y, x + w, y + 2 );
	surface()->DrawFilledRect( x,	y + h - 2, x + w, y + h );
	surface()->DrawFilledRect( x,			y, x + 2, y + h );
	surface()->DrawFilledRect( x + w - 2,	y, x + w, y + h );
}


//-----------------------------------------------------------------------------
//
//
// CDmePresetListPanel
//
// Declaration above because of scoping issues
//
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CDmePresetListPanel::CDmePresetListPanel( vgui::Panel *pParent, const char *pName, CDmePresetGroupEditorPanel *pComboPanel ) :
	BaseClass( pParent, pName ), m_pPresetGroupPanel( pComboPanel )
{
}

void CDmePresetListPanel::OnKeyCodeTyped( vgui::KeyCode code )
{
	CDmePresetGroup *pPresetGroup = m_pPresetGroupPanel->GetSelectedPresetGroup();
	if ( pPresetGroup && !pPresetGroup->m_bIsReadOnly )
	{
		if ( code == KEY_DELETE || code == KEY_BACKSPACE )
		{
			m_pPresetGroupPanel->OnRemovePreset();
			return;
		}

		// Not sure how to handle 'edit' mode... the relevant stuff is private
		if ( vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT ) )
		{
			if ( code == KEY_UP )
			{
				m_pPresetGroupPanel->OnMovePresetUp();
				return;
			}

			if ( code == KEY_DOWN )
			{
				m_pPresetGroupPanel->OnMovePresetDown();
				return;
			}
		}
	}	

	vgui::ListPanel::OnKeyCodeTyped( code );
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmePresetListPanel::OnCreateDragData( KeyValues *msg )
{
	CDmePresetGroup *pPresetGroup = m_pPresetGroupPanel->GetSelectedPresetGroup();
	if ( pPresetGroup->m_bIsReadOnly )
		return;

	CDmePreset *pPreset = m_pPresetGroupPanel->GetSelectedPreset();
	if ( !pPreset )
		return;

	SetElementKeyValue( msg, "preset", pPreset );
	msg->SetInt( "selfDroppable", 1 );
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
bool CDmePresetListPanel::IsDroppable( CUtlVector< KeyValues * >& msgList )
{
	if ( msgList.Count() > 0 )
	{
		KeyValues *pData( msgList[ 0 ] );
		if ( pData->GetPtr( "panel", NULL ) == this && m_pPresetGroupPanel )
		{
			CDmePreset *pPreset = GetElementKeyValue< CDmePreset >( pData, "preset" );
			if ( pPreset )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmePresetListPanel::OnPanelDropped( CUtlVector< KeyValues * >& msgList )
{
	if ( msgList.Count() == 0 )
		return;

	KeyValues *pData = msgList[ 0 ];
	if ( pData->GetPtr( "panel", NULL ) != this || !m_pPresetGroupPanel )
		return;

	// Discover the cell the panel is over
	int x, y, row, column;
	vgui::input()->GetCursorPos( x, y );
	GetCellAtPos( x, y, row, column );

	KeyValues *pKeyValues = GetItem( row );

	CDmePreset *pDragPreset = GetElementKeyValue<CDmePreset>( pData, "preset" );
	if ( pDragPreset )
	{
		CDmePreset *pDropPreset = pKeyValues ? GetElementKeyValue<CDmePreset>( pKeyValues, "preset" ) : NULL;
		m_pPresetGroupPanel->MovePresetInFrontOf( pDragPreset, pDropPreset );
		return;
	}
}


//-----------------------------------------------------------------------------
// Mouse is now over a droppable panel
//-----------------------------------------------------------------------------
void CDmePresetListPanel::OnDroppablePanelPaint( CUtlVector< KeyValues * >& msglist, CUtlVector< Panel * >& dragPanels )
{
	// Discover the cell the panel is over
	int x, y, w, h, row, column;
	vgui::input()->GetCursorPos( x, y );
	GetCellAtPos( x, y, row, column );
	GetCellBounds( row, column, x, y, w, h );
	LocalToScreen( x, y );

	surface()->DrawSetColor( GetDropFrameColor() );

	// Draw insertion point
	surface()->DrawFilledRect( x,			y, x + w, y + 2 );
	surface()->DrawFilledRect( x,	y + h - 2, x + w, y + h );
	surface()->DrawFilledRect( x,			y, x + 2, y + h );
	surface()->DrawFilledRect( x + w - 2,	y, x + w, y + h );
}



//-----------------------------------------------------------------------------
//
// Purpose: Combination system editor frame
//
//-----------------------------------------------------------------------------
CDmePresetGroupEditorFrame::CDmePresetGroupEditorFrame( vgui::Panel *pParent, const char *pTitle ) : 
	BaseClass( pParent, "DmePresetGroupEditorFrame" )
{
	SetDeleteSelfOnClose( true );
	m_pEditor = new CDmePresetGroupEditorPanel( this, "DmePresetGroupEditorPanel" );
	m_pEditor->AddActionSignalTarget( this );
	m_pOkButton = new vgui::Button( this, "OkButton", "#VGui_OK", this, "Ok" );
	SetBlockDragChaining( true );

	LoadControlSettingsAndUserConfig( "resource/dmepresetgroupeditorframe.res" );

	SetTitle( pTitle, false );
	g_pDataModel->InstallNotificationCallback( this );
}

CDmePresetGroupEditorFrame::~CDmePresetGroupEditorFrame()
{
	g_pDataModel->RemoveNotificationCallback( this );
}


//-----------------------------------------------------------------------------
// Sets the current scene + animation list
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorFrame::SetAnimationSet( CDmeAnimationSet *pComboSystem )
{
	m_pEditor->SetAnimationSet( pComboSystem );
}


//-----------------------------------------------------------------------------
// On command
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorFrame::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "Ok" ) )
	{
		CloseModal();
		return;
	}

	BaseClass::OnCommand( pCommand );
}


//-----------------------------------------------------------------------------
// Inherited from IDmNotify
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorFrame::NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	if ( !IsVisible() )
		return;

	if ( nNotifySource == NOTIFY_SOURCE_PRESET_GROUP_EDITOR )
		return;

	m_pEditor->RefreshAnimationSet();
}


//-----------------------------------------------------------------------------
// Chains notification messages from the contained panel to external clients
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorFrame::OnPresetsChanged()
{
	PostActionSignal( new KeyValues( "PresetsChanged" ) );
}

void CDmePresetGroupEditorFrame::OnAddNewPreset( KeyValues *pKeyValues )
{
	PostActionSignal( pKeyValues->MakeCopy() );
}


//-----------------------------------------------------------------------------
// Various command handlers related to the Edit menu
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorFrame::OnUndo()
{
	if ( g_pDataModel->CanUndo() )
	{
		CDisableUndoScopeGuard guard;
		g_pDataModel->Undo();
	}
}

void CDmePresetGroupEditorFrame::OnRedo()
{
	if ( g_pDataModel->CanRedo() )
	{
		CDisableUndoScopeGuard guard;
		g_pDataModel->Redo();
	}
}
