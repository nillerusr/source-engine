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
// HTML controls display HTML content.
//-----------------------------------------------------------------------------
class HTMLDemo: public DemoPage
{
	public:
		HTMLDemo(Panel *parent, const char *name);
		~HTMLDemo();
		
	private:

		HTML *m_pHTML;				
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
HTMLDemo::HTMLDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	m_pHTML = new HTML(this, "AHTML");

	// Position the window and make it nice and wide, but preserve the 
	// height to one line.
	m_pHTML->SetBounds(10, 10, 500, 300);
	
	// now open a URL
	m_pHTML->OpenURL("http://www.valvesoftware.com", NULL);
//	m_pHTML->OpenURL("file:///c:/temp/WebCap.plg");
	// the URL can be any valid URL accepted by Internet Explorer, use file:///c:/... for local filesystem files :)
	
	// this call causes the control to repaint itself every 1000msec or so, to allow animated gifs to work
	// bdawson:TODO
	//m_pHTML->StartAnimate(1000);
}



//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
HTMLDemo::~HTMLDemo()
{
}


Panel* HTMLDemo_Create(Panel *parent)
{
	return new HTMLDemo(parent, "HTMLDemo");
}


