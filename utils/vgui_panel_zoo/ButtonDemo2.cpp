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


class ButtonDemo2: public DemoPage
{
	public:
		ButtonDemo2(Panel *parent, const char *name);
		~ButtonDemo2();

		void OnCommand(const char *command);
	
	private:
		Button *m_pButton;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ButtonDemo2::ButtonDemo2(Panel *parent, const char *name) : DemoPage(parent, name)
{
	SetPos(0,80);
	int wide, tall;
	GetParent()->GetSize(wide, tall);
	SetSize (wide, tall - 80);

	// Create a button.
	m_pButton = new Button(this, "AButton", "ClickMe");

	// Set its position.
	m_pButton->SetPos(100, 100);

	// Install a command that will be executed when the button is pressed
	// Here we use a string command. Panels recieve string commands through
	// a command message map, already implemented in the Panel class.
	// the onCommand function parses the command string
	// and takes the appropriate action.
	m_pButton->SetCommand("ButtonClicked");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ButtonDemo2::~ButtonDemo2()
{
}

//-----------------------------------------------------------------------------
// Purpose:	 Respond to a message based action signal
//-----------------------------------------------------------------------------
void ButtonDemo2::OnCommand(const char *command)
{
	if (!strcmp(command, "ButtonClicked") )
	{
		ivgui()->DPrintf("Button was clicked.\n");
	}
}



Panel* ButtonDemo2_Create(Panel *parent)
{
	return new ButtonDemo2(parent, "ButtonDemo2");
}


