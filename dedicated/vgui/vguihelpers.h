//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef VGUIHELPERS_H
#define VGUIHELPERS_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"

int StartVGUI( CreateInterfaceFn dedicatedFactory );
void StopVGUI();
void RunVGUIFrame();
bool VGUIIsRunning();
bool VGUIIsStopping();
bool VGUIIsInConfig();
void VGUIFinishedConfig();
void VGUIPrintf( const char *msg );

#endif // VGUIHELPERS_H

