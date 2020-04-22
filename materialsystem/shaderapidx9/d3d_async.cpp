//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// methods for muti-core dx9 threading
//===========================================================================//

#ifdef D3D_ASYNC_SUPPORTED

#include "glmgr/dxabstract.h"
#include "utlsymbol.h"
#include "utlvector.h"
#include "utldict.h"
#include "utlbuffer.h"
#include "UtlStringMap.h"
#include "locald3dtypes.h"
#include "shaderapidx8_global.h"
#include "recording.h"
#include "tier0/vprof.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "shaderapidx8.h"
#include "materialsystem/IShader.h"
#include "utllinkedlist.h"
#include "IShaderSystem.h"
#include "tier0/fasttimer.h"
#include <stdlib.h>
#include "convar.h"
#include "materialsystem/shader_vcs_version.h"
#include "datacache/idatacache.h"
#include "winutils.h"

#include "tier0/memdbgon.h"

#if SHADERAPI_USE_SMP

// Set this to 1 to get vprof nodes for playing back the command stream.  This is good for counting calls in a frame, etc.
#define SHADERAPI_VPROF_BUFFER_PLAYBACK 1

#if SHADERAPI_VPROF_BUFFER_PLAYBACK && SHADERAPI_BUFFER_D3DCALLS
#define VPROF_BUFFER_PLAYBACK(name) VPROF(name)
#else
#define VPROF_BUFFER_PLAYBACK(name) ((void)0)
#endif

template<class T, int QSIZE> class FixedWorkQueue
{
	T Data[QSIZE];
	char pad0[256];
	volatile int n_added;
	int write_index;
	char pad1[256];											// make sure these don't share cache lines
	volatile int n_removed;
	int read_index;

public:
	FixedWorkQueue(void)
	{
		read_index=write_index=0;
		n_added=n_removed=0;
	}

	int IsEmpty(void)
	{
		return (n_added==n_removed);
	}

	int IsFull(void)
	{
		return (n_added-n_removed)>=QSIZE;
	}

	T GetWorkUnit(void)
	{
		if (IsEmpty())
			return 0;
		return Data[read_index];
	}

	void MarkUnitDone(void)
	{
		n_removed++;
		read_index=(read_index+1) % QSIZE;
	}

	void AddWorkUnit(T unit)
	{
#if SHADERAPI_BUFFER_D3DCALLS
		Assert( !IsFull() );
#else
		while (IsFull())
			Sleep(0);
#endif
		Data[write_index]=unit;
		n_added++;
		write_index=(write_index+1) % QSIZE;
	}
};


#if SHADERAPI_BUFFER_D3DCALLS
#define N_PUSH_BUFFERS 5000
#else
#define N_PUSH_BUFFERS 500
#endif
static volatile PushBuffer *PushBuffers[N_PUSH_BUFFERS];
FixedWorkQueue<PushBuffer *, N_PUSH_BUFFERS> PBQueue;



#ifdef WIN32
void __cdecl OurThreadInit( void * ourthis )
#else
unsigned int OurThreadInit( void * ourthis )
#endif
{
	(( D3DDeviceWrapper *) ourthis )->RunThread();
#ifndef WIN32
	return 0;
#endif
}

void D3DDeviceWrapper::RunThread( void )
{
	SetThreadAffinityMask(GetCurrentThread(), 2);
	for(;;)
	{
		PushBuffer *Pbuf=PBQueue.GetWorkUnit();
		if (! Pbuf)
		{
			; //Sleep(0);
		}
		else
		{
			ExecutePushBuffer( Pbuf );
			PBQueue.MarkUnitDone();
			Pbuf->m_State = PUSHBUFFER_AVAILABLE;
		}
	}
}

#if SHADERAPI_BUFFER_D3DCALLS
void D3DDeviceWrapper::ExecuteAllWork( void )
{
	if( !m_bBufferingD3DCalls )
		return;
	VPROF_BUDGET( "ExecuteAllWork", "ExecuteAllWork" );
	SubmitPushBufferAndGetANewOne();
	PushBuffer *Pbuf;
	while( ( Pbuf = PBQueue.GetWorkUnit() ) != NULL )
	{
		ExecutePushBuffer( Pbuf );
		PBQueue.MarkUnitDone();
		Pbuf->m_State = PUSHBUFFER_AVAILABLE;
	}
	m_bBufferingD3DCalls = false;
}
#endif

#if SHADERAPI_BUFFER_D3DCALLS
#define MAXIMUM_NUMBER_OF_BUFFERS_LOCKED_AT_ONCE 1600
#else
#define MAXIMUM_NUMBER_OF_BUFFERS_LOCKED_AT_ONCE 16
#endif

struct RememberedPointer
{
	void *m_pKey;
	void *m_pRememberedPtr;
} RememberedPointerHistory[MAXIMUM_NUMBER_OF_BUFFERS_LOCKED_AT_ONCE];

void D3DDeviceWrapper::SetASyncMode( bool onoff )
{
#if SHADERAPI_BUFFER_D3DCALLS
	if ( onoff )
	{
		m_bBufferingD3DCalls = true;
		// allocate push buffers if we need to
		if ( PushBuffers[0] == NULL )
		{
			for(int i=0; i<N_PUSH_BUFFERS; i++)
				PushBuffers[i]=new PushBuffer;
		}
		// create thread and init communications
		memset( RememberedPointerHistory,0,sizeof(RememberedPointerHistory) );
	}
#else
	if ( onoff )
	{
		if (! m_pASyncThreadHandle )
		{
			// allocate push buffers if we need to
			if ( PushBuffers[0] == NULL )
			{
				for(int i=0; i<N_PUSH_BUFFERS; i++)
					PushBuffers[i]=new PushBuffer;
			}
			// create thread and init communications
			memset( RememberedPointerHistory,0,sizeof(RememberedPointerHistory) );
			SetThreadAffinityMask(GetCurrentThread(), 1);
#ifdef WIN32
			m_pASyncThreadHandle = _beginthread( OurThreadInit, 128*1024, this );
#else
			m_pASyncThreadHandle = (uintptr_t)CreateSimpleThread( OurThreadInit, this, 128*1024 );
#endif
		}
	}
	else
	{
		Synchronize();
	}
#endif
}


PushBuffer *D3DDeviceWrapper::FindFreePushBuffer( PushBufferState newstate )
{
	VPROF_BUFFER_PLAYBACK( "D3DDeviceWrapper::FindFreePushBuffer" );
	for(;;)
	{
		for(int i=0;i<N_PUSH_BUFFERS;i++)
		{
			if (PushBuffers[i]->m_State == PUSHBUFFER_AVAILABLE )
			{
				PushBuffers[i]->m_State = newstate;
				return (PushBuffer *) PushBuffers[i];
			}
		}
		// hmm, out of push buffers. better sleep and try again later
		SubmitPushBufferAndGetANewOne();
		Sleep(0);
	}
}

void D3DDeviceWrapper::GetPushBuffer( void )
{
	VPROF_BUFFER_PLAYBACK( "D3DDeviceWrapper::GetPushBuffer" );
	m_pCurPushBuffer = FindFreePushBuffer( PUSHBUFFER_BEING_FILLED );
	m_pOutputPtr = m_pCurPushBuffer->m_BufferData;
	m_PushBufferFreeSlots = PUSHBUFFER_NELEMS - 1; // leave room for end marker
}

void D3DDeviceWrapper::SubmitPushBufferAndGetANewOne( void )
{
	VPROF_BUFFER_PLAYBACK( "D3DDeviceWrapper::SubmitPushBufferAndGetANewOne" );

	// submit the current push buffer
	if ( m_pCurPushBuffer )
	{
		if (m_pOutputPtr == m_pCurPushBuffer->m_BufferData)	// haven't done anyting, don't bother
			return;
		*(m_pOutputPtr) = PBCMD_END;				// mark end
		m_pCurPushBuffer->m_State = PUSHBUFFER_SUBMITTED;
		// here, enqueue for task
		PBQueue.AddWorkUnit( m_pCurPushBuffer );
	}
	GetPushBuffer();
}

void D3DDeviceWrapper::SubmitIfNotBusy( void )
{
	VPROF_BUFFER_PLAYBACK( "D3DDeviceWrapper::SubmitIfNotBusy" );
	if ( PBQueue.IsEmpty() )
		SubmitPushBufferAndGetANewOne();
}


void D3DDeviceWrapper::Synchronize( void )
{
#if SHADERAPI_BUFFER_D3DCALLS
	if( m_bBufferingD3DCalls )
	{
		Assert( 0 );
		Error( "Synchronize not supported with SHADERAPI_BUFFER_D3DCALLS" );
	}
	return;
#endif
	if ( ASyncMode())
	{
		SubmitPushBufferAndGetANewOne();
		// here, wait for queue to become empty
		while (! PBQueue.IsEmpty() )
		{
			// Sleep(1);
		}
	}
}

void D3DDeviceWrapper::AsynchronousLock( IDirect3DIndexBuffer9* ib,
										  size_t offset, size_t size, void **ptr,
										  DWORD flags,
										  LockedBufferContext *lb)
{
	VPROF_BUFFER_PLAYBACK( "D3DDeviceWrapper::AsynchronousLock index" );

	if ( size <= sizeof( PushBuffers[0]->m_BufferData ))
	{
		// can use one of our pushbuffers for this
		lb->m_pPushBuffer = FindFreePushBuffer( PUSHBUFFER_BEING_USED_FOR_LOCKEDDATA );
		*(ptr) = lb->m_pPushBuffer->m_BufferData;
		Assert( *ptr );
		lb->m_pMallocedMemory = NULL;
	}
	else												// out of buffer space or size too big
	{
		lb->m_pPushBuffer = NULL;
		lb->m_pMallocedMemory = new uint8 [ size ];
		*(ptr) = lb->m_pMallocedMemory;
	}
	// now, push lock commands
	AllocatePushBufferSpace( 1+N_DWORDS_IN_PTR+3 );
	*(m_pOutputPtr++)=PBCMD_ASYNC_LOCK_IB;
	*((LPDIRECT3DINDEXBUFFER *) m_pOutputPtr)=ib;
	m_pOutputPtr+=N_DWORDS_IN_PTR;
	*(m_pOutputPtr++)=offset;
	*(m_pOutputPtr++)=size;
	*(m_pOutputPtr++)=flags;
}

void D3DDeviceWrapper::AsynchronousLock( IDirect3DVertexBuffer9* vb,
										   size_t offset, size_t size, void **ptr,
										   DWORD flags,
										   LockedBufferContext *lb)
{
	VPROF_BUFFER_PLAYBACK( "D3DDeviceWrapper::AsynchronousLock vertex" );
	// we have commands in flight. Need to use temporary memory for this lock.
	// if the size needed is < the amount of space in a push buffer, we can use
	// a push buffer for the buffer. Otherwise, we're going to malloc one.
	if ( size <= sizeof( PushBuffers[0]->m_BufferData ))
	{
		// can use one of our pushbuffers for this
		lb->m_pPushBuffer = FindFreePushBuffer( PUSHBUFFER_BEING_USED_FOR_LOCKEDDATA );
		*(ptr) = lb->m_pPushBuffer->m_BufferData;
		Assert( *ptr );
		lb->m_pMallocedMemory = NULL;
	}
	else												// out of buffer space or size too big
	{
		lb->m_pPushBuffer = NULL;
		lb->m_pMallocedMemory = new uint8 [ size ];
		*(ptr) = lb->m_pMallocedMemory;
	}
	// now, push lock commands
	AllocatePushBufferSpace( 1+N_DWORDS_IN_PTR+3 );
	*(m_pOutputPtr++)=PBCMD_ASYNC_LOCK_VB;
	*((LPDIRECT3DVERTEXBUFFER *) m_pOutputPtr)=vb;
	m_pOutputPtr+=N_DWORDS_IN_PTR;
	*(m_pOutputPtr++)=offset;
	*(m_pOutputPtr++)=size;
	*(m_pOutputPtr++)=flags;
}



inline void RememberLockedPointer( void *key, void *value )
{
	VPROF_BUFFER_PLAYBACK( "RememberLockedPointer" );
	int repl=-1;
	int i;
	for(i=0;i<MAXIMUM_NUMBER_OF_BUFFERS_LOCKED_AT_ONCE;i++)
	{
		if ( RememberedPointerHistory[i].m_pKey==key )
			break;
		if ( (repl == -1 ) && (RememberedPointerHistory[i].m_pRememberedPtr == 0 ) )
			repl=i;
	}
	if (i != MAXIMUM_NUMBER_OF_BUFFERS_LOCKED_AT_ONCE )
	{
		RememberedPointerHistory[i].m_pRememberedPtr = value;
		if ( value==NULL )
			RememberedPointerHistory[i].m_pKey = NULL;
	}
	else
	{
		if (repl == -1 )
		{
			Assert( 0 );
		}
		else
		{
			RememberedPointerHistory[repl].m_pKey = key;
			RememberedPointerHistory[repl].m_pRememberedPtr = value;
		}
	}
}

inline void *RecallLockedPointer( void *key )
{
	VPROF_BUFFER_PLAYBACK( "RecallLockedPointer" );

	for(int i=0;i<MAXIMUM_NUMBER_OF_BUFFERS_LOCKED_AT_ONCE;i++)
		if ( RememberedPointerHistory[i].m_pKey == key )
			return RememberedPointerHistory[i].m_pRememberedPtr;
	return NULL;

}

void D3DDeviceWrapper::HandleAsynchronousLockVBCommand( uint32 const *dptr )
{
	dptr++;
	LPDIRECT3DVERTEXBUFFER vb=*((LPDIRECT3DVERTEXBUFFER *) dptr);
	dptr+=N_DWORDS_IN_PTR;
	uint32 offset=*(dptr++);
	uint32 size=*(dptr++);
	uint32 flags=*(dptr++);
	void *locked_ptr=0;
	vb->Lock( offset, size, &locked_ptr, flags );
	RememberLockedPointer( vb, locked_ptr );
}

void D3DDeviceWrapper::HandleAsynchronousUnLockVBCommand( uint32 const *dptr )
{
	dptr++;
	LPDIRECT3DVERTEXBUFFER vb=*((LPDIRECT3DVERTEXBUFFER *) dptr);
	dptr+=N_DWORDS_IN_PTR;
	LockedBufferContext lb=*((LockedBufferContext *) dptr);
	dptr+=N_DWORDS( LockedBufferContext );
	size_t unlock_size=*( dptr++ );
	void *locked_data=RecallLockedPointer( vb );
	Assert( locked_data );
	if (lb.m_pPushBuffer)
	{
		Assert( ! lb.m_pMallocedMemory );
		if ( locked_data )
			memcpy( locked_data, lb.m_pPushBuffer->m_BufferData, unlock_size );
		lb.m_pPushBuffer->m_State = PUSHBUFFER_AVAILABLE;
	}
	else if ( lb.m_pMallocedMemory )
	{
		Assert( ! lb.m_pPushBuffer );
		if ( locked_data )
			memcpy( locked_data, lb.m_pMallocedMemory, unlock_size );
		delete[] ((uint8 *) lb.m_pMallocedMemory);
	}
	// now, actually unlock
	RememberLockedPointer( vb, NULL );
	vb->Unlock();
}

void D3DDeviceWrapper::HandleAsynchronousLockIBCommand( uint32 const *dptr )
{
	dptr++;
	LPDIRECT3DINDEXBUFFER ib=*((LPDIRECT3DINDEXBUFFER *) dptr);
	Assert( ib );
	dptr+=N_DWORDS_IN_PTR;
	uint32 offset=*(dptr++);
	uint32 size=*(dptr++);
	uint32 flags=*(dptr++);
	void *locked_ptr=0;
	ib->Lock( offset, size, &locked_ptr, flags );
	RememberLockedPointer( ib, locked_ptr );
}

void D3DDeviceWrapper::HandleAsynchronousUnLockIBCommand( uint32 const *dptr )
{
	dptr++;
	LPDIRECT3DINDEXBUFFER ib=*((LPDIRECT3DINDEXBUFFER *) dptr);
	dptr+=N_DWORDS_IN_PTR;
	LockedBufferContext lb=*((LockedBufferContext *) dptr);
	dptr+=N_DWORDS( LockedBufferContext );
	size_t unlock_size=*( dptr++ );
	void *locked_data=RecallLockedPointer( ib );
	Assert( locked_data );
	if (lb.m_pPushBuffer)
	{
		Assert( ! lb.m_pMallocedMemory );
		if ( locked_data )
			memcpy( locked_data, lb.m_pPushBuffer->m_BufferData, unlock_size );
		lb.m_pPushBuffer->m_State = PUSHBUFFER_AVAILABLE;
	}
	else if ( lb.m_pMallocedMemory )
	{
		Assert( ! lb.m_pPushBuffer );
		if ( locked_data )
			memcpy( locked_data, lb.m_pMallocedMemory, unlock_size );
		delete[] ((uint8 *) lb.m_pMallocedMemory);
	}
	// now, actually unlock
	RememberLockedPointer( ib, NULL );
	ib->Unlock();

}


static inline void *FetchPtr( uint32 const *mem)
{
	void **p=(void **) mem;
	return *p;
}

#define CALC_STATS 1
#if CALC_STATS
int n_commands_executed=0;
int n_pbs_executed=0;
#endif

void D3DDeviceWrapper::ExecutePushBuffer( PushBuffer const* pb)
{
	VPROF_BUFFER_PLAYBACK( "D3DDeviceWrapper::ExecutePushBuffer" );
	uint32 const *dptr=pb->m_BufferData;
	n_pbs_executed++;
	for(;;)
	{
		n_commands_executed++;
		switch( dptr[0] )
		{
			case PBCMD_END:
			{
				VPROF_BUFFER_PLAYBACK( "END" );
				n_commands_executed--;						// doesn't count
				return;
			}

			case PBCMD_SET_RENDERSTATE:
			{
				VPROF_BUFFER_PLAYBACK( "SET_RENDERSTATE" );
				Dx9Device()->SetRenderState((D3DRENDERSTATETYPE) dptr[1],dptr[2]);
				dptr+=3;
				break;
			}

			case PBCMD_SET_SAMPLER_STATE:
			{
				VPROF_BUFFER_PLAYBACK( "SET_SAMPLER_STATE" );
				Dx9Device()->SetSamplerState(dptr[1], (D3DSAMPLERSTATETYPE) dptr[2], dptr[3]);
				dptr+=4;
				break;
			}

			case PBCMD_DRAWPRIM:
			{
				VPROF_BUFFER_PLAYBACK( "DRAWPRIM" );
				
				tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "Dx9Device()->DrawPrimitive" );
				
				Dx9Device()->DrawPrimitive( (D3DPRIMITIVETYPE) dptr[1], dptr[2], dptr[3] );
				dptr+=4;
				break;
			}

			case PBCMD_DRAWINDEXEDPRIM:
			{
				VPROF_BUFFER_PLAYBACK( "DRAWINDEXEDPRIM" );
				
				tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "Dx9Device()->DrawIndexedPrimitive" );

				Dx9Device()->DrawIndexedPrimitive( (D3DPRIMITIVETYPE) dptr[1], dptr[2], dptr[3],
												   dptr[4], dptr[5], dptr[6]);
				dptr+=7;
				break;
			}

			case PBCMD_SET_STREAM_SOURCE:
			{
				VPROF_BUFFER_PLAYBACK( "SET_STREAM_SOURCE" );
				Dx9Device()->SetStreamSource( dptr[1],(IDirect3DVertexBuffer9 *) FetchPtr(dptr+2),
											  dptr[3],dptr[4] );
				dptr += 4+N_DWORDS( IDirect3DVertexBuffer9 * );
				break;
			}

			case PBCMD_SET_TEXTURE:
			{
				VPROF_BUFFER_PLAYBACK( "SET_TEXTURE" );
				Dx9Device()->SetTexture( dptr[1],(IDirect3DBaseTexture *) FetchPtr(dptr+2));
				dptr += 2+N_DWORDS_IN_PTR;
				break;
			}

			case PBCMD_SET_RENDER_TARGET:
			{
				VPROF_BUFFER_PLAYBACK( "SET_RENDER_TARGET" );
				Dx9Device()->SetRenderTarget( dptr[1],(IDirect3DSurface *) FetchPtr(dptr+2));
				dptr += 2+N_DWORDS_IN_PTR;
				break;
			}

			case PBCMD_SET_PIXEL_SHADER:
			{
				VPROF_BUFFER_PLAYBACK( "SET_PIXEL_SHADER" );
				Dx9Device()->SetPixelShader( (IDirect3DPixelShader9 *) FetchPtr(dptr+1));
				dptr += 1+N_DWORDS_IN_PTR;
				break;
			}

			case PBCMD_SET_INDICES:
			{
				VPROF_BUFFER_PLAYBACK( "SET_INDICES" );
				Dx9Device()->SetIndices( (IDirect3DIndexBuffer9*) FetchPtr(dptr+1));
				dptr += 1+N_DWORDS_IN_PTR;
				break;
			}

			case PBCMD_SET_DEPTH_STENCIL_SURFACE:
			{
				VPROF_BUFFER_PLAYBACK( "SET_DEPTH_STENCIL_SURFACE" );
				Dx9Device()->SetDepthStencilSurface( (IDirect3DSurface9*) FetchPtr(dptr+1));
				dptr += 1+N_DWORDS_IN_PTR;
				break;
			}

			case PBCMD_SETVIEWPORT:
			{
				VPROF_BUFFER_PLAYBACK( "SETVIEWPORT" );
				Dx9Device()->SetViewport( (D3DVIEWPORT9 const *) (dptr+1) );
				dptr += 1+N_DWORDS(D3DVIEWPORT9);
				break;
			}

			case PBCMD_SET_VERTEX_SHADER:
			{
				VPROF_BUFFER_PLAYBACK( "SET_VERTEX_SHADER" );
				Dx9Device()->SetVertexShader( (IDirect3DVertexShader9 *) FetchPtr(dptr+1));
				dptr += 1+N_DWORDS_IN_PTR;
				break;
			}

			case PBCMD_ASYNC_LOCK_VB:
			{
				VPROF_BUFFER_PLAYBACK( "ASYNC_LOCK_VB" );
				HandleAsynchronousLockVBCommand(dptr);
				dptr+=1+N_DWORDS_IN_PTR+3;
				break;
			}

			case PBCMD_ASYNC_UNLOCK_VB:
			{
				VPROF_BUFFER_PLAYBACK( "ASYNC_UNLOCK_VB" );
				HandleAsynchronousUnLockVBCommand( dptr );
				dptr+=1+N_DWORDS_IN_PTR+N_DWORDS( LockedBufferContext )+1;
				break;
			}

			case PBCMD_ASYNC_LOCK_IB:
			{
				VPROF_BUFFER_PLAYBACK( "ASYNC_LOCK_IB" );
				HandleAsynchronousLockIBCommand(dptr);
				dptr+=1+N_DWORDS_IN_PTR+3;
				break;
			}

			case PBCMD_ASYNC_UNLOCK_IB:
			{
				VPROF_BUFFER_PLAYBACK( "ASYNC_UNLOCK_IB" );
				HandleAsynchronousUnLockIBCommand( dptr );
				dptr+=1+N_DWORDS_IN_PTR+N_DWORDS( LockedBufferContext )+1;
				break;
			}

			case PBCMD_UNLOCK_VB:
			{
				VPROF_BUFFER_PLAYBACK( "UNLOCK_VB" );
				IDirect3DVertexBuffer9 *p=(IDirect3DVertexBuffer9 *) FetchPtr(dptr+1);
				p->Unlock();
				dptr += 1+N_DWORDS_IN_PTR;
				break;
			}
			case PBCMD_UNLOCK_IB:
			{
				VPROF_BUFFER_PLAYBACK( "UNLOCK_IB" );
				IDirect3DIndexBuffer9 *p=(IDirect3DIndexBuffer9 *) FetchPtr(dptr+1);
				p->Unlock();
				dptr += 1+N_DWORDS_IN_PTR;
				break;
			}
			case PBCMD_SET_VERTEX_SHADER_CONSTANT:
			{
				VPROF_BUFFER_PLAYBACK( "SET_VERTEX_SHADER_CONSTANT" );
				Dx9Device()->SetVertexShaderConstantF( dptr[1], (float const *) dptr+3, dptr[2]);
				dptr += 3+4*dptr[2];
				break;
			}
			case PBCMD_SET_BOOLEAN_VERTEX_SHADER_CONSTANT:
			{
				VPROF_BUFFER_PLAYBACK( "SET_BOOLEAN_VERTEX_SHADER_CONSTANT" );
				Dx9Device()->SetVertexShaderConstantB( dptr[1], (int const *) dptr+3, dptr[2]);
				dptr += 3+dptr[2];
				break;
			}

			case PBCMD_SET_INTEGER_VERTEX_SHADER_CONSTANT:
			{
				VPROF_BUFFER_PLAYBACK( "SET_INTEGER_VERTEX_SHADER_CONSTANT" );
				Dx9Device()->SetVertexShaderConstantI( dptr[1], (int const *) dptr+3, dptr[2]);
				dptr += 3+4*dptr[2];
				break;
			}

			case PBCMD_SET_PIXEL_SHADER_CONSTANT:
			{
				VPROF_BUFFER_PLAYBACK( "SET_PIXEL_SHADER_CONSTANT" );
				Dx9Device()->SetPixelShaderConstantF( dptr[1], (float const *) dptr+3, dptr[2]);
				dptr += 3+4*dptr[2];
				break;
			}
			case PBCMD_SET_BOOLEAN_PIXEL_SHADER_CONSTANT:
			{
				VPROF_BUFFER_PLAYBACK( "SET_BOOLEAN_PIXEL_SHADER_CONSTANT" );
				Dx9Device()->SetPixelShaderConstantB( dptr[1], (int const *) dptr+3, dptr[2]);
				dptr += 3+dptr[2];
				break;
			}

			case PBCMD_SET_INTEGER_PIXEL_SHADER_CONSTANT:
			{
				VPROF_BUFFER_PLAYBACK( "SET_INTEGER_PIXEL_SHADER_CONSTANT" );
				Dx9Device()->SetPixelShaderConstantI( dptr[1], (int const *) dptr+3, dptr[2]);
				dptr += 3+4*dptr[2];
				break;
			}

			case PBCMD_BEGIN_SCENE:
			{
				VPROF_BUFFER_PLAYBACK( "BEGIN_SCENE" );
				Dx9Device()->BeginScene();
				dptr++;
				break;
			}
			
			case PBCMD_END_SCENE:
			{
				VPROF_BUFFER_PLAYBACK( "END_SCENE" );
				Dx9Device()->EndScene();
				dptr++;
				break;
			}

			case PBCMD_CLEAR:
			{
				VPROF_BUFFER_PLAYBACK( "CLEAR" );
				dptr++;
				int count=*(dptr++);
				D3DRECT const *pRects=0;
				if (count)
				{
					pRects=(D3DRECT const *) dptr;
					dptr+=count*N_DWORDS( D3DRECT );
				}
				int flags=*(dptr++);
				D3DCOLOR color=*((D3DCOLOR const *) (dptr++));
				float z=*((float const *) (dptr++));
				int stencil=*(dptr++);
				Dx9Device()->Clear( count, pRects, flags, color, z, stencil );
				break;
			}

			case PBCMD_SET_VERTEXDECLARATION:
			{
				VPROF_BUFFER_PLAYBACK( "SET_VERTEXDECLARATION" );
				Dx9Device()->SetVertexDeclaration( (IDirect3DVertexDeclaration9 *) FetchPtr(dptr+1));
				dptr += 1+N_DWORDS_IN_PTR;
				break;
			}

			case PBCMD_SETCLIPPLANE:
			{
				VPROF_BUFFER_PLAYBACK( "SETCLIPPLANE" );
				Dx9Device()->SetClipPlane( dptr[1], (float const *) dptr+2 );
				dptr+=6;
			}
			break;

			case PBCMD_STRETCHRECT:
			{
				VPROF_BUFFER_PLAYBACK( "STRETCHRECT" );
				dptr++;
				IDirect3DSurface9 *pSourceSurface=(IDirect3DSurface9 *) FetchPtr(dptr);
				dptr+=N_DWORDS_IN_PTR;
				RECT const *pSourceRect=0;
				if (*(dptr++))
					pSourceRect=(RECT const *) dptr;
				dptr += N_DWORDS( RECT );
				IDirect3DSurface9 *pDestSurface= (IDirect3DSurface9 *) FetchPtr( dptr );
				dptr += N_DWORDS_IN_PTR;
				RECT const *pDestRect=0;
				if (*(dptr++))
					pDestRect=(RECT const *) dptr;
				dptr += N_DWORDS( RECT );
				D3DTEXTUREFILTERTYPE Filter = (D3DTEXTUREFILTERTYPE) *(dptr++);
				Dx9Device()->StretchRect( pSourceSurface, pSourceRect,
										  pDestSurface, pDestRect,
										  Filter );
			}
			break;
			
			
			case PBCMD_PRESENT:
			{
				VPROF_BUFFER_PLAYBACK( "PRESENT" );
				dptr++;
				RECT const *pSourceRect=0;
				if (* (dptr++) )
					pSourceRect=(RECT const *) dptr;
				dptr+=N_DWORDS( RECT );
				RECT const *pDestRect = 0;
				if (* (dptr++) )
					pDestRect=(RECT const *) dptr;
				dptr+=N_DWORDS( RECT );
				VD3DHWND hDestWindowOverride = (VD3DHWND) *(dptr++);
				RGNDATA const *pDirtyRegion=0;
				if ( *(dptr++) )
					pDirtyRegion= (RGNDATA const *) dptr;
				dptr+=N_DWORDS( RGNDATA );

				tmZone( TELEMETRY_LEVEL1, TMZF_NONE, "!D3DPresent" );

				Dx9Device()->Present( pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion );
				break;
			}

			case PBCMD_SET_SCISSOR_RECT:
			{
				VPROF_BUFFER_PLAYBACK( "SET_SCISSOR_RECT" );
				dptr++;
				const RECT *pRect = ( RECT * )FetchPtr( dptr );
				dptr += sizeof( RECT );
				Dx9Device()->SetScissorRect( pRect );
			}
		}
	}
}

#endif

#endif // D3D_ASYNC_SUPPORTED
