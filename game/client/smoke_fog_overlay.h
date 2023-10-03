//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef SMOKE_FOG_OVERLAY_H
#define SMOKE_FOG_OVERLAY_H


#include "basetypes.h"
#include "mathlib/vector.h"


#define ROTATION_SPEED				0.6
#define TRADE_DURATION_MIN			5
#define TRADE_DURATION_MAX			10
#define SMOKEGRENADE_PARTICLERADIUS	55

#define SMOKESPHERE_EXPAND_TIME		5.5		// Take N seconds to expand to SMOKESPHERE_MAX_RADIUS.

#define NUM_PARTICLES_PER_DIMENSION	6
#define SMOKEPARTICLE_OVERLAP		0

#define SMOKEPARTICLE_SIZE			55
#define NUM_MATERIAL_HANDLES		1


void InitSmokeFogOverlay();
void TermSmokeFogOverlay();
void DrawSmokeFogOverlay();


// Set these before calling DrawSmokeFogOverlay.
extern float	g_SmokeFogOverlayAlpha;
extern Vector	g_SmokeFogOverlayColor;


#endif


