//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Export functions from l_studio.cpp
//
// $NoKeywords: $
//===========================================================================//

#ifndef L_STUDIO_H
#define L_STUDIO_H

#ifdef _WIN32
#pragma once
#endif

#include "engine/ivmodelrender.h"
#include "datacache/imdlcache.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct LightDesc_t;
class IStudioRender;
struct vcollide_t;

extern IStudioRender *g_pStudioRender;

void UpdateStudioRenderConfig( void );
void InitStudioRender( void );
void ShutdownStudioRender( void );
unsigned short& FirstShadowOnModelInstance( ModelInstanceHandle_t handle );

//-----------------------------------------------------------------------------
// Converts world lights to materialsystem lights
//-----------------------------------------------------------------------------
bool WorldLightToMaterialLight( dworldlight_t* worldlight, LightDesc_t& light );

//-----------------------------------------------------------------------------
// Computes the center of the studio model for illumination purposes
//-----------------------------------------------------------------------------
void R_ComputeLightingOrigin( IClientRenderable *pRenderable, studiohdr_t* pStudioHdr, const matrix3x4_t &matrix, Vector& center );

void DrawSavedModelDebugOverlays( void );

// Used to force the value of r_rootlod depending on if sv_cheats is 0 and if we're on a server.
bool CheckVarRange_r_rootlod();
bool CheckVarRange_r_lod();

extern ConVar r_drawmodelstatsoverlay;
extern ConVar r_drawmodelstatsoverlaydistance;
extern ConVar r_lod;
extern ConVar r_drawmodellightorigin;

#endif

