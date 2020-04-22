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
#include <vgui_controls/MessageBox.h>


using namespace vgui;

// Message boxes are windows that pop up in response to events.
// They are particularly useful for error messages.
// In this example we will trigger the opening of a message box when
// a button is pressed.

class MessageBoxDemo: public DemoPage
{
	public:
		MessageBoxDemo(Panel *parent, const char *name);
		~MessageBoxDemo();

		void OnButtonClicked();
		void ShowMessageBox();
	private:
		Button *m_pButton;

		DECLARE_PANELMAP();
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
MessageBoxDemo::MessageBoxDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// Create a button to trigger the message box.
	m_pButton = new Button(this, "AButton", "Click Me For A Message");

	// Size the button to its text.
	int wide, tall;
	m_pButton->GetContentSize(wide, tall);
	m_pButton->SetSize(wide + Label::Content, tall + Label::Content);

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
MessageBoxDemo::~MessageBoxDemo()
{
}


//-----------------------------------------------------------------------------
// Purpose:	 Respond to a message based action signal
//  
//-----------------------------------------------------------------------------
void MessageBoxDemo::OnButtonClicked()
{
	ivgui()->DPrintf("Button was clicked.\n");

	// When the button is clicked we open the message box in response.
	ShowMessageBox();
}

//-----------------------------------------------------------------------------
// Purpose:	Display a message box
//-----------------------------------------------------------------------------

void MessageBoxDemo::ShowMessageBox()
{
   	// create a new message box.
	// The first arg is the name of the window and will be across the top.
	// The second arg is the text that will appear in the message box.
	// The third arg is if the box starts minimized (yes since we want the button to open it)
	// The fourth arg is a parent window arg.
	MessageBox *pMessage = new MessageBox ("Message Window", "Here is some message box text\n You must click OK to continue.", NULL);


	// This command will pop up the message box and hold it there until we click
	// the OK button. When the OK button is clicked the message box object is destroyed.
	pMessage->DoModal();
}





MessageMapItem_t MessageBoxDemo::m_MessageMap[] =
{
	MAP_MESSAGE( MessageBoxDemo, "ButtonClicked", OnButtonClicked ), 
};

IMPLEMENT_PANELMAP(MessageBoxDemo, DemoPage);



Panel* MessageBoxDemo_Create(Panel *parent)
{
	return new MessageBoxDemo(parent, "MessageBoxDemo");
}


