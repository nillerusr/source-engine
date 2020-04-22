//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Stateless light computation routines 
//
//===========================================================================//

#ifndef R_STUDIOLIGHT_H
#define R_STUDIOLIGHT_H
#ifdef _WIN32
#pragma once
#endif


#include "tier0/platform.h"

#if defined( _WIN32 ) && !defined( _X360 )
#include <xmmintrin.h>
#endif


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class Vector;
class Vector4D;
class FourVectors;
struct lightpos_t;
struct LightDesc_t;


//-----------------------------------------------------------------------------
// Stateless light computation routines
//-----------------------------------------------------------------------------

// Computes the ambient term
void R_LightAmbient_4D( const Vector& normal, Vector4D* pLightBoxColor, Vector &lv );
void R_LightStrengthWorld( const Vector& vert, int lightcount, LightDesc_t* pLightDesc, lightpos_t *light );
float FASTCALL R_WorldLightDistanceFalloff( const LightDesc_t *wl, const Vector& delta );

// Copies lighting state into a buffer, returns number of lights copied
int CopyLocalLightingState( int nMaxLights, LightDesc_t *pDest, int nLightCount, const LightDesc_t *pSrc );

#if defined( _WIN32 ) && !defined( _X360 )
// SSE optimized versions
void R_LightAmbient_4D( const FourVectors& normal, Vector4D* pLightBoxColor, FourVectors &lv );
__m128 FASTCALL R_WorldLightDistanceFalloff( const LightDesc_t *wl, const FourVectors& delta );
#endif

#endif // R_STUDIOLIGHT_H
