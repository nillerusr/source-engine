#ifndef _INCLUDED_BRIEFINGTOOLIP_H
#define _INCLUDED_BRIEFINGTOOLIP_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PHandle.h>

// this is the tooltip panel used throughout the briefing

class BriefingTooltip : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( BriefingTooltip, vgui::Panel );
public:
	BriefingTooltip(Panel *parent, const char *panelName);
	virtual ~BriefingTooltip();

	void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible

	vgui::Panel* GetTooltipPanel() { return m_pTooltipPanel; }
	void SetTooltip(vgui::Panel* pPanel, const char* szMainText, const char* szSubText,
						int iTooltipX, int iTooltipY, bool bAlignTopLeft=false);
	void SetTooltip(vgui::Panel* pPanel, const wchar_t* szMainText, const wchar_t* szSubText,
						int iTooltipX, int iTooltipY, bool bAlignTopLeft=false);
		
	virtual void PerformLayout();
	vgui::Panel* m_pTooltipPanel;
	vgui::Label* m_pMainLabel;
	vgui::Label* m_pSubLabel;
	vgui::HFont m_MainFont, m_SubFont;	
	int m_iTooltipX, m_iTooltipY;

	// allow disabling of tooltips
	void SetTooltipsEnabled(bool bEnabled) { m_bTooltipsEnabled = bEnabled; }
	bool m_bTooltipsEnabled;
	
	bool m_bAlignTopLeft;
};


extern vgui::DHANDLE<BriefingTooltip> g_hBriefingTooltip;

#endif // _INCLUDED_BRIEFINGTOOLIP_H