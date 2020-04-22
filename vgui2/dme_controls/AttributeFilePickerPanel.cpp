//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/AttributeFilePickerPanel.h"
#include "filesystem.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/FileOpenDialog.h"
#include "dme_controls/AttributeTextEntry.h"
#include "vgui/IInput.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
// Various file picker types
//-----------------------------------------------------------------------------
IMPLEMENT_ATTRIBUTE_FILE_PICKER( CAttributeTgaFilePickerPanel, "Choose TGA file", "TGA", "tga" );
IMPLEMENT_ATTRIBUTE_FILE_PICKER( CAttributeDmeFilePickerPanel, "Choose DmE .xml file", "DmE XML", "xml" );
IMPLEMENT_ATTRIBUTE_FILE_PICKER( CAttributeAviFilePickerPanel, "Choose AVI file", "AVI", "avi" );
IMPLEMENT_ATTRIBUTE_FILE_PICKER( CAttributeShtFilePickerPanel, "Choose Sheet file", "SHT", "sht" );
IMPLEMENT_ATTRIBUTE_FILE_PICKER( CAttributeRawFilePickerPanel, "Choose RAW file", "RAW", "raw" );

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAttributeFilePickerPanel::CAttributeFilePickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
}

CAttributeFilePickerPanel::~CAttributeFilePickerPanel()
{
}


//-----------------------------------------------------------------------------
// Shows the picker dialog
//-----------------------------------------------------------------------------
void CAttributeFilePickerPanel::ShowPickerDialog()
{
	FileOpenDialog *pFileOpenDialog = new FileOpenDialog( this, "Choose file", true );
	SetupFileOpenDialog( pFileOpenDialog );
	pFileOpenDialog->AddActionSignalTarget( this );
	pFileOpenDialog->SetDeleteSelfOnClose( true );
	pFileOpenDialog->DoModal( true );
	input()->SetAppModalSurface( pFileOpenDialog->GetVPanel() );
}

void CAttributeFilePickerPanel::OnFileSelected( char const *fullpath )
{
	if ( !fullpath || !fullpath[ 0 ] )
		return;

	char relativepath[ 512 ];
	g_pFullFileSystem->FullPathToRelativePath( fullpath, relativepath, sizeof( relativepath ) );

	// Apply to text panel
	m_pData->SetText( relativepath );
	SetDirty(true);
	if ( IsAutoApply() )
	{
		Apply();
	}
}