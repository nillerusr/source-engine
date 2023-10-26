#ifndef _INCLUDED_RETURNCAMPAIGNMAPBUTTON_H
#define _INCLUDED_RETURNCAMPAIGNMAPBUTTON_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Button.h>
#include "ImageButton.h"

// button shown after mission ends.  Can be clicked to abort the mission and return to the campaign map

class ReturnCampaignMapButton : public ImageButton
{
	DECLARE_CLASS_SIMPLE( ReturnCampaignMapButton, ImageButton );
public:
	ReturnCampaignMapButton(Panel *parent, const char *panelName, const char *text);
	ReturnCampaignMapButton(Panel *parent, const char *panelName, const wchar_t *wszText);
	virtual ~ReturnCampaignMapButton();
	virtual void OnMouseReleased(vgui::MouseCode code);
	virtual void PerformLayout();
	void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();							// called every frame before painting, but only if panel is visible
	bool m_bCanReturn;
};

#endif // _INCLUDED_RETURNCAMPAIGNMAPBUTTON_H