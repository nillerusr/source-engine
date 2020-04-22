//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "DemoPage.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
DemoPage::DemoPage(Panel *parent, const char *name) : PropertyPage(parent, name)
{
	SetPos(1,80);
	int wide, tall;
	GetParent()->GetSize(wide, tall);
	SetSize (wide-2, tall - 81);

	SetPaintBorderEnabled(false);
	SetVisible(false);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
DemoPage::~DemoPage()
{
}
