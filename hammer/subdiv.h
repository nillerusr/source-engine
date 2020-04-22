//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SUBDIV_H
#define SUBDIV_H
#pragma once

class CMapDisp;
class CSubdivEdge;
class CSubdivQuad;

//=============================================================================
//
// Class Subdivision Point
//
class CSubdivPoint
{
public:

	enum { POINT_ORDINARY = 0,
		   POINT_CORNER = 1,
		   POINT_CREASE = 2 };
	
	enum { NUM_SUBDIV_EDGES = 8 };

	Vector		m_Point;
	Vector		m_Normal;
	Vector		m_NewPoint;
	Vector		m_NewNormal;
	int			m_Type;
	int			m_Valence;
	CSubdivEdge	*m_pEdges[NUM_SUBDIV_EDGES];

	void Clear( void );
	void Copy( const CSubdivPoint *pFrom );

	void CalcNewVertexPoint( void );
	void CalcNewVertexNormal( void );

	friend bool CompareSubdivPoints( const CSubdivPoint *pPoint1, const CSubdivPoint *pPoint2, float tolerance );
	friend bool CompareSubdivPointToPoint( const CSubdivPoint *pSubdivPoint, const Vector& point, float tolerance );
};


//=============================================================================
//
// Class Subdivision Edge
//
class CSubdivEdge
{
public:

	short		m_ndxPoint[2];
	CSubdivQuad	*m_pQuads[2];
	short		m_ndxQuadEdge[2];
	float		m_Sharpness;
	Vector		m_NewEdgePoint;
	Vector		m_NewEdgeNormal;
	bool		m_Active;

	void Clear( void );
	void Copy( const CSubdivEdge *pFrom );

	void CalcNewEdgePoint( void );
	void CalcNewEdgeNormal( void );

	friend bool CompareSubdivEdges( const CSubdivEdge *pEdge1, const CSubdivEdge *pEdge2 );
};


//=============================================================================
//
// Class Subdivision Quad
//
class CSubdivQuad
{
public:

	short	m_ndxQuad[4];		// quad indices -- see CSubdivManager
	short	m_ndxVert[4];		// vert indices -- see CSubdivManager
	short	m_ndxEdge[4];		// edge indices -- see CSubdivManager
	Vector	m_Centroid;			// center of quad
	Vector  m_Normal;			// quad normal

	void GetCentroid( Vector& centroid );
	void CalcCentroid( void );

	void GetNormal( Vector& normal );
	void CalcNormal( void );
};


//=============================================================================
//
// Class Subdivision Mesh
//
class CSubdivMesh
{
public:

	//=========================================================================
	//
	// Creation/Destruction
	//
	CSubdivMesh();
	~CSubdivMesh();

	//=========================================================================
	//
	// 
	//
	inline void Clear( void );
	void DoSubdivide( void );

	//=========================================================================
	//
	// 
	//
	inline int GetPointCount( void );
	int AddPoint( const Vector& point, const Vector& normal );
	void RemovePoint( Vector& point );
	inline void GetPoint( int index, Vector& point );
	inline void GetNormal( int index, Vector& normal );

	inline int GetEdgeCount( void );
	int AddEdge( CSubdivEdge *edge );
	void RemoveEdge( CSubdivEdge *edge );
	inline void GetEdge( int index, CSubdivEdge *edge );

private:

//	enum { MAX_SUBDIV_POINTS = 32000 };
//	enum { MAX_TREES = 64 };

	int				m_PointCount;
	int				m_MaxPointCount;
	CSubdivPoint	*m_pPoints;
//	CSubdivPoint	m_Points[MAX_SUBDIV_POINTS];		// mesh list of subdivision verts

	int				m_EdgeCount;
	int				m_MaxEdgeCount;
	CSubdivEdge		*m_pEdges;
//	CSubdivEdge	    m_Edges[MAX_SUBDIV_POINTS];			// mesh list of subdivision edges

	int				m_TreeCount;
	int				m_MaxTreeCount;
	CSubdivQuad		**m_ppTrees;
//	CSubdivQuad		*m_pTrees[MAX_TREES];

	void CatmullClarkSubdivide( void );
	int AddTree( CSubdivQuad *pTree );
	int GetStartIndexFromLevel( int levelIndex );
	int GetEndIndexFromLevel( int levelIndex );
	void AddQuadToMesh( CSubdivQuad *pQuad );

	inline void ClearEdges( void );

	void CreateChildQuads( CSubdivQuad *pRoot, int quadIndex );
	void SetEdgeData( CSubdivQuad *pRoot, int index, int parentIndex, int subdivIndex );
	void CreateChildQuad1( CSubdivQuad *pRoot, int index, int parentIndex );
	void CreateChildQuad2( CSubdivQuad *pRoot, int index, int parentIndex );
	void CreateChildQuad3( CSubdivQuad *pRoot, int index, int parentIndex );
	void CreateChildQuad4( CSubdivQuad *pRoot, int index, int parentIndex );

	bool PreSubdivide( void );
	void Subdivide( void );
	void PostSubdivide( void );

	bool AllocCache( int dispCount );
	void FreeCache( void );
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CSubdivMesh::Clear( void )
{
	m_PointCount = 0;
	m_EdgeCount = 0;
	m_TreeCount = 0;
	
	m_MaxPointCount = 0;
	m_MaxEdgeCount = 0;
	m_MaxTreeCount = 0;

	m_pPoints = NULL;
	m_pEdges = NULL;
	m_ppTrees = NULL;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int CSubdivMesh::GetPointCount( void )
{
	return m_PointCount;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CSubdivMesh::GetPoint( int index, Vector& point )
{
	assert( index >= 0 );
	assert( index < m_PointCount );

	point = m_pPoints[index].m_Point;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CSubdivMesh::GetNormal( int index, Vector& normal )
{
	assert( index >= 0 );
	assert( index < m_PointCount );

	normal = m_pPoints[index].m_Normal;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int CSubdivMesh::GetEdgeCount( void )
{
	return m_EdgeCount;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CSubdivMesh::GetEdge( int index, CSubdivEdge *edge )
{
	assert( index >= 0 );
	assert( index < m_EdgeCount );

	edge->Copy( &m_pEdges[index] );
}


#endif // SUBDIV_H