//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Dialog allowing users to select presets from within a preset group
//
//===========================================================================//

#include "dme_controls/presetpicker.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "vgui/IVGui.h"
#include "vgui_controls/button.h"
#include "vgui_controls/listpanel.h"
#include "vgui_controls/splitter.h"
#include "vgui_controls/messagebox.h"
#include "movieobjects/dmeanimationset.h"
#include "datamodel/dmelement.h"
#include "matsys_controls/picker.h"
#include "dme_controls/dmecontrols_utils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

static int __cdecl PresetNameSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString( "name" );
	const char *string2 = item2.kv->GetString( "name" );
	return Q_stricmp( string1, string2 );
}

CPresetPickerFrame::CPresetPickerFrame( vgui::Panel *pParent, const char *pTitle, bool bAllowMultiSelect ) : 
	BaseClass( pParent, "PresetPickerFrame" )
{
	SetDeleteSelfOnClose( true );
	m_pContextKeyValues = NULL;

	m_pPresetList = new vgui::ListPanel( this, "PresetList" );
	m_pPresetList->AddColumnHeader( 0, "name", "Preset Name", 52, 0 );
	m_pPresetList->SetSelectIndividualCells( false );
	m_pPresetList->SetMultiselectEnabled( bAllowMultiSelect );
	m_pPresetList->SetEmptyListText( "No presets" );
	m_pPresetList->AddActionSignalTarget( this );
	m_pPresetList->SetSortFunc( 0, PresetNameSortFunc );
	m_pPresetList->SetSortColumn( 0 );

	m_pOpenButton = new vgui::Button( this, "OkButton", "#MessageBox_OK", this, "Ok" );
	m_pCancelButton = new vgui::Button( this, "CancelButton", "#MessageBox_Cancel", this, "Cancel" );
	SetBlockDragChaining( true );

	LoadControlSettingsAndUserConfig( "resource/presetpicker.res" );

	SetTitle( pTitle, false );
}

CPresetPickerFrame::~CPresetPickerFrame()
{
	CleanUpMessage();
}


//-----------------------------------------------------------------------------
// Refreshes the list of presets
//-----------------------------------------------------------------------------
void CPresetPickerFrame::RefreshPresetList( CDmElement *pPresetGroup, bool bSelectAll )
{
	m_pPresetList->RemoveAll();

	const CDmrElementArray< CDmePreset > presets( pPresetGroup, "presets" );
	if ( !presets.IsValid() )
		return;

	int nCount = presets.Count();
	if ( nCount == 0 )
		return;

	for ( int i = 0; i < nCount; ++i )
	{
		CDmePreset *pPreset = presets[i];

		const char *pName = pPreset->GetName();
		if ( !pName || !pName[0] )
		{
			pName = "<no name>";
		}

		KeyValues *kv = new KeyValues( "node" );
		kv->SetString( "name", pName ); 
		SetElementKeyValue( kv, "preset", pPreset );

		int nItemID = m_pPresetList->AddItem( kv, 0, false, false );
		if ( bSelectAll )
		{
			m_pPresetList->AddSelectedItem( nItemID );
		}
	}
	m_pPresetList->SortList();
}


//-----------------------------------------------------------------------------
// Deletes the message
//-----------------------------------------------------------------------------
void CPresetPickerFrame::CleanUpMessage()
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
void CPresetPickerFrame::DoModal( CDmElement *pPresetGroup, bool bSelectAll, KeyValues *pContextKeyValues )
{
	CleanUpMessage();
	RefreshPresetList( pPresetGroup, bSelectAll );
	m_pContextKeyValues = pContextKeyValues;
	BaseClass::DoModal();
}


//-----------------------------------------------------------------------------
// On command
//-----------------------------------------------------------------------------
void CPresetPickerFrame::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "Ok" ) )
	{
		int nSelectedItemCount = m_pPresetList->GetSelectedItemsCount();
		if ( nSelectedItemCount == 0 )
			return;

		KeyValues *pActionKeys = new KeyValues( "PresetPicked" );

		if ( m_pPresetList->IsMultiselectEnabled() )
		{
			pActionKeys->SetInt( "count", nSelectedItemCount );

			// Adds them in selection order
			for ( int i = 0; i < nSelectedItemCount; ++i )
			{
				char pBuf[32];
				Q_snprintf( pBuf, sizeof(pBuf), "%d", i );

				int nItemID = m_pPresetList->GetSelectedItem( i );
				KeyValues *pKeyValues = m_pPresetList->GetItem( nItemID );
				CDmePreset *pPreset = GetElementKeyValue<CDmePreset>( pKeyValues, "preset" );

				SetElementKeyValue( pActionKeys, pBuf, pPreset );
			}
		}
		else
		{
			int nItemID = m_pPresetList->GetSelectedItem( 0 );
			KeyValues *pKeyValues = m_pPresetList->GetItem( nItemID );
			CDmePreset *pPreset = GetElementKeyValue<CDmePreset>( pKeyValues, "preset" );
			SetElementKeyValue( pActionKeys, "preset", pPreset );
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
		KeyValues *pActionKeys = new KeyValues( "PresetPickCancelled" );
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

