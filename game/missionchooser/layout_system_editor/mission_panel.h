//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Root panel of a layout system definition (aka mission)
//
//===============================================================================

#ifndef MISSION_PANEL_H
#define MISSION_PANEL_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"
#include "node_panel.h"

class KeyValues;
namespace vgui
{
	class Button;
}

class CMissionPanel : public CNodePanel
{
	DECLARE_CLASS_SIMPLE( CMissionPanel, CNodePanel );

public:
	CMissionPanel( Panel *pParent, const char *pName );

	virtual void PerformLayout();
	virtual void UpdateState();

protected:
	virtual void CreatePanelContents();
};

#endif // MISSION_PANEL_H