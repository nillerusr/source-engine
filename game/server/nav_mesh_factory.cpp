//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// NavMeshFactory.cpp
// Factory to create the NavMesh singleton
// Author: Michael S. Booth, June 2005

#include "cbase.h"
#include "nav_mesh.h"



// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNavMesh *NavMeshFactory( void )
{


	return new CNavMesh;
}
