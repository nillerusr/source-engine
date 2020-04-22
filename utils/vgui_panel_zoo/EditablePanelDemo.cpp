//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "DemoPage.h"

#include "vgui/IVGui.h"
#include "vgui_controls/Controls.h"

#include "tier1/KeyValues.h"
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ComboBox.h>

using namespace vgui;

// EditablePanels are panels that can create certain vgui controls 
// by using the function createControlByName()


class EditablePanelDemo: public DemoPage
{
	public:
		EditablePanelDemo(Panel *parent, const char *name);
		~EditablePanelDemo();
	
	private:
		EditablePanel *m_pEditablePanel;
		Label *m_pSpeedLabel;
		ComboBox *m_pInternetSpeed;

};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
EditablePanelDemo::EditablePanelDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// Create an EditablePanel.
	m_pEditablePanel = new EditablePanel(this, "AnEditablePanel");

	
	// Set its position and size
	m_pEditablePanel->SetSize(400, 200);
	m_pEditablePanel->SetPos(0, 0);

	// Add a child Label panel to the EditablePanel
	m_pSpeedLabel = (Label *)(m_pEditablePanel->CreateControlByName("Label"));
	// Set its parent to our editable panel.
	m_pSpeedLabel->SetParent(m_pEditablePanel);
	// Set its Position
	m_pSpeedLabel->SetPos(20, 30);
	// Set its size
	m_pSpeedLabel->SetSize(96,20);
	// Set it not to resize with the window.
	m_pSpeedLabel->SetAutoResize(PIN_TOPLEFT, AUTORESIZE_NO, 0, 0, 0, 0 );
	// Set it visible
	m_pSpeedLabel->SetVisible(true);
	// Set it enabled
	m_pSpeedLabel->SetEnabled(true);
	// Set its tab position
	m_pSpeedLabel->SetTabPosition(0);
	// Set the text in the label
	m_pSpeedLabel->SetText("Internet &Speed");
	// Set its text alignment
	m_pSpeedLabel->SetContentAlignment(Label::a_east);
	
	// Add another child panel to the EditablePanel, this time a ComboBox.
	
	// This will be the menu items of our combo box menu.	
	// List of all the different internet speeds
	char *g_Speeds[] =
	{
		{ "Modem - 14.4k"},
		{ "Modem - 28.8k"},
		{ "Modem - 33.6k"},
		{ "Modem - 56k"},
		{ "ISDN - 112k"},
		{ "DSL > 256k"},
		{ "LAN/T1 > 1M"},		
	};

	// Create the combo box using the create function
	m_pInternetSpeed = (ComboBox *)(m_pEditablePanel->CreateControlByName("ComboBox"));
	// Set its parent to our editable panel.
	m_pInternetSpeed->SetParent(m_pEditablePanel);
	// Set its position next to the label.
	m_pInternetSpeed->SetPos(124, 30);
	// Set its size
	m_pInternetSpeed->SetSize(200, 20);
	// Set it not to resize with the window.
	m_pInternetSpeed->SetAutoResize(PIN_TOPLEFT, AUTORESIZE_NO, 0, 0, 0, 0 );
	// Set it visible
	m_pInternetSpeed->SetVisible(true);
	// Set it enabled
	m_pInternetSpeed->SetEnabled(true);
	// Set its tab position
	m_pInternetSpeed->SetTabPosition(0);
	// Set its text hidden attribute
	m_pInternetSpeed->SetTextHidden(false);
	// Set it not editable
	m_pInternetSpeed->SetEditable(false);
	// Set its maxchars to -1 since it is not editable
	//m_pInternetSpeed->SetMaximumCharCount(-1);
	// Set the number of items in the combo box menu
	m_pInternetSpeed->SetNumberOfEditLines(ARRAYSIZE(g_Speeds));
	// Set the drop down arrow button visible
	m_pInternetSpeed->SetDropdownButtonVisible(true);


	// Add menu items to this combo box.
	for (int i = 0; i < ARRAYSIZE(g_Speeds); i++)
	{
		m_pInternetSpeed->AddItem(g_Speeds[i], NULL );
	}

	// Associate our label with our combo box
	m_pSpeedLabel->SetAssociatedControl(m_pInternetSpeed);

	// Now you're saying... why bother using an EditablePanel class for this?
	// I could have just created a panel and just created each child panel on my own
	// using code like this: memberLabel =  new Label(NULL, NULL, "Label");
	// I could have even saved many lines of code by choosing nice constructor args.
	// Well the real power of editable panels lies in the use of Resource Files
	// as seen in the next example.
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
EditablePanelDemo::~EditablePanelDemo()
{
}



Panel* EditablePanelDemo_Create(Panel *parent)
{
	return new EditablePanelDemo(parent, "EditablePanelDemo");
}


