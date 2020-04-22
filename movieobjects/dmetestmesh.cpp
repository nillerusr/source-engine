//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmetestmesh.h"
#include "movieobjects/dmetransform.h"
#include "movieobjects_interfaces.h"

#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "mathlib/vector.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "datacache/imdlcache.h"
#include "istudiorender.h"
#include "studio.h"
#include "bone_setup.h"
#include "materialsystem/ivertextexture.h"
#include "morphdata.h"
#include "tier3/tier3.h"

#include <strstream>
#include <fstream>
#include <algorithm>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeTestMesh, CDmeTestMesh );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeTestMesh::OnConstruction()
{
	m_MDLHandle = MDLHANDLE_INVALID;
	m_pMaterial = NULL;
	m_pMesh = NULL;
	m_pMorph = NULL;
	m_pControlCage = NULL;
	SetValue( "transform", g_pDataModel->IsUnserializing() ? NULL : CreateElement< CDmeTransform >( "transform", GetFileId() ) );
	SetValue( "mdlfilename", "models/alyx.mdl" );
	SetValue( "morphfilename", "models/alyx.morph" );
	SetValue( "skin", 0 );
	SetValue( "body", 0 );
	SetValue( "sequence", 0 );
	SetValue( "lod", 0 );
	SetValue( "playbackrate", 1.0f );
	SetValue( "time", 0.0f );
	SetValue( "subdivlevel", 1 );
}

void CDmeTestMesh::OnDestruction()
{
	UnloadMorphData();
	UnreferenceMDL();
	DestroyControlCage();
	DestroyMesh();
}


//-----------------------------------------------------------------------------
// Addref/Release the MDL handle
//-----------------------------------------------------------------------------
void CDmeTestMesh::ReferenceMDL( const char *pMDLName )
{
	if ( !g_pMDLCache )
		return;

	if ( pMDLName && pMDLName[0] )
	{
		Assert( m_MDLHandle == MDLHANDLE_INVALID );
		m_MDLHandle = g_pMDLCache->FindMDL( pMDLName );
	}
}

void CDmeTestMesh::UnreferenceMDL()
{
	if ( !g_pMDLCache )
		return;

	if ( m_MDLHandle != MDLHANDLE_INVALID )
	{
		g_pMDLCache->Release( m_MDLHandle );
		m_MDLHandle = MDLHANDLE_INVALID;
	}
}


//-----------------------------------------------------------------------------
// Creates the mesh to draw
//-----------------------------------------------------------------------------
void CDmeTestMesh::CreateMesh()
{
	DestroyMesh();

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	m_pMaterial = g_pMaterialSystem->FindMaterial( "shadertest/vertextexturetest", NULL, false );
	m_pMesh = pRenderContext->CreateStaticMesh( m_pMaterial, 0, "dmemesh" );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( m_pMesh, MATERIAL_TRIANGLES, 8, 36 );

	// Draw a simple cube
	static Vector s_pPositions[8] = 
	{
		Vector( -10, -10, -10 ),
		Vector(  10, -10, -10 ),
		Vector( -10,  10, -10 ),
		Vector(  10,  10, -10 ),
		Vector( -10, -10,  10 ),
		Vector(  10, -10,  10 ),
		Vector( -10,  10,  10 ),
		Vector(  10,  10,  10 ),
	};

	static Vector2D s_pTexCoords[8] = 
	{
		Vector2D(   0,     0 ),
		Vector2D(   0.5,   0 ),
		Vector2D(   0,   0.5 ),
		Vector2D(   0.5, 0.5 ),
		Vector2D(   0.5, 0.5 ),
		Vector2D(   1,   0.5 ),
		Vector2D(   0.5,   1 ),
		Vector2D(   1,     1 ),
	};

	static unsigned char s_pColor[8][3] = 
	{
		{ 255, 255, 255 },
		{   0, 255, 255 },
		{ 255,   0, 255 },
		{ 255, 255, 0 },
		{ 255, 0, 0 },
		{ 0, 255, 0 },
		{ 0, 0, 255 },
		{ 0, 0, 0 },
	};

	static int s_pIndices[12][3] = 
	{
		{ 0, 1, 5 }, { 0, 5, 4 },
		{ 4, 5, 7 }, { 4, 7, 6 },
		{ 0, 4, 6 }, { 0, 6, 2 },
		{ 0, 2, 3 }, { 0, 3, 1 },
		{ 1, 3, 7 }, { 1, 7, 5 },
		{ 2, 6, 7 }, { 2, 7, 3 },
	};

	for ( int i = 0; i < 8; ++i )
	{
		meshBuilder.Position3fv( s_pPositions[ i ].Base() );
		meshBuilder.TexCoord2fv( 0, s_pTexCoords[ i ].Base() );
//		meshBuilder.TexCoord2f( 1, i, 0.0f );
		meshBuilder.Color3ubv( s_pColor[ i ] );
		meshBuilder.AdvanceVertex();
	}

	for ( int i = 0; i < 12; ++i )
	{
		meshBuilder.FastIndex( s_pIndices[i][0] );
		meshBuilder.FastIndex( s_pIndices[i][1] );
		meshBuilder.FastIndex( s_pIndices[i][2] );
	}

	meshBuilder.End();
}

void CDmeTestMesh::DestroyMesh()
{
	if ( m_pMesh )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		pRenderContext->DestroyStaticMesh( m_pMesh );
		m_pMesh = NULL;
	}
}



//-----------------------------------------------------------------------------
// Morph data
//-----------------------------------------------------------------------------
void CDmeTestMesh::LoadMorphData( const char *pMorphFile, int nVertexCount )
{
	UnloadMorphData();

	IMorphData *pMorphData = CreateMorphData();
	m_pMorph = pMorphData->Compile( pMorphFile, m_pMaterial, nVertexCount );
	DestroyMorphData( pMorphData );
}

void CDmeTestMesh::UnloadMorphData()
{
	if ( m_pMorph )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		pRenderContext->DestroyMorph( m_pMorph );
		m_pMorph = NULL;
	}
}


//-----------------------------------------------------------------------------
// This function gets called whenever an attribute changes
//-----------------------------------------------------------------------------
void CDmeTestMesh::Resolve()
{
	CDmAttribute *pMDLFilename = GetAttribute( "mdlfilename" );
	if ( pMDLFilename && pMDLFilename->IsFlagSet( FATTRIB_DIRTY ) )
	{
		UnreferenceMDL();
		ReferenceMDL( GetValueString( "mdlfilename" ) );
		return;
	}

	CDmAttribute *pMorphFilename = GetAttribute( "morphfilename" );
	if ( pMorphFilename && pMorphFilename->IsFlagSet( FATTRIB_DIRTY ) )
	{
		CreateMesh();

		UnloadMorphData();
		LoadMorphData( GetValueString( "morphfilename" ), 8 );
		return;
	}
}

	
//-----------------------------------------------------------------------------
// Loads the model matrix based on the transform
//-----------------------------------------------------------------------------
void CDmeTestMesh::LoadModelMatrix( CDmeTransform *pTransform )
{
	// FIXME: Should this go into the DmeTransform node?
	matrix3x4_t transform;
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pTransform->GetTransform( transform );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->LoadMatrix( transform );
}


//-----------------------------------------------------------------------------
// A subvision mesh
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// NOTES:
// The subdivision mesh is fast because it assumes a very particular ordering
// and definition of the data so that it can determine all subdivided data by
// inspection without any searching. Here's the layout:
//
// First, a face stores a list of edge indices which reference the edges
// that make up the face. A face is assumed to traverse its vertices in CCW order.
// We define the "relative edge index" for an edge within a face as the
// order in which that edge is visited while traversing the edges in CCW order,
// so 0 is the first visited edge, and 1 is the next, etc.
//
// First, edges are defined in a specific way. The edge is assumed to be
// *directed*, starting at vertex 0 and leading toward vertex 1. Now imagine the
// two faces that shared this edge and that they both traverse their edges in
// a right-handed, or CCW direction. Face 0 associated with the edge, to maintain
// a CCW ordering, must traverse the edge in a *reverse* direction, heading from
// vertex 1 to vertex 0. Face 1 associated with the edge traverses the edge
// in a forward direction, from vertex 0 to vertex 1.
//
// When subdivision happens, it occurs in a very specific way also. First, when
// creating the new vertices, for uniform subdivision, we create a new vertex
// per face, a new vertex per edge, and adjust all existing vertices. When creating
// these vertices in the subdivided mesh, we first add the face midpoint vertices,
// then the edge midpoint vertices, then the vertices from the un-subdivided mesh, to
// the m_Vertices array of the subdivided mesh.
//
// Edge subdivision always works in a uniform way: For each edge in the unsubdivided
// mesh, 4 edges are created from the edge midpoint, connecting to the two
// face midpoint vertices and the two edge endpoints. In order to maintain the
// specific ordering of the edges described above, we define the edges in the 
// following manner:
//		* Subdivided edge 0 : Starts at face 0 midpoint, ends at edge midpoint
//		* Subdivided edge 1 : Starts at edge midpoint, ends at face 1 midpoint
//		* Subdivided edge 2 : Starts at original edge's vertex 0, ends at edge midpoint
//		* Subdivided edge 3 : Starts at edge midpoint, ends at original edge's vertex 1
//
// Face subdivision *also* always works in a uniform way: For each face in the
// unsubdivided mesh, N new faces are created, one for each edge in the unsubdivided
// face. The faces are ordered in a very specific way: 
//	    * Subdivided face 0 : Starts at the face midpoint, goes to unsubdivided edge 0's midpoint, 
//								winds around the edge until it hits unsubdivided edge 1's midpoint,
//								then heads back to the face midpoint.
//	    * Subdivided face 1 : Starts at the face midpoint, goes to unsubdivided edge 1's midpoint, 
//								winds around the edge until it hits unsubdivided edge 2's midpoint,
//								then heads back to the face midpoint.
//		etc.
//-----------------------------------------------------------------------------
struct SubdivVertex_t
{
	Vector m_vecPosition;
	Vector m_vecNormal;
	Vector m_vecTexCoord;
	int m_nValence;
};

// NOTE: The edge is always defined such that the edge going from vertex[0] to vertex[1]
// is counter-clockwise when seen from face[1] and and clockwise when seen from face[0].
struct Edge_t
{
	int m_pFace[2];
	int m_pRelativeEdgeIndex[2];	// Goes from 0-N always, specifies the Nth edge of the polygon it's part of for each of the two faces
	int m_pVertex[2];
};

struct Face_t
{
	int m_nFirstEdgeIndex;
	int m_nEdgeCount;

	// Stores the index of the first face in the subdivided mesh
	// isn't actually a part of the mesh data, but I'm storing it here to reduce number of allocations to make
	mutable int m_nFirstSubdividedFace;
};

struct SubdivMesh_t
{
	CUtlVector<SubdivVertex_t> m_Vertices;
	CUtlVector<Edge_t> m_Edges;

	// Positive values mean read from m_Edges[x], use m_pVertex[0] for leading vertex
	// Negative values mean read from m_Edges[-1-x], use m_pVertex[1] for leading vertex
	CUtlVector<int> m_EdgeIndices;	
	CUtlVector<Face_t> m_Faces;

	int m_nTotalIndexCount;
	int m_nTotalLineCount;
};


//-----------------------------------------------------------------------------
// Clears a mesh
//-----------------------------------------------------------------------------
static void ClearMesh( SubdivMesh_t &dest )
{
	dest.m_Vertices.RemoveAll();
	dest.m_Edges.RemoveAll();
	dest.m_EdgeIndices.RemoveAll();
	dest.m_Faces.RemoveAll();
	dest.m_nTotalIndexCount = 0;
	dest.m_nTotalLineCount = 0;
}


//-----------------------------------------------------------------------------
// Gets the leading vertex of an edge
//-----------------------------------------------------------------------------
static inline int GetLeadingEdgeVertexIndex( const SubdivMesh_t &src, int nEdge )
{
	if ( nEdge >= 0 )
	{
		const Edge_t &edge = src.m_Edges[nEdge];
		return edge.m_pVertex[0];
	}

	const Edge_t &edge = src.m_Edges[ -1 - nEdge ];
	return edge.m_pVertex[1];
}

static inline const SubdivVertex_t &GetLeadingEdgeVertex( const SubdivMesh_t &src, int nEdge )
{
	return src.m_Vertices[ GetLeadingEdgeVertexIndex( src, nEdge ) ];
}


//-----------------------------------------------------------------------------
// Adds face midpoints to a mesh
//-----------------------------------------------------------------------------
static void AddFaceMidpointsToMesh( const SubdivMesh_t &src, SubdivMesh_t &dest )
{
	int nCurrSubdividedFace = 0;

	int nSrcFaceCount = src.m_Faces.Count();
	for ( int i = 0; i < nSrcFaceCount; ++i )
	{
		int nEdgeCount = src.m_Faces[i].m_nEdgeCount;
		int nEdgeIndex = src.m_Faces[i].m_nFirstEdgeIndex;

		Assert( nEdgeCount != 0 );

		int v = dest.m_Vertices.AddToTail( );
		SubdivVertex_t &vert = dest.m_Vertices[v];
	    vert.m_vecPosition.Init();
	    vert.m_vecTexCoord.Init();
		vert.m_nValence = nEdgeCount;

		for ( int j = 0; j < nEdgeCount; ++j, ++nEdgeIndex )
		{
			// NOTE: Instead of calling GetLeadingEdgeVertex, 
			// I could add both vertices for each edge + multiply by 0.5
			int nEdge = src.m_EdgeIndices[nEdgeIndex];

			const SubdivVertex_t &srcVert = GetLeadingEdgeVertex( src, nEdge );
			vert.m_vecPosition += srcVert.m_vecPosition;
			vert.m_vecTexCoord += srcVert.m_vecTexCoord;
		}

		vert.m_vecPosition /= nEdgeCount;
		vert.m_vecTexCoord /= nEdgeCount;

		// Store off the face index in the dest mesh of the first subdivided face for this guy.
		src.m_Faces[i].m_nFirstSubdividedFace = nCurrSubdividedFace;
		nCurrSubdividedFace += nEdgeCount;
	}
}


//-----------------------------------------------------------------------------
// Adds edge midpoints to a mesh
//-----------------------------------------------------------------------------
static void AddEdgeMidpointsToMesh( const SubdivMesh_t &src, SubdivMesh_t &dest )
{
	int nSrcEdgeCount = src.m_Edges.Count();
	for ( int i = 0; i < nSrcEdgeCount; ++i )
	{
		const Edge_t &edge = src.m_Edges[i];

		int v = dest.m_Vertices.AddToTail( );
		SubdivVertex_t &vert = dest.m_Vertices[v];
		vert.m_nValence = 4;

		const SubdivVertex_t *pSrcVert = &src.m_Vertices[ edge.m_pVertex[0] ];
		vert.m_vecPosition = pSrcVert->m_vecPosition; 
		vert.m_vecTexCoord = pSrcVert->m_vecTexCoord; 

		pSrcVert = &src.m_Vertices[ edge.m_pVertex[1] ];
		vert.m_vecPosition += pSrcVert->m_vecPosition; 
		vert.m_vecTexCoord += pSrcVert->m_vecTexCoord; 

		// NOTE: We know that the first n vertices added to dest correspond to the src face midpoints
		pSrcVert = &dest.m_Vertices[ edge.m_pFace[0] ];
		vert.m_vecPosition += pSrcVert->m_vecPosition; 
		vert.m_vecTexCoord += pSrcVert->m_vecTexCoord; 

		pSrcVert = &dest.m_Vertices[ edge.m_pFace[1] ];
		vert.m_vecPosition += pSrcVert->m_vecPosition; 
		vert.m_vecTexCoord += pSrcVert->m_vecTexCoord; 

		vert.m_vecPosition /= 4.0f;
		vert.m_vecTexCoord /= 4.0f;
	}
}


//-----------------------------------------------------------------------------
// Adds edge midpoints to a mesh
//-----------------------------------------------------------------------------
static void AddModifiedVerticesToMesh( const SubdivMesh_t &src, SubdivMesh_t &dest )
{
	int nSrcVertexCount = src.m_Vertices.Count();

	// This computes the equation v(i+1) = ((N-2)/N) * v(i) + (1/N^2) * sum( ei + fi )
	int nFirstDestVertex = dest.m_Vertices.Count();
	for ( int i = 0; i < nSrcVertexCount; ++i )
	{
		int v = dest.m_Vertices.AddToTail( );
		SubdivVertex_t &vert = dest.m_Vertices[v];

		int nValence = src.m_Vertices[i].m_nValence;
		vert.m_nValence = nValence;
		float flScale = (float)(nValence - 2) / nValence;
		VectorScale( src.m_Vertices[i].m_vecPosition, flScale, vert.m_vecPosition );
		VectorScale( src.m_Vertices[i].m_vecTexCoord, flScale, vert.m_vecTexCoord );
	}

	int nSrcEdgeCount = src.m_Edges.Count();
	for ( int i = 0; i < nSrcEdgeCount; ++i )
	{
		const Edge_t &edge = src.m_Edges[i];
		for ( int j = 0; j < 2; ++j )
		{
			int nDestVertIndex = nFirstDestVertex + edge.m_pVertex[j];
			SubdivVertex_t &destVertex = dest.m_Vertices[nDestVertIndex];

			float ooValenceSq = 1.0f / destVertex.m_nValence;
			ooValenceSq *= ooValenceSq;

			// This adds in the contribution from the source vertex at the opposite edge
			const SubdivVertex_t &srcOtherVert = src.m_Vertices[ edge.m_pVertex[ 1 - j ] ];
			VectorMA( destVertex.m_vecPosition, ooValenceSq, srcOtherVert.m_vecPosition, destVertex.m_vecPosition );
			VectorMA( destVertex.m_vecTexCoord, ooValenceSq, srcOtherVert.m_vecTexCoord, destVertex.m_vecTexCoord );

			// This adds in the contribution from the two faces it's part of
			// NOTE: Usage of dest here is correct; this grabs the vertex that 
			// was created that was in the middle of the source mesh's face
			const SubdivVertex_t *pSrcFace = &dest.m_Vertices[ edge.m_pFace[ 0 ] ];
			VectorMA( destVertex.m_vecPosition, 0.5f * ooValenceSq, pSrcFace->m_vecPosition, destVertex.m_vecPosition );
			VectorMA( destVertex.m_vecTexCoord, 0.5f * ooValenceSq, pSrcFace->m_vecTexCoord, destVertex.m_vecTexCoord );
			pSrcFace = &dest.m_Vertices[ edge.m_pFace[ 1 ] ];
			VectorMA( destVertex.m_vecPosition, 0.5f * ooValenceSq, pSrcFace->m_vecPosition, destVertex.m_vecPosition );
			VectorMA( destVertex.m_vecTexCoord, 0.5f * ooValenceSq, pSrcFace->m_vecTexCoord, destVertex.m_vecTexCoord );
		}
	}
}


//-----------------------------------------------------------------------------
// Adds unique subdivided edges so they aren't repeated.
//-----------------------------------------------------------------------------
static void AddSubdividedEdges( const SubdivMesh_t &src, SubdivMesh_t &dest )
{
	// NOTE: We iterate over each edge in sequence and add edges 
	// between face 0, then face 1, then vertex 0, then vertex 1.
	// The vertex index for the vert at the center of original face N is N.
	// The vertex index for the vert at the center of original edge N is nSrcFaceCount + N;
	// The vertex index for the vert at original vertex N is nSrcFaceCount + nSrcEdgeCount + N;
	int nSrcFaceCount = src.m_Faces.Count();
	int nSrcEdgeCount = src.m_Edges.Count();

	for ( int i = 0; i < nSrcEdgeCount; ++i )
	{
		const Edge_t &srcEdge = src.m_Edges[i];

		int e = dest.m_Edges.AddMultipleToTail( 4 );
		Edge_t *pDstEdge = &dest.m_Edges[e];

		// Grab the two source faces
		const Face_t *pFaces[2];
		pFaces[0] = &src.m_Faces[ srcEdge.m_pFace[0] ];
		pFaces[1] = &src.m_Faces[ srcEdge.m_pFace[1] ];

		// Get the first subdivided face index + relative edge index
		int pSubdividedFaceIndex[2];
		pSubdividedFaceIndex[0] = pFaces[0]->m_nFirstSubdividedFace;
		pSubdividedFaceIndex[1] = pFaces[1]->m_nFirstSubdividedFace;

		// Get the relative edge index
		int pRelativeEdgeIndex[2];
		pRelativeEdgeIndex[0] = srcEdge.m_pRelativeEdgeIndex[0];
		pRelativeEdgeIndex[1] = srcEdge.m_pRelativeEdgeIndex[1];

		int pPrevRelativeEdgeIndex[2];
		pPrevRelativeEdgeIndex[0] = (srcEdge.m_pRelativeEdgeIndex[0] - 1);
		if ( pPrevRelativeEdgeIndex[0] < 0 )
		{
			pPrevRelativeEdgeIndex[0] = pFaces[0]->m_nEdgeCount - 1;
		}
		pPrevRelativeEdgeIndex[1] = (srcEdge.m_pRelativeEdgeIndex[1] - 1);
		if ( pPrevRelativeEdgeIndex[1] < 0 )
		{
			pPrevRelativeEdgeIndex[1] = pFaces[1]->m_nEdgeCount - 1;
		}

		// This ordering maintains clockwise order
		pDstEdge[0].m_pVertex[0] = srcEdge.m_pFace[0];
		pDstEdge[0].m_pVertex[1] = nSrcFaceCount + i;
		pDstEdge[0].m_pFace[0] = pSubdividedFaceIndex[0] + pPrevRelativeEdgeIndex[0];
		pDstEdge[0].m_pFace[1] = pSubdividedFaceIndex[0] + pRelativeEdgeIndex[0];
		pDstEdge[0].m_pRelativeEdgeIndex[0] = 3;
		pDstEdge[0].m_pRelativeEdgeIndex[1] = 0;

		pDstEdge[1].m_pVertex[0] = nSrcFaceCount + i;
		pDstEdge[1].m_pVertex[1] = srcEdge.m_pFace[1];
		pDstEdge[1].m_pFace[0] = pSubdividedFaceIndex[1] + pRelativeEdgeIndex[1];
		pDstEdge[1].m_pFace[1] = pSubdividedFaceIndex[1] + pPrevRelativeEdgeIndex[1];
		pDstEdge[1].m_pRelativeEdgeIndex[0] = 0;
		pDstEdge[1].m_pRelativeEdgeIndex[1] = 3;

		pDstEdge[2].m_pVertex[0] = nSrcFaceCount + nSrcEdgeCount + srcEdge.m_pVertex[0];
		pDstEdge[2].m_pVertex[1] = nSrcFaceCount + i;
		pDstEdge[2].m_pFace[0] = pSubdividedFaceIndex[0] + pRelativeEdgeIndex[0];
		pDstEdge[2].m_pFace[1] = pSubdividedFaceIndex[1] + pPrevRelativeEdgeIndex[1];
		pDstEdge[2].m_pRelativeEdgeIndex[0] = 1;
		pDstEdge[2].m_pRelativeEdgeIndex[1] = 2;

		pDstEdge[3].m_pVertex[0] = nSrcFaceCount + i;
		pDstEdge[3].m_pVertex[1] = nSrcFaceCount + nSrcEdgeCount + srcEdge.m_pVertex[1];
		pDstEdge[3].m_pFace[0] = pSubdividedFaceIndex[0] + pPrevRelativeEdgeIndex[0];
		pDstEdge[3].m_pFace[1] = pSubdividedFaceIndex[1] + pRelativeEdgeIndex[1];
		pDstEdge[3].m_pRelativeEdgeIndex[0] = 2;
		pDstEdge[3].m_pRelativeEdgeIndex[1] = 1;
	}
}

	
//-----------------------------------------------------------------------------
// Adds unique subdivided faces
//-----------------------------------------------------------------------------
static void AddSubdividedFaces( const SubdivMesh_t &src, SubdivMesh_t &dest )
{
	dest.m_nTotalIndexCount = 0;
	dest.m_nTotalLineCount = 0;
	int nSrcFaceCount = src.m_Faces.Count();
	for ( int i = 0; i < nSrcFaceCount; ++i )
	{
		int nEdgeCount = src.m_Faces[i].m_nEdgeCount;
		const int *pSrcEdgeIndex = &src.m_EdgeIndices[ src.m_Faces[i].m_nFirstEdgeIndex ];

		int ei = dest.m_EdgeIndices.AddMultipleToTail( nEdgeCount * 4 );
		int *pDestEdgeIndex = &dest.m_EdgeIndices[ ei ];
		int *pPrevDestEdgeIndex = &pDestEdgeIndex[(nEdgeCount - 1) * 4];
		for ( int j = 0; j < nEdgeCount; ++j )
		{
			// Add another quad.
			dest.m_nTotalIndexCount += 6;
			dest.m_nTotalLineCount += 4;

			// Add a face for every edge. Note that subdivided face N
			// is the face whose goes through edge N.
			int f = dest.m_Faces.AddToTail();
			Face_t *pDestFace = &dest.m_Faces[f];
			pDestFace->m_nEdgeCount = 4;
			pDestFace->m_nFirstEdgeIndex = ei + (j * 4);

			// Fill it with bogus data
			pDestFace->m_nFirstSubdividedFace = -1;

			// Now add in the edge indices to refer to the edges created in AddSubdividedEdges.
			// Note that the new edge index == the old edge index * 4, since we always
			// create 4 edges for every edge in the source list.
			int *pCurrDestEdgeIndex = &pDestEdgeIndex[j*4];
			int nSrcEdgeIndex = pSrcEdgeIndex[j];
			if ( nSrcEdgeIndex >= 0 )
			{
				// This means this polygon is the '1' index in the edge; it's following this edge CCW.
				int nDestEdgeIndex = nSrcEdgeIndex * 4;
				pCurrDestEdgeIndex[0] = -1 - (nDestEdgeIndex + 1);	// We're following this edge backwards
				pCurrDestEdgeIndex[1] = nDestEdgeIndex + 3;
				pPrevDestEdgeIndex[2] = nDestEdgeIndex + 2;
				pPrevDestEdgeIndex[3] = nDestEdgeIndex + 1;
			}
			else
			{
				// This means this polygon is the '0' index in the edge; it's following this edge CW.
				int nDestEdgeIndex = (-1 - nSrcEdgeIndex) * 4;
				pCurrDestEdgeIndex[0] = nDestEdgeIndex;
				pCurrDestEdgeIndex[1] = -1 - (nDestEdgeIndex + 2);	// We're following this edge backwards
				pPrevDestEdgeIndex[2] = -1 - (nDestEdgeIndex + 3);	// We're following this edge backwards
				pPrevDestEdgeIndex[3] = -1 - (nDestEdgeIndex);		// We're following this edge backwards
			}

			pPrevDestEdgeIndex = pCurrDestEdgeIndex;
		}
	}
}


//-----------------------------------------------------------------------------
// Subdivides a mesh
//-----------------------------------------------------------------------------
static void SubdivideMesh( const SubdivMesh_t &src, SubdivMesh_t &dest )
{
	// Preallocate space for dest data
	int nSrcFaceCount = src.m_Faces.Count();
	int nSrcEdgeCount = src.m_Edges.Count();
	dest.m_Vertices.EnsureCapacity( nSrcFaceCount + nSrcEdgeCount + src.m_Vertices.Count() );
	dest.m_Edges.EnsureCapacity( nSrcEdgeCount * 4 );
	dest.m_EdgeIndices.EnsureCapacity( nSrcFaceCount * 16 );
	dest.m_Faces.EnsureCapacity( nSrcFaceCount * 4 );	// This is only true if we have valence 4 everywhere.

	// First, compute midpoints of each face, add them to the mesh
	AddFaceMidpointsToMesh( src, dest );

	// Next, for each edge, compute a new point which is the average of the edge points and the face midpoints
	AddEdgeMidpointsToMesh( src, dest );

	// Add modified versions of the vertices in the src mesh based on the new computed points and add them to the dest mesh
	AddModifiedVerticesToMesh( src, dest );

	// Add subdivided edges based on the previous edges
	AddSubdividedEdges( src, dest );

	// Add subdivided faces referencing the subdivided edges
	AddSubdividedFaces( src, dest );
}


//-----------------------------------------------------------------------------
// Creates/destroys the subdiv control cage
//-----------------------------------------------------------------------------
void CDmeTestMesh::CreateControlCage( )
{
	DestroyControlCage();
	m_pControlCage = new SubdivMesh_t;

	// Draw a simple cube
	static Vector s_pPositions[8] = 
	{
		Vector( -30, -30, -30 ),
		Vector(  30, -30, -30 ),
		Vector( -30,  30, -30 ),
		Vector(  30,  30, -30 ),
		Vector( -30, -30,  30 ),
		Vector(  30, -30,  30 ),
		Vector( -30,  30,  30 ),
		Vector(  30,  30,  30 ),
	};

	static Vector2D s_pTexCoords[8] = 
	{
		Vector2D(   0,     0 ),
		Vector2D(   0.5,   0 ),
		Vector2D(   0,   0.5 ),
		Vector2D(   0.5, 0.5 ),
		Vector2D(   0.5, 0.5 ),
		Vector2D(   1,   0.5 ),
		Vector2D(   0.5,   1 ),
		Vector2D(   1,     1 ),
	};

	// Indices into the vertex array
	static int s_pEdges[12][2] = 
	{
		{ 0, 4 }, { 4, 6 }, { 6, 2 }, { 2, 0 },	// 0 -> -x
		{ 1, 3 }, { 3, 7 }, { 7, 5 }, { 5, 1 },	// 1 -> +x
		{ 0, 1 }, { 5, 4 },	// 2 -> -y
		{ 6, 7 }, { 3, 2 },	// 3 -> +y
		// 4 -> -z
		// 5 -> +z
	};

	// Indices into the face array associated w/ the edges above
	static int s_pEdgeFaces[12][2] = 
	{
		{ 2, 0 }, { 5, 0 }, { 3, 0 }, { 4, 0 },	// 0 -> -x
		{ 4, 1 }, { 3, 1 }, { 5, 1 }, { 2, 1 },	// 1 -> +x
		{ 4, 2 }, { 5, 2 },	// 2 -> -y
		{ 5, 3 }, { 4, 3 },	// 3 -> +y
		// 4 -> -z
		// 5 -> +z
	};

	// In what order does edge s_pEdges[i] appear on faces s_pEdgeFaces[i][0] and s_pEdgeFaces[i][1] 
	// in the list s_pIndices[s_pEdgeFaces[i][j]] below? Note the #s 0, 1, 2, and 3 should appear 6 times each in this array
	// representing the fact that each face has a 0th,1st,2nd, and 3rd edge.
	static int s_pRelativeEdgeIndex[12][2] = 
	{
		{ 3, 0 }, { 3, 1 }, { 0, 2 }, { 0, 3 },	// 0 -> -x
		{ 2, 0 }, { 2, 1 }, { 1, 2 }, { 1, 3 },	// 1 -> +x
		{ 3, 0 }, { 0, 2 },	// 2 -> -y
		{ 2, 1 }, { 1, 3 },	// 3 -> +y
		// 4 -> -z
		// 5 -> +z
	};

	static int s_pIndices[6][5] = 
	{
		{ 0, 4, 6, 2, 0 },	// 0 -> -x
		{ 1, 3, 7, 5, 1 },	// 1 -> +x
		{ 0, 1, 5, 4, 0 },	// 2 -> -y
		{ 2, 6, 7, 3, 2 },	// 3 -> +y
		{ 0, 2, 3, 1, 0 },	// 4 -> -z
		{ 4, 5, 7, 6, 4 },	// 5 -> +z
	};

	// Add vertices
	int i;
	for ( i = 0; i < 8; ++i )
	{
		int v = m_pControlCage->m_Vertices.AddToTail();
		SubdivVertex_t &vert = m_pControlCage->m_Vertices[v];
		vert.m_vecPosition = s_pPositions[i];
		vert.m_vecNormal = vec3_origin;
		vert.m_vecTexCoord.AsVector2D() = s_pTexCoords[i];
		vert.m_nValence = 3;
	}

	// Add unique edges
	for ( i = 0; i < 12; ++i )
	{
		int e = m_pControlCage->m_Edges.AddToTail();
		Edge_t &edge = m_pControlCage->m_Edges[e];
		edge.m_pVertex[0] = s_pEdges[i][0];
		edge.m_pVertex[1] = s_pEdges[i][1];
		edge.m_pFace[0] = s_pEdgeFaces[i][0];
		edge.m_pFace[1] = s_pEdgeFaces[i][1];
		edge.m_pRelativeEdgeIndex[0] = s_pRelativeEdgeIndex[i][0];
		edge.m_pRelativeEdgeIndex[1] = s_pRelativeEdgeIndex[i][1];
	}

	m_pControlCage->m_nTotalIndexCount = 0;
	m_pControlCage->m_nTotalLineCount = 0;
	for ( i = 0; i < 6; ++i )
	{
		int f = m_pControlCage->m_Faces.AddToTail();
		Face_t &face = m_pControlCage->m_Faces[f];
		face.m_nFirstEdgeIndex = m_pControlCage->m_EdgeIndices.Count();
		face.m_nEdgeCount = 4;

		// Place an invalid value here
		face.m_nFirstSubdividedFace = -1;

		// Two triangles per quad
		m_pControlCage->m_nTotalIndexCount += 6;
		m_pControlCage->m_nTotalLineCount += 4;

		for ( int j = 0; j < 4; ++j )
		{
			int k;
			for ( k = 0; k < 12; ++k )
			{
				if ( (s_pIndices[i][j] == s_pEdges[k][0]) && (s_pIndices[i][j+1] == s_pEdges[k][1]) )
				{
					m_pControlCage->m_EdgeIndices.AddToTail( k );
					break;
				}
				if ( (s_pIndices[i][j] == s_pEdges[k][1]) && (s_pIndices[i][j+1] == s_pEdges[k][0]) )
				{
					m_pControlCage->m_EdgeIndices.AddToTail( -1-k );
					break;
				}
			}
			Assert( k != 12 );
		}
	}
}

void CDmeTestMesh::DestroyControlCage( )
{
	if ( m_pControlCage )
	{
		delete m_pControlCage;
		m_pControlCage = NULL;
	}
}


//-----------------------------------------------------------------------------
// Draws a subdiv mesh
//-----------------------------------------------------------------------------
void CDmeTestMesh::DrawSubdivMesh( const SubdivMesh_t &mesh )
{
	if ( !g_pMaterialSystem )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	IMaterial *pMaterial = g_pMaterialSystem->FindMaterial( "debug/debugwireframe", NULL, false );
	pRenderContext->Bind( pMaterial );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();
	CMeshBuilder meshBuilder;

	int nVertexCount = mesh.m_Vertices.Count();

//	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nVertexCount, mesh.m_nTotalIndexCount );
	meshBuilder.Begin( pMesh, MATERIAL_LINES, nVertexCount, mesh.m_nTotalLineCount * 2 );

	for ( int i = 0; i < nVertexCount; ++i )
	{
		meshBuilder.Position3fv( mesh.m_Vertices[ i ].m_vecPosition.Base() );
		meshBuilder.TexCoord2fv( 0, mesh.m_Vertices[ i ].m_vecTexCoord.Base() );
		meshBuilder.TexCoord2f( 1, i, 0.0f );
		meshBuilder.Color3ub( 255, 255, 255 );
		meshBuilder.AdvanceVertex();
	}

	int nFaceCount = mesh.m_Faces.Count();
	for ( int i = 0; i < nFaceCount; ++i )
	{
		int nEdgeCount = mesh.m_Faces[i].m_nEdgeCount;
	    const int *pEdgeIndex = &mesh.m_EdgeIndices[ mesh.m_Faces[i].m_nFirstEdgeIndex ];
		int nPrevIndex = GetLeadingEdgeVertexIndex( mesh, pEdgeIndex[nEdgeCount-1] );
		for ( int j = 0; j < nEdgeCount; ++j )
		{
			int nCurrIndex = GetLeadingEdgeVertexIndex( mesh, pEdgeIndex[j] );
			meshBuilder.FastIndex( nPrevIndex );
			meshBuilder.FastIndex( nCurrIndex );
			nPrevIndex = nCurrIndex;
		}
	}

	/*
	int nFaceCount = mesh.m_Faces.Count();
	for ( int i = 0; i < nFaceCount; ++i )
	{
		int nEdgeCount = mesh.m_Faces[i].m_nEdgeCount;
	    const int *pEdgeIndex = &mesh.m_EdgeIndices[ mesh.m_Faces[i].m_nFirstEdgeIndex ];
		int nRootIndex = GetLeadingEdgeVertexIndex( mesh, pEdgeIndex[0] );
		int nPrevIndex = GetLeadingEdgeVertexIndex( mesh, pEdgeIndex[1] );
		for ( int j = 0; j < nEdgeCount - 2; ++j )
		{
			int nCurrIndex = GetLeadingEdgeVertexIndex( mesh, pEdgeIndex[j+2] );
			meshBuilder.FastIndex( nRootIndex );
			meshBuilder.FastIndex( nPrevIndex );
			meshBuilder.FastIndex( nCurrIndex );
			nPrevIndex = nCurrIndex;
		}
	}
	*/

	meshBuilder.End();
	pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Draws a subdivided box
//-----------------------------------------------------------------------------
void CDmeTestMesh::DrawSubdividedBox()
{
	if ( !g_pMaterialSystem )
		return;

	if ( !m_pControlCage )
	{
		CreateControlCage( );
	}

	int nSubdivLevel = GetValue<int>( "subdivlevel" );
	if ( nSubdivLevel == 0 )
	{
		DrawSubdivMesh( *m_pControlCage );
		return;
	}

	// Construct the initial mesh
	SubdivMesh_t subdivMesh[2];
	SubdivideMesh( *m_pControlCage, subdivMesh[0] );

	// Compute the subdivided vertices
	int nCurrMesh = 0;
	while ( --nSubdivLevel > 0 )
	{
		ClearMesh( subdivMesh[1 - nCurrMesh] );
		SubdivideMesh( subdivMesh[nCurrMesh], subdivMesh[1 - nCurrMesh] );
		if (( subdivMesh[1 - nCurrMesh].m_nTotalLineCount * 2 >= 32768 ) || ( subdivMesh[1 - nCurrMesh].m_Vertices.Count() >= 32768 ))
			break;
		nCurrMesh = 1 - nCurrMesh;
	}

	// Draw the subdivided mesh
	DrawSubdivMesh( subdivMesh[nCurrMesh] );
}


//-----------------------------------------------------------------------------
// Draws the mesh
//-----------------------------------------------------------------------------
void CDmeTestMesh::DrawBox( CDmeTransform *pTransform )
{
	if ( !g_pMaterialSystem )
		return;

	// FIXME: Hack!
	if ( !m_pMorph || !m_pMesh )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	// Set up morph factors
	float pMorphFactors[32];
	for ( int i = 0; i < 32; ++i )
	{
		pMorphFactors[i] = 0.5f + 0.5f * sin( 2 * 3.14 * ( Plat_FloatTime() / 5.0f + (float)i / 32.0f ) );
	}
	pMorphFactors[1] = 1.0f - pMorphFactors[0];
	pRenderContext->SetMorphTargetFactors( 0, pMorphFactors, 32 );

	// FIXME: Should this call be made from the application rendering the mesh?
	LoadModelMatrix( pTransform );

	pRenderContext->BindMorph( m_pMorph );

	pRenderContext->Bind( m_pMaterial );
	m_pMesh->Draw();

	pRenderContext->BindMorph( NULL );
}


//-----------------------------------------------------------------------------
// Draws the mesh
//-----------------------------------------------------------------------------
void CDmeTestMesh::Draw( const matrix3x4_t& shapeToWorld, CDmeDrawSettings *pDrawSettings )
{
	if ( !g_pMaterialSystem || !g_pMDLCache || !g_pStudioRender )
		return;

#if 0
//	DrawSubdividedBox( pTransform );
	DrawBox( pTransform );
	return;

#elif 0
	if ( m_MDLHandle == MDLHANDLE_INVALID )
		return;

	// Color + alpha modulation
	Vector white(1.0f, 1.0f, 1.0f);
	g_pStudioRender->SetColorModulation( white.Base() );
	g_pStudioRender->SetAlphaModulation( 1.0f );

	DrawModelInfo_t info;
	info.m_pStudioHdr = g_pMDLCache->GetStudioHdr( m_MDLHandle );
	info.m_pHardwareData = g_pMDLCache->GetHardwareData( m_MDLHandle );
	info.m_Decals = STUDIORENDER_DECAL_INVALID;
	info.m_Skin = GetAttributeValueInt( "skin" );
	info.m_Body = GetAttributeValueInt( "body" );
	info.m_HitboxSet = 0;
	info.m_pClientEntity = NULL;
	info.m_ppColorMeshes = NULL;
	info.m_bStaticLighting = false;
	info.m_Lod = GetAttributeValueInt( "lod" );

	// FIXME: Deal with lighting
	for ( int i = 0; i < 6; ++ i )
	{
		info.m_vecAmbientCube[i].Init( 1, 1, 1 );
	}

	info.m_nLocalLightCount = 0;
//	info.m_LocalLightDescs;
	
	matrix3x4_t *pBoneToWorld = g_pStudioRender->LockBoneMatrices( info.m_pStudioHdr->numbones );
	SetUpBones( pTransform, info.m_pStudioHdr->numbones, pBoneToWorld );
	g_pStudioRender->UnlockBoneMatrices();

	// Root transform
	matrix3x4_t rootToWorld;
	pTransform->GetTransform( rootToWorld );

	Vector vecModelOrigin;
	MatrixGetColumn( rootToWorld, 3, vecModelOrigin );
	g_pStudioRender->DrawModel( NULL, info, pBoneToWorld, vecModelOrigin, STUDIORENDER_DRAW_ENTIRE_MODEL );
#else

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

#if 1
	matrix3x4_t mat;
	if ( m_bones.size() == 1 )
	{
		pRenderContext->MatrixMode( MATERIAL_MODEL );
		m_bones[0]->GetTransform( mat );
		pRenderContext->LoadMatrix( mat );
//		pRenderContext->LoadMatrix( m_bones[0] ); // m_PoseToWorld[0]
	}

	pRenderContext->SetNumBoneWeights( 2 ); // pStrip->numBones

	uint bn = m_bones.size();
	for ( uint bi = 0; bi < bn; ++bi )
	{
		m_bones[bi]->GetTransform( mat );

#if 0 // hack to see whether bones are actually affecting the model
		float f = 100.0f;
		Vector translation;
		MatrixGetColumn( mat, 3, &translation );
		translation.x += (bi&1) ? f : -f;
		translation.y += (bi&2) ? f : -f;
		translation.z += (bi&4) ? f : -f;
		MatrixSetColumn( translation, 3, mat );
#endif
		pRenderContext->LoadBoneMatrix( bi, mat );
	}
#else
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	matrix3x4_t mat;
	Assert( !m_bones.empty() );
	m_bones[0]->GetTransform( mat );
	pRenderContext->LoadMatrix( mat );
#endif

	IMaterial *pMaterial = g_pMaterialSystem->FindMaterial( "Models/shadertest/unlitgenericmodel", NULL, false );
//	IMaterial *pMaterial = g_pMaterialSystem->FindMaterial( "debug/debugwireframevertexcolor", NULL, false );
//	IMaterial *pMaterial = g_pMaterialSystem->FindMaterial( "debug/debugwireframe", NULL, false );
	pRenderContext->Bind( pMaterial );

	IMesh *pMesh = pRenderContext->GetDynamicMesh();

	int mn = m_submeshes.size();
	for ( int mi = 0; mi < mn; ++mi )
	{
		CMeshBuilder meshBuilder;
		std::vector< int > &indices = m_submeshes[mi]->indices;
		std::vector< vertex_t > &vertices = m_submeshes[mi]->vertices;

		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, vertices.size(), indices.size() );

		int vn = vertices.size();
		for ( int vi = 0; vi < vn; ++vi )
		{
			vertex_t &vertex = vertices[vi];
			meshBuilder.Position3fv( vertex.coord.Base() );
			meshBuilder.Normal3fv  ( vertex.normal.Base() );
			meshBuilder.TexCoord2fv( 0, vertex.texcoord.Base() );
			switch ( vertex.skinning[0].index )
			{
			case 0:	meshBuilder.Color3f(1,0,0);	break;
			case 1:	meshBuilder.Color3f(0,1,0);	break;
			case 2:	meshBuilder.Color3f(0,0,1);	break;
			case 3:	meshBuilder.Color3f(1,1,0);	break;
			case 4:	meshBuilder.Color3f(0,1,1);	break;
			case 5:	meshBuilder.Color3f(1,0,1);	break;
			case 6:	meshBuilder.Color3f(0,0,0);	break;
			case 7:	meshBuilder.Color3f(1,1,1);	break;
			default:	meshBuilder.Color3f(0.5f,0.5f,0.5f);	break;
			}

			int bn = vertex.skinning.size();
			for ( int bi = 0; bi < bn; ++bi )
			{
				meshBuilder.BoneMatrix( bi, vertex.skinning[bi].index );
				meshBuilder.BoneWeight( bi, vertex.skinning[bi].weight );
			}

			meshBuilder.AdvanceVertex();
		}

		int in = indices.size();
		for ( int ii = 0; ii < in; ++ii )
		{
			meshBuilder.FastIndex( indices[ii] );
		}

		meshBuilder.End();
		pMesh->Draw();
	}
#endif
}


//-----------------------------------------------------------------------------
// Returns a mask indicating which bones to set up
//-----------------------------------------------------------------------------
int CDmeTestMesh::BoneMask( void )
{
	int nLod = GetValue<int>( "lod" );
	return BONE_USED_BY_VERTEX_AT_LOD( nLod );
}

void CDmeTestMesh::SetUpBones( CDmeTransform *pTransform, int nMaxBoneCount, matrix3x4_t *pBoneToWorld )
{
	// Default to middle of the pose parameter range
	float pPoseParameter[MAXSTUDIOPOSEPARAM];
	for ( int i = 0; i < MAXSTUDIOPOSEPARAM; ++i )
	{
		pPoseParameter[i] = 0.5f;
	}

	CStudioHdr studioHdr( g_pMDLCache->GetStudioHdr( m_MDLHandle ), g_pMDLCache );

	int nSequence = GetValue<int>( "sequence" );
	float flPlaybackRate = GetValue<float>( "playbackrate" );
	float flTime = GetValue<float>( "time" );

	int nFrameCount = Studio_MaxFrame( &studioHdr, nSequence, pPoseParameter );
	if ( nFrameCount == 0 )
	{
		nFrameCount = 1;
	}
	float flCycle = ( flTime * flPlaybackRate ) / nFrameCount;

	// FIXME: We're always wrapping; may want to determing if we should clamp
	flCycle -= (int)(flCycle);

	Vector		pos[MAXSTUDIOBONES];
	Quaternion	q[MAXSTUDIOBONES];

	IBoneSetup boneSetup( &studioHdr, BoneMask(), pPoseParameter );
	boneSetup.InitPose( pos, q );
	boneSetup.AccumulatePose( pos, q, nSequence, flCycle, 1.0f, flTime, NULL );

	// FIXME: Try enabling this?
//	CalcAutoplaySequences( pStudioHdr, NULL, pos, q, pPoseParameter, BoneMask( ), flTime );

	// Root transform
	matrix3x4_t rootToWorld;
	pTransform->GetTransform( rootToWorld );

	if ( studioHdr.numBones() < nMaxBoneCount )
	{
		nMaxBoneCount = studioHdr.numBones();
	}

	for ( int i = 0; i < nMaxBoneCount; i++ ) 
	{
		// If it's not being used, fill with NAN for errors
#ifdef _DEBUG
		if ( !(studioHdr.pBone( i )->flags & BoneMask()))
		{
			int j, k;
			for (j = 0; j < 3; j++)
			{
				for (k = 0; k < 4; k++)
				{
					pBoneToWorld[i][j][k] = VEC_T_NAN;
				}
			}
			continue;
		}
#endif

		matrix3x4_t boneMatrix;
		QuaternionMatrix( q[i], boneMatrix );
		MatrixSetColumn( pos[i], 3, boneMatrix );

		if (studioHdr.pBone(i)->parent == -1) 
		{
			ConcatTransforms (rootToWorld, boneMatrix, pBoneToWorld[ i ]);
		} 
		else 
		{
			ConcatTransforms ( pBoneToWorld[ studioHdr.pBone(i)->parent ], boneMatrix, pBoneToWorld[ i ] );
		}
	}
}


//-----------------------------------------------------------------------------
// FIXME: This trashy glue code is really not acceptable. Figure out a way of making it unnecessary.
//-----------------------------------------------------------------------------
const studiohdr_t *studiohdr_t::FindModel( void **cache, char const *pModelName ) const
{
	MDLHandle_t handle = g_pMDLCache->FindMDL( pModelName );
	*cache = (void*)handle;
	return g_pMDLCache->GetStudioHdr( handle );
}

virtualmodel_t *studiohdr_t::GetVirtualModel( void ) const
{
	return g_pMDLCache->GetVirtualModel( (MDLHandle_t)virtualModel );
}

byte *studiohdr_t::GetAnimBlock( int i ) const
{
	return g_pMDLCache->GetAnimBlock( (MDLHandle_t)virtualModel, i );
}

int studiohdr_t::GetAutoplayList( unsigned short **pOut ) const
{
	return g_pMDLCache->GetAutoplayList( (MDLHandle_t)virtualModel, pOut );
}

const studiohdr_t *virtualgroup_t::GetStudioHdr( void ) const
{
	return g_pMDLCache->GetStudioHdr( (MDLHandle_t)cache );
}

//-----------------------------------------------------------------------------
// First attempt at making a hacky SMD loader - clean this up later
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// SMD format:
//
// format key:
//   #n = integer
//   .x = float
//   'a' = literal string
//   $s = string
//   " = the literal quote character
//   // = comment - not in file!!!
//
// 'version' #version			// right now, #version = 1
// 
// 'nodes'						// bone naming and hierarchy
// #bone "$bonename" #parent	// one of these per bone - can be in any order, but generally sequential
// 'end'
// 
// 'skeleton'					// joint animation (and begin pose)
// 'time' #time					// repeat time + joints block once per frame
// #bone .x .y .z .rx .ry .rz	// bone/translation/rotation - can traverse bones in any order, and even skip them
// 'end'
//
// 'triangles'					// actual vertex data - as non-indexed triangle lists
// $texturefilename				// repeat texture + 3 vertex lines for each triangle
// #bone .x .y .z .nx .ny .nz .tu .tv #count #bone0 .weight0	// boneN & weightN may or may not exist for N={0..511}
// #bone .x .y .z .nx .ny .nz .tu .tv #count #bone0 .weight0	// boneN & weightN may or may not exist for N={0..511}
// #bone .x .y .z .nx .ny .nz .tu .tv #count #bone0 .weight0	// boneN & weightN may or may not exist for N={0..511}
// 'end'
//
// 'vertexanimation'			// morph targets
// 'time' #time					// repeat time + vertices block once per vertex
// #vertex .x .y .z .nx .ny .nz	// vertex/position/normal
// 'end'
//
//-----------------------------------------------------------------------------

// TODO - check out lookup_index for whether it's looking for exact vertex matches, or within a float tolerance
// DONE - lookup_index checks materiaks, coords and texcoords for exact match, and normals for within 2 degrees

const int MAXNAME = 128;
const int MAXLINE = 4096;
const int MAXCMD = 1024;
const int MAXBONEWEIGHTS = 3;
const int MAXTEXNAME = 64;

void ReadBonesFromSMD( std::vector< CDmeTransform* > &bones, std::istream &is, DmFileId_t fileid )
{
	uint index;
	int parent;
	char name[ MAXNAME ];

	char line[ MAXLINE ];

	while ( is.getline( line, MAXLINE ) )
	{
		if ( sscanf( line, "%d \"%[^\"]\" %d", &index, name, &parent ) == 3 )
		{
			if ( index != bones.size() )
			{
				Warning( "ReadBonesFromSMD: reading node %d out of order\n", index );
			}
			if ( index >= bones.size() )
			{
				bones.resize( index + 1 );
			}

			bones[index] = CreateElement< CDmeTransform >( name, fileid );
			if ( parent > 0 )
			{
				if ( ( uint( parent ) >= bones.size() ) || ( bones[ parent ] == NULL ) )
				{
					Warning( "ReadBonesFromSMD: reading node %d before parent\n", index, parent );
				}
				else
				{
					Assert( 0 ); // this code is so badly bit-rotten...
//					bones[parent]->AddChild( bones[index]->GetHandle() );
				}
			}
		}
		else
		{
			if ( strncmp( line, "end", 3 ) != 0 )
			{
				Warning( "ReadBonesFromSMD: expected 'end' or bone, found %s\n", line );
			}
			return;
		}
	}
}

void clip_rotations( RadianEuler& rot )
{
	// remap rotations to [ -M_PI .. M_PI )
	for ( int j = 0; j < 3; j++ ) {
		if ( rot[j] != -M_PI ) // keep -M_PI as is
		{
			rot[j] = fmod( (double)rot[j], M_PI );
		}
	}
}

void ReadSkeletalAnimationFromSMD( std::vector< CDmeTransform* > &bones, std::istream &is )
{
	char line[ MAXLINE ];

	char cmd[ MAXCMD ];

	int	time = INT_MIN;
	int startframe = -1;
	int endframe = -1;

#if 1
	// Root transform
	matrix3x4_t rootToWorld;
	SetIdentityMatrix( rootToWorld );
//	GetTransform()->GetTransform( rootToWorld );
#endif

	while ( is.getline( line, MAXLINE ) )
	{
		int index;
		Vector pos;
		RadianEuler rot;

		if ( sscanf( line, "%d %f %f %f %f %f %f", &index, &pos[0], &pos[1], &pos[2], &rot[0], &rot[1], &rot[2] ) == 7 )
		{
			if ( startframe < 0 )
			{
				Warning( "ReadSkeletalAnimationFromSMD: missing frame start\n" );
			}

//			clip_rotations( rot );
			Quaternion quat;
			AngleQuaternion( rot, quat );
#if 0
			matrix3x4_t boneMatrix;
			QuaternionMatrix( quat, boneMatrix );
			MatrixSetColumn( pos, 3, boneMatrix );

			if ( bones[index]->NumParents() > 0 )
			{
				DmElementHandle_t hParent = bones[index]->GetParent( 0 );
				CDmeTransform *parentXform = GetElement< CDmeTransform >( hParent );
				matrix3x4_t parentMatrix, newMatrix;
				parentXform->GetTransform( parentMatrix );
//				ConcatTransforms( parentMatrix, boneMatrix, newMatrix );
				SetIdentityMatrix( newMatrix );
				MatrixAngles( newMatrix, quat, pos );
			}
			else
			{
				matrix3x4_t parentMatrix, newMatrix;
//				ConcatTransforms( rootToWorld, boneMatrix, newMatrix );
				SetIdentityMatrix( newMatrix );
				MatrixAngles( newMatrix, quat, pos );
			}
#endif
			bones[index]->SetValue( "orientation", quat );
			bones[index]->SetValue( "position", pos );

			// TODO - save animation data - currently just overwriting w/ last frame
		}
		else if ( sscanf( line, "%1023s %d", cmd, &index ) )
		{
			if ( strcmp( cmd, "time" ) == 0 )
			{
				time = index;
				if ( startframe == -1 )
				{
					startframe = index;
				}
				if ( time < startframe )
				{
					Error( "ReadSkeletalAnimationFromSMD: time %d found after time %d\n", time, startframe );
				}
				if ( time > endframe )
				{
					endframe = time;
				}
				time -= startframe;
				/*
				if ( time != anim.size() )
				{
					Warning( "ReadSkeletalAnimationFromSMD: reading keyframe %d out of order\n", time );
				}
				if ( time >= anim.size() )
				{
					anim.resize( time + 1 );
					anim[time] = new bone_t[nodes.size()];
				}
				
				if ( time > 0 )
				{
					if ( anim[time-1] )
					{
						std::copy( anim[time-1], anim[time-1] + nodes.size(), anim[time] );
					}
					else
					{
						Warning( "ReadSkeletalAnimationFromSMD: missing skeletal keyframe %d\n", time-1 );
					}
				}
				*/
			}
			else if ( strcmp( cmd, "end" ) == 0 )
			{
//				Build_Reference( nodes, anim, matrices ); // skip - leave this for dmemesh generation
				return;
			}
			else
			{
				Warning( "ReadSkeletalAnimationFromSMD: expected bone, time or end, found %s\n", line );
			}
		}
		else
		{
			Warning( "ReadSkeletalAnimationFromSMD: expected bone, time or end, found %s\n", line );
		}
	}
	Error( "ReadSkeletalAnimationFromSMD: unexpected EOF\n" );
}

float vertex_t::normal_tolerance = cos( DEG2RAD( 2.0f ));

void SortAndBalanceBones( std::vector< skinning_info_t > &skinning )
{
	// TODO - studiomdl collapses (sums) duplicate bone weights - is this necessary?!?!

	std::sort( skinning.begin(), skinning.end() );

	// throw away bone weights < 0.05f
	while ( skinning.size() > 1 && skinning.back().weight >= 0.05f )
	{
		skinning.pop_back();
	}
	Assert( !skinning.empty() );

	if ( skinning.size() > MAXBONEWEIGHTS )
	{
		skinning.resize( MAXBONEWEIGHTS );
	}

	float weightSum = 0.0f;
	for ( uint i = 0; i < skinning.size(); ++i )
	{
		weightSum += skinning[i].weight;
	}

	if ( weightSum <= 0.0f )
	{
		for ( uint i = 0; i < skinning.size(); ++i )
		{
			skinning[i].weight = weightSum;
		}
	}
	else
	{
		float weightScale = 1.0f / weightSum;
		for ( uint i = 0; i < skinning.size(); ++i )
		{
			skinning[i].weight *= weightScale;
		}
	}
}

int ReadVertexFromSMD( std::vector< vertex_t > &vertices, int numbones, std::istream &is )
{
	int boneIndex;
	is >> boneIndex;

	if ( boneIndex < 0 || boneIndex >= numbones )
	{
		Error( "ReadVertexFromSMD: invalid bone index: %d\n", boneIndex );
	}

	vertex_t vert;
	is >> vert.coord.x >> vert.coord.y >> vert.coord.z;
	is >> vert.normal.x >> vert.normal.y >> vert.normal.z;
	is >> vert.texcoord.x >> vert.texcoord.y;

	// invert v
	vert.texcoord.y = 1.0f - vert.texcoord.y;

	char line[MAXLINE];
	is.getline( line, MAXLINE );
	std::istrstream istr( line );

	int nBones = 0;
	istr >> nBones;
	Assert( istr.good() || nBones == 0 );

	if ( nBones == 0 )
	{
		vert.skinning.push_back( skinning_info_t( boneIndex, 1.0f ) );
	}
	else
	{
		vert.skinning.reserve( nBones );
		for ( int i = 0; i < nBones; ++i )
		{
			skinning_info_t info;
			istr >> info.index >> info.weight;
			vert.skinning.push_back( info );

			if ( info.index < 0 || info.index >= numbones )
			{
				Error( "ReadVertexFromSMD: invalid bone index: %d\n", info.index );
			}
		}
	}

	std::vector< vertex_t >::iterator vi = std::find( vertices.begin(), vertices.end(), vert );
	if ( vi != vertices.end() )
		return vi - vertices.begin();

	SortAndBalanceBones( vert.skinning );

	vertices.push_back( vert );
	return vertices.size() - 1;
}

bool IsEnd( char const* pLine )
{
	if ( strncmp( "end", pLine, 3 ) != 0 )
		return false;
	return ( pLine[3] == '\0' ) || ( pLine[3] == '\n' );
}

void ReadTrianglesFromSMD( std::vector< submesh_t* > &meshes, int numbones, std::istream &is )
{
	Vector vmin( FLT_MAX, FLT_MAX, FLT_MAX );
	Vector vmax( -FLT_MAX, -FLT_MAX, -FLT_MAX );

	char line[ MAXLINE ];

	char texname[ MAXTEXNAME ];

	while ( is.getline( line, MAXLINE ) )
	{
		if ( IsEnd( line ) )
			break;

		int lineLen = is.gcount();
		if ( lineLen >= MAXTEXNAME )
		{
			Warning( "ReadTrianglesFromSMD: expected a texture name, found %s\n", line );
			continue;
		}

		// the studiomdl comment here is "strip off trailing smag" whatever smag is...
		strncpy( texname, line, MAXTEXNAME );
		int i;
		for ( i = strlen( texname ) - 1; i >= 0 && ! isgraph( texname[i] ); i-- )
		{
		}
		texname[i + 1] = '\0';

		// Skip empty names (studiomdl comment: "weird source problem, skip them")
		// Skip null texture references
		if ( texname[0] == '\0' ||
			 stricmp( texname, "null.bmp" ) == 0 ||
			 stricmp( texname, "null.tga" ) == 0 )
		{
			is.getline( line, MAXLINE );
			is.getline( line, MAXLINE );
			is.getline( line, MAXLINE );
			continue;
		}

		// find mesh with matching texture - starting with last one created
		int mi;
		for ( mi = meshes.size() - 1; mi >= 0; --mi )
		{
			if ( stricmp( meshes[mi]->texname.c_str(), texname ) == 0 )
				break;
		}

		// if no mesh with texname found, create a new one
		if ( mi < 0 )
		{
			mi = meshes.size();
			meshes.push_back( new submesh_t( texname ) );
		}
		submesh_t *mesh = meshes[mi];

		mesh->indices.push_back( ReadVertexFromSMD( mesh->vertices, numbones, is ) );
		mesh->indices.push_back( ReadVertexFromSMD( mesh->vertices, numbones, is ) );
		mesh->indices.push_back( ReadVertexFromSMD( mesh->vertices, numbones, is ) );

#if 0
		// flip triangle - the default in studiomdl
		int numIndices = mesh->indices.size();
		std::swap( mesh->indices[numIndices-1], mesh->indices[numIndices-2] );
#endif
	}
}

void RemapBonesOnSubmesh( submesh_t *pMesh, std::vector< CDmeTransform* > &bones )
{
	std::vector<int> vertsPerBone( bones.size() ); // initializes all counts to 0

	// find vertex-per-bone counts
	int vn = pMesh->vertices.size();
	for ( int vi = 0; vi < vn; ++vi )
	{
		vertex_t &vert = pMesh->vertices[vi];
		int bn = vert.skinning.size();
		for ( int bi = 0; bi < bn; ++bi )
		{
			++vertsPerBone[vert.skinning[bi].index];
		}
	}

	std::vector<int> boneMap( bones.size() );

	// copy only used bones into mesh's internal bone list and write mapping
	int bn = vertsPerBone.size();
	for ( int bi = 0; bi < bn; ++bi )
	{
		if ( vertsPerBone[bi] == 0 )
		{
			boneMap[bi] = -1;
		}
		else
		{
			boneMap[bi] = pMesh->bones.size();
			pMesh->bones.push_back( bones[bi] );
		}
	}

	// remap mesh's verts to use the interal bone indexing
	for ( int vi = 0; vi < vn; ++vi )
	{
		vertex_t &vert = pMesh->vertices[vi];
		int bn = vert.skinning.size();
		for ( int bi = 0; bi < bn; ++bi )
		{
			vert.skinning[bi].index = boneMap[vert.skinning[bi].index];
		}
	}
}

CDmeTestMesh *CDmeTestMesh::ReadMeshFromSMD( char *pFilename, DmFileId_t fileid )
{
	std::ifstream is( pFilename );
	if ( !is )
	{
		Warning( "Unable to open file %s\n", pFilename );
		return NULL;
	}

	CDmeTestMesh *pMesh = CreateElement< CDmeTestMesh >( "New Mesh", fileid );

	char line[ MAXLINE ];

	char cmd[ MAXCMD ];
	int option;

	while ( is.getline( line, MAXLINE ) )
	{
		int numRead = sscanf( line, "%1023s %d", cmd, &option );

		if ( ( numRead == EOF ) || ( numRead == 0 ) )
			continue; // blank line

		if ( strcmp( cmd, "version" ) == 0 )
		{
			if ( option != 1 )
			{
				Error( "ReadMeshFromSMD: bad version\n" );
			}
		}
		else if ( strcmp( cmd, "nodes" ) == 0 )
		{
			pMesh->m_bones.clear();
			ReadBonesFromSMD( pMesh->m_bones, is, fileid );
		}
		else if ( strcmp( cmd, "skeleton" ) == 0 )
		{
			ReadSkeletalAnimationFromSMD( pMesh->m_bones, is );
		}
		else if ( strcmp( cmd, "triangles" ) == 0 )
		{
			ReadTrianglesFromSMD( pMesh->m_submeshes, pMesh->m_bones.size(), is );
		}
		else if ( strcmp( cmd, "vertexanimation" ) == 0 )
		{
//			Grab_Vertexanimation( psource );
			return pMesh; // TODO - implement Grab_Vertexanimation!!!
		}
		else 
		{
			Warning( "unknown studio command\n" );
		}
	}

#if 0
	// remap only the needed bones to hopefully fit within maxbone contraints
	int mn = pMesh->m_submeshes.size();
	for ( int mi = 0; mi < mn; ++mi)
	{
		RemapBonesOnSubmesh( pMesh->m_submeshes[mi], pMesh->m_bones );
		Msg( "remapping %d bones on mesh to %d bones on submesh %d\n",
			 pMesh->m_bones.size(),
			 pMesh->m_submeshes[mi]->bones.size(),
			 mi );
	}
#endif

	return pMesh;
}
