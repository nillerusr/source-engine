//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "MouseMessageForwardingPanel.h"
#include "KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

CMouseMessageForwardingPanel::CMouseMessageForwardingPanel( Panel *parent, const char *name ) : BaseClass( parent, name )
{
	// don't draw an
	SetPaintEnabled(false);
	SetPaintBackgroundEnabled(false);
	SetPaintBorderEnabled(false);
}

void CMouseMessageForwardingPanel::PerformLayout()
{
	// fill out the whole area
	int w, t;
	GetParent()->GetSize(w, t);
	SetBounds(0, 0, w, t);
}

void CMouseMessageForwardingPanel::OnMousePressed( vgui::MouseCode code )
{
	CallParentFunction( new KeyValues("MousePressed", "code", code) );
}

void CMouseMessageForwardingPanel::OnMouseDoublePressed( vgui::MouseCode code )
{
	CallParentFunction( new KeyValues("MouseDoublePressed", "code", code) );
}