//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================
#include "cbase.h"
#ifdef SERVER_USES_VGUI
#include "vgui_gamedll_int.h"
#include "ienginevgui.h"
#include <vgui/isurface.h>
#include <vgui/IVGui.h>
#include <vgui/IInput.h>
#include "tier0/vprof.h"
#include <vgui_controls/Panel.h>
#include <KeyValues.h>

using namespace vgui;

#include <vgui_controls/Controls.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGUI_CreateGameDLLRootPanel( void )
{
	// Just using PANEL_ROOT in HL2 right now
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGUI_DestroyGameDLLRootPanel( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Game specific root panel
// Output : vgui::Panel
//-----------------------------------------------------------------------------
vgui::VPANEL VGui_GetGameDLLRootPanel( void )
{
	vgui::VPANEL root = enginevgui->GetPanel( PANEL_GAMEDLL );
	return root;
}



bool VGui_Startup( CreateInterfaceFn appSystemFactory )
{
	if ( !vgui::VGui_InitInterfacesList( "GAMEDLL", &appSystemFactory, 1 ) )
		return false;

	return true;
}

bool VGui_PostInit()
{
	// Create any root panels for .dll
	VGUI_CreateGameDLLRootPanel();

	// Make sure we have a panel
	VPANEL root = VGui_GetGameDLLRootPanel();
	if ( !root )
	{
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGui_CreateGlobalPanels( void )
{
}

void ShowGameDLLPanel( vgui::Panel *panel )
{
	panel->SetParent( VGui_GetGameDLLRootPanel() );

	panel->SetVisible( true );
	panel->SetEnabled( true );
	panel->MoveToFront();
	panel->InvalidateLayout();
}

void VGui_Shutdown( void )
{
	VGUI_DestroyGameDLLRootPanel();

	// Make sure anything "marked for deletion"
	//  actually gets deleted before this dll goes away
	vgui::ivgui()->RunFrame();
}

#endif // SERVER_USES_VGUI

