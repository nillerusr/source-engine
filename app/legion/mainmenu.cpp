//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: List of game systems to update  
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "menumanager.h"
#include "basemenu.h"


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
class CMainMenu : public CBaseMenu
{
	DECLARE_CLASS_SIMPLE( CMainMenu, CBaseMenu );

public:
	CMainMenu( vgui::Panel *pParent, const char *pPanelName );
	virtual ~CMainMenu();

private:
};


//-----------------------------------------------------------------------------
// Hooks the menu into the menu manager
//-----------------------------------------------------------------------------
REGISTER_MENU( "MainMenu", CMainMenu );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CMainMenu::CMainMenu( vgui::Panel *pParent, const char *pPanelName ) :
	BaseClass( pParent, pPanelName )
{
	LoadControlSettings( "resource/mainmenu.res", "GAME" ); 
}

CMainMenu::~CMainMenu()
{
}

