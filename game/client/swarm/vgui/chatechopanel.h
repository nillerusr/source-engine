#ifndef _INCLUDED_CHATECHOPANEL_H
#define _INCLUDED_CHATECHOPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include "hud_basechat.h"

namespace vgui
{
	class RichText;
};

class CASWChatHistoryEcho : public CHudChatHistory
{
	DECLARE_CLASS_SIMPLE( CASWChatHistoryEcho, CHudChatHistory );
public:
	CASWChatHistoryEcho( vgui::Panel *pParent, const char *panelName );

	virtual void	ApplySchemeSettings(vgui::IScheme *pScheme);
};

// this panel echoes the text on the HUD chat panel

#define ASW_CHAT_ECHO_HISTORY_WCHARS 2048

class ChatEchoPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( ChatEchoPanel, vgui::Panel );
public:
	ChatEchoPanel(vgui::Panel *parent, const char *name);
	virtual ~ChatEchoPanel();

	virtual void OnThink();							// called every frame before painting, but only if panel is visible
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();
	bool AddCursorToBuffer(wchar_t *buffer, int &iCursorPos, bool bFlash);

	vgui::RichText *m_pChatInputLine;
	CASWChatHistoryEcho* m_pChatHistory;

	vgui::HFont m_hFont;

	wchar_t m_InputBuffer[ASW_CHAT_ECHO_HISTORY_WCHARS];	// max chars that can be held in the chat history
	
};

#endif