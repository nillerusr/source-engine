//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "BasePanel.h"
#include "OptionsDialog.h"

#include "vgui_controls/Button.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/QueryBox.h"

#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"

#include "KeyValues.h"
#include "OptionsSubKeyboard.h"
#include "OptionsSubMouse.h"
#include "OptionsSubAudio.h"
#include "OptionsSubVideo.h"
#include "OptionsSubVoice.h"
#include "OptionsSubMultiplayer.h"
#include "OptionsSubDifficulty.h"
#include "OptionsSubPortal.h"
#ifdef WIN32
// NVNT haptic configuration dialog
#include "OptionsSubHaptics.h"
#endif
#include "ModInfo.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
COptionsDialog::COptionsDialog(vgui::Panel *parent) : PropertyDialog(parent, "OptionsDialog")
{
	SetDeleteSelfOnClose(true);
	SetBounds(0, 0, 512, 406);
	SetSizeable( false );

	SetTitle("#GameUI_Options", true);

	// debug timing code, this function takes too long
//	double s4 = system()->GetCurrentTime();

#if defined( WIN32 ) && !defined( _X360 )
	// NVNT START see if the user has a haptic device via convar. if so create haptics dialog.
	ConVarRef checkHap("hap_HasDevice");
	checkHap.Init("hap_HasDevice",true);
	if(checkHap.GetBool())
	{
		AddPage(new COptionsSubHaptics(this), "#GameUI_Haptics_TabTitle");
	}
	// NVNT END
#endif
	if (ModInfo().IsSinglePlayerOnly() && !ModInfo().NoDifficulty())
	{
		AddPage(new COptionsSubDifficulty(this), "#GameUI_Difficulty");
	}

	if (ModInfo().HasPortals())
	{
		AddPage(new COptionsSubPortal(this), "#GameUI_Portal");
	}

	AddPage(new COptionsSubKeyboard(this), "#GameUI_Keyboard");
	AddPage(new COptionsSubMouse(this), "#GameUI_Mouse");

	m_pOptionsSubAudio = new COptionsSubAudio(this);
	AddPage(m_pOptionsSubAudio, "#GameUI_Audio");
	m_pOptionsSubVideo = new COptionsSubVideo(this);
	AddPage(m_pOptionsSubVideo, "#GameUI_Video");

	if ( !ModInfo().IsSinglePlayerOnly() ) 
	{
		AddPage(new COptionsSubVoice(this), "#GameUI_Voice");
	}

	// add the multiplay page last, if we're combo single/multi or just multi
	if ( (ModInfo().IsMultiplayerOnly() && !ModInfo().IsSinglePlayerOnly()) ||
		 (!ModInfo().IsMultiplayerOnly() && !ModInfo().IsSinglePlayerOnly()) )
	{
		m_pOptionsSubMultiplayer = new COptionsSubMultiplayer(this);
		AddPage(m_pOptionsSubMultiplayer, "#GameUI_Multiplayer");
	}

//	double s5 = system()->GetCurrentTime();
//	Msg("COptionsDialog::COptionsDialog(): %.3fms\n", (float)(s5 - s4) * 1000.0f);

	SetApplyButtonVisible(true);
	GetPropertySheet()->SetTabWidth(84);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COptionsDialog::~COptionsDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Brings the dialog to the fore
//-----------------------------------------------------------------------------
void COptionsDialog::Activate()
{
	BaseClass::Activate();
	EnableApplyButton(false);
}

void COptionsDialog::OnKeyCodePressed( KeyCode code )
{
	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		OnCommand( "Cancel" );
		return;
	}

	BaseClass::OnKeyCodePressed( code );
}

//-----------------------------------------------------------------------------
// Purpose: Opens the dialog
//-----------------------------------------------------------------------------
void COptionsDialog::Run()
{
	SetTitle("#GameUI_Options", true);
	Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Called when the GameUI is hidden
//-----------------------------------------------------------------------------
void COptionsDialog::OnGameUIHidden()
{
	// tell our children about it
	for ( int i = 0 ; i < GetChildCount() ; i++ )
	{
		Panel *pChild = GetChild( i );
		if ( pChild )
		{
			PostMessage( pChild, new KeyValues( "GameUIHidden" ) );
		}
	}
}
