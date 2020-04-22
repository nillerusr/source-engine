//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef DIALOGADDBAN_H
#define DIALOGADDBAN_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/RadioButton.h>

//-----------------------------------------------------------------------------
// Purpose: Prompt for user to enter a password to be able to connect to the server
//-----------------------------------------------------------------------------
class CDialogAddBan : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CDialogAddBan, vgui::Frame ); 
public:
	CDialogAddBan(vgui::Panel *parent);
	~CDialogAddBan();

	// initializes the dialog and brings it to the foreground
	void Activate(const char *type,const char *player, const char *authid);

	/* message returned:
		"AddBanValue" 
			"id"
			"time"
			"type"
			"ipcheck"
	*/
	
	// make the input stars, ala a password entry dialog
	void MakePassword();
	
	// set the text in a certain label name
	void SetLabelText(const char *textEntryName, const char *text);
	void SetTextEntry(const char *textEntryName, const char *text);

	// returns if the IPCheck check button is checked
	bool IsIPCheck();

private:
	virtual void PerformLayout();
	virtual void OnCommand(const char *command);
	virtual void OnClose();
	MESSAGE_FUNC_PTR( OnButtonToggled, "RadioButtonChecked", panel );

	vgui::TextEntry *m_pTimeTextEntry;
	vgui::TextEntry *m_pIDTextEntry;
	vgui::Button *m_pOkayButton;
	vgui::ComboBox *m_pTimeCombo;
	vgui::RadioButton *m_pPermBanRadio;
	vgui::RadioButton *m_pTempBanRadio;

	const char *m_cType;
};


#endif // DIALOGADDBAN_H
