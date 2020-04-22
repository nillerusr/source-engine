//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>

#include <vgui_controls/TextEntry.h>


using namespace vgui;

//-----------------------------------------------------------------------------
// Text Entry controls are notepad-like windows that hold text. 
// In this demo we create an editable text entry window that holds multiple lines
// of text. We initialize it with some starting text and add a scroll bar to the 
// window. 
//-----------------------------------------------------------------------------
class TextEntryDemo2: public DemoPage
{
	public:
		TextEntryDemo2(Panel *parent, const char *name);
		~TextEntryDemo2();	
	private:
				
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
TextEntryDemo2::TextEntryDemo2(Panel *parent, const char *name) : DemoPage(parent, name)
{
	TextEntry *pTextEntry = new TextEntry(this, "AnotherTextEntry");

	// Position the window and make it nice and wide.
	// Make it tall enough to fit several lines of text.
	pTextEntry->SetBounds(100, 100, 200, 100);
	

	// Make this window hold multiple lines of text.
	// This will turn off horizontal scrolling, 
	// and wrap text from line to line.
	pTextEntry->SetMultiline(true);
	// When we type we want to catch the enter key and 
	// have the text entry insert a newline char.
	pTextEntry->SetCatchEnterKey(true);

	// Add a vertical scroll bar.
	pTextEntry->SetVerticalScrollbar(true);
	
	// Insert text after you have set the size and position of the window
	pTextEntry->InsertString("Some starting text and a pile of text. ");
	pTextEntry->InsertString("Some more text to make mutiple lines. ");
	pTextEntry->InsertString("Even more scrumptious, chocolatey delicious text. ");
	pTextEntry->InsertString("Enough text to get that scroll bar a-scrolling. ");
	pTextEntry->InsertString("That's it a nice number of chars.");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
TextEntryDemo2::~TextEntryDemo2()
{
}


Panel* TextEntryDemo2_Create(Panel *parent)
{
	return new TextEntryDemo2(parent, "TextEntryDemo2");
}


