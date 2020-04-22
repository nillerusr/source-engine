//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef R_AREAPORTAL_H
#define R_AREAPORTAL_H
#ifdef _WIN32
#pragma once
#endif


#include "utlvector.h"
#include "gl_model_private.h"


class Frustum_t;


// Used to clip area portals. The coordinates here are in normalized
// view space (-1,-1) - (1,1)
// BUGBUG: NOTE!!!: These are left->right, and bottom->top, so a full rect is left=-1,top=1,right=1,bottom=-1
class CPortalRect
{
public:
	float	left, top, right, bottom;
};


// ---------------------------------------------------------------------------- //
// Functions.
// ---------------------------------------------------------------------------- //

// Copies cl.pAreaBits, finds the area the viewer is in, and figures out what
// other areas are visible. The new bits are placed in g_RenderAreaBits.
void R_SetupAreaBits( int iForceViewLeaf = -1, const VisOverrideData_t* pVisData = NULL, float *pWaterReflectionHeight = NULL );

// Ask if an area is visible to the renderer.
unsigned char R_IsAreaVisible( int area );

void R_Areaportal_LevelInit();
void R_Areaportal_LevelShutdown();

// Decides if the node can be seen through the area portals (ie: if you're
// looking out a window with an areaportal in it, this will clip out the
// stuff to the sides).
enum
{
	FRUSTUM_CLIP_RIGHT = (1 << FRUSTUM_RIGHT),
	FRUSTUM_CLIP_LEFT = (1 << FRUSTUM_LEFT),
	FRUSTUM_CLIP_TOP = (1 << FRUSTUM_TOP),
	FRUSTUM_CLIP_BOTTOM = (1 << FRUSTUM_BOTTOM),

	FRUSTUM_CLIP_MASK = FRUSTUM_CLIP_RIGHT | FRUSTUM_CLIP_LEFT | FRUSTUM_CLIP_TOP | FRUSTUM_CLIP_BOTTOM,

	// This mask is used to reset the clip flags when we first enter an area
	FRUSTUM_CLIP_IN_AREA = 0x80000000,

	// Set the clipmask to this to cause it to clip against all frustum planes
	FRUSTUM_CLIP_ALL = FRUSTUM_CLIP_MASK,

	// Set the clipmask to this to suppress all future clipping
	FRUSTUM_SUPPRESS_CLIPPING = FRUSTUM_CLIP_IN_AREA,
};


bool R_CullNode( Frustum_t *pAreaFrustum, mnode_t *pNode, int &nClipMask );

const Frustum_t* GetAreaFrustum( int area );


// ---------------------------------------------------------------------------- //
// Globals.
// ---------------------------------------------------------------------------- //

extern ConVar r_DrawPortals;

// Used when r_DrawPortals is on. Draws the screen space rects for each portal.
extern CUtlVector<CPortalRect> g_PortalRects;



// ---------------------------------------------------------------------------- //
// Inlines.
// ---------------------------------------------------------------------------- //

inline unsigned char R_IsAreaVisible( int area )
{
	extern unsigned char g_RenderAreaBits[32];
	return g_RenderAreaBits[area>>3] & (1 << (area&7));
}


#endif // R_AREAPORTAL_H
