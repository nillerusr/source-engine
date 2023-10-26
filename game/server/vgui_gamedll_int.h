//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================
#if !defined( VGUI_GAMEDLL_INT_H )
#define VGUI_GAMEDLL_INT_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

#include <vgui/VGUI.h>

namespace vgui
{
	class Panel;
}

bool VGui_Startup( CreateInterfaceFn appSystemFactory );
bool VGui_PostInit();
void VGui_Shutdown( void );
void VGui_CreateGlobalPanels( void );
vgui::VPANEL VGui_GetGameDLLRootPanel( void );
void VGUI_CreateGameDLLRootPanel( void );
void VGUI_DestroyGameDLLRootPanel( void );
//void VGui_PreRender();

void ShowGameDLLPanel( vgui::Panel *panel );

#endif // VGUI_GAMEDLL_INT_H
