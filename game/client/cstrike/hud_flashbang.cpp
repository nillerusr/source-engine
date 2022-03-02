//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include "clientmode_csnormal.h"
#include "c_cs_player.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"

class CHudFlashbang : public CHudElement, public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CHudFlashbang, vgui::Panel );

	virtual bool ShouldDraw();	
	virtual void Paint();

	CHudFlashbang( const char *name );

private:

	int m_iAdditiveWhiteID;
};


DECLARE_HUDELEMENT( CHudFlashbang );


CHudFlashbang::CHudFlashbang( const char *pName ) :
	vgui::Panel( NULL, "HudFlashbang" ), CHudElement( pName )
{
	SetParent( g_pClientMode->GetViewport() );
	
	m_iAdditiveWhiteID = 0;

	SetHiddenBits( HIDEHUD_PLAYERDEAD );
}

// the flashbang effect cannot be drawn in the HUD, because this lets the user skip its effect
// by hitting Escape, or by setting "cl_drawhud 0".
bool CHudFlashbang::ShouldDraw()
{
	return true;
}

// the flashbang effect cannot be drawn in the HUD, because this lets the user skip its effect
// by hitting Escape, or by setting "cl_drawhud 0".
void CHudFlashbang::Paint()
{
	return;
}

