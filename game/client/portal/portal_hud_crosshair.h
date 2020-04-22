//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HUD_PORTAL_CROSSHAIR_H
#define HUD_PORTAL_CROSSHAIR_H
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
class CHudPortalCrosshair : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudPortalCrosshair, vgui::Panel );
public:
	CHudPortalCrosshair( const char *pElementName );

	void			SetCrosshairAngle( const QAngle& angle );
	void			SetCrosshair( CHudTexture *texture, Color& clr );
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
};


// Enable/disable crosshair rendering.
extern ConVar crosshair;


#endif // HUD_PORTAL_CROSSHAIR_H
