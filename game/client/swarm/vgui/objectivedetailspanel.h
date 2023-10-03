#ifndef _INCLUDED_OBJECTIVE_DETAILS_PANEL_H
#define _INCLUDED_OBJECTIVE_DETAILS_PANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include <vgui/IImage.h>

class C_ASW_Player;
class Label;
class ImagePanel;
class C_ASW_Objective;
class ObjectiveTitlePanel;

// this panel shows the text description for the currently selected objective
// used during briefing on the mission tab

class ObjectiveDetailsPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( ObjectiveDetailsPanel, vgui::Panel );
public:
	ObjectiveDetailsPanel(Panel *parent, const char *name);
	virtual ~ObjectiveDetailsPanel();

	virtual void OnThink();
	void UpdateText();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	void SetObjective(C_ASW_Objective* pObjective);
	virtual void PerformLayout();
	int GetTextWidth();

	C_ASW_Objective* m_pObjective;
	C_ASW_Objective* m_pQueuedObjective;
	vgui::PanelListPanel *m_pListPanel;
	vgui::Panel* m_pDescriptionContainer;
	vgui::Label* m_pDescription[4];
	int m_iTotalTextTall;
};

#endif // _INCLUDED_OBJECTIVE_DETAILS_PANEL_H
