//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "decal_clip.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// --------------------------------------------------------------------------- //
// Template classes for the clipper.
// --------------------------------------------------------------------------- //
class CPlane_Top
{
public:
	static inline bool Inside( CDecalVert *pVert )					{return pVert->m_ctCoords.y < 1;}
	static inline float Clip( CDecalVert *one, CDecalVert *two )	{return (1 - one->m_ctCoords.y) / (two->m_ctCoords.y - one->m_ctCoords.y);}
};

class CPlane_Left
{
public:
	static inline bool Inside( CDecalVert *pVert )					{return pVert->m_ctCoords.x > 0;}
	static inline float Clip( CDecalVert *one, CDecalVert *two )	{return one->m_ctCoords.x / (one->m_ctCoords.x - two->m_ctCoords.x);}
};

class CPlane_Right
{
public:
	static inline bool Inside( CDecalVert *pVert )					{return pVert->m_ctCoords.x < 1;}
	static inline float Clip( CDecalVert *one, CDecalVert *two )	{return (1 - one->m_ctCoords.x) / (two->m_ctCoords.x - one->m_ctCoords.x);}
};

class CPlane_Bottom
{
public:
	static inline bool Inside( CDecalVert *pVert )					{return pVert->m_ctCoords.y > 0;}
	static inline float Clip( CDecalVert *one, CDecalVert *two )	{return one->m_ctCoords.y / (one->m_ctCoords.y - two->m_ctCoords.y);}
};



// --------------------------------------------------------------------------- //
// Globals.
// --------------------------------------------------------------------------- //
CDecalVert ALIGN16 g_DecalClipVerts[MAX_DECALCLIPVERT] ALIGN16_POST;
static CDecalVert ALIGN16 g_DecalClipVerts2[MAX_DECALCLIPVERT] ALIGN16_POST;




template< class Clipper >
static inline void Intersect( Clipper &clip, CDecalVert *one, CDecalVert *two, CDecalVert *out )
{
	float t = Clipper::Clip( one, two );
	
	VectorLerp( one->m_vPos, two->m_vPos, t, out->m_vPos );
	Vector2DLerp( one->m_cLMCoords, two->m_cLMCoords, t, out->m_cLMCoords );
	Vector2DLerp( one->m_ctCoords, two->m_ctCoords, t, out->m_ctCoords );
}


template< class Clipper >
static inline int SHClip( CDecalVert *pDecalClipVerts, int vertCount, CDecalVert *out, Clipper &clip )
{
	int j, outCount;
	CDecalVert *s, *p;

	Assert( vertCount <= MAX_DECALCLIPVERT );

	outCount = 0;

	s = &pDecalClipVerts[ vertCount-1 ];
	for ( j = 0; j < vertCount; j++ ) 
	{
		p = &pDecalClipVerts[ j ];
		if ( Clipper::Inside( p ) ) 
		{
			if ( Clipper::Inside( s ) ) 
			{
				*out = *p;
				outCount++;
				out++;
			}
			else 
			{
				Intersect( clip, s, p, out );
				out++;
				outCount++;

				*out = *p;
				outCount++;
				out++;
			}
		}
		else 
		{
			if ( Clipper::Inside( s ) ) 
			{
				Intersect( clip, p, s, out );
				out++;
				outCount++;
			}
		}
		s = p;
	}
				
	return outCount;
}

const float DECAL_CLIP_EPSILON = 0.01f;

CDecalVert* R_DoDecalSHClip( CDecalVert *pInVerts, CDecalVert *pOutVerts, decal_t *pDecal, int nStartVerts, const Vector &vecNormal )
{
	if ( pOutVerts == NULL )
		pOutVerts = &g_DecalClipVerts[0];

	CPlane_Top top;
	CPlane_Left left;
	CPlane_Right right;
	CPlane_Bottom bottom;

	// Clip the polygon to the decal texture space
	int outCount = SHClip( pInVerts, nStartVerts, &g_DecalClipVerts2[0], top );
	outCount = SHClip( &g_DecalClipVerts2[0], outCount, &g_DecalClipVerts[0], left );
	outCount = SHClip( &g_DecalClipVerts[0], outCount, &g_DecalClipVerts2[0], right );
	outCount = SHClip( &g_DecalClipVerts2[0], outCount, pOutVerts, bottom );

	pDecal->clippedVertCount = outCount;

	if ( !outCount )
		return NULL;

	// FIXME: This is a brutally hack workaround for the fact that we get massive decal flicker
	// when looking at a decal at a glancing angle while standing right next to it.

	for ( int i = 0; i < outCount; ++i )
	{
		VectorMA( pOutVerts[i].m_vPos, OVERLAY_AVOID_FLICKER_NORMAL_OFFSET, vecNormal, pOutVerts[i].m_vPos );
	}
	if ( outCount && pDecal->material->InMaterialPage() )
	{
		float offset[2], scale[2];
		pDecal->material->GetMaterialOffset( offset );
		pDecal->material->GetMaterialScale( scale );
		for ( int i = 0; i < outCount; ++i )
		{
			pOutVerts[i].m_ctCoords.x = offset[0] + (pOutVerts[i].m_ctCoords.x * scale[0]);
			pOutVerts[i].m_ctCoords.y = offset[1] + (pOutVerts[i].m_ctCoords.y * scale[1]);
		}
	}

	return pOutVerts;
}

// Build the initial list of vertices from the surface verts into the global array, 'verts'.
void R_SetupDecalVertsForMSurface( 
	decal_t * RESTRICT pDecal, 
	SurfaceHandle_t surfID, 
	Vector * RESTRICT pTextureSpaceBasis,
	CDecalVert * RESTRICT pVerts )
{
	unsigned short * RESTRICT pIndices = &host_state.worldbrush->vertindices[MSurf_FirstVertIndex( surfID )];
	int count = MSurf_VertCount( surfID );
	float uOffset = 0.5f - pDecal->dx;
	float vOffset = 0.5f - pDecal->dy;

	for ( int j = 0; j < count; j++ )
	{
		int vertIndex = pIndices[j];
		
		pVerts[j].m_vPos = host_state.worldbrush->vertexes[vertIndex].position; // Copy model space coordinates
		// garymcthack - what about m_ParentTexCoords?
		pVerts[j].m_ctCoords.x = DotProduct( pVerts[j].m_vPos, pTextureSpaceBasis[0] ) + uOffset;
		pVerts[j].m_ctCoords.y = DotProduct( pVerts[j].m_vPos, pTextureSpaceBasis[1] ) + vOffset;
		pVerts[j].m_cLMCoords.Init();
	}
}


//-----------------------------------------------------------------------------
// compute the decal basis based on surface normal, and preferred saxis
//-----------------------------------------------------------------------------

#define SIN_45_DEGREES ( 0.70710678118654752440084436210485f )

void R_DecalComputeBasis( Vector const& surfaceNormal, Vector const* pSAxis, 
								 Vector* textureSpaceBasis )
{
	/*
	// s, t, textureSpaceNormal (T cross S = textureSpaceNormal(N))
	//   N     
	//   \   
	//    \     
	//     \  
	//      |---->S
	//      | 
	//		|  
	//      |T    
	// S = textureSpaceBasis[0]
	// T = textureSpaceBasis[1]
	// N = textureSpaceBasis[2]
	*/

	// Get the surface normal.
	VectorCopy( surfaceNormal, textureSpaceBasis[2] );

	if (pSAxis)
	{
		// T = S cross N
		CrossProduct( *pSAxis, textureSpaceBasis[2], textureSpaceBasis[1] );

		// Name sure they aren't parallel or antiparallel
		// In that case, fall back to the normal algorithm.
		if ( DotProduct( textureSpaceBasis[1], textureSpaceBasis[1] ) > 1e-6 )
		{
			// S = N cross T
			CrossProduct( textureSpaceBasis[2], textureSpaceBasis[1], textureSpaceBasis[0] );

			VectorNormalizeFast( textureSpaceBasis[0] );
			VectorNormalizeFast( textureSpaceBasis[1] );
			return;
		}

		// Fall through to the standard algorithm for parallel or antiparallel
	}

	// floor/ceiling?
	if( fabs( surfaceNormal[2] ) > SIN_45_DEGREES )
	{
		textureSpaceBasis[0][0] = 1.0f;
		textureSpaceBasis[0][1] = 0.0f;
		textureSpaceBasis[0][2] = 0.0f;

		// T = S cross N
		CrossProduct( textureSpaceBasis[0], textureSpaceBasis[2], textureSpaceBasis[1] );

		// S = N cross T
		CrossProduct( textureSpaceBasis[2], textureSpaceBasis[1], textureSpaceBasis[0] );
	}
	// wall
	else
	{
		textureSpaceBasis[1][0] = 0.0f;
		textureSpaceBasis[1][1] = 0.0f;
		textureSpaceBasis[1][2] = -1.0f;

		// S = N cross T
		CrossProduct( textureSpaceBasis[2], textureSpaceBasis[1], textureSpaceBasis[0] );
		// T = S cross N
		CrossProduct( textureSpaceBasis[0], textureSpaceBasis[2], textureSpaceBasis[1] );
	}

	VectorNormalizeFast( textureSpaceBasis[0] );
	VectorNormalizeFast( textureSpaceBasis[1] );
}

#define MAX_PLAYERSPRAY_SIZE		64

void R_SetupDecalTextureSpaceBasis( decal_t *pDecal, Vector &vSurfNormal, IMaterial *pMaterial, Vector textureSpaceBasis[3], float decalWorldScale[2] )
{
	// Compute the non-scaled decal basis
	R_DecalComputeBasis( vSurfNormal, (pDecal->flags & FDECAL_USESAXIS) ? &pDecal->saxis : 0, textureSpaceBasis );

	// world width of decal = ptexture->width / pDecal->scale
	// world height of decal = ptexture->height / pDecal->scale
	// scale is inverse, scales world space to decal u/v space [0,1]
	// OPTIMIZE: Get rid of these divides
	if ( pDecal->flags & FDECAL_PLAYERSPRAY )
	{
		int nWidthScale = pMaterial->GetMappingWidth() / MAX_PLAYERSPRAY_SIZE;
		int nHeightScale = pMaterial->GetMappingHeight() / MAX_PLAYERSPRAY_SIZE;
		float flScale = static_cast<float>( max( nWidthScale, nHeightScale ) );

		decalWorldScale[0] = pDecal->scale / pMaterial->GetMappingWidth();
		decalWorldScale[1] = pDecal->scale / pMaterial->GetMappingHeight();

		if ( flScale > 1.0f )
		{
			decalWorldScale[0] *= flScale;
			decalWorldScale[1] *= flScale;
		}
	}
	else
	{
		decalWorldScale[0] = pDecal->scale / pMaterial->GetMappingWidth();
		decalWorldScale[1] = pDecal->scale / pMaterial->GetMappingHeight();
	}

	VectorScale( textureSpaceBasis[0], decalWorldScale[0], textureSpaceBasis[0] );
	VectorScale( textureSpaceBasis[1], decalWorldScale[1], textureSpaceBasis[1] );
}


// Figure out where the decal maps onto the surface.
void R_SetupDecalClip( CDecalVert* &pOutVerts, decal_t *pDecal, Vector &vSurfNormal, IMaterial *pMaterial, Vector textureSpaceBasis[3], float decalWorldScale[2] )
{
//	if ( pOutVerts == NULL )
//		pOutVerts = &g_DecalClipVerts[0];

	R_SetupDecalTextureSpaceBasis( pDecal, vSurfNormal, pMaterial, textureSpaceBasis, decalWorldScale );

	// Generate texture coordinates for each vertex in decal s,t space
	// probably should pre-generate this, store it and use it for decal-decal collisions
	// as in R_DecalsIntersect()
	pDecal->dx = DotProduct( pDecal->position, textureSpaceBasis[0] );
	pDecal->dy = DotProduct( pDecal->position, textureSpaceBasis[1] );
}


//-----------------------------------------------------------------------------
// Generate clipped vertex list for decal pdecal projected onto polygon psurf
//-----------------------------------------------------------------------------
CDecalVert* R_DecalVertsClip( CDecalVert *pOutVerts, decal_t *pDecal, SurfaceHandle_t surfID, IMaterial *pMaterial )
{
	float decalWorldScale[2];
	Vector textureSpaceBasis[3]; 

	// Figure out where the decal maps onto the surface.
	R_SetupDecalClip( pOutVerts, pDecal, MSurf_Plane( surfID ).normal, pMaterial, textureSpaceBasis, decalWorldScale );

	// Build the initial list of vertices from the surface verts.
	R_SetupDecalVertsForMSurface( pDecal, surfID, textureSpaceBasis, g_DecalClipVerts );

	return R_DoDecalSHClip( g_DecalClipVerts, pOutVerts, pDecal, MSurf_VertCount( surfID ), MSurf_Plane( surfID ).normal );
}
