#ifndef _INCLUDED_ASW_VGUI_MANIPULATOR_H
#define _INCLUDED_ASW_VGUI_MANIPULATOR_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>

namespace vgui
{
	class Button;
	class Label;
};

// This panel is used to manipulate other panels and report their sizes, for easy coding and positioning of panels
//  I know there's editablepanel, but this class can attach to any panel and is designed for the way I've been coding vgui stuff so far (i.e. code created and driven, with lots of animation, frameless)


class CASW_VGUI_Manipulator : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASW_VGUI_Manipulator, vgui::Panel );
public:
	CASW_VGUI_Manipulator(vgui::Panel *pParent, vgui::Panel *pTarget);
	virtual ~CASW_VGUI_Manipulator();

	void PositionButtons();
	void OnCommand(const char *command);
	virtual void OnScreenSizeChanged(int iOldWide, int iOldTall);
	void ResizeFor(int width, int height);
	void Think();
	void ApplySchemeSettings(vgui::IScheme *pScheme);
	void OnMouseReleased(vgui::MouseCode code);

	void SetTarget(vgui::Panel *pPanel);
	bool ShouldSkipPanel(vgui::Panel *pPanel);
	static void EditPanel(vgui::Panel *pParent, vgui::Panel *pTargetPanel);

	void FindAllPanelsUnderCursor();
	void FindAllPanelsUnderCursorRecurse(vgui::Panel *pPanel);
	CUtlVector<vgui::Panel*> m_UnderCursorList;


	vgui::Panel *m_pTarget;
	// buttons for manipulating the target panel
	vgui::Button *m_pResizeButton[8];
	vgui::Label *m_pPositionLabel;
};

extern CASW_VGUI_Manipulator *g_pASW_VGUI_Manipulator;

#endif // _INCLUDED_ASW_VGUI_MANIPULATOR_H