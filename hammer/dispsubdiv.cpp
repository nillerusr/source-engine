//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include <stdafx.h>
#include "DispSubdiv.h"
#include "MapDisp.h"
#include "UtlLinkedList.h"
#include "utlvector.h"
#include "GlobalFunctions.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//=============================================================================
//
// Editable Displacement Subdivision Mesh Implementation
//
class CEditDispSubdivMesh : public IEditDispSubdivMesh
{
public: // functions

	void Init( void );
	void Shutdown( void );

	void AddDispTo( CMapDisp *pDisp  );
	void GetDispFrom( CMapDisp *pDisp );

	void DoCatmullClarkSubdivision( void );

public: // typedefs, enums, structs

	enum { EDITDISP_QUADSIZE = 4 };			// should be in mapdisp (general define)

private: // typedefs, enums, structs

	typedef int SubdivPointHandle_t;
	typedef int SubdivEdgeHandle_t;
	typedef int SubdivQuadHandle_t;

	enum { NUM_SUBDIV_LEVELS = 4 };			// number of subdivision levels

	enum
	{
		SUBDIV_DISPPOINTS = 512,
		SUBDIV_DISPEDGES = 1024,
		SUBDIV_DISPQUADS = 512
	};

	enum
	{
		SUBDIV_POINTORDINARY = 0,
		SUBDIV_POINTCORNER = 1,
		SUBDIV_POINTCREASE = 2
	};

	struct SubdivPoint_t
	{
		Vector				m_vPoint;
		Vector				m_vNormal;
		Vector				m_vNewPoint;
		Vector				m_vNewNormal;
		unsigned short		m_uType;
		unsigned short		m_uValence;
		SubdivEdgeHandle_t	m_EdgeHandles[EDITDISP_QUADSIZE*4];
	};

	struct SubdivEdge_t
	{
		Vector				m_vNewEdgePoint;
		Vector				m_vNewEdgeNormal;
		SubdivPointHandle_t	m_PointHandles[2];
		SubdivQuadHandle_t	m_QuadHandles[2];
		float				m_flSharpness;
		bool				m_bActive;
	};

	struct SubdivQuad_t
	{
		// generated
		Vector				m_vCentroid;						// quad center
		Vector				m_vNormal;							// quad normal
		
		// linkage
		SubdivQuadHandle_t	m_ndxParent;						// parent quad index
		SubdivQuadHandle_t	m_ndxChild[EDITDISP_QUADSIZE];		// chilren (4 of them) indices
		
		// quad data
		SubdivPointHandle_t	m_PointHandles[EDITDISP_QUADSIZE];	// point indices - unique list
		SubdivEdgeHandle_t	m_EdgeHandles[EDITDISP_QUADSIZE];	// edge indices - unique list

		// disp/quad mapping
		EditDispHandle_t	m_EditDispHandle;
		short				m_Level;							// level of quad in the hierarchy (tree)
		short				m_QuadIndices[EDITDISP_QUADSIZE];	// quad indices (in the X x X displacement surface)
	};

private: // functions

	SubdivPoint_t *GetPoint( SubdivPointHandle_t ptHandle );
	SubdivEdge_t *GetEdge( SubdivEdgeHandle_t edgeHandle );
	SubdivQuad_t *GetQuad( SubdivQuadHandle_t quadHandle );

	void Point_Init( SubdivPointHandle_t ptHandle );
	void Point_CalcNewPoint( SubdivPointHandle_t ptHandle );
	void Point_PointOrdinary( SubdivPoint_t *pPoint );
	void Point_PointCorner( SubdivPoint_t *pPoint );
	void Point_PointCrease( SubdivPoint_t *pPoint );

	void Edge_Init( SubdivEdgeHandle_t edgeHandle );
	void Edge_CalcNewPoint( SubdivEdgeHandle_t edgeHandle );

	void Quad_Init( SubdivQuadHandle_t quadHandle );
	void Quad_CalcCentroid( SubdivQuadHandle_t quadHandle );
	void Quad_CalcNormal( SubdivQuadHandle_t quadHandle );

	bool CompareSubdivPoints( Vector const &pt1, Vector const &pt2, float flTolerance );
	bool CompareSubdivEdges( SubdivPointHandle_t ptEdge0Handle0, SubdivPointHandle_t ptEdge0Handle1,
							 SubdivPointHandle_t ptEdge1Handle0, SubdivPointHandle_t ptEdge1Handle1 );

	SubdivPointHandle_t BuildSubdivPoint( Vector const &vPoint, Vector const &vNormal );
	SubdivEdgeHandle_t BuildSubdivEdge( int ndxEdge, SubdivQuadHandle_t quadHandle,
										SubdivQuadHandle_t parentHandle, int ndxChild );
	SubdivQuadHandle_t BuildSubdivQuad( int ndxChild, SubdivQuadHandle_t parentHandle );

	void CatmullClarkSubdivision( void );
	void UpdateSubdivisionHierarchy( int ndxLevel );

private: // variables

	CUtlLinkedList<SubdivPoint_t, SubdivPointHandle_t>	m_Points;
	CUtlLinkedList<SubdivEdge_t, SubdivEdgeHandle_t>	m_Edges;
	CUtlLinkedList<SubdivQuad_t, SubdivQuadHandle_t>	m_Quads;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IEditDispSubdivMesh *CreateEditDispSubdivMesh( void )
{
	return new CEditDispSubdivMesh;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void DestroyEditDispSubdivMesh( IEditDispSubdivMesh **pSubdivMesh )
{
	if ( *pSubdivMesh )
	{
		delete *pSubdivMesh;
		*pSubdivMesh = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEditDispSubdivMesh::SubdivPoint_t *CEditDispSubdivMesh::GetPoint( SubdivPointHandle_t ptHandle )
{
	if ( !m_Points.IsValidIndex( ptHandle ) )
		return NULL;

	return &m_Points.Element( ptHandle );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEditDispSubdivMesh::SubdivEdge_t *CEditDispSubdivMesh::GetEdge( SubdivEdgeHandle_t edgeHandle )
{
	if ( !m_Edges.IsValidIndex( edgeHandle ) )
		return NULL;

	return &m_Edges.Element( edgeHandle );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEditDispSubdivMesh::SubdivQuad_t *CEditDispSubdivMesh::GetQuad( SubdivQuadHandle_t quadHandle )
{
	if ( !m_Quads.IsValidIndex( quadHandle ) )
		return NULL;

	return &m_Quads.Element( quadHandle );
}


//=============================================================================
//
// Subdivision Edit Displacement Point Functions 
//

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::Point_Init( SubdivPointHandle_t ptHandle )
{
	SubdivPoint_t *pPoint = GetPoint( ptHandle );
	if ( pPoint )
	{
		VectorClear( pPoint->m_vPoint );
		VectorClear( pPoint->m_vNormal );
		VectorClear( pPoint->m_vNewPoint );
		VectorClear( pPoint->m_vNewNormal );
		
		pPoint->m_uType = (unsigned short)-1;
		pPoint->m_uValence = 0;

		for ( int ndxEdge = 0; ndxEdge < ( EDITDISP_QUADSIZE*2 ); ndxEdge++ )
		{
			pPoint->m_EdgeHandles[ndxEdge] = m_Edges.InvalidIndex();
		}			
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::Point_CalcNewPoint( SubdivPointHandle_t ptHandle )
{
	// get the point to act on
	SubdivPoint_t *pPoint = GetPoint( ptHandle );
	if ( !pPoint )
		return;

	switch ( pPoint->m_uType )
	{
	case SUBDIV_POINTORDINARY: { Point_PointOrdinary( pPoint ); break; }
	case SUBDIV_POINTCORNER: { Point_PointCorner( pPoint ); break; }
	case SUBDIV_POINTCREASE: { Point_PointCrease( pPoint ); break; }
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::Point_PointOrdinary( SubdivPoint_t *pPoint )
{
	//
	// accumulate the edge data and multiply by the valence (coincident edge)
	// ratio (squared)
	//
	Vector edgeAccumPoint( 0.0f, 0.0f, 0.0f );
	Vector edgeAccumNormal( 0.0f, 0.0f, 0.0f );
	for ( int ndxEdge = 0; ndxEdge < pPoint->m_uValence; ndxEdge++ )
	{
		SubdivEdge_t *pEdge = GetEdge( pPoint->m_EdgeHandles[ndxEdge] );
		if ( pEdge )
		{
			VectorAdd( edgeAccumPoint, pEdge->m_vNewEdgePoint, edgeAccumPoint );
			VectorAdd( edgeAccumNormal, pEdge->m_vNewEdgeNormal, edgeAccumNormal );
		}
	}

	float ratio = 1.0f / ( float )( pPoint->m_uValence * pPoint->m_uValence );

	VectorScale( edgeAccumPoint, ratio, edgeAccumPoint );
	VectorScale( edgeAccumNormal, ratio, edgeAccumNormal );

	//
	// accumlate the centroid data from all neighboring quads and multiply by
	// the valence (coincident edge) ratio (squared)
	//
	int					quadListCount = 0;
	SubdivQuadHandle_t	quadList[32];

	for ( int ndxEdge = 0; ndxEdge < pPoint->m_uValence; ndxEdge++ )
	{
		SubdivEdge_t *pEdge = GetEdge( pPoint->m_EdgeHandles[ndxEdge] );
		if ( pEdge )
		{
			for ( int ndxQuad = 0; ndxQuad < 2; ndxQuad++ )
			{
				if ( pEdge->m_QuadHandles[ndxQuad] != m_Quads.InvalidIndex() )
				{
					int ndxList;
					for ( ndxList = 0; ndxList < quadListCount; ndxList++ )
					{
						if( pEdge->m_QuadHandles[ndxQuad] == quadList[ndxList] )
							break;
					}

					if( ndxList == quadListCount )
					{
						quadList[quadListCount] = pEdge->m_QuadHandles[ndxQuad];
						quadListCount++;
					}
				}
			}
		}
	}

	Vector centroidAccum( 0.0f, 0.0f, 0.0f );
	for ( int ndxQuad = 0; ndxQuad < quadListCount; ndxQuad++ )
	{
		SubdivQuadHandle_t quadHandle = quadList[ndxQuad];
		Quad_CalcCentroid( quadHandle );
		SubdivQuad_t *pQuad = GetQuad( quadHandle );
		VectorAdd( centroidAccum, pQuad->m_vCentroid, centroidAccum );
	}

	VectorScale( centroidAccum, ratio, centroidAccum );

	//
	// 
	//
	ratio = ( ( float )pPoint->m_uValence - 2.0f ) / ( float )pPoint->m_uValence;

	VectorScale( pPoint->m_vPoint, ratio, pPoint->m_vNewPoint );
	VectorAdd( pPoint->m_vNewPoint, edgeAccumPoint, pPoint->m_vNewPoint );
	VectorAdd( pPoint->m_vNewPoint, centroidAccum, pPoint->m_vNewPoint );

	VectorScale( pPoint->m_vNormal, ratio, pPoint->m_vNewNormal );
	VectorAdd( pPoint->m_vNewNormal, edgeAccumNormal, pPoint->m_vNewNormal );
	VectorAdd( pPoint->m_vNewNormal, centroidAccum, pPoint->m_vNewNormal );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::Point_PointCorner( SubdivPoint_t *pPoint )
{
	VectorCopy( pPoint->m_vPoint, pPoint->m_vNewPoint );
	VectorCopy( pPoint->m_vNormal, pPoint->m_vNewNormal );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::Point_PointCrease( SubdivPoint_t *pPoint )
{
	//
	// accumulate the edge data and multiply by the valence (coincident edge)
	// ratio (squared)
	//
	Vector edgeAccumPoint( 0.0f, 0.0f, 0.0f );
	Vector edgeAccumNormal( 0.0f, 0.0f, 0.0f );
	for ( int ndxEdge = 0; ndxEdge < pPoint->m_uValence; ndxEdge++ )
	{
		SubdivEdge_t *pEdge = GetEdge( pPoint->m_EdgeHandles[ndxEdge] );
		if ( pEdge && ( pEdge->m_flSharpness > 0.0f ) )
		{
			VectorAdd( edgeAccumPoint, pEdge->m_vNewEdgePoint, edgeAccumPoint );
			VectorAdd( edgeAccumNormal, pEdge->m_vNewEdgeNormal, edgeAccumNormal );
		}
	}

	//
	// 
	//
	VectorScale( pPoint->m_vPoint, 6.0f, pPoint->m_vNewPoint );
	VectorAdd( pPoint->m_vNewPoint, edgeAccumPoint, pPoint->m_vNewPoint );
	VectorScale( pPoint->m_vNewPoint, 0.125f, pPoint->m_vNewPoint );

	VectorScale( pPoint->m_vNormal, 6.0f, pPoint->m_vNewNormal );
	VectorAdd( pPoint->m_vNewNormal, edgeAccumNormal, pPoint->m_vNewNormal );
	VectorScale( pPoint->m_vNewNormal, 0.125f, pPoint->m_vNewNormal );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::Edge_Init( SubdivEdgeHandle_t edgeHandle )
{
	SubdivEdge_t *pEdge = GetEdge( edgeHandle );
	if ( pEdge )
	{
		VectorClear( pEdge->m_vNewEdgePoint );
		VectorClear( pEdge->m_vNewEdgeNormal );

		pEdge->m_flSharpness = 1.0f;
		pEdge->m_bActive = false;

		for ( int ndx = 0; ndx < 2; ndx++ )
		{
			pEdge->m_PointHandles[ndx] = m_Points.InvalidIndex();
			pEdge->m_QuadHandles[ndx] = m_Quads.InvalidIndex();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::Edge_CalcNewPoint( SubdivEdgeHandle_t edgeHandle )
{
	SubdivEdge_t *pEdge = GetEdge( edgeHandle );
	if ( !pEdge )
		return;

	if ( !pEdge->m_bActive )
		return;

	//
	// get edge points
	//
	SubdivPoint_t *pPoint0 = GetPoint( pEdge->m_PointHandles[0] );
	SubdivPoint_t *pPoint1 = GetPoint( pEdge->m_PointHandles[1] );
	if ( !pPoint0 || !pPoint1 )
		return;

	//
	// calculate the "sharp" new edge point
	//
	Vector vSharpPoint( 0.0f, 0.0f, 0.0f );
	VectorAdd( pPoint0->m_vPoint, pPoint1->m_vPoint, vSharpPoint );
	VectorScale( vSharpPoint, 0.5f, vSharpPoint );

	Vector vSharpNormal( 0.0f, 0.0f, 0.0f );
	VectorAdd( pPoint0->m_vNormal, pPoint1->m_vNormal, vSharpNormal );
	VectorNormalize( vSharpNormal );

	//
	// calculate the "smooth" new edge point (if necessary)
	//
	Vector vSmoothPoint( 0.0f, 0.0f, 0.0f );
	Vector vSmoothNormal( 0.0f, 0.0f, 0.0f );
	if ( ( pEdge->m_QuadHandles[1] != m_Edges.InvalidIndex() ) && ( pEdge->m_flSharpness != 1.0f ) )
	{
		Quad_CalcCentroid( pEdge->m_QuadHandles[0] );
		Quad_CalcCentroid( pEdge->m_QuadHandles[1] );
		Quad_CalcNormal( pEdge->m_QuadHandles[0] );
		Quad_CalcNormal( pEdge->m_QuadHandles[1] );
		SubdivQuad_t *pQuad0 = GetQuad( pEdge->m_QuadHandles[0] );
		SubdivQuad_t *pQuad1 = GetQuad( pEdge->m_QuadHandles[1] );

		VectorAdd( pPoint0->m_vPoint, pPoint1->m_vPoint, vSmoothPoint );
		VectorAdd( vSmoothPoint, pQuad0->m_vCentroid, vSmoothPoint );
		VectorAdd( vSmoothPoint, pQuad1->m_vCentroid, vSmoothPoint );
		VectorScale( vSmoothPoint, 0.25f, vSmoothPoint );

		VectorAdd( pPoint0->m_vNormal, pPoint1->m_vNormal, vSmoothNormal );
		VectorAdd( vSmoothNormal, pQuad0->m_vNormal, vSmoothNormal );
		VectorAdd( vSmoothNormal, pQuad1->m_vNormal, vSmoothNormal );
		VectorNormalize( vSmoothNormal );
	}
	else
	{
		pEdge->m_flSharpness = 1.0f;
		Quad_CalcCentroid( pEdge->m_QuadHandles[0] );
		Quad_CalcNormal( pEdge->m_QuadHandles[0] );
	}

	//
	// calculate the new edge point
	//
	// ( 1 - edge(sharpness) ) * vSmooth + edge(sharpness) * vSharp
	//
	VectorScale( vSmoothPoint, ( 1.0f - pEdge->m_flSharpness ), vSmoothPoint );
	VectorScale( vSharpPoint, pEdge->m_flSharpness, vSharpPoint );
	VectorAdd( vSmoothPoint, vSharpPoint, pEdge->m_vNewEdgePoint );

	VectorScale( vSmoothNormal, ( 1.0f - pEdge->m_flSharpness ), vSmoothNormal );
	VectorScale( vSharpNormal, pEdge->m_flSharpness, vSharpNormal );
	VectorAdd( vSmoothNormal, vSharpNormal, pEdge->m_vNewEdgeNormal );
	VectorNormalize( pEdge->m_vNewEdgeNormal );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::Quad_Init( SubdivQuadHandle_t quadHandle )
{
	SubdivQuad_t *pQuad = GetQuad( quadHandle );
	if ( pQuad )
	{
		VectorClear( pQuad->m_vCentroid );
		VectorClear( pQuad->m_vNormal );

		pQuad->m_ndxParent = m_Quads.InvalidIndex();
		pQuad->m_EditDispHandle = EDITDISPHANDLE_INVALID;
		pQuad->m_Level = -1;

		for ( int ndx = 0; ndx < EDITDISP_QUADSIZE; ndx++ )
		{
			pQuad->m_ndxChild[ndx] = m_Quads.InvalidIndex();
			
			pQuad->m_PointHandles[ndx] = m_Points.InvalidIndex();
			pQuad->m_EdgeHandles[ndx] = m_Edges.InvalidIndex();
			pQuad->m_QuadIndices[ndx] = -1;			
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::Quad_CalcCentroid( SubdivQuadHandle_t quadHandle )
{
	SubdivQuad_t *pQuad = GetQuad( quadHandle );
	if ( pQuad )
	{
		VectorClear( pQuad->m_vCentroid );
		for ( int ndxPt = 0; ndxPt < EDITDISP_QUADSIZE; ndxPt++ )
		{
			SubdivPoint_t *pPoint = GetPoint( pQuad->m_PointHandles[ndxPt] );
			VectorAdd( pQuad->m_vCentroid, pPoint->m_vPoint, pQuad->m_vCentroid );
		}

		VectorScale( pQuad->m_vCentroid, 0.25f, pQuad->m_vCentroid );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::Quad_CalcNormal( SubdivQuadHandle_t quadHandle )
{
	SubdivQuad_t *pQuad = GetQuad( quadHandle );
	if ( pQuad )
	{
		SubdivPoint_t *pPoints[3];
		Vector edges[2];

		pPoints[0] = GetPoint( pQuad->m_PointHandles[0] );
		pPoints[1] = GetPoint( pQuad->m_PointHandles[1] );
		pPoints[2] = GetPoint( pQuad->m_PointHandles[2] );

		VectorSubtract( pPoints[1]->m_vPoint, pPoints[0]->m_vPoint, edges[0] );
		VectorSubtract( pPoints[2]->m_vPoint, pPoints[0]->m_vPoint, edges[1] );

		CrossProduct( edges[1], edges[0], pQuad->m_vNormal );
		VectorNormalize( pQuad->m_vNormal );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEditDispSubdivMesh::CompareSubdivPoints( Vector const &pt1, Vector const &pt2, 
											   float flTolerance )
{
	for ( int axis = 0 ; axis < 3 ; axis++ )
	{
		if ( fabs( pt1[axis] - pt2[axis] ) > flTolerance )
			return false;
	}
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEditDispSubdivMesh::CompareSubdivEdges( SubdivPointHandle_t ptEdge0Handle0,
											  SubdivPointHandle_t ptEdge0Handle1,
											  SubdivPointHandle_t ptEdge1Handle0,
											  SubdivPointHandle_t ptEdge1Handle1 )
{
	if ( ( ( ptEdge0Handle0 == ptEdge1Handle0 ) && ( ptEdge0Handle1 == ptEdge1Handle1 ) ) ||
		 ( ( ptEdge0Handle0 == ptEdge1Handle1 ) && ( ptEdge0Handle1 == ptEdge1Handle0 ) ) )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEditDispSubdivMesh::SubdivPointHandle_t CEditDispSubdivMesh::BuildSubdivPoint( Vector const &vPoint,
														                        Vector const &vPointNormal )
{
	//
	// build a "unique" point
	//
	SubdivPointHandle_t ptHandle;
	for ( ptHandle = m_Points.Head(); ptHandle != m_Points.InvalidIndex();
	      ptHandle = m_Points.Next( ptHandle ) )
	{
		SubdivPoint_t *pPoint = GetPoint( ptHandle );
		if ( pPoint )
		{
			// compare (positions)
			if ( CompareSubdivPoints( vPoint, pPoint->m_vPoint, 0.1f ) )
				return ptHandle;
		}
	}

	ptHandle = m_Points.AddToTail();	
	Point_Init( ptHandle );
	SubdivPoint_t *pPoint = GetPoint( ptHandle );
	VectorCopy( vPoint, pPoint->m_vPoint );
	VectorCopy( vPointNormal, pPoint->m_vNormal );

	return ptHandle;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEditDispSubdivMesh::SubdivEdgeHandle_t CEditDispSubdivMesh::BuildSubdivEdge( int ndxEdge, SubdivQuadHandle_t quadHandle,
														                      SubdivQuadHandle_t parentHandle, int ndxChild )
{
	// get the quad
	SubdivQuad_t *pQuad = GetQuad( quadHandle );
	if ( !pQuad )
		return m_Edges.InvalidIndex();

	//
	// define a unique edge (m_PointHandlesX2, m_QuadHandle)
	//
	SubdivEdgeHandle_t edgeHandle;
	for ( edgeHandle = m_Edges.Head(); edgeHandle != m_Edges.InvalidIndex();
	      edgeHandle = m_Edges.Next( edgeHandle ) )
	{
		SubdivEdge_t *pEdge = GetEdge( edgeHandle );
		if ( pEdge )
		{
			// compare (point handles)
			if ( CompareSubdivEdges( pQuad->m_PointHandles[ndxEdge], pQuad->m_PointHandles[(ndxEdge+1)%4],
				                     pEdge->m_PointHandles[0], pEdge->m_PointHandles[1] ) )
			{
				// check to see if the quad is quad 0 or 1 (or if it needs to be quad 1)
				if ( ( pEdge->m_QuadHandles[0] != quadHandle ) && 
				 	 ( pEdge->m_QuadHandles[1] == m_Quads.InvalidIndex() ) )
				{
					pEdge->m_QuadHandles[1] = quadHandle;
					pEdge->m_flSharpness = 0.0f;			// smooth edge (between two subdiv quads)
				}

				return edgeHandle;
			}
		}
	}

	edgeHandle = m_Edges.AddToTail();
	Edge_Init( edgeHandle );
	SubdivEdge_t *pEdge = GetEdge( edgeHandle );

	pEdge->m_PointHandles[0] = pQuad->m_PointHandles[ndxEdge];
	pEdge->m_PointHandles[1] = pQuad->m_PointHandles[(ndxEdge+1)%4];
	pEdge->m_QuadHandles[0] = quadHandle;
	pEdge->m_bActive = true;

	// extra data for children (get edge sharpness from parent or 
	// it may be an internal edge and its sharpness will be 0)
	if( ndxChild != -1 )
	{
		if ( ( ndxEdge == ndxChild ) || ( ndxEdge == ( (ndxChild+3)%4 ) ) )
		{
			SubdivQuad_t *pParentQuad = GetQuad( parentHandle );
			if ( pParentQuad )
			{
				SubdivEdge_t *pParentEdge = GetEdge( pParentQuad->m_EdgeHandles[ndxEdge] );
				pEdge->m_flSharpness = pParentEdge->m_flSharpness;
			}
		}
		else
		{
			pEdge->m_flSharpness = 0.0f;
		}
	}

	return edgeHandle;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEditDispSubdivMesh::SubdivQuadHandle_t CEditDispSubdivMesh::BuildSubdivQuad( int ndxChild, 
																			  SubdivQuadHandle_t parentHandle )
{
	// get parent quad
	SubdivQuad_t *pParentQuad = GetQuad( parentHandle );
	if( !pParentQuad )
		return m_Quads.InvalidIndex();

	// allocate a new quad
	SubdivQuadHandle_t quadHandle = m_Quads.AddToTail();
	Quad_Init( quadHandle );
	SubdivQuad_t *pQuad = GetQuad( quadHandle );
	pQuad->m_ndxParent = parentHandle;
	pQuad->m_EditDispHandle = pParentQuad->m_EditDispHandle;
	pQuad->m_Level = pParentQuad->m_Level + 1;

	switch ( ndxChild )
	{
	case 0:
		{
			// displacement quad indices
			pQuad->m_QuadIndices[0] = pParentQuad->m_QuadIndices[0];
			pQuad->m_QuadIndices[1] = ( pParentQuad->m_QuadIndices[0] + pParentQuad->m_QuadIndices[1] ) * 0.5f;
			pQuad->m_QuadIndices[2] = ( pParentQuad->m_QuadIndices[0] + pParentQuad->m_QuadIndices[2] ) * 0.5f;
			pQuad->m_QuadIndices[3] = ( pParentQuad->m_QuadIndices[0] + pParentQuad->m_QuadIndices[3] ) * 0.5f;

			// new verts
			SubdivEdge_t *pEdge0 = GetEdge( pParentQuad->m_EdgeHandles[0] );
			SubdivEdge_t *pEdge3 = GetEdge( pParentQuad->m_EdgeHandles[3] );
			if ( pEdge0 && pEdge3 )
			{
				pQuad->m_PointHandles[0] = pParentQuad->m_PointHandles[0];
				pQuad->m_PointHandles[1] = BuildSubdivPoint( pEdge0->m_vNewEdgePoint, pEdge0->m_vNewEdgeNormal );
				pQuad->m_PointHandles[2] = BuildSubdivPoint( pParentQuad->m_vCentroid, pParentQuad->m_vNormal );
				pQuad->m_PointHandles[3] = BuildSubdivPoint( pEdge3->m_vNewEdgePoint, pEdge3->m_vNewEdgeNormal );
			}

			break;
		}
	case 1:
		{
			// displacement quad indices
			pQuad->m_QuadIndices[0] = ( pParentQuad->m_QuadIndices[0] + pParentQuad->m_QuadIndices[1] ) * 0.5f;
			pQuad->m_QuadIndices[1] = pParentQuad->m_QuadIndices[1];
			pQuad->m_QuadIndices[2] = ( pParentQuad->m_QuadIndices[1] + pParentQuad->m_QuadIndices[2] ) * 0.5f;
			pQuad->m_QuadIndices[3] = ( pParentQuad->m_QuadIndices[0] + pParentQuad->m_QuadIndices[2] ) * 0.5f;

			// new verts
			SubdivEdge_t *pEdge0 = GetEdge( pParentQuad->m_EdgeHandles[0] );
			SubdivEdge_t *pEdge1 = GetEdge( pParentQuad->m_EdgeHandles[1] );
			if ( pEdge0 && pEdge1 )
			{
				pQuad->m_PointHandles[0] = BuildSubdivPoint( pEdge0->m_vNewEdgePoint, pEdge0->m_vNewEdgeNormal );
				pQuad->m_PointHandles[1] = pParentQuad->m_PointHandles[1];
				pQuad->m_PointHandles[2] = BuildSubdivPoint( pEdge1->m_vNewEdgePoint, pEdge1->m_vNewEdgeNormal );
				pQuad->m_PointHandles[3] = BuildSubdivPoint( pParentQuad->m_vCentroid, pParentQuad->m_vNormal );
			}

			break;
		}
	case 2:
		{
			// displacement quad indices
			pQuad->m_QuadIndices[0] = ( pParentQuad->m_QuadIndices[0] + pParentQuad->m_QuadIndices[2] ) * 0.5f;
			pQuad->m_QuadIndices[1] = ( pParentQuad->m_QuadIndices[1] + pParentQuad->m_QuadIndices[2] ) * 0.5f;
			pQuad->m_QuadIndices[2] = pParentQuad->m_QuadIndices[2];
			pQuad->m_QuadIndices[3] = ( pParentQuad->m_QuadIndices[2] + pParentQuad->m_QuadIndices[3] ) * 0.5f;

			// new verts
			SubdivEdge_t *pEdge1 = GetEdge( pParentQuad->m_EdgeHandles[1] );
			SubdivEdge_t *pEdge2 = GetEdge( pParentQuad->m_EdgeHandles[2] );
			if ( pEdge1 && pEdge2 )
			{
				pQuad->m_PointHandles[0] = BuildSubdivPoint( pParentQuad->m_vCentroid, pParentQuad->m_vNormal );
				pQuad->m_PointHandles[1] = BuildSubdivPoint( pEdge1->m_vNewEdgePoint, pEdge1->m_vNewEdgeNormal );
				pQuad->m_PointHandles[2] = pParentQuad->m_PointHandles[2];
				pQuad->m_PointHandles[3] = BuildSubdivPoint( pEdge2->m_vNewEdgePoint, pEdge2->m_vNewEdgeNormal );
			}

			break;
		}
	case 3:
		{
			// displacement quad indices
			pQuad->m_QuadIndices[0] = ( pParentQuad->m_QuadIndices[0] + pParentQuad->m_QuadIndices[3] ) * 0.5f;
			pQuad->m_QuadIndices[1] = ( pParentQuad->m_QuadIndices[0] + pParentQuad->m_QuadIndices[2] ) * 0.5f;
			pQuad->m_QuadIndices[2] = ( pParentQuad->m_QuadIndices[2] + pParentQuad->m_QuadIndices[3] ) * 0.5f;
			pQuad->m_QuadIndices[3] = pParentQuad->m_QuadIndices[3];

			// new verts
			SubdivEdge_t *pEdge2 = GetEdge( pParentQuad->m_EdgeHandles[2] );
			SubdivEdge_t *pEdge3 = GetEdge( pParentQuad->m_EdgeHandles[3] );
			if ( pEdge2 && pEdge3 )
			{
				pQuad->m_PointHandles[0] = BuildSubdivPoint( pEdge3->m_vNewEdgePoint, pEdge3->m_vNewEdgeNormal );
				pQuad->m_PointHandles[1] = BuildSubdivPoint( pParentQuad->m_vCentroid, pParentQuad->m_vNormal );
				pQuad->m_PointHandles[2] = BuildSubdivPoint( pEdge2->m_vNewEdgePoint, pEdge2->m_vNewEdgeNormal );
				pQuad->m_PointHandles[3] = pParentQuad->m_PointHandles[3];
			}

			break;
		}
	}

	//
	// buidl new quad edges
	//
	for ( int ndxEdge = 0; ndxEdge < 4; ndxEdge++ )
	{
		pQuad->m_EdgeHandles[ndxEdge] = BuildSubdivEdge( ndxEdge, quadHandle, parentHandle, ndxChild );
	}

	return quadHandle;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::Init( void )
{
	// ensure capacity on all lists
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
		return;

	int selectCount = pDispMgr->SelectCount();	
	m_Points.EnsureCapacity( SUBDIV_DISPPOINTS * selectCount );
	m_Edges.EnsureCapacity( SUBDIV_DISPEDGES * selectCount );
	m_Quads.EnsureCapacity( SUBDIV_DISPQUADS * selectCount );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::Shutdown( void )
{
	// clear all lists
	m_Points.Purge();
	m_Edges.Purge();
	m_Quads.Purge();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::AddDispTo( CMapDisp *pDisp )
{
	// add a quad to the subdivision mesh
	SubdivQuadHandle_t quadHandle = m_Quads.AddToTail();
	Quad_Init( quadHandle );
	SubdivQuad_t *pQuad = &m_Quads.Element( quadHandle );

	// this is the parent!
	pQuad->m_ndxParent = m_Quads.InvalidIndex();
	pQuad->m_EditDispHandle = pDisp->GetEditHandle();
	pQuad->m_Level = 0;

	//
	// get displacement data
	//
	int dispWidth = pDisp->GetWidth();
	int dispHeight = pDisp->GetHeight();

	//
	// setup mapping between the displacement size and initial quad indices
	//
	pQuad->m_QuadIndices[0] = 0;
	pQuad->m_QuadIndices[1] = dispWidth * ( dispHeight - 1 );
	pQuad->m_QuadIndices[2] = ( dispWidth * dispHeight ) - 1;
	pQuad->m_QuadIndices[3] = ( dispWidth - 1 );

	//
	// find point normals and neighbors -- "smooth"
	// NOTE: this is slow -- should write a faster version (is offline process, do later)
	//
	IWorldEditDispMgr *pDispMgr = GetActiveWorldEditDispManager();
	if( !pDispMgr )
		return;

	Vector vPoints[4];
	Vector vPointNormals[4];
	for( int ndxPt = 0; ndxPt < EDITDISP_QUADSIZE; ndxPt++ )
	{
		// get the base face normal of all surfaces touching this point!
		pDisp->GetSurfNormal( vPointNormals[ndxPt] );

		// get the point to compare to neighbors
		pDisp->GetSurfPoint( ndxPt, vPoints[ndxPt] );

		int count = pDispMgr->SelectCount();
		for( int ndxSelect = 0; ndxSelect < count; ndxSelect++ )
		{
			CMapDisp *pSelectDisp = pDispMgr->GetFromSelect( ndxSelect );
			if( !pSelectDisp || ( pSelectDisp == pDisp ) )
				continue;

			for( int ndxPt2 = 0; ndxPt2 < EDITDISP_QUADSIZE; ndxPt2++ )
			{
				Vector vPoint;
				pSelectDisp->GetSurfPoint( ndxPt2, vPoint );
				
				if( CompareSubdivPoints( vPoints[ndxPt], vPoint, 0.01f ) )
				{
					Vector vNormal;
					pSelectDisp->GetSurfNormal( vNormal );
					VectorAdd( vPointNormals[ndxPt], vNormal, vPointNormals[ndxPt] );
				}
			}
		}

		VectorNormalize( vPointNormals[ndxPt] );
	}

	// build subdivision points
	for( int ndxPt = 0; ndxPt < EDITDISP_QUADSIZE; ndxPt++ )
	{
		pQuad->m_PointHandles[ndxPt] = BuildSubdivPoint( vPoints[ndxPt], vPointNormals[ndxPt] );
	}

	// build subdivision edges
	for( int ndxEdge = 0; ndxEdge < EDITDISP_QUADSIZE; ndxEdge++ )
	{
		pQuad->m_EdgeHandles[ndxEdge] = BuildSubdivEdge( ndxEdge, quadHandle, m_Quads.InvalidIndex(), -1 );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::GetDispFrom( CMapDisp *pDisp )
{
	//
	// find the parent quad with the id of the displacement
	//
	for ( SubdivQuadHandle_t quadHandle = m_Quads.Head(); quadHandle != m_Quads.InvalidIndex();
	      quadHandle = m_Quads.Next( quadHandle ) )
	{
		SubdivQuad_t *pQuad = GetQuad( quadHandle );
		if ( pQuad )
		{
			// find children quads that "belong" to this displacement
			if( pQuad->m_EditDispHandle != pDisp->GetEditHandle() )
				continue;

			// get the data at the appropriate level -- (based on the size of the displacement)
			if ( pQuad->m_Level != pDisp->GetPower() )
				continue;

			//
			// fill in subdivision positions and normals
			//
			for ( int ndxPt = 0; ndxPt < 4; ndxPt++ )
			{
				SubdivPoint_t *pPoint = GetPoint( pQuad->m_PointHandles[ndxPt] );
				if ( pPoint )
				{
					Vector vFlatVert, vSubVert;
					pDisp->GetFlatVert( pQuad->m_QuadIndices[ndxPt], vFlatVert );
					VectorSubtract( pPoint->m_vPoint, vFlatVert, vSubVert );
					pDisp->UpdateVertPositionForSubdiv( pQuad->m_QuadIndices[ndxPt], vSubVert );
					pDisp->SetSubdivNormal( pQuad->m_QuadIndices[ndxPt], pPoint->m_vNormal );
				}
			}
		}
	}

	// tell the dispalcemet to update itself
	pDisp->UpdateData();

	// reset subdivision/subdivided flags
	pDisp->SetReSubdivision( false );
	pDisp->SetSubdivided( true );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::DoCatmullClarkSubdivision( void )
{
	for ( int ndxLevel = 0; ndxLevel < NUM_SUBDIV_LEVELS; ndxLevel++ )
	{
		// subdivide
		CatmullClarkSubdivision();

		// update the subdivision hierarchy (tree)
		UpdateSubdivisionHierarchy( ndxLevel );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::CatmullClarkSubdivision( void )
{
	//
	// step 1: calculate the "new edge points" for all edges
	//
	for ( SubdivEdgeHandle_t edgeHandle = m_Edges.Head(); edgeHandle != m_Edges.InvalidIndex();
	      edgeHandle = m_Edges.Next( edgeHandle ) )
	{
		Edge_CalcNewPoint( edgeHandle );
	}

	//
	// step 2: calculate the valence and edge list
	//
	for ( SubdivPointHandle_t ptHandle = m_Points.Head(); ptHandle != m_Points.InvalidIndex();
	      ptHandle = m_Points.Next( ptHandle ) )
	{
		for ( SubdivEdgeHandle_t edgeHandle = m_Edges.Head(); edgeHandle != m_Edges.InvalidIndex();
		      edgeHandle = m_Edges.Next( edgeHandle ) )
		{
			SubdivEdge_t *pEdge = GetEdge( edgeHandle );
			if ( !pEdge->m_bActive )
				continue;

			if ( ( ptHandle == pEdge->m_PointHandles[0] ) || ( ptHandle == pEdge->m_PointHandles[1] ) )
			{
				SubdivPoint_t *pPoint = GetPoint( ptHandle );

				if ( pPoint->m_uValence < ( EDITDISP_QUADSIZE*4 ) )
				{
					pPoint->m_EdgeHandles[pPoint->m_uValence] = edgeHandle;
					pPoint->m_uValence++;
				}
			}
		}
	}

	//
	// step 3: determine the point's Type (Oridinary, Corner, Crease)
	//
	for ( SubdivPointHandle_t ptHandle = m_Points.Head(); ptHandle != m_Points.InvalidIndex(); ptHandle = m_Points.Next( ptHandle ) )
	{
		SubdivPoint_t *pPoint = GetPoint( ptHandle );
		if ( pPoint )
		{
			int sharpCount = 0;
			int sharpThreshold = pPoint->m_uValence - 1;
			bool bHasNeighbors = false;

			// initialize as oridinary -- determine otherwise
			pPoint->m_uType = SUBDIV_POINTORDINARY;

			for ( int ndxEdge = 0; ndxEdge < pPoint->m_uValence; ndxEdge++ )
			{
				SubdivEdge_t *pEdge = GetEdge( pPoint->m_EdgeHandles[ndxEdge] );
				if ( pEdge )
				{
					if ( pEdge->m_flSharpness > 0.0f )
					{
						sharpCount++;
					}

					if ( pEdge->m_QuadHandles[1] != m_Quads.InvalidIndex() )
					{
						bHasNeighbors = true;
					}
				}
			}

			if ( !bHasNeighbors || ( sharpCount >= sharpThreshold ) )
			{
				pPoint->m_uType = SUBDIV_POINTCORNER;
			}
			else if( sharpCount > 1 )
			{
				pPoint->m_uType = SUBDIV_POINTCREASE;
			}
		}
	}

	//
	// step 4: calculate the "new points" for all points
	//
	for ( SubdivPointHandle_t ptHandle = m_Points.Head(); ptHandle != m_Points.InvalidIndex(); ptHandle = m_Points.Next( ptHandle ) )
	{
		Point_CalcNewPoint( ptHandle );
	}

	//
	// step 5: copy all "new point" data to point data
	//
	for ( SubdivPointHandle_t ptHandle = m_Points.Head(); ptHandle != m_Points.InvalidIndex(); ptHandle = m_Points.Next( ptHandle ) )
	{
		SubdivPoint_t *pPoint = GetPoint( ptHandle );
		VectorCopy( pPoint->m_vNewPoint, pPoint->m_vPoint );
		VectorCopy( pPoint->m_vNewNormal, pPoint->m_vNewNormal );
		VectorClear( pPoint->m_vNewPoint );
		VectorClear( pPoint->m_vNewNormal );

		// reset valence
		pPoint->m_uValence = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEditDispSubdivMesh::UpdateSubdivisionHierarchy( int ndxLevel )
{
	int quadCount = m_Quads.Count();
	SubdivQuadHandle_t quadHandle = m_Quads.Head();
	int ndxQuad = 0;

	while ( ( quadHandle != m_Quads.InvalidIndex() ) && ( ndxQuad < quadCount ) )
	{
		SubdivQuad_t *pQuad = GetQuad( quadHandle );
		if ( pQuad )
		{
			// skip parent quads
			if ( pQuad->m_ndxChild[0] != m_Quads.InvalidIndex() )
			{
				ndxQuad++;
				quadHandle = m_Quads.Next( quadHandle );
				continue;
			}

			for( int ndxChild = 0; ndxChild < 4; ndxChild++ )
			{
				pQuad->m_ndxChild[ndxChild] = BuildSubdivQuad( ndxChild, quadHandle );
			}

			// de-activate all edges (children's edges are active now!)
			for ( int ndxEdge = 0; ndxEdge < 4; ndxEdge++ )
			{
				SubdivEdge_t *pEdge = GetEdge( pQuad->m_EdgeHandles[ndxEdge] );
				if ( pEdge )
				{
					pEdge->m_bActive = false;
				}
			}
		}

		ndxQuad++;
		quadHandle = m_Quads.Next( quadHandle );
	}
}
