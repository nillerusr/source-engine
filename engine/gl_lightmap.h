//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef GL_LIGHTMAP_H
#define GL_LIGHTMAP_H

#ifdef _WIN32
#pragma once
#endif
#include "surfacehandle.h"

#define	MAX_LIGHTMAPS	256

// NOTE: This is used for a hack that deals with viewmodels creating dlights 
// behind walls
#define DLIGHT_BEHIND_PLANE_DIST	-15

extern bool g_RebuildLightmaps;

extern int r_dlightactive;

// Marks dlights as visible or not visible
void R_MarkDLightVisible( int dlight );
void R_MarkDLightNotVisible( int dlight );

// Must call these at the start + end of rendering each view
void R_DLightStartView();
void R_DLightEndView();

// Can we use another dynamic light, or is it just too expensive?
bool R_CanUseVisibleDLight( int dlight );

struct msurfacelighting_t;

void R_AddDynamicLights( SurfaceHandle_t surfID, msurfacelighting_t *pLighting, const matrix3x4_t& entityToWorld );
void R_BuildLightMap( struct dlight_t *pLights, class ICallQueue *pCallQueue, SurfaceHandle_t surfID, const matrix3x4_t& entityToWorld );
void R_RedownloadAllLightmaps();
void GL_RebuildLightmaps( void );
void FASTCALL R_RenderDynamicLightmaps( dlight_t *pLights, ICallQueue *pCallQueue, SurfaceHandle_t surfID, const matrix3x4_t& entityToWorld );
int R_AddLightmapPolyChain( SurfaceHandle_t surfID );
int R_AddLightmapSurfaceChain( SurfaceHandle_t surfID );
void R_SetLightmapBlendingMode( void );

extern Vector4D blocklights[NUM_BUMP_VECTS+1][ MAX_LIGHTMAP_DIM_INCLUDING_BORDER * MAX_LIGHTMAP_DIM_INCLUDING_BORDER ];

#endif // GL_LIGHTMAP_H
