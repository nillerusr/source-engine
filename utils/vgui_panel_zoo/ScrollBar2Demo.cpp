//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>

#include <vgui/IScheme.h>
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/Label.h>
#include <stdio.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// A ScrollBar is an class for selecting a numerical value from a range.
// E.g. in the case of a scrolling text window we use the scroll bar to select
// what line/char of text display should start at in the text window.
 
// Some terms:
//   
//	There are arrow buttons on either end of the scroll bar.
//  These move the scroll bar 'slider' or 'nob' across the space between the arrows.
//  The nob moves over a user specified 'range' of numbers.
//
//    ----------------------------------------------------
//    |	/ |..............|         |................ | \ |			
//    | \ |..............|   nob   |.................| / | 
//    ----------------------------------------------------
//
// In this demo we create a horizongal scroll bar that is not attached to anything.
// We display the current value of the slider nob next to the scroll bar.
//-----------------------------------------------------------------------------
class ScrollBar2Demo: public DemoPage
{
	public:
		ScrollBar2Demo(Panel *parent, const char *name);
		~ScrollBar2Demo();

		void OnSliderMoved();
		
	private:
		ScrollBar *m_pScrollbar;
		Label *m_pScrollValue;

		DECLARE_PANELMAP();
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ScrollBar2Demo::ScrollBar2Demo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	// A vertical slider
	m_pScrollbar = new ScrollBar (this, "AScrollbar", false);

	// Set the position of the bar
	m_pScrollbar->SetPos(100, 100);

	// Set the size of the bar
	m_pScrollbar->SetSize (200, 20);

	// Set the size of the bar nob, which is actually proportionally
	// related to how many lines of info fit into the window the
	// scroll bar is attatched to vs how many total lines there are.
	// With a size of 10, the scroll bar value will pass from 0 to 100.
	m_pScrollbar->SetRangeWindow(50);

	// Set the range of the bar, 
	// We want our range displayed to go from 0 to 100.
	// We must take size of the bar nob into account, and set the max to 110.
	m_pScrollbar->SetRange(0, 110);

	// Set how far the scroll bar slider moves 
	// when a scroll bar arrow button is pressed
	m_pScrollbar->SetButtonPressedScrollValue(5);

	// Set the starting value of the bar nob.
	// This will put the nob at the top of the vertical scroll bar.
	m_pScrollbar->SetValue(0);

	// Finally we create a little label to tell us what the current value
	// of the scroll bar is. 
	// We will update it every time the slider is moved.
	m_pScrollValue = new Label (this, "ScrollBarValue", "0");

	// Stick the label next to the scroll bar.
	m_pScrollValue->SetPos(100, 130);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
ScrollBar2Demo::~ScrollBar2Demo()
{
}

//-----------------------------------------------------------------------------
// Purpose: Respond to movement of the scroll bar nob by updating the label's
// text with the current value of the scrollbar.
//-----------------------------------------------------------------------------
void ScrollBar2Demo::OnSliderMoved()
{
	char number[6];
	sprintf (number, "%d", m_pScrollbar->GetValue());
	m_pScrollValue->SetText(number);
}

//-----------------------------------------------------------------------------
// Purpose: Message map
//-----------------------------------------------------------------------------
MessageMapItem_t ScrollBar2Demo::m_MessageMap[] =
{
	MAP_MESSAGE( ScrollBar2Demo, "ScrollBarSliderMoved", OnSliderMoved ),
};
IMPLEMENT_PANELMAP(ScrollBar2Demo, Panel);


Panel* ScrollBar2Demo_Create(Panel *parent)
{
	return new ScrollBar2Demo(parent, "ScrollBar2Demo");
}

