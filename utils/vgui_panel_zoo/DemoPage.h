//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <vgui_controls/PropertyPage.h>
#include "filesystem.h"
#include <vgui_controls/Panel.h>

#include <VGUI/IVGui.h> // for dprinf statements

using namespace vgui;

//-----------------------------------------------------------------------------
// This class contains the basic layout for every demo panel.
//-----------------------------------------------------------------------------
class DemoPage: public PropertyPage
{
	public:
		DemoPage(Panel *parent, const char *name);
		~DemoPage();
		
	private:
};