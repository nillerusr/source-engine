#ifndef _INCLUDED_BRIEFING_FRAME_H
#define _INCLUDED_BRIEFING_FRAME_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

class BriefingImagePanel;
class CNB_Mission_Panel;
class CNB_Main_Panel;

// fullscreen frame used to show the briefing

class BriefingFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( BriefingFrame, vgui::Frame );

	BriefingFrame(Panel *parent, const char *panelName, bool showTaskbarIcon = true);
	virtual ~BriefingFrame();

	virtual void PerformLayout();
	virtual void OnClose();

	BriefingImagePanel* m_pBackdrop;

	// new briefing elements
	CNB_Main_Panel *m_pMainPanel;
	CNB_Mission_Panel* m_pMissionPanel;
};

// a simple black panel that fades in while the server is launching the mission (spawning marines, equipping them, etc)
class BriefingPreLaunchPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( BriefingPreLaunchPanel, vgui::Panel );
public:
	BriefingPreLaunchPanel(Panel *parent, const char *panelName);
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	float m_fFadeAlpha;
};
#endif // _INCLUDED_BRIEFING_FRAME_H