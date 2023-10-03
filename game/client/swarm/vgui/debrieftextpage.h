#ifndef _INCLUDED_DEBRIEF_TEXT_PAGE_H
#define _INCLUDED_DEBRIEF_TEXT_PAGE_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>

#define ASW_NUM_DEBRIEF_PARAS 3

class C_ASW_Debrief_Stats;
class CNB_Island;

// shows debrief text after successfully completing a mission

class DebriefTextPage : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( DebriefTextPage, vgui::Panel );
public:
	DebriefTextPage(vgui::Panel *parent, const char *name);
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);	
	virtual void PerformLayout();
	virtual void OnThink();
	void UpdateStrings();
	void LayoutStrings();
	
	CNB_Island* m_pBackground;
	vgui::Label* m_pPara[ASW_NUM_DEBRIEF_PARAS];	
	CHandle<C_ASW_Debrief_Stats> m_hDebriefStats;
	bool m_bStringUpdated;

	bool m_bMissionSuccess;
};

#endif // _INCLUDED_DEBRIEF_TEXT_PAGE_H
