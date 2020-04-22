//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/AttributeAssetPickerPanel.h"
#include "dme_controls/AttributeTextEntry.h"
#include "matsys_controls/AssetPicker.h"
#include "matsys_controls/VtfPicker.h"
#include "matsys_controls/VMTPicker.h"
#include "tier1/KeyValues.h"


using namespace vgui;


//-----------------------------------------------------------------------------
// Assets
//-----------------------------------------------------------------------------
IMPLEMENT_ATTRIBUTE_ASSET_PICKER( CAttributeBspPickerPanel, "Select .BSP file", "BSP Files", "bsp", "maps", "bspName" );
IMPLEMENT_ATTRIBUTE_ASSET_PREVIEW_PICKER( CAttributeVmtPickerPanel, CVMTPickerFrame, "Select .VMT file" );
IMPLEMENT_ATTRIBUTE_ASSET_PREVIEW_PICKER( CAttributeVtfPickerPanel, CVTFPickerFrame, "Select .VTF file" );


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAttributeAssetPickerPanel::CAttributeAssetPickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
}

CAttributeAssetPickerPanel::~CAttributeAssetPickerPanel()
{
}


//-----------------------------------------------------------------------------
// Called when it's time to show the BSP picker
//-----------------------------------------------------------------------------
void CAttributeAssetPickerPanel::ShowPickerDialog()
{
	CBaseAssetPickerFrame *pAssetPickerDialog = CreateAssetPickerFrame( );
	pAssetPickerDialog->AddActionSignalTarget( this );
	pAssetPickerDialog->DoModal( );
}


//-----------------------------------------------------------------------------
// Called by the asset picker dialog if a asset was selected
//-----------------------------------------------------------------------------
void CAttributeAssetPickerPanel::OnAssetSelected( KeyValues *pKeyValues )
{
	// Get the asset name back
	const char *pAssetName = pKeyValues->GetString( "asset", NULL );
	if ( !pAssetName || !pAssetName[ 0 ] )
		return;

	// Apply to text panel
	m_pData->SetText( pAssetName );
	SetDirty(true);
	if ( IsAutoApply() )
	{
		Apply();
	}
}
