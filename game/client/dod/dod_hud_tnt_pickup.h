//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_HUD_TNT_PICKUP_H
#define DOD_HUD_TNT_PICKUP_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include "IconPanel.h"

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDODHudTNTPickupPanel : public CHudElement, public vgui::EditablePanel
{
public:
	DECLARE_CLASS_SIMPLE( CDODHudTNTPickupPanel, vgui::EditablePanel );

	CDODHudTNTPickupPanel( const char *pElementName );

	virtual void Init();
	virtual void VidInit();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnScreenSizeChanged( int iOldWide, int iOldTall );
	virtual void FireGameEvent( IGameEvent *event );
	virtual void OnThink( void );

private:
	bool m_bInitLayout;

	CIconPanel	*m_pTNTImage;
	vgui::Panel	*m_pBackground;

	vgui::Label *m_pPickupLabel;

	float m_flShowUntilTime;
};

#endif // DOD_HUD_TNT_PICKUP_H