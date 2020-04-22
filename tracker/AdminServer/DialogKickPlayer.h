//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef DIALOGKICKPLAYER_H
#define DIALOGKICKPLAYER_H
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
class CDialogKickPlayer : public vgui::Frame
{
public:
	CDialogKickPlayer();
	~CDialogKickPlayer();

	// initializes the dialog and brings it to the foreground
	void Activate(const char *playerName, const char *question,const char *type);

	/* message returned:
		"KickPlayer" 
			"player"
			"type"
	*/

private:
	virtual void PerformLayout();
	virtual void OnCommand(const char *command);
	virtual void OnClose();

	vgui::Label *m_pInfoLabel;
	vgui::Label *m_pPlayerLabel;
	vgui::Button *m_pOkayButton;

	typedef vgui::Frame BaseClass;
	const char *m_cType;

};


#endif // DIALOGKICKPLAYER_H
