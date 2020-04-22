//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>
#include <Keyvalues.h>
#include <vgui_controls/Controls.h>

#include <vgui_controls/RadioButton.h>


using namespace vgui;


class RadioButtonDemo: public DemoPage
{
	public:
		RadioButtonDemo(Panel *parent, const char *name);
		~RadioButtonDemo();

		void OnRadioButtonHit();

	
	private:
		RadioButton *m_pRadioButton1;
		RadioButton *m_pRadioButton2;

		DECLARE_PANELMAP();
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
RadioButtonDemo::RadioButtonDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// Radio buttons are a little dot circle with a label attached.
	// You can have only one radio button selected at a time.
	// Other radio buttons will become deselected.

	// Create a radio button.
	m_pRadioButton1 = new RadioButton(this, "ARadioButton", "ClickMe");

	// Set its position.
	m_pRadioButton1->SetPos(100, 100);

	// A little label for our button
	m_pRadioButton1->SetText("Click the radio button!");

	// Size the label so the message fits nicely.
	m_pRadioButton1->SizeToContents();

	// Radio buttons are Buttons, and can send a command when clicked.
    // Install a command to be sent when the box is checked or unchecked
	m_pRadioButton1->SetCommand(new KeyValues("Radio1"));

	// Radio buttons are grouped together by tab position sub tab position
	// determines the order to move through the buttons when arrow keys are hit
	m_pRadioButton1->SetSubTabPosition(0);


	// Create another radio button.
	m_pRadioButton2 = new RadioButton(this, "AnotherRadioButton", "ClickMe");

	// Set its position.
	m_pRadioButton2->SetPos(100, 120);

	// A little label for our button
	m_pRadioButton2->SetText("Click the other radio button!");

	// Size the label so the message fits nicely.
	m_pRadioButton2->SizeToContents();

	// Start the button off checked. Its unchecked by default.
	m_pRadioButton2->SetSelected(true);

	m_pRadioButton1->SetSubTabPosition(1);

    // Don't install a command to be sent when the box is checked or unchecked,
	// Because all buttons respons when a new one is checked (they uncheck themselves if checked)
	// In this case when a button is selected a RadioButtonChecked message gets sent

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
RadioButtonDemo::~RadioButtonDemo()
{
}

//-----------------------------------------------------------------------------
// Purpose:	 Respond to a message based action signal
//-----------------------------------------------------------------------------
void RadioButtonDemo::OnRadioButtonHit()
{
   	if (m_pRadioButton1->IsSelected())
	{
		ivgui()->DPrintf("Radio button one is checked.\n"); 
	}
	else
	{
		ivgui()->DPrintf("Radio button one is unchecked.\n"); 
	}
	
	if (m_pRadioButton2->IsSelected())
	{
		ivgui()->DPrintf("Radio button two is checked.\n"); 
	}
	else
	{
		ivgui()->DPrintf("Radio button two is unchecked.\n"); 
	}
}



MessageMapItem_t RadioButtonDemo::m_MessageMap[] =
{
	MAP_MESSAGE( RadioButtonDemo, "RadioButtonChecked", OnRadioButtonHit ),
};

IMPLEMENT_PANELMAP(RadioButtonDemo, DemoPage);



Panel* RadioButtonDemo_Create(Panel *parent)
{
	return new RadioButtonDemo(parent, "RadioButtonDemo");
}


