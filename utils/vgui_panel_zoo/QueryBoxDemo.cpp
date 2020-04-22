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
#include <vgui_controls/QueryBox.h>


using namespace vgui;

// Query boxes are windows that pop up in response to events.
// They are useful for asking questions (like... "Are you sure you want to do this?").
// In this example we will trigger the opening of a query box when
// a button is pressed.
// Query boxes are Message boxes that have an OK and a Cancel button, 
// each button may be linked to an additional command in order to trigger an
// appropriate response.

class QueryBoxDemo: public DemoPage
{
	public:
		QueryBoxDemo(Panel *parent, const char *name);
		~QueryBoxDemo();

		void OnButtonClicked();
		void ShowQueryBox();
		void OnOK();
		void OnCancel();

	private:
		Button *m_pButton;

		DECLARE_PANELMAP();
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
QueryBoxDemo::QueryBoxDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// Create a button to trigger the message box.
	m_pButton = new Button(this, "AButton", "Click Me For A Question");

	// Size the button to its text.
	m_pButton->SizeToContents();

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
QueryBoxDemo::~QueryBoxDemo()
{
}


//-----------------------------------------------------------------------------
// Purpose:	 Respond to a message based action signal
//    Popup the query box.
//-----------------------------------------------------------------------------
void QueryBoxDemo::OnButtonClicked()
{
	ivgui()->DPrintf("Button was clicked.\n");

	// When the button is clicked we open the message box in response.
	ShowQueryBox();
}

//-----------------------------------------------------------------------------
// Purpose:	Display a query box
//-----------------------------------------------------------------------------
void QueryBoxDemo::ShowQueryBox()
{
   	// create a new message box.
	// The first arg is the name of the window and will be across the top.
	// The second arg is the text that will appear in the message box.

	QueryBox *pQuery = new QueryBox ("Message Window", "Will you pick OK or Cancel?");

	// Make ourselves the target of the button messages
	pQuery->AddActionSignalTarget(this);

	// Install the message to be sent when the ok button is clicked.
	pQuery->SetOKCommand(new KeyValues("OKClicked"));

	// Install the message to be sent when the cancel button is clicked.
	pQuery->SetCancelCommand(new KeyValues("Cancel"));

	// This command will pop up the message box and hold it there until we click
	// a button. When a button is clicked the query box object is destroyed.
	pQuery->DoModal();
}

//-----------------------------------------------------------------------------
// Purpose:	 Respond to a message based action signal
//   Respond to the OK button in the query box. 
//-----------------------------------------------------------------------------
void QueryBoxDemo::OnOK()
{
	ivgui()->DPrintf("Query received the OK.\n");
}

//-----------------------------------------------------------------------------	
// Purpose:	 Respond to a message based action signal
//   Respond to the Cancel button in the query box
//-----------------------------------------------------------------------------
void QueryBoxDemo::OnCancel()
{
	ivgui()->DPrintf("Query was canceled.\n");
}



MessageMapItem_t QueryBoxDemo::m_MessageMap[] =
{
	MAP_MESSAGE( QueryBoxDemo, "ButtonClicked", OnButtonClicked ), 
	MAP_MESSAGE( QueryBoxDemo, "OKClicked", OnOK ), 
	MAP_MESSAGE( QueryBoxDemo, "Cancel", OnCancel ), 

};

IMPLEMENT_PANELMAP(QueryBoxDemo, DemoPage);



Panel* QueryBoxDemo_Create(Panel *parent)
{
	return new QueryBoxDemo(parent, "QueryBoxDemo");
}


