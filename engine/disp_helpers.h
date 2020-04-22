//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DISPINFO_HELPERS_H
#define DISPINFO_HELPERS_H
#ifdef _WIN32
#pragma once
#endif


#include "disp_defs.h"


// Figure out the max number of vertices and indices for a displacement
// of the specified power.
void CalcMaxNumVertsAndIndices( int power, int *nVerts, int *nIndices );


#endif // DISPINFO_HELPERS_H
