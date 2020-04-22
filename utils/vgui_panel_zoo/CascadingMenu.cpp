//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "DemoPage.h"

#include <VGUI/IVGui.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/MenuButton.h>
#include <Keyvalues.h>
#include <vgui_controls/Controls.h>



using namespace vgui;


class CascadingMenuDemo: public DemoPage
{
	public:
		CascadingMenuDemo(Panel *parent, const char *name);
		~CascadingMenuDemo();
		void InitMenus();
		
		// Functions that are executed in response to selecting ]
		// menu items.
		void OnMaggie();
		void OnHomer();
		void OnMarcia();
		void OnJohn();
		void OnRed();
		
	private:

		MenuButton *m_pOuterMenuButton;
		Menu *m_pOuterMenu;
		MenuButton *m_pInnerMenuButton;
		Menu *m_pInnerMenu;
		Menu *m_pInnerMenu2;
		Menu *m_pDeepestMenu;
		DECLARE_PANELMAP();
				
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CascadingMenuDemo::CascadingMenuDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	InitMenus();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CascadingMenuDemo::~CascadingMenuDemo()
{
}

//-----------------------------------------------------------------------------
// Purpose: Create cascading menus.
// We will create a Menu with 2 Menus inside it, and yet another Menu inside one of those!
//-----------------------------------------------------------------------------
void CascadingMenuDemo::InitMenus()
{
	// A drop down menu button for the top most menu
	m_pOuterMenuButton = new MenuButton(this, "OuterMenu", "Click to open menu");

	// Size the Button so we can read its label.
	m_pOuterMenuButton->SizeToContents();
	int wide, tall;
	m_pOuterMenuButton->GetContentSize(wide, tall);
	m_pOuterMenuButton->SetSize(wide + Label::Content, tall + Label::Content);


	// Create the Menu to go with it.
	m_pOuterMenu = new Menu(m_pOuterMenuButton, "OuterMenu");

	// Add the menu items to this menu
	m_pOuterMenu->AddMenuItem("&Homer", new KeyValues ("Homer"), this);
	m_pOuterMenu->AddMenuItem("&Apu", new KeyValues ("Apu"), this);
	m_pOuterMenu->AddMenuItem("&Bart", new KeyValues ("Bart"), this);
	m_pOuterMenu->AddMenuItem("Lisa", new KeyValues ("Lisa"), this);
	m_pOuterMenu->AddMenuItem("&George", new KeyValues ("George"), this);
	m_pOuterMenu->AddMenuItem("&Marge", new KeyValues ("Marge"), this);
	m_pOuterMenu->AddMenuItem("Maggi&e", new KeyValues ("Maggie"), this);
	m_pOuterMenu->SetVisible(false);
	
	// Make the number of visible menu items 20
	m_pOuterMenu->SetNumberOfVisibleItems(20); 

	// Attach this menu to the menu button
	m_pOuterMenuButton->SetMenu(m_pOuterMenu);

	// Position the menu button on screen.
	int x, y, dwide, dtall;
	GetBounds (x, y, dwide, dtall);
	m_pOuterMenuButton->SetPos(10, dtall/2);

	// Create cascading menu #1 
	// Cascading menu's don't need menu buttons, as they are triggered
	// by selecting the menu item of the menu they are in.
	m_pInnerMenu = new Menu(m_pOuterMenu, "InnerMenu");

	// Add menu items to this menu.
	m_pInnerMenu->AddMenuItem("Marcia", new KeyValues ("Marcia"), this);
	m_pInnerMenu->AddMenuItem("Greg", new KeyValues ("Greg"), this);
	m_pInnerMenu->AddMenuItem("&Peter", new KeyValues ("Peter"), this);
	m_pInnerMenu->AddMenuItem("AliceWithALongName", new KeyValues ("Alice"), this);
	m_pInnerMenu->AddMenuItem("&Carol", new KeyValues ("Carol"), this);
	m_pInnerMenu->AddMenuItem("&Tiger", new KeyValues ("Tiger"), this);
	m_pInnerMenu->AddMenuItem("&Sam", new KeyValues ("Sam"), this);
	m_pInnerMenu->AddMenuItem("&Bobby", new KeyValues ("Bobby"), this);
	m_pInnerMenu->AddMenuItem("Cindy", new KeyValues ("Cindy"), this);
	m_pInnerMenu->SetVisible(false);
	// Now add the cascading menu to the top menu as a menu item!
	m_pOuterMenu->AddCascadingMenuItem("InnerMenu", this, m_pInnerMenu);

	// Create cascading menu #2
	m_pInnerMenu2 = new Menu(m_pOuterMenu, "InnerMenu2");
	m_pInnerMenu2->AddMenuItem("John", new KeyValues ("John"), this);
	m_pInnerMenu2->AddMenuItem("Paul", new KeyValues ("Paul"), this);
	m_pInnerMenu2->AddMenuItem("Ringo", new KeyValues ("Ringo"), this);
	m_pInnerMenu2->AddMenuItem("George", new KeyValues ("George"), this);
	m_pInnerMenu2->AddMenuItem("Magical", new KeyValues ("Magical"), this);
	m_pInnerMenu2->AddMenuItem("Mystery", new KeyValues ("Mystery"), this);
	m_pInnerMenu2->AddMenuItem("Tour", new KeyValues ("Tour"), this);
	m_pInnerMenu2->AddMenuItem("Yellow Sub", new KeyValues ("Yellow Sub"), this);
	m_pInnerMenu2->SetVisible(false);
	
	// Add this cascading menu to the top menu as a manu item.
	m_pOuterMenu->AddCascadingMenuItem("InnerMenu2", this, m_pInnerMenu2);
	
	
	// Finally, a cascading menu inside a cascading menu!
	m_pDeepestMenu = new Menu(m_pInnerMenu, "DeepestMenu");

	// Add menu items to this menu.
	m_pDeepestMenu->AddMenuItem("Red", new KeyValues ("Red"), this);
	m_pDeepestMenu->AddMenuItem("Orange", new KeyValues ("Orange"), this);
	m_pDeepestMenu->AddMenuItem("Yellow", new KeyValues ("Yellow"), this);
	m_pDeepestMenu->AddMenuItem("Green", new KeyValues ("Green"), this);
	m_pDeepestMenu->AddMenuItem("Blue", new KeyValues ("Blue"), this);
	m_pDeepestMenu->AddMenuItem("Indigo", new KeyValues ("Indigo"), this);
	m_pDeepestMenu->AddMenuItem("Purple", new KeyValues ("Purple"), this);
	m_pDeepestMenu->AddMenuItem("Yellow Sun", new KeyValues ("Yellow Sun"), this);
	m_pDeepestMenu->SetVisible(false);

	// Set the number of visible items in the menu to 4, this menu will have a scrollbar.
	m_pDeepestMenu->SetNumberOfVisibleItems(4);	

	// Add this menu item to one of the other menus already in the top most menu above
	m_pInnerMenu->AddCascadingMenuItem("DeepestMenu", this, m_pDeepestMenu);

}

// Messages recieved from each of the menus, prints out when message is recieved
void CascadingMenuDemo::OnMaggie()
{
	ivgui()->DPrintf("Maggie selected.\n");
}

void CascadingMenuDemo::OnMarcia()
{
	ivgui()->DPrintf("Marcia selected.\n");
}

void CascadingMenuDemo::OnJohn()
{
	ivgui()->DPrintf("John selected.\n");
}

void CascadingMenuDemo::OnRed()
{
	ivgui()->DPrintf("Red selected.\n");
}

void CascadingMenuDemo::OnHomer()
{
	ivgui()->DPrintf("Homer selected.\n");
}


//-----------------------------------------------------------------------------
// Purpose: Message map
//-----------------------------------------------------------------------------
MessageMapItem_t CascadingMenuDemo::m_MessageMap[] =
{
	MAP_MESSAGE( CascadingMenuDemo, "Maggie", OnMaggie ),   // from outermenu
	MAP_MESSAGE( CascadingMenuDemo, "Homer", OnHomer ),		// from outermenu
	MAP_MESSAGE( CascadingMenuDemo, "Marcia", OnMarcia ),   // from innermenu2
	MAP_MESSAGE( CascadingMenuDemo, "John", OnJohn ),		// from innermenu2
	MAP_MESSAGE( CascadingMenuDemo, "Red", OnRed ),			// from deepest menu

};

IMPLEMENT_PANELMAP(CascadingMenuDemo, DemoPage);


Panel* CascadingMenuDemo_Create(Panel *parent)
{
	return new CascadingMenuDemo(parent, "CascadingMenuDemo");
}


