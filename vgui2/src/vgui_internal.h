//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Wraps pointers to basic vgui interfaces
//
// $NoKeywords: $
//===========================================================================//

#ifndef VGUI_INTERNAL_H
#define VGUI_INTERNAL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include "interface.h"
#include "tier3/tier3.h"
#include "xbox/xboxstubs.h"

namespace vgui
{

bool VGui_InternalLoadInterfaces( CreateInterfaceFn *factoryList, int numFactories );

// <vgui/IInputInternal.h> header
extern class IInputInternal *g_pInput;

// <vgui/IScheme.h> header
extern class ISchemeManager *g_pScheme;

// <vgui/ISurface.h> header
extern class ISurface *g_pSurface;

// <vgui/ISystem.h> header
extern class ISystem *g_pSystem;

// <vgui/IVGui.h> header
extern class IVGui *g_pIVgui;

// <vgui/IPanel.h> header
extern class IPanel *g_pIPanel;

// methods
void vgui_strcpy(char *dst, int dstLen, const char *src);
} // namespace vgui




#endif // VGUI_INTERNAL_H
