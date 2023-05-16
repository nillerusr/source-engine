#include "meshgl.h"

//-----------------------------------------------------------------------------
//
// The empty mesh...
//
//-----------------------------------------------------------------------------
CGLMesh::CGLMesh( bool bIsDynamic ) : m_bIsDynamic( bIsDynamic )
{
	m_pVertexMemory = new unsigned char[VERTEX_BUFFER_SIZE];
}

CGLMesh::~CGLMesh()
{
	delete[] m_pVertexMemory;
}

bool CGLMesh::Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t& desc )
{
	static int s_BogusIndex;
	desc.m_pIndices = (unsigned short*)&s_BogusIndex;
	desc.m_nIndexSize = 0;
	desc.m_nFirstIndex = 0;
	desc.m_nOffset = 0;
	return true;
}

void CGLMesh::Unlock( int nWrittenIndexCount, IndexDesc_t& desc )
{
}

void CGLMesh::ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc )
{
	Lock( nIndexCount, false, desc );
}

void CGLMesh::ModifyEnd( IndexDesc_t& desc )
{
}

void CGLMesh::Spew( int nIndexCount, const IndexDesc_t & desc )
{
}

void CGLMesh::ValidateData( int nIndexCount, const IndexDesc_t &desc )
{
}

bool CGLMesh::Lock( int nVertexCount, bool bAppend, VertexDesc_t &desc )
{
	// Who cares about the data?
	desc.m_pPosition = (float*)m_pVertexMemory;
	desc.m_pNormal = (float*)m_pVertexMemory;
	desc.m_pColor = m_pVertexMemory;

	int i;
	for ( i = 0; i < VERTEX_MAX_TEXTURE_COORDINATES; ++i)
	{
		desc.m_pTexCoord[i] = (float*)m_pVertexMemory;
	}

	desc.m_pBoneWeight = (float*)m_pVertexMemory;
	desc.m_pBoneMatrixIndex = (unsigned char*)m_pVertexMemory;
	desc.m_pTangentS = (float*)m_pVertexMemory;
	desc.m_pTangentT = (float*)m_pVertexMemory;
	desc.m_pUserData = (float*)m_pVertexMemory;
	desc.m_NumBoneWeights = 2;

	desc.m_VertexSize_Position = 0;
	desc.m_VertexSize_BoneWeight = 0;
	desc.m_VertexSize_BoneMatrixIndex = 0;
	desc.m_VertexSize_Normal = 0;
	desc.m_VertexSize_Color = 0;
	for( i=0; i < VERTEX_MAX_TEXTURE_COORDINATES; i++ )
	{
		desc.m_VertexSize_TexCoord[i] = 0;
	}
	desc.m_VertexSize_TangentS = 0;
	desc.m_VertexSize_TangentT = 0;
	desc.m_VertexSize_UserData = 0;
	desc.m_ActualVertexSize = 0;	// Size of the vertices.. Some of the m_VertexSize_ elements above

	desc.m_nFirstVertex = 0;
	desc.m_nOffset = 0;
	return true;
}

void CGLMesh::Unlock( int nVertexCount, VertexDesc_t &desc )
{
}

void CGLMesh::Spew( int nVertexCount, const VertexDesc_t &desc ) 
{
}

void CGLMesh::ValidateData( int nVertexCount, const VertexDesc_t & desc )
{
}

void CGLMesh::LockMesh( int numVerts, int numIndices, MeshDesc_t& desc )
{
	Lock( numVerts, false, *static_cast<VertexDesc_t*>( &desc ) );
	Lock( numIndices, false, *static_cast<IndexDesc_t*>( &desc ) );
}

void CGLMesh::UnlockMesh( int numVerts, int numIndices, MeshDesc_t& desc )
{
}

void CGLMesh::ModifyBeginEx( bool bReadOnly, int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc )
{
	Lock( numVerts, false, *static_cast<VertexDesc_t*>( &desc ) );
	Lock( numIndices, false, *static_cast<IndexDesc_t*>( &desc ) );
}

void CGLMesh::ModifyBegin( int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc )
{
	ModifyBeginEx( false, firstVertex, numVerts, firstIndex, numIndices, desc );
}

void CGLMesh::ModifyEnd( MeshDesc_t& desc )
{
}

// returns the # of vertices (static meshes only)
int CGLMesh::VertexCount() const
{
	return 0;
}

// Sets the primitive type
void CGLMesh::SetPrimitiveType( MaterialPrimitiveType_t type )
{
}

// Draws the entire mesh
void CGLMesh::Draw( int firstIndex, int numIndices )
{
}

void CGLMesh::Draw(CPrimList *pPrims, int nPrims)
{
}

// Copy verts and/or indices to a mesh builder. This only works for temp meshes!
void CGLMesh::CopyToMeshBuilder( 
	int iStartVert,		// Which vertices to copy.
	int nVerts, 
	int iStartIndex,	// Which indices to copy.
	int nIndices, 
	int indexOffset,	// This is added to each index.
	CMeshBuilder &builder )
{
}

// Spews the mesh data
void CGLMesh::Spew( int numVerts, int numIndices, const MeshDesc_t & desc )
{
}

void CGLMesh::ValidateData( int numVerts, int numIndices, const MeshDesc_t & desc )
{
}

// gets the associated material
IMaterial* CGLMesh::GetMaterial()
{
	// umm. this don't work none
	Assert(0);
	return 0;
}
