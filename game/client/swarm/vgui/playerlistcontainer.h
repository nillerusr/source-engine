#ifndef _INCLUDED_PLAYER_LIST_CONTAINER_H
#define _INCLUDED_PLAYER_LIST_CONTAINER_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Frame.h>

// some containers used to hold the PlayerListPanel (different ones used during game and during briefing/debrief)

class PlayerListContainer : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( PlayerListContainer, vgui::Panel );
public:
	PlayerListContainer(vgui::Panel *parent, const char *name);
	virtual ~PlayerListContainer();
	
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
};

class PlayerListContainerFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( PlayerListContainerFrame, vgui::Frame );

	PlayerListContainerFrame(Panel *parent, const char *panelName, bool showTaskbarIcon = true);
	virtual ~PlayerListContainerFrame();

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();
};

#endif // _INCLUDED_PLAYER_LIST_CONTAINER_H