//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#ifndef GL_SHADER_H
#define GL_SHADER_H

#ifdef _WIN32
#pragma once
#endif


void Shader_BeginRendering ();
bool Shader_Connect( bool bSetProxyFactory );
void Shader_Disconnect();
void Shader_SwapBuffers();

#include "mathlib/vector.h"
#include "convar.h"

extern	Vector		modelorg;
extern VMatrix g_BrushToWorldMatrix;

//
// screen size info
//
class	IMaterial;
extern	IMaterial*	g_materialEmpty;

extern	IMaterial*	g_materialWireframe;

extern	IMaterial*	g_materialTranslucentSingleColor;
extern	IMaterial*	g_materialTranslucentVertexColor;

extern	IMaterial*	g_materialWorldWireframe;
extern	IMaterial*	g_materialWorldWireframeZBuffer;
extern	IMaterial*	g_materialBrushWireframe;
extern	IMaterial*	g_materialDecalWireframe;
extern	IMaterial*	g_materialDebugLightmap;
extern	IMaterial*	g_materialDebugLightmapZBuffer;
extern	IMaterial*	g_materialDebugLuxels;
extern	IMaterial*	g_materialLeafVisWireframe;
extern	IMaterial*	g_pMaterialWireframeVertexColor;
extern	IMaterial*	g_pMaterialWireframeVertexColorIgnoreZ;
extern	IMaterial*  g_pMaterialLightSprite;
extern	IMaterial*  g_pMaterialShadowBuild;
extern	IMaterial*  g_pMaterialMRMWireframe;
extern	IMaterial*  g_pMaterialWriteZ;
extern	IMaterial*	g_pMaterialWaterDuDv;
extern	IMaterial*	g_pMaterialWaterFirstPass;
extern	IMaterial*	g_pMaterialWaterSecondPass;
extern	IMaterial*	g_pMaterialAmbientCube;
extern	IMaterial*	g_pMaterialDebugFlat;
extern	IMaterial*	g_pMaterialDepthWrite[2][2];
extern	IMaterial*	g_pMaterialSSAODepthWrite[2][2];

extern	ConVar	r_norefresh;
extern  ConVar	r_lightmapcolorscale;
extern	ConVar	r_decals;
extern	ConVar	mp_decals;
extern	ConVar	r_lightmap;
extern	ConVar	r_lightstyle;
extern	ConVar	r_dynamic;
extern  ConVar	r_unloadlightmaps;

extern  ConVar  r_lod_noupdate;

extern	ConVar	mat_fullbright;
extern	ConVar	mat_drawflat;
extern	ConVar	mat_reversedepth;
extern  ConVar	mat_norendering;

#endif // GL_SHADER_H
