//========= Copyright Valve Corporation, All rights reserved. ============//
//
//
//=============================================================================

#include <stdafx.h>
#include "FaceEditSheet.h"
#include "MainFrm.h"
#include "GlobalFunctions.h"
#include "MapDisp.h"
#include "DispShore.h"
#include "utlvector.h"
#include "mapdoc.h"
#include "mapworld.h"
#include "mapsolid.h"
#include "materialsystem/imesh.h"
#include "Material.h"
#include "collisionutils.h"
#include "TextureSystem.h"
#include "mapoverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

IMPLEMENT_MAPCLASS( CMapOverlayTransition )

#define DISPSHORE_WIDTH_WORLD		25.0f
#define DISPSHORE_WIDTH_WATER		25.0f
#define DISPSHORE_VECTOR_EPS		0.1f
#define	DISPSHORE_SURF_LENGTH		120.0f

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Shoreline_t::Shoreline_t()
{
	m_nShorelineId = -1;
	m_aSegments.Purge();
	m_aOverlays.Purge();
	m_flLength = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Shoreline_t::~Shoreline_t()
{
	m_aSegments.Purge();
	m_aOverlays.Purge();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Shoreline_t::AddSegment( Vector &vecPoint0, Vector &vecPoint1, 
						      Vector &vecNormal, float flWaterZ, 
							  CMapFace *pWaterFace, EditDispHandle_t hDisp )
{
	// Check for duplicates!
	int nSegmentCount = m_aSegments.Count();
	int iSegment;
	for ( iSegment = 0; iSegment < nSegmentCount; ++iSegment )
	{
		if ( VectorsAreEqual( m_aSegments[iSegment].m_vecPoints[0], vecPoint0, DISPSHORE_VECTOR_EPS ) ) 
		{ 
			if ( VectorsAreEqual( m_aSegments[iSegment].m_vecPoints[1], vecPoint1, DISPSHORE_VECTOR_EPS ) ) 
				return;
		}

		if ( VectorsAreEqual( m_aSegments[iSegment].m_vecPoints[1], vecPoint0, DISPSHORE_VECTOR_EPS ) ) 
		{
			if ( VectorsAreEqual( m_aSegments[iSegment].m_vecPoints[0], vecPoint1, DISPSHORE_VECTOR_EPS ) )
				return;
		}
	}

	iSegment = m_aSegments.AddToTail();

	Vector vecEdge, vecCross;
	VectorSubtract( vecPoint1, vecPoint0, vecEdge );
	CrossProduct( vecNormal, vecEdge, vecCross );
	if ( vecCross.z >= 0.0f )
	{
		VectorCopy( vecPoint1, m_aSegments[iSegment].m_vecPoints[0] );
		VectorCopy( vecPoint0, m_aSegments[iSegment].m_vecPoints[1] );
	}
	else
	{
		VectorCopy( vecPoint0, m_aSegments[iSegment].m_vecPoints[0] );
		VectorCopy( vecPoint1, m_aSegments[iSegment].m_vecPoints[1] );
	}

	VectorCopy( vecNormal, m_aSegments[iSegment].m_vecNormals[0] );
	VectorCopy( vecNormal, m_aSegments[iSegment].m_vecNormals[1] );

	m_aSegments[iSegment].m_hDisp = hDisp;
	m_aSegments[iSegment].m_flWaterZ = flWaterZ;
	m_aSegments[iSegment].m_iStartPoint = 0;
	m_aSegments[iSegment].m_bTouch = false;
	m_aSegments[iSegment].m_bCreated = false;
	m_aSegments[iSegment].m_vecCenter.Init();

	m_aSegments[iSegment].m_WorldFace.m_bAdjWinding = false;	
	m_aSegments[iSegment].m_WaterFace.m_bAdjWinding = false;
	for ( int i = 0; i < 4; ++i )
	{
		m_aSegments[iSegment].m_WorldFace.m_vecPoints[i].Init();
		m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[i].Init();
		m_aSegments[iSegment].m_WorldFace.m_pFaces[i] = NULL;

		m_aSegments[iSegment].m_WaterFace.m_vecPoints[i].Init();
		m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[i].Init();
		m_aSegments[iSegment].m_WaterFace.m_pFaces[i] = NULL;
	}
}

//=============================================================================
//
// CDispShoreManager
//
class CDispShoreManager : public IDispShoreManager
{
public:

	CDispShoreManager();
	~CDispShoreManager();

	// Interface.
	bool		Init( void );
	void		Shutdown( void );

	int			GetShorelineCount( void );
	Shoreline_t *GetShoreline( int nShorelineId );
	void		AddShoreline( int nShorelineId );
	void		RemoveShoreline( int nShorelineId );
	void		BuildShoreline( int nShorelineId, CUtlVector<CMapFace*> &aFaces, CUtlVector<CMapFace*> &aWaterFaces );
       
	void		Draw( CRender3D *pRender );
	void		DebugDraw( CRender3D *pRender );

private:

	void BuildShorelineSegments( Shoreline_t *pShoreline, CUtlVector<CMapFace*> &aFaces, CUtlVector<CMapFace*> &aWaterFaces );
	void AverageShorelineNormals( Shoreline_t *pShoreline );
	void BuildShorelineOverlayPoints( Shoreline_t *pShoreline, CUtlVector<CMapFace*> &aWaterFaces );
	void BuildShorelineOverlayPoint( Shoreline_t *pShoreline, int iSegment, CUtlVector<CMapFace*> &aWaterFaces );
	bool TexcoordShoreline( Shoreline_t *pShoreline );
	void ShorelineLength( Shoreline_t *pShoreline );
	void GenerateTexCoord( Shoreline_t *pShoreline, int iSegment, float flLengthToSegment, bool bEnd );
	void BuildShorelineOverlays( Shoreline_t *pShoreline );
	void CreateOverlays( Shoreline_t *pShoreline, int iSegment );

	void DrawShorelines( int iShoreline );
	void DrawShorelineNormals( int iShoreline );
	void DrawShorelineOverlayPoints( CRender3D *pRender, int iShoreline );

	bool ConnectShorelineSegments( Shoreline_t *pShoreline );
	int	 FindShorelineStart( Shoreline_t *pShoreline );

	bool IsTouched( Shoreline_t *pShoreline, int iSegment )		{ return pShoreline->m_aSegments[iSegment].m_bTouch; }

private:

	CUtlVector<Shoreline_t>		m_aShorelines;

	// Displacement face and water face cache - for building.
	CUtlVector<CMapDisp*>		m_aDispCache;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static CDispShoreManager s_DispShoreManager;

IDispShoreManager *GetShoreManager( void )
{
	return &s_DispShoreManager;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CDispShoreManager::CDispShoreManager()
{
	m_aDispCache.Purge();
	m_aShorelines.Purge();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CDispShoreManager::~CDispShoreManager()
{
	m_aDispCache.Purge();
	m_aShorelines.Purge();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CDispShoreManager::Init( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::Shutdown( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int	CDispShoreManager::GetShorelineCount( void )
{
	return m_aShorelines.Count();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Shoreline_t *CDispShoreManager::GetShoreline( int nShorelineId )
{
	int nShorelineCount = m_aShorelines.Count();
	for ( int iShoreline = 0; iShoreline < nShorelineCount; ++iShoreline )
	{
		if ( m_aShorelines[iShoreline].m_nShorelineId == nShorelineId )
			return &m_aShorelines[iShoreline];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::AddShoreline( int nShorelineId )
{
	// Check to see if the id is already taken, if so remove it and re-add it.
	RemoveShoreline( nShorelineId );

	int iShoreline = m_aShorelines.AddToTail();
	m_aShorelines[iShoreline].m_nShorelineId = nShorelineId;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::RemoveShoreline( int nShorelineId )
{
	int nShorelineCount = m_aShorelines.Count();
	for ( int iShoreline = ( nShorelineCount - 1 ); iShoreline >= 0; --iShoreline )
	{
		if ( m_aShorelines[iShoreline].m_nShorelineId == nShorelineId )
		{
			m_aShorelines.Remove( iShoreline );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::BuildShoreline( int nShorelineId, CUtlVector<CMapFace*> &aFaces, CUtlVector<CMapFace*> &aWaterFaces )
{
	// Verify faces to build a shoreline.
	if ( ( aFaces.Count() == 0 ) ||( aWaterFaces.Count() == 0 ) )
		return;

	Shoreline_t *pShoreline = GetShoreline( nShorelineId );
	if ( pShoreline )
	{
		BuildShorelineSegments( pShoreline, aFaces, aWaterFaces );
		AverageShorelineNormals( pShoreline );
		BuildShorelineOverlayPoints( pShoreline, aWaterFaces );
		TexcoordShoreline( pShoreline );
		BuildShorelineOverlays( pShoreline );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::BuildShorelineSegments( Shoreline_t *pShoreline, CUtlVector<CMapFace*> &aFaces, CUtlVector<CMapFace*> &aWaterFaces )
{
	int nWaterFaceCount = aWaterFaces.Count();
	for ( int iWaterFace = 0; iWaterFace < nWaterFaceCount; ++iWaterFace )
	{
		int nFaceCount = aFaces.Count();
		for ( int iFace = 0; iFace < nFaceCount; ++iFace )
		{	
			CMapFace *pFace = aFaces.Element( iFace );
			if ( pFace )
			{
				if ( !pFace->HasDisp() )
				{
					// Ignore for now!
				}
				else
				{
					// Displacement.
					CMapDisp *pDisp = EditDispMgr()->GetDisp( pFace->GetDisp() );
					if ( pDisp )
					{
						pDisp->CreateShoreOverlays( aWaterFaces[iWaterFace], pShoreline );
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::AverageShorelineNormals( Shoreline_t *pShoreline )
{
	int nSegmentCount = pShoreline->m_aSegments.Count();
	if ( nSegmentCount == 0 )
		return;

	for ( int iSegment1 = 0; iSegment1 < nSegmentCount; ++iSegment1 )
	{
		for ( int iSegment2 = iSegment1 + 1; iSegment2 < nSegmentCount; ++iSegment2 )
		{
			int iPoint1 = -1;
			int iPoint2 = -1;

			if ( VectorsAreEqual( pShoreline->m_aSegments[iSegment1].m_vecPoints[0], 
				                  pShoreline->m_aSegments[iSegment2].m_vecPoints[0], DISPSHORE_VECTOR_EPS ) )
			{
				iPoint1 = 0;
				iPoint2 = 0;
			}

			if ( VectorsAreEqual( pShoreline->m_aSegments[iSegment1].m_vecPoints[0], 
				                  pShoreline->m_aSegments[iSegment2].m_vecPoints[1], DISPSHORE_VECTOR_EPS ) )
			{
				iPoint1 = 0;
				iPoint2 = 1;
			}

			if ( VectorsAreEqual( pShoreline->m_aSegments[iSegment1].m_vecPoints[1], 
				                  pShoreline->m_aSegments[iSegment2].m_vecPoints[0], DISPSHORE_VECTOR_EPS ) )
			{
				iPoint1 = 1;
				iPoint2 = 0;
			}

			if ( VectorsAreEqual( pShoreline->m_aSegments[iSegment1].m_vecPoints[1], 
				                  pShoreline->m_aSegments[iSegment2].m_vecPoints[1], DISPSHORE_VECTOR_EPS ) )
			{
				iPoint1 = 1;
				iPoint2 = 1;
			}

			if ( ( iPoint1 != -1 ) && ( iPoint2 != -1 ) )
			{
				pShoreline->m_aSegments[iSegment2].m_vecPoints[iPoint2] = pShoreline->m_aSegments[iSegment1].m_vecPoints[iPoint1];
				Vector vecNormal = pShoreline->m_aSegments[iSegment1].m_vecNormals[iPoint1] + pShoreline->m_aSegments[iSegment2].m_vecNormals[iPoint2];
				VectorNormalize( vecNormal );
				pShoreline->m_aSegments[iSegment1].m_vecNormals[iPoint1] = vecNormal;
				pShoreline->m_aSegments[iSegment2].m_vecNormals[iPoint2] = vecNormal;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::BuildShorelineOverlayPoints( Shoreline_t *pShoreline, CUtlVector<CMapFace*> &aWaterFaces )
{
	int nSegmentCount = pShoreline->m_aSegments.Count();
	if ( nSegmentCount == 0 )
		return;

	for ( int iSegment = 0; iSegment < nSegmentCount; ++iSegment )
	{
		BuildShorelineOverlayPoint( pShoreline, iSegment, aWaterFaces );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::BuildShorelineOverlayPoint( Shoreline_t *pShoreline, int iSegment, CUtlVector<CMapFace*> &aWaterFaces )
{
	// Get the displacement manager and segment displacement.
	CMapDisp *pDisp = EditDispMgr()->GetDisp( pShoreline->m_aSegments[iSegment].m_hDisp );
	if ( !pDisp )
		return;

	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
		return;

	// Build a bounding box from the world points.
	Vector vecPoints[4];	
	vecPoints[0] = pShoreline->m_aSegments[iSegment].m_vecPoints[0];
	vecPoints[3] = pShoreline->m_aSegments[iSegment].m_vecPoints[1];
	vecPoints[1] = vecPoints[0] + ( pShoreline->m_aSegments[iSegment].m_vecNormals[0] * pShoreline->m_ShoreData.m_flWidths[0] );
	vecPoints[2] = vecPoints[3] + ( pShoreline->m_aSegments[iSegment].m_vecNormals[1] * pShoreline->m_ShoreData.m_flWidths[0] );

	Vector vecWorldMin = vecPoints[0]; 
	Vector vecWorldMax = vecPoints[0];
	for ( int iPoint = 1; iPoint < 4; ++iPoint )
	{
		for ( int iAxis = 0; iAxis < 3; ++iAxis )
		{
			if ( vecPoints[iPoint][iAxis] < vecWorldMin[iAxis] )
			{
				vecWorldMin[iAxis] = vecPoints[iPoint][iAxis];
			}

			if ( vecPoints[iPoint][iAxis] > vecWorldMax[iAxis] )
			{
				vecWorldMax[iAxis] = vecPoints[iPoint][iAxis];
			}
		}
	}

	for ( int iAxis = 0; iAxis < 2; ++iAxis )
	{
		vecWorldMin[iAxis] -= 1.0f;
		vecWorldMax[iAxis] += 1.0f;
	}
	vecWorldMin.z -= 150.0f;
	vecWorldMax.z += 150.0f;

	// Build a list of displacements that intersect the bounding box.
	CUtlVector<CMapDisp*> m_aDispList;
	m_aDispList.Purge();

	Vector vecDispMin, vecDispMax;
	int nDispCount = pDispMgr->WorldCount();
	for ( int iDisp = 0; iDisp < nDispCount; ++iDisp )
	{
		CMapDisp *pCurDisp = pDispMgr->GetFromWorld( iDisp );
		if ( !pCurDisp )
			continue;

		if ( pCurDisp == pDisp )
			continue;

		// Check for intersections.
		pCurDisp->GetBoundingBox( vecDispMin, vecDispMax );
		if ( IsBoxIntersectingBox( vecWorldMin, vecWorldMax, vecDispMin, vecDispMax ) )
		{
			m_aDispList.AddToTail( pCurDisp );
		}
	}

	// World points.
	CMapFace *pFace = static_cast<CMapFace*>( pDisp->GetParent() );
	for ( int iFace = 0; iFace < 4; ++iFace )
	{
		pShoreline->m_aSegments[iSegment].m_WorldFace.m_pFaces[iFace] = pFace;
	}

	pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[0] = pShoreline->m_aSegments[iSegment].m_vecPoints[0];
	pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[3] = pShoreline->m_aSegments[iSegment].m_vecPoints[1];

	Vector vecPoint = pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[0] + ( pShoreline->m_aSegments[iSegment].m_vecNormals[0] * pShoreline->m_ShoreData.m_flWidths[0] );
	Vector vecStart( vecPoint.x, vecPoint.y, vecPoint.z + 150.0f );
	Vector vecEnd( vecPoint.x, vecPoint.y, vecPoint.z - 150.0f );		
	Vector vecHit, vecHitNormal;
	CMapFace *pHitFace = pFace;
	if ( !pDisp->TraceLine( vecHit, vecHitNormal, vecStart, vecEnd ) )
	{
		nDispCount = m_aDispList.Count();
		int iDisp;
		for ( iDisp = 0; iDisp < nDispCount; ++iDisp )
		{
			if ( m_aDispList[iDisp]->TraceLine( vecHit, vecHitNormal, vecStart, vecEnd ) )
			{
				pHitFace = ( CMapFace* )m_aDispList[iDisp]->GetParent();
				break;
			}
		}

		if ( iDisp == nDispCount )
		{
			pDisp->TraceLineSnapTo( vecHit, vecHitNormal, vecStart, vecEnd );
		}
	}
	pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[1] = vecHit;
	pShoreline->m_aSegments[iSegment].m_WorldFace.m_pFaces[1] = pHitFace;
	
	vecPoint = pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[3] + ( pShoreline->m_aSegments[iSegment].m_vecNormals[1] * pShoreline->m_ShoreData.m_flWidths[0] );
	vecStart.Init( vecPoint.x, vecPoint.y, vecPoint.z + 150.0f );
	vecEnd.Init( vecPoint.x, vecPoint.y, vecPoint.z - 150.0f );
	pHitFace = pFace;
	if ( !pDisp->TraceLine( vecHit, vecHitNormal, vecStart, vecEnd ) )
	{
		nDispCount = m_aDispList.Count();
		int iDisp;
		for ( iDisp = 0; iDisp < nDispCount; ++iDisp )
		{
			if ( m_aDispList[iDisp]->TraceLine( vecHit, vecHitNormal, vecStart, vecEnd ) )
			{
				pHitFace = ( CMapFace* )m_aDispList[iDisp]->GetParent();
				break;
			}
		}

		if ( iDisp == nDispCount )
		{
			pDisp->TraceLineSnapTo( vecHit, vecHitNormal, vecStart, vecEnd );
		}
	}
	pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[2] = vecHit;
	pShoreline->m_aSegments[iSegment].m_WorldFace.m_pFaces[2] = pHitFace;
	
	// Water points.
	pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[0] = pShoreline->m_aSegments[iSegment].m_vecPoints[0] + ( pShoreline->m_aSegments[iSegment].m_vecNormals[0] * -pShoreline->m_ShoreData.m_flWidths[1] );
	pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[1] = pShoreline->m_aSegments[iSegment].m_vecPoints[0];
	pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[2] = pShoreline->m_aSegments[iSegment].m_vecPoints[1];
	pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[3] = pShoreline->m_aSegments[iSegment].m_vecPoints[1] + ( pShoreline->m_aSegments[iSegment].m_vecNormals[1] * -pShoreline->m_ShoreData.m_flWidths[1] );
	int nWaterFaceCount = aWaterFaces.Count();
	for ( int iWaterFace = 0; iWaterFace < nWaterFaceCount; ++iWaterFace )
	{
		CMapFace *pWaterFace = aWaterFaces.Element( iWaterFace );
		if ( pWaterFace )
		{
			for ( int iWaterPoint = 0; iWaterPoint < 4; ++iWaterPoint )
			{
				vecPoint = pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[iWaterPoint];
				vecStart.Init( vecPoint.x, vecPoint.y, vecPoint.z + 150.0f );
				vecEnd.Init( vecPoint.x, vecPoint.y, vecPoint.z - 150.0f );
				if ( pWaterFace->TraceLineInside( vecHit, vecHitNormal, vecStart, vecEnd ) )
				{
					pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[iWaterPoint] = pWaterFace;
				}
			}
		}
	}

	// Water face clean up!
	int nNoFaceCount = false;
	for ( int iWaterPoint = 0; iWaterPoint < 4; ++iWaterPoint )
	{
		if ( !pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[iWaterPoint] )
		{
			++nNoFaceCount;
		}
	}
	if ( ( nNoFaceCount > 0 ) && ( nNoFaceCount < 4 ) )
	{
		// Find a valid face.
		CMapFace *pWaterFace = NULL;
		for ( int iWaterPoint = 0; iWaterPoint < 4; ++iWaterPoint )
		{
			if ( pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[iWaterPoint] )
			{
				pWaterFace = pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[iWaterPoint];
				break;
			}
		}
	
		for ( int iWaterPoint = 0; iWaterPoint < 4; ++iWaterPoint )
		{
			if ( !pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[iWaterPoint] )
			{
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[iWaterPoint] = pWaterFace;
			}
		}
	}	

	// Center.
	pShoreline->m_aSegments[iSegment].m_vecCenter = ( pShoreline->m_aSegments[iSegment].m_vecPoints[0] + pShoreline->m_aSegments[iSegment].m_vecPoints[1] ) * 0.5f;

	// Check winding.
	Vector vecEdge0, vecEdge1, vecCross;

	pShoreline->m_aSegments[iSegment].m_WorldFace.m_bAdjWinding = false;
	VectorSubtract( pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[1], pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[0], vecEdge0 );
	VectorSubtract( pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[2], pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[0], vecEdge1 );
	VectorNormalize( vecEdge0 );
	VectorNormalize( vecEdge1 );
	CrossProduct( vecEdge1, vecEdge0, vecCross );
	if ( vecCross.z < 0.0f )
	{
		// Adjust winding.
		Vector vecTmp = pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[1];
		CMapFace *pTmpFace = pShoreline->m_aSegments[iSegment].m_WorldFace.m_pFaces[1];
		pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[1] = pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[3];
		pShoreline->m_aSegments[iSegment].m_WorldFace.m_pFaces[1] = pShoreline->m_aSegments[iSegment].m_WorldFace.m_pFaces[3];
		pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[3] = vecTmp;
		pShoreline->m_aSegments[iSegment].m_WorldFace.m_pFaces[3] = pTmpFace;
		pShoreline->m_aSegments[iSegment].m_WorldFace.m_bAdjWinding = true;
	}

	pShoreline->m_aSegments[iSegment].m_WaterFace.m_bAdjWinding = false;
	VectorSubtract( pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[1], pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[0], vecEdge0 );
	VectorSubtract( pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[2], pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[0], vecEdge1 );
	VectorNormalize( vecEdge0 );
	VectorNormalize( vecEdge1 );
	CrossProduct( vecEdge1, vecEdge0, vecCross );
	if ( vecCross.z < 0.0f )
	{
		// Adjust winding.
		Vector vecTmp = pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[1];
		CMapFace *pTmpFace = pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[1];
		pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[1] = pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[3];
		pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[1] = pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[3];
		pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[3] = vecTmp;
		pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[3] = pTmpFace;
		pShoreline->m_aSegments[iSegment].m_WaterFace.m_bAdjWinding = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CDispShoreManager::TexcoordShoreline( Shoreline_t *pShoreline )
{
	int nSegmentCount = pShoreline->m_aSegments.Count();
	if ( nSegmentCount == 0 )
		return false;

	// Conncect the shoreline segments to produce a continuous shoreline.
	if ( !ConnectShorelineSegments( pShoreline ) )
		return false;

	ShorelineLength( pShoreline );

	float flLengthToSegment = 0.0f;
	int nSortedSegmentCount = pShoreline->m_aSortedSegments.Count();
	for ( int iSegment = 0; iSegment < nSortedSegmentCount; ++iSegment )
	{
		int iSortSegment = pShoreline->m_aSortedSegments[iSegment];

		GenerateTexCoord( pShoreline, iSortSegment, flLengthToSegment, false );

		Vector vecEdge;
		VectorSubtract( pShoreline->m_aSegments[iSortSegment].m_vecPoints[1], pShoreline->m_aSegments[iSortSegment].m_vecPoints[0], vecEdge );
		flLengthToSegment += vecEdge.Length();

		GenerateTexCoord( pShoreline, iSortSegment, flLengthToSegment, true );
	}	

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CDispShoreManager::ConnectShorelineSegments( Shoreline_t *pShoreline )
{
	// Reset/recreate the shoreline sorted segment list.
	pShoreline->m_aSortedSegments.Purge();

	int iSegment = FindShorelineStart( pShoreline );
	if ( iSegment == -1 )
	{
		iSegment = 0;
	}

	int nSegmentCount = pShoreline->m_aSegments.Count();
	while ( iSegment != -1 )
	{
		int iSegment2;
		for ( iSegment2 = 0; iSegment2 < nSegmentCount; ++iSegment2 )
		{
			if ( iSegment2 == iSegment )
				continue;

			bool bIsTouching0 = false;
			if ( VectorsAreEqual( pShoreline->m_aSegments[iSegment].m_vecPoints[0], pShoreline->m_aSegments[iSegment2].m_vecPoints[0], DISPSHORE_VECTOR_EPS ) ) { bIsTouching0 = true; }
			if ( VectorsAreEqual( pShoreline->m_aSegments[iSegment].m_vecPoints[1], pShoreline->m_aSegments[iSegment2].m_vecPoints[0], DISPSHORE_VECTOR_EPS ) ) { bIsTouching0 = true; }
			bool bIsTouching1 = false;
			if ( VectorsAreEqual( pShoreline->m_aSegments[iSegment].m_vecPoints[0], pShoreline->m_aSegments[iSegment2].m_vecPoints[1], DISPSHORE_VECTOR_EPS ) ) { bIsTouching1 = true; }
			if ( VectorsAreEqual( pShoreline->m_aSegments[iSegment].m_vecPoints[1], pShoreline->m_aSegments[iSegment2].m_vecPoints[1], DISPSHORE_VECTOR_EPS ) ) { bIsTouching1 = true; }

			if ( ( bIsTouching0 || bIsTouching1 ) && !IsTouched( pShoreline, iSegment2 ) )
			{
				pShoreline->m_aSegments[iSegment2].m_iStartPoint = 0;
				if ( bIsTouching1 )
				{
					pShoreline->m_aSegments[iSegment2].m_iStartPoint = 1;
				}

				pShoreline->m_aSortedSegments.AddToTail( iSegment2 );
				pShoreline->m_aSegments[iSegment2].m_bTouch = true;
				break;
			}
		}

		if ( iSegment2 != nSegmentCount )
		{
			iSegment = iSegment2;
		}
		else
		{
			iSegment = -1;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CDispShoreManager::FindShorelineStart( Shoreline_t *pShoreline )
{
	// Find a segment that doesn't have any (fewest) matching point data.
	int nSegmentCount = pShoreline->m_aSegments.Count();
	for ( int iSegment = 0; iSegment < nSegmentCount; ++iSegment )
	{
		int nTouchCount = 0;
		int iStartPoint = -1;
		for ( int iSegment2 = 0; iSegment2 < nSegmentCount; ++iSegment2 )
		{
			if ( iSegment == iSegment2 )
				continue;

			if ( VectorsAreEqual( pShoreline->m_aSegments[iSegment].m_vecPoints[0], pShoreline->m_aSegments[iSegment2].m_vecPoints[0], DISPSHORE_VECTOR_EPS ) ) 
			{ 
				++nTouchCount; 
				iStartPoint = 1;
			}
			if ( VectorsAreEqual( pShoreline->m_aSegments[iSegment].m_vecPoints[0], pShoreline->m_aSegments[iSegment2].m_vecPoints[1], DISPSHORE_VECTOR_EPS ) ) 
			{ 
				++nTouchCount;
				iStartPoint = 1;
			}
			if ( VectorsAreEqual( pShoreline->m_aSegments[iSegment].m_vecPoints[1], pShoreline->m_aSegments[iSegment2].m_vecPoints[0], DISPSHORE_VECTOR_EPS ) ) 
			{ 
				++nTouchCount;
				iStartPoint = 0;
			}
			if ( VectorsAreEqual( pShoreline->m_aSegments[iSegment].m_vecPoints[1], pShoreline->m_aSegments[iSegment2].m_vecPoints[1], DISPSHORE_VECTOR_EPS ) ) 
			{ 
				++nTouchCount; 
				iStartPoint = 0;
			}
		}

		if ( nTouchCount == 1 )
		{
			pShoreline->m_aSegments[iSegment].m_iStartPoint = iStartPoint;
			pShoreline->m_aSortedSegments.AddToTail( iSegment );
			pShoreline->m_aSegments[iSegment].m_bTouch = true;
			return iSegment;
		}
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::ShorelineLength( Shoreline_t *pShoreline )
{
	float flLength = 0.0f;
	int nSegmentCount = pShoreline->m_aSegments.Count();
	for ( int iSegment = 0; iSegment < nSegmentCount; ++iSegment )
	{
		Vector vecEdge;
		VectorSubtract( pShoreline->m_aSegments[iSegment].m_vecPoints[1], pShoreline->m_aSegments[iSegment].m_vecPoints[0], vecEdge );
		flLength += vecEdge.Length();
	}

	pShoreline->m_flLength = flLength;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::GenerateTexCoord( Shoreline_t *pShoreline, int iSegment, float flLengthToSegment, bool bEnd )
{
	float flValue = pShoreline->m_ShoreData.m_vecLengthTexcoord[1] - pShoreline->m_ShoreData.m_vecLengthTexcoord[0];

	if ( pShoreline->m_aSegments[iSegment].m_iStartPoint == 0 )
	{
		if ( !bEnd )
		{
			float flRatio = flLengthToSegment / pShoreline->m_flLength;
			pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[0].x = flValue * flRatio;
			pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[0].x = flValue * flRatio;
			pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[0].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[0].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[0];
			
			if ( pShoreline->m_aSegments[iSegment].m_WorldFace.m_bAdjWinding )
			{
				pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[3].x = flValue * flRatio;
				pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[3].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1];
			}
			else
			{
				pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[1].x = flValue * flRatio;
				pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[1].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1];
			}

			if ( pShoreline->m_aSegments[iSegment].m_WaterFace.m_bAdjWinding )
			{
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[3].x = flValue * flRatio;
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[3].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			}
			else
			{
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[1].x = flValue * flRatio;
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[1].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			}
		}
		else
		{
			float flRatio = flLengthToSegment / pShoreline->m_flLength;

			pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[2].x = flValue * flRatio;
			pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[2].x = flValue * flRatio;
			pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[2].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1];
			pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[2].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			
			if ( pShoreline->m_aSegments[iSegment].m_WorldFace.m_bAdjWinding )
			{
				flRatio = flLengthToSegment / pShoreline->m_flLength;
				pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[1].x = flValue * flRatio;
				pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[1].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			}
			else
			{
				flRatio = flLengthToSegment / pShoreline->m_flLength;
				pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[3].x = flValue * flRatio;
				pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[3].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			}

			if ( pShoreline->m_aSegments[iSegment].m_WaterFace.m_bAdjWinding )
			{
				flRatio = flLengthToSegment / pShoreline->m_flLength;
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[1].x = flValue * flRatio;
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[1].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[0];
			}
			else
			{
				flRatio = flLengthToSegment / pShoreline->m_flLength;
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[3].x = flValue * flRatio;
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[3].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[0];
			}
		}
	}
	else
	{
		if ( !bEnd )
		{
			float flRatio = flLengthToSegment / pShoreline->m_flLength;
			pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[2].x = flValue * flRatio;
			pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[2].x = flValue * flRatio;
			pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[2].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1];
			pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[2].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			
			if ( pShoreline->m_aSegments[iSegment].m_WorldFace.m_bAdjWinding )
			{
				flRatio = flLengthToSegment / pShoreline->m_flLength;
				pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[1].x = flValue * flRatio;
				pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[1].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			}
			else
			{
				flRatio = flLengthToSegment / pShoreline->m_flLength;
				pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[3].x = flValue * flRatio;
				pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[3].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			}

			if ( pShoreline->m_aSegments[iSegment].m_WaterFace.m_bAdjWinding )
			{
				flRatio = flLengthToSegment / pShoreline->m_flLength;
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[1].x = flValue * flRatio;
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[1].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[0];
			}
			else
			{
				flRatio = flLengthToSegment / pShoreline->m_flLength;
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[3].x = flValue * flRatio;
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[3].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[0];
			}
		}
		else
		{
			float flRatio = flLengthToSegment / pShoreline->m_flLength;
			pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[0].x = flValue * flRatio;
			pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[0].x = flValue * flRatio;
			pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[0].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[0].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[0];
			
			if ( pShoreline->m_aSegments[iSegment].m_WorldFace.m_bAdjWinding )
			{
				pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[3].x = flValue * flRatio;
				pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[3].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1];
			}
			else
			{
				pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[1].x = flValue * flRatio;
				pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords[1].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1];
			}

			if ( pShoreline->m_aSegments[iSegment].m_WaterFace.m_bAdjWinding )
			{
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[3].x = flValue * flRatio;
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[3].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			}
			else
			{
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[1].x = flValue * flRatio;
				pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords[1].y = pShoreline->m_ShoreData.m_vecWidthTexcoord[1] * 0.5f;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::BuildShorelineOverlays( Shoreline_t *pShoreline )
{	
	// Reset the list.
	if ( pShoreline->m_aOverlays.Count() != 0 )
	{
		pShoreline->m_aOverlays.Purge();
	}

	int nSegmentCount = pShoreline->m_aSegments.Count();
	if ( nSegmentCount == 0 )
		return;

	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc )
		return;
	
	for ( int iSegment = 0; iSegment < nSegmentCount; ++iSegment )
	{
		CMapDisp *pDisp = EditDispMgr()->GetDisp( pShoreline->m_aSegments[iSegment].m_hDisp );
		if ( !pDisp )
			continue;

		CMapFace *pFace = ( CMapFace* )pDisp->GetParent();
		if ( !pFace )
			continue;

		CreateOverlays( pShoreline, iSegment );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::CreateOverlays( Shoreline_t *pShoreline, int iSegment )
{
	// Create the face list than this overlay will act upon.
	CUtlVector<CMapFace*> aWorldFaces;
	CUtlVector<CMapFace*> aWaterFaces;
	for ( int iFace = 0; iFace < 4; ++iFace )
	{
		if ( !pShoreline->m_aSegments[iSegment].m_WorldFace.m_pFaces[iFace] ||
			 !pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[iFace] )
			return;

		// World
		if ( aWorldFaces.Find( pShoreline->m_aSegments[iSegment].m_WorldFace.m_pFaces[iFace] ) == -1 )
		{
			aWorldFaces.AddToTail( pShoreline->m_aSegments[iSegment].m_WorldFace.m_pFaces[iFace] );
		}

		// Water
		if ( aWaterFaces.Find( pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[iFace] ) == -1 )
		{
			aWaterFaces.AddToTail( pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[iFace] );
		}
	}

	// Create and add data to the world overlay.
	int iOverlay = pShoreline->m_aOverlays.AddToTail();
	CMapOverlay *pOverlay = &pShoreline->m_aOverlays[iOverlay];

	pOverlay->SetOverlayType( OVERLAY_TYPE_SHORE );

	pOverlay->Basis_Init( aWorldFaces[0] );
	pOverlay->Handles_Init( aWorldFaces[0] );
	pOverlay->SideList_Init( aWorldFaces[0] );

	int nFaceCount = aWorldFaces.Count();
	for ( int iFace = 1; iFace < nFaceCount; ++iFace )
	{
		pOverlay->SideList_AddFace( aWorldFaces[iFace] );
	}

	pOverlay->SetLoaded( true );

	pOverlay->HandleMoveTo( 0, pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[0], pShoreline->m_aSegments[iSegment].m_WorldFace.m_pFaces[0] );
	pOverlay->HandleMoveTo( 1, pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[1], pShoreline->m_aSegments[iSegment].m_WorldFace.m_pFaces[1] );
	pOverlay->HandleMoveTo( 2, pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[2], pShoreline->m_aSegments[iSegment].m_WorldFace.m_pFaces[2] );
	pOverlay->HandleMoveTo( 3, pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[3], pShoreline->m_aSegments[iSegment].m_WorldFace.m_pFaces[3] );

	if ( !pShoreline->m_ShoreData.m_pTexture )
	{
		pOverlay->SetMaterial( "decals/decal_signroute004b" );
	}
	else
	{
		pOverlay->SetMaterial( pShoreline->m_ShoreData.m_pTexture );
	}
	pOverlay->SetTexCoords( pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecTexCoords );

	pOverlay->CalcBounds( true );

	pOverlay->DoClip();
	pOverlay->PostUpdate( Notify_Changed );

	// Create and add data to the water overlay.
	iOverlay = pShoreline->m_aOverlays.AddToTail();
	pOverlay = &pShoreline->m_aOverlays[iOverlay];

	pOverlay->SetOverlayType( OVERLAY_TYPE_SHORE );

	pOverlay->Basis_Init( aWaterFaces[0] );
	pOverlay->Handles_Init( aWaterFaces[0] );
	pOverlay->SideList_Init( aWaterFaces[0] );

	nFaceCount = aWaterFaces.Count();
	for ( int iFace = 1; iFace < nFaceCount; ++iFace )
	{
		pOverlay->SideList_AddFace( aWaterFaces[iFace] );
	}

	pOverlay->SetLoaded( true );

	pOverlay->HandleMoveTo( 0, pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[0], pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[0] );
	pOverlay->HandleMoveTo( 1, pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[1], pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[1] );
	pOverlay->HandleMoveTo( 2, pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[2], pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[2] );
	pOverlay->HandleMoveTo( 3, pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[3], pShoreline->m_aSegments[iSegment].m_WaterFace.m_pFaces[3] );

	if ( !pShoreline->m_ShoreData.m_pTexture )
	{
		pOverlay->SetMaterial( "decals/decal_signroute004b" );
	}
	else
	{
		pOverlay->SetMaterial( pShoreline->m_ShoreData.m_pTexture );
	}
	pOverlay->SetTexCoords( pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecTexCoords );

	pOverlay->SetOverlayType( OVERLAY_TYPE_SHORE );

	pOverlay->CalcBounds( true );

	pOverlay->DoClip();
	pOverlay->PostUpdate( Notify_Changed );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::Draw( CRender3D *pRender )
{
	int nShoreCount = m_aShorelines.Count();
	for ( int iShore = 0; iShore < nShoreCount; ++iShore )
	{
		Shoreline_t *pShoreline = &m_aShorelines[iShore];
		if ( pShoreline )
		{
			int nOverlayCount = pShoreline->m_aOverlays.Count();
			for ( int iOverlay = 0; iOverlay < nOverlayCount; ++iOverlay )
			{
				CMapOverlay *pOverlay = &pShoreline->m_aOverlays[iOverlay];
				if ( pOverlay )
				{
					pOverlay->Render3D( pRender );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::DebugDraw( CRender3D *pRender )
{
	pRender->SetRenderMode( RENDER_MODE_WIREFRAME );

	int nShorelineCount = GetShorelineCount();
	for ( int iShoreline = 0; iShoreline < nShorelineCount; ++iShoreline )
	{
		DrawShorelines( iShoreline );
		DrawShorelineNormals( iShoreline );
		DrawShorelineOverlayPoints( pRender, iShoreline );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::DrawShorelines( int iShoreline )
{
	Shoreline_t *pShoreline = &m_aShorelines[iShoreline];
	if ( pShoreline )
	{
		int nSegmentCount = pShoreline->m_aSegments.Count();
		if ( nSegmentCount == 0 )
			return;

		CMeshBuilder meshBuilder;
		CMatRenderContextPtr pRenderContext( MaterialSystemInterface() );
		IMesh* pMesh = pRenderContext->GetDynamicMesh();
		meshBuilder.Begin( pMesh, MATERIAL_LINES, ( nSegmentCount * 2 ) );

		for ( int iSegment = 0; iSegment < nSegmentCount; ++iSegment )
		{
			meshBuilder.Color3f( 1.0f, 0.0f, 0.0f );
			meshBuilder.Position3f( pShoreline->m_aSegments[iSegment].m_vecPoints[0].x, 
				                    pShoreline->m_aSegments[iSegment].m_vecPoints[0].y, 
				                    pShoreline->m_aSegments[iSegment].m_vecPoints[0].z + 50.0f );
			meshBuilder.AdvanceVertex();
			
			meshBuilder.Color3f( 1.0f, 0.0f, 0.0f );
			meshBuilder.Position3f( pShoreline->m_aSegments[iSegment].m_vecPoints[1].x, 
				                    pShoreline->m_aSegments[iSegment].m_vecPoints[1].y, 
				                    pShoreline->m_aSegments[iSegment].m_vecPoints[1].z + 50.0f );
			meshBuilder.AdvanceVertex();
		}

		meshBuilder.End();
		pMesh->Draw();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::DrawShorelineNormals( int iShoreline )
{
#define DISPSHORE_NORMAL_SCALE 25.0f

	Shoreline_t *pShoreline = &m_aShorelines[iShoreline];
	if ( pShoreline )
	{
		int nSegmentCount = pShoreline->m_aSegments.Count();
		if ( nSegmentCount == 0 )
			return;

		CMeshBuilder meshBuilder;
		CMatRenderContextPtr pRenderContext( materials );
		IMesh* pMesh = pRenderContext->GetDynamicMesh();
		meshBuilder.Begin( pMesh, MATERIAL_LINES, ( nSegmentCount * 4 ) );

		for ( int iSegment = 0; iSegment < nSegmentCount; ++iSegment )
		{
			// Normal for vertex 0.
			meshBuilder.Color3f( 1.0f, 1.0f, 0.0f );
			meshBuilder.Position3f( pShoreline->m_aSegments[iSegment].m_vecPoints[0].x, 
				                    pShoreline->m_aSegments[iSegment].m_vecPoints[0].y, 
				                    pShoreline->m_aSegments[iSegment].m_vecPoints[0].z + 50.0f );
			meshBuilder.AdvanceVertex();
			
			meshBuilder.Color3f( 1.0f, 1.0f, 0.0f );
			meshBuilder.Position3f( pShoreline->m_aSegments[iSegment].m_vecPoints[0].x + ( pShoreline->m_aSegments[iSegment].m_vecNormals[0].x * DISPSHORE_NORMAL_SCALE ), 
				                    pShoreline->m_aSegments[iSegment].m_vecPoints[0].y + ( pShoreline->m_aSegments[iSegment].m_vecNormals[0].y * DISPSHORE_NORMAL_SCALE ), 
				                    pShoreline->m_aSegments[iSegment].m_vecPoints[0].z + 50.0f + ( pShoreline->m_aSegments[iSegment].m_vecNormals[0].z * DISPSHORE_NORMAL_SCALE ) );
			meshBuilder.AdvanceVertex();
			
			// Normal for vertex 1.
			meshBuilder.Color3f( 1.0f, 1.0f, 0.0f );
			meshBuilder.Position3f( pShoreline->m_aSegments[iSegment].m_vecPoints[1].x, 
				                    pShoreline->m_aSegments[iSegment].m_vecPoints[1].y, 
				                    pShoreline->m_aSegments[iSegment].m_vecPoints[1].z + 50.0f );
			meshBuilder.AdvanceVertex();
			
			meshBuilder.Color3f( 1.0f, 1.0f, 0.0f );
			meshBuilder.Position3f( pShoreline->m_aSegments[iSegment].m_vecPoints[1].x + ( pShoreline->m_aSegments[iSegment].m_vecNormals[1].x * DISPSHORE_NORMAL_SCALE ), 
				                    pShoreline->m_aSegments[iSegment].m_vecPoints[1].y + ( pShoreline->m_aSegments[iSegment].m_vecNormals[1].y * DISPSHORE_NORMAL_SCALE ), 
				                    pShoreline->m_aSegments[iSegment].m_vecPoints[1].z + 50.0f + ( pShoreline->m_aSegments[iSegment].m_vecNormals[1].z * DISPSHORE_NORMAL_SCALE ) );
			meshBuilder.AdvanceVertex();
		}

		meshBuilder.End();
		pMesh->Draw();
	}

#undef DISPSHORE_NORMAL_SCALE
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDispShoreManager::DrawShorelineOverlayPoints( CRender3D *pRender, int iShoreline )
{
#define DISPSHORE_BOX_SIZE	5.0f

	Shoreline_t *pShoreline = &m_aShorelines[iShoreline];
	if ( pShoreline )
	{
		int nSegmentCount = pShoreline->m_aSegments.Count();
		if ( nSegmentCount == 0 )
			return;
		
		Vector vecWorldMin, vecWorldMax;
		for ( int iSegment = 0; iSegment < nSegmentCount; ++iSegment )
		{
			for ( int iWorldPoint = 0; iWorldPoint < 4; ++iWorldPoint )
			{
				vecWorldMin = pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[iWorldPoint];
				vecWorldMax = pShoreline->m_aSegments[iSegment].m_WorldFace.m_vecPoints[iWorldPoint];
				for ( int iAxis = 0; iAxis < 3; ++iAxis )
				{
					vecWorldMin[iAxis] -= DISPSHORE_BOX_SIZE;
					vecWorldMax[iAxis] += DISPSHORE_BOX_SIZE;
				}
				
				pRender->RenderBox( vecWorldMin, vecWorldMax, 255, 0, 0, SELECT_NONE );
			}
			
			for ( int iWorldPoint = 0; iWorldPoint < 4; ++iWorldPoint )
			{
				vecWorldMin = pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[iWorldPoint];
				vecWorldMax = pShoreline->m_aSegments[iSegment].m_WaterFace.m_vecPoints[iWorldPoint];
				for ( int iAxis = 0; iAxis < 3; ++iAxis )
				{
					vecWorldMin[iAxis] -= DISPSHORE_BOX_SIZE;
					vecWorldMax[iAxis] += DISPSHORE_BOX_SIZE;
				}
				
				pRender->RenderBox( vecWorldMin, vecWorldMax, 0, 0, 255, SELECT_NONE );
			}
		}
	}

#undef DISPSHORE_BOX_SIZE
}

