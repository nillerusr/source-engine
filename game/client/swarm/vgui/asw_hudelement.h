#ifndef _INCLUDED_ASW_HUDELEMENT_H
#define _INCLUDED_ASW_HUDELEMENT_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"

class CASW_HudElement : public CHudElement
{
public:
//	DECLARE_CLASS_SIMPLE( CASW_HudElement, CHudElement );

	CASW_HudElement( const char *pElementName );

	// Return true if this hud element should be visible in the current hud state
	virtual bool				ShouldDraw( void );

protected:
	/// simple convenience for printing unformatted text with a drop shadow
	static void DrawColoredTextWithDropShadow( const vgui::HFont &font, int x, int y, int r, int g, int b, int a, char *fmt );
};

#endif //_INCLUDED_ASW_HUDELEMENT_H