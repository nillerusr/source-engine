//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef GL_RMAIN_H
#define GL_RMAIN_H

#ifdef _WIN32
#pragma once
#endif


#include "mathlib/vector.h"
#include "mathlib/mathlib.h"

extern Frustum_t g_Frustum;

// Cull to the full screen frustum.
inline bool R_CullBox( const Vector& mins, const Vector& maxs )
{
	return R_CullBox( mins, maxs, g_Frustum );
}
inline bool R_CullBoxSkipNear( const Vector& mins, const Vector& maxs )
{
	return R_CullBoxSkipNear( mins, maxs, g_Frustum );
}

// Draw a rectangle in screenspace. The screen window is (-1,-1) to (1,1).
void R_DrawScreenRect( float left, float top, float right, float bottom );

void R_DrawPortals();
float GetScreenAspect( );
void R_CheckForLightingConfigChanges();

// Methods related to view/projection matrices
void ComputeViewMatrix( VMatrix *pViewMatrix, const Vector &origin, const QAngle &angles );

// NOTE: Projection coordinates are -1->1 in both dimensions
// NOTE: Returns actual aspect ratio used
float ComputeViewMatrices( VMatrix *pWorldToView, VMatrix *pViewToProjection, VMatrix *pWorldToProjection, const CViewSetup &viewSetup );

// NOTE: Screen coordinates go from 0->w, 0->h
void ComputeWorldToScreenMatrix( VMatrix *pWorldToScreen, const VMatrix &worldToProjection, const CViewSetup &viewSetup );

#endif // GL_RMAIN_H
