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


class SampleCheckButtons: public DemoPage
{
	public:
		SampleCheckButtons(Panel *parent, const char *name);
		~SampleCheckButtons();
	
	private:

};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SampleCheckButtons::SampleCheckButtons(Panel *parent, const char *name) : DemoPage(parent, name)
{
	LoadControlSettings("Demo/SampleCheckButtons.res");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
SampleCheckButtons::~SampleCheckButtons()
{
}




Panel* SampleCheckButtons_Create(Panel *parent)
{
	return new SampleCheckButtons(parent, "Check buttons");
}


