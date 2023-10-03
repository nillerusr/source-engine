#ifndef _INCLUDED_OBJECTIVE_LISTBOX_H
#define _INCLUDED_OBJECTIVE_LISTBOX_H

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
class ObjectiveDetailsPanel;
class ObjectiveMap;
class ObjectiveIcons;

// responsible for managing all the ObjectiveTitlePanels

class ObjectiveListBox : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( ObjectiveListBox, vgui::Panel );
public:
	ObjectiveListBox(Panel *parent, const char *name);
	virtual ~ObjectiveListBox();
	
	virtual void PerformLayout();

	virtual void OnThink();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	void ClickedTitle(ObjectiveTitlePanel* pTitle);
	bool ClickFirstTitle();
	void SetDetailsPanel(ObjectiveDetailsPanel* pDetails) { m_pDetailsPanel = pDetails; }
	void SetMapPanel(ObjectiveMap* pMap) { m_pMapPanel = pMap; }
	void SetIcons(ObjectiveIcons* pIcons) { m_pObjectiveIcons = pIcons; }

	ObjectiveTitlePanel* m_pTitlePanel[12];
	ObjectiveTitlePanel* m_pSelectedTitle;
	ObjectiveDetailsPanel* m_pDetailsPanel;
	ObjectiveMap* m_pMapPanel;
	ObjectiveIcons* m_pObjectiveIcons;
	int m_iNumTitlePanels;
};

#endif // _INCLUDED_OBJECTIVE_LISTBOX_H
