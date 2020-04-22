//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/AttributeDetailTypePickerPanel.h"
#include "dme_controls/AttributeTextEntry.h"
#include "tier1/KeyValues.h"
#include "filesystem.h"


using namespace vgui;


const char *DETAILTYPE_FILE = "detail.vbsp";


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAttributeDetailTypePickerPanel::CAttributeDetailTypePickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
}

CAttributeDetailTypePickerPanel::~CAttributeDetailTypePickerPanel()
{
}


//-----------------------------------------------------------------------------
// Reads the detail types
//-----------------------------------------------------------------------------
void CAttributeDetailTypePickerPanel::AddDetailTypesToList( PickerList_t &list )
{
	KeyValues *pDetailTypes = new KeyValues( DETAILTYPE_FILE );
	if ( pDetailTypes->LoadFromFile( g_pFullFileSystem, DETAILTYPE_FILE, "GAME" ) )
	{
		for ( KeyValues *sub = pDetailTypes->GetFirstTrueSubKey(); sub != NULL; sub = sub->GetNextTrueSubKey() )
		{
			int i = list.AddToTail( );
			list[i].m_pChoiceString = sub->GetName();
			list[i].m_pChoiceValue = sub->GetName();
		}
	}
	else
	{
		Warning( "Unable to load detail prop file '%s'\n", DETAILTYPE_FILE );
	}

	pDetailTypes->deleteThis();
}


//-----------------------------------------------------------------------------
// Called when it's time to show the picker
//-----------------------------------------------------------------------------
void CAttributeDetailTypePickerPanel::ShowPickerDialog()
{
	CPickerFrame *pDetailTypePickerDialog = new CPickerFrame( this, "Select Detail Type", "Detail Type", "detailTypeName" );
	PickerList_t detailTypeList;
	AddDetailTypesToList( detailTypeList );
	pDetailTypePickerDialog->AddActionSignalTarget( this );
	pDetailTypePickerDialog->DoModal( detailTypeList );
}


//-----------------------------------------------------------------------------
// Called by the picker dialog if a asset was selected
//-----------------------------------------------------------------------------
void CAttributeDetailTypePickerPanel::OnPicked( KeyValues *pKeyValues )
{
	// Get the detail type name back
	const char *pDetailTypeName = pKeyValues->GetString( "choice", NULL );
	if ( !pDetailTypeName || !pDetailTypeName[ 0 ] )
		return;

	// Apply to text panel
	m_pData->SetText( pDetailTypeName );
	SetDirty(true);
	if ( IsAutoApply() )
	{
		Apply();
	}
}
