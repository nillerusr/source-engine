//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The main manager of the UI 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#ifndef UIMANAGER_H
#define UIMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include "gamemanager.h"
#include "vgui/vgui.h"
#include "tier1/convar.h"
#include "vgui_controls/panel.h"
#include "vgui_controls/phandle.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct InputEvent_t;
class CLegionConsoleDialog;

namespace vgui
{
	class Panel;
}


//-----------------------------------------------------------------------------
// Enum indicating the various main UI panels
// NOTE: The order in which they appear here is the order in which they will draw
//-----------------------------------------------------------------------------
enum UIRootPanel_t
{
	UI_ROOT_GAME = 0,
	UI_ROOT_MENU,
	UI_ROOT_TOOLS,

	UI_ROOT_PANEL_COUNT,
};


//-----------------------------------------------------------------------------
// UI management
//-----------------------------------------------------------------------------
class CUIManager : public CGameManager<>
{
public:
	CUIManager();

	// Inherited from IGameManager
	virtual bool Init();
	virtual void Update( );
	virtual void Shutdown();

	// Root panels
	vgui::Panel *GetRootPanel( UIRootPanel_t id );

	// Sets particular root panels to be visible
	void EnablePanel( UIRootPanel_t id, bool bEnable );

	// Attempt to process an input event, return true if it sholdn't be chained to the rest of the game
	bool ProcessInputEvent( const InputEvent_t& event );

	// Draws the UI
	void DrawUI();

	// Hides the console
	void HideConsole();

private:
	CON_COMMAND_MEMBER_F( CUIManager, "toggleconsole", ToggleConsole, "Toggle Console", 0 );

	vgui::VPANEL m_hEmbeddedPanel;
	vgui::Panel *m_pRootPanels[ UI_ROOT_PANEL_COUNT ];
	int m_nWindowWidth;
	int m_nWindowHeight;
	vgui::DHANDLE< CLegionConsoleDialog > m_hConsole;
};


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
extern CUIManager *g_pUIManager;


#endif // UIMANAGER_H

