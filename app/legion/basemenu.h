//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Base class menus should all inherit from 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#ifndef BASEMENU_H
#define BASEMENU_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/frame.h"
#include "vgui/keycode.h"


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
class CBaseMenu : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CBaseMenu, vgui::Frame );

public:
	CBaseMenu( vgui::Panel *pParent, const char *pPanelName );
	virtual ~CBaseMenu();

	// Commands
	virtual void OnCommand( const char *pCommand );
	virtual void OnKeyCodeTyped( vgui::KeyCode code );

private:
};

#endif // BASEMENU_H

