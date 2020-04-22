//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include "vgui/IVGui.h"
#include "tier1/KeyValues.h"
#include <vgui_controls/Label.h>


using namespace vgui;

//-----------------------------------------------------------------------------
// A Label is a panel class to handle the display of images and text strings.
// Here we demonstrate a simple text only label.
//-----------------------------------------------------------------------------

class LabelDemo: public DemoPage
{
	public:
		LabelDemo(Panel *parent, const char *name);
		~LabelDemo();
		
	private:
		Label *m_pLabel;				
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
LabelDemo::LabelDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	m_pLabel = new Label(this, "ALabel", "LabelText");
	m_pLabel->SetPos(100, 100);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
LabelDemo::~LabelDemo()
{
}


Panel* LabelDemo_Create(Panel *parent)
{
	return new LabelDemo(parent, "LabelDemo");
}


