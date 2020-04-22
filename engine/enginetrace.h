//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef ENGINETRACE_H
#define ENGINETRACE_H

#ifdef _WIN32
#pragma once
#endif


#include "engine/IEngineTrace.h"

extern IEngineTrace *g_pEngineTraceServer;
extern IEngineTrace *g_pEngineTraceClient;


//-----------------------------------------------------------------------------
// Debugging code to render all ray casts since the last time this call was made
//-----------------------------------------------------------------------------
void EngineTraceRenderRayCasts();


#endif // ENGINETRACE_H
