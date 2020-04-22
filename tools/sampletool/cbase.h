//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef CBASE_H
#define CBASE_H
#ifdef _WIN32
#pragma once
#endif

#ifdef _WIN32
// Silence certain warnings
#pragma warning(disable : 4244)		// int or float down-conversion
#pragma warning(disable : 4305)		// int or float data truncation
#pragma warning(disable : 4201)		// nameless struct/union
#pragma warning(disable : 4511)     // copy constructor could not be generated
#pragma warning(disable : 4675)     // resolved overload was found by argument dependent lookup
#pragma warning(disable : 4706)     // assignment within conditional expression
#endif

#ifdef _DEBUG
#define DEBUG 1
#endif

// Misc C-runtime library headers
#include <math.h>
#include <stdio.h>
#include "minmax.h"

// tier 0
#include "tier0/dbg.h"
#include "tier0/platform.h"
#include "basetypes.h"

// tier 1
#include "tier1/strtools.h"
#include "utlvector.h"
#include "mathlib/vmatrix.h"
#include "filesystem.h"

#include "tier1/ConVar.h"
#include "icvar.h"

#endif // CBASE_H
