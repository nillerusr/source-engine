//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "DemoPage.h"

#include <VGUI/IVGui.h>
#include <KeyValues.h>
#include <vgui_controls/ToggleButton.h>
#include <vgui_controls/Tooltip.h>

using namespace vgui;


class TooltipsDemo: public DemoPage
{
	public:
		TooltipsDemo(Panel *parent, const char *name);
		~TooltipsDemo();
	
	private:

};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
TooltipsDemo::TooltipsDemo(Panel *parent, const char *name) : DemoPage(parent, name)
{
	ToggleButton *pButton = new ToggleButton (this, "RadioDesc5", "");
	pButton->GetTooltip()->SetTooltipFormatToSingleLine();

	LoadControlSettings("Demo/SampleToolTips.res");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
TooltipsDemo::~TooltipsDemo()
{
}




Panel* TooltipsDemo_Create(Panel *parent)
{
	return new TooltipsDemo(parent, "TooltipsDemo");
}


