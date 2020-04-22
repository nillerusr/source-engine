//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>
#include <Keyvalues.h>
#include <vgui_controls/Controls.h>


using namespace vgui;


class SampleRadioButtons: public DemoPage
{
	public:
		SampleRadioButtons(Panel *parent, const char *name);
		~SampleRadioButtons();
	
	private:

};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SampleRadioButtons::SampleRadioButtons(Panel *parent, const char *name) : DemoPage(parent, name)
{
	LoadControlSettings("Demo/SampleRadioButtons.res");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
SampleRadioButtons::~SampleRadioButtons()
{
}




Panel* SampleRadioButtons_Create(Panel *parent)
{
	return new SampleRadioButtons(parent, "Radio buttons");
}


