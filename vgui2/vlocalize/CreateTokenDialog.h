//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CREATETOKENDIALOG_H
#define CREATETOKENDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <VGUI_controls/Frame.h>

namespace vgui
{
class Button;
class TextEntry;
};

class CLocalizationDialog;
//-----------------------------------------------------------------------------
// Purpose: Creates a new token
//-----------------------------------------------------------------------------
class CCreateTokenDialog : public vgui::Frame
{
public:
	CCreateTokenDialog( CLocalizationDialog *pLocalizationDialog );
	~CCreateTokenDialog();

	// prompts user to create a single token
	virtual void CreateSingleToken();

	// loads a token file to prompt the user to create multiple tokens
	virtual void CreateMultipleTokens();

private:
	virtual void OnCommand(const char *command);
	virtual void OnClose();

	virtual void OnOK();
	virtual void OnSkip();

	vgui::Button *m_pSkipButton;
	vgui::TextEntry *m_pTokenName;
	vgui::TextEntry *m_pTokenValue;
	CLocalizationDialog *m_pLocalizationDialog;

	bool m_bMultiToken;

	typedef vgui::Frame BaseClass;
};


#endif // CREATETOKENDIALOG_H
