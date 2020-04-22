//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "dme_controls/DmePicker.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/Button.h"
#include "datamodel/dmelement.h"
#include "vgui/ISurface.h"
#include "vgui/iinput.h"
#include "dme_controls/dmecontrols_utils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
//
// Dme Picker
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Sort by MDL name
//-----------------------------------------------------------------------------
static int __cdecl DmeBrowserSortFunc( vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString("dme");
	const char *string2 = item2.kv->GetString("dme");
	return stricmp( string1, string2 );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDmePicker::CDmePicker( vgui::Panel *pParent ) : BaseClass( pParent, "DmePicker" )
{
	// FIXME: Make this an image browser
	m_pDmeBrowser = new vgui::ListPanel( this, "DmeBrowser" );
	m_pDmeBrowser->AddColumnHeader( 0, "dme", "Dme Elements", 52, 0 );
	m_pDmeBrowser->SetSelectIndividualCells( true );
	m_pDmeBrowser->SetEmptyListText( "No Dme Elements" );
	m_pDmeBrowser->SetDragEnabled( true );
	m_pDmeBrowser->AddActionSignalTarget( this );
	m_pDmeBrowser->SetSortFunc( 0, DmeBrowserSortFunc );
	m_pDmeBrowser->SetSortColumn( 0 );

	// filter selection
	m_pFilterList = new TextEntry( this, "FilterList" );
	m_pFilterList->AddActionSignalTarget( this );
	m_pFilterList->RequestFocus();

	LoadControlSettingsAndUserConfig( "resource/dmepicker.res" );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDmePicker::~CDmePicker()
{
}


//-----------------------------------------------------------------------------
// Purpose: called to open
//-----------------------------------------------------------------------------
void CDmePicker::Activate( const CUtlVector< DmePickerInfo_t >&vec )
{
	m_pDmeBrowser->RemoveAll();	

	int nCount = vec.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmElement *pElement = GetElement<CDmElement>( vec[i].m_hElement );
		const char *pElementName = pElement ? pElement->GetName() : "<null element>";
		const char *pItemName = vec[i].m_pChoiceString ? vec[i].m_pChoiceString : pElementName;

		KeyValues *kv = new KeyValues( "node", "dme", pItemName );
		kv->SetInt( "dmeHandle", vec[i].m_hElement ); 
		int nItemID = m_pDmeBrowser->AddItem( kv, 0, false, false );

		KeyValues *pDrag = new KeyValues( "drag", "text", pElementName );
		pDrag->SetString( "texttype", "dmeName" );
		pDrag->SetInt( "dmeelement", vec[i].m_hElement );
		m_pDmeBrowser->SetItemDragData( nItemID, pDrag );
	}

	RefreshDmeList();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmePicker::OnKeyCodePressed( KeyCode code )
{
	if (( code == KEY_UP ) || ( code == KEY_DOWN ) || ( code == KEY_PAGEUP ) || ( code == KEY_PAGEDOWN ))
	{
		KeyValues *pMsg = new KeyValues("KeyCodePressed", "code", code);
		vgui::ipanel()->SendMessage( m_pDmeBrowser->GetVPanel(), pMsg, GetVPanel());
		pMsg->deleteThis();
	}
	else
	{
		BaseClass::OnKeyCodePressed( code );
	}
}


//-----------------------------------------------------------------------------
// Purpose: refreshes the file list
//-----------------------------------------------------------------------------
void CDmePicker::RefreshDmeList()
{
	// Check the filter matches
	int nMatchingElements = 0;
	int nTotalCount = 0;
	for ( int nItemID = m_pDmeBrowser->FirstItem(); nItemID != m_pDmeBrowser->InvalidItemID(); nItemID = m_pDmeBrowser->NextItem( nItemID ) )
	{
		KeyValues *kv = m_pDmeBrowser->GetItem( nItemID );
		const char *pElementName = kv->GetString( "dme" );
		bool bIsVisible = !m_Filter.Length() || Q_stristr( pElementName, m_Filter.Get() );
		m_pDmeBrowser->SetItemVisible( nItemID, bIsVisible );
		if ( bIsVisible )
		{
			++nMatchingElements;
		}
		++nTotalCount;
	}
	m_pDmeBrowser->SortList();

	char pColumnTitle[512];
	Q_snprintf( pColumnTitle, sizeof(pColumnTitle), "%s (%d/%d)",
		"Dme Elements", nMatchingElements, nTotalCount );
	m_pDmeBrowser->SetColumnHeaderText( 0, pColumnTitle );

	if ( ( m_pDmeBrowser->GetItemCount() > 0 ) && ( m_pDmeBrowser->GetSelectedItemsCount() == 0 ) )
	{
		int nItemID = m_pDmeBrowser->GetItemIDFromRow( 0 );
		m_pDmeBrowser->SetSelectedCell( nItemID, 0 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: refreshes dialog on text changing
//-----------------------------------------------------------------------------
void CDmePicker::OnTextChanged( )
{
	int nLength = m_pFilterList->GetTextLength();
	m_Filter.SetLength( nLength );
	if ( nLength > 0 )
	{
		m_pFilterList->GetText( m_Filter.GetForModify(), nLength+1 );
	}
	RefreshDmeList();
}


//-----------------------------------------------------------------------------
// Returns the selceted model name
//-----------------------------------------------------------------------------
CDmElement *CDmePicker::GetSelectedDme( )
{
	if ( m_pDmeBrowser->GetSelectedItemsCount() == 0 )
		return NULL;

	int nIndex = m_pDmeBrowser->GetSelectedItem( 0 );
	KeyValues *pItemKeyValues = m_pDmeBrowser->GetItem( nIndex );
	return GetElementKeyValue< CDmElement >( pItemKeyValues, "dmeHandle" );
}


//-----------------------------------------------------------------------------
//
// Purpose: Modal picker frame
//
//-----------------------------------------------------------------------------
CDmePickerFrame::CDmePickerFrame( vgui::Panel *pParent, const char *pTitle ) : 
BaseClass( pParent, "DmePickerFrame" )
{
	m_pContextKeyValues = NULL;
	SetDeleteSelfOnClose( true );
	m_pPicker = new CDmePicker( this );
	m_pPicker->AddActionSignalTarget( this );
	m_pOpenButton = new Button( this, "OpenButton", "#FileOpenDialog_Open", this, "Open" );
	m_pCancelButton = new Button( this, "CancelButton", "#FileOpenDialog_Cancel", this, "Cancel" );
	SetBlockDragChaining( true );

	LoadControlSettingsAndUserConfig( "resource/dmepickerframe.res" );

	SetTitle( pTitle, false );
}

CDmePickerFrame::~CDmePickerFrame()
{
	CleanUpMessage();
}


//-----------------------------------------------------------------------------
// Deletes the message
//-----------------------------------------------------------------------------
void CDmePickerFrame::CleanUpMessage()
{
	if ( m_pContextKeyValues )
	{
		m_pContextKeyValues->deleteThis();
		m_pContextKeyValues = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Activate the dialog
//-----------------------------------------------------------------------------
void CDmePickerFrame::DoModal( const CUtlVector< DmePickerInfo_t >& vec, KeyValues *pKeyValues )
{
	CleanUpMessage();
	m_pContextKeyValues = pKeyValues;
	m_pPicker->Activate( vec );
	m_pOpenButton->SetEnabled( vec.Count() != 0 );
	BaseClass::DoModal();
}


//-----------------------------------------------------------------------------
// On command
//-----------------------------------------------------------------------------
void CDmePickerFrame::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "Open" ) )
	{
		CDmElement *pElement = m_pPicker->GetSelectedDme( );

		KeyValues *pActionKeys = new KeyValues( "DmeSelected" );
		SetElementKeyValue( pActionKeys, "dme", pElement );
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
		KeyValues *pActionKeys = new KeyValues( "DmeSelectionCancelled" );
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

	BaseClass::OnCommand( pCommand );
}


