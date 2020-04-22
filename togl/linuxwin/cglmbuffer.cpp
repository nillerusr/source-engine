//========= Copyright Valve Corporation, All rights reserved. ============//
//                       TOGL CODE LICENSE
//
//  Copyright 2011-2014 Valve Corporation
//  All Rights Reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//
// cglmbuffer.cpp
//
//===============================================================================

#include "togl/rendermechanism.h"

// memdbgon -must- be the last include file in a .cpp file.
#include "tier0/memdbgon.h"

// 7LS TODO : took out cmdline here
bool g_bUsePseudoBufs = false; //( Plat_GetCommandLineA() ) ? ( strstr( Plat_GetCommandLineA(), "-gl_enable_pseudobufs" ) != NULL ) : false;
#ifdef OSX
// Significant perf degradation on some OSX parts if static buffers not disabled
bool g_bDisableStaticBuffer = true;
#else
bool g_bDisableStaticBuffer = false; //( Plat_GetCommandLineA() ) ? ( strstr( Plat_GetCommandLineA(), "-gl_disable_static_buffer" ) != NULL ) : false;
#endif

// http://www.opengl.org/registry/specs/ARB/vertex_buffer_object.txt
// http://www.opengl.org/registry/specs/ARB/pixel_buffer_object.txt

// gl_bufmode: zero means we mark all vertex/index buffers static

// non zero means buffers are initially marked static..
// ->but can shift to dynamic upon first 'discard' (orphaning)

// #define REPORT_LOCK_TIME	0

ConVar gl_bufmode( "gl_bufmode", "1" );

char ALIGN16 CGLMBuffer::m_StaticBuffers[ GL_MAX_STATIC_BUFFERS ][ GL_STATIC_BUFFER_SIZE ] ALIGN16_POST;
bool CGLMBuffer::m_bStaticBufferUsed[ GL_MAX_STATIC_BUFFERS ];

extern bool g_bNullD3DDevice; 

//===========================================================================//

static uint gMaxPersistentOffset[kGLMNumBufferTypes] =
{
	0,
	0,
	0,
	0
};
CON_COMMAND( gl_persistent_buffer_max_offset, "" )
{
	ConMsg( "OpenGL Persistent buffer max offset :\n" );
	ConMsg( "  Vertex buffer : %d bytes (%f MB) \n", gMaxPersistentOffset[kGLMVertexBuffer], gMaxPersistentOffset[kGLMVertexBuffer] / (1024.0f*1024.0f) );
	ConMsg( "  Index buffer : %d bytes (%f MB) \n", gMaxPersistentOffset[kGLMIndexBuffer], gMaxPersistentOffset[kGLMIndexBuffer] / (1024.0f*1024.0f) );
	ConMsg( "  Uniform buffer : %d bytes (%f MB) \n", gMaxPersistentOffset[kGLMUniformBuffer], gMaxPersistentOffset[kGLMUniformBuffer] / (1024.0f*1024.0f) );
	ConMsg( "  Pixel buffer : %d bytes (%f MB) \n", gMaxPersistentOffset[kGLMPixelBuffer], gMaxPersistentOffset[kGLMPixelBuffer] / (1024.0f*1024.0f) );
}

CPersistentBuffer::CPersistentBuffer()
:
	m_nSize( 0 )
	, m_nHandle( 0 )
	, m_pImmutablePersistentBuf( NULL )
	, m_nOffset( 0 )
#ifdef HAVE_GL_ARB_SYNC
	, m_nSyncObj( 0 )
#endif
{}

CPersistentBuffer::~CPersistentBuffer()
{
	Deinit();
}

void CPersistentBuffer::Init( EGLMBufferType type,uint nSize )
{
	Assert( gGL->m_bHave_GL_ARB_buffer_storage );
	Assert( gGL->m_bHave_GL_ARB_map_buffer_range );
	
	m_nSize		= nSize;
	m_nOffset	= 0;
	m_type		= type;
	
	switch ( type )
	{
	case kGLMVertexBuffer:	m_buffGLTarget = GL_ARRAY_BUFFER_ARB; break;
	case kGLMIndexBuffer:	m_buffGLTarget = GL_ELEMENT_ARRAY_BUFFER_ARB; break;

	default: Assert( nSize == 0 );
	}
	
	if ( m_nSize > 0 )
	{
		gGL->glGenBuffersARB( 1, &m_nHandle );
		gGL->glBindBufferARB( m_buffGLTarget, m_nHandle );

		// Create persistent immutable buffer that we will permanently map.  This buffer can be written from any thread (not just
		// the renderthread)
		gGL->glBufferStorage( m_buffGLTarget, m_nSize, (const GLvoid *)NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT ); // V_GL_REQ: GL_ARB_buffer_storage, GL_ARB_map_buffer_range, GL_VERSION_4_4

		// Map the buffer for all of eternity.  Pointer can be used from multiple threads.
		m_pImmutablePersistentBuf = gGL->glMapBufferRange( m_buffGLTarget, 0, m_nSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT ); // V_GL_REQ: GL_ARB_map_buffer_range, GL_ARB_buffer_storage, GL_VERSION_4_4
		Assert( m_pImmutablePersistentBuf != NULL );
	}
}

void CPersistentBuffer::Deinit()
{
	if ( !m_pImmutablePersistentBuf )
	{
		return;
	}

	BlockUntilNotBusy();

	gGL->glBindBufferARB( m_buffGLTarget, m_nHandle );
	gGL->glUnmapBuffer( m_buffGLTarget );
	gGL->glBindBufferARB( m_buffGLTarget, 0 );

	gGL->glDeleteBuffersARB( 1, &m_nHandle );
	
	m_nSize		= 0;
	m_nHandle	= 0;
	m_nOffset	= 0;
	m_pImmutablePersistentBuf = NULL;
}

void CPersistentBuffer::InsertFence()
{
#ifdef HAVE_GL_ARB_SYNC
	if (m_nSyncObj)
	{
		gGL->glDeleteSync( m_nSyncObj );
	}

	m_nSyncObj = gGL->glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
#endif
}

void CPersistentBuffer::BlockUntilNotBusy()
{
#ifdef HAVE_GL_ARB_SYNC
	if (m_nSyncObj)
	{
		gGL->glClientWaitSync( m_nSyncObj, GL_SYNC_FLUSH_COMMANDS_BIT, 3000000000000ULL );

		gGL->glDeleteSync( m_nSyncObj );

		m_nSyncObj = 0;
	}
#endif
	m_nOffset = 0;
}

void CPersistentBuffer::Append( uint nSize )
{
	m_nOffset += nSize;
	Assert( m_nOffset <= m_nSize );

	gMaxPersistentOffset[m_type] = Max( m_nOffset, gMaxPersistentOffset[m_type] );
}

//===========================================================================//

#if GL_ENABLE_INDEX_VERIFICATION

CGLMBufferSpanManager::CGLMBufferSpanManager() : 
	m_pCtx( NULL ),
	m_nBufType( kGLMVertexBuffer ),
	m_nBufSize( 0 ),
	m_bDynamic( false ),
	m_nSpanEndMax( -1 ),
	m_nNumAllocatedBufs( 0 ),
	m_nTotalBytesAllocated( 0 )
{
}

CGLMBufferSpanManager::~CGLMBufferSpanManager()
{
	Deinit();
}

void CGLMBufferSpanManager::Init( GLMContext *pContext, EGLMBufferType nBufType, uint nInitialCapacity, uint nBufSize, bool bDynamic )
{
	Assert( ( nBufType == kGLMIndexBuffer ) || ( nBufType == kGLMVertexBuffer ) );

	m_pCtx = pContext;
	m_nBufType = nBufType;
	
	m_nBufSize = nBufSize;
	m_bDynamic = bDynamic;

	m_ActiveSpans.EnsureCapacity( nInitialCapacity );
	m_DeletedSpans.EnsureCapacity( nInitialCapacity );
	m_nSpanEndMax = -1;

	m_nNumAllocatedBufs = 0;
	m_nTotalBytesAllocated = 0;
}

bool CGLMBufferSpanManager::AllocDynamicBuf( uint nSize, GLDynamicBuf_t &buf )
{
	buf.m_nGLType = GetGLBufType();
	buf.m_nActualBufSize = nSize;
	buf.m_nHandle = 0;
	buf.m_nSize = nSize;

	m_nNumAllocatedBufs++;
	m_nTotalBytesAllocated += buf.m_nActualBufSize;

	return true;
}

void CGLMBufferSpanManager::ReleaseDynamicBuf( GLDynamicBuf_t &buf )
{
	Assert( m_nNumAllocatedBufs > 0 );
	m_nNumAllocatedBufs--;

	Assert( m_nTotalBytesAllocated >= (int)buf.m_nActualBufSize );
	m_nTotalBytesAllocated -= buf.m_nActualBufSize;
}

void CGLMBufferSpanManager::Deinit()
{
	if ( !m_pCtx )
		return;

	for ( int i = 0; i < m_ActiveSpans.Count(); i++ )
	{
		if ( m_ActiveSpans[i].m_bOriginalAlloc )
			ReleaseDynamicBuf( m_ActiveSpans[i].m_buf );
	}
	m_ActiveSpans.SetCountNonDestructively( 0 );

	for ( int i = 0; i < m_DeletedSpans.Count(); i++ )
		ReleaseDynamicBuf( m_DeletedSpans[i].m_buf );

	m_DeletedSpans.SetCountNonDestructively( 0 );

	m_pCtx->BindGLBufferToCtx( GetGLBufType(), NULL, true );

	m_nSpanEndMax = -1;
	m_pCtx = NULL;

	Assert( !m_nNumAllocatedBufs );
	Assert( !m_nTotalBytesAllocated );
}

void CGLMBufferSpanManager::DiscardAllSpans()
{
	for ( int i = 0; i < m_ActiveSpans.Count(); i++ )
	{
		if ( m_ActiveSpans[i].m_bOriginalAlloc )
			ReleaseDynamicBuf( m_ActiveSpans[i].m_buf );
	}
	m_ActiveSpans.SetCountNonDestructively( 0 );

	for ( int i = 0; i < m_DeletedSpans.Count(); i++ )
		ReleaseDynamicBuf( m_DeletedSpans[i].m_buf );

	m_DeletedSpans.SetCountNonDestructively( 0 );

	m_nSpanEndMax = -1;

	Assert( !m_nNumAllocatedBufs );
	Assert( !m_nTotalBytesAllocated );
}

// TODO: Add logic to detect incorrect usage of bNoOverwrite.
CGLMBufferSpanManager::ActiveSpan_t *CGLMBufferSpanManager::AddSpan( uint nOffset, uint nMaxSize, uint nActualSize, bool bDiscard, bool bNoOverwrite  )
{
	(void)bDiscard;
	(void)bNoOverwrite;

	const uint nStart = nOffset;
	const uint nSize = nActualSize;
	const uint nEnd = nStart + nSize;

	GLDynamicBuf_t newDynamicBuf;
	if ( !AllocDynamicBuf( nSize, newDynamicBuf ) )
	{
		DXABSTRACT_BREAK_ON_ERROR();
		return NULL;
	}

	if ( (int)nStart < m_nSpanEndMax )
	{
		// Lock region potentially overlaps another previously locked region (since the last discard) - this is a very rarely (if ever) taken path in Source1 games.
		int i = 0;
		while ( i < m_ActiveSpans.Count() )
		{
			ActiveSpan_t &existingSpan = m_ActiveSpans[i];
			if ( ( nEnd <= existingSpan.m_nStart ) || ( nStart >= existingSpan.m_nEnd ) )
			{
				i++;
				continue;
			}

			Warning( "GL performance warning: AddSpan() at offset %u max size %u actual size %u, on a %s %s buffer of total size %u, overwrites an existing active lock span at offset %u size %u!\n", 
				nOffset, nMaxSize, nActualSize, 
				m_bDynamic ? "dynamic" : "static", ( m_nBufType == kGLMVertexBuffer ) ? "vertex" : "index", m_nBufSize, 
				existingSpan.m_nStart, existingSpan.m_nEnd - existingSpan.m_nStart );
			
			if ( ( nStart <= existingSpan.m_nStart ) && ( nEnd >= existingSpan.m_nEnd ) )
			{
				if ( existingSpan.m_bOriginalAlloc )
				{
					// New span totally covers existing span
					// Can't immediately delete the span's buffer because it could be referred to by another (child) span.
					m_DeletedSpans.AddToTail( existingSpan );
				}

				// Delete span
				m_ActiveSpans[i] = m_ActiveSpans[ m_ActiveSpans.Count() - 1 ];
				m_ActiveSpans.SetCountNonDestructively( m_ActiveSpans.Count() - 1 );
				continue;
			}

			// New span does NOT fully cover the existing span (partial overlap)
			if ( nStart < existingSpan.m_nStart )
			{
				// New span starts before existing span, but ends somewhere inside, so shrink it (start moves "right")
				existingSpan.m_nStart = nEnd;
			}
			else if ( nEnd > existingSpan.m_nEnd )
			{
				// New span ends after existing span, but starts somewhere inside (end moves "left")
				existingSpan.m_nEnd = nStart;
			}
			else //if ( ( nStart >= existingSpan.m_nStart ) && ( nEnd <= existingSpan.m_nEnd ) )
			{
				// New span lies inside of existing span
				if ( nStart == existingSpan.m_nStart )
				{
					// New span begins inside the existing span (start moves "right")
					existingSpan.m_nStart = nEnd;
				}
				else
				{
					if ( nEnd < existingSpan.m_nEnd )
					{
						// New span is completely inside existing span
						m_ActiveSpans.AddToTail( ActiveSpan_t( nEnd, existingSpan.m_nEnd, existingSpan.m_buf, false ) );
					}

					existingSpan.m_nEnd = nStart;
				}
			}

			Assert( existingSpan.m_nStart < existingSpan.m_nEnd );
			i++;
		}
	}

	newDynamicBuf.m_nLockOffset = nStart;
	newDynamicBuf.m_nLockSize = nSize;

	m_ActiveSpans.AddToTail( ActiveSpan_t( nStart, nEnd, newDynamicBuf, true ) );
	m_nSpanEndMax = MAX( m_nSpanEndMax, (int)nEnd );

	return &m_ActiveSpans.Tail();
}

bool CGLMBufferSpanManager::IsValid( uint nOffset, uint nSize ) const
{
	const uint nEnd = nOffset + nSize;
	
	int nTotalBytesRemaining = nSize;

	for ( int i = m_ActiveSpans.Count() - 1; i >= 0; --i )
	{
		const ActiveSpan_t &span = m_ActiveSpans[i];
		
		if ( span.m_nEnd <= nOffset )
			continue;
		if ( span.m_nStart >= nEnd )
			continue;

		uint nIntersectStart = MAX( span.m_nStart, nOffset );
		uint nIntersectEnd = MIN( span.m_nEnd, nEnd );
		Assert( nIntersectStart <= nIntersectEnd );

		nTotalBytesRemaining -= ( nIntersectEnd - nIntersectStart );
		Assert( nTotalBytesRemaining >= 0 );
		if ( nTotalBytesRemaining <= 0 )
			break;
	}

	return nTotalBytesRemaining == 0;
}
#endif // GL_ENABLE_INDEX_VERIFICATION

// glBufferSubData() with a max size limit, to work around NVidia's threaded driver limits (anything > than roughly 256KB triggers a sync with the server thread).
void glBufferSubDataMaxSize( GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data, uint nMaxSizePerCall )
{
#if TOGL_SUPPORT_NULL_DEVICE
	if ( g_bNullD3DDevice ) return;
#endif

	uint nBytesLeft = size;
	uint nOfs = 0;
	while ( nBytesLeft )
	{
		uint nBytesToCopy = MIN( nMaxSizePerCall, nBytesLeft );

		gGL->glBufferSubData( target, offset + nOfs, nBytesToCopy, static_cast<const unsigned char *>( data ) + nOfs );

		nBytesLeft -= nBytesToCopy;
		nOfs += nBytesToCopy;
	}
}

CGLMBuffer::CGLMBuffer( GLMContext *pCtx, EGLMBufferType type, uint size, uint options )
{
	m_pCtx = pCtx;
	m_type = type;
	
	m_bDynamic = ( options & GLMBufferOptionDynamic ) != 0;
				
	switch ( m_type )
	{
		case kGLMVertexBuffer:	m_buffGLTarget = GL_ARRAY_BUFFER_ARB; break;
		case kGLMIndexBuffer:	m_buffGLTarget = GL_ELEMENT_ARRAY_BUFFER_ARB; break;
		case kGLMUniformBuffer:	m_buffGLTarget = GL_UNIFORM_BUFFER_EXT; break;
		case kGLMPixelBuffer:	m_buffGLTarget = GL_PIXEL_UNPACK_BUFFER_ARB; break;
		
		default: Assert(!"Unknown buffer type" ); DXABSTRACT_BREAK_ON_ERROR();
	}
				
	m_nSize = size;
	m_nActualSize = size;
	m_bMapped = false;
	m_pLastMappedAddress = NULL;

	m_pStaticBuffer = NULL;
	m_nPinnedMemoryOfs = -1;
	m_nPersistentBufferStartOffset = 0;
	m_bUsingPersistentBuffer = false;

	m_bEnableAsyncMap = false;
	m_bEnableExplicitFlush = false;
	m_dirtyMinOffset = m_dirtyMaxOffset = 0;								// adjust/grow on lock, clear on unlock

	m_pCtx->CheckCurrent();
	m_nRevision = rand();
		
	m_pPseudoBuf = NULL;
	m_pActualPseudoBuf = NULL;

	m_bPseudo = false;
		
#if GL_ENABLE_UNLOCK_BUFFER_OVERWRITE_DETECTION
	m_bPseudo = true;
#endif

#if GL_ENABLE_INDEX_VERIFICATION
	m_BufferSpanManager.Init( m_pCtx, m_type, 512, m_nSize, m_bDynamic );
		
	if ( m_type == kGLMIndexBuffer )
		m_bPseudo = true;
#endif
	
	if ( g_bUsePseudoBufs && m_bDynamic )
	{
		m_bPseudo = true;
	}
			
	if ( m_bPseudo )
	{
		m_nHandle = 0;		

#if GL_ENABLE_UNLOCK_BUFFER_OVERWRITE_DETECTION
		m_nDirtyRangeStart = 0xFFFFFFFF;
		m_nDirtyRangeEnd = 0;

		m_nActualSize = ALIGN_VALUE( ( m_nSize + sizeof( uint32 ) ), 4096 );
		m_pPseudoBuf = m_pActualPseudoBuf = (char *)VirtualAlloc( NULL, m_nActualSize, MEM_COMMIT, PAGE_READWRITE );
		if ( !m_pPseudoBuf )
		{
			Error( "VirtualAlloc() failed!\n" );
		}

		for ( uint i = 0; i < m_nActualSize / sizeof( uint32 ); i++ )
		{
			reinterpret_cast< uint32 * >( m_pPseudoBuf )[i] = 0xDEADBEEF;
		}

		DWORD nOldProtect;
		BOOL bResult = VirtualProtect( m_pActualPseudoBuf, m_nActualSize, PAGE_READONLY, &nOldProtect );
		if ( !bResult )
		{
			Error( "VirtualProtect() failed!\n" );
		}
#else
		m_nActualSize = size + 15;
		m_pActualPseudoBuf = (char*)malloc( m_nActualSize );
		m_pPseudoBuf = (char*)(((intp)m_pActualPseudoBuf + 15) & ~15);
#endif
		
		m_pCtx->BindBufferToCtx( m_type, NULL );		// exit with no buffer bound
	}
	else
	{
		gGL->glGenBuffersARB( 1, &m_nHandle );

		m_pCtx->BindBufferToCtx( m_type, this );	// causes glBindBufferARB

		// buffers start out static, but if they get orphaned and gl_bufmode is non zero,
		// then they will get flipped to dynamic.
		
		GLenum hint = GL_STATIC_DRAW_ARB;
		switch (m_type)
		{
			case kGLMVertexBuffer:	hint = m_bDynamic ? GL_DYNAMIC_DRAW_ARB : GL_STATIC_DRAW_ARB; break;
			case kGLMIndexBuffer:	hint = m_bDynamic ? GL_DYNAMIC_DRAW_ARB : GL_STATIC_DRAW_ARB; break;
			case kGLMUniformBuffer:	hint = GL_DYNAMIC_DRAW_ARB; break;
			case kGLMPixelBuffer:	hint = m_bDynamic ? GL_DYNAMIC_DRAW_ARB : GL_STATIC_DRAW_ARB; break;
			
			default: Assert(!"Unknown buffer type" ); DXABSTRACT_BREAK_ON_ERROR();
		}

		gGL->glBufferDataARB( m_buffGLTarget, m_nSize, (const GLvoid*)NULL, hint );	// may ultimately need more hints to set the usage correctly (esp for streaming)

		SetModes( false, true, true );

		m_pCtx->BindBufferToCtx( m_type, NULL );	// unbind me
	}
}

CGLMBuffer::~CGLMBuffer( )
{
	m_pCtx->CheckCurrent();
	
	if ( m_bPseudo )
	{
#if GL_ENABLE_UNLOCK_BUFFER_OVERWRITE_DETECTION
		BOOL bResult = VirtualFree( m_pActualPseudoBuf, 0, MEM_RELEASE );
		if ( !bResult )
		{
			Error( "VirtualFree() failed!\n" );
		}
#else
		free( m_pActualPseudoBuf );
#endif
		m_pActualPseudoBuf = NULL;
		m_pPseudoBuf = NULL;
	}
	else
	{
		gGL->glDeleteBuffersARB( 1, &m_nHandle );
	}
	
	m_pCtx = NULL;
	m_nHandle = 0;
		
	m_pLastMappedAddress = NULL;

#if GL_ENABLE_INDEX_VERIFICATION
	m_BufferSpanManager.Deinit();
#endif
}

void CGLMBuffer::SetModes( bool bAsyncMap, bool bExplicitFlush, bool bForce )
{
	// assumes buffer is bound. called by constructor and by Lock.

	if ( m_bPseudo )
	{
		// ignore it...
	}
	else
	{
		if ( bForce || ( m_bEnableAsyncMap != bAsyncMap ) )
		{
			// note the sense of the parameter, it's TRUE if you *want* serialization, so for async you turn it to false.
			if ( ( gGL->m_bHave_GL_APPLE_flush_buffer_range ) && ( !gGL->m_bHave_GL_ARB_map_buffer_range ) )
			{
				gGL->glBufferParameteriAPPLE( m_buffGLTarget, GL_BUFFER_SERIALIZED_MODIFY_APPLE, bAsyncMap == false );
			}
			m_bEnableAsyncMap = bAsyncMap;
		}

		if ( bForce || ( m_bEnableExplicitFlush != bExplicitFlush ) )
		{
			// Note that the GL_ARB_map_buffer_range path handles this in the glMapBufferRange() call in Lock().
			// note the sense of the parameter, it's TRUE if you *want* auto-flush-on-unmap, so for explicit-flush, you turn it to false.
			if ( ( gGL->m_bHave_GL_APPLE_flush_buffer_range ) && ( !gGL->m_bHave_GL_ARB_map_buffer_range ) )
			{
				gGL->glBufferParameteriAPPLE( m_buffGLTarget, GL_BUFFER_FLUSHING_UNMAP_APPLE, bExplicitFlush == false );
			}
			m_bEnableExplicitFlush = bExplicitFlush;
		}
	}
}

#if GL_ENABLE_INDEX_VERIFICATION
bool CGLMBuffer::IsSpanValid( uint nOffset, uint nSize ) const
{
	return m_BufferSpanManager.IsValid( nOffset, nSize );
}
#endif

void CGLMBuffer::FlushRange( uint offset, uint size )
{
	if ( m_pStaticBuffer )
	{
	}
	else if ( m_bPseudo )
	{
		// nothing to do
	}
	else
	{
#ifdef REPORT_LOCK_TIME
		double flStart = Plat_FloatTime();
#endif

		// assumes buffer is bound.
		if ( gGL->m_bHave_GL_ARB_map_buffer_range )
		{
			gGL->glFlushMappedBufferRange( m_buffGLTarget, (GLintptr)( offset - m_dirtyMinOffset ), (GLsizeiptr)size );
		}
		else if ( gGL->m_bHave_GL_APPLE_flush_buffer_range )
		{
			gGL->glFlushMappedBufferRangeAPPLE( m_buffGLTarget, (GLintptr)offset, (GLsizeiptr)size );
		}
		
#ifdef REPORT_LOCK_TIME
		double flEnd = Plat_FloatTime();
		if ( flEnd - flStart > 5.0 / 1000.0 )
		{
			int nDelta = ( int )( ( flEnd - flStart ) * 1000 );
			if ( nDelta > 2 )
			{
				Msg( "**** " );
			}
			Msg( "glFlushMappedBufferRange Time %d: ( Name=%d BufSize=%d ) Target=%p Offset=%d FlushSize=%d\n", nDelta, m_nHandle, m_nSize, m_buffGLTarget, offset - m_dirtyMinOffset, size );
		}
#endif

		// If you don't have any extension support here, you'll flush the whole buffer on unmap. Performance loss, but it's still safe and correct.
	}
}

void CGLMBuffer::Lock( GLMBuffLockParams *pParams, char **pAddressOut )
{
#if GL_TELEMETRY_GPU_ZONES
	CScopedGLMPIXEvent glmPIXEvent( "CGLMBuffer::Lock" );
	g_TelemetryGPUStats.m_nTotalBufferLocksAndUnlocks++;
#endif

	char *resultPtr = NULL;
	
	if ( m_bMapped )
	{
		DXABSTRACT_BREAK_ON_ERROR();
		return;
	}
	
	m_pCtx->CheckCurrent();

	Assert( pParams->m_nSize );
	
	m_LockParams = *pParams;
	
	if ( pParams->m_nOffset >= m_nSize )
	{
		DXABSTRACT_BREAK_ON_ERROR();
		return;
	}
	
	if ( ( pParams->m_nOffset + pParams->m_nSize ) > m_nSize)
	{
		DXABSTRACT_BREAK_ON_ERROR();
		return;
	}

#if GL_ENABLE_INDEX_VERIFICATION
	if ( pParams->m_bDiscard )
	{
		m_BufferSpanManager.DiscardAllSpans();
	}
#endif

	m_pStaticBuffer = NULL;
	bool bUsingPersistentBuffer = false;

	uint padding = 0;
	if ( m_bDynamic && gGL->m_bHave_GL_ARB_buffer_storage )
	{
		// Compute padding to add to make sure the start offset is valid
		CPersistentBuffer *pTempBuffer = m_pCtx->GetCurPersistentBuffer( m_type );
		uint persistentBufferOffset = pTempBuffer->GetOffset();

		if (pParams->m_nOffset > persistentBufferOffset)
		{
			// Make sure the start offset if valid (adding padding to the persistent buffer)
			padding = pParams->m_nOffset - persistentBufferOffset;
		}
	}
	
	if ( m_bPseudo )
	{
		if ( pParams->m_bDiscard )
		{
			m_nRevision++;
		}

		// async map modes are a no-op
				
		// calc lock address
		resultPtr = m_pPseudoBuf + pParams->m_nOffset;

#if GL_ENABLE_UNLOCK_BUFFER_OVERWRITE_DETECTION
		BOOL bResult;
		DWORD nOldProtect;
		if ( pParams->m_bDiscard )
		{
			bResult = VirtualProtect( m_pActualPseudoBuf, m_nSize, PAGE_READWRITE, &nOldProtect );
			if ( !bResult )
			{
				Error( "VirtualProtect() failed!\n" );
			}

			m_nDirtyRangeStart = 0xFFFFFFFF;
			m_nDirtyRangeEnd = 0;

			for ( uint i = 0; i < m_nSize / sizeof( uint32 ); i++ )
			{
				reinterpret_cast< uint32 * >( m_pPseudoBuf )[i] = 0xDEADBEEF;
			}

			bResult = VirtualProtect( m_pActualPseudoBuf, m_nSize, PAGE_READONLY, &nOldProtect );
			if ( !bResult )
			{
				Error( "VirtualProtect() failed!\n" );
			}
		}
		uint nProtectOfs = m_LockParams.m_nOffset & 4095;
		uint nProtectEnd = ( m_LockParams.m_nOffset + m_LockParams.m_nSize + 4095 ) & ~4095;
		uint nProtectSize = nProtectEnd - nProtectOfs;
		bResult = VirtualProtect( m_pActualPseudoBuf + nProtectOfs, nProtectSize, PAGE_READWRITE, &nOldProtect );
		if ( !bResult )
		{
			Error( "VirtualProtect() failed!\n" );
		}
#endif
	}
	else if ( m_bDynamic && gGL->m_bHave_GL_ARB_buffer_storage && ( m_pCtx->GetCurPersistentBuffer( m_type )->GetBytesRemaining() >= ( pParams->m_nSize + padding ) ) )
	{
		CPersistentBuffer *pTempBuffer = m_pCtx->GetCurPersistentBuffer( m_type );

		// Make sure the start offset if valid (adding padding to the persistent buffer)
		pTempBuffer->Append( padding );

		uint persistentBufferOffset = pTempBuffer->GetOffset();
		uint startOffset = persistentBufferOffset - pParams->m_nOffset;

		if ( pParams->m_bDiscard || ( startOffset != m_nPersistentBufferStartOffset ) )
		{
			m_nRevision++;
			// Offset to be added to the vertex and index buffer when setting the vertex and index buffer (before drawing)
			// Since we are using a immutable buffer storage, the persistent buffer is actually bigger than
			// buffer size requested upon creation. We keep appending to the end of the persistent buffer 
			// and therefore need to keep track of the start of the actual buffer (in the persistent one)
			m_nPersistentBufferStartOffset = startOffset;

			//DevMsg( "Discard (%s): startOffset = %d\n", pParams->m_bDiscard ? "true" : "false", m_nPersistentBufferStartOffset );
		}

		resultPtr = static_cast<char*>(pTempBuffer->GetPtr()) + persistentBufferOffset;
		bUsingPersistentBuffer = true;

		//DevMsg( " --> buff=%x, startOffset=%d, paramsOffset=%d, persistOffset = %d\n", this, m_nPersistentBufferStartOffset, pParams->m_nOffset, persistentBufferOffset );
	}
#ifndef OSX
	else if ( m_bDynamic && gGL->m_bHave_GL_AMD_pinned_memory && ( m_pCtx->GetCurPinnedMemoryBuffer()->GetBytesRemaining() >= pParams->m_nSize ) )
	{
		if ( pParams->m_bDiscard )
		{
			m_nRevision++;
		}

		m_dirtyMinOffset = pParams->m_nOffset;
		m_dirtyMaxOffset = pParams->m_nOffset + pParams->m_nSize;

		CPinnedMemoryBuffer *pTempBuffer = m_pCtx->GetCurPinnedMemoryBuffer();

		m_nPinnedMemoryOfs = pTempBuffer->GetOfs();

		resultPtr = static_cast<char*>( pTempBuffer->GetPtr() ) + m_nPinnedMemoryOfs;
		
		pTempBuffer->Append( pParams->m_nSize );
	}
#endif // OSX
	else if ( !g_bDisableStaticBuffer && ( pParams->m_bDiscard || pParams->m_bNoOverwrite ) && ( pParams->m_nSize <= GL_STATIC_BUFFER_SIZE ) )
	{
#if TOGL_SUPPORT_NULL_DEVICE
		if ( !g_bNullD3DDevice )
#endif
		{
			if ( pParams->m_bDiscard )
			{
				m_pCtx->BindBufferToCtx( m_type, this );

				// observe gl_bufmode on any orphan event.
				// if orphaned and bufmode is nonzero, flip it to dynamic.
				GLenum hint = gl_bufmode.GetInt() ? GL_DYNAMIC_DRAW_ARB : GL_STATIC_DRAW_ARB;
				gGL->glBufferDataARB( m_buffGLTarget, m_nSize, (const GLvoid*)NULL, hint );
			
				m_nRevision++; // revision grows on orphan event
			}
		}

		m_dirtyMinOffset = pParams->m_nOffset;
		m_dirtyMaxOffset = pParams->m_nOffset + pParams->m_nSize;

		switch ( m_type )
		{
			case kGLMVertexBuffer:
			{
				m_pStaticBuffer = m_StaticBuffers[ 0 ];
				break;
			}
			case kGLMIndexBuffer:
			{
				m_pStaticBuffer = m_StaticBuffers[ 1 ];
				break;
			}
			default:
			{
				DXABSTRACT_BREAK_ON_ERROR();
				return;
			}
		}

		resultPtr = m_pStaticBuffer;
	}
	else
	{
		// bind (yes, even for pseudo - this binds name 0)
		m_pCtx->BindBufferToCtx( m_type, this );

		// perform discard if requested
		if ( pParams->m_bDiscard )
		{
			// observe gl_bufmode on any orphan event.
			// if orphaned and bufmode is nonzero, flip it to dynamic.
			
			// We always want to call glBufferData( ..., NULL ) on discards, even though we're using the GL_MAP_INVALIDATE_BUFFER_BIT flag, because this flag is actually only a hint according to AMD.
			GLenum hint = gl_bufmode.GetInt() ? GL_DYNAMIC_DRAW_ARB : GL_STATIC_DRAW_ARB;
			gGL->glBufferDataARB( m_buffGLTarget, m_nSize, (const GLvoid*)NULL, hint );
									
			m_nRevision++;	// revision grows on orphan event
		}

		// adjust async map option appropriately, leave explicit flush unchanged
		SetModes( pParams->m_bNoOverwrite, m_bEnableExplicitFlush );

		// map
		char *mapPtr;
		if ( gGL->m_bHave_GL_ARB_map_buffer_range )
		{
			// m_bEnableAsyncMap is actually pParams->m_bNoOverwrite
			GLbitfield parms = GL_MAP_WRITE_BIT | ( m_bEnableAsyncMap ? GL_MAP_UNSYNCHRONIZED_BIT : 0 ) | ( pParams->m_bDiscard ? GL_MAP_INVALIDATE_BUFFER_BIT : 0 ) | ( m_bEnableExplicitFlush ? GL_MAP_FLUSH_EXPLICIT_BIT : 0 );

#ifdef REPORT_LOCK_TIME
			double flStart = Plat_FloatTime();
#endif

			mapPtr = (char*)gGL->glMapBufferRange( m_buffGLTarget, pParams->m_nOffset, pParams->m_nSize, parms);

#ifdef REPORT_LOCK_TIME
			double flEnd = Plat_FloatTime();
			if ( flEnd - flStart > 5.0 / 1000.0 )
			{
				int nDelta = ( int )( ( flEnd - flStart ) * 1000 );
				if ( nDelta > 2 )
				{
					Msg( "**** " );
				}
				Msg( "glMapBufferRange Time=%d: ( Name=%d BufSize=%d ) Target=%p Offset=%d LockSize=%d ", nDelta, m_nHandle, m_nSize, m_buffGLTarget, pParams->m_nOffset, pParams->m_nSize );
				if ( parms & GL_MAP_WRITE_BIT )
				{
					Msg( "GL_MAP_WRITE_BIT ");
				}
				if ( parms & GL_MAP_UNSYNCHRONIZED_BIT )
				{
					Msg( "GL_MAP_UNSYNCHRONIZED_BIT ");
				}
				if ( parms & GL_MAP_INVALIDATE_BUFFER_BIT )
				{
					Msg( "GL_MAP_INVALIDATE_BUFFER_BIT ");
				}
				if ( parms & GL_MAP_INVALIDATE_RANGE_BIT )
				{
					Msg( "GL_MAP_INVALIDATE_RANGE_BIT ");
				}
				if ( parms & GL_MAP_FLUSH_EXPLICIT_BIT )
				{
					Msg( "GL_MAP_FLUSH_EXPLICIT_BIT ");
				}
				Msg( "\n" );
			}
#endif
		}
		else
		{
			mapPtr = (char*)gGL->glMapBufferARB( m_buffGLTarget, GL_WRITE_ONLY_ARB );
		}

		Assert( mapPtr );
				
		// calculate offset location
		resultPtr = mapPtr;
		if ( !gGL->m_bHave_GL_ARB_map_buffer_range )
		{
			resultPtr += pParams->m_nOffset;
		}

		// set range
		m_dirtyMinOffset = pParams->m_nOffset;
		m_dirtyMaxOffset = pParams->m_nOffset + pParams->m_nSize;
	}

	if ( m_bUsingPersistentBuffer != bUsingPersistentBuffer )
	{
		// Up the revision number when switching from a persistent to a non persistent buffer (or vice versa)
		// Ensure the right GL buffer is bound before drawing (and vertex attribs properly set)
		m_nRevision++;
		m_bUsingPersistentBuffer = bUsingPersistentBuffer;
	}

	m_bMapped = true;

	m_pLastMappedAddress = (float*)resultPtr;
	
	*pAddressOut = resultPtr;
}

void CGLMBuffer::Unlock( int nActualSize, const void *pActualData )
{
#if GL_TELEMETRY_GPU_ZONES
	CScopedGLMPIXEvent glmPIXEvent( "CGLMBuffer::Unlock" );
	g_TelemetryGPUStats.m_nTotalBufferLocksAndUnlocks++;
#endif

	m_pCtx->CheckCurrent();
	
	if ( !m_bMapped )
	{
		DXABSTRACT_BREAK_ON_ERROR();
		return;
	}

	if ( nActualSize < 0 )
	{
		nActualSize = m_LockParams.m_nSize;
	}

	if ( nActualSize > (int)m_LockParams.m_nSize )
	{
		DXABSTRACT_BREAK_ON_ERROR();
		return;
	}

#if GL_ENABLE_UNLOCK_BUFFER_OVERWRITE_DETECTION
	if ( m_bPseudo )
	{
		// Check guard DWORD to detect buffer overruns (but are still within the last 4KB page so they don't get caught via pagefaults)
		if ( *reinterpret_cast< const uint32 * >( m_pPseudoBuf + m_nSize ) != 0xDEADBEEF )
		{
			// If this fires the client app has overwritten the guard DWORD beyond the end of the buffer.
			DXABSTRACT_BREAK_ON_ERROR();
		}

		static const uint s_nInitialValues[4] = { 0xEF, 0xBE, 0xAD, 0xDE };

		int nActualModifiedStart, nActualModifiedEnd;
		for ( nActualModifiedStart = 0; nActualModifiedStart < (int)m_LockParams.m_nSize; ++nActualModifiedStart )
			if ( reinterpret_cast< const uint8 * >( m_pLastMappedAddress )[nActualModifiedStart] != s_nInitialValues[ ( m_LockParams.m_nOffset + nActualModifiedStart ) & 3 ] )
				break;

		for ( nActualModifiedEnd = m_LockParams.m_nSize - 1; nActualModifiedEnd > nActualModifiedStart; --nActualModifiedEnd )
			if ( reinterpret_cast< const uint8 * >( m_pLastMappedAddress )[nActualModifiedEnd] != s_nInitialValues[ ( m_LockParams.m_nOffset + nActualModifiedEnd ) & 3 ] )
				break;

		int nNumActualBytesModified = 0;

		if ( nActualModifiedEnd >= nActualModifiedStart )
		{
			// The modified check is conservative (i.e. it should always err on the side of detecting <= actual bytes than where actually modified, never more).
			// We primarily care about the case where the user lies about the actual # of modified bytes, which can lead to difficult to debug/inconsistent problems with some drivers.
			// Round up/down the modified range, because the user's data may alias with the initial buffer values (0xDEADBEEF) so we may miss some bytes that where written.
			if ( m_type == kGLMIndexBuffer )
			{
				nActualModifiedStart &= ~1;
				nActualModifiedEnd = MIN( (int)m_LockParams.m_nSize, ( ( nActualModifiedEnd + 1 ) + 1 ) & ~1 ) - 1;
			}
			else
			{
				nActualModifiedStart &= ~3;
				nActualModifiedEnd = MIN( (int)m_LockParams.m_nSize, ( ( nActualModifiedEnd + 1 ) + 3 ) & ~3 ) - 1;
			}
		
			nNumActualBytesModified = nActualModifiedEnd + 1;

			if ( nActualSize < nNumActualBytesModified )
			{
				// The caller may be lying about the # of actually modified bytes in this lock.
				// Has this lock region been previously locked? If so, it may have been previously overwritten before. Otherwise, the region had to be the 0xDEADBEEF fill DWORD at lock time.
				if ( ( m_nDirtyRangeStart > m_nDirtyRangeEnd ) ||
				     ( m_LockParams.m_nOffset > m_nDirtyRangeEnd ) || ( ( m_LockParams.m_nOffset + m_LockParams.m_nSize ) <= m_nDirtyRangeStart )  )
				{
					// If this fires the client has lied about the actual # of bytes they've modified in the buffer - this will cause unreliable rendering on AMD drivers (because AMD actually pays attention to the actual # of flushed bytes).
					DXABSTRACT_BREAK_ON_ERROR();
				}
			}
		
			m_nDirtyRangeStart = MIN( m_nDirtyRangeStart, m_LockParams.m_nOffset + nActualModifiedStart );
			m_nDirtyRangeEnd = MAX( m_nDirtyRangeEnd, m_LockParams.m_nOffset + nActualModifiedEnd );
		}

#if GL_ENABLE_INDEX_VERIFICATION
		if ( nActualModifiedEnd >= nActualModifiedStart )
		{
			int n = nActualModifiedEnd + 1;
			if ( n != nActualSize )
			{
				// The actual detected modified size is < than the reported size, which is common because the last few DWORD's of the vertex format may not actually be used/written (or read by the vertex shader). So just fudge it so the batch consumption checks work.
				if ( ( (int)nActualSize - n ) <= 32 )
				{
					n = nActualSize;
				}
			}

			m_BufferSpanManager.AddSpan( m_LockParams.m_nOffset + nActualModifiedStart, m_LockParams.m_nSize, n - nActualModifiedStart, m_LockParams.m_bDiscard, m_LockParams.m_bNoOverwrite );
		}
#endif		
	}
#elif GL_ENABLE_INDEX_VERIFICATION
	if ( nActualSize > 0 )
	{
		m_BufferSpanManager.AddSpan( m_LockParams.m_nOffset, m_LockParams.m_nSize, nActualSize, m_LockParams.m_bDiscard, m_LockParams.m_bNoOverwrite );
	}
#endif

#if GL_BATCH_PERF_ANALYSIS
	if ( m_type == kGLMIndexBuffer )
		g_nTotalIBLockBytes += nActualSize;
	else if ( m_type == kGLMVertexBuffer )
		g_nTotalVBLockBytes += nActualSize;
#endif

#ifndef OSX
	if ( m_nPinnedMemoryOfs >= 0 )
	{
#if TOGL_SUPPORT_NULL_DEVICE
		if ( !g_bNullD3DDevice )
		{
#endif
		if ( nActualSize )
		{
			m_pCtx->BindBufferToCtx( m_type, this );

			gGL->glCopyBufferSubData( 
				GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD,
				m_buffGLTarget,
				m_nPinnedMemoryOfs,
				m_dirtyMinOffset,
				nActualSize );
		}

#if TOGL_SUPPORT_NULL_DEVICE
		}
#endif
		
		m_nPinnedMemoryOfs = -1;
	}
	else
#endif // !OSX
	if ( m_bUsingPersistentBuffer )
	{
		if ( nActualSize )
		{
			CPersistentBuffer *pTempBuffer = m_pCtx->GetCurPersistentBuffer( m_type );
			pTempBuffer->Append( nActualSize );

			//DevMsg( "   <-- actualSize=%d, persistOffset = %d\n", nActualSize, pTempBuffer->GetOffset() );
		}
	}
	else if ( m_pStaticBuffer )
	{
#if TOGL_SUPPORT_NULL_DEVICE
		if ( !g_bNullD3DDevice )
#endif
		{
			if ( nActualSize )
			{
				tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "UnlockSubData" );

	#ifdef REPORT_LOCK_TIME
				double flStart = Plat_FloatTime();
	#endif
				m_pCtx->BindBufferToCtx( m_type, this );
				
				Assert( nActualSize <= (int)( m_dirtyMaxOffset - m_dirtyMinOffset ) );

				glBufferSubDataMaxSize( m_buffGLTarget, m_dirtyMinOffset, nActualSize, pActualData ? pActualData : m_pStaticBuffer );
						
		#ifdef REPORT_LOCK_TIME
				double flEnd = Plat_FloatTime();
				if ( flEnd - flStart > 5.0 / 1000.0 )
				{
					int nDelta = ( int )( ( flEnd - flStart ) * 1000 );
					if ( nDelta > 2 )
					{
						Msg( "**** " );
					}
					// Msg( "glBufferSubData Time=%d: ( Name=%d BufSize=%d ) Target=%p Offset=%d Size=%d\n", nDelta, m_nHandle, m_nSize, m_buffGLTarget, m_dirtyMinOffset, m_dirtyMaxOffset - m_dirtyMinOffset );
				}
	#endif		
			}
		}

		m_pStaticBuffer = NULL;
	}
	else if ( m_bPseudo )
	{
		if ( pActualData )
		{
			memcpy( m_pLastMappedAddress, pActualData, nActualSize );
		}

#if GL_ENABLE_UNLOCK_BUFFER_OVERWRITE_DETECTION
		uint nProtectOfs = m_LockParams.m_nOffset & 4095;
		uint nProtectEnd = ( m_LockParams.m_nOffset + m_LockParams.m_nSize + 4095 ) & ~4095;
		uint nProtectSize = nProtectEnd - nProtectOfs;

		DWORD nOldProtect;
		BOOL bResult = VirtualProtect( m_pActualPseudoBuf + nProtectOfs, nProtectSize, PAGE_READONLY, &nOldProtect );
		if ( !bResult )
		{
			Error( "VirtualProtect() failed!\n" );
		}
#endif
	}
	else
	{
		tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "UnlockUnmap" );

		if ( pActualData )
		{
			memcpy( m_pLastMappedAddress, pActualData, nActualSize );
		}

		m_pCtx->BindBufferToCtx( m_type, this );

		Assert( nActualSize <= (int)( m_dirtyMaxOffset - m_dirtyMinOffset ) );

		// time to do explicit flush (currently m_bEnableExplicitFlush is always true)
		if ( m_bEnableExplicitFlush )
		{
			FlushRange( m_dirtyMinOffset, nActualSize );
		}
		
		// clear dirty range no matter what
		m_dirtyMinOffset = m_dirtyMaxOffset = 0;								// adjust/grow on lock, clear on unlock

#ifdef REPORT_LOCK_TIME
		double flStart = Plat_FloatTime();
#endif

		gGL->glUnmapBuffer( m_buffGLTarget );

#ifdef REPORT_LOCK_TIME
		double flEnd = Plat_FloatTime();
		if ( flEnd - flStart > 5.0 / 1000.0 )
		{
			int nDelta = ( int )( ( flEnd - flStart ) * 1000 );
			if ( nDelta > 2 )
			{
				Msg( "**** " );
			}
			Msg( "glUnmapBuffer Time=%d: ( Name=%d BufSize=%d ) Target=%p\n", nDelta, m_nHandle, m_nSize, m_buffGLTarget );
		}
#endif		
	}

	m_bMapped = false;
}

GLuint CGLMBuffer::GetHandle() const
{ 
	return ( m_bUsingPersistentBuffer ? m_pCtx->GetCurPersistentBuffer( m_type )->GetHandle() : m_nHandle ); 
}
