#ifndef _INCLUDED_MAP_LOCATION_H
#define _INCLUDED_MAP_LOCATION_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

namespace vgui
{
	class Label;
	class WrappedLabel;
	class ImagePanel;
};

class CampaignPanel;

// this draws a location on the campaign map

class CampaignMapLocation : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CampaignMapLocation, vgui::Panel );
public:
	CampaignMapLocation(vgui::Panel *parent, const char *name, CampaignPanel* pPanel, int iMission);
	virtual ~CampaignMapLocation();
	virtual void OnMouseReleased(vgui::MouseCode code);
	virtual void OnCursorEntered();
	void ResizeTo(int width, int height);  // resizes us and our image
	void SetSelected(bool bSelected);
	bool ValidMissionChoice();
	void ApplySchemeSettings( vgui::IScheme *scheme );	
	virtual void OnThink();
	int FindNumVotes();

	bool m_bValidMissionChoice;
	vgui::ImagePanel* m_pLocationImage;
	vgui::ImagePanel* m_pBlueCircle;
	CampaignPanel* m_pCampaignPanel;
	int m_iMission;
	int m_iNumVotes;
};


#endif // _INCLUDED_MAP_LOCATION_H