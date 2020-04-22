//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#ifndef TOOLMENUBUTTON_H
#define TOOLMENUBUTTON_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/menubutton.h"
#include "tier1/utldict.h"
#include "tier1/utlsymbol.h"


//-----------------------------------------------------------------------------
// Base class for tools menus
//-----------------------------------------------------------------------------
class CToolMenuButton : public vgui::MenuButton
{
	DECLARE_CLASS_SIMPLE( CToolMenuButton, vgui::MenuButton );
public:
	CToolMenuButton( vgui::Panel *parent, const char *panelName, const char *text, vgui::Panel *actionTarget );

	virtual void OnShowMenu(vgui::Menu *menu);

	vgui::Menu	*GetMenu();

	// Add a simple text item to the menu
	virtual int AddMenuItem( char const *itemName, const char *itemText, KeyValues *message, Panel *target, const KeyValues *userData = NULL, char const *kbcommandname = NULL );
	virtual int AddCheckableMenuItem( char const *itemName, const char *itemText, KeyValues *message, Panel *target, const KeyValues *userData = NULL, char const *kbcommandname = NULL );

	// Wide-character version to add a simple text item to the menu
	virtual int AddMenuItem( char const *itemName, const wchar_t *itemText, KeyValues *message, Panel *target, const KeyValues *userData = NULL, char const *kbcommandname = NULL );
	virtual int AddCheckableMenuItem( char const *itemName, const wchar_t *itemText, KeyValues *message, Panel *target, const KeyValues *userData = NULL, char const *kbcommandname = NULL );

	virtual int FindMenuItem( char const *itemName );
	virtual void AddSeparatorAfterItem( char const *itemName );
	virtual void MoveMenuItem( int itemID, int moveBeforeThisItemID );

	virtual void SetItemEnabled( int itemID, bool state );

	// Pass in a NULL binding to clear it
	virtual void SetCurrentKeyBindingLabel( char const *itemName, char const *binding );

	virtual void AddSeparator();

	void		Reset();

protected:
	void		UpdateMenuItemKeyBindings();

	vgui::Menu	*m_pMenu;
	vgui::Panel	*m_pActionTarget;

	struct MenuItem_t
	{
		MenuItem_t()
			: m_ItemID( 0 ),
			  m_KeyBinding( UTL_INVAL_SYMBOL )
		{
		}
		unsigned short	m_ItemID;
		CUtlSymbol		m_KeyBinding;
	};

	CUtlDict< MenuItem_t, unsigned short >	m_Items;
};


#endif // TOOLMENUBUTTON_H

