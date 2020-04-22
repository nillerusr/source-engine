//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef MOTDPANEL_H
#define MOTDPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <VGUI_Frame.h>
#include <VGUI_PHandle.h>
#include <VGUI_ListPanel.h>
#include <VGUI_KeyValues.h>
#include <VGUI_PropertyPage.h>

#include "rcon.h"

class KeyValues;

namespace vgui
{
class Button;
class ToggleButton;
class RadioButton;
class Label;
class TextEntry;
class ListPanel;
class MessageBox;
class ComboBox;
class Panel;
class PropertySheet;
};


//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying information about a game server
//-----------------------------------------------------------------------------
class CMOTDPanel:public vgui::PropertyPage
{
public:
	CMOTDPanel(vgui::Panel *parent, const char *name);
	~CMOTDPanel();

	// property page handlers
	virtual void OnPageShow();
	virtual void OnPageHide();
	void DoInsertString(const char *str);

	void SetRcon(CRcon *rcon);

	virtual void PerformLayout();

private:

	void OnSendMOTD();
	void OnTextChanged(vgui::Panel *panel, const char *text);

	vgui::TextEntry *m_pMOTDPanel;
	vgui::Button *m_pSendMOTDButton;

	CRcon *m_pRcon;
		
	DECLARE_PANELMAP();
	typedef vgui::PropertyPage BaseClass;
};

#endif // MOTDPANEL_H