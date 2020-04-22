//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_HUD_PLAYERSTATUS_TNT_H
#define DOD_HUD_PLAYERSTATUS_TNT_H
#ifdef _WIN32
#pragma once
#endif

#include "dodcornercutpanel.h"
#include "IconPanel.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: FireSelect panel
//-----------------------------------------------------------------------------
class CDoDHudTNT : public CDoDCutEditablePanel
{
	DECLARE_CLASS_SIMPLE( CDoDHudTNT, CDoDCutEditablePanel );

public:
	CDoDHudTNT( vgui::Panel *parent, const char *name );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void OnThink();
	virtual void SetVisible( bool state );

private:

	CIconPanel *m_pIconTNT;
	CIconPanel *m_pIconTNT_Missing;

	bool m_bHasTNT;

	int m_iIconX;
	int m_iIconY;
	int m_iIconW;
	int m_iIconH;

	CPanelAnimationVar( float, m_flIconAlpha, "icon_alpha", "255" );
};	

#endif // DOD_HUD_PLAYERSTATUS_TNT_H