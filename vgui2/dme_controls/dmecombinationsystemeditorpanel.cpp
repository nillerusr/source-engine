//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "dme_controls/dmecombinationsystemeditorpanel.h"
#include "dme_controls/dmepanel.h"
#include "dme_controls/elementpropertiestree.h"
#include "dme_controls/dmecontrols_utils.h"
#include "movieobjects/dmecombinationoperator.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/Splitter.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/InputDialog.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/FileOpenDialog.h"
#include "vgui_controls/perforcefilelistframe.h"
#include "vgui/MouseCode.h"
#include "vgui/IInput.h"
#include "tier1/KeyValues.h"
#include "tier2/fileutils.h"


//-----------------------------------------------------------------------------
//
// Hook into the dme panel editor system
//
//-----------------------------------------------------------------------------
IMPLEMENT_DMEPANEL_FACTORY( CDmeCombinationSystemEditorPanel, DmeCombinationOperator, "DmeCombinationOperatorEditor", "Combination Operator Editor", true );


// Forward declaration
class CDmeCombinationControlsPanel;


//-----------------------------------------------------------------------------
// Import combination rules from this operator
//-----------------------------------------------------------------------------
static void ImportCombinationControls( CDmeCombinationOperator *pDestComboOp, CDmeCombinationOperator *pSrcComboOp, COperationFileListFrame *pStatusFrame )
{
	pDestComboOp->RemoveAllControls();

	// Iterate through all controls in the imported operator.
	// For each control that contains at least 1 raw controls
	// that also exist in this combination op, create a control here also.
	int nCount = pSrcComboOp->GetControlCount();
	for ( int i = 0; i < nCount; ++i )
	{
		const char *pControlName = pSrcComboOp->GetControlName( i );

		int nRawControls = pSrcComboOp->GetRawControlCount( i );
		int nMatchCount = 0; 
		bool *pFoundMatch = (bool*)_alloca( nRawControls * sizeof(bool) );
		for ( int j = 0; j < nRawControls; ++j )
		{
			const char *pRawControl = pSrcComboOp->GetRawControlName( i, j );
			pFoundMatch[j] = pDestComboOp->DoesTargetContainDeltaState( pRawControl );
			nMatchCount += pFoundMatch[j];
		}

		// No match? Don't import
		if ( nMatchCount == 0 )
		{
			pStatusFrame->AddOperation( pControlName, "No raw controls found!" ); 
			continue;
		}

		bool bPartialMatch = ( nMatchCount != nRawControls );
		pStatusFrame->AddOperation( pControlName, bPartialMatch ? "Partial rule match" : "Successful" ); 

		// Found a match! Let's create the control and potentially raw control
		bool bIsStereo = pSrcComboOp->IsStereoControl( i );
		bool bIsEyelid = pSrcComboOp->IsEyelidControl( i );
		ControlIndex_t index = pDestComboOp->FindOrCreateControl( pControlName, bIsStereo );
		pDestComboOp->SetEyelidControl( index, bIsEyelid );
		for ( int j = 0; j < nRawControls; ++j )
		{
			if ( pFoundMatch[j] )
			{
				const char *pRawControl = pSrcComboOp->GetRawControlName( i, j );
				float flWrinkleScale = pSrcComboOp->GetRawControlWrinkleScale( i, j );

				pDestComboOp->AddRawControl( index, pRawControl );
				pDestComboOp->SetWrinkleScale( index, pRawControl, flWrinkleScale );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Import dominance rules from this operator
//-----------------------------------------------------------------------------
static void ImportDominationRules( CDmeCombinationOperator *pDestComboOp, CDmeCombinationOperator *pSrcComboOp, COperationFileListFrame *pStatusFrame )
{
	pDestComboOp->RemoveAllDominationRules();

	// Now deal with dominance rules
	int nRuleCount = pSrcComboOp->DominationRuleCount();
	for ( int i = 0; i < nRuleCount; ++i )
	{
		bool bMismatch = false;

		// Only add dominance rule if *all* raw controls are present
		CDmeCombinationDominationRule *pSrcRule = pSrcComboOp->GetDominationRule( i );
		int nDominatorCount = pSrcRule->DominatorCount();
		for ( int j = 0; j < nDominatorCount; ++j )
		{
			const char *pDominatorName = pSrcRule->GetDominator( j );
			if ( !pDestComboOp->HasRawControl( pDominatorName ) )
			{
				bMismatch = true;
				pStatusFrame->AddOperation( pDominatorName, "Missing raw control for dominance rule" ); 
				break;
			}
		}

		int nSuppressedCount = pSrcRule->SuppressedCount();
		for ( int j = 0; j < nSuppressedCount; ++j )
		{
			const char *pSuppressedName = pSrcRule->GetSuppressed( j );
			if ( !pDestComboOp->HasRawControl( pSuppressedName ) )
			{
				bMismatch = true;
				pStatusFrame->AddOperation( pSuppressedName, "Missing raw control for dominance rule" ); 
				break;
			}
		}

		if ( bMismatch )
			continue;

		pDestComboOp->AddDominationRule( pSrcRule );
	}
}


//-----------------------------------------------------------------------------
// Called when the file open dialog for browsing source files selects something
//-----------------------------------------------------------------------------
static bool ImportCombinationData( vgui::Panel* pParent, CDmeCombinationOperator *pDestComboOp, KeyValues *kv )
{
	const char *pFileName = kv->GetString( "fullpath", NULL );
	if ( !pFileName )
		return false;

	CDmElement *pRoot;

	{
		CDisableUndoScopeGuard sg;
		g_pDataModel->RestoreFromFile( pFileName, NULL, NULL, &pRoot, CR_FORCE_COPY );
	}

	if ( !pRoot )
		return false;

	// Try to find a combination system in the file
	CDmeCombinationOperator *pComboOp = CastElement<CDmeCombinationOperator>( pRoot );
	if ( !pComboOp )
	{
		pComboOp = pRoot->GetValueElement< CDmeCombinationOperator >( "combinationOperator" );
	}

	if ( pComboOp )
	{
		// Actually rename the files, build an error dialog if necessary
		COperationFileListFrame *pStatusFrame = new COperationFileListFrame( pParent, 
			"Import Status", "Status", false, true );
		pStatusFrame->SetOperationColumnHeaderText( "Control Name" );

		CUndoScopeGuard sg( "Import Combination Rules" );
		if ( kv->FindKey( "ImportControls" ) )
		{
			ImportCombinationControls( pDestComboOp, pComboOp, pStatusFrame );
		}
		ImportDominationRules( pDestComboOp, pComboOp, pStatusFrame );
		sg.Release();

		pStatusFrame->DoModal();
	}

	CDisableUndoScopeGuard sg;
	g_pDataModel->UnloadFile( pRoot->GetFileId() );
	sg.Release();

	return true;
}


//-----------------------------------------------------------------------------
//
//
// CDmeInputControlListPanel
//
// Implementation below because of scoping issues
//
//
//-----------------------------------------------------------------------------
class CDmeInputControlListPanel : public vgui::ListPanel
{
	DECLARE_CLASS_SIMPLE( CDmeInputControlListPanel, vgui::ListPanel );

public:
	// constructor, destructor
	CDmeInputControlListPanel( vgui::Panel *pParent, const char *pName, CDmeCombinationControlsPanel *pComboPanel );

	virtual void OnCreateDragData( KeyValues *msg );
	virtual bool IsDroppable( CUtlVector< KeyValues * >& msgList );
	virtual void OnPanelDropped( CUtlVector< KeyValues * >& msgList );
	virtual void OnKeyCodeTyped( vgui::KeyCode code );

private:
	CDmeCombinationControlsPanel *m_pComboPanel;
};


//-----------------------------------------------------------------------------
//
//
// CDmeRawControlListPanel
//
// Implementation below because of scoping issues
//
//
//-----------------------------------------------------------------------------
class CDmeRawControlListPanel : public vgui::ListPanel
{
	DECLARE_CLASS_SIMPLE( CDmeRawControlListPanel, vgui::ListPanel );

public:
	// constructor, destructor
	CDmeRawControlListPanel( vgui::Panel *pParent, const char *pName, CDmeCombinationControlsPanel *pComboPanel );

	virtual void OnKeyCodeTyped( vgui::KeyCode code );
	virtual void OnMouseDoublePressed( vgui::MouseCode code );

private:
	MESSAGE_FUNC( OnNewWrinkleText, "TextNewLine" );	

	CDmeCombinationControlsPanel *m_pComboPanel;
	vgui::TextEntry *m_pWrinkleEdit;
	bool m_bIsWrinkle;
};


//-----------------------------------------------------------------------------
//
//
// Slider panel
//
//
//-----------------------------------------------------------------------------
class CDmeCombinationControlsPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CDmeCombinationControlsPanel, vgui::EditablePanel );

public:
	// constructor, destructor
	CDmeCombinationControlsPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CDmeCombinationControlsPanel();

	void SetCombinationOperator( CDmeCombinationOperator *pOp );
	CDmeCombinationOperator* GetCombinationOperator();
	void RefreshCombinationOperator();
	void NotifyDataChanged();

	const char *GetSelectedControlName();
	void MoveControlInFrontOf( const char *pDragControl, const char *pDropControl );

	void SetRawControlWrinkleValue( float flWrinkleValue );

	int GetSelectedInputControlItemId();
	void SelectedInputControlByItemId( int );

	MESSAGE_FUNC( OnMoveUpInputControl, "MoveUpInputControl" );
	MESSAGE_FUNC( OnMoveDownInputControl, "MoveDownInputControl" );
	MESSAGE_FUNC( OnMoveUp, "MoveUp" );
	MESSAGE_FUNC( OnMoveDown, "MoveDown" );

private:
	MESSAGE_FUNC_PARAMS( OnOpenContextMenu, "OpenContextMenu", kv );
	MESSAGE_FUNC_PARAMS( OnInputCompleted, "InputCompleted", kv );
	MESSAGE_FUNC( OnGroupControls, "GroupControls" );
	MESSAGE_FUNC( OnUngroupControls, "UngroupControls" );
	MESSAGE_FUNC( OnRenameControl, "RenameControl" );
	MESSAGE_FUNC( OnImportCombination, "ImportCombination" );
	MESSAGE_FUNC_PARAMS( OnFileSelected, "FileSelected", kv );
	MESSAGE_FUNC( OnToggleStereoControl, "ToggleStereoControl" );
	MESSAGE_FUNC( OnToggleEyelidControl, "ToggleEyelidControl" );
	MESSAGE_FUNC( OnToggleWrinkleType, "ToggleWrinkleType" );
	MESSAGE_FUNC_PARAMS( OnItemSelected, "ItemSelected", kv );	
	MESSAGE_FUNC_PARAMS( OnItemDeselected, "ItemDeselected", kv );	

	// Cleans up the context menu
	void CleanupContextMenu();

	// Builds a list of selected control + raw control names, returns true if any control is stereo
	void BuildSelectedControlLists( bool bOnlyGroupedControls, CUtlVector< CUtlString >& controlNames, CUtlVector< CUtlString >& rawControlNames, bool *pbStereo = NULL, bool *pbEyelid = NULL );

	// If it finds a duplicate control name, reports an error message and returns it found one
	bool HasDuplicateControlName( const char *pControlName, CUtlVector< CUtlString >& retiredControlNames );

	// Refreshes the list of raw controls
	void RefreshRawControlNames();

	// Called by OnGroupControls and OnRenameControl after we get a new group name
	void PerformGroupControls( const char *pGroupedControlName );

	// Called by OnGroupControls after we get a new group name
	void PerformRenameControl( const char *pNewControlName );

	// Called to open a context-sensitive menu for a particular menu item
	void OnOpenRawControlsContextMenu( );

	// Called to open a context-sensitive menu for a particular menu item
	const char* GetSelectedRawControl( ControlIndex_t &nControlIndex );

	CDmeHandle< CDmeCombinationOperator > m_hCombinationOperator;
	vgui::Splitter *m_pSplitter;
	CDmeInputControlListPanel *m_pControlList;
	CDmeRawControlListPanel *m_pRawControlList;
	vgui::DHANDLE< vgui::Menu > m_hContextMenu;
};


//-----------------------------------------------------------------------------
// Sort functions for list panel
//-----------------------------------------------------------------------------
static int __cdecl ControlNameSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString("name");
	const char *string2 = item2.kv->GetString("name");
	return Q_stricmp( string1, string2 );
}

static int __cdecl PeakSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	float flPeak1 = item1.kv->GetFloat("peak");
	float flPeak2 = item2.kv->GetFloat("peak");
	if ( flPeak1 < flPeak2 )
		return -1;
	if ( flPeak1 > flPeak2 )
		return 1;
	return 0;
}


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CDmeCombinationControlsPanel::CDmeCombinationControlsPanel( vgui::Panel *pParent, const char *pName ) :
	BaseClass( pParent, pName )
{
	m_pSplitter = new vgui::Splitter( this, "ControlsSplitter", vgui::SPLITTER_MODE_VERTICAL, 1 );
	vgui::Panel *pSplitterLeftSide = m_pSplitter->GetChild( 0 );
	vgui::Panel *pSplitterRightSide = m_pSplitter->GetChild( 1 );

	m_pControlList = new CDmeInputControlListPanel( pSplitterLeftSide, "ControlList", this );
	m_pControlList->AddColumnHeader( 0, "name", "Control Name", 150, 0 );
	m_pControlList->AddColumnHeader( 1, "stereo", "Stereo", 70, 0 );
	m_pControlList->AddColumnHeader( 2, "eyelid", "Eyelid", 70, 0 );
	m_pControlList->AddColumnHeader( 3, "default", "Default", 52, 0 );
	m_pControlList->SetSelectIndividualCells( false );
	m_pControlList->SetMultiselectEnabled( true );
	m_pControlList->SetEmptyListText( "No controls" );
	m_pControlList->AddActionSignalTarget( this );
	m_pControlList->SetSortFunc( 0, NULL );
	m_pControlList->SetColumnSortable( 0, false );
	m_pControlList->SetSortFunc( 1, NULL );
	m_pControlList->SetColumnSortable( 1, false );
	m_pControlList->SetSortFunc( 2, NULL );
	m_pControlList->SetColumnSortable( 2, false );
	m_pControlList->SetSortFunc( 3, NULL );
	m_pControlList->SetColumnSortable( 3, false );
	m_pControlList->SetDragEnabled( true );
	m_pControlList->SetDragEnabled( true );
	m_pControlList->SetDropEnabled( true );

	m_pRawControlList = new CDmeRawControlListPanel( pSplitterRightSide, "RawControlList", this );
	m_pRawControlList->AddColumnHeader( 0, "name", "Raw Control Name", 150, 0 );
	m_pRawControlList->AddColumnHeader( 1, "peak", "Peak", 52, 0 );
	m_pRawControlList->AddColumnHeader( 2, "wrinkletype", "Wrinkle Type", 100, 0 );
	m_pRawControlList->AddColumnHeader( 3, "wrinkle", "Wrinkle Amount", 100, 0 );
	m_pRawControlList->SetSelectIndividualCells( false );
	m_pRawControlList->SetEmptyListText( "No raw controls" );
	m_pRawControlList->AddActionSignalTarget( this );
	m_pRawControlList->SetSortFunc( 0, ControlNameSortFunc );
	m_pRawControlList->SetSortFunc( 1, PeakSortFunc );
	m_pRawControlList->SetSortFunc( 2, NULL );
	m_pRawControlList->SetColumnSortable( 2, false );
	m_pRawControlList->SetSortFunc( 3, NULL );
	m_pRawControlList->SetColumnSortable( 3, false );
	m_pRawControlList->SetSortColumn( 1 );
}


CDmeCombinationControlsPanel::~CDmeCombinationControlsPanel()
{
	CleanupContextMenu();
	SaveUserConfig();
}


//-----------------------------------------------------------------------------
// Cleans up the context menu
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::CleanupContextMenu()
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
void CDmeCombinationControlsPanel::SetCombinationOperator( CDmeCombinationOperator *pOp )
{
	if ( pOp != m_hCombinationOperator.Get() )
	{
		m_hCombinationOperator = pOp;
		RefreshCombinationOperator();
	}
}

CDmeCombinationOperator* CDmeCombinationControlsPanel::GetCombinationOperator()
{
	return m_hCombinationOperator;
}


//-----------------------------------------------------------------------------
// Builds the control list for the combination operator
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::RefreshCombinationOperator()
{
	const CUtlString controlName = GetSelectedControlName();

	m_pControlList->RemoveAll();	
	if ( !m_hCombinationOperator.Get() )
		return;
				  
	int nCount = m_hCombinationOperator->GetControlCount();
	for ( int i = 0; i < nCount; ++i )
	{
		bool bIsMultiControl = m_hCombinationOperator->GetRawControlCount(i) > 1;
		float flDefault = m_hCombinationOperator->GetRawControlCount(i) == 2 ? 0.5f : 0.0f;
		const char *pName = m_hCombinationOperator->GetControlName( i );
		KeyValues *kv = new KeyValues( "node", "name", pName );
		kv->SetString( "stereo", m_hCombinationOperator->IsStereoControl(i) ? "On" : "Off" ); 
		kv->SetString( "eyelid", m_hCombinationOperator->IsEyelidControl(i) ? "On" : "Off" ); 
		kv->SetFloat( "default", flDefault ); 
		kv->SetColor( "cellcolor", bIsMultiControl ? Color( 192, 192, 0, 255 ) : Color( 255, 255, 255, 255 ) ); 
		const int nItemId = m_pControlList->AddItem( kv, 0, false, false );

		if ( !Q_strcmp( controlName.Get(), pName ) )
		{
			m_pControlList->SetSingleSelectedItem( nItemId );
		}
	}

	RefreshRawControlNames();
}


//-----------------------------------------------------------------------------
// Tells any class that cares that the data in this thing has changed
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::NotifyDataChanged()
{
	PostActionSignal( new KeyValues( "DmeElementChanged", "DmeCombinationControlsPanel", 1 ) );
}


//-----------------------------------------------------------------------------
// Refreshes the list of raw controls
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::RefreshRawControlNames()
{
	m_pRawControlList->RemoveAll();	
	if ( !m_hCombinationOperator.Get() )
		return;

	int nSelectedItemCount = m_pControlList->GetSelectedItemsCount();
	if ( nSelectedItemCount != 1 )
		return;

	int nItemID = m_pControlList->GetSelectedItem( 0 );

	KeyValues *pKeyValues = m_pControlList->GetItem( nItemID );
	const char *pControlName = pKeyValues->GetString( "name" );
	ControlIndex_t nControlIndex = m_hCombinationOperator->FindControlIndex( pControlName );
	if ( nControlIndex < 0 )
		return;

	int nCount = m_hCombinationOperator->GetRawControlCount( nControlIndex );
	for ( int i = 0; i < nCount; ++i )
	{
		KeyValues *kv = new KeyValues( "node", "name", m_hCombinationOperator->GetRawControlName( nControlIndex, i ) );
		switch( nCount )
		{
		case 0:
		case 1:
			kv->SetFloat( "peak", 1.0f );
			break;

		case 2:
			kv->SetFloat( "peak", i == 0 ? 0.0f : 1.0f );
			break;

		default:
			kv->SetFloat( "peak", (float)i / (nCount - 1) );
			break;
		}

		float flWrinkleScale = m_hCombinationOperator->GetRawControlWrinkleScale( nControlIndex, i );
		kv->SetString( "wrinkletype", ( flWrinkleScale < 0.0f ) ? "Compress" : "Stretch" );
		kv->SetFloat( "wrinkle", fabs( flWrinkleScale ) );
		m_pRawControlList->AddItem( kv, 0, false, false );
	}

	m_pRawControlList->SortList();
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
const char* CDmeCombinationControlsPanel::GetSelectedRawControl( ControlIndex_t &nControlIndex )
{
	if ( !m_hCombinationOperator.Get() )
		return NULL;

	nControlIndex = -1;

	int nSelectedItemCount = m_pControlList->GetSelectedItemsCount();
	if ( nSelectedItemCount != 1 )
		return NULL;

	int nSelectedRawItemCount = m_pRawControlList->GetSelectedItemsCount();
	if ( nSelectedRawItemCount != 1 )
		return NULL;

	int nItemID = m_pControlList->GetSelectedItem( 0 );
	KeyValues *pKeyValues = m_pControlList->GetItem( nItemID );
	const char *pControlName = pKeyValues->GetString( "name" );
	nControlIndex = m_hCombinationOperator->FindControlIndex( pControlName );

	nItemID = m_pRawControlList->GetSelectedItem( 0 );
	pKeyValues = m_pRawControlList->GetItem( nItemID );
	return pKeyValues->GetString( "name" );
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
const char *CDmeCombinationControlsPanel::GetSelectedControlName()
{
	if ( !m_hCombinationOperator.Get() )
		return NULL;

	int nSelectedItemCount = m_pControlList->GetSelectedItemsCount();
	if ( nSelectedItemCount != 1 )
		return NULL;

	const int nItemId = m_pControlList->GetSelectedItem( 0 );
	if ( !m_pControlList->IsValidItemID( nItemId ) )
		return NULL;

	KeyValues *pKeyValues = m_pControlList->GetItem( nItemId );
	if ( !pKeyValues )
		return NULL;

	return pKeyValues->GetString( "name" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmeCombinationControlsPanel::GetSelectedInputControlItemId()
{
	return m_pControlList->GetSelectedItem( 0 );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::SelectedInputControlByItemId( int nItemId )
{
	m_pControlList->SetSingleSelectedItem( nItemId );
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnMoveUp()
{
	ControlIndex_t nControlIndex;
	const char *pRawControlName = GetSelectedRawControl( nControlIndex );
	if ( !pRawControlName )
		return;

	m_hCombinationOperator->MoveRawControlUp( nControlIndex, pRawControlName );
	RefreshRawControlNames();
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnMoveDown()
{
	ControlIndex_t nControlIndex;
	const char *pRawControlName = GetSelectedRawControl( nControlIndex );
	if ( !pRawControlName )
		return;

	m_hCombinationOperator->MoveRawControlDown( nControlIndex, pRawControlName );
	RefreshRawControlNames();

	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnMoveUpInputControl()
{
	const char *pControlName = GetSelectedControlName();
	if ( !pControlName )
		return;

	m_hCombinationOperator->MoveControlUp( pControlName );
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnMoveDownInputControl()
{
	const char *pControlName = GetSelectedControlName();
	if ( !pControlName )
		return;

	m_hCombinationOperator->MoveControlDown( pControlName );
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::MoveControlInFrontOf(
	const char *pDragControl,
	const char *pDropControl )
{
	m_hCombinationOperator->MoveControlBefore( pDragControl, pDropControl );
	RefreshCombinationOperator();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Toggles the wrinkle type
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnToggleWrinkleType()
{
	ControlIndex_t nControlIndex;
	const char *pRawControlName = GetSelectedRawControl( nControlIndex );
	if ( !pRawControlName )
		return;

	float flWrinkleScale = m_hCombinationOperator->GetRawControlWrinkleScale( nControlIndex, pRawControlName );
	m_hCombinationOperator->SetWrinkleScale( nControlIndex, pRawControlName, -flWrinkleScale );
	RefreshRawControlNames();
	m_hCombinationOperator->GenerateWrinkleDeltas();
}


void CDmeCombinationControlsPanel::SetRawControlWrinkleValue( float flWrinkleValue )
{
	ControlIndex_t nControlIndex;
	const char *pRawControlName = GetSelectedRawControl( nControlIndex );
	if ( !pRawControlName )
		return;

	m_hCombinationOperator->SetWrinkleScale( nControlIndex, pRawControlName, flWrinkleValue );
	RefreshRawControlNames();
	m_hCombinationOperator->GenerateWrinkleDeltas();
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnOpenRawControlsContextMenu( )
{
	if ( !m_hCombinationOperator.Get() )
		return;

	int nSelectedItemCount = m_pControlList->GetSelectedItemsCount();
	if ( nSelectedItemCount != 1 )
		return;

	int nSelectedRawItemCount = m_pRawControlList->GetSelectedItemsCount();
	if ( nSelectedRawItemCount != 1 )
		return;

	m_hContextMenu = new vgui::Menu( this, "ActionMenu" );
	m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_MoveUp", new KeyValues( "MoveUp" ), this );
	m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_MoveDown", new KeyValues( "MoveDown" ), this );
	m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_ToggleWrinkleType", new KeyValues( "ToggleWrinkleType" ), this );

	vgui::Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnOpenContextMenu( KeyValues *kv )
{
	CleanupContextMenu();
	if ( !m_hCombinationOperator.Get() )
		return;

	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pRawControlList )
	{
		OnOpenRawControlsContextMenu();
		return;
	}

	if ( pPanel != m_pControlList )
		return;
    
	bool bGroupedControls = false;
	bool bStereoControls = false;
	bool bEyelidControls = false;
	int nSelectedItemCount = m_pControlList->GetSelectedItemsCount();
	for ( int i = 0; i < nSelectedItemCount; ++i )
	{
		int nItemID = m_pControlList->GetSelectedItem( i );

		KeyValues *pKeyValues = m_pControlList->GetItem( nItemID );
		const char *pControlName = pKeyValues->GetString( "name" );
		ControlIndex_t nControlIndex = m_hCombinationOperator->FindControlIndex( pControlName );
		if ( nControlIndex < 0 )
			continue;

		if ( m_hCombinationOperator->GetRawControlCount( nControlIndex ) > 1 )
		{
			bGroupedControls = true;
		}

		if ( m_hCombinationOperator->IsStereoControl( nControlIndex ) )
		{
			bStereoControls = true;
		}

		if ( m_hCombinationOperator->IsEyelidControl( nControlIndex ) )
		{
			bEyelidControls = true;
		}
	}

	m_hContextMenu = new vgui::Menu( this, "ActionMenu" );

	if ( nSelectedItemCount > 1 )
	{
		m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_GroupControls", new KeyValues( "GroupControls" ), this );
	}

	if ( bGroupedControls )
	{
		m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_UngroupControls", new KeyValues( "UngroupControls" ), this );
	}

	if ( nSelectedItemCount >= 1 )
	{
		int nMenuItemID = m_hContextMenu->AddCheckableMenuItem( "#DmeCombinationSystemEditor_StereoControl",  new KeyValues( "ToggleStereoControl" ), this );
		m_hContextMenu->SetMenuItemChecked( nMenuItemID, bStereoControls );
	}

	if ( nSelectedItemCount >= 1 )
	{
		int nMenuItemID = m_hContextMenu->AddCheckableMenuItem( "#DmeCombinationSystemEditor_EyelidControl",  new KeyValues( "ToggleEyelidControl" ), this );
		m_hContextMenu->SetMenuItemChecked( nMenuItemID, bEyelidControls );
	}

	if ( nSelectedItemCount == 1 )
	{
		m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_RenameControl", new KeyValues( "RenameControl" ), this );
	}

	if ( nSelectedItemCount >= 1 )
	{
		m_hContextMenu->AddSeparator();
		m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_MoveUp", new KeyValues( "MoveUpInputControl" ), this );
		m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_MoveDown", new KeyValues( "MoveDownInputControl" ), this );
	}

	if ( nSelectedItemCount >= 1 || bGroupedControls )
	{
		m_hContextMenu->AddSeparator();
	}
	m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_Import", new KeyValues( "ImportCombination" ), this );

	vgui::Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnItemSelected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pControlList )
	{
		RefreshRawControlNames();
		return;
	}
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnItemDeselected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pControlList )
	{
		RefreshRawControlNames();
		return;
	}
}


//-----------------------------------------------------------------------------
// Builds a list of selected control + raw control names, returns true if any control is stereo
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::BuildSelectedControlLists(
	bool bOnlyGroupedControls,
	CUtlVector< CUtlString >& controlNames, CUtlVector< CUtlString >& rawControlNames,
	bool *pbStereo, bool *pbEyelid )
{
	bool bIsStereo = false;
	bool bIsEyelid = false;
	int nSelectedItemCount = m_pControlList->GetSelectedItemsCount();
	for ( int i = 0; i < nSelectedItemCount; ++i )
	{
		int nItemID = m_pControlList->GetSelectedItem( i );

		KeyValues *pKeyValues = m_pControlList->GetItem( nItemID );
		const char *pControlName = pKeyValues->GetString( "name" );
		ControlIndex_t nControlIndex = m_hCombinationOperator->FindControlIndex( pControlName );
		if ( nControlIndex < 0 )
			continue;

		int nRawControlCount = m_hCombinationOperator->GetRawControlCount( nControlIndex );
		if ( bOnlyGroupedControls && ( nRawControlCount <= 1 ) )
			continue;

		if ( m_hCombinationOperator->IsStereoControl( nControlIndex ) )
		{
			bIsStereo = true;
		}

		if ( m_hCombinationOperator->IsEyelidControl( nControlIndex ) )
		{
			bIsEyelid = true;
		}

		controlNames.AddToTail( pControlName );
		for ( int j = 0; j < nRawControlCount; ++j )
		{
			rawControlNames.AddToTail( m_hCombinationOperator->GetRawControlName( nControlIndex, j ) );
		}
	}

	if ( pbStereo )
	{
		*pbStereo = bIsStereo;
	}

	if ( pbEyelid )
	{
		*pbEyelid = bIsEyelid;
	}
}


//-----------------------------------------------------------------------------
// If it finds a duplicate control name, reports an error message and returns it found one
//-----------------------------------------------------------------------------
bool CDmeCombinationControlsPanel::HasDuplicateControlName( const char *pControlName, CUtlVector< CUtlString >& retiredControlNames )
{
	int i;
	int nRetiredControlNameCount = retiredControlNames.Count();
	for ( i = 0; i < nRetiredControlNameCount; ++i )
	{
		if ( !Q_stricmp( retiredControlNames[i], pControlName ) )
			break;
	}
	if ( i == nRetiredControlNameCount )
	{
		// no match
		if ( m_hCombinationOperator->FindControlIndex( pControlName ) >= 0 )
		{
			vgui::MessageBox *pError = new vgui::MessageBox( "#DmeCombinationSystemEditor_DuplicateNameTitle", "#DmeCombinationSystemEditor_DuplicateNameText", this );
			pError->DoModal();
			return true;
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Called by OnGroupControls and OnRenameControl after we get a new group name
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::PerformGroupControls( const char *pGroupedControlName )
{
	int nSelectedItemCount = m_pControlList->GetSelectedItemsCount();
	if ( nSelectedItemCount <= 1 )
		return;

	// Build lists of selected controls + raw controls
	CUtlVector< CUtlString > controlNames;
	CUtlVector< CUtlString > rawControlNames;
	bool bIsStereo = false;
	bool bIsEyelid = false;
	BuildSelectedControlLists( false, controlNames, rawControlNames, &bIsStereo, &bIsEyelid );

	// NOTE: It's illegal to use a grouped control name which already exists
	// assuming it's not in the list of grouped control names we're going to group together
	if ( HasDuplicateControlName( pGroupedControlName, controlNames ) )
		return;

	// Delete old controls
	int nRetiredControlNameCount = controlNames.Count();
	for ( int i = 0; i < nRetiredControlNameCount; ++i )
	{
		m_hCombinationOperator->RemoveControl( controlNames[i] );
	}

	// Create new control
	ControlIndex_t nNewControl = m_hCombinationOperator->FindOrCreateControl( pGroupedControlName, bIsStereo );
	m_hCombinationOperator->SetEyelidControl( nNewControl, bIsEyelid );
	int nGroupedControlCount = rawControlNames.Count();
	for ( int i = 0; i < nGroupedControlCount; ++i )
	{
		m_hCombinationOperator->AddRawControl( nNewControl, rawControlNames[i] );
	}

	RefreshCombinationOperator();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called by OnGroupControls after we get a new group name
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::PerformRenameControl( const char *pNewControlName )
{
	int nSelectedItemCount = m_pControlList->GetSelectedItemsCount();
	if ( nSelectedItemCount != 1 )
		return;

	int nItemID = m_pControlList->GetSelectedItem( 0 );
	KeyValues *pKeyValues = m_pControlList->GetItem( nItemID );
	const char *pControlName = pKeyValues->GetString( "name" );
	ControlIndex_t nControlIndex = m_hCombinationOperator->FindControlIndex( pControlName );
	if ( nControlIndex < 0 )
		return;

	// NOTE: It's illegal to use a grouped control name which already exists
	// assuming it's not in the list of grouped control names we're going to group together
	ControlIndex_t nFoundIndex = m_hCombinationOperator->FindControlIndex( pNewControlName );
	if ( nFoundIndex >= 0 && nFoundIndex != nControlIndex )
		return;

	m_hCombinationOperator->SetControlName( nControlIndex, pNewControlName );

	RefreshCombinationOperator();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called by OnGroupControls after we get a new group name
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnInputCompleted( KeyValues *pKeyValues )
{
	const char *pControlName = pKeyValues->GetString( "text", NULL );
	if ( !pControlName || !pControlName[0] )						  
		return;

	if ( pKeyValues->FindKey( "OnGroupControls" ) )
	{
		PerformGroupControls( pControlName );
		return;
	}

	if ( pKeyValues->FindKey( "OnRenameControl" ) )
	{
		PerformRenameControl( pControlName );
		return;
	}
}


//-----------------------------------------------------------------------------
// Group controls together
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnGroupControls( )
{
	vgui::InputDialog *pInput = new vgui::InputDialog( this, "Group Controls", "Enter name of grouped control" );
	pInput->SetMultiline( false );
	pInput->DoModal( new KeyValues( "OnGroupControls" ) );
}


//-----------------------------------------------------------------------------
// Ungroup controls from each other
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnUngroupControls( )
{
	// Build lists of selected controls + raw controls
	CUtlVector< CUtlString > controlNames;
	CUtlVector< CUtlString > rawControlNames;
	bool bIsStereo = false;
	bool bIsEyelid = false;
	BuildSelectedControlLists( true, controlNames, rawControlNames, &bIsStereo, &bIsEyelid );

	// NOTE: It's illegal to use a grouped control name which already exists
	// assuming it's not in the list of grouped control names we're going to group together
	int nRawControlCount = rawControlNames.Count();
	for ( int i = 0; i < nRawControlCount; ++i )
	{
		if ( HasDuplicateControlName( rawControlNames[i], controlNames ) )
			return;
	}

	// Delete old controls
	int nRetiredControlNameCount = controlNames.Count();
	for ( int i = 0; i < nRetiredControlNameCount; ++i )
	{
		m_hCombinationOperator->RemoveControl( controlNames[i] );
	}

	// Create new control (this will also create raw controls with the same name)
	int nGroupedControlCount = rawControlNames.Count();
	for ( int i = 0; i < nGroupedControlCount; ++i )
	{
		const int nControlIndex = m_hCombinationOperator->FindOrCreateControl( rawControlNames[i], bIsStereo, true );
		m_hCombinationOperator->SetEyelidControl( nControlIndex, bIsEyelid );
	}

	RefreshCombinationOperator();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Ungroup controls from each other
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnToggleStereoControl( )
{
	// Build lists of selected controls + raw controls
	// Yeah, this isn't super efficient, but this UI is not going to be super-polished
	CUtlVector< CUtlString > controlNames;
	CUtlVector< CUtlString > rawControlNames;

	bool bIsStereo = false;
	BuildSelectedControlLists( false, controlNames, rawControlNames, &bIsStereo );

	int nControlCount = controlNames.Count();
	for ( int i = 0; i < nControlCount; ++i )
	{
		ControlIndex_t nControlIndex = m_hCombinationOperator->FindControlIndex( controlNames[i] );
		m_hCombinationOperator->SetStereoControl( nControlIndex, !bIsStereo );
	}

	RefreshCombinationOperator();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Toggle Eyelid-Ness
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnToggleEyelidControl()
{
	// Build lists of selected controls + raw controls
	// Yeah, this isn't super efficient, but this UI is not going to be super-polished
	CUtlVector< CUtlString > controlNames;
	CUtlVector< CUtlString > rawControlNames;

	bool bIsEyelid = false;
	BuildSelectedControlLists( false, controlNames, rawControlNames, NULL, &bIsEyelid );

	int nControlCount = controlNames.Count();
	for ( int i = 0; i < nControlCount; ++i )
	{
		ControlIndex_t nControlIndex = m_hCombinationOperator->FindControlIndex( controlNames[i] );
		m_hCombinationOperator->SetEyelidControl( nControlIndex, !bIsEyelid );
	}

	RefreshCombinationOperator();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Rename a control
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnRenameControl()
{
	int nSelectedItemCount = m_pControlList->GetSelectedItemsCount();
	if ( nSelectedItemCount != 1 )
		return;

	vgui::InputDialog *pInput = new vgui::InputDialog( this, "Rename Control", "Enter new name of control" );
	pInput->SetMultiline( false );
	pInput->DoModal( new KeyValues( "OnRenameControl" ) );
}


//-----------------------------------------------------------------------------
// Called when the file open dialog for browsing source files selects something
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnFileSelected( KeyValues *kv )
{
	if ( ImportCombinationData( this, m_hCombinationOperator, kv ) )
	{
		RefreshCombinationOperator();
		NotifyDataChanged();
	}
}

	
//-----------------------------------------------------------------------------
// Import combination controls + domination rules
//-----------------------------------------------------------------------------
void CDmeCombinationControlsPanel::OnImportCombination()
{
	char pStartingDir[MAX_PATH];
	GetModContentSubdirectory( "models", pStartingDir, sizeof(pStartingDir) );

	vgui::FileOpenDialog *pDialog = new vgui::FileOpenDialog( this, "Select File to Import", true, new KeyValues( "ImportControls" ) );
	pDialog->SetStartDirectoryContext( "combination_system_import", pStartingDir );
	pDialog->AddFilter( "*.dmx", "Exported model file (*.dmx)", true );
	pDialog->SetDeleteSelfOnClose( true );
	pDialog->AddActionSignalTarget( this );
	pDialog->DoModal( false );
}


//-----------------------------------------------------------------------------
//
//
// CDmeInputControlListPanel
//
// Declaration above because of scoping issues
//
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CDmeInputControlListPanel::CDmeInputControlListPanel( vgui::Panel *pParent, const char *pName, CDmeCombinationControlsPanel *pComboPanel ) : 
	BaseClass( pParent, pName ), m_pComboPanel( pComboPanel )
{
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmeInputControlListPanel::OnCreateDragData( KeyValues *msg )
{
	const char *const pControlName = m_pComboPanel->GetSelectedControlName();
	if ( pControlName )
	{
		msg->SetString( "inputControl", pControlName );
		msg->SetInt( "selfDroppable", 1 );
	}
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
bool CDmeInputControlListPanel::IsDroppable( CUtlVector< KeyValues * >& msgList )
{
	if ( msgList.Count() > 0 )
	{
		KeyValues *pData( msgList[ 0 ] );
		if ( pData->GetPtr( "panel", NULL ) == this && m_pComboPanel )
		{
			if ( pData->GetString( "inputControl" ) && pData->GetInt( "selfDroppable" ) )
			{
				return true;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmeInputControlListPanel::OnPanelDropped( CUtlVector< KeyValues * >& msgList )
{
	if ( msgList.Count() > 0 )
	{
		KeyValues *pData( msgList[ 0 ] );
		if ( pData->GetPtr( "panel", NULL ) == this && m_pComboPanel )
		{
			const char *const pDragControl( pData->GetString( "inputControl" ) );
			if ( pDragControl )
			{
				int x;
				int y;
				
				vgui::input()->GetCursorPos( x, y );

				int row;
				int column;
				GetCellAtPos( x, y, row, column );

				KeyValues *pKeyValues = GetItem( GetItemIDFromRow( row ) );
				if ( pKeyValues )
				{
					const char *pDropControl = pKeyValues->GetString( "name" );
					if ( pDropControl )
					{
						m_pComboPanel->MoveControlInFrontOf( pDragControl, pDropControl );
					}
				}
			}
		}
	}
}


void CDmeInputControlListPanel::OnKeyCodeTyped( vgui::KeyCode code )
{
	// Not sure how to handle 'edit' mode... the relevant stuff is private
	if ( vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT ) )
	{
		if ( code == KEY_UP )
		{
			const int nItemId = m_pComboPanel->GetSelectedInputControlItemId();
			m_pComboPanel->OnMoveUpInputControl();
			vgui::ListPanel::OnKeyCodeTyped( code );
			m_pComboPanel->SelectedInputControlByItemId( nItemId );
			return;
		}
		else if ( code == KEY_DOWN )
		{
			const int nItemId = m_pComboPanel->GetSelectedInputControlItemId();
			m_pComboPanel->OnMoveDownInputControl();
			vgui::ListPanel::OnKeyCodeTyped( code );
			m_pComboPanel->SelectedInputControlByItemId( nItemId );
			return;
		}
	}

	vgui::ListPanel::OnKeyCodeTyped( code );
}


//-----------------------------------------------------------------------------
//
//
// CDmeRawControlListPanel
//
// Declaration above because of scoping issues
//
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CDmeRawControlListPanel::CDmeRawControlListPanel( vgui::Panel *pParent, const char *pName, CDmeCombinationControlsPanel *pComboPanel ) :
	BaseClass( pParent, pName ), m_pComboPanel( pComboPanel )
{
	m_pWrinkleEdit = new vgui::TextEntry( this, "WrinkleEdit" );
	m_pWrinkleEdit->SetVisible( false );
	m_pWrinkleEdit->AddActionSignalTarget( this );
	m_pWrinkleEdit->SetAllowNumericInputOnly( true );
}

void CDmeRawControlListPanel::OnKeyCodeTyped( vgui::KeyCode code )
{
	// Not sure how to handle 'edit' mode... the relevant stuff is private
	if ( vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT ) )
	{
		if ( code == KEY_UP )
		{
			m_pComboPanel->OnMoveUp();
		}
		else if ( code == KEY_DOWN )
		{
			m_pComboPanel->OnMoveDown();
		}
	}

	vgui::ListPanel::OnKeyCodeTyped( code );
}

void CDmeRawControlListPanel::OnMouseDoublePressed( vgui::MouseCode code )
{
	if ( code != MOUSE_LEFT )
	{
		BaseClass::OnMouseDoublePressed( code );
		return;
	}

	int nNumSelected = GetSelectedItemsCount();
	if ( IsInEditMode() || nNumSelected != 1 )
		return;

	m_pWrinkleEdit->SetVisible( true );
	m_pWrinkleEdit->SendNewLine( true );

	// Always edit column 3, which contains the wrinkle amount
	int nEditingItem = GetSelectedItem( 0 );
	KeyValues *pKeyValues = GetItem( nEditingItem );
	float flWrinkleValue = pKeyValues->GetFloat( "wrinkle" );

	m_bIsWrinkle = !Q_stricmp( pKeyValues->GetString( "wrinkletype" ), "Wrinkle" );

	char buf[64];
	Q_snprintf( buf, sizeof(buf), "%f", flWrinkleValue );
	m_pWrinkleEdit->SetText( buf );

	EnterEditMode( nEditingItem, 3, m_pWrinkleEdit );
}

void CDmeRawControlListPanel::OnNewWrinkleText()
{
	LeaveEditMode();

	char szEditText[MAX_PATH];
	m_pWrinkleEdit->GetText( szEditText, MAX_PATH );
	m_pWrinkleEdit->SetVisible( false );

	float flWrinkleScale = atof( szEditText );
	if ( m_bIsWrinkle )
	{
		flWrinkleScale = -flWrinkleScale;
	}
	m_pComboPanel->SetRawControlWrinkleValue( flWrinkleScale );
}


//-----------------------------------------------------------------------------
//
// Purpose: Multiselect for raw controls
//
//-----------------------------------------------------------------------------
class CRawControlPickerFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CRawControlPickerFrame, vgui::Frame );

public:
	CRawControlPickerFrame( vgui::Panel *pParent, const char *pTitle );
	~CRawControlPickerFrame();

	// Sets the current scene + animation list
	void DoModal( CDmeCombinationOperator *pCombinationOperator, CDmeCombinationDominationRule *pRule, bool bSuppressed, KeyValues *pContextKeyValues );

	// Inherited from Frame
	virtual void OnCommand( const char *pCommand );

private:
	// Refreshes the list of raw controls
	void RefreshRawControlNames( CDmeCombinationOperator *pCombinationOperator, CDmeCombinationDominationRule *pRule, bool bSuppressed );
	void CleanUpMessage();

	vgui::ListPanel *m_pRawControlList;
	vgui::Button *m_pOpenButton;
	vgui::Button *m_pCancelButton;
	KeyValues *m_pContextKeyValues;
};

CRawControlPickerFrame::CRawControlPickerFrame( vgui::Panel *pParent, const char *pTitle ) : 
	BaseClass( pParent, "RawControlPickerFrame" )
{
	SetDeleteSelfOnClose( true );
	m_pContextKeyValues = NULL;

	m_pRawControlList = new vgui::ListPanel( this, "RawControlList" );
	m_pRawControlList->AddColumnHeader( 0, "name", "Raw Control Name", 52, 0 );
	m_pRawControlList->SetSelectIndividualCells( false );
	m_pRawControlList->SetEmptyListText( "No raw controls" );
	m_pRawControlList->AddActionSignalTarget( this );
	m_pRawControlList->SetSortFunc( 0, ControlNameSortFunc );
	m_pRawControlList->SetSortColumn( 0 );

	m_pOpenButton = new vgui::Button( this, "OkButton", "#MessageBox_OK", this, "Ok" );
	m_pCancelButton = new vgui::Button( this, "CancelButton", "#MessageBox_Cancel", this, "Cancel" );
	SetBlockDragChaining( true );

	LoadControlSettingsAndUserConfig( "resource/dmecombinationsystemeditor_rawcontrolpickerframe.res" );

	SetTitle( pTitle, false );
}

CRawControlPickerFrame::~CRawControlPickerFrame()
{
	CleanUpMessage();
}


//-----------------------------------------------------------------------------
// Refreshes the list of raw controls
//-----------------------------------------------------------------------------
void CRawControlPickerFrame::RefreshRawControlNames( CDmeCombinationOperator *pCombinationOperator, CDmeCombinationDominationRule *pRule, bool bChooseSuppressed )
{
	m_pRawControlList->RemoveAll();	
	if ( !pCombinationOperator )
		return;

	int nCount = pCombinationOperator->GetRawControlCount( );
	for ( int i = 0; i < nCount; ++i )
	{
		const char *pRawControl = pCombinationOperator->GetRawControlName( i );

		// Hide controls that are in the other part of the rule
		bool bIsDominator = pRule->HasDominatorControl( pRawControl );
		bool bIsSuppressed = pRule->HasSuppressedControl( pRawControl );
		Assert( !bIsDominator || !bIsSuppressed );
		if ( ( bChooseSuppressed && bIsDominator ) || ( !bChooseSuppressed && bIsSuppressed ) )
			continue;

		KeyValues *kv = new KeyValues( "node", "name", pCombinationOperator->GetRawControlName( i ) );
		int nItemID = m_pRawControlList->AddItem( kv, 0, false, false );
		if ( ( bChooseSuppressed && bIsSuppressed ) || ( !bChooseSuppressed && bIsDominator ) )
		{
			m_pRawControlList->AddSelectedItem( nItemID );
		}
	}

	m_pRawControlList->SortList();
}


//-----------------------------------------------------------------------------
// Deletes the message
//-----------------------------------------------------------------------------
void CRawControlPickerFrame::CleanUpMessage()
{
	if ( m_pContextKeyValues )
	{
		m_pContextKeyValues->deleteThis();
		m_pContextKeyValues = NULL;
	}
}


//-----------------------------------------------------------------------------
// Sets the current scene + animation list
//-----------------------------------------------------------------------------
void CRawControlPickerFrame::DoModal( CDmeCombinationOperator *pCombinationOperator, 
	CDmeCombinationDominationRule *pRule, bool bSuppressed, KeyValues *pContextKeyValues )
{
	CleanUpMessage();
	RefreshRawControlNames( pCombinationOperator, pRule, bSuppressed );
	m_pContextKeyValues = pContextKeyValues;
	BaseClass::DoModal();
}


//-----------------------------------------------------------------------------
// On command
//-----------------------------------------------------------------------------
void CRawControlPickerFrame::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "Ok" ) )
	{
		KeyValues *pActionKeys = new KeyValues( "RawControlPicked" );
		KeyValues *pControlList = pActionKeys->FindKey( "rawControls", true );

		int nSelectedItemCount = m_pRawControlList->GetSelectedItemsCount();
		for ( int i = 0; i < nSelectedItemCount; ++i )
		{
			int nItemID = m_pRawControlList->GetSelectedItem( i );
			KeyValues *pKeyValues = m_pRawControlList->GetItem( nItemID );
			const char *pControlName = pKeyValues->GetString( "name" );

			pControlList->SetString( pControlName, pControlName );
		}

		if ( m_pContextKeyValues )
		{
			pActionKeys->AddSubKey( m_pContextKeyValues );

			// This prevents them from being deleted later
			m_pContextKeyValues = NULL;
		}

		PostActionSignal( pActionKeys );
		CloseModal();
		return;
	}

	if ( !Q_stricmp( pCommand, "Cancel" ) )
	{
		CloseModal();
		return;
	}

	BaseClass::OnCommand( pCommand );
}


//-----------------------------------------------------------------------------
// Domination rules panel
//-----------------------------------------------------------------------------
class CDmeCombinationDominationRulesPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CDmeCombinationDominationRulesPanel, vgui::EditablePanel );

public:
	// constructor, destructor
	CDmeCombinationDominationRulesPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CDmeCombinationDominationRulesPanel();

	void SetCombinationOperator( CDmeCombinationOperator *pOp );
	void RefreshCombinationOperator();
	void NotifyDataChanged();

private:
	MESSAGE_FUNC_PARAMS( OnOpenContextMenu, "OpenContextMenu", kv );
	MESSAGE_FUNC_PARAMS( OnRawControlPicked, "RawControlPicked", kv );
	MESSAGE_FUNC( OnAddDominationRule, "AddDominationRule" );
	MESSAGE_FUNC( OnRemoveDominationRule, "RemoveDominationRule" );
	MESSAGE_FUNC( OnDuplicateSuppressed, "DuplicateSuppressed" );
	MESSAGE_FUNC( OnDuplicateDominators, "DuplicateDominators" );
	MESSAGE_FUNC( OnSelectDominators, "SelectDominators" );
	MESSAGE_FUNC( OnSelectSuppressed, "SelectSuppressed" );
	MESSAGE_FUNC( OnMoveUp, "MoveUp" );
	MESSAGE_FUNC( OnMoveDown, "MoveDown" );
	MESSAGE_FUNC( OnImportDominationRules, "ImportDominationRules" );
	MESSAGE_FUNC_PARAMS( OnFileSelected, "FileSelected", kv );

	// Cleans up the context menu
	void CleanupContextMenu();

	// Selects a particular domination rule
	void SelectRule( CDmeCombinationDominationRule* pRule );

	// Returns the currently selected rule
	CDmeCombinationDominationRule* GetSelectedRule( );

	CDmeHandle< CDmeCombinationOperator > m_hCombinationOperator;
	vgui::ListPanel *m_pDominationRulesList;
	vgui::DHANDLE< vgui::Menu > m_hContextMenu;
};


static int __cdecl DominatorNameSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	int i1 = item1.kv->GetInt("index");
	int i2 = item2.kv->GetInt("index");
	return i1 - i2;
}


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CDmeCombinationDominationRulesPanel::CDmeCombinationDominationRulesPanel( vgui::Panel *pParent, const char *pName ) :
	BaseClass( pParent, pName )
{
	m_pDominationRulesList = new vgui::ListPanel( this, "DominationRulesList" );
	m_pDominationRulesList->AddColumnHeader( 0, "suppressed", "Suppress", 100, 0 );
	m_pDominationRulesList->AddColumnHeader( 1, "dominator", "Dominate", 100, 0 );
	m_pDominationRulesList->AddActionSignalTarget( this );
	m_pDominationRulesList->SetSortFunc( 0, DominatorNameSortFunc );
	m_pDominationRulesList->SetSortFunc( 1, DominatorNameSortFunc );
	m_pDominationRulesList->SetSortColumn( 0 );
	m_pDominationRulesList->SetEmptyListText("No domination rules.");
	m_pDominationRulesList->SetMultiselectEnabled( false );
}

CDmeCombinationDominationRulesPanel::~CDmeCombinationDominationRulesPanel()
{
	CleanupContextMenu();
	SaveUserConfig();
}


//-----------------------------------------------------------------------------
// Cleans up the context menu
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRulesPanel::CleanupContextMenu()
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
void CDmeCombinationDominationRulesPanel::SetCombinationOperator( CDmeCombinationOperator *pOp )
{
	if ( pOp != m_hCombinationOperator.Get() )
	{
		m_hCombinationOperator = pOp;
		RefreshCombinationOperator();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Basic sort function, for use in qsort
//-----------------------------------------------------------------------------
static int __cdecl ControlNameSortFunc(const void *elem1, const void *elem2)
{
	const char *pItem1 = *((const char **) elem1);
	const char *pItem2 = *((const char **) elem2);
	return Q_stricmp( pItem1, pItem2 );
}

	
//-----------------------------------------------------------------------------
// Builds the list of animations
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRulesPanel::RefreshCombinationOperator()
{
	m_pDominationRulesList->RemoveAll();
	if ( !m_hCombinationOperator.Get() )
		return;

	char pTemp[1024];
	int nCount = m_hCombinationOperator->DominationRuleCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeCombinationDominationRule *pRule = m_hCombinationOperator->GetDominationRule( i );

		KeyValues *pItemKeys = new KeyValues( "node" );

		int nLen = 0;
		int nControlCount = pRule->DominatorCount();
		pTemp[0] = 0;
		const char **ppStrings = (const char**)_alloca( nControlCount * sizeof(const char*) );
		for ( int j = 0; j < nControlCount; ++j )
		{
			ppStrings[j] = pRule->GetDominator(j);
		}
		qsort( ppStrings, (size_t)nControlCount, (size_t)sizeof(char*), ControlNameSortFunc );
		for ( int j = 0; j < nControlCount; ++j )
		{
			nLen += Q_snprintf( &pTemp[nLen], sizeof(pTemp) - nLen, "%s ", ppStrings[j] );
		}

		pItemKeys->SetString( "dominator", pTemp ); 

		nLen = 0;
		nControlCount = pRule->SuppressedCount();
		pTemp[0] = 0;
		ppStrings = (const char**)_alloca( nControlCount * sizeof(const char*) );
		for ( int j = 0; j < nControlCount; ++j )
		{
			ppStrings[j] = pRule->GetSuppressed(j);
		}
		qsort( ppStrings, (size_t)nControlCount, (size_t)sizeof(char*), ControlNameSortFunc );
		for ( int j = 0; j < nControlCount; ++j )
		{
			nLen += Q_snprintf( &pTemp[nLen], sizeof(pTemp) - nLen, "%s ", ppStrings[j] );
		}
		pItemKeys->SetString( "suppressed", pTemp ); 
		pItemKeys->SetInt( "index", i ); 
		SetElementKeyValue( pItemKeys, "rule", pRule );

		m_pDominationRulesList->AddItem( pItemKeys, 0, false, false );
	}

	m_pDominationRulesList->SortList();
}


void CDmeCombinationDominationRulesPanel::NotifyDataChanged()
{
	PostActionSignal( new KeyValues( "DmeElementChanged", "DmeCombinationDominationRulesPanel", 2 ) );
}


//-----------------------------------------------------------------------------
// Returns the currently selected domination rule
//-----------------------------------------------------------------------------
CDmeCombinationDominationRule* CDmeCombinationDominationRulesPanel::GetSelectedRule( )
{
	if ( !m_hCombinationOperator.Get() )
		return NULL;

	int nSelectedItemCount = m_pDominationRulesList->GetSelectedItemsCount();
	if ( nSelectedItemCount != 1 )
		return NULL;

	int nItemID = m_pDominationRulesList->GetSelectedItem( 0 );
	KeyValues *pKeyValues = m_pDominationRulesList->GetItem( nItemID );
	return GetElementKeyValue<CDmeCombinationDominationRule>( pKeyValues, "rule" );
}


//-----------------------------------------------------------------------------
// Reorder rules
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRulesPanel::OnMoveUp( )
{
	CDmeCombinationDominationRule* pRule = GetSelectedRule( );
	if ( !pRule )
		return;

	m_hCombinationOperator->MoveDominationRuleUp( pRule );
	RefreshCombinationOperator();
	NotifyDataChanged();
}

void CDmeCombinationDominationRulesPanel::OnMoveDown( )
{
	CDmeCombinationDominationRule* pRule = GetSelectedRule( );
	if ( !pRule )
		return;

	m_hCombinationOperator->MoveDominationRuleDown( pRule );
	RefreshCombinationOperator();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called when the file open dialog for browsing source files selects something
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRulesPanel::OnFileSelected( KeyValues *kv )
{
	if ( ImportCombinationData( this, m_hCombinationOperator, kv ) )
	{
		RefreshCombinationOperator();
		NotifyDataChanged();
	}
}


//-----------------------------------------------------------------------------
// Import domination rules
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRulesPanel::OnImportDominationRules()
{
	char pStartingDir[MAX_PATH];
	GetModContentSubdirectory( "models", pStartingDir, sizeof(pStartingDir) );

	vgui::FileOpenDialog *pDialog = new vgui::FileOpenDialog( this, "Select File to Import", true, new KeyValues( "ImportDominationRules" ) );
	pDialog->SetStartDirectoryContext( "combination_system_import", pStartingDir );
	pDialog->AddFilter( "*.dmx", "Exported model file (*.dmx)", true );
	pDialog->SetDeleteSelfOnClose( true );
	pDialog->AddActionSignalTarget( this );
	pDialog->DoModal( false );
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRulesPanel::OnOpenContextMenu( KeyValues *kv )
{
	CleanupContextMenu();
	if ( !m_hCombinationOperator.Get() )
		return;

	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel != m_pDominationRulesList )
		return;

	int nSelectedItemCount = m_pDominationRulesList->GetSelectedItemsCount();

	m_hContextMenu = new vgui::Menu( this, "ActionMenu" );
	m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_AddDominationRule", new KeyValues( "AddDominationRule" ), this );
	if ( nSelectedItemCount > 0 )
	{
		m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_RemoveDominationRule", new KeyValues( "RemoveDominationRule" ), this );
	}

	if ( nSelectedItemCount == 1 )
	{
		m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_DuplicateSuppressed", new KeyValues( "DuplicateSuppressed" ), this );
		m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_DuplicateDominators", new KeyValues( "DuplicateDominators" ), this );
		m_hContextMenu->AddSeparator();
		m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_SelectSuppressed", new KeyValues( "SelectSuppressed" ), this );
		m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_SelectDominators", new KeyValues( "SelectDominators" ), this );
		m_hContextMenu->AddSeparator();
		m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_MoveUp", new KeyValues( "MoveUp" ), this );
		m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_MoveDown", new KeyValues( "MoveDown" ), this );
	}

	m_hContextMenu->AddSeparator();
	m_hContextMenu->AddMenuItem( "#DmeCombinationSystemEditor_ImportDomination", new KeyValues( "ImportDominationRules" ), this );

	vgui::Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}


//-----------------------------------------------------------------------------
// Selects a particular domination rule
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRulesPanel::SelectRule( CDmeCombinationDominationRule* pRule )
{
	for ( int nItemID = m_pDominationRulesList->FirstItem(); nItemID != m_pDominationRulesList->InvalidItemID(); nItemID = m_pDominationRulesList->NextItem( nItemID ) )
	{
		KeyValues *pKeyValues = m_pDominationRulesList->GetItem( nItemID );
		if ( pRule == GetElementKeyValue<CDmeCombinationDominationRule>( pKeyValues, "rule" ) )
		{
			m_pDominationRulesList->SetSingleSelectedItem( nItemID );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Called by the context menu to add dominator rules
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRulesPanel::OnAddDominationRule( )
{
	CDmeCombinationDominationRule* pRule = m_hCombinationOperator->AddDominationRule();
	RefreshCombinationOperator();
	SelectRule( pRule );
	OnSelectSuppressed();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Duplicate suppressed
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRulesPanel::OnDuplicateSuppressed()
{
	CDmeCombinationDominationRule *pSrcRule = GetSelectedRule();
	if ( !pSrcRule )
		return;

	CDmeCombinationDominationRule* pRule = m_hCombinationOperator->AddDominationRule();
	RefreshCombinationOperator();
	SelectRule( pRule );
	for ( int i = 0; i < pSrcRule->SuppressedCount(); ++i )
	{
		pRule->AddSuppressed( pSrcRule->GetSuppressed( i ) );
	}
	OnSelectDominators();
	NotifyDataChanged();
}

void CDmeCombinationDominationRulesPanel::OnDuplicateDominators()
{
	CDmeCombinationDominationRule *pSrcRule = GetSelectedRule();
	if ( !pSrcRule )
		return;

	CDmeCombinationDominationRule* pRule = m_hCombinationOperator->AddDominationRule();
	RefreshCombinationOperator();
	SelectRule( pRule );
	for ( int i = 0; i < pSrcRule->DominatorCount(); ++i )
	{
		pRule->AddDominator( pSrcRule->GetDominator( i ) );
	}
	OnSelectSuppressed();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called by the context menu to remove dominator rules
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRulesPanel::OnRemoveDominationRule( )
{
	int nSelectedItemCount = m_pDominationRulesList->GetSelectedItemsCount();
	for ( int i = 0; i < nSelectedItemCount; ++i )
	{
		int nItemID = m_pDominationRulesList->GetSelectedItem( i );
		KeyValues *pKeyValues = m_pDominationRulesList->GetItem( nItemID );

		CDmeCombinationDominationRule *pRule = GetElementKeyValue<CDmeCombinationDominationRule>( pKeyValues, "rule" );
		if ( pRule )
		{
			m_hCombinationOperator->RemoveDominationRule( pRule );
		}
	}

	RefreshCombinationOperator();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called by the pickers made in OnSelectDominators + OnSelectSuppressed
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRulesPanel::OnRawControlPicked( KeyValues *pKeyValues )
{
	KeyValues *pControlList = pKeyValues->FindKey( "rawControls" );
	if ( !pControlList )
		return;

	KeyValues *pContextKeys = pKeyValues->FindKey( "OnSelectDominators" );
	if ( pContextKeys )
	{
		CDmeCombinationDominationRule *pRule = GetElementKeyValue<CDmeCombinationDominationRule>( pContextKeys, "rule" );
		if ( !pRule )
			return;

		pRule->RemoveAllDominators();
		for ( KeyValues *pKey = pControlList->GetFirstValue(); pKey; pKey = pKey->GetNextValue() )
		{
			pRule->AddDominator( pKey->GetName() );
		}
		RefreshCombinationOperator();
		NotifyDataChanged();
		return;
	}

	pContextKeys = pKeyValues->FindKey( "OnSelectSuppressed" );
	if ( pContextKeys )
	{
		CDmeCombinationDominationRule *pRule = GetElementKeyValue<CDmeCombinationDominationRule>( pContextKeys, "rule" );
		if ( !pRule )
			return;

		pRule->RemoveAllSuppressed();
		for ( KeyValues *pKey = pControlList->GetFirstValue(); pKey; pKey = pKey->GetNextValue() )
		{
			pRule->AddSuppressed( pKey->GetName() );
		}
		RefreshCombinationOperator();
		NotifyDataChanged();
		return;
	}
}


//-----------------------------------------------------------------------------
// Called by the context menu to change dominator rules
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRulesPanel::OnSelectDominators( )
{
	CDmeCombinationDominationRule *pRule = GetSelectedRule();
	if ( !pRule )
		return;

	KeyValues *pContextKeyValues =  new KeyValues( "OnSelectDominators" );
	SetElementKeyValue( pContextKeyValues, "rule", pRule );

	CRawControlPickerFrame *pPicker = new CRawControlPickerFrame( this, "Select Dominator(s)" );
	pPicker->DoModal( m_hCombinationOperator, pRule, false, pContextKeyValues );
}

void CDmeCombinationDominationRulesPanel::OnSelectSuppressed( )
{
	CDmeCombinationDominationRule *pRule = GetSelectedRule();
	if ( !pRule )
		return;

	KeyValues *pContextKeyValues =  new KeyValues( "OnSelectSuppressed" );
	SetElementKeyValue( pContextKeyValues, "rule", pRule );

	CRawControlPickerFrame *pPicker = new CRawControlPickerFrame( this, "Select Suppressed" );
	pPicker->DoModal( m_hCombinationOperator, pRule, true, pContextKeyValues );
}


//-----------------------------------------------------------------------------
//
// Combination system editor panel
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CDmeCombinationSystemEditorPanel::CDmeCombinationSystemEditorPanel( vgui::Panel *pParent, const char *pName ) : BaseClass( pParent, pName )
{
	m_pEditorSheet = new vgui::PropertySheet( this, "EditorSheet" );
	m_pEditorSheet->AddActionSignalTarget( this );

	m_pControlsPage = new vgui::PropertyPage( m_pEditorSheet, "ControlsPage" );
	m_pDominationRulesPage = new vgui::PropertyPage( m_pEditorSheet, "DominationRulesPage" );
	m_pPropertiesPage = new vgui::PropertyPage( m_pEditorSheet, "PropertiesPage" );

	m_pControlsPanel = new CDmeCombinationControlsPanel( (vgui::Panel*)NULL, "CombinationControls" );
	m_pControlsPanel->AddActionSignalTarget( this );

	m_pDominationRulesPanel = new CDmeCombinationDominationRulesPanel( (vgui::Panel*)NULL, "DominationRules" );
	m_pDominationRulesPanel->AddActionSignalTarget( this );

	m_pPropertiesPanel = new CDmeElementPanel( (vgui::Panel*)NULL, "PropertiesPanel" );
	m_pPropertiesPanel->AddActionSignalTarget( this );

	m_pControlsPanel->LoadControlSettingsAndUserConfig( "resource/dmecombinationsystemeditorpanel_controlspage.res" );
	m_pDominationRulesPanel->LoadControlSettingsAndUserConfig( "resource/dmecombinationsystemeditorpanel_dominationpage.res" );

	// Load layout settings; has to happen before pinning occurs in code
	LoadControlSettingsAndUserConfig( "resource/dmecombinationsystemeditorpanel.res" );

	// NOTE: Page adding happens *after* LoadControlSettingsAndUserConfig
	// because the layout of the sheet is correct at this point.
	m_pEditorSheet->AddPage( m_pControlsPanel, "Controls" );
	m_pEditorSheet->AddPage( m_pDominationRulesPanel, "Domination" );
	m_pEditorSheet->AddPage( m_pPropertiesPanel, "Properties" );
}

CDmeCombinationSystemEditorPanel::~CDmeCombinationSystemEditorPanel()
{
}


//-----------------------------------------------------------------------------
// Set the scene

//-----------------------------------------------------------------------------
void CDmeCombinationSystemEditorPanel::SetDmeElement( CDmeCombinationOperator *pComboOp )
{
	m_pControlsPanel->SetCombinationOperator( pComboOp );
	m_pDominationRulesPanel->SetCombinationOperator( pComboOp );
	m_pPropertiesPanel->SetDmeElement( pComboOp );
}

CDmeCombinationOperator *CDmeCombinationSystemEditorPanel::GetDmeElement()
{
	return m_pControlsPanel->GetCombinationOperator( );
}


//-----------------------------------------------------------------------------
// Called when the page changes
//-----------------------------------------------------------------------------
void CDmeCombinationSystemEditorPanel::OnPageChanged( )
{
	if ( m_pEditorSheet->GetActivePage() == m_pControlsPage )
	{
		return;
	}

	if ( m_pEditorSheet->GetActivePage() == m_pDominationRulesPage )
	{
		return;
	}
}


//-----------------------------------------------------------------------------
// Called when the property editor has made a change
//-----------------------------------------------------------------------------
void CDmeCombinationSystemEditorPanel::OnDmeElementChanged( KeyValues *kv )
{
	m_pControlsPanel->RefreshCombinationOperator();
	m_pDominationRulesPanel->RefreshCombinationOperator();
	m_pPropertiesPanel->Refresh();

	PostActionSignal( new KeyValues( "DmeElementChanged" ) );
}


//-----------------------------------------------------------------------------
//
// Purpose: Combination system editor frame
//
//-----------------------------------------------------------------------------
CDmeCombinationSystemEditorFrame::CDmeCombinationSystemEditorFrame( vgui::Panel *pParent, const char *pTitle ) : 
	BaseClass( pParent, "DmeCombinationSystemEditorFrame" )
{
	SetDeleteSelfOnClose( true );
	m_pEditor = new CDmeCombinationSystemEditorPanel( this, "DmeCombinationSystemEditorPanel" );
	m_pEditor->AddActionSignalTarget( this );
	m_pOpenButton = new vgui::Button( this, "OpenButton", "#FileOpenDialog_Open", this, "Open" );
	m_pCancelButton = new vgui::Button( this, "CancelButton", "#FileOpenDialog_Cancel", this, "Cancel" );
	SetBlockDragChaining( true );

	LoadControlSettingsAndUserConfig( "resource/dmecombinationsystemeditorframe.res" );

	SetTitle( pTitle, false );
}

CDmeCombinationSystemEditorFrame::~CDmeCombinationSystemEditorFrame()
{
}


//-----------------------------------------------------------------------------
// Sets the current scene + animation list
//-----------------------------------------------------------------------------
void CDmeCombinationSystemEditorFrame::SetCombinationOperator( CDmeCombinationOperator *pComboSystem )
{
	m_pEditor->SetDmeElement( pComboSystem );
}


//-----------------------------------------------------------------------------
// On command
//-----------------------------------------------------------------------------
void CDmeCombinationSystemEditorFrame::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "Open" ) )
	{
		return;
	}

	if ( !Q_stricmp( pCommand, "Cancel" ) )
	{
		return;
	}

	BaseClass::OnCommand( pCommand );
}


//-----------------------------------------------------------------------------
// On command
//-----------------------------------------------------------------------------
void CDmeCombinationSystemEditorFrame::OnDmeElementChanged()
{
	PostActionSignal( new KeyValues( "CombinationOperatorChanged" ) );
}