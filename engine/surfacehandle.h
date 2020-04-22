//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef SURFACEHANDLE_H
#define SURFACEHANDLE_H
#ifdef _WIN32
#pragma once
#endif

struct msurface2_t;
typedef msurface2_t *SurfaceHandle_t;
typedef msurface2_t * RESTRICT SurfaceHandleRestrict_t;
const SurfaceHandle_t SURFACE_HANDLE_INVALID = NULL;
#define IS_SURF_VALID(surfID) ( surfID != SURFACE_HANDLE_INVALID )

#endif // SURFACEHANDLE_H
