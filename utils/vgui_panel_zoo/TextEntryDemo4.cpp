//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>

#include <vgui_controls/TextEntry.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Controls.h>
#include <stdio.h>

using namespace vgui;

static const int TIMEOUT = 1000; // 1 second timeout

//-----------------------------------------------------------------------------
// Text Entry controls are notepad-like windows that hold text. 
// In this demo we create a NON-editable text entry window that holds multiple lines
// of text. We initialize it with some starting text and add a scroll bar to the 
// window.
// Then we use a tick function to add some more text to the window every one second.
// As the window fills with text, the window scrolls vertically. 
// The scroll bar will appear after a few lines and the scroll bar
// slider will shrink as even more text is added.
//-----------------------------------------------------------------------------
class TextEntryDemo4: public DemoPage
{
	public:
		TextEntryDemo4(Panel *parent, const char *name);
		~TextEntryDemo4();

		void SetVisible(bool state);
		void OnTick();
		
	private:
		TextEntry *m_pTextEntry;
		int m_iTimeoutTime;

};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
TextEntryDemo4::TextEntryDemo4(Panel *parent, const char *name) : DemoPage(parent, name)
{
	m_pTextEntry = new TextEntry(this, "FancyTextEntry");

	// Position the window and make it nice and wide.
	// Make it tall enough to fit several lines of text.
	m_pTextEntry->SetBounds(100, 100, 400, 200);
	

	// Make this window hold multiple lines of text.
	// This will turn off horizontal scrolling, 
	// and wrap text from line to line.
	m_pTextEntry->SetMultiline(true);

	// Add a vertical scroll bar.
	m_pTextEntry->SetVerticalScrollbar(true);
	
	// Insert text after you have set the size and position of the window
	m_pTextEntry->InsertString("Some starting text and a pile of text. ");
	m_pTextEntry->InsertString("Some more text to make mutiple lines. ");
	m_pTextEntry->InsertString("Even more scrumptious, chocolatey delicious text. ");
	m_pTextEntry->InsertString("Enough text to get that scroll bar a-scrolling. ");
	m_pTextEntry->InsertString("That's it a nice number of chars.\n");

	// This Text window is not editable by the user. It will only display.
	m_pTextEntry->SetEditable(false);
	
	// This makes panel receive a 'Tick' message every frame 
	// (~50ms, depending on sleep times/framerate)
	// Panel is automatically removed from tick signal list when it's deleted
	ivgui()->AddTickSignal(this->GetVPanel());

	m_iTimeoutTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
TextEntryDemo4::~TextEntryDemo4()
{
}


//-----------------------------------------------------------------------------
// Purpose: When the page is shown, initialize the time
//-----------------------------------------------------------------------------
void TextEntryDemo4::SetVisible(bool state)
{
	if (state)
	{
		// Set the timeout time.
		m_iTimeoutTime = system()->GetTimeMillis() + TIMEOUT;

	}
	DemoPage::SetVisible(state);
}


//-----------------------------------------------------------------------------
// Purpose: Adds lines to the text entry every second.
//-----------------------------------------------------------------------------
void TextEntryDemo4::OnTick()
{
	if (m_iTimeoutTime)
	{
		int currentTime = system()->GetTimeMillis();
		
		// Check for timeout
		if (currentTime > m_iTimeoutTime)
		{	
			char buf[125];
			sprintf (buf, "Additional Text %d\n", m_iTimeoutTime);

			// Move to the end of the history before we add some new text.
			// Its important to call this and explicitly move to the
			// correct position in case someone clicked in
			// the window (this moves the cursor)
			// If you comment out this line and rerun you will see
			// that if you click in the text window additional
			// text will be added where you clicked.
			m_pTextEntry->GotoTextEnd();

			// Add some text to the text entry window
			m_pTextEntry->InsertString(buf);

			// Timed out, make a new timeout time
			m_iTimeoutTime = system()->GetTimeMillis() + TIMEOUT;
		}
	}
}



Panel* TextEntryDemo4_Create(Panel *parent)
{
	return new TextEntryDemo4(parent, "TextEntryDemo4");
}


