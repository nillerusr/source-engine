//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

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
#include "ModInfo.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
COptionsDialog::COptionsDialog(vgui::Panel *parent, OptionsDialogTabStyle iTabStyle) : PropertyDialog(parent, "OptionsDialog")
{
	SetProportional( true );
	SetDeleteSelfOnClose( true );
	SetBounds( 
		0, 
		0, 
		vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 512 ),
		vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 415 ) );
	SetSizeable( false );

	// debug timing code, this function takes too long
//	double s4 = system()->GetCurrentTime();

	if ( iTabStyle == OPTIONS_DIALOG_ALL_TABS )
	{
		SetTitle("#GameUI_Options", true);

		if ( ModInfo().IsSinglePlayerOnly() && !ModInfo().NoDifficulty() )
		{
			AddPage(new COptionsSubDifficulty(this), "#GameUI_Difficulty");
		}

		if ( ModInfo().HasPortals() )
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
			AddPage(new COptionsSubMultiplayer(this), "#GameUI_Multiplayer");
		}
	}
	else if ( iTabStyle == OPTIONS_DIALOG_ONLY_BINDING_TABS )
	{
		SetTitle("#L4D360UI_Controller_Edit_Keys_Buttons", true);

		AddPage(new COptionsSubKeyboard(this), "#GameUI_Console_UserSettings");
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

//-----------------------------------------------------------------------------
// Purpose: Opens the dialog
//-----------------------------------------------------------------------------
void COptionsDialog::Run()
{
	SetTitle("#GameUI_Options", true);
	Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Opens the gamma dialog directly
//-----------------------------------------------------------------------------
void COptionsDialog::OpenGammaDialog()
{
	m_pOptionsSubVideo->OpenGammaDialog();
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
