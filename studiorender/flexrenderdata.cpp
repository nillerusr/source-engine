//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "flexrenderdata.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

CCachedRenderData::CCachedRenderData() : m_CurrentTag(0), m_pFirstFlexIndex(0),
	m_pFirstWorldIndex(0)
{
#ifdef _DEBUG
	int i;
	float val = VEC_T_NAN;
	for( i = 0; i < MAXSTUDIOFLEXVERTS; i++ )
	{
		m_pFlexVerts[i].m_Position[0] = val; 
		m_pFlexVerts[i].m_Position[1] = val;
		m_pFlexVerts[i].m_Position[2] = val;
		m_pFlexVerts[i].m_Normal[0] = val; 
		m_pFlexVerts[i].m_Normal[1] = val;
		m_pFlexVerts[i].m_Normal[2] = val;

		m_pThinFlexVerts[i].m_Position[0] = val; 
		m_pThinFlexVerts[i].m_Position[1] = val;
		m_pThinFlexVerts[i].m_Position[2] = val;
		m_pThinFlexVerts[i].m_Normal[0] = val; 
		m_pThinFlexVerts[i].m_Normal[1] = val;
		m_pThinFlexVerts[i].m_Normal[2] = val;

		m_pFlexVerts[i].m_TangentS[0] = val; 
		m_pFlexVerts[i].m_TangentS[1] = val;
		m_pFlexVerts[i].m_TangentS[2] = val;
		m_pFlexVerts[i].m_TangentS[3] = val;
	}
#endif
}

//-----------------------------------------------------------------------------
// Call this before rendering the model
//-----------------------------------------------------------------------------

void CCachedRenderData::StartModel()
{
	++m_CurrentTag;
	m_IndexCount = 0;
	m_FlexVertexCount = 0;
	m_ThinFlexVertexCount = 0;
	m_WorldVertexCount = 0;
	m_pFirstFlexIndex = 0;
	m_pFirstThinFlexIndex = 0;
	m_pFirstWorldIndex = 0;
}

//-----------------------------------------------------------------------------
// Used to hook ourselves into a particular body part, model, and mesh
//-----------------------------------------------------------------------------

void CCachedRenderData::SetBodyPart( int bodypart )
{
	m_Body = bodypart;
	m_CacheDict.EnsureCount(m_Body+1);
	m_Model = m_Mesh = -1;
	m_pFirstFlexIndex = 0;
	m_pFirstThinFlexIndex = 0;
	m_pFirstWorldIndex = 0;
}

void CCachedRenderData::SetModel( int model )
{
	Assert(m_Body >= 0);
	m_Model = model;
	m_CacheDict[m_Body].EnsureCount(m_Model+1);
	m_Mesh = -1;
	m_pFirstFlexIndex = 0;
	m_pFirstThinFlexIndex = 0;
	m_pFirstWorldIndex = 0;
}

void CCachedRenderData::SetMesh( int mesh )
{
	Assert((m_Model >= 0) && (m_Body >= 0));

	m_Mesh = mesh;
	m_CacheDict[m_Body][m_Model].EnsureCount(m_Mesh+1);

	// At this point, we should have all 3 defined.
	CacheDict_t& dict = m_CacheDict[m_Body][m_Model][m_Mesh];

	if (dict.m_Tag == m_CurrentTag)
	{
		m_pFirstFlexIndex = &m_pFlexIndex[dict.m_FirstIndex];
		m_pFirstThinFlexIndex = &m_pThinFlexIndex[dict.m_FirstIndex];
		m_pFirstWorldIndex = &m_pWorldIndex[dict.m_FirstIndex];
	}
	else
	{
		m_pFirstFlexIndex = 0;
		m_pFirstThinFlexIndex = 0;
		m_pFirstWorldIndex = 0;
	}
}


//-----------------------------------------------------------------------------
// Used to set up a flex computation
//-----------------------------------------------------------------------------

bool CCachedRenderData::IsFlexComputationDone( ) const
{
	Assert((m_Model >= 0) && (m_Body >= 0) && (m_Mesh >= 0));

	// Lets create the dictionary entry
	// If the tags match, that means we're doing the computation twice!!!
	CacheDict_t const& dict = m_CacheDict[m_Body][m_Model][m_Mesh];
	return (dict.m_FlexTag == m_CurrentTag);
}

//-----------------------------------------------------------------------------
// Used to set up a computation	(modifies vertex data)
//-----------------------------------------------------------------------------

void CCachedRenderData::SetupComputation( mstudiomesh_t *pMesh, bool flexComputation )
{
	Assert((m_Model >= 0) && (m_Body >= 0) && (m_Mesh >= 0));
//	Assert( !m_pFirstIndex );

	// Lets create the dictionary entry
	// If the tags match, that means we're doing the computation twice!!!
	CacheDict_t& dict = m_CacheDict[m_Body][m_Model][m_Mesh];
	if (dict.m_Tag != m_CurrentTag)
	{
		dict.m_FirstIndex = m_IndexCount;
		dict.m_IndexCount = pMesh->numvertices;
		dict.m_Tag = m_CurrentTag;
		m_IndexCount += dict.m_IndexCount;
	}

	if (flexComputation)
		dict.m_FlexTag = m_CurrentTag;

	m_pFirstFlexIndex = &m_pFlexIndex[dict.m_FirstIndex];
	m_pFirstThinFlexIndex = &m_pThinFlexIndex[dict.m_FirstIndex];
	m_pFirstWorldIndex = &m_pWorldIndex[dict.m_FirstIndex];
}

//-----------------------------------------------------------------------------
// Creates a new flexed vertex to be associated with a vertex
//-----------------------------------------------------------------------------

CachedPosNormTan_t* CCachedRenderData::CreateFlexVertex( int vertex )
{
	Assert( m_pFirstFlexIndex );
	Assert( m_pFirstFlexIndex[vertex].m_Tag != m_CurrentTag );

	Assert ( m_FlexVertexCount < MAXSTUDIOFLEXVERTS );
	if ( m_FlexVertexCount >= MAXSTUDIOFLEXVERTS )
		return NULL;

	// Point the flex list to the new flexed vertex
	m_pFirstFlexIndex[vertex].m_Tag = m_CurrentTag;
	m_pFirstFlexIndex[vertex].m_VertexIndex = m_FlexVertexCount;

	// Add a new flexed vert to the flexed vertex list
	++m_FlexVertexCount;

	return GetFlexVertex( vertex );
}

//-----------------------------------------------------------------------------
// Creates a new flexed vertex to be associated with a vertex
//-----------------------------------------------------------------------------

CachedPosNorm_t* CCachedRenderData::CreateThinFlexVertex( int vertex )
{
	Assert( m_pFirstThinFlexIndex );
	Assert( m_pFirstThinFlexIndex[vertex].m_Tag != m_CurrentTag );

	Assert ( m_ThinFlexVertexCount < MAXSTUDIOFLEXVERTS );
	if ( m_ThinFlexVertexCount >= MAXSTUDIOFLEXVERTS )
		return NULL;

	// Point the flex list to the new flexed vertex
	m_pFirstThinFlexIndex[vertex].m_Tag = m_CurrentTag;
	m_pFirstThinFlexIndex[vertex].m_VertexIndex = m_ThinFlexVertexCount;

	// Add a new flexed vert to the thin flexed vertex list
	++m_ThinFlexVertexCount;

	return GetThinFlexVertex( vertex );
}

//-----------------------------------------------------------------------------
// Re-normalize the surface normals and tangents of the flexed vertices
// No thin ones since they're intended to be deltas, not unit vectors
//-----------------------------------------------------------------------------
void CCachedRenderData::RenormalizeFlexVertices( bool bHasTangentData )
{
	int i;

	for (i = 0; i < m_FlexVertexCount; i++)
	{
		m_pFlexVerts[ i ].m_Normal.NormalizeInPlace();
		if (bHasTangentData)
		{
			m_pFlexVerts[ i ].m_TangentS.AsVector3D().NormalizeInPlace();
		}
	}
}

//-----------------------------------------------------------------------------
// Creates a new flexed vertex to be associated with a vertex
//-----------------------------------------------------------------------------

CachedPosNorm_t* CCachedRenderData::CreateWorldVertex( int vertex )
{
	Assert( m_pFirstWorldIndex );
	if ( m_pFirstWorldIndex[vertex].m_Tag != m_CurrentTag )
	{
		// Point the world list to the new world vertex
		Assert( m_WorldVertexCount < MAXSTUDIOVERTS );
		m_pFirstWorldIndex[vertex].m_Tag = m_CurrentTag;
		m_pFirstWorldIndex[vertex].m_VertexIndex = m_WorldVertexCount;

		// Add a new world vert to the world vertex list
		++m_WorldVertexCount;
	}
	return GetWorldVertex( vertex );
}

