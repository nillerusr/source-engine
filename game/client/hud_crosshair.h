//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HUD_CROSSHAIR_H
#define HUD_CROSSHAIR_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>

namespace vgui
{
	class IScheme;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudCrosshair : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudCrosshair, vgui::Panel );
public:
	CHudCrosshair( const char *pElementName );

	void			SetCrosshairAngle( const QAngle& angle );
	void			SetCrosshair( CHudTexture *texture, const Color& clr );
	void			ResetCrosshair();
	void			DrawCrosshair( void );
  	bool			HasCrosshair( void ) { return ( m_pCrosshair != NULL ); }
	bool			ShouldDraw();

protected:
	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void	Paint();

private:
	// Crosshair sprite and colors
	CHudTexture		*m_pCrosshair;
	CHudTexture		*m_pDefaultCrosshair;
	Color			m_clrCrosshair;
	QAngle			m_vecCrossHairOffsetAngle;

	QAngle			m_curViewAngles;
	Vector			m_curViewOrigin;

	CPanelAnimationVar( bool, m_bHideCrosshair, "never_draw", "false" );
};


// Enable/disable crosshair rendering.
extern ConVar crosshair;


#endif // HUD_CROSSHAIR_H
