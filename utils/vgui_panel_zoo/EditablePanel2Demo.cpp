//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include "tier1/KeyValues.h"
#include <vgui_controls/ComboBox.h>

using namespace vgui;


class EditablePanel2Demo: public DemoPage
{
	public:
		EditablePanel2Demo(Panel *parent, const char *name);
		~EditablePanel2Demo();
	
	private:
		ComboBox *m_pInternetSpeed;	
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
EditablePanel2Demo::EditablePanel2Demo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// Editable panels are able to have their layout described by external
	// resource files. All Frames belong to the Editable Panel class.
	// The class EditablePanel2Demo is a PropertyPage class, and PropertyPage
	// is an EditablePanel, so we can load in a resource file for this class.


	// Load the vgui controls from a resource file.
	// The resource file looks like this:
	/* EDITABLE PANEL DEMO RESOURCE FILE LAYOUT
	"EditablePanelDemo"
	{
		"SpeedLabel"
		{
			"ControlName"		"Label"
				"fieldName"		"SpeedLabel"
				"xpos"		"20"
				"ypos"		"30"
				"wide"		"96"
				"tall"		"20"
				"autoResize"		"0"
				"pinCorner"		"0"
				"visible"		"1"
				"enabled"		"1"
				"tabPosition"		"0"
				"labelText"		"Internet &Speed"
				"textAlignment"		"east"
				"associate"		"InternetSpeed"
		}
		"InternetSpeed"
		{
			"ControlName"		"ComboBox"
				"fieldName"		"InternetSpeed"
				"xpos"		"124"
				"ypos"		"30"
				"wide"		"200"
				"tall"		"20"
				"autoResize"		"0"
				"pinCorner"		"0"
				"visible"		"1"
				"enabled"		"1"
				"tabPosition"		"0"
				"textHidden"		"0"
				"editable"		"0"
				"maxchars"		"-1"
		}
	}
	*/

	// VGUI control panel resource values are grouped by a panel name, here 
	// we have 2 panels, one called "SpeedLabel" and one called
	// "Internet Speed".
	// Each control has a set of keyValues that describe attributes of
	// the panel. SpeedLabel's control name is Label, this is a vgui Label class.
	// Its 'xpos' and 'ypos' tell is position in the parent window. 
	// Its 'wide' and 'tall' tell its size.
	// 'AutoResize' is false meaning this panel will not grow or shrink in size
	// response to panel resizing.
	// More panel attributes follow, until we come to 'labelText'.
	// This is the text that will appear in the label. The text is
	// "Internet Speed" and the ampersand (&) in front of the 'S' indicates
	// that S is a hotkey in the label name.
	// 'Associate' associates this label with another vgui control that will
	// gain focus when the hotkey is hit. The associated control is called
	// "InternetSpeed" and this panel name happens to be the very next panel in the list.
	// Its 'ControlName' tells us it is a ComboBox. Hitting the 'S' hotkey will trigger
	// the combo box to gain focus. Note that the combo box does not currently 
	// have any items within it.


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

	// Create a combo box with the name "InternetSpeed"
	// The parameters of the resource file will be applied to this
	// combo box.
	m_pInternetSpeed = new ComboBox(this, "InternetSpeed", ARRAYSIZE(g_Speeds), false);

	// Add menu items to this combo box.
	for (int i = 0; i < ARRAYSIZE(g_Speeds); i++)
	{
		m_pInternetSpeed->AddItem(g_Speeds[i], NULL );
	}

	// Load the resource file settings into our panel.
	LoadControlSettings("Demo/EditablePanelDemo.res");

	// When the settings are applied (in applySettings()) our panels gain the 
	// additional layout specified by the resource keyValues.

	// There, we are done, we have created one control from "thin air" (the Label)
	// and only had to specify the menu items of the other control (the ComboBox).
	// All the rest of the layout was done using the EditablePanel's smarts and
	// a resource file.

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
EditablePanel2Demo::~EditablePanel2Demo()
{
}



Panel* EditablePanel2Demo_Create(Panel *parent)
{
	return new EditablePanel2Demo(parent, "EditablePanel2Demo");
}


