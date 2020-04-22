//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "MenuDemo.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
MenuDemo::MenuDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// It takes 3 parts to make a simple menu.
	// 1. A MenuButton that triggers the menu to open.
	// 2. A Menu that opens	when the button is pressed.
	// 3. MenuItems that are listed in the menu and execute commands when selected.

	// First create a menu button.
	m_pMenuButton = new MenuButton(this, "AMenuButton", "Press Me To Open Menu!");

	// Size the Button to its label.
	int wide, tall;
	m_pMenuButton->GetContentSize(wide, tall);
	m_pMenuButton->SetSize(wide + Label::Content, tall + Label::Content);


	// Next create a menu, and link it to the menu button
	m_pMenu = new Menu(m_pMenuButton, "AMenu");

	// Now add menu items to the menu. These are the choices that will be seen.
	
	// In adding an item the first arg is the name of the item as it will
	// appear in the list, '&' chars are used to flag windows hotkeys that
	// will also work to select the item. They will have a little underline
	// underneath them.
	// The second arg is a KeyValues structure, that in this case holds a string name
	// of the command to be sent when the item is selected. 
	// The third arg is the target of the command, we are sending all commands
	// back to this class.
	m_pMenu->AddMenuItem("&Homer", new KeyValues ("Homer"), this);
	m_pMenu->AddMenuItem("&Apu", new KeyValues ("Apu"), this);
	m_pMenu->AddMenuItem("&Bart", new KeyValues ("Bart"), this);
	// A Menu Item with no hotkey
	m_pMenu->AddMenuItem("Lisa", new KeyValues ("Lisa"), this);
	m_pMenu->AddMenuItem("&George", new KeyValues ("George"), this);
	m_pMenu->AddMenuItem("&Marge", new KeyValues ("Marge"), this);
	// The 'M' hotkey was already used in Marge, 'a' and 'g' are also taken.  
	// Use another letter not taken for the hotkey.
	m_pMenu->AddMenuItem("Maggi&e", new KeyValues ("Maggie"), this);

	// Hide the menu to start. The button will make it visible.
	m_pMenu->SetVisible(false);

	// Tell the menu button which menu it should open.
	m_pMenuButton->SetMenu(m_pMenu);

	// Position the menu button in the window.
	int x, y, dwide, dtall;
	GetBounds (x, y, dwide, dtall);
	m_pMenuButton->SetPos(10, dtall/2);
	
	// By default the menu's width will be that of the largest item it contains.

	// You can also set a fixed width, which we will do so our menu doesn't look so 
	// scrawny.
	// Get the size of the menu button
	m_pMenuButton->GetSize(wide, tall);
	
	// Set the width of the menu to be the same as the menu button.
	// Comment this line out if you want to see the menu in its original scrawny size
	m_pMenu->SetFixedWidth(wide);

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
MenuDemo::~MenuDemo()
{
}

//-----------------------------------------------------------------------------
// Purpose: This is the function that will be executed when the "Maggie"
// messsage is recieved. Selecting the "Maggie" menu item will trigger
// this message to be sent. This class has been selected as the target of the 
// button, and our Message Map translates this string message to a function
// command.
//-----------------------------------------------------------------------------
void MenuDemo::OnMaggie()
{
     // Put a breakpoint here and watch the code break 
	// when you select "Maggie"
	ivgui()->DPrintf("Maggie selected.\n");

}


// explain message passing
MessageMapItem_t MenuDemo::m_MessageMap[] =
{
	MAP_MESSAGE( MenuDemo, "Maggie", OnMaggie ),   // from outermenu
};

IMPLEMENT_PANELMAP(MenuDemo, DemoPage);




Panel* MenuDemo_Create(Panel *parent)
{
	return new MenuDemo(parent, "MenuDemo");
}


