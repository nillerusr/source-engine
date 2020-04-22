//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//


#include "render_pch.h"
#include "gl_lightmap.h"
#include "view.h"
#include "gl_cvars.h"
#include "zone.h"
#include "gl_water.h"
#include "r_local.h"
#include "gl_model_private.h"
#include "mathlib/bumpvects.h"
#include "gl_matsysiface.h"
#include <float.h>
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imesh.h"
#include "tier0/dbg.h"
#include "tier0/vprof.h"
#include "tier1/callqueue.h"
#include "lightcache.h"
#include "cl_main.h"
#include "materialsystem/imaterial.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------

// Only enable this if you are testing lightstyle performance.
//#define UPDATE_LIGHTSTYLES_EVERY_FRAME

ALIGN128 Vector4D blocklights[NUM_BUMP_VECTS+1][ MAX_LIGHTMAP_DIM_INCLUDING_BORDER * MAX_LIGHTMAP_DIM_INCLUDING_BORDER ];

ConVar r_avglightmap( "r_avglightmap", "0", FCVAR_CHEAT | FCVAR_MATERIAL_SYSTEM_THREAD );
ConVar r_maxdlights( "r_maxdlights", "32" );
extern ConVar r_unloadlightmaps;
extern bool g_bHunkAllocLightmaps;

static int r_dlightvisible;
static int r_dlightvisiblethisframe;
static int s_nVisibleDLightCount;
static int s_nMaxVisibleDLightCount;


//-----------------------------------------------------------------------------
// Visible, not visible DLights
//-----------------------------------------------------------------------------
void R_MarkDLightVisible( int dlight )
{
	if ( (r_dlightvisible & ( 1 << dlight )) == 0 )
	{
		++s_nVisibleDLightCount;
		r_dlightvisible |= 1 << dlight;
	}
}

void R_MarkDLightNotVisible( int dlight )
{
	if ( r_dlightvisible & ( 1 << dlight ))
	{
		--s_nVisibleDLightCount;
		r_dlightvisible &= ~( 1 << dlight );
	}
}


//-----------------------------------------------------------------------------
// Must call these at the start + end of rendering each view
//-----------------------------------------------------------------------------
void R_DLightStartView()
{
	r_dlightvisiblethisframe = 0;
	s_nMaxVisibleDLightCount = r_maxdlights.GetInt();
}

void R_DLightEndView()
{
	if ( !g_bActiveDlights )
		return;
	for( int lnum=0 ; lnum<MAX_DLIGHTS; lnum++ )
	{
		if ( r_dlightvisiblethisframe & ( 1 << lnum ))
			continue;

		R_MarkDLightNotVisible( lnum );
	}
}


//-----------------------------------------------------------------------------
// Can we use another dynamic light, or is it just too expensive?
//-----------------------------------------------------------------------------
bool R_CanUseVisibleDLight( int dlight )
{
	r_dlightvisiblethisframe |= (1 << dlight);

	if ( r_dlightvisible & ( 1 << dlight ) )
		return true;

	if ( s_nVisibleDLightCount >= s_nMaxVisibleDLightCount )
		return false;

	R_MarkDLightVisible( dlight );
	return true;
}


//-----------------------------------------------------------------------------
// Adds a single dynamic light
//-----------------------------------------------------------------------------
static bool AddSingleDynamicLight( dlight_t& dl, SurfaceHandle_t surfID, const Vector &lightOrigin, float perpDistSq, float lightRadiusSq )
{
	// transform the light into brush local space
	Vector local;
	// Spotlight early outs...
	if (dl.m_OuterAngle)
	{
		if (dl.m_OuterAngle < 180.0f)
		{
			// Can't light anything from the rear...
			if (DotProduct(dl.m_Direction, MSurf_Plane( surfID ).normal) >= 0.0f)
				return false;
		}
	}

	// Transform the light center point into (u,v) space of the lightmap
	mtexinfo_t* tex = MSurf_TexInfo( surfID );
	local[0] = DotProduct (lightOrigin, tex->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D()) + 
			   tex->lightmapVecsLuxelsPerWorldUnits[0][3];
	local[1] = DotProduct (lightOrigin, tex->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D()) + 
			   tex->lightmapVecsLuxelsPerWorldUnits[1][3];

	// Now put the center points into the space of the lightmap rectangle
	// defined by the lightmapMins + lightmapExtents
	local[0] -= MSurf_LightmapMins( surfID )[0];
	local[1] -= MSurf_LightmapMins( surfID )[1];
	
	// Figure out the quadratic attenuation factor...
	Vector intensity;
	float lightStyleValue = LightStyleValue( dl.style );
	intensity[0] = TexLightToLinear( dl.color.r, dl.color.exponent ) * lightStyleValue;
	intensity[1] = TexLightToLinear( dl.color.g, dl.color.exponent ) * lightStyleValue;
	intensity[2] = TexLightToLinear( dl.color.b, dl.color.exponent ) * lightStyleValue;

	float minlight = fpmax( g_flMinLightingValue, dl.minlight );
	float ooQuadraticAttn = lightRadiusSq * minlight;
	float ooRadiusSq = 1.0f / lightRadiusSq;

	// Compute a color at each luxel
	// We want to know the square distance from luxel center to light
	// so we can compute an 1/r^2 falloff in light color
	int smax = MSurf_LightmapExtents( surfID )[0] + 1;
	int tmax = MSurf_LightmapExtents( surfID )[1] + 1;
	for (int t=0; t<tmax; ++t)
	{
		float td = (local[1] - t) * tex->worldUnitsPerLuxel;
		
		for (int s=0; s<smax; ++s)
		{
			float sd = (local[0] - s) * tex->worldUnitsPerLuxel;

			float inPlaneDistSq = sd * sd + td * td;
			float totalDistSq = inPlaneDistSq + perpDistSq;
			if (totalDistSq < lightRadiusSq)
			{
				// at least all floating point only happens when a luxel is lit.
				float scale = (totalDistSq != 0.0f) ? ooQuadraticAttn / totalDistSq : 1.0f;

				// Apply a little extra attenuation
				scale *= (1.0f - totalDistSq * ooRadiusSq);

				if (scale > 2.0f)
					scale = 2.0f;

				int idx = t*smax + s;

				// Compute the base lighting just as is done in the non-bump case...
				blocklights[0][idx][0] += scale * intensity[0];
				blocklights[0][idx][1] += scale * intensity[1];
				blocklights[0][idx][2] += scale * intensity[2];
			}
		}
	}
	return true;
}												

//-----------------------------------------------------------------------------
// Adds a dynamic light to the bumped lighting
//-----------------------------------------------------------------------------
static void AddSingleDynamicLightToBumpLighting( dlight_t& dl, SurfaceHandle_t surfID, 
	const Vector &lightOrigin, float perpDistSq, float lightRadiusSq, Vector* pBumpBasis, const Vector& luxelBasePosition )
{
	Vector local;
	// FIXME: For now, only elights can be spotlights
	// the lightmap computation could get expensive for spotlights...
	Assert( dl.m_OuterAngle == 0.0f );

	// Transform the light center point into (u,v) space of the lightmap
	mtexinfo_t *pTexInfo = MSurf_TexInfo( surfID );
	local[0] = DotProduct (lightOrigin, pTexInfo->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D()) + 
			   pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][3];
	local[1] = DotProduct (lightOrigin, pTexInfo->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D()) + 
			   pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][3];

	// Now put the center points into the space of the lightmap rectangle
	// defined by the lightmapMins + lightmapExtents
	local[0] -= MSurf_LightmapMins( surfID )[0];
	local[1] -= MSurf_LightmapMins( surfID )[1];

	// Figure out the quadratic attenuation factor...
	Vector intensity;
	float lightStyleValue = LightStyleValue( dl.style );
	intensity[0] = TexLightToLinear( dl.color.r, dl.color.exponent ) * lightStyleValue;
	intensity[1] = TexLightToLinear( dl.color.g, dl.color.exponent ) * lightStyleValue;
	intensity[2] = TexLightToLinear( dl.color.b, dl.color.exponent ) * lightStyleValue;

	float minlight = fpmax( g_flMinLightingValue, dl.minlight );
	float ooQuadraticAttn = lightRadiusSq * minlight;
	float ooRadiusSq = 1.0f / lightRadiusSq;

	// The algorithm here is necessary to make dynamic lights live in the
	// same world as the non-bumped dynamic lights. Therefore, we compute
	// the intensity of the flat lightmap the exact same way here as when
	// we've got a non-bumped surface.

	// Then, I compute an actual light direction vector per luxel (FIXME: !!expensive!!)
	// and compute what light would have to come in along that direction
	// in order to produce the same illumination on the flat lightmap. That's
	// computed by dividing the flat lightmap color by n dot l.
	Vector lightDirection(0, 0, 0), texelWorldPosition;
#if 1
	bool useLightDirection = (dl.m_OuterAngle != 0.0f) &&
		(fabs(dl.m_Direction.LengthSqr() - 1.0f) < 1e-3);
	if (useLightDirection)
		VectorMultiply( dl.m_Direction, -1.0f, lightDirection );
#endif

	// Since there's a scale factor used when going from world to luxel,
	// we gotta undo that scale factor when going from luxel to world
	float fixupFactor = pTexInfo->worldUnitsPerLuxel * pTexInfo->worldUnitsPerLuxel;

	// Compute a color at each luxel
	// We want to know the square distance from luxel center to light
	// so we can compute an 1/r^2 falloff in light color
	int smax = MSurf_LightmapExtents( surfID )[0] + 1;
	int tmax = MSurf_LightmapExtents( surfID )[1] + 1;
	for (int t=0; t<tmax; ++t)
	{
		float td = (local[1] - t) * pTexInfo->worldUnitsPerLuxel;
		
		// Move along the v direction
		VectorMA( luxelBasePosition, t * fixupFactor, pTexInfo->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D(), 
			texelWorldPosition );

		for (int s=0; s<smax; ++s)
		{
			float sd = (local[0] - s) * pTexInfo->worldUnitsPerLuxel;

			float inPlaneDistSq = sd * sd + td * td;
			float totalDistSq = inPlaneDistSq + perpDistSq;

			if (totalDistSq < lightRadiusSq)
			{
				// at least all floating point only happens when a luxel is lit.
				float scale = (totalDistSq != 0.0f) ? ooQuadraticAttn / totalDistSq : 1.0f;

				// Apply a little extra attenuation
				scale *= (1.0f - totalDistSq * ooRadiusSq);

				if (scale > 2.0f)
					scale = 2.0f;

				int idx = t*smax + s;

				// Compute the base lighting just as is done in the non-bump case...
				VectorMA( blocklights[0][idx].AsVector3D(), scale, intensity, blocklights[0][idx].AsVector3D() );

#if 1
				if (!useLightDirection)
				{
					VectorSubtract( lightOrigin, texelWorldPosition, lightDirection );
					VectorNormalize( lightDirection );
				}
				
				float lDotN = DotProduct( lightDirection, MSurf_Plane( surfID ).normal );
				if (lDotN < 1e-3)
					lDotN = 1e-3;
				scale /= lDotN;

				int i;
				for( i = 1; i < NUM_BUMP_VECTS + 1; i++ )
				{
					float dot = DotProduct( lightDirection, pBumpBasis[i-1] );
					if( dot <= 0.0f )
						continue;
					
					VectorMA( blocklights[i][idx].AsVector3D(), dot * scale, intensity, 
						blocklights[i][idx].AsVector3D() );
				}
#else
				VectorMA( blocklights[1][idx].AsVector3D(), scale, intensity, blocklights[1][idx].AsVector3D() );
				VectorMA( blocklights[2][idx].AsVector3D(), scale, intensity, blocklights[2][idx].AsVector3D() );
				VectorMA( blocklights[3][idx].AsVector3D(), scale, intensity, blocklights[3][idx].AsVector3D() );
#endif
			}
		}

		// Move along u
		VectorMA( texelWorldPosition, fixupFactor, 
			pTexInfo->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D(), texelWorldPosition );

	}
}

//-----------------------------------------------------------------------------
// Compute the bumpmap basis for this surface
//-----------------------------------------------------------------------------
static void R_ComputeSurfaceBasis( SurfaceHandle_t surfID, Vector *pBumpNormals, Vector &luxelBasePosition )
{
	// Get the bump basis vects in the space of the surface.
	Vector sVect, tVect;
	VectorCopy( MSurf_TexInfo( surfID )->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D(), sVect );
	VectorNormalize( sVect );
	VectorCopy( MSurf_TexInfo( surfID )->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D(), tVect );
	VectorNormalize( tVect );
	GetBumpNormals( sVect, tVect, MSurf_Plane( surfID ).normal, MSurf_Plane( surfID ).normal, pBumpNormals );

	// Compute the location of the first luxel in worldspace

	// Since there's a scale factor used when going from world to luxel,
	// we gotta undo that scale factor when going from luxel to world
	float fixupFactor = 
		MSurf_TexInfo( surfID )->worldUnitsPerLuxel * 
		MSurf_TexInfo( surfID )->worldUnitsPerLuxel;

	// The starting u of the surface is surf->lightmapMins[0];
	// since N * P + D = u, N * P = u - D, therefore we gotta move (u-D) along uvec
	VectorMultiply( MSurf_TexInfo( surfID )->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D(),
		(MSurf_LightmapMins( surfID )[0] - MSurf_TexInfo( surfID )->lightmapVecsLuxelsPerWorldUnits[0][3]) * fixupFactor,
		luxelBasePosition );

	// Do the same thing for the v direction.
	VectorMA( luxelBasePosition, 
		(MSurf_LightmapMins( surfID )[1] - 
		MSurf_TexInfo( surfID )->lightmapVecsLuxelsPerWorldUnits[1][3]) * fixupFactor,
		MSurf_TexInfo( surfID )->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D(),
		luxelBasePosition );

	// Move out in the direction of the plane normal...
	VectorMA( luxelBasePosition, MSurf_Plane( surfID ).dist, MSurf_Plane( surfID ).normal, luxelBasePosition ); 
}

//-----------------------------------------------------------------------------
// Purpose: Compute the mask of which dlights affect a surface
//			NOTE: Also has the side effect of updating the surface lighting dlight flags!
//-----------------------------------------------------------------------------
unsigned int R_ComputeDynamicLightMask( dlight_t *pLights, SurfaceHandle_t surfID, msurfacelighting_t *pLighting, const matrix3x4_t& entityToWorld )
{
	ASSERT_SURF_VALID( surfID );
	Vector bumpNormals[3];
	Vector luxelBasePosition;

	// Displacements do dynamic lights different
	if( SurfaceHasDispInfo( surfID ) )
	{
		return MSurf_DispInfo( surfID )->ComputeDynamicLightMask(pLights);
	}

	if ( !g_bActiveDlights )
		return 0;

	int lightMask = 0;
	for ( int lnum = 0, testBit = 1, mask = r_dlightactive; lnum < MAX_DLIGHTS; lnum++, mask >>= 1, testBit <<= 1 )
	{
		if ( mask & 1 )
		{
			// not lit by this light
			if ( !(pLighting->m_fDLightBits & testBit ) )
				continue;

			// This light doesn't affect the world
			if ( pLights[lnum].flags & (DLIGHT_NO_WORLD_ILLUMINATION|DLIGHT_DISPLACEMENT_MASK))
				continue;

			// This is used to ensure a maximum number of dlights in a frame
			if ( !R_CanUseVisibleDLight( lnum ) ) 
				continue;

			// Cull surface to light radius
			Vector lightOrigin;

			VectorITransform( pLights[lnum].origin, entityToWorld, lightOrigin );

			// NOTE: Dist can be negative because muzzle flashes can actually get behind walls
			// since the gun isn't checked for collision tests.
			float perpDistSq = DotProduct (lightOrigin, MSurf_Plane( surfID ).normal) - MSurf_Plane( surfID ).dist;
			if (perpDistSq < DLIGHT_BEHIND_PLANE_DIST)
			{
				// update the surfacelighting and remove this light's bit
				pLighting->m_fDLightBits &= ~testBit;
				continue;
			}

			perpDistSq *= perpDistSq;

			// If the perp distance > radius of light, blow it off
			float lightRadiusSq = pLights[lnum].GetRadiusSquared();
			if (lightRadiusSq <= perpDistSq)
			{
				// update the surfacelighting and remove this light's bit
				pLighting->m_fDLightBits &= ~testBit;
				continue;
			}

			lightMask |= testBit;
		}
	}

	return lightMask;
}


//-----------------------------------------------------------------------------
// Purpose: Modifies blocklights[][][] to include the state of the dlights 
//			affecting this surface.
//			NOTE: Can be threaded, should not reference or modify any global state 
//			other than blocklights.
//-----------------------------------------------------------------------------
void R_AddDynamicLights( dlight_t *pLights, SurfaceHandle_t surfID, const matrix3x4_t& entityToWorld, bool needsBumpmap, unsigned int lightMask )
{
	ASSERT_SURF_VALID( surfID );
	VPROF( "R_AddDynamicLights" );

	Vector bumpNormals[3];
	bool computedBumpBasis = false;
	Vector luxelBasePosition;

	// Displacements do dynamic lights different
	if( SurfaceHasDispInfo( surfID ) )
	{
		MSurf_DispInfo( surfID )->AddDynamicLights(pLights, lightMask);
		return;
	}

	// iterate all of the active dynamic lights.  Uses several iterators to keep 
	// the light mask (bit), light index, and active mask current
	for ( int lnum = 0, testBit = 1, mask = lightMask; lnum < MAX_DLIGHTS && mask != 0; lnum++, mask >>= 1, testBit <<= 1 )
	{
		// shift over the mask of active lights each iteration, if this one is active, apply it
		if ( mask & 1 )
		{
			// Cull surface to light radius
			Vector lightOrigin;

			VectorITransform( pLights[lnum].origin, entityToWorld, lightOrigin );

			// NOTE: Dist can be negative because muzzle flashes can actually get behind walls
			// since the gun isn't checked for collision tests.
			float perpDistSq = DotProduct (lightOrigin, MSurf_Plane( surfID ).normal) - MSurf_Plane( surfID ).dist;
			if (perpDistSq < DLIGHT_BEHIND_PLANE_DIST)
				continue;

			perpDistSq *= perpDistSq;

			// If the perp distance > radius of light, blow it off
			float lightRadiusSq = pLights[lnum].GetRadiusSquared();
			if (lightRadiusSq <= perpDistSq)
				continue;

			if (!needsBumpmap)
			{
				AddSingleDynamicLight( pLights[lnum], surfID, lightOrigin, perpDistSq, lightRadiusSq );
				continue;
			}

			// Here, I'm precomputing things needed by bumped lighting that
			// are the same for a surface...
			if (!computedBumpBasis)
			{
				R_ComputeSurfaceBasis( surfID, bumpNormals, luxelBasePosition );
				computedBumpBasis = true;
			}

			AddSingleDynamicLightToBumpLighting( pLights[lnum], surfID, lightOrigin, perpDistSq, lightRadiusSq, bumpNormals, luxelBasePosition );
		}
	}
}


// Fixed point (8.8) color/intensity ratios
#define I_RED		((int)(0.299*255))
#define I_GREEN		((int)(0.587*255))
#define I_BLUE		((int)(0.114*255))


//-----------------------------------------------------------------------------
// Sets all elements in a lightmap to a particular opaque greyscale value
//-----------------------------------------------------------------------------
static void InitLMSamples( Vector4D *pSamples, int nSamples, float value )
{
	for( int i=0; i < nSamples; i++ )
	{
		pSamples[i][0] = pSamples[i][1] = pSamples[i][2] = value;
		pSamples[i][3] = 1.0f;
	}
}


//-----------------------------------------------------------------------------
// Computes the lightmap size
//-----------------------------------------------------------------------------
static int ComputeLightmapSize( SurfaceHandle_t surfID )
{
	int smax = ( MSurf_LightmapExtents( surfID )[0] ) + 1;
	int tmax = ( MSurf_LightmapExtents( surfID )[1] ) + 1;
	int size = smax * tmax;

	int nMaxSize = MSurf_MaxLightmapSizeWithBorder( surfID );
	if (size > nMaxSize * nMaxSize)
	{
		ConMsg("Bad lightmap extents on material \"%s\"\n", 
			materialSortInfoArray[MSurf_MaterialSortID( surfID )].material->GetName());
		return 0;
	}
	
	return size;
}


#ifndef _X360
//-----------------------------------------------------------------------------
// Compute the portion of the lightmap generated from lightstyles
//-----------------------------------------------------------------------------
static void AccumulateLightstyles( ColorRGBExp32* pLightmap, int lightmapSize, float scalar ) 
{
	Assert( pLightmap );
	for (int i=0; i<lightmapSize ; ++i)
	{
		blocklights[0][i][0] += scalar * TexLightToLinear( pLightmap[i].r, pLightmap[i].exponent );
		blocklights[0][i][1] += scalar * TexLightToLinear( pLightmap[i].g, pLightmap[i].exponent );
		blocklights[0][i][2] += scalar * TexLightToLinear( pLightmap[i].b, pLightmap[i].exponent );
	}
}

static void AccumulateLightstylesFlat( ColorRGBExp32* pLightmap, int lightmapSize, float scalar ) 
{
	Assert( pLightmap );
	for (int i=0; i<lightmapSize ; ++i)
	{
		blocklights[0][i][0] += scalar * TexLightToLinear( pLightmap->r, pLightmap->exponent );
		blocklights[0][i][1] += scalar * TexLightToLinear( pLightmap->g, pLightmap->exponent );
		blocklights[0][i][2] += scalar * TexLightToLinear( pLightmap->b, pLightmap->exponent );
	}
}


static void AccumulateBumpedLightstyles( ColorRGBExp32* pLightmap, int lightmapSize, float scalar ) 
{
	ColorRGBExp32 *pBumpedLightmaps[3];
	pBumpedLightmaps[0] = pLightmap + lightmapSize;
	pBumpedLightmaps[1] = pLightmap + 2 * lightmapSize;
	pBumpedLightmaps[2] = pLightmap + 3 * lightmapSize;

	// I chose to split up the loops this way because it was the best tradeoff
	// based on profiles between cache miss + loop overhead
	for (int i=0 ; i<lightmapSize ; ++i)
	{
		blocklights[0][i][0] += scalar * TexLightToLinear( pLightmap[i].r, pLightmap[i].exponent );
		blocklights[0][i][1] += scalar * TexLightToLinear( pLightmap[i].g, pLightmap[i].exponent );
		blocklights[0][i][2] += scalar * TexLightToLinear( pLightmap[i].b, pLightmap[i].exponent );
		Assert( blocklights[0][i][0] >= 0.0f );
		Assert( blocklights[0][i][1] >= 0.0f );
		Assert( blocklights[0][i][2] >= 0.0f );

		blocklights[1][i][0] += scalar * TexLightToLinear( pBumpedLightmaps[0][i].r, pBumpedLightmaps[0][i].exponent );
		blocklights[1][i][1] += scalar * TexLightToLinear( pBumpedLightmaps[0][i].g, pBumpedLightmaps[0][i].exponent );
		blocklights[1][i][2] += scalar * TexLightToLinear( pBumpedLightmaps[0][i].b, pBumpedLightmaps[0][i].exponent );
		Assert( blocklights[1][i][0] >= 0.0f );
		Assert( blocklights[1][i][1] >= 0.0f );
		Assert( blocklights[1][i][2] >= 0.0f );
	}
	for ( int i=0 ; i<lightmapSize ; ++i)
	{
		blocklights[2][i][0] += scalar * TexLightToLinear( pBumpedLightmaps[1][i].r, pBumpedLightmaps[1][i].exponent );
		blocklights[2][i][1] += scalar * TexLightToLinear( pBumpedLightmaps[1][i].g, pBumpedLightmaps[1][i].exponent );
		blocklights[2][i][2] += scalar * TexLightToLinear( pBumpedLightmaps[1][i].b, pBumpedLightmaps[1][i].exponent );
		Assert( blocklights[2][i][0] >= 0.0f );
		Assert( blocklights[2][i][1] >= 0.0f );
		Assert( blocklights[2][i][2] >= 0.0f );

		blocklights[3][i][0] += scalar * TexLightToLinear( pBumpedLightmaps[2][i].r, pBumpedLightmaps[2][i].exponent );
		blocklights[3][i][1] += scalar * TexLightToLinear( pBumpedLightmaps[2][i].g, pBumpedLightmaps[2][i].exponent );
		blocklights[3][i][2] += scalar * TexLightToLinear( pBumpedLightmaps[2][i].b, pBumpedLightmaps[2][i].exponent );
		Assert( blocklights[3][i][0] >= 0.0f );
		Assert( blocklights[3][i][1] >= 0.0f );
		Assert( blocklights[3][i][2] >= 0.0f );
	}
}
#else
/*
// unpack four ColorRGBExp32's loaded into a single vector register
// into four. Can't do this as a function coz you can't return four 
// values and even the inliner falls down on pass-by-ref.
#define UNPACK_COLORRGBEXP(fromVec, toVec0, toVec1, toVec2, toVec3) {\
	
}
*/

// because the e component of the colors is signed, we need to mask
// off the corresponding channel in the intermediate halfword expansion
// when we combine it with the unsigned unpack for the other channels
static const int32 ALIGN16 g_SIMD_HalfWordMask[4]= {  0x0000000, 0x0000FFFF, 0x0000000, 0x0000FFFF };
static const fltx4 vOneOverTwoFiftyFive = { 1.0f / 255.0f , 1.0f / 255.0f , 1.0f / 255.0f , 1.0f / 255.0f };

// grind through accumlating onto the blocklights, 
// one cache line at a time. Input pointers are assumed
// to be cache aligned.
// For a simpler reference implementation, see the PC version in the ifdef above.
// This function makes heavy use of the special XBOX360 opcodes for
// packing and unpacking integer d3d data. (Not available in SSE, sadly.)
static void AccumulateLightstyles_EightAtAtime( ColorRGBExp32* RESTRICT pLightmap, // the input lightmap (not necessarily aligned)
													  int lightmapSize, 
													  fltx4 vScalar,
													  Vector4D * RESTRICT bLights // pointer to the blocklights row we'll be writing into -- should be cache aligned, but only hurts perf if it's not
													 )
{
	// We process blockLights in groups of four at a time, because we load the pLightmap four
	// at a time (four words fit into a vector register). 
	// On top of that, we do two groups at once, because that's the length
	// of a cache line, and it helps us better hide latency.
	AssertMsg((lightmapSize & 7) == 0, "Input to Accumulate...EightAtATime not divisible by eight. Data corruption is the likely result." );

	const fltx4 vHalfWordMask = XMLoadVector4A(g_SIMD_HalfWordMask);

	fltx4 zero = Four_Zeros;
	for (int i = 0 ; i < lightmapSize ; i += 8 )
	{
		// cache prefetch two lines ahead on bLights, and one on pLightmap
		__dcbt(256, bLights);
		__dcbt(128, pLightmap);

		// the naming convention on these psuedoarrays (they are actually
		// registers) is that the number before the index is the group id,
		// and the index itself is which word in the group. If this seems
		// unclear to you, feel free to just use array indices 0..7
		// The compiler doesn't seem to deal properly with multidim arrays
		// (at least in the sense of aliasing them to registers)
		// However, if you always access through the arrays by using
		// compile-time immediate constants (eg, foo[2] rather than
		// int x = 2; foo[x]
		// it will at least treat them as register variables.

		// load four blockLights entries, and four colors
		fltx4 vLight0[4], vLight1[4];
		fltx4 colorLightMap0[4], colorLightMap1[4]; 

		fltx4 bytePackedLightMap0 = XMLoadVector4(pLightmap+i); // because each colorrgbexp is actually a 32-bit struct,
		// this loads four of them into one vector -- they are ubytes for rgb and sbyte for e
		fltx4 bytePackedLightMap1 = XMLoadVector4(pLightmap+i+4);

		// load group 0
		vLight0[0] = LoadAlignedSIMD( &(bLights + i + 0)->x );
		vLight0[1] = LoadAlignedSIMD( &(bLights + i + 1)->x );
		vLight0[2] = LoadAlignedSIMD( &(bLights + i + 2)->x );
		vLight0[3] = LoadAlignedSIMD( &(bLights + i + 3)->x );

			// load group 1
			vLight1[0] = LoadAlignedSIMD( &(bLights + i + 4)->x );
			vLight1[1] = LoadAlignedSIMD( &(bLights + i + 5)->x );
			vLight1[2] = LoadAlignedSIMD( &(bLights + i + 6)->x );
			vLight1[3] = LoadAlignedSIMD( &(bLights + i + 7)->x );

		// unpack the color light maps now that they have loaded
		// interleaving (four-vector) group 0 and 1
			
		// unpack rgbe 0 and 1:
		// like an unsigned unpack: { 0x00, colorLightMap[0].r, 0x00, colorLightMap[0].g, 0x00, colorLightMap[0].b, 0x00, colorLightMap[0].e, 
		//							  0x00, colorLightMap[1].r, 0x00, colorLightMap[1].g, 0x00, colorLightMap[1].b, 0x00, colorLightMap[1].e} 
		fltx4 unsignedUnpackHi0 = __vmrghb(zero, bytePackedLightMap0); // GROUP 0
		fltx4 unsignedUnpackLo0 = __vmrglb(zero, bytePackedLightMap0); // rgbe words 2 and 3
			fltx4 unsignedUnpackHi1 = __vmrghb(zero, bytePackedLightMap1); // GROUP 1
			fltx4 unsignedUnpackLo1 = __vmrglb(zero, bytePackedLightMap1); // rgbe words 2 and 3

		fltx4 signedUnpackHi0 = __vupkhsb(bytePackedLightMap0); // signed unpack of words 0 and 1, like the unsigned unpack but replaces 0x00 w/ sign extension
		fltx4 signedUnpackLo0 = __vupklsb(bytePackedLightMap0); // GROUP 0
			fltx4 signedUnpackHi1 = __vupkhsb(bytePackedLightMap1); // signed unpack of words 0 and 1, like the unsigned unpack but replaces 0x00 w/ sign extension
			fltx4 signedUnpackLo1 = __vupklsb(bytePackedLightMap1); // GROUP 1

		// merge the signed and unsigned unpacks together to make the full halfwords
		unsignedUnpackHi0 = MaskedAssign(vHalfWordMask, signedUnpackHi0, unsignedUnpackHi0 );
		unsignedUnpackLo0 = MaskedAssign(vHalfWordMask, signedUnpackLo0, unsignedUnpackLo0 );
			unsignedUnpackHi1 = MaskedAssign(vHalfWordMask, signedUnpackHi1, unsignedUnpackHi1 );
			unsignedUnpackLo1 = MaskedAssign(vHalfWordMask, signedUnpackLo1, unsignedUnpackLo1 );

		// now complete the unpack from halfwords to words (we can just use signed because there are 0x00's above the rgb channels)
		colorLightMap0[0] = __vupkhsh(unsignedUnpackHi0); // vector unpack high signed halfword
		colorLightMap0[1] = __vupklsh(unsignedUnpackHi0); // vector unpack low signed halfword
		colorLightMap0[2] = __vupkhsh(unsignedUnpackLo0);
		colorLightMap0[3] = __vupklsh(unsignedUnpackLo0);
		colorLightMap0[0] =  __vcfsx( colorLightMap0[0], 0); // convert to floats
			colorLightMap1[0] = __vupkhsh(unsignedUnpackHi1); // interleave group 1 unpacks
		colorLightMap0[1] =  __vcfsx( colorLightMap0[1], 0); // convert to floats
			colorLightMap1[1] = __vupklsh(unsignedUnpackHi1);	// should dual issue
		colorLightMap0[2] =  __vcfsx( colorLightMap0[2], 0); // convert to floats
			colorLightMap1[2] = __vupkhsh(unsignedUnpackLo1);
		colorLightMap0[3] =  __vcfsx( colorLightMap0[3], 0); // convert to floats
			colorLightMap1[3] = __vupklsh(unsignedUnpackLo1);

		// finish unpacking group 1 (giving group 0 time to finish converting)
			colorLightMap1[0] = __vcfsx( colorLightMap1[0], 0);
			colorLightMap1[1] = __vcfsx( colorLightMap1[1], 0);
			colorLightMap1[2] = __vcfsx( colorLightMap1[2], 0);
			colorLightMap1[3] = __vcfsx( colorLightMap1[3], 0);

		// manufacture exponent splats and start normalizing the rgb channels (eg *= 1/255)
		fltx4 expW0[4], expW1[4];
		expW0[0] = XMVectorSplatW(colorLightMap0[0]);
		colorLightMap0[0] = MulSIMD(colorLightMap0[0], vOneOverTwoFiftyFive); // normalize the rgb channels
		expW0[1] = XMVectorSplatW(colorLightMap0[1]);
		colorLightMap0[1] = MulSIMD(colorLightMap0[1], vOneOverTwoFiftyFive); // normalize the rgb channels
		expW0[2] = XMVectorSplatW(colorLightMap0[2]);
		colorLightMap0[2] = MulSIMD(colorLightMap0[2], vOneOverTwoFiftyFive); // normalize the rgb channels
		expW0[3] = XMVectorSplatW(colorLightMap0[3]);
		colorLightMap0[3] = MulSIMD(colorLightMap0[3], vOneOverTwoFiftyFive); // normalize the rgb channels
		
		// scale each of the color channels by the exponent channel
		// (the estimate operation is exact for integral inputs, as here)
		expW0[0] = XMVectorExpEst( expW0[0] ); // x = 2^x
			expW1[0] = XMVectorSplatW(colorLightMap1[0]); // interleave splats on exp group 1 (dual issue)
			colorLightMap1[0] = MulSIMD(colorLightMap1[0], vOneOverTwoFiftyFive); // normalize the rgb channels
		expW0[1] = XMVectorExpEst( expW0[1] );
			expW1[1] = XMVectorSplatW(colorLightMap1[1]);
			colorLightMap1[1] = MulSIMD(colorLightMap1[1], vOneOverTwoFiftyFive); // normalize the rgb channels
		expW0[2] = XMVectorExpEst( expW0[2] );
			expW1[2] = XMVectorSplatW(colorLightMap1[2]);
			colorLightMap1[2] = MulSIMD(colorLightMap1[2], vOneOverTwoFiftyFive); // normalize the rgb channels
		expW0[3] = XMVectorExpEst( expW0[3] );
			expW1[3] = XMVectorSplatW(colorLightMap1[3]);
			colorLightMap1[3] = MulSIMD(colorLightMap1[3], vOneOverTwoFiftyFive); // normalize the rgb channels

		// finish scale-by-exponent on group 1
			expW1[0] = XMVectorExpEst( expW1[0] );
			expW1[1] = XMVectorExpEst( expW1[1] );
			expW1[2] = XMVectorExpEst( expW1[2] );
			expW1[3] = XMVectorExpEst( expW1[3] );

		colorLightMap0[0] = MulSIMD(expW0[0], colorLightMap0[0]);
		colorLightMap0[1] = MulSIMD(expW0[1], colorLightMap0[1]);
		colorLightMap0[2] = MulSIMD(expW0[2], colorLightMap0[2]);
		colorLightMap0[3] = MulSIMD(expW0[3], colorLightMap0[3]);
			colorLightMap1[0] = MulSIMD(expW1[0], colorLightMap1[0]);
			colorLightMap1[1] = MulSIMD(expW1[1], colorLightMap1[1]);
			colorLightMap1[2] = MulSIMD(expW1[2], colorLightMap1[2]);
			colorLightMap1[3] = MulSIMD(expW1[3], colorLightMap1[3]);

#ifdef X360_DOUBLECHECK_LIGHTMAPS
		for (int group = 0 ; group < 4 ; ++group)
		{
			Assert( colorLightMap0[group].v[0] == TexLightToLinear( pLightmap[i + group].r, pLightmap[i + group].exponent ) &&
					colorLightMap0[group].v[1] == TexLightToLinear( pLightmap[i + group].g, pLightmap[i + group].exponent ) &&
					colorLightMap0[group].v[2] == TexLightToLinear( pLightmap[i + group].b, pLightmap[i + group].exponent ) );
		}
#endif


		// accumulate into blocklights
		vLight0[0] = XMVectorMultiplyAdd(vScalar, colorLightMap0[0], vLight0[0]);
		vLight0[1] = XMVectorMultiplyAdd(vScalar, colorLightMap0[1], vLight0[1]);
		vLight0[2] = XMVectorMultiplyAdd(vScalar, colorLightMap0[2], vLight0[2]);
		vLight0[3] = XMVectorMultiplyAdd(vScalar, colorLightMap0[3], vLight0[3]);
			vLight1[0] = XMVectorMultiplyAdd(vScalar, colorLightMap1[0], vLight1[0]);
			vLight1[1] = XMVectorMultiplyAdd(vScalar, colorLightMap1[1], vLight1[1]);
			vLight1[2] = XMVectorMultiplyAdd(vScalar, colorLightMap1[2], vLight1[2]);
			vLight1[3] = XMVectorMultiplyAdd(vScalar, colorLightMap1[3], vLight1[3]);

		// save 
		XMStoreVector4A( bLights + i + 0, vLight0[0]);
		XMStoreVector4A( bLights + i + 1, vLight0[1]);
		XMStoreVector4A( bLights + i + 2, vLight0[2]);
		XMStoreVector4A( bLights + i + 3, vLight0[3]);
			XMStoreVector4A( bLights + i + 4, vLight1[0]);
			XMStoreVector4A( bLights + i + 5, vLight1[1]);
			XMStoreVector4A( bLights + i + 6, vLight1[2]);
			XMStoreVector4A( bLights + i + 7, vLight1[3]);
	}
}

// just like XMLoadByte4 only no asserts
FORCEINLINE XMVECTOR LoadSignedByte4NoAssert ( CONST XMBYTE4* pSource )
{
	XMVECTOR V;

	V = __lvlx(pSource, 0);
	V = __vupkhsb(V);
	V = __vupkhsh(V);
	V = __vcfsx(V, 0);
	
	return V;
}


//-----------------------------------------------------------------------------
// Compute the portion of the lightmap generated from lightstyles
//-----------------------------------------------------------------------------
static void AccumulateLightstyles( ColorRGBExp32* pLightmap, int lightmapSize, fltx4 vScalar ) 
{
	Assert( pLightmap );
	// crush w of the scalar to zero (so we don't overwrite blocklight[x][y][3] in the madds)
	vScalar = __vrlimi(vScalar, Four_Zeros, 1, 0);
	int lightmapSizeEightAligned = lightmapSize & (~0x07);

	// crunch as many groups of eight as possible, then deal with the remainder
	AccumulateLightstyles_EightAtAtime(pLightmap, lightmapSizeEightAligned, vScalar, blocklights[0]);

	// handle remainders
	for (int i = lightmapSizeEightAligned; i < lightmapSize ; ++i )
	{
		// load four blockLights entries, and four colors
		fltx4 vLight;
		fltx4 colorLightMap; 
		vLight = LoadAlignedSIMD(blocklights[0][i].Base());

		// unpack the color light maps
		// load the unsigned bytes
		colorLightMap = XMLoadUByte4(reinterpret_cast<XMUBYTE4 *>(pLightmap + i));
		// fish out the exponent component from a signed load
		fltx4 exponentiator = XMVectorExpEst(XMVectorSplatW(LoadSignedByte4NoAssert(reinterpret_cast<XMBYTE4 *>(pLightmap + i))));

		// scale each of the color light channels by the exponent
		colorLightMap = MulSIMD( MulSIMD(colorLightMap, vOneOverTwoFiftyFive ), exponentiator );

		Assert( colorLightMap.v[0] == TexLightToLinear( pLightmap[i].r, pLightmap[i].exponent ) &&
			colorLightMap.v[1] == TexLightToLinear( pLightmap[i].g, pLightmap[i].exponent ) &&
			colorLightMap.v[2] == TexLightToLinear( pLightmap[i].b, pLightmap[i].exponent ) );

		// accumulate onto blocklights
		vLight = MaddSIMD(vScalar, colorLightMap, vLight);

		StoreAlignedSIMD(blocklights[0][i].Base(), vLight);
	}
}

static void AccumulateLightstylesFlat( ColorRGBExp32* pLightmap, int lightmapSize, fltx4 vScalar ) 
{
	Assert( pLightmap );

	// this isn't a terribly fast way of doing things, but 
	// this function doesn't seem to be called much (so 
	// it's not worth the trouble of custom loop scheduling)
	fltx4 colorLightMap; 
	// unpack the color light maps
	// load the unsigned bytes
	colorLightMap = XMLoadUByte4(reinterpret_cast<XMUBYTE4 *>(pLightmap));
	// fish out the exponent component from a signed load
	fltx4 exponentiator = XMVectorExpEst(XMVectorSplatW(LoadSignedByte4NoAssert(reinterpret_cast<XMBYTE4 *>(pLightmap))));

	// scale each of the color light channels by the exponent
	colorLightMap = MulSIMD( MulSIMD(colorLightMap, vOneOverTwoFiftyFive ), exponentiator );

	for (int i = 0; i < lightmapSize ; ++i )
	{
		// load four blockLights entries, and four colors
		fltx4 vLight;
		vLight = LoadAlignedSIMD(blocklights[0][i].Base());

		// accumulate onto blocklights
		vLight = MaddSIMD(vScalar, colorLightMap, vLight);

		StoreAlignedSIMD(blocklights[0][i].Base(), vLight);
	}
}



static void AccumulateBumpedLightstyles( ColorRGBExp32* RESTRICT pLightmap, int lightmapSize, fltx4 vScalar ) 
{
	COMPILE_TIME_ASSERT(sizeof(ColorRGBExp32) == 4); // This function is carefully scheduled around four-byte colors

	// crush w of the scalar to zero (so we don't overwrite blocklight[x][y][3] in the madds)
	vScalar = __vrlimi(vScalar, Four_Zeros, 1, 0);

	/*
	ColorRGBExp32 * RESTRICT pBumpedLightmaps[3];
	pBumpedLightmaps[1] = pLightmap + lightmapSize;
	pBumpedLightmaps[2] = pLightmap + 2 * lightmapSize;
	pBumpedLightmaps[3] = pLightmap + 3 * lightmapSize;
	*/
	
	// assert word (not vector) alignment
	AssertMsg( ((reinterpret_cast<unsigned int>(pLightmap) & 0x03 ) == 0), "Lightmap was not word-aligned: AccumulateBumpedLightstyles must fail." );
	// assert vector alignment
	AssertMsg( (reinterpret_cast<unsigned int>(blocklights) & 0x0F ) == 0, "Blocklights is not vector-aligned. You're doomed." );
	AssertMsg( (reinterpret_cast<unsigned int>(blocklights) & 127 ) == 0, "Blocklights is not cache-aligned. Performance will suffer." );

#if 0 // reference: This is the simple version -- four-way accumulate (no interleaving)
	for (int i = 0 ; i < lightmapSize ; i+= 4)
	{
		// load four blockLights entries, and four colors
		fltx4 vLight[4];
		fltx4 colorLightMap[4]; 
		vLight[0] = LoadUnalignedSIMD(&blocklights[0][i]);
		vLight[1] = LoadUnalignedSIMD(&blocklights[0][i+1]);
		vLight[2] = LoadUnalignedSIMD(&blocklights[0][i+2]);
		vLight[3] = LoadUnalignedSIMD(&blocklights[0][i+3]);
		// unpack the color light maps
		{
			fltx4 zero = Four_Zeros;
			fltx4 colorLightmap = LoadUnalignedSIMD(pLightmap+i); // because each colorrgbexp is actually a 32-bit struct,
			// this loads four of them into one vector -- they are ubytes for rgb and sbyte for e
			// unpack rgbe 0 and 1:
			// like an unsigned unpack: { 0x00, colorLightMap[0].r, 0x00, colorLightMap[0].g, 0x00, colorLightMap[0].b, 0x00, colorLightMap[0].e, 
			//							  0x00, colorLightMap[1].r, 0x00, colorLightMap[1].g, 0x00, colorLightMap[1].b, 0x00, colorLightMap[1].e} 
			fltx4 unsignedUnpackHi = __vmrghb(zero, colorLightMap);
			fltx4 unsignedUnpackLo = __vmrghb(zero, colorLightMap); // rgbe words 2 and 3
			fltx4 signedUnpackHi = __vupkhsb(colorLightMap); // signed unpack of words 0 and 1, like the unsigned unpack but repl 0x00 w/ sign extension
			fltx4 signedUnpackLo = __vupklsb(colorLightMap);
			// merge the signed and unsigned unpacks together to make the full halfwords
			unsignedUnpackHi = MaskedAssign(vHalfWordMask, signedUnpackHi, unsignedUnpackHi );
			unsignedUnpackLo = MaskedAssign(vHalfWordMask, signedUnpackLo, unsignedUnpackLo );
			// now complete the unpack from halfwords to words (we can just use signed because there are 0x00's above the rgb channels)
			// and convert to float
			colorLightMap[0] = __vcfsx( __vupkhsh(unsignedUnpackHi), 0);
			colorLightMap[1] = __vcfsx( __vupklsh(unsignedUnpackHi), 0);
			colorLightMap[2] = __vcfsx( __vupkhsh(unsignedUnpackLo), 0);
			colorLightMap[3] = __vcfsx( __vupklsh(unsignedUnpackLo), 0);
		}

		// scale each of the color channels by the exponent channel
		colorLightMap[0] = XMVectorExpEst( XMVectorSplatW(colorLightMap[0]) );
		colorLightMap[1] = XMVectorExpEst( XMVectorSplatW(colorLightMap[1]) );
		colorLightMap[2] = XMVectorExpEst( XMVectorSplatW(colorLightMap[2]) );
		colorLightMap[3] = XMVectorExpEst( XMVectorSplatW(colorLightMap[3]) );

		// accumulate into blocklights
		vLight[0] = XMVectorMultiplyAdd(vScalar, colorLightMap[0], vLight[0]);
		vLight[1] = XMVectorMultiplyAdd(vScalar, colorLightMap[1], vLight[1]);
		vLight[2] = XMVectorMultiplyAdd(vScalar, colorLightMap[2], vLight[2]);
		vLight[3] = XMVectorMultiplyAdd(vScalar, colorLightMap[3], vLight[3]);

		// save 
		XMStoreVector4(&blocklights[0][i], vLight[0]);
		XMStoreVector4(&blocklights[1][i], vLight[1]);
		XMStoreVector4(&blocklights[2][i], vLight[2]);
		XMStoreVector4(&blocklights[3][i], vLight[3]);
	}
#endif

	int lightmapSizeEightAligned = lightmapSize & (~0x07);

	// crunch each of the lightmap groups.
	for (int mapGroup = 0 ; mapGroup <= 3 ; ++mapGroup, pLightmap += lightmapSize )
	{
		// process the base lightmap
		AccumulateLightstyles_EightAtAtime(pLightmap, lightmapSizeEightAligned, vScalar, blocklights[mapGroup]);
		// handle remainders
		for (int i = lightmapSizeEightAligned; i < lightmapSize ; ++i )
		{
			// load four blockLights entries, and four colors
			fltx4 vLight;
			fltx4 colorLightMap; 
			vLight = LoadAlignedSIMD(blocklights[mapGroup][i].Base());

			// unpack the color light maps
			// load the unsigned bytes
			colorLightMap = XMLoadUByte4(reinterpret_cast<XMUBYTE4 *>(pLightmap + i));
			// fish out the exponent component from a signed load
			fltx4 exponentiator = XMVectorExpEst(XMVectorSplatW(LoadSignedByte4NoAssert(reinterpret_cast<XMBYTE4 *>(pLightmap + i))));

			// scale each of the color light channels by the exponent
			colorLightMap = MulSIMD( MulSIMD(colorLightMap, vOneOverTwoFiftyFive ), exponentiator );

			Assert( colorLightMap.v[0] == TexLightToLinear( pLightmap[i].r, pLightmap[i].exponent ) &&
				colorLightMap.v[1] == TexLightToLinear( pLightmap[i].g, pLightmap[i].exponent ) &&
				colorLightMap.v[2] == TexLightToLinear( pLightmap[i].b, pLightmap[i].exponent ) );


			// accumulate onto blocklights
			vLight = MaddSIMD(vScalar, colorLightMap, vLight);
			
			StoreAlignedSIMD(blocklights[mapGroup][i].Base(), vLight);
		}

		// note: pLightmap is incremented as well.
	}
}
#endif

//-----------------------------------------------------------------------------
// Compute the portion of the lightmap generated from lightstyles
//-----------------------------------------------------------------------------
static void ComputeLightmapFromLightstyle( msurfacelighting_t *pLighting, bool computeLightmap, 
				bool computeBumpmap, int lightmapSize, bool hasBumpmapLightmapData )
{
	VPROF( "ComputeLightmapFromLightstyle" );

	ColorRGBExp32 *pLightmap = pLighting->m_pSamples;

	// Compute iteration range
	int minmap, maxmap;
#ifdef USE_CONVARS
	if( r_lightmap.GetInt() != -1 )
	{
		minmap = r_lightmap.GetInt();
		maxmap = minmap + 1;
	}
	else
#endif
	{
		minmap = 0; maxmap = MAXLIGHTMAPS;
	}

	for (int maps = minmap; maps < maxmap && pLighting->m_nStyles[maps] != 255; ++maps)
	{
		if( r_lightstyle.GetInt() != -1 && pLighting->m_nStyles[maps] != r_lightstyle.GetInt())
		{
			continue;
		}

		float fscalar = LightStyleValue( pLighting->m_nStyles[maps] );

		// hack - don't know why we are getting negative values here.
//		if (scalar > 0.0f && maps > 0 )
		if (fscalar > 0.0f)
		{
#ifdef _X360
			fltx4 scalar = ReplicateX4(fscalar); // we use SIMD versions of these functions on 360
#else
			const float &scalar = fscalar;
#endif

			if( computeBumpmap )
			{
				AccumulateBumpedLightstyles( pLightmap, lightmapSize, scalar );
			}
			else if( computeLightmap )
			{
				if (r_avglightmap.GetInt())
				{
					pLightmap = pLighting->AvgLightColor(maps);
					AccumulateLightstylesFlat( pLightmap, lightmapSize, scalar );
				}
				else
				{
					AccumulateLightstyles( pLightmap, lightmapSize, scalar );
				}
			}
		}

		// skip to next lightmap. If we store lightmap data, we need to jump forward 4
		pLightmap += hasBumpmapLightmapData ? lightmapSize * ( NUM_BUMP_VECTS + 1 ) : lightmapSize;
	}
}

// instrumentation to measure locks
/*
static CUtlVector<int> g_LightmapLocks;
static int g_Lastdlightframe = -1;
static int g_lastlock = -1;
static int g_unsorted = 0;
void MarkPage( int pageID )
{
	if ( g_Lastdlightframe != r_framecount )
	{
		int total = 0;
		int locks = 0;
		for ( int i = 0; i < g_LightmapLocks.Count(); i++ )
		{
			int count = g_LightmapLocks[i];
			if ( count )
			{
				total++;
				locks += count;
			}
			g_LightmapLocks[i] = 0;
		}
		g_Lastdlightframe = r_framecount;
		g_lastlock = -1;
		if ( locks )
		Msg("Total pages %d, locks %d, unsorted locks %d\n", total, locks, g_unsorted );
		g_unsorted = 0;
	}
	if ( pageID != g_lastlock )
	{
		g_lastlock = pageID;
		g_unsorted++;
	}
	g_LightmapLocks.EnsureCount(pageID+1);
	g_LightmapLocks[pageID]++;
}
*/
//-----------------------------------------------------------------------------
// Update the lightmaps...
//-----------------------------------------------------------------------------
static void UpdateLightmapTextures( SurfaceHandle_t surfID, bool needsBumpmap )
{
	ASSERT_SURF_VALID( surfID );

	if( materialSortInfoArray )
	{
		int lightmapSize[2];
		int offsetIntoLightmapPage[2];
		lightmapSize[0] = ( MSurf_LightmapExtents( surfID )[0] ) + 1;
		lightmapSize[1] = ( MSurf_LightmapExtents( surfID )[1] ) + 1;
		offsetIntoLightmapPage[0] = MSurf_OffsetIntoLightmapPage( surfID )[0];
		offsetIntoLightmapPage[1] = MSurf_OffsetIntoLightmapPage( surfID )[1];
		Assert( MSurf_MaterialSortID( surfID ) >= 0 && 
			MSurf_MaterialSortID( surfID ) < g_WorldStaticMeshes.Count() );
		// FIXME: Should differentiate between bumped and unbumped since the perf characteristics
		// are completely different?
//		MarkPage( materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID );

		if( needsBumpmap )
		{
			materials->UpdateLightmap( materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID,
				lightmapSize, offsetIntoLightmapPage, 
				&blocklights[0][0][0], &blocklights[1][0][0], &blocklights[2][0][0], &blocklights[3][0][0] );
		}
		else
		{
			materials->UpdateLightmap( materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID,
				lightmapSize, offsetIntoLightmapPage, 
				&blocklights[0][0][0], NULL, NULL, NULL );
		}
	}
}


unsigned int R_UpdateDlightState( dlight_t *pLights, SurfaceHandle_t surfID, const matrix3x4_t& entityToWorld, bool bOnlyUseLightStyles, bool bLightmap )
{
	unsigned int dlightMask = 0;
	// Mark the surface with the particular cached light values...
	msurfacelighting_t *pLighting = SurfaceLighting( surfID );

	// Retire dlights that are no longer active
	pLighting->m_fDLightBits &= r_dlightactive;
	pLighting->m_nLastComputedFrame = r_framecount;

	// Here, it's got the data it needs. So use it!
	if ( !bOnlyUseLightStyles )
	{
		// add all the dynamic lights
		if( bLightmap && ( pLighting->m_nDLightFrame == r_framecount ) )
		{
			dlightMask = R_ComputeDynamicLightMask( pLights, surfID, pLighting, entityToWorld );
		}

		if ( !dlightMask || !pLighting->m_fDLightBits )
		{
			pLighting->m_fDLightBits = 0;
			MSurf_Flags(surfID) &= ~SURFDRAW_HASDLIGHT;
		}
	}
	return dlightMask;
}

//-----------------------------------------------------------------------------
// Purpose: Build the blocklights array for a given surface and copy to dest
//			Combine and scale multiple lightmaps into the 8.8 format in blocklights
// Input  : *psurf - surface to rebuild
//			*dest - texture pointer to receive copy in lightmap texture format
//			stride - stride of *dest memory
//-----------------------------------------------------------------------------
void R_BuildLightMapGuts( dlight_t *pLights, SurfaceHandle_t surfID, const matrix3x4_t& entityToWorld, unsigned int dlightMask, bool needsBumpmap, bool needsLightmap )
{
	VPROF_("R_BuildLightMapGuts", 1, VPROF_BUDGETGROUP_DLIGHT_RENDERING, false, 0);
	int bumpID;

	// Lightmap data can be dumped to save memory - this precludes any dynamic lighting on the world
	Assert( !host_state.worldbrush->unloadedlightmaps );

	// Mark the surface with the particular cached light values...
	msurfacelighting_t *pLighting = SurfaceLighting( surfID );

	int size = ComputeLightmapSize( surfID );
	if (size == 0)
		return;

	bool hasBumpmap = SurfHasBumpedLightmaps( surfID );
	bool hasLightmap = SurfHasLightmap( surfID );

	// clear to no light
	if( needsLightmap )
	{
		// set to full bright if no light data
		InitLMSamples( blocklights[0], size, hasLightmap ? 0.0f : 1.0f );
	}

	if( needsBumpmap )
	{
		// set to full bright if no light data
		for( bumpID = 1; bumpID < NUM_BUMP_VECTS + 1; bumpID++ )
		{
			InitLMSamples( blocklights[bumpID], size, hasBumpmap ? 0.0f : 1.0f );
		}
	}

	// add all the lightmaps
	// Here, it's got the data it needs. So use it!
	if( ( hasLightmap && needsLightmap ) || ( hasBumpmap && needsBumpmap ) )
	{
		ComputeLightmapFromLightstyle( pLighting, ( hasLightmap && needsLightmap ),
			( hasBumpmap && needsBumpmap ), size, hasBumpmap );
	}
	else if( !hasBumpmap && needsBumpmap && hasLightmap )
	{
		// make something up for the bumped lights if you need them but don't have the data
		// if you have a lightmap, use that, otherwise fullbright
		ComputeLightmapFromLightstyle( pLighting, true, false, size, hasBumpmap );

		for( bumpID = 0; bumpID < ( hasBumpmap ? ( NUM_BUMP_VECTS + 1 ) : 1 ); bumpID++ )
		{
			for (int i=0 ; i<size ; i++)
			{
				blocklights[bumpID][i].AsVector3D() = blocklights[0][i].AsVector3D();
			}
		}
	}
	else if( needsBumpmap && !hasLightmap )
	{
		// set to full bright if no light data
		InitLMSamples( blocklights[1], size, 0.0f );
		InitLMSamples( blocklights[2], size, 0.0f );
		InitLMSamples( blocklights[3], size, 0.0f );
	}
	else if( !needsBumpmap && !needsLightmap )
	{
	}
	else if( needsLightmap && !hasLightmap )
	{
	}
	else
	{
		Assert( 0 );
	}

	// add all the dynamic lights
	if ( dlightMask && (needsLightmap || needsBumpmap) )
	{
		R_AddDynamicLights( pLights, surfID, entityToWorld, needsBumpmap, dlightMask );
	}

	// Update the texture state
	UpdateLightmapTextures( surfID, needsBumpmap );
}

void R_BuildLightMap( dlight_t *pLights, ICallQueue *pCallQueue, SurfaceHandle_t surfID, const matrix3x4_t &entityToWorld, bool bOnlyUseLightStyles )
{
	bool needsBumpmap = SurfNeedsBumpedLightmaps( surfID );
	bool needsLightmap = SurfNeedsLightmap( surfID );

	if( !needsBumpmap && !needsLightmap )
		return;
	
	if( materialSortInfoArray )
	{
		Assert( MSurf_MaterialSortID( surfID ) >= 0 && 
			    MSurf_MaterialSortID( surfID ) < g_WorldStaticMeshes.Count() );
		if (( materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID == MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE )	||
		   ( materialSortInfoArray[MSurf_MaterialSortID( surfID )].lightmapPageID == MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP ) )
		{
			return;
		}
	}

	bool bDlightsInLightmap = needsLightmap || needsBumpmap;
	unsigned int dlightMask = R_UpdateDlightState( pLights, surfID, entityToWorld, bOnlyUseLightStyles, bDlightsInLightmap );

	// update the state, but don't render any dlights if only lightstyles requested
	if ( bOnlyUseLightStyles )
		dlightMask = 0;

	if ( !pCallQueue )
	{
		R_BuildLightMapGuts( pLights, surfID, entityToWorld, dlightMask, needsBumpmap, needsLightmap );
	}
	else
	{
		pCallQueue->QueueCall( R_BuildLightMapGuts, pLights, surfID, RefToVal( entityToWorld ), dlightMask, needsBumpmap, needsLightmap );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Save off the average light values, and dump the rest of the lightmap data.
// Can be used to save memory, at the expense of dynamic lights and lightstyles.
//-----------------------------------------------------------------------------
void CacheAndUnloadLightmapData()
{
	Assert( !g_bHunkAllocLightmaps );
	if ( g_bHunkAllocLightmaps )
	{
		return;
	}

	worldbrushdata_t *pBrushData = host_state.worldbrush;
	msurfacelighting_t *pLighting = pBrushData->surfacelighting;
	int numSurfaces = pBrushData->numsurfaces;

	// This will allocate more data than necessary, but only 1-2K max
	byte *pDestBase = (byte*)malloc( numSurfaces * MAXLIGHTMAPS * sizeof( ColorRGBExp32 ) );
	byte *pDest = pDestBase;

	for ( int i = 0; i < numSurfaces; ++i, ++pLighting )
	{
		int nStyleCt = 0;
		for ( int map = 0 ; map < MAXLIGHTMAPS; ++map )
		{
			if ( pLighting->m_nStyles[map] != 255 )
				++nStyleCt;
		}

		const int nHdrBytes = nStyleCt * sizeof( ColorRGBExp32 );
		byte *pHdr = (byte*)pLighting->m_pSamples - nHdrBytes;

		// Copy just the 0-4 average color entries
		Q_memcpy( pDest, pHdr, nHdrBytes );

		// m_pSamples needs to point AFTER the average color data
		pDest += nHdrBytes;
		pLighting->m_pSamples = (ColorRGBExp32*)pDest;
	}

	// Update the lightdata pointer
	free( host_state.worldbrush->lightdata );
	host_state.worldbrush->lightdata = (ColorRGBExp32*)pDestBase;
	host_state.worldbrush->unloadedlightmaps = true;
}

//sorts the surfaces in place
static void SortSurfacesByLightmapID( SurfaceHandle_t *pToSort, int iSurfaceCount )
{
	SurfaceHandle_t *pSortTemp = (SurfaceHandle_t *)stackalloc( sizeof( SurfaceHandle_t ) * iSurfaceCount );
	
	//radix sort
	for( int radix = 0; radix != 4; ++radix )
	{
		//swap the inputs for the next pass
		{
			SurfaceHandle_t *pTemp = pToSort;
			pToSort = pSortTemp;
			pSortTemp = pTemp;
		}

		int iCounts[256] = { 0 };
		int iBitOffset = radix * 8;
		for( int i = 0; i != iSurfaceCount; ++i )
		{
			uint8 val = (materialSortInfoArray[MSurf_MaterialSortID( pSortTemp[i] )].lightmapPageID >> iBitOffset) & 0xFF;
			++iCounts[val];
		}

		int iOffsetTable[256];
		iOffsetTable[0] = 0;
		for( int i = 0; i != 255; ++i )
		{
			iOffsetTable[i + 1] = iOffsetTable[i] + iCounts[i];
		}

		for( int i = 0; i != iSurfaceCount; ++i )
		{
			uint8 val = (materialSortInfoArray[MSurf_MaterialSortID( pSortTemp[i] )].lightmapPageID >> iBitOffset) & 0xFF;
			int iWriteIndex = iOffsetTable[val];
			pToSort[iWriteIndex] = pSortTemp[i];
			++iOffsetTable[val];
		}
	}
}

void R_RedownloadAllLightmaps()
{
#ifdef _DEBUG
	static bool initializedBlockLights = false;
	if (!initializedBlockLights)
	{
		memset( &blocklights[0][0][0], 0, MAX_LIGHTMAP_DIM_INCLUDING_BORDER * MAX_LIGHTMAP_DIM_INCLUDING_BORDER * (NUM_BUMP_VECTS + 1) * sizeof( Vector ) );
		initializedBlockLights = true;
	}
#endif

	double st = Sys_FloatTime();

	bool bOnlyUseLightStyles = false;

	if( r_dynamic.GetInt() == 0 )
	{
		bOnlyUseLightStyles = true;
	}

	// Can't build lightmaps if the source data has been dumped
	CMatRenderContextPtr pRenderContext( materials );
	ICallQueue *pCallQueue = pRenderContext->GetCallQueue();
	if ( !host_state.worldbrush->unloadedlightmaps )
	{		
		int iSurfaceCount = host_state.worldbrush->numsurfaces;
		
		SurfaceHandle_t *pSortedSurfaces = (SurfaceHandle_t *)stackalloc( sizeof( SurfaceHandle_t ) * iSurfaceCount );
		for( int surfaceIndex = 0; surfaceIndex < iSurfaceCount; surfaceIndex++ )
		{
			SurfaceHandle_t surfID = SurfaceHandleFromIndex( surfaceIndex );
			pSortedSurfaces[surfaceIndex] = surfID;
		}

		SortSurfacesByLightmapID( pSortedSurfaces, iSurfaceCount ); //sorts in place, so now the array really is sorted

		if( pCallQueue )
			pCallQueue->QueueCall( materials, &IMaterialSystem::BeginUpdateLightmaps );
		else
			materials->BeginUpdateLightmaps();
		
		matrix3x4_t xform;
		SetIdentityMatrix(xform);
		for( int surfaceIndex = 0; surfaceIndex < iSurfaceCount; surfaceIndex++ )
		{
			SurfaceHandle_t surfID = pSortedSurfaces[surfaceIndex];

			ASSERT_SURF_VALID( surfID );
			R_BuildLightMap( &cl_dlights[0], pCallQueue, surfID, xform, bOnlyUseLightStyles );
		}

		if( pCallQueue )
			pCallQueue->QueueCall( materials, &IMaterialSystem::EndUpdateLightmaps );
		else
			materials->EndUpdateLightmaps();		

		if ( !g_bHunkAllocLightmaps && r_unloadlightmaps.GetInt() == 1 )
		{
			// Delete the lightmap data from memory
			if ( !pCallQueue )
			{
				CacheAndUnloadLightmapData();
			}
			else
			{
				pCallQueue->QueueCall( CacheAndUnloadLightmapData );
			}
		}
	}

	float elapsed = ( float )( Sys_FloatTime() - st ) * 1000.0;
	DevMsg( "R_RedownloadAllLightmaps took %.3f msec!\n", elapsed );

	g_RebuildLightmaps = false;
}

//-----------------------------------------------------------------------------
// Purpose: flag the lightmaps as needing to be rebuilt (gamma change)
//-----------------------------------------------------------------------------
bool g_RebuildLightmaps = false;

void GL_RebuildLightmaps( void )
{
	g_RebuildLightmaps = true;
}


//-----------------------------------------------------------------------------
// Purpose: Update the in-RAM texture for the given surface's lightmap
// Input  : *fa - surface pointer
//-----------------------------------------------------------------------------

#ifdef UPDATE_LIGHTSTYLES_EVERY_FRAME
ConVar mat_updatelightstyleseveryframe( "mat_updatelightstyleseveryframe", "0" );
#endif
void FASTCALL R_RenderDynamicLightmaps ( dlight_t *pLights, ICallQueue *pCallQueue, SurfaceHandle_t surfID, const matrix3x4_t &xform )
{
	VPROF_BUDGET( "R_RenderDynamicLightmaps", VPROF_BUDGETGROUP_DLIGHT_RENDERING );
	ASSERT_SURF_VALID( surfID );

	int fSurfFlags = MSurf_Flags( surfID );

	if( fSurfFlags & SURFDRAW_NOLIGHT )
		return;

	// check for lightmap modification
	bool bChanged = false;
	msurfacelighting_t *pLighting = SurfaceLighting( surfID );
	if( fSurfFlags & SURFDRAW_HASLIGHTSYTLES )
	{
#ifdef UPDATE_LIGHTSTYLES_EVERY_FRAME
		if( mat_updatelightstyleseveryframe.GetBool() && ( pLighting->m_nStyles[0] != 0 || pLighting->m_nStyles[1] != 255 ) )
		{
			bChanged = true;
		}
#endif
		for( int maps = 0; maps < MAXLIGHTMAPS && pLighting->m_nStyles[maps] != 255; maps++ )
		{
			if( d_lightstyleframe[pLighting->m_nStyles[maps]] > pLighting->m_nLastComputedFrame )
			{
				bChanged = true;
				break;
			}
		}
	}

	// was it dynamic this frame (pLighting->m_nDLightFrame == r_framecount) 
	// or dynamic previously (pLighting->m_fDLightBits)
	bool bDLightChanged = ( pLighting->m_nDLightFrame == r_framecount ) || pLighting->m_fDLightBits;
	bool bOnlyUseLightStyles = false;

	if( r_dynamic.GetInt() == 0 )
	{
		bOnlyUseLightStyles = true;
		bDLightChanged = false;
	}

	if ( bChanged || bDLightChanged )
	{
		R_BuildLightMap( pLights, pCallQueue, surfID, xform, bOnlyUseLightStyles );
	}
}
