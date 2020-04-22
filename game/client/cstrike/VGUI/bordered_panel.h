//========= Copyright Valve Corporation, All rights reserved. ============//
//-------------------------------------------------------------
// File:		bordered_panel.h
// Desc: 		
// Author: 		Peter Freese <peter@hiddenpath.com>
// Date: 		2009/05/20
// Copyright:	© 2009 Hidden Path Entertainment
//-------------------------------------------------------------

#ifndef INCLUDED_BorderedPanel
#define INCLUDED_BorderedPanel
#pragma once

#include <vgui_controls/EditablePanel.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Editable panel with a forced rounded/outlined border
//-----------------------------------------------------------------------------
class BorderedPanel : public EditablePanel
{
public:
	DECLARE_CLASS_SIMPLE( BorderedPanel, EditablePanel );

	BorderedPanel( Panel *parent, const char *name ) : 
	EditablePanel( parent, name )
	{
	}

	void PaintBackground();
};


#endif // INCLUDED_BorderedPanel
