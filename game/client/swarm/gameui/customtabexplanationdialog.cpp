//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "CustomTabExplanationDialog.h"
#include "BasePanel.h"
#include "convar.h"
#include "EngineInterface.h"
#include "GameUI_Interface.h"
#include "vgui/ISurface.h"
#include "vgui/IInput.h"
#include "ModInfo.h"
#include <stdio.h>

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCustomTabExplanationDialog::CCustomTabExplanationDialog(vgui::Panel *parent) : BaseClass(parent, "CustomTabExplanationDialog")
{
	SetDeleteSelfOnClose(true);
	SetSizeable( false );

	input()->SetAppModalSurface(GetVPanel());

	LoadControlSettings("Resource/CustomTabExplanationDialog.res");

	MoveToCenterOfScreen();

	GameUI().PreventEngineHideGameUI();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCustomTabExplanationDialog::~CCustomTabExplanationDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCustomTabExplanationDialog::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetDialogVariable( "game", ModInfo().GetGameName() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCustomTabExplanationDialog::OnKeyCodePressed(KeyCode code)
{
	if (code == KEY_ESCAPE)
	{
		Close();
	}
	else
	{
		BaseClass::OnKeyCodePressed(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: handles button commands
//-----------------------------------------------------------------------------
void CCustomTabExplanationDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "ok" ) || !stricmp( command, "cancel" ) || !stricmp( command, "close" ) )
	{
		Close();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCustomTabExplanationDialog::OnClose( void )
{
	BaseClass::OnClose();
	GameUI().AllowEngineHideGameUI();
}
