//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifdef D3D_ASYNC_SUPPORTED

#ifndef D3DASYNC_H
#define D3DASYNC_H

#ifdef _WIN32
#pragma once
#endif

// Set this to 1 to allow d3d calls to be buffered and played back on another thread
// Slamming this off - it's causing very hot D3D9 function calls to not be inlined and contain a bunch of unused code. (Does this code even work/add real value any more?)
#define SHADERAPI_USE_SMP 0

// Set this to 1 to allow buffering of the whole frame to memory and then playback (singlethreaded).  
// This is for debugging only and is used to test the performance of just calling D3D and rendering without other CPU overhead.
#define SHADERAPI_BUFFER_D3DCALLS 0

#if SHADERAPI_BUFFER_D3DCALLS && !SHADERAPI_USE_SMP
#	error "SHADERAPI_USE_SMP must be 1 for SHADERAPI_BUFFER_D3DCALLS to work!"
#endif

#include "recording.h"
#include "strtools.h"
#include "glmgr/dxabstract.h"

#ifdef NDEBUG
#define DO_D3D(x) Dx9Device()->x
#else
#define DO_D3D(x) Dx9Device()->x
//#define DO_D3D(x) { HRESULT	hr=Dx9Device()->x; Assert( !FAILED(hr) ); }
#endif

#define PUSHBUFFER_NELEMS 4096

enum PushBufferState 
{
	PUSHBUFFER_AVAILABLE,
	PUSHBUFFER_BEING_FILLED,
	PUSHBUFFER_SUBMITTED,
	PUSHBUFFER_BEING_USED_FOR_LOCKEDDATA,
};

class PushBuffer
{
	friend class D3DDeviceWrapper;

	volatile PushBufferState m_State;
	uint32 m_BufferData[PUSHBUFFER_NELEMS];
public:
	PushBuffer(void)
	{
		m_State = PUSHBUFFER_AVAILABLE;
	}
};

// When running multithreaded, lock for write calls actually return a pointer to temporary memory
// buffer. When the buffer is later unlocked by the caller, data must be queued with the Unlock()
// that lets the d3d thread know how much data to copy from where.  One possible optimization for
// things which write a lot of data into lock buffers woudl be to proviude a way for the caller to
// occasionally check if the Lock() has been dequeued. If so, the the data pushed so far could be
// copied asynchronously into the buffer, while the caller would be told to switch to writing
// directly to the vertex buffer.
//
// another possibility would be lock()ing in advance for large ones, such as the world renderer,
// or keeping multiple locked vb's open for meshbuilder.

struct LockedBufferContext
{
	PushBuffer *m_pPushBuffer;								// if a push buffer was used to hold
															// the temporary data, this will be non-null
	void *m_pMallocedMemory;								// if memory had to be malloc'd, this will be set.

	size_t m_MallocSize;									// # of bytes malloced if mallocedmem ptr non-null

	LockedBufferContext( void )
	{
		m_pPushBuffer = NULL;
		m_pMallocedMemory = NULL;
	}

};



// push buffer commands follow
enum PushBufferCommand
{
	PBCMD_END,												// at end of push buffer
	PBCMD_SET_RENDERSTATE,									// state, val
	PBCMD_SET_TEXTURE,										// stage, txtr
	PBCMD_DRAWPRIM,											// prim type, start v, nprims
	PBCMD_DRAWINDEXEDPRIM,									// prim type, baseidx, minidx, numv, starti, pcount
	PBCMD_SET_PIXEL_SHADER,									// shaderptr
	PBCMD_SET_VERTEX_SHADER,								// shaderptr
	PBCMD_SET_PIXEL_SHADER_CONSTANT,						// startreg, nregs, data...
	PBCMD_SET_BOOLEAN_PIXEL_SHADER_CONSTANT,				// startreg, nregs, data...
	PBCMD_SET_INTEGER_PIXEL_SHADER_CONSTANT,				// startreg, nregs, data...
	PBCMD_SET_VERTEX_SHADER_CONSTANT,						// startreg, nregs, data...
	PBCMD_SET_BOOLEAN_VERTEX_SHADER_CONSTANT,				// startreg, nregs, data...
	PBCMD_SET_INTEGER_VERTEX_SHADER_CONSTANT,				// startreg, nregs, data...
	PBCMD_SET_RENDER_TARGET,								// idx, targetptr
	PBCMD_SET_DEPTH_STENCIL_SURFACE,						// surfptr
	PBCMD_SET_STREAM_SOURCE,								// idx, sptr, ofs, stride
	PBCMD_SET_INDICES,										// idxbuffer
	PBCMD_SET_SAMPLER_STATE,								// stage, state, val
	PBCMD_UNLOCK_VB,										// vptr
	PBCMD_UNLOCK_IB,										// idxbufptr
	PBCMD_SETVIEWPORT,										// vp_struct
	PBCMD_CLEAR,											// count, n rect structs, flags, color, z, stencil
	PBCMD_SET_VERTEXDECLARATION,							// vdeclptr
	PBCMD_BEGIN_SCENE,										// 
	PBCMD_END_SCENE,										// 
	PBCMD_PRESENT,											// complicated..see code
	PBCMD_SETCLIPPLANE,										// idx, 4 floats
	PBCMD_STRETCHRECT,										// see code
	PBCMD_ASYNC_LOCK_VB,									// see code
	PBCMD_ASYNC_UNLOCK_VB,
	PBCMD_ASYNC_LOCK_IB,									// see code
	PBCMD_ASYNC_UNLOCK_IB,
	PBCMD_SET_SCISSOR_RECT,									// RECT
};



#define N_DWORDS( x ) (( sizeof(x)+3)/sizeof( DWORD ))
#define N_DWORDS_IN_PTR (N_DWORDS( void * ))

class D3DDeviceWrapper
{
private:
	IDirect3DDevice9 *m_pD3DDevice;
	bool m_bSupportsTessellation;
	int m_nCurrentTessLevel;
	TessellationMode_t m_nTessellationMode;

#if SHADERAPI_USE_SMP
	uintptr_t m_pASyncThreadHandle;
	PushBuffer *m_pCurPushBuffer;
	uint32 *m_pOutputPtr;
	size_t m_PushBufferFreeSlots;
#endif

#if SHADERAPI_BUFFER_D3DCALLS
	bool m_bBufferingD3DCalls;
#	define SHADERAPI_BUFFER_MAXRENDERTARGETS 4
	IDirect3DSurface9 *m_StoredRenderTargets[SHADERAPI_BUFFER_MAXRENDERTARGETS];
#endif

	PushBuffer *FindFreePushBuffer( PushBufferState newstate );	// find a free push buffer and change its state

	void GetPushBuffer(void);								// set us up to point at a new push buffer
	void SubmitPushBufferAndGetANewOne(void);				// submit the current push buffer
	void ExecutePushBuffer( PushBuffer const *pb);

#if	SHADERAPI_USE_SMP
	void Synchronize(void);									// wait for all commands to be done
#else
	FORCEINLINE void Synchronize(void)
	{
	}
#endif


	void SubmitIfNotBusy(void);

#if	SHADERAPI_USE_SMP	
	template<class T> FORCEINLINE void PushStruct( PushBufferCommand cmd, T const *str )
	{
		int nwords=N_DWORDS( T );
		AllocatePushBufferSpace( 1+ nwords );
		m_pOutputPtr[0]=cmd;
		memcpy( m_pOutputPtr+1, str, sizeof( T ) );
		m_pOutputPtr += 1+nwords;
	}
	
	FORCEINLINE void AllocatePushBufferSpace(size_t nSlots)
	{
		// check for N slots of space, and decrement amount of space left
		if ( nSlots>m_PushBufferFreeSlots )					// out of room?
		{
			SubmitPushBufferAndGetANewOne();
		}
		m_PushBufferFreeSlots -= nSlots;
	}

	// simple methods for pushing a few words into output buffer
	FORCEINLINE void Push( PushBufferCommand cmd )
	{
		AllocatePushBufferSpace(1);
		m_pOutputPtr[0]=cmd;
		m_pOutputPtr++;
	}

	FORCEINLINE void Push( PushBufferCommand cmd, int arg1)
	{
		AllocatePushBufferSpace(2);
		m_pOutputPtr[0]=cmd;
		m_pOutputPtr[1]=arg1;
		m_pOutputPtr += 2;
	}

	FORCEINLINE void Push( PushBufferCommand cmd, void *ptr )
	{
		AllocatePushBufferSpace(1+N_DWORDS_IN_PTR);
		*(m_pOutputPtr++)=cmd;
		*((void **) m_pOutputPtr)=ptr;
		m_pOutputPtr+=N_DWORDS_IN_PTR;
	}

	FORCEINLINE void Push( PushBufferCommand cmd, void *ptr, void *ptr1 )
	{
		AllocatePushBufferSpace(1+2*N_DWORDS_IN_PTR);
		*(m_pOutputPtr++)=cmd;
		*((void **) m_pOutputPtr)=ptr;
		m_pOutputPtr+=N_DWORDS_IN_PTR;
		*((void **) m_pOutputPtr)=ptr1;
		m_pOutputPtr+=N_DWORDS_IN_PTR;
	}

	FORCEINLINE void Push( PushBufferCommand cmd, void *arg1, uint32 arg2, uint32 arg3, uint32 arg4,
					  void *arg5)
	{
		AllocatePushBufferSpace(1+N_DWORDS_IN_PTR+1+1+1+N_DWORDS_IN_PTR);
		*(m_pOutputPtr++)=cmd;
		*((void **) m_pOutputPtr)=arg1;
		m_pOutputPtr+=N_DWORDS_IN_PTR;
		*(m_pOutputPtr++)=arg2;
		*(m_pOutputPtr++)=arg3;
		*(m_pOutputPtr++)=arg4;
		*((void **) m_pOutputPtr)=arg5;
		m_pOutputPtr+=N_DWORDS_IN_PTR;

	}

	FORCEINLINE void Push( PushBufferCommand cmd, uint32 arg1, void *ptr )
	{
		AllocatePushBufferSpace(2+N_DWORDS_IN_PTR);
		*(m_pOutputPtr++)=cmd;
		*(m_pOutputPtr++)=arg1;
		*((void **) m_pOutputPtr)=ptr;
		m_pOutputPtr+=N_DWORDS_IN_PTR;
	}

	FORCEINLINE void Push( PushBufferCommand cmd, uint32 arg1, void *ptr, int arg2, int arg3 )
	{
		AllocatePushBufferSpace( 4+N_DWORDS_IN_PTR );
		*(m_pOutputPtr++)=cmd;
		*(m_pOutputPtr++)=arg1;
		*((void **) m_pOutputPtr)=ptr;
		m_pOutputPtr+=N_DWORDS_IN_PTR;
		m_pOutputPtr[0]=arg2;
		m_pOutputPtr[1]=arg3;
		m_pOutputPtr += 2;
	}

	FORCEINLINE void Push( PushBufferCommand cmd, int arg1, int arg2)
	{
		AllocatePushBufferSpace(3);
		m_pOutputPtr[0]=cmd;
		m_pOutputPtr[1]=arg1;
		m_pOutputPtr[2]=arg2;
		m_pOutputPtr += 3;
	}

	FORCEINLINE void Push( PushBufferCommand cmd, int arg1, int arg2, int arg3)
	{
		AllocatePushBufferSpace(4);
		m_pOutputPtr[0]=cmd;
		m_pOutputPtr[1]=arg1;
		m_pOutputPtr[2]=arg2;
		m_pOutputPtr[3]=arg3;
		m_pOutputPtr += 4;
	}

	FORCEINLINE void Push( PushBufferCommand cmd, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6 )
	{
		AllocatePushBufferSpace(7);
		m_pOutputPtr[0]=cmd;
		m_pOutputPtr[1]=arg1;
		m_pOutputPtr[2]=arg2;
		m_pOutputPtr[3]=arg3;
		m_pOutputPtr[4]=arg4;
		m_pOutputPtr[5]=arg5;
		m_pOutputPtr[6]=arg6;
		m_pOutputPtr += 7;
	}

#else
	template<class T> FORCEINLINE void PushStruct( PushBufferCommand cmd, T const *str )
	{
	}
	
	FORCEINLINE void AllocatePushBufferSpace(size_t nSlots)
	{
	}

	// simple methods for pushing a few words into output buffer
	FORCEINLINE void Push( PushBufferCommand cmd )
	{
	}

	FORCEINLINE void Push( PushBufferCommand cmd, int arg1)
	{
	}

	FORCEINLINE void Push( PushBufferCommand cmd, void *ptr )
	{
	}

	FORCEINLINE void Push( PushBufferCommand cmd, void *ptr, void *ptr1 )
	{
	}

	FORCEINLINE void Push( PushBufferCommand cmd, void *arg1, uint32 arg2, uint32 arg3, uint32 arg4,
					  void *arg5)
	{
	}

	FORCEINLINE void Push( PushBufferCommand cmd, uint32 arg1, void *ptr )
	{
	}

	FORCEINLINE void Push( PushBufferCommand cmd, uint32 arg1, void *ptr, int arg2, int arg3 )
	{
	}

	FORCEINLINE void Push( PushBufferCommand cmd, int arg1, int arg2)
	{
	}

	FORCEINLINE void Push( PushBufferCommand cmd, int arg1, int arg2, int arg3)
	{
	}

	FORCEINLINE void Push( PushBufferCommand cmd, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6 )
	{
	}

#endif

	FORCEINLINE bool ASyncMode(void) const
	{
#if SHADERAPI_USE_SMP
#	if SHADERAPI_BUFFER_D3DCALLS
		return m_bBufferingD3DCalls;
#	else
		return (m_pASyncThreadHandle != 0 );
#	endif
#else
		return false;
#endif
	}

	FORCEINLINE IDirect3DDevice9* Dx9Device(void) const
	{
		return m_pD3DDevice;
	}

	void  AsynchronousLock( IDirect3DVertexBuffer9* vb, size_t offset, size_t size, void **ptr,
							DWORD flags,
							LockedBufferContext *lb);

	void  AsynchronousLock( IDirect3DIndexBuffer9* ib, size_t offset, size_t size, void **ptr,
							DWORD flags,
							LockedBufferContext *lb);

	// handlers for push buffer contexts
	void HandleAsynchronousLockVBCommand( uint32 const *dptr );
	void HandleAsynchronousUnLockVBCommand( uint32 const *dptr );
	void HandleAsynchronousLockIBCommand( uint32 const *dptr );
	void HandleAsynchronousUnLockIBCommand( uint32 const *dptr );

public:

#if SHADERAPI_BUFFER_D3DCALLS
	void ExecuteAllWork( void );
#endif
	void RunThread( void );									// this is what the worker thread runs

	void SetASyncMode( bool onoff );


	bool IsActive( void )const
	{
		return m_pD3DDevice != NULL;
	}

	void D3DeviceWrapper(void)
	{
		m_pD3DDevice = 0;
#if SHADERAPI_USE_SMP
		m_pASyncThreadHandle = 0;
#endif
#if SHADERAPI_BUFFER_D3DCALLS
		m_bBufferingD3DCalls = false;
#endif
	}
	
	void SetDevicePtr(IDirect3DDevice9 *pD3DDev )
	{
		m_pD3DDevice = pD3DDev;
	}

	void SetSupportsTessellation( bool bSupportsTessellation )
	{
		m_bSupportsTessellation = bSupportsTessellation;
	}

	void ShutDownDevice(void)
	{
		if ( ASyncMode() )
		{
			// sync w/ thread
		}
		m_pD3DDevice = 0;
	}

	void FORCEINLINE SetDepthStencilSurface( IDirect3DSurface9 *new_stencil )
	{
		if ( ASyncMode() )
			Push( PBCMD_SET_DEPTH_STENCIL_SURFACE, new_stencil );
		else
			DO_D3D( SetDepthStencilSurface( new_stencil ) );
	}
	
	HRESULT CreateCubeTexture(
		UINT EdgeLength,
		UINT Levels,
		DWORD Usage,
		D3DFORMAT Format,
		D3DPOOL Pool,
		IDirect3DCubeTexture9 ** ppCubeTexture,
		HANDLE* pSharedHandle,
		char *debugLabel = NULL				// <-- OK to not pass this arg, only passed through on DX_TO_GL_ABSTRACTION
		)
	{
		Synchronize();
		return m_pD3DDevice->CreateCubeTexture( EdgeLength, Levels, Usage, Format, Pool,
												ppCubeTexture, pSharedHandle
											#if defined( DX_TO_GL_ABSTRACTION )
												,debugLabel
											#endif
											  );
	}

	HRESULT CreateVolumeTexture(
		UINT Width,
		UINT Height,
		UINT Depth,
		UINT Levels,
		DWORD Usage,
		D3DFORMAT Format,
		D3DPOOL Pool,
		IDirect3DVolumeTexture9** ppVolumeTexture,
		HANDLE* pSharedHandle,
		char *debugLabel = NULL				// <-- OK to not pass this arg, only passed through on DX_TO_GL_ABSTRACTION
		)
	{
		Synchronize();
		return m_pD3DDevice->CreateVolumeTexture( Width, Height, Depth, Levels,
												  Usage, Format, Pool, ppVolumeTexture,
												  pSharedHandle
											#if defined( DX_TO_GL_ABSTRACTION )
												,debugLabel
											#endif
												  );
	}

	HRESULT CreateOffscreenPlainSurface( UINT Width,
										 UINT Height,
										 D3DFORMAT Format,
										 D3DPOOL Pool,
										 IDirect3DSurface9** ppSurface,
										 HANDLE* pSharedHandle)
	{
		Synchronize();
		return m_pD3DDevice->CreateOffscreenPlainSurface( Width, Height, Format, Pool,
														  ppSurface, pSharedHandle);
	}
	
	HRESULT CreateTexture(
		UINT Width,
		UINT Height,
		UINT Levels,
		DWORD Usage,
		D3DFORMAT Format,
		D3DPOOL Pool,
		IDirect3DTexture9** ppTexture,
		HANDLE* pSharedHandle,
		char *debugLabel = NULL				// <-- OK to not pass this arg, only passed through on DX_TO_GL_ABSTRACTION
		)
	{
		Synchronize();
		return m_pD3DDevice->CreateTexture( Width, Height, Levels, Usage, 
											Format, Pool, ppTexture, pSharedHandle
											#if defined( DX_TO_GL_ABSTRACTION )
												,debugLabel
											#endif
											);
	}

	HRESULT GetRenderTargetData(
		IDirect3DSurface9* pRenderTarget,
		IDirect3DSurface9* pDestSurface
		)
	{
		Synchronize();
		return m_pD3DDevice->GetRenderTargetData( pRenderTarget, pDestSurface );
	}


	void GetDeviceCaps( D3DCAPS9 * pCaps )
	{
		Synchronize();
		m_pD3DDevice->GetDeviceCaps( pCaps );
	}

	LPCSTR GetPixelShaderProfile( void )
	{
		Synchronize();
		return D3DXGetPixelShaderProfile( m_pD3DDevice );
	}

	HRESULT TestCooperativeLevel( void )
	{
	// hack!  We are going to assume that calling this immediately when in buffered mode isn't going to cause problems.
#if !SHADERAPI_BUFFER_D3DCALLS
		Synchronize();
#endif
		return m_pD3DDevice->TestCooperativeLevel();
	}

	HRESULT GetFrontBufferData( UINT  iSwapChain, IDirect3DSurface9 * pDestSurface )
	{
		Synchronize();
		return m_pD3DDevice->GetFrontBufferData( iSwapChain, pDestSurface );
	}

	void SetGammaRamp( int swapchain, int flags, D3DGAMMARAMP const *pRamp)
	{
		Synchronize();
		m_pD3DDevice->SetGammaRamp( swapchain, flags, pRamp);
	}

	HRESULT GetTexture(  DWORD Stage,  IDirect3DBaseTexture9 ** ppTexture )
	{
		Synchronize();
		return m_pD3DDevice->GetTexture( Stage, ppTexture );
	}

	HRESULT GetFVF( DWORD * pFVF )
	{
		Synchronize();
		return m_pD3DDevice->GetFVF( pFVF );
	}

	HRESULT GetDepthStencilSurface(
		IDirect3DSurface9 ** ppZStencilSurface
		)
	{
		Synchronize();
		return m_pD3DDevice->GetDepthStencilSurface( ppZStencilSurface );
	}

	FORCEINLINE void SetClipPlane( int idx, float const * pplane)
	{
		RECORD_COMMAND( DX8_SET_CLIP_PLANE, 5 );
		RECORD_INT( idx );
		RECORD_FLOAT( pplane[0] );
		RECORD_FLOAT( pplane[1] );
		RECORD_FLOAT( pplane[2] );
		RECORD_FLOAT( pplane[3] );
#if SHADERAPI_USE_SMP
		if ( ASyncMode() )
		{
			AllocatePushBufferSpace( 6 );
			m_pOutputPtr[0]=PBCMD_SETCLIPPLANE;
			m_pOutputPtr[1]=idx;
			memcpy(m_pOutputPtr+2,pplane, 4*sizeof(float) );
			m_pOutputPtr += 6;
		}
		else
#endif
			DO_D3D( SetClipPlane( idx, pplane ) );
	}

	FORCEINLINE void SetVertexDeclaration( IDirect3DVertexDeclaration9 *decl )
	{
		RECORD_COMMAND( DX8_SET_VERTEX_DECLARATION, 1 );
		RECORD_INT( ( int ) decl );

#if SHADERAPI_USE_SMP
		if ( ASyncMode() )
		{
			Push( PBCMD_SET_VERTEXDECLARATION, decl );
		}
		else
#endif
			DO_D3D( SetVertexDeclaration( decl ) );
	}

	FORCEINLINE void SetViewport( D3DVIEWPORT9 const *vp )
	{
		RECORD_COMMAND( DX8_SET_VIEWPORT, 1 );
		RECORD_STRUCT( vp, sizeof( *vp ));

#if SHADERAPI_USE_SMP
		if ( ASyncMode() )
			PushStruct( PBCMD_SETVIEWPORT, vp );
		else
#endif
			DO_D3D( SetViewport( vp ) );
	}

	HRESULT GetRenderTarget(
		DWORD RenderTargetIndex,
		IDirect3DSurface9 ** ppRenderTarget)
	{
#if SHADERAPI_BUFFER_D3DCALLS
		if ( ASyncMode() )
		{
			Assert( RenderTargetIndex >= 0 && RenderTargetIndex < SHADERAPI_BUFFER_MAXRENDERTARGETS );
			*ppRenderTarget = m_StoredRenderTargets[RenderTargetIndex];
			return D3D_OK;
		}
#endif
		Synchronize();
		return m_pD3DDevice->GetRenderTarget( RenderTargetIndex, ppRenderTarget );
	}

	HRESULT CreateQuery( D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery )
	{
		Synchronize();
		return m_pD3DDevice->CreateQuery( Type, ppQuery );
	}

	HRESULT CreateRenderTarget(
		UINT Width,
		UINT Height,
		D3DFORMAT Format,
		D3DMULTISAMPLE_TYPE MultiSample,
		DWORD MultisampleQuality,
		BOOL Lockable,
		IDirect3DSurface9** ppSurface,
		HANDLE* pSharedHandle
		)
	{
		Synchronize();
		return m_pD3DDevice->CreateRenderTarget( Width, Height, Format, MultiSample,
												 MultisampleQuality, Lockable, ppSurface,
												 pSharedHandle);
	}

	HRESULT CreateDepthStencilSurface(
		UINT Width,
		UINT Height,
		D3DFORMAT Format,
		D3DMULTISAMPLE_TYPE MultiSample,
		DWORD MultisampleQuality,
		BOOL Discard,
		IDirect3DSurface9** ppSurface,
		HANDLE* pSharedHandle
		)
	{
		Synchronize();
		return m_pD3DDevice->CreateDepthStencilSurface( Width, Height, Format, MultiSample,
														MultisampleQuality, Discard, ppSurface,
														pSharedHandle );
	}


	FORCEINLINE void SetRenderTarget( int idx, IDirect3DSurface9 *new_rt )
	{
		if (ASyncMode())
		{
			Push( PBCMD_SET_RENDER_TARGET, idx, new_rt );
#if SHADERAPI_BUFFER_D3DCALLS
			m_StoredRenderTargets[idx] = new_rt;
#endif
		}
		else
		{
			// NOTE: If the debug runtime breaks here on the shadow depth render target that is normal.  dx9 doesn't directly support shadow
			// depth texturing so we are forced to initialize this texture without the render target flagr
			DO_D3D( SetRenderTarget( idx, new_rt) );
		}
	}

	FORCEINLINE void LightEnable( int lidx, bool onoff )
	{
		RECORD_COMMAND( DX8_LIGHT_ENABLE, 2 );
		RECORD_INT( lidx );
		RECORD_INT( onoff );

		Synchronize();
		DO_D3D( LightEnable( lidx, onoff ) );
	}

	FORCEINLINE void SetRenderState( D3DRENDERSTATETYPE state, DWORD val )
	{
//		Assert( state >= 0 && state < MAX_NUM_RENDERSTATES );
		RECORD_RENDER_STATE( state, val ); 
		if (ASyncMode())
		{
			Push( PBCMD_SET_RENDERSTATE, state, val );
		}
		else
			DO_D3D( SetRenderState( state, val ) );
	}

	FORCEINLINE void SetRenderStateInline( D3DRENDERSTATETYPE state, DWORD val )
	{
		//		Assert( state >= 0 && state < MAX_NUM_RENDERSTATES );
		RECORD_RENDER_STATE( state, val ); 
		if (ASyncMode())
		{
			SetRenderState( state, val );
		}
		else
		{
#ifdef DX_TO_GL_ABSTRACTION
			DO_D3D( SetRenderStateInline( state, val ) );
#else
			DO_D3D( SetRenderState( state, val ) );
#endif
		}
	}

	FORCEINLINE void SetScissorRect( const RECT *pScissorRect )
	{
		RECORD_COMMAND( DX8_SET_SCISSOR_RECT, 1 );
		RECORD_STRUCT( pScissorRect, 4 * sizeof(LONG) );
#if SHADERAPI_USE_SMP
		if ( ASyncMode() )
		{
			AllocatePushBufferSpace( 5 );
			m_pOutputPtr[0] = PBCMD_SET_SCISSOR_RECT;
			memcpy( m_pOutputPtr + 1, pScissorRect, sizeof( *pScissorRect ) );
		}
		else
#endif
			DO_D3D( SetScissorRect( pScissorRect ) );
	}

	FORCEINLINE void SetVertexShaderConstantF( UINT StartRegister, CONST float * pConstantData,
								   UINT Vector4fCount)
	{
		RECORD_COMMAND( DX8_SET_VERTEX_SHADER_CONSTANT, 3 );
		RECORD_INT( StartRegister );
		RECORD_INT( Vector4fCount );
		RECORD_STRUCT( pConstantData, Vector4fCount * 4 * sizeof(float) );
#if SHADERAPI_USE_SMP
		if ( ASyncMode() )
		{
			AllocatePushBufferSpace(3+4*Vector4fCount);
			m_pOutputPtr[0]=PBCMD_SET_VERTEX_SHADER_CONSTANT;
			m_pOutputPtr[1]=StartRegister;
			m_pOutputPtr[2]=Vector4fCount;
			memcpy(m_pOutputPtr+3,pConstantData,sizeof(float)*4*Vector4fCount);
			m_pOutputPtr+=3+4*Vector4fCount;
		}
		else
#endif
			DO_D3D( SetVertexShaderConstantF( StartRegister, pConstantData, Vector4fCount ) );
	}

	FORCEINLINE void SetVertexShaderConstantB( UINT StartRegister, CONST int * pConstantData,
		UINT BoolCount)
	{
		RECORD_COMMAND( DX8_SET_VERTEX_SHADER_CONSTANT, 3 );
		RECORD_INT( StartRegister );
		RECORD_INT( BoolCount );
		RECORD_STRUCT( pConstantData, BoolCount * sizeof(int) );
#if SHADERAPI_USE_SMP
		if ( ASyncMode() )
		{
			AllocatePushBufferSpace(3+BoolCount);
			m_pOutputPtr[0]=PBCMD_SET_BOOLEAN_VERTEX_SHADER_CONSTANT;
			m_pOutputPtr[1]=StartRegister;
			m_pOutputPtr[2]=BoolCount;
			memcpy(m_pOutputPtr+3,pConstantData,sizeof(int)*BoolCount);
			m_pOutputPtr+=3+BoolCount;
		}
		else
#endif
			DO_D3D( SetVertexShaderConstantB( StartRegister, pConstantData, BoolCount ) );
	}

	FORCEINLINE void SetVertexShaderConstantI( UINT StartRegister, CONST int * pConstantData,
		UINT Vector4IntCount)
	{
		RECORD_COMMAND( DX8_SET_VERTEX_SHADER_CONSTANT, 3 );
		RECORD_INT( StartRegister );
		RECORD_INT( Vector4IntCount );
		RECORD_STRUCT( pConstantData, Vector4IntCount * 4 * sizeof(int) );
#if SHADERAPI_USE_SMP
		if ( ASyncMode() )
		{
			AllocatePushBufferSpace(3+4*Vector4IntCount);
			m_pOutputPtr[0]=PBCMD_SET_INTEGER_VERTEX_SHADER_CONSTANT;
			m_pOutputPtr[1]=StartRegister;
			m_pOutputPtr[2]=Vector4IntCount;
			memcpy(m_pOutputPtr+3,pConstantData,sizeof(int)*4*Vector4IntCount);
			m_pOutputPtr+=3+4*Vector4IntCount;
		}
		else
#endif
			DO_D3D( SetVertexShaderConstantI( StartRegister, pConstantData, Vector4IntCount ) );
	}

	FORCEINLINE void SetPixelShaderConstantF( UINT StartRegister, CONST float * pConstantData,
								   UINT Vector4fCount)
	{
		RECORD_COMMAND( DX8_SET_PIXEL_SHADER_CONSTANT, 3 );
		RECORD_INT( StartRegister );
		RECORD_INT( Vector4fCount );
		RECORD_STRUCT( pConstantData, Vector4fCount * 4 * sizeof(float) );
#if SHADERAPI_USE_SMP
		if ( ASyncMode() )
		{
			AllocatePushBufferSpace(3+4*Vector4fCount);
			m_pOutputPtr[0]=PBCMD_SET_PIXEL_SHADER_CONSTANT;
			m_pOutputPtr[1]=StartRegister;
			m_pOutputPtr[2]=Vector4fCount;
			memcpy(m_pOutputPtr+3,pConstantData,sizeof(float)*4*Vector4fCount);
			m_pOutputPtr+=3+4*Vector4fCount;
		}
		else
#endif
			DO_D3D( SetPixelShaderConstantF( StartRegister, pConstantData, Vector4fCount ) );
	}

	FORCEINLINE void SetPixelShaderConstantB( UINT StartRegister, CONST int * pConstantData,
		UINT BoolCount)
	{
		RECORD_COMMAND( DX8_SET_PIXEL_SHADER_CONSTANT, 3 );
		RECORD_INT( StartRegister );
		RECORD_INT( BoolCount );
		RECORD_STRUCT( pConstantData, BoolCount * sizeof(int) );
#if SHADERAPI_USE_SMP
		if ( ASyncMode() )
		{
			AllocatePushBufferSpace(3+BoolCount);
			m_pOutputPtr[0]=PBCMD_SET_BOOLEAN_PIXEL_SHADER_CONSTANT;
			m_pOutputPtr[1]=StartRegister;
			m_pOutputPtr[2]=BoolCount;
			memcpy(m_pOutputPtr+3,pConstantData,sizeof(int)*BoolCount);
			m_pOutputPtr+=3+BoolCount;
		}
		else
#endif
			DO_D3D( SetPixelShaderConstantB( StartRegister, pConstantData, BoolCount ) );
	}

	FORCEINLINE void SetPixelShaderConstantI( UINT StartRegister, CONST int * pConstantData,
		UINT Vector4IntCount)
	{
		RECORD_COMMAND( DX8_SET_PIXEL_SHADER_CONSTANT, 3 );
		RECORD_INT( StartRegister );
		RECORD_INT( Vector4IntCount );
		RECORD_STRUCT( pConstantData, Vector4IntCount * 4 * sizeof(int) );
#if SHADERAPI_USE_SMP
		if ( ASyncMode() )
		{
			AllocatePushBufferSpace(3+4*Vector4IntCount);
			m_pOutputPtr[0]=PBCMD_SET_INTEGER_PIXEL_SHADER_CONSTANT;
			m_pOutputPtr[1]=StartRegister;
			m_pOutputPtr[2]=Vector4IntCount;
			memcpy(m_pOutputPtr+3,pConstantData,sizeof(int)*4*Vector4IntCount);
			m_pOutputPtr+=3+4*Vector4IntCount;
		}
		else
#endif
			DO_D3D( SetPixelShaderConstantI( StartRegister, pConstantData, Vector4IntCount ) );
	}

	HRESULT StretchRect( IDirect3DSurface9 * pSourceSurface,
					  CONST RECT * pSourceRect,
					  IDirect3DSurface9 * pDestSurface,
					  CONST RECT * pDestRect,
					  D3DTEXTUREFILTERTYPE Filter )
	{
#if SHADERAPI_USE_SMP
		if ( ASyncMode() )
		{
			AllocatePushBufferSpace(1+1+1+N_DWORDS( RECT )+1+1+N_DWORDS( RECT ) + 1);
			*(m_pOutputPtr++)=PBCMD_STRETCHRECT;
			*(m_pOutputPtr++)=(int) pSourceSurface;
			*(m_pOutputPtr++)=(pSourceRect != NULL);
			if (pSourceRect)
			{
				memcpy(m_pOutputPtr,pSourceRect,sizeof(RECT));
			}
			m_pOutputPtr+=N_DWORDS(RECT);
			*(m_pOutputPtr++)=(int) pDestSurface;
			*(m_pOutputPtr++)=(pDestRect != NULL);
			if (pDestRect)
				memcpy(m_pOutputPtr,pDestRect,sizeof(RECT));
			m_pOutputPtr+=N_DWORDS(RECT);
			*(m_pOutputPtr++)=Filter;
			return S_OK;									// !bug!
		}
		else
#endif
			return m_pD3DDevice->
				StretchRect( pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter );
	}


	FORCEINLINE void BeginScene(void)
	{
		RECORD_COMMAND( DX8_BEGIN_SCENE, 0 );
		if ( ASyncMode() )
			Push( PBCMD_BEGIN_SCENE );
		else
			DO_D3D( BeginScene() );
	}

	FORCEINLINE void EndScene(void)
	{
		RECORD_COMMAND( DX8_END_SCENE, 0 );
		if ( ASyncMode() )
			Push( PBCMD_END_SCENE );
		else
			DO_D3D( EndScene() );
	}

	FORCEINLINE HRESULT Lock( IDirect3DVertexBuffer9* vb, size_t offset, size_t size, void **ptr, DWORD flags )
	{
		Assert( size );						 // lock size of 0 = unknown entire size of buffer = bad
		Synchronize();

		HRESULT hr = vb->Lock(offset, size, ptr, flags);
		switch (hr)
		{
			case D3DERR_INVALIDCALL:
				Warning( "D3DERR_INVALIDCALL - Vertex Buffer Lock Failed in %s on line %d(offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags );
				break;
			case D3DERR_DRIVERINTERNALERROR:
				Warning( "D3DERR_DRIVERINTERNALERROR - Vertex Buffer Lock Failed in %s on line %d (offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags );
				break;
			case D3DERR_OUTOFVIDEOMEMORY:
				Warning( "D3DERR_OUTOFVIDEOMEMORY - Vertex Buffer Lock Failed in %s on line %d (offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags );
				break;
		}

		return hr;
	}

		
	FORCEINLINE HRESULT  Lock( IDirect3DVertexBuffer9* vb, size_t offset, size_t size, void **ptr,
							DWORD flags,
							LockedBufferContext *lb)
	{

		HRESULT hr = D3D_OK;

		// asynchronous write-only dynamic vb lock
		if ( ASyncMode() )
		{
			AsynchronousLock( vb, offset, size, ptr, flags, lb );
		}
		else
		{
			hr = vb->Lock(offset, size, ptr, flags);
			switch (hr)
			{
				case D3DERR_INVALIDCALL:
					Warning( "D3DERR_INVALIDCALL - Vertex Buffer Lock Failed in %s on line %d(offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags );
					break;
				case D3DERR_DRIVERINTERNALERROR:
					Warning( "D3DERR_DRIVERINTERNALERROR - Vertex Buffer Lock Failed in %s on line %d (offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags );
					break;
				case D3DERR_OUTOFVIDEOMEMORY:
					Warning( "D3DERR_OUTOFVIDEOMEMORY - Vertex Buffer Lock Failed in %s on line %d (offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags );
					break;
			}
		}

		return hr;
	}

	FORCEINLINE HRESULT Lock( IDirect3DIndexBuffer9* ib, size_t offset, size_t size, void **ptr, DWORD flags)
	{
		HRESULT hr = D3D_OK;

		Synchronize();

		hr = ib->Lock(offset, size, ptr, flags);
		switch (hr)
		{
			case D3DERR_INVALIDCALL:
				Warning( "D3DERR_INVALIDCALL - Index Buffer Lock Failed in %s on line %d(offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags );
				break;
			case D3DERR_DRIVERINTERNALERROR:
				Warning( "D3DERR_DRIVERINTERNALERROR - Index Buffer Lock Failed in %s on line %d (offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags );
				break;
			case D3DERR_OUTOFVIDEOMEMORY:
				Warning( "D3DERR_OUTOFVIDEOMEMORY - Index Buffer Lock Failed in %s on line %d (offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags );
				break;
		}

		return hr;
	}
	
	// asycnhronous lock of index buffer
	FORCEINLINE HRESULT Lock( IDirect3DIndexBuffer9* ib, size_t offset, size_t size, void **ptr, DWORD flags,
						   LockedBufferContext * lb)
	{
		HRESULT hr = D3D_OK;

		if ( ASyncMode() )
			AsynchronousLock( ib, offset, size, ptr, flags, lb );
		else
		{
			hr = ib->Lock(offset, size, ptr, flags);
			switch (hr)
			{
				case D3DERR_INVALIDCALL:
					Warning( "D3DERR_INVALIDCALL - Index Buffer Lock Failed in %s on line %d(offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags );
					break;
				case D3DERR_DRIVERINTERNALERROR:
					Warning( "D3DERR_DRIVERINTERNALERROR - Index Buffer Lock Failed in %s on line %d (offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags );
					break;
				case D3DERR_OUTOFVIDEOMEMORY:
					Warning( "D3DERR_OUTOFVIDEOMEMORY - Index Buffer Lock Failed in %s on line %d (offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, offset, size, flags );
					break;
			}
		}

		return hr;
	}

#ifndef DX_TO_GL_ABSTRACTION
	FORCEINLINE HRESULT  UpdateSurface(	IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestSurface, CONST POINT* pDestPoint )
	{
		return m_pD3DDevice->UpdateSurface( pSourceSurface, pSourceRect, pDestSurface, pDestPoint );
	}
#endif

	void Release( IDirect3DIndexBuffer9* ib )
	{
		Synchronize();
		ib->Release();
	}

	void Release( IDirect3DVertexBuffer9* vb )
	{
		Synchronize();
		vb->Release();
	}

	FORCEINLINE void Unlock( IDirect3DVertexBuffer9* vb )
	{
		// needed for d3d on pc only
		if ( ASyncMode() )
			Push(PBCMD_UNLOCK_VB, vb);
		else
		{
			HRESULT hr = vb->Unlock( );

			if ( FAILED(hr) )
			{
				Warning( "Vertex Buffer Unlock Failed in %s on line %d\n", V_UnqualifiedFileName(__FILE__), __LINE__ );
			}
		}
	}

	FORCEINLINE void Unlock( IDirect3DVertexBuffer9* vb, LockedBufferContext *lb, size_t unlock_size)
	{
		// needed for d3d on pc only
#if SHADERAPI_USE_SMP
		if ( ASyncMode() )
		{
			AllocatePushBufferSpace( 1+N_DWORDS_IN_PTR+N_DWORDS( LockedBufferContext )+1 );
			*(m_pOutputPtr++)=PBCMD_ASYNC_UNLOCK_VB;
			*((IDirect3DVertexBuffer9* *) m_pOutputPtr)=vb;
			m_pOutputPtr+=N_DWORDS_IN_PTR;
			*((LockedBufferContext *) m_pOutputPtr)=*lb;
			m_pOutputPtr+=N_DWORDS( LockedBufferContext );
			*(m_pOutputPtr++)=unlock_size;
		}
		else
#endif
		{
			HRESULT hr = vb->Unlock();

			if ( FAILED(hr) )
			{
				Warning( "Vertex Buffer Unlock Failed in %s on line %d\n", V_UnqualifiedFileName(__FILE__), __LINE__ );
			}
		}
	}

	FORCEINLINE void Unlock( IDirect3DIndexBuffer9* ib )
	{
		// needed for d3d on pc only
		if ( ASyncMode() )
			Push(PBCMD_UNLOCK_IB, ib);
		else
		{
			HRESULT hr = ib->Unlock();

			if ( FAILED(hr) )
			{
				Warning( "Index Buffer Unlock Failed in %s on line %d\n", V_UnqualifiedFileName(__FILE__), __LINE__ );
			}
		}
	}

	FORCEINLINE void Unlock( IDirect3DIndexBuffer9* ib, LockedBufferContext *lb, size_t unlock_size)
	{
		// needed for d3d on pc only
#if SHADERAPI_USE_SMP
		if ( ASyncMode() )
		{
			AllocatePushBufferSpace( 1+N_DWORDS_IN_PTR+N_DWORDS( LockedBufferContext )+1 );
			*(m_pOutputPtr++)=PBCMD_ASYNC_UNLOCK_IB;
			*((IDirect3DIndexBuffer9* *) m_pOutputPtr)=ib;
			m_pOutputPtr+=N_DWORDS_IN_PTR;
			*((LockedBufferContext *) m_pOutputPtr)=*lb;
			m_pOutputPtr+=N_DWORDS( LockedBufferContext );
			*(m_pOutputPtr++)=unlock_size;
		}
		else
#endif
		{
			HRESULT hr = ib->Unlock( );

			if ( FAILED(hr) )
			{
				Warning( "Index Buffer Unlock Failed in %s on line %d\n", V_UnqualifiedFileName(__FILE__), __LINE__ );
			}
		}
	}

	void ShowCursor( bool onoff)
	{
		Synchronize();
		DO_D3D( ShowCursor(onoff) );
	}

	FORCEINLINE void Clear( int count, D3DRECT const *pRects, int Flags, D3DCOLOR color, float Z, int stencil)
	{
#if SHADERAPI_USE_SMP
		if ( ASyncMode() )
		{
			int n_rects_words = count * N_DWORDS( D3DRECT );
			AllocatePushBufferSpace( 2 + n_rects_words + 4 );
			*(m_pOutputPtr++) = PBCMD_CLEAR;
			*(m_pOutputPtr++) = count;
			if ( count )
			{
				memcpy( m_pOutputPtr, pRects, count * sizeof( D3DRECT ) );
				m_pOutputPtr += n_rects_words;
			}
			*(m_pOutputPtr++) = Flags;
			*( (D3DCOLOR *) m_pOutputPtr ) = color;
			m_pOutputPtr++;
			*( (float *) m_pOutputPtr ) = Z;
			m_pOutputPtr++;
			*(m_pOutputPtr++) = stencil;
		}
		else
#endif
			DO_D3D( Clear(count, pRects, Flags, color, Z, stencil) );
	}
	
	HRESULT Reset( D3DPRESENT_PARAMETERS *parms)
	{
		RECORD_COMMAND( DX8_RESET, 1 );
		RECORD_STRUCT( parms, sizeof(*parms) );
		Synchronize();
		return m_pD3DDevice->Reset( parms );
	}

	void Release( void )
	{
		Synchronize();
		DO_D3D( Release() );
	}

	FORCEINLINE void SetTexture(int stage, IDirect3DBaseTexture9 *txtr)
	{
		RECORD_COMMAND( DX8_SET_TEXTURE, 3 );
		RECORD_INT( stage );
		RECORD_INT( -1 );
		RECORD_INT( -1 );
		if (ASyncMode())
		{
			Push( PBCMD_SET_TEXTURE, stage, txtr );
		}
		else
			DO_D3D( SetTexture( stage, txtr) );
	}
	void SetTransform( D3DTRANSFORMSTATETYPE mtrx_id, D3DXMATRIX const *mt)
	{
		RECORD_COMMAND( DX8_SET_TRANSFORM, 2 );
		RECORD_INT( mtrx_id );
		RECORD_STRUCT( mt, sizeof(D3DXMATRIX) );
		Synchronize();
		DO_D3D( SetTransform( mtrx_id, mt) );
	}
	
	FORCEINLINE void SetSamplerState( int stage, D3DSAMPLERSTATETYPE state, DWORD val)
	{
		RECORD_SAMPLER_STATE( stage, state, val ); 
		if ( ASyncMode() )
			Push( PBCMD_SET_SAMPLER_STATE, stage, state, val );
		else
			DO_D3D( SetSamplerState( stage, state, val) );
	}
	
	void SetFVF( int fvf)
	{
		Synchronize();
		DO_D3D( SetFVF( fvf) );
	}

	FORCEINLINE void SetTextureStageState( int stage, D3DTEXTURESTAGESTATETYPE state, DWORD val )
	{
		RECORD_TEXTURE_STAGE_STATE( stage, state, val ); 
		Synchronize();
		DO_D3D( SetTextureStageState( stage, state, val) );
	}

	FORCEINLINE void DrawPrimitive(
		D3DPRIMITIVETYPE PrimitiveType,
		UINT StartVertex,
		UINT PrimitiveCount
		)
	{
		RECORD_COMMAND( DX8_DRAW_PRIMITIVE, 3 );
		RECORD_INT( PrimitiveType );
		RECORD_INT(	StartVertex );
		RECORD_INT( PrimitiveCount );
		if ( ASyncMode() )
		{
			Push( PBCMD_DRAWPRIM, PrimitiveType, StartVertex, PrimitiveCount );
			SubmitIfNotBusy();
		}
		else
			DO_D3D( DrawPrimitive( PrimitiveType, StartVertex, PrimitiveCount ) );

	}

	HRESULT CreateVertexDeclaration(
		CONST D3DVERTEXELEMENT9* pVertexElements,
		IDirect3DVertexDeclaration9** ppDecl
		)
	{
		Synchronize();
		return m_pD3DDevice->CreateVertexDeclaration( pVertexElements, ppDecl );
	}


	HRESULT ValidateDevice( DWORD * pNumPasses )
	{
		Synchronize();
		return m_pD3DDevice->ValidateDevice( pNumPasses );
	}



	HRESULT CreateVertexShader(
		CONST DWORD * pFunction,
		IDirect3DVertexShader9** ppShader,
		const char *pShaderName,
		char *debugLabel = NULL
		)
	{
		Synchronize();
		#ifdef DX_TO_GL_ABSTRACTION
			return m_pD3DDevice->CreateVertexShader( pFunction, ppShader, pShaderName, debugLabel );
		#else
			return m_pD3DDevice->CreateVertexShader( pFunction, ppShader );
		#endif
	}

	HRESULT CreatePixelShader(
		CONST DWORD * pFunction,
		IDirect3DPixelShader9** ppShader,
	    const char *pShaderName,
		char *debugLabel = NULL
		)
	{
		Synchronize();
		#ifdef DX_TO_GL_ABSTRACTION
			return m_pD3DDevice->CreatePixelShader( pFunction, ppShader, pShaderName, debugLabel );
		#else
			return m_pD3DDevice->CreatePixelShader( pFunction, ppShader );
		#endif
	}


	FORCEINLINE void SetIndices(
		IDirect3DIndexBuffer9 * pIndexData
		)
	{
		if ( ASyncMode() )
			Push( PBCMD_SET_INDICES, pIndexData );
		else
			DO_D3D( SetIndices( pIndexData ) );
	}

	FORCEINLINE void SetStreamSource(
		UINT StreamNumber,
		IDirect3DVertexBuffer9 * pStreamData,
		UINT OffsetInBytes,
		UINT Stride
		)
	{
		if ( ASyncMode() )
			Push( PBCMD_SET_STREAM_SOURCE, StreamNumber, pStreamData, OffsetInBytes, Stride );
		else
			DO_D3D( SetStreamSource( StreamNumber, pStreamData, OffsetInBytes, Stride ) );
	}


	HRESULT CreateVertexBuffer(
		UINT Length,
		DWORD Usage,
		DWORD FVF,
		D3DPOOL Pool,
		IDirect3DVertexBuffer9** ppVertexBuffer,
		HANDLE* pSharedHandle
		)
	{
		Synchronize();
		return m_pD3DDevice->CreateVertexBuffer( Length, Usage, FVF,
												 Pool, ppVertexBuffer, pSharedHandle );
	}

	HRESULT CreateIndexBuffer(
		UINT Length,
		DWORD Usage,
		D3DFORMAT Format,
		D3DPOOL Pool,
		IDirect3DIndexBuffer9** ppIndexBuffer,
		HANDLE* pSharedHandle
		)
	{
		Synchronize();
		return m_pD3DDevice->CreateIndexBuffer( Length, Usage, Format, Pool, ppIndexBuffer,
												pSharedHandle );
	}


	FORCEINLINE void DrawIndexedPrimitive(
		D3DPRIMITIVETYPE Type,
		INT BaseVertexIndex,
		UINT MinIndex,
		UINT NumVertices,
		UINT StartIndex,
		UINT PrimitiveCount	)
	{
		RECORD_COMMAND( DX8_DRAW_INDEXED_PRIMITIVE, 6 );
		RECORD_INT( Type );
		RECORD_INT( BaseVertexIndex );
		RECORD_INT( MinIndex );
		RECORD_INT( NumVertices );
		RECORD_INT(	StartIndex );
		RECORD_INT( PrimitiveCount );
		if ( ASyncMode() )
		{
			Push(PBCMD_DRAWINDEXEDPRIM, 
				 Type, BaseVertexIndex, MinIndex, NumVertices, StartIndex, PrimitiveCount );
//			SubmitIfNotBusy();
		}
		else
		{
			DO_D3D( DrawIndexedPrimitive( Type, BaseVertexIndex, MinIndex, NumVertices, StartIndex, PrimitiveCount ) );
		}
	}

#ifndef DX_TO_GL_ABSTRACTION
	FORCEINLINE void DrawTessellatedIndexedPrimitive( INT BaseVertexIndex, UINT MinIndex, UINT NumVertices,
													  UINT StartIndex, UINT PrimitiveCount )
	{
		// Setup our stream-source frequencies
		DO_D3D( SetStreamSourceFreq( 0, D3DSTREAMSOURCE_INDEXEDDATA | PrimitiveCount  ) );
		DO_D3D( SetStreamSourceFreq( VertexStreamSpec_t::STREAM_MORPH, D3DSTREAMSOURCE_INSTANCEDATA | 1ul  ) );
		DO_D3D( SetStreamSourceFreq( VertexStreamSpec_t::STREAM_SUBDQUADS, D3DSTREAMSOURCE_INSTANCEDATA | 1ul ) );

		int nIndicesPerPatch = ( ( ( m_nCurrentTessLevel + 1 ) * 2 + 2 ) * m_nCurrentTessLevel ) - 2;
		int nVerticesPerPatch = m_nCurrentTessLevel + 1;
		nVerticesPerPatch *= nVerticesPerPatch;
		int nPrimitiveCount = nIndicesPerPatch - 2;
		DO_D3D( DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP, 0, 0, nVerticesPerPatch, 0, nPrimitiveCount ) );

		// Disable instancing
		DO_D3D( SetStreamSourceFreq( 0, 1ul ) );
		DO_D3D( SetStreamSourceFreq( VertexStreamSpec_t::STREAM_MORPH, 1ul ) );
		DO_D3D( SetStreamSourceFreq( VertexStreamSpec_t::STREAM_SUBDQUADS, 1ul ) );
	}

	FORCEINLINE void DrawTessellatedPrimitive( UINT StartVertex, UINT PrimitiveCount )
	{

		// Setup our stream-source frequencies
		DO_D3D( SetStreamSourceFreq( 0, D3DSTREAMSOURCE_INDEXEDDATA | PrimitiveCount  ) );
		DO_D3D( SetStreamSourceFreq( VertexStreamSpec_t::STREAM_MORPH, D3DSTREAMSOURCE_INSTANCEDATA | 1ul  ) );
		DO_D3D( SetStreamSourceFreq( VertexStreamSpec_t::STREAM_SUBDQUADS, D3DSTREAMSOURCE_INSTANCEDATA | 1ul ) );

		int nIndicesPerPatch = ( ( ( m_nCurrentTessLevel + 1 ) * 2 + 2 ) * m_nCurrentTessLevel ) - 2;
		int nVerticesPerPatch = m_nCurrentTessLevel + 1;
		nVerticesPerPatch *= nVerticesPerPatch;
		int nPrimitiveCount = nIndicesPerPatch - 2;
		DO_D3D( DrawIndexedPrimitive( D3DPT_TRIANGLESTRIP, 0, 0, nVerticesPerPatch, 0, nPrimitiveCount ) );

		// Disable instancing
		DO_D3D( SetStreamSourceFreq( 0, 1ul ) );
		DO_D3D( SetStreamSourceFreq( VertexStreamSpec_t::STREAM_MORPH, 1ul ) );
		DO_D3D( SetStreamSourceFreq( VertexStreamSpec_t::STREAM_SUBDQUADS, 1ul ) );
	}

	FORCEINLINE void SetTessellationLevel( float level )
	{
		// Track our current tessellation level
		m_nCurrentTessLevel = (int)ceil( level );
	}
#endif
	
	void SetMaterial( D3DMATERIAL9 const *mat)
	{
		RECORD_COMMAND( DX8_SET_MATERIAL, 1 );
		RECORD_STRUCT( &mat, sizeof(mat) );
		Synchronize();
		DO_D3D( SetMaterial( mat ) );
	}

	FORCEINLINE void SetPixelShader( IDirect3DPixelShader9 *pShader )
	{
		RECORD_COMMAND( DX8_SET_PIXEL_SHADER, 1 );
		RECORD_INT( ( int ) pShader );
		if ( ASyncMode() )
			Push( PBCMD_SET_PIXEL_SHADER, pShader );
		else
			DO_D3D( SetPixelShader( pShader ) );
	}

	FORCEINLINE void SetVertexShader( IDirect3DVertexShader9 *pShader )
	{
		if ( ASyncMode() )
			Push( PBCMD_SET_VERTEX_SHADER, pShader );
		else
			DO_D3D( SetVertexShader( pShader ) );
	}

#ifdef DX_TO_GL_ABSTRACTION
	FORCEINLINE HRESULT LinkShaderPair( IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps )
	{
		Assert ( !ASyncMode() );
		return DO_D3D( LinkShaderPair( vs, ps ) );
	}

	HRESULT QueryShaderPair( int index, GLMShaderPairInfo *infoOut )
	{
		Assert ( !ASyncMode() );
		return DO_D3D( QueryShaderPair( index, infoOut ) );
	}

	void SetMaxUsedVertexShaderConstantsHint( uint nMaxReg )
	{
		Assert( !ASyncMode() );
		DO_D3D( SetMaxUsedVertexShaderConstantsHint( nMaxReg ) );
	}

#endif

	void EvictManagedResources( void )
	{
		if (m_pD3DDevice)									// people call this before creating the device
		{
			Synchronize();
			DO_D3D( EvictManagedResources() );
		}
	}

	void SetLight( int i, D3DLIGHT9 const *l)
	{
		RECORD_COMMAND( DX8_SET_LIGHT, 2 );
		RECORD_INT( i );
		RECORD_STRUCT( l, sizeof(*l) );

		Synchronize();
		DO_D3D( SetLight(i, l) );
	}

	void DrawIndexedPrimitiveUP( D3DPRIMITIVETYPE PrimitiveType,
								 UINT MinVertexIndex,
								 UINT NumVertices,
								 UINT PrimitiveCount,
								 CONST void * pIndexData,
								 D3DFORMAT IndexDataFormat,
								 CONST void* pVertexStreamZeroData,
								 UINT VertexStreamZeroStride )
	{
		Synchronize();
		DO_D3D( DrawIndexedPrimitiveUP( PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount,
										pIndexData, IndexDataFormat, pVertexStreamZeroData,
										VertexStreamZeroStride ) );
	}

	HRESULT Present(
		CONST RECT * pSourceRect,
		CONST RECT * pDestRect,
		VD3DHWND hDestWindowOverride,
		CONST RGNDATA * pDirtyRegion)
	{
		RECORD_COMMAND( DX8_PRESENT, 0 );
#if SHADERAPI_USE_SMP
		if ( ASyncMode() )
		{
			// need to deal with ret code here
			AllocatePushBufferSpace(1+1+
									N_DWORDS( RECT )+1+N_DWORDS( RECT )+1+1+N_DWORDS( RGNDATA ));
			*(m_pOutputPtr++)=PBCMD_PRESENT;
			*(m_pOutputPtr++)=( pSourceRect != NULL );
			if (pSourceRect)
				memcpy(m_pOutputPtr, pSourceRect, sizeof( RECT ) );
			m_pOutputPtr+=N_DWORDS( RECT );
			*(m_pOutputPtr++)=( pDestRect != NULL );
			if (pDestRect)
				memcpy(m_pOutputPtr, pDestRect, sizeof( RECT ) );
			m_pOutputPtr+=N_DWORDS( RECT );
			*(m_pOutputPtr++)=(uint32) hDestWindowOverride;
			*(m_pOutputPtr++)=( pDirtyRegion != NULL );
			if (pDirtyRegion)
				memcpy(m_pOutputPtr, pDirtyRegion, sizeof( RGNDATA ));
			m_pOutputPtr+=N_DWORDS( RGNDATA );
			return S_OK;									// not good - caller wants to here about lost devices
		}
		else
#endif
			return m_pD3DDevice->Present( pSourceRect, pDestRect, 
									  hDestWindowOverride, pDirtyRegion );
	}
	

#if defined( DX_TO_GL_ABSTRACTION )

	void AcquireThreadOwnership( )
	{
		m_pD3DDevice->AcquireThreadOwnership();
	}

	void ReleaseThreadOwnership( )
	{
		m_pD3DDevice->ReleaseThreadOwnership();
	}

#endif

};

#endif // D3DASYNC_H

#endif // #if D3D_ASYNC_SUPPORTED
