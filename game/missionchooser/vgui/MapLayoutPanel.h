#ifndef TILEGEN_MAPLAYOUTPANEL_H
#define TILEGEN_MAPLAYOUTPANEL_H
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
class CNPC_Spawns_Panel;

// shows the current map layout the player has put together
class CMapLayoutPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CMapLayoutPanel, vgui::Panel );

public:

	CMapLayoutPanel(Panel *parent, const char *name);
	virtual ~CMapLayoutPanel();

	virtual void OnMouseReleased(vgui::MouseCode code);
	virtual void OnMousePressed(vgui::MouseCode code);
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void Paint();
	
	void GetCursorTile(int &tilex, int &tiley);	// returns the x and y of the tile under the mouse cursor

	void AddRoom( const CRoomTemplate *pTemplate, int iTileX, int iTileY );
	void RemoveRoom(CRoom *pRoom);
	void CreateAllUIPanels();
	void CreateRoomUIPanel( CRoom *pRoom );

	CMissionChooserTGAImagePanel *m_pPlayerStartImagePanel;
	CNPC_Spawns_Panel *m_pSpawnsPanel;

private:
	CMapLayout* GetMapLayout();
};

#endif TILEGEN_MAPLAYOUTPANEL_H