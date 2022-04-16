//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DOD_HUD_PLAYERSTATUS_FIRESELECT_H
#define DOD_HUD_PLAYERSTATUS_FIRESELECT_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include "IconPanel.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: FireSelect panel
//-----------------------------------------------------------------------------
class CDoDHudFireSelect : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CDoDHudFireSelect, vgui::EditablePanel );

public:
	CDoDHudFireSelect( vgui::Panel *parent, const char *name );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnThink();
	virtual void SetVisible( bool state );

private:

	CDoDCutEditablePanel		*m_pBackground;

	CIconPanel *m_pIconMP44;
	CIconPanel *m_pIconBAR;

	CIconPanel *m_pBulletLeft;
	CIconPanel *m_pBulletCenter;
	CIconPanel *m_pBulletRight;
};	

#endif // DOD_HUD_PLAYERSTATUS_FIRESELECT_H