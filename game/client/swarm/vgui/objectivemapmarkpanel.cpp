#include "cbase.h"

#include "ObjectiveMapMarkPanel.h"
#include "vgui_controls/AnimationController.h"
#include <vgui/ISurface.h>


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

ObjectiveMapMarkPanel::ObjectiveMapMarkPanel(Panel *parent, const char *name) : Panel(parent, name)
{	
	m_iBracketSize = 10;
	m_bPulsing = false;
}
	
ObjectiveMapMarkPanel::~ObjectiveMapMarkPanel()
{

}

void ObjectiveMapMarkPanel::SetBracketScale(float f)
{
	m_fScale = f;
}

void ObjectiveMapMarkPanel::Think()
{	
	
	
}

void ObjectiveMapMarkPanel::Paint()
{
	BaseClass::Paint();

	if (GetAlpha() <= 0)
	{
		return;
	}

	// draw brackets
	vgui::surface()->DrawSetColor(GetAlpha(),GetAlpha(),GetAlpha(),255);
	SetZPos(200);

	int x1 = 1;
	int y1 = 1;
	int x2 = GetWide() - 1;
	int y2 = GetTall() - 1;

	if ( !m_bComplete )
	{
		vgui::surface()->DrawSetColor( 225, 40, 40, GetAlpha() );
	}
	else
	{
		vgui::surface()->DrawSetColor( 40, 225, 40, GetAlpha() );
	}

	int BracketSize = (ScreenHeight() / 768.0f) * 10.0f;
	vgui::surface()->DrawSetTexture(m_nTopLeftBracketTexture);
	vgui::surface()->DrawTexturedRect( x1, y1, x1+BracketSize, y1+BracketSize );
	vgui::surface()->DrawSetTexture(m_nTopRightBracketTexture);
	vgui::surface()->DrawTexturedRect( x2-BracketSize, y1, x2, y1+BracketSize );
	vgui::surface()->DrawSetTexture(m_nBottomLeftBracketTexture);
	vgui::surface()->DrawTexturedRect( x1, y2-BracketSize, x1+BracketSize, y2 );
	vgui::surface()->DrawSetTexture(m_nBottomRightBracketTexture);
	vgui::surface()->DrawTexturedRect( x2-BracketSize, y2-BracketSize, x2, y2 );
}