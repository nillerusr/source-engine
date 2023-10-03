#ifndef TILEGEN_PLACEDROOMTEMPLATEPANEL_H
#define TILEGEN_PLACEDROOMTEMPLATEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "RoomTemplatePanel.h"

class CRoom;

// draws a particlar room template (based on its image and grid/exits)
class CPlacedRoomTemplatePanel : public CRoomTemplatePanel
{
	DECLARE_CLASS_SIMPLE( CPlacedRoomTemplatePanel, CRoomTemplatePanel );

public:

	CPlacedRoomTemplatePanel(CRoom* pRoom, Panel *parent, const char *name);
	virtual ~CPlacedRoomTemplatePanel();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnMouseReleased(vgui::MouseCode code);
	virtual void OnMousePressed(vgui::MouseCode code);
	virtual void MarkForDeletion();

	virtual void OnDragged();

	CRoom *m_pRoom;

private:
	bool m_bSetAlpha, m_bStartedGrowAnimation, 	m_bSelectedOnThisPress, m_bDragged;
};

#endif TILEGEN_PLACEDROOMTEMPLATEPANEL_H