//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <stdafx.h>
#include "MapOverlay.h"
#include "MapFace.h"
#include "MapSolid.h"
#include "MapWorld.h"
#include "MainFrm.h"
#include "GlobalFunctions.h"
#include "MapDoc.h"
#include "TextureSystem.h"
#include "Material.h"
#include "materialsystem/imesh.h"
#include "Box3D.h"
#include "MapDefs.h"
#include "CollisionUtils.h"
#include "MapSideList.h"
#include "MapDisp.h"
#include "ToolManager.h"
#include "objectproperties.h"
#include "ChunkFile.h"
#include "mapview.h"
#include "options.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

IMPLEMENT_MAPCLASS( CMapOverlay )

#define OVERLAY_INITSIZE				25.0f		// x2

#define OVERLAY_BASIS_U					0
#define OVERLAY_BASIS_V					1
#define OVERLAY_BASIS_NORMAL			2	

#define OVERLAY_HANDLES_COUNT			4

#define OVERLAY_WORLDSPACE_EPSILON		0.03125f
#define OVERLAY_DISPSPACE_EPSILON		0.000001f
#define OVERLAY_BARYCENTRIC_EPSILON		0.001f

#define OVERLAY_BLENDTYPE_VERT			0
#define OVERLAY_BLENDTYPE_EDGE			1
#define OVERLAY_BLENDTYPE_BARY			2
#define OVERLAY_ANGLE0					1
#define OVERLAY_ANGLE45					2
#define OVERLAY_ANGLE90					3
#define OVERLAY_ANGLE135				4

#define OVERLAY_INVALID_VALUE			-99999.9f

//=============================================================================
//
// Basis Functions
//

//-----------------------------------------------------------------------------
// Purpose: Initialize the basis data.
//-----------------------------------------------------------------------------
void CMapOverlay::Basis_Clear( void )
{
	m_Basis.m_pFace = NULL;
	m_Basis.m_vecOrigin.Init();

	for( int iAxis = 0; iAxis < 3; iAxis++ )
	{
		m_Basis.m_vecAxes[iAxis].Init( OVERLAY_INVALID_VALUE, OVERLAY_INVALID_VALUE, OVERLAY_INVALID_VALUE );
		m_Basis.m_nAxesFlip[iAxis] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Build the overlay basis given an entity and base face (CMapFace).
//-----------------------------------------------------------------------------
void CMapOverlay::Basis_Init( CMapFace *pFace )
{
	// Valid face?
	Assert( pFace != NULL );
	if( !pFace )
		return;

	// Set the face the basis are derived from.
	Basis_SetFace( pFace );

	// Set the basis origin.
	Basis_UpdateOrigin();

	// Setup the basis axes.
	Basis_BuildAxes();

	// Initialize the texture coordinates - based on basis.
	Material_TexCoordInit();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::Basis_UpdateOrigin( void )
{
	CMapEntity *pEntity = static_cast<CMapEntity*>( GetParent() );
	if ( pEntity )
	{
		Vector vecEntityOrigin;
		pEntity->GetOrigin( vecEntityOrigin );

		Vector vecPoint( 0.0f, 0.0f, 0.0f );
		if ( !EntityOnSurfFromListToBaseFacePlane( vecEntityOrigin, vecPoint ) )
		{
			vecPoint = vecEntityOrigin;
		}

		m_Basis.m_vecOrigin = vecPoint;
	}

	// Update the property box.
	Basis_UpdateParentKey();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::Basis_BuildAxes( void )
{
	// Valid face?
	if( !m_Basis.m_pFace )
		return;

	// Build the basis axes.
	Vector vecFaceNormal;
	m_Basis.m_pFace->GetFaceNormal( vecFaceNormal );
	VectorNormalize( vecFaceNormal );
	VectorCopy( vecFaceNormal, m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL] );

	Basis_SetInitialUAxis( vecFaceNormal );

	m_Basis.m_vecAxes[OVERLAY_BASIS_V] = m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL].Cross( m_Basis.m_vecAxes[OVERLAY_BASIS_U] );
	VectorNormalize( m_Basis.m_vecAxes[OVERLAY_BASIS_V] );

	m_Basis.m_vecAxes[OVERLAY_BASIS_U] = m_Basis.m_vecAxes[OVERLAY_BASIS_V].Cross( m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL] );
	VectorNormalize( m_Basis.m_vecAxes[OVERLAY_BASIS_U] );

	// Flip uvn axes?
	for ( int iAxis = 0; iAxis < 3; ++iAxis )
	{
		for ( int iComp = 0; iComp < 3; ++iComp )
		{
			if ( Basis_IsFlipped( iAxis, iComp ) )
			{
				m_Basis.m_vecAxes[iAxis][iComp] = -m_Basis.m_vecAxes[iAxis][iComp];
			}
		}
	}

	Basis_UpdateParentKey();
}

//-----------------------------------------------------------------------------
// Purpose: A basis building helper function that finds the best guess u-axis
//          given a base face (CMapFace) normal.
//   Input: vecNormal - the base face normal
//-----------------------------------------------------------------------------
void CMapOverlay::Basis_SetInitialUAxis( Vector const &vecNormal )
{
	// Find the major vector component.
	int nMajorAxis = 0;
	float flAxisValue = vecNormal[0];
	if ( FloatMakePositive( vecNormal[1] ) > FloatMakePositive( flAxisValue ) ) 
	{ 
		nMajorAxis = 1; 
		flAxisValue = vecNormal[1]; 
	}
	if ( FloatMakePositive( vecNormal[2] ) > FloatMakePositive( flAxisValue ) ) 
	{ 
		nMajorAxis = 2; 
	}

	if ( ( nMajorAxis == 1 ) || ( nMajorAxis == 2 ) )
	{
		m_Basis.m_vecAxes[OVERLAY_BASIS_U].Init( 1.0f, 0.0f, 0.0f );
	}
	else
	{
		m_Basis.m_vecAxes[OVERLAY_BASIS_U].Init( 0.0f, 1.0f, 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMapOverlay::Basis_IsValid( void )
{
	for ( int iBasis = 0; iBasis < 3; ++iBasis )
	{
		for ( int iAxis = 0; iAxis < 3; ++iAxis )
		{
			if ( m_Basis.m_vecAxes[iBasis][iAxis] == OVERLAY_INVALID_VALUE )
				return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::Basis_SetFace( CMapFace *pFace )
{
	// Verify face.
	if ( !pFace )
		return;

	m_Basis.m_pFace = pFace;
}

//-----------------------------------------------------------------------------
// Purpose: Copy the basis data from the source into the destination.
//   Input: pSrc - the basis source data
//          pDst (Output) - destination for the  basis data
//-----------------------------------------------------------------------------
void CMapOverlay::Basis_Copy( Basis_t *pSrc, Basis_t *pDst )
{
	pDst->m_pFace = pSrc->m_pFace;
	pDst->m_vecOrigin = pSrc->m_vecOrigin;

	for ( int iAxis = 0; iAxis < 3; iAxis++ )
	{
		pDst->m_vecAxes[iAxis] = pSrc->m_vecAxes[iAxis];
		pDst->m_nAxesFlip[iAxis] = pSrc->m_nAxesFlip[iAxis];
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::Basis_UpdateParentKey( void )
{
	char szValue[80];

	CMapEntity *pEntity = ( CMapEntity* )GetParent();
	if ( pEntity )
	{
		sprintf( szValue, "%g %g %g", m_Basis.m_vecOrigin.x, m_Basis.m_vecOrigin.y, m_Basis.m_vecOrigin.z );
		pEntity->NotifyChildKeyChanged( this, "BasisOrigin", szValue );

		sprintf( szValue, "%g %g %g", m_Basis.m_vecAxes[OVERLAY_BASIS_U].x, m_Basis.m_vecAxes[OVERLAY_BASIS_U].y, m_Basis.m_vecAxes[OVERLAY_BASIS_U].z );
		pEntity->NotifyChildKeyChanged( this, "BasisU", szValue );

		sprintf( szValue, "%g %g %g", m_Basis.m_vecAxes[OVERLAY_BASIS_V].x, m_Basis.m_vecAxes[OVERLAY_BASIS_V].y, m_Basis.m_vecAxes[OVERLAY_BASIS_V].z );
		pEntity->NotifyChildKeyChanged( this, "BasisV", szValue );

		sprintf( szValue, "%g %g %g", m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL].x, m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL].y, m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL].z );
		pEntity->NotifyChildKeyChanged( this, "BasisNormal", szValue );
	}
}

//=============================================================================
//
// Basis - Legacy support!
//

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::Basis_BuildFromSideList( void )
{
	// Initialization (don't have or couldn't find the basis face)
	if ( m_Faces.Count() > 0 )
	{
		Basis_Init( m_Faces.Element( 0 ) );
	}
	else
	{
		m_Basis.m_pFace = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//   Input: iAxis - 0, 1, 2 (u, v, n)
//          iComponet - 0, 1, 2 (x, y, z)
//-----------------------------------------------------------------------------
void CMapOverlay::Basis_ToggleAxesFlip( int iAxis, int iComponent )
{
	if ( iAxis < 0 || iAxis > 2 || iComponent < 0 || iComponent > 2 )
		return;

	int nValue = ( 1 << iComponent );
	m_Basis.m_nAxesFlip[iAxis] ^= nValue;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMapOverlay::Basis_IsFlipped( int iAxis, int iComponent )
{
	if ( iAxis < 0 || iAxis > 2 || iComponent < 0 || iComponent > 2 )
		return false;
	
	int nValue = ( 1 << iComponent );
	return ( ( m_Basis.m_nAxesFlip[iAxis] & nValue ) != 0 );
}

//=============================================================================
//
// Handles Functions
//

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::Handles_Clear( void )
{
	m_Handles.m_iHit = -1;

	for ( int iHandle = 0; iHandle < OVERLAY_HANDLES_COUNT; iHandle++ )
	{
		m_Handles.m_vec3D[iHandle].Init();
	}

	m_Handles.m_vecBasisCoords[0].Init( -OVERLAY_INITSIZE, -OVERLAY_INITSIZE );
	m_Handles.m_vecBasisCoords[1].Init( -OVERLAY_INITSIZE, OVERLAY_INITSIZE );
	m_Handles.m_vecBasisCoords[2].Init( OVERLAY_INITSIZE, OVERLAY_INITSIZE );
	m_Handles.m_vecBasisCoords[3].Init( OVERLAY_INITSIZE, -OVERLAY_INITSIZE );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::Handles_Init( CMapFace *pFace )
{
	IEditorTexture *pTexture = g_Textures.FindActiveTexture( GetDefaultTextureName() );
	int nWidth = pTexture->GetImageWidth();
	int nHeight = pTexture->GetImageHeight();

	// Half-height (width) and 1/4 scale
	int nWidthHalf = nWidth / 8;
	int nHeightHalf = nHeight / 8;

	m_Handles.m_vecBasisCoords[0].Init( -nWidthHalf, -nHeightHalf );
	m_Handles.m_vecBasisCoords[1].Init( -nWidthHalf, nHeightHalf );
	m_Handles.m_vecBasisCoords[2].Init( nWidthHalf, nHeightHalf );
	m_Handles.m_vecBasisCoords[3].Init( nWidthHalf, -nHeightHalf );

	Handles_Build3D();

	Handles_UpdateParentKey();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::Handles_Build3D( void )
{
	// Verify that we have a valid basis to build the handles from.
	if ( !Basis_IsValid() )
		return;

	for ( int iHandle = 0; iHandle < OVERLAY_HANDLES_COUNT; iHandle++ )
	{
		Vector vecHandle;
		OverlayUVToOverlayPlane( m_Handles.m_vecBasisCoords[iHandle], vecHandle );
		OverlayPlaneToSurfFromList( vecHandle, m_Handles.m_vec3D[iHandle] );
	}

	Handles_FixOrder();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::Handles_Render3D( CRender3D *pRender )
{
	// Set the render mode to "flat."
	pRender->PushRenderMode( RENDER_MODE_FLAT );

	// Set the color, should be based on selection.
	unsigned char ucColor[4];
	ucColor[0] = ucColor[1] = ucColor[2] = ucColor[3] = 255;

	unsigned char ucSelectColor[4];
	ucSelectColor[0] = ucSelectColor[3] = 255;
	ucSelectColor[1] = ucSelectColor[2] = 0;

	pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_SQUARE );

	for ( int iHandle = 0; iHandle < OVERLAY_HANDLES_COUNT; iHandle++ )
	{
		pRender->BeginRenderHitTarget( this, iHandle );
		if ( m_Handles.m_iHit == iHandle )
		{
			pRender->SetHandleColor( ucSelectColor[0], ucSelectColor[1], ucSelectColor[2] );
		}
		else
		{
			pRender->SetHandleColor( ucColor[0], ucColor[1], ucColor[2] );
		}

		pRender->DrawHandle( m_Handles.m_vec3D[iHandle] );

		pRender->EndRenderHitTarget();
	}

	pRender->PopRenderMode();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::Handles_SurfToOverlayPlane( CMapFace *pFace, Vector const &vecSurf, Vector &vecPoint )
{
	Vector vecWorld;
	if ( pFace->HasDisp() )
	{
		EditDispHandle_t handle = pFace->GetDisp();
		CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );
		pDisp->SurfToBaseFacePlane( vecSurf, vecWorld );
	}
	else
	{
		vecWorld = vecSurf;
	}

	WorldToOverlayPlane( vecWorld, vecPoint );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::Handles_Copy( Handles_t *pSrc, Handles_t *pDst )
{
	pDst->m_iHit = pSrc->m_iHit;

	for ( int iHandle = 0; iHandle < OVERLAY_HANDLES_COUNT; ++iHandle )
	{
		pDst->m_vecBasisCoords[iHandle] = pSrc->m_vecBasisCoords[iHandle];
		pDst->m_vec3D[iHandle] = pSrc->m_vec3D[iHandle];
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::Handles_UpdateParentKey( void )
{
	char szValue[80];

	CMapEntity *pEntity = ( CMapEntity* )GetParent();
	if ( pEntity )
	{
		sprintf( szValue, "%g %g %g", m_Handles.m_vecBasisCoords[0].x, m_Handles.m_vecBasisCoords[0].y, ( float )m_Basis.m_nAxesFlip[0] );
		pEntity->NotifyChildKeyChanged( this, "uv0", szValue );

		sprintf( szValue, "%g %g %g", m_Handles.m_vecBasisCoords[1].x, m_Handles.m_vecBasisCoords[1].y, ( float )m_Basis.m_nAxesFlip[1] );
		pEntity->NotifyChildKeyChanged( this, "uv1", szValue );

		sprintf( szValue, "%g %g %g", m_Handles.m_vecBasisCoords[2].x, m_Handles.m_vecBasisCoords[2].y, ( float )m_Basis.m_nAxesFlip[2] );
		pEntity->NotifyChildKeyChanged( this, "uv2", szValue );

		sprintf( szValue, "%g %g %g", m_Handles.m_vecBasisCoords[3].x, m_Handles.m_vecBasisCoords[3].y, 0.0f );
		pEntity->NotifyChildKeyChanged( this, "uv3", szValue );
	}
}

//=============================================================================
//
// ClipFace Functions
//

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CMapOverlay::ClipFace_t *CMapOverlay::ClipFace_Create( int nSize )
{
	ClipFace_t *pClipFace = new ClipFace_t;
	if ( pClipFace )
	{
		pClipFace->m_nPointCount = nSize;
		if ( nSize > 0 )
		{
			pClipFace->m_aPoints.SetSize( nSize );
			pClipFace->m_aDispPointUVs.SetSize( nSize );
			
			for ( int iCoord = 0; iCoord < NUM_CLIPFACE_TEXCOORDS; iCoord++ )
			{
				pClipFace->m_aTexCoords[iCoord].SetSize( nSize );
			}

			pClipFace->m_aBlends.SetSize( nSize );
			
			for ( int iPoint = 0; iPoint < nSize; iPoint++ )
			{
				pClipFace->m_aPoints[iPoint].Init();
				pClipFace->m_aDispPointUVs[iPoint].Init();
				pClipFace->m_aBlends[iPoint].Init();
				
				for ( int iCoord = 0; iCoord < NUM_CLIPFACE_TEXCOORDS; iCoord++ )
				{
					pClipFace->m_aTexCoords[iCoord][iPoint].Init();
				}
			}
		}
	}

	return pClipFace;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::ClipFace_Destroy( ClipFace_t **ppClipFace )
{
	if( *ppClipFace )
	{
		delete *ppClipFace;
		*ppClipFace = NULL;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CMapOverlay::ClipFace_t *CMapOverlay::ClipFace_Copy( ClipFace_t *pSrc )
{
	ClipFace_t *pDst = ClipFace_Create( pSrc->m_nPointCount );
	if ( pDst )
	{
		for ( int iPoint = 0; iPoint < pSrc->m_nPointCount; iPoint++ )
		{
			pDst->m_aPoints[iPoint] = pSrc->m_aPoints[iPoint];
			pDst->m_aDispPointUVs[iPoint] = pSrc->m_aDispPointUVs[iPoint];
			for ( int iTexCoord=0; iTexCoord < NUM_CLIPFACE_TEXCOORDS; iTexCoord++ )
			{
				pDst->m_aTexCoords[iTexCoord][iPoint] = pSrc->m_aTexCoords[iTexCoord][iPoint];
			}

			pDst->m_aBlends[iPoint].m_nType = pSrc->m_aBlends[iPoint].m_nType;
			for ( int iBlend = 0; iBlend < 3; iBlend++ )
			{
				pDst->m_aBlends[iPoint].m_iPoints[iBlend] = pSrc->m_aBlends[iPoint].m_iPoints[iBlend];
				pDst->m_aBlends[iPoint].m_flBlends[iBlend] = pSrc->m_aBlends[iPoint].m_flBlends[iBlend];
			}
		}
	}

	return pDst;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::ClipFace_GetBounds( ClipFace_t *pClipFace, Vector &vecMin, Vector &vecMax )
{
	if ( pClipFace )
	{
		vecMin = vecMax = pClipFace->m_aPoints.Element( 0 );
		
		for ( int iPoints = 1; iPoints < pClipFace->m_nPointCount; iPoints++ )
		{
			Vector vecPoint = pClipFace->m_aPoints.Element( iPoints );
			
			// Min
			if ( vecMin.x > vecPoint.x ) { vecMin.x = vecPoint.x; }
			if ( vecMin.y > vecPoint.y ) { vecMin.y = vecPoint.y; }
			if ( vecMin.z > vecPoint.z ) { vecMin.z = vecPoint.z; }
			
			// Max
			if ( vecMax.x < vecPoint.x ) { vecMax.x = vecPoint.x; }
			if ( vecMax.y < vecPoint.y ) { vecMax.y = vecPoint.y; }
			if ( vecMax.z < vecPoint.z ) { vecMax.z = vecPoint.z; }
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::ClipFace_Clip( ClipFace_t *pClipFace, cplane_t *pClipPlane, float flEpsilon,
							     ClipFace_t **ppFront, ClipFace_t **ppBack )
{
	if ( !pClipFace )
		return;

	float flDists[128];
	int	nSides[128];
	int nSideCounts[3];

	// Initialize
	*ppFront = *ppBack = NULL;

	// Determine "sidedness" of all the polygon points.
	nSideCounts[0] = nSideCounts[1] = nSideCounts[2] = 0;
	int iPoint;
	for ( iPoint = 0; iPoint < pClipFace->m_nPointCount; iPoint++ )
	{
		flDists[iPoint] = pClipPlane->normal.Dot( pClipFace->m_aPoints.Element( iPoint ) ) - pClipPlane->dist;

		if ( flDists[iPoint] > flEpsilon )
		{
			nSides[iPoint] = SIDE_FRONT;
		}
		else if ( flDists[iPoint] < -flEpsilon )
		{
			nSides[iPoint] = SIDE_BACK;
		}
		else
		{
			nSides[iPoint] = SIDE_ON;
		}

		nSideCounts[nSides[iPoint]]++;
	}

	// Wrap around (close the polygon).
	nSides[iPoint] = nSides[0];
	flDists[iPoint] =  flDists[0];

	// All points in back - no split (copy face to back).
	if( !nSideCounts[SIDE_FRONT] )
	{
		*ppBack = ClipFace_Copy( pClipFace );
		return;
	}

	// All points in front - no split (copy face to front).
	if( !nSideCounts[SIDE_BACK] )
	{
		*ppFront = ClipFace_Copy( pClipFace );
		return;
	}

	// Build new front and back faces. Leave room for two extra points on each side because any
	// point might be on the plane, which would put it into both the front and back sides, and then
	// we need to allow for an additional vertex created by clipping.
	ClipFace_t *pFront = ClipFace_Create( pClipFace->m_nPointCount + 2 );
	ClipFace_t *pBack = ClipFace_Create( pClipFace->m_nPointCount + 2 );
	if ( !pFront || !pBack )
	{
		ClipFace_Destroy( &pFront );
		ClipFace_Destroy( &pBack );
		return;
	}

	// Reset the counts as they are used to build the surface.
	pFront->m_nPointCount = 0;
	pBack->m_nPointCount = 0;

	// For every point on the face being clipped, determine which side of the clipping plane it is on
	// and add it to a either a front list or a back list. Points that are on the plane are added to
	// both lists.
	for ( iPoint = 0; iPoint < pClipFace->m_nPointCount; iPoint++ )
	{
		// "On" clip plane.
		if ( nSides[iPoint] == SIDE_ON )
		{
			pFront->m_aPoints[pFront->m_nPointCount] = pClipFace->m_aPoints[iPoint];
			for ( int iTexCoord=0; iTexCoord < NUM_CLIPFACE_TEXCOORDS; iTexCoord++ )
				pFront->m_aTexCoords[iTexCoord][pFront->m_nPointCount] = pClipFace->m_aTexCoords[iTexCoord][iPoint];
			pFront->m_nPointCount++;

			pBack->m_aPoints[pBack->m_nPointCount] = pClipFace->m_aPoints[iPoint];
			for ( int iTexCoord=0; iTexCoord < NUM_CLIPFACE_TEXCOORDS; iTexCoord++ )
				pBack->m_aTexCoords[iTexCoord][pBack->m_nPointCount] = pClipFace->m_aTexCoords[iTexCoord][iPoint];
			pBack->m_nPointCount++;

			continue;
		}

		// "In back" of clip plane.
		if ( nSides[iPoint] == SIDE_BACK )
		{
			pBack->m_aPoints[pBack->m_nPointCount] = pClipFace->m_aPoints[iPoint];
			for ( int iTexCoord=0; iTexCoord < NUM_CLIPFACE_TEXCOORDS; iTexCoord++ )
				pBack->m_aTexCoords[iTexCoord][pBack->m_nPointCount] = pClipFace->m_aTexCoords[iTexCoord][iPoint];
			pBack->m_nPointCount++;
		}

		// "In front" of clip plane.
		if ( nSides[iPoint] == SIDE_FRONT )
		{
			pFront->m_aPoints[pFront->m_nPointCount] = pClipFace->m_aPoints[iPoint];
			for ( int iTexCoord=0; iTexCoord < NUM_CLIPFACE_TEXCOORDS; iTexCoord++ )
				pFront->m_aTexCoords[iTexCoord][pFront->m_nPointCount] = pClipFace->m_aTexCoords[iTexCoord][iPoint];
			pFront->m_nPointCount++;
		}

		if ( nSides[iPoint+1] == SIDE_ON || nSides[iPoint+1] == nSides[iPoint] )
			continue;

		// Split!
		float fraction = flDists[iPoint] / ( flDists[iPoint] - flDists[iPoint+1] );

		Vector vecPoint = pClipFace->m_aPoints[iPoint] + ( pClipFace->m_aPoints[(iPoint+1)%pClipFace->m_nPointCount] - pClipFace->m_aPoints[iPoint] ) * fraction;
		for ( int iTexCoord=0; iTexCoord < NUM_CLIPFACE_TEXCOORDS; iTexCoord++ )
		{
			Vector2D vecTexCoord = pClipFace->m_aTexCoords[iTexCoord][iPoint] + ( pClipFace->m_aTexCoords[iTexCoord][(iPoint+1)%pClipFace->m_nPointCount] - pClipFace->m_aTexCoords[iTexCoord][iPoint] ) * fraction;
			pFront->m_aTexCoords[iTexCoord][pFront->m_nPointCount] = vecTexCoord;
			pBack->m_aTexCoords[iTexCoord][pBack->m_nPointCount] = vecTexCoord;
		}
	
		pFront->m_aPoints[pFront->m_nPointCount] = vecPoint;
		pFront->m_nPointCount++;

		pBack->m_aPoints[pBack->m_nPointCount] = vecPoint;
		pBack->m_nPointCount++;
	}

	*ppFront = pFront;
	*ppBack = pBack;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::ClipFace_ClipBarycentric( ClipFace_t *pClipFace, cplane_t *pClipPlane, float flEpsilon,
									        int iClip, CMapDisp *pDisp,
									        ClipFace_t **ppFront, ClipFace_t **ppBack )
{
	if ( !pClipFace )
		return;

	float flDists[128];
	int nSides[128];
	int	nSideCounts[3];

	// Determine "sidedness" of all the polygon points.
	nSideCounts[0] = nSideCounts[1] = nSideCounts[2] = 0;
	int iPoint;
	for ( iPoint = 0; iPoint < pClipFace->m_nPointCount; iPoint++ )
	{
		flDists[iPoint] = pClipPlane->normal.Dot( pClipFace->m_aDispPointUVs.Element( iPoint ) ) - pClipPlane->dist;

		if ( flDists[iPoint] > flEpsilon )
		{
			nSides[iPoint] = SIDE_FRONT;
		}
		else if ( flDists[iPoint] < -flEpsilon )
		{
			nSides[iPoint] = SIDE_BACK;
		}
		else
		{
			nSides[iPoint] = SIDE_ON;
		}

		nSideCounts[nSides[iPoint]]++;
	}

	// Wrap around (close the polygon).
	nSides[iPoint] = nSides[0];
	flDists[iPoint] =  flDists[0];

	// All points in back - no split (copy face to back).
	if( !nSideCounts[SIDE_FRONT] )
	{
		*ppBack = ClipFace_Copy( pClipFace );
		return;
	}

	// All points in front - no split (copy face to front).
	if( !nSideCounts[SIDE_BACK] )
	{
		*ppFront = ClipFace_Copy( pClipFace );
		return;
	}

	// Build new front and back faces.
	// NOTE: We are allowing to go over by 2 and then destroy the surface later.  The old system
	//       allowed for some bad data and we need to be able to load the map and destroy the surface!
	int nMaxPointCount = pClipFace->m_nPointCount + 1;
	ClipFace_t *pFront = ClipFace_Create( nMaxPointCount + 2 );
	ClipFace_t *pBack = ClipFace_Create( nMaxPointCount + 2 );
	if ( !pFront || !pBack )
	{
		ClipFace_Destroy( &pFront );
		ClipFace_Destroy( &pBack );
		return;
	}

	// Reset the counts as they are used to build the surface.
	pFront->m_nPointCount = 0;
	pBack->m_nPointCount = 0;

	for ( iPoint = 0; iPoint < pClipFace->m_nPointCount; iPoint++ )
	{
		// "On" clip plane.
		if ( nSides[iPoint] == SIDE_ON )
		{
			pFront->m_aPoints[pFront->m_nPointCount] = pClipFace->m_aPoints[iPoint];
			pFront->m_aDispPointUVs[pFront->m_nPointCount] = pClipFace->m_aDispPointUVs[iPoint];
			
			for ( int iTexCoord=0; iTexCoord < NUM_CLIPFACE_TEXCOORDS; iTexCoord++ )
				pFront->m_aTexCoords[iTexCoord][pFront->m_nPointCount] = pClipFace->m_aTexCoords[iTexCoord][iPoint];
			
			ClipFace_CopyBlendFrom( pFront, &pClipFace->m_aBlends[iPoint] );
			pFront->m_nPointCount++;

			pBack->m_aPoints[pBack->m_nPointCount] = pClipFace->m_aPoints[iPoint];
			pBack->m_aDispPointUVs[pBack->m_nPointCount] = pClipFace->m_aDispPointUVs[iPoint];

			for ( int iTexCoord=0; iTexCoord < NUM_CLIPFACE_TEXCOORDS; iTexCoord++ )
				pBack->m_aTexCoords[iTexCoord][pBack->m_nPointCount] = pClipFace->m_aTexCoords[iTexCoord][iPoint];

			ClipFace_CopyBlendFrom( pBack, &pClipFace->m_aBlends[iPoint] );
			pBack->m_nPointCount++;

			continue;
		}

		// "In back" of clip plane.
		if ( nSides[iPoint] == SIDE_BACK )
		{
			pBack->m_aPoints[pBack->m_nPointCount] = pClipFace->m_aPoints[iPoint];
			pBack->m_aDispPointUVs[pBack->m_nPointCount] = pClipFace->m_aDispPointUVs[iPoint];

			for ( int iTexCoord=0; iTexCoord < NUM_CLIPFACE_TEXCOORDS; iTexCoord++ )
				pBack->m_aTexCoords[iTexCoord][pBack->m_nPointCount] = pClipFace->m_aTexCoords[iTexCoord][iPoint];
			
			ClipFace_CopyBlendFrom( pBack, &pClipFace->m_aBlends[iPoint] );
			pBack->m_nPointCount++;
		}

		// "In front" of clip plane.
		if ( nSides[iPoint] == SIDE_FRONT )
		{
			pFront->m_aPoints[pFront->m_nPointCount] = pClipFace->m_aPoints[iPoint];
			pFront->m_aDispPointUVs[pFront->m_nPointCount] = pClipFace->m_aDispPointUVs[iPoint];

			for ( int iTexCoord=0; iTexCoord < NUM_CLIPFACE_TEXCOORDS; iTexCoord++ )
				pFront->m_aTexCoords[iTexCoord][pFront->m_nPointCount] = pClipFace->m_aTexCoords[iTexCoord][iPoint];

			ClipFace_CopyBlendFrom( pFront, &pClipFace->m_aBlends[iPoint] );
			pFront->m_nPointCount++;
		}

		if ( nSides[iPoint+1] == SIDE_ON || nSides[iPoint+1] == nSides[iPoint] )
			continue;

		// Split!
		float fraction = flDists[iPoint] / ( flDists[iPoint] - flDists[iPoint+1] );

		Vector vecPoint = pClipFace->m_aPoints[iPoint] + ( pClipFace->m_aPoints[(iPoint+1)%pClipFace->m_nPointCount] - pClipFace->m_aPoints[iPoint] ) * fraction;
		Vector vecDispPointUV = pClipFace->m_aDispPointUVs[iPoint] + ( pClipFace->m_aDispPointUVs[(iPoint+1)%pClipFace->m_nPointCount] - pClipFace->m_aDispPointUVs[iPoint] ) * fraction;

		Vector2D vecUV, vecTexCoord;
		PointInQuadToBarycentric( m_pOverlayFace->m_aPoints[0], m_pOverlayFace->m_aPoints[3], 
			                      m_pOverlayFace->m_aPoints[2], m_pOverlayFace->m_aPoints[1],
								  vecPoint, vecUV );

		vecUV.x = clamp( vecUV.x, 0.0f, 1.0f );
		vecUV.y = clamp( vecUV.y, 0.0f, 1.0f );

		for ( int iTexCoord=0; iTexCoord < NUM_CLIPFACE_TEXCOORDS; iTexCoord++ )
		{
			TexCoordInQuadFromBarycentric( m_pOverlayFace->m_aTexCoords[iTexCoord][0], m_pOverlayFace->m_aTexCoords[iTexCoord][3], 
										m_pOverlayFace->m_aTexCoords[iTexCoord][2], m_pOverlayFace->m_aTexCoords[iTexCoord][1],
										vecUV, vecTexCoord );
			
			pFront->m_aTexCoords[iTexCoord][pFront->m_nPointCount] = vecTexCoord;
			pBack->m_aTexCoords[iTexCoord][pBack->m_nPointCount] = vecTexCoord;
		}

		pFront->m_aPoints[pFront->m_nPointCount] = vecPoint;
		pFront->m_aDispPointUVs[pFront->m_nPointCount] = vecDispPointUV;
		ClipFace_BuildBlend( pFront, pDisp, pClipPlane, iClip, vecDispPointUV, vecPoint );
		pFront->m_nPointCount++;

		pBack->m_aPoints[pBack->m_nPointCount] = vecPoint;
		pBack->m_aDispPointUVs[pBack->m_nPointCount] = vecDispPointUV;
		ClipFace_BuildBlend( pBack, pDisp, pClipPlane, iClip, vecDispPointUV, vecPoint );
		pBack->m_nPointCount++;
	}

	// Check for a bad surface.
	if ( ( pFront->m_nPointCount > nMaxPointCount ) || ( pBack->m_nPointCount > nMaxPointCount ) )
		return;

	*ppFront = pFront;
	*ppBack = pBack;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::ClipFace_PreClipDisp( ClipFace_t *pClipFace, CMapDisp *pDisp )
{
	// Valid clip face and/or displacement surface.
	if ( !pClipFace || !pDisp )
		return;

	// Transform all of the overlay points into disp uv space. 
	for ( int iPoint = 0; iPoint < pClipFace->m_nPointCount; iPoint++ )
	{
		Vector2D vecTmp;
		pDisp->BaseFacePlaneToDispUV( pClipFace->m_aPoints[iPoint], vecTmp );
				
		pClipFace->m_aDispPointUVs[iPoint].x = clamp(vecTmp.x, 0.0f, 1.0f);
		pClipFace->m_aDispPointUVs[iPoint].y = clamp(vecTmp.y, 0.0f, 1.0f);
		pClipFace->m_aDispPointUVs[iPoint].z = 0.0f;
	}

	// Set initial point barycentric blend types.
	for ( int iPoint = 0; iPoint < pClipFace->m_nPointCount; ++iPoint )
	{
		Vector2D vecDispUV;
		vecDispUV.x = pClipFace->m_aDispPointUVs[iPoint].x;
		vecDispUV.y = pClipFace->m_aDispPointUVs[iPoint].y;

		int iTris[3];
		Vector2D vecVertsUV[3];
		GetTriVerts( pDisp, vecDispUV, iTris, vecVertsUV );

		float flCoefs[3];
		if ( ClipFace_CalcBarycentricCooefs( pDisp, vecVertsUV, vecDispUV, flCoefs ) )
		{
			ClipFace_ResolveBarycentricClip( pDisp, pClipFace, iPoint, vecDispUV, flCoefs, iTris, vecVertsUV );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::ClipFace_PostClipDisp( void )
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CMapOverlay::ClipFace_CalcBarycentricCooefs( CMapDisp *pDisp, Vector2D *pVertsUV, 
												  const Vector2D &vecPointUV, float *pCoefs )
{
	// Area in disp UV space is always the same.
	float flTotalArea = 0.5f;		
	float flOOTotalArea = 1.0f / flTotalArea;

	int nInterval = pDisp->GetWidth();
	Vector2D vecScaledPointUV = vecPointUV * ( nInterval - 1.000001f );

	Vector2D vecSegment0, vecSegment1;

	// Get the area for cooeficient 0 (pt, v1, v2).
	vecSegment0 = pVertsUV[1] - vecScaledPointUV;
	vecSegment1 = pVertsUV[2] - vecScaledPointUV;
	// Cross
	float flSubArea = ( ( vecSegment1.x * vecSegment0.y ) - ( vecSegment0.x * vecSegment1.y ) ) * 0.5f;
	pCoefs[0] = flSubArea * flOOTotalArea;

	// Get the area for cooeficient 1 (v0, pt, v2).
	vecSegment0 = vecScaledPointUV - pVertsUV[0];
	vecSegment1 = pVertsUV[2] - pVertsUV[0];
	// Cross
	flSubArea = ( ( vecSegment1.x * vecSegment0.y ) - ( vecSegment0.x * vecSegment1.y ) ) * 0.5f;
	pCoefs[1] = flSubArea * flOOTotalArea;

	// Get the area for cooeficient 2 (v0, v1, pt).
	vecSegment0 = pVertsUV[1] - pVertsUV[0];
	vecSegment1 = vecScaledPointUV - pVertsUV[0];
	// Cross
	flSubArea = ( ( vecSegment1.x * vecSegment0.y ) - ( vecSegment0.x * vecSegment1.y ) ) * 0.5f;
	pCoefs[2] = flSubArea * flOOTotalArea;

	float flCoefTotal = pCoefs[0] + pCoefs[1] + pCoefs[2];
	if ( FloatMakePositive( 1.0f - flCoefTotal ) < 0.00001f )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::ClipFace_ResolveBarycentricClip( CMapDisp *pDisp, ClipFace_t *pClipFace, int iClipFacePoint, 
										           const Vector2D &vecPointUV, float *pCoefs, 
										           int *pTris, Vector2D *pVertsUV )
{
	int nInterval = pDisp->GetWidth();
	Vector2D vecScaledPointUV = vecPointUV * ( nInterval - 1.000001f );

	// Find the number of coefficients "equal" to zero.
	int nZeroCount = 0;
	bool bZeroPoint[3];
	for ( int iVert = 0; iVert < 3; ++iVert )
	{
		bZeroPoint[iVert] = false;
		if ( fabs( pCoefs[iVert] ) < OVERLAY_BARYCENTRIC_EPSILON )
		{
			nZeroCount++;
			bZeroPoint[iVert] = true;
		}
	}
	
	// Check for points - set to a point.
	if ( nZeroCount == 2 )
	{
		for ( int iVert = 0; iVert < 3; ++iVert )
		{
			if ( !bZeroPoint[iVert] )
			{
				pClipFace->m_aBlends[iClipFacePoint].m_nType = OVERLAY_BLENDTYPE_VERT;
				pClipFace->m_aBlends[iClipFacePoint].m_iPoints[0] = pTris[iVert];
				return;
			}
		}
	}
	
	// Check for edges - setup edge blend.
	if ( nZeroCount == 1 )
	{
		for ( int iVert = 0; iVert < 3; ++iVert )
		{
			if ( bZeroPoint[iVert] )
			{
				pClipFace->m_aBlends[iClipFacePoint].m_nType = OVERLAY_BLENDTYPE_EDGE;
				pClipFace->m_aBlends[iClipFacePoint].m_iPoints[0] = pTris[(iVert+1)%3];
				pClipFace->m_aBlends[iClipFacePoint].m_iPoints[1] = pTris[(iVert+2)%3];
				
				Vector2D vecLength1, vecLength2;
				vecLength1 = vecScaledPointUV - pVertsUV[(iVert+1)%3];
				vecLength2 = pVertsUV[(iVert+2)%3] - pVertsUV[(iVert+1)%3];
				float flBlend = vecLength1.Length() / vecLength2.Length();
				pClipFace->m_aBlends[iClipFacePoint].m_flBlends[0] = flBlend;
				return;
			}
		}
	}
	
	// Lies inside triangles - setup full barycentric blend.
	pClipFace->m_aBlends[iClipFacePoint].m_nType = OVERLAY_BLENDTYPE_BARY;
	pClipFace->m_aBlends[iClipFacePoint].m_iPoints[0] = pTris[0];
	pClipFace->m_aBlends[iClipFacePoint].m_iPoints[1] = pTris[1];
	pClipFace->m_aBlends[iClipFacePoint].m_iPoints[2] = pTris[2];
	pClipFace->m_aBlends[iClipFacePoint].m_flBlends[0] = pCoefs[0];
	pClipFace->m_aBlends[iClipFacePoint].m_flBlends[1] = pCoefs[1];
	pClipFace->m_aBlends[iClipFacePoint].m_flBlends[2] = pCoefs[2];
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CMapOverlay::ClipFace_GetAxisType( cplane_t *pClipPlane )
{
	if ( pClipPlane->normal[0] == 1.0f ) { return OVERLAY_ANGLE90; }
	if ( pClipPlane->normal[1] == 1.0f ) { return OVERLAY_ANGLE0; }
	if ( ( pClipPlane->normal[0] == 0.707f ) && ( pClipPlane->normal[1] == 0.707f ) ) { return OVERLAY_ANGLE45; }
	if ( ( pClipPlane->normal[0] == -0.707f ) && ( pClipPlane->normal[1] == 0.707f ) ) { return OVERLAY_ANGLE135; }

	return OVERLAY_ANGLE0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::ClipFace_BuildBlend( ClipFace_t *pClipFace, CMapDisp *pDisp, 
									   cplane_t *pClipPlane, int iClip, 
									   const Vector &vecUV, const Vector &vecPoint )
{
	// Get the displacement space interval.
	int nWidth = pDisp->GetWidth();
	int nHeight = pDisp->GetHeight();

	float flU = vecUV.x * ( nWidth - 1.000001f );
	float flV = vecUV.y * ( nHeight - 1.000001f );

	// find the triangle the "uv spot" resides in
	int nSnapU = static_cast<int>( flU );
	int nSnapV = static_cast<int>( flV );
	if ( nSnapU == ( nWidth - 1 ) ) { --nSnapU; }
	if ( nSnapV == ( nHeight - 1 ) ) { --nSnapV; }
	int nNextU = nSnapU + 1;
	int nNextV = nSnapV + 1;

	float flFracU = flU - static_cast<float>( nSnapU );
	float flFracV = flV - static_cast<float>( nSnapV );
		
	int iAxisType = ClipFace_GetAxisType( pClipPlane );
	switch( iAxisType )
	{
	case OVERLAY_ANGLE0:
		{
			// Vert type
			if ( fabs( flFracU ) < OVERLAY_DISPSPACE_EPSILON )
			{
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_nType = OVERLAY_BLENDTYPE_VERT;
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_iPoints[0] = ( nWidth * iClip ) + nSnapU;
			}
			// Edge type
			else
			{
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_nType = OVERLAY_BLENDTYPE_EDGE;
				int iPoint0 = ( nWidth * iClip ) + nSnapU;
				int iPoint1 = ( nWidth * iClip ) + nNextU;
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_iPoints[0] = iPoint0;
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_iPoints[1] = iPoint1;
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_flBlends[0] = flFracU;
			}
			return;
		}
	case OVERLAY_ANGLE45:
		{
			// Vert type
			if ( ( fabs( flFracU ) < OVERLAY_DISPSPACE_EPSILON ) &&
				 ( fabs( flFracV ) < OVERLAY_DISPSPACE_EPSILON ) )
			{
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_nType = OVERLAY_BLENDTYPE_VERT;
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_iPoints[0] = ( nWidth * nSnapV ) + nSnapU;
			}
			// Edge type
			else
			{
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_nType = OVERLAY_BLENDTYPE_EDGE;
				int iPoint0 = ( nWidth * nNextV ) + nSnapU;
				int iPoint1 = ( nWidth * nSnapV ) + nNextU;
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_iPoints[0] = iPoint0;
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_iPoints[1] = iPoint1;
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_flBlends[0] = flFracU;
			}
			return;
		}
	case OVERLAY_ANGLE90:
		{
			// Vert type
			if ( fabs( flFracV ) < OVERLAY_DISPSPACE_EPSILON )
			{
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_nType = OVERLAY_BLENDTYPE_VERT;
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_iPoints[0] = ( nWidth * nSnapV ) + iClip;
			}
			// Edge type
			else
			{
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_nType = OVERLAY_BLENDTYPE_EDGE;
				int iPoint0 = ( nWidth * nSnapV ) + iClip;
				int iPoint1 = ( nWidth * nNextV ) + iClip;
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_iPoints[0] = iPoint0;
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_iPoints[1] = iPoint1;
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_flBlends[0] = flFracV;
			}
			return;
		}
	case OVERLAY_ANGLE135:
		{
			// Vert type
			if ( ( fabs( flFracU ) < OVERLAY_DISPSPACE_EPSILON ) &&
				 ( fabs( flFracV ) < OVERLAY_DISPSPACE_EPSILON ) )
			{
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_nType = OVERLAY_BLENDTYPE_VERT;
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_iPoints[0] = ( nWidth * nSnapV ) + nSnapU;
			}
			// Edge type
			else
			{
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_nType = OVERLAY_BLENDTYPE_EDGE;
				int iPoint0 = ( nWidth * nSnapV ) + nSnapU;
				int iPoint1 = ( nWidth * nNextV ) + nNextU;
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_iPoints[0] = iPoint0;
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_iPoints[1] = iPoint1;
				pClipFace->m_aBlends[pClipFace->m_nPointCount].m_flBlends[0] = flFracU;
			}
			return;
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::ClipFace_CopyBlendFrom( ClipFace_t *pClipFace, BlendData_t *pBlendFrom )
{
	pClipFace->m_aBlends[pClipFace->m_nPointCount].m_nType = pBlendFrom->m_nType;
	for ( int iPoint = 0; iPoint < 3; iPoint++ )
	{
		pClipFace->m_aBlends[pClipFace->m_nPointCount].m_iPoints[iPoint] = pBlendFrom->m_iPoints[iPoint];
		pClipFace->m_aBlends[pClipFace->m_nPointCount].m_flBlends[iPoint] = pBlendFrom->m_flBlends[iPoint];
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::ClipFace_BuildFacesFromBlendedData( ClipFace_t *pClipFace )
{
	if( pClipFace->m_pBuildFace->HasDisp() )
	{
		EditDispHandle_t handle = pClipFace->m_pBuildFace->GetDisp();
		CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );

		Vector vecPos[3];
		for ( int iPoint = 0; iPoint < pClipFace->m_nPointCount; iPoint++ )
		{
			if ( pClipFace->m_aBlends[iPoint].m_nType == OVERLAY_BLENDTYPE_VERT )
			{
				pDisp->GetVert( pClipFace->m_aBlends[iPoint].m_iPoints[0], vecPos[0] );
				pClipFace->m_aPoints[iPoint] = vecPos[0];
			}
			else if ( pClipFace->m_aBlends[iPoint].m_nType == OVERLAY_BLENDTYPE_EDGE )
			{
				pDisp->GetVert( pClipFace->m_aBlends[iPoint].m_iPoints[0], vecPos[0] );
				pDisp->GetVert( pClipFace->m_aBlends[iPoint].m_iPoints[1], vecPos[1] );
				pClipFace->m_aPoints[iPoint] = vecPos[0] + ( vecPos[1] - vecPos[0] ) * pClipFace->m_aBlends[iPoint].m_flBlends[0];
			}
			else if ( pClipFace->m_aBlends[iPoint].m_nType == OVERLAY_BLENDTYPE_BARY )
			{
				pDisp->GetVert( pClipFace->m_aBlends[iPoint].m_iPoints[0], vecPos[0] );
				pDisp->GetVert( pClipFace->m_aBlends[iPoint].m_iPoints[1], vecPos[1] );
				pDisp->GetVert( pClipFace->m_aBlends[iPoint].m_iPoints[2], vecPos[2] );
				pClipFace->m_aPoints[iPoint] = ( vecPos[0] * pClipFace->m_aBlends[iPoint].m_flBlends[0] ) +
					                           ( vecPos[1] * pClipFace->m_aBlends[iPoint].m_flBlends[1] ) +
								               ( vecPos[2] * pClipFace->m_aBlends[iPoint].m_flBlends[2] );
			}
		}
	}
}


//=============================================================================
//
// CMapOverlay Material Functions
//

int MaxComponent( const Vector &v0 )
{
	int nMax = 0;
	if ( FloatMakePositive( v0[1] ) > FloatMakePositive( v0[nMax] ) )
	{
		nMax = 1;
	}

	if ( FloatMakePositive( v0[2] ) > FloatMakePositive( v0[nMax] ) )
	{
		nMax = 2;
	}

	return nMax;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::Material_Clear( void )
{
	m_Material.m_pTexture = NULL;
	m_Material.m_vecTextureU.Init( 0.0f, 1.0f );
	m_Material.m_vecTextureV.Init( 0.0f, 1.0f );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::Material_TexCoordInit( void )
{
	int nMaxU = MaxComponent( m_Basis.m_vecAxes[OVERLAY_BASIS_U] );
	int nMaxV = MaxComponent( m_Basis.m_vecAxes[OVERLAY_BASIS_V] );

	bool bUPos = m_Basis.m_vecAxes[OVERLAY_BASIS_U][nMaxU] >= 0.0f;
	bool bVPos = m_Basis.m_vecAxes[OVERLAY_BASIS_V][nMaxV] >= 0.0f;

	m_Material.m_vecTextureU.Init( 0.0f, 1.0f );
	m_Material.m_vecTextureV.Init( 1.0f, 0.0f );

	if ( ( bUPos && !bVPos ) || ( !bUPos && bVPos ) )
	{
		m_Material.m_vecTextureU.Init( 1.0f, 0.0f );
		m_Material.m_vecTextureV.Init( 0.0f, 1.0f );
	}

	Material_UpdateParentKey();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::Material_Copy( Material_t *pSrc, Material_t *pDst )
{
	pDst->m_pTexture = pSrc->m_pTexture;
	pDst->m_vecTextureU = pSrc->m_vecTextureU;
	pDst->m_vecTextureV = pSrc->m_vecTextureV;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::Material_UpdateParentKey( void )
{
	char szValue[80];

	CMapEntity *pEntity = ( CMapEntity* )GetParent();
	if ( pEntity )
	{
		sprintf( szValue, "%g", m_Material.m_vecTextureU.x );
		pEntity->NotifyChildKeyChanged( this, "StartU", szValue );

		sprintf( szValue, "%g", m_Material.m_vecTextureU.y );
		pEntity->NotifyChildKeyChanged( this, "EndU", szValue );

		sprintf( szValue, "%g", m_Material.m_vecTextureV.x );
		pEntity->NotifyChildKeyChanged( this, "StartV", szValue );

		sprintf( szValue, "%g", m_Material.m_vecTextureV.y );
		pEntity->NotifyChildKeyChanged( this, "EndV", szValue );
	}
}

//=============================================================================
//
// CMapOverlay Functions
//

//-----------------------------------------------------------------------------
// Purpose: Construct a CMapOverlay instance.
//-----------------------------------------------------------------------------
CMapOverlay::CMapOverlay() : CMapSideList( "sides" )
{
	Basis_Clear();
	Handles_Clear();
	Material_Clear();

	m_bLoaded = false;
	m_pOverlayFace = NULL;
	m_uiFlags = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destruct a CMapOverlay instance.
//-----------------------------------------------------------------------------
CMapOverlay::~CMapOverlay()
{
	ClipFace_Destroy( &m_pOverlayFace );
	m_aRenderFaces.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CMapClass *CMapOverlay::CreateMapOverlay( CHelperInfo *pInfo, CMapEntity *pParent )
{
	CMapOverlay *pOverlay = new CMapOverlay;
	return pOverlay;
}

//-----------------------------------------------------------------------------
// Purpose: Called after the entire map has been loaded. This allows the object
//			to perform any linking with other map objects or to do other operations
//			that require all world objects to be present.
// Input  : pWorld - The world that we are in.
//-----------------------------------------------------------------------------
void CMapOverlay::PostloadWorld( CMapWorld *pWorld )
{
	CMapSideList::PostloadWorld( pWorld );

	// Support older overlay versions which didn't have specific basis axes.
	if ( !Basis_IsValid() )
	{
		Basis_BuildFromSideList();
	}

	Handles_Build3D();
	DoClip();
	CalcBounds();
	m_bLoaded = true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CMapClass *CMapOverlay::Copy( bool bUpdateDependencies )
{
	CMapOverlay *pCopy = new CMapOverlay;
	if ( pCopy )
	{
		pCopy->CopyFrom( this, bUpdateDependencies );
	}

	return pCopy;
}

void CMapOverlay::Handles_FixOrder()
{
	static bool s_FixingHandles = false;

	// make sure that handle order and plane normal are in sync so CCW culling works correctly
	Vector vNormal = GetNormalFromPoints( m_Handles.m_vec3D[0], m_Handles.m_vec3D[1], m_Handles.m_vec3D[2] );

	if ( DotProduct( vNormal, m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL]) < 0.5 )
	{
		// dont try to fix twice
		if ( s_FixingHandles )
		{
			Assert( !s_FixingHandles );
			return;
		}

		s_FixingHandles = true;

		// Flip handles.
		Vector2D vecCoords[OVERLAY_HANDLES_COUNT];
		for ( int iHandle = 0; iHandle < OVERLAY_HANDLES_COUNT; iHandle++ )
		{
			vecCoords[4-iHandle-1] = m_Handles.m_vecBasisCoords[iHandle];
		}

		for ( int iHandle = 0; iHandle < OVERLAY_HANDLES_COUNT; iHandle++ )
		{
			m_Handles.m_vecBasisCoords[iHandle] = vecCoords[iHandle];
		}

		// rebuild handles
		
		Handles_Build3D();

		s_FixingHandles = false;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CMapClass *CMapOverlay::CopyFrom( CMapClass *pObject, bool bUpdateDependencies )
{
	// Verify the object is of the correct type and cast.
	Assert( pObject->IsMapClass( MAPCLASS_TYPE( CMapOverlay ) ) );
	CMapOverlay *pFrom = ( CMapOverlay* )pObject;
	if ( pFrom )
	{
		// Copy the parent class data.
		CMapSideList::CopyFrom( pObject, bUpdateDependencies );

		// Copy basis data.
		Basis_Copy( &pFrom->m_Basis, &m_Basis );

		// Copy handle data.
		Handles_Copy( &pFrom->m_Handles, &m_Handles );

		// Copy material data.
		Material_Copy( &pFrom->m_Material, &m_Material );
	}

	return this;
}

//-----------------------------------------------------------------------------
// Purpose: Notify me when a key has had a data change, so the overlay can
//          update itself appropriately.
//   Input: szKey - the key that changed
//          szValue - the new value (key/data pair)
//-----------------------------------------------------------------------------
void CMapOverlay::OnParentKeyChanged( const char* szKey, const char* szValue )
{
	// Pass this to the sidelist first.
	CMapSideList::OnParentKeyChanged( szKey, szValue );

	// Read side data.
	if ( !stricmp( szKey, "sides" ) )	
	{ 
		if ( m_Faces.Count() > 0 )
		{
			Basis_SetFace( m_Faces.Element( 0 ) );
		}
	}

	// Read geometry data.
	float flDummy;
	if ( !stricmp( szKey, "uv0" ) )     
	{ 
		sscanf( szValue, "%f %f %f", &m_Handles.m_vecBasisCoords[0].x, &m_Handles.m_vecBasisCoords[0].y, &flDummy ); 
		m_Basis.m_nAxesFlip[0] = ( int )flDummy;
	}
	if ( !stricmp( szKey, "uv1" ) )     
	{ 
		sscanf( szValue, "%f %f %f", &m_Handles.m_vecBasisCoords[1].x, &m_Handles.m_vecBasisCoords[1].y, &flDummy ); 
		m_Basis.m_nAxesFlip[1] = ( int )flDummy; 
	}
	if ( !stricmp( szKey, "uv2" ) )     
	{ 
		sscanf( szValue, "%f %f %f", &m_Handles.m_vecBasisCoords[2].x, &m_Handles.m_vecBasisCoords[2].y, &flDummy ); 
		m_Basis.m_nAxesFlip[2] = ( int )flDummy; 
	}
	if ( !stricmp( szKey, "uv3" ) )     
	{ 
		sscanf( szValue, "%f %f %f", &m_Handles.m_vecBasisCoords[3].x, &m_Handles.m_vecBasisCoords[3].y, &flDummy ); 
	}

	// Read basis data.
	if ( !stricmp( szKey, "BasisOrigin" ) )     
	{ 
		sscanf( szValue, "%f %f %f", &m_Basis.m_vecOrigin.x, &m_Basis.m_vecOrigin.y, &m_Basis.m_vecOrigin.z ); 
	}

	if ( !stricmp( szKey, "BasisU" ) )     
	{ 
		sscanf( szValue, "%f %f %f", &m_Basis.m_vecAxes[OVERLAY_BASIS_U].x, &m_Basis.m_vecAxes[OVERLAY_BASIS_U].y, &m_Basis.m_vecAxes[OVERLAY_BASIS_U].z ); 
	}

	if ( !stricmp( szKey, "BasisV" ) )     
	{ 
		sscanf( szValue, "%f %f %f", &m_Basis.m_vecAxes[OVERLAY_BASIS_V].x, &m_Basis.m_vecAxes[OVERLAY_BASIS_V].y, &m_Basis.m_vecAxes[OVERLAY_BASIS_V].z ); 
	}

	if ( !stricmp( szKey, "BasisNormal" ) )     
	{ 
		sscanf( szValue, "%f %f %f", &m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL].x, &m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL].y, &m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL].z ); 
	}

	// Read material data.
	if ( !stricmp( szKey, "material" ) )
	{
		// Get the new material.
		IEditorTexture *pTex = g_Textures.FindActiveTexture( szValue );
		if ( !pTex )
			return;

		// Save the new material.
		m_Material.m_pTexture = pTex;
	}

	if ( !stricmp( szKey, "StartU" ) )	
	{ 
		m_Material.m_vecTextureU.x = atof( szValue ); 
	}
	if ( !stricmp( szKey, "EndU" ) )	
	{ 
		m_Material.m_vecTextureU.y = atof( szValue ); 
	}
	if ( !stricmp( szKey, "StartV" ) )	
	{ 
		m_Material.m_vecTextureV.x = atof( szValue ); 
	}
	if ( !stricmp( szKey, "EndV" ) )	
	{ 
		m_Material.m_vecTextureV.y = atof( szValue ); 
	}

	if ( m_bLoaded )
	{
		// Clip - this needs to be done for everything other than a material change, so go ahead.
		DoClip();
		
		// Post updated.
		PostUpdate( Notify_Changed );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::OnUndoRedo( void )
{
	PostModified();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::CalcBounds( BOOL bFullUpdate )
{
	// Pass the info along.
	CMapSideList::CalcBounds( bFullUpdate );

	// Verify that we have valid data.
	if ( !Basis_IsValid() )
		return;

	// Calculate the 2d bounds.
	Vector vecMins, vecMaxs;
	vecMins = m_Origin - Vector( 2.0f, 2.0f, 2.0f );
	vecMaxs = m_Origin + Vector( 2.0f, 2.0f, 2.0f );

	// Reset bounds
	m_CullBox.ResetBounds();
	m_Render2DBox.ResetBounds();

	for ( int iHandle = 0; iHandle < 4; ++iHandle )
	{
		for ( int iAxis = 0; iAxis < 3; ++iAxis )
		{
			// Min
			if ( m_Handles.m_vec3D[iHandle][iAxis] < vecMins[iAxis] )
			{
				vecMins[iAxis] = m_Handles.m_vec3D[iHandle][iAxis];
			}

			// Max
			if ( m_Handles.m_vec3D[iHandle][iAxis] > vecMaxs[iAxis] )
			{
				vecMaxs[iAxis] = m_Handles.m_vec3D[iHandle][iAxis];
			}
		}
	}

	// Don't allow for NULL bounds.
	for ( int iAxis = 0; iAxis < 3; ++iAxis )
	{
		if( ( vecMaxs[iAxis] - vecMins[iAxis] ) == 0.0f )
		{
			vecMins[iAxis] -= 0.5f;
			vecMaxs[iAxis] += 0.5f;
		}
	}

	// Update the bounds.
	m_CullBox.UpdateBounds( vecMins, vecMaxs );
	m_BoundingBox = m_CullBox;
	m_Render2DBox.UpdateBounds( vecMins, vecMaxs );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapOverlay::PostModified( void )
{
	// update face and origin 
	if ( m_Faces.Count() > 0 )
	{
		Basis_SetFace( m_Faces.Element( 0 ) );
		Basis_UpdateOrigin();
	}
	else
	{
		m_Basis.m_pFace = NULL;
	}

	Handles_Build3D();
	DoClip();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pTransBox - 
//-----------------------------------------------------------------------------
void CMapOverlay::DoTransform( const VMatrix &matrix )
{
	BaseClass::DoTransform( matrix );

	VMatrix tmpMatrix = matrix;
		
	// erase move component
	tmpMatrix.SetTranslation( vec3_origin );

	// check if matrix would still change something 
	if ( !tmpMatrix.IsIdentity() )
	{
		// make sure axes are normalized (they should be anyways)
		m_Basis.m_vecAxes[OVERLAY_BASIS_U].NormalizeInPlace();
		m_Basis.m_vecAxes[OVERLAY_BASIS_V].NormalizeInPlace();

		Vector vecU = m_Basis.m_vecAxes[OVERLAY_BASIS_U];
		Vector vecV = m_Basis.m_vecAxes[OVERLAY_BASIS_V];
		Vector vecNormal = m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL];

		TransformPoint( tmpMatrix, vecU );
		TransformPoint( tmpMatrix, vecV );
		TransformPoint( tmpMatrix, vecNormal );

		float fScaleU = vecU.Length();
		float fScaleV = vecV.Length();
		float flScaleNormal = vecNormal.Length();

		bool bIsUnit = ( fequal( fScaleU, 1.0f, 0.0001 ) && fequal( fScaleV, 1.0f, 0.0001 ) && fequal( flScaleNormal, 1.0f, 0.0001 ) );
		bool bIsPerp = ( fequal( DotProduct( vecU, vecV ), 0.0f, 0.0025 ) && fequal( DotProduct( vecU, vecNormal ), 0.0f, 0.0025 ) && fequal( DotProduct( vecV, vecNormal ), 0.0f, 0.0025 ) );

//		if ( fequal(fScaleU,1,0.0001) && fequal(fScaleV,1,0.0001) && fequal(DotProduct( vecU, vecV ),0,0.0025) )
		if ( bIsUnit && bIsPerp )
		{
			// transformation doesnt scale or shear anything, so just update base axes
			m_Basis.m_vecAxes[OVERLAY_BASIS_U] = vecU;
			m_Basis.m_vecAxes[OVERLAY_BASIS_V] = vecV;
			m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL] = vecNormal;
		}
		else
		{
			// more complex transformation, move UV coordinates, but leave base axes 
			for ( int iHandle=0; iHandle<OVERLAY_HANDLES_COUNT;iHandle++)
			{
				Vector2D vecUV = m_Handles.m_vecBasisCoords[iHandle];
				Vector vecPos = ( vecUV.x * m_Basis.m_vecAxes[OVERLAY_BASIS_U] + vecUV.y * m_Basis.m_vecAxes[OVERLAY_BASIS_V] );
				
				// to transform in world space
				TransformPoint( tmpMatrix, vecPos );
				
				vecUV.x = m_Basis.m_vecAxes[OVERLAY_BASIS_U].Dot( vecPos );
				vecUV.y = m_Basis.m_vecAxes[OVERLAY_BASIS_V].Dot( vecPos );

				m_Handles.m_vecBasisCoords[iHandle] = vecUV;
			}

				if ( !Options.IsLockingTextures() )
			{
				// scale textures if locking is off
				m_Material.m_vecTextureU *= fScaleU;
				m_Material.m_vecTextureV *= fScaleV;
				Material_UpdateParentKey();
			}
		}
	}

	// Send modified notice.
	PostModified();

	Handles_UpdateParentKey();
}

//-----------------------------------------------------------------------------
// Purpose: Notifies us that a copy of ourselves was pasted.
//-----------------------------------------------------------------------------
void CMapOverlay::OnPaste( CMapClass *pCopy, CMapWorld *pSourceWorld, CMapWorld *pDestWorld, 
						   const CMapObjectList &OriginalList, CMapObjectList &NewList)
{
	//
	// NOTE: currently pCopy is the Overlay being pasted into the world, "this" is
	//       what is being copied from
	//
	CMapSideList::OnPaste( pCopy, pSourceWorld, pDestWorld, OriginalList, NewList );
	CMapOverlay *pOverlay = dynamic_cast<CMapOverlay*>( pCopy );
	if ( pOverlay )
	{
		pOverlay->Basis_BuildFromSideList();
		pOverlay->PostModified();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Notifies us that we created a copy of ourselves (a clone).
//-----------------------------------------------------------------------------
void CMapOverlay::OnClone( CMapClass *pClone, CMapWorld *pWorld, 
						   const CMapObjectList &OriginalList, CMapObjectList &NewList )
{
	CMapSideList::OnClone( pClone, pWorld, OriginalList, NewList );
	CMapOverlay *pOverlay = dynamic_cast<CMapOverlay*>( pClone );
	if ( pOverlay )
	{
		if ( ( GetOverlayType() && OVERLAY_TYPE_SHORE ) == 0 )
		{
			// Update the clone's solid dependencies (this doesn't happen on clone generally).
			int nFaceCount = pOverlay->GetFaceCount();
			for ( int iFace = 0; iFace < nFaceCount; ++iFace )
			{
				CMapFace *pFace = pOverlay->GetFace( iFace );
				CMapSolid *pSolid = ( CMapSolid* )pFace->GetParent();
				pOverlay->UpdateDependency( NULL, pSolid );
			}
		}

		pOverlay->PostModified();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Notifys this decal of a change to a solid that it is attached to.
//-----------------------------------------------------------------------------
void CMapOverlay::OnNotifyDependent( CMapClass *pObject, Notify_Dependent_t eNotifyType )
{
	// Chain to base class FIRST so it can rebuild the face list if necessary.
	CMapSideList::OnNotifyDependent( pObject, eNotifyType );

	//
	// NOTE: the solid moving (changing) can update the overlay/solid(face) dependency
	//       so "rebuild" the overlay
	//
	switch ( eNotifyType )
	{
	case Notify_Changed:
	case Notify_Undo:
	case Notify_Transform:
		{
			PostModified();
			break;
		}
	case Notify_Removed:
	case Notify_Clipped:
		{
			m_aRenderFaces.Purge();
			PostModified();
			break;
		}
	case Notify_Rebuild:
		{
			UpdateDispBarycentric();
			break;
		}
	case Notify_Rebuild_Full:
		{
			DoClip();
			CenterEntity();
			Handles_Build3D();
			break;
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::Render3D( CRender3D *pRender )
{
	int nFaceCount = m_aRenderFaces.Count();

	if ( nFaceCount != 0 )
	{
		// dont draw textured during manipulating
		if ( GetSelectionState() != SELECT_MODIFY )
		{

			// Bind the matrial -- if there is one!!
			bool bTextured = false;
			if ( m_Material.m_pTexture )
			{
				pRender->BindTexture( m_Material.m_pTexture );
				pRender->PushRenderMode( RENDER_MODE_TEXTURED );
				bTextured = true;
			}
			else
			{
				// Default state.
				pRender->PushRenderMode( RENDER_MODE_FLAT );
			}
			
			for ( int iFace = 0; iFace < nFaceCount; iFace++ )
			{
				ClipFace_t *pRenderFace = m_aRenderFaces.Element( iFace );
				if( !pRenderFace )
					continue;
				
				MaterialPrimitiveType_t type = MATERIAL_POLYGON;
				
				// Get a dynamic mesh.
				CMeshBuilder meshBuilder;
				CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
				IMesh* pMesh = pRenderContext->GetDynamicMesh();
				
				meshBuilder.Begin( pMesh, type, pRenderFace->m_nPointCount );
				for ( int iPoint = 0; iPoint < pRenderFace->m_nPointCount; iPoint++ )
				{
					if ( !bTextured )
					{
						meshBuilder.Color3ub( 0, 128, 0 );
					}
					else
					{
						meshBuilder.TexCoord2f( 0, pRenderFace->m_aTexCoords[0][iPoint].x, pRenderFace->m_aTexCoords[0][iPoint].y );
						meshBuilder.TexCoord2f( 2, pRenderFace->m_aTexCoords[1][iPoint].x, pRenderFace->m_aTexCoords[1][iPoint].y );
						meshBuilder.Color4ub( 255, 255, 255, 255 );
					}
					meshBuilder.Position3f( pRenderFace->m_aPoints[iPoint].x, pRenderFace->m_aPoints[iPoint].y, pRenderFace->m_aPoints[iPoint].z );
					meshBuilder.AdvanceVertex();
				}
				meshBuilder.End();
				
				pMesh->Draw();
			}

			pRender->PopRenderMode();
		}
		
		// Render wireframe on top when seleted.
		if ( GetSelectionState() != SELECT_NONE )
		{
			pRender->PushRenderMode( RENDER_MODE_WIREFRAME );
			for ( int iFace = 0; iFace < nFaceCount; iFace++ )
			{
				ClipFace_t *pRenderFace = m_aRenderFaces.Element( iFace );
				if( !pRenderFace )
					continue;
				
				MaterialPrimitiveType_t type = MATERIAL_LINE_LOOP;
				
				// get a dynamic mesh
				CMeshBuilder meshBuilder;
				CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
				IMesh* pMesh = pRenderContext->GetDynamicMesh();
				
				meshBuilder.Begin( pMesh, type, pRenderFace->m_nPointCount );
				for( int iPoint = 0; iPoint < pRenderFace->m_nPointCount; iPoint++ )
				{
					meshBuilder.Color3ub( 0, 255, 0 );
					meshBuilder.Position3f( pRenderFace->m_aPoints[iPoint].x, pRenderFace->m_aPoints[iPoint].y, pRenderFace->m_aPoints[iPoint].z );
					meshBuilder.AdvanceVertex();
				}
				meshBuilder.End();
				
				pMesh->Draw();
			}
			pRender->PopRenderMode();
		}
	}

	// Render the handles - if selected or in overlay tool mode.
	if ( ( ToolManager()->GetActiveToolID() == TOOL_OVERLAY ) && Basis_IsValid() && IsSelected() )
	{
		Handles_Render3D( pRender );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clip the overlay "face" to all of the faces in the overlay sidelist.
//          The sidelist defines all faces affected by the "overlay."
//-----------------------------------------------------------------------------
void CMapOverlay::DoClip( void )
{
	// Check to see if we have any faces to clip against.
	int nFaceCount = m_Faces.Count();
	if( nFaceCount == 0 )
		return;

	// Destroy the render face cache.
	m_aRenderFaces.Purge();

	// clip the overlay against all faces in the sidelist
	for ( int iFace = 0; iFace < nFaceCount; iFace++ )
	{
		CMapFace *pFace = m_Faces.Element( iFace );
		if ( pFace )
		{
			PreClip();
			DoClipFace( pFace );
			PostClip();
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::PreClip( void )
{
	//
	// Create the initial face to be clipped - the overlay.
	//
	m_pOverlayFace = ClipFace_Create( OVERLAY_HANDLES_COUNT );
	if ( m_pOverlayFace )
	{
		for ( int iPoint = 0; iPoint < OVERLAY_HANDLES_COUNT; iPoint++ )
		{
			OverlayUVToOverlayPlane( m_Handles.m_vecBasisCoords[iPoint], m_pOverlayFace->m_aPoints[iPoint] );

			// translate texture UV to texture coords:
			Vector2D vTexCoord;
			switch( iPoint )
			{
				case 0 : vTexCoord = Vector2D(m_Material.m_vecTextureU.x, m_Material.m_vecTextureV.x); break;
				case 1 : vTexCoord = Vector2D(m_Material.m_vecTextureU.x, m_Material.m_vecTextureV.y); break;
				case 2 : vTexCoord = Vector2D(m_Material.m_vecTextureU.y, m_Material.m_vecTextureV.y); break;
				case 3 : vTexCoord = Vector2D(m_Material.m_vecTextureU.y, m_Material.m_vecTextureV.x); break;
				default : Assert( iPoint <= OVERLAY_HANDLES_COUNT);
			}

			m_pOverlayFace->m_aTexCoords[0][iPoint] = vTexCoord;

			if ( m_Basis.m_pFace->HasDisp() )
			{
				EditDispHandle_t handle = m_Basis.m_pFace->GetDisp();
				CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );
				if ( pDisp )
				{
					Vector2D vecTmp;
					pDisp->BaseFacePlaneToDispUV( m_pOverlayFace->m_aPoints[iPoint], vecTmp );
					m_pOverlayFace->m_aDispPointUVs[iPoint].x = vecTmp.x;
					m_pOverlayFace->m_aDispPointUVs[iPoint].y = vecTmp.y;
					m_pOverlayFace->m_aDispPointUVs[iPoint].z = 0.0f;
				}
			}
		}
		// The second set of texcoords on the overlay is used for alpha by certain shaders,
		// and they want to stretch the texture across the whole overlay.
		m_pOverlayFace->m_aTexCoords[1][0].Init( 0, 0 );
		m_pOverlayFace->m_aTexCoords[1][1].Init( 0, 1 );
		m_pOverlayFace->m_aTexCoords[1][2].Init( 1, 1 );
		m_pOverlayFace->m_aTexCoords[1][3].Init( 1, 0 );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::PostClip( void )
{
	ClipFace_Destroy( &m_pOverlayFace );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::DoClipFace( CMapFace *pFace )
{
	// Valid face?
	Assert( pFace != NULL );
	if( !pFace )
		return;

	// Copy the original overlay to the "clipped" overlay.
	ClipFace_t *pClippedFace = ClipFace_Copy( m_pOverlayFace );
	if ( !pClippedFace )
		return;

	//
	// Project all face points into the overlay plane.
	//
	int nPointCount = pFace->nPoints;
	Vector *pPoints = new Vector[nPointCount];
	int	nEdgePlaneCount = nPointCount;
	cplane_t *pEdgePlanes = new cplane_t[nEdgePlaneCount];
	if ( !pPoints || !pEdgePlanes )
	{
		delete [] pPoints;
		delete [] pEdgePlanes;
		return;
	}

	for ( int iPoint = 0; iPoint < nPointCount; iPoint++ )
	{
		WorldToOverlayPlane( pFace->Points[iPoint], pPoints[iPoint] );
	}

	// Create the face clipping planes (edges cross overlay plane normal).
	BuildEdgePlanes( pPoints, nPointCount, pEdgePlanes, nEdgePlaneCount );

	//
	// Clip overlay against all the edge planes.
	//
	for ( int iClipPlane = 0; iClipPlane < nEdgePlaneCount; iClipPlane++ )
	{
		ClipFace_t *pFront = NULL;
		ClipFace_t *pBack = NULL;

		if ( pClippedFace )
		{
			// Clip the overlay and delete the data (we are done with it - we are only interested in what is left).
			ClipFace_Clip( pClippedFace, &pEdgePlanes[iClipPlane], OVERLAY_WORLDSPACE_EPSILON, &pFront, &pBack );
			ClipFace_Destroy( &pClippedFace );

			// Keep the backside -- if it exists and continue clipping.
			if ( pBack )
			{
				pClippedFace = pBack;
			}

			// Destroy the front side -- if it exists.
			if ( pFront )
			{
				ClipFace_Destroy( &pFront );
			}
		}
	}

	//
	// Free temporary memory (clip planes and point).
	//
	delete [] pPoints;
	delete [] pEdgePlanes;


	//
	// If it exists, move points from the overlay plane back into
	// the base face plane.
	//
	if ( !pClippedFace )
		return;

	for ( int iPoint = 0; iPoint < pClippedFace->m_nPointCount; iPoint++ )
	{
		Vector2D vecUV;
		PointInQuadToBarycentric( m_pOverlayFace->m_aPoints[0], m_pOverlayFace->m_aPoints[3], 
			                      m_pOverlayFace->m_aPoints[2], m_pOverlayFace->m_aPoints[1],
								  pClippedFace->m_aPoints[iPoint], vecUV );

		Vector vecTmp;
		OverlayPlaneToWorld( pFace, pClippedFace->m_aPoints[iPoint], vecTmp );
		pClippedFace->m_aPoints[iPoint] = vecTmp;

		Vector2D vecTexCoord;
		for ( int iTexCoord=0; iTexCoord < NUM_CLIPFACE_TEXCOORDS; iTexCoord++ )
		{
			TexCoordInQuadFromBarycentric( m_pOverlayFace->m_aTexCoords[iTexCoord][0], m_pOverlayFace->m_aTexCoords[iTexCoord][3],
										m_pOverlayFace->m_aTexCoords[iTexCoord][2], m_pOverlayFace->m_aTexCoords[iTexCoord][1],
										vecUV, vecTexCoord );
			
			pClippedFace->m_aTexCoords[iTexCoord][iPoint] = vecTexCoord;
		}
	}

	//
	// If the face has a displacement map -- continue clipping.
	//
	if( pFace->HasDisp() )
	{
		DoClipDisp( pFace, pClippedFace );
	}
	// Done - save it!
	else
	{
		pClippedFace->m_pBuildFace = pFace;
		m_aRenderFaces.AddToTail( pClippedFace );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CMapOverlay::BuildEdgePlanes( Vector const *pPoints, int nPointCount,
								   cplane_t *pEdgePlanes, int nEdgePlaneCount )
{
	for ( int iPoint = 0; iPoint < nPointCount; iPoint++ )
	{
		Vector vecEdge;
		vecEdge = pPoints[(iPoint+1)%nPointCount] - pPoints[iPoint];
		VectorNormalize( vecEdge );

		pEdgePlanes[iPoint].normal = m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL].Cross( vecEdge );
		pEdgePlanes[iPoint].dist = pEdgePlanes[iPoint].normal.Dot( pPoints[iPoint] );

		// Check normal facing.
		float flDist = pEdgePlanes[iPoint].normal.Dot( pPoints[(iPoint+2)%nPointCount] ) - pEdgePlanes[iPoint].dist;
		if( flDist > 0.0f )
		{
			// flip
			pEdgePlanes[iPoint].normal.Negate();
			pEdgePlanes[iPoint].dist = -pEdgePlanes[iPoint].dist;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::Disp_ClipFragments( CMapDisp *pDisp, ClipFaces_t &aDispFragments )
{
	cplane_t clipPlane;

	// Cache the displacement interval.
	int nInterval = pDisp->GetWidth() - 1;

	// Displacement-space clipping in V.
	clipPlane.normal.Init( 1.0f, 0.0f, 0.0f );
	Disp_DoClip( pDisp, aDispFragments, clipPlane, 1.0f, nInterval, 1, nInterval, 1 );

	// Displacement-space clipping in U.
	clipPlane.normal.Init( 0.0f, 1.0f, 0.0f );
	Disp_DoClip( pDisp, aDispFragments, clipPlane, 1.0f, nInterval, 1, nInterval, 1 );

	// Displacement-space clipping UV from top-left to bottom-right.
	clipPlane.normal.Init( 0.707f, 0.707f, 0.0f );  // 45 degrees
	Disp_DoClip( pDisp, aDispFragments, clipPlane, 0.707f, nInterval, 2, ( nInterval * 2 - 1 ), 2 );

	// Displacement-space clipping UV from bottom-left to top-right.
	clipPlane.normal.Init( -0.707f, 0.707f, 0.0f );  // 135 degrees
	Disp_DoClip( pDisp, aDispFragments, clipPlane, 0.707f, nInterval, -( nInterval - 2 ), ( nInterval - 1 ), 2 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::Disp_DoClip( CMapDisp *pDisp, ClipFaces_t &aDispFragments,
							   cplane_t &clipPlane, float clipDistStart, int nInterval,
							   int nLoopStart, int nLoopEnd, int nLoopInc )
{
	// Setup interval information.
	float flInterval = static_cast<float>( nInterval );
	float flOOInterval = 1.0f / flInterval;

	// Holds the current set of clipped faces.
	ClipFaces_t aClippedFragments;

	for ( int iInterval = nLoopStart; iInterval < nLoopEnd; iInterval += nLoopInc )
	{
		// Copy the current list to clipped face list.
		aClippedFragments.CopyArray( aDispFragments.Base(), aDispFragments.Count() );
		aDispFragments.Purge();

		// Clip in V.
		int nFragCount = aClippedFragments.Count();
		for ( int iFrag = 0; iFrag < nFragCount; iFrag++ )
		{
			ClipFace_t *pClipFrag = aClippedFragments[iFrag];
			if ( pClipFrag )
			{
				ClipFace_t *pFront = NULL, *pBack = NULL;

				clipPlane.dist = clipDistStart * ( ( float )iInterval * flOOInterval );
				ClipFace_ClipBarycentric( pClipFrag, &clipPlane, OVERLAY_DISPSPACE_EPSILON, iInterval, pDisp, &pFront, &pBack );
				ClipFace_Destroy( &pClipFrag );

				if ( pFront )
				{
					aDispFragments.AddToTail( pFront );
				}

				if ( pBack )
				{
					aDispFragments.AddToTail( pBack );
				}
			}
		}
	}

	// Clean up!
	aClippedFragments.Purge();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::DoClipDisp( CMapFace *pFace, ClipFace_t *pClippedFace )
{
	// Get the displacement data.
	EditDispHandle_t handle = pFace->GetDisp();
	CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );

	// Initialize local clip data.
	ClipFace_PreClipDisp( pClippedFace, pDisp );

	// Setup clipped face lists.
	ClipFaces_t aCurrentFaces;
	aCurrentFaces.AddToTail( pClippedFace );

	Disp_ClipFragments( pDisp, aCurrentFaces );

	//
	// Project points back onto the displacement surface.
	//
	int nFaceCount = aCurrentFaces.Count();
	for( int iFace = 0; iFace < nFaceCount; iFace++ )
	{	
		ClipFace_t *pClipFace = aCurrentFaces[iFace];
		if ( pClipFace )
		{
			// Save for re-building later!
			pClipFace->m_pBuildFace = pFace;
			m_aRenderFaces.AddToTail( aCurrentFaces[iFace] );
			ClipFace_BuildFacesFromBlendedData( pClipFace );
		}
	}

	// Clean up!
	aCurrentFaces.Purge();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::HandlesReset( void )
{
	m_Handles.m_iHit = -1;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CMapOverlay::HandlesHitTest( CMapView *pView, const Vector2D &vPoint )
{
	int handleRadius = 8;

	for ( int iPoint = 0; iPoint < 4; iPoint++ )
	{
		Vector2D vHandle; 

		pView->WorldToClient( vHandle, m_Handles.m_vec3D[iPoint] );
		
		if ( vPoint.x < (vHandle.x-handleRadius) || vPoint.x > ( vHandle.x+handleRadius) )
			continue;

		if ( vPoint.y < (vHandle.y-handleRadius) || vPoint.y > ( vHandle.y+handleRadius) )
			continue;

		m_Handles.m_iHit = iPoint;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::HandlesDragTo( Vector &vecImpact, CMapFace *pFace )
{
	// Check handle index range.
	if ( ( m_Handles.m_iHit < 0 ) || ( m_Handles.m_iHit > 3 ) )
		return;

	// Save
	m_Handles.m_vec3D[m_Handles.m_iHit] = vecImpact;

	// Project the point into the overlay plane (from face/disp).
	Vector vecOverlay;
	Vector2D vecUVOverlay;
	Handles_SurfToOverlayPlane( pFace, vecImpact, vecOverlay );
	OverlayPlaneToOverlayUV( vecOverlay, vecUVOverlay );
	m_Handles.m_vecBasisCoords[m_Handles.m_iHit] = vecUVOverlay;
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::HandleMoveTo( int iHandle, Vector &vecPoint, CMapFace *pFace )
{
	if ( ( iHandle < 0 ) || ( iHandle > 3 ) )
		return;

	m_Handles.m_vec3D[iHandle] = vecPoint;

	// Project the point into the overlay plane (from face/disp).
	Vector vecOverlay;
	Vector2D vecUVOverlay;
	Handles_SurfToOverlayPlane( pFace, vecPoint, vecOverlay );
	OverlayPlaneToOverlayUV( vecOverlay, vecUVOverlay );
	m_Handles.m_vecBasisCoords[iHandle] = vecUVOverlay;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::SetTexCoords( Vector2D vecTexCoords[4] )
{
	m_Material.m_vecTextureU.x = vecTexCoords[0][0];
	m_Material.m_vecTextureV.x = vecTexCoords[0][1];
//	m_Material.m_vecTextureU.x = vecTexCoord[1][0];
	m_Material.m_vecTextureV.y = vecTexCoords[1][1];
	m_Material.m_vecTextureU.y = vecTexCoords[2][0];
//	m_Material.m_vecTextureV.y = vecTexCoord[2][1];
//	m_Material.m_vecTextureU.y = vecTexCoord[3][0];
//	m_Material.m_vecTextureV.x = vecTexCoord[3][1];
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::UpdateDispBarycentric( void )
{
	//
	// Project points back onto the displacement surface.
	//
	int nFaceCount = m_aRenderFaces.Count();
	for ( int iFace = 0; iFace < nFaceCount; iFace++ )
	{
		// Get the current face and remove it from the list.
		ClipFace_t *pClipFace = m_aRenderFaces[iFace];
		if ( pClipFace )
		{
			if ( pClipFace->m_pBuildFace->HasDisp() )
			{
				ClipFace_BuildFacesFromBlendedData( pClipFace );
			}
		}
	}

	// Update the entity position.
	CenterEntity();

	// Update the handles.
	Handles_Build3D();
}

//-----------------------------------------------------------------------------
// Purpose:  
//-----------------------------------------------------------------------------
void CMapOverlay::CenterEntity( void )
{
	// Center in overlay plane.
	Vector vecTotal;
	Vector vecHandle;

	vecTotal.Init();
	for( int iHandle = 0; iHandle < OVERLAY_HANDLES_COUNT; ++iHandle )
	{
		OverlayUVToOverlayPlane( m_Handles.m_vecBasisCoords[iHandle], vecHandle );
		vecTotal += vecHandle;
	}
	vecTotal *= 0.25f;

	// Center in overlay uv-space.
	Vector2D vecNewCenter;
	OverlayPlaneToOverlayUV( vecTotal, vecNewCenter );
	for( int iHandle = 0; iHandle < OVERLAY_HANDLES_COUNT; ++iHandle )
	{
		m_Handles.m_vecBasisCoords[iHandle] -= vecNewCenter;
	}

	// Update the entity's origin.
	m_Basis.m_vecOrigin = vecTotal;

	CMapEntity *pEntity = ( CMapEntity* )GetParent();
	if ( pEntity )
	{
		Vector vecSurfPoint;
		OverlayPlaneToSurfFromList( vecTotal, vecSurfPoint );
		pEntity->SetOrigin( vecSurfPoint );
	}

	// Update the property box.
	Basis_UpdateParentKey();
	Handles_UpdateParentKey();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::GetPlane( cplane_t &plane )
{
	plane.normal = m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL];
	plane.dist = plane.normal.Dot( m_Basis.m_vecOrigin );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::GetHandlePos( int iHandle, Vector &vecPos )
{
	Assert( iHandle >= 0 );
	Assert( iHandle < 4 );

	vecPos = m_Handles.m_vec3D[iHandle];
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::SideList_Init( CMapFace *pFace )
{
	// Valid face?
	if ( !pFace )
		return;

	// Purge side list as this should be the initial face!
	m_Faces.Purge();
	m_Faces.AddToTail( pFace );

	if ( ( GetOverlayType() && OVERLAY_TYPE_SHORE ) == 0 )
	{
		// Update dependencies.
		UpdateDependency( NULL, ( CMapSolid* )pFace->GetParent() );
		UpdateParentKey();
	}

	// Initialize the overlay.
	Basis_Init( pFace );
	PostModified();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::SideList_AddFace( CMapFace *pFace )
{
	// Valid face?
	if ( !pFace )
		return;

	// Purge side list as this should be the initial face!
	m_Faces.AddToTail( pFace );

	if ( ( GetOverlayType() && OVERLAY_TYPE_SHORE ) == 0 )
	{
		// Update dependencies.
		UpdateDependency( NULL, ( CMapSolid* )pFace->GetParent() );
		UpdateParentKey();
	}

	PostModified();
}

//=============================================================================
//
// Overlay Utility Functions
//

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::OverlayUVToOverlayPlane( const Vector2D &vecUV, Vector &vecOverlayPoint )
{
	vecOverlayPoint = ( vecUV.x * m_Basis.m_vecAxes[OVERLAY_BASIS_U] +
		                vecUV.y * m_Basis.m_vecAxes[OVERLAY_BASIS_V] );
	vecOverlayPoint += m_Basis.m_vecOrigin;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::OverlayPlaneToOverlayUV( const Vector &vecOverlayPoint, Vector2D &vecUV )
{
	Vector vecDelta;
	vecDelta = vecOverlayPoint - m_Basis.m_vecOrigin;
	vecUV.x = m_Basis.m_vecAxes[OVERLAY_BASIS_U].Dot( vecDelta );
	vecUV.y = m_Basis.m_vecAxes[OVERLAY_BASIS_V].Dot( vecDelta );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::WorldToOverlayPlane( const Vector &vecWorldPoint, Vector &vecOverlayPoint )
{
	Vector vecDelta = vecWorldPoint - m_Basis.m_vecOrigin;
	float flDist = m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL].Dot( vecDelta );
	vecOverlayPoint = vecWorldPoint - ( m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL] * flDist );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::OverlayPlaneToWorld( CMapFace *pFace, const Vector &vecOverlayPoint,
									   Vector &vecWorldPoint )
{
	// Create the overlay plane - the base face plane.
	cplane_t surfacePlane;
	pFace->GetFaceNormal( surfacePlane.normal );
	VectorNormalize( surfacePlane.normal );
	Vector vecPoint;
	pFace->GetPoint( vecPoint, 0 );
	surfacePlane.dist = surfacePlane.normal.Dot( vecPoint );

	float flDistToSurface = surfacePlane.normal.Dot( vecOverlayPoint ) - surfacePlane.dist;
	float flDist = flDistToSurface;
	float flDot = surfacePlane.normal.Dot( m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL] );
	if ( flDot != 0.0f )
	{
		flDist = ( 1.0f / flDot ) * flDistToSurface;
	}

	vecWorldPoint = vecOverlayPoint - ( m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL] * flDist );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CMapOverlay::OverlayPlaneToSurfFromList( const Vector &vecOverlayPoint, Vector &vecSurfPoint )
{
	// Initialize the point with the overlay point.
	vecSurfPoint = vecOverlayPoint;

	int nFaceCount = GetFaceCount();
	CUtlVector<Vector> aPoints;
	CUtlVector<cplane_t> aPlanes;

	for ( int iFace = 0; iFace < nFaceCount; ++iFace )
	{
		CMapFace *pFace = GetFace( iFace );
		if ( !pFace )
			continue;

		// Set points.
		aPoints.Purge();
		aPoints.SetSize( pFace->nPoints );
		aPlanes.Purge();
		aPlanes.SetSize( pFace->nPoints );

		// Project all the face points into the overlay plane.
		for ( int iPoint = 0; iPoint < pFace->nPoints; ++iPoint )
		{
			WorldToOverlayPlane( pFace->Points[iPoint], aPoints[iPoint] );
		}

		// Create edge planes for clipping.
		BuildEdgePlanes( aPoints.Base(), aPoints.Count(), aPlanes.Base(), aPlanes.Count() );

		// Check to see if a point lies behind all of the edge planes - this is our face.
		int iPlane;
		for ( iPlane = 0; iPlane < aPlanes.Count(); ++iPlane )
		{
			float flDist = aPlanes[iPlane].normal.Dot( vecOverlayPoint ) - aPlanes[iPlane].dist;
			if( flDist >= 0.0f )
				break;
		}

		// Point lies outside off at least one plane.
		if( iPlane != aPlanes.Count() )
		{
			continue;
		}

		// Project the point up to the base face plane (displacement if necessary).
		OverlayPlaneToWorld( pFace, vecOverlayPoint, vecSurfPoint );
		
		if( pFace->HasDisp() )
		{
			Vector2D vecTmp;
			EditDispHandle_t handle = pFace->GetDisp();
			CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );
			pDisp->BaseFacePlaneToDispUV( vecSurfPoint, vecTmp );
			pDisp->DispUVToSurf( vecTmp, vecSurfPoint, NULL, NULL );
		}

		// Clean-up.
		aPoints.Purge();
		aPlanes.Purge();
		return;
	}

	// Clean-up.
	aPoints.Purge();
	aPlanes.Purge();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMapOverlay::EntityOnSurfFromListToBaseFacePlane( const Vector &vecWorldPoint, Vector &vecBasePoint )
{
	int nFaceCount = GetFaceCount();
	for ( int iFace = 0; iFace < nFaceCount; ++iFace )
	{
		CMapFace *pFace = GetFace( iFace );
		if ( !pFace )
			continue;
		
		if ( !pFace->HasDisp() )
			continue;

		EditDispHandle_t handle = pFace->GetDisp();
		CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );

		if ( pDisp->SurfToBaseFacePlane( vecWorldPoint, vecBasePoint ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::GetTriVerts( CMapDisp *pDisp, const Vector2D &vecSurfUV, int *pTris, Vector2D *pVertsUV )
{
	// Get the displacement width.
	int nWidth = pDisp->GetWidth();
	int nHeight = pDisp->GetHeight();

	// scale the u, v coordinates the displacement grid size
	float flU = vecSurfUV.x * ( nWidth - 1.000001f );
	float flV = vecSurfUV.y * ( nHeight - 1.000001f );

	// find the triangle the "uv spot" resides in
	int nSnapU = static_cast<int>( flU );
	int nSnapV = static_cast<int>( flV );
	if ( nSnapU == ( nWidth - 1 ) ) { --nSnapU; }
	if ( nSnapV == ( nHeight - 1 ) ) { --nSnapV; }
	int nNextU = nSnapU + 1;
	int nNextV = nSnapV + 1;
	
	// Fractional portion
	float flFracU = flU - static_cast<float>( nSnapU );
	float flFracV = flV - static_cast<float>( nSnapV );

	bool bOdd = ( ( ( nSnapV * nWidth ) + nSnapU ) % 2 ) == 1;
	if ( bOdd )
	{
		if( ( flFracU + flFracV ) >= ( 1.0f + OVERLAY_DISPSPACE_EPSILON ) )
		{
			pVertsUV[0].x = nSnapU;  pVertsUV[0].y = nNextV;
			pVertsUV[1].x = nNextU;  pVertsUV[1].y = nNextV;
			pVertsUV[2].x = nNextU;  pVertsUV[2].y = nSnapV;
		}
		else
		{
			pVertsUV[0].x = nSnapU;  pVertsUV[0].y = nSnapV;
			pVertsUV[1].x = nSnapU;  pVertsUV[1].y = nNextV;
			pVertsUV[2].x = nNextU;  pVertsUV[2].y = nSnapV;
		}
	}
	else
	{
		if ( flFracU < flFracV )
		{
			pVertsUV[0].x = nSnapU;  pVertsUV[0].y = nSnapV;
			pVertsUV[1].x = nSnapU;  pVertsUV[1].y = nNextV;
			pVertsUV[2].x = nNextU;  pVertsUV[2].y = nNextV;
		}
		else
		{
			pVertsUV[0].x = nSnapU;  pVertsUV[0].y = nSnapV;
			pVertsUV[1].x = nNextU;  pVertsUV[1].y = nNextV;
			pVertsUV[2].x = nNextU;  pVertsUV[2].y = nSnapV;
		}
	}

	// Calculate the triangle indices.
	for( int iVert = 0; iVert < 3; ++iVert )
	{
		pTris[iVert] = pVertsUV[iVert].y * nWidth + pVertsUV[iVert].x;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapOverlay::SetMaterial( const char *szMaterialName )
{
	// Get the new material.
	IEditorTexture *pTex = g_Textures.FindActiveTexture( szMaterialName );
	if ( !pTex )
		return;
	
	// Save the new material.
	m_Material.m_pTexture = pTex;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
ChunkFileResult_t CMapOverlay::SaveDataToVMF( CChunkFile *pFile, CSaveInfo *pSaveInfo )
{
	ChunkFileResult_t eResult = pFile->BeginChunk("overlaydata");

	// Save the material name.
	if ( eResult == ChunkFile_Ok )
	{
		eResult = pFile->WriteKeyValue( "material", m_Material.m_pTexture->GetName() );
	}

	// Save the u,v data.
	if ( eResult == ChunkFile_Ok )
	{
		eResult = pFile->WriteKeyValueFloat( "StartU", m_Material.m_vecTextureU.x );
	}
	if ( eResult == ChunkFile_Ok )
	{
		eResult = pFile->WriteKeyValueFloat( "EndU", m_Material.m_vecTextureU.y );
	}

	if ( eResult == ChunkFile_Ok )
	{
		eResult = pFile->WriteKeyValueFloat( "StartV", m_Material.m_vecTextureV.x );
	}
	if ( eResult == ChunkFile_Ok )
	{
		eResult = pFile->WriteKeyValueFloat( "EndV", m_Material.m_vecTextureV.y );
	}

	// Basis data.
	if ( eResult == ChunkFile_Ok )
	{
		eResult = pFile->WriteKeyValueVector3( "BasisOrigin", m_Basis.m_vecOrigin );
	}
	if ( eResult == ChunkFile_Ok )
	{
		eResult = pFile->WriteKeyValueVector3( "BasisU", m_Basis.m_vecAxes[OVERLAY_BASIS_U] );
	}
	if ( eResult == ChunkFile_Ok )
	{
		eResult = pFile->WriteKeyValueVector3( "BasisV", m_Basis.m_vecAxes[OVERLAY_BASIS_V] );
	}
	if ( eResult == ChunkFile_Ok )
	{
		eResult = pFile->WriteKeyValueVector3( "BasisNormal", m_Basis.m_vecAxes[OVERLAY_BASIS_NORMAL] );
	}

	if ( eResult == ChunkFile_Ok )
	{
		Vector vecTmp( m_Handles.m_vecBasisCoords[0].x, m_Handles.m_vecBasisCoords[0].y, ( float )m_Basis.m_nAxesFlip[0] );
		eResult = pFile->WriteKeyValueVector3( "uv0", vecTmp );
	}
	if ( eResult == ChunkFile_Ok )
	{
		Vector vecTmp( m_Handles.m_vecBasisCoords[1].x, m_Handles.m_vecBasisCoords[1].y, ( float )m_Basis.m_nAxesFlip[1] );
		eResult = pFile->WriteKeyValueVector3( "uv1", vecTmp );
	}
	if ( eResult == ChunkFile_Ok ) 
	{
		Vector vecTmp( m_Handles.m_vecBasisCoords[2].x, m_Handles.m_vecBasisCoords[2].y, ( float )m_Basis.m_nAxesFlip[2] );
		eResult = pFile->WriteKeyValueVector3( "uv2", vecTmp );
	}
	if ( eResult == ChunkFile_Ok )
	{
		Vector vecTmp( m_Handles.m_vecBasisCoords[3].x, m_Handles.m_vecBasisCoords[3].y, 0.0f );
		eResult = pFile->WriteKeyValueVector3( "uv3", vecTmp );
	}
 
	// Sidelist.
	if ( eResult == ChunkFile_Ok )
	{
		char szSetValue[KEYVALUE_MAX_VALUE_LENGTH];
		CMapWorld::FaceID_FaceListsToString( szSetValue, sizeof( szSetValue ), &m_Faces, NULL );
		eResult = pFile->WriteKeyValue( "sides", szSetValue );
	}

	if ( eResult == ChunkFile_Ok )
	{
		eResult = pFile->EndChunk();
	}

	return eResult;
}
