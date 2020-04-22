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

#include <vgui_controls/CheckButton.h>


using namespace vgui;


class CheckButtonDemo: public DemoPage
{
	public:
		CheckButtonDemo(Panel *parent, const char *name);
		~CheckButtonDemo();

		void OnCheckButton1Checked();
		void OnCheckButton2Checked();

	
	private:
		CheckButton *m_pCheckButton1;
		CheckButton *m_pCheckButton2;

		DECLARE_PANELMAP();
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CheckButtonDemo::CheckButtonDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	SetBorder(NULL);

	// Check buttons are a little checkable box with a label attached.
	// You can have as many check buttons checked at one time as you want.

	// Create a check button.
	m_pCheckButton1 = new CheckButton(this, "ACheckButton", "ClickMe");

	// Set its position.
	m_pCheckButton1->SetPos(100, 100);

	// A little label for our button
	m_pCheckButton1->SetText("Click the check button!");

	// Size the label so the message fits nicely.
	m_pCheckButton1->SizeToContents();

	// Start the button off checked. Its unchecked by default.
	m_pCheckButton1->SetSelected(true);

	// Check buttons are Buttons, and can send a command when clicked.
    // Install a command to be sent when the box is checked or unchecked
	m_pCheckButton1->SetCommand(new KeyValues("Check1"));



	// Create another check button.
	m_pCheckButton2 = new CheckButton(this, "AnotherCheckButton", "ClickMe");

	// Set its position.
	m_pCheckButton2->SetPos(100, 120);

	// A little label for our button
	m_pCheckButton2->SetText("Click the other check button!");

	// Size the label so the message fits nicely.
	m_pCheckButton2->SizeToContents();

    // Install a command to be sent when the box is checked or unchecked
	m_pCheckButton2->SetCommand(new KeyValues("Check2"));

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CheckButtonDemo::~CheckButtonDemo()
{
}

//-----------------------------------------------------------------------------
// Purpose:	 Respond to a message based action signal
//-----------------------------------------------------------------------------
void CheckButtonDemo::OnCheckButton1Checked()
{
	if (m_pCheckButton1->IsSelected())
	{
		ivgui()->DPrintf("Check box one is checked.\n"); 
	}
	else
	{
		ivgui()->DPrintf("Check box one is unchecked.\n"); 
	}

}

//-----------------------------------------------------------------------------
// Purpose:	 Respond to a message based action signal
//-----------------------------------------------------------------------------
void CheckButtonDemo::OnCheckButton2Checked()
{
	if (m_pCheckButton2->IsSelected())
	{
		ivgui()->DPrintf("Check box two is checked.\n"); 
	}
	else
	{
		ivgui()->DPrintf("Check box two is unchecked.\n"); 
	}
}




MessageMapItem_t CheckButtonDemo::m_MessageMap[] =
{
	MAP_MESSAGE( CheckButtonDemo, "Check1", OnCheckButton1Checked ),
	MAP_MESSAGE( CheckButtonDemo, "Check2", OnCheckButton2Checked ),
};

IMPLEMENT_PANELMAP(CheckButtonDemo, DemoPage);



Panel* CheckButtonDemo_Create(Panel *parent)
{
	return new CheckButtonDemo(parent, "CheckButtonDemo");
}


