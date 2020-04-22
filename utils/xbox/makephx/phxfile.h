//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef PHXFILE_H
#define PHXFILE_H
#ifdef _WIN32
#pragma once
#endif

#include "simplify.h"
// convert a vcollide to packed/simplified format
vcollide_t *ConvertVCollideToPHX( const vcollide_t *pCollideIn, const simplifyparams_t &params, int *pSize, bool bStoreSurfaceprops, bool bStoreSolidNames );
void DestroyPHX( vcollide_t *pCollide );

#endif // PHXFILE_H
