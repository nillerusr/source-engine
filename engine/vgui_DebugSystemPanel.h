//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef VGUI_DEBUGSYSTEMPANEL_H
#define VGUI_DEBUGSYSTEMPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui_controls/PHandle.h>

class CDebugMenuButton;
class CDebugOptionsPanel;

//-----------------------------------------------------------------------------
// Purpose: A simple panel to contain a debug menu button w/ cascading menus
//-----------------------------------------------------------------------------
class CDebugSystemPanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;
public:

	CDebugSystemPanel( vgui::Panel *parent, char const *panelName );

	// Trap visibility so that we can force the cursor on
	virtual void SetVisible( bool state );
	virtual void OnCommand( char const *command );

private:
	CDebugMenuButton	*m_pDebugMenu;

	vgui::DHANDLE< CDebugOptionsPanel >	m_hDebugOptions;
};

#endif // VGUI_DEBUGSYSTEMPANEL_H
