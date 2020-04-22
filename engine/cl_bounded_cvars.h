//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Provides access to cvar values that are bounded by the server.
//
//=============================================================================//

#ifndef CL_BOUNDED_CVARS_H
#define CL_BOUNDED_CVARS_H
#ifdef _WIN32
#pragma once
#endif


#include "convar_serverbounded.h"


extern ConVar_ServerBounded *cl_rate;
extern ConVar_ServerBounded *cl_cmdrate;
extern ConVar_ServerBounded *cl_updaterate;


#endif // CL_BOUNDED_CVARS_H

