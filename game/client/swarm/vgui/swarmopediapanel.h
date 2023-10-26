#ifndef _INCLUDED_SWARMOPEDIA_PANEL_H
#define _INCLUDED_SWARMOPEDIA_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>

namespace vgui
{
	class Label;
	class TextEntry;
	class ComboBox;
	class CheckButton;
	class Button;
	class PanelListPanel;
};
class SwarmopediaTopicButton;

// shows a list of buttons down the left side of the screen, each one corresponding to a HTML page to display on the right

class SwarmopediaPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( SwarmopediaPanel, vgui::Panel );
public:
	SwarmopediaPanel( vgui::Panel *pParent, const char *pElementName);
	virtual ~SwarmopediaPanel();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnCommand(const char* command);
	virtual void OnThink();

	bool ShowDoc(const char *szDoc);
	bool ShowList(const char *szListName);

	void AddListEntry(const char* szName, const char* szArticle, const char* szListTarget, int iSectionHeader);
	void ListEntryClicked(SwarmopediaTopicButton *pTopic);

private:
	vgui::Label *m_pTopicLabel;
	vgui::PanelListPanel *m_pList;
	vgui::HTML *m_pHTML;
	bool m_bCreatedPanels;
};

class SwarmopediaTopicButton : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( SwarmopediaTopicButton, vgui::Panel );
public:
	SwarmopediaTopicButton( vgui::Panel *pParent, const char *pElementName, SwarmopediaPanel *pSwarmopedia);
	virtual ~SwarmopediaTopicButton();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnMouseReleased(vgui::MouseCode code);
	virtual void OnCursorEntered();
	virtual void OnCursorExited();

	char m_szArticleTarget[128];
	char m_szListTarget[128];

	vgui::Label* m_pLabel;
	vgui::HFont m_hFont;
	SwarmopediaPanel* m_pSwarmopedia;
	int m_iSectionHeader;
};

#endif _INCLUDED_SWARMOPEDIA_PANEL_H