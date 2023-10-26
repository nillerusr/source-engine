#ifndef _INCLUDED_MISSION_COMPLETE_FRAME_H
#define _INCLUDED_MISSION_COMPLETE_FRAME_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

class MissionCompletePanel;

// full screen frame to hold the mission complete panel

class MissionCompleteFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( MissionCompleteFrame, vgui::Frame );

	MissionCompleteFrame(bool bSuccess, Panel *parent, const char *panelName, bool showTaskbarIcon = true);

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();

	MissionCompletePanel* m_pMissionCompletePanel;

	bool m_bFadeInBackground;
	float m_fBackgroundFade;
	float m_fWrongState;
};
#endif // _INCLUDED_MISSION_COMPLETE_FRAME_H