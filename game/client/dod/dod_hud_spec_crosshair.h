//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HUD_SPEC_CROSSHAIR_H
#define HUD_SPEC_CROSSHAIR_H
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
class CHudSpecCrosshair : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudSpecCrosshair, vgui::Panel );
public:
	CHudSpecCrosshair( const char *pElementName );

	void				DrawCrosshair( void );
  	bool				HasCrosshair( void )		{ return ( m_pCrosshair != NULL ); }

protected:
	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void	Paint();

private:
	// Crosshair sprite and colors
	CHudTexture			*m_pCrosshair;
	Color				m_clrCrosshair;
};


// Enable/disable crosshair rendering.
extern ConVar crosshair;


#endif // HUD_SPEC_CROSSHAIR_H
