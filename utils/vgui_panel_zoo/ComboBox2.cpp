//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "DemoPage.h"

#include <VGUI/IVGui.h>
#include "vgui_controls/Controls.h"

#include "tier1/KeyValues.h"
#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>


using namespace vgui;

// Combo boxes are boxes that display text and have a menu attached.
// Selecting an item from the menu changes the displayed text in the box.

class ComboBox2Demo: public DemoPage
{
	DECLARE_CLASS_SIMPLE( ComboBox2Demo, DemoPage );
	public:
		ComboBox2Demo(Panel *parent, const char *name);
		~ComboBox2Demo();

		void OnButtonClicked();
		void AddItemToComboBoxMenu();

	private:
		Button *m_pButton;
		ComboBox *m_pComboBox;

		DECLARE_PANELMAP();

};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ComboBox2Demo::ComboBox2Demo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// Create a new combo box.
	// The first arg is the parent, the second the name
	// The third arg is the number of items that will be in the menu
	// In settin this arg to 4, if we add more than 4 items to the menu
	// a scroll bar will be enabled.
	// The fourth arg is if the box is editable or not, this time we set it to true.
	m_pComboBox = new ComboBox(this, "Directions", 6, true);

	// Position the box.
	m_pComboBox->SetPos(100, 100);

	// Set the width of the Combo box so any element selected will display nicely.
	m_pComboBox->SetWide(150);

	// Add some text selections to the menu list
	m_pComboBox->AddItem("Right", NULL );
	m_pComboBox->AddItem("Left", NULL );
	m_pComboBox->AddItem("Up", NULL );
	m_pComboBox->AddItem("Down", NULL );
	m_pComboBox->AddItem("Forward", NULL );
	m_pComboBox->AddItem("Backward", NULL );
	m_pComboBox->AddItem("Backward and really long", NULL );


	// Create a button. Clicking this button will send a command back to us.
	// In response to this command we will get the contents of the combo box
	// and add it to the list of items in the list.
	m_pButton = new Button (this, "AButton", "Click to add an item name to the menu");

	m_pButton->SizeToContents();
	m_pButton->SetPos(270, 100);

	// Install a command that will be executed when the button is pressed
	m_pButton->SetCommand(new KeyValues ("ButtonClicked"));

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ComboBox2Demo::~ComboBox2Demo()
{
}

//-----------------------------------------------------------------------------
// Purpose:	 Respond to a message based action signal
//-----------------------------------------------------------------------------
void ComboBox2Demo::OnButtonClicked()
{
	ivgui()->DPrintf("Button was clicked.\n");

	// Lets check out what's in the combo box and add it to the menu.
	AddItemToComboBoxMenu();
}

void ComboBox2Demo::AddItemToComboBoxMenu()
{
	char boxText[128];

	// Get the text from the combo box 
	m_pComboBox->GetText(boxText, sizeof( boxText ) );

	// We won't add empty text strings to the menu.
	if (!strcmp(boxText, ""))
		return;

	// Add this item to the combo box menu
	// If you wanted to check for uniqueness you would have to keep a 
	// list of the items in the menu and check it.
	m_pComboBox->AddItem(boxText, NULL);

	// Reset the combo box to empty
	m_pComboBox->SetText("");
}

MessageMapItem_t ComboBox2Demo::m_MessageMap[] =
{
	MAP_MESSAGE( ComboBox2Demo, "ButtonClicked", OnButtonClicked ),   
};

IMPLEMENT_PANELMAP(ComboBox2Demo, BaseClass);




Panel* ComboBox2Demo_Create(Panel *parent)
{
	return new ComboBox2Demo(parent, "ComboBox2Demo");
}


