//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/AttributeSoundPickerPanel.h"
#include "dme_controls/soundpicker.h"
#include "tier1/KeyValues.h"
#include "dme_controls/AttributeTextEntry.h"
#include "datamodel/dmelement.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAttributeSoundPickerPanel::CAttributeSoundPickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
}

CAttributeSoundPickerPanel::~CAttributeSoundPickerPanel()
{
}


//-----------------------------------------------------------------------------
// Called when it's time to show the sound picker
//-----------------------------------------------------------------------------
void CAttributeSoundPickerPanel::ShowPickerDialog()
{
	// Open file
	CSoundPicker::PickType_t pickType = CSoundPicker::PICK_ALL;
	const char *pTextType = GetTextType();
	if ( pTextType )
	{
		if ( !Q_stricmp( pTextType, "gamesoundName" ) )
		{
			pickType = CSoundPicker::PICK_GAMESOUNDS;
		}
		else if ( !Q_stricmp( pTextType, "wavName" ) )
		{
			pickType = CSoundPicker::PICK_WAVFILES;
		}
	}

	const char *pCurrentSound = GetAttributeValue<CUtlString>().Get();
	CSoundPickerFrame *pSoundPickerDialog = new CSoundPickerFrame( this, "Select sound", pickType );
	pSoundPickerDialog->AddActionSignalTarget( this );

	if ( pickType == CSoundPicker::PICK_ALL )
	{
		pickType = CSoundPicker::PICK_NONE;
	}
	pSoundPickerDialog->DoModal( pickType, pCurrentSound );
}


//-----------------------------------------------------------------------------
// Called when the sound picker has picked a sound
//-----------------------------------------------------------------------------
void CAttributeSoundPickerPanel::OnSoundSelected( KeyValues *pKeyValues )
{
	// We're either going to get an activity or sequence name
	const char *pGameSoundName = pKeyValues->GetString( "gamesound", NULL );
	const char *pSoundName = pKeyValues->GetString( "wav", pGameSoundName );
	if ( !pSoundName || !pSoundName[ 0 ] )
		return;

	// Apply to text panel
	m_pData->SetText( pSoundName );
	SetDirty(true);
	if ( IsAutoApply() )
	{
		Apply();
	}
}
