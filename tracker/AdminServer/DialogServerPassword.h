//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef DIALOGSERVERPASSWORD_H
#define DIALOGSERVERPASSWORD_H
#ifdef _WIN32
#pragma once
#endif

#include <VGUI_Frame.h>


namespace vgui
{
class TextEntry;
class Label;
class Button;
};

//-----------------------------------------------------------------------------
// Purpose: Prompt for user to enter a password to be able to connect to the server
//-----------------------------------------------------------------------------
class CDialogServerPassword : public vgui::Frame
{
public:
	CDialogServerPassword();
	~CDialogServerPassword();

	// initializes the dialog and brings it to the foreground
	void Activate(const char *serverName, unsigned int serverID);

	/* message returned:
		"JoinServerWithPassword" 
			"serverID"
			"password"
	*/

private:
	virtual void PerformLayout();
	virtual void OnCommand(const char *command);
	virtual void OnClose();

	vgui::Label *m_pInfoLabel;
	vgui::Label *m_pGameLabel;
	vgui::TextEntry *m_pPasswordEntry;
	vgui::Button *m_pConnectButton;

	typedef vgui::Frame BaseClass;

	int m_iServerID;
};


#endif // DIALOGSERVERPASSWORD_H
