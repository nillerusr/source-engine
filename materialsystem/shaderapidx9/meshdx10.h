//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//


#ifndef MESHDX10_H
#define MESHDX10_H

#ifdef _WIN32
#pragma once
#endif

#include "meshbase.h"
#include "shaderapi/ishaderdevice.h"


//-----------------------------------------------------------------------------
// Forward declaration
//-----------------------------------------------------------------------------
struct ID3D10Buffer;


//-----------------------------------------------------------------------------
// Dx10 implementation of a vertex buffer
//-----------------------------------------------------------------------------
class CVertexBufferDx10 : public CVertexBufferBase
{
	typedef CVertexBufferBase BaseClass;

	// Methods of IVertexBuffer
public:
	virtual int VertexCount() const;
	virtual VertexFormat_t GetVertexFormat() const;
	virtual bool Lock( int nMaxVertexCount, bool bAppend, VertexDesc_t &desc );
	virtual void Unlock( int nWrittenVertexCount, VertexDesc_t &desc );
	virtual bool IsDynamic() const;
	virtual void BeginCastBuffer( VertexFormat_t format );
	virtual	void EndCastBuffer( );
	virtual int GetRoomRemaining() const;

	// Other public methods
public:
	// constructor, destructor
	CVertexBufferDx10( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroupName );
	virtual ~CVertexBufferDx10();

	ID3D10Buffer* GetDx10Buffer() const;
	int VertexSize() const;

	// Only used by dynamic buffers, indicates the next lock should perform a discard.
	void Flush();

protected:
	// Creates, destroys the index buffer
	bool Allocate( );
	void Free();

	ID3D10Buffer *m_pVertexBuffer;
	VertexFormat_t m_VertexFormat;
	int m_nVertexCount;
	int m_nBufferSize;
	int m_nFirstUnwrittenOffset;
	bool m_bIsLocked : 1;
	bool m_bIsDynamic : 1;
	bool m_bFlush : 1;				// Used only for dynamic buffers, indicates to discard the next time

#ifdef _DEBUG
	static int s_nBufferCount;
#endif
};


//-----------------------------------------------------------------------------
// inline methods for CVertexBufferDx10
//-----------------------------------------------------------------------------
inline ID3D10Buffer* CVertexBufferDx10::GetDx10Buffer() const
{
	return m_pVertexBuffer;
}

inline int CVertexBufferDx10::VertexSize() const
{
	return VertexFormatSize( m_VertexFormat );
}


//-----------------------------------------------------------------------------
// Dx10 implementation of an index buffer
//-----------------------------------------------------------------------------
class CIndexBufferDx10 : public CIndexBufferBase
{
	typedef CIndexBufferBase BaseClass;

	// Methods of IIndexBuffer
public:
	virtual int IndexCount() const;
	virtual MaterialIndexFormat_t IndexFormat() const;
	virtual int GetRoomRemaining() const;
	virtual bool Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t &desc );
	virtual void Unlock( int nWrittenIndexCount, IndexDesc_t &desc );
	virtual void ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc );
	virtual void ModifyEnd( IndexDesc_t& desc );
	virtual bool IsDynamic() const;
	virtual void BeginCastBuffer( MaterialIndexFormat_t format );
	virtual void EndCastBuffer( );

	// Other public methods
public:
	// constructor, destructor
	CIndexBufferDx10( ShaderBufferType_t type, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroupName );
	virtual ~CIndexBufferDx10();

	ID3D10Buffer* GetDx10Buffer() const;
	MaterialIndexFormat_t GetIndexFormat() const;

	// Only used by dynamic buffers, indicates the next lock should perform a discard.
	void Flush();

protected:
	// Creates, destroys the index buffer
	bool Allocate( );
	void Free();

	// Returns the size of the index in bytes
	int IndexSize() const;

	ID3D10Buffer *m_pIndexBuffer;
	MaterialIndexFormat_t m_IndexFormat;
	int m_nIndexCount;
	int m_nBufferSize;
	int m_nFirstUnwrittenOffset;	// Used only for dynamic buffers, indicates where it's safe to write (nooverwrite)
	bool m_bIsLocked : 1;
	bool m_bIsDynamic : 1;
	bool m_bFlush : 1;				// Used only for dynamic buffers, indicates to discard the next time

#ifdef _DEBUG
	static int s_nBufferCount;
#endif
};


//-----------------------------------------------------------------------------
// Returns the size of the index in bytes
//-----------------------------------------------------------------------------
inline int CIndexBufferDx10::IndexSize() const
{
	switch( m_IndexFormat )
	{
	default:
	case MATERIAL_INDEX_FORMAT_UNKNOWN:
		return 0;
	case MATERIAL_INDEX_FORMAT_16BIT:
		return 2;
	case MATERIAL_INDEX_FORMAT_32BIT:
		return 4;
	}
}

inline ID3D10Buffer* CIndexBufferDx10::GetDx10Buffer() const
{
	return m_pIndexBuffer;
}

inline MaterialIndexFormat_t CIndexBufferDx10::GetIndexFormat() const
{
	return m_IndexFormat;
}


//-----------------------------------------------------------------------------
// Dx10 implementation of a mesh
//-----------------------------------------------------------------------------
class CMeshDx10 : public CMeshBase
{
public:
	CMeshDx10();
	virtual ~CMeshDx10();

	// FIXME: Make this work! Unsupported methods of IIndexBuffer
	virtual bool Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t& desc ) { Assert(0); return false; }
	virtual void Unlock( int nWrittenIndexCount, IndexDesc_t& desc ) { Assert(0); }
	virtual int GetRoomRemaining() const { Assert(0); return 0; }
	virtual void ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc ) { Assert(0); }
	virtual void ModifyEnd( IndexDesc_t& desc ) { Assert(0); }
	virtual void Spew( int nIndexCount, const IndexDesc_t & desc ) { Assert(0); }
	virtual void ValidateData( int nIndexCount, const IndexDesc_t &desc ) { Assert(0); }
	virtual bool Lock( int nVertexCount, bool bAppend, VertexDesc_t &desc ) { Assert(0); return false; }
	virtual void Unlock( int nVertexCount, VertexDesc_t &desc ) { Assert(0); }
	virtual void Spew( int nVertexCount, const VertexDesc_t &desc ) { Assert(0); }
	virtual void ValidateData( int nVertexCount, const VertexDesc_t & desc ) { Assert(0); }
	virtual bool IsDynamic() const { Assert(0); return false; }
	virtual void BeginCastBuffer( MaterialIndexFormat_t format ) { Assert(0); }
	virtual void BeginCastBuffer( VertexFormat_t format ) { Assert(0); }
	virtual void EndCastBuffer( ) { Assert(0); }

	void LockMesh( int numVerts, int numIndices, MeshDesc_t& desc );
	void UnlockMesh( int numVerts, int numIndices, MeshDesc_t& desc );

	void ModifyBeginEx( bool bReadOnly, int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc );
	void ModifyBegin( int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc );
	void ModifyEnd( MeshDesc_t& desc );

	// returns the # of vertices (static meshes only)
	int  VertexCount() const;

	virtual void BeginPass( ) {}
	virtual void RenderPass() {}
	virtual bool HasColorMesh() const { return false; }
	virtual bool IsUsingMorphData() const { return false; }
	virtual bool HasFlexMesh() const { return false; }

	// Sets the primitive type
	void SetPrimitiveType( MaterialPrimitiveType_t type );

	// Draws the entire mesh
	void Draw(int firstIndex, int numIndices);

	void Draw(CPrimList *pPrims, int nPrims);

	// Copy verts and/or indices to a mesh builder. This only works for temp meshes!
	virtual void CopyToMeshBuilder( 
		int iStartVert,		// Which vertices to copy.
		int nVerts, 
		int iStartIndex,	// Which indices to copy.
		int nIndices, 
		int indexOffset,	// This is added to each index.
		CMeshBuilder &builder );

	// Spews the mesh data
	void Spew( int numVerts, int numIndices, const MeshDesc_t & desc );

	void ValidateData( int numVerts, int numIndices, const MeshDesc_t & desc );

	// gets the associated material
	IMaterial* GetMaterial();

	void SetColorMesh( IMesh *pColorMesh, int nVertexOffset )
	{
	}


	virtual int IndexCount() const
	{
		return 0;
	}

	virtual MaterialIndexFormat_t IndexFormat() const
	{
		Assert( 0 );
		return MATERIAL_INDEX_FORMAT_UNKNOWN;
	}

	virtual void SetFlexMesh( IMesh *pMesh, int nVertexOffset ) {}

	virtual void DisableFlexMesh() {}

	virtual void MarkAsDrawn() {}

	virtual unsigned ComputeMemoryUsed() { return 0; }

	virtual VertexFormat_t GetVertexFormat() const { return VERTEX_POSITION; }

	virtual IMesh *GetMesh()
	{
		return this;
	}

private:
	enum
	{
		VERTEX_BUFFER_SIZE = 1024 * 1024
	};

	unsigned char* m_pVertexMemory;
};

#endif // MESHDX10_H

