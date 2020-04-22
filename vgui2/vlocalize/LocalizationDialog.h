//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef LOCALIZATIONDIALOG_H
#define LOCALIZATIONDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <VGUI_controls/Frame.h>

namespace vgui
{
class Button;
class ComboBox;
class Label;
class TextEntry;
class ListPanel;
class MenuButton;
};

//-----------------------------------------------------------------------------
// Purpose: Main localization dialog class
//-----------------------------------------------------------------------------
class CLocalizationDialog : public vgui::Frame
{
public:
	CLocalizationDialog(const char *fileName);
	~CLocalizationDialog();

	char const *GetFileName() const;

private:
	// vgui overrides
	virtual void PerformLayout();
	virtual void OnClose();
	virtual void OnCommand(const char *command);

	// message handlers
	virtual void OnTokenSelected();
	virtual void OnTextChanged();
	virtual void OnApplyChanges();
	virtual void OnFileOpen();
	virtual void OnFileSave();
	virtual void OnCreateToken();
	virtual void OnTokenCreated(const char *tokenName);
	
	vgui::ListPanel *m_pTokenList;
	vgui::TextEntry *m_pLanguageEdit;
	vgui::TextEntry *m_pEnglishEdit;
	vgui::MenuButton *m_pFileMenuButton;
	vgui::Menu *m_pFileMenu;
	vgui::Button *m_pApplyButton;
	vgui::Label *m_pTestLabel;

	int m_iCurrentToken;

	char m_szFileName[512];

	DECLARE_PANELMAP();

	typedef vgui::Frame BaseClass;
};


#endif // LOCALIZATIONDIALOG_H
