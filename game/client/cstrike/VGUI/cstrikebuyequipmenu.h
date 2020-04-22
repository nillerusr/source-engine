//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSTRIKEBUYEQUIPMENU_H
#define CSTRIKEBUYEQUIPMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/WizardPanel.h>

#include <buymenu.h>

namespace vgui
{
	class Panel;
}

//============================
// CT Equipment menu
//============================
class CCSBuyEquipMenu_CT : public CBuyMenu
{
private:
	typedef vgui::WizardPanel BaseClass;

public:
	CCSBuyEquipMenu_CT(IViewPort *pViewPort);

	virtual const char *GetName( void ) { return PANEL_BUY_EQUIP_CT; }

	// Background panel -------------------------------------------------------

public:
	virtual void PaintBackground();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	bool m_backgroundLayoutFinished;

	// End background panel ---------------------------------------------------
};


//============================
// Terrorist Equipment menu
//============================

class CCSBuyEquipMenu_TER : public CBuyMenu
{
private:
	typedef vgui::WizardPanel BaseClass;

public:
	CCSBuyEquipMenu_TER(IViewPort *pViewPort);

	virtual const char *GetName( void ) { return PANEL_BUY_EQUIP_TER; }

	// Background panel -------------------------------------------------------

public:
	virtual void PaintBackground();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	bool m_backgroundLayoutFinished;

	// End background panel ---------------------------------------------------
};

#endif // CSTRIKEBUYEQUIPMENU_H
