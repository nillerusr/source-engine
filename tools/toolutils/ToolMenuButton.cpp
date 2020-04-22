//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#include "toolutils/toolmenubutton.h"
#include "toolutils/toolmenubar.h"
#include "toolutils/basetoolsystem.h"
#include "vgui_controls/menu.h"
#include "vgui_controls/KeyBindingMap.h"
#include "vgui/ILocalize.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CToolMenuButton::CToolMenuButton( Panel *parent, const char *panelName, const char *text, Panel *actionTarget ) :
	BaseClass( parent, panelName, text ),
	m_pActionTarget( actionTarget )
{
	m_pMenu = new Menu( this, "Menu" );
}

void CToolMenuButton::Reset()
{
	m_Items.RemoveAll();
	m_pMenu->DeleteAllItems();
}

int CToolMenuButton::AddMenuItem( char const *itemName, const char *itemText, KeyValues *message, Panel *target, const KeyValues *userData /*= NULL*/, char const *kbcommandname /*= NULL*/ )
{
	int id = m_pMenu->AddMenuItem(itemText, message, target, userData);
	MenuItem_t item;
	item.m_ItemID = id;
	if ( kbcommandname )
	{
		item.m_KeyBinding = kbcommandname;
	}
	m_Items.Insert( itemName, item );
	return id;
}

int CToolMenuButton::AddCheckableMenuItem( char const *itemName, const char *itemText, KeyValues *message, Panel *target, const KeyValues *userData /*= NULL*/, char const *kbcommandname /*= NULL*/ )
{
	int id = m_pMenu->AddCheckableMenuItem(itemText, message, target, userData);
	MenuItem_t item;
	item.m_ItemID = id;
	if ( kbcommandname )
	{
		item.m_KeyBinding = kbcommandname;
	}
	m_Items.Insert( itemName, item );
	return id;
}

int CToolMenuButton::AddMenuItem( char const *itemName, const wchar_t *itemText, KeyValues *message, Panel *target, const KeyValues *userData /*= NULL*/, char const *kbcommandname /*= NULL*/ )
{
	int id = m_pMenu->AddMenuItem(itemName, itemText, message, target, userData);
	MenuItem_t item;
	item.m_ItemID = id;
	if ( kbcommandname )
	{
		item.m_KeyBinding = kbcommandname;
	}
	m_Items.Insert( itemName, item );
	return id;
}

int CToolMenuButton::AddCheckableMenuItem( char const *itemName, const wchar_t *itemText, KeyValues *message, Panel *target, const KeyValues *userData /*= NULL*/, char const *kbcommandname /*= NULL*/ )
{
	int id = m_pMenu->AddCheckableMenuItem(itemName, itemText, message, target, userData);
	MenuItem_t item;
	item.m_ItemID = id;
	if ( kbcommandname )
	{
		item.m_KeyBinding = kbcommandname;
	}
	m_Items.Insert( itemName, item );
	return id;
}

void CToolMenuButton::AddSeparator()
{
	m_pMenu->AddSeparator();
}

void CToolMenuButton::SetItemEnabled( int itemID, bool state )
{
	m_pMenu->SetItemEnabled( m_Items[ itemID ].m_ItemID, state );
}

int CToolMenuButton::FindMenuItem( char const *itemName )
{
	int id = m_Items.Find( itemName );
	if ( id == m_Items.InvalidIndex() )
		return -1;
	return m_Items[ id ].m_ItemID;
}

void CToolMenuButton::AddSeparatorAfterItem( char const *itemName )
{
	int id = FindMenuItem( itemName );
	if ( id != -1 )
	{
		m_pMenu->AddSeparatorAfterItem( id );
	}
}

void CToolMenuButton::MoveMenuItem( int itemID, int moveBeforeThisItemID )
{
	m_pMenu->MoveMenuItem( itemID, moveBeforeThisItemID );
}

void CToolMenuButton::SetCurrentKeyBindingLabel( char const *itemName, char const *binding )
{
	int id = FindMenuItem( itemName );
	if ( id != -1 )
	{
		m_pMenu->SetCurrentKeyBinding( id, binding );
	}
}



void CToolMenuButton::UpdateMenuItemKeyBindings()
{
	if ( !m_pActionTarget )
		return;

	int c = m_Items.Count();
	for ( int i = 0; i < c; ++i )
	{
		if ( !m_Items[ i ].m_KeyBinding.IsValid() )
			continue;

		char const *bindingName = m_Items[ i ].m_KeyBinding.String();

		CUtlVector< BoundKey_t * > list;
		m_pActionTarget->LookupBoundKeys( bindingName, list );
		if ( list.Count() <= 0 )
			continue;

		BoundKey_t *kb = list[ 0 ];
		Assert( kb );

		// Found it, now convert to binding string
		// First do modifiers
		wchar_t sz[ 256 ];
		wcsncpy( sz, Panel::KeyCodeModifiersToDisplayString( (KeyCode)kb->keycode, kb->modifiers ), 256 );
		sz[ 255 ] = L'\0';

		char ansi[ 512 ];
		g_pVGuiLocalize->ConvertUnicodeToANSI( sz, ansi, sizeof( ansi ) );
		m_pMenu->SetCurrentKeyBinding( m_Items[ i ].m_ItemID, ansi );

	}
}

void CToolMenuButton::OnShowMenu( Menu *menu )
{
	CToolMenuBar *bar = dynamic_cast< CToolMenuBar * >( GetParent() );
	if ( bar )
	{
		CBaseToolSystem *sys = bar->GetToolSystem();
		if ( sys )
		{
			sys->UpdateMenu( menu );
		}
	}

	UpdateMenuItemKeyBindings();

	m_pMenu->ForceCalculateWidth();
}

vgui::Menu *CToolMenuButton::GetMenu()
{
	return m_pMenu;
}


