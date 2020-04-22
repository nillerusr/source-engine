//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Dialog used to edit properties of a particle system definition
//
//===========================================================================//

#include "dme_controls/ParticleSystemPropertiesPanel.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "vgui/IVGui.h"
#include "vgui_controls/button.h"
#include "vgui_controls/listpanel.h"
#include "vgui_controls/splitter.h"
#include "vgui_controls/messagebox.h"
#include "vgui_controls/combobox.h"
#include "datamodel/dmelement.h"
#include "movieobjects/dmeparticlesystemdefinition.h"
#include "dme_controls/elementpropertiestree.h"
#include "matsys_controls/picker.h"
#include "dme_controls/dmecontrols_utils.h"
#include "dme_controls/particlesystempanel.h"
#include "dme_controls/dmepanel.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;


//-----------------------------------------------------------------------------
//
// Purpose: Picker for particle functions
//
//-----------------------------------------------------------------------------
class CParticleFunctionPickerFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CParticleFunctionPickerFrame, vgui::Frame );

public:
	CParticleFunctionPickerFrame( vgui::Panel *pParent, const char *pTitle );
	~CParticleFunctionPickerFrame();

	// Sets the current scene + animation list
	void DoModal( CDmeParticleSystemDefinition *pParticleSystem, ParticleFunctionType_t type, KeyValues *pContextKeyValues );

	// Inherited from Frame
	virtual void OnCommand( const char *pCommand );

private:
	// Refreshes the list of particle functions
	void RefreshParticleFunctions( CDmeParticleSystemDefinition *pDefinition, ParticleFunctionType_t type  );
	void CleanUpMessage();

	vgui::ListPanel *m_pFunctionList;
	vgui::Button *m_pOpenButton;
	vgui::Button *m_pCancelButton;
	KeyValues *m_pContextKeyValues;
};

static int __cdecl ParticleFunctionNameSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString( "name" );
	const char *string2 = item2.kv->GetString( "name" );
	return Q_stricmp( string1, string2 );
}

CParticleFunctionPickerFrame::CParticleFunctionPickerFrame( vgui::Panel *pParent, const char *pTitle ) : 
	BaseClass( pParent, "ParticleFunctionPickerFrame" )
{
	SetDeleteSelfOnClose( true );
	m_pContextKeyValues = NULL;

	m_pFunctionList = new vgui::ListPanel( this, "ParticleFunctionList" );
	m_pFunctionList->AddColumnHeader( 0, "name", "Particle Function Name", 52, 0 );
	m_pFunctionList->SetSelectIndividualCells( false );
	m_pFunctionList->SetMultiselectEnabled( false );
	m_pFunctionList->SetEmptyListText( "No particle functions" );
	m_pFunctionList->AddActionSignalTarget( this );
	m_pFunctionList->SetSortFunc( 0, ParticleFunctionNameSortFunc );
	m_pFunctionList->SetSortColumn( 0 );

	m_pOpenButton = new vgui::Button( this, "OkButton", "#MessageBox_OK", this, "Ok" );
	m_pCancelButton = new vgui::Button( this, "CancelButton", "#MessageBox_Cancel", this, "Cancel" );
	SetBlockDragChaining( true );

	LoadControlSettingsAndUserConfig( "resource/particlefunctionpicker.res" );

	SetTitle( pTitle, false );
}

CParticleFunctionPickerFrame::~CParticleFunctionPickerFrame()
{
	CleanUpMessage();
}


//-----------------------------------------------------------------------------
// Refreshes the list of raw controls
//-----------------------------------------------------------------------------
void CParticleFunctionPickerFrame::RefreshParticleFunctions( CDmeParticleSystemDefinition *pParticleSystem, ParticleFunctionType_t type )
{
	m_pFunctionList->RemoveAll();	
	if ( !pParticleSystem )
		return;

	CUtlVector< IParticleOperatorDefinition *> &list = g_pParticleSystemMgr->GetAvailableParticleOperatorList( type );
	int nCount = list.Count();

	// Build a list of used operator IDs
	bool pUsedIDs[OPERATOR_ID_COUNT];
	memset( pUsedIDs, 0, sizeof(pUsedIDs) );

	int nFunctionCount = pParticleSystem->GetParticleFunctionCount( type );
	for ( int i = 0; i < nFunctionCount; ++i )
	{
		const char *pFunctionName = pParticleSystem->GetParticleFunction( type, i )->GetName();
		for ( int j = 0; j < nCount; ++j )
		{
			if ( Q_stricmp( pFunctionName, list[j]->GetName() ) )
				continue;

			if ( list[j]->GetId() >= 0 )
			{
				pUsedIDs[ list[j]->GetId() ] = true;
			}
			break;
		}
	}

	for ( int i = 0; i < nCount; ++i )
	{
		const char *pFunctionName = list[i]->GetName();

		// Look to see if this is in a special operator group
		if ( list[i]->GetId() >= 0 )
		{
			// Don't display ones that are already in the particle system
			if ( pUsedIDs[ list[i]->GetId() ] )
				continue;
		}

		if ( list[i]->GetId() == OPERATOR_SINGLETON )
		{
			// Don't display ones that are already in the particle system
			if ( pParticleSystem->FindFunction( type, pFunctionName ) >= 0 )
				continue;
		}

		// Don't display obsolete operators
		if ( list[i]->IsObsolete() )
			continue;

		KeyValues *kv = new KeyValues( "node", "name", pFunctionName );
		m_pFunctionList->AddItem( kv, 0, false, false );
	}

	m_pFunctionList->SortList();
}


//-----------------------------------------------------------------------------
// Deletes the message
//-----------------------------------------------------------------------------
void CParticleFunctionPickerFrame::CleanUpMessage()
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
void CParticleFunctionPickerFrame::DoModal( CDmeParticleSystemDefinition *pParticleSystem, ParticleFunctionType_t type, KeyValues *pContextKeyValues )
{
	CleanUpMessage();
	RefreshParticleFunctions( pParticleSystem, type );
	m_pContextKeyValues = pContextKeyValues;
	BaseClass::DoModal();
}


//-----------------------------------------------------------------------------
// On command
//-----------------------------------------------------------------------------
void CParticleFunctionPickerFrame::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "Ok" ) )
	{
		int nSelectedItemCount = m_pFunctionList->GetSelectedItemsCount();
		if ( nSelectedItemCount == 0 )
			return;

		Assert( nSelectedItemCount == 1 );
		int nItemID = m_pFunctionList->GetSelectedItem( 0 );
		KeyValues *pKeyValues = m_pFunctionList->GetItem( nItemID );

		KeyValues *pActionKeys = new KeyValues( "ParticleFunctionPicked" );
		pActionKeys->SetString( "name", pKeyValues->GetString( "name" ) );

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
//
// Purpose: Picker for child particle systems
//
//-----------------------------------------------------------------------------
class CParticleChildrenPickerFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CParticleChildrenPickerFrame, vgui::Frame );

public:
	CParticleChildrenPickerFrame( vgui::Panel *pParent, const char *pTitle, IParticleSystemPropertiesPanelQuery *pQuery );
	~CParticleChildrenPickerFrame();

	// Sets the current scene + animation list
	void DoModal( CDmeParticleSystemDefinition *pParticleSystem, KeyValues *pContextKeyValues );

	// Inherited from Frame
	virtual void OnCommand( const char *pCommand );

private:
	// Refreshes the list of children particle systems
	void RefreshChildrenList( CDmeParticleSystemDefinition *pDefinition );
	void CleanUpMessage();

	IParticleSystemPropertiesPanelQuery *m_pQuery;
	vgui::ListPanel *m_pChildrenList;
	vgui::Button *m_pOpenButton;
	vgui::Button *m_pCancelButton;
	KeyValues *m_pContextKeyValues;
};

static int __cdecl ParticleChildrenNameSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString( "name" );
	const char *string2 = item2.kv->GetString( "name" );
	return Q_stricmp( string1, string2 );
}

CParticleChildrenPickerFrame::CParticleChildrenPickerFrame( vgui::Panel *pParent, const char *pTitle, IParticleSystemPropertiesPanelQuery *pQuery ) : 
	BaseClass( pParent, "ParticleChildrenPickerFrame" ), m_pQuery( pQuery )
{
	SetDeleteSelfOnClose( true );
	m_pContextKeyValues = NULL;

	m_pChildrenList = new vgui::ListPanel( this, "ParticleChildrenList" );
	m_pChildrenList->AddColumnHeader( 0, "name", "Particle System Name", 52, 0 );
	m_pChildrenList->SetSelectIndividualCells( false );
	m_pChildrenList->SetMultiselectEnabled( false );
	m_pChildrenList->SetEmptyListText( "No particle systems" );
	m_pChildrenList->AddActionSignalTarget( this );
	m_pChildrenList->SetSortFunc( 0, ParticleChildrenNameSortFunc );
	m_pChildrenList->SetSortColumn( 0 );

	m_pOpenButton = new vgui::Button( this, "OkButton", "#MessageBox_OK", this, "Ok" );
	m_pCancelButton = new vgui::Button( this, "CancelButton", "#MessageBox_Cancel", this, "Cancel" );
	SetBlockDragChaining( true );

	LoadControlSettingsAndUserConfig( "resource/particlechildrenpicker.res" );

	SetTitle( pTitle, false );
}

CParticleChildrenPickerFrame::~CParticleChildrenPickerFrame()
{
	CleanUpMessage();
}


//-----------------------------------------------------------------------------
// Refreshes the list of raw controls
//-----------------------------------------------------------------------------
void CParticleChildrenPickerFrame::RefreshChildrenList( CDmeParticleSystemDefinition *pCurrentParticleSystem )
{
	m_pChildrenList->RemoveAll();

	CUtlVector< CDmeParticleSystemDefinition* > definitions;
	if ( m_pQuery )
	{
		m_pQuery->GetKnownParticleDefinitions( definitions );
	}
	int nCount = definitions.Count();
	if ( nCount == 0 )
		return;

	for ( int i = 0; i < nCount; ++i )
	{
		CDmeParticleSystemDefinition *pParticleSystem = definitions[i];
		if ( pParticleSystem == pCurrentParticleSystem )
			continue;

		const char *pName = pParticleSystem->GetName();
		if ( !pName || !pName[0] )
		{
			pName = "<no name>";
		}

		KeyValues *kv = new KeyValues( "node" );
		kv->SetString( "name", pName ); 
		SetElementKeyValue( kv, "particleSystem", pParticleSystem );

		m_pChildrenList->AddItem( kv, 0, false, false );
	}
	m_pChildrenList->SortList();
}


//-----------------------------------------------------------------------------
// Deletes the message
//-----------------------------------------------------------------------------
void CParticleChildrenPickerFrame::CleanUpMessage()
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
void CParticleChildrenPickerFrame::DoModal( CDmeParticleSystemDefinition *pParticleSystem, KeyValues *pContextKeyValues )
{
	CleanUpMessage();
	RefreshChildrenList( pParticleSystem );
	m_pContextKeyValues = pContextKeyValues;
	BaseClass::DoModal();
}


//-----------------------------------------------------------------------------
// On command
//-----------------------------------------------------------------------------
void CParticleChildrenPickerFrame::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "Ok" ) )
	{
		int nSelectedItemCount = m_pChildrenList->GetSelectedItemsCount();
		if ( nSelectedItemCount == 0 )
			return;

		Assert( nSelectedItemCount == 1 );
		int nItemID = m_pChildrenList->GetSelectedItem( 0 );
		KeyValues *pKeyValues = m_pChildrenList->GetItem( nItemID );
		CDmeParticleSystemDefinition *pParticleSystem = GetElementKeyValue<CDmeParticleSystemDefinition>( pKeyValues, "particleSystem" );

		KeyValues *pActionKeys = new KeyValues( "ParticleChildPicked" );
		SetElementKeyValue( pActionKeys, "particleSystem", pParticleSystem );

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
// Browser of various particle functions
//-----------------------------------------------------------------------------
class CParticleFunctionBrowser : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CParticleFunctionBrowser, vgui::EditablePanel );

public:
	// constructor, destructor
	CParticleFunctionBrowser( vgui::Panel *pParent, const char *pName, ParticleFunctionType_t type, IParticleSystemPropertiesPanelQuery *pQuery );
	virtual ~CParticleFunctionBrowser();

	// Inherited from Panel
	virtual void OnKeyCodeTyped( vgui::KeyCode code );

	void SetParticleFunctionProperties( CDmeElementPanel *pPanel );
	void SetParticleSystem( CDmeParticleSystemDefinition *pOp );
	void RefreshParticleFunctionList();
	void SelectDefaultFunction();

	// Called when a list panel's selection changes
	void RefreshParticleFunctionProperties( );

private:
	MESSAGE_FUNC_PARAMS( OnOpenContextMenu, "OpenContextMenu", kv );
	MESSAGE_FUNC_PARAMS( OnItemSelected, "ItemSelected", kv );	
	MESSAGE_FUNC_PARAMS( OnItemDeselected, "ItemDeselected", kv );	
	MESSAGE_FUNC( OnAdd, "Add" );
	MESSAGE_FUNC( OnRemove, "Remove" );
	MESSAGE_FUNC( OnRename, "Rename" );
	MESSAGE_FUNC( OnMoveUp, "MoveUp" );
	MESSAGE_FUNC( OnMoveDown, "MoveDown" );
	MESSAGE_FUNC_PARAMS( OnInputCompleted, "InputCompleted", kv );
	MESSAGE_FUNC_PARAMS( OnParticleFunctionPicked, "ParticleFunctionPicked", kv );
	MESSAGE_FUNC_PARAMS( OnParticleChildPicked, "ParticleChildPicked", kv );

	// Cleans up the context menu
	void CleanupContextMenu();

	// Returns the selected particle function
	CDmeParticleFunction* GetSelectedFunction( );

	// Returns the selected particle function
	CDmeParticleFunction* GetSelectedFunction( int nIndex );

	// Select a particular particle function
	void SelectParticleFunction( CDmeParticleFunction *pFind );

	IParticleSystemPropertiesPanelQuery *m_pQuery;
	CDmeHandle< CDmeParticleSystemDefinition > m_hParticleSystem;
	vgui::ListPanel *m_pFunctionList;
	ParticleFunctionType_t m_FuncType;
	vgui::DHANDLE< vgui::Menu > m_hContextMenu;
	CDmeElementPanel *m_pParticleFunctionProperties;
};


//-----------------------------------------------------------------------------
// Sort functions for list panel
//-----------------------------------------------------------------------------
static int __cdecl ParticleFunctionSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	int i1 = item1.kv->GetInt( "index" );
	int i2 = item2.kv->GetInt( "index" );
	return i1 - i2;
}


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CParticleFunctionBrowser::CParticleFunctionBrowser( vgui::Panel *pParent, const char *pName, ParticleFunctionType_t type, IParticleSystemPropertiesPanelQuery *pQuery ) :
	BaseClass( pParent, pName ), m_pQuery( pQuery )
{
	SetKeyBoardInputEnabled( true );

	m_FuncType = type;

	m_pFunctionList = new vgui::ListPanel( this, "FunctionList" );
	if ( m_FuncType != FUNCTION_CHILDREN )
	{
		m_pFunctionList->AddColumnHeader( 0, "name", "Function Name", 150, 0 );
		m_pFunctionList->AddColumnHeader( 1, "type", "Function Type", 150, 0 );
		m_pFunctionList->SetEmptyListText( "No particle functions" );
	}
	else
	{
		m_pFunctionList->AddColumnHeader( 0, "name", "Label", 150, 0 );
		m_pFunctionList->AddColumnHeader( 1, "type", "Child Name", 150, 0 );
		m_pFunctionList->SetEmptyListText( "No children" );
	}
	m_pFunctionList->SetSelectIndividualCells( false );
	m_pFunctionList->SetMultiselectEnabled( true );
	m_pFunctionList->AddActionSignalTarget( this );
	m_pFunctionList->SetSortFunc( 0, ParticleFunctionSortFunc );
	m_pFunctionList->SetSortColumn( 0 );
	m_pFunctionList->AddActionSignalTarget( this );

	LoadControlSettings( "resource/particlefunctionbrowser.res" );
}

CParticleFunctionBrowser::~CParticleFunctionBrowser()
{
	CleanupContextMenu();
	SaveUserConfig();
}


//-----------------------------------------------------------------------------
// Cleans up the context menu
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::CleanupContextMenu()
{
	if ( m_hContextMenu.Get() )
	{
		m_hContextMenu->MarkForDeletion();
		m_hContextMenu = NULL;
	}
}


//-----------------------------------------------------------------------------
// Selects a particular function by default
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::SelectDefaultFunction()
{
	if ( m_pFunctionList->GetSelectedItemsCount() == 0 && m_pFunctionList->GetItemCount() > 0 )
	{
		int nItemID = m_pFunctionList->GetItemIDFromRow( 0 );
		m_pFunctionList->SetSingleSelectedItem( nItemID );
	}
}


//-----------------------------------------------------------------------------
// Sets the particle system	properties panel
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::SetParticleFunctionProperties( CDmeElementPanel *pPanel )
{
	m_pParticleFunctionProperties = pPanel;
}


//-----------------------------------------------------------------------------
// Sets/gets the particle system
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::SetParticleSystem( CDmeParticleSystemDefinition *pParticleSystem )
{
	if ( pParticleSystem != m_hParticleSystem.Get() )
	{
		m_hParticleSystem = pParticleSystem;
		RefreshParticleFunctionList();
		SelectDefaultFunction();
	}
}


//-----------------------------------------------------------------------------
// Builds the particle function list for the particle system
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::RefreshParticleFunctionList()
{
	if ( !m_hParticleSystem.Get() )
		return;

	// Maintain selection if possible
	CUtlVector< CDmeParticleFunction* > selectedItems;
	int nCount = m_pFunctionList->GetSelectedItemsCount();
	for ( int i = 0; i < nCount; ++i )
	{
		selectedItems.AddToTail( GetSelectedFunction( i ) );
	}

	m_pFunctionList->RemoveAll();	

	int nSelectedCount = selectedItems.Count();
	nCount = m_hParticleSystem->GetParticleFunctionCount( m_FuncType );
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeParticleFunction *pFunction = m_hParticleSystem->GetParticleFunction( m_FuncType, i );
		KeyValues *kv = new KeyValues( "node", "name", pFunction->GetName() );
		kv->SetString( "type", pFunction->GetFunctionType() );
		kv->SetInt( "index", i );
		SetElementKeyValue( kv, "particleFunction", pFunction );
		int nItemID = m_pFunctionList->AddItem( kv, 0, false, false );

		for ( int j = 0; j < nSelectedCount; ++j )
		{
			if ( selectedItems[j] == pFunction )
			{
				m_pFunctionList->AddSelectedItem( nItemID );
				selectedItems.FastRemove( j );
				--nSelectedCount;
				break;
			}
		}
	}

	m_pFunctionList->SortList();
}


//-----------------------------------------------------------------------------
// Returns the selected particle function
//-----------------------------------------------------------------------------
CDmeParticleFunction* CParticleFunctionBrowser::GetSelectedFunction( int nIndex )
{
	if ( !m_hParticleSystem.Get() )
		return NULL;

	int nSelectedItemCount = m_pFunctionList->GetSelectedItemsCount();
	if ( nSelectedItemCount <= nIndex )
		return NULL;

	int nItemID = m_pFunctionList->GetSelectedItem( nIndex );
	KeyValues *pKeyValues = m_pFunctionList->GetItem( nItemID );
	return GetElementKeyValue<CDmeParticleFunction>( pKeyValues, "particleFunction" );
}


//-----------------------------------------------------------------------------
// Returns the selected particle function
//-----------------------------------------------------------------------------
CDmeParticleFunction* CParticleFunctionBrowser::GetSelectedFunction( )
{
	if ( !m_hParticleSystem.Get() )
		return NULL;

	int nSelectedItemCount = m_pFunctionList->GetSelectedItemsCount();
	if ( nSelectedItemCount != 1 )
		return NULL;

	int nItemID = m_pFunctionList->GetSelectedItem( 0 );
	KeyValues *pKeyValues = m_pFunctionList->GetItem( nItemID );
	return GetElementKeyValue<CDmeParticleFunction>( pKeyValues, "particleFunction" );
}

void CParticleFunctionBrowser::OnMoveUp( )
{
	CDmeParticleFunction* pFunction = GetSelectedFunction( );
	if ( !pFunction )
		return;

	{
		CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Move Function Up", "Move Function Up" );
		m_hParticleSystem->MoveFunctionUp( m_FuncType, pFunction );
	}
	PostActionSignal( new KeyValues( "ParticleSystemModified" ) );
}

void CParticleFunctionBrowser::OnMoveDown( )
{
	CDmeParticleFunction* pFunction = GetSelectedFunction( );
	if ( !pFunction )
		return;

	{
		CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Move Function Down", "Move Function Down" );
		m_hParticleSystem->MoveFunctionDown( m_FuncType, pFunction );
	}	
	PostActionSignal( new KeyValues( "ParticleSystemModified" ) );
}


//-----------------------------------------------------------------------------
// Select a particular particle function
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::SelectParticleFunction( CDmeParticleFunction *pFind )
{
	m_pFunctionList->ClearSelectedItems();
	for ( int nItemID = m_pFunctionList->FirstItem(); nItemID != m_pFunctionList->InvalidItemID(); nItemID = m_pFunctionList->NextItem( nItemID ) )
	{
		KeyValues *kv = m_pFunctionList->GetItem( nItemID );
		CDmeParticleFunction *pFunction = GetElementKeyValue<CDmeParticleFunction>( kv, "particleFunction" );
		if ( pFunction == pFind )
		{
			m_pFunctionList->AddSelectedItem( nItemID );
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Add/remove functions
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::OnParticleChildPicked( KeyValues *pKeyValues )
{
	CDmeParticleSystemDefinition *pParticleSystem = GetElementKeyValue<CDmeParticleSystemDefinition>( pKeyValues, "particleSystem" );
	if ( !pParticleSystem || ( m_FuncType != FUNCTION_CHILDREN ) )
		return;

	CDmeParticleFunction *pFunction;
	{
		CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Add Particle System Child", "Add Particle System Child" );
		pFunction = m_hParticleSystem->AddChild( pParticleSystem );
	}
	PostActionSignal( new KeyValues( "ParticleSystemModified" ) );
	SelectParticleFunction( pFunction );
}

void CParticleFunctionBrowser::OnParticleFunctionPicked( KeyValues *pKeyValues )
{
	if ( m_FuncType == FUNCTION_CHILDREN )
		return;

	const char *pParticleFuncName = pKeyValues->GetString( "name" );
	CDmeParticleFunction *pFunction;
	{
		CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Add Particle Function", "Add Particle Function" );
		pFunction = m_hParticleSystem->AddOperator( m_FuncType, pParticleFuncName );
	}
	PostActionSignal( new KeyValues( "ParticleSystemModified" ) );
	SelectParticleFunction( pFunction );
}

void CParticleFunctionBrowser::OnAdd( )
{
	if ( m_FuncType != FUNCTION_CHILDREN )
	{
		CParticleFunctionPickerFrame *pPicker = new CParticleFunctionPickerFrame( this, "Select Particle Function" );
		pPicker->DoModal( m_hParticleSystem, m_FuncType, NULL );
	}
	else
	{
		CParticleChildrenPickerFrame *pPicker = new CParticleChildrenPickerFrame( this, "Select Child Particle Systems", m_pQuery );
		pPicker->DoModal( m_hParticleSystem, NULL );
	}
}

void CParticleFunctionBrowser::OnRemove( )
{
	int iSel = m_pFunctionList->GetSelectedItem( 0 );
	int nRow = m_pFunctionList->GetItemCurrentRow( iSel ) - 1;
	{
		CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Remove Particle Function", "Remove Particle Function" );

		//
		// Build a list of objects to delete.
		//
		CUtlVector< CDmeParticleFunction* > itemsToDelete;
		int nCount = m_pFunctionList->GetSelectedItemsCount();
		for (int i = 0; i < nCount; i++)
		{
			CDmeParticleFunction *pParticleFunction = GetSelectedFunction( i );
			if ( pParticleFunction )
			{
				itemsToDelete.AddToTail( pParticleFunction );
			}
		}

		nCount = itemsToDelete.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			m_hParticleSystem->RemoveFunction( m_FuncType, itemsToDelete[i] );
		}
	}

	PostActionSignal( new KeyValues( "ParticleSystemModified" ) );

	// Update the list box selection.
	if ( m_pFunctionList->GetItemCount() > 0 )
	{
		if ( nRow < 0 )
		{
			nRow = 0;
		}
		else if ( nRow >= m_pFunctionList->GetItemCount() ) 
		{
			nRow = m_pFunctionList->GetItemCount() - 1;
		}

		iSel = m_pFunctionList->GetItemIDFromRow( nRow );
		m_pFunctionList->SetSingleSelectedItem( iSel );
	}
	else
	{
		m_pFunctionList->ClearSelectedItems();
	}
}


//-----------------------------------------------------------------------------
// Rename a particle function
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::OnInputCompleted( KeyValues *pKeyValues )
{
	const char *pName = pKeyValues->GetString( "text" );
	CDmeParticleFunction *pFunction = GetSelectedFunction();
	{
		CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Rename Particle Function", "Rename Particle Function" );
		pFunction->SetName( pName );
	}
	PostActionSignal( new KeyValues( "ParticleSystemModified" ) );
}


//-----------------------------------------------------------------------------
// Rename a particle function
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::OnRename()
{
	int nSelectedItemCount = m_pFunctionList->GetSelectedItemsCount();
	if ( nSelectedItemCount != 1 )
		return;

	vgui::InputDialog *pInput = new vgui::InputDialog( this, "Rename Particle Function", "Enter new name of particle function" );
	pInput->SetMultiline( false );
	pInput->DoModal( );
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::RefreshParticleFunctionProperties( )
{
	if ( IsVisible() )
	{
		CDmeParticleFunction *pFunction = GetSelectedFunction();
		if ( pFunction )
		{
			m_pParticleFunctionProperties->SetTypeDictionary( pFunction->GetEditorTypeDictionary() );
		}
		m_pParticleFunctionProperties->SetDmeElement( pFunction );

		// Notify the outside world so we can get helpers to render correctly in the preview
		KeyValues *pMessage = new KeyValues( "ParticleFunctionSelChanged" );
		SetElementKeyValue( pMessage, "function", pFunction );
		PostActionSignal( pMessage );
	}
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::OnItemSelected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pFunctionList )
	{
		RefreshParticleFunctionProperties();
		return;
	}
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::OnItemDeselected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pFunctionList )
	{
		RefreshParticleFunctionProperties();
		return;
	}
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::OnOpenContextMenu( KeyValues *kv )
{
	CleanupContextMenu();
	if ( !m_hParticleSystem.Get() )
		return;

	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel != m_pFunctionList )
		return;

	int nSelectedItemCount = m_pFunctionList->GetSelectedItemsCount();
	m_hContextMenu = new vgui::Menu( this, "ActionMenu" );
	m_hContextMenu->AddMenuItem( "#ParticleFunctionBrowser_Add", new KeyValues( "Add" ), this );
	if ( nSelectedItemCount >= 1 )
	{
		m_hContextMenu->AddMenuItem( "#ParticleFunctionBrowser_Remove", new KeyValues( "Remove" ), this );
	}
	if ( nSelectedItemCount == 1 )
	{
		m_hContextMenu->AddMenuItem( "#ParticleFunctionBrowser_Rename", new KeyValues( "Rename" ), this  );
		m_hContextMenu->AddSeparator();
		m_hContextMenu->AddMenuItem( "#ParticleFunctionBrowser_MoveUp", new KeyValues( "MoveUp" ), this );
		m_hContextMenu->AddMenuItem( "#ParticleFunctionBrowser_MoveDown", new KeyValues( "MoveDown" ), this );
	}

	vgui::Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::OnKeyCodeTyped( vgui::KeyCode code )
{
	if ( code == KEY_DELETE ) 
	{
		OnRemove();
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}



//-----------------------------------------------------------------------------
// Strings
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CParticleSystemPropertiesPanel::CParticleSystemPropertiesPanel( IParticleSystemPropertiesPanelQuery *pQuery, vgui::Panel* pParent )
	: BaseClass( pParent, "ParticleSystemPropertiesPanel" ), m_pQuery( pQuery )
{
	SetKeyBoardInputEnabled( true );

	m_pSplitter = new vgui::Splitter( this, "Splitter", vgui::SPLITTER_MODE_VERTICAL, 1 );
	vgui::Panel *pSplitterLeftSide = m_pSplitter->GetChild( 0 );
	vgui::Panel *pSplitterRightSide = m_pSplitter->GetChild( 1 );

	m_pFunctionBrowserArea = new vgui::EditablePanel( pSplitterLeftSide, "FunctionBrowserArea" );
	m_pFunctionTypeCombo = new vgui::ComboBox( pSplitterLeftSide, "FunctionTypeCombo", PARTICLE_FUNCTION_COUNT+1, false );
	m_pFunctionTypeCombo->AddItem( "Properties", new KeyValues( "choice", "index", -1 ) );
	m_pFunctionTypeCombo->AddActionSignalTarget( this );

	m_pParticleFunctionProperties = new CDmeElementPanel( pSplitterRightSide, "FunctionProperties" );
	m_pParticleFunctionProperties->SetAutoResize( vgui::Panel::PIN_TOPLEFT, vgui::Panel::AUTORESIZE_DOWNANDRIGHT, 6, 6, -6, -6 );
	m_pParticleFunctionProperties->AddActionSignalTarget( this );

	for ( int i = 0; i < PARTICLE_FUNCTION_COUNT; ++i )
	{
		const char *pTypeName = GetParticleFunctionTypeName( (ParticleFunctionType_t)i );

		m_pFunctionTypeCombo->AddItem( pTypeName, new KeyValues( "choice", "index", i ) );
		m_pParticleFunctionBrowser[i] = new CParticleFunctionBrowser( m_pFunctionBrowserArea, "FunctionBrowser", (ParticleFunctionType_t)i, m_pQuery );
		m_pParticleFunctionBrowser[i]->SetParticleFunctionProperties( m_pParticleFunctionProperties );
		m_pParticleFunctionBrowser[i]->AddActionSignalTarget( this );
	}

	m_pFunctionTypeCombo->ActivateItemByRow( 0 );
	LoadControlSettings( "resource/particlesystempropertiespanel.res" );
}


//-----------------------------------------------------------------------------
// Sets the particle system to look at
//-----------------------------------------------------------------------------
void CParticleSystemPropertiesPanel::SetParticleSystem( CDmeParticleSystemDefinition *pParticleSystem )
{
	m_hParticleSystem = pParticleSystem;
	for ( int i = 0; i < PARTICLE_FUNCTION_COUNT; ++i )
	{
		m_pParticleFunctionBrowser[i]->SetParticleSystem( pParticleSystem );
	}

	KeyValues *pKeyValues = m_pFunctionTypeCombo->GetActiveItemUserData();
	int nPage = pKeyValues->GetInt( "index" );
	if ( nPage < 0 )
	{
		if ( m_hParticleSystem.Get() )
		{
			m_pParticleFunctionProperties->SetTypeDictionary( m_hParticleSystem->GetEditorTypeDictionary() );
		}
		m_pParticleFunctionProperties->SetDmeElement( m_hParticleSystem );
	}
}


//-----------------------------------------------------------------------------
// Called when something changes in the dmeelement panel
//-----------------------------------------------------------------------------
void CParticleSystemPropertiesPanel::OnDmeElementChanged( KeyValues *pKeyValues )
{
	int nNotifyFlags = pKeyValues->GetInt( "notifyFlags" );
	OnParticleSystemModified();

	if ( nNotifyFlags & ( NOTIFY_CHANGE_TOPOLOGICAL | NOTIFY_CHANGE_ATTRIBUTE_ARRAY_SIZE ) )
	{
		for ( int i = 0; i < PARTICLE_FUNCTION_COUNT; ++i )
		{
			m_pParticleFunctionBrowser[i]->RefreshParticleFunctionList();
		}
	}
	PostActionSignal( new KeyValues( "ParticleSystemModified" ) );
}

void CParticleSystemPropertiesPanel::OnParticleSystemModifiedInternal()
{
	OnParticleSystemModified();
	Refresh( false );
	PostActionSignal( new KeyValues( "ParticleSystemModified" ) );
}


void CParticleSystemPropertiesPanel::OnParticleFunctionSelChanged( KeyValues *pParams )
{
	// Notify the outside world so we can get helpers to render correctly in the preview
	CDmeParticleFunction *pFunction = GetElementKeyValue<CDmeParticleFunction>( pParams, "function" );
	KeyValues *pMessage = new KeyValues( "ParticleFunctionSelChanged" );
	SetElementKeyValue( pMessage, "function", pFunction );
	PostActionSignal( pMessage );
}


//-----------------------------------------------------------------------------
// Refreshes display
//-----------------------------------------------------------------------------
void CParticleSystemPropertiesPanel::Refresh( bool bValuesOnly )
{
	for ( int i = 0; i < PARTICLE_FUNCTION_COUNT; ++i )
	{
		m_pParticleFunctionBrowser[i]->RefreshParticleFunctionList();
	}
	m_pParticleFunctionProperties->Refresh( bValuesOnly ? CElementPropertiesTreeInternal::REFRESH_VALUES_ONLY :
		CElementPropertiesTreeInternal::REFRESH_TREE_VIEW );
}


//-----------------------------------------------------------------------------
// Purpose: Called when a page is shown
//-----------------------------------------------------------------------------
void CParticleSystemPropertiesPanel::OnTextChanged( )
{
	for ( int i = 0; i < PARTICLE_FUNCTION_COUNT; ++i )
	{
		m_pParticleFunctionBrowser[i]->SetVisible( false );
	}

	KeyValues *pKeyValues = m_pFunctionTypeCombo->GetActiveItemUserData();
	int nPage = pKeyValues->GetInt( "index" );
	if ( nPage < 0 )
	{
		if ( m_hParticleSystem.Get() )
		{
			m_pParticleFunctionProperties->SetTypeDictionary( m_hParticleSystem->GetEditorTypeDictionary() );
		}
		m_pParticleFunctionProperties->SetDmeElement( m_hParticleSystem );
		return;
	}

	m_pParticleFunctionBrowser[nPage]->SetVisible( true );
	m_pParticleFunctionBrowser[nPage]->SelectDefaultFunction();
	m_pParticleFunctionBrowser[nPage]->RefreshParticleFunctionProperties();
}




//-----------------------------------------------------------------------------
// A little panel used as a DmePanel for particle systems
//-----------------------------------------------------------------------------
class CParticleSystemDmePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CParticleSystemDmePanel, vgui::EditablePanel );

public:
	// constructor, destructor
	CParticleSystemDmePanel( vgui::Panel *pParent, const char *pName );
	virtual ~CParticleSystemDmePanel();

	// Set the material to draw
	void SetDmeElement( CDmeParticleSystemDefinition *pDef );

private:
	MESSAGE_FUNC_INT( OnElementChangedExternally, "ElementChangedExternally", valuesOnly );
	MESSAGE_FUNC( OnParticleSystemModified, "ParticleSystemModified" );
	MESSAGE_FUNC_PARAMS( OnParticleFunctionSelChanged, "ParticleFunctionSelChanged", params );

	vgui::Splitter *m_Splitter;
	CParticleSystemPropertiesPanel *m_pProperties;
	CParticleSystemPreviewPanel *m_pPreview;
};


//-----------------------------------------------------------------------------
// Dme panel connection
//-----------------------------------------------------------------------------
IMPLEMENT_DMEPANEL_FACTORY( CParticleSystemDmePanel, DmeParticleSystemDefinition, "DmeParticleSystemDefinitionEditor", "Particle System Editor", true );


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CParticleSystemDmePanel::CParticleSystemDmePanel( vgui::Panel *pParent, const char *pName ) :
	BaseClass( pParent, pName )
{
	m_Splitter = new vgui::Splitter( this, "Splitter", SPLITTER_MODE_HORIZONTAL, 1 );
	vgui::Panel *pSplitterTopSide = m_Splitter->GetChild( 0 );
	vgui::Panel *pSplitterBottomSide = m_Splitter->GetChild( 1 );
	m_Splitter->SetAutoResize( PIN_TOPLEFT, AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );

	m_pProperties = new CParticleSystemPropertiesPanel( NULL, pSplitterBottomSide );
	m_pProperties->AddActionSignalTarget( this );
	m_pProperties->SetAutoResize( PIN_TOPLEFT, AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );

	m_pPreview = new CParticleSystemPreviewPanel( pSplitterTopSide, "Preview" );
	m_pPreview->SetAutoResize( PIN_TOPLEFT, AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );
}

CParticleSystemDmePanel::~CParticleSystemDmePanel()
{
}


//-----------------------------------------------------------------------------
// Layout
//-----------------------------------------------------------------------------
void CParticleSystemDmePanel::SetDmeElement( CDmeParticleSystemDefinition *pDef )
{
	m_pProperties->SetParticleSystem( pDef );
	m_pPreview->SetParticleSystem( pDef );
}


//-----------------------------------------------------------------------------
// Particle system modified	externally to the editor
//-----------------------------------------------------------------------------
void CParticleSystemDmePanel::OnElementChangedExternally( int valuesOnly )
{
	m_pProperties->Refresh( valuesOnly != 0 );
}


//-----------------------------------------------------------------------------
// Called when the selected particle function changes
//-----------------------------------------------------------------------------
void CParticleSystemDmePanel::OnParticleFunctionSelChanged( KeyValues *pParams )
{
	CDmeParticleFunction *pFunction = GetElementKeyValue<CDmeParticleFunction>( pParams, "function" );
	m_pPreview->SetParticleFunction( pFunction );
}


//-----------------------------------------------------------------------------
// Communicate to the owning DmePanel
//-----------------------------------------------------------------------------
void CParticleSystemDmePanel::OnParticleSystemModified()
{
	PostActionSignal( new KeyValues( "DmeElementChanged" ) );
}
