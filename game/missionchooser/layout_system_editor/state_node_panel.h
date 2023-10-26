//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Panel to edit "state" nodes in layout system definitions.
//
//===============================================================================

#ifndef STATE_NODE_PANEL_H
#define STATE_NODE_PANEL_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

#include "node_panel.h"

class CStateNodePanel : public CNodePanel
{
	DECLARE_CLASS_SIMPLE( CStateNodePanel, CNodePanel );

public:
	CStateNodePanel( Panel *pParent, const char *pName );
	
protected:
	virtual void CreatePanelContents();
	virtual void UpdateState();
};

#endif // STATE_NODE_PANEL_H