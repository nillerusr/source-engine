//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include <stdafx.h>
#include <malloc.h>
#include "FaceEditSheet.h"
#include "MainFrm.h"
#include "GlobalFunctions.h"
#include "MapDisp.h"
#include "MapFace.h"
#include "utlvector.h"
#include "disp_tesselate.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//============================================================================
//
//         e1
//     c1------c2
//     |        |
//  e0 |        | e2		
//     |        |
//     c0------c3
//         e3 
//
// Note: edges refer to internal edge points only, corners "contain" all surfaces
//       touching the corner (surfaces that only touch the corner, as well as those
//       "edges" that end/begin at the corner(s))
//
#define DISPSEW_POINT_TOLERANCE		1.0f		// one unit

#define DISPSEW_NULL_INDEX			-99999

#define DISPSEW_EDGE_NORMAL			0
#define DISPSEW_EDGE_TJSTART		1
#define DISPSEW_EDGE_TJEND			2
#define DISPSEW_EDGE_TJ				3

#define DISPSEW_FACES_AT_EDGE	3
#define DISPSEW_FACES_AT_CORNER	16
#define DISPSEW_FACES_AT_TJUNC	8

struct SewEdgeData_t
{
	int			faceCount;								// number of faces contributing to the edge sew
	CMapFace	*pFaces[DISPSEW_FACES_AT_EDGE];			// the faces contributing to the edge sew
	int			ndxEdges[DISPSEW_FACES_AT_EDGE];		// the faces' edge indices contributing to the edge sew
	int			type[DISPSEW_FACES_AT_EDGE];			// the type of edge t-junction, match t-junction start, etc....
};

struct SewCornerData_t
{
	int			faceCount;								// number of faces contributing to the corner sew
	CMapFace	*pFaces[DISPSEW_FACES_AT_CORNER];		// the faces contributing to the corner sew
	int			ndxCorners[DISPSEW_FACES_AT_CORNER];	// the faces' corner indices contributing to the corner sew
};

struct SewTJuncData_t
{
	int			faceCount;								// number of faces contributing to the t-junction sew
	CMapFace	*pFaces[DISPSEW_FACES_AT_TJUNC];			// the faces contributing to the t-junction sew
	int			ndxCorners[DISPSEW_FACES_AT_TJUNC];		// the faces' corner indices contributing to the t-junction sew
	int			ndxEdges[DISPSEW_FACES_AT_TJUNC];		// the faces' edge (midpoint) indices contributing to the t-junction sew
};

static CUtlVector<SewEdgeData_t*> s_EdgeData;
static CUtlVector<SewCornerData_t*> s_CornerData;
static CUtlVector<SewTJuncData_t*> s_TJData;
static CUtlVector<CCoreDispInfo*> m_aCoreDispInfos;

// local functions
void SewCorner_Build( void );
void SewCorner_Resolve( void );
void SewCorner_Destroy( SewCornerData_t *pCornerData );
void SewTJunc_Build( void );
void SewTJunc_Resolve( void );
void SewTJunc_Destroy( SewTJuncData_t *pTJData );
void SewEdge_Build( void );
void SewEdge_Resolve( void );
void SewEdge_Destroy( SewEdgeData_t *pEdgeData );

void PlanarizeDependentVerts( void );

//-----------------------------------------------------------------------------
// Purpose: compare two point positions to see if they are equivolent given a
//          tolerance
//-----------------------------------------------------------------------------
bool PointCompareWithTolerance( Vector const& pt1, Vector const& pt2, float tolerance )
{
	for( int i = 0 ; i < 3 ; i++ )
	{
		if( fabs( pt1[i] - pt2[i] ) > tolerance )
			return false;
	}
	
	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool EdgeCompare( Vector *pEdgePts1, Vector *pEdgePts2, int &edgeType1, int &edgeType2 )
{
	Vector edge1[3];
	Vector edge2[3];

	//
	// create edges and midpoints
	//
	edge1[0] = pEdgePts1[0];
	edge1[1] = ( pEdgePts1[0] + pEdgePts1[1] ) * 0.5f;
	edge1[2] = pEdgePts1[1];

	edge2[0] = pEdgePts2[0];
	edge2[1] = ( pEdgePts2[0] + pEdgePts2[1] ) * 0.5f;
	edge2[2] = pEdgePts2[1];
	
	// assume edge type to be normal (will get overridden if otherwise)
	edgeType1 = DISPSEW_EDGE_NORMAL;
	edgeType2 = DISPSEW_EDGE_NORMAL;

	//
	// compare points and determine how many are shared between the two edges
	//
	int overlapCount = 0;
	int ndxEdge1[2];
	int ndxEdge2[2];

	for( int ndx1 = 0; ndx1 < 3; ndx1++ )
	{
		for( int ndx2 = 0; ndx2 < 3; ndx2++ )
		{
			if( PointCompareWithTolerance( edge1[ndx1], edge2[ndx2], DISPSEW_POINT_TOLERANCE ) )
			{
				// no midpoint to midpoint sharing allowed (midpoints are odd index values)
				if( ( ndx1%2 != 0 ) && ( ndx2%2 != 0 ) )
					continue;

				// sanity check
				assert( overlapCount >= 0 );
				assert( overlapCount < 2 );

				ndxEdge1[overlapCount] = ndx1;
				ndxEdge2[overlapCount] = ndx2;

				overlapCount++;
				break;
			}
		}
	}

	if( overlapCount != 2 )
		return false;

	// handle edge1 as t-junction edge
	if( ndxEdge1[0]%2 != 0 )
	{
		edgeType1 = DISPSEW_EDGE_TJ;
		
		if( ndxEdge1[1] == 0 )
		{
			edgeType2 = DISPSEW_EDGE_TJSTART;
		}
		else if( ndxEdge1[1] == 2 )
		{
			edgeType2 = DISPSEW_EDGE_TJEND;			
		}
	}
	else if( ndxEdge1[1]%2 != 0 ) 
	{
		edgeType1 = DISPSEW_EDGE_TJ;

		if( ndxEdge1[0] == 0 )
		{
			edgeType2 = DISPSEW_EDGE_TJSTART;
		}
		else if( ndxEdge1[0] == 2 )
		{
			edgeType2 = DISPSEW_EDGE_TJEND;
		}
	}

	// handle edge2 as t-junction edge
	if( ndxEdge2[0]%2 != 0 )
	{
		edgeType2 = DISPSEW_EDGE_TJ;

		if( ndxEdge2[1] == 0 )
		{
			edgeType1 = DISPSEW_EDGE_TJSTART;
		}
		else if( ndxEdge2[1] == 2 )
		{
			edgeType1 = DISPSEW_EDGE_TJEND;
		}
	}
	else if( ndxEdge2[1]%2 != 0 ) 
	{
		edgeType2 = DISPSEW_EDGE_TJ;

		if( ndxEdge2[0] == 0 )
		{
			edgeType1 = DISPSEW_EDGE_TJSTART;
		}
		else if( ndxEdge2[0] == 2 )
		{
			edgeType1 = DISPSEW_EDGE_TJEND;
		}
	}

	return true;	
}


//-----------------------------------------------------------------------------
// Purpose: get a point from the surface at the given index, will get the point
//          from the displacement surface if it exists, it will get it from the
//          base face otherwise
//-----------------------------------------------------------------------------
inline void GetPointFromSurface( CMapFace *pFace, int ndxPt, Vector &pt )
{
	EditDispHandle_t dispHandle = pFace->GetDisp();
	if( dispHandle != EDITDISPHANDLE_INVALID )
	{
		CMapDisp *pDisp = EditDispMgr()->GetDisp( dispHandle );
		pDisp->GetSurfPoint( ndxPt, pt );
	}
	else
	{
		pFace->GetPoint( pt, ndxPt );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int GetEdgePointIndex( CMapDisp *pDisp, int edgeIndex, int edgePtIndex, bool bCCW )
{
	int height = pDisp->GetHeight();
	int width = pDisp->GetWidth();

	if( bCCW )
	{
		switch( edgeIndex )
		{
		case 0: { return ( edgePtIndex * height ); }
		case 1: { return ( ( ( height - 1 ) * width ) + edgePtIndex ); }
		case 2: { return ( ( height * width - 1 ) - ( edgePtIndex * height ) ); }
		case 3: { return ( ( width - 1 ) - edgePtIndex ); }
		default: { return -1; }
		}
	}
	else
	{
		switch( edgeIndex )
		{
		case 0: { return ( ( ( height - 1 ) * width ) - ( edgePtIndex * height ) ); }
		case 1: { return ( ( height * width - 1 ) - edgePtIndex ); }
		case 2: { return ( ( width - 1 ) + ( edgePtIndex * height ) ); }
		case 3: { return ( edgePtIndex ); }
		default: { return -1; }
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int GetCornerPointIndex( CMapDisp *pDisp, int cornerIndex )
{
	int width = pDisp->GetWidth();
	int height = pDisp->GetHeight();

	switch( cornerIndex )
	{
	case 0: { return 0; }
	case 1: { return ( ( height - 1 ) * width ); }
	case 2: { return ( height * width - 1 ); }
	case 3: { return ( width - 1 ); }
	default: { return -1; }
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int GetTJuncIndex( CMapDisp *pDisp, int ndxEdge )
{
	int width = pDisp->GetWidth();
	int height = pDisp->GetHeight();

	switch( ndxEdge )
	{
	case 0: { return( height * ( width / 2 ) ); }
	case 1: { return( ( ( height - 1 ) * width ) + ( width / 2 ) ); }
	case 2: { return( ( height * ( width / 2 ) ) + ( width - 1 ) ); }
	case 3: { return( width / 2 ); }
	default: { return -1; }
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void AverageVectorFieldData( CMapDisp *pDisp1, int ndx1, CMapDisp *pDisp2, int ndx2 )
{
	//
	// average the positions at each index
	// position = dispVector * dispDist
	//
	float dist1 = pDisp1->GetFieldDistance( ndx1 );
	float dist2 = pDisp2->GetFieldDistance( ndx2 );
	
	Vector v1, v2;
	pDisp1->GetFieldVector( ndx1, v1 );
	pDisp2->GetFieldVector( ndx2, v2 );

	v1 *= dist1;
	v2 *= dist2;

	Vector vAvg;
	vAvg = ( v1 + v2 ) * 0.5f;

	float distAvg = VectorNormalize( vAvg );

	pDisp1->SetFieldDistance( ndx1, distAvg );
	pDisp2->SetFieldDistance( ndx2, distAvg );
	pDisp1->SetFieldVector( ndx1, vAvg );
	pDisp2->SetFieldVector( ndx2, vAvg );

	// Check to see if the materials match and blend alphas if they do.
	CMapFace *pFace1 = static_cast<CMapFace*>( pDisp1->GetParent() );
	CMapFace *pFace2 = static_cast<CMapFace*>( pDisp2->GetParent() );
	char szMatName1[128];
	char szMatName2[128];
	pFace1->GetTexture()->GetShortName( szMatName1 );
	pFace2->GetTexture()->GetShortName( szMatName2 );
	if ( !strcmpi( szMatName1, szMatName2 ) )
	{
		// Grab the alphas at the points and average them.
		float flAlpha1, flAlpha2;
		flAlpha1 = pDisp1->GetAlpha( ndx1 );
		flAlpha2 = pDisp2->GetAlpha( ndx2 );
		float flAlphaBlend = ( flAlpha1 + flAlpha1 ) * 0.5f;
		pDisp1->SetAlpha( ndx1, flAlphaBlend );
		pDisp2->SetAlpha( ndx2, flAlphaBlend );
	}

	//
	// average the subdivion positions and normals
	//
	pDisp1->GetSubdivPosition( ndx1, v1 );
	pDisp2->GetSubdivPosition( ndx2, v2 );
	vAvg = ( v1 + v2 ) * 0.5f;
	pDisp1->SetSubdivPosition( ndx1, vAvg );
	pDisp2->SetSubdivPosition( ndx2, vAvg );

	pDisp1->GetSubdivNormal( ndx1, v1 );
	pDisp2->GetSubdivNormal( ndx2, v2 );
	vAvg = v1 + v2;
	VectorNormalize( vAvg );
	pDisp1->SetSubdivNormal( ndx1, vAvg );
	pDisp2->SetSubdivNormal( ndx2, vAvg );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void BlendVectorFieldData( CMapDisp *pDisp1, int ndxSrc1, int ndxDst1, 
						   CMapDisp *pDisp2, int ndxSrc2, int ndxDst2,
						   float blendFactor )
{
	//
	// to blend positions -- calculate the positions at the end points
	// find the new point along the parameterized line and calculate the
	// new field vector direction and distance (position)
	//
	float dist1 = pDisp1->GetFieldDistance( ndxSrc1 );
	float dist2 = pDisp2->GetFieldDistance( ndxSrc2 );
	
	Vector v1, v2;
	pDisp1->GetFieldVector( ndxSrc1, v1 );
	pDisp2->GetFieldVector( ndxSrc2, v2 );

	v1 *= dist1;
	v2 *= dist2;

	Vector vBlend;
	vBlend = v1 + ( v2 - v1 ) * blendFactor;

	float distBlend = VectorNormalize( vBlend );

	pDisp1->SetFieldDistance( ndxDst1, distBlend );
	pDisp2->SetFieldDistance( ndxDst2, distBlend );
	pDisp1->SetFieldVector( ndxDst1, vBlend );
	pDisp2->SetFieldVector( ndxDst2, vBlend );

	// Check to see if the materials match and blend alphas if they do.
	CMapFace *pFace1 = static_cast<CMapFace*>( pDisp1->GetParent() );
	CMapFace *pFace2 = static_cast<CMapFace*>( pDisp2->GetParent() );
	char szMatName1[128];
	char szMatName2[128];
	pFace1->GetTexture()->GetShortName( szMatName1 );
	pFace2->GetTexture()->GetShortName( szMatName2 );
	if ( !strcmpi( szMatName1, szMatName2 ) )
	{
		float flAlpha1, flAlpha2;
		flAlpha1 = pDisp1->GetAlpha( ndxDst1 );
		flAlpha2 = pDisp2->GetAlpha( ndxDst2 );
		float flAlphaBlend = flAlpha1 + ( flAlpha2 - flAlpha1 ) * blendFactor;
		pDisp1->SetAlpha( ndxDst1, flAlphaBlend );
		pDisp2->SetAlpha( ndxDst2, flAlphaBlend );
	}

	//
	// blend subdivision positions and normals as before,
	// this isn't truly accurate, but I am not sure what these
	// values mean in the edge sewing case anyway???
	//
	pDisp1->GetSubdivPosition( ndxSrc1, v1 );
	pDisp2->GetSubdivPosition( ndxSrc2, v2 );
	vBlend = v1 + ( v2 - v1 ) * blendFactor;
	pDisp1->SetSubdivPosition( ndxDst1, vBlend );
	pDisp2->SetSubdivPosition( ndxDst2, vBlend );

	pDisp1->GetSubdivNormal( ndxSrc1, v1 );
	pDisp2->GetSubdivNormal( ndxSrc2, v2 );
	vBlend = v1 + ( v2 - v1 ) * blendFactor;
	pDisp1->SetSubdivNormal( ndxDst1, vBlend );
	pDisp2->SetSubdivNormal( ndxDst2, vBlend );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline bool Face_IsSolid( CMapFace *pFace )
{
	return ( pFace->GetDisp() == EDITDISPHANDLE_INVALID );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Faces_Update( void )
{
	//
	// get the "faces" selection list (contains displaced and non-displaced faces)
	//
	CFaceEditSheet *pSheet = GetMainWnd()->GetFaceEditSheet();
	if( !pSheet )
		return;

	//
	// for each face in list
	//
	int faceCount = pSheet->GetFaceListCount();

	for( int ndxFace = 0; ndxFace < faceCount; ndxFace++ )
	{
		// get the current face
		CMapFace *pFace = pSheet->GetFaceListDataFace( ndxFace );
		if( !pFace )
			continue;

		// only update displacement surfaces
		EditDispHandle_t dispHandle = pFace->GetDisp();
		if( dispHandle == EDITDISPHANDLE_INVALID )
			continue;

		CMapDisp *pDisp = EditDispMgr()->GetDisp( dispHandle );
		pDisp->UpdateData();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Build temporary edge/midpoint/corner info for sewing.
//-----------------------------------------------------------------------------
void PreFaceListSew( void )
{
	// Build edge/midpoint/corner data.
	SewCorner_Build();
	SewTJunc_Build();
	SewEdge_Build();
}

//-----------------------------------------------------------------------------
// Purpose: Destroy temporary edge/midpoint/corner info for sewing and
//          update the effected displacements.
//-----------------------------------------------------------------------------
void PostFaceListSew( void )
{
	// Destroy all corners, midpoint, edges.
	int count = s_CornerData.Size();
	for( int i = 0; i < count; i++ )
	{
		SewCorner_Destroy( s_CornerData.Element( i ) );
	}

	count = s_TJData.Size();
	for( int i = 0; i < count; i++ )
	{
		SewTJunc_Destroy( s_TJData.Element( i ) );
	}
	
	count = s_EdgeData.Size();
	for( int i = 0; i < count; i++ )
	{
		SewEdge_Destroy( s_EdgeData.Element( i ) );
	}

	// Flush all of the sewing data buffers.
	s_CornerData.Purge();
	s_TJData.Purge();
	s_EdgeData.Purge();

	// Update the faces.
	Faces_Update();
}

//-----------------------------------------------------------------------------
// Purpose: given a face with a displacement surface, "sew" all edges to all
//          neighboring displacement and non-displacement surfaces 
//          found in the selection set
//-----------------------------------------------------------------------------
void FaceListSewEdges( void )
{
	// Setup.
	PreFaceListSew();

	// Resolve/Planarize unusable verts.
	PlanarizeDependentVerts();

	// Resolve sewing.
	SewCorner_Resolve();
	SewTJunc_Resolve();
	SewEdge_Resolve();

	// Update and clean-up.
	PostFaceListSew();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
SewCornerData_t *SewCorner_Create( void )
{
	SewCornerData_t *pCornerData = new SewCornerData_t;
	if( pCornerData )
	{
		// initialize the data
		pCornerData->faceCount = 0;
		return pCornerData;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewCorner_Destroy( SewCornerData_t *pCornerData )
{
	if( pCornerData )
	{
		delete pCornerData;
		pCornerData = NULL;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool SewCorner_IsSolid( SewCornerData_t *pCornerData )
{
	for( int i = 0; i < pCornerData->faceCount; i++ )
	{
		if( Face_IsSolid( pCornerData->pFaces[i] ) )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewCorner_Add( SewCornerData_t *pCornerData, CMapFace *pFace, int ndx )
{
	if ( pCornerData->faceCount >= DISPSEW_FACES_AT_CORNER )
	{
		AfxMessageBox( "Warning: Too many displacement faces at corner!\n" );
		return;
	}

	pCornerData->pFaces[pCornerData->faceCount] = pFace;
	pCornerData->ndxCorners[pCornerData->faceCount] = ndx;
	pCornerData->faceCount++;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewCorner_AddToList( SewCornerData_t *pCornerData )
{
	// get the current corner point
	Vector pt;
	GetPointFromSurface( pCornerData->pFaces[0], pCornerData->ndxCorners[0], pt );

	//
	// check to see if the corner point already exists in the corner data list
	//
	int cornerCount = s_CornerData.Size();

	for( int i = 0; i < cornerCount; i++ )
	{
		//
		// get the compare corner point
		//
		SewCornerData_t *pCmpData = s_CornerData.Element( i );
		if( !pCmpData )
			continue;

		Vector cmpPt;
		GetPointFromSurface( pCmpData->pFaces[0], pCmpData->ndxCorners[0], cmpPt );

		// compare the points - return if found
		if( PointCompareWithTolerance( pt, cmpPt, DISPSEW_POINT_TOLERANCE ) )
		{
			SewCorner_Destroy( pCornerData );
			return;
		}
	}

	// unique corner point -- add it to the list
	s_CornerData.AddToTail( pCornerData );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewCorner_Build( void )
{
	//
	// get the "faces" selection list (contains displaced and non-displaced faces)
	//
	CFaceEditSheet *pSheet = GetMainWnd()->GetFaceEditSheet();
	if( !pSheet )
		return;

	//
	// for each face in list
	//
	int faceCount = pSheet->GetFaceListCount();

	for( int ndxFace = 0; ndxFace < faceCount; ndxFace++ )
	{
		// get the current face
		CMapFace *pFace = pSheet->GetFaceListDataFace( ndxFace );
		if( !pFace )
			continue;

		//
		// for each face point
		//
		int ptCount = pFace->GetPointCount();
		for( int ndxPt = 0; ndxPt < ptCount; ndxPt++ )
		{
			// get the current point
			Vector pt;
			GetPointFromSurface( pFace, ndxPt, pt );

			// allocate new corner point
			SewCornerData_t *pCornerData = SewCorner_Create();
			if( !pCornerData )
				return;

			//
			// compare this point to all of the other points on all the other faces in the list
			//
			for( int ndxFace2 = 0; ndxFace2 < faceCount; ndxFace2++ )
			{
				// don't compare to itself
				if( ndxFace == ndxFace2 )
					continue;

				// get the current compare face
				CMapFace *pFace2 = pSheet->GetFaceListDataFace( ndxFace2 );
				if( !pFace2 )
					continue;

				//
				// for each compare face point
				//
				int ptCount2 = pFace2->GetPointCount();
				for( int ndxPt2 = 0; ndxPt2 < ptCount2; ndxPt2++ )
				{
					// get the current compare point
					Vector pt2;
					GetPointFromSurface( pFace2, ndxPt2, pt2 );

					// compare pt1 and pt2
					if( PointCompareWithTolerance( pt, pt2, DISPSEW_POINT_TOLERANCE ) )
					{
						SewCorner_Add( pCornerData, pFace2, ndxPt2 );
					}
				}
			}

			// had neighbors -- add base point and add it to corner list
			if( pCornerData->faceCount > 0 )
			{
				SewCorner_Add( pCornerData, pFace, ndxPt );
				SewCorner_AddToList( pCornerData );
			}
			// no neighbors -- de-allocate
			else
			{
				SewCorner_Destroy( pCornerData );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewCorner_ResolveDisp( SewCornerData_t *pCornerData )
{
	// the field data accumulators
	float avgDist = 0.0f;
	Vector vAvgField( 0.0f, 0.0f, 0.0f );
	Vector vAvgSubdivPos( 0.0f, 0.0f, 0.0f );
	Vector vAvgSubdivNormal( 0.0f, 0.0f, 0.0f );
	float flAvgAlpha = 0.0f;

	// Blend the alpha?
	bool bBlendAlpha = true;
	char szMatName1[128];
	char szMatName2[128];
	bool bInitMat = false;
	for( int i = 0; i < pCornerData->faceCount; i++ )
	{
		// get the current corner face
		CMapFace *pFace = pCornerData->pFaces[i];
		if( !pFace )
			continue;

		// get the current displacement surface to reset, if solid = done!
		EditDispHandle_t dispHandle = pFace->GetDisp();
		if( dispHandle == EDITDISPHANDLE_INVALID )
			continue;

		if ( !bInitMat )
		{
			pFace->GetTexture()->GetShortName( szMatName1 );
			bInitMat = true;
			continue;
		}
		else
		{
			pFace->GetTexture()->GetShortName( szMatName2 );
			if ( strcmpi( szMatName1, szMatName2 ) )
			{
				bBlendAlpha = false;
				break;
			}
		}
	}

	// for all the faces at the corner
	for( int i = 0; i < pCornerData->faceCount; i++ )
	{
		// get the current corner face
		CMapFace *pFace = pCornerData->pFaces[i];
		if( !pFace )
			continue;

		// get the current displacement surface to reset, if solid = done!
		EditDispHandle_t dispHandle = pFace->GetDisp();
		if( dispHandle == EDITDISPHANDLE_INVALID )
			continue;
		
		CMapDisp *pDisp = EditDispMgr()->GetDisp( dispHandle );

		// get the corner index
		int ndxPt = GetCornerPointIndex( pDisp, pCornerData->ndxCorners[i] );
		if( ndxPt == -1 )
			continue;

		Vector vecPos;
		pDisp->GetVert( ndxPt, vecPos );

		float dist = pDisp->GetFieldDistance( ndxPt );
		avgDist += dist;

		Vector vTmp;
		pDisp->GetFieldVector( ndxPt, vTmp );
		vAvgField += vTmp;

		pDisp->GetSubdivPosition( ndxPt, vTmp );
		vAvgSubdivPos += vTmp;

		pDisp->GetSubdivNormal( ndxPt, vTmp );
		vAvgSubdivNormal += vTmp;

		if ( bBlendAlpha )
		{
			flAvgAlpha += pDisp->GetAlpha( ndxPt );
		}
	}

	// calculate the average
	avgDist /= pCornerData->faceCount;
	vAvgField /= pCornerData->faceCount;
	vAvgSubdivPos /= pCornerData->faceCount;
	vAvgSubdivNormal /= pCornerData->faceCount;
	if ( bBlendAlpha )
	{
		flAvgAlpha /= pCornerData->faceCount;
	}

	for( int i = 0; i < pCornerData->faceCount; i++ )
	{
		// get the current corner face
		CMapFace *pFace = pCornerData->pFaces[i];
		if( !pFace )
			continue;

		// get the current displacement surface to reset, if solid = done!
		EditDispHandle_t dispHandle = pFace->GetDisp();
		if( dispHandle == EDITDISPHANDLE_INVALID )
			continue;

		CMapDisp *pDisp = EditDispMgr()->GetDisp( dispHandle );

		// get the corner index
		int ndxPt = GetCornerPointIndex( pDisp, pCornerData->ndxCorners[i] );
		if( ndxPt == -1 )
			continue;

		// set the averaged values
		pDisp->SetFieldDistance( ndxPt, avgDist );
		pDisp->SetFieldVector( ndxPt, vAvgField );
		pDisp->SetSubdivPosition( ndxPt, vAvgSubdivPos );
		pDisp->SetSubdivNormal( ndxPt, vAvgSubdivNormal );
		if ( bBlendAlpha )
		{
			pDisp->SetAlpha( ndxPt, flAvgAlpha );
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewCorner_ResolveSolid( SewCornerData_t *pCornerData )
{
	// create a clear vector - to reset the offset vector
	Vector vClear( 0.0f, 0.0f, 0.0f );

	// for all the faces at the corner
	for( int i = 0; i < pCornerData->faceCount; i++ )
	{
		// get the current corner face
		CMapFace *pFace = pCornerData->pFaces[i];
		if( !pFace )
			continue;

		// get the current displacement surface to reset, if solid = done!
		EditDispHandle_t dispHandle = pFace->GetDisp();
		if( dispHandle == EDITDISPHANDLE_INVALID )
			continue;

		CMapDisp *pDisp = EditDispMgr()->GetDisp( dispHandle );

		// get the face normal -- to reset the field vector
		Vector vNormal;
		pDisp->GetSurfNormal( vNormal );

		// get the corner index
		int ndxPt = GetCornerPointIndex( pDisp, pCornerData->ndxCorners[i] );
		if( ndxPt == -1 )
			continue;

		//
		// reset all neighbor surface data - field vector, distance, and offset
		//
		pDisp->SetFieldDistance( ndxPt, 0.0f );
		pDisp->SetFieldVector( ndxPt, vNormal );
		pDisp->SetSubdivPosition( ndxPt, vClear );
		pDisp->SetSubdivNormal( ndxPt, vNormal );
		pDisp->SetAlpha( ndxPt, 0.0f );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewCorner_Resolve( void )
{
	// get the number of corners in the corner list
	int cornerCount = s_CornerData.Size();

	// resolve each corner
	for( int i = 0; i < cornerCount; i++ )
	{
		// get the current corner data struct
		SewCornerData_t *pCornerData = s_CornerData.Element( i );
		if( !pCornerData )
			continue;

		// determine if any of the faces is solid
		bool bSolid = SewCorner_IsSolid( pCornerData );

		// solid at corner -- reset corner data
		if( bSolid )
		{
			SewCorner_ResolveSolid( pCornerData );
		}
		// all disps at corner -- average
		else
		{
			SewCorner_ResolveDisp( pCornerData );
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
SewTJuncData_t *SewTJunc_Create( void )
{
	SewTJuncData_t *pTJData = new SewTJuncData_t;
	if( pTJData )
	{
		// initialize the data
		pTJData->faceCount = 0;
		return pTJData;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewTJunc_Destroy( SewTJuncData_t *pTJData )
{
	if( pTJData )
	{
		delete pTJData;
		pTJData = NULL;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool SewTJunc_IsSolid( SewTJuncData_t *pTJData )
{
	for( int i = 0; i < pTJData->faceCount; i++ )
	{
		if( Face_IsSolid( pTJData->pFaces[i] ) )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewTJunc_Add( SewTJuncData_t *pTJData, CMapFace *pFace, int ndxCorner, int ndxEdge )
{
	if ( pTJData->faceCount >= DISPSEW_FACES_AT_TJUNC )
	{
		AfxMessageBox( "Warning: Too many displacement faces at t-junction!\n" );
		return;
	}

	pTJData->pFaces[pTJData->faceCount] = pFace;
	pTJData->ndxCorners[pTJData->faceCount] = ndxCorner;
	pTJData->ndxEdges[pTJData->faceCount] = ndxEdge;
	pTJData->faceCount++;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewTJunc_AddToList( SewTJuncData_t *pTJData )
{
	// get the current t-junction point
	Vector pt;
	GetPointFromSurface( pTJData->pFaces[0], pTJData->ndxCorners[0], pt );

	//
	// check to see if the t-junction point already exists in the t-junction data list
	//
	int tjCount = s_TJData.Size();
	for( int i = 0; i < tjCount; i++ )
	{
		// get the compare t-junction point
		SewTJuncData_t *pCmpData = s_TJData.Element( i );
		if( !pCmpData )
			continue;

		Vector cmpPt;
		GetPointFromSurface( pCmpData->pFaces[0], pCmpData->ndxCorners[0], cmpPt );

		// compare the points - return if found
		if( PointCompareWithTolerance( pt, cmpPt, DISPSEW_POINT_TOLERANCE ) )
		{
			SewTJunc_Destroy( pTJData );
			return;
		}
	}

	// unique t-junction point -- add it to the list
	s_TJData.AddToTail( pTJData );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewTJunc_Build( void )
{
	//
	// get the "faces" selection list (contains displaced and non-displaced faces)
	//
	CFaceEditSheet *pSheet = GetMainWnd()->GetFaceEditSheet();
	if( !pSheet )
		return;

	//
	// for each face in list
	//
	int faceCount = pSheet->GetFaceListCount();

	for( int ndxFace = 0; ndxFace < faceCount; ndxFace++ )
	{
		// get the current face
		CMapFace *pFace = pSheet->GetFaceListDataFace( ndxFace );
		if( !pFace )
			continue;

		//
		// for each face point
		//
		int ptCount = pFace->GetPointCount();
		for( int ndxPt = 0; ndxPt < ptCount; ndxPt++ )
		{
			// get the current t-junction point
			Vector pt, tmpPt1, tmpPt2;
			GetPointFromSurface( pFace, ndxPt, tmpPt1 );
			GetPointFromSurface( pFace, (ndxPt+1)%ptCount, tmpPt2 );
			pt = ( tmpPt1 + tmpPt2 ) * 0.5f;

			// allocate new corner point
			SewTJuncData_t *pTJData = SewTJunc_Create();
			if( !pTJData )
				return;

			//
			// compare this point to all of the other points on all the other faces in the list
			//
			for( int ndxFace2 = 0; ndxFace2 < faceCount; ndxFace2++ )
			{
				// don't compare to itself
				if( ndxFace == ndxFace2 )
					continue;

				// get the current compare face
				CMapFace *pFace2 = pSheet->GetFaceListDataFace( ndxFace2 );
				if( !pFace2 )
					continue;

				//
				// for each compare face point
				//
				int ptCount2 = pFace2->GetPointCount();
				for( int ndxPt2 = 0; ndxPt2 < ptCount2; ndxPt2++ )
				{
					// get the current compare point
					Vector pt2;
					GetPointFromSurface( pFace2, ndxPt2, pt2 );

					// compare pt1 and pt2
					if( PointCompareWithTolerance( pt, pt2, DISPSEW_POINT_TOLERANCE ) )
					{
						SewTJunc_Add( pTJData, pFace2, ndxPt2, -1 );
					}
				}
			}

			// had neighbors -- add base point and add it to corner list
			if( pTJData->faceCount > 0 )
			{
				SewTJunc_Add( pTJData, pFace, -1, ndxPt );
				SewTJunc_AddToList( pTJData );
			}
			// no neighbors -- de-allocate
			else
			{
				SewTJunc_Destroy( pTJData );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewTJunc_ResolveDisp( SewTJuncData_t *pTJData )
{
	// the field data accumulators
	float avgDist = 0.0f;
	Vector vAvgField( 0.0f, 0.0f, 0.0f );
	Vector vAvgSubdivPos( 0.0f, 0.0f, 0.0f );
	Vector vAvgSubdivNormal( 0.0f, 0.0f, 0.0f );
	float  flAvgAlpha = 0.0f;

	// for all the faces at the t-junction
	for( int i = 0; i < pTJData->faceCount; i++ )
	{
		// get the current t-junction face
		CMapFace *pFace = pTJData->pFaces[i];
		if( !pFace )
			continue;

		// get the current displacement surface to reset, if solid = done!
		EditDispHandle_t dispHandle = pFace->GetDisp();
		if( dispHandle == EDITDISPHANDLE_INVALID )
			continue;

		CMapDisp *pDisp = EditDispMgr()->GetDisp( dispHandle );

		// get the t-junction index
		int ndxPt = -1;
		if( pTJData->ndxCorners[i] != -1 )
		{
			ndxPt = GetCornerPointIndex( pDisp, pTJData->ndxCorners[i] );
		}
		else if( pTJData->ndxEdges[i] != -1 )
		{
			int ndxEdgePt = pDisp->GetWidth() / 2;
			ndxPt = GetEdgePointIndex( pDisp, pTJData->ndxEdges[i], ndxEdgePt, true );
		}

		if( ndxPt == -1 )
			continue;

		float dist = pDisp->GetFieldDistance( ndxPt );
		avgDist += dist;

		Vector vTmp;
		pDisp->GetFieldVector( ndxPt, vTmp );
		vAvgField += vTmp;

		pDisp->GetSubdivPosition( ndxPt, vTmp );
		vAvgSubdivPos += vTmp;

		pDisp->GetSubdivNormal( ndxPt, vTmp );
		vAvgSubdivNormal += vTmp;

		flAvgAlpha += pDisp->GetAlpha( ndxPt );
	}

	// calculate the average
	avgDist /= pTJData->faceCount;
	vAvgField /= pTJData->faceCount;
	vAvgSubdivPos /= pTJData->faceCount;
	vAvgSubdivNormal /= pTJData->faceCount;
	flAvgAlpha /= pTJData->faceCount;

	for( int i = 0; i < pTJData->faceCount; i++ )
	{
		// get the current t-junction face
		CMapFace *pFace = pTJData->pFaces[i];
		if( !pFace )
			continue;

		// get the current displacement surface to reset, if solid = done!
		EditDispHandle_t dispHandle = pFace->GetDisp();
		if( dispHandle == EDITDISPHANDLE_INVALID )
			continue;

		CMapDisp *pDisp = EditDispMgr()->GetDisp( dispHandle );

		// get the t-junction index
		int ndxPt = -1;
		if( pTJData->ndxCorners[i] != -1 )
		{
			ndxPt = GetCornerPointIndex( pDisp, pTJData->ndxCorners[i] );
		}
		else if( pTJData->ndxEdges[i] != -1 )
		{
			int ndxEdgePt = pDisp->GetWidth() / 2;
			ndxPt = GetEdgePointIndex( pDisp, pTJData->ndxEdges[i], ndxEdgePt, true );
		}

		if( ndxPt == -1 )
			continue;

		// set the averaged values
		pDisp->SetFieldDistance( ndxPt, avgDist );
		pDisp->SetFieldVector( ndxPt, vAvgField );
		pDisp->SetSubdivPosition( ndxPt, vAvgSubdivPos );
		pDisp->SetSubdivNormal( ndxPt, vAvgSubdivNormal );
		pDisp->SetAlpha( ndxPt, flAvgAlpha );
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewTJunc_ResolveSolid( SewTJuncData_t *pTJData )
{
	// create a clear vector - to reset the offset vector
	Vector vClear( 0.0f, 0.0f, 0.0f );

	// for all the faces at the t-junction
	for( int i = 0; i < pTJData->faceCount; i++ )
	{
		// get the current t-junction face
		CMapFace *pFace = pTJData->pFaces[i];
		if( !pFace )
			continue;

		// get the current displacement surface to reset, if solid = done!
		EditDispHandle_t dispHandle = pFace->GetDisp();
		if( dispHandle == EDITDISPHANDLE_INVALID )
			continue;

		CMapDisp *pDisp = EditDispMgr()->GetDisp( dispHandle );

		// get the face normal -- to reset the field vector
		Vector vNormal;
		pDisp->GetSurfNormal( vNormal );

		// get the t-junction index
		int ndxPt = -1;
		if( pTJData->ndxCorners[i] != -1 )
		{
			ndxPt = GetCornerPointIndex( pDisp, pTJData->ndxCorners[i] );
		}
		else if( pTJData->ndxEdges[i] != -1 )
		{
			int ndxEdgePt = pDisp->GetWidth() / 2;
			ndxPt = GetEdgePointIndex( pDisp, pTJData->ndxEdges[i], ndxEdgePt, true );
		}

		if( ndxPt == -1 )
			continue;

		//
		// reset all neighbor surface data - field vector, distance, and offset
		//
		pDisp->SetFieldDistance( ndxPt, 0.0f );
		pDisp->SetFieldVector( ndxPt, vNormal );
		pDisp->SetSubdivPosition( ndxPt, vClear );
		pDisp->SetSubdivNormal( ndxPt, vNormal );
		pDisp->SetAlpha( ndxPt, 0.0f );
	}
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewTJunc_Resolve( void )
{
	// get the number of t-junctions in the t-junction list
	int tjCount = s_TJData.Size();

	// resolve each t-junction
	for( int i = 0; i < tjCount; i++ )
	{
		// get the current t-junction data struct
		SewTJuncData_t *pTJData = s_TJData.Element( i );
		if( !pTJData )
			continue;

		// determine if any of the faces is solid
		bool bSolid = SewTJunc_IsSolid( pTJData );

		// solid at t-junction -- reset t-junction data
		if( bSolid )
		{
			SewTJunc_ResolveSolid( pTJData );
		}
		// all disps at t-junction -- average
		else
		{
			SewTJunc_ResolveDisp( pTJData );
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
SewEdgeData_t *SewEdge_Create( void )
{
	SewEdgeData_t *pEdgeData = new SewEdgeData_t;
	if( pEdgeData )
	{
		// initialize the data
		pEdgeData->faceCount = 0;
		return pEdgeData;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewEdge_Destroy( SewEdgeData_t *pEdgeData )
{
	if( pEdgeData )
	{
		delete pEdgeData;
		pEdgeData = NULL;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline bool SewEdge_IsSolidNormal( SewEdgeData_t *pEdgeData )
{
	for( int i = 0; i < pEdgeData->faceCount; i++ )
	{
		if( Face_IsSolid( pEdgeData->pFaces[i] ) )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int SewEdge_TJIndex( SewEdgeData_t *pEdgeData, int type )
{
	for( int i = 0; i < pEdgeData->faceCount; i++ )
	{
		if( pEdgeData->type[i] == type )
			return i;
	}

	return -1;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline bool SewEdge_IsSolidTJunc( SewEdgeData_t *pEdgeData, int type )
{
	for( int i = 0; i < pEdgeData->faceCount; i++ )
	{	
		if( pEdgeData->type[i] != type )
			continue;

		if( Face_IsSolid( pEdgeData->pFaces[i] ) )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool SewEdge_Add( SewEdgeData_t *pEdgeData, CMapFace *pFace, int ndxEdge, int type )
{
	if ( pEdgeData->faceCount >= DISPSEW_FACES_AT_EDGE )
	{
		return false;
	}

	// Add face to edge.
	pEdgeData->pFaces[pEdgeData->faceCount] = pFace;
	pEdgeData->ndxEdges[pEdgeData->faceCount] = ndxEdge;
	pEdgeData->type[pEdgeData->faceCount] = type; 
	pEdgeData->faceCount++;

	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool SewEdge_AddToListMerge( SewEdgeData_t *pEdgeData, SewEdgeData_t *pCmpData )
{
	bool bReturn = true;

	for( int i = 0; i < pEdgeData->faceCount; i++ )
	{
		// t-junction edges already exist in both (skip it!)
		if( pEdgeData->type[i] == DISPSEW_EDGE_TJ )
			continue;

		int j;
		for( j = 0; j < pCmpData->faceCount; j++ )
		{
			// t-junction edges already exist in both (skip it!)
			if( pCmpData->type[j] == DISPSEW_EDGE_TJ )
				continue;

			if( pEdgeData->type[i] == pCmpData->type[j] )
				break;
		}

		// no match found -- add it
		if( j == pCmpData->faceCount )
		{
			if (!SewEdge_Add( pCmpData, pEdgeData->pFaces[i], pEdgeData->ndxEdges[i], pEdgeData->type[i] ))
			{
				bReturn = false;
			}
		}
	}

	return bReturn;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool SewEdge_AddToListTJunc( SewEdgeData_t *pEdgeData )
{
	// find the t-junction edge
	int ndxTJ = SewEdge_TJIndex( pEdgeData, DISPSEW_EDGE_TJ );
	if( ndxTJ == -1 )
		return true;

	// get the t-junction edge point count
	int ptCount = pEdgeData->pFaces[ndxTJ]->GetPointCount();

	// get the current t-junction edge (edge points)
	Vector edgePts[2];
	GetPointFromSurface( pEdgeData->pFaces[ndxTJ], pEdgeData->ndxEdges[ndxTJ], edgePts[0] );
	GetPointFromSurface( pEdgeData->pFaces[ndxTJ], (pEdgeData->ndxEdges[ndxTJ]+1)%ptCount, edgePts[1] );

	//
	// check to see if the edge already exists in the edge data list
	//
	int edgeCount = s_EdgeData.Size();
	for( int i = 0; i < edgeCount; i++ )
	{
		// get the edge points to compare against
		SewEdgeData_t *pCmpData = s_EdgeData.Element( i );
		if( !pCmpData )
			continue;

		// get the compare t-junction edge
		int ndxCmp = SewEdge_TJIndex( pCmpData, DISPSEW_EDGE_TJ );
		if( ndxCmp == -1 )
			continue;

		// get the compare face point count
		int ptCount2 = pCmpData->pFaces[ndxCmp]->GetPointCount();
		
		Vector edgePts2[2];
		GetPointFromSurface( pCmpData->pFaces[ndxCmp], pCmpData->ndxEdges[ndxCmp], edgePts2[0] );
		GetPointFromSurface( pCmpData->pFaces[ndxCmp], (pCmpData->ndxEdges[ndxCmp]+1)%ptCount2, edgePts2[1] );
		
		// compare the edges -- return if found
		int edgeType1, edgeType2;
		if( EdgeCompare( edgePts, edgePts2, edgeType1, edgeType2 ) )
		{
			bool bReturn = SewEdge_AddToListMerge( pEdgeData, pCmpData );
			SewEdge_Destroy( pEdgeData );
			return bReturn;
		}
	}

	// unique edge -- add it to the list
	s_EdgeData.AddToTail( pEdgeData );
	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewEdge_AddToListNormal( SewEdgeData_t *pEdgeData )
{
	// get the face point count
	int ptCount = pEdgeData->pFaces[0]->GetPointCount();

	// get the current edge (edge points)
	Vector edgePts[2];
	GetPointFromSurface( pEdgeData->pFaces[0], pEdgeData->ndxEdges[0], edgePts[0] );
	GetPointFromSurface( pEdgeData->pFaces[0], (pEdgeData->ndxEdges[0]+1)%ptCount, edgePts[1] );

	//
	// check to see if the edge already exists in the edge data list
	//
	int edgeCount = s_EdgeData.Size();
	for( int i = 0; i < edgeCount; i++ )
	{
		// get the edge points to compare against
		SewEdgeData_t *pCmpData = s_EdgeData.Element( i );
		if( !pCmpData )
			continue;

		// compare against each edge (all colinear) in struct
		for( int j = 0; j < pCmpData->faceCount; j++ )
		{
			// get the compare face point count
			int ptCount2 = pCmpData->pFaces[j]->GetPointCount();

			Vector edgePts2[2];
			GetPointFromSurface( pCmpData->pFaces[j], pCmpData->ndxEdges[j], edgePts2[0] );
			GetPointFromSurface( pCmpData->pFaces[j], (pCmpData->ndxEdges[j]+1)%ptCount2, edgePts2[1] );
		
			// compare the edges -- return if found
			int edgeType1, edgeType2;
			if( EdgeCompare( edgePts, edgePts2, edgeType1, edgeType2 ) )
			{
				SewEdge_Destroy( pEdgeData );
				return;
			}
		}
	}

	// unique edge -- add it to the list
	s_EdgeData.AddToTail( pEdgeData );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool SewEdge_AddToList( SewEdgeData_t *pEdgeData )
{
	// if this is a "normal" edge - handle it
	if( pEdgeData->type[0] == DISPSEW_EDGE_NORMAL )
	{
		SewEdge_AddToListNormal( pEdgeData );
		return true;
	}

	// this is a "t-junction" edge - handle it
	return SewEdge_AddToListTJunc( pEdgeData );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewEdge_Build( void )
{
	//
	// get the "faces" selection list (contains displaced and non-displaced faces)
	//
	CFaceEditSheet *pSheet = GetMainWnd()->GetFaceEditSheet();
	if( !pSheet )
		return;
	
	bool bError = false;

	//
	// for each face in list
	//
	int faceCount = pSheet->GetFaceListCount();

	for( int ndxFace = 0; ndxFace < faceCount; ndxFace++ )
	{
		// get the current face
		CMapFace *pFace = pSheet->GetFaceListDataFace( ndxFace );
		if( !pFace )
			continue;

		//
		// for each face edge
		//
		int ptCount = pFace->GetPointCount();
		for( int ndxPt = 0; ndxPt < ptCount; ndxPt++ )
		{
			// get the current edge points
			int type1_keep = 0;
			Vector edgePts[2];
			GetPointFromSurface( pFace, ndxPt, edgePts[0] );
			GetPointFromSurface( pFace, (ndxPt+1)%ptCount, edgePts[1] );

			// allocate new edge
			SewEdgeData_t *pEdgeData = SewEdge_Create();
			if( !pEdgeData )
				return;

			//
			// compare this edge to all of the other edges on all the other faces in the list
			//
			for( int ndxFace2 = 0; ndxFace2 < faceCount; ndxFace2++ )
			{
				// don't compare to itself
				if( ndxFace == ndxFace2 )
					continue;

				// get the current compare face
				CMapFace *pFace2 = pSheet->GetFaceListDataFace( ndxFace2 );
				if( !pFace2 )
					continue;

				//
				// for each compare face edge
				//
				int ptCount2 = pFace2->GetPointCount();
				for( int ndxPt2 = 0; ndxPt2 < ptCount2; ndxPt2++ )
				{
					// get the current compare edge point
					Vector edgePts2[2];
					GetPointFromSurface( pFace2, ndxPt2, edgePts2[0] );
					GetPointFromSurface( pFace2, (ndxPt2+1)%ptCount2, edgePts2[1] );

					// compare pt1 and pt2
					int type1, type2;
					if( EdgeCompare( edgePts, edgePts2, type1, type2 ) )
					{
						if (!SewEdge_Add( pEdgeData, pFace2, ndxPt2, type2 ))
						{
							bError = true;
						}
						type1_keep = type1;
					}
				}
			}

			// had neighbors -- add base point and add it to corner list
			if( pEdgeData->faceCount > 0 )
			{
				if (!SewEdge_Add( pEdgeData, pFace, ndxPt, type1_keep ))
				{
					bError = true;
				}

				if (!SewEdge_AddToList( pEdgeData ))
				{
					bError = true;
				}
			}
			// no neighbors -- de-allocate
			else
			{
				SewEdge_Destroy( pEdgeData );
			}
		}
	}

	if (bError)
	{
		AfxMessageBox("Not all selected faces could be sewn because too many selected faces share a single edge.\n\nLook for places where 3 or more selected faces (displacement or non-displacement) all share an edge.");
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewEdge_ResolveDispTJunc( SewEdgeData_t *pEdgeData, int ndxTJ, int ndxTJNeighbor, bool bStart )
{
	//
	// handle displacement sewing to displacement t-junction edge
	//
	EditDispHandle_t tjEdgeHandle = pEdgeData->pFaces[ndxTJ]->GetDisp();
	EditDispHandle_t edgeHandle = pEdgeData->pFaces[ndxTJNeighbor]->GetDisp();
	if( ( tjEdgeHandle == EDITDISPHANDLE_INVALID ) || ( edgeHandle == EDITDISPHANDLE_INVALID ) )
		return;

	CMapDisp *pTJEdgeDisp = EditDispMgr()->GetDisp( tjEdgeHandle );
	CMapDisp *pEdgeDisp = EditDispMgr()->GetDisp( edgeHandle );

	//
	// get the t-junction edge interval (or half of it)
	//
	int tjWidth = pTJEdgeDisp->GetWidth();
	int tjInterval = pTJEdgeDisp->GetWidth() / 2;

	//
	// get edge interval
	//
	int edgeWidth = pEdgeDisp->GetWidth();
	int edgeInterval = pEdgeDisp->GetWidth();

	int ratio = ( edgeInterval - 1 ) / tjInterval;

	bool bFlip = ( ratio < 1 );
	if( bFlip )
	{
		ratio = tjInterval / ( edgeInterval - 1 );
	}

	//
	// average the "like" points
	//
	if( bStart )
	{
		if( bFlip )
		{
			for( int i = 1, j = ratio; i < edgeInterval; i++, j += ratio )
			{
				int ndxTJPt, ndxEdgePt;
				ndxTJPt = GetEdgePointIndex( pTJEdgeDisp, pEdgeData->ndxEdges[ndxTJ], j, true );
				ndxEdgePt = GetEdgePointIndex( pEdgeDisp, pEdgeData->ndxEdges[ndxTJNeighbor], i, false );

				// average
				AverageVectorFieldData( pEdgeDisp, ndxEdgePt, pTJEdgeDisp, ndxTJPt );
			}
		}
		else
		{
			for( int i = 1, j = ratio; i < tjInterval; i++, j += ratio )
			{
				int ndxTJPt, ndxEdgePt;
				
				ndxTJPt = GetEdgePointIndex( pTJEdgeDisp, pEdgeData->ndxEdges[ndxTJ], i, true );
				ndxEdgePt = GetEdgePointIndex( pEdgeDisp, pEdgeData->ndxEdges[ndxTJNeighbor], j, false );
				
				// average
				AverageVectorFieldData( pEdgeDisp, ndxEdgePt, pTJEdgeDisp, ndxTJPt );
			}
		}
	}
	else
	{
		if( bFlip )
		{
			for( int i = 1, j = ratio; i < edgeWidth; i++, j += ratio )
			{
				int ndxTJPt, ndxEdgePt;
				ndxTJPt = GetEdgePointIndex( pTJEdgeDisp, pEdgeData->ndxEdges[ndxTJ], j, false );
				ndxEdgePt = GetEdgePointIndex( pEdgeDisp, pEdgeData->ndxEdges[ndxTJNeighbor], i, true );

				// average
				AverageVectorFieldData( pEdgeDisp, ndxEdgePt, pTJEdgeDisp, ndxTJPt );
			}
		}
		else
		{
			for( int i = ( tjInterval + 1 ), j = ratio; i < ( tjWidth - 1 ); i++, j += ratio )
			{
				int ndxTJPt, ndxEdgePt;
				ndxTJPt = GetEdgePointIndex( pTJEdgeDisp, pEdgeData->ndxEdges[ndxTJ], i, true );
				ndxEdgePt = GetEdgePointIndex( pEdgeDisp, pEdgeData->ndxEdges[ndxTJNeighbor], j, false );
				
				// average
				AverageVectorFieldData( pEdgeDisp, ndxEdgePt, pTJEdgeDisp, ndxTJPt );
			}
		}
	}

	//
	// linearly interpolate the "unlike" points
	//
	float blendRatio = 1.0f / ratio;
	
	if( bFlip )
	{
		for( int i = 0; i < ( tjWidth - ratio ); i += ratio )
		{
			int ndxStart = i;
			int ndxEnd = ( i + ratio );
			
			int ndxStartPt = GetEdgePointIndex( pTJEdgeDisp, pEdgeData->ndxEdges[ndxTJ], ndxStart, true );
			int ndxEndPt = GetEdgePointIndex( pTJEdgeDisp, pEdgeData->ndxEdges[ndxTJ], ndxEnd, true );
			
			for( int j = ( ndxStart + 1 ); j < ndxEnd; j++ )
			{
				float blend = blendRatio * ( j - ndxStart );
				
				int ndxDst = GetEdgePointIndex( pTJEdgeDisp, pEdgeData->ndxEdges[ndxTJ], j, true );
				
				BlendVectorFieldData( pTJEdgeDisp, ndxStartPt, ndxDst, pTJEdgeDisp, ndxEndPt, ndxDst, blend );
			}
		}
	}
	else
	{
		for( int i = 0; i < ( edgeWidth - ratio ); i += ratio )
		{
			int ndxStart = i;
			int ndxEnd = ( i + ratio );
			
			int ndxStartPt = GetEdgePointIndex( pEdgeDisp, pEdgeData->ndxEdges[ndxTJNeighbor], ndxStart, true );
			int ndxEndPt = GetEdgePointIndex( pEdgeDisp, pEdgeData->ndxEdges[ndxTJNeighbor], ndxEnd, true );
			
			for( int j = ( ndxStart + 1 ); j < ndxEnd; j++ )
			{
				float blend = blendRatio * ( j - ndxStart );
				
				int ndxDst = GetEdgePointIndex( pEdgeDisp, pEdgeData->ndxEdges[ndxTJNeighbor], j, true );
				
				BlendVectorFieldData( pEdgeDisp, ndxStartPt, ndxDst, pEdgeDisp, ndxEndPt, ndxDst, blend );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewEdge_ResolveSolidTJunc( SewEdgeData_t *pEdgeData, int type, bool bStart )
{
	// create an empty vector to reset the offset with
	Vector vClear( 0.0f, 0.0f, 0.0f );

	for( int i = 0; i < pEdgeData->faceCount; i++ )
	{	
		if( pEdgeData->type[i] != type )
			continue;

		// get the displacement surface associated with the face
		EditDispHandle_t dispHandle = pEdgeData->pFaces[i]->GetDisp();
		if( dispHandle == EDITDISPHANDLE_INVALID )
			continue;

		CMapDisp *pDisp = EditDispMgr()->GetDisp( dispHandle );

		// get surface normal, to reset vector field to base state
		Vector vNormal;
		pDisp->GetSurfNormal( vNormal );

		// reset tjstart and tjend
		if( type != DISPSEW_EDGE_TJ )
		{
			//
			// for all points along the edge -- reset
			//
			int width = pDisp->GetWidth();
			for( int j = 1; j < ( width - 1 ); j++ )
			{
				// get the edge point index
				int ndxPt = GetEdgePointIndex( pDisp, pEdgeData->ndxEdges[i], j, true );
				if( ndxPt == -1 )
					continue;
				
				//
				// reset displacement data (dist, field vector, and offset vector)
				//
				pDisp->SetFieldDistance( ndxPt, 0.0f );
				pDisp->SetFieldVector( ndxPt, vNormal );
				pDisp->SetSubdivPosition( ndxPt, vClear );
				pDisp->SetSubdivNormal( ndxPt, vNormal );
				pDisp->SetAlpha( ndxPt, 0.0f );
			}
		}
		// reset tj (upper and lower)
		else
		{
			//
			// for all points along the edge -- reset
			//
			int width = pDisp->GetWidth();
			int widthDiv2 = width / 2;

			if( bStart )
			{
				for( int j = 1; j < widthDiv2; j++ )
				{
					// get the edge point index
					int ndxPt = GetEdgePointIndex( pDisp, pEdgeData->ndxEdges[i], j, true );
					if( ndxPt == -1 )
						continue;
					
					//
					// reset displacement data (dist, field vector, and offset vector)
					//
					pDisp->SetFieldDistance( ndxPt, 0.0f );
					pDisp->SetFieldVector( ndxPt, vNormal );
					pDisp->SetSubdivPosition( ndxPt, vClear );
					pDisp->SetSubdivNormal( ndxPt, vNormal );
					pDisp->SetAlpha( ndxPt, 0.0f );
				}
			}
			else
			{
				for( int j = ( widthDiv2 + 1 ); j < ( width - 1 ); j++ )
				{
					// get the edge point index
					int ndxPt = GetEdgePointIndex( pDisp, pEdgeData->ndxEdges[i], j, true );
					if( ndxPt == -1 )
						continue;
					
					//
					// reset displacement data (dist, field vector, and offset vector)
					//
					pDisp->SetFieldDistance( ndxPt, 0.0f );
					pDisp->SetFieldVector( ndxPt, vNormal );
					pDisp->SetSubdivPosition( ndxPt, vClear );
					pDisp->SetSubdivNormal( ndxPt, vNormal );
					pDisp->SetAlpha( ndxPt, 0.0f );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewEdge_ResolveDispNormal( SewEdgeData_t *pEdgeData )
{
	//
	// get displacement surfaces -- if any
	//
	EditDispHandle_t handle1 = pEdgeData->pFaces[0]->GetDisp();
	EditDispHandle_t handle2 = pEdgeData->pFaces[1]->GetDisp();
	if( ( handle1 == EDITDISPHANDLE_INVALID ) || ( handle2 == EDITDISPHANDLE_INVALID ) )
		return;

	CMapDisp *pEdgeDisp1 = EditDispMgr()->GetDisp( handle1 );
	CMapDisp *pEdgeDisp2 = EditDispMgr()->GetDisp( handle2 );
	
	//
	// sew displacement edges
	//
	
	//
	// find displacement with smallest/largest interval
	//
	CMapDisp *pSmDisp, *pLgDisp;
	int smInterval, lgInterval;
	int ndxSmEdge, ndxLgEdge;
	
	if( pEdgeDisp1->GetWidth() > pEdgeDisp2->GetWidth() )
	{
		pSmDisp = pEdgeDisp2;
		ndxSmEdge = pEdgeData->ndxEdges[1];
		smInterval = pEdgeDisp2->GetWidth();
		
		pLgDisp = pEdgeDisp1;
		ndxLgEdge = pEdgeData->ndxEdges[0];
		lgInterval = pEdgeDisp1->GetWidth();
	}
	else
	{
		pSmDisp = pEdgeDisp1;
		ndxSmEdge = pEdgeData->ndxEdges[0];
		smInterval = pEdgeDisp1->GetWidth();
		
		pLgDisp = pEdgeDisp2;
		ndxLgEdge = pEdgeData->ndxEdges[1];
		lgInterval = pEdgeDisp2->GetWidth();
	}
	
	// calculate the ratio
	int ratio = ( lgInterval - 1 ) / ( smInterval - 1 );
	
	//
	// average "like" points
	//
	for( int ndxSm = 1, ndxLg = ratio; ndxSm < ( smInterval - 1 ); ndxSm++, ndxLg += ratio )
	{
		int ndxSmPt = GetEdgePointIndex( pSmDisp, ndxSmEdge, ndxSm, true );
		int ndxLgPt = GetEdgePointIndex( pLgDisp, ndxLgEdge, ndxLg, false );
		
		// average
		AverageVectorFieldData( pSmDisp, ndxSmPt, pLgDisp, ndxLgPt );
	}
	
	//
	// linearly interpolate the "unlike" points
	//
	float blendRatio = 1.0f / ratio;
	
	for( int ndxLg = 0; ndxLg < ( lgInterval - 1 ); ndxLg += ratio )
	{
		int ndxStart = ndxLg;
		int ndxEnd = ( ndxLg + ratio );
		
		int ndxStartPt = GetEdgePointIndex( pLgDisp, ndxLgEdge, ndxStart, true );
		int ndxEndPt = GetEdgePointIndex( pLgDisp, ndxLgEdge, ndxEnd, true );
		
		for( int ndx = ( ndxStart + 1 ); ndx < ndxEnd; ndx++ )
		{
			float blend = blendRatio * ( ndx - ndxStart );
			int ndxDst = GetEdgePointIndex( pLgDisp, ndxLgEdge, ndx, true );
			BlendVectorFieldData( pLgDisp, ndxStartPt, ndxDst, pLgDisp, ndxEndPt, ndxDst, blend );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewEdge_ResolveSolidNormal( SewEdgeData_t *pEdgeData )
{
	// create an empty vector to reset the offset with
	Vector vClear( 0.0f, 0.0f, 0.0f );

	for( int i = 0; i < pEdgeData->faceCount; i++ )
	{
		// get the displacement surface associated with the face
		EditDispHandle_t handle = pEdgeData->pFaces[i]->GetDisp();
		if( handle == EDITDISPHANDLE_INVALID )
			continue;

		CMapDisp *pDisp = EditDispMgr()->GetDisp( handle );

		// get surface normal, to reset vector field to base state
		Vector vNormal;
		pDisp->GetSurfNormal( vNormal );

		//
		// for all points along the edge -- reset
		//
		int width = pDisp->GetWidth();
		for( int j = 0; j < width; j++ )
		{
			// get the edge point index
			int ndxPt = GetEdgePointIndex( pDisp, pEdgeData->ndxEdges[i], j, true );
			if( ndxPt == -1 )
				continue;
			
			//
			// reset displacement data (dist, field vector, and offset vector)
			//
			pDisp->SetFieldDistance( ndxPt, 0.0f );
			pDisp->SetFieldVector( ndxPt, vClear );
			pDisp->SetSubdivPosition( ndxPt, vClear );
			pDisp->SetSubdivNormal( ndxPt, vNormal );
			pDisp->SetAlpha( ndxPt, 0.0f );
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SewEdge_Resolve( void )
{
	// get the number of edges in the edge list
	int edgeCount = s_EdgeData.Size();

	// resolve each edge
	for( int i = 0; i < edgeCount; i++ )
	{
		// get the current edge data struct
		SewEdgeData_t *pEdgeData = s_EdgeData.Element( i );
		if( !pEdgeData )
			continue;

		// handle "normal" edge
		if( pEdgeData->type[0] == DISPSEW_EDGE_NORMAL )
		{
			// solid "normal" edge
			if( SewEdge_IsSolidNormal( pEdgeData ) )
			{
				SewEdge_ResolveSolidNormal( pEdgeData );
			}
			// disps "normal" edge
			else
			{
				SewEdge_ResolveDispNormal( pEdgeData );
			}
		}
		// handle "t-junction" edge
		else
		{
			int ndxTJ = SewEdge_TJIndex( pEdgeData, DISPSEW_EDGE_TJ );
			int ndxTJStart = SewEdge_TJIndex( pEdgeData, DISPSEW_EDGE_TJSTART );
			int ndxTJEnd = SewEdge_TJIndex( pEdgeData, DISPSEW_EDGE_TJEND );

			if( SewEdge_IsSolidTJunc( pEdgeData, DISPSEW_EDGE_TJ ) )
			{
				// reset both start and end t-junction edges if they exist
				if( ndxTJStart != -1 )
				{	
					SewEdge_ResolveSolidTJunc( pEdgeData, DISPSEW_EDGE_TJSTART, false );
				}

				if( ndxTJEnd != -1 )
				{
					SewEdge_ResolveSolidTJunc( pEdgeData, DISPSEW_EDGE_TJEND, false );
				}

				continue;
			}

			// handle start edge
			if( ndxTJStart != -1 )
			{
				if( SewEdge_IsSolidTJunc( pEdgeData, DISPSEW_EDGE_TJSTART ) )
				{
					SewEdge_ResolveSolidTJunc( pEdgeData, DISPSEW_EDGE_TJ, true );
				}
				else
				{
					SewEdge_ResolveDispTJunc( pEdgeData, ndxTJ, ndxTJStart, true );
				}
			}

			// handle end edge
			if( ndxTJEnd != -1 )
			{
				if( SewEdge_IsSolidTJunc( pEdgeData, DISPSEW_EDGE_TJEND ) )
				{
					SewEdge_ResolveSolidTJunc( pEdgeData, DISPSEW_EDGE_TJ, false );
				}
				else
				{
					SewEdge_ResolveDispTJunc( pEdgeData, ndxTJ, ndxTJEnd, false );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Convert the edge/midpoint/corner data for shared code,
//-----------------------------------------------------------------------------
bool PrePlanarizeDependentVerts( void )
{
	// Create a list of all the selected displacement cores.
	CFaceEditSheet *pSheet = GetMainWnd()->GetFaceEditSheet();
	if( !pSheet )
		return false;

	int nFaceCount = pSheet->GetFaceListCount();
	for( int iFace = 0; iFace < nFaceCount; ++iFace )
	{
		CMapFace *pFace = pSheet->GetFaceListDataFace( iFace );
		if( !pFace || !pFace->HasDisp() )
			continue;

		CMapDisp *pDisp = EditDispMgr()->GetDisp( pFace->GetDisp() );
		Assert( pDisp );

		int iDisp = m_aCoreDispInfos.AddToTail();
		pDisp->GetCoreDispInfo()->SetListIndex( iDisp );
		m_aCoreDispInfos[iDisp] = pDisp->GetCoreDispInfo();
	}

	// Add the list to the displacements -- this is a bit hacky!!
	for ( int iDisp = 0; iDisp < m_aCoreDispInfos.Count(); ++iDisp )
	{
		m_aCoreDispInfos[iDisp]->SetDispUtilsHelperInfo( m_aCoreDispInfos.Base(), m_aCoreDispInfos.Count() );
	}

	// Build neighboring info.
	FindNeighboringDispSurfs( m_aCoreDispInfos.Base(), m_aCoreDispInfos.Count() );

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CHammerTesselateHelper : public CBaseTesselateHelper
{
public:

	void EndTriangle()
	{
		m_pIndices->AddToTail( m_TempIndices[0] );
		m_pIndices->AddToTail( m_TempIndices[1] );
		m_pIndices->AddToTail( m_TempIndices[2] );
	}

	DispNodeInfo_t& GetNodeInfo( int iNodeBit )
	{
		// Hammer doesn't care about these. Give it back something to play with.
		static DispNodeInfo_t dummy;
		return dummy;
	}
	
public:

	CUtlVector<unsigned short> *m_pIndices;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool FindEnclosingTri( const Vector2D &vert, CUtlVector<Vector2D> &vertCoords,
	                   CUtlVector<unsigned short> &indices, int *pStartVert,
					   float bcCoords[3] )
{
	for ( int i = 0; i < indices.Count(); i += 3 )
	{
		GetBarycentricCoords2D( vertCoords[indices[i+0]],
			                    vertCoords[indices[i+1]],
			                    vertCoords[indices[i+2]],
			                    vert, bcCoords );

		if ( bcCoords[0] >= 0 && bcCoords[0] <= 1 && 
			 bcCoords[1] >= 0 && bcCoords[1] <= 1 && 
			 bcCoords[2] >= 0 && bcCoords[2] <= 1 )
		{
			*pStartVert = i;
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void SnapDependentVertsToSurface( CCoreDispInfo *pCoreDisp )
{
	// Don't really want to do this, but.......
	CUtlVector<unsigned short> indices;
	CHammerTesselateHelper helper;
	helper.m_pIndices = &indices;
	helper.m_pActiveVerts = pCoreDisp->GetAllowedVerts().Base();
	helper.m_pPowerInfo = pCoreDisp->GetPowerInfo();
	TesselateDisplacement( &helper );

	// Find allowed/non-allowed verts.
	CUtlVector<bool> vertsTouched;
	vertsTouched.SetSize( pCoreDisp->GetSize() );
	memset( vertsTouched.Base(), 0, sizeof( bool ) * vertsTouched.Count() );
	for ( int iVert = 0; iVert < indices.Count(); ++iVert )
	{
		vertsTouched[indices[iVert]] = true;
	}

	// Generate 2D floating point coordinates for each vertex. We use these to generate
	// barycentric coordinates, and the scale doesn't matter.
	CUtlVector<Vector2D> vertCoords;
	vertCoords.SetSize( pCoreDisp->GetSize() );
	for ( int iHgt = 0; iHgt < pCoreDisp->GetHeight(); ++iHgt )
	{
		for ( int iWid = 0; iWid < pCoreDisp->GetWidth(); ++iWid )
		{
			vertCoords[iHgt*pCoreDisp->GetWidth()+iWid].Init( iWid, iHgt );
		}
	}
	
	// Now, for each vert not touched, snap its position to the main surface.
	for ( int iHgt = 0; iHgt < pCoreDisp->GetHeight(); ++iHgt )
	{
		for ( int iWid = 0; iWid < pCoreDisp->GetWidth(); ++iWid )
		{
			int nIndex = iHgt * pCoreDisp->GetWidth() + iWid;
			if ( !( vertsTouched[nIndex] ) )
			{
				float flBCoords[3];
				int iStartVert = -1;

				if ( FindEnclosingTri( vertCoords[nIndex], vertCoords, indices, &iStartVert, flBCoords ) )
				{
					const Vector &A = pCoreDisp->GetVert( indices[iStartVert+0] );
					const Vector &B = pCoreDisp->GetVert( indices[iStartVert+1] );
					const Vector &C = pCoreDisp->GetVert( indices[iStartVert+2] );
					Vector vNewPos = A*flBCoords[0] + B*flBCoords[1] + C*flBCoords[2];

					// Modify the CCoreDispInfo vert (although it probably won't be used later).
					pCoreDisp->Position_Update( nIndex, vNewPos );
				}
				else
				{
					// This shouldn't happen because it would mean that the triangulation that 
					// disp_tesselation.h produced was missing a chunk of the space that the
					// displacement covers. 
					// It also could indicate a floating-point epsilon error.. check to see if
					// FindEnclosingTri finds a triangle that -almost- encloses the vert.
					Assert( false );
				}
			}
		}
	} 
}

//-----------------------------------------------------------------------------
// Purpose: Get allowed verts bits and planarize cleared verts and purge disp
//          infos.
//-----------------------------------------------------------------------------
void PostPlanarizeDependentVerts( void )
{
	// Snap dependents verts to the displacement surface.
	for ( int iDispCore = 0; iDispCore < m_aCoreDispInfos.Count(); ++iDispCore )
	{
		SnapDependentVertsToSurface( m_aCoreDispInfos[iDispCore] );
	}
		
	// Clear out the displacement info list.
	m_aCoreDispInfos.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Planarize vertices that are removed because of dependencies with
//          neighboring displacements.
//-----------------------------------------------------------------------------
void PlanarizeDependentVerts( void )
{
	// Setup.
	if ( !PrePlanarizeDependentVerts() )
		return;

	SetupAllowedVerts( m_aCoreDispInfos.Base(), m_aCoreDispInfos.Count() );

	// Update and clean-up.
	PostPlanarizeDependentVerts();
}
