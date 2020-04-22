//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#define DISABLE_PROTECTED_THINGS
#include "locald3dtypes.h"
#include "imeshdx8.h"
#include "shaderapidx8_global.h"
#include "materialsystem/IShader.h"
#include "tier0/vprof.h"
#include "studio.h"
#include "tier1/fmtstr.h"

#include "tier0/platform.h"
#include "tier0/systeminformation.h"

// fixme - stick this in a header file.
#if defined( _DEBUG ) && !defined( _X360 )
// define this if you want to range check all indices when drawing
#define CHECK_INDICES
#endif
#ifdef CHECK_INDICES
#define CHECK_INDICES_MAX_NUM_STREAMS 2
#endif

#include "dynamicib.h"
#include "dynamicvb.h"
#include "utlvector.h"
#include "shaderapi/ishaderapi.h"
#include "imaterialinternal.h"
#include "imaterialsysteminternal.h"
#include "shaderapidx8.h"
#include "shaderapi/ishaderutil.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/ivballoctracker.h"
#include "tier1/strtools.h"
#include "convar.h"
#include "shaderdevicedx8.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Uncomment this to test buffered state
//-----------------------------------------------------------------------------
//#define DEBUG_BUFFERED_MESHES 1

#define MAX_DX8_STREAMS 16

#define VERTEX_FORMAT_INVALID	0xFFFFFFFFFFFFFFFFull

// this is hooked into the engines convar
extern ConVar mat_debugalttab;

//#define DRAW_SELECTION 1
static bool g_bDrawSelection = true;	// only used in DRAW_SELECTION 
static unsigned short g_nScratchIndexBuffer[6]; // large enough for a fast quad; used when device is not active
#ifdef _DEBUG
int CVertexBuffer::s_BufferCount = 0;
int CIndexBuffer::s_BufferCount = 0;
#endif

//-----------------------------------------------------------------------------
// Important enumerations
//-----------------------------------------------------------------------------
enum
{
	VERTEX_BUFFER_SIZE = 32768,
	MAX_QUAD_INDICES = 16384,
};


//-----------------------------------------------------------------------------
//
// Code related to vertex buffers start here
//
//-----------------------------------------------------------------------------
class CVertexBufferDx8 : public CVertexBufferBase
{
	typedef CVertexBufferBase BaseClass;

	// Methods of IVertexBuffer
public:
	virtual int VertexCount() const;
	virtual VertexFormat_t GetVertexFormat() const;
	virtual bool IsDynamic() const;
	virtual void BeginCastBuffer( VertexFormat_t format );
	virtual void EndCastBuffer( );
	virtual int GetRoomRemaining() const;
	virtual bool Lock( int nVertexCount, bool bAppend, VertexDesc_t &desc );
	virtual void Unlock( int nVertexCount, VertexDesc_t &desc );

public:
	// constructor
	CVertexBufferDx8( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroupName );
	virtual ~CVertexBufferDx8();

	// Allocates, deallocates the index buffer
	bool Allocate( );
	void Free();

	// Returns the vertex size
	int VertexSize() const;

	// Only used by dynamic buffers, indicates the next lock should perform a discard.
	void Flush();

	// Returns the D3D buffer
	IDirect3DVertexBuffer9* GetDx9Buffer();

	// Used to measure how much static buffer memory is touched each frame 
	void HandlePerFrameTextureStats( int nFrame );

protected:
	IDirect3DVertexBuffer9 *m_pVertexBuffer;
	VertexFormat_t m_VertexFormat;
	int m_nVertexCount;
	int m_nBufferSize;
	int m_nFirstUnwrittenOffset;	// Used only for dynamic buffers, indicates where it's safe to write (nooverwrite)

	// Is it locked?
	bool m_bIsLocked : 1;
	bool m_bIsDynamic : 1;
	bool m_bFlush : 1;				// Used only for dynamic buffers, indicates to discard the next time

#ifdef VPROF_ENABLED
	int m_nVProfFrame;
	int	*m_pFrameCounter;
	int	*m_pGlobalCounter;
#endif

#ifdef _DEBUG
	static int s_nBufferCount;
#endif
};


//-----------------------------------------------------------------------------
//
// Code related to index buffers start here
//
//-----------------------------------------------------------------------------
class CIndexBufferDx8 : public CIndexBufferBase
{
	typedef CIndexBufferBase BaseClass;

	// Methods of IIndexBuffer
public:
	virtual int IndexCount( ) const;
	virtual MaterialIndexFormat_t IndexFormat() const;
	virtual int GetRoomRemaining() const;
	virtual bool Lock( int nIndexCount, bool bAppend, IndexDesc_t &desc );
	virtual void Unlock( int nIndexCount, IndexDesc_t &desc );
	virtual void BeginCastBuffer( MaterialIndexFormat_t format );
	virtual void EndCastBuffer( );
	virtual bool IsDynamic() const;
	virtual void ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc ) { Assert(0); }
	virtual void ModifyEnd( IndexDesc_t& desc ) { Assert(0); }

public:
	// constructor
	CIndexBufferDx8( ShaderBufferType_t bufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroupName );
	virtual ~CIndexBufferDx8();

	// Allocates, deallocates the index buffer
	bool Allocate( );
	void Free();

	// Returns the index size
	int IndexSize() const;

	// Only used by dynamic buffers, indicates the next lock should perform a discard.
	void Flush();

	// Returns the D3D buffer
	IDirect3DIndexBuffer9* GetDx9Buffer();

	// Used to measure how much static buffer memory is touched each frame 
	void HandlePerFrameTextureStats( int nFrame );

#ifdef CHECK_INDICES
	unsigned short GetShadowIndex( int i ) const;
#endif

private:
	IDirect3DIndexBuffer9 *m_pIndexBuffer;
	MaterialIndexFormat_t m_IndexFormat;
	int m_nIndexCount;
	int m_nBufferSize;
	int m_nFirstUnwrittenOffset;	// Used only for dynamic buffers, indicates where it's safe to write (nooverwrite)

	// Is it locked?
	bool m_bIsLocked : 1;
	bool m_bIsDynamic : 1;
	bool m_bFlush : 1;				// Used only for dynamic buffers, indicates to discard the next time

#ifdef CHECK_INDICES
	unsigned char *m_pShadowIndices;
	void *m_pLockIndexBuffer;
	int m_nLockIndexBufferSize;
	int m_nLockIndexOffset;
#endif

#ifdef VPROF_ENABLED
	int m_nVProfFrame;
#endif

#ifdef _DEBUG
	static int s_nBufferCount;
#endif
};


//-----------------------------------------------------------------------------
//
// Backward compat mesh code; will go away soon
//
//-----------------------------------------------------------------------------
abstract_class CBaseMeshDX8 : public CMeshBase
{
public:
	// constructor, destructor
	CBaseMeshDX8();
	virtual ~CBaseMeshDX8();

	// FIXME: Make this work! Unsupported methods of IIndexBuffer + IVertexBuffer
	virtual bool Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t& desc ) { Assert(0); return false; }
	virtual void Unlock( int nWrittenIndexCount, IndexDesc_t& desc ) { Assert(0); }
	virtual void ModifyBegin( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t& desc ) { Assert(0); }
	virtual void ModifyEnd( IndexDesc_t& desc ) { Assert(0); }
	virtual void Spew( int nIndexCount, const IndexDesc_t & desc ) { Assert(0); }
	virtual void ValidateData( int nIndexCount, const IndexDesc_t &desc ) { Assert(0); }
	virtual bool Lock( int nVertexCount, bool bAppend, VertexDesc_t &desc ) { Assert(0); return false; }
	virtual void Unlock( int nVertexCount, VertexDesc_t &desc ) { Assert(0); }
	virtual void Spew( int nVertexCount, const VertexDesc_t &desc ) { Assert(0); }
	virtual void ValidateData( int nVertexCount, const VertexDesc_t & desc ) { Assert(0); }

	// Locks mesh for modifying
	void ModifyBeginEx( bool bReadOnly, int nFirstVertex, int nVertexCount, int nFirstIndex, int nIndexCount, MeshDesc_t& desc );
	void ModifyBegin( int nFirstVertex, int nVertexCount, int nFirstIndex, int nIndexCount, MeshDesc_t& desc );
	void ModifyEnd( MeshDesc_t& desc );

	// Sets/gets the vertex format
	virtual void SetVertexFormat( VertexFormat_t format );
	virtual VertexFormat_t GetVertexFormat() const;

	// Sets/gets the morph format
	virtual void SetMorphFormat( MorphFormat_t format );
	virtual MorphFormat_t GetMorphFormat() const;
	// Am I using morph data?
	virtual bool IsUsingMorphData() const;
	bool IsUsingVertexID() const 
	{ 
		return ShaderAPI()->GetBoundMaterial()->IsUsingVertexID();
	}

	// Sets the material
	virtual void SetMaterial( IMaterial* pMaterial );

	// returns the # of vertices (static meshes only)
	int VertexCount() const { return 0; }

	void SetColorMesh( IMesh *pColorMesh, int nVertexOffsetInBytes )
	{
		Assert( 0 );
	}

	void SetFlexMesh( IMesh *pMesh, int nVertexOffsetInBytes )
	{
		Assert( pMesh == NULL && nVertexOffsetInBytes == 0 );
	}

	void DisableFlexMesh( )
	{
		Assert( 0 );
	}

	void MarkAsDrawn() {}

	bool HasColorMesh( ) const { return false; }
	bool HasFlexMesh( ) const { return false; }
	
	// Draws the mesh
	void DrawMesh( );

	// Begins a pass
	void BeginPass( );

	// Spews the mesh data
	virtual void Spew( int nVertexCount, int nIndexCount, const MeshDesc_t & desc );

	// Call this in debug mode to make sure our data is good.
	virtual void ValidateData( int nVertexCount, int nIndexCount, const MeshDesc_t & desc );

	virtual void HandleLateCreation( ) = 0;

	void Draw( CPrimList *pLists, int nLists );

	// Copy verts and/or indices to a mesh builder. This only works for temp meshes!
	virtual void CopyToMeshBuilder( 
		int iStartVert,		// Which vertices to copy.
		int nVerts, 
		int iStartIndex,	// Which indices to copy.
		int nIndices, 
		int indexOffset,	// This is added to each index.
		CMeshBuilder &builder );

	// returns the primitive type
	virtual MaterialPrimitiveType_t GetPrimitiveType() const = 0;

	// Returns the number of indices in a mesh..
	virtual int IndexCount( ) const = 0;

	// FIXME: Make this work!
	virtual MaterialIndexFormat_t IndexFormat() const { return MATERIAL_INDEX_FORMAT_16BIT; }

	// NOTE: For dynamic index buffers only!
	// Casts the memory of the dynamic index buffer to the appropriate type
	virtual void BeginCastBuffer( MaterialIndexFormat_t format ) { Assert(0); }
	virtual void BeginCastBuffer( VertexFormat_t format ) { Assert(0); }
	virtual void EndCastBuffer( ) { Assert(0); }
	virtual int GetRoomRemaining() const { Assert(0); return 0; }

	// returns a static vertex buffer...
	virtual CVertexBuffer *GetVertexBuffer() { return 0; }
	virtual CIndexBuffer *GetIndexBuffer() { return 0; }

	// Do I need to reset the vertex format?
	virtual bool NeedsVertexFormatReset( VertexFormat_t fmt ) const;

	// Do I have enough room?
	virtual bool HasEnoughRoom( int nVertexCount, int nIndexCount ) const;

	// Operation to do pre-lock
	virtual void PreLock() {}

	virtual unsigned ComputeMemoryUsed();

	bool m_bMeshLocked;

protected:
	bool DebugTrace() const;
	
	// The vertex format we're using...
	VertexFormat_t m_VertexFormat;
	
	// The morph format we're using
	MorphFormat_t m_MorphFormat;

#ifdef DBGFLAG_ASSERT
	IMaterialInternal* m_pMaterial;
	bool m_IsDrawing;
#endif
};

//-----------------------------------------------------------------------------
// Implementation of the mesh
//-----------------------------------------------------------------------------
class CMeshDX8 : public CBaseMeshDX8
{
public:
	// constructor
	CMeshDX8( const char *pTextureGroupName );
	virtual ~CMeshDX8();

	// Locks/unlocks the mesh
	void LockMesh( int nVertexCount, int nIndexCount, MeshDesc_t& desc );
	void UnlockMesh( int nVertexCount, int nIndexCount, MeshDesc_t& desc );

	// Locks mesh for modifying
	void ModifyBeginEx( bool bReadOnly, int nFirstVertex, int nVertexCount, int nFirstIndex, int nIndexCount, MeshDesc_t& desc );
	void ModifyBegin( int nFirstVertex, int nVertexCount, int nFirstIndex, int nIndexCount, MeshDesc_t& desc );
	void ModifyEnd( MeshDesc_t& desc );

	// returns the # of vertices (static meshes only)
	int VertexCount() const;

	// returns the # of indices 
	virtual int IndexCount( ) const;

	// Sets up the vertex and index buffers
	void UseIndexBuffer( CIndexBuffer* pBuffer );
	void UseVertexBuffer( CVertexBuffer* pBuffer );

	// returns a static vertex buffer...
	CVertexBuffer *GetVertexBuffer() { return m_pVertexBuffer; }
	CIndexBuffer *GetIndexBuffer() { return m_pIndexBuffer; }

	void SetColorMesh( IMesh *pColorMesh, int nVertexOffsetInBytes );
	void SetFlexMesh( IMesh *pMesh, int nVertexOffsetInBytes );
	void DisableFlexMesh();

	virtual void HandleLateCreation( );

	bool HasColorMesh( ) const;
	bool HasFlexMesh( ) const;

	// Draws the mesh
	void Draw( int nFirstIndex, int nIndexCount );
	void Draw( CPrimList *pLists, int nLists );
	void DrawInternal( CPrimList *pLists, int nLists );

	// Draws a single pass
	void RenderPass();

	// Sets the primitive type
	void SetPrimitiveType( MaterialPrimitiveType_t type );
	MaterialPrimitiveType_t GetPrimitiveType() const;

	bool IsDynamic() const { return false; }

protected:
	// Sets the render state.
	bool SetRenderState( int nVertexOffsetInBytes, int nFirstVertexIdx, VertexFormat_t vertexFormat = VERTEX_FORMAT_INVALID );

	// Is the vertex format valid?
	bool IsValidVertexFormat( VertexFormat_t vertexFormat = VERTEX_FORMAT_INVALID );

	// Locks/ unlocks the vertex buffer
	bool Lock( int nVertexCount, bool bAppend, VertexDesc_t &desc );
	void Unlock( int nVertexCount, VertexDesc_t &desc );

	// Locks/unlocks the index buffer
	// Pass in nFirstIndex=-1 to lock wherever the index buffer is. Pass in a value 
	// >= 0 to specify where to lock.
	int  Lock( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t &pIndices );
	void Unlock( int nIndexCount, IndexDesc_t &desc );

	// computes how many primitives we've got
	int NumPrimitives( int nVertexCount, int nIndexCount ) const;

	// Debugging output...
	void SpewMaterialVerts( );

	// Stream source setting methods
	void SetVertexIDStreamState( );
	void SetColorStreamState( );
	void SetVertexStreamState( int nVertOffsetInBytes );
	void SetIndexStreamState( int firstVertexIdx );

	void CheckIndices( CPrimList *pPrim, int numPrimitives );

	// The vertex and index buffers
	CVertexBuffer* m_pVertexBuffer;
	CIndexBuffer* m_pIndexBuffer;

	// The current color mesh (to be bound to stream 1)
	// The vertex offset allows use of a global, shared color mesh VB
	CMeshDX8 *	m_pColorMesh;
	int			m_nColorMeshVertOffsetInBytes;

	CVertexBuffer *m_pFlexVertexBuffer;

	bool   m_bHasFlexVerts;
	int	   m_nFlexVertOffsetInBytes;
	int m_flexVertCount;

	// Primitive type
	MaterialPrimitiveType_t m_Type;

	// Primitive mode
	D3DPRIMITIVETYPE m_Mode;

	// Number of primitives
	unsigned int m_NumIndices;
	unsigned short m_NumVertices;

	// Is it locked?
	bool m_IsVBLocked;
	bool m_IsIBLocked;

	// Used in rendering sub-parts of the mesh
	static CPrimList *s_pPrims;
	static int s_nPrims;
	static unsigned int s_FirstVertex; // Gets reset during CMeshDX8::DrawInternal
	static unsigned int s_NumVertices;
	int	m_FirstIndex;

#ifdef RECORDING
	int	m_LockVertexBufferSize;
	void *m_LockVertexBuffer;
#endif

#if defined( RECORDING ) || defined( CHECK_INDICES )
	void *m_LockIndexBuffer;
	int	m_LockIndexBufferSize;
#endif
	const char *m_pTextureGroupName;

	friend class CMeshMgr; // MESHFIXME
};


//-----------------------------------------------------------------------------
// A little extra stuff for the dynamic version
//-----------------------------------------------------------------------------
class CDynamicMeshDX8 : public CMeshDX8
{
public:
	// constructor, destructor
	CDynamicMeshDX8();
	virtual ~CDynamicMeshDX8();

	// Initializes the dynamic mesh
	void Init( int nBufferId );

	// Sets the vertex format
	virtual void SetVertexFormat( VertexFormat_t format );

	// Resets the state in case of a task switch
	void Reset();

	// Do I have enough room in the buffer?
	bool HasEnoughRoom( int nVertexCount, int nIndexCount ) const;

	// returns the # of indices
	int IndexCount( ) const;

	// Locks the mesh
	void LockMesh( int nVertexCount, int nIndexCount, MeshDesc_t& desc );

	// Unlocks the mesh
	void UnlockMesh( int nVertexCount, int nIndexCount, MeshDesc_t& desc );

	// Override vertex + index buffer
	void OverrideVertexBuffer( CVertexBuffer *pStaticVertexBuffer );
	void OverrideIndexBuffer( CIndexBuffer *pStaticIndexBuffer );

	// Do I need to reset the vertex format?
	bool NeedsVertexFormatReset(VertexFormat_t fmt) const;

	// Draws it						   
	void Draw( int nFirstIndex, int nIndexCount );
	void MarkAsDrawn() { m_HasDrawn = true; }
	// Simply draws what's been buffered up immediately, without state change 
	void DrawSinglePassImmediately();

	// Operation to do pre-lock
	void PreLock();

	bool IsDynamic() const { return true; }

private:
	// Resets buffering state
	void ResetVertexAndIndexCounts();

	// Buffer Id
	int m_nBufferId;

	// total queued vertices
	int m_TotalVertices;
	int	m_TotalIndices;

	// the first vertex and index since the last draw
	int m_nFirstVertex;
	int m_FirstIndex;

	// Have we drawn since the last lock?
	bool m_HasDrawn;

	// Any overrides?
	bool m_VertexOverride;
	bool m_IndexOverride;
};


//-----------------------------------------------------------------------------
// A mesh that stores temporary vertex data in the correct format (for modification)
//-----------------------------------------------------------------------------
class CTempMeshDX8 : public CBaseMeshDX8
{
public:
	// constructor, destructor
	CTempMeshDX8( bool isDynamic );
	virtual ~CTempMeshDX8();

	// Sets the material
	virtual void SetVertexFormat( VertexFormat_t format );

	// Locks/unlocks the mesh
	void LockMesh( int nVertexCount, int nIndexCount, MeshDesc_t &desc );
	void UnlockMesh( int nVertexCount, int nIndexCount, MeshDesc_t &desc );

	// Locks mesh for modifying
	virtual void ModifyBeginEx( bool bReadOnly, int nFirstVertex, int nVertexCount, int nFirstIndex, int nIndexCount, MeshDesc_t& desc );
	virtual void ModifyBegin( int nFirstVertex, int nVertexCount, int nFirstIndex, int nIndexCount, MeshDesc_t& desc );
	virtual void ModifyEnd( MeshDesc_t& desc );

	// Number of indices + vertices
	int VertexCount() const;
	virtual int IndexCount() const;
	virtual bool IsDynamic() const;

	// Sets the primitive type
	void SetPrimitiveType( MaterialPrimitiveType_t type );
	MaterialPrimitiveType_t GetPrimitiveType() const;

	// Begins a pass
	void BeginPass( );

	// Draws a single pass
	void RenderPass();

	virtual void HandleLateCreation() 
	{ 
		Assert( !"TBD - CTempMeshDX8::HandleLateCreation()" ); 
	}

	// Draws the entire beast
	void Draw( int nFirstIndex, int nIndexCount );

	virtual void CopyToMeshBuilder( 
		int iStartVert,		// Which vertices to copy.
		int nVerts, 
		int iStartIndex,	// Which indices to copy.
		int nIndices, 
		int indexOffset,	// This is added to each index.
		CMeshBuilder &builder );
private:
	// Selection mode 
	void TestSelection( );
	void ClipTriangle( D3DXVECTOR3 **ppVert, float zNear, D3DXMATRIX &proj );

	CDynamicMeshDX8 *GetDynamicMesh();

	CUtlVector< unsigned char, CUtlMemoryAligned< unsigned char, 32 > > m_VertexData;
	CUtlVector< unsigned short > m_IndexData;

	unsigned short m_VertexSize;
	MaterialPrimitiveType_t m_Type;
	int m_LockedVerts;
	int m_LockedIndices;
	bool m_IsDynamic;

	// Used in rendering sub-parts of the mesh
	static unsigned int s_NumIndices;
	static unsigned int s_FirstIndex;

#ifdef DBGFLAG_ASSERT
	bool m_Locked;
	bool m_InPass;
#endif
};

#if 0
//-----------------------------------------------------------------------------
// A mesh that stores temporary vertex data in the correct format (for modification)
//-----------------------------------------------------------------------------
class CTempIndexBufferDX8 : public CIndexBufferBase
{
public:
	// constructor, destructor
	CTempIndexBufferDX8( bool isDynamic );
	virtual ~CTempIndexBufferDX8();

	// Locks/unlocks the mesh
	void LockIndexBuffer( int nIndexCount );
	void UnlockMesh( int nIndexCount );

	// Locks mesh for modifying
	virtual void ModifyBeginEx( bool bReadOnly, int nFirstIndex, int nIndexCount );
	virtual void ModifyEnd();

	// Number of indices
	virtual int IndexCount() const;
	virtual bool IsDynamic() const;

	virtual void CopyToIndexBuilder( 
		int iStartIndex,	// Which indices to copy.
		int nIndices, 
		int indexOffset,	// This is added to each index.
		CIndexBuilder &builder );
private:
	// Selection mode 
	void TestSelection( );

	CDynamicMeshDX8 *GetDynamicMesh();

	CUtlVector< unsigned short > m_IndexData;

	MaterialPrimitiveType_t m_Type;
	int m_LockedIndices;
	bool m_IsDynamic;

	// Used in rendering sub-parts of the mesh
	static unsigned int s_NumIndices;
	static unsigned int s_FirstIndex;

#ifdef DBGFLAG_ASSERT
	bool m_Locked;
	bool m_InPass;
#endif
};
#endif
//-----------------------------------------------------------------------------
// This is a version that buffers up vertex data so we can blast through it later
//-----------------------------------------------------------------------------
class CBufferedMeshDX8 : public CBaseMeshDX8
{
public:
	// constructor, destructor
	CBufferedMeshDX8();
	virtual ~CBufferedMeshDX8();

	// checks to see if it was rendered..
	void ResetRendered();
	bool WasNotRendered() const;

	// Sets the mesh we're really going to draw into
	void SetMesh( CBaseMeshDX8* pMesh );
	const CBaseMeshDX8* GetMesh() const { return m_pMesh; }

	// Spews the mesh data
	virtual void Spew( int nVertexCount, int nIndexCount, const MeshDesc_t &spewDesc );

	// Sets the vertex format
	virtual void SetVertexFormat( VertexFormat_t format );
	virtual VertexFormat_t GetVertexFormat() const;

	// Sets the morph format
	virtual void SetMorphFormat( MorphFormat_t format );

	// Sets the material
	void SetMaterial( IMaterial *pMaterial );

	// returns the number of indices (should never be called!)
	virtual int IndexCount() const { Assert(0); return 0; }
	virtual MaterialIndexFormat_t IndexFormat() const { Assert(0); return MATERIAL_INDEX_FORMAT_16BIT; }
	virtual bool IsDynamic() const { Assert(0); return true; }
	virtual void BeginCastBuffer( MaterialIndexFormat_t format ) { Assert(0); }
	virtual void EndCastBuffer( ) { Assert(0); }
	virtual int GetRoomRemaining() const { Assert(0); return 0; }

	// Locks the mesh
	void LockMesh( int nVertexCount, int nIndexCount, MeshDesc_t& desc );
	void UnlockMesh( int nVertexCount, int nIndexCount, MeshDesc_t& desc );

	// Sets the primitive type
	void SetPrimitiveType( MaterialPrimitiveType_t type );
	MaterialPrimitiveType_t GetPrimitiveType( ) const;

	void ValidateData( int nVertexCount, int nIndexCount, const MeshDesc_t & spewDesc );

	virtual void HandleLateCreation( ) 
	{ 
		if ( m_pMesh ) 
		{
			m_pMesh->HandleLateCreation(); 
		}
	}

	// Draws it
	void Draw( int nFirstIndex, int nIndexCount );

	// Renders a pass
	void RenderPass();

	// Flushes queued data
	void Flush( );

	void SetFlexMesh( IMesh *pMesh, int nVertexOffsetInBytes );

private:
	// The actual mesh we need to render....
	CBaseMeshDX8* m_pMesh;
	
	// The index of the last vertex (for tristrip fixup)
	unsigned short m_LastIndex;

	// Extra padding indices for tristrips
	unsigned short m_ExtraIndices;

	// Am I currently flushing?
	bool m_IsFlushing;

	// has the dynamic mesh been rendered?
	bool m_WasRendered;

	// Do I need to flush?
	bool m_FlushNeeded;

#ifdef DEBUG_BUFFERED_MESHES
	// for debugging only
	bool m_BufferedStateSet;
	BufferedState_t m_BufferedState;
#endif
};


//-----------------------------------------------------------------------------
// Implementation of the mesh manager
//-----------------------------------------------------------------------------
class CMeshMgr : public IMeshMgr
{
public:
	// constructor, destructor
	CMeshMgr();
	virtual ~CMeshMgr();

	// Initialize, shutdown
	void Init();
	void Shutdown();

	// Task switch...
	void ReleaseBuffers();
	void RestoreBuffers();

	// Releases all dynamic vertex buffers
	void DestroyVertexBuffers();

	// Flushes the dynamic mesh
	void Flush();

	// Flushes the vertex buffers
	void DiscardVertexBuffers();

	// Creates, destroys static meshes
	IMesh *CreateStaticMesh( VertexFormat_t vertexFormat, const char *pTextureBudgetGroup, IMaterial *pMaterial = NULL );
	void DestroyStaticMesh( IMesh *pMesh );

	// Gets at the dynamic mesh	(spoofs it though)
	IMesh *GetDynamicMesh( IMaterial *pMaterial, VertexFormat_t vertexFormat, int nHWSkinBoneCount, bool buffered,
		IMesh *pVertexOverride, IMesh *pIndexOverride );

// -----------------------------------------------------------
// ------------ New Vertex/Index Buffer interface ----------------------------
	// Do we need support for bForceTempMesh and bSoftwareVertexShader?
	// I don't think we use bSoftwareVertexShader anymore. .need to look into bForceTempMesh.
	IVertexBuffer *CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup );
	IIndexBuffer *CreateIndexBuffer( ShaderBufferType_t bufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup );
	void DestroyVertexBuffer( IVertexBuffer * );
	void DestroyIndexBuffer( IIndexBuffer * );
	// Do we need to specify the stream here in the case of locking multiple dynamic VBs on different streams?
	IVertexBuffer *GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBuffered = true );
	IIndexBuffer *GetDynamicIndexBuffer( MaterialIndexFormat_t fmt, bool bBuffered = true );
	void BindVertexBuffer( int streamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions = 1 );
	void BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes );
	void Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount );
// ------------ End ----------------------------
	void RenderPassWithVertexAndIndexBuffers( void );

	VertexFormat_t GetCurrentVertexFormat( void ) const { return m_CurrentVertexFormat; }

	// Gets at the *actual* dynamic mesh
	IMesh*	GetActualDynamicMesh( VertexFormat_t vertexFormat );
	IMesh	*GetFlexMesh();

	// Computes vertex format from a list of ingredients
	VertexFormat_t ComputeVertexFormat( unsigned int flags, 
				int numTexCoords, int *pTexCoordDimensions, int numBoneWeights,
				int userDataSize ) const;

	// Use fat vertices (for tools)
	virtual void UseFatVertices( bool bUseFat );

	// Returns the number of vertices we can render using the dynamic mesh
	virtual void GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices );
	virtual int GetMaxVerticesToRender( IMaterial *pMaterial );
	virtual int GetMaxIndicesToRender( );

	// Returns a vertex buffer appropriate for the flags
	CVertexBuffer *FindOrCreateVertexBuffer( int nDynamicBufferId, VertexFormat_t fmt );
	CIndexBuffer *GetDynamicIndexBuffer();

	// Is the mesh dynamic?
	bool IsDynamicMesh( IMesh *pMesh ) const;
	bool IsBufferedDynamicMesh( IMesh *pMesh ) const;

	// Is the vertex buffer dynamic?
	bool IsDynamicVertexBuffer( IVertexBuffer *pVertexBuffer ) const;

	// Is the index buffer dynamic?
	bool IsDynamicIndexBuffer( IIndexBuffer *pIndexBuffer ) const;

	// Returns the vertex size 
	int VertexFormatSize( VertexFormat_t vertexFormat ) const
	{
		return CVertexBufferBase::VertexFormatSize( vertexFormat );
	}

	// Computes the vertex buffer pointers 
	void ComputeVertexDescription( unsigned char *pBuffer, 
		VertexFormat_t vertexFormat, MeshDesc_t &desc ) const;

	// Returns the number of buffers...
	int BufferCount() const
	{
#ifdef _DEBUG
		return CVertexBuffer::BufferCount() + CIndexBuffer::BufferCount();
#else
		return 0;
#endif
	}

	CVertexBuffer *GetVertexIDBuffer();

	IVertexBuffer *GetDynamicVertexBuffer( IMaterial *pMaterial, bool buffered = true );
	IIndexBuffer *GetDynamicIndexBuffer( IMaterial *pMaterial, bool buffered = true );
	virtual void MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords );

	int UnusedVertexFields() const { return m_nUnusedVertexFields; }
	int UnusedTextureCoords() const { return m_nUnusedTextureCoords; }

	IDirect3DVertexBuffer9 *GetZeroVertexBuffer() const { return m_pZeroVertexBuffer; }

private:
	void SetVertexIDStreamState( );
	void SetColorStreamState( );
	void SetVertexStreamState( int nVertOffsetInBytes, int nVertexStride );
	void SetIndexStreamState( int firstVertexIdx );
	bool SetRenderState( int nVertexOffsetInBytes, int nFirstVertexIdx, VertexFormat_t vertexFormat, int nVertexStride );

	struct VertexBufferLookup_t
	{
		CVertexBuffer*	m_pBuffer;
		int				m_VertexSize;
	};

	void CopyStaticMeshIndexBufferToTempMeshIndexBuffer( CTempMeshDX8 *pDstIndexMesh, CMeshDX8 *pSrcIndexMesh );

	// Cleans up the class
	void CleanUp();

	// Creates, destroys the dynamic index
	void CreateDynamicIndexBuffer();
	void DestroyDynamicIndexBuffer();

	// Creates, destroys the vertexID buffer
	void CreateVertexIDBuffer();
	void DestroyVertexIDBuffer();
	void CreateZeroVertexBuffer();
	void DestroyZeroVertexBuffer();

	// Fills a vertexID buffer
	void FillVertexIDBuffer( CVertexBuffer *pVertexIDBuffer, int nCount );

	// The dynamic index buffer
	CIndexBuffer *m_pDynamicIndexBuffer;

	// A static vertexID buffer
	CVertexBuffer *m_pVertexIDBuffer;

	// The dynamic vertex buffers
	CUtlVector< VertexBufferLookup_t >	m_DynamicVertexBuffers;

	// The buffered mesh
	CBufferedMeshDX8 m_BufferedMesh;

	// The current dynamic mesh
	CDynamicMeshDX8 m_DynamicMesh;
	CDynamicMeshDX8 m_DynamicFlexMesh;

	// The current dynamic vertex buffer
	CVertexBufferDx8 m_DynamicVertexBuffer;

	// The current dynamic index buffer
	CIndexBufferDx8 m_DynamicIndexBuffer;

	// The dynamic mesh temp version (for shaders that modify vertex data)
	CTempMeshDX8 m_DynamicTempMesh;

	// Am I buffering or not?
	bool m_BufferedMode;

	// Using fat vertices?
	bool m_bUseFatVertices;

	CVertexBufferDx8 *m_pCurrentVertexBuffer;
	VertexFormat_t m_CurrentVertexFormat;
	int m_pVertexBufferOffset[MAX_DX8_STREAMS];
	int m_pCurrentVertexStride[MAX_DX8_STREAMS];
	int m_pFirstVertex[MAX_DX8_STREAMS];
	int m_pVertexCount[MAX_DX8_STREAMS];
	CIndexBufferBase *m_pCurrentIndexBuffer;
	int m_nIndexBufferOffset;
	MaterialPrimitiveType_t m_PrimitiveType;
	int m_nFirstIndex;
	int m_nNumIndices;

	unsigned int m_nUnusedVertexFields;
	unsigned int m_nUnusedTextureCoords;

	// 4096 byte static VB containing all-zeros
	IDirect3DVertexBuffer9 *m_pZeroVertexBuffer;
};

//-----------------------------------------------------------------------------
// Singleton...
//-----------------------------------------------------------------------------
static CMeshMgr g_MeshMgr;
IMeshMgr* MeshMgr()
{
	return &g_MeshMgr;
}

//-----------------------------------------------------------------------------
// Tracks stream state and queued data
//-----------------------------------------------------------------------------
static CIndexBuffer *g_pLastIndex = NULL;
static IDirect3DIndexBuffer9 *g_pLastIndexBuffer = NULL;
static CVertexBuffer *g_pLastVertex = NULL;
static IDirect3DVertexBuffer9 *g_pLastVertexBuffer = NULL;
static int g_nLastVertOffsetInBytes = 0;
static int g_nLastVertStride = 0;
static int g_LastVertexIdx = -1;
static CMeshDX8 *g_pLastColorMesh = NULL;
static int g_nLastColorMeshVertOffsetInBytes = 0;
static bool g_bUsingVertexID = false;
static bool g_bFlexMeshStreamSet = false;
static VertexFormat_t g_LastVertexFormat = 0;

inline void D3DSetStreamSource( unsigned int streamNumber, IDirect3DVertexBuffer9 *pStreamData,
								unsigned int nVertexOffsetInBytes, unsigned int stride )
{
	Dx9Device()->SetStreamSource( streamNumber, pStreamData, nVertexOffsetInBytes, stride );
}


//-----------------------------------------------------------------------------
// Tracks stream state and queued data
//-----------------------------------------------------------------------------
void Unbind( IDirect3DIndexBuffer9 *pIndexBuffer )
{
#ifdef _X360
	IDirect3DIndexBuffer9 *pBoundBuffer;
	Dx9Device()->GetIndices( &pBoundBuffer );
	if ( pBoundBuffer == pIndexBuffer )
	{
		// xboxissue - cannot lock indexes set in a d3d device, clear possibly set indices
		Dx9Device()->SetIndices( NULL );
		g_pLastIndex = NULL;
		g_pLastIndexBuffer = NULL;
	}

	if ( pBoundBuffer )
	{
		pBoundBuffer->Release();
	}
#endif
}

void Unbind( IDirect3DVertexBuffer9 *pVertexBuffer )
{
#ifdef _X360
	UINT nOffset, nStride;
	IDirect3DVertexBuffer9 *pBoundBuffer;
	for ( int i = 0; i < MAX_DX8_STREAMS; ++i )
	{
		Dx9Device()->GetStreamSource( i, &pBoundBuffer, &nOffset, &nStride );
		if ( pBoundBuffer == pVertexBuffer )
		{
			// xboxissue - cannot lock indexes set in a d3d device, clear possibly set indices
			Dx9Device()->SetStreamSource( i, 0, 0, 0 );
			switch ( i )
			{
			case 0:
				g_pLastVertex = NULL;
				g_pLastVertexBuffer = NULL;
				break;

			case 1:
				g_pLastColorMesh = NULL;
				g_nLastColorMeshVertOffsetInBytes = 0;
				break;
			}
		}

		if ( pBoundBuffer )
		{
			pBoundBuffer->Release();
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Helpers to count texture coordinates
//-----------------------------------------------------------------------------
static int NumTextureCoordinates( VertexFormat_t vertexFormat )
{
	int nTexCoordCount = 0;
	for ( int i = 0; i < VERTEX_MAX_TEXTURE_COORDINATES; ++i )
	{
		if ( TexCoordSize( i, vertexFormat ) == 0 )
			continue;
		++nTexCoordCount;
	}
	return nTexCoordCount;
}


//-----------------------------------------------------------------------------
// Makes sure that the render state is always set next time
//-----------------------------------------------------------------------------
static void ResetMeshRenderState()
{
	SafeRelease( &g_pLastIndex );
	g_pLastIndexBuffer = 0;
	g_pLastVertex = 0;
	g_nLastVertOffsetInBytes = 0;
	g_pLastColorMesh = 0;
	g_nLastColorMeshVertOffsetInBytes = 0;
	g_LastVertexIdx = -1;
	g_bUsingVertexID = false;
	g_bFlexMeshStreamSet = false;
	g_LastVertexFormat = 0;
}

//-----------------------------------------------------------------------------
// Makes sure that the render state is always set next time
//-----------------------------------------------------------------------------
static void ResetIndexBufferRenderState()
{
	SafeRelease( &g_pLastIndex );
	g_pLastIndexBuffer = 0;
	g_LastVertexIdx = -1;
}


//-----------------------------------------------------------------------------
//
// Index Buffer implementations begin here
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
#ifdef _DEBUG
int CIndexBufferDx8::s_nBufferCount = 0;
#endif


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CIndexBufferDx8::CIndexBufferDx8( ShaderBufferType_t bufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroupName ) :
	BaseClass( pBudgetGroupName )
{
//	Debugger();
	
	Assert( nIndexCount != 0 );

	// NOTE: MATERIAL_INDEX_FORMAT_UNKNOWN can't be dealt with under dx9
	// because format is bound at buffer creation time. What we'll do 
	// is just arbitrarily choose to use a 16-bit index buffer of the same size	
	if ( fmt == MATERIAL_INDEX_FORMAT_UNKNOWN )
	{
		fmt = MATERIAL_INDEX_FORMAT_16BIT;
		nIndexCount /= 2;
	}

	m_pIndexBuffer = NULL;
	m_IndexFormat = fmt;
	m_nBufferSize = nIndexCount * IndexSize();
	m_nIndexCount = nIndexCount;
	m_nFirstUnwrittenOffset = 0;
	m_bIsLocked = false;
	m_bIsDynamic = IsDynamicBufferType( bufferType );
	m_bFlush = false;

#ifdef CHECK_INDICES
	m_pShadowIndices = NULL;
#endif

#ifdef VPROF_ENABLED
	m_nVProfFrame = -1;
#endif
}

CIndexBufferDx8::~CIndexBufferDx8()
{
	Free();
}


//-----------------------------------------------------------------------------
// Returns the index size
//-----------------------------------------------------------------------------
inline int CIndexBufferDx8::IndexSize() const
{
	Assert( m_IndexFormat != MATERIAL_INDEX_FORMAT_UNKNOWN );
	return ( m_IndexFormat == MATERIAL_INDEX_FORMAT_16BIT ) ? 2 : 4;
}


//-----------------------------------------------------------------------------
// Creates, destroys the index buffer
//-----------------------------------------------------------------------------
bool CIndexBufferDx8::Allocate()
{
#ifdef OSX
	Debugger();
#endif
	Assert( !m_pIndexBuffer );
	m_nFirstUnwrittenOffset = 0;

	// FIXME: This doesn't really work for dynamic buffers; dynamic buffers
	// can't have mixed-type indices in them. Bleah.
	D3DFORMAT format = ( m_IndexFormat == MATERIAL_INDEX_FORMAT_32BIT ) ? 
		D3DFMT_INDEX32 : D3DFMT_INDEX16;

	DWORD usage = D3DUSAGE_WRITEONLY;
	if ( m_bIsDynamic )
	{
		usage |= D3DUSAGE_DYNAMIC;
	}

	HRESULT hr = Dx9Device()->CreateIndexBuffer( 
		m_nBufferSize, usage, format, D3DPOOL_DEFAULT, &m_pIndexBuffer, NULL );

#if !defined( _X360 )
	if ( ( hr == D3DERR_OUTOFVIDEOMEMORY ) || ( hr == E_OUTOFMEMORY ) )
	{
		// Don't have the memory for this.  Try flushing all managed resources
		// out of vid mem and try again.
		// FIXME: need to record this
		Dx9Device()->EvictManagedResources();
		hr = Dx9Device()->CreateIndexBuffer( 
			m_nBufferSize, usage, format, D3DPOOL_DEFAULT, &m_pIndexBuffer, NULL );
	}
#endif // !X360

	if ( FAILED(hr) || ( m_pIndexBuffer == NULL ) )
	{
		Warning( "CIndexBufferDx8::Allocate: CreateIndexBuffer failed!\n" );
		return false;
	}

	if ( !m_bIsDynamic )
	{
		VPROF_INCREMENT_GROUP_COUNTER( "TexGroup_global_" TEXTURE_GROUP_STATIC_INDEX_BUFFER, 
			COUNTER_GROUP_TEXTURE_GLOBAL, m_nBufferSize );
	}
	else
	{
		VPROF_INCREMENT_GROUP_COUNTER( "TexGroup_global_" TEXTURE_GROUP_DYNAMIC_INDEX_BUFFER, 
			COUNTER_GROUP_TEXTURE_GLOBAL, m_nBufferSize );
	}

#ifdef CHECK_INDICES
	Assert ( !m_pShadowIndices );
	m_pShadowIndices = new unsigned char[ m_nBufferSize ];
	memset( m_pShadowIndices, 0xFF, m_nBufferSize );
#endif // CHECK_INDICES

#ifdef _DEBUG
	++s_nBufferCount;
#endif

	return true;
}

void CIndexBufferDx8::Free()
{
// FIXME:	Unlock(0);
	if ( m_pIndexBuffer )
	{
#ifdef _DEBUG
		--s_nBufferCount;
#endif

		m_pIndexBuffer->Release();
		m_pIndexBuffer = NULL;

		if ( !m_bIsDynamic )
		{
			VPROF_INCREMENT_GROUP_COUNTER( "TexGroup_global_" TEXTURE_GROUP_STATIC_INDEX_BUFFER, 
				COUNTER_GROUP_TEXTURE_GLOBAL, - m_nBufferSize );
		}
		else
		{
			VPROF_INCREMENT_GROUP_COUNTER( "TexGroup_global_" TEXTURE_GROUP_DYNAMIC_INDEX_BUFFER, 
				COUNTER_GROUP_TEXTURE_GLOBAL, - m_nBufferSize );
		}
	}

#ifdef CHECK_INDICES
	if ( m_pShadowIndices )
	{
		delete[] m_pShadowIndices;
		m_pShadowIndices = NULL;
	}
#endif // CHECK_INDICES
}


//-----------------------------------------------------------------------------
// Index buffer information
//-----------------------------------------------------------------------------
int CIndexBufferDx8::IndexCount( ) const
{
	Assert( !m_bIsDynamic );
	return m_nIndexCount;
}

MaterialIndexFormat_t CIndexBufferDx8::IndexFormat() const
{
	Assert( !m_bIsDynamic );
	return m_IndexFormat;
}


//-----------------------------------------------------------------------------
// Returns true if the buffer is dynamic
//-----------------------------------------------------------------------------
bool CIndexBufferDx8::IsDynamic() const
{
	return m_bIsDynamic;
}


//-----------------------------------------------------------------------------
// Only used by dynamic buffers, indicates the next lock should perform a discard.
//-----------------------------------------------------------------------------
void CIndexBufferDx8::Flush()
{
	// This strange-looking line makes a flush only occur if the buffer is dynamic.
	m_bFlush = m_bIsDynamic;
}


//-----------------------------------------------------------------------------
// Returns the D3D buffer
//-----------------------------------------------------------------------------
IDirect3DIndexBuffer9* CIndexBufferDx8::GetDx9Buffer()
{
	return m_pIndexBuffer;
}


//-----------------------------------------------------------------------------
// Returns a shadowed index, for validation
//-----------------------------------------------------------------------------
#ifdef CHECK_INDICES
unsigned short CIndexBufferDx8::GetShadowIndex( int i ) const
{
	Assert( i >= 0 && i < m_nIndexCount );
	Assert( m_IndexFormat == MATERIAL_INDEX_FORMAT_16BIT );
	return *(unsigned short*)( &m_pShadowIndices[ i * IndexSize() ] );
}
#endif // CHECK_INDICES


//-----------------------------------------------------------------------------
// Used to measure how much static buffer memory is touched each frame 
//-----------------------------------------------------------------------------
void CIndexBufferDx8::HandlePerFrameTextureStats( int nFrame )
{
#ifdef VPROF_ENABLED
	if ( m_nVProfFrame != nFrame && !m_bIsDynamic )
	{
		m_nVProfFrame = nFrame;
		VPROF_INCREMENT_GROUP_COUNTER( "TexGroup_frame_" TEXTURE_GROUP_STATIC_INDEX_BUFFER, 
			COUNTER_GROUP_TEXTURE_PER_FRAME, m_nBufferSize );
	}
#endif
}


//-----------------------------------------------------------------------------
// Casts a dynamic buffer to be a particular index type
//-----------------------------------------------------------------------------
void CIndexBufferDx8::BeginCastBuffer( MaterialIndexFormat_t format )
{
	// NOTE: This should have no effect under Dx9, since we can't recast index buffers.
	Assert( format != MATERIAL_INDEX_FORMAT_UNKNOWN );
	Assert( m_bIsDynamic && ( m_IndexFormat == format ) );
}

void CIndexBufferDx8::EndCastBuffer( )
{
	// NOTE: This should have no effect under Dx9, since we can't recast index buffers.
}

int CIndexBufferDx8::GetRoomRemaining() const
{ 
	return ( m_nBufferSize - m_nFirstUnwrittenOffset ) / IndexSize();
}


//-----------------------------------------------------------------------------
// Locks/unlocks the index buffer
//-----------------------------------------------------------------------------
bool CIndexBufferDx8::Lock( int nMaxIndexCount, bool bAppend, IndexDesc_t &desc )
{
	Assert( !m_bIsLocked && ( nMaxIndexCount != 0 ) && ( nMaxIndexCount <= m_nIndexCount ) );
	Assert( m_IndexFormat != MATERIAL_INDEX_FORMAT_UNKNOWN );

	// FIXME: Why do we need to sync matrices now?
	ShaderUtil()->SyncMatrices();
	g_ShaderMutex.Lock();

	VPROF( "CIndexBufferX8::Lock" );		

	void *pLockedData = NULL;
	HRESULT hr;
	int nMemoryRequired;
	bool bHasEnoughMemory;
	UINT nLockFlags;

	// This can happen if the buffer was locked but a type wasn't bound
	if ( m_IndexFormat == MATERIAL_INDEX_FORMAT_UNKNOWN )
		goto indexBufferLockFailed;

	// Just give the app crap buffers to fill up while we're suppressed...
	if ( g_pShaderDeviceDx8->IsDeactivated() || ( nMaxIndexCount == 0 ) )
		goto indexBufferLockFailed;

	// Did we ask for something too large?
	if ( nMaxIndexCount > m_nIndexCount )
	{
		Warning( "Too many indices for index buffer. . tell a programmer (%d>%d)\n", nMaxIndexCount, m_nIndexCount );
		goto indexBufferLockFailed;
	}

	// We might not have a buffer owing to alt-tab type stuff
	if ( !m_pIndexBuffer )
	{
		if ( !Allocate() )
			goto indexBufferLockFailed;
	}

	// Check to see if we have enough memory 
	nMemoryRequired = nMaxIndexCount * IndexSize();
	bHasEnoughMemory = ( m_nFirstUnwrittenOffset + nMemoryRequired <= m_nBufferSize );

	nLockFlags = D3DLOCK_NOSYSLOCK;
	if ( bAppend )
	{
		// Can't have the first lock after a flush be an appending lock
		Assert( !m_bFlush );

		// If we're appending and we don't have enough room, then puke!
		if ( !bHasEnoughMemory || m_bFlush )
			goto indexBufferLockFailed;
		nLockFlags |= ( m_nFirstUnwrittenOffset == 0 ) ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE;
	}
	else
	{
		// If we're not appending, no overwrite unless we don't have enough room
		// If we're a static buffer, always discard if we're not appending
		if ( !m_bFlush && bHasEnoughMemory && m_bIsDynamic )
		{
			nLockFlags |= ( m_nFirstUnwrittenOffset == 0 ) ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE;
		}
		else
		{
			if ( m_bIsDynamic )
			{
				nLockFlags |= D3DLOCK_DISCARD;
			}
			m_nFirstUnwrittenOffset = 0;
			m_bFlush = false;
		}
	}

#if !defined( _X360 )
	hr = m_pIndexBuffer->Lock(  m_nFirstUnwrittenOffset, nMemoryRequired, &pLockedData, nLockFlags );
#else
	hr = m_pIndexBuffer->Lock( 0, 0, &pLockedData, nLockFlags );
	pLockedData = ( ( unsigned char * )pLockedData + m_nFirstUnwrittenOffset );
#endif

	if ( FAILED( hr ) )
	{
		Warning( "Failed to lock index buffer in CIndexBufferDx8::LockIndexBuffer\n" );
		goto indexBufferLockFailed;
	}

	desc.m_pIndices = (unsigned short*)( pLockedData );
	desc.m_nIndexSize = IndexSize() >> 1;
	if ( g_pHardwareConfig->SupportsStreamOffset() )
	{
		desc.m_nFirstIndex = 0;
		desc.m_nOffset = m_nFirstUnwrittenOffset;
	}
	else
	{
		desc.m_nFirstIndex = m_nFirstUnwrittenOffset / IndexSize();
		Assert( (int)( desc.m_nFirstIndex * IndexSize() ) == m_nFirstUnwrittenOffset );
		desc.m_nOffset = 0;
	}
	m_bIsLocked = true;

#ifdef CHECK_INDICES
	m_nLockIndexBufferSize = nMemoryRequired;
	m_pLockIndexBuffer = desc.m_pIndices;
	m_nLockIndexOffset = m_nFirstUnwrittenOffset;
#endif // CHECK_INDICES

	return true;

indexBufferLockFailed:
	g_ShaderMutex.Unlock();

	// Set up a bogus index descriptor
	desc.m_pIndices = g_nScratchIndexBuffer;
	desc.m_nIndexSize = 0;
	desc.m_nFirstIndex = 0;
	desc.m_nOffset = 0;
	return false;
}

void CIndexBufferDx8::Unlock( int nWrittenIndexCount, IndexDesc_t &desc )
{
	Assert( nWrittenIndexCount <= m_nIndexCount );

	// NOTE: This can happen if another application finishes
	// initializing during the construction of a mesh
	if ( !m_bIsLocked )
		return;

#ifdef CHECK_INDICES
	memcpy( (unsigned char*)m_pShadowIndices + m_nLockIndexOffset, m_pLockIndexBuffer, nWrittenIndexCount * IndexSize() );
#endif // CHECK_INDICES

	if ( m_pIndexBuffer )
	{
		m_pIndexBuffer->Unlock();
	}

	m_nFirstUnwrittenOffset += nWrittenIndexCount * IndexSize();
	m_bIsLocked = false;
	g_ShaderMutex.Unlock();
}


//-----------------------------------------------------------------------------
//
// Vertex Buffer implementations begin here
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------
#ifdef _DEBUG
int CVertexBufferDx8::s_nBufferCount = 0;
#endif


//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
CVertexBufferDx8::CVertexBufferDx8( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroupName ) : 
	BaseClass( pBudgetGroupName )
{
//	Debugger();
	Assert( nVertexCount != 0 );

	m_pVertexBuffer = NULL;
	m_VertexFormat = fmt;
	m_nVertexCount = ( fmt == VERTEX_FORMAT_UNKNOWN ) ? 0 : nVertexCount;
	m_nBufferSize = ( fmt == VERTEX_FORMAT_UNKNOWN ) ? nVertexCount : nVertexCount * VertexSize();
	m_nFirstUnwrittenOffset = 0;
	m_bIsLocked = false;
	m_bIsDynamic = ( type == SHADER_BUFFER_TYPE_DYNAMIC ) || ( type == SHADER_BUFFER_TYPE_DYNAMIC_TEMP );
	m_bFlush = false;

#ifdef VPROF_ENABLED
	if ( !m_bIsDynamic )
	{
		char name[256];
		V_strcpy_safe( name, "TexGroup_global_" );
		V_strcat_safe( name, pBudgetGroupName, sizeof(name) );
		m_pGlobalCounter = g_VProfCurrentProfile.FindOrCreateCounter( name, COUNTER_GROUP_TEXTURE_GLOBAL );

		V_strcpy_safe( name, "TexGroup_frame_" );
		V_strcat_safe( name, pBudgetGroupName, sizeof(name) );
		m_pFrameCounter = g_VProfCurrentProfile.FindOrCreateCounter( name, COUNTER_GROUP_TEXTURE_PER_FRAME );
	}
	else
	{
		m_pGlobalCounter = g_VProfCurrentProfile.FindOrCreateCounter( "TexGroup_global_" TEXTURE_GROUP_DYNAMIC_VERTEX_BUFFER, COUNTER_GROUP_TEXTURE_GLOBAL );
		m_pFrameCounter = NULL;
	}
	m_nVProfFrame = -1;
#endif
}

CVertexBufferDx8::~CVertexBufferDx8()
{
	Free();
}


//-----------------------------------------------------------------------------
// Returns the vertex size
//-----------------------------------------------------------------------------
inline int CVertexBufferDx8::VertexSize() const
{
	Assert( m_VertexFormat != VERTEX_FORMAT_UNKNOWN );
	return VertexFormatSize( m_VertexFormat );
}

//-----------------------------------------------------------------------------
// Creates, destroys the vertex buffer
//-----------------------------------------------------------------------------
bool CVertexBufferDx8::Allocate()
{
	Assert( !m_pVertexBuffer );
	m_nFirstUnwrittenOffset = 0;

	D3DPOOL pool = D3DPOOL_MANAGED;

#if defined(IS_WINDOWS_PC) && defined(SHADERAPIDX9)
	extern bool g_ShaderDeviceUsingD3D9Ex;
	if ( g_ShaderDeviceUsingD3D9Ex )
	{
		pool = D3DPOOL_DEFAULT;
	}
#endif

	DWORD usage = D3DUSAGE_WRITEONLY;
	if ( m_bIsDynamic )
	{
		usage |= D3DUSAGE_DYNAMIC;
		pool = D3DPOOL_DEFAULT;
		// Dynamic meshes should never be compressed (slows down writing to them)
		Assert( CompressionType( GetVertexFormat() ) == VERTEX_COMPRESSION_NONE );
	}

	HRESULT hr = Dx9Device()->CreateVertexBuffer( 
		m_nBufferSize, usage, 0, pool, &m_pVertexBuffer, NULL );

#if !defined( _X360 )
	if ( ( hr == D3DERR_OUTOFVIDEOMEMORY ) || ( hr == E_OUTOFMEMORY ) )
	{
		// Don't have the memory for this.  Try flushing all managed resources
		// out of vid mem and try again.
		// FIXME: need to record this
		Dx9Device()->EvictManagedResources();
		hr = Dx9Device()->CreateVertexBuffer( 
			m_nBufferSize, usage, 0, pool, &m_pVertexBuffer, NULL );
	}
#endif // !X360

	if ( FAILED(hr) || ( m_pVertexBuffer == NULL ) )
	{
		Warning( "CVertexBufferDx8::Allocate: CreateVertexBuffer failed!\n" );
		return false;
	}

	// Track VB allocations
	g_VBAllocTracker->CountVB( m_pVertexBuffer, m_bIsDynamic, m_nBufferSize, VertexSize(), GetVertexFormat() );

#ifdef VPROF_ENABLED
	if ( IsX360() || !m_bIsDynamic )
	{
		Assert( m_pGlobalCounter );
		*m_pGlobalCounter += m_nBufferSize;
	}
#endif

#ifdef _DEBUG
		++s_nBufferCount;
#endif

	return true;
}

void CVertexBufferDx8::Free()
{
	// FIXME:	Unlock(0);
	if ( m_pVertexBuffer )
	{
#ifdef _DEBUG
		--s_nBufferCount;
#endif

	// Track VB allocations
	g_VBAllocTracker->UnCountVB( m_pVertexBuffer );

#ifdef VPROF_ENABLED
		if ( IsX360() || !m_bIsDynamic )
		{
			Assert( m_pGlobalCounter );
			*m_pGlobalCounter -= m_nBufferSize;
		}
#endif

		m_pVertexBuffer->Release();
		m_pVertexBuffer = NULL;
	}
}


//-----------------------------------------------------------------------------
// Vertex buffer information
//-----------------------------------------------------------------------------
int CVertexBufferDx8::VertexCount() const
{
	Assert( !m_bIsDynamic );
	return m_nVertexCount;
}

VertexFormat_t CVertexBufferDx8::GetVertexFormat() const
{
	Assert( !m_bIsDynamic );
	return m_VertexFormat;
}


//-----------------------------------------------------------------------------
// Returns true if the buffer is dynamic
//-----------------------------------------------------------------------------
bool CVertexBufferDx8::IsDynamic() const
{
	return m_bIsDynamic;
}


//-----------------------------------------------------------------------------
// Only used by dynamic buffers, indicates the next lock should perform a discard.
//-----------------------------------------------------------------------------
void CVertexBufferDx8::Flush()
{
	// This strange-looking line makes a flush only occur if the buffer is dynamic.
	m_bFlush = m_bIsDynamic;
}


//-----------------------------------------------------------------------------
// Returns the D3D buffer
//-----------------------------------------------------------------------------
IDirect3DVertexBuffer9* CVertexBufferDx8::GetDx9Buffer()
{
	return m_pVertexBuffer;
}


//-----------------------------------------------------------------------------
// Casts a dynamic buffer to be a particular vertex type
//-----------------------------------------------------------------------------
void CVertexBufferDx8::BeginCastBuffer( VertexFormat_t format )
{
	Assert( format != MATERIAL_INDEX_FORMAT_UNKNOWN );
	Assert( m_bIsDynamic && ( m_VertexFormat == 0 || m_VertexFormat == format ) );
	if ( !m_bIsDynamic )
		return;

	m_VertexFormat = format;
	int nVertexSize = VertexSize();
	m_nVertexCount = m_nBufferSize / nVertexSize;

	// snap current position up to the next position based on expected size
	// so append can safely guarantee nooverwrite regardless of a format growth or shrinkage 
	if ( !g_pHardwareConfig->SupportsStreamOffset() )
	{
		m_nFirstUnwrittenOffset = ( m_nFirstUnwrittenOffset + nVertexSize - 1 ) / nVertexSize;
		m_nFirstUnwrittenOffset *= nVertexSize;
		if ( m_nFirstUnwrittenOffset > m_nBufferSize )
		{
			m_nFirstUnwrittenOffset = m_nBufferSize;
		}
	}
}

void CVertexBufferDx8::EndCastBuffer( )
{
	Assert( m_bIsDynamic && m_VertexFormat != 0 );
	if ( !m_bIsDynamic )
		return;
	m_VertexFormat = 0;
	m_nVertexCount = 0;
}


//-----------------------------------------------------------------------------
// Returns the number of vertices we can still write into the buffer
//-----------------------------------------------------------------------------
int CVertexBufferDx8::GetRoomRemaining() const
{ 
	return ( m_nBufferSize - m_nFirstUnwrittenOffset ) / VertexSize();
}


//-----------------------------------------------------------------------------
// Locks/unlocks the vertex buffer mesh
//-----------------------------------------------------------------------------
bool CVertexBufferDx8::Lock( int nMaxVertexCount, bool bAppend, VertexDesc_t &desc )
{
	Assert( !m_bIsLocked && ( nMaxVertexCount != 0 ) && ( nMaxVertexCount <= m_nVertexCount ) );
	Assert( m_VertexFormat != VERTEX_FORMAT_UNKNOWN );

	// FIXME: Why do we need to sync matrices now?
	ShaderUtil()->SyncMatrices();
	g_ShaderMutex.Lock();

	VPROF( "CVertexBufferDx8::Lock" );		

	void *pLockedData = NULL;
	HRESULT hr;
	int nMemoryRequired;
	bool bHasEnoughMemory;
	UINT nLockFlags;

	// This can happen if the buffer was locked but a type wasn't bound
	if ( m_VertexFormat == VERTEX_FORMAT_UNKNOWN )
		goto vertexBufferLockFailed;

	// Just give the app crap buffers to fill up while we're suppressed...
	if ( g_pShaderDeviceDx8->IsDeactivated() || ( nMaxVertexCount == 0 ) )
		goto vertexBufferLockFailed;

	// Did we ask for something too large?
	if ( nMaxVertexCount > m_nVertexCount )
	{
		Warning( "Too many vertices for vertex buffer. . tell a programmer (%d>%d)\n", nMaxVertexCount, m_nVertexCount );
		goto vertexBufferLockFailed;
	}

	// We might not have a buffer owing to alt-tab type stuff
	if ( !m_pVertexBuffer )
	{
		if ( !Allocate() )
			goto vertexBufferLockFailed;
	}

	// Check to see if we have enough memory 
	nMemoryRequired = nMaxVertexCount * VertexSize();
	bHasEnoughMemory = ( m_nFirstUnwrittenOffset + nMemoryRequired <= m_nBufferSize );

	nLockFlags = D3DLOCK_NOSYSLOCK;
	if ( bAppend )
	{
		// Can't have the first lock after a flush be an appending lock
		Assert( !m_bFlush );

		// If we're appending and we don't have enough room, then puke!
		if ( !bHasEnoughMemory || m_bFlush )
			goto vertexBufferLockFailed;
		nLockFlags |= ( m_nFirstUnwrittenOffset == 0 ) ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE;
	}
	else
	{
		// If we're not appending, no overwrite unless we don't have enough room
		// If we're a static buffer, always discard if we're not appending
		if ( !m_bFlush && bHasEnoughMemory && m_bIsDynamic )
		{
			nLockFlags |= ( m_nFirstUnwrittenOffset == 0 ) ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE;
		}
		else
		{
			if ( m_bIsDynamic )
			{
				nLockFlags |= D3DLOCK_DISCARD;
			}
			m_nFirstUnwrittenOffset = 0;
			m_bFlush = false;
		}
	}

#if !defined( _X360 )
	hr = m_pVertexBuffer->Lock( m_nFirstUnwrittenOffset, nMemoryRequired, &pLockedData, nLockFlags );
#else
	hr = m_pVertexBuffer->Lock( 0, 0, &pLockedData, nLockFlags );
	pLockedData = (unsigned char*)pLockedData + m_nFirstUnwrittenOffset;
#endif

	if ( FAILED( hr ) )
	{
		// Check if paged pool is in critical state ( < 5% free )
		PAGED_POOL_INFO_t ppi;
		if ( ( SYSCALL_SUCCESS == Plat_GetPagedPoolInfo( &ppi ) ) &&
			( ( ppi.numPagesFree * 20 ) < ( ppi.numPagesUsed + ppi.numPagesFree ) ) )
		{
			Error( "Out of OS Paged Pool Memory! For more information, please see\nhttp://support.steampowered.com/cgi-bin/steampowered.cfg/php/enduser/std_adp.php?p_faqid=150\n" );
		}
		else
		{
			Warning( "Failed to lock vertex buffer in CVertexBufferDx8::Lock\n" );
		}
		goto vertexBufferLockFailed;
	}

	ComputeVertexDescription( (unsigned char*)pLockedData, m_VertexFormat, desc );
	if ( g_pHardwareConfig->SupportsStreamOffset() )
	{
		desc.m_nFirstVertex = 0;
		desc.m_nOffset = m_nFirstUnwrittenOffset;
	}
	else
	{
		desc.m_nFirstVertex = m_nFirstUnwrittenOffset / VertexSize();
		desc.m_nOffset = 0;
		Assert( m_nFirstUnwrittenOffset == VertexSize() * desc.m_nFirstVertex );
	}
	m_bIsLocked = true;
	return true;

vertexBufferLockFailed:
	ComputeVertexDescription( 0, 0, desc );
	desc.m_nFirstVertex = 0;
	desc.m_nOffset = 0; 
	return false;
}


void CVertexBufferDx8::Unlock( int nWrittenVertexCount, VertexDesc_t &desc )
{
	Assert( nWrittenVertexCount <= m_nVertexCount );

	// NOTE: This can happen if another application finishes
	// initializing during the construction of a mesh
	if ( !m_bIsLocked )
		return;

	if ( m_pVertexBuffer )
	{
		m_pVertexBuffer->Unlock();
	}

	m_nFirstUnwrittenOffset += nWrittenVertexCount * VertexSize();
	m_bIsLocked = false;
	g_ShaderMutex.Unlock();
}


//-----------------------------------------------------------------------------
// Used to measure how much static buffer memory is touched each frame 
//-----------------------------------------------------------------------------
void CVertexBufferDx8::HandlePerFrameTextureStats( int nFrame )
{
#ifdef VPROF_ENABLED
	if ( m_nVProfFrame != nFrame && !m_bIsDynamic )
	{
		m_nVProfFrame = nFrame;
		m_pFrameCounter += m_nBufferSize;
	}
#endif
}


//-----------------------------------------------------------------------------
// Helpers with meshdescs...
//-----------------------------------------------------------------------------
// FIXME: add compression-agnostic read-accessors (which decompress and return by value, checking desc.m_CompressionType)
inline D3DXVECTOR3 &Position( MeshDesc_t const &desc, int vert )
{
	return *(D3DXVECTOR3*)((unsigned char*)desc.m_pPosition + vert * desc.m_VertexSize_Position );
}

inline float Wrinkle( MeshDesc_t const &desc, int vert )
{
	return *(float*)((unsigned char*)desc.m_pWrinkle + vert * desc.m_VertexSize_Wrinkle );
}

inline D3DXVECTOR3 &BoneWeight( MeshDesc_t const &desc, int vert )
{
	Assert( desc.m_CompressionType == VERTEX_COMPRESSION_NONE );
	return *(D3DXVECTOR3*)((unsigned char*)desc.m_pBoneWeight + vert * desc.m_VertexSize_BoneWeight );
}

inline unsigned char *BoneIndex( MeshDesc_t const &desc, int vert )
{
	return desc.m_pBoneMatrixIndex + vert * desc.m_VertexSize_BoneMatrixIndex;
}

inline D3DXVECTOR3 &Normal( MeshDesc_t const &desc, int vert )
{
	Assert( desc.m_CompressionType == VERTEX_COMPRESSION_NONE );
	return *(D3DXVECTOR3*)((unsigned char*)desc.m_pNormal + vert * desc.m_VertexSize_Normal );
}

inline unsigned char *Color( MeshDesc_t const &desc, int vert )
{
	return desc.m_pColor + vert * desc.m_VertexSize_Color;
}

inline D3DXVECTOR2 &TexCoord( MeshDesc_t const &desc, int vert, int stage )
{
	return *(D3DXVECTOR2*)((unsigned char*)desc.m_pTexCoord[stage] + vert * desc.m_VertexSize_TexCoord[stage] );
}

inline D3DXVECTOR3 &TangentS( MeshDesc_t const &desc, int vert )
{
	return *(D3DXVECTOR3*)((unsigned char*)desc.m_pTangentS + vert * desc.m_VertexSize_TangentS );
}

inline D3DXVECTOR3 &TangentT( MeshDesc_t const &desc, int vert )
{
	return *(D3DXVECTOR3*)((unsigned char*)desc.m_pTangentT + vert * desc.m_VertexSize_TangentT );
}


//-----------------------------------------------------------------------------
//
// Base mesh
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------

CBaseMeshDX8::CBaseMeshDX8() : m_VertexFormat(0)
{
	m_bMeshLocked = false;
#ifdef DBGFLAG_ASSERT
	m_IsDrawing = false;
	m_pMaterial = 0;
#endif
}

CBaseMeshDX8::~CBaseMeshDX8()
{			   
}


//-----------------------------------------------------------------------------
// For debugging...
//-----------------------------------------------------------------------------
bool CBaseMeshDX8::DebugTrace() const
{
#ifdef _DEBUG
	if (m_pMaterial)
		return m_pMaterial->PerformDebugTrace();
#endif

	return false;
}

void CBaseMeshDX8::SetMaterial( IMaterial *pMaterial )
{
#ifdef DBGFLAG_ASSERT
	m_pMaterial = static_cast<IMaterialInternal *>(pMaterial);
#endif
}


//-----------------------------------------------------------------------------
// Sets, gets the vertex format
//-----------------------------------------------------------------------------
void CBaseMeshDX8::SetVertexFormat( VertexFormat_t format )
{
	m_VertexFormat = format;
}

VertexFormat_t CBaseMeshDX8::GetVertexFormat() const
{
	return m_VertexFormat;
}


//-----------------------------------------------------------------------------
// Sets/gets the morph format
//-----------------------------------------------------------------------------
void CBaseMeshDX8::SetMorphFormat( MorphFormat_t format )
{
	m_MorphFormat = format;
}

MorphFormat_t CBaseMeshDX8::GetMorphFormat() const
{
	return m_MorphFormat;
}


//-----------------------------------------------------------------------------
// Am I using morph data?
//-----------------------------------------------------------------------------
bool CBaseMeshDX8::IsUsingMorphData() const
{
	LOCK_SHADERAPI();
	// We're not using a morph unless the bound morph is a superset of what the rendermesh needs
	MorphFormat_t morphFormat = GetMorphFormat();
	if ( !morphFormat )
		return false;

	return ( ( morphFormat & ShaderUtil()->GetBoundMorphFormat() ) == morphFormat );
}

//-----------------------------------------------------------------------------
// Do I need to reset the vertex format?
//-----------------------------------------------------------------------------
bool CBaseMeshDX8::NeedsVertexFormatReset( VertexFormat_t fmt ) const
{
	return m_VertexFormat != fmt;
}


//-----------------------------------------------------------------------------
// Do I have enough room?
//-----------------------------------------------------------------------------
bool CBaseMeshDX8::HasEnoughRoom( int nVertexCount, int nIndexCount ) const
{
	// by default, we do
	return true;
}

//-----------------------------------------------------------------------------
// Estimate the memory used
//-----------------------------------------------------------------------------
unsigned CBaseMeshDX8::ComputeMemoryUsed()
{
	unsigned size = 0;

	if ( GetVertexBuffer() )
	{
		size += GetVertexBuffer()->VertexCount() * GetVertexBuffer()->VertexSize();
	}

	if ( GetIndexBuffer() )
	{
		size += GetIndexBuffer()->IndexCount() * GetIndexBuffer()->IndexSize();
	}

	return size;
}


//-----------------------------------------------------------------------------
// Locks mesh for modifying
//-----------------------------------------------------------------------------
void CBaseMeshDX8::ModifyBeginEx( bool bReadOnly, int nFirstVertex, int nVertexCount, int nFirstIndex, int nIndexCount, MeshDesc_t& desc )
{
	LOCK_SHADERAPI();
	// for the time being, disallow for most cases
	Assert(0);
}

void CBaseMeshDX8::ModifyBegin( int nFirstVertex, int nVertexCount, int nFirstIndex, int nIndexCount, MeshDesc_t& desc )
{
	LOCK_SHADERAPI();
	// for the time being, disallow for most cases
	Assert(0);
}

void CBaseMeshDX8::ModifyEnd( MeshDesc_t& desc )
{
	LOCK_SHADERAPI();
	// for the time being, disallow for most cases
	Assert(0);
}


//-----------------------------------------------------------------------------
// Begins a pass
//-----------------------------------------------------------------------------
void CBaseMeshDX8::BeginPass( )
{
	LOCK_SHADERAPI();
}


//-----------------------------------------------------------------------------
// Sets the render state and gets the drawing going
//-----------------------------------------------------------------------------
inline void CBaseMeshDX8::DrawMesh( )
{
#ifdef DBGFLAG_ASSERT
	// Make sure we're not drawing...
	Assert( !m_IsDrawing );
	m_IsDrawing = true;
#endif

	// This is going to cause RenderPass to get called a bunch
	ShaderAPI()->DrawMesh( this );

#ifdef DBGFLAG_ASSERT
	m_IsDrawing = false;
#endif
}


//-----------------------------------------------------------------------------
// Spews the mesh data
//-----------------------------------------------------------------------------
void CBaseMeshDX8::Spew( int nVertexCount, int nIndexCount, const MeshDesc_t &spewDesc )
{
	LOCK_SHADERAPI();
	// This has regressed.
	int i;


	// FIXME: just fall back to the base class (CVertexBufferBase) version of this function!


#ifdef _DEBUG
	if( m_pMaterial )
	{
		Plat_DebugString( ( const char * )m_pMaterial->GetName() );
		Plat_DebugString( "\n" );
	}
#endif // _DEBUG
	
	// This is needed so buffering can just use this
	VertexFormat_t fmt = m_VertexFormat;

	// Set up the vertex descriptor
	MeshDesc_t desc = spewDesc;

	char tempbuf[256];
	char* temp = tempbuf;
	sprintf( tempbuf,"\nVerts: (Vertex Format %llx)\n", fmt);
	Plat_DebugString(tempbuf);

	CVertexBufferBase::PrintVertexFormat( fmt );

	int numBoneWeights = NumBoneWeights( fmt );
	for ( i = 0; i < nVertexCount; ++i )
	{
		temp += sprintf( temp, "[%4d] ", i + desc.m_nFirstVertex );
		if( fmt & VERTEX_POSITION )
		{
			D3DXVECTOR3& pos = Position( desc, i );
			temp += sprintf(temp, "P %8.2f %8.2f %8.2f ",
				pos[0], pos[1], pos[2]);
		}

		if ( fmt & VERTEX_WRINKLE )
		{
			float flWrinkle = Wrinkle( desc, i );
			temp += sprintf(temp, "Wr %8.2f ",flWrinkle );
		}

		if (numBoneWeights > 0)
		{
			temp += sprintf(temp, "BW ");
			float* pWeight = BoneWeight( desc, i );
			for (int j = 0; j < numBoneWeights; ++j)
			{
				temp += sprintf(temp, "%1.2f ", pWeight[j]);
			}
		}
		if ( fmt & VERTEX_BONE_INDEX )
		{
			unsigned char *pIndex = BoneIndex( desc, i );
			temp += sprintf( temp, "BI %d %d %d %d ", ( int )pIndex[0], ( int )pIndex[1], ( int )pIndex[2], ( int )pIndex[3] );
			Assert( pIndex[0] < 16 );
			Assert( pIndex[1] < 16 );
			Assert( pIndex[2] < 16 );
			Assert( pIndex[3] < 16 );
		}

		if ( fmt & VERTEX_NORMAL )
		{
			D3DXVECTOR3& normal = Normal( desc, i );
			temp += sprintf(temp, "N %1.2f %1.2f %1.2f ",
				normal[0],	normal[1],	normal[2]);
		}
		
		if (fmt & VERTEX_COLOR)
		{
			unsigned char* pColor = Color( desc, i );
			temp += sprintf(temp, "C b %3d g %3d r %3d a %3d ",
				pColor[0], pColor[1], pColor[2], pColor[3]);
		}

		for (int j = 0; j < VERTEX_MAX_TEXTURE_COORDINATES; ++j)
		{
			if( TexCoordSize( j, fmt ) > 0)
			{
				D3DXVECTOR2& texcoord = TexCoord( desc, i, j );
				temp += sprintf(temp, "T%d %.2f %.2f ", j,texcoord[0], texcoord[1]);
			}
		}

		if (fmt & VERTEX_TANGENT_S)
		{
			D3DXVECTOR3& tangentS = TangentS( desc, i );
			temp += sprintf(temp, "S %1.2f %1.2f %1.2f ",
				tangentS[0], tangentS[1], tangentS[2]);
		}

		if (fmt & VERTEX_TANGENT_T)
		{
			D3DXVECTOR3& tangentT = TangentT( desc, i );
			temp += sprintf(temp, "T %1.2f %1.2f %1.2f ",
				tangentT[0], tangentT[1], tangentT[2]);
		}

		sprintf(temp,"\n");
		Plat_DebugString(tempbuf);
		temp = tempbuf;
	}

	sprintf( tempbuf,"\nIndices: %d\n", nIndexCount );
	Plat_DebugString(tempbuf);
	for ( i = 0; i < nIndexCount; ++i )
	{
		temp += sprintf( temp, "%d ", ( int )desc.m_pIndices[i] );
		if ((i & 0x0F) == 0x0F)
		{
			sprintf( temp, "\n" );
			Plat_DebugString(tempbuf);
			tempbuf[0] = '\0';
			temp = tempbuf;
		}
	}
	sprintf(temp,"\n");
	Plat_DebugString( tempbuf );
}

void CBaseMeshDX8::ValidateData( int nVertexCount, int nIndexCount, const MeshDesc_t &spewDesc )
{
	LOCK_SHADERAPI();
#ifdef VALIDATE_DEBUG
	int i;


	// FIXME: just fall back to the base class (CVertexBufferBase) version of this function!


	// This is needed so buffering can just use this
	VertexFormat_t fmt = m_pMaterial->GetVertexUsage();

	// Set up the vertex descriptor
	MeshDesc_t desc = spewDesc;

	int numBoneWeights = NumBoneWeights( fmt );
	for ( i = 0; i < nVertexCount; ++i )
	{
		if( fmt & VERTEX_POSITION )
		{
			D3DXVECTOR3& pos = Position( desc, i );
			Assert( IsFinite( pos[0] ) && IsFinite( pos[1] ) && IsFinite( pos[2] ) );
		}
		if( fmt & VERTEX_WRINKLE )
		{
			float flWrinkle = Wrinkle( desc, i );
			Assert( IsFinite( flWrinkle ) );
		}
		if (numBoneWeights > 0)
		{
			float* pWeight = BoneWeight( desc, i );
			for (int j = 0; j < numBoneWeights; ++j)
			{
				Assert( pWeight[j] >= 0.0f && pWeight[j] <= 1.0f );
			}
		}
		if( fmt & VERTEX_BONE_INDEX )
		{
			unsigned char *pIndex = BoneIndex( desc, i );
			Assert( pIndex[0] >= 0 && pIndex[0] < 16 );
			Assert( pIndex[1] >= 0 && pIndex[1] < 16 );
			Assert( pIndex[2] >= 0 && pIndex[2] < 16 );
			Assert( pIndex[3] >= 0 && pIndex[3] < 16 );
		}
		if( fmt & VERTEX_NORMAL )
		{
			D3DXVECTOR3& normal = Normal( desc, i );
			Assert( normal[0] >= -1.05f && normal[0] <= 1.05f );
			Assert( normal[1] >= -1.05f && normal[1] <= 1.05f );
			Assert( normal[2] >= -1.05f && normal[2] <= 1.05f );
		}
		
		if (fmt & VERTEX_COLOR)
		{
			int* pColor = (int*)Color( desc, i );
			Assert( *pColor != FLOAT32_NAN_BITS );
		}

		for (int j = 0; j < VERTEX_MAX_TEXTURE_COORDINATES; ++j)
		{
			if( TexCoordSize( j, fmt ) > 0)
			{
				D3DXVECTOR2& texcoord = TexCoord( desc, i, j );
				Assert( IsFinite( texcoord[0] ) && IsFinite( texcoord[1] ) );
			}
		}

		if (fmt & VERTEX_TANGENT_S)
		{
			D3DXVECTOR3& tangentS = TangentS( desc, i );
			Assert( IsFinite( tangentS[0] ) && IsFinite( tangentS[1] ) && IsFinite( tangentS[2] ) );
		}

		if (fmt & VERTEX_TANGENT_T)
		{
			D3DXVECTOR3& tangentT = TangentT( desc, i );
			Assert( IsFinite( tangentT[0] ) && IsFinite( tangentT[1] ) && IsFinite( tangentT[2] ) );
		}
	}
#endif // _DEBUG
}

void CBaseMeshDX8::Draw( CPrimList *pLists, int nLists )
{
	LOCK_SHADERAPI();
	Assert( !"CBaseMeshDX8::Draw(CPrimList, int): should never get here." );
}


// Copy verts and/or indices to a mesh builder. This only works for temp meshes!
void CBaseMeshDX8::CopyToMeshBuilder( 
	int iStartVert,		// Which vertices to copy.
	int nVerts, 
	int iStartIndex,	// Which indices to copy.
	int nIndices, 
	int indexOffset,	// This is added to each index.
	CMeshBuilder &builder )
{
	LOCK_SHADERAPI();
	Assert( false );
	Warning( "CopyToMeshBuilder called on something other than a temp mesh.\n" );
}


//-----------------------------------------------------------------------------
//
// static mesh
//
//-----------------------------------------------------------------------------

CPrimList *CMeshDX8::s_pPrims;
int CMeshDX8::s_nPrims;
unsigned int CMeshDX8::s_FirstVertex;
unsigned int CMeshDX8::s_NumVertices;


//-----------------------------------------------------------------------------
// Computes the mode
//-----------------------------------------------------------------------------
inline D3DPRIMITIVETYPE ComputeMode( MaterialPrimitiveType_t type )
{
	switch(type)
	{
#ifdef _X360
	case MATERIAL_INSTANCED_QUADS:
		return D3DPT_QUADLIST;
#endif

	case MATERIAL_POINTS:
		return D3DPT_POINTLIST;
		
	case MATERIAL_LINES:
		return D3DPT_LINELIST;

	case MATERIAL_TRIANGLES:
		return D3DPT_TRIANGLELIST;

	case MATERIAL_TRIANGLE_STRIP:
		return D3DPT_TRIANGLESTRIP;

	// Here, we expect to have the type set later. only works for static meshes
	case MATERIAL_HETEROGENOUS:
		return (D3DPRIMITIVETYPE)-1;

	default:
		Assert(0);
		return (D3DPRIMITIVETYPE)-1;
	}
}

//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
CMeshDX8::CMeshDX8( const char *pTextureGroupName ) : m_NumVertices(0), m_NumIndices(0), m_pVertexBuffer(0),
	m_pColorMesh( 0 ), m_nColorMeshVertOffsetInBytes( 0 ),
	m_pIndexBuffer(0), m_Type(MATERIAL_TRIANGLES), m_IsVBLocked(false),
	m_IsIBLocked(false)
{
	m_pTextureGroupName = pTextureGroupName;
	m_Mode = ComputeMode(m_Type);

	m_bHasFlexVerts = false;
	m_pFlexVertexBuffer = NULL;
	m_nFlexVertOffsetInBytes = 0;
}

CMeshDX8::~CMeshDX8()
{
	// Don't release the vertex buffer 
	if (!g_MeshMgr.IsDynamicMesh(this))
	{
		if (m_pVertexBuffer)
		{
			delete m_pVertexBuffer;
		}
		if (m_pIndexBuffer)
		{
			SafeRelease( &m_pIndexBuffer );
		}
	}
}

void CMeshDX8::SetFlexMesh( IMesh *pMesh, int nVertexOffsetInBytes )
{
	if ( !ShaderUtil()->OnSetFlexMesh( this, pMesh, nVertexOffsetInBytes ) )
		return;

	LOCK_SHADERAPI();
	m_nFlexVertOffsetInBytes = nVertexOffsetInBytes;	// Offset into dynamic mesh (in bytes)

	if ( pMesh )
	{		
		m_flexVertCount = pMesh->VertexCount();
		pMesh->MarkAsDrawn();

		CBaseMeshDX8 *pBaseMesh = static_cast<CBaseMeshDX8 *>(pMesh);
		m_pFlexVertexBuffer = pBaseMesh->GetVertexBuffer();

		m_bHasFlexVerts = true;
	}
	else
	{
		m_flexVertCount = 0;
		m_pFlexVertexBuffer = NULL;
		m_bHasFlexVerts = false;
	}
}

void CMeshDX8::DisableFlexMesh( )
{
	CMeshDX8::SetFlexMesh( NULL, 0 );
}

bool CMeshDX8::HasFlexMesh( ) const
{
	LOCK_SHADERAPI();
	return m_bHasFlexVerts;
}

void CMeshDX8::SetColorMesh( IMesh *pColorMesh, int nVertexOffsetInBytes )
{
	if ( !ShaderUtil()->OnSetColorMesh( this, pColorMesh, nVertexOffsetInBytes ) )
		return;
		
	LOCK_SHADERAPI();
	m_pColorMesh = ( CMeshDX8 * )pColorMesh; // dangerous conversion! garymcthack
	m_nColorMeshVertOffsetInBytes = nVertexOffsetInBytes;
	Assert( m_pColorMesh || ( nVertexOffsetInBytes == 0 ) );

#ifdef _DEBUG
	if ( pColorMesh )
	{
		int nVertexCount = VertexCount();
 		int numVertsColorMesh = m_pColorMesh->VertexCount();
		Assert( numVertsColorMesh >= nVertexCount );
	}
#endif
}


void CMeshDX8::HandleLateCreation( )
{
	if ( m_pVertexBuffer )
	{
		m_pVertexBuffer->HandleLateCreation();
	}
	if ( m_pIndexBuffer )
	{
		m_pIndexBuffer->HandleLateCreation();
	}
	if ( m_pFlexVertexBuffer )
	{
		m_pFlexVertexBuffer->HandleLateCreation();
	}

	if ( m_pColorMesh )
	{
		m_pColorMesh->HandleLateCreation();
	}
}


bool CMeshDX8::HasColorMesh( ) const
{
	LOCK_SHADERAPI();
	return (m_pColorMesh != NULL);
}


//-----------------------------------------------------------------------------
// Locks/ unlocks the vertex buffer
//-----------------------------------------------------------------------------
bool CMeshDX8::Lock( int nVertexCount, bool bAppend, VertexDesc_t &desc )
{
	Assert( !m_IsVBLocked );

	// Just give the app crap buffers to fill up while we're suppressed...
	if ( g_pShaderDeviceDx8->IsDeactivated() || (nVertexCount == 0))
	{
		// Set up the vertex descriptor
		CVertexBufferBase::ComputeVertexDescription( 0, 0, desc );
		desc.m_nFirstVertex = 0;
		return false;
	}

	// Static vertex buffer case
	if (!m_pVertexBuffer)
	{
		int size = g_MeshMgr.VertexFormatSize( m_VertexFormat );
		m_pVertexBuffer = new CVertexBuffer( Dx9Device(), m_VertexFormat, 0, size, nVertexCount, m_pTextureGroupName, ShaderAPI()->UsingSoftwareVertexProcessing() );
	}

	// Lock it baby
	int nMaxVerts, nMaxIndices;
	g_MeshMgr.GetMaxToRender( this, false, &nMaxVerts, &nMaxIndices );
	if ( !g_pHardwareConfig->SupportsStreamOffset() )
	{
		// Without stream offset, we can't use VBs greater than 65535 verts (due to our using 16-bit indices)
		Assert( nVertexCount <= nMaxVerts );
	}

	unsigned char *pVertexMemory = m_pVertexBuffer->Lock( nVertexCount, desc.m_nFirstVertex );
	if ( !pVertexMemory )
	{
		if ( nVertexCount > nMaxVerts )
		{
			Assert( 0 );
			Error( "Too many verts for a dynamic vertex buffer (%d>%d) Tell a programmer to up VERTEX_BUFFER_SIZE.\n", 
				( int )nVertexCount, ( int )nMaxVerts );
		}
		else
		{
			// Check if paged pool is in critical state ( < 5% free )
			PAGED_POOL_INFO_t ppi;
			if ( ( SYSCALL_SUCCESS == Plat_GetPagedPoolInfo( &ppi ) ) &&
				 ( ( ppi.numPagesFree * 20 ) < ( ppi.numPagesUsed + ppi.numPagesFree ) ) )
			{
				Error( "Out of OS Paged Pool Memory! For more information, please see\nhttp://support.steampowered.com/cgi-bin/steampowered.cfg/php/enduser/std_adp.php?p_faqid=150\n" );
			}
			else
			{
				Assert( 0 );
				Error( "failed to lock vertex buffer in CMeshDX8::LockVertexBuffer: nVertexCount=%d, nFirstVertex=%d\n", nVertexCount, desc.m_nFirstVertex );
			}
		}
		CVertexBufferBase::ComputeVertexDescription( 0, 0, desc );
		return false;
	}

	// Set up the vertex descriptor
	CVertexBufferBase::ComputeVertexDescription( pVertexMemory, m_VertexFormat, desc );
	m_IsVBLocked = true;

#ifdef RECORDING
	m_LockVertexBufferSize = nVertexCount * desc.m_ActualVertexSize;
	m_LockVertexBuffer = pVertexMemory;
#endif

	return true;
}

void CMeshDX8::Unlock( int nVertexCount, VertexDesc_t& desc )
{
	// NOTE: This can happen if another application finishes
	// initializing during the construction of a mesh
	if (!m_IsVBLocked)
		return;

	// This is recorded for debugging. . not sent to dx.
	RECORD_COMMAND( DX8_SET_VERTEX_BUFFER_FORMAT, 2 );
	RECORD_INT( m_pVertexBuffer->UID() );
	RECORD_INT( m_VertexFormat );
	
	RECORD_COMMAND( DX8_VERTEX_DATA, 3 );
	RECORD_INT( m_pVertexBuffer->UID() );
	RECORD_INT( m_LockVertexBufferSize );
	RECORD_STRUCT( m_LockVertexBuffer, m_LockVertexBufferSize );

	Assert(m_pVertexBuffer);
	m_pVertexBuffer->Unlock(nVertexCount);
	m_IsVBLocked = false;
}

//-----------------------------------------------------------------------------
// Locks/unlocks the index buffer
//-----------------------------------------------------------------------------
int CMeshDX8::Lock( bool bReadOnly, int nFirstIndex, int nIndexCount, IndexDesc_t &desc )
{
	Assert( !m_IsIBLocked );

	// Just give the app crap buffers to fill up while we're suppressed...
	if ( g_pShaderDeviceDx8->IsDeactivated() || (nIndexCount == 0))
	{
		// Set up a bogus index descriptor
		desc.m_pIndices = g_nScratchIndexBuffer;
		desc.m_nIndexSize = 0;
		return 0;
	}

	// Static vertex buffer case
	if (!m_pIndexBuffer)
	{
		SafeAssign( &m_pIndexBuffer, new CIndexBuffer( Dx9Device(), nIndexCount, ShaderAPI()->UsingSoftwareVertexProcessing() ) );
	}

	int startIndex;
	desc.m_pIndices = m_pIndexBuffer->Lock( bReadOnly, nIndexCount, startIndex, nFirstIndex );
	if( !desc.m_pIndices )
	{
		desc.m_pIndices = g_nScratchIndexBuffer;
		desc.m_nIndexSize = 0;

		// Check if paged pool is in critical state ( < 5% free )
		PAGED_POOL_INFO_t ppi;
		if ( ( SYSCALL_SUCCESS == Plat_GetPagedPoolInfo( &ppi ) ) &&
			( ( ppi.numPagesFree * 20 ) < ( ppi.numPagesUsed + ppi.numPagesFree ) ) )
		{
			Error( "Out of OS Paged Pool Memory! For more information, please see\nhttp://support.steampowered.com/cgi-bin/steampowered.cfg/php/enduser/std_adp.php?p_faqid=150\n" );
		}
		else
		{
			Assert( 0 );
			Error( "failed to lock index buffer in CMeshDX8::LockIndexBuffer\n" );
		}

		return 0;
	}
	
	desc.m_nIndexSize = 1;
	m_IsIBLocked = true;

#if defined( RECORDING ) || defined( CHECK_INDICES )
	m_LockIndexBufferSize = nIndexCount * 2;
	m_LockIndexBuffer = desc.m_pIndices;
#endif

	return startIndex;
}


void CMeshDX8::Unlock( int nIndexCount, IndexDesc_t &desc )
{
	// NOTE: This can happen if another application finishes
	// initializing during the construction of a mesh
	if (!m_IsIBLocked)
		return;

	RECORD_COMMAND( DX8_INDEX_DATA, 3 );
	RECORD_INT( m_pIndexBuffer->UID() );
	RECORD_INT( m_LockIndexBufferSize );
	RECORD_STRUCT( m_LockIndexBuffer, m_LockIndexBufferSize );

	Assert(m_pIndexBuffer);

#ifdef CHECK_INDICES
	m_pIndexBuffer->UpdateShadowIndices( ( unsigned short * )m_LockIndexBuffer );
#endif // CHECK_INDICES
	
	// Unlock, and indicate how many vertices we actually used
	m_pIndexBuffer->Unlock(nIndexCount);
	m_IsIBLocked = false;
}


//-----------------------------------------------------------------------------
// Locks/unlocks the entire mesh
//-----------------------------------------------------------------------------
void CMeshDX8::LockMesh( int nVertexCount, int nIndexCount, MeshDesc_t& desc )
{
	ShaderUtil()->SyncMatrices();

	g_ShaderMutex.Lock();
	VPROF( "CMeshDX8::LockMesh" );		
	Lock( nVertexCount, false, *static_cast<VertexDesc_t*>( &desc ) );
	if ( m_Type != MATERIAL_POINTS )
	{
		Lock( false, -1, nIndexCount, *static_cast<IndexDesc_t*>( &desc ) );
	}
	else
	{
		desc.m_pIndices = g_nScratchIndexBuffer;
		desc.m_nIndexSize = 0;
	}

	CBaseMeshDX8::m_bMeshLocked = true;
}


void CMeshDX8::UnlockMesh( int nVertexCount, int nIndexCount, MeshDesc_t& desc )
{
	VPROF( "CMeshDX8::UnlockMesh" );

	Assert( CBaseMeshDX8::m_bMeshLocked );

	Unlock( nVertexCount, *static_cast<VertexDesc_t*>( &desc ) );
	if ( m_Type != MATERIAL_POINTS )
	{
		Unlock( nIndexCount, *static_cast<IndexDesc_t*>( &desc ) );
	}
																	    
	// The actual # we wrote
	m_NumVertices = nVertexCount;
	m_NumIndices = nIndexCount;

	CBaseMeshDX8::m_bMeshLocked = false;
	g_ShaderMutex.Unlock();
}

 
//-----------------------------------------------------------------------------
// Locks mesh for modifying
//-----------------------------------------------------------------------------
void CMeshDX8::ModifyBeginEx( bool bReadOnly, int nFirstVertex, int nVertexCount, int nFirstIndex, int nIndexCount, MeshDesc_t& desc )
{
	VPROF( "CMeshDX8::ModifyBegin" );

	// Just give the app crap buffers to fill up while we're suppressed...
	if ( g_pShaderDeviceDx8->IsDeactivated())
	{
		// Set up a bogus descriptor
		g_MeshMgr.ComputeVertexDescription( 0, 0, desc );
		desc.m_pIndices = g_nScratchIndexBuffer;
		desc.m_nIndexSize = 0;
		return;
	}

	Assert( m_pVertexBuffer );

	// Lock it baby
	unsigned char* pVertexMemory = m_pVertexBuffer->Modify( bReadOnly, nFirstVertex, nVertexCount );
	if ( pVertexMemory )
	{
		m_IsVBLocked = true;
		g_MeshMgr.ComputeVertexDescription( pVertexMemory, m_VertexFormat, desc );

#ifdef RECORDING
		m_LockVertexBufferSize = nVertexCount * desc.m_ActualVertexSize;
		m_LockVertexBuffer = pVertexMemory;
#endif
	}

	desc.m_nFirstVertex = nFirstVertex;

	Lock( bReadOnly, nFirstIndex, nIndexCount, *static_cast<IndexDesc_t*>( &desc ) );
}

void CMeshDX8::ModifyBegin( int nFirstVertex, int nVertexCount, int nFirstIndex, int nIndexCount, MeshDesc_t& desc )
{
	ModifyBeginEx( false, nFirstVertex, nVertexCount, nFirstIndex, nIndexCount, desc );
}

void CMeshDX8::ModifyEnd( MeshDesc_t& desc )
{
	VPROF( "CMeshDX8::ModifyEnd" );
	Unlock( 0, *static_cast<IndexDesc_t*>( &desc ) );
	Unlock( 0, *static_cast<VertexDesc_t*>( &desc ) );
}


//-----------------------------------------------------------------------------
// returns the # of vertices (static meshes only)
//-----------------------------------------------------------------------------
int CMeshDX8::VertexCount() const
{
	return m_pVertexBuffer ? m_pVertexBuffer->VertexCount() : 0;
}


//-----------------------------------------------------------------------------
// returns the # of indices 
//-----------------------------------------------------------------------------
int CMeshDX8::IndexCount( ) const
{
	return m_pIndexBuffer ? m_pIndexBuffer->IndexCount() : 0;
}


//-----------------------------------------------------------------------------
// Sets up the vertex and index buffers
//-----------------------------------------------------------------------------
void CMeshDX8::UseIndexBuffer( CIndexBuffer* pBuffer )
{
	SafeAssign( &m_pIndexBuffer, pBuffer );
}

void CMeshDX8::UseVertexBuffer( CVertexBuffer* pBuffer )
{
	m_pVertexBuffer = pBuffer;
}


//-----------------------------------------------------------------------------
// Sets the primitive type
//-----------------------------------------------------------------------------
void CMeshDX8::SetPrimitiveType( MaterialPrimitiveType_t type )
{
	Assert( IsX360() || ( type != MATERIAL_INSTANCED_QUADS ) );
	if ( !ShaderUtil()->OnSetPrimitiveType( this, type ) )
	{
		return;
	}

	LOCK_SHADERAPI();
	m_Type = type;
	m_Mode = ComputeMode( type );
}

MaterialPrimitiveType_t CMeshDX8::GetPrimitiveType( ) const
{
	return m_Type;
}


//-----------------------------------------------------------------------------
// Computes the number of primitives we're gonna draw
//-----------------------------------------------------------------------------
int CMeshDX8::NumPrimitives( int nVertexCount, int nIndexCount ) const
{
	switch(m_Mode)
	{
	case D3DPT_POINTLIST:
		return nVertexCount;
		
	case D3DPT_LINELIST:
		return nIndexCount / 2;

	case D3DPT_TRIANGLELIST:
		return nIndexCount / 3;

	case D3DPT_TRIANGLESTRIP:
		return nIndexCount - 2;

	default:
		// invalid, baby!
		Assert(0);
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Checks if it's a valid format
//-----------------------------------------------------------------------------
#ifdef _DEBUG
static void OutputVertexFormat( VertexFormat_t format )
{
	// FIXME: this is a duplicate of the function in meshdx8.cpp
	VertexCompressionType_t compressionType = CompressionType( format );

	if ( format & VERTEX_POSITION )
	{
		Warning( "VERTEX_POSITION|" );
	}
	if ( format & VERTEX_NORMAL )
	{
		if ( compressionType == VERTEX_COMPRESSION_ON )
			Warning( "VERTEX_NORMAL[COMPRESSED]|" );
		else
			Warning( "VERTEX_NORMAL|" );
	}
	if ( format & VERTEX_COLOR )
	{
		Warning( "VERTEX_COLOR|" );
	}
	if ( format & VERTEX_SPECULAR )
	{
		Warning( "VERTEX_SPECULAR|" );
	}
	if ( format & VERTEX_TANGENT_S )
	{
		Warning( "VERTEX_TANGENT_S|" );
	}
	if ( format & VERTEX_TANGENT_T )
	{
		Warning( "VERTEX_TANGENT_T|" );
	}
	if ( format & VERTEX_BONE_INDEX )
	{
		Warning( "VERTEX_BONE_INDEX|" );
	}
	if ( format & VERTEX_FORMAT_VERTEX_SHADER )
	{
		Warning( "VERTEX_FORMAT_VERTEX_SHADER|" );
	}
	Warning( "\nBone weights: %d (%s)\n", NumBoneWeights( format ),
		( CompressionType( format ) == VERTEX_COMPRESSION_ON ? "compressed" : "uncompressed" ) );
	Warning( "user data size: %d (%s)\n", UserDataSize( format ),
		( CompressionType( format ) == VERTEX_COMPRESSION_ON ? "compressed" : "uncompressed" ) );
	Warning( "num tex coords: %d\n", NumTextureCoordinates( format ) );
	// NOTE: This doesn't print texcoord sizes.
}
#endif

bool CMeshDX8::IsValidVertexFormat( VertexFormat_t vertexFormat )
{
	// FIXME: Make this a debug-only check on say 6th July 2007 (after a week or so's testing)
	//        (i.e. avoid the 360 release build perf. hit for when we ship)
	bool bCheckCompression = ( m_VertexFormat & VERTEX_FORMAT_COMPRESSED ) &&
							( ( vertexFormat == VERTEX_FORMAT_INVALID ) || ( ( vertexFormat & VERTEX_FORMAT_COMPRESSED ) == 0 ) );

	if ( bCheckCompression || IsPC() || IsDebug() )
	{
		IMaterialInternal* pMaterial = ShaderAPI()->GetBoundMaterial();
		Assert( pMaterial );

		// the material format should match the vertex usage, unless another format is passed in
		if ( vertexFormat == VERTEX_FORMAT_INVALID )
		{
			vertexFormat = pMaterial->GetVertexUsage() & ~( VERTEX_FORMAT_VERTEX_SHADER | VERTEX_FORMAT_USE_EXACT_FORMAT );

			// Blat out unused fields
			vertexFormat &= ~g_MeshMgr.UnusedVertexFields();
			int nUnusedTextureCoords = g_MeshMgr.UnusedTextureCoords();
			for ( int i = 0; i < VERTEX_MAX_TEXTURE_COORDINATES; ++i )
			{
				if ( nUnusedTextureCoords & ( 1 << i ) )
				{
					vertexFormat &= ~VERTEX_TEXCOORD_MASK( i );
				}
			}
		}
		else
		{
			vertexFormat &= ~( VERTEX_FORMAT_VERTEX_SHADER | VERTEX_FORMAT_USE_EXACT_FORMAT );
		}

		bool bIsValid = (( VERTEX_FORMAT_FIELD_MASK & vertexFormat ) & ( VERTEX_FORMAT_FIELD_MASK & ~m_VertexFormat )) == 0;

		if ( m_VertexFormat & VERTEX_FORMAT_COMPRESSED )
		{
			// We shouldn't get compressed verts if this material doesn't support them!
			if ( ( vertexFormat & VERTEX_FORMAT_COMPRESSED ) == 0 )
			{
				static int numWarnings = 0;
				if ( numWarnings++ == 0 )
				{
					// NOTE: ComputeVertexFormat() will make sure no materials support VERTEX_FORMAT_COMPRESSED
					//       if vertex compression is disabled in the config
					if ( g_pHardwareConfig->SupportsCompressedVertices() == VERTEX_COMPRESSION_NONE )
						Warning( "ERROR: Compressed vertices in use but vertex compression is disabled (or not supported on this hardware)!\n" );
					else
						Warning( "ERROR: Compressed vertices in use but material does not support them!\n" );
				}
				Assert( 0 );
				bIsValid = false;
			}
		}

		bIsValid = bIsValid && UserDataSize( m_VertexFormat ) >= UserDataSize( vertexFormat );

		for ( int i=0; i < VERTEX_MAX_TEXTURE_COORDINATES; i++ )
		{
			if ( TexCoordSize( i, m_VertexFormat ) < TexCoordSize( i, vertexFormat ) )
			{
				bIsValid = false;
			}
		}
		
		// NOTE: It can totally be valid to have more weights than the current number of bones.
		// The -1 here is because if we have N bones, we can have only (N-1) weights,
		// since the Nth is implied (the weights sum to 1).
		int nWeightCount = NumBoneWeights( m_VertexFormat );
		bIsValid = bIsValid && ( nWeightCount >= ( g_pShaderAPI->GetCurrentNumBones() - 1 ) );

#ifdef _DEBUG
		if ( !bIsValid )
		{
			Warning( "Material Format:" );
			if ( g_pShaderAPI->GetCurrentNumBones() > 0 )
			{
				vertexFormat |= VERTEX_BONE_INDEX;
				vertexFormat &= ~VERTEX_BONE_WEIGHT_MASK;
				vertexFormat |= VERTEX_BONEWEIGHT( 2 );
			}

			OutputVertexFormat( vertexFormat );
			Warning( "Mesh Format:" );
			OutputVertexFormat( m_VertexFormat );
		}
#endif
		return bIsValid;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Stream source setting methods
//-----------------------------------------------------------------------------
void CMeshDX8::SetVertexIDStreamState()
{
	// FIXME: this method duplicates the code in CMeshMgr::SetVertexIDStreamState

	if ( IsX360() )
		return;

	bool bUsingVertexID = IsUsingVertexID();
	if ( bUsingVertexID != g_bUsingVertexID )
	{
		if ( bUsingVertexID )
		{
			// NOTE: Morphing doesn't work with dynamic buffers!!! BLEAH
			// It's because the indices (which are not 0 based for dynamic buffers)
			// are accessing both the vertexID buffer + the regular vertex buffer.
			// This *might* be fixable with baseVertexIndex?

			// NOTE: At the moment, vertex id is only used for hw morphing. I've got it
			// set up so that a shader that supports hw morphing always says it uses vertex id.
			// If we ever use vertex id for something other than hw morphing, we're going
			// to have to revisit how those shaders say they want to use vertex id
			// or  fix this some other way
			Assert( !g_pShaderAPI->IsHWMorphingEnabled() || !m_pVertexBuffer->IsDynamic() );

			CVertexBuffer *pVertexIDBuffer = g_MeshMgr.GetVertexIDBuffer( );
			RECORD_COMMAND( DX8_SET_STREAM_SOURCE, 4 );
			RECORD_INT( pVertexIDBuffer->UID() );
			RECORD_INT( 3 );
			RECORD_INT( 0 );
			RECORD_INT( pVertexIDBuffer->VertexSize() );

			D3DSetStreamSource( 3, pVertexIDBuffer->GetInterface(), 0, pVertexIDBuffer->VertexSize() );
			pVertexIDBuffer->HandlePerFrameTextureStats( ShaderAPI()->GetCurrentFrameCounter() );
		}
		else
		{
			RECORD_COMMAND( DX8_SET_STREAM_SOURCE, 4 );
			RECORD_INT( -1 );	// vertex buffer id
			RECORD_INT( 3 );	// stream
			RECORD_INT( 0 );	// vertex offset
			RECORD_INT( 0 );	// vertex size

			D3DSetStreamSource( 3, 0, 0, 0 );
		}
		g_bUsingVertexID = bUsingVertexID;
	}
}

void CMeshDX8::SetColorStreamState()
{
	if ( ( m_pColorMesh != g_pLastColorMesh ) || ( m_nColorMeshVertOffsetInBytes != g_nLastColorMeshVertOffsetInBytes ) )
	{
		if ( m_pColorMesh )
		{
			RECORD_COMMAND( DX8_SET_STREAM_SOURCE, 4 );
			RECORD_INT( m_pColorMesh->GetVertexBuffer()->UID() );
			RECORD_INT( 1 );
			RECORD_INT( m_nColorMeshVertOffsetInBytes );
			RECORD_INT( m_pColorMesh->GetVertexBuffer()->VertexSize() );

			D3DSetStreamSource( 1, m_pColorMesh->GetVertexBuffer()->GetInterface(), 
								m_nColorMeshVertOffsetInBytes, m_pColorMesh->GetVertexBuffer()->VertexSize() );
			m_pColorMesh->GetVertexBuffer()->HandlePerFrameTextureStats( ShaderAPI()->GetCurrentFrameCounter() );
		}
		else
		{
			RECORD_COMMAND( DX8_SET_STREAM_SOURCE, 4 );
			RECORD_INT( -1 );	// vertex buffer id
			RECORD_INT( 1 );	// stream
			RECORD_INT( 0 );	// vertex offset
			RECORD_INT( 0 );	// vertex size

			D3DSetStreamSource( 1, 0, 0, 0 );
		}
		g_pLastColorMesh = m_pColorMesh;
		g_nLastColorMeshVertOffsetInBytes = m_nColorMeshVertOffsetInBytes;
	}
}

void CMeshDX8::SetVertexStreamState( int nVertOffsetInBytes )
{
	// Calls in here assume shader support...
	if ( HardwareConfig()->SupportsVertexAndPixelShaders() )
	{
		if ( HasFlexMesh() )
		{
			// m_pFlexVertexBuffer is the flex buffer down inside the CMeshMgr singleton
			D3DSetStreamSource( 2, m_pFlexVertexBuffer->GetInterface(), m_nFlexVertOffsetInBytes, m_pFlexVertexBuffer->VertexSize() );

			if ( g_pHardwareConfig->Caps().m_SupportsVertexShaders_2_0 )
			{
				float c[4] = { 1.0f, g_pMaterialSystemHardwareConfig->SupportsPixelShaders_2_b() ? 1.0f : 0.0f, 0.0f, 0.0f };
				ShaderAPI()->SetVertexShaderConstant( VERTEX_SHADER_FLEXSCALE, c, 1 );
			}

			g_bFlexMeshStreamSet = true;
		}
		else
		{
			Assert( nVertOffsetInBytes == 0 );
			Assert( m_pVertexBuffer );

			// HACK...point stream 2 at the same VB which is bound to stream 0...
			// NOTE: D3D debug DLLs will RIP if stream 0 has a smaller stride than the largest
			//       offset in the stream 2 vertex decl elements (which are position(12)+wrinkle(4)+normal(12))
			// If this fires, go find the material/shader which is requesting a really 'thin'
			// stream 0 vertex, and fatten it up slightly (e.g. add a D3DCOLOR element)
			int minimumStreamZeroStride = 4 * sizeof( float );
			Assert( m_pVertexBuffer->VertexSize() >= minimumStreamZeroStride );
			if ( m_pVertexBuffer->VertexSize() < minimumStreamZeroStride )
			{
				static bool bWarned = false;
				if( !bWarned )
				{
					Warning( "Shader specifying too-thin vertex format, should be at least %d bytes! (Suppressing furthur warnings)\n", minimumStreamZeroStride );
					bWarned = true;
				}
			}

			// Set a 4kb all-zero static VB into the flex/wrinkle stream with a stride of 1 bytes, so the vertex shader always reads valid floating point values (otherwise it can get NaN's/Inf's, and under OpenGL this is bad on NVidia)
			// togl requires non-zero strides, but on D3D9 we can set a stride of 0 for a little more efficiency.
			D3DSetStreamSource( 2, g_MeshMgr.GetZeroVertexBuffer(), 0, IsOpenGL() ? 4 : 0 );

			if ( g_pHardwareConfig->Caps().m_SupportsVertexShaders_2_0 )
			{
				float c[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				ShaderAPI()->SetVertexShaderConstant( VERTEX_SHADER_FLEXSCALE, c, 1 );
			}

			g_bFlexMeshStreamSet = false;
		}
	}

	// MESHFIXME: Make sure this jives between the mesh/ib/vb version.
#ifdef _X360
	if ( ( g_pLastVertex != m_pVertexBuffer ) || ( m_pVertexBuffer->IsDynamic() ) || ( g_nLastVertOffsetInBytes != nVertOffsetInBytes ) )
#else
	if ( ( g_pLastVertex != m_pVertexBuffer ) || ( g_nLastVertOffsetInBytes != nVertOffsetInBytes ) )
#endif
	{
		Assert( m_pVertexBuffer );

		RECORD_COMMAND( DX8_SET_STREAM_SOURCE, 4 );
		RECORD_INT( m_pVertexBuffer->UID() );
		RECORD_INT( 0 );
		RECORD_INT( nVertOffsetInBytes );
		RECORD_INT( m_pVertexBuffer->VertexSize() );

		D3DSetStreamSource( 0, m_pVertexBuffer->GetInterface(), nVertOffsetInBytes, m_pVertexBuffer->VertexSize() );
		m_pVertexBuffer->HandlePerFrameTextureStats( ShaderAPI()->GetCurrentFrameCounter() );
				
		g_pLastVertex = m_pVertexBuffer;
		g_nLastVertOffsetInBytes = nVertOffsetInBytes;
	}
}

void CMeshDX8::SetIndexStreamState( int firstVertexIdx )
{
#ifdef _X360
	if ( ( g_pLastIndexBuffer != NULL ) || (g_pLastIndex != m_pIndexBuffer) || ( m_pIndexBuffer->IsDynamic() ) || ( firstVertexIdx != g_LastVertexIdx ) )
#else
	if ( ( g_pLastIndexBuffer != NULL ) || (g_pLastIndex != m_pIndexBuffer) || ( firstVertexIdx != g_LastVertexIdx ) )
#endif
	{
		Assert( m_pIndexBuffer );

		RECORD_COMMAND( DX8_SET_INDICES, 2 );
		RECORD_INT( m_pIndexBuffer->UID() );
		RECORD_INT( firstVertexIdx );

		Dx9Device()->SetIndices( m_pIndexBuffer->GetInterface() );
		m_pIndexBuffer->HandlePerFrameTextureStats( ShaderAPI()->GetCurrentFrameCounter() );
		m_FirstIndex = firstVertexIdx;

		SafeAssign( &g_pLastIndex, m_pIndexBuffer );
		g_pLastIndexBuffer = NULL;
		g_LastVertexIdx = firstVertexIdx;
	}
}

bool CMeshDX8::SetRenderState( int nVertexOffsetInBytes, int nFirstVertexIdx, VertexFormat_t vertexFormat )
{
	// Can't set the state if we're deactivated
	if ( g_pShaderDeviceDx8->IsDeactivated() )
	{
		ResetMeshRenderState();
		return false;
	}

	g_LastVertexFormat = vertexFormat;

	SetVertexIDStreamState();
	SetColorStreamState();
	SetVertexStreamState( nVertexOffsetInBytes );
	SetIndexStreamState( nFirstVertexIdx );

	return true;
}


//-----------------------------------------------------------------------------
// Draws the static mesh
//-----------------------------------------------------------------------------
void CMeshDX8::Draw( int nFirstIndex, int nIndexCount )
{
	Assert( m_pVertexBuffer );
	if ( !m_pVertexBuffer )
	{
		return;
	}

	if ( !ShaderUtil()->OnDrawMesh( this, nFirstIndex, nIndexCount ) )
	{
		MarkAsDrawn();
		return;
	}

	CPrimList primList;
	if( nFirstIndex == -1 || nIndexCount == 0 )
	{
		primList.m_FirstIndex = 0;
		primList.m_NumIndices = m_NumIndices;
	}
	else
	{
		primList.m_FirstIndex = nFirstIndex;
		primList.m_NumIndices = nIndexCount;
	}
	DrawInternal( &primList, 1 );
}

void CMeshDX8::Draw( CPrimList *pLists, int nLists )
{
	Assert( m_pVertexBuffer );
	if ( !m_pVertexBuffer )
	{
		return;
	}

	if ( !ShaderUtil()->OnDrawMesh( this, pLists, nLists ) )
	{
		MarkAsDrawn();
		return;
	}

	DrawInternal( pLists, nLists );
}


void CMeshDX8::DrawInternal( CPrimList *pLists, int nLists )
{
	HandleLateCreation();

	// Make sure there's something to draw..
	int i;
	for ( i=0; i < nLists; i++ )
	{
		if ( pLists[i].m_NumIndices > 0 )
			break;
	}
	if ( i == nLists )
		return;

	// can't do these in selection mode!
	Assert( !ShaderAPI()->IsInSelectionMode() );

	if ( !SetRenderState( 0, 0 ) )
		return;

	s_pPrims = pLists;
	s_nPrims = nLists;

#ifdef _DEBUG
	for ( i = 0; i < nLists; ++i)
	{
		Assert( pLists[i].m_NumIndices > 0 );
	}
#endif

	s_FirstVertex = 0;
	s_NumVertices = m_pVertexBuffer->VertexCount();

	DrawMesh();
}


#ifdef CHECK_INDICES
void CMeshDX8::CheckIndices( CPrimList *pPrim, int numPrimitives )
{
	// g_pLastVertex - this is the current vertex buffer
	// g_pLastColorMesh - this is the current color mesh, if there is one.
	// g_pLastIndex - this is the current index buffer.
	// vertoffset : m_FirstIndex
	if( m_Mode == D3DPT_TRIANGLELIST || m_Mode == D3DPT_TRIANGLESTRIP )
	{
		Assert( pPrim->m_FirstIndex >= 0 && pPrim->m_FirstIndex < g_pLastIndex->IndexCount() );
		int i;
		for( i = 0; i < 2; i++ )
		{
			CVertexBuffer *pMesh;
			if( i == 0 )
			{
				pMesh = g_pLastVertex;
				Assert( pMesh );
			}
			else
			{
				if( !g_pLastColorMesh )
				{
					continue;
				}
				pMesh = g_pLastColorMesh->m_pVertexBuffer;
				if( !pMesh )
				{
					continue;
				}
			}
			Assert( s_FirstVertex >= 0 && 
				(int)( s_FirstVertex + m_FirstIndex ) < pMesh->VertexCount() );
			int nIndexCount = 0;
			if( m_Mode == D3DPT_TRIANGLELIST )
			{
				nIndexCount = numPrimitives * 3;
			}
			else if( m_Mode == D3DPT_TRIANGLESTRIP )
			{
				nIndexCount = numPrimitives + 2;
			}
			else
			{
				Assert( 0 );
			}
			int j;
			for( j = 0; j < nIndexCount; j++ )
			{
				int index = g_pLastIndex->GetShadowIndex( j + pPrim->m_FirstIndex );
				if ( ( index < (int)s_FirstVertex ) || ( index >= (int)( s_FirstVertex + s_NumVertices ) ) )
					Warning("%s invalid index: %d [%u..%u]\n", __FUNCTION__, index, s_FirstVertex, s_FirstVertex + s_NumVertices - 1 );
				Assert( index >= (int)s_FirstVertex );
				Assert( index < (int)(s_FirstVertex + s_NumVertices) );
			}
		}
	}
}
#endif // CHECK_INDICES


//-----------------------------------------------------------------------------
// Actually does the dirty deed of rendering
//-----------------------------------------------------------------------------
void CMeshDX8::RenderPass()
{
	LOCK_SHADERAPI();
	VPROF( "CMeshDX8::RenderPass" );

	HandleLateCreation();

	Assert( m_Type != MATERIAL_HETEROGENOUS );

	// make sure the vertex format is a superset of the current material's
	// vertex format...
	if ( !IsValidVertexFormat( g_LastVertexFormat ) )
	{
		Warning( "Material %s does not support vertex format used by the mesh (maybe missing fields or mismatched vertex compression?), mesh will not be rendered. Grab a programmer!\n",
			ShaderAPI()->GetBoundMaterial()->GetName() );
		return;
	}

	for ( int iPrim=0; iPrim < s_nPrims; iPrim++ )
	{
		CPrimList *pPrim = &s_pPrims[iPrim];

		if ( pPrim->m_NumIndices == 0 )
			continue;

		if ( ( m_Type == MATERIAL_POINTS ) || ( m_Type == MATERIAL_INSTANCED_QUADS ) )
		{
			tmZone( TELEMETRY_LEVEL1, TMZF_NONE, "Dx9Device_DrawPrimitive" );

			// (For point/instanced-quad lists, we don't actually fill in indices, but we treat it as
			// though there are indices for the list up until here).
			Dx9Device()->DrawPrimitive( m_Mode, s_FirstVertex, pPrim->m_NumIndices );
		}
		else
		{
			int numPrimitives = NumPrimitives( s_NumVertices, pPrim->m_NumIndices );

#ifdef CHECK_INDICES
			CheckIndices( pPrim, numPrimitives );
#endif // CHECK_INDICES
			{
				VPROF( "Dx9Device()->DrawIndexedPrimitive" );
				VPROF_INCREMENT_COUNTER( "DrawIndexedPrimitive", 1 );
				VPROF_INCREMENT_COUNTER( "numPrimitives", numPrimitives );
				VPROF_INCREMENT_GROUP_COUNTER( "render/DrawIndexedPrimitive", COUNTER_GROUP_TELEMETRY, 1 );
				VPROF_INCREMENT_GROUP_COUNTER( "render/numPrimitives", COUNTER_GROUP_TELEMETRY, 1 );

				Dx9Device()->DrawIndexedPrimitive( 
					m_Mode,			// Member of the D3DPRIMITIVETYPE enumerated type, describing the type of primitive to render. D3DPT_POINTLIST is not supported with this method.

					m_FirstIndex,	// Offset from the start of the vertex buffer to the first vertex index. An index of 0 in the index buffer refers to this location in the vertex buffer.

					s_FirstVertex,	// Minimum vertex index for vertices used during this call. This is a zero based index relative to BaseVertexIndex.
									// The first Vertex in the vertexbuffer that we are currently using for the current batch.

					s_NumVertices,	// Number of vertices used during this call. The first vertex is located at index: BaseVertexIndex + MinIndex.

					pPrim->m_FirstIndex, // Index of the first index to use when accesssing the vertex buffer. Beginning at StartIndex to index vertices from the vertex buffer.

					numPrimitives );// Number of primitives to render. The number of vertices used is a function of the primitive count and the primitive type.
			}
		}
	}

	if ( g_pLastVertex )
	{
		g_pLastVertex->MarkUsedInRendering();
	}

	if( g_pLastIndex )
	{
		g_pLastIndex->MarkUsedInRendering();
	}
}

//-----------------------------------------------------------------------------
//
// Dynamic mesh implementation
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CDynamicMeshDX8::CDynamicMeshDX8() : CMeshDX8( "CDynamicMeshDX8" )
{
	m_nBufferId = 0;
	ResetVertexAndIndexCounts();
}

CDynamicMeshDX8::~CDynamicMeshDX8()
{
}


//-----------------------------------------------------------------------------
// Initializes the dynamic mesh
//-----------------------------------------------------------------------------
void CDynamicMeshDX8::Init( int nBufferId )
{
	m_nBufferId = nBufferId;
}


//-----------------------------------------------------------------------------
// Resets buffering state
//-----------------------------------------------------------------------------
void CDynamicMeshDX8::ResetVertexAndIndexCounts()
{
	m_TotalVertices = m_TotalIndices = 0;
	m_FirstIndex = m_nFirstVertex = -1;
	m_HasDrawn = false;
}


//-----------------------------------------------------------------------------
// Resets the state in case of a task switch
//-----------------------------------------------------------------------------
void CDynamicMeshDX8::Reset()
{
	m_VertexFormat = 0;
	m_pVertexBuffer = 0;
	SafeRelease( &m_pIndexBuffer );
	ResetVertexAndIndexCounts();

	// Force the render state to be updated next time
	ResetMeshRenderState();
}


//-----------------------------------------------------------------------------
// Sets the vertex format associated with the dynamic mesh
//-----------------------------------------------------------------------------
void CDynamicMeshDX8::SetVertexFormat( VertexFormat_t format )
{
	if ( g_pShaderDeviceDx8->IsDeactivated())
		return;

	if ( CompressionType( format ) != VERTEX_COMPRESSION_NONE )
	{
		// UNDONE: support compressed dynamic meshes if needed (pro: less VB memory, con: CMeshBuilder gets slower)
		Warning( "ERROR: dynamic meshes cannot use compressed vertices!\n" );
		Assert( 0 );
		format &= ~VERTEX_FORMAT_COMPRESSED;
	}

	if ((format != m_VertexFormat) || m_VertexOverride || m_IndexOverride)
	{
		m_VertexFormat = format;
		UseVertexBuffer( g_MeshMgr.FindOrCreateVertexBuffer( m_nBufferId, format ) );

		if ( m_nBufferId == 0 )
		{
			UseIndexBuffer( g_MeshMgr.GetDynamicIndexBuffer() );
		}

		m_VertexOverride = m_IndexOverride = false;
	}
}

void CDynamicMeshDX8::OverrideVertexBuffer( CVertexBuffer* pVertexBuffer )
{
	UseVertexBuffer( pVertexBuffer );
	m_VertexOverride = true;
}

void CDynamicMeshDX8::OverrideIndexBuffer( CIndexBuffer* pIndexBuffer )
{
	UseIndexBuffer( pIndexBuffer );
	m_IndexOverride = true;
}


//-----------------------------------------------------------------------------
// Do I need to reset the vertex format?
//-----------------------------------------------------------------------------
bool CDynamicMeshDX8::NeedsVertexFormatReset( VertexFormat_t fmt ) const
{
	return m_VertexOverride || m_IndexOverride || (m_VertexFormat != fmt);
}



//-----------------------------------------------------------------------------
// Locks/unlocks the entire mesh
//-----------------------------------------------------------------------------
bool CDynamicMeshDX8::HasEnoughRoom( int nVertexCount, int nIndexCount ) const
{
	if ( g_pShaderDeviceDx8->IsDeactivated() )
		return false;

	Assert( m_pVertexBuffer != NULL );

	// We need space in both the vertex and index buffer
	return m_pVertexBuffer->HasEnoughRoom( nVertexCount ) && 
		m_pIndexBuffer->HasEnoughRoom( nIndexCount );
}


//-----------------------------------------------------------------------------
// returns the number of indices in the mesh
//-----------------------------------------------------------------------------
int CDynamicMeshDX8::IndexCount( ) const
{
	return m_TotalIndices;
}


//-----------------------------------------------------------------------------
// Operation to do pre-lock	(only called for buffered meshes)
//-----------------------------------------------------------------------------
void CDynamicMeshDX8::PreLock()
{
	if (m_HasDrawn)
	{
		// Start again then
		ResetVertexAndIndexCounts();
	}
}


//-----------------------------------------------------------------------------
// Locks/unlocks the entire mesh
//-----------------------------------------------------------------------------
void CDynamicMeshDX8::LockMesh( int nVertexCount, int nIndexCount, MeshDesc_t& desc )
{
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s %d %d", __FUNCTION__, nVertexCount, nIndexCount );

	ShaderUtil()->SyncMatrices();

	{
		tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "g_ShaderMutex.Lock" );
		g_ShaderMutex.Lock();
	}

	// Yes, this may well also be called from BufferedMesh but that's ok
	{
		tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "PreLock" );
		PreLock();
	}

	if (m_VertexOverride)
	{
		nVertexCount = 0;
	}

	if (m_IndexOverride)
	{
		nIndexCount = 0;
	}

	{
		tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "Lock" );
		Lock( nVertexCount, false, *static_cast<VertexDesc_t*>( &desc ) );
	}

	if (m_nFirstVertex < 0)
	{
		m_nFirstVertex = desc.m_nFirstVertex;
	}

	// When we're using a static index buffer or a flex mesh, the indices assume vertices start at 0
	if ( m_IndexOverride || HasFlexMesh() )
	{
		desc.m_nFirstVertex -= m_nFirstVertex;
	}

	// Don't add indices for points; DrawIndexedPrimitive not supported for them.
	if ( m_Type != MATERIAL_POINTS )
	{
		int nFirstIndex;
		{
			tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "Lock nFirstIndex" );
			nFirstIndex = Lock( false, -1, nIndexCount, *static_cast<IndexDesc_t*>( &desc ) );
		}
		if (m_FirstIndex < 0)
		{
			m_FirstIndex = nFirstIndex;
		}
	}
	else
	{
		desc.m_pIndices = g_nScratchIndexBuffer;
		desc.m_nIndexSize = 0;
	}

	CBaseMeshDX8::m_bMeshLocked = true;
}


//-----------------------------------------------------------------------------
// Unlocks the mesh
//-----------------------------------------------------------------------------
void CDynamicMeshDX8::UnlockMesh( int nVertexCount, int nIndexCount, MeshDesc_t& desc )
{
	m_TotalVertices += nVertexCount;
	m_TotalIndices += nIndexCount;

	if (DebugTrace())
	{
		Spew( nVertexCount, nIndexCount, desc );
	}

	CMeshDX8::UnlockMesh( nVertexCount, nIndexCount, desc );

	// This is handled in the CMeshDX8::UnlockMesh above.
	//CBaseMeshDX8::m_bMeshLocked = false;
}


//-----------------------------------------------------------------------------
// Draws it
//-----------------------------------------------------------------------------
void CDynamicMeshDX8::Draw( int nFirstIndex, int nIndexCount )
{
	if ( !ShaderUtil()->OnDrawMesh( this, nFirstIndex, nIndexCount ) )
	{
		MarkAsDrawn();
		return;
	}

	VPROF( "CDynamicMeshDX8::Draw" );

	m_HasDrawn = true;

	if (m_IndexOverride || m_VertexOverride || 
		( ( m_TotalVertices > 0 ) && ( m_TotalIndices > 0 || m_Type == MATERIAL_POINTS || m_Type == MATERIAL_INSTANCED_QUADS ) ) )
	{
		Assert( !m_IsDrawing );
		
		HandleLateCreation( );

		// only have a non-zero first vertex when we are using static indices
		int nFirstVertex = m_VertexOverride ? 0 : m_nFirstVertex;
		int actualFirstVertex = m_IndexOverride ? nFirstVertex : 0;
		int nVertexOffsetInBytes = HasFlexMesh() ? nFirstVertex * g_MeshMgr.VertexFormatSize( GetVertexFormat() ) : 0;
		int baseIndex = m_IndexOverride ? 0 : m_FirstIndex;

		// Overriding with the dynamic index buffer, preserve state!
		if ( m_IndexOverride && m_pIndexBuffer == g_MeshMgr.GetDynamicIndexBuffer() )
		{
			baseIndex = m_FirstIndex;
		}

		VertexFormat_t fmt = m_VertexOverride ? GetVertexFormat() : VERTEX_FORMAT_INVALID;
		if ( !SetRenderState( nVertexOffsetInBytes, actualFirstVertex, fmt ) )
			return;

		// Draws a portion of the mesh
		int numVertices = m_VertexOverride ? m_pVertexBuffer->VertexCount() : m_TotalVertices;
		if ((nFirstIndex != -1) && (nIndexCount != 0))
		{
			nFirstIndex += baseIndex;
		}
		else
		{
			// by default we draw the whole thing
			nFirstIndex = baseIndex;
			if( m_IndexOverride )
			{
				nIndexCount = m_pIndexBuffer->IndexCount();
				Assert( nIndexCount != 0 );
			}
			else
			{
				nIndexCount = m_TotalIndices;
				// Fake out the index count	if we're drawing points/instanced-quads
				if ( ( m_Type == MATERIAL_POINTS ) || ( m_Type == MATERIAL_INSTANCED_QUADS ) )
				{
					nIndexCount = m_TotalVertices;
				}
				Assert( nIndexCount != 0 );
			}
		}

		// Fix up nFirstVertex to indicate the first vertex used in the data
		if ( !HasFlexMesh() )
		{
			actualFirstVertex = nFirstVertex - actualFirstVertex;
		}
		
		s_FirstVertex = actualFirstVertex;
		s_NumVertices = numVertices;
		
		// Build a primlist with 1 element..
		CPrimList prim;
		prim.m_FirstIndex = nFirstIndex;
		prim.m_NumIndices = nIndexCount;
		Assert( nIndexCount != 0 );
		s_pPrims = &prim;
		s_nPrims = 1;

		DrawMesh();

		s_pPrims = NULL;
	}
}


//-----------------------------------------------------------------------------
// This is useful when we need to dynamically modify data; just set the
// render state and draw the pass immediately
//-----------------------------------------------------------------------------
void CDynamicMeshDX8::DrawSinglePassImmediately()
{
	if ((m_TotalVertices > 0) || (m_TotalIndices > 0))
	{
		Assert( !m_IsDrawing );

		// Set the render state
		if ( SetRenderState( 0, 0 ) )
		{
			s_FirstVertex = m_nFirstVertex;
			s_NumVertices = m_TotalVertices;

			// Make a temporary PrimList to hold the indices.
			CPrimList prim( m_FirstIndex, m_TotalIndices );
			Assert( m_TotalIndices != 0 );
			s_pPrims = &prim;
			s_nPrims = 1;

			// Render it
			RenderPass();
		}

		// We're done with our data
		ResetVertexAndIndexCounts();
	}
}

//-----------------------------------------------------------------------------
//
// A mesh that stores temporary vertex data in the correct format (for modification)
//
//-----------------------------------------------------------------------------
// Used in rendering sub-parts of the mesh
unsigned int CTempMeshDX8::s_NumIndices;
unsigned int CTempMeshDX8::s_FirstIndex;

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CTempMeshDX8::CTempMeshDX8( bool isDynamic ) : m_VertexSize(0xFFFF), m_IsDynamic(isDynamic)
{
#ifdef DBGFLAG_ASSERT
	m_Locked = false;
	m_InPass = false;
#endif
}

CTempMeshDX8::~CTempMeshDX8()
{
}

//-----------------------------------------------------------------------------
// Is the temp mesh dynamic?
//-----------------------------------------------------------------------------
bool CTempMeshDX8::IsDynamic() const
{
	return m_IsDynamic;
}


//-----------------------------------------------------------------------------
// Sets the vertex format
//-----------------------------------------------------------------------------
void CTempMeshDX8::SetVertexFormat( VertexFormat_t format )
{
	CBaseMeshDX8::SetVertexFormat(format);
	m_VertexSize = g_MeshMgr.VertexFormatSize( format );
}

//-----------------------------------------------------------------------------
// returns the # of vertices (static meshes only)
//-----------------------------------------------------------------------------
int CTempMeshDX8::VertexCount() const
{
	return m_VertexSize ? m_VertexData.Count() / m_VertexSize : 0;
}

//-----------------------------------------------------------------------------
// returns the # of indices 
//-----------------------------------------------------------------------------
int CTempMeshDX8::IndexCount( ) const
{
	return m_IndexData.Count();
}

void CTempMeshDX8::ModifyBeginEx( bool bReadOnly, int nFirstVertex, int nVertexCount, int nFirstIndex, int nIndexCount, MeshDesc_t& desc )
{
	Assert( !m_Locked );

	m_LockedVerts = nVertexCount;
	m_LockedIndices = nIndexCount;

	if( nVertexCount > 0 )
	{
		int vertexByteOffset = m_VertexSize * nFirstVertex;
		
		// Lock it baby
		unsigned char* pVertexMemory = &m_VertexData[vertexByteOffset];
		
		// Compute the vertex index..
		desc.m_nFirstVertex = vertexByteOffset / m_VertexSize;
		
		// Set up the mesh descriptor
		g_MeshMgr.ComputeVertexDescription( pVertexMemory, m_VertexFormat, desc );
	}
	else
	{
		desc.m_nFirstVertex = 0;
		// Set up the mesh descriptor
		g_MeshMgr.ComputeVertexDescription( 0, 0, desc );
	}

	if (m_Type != MATERIAL_POINTS && nIndexCount > 0 )
	{
		desc.m_pIndices = &m_IndexData[nFirstIndex];
		desc.m_nIndexSize = 1;
	}
	else
	{
		desc.m_pIndices = g_nScratchIndexBuffer;
		desc.m_nIndexSize = 0;
	}

#ifdef DBGFLAG_ASSERT
	m_Locked = true;
#endif
}

void CTempMeshDX8::ModifyBegin( int nFirstVertex, int nVertexCount, int nFirstIndex, int nIndexCount, MeshDesc_t& desc )
{
	ModifyBeginEx( false, nFirstVertex, nVertexCount, nFirstIndex, nIndexCount, desc );
}

void CTempMeshDX8::ModifyEnd( MeshDesc_t& desc )
{
#ifdef DBGFLAG_ASSERT
	Assert( m_Locked );
	m_Locked = false;
#endif
}

//-----------------------------------------------------------------------------
// Locks/unlocks the mesh
//-----------------------------------------------------------------------------
void CTempMeshDX8::LockMesh( int nVertexCount, int nIndexCount, MeshDesc_t& desc )
{
	ShaderUtil()->SyncMatrices();

	g_ShaderMutex.Lock();

	Assert( !m_Locked );

	m_LockedVerts = nVertexCount;
	m_LockedIndices = nIndexCount;

	if( nVertexCount > 0 )
	{
		int vertexByteOffset = m_VertexData.AddMultipleToTail( m_VertexSize * nVertexCount );
		
		// Lock it baby
		unsigned char* pVertexMemory = &m_VertexData[vertexByteOffset];
		
		// Compute the vertex index..
		desc.m_nFirstVertex = vertexByteOffset / m_VertexSize;
		
		// Set up the mesh descriptor
		g_MeshMgr.ComputeVertexDescription( pVertexMemory, m_VertexFormat, desc );
	}
	else
	{
		desc.m_nFirstVertex = 0;
		// Set up the mesh descriptor
		g_MeshMgr.ComputeVertexDescription( 0, 0, desc );
	}

	if (m_Type != MATERIAL_POINTS && nIndexCount > 0 )
	{
		int nFirstIndex = m_IndexData.AddMultipleToTail( nIndexCount );
		desc.m_pIndices = &m_IndexData[nFirstIndex];
		desc.m_nIndexSize = 1;
	}
	else
	{
		desc.m_pIndices = g_nScratchIndexBuffer;
		desc.m_nIndexSize = 0;
	}

#ifdef DBGFLAG_ASSERT
	m_Locked = true;
#endif

	CBaseMeshDX8::m_bMeshLocked = true;
}

void CTempMeshDX8::UnlockMesh( int nVertexCount, int nIndexCount, MeshDesc_t& desc )
{
	Assert( m_Locked );

	// Remove unused vertices and indices
	int verticesToRemove = m_LockedVerts - nVertexCount;
	if( verticesToRemove != 0 )
	{
		m_VertexData.RemoveMultiple( m_VertexData.Count() - verticesToRemove, verticesToRemove );
	}

	int indicesToRemove = m_LockedIndices - nIndexCount;
	if( indicesToRemove != 0 )
	{
		m_IndexData.RemoveMultiple( m_IndexData.Count() - indicesToRemove, indicesToRemove );
	}

#ifdef DBGFLAG_ASSERT
	m_Locked = false;
#endif

	CBaseMeshDX8::m_bMeshLocked = false;

	g_ShaderMutex.Unlock();
}

//-----------------------------------------------------------------------------
// Sets the primitive type
//-----------------------------------------------------------------------------
void CTempMeshDX8::SetPrimitiveType( MaterialPrimitiveType_t type )
{
	// FIXME: Support MATERIAL_INSTANCED_QUADS for CTempMeshDX8 (X360 only)
	Assert( ( type != MATERIAL_INSTANCED_QUADS ) /* || IsX360() */ );
	m_Type = type;
}

MaterialPrimitiveType_t CTempMeshDX8::GetPrimitiveType( ) const
{
	return m_Type;
}

//-----------------------------------------------------------------------------
// Gets the dynamic mesh
//-----------------------------------------------------------------------------
CDynamicMeshDX8* CTempMeshDX8::GetDynamicMesh( )
{
	return static_cast<CDynamicMeshDX8*>(g_MeshMgr.GetActualDynamicMesh( m_VertexFormat ));
}

//-----------------------------------------------------------------------------
// Draws the entire mesh
//-----------------------------------------------------------------------------
void CTempMeshDX8::Draw( int nFirstIndex, int nIndexCount )
{
	if ( !ShaderUtil()->OnDrawMesh( this, nFirstIndex, nIndexCount ) )
	{
		MarkAsDrawn();
		return;
	}

	if (m_VertexData.Count() > 0)
	{
		if ( !g_pShaderDeviceDx8->IsDeactivated() )
		{
#ifdef DRAW_SELECTION
			if (!g_bDrawSelection && !ShaderAPI()->IsInSelectionMode())
#else
			if (!ShaderAPI()->IsInSelectionMode())
#endif
			{
				s_FirstIndex = nFirstIndex;
				s_NumIndices = nIndexCount;

				DrawMesh( );

				// This assertion fails if a BeginPass() call was not matched by 
				// a RenderPass() call
				Assert(!m_InPass);
			}
			else
			{
				TestSelection();
			}
		}

		// Clear out the data if this temp mesh is a dynamic one...
		if (m_IsDynamic)
		{
			m_VertexData.RemoveAll();
			m_IndexData.RemoveAll();
		}
	}
}

void CTempMeshDX8::CopyToMeshBuilder( 
	int iStartVert,		// Which vertices to copy.
	int nVerts, 
	int iStartIndex,	// Which indices to copy.
	int nIndices, 
	int indexOffset,	// This is added to each index.
	CMeshBuilder &builder )
{
	int startOffset = iStartVert * m_VertexSize;
	int endOffset = (iStartVert + nVerts) * m_VertexSize;
	Assert( startOffset >= 0 && startOffset <= m_VertexData.Count() );
	Assert( endOffset >= 0 && endOffset <= m_VertexData.Count() && endOffset >= startOffset );
	if ( endOffset > startOffset )
	{
		// FIXME: make this a method of CMeshBuilder (so the 'Position' pointer accessor can be removed)
		//        make sure it takes a VertexFormat_t parameter for src/dest match validation
		memcpy( (void*)builder.Position(), &m_VertexData[startOffset], endOffset - startOffset );
		builder.AdvanceVertices( nVerts );
	}

	for ( int i = 0; i < nIndices; ++i )
	{
		builder.Index( m_IndexData[iStartIndex+i] + indexOffset );
		builder.AdvanceIndex();
	}		
}

//-----------------------------------------------------------------------------
// Selection mode helper functions
//-----------------------------------------------------------------------------
static void ComputeModelToView( D3DXMATRIX& modelToView )
{
	// Get the modelview matrix...
	D3DXMATRIX world, view;
	ShaderAPI()->GetMatrix( MATERIAL_MODEL, (float*)&world );
	ShaderAPI()->GetMatrix( MATERIAL_VIEW, (float*)&view );
	D3DXMatrixMultiply( &modelToView, &world, &view );
}

static float ComputeCullFactor( )
{
	D3DCULL cullMode = ShaderAPI()->GetCullMode();

	float cullFactor;
	switch(cullMode)
	{
	case D3DCULL_CCW:
		cullFactor = -1.0f;
		break;
		
	case D3DCULL_CW:
		cullFactor = 1.0f;
		break;

	default:
		cullFactor = 0.0f;
		break;
	};

	return cullFactor;
}

//-----------------------------------------------------------------------------
// Clip to viewport
//-----------------------------------------------------------------------------
static int g_NumClipVerts;
static D3DXVECTOR3 g_ClipVerts[16];

static bool PointInsidePlane( D3DXVECTOR3* pVert, int normalInd, float val, bool nearClip )
{
	if ((val > 0) || nearClip)
		return (val - (*pVert)[normalInd] >= 0);
	else
		return ((*pVert)[normalInd] - val >= 0);
}

static void IntersectPlane( D3DXVECTOR3* pStart, D3DXVECTOR3* pEnd, 
						    int normalInd, float val, D3DXVECTOR3* pOutVert )
{
	D3DXVECTOR3 dir;
	D3DXVec3Subtract( &dir, pEnd, pStart );
	Assert( dir[normalInd] != 0.0f );
	float t = (val - (*pStart)[normalInd]) / dir[normalInd];
	pOutVert->x = pStart->x + dir.x * t;
	pOutVert->y = pStart->y + dir.y * t;
	pOutVert->z = pStart->z + dir.z * t;

	// Avoid any precision problems.
	(*pOutVert)[normalInd] = val;
}

static int ClipTriangleAgainstPlane( D3DXVECTOR3** ppVert, int nVertexCount, 
			D3DXVECTOR3** ppOutVert, int normalInd, float val, bool nearClip = false )
{
	// Ye Olde Sutherland-Hodgman clipping algorithm
	int numOutVerts = 0;
	D3DXVECTOR3* pStart = ppVert[nVertexCount-1];
	bool startInside = PointInsidePlane( pStart, normalInd, val, nearClip );
	for (int i = 0; i < nVertexCount; ++i)
	{
		D3DXVECTOR3* pEnd = ppVert[i];
		bool endInside = PointInsidePlane( pEnd, normalInd, val, nearClip );
		if (endInside)
		{
			if (!startInside)
			{
				IntersectPlane( pStart, pEnd, normalInd, val, &g_ClipVerts[g_NumClipVerts] );
				ppOutVert[numOutVerts++] = &g_ClipVerts[g_NumClipVerts++];
			}
			ppOutVert[numOutVerts++] = pEnd;
		}
		else
		{
			if (startInside)
			{
				IntersectPlane( pStart, pEnd, normalInd, val, &g_ClipVerts[g_NumClipVerts] );
				ppOutVert[numOutVerts++] = &g_ClipVerts[g_NumClipVerts++];
			}
		}
		pStart = pEnd;
		startInside = endInside;
	}

	return numOutVerts;
}

void CTempMeshDX8::ClipTriangle( D3DXVECTOR3** ppVert, float zNear, D3DXMATRIX& projection )
{
	int i;
	int nVertexCount = 3;
	D3DXVECTOR3* ppClipVert1[10];
	D3DXVECTOR3* ppClipVert2[10];

	g_NumClipVerts = 0;

	// Clip against the near plane in view space to prevent negative w.
	// Clip against each plane
	nVertexCount = ClipTriangleAgainstPlane( ppVert, nVertexCount, ppClipVert1, 2, zNear, true );
	if (nVertexCount < 3)
		return;

	// Sucks that I have to do this, but I have to clip near plane in view space 
	// Clipping in projection space is screwy when w < 0
	// Transform the clipped points into projection space
	Assert( g_NumClipVerts <= 2 );
	for (i = 0; i < nVertexCount; ++i)
	{
		if (ppClipVert1[i] == &g_ClipVerts[0])
		{
			D3DXVec3TransformCoord( &g_ClipVerts[0], ppClipVert1[i], &projection ); 
		}
		else if (ppClipVert1[i] == &g_ClipVerts[1])
		{
			D3DXVec3TransformCoord( &g_ClipVerts[1], ppClipVert1[i], &projection ); 
		}
		else
		{
			D3DXVec3TransformCoord( &g_ClipVerts[g_NumClipVerts], ppClipVert1[i], &projection );
		    ppClipVert1[i] = &g_ClipVerts[g_NumClipVerts];
			++g_NumClipVerts;
		}
	}

	nVertexCount = ClipTriangleAgainstPlane( ppClipVert1, nVertexCount, ppClipVert2, 2, 1.0f );
	if (nVertexCount < 3)
		return;

	nVertexCount = ClipTriangleAgainstPlane( ppClipVert2, nVertexCount, ppClipVert1, 0, 1.0f );
	if (nVertexCount < 3)
		return;

	nVertexCount = ClipTriangleAgainstPlane( ppClipVert1, nVertexCount, ppClipVert2, 0, -1.0f );
	if (nVertexCount < 3)
		return;

	nVertexCount = ClipTriangleAgainstPlane( ppClipVert2, nVertexCount, ppClipVert1, 1, 1.0f );
	if (nVertexCount < 3)
		return;
	
	nVertexCount = ClipTriangleAgainstPlane( ppClipVert1, nVertexCount, ppClipVert2, 1, -1.0f );
	if (nVertexCount < 3)
		return;

#ifdef DRAW_SELECTION
	if( 1 || g_bDrawSelection )
	{
		srand( *(int*)(&ppClipVert2[0]->x) ); 
		unsigned char r = (unsigned char)(rand() * 191.0f / VALVE_RAND_MAX) + 64;
		unsigned char g = (unsigned char)(rand() * 191.0f / VALVE_RAND_MAX) + 64;
		unsigned char b = (unsigned char)(rand() * 191.0f / VALVE_RAND_MAX) + 64;

		ShaderAPI()->SetupSelectionModeVisualizationState();

		CMeshBuilder* pMeshBuilder = ShaderAPI()->GetVertexModifyBuilder();
		IMesh* pMesh = GetDynamicMesh();
		pMeshBuilder->Begin( pMesh, MATERIAL_POLYGON, nVertexCount );
		
		for ( i = 0; i < nVertexCount; ++i)
		{
			pMeshBuilder->Position3fv( *ppClipVert2[i] );
			pMeshBuilder->Color3ub( r, g, b );
			pMeshBuilder->AdvanceVertex();
		}

		pMeshBuilder->End();
		pMesh->Draw();

		pMeshBuilder->Begin( pMesh, MATERIAL_LINE_LOOP, nVertexCount );
		
		for ( i = 0; i < nVertexCount; ++i)
		{
			pMeshBuilder->Position3fv( *ppClipVert2[i] );
			pMeshBuilder->Color3ub( 255, 255, 255 );
			pMeshBuilder->AdvanceVertex();
		}

		pMeshBuilder->End();
		pMesh->Draw();
	}
#endif

	// Compute closest and furthest verts
	float minz = ppClipVert2[0]->z;
	float maxz = ppClipVert2[0]->z;
	for ( i = 1; i < nVertexCount; ++i )
	{
		if (ppClipVert2[i]->z < minz)
			minz = ppClipVert2[i]->z;
		else if (ppClipVert2[i]->z > maxz)
			maxz = ppClipVert2[i]->z;
	}

	ShaderAPI()->RegisterSelectionHit( minz, maxz );
}

//-----------------------------------------------------------------------------
// Selection mode 
//-----------------------------------------------------------------------------
void CTempMeshDX8::TestSelection()
{
	// Note that this doesn't take into account any vertex modification
	// done in a vertex shader. Also it doesn't take into account any clipping
	// done in hardware

	// Blow off points and lines; they don't matter
	if ((m_Type != MATERIAL_TRIANGLES) && (m_Type != MATERIAL_TRIANGLE_STRIP))
		return;

	D3DXMATRIX modelToView, projection;
	ComputeModelToView( modelToView );
	ShaderAPI()->GetMatrix( MATERIAL_PROJECTION, (float*)&projection );
	float zNear = -projection.m[3][2] / projection.m[2][2];

	D3DXVECTOR3* pPos[3];
	D3DXVECTOR3 edge[2];
	D3DXVECTOR3 normal;

	int numTriangles;
	if (m_Type == MATERIAL_TRIANGLES)
		numTriangles = m_IndexData.Count() / 3;
	else
		numTriangles = m_IndexData.Count() - 2;

	float cullFactor = ComputeCullFactor();

	// Makes the lovely loop simpler
	if (m_Type == MATERIAL_TRIANGLE_STRIP)
		cullFactor *= -1.0f;

	// We'll need some temporary memory to tell us if we're transformed the vert
	int nVertexCount = m_VertexData.Count() / m_VertexSize;
	static CUtlVector< unsigned char > transformedVert;
	int transformedVertSize = (nVertexCount + 7) >> 3;
	transformedVert.RemoveAll();
	transformedVert.EnsureCapacity( transformedVertSize );
	transformedVert.AddMultipleToTail( transformedVertSize );
	memset( transformedVert.Base(), 0, transformedVertSize );

	int indexPos;
	for (int i = 0; i < numTriangles; ++i)
	{
		// Get the three indices
		if (m_Type == MATERIAL_TRIANGLES)
		{
			indexPos = i * 3;
		}
		else
		{
			Assert( m_Type == MATERIAL_TRIANGLE_STRIP );
			cullFactor *= -1.0f;
			indexPos = i;
		}

		// BAH. Gotta clip to the near clip plane in view space to prevent
		// negative w coords; negative coords throw off the projection-space clipper.

		// Get the three positions in view space
		int inFrontIdx = -1;
		for (int j = 0; j < 3; ++j)
		{
			int index = m_IndexData[indexPos];
			D3DXVECTOR3* pPosition = (D3DXVECTOR3*)&m_VertexData[index * m_VertexSize];
			if ((transformedVert[index >> 3] & (1 << (index & 0x7))) == 0)
			{
				D3DXVec3TransformCoord( pPosition, pPosition, &modelToView );
				transformedVert[index >> 3] |= (1 << (index & 0x7));
			}

			pPos[j] = pPosition;
			if (pPos[j]->z < 0.0f)
				inFrontIdx = j;
			++indexPos;
		}

		// all points are behind the camera
		if (inFrontIdx < 0)
			continue;

		// backface cull....
		D3DXVec3Subtract( &edge[0], pPos[1], pPos[0] );
		D3DXVec3Subtract( &edge[1], pPos[2], pPos[0] );
		D3DXVec3Cross( &normal, &edge[0], &edge[1] );
		float dot = D3DXVec3Dot( &normal, pPos[inFrontIdx] );
		if (dot * cullFactor > 0.0f)
			continue;

		// Clip to viewport
		ClipTriangle( pPos, zNear, projection );
	}
}

//-----------------------------------------------------------------------------
// Begins a render pass
//-----------------------------------------------------------------------------
void CTempMeshDX8::BeginPass( )
{
	Assert( !m_InPass );

#ifdef DBGFLAG_ASSERT
	m_InPass = true;
#endif

	CMeshBuilder* pMeshBuilder = ShaderAPI()->GetVertexModifyBuilder();

	CDynamicMeshDX8* pMesh = GetDynamicMesh( );

	int nIndexCount;
	int nFirstIndex;
	if ((s_FirstIndex == -1) && (s_NumIndices == 0))
	{
		nIndexCount = m_IndexData.Count();
		nFirstIndex = 0;
	}
	else
	{
		nIndexCount = s_NumIndices;
		nFirstIndex = s_FirstIndex;
	}
	
	int i;
	int nVertexCount = m_VertexData.Count() / m_VertexSize;
	pMeshBuilder->Begin( pMesh, m_Type, nVertexCount, nIndexCount );

	// Copy in the vertex data...
	// Note that since we pad the vertices, it's faster for us to simply
	// copy the fields we're using...
	Assert( pMeshBuilder->BaseVertexData() );
	memcpy( pMeshBuilder->BaseVertexData(), m_VertexData.Base(), m_VertexData.Count() );
	pMeshBuilder->AdvanceVertices( m_VertexData.Count() / m_VertexSize );

	for ( i = 0; i < nIndexCount; ++i )
	{
		pMeshBuilder->Index( m_IndexData[nFirstIndex+i] );
		pMeshBuilder->AdvanceIndex();
	}

	// NOTE: The client is expected to modify the data after this call is made
	pMeshBuilder->Reset();
}

//-----------------------------------------------------------------------------
// Draws a single pass
//-----------------------------------------------------------------------------
void CTempMeshDX8::RenderPass()
{
	Assert( m_InPass );

#ifdef DBGFLAG_ASSERT
	m_InPass = false;
#endif

	// Have the shader API modify the vertex data as it needs
	// This vertex data is modified based on state set by the material
	ShaderAPI()->ModifyVertexData( );

	// Done building the mesh
	ShaderAPI()->GetVertexModifyBuilder()->End();

	// Have the dynamic mesh render a single pass...
	GetDynamicMesh()->DrawSinglePassImmediately();
}

//-----------------------------------------------------------------------------
//
// Buffered mesh implementation
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------

CBufferedMeshDX8::CBufferedMeshDX8() : m_IsFlushing(false), m_WasRendered(true)
{
	m_pMesh = NULL;
#ifdef DEBUG_BUFFERED_STATE
	m_BufferedStateSet = false;
#endif
}

CBufferedMeshDX8::~CBufferedMeshDX8()
{
}


//-----------------------------------------------------------------------------
// Sets the mesh
//-----------------------------------------------------------------------------
void CBufferedMeshDX8::SetMesh( CBaseMeshDX8* pMesh )
{
	if (m_pMesh != pMesh)
	{
		ShaderAPI()->FlushBufferedPrimitives();
		m_pMesh = pMesh;
	}
}


//-----------------------------------------------------------------------------
// Spews the mesh data
//-----------------------------------------------------------------------------
void CBufferedMeshDX8::Spew( int nVertexCount, int nIndexCount, const MeshDesc_t &spewDesc )
{
	if ( m_pMesh )
	{
		m_pMesh->Spew( nVertexCount, nIndexCount, spewDesc );
	}
}


//-----------------------------------------------------------------------------
// Sets the material
//-----------------------------------------------------------------------------
void CBufferedMeshDX8::SetVertexFormat( VertexFormat_t format )
{
	Assert( m_pMesh );
	if (m_pMesh->NeedsVertexFormatReset(format))
	{
		ShaderAPI()->FlushBufferedPrimitives();
		m_pMesh->SetVertexFormat( format );
	}
}

void CBufferedMeshDX8::SetMorphFormat( MorphFormat_t format )
{
	Assert( m_pMesh );
	m_pMesh->SetMorphFormat( format );
}

VertexFormat_t CBufferedMeshDX8::GetVertexFormat( ) const
{
	Assert( m_pMesh );
	return m_pMesh->GetVertexFormat();
}

void CBufferedMeshDX8::SetMaterial( IMaterial* pMaterial )
{
#if _DEBUG
	Assert( m_pMesh );
	m_pMesh->SetMaterial( pMaterial );
#endif
}

void CBufferedMeshDX8::ValidateData( int nVertexCount, int nIndexCount, const MeshDesc_t & spewDesc )
{
#if _DEBUG
	Assert( m_pMesh );
	m_pMesh->ValidateData( nVertexCount, nIndexCount, spewDesc );
#endif
}


//-----------------------------------------------------------------------------
// Sets the flex mesh to render with this mesh
//-----------------------------------------------------------------------------
void CBufferedMeshDX8::SetFlexMesh( IMesh *pMesh, int nVertexOffsetInBytes )
{
	// FIXME: Probably are situations where we don't need to flush,
	// but this is going to look different in a very short while, so I'm not going to bother
	ShaderAPI()->FlushBufferedPrimitives();
	m_pMesh->SetFlexMesh( pMesh, nVertexOffsetInBytes );
}


//-----------------------------------------------------------------------------
// checks to see if it was rendered..
//-----------------------------------------------------------------------------
void CBufferedMeshDX8::ResetRendered()
{
	m_WasRendered = false;
}

bool CBufferedMeshDX8::WasNotRendered() const
{
	return !m_WasRendered;
}


//-----------------------------------------------------------------------------
// "Draws" it
//-----------------------------------------------------------------------------
void CBufferedMeshDX8::Draw( int nFirstIndex, int nIndexCount )
{
	if ( !ShaderUtil()->OnDrawMesh( this, nFirstIndex, nIndexCount ) )
	{
		m_WasRendered = true;
		MarkAsDrawn();
		return;
	}
	
	Assert( !m_IsFlushing && !m_WasRendered );

	// Gotta draw all of the buffered mesh
	Assert( (nFirstIndex == -1) && (nIndexCount == 0) );

	// No need to draw it more than once...
	m_WasRendered = true;

	// We've got something to flush
	m_FlushNeeded = true;

	// Less than 0 indices indicates we were using a standard buffer
	if ( m_pMesh->HasFlexMesh() || !ShaderUtil()->GetConfig().bBufferPrimitives )
	{
		ShaderAPI()->FlushBufferedPrimitives();
	}
}


//-----------------------------------------------------------------------------
// Sets the primitive mode
//-----------------------------------------------------------------------------

void CBufferedMeshDX8::SetPrimitiveType( MaterialPrimitiveType_t type )
{
	Assert( IsX360() || ( type != MATERIAL_INSTANCED_QUADS ) );
	Assert( type != MATERIAL_HETEROGENOUS );

	if (type != GetPrimitiveType())
	{
		ShaderAPI()->FlushBufferedPrimitives();
		m_pMesh->SetPrimitiveType(type);
	}
}

MaterialPrimitiveType_t CBufferedMeshDX8::GetPrimitiveType( ) const
{
	return m_pMesh->GetPrimitiveType();
}


//-----------------------------------------------------------------------------
// Locks/unlocks the entire mesh
//-----------------------------------------------------------------------------
void CBufferedMeshDX8::LockMesh( int nVertexCount, int nIndexCount, MeshDesc_t& desc )
{
	ShaderUtil()->SyncMatrices();

	Assert( m_pMesh );
	Assert( m_WasRendered );

	// Do some pre-lock processing
	m_pMesh->PreLock();

	// for tristrips, gotta make degenerate ones...
	m_ExtraIndices = 0;
	bool tristripFixup = (m_pMesh->IndexCount() != 0) && 
		(m_pMesh->GetPrimitiveType() == MATERIAL_TRIANGLE_STRIP);
	if (tristripFixup)
	{
		m_ExtraIndices = (m_pMesh->IndexCount() & 0x1) != 0 ? 3 : 2;
		nIndexCount += m_ExtraIndices;
	}

	// Flush if we gotta
	if (!m_pMesh->HasEnoughRoom(nVertexCount, nIndexCount))
	{
		ShaderAPI()->FlushBufferedPrimitives();
	}

	m_pMesh->LockMesh( nVertexCount, nIndexCount, desc );

//	This is taken care of in the function above.
//	CBaseMeshDX8::m_bMeshLocked = true;

	// Deal with fixing up the tristrip..
	if ( tristripFixup && desc.m_nIndexSize )
	{
		char buf[32];
		if (DebugTrace())
		{
			if (m_ExtraIndices == 3)
				sprintf(buf,"Link Index: %d %d\n", m_LastIndex, m_LastIndex);
			else
				sprintf(buf,"Link Index: %d\n", m_LastIndex);
			Plat_DebugString(buf);
		}
		*desc.m_pIndices++ = m_LastIndex;
		if (m_ExtraIndices == 3)
		{
			*desc.m_pIndices++ = m_LastIndex;
		}

		// Leave room for the last padding index
		++desc.m_pIndices;
	}

	m_WasRendered = false;

#ifdef DEBUG_BUFFERED_MESHES
	if (m_BufferedStateSet)
	{
		BufferedState_t compare;
		ShaderAPI()->GetBufferedState( compare );
		Assert( !memcmp( &compare, &m_BufferedState, sizeof(compare) ) );
	}
	else
	{
		ShaderAPI()->GetBufferedState( m_BufferedState );
		m_BufferedStateSet = true;
	}
#endif
}


//-----------------------------------------------------------------------------
// Locks/unlocks the entire mesh
//-----------------------------------------------------------------------------
void CBufferedMeshDX8::UnlockMesh( int nVertexCount, int nIndexCount, MeshDesc_t& desc )
{
	Assert( m_pMesh );

	// Gotta fix up the first index to batch strips reasonably
	if ((m_pMesh->GetPrimitiveType() == MATERIAL_TRIANGLE_STRIP) && desc.m_nIndexSize )
	{
		if (m_ExtraIndices > 0)
		{
			*(desc.m_pIndices - 1) = *desc.m_pIndices;

			if (DebugTrace())
			{
				char buf[32];
				sprintf(buf,"Link Index: %d\n", *desc.m_pIndices);
				Plat_DebugString(buf);
			}
		}
		
		// Remember the last index for next time 
		m_LastIndex = desc.m_pIndices[nIndexCount - 1];

		nIndexCount += m_ExtraIndices;
	}

	m_pMesh->UnlockMesh( nVertexCount, nIndexCount, desc );

//	This is taken care of in the function above.
//	CBaseMeshDX8::m_bMeshLocked = false;
}


//-----------------------------------------------------------------------------
// Renders a pass
//-----------------------------------------------------------------------------

void CBufferedMeshDX8::RenderPass()
{
	// this should never be called!
	Assert(0);
}

//-----------------------------------------------------------------------------
// Flushes queued data
//-----------------------------------------------------------------------------

void CBufferedMeshDX8::Flush( )
{
	// If you are hitting this assert you are causing a flush between a 
	// meshbuilder begin/end and you are more than likely losing rendering data.
	AssertOnce( !CBaseMeshDX8::m_bMeshLocked );

	if ( m_pMesh && !m_IsFlushing && m_FlushNeeded )
	{
		VPROF( "CBufferedMeshDX8::Flush" );

#ifdef DEBUG_BUFFERED_MESHES
		if( m_BufferedStateSet )
		{
			BufferedState_t compare;
			ShaderAPI()->GetBufferedState( compare );
			Assert( !memcmp( &compare, &m_BufferedState, sizeof(compare) ) );
			m_BufferedStateSet = false;
		}
#endif

		m_IsFlushing = true;

		// Actually draws the data using the mesh's material
		static_cast<IMesh*>(m_pMesh)->Draw();

		m_IsFlushing = false;
		m_FlushNeeded = false;

		m_pMesh->SetFlexMesh( NULL, 0 );
	}
}

//-----------------------------------------------------------------------------
//
// Mesh manager implementation
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CMeshMgr::CMeshMgr() : 
	m_pDynamicIndexBuffer(0), 
	m_DynamicTempMesh(true), 
	m_pVertexIDBuffer(0),
	m_pCurrentVertexBuffer( NULL ),
	m_CurrentVertexFormat( 0 ),
	m_pCurrentIndexBuffer( NULL ),
	m_DynamicIndexBuffer( SHADER_BUFFER_TYPE_DYNAMIC, MATERIAL_INDEX_FORMAT_16BIT, INDEX_BUFFER_SIZE, "dynamic" ),
	m_DynamicVertexBuffer( SHADER_BUFFER_TYPE_DYNAMIC, VERTEX_FORMAT_UNKNOWN, DYNAMIC_VERTEX_BUFFER_MEMORY, "dynamic" )
{
	m_bUseFatVertices = false;
	m_nIndexBufferOffset = 0;
	memset( m_pVertexBufferOffset, 0, sizeof(m_pVertexBufferOffset) );
	memset( m_pCurrentVertexStride, 0, sizeof(m_pCurrentVertexStride) );
	memset( m_pFirstVertex, 0, sizeof(m_pFirstVertex) );
	memset( m_pVertexCount, 0, sizeof(m_pVertexCount) );
	m_nUnusedVertexFields = 0;
	m_nUnusedTextureCoords = 0;
	m_pZeroVertexBuffer = NULL;
}

CMeshMgr::~CMeshMgr()
{
}


//-----------------------------------------------------------------------------
// Initialize, shutdown
//-----------------------------------------------------------------------------
void CMeshMgr::Init()
{
	m_DynamicMesh.Init( 0 );
	m_DynamicFlexMesh.Init( 1 );

	CreateDynamicIndexBuffer();

	// If we're running in vs3.0, allocate a vertexID buffer
	CreateVertexIDBuffer();

	CreateZeroVertexBuffer();
		
	m_BufferedMode = !IsX360();
}

void CMeshMgr::Shutdown()
{
	CleanUp();
}


//-----------------------------------------------------------------------------
// Task switch...
//-----------------------------------------------------------------------------
void CMeshMgr::ReleaseBuffers()
{
	if ( IsPC() && mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: CMeshMgr::ReleaseBuffers\n" );
	}

	CleanUp();
	m_DynamicMesh.Reset( );
	m_DynamicFlexMesh.Reset( );
}

void CMeshMgr::RestoreBuffers()
{
	if ( IsPC() && mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: CMeshMgr::RestoreBuffers\n" );
	}
	Init();
}


//-----------------------------------------------------------------------------
// Cleans up vertex and index buffers
//-----------------------------------------------------------------------------
void CMeshMgr::CleanUp()
{
	DestroyDynamicIndexBuffer();

	DestroyVertexBuffers();

	// If we're running in vs3.0, allocate a vertexID buffer
	DestroyVertexIDBuffer();
	DestroyZeroVertexBuffer();
}

//-----------------------------------------------------------------------------
// Fills a vertexID buffer
//-----------------------------------------------------------------------------
void CMeshMgr::FillVertexIDBuffer( CVertexBuffer *pVertexIDBuffer, int nCount )
{
	if ( IsX360() )
		return;

	// Fill the buffer with the values 0->(nCount-1)
	int nBaseVertexIndex = 0;
	float *pBuffer = (float*)pVertexIDBuffer->Lock( nCount, nBaseVertexIndex );	
	for ( int i = 0; i < nCount; ++i )
	{
		*pBuffer++ = (float)i;
	}
	pVertexIDBuffer->Unlock( nCount );
}

//-----------------------------------------------------------------------------
// Creates, destroys the dynamic index buffer
//-----------------------------------------------------------------------------
void CMeshMgr::CreateDynamicIndexBuffer()
{
	DestroyDynamicIndexBuffer();
	SafeAssign( &m_pDynamicIndexBuffer, new CIndexBuffer( Dx9Device(), INDEX_BUFFER_SIZE, ShaderAPI()->UsingSoftwareVertexProcessing(), true ) );
}

void CMeshMgr::DestroyDynamicIndexBuffer()
{
	SafeRelease( &m_pDynamicIndexBuffer );
}

//-----------------------------------------------------------------------------
// Creates, destroys the vertexID buffer
//-----------------------------------------------------------------------------
void CMeshMgr::CreateVertexIDBuffer()
{
	if ( IsX360() )
		return;

	DestroyVertexIDBuffer();

	// Track mesh allocations
	g_VBAllocTracker->TrackMeshAllocations( "CreateVertexIDBuffer" );
	if ( g_pHardwareConfig->HasFastVertexTextures() )
	{
		m_pVertexIDBuffer = new CVertexBuffer( Dx9Device(), 0, 0, sizeof(float), 
			VERTEX_BUFFER_SIZE, TEXTURE_GROUP_STATIC_VERTEX_BUFFER_OTHER, ShaderAPI()->UsingSoftwareVertexProcessing() );
		FillVertexIDBuffer( m_pVertexIDBuffer, VERTEX_BUFFER_SIZE );
	}
	g_VBAllocTracker->TrackMeshAllocations( NULL );
}

void CMeshMgr::CreateZeroVertexBuffer()
{
	if ( !m_pZeroVertexBuffer )
	{
		// In GL glVertexAttribPointer() doesn't support strides of 0, so we need to allocate a dummy vertex buffer large enough to handle 16-bit indices with a stride of 4 byte per vertex, plus a bit more for safety (in case basevertexindex is > 0).
		// We could also try just disabling any vertex attribs that fetch from stream 2 and need 0's, but AMD reports this could hit a slow path in the driver. Argh.
		uint nBufSize = IsOpenGL() ? ( 65536 * 2 * 4 ) : 4096;
		HRESULT hr = Dx9Device()->CreateVertexBuffer( nBufSize, D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &m_pZeroVertexBuffer, NULL );
		if ( !FAILED( hr ) )
		{
			void *pData = NULL;
			m_pZeroVertexBuffer->Lock( 0, nBufSize, &pData, D3DLOCK_NOSYSLOCK );
			if ( pData )
			{
				V_memset( pData, 0, nBufSize );
				m_pZeroVertexBuffer->Unlock();
			}
		}
	}
}

void CMeshMgr::DestroyZeroVertexBuffer()
{
	if ( m_pZeroVertexBuffer )
	{
		m_pZeroVertexBuffer->Release();
		m_pZeroVertexBuffer = NULL;
	}
}

void CMeshMgr::DestroyVertexIDBuffer()
{
	if ( m_pVertexIDBuffer )
	{
		delete m_pVertexIDBuffer;
		m_pVertexIDBuffer = NULL;
	}
}

CVertexBuffer *CMeshMgr::GetVertexIDBuffer( )
{
	return m_pVertexIDBuffer;
}


//-----------------------------------------------------------------------------
// Unused vertex fields
//-----------------------------------------------------------------------------
void CMeshMgr::MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords )
{
	m_nUnusedVertexFields = nFlags;
	m_nUnusedTextureCoords = 0;
	for ( int i = 0; i < nTexCoordCount; ++i )
	{
		if ( pUnusedTexCoords[i] )
		{
			m_nUnusedTextureCoords |= ( 1 << i );
		}
	}
}


//-----------------------------------------------------------------------------
// Is the mesh dynamic?
//-----------------------------------------------------------------------------
bool CMeshMgr::IsDynamicMesh( IMesh* pMesh ) const
{
	return ( pMesh == &m_DynamicMesh ) || ( pMesh == &m_DynamicFlexMesh );
}

bool CMeshMgr::IsBufferedDynamicMesh( IMesh* pMesh ) const
{
	return ( pMesh == &m_BufferedMesh );
}

bool CMeshMgr::IsDynamicVertexBuffer( IVertexBuffer *pVertexBuffer ) const
{
	return ( pVertexBuffer == &m_DynamicVertexBuffer );
}

bool CMeshMgr::IsDynamicIndexBuffer( IIndexBuffer *pIndexBuffer ) const
{
	return ( pIndexBuffer == &m_DynamicIndexBuffer );
}

//-----------------------------------------------------------------------------
// Discards the dynamic vertex and index buffer
//-----------------------------------------------------------------------------
void CMeshMgr::DiscardVertexBuffers()
{
	VPROF_BUDGET( "CMeshMgr::DiscardVertexBuffers", VPROF_BUDGETGROUP_SWAP_BUFFERS );
	// This shouldn't be necessary, but it seems to be on GeForce 2
	// It helps when running WC and the engine simultaneously.
	ResetMeshRenderState();

	if ( !g_pShaderDeviceDx8->IsDeactivated() )
	{
		for (int i = m_DynamicVertexBuffers.Count(); --i >= 0; )
		{
			m_DynamicVertexBuffers[i].m_pBuffer->FlushAtFrameStart();
		}
		m_pDynamicIndexBuffer->FlushAtFrameStart();
	}
}


//-----------------------------------------------------------------------------
// Releases all dynamic vertex buffers
//-----------------------------------------------------------------------------
void CMeshMgr::DestroyVertexBuffers()
{
	// Necessary for cleanup
	RECORD_COMMAND( DX8_SET_STREAM_SOURCE, 4 );
	RECORD_INT( -1 );
	RECORD_INT( 0 );
	RECORD_INT( 0 );
	RECORD_INT( 0 );
	D3DSetStreamSource( 0, 0, 0, 0 );

	RECORD_COMMAND( DX8_SET_STREAM_SOURCE, 4 );
	RECORD_INT( -1 );
	RECORD_INT( 1 );
	RECORD_INT( 0 );
	RECORD_INT( 0 );
	D3DSetStreamSource( 1, 0, 0, 0 );

	RECORD_COMMAND( DX8_SET_STREAM_SOURCE, 4 );
	RECORD_INT( -1 );
	RECORD_INT( 2 );
	RECORD_INT( 0 );
	RECORD_INT( 0 );
	D3DSetStreamSource( 2, 0, 0, 0 );

#ifndef _X360
	RECORD_COMMAND( DX8_SET_STREAM_SOURCE, 4 );
	RECORD_INT( -1 );
	RECORD_INT( 3 );
	RECORD_INT( 0 );
	RECORD_INT( 0 );
	D3DSetStreamSource( 3, 0, 0, 0 );
#endif

	for (int i = m_DynamicVertexBuffers.Count(); --i >= 0; )
	{
		if (m_DynamicVertexBuffers[i].m_pBuffer)
		{
			delete m_DynamicVertexBuffers[i].m_pBuffer;
		}
	}
	m_DynamicVertexBuffers.RemoveAll();
	m_DynamicMesh.Reset();
	m_DynamicFlexMesh.Reset();
}


//-----------------------------------------------------------------------------
// Flushes the dynamic mesh
//-----------------------------------------------------------------------------
void CMeshMgr::Flush()
{
	if ( IsPC() )
	{
		m_BufferedMesh.HandleLateCreation();
		m_BufferedMesh.Flush();
	}
}


//-----------------------------------------------------------------------------
// Creates, destroys static meshes
//-----------------------------------------------------------------------------
IMesh* CMeshMgr::CreateStaticMesh( VertexFormat_t format, const char *pTextureBudgetGroup, IMaterial * pMaterial )
{
	// FIXME: Use a fixed-size allocator
	CMeshDX8* pNewMesh = new CMeshDX8( pTextureBudgetGroup );
	pNewMesh->SetVertexFormat( format );
	if ( pMaterial != NULL )
	{
		pNewMesh->SetMorphFormat( pMaterial->GetMorphFormat() );
		pNewMesh->SetMaterial( pMaterial );
	}
	return pNewMesh;
}

void CMeshMgr::DestroyStaticMesh( IMesh* pMesh )
{
	// Don't destroy the dynamic mesh!
	Assert( !IsDynamicMesh( pMesh ) );
	CBaseMeshDX8* pMeshImp = static_cast<CBaseMeshDX8*>(pMesh);
	if (pMeshImp)
	{
		delete pMeshImp;
	}
}

//-----------------------------------------------------------------------------
// Gets at the *real* dynamic mesh
//-----------------------------------------------------------------------------
IMesh* CMeshMgr::GetActualDynamicMesh( VertexFormat_t format )
{
	m_DynamicMesh.SetVertexFormat( format );
	return &m_DynamicMesh;
}

//-----------------------------------------------------------------------------
// Copy a static mesh index buffer to a dynamic mesh index buffer
//-----------------------------------------------------------------------------
void CMeshMgr::CopyStaticMeshIndexBufferToTempMeshIndexBuffer( CTempMeshDX8 *pDstIndexMesh,
															   CMeshDX8 *pSrcIndexMesh )
{
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s", __FUNCTION__ );

	Assert( !pSrcIndexMesh->IsDynamic() );
	int nIndexCount = pSrcIndexMesh->IndexCount();
	
	CMeshBuilder dstMeshBuilder;
	dstMeshBuilder.Begin( pDstIndexMesh, pSrcIndexMesh->GetPrimitiveType(), 0, nIndexCount );
	CIndexBuffer *srcIndexBuffer = pSrcIndexMesh->GetIndexBuffer();
	int dummy = 0;
	unsigned short *srcIndexArray = srcIndexBuffer->Lock( false, nIndexCount, dummy, 0 );
	int i;
	for( i = 0; i < nIndexCount; i++ )
	{
		dstMeshBuilder.Index( srcIndexArray[i] );
		dstMeshBuilder.AdvanceIndex();
	}
	srcIndexBuffer->Unlock( 0 );
	dstMeshBuilder.End();
}


IMesh *CMeshMgr::GetFlexMesh()
{
	if ( g_pMaterialSystemHardwareConfig->SupportsPixelShaders_2_b() )
	{
		// FIXME: Kinda ugly size.. 28 bytes
		m_DynamicFlexMesh.SetVertexFormat( VERTEX_POSITION | VERTEX_NORMAL | VERTEX_WRINKLE | VERTEX_FORMAT_USE_EXACT_FORMAT );
	}
	else
	{
		// Same size as a pair of float3s (24 bytes)
		m_DynamicFlexMesh.SetVertexFormat( VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_USE_EXACT_FORMAT );
	}
	return &m_DynamicFlexMesh;
}

//-----------------------------------------------------------------------------
// Gets at the dynamic mesh
//-----------------------------------------------------------------------------
IMesh* CMeshMgr::GetDynamicMesh( IMaterial* pMaterial, VertexFormat_t vertexFormat, int nHWSkinBoneCount,
	bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride )
{
	tmZoneFiltered( TELEMETRY_LEVEL0, 50, TMZF_NONE, "%s", __FUNCTION__ );

	Assert( (pMaterial == NULL) || ((IMaterialInternal *)pMaterial)->IsRealTimeVersion() );

	if ( IsX360() )
	{
		buffered = false;
	}

	// Can't be buffered if we're overriding the buffers
	if ( pVertexOverride || pIndexOverride )
	{
		buffered = false;
	}

	// When going from buffered to unbuffered mode, need to flush..
	if ((m_BufferedMode != buffered) && m_BufferedMode)
	{
		m_BufferedMesh.SetMesh(0);
	}
	m_BufferedMode = buffered;

	IMaterialInternal* pMatInternal = static_cast<IMaterialInternal*>(pMaterial);

	bool needTempMesh = ShaderAPI()->IsInSelectionMode();

#ifdef DRAW_SELECTION
	if( g_bDrawSelection )
	{
		needTempMesh = true;
	}
#endif

	CBaseMeshDX8* pMesh;

	if ( needTempMesh )
	{
		// These haven't been implemented yet for temp meshes!
		// I'm not a hundred percent sure how to implement them; it would
		// involve a lock and a copy at least, which would stall the entire
		// rendering pipeline.
		Assert( !pVertexOverride );
		
		if( pIndexOverride )
		{
			CopyStaticMeshIndexBufferToTempMeshIndexBuffer( &m_DynamicTempMesh,
				( CMeshDX8 * )pIndexOverride );
		}
		pMesh = &m_DynamicTempMesh;
	}
	else
	{
		pMesh = &m_DynamicMesh;
	}

	if ( m_BufferedMode )
	{
		Assert( !m_BufferedMesh.WasNotRendered() );
		m_BufferedMesh.SetMesh( pMesh );
		pMesh = &m_BufferedMesh;
	}

	if( !pVertexOverride )
	{
		// Remove VERTEX_FORMAT_COMPRESSED from the material's format (dynamic meshes don't
		// support compression, and all materials should support uncompressed verts too)
		VertexFormat_t materialFormat = pMatInternal->GetVertexFormat() & ~VERTEX_FORMAT_COMPRESSED;
		VertexFormat_t fmt = ( vertexFormat != 0 ) ? vertexFormat : materialFormat;
		if ( vertexFormat != 0 )
		{
			int nVertexFormatBoneWeights = NumBoneWeights( vertexFormat );
			if ( nHWSkinBoneCount < nVertexFormatBoneWeights )
			{
				nHWSkinBoneCount = nVertexFormatBoneWeights;
			}
		}

		// Force the requested number of bone weights
		fmt &= ~VERTEX_BONE_WEIGHT_MASK;
		if ( nHWSkinBoneCount > 0 )
		{
			fmt |= VERTEX_BONEWEIGHT( 2 );
			fmt |= VERTEX_BONE_INDEX;
		}

		pMesh->SetVertexFormat( fmt );
	}
	else
	{
		CBaseMeshDX8 *pDX8Mesh = static_cast<CBaseMeshDX8*>(pVertexOverride);
		pMesh->SetVertexFormat( pDX8Mesh->GetVertexFormat() );
	}
	pMesh->SetMorphFormat( pMatInternal->GetMorphFormat() );
	pMesh->SetMaterial( pMatInternal );

	// Note this works because we're guaranteed to not be using a buffered mesh
	// when we have overrides on
	// FIXME: Make work for temp meshes
	if ( pMesh == &m_DynamicMesh )
	{
		CBaseMeshDX8* pBaseVertex = static_cast<CBaseMeshDX8*>( pVertexOverride );
		if ( pBaseVertex )
		{
			m_DynamicMesh.OverrideVertexBuffer( pBaseVertex->GetVertexBuffer() );
		}

		CBaseMeshDX8* pBaseIndex = static_cast<CBaseMeshDX8*>( pIndexOverride );
		if ( pBaseIndex )
		{
			m_DynamicMesh.OverrideIndexBuffer( pBaseIndex->GetIndexBuffer() );
		}
	}

	return pMesh;
}


//-----------------------------------------------------------------------------
// Used to construct vertex data
//-----------------------------------------------------------------------------
void CMeshMgr::ComputeVertexDescription( unsigned char* pBuffer, 
	VertexFormat_t vertexFormat, MeshDesc_t& desc ) const
{
	ComputeVertexDesc( pBuffer, vertexFormat, (VertexDesc_t &)desc );
}


//-----------------------------------------------------------------------------
// Computes the vertex format
//-----------------------------------------------------------------------------
VertexFormat_t CMeshMgr::ComputeVertexFormat( unsigned int flags, 
			int nTexCoordArraySize, int* pTexCoordDimensions, int numBoneWeights,
			int userDataSize ) const
{
	// Construct a bitfield that makes sense and is unique from the standard FVF formats
	VertexFormat_t fmt = flags & ~VERTEX_FORMAT_USE_EXACT_FORMAT;

	if ( g_pHardwareConfig->SupportsCompressedVertices() == VERTEX_COMPRESSION_NONE )
	{
		// Vertex compression is disabled - make sure all materials
		// say "No!" to compressed verts ( tested in IsValidVertexFormat() )
		fmt &= ~VERTEX_FORMAT_COMPRESSED;
	}

	// This'll take 3 bits at most
	Assert( numBoneWeights <= 4 );

	if ( numBoneWeights > 0 )
	{
		fmt |= VERTEX_BONEWEIGHT( 2 ); // Always exactly two weights
	}

	// Size is measured in # of floats
	Assert( userDataSize <= 4 );
	fmt |= VERTEX_USERDATA_SIZE(userDataSize);

	// NOTE: If pTexCoordDimensions isn't specified, then nTexCoordArraySize
	// is interpreted as meaning that we have n 2D texcoords in the first N texcoord slots
	nTexCoordArraySize = min( nTexCoordArraySize, (int)VERTEX_MAX_TEXTURE_COORDINATES );
	for ( int i = 0; i < nTexCoordArraySize; ++i )
	{
		if ( pTexCoordDimensions )
		{
			Assert( pTexCoordDimensions[i] >= 0 && pTexCoordDimensions[i] <= 4 );
			fmt |= VERTEX_TEXCOORD_SIZE( (TextureStage_t)i, pTexCoordDimensions[i] );
		}
		else 
		{
			fmt |= VERTEX_TEXCOORD_SIZE( (TextureStage_t)i, 2 );
		}
	}

	return fmt;
}


//-----------------------------------------------------------------------------
// Use fat vertices (for tools)
//-----------------------------------------------------------------------------
void CMeshMgr::UseFatVertices( bool bUseFat )
{
	m_bUseFatVertices = bUseFat;
}


//-----------------------------------------------------------------------------
// Returns the number of vertices we can render using the dynamic mesh
//-----------------------------------------------------------------------------
void CMeshMgr::GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices )
{
	CBaseMeshDX8 *pBaseMesh = static_cast<CBaseMeshDX8*>( pMesh );
	if ( !pBaseMesh )
	{
		*pMaxVerts = 0;
		*pMaxIndices = m_pDynamicIndexBuffer->IndexCount();
		return;
	}

	if ( IsBufferedDynamicMesh( pMesh ) )
	{
		pBaseMesh = (CBaseMeshDX8*)static_cast<CBufferedMeshDX8*>( pBaseMesh )->GetMesh();
		pMesh = pBaseMesh;
	}

	// Static mesh? Max you can use is 65535
	if ( !IsDynamicMesh( pMesh ) )
	{
		*pMaxVerts = 65535;
		*pMaxIndices = 65535;
		return;
	}

	CVertexBuffer *pVertexBuffer = pBaseMesh->GetVertexBuffer();
	CIndexBuffer *pIndexBuffer = pBaseMesh->GetIndexBuffer();

	if ( !pVertexBuffer )
	{
		*pMaxVerts = 0;
		*pMaxIndices = 0;
		return;
	}

	if ( !bMaxUntilFlush )
	{
		*pMaxVerts = ShaderAPI()->GetCurrentDynamicVBSize() / pVertexBuffer->VertexSize();
		if ( *pMaxVerts > 65535 )
		{
			*pMaxVerts = 65535;
		}
		*pMaxIndices = pIndexBuffer ? pIndexBuffer->IndexCount() : 0;
		return;
	}

	*pMaxVerts = pVertexBuffer->NumVerticesUntilFlush();
	*pMaxIndices = pIndexBuffer ? pIndexBuffer->IndexCount() - pIndexBuffer->IndexPosition() : 0;
	if ( *pMaxVerts == 0 )
	{
		*pMaxVerts = ShaderAPI()->GetCurrentDynamicVBSize() / pVertexBuffer->VertexSize();
	}
	if ( *pMaxVerts > 65535 )
	{
		*pMaxVerts = 65535;
	}
	if ( *pMaxIndices == 0 )
	{
		*pMaxIndices = pIndexBuffer ? pIndexBuffer->IndexCount() : 0;
	}
}

int CMeshMgr::GetMaxVerticesToRender( IMaterial *pMaterial )
{
	Assert( (pMaterial == NULL) || ((IMaterialInternal *)pMaterial)->IsRealTimeVersion() );

	// Be conservative, assume no compression (in here, we don't know if the caller will used a compressed VB or not)
	// FIXME: allow the caller to specify which compression type should be used to compute size from the vertex format
	//        (this can vary between multiple VBs/Meshes using the same material)
	VertexFormat_t fmt = pMaterial->GetVertexFormat() & ~VERTEX_FORMAT_COMPRESSED;
	int nVertexSize = VertexFormatSize( fmt );
	if ( nVertexSize == 0 )
	{
		// unable to determine vertex format information, possibly due to device loss.
		Warning( "bad vertex size for material %s\n", pMaterial->GetName() );
		return 0;
	}

	int nMaxVerts = ShaderAPI()->GetCurrentDynamicVBSize() / nVertexSize;
	return MIN( nMaxVerts, 65535 );
}

int CMeshMgr::GetMaxIndicesToRender( )
{
	return INDEX_BUFFER_SIZE;
}

//-----------------------------------------------------------------------------
// Returns a vertex buffer appropriate for the flags
//-----------------------------------------------------------------------------
CVertexBuffer *CMeshMgr::FindOrCreateVertexBuffer( int nDynamicBufferId, VertexFormat_t vertexFormat )
{
	int vertexSize = VertexFormatSize( vertexFormat );

	while ( m_DynamicVertexBuffers.Count() <= nDynamicBufferId )
	{
		// Track VB allocations (override any prior allocator string set higher up on the callstack)
		g_VBAllocTracker->TrackMeshAllocations( NULL );
		g_VBAllocTracker->TrackMeshAllocations( "CMeshMgr::FindOrCreateVertexBuffer (dynamic VB)" );

		// create the single 1MB dynamic vb that will be shared amongst all consumers
		// the correct thing is to use the largest expected vertex format size of max elements, but this
		// creates an undesirably large buffer - instead create the buffer we want, and fix consumers that bork
		// NOTE: GetCurrentDynamicVBSize returns a smaller value during level transitions
		int nBufferMemory = ShaderAPI()->GetCurrentDynamicVBSize();
		int nIndex = m_DynamicVertexBuffers.AddToTail();
		m_DynamicVertexBuffers[nIndex].m_VertexSize = 0;
		m_DynamicVertexBuffers[nIndex].m_pBuffer = new CVertexBuffer( Dx9Device(), 0, 0, 
			nBufferMemory / VERTEX_BUFFER_SIZE, VERTEX_BUFFER_SIZE, TEXTURE_GROUP_STATIC_VERTEX_BUFFER_OTHER, ShaderAPI()->UsingSoftwareVertexProcessing(), true ); 

		g_VBAllocTracker->TrackMeshAllocations( NULL );
	}
	
	if ( m_DynamicVertexBuffers[nDynamicBufferId].m_VertexSize != vertexSize )
	{
		// provide caller with dynamic vb in expected format
		// NOTE: GetCurrentDynamicVBSize returns a smaller value during level transitions
		int nBufferMemory = ShaderAPI()->GetCurrentDynamicVBSize();
		m_DynamicVertexBuffers[nDynamicBufferId].m_VertexSize = vertexSize;
		m_DynamicVertexBuffers[nDynamicBufferId].m_pBuffer->ChangeConfiguration( vertexSize, nBufferMemory );

		// size changed means stream stride needs update
		// mark cached stream state as invalid to reset stream
		if ( nDynamicBufferId == 0 )
		{
			g_pLastVertex = NULL;
		}
	}

	return m_DynamicVertexBuffers[nDynamicBufferId].m_pBuffer;
}

CIndexBuffer *CMeshMgr::GetDynamicIndexBuffer()
{
	return m_pDynamicIndexBuffer;
}

IVertexBuffer *CMeshMgr::GetDynamicVertexBuffer( IMaterial *pMaterial, bool buffered )
{
	Assert( 0 );
	return NULL;
//	return ( IMeshDX8 * )GetDynamicMesh( pMaterial, buffered, NULL, NULL );
}

IIndexBuffer *CMeshMgr::GetDynamicIndexBuffer( IMaterial *pMaterial, bool buffered )
{
	Assert( 0 );
	return NULL;
//	return ( IMeshDX8 * )GetDynamicMesh( pMaterial, buffered, NULL, NULL );
}


//-----------------------------------------------------------------------------
IVertexBuffer *CMeshMgr::CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup )
{
	// FIXME: Use a fixed-size allocator
	CVertexBufferDx8 *pNewVertexBuffer = new CVertexBufferDx8( type, fmt, nVertexCount, pBudgetGroup );
	return pNewVertexBuffer;
}

IIndexBuffer *CMeshMgr::CreateIndexBuffer( ShaderBufferType_t bufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup )
{
	switch( bufferType )
	{
	case SHADER_BUFFER_TYPE_STATIC:
	case SHADER_BUFFER_TYPE_DYNAMIC:
		{
			CIndexBufferDx8 *pIndexBuffer = new CIndexBufferDx8( bufferType, fmt, nIndexCount, pBudgetGroup );
			return pIndexBuffer;
		}
	case SHADER_BUFFER_TYPE_STATIC_TEMP:
	case SHADER_BUFFER_TYPE_DYNAMIC_TEMP:
		Assert( 0 );
		return NULL;
	default:
		Assert( 0 );
		return NULL;
	}
}

void CMeshMgr::DestroyVertexBuffer( IVertexBuffer *pVertexBuffer )
{
	if ( pVertexBuffer && !IsDynamicVertexBuffer( pVertexBuffer ) )
	{
		delete pVertexBuffer;
	}
}

void CMeshMgr::DestroyIndexBuffer( IIndexBuffer *pIndexBuffer )
{
	if ( pIndexBuffer && !IsDynamicIndexBuffer( pIndexBuffer ) )
	{
		delete pIndexBuffer;
	}
}

// Do we need to specify the stream here in the case of locking multiple dynamic VBs on different streams?
IVertexBuffer *CMeshMgr::GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBuffered )
{
	if ( IsX360() )
	{
		bBuffered = false;
	}

	if ( CompressionType( vertexFormat ) != VERTEX_COMPRESSION_NONE )
	{
		// UNDONE: support compressed dynamic meshes if needed (pro: less VB memory, con: time spent compressing)
		DebuggerBreak();
		return NULL;
	}

	// MESHFIXME
#if 0
	if ( ( m_BufferedMode != bBuffered ) && m_BufferedMode )
	{
		m_BufferedIndexBuffer.SetIndexBuffer( NULL );
	}
#endif

	m_BufferedMode = bBuffered;
	Assert( !m_BufferedMode ); // MESHFIXME: don't deal with buffered VBs yet.

	bool needTempMesh = ShaderAPI()->IsInSelectionMode();
	
#ifdef DRAW_SELECTION
	if( g_bDrawSelection )
	{
		needTempMesh = true;
	}
#endif

	Assert( !needTempMesh ); // MESHFIXME: don't support temp meshes here yet.

	CVertexBufferDx8 *pVertexBuffer;

	if ( needTempMesh )
	{
		Assert( 0 ); // MESHFIXME: don't do this yet.
//		pVertexBuffer = &m_DynamicTempVertexBuffer;
		pVertexBuffer = NULL;
	}
	else
	{
		pVertexBuffer = &m_DynamicVertexBuffer;
	}

	if ( m_BufferedMode )
	{
		Assert( 0 ); // don't support this yet.
#if 0
		Assert( !m_BufferedMesh.WasNotRendered() );
		m_BufferedMesh.SetMesh( pMesh );
		pMesh = &m_BufferedMesh;
#endif
	}

	return pVertexBuffer;
}

IIndexBuffer *CMeshMgr::GetDynamicIndexBuffer( MaterialIndexFormat_t fmt, bool bBuffered )
{
	if ( IsX360() )
	{
		bBuffered = false;
	}

	m_BufferedMode = bBuffered;

	Assert( !m_BufferedMode );

#ifdef DBGFLAG_ASSERT
	bool needTempMesh = 
#endif
		ShaderAPI()->IsInSelectionMode();

#ifdef DRAW_SELECTION
	if( g_bDrawSelection )
	{
		needTempMesh = true;
	}
#endif

	Assert( !needTempMesh ); // don't handle this yet. MESHFIXME

	CIndexBufferBase *pIndexBuffer = &m_DynamicIndexBuffer;
	return pIndexBuffer;
}

void CMeshMgr::SetVertexIDStreamState()
{
	if ( IsX360() )
		return;

	// MESHFIXME : This path is only used for the new index/vertex buffer interfaces.
	// MESHFIXME : This path is only used for the new index/vertex buffer interfaces.
	bool bUsingVertexID = false;//IsUsingVertexID();
//	if ( bUsingVertexID != g_bUsingVertexID )
	{
		if ( bUsingVertexID )
		{
			// NOTE: Morphing doesn't work with dynamic buffers!!! BLEAH
			// It's because the indices (which are not 0 based for dynamic buffers)
			// are accessing both the vertexID buffer + the regular vertex buffer.
			// This *might* be fixable with baseVertexIndex?
			Assert( !m_pCurrentVertexBuffer->IsDynamic() );

			CVertexBuffer *pVertexIDBuffer = g_MeshMgr.GetVertexIDBuffer( );
			RECORD_COMMAND( DX8_SET_STREAM_SOURCE, 4 );
			RECORD_INT( pVertexIDBuffer->UID() );
			RECORD_INT( 3 );
			RECORD_INT( 0 );
			RECORD_INT( pVertexIDBuffer->VertexSize() );

			D3DSetStreamSource( 3, pVertexIDBuffer->GetInterface(), 0, pVertexIDBuffer->VertexSize() );
			pVertexIDBuffer->HandlePerFrameTextureStats( ShaderAPI()->GetCurrentFrameCounter() );
		}
		else
		{
			RECORD_COMMAND( DX8_SET_STREAM_SOURCE, 4 );
			RECORD_INT( -1 );	// vertex buffer id
			RECORD_INT( 3 );	// stream
			RECORD_INT( 0 );	// vertex offset
			RECORD_INT( 0 );	// vertex size

			D3DSetStreamSource( 3, 0, 0, 0 );
		}
		g_bUsingVertexID = bUsingVertexID;
	}
}

void CMeshMgr::SetColorStreamState()
{
	if ( g_pLastColorMesh )
	{
		RECORD_COMMAND( DX8_SET_STREAM_SOURCE, 4 );
		RECORD_INT( -1 );	// vertex buffer id
		RECORD_INT( 1 );	// stream
		RECORD_INT( 0 );	// vertex offset
		RECORD_INT( 0 );	// vertex size

		D3DSetStreamSource( 1, 0, 0, 0 );
	}
	g_pLastColorMesh = NULL;
	g_nLastColorMeshVertOffsetInBytes = 0;
}

void CMeshMgr::SetVertexStreamState( int nVertOffsetInBytes, int nVertexStride )
{
	// Calls in here assume shader support...
	if ( HardwareConfig()->SupportsVertexAndPixelShaders() )
	{
		// Set a 4kb all-zero static VB into the flex/wrinkle stream with a stride of 0 bytes, so the vertex shader always reads valid floating point values (otherwise it can get NaN's/Inf's, and under OpenGL this is bad on NVidia)
		// togl requires non-zero strides, but on D3D9 we can set a stride of 0 for a little more efficiency.
		D3DSetStreamSource( 2, g_MeshMgr.GetZeroVertexBuffer(), 0, IsOpenGL() ? 4 : 0 );

		// cFlexScale.x masks flex in vertex shader
		if ( g_pHardwareConfig->Caps().m_SupportsVertexShaders_2_0 )
		{
			float c[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			ShaderAPI()->SetVertexShaderConstant( VERTEX_SHADER_FLEXSCALE, c, 1 );
		}

		g_bFlexMeshStreamSet = false;
	}

	// MESHFIXME : This path is only used for the new index/vertex buffer interfaces.
	if ( g_pLastVertex || ( g_pLastVertexBuffer != m_pCurrentVertexBuffer->GetDx9Buffer() ) || 
		( g_nLastVertOffsetInBytes != nVertOffsetInBytes ) || ( g_nLastVertStride != nVertexStride ))
	{
		Assert( m_pCurrentVertexBuffer && m_pCurrentVertexBuffer->GetDx9Buffer() );

		D3DSetStreamSource( 0, m_pCurrentVertexBuffer->GetDx9Buffer(), nVertOffsetInBytes, nVertexStride );
		m_pCurrentVertexBuffer->HandlePerFrameTextureStats( ShaderAPI()->GetCurrentFrameCounter() );

		g_pLastVertex = NULL;
		g_nLastVertStride = nVertexStride;
		g_pLastVertexBuffer = m_pCurrentVertexBuffer->GetDx9Buffer();
		g_nLastVertOffsetInBytes = nVertOffsetInBytes;
	}
}

bool CMeshMgr::SetRenderState( int nVertexOffsetInBytes, int nFirstVertexIdx, VertexFormat_t vertexFormat, int nVertexStride )
{
	// Can't set the state if we're deactivated
	if ( g_pShaderDeviceDx8->IsDeactivated() )
	{
		ResetMeshRenderState();
		return false;
	}

	// make sure the vertex format is a superset of the current material's
	// vertex format...
	// MESHFIXME : This path is only used for the new index/vertex buffer interfaces.
#if 0
	// FIXME
	if ( !IsValidVertexFormat( vertexFormat ) )
	{
		Warning( "Material %s is being applied to a model, you need $model=1 in the .vmt file!\n",
			ShaderAPI()->GetBoundMaterial()->GetName() );
		return false;
	}
#endif

	SetVertexIDStreamState();
	SetColorStreamState();
	SetVertexStreamState( nVertexOffsetInBytes, nVertexStride );
	SetIndexStreamState( nFirstVertexIdx );

	return true;
}

void CMeshMgr::BindVertexBuffer( int nStreamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions )
{
	// FIXME: Multiple stream support isn't implemented yet
	Assert( nStreamID == 0 );

	m_pCurrentVertexBuffer = static_cast< CVertexBufferDx8 * >( pVertexBuffer ); 
	m_CurrentVertexFormat = fmt;
	m_pVertexBufferOffset[nStreamID] = nOffsetInBytes;
	m_pCurrentVertexStride[nStreamID] = m_pCurrentVertexBuffer->VertexSize();
	m_pFirstVertex[nStreamID] = nFirstVertex;
	m_pVertexCount[nStreamID] = nVertexCount, 
	m_pVertexIDBuffer = NULL;
}

void CMeshMgr::BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes )
{
	m_pCurrentIndexBuffer = static_cast< CIndexBufferBase * >( pIndexBuffer );
	m_nIndexBufferOffset = nOffsetInBytes;
}

void CMeshMgr::Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount )
{
	// MESHFIXME : This path is only used for the new index/vertex buffer interfaces.
	// make sure we aren't using a morph stream for this path.
//	Assert( !IsUsingMorphData() );
//	Assert( !m_pColorMesh );

	SetRenderState( m_pVertexBufferOffset[0], /* nFirstVertexIdx */0, m_CurrentVertexFormat, m_pCurrentVertexStride[0] );

	m_PrimitiveType = MATERIAL_TRIANGLES;
	Assert( primitiveType == MATERIAL_TRIANGLES );

	m_nFirstIndex = nFirstIndex;
	m_nNumIndices = nIndexCount;

	ShaderAPI()->DrawWithVertexAndIndexBuffers();
}

void CMeshMgr::RenderPassWithVertexAndIndexBuffers( void )
{
//	LOCK_SHADERAPI(); MESHFIXME
	VPROF( "CShaderAPIDX8::RenderPassWithVertexAndIndexBuffers" );

	Assert( m_PrimitiveType != MATERIAL_HETEROGENOUS );

//	for ( int iPrim=0; iPrim < s_nPrims; iPrim++ )
	{
//		CPrimList *pPrim = &s_pPrims[iPrim];

//		if ( pPrim->m_NumIndices == 0 )
//			continue;

		if ( m_PrimitiveType == MATERIAL_POINTS )
		{
			// (For point lists, we don't actually fill in indices, but we treat it as
			// though there are indices for the list up until here).
			Assert( 0 );
//			Dx9Device()->DrawPrimitive( ComputeMode( m_PrimitiveType ), s_FirstVertex, pPrim->m_NumIndices );
		}
		else
		{
//			int numPrimitives = NumPrimitives( s_NumVertices, pPrim->m_NumIndices );

//			Warning( "CMeshMgr::RenderPassWithVertexAndIndexBuffers: DrawIndexedPrimitive: m_nFirstIndex = %d numPrimitives = %d\n", ( int )( ( CDynamiCIndexBufferDx8 * )m_pCurrentIndexBuffer )->m_FirstIndex, ( int )( m_nNumIndices / 3 ) );
			{
				VPROF( "Dx9Device()->DrawIndexedPrimitive" );
//				VPROF_INCREMENT_COUNTER( "DrawIndexedPrimitive", 1 );
//				VPROF_INCREMENT_COUNTER( "numPrimitives", numPrimitives );

//				Dx9Device()->DrawIndexedPrimitive( 
//					m_Mode,
//					m_FirstIndex,
//					s_FirstVertex, 
//					s_NumVertices,
//					pPrim->m_FirstIndex,
//					numPrimitives );

				Assert( m_nFirstIndex >= 0 );

#ifdef CHECK_INDICES
				// g_pLastVertex - this is the current vertex buffer
				// g_pLastColorMesh - this is the curent color mesh, if there is one.
				// g_pLastIndex - this is the current index buffer.
				// vertoffset : m_FirstIndex
				CIndexBufferDx8 *pIndexBuffer = assert_cast< CIndexBufferDx8 * >( m_pCurrentIndexBuffer );
				if( m_PrimitiveType == MATERIAL_TRIANGLES || m_PrimitiveType == MATERIAL_TRIANGLE_STRIP )
				{
					// FIXME: need to be able to deal with multiple stream here, but don't bother for now.
					int j;
					int numVerts = m_pVertexCount[0];
					for( j = 0; j < m_nNumIndices; j++ )
					{
						int index = pIndexBuffer->GetShadowIndex( j + m_nFirstIndex );
						Assert( index >= m_pFirstVertex[0] );
						Assert( index < m_pFirstVertex[0] + numVerts );
					}
				}
#endif // CHECK_INDICES
				Dx9Device()->DrawIndexedPrimitive( 
					ComputeMode( m_PrimitiveType ),		// Member of the D3DPRIMITIVETYPE enumerated type, describing the type of primitive to render. D3DPT_POINTLIST is not supported with this method.

					/*m_FirstIndex*/ 0,					// Offset from the start of the vertex buffer to the first vertex index. An index of 0 in the index buffer refers to this location in the vertex buffer.

					/*s_FirstVertex*/ m_pFirstVertex[0],// Minimum vertex index for vertices used during this call. This is a zero based index relative to BaseVertexIndex.
														// This is zero for now since we don't do more than one batch yet with the new mesh interface.

					/*s_NumVertices*/ m_pVertexCount[0], 
														// Number of vertices used during this call. The first vertex is located at index: BaseVertexIndex + MinIndex.
														// This is simple the number of verts in the current vertex buffer for now since we don't do more than one batch with the new mesh interface.

					m_nFirstIndex /*pPrim->m_FirstIndex*/,	// Index of the first index to use when accesssing the vertex buffer. Beginning at StartIndex to index vertices from the vertex buffer.

					m_nNumIndices / 3/*numPrimitives*/  // Number of primitives to render. The number of vertices used is a function of the primitive count and the primitive type.
					);

				Assert( CMeshDX8::s_FirstVertex == 0 );
				Assert( CMeshDX8::s_NumVertices == 0 );
			}
		}
	}
}

//-----------------------------------------------------------------------------
void CMeshMgr::SetIndexStreamState( int firstVertexIdx )
{
	CIndexBufferDx8 *pIndexBuffer = assert_cast< CIndexBufferDx8* >( m_pCurrentIndexBuffer );
	IDirect3DIndexBuffer9 *pDx9Buffer = pIndexBuffer ? pIndexBuffer->GetDx9Buffer() : NULL;
	if ( g_pLastIndex || g_pLastIndexBuffer != pDx9Buffer )
	{
		Dx9Device()->SetIndices( pDx9Buffer );
		pIndexBuffer->HandlePerFrameTextureStats( ShaderAPI()->GetCurrentFrameCounter() );

		g_pLastIndexBuffer = pDx9Buffer;
		SafeRelease( &g_pLastIndex );
		g_LastVertexIdx = -1;
	}
}


