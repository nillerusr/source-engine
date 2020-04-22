//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "CDKeyEntryDialog.h"
#include "EngineInterface.h"

#include "steamcommon.h" // VALVE_AUTH cd key checking code
#include "ValidateNewValveCDKeyClient.h"
#include "ValveCDKeyGameAndTerritoryCodes.h"

#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/ISystem.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/TextEntry.h"
#include "vstdlib/random.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#include <ctype.h>

using namespace vgui;

#define FAKE_CDKEY_LEN		49
#define FAKE_CDKEY_REGKEY	"HKEY_CURRENT_USER\\Software\\Valve\\Source\\Settings\\EncryptedCDKey"

//-----------------------------------------------------------------------------
// Purpose: hacky class to make all input uppercase
//-----------------------------------------------------------------------------
class CUpperCaseTextEntry : public vgui::TextEntry
{
	DECLARE_CLASS_SIMPLE( CUpperCaseTextEntry, vgui::TextEntry );
public:
	CUpperCaseTextEntry(vgui::Panel *parent, const char *name) : TextEntry(parent, name) {}

	virtual void InsertChar(wchar_t unichar)
	{
		// only allow input of valid cdkey characters
		if (unichar >= 'a' && unichar <= 'z')
		{
			// force to be uppercase
			BaseClass::InsertChar(unichar - 'a' + 'A');
		}
		else if ((unichar >= 'A' && unichar <= 'Z')
			|| (unichar >= '0' && unichar <= '9'))
		{
			BaseClass::InsertChar(unichar);
		}
	}
};


class CloseMessageBox : public vgui::MessageBox
{
public:
	CloseMessageBox(const char *title, const char *text, Panel *parent = NULL): MessageBox( title, text, parent) {}

	virtual void OnClose()
	{
			engine->ClientCmd_Unrestricted("quit\n");
	}
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCDKeyEntryDialog::CCDKeyEntryDialog(vgui::Panel *parent, bool inConnect) : BaseClass(parent, "CDKeyEntryDialog")
{
	// see what type of cdkey we have
	SetDeleteSelfOnClose(true);
	m_bInConnect = inConnect;
	m_iErrCount = 0;
	m_bEnteredValidCDKey = false;

	m_pOK = new Button(this, "OKButton", "#GameUI_OK");
	m_pQuitGame = new Button(this, "CancelButton", "#GameUI_Quit");
	m_pEntry1 = new CUpperCaseTextEntry(this, "Entry1");;
	m_pEntry2 = new CUpperCaseTextEntry(this, "Entry2");
	m_pEntry3 = new CUpperCaseTextEntry(this, "Entry3");
	m_pEntry4 = new CUpperCaseTextEntry(this, "Entry4");
	m_pEntry5 = new CUpperCaseTextEntry(this, "Entry5");

	m_pEntry1->SetMaximumCharCount(5);
	m_pEntry2->SetMaximumCharCount(5);
	m_pEntry3->SetMaximumCharCount(5);
	m_pEntry4->SetMaximumCharCount(5);
	m_pEntry5->SetMaximumCharCount(5);

	m_pEntry1->SetAutoProgressOnHittingCharLimit(true);
	m_pEntry2->SetAutoProgressOnHittingCharLimit(true);
	m_pEntry3->SetAutoProgressOnHittingCharLimit(true);
	m_pEntry4->SetAutoProgressOnHittingCharLimit(true);
	m_pEntry5->SetAutoProgressOnHittingCharLimit(false);
	
    SetSizeable(false);
	SetSize(360, 224);
	SetTitle("#GameUI_CDKey", true);

	LoadControlSettings("Resource/ValveCDKeyEntryDialog.res");
//	MoveToCenterOfScreen();

	SetMinimizeButtonVisible(false);
	SetMaximizeButtonVisible(false);

	m_pOK->SetEnabled(false);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCDKeyEntryDialog::~CCDKeyEntryDialog()
{
	vgui::surface()->RestrictPaintToSinglePanel(NULL);
}

//-----------------------------------------------------------------------------
// Purpose: returns true if there is a valid weak check key in the registry
//-----------------------------------------------------------------------------
bool CCDKeyEntryDialog::IsValidWeakCDKeyInRegistry()
{
	char fakekey[FAKE_CDKEY_LEN];
	if (vgui::system()->GetRegistryString(FAKE_CDKEY_REGKEY, fakekey, sizeof(fakekey)))
	{
		if (strlen(fakekey) == (FAKE_CDKEY_LEN - 1)
			&& fakekey[17] == 'E'
			&& fakekey[31] == 'z'
			&& fakekey[43] == 'n')
		{
			return true;
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCDKeyEntryDialog::OnCommand(const char *command)
{
	if (!stricmp(command, "OK"))
	{
		if (IsEnteredKeyValid())
		{
			m_bEnteredValidCDKey = true;

			// write out fake key and continue
			char fakekey[FAKE_CDKEY_LEN];
			for (int i = 0; i < sizeof(fakekey) - 1; i++)
			{
				// pick some random fields for us to check later
				if (i == 17)
				{
					fakekey[i] = 'E';
				}
				else if (i == 31)
				{
					fakekey[i] = 'z';
				}
				else if (i == 43)
				{
					fakekey[i] = 'n';
				}
				else
				{
					fakekey[i] = RandomInt('0', 'z');
				}
			}
			fakekey[sizeof(fakekey) - 1] = 0;
			vgui::system()->SetRegistryString(FAKE_CDKEY_REGKEY, fakekey);

			if ( m_bInConnect )
			{
				engine->ClientCmd_Unrestricted( "retry\n" ); // retry the server connection with this new key...
			}
			Close();
		}
		else
		{
			m_hErrorBox = new MessageBox("#GameUI_CDKey_Invalid_Title","#GameUI_CDKey_Invalid_Text", this);
			m_hErrorBox->ShowWindow( this );
		}
	}
	else if (!stricmp(command, "Cancel") || !stricmp(command, "Close"))
	{
		Close();
	}
	else
	{
		BaseClass::OnCommand(command);
	}

	if (!m_bEnteredValidCDKey) // moved away from the dialog box to make it a little harder to crack...
	{
		m_iErrCount++;
		if( m_iErrCount >= MAX_CDKEY_ERRORS )
		{
			// too many bad entries, make em quit
			CloseMessageBox *bx = new CloseMessageBox("#GameUI_CDKey_Invalid_Title","#GameUI_CDKey_TooManyTries", this);
			bx->ShowWindow( this );
		}
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCDKeyEntryDialog::OnClose()
{
	if (!m_bEnteredValidCDKey)
	{
		// if we don't have a valid key we can't continue
		engine->ClientCmd_Unrestricted("quit\n");
	}

	BaseClass::OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCDKeyEntryDialog::OnThink()
{
	if (!m_bEnteredValidCDKey)
	{
		// force us to be the only thing to draw
		VPANEL vpanel = m_hErrorBox.Get() ? m_hErrorBox->GetVPanel() : GetVPanel();
		vgui::surface()->RestrictPaintToSinglePanel(vpanel);

		// make sure we're the focus
		if (!(input()->GetFocus() && ipanel()->HasParent(input()->GetFocus(), GetVPanel())))
		{
			BaseClass::Activate();
		}
	}
	BaseClass::OnThink();
}

//-----------------------------------------------------------------------------
// Purpose: returns whether or not the key entered by the user passes the simple check
//-----------------------------------------------------------------------------
bool CCDKeyEntryDialog::IsEnteredKeyValid()
{
	// get the entered text
	char cdkey[32];
	m_pEntry1->GetText(cdkey, 6);
	m_pEntry2->GetText(cdkey + 6, 6);
	m_pEntry3->GetText(cdkey + 12, 6);
	m_pEntry4->GetText(cdkey + 18, 6);
	m_pEntry5->GetText(cdkey + 24, 6);
	
	// add in the hyphens
	cdkey[5] = '-';
	cdkey[11] = '-';
	cdkey[17] = '-';
	cdkey[23] = '-';

	// verify the entry
	for (int i = 0; i < 29; i++)
	{
		if (cdkey[i] != '-' && !isalnum(cdkey[i]))
			return false;
	}
#if !defined( NO_STEAM )
	uint gameCode, salesTerritoryCode, uniqueSerialNumber;

	// test key - this key passes they weak check, but not the strong check
	// "5J5Q3-QCME2-2SMMV-SBHN9-43S2S"
	if ( SteamWeakVerifyNewValveCDKey(cdkey, &gameCode, &salesTerritoryCode, &uniqueSerialNumber) == eSteamErrorNone )
	{
		// require hl2 retail cdkey (ATI Bundle keys are also in this category)
		if (gameCode == eHalfLife2)
			return true;
	}
#endif
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: handles user entering data in fields
//-----------------------------------------------------------------------------
void CCDKeyEntryDialog::OnTextChanged(Panel *entry)
{
	char cdkey[32];
	m_pEntry1->GetText(cdkey, 6);
	m_pEntry2->GetText(cdkey + 6, 6);
	m_pEntry3->GetText(cdkey + 12, 6);
	m_pEntry4->GetText(cdkey + 18, 6);
	m_pEntry5->GetText(cdkey + 24, 6);
	
	// add in the hyphens
	cdkey[5] = '-';
	cdkey[11] = '-';
	cdkey[17] = '-';
	cdkey[23] = '-';

	if (strlen(cdkey) == 29)
	{
		m_pOK->SetEnabled(true);
	}
	else
	{
		m_pOK->SetEnabled(false);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCDKeyEntryDialog::Activate()
{
    BaseClass::Activate();
    m_pEntry1->RequestFocus();
}
