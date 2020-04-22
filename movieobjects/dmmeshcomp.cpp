//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Implementation of CDmMeshComp - CDmeMesh computation class
//
//=============================================================================


// Valve includes
#include "movieobjects/dmmeshcomp.h"
#include "movieobjects/dmefaceset.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmevertexdata.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//=============================================================================
//
//=============================================================================
CDmMeshComp::CDmMeshComp( CDmeMesh *pMesh, CDmeVertexData *pPassedBase )
: m_pMesh( pMesh )
, m_pBase( NULL )
{
	m_pBase = pPassedBase ? pPassedBase : pMesh->GetCurrentBaseState();
	if ( !m_pBase )
		return;

	const FieldIndex_t pIndex( m_pBase->FindFieldIndex( CDmeVertexData::FIELD_POSITION ) );
	if ( pIndex < 0 )
		return;

	const CUtlVector< Vector > &pPositionData( m_pBase->GetPositionData() );
	const CUtlVector<int> &pPositionIndices( m_pBase->GetVertexIndexData( CDmeVertexData::FIELD_POSITION ) );

	const int nVertices( pPositionData.Count() );
	if ( nVertices <= 0 )
		return;

	// Create vertices
	// TODO: check for duplicates in pPositionData - that would break this algorithm
	m_verts.EnsureCapacity( nVertices );
	for ( int i = 0; i < nVertices; ++i )
	{
		const CUtlVector< int > &vertexIndices = m_pBase->FindVertexIndicesFromDataIndex( CDmeVertexData::FIELD_POSITION, i );
		m_verts.AddToTail( new CVert( i, &vertexIndices, &pPositionData[ i ] ) );
	}

	// Create edges and faces
	const int nFaceSets( pMesh->FaceSetCount() );
	for ( int i = 0; i < nFaceSets; ++i )
	{
		CDmeFaceSet *pFaceSet( pMesh->GetFaceSet( i ) );
		const int nIndices( pFaceSet->NumIndices() );
		if ( nIndices < 4 )	// At least a triangle and a -1
			continue;

		m_faces.EnsureCapacity( m_faces.Count() + nIndices / 4 ); // # new faces <= nIndices/4 (tri + -1)
		m_edges.EnsureCapacity( m_edges.Count() + nIndices / 2 ); // # new edges <= 2*new faces

		int facePosIndex( -1 );
		int edgePosIndex0( -1 );
		int edgePosIndex1( -1 );

		CUtlVector< CVert * > verts;
		CUtlVector< CEdge * > edges;
		CUtlVector< bool > edgeReverseMap;
		bool bReverse = false;

		for ( int j( 0 ); j < nIndices; ++j )
		{
			const int faceVertexIndex( pFaceSet->GetIndex( j ) );

			if ( faceVertexIndex < 0 )
			{
				// End of face
				edgePosIndex0 = edgePosIndex1;
				edgePosIndex1 = facePosIndex;

				Assert( edgePosIndex0 >= 0 );
				Assert( edgePosIndex1 >= 0 );

				edges.AddToTail( FindOrCreateEdge( edgePosIndex0, edgePosIndex1, &bReverse ) );
				edgeReverseMap.AddToTail( bReverse );

				CreateFace( verts, edges, edgeReverseMap );

				facePosIndex = -1;
				verts.RemoveAll();
				edges.RemoveAll();
				edgeReverseMap.RemoveAll();
				continue;
			}

			if ( facePosIndex < 0 )
			{
				// First vertex
				facePosIndex = pPositionIndices[ faceVertexIndex ];
				edgePosIndex1 = facePosIndex;
				verts.AddToTail( m_verts[ edgePosIndex1 ] );
				continue;
			}

			// 2nd through last vertex
			edgePosIndex0 = edgePosIndex1;
			edgePosIndex1 = pPositionIndices[ faceVertexIndex ];
			verts.AddToTail( m_verts[ edgePosIndex1 ] );

			Assert( edgePosIndex0 >= 0 );
			Assert( edgePosIndex1 >= 0 );

			edges.AddToTail( FindOrCreateEdge( edgePosIndex0, edgePosIndex1, &bReverse ) );
			edgeReverseMap.AddToTail( bReverse );
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmMeshComp::~CDmMeshComp()
{
	m_verts.PurgeAndDeleteElements();
	m_edges.PurgeAndDeleteElements();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmMeshComp::CVert::CVert( int nPositionIndex, const CUtlVector< int > *pVertexIndices, const Vector *pPosition )
: m_positionIndex( nPositionIndex )
, m_pVertexIndices( pVertexIndices )
, m_pPosition( pPosition )
, m_edges( 8, 8 )
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmMeshComp::CVert::CVert( const CVert &src )
: m_positionIndex( src.m_positionIndex )
, m_pVertexIndices( src.m_pVertexIndices )
, m_pPosition( src.m_pPosition )
, m_edges( 8, 8 )
{
	m_edges.AddMultipleToTail( src.m_edges.Count(), src.m_edges.Base() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmMeshComp::CVert::PositionIndex() const
{
	return m_positionIndex;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const Vector *CDmMeshComp::CVert::Position() const
{
	return m_pPosition;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const CUtlVector< int > *CDmMeshComp::CVert::VertexIndices() const
{
	return m_pVertexIndices;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmMeshComp::CVert::operator==( const CVert &rhs ) const
{
	return ( m_pPosition->DistToSqr( *rhs.m_pPosition ) < FLT_EPSILON );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmMeshComp::CEdge::CEdge()
: m_pVert0( NULL )
, m_pVert1( NULL )
, m_faceCount( 0 )
{
}


//-----------------------------------------------------------------------------
// Returns the vertex position index given the edge relative vertex index
//-----------------------------------------------------------------------------
int CDmMeshComp::CEdge::GetVertPositionIndex( int edgeRelativeVertexIndex ) const
{
	if ( edgeRelativeVertexIndex == 0 && m_pVert0 )
		return m_pVert0->PositionIndex();

	if ( edgeRelativeVertexIndex == 1 && m_pVert1 )
		return m_pVert1->PositionIndex();

	return -1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmMeshComp::CVert *CDmMeshComp::CEdge::GetVert( int edgeRelativeVertexIndex ) const
{
	if ( edgeRelativeVertexIndex == 0 )
		return m_pVert0;

	if ( edgeRelativeVertexIndex == 1 )
		return m_pVert1;

	return NULL;
}


//-----------------------------------------------------------------------------
// Returns true if the edge starts and stops at the same position in space
// The order of the vertices is not checked
//-----------------------------------------------------------------------------
bool CDmMeshComp::CEdge::operator==( const CEdge &rhs ) const
{
	return (
		( *m_pVert0 == *rhs.m_pVert0 && *m_pVert1 == *rhs.m_pVert1 ) ||
		( *m_pVert0 == *rhs.m_pVert1 && *m_pVert1 == *rhs.m_pVert0 ) );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
Vector CDmMeshComp::CEdge::EdgeVector() const
{
	if ( m_pVert0 && m_pVert1 )
		return *m_pVert1->Position() - *m_pVert0->Position();

	return vec3_origin;
}


//-----------------------------------------------------------------------------
// Finds or Creates an edge... Can still return NULL if vertices do not exist
//-----------------------------------------------------------------------------
CDmMeshComp::CEdge *CDmMeshComp::FindOrCreateEdge( int vIndex0, int vIndex1, bool *pReverse /* = NULL */ )
{
	CEdge *pEdge = FindEdge( vIndex0, vIndex1, pReverse );
	if ( pEdge )
		return pEdge;

	CVert *pVert0 = m_verts[ vIndex0 ];
	if ( pVert0 == NULL )
		return NULL;

	CVert *pVert1 = m_verts[ vIndex1 ];
	if ( pVert1 == NULL )
		return NULL;

	pEdge = m_edges[ m_edges.AddToTail( new CEdge() ) ];
	pEdge->m_pVert0 = pVert0;
	pEdge->m_pVert1 = pVert1;
	pVert0->m_edges.AddToTail( pEdge );
	if ( vIndex0 != vIndex1 )
		pVert1->m_edges.AddToTail( pEdge );

	if ( pReverse )
	{
		*pReverse = false;
	}

	return pEdge;
}


//-----------------------------------------------------------------------------
// Returns the edge between vIndex0 & vIndex1 (or vice versa), NULL if not found
//-----------------------------------------------------------------------------
CDmMeshComp::CEdge *CDmMeshComp::FindEdge( int vIndex0, int vIndex1, bool *pReverse /* = NULL */ )
{
	CUtlVector< CEdge * > &edges = m_verts[ vIndex0 ]->m_edges;
	for ( int i = 0; i < edges.Count(); i++ )
	{
		CEdge *e = edges[ i ];

		if ( e->GetVertPositionIndex( 0 ) == vIndex0 && e->GetVertPositionIndex( 1 ) == vIndex1 )
		{
			if ( pReverse )
			{
				*pReverse = false;
			}
			return e;
		}

		if ( e->GetVertPositionIndex( 1 ) == vIndex0 && e->GetVertPositionIndex( 0 ) == vIndex1 )
		{
			if ( pReverse )
			{
				*pReverse = true;
			}
			return e;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmMeshComp::CFace *CDmMeshComp::CreateFace( const CUtlVector< CVert * > &verts, const CUtlVector< CEdge * > &edges, const CUtlVector< bool > &edgeReverseMap )
{
	CFace *pFace = &m_faces[ m_faces.AddToTail() ];

	pFace->m_verts.RemoveAll();
	pFace->m_verts.AddVectorToTail( verts );

	pFace->m_edges.RemoveAll();
	pFace->m_edges.AddVectorToTail( edges );

	pFace->m_edgeReverseMap.RemoveAll();
	pFace->m_edgeReverseMap.AddVectorToTail( edgeReverseMap );

	for ( int nEdgeIndex = edges.Count() - 1; nEdgeIndex >= 0; --nEdgeIndex )
	{
		edges[ nEdgeIndex ]->m_faceCount += 1;
	}

	return pFace;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmMeshComp::FindFacesWithVert( int vIndex, CUtlVector< CFace * > &faces )
{
	// TODO: optimize this by adding a vector of face pointers to each vertex
	faces.RemoveAll();

	for ( int fi( m_faces.Head() ); fi != m_faces.InvalidIndex(); fi = m_faces.Next( fi ) )
	{
		CFace &face( m_faces[ fi ] );
		for ( int i = 0; i < face.m_verts.Count(); ++i )
		{
			if ( face.m_verts[ i ]->PositionIndex() == vIndex )
			{
				faces.AddToTail( &face );
				break;
			}
		}
	}

	return faces.Count();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmMeshComp::FindNeighbouringVerts( int vIndex, CUtlVector< CVert * > &verts )
{
	verts.RemoveAll();

	const CUtlVector< CEdge * > & edges = m_verts[ vIndex ]->m_edges;

	for ( int i = 0; i < edges.Count(); ++i )
	{
		CEdge *e = edges[ i ];
		if ( e->GetVertPositionIndex( 0 ) == vIndex )
		{
			verts.AddToTail( e->GetVert( 1 ) );
		}
		else
		{
			verts.AddToTail( e->GetVert( 0 ) );
		}
	}

	return verts.Count();
}


//-----------------------------------------------------------------------------
// Find all edges that are only used by 1 face
//-----------------------------------------------------------------------------
int CDmMeshComp::GetBorderEdges( CUtlVector< CUtlVector< CEdge * > > &borderEdgesList )
{
	// TODO: optimize this by stepping from edge to edge to build chains, using CVert::m_edges
	int retVal = 0;

	borderEdgesList.RemoveAll();

	bool connected;

	for ( int ei = 0; ei < m_edges.Count(); ei++ )
	{
		CEdge *pEdge = m_edges[ ei ];
		if ( pEdge->IsBorderEdge() )
		{
			++retVal;
			connected = false;

			for ( int i = borderEdgesList.Count() - 1; !connected && i >= 0; --i )
			{
				CUtlVector< CEdge * > &borderEdges = borderEdgesList[ i ];
				for ( int j = borderEdges.Count() - 1; j >= 0; --j )
				{
					if ( borderEdges[ j ]->ConnectedTo( pEdge ) )
					{
						borderEdges.AddToTail( pEdge );
						connected = true;
						break;
					}
				}
			}

			if ( !connected )
			{
				CUtlVector< CEdge * > &borderEdges = borderEdgesList[ borderEdgesList.AddToTail() ];
				borderEdges.AddToTail( pEdge );
			}
		}
	}

	// Shrink the borderEdgesList to minimum number required

	bool anyConnected = false;
	do 
	{
		anyConnected = false;

		for ( int i = borderEdgesList.Count() - 1; i >= 0; --i )
		{
			CUtlVector< CEdge * > &srcBorderEdges = borderEdgesList[ i ];
			for ( int j = srcBorderEdges.Count() - 1; j >= 0; --j )
			{
				CEdge *pSrcEdge = srcBorderEdges[ j ];
				connected = false;

				for ( int k = 0; !connected && k < i; ++k )
				{
					CUtlVector< CEdge * > &dstBorderEdges = borderEdgesList[ k ];
					for ( int l = dstBorderEdges.Count() - 1; l >= 0; --l )
					{
						if ( dstBorderEdges[ l ]->ConnectedTo( pSrcEdge ) )
						{
							connected = true;
							anyConnected = true;
							dstBorderEdges.AddToTail( pSrcEdge );
							srcBorderEdges.Remove( j );
							break;
						}
					}
				}
			}

			if ( srcBorderEdges.Count() == 0 )
			{
				borderEdgesList.Remove( i );
			}
		}
	} while( anyConnected );

	return retVal;
}
