//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "GameConsoleDialog.h"
#include "GameUI_Interface.h"
#include "vgui/iinput.h"
#include "vgui/isurface.h"
#include "vgui/keycode.h"
#include "LoadingDialog.h"
#include "igameuifuncs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameConsoleDialog::CGameConsoleDialog() : BaseClass( NULL, "GameConsole", false )
{
	AddActionSignalTarget( this );
}


//-----------------------------------------------------------------------------
// Purpose: generic vgui command handler
//-----------------------------------------------------------------------------
void CGameConsoleDialog::OnCommand(const char *command)
{
	if ( !Q_stricmp( command, "Close" ) )
	{
		if ( GameUI().IsInBackgroundLevel() )
		{
			// Tell the engine we've hid the console, so that it unpauses the game
			// even though we're still sitting at the menu.
			engine->ClientCmd_Unrestricted( "unpause" );
		}
	}

	BaseClass::OnCommand(command);
}


//-----------------------------------------------------------------------------
// HACK: Allow F key bindings to operate even when typing in the text entry field
//-----------------------------------------------------------------------------
void CGameConsoleDialog::OnKeyCodeTyped(KeyCode code)
{
	BaseClass::OnKeyCodeTyped(code);

	// check for processing
	if ( m_pConsolePanel->TextEntryHasFocus() )
	{
		// HACK: Allow F key bindings to operate even here
		if ( code >= KEY_F1 && code <= KEY_F12 )
		{
			// See if there is a binding for the FKey
			const char *binding = gameuifuncs->GetBindingForButtonCode( code );
			if ( binding && binding[0] )
			{
				// submit the entry as a console commmand
				char szCommand[256];
				Q_strncpy( szCommand, binding, sizeof( szCommand ) );
				engine->ClientCmd_Unrestricted( szCommand );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Submits a command
//-----------------------------------------------------------------------------
void CGameConsoleDialog::OnCommandSubmitted( const char *pCommand )
{
	engine->ClientCmd_Unrestricted( pCommand );
}


//-----------------------------------------------------------------------------
// Submits a command
//-----------------------------------------------------------------------------
void CGameConsoleDialog::OnClosedByHittingTilde()
{
	if ( !LoadingDialog() )
	{
		GameUI().HideGameUI();
	}
	else
	{
		vgui::surface()->RestrictPaintToSinglePanel( LoadingDialog()->GetVPanel() );
	}
}
