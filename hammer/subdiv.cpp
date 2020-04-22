//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdafx.h>
#include "MainFrm.h"
#include "MapDoc.h"
#include "GlobalFunctions.h"
#include "Subdiv.h"
#include "History.h"

//=============================================================================
//
// Subdivision Point Functions
//

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivPoint::Clear( void )
{
	VectorClear( m_Point );
	VectorClear( m_NewPoint );
	VectorClear( m_Normal );
	VectorClear( m_NewNormal );

	m_Type = -1;
	m_Valence = 0;

	for( int i = 0; i < NUM_SUBDIV_EDGES; i++ )
	{
		m_pEdges[i] = NULL;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivPoint::Copy( const CSubdivPoint *pFrom )
{
	m_Point = pFrom->m_Point;
	m_NewPoint = pFrom->m_NewPoint;
	m_Normal = pFrom->m_Normal;
	m_NewNormal = pFrom->m_NewNormal;

	m_Type = pFrom->m_Type;
	m_Valence = pFrom->m_Valence;

	for( int i = 0; i < NUM_SUBDIV_EDGES; i++ )
	{
		m_pEdges[i] = pFrom->m_pEdges[i];
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivPoint::CalcNewVertexNormal( void )
{
	switch( m_Type )
	{
	case POINT_CORNER:
		{
			m_NewNormal = m_Normal;
			break;
		}
	case POINT_CREASE:
		{
			Vector edgeAccum;
			VectorClear( edgeAccum );
			for( int i = 0; i < m_Valence; i++ )
			{
				if( m_pEdges[i]->m_Sharpness > 0.0f )
				{
					VectorAdd( edgeAccum, m_pEdges[i]->m_NewEdgeNormal, edgeAccum );
				}
			}

			//
			// normal
			//
			VectorScale( m_Normal, 6.0f, m_NewNormal );
			VectorAdd( m_NewNormal, edgeAccum, m_NewNormal );
			VectorScale( m_NewNormal, 0.125f, m_NewNormal );

			break;
		}
	case POINT_ORDINARY:
		{
			//
			// accumulate edge data and multiply by valence ratio
			//
			Vector edgeAccum;
			VectorClear( edgeAccum );
			for( int i = 0; i < m_Valence; i++ )
			{
				VectorAdd( edgeAccum, m_pEdges[i]->m_NewEdgeNormal, edgeAccum );
			}
			float ratio = ( 1.0f / ( float )( m_Valence * m_Valence ) );
			VectorScale( edgeAccum, ratio, edgeAccum );

			//
			// accumulate centroid data and multiply by valence ratio
			//
			int	        quadCount = 0;
			CSubdivQuad *quadList[16];
			for( i = 0; i < m_Valence; i++ )
			{
				for( int j = 0; j < 2; j++ )
				{
					if( m_pEdges[i]->m_pQuads[j] )
					{
						for( int k = 0; k < quadCount; k++ )
						{
							if( m_pEdges[i]->m_pQuads[j] == quadList[k] )
								break;
						}

						if( k != quadCount )
							continue;

						quadList[quadCount] = m_pEdges[i]->m_pQuads[j];
						quadCount++;
					}
				}
			}

			Vector centroidAccum;
			VectorClear( centroidAccum );
			for( i = 0; i < quadCount; i++ )
			{
				Vector centroid;
				quadList[i]->GetNormal( centroid );
				VectorAdd( centroidAccum, centroid, centroidAccum );
			}
			VectorScale( centroidAccum, ratio, centroidAccum );

			//
			// normal
			//
			VectorScale( m_Normal, ratio, m_NewNormal );
			VectorAdd( m_NewNormal, edgeAccum, m_NewNormal );
			VectorAdd( m_NewNormal, centroidAccum, m_NewNormal );

			break;
		}
	default:
		break;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivPoint::CalcNewVertexPoint( void )
{
	switch( m_Type )
	{
	case POINT_CORNER:
		{
			m_NewPoint = m_Point;
			break;
		}
	case POINT_CREASE:
		{
			Vector edgeAccum;
			VectorClear( edgeAccum );
			for( int i = 0; i < m_Valence; i++ )
			{
				if( m_pEdges[i]->m_Sharpness > 0.0f )
				{
					VectorAdd( edgeAccum, m_pEdges[i]->m_NewEdgePoint, edgeAccum );
				}
			}

			//
			// point
			//
			VectorScale( m_Point, 6.0f, m_NewPoint );
			VectorAdd( m_NewPoint, edgeAccum, m_NewPoint );
			VectorScale( m_NewPoint, 0.125f, m_NewPoint );

			break;
		}
	case POINT_ORDINARY:
		{
			//
			// accumulate edge data and multiply by valence ratio
			//
			Vector edgeAccum;
			VectorClear( edgeAccum );
			for( int i = 0; i < m_Valence; i++ )
			{
				VectorAdd( edgeAccum, m_pEdges[i]->m_NewEdgePoint, edgeAccum );
			}
			float ratio = ( 1.0f / ( float )( m_Valence * m_Valence ) );
			VectorScale( edgeAccum, ratio, edgeAccum );

			//
			// accumulate centroid data and multiply by valence ratio
			//
			int	        quadCount = 0;
			CSubdivQuad *quadList[16];
			for( i = 0; i < m_Valence; i++ )
			{
				for( int j = 0; j < 2; j++ )
				{
					if( m_pEdges[i]->m_pQuads[j] )
					{
						for( int k = 0; k < quadCount; k++ )
						{
							if( m_pEdges[i]->m_pQuads[j] == quadList[k] )
								break;
						}

						if( k != quadCount )
							continue;

						quadList[quadCount] = m_pEdges[i]->m_pQuads[j];
						quadCount++;
					}
				}
			}

			Vector centroidAccum;
			VectorClear( centroidAccum );
			for( i = 0; i < quadCount; i++ )
			{
				Vector centroid;
				quadList[i]->GetCentroid( centroid );
				VectorAdd( centroidAccum, centroid, centroidAccum );
			}
			VectorScale( centroidAccum, ratio, centroidAccum );

			//
			// point contribution to eqtn.
			//
			ratio = ( ( float )m_Valence - 2.0f ) / ( float )m_Valence;
			VectorScale( m_Point, ratio, m_NewPoint );

			VectorAdd( m_NewPoint, edgeAccum, m_NewPoint );
			VectorAdd( m_NewPoint, centroidAccum, m_NewPoint );

			break;
		}
	default:
		break;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CompareSubdivPoints( const CSubdivPoint *pPoint1, const CSubdivPoint *pPoint2, float tolerance )
{
	for( int i = 0 ; i < 3 ; i++ )
	{
		if( fabs( pPoint1->m_Point[i] - pPoint2->m_Point[i] ) > tolerance )
			return false;
	}
	
	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CompareSubdivPointToPoint( const CSubdivPoint *pSubdivPoint, const Vector& point, float tolerance )
{
	for( int i = 0 ; i < 3 ; i++ )
	{
		if( fabs( pSubdivPoint->m_Point[i] - point[i] ) > tolerance )
			return false;
	}
	
	return true;
}


//=============================================================================
//
// Subdivision Edge Functions
//

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivEdge::Clear( void )
{
	for( int i = 0; i < 2; i++ )
	{
		m_ndxPoint[i] = -1;
		m_pQuads[i] = NULL;
		m_ndxQuadEdge[i] = -1;
	}

	m_Sharpness = 1.0f;
	VectorClear( m_NewEdgePoint );
	m_Active = false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivEdge::Copy( const CSubdivEdge *pFrom )
{
	for( int i = 0; i < 2; i++ )
	{
		m_ndxPoint[i] = pFrom->m_ndxPoint[i];
		m_pQuads[i] = pFrom->m_pQuads[i];
		m_ndxQuadEdge[i] = pFrom->m_ndxQuadEdge[i];
	}

	m_Sharpness = pFrom->m_Sharpness;
	m_NewEdgePoint = pFrom->m_NewEdgePoint;
	m_Active = pFrom->m_Active;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivEdge::CalcNewEdgeNormal( void )
{
	if( !m_Active )
		return;

	//
	// get the subdivision mesh
	//
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if( !pDoc )
		return;

	CSubdivMesh *pMesh = pDoc->GetSubdivMesh();

	//
	// get the edge data
	//
	Vector normal0, normal1;
	pMesh->GetNormal( m_ndxPoint[0], normal0 );
	pMesh->GetNormal( m_ndxPoint[1], normal1 );

	//
	// calculate the "sharp" new edge point
	//
	Vector vSharp;
	VectorClear( vSharp );
	VectorAdd( normal0, normal1, vSharp );
	VectorScale( vSharp, 0.5f, vSharp );

	//
	// calculate the "smooth" new edge point if necessary
	//
	Vector vSmooth;
	VectorClear( vSmooth );
	if( m_pQuads[1] && ( m_Sharpness != 1.0f ) )
	{
		Vector quadNormals[2];
		m_pQuads[0]->GetNormal( quadNormals[0] );
		m_pQuads[1]->GetNormal( quadNormals[1] );
		VectorAdd( normal0, normal1, vSmooth );
		VectorAdd( vSmooth, quadNormals[0], vSmooth );
		VectorAdd( vSmooth, quadNormals[1], vSmooth );
		VectorScale( vSmooth, 0.25f, vSmooth );
	}
	else
	{
		// make sure -- if here because of no neighboring quad
		m_Sharpness = 1.0f;
		m_pQuads[0]->CalcNormal();
	}
			
	//
	// calculate the new edge point
	//
	// ( 1 - edge(sharpness) ) * vSmooth + edge(sharpness) * vSharp
	//
	VectorScale( vSmooth, ( 1.0f - m_Sharpness ), vSmooth );
	VectorScale( vSharp, m_Sharpness, vSharp );
	VectorAdd( vSmooth, vSharp, m_NewEdgeNormal );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivEdge::CalcNewEdgePoint( void )
{
	if( !m_Active )
		return;

	//
	// get the subdivision mesh
	//
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if( !pDoc )
		return;

	CSubdivMesh *pMesh = pDoc->GetSubdivMesh();

	//
	// get the edge data
	//
	Vector edgePt0, edgePt1;
	pMesh->GetPoint( m_ndxPoint[0], edgePt0 );
	pMesh->GetPoint( m_ndxPoint[1], edgePt1 );

	//
	// calculate the "sharp" new edge point
	//
	Vector vSharp;
	VectorClear( vSharp );
	VectorAdd( edgePt0, edgePt1, vSharp );
	VectorScale( vSharp, 0.5f, vSharp );

	//
	// calculate the "smooth" new edge point if necessary
	//
	Vector vSmooth;
	VectorClear( vSmooth );
	if( m_pQuads[1] && ( m_Sharpness != 1.0f ) )
	{
		Vector centroids[2];
		m_pQuads[0]->GetCentroid( centroids[0] );
		m_pQuads[1]->GetCentroid( centroids[1] );
		VectorAdd( edgePt0, edgePt1, vSmooth );
		VectorAdd( vSmooth, centroids[0], vSmooth );
		VectorAdd( vSmooth, centroids[1], vSmooth );
		VectorScale( vSmooth, 0.25f, vSmooth );
	}
	else
	{
		// make sure -- if here because of no neighboring quad
		m_Sharpness = 1.0f;
		m_pQuads[0]->CalcCentroid();
	}
			
	//
	// calculate the new edge point
	//
	// ( 1 - edge(sharpness) ) * vSmooth + edge(sharpness) * vSharp
	//
	VectorScale( vSmooth, ( 1.0f - m_Sharpness ), vSmooth );
	VectorScale( vSharp, m_Sharpness, vSharp );
	VectorAdd( vSmooth, vSharp, m_NewEdgePoint );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CompareSubdivEdges( const CSubdivEdge *pEdge1, const CSubdivEdge *pEdge2 )
{
	if( ( ( pEdge1->m_ndxPoint[0] == pEdge2->m_ndxPoint[0] ) && ( pEdge1->m_ndxPoint[1] == pEdge2->m_ndxPoint[1] ) ) ||
		( ( pEdge1->m_ndxPoint[0] == pEdge2->m_ndxPoint[1] ) && ( pEdge1->m_ndxPoint[1] == pEdge2->m_ndxPoint[0] ) ) )
		return true;

	return false;	
}


//=============================================================================
//
// Subdivision Quad Functions
//

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivQuad::GetCentroid( Vector& centroid )
{
	// get the subdivision mesh
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if( !pDoc )
		return;

	CSubdivMesh *pMesh = pDoc->GetSubdivMesh();

	VectorClear( centroid );
	for( int i = 0; i < 4; i++ )
	{
		Vector point;
		pMesh->GetPoint( m_ndxVert[i], point );
		VectorAdd( centroid, point, centroid );
	}

	VectorScale( centroid, 0.25f, centroid );

	// keep to surface creation
	m_Centroid = centroid;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivQuad::CalcCentroid( void )
{
	// get the subdivision mesh
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if( !pDoc )
		return;

	CSubdivMesh *pMesh = pDoc->GetSubdivMesh();

	VectorClear( m_Centroid );
	for( int i = 0; i < 4; i++ )
	{
		Vector point;
		pMesh->GetPoint( m_ndxVert[i], point );
		VectorAdd( m_Centroid, point, m_Centroid );
	}

	VectorScale( m_Centroid, 0.25f, m_Centroid );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivQuad::GetNormal( Vector& normal )
{
	// get the subdivision mesh
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if( !pDoc )
		return;

	CSubdivMesh *pMesh = pDoc->GetSubdivMesh();

	Vector points[3];
	Vector segs[2];

	pMesh->GetPoint( m_ndxVert[0], points[0] );
	pMesh->GetPoint( m_ndxVert[1], points[1] );
	pMesh->GetPoint( m_ndxVert[2], points[2] );

	VectorSubtract( points[1], points[0], segs[0] );
	VectorSubtract( points[2], points[0], segs[1] );

	CrossProduct( segs[1], segs[0], normal );
	VectorNormalize( normal );

	m_Normal = normal;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivQuad::CalcNormal( void )
{
	// get the subdivision mesh
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if( !pDoc )
		return;

	CSubdivMesh *pMesh = pDoc->GetSubdivMesh();

	Vector points[3];
	Vector segs[2];

	pMesh->GetPoint( m_ndxVert[0], points[0] );
	pMesh->GetPoint( m_ndxVert[1], points[1] );
	pMesh->GetPoint( m_ndxVert[2], points[2] );

	VectorSubtract( points[1], points[0], segs[0] );
	VectorSubtract( points[2], points[0], segs[1] );

	CrossProduct( segs[1], segs[0], m_Normal );
	VectorNormalize( m_Normal );
}


//=============================================================================
//
// Subdivision Mesh Functions
//

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CSubdivMesh::CSubdivMesh()
{
	Clear();
}

	
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CSubdivMesh::~CSubdivMesh()
{
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CSubdivMesh::AddPoint( const Vector& point, const Vector& normal )
{
	//
	// check for existing point within CSubdivPoints
	//
	for( int i = 0; i < m_PointCount; i++ )
	{
		if( CompareSubdivPointToPoint( &m_pPoints[i], point, 0.01f ) )
			return i;
	}

	if( m_PointCount >= m_MaxPointCount )
	{
		// error message!
		return -1;
	}

	m_pPoints[m_PointCount].m_Point = point;
	m_pPoints[m_PointCount].m_Normal = normal;
	m_PointCount++;

	return ( m_PointCount - 1 );
}

	
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivMesh::RemovePoint( Vector& point )
{
	//
	// find point in list (and remove it)
	//
	for( int i = 0; i < m_PointCount; i++ )
	{
		if( !CompareSubdivPointToPoint( &m_pPoints[i], point, 0.01f ) )
			continue;

		if( i == ( m_PointCount - 1 ) )
		{
			m_pPoints[i].Clear();
		}
		else
		{
			m_pPoints[i].Copy( &m_pPoints[m_PointCount-1] );
			m_pPoints[m_PointCount-1].Clear();
		}
		
		m_PointCount--;
	}
}

	
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CSubdivMesh::AddEdge( CSubdivEdge *edge )
{
	//
	// check for existing edge
	//
	for( int i = 0; i < m_EdgeCount; i++ )
	{
		if( CompareSubdivEdges( edge, &m_pEdges[i] ) )
		{
			//
			// check for "quads" on both sides of edge (add if necessary)
			//
			if( ( !m_pEdges[i].m_pQuads[1] ) && ( edge->m_pQuads[0] != m_pEdges[i].m_pQuads[0] ) )
			{
				m_pEdges[i].m_pQuads[1] = edge->m_pQuads[0];
				m_pEdges[i].m_ndxQuadEdge[1] = edge->m_ndxQuadEdge[0];
				m_pEdges[i].m_Sharpness = 0.0f;
			}

			return i;
		}
	}

	if( m_EdgeCount >= m_MaxEdgeCount )
	{
		// error message!
		return -1;
	}

	m_pEdges[m_EdgeCount].Copy( edge );
	m_pEdges[m_EdgeCount].m_Active = true;
	m_EdgeCount++;

	return ( m_EdgeCount - 1 );
}

	
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivMesh::RemoveEdge( CSubdivEdge *edge )
{
	return;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivMesh::CatmullClarkSubdivide( void )
{
	//
	// calculate the "new edge points"
	//
	for( int i = 0; i < m_EdgeCount; i++ )
	{
		m_pEdges[i].CalcNewEdgePoint();
		m_pEdges[i].CalcNewEdgeNormal();
	}

	//
	// if point index if part of edge, add to point edge list and increment valence
	//
	for( i = 0; i < m_PointCount; i++ )
	{
		for( int j = 0; j < m_EdgeCount; j++ )
		{
			if( !m_pEdges[j].m_Active )
				continue;

			if( ( i == m_pEdges[j].m_ndxPoint[0] ) || ( i == m_pEdges[j].m_ndxPoint[1] ) )
			{
				m_pPoints[i].m_pEdges[m_pPoints[i].m_Valence] = &m_pEdges[j];
				m_pPoints[i].m_Valence++;
			}
		}
	}

	//
	// determine the point's "type"
	//
	for( i = 0; i < m_PointCount; i++ )
	{
		//
		// get the number of sharp incident edges and neighbor data
		//
		int sharpnessCount = 0;
		int sharpnessThreshold = m_pPoints[i].m_Valence - 1;
		bool bHasNeighbors = false;

		for( int j = 0; j < m_pPoints[i].m_Valence; j++ )
		{
			if( m_pPoints[i].m_pEdges[j]->m_Sharpness > 0.0f )
			{
				sharpnessCount++;
			}

			if( m_pPoints[i].m_pEdges[j]->m_pQuads[1] )
			{
				bHasNeighbors = true;
			}
		}

		//
		// determine point type
		//
		if( ( sharpnessCount >= sharpnessThreshold ) || !bHasNeighbors )
//		if( ( sharpnessCount > 2 ) || !bHasNeighbors )
		{
			m_pPoints[i].m_Type = CSubdivPoint::POINT_CORNER;
			continue;
		}

		if( sharpnessCount > 1 )
//		if( sharpnessCount == 2 )
		{
			m_pPoints[i].m_Type = CSubdivPoint::POINT_CREASE;
			continue;
		}

		m_pPoints[i].m_Type = CSubdivPoint::POINT_ORDINARY;
	}

	//
	// calculate the new vertex point
	//
	for( i = 0; i < m_PointCount; i++ )
	{
		m_pPoints[i].CalcNewVertexPoint();
		m_pPoints[i].CalcNewVertexNormal();
	}

	//
	// move all new points to points
	//
	for( i = 0; i < m_PointCount; i++ )
	{
		m_pPoints[i].m_Point = m_pPoints[i].m_NewPoint;
		m_pPoints[i].m_Normal = m_pPoints[i].m_NewNormal;
		VectorClear( m_pPoints[i].m_NewPoint );
		VectorClear( m_pPoints[i].m_NewNormal );
		m_pPoints[i].m_Valence = 0;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CSubdivMesh::AddTree( CSubdivQuad *pTree )
{
	//
	// check to see if tree already exists in list
	//
	for( int i = 0; i < m_TreeCount; i++ )
	{
		if( pTree == m_ppTrees[i] )
			return i;
	}

	//
	// check tree count
	//
	if( m_TreeCount >= m_MaxTreeCount )
	{
		// error message
		_asm int 3;
		return -1;
	}

	//
	// add tree to list
	//
	m_ppTrees[m_TreeCount] = pTree;
	m_TreeCount++;

	return ( m_TreeCount - 1 );
}

static HCURSOR preSubdivCursor;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CSubdivMesh::PreSubdivide( void )
{
	// change the mouse to hourglass -- so level designers know something is
	// happening
	preSubdivCursor = SetCursor( LoadCursor( NULL, IDC_WAIT ) );

	// clear the mesh
	Clear();

	//
	// get the selection set
	//
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if( !pDoc )
		return false;

	CDispManager *pDispManager = pDoc->GetDispManager();
	if( !pDispManager )
		return false;

	// get number of displacements in selection
	int selectionCount = pDispManager->GetSelectionListCount();

	// allocate memory
	if( !AllocCache( selectionCount ) )
		return false;

	// mark the subdivision undo
	GetHistory()->MarkUndoPosition( NULL, "Subdivision" );
	
	//
	// add all surfaces to mesh to subdivide
	//
	for( int i = 0; i < selectionCount; i++ )
	{
		// get the current displacement surface
		CMapDisp *pDisp = pDispManager->GetFromSelectionList( i );
		if( !pDisp )
			continue;
			
		//
		// setup for undo
		//
		CMapFace *pFace = ( CMapFace* )pDisp->GetParent();
		CMapSolid *pSolid = ( CMapSolid* )pFace->GetParent();
		GetHistory()->Keep( ( CMapClass* )pSolid );

		//
		// add displacement's subdivision tree to mesh list
		//
		if( AddTree( pDisp->PreSubdivide( this ) ) == -1 )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivMesh::SetEdgeData( CSubdivQuad *pRoot, int index, int parentIndex, int subdivIndex )
{
	for( int i = 0; i < 4; i++ )
	{
		CSubdivEdge edge;
		
		//
		// add vert indices
		//
		edge.m_ndxPoint[0] = pRoot[index].m_ndxVert[i];
		edge.m_ndxPoint[1] = pRoot[index].m_ndxVert[(i+1)%4];
		
		//
		// set initial quads and edges data
		//
		edge.m_pQuads[0] = &pRoot[index];
		edge.m_pQuads[1] = NULL;
		
		edge.m_ndxQuadEdge[0] = i;
		edge.m_ndxQuadEdge[1] = -1;

		//
		// set edge sharpness
		//
		if( ( i == subdivIndex ) || ( i == ( (subdivIndex+3)%4 ) ) )
		{
			edge.m_Sharpness = m_pEdges[pRoot[parentIndex].m_ndxEdge[i]].m_Sharpness;
		}
		else
		{
			edge.m_Sharpness = 0.0f;
		}
	
		// add edge to global list
		pRoot[index].m_ndxEdge[i] = AddEdge( &edge );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivMesh::CreateChildQuad4( CSubdivQuad *pRoot, int index, int parentIndex )
{
	//
	// set quad indices -- displacement index values
	//
	pRoot[index].m_ndxQuad[0] = ( ( pRoot[parentIndex].m_ndxQuad[0] + pRoot[parentIndex].m_ndxQuad[3] ) / 2 );
	pRoot[index].m_ndxQuad[1] = ( ( pRoot[parentIndex].m_ndxQuad[0] + pRoot[parentIndex].m_ndxQuad[2] ) / 2 );
	pRoot[index].m_ndxQuad[2] = ( ( pRoot[parentIndex].m_ndxQuad[2] + pRoot[parentIndex].m_ndxQuad[3] ) / 2 );
	pRoot[index].m_ndxQuad[3] = pRoot[parentIndex].m_ndxQuad[3];

	//
	// set vert indices
	//
	pRoot[index].m_ndxVert[0] = AddPoint( m_pEdges[pRoot[parentIndex].m_ndxEdge[3]].m_NewEdgePoint, 
		                                  m_pEdges[pRoot[parentIndex].m_ndxEdge[3]].m_NewEdgeNormal );
	pRoot[index].m_ndxVert[1] = AddPoint( pRoot[parentIndex].m_Centroid, pRoot[parentIndex].m_Normal );
	pRoot[index].m_ndxVert[2] = AddPoint( m_pEdges[pRoot[parentIndex].m_ndxEdge[2]].m_NewEdgePoint,
		                                  m_pEdges[pRoot[parentIndex].m_ndxEdge[2]].m_NewEdgeNormal );
	pRoot[index].m_ndxVert[3] = pRoot[parentIndex].m_ndxVert[3];

	// set edge data
	SetEdgeData( pRoot, index, parentIndex, 3 );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivMesh::CreateChildQuad3( CSubdivQuad *pRoot, int index, int parentIndex )
{
	//
	// set quad indices -- displacement index values
	//
	pRoot[index].m_ndxQuad[0] = ( ( pRoot[parentIndex].m_ndxQuad[0] + pRoot[parentIndex].m_ndxQuad[2] ) / 2 );
	pRoot[index].m_ndxQuad[1] = ( ( pRoot[parentIndex].m_ndxQuad[1] + pRoot[parentIndex].m_ndxQuad[2] ) / 2 );
	pRoot[index].m_ndxQuad[2] = pRoot[parentIndex].m_ndxQuad[2];
	pRoot[index].m_ndxQuad[3] = ( ( pRoot[parentIndex].m_ndxQuad[2] + pRoot[parentIndex].m_ndxQuad[3] ) / 2 );

	//
	// set vert indices
	//
	pRoot[index].m_ndxVert[0] = AddPoint( pRoot[parentIndex].m_Centroid, pRoot[parentIndex].m_Normal );
	pRoot[index].m_ndxVert[1] = AddPoint( m_pEdges[pRoot[parentIndex].m_ndxEdge[1]].m_NewEdgePoint,
		                                  m_pEdges[pRoot[parentIndex].m_ndxEdge[1]].m_NewEdgeNormal );
	pRoot[index].m_ndxVert[2] = pRoot[parentIndex].m_ndxVert[2];
	pRoot[index].m_ndxVert[3] = AddPoint( m_pEdges[pRoot[parentIndex].m_ndxEdge[2]].m_NewEdgePoint,
		                                  m_pEdges[pRoot[parentIndex].m_ndxEdge[2]].m_NewEdgeNormal );

	// set edge data
	SetEdgeData( pRoot, index, parentIndex, 2 );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivMesh::CreateChildQuad2( CSubdivQuad *pRoot, int index, int parentIndex )
{
	//
	// set quad indices -- displacement index values
	//
	pRoot[index].m_ndxQuad[0] = ( ( pRoot[parentIndex].m_ndxQuad[0] + pRoot[parentIndex].m_ndxQuad[1] ) / 2 );
	pRoot[index].m_ndxQuad[1] = pRoot[parentIndex].m_ndxQuad[1];
	pRoot[index].m_ndxQuad[2] = ( ( pRoot[parentIndex].m_ndxQuad[1] + pRoot[parentIndex].m_ndxQuad[2] ) / 2 );
	pRoot[index].m_ndxQuad[3] = ( ( pRoot[parentIndex].m_ndxQuad[0] + pRoot[parentIndex].m_ndxQuad[2] ) / 2 );

	//
	// set vert indices
	//
	pRoot[index].m_ndxVert[0] = AddPoint( m_pEdges[pRoot[parentIndex].m_ndxEdge[0]].m_NewEdgePoint,
		                                  m_pEdges[pRoot[parentIndex].m_ndxEdge[0]].m_NewEdgeNormal );
	pRoot[index].m_ndxVert[1] = pRoot[parentIndex].m_ndxVert[1];
	pRoot[index].m_ndxVert[2] = AddPoint( m_pEdges[pRoot[parentIndex].m_ndxEdge[1]].m_NewEdgePoint,
		                                  m_pEdges[pRoot[parentIndex].m_ndxEdge[1]].m_NewEdgeNormal );
	pRoot[index].m_ndxVert[3] = AddPoint( pRoot[parentIndex].m_Centroid, pRoot[parentIndex].m_Normal );

	// set edge data
	SetEdgeData( pRoot, index, parentIndex, 1 );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivMesh::CreateChildQuad1( CSubdivQuad *pRoot, int index, int parentIndex )
{
	//
	// set quad indices -- displacement index values
	//
	pRoot[index].m_ndxQuad[0] = pRoot[parentIndex].m_ndxQuad[0];
	pRoot[index].m_ndxQuad[1] = ( ( pRoot[parentIndex].m_ndxQuad[0] + pRoot[parentIndex].m_ndxQuad[1] ) / 2 );
	pRoot[index].m_ndxQuad[2] = ( ( pRoot[parentIndex].m_ndxQuad[0] + pRoot[parentIndex].m_ndxQuad[2] ) / 2 );
	pRoot[index].m_ndxQuad[3] = ( ( pRoot[parentIndex].m_ndxQuad[0] + pRoot[parentIndex].m_ndxQuad[3] ) / 2 );

	//
	// set vert indices
	//
	pRoot[index].m_ndxVert[0] = pRoot[parentIndex].m_ndxVert[0];
	pRoot[index].m_ndxVert[1] = AddPoint( m_pEdges[pRoot[parentIndex].m_ndxEdge[0]].m_NewEdgePoint,
		                                  m_pEdges[pRoot[parentIndex].m_ndxEdge[0]].m_NewEdgeNormal );
	pRoot[index].m_ndxVert[2] = AddPoint( pRoot[parentIndex].m_Centroid, pRoot[parentIndex].m_Normal );
	pRoot[index].m_ndxVert[3] = AddPoint( m_pEdges[pRoot[parentIndex].m_ndxEdge[3]].m_NewEdgePoint,
		                                  m_pEdges[pRoot[parentIndex].m_ndxEdge[3]].m_NewEdgeNormal );

	// set edge data
	SetEdgeData( pRoot, index, parentIndex, 0 );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivMesh::CreateChildQuads( CSubdivQuad *pRoot, int quadIndex )
{
	//
	// create children
	//
	CreateChildQuad1( pRoot, ( ( quadIndex << 2 ) + 1 ), quadIndex );
	CreateChildQuad2( pRoot, ( ( quadIndex << 2 ) + 2 ), quadIndex );
	CreateChildQuad3( pRoot, ( ( quadIndex << 2 ) + 3 ), quadIndex );
	CreateChildQuad4( pRoot, ( ( quadIndex << 2 ) + 4 ), quadIndex );

	for( int i = 0; i < 4; i++ )
	{
		m_pEdges[pRoot[quadIndex].m_ndxEdge[i]].m_Active = false;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivMesh::AddQuadToMesh( CSubdivQuad *pQuad )
{
	return;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CSubdivMesh::GetEndIndexFromLevel( int levelIndex )
{
	switch( levelIndex )
	{
	case 0: { return 0; }
	case 1: { return 4; }
	case 2: { return 20; }
	case 3: { return 84; }
	default: { return 0; }
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CSubdivMesh::GetStartIndexFromLevel( int levelIndex )
{
	switch( levelIndex )
	{
	case 0: { return 0; }
	case 1: { return 1; }
	case 2: { return 5; }
	case 3: { return 21; }
	default: { return 0; }
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivMesh::Subdivide( void )
{
	//
	// subdivide to four levels always (what the trees hold)
	//
	for( int subdivLevel = 0; subdivLevel < 4; subdivLevel++ )
	{
		int startIndex = GetStartIndexFromLevel( subdivLevel );
		int endIndex = GetEndIndexFromLevel( subdivLevel );

		// subdivide
		CatmullClarkSubdivide();

		//
		// add subdivision data to subdivision tree
		//
		for( int treeIndex = 0; treeIndex < m_TreeCount; treeIndex++ )
		{
			//
			// get the current tree
			//
			CSubdivQuad *pTree = m_ppTrees[treeIndex];
			if( !pTree )
				continue;
			
			//
			// for each quad in the tree (at the given level)
			//
			for( int index = startIndex; index <= endIndex; index++ )
			{
				CreateChildQuads( pTree, index );			
			}
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivMesh::PostSubdivide( void )
{
	//
	// get the selection set
	//
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if( !pDoc )
		return;

	CDispManager *pDispManager = pDoc->GetDispManager();
	if( !pDispManager )
		return;

	//
	// add all surfaces to mesh to subdivide
	//
	int selectionCount = pDispManager->GetSelectionListCount();
	for( int i = 0; i < selectionCount; i++ )
	{
		// get the current displacement surface
		CMapDisp *pDisp = pDispManager->GetFromSelectionList( i );
		if( !pDisp )
			continue;

		// post subdivide
		pDisp->PostSubdivide( this );
	}

	// destroy cache!!!
	FreeCache();

	// set the cursor back to its previous state (before subdivision
	SetCursor( preSubdivCursor );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivMesh::DoSubdivide( void )
{
	PreSubdivide();
	Subdivide();
	PostSubdivide();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CSubdivMesh::AllocCache( int dispCount )
{
#define POINTS_PER_DISP		512
#define EDGES_PER_DISP		1024

	m_MaxPointCount = POINTS_PER_DISP * dispCount;
	m_MaxEdgeCount = EDGES_PER_DISP * dispCount;
	m_MaxTreeCount = dispCount;

	m_pPoints = new CSubdivPoint[m_MaxPointCount];
	m_pEdges = new CSubdivEdge[m_MaxEdgeCount];
	m_ppTrees = new CSubdivQuad*[m_MaxTreeCount];

	if( !m_pPoints || !m_pEdges || !m_ppTrees )
	{
		FreeCache();
		return false;
	}

	//
	// clear cache
	//
	for( int i = 0; i < m_MaxPointCount; i++ )
	{
		m_pPoints[i].Clear();
	}

	for( i = 0; i < m_MaxEdgeCount; i++ )
	{
		m_pEdges[i].Clear();
	}

	//
	// tell size of cache
	//
	int size = m_MaxPointCount * sizeof( CSubdivPoint );
	size += m_MaxEdgeCount * sizeof( CSubdivEdge );
	size += m_MaxTreeCount * sizeof( CSubdivQuad );

	TRACE1( "Subdiv Cache: %d\n", size );

	return true;

#undef POINTS_PER_DISP
#undef EDGES_PER_DISP
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSubdivMesh::FreeCache( void )
{
	if( m_pPoints )
	{
		delete [] m_pPoints;
		m_pPoints = NULL;
		m_PointCount = 0;
	}

	if( m_pEdges )
	{
		delete [] m_pEdges;
		m_pEdges = NULL;
		m_EdgeCount = 0;
	}

	if( m_ppTrees )
	{
		delete [] m_ppTrees;
		m_ppTrees = NULL;
		m_TreeCount = 0;
	}

	// tell cache destroyed!!
	TRACE0( "Subdiv Cache Destroyed!\n" );
}
