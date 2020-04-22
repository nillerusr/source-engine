//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds a pointer to the GC's host's interface
//
//=============================================================================
#include "stdafx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{
IGameCoordinatorHost *g_pGCHost = NULL;

IGameCoordinatorHost *GGCHost()
{
	return g_pGCHost;
}


void SetGCHost( IGameCoordinatorHost *pHost )
{
	g_pGCHost = pHost;
}

} // namespace GCSDK
