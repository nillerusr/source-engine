//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef VPHYSICS_INTERNAL_H
#define VPHYSICS_INTERNAL_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/memalloc.h"

extern class IPhysics *g_PhysicsInternal;

//-----------------------------------------------------------------------------
// Memory debugging 
//-----------------------------------------------------------------------------
#if defined(_DEBUG) || defined(USE_MEM_DEBUG)
#define BEGIN_IVP_ALLOCATION()	MemAlloc_PushAllocDbgInfo("IVP: " __FILE__ , __LINE__ )
#define END_IVP_ALLOCATION()	MemAlloc_PopAllocDbgInfo()
#else
#define BEGIN_IVP_ALLOCATION()	0
#define END_IVP_ALLOCATION()	0
#endif


#endif // VPHYSICS_INTERNAL_H
