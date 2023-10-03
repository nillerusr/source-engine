#ifndef _INCLUDED_FONTTESTPANEL_H
#define _INCLUDED_FONTTESTPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

namespace vgui
{
	class Label;
};

// this panel shows each font size up, scaled for resolution

#define ASW_FONT_TEST_LABELS 46

class FontTestPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( FontTestPanel, vgui::Panel );
public:
	FontTestPanel(vgui::Panel *parent, const char *name);
	virtual ~FontTestPanel();
	
	virtual void OnThink();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);	
	virtual void OnMouseReleased(vgui::MouseCode code);
	void SetLabelFont(vgui::IScheme *pScheme, int i, const char* font, const char* text, bool bScale);

	vgui::Label* m_pLabel[ASW_FONT_TEST_LABELS];

	int m_iPage;
};

#endif // _INCLUDED_FONTTESTPANEL_H