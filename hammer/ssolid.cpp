//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Structured Solid (CSSolid) implementation.
//
// Method of identifying different parts of solid (vertices/edges/faces) is 
// a unique-id system. The AddFace/AddEdge/AddVertex functions assign each
// new "part" an id using GetNewID(). External objects referencing the CSSolid
// do not have to worry about keeping track of indices into the private 
// arrays, since an ID is valid only if the part still exists. To get 
// information about an ID, use the GetHandleInfo() function -> it returns
// FALSE if the given ID is no longer valid.
//
//=============================================================================

#include "stdafx.h"
#include "BrushOps.h"
#include "GameConfig.h"
#include "MapSolid.h"
#include "MapWorld.h"
#include "SSolid.h"
#include "StockSolids.h"
#include "Options.h"
#include "WorldSize.h"
#include "mapdisp.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

BOOL CheckFace(Vector *Points, int nPoints, Vector* pNormal, float dist, CCheckFaceInfo *pInfo)
{
	int		 j;
	float	d, edgedist;
	Vector	dir, edgenormal;

	if(!pInfo)
	{
		static CCheckFaceInfo dummyinfo;
		pInfo = &dummyinfo;
		pInfo->iPoint = -1;	// make sure it's reset to default
	}

	if(pInfo->iPoint == -2)
		return TRUE;	// stop!!!!!

	// do we need to create a normal?
	if(!pNormal)
	{
		static Vector _normal;
		pNormal = &_normal;

		// calc a plane from the points
		Vector t1, t2, t3;

		for(int i = 0; i < 3; i++)
		{
			t1[i] = Points[0][i] - Points[1][i];
			t2[i] = Points[2][i] - Points[1][i];
			t3[i] = Points[1][i];
		}

		CrossProduct(t1, t2, *pNormal);
		VectorNormalize(*pNormal);
		dist = DotProduct(t3, *pNormal);
	}

	if(!nPoints)
	{
		strcpy(pInfo->szDescription, "no points");
		pInfo->iPoint = -2;
		return FALSE;
	}

	if(nPoints < 3)
	{
		strcpy(pInfo->szDescription, "fewer than three points");
		pInfo->iPoint = -2;
		return FALSE;
	}

	for(int i = pInfo->iPoint + 1; i < nPoints; i++ )
	{
		pInfo->iPoint = i;

		Vector& p1 = Points[i];

		for (j=0 ; j<3 ; j++)
		{
			if (p1[j] > MAX_COORD_INTEGER || p1[j] < MIN_COORD_INTEGER)
			{
				strcpy(pInfo->szDescription, "out of range");
				return FALSE;
			}
		}

		// check the point is on the face plane
		d = DotProduct (p1, *pNormal) - dist;
		if (d < -ON_PLANE_EPSILON || d > ON_PLANE_EPSILON)
		{
			strcpy(pInfo->szDescription, "point off plane");
			return FALSE;
		}

		// check the edge isn't degenerate
		Vector& p2 = Points[i+1 == nPoints ? 0 : i+1];	// (next point)
		VectorSubtract (p2, p1, dir);

		if (VectorLength (dir) < MIN_EDGE_LENGTH_EPSILON)
		{
			strcpy(pInfo->szDescription, "edge is too small");
			return FALSE;
		}

		CrossProduct(*pNormal, dir, edgenormal);
		VectorNormalize (edgenormal);
		edgedist = DotProduct(p1, edgenormal);
		edgedist += ON_PLANE_EPSILON;

		// all other points must be on front side
		for (j=0 ; j< nPoints; j++)
		{
			if (j == i)
				continue;
			d = DotProduct (Points[j], edgenormal);
			if (d > edgedist)
			{
				strcpy(pInfo->szDescription, "face is not convex");
				return FALSE;
			}
		}
	}

	pInfo->iPoint = -2;
	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CSSolid::CSSolid()
{
	m_nVertices = 0;
	m_nEdges = 0;
	m_nFaces = 0;
	m_curid = 1;
	m_pMapSolid = NULL;
	m_bShowVertices = TRUE;
	m_bShowEdges = TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CSSolid::~CSSolid()
{
	memset(this, 0, sizeof(this));
}


SSHANDLE CSSolid::GetNewID()
{
	return m_curid++;
}


PVOID CSSolid::GetHandleData(SSHANDLE id)
{
	SSHANDLEINFO hi;
	if(!GetHandleInfo(&hi, id))
		return NULL;
	return hi.pData;
}


BOOL CSSolid::GetHandleInfo(SSHANDLEINFO *pInfo, SSHANDLE id)
{
	// try vertices .. 
	for(int i = 0; i < m_nVertices; i++)
	{
		if(m_Vertices[i].id != id)
			continue;	// not this one

		pInfo->Type = shtVertex;
		pInfo->iIndex = i;
		pInfo->pData = PVOID(& m_Vertices[i]);
		pInfo->p2DHandle = & m_Vertices[i];
		pInfo->pos = m_Vertices[i].pos;

		return TRUE;
	}

	// try edges .. 
	for(int i = 0; i < m_nEdges; i++)
	{
		if(m_Edges[i].id != id)
			continue;	// not this one

		pInfo->Type = shtEdge;
		pInfo->iIndex = i;
		pInfo->pData = PVOID(& m_Edges[i]);
		pInfo->p2DHandle = & m_Edges[i];
		pInfo->pos = m_Edges[i].ptCenter;

		return TRUE;
	}

	// try faces ..
	for(int i = 0; i < m_nFaces; i++)
	{
		if(m_Faces[i].id != id)
			continue;	// not this one

		pInfo->Type = shtFace;
		pInfo->iIndex = i;
		pInfo->pData = PVOID(& m_Faces[i]);
		pInfo->p2DHandle = & m_Faces[i];
		pInfo->pos = m_Faces[i].ptCenter;

		return TRUE;
	}

	pInfo->Type = shtNothing;
	return FALSE;
}


// Find data functions ->
int CSSolid::GetEdgeIndex(SSHANDLE v1, SSHANDLE v2)
{
	for(int i = 0; i < m_nEdges; i++)
	{
		CSSEdge & theEdge = m_Edges[i];
		if((theEdge.hvStart == v1 && theEdge.hvEnd == v2) ||
			(theEdge.hvStart == v2 && theEdge.hvEnd == v1))
		{
			return i;
		}
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Point - 
//			fLeniency - 
// Output : 
//-----------------------------------------------------------------------------
int CSSolid::GetEdgeIndex(const Vector &Point, float fLeniency)
{
	for (int i = 0; i < m_nEdges; i++)
	{
		Vector ptEdgeCenter = m_Edges[i].ptCenter;

		float fDiff = 0.0f;
		for (int j = 0; j < 3; j++)
		{
			fDiff += (Point[j] - ptEdgeCenter[j]) * (Point[j] - ptEdgeCenter[j]);
		}

		if (fDiff > fLeniency * fLeniency)
		{
			continue;
		}
		
		// if we are here, the 3 axes compare ok.
		return i;
	}

	// no edge matches
	return -1;
}


int CSSolid::GetVertexIndex(const Vector &Point, float fLeniency)
{
	for(int i = 0; i < m_nVertices; i++)
	{
		Vector Vertex = m_Vertices[i].pos;

		float fDiff = 0.0f;
		for(int j = 0; j < 3; j++)
		{
			fDiff += (Point[j] - Vertex[j]) * (Point[j] - Vertex[j]);
		}

		if (fDiff > (fLeniency*fLeniency))
			continue;
			
		// if we are here, the 3 axes compare ok.
		return i;
	}

	// no vertex matches.
	return -1;
}


int CSSolid::GetFaceIndex(const Vector &Point, float fLeniency)
{
	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: Calculates the center of an edge.
// Input  : pEdge - 
//-----------------------------------------------------------------------------
void CSSolid::CalcEdgeCenter(CSSEdge *pEdge)
{
	SSHANDLEINFO hi;

	GetHandleInfo(&hi, pEdge->hvStart);
	Vector &pt1 = m_Vertices[hi.iIndex].pos;

	GetHandleInfo(&hi, pEdge->hvEnd);
	Vector &pt2 = m_Vertices[hi.iIndex].pos;

	for (int i = 0; i < 3; i++)
	{
		pEdge->ptCenter[i] = (pt1[i] + pt2[i]) / 2.0f;
	}
}

// get common vertex from two edges ->

SSHANDLE CSSolid::GetConnectionVertex(CSSEdge *pEdge1, CSSEdge *pEdge2)
{
	if((pEdge1->hvStart == pEdge2->hvStart) || 
		(pEdge1->hvStart == pEdge2->hvEnd))
		return pEdge1->hvStart;

	if((pEdge1->hvEnd == pEdge2->hvStart) || 
		(pEdge1->hvEnd == pEdge2->hvEnd))
		return pEdge1->hvEnd;

	return 0;
}


// Create list of points from face ->
Vector * CSSolid::CreatePointList(CSSFace & face)
{
	Vector * pts = new Vector[face.nEdges+1];

	for(int i = 0; i < face.nEdges; i++)
	{
		// calc next edge so we can see which is the next clockwise point
		int iNextEdge = i+1;
		if(iNextEdge == face.nEdges)
			iNextEdge = 0;

		CSSEdge * edgeCur = (CSSEdge*) GetHandleData(face.Edges[i]);
		CSSEdge * edgeNext = (CSSEdge*) GetHandleData(face.Edges[iNextEdge]);

		if(!edgeCur || !edgeNext)
		{
			CString str;
			str.Format("Conversion error!\n"
				"edgeCur = %p, edgeNext = %p", edgeCur, edgeNext);
			AfxMessageBox(str);
			return NULL;
		}

		SSHANDLE hVertex = GetConnectionVertex(edgeCur, edgeNext);

		if(!hVertex)
		{
			CString str;
			str.Format("Conversion error!\n"
				"hVertex = %08X", hVertex);
			AfxMessageBox(str);
			return NULL;
		}

		CSSVertex *pVertex = (CSSVertex*) GetHandleData(hVertex);

		pts[i] = pVertex->pos;
	}

	return pts;
}


// Create point list, but return indices instead of positions ->
PINT CSSolid::CreatePointIndexList(CSSFace & face, PINT piPoints)
{
	PINT pts;
	if(piPoints)
		pts = piPoints;
	else
		pts = new int[face.nEdges+1];

	SSHANDLEINFO hi;

	for(int i = 0; i < face.nEdges; i++)
	{
		// calc next edge so we can see which is the next clockwise point
		int iNextEdge = i+1;
		if(iNextEdge == face.nEdges)
			iNextEdge = 0;

		CSSEdge * edgeCur = (CSSEdge*) GetHandleData(face.Edges[i]);
		CSSEdge * edgeNext = (CSSEdge*)  GetHandleData(face.Edges[iNextEdge]);

		SSHANDLE hVertex = GetConnectionVertex(edgeCur, edgeNext);
		Assert(hVertex);

		GetHandleInfo(&hi, hVertex);
		pts[i] = hi.iIndex;
	}

	return pts;
}

// Create point list, and use handles ->

SSHANDLE* CSSolid::CreatePointHandleList(CSSFace & face, SSHANDLE* phPoints)
{
	SSHANDLE* pts;
	if(phPoints)
		pts = phPoints;
	else
		pts = new SSHANDLE[face.nEdges+1];

	for(int i = 0; i < face.nEdges; i++)
	{
		// calc next edge so we can see which is the next clockwise point
		int iNextEdge = i+1;
		if(iNextEdge == face.nEdges)
			iNextEdge = 0;

		CSSEdge * edgeCur = (CSSEdge*) GetHandleData(face.Edges[i]);
		CSSEdge * edgeNext = (CSSEdge*) GetHandleData(face.Edges[iNextEdge]);

		SSHANDLE hVertex = GetConnectionVertex(edgeCur, edgeNext);
		Assert(hVertex);

		pts[i] = hVertex;
	}

	return pts;
}


void CSSolid::Attach(CMapSolid *pMapSolid)
{
	m_pMapSolid = pMapSolid;
}


CMapSolid *CSSolid::Detach()
{
	CMapSolid *pTmp = m_pMapSolid;
	m_pMapSolid = NULL;
	return pTmp;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not the SSolid has displacements.
//-----------------------------------------------------------------------------
bool CSSolid::HasDisps( void )
{
	for ( int iFace = 0; iFace < m_nFaces; ++iFace )
	{
		CSSFace *pFace = &m_Faces[iFace];
		if ( pFace->m_hDisp != EDITDISPHANDLE_INVALID )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Check to see if the SSolid with displacement surfaces has valid 
//          base face surfaces.
//-----------------------------------------------------------------------------
bool CSSolid::IsValidWithDisps( void )
{
	if ( !HasDisps() )
		return true;

	for ( int iFace = 0; iFace < m_nFaces; ++iFace )
	{
		// Get the face(s) that have displacements.
		CSSFace *pFace = &m_Faces[iFace];
		if ( pFace->m_hDisp == EDITDISPHANDLE_INVALID )
			continue;

		// Create a face point list.
		Vector *pFacePoints = CreatePointList( *pFace );

		// If the face has changed the number of points - via merges, etc.
		if ( pFace->nEdges != 4 )
			return false;

		// Check the face for validity.
		CCheckFaceInfo faceInfo;
		if ( !CheckFace( pFacePoints, pFace->nEdges, NULL, 0.0f, &faceInfo ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Destroy all the displacement data on the SSolid.
//-----------------------------------------------------------------------------
void CSSolid::DestroyDisps( void )
{
	for ( int iFace = 0; iFace < m_nFaces; ++iFace )
	{
		CSSFace *pFace = &m_Faces[iFace];
		if ( pFace->m_hDisp != EDITDISPHANDLE_INVALID )
		{
			EditDispMgr()->Destroy( pFace->m_hDisp );
			pFace->m_hDisp = EDITDISPHANDLE_INVALID;
		}
	}
}

void CSSolid::Convert(BOOL bFromMap, bool bSkipDisplacementFaces )
{
	if(bFromMap)
		FromMapSolid(NULL, bSkipDisplacementFaces);
	else
		ToMapSolid();
}


void CSSolid::ToMapSolid(CMapSolid *p)
{
	// so we can pass NULL (default) or another solid (to copy):
	CMapSolid *pSolid;
	if (p)
	{
		pSolid = p;
	}
	else
	{
		pSolid = m_pMapSolid;
	}

	pSolid->SetFaceCount(m_nFaces);

	unsigned char r, g, b;
	pSolid->GetRenderColor( r,g,b );

	for (int i = 0; i < m_nFaces; i++)
	{
		CSSFace &pFace = m_Faces[i];
		CMapFace SolidFace;

		//
		// Copy original texture information and face ID back.
		//
		Q_memcpy(&SolidFace.texture, &pFace.texture, sizeof(TEXTURE));
		SolidFace.SetTexture(SolidFace.texture.texture);
		SolidFace.SetFaceID(pFace.m_nFaceID);

		//
		// Create face from new points.
		//
		Vector *pts = CreatePointList(pFace);
		
		SolidFace.CreateFace(pts, pFace.nEdges);

		//
		// Vertex manipulation; the face orientation may have changed. If one of the texture axes is now
		// perpendicular to the face, recalculate the texture axes using the default alignment (world or face).
		// Ideally we would transform the texture axes so that their orientation relative to the face is preserved.
		// By reinitializing the axes we risk having the axes rotate unpredictably.
		//
		if (!SolidFace.IsTextureAxisValid())
		{
			SolidFace.InitializeTextureAxes(Options.GetTextureAlignment(), INIT_TEXTURE_AXES | INIT_TEXTURE_FORCE);
		}
			
		// Attempt to update the displacement - if there is one.
		if ( pFace.m_hDisp != EDITDISPHANDLE_INVALID )
		{
			EditDispHandle_t hDisp = EditDispMgr()->Create();
			CMapDisp *pSolidDisp = EditDispMgr()->GetDisp( hDisp );
			CMapDisp *pDisp = EditDispMgr()->GetDisp( pFace.m_hDisp );
			pSolidDisp->CopyFrom( pDisp, false );
			int iStart = pSolidDisp->GetSurfPointStartIndex();
			pSolidDisp->SetSurfPointStartIndex( (iStart+3)%4 );
			pSolidDisp->InitDispSurfaceData( &SolidFace, false );
			pSolidDisp->Create();
			SolidFace.SetDisp( hDisp );
		}

		CMapFace *pNewFace = pSolid->GetFace( i );
		pNewFace->CopyFrom( &SolidFace, COPY_FACE_POINTS);
		pNewFace->SetRenderColor(r, g, b);
		pNewFace->SetParent(pSolid);

		delete[] pts;
	}

	pSolid->PostUpdate(Notify_Changed);
}


CSSFace* CSSolid::AddFace(int* piNewIndex)
{
	m_Faces.SetCount(++m_nFaces);
	if(piNewIndex)
		piNewIndex[0] = m_nFaces-1;
	CSSFace *pFace = & m_Faces[m_nFaces-1];
	pFace->id = GetNewID();
	return pFace;
}

// Add Edge ->

CSSEdge* CSSolid::AddEdge(int* piNewIndex)
{
	m_Edges.SetCount(++m_nEdges);
	if(piNewIndex)
		piNewIndex[0] = m_nEdges-1;
	CSSEdge *pEdge = & m_Edges[m_nEdges-1];
	pEdge->id = GetNewID();
	return pEdge;
}

// Add Vertex ->

CSSVertex* CSSolid::AddVertex(int* piNewIndex)
{
	m_Vertices.SetCount(++m_nVertices);
	if(piNewIndex)
		piNewIndex[0] = m_nVertices-1;
	CSSVertex *pVertex = & m_Vertices[m_nVertices-1];
	pVertex->id = GetNewID();
	return pVertex;
}

// Assign a face to an edge ->

void CSSolid::AssignFace(CSSEdge* pEdge, SSHANDLE hFace, BOOL bRemove)
{
	if(!bRemove)
	{
		if(pEdge->Faces[0] == 0 || pEdge->Faces[0] == hFace)
			pEdge->Faces[0] = hFace;
		else if(pEdge->Faces[1] == 0)
			pEdge->Faces[1] = hFace;
	}
	else
	{
		if(pEdge->Faces[0] == hFace)
			pEdge->Faces[0] = 0;
		if(pEdge->Faces[1] == hFace)
			pEdge->Faces[1] = 0;
	}
}

// Convert From Map Solid ->

void CSSolid::FromMapSolid(CMapSolid *p, bool bSkipDisplacementFaces)
{
	// so we can pass NULL (default) or another solid (to copy):
	CMapSolid *pSolid;
	if(p)
		pSolid = p;
	else
		pSolid = m_pMapSolid;

	m_nFaces = 0;
	m_nEdges = 0;
	m_nVertices = 0;

	// Create vertices, edges, faces.
	int nSolidFaces = pSolid->GetFaceCount();
	for(int i = 0; i < nSolidFaces; i++)
	{
		CMapFace *pSolidFace = pSolid->GetFace(i);

		if (bSkipDisplacementFaces)
		{
			if (pSolidFace->HasDisp())
				continue;
		}

		// Add a face
		CSSFace *pFace = AddFace();

		memcpy(pFace->PlanePts, pSolidFace->plane.planepts, sizeof(Vector) * 3);
		pFace->texture = pSolidFace->texture;
		pFace->normal = pSolidFace->plane.normal;
		pFace->m_nFaceID = pSolidFace->GetFaceID();

		// Displacement.
		if ( pSolidFace->HasDisp() )
		{
			pFace->m_hDisp = EditDispMgr()->Create();
			CMapDisp *pDisp = EditDispMgr()->GetDisp( pFace->m_hDisp );
			CMapDisp *pSolidDisp = EditDispMgr()->GetDisp( pSolidFace->GetDisp() );
			pDisp->CopyFrom( pSolidDisp, false );
		}

		// Convert vertices and edges
		int nFacePoints = pSolidFace->nPoints;
		Vector *pFacePoints = pSolidFace->Points;
		SSHANDLE hLastVertex = 0;	// valid IDs start at 1
		SSHANDLE hThisVertex, hFirstVertex = 0;
		for(int pt = 0; pt <= nFacePoints; pt++)
		{
			int iVertex;
			
			if(pt < nFacePoints)
			{
				// YWB:  Change leniency from 1.0 down to 0.1
				iVertex = GetVertexIndex(pFacePoints[pt], 0.1f);
				if (iVertex == -1)
				{
					// not found - add the vertex
					CSSVertex *pVertex = AddVertex(&iVertex);
					pVertex->pos = pFacePoints[pt];
				}

				// assign this vertex handle
				hThisVertex = m_Vertices[iVertex].id;

				if (pt == 0)
					hFirstVertex = hThisVertex;
			}
			else
			{
				// connect last to first
				hThisVertex = hFirstVertex;
			}

			if (hLastVertex)
			{
				// create the edge from the last vertex to current vertex.
				//  first check to see if this edge already exists.. 
				int iEdge = GetEdgeIndex(hLastVertex, hThisVertex);
				CSSEdge *pEdge;
				if (iEdge == -1)
				{
					// not found - add new edge
					pEdge = AddEdge(&iEdge);
					pEdge->hvStart = hLastVertex;
					pEdge->hvEnd   = hThisVertex;

					// make sure edge center is valid:
					CalcEdgeCenter(pEdge);
				}
				else
				{
					pEdge = &m_Edges[iEdge];
				}

				// add the edge to the face
				pFace->Edges[pFace->nEdges++] = pEdge->id;

				// set edge's face array
				if(!pEdge->Faces[0])
					pEdge->Faces[0] = pFace->id;
				else if(!pEdge->Faces[1])
					pEdge->Faces[1] = pFace->id;
				else
				{
					// YWB try filling in front side
					//  rather than Assert(0) crash
					pEdge->Faces[0] = pFace->id;
					AfxMessageBox("Edge with both face id's already filled, skipping...");
				}
			}

			hLastVertex = hThisVertex;
		}
	}
}

// Find edges that reference a vertex ->

CSSEdge ** CSSolid::FindAffectedEdges(SSHANDLE *pHandles, int iNumHandles, int& iNumEdges)
{
	static CSSEdge *ppEdges[128];
	iNumEdges = 0;

	for(int h = 0; h < iNumHandles; h++)
	{
		for(int i = 0; i < m_nEdges; i++)
		{
			CSSEdge *pEdge = &m_Edges[i];
			if(pEdge->hvStart == pHandles[h] || 
				pEdge->hvEnd == pHandles[h])
			{
				// ensure it's not already stored
				int s;
				for(s = 0; s < iNumEdges; s++)
				{
					if(ppEdges[s] == pEdge)
						break;
				}
				if(s == iNumEdges)
					ppEdges[iNumEdges++] = pEdge;
			}
		}
	}

	return ppEdges;
}


// tell drawing code to show/hide kinds of handles
void CSSolid::ShowHandles(BOOL bShowVertices, BOOL bShowEdges)
{
	m_bShowEdges = bShowEdges;
	m_bShowVertices = bShowVertices;
}


// Move handle(s) to a new location ->
void CSSolid::MoveSelectedHandles(const Vector &Delta)
{
	SSHANDLE MoveVertices[128];
	int nMoveVertices = 0;

	SSHANDLEINFO hi;
	
	for(int i = 0; i < m_nVertices; i++)
	{
		if(m_Vertices[i].m_bSelected)
			MoveVertices[nMoveVertices++] = m_Vertices[i].id;
	}

	for(int i = 0; i < m_nEdges; i++)
	{
		CSSEdge* pEdge = &m_Edges[i];

		if(!pEdge->m_bSelected)	// make sure it's selected
			continue;

		// add edge's vertices to the movement list
		BOOL bAddStart = TRUE, bAddEnd = TRUE;
		for(int i2 = 0; i2 < nMoveVertices; i2++)
		{
			if(pEdge->hvStart == MoveVertices[i2])
				bAddStart = FALSE;	// already got this one
			if(pEdge->hvEnd == MoveVertices[i2])
				bAddEnd = FALSE;	// already got this one
		}

		if(bAddStart)
			MoveVertices[nMoveVertices++] = pEdge->hvStart;
		if(bAddEnd)
			MoveVertices[nMoveVertices++] = pEdge->hvEnd;
	}

	// move vertices now
	for(int i = 0; i < nMoveVertices; i++)
	{
		GetHandleInfo(&hi, MoveVertices[i]);
		CSSVertex* pVertex = (CSSVertex*) hi.pData;
		SetVertexPosition(hi.iIndex, pVertex->pos[0] + Delta[0], pVertex->pos[1] + Delta[1], pVertex->pos[2] + Delta[2]);
	}

	// calculate center of moved edges
	int nEdges;
	CSSEdge ** ppEdges = FindAffectedEdges(MoveVertices, nMoveVertices, nEdges);
	for(int i = 0; i < nEdges; i++)
	{
		CalcEdgeCenter(ppEdges[i]);
	}
}


// check faces for irregularities ->
void CSSolid::CheckFaces()
{
	for(int i = 0; i < m_nFaces; i++)
	{
		CSSFace &face = m_Faces[i];

		// get points for face
		Vector *pts = CreatePointList(face);

		// call checkface function
		CCheckFaceInfo cfi;

		while(CheckFace(pts, face.nEdges, NULL, 0, &cfi) == FALSE)
		{
			CString str;
			str.Format("face %d - %s", i, cfi.szDescription);
			AfxMessageBox(str);
		}

		delete[] pts;
	}
}


void CSSolid::SetVertexPosition(int iVertex, float x, float y, float z)
{
	m_Vertices[iVertex].pos = Vector(x, y, z);
}


static int GetNext(int iIndex, int iDirection, int iMax)
{
	iIndex += iDirection;
	if(iIndex == iMax)
		iIndex = 0;
	if(iIndex == -1)
		iIndex = iMax-1;
	return iIndex;
}


BOOL CSSolid::SplitFace(SSHANDLE h1, SSHANDLE h2)
{
	SSHANDLEINFO hi;
	GetHandleInfo(&hi, h1);

	if(m_nFaces == MAX_FACES-1)
		return FALSE;

	BOOL bRvl = FALSE;

	if(hi.Type == shtEdge)
	{
		// edge-based face split
		bRvl = SplitFaceByEdges((CSSEdge*) hi.pData, 
			(CSSEdge*) GetHandleData(h2));
	}
	else if(hi.Type == shtVertex)
	{
		// vertex-based face split
		bRvl = SplitFaceByVertices((CSSVertex*) hi.pData, 
			(CSSVertex*) GetHandleData(h2));
	}

	return bRvl;
}


BOOL CSSolid::SplitFaceByVertices(CSSVertex *pVertex1, CSSVertex *pVertex2)
{
	if(GetEdgeIndex(pVertex1->id, pVertex2->id) != -1)
		return FALSE;	// already an edge there!

	// find the face, first - get a list of affected edges and find
	//  two with a common face
	int iNumEdges1, iNumEdges2;
	SSHANDLE hFace = 0;
	CSSEdge *pEdges1[64], *pEdges2[64], **pTmp;

	pTmp = FindAffectedEdges(&pVertex1->id, 1, iNumEdges1);
	memcpy(pEdges1, pTmp, iNumEdges1 * sizeof(CSSEdge*));
	pTmp = FindAffectedEdges(&pVertex2->id, 1, iNumEdges2);
	memcpy(pEdges2, pTmp, iNumEdges2 * sizeof(CSSEdge*));

	for(int i = 0; i < iNumEdges1; i++)
	{
		SSHANDLE hFace0 = pEdges1[i]->Faces[0];
		SSHANDLE hFace1 = pEdges1[i]->Faces[1];
		for(int i2 = 0; i2 < iNumEdges2; i2++)
		{
			if(hFace0 == pEdges2[i2]->Faces[0] ||
				hFace0 == pEdges2[i2]->Faces[1])
			{
				hFace = hFace0;
				break;
			}
			else if(hFace1 == pEdges2[i2]->Faces[0] ||
				hFace1 == pEdges2[i2]->Faces[1])
			{
				hFace = hFace1;
				break;
			}
		}
	}

	// couldn't find a common face
	if(hFace == 0)
		return FALSE;

	CSSFace *pFace = (CSSFace*) GetHandleData(hFace);

	// create a new face
	CSSFace *pNewFace = AddFace();
	memcpy(&pNewFace->texture, &pFace->texture, sizeof(TEXTURE));

	// create a new edge between two vertices
	CSSEdge *pNewEdge = AddEdge();
	pNewEdge->hvStart = pVertex1->id;
	pNewEdge->hvEnd = pVertex2->id;
	CalcEdgeCenter(pNewEdge);

	// assign face ids to the new edge
	AssignFace(pNewEdge, pFace->id);
	AssignFace(pNewEdge, pNewFace->id);

	// set up edges - start with newvertex1 
	SSHANDLE hNewEdges[64];
	int nNewEdges;
	BOOL bFirst = TRUE;
	CSSFace *pStoreFace = pFace;

	SSHANDLE *phVertexList = CreatePointHandleList(*pFace);
	int nVertices = pFace->nEdges;

	int v1index = 0, v2index = 0;

	// find where the vertices are and
	//  kill face references in edges first
	for(int i = 0; i < nVertices; i++)
	{
		int iNextVertex = GetNext(i, 1, nVertices);
		int iEdgeIndex = GetEdgeIndex(phVertexList[i], 
			phVertexList[iNextVertex]);
		CSSEdge *pEdge = &m_Edges[iEdgeIndex];
		AssignFace(pEdge, pFace->id, TRUE);

		if(phVertexList[i] == pVertex1->id)
			v1index = i;
		else if(phVertexList[i] == pVertex2->id)
			v2index = i;
	}

DoNextFace:
	nNewEdges = 0;
	for(int i = v1index; ; i++)
	{
		if(i == nVertices)
			i = 0;

		if(i == v2index)
			break;

		int iNextVertex = GetNext(i, 1, nVertices);
		int iEdgeIndex = GetEdgeIndex(phVertexList[i], phVertexList[iNextVertex]);
		Assert(iEdgeIndex != -1);

		hNewEdges[nNewEdges++] = m_Edges[iEdgeIndex].id;

		AssignFace(&m_Edges[iEdgeIndex], pFace->id);
	}
	// now add the middle edge
	hNewEdges[nNewEdges++] = pNewEdge->id;
	// now set up in face
	pStoreFace->nEdges = nNewEdges;
	memcpy(pStoreFace->Edges, hNewEdges, sizeof(SSHANDLE) * nNewEdges);

	if(bFirst)
	{
		int tmp = v1index;
		v1index = v2index;
		v2index = tmp;
		pStoreFace = pNewFace;
		bFirst = FALSE;
		goto DoNextFace;
	}

	delete phVertexList;

	return(TRUE);
}

BOOL CSSolid::SplitFaceByEdges(CSSEdge *pEdge1, CSSEdge *pEdge2)
{
	SSHANDLE hFace;

	// find the handle of the face
	if(pEdge1->Faces[0] == pEdge2->Faces[0] || 
		pEdge1->Faces[0] == pEdge2->Faces[1])
	{
		hFace = pEdge1->Faces[0];
	}
	else if(pEdge1->Faces[1] == pEdge2->Faces[0] || 
		pEdge1->Faces[1] == pEdge2->Faces[1])
	{
		hFace = pEdge1->Faces[1];
	}
	else return FALSE;	// not the same face

	// get pointer to face
	CSSFace *pFace = (CSSFace*) GetHandleData(hFace);

	// create new objects
	CSSFace *pNewFace = AddFace();
	CSSEdge *pNewEdgeMid = AddEdge();
	int iNewVertex1, iNewVertex2;
	CSSVertex *pNewVertex1 = AddVertex(&iNewVertex1);
	CSSVertex *pNewVertex2 = AddVertex(&iNewVertex2);

	// assign faces to new edge
	AssignFace(pNewEdgeMid, pFace->id);
	AssignFace(pNewEdgeMid, pNewFace->id);

	// copy texture info from one face to the other
	memcpy(&pNewFace->texture, &pFace->texture, sizeof(TEXTURE));

	// set vertex positions
	m_Vertices[iNewVertex1].pos = pEdge1->ptCenter;
	m_Vertices[iNewVertex2].pos = pEdge2->ptCenter;

	// set up middle edge
	pNewEdgeMid->hvStart = pNewVertex1->id;
	pNewEdgeMid->hvEnd = pNewVertex2->id;
	CalcEdgeCenter(pNewEdgeMid);

	// set up new side edges
	CSSEdge *pEdgeTmp = AddEdge();
	pEdgeTmp->hvStart = pEdge1->hvStart;
	pEdgeTmp->hvEnd = pNewVertex1->id;
	CalcEdgeCenter(pEdgeTmp);

	pEdgeTmp = AddEdge();
	pEdgeTmp->hvStart = pEdge1->hvEnd;
	pEdgeTmp->hvEnd = pNewVertex1->id;
	CalcEdgeCenter(pEdgeTmp);

	pEdgeTmp = AddEdge();
	pEdgeTmp->hvStart = pEdge2->hvStart;
	pEdgeTmp->hvEnd = pNewVertex2->id;
	CalcEdgeCenter(pEdgeTmp);

	pEdgeTmp = AddEdge();
	pEdgeTmp->hvStart = pEdge2->hvEnd;
	pEdgeTmp->hvEnd = pNewVertex2->id;
	CalcEdgeCenter(pEdgeTmp);

/*
	FILE *fp = fopen("split", "w");
	for(i = 0; i < nVertices; i++)
	{
		fprintf(fp, "%lu\n", phVertexList[i]);
	}
	fclose(fp);
*/

	// set up edges - start with newvertex1 
	SSHANDLE hNewEdges[64];
	int nNewEdges;
	BOOL bFirst = TRUE;
	CSSFace *pStoreFace = pFace;

	// ** do two new faces first **

	int nv1index, nv2index;
	SSHANDLE *phVertexList = CreateNewVertexList(pFace, pEdge1, pEdge2, 
		nv1index, nv2index, pNewVertex1, pNewVertex2);
	int nVertices = pFace->nEdges;
	if(nv1index != -1)
		++nVertices;
	if(nv2index != -1)
		++nVertices;

	// kill face references in edges first
	for(int i = 0; i < nVertices; i++)
	{
		int iNextVertex = GetNext(i, 1, nVertices);
		int iEdgeIndex = GetEdgeIndex(phVertexList[i], 
			phVertexList[iNextVertex]);
		CSSEdge *pEdge = &m_Edges[iEdgeIndex];
		Assert(pEdge->id != pEdge1->id);
		Assert(pEdge->id != pEdge2->id);
		AssignFace(pEdge, pFace->id, TRUE);
	}

DoNextFace:
	nNewEdges = 0;
	for(int i = nv1index; ; i++)
	{
		if(i == nVertices)
			i = 0;

		if(i == nv2index)
			break;

		int iNextVertex = GetNext(i, 1, nVertices);
		int iEdgeIndex = GetEdgeIndex(phVertexList[i], phVertexList[iNextVertex]);
		Assert(iEdgeIndex != -1);

		hNewEdges[nNewEdges++] = m_Edges[iEdgeIndex].id;

		AssignFace(&m_Edges[iEdgeIndex], pStoreFace->id);
	}
	// now add the middle edge
	hNewEdges[nNewEdges++] = pNewEdgeMid->id;
	// now set up in face
	pStoreFace->nEdges = nNewEdges;
	memcpy(pStoreFace->Edges, hNewEdges, sizeof(SSHANDLE) * nNewEdges);

	if(bFirst)
	{
		int tmp = nv1index;
		nv1index = nv2index;
		nv2index = tmp;
		pStoreFace = pNewFace;
		bFirst = FALSE;
		goto DoNextFace;
	}

	delete phVertexList;

	// ** now regular faces **
	for(int iFace = 0; iFace < m_nFaces; iFace++)
	{
		CSSFace *pUpdFace = &m_Faces[iFace];

		if(pUpdFace == pNewFace || pUpdFace == pFace)
			continue;
		
		phVertexList = CreateNewVertexList(pUpdFace, pEdge1, pEdge2,
			nv1index, nv2index, pNewVertex1, pNewVertex2);

		if(phVertexList == NULL)	// don't need to update this face
			continue;
	
		nNewEdges = 0;
		nVertices = pUpdFace->nEdges;
		if(nv1index != -1)
			++nVertices;
		if(nv2index != -1)
			++nVertices;
		for(int i = 0; i < nVertices; i++)
		{
			int iNextVertex = GetNext(i, 1, nVertices);
			int iEdgeIndex = GetEdgeIndex(phVertexList[i], phVertexList[iNextVertex]);
			Assert(iEdgeIndex != -1);

			AssignFace(&m_Edges[iEdgeIndex], pUpdFace->id);
			hNewEdges[nNewEdges++] = m_Edges[iEdgeIndex].id;
		}

		// now set up in face
		pUpdFace->nEdges = nNewEdges;
		memcpy(pUpdFace->Edges, hNewEdges, sizeof(SSHANDLE) * nNewEdges);

		delete phVertexList;
	}

	SSHANDLE id1 = pEdge1->id;
	SSHANDLE id2 = pEdge2->id;

	// delete old edges
	for(int i = 0; i < m_nEdges; i++)
	{
		if(m_Edges[i].id == id1 || m_Edges[i].id == id2)
		{
			DeleteEdge(i);
			--i;
		}
	}

	return TRUE;
}


void CSSolid::DeleteEdge(int iEdge)
{
	SSHANDLE edgeid = m_Edges[iEdge].id;
		
	// kill this edge
	for(int i2 = iEdge; i2 < m_nEdges-1; i2++)
	{
		memcpy(&m_Edges[i2], &m_Edges[i2+1], sizeof(CSSEdge));
	}
	--m_nEdges;

	memset(&m_Edges[m_nEdges], 0, sizeof(CSSEdge));

	// kill all references to this edge in faces
	for(int f = 0; f < m_nFaces; f++)
	{
		CSSFace& face = m_Faces[f];
		for(int e = 0; e < face.nEdges; e++)
		{
			if(face.Edges[e] != edgeid)
				continue;

			memcpy(&face.Edges[e], &face.Edges[e+1], (face.nEdges-e) * 
				sizeof(face.Edges[0]));
			--face.nEdges;
			break;	// no more in this face
		}
	}
}


void CSSolid::DeleteVertex(int iVertex)
{
	for(int i2 = iVertex; i2 < m_nVertices-1; i2++)
	{
		memcpy(&m_Vertices[i2], &m_Vertices[i2+1], sizeof(CSSVertex));
	}
	--m_nVertices;

	memset(&m_Vertices[m_nVertices], 0, sizeof(CSSVertex));
}


void CSSolid::DeleteFace(int iFace)
{
	// Destroy the displacement if there is one.
	CSSFace *pFace = &m_Faces[iFace];
	if ( pFace )
	{
		if ( pFace->m_hDisp != EDITDISPHANDLE_INVALID )
		{
			EditDispMgr()->Destroy( pFace->m_hDisp );
			pFace->m_hDisp = EDITDISPHANDLE_INVALID;
		}
	}

	for(int i2 = iFace; i2 < m_nFaces-1; i2++)
	{
		memcpy(&m_Faces[i2], &m_Faces[i2+1], sizeof(CSSFace));
	}
	--m_nFaces;

	m_Faces[m_nFaces].Init();
}


SSHANDLE* CSSolid::CreateNewVertexList(CSSFace *pFace, CSSEdge *pEdge1, 
									   CSSEdge *pEdge2, int& nv1index, int& nv2index,
									   CSSVertex *pNewVertex1, CSSVertex *pNewVertex2)
{
	// get original vertex list
	CUtlVector<SSHANDLE> hVertexList;
	hVertexList.SetCount(pFace->nEdges+4);
	CreatePointHandleList(*pFace, hVertexList.Base());

	// add vertex1 and vertex2.
	nv1index = -1;
	nv2index = -1;

	int nVertices = pFace->nEdges;
	int iPass = 0;
DoAgain:
	for(int i = 0; i < nVertices; i++)
	{
		int iPrevIndex = GetNext(i, -1, nVertices);
		int iNextIndex = GetNext(i, 1, nVertices);

		if(nv1index == -1 && (hVertexList[i] == pEdge1->hvEnd || 
			hVertexList[i] == pEdge1->hvStart))
		{
			// find pEdge1->hvStart
			if(hVertexList[iPrevIndex] == pEdge1->hvStart || 
				hVertexList[iPrevIndex] == pEdge1->hvEnd)
			{
				// add at i.
				nv1index = i;
			}
			if(hVertexList[iNextIndex] == pEdge1->hvStart || 
				hVertexList[iNextIndex] == pEdge1->hvEnd)
			{
				// add at iNextIndex
				nv1index = iNextIndex;
			}

			if(nv1index != -1)
			{
				hVertexList.InsertBefore(nv1index, pNewVertex1->id);
				++nVertices;
				break;
			}
		}

		if(nv2index == -1 && (hVertexList[i] == pEdge2->hvEnd || 
			hVertexList[i] == pEdge2->hvStart))
		{
			// find pEdge1->hvStart
			if(hVertexList[iPrevIndex] == pEdge2->hvStart || 
				hVertexList[iPrevIndex] == pEdge2->hvEnd)
			{
				// add at i.
				nv2index = i;
			}
			if(hVertexList[iNextIndex] == pEdge2->hvStart || 
				hVertexList[iNextIndex] == pEdge2->hvEnd)
			{
				// add at iNextIndex
				nv2index = iNextIndex;
			}

			if(nv2index != -1)
			{
				hVertexList.InsertBefore(nv2index, pNewVertex2->id);
				++nVertices;
				break;
			}
		}
	}

	SSHANDLE hTmp[64];
	memcpy(hTmp, hVertexList.Base(), sizeof(SSHANDLE) * nVertices);

	if(nv1index == -1 && nv2index == -1)
		return NULL;	// not used here.

	if(nv1index == -1 || nv2index == -1)
	{
		if(++iPass != 2)
			goto DoAgain;
	}

	SSHANDLE *rvl = new SSHANDLE[nVertices];
	memcpy(rvl, hVertexList.Base(), sizeof(SSHANDLE) * nVertices);

	return rvl;
}

// merge same vertices ->

BOOL CSSolid::CanMergeVertices()
{
	for(int v1 = 0; v1 < m_nVertices; v1++)
	{
		for(int v2 = 0; v2 < m_nVertices; v2++)
		{
			if(v1 == v2)
				continue;	// no!
			if(VectorCompare(m_Vertices[v1].pos, m_Vertices[v2].pos))
				return TRUE;	// got a match
		}
	}

	return FALSE;
}

SSHANDLE * CSSolid::MergeSameVertices(int& nDeleted)
{
	int nMerged = 0;
	nDeleted = 0;
	static SSHANDLE hDeletedList[128];

DoVertices:
	for(int v1 = 0; v1 < m_nVertices; v1++)
	{
		for(int v2 = 0; v2 < m_nVertices; v2++)
		{
			if(v1 == v2)
				continue;	// no!
			if(!VectorCompare(m_Vertices[v1].pos, m_Vertices[v2].pos))
			{	// no match
				continue;
			}

			++nMerged;

			// same vertices - kill v1, set edge refs to use v2.
			SSHANDLE hV1 = m_Vertices[v1].id;
			SSHANDLE hV2 = m_Vertices[v2].id;
			
			hDeletedList[nDeleted++] = hV1;
			
			DeleteVertex(v1);

			int nAffected;
			CSSEdge **ppEdges = FindAffectedEdges(&hV1, 1, nAffected);

			// run through edges and change references
			for(int e = 0; e < nAffected; e++)
			{
				if(ppEdges[e]->hvStart == hV1)
					ppEdges[e]->hvStart = hV2;
				if(ppEdges[e]->hvEnd == hV1)
					ppEdges[e]->hvEnd = hV2;
				CalcEdgeCenter(ppEdges[e]);
			}
			
			goto DoVertices;
		}
	}

	if(!nMerged)
		return NULL;

	int e;

	// kill edges that have same vertices
	for(e = 0; e < m_nEdges; e++)
	{
		CSSEdge &edge = m_Edges[e];

		if(edge.hvStart != edge.hvEnd)
			continue;	// edge is OK

		hDeletedList[nDeleted++] = edge.id;

		DeleteEdge(e);
		--e;
	}

	// kill similar edges (replace in faces too)
DoEdges:
	for(e = 0; e < m_nEdges; e++)
	{
		CSSEdge &edge = m_Edges[e];

		for(int e2 = 0; e2 < m_nEdges; e2++)
		{
			if(e == e2)
				continue;

			CSSEdge &edge2 = m_Edges[e2];

			if(!((edge2.hvStart == edge.hvStart && edge2.hvEnd == edge.hvEnd) || 
				(edge2.hvEnd == edge.hvStart && edge2.hvStart == edge.hvEnd)))
				continue;

			// we're going to delete edge2.
			SSHANDLE id2 = edge2.id;
			SSHANDLE id1 = edge.id;

			for(int f = 0; f < m_nFaces; f++)
			{
				CSSFace& face = m_Faces[f];
				for(int ef = 0; ef < face.nEdges; ef++)
				{
					if(face.Edges[ef] == id2)
					{
						face.Edges[ef] = id1;
						break;
					}
				}
			}

			hDeletedList[nDeleted++] = id2;
			DeleteEdge(e2);

			goto DoEdges;
		}
	}

	// delete concurrent edge references in face
	for(int f = 0; f < m_nFaces; f++)
	{
		CSSFace& face = m_Faces[f];

DoConcurrentEdges:
		for(int ef1 = 0; ef1 < face.nEdges; ef1++)
		{
			for(int ef2 = 0; ef2 < face.nEdges; ef2++)
			{
				if(ef2 == ef1)
					continue;

				if(face.Edges[ef1] != face.Edges[ef2])
					continue;
			
				// delete this ref
				memcpy(&face.Edges[ef2], &face.Edges[ef2+1], (face.nEdges-ef2) * 
					sizeof(face.Edges[0]));
				--face.nEdges;

				goto DoConcurrentEdges;
			}
		}

		if(face.nEdges < 3)
		{
			// kill this face
			hDeletedList[nDeleted++] = face.id;
			DeleteFace(f);
			--f;
		}
	}

	return hDeletedList;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CSSFace::CSSFace(void)
{
	Init();
}

//-----------------------------------------------------------------------------
// Purpose: Initialize the SSFace.
//-----------------------------------------------------------------------------
void CSSFace::Init(void)
{
	nEdges = 0;
	bModified = FALSE;
	
	m_nFaceID = 0;
	m_hDisp = EDITDISPHANDLE_INVALID;
	
	memset(&texture, 0, sizeof(TEXTURE));

	texture.scale[0] = g_pGameConfig->GetDefaultTextureScale();
	texture.scale[1] = g_pGameConfig->GetDefaultTextureScale();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CSSFace::~CSSFace(void)
{
	if ( m_hDisp != EDITDISPHANDLE_INVALID )
	{
		EditDispMgr()->Destroy( m_hDisp );
		m_hDisp = EDITDISPHANDLE_INVALID;
	}

	memset(this, 0, sizeof(this));
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CSSEdge::CSSEdge(void)
{
	Faces[0] = Faces[1] = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CSSEdge::~CSSEdge()
{
	memset(this, 0, sizeof(this));
}


//-----------------------------------------------------------------------------
// Purpose: Gets the world coordinates of the center point of this edge.
// Input  : Point - Receives the world coordinates of the center point.
//-----------------------------------------------------------------------------
void CSSEdge::GetCenterPoint(Vector& Point)
{
	Point = ptCenter;
}


CSSVertex::CSSVertex(void)
{
}


CSSVertex::~CSSVertex(void)
{
	pos[0] = pos[1] = pos[2] = 0;
	id = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Gets the world coordinates of this vertex.
// Input  : Position - Receives the world coordinates.
//-----------------------------------------------------------------------------
void CSSVertex::GetPosition(Vector& Position)
{
	Position = pos;
}


// 
// save to .DXF
//
void CSSolid::SerializeDXF(FILE *stream, int nObject)
{
	char szName[128];
	sprintf(szName, "OBJECT%03d", nObject);

	// count number of triangulated faces
	int nTriFaces = 0;
	for(int i = 0; i < m_nFaces; i++)
	{
		CSSFace &face = m_Faces[i];
		nTriFaces += face.nEdges-2;
	}

	fprintf(stream,"0\nPOLYLINE\n8\n%s\n66\n1\n70\n64\n71\n%u\n72\n%u\n", szName, m_nVertices, nTriFaces);
	fprintf(stream,"62\n50\n");

	for (int i = 0; i < m_nVertices; i++)
	{
		Vector &pos = m_Vertices[i].pos;
		fprintf(stream,	"0\nVERTEX\n8\n%s\n10\n%.6f\n20\n%.6f\n30\n%.6f\n70\n192\n", szName, pos[0], pos[1], pos[2]);
	}

	// triangulate each face and write
	for(int i = 0; i < m_nFaces; i++)
	{
		CSSFace &face = m_Faces[i];
		PINT pVerts = CreatePointIndexList(face);

		for(int v = 0; v < face.nEdges; v++)
			pVerts[v]++;

		for(int v = 0; v < face.nEdges-2; v++)
		{
			fprintf(stream, "0\nVERTEX\n8\n%s\n10\n0\n20\n0\n30\n"
				"0\n70\n128\n71\n%d\n72\n%d\n73\n%d\n", szName, 
				v == 0 ? pVerts[0] : -pVerts[0],
				pVerts[v+1],
				v == (face.nEdges-3) ? pVerts[v+2] : -pVerts[v+2]
				);
		}
	}

	fprintf(stream, "0\nSEQEND\n8\n%s\n", szName);
}
