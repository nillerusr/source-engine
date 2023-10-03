//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "BackgroundMenuButton.h"

#include <KeyValues.h>
#include <vgui/IImage.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/MenuItem.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBackgroundMenuButton::CBackgroundMenuButton(vgui::Panel *parent, const char *name) : BaseClass(parent, name, "")
{
	m_pImage = NULL;
	m_pMouseOverImage = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBackgroundMenuButton::~CBackgroundMenuButton()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBackgroundMenuButton::OnCommand(const char *command)
{
	BaseClass::OnCommand(command);
}

//-----------------------------------------------------------------------------
// Purpose: Makes the button transparent
//-----------------------------------------------------------------------------
void CBackgroundMenuButton::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	// hack some colors in
	SetFgColor(Color(255, 255, 255, 255));
	SetBgColor(Color(0, 0, 0, 0));
	SetDefaultColor(Color(255, 255, 255, 255), Color(0, 0, 0, 0));
	SetArmedColor(Color(255, 255, 0, 255), Color(0, 0, 0, 0));
	SetDepressedColor(Color(255, 255, 0, 255), Color(0, 0, 0, 0));
	SetContentAlignment(Label::a_west);
	SetBorder(NULL);
	SetDefaultBorder(NULL);
	SetDepressedBorder(NULL);
	SetKeyFocusBorder(NULL);
	SetTextInset(0, 0);
	SetAlpha(0);

	/*
	// sounds disabled for this button (since it's so big now)
	SetArmedSound("UI/buttonrollover.wav");
	SetDepressedSound("UI/buttonclick.wav");
	SetReleasedSound("UI/buttonclickrelease.wav");
	*/
}
