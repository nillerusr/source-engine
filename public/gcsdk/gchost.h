//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds a pointer to the GC's host's interface
//
//=============================================================================

#ifndef GCHOST_H
#define GCHOST_H
#ifdef _WIN32
#pragma once
#endif

#include "gamecoordinator/igamecoordinatorhost.h"

namespace GCSDK
{


extern IGameCoordinatorHost *GGCHost();
extern void SetGCHost( IGameCoordinatorHost *pHost );

} // namespace GCSDK

#endif // GCHOST_H