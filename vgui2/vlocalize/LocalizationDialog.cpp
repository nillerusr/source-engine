//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "LocalizationDialog.h"
#include "CreateTokenDialog.h"

#include "vgui_controls/Button.h"
#include "vgui_controls/ListPanel.h"
#include"vgui_controls/TextEntry.h"
#include "VGUI/IVGui.h"
#include "VGUI/ILocalize.h"
#include "VGUI/ISurface.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/MenuButton.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/FileOpenDialog.h"

#include <stdio.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLocalizationDialog::CLocalizationDialog(const char *fileName) : Frame(NULL, "LocalizationDialog")
{
	m_iCurrentToken = -1;

	m_pTokenList = new ListPanel(this, "TokenList");

	m_pTokenList->AddColumnHeader(0, "Token", "Token Name", 128, 128, 1024, 0 );
	
	m_pLanguageEdit = new TextEntry(this, "LanguageEdit");
	m_pLanguageEdit->SetMultiline(true);
	m_pLanguageEdit->SetVerticalScrollbar(true);
	m_pLanguageEdit->SetCatchEnterKey(true);
	m_pEnglishEdit = new TextEntry(this, "EnglishEdit");
	m_pEnglishEdit->SetMultiline(true);
	m_pEnglishEdit->SetVerticalScrollbar(true);
	m_pEnglishEdit->SetVerticalScrollbar(true);

	m_pFileMenu = new Menu(this, "FileMenu");

	m_pFileMenu->AddMenuItem(" &Open File ", new KeyValues("FileOpen"), this);
	m_pFileMenu->AddMenuItem(" &Save File ", new KeyValues("FileSave"), this);
	m_pFileMenu->AddMenuItem(" E&xit Localizer ", new KeyValues("Close"), this);
	m_pFileMenuButton = new MenuButton(this, "FileMenuButton", "File");
	m_pFileMenuButton->SetMenu(m_pFileMenu);
	m_pApplyButton = new Button(this, "ApplyButton", "Apply");
	m_pApplyButton->SetCommand(new KeyValues("ApplyChanges"));
	m_pTestLabel = new Label(this, "TestLabel", "");

	LoadControlSettings("Resource/LocalizationDialog.res");

	strcpy(m_szFileName, fileName);

	char buf[512];
	Q_snprintf(buf, sizeof( buf ), "%s - Localization Editor", m_szFileName);
	SetTitle(buf, true);

	// load in the string table
	if (!g_pVGuiLocalize->AddFile( m_szFileName ) )
	{
		MessageBox *msg = new MessageBox("Fatal error", "couldn't load specified file");
		msg->SetCommand("Close");
		msg->AddActionSignalTarget(this);
		msg->DoModal();
		return;	
	}

	// populate the dialog with the strings
	StringIndex_t idx = g_pVGuiLocalize->GetFirstStringIndex();
	while ( idx != INVALID_LOCALIZE_STRING_INDEX )
	{
		// adds the strings into the table, along with the indexes
		m_pTokenList->AddItem(new KeyValues("LString", "Token", g_pVGuiLocalize->GetNameByIndex(idx)), idx, false, false);

		// move to the next string
		idx = g_pVGuiLocalize->GetNextStringIndex(idx);
	}

	// sort the table
	m_pTokenList->SetSortColumn(0);
	m_pTokenList->SortList();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLocalizationDialog::~CLocalizationDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Handles closing of the dialog - shuts down the whole app
//-----------------------------------------------------------------------------
void CLocalizationDialog::OnClose()
{
	BaseClass::OnClose();

	// Stop vgui running
	vgui::ivgui()->Stop();
}

//-----------------------------------------------------------------------------
// Purpose: lays out the dialog
//-----------------------------------------------------------------------------
void CLocalizationDialog::PerformLayout()
{
	OnTextChanged();

	BaseClass::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the currently selected token
//-----------------------------------------------------------------------------
void CLocalizationDialog::OnTokenSelected()
{
	if (m_pTokenList->GetSelectedItemsCount() != 1)
	{
		// clear the list
		m_pLanguageEdit->SetText("");
		m_pEnglishEdit->SetText("");
		
		//!! unicode test label
		m_pTestLabel->SetText("");

		m_iCurrentToken = -1;
	}
	else
	{
		// get the data
		int itemId = m_pTokenList->GetSelectedItem(0);
		vgui::ListPanelItem *data = m_pTokenList->GetItemData( itemId );
		Assert( data );
		m_iCurrentToken = data->userData;
		wchar_t *unicodeString = g_pVGuiLocalize->GetValueByIndex(m_iCurrentToken);

		char value[2048];
		g_pVGuiLocalize->ConvertUnicodeToANSI(unicodeString, value, sizeof(value));

		//!! unicode test label
		m_pTestLabel->SetText(unicodeString);

		// set the text
		m_pLanguageEdit->SetText(value);
		m_pEnglishEdit->SetText(value);
	}

	m_pApplyButton->SetEnabled(false);
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if any text has changed
//-----------------------------------------------------------------------------
void CLocalizationDialog::OnTextChanged()
{
	static char buf1[1024], buf2[1024];

	m_pLanguageEdit->GetText( buf1, sizeof( buf1 ) );
	m_pEnglishEdit->GetText( buf2, sizeof( buf2 ) );

	if (!strcmp(buf1, buf2))
	{
		m_pApplyButton->SetEnabled(false);
	}
	else
	{
		m_pApplyButton->SetEnabled(true);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Copies any changes made into the main database
//-----------------------------------------------------------------------------
void CLocalizationDialog::OnApplyChanges()
{
	if (m_iCurrentToken < 0)
		return;

	static char buf1[1024];
	static wchar_t unicodeString[1024];
	m_pLanguageEdit->GetText( buf1, sizeof( buf1 ) );
	g_pVGuiLocalize->ConvertANSIToUnicode(buf1, unicodeString, sizeof(unicodeString) / sizeof(wchar_t));

	//!! unicode test label
	m_pTestLabel->SetText(unicodeString);

	// apply the text change to the database
	g_pVGuiLocalize->SetValueByIndex(m_iCurrentToken, unicodeString);

	// disable the apply button
	m_pApplyButton->SetEnabled(false);

	// reselect the token
	OnTokenSelected();
}

//-----------------------------------------------------------------------------
// Purpose: Message handler for saving current file
//-----------------------------------------------------------------------------
void CLocalizationDialog::OnFileSave()
{
	if (g_pVGuiLocalize->SaveToFile( m_szFileName ) )
	{
		// success
		MessageBox *box = new MessageBox("Save Successful - VLocalize", "File was successfully saved.", false);
		box->DoModal();
	}
	else
	{
		// failure
		MessageBox *box = new MessageBox("Error during save - VLocalize", "Error - File was not successfully saved.", false);
		box->DoModal();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Message handler for loading a file
//-----------------------------------------------------------------------------
void CLocalizationDialog::OnFileOpen()
{
	FileOpenDialog *box = new FileOpenDialog( this, "Open", true );

	box->SetStartDirectory("u:\\");
	box->AddFilter("*.*", "All Files (*.*)", true );
	box->DoModal(false);
}

//-----------------------------------------------------------------------------
// Purpose: Handles a token created message
// Input  : *tokenName - the name of the newly created token
//-----------------------------------------------------------------------------
void CLocalizationDialog::OnTokenCreated(const char *tokenName)
{
	// add the new string table token to the token list
	int idx = g_pVGuiLocalize->FindIndex(tokenName);
	int itemId = m_pTokenList->AddItem(new KeyValues("LString", "Token", g_pVGuiLocalize->GetNameByIndex(idx)), idx, true, true );

	// make that currently selected
	m_pTokenList->SetSingleSelectedItem( itemId );
	OnTokenSelected();
}

//-----------------------------------------------------------------------------
// Purpose: Creates a new token
//-----------------------------------------------------------------------------
void CLocalizationDialog::OnCreateToken()
{
	CCreateTokenDialog *dlg = new CCreateTokenDialog( this );
	dlg->AddActionSignalTarget(this);
	dlg->CreateSingleToken();
}

char const *CLocalizationDialog::GetFileName() const
{
	return m_szFileName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CLocalizationDialog::OnCommand(const char *command)
{
	if (!stricmp(command, "CreateToken"))
	{
		OnCreateToken();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}


//-----------------------------------------------------------------------------
// Purpose: empty message map
//-----------------------------------------------------------------------------
MessageMapItem_t CLocalizationDialog::m_MessageMap[] =
{
	MAP_MESSAGE( CLocalizationDialog, "RowSelected", OnTokenSelected ),	// message from the m_pTokenList
	MAP_MESSAGE( CLocalizationDialog, "TextChanged", OnTextChanged ),	// message from the text entry
	MAP_MESSAGE( CLocalizationDialog, "ApplyChanges", OnApplyChanges ),	// message from the text entry
	MAP_MESSAGE( CLocalizationDialog, "FileSave", OnFileSave ),
	MAP_MESSAGE( CLocalizationDialog, "FileOpen", OnFileOpen ),
	MAP_MESSAGE_CONSTCHARPTR( CLocalizationDialog, "TokenCreated", OnTokenCreated, "name" ),
};
IMPLEMENT_PANELMAP(CLocalizationDialog, BaseClass);
