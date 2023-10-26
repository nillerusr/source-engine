#ifndef _INCLUDED_RESTARTMISSIONBUTTON_H
#define _INCLUDED_RESTARTMISSIONBUTTON_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Button.h>
#include "ImageButton.h"

// button shown after a mission ends.  Can be clicked to retry the mission

class RestartMissionButton : public ImageButton
{
	DECLARE_CLASS_SIMPLE( RestartMissionButton, ImageButton );
public:
	RestartMissionButton(Panel *parent, const char *panelName, const char *text);
	RestartMissionButton(Panel *parent, const char *panelName, const wchar_t *wszText);
	virtual ~RestartMissionButton();
	virtual void OnMouseReleased(vgui::MouseCode code);
	virtual void PerformLayout();
	void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible

	bool m_bCanStart;
};

#endif // _INCLUDED_RESTARTMISSIONBUTTON_H