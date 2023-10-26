#ifndef _INCLUDED_TECHMARINEFAILPANEL_H
#define _INCLUDED_TECHMARINEFAILPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

namespace vgui
{
	class Label;
	class ImagePanel;
};

// this panel shows a message on screen about restarting the mission because all techs are dead

class TechMarineFailPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( TechMarineFailPanel, vgui::Panel );
public:
	TechMarineFailPanel(vgui::Panel *parent, const char *name);
	virtual ~TechMarineFailPanel();
	
	virtual void OnThink();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);	

	vgui::Panel *m_pBackground;
	vgui::Label *m_pFailedMessage;
	vgui::Label *m_pReasonMessage;
	vgui::ImagePanel *m_pIcon;
	bool m_bLeader;
	vgui::HFont m_hFont;

	static void ShowTechFailPanel();
	static int s_iTechPanels;
	static TechMarineFailPanel *s_pTechPanel;
};

#endif // _INCLUDED_TECHMARINEFAILPANEL_H