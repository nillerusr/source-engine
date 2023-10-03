#ifndef _INCLUDED_FORCE_READY_PANEL_H
#define _INCLUDED_FORCE_READY_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>

class ImageButton;
class BriefingTooltip;
class CNB_Gradient_Bar;
class CNB_Button;

// this panel asks the leader player if he wants to force other players to be ready

class ForceReadyPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( ForceReadyPanel, vgui::Panel );
public:
	ForceReadyPanel(vgui::Panel *parent, const char *name, const char *message, int iForceReadyType);	
	virtual ~ForceReadyPanel();

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnCommand(const char *command);
	
	CNB_Gradient_Bar* m_pGradientBar;
	vgui::Panel *m_pMessageBox;
	vgui::Label *m_pMessage;
	CNB_Button* m_pForceButton;
	CNB_Button* m_pCancelButton;
	int m_iForceReadyType;
};

extern vgui::DHANDLE<vgui::Panel> g_hPopupDialog;

#endif // _INCLUDED_FORCE_READY_PANEL_H
