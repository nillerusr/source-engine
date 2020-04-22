//========= Copyright Valve Corporation, All rights reserved. ============//
//-------------------------------------------------------------
// File:		BorderedPanel.cpp
// Desc: 		
// Author: 		Peter Freese <peter@hiddenpath.com>
// Date: 		2009/05/20
// Copyright:	© 2009 Hidden Path Entertainment
//-------------------------------------------------------------

#include "cbase.h"
#include "bordered_panel.h"
#include "backgroundpanel.h"	// rounded border support

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void BorderedPanel::PaintBackground()
{
	int wide, tall;
	GetSize( wide, tall );

	DrawRoundedBackground( GetBgColor(), wide, tall );
	DrawRoundedBorder( GetFgColor(), wide, tall );
}

DECLARE_BUILD_FACTORY( BorderedPanel );

