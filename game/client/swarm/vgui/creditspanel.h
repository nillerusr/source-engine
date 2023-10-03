#ifndef _INCLUDED_MEDAL_PANEL_H
#define _INCLUDED_MEDAL_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

namespace vgui
{
	class ImagePanel;
	class Label;
	class ImagePanel;
};

#define ASW_MESSAGE_INTERVAL 0.8f
#define ASW_LABEL_POOL_SIZE 16

// shows the ASI logo and scrolls ASI credits up the screen
class CreditsPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CreditsPanel, vgui::Panel );
public:
	CreditsPanel(vgui::Panel *parent, const char *name);
	virtual ~CreditsPanel();
	
	virtual void OnThink();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);	

	vgui::Label* m_LabelPool[ASW_LABEL_POOL_SIZE];
	vgui::ImagePanel* m_pLogo;
	CUtlVector<vgui::Label*> m_Scrolling;
	CUtlVector<vgui::Label*> m_FadingOut;
	KeyValues *pCreditsText;
	KeyValues *pCurrentCredit;
	float m_fNextMessage;
	int m_iCurMessage;
	vgui::HFont m_Font;
	bool m_bShowCreditsLogo;
};

#endif // _INCLUDED_MEDAL_PANEL_H