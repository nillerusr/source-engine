//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base class menus should all inherit from 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "basemenu.h"
#include "menumanager.h"
#include <ctype.h>
#include "vgui/iinput.h"


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CBaseMenu::CBaseMenu( vgui::Panel *pParent, const char *pPanelName ) :
	BaseClass( pParent, pPanelName )
{
	SetKeyBoardInputEnabled( true );
	SetMouseInputEnabled( true );
	SetSizeable( false );
	SetMoveable( false );
}

CBaseMenu::~CBaseMenu()
{
}


void CBaseMenu::OnKeyCodeTyped( vgui::KeyCode code )
{
	BaseClass::OnKeyCodeTyped( code );

	bool shift = (vgui::input()->IsKeyDown(vgui::KEY_LSHIFT) || vgui::input()->IsKeyDown(vgui::KEY_RSHIFT));
	bool ctrl = (vgui::input()->IsKeyDown(vgui::KEY_LCONTROL) || vgui::input()->IsKeyDown(vgui::KEY_RCONTROL));
	bool alt = (vgui::input()->IsKeyDown(vgui::KEY_LALT) || vgui::input()->IsKeyDown(vgui::KEY_RALT));
	
	if ( ctrl && shift && alt && code == vgui::KEY_B)
	{
		// enable build mode
		ActivateBuildMode();
	}

}
	
//-----------------------------------------------------------------------------
// Commands
//-----------------------------------------------------------------------------
void CBaseMenu::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "quit" ) )
	{
		IGameManager::Stop();
		return;
	}

	if ( !Q_stricmp( pCommand, "popmenu" ) )
	{
		g_pMenuManager->PopMenu();
		return;
	}

	if ( !Q_stricmp( pCommand, "popallmenus" ) )
	{
		g_pMenuManager->PopAllMenus();
		return;
	}

	if ( !Q_strnicmp( pCommand, "pushmenu ", 9 ) )
	{
		const char *pMenuName = pCommand + 9;
		while( isspace(*pMenuName) )
		{
			++pMenuName;
		}
		g_pMenuManager->PushMenu( pMenuName );
		return;
	}

	if ( !Q_strnicmp( pCommand, "switchmenu ", 11 ) )
	{
		const char *pMenuName = pCommand + 11;
		while( isspace(*pMenuName) )
		{
			++pMenuName;
		}
		g_pMenuManager->SwitchToMenu( pMenuName );
		return;
	}

	BaseClass::OnCommand( pCommand );
}

