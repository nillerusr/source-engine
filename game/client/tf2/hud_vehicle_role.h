//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HUD_VEHICLE_ROLE_H
#define HUD_VEHICLE_ROLE_H
#ifdef _WIN32
#pragma once
#endif


#include "hudelement.h"
#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>

class CVehicleRoleHudElement : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CVehicleRoleHudElement, vgui::Panel );
public:
	CVehicleRoleHudElement( const char *pElementName );
	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void	Paint( void );

	// Set which role to display on the hud.
	void ShowVehicleRole( int iRole );
private:
	int m_iRole;
	vgui::HFont	m_hTextFont;
};


#endif // HUD_VEHICLE_ROLE_H
