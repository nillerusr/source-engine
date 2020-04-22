//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>

#include <vgui_controls/TextImage.h>


using namespace vgui;

//-----------------------------------------------------------------------------
// A TextImage is an Image that handles drawing of a text string
// They are not panels.
//-----------------------------------------------------------------------------
class TextImageDemo: public DemoPage
{
	public:
		TextImageDemo(Panel *parent, const char *name);
		~TextImageDemo();

		void Paint();
		
	private:
		TextImage *m_pTextImage;				
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
TextImageDemo::TextImageDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{

	// Create a TextImage object that says "Text Image Text"
	//m_pTextImage = new TextImage("Text Image Text", GetScheme());
	m_pTextImage = new TextImage("Text Image Text");

	// Set the position
	m_pTextImage->SetPos(100, 100);

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
TextImageDemo::~TextImageDemo()
{
}

//-----------------------------------------------------------------------------
// Purpose: Paint the image on screen. TextImages are not panels, you must
//  call this method explicitly for them.
//-----------------------------------------------------------------------------
void TextImageDemo::Paint()
{
   m_pTextImage->Paint();
}


Panel* TextImageDemo_Create(Panel *parent)
{
	return new TextImageDemo(parent, "TextImageDemo");
}

