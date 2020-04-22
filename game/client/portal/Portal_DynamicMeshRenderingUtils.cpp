//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "Portal_DynamicMeshRenderingUtils.h"
#include "iviewrender.h"

extern ConVar mat_wireframe;


int ClipPolyToPlane_LerpTexCoords( PortalMeshPoint_t *inVerts, int vertCount, PortalMeshPoint_t *outVerts, const Vector& normal, float dist, float fOnPlaneEpsilon )
{
	vec_t	*dists = (vec_t *)stackalloc( sizeof(vec_t) * vertCount * 4 ); //4x vertcount should cover all cases
	int		*sides = (int *)stackalloc( sizeof(int) * vertCount * 4 );
	int		counts[3];
	vec_t	dot;
	int		i, j;
	Vector	mid = vec3_origin;
	int		outCount;

	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for ( i = 0; i < vertCount; i++ )
	{
		dot = DotProduct( inVerts[i].vWorldSpacePosition, normal) - dist;
		dists[i] = dot;
		if ( dot > fOnPlaneEpsilon )
			sides[i] = SIDE_FRONT;
		else if ( dot < -fOnPlaneEpsilon )
			sides[i] = SIDE_BACK;
		else
			sides[i] = SIDE_ON;
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if (!counts[0])
		return 0;

	if (!counts[1])
	{
		// Copy to output verts
		//for ( i = 0; i < vertCount; i++ )
		memcpy( outVerts, inVerts, sizeof( PortalMeshPoint_t ) * vertCount );
		return vertCount;
	}

	outCount = 0;
	for ( i = 0; i < vertCount; i++ )
	{
		if (sides[i] == SIDE_ON)
		{
			memcpy( &outVerts[outCount], &inVerts[i], sizeof( PortalMeshPoint_t ) );
			++outCount;
			continue;
		}
		if (sides[i] == SIDE_FRONT)
		{
			memcpy( &outVerts[outCount], &inVerts[i], sizeof( PortalMeshPoint_t ) );
			++outCount;
		}
		if (sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		Vector& p1 = inVerts[i].vWorldSpacePosition;
		

		// generate a split point
		int i2 = (i+1)%vertCount;
		Vector& p2 = inVerts[i2].vWorldSpacePosition;
		
		dot = dists[i] / (dists[i]-dists[i+1]);
		for (j=0 ; j<3 ; j++)
		{	
			mid[j] = p1[j] + dot*(p2[j]-p1[j]);
		}

		VectorCopy (mid, outVerts[outCount].vWorldSpacePosition);
		
		outVerts[outCount].texCoord.x = inVerts[i].texCoord.x + dot*(inVerts[i2].texCoord.x - inVerts[i].texCoord.x);
		outVerts[outCount].texCoord.y = inVerts[i].texCoord.y + dot*(inVerts[i2].texCoord.y - inVerts[i].texCoord.y);
		
		++outCount;
	}

	return outCount;
}

void RenderPortalMeshConvexPolygon( PortalMeshPoint_t *pVerts, int iVertCount, const IMaterial *pMaterial, void *pBind )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( (IMaterial *)pMaterial, pBind );

	//PortalMeshPoint_t *pMidVerts = (PortalMeshPoint_t *)stackalloc( sizeof( PortalMeshPoint_t ) * iVertCount );

	CMeshBuilder meshBuilder;
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, iVertCount - 2 );

	//any convex polygon can be rendered with a triangle strip by starting at a vertex and alternating vertices from each side
	int iForwardCounter = 0;
	int iReverseCounter = iVertCount - 1; //guaranteed to be >= 2 to start

	do
	{
		PortalMeshPoint_t *pVertex = &pVerts[iForwardCounter];
		meshBuilder.Position3fv( &pVertex->vWorldSpacePosition.x );
		meshBuilder.TexCoord2fv( 0, &pVertex->texCoord.x );
		meshBuilder.AdvanceVertex();
		++iForwardCounter;

		if( iForwardCounter > iReverseCounter )
			break;

		pVertex = &pVerts[iReverseCounter];
		meshBuilder.Position3fv( &pVertex->vWorldSpacePosition.x );
		meshBuilder.TexCoord2fv( 0, &pVertex->texCoord.x );
		meshBuilder.AdvanceVertex();
		--iReverseCounter;
	} while( iForwardCounter <= iReverseCounter );

	meshBuilder.End();
	pMesh->Draw();
}


void Clip_And_Render_Convex_Polygon( PortalMeshPoint_t *pVerts, int iVertCount, const IMaterial *pMaterial, void *pBind )
{
	PortalMeshPoint_t *pInVerts = (PortalMeshPoint_t *)stackalloc( iVertCount * 4 * sizeof( PortalMeshPoint_t ) ); //really only should need 2x points, but I'm paranoid
	PortalMeshPoint_t *pOutVerts = (PortalMeshPoint_t *)stackalloc( iVertCount * 4 * sizeof( PortalMeshPoint_t ) );
	PortalMeshPoint_t *pTempVerts;


	//clip by the viewing frustum
	{
		VPlane *pFrustum = view->GetFrustum();
		
		//clip by first plane and put output into pInVerts
		iVertCount = ClipPolyToPlane_LerpTexCoords( pVerts, iVertCount, pInVerts, pFrustum[0].m_Normal, pFrustum[0].m_Dist, 0.01f );

		//clip by other planes and flipflop in and out pointers
		for( int i = 1; i != FRUSTUM_NUMPLANES; ++i )
		{
			if( iVertCount < 3 )
				return; //nothing to draw

			iVertCount = ClipPolyToPlane_LerpTexCoords( pInVerts, iVertCount, pOutVerts, pFrustum[i].m_Normal, pFrustum[i].m_Dist, 0.01f );
			pTempVerts = pInVerts; pInVerts = pOutVerts; pOutVerts = pTempVerts; //swap vertex pointers
		}

		if( iVertCount < 3 )
			return; //nothing to draw
	}

	CMatRenderContextPtr pRenderContext( materials );
	
	RenderPortalMeshConvexPolygon( pOutVerts, iVertCount, pMaterial, pBind );
	if( mat_wireframe.GetBool() )
		RenderPortalMeshConvexPolygon( pOutVerts, iVertCount, materials->FindMaterial( "shadertest/wireframe", TEXTURE_GROUP_CLIENT_EFFECTS, false ), pBind );

	stackfree( pOutVerts );
	stackfree( pInVerts );
}














