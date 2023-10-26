//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MOUSEMESSAGEFORWARDINGPANEL_H
#define MOUSEMESSAGEFORWARDINGPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Panel.h"

//-----------------------------------------------------------------------------
// Purpose: Invisible panel that forwards up mouse movement
//-----------------------------------------------------------------------------
class CMouseMessageForwardingPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CMouseMessageForwardingPanel, vgui::Panel );
public:
	CMouseMessageForwardingPanel( Panel *parent, const char *name );

	virtual void PerformLayout( void );
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseDoublePressed( vgui::MouseCode code );
};

#endif //MOUSEMESSAGEFORWARDINGPANEL_H