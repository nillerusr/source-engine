#ifndef _INCLUDED_MEDALS_TOOLTIP_H
#define _INCLUDED_MEDALS_TOOLTIP_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Label.h>

class MedalsTooltip : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( MedalsTooltip, vgui::Panel );
public:
	MedalsTooltip(Panel *parent, const char *panelName);
	virtual ~MedalsTooltip();

	void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible

	vgui::Panel* GetTooltipPanel() { return m_pTooltipPanel; }
	void SetTooltip(vgui::Panel* pPanel, const char* szMainText, const char* szSubText,
						int iTooltipX, int iTooltipY);
	
	virtual void OnScreenSizeChanged(int iOldWide, int iOldTall);
	void ResizeFor(int width, int height);		// given the supplied screen resolution, resize this gui element appropriately
	virtual void PerformLayout();
	vgui::Panel* m_pTooltipPanel;
	vgui::Label* m_pMainLabel;
	vgui::Label* m_pSubLabel;
	vgui::HFont m_MainFont, m_SubFont;
	int m_iLastWidth;
	int m_iTooltipX, m_iTooltipY;
};

#endif // _INCLUDED_MEDALS_TOOLTIP_H