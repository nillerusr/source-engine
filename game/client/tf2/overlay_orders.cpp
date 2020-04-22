//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $Workfile: $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "vgui_entityimagepanel.h"
#include "CommanderOverlay.h"



class COrderStatusPanel : public CEntityImagePanel
{
	DECLARE_CLASS( COrderStatusPanel, CEntityImagePanel );
public:
	COrderStatusPanel( vgui::Panel *parent, const char *panelName )
		: BaseClass( parent, "COrderStatusPanel" )
	{
	}
};


DECLARE_OVERLAY_FACTORY( COrderStatusPanel, "personal_order" );
