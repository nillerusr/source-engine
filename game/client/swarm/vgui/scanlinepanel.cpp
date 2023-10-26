#include "cbase.h"
#include "ScanLinePanel.h"
#include "vgui/isurface.h"
#include <KeyValues.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ScanLinePanel::ScanLinePanel(vgui::Panel* parent, const char *panelName, bool bDouble) :
	vgui::Panel(parent, panelName),
	m_bDouble(bDouble)
{
}

void ScanLinePanel::Paint()
{
	vgui::surface()->DrawSetColor(GetFgColor());
	if (m_bDouble)
	{
		vgui::surface()->DrawSetTexture(m_nDoubleScanLine);	
	}
	else
	{
		vgui::surface()->DrawSetTexture(m_nSingleScanLine);	
	}
	int w, t;
	GetSize(w, t);
	float u = float(w) / 4.0f;
	float v = float(t) / 4.0f;
	vgui::Vertex_t points[4] = 
	{ 
	vgui::Vertex_t( Vector2D(0, 0), Vector2D(0,0) ), 
	vgui::Vertex_t( Vector2D(w, 0), Vector2D(u,0) ), 
	vgui::Vertex_t( Vector2D(w, t), Vector2D(u,v) ), 
	vgui::Vertex_t( Vector2D(0, t), Vector2D(0,v) ) 
	}; 
	vgui::surface()->DrawTexturedPolygon( 4, points );
}