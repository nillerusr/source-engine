//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( VGUI_INT_H )
#define VGUI_INT_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

#include <vgui/VGUI.h>

namespace vgui
{
	class Panel;
}

struct vrect_t;

bool VGui_Startup( CreateInterfaceFn appSystemFactory );
void VGui_Shutdown( void );
void VGui_CreateGlobalPanels( void );
vgui::VPANEL VGui_GetClientDLLRootPanel( void );
vgui::VPANEL VGui_GetFullscreenRootVPANEL( void );
vgui::Panel *VGui_GetFullscreenRootPanel( void );
void VGUI_CreateClientDLLRootPanel( void );
void VGUI_DestroyClientDLLRootPanel( void );
void VGui_PreRender();
void VGui_PostRender();
void VGui_GetPanelList( CUtlVector< vgui::Panel * > &list );
void VGui_GetPanelBounds( int slot, int &x, int &y, int &w, int &h );
// If the engine is inset from the VGui_GetPanelBounds due to splitscreen aspect ratio fixups...
void VGui_GetEngineRenderBounds( int slot, int &x, int &y, int &w, int &h, int &insetX, int &insetY );
void VGui_GetHudBounds( int slot, int &x, int &y, int &w, int &h );
int VGUI_FindSlotForRootPanel( vgui::Panel *pRoot );
void VGui_GetTrueScreenSize( int &w, int &h );
void VGui_OnScreenSizeChanged();
bool VGui_IsSplitScreen();

void VGui_OnSplitScreenStateChanged();

#endif // VGUI_INT_H