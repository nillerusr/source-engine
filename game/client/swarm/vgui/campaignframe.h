#ifndef _INCLUDED_CAMPAIGN_FRAME_H
#define _INCLUDED_CAMPAIGN_FRAME_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

class CampaignPanel;

// frame used to hold the campaign screen panels

class CampaignFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CampaignFrame, vgui::Frame );

	CampaignFrame(Panel *parent, const char *panelName, bool showTaskbarIcon = true);
	virtual ~CampaignFrame();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );

	CampaignPanel* m_pCampaignPanel;
};
#endif // _INCLUDED_CAMPAIGN_FRAME_H