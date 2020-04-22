//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include "DialogKickPlayer.h"

#include <VGUI_Button.h>
#include <VGUI_KeyValues.h>
#include <VGUI_Label.h>
#include <VGUI_TextEntry.h>

#include <VGUI_Controls.h>
#include <VGUI_ISurface.h>

using namespace vgui;

#define max(a,b)            (((a) > (b)) ? (a) : (b))


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDialogKickPlayer::CDialogKickPlayer() : Frame(NULL, "DialogKickPlayer")
{
	SetSize(320, 200);

	m_pInfoLabel = new Label(this, "InfoLabel", "Kick Player?");
	m_pPlayerLabel = new Label(this, "PlayerLabel", "<player name>");
	m_pOkayButton = new Button(this, "OkayButton", "&Okay");

	LoadControlSettings("Admin\\DialogKickPlayer.res");

	SetTitle("Kick/Ban/Status Player", true);

	// set our initial position in the middle of the workspace
	MoveToCenterOfScreen();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDialogKickPlayer::~CDialogKickPlayer()
{
}

//-----------------------------------------------------------------------------
// Purpose: initializes the dialog and brings it to the foreground
//-----------------------------------------------------------------------------
void CDialogKickPlayer::Activate(const char *playerName,const char *question,const char *type)
{
	m_pPlayerLabel->SetText(playerName);
	m_pInfoLabel->SetText(question);
	m_pInfoLabel->SizeToContents();

//	SetSize(
	int wide,tall;
	m_pInfoLabel->GetSize(wide,tall);

	SetWide(max(wide+50,GetWide()));

	m_cType=type;

	m_pOkayButton->SetAsDefaultButton(true);
	MakePopup();
	MoveToFront();

	RequestFocus();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CDialogKickPlayer::OnCommand(const char *command)
{
	bool bClose = false;

	if (!stricmp(command, "Okay"))
	{
		KeyValues *msg = new KeyValues("KickPlayer");
		char buf[64];
		m_pPlayerLabel->GetText(buf,64);
		msg->SetString("player", buf );
		msg->SetString("type",m_cType);

		PostActionSignal(msg);

		bClose = true;
	}
	else if (!stricmp(command, "Close"))
	{
		bClose = true;
	}
	else
	{
		BaseClass::OnCommand(command);
	}

	if (bClose)
	{
		PostMessage(this, new KeyValues("Close"));
		MarkForDeletion();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDialogKickPlayer::PerformLayout()
{
	BaseClass::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: deletes the dialog on close
//-----------------------------------------------------------------------------
void CDialogKickPlayer::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}




