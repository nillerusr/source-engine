#ifndef _INCLUDED_MEDAL_PANEL_H
#define _INCLUDED_MEDAL_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

namespace vgui
{
	class ImagePanel;
	class Label;
	class ImagePanel;
};
class BriefingTooltip;

// shows the icon for a single medal and displays tooltip info about it when mouseovered
class MedalPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( MedalPanel, vgui::Panel );
public:
	MedalPanel(vgui::Panel *parent, const char *name, int iSlot, BriefingTooltip* pTooltip = NULL);
	
	virtual void OnThink();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	void SetMedalIndex(int i, bool bOffline);
	BriefingTooltip* GetTooltip();

	vgui::ImagePanel *m_pMedalIcon;
	int m_iMedalIndex;
	int m_iSlot;

	bool m_bShowTooltip;
	bool m_bOffline;

	char m_szMedalName[32];
	char m_szMedalDescription[32];

	vgui::DHANDLE<BriefingTooltip> m_hTooltip;
};

#endif // _INCLUDED_MEDAL_PANEL_H