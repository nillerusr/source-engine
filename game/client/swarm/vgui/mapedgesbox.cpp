#include "cbase.h"
#include "MapEdgesBox.h"
#include "vgui/ISurface.h"
#include <KeyValues.h>
#include "SoftLine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


MapEdgesBox::MapEdgesBox(vgui::Panel *parent, const char *panelName, Color col) :
	vgui::Panel(parent, panelName)
{
	m_Color = col;	
}

void MapEdgesBox::Paint()
{
	DrawMapEdges();
}

int MapEdgesBox::s_nWhiteTexture = -1;

void MapEdgesBox::DrawALine(int x, int y, int x2, int y2, bool bVertThick, bool bHorizThick)
{
	vgui::Vertex_t start, end;
	start.Init(Vector2D(x,y), Vector2D(0,0));
	end.Init(Vector2D(x2,y2), Vector2D(1,1));
	SoftLine::DrawPolygonLine(start, end);
	return;
	if (bVertThick)
	{
		start.m_Position.x += 1;
		end.m_Position.x += 1;
		SoftLine::DrawPolygonLine(start, end);
	}
	if (bHorizThick)
	{
		start.m_Position.y += 1;
		end.m_Position.y += 1;
		SoftLine::DrawPolygonLine(start, end);
	}
}

void MapEdgesBox::DrawMapEdges()
{
	if (s_nWhiteTexture == -1)
	{
		s_nWhiteTexture = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( s_nWhiteTexture, "vgui/white" , true, false);
		if (s_nWhiteTexture == -1)
			return;
		return;
	}

	// draw main line
	vgui::surface()->DrawSetTexture(s_nWhiteTexture);
	vgui::surface()->DrawSetColor(m_Color);

	// draw box
	int w, t;
	GetSize(w, t);
	vgui::surface()->DrawOutlinedRect( 0, 0, w - 2, t - 2 );
// 	DrawALine(0, 0, w-2, 0, false, true);
// 	DrawALine(w-2, 0, w-2, t-2, true, false);
// 	DrawALine(w-2, t-2, 0, t-2, true, false);
// 	DrawALine(0, t-2, 0, 0, false, true);

	// draw horizontal subdivisions
	const int subs = 5;
	const int long_spike = t * 0.024f;
	//const int short_spike = t * 0.008f;
	for (int i=1;i<subs;i++)
	{
		int x = i * w * (1.0f / subs);
		DrawALine(x, 0, x, long_spike, true, false);
		DrawALine(x, t-2, x, t-(long_spike+2), true, false);

		// vert subs
		int y = i * t * (1.0f / subs);
		DrawALine(0, y, long_spike, y, false, true);
		DrawALine(t-2, y, t-(long_spike+2), y, false, true);
	}	
}