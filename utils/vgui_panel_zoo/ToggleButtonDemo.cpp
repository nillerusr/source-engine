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

#include <vgui_controls/ToggleButton.h>


using namespace vgui;

// Toggle buttons are a buttons that stay down when you click them.
// Clicking again toggles the button back up.

class ToggleButtonDemo: public DemoPage
{
	public:
		ToggleButtonDemo(Panel *parent, const char *name);
		~ToggleButtonDemo();

		void OnToggleButtonToggled();

	
	private:
		ToggleButton *m_pToggleButton;

		DECLARE_PANELMAP();
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ToggleButtonDemo::ToggleButtonDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// Toggle buttons are a buttons that stay down when you click them.

	// Create a Toggle button.
	m_pToggleButton = new ToggleButton(this, "AToggleButton", "Toggle me!");

	// Set its position.
	m_pToggleButton->SetPos(100, 100);

	// Size the button so the message fits nicely.
	int wide, tall;
	m_pToggleButton->GetContentSize(wide, tall);
	m_pToggleButton->SetSize(wide + Label::Content, tall + Label::Content);


	// Toggle buttons are Buttons, and can send a command when clicked.
    // Install a command to be sent when the button is toggled.
	m_pToggleButton->SetCommand(new KeyValues("Toggle"));

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ToggleButtonDemo::~ToggleButtonDemo()
{
}

//-----------------------------------------------------------------------------
// Purpose:	 Respond to a message based action signal
//-----------------------------------------------------------------------------
void ToggleButtonDemo::OnToggleButtonToggled()
{
	if (m_pToggleButton->IsDepressed())
	{
		ivgui()->DPrintf("Toggle button is down.\n");
	}
	else
	{
		ivgui()->DPrintf("Toggle button is up.\n");
	}
}



MessageMapItem_t ToggleButtonDemo::m_MessageMap[] =
{
	MAP_MESSAGE( ToggleButtonDemo, "Toggle", OnToggleButtonToggled ),
};

IMPLEMENT_PANELMAP(ToggleButtonDemo, DemoPage);



Panel* ToggleButtonDemo_Create(Panel *parent)
{
	return new ToggleButtonDemo(parent, "ToggleButtonDemo");
}


