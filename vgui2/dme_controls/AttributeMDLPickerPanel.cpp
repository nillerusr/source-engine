//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "dme_controls/AttributeMDLPickerPanel.h"
#include "filesystem.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/FileOpenDialog.h"
#include "dme_controls/AttributeTextEntry.h"
#include "matsys_controls/MDLPicker.h"
#include "tier1/KeyValues.h"


using namespace vgui;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAttributeMDLPickerPanel::CAttributeMDLPickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
}

CAttributeMDLPickerPanel::~CAttributeMDLPickerPanel()
{
}


//-----------------------------------------------------------------------------
// Called when it's time to show the MDL picker
//-----------------------------------------------------------------------------
void CAttributeMDLPickerPanel::ShowPickerDialog()
{
	// Open file
	CMDLPickerFrame *pMDLPickerDialog = new CMDLPickerFrame( this, "Select .MDL File" );
	pMDLPickerDialog->AddActionSignalTarget( this );
	pMDLPickerDialog->DoModal( );
}


//-----------------------------------------------------------------------------
// Called when it's time to show the MDL picker
//-----------------------------------------------------------------------------
void CAttributeMDLPickerPanel::OnMDLSelected( KeyValues *pKeyValues )
{
	const char *pMDLName = pKeyValues->GetString( "mdl", NULL );
	if ( !pMDLName || !pMDLName[ 0 ] )
		return;

	// Apply to text panel
	m_pData->SetText( pMDLName );
	SetDirty(true);
	if ( IsAutoApply() )
	{
		Apply();
	}
}