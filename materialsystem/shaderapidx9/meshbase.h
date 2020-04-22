//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef MESHBASE_H
#define MESHBASE_H

#ifdef _WIN32
#pragma once
#endif


#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"


//-----------------------------------------------------------------------------
// Base vertex buffer
//-----------------------------------------------------------------------------
abstract_class CVertexBufferBase : public IVertexBuffer
{
	// Methods of IVertexBuffer
public:
	virtual void Spew( int nVertexCount, const VertexDesc_t &desc );
	virtual void ValidateData( int nVertexCount, const VertexDesc_t& desc );

public:
	// constructor, destructor
	CVertexBufferBase( const char *pBudgetGroupName );
	virtual ~CVertexBufferBase();

	// Displays the vertex format
	static void PrintVertexFormat( VertexFormat_t vertexFormat );

	// Used to construct vertex data
	static void ComputeVertexDescription( unsigned char *pBuffer, VertexFormat_t vertexFormat, VertexDesc_t &desc );

	// Returns the vertex format size 
	static int VertexFormatSize( VertexFormat_t vertexFormat );

protected:
	const char *m_pBudgetGroupName;
};


//-----------------------------------------------------------------------------
// Base index buffer
//-----------------------------------------------------------------------------
abstract_class CIndexBufferBase : public IIndexBuffer
{
	// Methods of IIndexBuffer
public:
	virtual void Spew( int nIndexCount, const IndexDesc_t &desc );
	virtual void ValidateData( int nIndexCount, const IndexDesc_t& desc );

	// Other public methods
public:
	// constructor, destructor
	CIndexBufferBase( const char *pBudgetGroupName );
	virtual ~CIndexBufferBase() {}

protected:
	const char *m_pBudgetGroupName;
};


//-----------------------------------------------------------------------------
// Base mesh
//-----------------------------------------------------------------------------
class CMeshBase : public IMesh
{
	// Methods of IMesh
public:

	// Other public methods that need to be overridden
public:
	// Begins a pass
	virtual void BeginPass( ) = 0;

	// Draws a single pass of the mesh
	virtual void RenderPass() = 0;

	// Does it have a color mesh?
	virtual bool HasColorMesh() const = 0;

	// Am I using morph data?
	virtual bool IsUsingMorphData() const = 0;

	virtual bool HasFlexMesh() const = 0;

	virtual IMesh *GetMesh() { return this; }

public:
	// constructor, destructor
	CMeshBase();
	virtual ~CMeshBase();

};

//-----------------------------------------------------------------------------
// Utility method for VertexDesc_t (don't want to expose it in public, in imesh.h)
//-----------------------------------------------------------------------------
inline void ComputeVertexDesc( unsigned char * pBuffer, VertexFormat_t vertexFormat, VertexDesc_t & desc )
{
	int i;
	int *pVertexSizesToSet[64];
	int nVertexSizesToSet = 0;
	static ALIGN32 ModelVertexDX8_t temp[4];
	float *dummyData = (float*)&temp; // should be larger than any CMeshBuilder command can set.

	// Determine which vertex compression type this format specifies (affects element sizes/decls):
	VertexCompressionType_t compression = CompressionType( vertexFormat );
	desc.m_CompressionType = compression;

	// We use fvf instead of flags here because we may pad out the fvf
	// vertex structure to optimize performance
	int offset = 0;
	// NOTE: At the moment, we assume that if you specify wrinkle, you also specify position
	Assert( ( ( vertexFormat & VERTEX_WRINKLE ) == 0 ) || ( ( vertexFormat & VERTEX_POSITION ) != 0 ) );
	if ( vertexFormat & VERTEX_POSITION )
	{
		// UNDONE: compress position+wrinkle to SHORT4N, and roll the scale into the transform matrices
		desc.m_pPosition = reinterpret_cast<float*>(pBuffer);
		offset += GetVertexElementSize( VERTEX_ELEMENT_POSITION, compression );
		pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_Position;

		if ( vertexFormat & VERTEX_WRINKLE )
		{
			desc.m_pWrinkle = reinterpret_cast<float*>( pBuffer + offset );
			offset += GetVertexElementSize( VERTEX_ELEMENT_WRINKLE, compression );
			pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_Wrinkle;
		}
		else
		{
			desc.m_pWrinkle = dummyData;
			desc.m_VertexSize_Wrinkle = 0;
		}
	}
	else
	{
		desc.m_pPosition = dummyData;
		desc.m_VertexSize_Position = 0;
		desc.m_pWrinkle = dummyData;
		desc.m_VertexSize_Wrinkle = 0;
	}

	// Bone weights/matrix indices
	desc.m_NumBoneWeights = NumBoneWeights( vertexFormat );

	Assert( ( desc.m_NumBoneWeights == 2 ) || ( desc.m_NumBoneWeights == 0 ) );

	// We assume that if you have any indices/weights, you have exactly two of them
	Assert( ( ( desc.m_NumBoneWeights == 2 ) && ( ( vertexFormat & VERTEX_BONE_INDEX ) != 0 ) ) ||
			( ( desc.m_NumBoneWeights == 0 ) && ( ( vertexFormat & VERTEX_BONE_INDEX ) == 0 ) ) );

	if ( ( vertexFormat & VERTEX_BONE_INDEX ) != 0 )
	{
		if ( desc.m_NumBoneWeights > 0 )
		{
			Assert( desc.m_NumBoneWeights == 2 );

			// Always exactly two weights
			desc.m_pBoneWeight = reinterpret_cast<float*>(pBuffer + offset);
			offset += GetVertexElementSize( VERTEX_ELEMENT_BONEWEIGHTS2, compression );
			pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_BoneWeight;
		}
		else
		{
			desc.m_pBoneWeight = dummyData;
			desc.m_VertexSize_BoneWeight = 0;
		}

		desc.m_pBoneMatrixIndex = pBuffer + offset;
		offset += GetVertexElementSize( VERTEX_ELEMENT_BONEINDEX, compression );
		pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_BoneMatrixIndex;
	}
	else
	{
		desc.m_pBoneWeight = dummyData;
		desc.m_VertexSize_BoneWeight = 0;

		desc.m_pBoneMatrixIndex = (unsigned char*)dummyData;
		desc.m_VertexSize_BoneMatrixIndex = 0;
	}

	if ( vertexFormat & VERTEX_NORMAL )
	{
		desc.m_pNormal = reinterpret_cast<float*>(pBuffer + offset);
		// See PackNormal_[SHORT2|UBYTE4|HEND3N] in mathlib.h for the compression algorithm
		offset += GetVertexElementSize( VERTEX_ELEMENT_NORMAL, compression );
		pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_Normal;
	}
	else
	{
		desc.m_pNormal = dummyData;
		desc.m_VertexSize_Normal = 0;
	}

	if ( vertexFormat & VERTEX_COLOR )
	{
		desc.m_pColor = pBuffer + offset;
		offset += GetVertexElementSize( VERTEX_ELEMENT_COLOR, compression );
		pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_Color;
	}
	else
	{
		desc.m_pColor = (unsigned char*)dummyData;
		desc.m_VertexSize_Color = 0;
	}

	if ( vertexFormat & VERTEX_SPECULAR )
	{
		desc.m_pSpecular = pBuffer + offset;
		offset += GetVertexElementSize( VERTEX_ELEMENT_SPECULAR, compression );
		pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_Specular;
	}
	else
	{
		desc.m_pSpecular = (unsigned char*)dummyData;
		desc.m_VertexSize_Specular = 0;
	}

	// Set up texture coordinates
	for ( i = 0; i < VERTEX_MAX_TEXTURE_COORDINATES; ++i )
	{
		// FIXME: compress texcoords to SHORT2N/SHORT4N, with a scale rolled into the texture transform
		VertexElement_t texCoordElements[4] = { VERTEX_ELEMENT_TEXCOORD1D_0, VERTEX_ELEMENT_TEXCOORD2D_0, VERTEX_ELEMENT_TEXCOORD3D_0, VERTEX_ELEMENT_TEXCOORD4D_0 };
		int nSize = TexCoordSize( i, vertexFormat );
		if ( nSize != 0 )
		{
			desc.m_pTexCoord[i] = reinterpret_cast<float*>(pBuffer + offset);
			VertexElement_t texCoordElement = (VertexElement_t)( texCoordElements[ nSize - 1 ] + i );
			offset += GetVertexElementSize( texCoordElement, compression );
			pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_TexCoord[i];
		}
		else
		{
			desc.m_pTexCoord[i] = dummyData;
			desc.m_VertexSize_TexCoord[i] = 0;
		}
	}

	// Binormal + tangent...
	// Note we have to put these at the end so the vertex is FVF + stuff at end
	if ( vertexFormat & VERTEX_TANGENT_S )
	{
		// UNDONE: use normal compression here (use mem_dumpvballocs to see if this uses much memory)
		desc.m_pTangentS = reinterpret_cast<float*>(pBuffer + offset);
		offset += GetVertexElementSize( VERTEX_ELEMENT_TANGENT_S, compression );
		pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_TangentS;
	}
	else
	{
		desc.m_pTangentS = dummyData;
		desc.m_VertexSize_TangentS = 0;
	}

	if ( vertexFormat & VERTEX_TANGENT_T )
	{
		// UNDONE: use normal compression here (use mem_dumpvballocs to see if this uses much memory)
		desc.m_pTangentT = reinterpret_cast<float*>(pBuffer + offset);
		offset += GetVertexElementSize( VERTEX_ELEMENT_TANGENT_T, compression );
		pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_TangentT;
	}
	else
	{
		desc.m_pTangentT = dummyData;
		desc.m_VertexSize_TangentT = 0;
	}

	// User data..
	int userDataSize = UserDataSize( vertexFormat );
	if ( userDataSize > 0 )
	{
		desc.m_pUserData = reinterpret_cast<float*>(pBuffer + offset);
		VertexElement_t userDataElement = (VertexElement_t)( VERTEX_ELEMENT_USERDATA1 + ( userDataSize - 1 ) );
		// See PackNormal_[SHORT2|UBYTE4|HEND3N] in mathlib.h for the compression algorithm
		offset += GetVertexElementSize( userDataElement, compression );

		pVertexSizesToSet[nVertexSizesToSet++] = &desc.m_VertexSize_UserData;
	}
	else
	{
		desc.m_pUserData = dummyData;
		desc.m_VertexSize_UserData = 0;
	}

	// We always use vertex sizes which are half-cache aligned (16 bytes)
	// x360 compressed vertexes are not compatible with forced alignments
	bool bCacheAlign = ( vertexFormat & VERTEX_FORMAT_USE_EXACT_FORMAT ) == 0;
	if ( bCacheAlign && ( offset > 16 ) && IsPC() )
	{
		offset = (offset + 0xF) & (~0xF);
	}
	desc.m_ActualVertexSize = offset;

	// Now set the m_VertexSize for all the members that were actually valid.
	Assert( nVertexSizesToSet < sizeof(pVertexSizesToSet)/sizeof(pVertexSizesToSet[0]) );
	for ( int iElement=0; iElement < nVertexSizesToSet; iElement++ )
	{
		*pVertexSizesToSet[iElement] = offset;
	}
}

#endif // MESHBASE_H
