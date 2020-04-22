//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HL1_HUD_NUMBERS_H
#define HL1_HUD_NUMBERS_H
#ifdef _WIN32
#pragma once
#endif


#include <vgui_controls/Panel.h>


class CHL1HudNumbers : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHL1HudNumbers, vgui::Panel );

public:
	CHL1HudNumbers( vgui::Panel *parent, const char *name );
	void	VidInit( void );

protected:
	int		DrawHudNumber( int x, int y, int iNumber, Color &clrDraw );
	int		GetNumberFontHeight( void );
	int		GetNumberFontWidth( void );

private:
	CHudTexture *icon_digits[ 10 ];
};


#endif // HL1_HUD_NUMBERS_H
