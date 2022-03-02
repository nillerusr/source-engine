//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSTRIKEBUYMENU_H
#define CSTRIKEBUYMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/WizardPanel.h>

#include <buymenu.h>

class BuyPresetEditPanel;
class BuyPresetButton;

namespace vgui
{
	class Panel;
	class Button;
	class Label;
}

enum
{
	NUM_BUY_PRESET_BUTTONS = 4,
};

//============================
// Base CS buy Menu
//============================
class CCSBaseBuyMenu : public CBuyMenu
{
private:
	typedef vgui::WizardPanel BaseClass;

public:
	CCSBaseBuyMenu(IViewPort *pViewPort, const char *subPanelName);

	virtual void ShowPanel( bool bShow );
	virtual void Paint( void );
	virtual void SetVisible( bool state );

	void HandleBlackMarket( void );

private:
	void UpdateBuyPresets( bool showDefaultPanel = false );	///< Update the Buy Preset buttons and their info panels on the main buy menu
	vgui::Panel *m_pMainBackground;
	BuyPresetButton *m_pBuyPresetButtons[NUM_BUY_PRESET_BUTTONS];
	BuyPresetEditPanel *m_pLoadout;
	vgui::Label *m_pMoney;
	int m_lastMoney;

	vgui::EditablePanel *m_pBlackMarket;
	HFont m_hUnderlineFont;

	// Background panel -------------------------------------------------------

public:
	virtual void PaintBackground();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	bool m_backgroundLayoutFinished;

	// End background panel ---------------------------------------------------
};

//============================
// CT main buy Menu
//============================
class CCSBuyMenu_CT : public CCSBaseBuyMenu
{
private:
	typedef vgui::WizardPanel BaseClass;

public:
	CCSBuyMenu_CT(IViewPort *pViewPort);

	virtual const char *GetName( void ) { return PANEL_BUY_CT; }
};


//============================
// Terrorist main buy Menu
//============================
class CCSBuyMenu_TER : public CCSBaseBuyMenu
{
private:
	typedef vgui::WizardPanel BaseClass;

public:
	CCSBuyMenu_TER(IViewPort *pViewPort);
	
	virtual const char *GetName( void ) { return PANEL_BUY_TER; }
};

#endif // CSTRIKEBUYMENU_H
