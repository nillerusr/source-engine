#ifndef TILEGEN_NPC_SPAWNS_PANEL_H
#define TILEGEN_NPC_SPAWNS_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "TileGenDialog.h"

namespace vgui
{
	class Scrollbar;
	class Menu;
};

class CRoomTemplate;
class CRoom;
class CMissionChooserTGAImagePanel;

// shows position of fixed encounters in the map layout
class CNPC_Spawns_Panel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CNPC_Spawns_Panel, vgui::Panel );

public:

	CNPC_Spawns_Panel(Panel *parent, const char *name);
	virtual ~CNPC_Spawns_Panel();

	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void Paint();

private:
	CMapLayoutPanel* GetMapLayoutPanel();
	CMapLayout* GetMapLayout();
	vgui::HFont m_hTextFont;
	int m_nWhiteTextureId;
};

#endif TILEGEN_NPC_SPAWNS_PANEL_H