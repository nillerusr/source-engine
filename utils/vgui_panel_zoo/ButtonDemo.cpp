//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>
#include <vgui_controls/Controls.h>
#include <Keyvalues.h>
#include <vgui_controls/Button.h>


using namespace vgui;


class ButtonDemo: public DemoPage
{
	public:
		ButtonDemo(Panel *parent, const char *name);
		~ButtonDemo();

		void OnButtonClicked();
	
	private:
		Button *m_pButton;
		
		DECLARE_PANELMAP();
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ButtonDemo::ButtonDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// Create a button.
	m_pButton = new Button(this, "AButton", "ClickMe");

	// Set its position.
	m_pButton->SetPos(100, 100);

	// Install a command that will be executed when the button is pressed
	// Here we use a KeyValues command, this is mapped using the Message map
	// below to a function.
	m_pButton->SetCommand(new KeyValues ("ButtonClicked"));
	
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ButtonDemo::~ButtonDemo()
{
}

//-----------------------------------------------------------------------------
// Purpose:	 Respond to a message based action signal
//-----------------------------------------------------------------------------
void ButtonDemo::OnButtonClicked()
{
	ivgui()->DPrintf("Button was clicked.\n");
}



MessageMapItem_t ButtonDemo::m_MessageMap[] =
{
	MAP_MESSAGE( ButtonDemo, "ButtonClicked", OnButtonClicked ),   
};

IMPLEMENT_PANELMAP(ButtonDemo, DemoPage);



Panel* ButtonDemo_Create(Panel *parent)
{
	return new ButtonDemo(parent, "ButtonDemo");
}


