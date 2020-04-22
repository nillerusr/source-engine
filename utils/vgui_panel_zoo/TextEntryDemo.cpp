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
// They have a border around them and typically hold editable text information.
// In this demo we create a very simple text entry window. It holds one
// line of text and is editable. Typing more text will fill the window with
// text and as you hit the end the text will scroll. 
// The cursor can be moved
// around with arrow keys or positioned with the mouse. Clicking and dragging
// will select text. Right clicking in
// a text edit window will open a cut/copy/paste dropdown, and the windows
// keyboard commands will work as well (ctrl-c/ctrl-v).	Some other windows
// keys work as well (home, delete, end).
// When URL's are displayed in TextEntry windows they become clickable, and
// will open a browser when clicked.
//-----------------------------------------------------------------------------
class TextEntryDemo: public DemoPage
{
	public:
		TextEntryDemo(Panel *parent, const char *name);
		~TextEntryDemo();
		
	private:
		void SetVisible(bool status);

		TextEntry *m_pTextEntry;				
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
TextEntryDemo::TextEntryDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	m_pTextEntry = new TextEntry(this, "ATextEntry");

	// Get the size of the window
	int wide, tall;
	m_pTextEntry->GetSize(wide, tall);

	// Position the window and make it nice and wide, but preserve the 
	// height to one line.
	m_pTextEntry->SetBounds(100, 100, 200, tall);
	
	// Insert text after you have set the starting 
	// size and position of the window
	m_pTextEntry->InsertString("Some starting text");

	// We want all the text in the window selected the 
	// first time the user clicks in the window.
	m_pTextEntry->SelectAllOnFirstFocus(true);

	// Note window has horizontal scrolling of text on by default.
	// You can enforce a char limit by using setMaximumCharCount()


	// A non editable textentry filled with text to test elipses:
	TextEntry *m_pTextEntry2 = new TextEntry(this, "ATextEntry");
	m_pTextEntry2->SetBounds(100, 130, 200, tall);
	m_pTextEntry2->InsertString("Some starting text longer than before for an elipsis");
	m_pTextEntry2->SetHorizontalScrolling(false);
}

void TextEntryDemo::SetVisible(bool status)
{
	// We want all the text in the window selected the 
	// first time the user clicks in the window.
	if (status)
		m_pTextEntry->SelectAllOnFirstFocus(true);;

	DemoPage::SetVisible(status);
	
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
TextEntryDemo::~TextEntryDemo()
{
}


Panel* TextEntryDemo_Create(Panel *parent)
{
	return new TextEntryDemo(parent, "TextEntryDemo");
}


