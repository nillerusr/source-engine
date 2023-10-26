#ifndef _INCLUDED_OBJECTIVE_TITLE_PANEL_H
#define _INCLUDED_OBJECTIVE_TITLE_PANEL_H

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
class ObjectiveListBox;

// this Panel will show a single objective text in a thin horizonal black box
// when clicked, the text will highlight and the box will grow over time
// to also display the objective pic

class ObjectiveTitlePanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( ObjectiveTitlePanel, vgui::Panel );
public:
	ObjectiveTitlePanel(Panel *parent, const char *name);
	virtual ~ObjectiveTitlePanel();

	virtual void OnMouseReleased(vgui::MouseCode code);
	virtual void OnCursorEntered();

	virtual void PerformLayout();

	virtual void OnThink();
	void UpdateElements();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

	void SetObjective(C_ASW_Objective* pObjective);
	C_ASW_Objective* GetObjective() { return m_hObjective.Get(); }
	void SetSelected(bool bSelected);

	vgui::Label* m_ObjectiveLabel;
	vgui::ImagePanel* m_ObjectiveImagePanel;
	vgui::ImagePanel* m_pCheckbox;

	void SetIndex(int i) { m_Index = i; }
	int m_Index;
	vgui::HTexture m_TextureID;

	CHandle<C_ASW_Objective> m_hObjective;
	bool m_bObjectiveSelected;

	Color m_BlueColor;

	char m_szImageName[256];

	vgui::HFont m_hFont, m_hSmallFont;
	ObjectiveListBox* m_pListBox;
};

#endif // _INCLUDED_OBJECTIVE_TITLE_PANEL_H
