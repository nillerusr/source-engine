//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>

#include "MOTDPanel.h"


#include <VGUI_Controls.h>
#include <VGUI_ISystem.h>
#include <VGUI_ISurface.h>
#include <VGUI_IVGui.h>
#include <VGUI_KeyValues.h>
#include <VGUI_Label.h>
#include <VGUI_TextEntry.h>
#include <VGUI_Button.h>
#include <VGUI_ToggleButton.h>
#include <VGUI_RadioButton.h>
#include <VGUI_ListPanel.h>
#include <VGUI_ComboBox.h>
#include <VGUI_PHandle.h>
#include <VGUI_PropertySheet.h>

using namespace vgui;
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMOTDPanel::CMOTDPanel(vgui::Panel *parent, const char *name) : PropertyPage(parent, name)
{
	m_pRcon=NULL;

	m_pMOTDPanel = new TextEntry(this, "ServerMOTDText");

	m_pMOTDPanel->SetMultiline(true);
	m_pMOTDPanel->SetEnabled(true);
	m_pMOTDPanel->SetEditable(true);
	m_pMOTDPanel->SetVerticalScrollbar(true);
	m_pMOTDPanel->SetRichEdit(false);
	m_pMOTDPanel->SetCatchEnterKey(true);
	m_pMOTDPanel->setMaximumCharCount(1024);
	
	m_pSendMOTDButton = new Button(this, "SendMOTD", "&Send");
	m_pSendMOTDButton->SetCommand(new KeyValues("SendMOTD"));
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMOTDPanel::~CMOTDPanel()
{

}

//-----------------------------------------------------------------------------
// Purpose: Activates the page
//-----------------------------------------------------------------------------
void CMOTDPanel::OnPageShow()
{
	m_pMOTDPanel->RequestFocus();
}

//-----------------------------------------------------------------------------
// Purpose: Hides the page
//-----------------------------------------------------------------------------
void CMOTDPanel::OnPageHide()
{
}

//-----------------------------------------------------------------------------
// Purpose: Relayouts the data
//-----------------------------------------------------------------------------
void CMOTDPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	// setup the layout of the panels
	m_pMOTDPanel->SetBounds(5,5,GetWide()-10,GetTall()-35);

	m_pSendMOTDButton->SetBounds(GetWide()-70,GetTall()-25,60,20);
}

//-----------------------------------------------------------------------------
// Purpose: inserts a new string into the main chat panel
//-----------------------------------------------------------------------------
void CMOTDPanel::DoInsertString(const char *str) 
{
	m_pMOTDPanel->SetText("");
	if(strlen(str)>1024)
	{
		char *fix = const_cast<char *>(str);
		fix[1024]='\0';
	}
	m_pMOTDPanel->DoInsertString(str);
}

//-----------------------------------------------------------------------------
// Purpose: passes the rcon class to use
//-----------------------------------------------------------------------------
void CMOTDPanel::SetRcon(CRcon *rcon) 
{
	m_pRcon=rcon;
}


//-----------------------------------------------------------------------------
// Purpose: run when the send button is pressed, send a rcon "say" to the server
//-----------------------------------------------------------------------------
void CMOTDPanel::OnSendMOTD()
{
	if(m_pRcon)
	{
		char chat_text[2048];

		_snprintf(chat_text,512,"motd_write ");
		m_pMOTDPanel->GetText(0,chat_text+11,2048-11);
		if(strlen("motd_write ")!=strlen(chat_text)) // check there is something in the text panel
		{
			unsigned int i=0;
			while(i<strlen(chat_text) && i<2048)
			{
				if(chat_text[i]=='\n')
				{
					// shift everything up one
					for(unsigned int k=strlen(chat_text)+1;k>i;k--)
					{
						chat_text[k+1]=chat_text[k];
					}
					
					// replace the newline with the string "\n"
					chat_text[i]='\\'; 
					chat_text[i+1]='n';

					i++; // skip this insert
				}
				i++;
			}

			m_pRcon->SendRcon(chat_text);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when the game dir combo box is changed
//-----------------------------------------------------------------------------
void CMOTDPanel::OnTextChanged(Panel *panel, const char *text)
{
// BUG - TextEntry NEVER lets the enter key through... This doesn't work

	if( text[strlen(text)-1]=='\n') // the enter key was just pressed :)
	{
		OnSendMOTD();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Message map
//-----------------------------------------------------------------------------
MessageMapItem_t CMOTDPanel::m_MessageMap[] =
{
	MAP_MESSAGE( CMOTDPanel, "SendMOTD", OnSendMOTD ),
	MAP_MESSAGE( CMOTDPanel, "PageShow", OnPageShow ),
//	MAP_MESSAGE_PTR_CONSTCHARPTR( CMOTDPanel, "TextChanged", OnTextChanged, "panel", "text" ),
};

IMPLEMENT_PANELMAP( CMOTDPanel, Frame );