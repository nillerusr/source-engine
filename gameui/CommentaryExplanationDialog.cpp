//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "CommentaryExplanationDialog.h"
#include "BasePanel.h"
#include "convar.h"
#include "EngineInterface.h"
#include "GameUI_Interface.h"
#include "vgui/ISurface.h"
#include "vgui/IInput.h"

#include <stdio.h>

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCommentaryExplanationDialog::CCommentaryExplanationDialog(vgui::Panel *parent, char *pszFinishCommand) : BaseClass(parent, "CommentaryExplanationDialog")
{
	SetDeleteSelfOnClose(true);
	SetSizeable( false );

	input()->SetAppModalSurface(GetVPanel());

	LoadControlSettings("Resource/CommentaryExplanationDialog.res");

	MoveToCenterOfScreen();

	GameUI().PreventEngineHideGameUI();

	// Save off the finish command
	Q_snprintf( m_pszFinishCommand, sizeof( m_pszFinishCommand ), "%s", pszFinishCommand );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCommentaryExplanationDialog::~CCommentaryExplanationDialog()
{
}

void CCommentaryExplanationDialog::OnKeyCodeTyped(KeyCode code)
{
	if ( code == KEY_ESCAPE )
	{
		OnCommand( "cancel" );
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommentaryExplanationDialog::OnKeyCodePressed(KeyCode code)
{
	if ( GetBaseButtonCode( code ) == KEY_XBUTTON_B || GetBaseButtonCode( code ) == STEAMCONTROLLER_B )
	{
		OnCommand( "cancel" );
	}
	else if ( GetBaseButtonCode( code ) == KEY_XBUTTON_A || GetBaseButtonCode( code ) == STEAMCONTROLLER_A )
	{
		OnCommand( "ok" );
	}
	else
	{
		BaseClass::OnKeyCodePressed(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: handles button commands
//-----------------------------------------------------------------------------
void CCommentaryExplanationDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "ok" ) )
	{
		Close();
		BasePanel()->FadeToBlackAndRunEngineCommand( m_pszFinishCommand );
	}
	else if ( !stricmp( command, "cancel" ) || !stricmp( command, "close" ) )
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
void CCommentaryExplanationDialog::OnClose( void )
{
	BaseClass::OnClose();
	GameUI().AllowEngineHideGameUI();
}
