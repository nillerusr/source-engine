//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#include "pch_materialsystem.h"

#include "tier1/functors.h"
#include "itextureinternal.h"

#define MATSYS_INTERNAL

#include "cmatqueuedrendercontext.h"
#include "cmaterialsystem.h" // @HACKHACK

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


ConVar mat_report_queue_status( "mat_report_queue_status", "0", FCVAR_MATERIAL_SYSTEM_THREAD );

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

#if defined( _WIN32 )
void FastCopy( byte *pDest, const byte *pSrc, size_t nBytes )
{
	if ( !nBytes )
	{
		return;
	}

#if !defined( _X360 )
	if ( (size_t)pDest % 16 == 0 && (size_t)pSrc % 16 == 0 )
	{
		const int BYTES_PER_FULL = 128;
		int nBytesFull = nBytes - ( nBytes % BYTES_PER_FULL );
		for ( byte *pLimit = pDest + nBytesFull; pDest < pLimit; pDest += BYTES_PER_FULL, pSrc += BYTES_PER_FULL )
		{
			// memcpy( pDest, pSrc, BYTES_PER_FULL);
			__asm
			{
				mov esi, pSrc
				mov edi, pDest

				movaps xmm0, [esi + 0]
				movaps xmm1, [esi + 16]
				movaps xmm2, [esi + 32]
				movaps xmm3, [esi + 48]
				movaps xmm4, [esi + 64]
				movaps xmm5, [esi + 80]
				movaps xmm6, [esi + 96]
				movaps xmm7, [esi + 112]

				movntps [edi + 0], xmm0
				movntps [edi + 16], xmm1
				movntps [edi + 32], xmm2
				movntps [edi + 48], xmm3
				movntps [edi + 64], xmm4
				movntps [edi + 80], xmm5
				movntps [edi + 96], xmm6
				movntps [edi + 112], xmm7
			}
		}
		nBytes -= nBytesFull;
	}

	if ( nBytes )
	{
		memcpy( pDest, pSrc, nBytes );
	}
#else
	if ( (size_t)pDest % 4 == 0 && nBytes % 4 == 0 )
	{
		XMemCpyStreaming_WriteCombined( pDest, pSrc, nBytes );
	}
	else
	{
		// work around a bug in memcpy
		if ((size_t)pDest % 2 == 0 && nBytes == 4)
		{
			*(reinterpret_cast<short *>(pDest)) = *(reinterpret_cast<const short *>(pSrc));
			*(reinterpret_cast<short *>(pDest)+1) = *(reinterpret_cast<const short *>(pSrc)+1);
		}
		else
		{
			memcpy( pDest, pSrc, nBytes );
		}
	}
#endif
}
#else
#define FastCopy memcpy
#endif

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------

enum MatQueuedMeshFlags_t
{
	MQM_BUFFERED	= ( 1 << 0 ),
	MQM_FLEX		= ( 1 << 1 ),
};

class CMatQueuedMesh : public IMesh
{
public:
	CMatQueuedMesh( CMatQueuedRenderContext *pOwner, IMatRenderContextInternal *pHardwareContext )
	 :	m_pLateBoundMesh( &m_pActualMesh ),
		m_pOwner( pOwner ), 
		m_pCallQueue( pOwner->GetCallQueueInternal() ),
		m_pHardwareContext( pHardwareContext ),
		m_pVertexData( NULL ),
		m_pIndexData( NULL ),
		m_nVerts( 0 ),
		m_nIndices( 0 ),
		m_VertexSize( 0 ),
		m_Type(MATERIAL_TRIANGLES),
		m_pVertexOverride( NULL ),
		m_pIndexOverride ( NULL ),
		m_pActualMesh( NULL ),
		m_nActualVertexOffsetInBytes( 0 ),
		m_VertexFormat( 0 ),
		m_MorphFormat( 0 )
	{
	}

	CLateBoundPtr<IMesh> &AccessLateBoundMesh()
	{
		return m_pLateBoundMesh;
	}

	byte *GetVertexData()		{ return m_pVertexData; }
	uint16 *GetIndexData()		{ return m_pIndexData; }
	IMesh *DetachActualMesh()	{ IMesh *p = m_pActualMesh; m_pActualMesh = NULL; return p; }
	IMesh *GetActualMesh()		{ return m_pActualMesh; }
	int GetActualVertexOffsetInBytes() { return m_nActualVertexOffsetInBytes; }

	void DeferredGetDynamicMesh( VertexFormat_t vertexFormat, unsigned flags, IMesh* pVertexOverride, IMesh* pIndexOverride, IMaterialInternal *pMaterial )
	{
		if ( !( flags & MQM_FLEX ))
		{
			if ( vertexFormat == 0 )
			{
				m_pActualMesh = m_pHardwareContext->GetDynamicMesh( ( ( flags & MQM_BUFFERED ) != 0 ), pVertexOverride, pIndexOverride, pMaterial );
			}
			else
			{
				m_pActualMesh = m_pHardwareContext->GetDynamicMeshEx( vertexFormat, ( ( flags & MQM_BUFFERED ) != 0 ), pVertexOverride, pIndexOverride, pMaterial );
			}
		}
		else
		{
			m_pActualMesh = m_pHardwareContext->GetFlexMesh();
		}
	}

	bool OnGetDynamicMesh( VertexFormat_t vertexFormat, unsigned flags, IMesh* pVertexOverride, IMesh* pIndexOverride, IMaterialInternal *pMaterial, int nHWSkinBoneCount )
	{
		if ( !m_pVertexOverride && ( m_pVertexData || m_pIndexData ) )
		{
			CannotSupport();
			if ( IsDebug() )
			{
				Assert( !"Getting a dynamic mesh without resolving the previous one" );
			}
			else
			{
				Error( "Getting a dynamic mesh without resolving the previous one" );
			}
		}
		FreeBuffers();

		m_pVertexOverride = pVertexOverride;
		m_pIndexOverride = pIndexOverride;

		if ( !( flags & MQM_FLEX ) )
		{
			if ( pVertexOverride )
			{
				m_VertexFormat = pVertexOverride->GetVertexFormat();
			}
			else
			{
				// Remove VERTEX_FORMAT_COMPRESSED from the material's format (dynamic meshes don't
				// support compression, and all materials should support uncompressed verts too)
				m_VertexFormat = ( vertexFormat == 0 ) ? ( pMaterial->GetVertexFormat() & ~VERTEX_FORMAT_COMPRESSED ) : vertexFormat;

				if ( vertexFormat != 0 )
				{
					int nVertexFormatBoneWeights = NumBoneWeights( vertexFormat );
					if ( nHWSkinBoneCount < nVertexFormatBoneWeights )
					{
						nHWSkinBoneCount = nVertexFormatBoneWeights;
					}
				}
				// Force the requested number of bone weights
				m_VertexFormat &= ~VERTEX_BONE_WEIGHT_MASK;
				m_VertexFormat |= VERTEX_BONEWEIGHT( nHWSkinBoneCount );
				if ( nHWSkinBoneCount > 0 )
				{
					m_VertexFormat |= VERTEX_BONE_INDEX;
				}
			}
		}
		else
		{
			m_VertexFormat = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_USE_EXACT_FORMAT;
			if ( g_pMaterialSystemHardwareConfig->SupportsPixelShaders_2_b() )
			{
				m_VertexFormat |= VERTEX_WRINKLE;
			}
		}

		MeshDesc_t temp;
		g_pShaderAPI->ComputeVertexDescription( 0, m_VertexFormat, temp );
		m_VertexSize = temp.m_ActualVertexSize;

		// queue up get of real dynamic mesh, allocate space for verts & indices
		m_pCallQueue->QueueCall( this, &CMatQueuedMesh::DeferredGetDynamicMesh, vertexFormat, flags, pVertexOverride, pIndexOverride, pMaterial );
		return true;
	}

	void ModifyBegin( int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc )
	{
		CannotSupport();
	}

	void ModifyBeginEx( bool bReadOnly, int firstVertex, int numVerts, int firstIndex, int numIndices, MeshDesc_t& desc )
	{
		CannotSupport();
	}

	void ModifyEnd( MeshDesc_t& desc )
	{
		CannotSupport();
	}

	void GenerateSequentialIndexBuffer( unsigned short* pIndexMemory, int numIndices, int firstVertex )
	{
		Assert( pIndexMemory == m_pIndexData );
		m_pCallQueue->QueueCall( &::GenerateSequentialIndexBuffer, pIndexMemory, numIndices, firstVertex );
	}

	void GenerateQuadIndexBuffer( unsigned short* pIndexMemory, int numIndices, int firstVertex )
	{
		Assert( pIndexMemory == m_pIndexData );
		m_pCallQueue->QueueCall( &::GenerateQuadIndexBuffer, pIndexMemory, numIndices, firstVertex );
	}

	void GeneratePolygonIndexBuffer( unsigned short* pIndexMemory, int numIndices, int firstVertex )
	{
		Assert( pIndexMemory == m_pIndexData );
		m_pCallQueue->QueueCall( &::GeneratePolygonIndexBuffer, pIndexMemory, numIndices, firstVertex );
	}

	void GenerateLineStripIndexBuffer( unsigned short* pIndexMemory, int numIndices, int firstVertex )
	{
		Assert( pIndexMemory == m_pIndexData );
		m_pCallQueue->QueueCall( &::GenerateLineStripIndexBuffer, pIndexMemory, numIndices, firstVertex );
	}

	void GenerateLineLoopIndexBuffer( unsigned short* pIndexMemory, int numIndices, int firstVertex )
	{
		Assert( pIndexMemory == m_pIndexData );
		m_pCallQueue->QueueCall( &::GenerateLineLoopIndexBuffer, pIndexMemory, numIndices, firstVertex );
	}

	int VertexCount() const
	{
		return m_VertexSize ? m_nVerts : 0;
	}

	int IndexCount() const
	{
		return m_nIndices;
	}

	int GetVertexSize()
	{
		return m_VertexSize;
	}

	void SetPrimitiveType( MaterialPrimitiveType_t type )
	{
		m_Type = type;
		m_pCallQueue->QueueCall( m_pLateBoundMesh, &IMesh::SetPrimitiveType, type );
	}

	void SetColorMesh( IMesh *pColorMesh, int nVertexOffset )
	{
		m_pCallQueue->QueueCall( m_pLateBoundMesh, &IMesh::SetColorMesh, pColorMesh, nVertexOffset );
	}

	void Draw( CPrimList *pLists, int nLists )
	{
		CannotSupport();
	}

	void CopyToMeshBuilder( int iStartVert, int nVerts, int iStartIndex, int nIndices, int indexOffset, CMeshBuilder &builder )
	{
		CannotSupport();
	}

	void Spew( int numVerts, int numIndices, const MeshDesc_t & desc )
	{
	}

	void ValidateData( int numVerts, int numIndices, const MeshDesc_t & desc )
	{
	}

	void LockMesh( int numVerts, int numIndices, MeshDesc_t& desc )
	{
		if ( !m_pVertexOverride )
		{
			m_nVerts = numVerts;
		}
		else
		{
			m_nVerts = 0;
		}

		if ( !m_pIndexOverride )
		{
			m_nIndices = numIndices;
		}
		else
		{
			m_nIndices = 0;
		}

		if( numVerts > 0 )
		{
			Assert( m_VertexSize );
			Assert( !m_pVertexData );
			m_pVertexData = (byte *)m_pOwner->AllocVertices( numVerts, m_VertexSize );
			Assert( (unsigned)m_pVertexData % 16 == 0 );

			// Compute the vertex index..
			desc.m_nFirstVertex = 0;
			static_cast< VertexDesc_t* >( &desc )->m_nOffset = 0;
			// Set up the mesh descriptor
			g_pShaderAPI->ComputeVertexDescription( m_pVertexData, m_VertexFormat, desc );
		}
		else
		{
			desc.m_nFirstVertex = 0;
			static_cast< VertexDesc_t* >( &desc )->m_nOffset = 0;
			// Set up the mesh descriptor
			g_pShaderAPI->ComputeVertexDescription( 0, 0, desc );
		}

		if ( m_Type != MATERIAL_POINTS && numIndices > 0 )
		{
			Assert( !m_pIndexData );
			m_pIndexData = m_pOwner->AllocIndices( numIndices );
			desc.m_pIndices = m_pIndexData;
			desc.m_nIndexSize = 1;
			desc.m_nFirstIndex = 0;
			static_cast< IndexDesc_t* >( &desc )->m_nOffset = 0;
		}
		else
		{
			desc.m_pIndices = &gm_ScratchIndexBuffer[0];
			desc.m_nIndexSize = 0;
			desc.m_nFirstIndex = 0;
			static_cast< IndexDesc_t* >( &desc )->m_nOffset = 0;
		}
	}

	void UnlockMesh( int numVerts, int numIndices, MeshDesc_t& desc )
	{
		if ( m_pVertexData && numVerts < m_nVerts )
		{
			m_pVertexData = m_pOwner->ReallocVertices( m_pVertexData, m_nVerts, numVerts, m_VertexSize );
		}
		m_nVerts = numVerts;

		if ( m_pIndexData && numIndices < m_nIndices )
		{
			m_pIndexData = m_pOwner->ReallocIndices( m_pIndexData, m_nIndices, numIndices );
		}
		m_nIndices = numIndices;
	}

	void SetFlexMesh( IMesh *pMesh, int nVertexOffset )
	{
		m_pCallQueue->QueueCall( m_pLateBoundMesh, &IMesh::SetFlexMesh, pMesh, nVertexOffset );
	}

	void DisableFlexMesh()
	{
		m_pCallQueue->QueueCall( m_pLateBoundMesh, &IMesh::DisableFlexMesh );
	}

	void ExecuteDefferredBuild( byte *pVertexData, int nVerts, int nBytesVerts, uint16 *pIndexData, int nIndices )
	{
		Assert( m_pActualMesh );
		MeshDesc_t desc;
		m_pActualMesh->LockMesh( nVerts, nIndices, desc );
		m_nActualVertexOffsetInBytes = desc.m_nFirstVertex * desc.m_ActualVertexSize;
		if ( pVertexData && desc.m_ActualVertexSize ) // if !desc.m_ActualVertexSize, device lost
		{
			void *pDest;
			if ( desc.m_VertexSize_Position != 0 )
			{
				pDest = desc.m_pPosition;
			}
			else
			{
				#define FindMin( desc, pCurrent, tag )	( ( desc.m_VertexSize_##tag != 0 ) ? min( pCurrent, (void *)desc.m_p##tag )  : pCurrent )

				pDest = (void *)(((byte *)0) - 1);

				pDest = FindMin( desc, pDest, BoneWeight );
				pDest = FindMin( desc, pDest, BoneMatrixIndex );
				pDest = FindMin( desc, pDest, Normal );
				pDest = FindMin( desc, pDest, Color );
				pDest = FindMin( desc, pDest, Specular );
				pDest = FindMin( desc, pDest, TangentS );
				pDest = FindMin( desc, pDest, TangentT );
				pDest = FindMin( desc, pDest, Wrinkle );

				for ( int i = 0; i < VERTEX_MAX_TEXTURE_COORDINATES; i++ )
				{
					if ( desc.m_VertexSize_TexCoord[i] && desc.m_pTexCoord < pDest )
					{
						pDest = desc.m_pTexCoord;
					}
				}

				#undef FindMin
			}

			Assert( pDest );
			if ( pDest )
			{
				FastCopy( (byte *)pDest, pVertexData, nBytesVerts );
			}
		}

		if ( pIndexData && pIndexData != &gm_ScratchIndexBuffer[0] && desc.m_nIndexSize )
		{
			if ( !desc.m_nFirstVertex )
			{
				// AssertMsg(desc.m_pIndices & 0x03 == 0,"desc.m_pIndices is misaligned in CMatQueuedMesh::ExecuteDefferedBuild\n");
				FastCopy( (byte *)desc.m_pIndices, (byte *)pIndexData, nIndices * sizeof(*pIndexData) );
			}
			else
			{
				ALIGN16 uint16 tempIndices[16];

				int i = 0;
				if ( (size_t)desc.m_pIndices % 4 == 2 )
				{
					desc.m_pIndices[i] = pIndexData[i] + desc.m_nFirstVertex;
					i++;
				}
				while ( i < nIndices )
				{
					int nToCopy = min( (int)ARRAYSIZE(tempIndices), nIndices - i );
					for ( int j = 0; j < nToCopy; j++ )
					{
						tempIndices[j] = pIndexData[i+j] + desc.m_nFirstVertex;
					}
					FastCopy( (byte *)(desc.m_pIndices + i), (byte *)tempIndices, nToCopy * sizeof(uint16)  );
					i += nToCopy;
				}
			}
		}

		m_pActualMesh->UnlockMesh( nVerts, nIndices, desc );

		if ( pIndexData && pIndexData != &gm_ScratchIndexBuffer[0])
		{
			m_pOwner->FreeIndices( pIndexData, nIndices );
		}
		if ( pVertexData )
		{
			m_pOwner->FreeVertices( pVertexData, nVerts, desc.m_ActualVertexSize );
		}
	}

	void QueueBuild( bool bDetachBuffers = true )
	{
		m_pCallQueue->QueueCall( this, &CMatQueuedMesh::ExecuteDefferredBuild, m_pVertexData, m_nVerts, m_nVerts * m_VertexSize, m_pIndexData, m_nIndices );
		if ( bDetachBuffers )
		{
			DetachBuffers();
			m_Type = MATERIAL_TRIANGLES;
		}
	}

	void Draw( int firstIndex = -1, int numIndices = 0 )
	{
		if ( !m_nVerts && !m_nIndices )
		{
			MarkAsDrawn();
			return;
		}

		void (IMesh::*pfnDraw)( int, int) = &IMesh::Draw; // need assignment to disambiguate overloaded function
		bool bDetachBuffers;
		if ( firstIndex == -1 || numIndices == 0 )
		{
			bDetachBuffers = true;
		}
		else if ( m_pIndexOverride )
		{
			bDetachBuffers = ( firstIndex + numIndices == m_pIndexOverride->IndexCount() );
		}
		else if ( !m_nIndices || firstIndex + numIndices == m_nIndices )
		{
			bDetachBuffers = true;
		}
		else
		{
			bDetachBuffers = false;
		}

		QueueBuild( bDetachBuffers );
		m_pCallQueue->QueueCall( m_pLateBoundMesh, pfnDraw, firstIndex, numIndices );
	}

	void MarkAsDrawn()
	{
		FreeBuffers();
		m_pCallQueue->QueueCall( m_pLateBoundMesh, &IMesh::MarkAsDrawn );
	}

	void FreeBuffers()
	{
		if ( m_pIndexData && m_pIndexData != &gm_ScratchIndexBuffer[0])
		{
			m_pOwner->FreeIndices( m_pIndexData, m_nIndices );
			m_pIndexData = NULL;
		}
		if ( m_pVertexData )
		{
			m_pOwner->FreeVertices( m_pVertexData, m_nVerts, m_VertexSize );
			m_pVertexData = NULL;
		}
	}

	void DetachBuffers()
	{
		m_pVertexData = NULL;
		m_pIndexData = NULL;
	}

	unsigned ComputeMemoryUsed()
	{
		return 0;
	}

	virtual VertexFormat_t GetVertexFormat() const
	{
		return m_VertexFormat;
	}

	virtual IMesh *GetMesh()
	{
		return this;
	}

	// FIXME: Implement!
	virtual bool Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t& desc )
	{
		Assert( 0 );
		return false;
	}
	virtual void Unlock( int nWrittenIndexCount, IndexDesc_t& desc )
	{
		Assert( 0 );
	}
	virtual void ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc )
	{
		CannotSupport();
	}
	void ModifyEnd( IndexDesc_t& desc )
	{
		CannotSupport();
	}
	virtual void Spew( int nIndexCount, const IndexDesc_t & desc )
	{
		Assert( 0 );
	}
	virtual void ValidateData( int nIndexCount, const IndexDesc_t &desc )
	{
		Assert( 0 );
	}
	virtual bool Lock( int nVertexCount, bool bAppend, VertexDesc_t &desc )
	{
		Assert( 0 );
		return false;
	}
	virtual void Unlock( int nVertexCount, VertexDesc_t &desc )
	{
		Assert( 0 );
	}
	virtual void Spew( int nVertexCount, const VertexDesc_t &desc )
	{
		Assert( 0 );
	}
	virtual void ValidateData( int nVertexCount, const VertexDesc_t & desc )
	{
		Assert( 0 );
	}
	virtual bool IsDynamic() const 
	{
		Assert( 0 );
		return false;
	}

	virtual MaterialIndexFormat_t IndexFormat() const
	{
		Assert( 0 );
		return MATERIAL_INDEX_FORMAT_UNKNOWN;
	}

	virtual void BeginCastBuffer( VertexFormat_t format )
	{
		Assert( 0 );
	}

	virtual void BeginCastBuffer( MaterialIndexFormat_t format )
	{
		Assert( 0 );
	}

	virtual void EndCastBuffer( )
	{
		Assert( 0 );
	}

	// Returns the number of vertices that can still be written into the buffer
	virtual int GetRoomRemaining() const
	{
		Assert( 0 );
		return 0;
	}


	//----------------------------------------------------------------------------

	static void DoDraw( int firstIndex = -1, int numIndices = 0 )
	{

	}
private:

	IMesh *m_pActualMesh;
	int m_nActualVertexOffsetInBytes;

	CLateBoundPtr<IMesh> m_pLateBoundMesh;

	CMatQueuedRenderContext *m_pOwner;
	CMatCallQueue *m_pCallQueue;
	IMatRenderContextInternal *m_pHardwareContext;

	//-----------------------------------------------------

	// The vertex format we're using...
	VertexFormat_t m_VertexFormat;

	// The morph format we're using
	MorphFormat_t m_MorphFormat;

	byte *m_pVertexData;
	uint16 *m_pIndexData;

	int m_nVerts;
	int m_nIndices;

	unsigned short m_VertexSize;
	MaterialPrimitiveType_t m_Type;

	// Used in rendering sub-parts of the mesh
	//static unsigned int s_NumIndices;
	//static unsigned int s_FirstIndex;

	IMesh *m_pVertexOverride;
	IMesh *m_pIndexOverride;

	static unsigned short gm_ScratchIndexBuffer[6];
};

unsigned short CMatQueuedMesh::gm_ScratchIndexBuffer[6];

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::Init( CMaterialSystem *pMaterialSystem, CMatRenderContextBase *pHardwareContext )
{
	BaseClass::Init();

	m_pMaterialSystem = pMaterialSystem;
	m_pHardwareContext = pHardwareContext;

	m_pQueuedMesh = new CMatQueuedMesh( this, pHardwareContext );

	MEM_ALLOC_CREDIT();

	int nSize = 16 * 1024 * 1024;
	int nCommitSize = 128 * 1024;
#if defined(DEDICATED)
	Assert( !"CMatQueuedRenderContext shouldn't be initialized on dedicated servers..." );
	nSize = nCommitSize = 1024;
#endif

	bool bVerticesInit = m_Vertices.Init( nSize, nCommitSize );
	bool bIndicesInit = m_Indices.Init( nSize, nCommitSize );

	if ( !bVerticesInit || !bIndicesInit )
	{
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::Shutdown()
{
	if ( !m_pHardwareContext )
	{
		return;
	}

	Assert( !m_pCurrentMaterial );

	delete m_pQueuedMesh;
	m_pMaterialSystem = NULL;
	m_pHardwareContext = NULL;
	m_pQueuedMesh = NULL;

	m_Vertices.Term();
	m_Indices.Term();

	BaseClass::Shutdown();
	Assert(m_queue.Count() == 0);
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::Free()
{
	m_Vertices.FreeAll();
	m_Indices.FreeAll();
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::CompactMemory()
{
	BaseClass::CompactMemory();
	m_Vertices.FreeAll();
	m_Indices.FreeAll();
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::BeginQueue( CMatRenderContextBase *pInitialState )
{
	if ( !pInitialState )
	{
		pInitialState = m_pHardwareContext;
	}
	CMatRenderContextBase::InitializeFrom( pInitialState );
	g_pShaderAPI->GetBackBufferDimensions( m_WidthBackBuffer, m_HeightBackBuffer );
	m_FogMode = pInitialState->GetFogMode();
	m_nBoneCount = pInitialState->GetCurrentNumBones();
	pInitialState->GetFogDistances( &m_flFogStart, &m_flFogEnd, &m_flFogZ );

}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::EndQueue( bool bCallQueued )
{
	if ( bCallQueued )
	{
		CallQueued();
	}
	int i;

	if ( m_pCurrentMaterial )
	{
		m_pCurrentMaterial = NULL;
	}

	if ( m_pUserDefinedLightmap )
	{
		m_pUserDefinedLightmap = NULL;
	}

	if ( m_pLocalCubemapTexture )
	{
		m_pLocalCubemapTexture = NULL;
	}

	for ( i = 0; i < MAX_FB_TEXTURES; i++ )
	{
		if ( m_pCurrentFrameBufferCopyTexture[i] )
		{
			m_pCurrentFrameBufferCopyTexture[i] = NULL;
		}
	}

	for ( i = 0; i < m_RenderTargetStack.Count(); i++ )
	{
		for ( int j = 0; j < MAX_RENDER_TARGETS; j++ )
		{
			if ( m_RenderTargetStack[i].m_pRenderTargets[j] )
			{
				m_RenderTargetStack[i].m_pRenderTargets[j] = NULL;
			}
		}
	}

	m_RenderTargetStack.Clear();
}


void CMatQueuedRenderContext::Bind( IMaterial *iMaterial, void *proxyData )
{
	if ( !iMaterial )
	{
		if( !g_pErrorMaterial )
			return;
	}
	else
	{
		iMaterial = ((IMaterialInternal *)iMaterial)->GetRealTimeVersion(); //always work with the real time versions of materials internally
	}

	CMatRenderContextBase::Bind( iMaterial, proxyData );

	// We've always gotta call the bind proxy (assuming there is one)
	// so we can copy off the material vars at this point.
	IMaterialInternal* pIMaterial = GetCurrentMaterialInternal();
	pIMaterial->CallBindProxy( proxyData );

	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::Bind, iMaterial, proxyData );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::BeginRender()
{
	if ( ++m_iRenderDepth == 1 )
	{
		VPROF_INCREMENT_GROUP_COUNTER( "render/CMatQBeginRender", COUNTER_GROUP_TELEMETRY, 1 );

		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::BeginRender );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::EndRender()
{
	if ( --m_iRenderDepth == 0 )
	{
		m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::EndRender );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::CallQueued( bool bTermAfterCall )
{
	if ( mat_report_queue_status.GetBool() )
	{
		Msg( "%d calls queued for %llu bytes in parameters and overhead, %d bytes verts, %d bytes indices, %d bytes other\n", m_queue.Count(), (uint64)(m_queue.GetMemoryUsed()), m_Vertices.GetUsed(), m_Indices.GetUsed(), RenderDataSizeUsed() );
	}

	m_queue.CallQueued();

	m_Vertices.FreeAll( false );
	m_Indices.FreeAll( false );

	if ( bTermAfterCall )
	{
		Shutdown();
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::FlushQueued()
{
	m_queue.Flush();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
ICallQueue *CMatQueuedRenderContext::GetCallQueue()
{
	return &m_CallQueueExternal;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetRenderTargetEx( int nRenderTargetID, ITexture *pNewTarget ) 
{
	CMatRenderContextBase::SetRenderTargetEx( nRenderTargetID, pNewTarget );

	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetRenderTargetEx, nRenderTargetID, pNewTarget );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::GetRenderTargetDimensions( int &width, int &height) const
{
	// Target at top of stack
	ITexture *pTOS = NULL;

	if ( m_RenderTargetStack.Count() )
	{
		pTOS = m_RenderTargetStack.Top().m_pRenderTargets[ 0 ];
	}

	// If top of stack isn't the back buffer, get dimensions from the texture
	if ( pTOS != NULL )
	{
		width = pTOS->GetActualWidth();
		height = pTOS->GetActualHeight();
	}
	else // otherwise, get them from the shader API
	{
		width = m_WidthBackBuffer;
		height = m_HeightBackBuffer;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::Viewport( int x, int y, int width, int height )
{
	CMatRenderContextBase::Viewport( x, y, width, height );
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::Viewport, x, y, width, height );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetLight( int i, const LightDesc_t &desc )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetLight, i, RefToVal( desc ) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetLightingOrigin( Vector vLightingOrigin )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetLightingOrigin, vLightingOrigin );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetAmbientLightCube( LightCube_t cube )
{
	// FIXME: does compiler do the right thing, is envelope needed?
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetAmbientLightCube, CUtlEnvelope<Vector4D>( &cube[0], 6 ) );
}

//-----------------------------------------------------------------------------
// Bone count
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetNumBoneWeights( int nBoneCount )
{
	m_nBoneCount = nBoneCount;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetNumBoneWeights, nBoneCount );
}

int	CMatQueuedRenderContext::GetCurrentNumBones( ) const
{
	return m_nBoneCount;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::FogMode( MaterialFogMode_t fogMode )
{
	m_FogMode = fogMode;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::FogMode, fogMode );
}

void CMatQueuedRenderContext::FogStart( float fStart )
{
	m_flFogStart = fStart;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::FogStart, fStart );
}

void CMatQueuedRenderContext::FogEnd( float fEnd )
{
	m_flFogEnd = fEnd;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::FogEnd, fEnd );
}

void CMatQueuedRenderContext::FogMaxDensity( float flMaxDensity )
{
	m_flFogMaxDensity = flMaxDensity;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::FogMaxDensity, flMaxDensity );
}

void CMatQueuedRenderContext::SetFogZ( float fogZ )
{
	m_flFogZ = fogZ;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetFogZ, fogZ );
}

MaterialFogMode_t CMatQueuedRenderContext::GetFogMode( void )
{
	return m_FogMode;
}

void CMatQueuedRenderContext::FogColor3f( float r, float g, float b )
{
	FogColor3ub( clamp( (int)(r * 255.0f), 0, 255 ), clamp( (int)(g * 255.0f), 0, 255 ), clamp( (int)(b * 255.0f), 0, 255 ) );
}

void CMatQueuedRenderContext::FogColor3fv( float const* rgb )
{
	FogColor3ub( clamp( (int)(rgb[0] * 255.0f), 0, 255 ), clamp( (int)(rgb[1] * 255.0f), 0, 255 ), clamp( (int)(rgb[2] * 255.0f), 0, 255 ) );
}

void CMatQueuedRenderContext::FogColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
	m_FogColor.r = r;
	m_FogColor.g = g;
	m_FogColor.b = b;
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::FogColor3ub, r, g, b );
}

void CMatQueuedRenderContext::FogColor3ubv( unsigned char const* rgb )
{
	FogColor3ub( rgb[0], rgb[1], rgb[2] );
}

void CMatQueuedRenderContext::GetFogColor( unsigned char *rgb )
{
	rgb[0] = m_FogColor.r;
	rgb[1] = m_FogColor.g;
	rgb[2] = m_FogColor.b;
}

void CMatQueuedRenderContext::GetFogDistances( float *fStart, float *fEnd, float *fFogZ )
{
	if( fStart )
		*fStart = m_flFogStart;

	if( fEnd )
		*fEnd = m_flFogEnd;

	if( fFogZ )
		*fFogZ = m_flFogZ;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::GetViewport( int& x, int& y, int& width, int& height ) const
{
	// Verify valid top of RT stack
	Assert ( m_RenderTargetStack.Count() > 0 );

	// Grab the top of stack
	const RenderTargetStackElement_t& element = m_RenderTargetStack.Top();

	// If either dimension is negative, set to full bounds of current target
	if ( (element.m_nViewW < 0) || (element.m_nViewH < 0) )
	{
		// Viewport origin at target origin
		x = y = 0;

		// If target is back buffer
		if ( element.m_pRenderTargets[0] == NULL )
		{
			width = m_WidthBackBuffer;
			height = m_HeightBackBuffer;
		}
		else // if target is texture
		{
			width = element.m_pRenderTargets[0]->GetActualWidth();
			height = element.m_pRenderTargets[0]->GetActualHeight();
		}
	}
	else // use the bounds from the stack directly
	{
		x = element.m_nViewX;
		y = element.m_nViewY;
		width = element.m_nViewW;
		height = element.m_nViewH;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SyncToken( const char *p )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SyncToken, CUtlEnvelope<const char *>( p ) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IMesh* CMatQueuedRenderContext::GetDynamicMesh( bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride, IMaterial *pAutoBind )
{
	if( pAutoBind )
		Bind( pAutoBind, NULL );

	if ( pVertexOverride && pIndexOverride )
	{
		// Use the new batch API
		DebuggerBreak();
		return NULL;
	}

	if ( pVertexOverride )
	{
		if ( CompressionType( pVertexOverride->GetVertexFormat() ) != VERTEX_COMPRESSION_NONE )
		{
			// UNDONE: support compressed dynamic meshes if needed (pro: less VB memory, con: time spent compressing)
			DebuggerBreak();
			return NULL;
		}
	}

	// For anything more than 1 bone, imply the last weight from the 1 - the sum of the others.
	int nCurrentBoneCount = GetCurrentNumBones();
	Assert( nCurrentBoneCount <= 4 );
	if ( nCurrentBoneCount > 1 )
	{
		--nCurrentBoneCount;
	}

	m_pQueuedMesh->OnGetDynamicMesh( 0, ( buffered ) ? MQM_BUFFERED : 0, pVertexOverride, pIndexOverride, GetCurrentMaterialInternal(), nCurrentBoneCount );
	return m_pQueuedMesh;
}

IMesh* CMatQueuedRenderContext::GetDynamicMeshEx( VertexFormat_t vertexFormat, bool bBuffered, IMesh* pVertexOverride, IMesh* pIndexOverride, IMaterial *pAutoBind )
{
	if( pAutoBind )
	{
		Bind( pAutoBind, NULL );
	}

	if ( pVertexOverride && pIndexOverride )
	{
		// Use the new batch API
		DebuggerBreak();
		return NULL;
	}

	if ( pVertexOverride )
	{
		if ( CompressionType( pVertexOverride->GetVertexFormat() ) != VERTEX_COMPRESSION_NONE )
		{
			// UNDONE: support compressed dynamic meshes if needed (pro: less VB memory, con: time spent compressing)
			DebuggerBreak();
			return NULL;
		}
	}

	// For anything more than 1 bone, imply the last weight from the 1 - the sum of the others.
	int nCurrentBoneCount = GetCurrentNumBones();
	Assert( nCurrentBoneCount <= 4 );
	if ( nCurrentBoneCount > 1 )
	{
		--nCurrentBoneCount;
	}

	m_pQueuedMesh->OnGetDynamicMesh( vertexFormat, ( bBuffered ) ? MQM_BUFFERED : 0, pVertexOverride, pIndexOverride, GetCurrentMaterialInternal(), nCurrentBoneCount );
	return m_pQueuedMesh;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CMatQueuedRenderContext::GetMaxVerticesToRender( IMaterial *pMaterial )
{
	pMaterial = ((IMaterialInternal *)pMaterial)->GetRealTimeVersion(); //always work with the real time version of materials internally.

	MeshDesc_t temp;

	// Be conservative, assume no compression (in here, we don't know if the caller will used a compressed VB or not)
	// FIXME: allow the caller to specify which compression type should be used to compute size from the vertex format
	//        (this can vary between multiple VBs/Meshes using the same material)
	VertexFormat_t materialFormat = pMaterial->GetVertexFormat() & ~VERTEX_FORMAT_COMPRESSED;
	g_pShaderAPI->ComputeVertexDescription( 0, materialFormat, temp );

	int maxVerts = g_pShaderAPI->GetCurrentDynamicVBSize() / temp.m_ActualVertexSize;
	if ( maxVerts > 65535 )
	{
		maxVerts = 65535;
	}
	return maxVerts;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices )
{
	Assert( !bMaxUntilFlush );
	*pMaxVerts = g_pShaderAPI->GetCurrentDynamicVBSize() / m_pQueuedMesh->GetVertexSize();
	if ( *pMaxVerts > 65535 )
	{
		*pMaxVerts = 65535;
	}
	*pMaxIndices = INDEX_BUFFER_SIZE;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IMesh *CMatQueuedRenderContext::GetFlexMesh()
{
	m_pQueuedMesh->OnGetDynamicMesh( 0, MQM_FLEX, NULL, NULL, NULL, 0 );
	return m_pQueuedMesh;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
OcclusionQueryObjectHandle_t CMatQueuedRenderContext::CreateOcclusionQueryObject()
{
	OcclusionQueryObjectHandle_t h = g_pOcclusionQueryMgr->CreateOcclusionQueryObject();
	m_queue.QueueCall( g_pOcclusionQueryMgr, &COcclusionQueryMgr::OnCreateOcclusionQueryObject, h );
	return h;
}

int CMatQueuedRenderContext::OcclusionQuery_GetNumPixelsRendered( OcclusionQueryObjectHandle_t h )
{
	m_queue.QueueCall( g_pOcclusionQueryMgr, &COcclusionQueryMgr::OcclusionQuery_IssueNumPixelsRenderedQuery, h );
	return g_pOcclusionQueryMgr->OcclusionQuery_GetNumPixelsRendered( h, false );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::SetFlashlightState( const FlashlightState_t &s, const VMatrix &m )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::SetFlashlightState, RefToVal( s ), RefToVal( m ) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::EnableClipping( bool bEnable )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::EnableClipping, bEnable );
	return BaseClass::EnableClipping( bEnable );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::UserClipTransform( const VMatrix &m )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::UserClipTransform, RefToVal( m ) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::GetWindowSize( int &width, int &height ) const
{
	width = m_WidthBackBuffer;
	height = m_HeightBackBuffer;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::DrawScreenSpaceRectangle( 
	IMaterial *pMaterial,
	int destx, int desty,
	int width, int height,
	float src_texture_x0, float src_texture_y0,			// which texel you want to appear at
	// destx/y
	float src_texture_x1, float src_texture_y1,			// which texel you want to appear at
	// destx+width-1, desty+height-1
	int src_texture_width, int src_texture_height,		// needed for fixup
	void *pClientRenderable,
	int nXDice, int nYDice )							// Amount to tessellate the quad
{
	IMaterial *pRealTimeVersionMaterial = ((IMaterialInternal *)pMaterial)->GetRealTimeVersion();
	pRealTimeVersionMaterial->CallBindProxy( pClientRenderable );
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::DrawScreenSpaceRectangle, pMaterial, destx, desty, width, height, src_texture_x0, src_texture_y0, src_texture_x1, src_texture_y1,	src_texture_width, src_texture_height, pClientRenderable, nXDice, nYDice );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::LoadBoneMatrix( int i, const matrix3x4_t &m )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::LoadBoneMatrix, i, RefToVal( m ) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::CopyRenderTargetToTextureEx( ITexture *pTexture, int i, Rect_t *pSrc, Rect_t *pDst )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::CopyRenderTargetToTextureEx, pTexture, i, CUtlEnvelope<Rect_t>(pSrc), CUtlEnvelope<Rect_t>(pDst) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::CopyTextureToRenderTargetEx( int i, ITexture *pTexture, Rect_t *pSrc, Rect_t *pDst )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::CopyTextureToRenderTargetEx, i, pTexture, CUtlEnvelope<Rect_t>(pSrc), CUtlEnvelope<Rect_t>(pDst) );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::OnDrawMesh( IMesh *pMesh, int firstIndex, int numIndices )
{
	void (IMesh::*pfnDraw)( int, int) = &IMesh::Draw; // need assignment to disambiguate overloaded function
	m_queue.QueueCall( pMesh, pfnDraw, firstIndex, numIndices );
	return false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::OnDrawMesh( IMesh *pMesh, CPrimList *pLists, int nLists )
{
	CMatRenderData< CPrimList > rdPrimList( this, nLists, pLists );
	m_queue.QueueCall( this, &CMatQueuedRenderContext::DeferredDrawPrimList, pMesh, rdPrimList.Base(), nLists );
	return false;
}

void CMatQueuedRenderContext::DeferredDrawPrimList( IMesh *pMesh, CPrimList *pLists, int nLists )
{
	pMesh->Draw( pLists, nLists );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CMatQueuedRenderContext::DeferredSetFlexMesh( IMesh *pStaticMesh, int nVertexOffsetInBytes )
{
 	pStaticMesh->SetFlexMesh( m_pQueuedMesh->GetActualMesh(), m_pQueuedMesh->GetActualVertexOffsetInBytes() );
}

bool CMatQueuedRenderContext::OnSetFlexMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes )
{
	Assert( pMesh == m_pQueuedMesh || !pMesh );
	if ( pMesh )
	{
		m_pQueuedMesh->QueueBuild();
		m_queue.QueueCall( this, &CMatQueuedRenderContext::DeferredSetFlexMesh, pStaticMesh, nVertexOffsetInBytes );
	}
	else
	{
		m_queue.QueueCall( pStaticMesh, &IMesh::SetFlexMesh, (IMesh *)NULL, 0 );
	}
	return false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::OnSetColorMesh( IMesh *pStaticMesh, IMesh *pMesh, int nVertexOffsetInBytes )
{
	m_queue.QueueCall( pStaticMesh, &IMesh::SetColorMesh, pMesh, nVertexOffsetInBytes );
	return false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::OnSetPrimitiveType( IMesh *pMesh, MaterialPrimitiveType_t type )
{
	m_queue.QueueCall( pMesh, &IMesh::SetPrimitiveType, type );
	return false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMatQueuedRenderContext::OnFlushBufferedPrimitives()
{
	m_queue.QueueCall( g_pShaderAPI, &IShaderAPI::FlushBufferedPrimitives );
	return false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
inline void CMatQueuedRenderContext::QueueMatrixSync()
{
	void (IMatRenderContext::*pfnLoadMatrix)( const VMatrix & ) = &IMatRenderContext::LoadMatrix; // need assignment to disambiguate overloaded function
	m_queue.QueueCall( m_pHardwareContext, pfnLoadMatrix, RefToVal( AccessCurrentMatrix() ) );
}

void CMatQueuedRenderContext::MatrixMode( MaterialMatrixMode_t mode )
{
	CMatRenderContextBase::MatrixMode( mode );
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::MatrixMode, mode );
}

void CMatQueuedRenderContext::PushMatrix()
{
	CMatRenderContextBase::PushMatrix();
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::PushMatrix );
}

void CMatQueuedRenderContext::PopMatrix()
{
	CMatRenderContextBase::PopMatrix();
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::PopMatrix );
}

void CMatQueuedRenderContext::LoadMatrix( const VMatrix& matrix )
{
	CMatRenderContextBase::LoadMatrix( matrix );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::LoadMatrix( const matrix3x4_t& matrix )
{
	CMatRenderContextBase::LoadMatrix( matrix );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::MultMatrix( const VMatrix& matrix )
{
	CMatRenderContextBase::MultMatrix( matrix );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::MultMatrix( const matrix3x4_t& matrix )
{
	CMatRenderContextBase::MultMatrix( VMatrix( matrix ) );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::MultMatrixLocal( const VMatrix& matrix )
{
	CMatRenderContextBase::MultMatrixLocal( matrix );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::MultMatrixLocal( const matrix3x4_t& matrix )
{
	CMatRenderContextBase::MultMatrixLocal( VMatrix( matrix ) );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::LoadIdentity()
{
	CMatRenderContextBase::LoadIdentity();
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::LoadIdentity );
}

void CMatQueuedRenderContext::Ortho( double left, double top, double right, double bottom, double zNear, double zFar )
{
	CMatRenderContextBase::Ortho( left, top, right, bottom, zNear, zFar );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::PerspectiveX( double flFovX, double flAspect, double flZNear, double flZFar )
{
	CMatRenderContextBase::PerspectiveX( flFovX, flAspect, flZNear, flZFar );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::PerspectiveOffCenterX( double flFovX, double flAspect, double flZNear, double flZFar, double bottom, double top, double left, double right )
{
	CMatRenderContextBase::PerspectiveOffCenterX( flFovX, flAspect, flZNear, flZFar, bottom, top, left, right );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::PickMatrix( int x, int y, int nWidth, int nHeight )
{
	CMatRenderContextBase::PickMatrix( x, y, nWidth, nHeight );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::Rotate( float flAngle, float x, float y, float z )
{
	CMatRenderContextBase::Rotate( flAngle, x, y, z );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::Translate( float x, float y, float z )
{
	CMatRenderContextBase::Translate( x, y, z );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::Scale( float x, float y, float z )
{
	CMatRenderContextBase::Scale( x, y, z );
	QueueMatrixSync();
}

void CMatQueuedRenderContext::BeginBatch( IMesh* pIndices ) 
{
	Assert( pIndices == (IMesh *)m_pQueuedMesh );
	m_queue.QueueCall( this, &CMatQueuedRenderContext::DeferredBeginBatch, m_pQueuedMesh->GetIndexData(), m_pQueuedMesh->IndexCount() );
	m_pQueuedMesh->DetachBuffers();
}

void CMatQueuedRenderContext::BindBatch( IMesh* pVertices, IMaterial *pAutoBind ) 
{
	Assert( pVertices != (IMesh *)m_pQueuedMesh );
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::BindBatch, pVertices, pAutoBind );
}

void CMatQueuedRenderContext::DrawBatch(int firstIndex, int numIndices )
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::DrawBatch, firstIndex, numIndices );
}

void CMatQueuedRenderContext::EndBatch()
{
	m_queue.QueueCall( m_pHardwareContext, &IMatRenderContext::EndBatch );
}

void CMatQueuedRenderContext::DeferredBeginBatch( uint16 *pIndexData, int nIndices )
{
	m_pQueuedMesh->DeferredGetDynamicMesh( 0, false, NULL, NULL, NULL );
	m_pQueuedMesh->ExecuteDefferredBuild( NULL, 0, 0, pIndexData, nIndices );
	m_pHardwareContext->BeginBatch( m_pQueuedMesh->DetachActualMesh() );
}

//-----------------------------------------------------------------------------
// Memory allocation calls for queued mesh, et. al.
//-----------------------------------------------------------------------------
byte *CMatQueuedRenderContext::AllocVertices( int nVerts, int nVertexSize )
{
	MEM_ALLOC_CREDIT();

#if defined(_WIN32) && !defined(_X360)
	const byte *pNextAlloc = (const byte *)(m_Vertices.GetBase()) + m_Vertices.GetUsed() + AlignValue( nVerts * nVertexSize, 16 );
	const byte *pCommitLimit = (const byte *)(m_Vertices.GetBase()) + m_Vertices.GetSize();
#endif

	void *pResult = m_Vertices.Alloc( nVerts * nVertexSize, false );
#if defined(_WIN32) && !defined(_X360)
	if ( !pResult )
	{
		// Force a crash with useful minidump info in the registers.
		uint64 status = 0x31415926;

		// Print some information to the console so that it's picked up in the minidump comment.
		Msg( "AllocVertices( %d, %d ) on %p failed. m_Vertices is based at %p with a size of 0x%x.\n", nVerts, nVertexSize, this, m_Vertices.GetBase(), m_Vertices.GetSize() );
		Msg( "%d vertices used.\n", m_Vertices.GetUsed() );
		if ( pNextAlloc > pCommitLimit )
		{
			Msg( "VirtualAlloc would have been called. %p > %p.\n", pNextAlloc, pCommitLimit );

			const byte *pNewCommitLimit = AlignValue( pNextAlloc, 128 * 1024 );
			const uint32 commitSize = pNewCommitLimit - pCommitLimit;
			const void *pRet = VirtualAlloc( (void *)pCommitLimit, commitSize, MEM_COMMIT, PAGE_READWRITE );
			if ( !pRet )
				status = GetLastError();

			Msg( "VirtualAlloc( %p, %d ) returned %p on repeat. VirtualAlloc %s with code %x.\n", pCommitLimit, commitSize, pRet, (pRet != NULL) ? "succeeded" : "failed", (uint32) status );
		}
		else
		{
			Msg( "VirtualAlloc would not have been called. %p <= %p.\n", pNextAlloc, pCommitLimit );
		}

		// Now crash.
		*(volatile uint64 *)0 = status << 32 | m_Vertices.GetUsed();
	}
#endif
	return (byte *) pResult;
}

uint16 *CMatQueuedRenderContext::AllocIndices( int nIndices )
{
	MEM_ALLOC_CREDIT();

#if defined(_WIN32) && !defined(_X360)
	const byte *pNextAlloc = (const byte *)(m_Indices.GetBase()) + m_Indices.GetUsed() + AlignValue( nIndices * sizeof(uint16), 16 );
	const byte *pCommitLimit = (const byte *)(m_Indices.GetBase()) + m_Indices.GetSize();
#endif

	void *pResult = m_Indices.Alloc( nIndices * sizeof(uint16), false );
#if defined(_WIN32) && !defined(_X360)
	if ( !pResult )
	{
		// Force a crash with useful minidump info in the registers.
		uint64 status = 0x31415926;

		// Print some information to the console so that it's picked up in the minidump comment.
		Msg( "AllocIndices( %d ) on %p failed. m_Indices is based at %p with a size of 0x%x.\n", nIndices, this, m_Indices.GetBase(), m_Indices.GetSize() );
		Msg( "%d indices used.\n", m_Indices.GetUsed() );
		if ( pNextAlloc > pCommitLimit )
		{
			Msg( "VirtualAlloc would have been called. %p > %p.\n", pNextAlloc, pCommitLimit );

			const byte *pNewCommitLimit = AlignValue( pNextAlloc, 128 * 1024 );
			const uint32 commitSize = pNewCommitLimit - pCommitLimit;
			const void *pRet = VirtualAlloc( (void *)pCommitLimit, commitSize, MEM_COMMIT, PAGE_READWRITE );
			if ( !pRet )
				status = GetLastError();

			Msg( "VirtualAlloc( %p, %d ) returned %p on repeat. VirtualAlloc %s with code %x.\n", pCommitLimit, commitSize, pRet, (pRet != NULL) ? "succeeded" : "failed", (uint32) status );
		}
		else
		{
			Msg( "VirtualAlloc would not have been called. %p <= %p.\n", pNextAlloc, pCommitLimit );
		}

		// Now crash.
		*(volatile uint64 *)0 = status << 32 | m_Indices.GetUsed();
	}
#endif
	return (uint16 *) pResult;
}

byte *CMatQueuedRenderContext::ReallocVertices( byte *pVerts, int nVertsOld, int nVertsNew, int nVertexSize )
{
	Assert( nVertsNew <= nVertsOld );

	if ( nVertsNew < nVertsOld )
	{
		unsigned nBytes = ( ( nVertsOld - nVertsNew ) * nVertexSize );
		m_Vertices.FreeToAllocPoint( AlignValue( m_Vertices.GetCurrentAllocPoint() - nBytes, 16), false ); // memstacks 128 bit aligned
	}
	return pVerts;
}

uint16 *CMatQueuedRenderContext::ReallocIndices( uint16 *pIndices, int nIndicesOld, int nIndicesNew )
{
	Assert( nIndicesNew <= nIndicesOld );
	if ( nIndicesNew < nIndicesOld )
	{
		unsigned nBytes = ( ( nIndicesOld - nIndicesNew ) * sizeof(uint16) );
		m_Indices.FreeToAllocPoint( AlignValue( m_Indices.GetCurrentAllocPoint() - nBytes, 16 ), false ); // memstacks 128 bit aligned
	}
	return pIndices;
}

void CMatQueuedRenderContext::FreeVertices( byte *pVerts, int nVerts, int nVertexSize )
{
	// free at end of call dispatch
}

void CMatQueuedRenderContext::FreeIndices( uint16 *pIndices, int nIndices )
{
	// free at end of call dispatch
}



//------------------------------------------------------------------------------
// Color correction related methods
//------------------------------------------------------------------------------
ColorCorrectionHandle_t CMatQueuedRenderContext::AddLookup( const char *pName )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	ColorCorrectionHandle_t hCC = ColorCorrectionSystem()->AddLookup( pName );
	m_pMaterialSystem->Unlock( hLock );
	return hCC;
}

bool CMatQueuedRenderContext::RemoveLookup( ColorCorrectionHandle_t handle )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	bool bRemoved = ColorCorrectionSystem()->RemoveLookup( handle );
	m_pMaterialSystem->Unlock( hLock );
	return bRemoved;
}

void CMatQueuedRenderContext::LockLookup( ColorCorrectionHandle_t handle )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	ColorCorrectionSystem()->LockLookup( handle );
	m_pMaterialSystem->Unlock( hLock );
}

void CMatQueuedRenderContext::LoadLookup( ColorCorrectionHandle_t handle, const char *pLookupName )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	ColorCorrectionSystem()->LoadLookup( handle, pLookupName );
	m_pMaterialSystem->Unlock( hLock );
}

void CMatQueuedRenderContext::UnlockLookup( ColorCorrectionHandle_t handle )
{
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	ColorCorrectionSystem()->UnlockLookup( handle );
	m_pMaterialSystem->Unlock( hLock );
}

// NOTE: These are synchronous calls!  The rendering thread is stopped, the current queue is drained and the pixels are read
// NOTE: We should also have a queued read pixels in the API for doing mid frame reads (as opposed to screenshots)
void CMatQueuedRenderContext::ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat )
{
	EndRender();
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	this->CallQueued(false);
	g_pShaderAPI->ReadPixels( x, y, width, height, data, dstFormat );
	m_pMaterialSystem->Unlock( hLock );
	BeginRender();
}

void CMatQueuedRenderContext::ReadPixelsAndStretch( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *pBuffer, ImageFormat dstFormat, int nDstStride )
{
	EndRender();
	MaterialLock_t hLock = m_pMaterialSystem->Lock();
	this->CallQueued(false);
	g_pShaderAPI->ReadPixels( pSrcRect, pDstRect, pBuffer, dstFormat, nDstStride );
	m_pMaterialSystem->Unlock( hLock );
	BeginRender();
}

