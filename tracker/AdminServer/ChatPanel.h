//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef CHATPANEL_H
#define CHATPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <KeyValues.h>

#include <vgui_controls/Frame.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/PropertyPage.h>

//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying information about a game server
//-----------------------------------------------------------------------------
class CChatPanel : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( CChatPanel, vgui::PropertyPage );
public:
	CChatPanel(vgui::Panel *parent, const char *name);
	~CChatPanel();

	// property page handlers
	virtual void OnPageShow();
	virtual void OnPageHide();
	void DoInsertString(const char *str);

private:
	MESSAGE_FUNC( OnSendChat, "SendChat" );

	vgui::RichText *m_pServerChatPanel;
	vgui::TextEntry *m_pEnterChatPanel;
	vgui::Button *m_pSendChatButton;
};

#endif // CHATPANEL_H