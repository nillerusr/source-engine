//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "MenuDemo.h"

class MenuDemo;

using namespace vgui;


class MenuDemo2 : public MenuDemo
{	
	public:
		MenuDemo2(Panel *parent, const char *name);
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
MenuDemo2::MenuDemo2(Panel *parent, const char *name) : MenuDemo(parent, name)
{	
	// Set the number of items visible in the menu at one time to 5
	// If there are more than 5 items in the menu a scrollbar will
	// be added automatically
	m_pMenu->SetNumberOfVisibleItems(5);

	// Lets also make it so this menu opens up above the button
	// instead of below it
	m_pMenuButton->SetOpenDirection(Menu::UP);
}


Panel* MenuDemo2_Create(Panel *parent)
{
	return new MenuDemo2(parent, "MenuDemo2");
}


