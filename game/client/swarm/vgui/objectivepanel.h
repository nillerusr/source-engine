#ifndef OBJECTIVEPANEL_H
#define OBJECTIVEPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

class C_ASW_Player;
class Label;
class ImagePanel;

// this panel is used during the briefing to show a particular objective name and image in the list on the left

class ObjectivePanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( ObjectivePanel, vgui::Panel );
public:
	ObjectivePanel(Panel *parent, const char *name);
	virtual ~ObjectivePanel();

	virtual void PerformLayout();
	
	vgui::Label* m_ObjectiveLabel;
	vgui::ImagePanel* m_ObjectiveImagePanel;

	bool m_bImageSet, m_bTextSet;
};


#endif // OBJECTIVEPANEL_H
