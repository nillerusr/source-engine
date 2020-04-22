//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Structured Solid Class definition
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//


#ifndef SSOLID_H
#define SSOLID_H

#ifdef _WIN32
#pragma once
#endif


#include "MapFace.h"


#define MAX_FACES 120
#define MAX_EDGES 512


class Morph3D;
class CSSolid;
class CSSEdge;


typedef DWORD SSHANDLE;


class C2DHandle
{
	public:

		C2DHandle(void) { m_bVisible = FALSE; m_bSelected = FALSE; m_bUse = TRUE; }

		BOOL m_bVisible;				// visible?
		BOOL m_bSelected;				// selected?
		BOOL m_bUse;					// use this?

		// only valid if (m_bVisible):
		short m_x, m_y;					// 2d position in 3d view
		RECT m_r;						// 2d bound box in 3d view
};


// for GetHandleInfo():
typedef enum
{
	shtNothing = -1,
	shtVertex,
	shtEdge,
	shtFace
} SSHANDLETYPE;


typedef struct
{
	SSHANDLETYPE Type;
	int iIndex;
	PVOID pData;
	C2DHandle *p2DHandle;
	Vector pos;	// 3d position of handle
} SSHANDLEINFO;


// define a face:
class CSSFace : public C2DHandle
{
	public:

		CSSFace();
		~CSSFace();

		void Init(void);

		inline int GetEdgeCount(void) { return(nEdges); }
		inline SSHANDLE GetEdgeHandle(int nEdge) { Assert(nEdge < GetEdgeCount()); return(Edges[nEdge]); }

		// edge IDs:
		SSHANDLE Edges[MAX_FACES];

		int nEdges;
		BOOL bModified;
		Vector PlanePts[3];
		Vector normal;

		TEXTURE texture;			// Original face's texture info.
		int m_nFaceID;				// Original face's unique ID.

		EditDispHandle_t m_hDisp;	// Copy of the original faces displacement.

		Vector ptCenter;

		DWORD id;
};


class CSSEdge : public C2DHandle
{
	public:

		CSSEdge();
		~CSSEdge();

		void GetCenterPoint(Vector& Point);

		// vertex IDs:
		SSHANDLE hvStart;
		SSHANDLE hvEnd;

		Vector ptCenter;

		// faces this edge belongs to.
		SSHANDLE Faces[2];

		DWORD id;
};


class CSSVertex : public C2DHandle
{
	public:

		CSSVertex();
		~CSSVertex();
		void GetPosition(Vector& Position);
		
		Vector pos;	// Position.

		DWORD id;
};


class CSSolid
{
	friend Morph3D;

	public:

		// construction/destruction:
		CSSolid();
		~CSSolid();

		// attach/detach mapsolid:
		void Attach(CMapSolid *pMapSolid);
		CMapSolid* Detach();

		// Verify that the solid (with displaced surfaces) is valid to convert back into a map solid.
		bool IsValidWithDisps( void );
		bool HasDisps( void );
		void DestroyDisps( void );

		// conversion to/from editing format:
		void Convert(BOOL bFromMapSolid = TRUE, bool bSkipDisplacementFaces = false);

		// move selected handles by a delta:
		void MoveSelectedHandles(const Vector &Delta);

		// attached map solid:
		CMapSolid *m_pMapSolid;

		inline int GetFaceCount(void) { return(m_nFaces); }
		inline CSSFace *GetFace(int nFace) { Assert(nFace < m_nFaces); return(&m_Faces[nFace]); }

		BOOL GetHandleInfo(SSHANDLEINFO * pInfo, SSHANDLE id);
		PVOID GetHandleData(SSHANDLE id);
		BOOL SplitFace(SSHANDLE h1, SSHANDLE h2);
		BOOL SplitFaceByVertices(CSSVertex *pVertex1, CSSVertex *pVertex2);
		BOOL SplitFaceByEdges(CSSEdge *pEdge1, CSSEdge *pEdge2);

		inline BOOL ShowEdges(void) { return(m_bShowEdges); }
		inline BOOL ShowVertices(void) { return(m_bShowVertices); }

		// check faces and report errors:
		void CheckFaces();

		// save to .dxf:
		void SerializeDXF(FILE* stream, int nObject);

		SSHANDLE GetConnectionVertex(CSSEdge *pEdge1, CSSEdge *pEdge2);

	private:

		// called by Convert():
		void ToMapSolid(CMapSolid* = NULL);
		void FromMapSolid(CMapSolid* = NULL, bool bSkipDisplacementFaces = false);

		void AssignFace(CSSEdge* pEdge, SSHANDLE hFace, BOOL = FALSE);

		// delete face/edge/vertex:
		void DeleteFace(int);
		void DeleteVertex(int);
		void DeleteEdge(int);
		
		SSHANDLE * MergeSameVertices(int& nDeleted);
		BOOL CanMergeVertices();

		// add face/edge/vertex:
		CSSFace* AddFace(int* = NULL);
		CSSEdge* AddEdge(int* = NULL);
		CSSVertex* AddVertex(int* = NULL);

		// get the index to the vertex at this point - 
		//  return -1 if no matching vertex.
		int GetVertexIndex(const Vector &Point, float fLeniency = 0.0f);
		// ditto for edge
		int GetEdgeIndex(const Vector &Point, float fLeniency = 0.0f);
		int GetEdgeIndex(SSHANDLE v1, SSHANDLE v2);
		// ditto for face
		int GetFaceIndex(const Vector &Point, float fLeniency = 0.0f);

		SSHANDLE GetNewID();
		void CalcEdgeCenter(CSSEdge *pEdge);
		CSSEdge ** FindAffectedEdges(SSHANDLE *pHandles, int iNumHandles, 
								int& iNumEdges);
		Vector * CreatePointList(CSSFace & face);
		PINT CreatePointIndexList(CSSFace & face, PINT piPoints = NULL);
		SSHANDLE* CreatePointHandleList(CSSFace & face, SSHANDLE* phPoints = NULL);
		void SetVertexPosition(int iVertex, float x, float y, float z);
		SSHANDLE* CreateNewVertexList(CSSFace *pFace, CSSEdge *pEdge1, 
										   CSSEdge *pEdge2, int& nv1index, int& nv2index,
										   CSSVertex *pNewVertex1, CSSVertex *pNewVertex2);
		void ShowHandles(BOOL bShowVertices, BOOL bShowEdges);

		int m_nVertices;	// number of unique vertices
		BlockArray<CSSVertex, 16, 32> m_Vertices;	// vertices

		int m_nEdges;		// number of unique edges
		BlockArray<CSSEdge, 16, 32> m_Edges;	// edges

		int m_nFaces;		// number of faces
		BlockArray<CSSFace, 16, 10> m_Faces;	// faces

		SSHANDLE m_curid;
		BOOL m_bShowVertices, m_bShowEdges;
};


#endif SSOLID_H
