//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FLEXRENDERDATA_H
#define FLEXRENDERDATA_H
#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector.h"
#include "utlvector.h"
#include "studio.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------

struct mstudiomesh_t;

//-----------------------------------------------------------------------------
// Used by flex vertex data cache
//-----------------------------------------------------------------------------

struct CachedPosNormTan_t
{
	Vector		m_Position;
	Vector		m_Normal;
	Vector4D	m_TangentS;

	CachedPosNormTan_t() {}

	CachedPosNormTan_t( CachedPosNormTan_t const& src )
	{
		VectorCopy( src.m_Position, m_Position );
		VectorCopy( src.m_Normal, m_Normal );
		Vector4DCopy( src.m_TangentS, m_TangentS );
		Assert( m_TangentS.w == 1.0f || m_TangentS.w == -1.0f );
	}
};

//-----------------------------------------------------------------------------
// Used by world (decal) vertex data cache
//-----------------------------------------------------------------------------

struct CachedPosNorm_t
{
	Vector4DAligned	m_Position;
	Vector4DAligned	m_Normal;

	CachedPosNorm_t() {}

	CachedPosNorm_t( CachedPosNorm_t const& src )
	{
		Vector4DCopy( src.m_Position, m_Position );
		Vector4DCopy( src.m_Normal, m_Normal );
	}
};


//-----------------------------------------------------------------------------
// Stores flex vertex data and world (decal) vertex data for the lifetime of the model rendering
//-----------------------------------------------------------------------------


class CCachedRenderData
{
public:
	// Constructor
	CCachedRenderData();

	// Call this when we start to render a new model
	void StartModel();

	// Used to hook ourselves into a particular body part, model, and mesh
	void SetBodyPart( int bodypart );
	void SetModel( int model );
	void SetMesh( int mesh );

	// For faster setup in the decal code
	void SetBodyModelMesh( int body, int model, int mesh );

	// Used to set up a flex computation
	bool IsFlexComputationDone( ) const;

	// Used to set up a computation (for world or flex data)
	void SetupComputation( mstudiomesh_t *pMesh, bool flexComputation = false );

	// Is a particular vertex flexed?
	bool IsVertexFlexed( int vertex ) const;
	bool IsThinVertexFlexed( int vertex ) const;

	// Checks to see if the vertex is defined
	bool IsVertexPositionCached( int vertex ) const;

	// Gets a flexed vertex
	CachedPosNormTan_t* GetFlexVertex( int vertex );

	// Gets a flexed vertex
	CachedPosNorm_t* GetThinFlexVertex( int vertex );

	// Creates a new flexed vertex to be associated with a vertex
	CachedPosNormTan_t* CreateFlexVertex( int vertex );

	// Creates a new flexed vertex to be associated with a vertex
	CachedPosNorm_t* CreateThinFlexVertex( int vertex );

	// Renormalizes the normals and tangents of the flex verts
	void RenormalizeFlexVertices( bool bHasTangentData );

	// Gets a decal vertex
	CachedPosNorm_t* GetWorldVertex( int vertex );

	// Creates a new decal vertex to be associated with a vertex
	CachedPosNorm_t* CreateWorldVertex( int vertex );

	template< class T >
	void ComputeFlexedVertex_StreamOffset( studiohdr_t *pStudioHdr, mstudioflex_t *pflex, T *pvanim, int vertCount, float w1, float w2, float w3, float w4 );

#ifdef PLATFORM_WINDOWS
	void ComputeFlexedVertex_StreamOffset_Optimized( studiohdr_t *pStudioHdr, mstudioflex_t *pflex, mstudiovertanim_t *pvanim, int vertCount, float w1, float w2, float w3, float w4);
	void ComputeFlexedVertexWrinkle_StreamOffset_Optimized( studiohdr_t *pStudioHdr, mstudioflex_t *pflex, mstudiovertanim_wrinkle_t *pvanim, int vertCount, float w1, float w2, float w3, float w4);
#endif // PLATFORM_WINDOWS

private:
	// Used to create the flex render data. maps 
	struct CacheIndex_t
	{
		unsigned short	m_Tag;
		unsigned short	m_VertexIndex;
	};

	// A dictionary for the cached data
	struct CacheDict_t
	{
		unsigned short	m_FirstIndex;
		unsigned short	m_IndexCount;
		unsigned short	m_Tag;
		unsigned short	m_FlexTag;

		CacheDict_t() : m_Tag(0), m_FlexTag(0) {}
	};

	typedef CUtlVector< CacheDict_t >		CacheMeshDict_t;
	typedef CUtlVector< CacheMeshDict_t >	CacheModelDict_t;
	typedef CUtlVector<	CacheModelDict_t >	CacheBodyPartDict_t;

	// Flex data, allocated for the lifespan of rendering
	// Can't use UtlVector due to alignment issues
	int					m_FlexVertexCount;
	CachedPosNormTan_t	m_pFlexVerts[MAXSTUDIOFLEXVERTS+1];

	// Flex data, allocated for the lifespan of rendering
	// Can't use UtlVector due to alignment issues
	int				m_ThinFlexVertexCount;
	CachedPosNorm_t	m_pThinFlexVerts[MAXSTUDIOFLEXVERTS+1];

	// World data, allocated for the lifespan of rendering
	// Can't use UtlVector due to alignment issues
	int				m_WorldVertexCount;
	CachedPosNorm_t	m_pWorldVerts[MAXSTUDIOVERTS+1];

	// Maps actual mesh vertices into flex cache + world cache indices
	int				m_IndexCount;
	CacheIndex_t	m_pFlexIndex[MAXSTUDIOVERTS+1];
	CacheIndex_t	m_pThinFlexIndex[MAXSTUDIOVERTS+1];
	CacheIndex_t	m_pWorldIndex[MAXSTUDIOVERTS+1];

	CacheBodyPartDict_t m_CacheDict;

	// The flex tag
	unsigned short m_CurrentTag;

	// the current body, model, and mesh
	int m_Body;
	int m_Model;
	int m_Mesh;

	// mapping for the current mesh to flex data
	CacheIndex_t*	m_pFirstFlexIndex;
	CacheIndex_t*	m_pFirstThinFlexIndex;
	CacheIndex_t*	m_pFirstWorldIndex;

	friend class CStudioRender;
};


//-----------------------------------------------------------------------------
// Checks to see if the vertex is defined
//-----------------------------------------------------------------------------

inline bool CCachedRenderData::IsVertexFlexed( int vertex ) const
{
	return (m_pFirstFlexIndex && (m_pFirstFlexIndex[vertex].m_Tag == m_CurrentTag));
}

inline bool CCachedRenderData::IsThinVertexFlexed( int vertex ) const
{
	return (m_pFirstThinFlexIndex && (m_pFirstThinFlexIndex[vertex].m_Tag == m_CurrentTag));
}

//-----------------------------------------------------------------------------
// Gets an existing flexed vertex associated with a vertex
//-----------------------------------------------------------------------------

inline CachedPosNormTan_t* CCachedRenderData::GetFlexVertex( int vertex )
{
	Assert( m_pFirstFlexIndex );
	Assert( m_pFirstFlexIndex[vertex].m_Tag == m_CurrentTag );
	return &m_pFlexVerts[ m_pFirstFlexIndex[vertex].m_VertexIndex ];
}

inline CachedPosNorm_t* CCachedRenderData::GetThinFlexVertex( int vertex )
{
	Assert( m_pFirstThinFlexIndex );
	Assert( m_pFirstThinFlexIndex[vertex].m_Tag == m_CurrentTag );
	return &m_pThinFlexVerts[ m_pFirstThinFlexIndex[vertex].m_VertexIndex ];
}




//-----------------------------------------------------------------------------
// Checks to see if the vertex is defined
//-----------------------------------------------------------------------------

inline bool CCachedRenderData::IsVertexPositionCached( int vertex ) const
{
	return (m_pFirstWorldIndex && (m_pFirstWorldIndex[vertex].m_Tag == m_CurrentTag));
}

//-----------------------------------------------------------------------------
// Gets an existing world vertex associated with a vertex
//-----------------------------------------------------------------------------

inline CachedPosNorm_t* CCachedRenderData::GetWorldVertex( int vertex )
{
	Assert( m_pFirstWorldIndex );
	Assert( m_pFirstWorldIndex[vertex].m_Tag == m_CurrentTag );
	return &m_pWorldVerts[ m_pFirstWorldIndex[vertex].m_VertexIndex ];
}

//-----------------------------------------------------------------------------
// For faster setup in the decal code
//-----------------------------------------------------------------------------

inline void CCachedRenderData::SetBodyModelMesh( int body, int model, int mesh)
{
	m_Body = body;
	m_Model = model;
	m_Mesh = mesh;

	Assert((m_Model >= 0) && (m_Body >= 0));
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
// Purpose: 
//
//  ** Only execute this function if device supports stream offset **
//
// Input  : pmesh - pointer to a studio mesh
//          lod - integer lod (0 is most detailed)
// Output : none
//-----------------------------------------------------------------------------
template< class T > 
void CCachedRenderData::ComputeFlexedVertex_StreamOffset( studiohdr_t *pStudioHdr, mstudioflex_t *pflex, 
	T *pvanim, int vertCount, float w1, float w2, float w3, float w4 )
{
	float w12 = w1 - w2;
	float w34 = w3 - w4;
	float flVertAnimFixedPointScale = pStudioHdr->VertAnimFixedPointScale();

	CachedPosNorm_t *pFlexedVertex = NULL;
	for (int j = 0; j < pflex->numverts; j++)
	{
		int n = pvanim[j].index;

		// only flex the indices that are (still) part of this mesh at this lod
		if ( n >= vertCount )
			continue;

		float s = pvanim[j].speed;
		float b = pvanim[j].side;

		Vector4DAligned vPosition, vNormal;
		pvanim[j].GetDeltaFixed4DAligned( &vPosition, flVertAnimFixedPointScale );
		pvanim[j].GetNDeltaFixed4DAligned( &vNormal, flVertAnimFixedPointScale );

		if ( !IsThinVertexFlexed(n) )
		{
			// Add a new flexed vert to the flexed vertex list
			pFlexedVertex = CreateThinFlexVertex(n);

			Assert( pFlexedVertex != NULL);

			pFlexedVertex->m_Position.InitZero();
			pFlexedVertex->m_Normal.InitZero();
		}
		else
		{
			pFlexedVertex = GetThinFlexVertex(n);
		}

		s *= 1.0f / 255.0f;
		b *= 1.0f / 255.0f;

		float wa = w2 + w12 * s;
		float wb = w4 + w34 * s;
		float w = wa + ( wb - wa ) * b;
		Vector4DWeightMAD( w, vPosition, pFlexedVertex->m_Position, vNormal, pFlexedVertex->m_Normal );
	}
}							 

#endif // FLEXRENDERDATA_H
