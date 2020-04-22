//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef IMESHDX8_H
#define IMESHDX8_H

#ifdef _WIN32
#pragma once
#endif


#include "meshbase.h"
#include "shaderapi/ishaderapi.h"


abstract_class IMeshMgr
{
public:
	// Initialize, shutdown
	virtual void Init() = 0;
	virtual void Shutdown() = 0;

	// Task switch...
	virtual void ReleaseBuffers() = 0;
	virtual void RestoreBuffers() = 0;

	// Releases all dynamic vertex buffers
	virtual void DestroyVertexBuffers() = 0;

	// Flushes the dynamic mesh. Should be called when state changes
	virtual void Flush() = 0;

	// Discards the dynamic vertex and index buffer
	virtual void DiscardVertexBuffers() = 0;

	// Creates, destroys static meshes
	virtual IMesh*	CreateStaticMesh( VertexFormat_t vertexFormat, const char *pTextureBudgetGroup, IMaterial *pMaterial = NULL ) = 0;
	virtual void	DestroyStaticMesh( IMesh* pMesh ) = 0;

	// Gets at the dynamic mesh
	virtual IMesh*	GetDynamicMesh( IMaterial* pMaterial, VertexFormat_t vertexFormat, int nHWSkinBoneCount, bool buffered = true,
		IMesh* pVertexOverride = 0, IMesh* pIndexOverride = 0) = 0;

// ------------ New Vertex/Index Buffer interface ----------------------------
	// Do we need support for bForceTempMesh and bSoftwareVertexShader?
	// I don't think we use bSoftwareVertexShader anymore. .need to look into bForceTempMesh.
	virtual IVertexBuffer *CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup ) = 0;
	virtual IIndexBuffer *CreateIndexBuffer( ShaderBufferType_t indexBufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup ) = 0;
	virtual void DestroyVertexBuffer( IVertexBuffer * ) = 0;
	virtual void DestroyIndexBuffer( IIndexBuffer * ) = 0;
	// Do we need to specify the stream here in the case of locking multiple dynamic VBs on different streams?
	virtual IVertexBuffer *GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBuffered = true ) = 0;
	virtual IIndexBuffer *GetDynamicIndexBuffer( MaterialIndexFormat_t fmt, bool bBuffered = true ) = 0;
	virtual void BindVertexBuffer( int streamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions = 1 ) = 0;
	virtual void BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes ) = 0;
	virtual void Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount ) = 0;
// ------------ End ----------------------------
	virtual VertexFormat_t GetCurrentVertexFormat( void ) const = 0;
	virtual void RenderPassWithVertexAndIndexBuffers( void ) = 0;

	// Computes the vertex format
	virtual VertexFormat_t ComputeVertexFormat( unsigned int flags, 
			int numTexCoords, int* pTexCoordDimensions, int numBoneWeights,
			int userDataSize ) const = 0;

	// Returns the number of buffers...
	virtual int BufferCount() const = 0;

	// Use fat vertices (for tools)
	virtual void UseFatVertices( bool bUseFat ) = 0;

	// Returns the number of vertices + indices we can render using the dynamic mesh
	// Passing true in the second parameter will return the max # of vertices + indices
	// we can use before a flush is provoked and may return different values 
	// if called multiple times in succession. 
	// Passing false into the second parameter will return
	// the maximum possible vertices + indices that can be rendered in a single batch
	virtual void GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices ) = 0;

	// Returns the max number of vertices we can render for a given material
	virtual int GetMaxVerticesToRender( IMaterial *pMaterial ) = 0;
	virtual int GetMaxIndicesToRender( ) = 0;
	virtual IMesh *GetFlexMesh() = 0;

	virtual void ComputeVertexDescription( unsigned char* pBuffer, VertexFormat_t vertexFormat, MeshDesc_t& desc ) const = 0;

	virtual IVertexBuffer *GetDynamicVertexBuffer( IMaterial *pMaterial, bool buffered = true ) = 0;
	virtual IIndexBuffer *GetDynamicIndexBuffer( IMaterial *pMaterial, bool buffered = true ) = 0;

	virtual void MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords ) = 0;
};

#endif // IMESHDX8_H
