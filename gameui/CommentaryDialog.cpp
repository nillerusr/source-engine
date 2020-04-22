//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "CommentaryDialog.h"
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
CCommentaryDialog::CCommentaryDialog(vgui::Panel *parent) : BaseClass(parent, "CommentaryDialog")
{
	SetDeleteSelfOnClose(true);
	SetSizeable( false );

	input()->SetAppModalSurface(GetVPanel());
	vgui::surface()->RestrictPaintToSinglePanel(GetVPanel());
	GameUI().PreventEngineHideGameUI();

	SetTitle("#GameUI_CommentaryDialogTitle", true);

	LoadControlSettings("Resource/CommentaryDialog.res");

	MoveToCenterOfScreen();

	bool bCommentaryOn = false;
	ConVarRef commentary( "commentary" );
	if ( commentary.IsValid() )
	{
		bCommentaryOn = commentary.GetBool();
	}

	// Setup the buttons & labels to reflect the current state of the commentary
	if ( bCommentaryOn )
	{
		SetControlString( "ModeLabel", "#GAMEUI_Commentary_LabelOn" );
		SetControlString( "TurnOnButton", "#GAMEUI_Commentary_LeaveOn" );
		SetControlString( "TurnOffButton", "#GAMEUI_Commentary_TurnOff" );
	}
	else
	{
		SetControlString( "ModeLabel", "#GAMEUI_Commentary_LabelOff" );
		SetControlString( "TurnOnButton", "#GAMEUI_Commentary_TurnOn" );
		SetControlString( "TurnOffButton", "#GAMEUI_Commentary_LeaveOff" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCommentaryDialog::~CCommentaryDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCommentaryDialog::OnClose( void )
{
	BaseClass::OnClose();

	vgui::surface()->RestrictPaintToSinglePanel(NULL);
	GameUI().AllowEngineHideGameUI();

	// Bring up the post dialog
	DHANDLE<CPostCommentaryDialog> hPostCommentaryDialog;
	if ( !hPostCommentaryDialog.Get() )
	{
		hPostCommentaryDialog = new CPostCommentaryDialog( BasePanel() );
	}
	hPostCommentaryDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CCommentaryDialog::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "TurnOn" ) )
	{
		ConVarRef commentary("commentary");
		commentary.SetValue( 1 );
		Close();
	}
	else if ( !Q_stricmp( command, "TurnOff" ) )
	{
		ConVarRef commentary("commentary");
		commentary.SetValue( 0 );
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
void CCommentaryDialog::OnKeyCodePressed(KeyCode code)
{
	// Ignore escape key
	if ( code == KEY_ESCAPE )
		return;

	if ( code == KEY_XBUTTON_A || code == STEAMCONTROLLER_A )
	{
		OnCommand( "TurnOn" );
		return;
	}
	else if ( code == KEY_XBUTTON_B || code == STEAMCONTROLLER_B )
	{
		OnCommand( "TurnOff" );
		return;
	}

	BaseClass::OnKeyCodePressed(code);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void OpenCommentaryDialog( void )
{
	DHANDLE<CCommentaryDialog> hCommentaryDialog;
	if ( !hCommentaryDialog.Get() )
	{
		hCommentaryDialog = new CCommentaryDialog( BasePanel() );
	}

	GameUI().ActivateGameUI();
	hCommentaryDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ConVar commentary_firstrun("commentary_firstrun", "0", FCVAR_ARCHIVE );
void CC_CommentaryTestFirstRun( void )
{
	// The enable/disable commentary box in the sound options got lost in time;
	// always prompt the user for commentary mode instead on new game.
	//if ( !commentary_firstrun.GetBool() )
	{
		commentary_firstrun.SetValue(1);
		OpenCommentaryDialog();
	}
}
static ConCommand commentary_testfirstrun("commentary_testfirstrun", CC_CommentaryTestFirstRun, 0 );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPostCommentaryDialog::CPostCommentaryDialog(vgui::Panel *parent) : BaseClass(parent, "PostCommentaryDialog")
{
	SetDeleteSelfOnClose(true);
	SetSizeable( false );

	input()->SetAppModalSurface(GetVPanel());
	vgui::surface()->RestrictPaintToSinglePanel(GetVPanel());
	m_bResetPaintRestrict = false;

	SetTitle("#GameUI_CommentaryDialogTitle", true);

	LoadControlSettings("Resource/PostCommentaryDialog.res");

	MoveToCenterOfScreen();

	bool bCommentaryOn = false;
	ConVarRef commentary("commentary");
	if ( commentary.IsValid() )
	{
		bCommentaryOn = commentary.GetBool();
	}
 
	// Setup the buttons & labels to reflect the current state of the commentary
	if ( bCommentaryOn )
	{
		SetControlString( "PostModeLabel", "#GAMEUI_PostCommentary_ModeLabelOn" );
	}
	else
	{
		SetControlString( "PostModeLabel", "#GAMEUI_PostCommentary_ModeLabelOff" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPostCommentaryDialog::~CPostCommentaryDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPostCommentaryDialog::OnFinishedClose( void )
{
	BaseClass::OnFinishedClose();

	if ( !m_bResetPaintRestrict )
	{
		m_bResetPaintRestrict = true;
		vgui::surface()->RestrictPaintToSinglePanel(NULL);
		GameUI().HideGameUI();
	}
}

void CPostCommentaryDialog::OnKeyCodeTyped(KeyCode code)
{
	if ( code == KEY_ESCAPE )
	{
		Close();
		vgui::surface()->RestrictPaintToSinglePanel(NULL);
		m_bResetPaintRestrict = true;
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPostCommentaryDialog::OnKeyCodePressed(KeyCode code)
{
   	if ( code == KEY_XBUTTON_A || code == KEY_XBUTTON_B || code == STEAMCONTROLLER_B )
	{
		Close();
		vgui::surface()->RestrictPaintToSinglePanel(NULL);
		m_bResetPaintRestrict = true;
	}
	else
	{
		BaseClass::OnKeyCodePressed(code);
	}
}
