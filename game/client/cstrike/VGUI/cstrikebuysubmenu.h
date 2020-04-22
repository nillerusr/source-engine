//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSBUYSUBMENU_H
#define CSBUYSUBMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <buysubmenu.h>
#include <buymouseoverpanelbutton.h>

using namespace vgui;

class CCSBuySubMenu : public CBuySubMenu
{
private:
	DECLARE_CLASS_SIMPLE(CCSBuySubMenu, CBuySubMenu); 

public:
	CCSBuySubMenu (vgui::Panel *parent,const char *name = "BuySubMenu") : CBuySubMenu( parent, name )
	{
		m_backgroundLayoutFinished = false;
	}; 
	
protected:

	virtual void OnThink();
	void UpdateVestHelmPrice();

	virtual void OnCommand( const char *command );

	MouseOverPanelButton* CreateNewMouseOverPanelButton(EditablePanel * panel)
	{ 
		return new BuyMouseOverPanelButton(this, NULL, panel);
	}

	CBuySubMenu* CreateNewSubMenu() { return new CCSBuySubMenu( this, "BuySubMenu" ); }

	// Background panel -------------------------------------------------------

	virtual void PerformLayout();
	virtual void OnSizeChanged(int newWide, int newTall);	// called after the size of a panel has been changed
	
	void HandleBlackMarket( void );

	bool m_backgroundLayoutFinished;

	// End background panel ---------------------------------------------------
};

#endif //CSBUYSUBMENU_H
