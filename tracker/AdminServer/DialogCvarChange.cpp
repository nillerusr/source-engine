//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include "DialogCvarChange.h"

#include <vgui/IInput.h>
#include <vgui/ISurface.h>
#include <KeyValues.h>

#include <vgui_controls/Button.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDialogCvarChange::CDialogCvarChange(vgui::Panel *parent) : Frame(parent, "DialogCvarChange")
{
	SetSize(320, 200);

	m_bAddCvarText = true;

	m_pInfoLabel = new Label(this, "InfoLabel", "");
	m_pCvarLabel = new Label(this, "CvarLabel", "");
	m_pCvarEntry = new TextEntry(this, "CvarEntry");
	m_pOkayButton = new Button(this, "OkayButton", "#Okay_Button");

	LoadControlSettings("Admin/DialogCvarChange.res", "PLATFORM");

	SetTitle("#Cvar_Title", true);

	SetSizeable(false);
	// set our initial position in the middle of the workspace
	MoveToCenterOfScreen();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDialogCvarChange::~CDialogCvarChange()
{
}

//-----------------------------------------------------------------------------
// Purpose: Hides value text
//-----------------------------------------------------------------------------
void CDialogCvarChange::MakePassword()
{
	m_pCvarEntry->SetTextHidden(true);
	m_bAddCvarText = false; // this isn't asking about a cvar
}

//-----------------------------------------------------------------------------
// Purpose: initializes the dialog and brings it to the foreground
//-----------------------------------------------------------------------------
void CDialogCvarChange::Activate(const char *cvarName, const char *curValue, const char *type, const char *question)
{
	m_pCvarLabel->SetText(cvarName);
	if (!m_bAddCvarText)
	{
		m_pCvarLabel->SetVisible(false); // hide this
	}

	m_pInfoLabel->SetText(question);

	m_cType=type;

	m_pOkayButton->SetAsDefaultButton(true);
	MakePopup();
	MoveToFront();

	m_pCvarEntry->SetText(curValue);
	m_pCvarEntry->RequestFocus();
	RequestFocus();

	// make it modal
	input()->SetAppModalSurface(GetVPanel());
	SetVisible(true);

	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the text of a labell by name
//-----------------------------------------------------------------------------
void CDialogCvarChange::SetLabelText(const char *textEntryName, const char *text)
{
	Label *entry = dynamic_cast<Label *>(FindChildByName(textEntryName));
	if (entry)
	{
		entry->SetText(text);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles button presses
//-----------------------------------------------------------------------------
void CDialogCvarChange::OnCommand(const char *command)
{
	bool bClose = false;

	if (!stricmp(command, "Okay"))
	{
		KeyValues *msg = new KeyValues("CvarChangeValue");
		char buf[64];
		m_pCvarLabel->GetText(buf,64);

		msg->SetString("player", buf );
		m_pCvarEntry->GetText(buf, sizeof(buf)-1);
		msg->SetString("value", buf);
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
		Close();
	}
}

//-----------------------------------------------------------------------------
// Purpose: deletes the dialog on close
//-----------------------------------------------------------------------------
void CDialogCvarChange::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}




