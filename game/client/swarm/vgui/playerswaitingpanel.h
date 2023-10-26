#ifndef _INCLUDED_PLAYERS_WAITING_PANEL_H
#define _INCLUDED_PLAYERS_WAITING_PANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include "asw_shareddefs.h"

namespace vgui
{
	class Label;
};

// lists player names along with whether they're ready or waiting (for campaign screen vote)
class PlayersWaitingPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( PlayersWaitingPanel, vgui::Panel );
public:
	PlayersWaitingPanel(vgui::Panel *parent, const char *panelName);
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnThink();
	
	vgui::Label* m_pTitle;
	vgui::Label* m_pPlayerNameLabel[ASW_MAX_READY_PLAYERS];
	vgui::Label* m_pPlayerReadyLabel[ASW_MAX_READY_PLAYERS];
};


#endif // _INCLUDED_PLAYERS_WAITING_PANEL_H