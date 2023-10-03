//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VHYBRIDBUTTON_H__
#define __VHYBRIDBUTTON_H__

#include "basemodui.h"

//=============================================================================
// The Hybrid Button has the ability to either display a texture , solid color
//	or gradient color depending on it's state.
//=============================================================================

namespace BaseModUI
{

class BaseModHybridButton : public vgui::Button
{
public:
	DECLARE_CLASS_SIMPLE( BaseModHybridButton, vgui::Button );

	enum State
	{
		Enabled = 0,
		Disabled,
		Focus,
		FocusDisabled,
		Open,			//when a hybrid button has spawned a flyout menu
		NUMSTATES		//must always be last!
	};

	enum ButtonStyle_t
	{
		BUTTON_SIMPLE,
		BUTTON_MAINMENU,		// item strictly on the main or ingame menu
		BUTTON_FLYOUTITEM,		// item inside of a flyout
		BUTTON_DROPDOWN,		// button that has an associated value with an optional flyout
		BUTTON_DIALOG,			// button inside of a dialog, centered		
		BUTTON_RED,				// same as simple button, but colored red
		BUTTON_REDMAIN,			// same as simple button, but colored red
		BUTTON_SMALL,			// same as simple button, but colored red
		BUTTON_MEDIUM,			// same as simple button, medium font
		BUTTON_GAMEMODE,		// button on game mode carousel
		BUTTON_MAINMENUSMALL,	// smaller item strictly on the main or ingame menu
		BUTTON_ALIENSWARMMENUBUTTON,
		BUTTON_ALIENSWARMMENUBUTTONSMALL,
		BUTTON_ALIENSWARMDEFAULT,
	};

	enum EnableCondition
	{
		EC_ALWAYS,
		EC_LIVE_REQUIRED,
		EC_NOTFORDEMO,
	};

	BaseModHybridButton( Panel *parent, const char *panelName, const char *text, Panel *pActionSignalTarget = NULL, const char *pCmd = NULL );
	BaseModHybridButton( Panel *parent, const char *panelName, const wchar_t *text, Panel *pActionSignalTarget = NULL, const char *pCmd = NULL );
	virtual ~BaseModHybridButton();

	State			GetCurrentState();
	int				GetOriginalTall() { return m_originalTall; }
	ButtonStyle_t	GetStyle() { return m_nStyle; }
	
	//used by flyout menus to indicate this button has spawned a flyout.
	void		SetOpen();
	void		SetClosed();
	void		NavigateTo( );
	void		NavigateFrom( );

	void		SetHelpText( const char* tooltip , bool enabled = true );

	// only applies to drop down style
	void		SetDropdownSelection( const char *pText );
	void		EnableDropdownSelection( bool bEnable );

	void		UpdateFooterHelpText();

	char const  *GetHelpText( bool bEnabled ) const;

	void		SetShowDropDownIndicator( bool bShowIndicator ) { m_bShowDropDownIndicator = bShowIndicator; }
	void		SetOverrideDropDownIndicator( bool bOverrideDropDownIndicator ) { m_bOverrideDropDownIndicator = bOverrideDropDownIndicator; }

	virtual void ApplySettings( KeyValues *inResourceData );

	int			GetWideAtOpen() { return m_nWideAtOpen; }

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void OnKeyCodeReleased( vgui::KeyCode keycode );
	virtual void Paint();
	virtual void OnThink();
	virtual void OnCursorEntered();
	virtual void OnCursorExited();
	virtual void FireActionSignal( void );

	MESSAGE_FUNC( OnSiblingHybridButtonOpened, "OnSiblingHybridButtonOpened" );

	virtual Panel* NavigateUp();
	virtual Panel* NavigateDown();
	virtual Panel* NavigateLeft();
	virtual Panel* NavigateRight();

private:
	void		PaintButtonEx();

	int			m_originalTall;
	int			m_textInsetX;
	int			m_textInsetY;

	bool		m_isOpen;
	bool		m_isNavigateTo; //to help cure flashing
	bool		m_bOnlyActiveUser;
	bool		m_bIgnoreButtonA;

	enum UseIndex_t
	{
		USE_NOBODY = -2,		// Nobody can use the button
		USE_EVERYBODY = -1,		// Everybody can use the button
		USE_SLOT0 = 0,			// Only Slot0 can use the button
		USE_SLOT1,				// Only Slot1 can use the button
		USE_SLOT2,				// Only Slot2 can use the button
		USE_SLOT3,				// Only Slot3 can use the button
		USE_PRIMARY = 0xFF,		// Only primary user can use the button
	};

	int m_iUsablePlayerIndex;
	EnableCondition mEnableCondition;

	CUtlString	m_enabledToolText;
	CUtlString	m_disabledToolText;
	
	ButtonStyle_t m_nStyle;

	vgui::HFont	m_hTextFont;
	vgui::HFont	m_hTextBlurFont;
	vgui::HFont	m_hHintTextFont;

	int			m_nTextFontHeight;
	int			m_nHintTextFontHeight;

	vgui::HFont	m_hSelectionFont;
	vgui::HFont	m_hSelectionBlurFont;

	bool		m_bDropDownSelection;
	CUtlString	m_DropDownSelection;

	bool		m_bShowDropDownIndicator;	// down arrow used for player names that can be clicked on
	bool		m_bOverrideDropDownIndicator;	// down arrow used for player names that can be clicked on
	int			m_iSelectedArrow;			// texture ids for the arrow
	int			m_iUnselectedArrow;
	int			m_iSelectedArrowSize;		// size to draw the arrow

	int			m_nWideAtOpen;
};

}; //namespace BaseModUI

#endif //__VHYBRIDBUTTON_H__