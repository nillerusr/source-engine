//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef DIALOGCVARCHANGE_H
#define DIALOGCVARCHANGE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

//-----------------------------------------------------------------------------
// Purpose: Prompt for user to enter a password to be able to connect to the server
//-----------------------------------------------------------------------------
class CDialogCvarChange : public vgui::Frame
{
public:
	CDialogCvarChange(vgui::Panel *parent);
	~CDialogCvarChange();

	// initializes the dialog and brings it to the foreground
	void Activate(const char *cvarName, const char *curValue, const char *type, const char *question);

	/* message returned:
		"CvarChangeValue" 
			"player"
			"value"
			"type"
	*/
	
	// make the input stars, ala a password entry dialog
	void MakePassword();
	
	// set the text in a certain label name
	void SetLabelText(const char *textEntryName, const char *text);

private:
	virtual void OnCommand(const char *command);
	virtual void OnClose();

	vgui::Label *m_pInfoLabel;
	vgui::Label *m_pCvarLabel;
	vgui::TextEntry *m_pCvarEntry;
	vgui::Button *m_pOkayButton;

	bool m_bAddCvarText; // if true puts the cvar name into the dialog

	typedef vgui::Frame BaseClass;
	const char *m_cType;

};


#endif // DIALOGCVARCHANGE_H
