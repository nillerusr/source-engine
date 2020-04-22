//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef R_LOCAL_H
#define R_LOCAL_H

#ifdef _WIN32
#pragma once
#endif

#include "surfacehandle.h"
#include "bspfile.h"

extern	int		r_framecount;

// NOTE: We store all 256 here so that 255 can be accessed easily...
extern	int		d_lightstylevalue[256];	// 8.8 fraction of base light value
extern	int		d_lightstyleframe[256];	// Frame when the light style value changed
extern	int		d_lightstylenumframes[256]; // number of frames in the lightstyle
extern ConVar	r_lightmapcolorscale;
extern ConVar	r_decals;
extern ConVar	mp_decals;
extern ConVar	r_lightmap;
extern ConVar	r_lightstyle;

extern int		r_dlightchanged;	// which ones changed
extern int		r_dlightactive;		// which ones are active
extern VMatrix g_BrushToWorldMatrix;

class IClientEntity;

colorVec R_LightPoint (Vector& p);

// returns surfID
SurfaceHandle_t R_LightVec (const Vector& start, const Vector& end, bool bUseLightStyles, Vector& c, float *textureS = NULL, float *textureT = NULL, float *lightmapS = NULL, float *lightmapT = NULL);

// This is to allow us to do R_LightVec on a brush model
void R_LightVecUseModel(model_t* pModel = 0);

void R_InitStudio( void );
void R_LoadSkys (void);
void R_DecalInit ( void );
void R_AnimateLight (void);

void MarkDLightsOnStaticProps( void );

//-----------------------------------------------------------------------------
// Method to get at the light style value
//-----------------------------------------------------------------------------

inline float LightStyleValue( int style )
{
	Assert( style >= 0 && style < MAX_LIGHTSTYLES );
	return ( float )d_lightstylevalue[style] * (1.0f / 264.0f);
}

#endif // R_LOCAL_H
