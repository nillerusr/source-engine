//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CBASE_H
#define CBASE_H
#ifdef _WIN32
#pragma once
#endif

// Shared header file
#include "basetypes.h"
#include "tier1/strtools.h"
#include "vstdlib/random.h"

#ifdef _WIN32
extern IUniformRandomStream *random;
#endif

#endif // CBASE_H
