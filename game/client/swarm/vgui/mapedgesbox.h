#ifndef _INCLUDED_MAP_EDGES_BOX_H
#define _INCLUDED_MAP_EDGES_BOX_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>

// draws dashed lines around its edge.  Used to frame the map on the campaign screen.

class MapEdgesBox : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( MapEdgesBox, vgui::Panel );
public:
	MapEdgesBox(vgui::Panel *parent, const char *panelName, Color col);
	virtual void Paint();
	void DrawMapEdges();
	void DrawALine(int x, int y, int x2, int y2, bool bVertThick=false, bool bHorizThick=false);
	
	Color m_Color;

	static int s_nWhiteTexture;
};


#endif // _INCLUDED_MAP_EDGES_BOX_H