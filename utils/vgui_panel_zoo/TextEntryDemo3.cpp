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
// TextEntry controls are notepad-like windows that hold text. 
// In this demo we create an NON-editable text entry window that holds multiple lines
// of text. We initialize it with some text and add a scroll bar to the 
// window. 
//-----------------------------------------------------------------------------
class TextEntryDemo3: public DemoPage
{
	public:
		TextEntryDemo3(Panel *parent, const char *name);
		~TextEntryDemo3();
		
	private:
		TextEntry *m_pTextEntry;				
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
TextEntryDemo3::TextEntryDemo3(Panel *parent, const char *name) : DemoPage(parent, name)
{
	m_pTextEntry = new TextEntry(this, "YetAnotherTextEntry");

	// Position the window and make it nice and wide.
	// Make it tall enough to fit several lines of text.
	m_pTextEntry->SetBounds(100, 100, 200, 100);
	

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
	m_pTextEntry->InsertString("That's it a nice number of chars.");

	// This Text window is not editable by the user. It will only display.
	m_pTextEntry->SetEditable(false);
	
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
TextEntryDemo3::~TextEntryDemo3()
{
}


Panel* TextEntryDemo3_Create(Panel *parent)
{
	return new TextEntryDemo3(parent, "TextEntryDemo3");
}


