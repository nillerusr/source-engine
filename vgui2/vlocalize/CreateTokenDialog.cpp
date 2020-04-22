//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "CreateTokenDialog.h"
#include "LocalizationDialog.h"

#include "vgui_controls/TextEntry.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/MessageBox.h"
#include "tier1/KeyValues.h"
#include "vgui/ILocalize.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Cosntructor
//-----------------------------------------------------------------------------
CCreateTokenDialog::CCreateTokenDialog( CLocalizationDialog *pLocalizationDialog ) : Frame(NULL, "CreateTokenDialog"),
	m_pLocalizationDialog( m_pLocalizationDialog )
{
	Assert( m_pLocalizationDialog );

	MakePopup();

	SetTitle("Create New Token - Localizer", true);

	m_pSkipButton = new Button(this, "SkipButton", "&Skip Token");
	m_pTokenName = new TextEntry(this, "TokenName");

	m_pTokenValue = new TextEntry(this, "TokenValue");
	m_pTokenValue->SetMultiline(true);
	m_pTokenValue->SetCatchEnterKey(true);
	
	m_pSkipButton->SetCommand("SkipToken");
	m_pSkipButton->SetVisible(false);

	LoadControlSettings("Resource/CreateTokenDialog.res");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCreateTokenDialog::~CCreateTokenDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: prompts user to create a single token
//-----------------------------------------------------------------------------
void CCreateTokenDialog::CreateSingleToken()
{
	// bring us to the front
	SetVisible(true);
	RequestFocus();
	MoveToFront();
	m_pTokenName->RequestFocus();
	m_pSkipButton->SetVisible(false);

	m_bMultiToken = false;
}

//-----------------------------------------------------------------------------
// Purpose: loads a file to create multiple tokens
//-----------------------------------------------------------------------------
void CCreateTokenDialog::CreateMultipleTokens()
{
	SetVisible(true);
	RequestFocus();
	MoveToFront();
	m_pTokenValue->RequestFocus();
	m_pSkipButton->SetVisible(true);

	m_bMultiToken = true;

	//!! read tokens from file, prompt user to each in turn
}

//-----------------------------------------------------------------------------
// Purpose: Handles an OK message, creating the current token
//-----------------------------------------------------------------------------
void CCreateTokenDialog::OnOK()
{
	// get the data
	char tokenName[1024], tokenValue[1024];
	m_pTokenName->GetText( tokenName, sizeof( tokenName ) );
	m_pTokenValue->GetText( tokenValue, sizeof( tokenValue ) );

	if ( Q_strlen( tokenName ) < 4 )
	{
		MessageBox *box = new MessageBox("Create Token Error", "Could not create token.\nToken names need to be at least 4 characters long.");
		box->DoModal();
	}
	else
	{
		// create the token
		wchar_t unicodeString[1024];
		g_pVGuiLocalize->ConvertANSIToUnicode(tokenValue, unicodeString, sizeof(unicodeString) / sizeof(wchar_t));
		g_pVGuiLocalize->AddString(tokenName, unicodeString, m_pLocalizationDialog->GetFileName() );

		// notify the dialog creator
		PostActionSignal(new KeyValues("TokenCreated", "name", tokenName));

		// close
		if (!m_bMultiToken)
		{
			PostMessage(this, new KeyValues("Close"));
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: skips the current token in the multitoken edit mode
//-----------------------------------------------------------------------------
void CCreateTokenDialog::OnSkip()
{
}

//-----------------------------------------------------------------------------
// Purpose: handles a button command
// Input  : *command - 
//-----------------------------------------------------------------------------
void CCreateTokenDialog::OnCommand(const char *command)
{
	if (!stricmp(command, "OK"))
	{
		OnOK();
	}
	else if (!stricmp(command, "SkipToken"))
	{
		OnSkip();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

//-----------------------------------------------------------------------------
// Purpose: handles the close message
//-----------------------------------------------------------------------------
void CCreateTokenDialog::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}


