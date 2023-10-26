#include "cbase.h"
#include "asw_vgui_skip_intro.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASW_VGUI_Skip_Intro::CASW_VGUI_Skip_Intro( vgui::Panel *pParent, const char *pElementName) 
:	vgui::Panel( pParent, pElementName ),
	CASW_VGUI_Ingame_Panel()
{
	SetKeyBoardInputEnabled(true);	
	RequestFocus();
}

CASW_VGUI_Skip_Intro::~CASW_VGUI_Skip_Intro()
{
}

void CASW_VGUI_Skip_Intro::PerformLayout()
{
	SetBounds(0, 0, ScreenWidth(), ScreenHeight());
}

void CASW_VGUI_Skip_Intro::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundType(0);
	SetPaintBackgroundEnabled(false);
	SetBgColor( Color(0,0,0,255) );
	SetMouseInputEnabled(true);
}

bool CASW_VGUI_Skip_Intro::MouseClick(int x, int y, bool bRightClick, bool bDown)
{
	engine->ClientCmd("cl_skip_intro");
	MarkForDeletion();
	SetVisible(false);
	return true;	// swallow click
}