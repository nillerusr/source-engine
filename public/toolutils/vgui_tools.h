//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef VGUI_TOOLS_H
#define VGUI_TOOLS_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

#include <vgui/VGUI.h>

// Every tool must expose this back to us
extern char const *GetVGuiControlsModuleName();

bool VGui_Startup( CreateInterfaceFn appSystemFactory );
bool VGui_PostInit();
void VGui_Shutdown( void );

// Must be implemented by .dll
void VGUI_CreateToolRootPanel( void );
void VGUI_DestroyToolRootPanel( void );
vgui::VPANEL VGui_GetToolRootPanel( void );

#endif // VGUI_TOOLS_H
