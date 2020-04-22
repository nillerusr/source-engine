//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "stdafx.h"
#include "IEditorTexture.h"
#include "MapFace.h"
#include "clipcode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static float g_vert[MAX_CLIPVERT][VERTEXSIZE];
static int g_outCount;


// Quick and dirty sutherland Hodgman clipper
// Clip polygon to decal in texture space
// JAY: This code is lame, change it later.  It does way too much work per frame
// It can be made to recursively call the clipping code and only copy the vertex list once
int Inside( float *vert, int edge )
{
	switch( edge ) {
		case 0:		// left
			if ( vert[3] > 0.0 )
				return 1;
			return 0;
		case 1:		// right
			if ( vert[3] < 1.0 )
				return 1;
			return 0;

		case 2:		// top
			if ( vert[4] > 0.0 )
				return 1;
			return 0;

		case 3:
			if ( vert[4] < 1.0 )
				return 1;
			return 0;
	}
	return 0;
}


void Intersect( float *one, float *two, int edge, float *out )
{
	float t;

	// t is the parameter of the line between one and two clipped to the edge
	// or the fraction of the clipped point between one & two
	// vert[3] is u
	// vert[4] is v
	// vert[0], vert[1], vert[2] is X, Y, Z
	if ( edge < 2 ) {
		if ( edge == 0 ) {	// left
			t = ( (one[3] - 0) / (one[3] - two[3]) );
			out[3] = 0;
		}
		else {				// right
			t = ( (one[3] - 1) / (one[3] - two[3]) );
			out[3] = 1;
		}
		out[4] = one[4] + (two[4] - one[4]) * t;
	}
	else {
		if ( edge == 2 ) {	// top
			t = ( (one[4] - 0)  / (one[4] - two[4]) );
			out[4] = 0;
		}
		else {				// bottom
			t = ( (one[4] - 1) / (one[4] - two[4]) );
			out[4] = 1;
		}
		out[3] = one[3] + (two[3] - one[3]) * t;
	}
	out[0] = one[0] + (two[0] - one[0]) * t;
	out[1] = one[1] + (two[1] - one[1]) * t;
	out[2] = one[2] + (two[2] - one[2]) * t;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *vert - 
//			vertCount - 
//			*out - 
//			outSize - 
//			edge - 
// Output : int
//-----------------------------------------------------------------------------
int SHClip( float *vert, int vertCount, float *out, int outSize, int edge )
{
	int		j, outCount;
	float	*s, *p;

	outCount = 0;

	s = &vert[ (vertCount-1) * VERTEXSIZE ];
	for ( j = 0; j < vertCount; j++ ) {
		p = &vert[ j * VERTEXSIZE ];
		if ( Inside( p, edge ) ) {
			if ( Inside( s, edge ) ) {
				// Add a vertex and advance out to next vertex
				memcpy( out, p, sizeof(float)*VERTEXSIZE );
				outCount++;
				out += VERTEXSIZE;
			}
			else {
				Intersect( s, p, edge, out );
				out += VERTEXSIZE;
				outCount++;
				memcpy( out, p, sizeof(float)*VERTEXSIZE );
				outCount++;
				out += VERTEXSIZE;
			}
		}
		else {
			if ( Inside( s, edge ) ) {
				Intersect( p, s, edge, out );
				out += VERTEXSIZE;
				outCount++;
			}
		}

		if (outCount >= outSize)
		{
			Assert(FALSE);
			break;
		}

		s = p;
	}
				
	return outCount;
}


#define SIN_45_DEGREES ( 0.70710678118654752440084436210485f )

// The world coordinate system is right handed with Z up.
// 
//      ^ Z
//      |
//      |   
//      | 
//X<----|
//       \
//		  \
//         \ Y

//-----------------------------------------------------------------------------
// compute the decal basis based on surface normal, and preferred saxis
//-----------------------------------------------------------------------------

static void R_DecalComputeBasis( Vector const& surfaceNormal, Vector const* pSAxis, 
								 bool flipNormal, Vector* textureSpaceBasis )
{
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

	// Get the surface normal.
	textureSpaceBasis[2] = surfaceNormal;
	if (flipNormal)
		VectorNegate( textureSpaceBasis[2] );

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

			VectorNormalize( textureSpaceBasis[0] );
			VectorNormalize( textureSpaceBasis[1] );
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

	VectorNormalize( textureSpaceBasis[0] );
	VectorNormalize( textureSpaceBasis[1] );
}


//-----------------------------------------------------------------------------
// Purpose: Clips a texture to a face. Used for decal application.
// NOTE	  : HL and HL2 generate texcoords for decals differently!!!
// Input  : pFace -
//			pDecalTex - 
//			org - 
//			pOutPoints - 
// Output : Returns the number of points places in the pOutPoints array.
//-----------------------------------------------------------------------------
int CreateClippedPoly(CMapFace *pFace, IEditorTexture *pDecalTex, Vector& org, vec5_t *pOutPoints, int nOutSize)
{
	float outvert[MAX_CLIPVERT][VERTEXSIZE];
	Assert(nOutSize <= MAX_CLIPVERT);			// This code uses temp buffers of this size.

/*#ifdef SDK_BUILD
	BUG: THIS IS THE HL1 VERSION! SWITCH BETWEEN THESE ALGORITHMS AT RUNTIME
	Vector vecOrg, vecSAxis, vecTAxis;

	// Copy the origin.
	vecOrg = org;

	// Get the U/V axes for this face.
	vecSAxis = pFace->texture.UAxis;
	vecTAxis = pFace->texture.VAxis;

	float decalwidth = pDecalTex->GetWidth();
	float decalheight = pDecalTex->GetHeight();

	float scale = 1.0f;
	IEditorTexture *pFaceTex = pFace->GetTexture();
	float scalex = scale * (float)pFaceTex->GetWidth() / decalwidth;
	float scaley = scale * (float)pFaceTex->GetHeight() / decalheight;

	float u = DotProduct(vecSAxis, vecOrg);
	float v = DotProduct(vecTAxis, vecOrg);

	u -= decalwidth / 2;
	v -= decalheight / 2;

	u /= pFaceTex->GetWidth();
	v /= pFaceTex->GetHeight();

	// Generate texture coordinates for each vertex in decal s,t space
	Vector *pVertex = pFace->Points;
	float curU, curV;
	for (int j = 0; j < pFace->nPoints; j++, pVertex++)
	{
		// Copy X, Y, & Z
		g_vert[j][0] = pVertex[0][0];
		g_vert[j][1] = pVertex[0][1];
		g_vert[j][2] = pVertex[0][2];

		// Get u, v coordinates of vertex in DECAL SPACE
		curU = DotProduct(vecSAxis, *pVertex) / pFaceTex->GetWidth();
		curV = DotProduct(vecTAxis, *pVertex) / pFaceTex->GetHeight();

		// Generate U & V
		g_vert[j][3] = (curU - u) * scalex;		// Decal relative texture coordinates
		g_vert[j][4] = (curV - v) * scaley;
	}
#else */
	// THIS IS THE HL2 VERSION!
	float decalScale = pDecalTex->GetDecalScale();
	float decalWidth = pDecalTex->GetWidth();
	float decalHeight = pDecalTex->GetHeight();

	Vector textureSpaceBasis[3];
	
	R_DecalComputeBasis( pFace->plane.normal, NULL,
						 false, textureSpaceBasis );

	float u = DotProduct(textureSpaceBasis[0], org);
	float v = DotProduct(textureSpaceBasis[1], org);

	// subtract the world space dist from the center of the 
	// decal to the origin of the decal
	u -= decalWidth * decalScale / 2.0f;
	v -= decalHeight * decalScale / 2.0f;

	float scalex = 1.0f / ( decalScale * decalWidth );
	float scaley = 1.0f / ( decalScale * decalHeight );

	// Generate texture coordinates for each vertex in decal s,t space
	Vector *pVertex = pFace->Points;
	float curU, curV;
	for (int j = 0; j < pFace->nPoints; j++, pVertex++)
	{
		// Copy X, Y, & Z
		g_vert[j][0] = pVertex[0][0];
		g_vert[j][1] = pVertex[0][1];
		g_vert[j][2] = pVertex[0][2];

		// Get u, v coordinates of vertex in DECAL SPACE
		curU = DotProduct(textureSpaceBasis[0], *pVertex);
		curV = DotProduct(textureSpaceBasis[1], *pVertex);

		// Generate U & V
		g_vert[j][3] = (curU - u) * scalex;		// Decal relative texture coordinates
		g_vert[j][4] = (curV - v) * scaley;
	}
// #endif

	// Clip the polygon to the decal texture space
	// FIXME: Yes this realy copies the vertex list 4 times !!
	int nMaxVerts = min(nOutSize, MAX_CLIPVERT);
	g_outCount = SHClip( g_vert[0], pFace->nPoints, outvert[0], nMaxVerts, 0 );	// clip left
	g_outCount = SHClip( outvert[0], g_outCount, g_vert[0], nMaxVerts, 1 );		// clip right
	g_outCount = SHClip( g_vert[0], g_outCount, outvert[0], nMaxVerts, 2 );		// clip top
	g_outCount = SHClip( outvert[0], g_outCount, g_vert[0], nMaxVerts, 3 );		// clip bottom

	memcpy(pOutPoints, g_vert, sizeof(vec5_t) * g_outCount);

	return(g_outCount);
}

