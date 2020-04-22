//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
	   

#include "render_pch.h"
#include "client.h"
#include "bitmap/imageformat.h"
#include "bitmap/tgawriter.h"
#include <float.h>
#include "collisionutils.h"
#include "cl_main.h"
#include "tier0/vprof.h"
#include "debugoverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
extern ConVar r_avglight;
extern int r_surfacevisframe;

static model_t* s_pLightVecModel = 0;

ConVar r_visualizetraces( "r_visualizetraces", "0", FCVAR_CHEAT );
ConVar r_visualizelighttraces( "r_visualizelighttraces", "0", FCVAR_CHEAT );
ConVar r_visualizelighttracesshowfulltrace( "r_visualizelighttracesshowfulltrace", "0", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// State associated with R_LightVec
//-----------------------------------------------------------------------------
struct LightVecState_t
{
	LightVecState_t()
	{
	}
	Ray_t	m_Ray;
	float	m_HitFrac;
	float*	m_pTextureS;
	float*	m_pTextureT;
	float*	m_pLightmapS;
	float*	m_pLightmapT;
	SurfaceHandle_t m_nSkySurfID;
	bool	m_bUseLightStyles;
	CUtlVector<IDispInfo *>	m_LightTestDisps;
};


//-----------------------------------------------------------------------------
// Globals associated with dynamic lighting
//-----------------------------------------------------------------------------
int	r_dlightchanged;
int	r_dlightactive;


//-----------------------------------------------------------------------------
// Displacements to test against for R_LightVec
//-----------------------------------------------------------------------------


/*
==================
R_AnimateLight
==================
*/
void R_AnimateLight (void)
{
	INetworkStringTable *table = cl.m_pLightStyleTable;

	if ( !table )
		return;

	// light animations
	// 'm' is normal light, 'a' is no light, 'z' is double bright
	int i = (int)(cl.GetTime()*10);

	for (int j=0 ; j<MAX_LIGHTSTYLES ; j++)
	{
		int length;
		const char * lightstyle = (const char*) table->GetStringUserData( j, &length );
		length--;
		
		if (!lightstyle || !lightstyle[0])
		{
			d_lightstylevalue[j] = 256;
			d_lightstylenumframes[j] = 0;
			continue;
		}
		d_lightstylenumframes[j] = length;
		int k = i % length;
		k = lightstyle[k] - 'a';
		k = k*22;
		if (d_lightstylevalue[j] != k)
		{
			d_lightstylevalue[j] = k;
			d_lightstyleframe[j] = r_framecount;
		}
	}	
}

/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

// Returns true if the surface has the specified dlight already set on it for this frame.
inline bool R_IsDLightAlreadyMarked( msurfacelighting_t *pLighting, int bit )
{
	return (pLighting->m_nDLightFrame == r_framecount) && (pLighting->m_fDLightBits & bit);
}

// Mark the surface as changed by the specified dlight (so its texture gets updated when 
// it comes time to render).
inline void R_MarkSurfaceDLight( SurfaceHandle_t surfID, msurfacelighting_t *pLighting, int bit)
{
	pLighting->m_nDLightFrame = r_framecount;
	pLighting->m_fDLightBits |= bit;
	MSurf_Flags( surfID ) |= SURFDRAW_HASDLIGHT;
}

int R_TryLightMarkSurface( dlight_t *light, msurfacelighting_t *pLighting, SurfaceHandle_t surfID, int bit )
{
	// Make sure this light actually intersects the surface cache of the surfaces it hits
	mtexinfo_t	*tex;

	// FIXME: No worky for brush models

	// Find the perpendicular distance to the surface we're lighting
	// NOTE: Allow some stuff that's slightly behind it because view models can get behind walls
	// FIXME: We should figure out a better way to deal with view models
	float perpDistSq = DotProduct (light->origin, MSurf_Plane( surfID ).normal) - MSurf_Plane( surfID ).dist;
	if (perpDistSq < DLIGHT_BEHIND_PLANE_DIST)
		return 0;

	perpDistSq *= perpDistSq;

	float flInPlaneRadiusSq = light->GetRadiusSquared() - perpDistSq;
	if (flInPlaneRadiusSq <= 0)
		return 0;

	tex = MSurf_TexInfo( surfID );

	Vector2D mins, maxs;
	mins.Init( pLighting->m_LightmapMins[0], pLighting->m_LightmapMins[1] ); 
	maxs.Init( mins.x + pLighting->m_LightmapExtents[0], mins.y + pLighting->m_LightmapExtents[1] );

	// Project light center into texture coordinates
	Vector2D vecCircleCenter;
	vecCircleCenter.x = DotProduct (light->origin, tex->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D()) + 
		tex->lightmapVecsLuxelsPerWorldUnits[0][3];
	vecCircleCenter.y = DotProduct (light->origin, tex->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D()) + 
		tex->lightmapVecsLuxelsPerWorldUnits[1][3];

	// convert from world space to luxel space and convert to int
	float flInPlaneLuxelRadius = sqrtf( flInPlaneRadiusSq * tex->luxelsPerWorldUnit * tex->luxelsPerWorldUnit );

	// Does the circle intersect the square?
	if ( !IsCircleIntersectingRectangle( mins, maxs, vecCircleCenter, flInPlaneLuxelRadius ) )
		return 0;

	// Ok, mark the surface as using this light.
	R_MarkSurfaceDLight( surfID, pLighting, bit); 
	return 1;
}

int R_MarkLightsLeaf( dlight_t *light, int bit, mleaf_t *pLeaf )
{
	int countMarked = 0;
	for ( int i = 0; i < pLeaf->dispCount; i++ )
	{
		IDispInfo *pDispInfo = MLeaf_Disaplcement( pLeaf, i );

		SurfaceHandle_t parentSurfID = pDispInfo->GetParent();
		if ( parentSurfID )
		{
			// Don't redo all this work if we already hit this surface and decided it's lit by this light.
			msurfacelighting_t *pLighting = SurfaceLighting( parentSurfID );
			if( !R_IsDLightAlreadyMarked( pLighting, bit) )
			{
				// Do a different test for displacement surfaces.
				Vector bmin, bmax;
				MSurf_DispInfo( parentSurfID )->GetBoundingBox( bmin, bmax );
				if ( IsBoxIntersectingSphere(bmin, bmax, light->origin, light->GetRadius()) )
				{
					R_MarkSurfaceDLight( parentSurfID, pLighting, bit );
					countMarked++;
				}
			}
		}
	}

	SurfaceHandle_t *pHandle = &host_state.worldbrush->marksurfaces[pLeaf->firstmarksurface];
	for ( int i = 0; i < pLeaf->nummarksurfaces; i++ )
	{
		SurfaceHandle_t surfID = pHandle[i];
		ASSERT_SURF_VALID( surfID );
		
		// only process leaf surfaces
		if ( MSurf_Flags( surfID ) & SURFDRAW_NODE )
			continue;

		// Don't redo all this work if we already hit this surface and decided it's lit by this light.
		msurfacelighting_t *pLighting = SurfaceLighting( surfID );
		if(R_IsDLightAlreadyMarked(pLighting, bit))
			continue;

		float dist = DotProduct( light->origin, MSurf_Plane( surfID ).normal) - MSurf_Plane( surfID ).dist;
		
		if ( dist > light->GetRadius() || dist < -light->GetRadius() )
			continue;

		countMarked += R_TryLightMarkSurface( light, pLighting, surfID, bit );
	}
	return countMarked;
}


/*
=============
R_MarkLights
=============
*/
int R_MarkLights (dlight_t *light, int bit, mnode_t *node)
{
	cplane_t	*splitplane;
	float		dist;
	int			i;
	
	if (node->contents >= 0)
	{
		// This is a leaf, so check displacement surfaces and leaf faces
		return R_MarkLightsLeaf( light, bit, (mleaf_t*)node );
	}
	
	splitplane = node->plane;
	dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist;
	
	if (dist > light->GetRadius())
	{
		return R_MarkLights (light, bit, node->children[0]);
	}
	if (dist < -light->GetRadius())
	{
		return R_MarkLights (light, bit, node->children[1]);
	}
		
	// mark the polygons
	int countMarked = 0;
	SurfaceHandle_t surfID = SurfaceHandleFromIndex( node->firstsurface );
	for (i=0 ; i<node->numsurfaces ; i++, surfID++)
	{
		// Don't redo all this work if we already hit this surface and decided it's lit by this light.
		msurfacelighting_t *pLighting = SurfaceLighting( surfID );
		if(R_IsDLightAlreadyMarked( pLighting, bit))
			continue;

		countMarked += R_TryLightMarkSurface( light, pLighting, surfID, bit );
	}

	countMarked += R_MarkLights( light, bit, node->children[0] );
	return countMarked + R_MarkLights( light, bit, node->children[1] );
}


void R_MarkDLightsOnSurface( mnode_t* pNode )
{
	if (!pNode || !g_bActiveDlights)
		return;

	dlight_t	*l = cl_dlights;
	for (int i=0 ; i<MAX_DLIGHTS ; i++, l++)
	{
		if (l->die < cl.GetTime() || !l->IsRadiusGreaterThanZero() )
			continue;
		if (l->flags & DLIGHT_NO_WORLD_ILLUMINATION)
			continue;
		
		R_MarkLights ( l, 1<<i, pNode );
	}
}

/*
=============
R_PushDlights
=============
*/
void R_PushDlights (void)
{
	R_MarkDLightsOnSurface( host_state.worldbrush->nodes );
	MarkDLightsOnStaticProps();
}



//-----------------------------------------------------------------------------
// Computes s and t coords of texture at intersection pt
//-----------------------------------------------------------------------------

static void ComputeTextureCoordsAtIntersection( mtexinfo_t* pTex, Vector const& pt, float *textureS, float *textureT )
{
	if( pTex->material && textureS && textureT )
	{
		*textureS = DotProduct( pt, pTex->textureVecsTexelsPerWorldUnits[0].AsVector3D() ) +
			pTex->textureVecsTexelsPerWorldUnits[0][3];
		*textureT = DotProduct( pt, pTex->textureVecsTexelsPerWorldUnits[1].AsVector3D() ) +
			pTex->textureVecsTexelsPerWorldUnits[1][3];
		
		*textureS /= pTex->material->GetMappingWidth();
		*textureT /= pTex->material->GetMappingHeight();
	}
}


//-----------------------------------------------------------------------------
// Computes s and t coords of texture at intersection pt
//-----------------------------------------------------------------------------
static void ComputeLightmapCoordsAtIntersection( msurfacelighting_t *pLighting, float ds, 
								float dt, float *lightmapS, float *lightmapT )
{
	if( lightmapS && lightmapT )
	{
		if( pLighting->m_LightmapExtents[0] != 0 )
			*lightmapS = (ds + 0.5f) / ( float )pLighting->m_LightmapExtents[0];
		else
			*lightmapS = 0.5f;

		if( pLighting->m_LightmapExtents[1] != 0 )
			*lightmapT = (dt + 0.5f) / ( float )pLighting->m_LightmapExtents[1];
		else
			*lightmapT = 0.5f;
	}
}


//-----------------------------------------------------------------------------
// Computes the lightmap color at a particular point
//-----------------------------------------------------------------------------
static void ComputeLightmapColor( SurfaceHandle_t surfID, int ds, int dt, bool bUseLightStyles, Vector& c )
{
	msurfacelighting_t *pLighting = SurfaceLighting( surfID );

	ColorRGBExp32* pLightmap = pLighting->m_pSamples;
	if( !pLightmap )
	{
		static int messagecount = 0;
		if ( ++messagecount < 10 )
		{
			// Stop spamming. I heard you already!!!
			ConMsg( "hit surface has no samples\n" );
		}
		return;
	}

	int smax = ( pLighting->m_LightmapExtents[0] ) + 1;
	int tmax = ( pLighting->m_LightmapExtents[1] ) + 1;
	int offset = smax * tmax;
	if ( SurfHasBumpedLightmaps( surfID ) )
	{
		offset *= ( NUM_BUMP_VECTS + 1 );
	}

	pLightmap += dt * smax + ds;
	int nMaxMaps = bUseLightStyles ? MAXLIGHTMAPS : 1; 
	for (int maps = 0 ; maps < nMaxMaps && pLighting->m_nStyles[maps] != 255 ; ++maps)
	{
		float scale = LightStyleValue( pLighting->m_nStyles[maps] );

		c[0] += TexLightToLinear( pLightmap->r, pLightmap->exponent ) * scale;
		c[1] += TexLightToLinear( pLightmap->g, pLightmap->exponent ) * scale;
		c[2] += TexLightToLinear( pLightmap->b, pLightmap->exponent ) * scale;

		// Check version 32 in source safe for some debugging crap
		pLightmap += offset;
	}
}


//-----------------------------------------------------------------------------
// Computes the lightmap color at a particular point
//-----------------------------------------------------------------------------
static void ComputeLightmapColorFromAverage( msurfacelighting_t *pLighting, bool bUseLightStyles, Vector& c )
{
	int nMaxMaps = bUseLightStyles ? MAXLIGHTMAPS : 1; 
	for (int maps = 0 ; maps < nMaxMaps && pLighting->m_nStyles[maps] != 255 ; ++maps)
	{
		float scale = LightStyleValue( pLighting->m_nStyles[maps] );

		ColorRGBExp32* pAvgColor = pLighting->AvgLightColor(maps);
		c[0] += TexLightToLinear( pAvgColor->r, pAvgColor->exponent ) * scale;
		c[1] += TexLightToLinear( pAvgColor->g, pAvgColor->exponent ) * scale;
		c[2] += TexLightToLinear( pAvgColor->b, pAvgColor->exponent ) * scale;
	}
}


//-----------------------------------------------------------------------------
// Tests a particular surface
//-----------------------------------------------------------------------------
static bool FASTCALL FindIntersectionAtSurface( SurfaceHandle_t surfID, float f, 
	Vector& c, LightVecState_t& state )
{
	// no lightmaps on this surface? punt...
	// FIXME: should be water surface?
	if (MSurf_Flags( surfID ) & SURFDRAW_NOLIGHT)
		return false;	

	// Compute the actual point
	Vector pt;
	VectorMA( state.m_Ray.m_Start, f, state.m_Ray.m_Delta, pt );

	mtexinfo_t* pTex = MSurf_TexInfo( surfID );
	
	// See where in lightmap space our intersection point is 
	float s, t;
	s = DotProduct (pt, pTex->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D()) + 
		pTex->lightmapVecsLuxelsPerWorldUnits[0][3];
	t = DotProduct (pt, pTex->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D()) + 
		pTex->lightmapVecsLuxelsPerWorldUnits[1][3];

	// Not in the bounds of our lightmap? punt...
	msurfacelighting_t *pLighting = SurfaceLighting( surfID );
	if( s < pLighting->m_LightmapMins[0] || 
		t < pLighting->m_LightmapMins[1] )
		return false;	
	
	// assuming a square lightmap (FIXME: which ain't always the case),
	// lets see if it lies in that rectangle. If not, punt...
	float ds = s - pLighting->m_LightmapMins[0];
	float dt = t - pLighting->m_LightmapMins[1];
	if ( !pLighting->m_LightmapExtents[0] && !pLighting->m_LightmapExtents[1] )
	{
		worldbrushdata_t *pBrushData = host_state.worldbrush;

		// 
		float	lightMaxs[2];
		lightMaxs[ 0 ] = pLighting->m_LightmapMins[0];
		lightMaxs[ 1 ] = pLighting->m_LightmapMins[1];
		int i;
		for (i=0 ; i<MSurf_VertCount( surfID ); i++)
		{
			int e = pBrushData->vertindices[MSurf_FirstVertIndex( surfID )+i];
			mvertex_t *v = &pBrushData->vertexes[e];
			
			int j;
			for ( j=0 ; j<2 ; j++)
			{
				float sextent, textent;
				sextent = DotProduct (v->position, pTex->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D()) + 
					pTex->lightmapVecsLuxelsPerWorldUnits[0][3] - pLighting->m_LightmapMins[0];
				textent = DotProduct (v->position, pTex->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D()) + 
					pTex->lightmapVecsLuxelsPerWorldUnits[1][3] - pLighting->m_LightmapMins[1];

				if ( sextent > lightMaxs[ 0 ] )
				{
					lightMaxs[ 0 ] = sextent;
				}
				if ( textent > lightMaxs[ 1 ] )
				{
					lightMaxs[ 1 ] = textent;
				}
			}
		}
		if( ds > lightMaxs[0] || dt > lightMaxs[1] )
			return false;	
	}
	else
	{
		if( ds > pLighting->m_LightmapExtents[0] || dt > pLighting->m_LightmapExtents[1] )
			return false;	
	}

	// Store off the hit distance...
	state.m_HitFrac = f;

	// You heard the man!
	ComputeTextureCoordsAtIntersection( pTex, pt, state.m_pTextureS, state.m_pTextureT );

#ifdef USE_CONVARS
	if ( r_avglight.GetInt() )
#else
	if ( 1 )
#endif
	{
		// This is the faster path; it looks slightly different though
		ComputeLightmapColorFromAverage( pLighting, state.m_bUseLightStyles, c );
	}
	else
	{
		// Compute lightmap coords
		ComputeLightmapCoordsAtIntersection( pLighting, ds, dt, state.m_pLightmapS, state.m_pLightmapT );
		
		// Check out the value of the lightmap at the intersection point
		ComputeLightmapColor( surfID, (int)ds, (int)dt, state.m_bUseLightStyles, c );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Tests a particular node
//-----------------------------------------------------------------------------

// returns a surfID
static SurfaceHandle_t FindIntersectionSurfaceAtNode( mnode_t *node, float t, 
	Vector& c, LightVecState_t& state )
{
	SurfaceHandle_t surfID = SurfaceHandleFromIndex( node->firstsurface );
	for (int i=0 ; i<node->numsurfaces ; ++i, ++surfID)
	{
		// Don't immediately return when we hit sky; 
		// we may actually hit another surface
		if (MSurf_Flags( surfID ) & SURFDRAW_SKY)
		{
			state.m_nSkySurfID = surfID;
			continue;
		}

		// Don't let water surfaces affect us
		if (MSurf_Flags( surfID ) & SURFDRAW_WATERSURFACE)
			continue;

		// Check this surface to see if there's an intersection
		if (FindIntersectionAtSurface( surfID, t, c, state ))
		{
			return surfID;
		}
	}

	return SURFACE_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
// Tests a ray against displacements
//-----------------------------------------------------------------------------

// returns surfID
static SurfaceHandle_t R_LightVecDisplacementChain( LightVecState_t& state, bool bUseLightStyles, Vector& c )
{
	// test the ray against displacements
	SurfaceHandle_t surfID = SURFACE_HANDLE_INVALID;

	for ( int i = 0; i < state.m_LightTestDisps.Count(); i++ )
	{
	
		float dist;
		Vector2D luv, tuv;
		IDispInfo *pDispInfo = state.m_LightTestDisps[i];
		if (pDispInfo->TestRay( state.m_Ray, 0.0f, state.m_HitFrac, dist, &luv, &tuv ))
		{
			// It hit it, and at a point closer than the previously computed
			// nearest intersection point
			state.m_HitFrac = dist;
			surfID = pDispInfo->GetParent();
			ComputeLightmapColor( surfID, (int)luv.x, (int)luv.y, bUseLightStyles, c );

			if (state.m_pLightmapS && state.m_pLightmapT)
			{
				ComputeLightmapCoordsAtIntersection( SurfaceLighting(surfID), (int)luv.x, (int)luv.y, state.m_pLightmapS, state.m_pLightmapT );
			}

			if (state.m_pTextureS && state.m_pTextureT)
			{
				*state.m_pTextureS = tuv.x;
				*state.m_pTextureT = tuv.y;
			}
		}
	}

	return surfID;
}


//-----------------------------------------------------------------------------
// Adds displacements in a leaf to a list to be tested against
//-----------------------------------------------------------------------------

static void AddDisplacementsInLeafToTestList( mleaf_t* pLeaf, LightVecState_t& state )
{
	// add displacement surfaces
	for ( int i = 0; i < pLeaf->dispCount; i++ )
	{
		// NOTE: We're not using the displacement's touched method here 
		// because we're just using the parent surface's visframe in the
		// surface add methods below
		IDispInfo *pDispInfo = MLeaf_Disaplcement( pLeaf, i );
		SurfaceHandle_t parentSurfID = pDispInfo->GetParent();

		// already processed this frame? Then don't do it again!
		if (MSurf_VisFrame( parentSurfID ) != r_surfacevisframe)
		{
			MSurf_VisFrame( parentSurfID ) = r_surfacevisframe;
			state.m_LightTestDisps.AddToTail( pDispInfo );
		}
	}
}


//-----------------------------------------------------------------------------
// Tests a particular leaf
//-----------------------------------------------------------------------------

// returns surfID
static SurfaceHandle_t FASTCALL FindIntersectionSurfaceAtLeaf( mleaf_t *pLeaf, 
					float start, float end, Vector& c, LightVecState_t& state )
{
	Vector pt;
	SurfaceHandle_t closestSurfID = SURFACE_HANDLE_INVALID;

	// Adds displacements in the leaf to a list of displacements to test at the end
	AddDisplacementsInLeafToTestList( pLeaf, state );

	// Add non-displacement surfaces
	// Since there's no BSP tree here, we gotta test *all* surfaces! (blech)
	SurfaceHandle_t *pHandle = &host_state.worldbrush->marksurfaces[pLeaf->firstmarksurface];
	// NOTE: Skip all marknodesurfaces, only check detail/leaf faces
	for ( int i = pLeaf->nummarknodesurfaces; i < pLeaf->nummarksurfaces; i++ )
	{
		SurfaceHandle_t surfID = pHandle[i];
		ASSERT_SURF_VALID( surfID );

		// Don't add surfaces that have displacement; they are handled above
		// In fact, don't even set the vis frame; we need it unset for translucent
		// displacement code
		if ( SurfaceHasDispInfo(surfID) )
			continue;
		Assert(!(MSurf_Flags( surfID ) & SURFDRAW_NODE));

		if ( MSurf_Flags( surfID ) & (SURFDRAW_NODE|SURFDRAW_NODRAW | SURFDRAW_WATERSURFACE) )
			continue;

		cplane_t* pPlane = &MSurf_Plane( surfID );

		// Backface cull...
		if (DotProduct( pPlane->normal, state.m_Ray.m_Delta ) > 0.f)
			continue;

		float startDotN = DotProduct( state.m_Ray.m_Start, pPlane->normal );
		float deltaDotN = DotProduct( state.m_Ray.m_Delta, pPlane->normal );

		float front = startDotN + start * deltaDotN - pPlane->dist;
		float back = startDotN + end * deltaDotN - pPlane->dist;
		
		int side = front < 0.f;

		// Blow it off if it doesn't split the plane...
		if ( (back < 0.f) == side )
			continue;

		// Don't test a surface that is farther away from the closest found intersection
		float frac = front / (front-back);
		if (frac >= state.m_HitFrac)
			continue;

		float mid = start * (1.0f - frac) + end * frac;

		// Check this surface to see if there's an intersection
		if (FindIntersectionAtSurface( surfID, mid, c, state ))
		{
			closestSurfID = surfID;
		}
	}

	// Return the closest surface hit
	return closestSurfID;
}

//-----------------------------------------------------------------------------
// LIGHT SAMPLING
//-----------------------------------------------------------------------------

// returns surfID
SurfaceHandle_t RecursiveLightPoint (mnode_t *node, float start, float end,
	Vector& c, LightVecState_t& state )
{
	// didn't hit anything
	if (node->contents >= 0)
	{
		// FIXME: Should we always do this? It could get expensive...
		// Check all the faces at the leaves
		return FindIntersectionSurfaceAtLeaf( (mleaf_t*)node, start, end, c, state );
	}
	
	// Determine which side of the node plane our points are on
	// FIXME: optimize for axial
	cplane_t* plane = node->plane;

	float startDotN = DotProduct( state.m_Ray.m_Start, plane->normal );
	float deltaDotN = DotProduct( state.m_Ray.m_Delta, plane->normal );

	float front = startDotN + start * deltaDotN - plane->dist;
	float back = startDotN + end * deltaDotN - plane->dist;
	int side = front < 0;
	
	// If they're both on the same side of the plane, don't bother to split
	// just check the appropriate child
	SurfaceHandle_t surfID;
	if ( (back < 0) == side )
	{
		surfID = RecursiveLightPoint (node->children[side], start, end, c, state);
		return surfID;
	}
	
	// calculate mid point
	float frac = front / (front-back);
	float mid = start * (1.0f - frac) + end * frac;
	
	// go down front side	
	surfID = RecursiveLightPoint (node->children[side], start, mid, c, state );
	if ( IS_SURF_VALID( surfID ) )
		return surfID;		// hit something
				
	// check for impact on this node
	surfID = FindIntersectionSurfaceAtNode( node, mid, c, state );
	if ( IS_SURF_VALID( surfID ) )
		return surfID;

	// go down back side
	surfID = RecursiveLightPoint (node->children[!side], mid, end, c, state );
	return surfID;
}


//-----------------------------------------------------------------------------
// Allows us to use a different model for R_LightVec
//-----------------------------------------------------------------------------
void R_LightVecUseModel( model_t* pModel )
{
	s_pLightVecModel = pModel;
}


//-----------------------------------------------------------------------------
// returns light in range from 0 to 1.
// lightmapS/T is in [0,1] within the space of the surface.
// returns surfID
//-----------------------------------------------------------------------------
SurfaceHandle_t R_LightVec (const Vector& start, const Vector& end, bool bUseLightStyles, Vector& c, 
		float *textureS, float *textureT, float *lightmapS, float *lightmapT )
{
	VPROF_INCREMENT_COUNTER( "R_LightVec", 1 );

	SurfaceHandle_t retSurfID;
	SurfaceHandle_t dispSurfID;
	
	// We're using the vis frame here for lightvec tests
	// to make sure we test each displacement only once
	++r_surfacevisframe;

	LightVecState_t state;
	state.m_HitFrac = 1.0f;
	state.m_Ray.Init( start, end );
	state.m_pTextureS = textureS;
	state.m_pTextureT = textureT;
	state.m_pLightmapS = lightmapS;
	state.m_pLightmapT = lightmapT;
	state.m_nSkySurfID = SURFACE_HANDLE_INVALID;
	state.m_bUseLightStyles = bUseLightStyles;

	c[0] = c[1] = c[2] = 0.0f;

	model_t* model = s_pLightVecModel ? s_pLightVecModel : host_state.worldmodel; 
	retSurfID = RecursiveLightPoint(&model->brush.pShared->nodes[model->brush.firstnode],
		0.0f, 1.0f, c, state );

	// While doing recursive light point, we built a list of all
	// displacement surfaces which we need to test, so let's test them
	dispSurfID = R_LightVecDisplacementChain( state, bUseLightStyles, c );

	if( r_visualizelighttraces.GetBool() )
	{
		if( r_visualizelighttracesshowfulltrace.GetBool() )
		{
			CDebugOverlay::AddLineOverlay( start, end, 0, 255, 0, 255, true, -1.0f );
		}
		else
		{
			CDebugOverlay::AddLineOverlay( start, start + ( end - start ) * state.m_HitFrac, 0, 255, 0, 255, true, -1.0f );
		}
	}

	if ( IS_SURF_VALID( dispSurfID ) )
		retSurfID = dispSurfID;

//	ConMsg( "R_LightVec: %f %f %f\n", c[0], c[1], c[2] );

	// If we didn't hit anything else, but we hit a sky surface at
	// some point along the ray cast, return the sky id.
	if ( ( retSurfID == SURFACE_HANDLE_INVALID ) && ( state.m_nSkySurfID != SURFACE_HANDLE_INVALID ) )
		return state.m_nSkySurfID;

	return retSurfID;
}

// returns light in range from 0 to 1.
colorVec R_LightPoint (Vector& p)
{
	SurfaceHandle_t surfID;
	Vector		end;
	colorVec	c;
	Vector		color;
	
	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	surfID = R_LightVec( p, end, true, color );

	if( IS_SURF_VALID( surfID ) )
	{
		c.r = LinearToScreenGamma( color[0] ) * 255;
		c.g = LinearToScreenGamma( color[1] ) * 255;
		c.b = LinearToScreenGamma( color[2] ) * 255;
		c.a = 1;
	}
	else
	{
		c.r = c.g = c.b = c.a = 0;
	}
	return c;
}
