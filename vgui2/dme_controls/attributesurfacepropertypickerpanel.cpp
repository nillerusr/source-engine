//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/AttributeSurfacePropertyPickerPanel.h"
#include "dme_controls/AttributeTextEntry.h"
#include "tier1/KeyValues.h"
#include "filesystem.h"


using namespace vgui;


const char *SURFACEPROP_MANIFEST_FILE = "scripts/surfaceproperties_manifest.txt";


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAttributeSurfacePropertyPickerPanel::CAttributeSurfacePropertyPickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
}

CAttributeSurfacePropertyPickerPanel::~CAttributeSurfacePropertyPickerPanel()
{
}


//-----------------------------------------------------------------------------
// Reads the surface properties
//-----------------------------------------------------------------------------
void CAttributeSurfacePropertyPickerPanel::AddSurfacePropertiesToList( PickerList_t &list )
{
	KeyValues *manifest = new KeyValues( SURFACEPROP_MANIFEST_FILE );
	if ( manifest->LoadFromFile( g_pFullFileSystem, SURFACEPROP_MANIFEST_FILE, "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
		{
			if ( Q_stricmp( sub->GetName(), "file" ) )
				continue;
							  
			KeyValues *file = new KeyValues( SURFACEPROP_MANIFEST_FILE );
			if ( file->LoadFromFile( g_pFullFileSystem, sub->GetString(), "GAME" ) )
			{
				for ( KeyValues *pTrav = file; pTrav; pTrav = pTrav->GetNextKey() )
				{
					int i = list.AddToTail();
					list[i].m_pChoiceString = pTrav->GetName();
					list[i].m_pChoiceValue = pTrav->GetName();
				}
			}
			else
			{
				Warning( "Unable to load surface properties file '%s'\n", sub->GetString() );
			}
			file->deleteThis();
		}
	}
	else
	{
		Warning( "Unable to load manifest file '%s'\n", SURFACEPROP_MANIFEST_FILE );
	}

	manifest->deleteThis();
}


//-----------------------------------------------------------------------------
// Called when it's time to show the picker
//-----------------------------------------------------------------------------
void CAttributeSurfacePropertyPickerPanel::ShowPickerDialog()
{
	CPickerFrame *pSurfacePropPickerDialog = new CPickerFrame( this, "Select Surface Property", "Surface Property", "surfacePropertyName" );
	PickerList_t surfacePropList;
	AddSurfacePropertiesToList( surfacePropList );
	pSurfacePropPickerDialog->AddActionSignalTarget( this );
	pSurfacePropPickerDialog->DoModal( surfacePropList );
}


//-----------------------------------------------------------------------------
// Called by the picker dialog if a asset was selected
//-----------------------------------------------------------------------------
void CAttributeSurfacePropertyPickerPanel::OnPicked( KeyValues *pKeyValues )
{
	// Get the asset name back
	const char *pSurfacePropertyName = pKeyValues->GetString( "choice", NULL );
	if ( !pSurfacePropertyName || !pSurfacePropertyName[ 0 ] )
		return;

	// Apply to text panel
	m_pData->SetText( pSurfacePropertyName );
	SetDirty(true);
	if ( IsAutoApply() )
	{
		Apply();
	}
}
