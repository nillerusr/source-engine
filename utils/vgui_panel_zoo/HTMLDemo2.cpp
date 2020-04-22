//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>

#include <vgui_controls/HTML.h>


using namespace vgui;

//-----------------------------------------------------------------------------
// HTML controls display HTML content, 2 side by side.
//-----------------------------------------------------------------------------
class HTMLDemo2: public DemoPage
{
	public:
		HTMLDemo2(Panel *parent, const char *name);
		~HTMLDemo2();
		
	private:

		HTML *m_pHTML1;	
		HTML *m_pHTML2;				
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
HTMLDemo2::HTMLDemo2(Panel *parent, const char *name) : DemoPage(parent, name)
{
	m_pHTML1 = new HTML(this, "AHTML1");
	m_pHTML2 = new HTML(this, "AHTML2");

	// Position the window and make it nice and wide, but preserve the 
	// height to one line.
	m_pHTML1->SetBounds(10, 10, 240, 300);

	m_pHTML2->SetBounds(20+250, 10, 240, 300);
	
	// now open a URL
	m_pHTML1->OpenURL("http://www.valvesoftware.com", NULL);
	m_pHTML2->OpenURL("http://www.valve-erc.com", NULL);
	// the URL can be any valid URL accepted by Internet Explorer, use file:///c:/... for local filesystem files :)
	
	// this call causes the control to repaint itself every 1000msec or so, to allow animated gifs to work
	// bdawson:TODO
	//m_pHTML1->StartAnimate(1000);
}



//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
HTMLDemo2::~HTMLDemo2()
{
}


Panel* HTMLDemo2_Create(Panel *parent)
{
	return new HTMLDemo2(parent, "HTMLDemo2");
}


