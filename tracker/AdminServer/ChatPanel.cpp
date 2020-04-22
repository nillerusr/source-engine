//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "ChatPanel.h"
#include "RemoteServer.h"

#include <vgui/IVGui.h>
#include <KeyValues.h>

#include <vgui_controls/TextEntry.h>
#include <vgui_controls/RichText.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/PHandle.h>

#include <stdio.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CChatPanel::CChatPanel(vgui::Panel *parent, const char *name) : PropertyPage(parent, name)
{
	m_pServerChatPanel = new RichText(this, "ServerChatText");
	m_pServerChatPanel->SetMaximumCharCount(8000);

	m_pEnterChatPanel = new TextEntry(this,"ChatMessage");

	m_pSendChatButton = new Button(this, "SendChat", "#Chat_Panel_Send");
	m_pSendChatButton->SetCommand(new KeyValues("SendChat"));
	m_pSendChatButton->SetAsDefaultButton(true);

	LoadControlSettings("Admin/ChatPanel.res", "PLATFORM");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CChatPanel::~CChatPanel()
{
}

//-----------------------------------------------------------------------------
// Purpose: Activates the page
//-----------------------------------------------------------------------------
void CChatPanel::OnPageShow()
{
	BaseClass::OnPageShow();
}

//-----------------------------------------------------------------------------
// Purpose: Hides the page
//-----------------------------------------------------------------------------
void CChatPanel::OnPageHide()
{
	BaseClass::OnPageHide();
}

//-----------------------------------------------------------------------------
// Purpose: inserts a new string into the main chat panel
//-----------------------------------------------------------------------------
void CChatPanel::DoInsertString(const char *str) 
{
	m_pServerChatPanel->InsertString(str);
}

//-----------------------------------------------------------------------------
// Purpose: run when the send button is pressed, send a rcon "say" to the server
//-----------------------------------------------------------------------------
void CChatPanel::OnSendChat()
{
	// build a chat command and send it to the server
	char chat_text[512];
	strcpy(chat_text, "say ");
	m_pEnterChatPanel->GetText(chat_text + 4, sizeof(chat_text) - 4);
	if (strlen("say ") != strlen(chat_text))
	{
		RemoteServer().SendCommand(chat_text);

		// the message is sent, zero the text
		m_pEnterChatPanel->SetText("");
	}
}
