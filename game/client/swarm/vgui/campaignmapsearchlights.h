#ifndef _INCLUDED_CAMPAIGN_MAP_SEARCH_LIGHTS_H
#define _INCLUDED_CAMPAIGN_MAP_SEARCH_LIGHTS_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

#define ASW_NUM_SEARCH_LIGHTS 4

// draws oscillating search lights over the campaign map

class CampaignMapSearchLights : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CampaignMapSearchLights, vgui::Panel );
public:
	CampaignMapSearchLights(vgui::Panel *parent, const char *panelName);
	virtual void Paint();
	void PaintSearchLights();
	void PaintSearchLight(int x, int y, float angle);

	static int s_nSearchLightTexture;
};


#endif // _INCLUDED_CAMPAIGN_MAP_SEARCH_LIGHTS_H