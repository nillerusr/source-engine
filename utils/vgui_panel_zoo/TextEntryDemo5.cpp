//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>

#include <vgui_controls/TextEntry.h>
#include <vgui/KeyCode.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Text Entry controls are notepad-like windows that hold text. 
// In this demo we create an editable text entry window that holds multiple lines
// of text. We initialize it with some starting text.
// We override the enter key to clear the text. To add a newline manually you can
// type ctrl-enter
//-----------------------------------------------------------------------------
class TextEntryDemo5: public DemoPage
{
	public:
		TextEntryDemo5(Panel *parent, const char *name);
		~TextEntryDemo5();	
	private:

		void OnKeyCodeTyped(KeyCode code);

		TextEntry *m_pTextEntry;
				
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
TextEntryDemo5::TextEntryDemo5(Panel *parent, const char *name) : DemoPage(parent, name)
{
	m_pTextEntry = new TextEntry(this, "AnotherTextEntry");

	// Position the window and make it nice and wide.
	// Make it tall enough to fit several lines of text.
	m_pTextEntry->SetBounds(100, 100, 200, 100);
	

	// Make this window hold multiple lines of text.
	// This will turn off horizontal scrolling, 
	// and wrap text from line to line.
	m_pTextEntry->SetMultiline(true);
	
	// Insert text after you have set the size and position of the window
	m_pTextEntry->InsertString("Some starting text and a pile of text. ");
	m_pTextEntry->InsertString("Some more text to make mutiple lines. ");
	m_pTextEntry->InsertString("Even more scrumptious, chocolatey delicious text. ");
	m_pTextEntry->InsertString("Enough text to get that scroll bar a-scrolling. ");
	m_pTextEntry->InsertString("That's it a nice number of chars.");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
TextEntryDemo5::~TextEntryDemo5()
{
}

//-----------------------------------------------------------------------------
// Purpose: When the enter key is pressed we clear the textentry.
//  To add a newline use ctrl-return.
//-----------------------------------------------------------------------------
void TextEntryDemo5::OnKeyCodeTyped(KeyCode code)
{
	if (code == KEY_ENTER)
	{
		m_pTextEntry->SetText("");
	}

	DemoPage::OnKeyCodeTyped(code);
}

Panel* TextEntryDemo5_Create(Panel *parent)
{
	return new TextEntryDemo5(parent, "TextEntryDemo5");
}


