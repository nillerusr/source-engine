//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "DemoPage.h"

#include <VGUI/IVGui.h>
#include <vgui_controls/Controls.h>

#include <vgui_controls/Menu.h> 
#include <vgui_controls/MenuItem.h>
#include <vgui_controls/MenuButton.h>
#include <Keyvalues.h>
#include <vgui_controls/Label.h>

using namespace vgui;


class SampleMenus: public DemoPage
{
	public:
		SampleMenus(Panel *parent, const char *name);
		~SampleMenus();
		void InitMenus();
		void OnCommand( const char *command );
				
private:		
		// Menu that opens when button is pressed
		Menu *m_pMenu;

		Menu *m_pOuterMenu;
		Menu *m_pInnerMenu;
		Menu *m_pInnerMenu2;

		Menu *m_pScrollMenu;

		// Button to trigger the menu
		MenuButton *m_pMenuButton;
		MenuButton *m_pOuterMenuButton;
		MenuButton *m_pScrollMenuButton;

		// These are the same menus as above with checkable menu items in them
		MenuButton *m_pMenuButton2;
		MenuButton *m_pOuterMenuButton2;
		MenuButton *m_pScrollMenuButton2;

		Menu *m_pMenu2;

		Menu *m_pOuterMenu2;
		Menu *m_pInnerMenu_2;
		Menu *m_pInnerMenu22;

		Menu *m_pScrollMenu2;


		MenuButton *m_pMenuButton3;
		Menu *m_pMenu3;

		MenuButton *m_pMenuButton4;
		Menu *m_pMenu4;
		
		MenuButton *m_pMenuButton5;
		Menu *m_pMenu5;
				
};
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SampleMenus::SampleMenus(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// A fixed width menu
	m_pMenuButton = new MenuButton(this, "AMenuButton", "Fixed width");
	int wide, tall;
	m_pMenuButton->GetContentSize(wide, tall);
	m_pMenuButton->SetSize(wide + Label::Content, tall + Label::Content);
	// Position the menu button in the window.
	m_pMenuButton->SetPos(95, 15);

	m_pMenu = new Menu(m_pMenuButton, "AMenu");
	m_pMenu->AddMenuItem("Menu Item", "junk", this);
	m_pMenu->AddMenuItem("Another Choice", "junk", this);
	m_pMenu->AddMenuItem("Menu Item", "junk", this);
	m_pMenu->SetVisible(false);
	m_pMenuButton->SetMenu(m_pMenu);

	m_pMenuButton->GetSize(wide, tall);	
	m_pMenu->SetFixedWidth(wide);


	// Cascading menu
	InitMenus();


	// Scrolling menu
	m_pScrollMenuButton = new MenuButton(this, "AMenuButton", "Scrolling");
	m_pScrollMenuButton->GetContentSize(wide, tall);
	m_pScrollMenuButton->SetSize(wide + Label::Content, tall + Label::Content);
	// Position the menu button in the window.
	m_pScrollMenuButton->SetPos(360, 15);
	m_pScrollMenu = new Menu(m_pScrollMenuButton, "AScrollingMenu");
	m_pScrollMenu->AddMenuItem("Freezes", "junk", this);
	m_pScrollMenu->AddMenuItem("Kipup", "junk", this);
	m_pScrollMenu->AddMenuItem("Donkey", "junk", this);
	m_pScrollMenu->AddMenuItem("Sidewinder", "junk", this);
	m_pScrollMenu->AddMenuItem("Handspin", "junk", this);
	m_pScrollMenu->AddMenuItem("Coffee Grinder", "junk", this);
	m_pScrollMenu->AddMenuItem("Headspin", "junk", this);
	m_pScrollMenu->AddMenuItem("The Worm", "junk", this);
	m_pScrollMenu->SetNumberOfVisibleItems(5); 
	m_pScrollMenu->SetVisible(false);
	m_pScrollMenuButton->SetMenu(m_pScrollMenu);



	// A fixed width menu with checkable menu items and items with names longer than the menu width
	m_pMenuButton2 = new MenuButton(this, "AMenuButton", "A Check menu");
	m_pMenuButton2->GetContentSize(wide, tall);
	m_pMenuButton2->SetSize(wide + Label::Content, tall + Label::Content);

	// Position the menu button in the window.
	m_pMenuButton2->SetPos(95, 85);
	m_pMenu2 = new Menu(m_pMenuButton2, "AMenu");
	m_pMenu2->AddCheckableMenuItem("Menu Item Long Long Name", "junk", this );
	m_pMenu2->AddCheckableMenuItem("Another Choice", "junk", this );
	// last item not checkable for testing
	m_pMenu2->AddMenuItem("Menu Item", "junk", this);
	m_pMenu2->SetVisible(false);
	m_pMenuButton2->SetMenu(m_pMenu2);
	m_pMenuButton2->GetSize(wide, tall);	
	m_pMenu2->SetFixedWidth(wide);

	// Cascading menu with checkable menu items and items with names longer than the menu width
	InitMenus();

	// Scrolling menu with checkable menu items and items with names longer than the menu width
	m_pScrollMenuButton2 = new MenuButton(this, "AMenuButton", "Scrolling");
	m_pScrollMenuButton2->GetContentSize(wide, tall);
	m_pScrollMenuButton2->SetSize(wide + Label::Content, tall + Label::Content);

	// Position the menu button in the window.
	m_pScrollMenuButton2->SetPos(360, 85);
	m_pScrollMenu2 = new Menu(m_pScrollMenuButton2, "AScrollingMenu");
	m_pScrollMenu2->AddCheckableMenuItem("Freezes", "junk", this);
	m_pScrollMenu2->AddCheckableMenuItem("Kipup", "junk", this);
	m_pScrollMenu2->AddCheckableMenuItem("Donkey", "junk", this);
	m_pScrollMenu2->AddCheckableMenuItem("Sidewinder", "junk", this);
	m_pScrollMenu2->AddCheckableMenuItem("Handspin", "junk", this);
	m_pScrollMenu2->AddCheckableMenuItem("Coffee Grinder", "junk", this);
	m_pScrollMenu2->AddCheckableMenuItem("Headspin", "junk", this);
	// last item not checkable for testing
	m_pScrollMenu2->AddMenuItem("The Worm", "junk", this);
	m_pScrollMenu2->SetNumberOfVisibleItems(5); 
	m_pScrollMenu2->SetVisible(false);
	m_pScrollMenuButton2->SetMenu(m_pScrollMenu2);
	// Lets check off some stuff
	m_pMenu2->SetMenuItemChecked( 1, true );
	m_pScrollMenu2->SetMenuItemChecked( 0, true );
	m_pScrollMenu2->SetMenuItemChecked( 2, true );
	m_pScrollMenu2->SetMenuItemChecked( 3, true );


	Label *buttonLabel = new Label (this, "buttonLabel", "These are the same menus as above with checkable/elipsis menuitems");
	buttonLabel->SetPos(95, 60);
	buttonLabel->SizeToContents();


	// A button to toggle a check setting in a menu
	Button *toggleMenuCheckButton =  new Button (this, "Toggle Menu Check", "Toggle Simple Menu Check");
	toggleMenuCheckButton->SetCommand("Check");
	toggleMenuCheckButton->AddActionSignalTarget(this);
	toggleMenuCheckButton->GetContentSize(wide, tall);
	toggleMenuCheckButton->SetSize(wide + Label::Content, tall + Label::Content);
	toggleMenuCheckButton->SetPos( 95, 220);


	// A non fixed width menu with checkable menu items 
	m_pMenuButton3 = new MenuButton(this, "AMenuButton", "A menu");
	m_pMenuButton3->GetContentSize(wide, tall);
	m_pMenuButton3->SetSize(wide + Label::Content, tall + Label::Content);
	// Position the menu button in the window.
	m_pMenuButton3->SetPos(285, 220);
	m_pMenu3 = new Menu(m_pMenuButton3, "AMenu");
	m_pMenu3->AddCheckableMenuItem("Menu Item", "junk", this);
	m_pMenu3->AddCheckableMenuItem("Another Choice", "junk", this);
	m_pMenu3->AddCheckableMenuItem("Menu Item", "junk", this);
	m_pMenu3->SetVisible(false);
	m_pMenuButton3->SetMenu(m_pMenu3);


	// A non fixed width menu 
	m_pMenuButton4 = new MenuButton(this, "AMenuButton", "A menu");
	m_pMenuButton4->GetContentSize(wide, tall);
	m_pMenuButton4->SetSize(wide + Label::Content, tall + Label::Content);
	// Position the menu button in the window.
	m_pMenuButton4->SetPos(355, 220);
	m_pMenu4 = new Menu(m_pMenuButton4, "AMenu");
	m_pMenu4->AddMenuItem("Menu Item", "junk", this);
	m_pMenu4->AddMenuItem("Another Choice", "junk", this);
	m_pMenu4->AddMenuItem("Menu Item", "junk", this);
	m_pMenu4->SetVisible(false);
	m_pMenuButton4->SetMenu(m_pMenu4);


	// A non fixed width menu with a minimum width
	m_pMenuButton5 = new MenuButton(this, "AMenuButton", "A Minimum Width Menu");
	m_pMenuButton5->GetContentSize(wide, tall);
	m_pMenuButton5->SetSize(wide + Label::Content, tall + Label::Content);

	m_pMenuButton5->GetContentSize(wide, tall);
	m_pMenuButton5->SetSize(wide + Label::Content, tall + Label::Content);
	// Position the menu button in the window.
	m_pMenuButton5->SetPos(355, 270);
	m_pMenu5 = new Menu(m_pMenuButton5, "AMenu");
	m_pMenu5->AddMenuItem("Menu Item", "junk", this);
	m_pMenu5->AddMenuItem("Another Choice", "junk", this);
	m_pMenu5->AddMenuItem("Menu Item", "junk", this);
	m_pMenu5->SetVisible(false);
	m_pMenuButton5->GetSize(wide, tall);
	m_pMenu5->SetMinimumWidth(wide);
	m_pMenuButton5->SetMenu(m_pMenu5);

}


//-----------------------------------------------------------------------------
// Purpose: Create cascading menus.
// We will create a Menu with 2 Menus inside it, and yet another Menu inside one of those!
//-----------------------------------------------------------------------------
void SampleMenus::InitMenus()
{
	// A drop down menu button for the top most menu
	m_pOuterMenuButton = new MenuButton(this, "OuterMenu", "Cascading");

	// Size the Button so we can read its label.
	int wide, tall;
	m_pOuterMenuButton->GetContentSize(wide, tall);
	m_pOuterMenuButton->SetSize(wide + Label::Content, tall + Label::Content);


	// Create the Menu to go with it.
	m_pOuterMenu = new Menu(m_pOuterMenuButton, "OuterMenu");

	// Add the menu items to this menu
	m_pOuterMenu->SetVisible(false);
	
	m_pOuterMenu->SetNumberOfVisibleItems(5); 

	// Attach this menu to the menu button
	m_pOuterMenuButton->SetMenu(m_pOuterMenu);

	// Position the menu button on screen.
	m_pOuterMenuButton->SetPos(220, 15);

	// Create cascading menu #1 
	m_pInnerMenu = new Menu(m_pOuterMenu, "InnerMenu");

	// Add menu items to this menu.
	m_pInnerMenu->AddMenuItem("Marcia", "junk", this);
	m_pInnerMenu->AddMenuItem("Greg", "junk", this);
	m_pInnerMenu->AddMenuItem("Peter", "junk", this);
	m_pInnerMenu->SetVisible(false);

	// Now add the cascading menu to the top menu as a menu item!
	m_pOuterMenu->AddCascadingMenuItem("Cascades", this, m_pInnerMenu);

	// Create cascading menu #2
	m_pInnerMenu2 = new Menu(m_pOuterMenu, "InnerMenu2");
	m_pInnerMenu2->AddMenuItem("One Fish", "junk", this);
	m_pInnerMenu2->AddMenuItem("Two Fish", "junk", this);
	m_pInnerMenu2->AddMenuItem("Red Fish", "junk", this);
	m_pInnerMenu2->AddMenuItem("Blue Fish", "junk", this);
	m_pInnerMenu2->SetVisible(false);
	
	// Add this cascading menu to the top menu as a manu item.
	m_pOuterMenu->AddCascadingMenuItem("Cascading Choice", this, m_pInnerMenu2);


	// Make one of the items in the menu disabled.
	m_pOuterMenu->AddMenuItem("Normal Menuitem", "junk", this);
	m_pOuterMenu->AddMenuItem("Disabled Menuitem", "Disabled", this);

	Panel *menuItem = m_pOuterMenu->FindChildByName("Disabled Menuitem");
	assert(menuItem);
	menuItem->SetEnabled(false);

	m_pOuterMenu->AddMenuItem("Normal Menuitem", "junk", this);






	// A drop down menu button for the top most menu
	m_pOuterMenuButton2 = new MenuButton(this, "OuterMenu", "Cascading");

	// Size the Button so we can read its label.
	m_pOuterMenuButton2->GetContentSize(wide, tall);
	m_pOuterMenuButton2->SetSize(wide + Label::Content, tall + Label::Content);

	// Create the Menu to go with it.
	m_pOuterMenu2 = new Menu(m_pOuterMenuButton2, "OuterMenu");

	// Add the menu items to this menu
	m_pOuterMenu2->SetVisible(false);
	
	m_pOuterMenu2->SetNumberOfVisibleItems(7); 

	// Attach this menu to the menu button
	m_pOuterMenuButton2->SetMenu(m_pOuterMenu2);

	// Position the menu button on screen.
	m_pOuterMenuButton2->SetPos(220, 85);

	// Create cascading menu #1 
	m_pInnerMenu_2 = new Menu(m_pOuterMenu2, "InnerMenu");

	// Add menu items to this menu.
	m_pInnerMenu_2->AddCheckableMenuItem("Marcia", "junk", this);
	m_pInnerMenu_2->AddCheckableMenuItem("Greg", "junk", this);
	m_pInnerMenu_2->AddMenuItem("Peter", "junk", this);
	m_pInnerMenu_2->SetVisible(false);

	// Now add the cascading menu to the top menu as a menu item!
	m_pOuterMenu2->AddCascadingMenuItem("Cascades", this, m_pInnerMenu_2);

	// Create cascading menu #2
	m_pInnerMenu22 = new Menu(m_pOuterMenu2, "InnerMenu2");
	m_pInnerMenu22->AddCheckableMenuItem("One Fish", "junk", this);
	m_pInnerMenu22->AddCheckableMenuItem("Two Fish", "junk", this);
	m_pInnerMenu22->AddCheckableMenuItem("Red Fish", "junk", this);
	m_pInnerMenu22->AddCheckableMenuItem("Blue Fish", "junk", this);
	m_pInnerMenu22->SetVisible(false);
	
	// Add this cascading menu to the top menu as a manu item.
	m_pOuterMenu2->AddCascadingMenuItem("Cascading Choice", this, m_pInnerMenu22);


	// Make one of the items in the menu disabled.
	m_pOuterMenu2->AddMenuItem("Normal Menuitem", "junk", this);
	// checkable so we can see what disabled checkable items look like
	m_pOuterMenu2->AddCheckableMenuItem("Disabled Menuitem", "Disabled", this);

	menuItem = m_pOuterMenu2->FindChildByName("Disabled Menuitem");
	assert(menuItem);
	menuItem->SetEnabled(false);

	m_pOuterMenu2->AddMenuItem("Normal Menuitem", "junk", this);
	m_pOuterMenu2->AddCheckableMenuItem("Normal Checkable", "junk", this);

	// Lets check off some stuff for starters
	m_pOuterMenu2->SetMenuItemChecked( 3, true );
	m_pOuterMenu2->SetMenuItemChecked( 5, true );
	//m_pInnerMenu_2->SetMenuItemChecked( 0, true );
	m_pInnerMenu22->SetMenuItemChecked( 0, true );
	m_pInnerMenu22->SetMenuItemChecked( 1, true );
	m_pInnerMenu22->SetMenuItemChecked( 2, true );
	// another way of checking a menu item
	m_pInnerMenu22->GetMenuItem(3)->SetChecked( true );
	

}

void SampleMenus::OnCommand( const char *command )
{
	// Hitting the button will toggle the checking and unchecking 
	// of the first item of menu3
	if (!stricmp(command, "Check"))
	{
		if (!m_pMenu3->GetMenuItem(0)->IsChecked())
			// check the first menu item in the first menu
			m_pMenu3->SetMenuItemChecked( 0, true );
		else
			// another way of setting the checked state.
			m_pMenu3->GetMenuItem(0)->SetChecked(false);
	}

	Panel::OnCommand(command);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
SampleMenus::~SampleMenus()
{
}



Panel* SampleMenus_Create(Panel *parent)
{
	return new SampleMenus(parent, "Menus");
}


