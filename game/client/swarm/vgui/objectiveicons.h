#ifndef _INCLUDED_OBJECTIVE_ICONS_H
#define _INCLUDED_OBJECTIVE_ICONS_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/ImagePanel.h>

class C_ASW_Objective;

// responsible for the small icons that flash up next to the map relevant to each objective

class ObjectiveIcons: public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( ObjectiveIcons, vgui::Panel );
public:
	ObjectiveIcons(Panel *parent, const char *name);
	virtual ~ObjectiveIcons();
	void FadeOut();
	void SetObjective(C_ASW_Objective *pObjective);

	virtual void PerformLayout();
	virtual void OnThink();
	//virtual void Paint();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	
	vgui::ImagePanel* m_pIcon[5];
	bool m_bHaveQueued;
	int m_iNumFading;
	C_ASW_Objective* m_pObjective;
	C_ASW_Objective* m_pQueuedObjective;
};

#endif // _INCLUDED_OBJECTIVE_ICONS_H
